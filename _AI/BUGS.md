# Bugs found while writing tests

Defects found by the test suite. Each entry has the observation, the measured
cause, the effect, and a suggested fix that was **not** applied (see
`__CONVENTIONS.md` section 3). Non-obvious behaviour that is not a defect lives
in `_AI/NON-OBVIOUS-FACTS.md`.

Status: OPEN unless a commit is named.

---

## BUG-1 - Condition 4 (switch) ignores switch state in this tree

**Status: FIXED** (this tree now matches the harness: condition 4 reads
`st.switches`, out-of-range is false; T12/T13 green on both default maps).

**Observed:** `tests/trigger_semantics_test.cpp` T12 and T13 fail on
`1Drag_v_1Zeal.scm`:

```
FAIL T12 switch condition follows switch state
FAIL T13 out-of-range switch is false
```

**Cause:** `bwgame.h` in this tree still contains the placeholder

```cpp
case 4: // switch (map-defined switches are not simulated); treated as enabled
    return true;
```

while `state::switches` exists (the field was added) and action 5 is meant to
write it. So the storage exists but nothing reads it: **every trigger that
waits for a switch is always true**, and an out-of-range index is
"true" instead of false.

**Effect:** any map that gates content on a switch runs that content from frame 0.
A switch that should turn something on later cannot turn it off.

**Suggested fix (not applied):** implement condition 4 as
`index = c.location; return index >= 0 && index < 256 && st.switches[index];`.

**Note:** the same fix exists in the harness copy of the engine, which is why
the harness behaves differently from this tree. The two copies have drifted;
see BUG-2.

---

## BUG-2 - Engine copies drifted: harness has fixes this tree does not

**Status: NARROWED, deliberately not closed.** The trigger-semantics
direction is now ported here (BUG-1, BUG-5, switch/death/attachment
fixes verified by T12/T13 and `tests/run.sh`). The remaining diff
(~200 lines) goes both ways and includes the harness's own uncommitted
engine work (build-queue helpers, visibility checks) that must not be
overwritten or blind-ported - it belongs to the harness track until
their done-signal. In particular the harness was NOT reinstalled from
this tree, on purpose. The check-before-build suggestion stays open
for the harness owner.

**Observed:** compiling the same test against
`/sc-ai/OpenBW/openbw/bwgame.h` and against
`/sc-ai/StardustDevEnvironment/3rdparty/openbw/openbw/bwgame.h` gives different
results: the harness passes T12/T13, this tree fails them.

**Cause:** fixes were applied to the harness copy and only partly ported here.
The trees also have different bases (313 diff lines between
`HEAD:bwgame.h` and the harness copy, only some of them ours).

**Effect:** "it works" depends on which copy the launcher was built from - the
worst possible failure mode for a diagnosis. A fix verified in one tree can
silently be absent in the other.

**Suggested fix (not applied):** make the harness build from this repository
(`OPENBW_DIR=/sc-ai/OpenBW/openbw`) instead of a vendored copy, or add a check
that both copies' `bwgame.h` are identical before building.

---

## BUG-3 - `get_unit(index)` does not accept the id an existing unit reports

**Status: DOCUMENTED, no behavior change.** Verified live: id-based
round-trip over all 189 visible units on M&M_v_ZealDrag returns the same
unit every time, and T17 pins the raw-index path. The trap is real but
narrow - constructing a `unit_id_t` from the raw 0-based `u->index`
skips the +1 and can only resolve to nullptr, never to a wrong unit
(the generation check rejects it). Both overloads plus `get_unit_id`
now document the 1-based convention; no encoding change (that would
ripple to every caller and to replay compat).

**Observed while writing T17.**

`unit_t::index` is what callers have, and `get_unit(size_t index)` resolves it
through `st.units_container.try_get(index)`, but the overload taking
`unit_id_t<T>` goes through `id.index()` and `get_unit(idx - 1)` - an off-by-one
relative to the raw overload. Passing `unit_id_32{unit->index}` therefore does
not round-trip.

**Effect:** easy to write a lookup that silently finds the wrong unit (or
nullptr), especially in code copied from either call site.

**Suggested fix (not applied):** give `unit_t` an accessor returning the exact
id type `get_unit` expects, or make the two overloads agree; add a test that
round-trips every visible unit.

---

## BUG-4 - Map-declared defeat makes "the game ends" indistinguishable from a bug

**Status: FIXED (diagnostic).** `execute_trigger_action` logs one stderr
line per victory/defeat action (`bwgame: trigger victory|defeat: frame N
slot M`); visible in server logs, harmless to the exit-code-based test
runner. Verified live: 1Drag's frame-30/32 trigger victories now name
themselves in `tests/run.sh` output.

