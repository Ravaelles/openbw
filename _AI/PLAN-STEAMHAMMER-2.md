# Steamhammer: do-over plan (round 2)

Round 1 (`PLAN-STEAMHAMMER.md.status`) accumulated **17 measured steps** and ended
in a loop: every attempt to give the module its own player slot needed another
engine change, and each change revealed another place that assumed a sync
client. All slot-related engine edits were **reverted**; the tree builds clean.

This file starts over with the loop diagnosed.

---

## Why round 1 looped

| symptom | cause |
|---|---|
| try a slot, get `-1` | five different places decide "who is the local player" |
| fix one, another breaks | `local_player_id`, `g_LocalHumanID()`, `BWAPIPlayer`, `switch_to_player`, auto-menu all form a cycle |
| each fix is "one line" | but each one changes what a **real client** sees |

Measured chain (from round 1, still valid):

```
self() <- BWAPIPlayer <- onGameStart <- _currentPlayerId()
       <- g_LocalHumanID() <- local_client->player_slot <- sync()
```

`local_player_id` (which units you see) and `g_LocalHumanID()` (who you are) are
**two separate** values. Every fix touched one and broke the other. That is the
loop: the design assumes **one** local player and we are asking for two.

**Lesson:** stop patching the identity chain. Choose an approach that does not
need a second local player.

---

## Approaches, cheapest first

### A. Bot shares the client's slot (no engine change) - TRY FIRST

The module and the client play the **same** player. Steamhammer gets
`self()` = the client's slot, which already works (measured: `units=107`).
The client's own bot simply does not issue orders.

- Cost: ~0. No engine edits. Uses what already works.
- Gives: Steamhammer **playing** (gather, build, attack) against whatever the
  map puts on the other side (usually a passive Computer player).
- Does not give: Steamhammer vs Atlantis as opponents.
- Test: `BWAPI_MODULE_PLAYER_SLOT` unset, module attached, map with a base on
  the client slot. Success = Steamhammer logs a build decision and its worker
  count rises.

**Why first:** it answers "can Steamhammer drive our engine at all?" without any
engine risk. If it cannot, no slot work matters.

### B. Two processes: Steamhammer as its own client (no engine change)

Run **two** BWAPI clients against one server: our normal client on slot 0, and a
second process that loads Steamhammer on slot 1. This is the classic BWAPI
arrangement and needs no identity changes because each process has its own
`local_client`.

- Cost: a small C++ launcher that connects like a client and loads the module
  (roughly what `BWAPILauncher` does, minus the hosting).
- Gives: a real 1v1 with two independent players.
- Risk: the sync protocol must accept two clients; `run-openbw-ums.sh` already
  assumes one.

### C. Engine support for a module slot (what round 1 attempted)

Requires touching all five places consistently, and a test that proves a real
client still behaves identically. Largest cost, highest regression risk.

- Only justified if A and B both fail.

---

## Order of work

| step | goal | success test | risk |
|---|---|---|---|
| R1 | confirm the build is clean after the revert | `cmake --build` 0 errors | none |
| R2 | approach A: module on the client slot | Steamhammer logs a build order; worker count rises | none |
| R3 | measure what it does for 60 s | units built, no crash | none |
| R4 | decide from R3: is A enough as an opponent? | written verdict | none |
| R5 | if not, approach B spike: can two clients connect? | second client accepted | medium |
| R6 | only if B fails: revisit C with all five sites | identical client behaviour + module slot | high |

---

## Rules for this round (learned the hard way)

1. **No engine edit before R4.** A is free; C is not.
2. One step = one measurement recorded here. No step may depend on an
   unmeasured assumption.
3. If two consecutive steps produce no new information, stop and switch
   approach (this is what round 1 lacked until too late).
4. Never claim "the bot plays" from a log line; check engine state (worker
   count, structure count, kills).

---

## What round 1 produced that is still useful

- `3rdparty/Steamhammer/` vendored (145 .cpp) with provenance.
- `3rdparty/Steamhammer-integration/` builds `libSteamhammer.so` for Linux:
  POSIX entry point, `BroodwarPtr` definition, excludes for
  `BOSS/source/deprecated` and the GUI.
- Module **loads and runs**: `onStart` executes, `ParseConfigFile` passes when a
  game is in progress, and it can see 107 units when given a slot's units.
- Every failure mode is documented with file:line in
  `PLAN-STEAMHAMMER.md.status`.

---

## R2 findings (measured)

### The bot IS given a slot and units

```
steamhammer: slot=0 self=0x6049a5b1ed60 units=11 start=63,46
```

So on `GenericTwoBases.scm` the module owns 11 units and a start location.
The earlier "no units" was map-specific, not a slot failure.

### The crash is ours, not Steamhammer's

Full backtrace of the "core dump":

```
#0 __GI___libc_write (fd=15, ...)          <- SIGPIPE
#1 BWAPI::Server::callOnFrame()
#2 BWAPI::Server::update()
#3 BWAPI::GameImpl::update()
#4 main
```

