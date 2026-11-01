// coop/mod_compat/smarttv.cpp -- see coop/mod_compat/smarttv.h.

#include "coop/mod_compat/smarttv.h"

#include "coop/interactables/tv_sync.h"

#include "ue_wrap/core/field_io.h"
#include "ue_wrap/core/log.h"
#include "ue_wrap/core/reflection.h"
#include "ue_wrap/core/reflection_props.h"
#include "ue_wrap/core/script_gate.h"
#include "ue_wrap/devices/tv.h"
#include "ue_wrap/facts/function_row.h"

#include <cstdint>
#include <string>

namespace coop::mod_compat::smarttv {
namespace {

namespace sg = ue_wrap::script_gate;
namespace R = ue_wrap::reflection;

UE_FACT_FN(kRowSubmit, "mod_compat.smarttv", L"stv_lib_C", L"STV_Submit", Declared);
constexpr int kTagSubmit = 0x53545653;  // 'STVS'

bool g_watched = false;
uint64_t g_submits = 0;

R::ParamOffset g_panel{L"Panel"};
R::ParamOffset g_url{L"Url"};
R::InstanceOffset g_iface{L"iface"};  // the panel's TV menu

void* PanelTv(void* panel) {
    const int32_t o = R::IsLive(panel) ? g_iface.Of(panel) : -1;
    if (o < 0) return nullptr;
    void* iface = *reinterpret_cast<void* const*>(static_cast<const uint8_t*>(panel) + o);
    return R::IsLive(iface) ? ue_wrap::tv::TvOfInterface(iface) : nullptr;
}

void OnSubmitPost(const sg::Call& c) {
    const int32_t panelOff = g_panel.Of(c.function);
    const int32_t urlOff = g_url.Of(c.function);
    if (panelOff < 0 || urlOff < 0 || !c.locals) return;
    void* panel = *reinterpret_cast<void* const*>(c.locals + panelOff);
    ue_wrap::tv::Link link;
    link.link = ue_wrap::field_io::ReadFStringAt(c.locals, urlOff);
    if (link.link.empty()) return;
    void* tv = PanelTv(panel);
    if (!tv) {
        static bool said = false;
        if (!said) {
            said = true;
            UE_LOGW("[MOD] SmartTV: a URL bar with no TV behind it (its `iface` did not resolve); "
                    "the link plays on this peer only (said once)");
        }
        return;
    }
    link.name = link.link;
    link.isUrl = true;
    ++g_submits;
    coop::tv_sync::SendLink(tv, link, false);
}

}  // namespace

void Install(coop::net::Session& /*session*/) {
    if (!g_watched) g_watched = sg::WatchName(kRowSubmit, kTagSubmit, nullptr, &OnSubmitPost);
}

void OnDisconnect() {
    if (g_submits) UE_LOGI("[MOD] SmartTV: session end -- %llu URL bar link(s) sent",
                           static_cast<unsigned long long>(g_submits));
    g_submits = 0;
}

}  // namespace coop::mod_compat::smarttv
