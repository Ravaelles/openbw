# Conventions for working on this project

Rules agreed during the UMS/OpenBW work. They exist because several defects and
several wrong test assertions came from skipping one of them.

---

## 1. Game knowledge comes from Liquipedia, first

**Canonical source: [Liquipedia StarCraft](https://liquipedia.net/starcraft/).**

When a test, a comment or a fix asserts **anything about how StarCraft plays** -
unit stats, costs, shields, production multiples, trigger behaviour, timings -
look it up there before writing the assertion. Memory is not a source.

Consequences:

- Every domain assertion in a test carries a `// [VERIFIED <page>]` marker, or a
  `[ENGINE DATA]` marker if it only describes what was loaded.
- Values that disagree are **not** silently "fixed" to the convenient side; they
  are recorded in `__SC_DOMAIN_QUESTIONS.md` as CONFLICT and left open.
- If Liquipedia does not document something (editor groups, chunk layout,
  engine constants), the test asserts **engine data** and says so.

### Traps already hit (do not repeat)

| trap | reality |
|---|---|
| "Terran/Zerg shield_points is a shield" | **Only Protoss have shields.** `shield_points` is 100 garbage for 223 of 228 types; the authoritative field is `has_shield` (47 types, all Protoss). |
| "Nexus has 1500 hp" | 1500 is **hp + shields** (750 + 750). Liquipedia's "hit points" for Protoss can mean the total damage needed. |
| "Zergling costs 25" | Zerglings come **in pairs** from one larva: 50 per cycle, 25 each. `units.dat` stores the **cycle** cost. |
| Liquipedia "Defense: 1250 1" | the second number is **armor**, not a shield. |
| Units produced in multiples | `units.dat` holds the cost of one **production cycle**, not one unit. |

---

## 2. Tests: one assertion, one reason

- One test = one check with a human-readable name that states the expectation.
- A failing test is a **fact about the engine**. Do not weaken an assertion to
  make it pass; first decide whether the test or the engine is wrong.
- When a test fails while being written, check in this order:
  1. is the domain assumption right (section 1)?
  2. is the test driving the real entry point?
  3. is the test leaking state from an earlier test?
  4. only then: is the engine wrong?
- Failures caused by the engine are recorded in `__BUGS.md`; the test stays red
  until someone decides to fix it. Never delete a test to get a green run.

---

## 3. Never fix while writing tests

Recording and fixing are separate acts:

- `__BUGS.md` - defects, each with reproduction and a suggested fix that was
  **not** applied.
- Fixes happen only when explicitly asked for, and then the corresponding test
  is flipped from documenting the defect to guarding the fix.

---

## 4. Evidence over description

- Every claim in a doc, a commit message or an answer names the measurement:
  a command, a test id, a file and line, or a Liquipedia page.
- "It works" without a runnable check is not evidence.
- When a measurement contradicts an earlier claim, **correct it visibly**
  (`[CORRECTED]` / "RESOLVED" sections) instead of editing history away.

---

## 5. Unsupported engine behaviour is counted, not hidden

- Unknown trigger conditions/actions must not abort the match; they are counted
  (`skipped_*` counters) and logged.
- A map-declared victory/defeat is logged with frame, slot and trigger.
- If something cannot be implemented yet (AI scripts), the omission must be
  observable in state, not a silent `return true`.

---

## 6. Engine copies must not drift

- `/sc-ai/OpenBW/openbw` is the repository; the harness builds from
  `3rdparty/openbw/openbw`. They have diverged before, and "it works" depended
  on which copy was used.
- Any engine fix must be present in both, and the test suite run against the
  copy that is actually hosted.

---

## 7. Operational rules for running games

- Clean the box before a run: `scripts/kill-leftovers.sh` (argv0-anchored,
  case-insensitive), then check `scripts/bwapi-publish.sh` reports
  `PUBLISHED`, not `STALE`.
- Never use `pkill -f <pattern>`: the pattern matches the calling shell's own
  command line. Match the process name exactly (`-x`) or anchor to argv0.
- One run at a time; a second launcher competes for `/dev/shm/bwapi_shared_memory_*`.
- A run is judged on three facts: no timeout (exit 124), a live published game,
  and a frame count above zero.

---

## 8. Repositories and commits

- Commit normally in the OpenBW repository.
- **No commits in Atlantis** unless the owner asks; changes there stay in the
  working tree for review by its owner.
- One commit per coherent step, with a message that states what was measured.

---

## 9. Documentation layout

| file | purpose |
|---|---|
| `__CONVENTIONS.md` | this file: how we work |
| `__BUGS.md` | non-obvious facts (N01..) and defects (BUG-xx) |
| `__SC_DOMAIN_QUESTIONS.md` | every StarCraft domain assumption and its verification status |
| `tests/run.sh` | builds and runs every suite on several maps |
| Atlantis `docs/openbw-run-regression.md` | chronological investigation log |

A new finding goes into exactly one of these, and cross-references the others.
