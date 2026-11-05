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

// Bind the TV to `channel` (0 or more) through its own useChannel. The old channel is not freed: the
// gamemode's channels are shared by every TV on them, and a mod that keeps its own per-channel players
// (SmartTV) skips the menu's free on purpose. False for a negative channel or a call that did not resolve.
bool SetChannel(void* tv, int32_t channel);

// The TV's own openLink(data, pausePlayer). False when the call did not resolve.
bool OpenLink(void* tv, const Link& link, bool pausePlayer);

// True when `obj` is a TV's menu (ui_tvInterface_C): the caller of the playback calls a player makes.
bool IsMenu(void* obj);

// What the menu's play bar does to the TV's media player (its `mediaPlayer`, the channel's): Pause, Play, Seek to
// `ms` milliseconds; and its skip button, the TV's own fin (the next item). False when the call did not resolve.
enum class Playback : uint8_t { Pause, Play, Seek, Skip };
bool RunPlayback(void* tv, Playback what, int32_t ms);

// The length of what the TV's player holds, in milliseconds (its GetDuration, which a mod like SmartTV answers for
// its own player). False when the call did not resolve or the length is not known.
bool DurationMs(void* tv, int64_t& ms);

}  // namespace ue_wrap::tv
