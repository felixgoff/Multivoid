// coop/mod_compat/smarttv.h -- Moddy's SmartTV_Beta on the vanilla TVs. The mod hooks the TV's own
// useChannel and openLink, so a channel picked or a link opened from the TV's menu already reaches it on
// every peer through coop/interactables/tv_sync. Its own URL bar (ui_stv_panel's Open, the stub
// stv_lib_C.STV_Submit(Panel, Url) its DLL answers) is the one control that bypasses the menu: after that
// call the adapter sends the URL as a link opened on the panel's TV, which the mod on each receiver
// redirects into its own player. Not carried: its playback controls and queue (STV_PanelAction,
// STV_PanelValue), its library rows, its Jellyfin sign-in. Game thread.

#pragma once

namespace coop::net {
class Session;
}  // namespace coop::net

namespace coop::mod_compat::smarttv {

void Install(coop::net::Session& session);
void OnDisconnect();

}  // namespace coop::mod_compat::smarttv
