// coop/mod_compat/mod_compat.cpp -- see coop/mod_compat/mod_compat.h.

#include "coop/mod_compat/mod_compat.h"

#include "coop/mod_compat/smarttv.h"

#include "ue_wrap/core/log.h"
#include "ue_wrap/core/object_index.h"

namespace coop::mod_compat {
namespace {

struct Adapter {
    const char* mod;
    const wchar_t* markerClass;  // a class the mod's pak loads
    const char* what;            // what the adapter carries
    void (*Install)(coop::net::Session&);
    void (*OnDisconnect)();
};
const Adapter kAdapters[] = {
    { "SmartTV_Beta", L"stv_lib_C", "its URL bar opens the link on the TV for every peer",
      &coop::mod_compat::smarttv::Install, &coop::mod_compat::smarttv::OnDisconnect },
};
constexpr int kCount = static_cast<int>(sizeof(kAdapters) / sizeof(kAdapters[0]));
bool g_named[kCount] = {};

}  // namespace

void Install(coop::net::Session& session) {
    for (const Adapter& a : kAdapters) a.Install(session);
}

void Tick() {
    for (int i = 0; i < kCount; ++i) {
        if (g_named[i] || !ue_wrap::object_index::ClassByName(kAdapters[i].markerClass)) continue;
        g_named[i] = true;
        UE_LOGI("[MOD] %s is loaded: %s", kAdapters[i].mod, kAdapters[i].what);
    }
}

void OnDisconnect() {
    for (const Adapter& a : kAdapters) a.OnDisconnect();
}

}  // namespace coop::mod_compat
