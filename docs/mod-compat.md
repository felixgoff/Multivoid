# Mod compatibility

## Purpose

Other VOTV mods in a co-op session. A mod that adds its own controls to something Multivoid already
syncs gets an adapter, so pressing those controls on one peer shows the same result on every peer. One
mod is supported now: Moddy's [SmartTV_Beta](https://thunderstore.io/c/voices-of-the-void/p/Moddy/SmartTV_Beta/)
1.0.0.

## How it works

An adapter does not sync the mod's own state. It watches the mod's control at the script-body gate
and hands what the control did to the lane that carries the thing it acts on. That lane runs the
game's own verb on each receiver, and the mod there reacts to it as it would to a local press. So
the mod has to be installed on every peer, and the adapter needs no knowledge of the mod's internals
beyond the one call it watches.

The registry (`coop/mod_compat/mod_compat`) lists the adapters, installs each one's watches, and logs
`[MOD] <mod> is loaded: ...` once the mod's marker class loads. A watch on a class that never loads
never fires, so a missing mod costs nothing.

### SmartTV_Beta

The mod turns the game's TVs (`prop_tv2_C`) into smart TVs. Its DLL hooks the TV's own `useChannel`
and `openLink` `[RD]` (the hook names in its `main.dll`), which is why most of it needs no adapter. A
channel picked from the TV's menu reaches it on every peer through the TV lane
([devices.md](devices.md#tvs)), and so does every link that ends in the TV's `openLink`: the menu's
list and the mod's library and Jellyfin rows, which open a `jellyfin://<server>/<item>` link that
carries no credential; each peer's copy of the mod resolves it with its own sign-in `[V]` (a library
row opened on a client played on the host's copy of the TV). Its URL bar is the one control that skips the menu: the panel's
Open calls `stv_lib_C.STV_Submit(Panel, Url)`, a stub its DLL answers `[RD]` (the pak's
`ui_stv_panel`). After that call, the adapter sends the URL as a link opened on the TV that the
panel's `iface` menu is open on. On each receiver the mod redirects that `openLink` into its own
player.

## Who owns what

Nobody. A press is the presser's, and the last press stands. That is the TV lane's rule.

## Wire messages

No message of its own. An adapter sends through the lane it hands to (`TvState`, in
[devices.md](devices.md#wire-messages)).

## Late join

What the lane sends a joiner: for the TVs, each TV's last channel and last link.

## Known limits

| Limit | Evidence |
|---|---|
| SmartTV's playback controls and queue are not carried (`STV_PanelAction`, `STV_PanelValue`): pause, seek and skip stay on the presser's peer. Each peer opens the link on its own, so playback position differs by the delivery delay and drifts with any local control | `[RD]` the stub library's functions |
| A Jellyfin item plays on a peer only if that peer's copy of the mod is signed in to the same server | `[RD]` the mod's `jellyfin://` link form |
| Local files and SmartTV's inputs (webcams, capture cards, the screen) are opened by name: another peer shows them only if it has the same file or device | `[RD]` `prop_tv2_C` (`OpenFile` on a file name) |
| A peer without the mod gets the vanilla TV's handling of a URL: it opens a direct media link and shows nothing for a page link | `[RD]` `prop_tv2_C` ubergraph (`OpenUrl` on `isUrl`) |
| A URL longer than 160 bytes plays on the presser's peer only, logged once | `coop/interactables/tv_sync` (`SendLink`) |

## Code map

| Concept | Files |
|---|---|
| the registry | `coop/mod_compat/mod_compat` |
| SmartTV_Beta | `coop/mod_compat/smarttv`, `coop/interactables/tv_sync`, `ue_wrap/devices/tv` |
