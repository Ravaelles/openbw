# Bugs found while writing tests

Found by `tests/trigger_semantics_test.cpp` and by reading the engine while
writing it. Per instruction, **nothing here was fixed** - this file only records
what is broken and how it was observed.

Each entry: what, how to reproduce, effect, and a suggested fix (not applied).

---

## BUG-1 - Condition 4 (switch) ignores switch state in this tree

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
