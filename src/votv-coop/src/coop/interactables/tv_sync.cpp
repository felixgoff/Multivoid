// coop/interactables/tv_sync.cpp -- see coop/interactables/tv_sync.h.

#include "coop/interactables/tv_sync.h"

#include "coop/net/session.h"
#include "coop/net/wire_key_util.h"
#include "coop/props/prop_key_index.h"
#include "coop/session/net_pump.h"  // IsInAnnouncedWorld: a client's own world load stays local
#include "coop/text/utf8_codec.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/facts/function_row.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace coop::tv_sync {
namespace {

namespace sg = ue_wrap::script_gate;
namespace T = ue_wrap::tv;
using Clock = std::chrono::steady_clock;

UE_FACT_FN(kRowSetChannel, "tv_sync", L"ui_tvInterface_C", L"setChannel", Declared);
// Every route that opens a link on a TV ends in its openLink: the menu's select, and a mod's own control (SmartTV's
// library and Jellyfin rows). Any class, since a TV subclass may declare its own.
UE_FACT_FN(kRowOpenLink, "tv_sync", nullptr, L"openLink", AnyClass);

constexpr int kTagChannel = 0x54564348;  // 'TVCH'
constexpr int kTagOpenLink = 0x54564F4C; // 'TVOL'
// A received state whose TV has not streamed in is retried this long.
constexpr auto kParkFor = std::chrono::seconds(10);

std::atomic<coop::net::Session*> g_session{nullptr};
bool g_channelWatched = false;
bool g_openLinkWatched = false;
bool g_settled = false;

// HOST: each TV's last state this session, the seed a joiner is sent.
struct Last {
    int32_t channel = -1;
    bool hasChannel = false;
    coop::net::TvStatePayload link{};
    uint64_t linkSeq = 0;  // 0: no link
};
std::unordered_map<std::wstring, Last> g_last;
uint64_t g_linkSeq = 0;

struct Parked {
    coop::net::TvStatePayload p;
    Clock::time_point until;
};
std::vector<Parked> g_parked;

uint64_t g_sent = 0, g_applied = 0, g_failed = 0;

// One press can reach two seams (a mod's panel that opens its link through the TV's openLink), so a link
// identical to the one this peer sent for the same TV a moment ago is that press again.
constexpr auto kSamePress = std::chrono::milliseconds(500);
coop::net::TvStatePayload g_lastLink{};
Clock::time_point g_lastLinkAt{};

coop::net::Session* Live() {
    auto* s = g_session.load(std::memory_order_acquire);
    return s && s->running() && s->connected() ? s : nullptr;
}

void Remember(const coop::net::TvStatePayload& p) {
    Last& l = g_last[coop::net::StringFromWireKey(p.tv)];
    if (p.op == 0) {
        l.channel = p.channel;
        l.hasChannel = true;
    } else {
        l.link = p;
        l.linkSeq = ++g_linkSeq;
    }
}

void Send(coop::net::Session* s, const coop::net::TvStatePayload& p) {
    const bool host = s->role() == coop::net::Role::Host;
    if (host) {
        Remember(p);
        s->SendReliable(coop::net::ReliableKind::TvState, &p, sizeof(p));
    } else {
        s->SendReliableToSlot(0, coop::net::ReliableKind::TvState, &p, sizeof(p));
    }
    ++g_sent;
    if (p.op == 0)
        UE_LOGI("[TV] sent '%ls' channel %d", coop::net::StringFromWireKey(p.tv).c_str(), p.channel);
    else
        UE_LOGI("[TV] sent '%ls' link (%u bytes, isUrl=%u)", coop::net::StringFromWireKey(p.tv).c_str(),
                static_cast<unsigned>(p.linkLen), static_cast<unsigned>(p.isUrl));
}

// The TvState for `tv`, its Key set; false for a TV with no Key or a peer that must not send.
bool Begin(coop::net::Session* s, void* tv, coop::net::TvStatePayload& p) {
    if (!tv) return false;
    // A client's load of a world is the host's save; its TVs' own calls there are not its player's.
    if (s->role() != coop::net::Role::Host && !coop::net_pump::IsInAnnouncedWorld(tv)) return false;
    const std::wstring key = ue_wrap::prop::GetKeyString(tv);
    if (key.empty() || key == L"None" || key.size() > sizeof(p.tv.data)) {
        static bool said = false;
        if (!said) {
            said = true;
            UE_LOGW("[TV] a TV with no usable Key ('%ls'); its channel and links stay on this peer (said once)",
                    key.c_str());
        }
        return false;
    }
    std::memset(&p, 0, sizeof(p));
    coop::net::WireKeyFromString(key, p.tv);
    return true;
}

void OnSetChannelPost(const sg::Call& c) {
    auto* s = Live();
    coop::net::TvStatePayload p{};
    void* tv = s ? T::TvOfInterface(c.object) : nullptr;
    if (!Begin(s, tv, p) || !T::ReadChannel(tv, p.channel)) return;
    Send(s, p);
}

// Before the body, which a mod may skip: our own apply is not sent again, and a TV's call from its own graph (its
// playlist's next item at fin, its BeginPlay) runs on every peer's copy alike.
sg::Verdict OnOpenLinkPre(const sg::Call& c) {
    if (c.fromOurCode || !c.object || c.callerObject == c.object || !T::IsTv(c.object) || !Live())
        return sg::Verdict::Run;
    T::Link link;
    if (!T::ReadLinkParam(c.function, c.locals, link)) return sg::Verdict::Run;
    static ue_wrap::reflection::ParamOffset pause{L"pausePlayer"};
    const int32_t pauseOff = pause.Of(c.function);
    SendLink(c.object, link, pauseOff >= 0 && c.locals[pauseOff] != 0);
    return sg::Verdict::Run;
}

// Runs the state on this peer's copy; false only while its TV has not streamed in (a verb that fails is counted).
bool Apply(const coop::net::TvStatePayload& p) {
    void* tv = coop::prop_key_index::Find(coop::net::StringFromWireKey(p.tv));
    if (!tv || !T::IsTv(tv)) return false;
    bool ok;
    if (p.op == 0) {
        int32_t now = -1;
        ok = (T::ReadChannel(tv, now) && now == p.channel) || T::SetChannel(tv, p.channel);
    } else {
        T::Link link;
        link.name = coop::text::FromUtf8Lossy(p.name, p.nameLen);
        link.link = coop::text::FromUtf8Lossy(p.link, p.linkLen);
        link.isUrl = p.isUrl != 0;
        ok = T::OpenLink(tv, link, p.pausePlayer != 0);
    }
    if (ok) {
        ++g_applied;
        UE_LOGI("[TV] applied '%ls' %s", coop::net::StringFromWireKey(p.tv).c_str(), p.op == 0 ? "channel" : "link");
    } else if (++g_failed == 1) {
        UE_LOGW("[TV] a TV's own %s did not resolve on '%ls'; the state stays unapplied (said once)",
                p.op == 0 ? "setChannel" : "openLink", coop::net::StringFromWireKey(p.tv).c_str());
    }
    return true;
}

}  // namespace

