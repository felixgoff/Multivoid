// ue_wrap/devices/tv.cpp -- see ue_wrap/devices/tv.h.

#include "ue_wrap/devices/tv.h"

#include "ue_wrap/actors/prop.h"
#include "ue_wrap/core/cached_obj_ref.h"
#include "ue_wrap/core/call.h"
#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/object_index.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"

#include <cstring>

namespace ue_wrap::tv {
namespace {

namespace R = reflection;

ue_wrap::CachedObjRef g_tvClass;
R::InstanceOffset g_channel{L"channel"};
R::InstanceOffset g_gamemode{L"gamemode"};
R::InstanceOffset g_radio{L"radio"};

void* TvClass() {
    if (void* k = g_tvClass.Get()) return k;
    void* k = ue_wrap::object_index::ClassByName(L"prop_tv2_C");
    if (k) g_tvClass.Set(k);
    return k;
}

void* ReadObject(void* obj, R::InstanceOffset& off) {
    const int32_t o = obj ? off.Of(obj) : -1;
    if (o < 0) return nullptr;
    void* p = *reinterpret_cast<void* const*>(reinterpret_cast<const uint8_t*>(obj) + o);
    return R::IsLive(p) ? p : nullptr;
}

// struct_tv's members, by their stable prefixes; the struct is the one the function's `data` names.
struct LinkLayout {
    void* fn = nullptr;
    int32_t data = -1, name = -1, link = -1, isUrl = -1;
};
LinkLayout g_layout[2];  // openLink's, and the menu's select's

const LinkLayout* LayoutOf(void* function) {
    LinkLayout* slot = nullptr;
    for (auto& l : g_layout) {
        if (l.fn == function) return l.data >= 0 ? &l : nullptr;
        if (!slot && !l.fn) slot = &l;
    }
    if (!slot) slot = &g_layout[0];
    *slot = LinkLayout{};
    slot->fn = function;
    void* st = R::PropertyInnerStruct(function, L"data");
    if (!st) return nullptr;
    const int32_t name = R::FindPropertyOffsetByPrefix(st, L"name_");
    const int32_t link = R::FindPropertyOffsetByPrefix(st, L"link_");
    const int32_t isUrl = R::FindPropertyOffsetByPrefix(st, L"isUrl_");
    const int32_t data = R::FindParamOffset(function, L"data");
    if (name < 0 || link < 0 || isUrl < 0 || data < 0) return nullptr;
    slot->name = name;
    slot->link = link;
    slot->isUrl = isUrl;
    slot->data = data;
    return slot;
}

void* FunctionOf(void* obj, const wchar_t* name) {
    return obj ? R::FindDispatchFunctionCached(R::ClassOf(obj), name) : nullptr;
}

}  // namespace

bool IsTv(void* actor) {
    void* base = TvClass();
    return actor && base && ue_wrap::prop::WalksToBase(R::ClassOf(actor), base);
}

void* TvOfInterface(void* ui) {
    void* tv = ReadObject(ui, g_radio);
    return IsTv(tv) ? tv : nullptr;
}

bool ReadChannel(void* tv, int32_t& out) {
    const int32_t o = IsTv(tv) ? g_channel.Of(tv) : -1;
    if (o < 0) return false;
    std::memcpy(&out, reinterpret_cast<const uint8_t*>(tv) + o, sizeof(out));
    return true;
}

bool ReadLinkParam(void* function, const uint8_t* locals, Link& out) {
    const LinkLayout* l = function && locals ? LayoutOf(function) : nullptr;
    if (!l) return false;
    const uint8_t* d = locals + l->data;
    out.name = field_io::ReadFStringAt(d, l->name);
    out.link = field_io::ReadFStringAt(d, l->link);
    out.isUrl = d[l->isUrl] != 0;
    return true;
}

bool SetChannel(void* tv, int32_t channel) {
    int32_t old = -1;
    void* use = FunctionOf(tv, L"useChannel");
    if (!use || !ReadChannel(tv, old)) return false;
    if (old >= 0) {
        void* gm = ReadObject(tv, g_gamemode);
        void* remove = FunctionOf(gm, L"removeMediaChannel");
        if (!remove) return false;
        ParamFrame f(remove);
        if (!f.valid() || !f.Set(L"Index", old) || !Call(gm, f)) return false;
    }
    ParamFrame f(use);
    return f.valid() && f.Set(L"channel", channel) && Call(tv, f);
}

bool OpenLink(void* tv, const Link& link, bool pausePlayer) {
    void* fn = FunctionOf(tv, L"openLink");
    const LinkLayout* l = fn ? LayoutOf(fn) : nullptr;
    if (!l) return false;
    ParamFrame f(fn);
    if (!f.valid() || !f.Set(L"pausePlayer", pausePlayer)) return false;
    // The strings point at our buffers for the call: the callee copies its parameters.
    auto put = [](uint8_t* at, const std::wstring& s) {
        R::FString h{};
        if (!s.empty()) {
            h.Data = const_cast<wchar_t*>(s.c_str());
            h.Num = h.Max = static_cast<int32_t>(s.size() + 1);
        }
        std::memcpy(at, &h, sizeof(h));
    };
    uint8_t* d = static_cast<uint8_t*>(f.data()) + l->data;
    put(d + l->name, link.name);
    put(d + l->link, link.link);
    d[l->isUrl] = link.isUrl ? 1 : 0;
    return Call(tv, f);
}

}  // namespace ue_wrap::tv
