// coop/interactables/tv_sync.h -- a TV's channel and the link it plays, on every peer.
// A TV shows one of the gamemode's shared media channels, and a link opens on that channel's player, so
// two verbs carry a TV's picture: its menu's setChannel (the TV's `channel` after the body) and the TV's own
// openLink (the struct_tv it opens, read before the body: from the menu's select or a mod's control, never from
// the TV's own graph). Each goes out on the peer whose player pressed it, as TvState; the host relays a client's
// to the other clients; each receiver runs the TV's own verbs on its copy (ue_wrap/devices/tv), a call of ours
// that its watches pass over, so nothing echoes. A mod control that reaches no TV verb sends through SendLink
// (coop/mod_compat). Symmetric: the last pick
// stands. The host keeps each TV's last channel and last link and sends them to a joiner at its world-ready.
// Playback position is not synced: each peer opens the link itself.

#pragma once

#include "coop/net/protocol.h"
#include "ue_wrap/devices/tv.h"

#include <cstdint>

namespace coop::net {
class Session;
}  // namespace coop::net

namespace coop::tv_sync {

// Registers the menu's watches (the per-tick retry pump, coop/interactables/verb_lanes). Game thread.
void Install(coop::net::Session* session);

// Settles the watches; retries a received state whose TV has not streamed in yet. Game thread.
void Tick();

// A player opened `link` on `tv` through a control other than the TV's menu: send it as the menu's
// own pick goes. Nothing for a TV with no Key, a link too long for the wire, or no session. Game thread.
void SendLink(void* tv, const ue_wrap::tv::Link& link, bool pausePlayer);

// A TvState off the wire, its size and fields checked by the dispatcher. Game thread.
void OnReliable(const coop::net::TvStatePayload& p, uint8_t senderSlot);

// HOST, at a joiner's world-ready: each TV's last channel, then each last link in the order opened.
void ConnectReplayForSlot(int slot);

void OnDisconnect();

}  // namespace coop::tv_sync
