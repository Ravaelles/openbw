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