void Install(coop::net::Session* session) {
    g_session.store(session, std::memory_order_release);
    if (!g_channelWatched) g_channelWatched = sg::WatchName(kRowSetChannel, kTagChannel, nullptr, &OnSetChannelPost);
    if (!g_openLinkWatched) g_openLinkWatched = sg::WatchName(kRowOpenLink, kTagOpenLink, &OnOpenLinkPre, nullptr);
}

void Tick() {
    if (!g_settled && sg::NameWatchSettled(kRowSetChannel, kTagChannel) &&
        sg::NameWatchSettled(kRowOpenLink, kTagOpenLink)) {
        g_settled = true;
        const bool live = sg::NameWatchLive(kRowSetChannel, kTagChannel) && sg::NameWatchLive(kRowOpenLink, kTagOpenLink);
        if (live) UE_LOGI("[TV] the TV menu's setChannel and the TVs' openLink watches are live");
        else UE_LOGE("[TV] a TV menu watch is dead: a TV's channel or link stays on the peer that picked it");
    }
    if (g_parked.empty()) return;
    const auto now = Clock::now();
    g_parked.erase(std::remove_if(g_parked.begin(), g_parked.end(), [&](const Parked& k) {
        if (Apply(k.p)) return true;
        if (now < k.until) return false;
        UE_LOGW("[TV] no TV '%ls' streamed in within %llds; its state is dropped",
                coop::net::StringFromWireKey(k.p.tv).c_str(), static_cast<long long>(kParkFor.count()));
        return true;
    }), g_parked.end());
}