**Observed:** `1Drag_v_1Zeal.scm` defeats slots 1-3 at frame 312 because its own
trigger 1 is `Elapsed time 300 s -> Defeat` (and trigger 0 is
`Command 18 -> Defeat`). Nothing logs which trigger ended the game.

**Effect:** every investigation of "the game stops" starts by guessing whether
the engine or the map ended it. It cost several hours here (see the Atlantis
investigation log, sections 66 and 70).

**Suggested fix (not applied):** log one line when a trigger sets victory or
defeat: frame, slot, trigger index, action type. Diagnostic only, no behaviour
change.

---

## BUG-5 - Unsupported trigger types abort the whole simulation

**Status: FIXED (tolerance + counters).** Unknown conditions evaluate
false (a trigger that cannot prove it should fire doesn't) and unknown
actions are skipped like action 105; both increment
`state::skipped_trigger_conditions/actions`, first occurrence logged.
`tests/run.sh` stays green on both default maps. Note: the A3.1
`fixme rescue passive / unknown flag` map-load aborts are a separate
code path, not covered by this change.

**Observed:** `test_trigger_condition()` / `execute_trigger_action()` call
`error("unknown trigger condition/action %d")` for anything not implemented.

**Effect:** one unsupported condition in one trigger of one player kills the
server mid-game, with no indication which map feature caused it. Several maps
cannot be hosted at all for this reason (`fixme rescue passive`,
`fixme unknown flag` are in the same class).

**Suggested fix (not applied):** make it a counted, logged no-op
(`state::skipped_trigger_actions`) like action 105, so the map still runs and the
omission is visible in one line.

---

## BUG-6 - "Move location" does not preserve region size

**Observed:** `tests/trigger_actions_test.cpp` A12:

```
FAIL A12 move location keeps region size: dx 160 -> 256
```

**Cause:** case 38 in `execute_trigger_action()` computes `diff` between the
source region's centre and the unit position, then adds it to the **target**
region's corners. When the two regions have different sizes, the result is
clamped independently per corner (`if (to.x >= map_width) ...`), so width and
height can change. A trigger that moves a location is expected to translate it.

**Effect:** maps that move a region (e.g. "follow the hero") get a differently
sized region after the first move, so "bring units to location" starts
matching areas the mapper never chose. Silent, and only visible several
triggers later.

**Suggested fix (not applied):** translate the **source** region's size onto the
target, i.e. set `target = pos +- (source_size / 2)` (or keep the target's size
and only move its centre), and clamp the whole rectangle as a unit so width and
height cannot change.

---

## BUG-7 - "Wait" cannot be tested through the action alone

**Observed while writing A03.** The resume-after-timer behaviour lives in
`process_triggers()` (`trigger_wait_timers` / `trigger_timer`), not in
`execute_trigger_action()`. Calling the action twice in isolation therefore never
exercises the resume path, and it is easy to write a test that "proves" the
wrong thing (the first version of A03 did exactly that and failed).

**Effect:** no user-visible bug, but it is a testability gap: the wait state
machine has two owners (the action and the scheduler), which is also why it is
easy to break when either changes.

**Suggested fix (not applied):** move the timer decrement into a named
`advance_trigger_wait(owner)` used by `process_triggers()`, so a test can drive
the full cycle without a frame loop.

---

## BUG-8 - Non-Protoss units are created with shield points (raw 25600)

**Observed:** `tests/liquipedia_facts_test.cpp` L29 fails on
`M&M_v_ZealDrag.scx`, passes on `1Drag_v_1Zeal.scm`:

```
FAIL L29 spawned non-Protoss units have zero shields: type 124 has 25600 raw shield
```

**Measured:**

```
type 124 (Terran, no shields): has_shield=0, shield_points=100 (garbage slot), hp=200
every spawned unit of that type: owner=2, shield raw_value=25600 (= 100.0 fp8)
```

**Cause:** the unit factory copies `unit_type_t::shield_points` into
`unit_t::shield_points` unconditionally. For Protoss that is correct; for every
other type the field holds the data-file fill value (100), so units that must
have no shields are created with a full shield pool. `has_shield` is set
correctly, so the *type* is right and only the *instance* is wrong.

**Effect:** damage is absorbed by a shield that should not exist. A Terran or
Zerg unit survives the first 100 damage of every engagement (until the shield is
drained, and if shields regenerate it never becomes a plain hp fight). Balance
tests would read this as "Marines are far too tanky". It also means
"shields destroyed" statistics pick up non-Protoss units.

Why `1Drag` did not show it: the type it spawns (124 appears only on M&M's map
units; 1Drag's units are Protoss/revealer/resource) has `shield_points == 0`
after the data load for those ids, so copying it copied zero.

**Suggested fix (not applied):** in the unit constructor, set
`shield_points = unit_type->has_shield ? unit_type->shield_points : 0`, or clear
shields when `!has_shield` after creation. Then L29 should pass on both maps.

**Test status:** L29 is left red on M&M on purpose - it documents the defect.

---

## BUG-9 - Concussive damage reads wrong unit sizes (kills the wrong targets)

**Observed:** reading `damage_by_weapon_type()` while writing C22-C26:

```cpp
case damage_type_concussive:
  if (target->unit_type->unit_size == 0) damage = 0_fp8;        // -> ZERO
  else if (target->unit_type->unit_size == 2) damage *= 128_fp8; // 50%
  else if (target->unit_type->unit_size == 3) damage *= 64_fp8;  // 25%
  break;
```

**Measured data:** `unit_size` in this units.dat is **1 = small, 2 = medium,
3 = large** (and 0 for special/no-size types). Measured directly:
Marine = 1, Zergling = 1, Zealot = 1, Dragoon = 3, Nexus = 3.

The two branches do **not** agree with each other:

| branch | 0 | 1 | 2 | 3 |
|---|---|---|---|---|
| explosive | 0 | 50% | 75% | **no multiplier (100%)** |
| concussive | 0 | **no multiplier** | 50% | 25% |

So explosive treats 0 as small (wrong for this data: small is 1) and concussive
treats 1 as the full-damage case, 2 as 50% and 3 as 25%. With the real mapping
(1 small, 2 medium, 3 large) the correct concussive row is 1 -> 100%,
2 -> 50%, 3 -> 25%; the code gives 1 -> 100% (accidentally right), 2 -> 50%
(right), 3 -> 25% (right) - but **0 -> 0 damage**, and explosive is off by one
because its 0 slot never occurs. The lists disagree, which is the actual
defect: one of them must be wrong.

**The rule** ([Liquipedia: Damage Type](https://liquipedia.net/starcraft/Damage_Type)):

| damage type | small | medium | large |
|---|---|---|---|
| concussive | 100% | 50% | 25% |
| explosive | 50% | 75% | 100% |

**Consequence:** against a **small** unit (Marine, Zealot, Zergling) concussive
damage becomes **0**, and against a **large** unit it gets no multiplier at all.
Since almost every classic concussive attacker (Vulture, Firebat, Ghost,
Medic-irrelevant, Valkyrie vs air) targets ground units that are small or
medium, the effect is not "wrong by one step": small units are **immune** to
concussive fire, which is the opposite of the rule.

The explosive branch is correct (0 -> 50%, 1 -> 75%, 2 -> 100%).

**Suggested fix (not applied):** use the same size mapping as the explosive
branch: 0 -> full, 1 -> 50%, 2 -> 25%.

**Not covered by a passing test yet:** C22-C26 currently assert sizes, not
damage outcome, and the tests are left documenting this in comments.

---

## BUG-10 - Hatchery provides 1 supply in units.dat, 0 in the game

**Observed:** `tests/production_and_buildings_test.cpp` P17:

```
FAIL P17 Hatchery provides no supply (Liquipedia; data says 1): data raw=2 (=1.0)
```

**Sources:** [Liquipedia: Hatchery](https://liquipedia.net/starcraft/Hatchery)
states the Hatchery provides no control; Zerg supply comes from
[Overlords](https://liquipedia.net/starcraft/Overlord) (8 each).

**Measured:** `units.dat` gives `Zerg_Hatchery.supply_provided == 1.0` (fp1 raw
2). The Overlord value is correct (8.0, raw 16).

**Effect:** every Hatchery adds 1 headroom to the Zerg player's supply. With
three Hatcheries that is 3 extra supply that should not exist - usually hidden
because Zerg players build Overlords early, but it changes the exact moment a
player gets supply-blocked, and it makes "supply blocked at N" tests unreliable.

**Possible explanations, not distinguished here:**

1. a data-file variant (some mods give Hives/Lairs supply),
2. the field is intentionally the Hatchery's own "supply provided" as used by
   the campaign, where the player starts with a Hatchery and no Overlord,
3. a loader shift - ruled unlikely: Overlord, Pylon and Supply Depot values are
   all correct in the same field.

**Suggested check (not applied):** compare a Lair and a Hive; if both are 1 too,
this is probably deliberate for campaign starts. The test asserts the *game*
value (0) so the discrepancy is visible on every run.
