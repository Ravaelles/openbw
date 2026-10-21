# How we work on this project

Terse rules. Each exists because ignoring it produced a real error.

---

## 1. Two axes of truth - prove both, separately

| axis | source | proof |
|---|---|---|
| **game rule** ("Zealot has 60 shields") | [Liquipedia](https://liquipedia.net/starcraft/) | quote <=3 words, `[VERIFIED]` |
| **engine data** ("`units.dat` says 60") | the loaded table | measured value, `[ENGINE DATA]` |

A passing test proves neither on its own. Measure the data, check the rule,
compare. If they disagree, that is a finding - not a reason to accept either.

**Never** assert what a file contains from memory. The file may differ from the
game (it does, in several places - see `_AI/NON-OBVIOUS-FACTS.md`).

### Errors this rule prevents

| claimed | reality |
|---|---|
| "Terran/Zerg `shield_points` = 100, so they have shields" | Only Protoss have shields; 100 is a garbage slot; use `has_shield` |
| "Nexus has 1500 hp" | 1500 = 750 hp + 750 shields |
| "Zergling costs 25" | 25 each, sold in pairs for 50 (the stored value is the cycle) |
| "Hatchery gives no supply" | Page says "one Psi", and Psi == supply (I never read it, and filed false BUG-10) |
| "Most upgrades have 3 levels" | 34 of 61 are single-level abilities |
| "The 100/100 tech is unique" | 15 techs share that price |
| "All priced techs have a race 0..2" | 10 carry race 3 (campaign) |
| "`supply_available` starts at 0" | M&M slot 2 owns 18 Depots -> 144 at load |
| "Building flag means producible structure" | Resources also carry it (0x24002001) |

Pattern: each error was **a gap filled with an assumption and labelled verified**.

---

## 2. Reading a source

1. **Open the page; do not ask it a closed question.** "Does X provide supply?"
   returned a paragraph about larvae; silence is not "no".
2. **Absence of a fact != negation.** Unmentioned = `UNVERIFIED`.
3. **Never write `[VERIFIED]` without a quote** from the page text.
4. **Terms are traps.** Psi = supply. `Defense: 1500 1` = hp, shields, armor -
   the third number is armor. `Cost: 100 0 19` = minerals, gas, **seconds**;
   the 19 is build time, not supply. `Sight: 7` = tiles, `Range: 4` = tiles in
   the wiki but **pixels (x32)** in the data. `Cooldown: 0.63` = seconds in the
   wiki but **frames at 24 fps** in the data.
5. **No bug report against data from an ambiguous reading.**

---

## 3. Writing tests

- One test = one assertion + a name that states the expectation.
- A test must be able to **fail**. No `test(..., true)`, no `ok = ok`. (Two such
  slipped into V14/V46 and were removed.)
- On failure check in order: **(1)** domain assumption, **(2)** real entry
  point (not a reimplementation), **(3)** state leaked from an earlier test,
  **(4)** engine wrong.
- A failing test is a **fact**. Do not weaken it to get green. Record in
  `_AI/BUGS.md` and leave it red.
- Document wrong assumptions **inside the test**, so the next reader does not
  repeat them.

---

## 4. Fixing vs documenting

Recording and fixing are separate acts. `_AI/BUGS.md` holds defects with a
suggested fix that is **not applied**. Fix only when asked; then flip the
matching test from documenting to guarding, and rerun every suite.

---

## 5. Evidence over description

Name the measurement: command, test id, file:line, or URL. "It works" is not
evidence. When a measurement contradicts an earlier claim, correct it
**visibly** (`[CORRECTED]`, `WITHDRAWN`) - never edit history away.

---

## 6. Unsupported behaviour is counted, not hidden

Unknown trigger conditions/actions must not abort a match - count them
(`skipped_*`). Map-declared victory/defeat must log frame, slot and trigger.
Missing features must be observable in state, never a silent `return true`.

---

## 7. Engine copies must not drift

`/sc-ai/OpenBW/openbw` (repo) and `3rdparty/openbw/openbw` (what the harness
builds) diverged once; "it works" then depended on which copy was used. Fix in
both and run the suite against the copy actually hosted.

---

## 8. Build gate before commit

Any change touching `bwgame.h` **must** compile the test suite before commit.
BUG-11 (parameter name shadowing its own type) left the whole tree unbuildable
and nobody noticed, because nothing was built between the edit and the next task.

---

## 9. Running games

- Clean the box first: `scripts/kill-leftovers.sh`, then `scripts/bwapi-publish.sh`
  must report `PUBLISHED`, not `STALE`.
- Never `pkill -f <pattern>`: it matches the calling shell's own command line.
  Use `-x` (name <=15 chars) or anchor to argv0 with `^([^ ]*/)?`.
- One run at a time; two launchers fight over `/dev/shm/bwapi_shared_memory_*`.
- Judge a run by three facts: no timeout (exit != 124), a live published game,
  frame count > 0. `KILLED:0 LOST:0` alone proves nothing.

---

## 10. Scope

`Hero_*` units are out of scope by instruction: do not research them, do not
assert rules about them. Flags may still be pinned as engine data.

---

## 11. Repositories

Commit normally in OpenBW. **No commits in Atlantis** unless the owner asks -
those changes stay in the working tree for its owner to review.

---

## 12. Documentation layout

| file | purpose |
|---|---|
| `_AI/CONVENTIONS.md` | this file |
| `_AI/BUGS.md` | defects BUG-xx (reproduction, suggested fix not applied) |
| `_AI/NON-OBVIOUS-FACTS.md` | N01..: measured engine behaviour that surprises |
| `__SC_DOMAIN_EVIDENCE.md` | every asserted game fact + quote + READ status |
| `__SC_DOMAIN_QUESTIONS.md` | open/answered domain questions and conflicts |
| `tests/run.sh` | builds and runs all suites |

New finding -> exactly one file, cross-referenced from the others.

---

## 13. Session notes (2026-10-20)

- Test suite grew to **13 binaries / 500 assertions**; suites are the main tool
  for learning the engine, not just for regression.
- Two suites stay red on purpose: A12 (BUG-6, `move location` resizes regions),
  L29/U26 (BUG-8 was fixed; the shield-pool check remains as the guard).
- Measured engine quirks worth remembering are all in
  `_AI/NON-OBVIOUS-FACTS.md` (N01-N20): revealer lists, 255-entry MRGN with a
  whole-map region, Melee.trg-only triggers, blocking `callOnFrame`, per-race
  supply arrays, fp8 hitpoints.
