# Steamhammer (UAlbertaBot) - why it is not our opponent, and what to do instead

> ## !!! IT CANNOT RUN ON OUR TEST MAPS !!!
>
> We host **unit-only maps**: no base, no workers, one fighter per side.
> Steamhammer **crashes** on those before its first frame:
>
> ```
> #0 UAlbertaBot::ParseUtils::ParseConfigFile
> #1 UAlbertaBot::UAlbertaBotModule::onStart
>    -> BWAPI::Broodwar->enemy()->getRace()   <- enemy() is NULL -> SIGSEGV
> ```
>
> - one unit on **one** side -> `enemy()` is null -> **hard crash** in `onStart`.
> - units on **both** sides, no base -> passes `onStart`, then idles forever on
>   `UAB_ASSERT(startingBase, "our base is unknown")`. `UAB_BREAK` is defined
>   **empty**, so the assert only logs - the bot silently does nothing.
>
> It is a **normal-match bot** (economy, build orders). Making it play a base-less
> map means forking 50k lines of assumptions, not a configuration change.
>
> **Decision: we do not use Steamhammer. We write our own small opponent.**
> The rest of this file is kept only in case that decision is ever revisited.

---

## What still exists in the tree (in case of a revisit)

- `3rdparty/Steamhammer/` - upstream sources vendored verbatim, 145 `.cpp`,
  provenance in `README.upstream.md`. Unmodified.
- `3rdparty/Steamhammer-integration/` - builds `libSteamhammer.so` for Linux:
  POSIX entry point (`newAIModule`, `gameInit`, defines `BWAPI::BroodwarPtr`),
  excludes `BOSS/source/deprecated` and the GUI, links **`libBWAPILIB.so`**
  (client API, not `libBWAPI.so` - that mistake cost hours).
- The module **loads and runs**: `dlopen` succeeds, `onStart` executes, and with
  a slot it sees units (measured: `units=11 start=63,46`).

## If revisited: the real obstacle, in one paragraph

The engine has **five** places deciding "who is the local player":
`local_player_id`, `g_LocalHumanID()`, `BWAPIPlayer`, `switchToPlayer`, and the
auto-menu. `self()` is built from `g_LocalHumanID()` -> `local_client->player_slot`,
i.e. from the **sync client**, not from `local_player_id`. A filesystem AI module
has no sync client, so it has no identity unless the harness fakes one. Every
fix to one site broke another; that is why round 1 looped and was reverted.
Round 2 would have to touch all five consistently and prove a real client still
behaves identically.

## Do-not-repeat list (measured, so nobody pays for it twice)

| attempt | result |
|---|---|
| give the module its own slot via `switchToPlayer` | no-op: `inGame` is already true when the module loads (`GameUpdate.cpp:39` vs `:140`) |
| move the switch to `gameInit` | same: `inGame` is set in the same update |
| set `local_player_id` in the setup loop | overwritten by `sync()` every frame (`BWData.cpp:691`) |
| keep the module slot in `sync()` | auto-menu then finds no free human slot -> `no available slot` |
| `STEAMHAMMER_SLOT=<n>` env | had no effect for the reasons above |
| Makefile link to `libBWAPI.so` | 198 unresolved symbols; modules link `libBWAPILIB.so` |

All slot-related engine edits were **reverted**; the tree builds clean.

## Useful side effects that stayed

- **SIGPIPE fix** in `Server::callOnFrame()`: `write()` -> `send(..., MSG_NOSIGNAL)`.
  A client that disconnected mid-frame used to kill the host before the error
  handling ran. This was a real bug, unrelated to Steamhammer.
- Knowledge that `UAB_ASSERT` does not stop the bot, and that this bot's asserts
  go to stderr (its own error-log file is never written).

## Reward, if someone ever makes it work

A mature opponent: economy, build orders, scouting, combat, micro - far better
than anything hand-written. That is why it was worth trying; it is not worth
forcing.
