// coop/mod_compat/mod_compat.h -- other VOTV mods in a session. A mod that adds controls of its own to
// something the lanes already carry is supported by an adapter: it watches the mod's control at the
// script-body gate and hands what the control did to the lane that carries the thing, so the other peers
// run the game's own verb and the mod there reacts as it does to a local press. An adapter registers its
// watches whether or not the mod is installed (a watch on a class that never loads never fires), and the
// mod is named in the log once its marker class loads. The adapters, one file each:
//   SmartTV_Beta (Moddy) -- coop/mod_compat/smarttv: its URL bar opens the link on the TV for every peer.
// Every peer needs the mod for its controls to do anything there. Game thread.

#pragma once

namespace coop::net {
class Session;
}  // namespace coop::net

namespace coop::mod_compat {

// Each adapter's watches (the per-tick retry pump, coop/interactables/verb_lanes).
void Install(coop::net::Session& session);

// Names a supported mod in the log once its marker class loads.
void Tick();

void OnDisconnect();

}  // namespace coop::mod_compat
