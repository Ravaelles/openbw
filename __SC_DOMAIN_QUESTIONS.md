# StarCraft domain assumptions in the tests - verification log

Purpose: every test that encodes a **fact about StarCraft as a game** must cite
where that fact comes from. Anything not verified lives here as an open
question, and the corresponding test is marked `[UNVERIFIED]` in its name or is
written so that it tests the *engine's data* rather than the *game's rules*.

Status legend:

- **VERIFIED** - confirmed against Liquipedia (link given).
- **ENGINE DATA** - the test asserts what `units.dat` contains, not what the
  game should be. Safe even if the data is odd: it pins the load.
- **UNVERIFIED** - my assumption, not confirmed. Listed in the questions below.
- **CONFLICT** - Liquipedia and the data files disagree. The test must not pick
  a side silently.

Reference used: [Liquipedia: Protoss](https://liquipedia.net/starcraft/Protoss)

---

## A. Shield rules - the make-or-break one

**CORRECTED.** The earlier claim "Terran and Zerg carry 100 shield points, it is
a data slot" was wrong as stated: in StarCraft **only Protoss units have
shields**; Terran and Zerg have none at all
([Liquipedia: Protoss](https://liquipedia.net/starcraft/Protoss)).

Engine data measured (228 unit types):

```
has_shield flag set        : 47 types   (all Protoss)
shield_points > 0          : 223 types  (includes Terran and Zerg)
Zergling: shield_points=100, has_shield=0
Marine  : shield_points=100, has_shield=0
Zealot  : shield_points=60,  has_shield=1
```

So the `shield_points` field in `units.dat` is **garbage for non-Protoss
types**, and the only correct way to ask "does this unit have shields" is
`unit_type_t::has_shield`, never `shield_points > 0`.

Tests affected: I003, I004, I097. They now assert on `has_shield`.

| id | assumption | status |
|---|---|---|
| I003 | Marine has no shields (Terran) | VERIFIED |
| I004 | Zergling has no shields (Zerg) | VERIFIED |
| I097 | a unit's shields never exceed its type maximum | VERIFIED (follows from A) |
| I001 | Zealot shield is 60 | VERIFIED |
| I002 | Dragoon shield is 80 | VERIFIED |

---

## B. Unit stats used by the tests

| id | assertion | Liquipedia | engine data | status |
|---|---|---|---|---|
| I005 | Zealot hp 100 | 100 | 100 | VERIFIED |
| I006 | Marine hp 40 | 40 | 40 | VERIFIED |
| I007 | Dragoon hp 100 | 100 | 100 | VERIFIED |
| I008 | Zergling hp 35 | 35 | 35 | VERIFIED |
| I009 | Nexus hp | **1500** | **750** | **CONFLICT** |
| I015 | Gateway 150 minerals | 150 | 150 | VERIFIED |
| I016 | Pylon 100 minerals | 100 | 100 | VERIFIED |
| I017 | Nexus 400 minerals | 400 | 400 | VERIFIED |
| D04 | Zealot 100 hp, 60 shield | 100 / 60 | 100 / 60 | VERIFIED |
| D05 | Zealot 100/0 | 100 / 0 | 100 / 0 | VERIFIED |
| D06 | Dragoon 125/50 | 125 / 50 | 125 / 50 | VERIFIED |
| D07 | Marine 50/0 | 50 / 0 | 50 / 0 | VERIFIED |
| I025 | "some unit types have armor" | n/a (data) | 50+ | ENGINE DATA |

Sources:
[Zealot](https://liquipedia.net/starcraft/Zealot),
[Dragoon](https://liquipedia.net/starcraft/Dragoon),
[Marine](https://liquipedia.net/starcraft/Marine),
[Zergling](https://liquipedia.net/starcraft/Zergling).

### B2. CONFLICT: Nexus hp is 750 in the data, 1500 in the game

```
Liquipedia Nexus : 1500 hp
units.dat        : 750 hp
```

A clean 2x factor, which does not look like a balance tweak. Candidates: a
Protoss-specific "shield" column being folded into hp elsewhere, a data-file
variant, or a different units.dat revision. I009 asserts the **game** value
(1500) so the gap shows up on every run.

Until this is explained, **no building-hit-point assertion should be written**
from memory: Nexus is the first one tested and it already disagrees.

---

## C. Open question: Zergling cost is 50/0 in the data, 25/0 in the game

```
Liquipedia Zergling : minerals 25, gas 0, build time 18 s
engine units.dat    : minerals 50, gas 0, build time 420 frames (17.5 s)
```

The build time matches (420 frames at ~24 fps = 17.5 s, Liquipedia rounds to
18). **The mineral cost does not.**

Possible explanations, none confirmed here:

1. this `units.dat` is not the one Liquipedia documents (a modified/custom data
   set, or a partial one),
2. the field is read from the wrong offset in `data_loading::load_units_dat`,
3. Liquipedia documents a different game version for that row.

**Status: CONFLICT, unresolved.** D08 was rewritten to assert the **data value**
(50/0) and to name the conflict in the test, so a future change shows up here
instead of silently "confirming" one of the two.

**This must be resolved before any balance test is written.** If (2) is true,
every cost in the game is shifted by one field.

---

## D. Remaining assumptions to confirm before trusting the tests

These are currently written as **ENGINE DATA** (they describe what the loader
produced), so they cannot be "wrong about StarCraft" - but they also do not
prove the game rule. Listed for review:

| area | tests | question |
|---|---|---|
| Worker flags | D12, I018-I021 | Is `flag_worker` set for exactly Probe/SCV/Drone? Is any non-worker flagged? |
| Men/Buildings/Factory groups | I067, I067b, I070-I072 | Liquipedia does not document these editor groups; the 12-type overlap is an engine fact only. Confirm against StarEdit documentation, not gameplay. |
| Building build times | D09 | Four special types have no build time. Which four, and is that correct for the game or an artefact of the data file? |
| Revealer units | UMS-2, I028-I031 | Are revealers truly editor-only helpers with no gameplay effect? (Engine treats them as vision sources.) |
| MRGN region 63 = whole map | I042 | Is a 0,0-4096,4096 region a legitimate map region, or a parsing artefact? |
| Trigger tick rate | I063, I064 | Triggers are evaluated every 30 frames (engine constant). Confirm this matches StarCraft's "trigger evaluation period". |
| Supply per race | D31 | `supply_used` being per-race is an engine layout fact; confirm nothing else reads it as a single value. |

---

## E. Process rule adopted after the shield mistake

1. A test may only assert a **game rule** if it cites a source in this file.
2. Otherwise it asserts **engine data** and the test name says so.
3. Any disagreement between Liquipedia and the data files is recorded as
   CONFLICT and left open; it is never "resolved" by choosing the convenient
   value.
4. A wrong domain assumption found in an existing test is corrected in the
   test **and** logged here with the correction visible, not silently edited.