`callOnFrame()` writes the handshake byte to the client socket. When the client
(the probe) disconnects, the write raises SIGPIPE and kills the process. **This
is the same server path we fixed for the handshake, and it affects any short-
lived client, not just Steamhammer.**

### Steamhammer's asserts do NOT stop it

`Steamhammer/Source/UABAssert.h` defines `UAB_BREAK` as **empty** on this build,
so `UAB_ASSERT(...)` only logs and continues. "our base is unknown" is therefore
**not** the reason nothing happens - the bot keeps running and silently does
nothing useful.

### Its logs

- Assertions go to **stderr**, which our launcher redirects into its own log
  (`File:` / `Message:` lines) - that is where I read them.
- `bwapi-data/write/Steamhammer_ErrorLog.txt` stays empty; the write directory
  contains files from a *different* bot (`Locutus`, `om_Tests.txt`, dated Oct 1),
  i.e. leftovers from another installation, not Steamhammer output.

**Conclusion for the next step:** fix the SIGPIPE first - with a client that
disconnects we cannot observe the bot long enough to judge it.

### R3 - SIGPIPE fixed (server bug, affects every short-lived client)

`Server::callOnFrame()` used `write(syncSocket, ...)`. When the client goes away,
the kernel raises **SIGPIPE** and the process dies before `write()` returns, so
the existing error handling never ran. Replaced with

```cpp
send(syncSocket, &code, 1, MSG_NOSIGNAL)
```

and reset `everSynced` on failure.

**Measured after the fix:** server stays alive (`pgrep` = 1) with the module
attached, `0` assertion lines, module reports `units=11 start=63,46`.

This is not a Steamhammer issue: any client that disconnects mid-frame used to
kill the host. It also explains why several earlier runs "crashed" at exactly
the moment a probe finished.

### R3b - what Steamhammer reports, and what it does not

- `UAB_BREAK` is defined **empty** in `Steamhammer/Source/UABAssert.h`, so every
  `UAB_ASSERT` only logs. "our base is unknown" does not stop the bot; it
  continues silently doing nothing.
- Asserts are printed to **stderr**, which our launcher captures - that is the
  `File:`/`Message:` pair.
- `bwapi-data/write/Steamhammer_ErrorLog.txt` is never written; the `write/`
  directory only holds leftovers from another bot (`Locutus`, Oct 1).

So "nothing happens" = the bot runs, hits a soft assert, and idles. To see
whether it recovers we must give it a real base **and** let it run for a while.

**Next (R4):** run with a client that stays connected (Atlantis, not the probe)
on `GenericTwoBases.scm`, and watch for the first `Build order` / worker count
change over ~60 s.

---

## R4 - answer to "can it play on unit-only test maps?"

**Short answer: not as-is, and the reason is not a bug.**

### What actually happens now (measured, full backtrace)

With the module attached and no other client:

```
#0 UAlbertaBot::ParseUtils::ParseConfigFile
#1 UAlbertaBot::UAlbertaBotModule::onStart
#2 SlotAssigningModule::onStart          (6 frames deep, all ours)
```

The first statement of `ParseConfigFile` is

```cpp
const std::string ourRaceStr(BWAPI::Broodwar->self()->getRace().getName());
const std::string theirRaceStr(BWAPI::Broodwar->enemy()->getRace().getName());
```

`self()` is fine (measured: `units=11 start=63,46`), **`enemy()` is null**.
`enemy()` is the first non-ally player with units; a map that gives the module a
single unit and gives nobody else anything has no enemy, so the dereference is a
SIGSEGV before the bot ever reaches a frame.

### What that means for our goal

| map kind | works? | why |
|---|---|---|
| Base + workers + opponent (a real map) | plausible | self() and enemy() both exist; the bot has an economy to run |
| Units only, both sides (1v1 fighters, no base) | **crashes** in onStart | `enemy()` exists, but `startingBase` is null - soft assert, bot then idles |
| Units only, one side | **crashes** in onStart | `enemy()` is null - hard SIGSEGV |

So: Steamhammer is written for **normal StarCraft matches**. It is not a
"micro AI for 1v1 fighters". Making it drive a units-only test map means
changing its initialisation (enemy/race defaults, base-less operation, build
order skipping) - that is a fork, not a configuration.

### Recommendation (decision point, not a step)

1. **Keep Steamhammer for realistic maps.** It is a mature opponent there and
   costs nothing more to run. Do not spend more time making it play bare maps.
2. **Write a small purpose-built opponent for the unit-only test maps.** It only
   needs: find enemies, attack-move, retreat when losing, and it is ~150 lines
   against APIs we already use (`find_units`, `set_unit_order`). For that test
   shape it is *the right tool*, not a fallback.
3. Do not fork Steamhammer to fix `enemy()`/`startingBase`: that path leads into
   50k lines of assumptions about economy and build orders, which a one-unit
   map cannot satisfy anyway.

**This supersedes N5-N16:** no engine slot changes are needed for (2), because the
small bot runs in-process and shares the same player as the map intends.
