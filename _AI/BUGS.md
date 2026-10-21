# Bugs found while writing tests

Defects found by the test suite. Each entry has the observation, the measured
cause, the effect, and a suggested fix that was **not** applied (see
`_AI/CONVENTIONS.md` section 3). Non-obvious behaviour that is not a defect lives
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

## BUG-6 - WITHDRAWN: "move location" was a test bug, not an engine bug

> **WITHDRAWN 2026-10-21 (test bug, same class as BUG-10).** The A12
> failure (`dx 160 -> 256`) never showed a size change: the test drove
> `source == target` (`make_action(38, 0, 1, 0, 0, 1)` sets both
> indices to locations[0]) and asserted on `locations[1]`, which the
> action never touches. Probe output on 1Drag: `src dx 160,
> untouched-dst dx 256, moved-src dx 160` - the moved region kept its
> size; the test compared two different static regions. The suggested
> fix (adopt the *source* size) would have broken real semantics:
> [Move Location](https://docs.scmjs.dev/triggers/move-location)
> centres the moved location on the unit and **keeps its size** ("The
> moved location keeps its size and is kept inside the map";
> independently: NamuWiki "the location moves to the center of the
> unit"). The engine already does exactly this, including
> width-preserving edge clamps (verified branch by branch in
> `bwgame.h` case 38). A12 now drives source=locations[0],
> target=locations[1] and pins target-size preservation plus
> inside-the-map containment; A12b covers the clamp half. Green on
> both reference maps. Lesson kept: a failing test is a fact about
> the *test setup* until the indices are re-read - see
> `_AI/CONVENTIONS.md` section 2, step 2.

## BUG-7 - "Wait" cannot be tested through the action alone

**Observed while writing A03.** The resume-after-timer behaviour lives in
`process_triggers()` (`trigger_wait_timers` / `trigger_timer`), not in
`execute_trigger_action()`. Calling the action twice in isolation therefore never
exercises the resume path, and it is easy to write a test that "proves" the
wrong thing (the first version of A03 did exactly that and failed).

**Effect:** no user-visible bug, but it is a testability gap: the wait state
machine has two owners (the action and the scheduler), which is also why it is
easy to break when either changes.

**Status: FIXED (verbatim extraction + pin).** The countdown lives in
`advance_trigger_wait_timers()` called once per update; A22 drives it
directly (100 -> 58 -> 16 -> released) without frames. No behavior
change: full suite green on both maps after the move.

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

**Effect, corrected on verification:** every in-engine consumer
guards on `has_shield` (damage absorption, regen, the max-shield sums -
checked branch by branch), so no fight outcome, eval number or regen
tick ever read the phantom pool. What the defect was: wrong instance
state from creation (the project's own L29/U26 pins treat the raw
pool as the defect). No new path opened by the fix: Shield Battery
recharge requires has_shield on the target
(`unit_can_use_shield_battery`), so non-Protoss units can neither gain
nor recharge shields. Fixed at the three creation sites
(create, finish_building_unit, interceptor release).

Why `1Drag` did not show it: the type it spawns (124 appears only on M&M's map
units; 1Drag's units are Protoss/revealer/resource) has `shield_points == 0`
after the data load for those ids, so copying it copied zero.

**Status: FIXED.** Three creation sites gated on `has_shield`;
L29 and the cross-check U26 pass on both maps, full suite green.
Protoss behavior unchanged (guarded readers already required
has_shield everywhere - verified branch by branch before touching).

---

## BUG-9 - WITHDRAWN: the concussive/explosive branches were already correct

> **WITHDRAWN as a defect 2026-10-21, kept as a pinned fact.** The entry
> as written contradicted its own evidence: it claimed small units are
> immune to concussive damage while quoting code that gives them 100%,
> and its suggested fix (0 -> full, 1 -> 50%, 2 -> 25%) would have set
> concussive-vs-small to 50% against every cited source. Verified
> instead, three ways: (1) the code reads `unit_size` 1/2/3 as
> concussive 100/50/25 and explosive 50/75/100 - exactly the Liquipedia
> Damage_Type table, the Damage_Order_of_Operations page, Wikibooks,
> starcraftai.com and classic.battle.net/gs/damage.shtml, which all
> agree; (2) the mapping 1 = small / 2 = medium / 3 = large is pinned
> by C22-C25 against per-unit Liquipedia pages (Marine/Zealot/Zergling
> small, Dragoon large); (3) C41/C42 now pin the multiplier table
> itself (concussive 256/128/64, explosive 128/192/256 of fp8 256),
> so a future "fix" cannot silently invert it.

> What remains open (not a bug, no change): size 0 (specials only -
> revealers, resources, critters; measured census in-repo) takes no
> explosive/concussive damage. No source found for what independent
> sizes should take, and no test covers a size-0 combat case, so the
> branch stays exactly as it was. If a source turns up, it lands with
> a test first.

## BUG-10 - WITHDRAWN: false alarm, the Hatchery does provide 1 supply

This entry claimed `units.dat` was wrong because "Liquipedia says the Hatchery
gives no supply". **That was my error, not a data bug.** The Liquipedia Hatchery
page states: "Hatcheries by themselves provide one Psi to the Zerg player", and
Psi is another name for supply. `units.dat` value of 1 is correct.

How the mistake was made, for the record: I fetched the page with a closed
question ("does it provide supply?"), the reply covered larvae and not supply, and
I treated the absence of the fact as a negative answer - then marked it
`[VERIFIED]` and filed a bug on top of it. See `_AI/CONVENTIONS.md` section 1,
rule added after this.

No code or data change is warranted. P17 now asserts 1 supply.

---

## BUG-11 - `damage_size_multiplier` did not compile (parameter shadowed its type)

**Observed while building the T-series tests:**

```
bwgame.h:13720: error: expected ')' before '(' token
```

**Cause:** the helper added for the size-multiplier work declared

```cpp
static fp8 damage_size_multiplier(weapon_type_t::damage_type damage_type, int unit_size)
```

The parameter name `damage_type` shadows the type name used earlier in the same
declaration, so the compiler cannot parse the parameter list. The body compared
`damage_type == damage_type_concussive`, which then reads as a member access on
the parameter.

**Effect:** the whole translation unit fails to compile - every test and the
harness itself. Nobody noticed because the tree was not built between the change
and this batch.

**Fix applied:** the parameter is now `int dt` (the field is a plain `int` in
`weapon_type_t`, not an enum). Kept here because it shows the failure mode: a
change that "looks like refactoring" can leave the tree unbuildable, and only a
compile gate catches it.

**Suggested process addition:** run `tests/run.sh` (which builds everything)
before any commit that touches `bwgame.h`.
