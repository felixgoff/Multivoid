// ue_wrap/devices/tv.h -- reflected access to VOTV's TV (prop_tv2_C and its subclasses) and its menu
// (ui_tvInterface_C): which channel a TV shows, the link it plays, and the two verbs that change them.
// A TV shows one of the gamemode's shared media channels (`channel`, bound by useChannel); a link
// (struct_tv: a name, a link, isUrl) opens on that channel's player through openLink. No network or
// gameplay logic (principle 7). Game thread.
#pragma once

#include <cstdint>
#include <string>

namespace ue_wrap::tv {

// One struct_tv: the menu's label, the URL or the file name under the game's tv folder, and which.
struct Link {
    std::wstring name;
    std::wstring link;
    bool isUrl = false;
};

// True when the actor's class is prop_tv2_C or derives from it (another mod's TV prop does).
bool IsTv(void* actor);

// The TV a menu is open on (its `radio`); null for no menu, no TV, or a field that did not resolve.
void* TvOfInterface(void* ui);

// The TV's `channel`. False when the actor is no TV or the field did not resolve.
bool ReadChannel(void* tv, int32_t& out);

// The struct_tv parameter `data` of `function` (openLink) from its frame.
bool ReadLinkParam(void* function, const uint8_t* locals, Link& out);

// What the menu's setChannel does to its TV: free the channel it shows (the gamemode's
// removeMediaChannel), then bind the new one (useChannel). False when a call did not resolve.
bool SetChannel(void* tv, int32_t channel);

// The TV's own openLink(data, pausePlayer). False when the call did not resolve.
bool OpenLink(void* tv, const Link& link, bool pausePlayer);

}  // namespace ue_wrap::tv