void SendLink(void* tv, const T::Link& link, bool pausePlayer) {
    auto* s = Live();
    coop::net::TvStatePayload p{};
    if (!s || !Begin(s, tv, p)) return;
    const std::string url = coop::text::ToUtf8Exact(link.link);
    if (url.empty() || url.size() > sizeof(p.link)) {
        static bool said = false;
        if (!said && !url.empty()) {
            said = true;
            UE_LOGW("[TV] a link of %zu bytes is longer than the wire's %zu; it plays on this peer only (said once)",
                    url.size(), sizeof(p.link));
        }
        return;
    }
    const std::string name = coop::text::CapUtf8Bytes(coop::text::ToUtf8Exact(link.name), sizeof(p.name));
    p.op = 1;
    p.isUrl = link.isUrl ? 1 : 0;
    p.pausePlayer = pausePlayer ? 1 : 0;
    p.linkLen = static_cast<uint8_t>(url.size());
    std::memcpy(p.link, url.data(), url.size());
    p.nameLen = static_cast<uint8_t>(name.size());
    std::memcpy(p.name, name.data(), name.size());
    const auto now = Clock::now();
    if (now - g_lastLinkAt < kSamePress && std::memcmp(&g_lastLink, &p, sizeof(p)) == 0) return;
    g_lastLink = p;
    g_lastLinkAt = now;
    Send(s, p);
}

void OnReliable(const coop::net::TvStatePayload& p, uint8_t /*senderSlot*/) {
    auto* s = g_session.load(std::memory_order_acquire);
    if (s && s->role() == coop::net::Role::Host) Remember(p);
    if (!Apply(p)) g_parked.push_back({p, Clock::now() + kParkFor});
}

void ConnectReplayForSlot(int slot) {
    auto* s = Live();
    if (!s || s->role() != coop::net::Role::Host) return;
    std::vector<const Last*> links;
    for (const auto& [key, l] : g_last) {
        if (l.hasChannel) {
            coop::net::TvStatePayload p{};
            coop::net::WireKeyFromString(key, p.tv);
            p.channel = l.channel;
            s->SendReliableToSlot(slot, coop::net::ReliableKind::TvState, &p, sizeof(p));
        }
        if (l.linkSeq) links.push_back(&l);
    }
    // TVs on one channel share its player, so the newest link must open last.
    std::sort(links.begin(), links.end(), [](const Last* a, const Last* b) { return a->linkSeq < b->linkSeq; });
    for (const Last* l : links)
        s->SendReliableToSlot(slot, coop::net::ReliableKind::TvState, &l->link, sizeof(l->link));
    if (!g_last.empty())
        UE_LOGI("[TV] seeded slot %d with %zu TV(s), %zu link(s)", slot, g_last.size(), links.size());
}

void OnDisconnect() {
    if (g_sent || g_applied || g_failed)
        UE_LOGI("[TV] session end -- %llu state(s) sent, %llu applied, %llu failed, %zu parked",
                static_cast<unsigned long long>(g_sent), static_cast<unsigned long long>(g_applied),
                static_cast<unsigned long long>(g_failed), g_parked.size());
    g_sent = g_applied = g_failed = 0;
    g_last.clear();
    g_linkSeq = 0;
    g_parked.clear();
}

}  // namespace coop::tv_sync
