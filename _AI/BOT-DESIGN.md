# Zombie bot - architecture and requirements

Status: **design, no code yet**. Purpose: an opponent for **unit-only test maps**
(no base, no workers, a few fighters per side), where Steamhammer cannot run
(see `PLAN-STEAMHAMMER.md`).

---

## 1. Requirements

### Functional

| # | requirement | how it is judged |
|---|---|---|
| F1 | Every own armed unit attacks an enemy | after N frames, no idle armed unit while an enemy is reachable |
| F2 | Melee units prefer the **nearest** enemy | target chosen by distance |
| F3 | Ranged units prefer the **most wounded** enemy in range, else the nearest | target chosen by hp, then distance |
| F4 | Units re-target when their target dies | no unit keeps a dead pointer |
| F5 | Bot never stalls the match | engine keeps stepping; no exception |
| F6 | Bot is optional and off by default | existing tests unaffected |

### Non-functional

| # | requirement |
|---|---|
| N1 | **In-process**, compiled into the engine: no BWAPI client, no slots, no shared memory |
| N2 | Small and readable: one file, ~200-300 lines, no new dependencies |
| N3 | Deterministic: same state + same frame -> same orders (replayable) |
| N4 | Testable without a match: pure functions over state, driven by a unit test |
| N5 | Extensible: new behaviours (retreat, focus fire, kiting) are separate policies |

---

## 2. Constraints found by measurement (not assumptions)

| fact | consequence |
|---|---|
| `enemy()`-style lookups crash when a map has no opponent | the bot must find enemies itself, never assume one exists |
| Engine already has `find_units`, `unit_target_is_enemy`, `unit_can_attack_target` | use them; do not re-implement visibility/enemy rules |
| Engine has `find_target()` with `unit_target_attack_priority` (`bwgame.h:1943`) | good default, but it optimises *engine* priorities, not our zombie policy -> we wrap, not replace |
| `set_unit_order(u, order, order_target_t)` issues orders | the actuator's single entry point |
| the attack order for a **unit** target is `Orders::AttackUnit` (id **10**); `Orders::AttackMove` is 14, `Move` is 6, `Guard` is 2, `PlayerGuard` is 3 | `Attack` alone is not an order name in this enum - measured, do not guess it |
| Units belong to players; `alliances` decide enemies | multi-side maps need no special case |
| `unit_t::hp` is fp8; `shield_points` fp8 | wound comparison must use integer_part or raw consistently |

---

## 3. Architecture

Three layers, each replaceable, so the bot can grow without rewrites.

```
+---------------------------------------------------------------+
|  Tick driver        zombie_bot::tick(state&, player)          |
|  - decides WHEN to think (every N frames, deterministic)      |
+-------------------------------+-------------------------------+
                                |
+-------------------------------v-------------------------------+
|  Policy (pure functions, testable without a match)            |
|    choose_target(unit, enemies, mode) -> unit_t*              |
|    mode = MELEE | RANGED                                      |
+-------------------------------+-------------------------------+
                                |
+-------------------------------v-------------------------------+
|  Actuator           issue_attack(unit, target)                |
|                     issue_move(unit, pos)                     |
|  - the only place that touches engine ordering APIs           |
+---------------------------------------------------------------+
```

### Why this split

- **Policy is pure**: given a unit and a list of enemies it returns a pointer.
  That is where the zombie logic lives and where the rules are unit-tested
  without any match.
- **Actuator is the only impure part**: one place issues orders, one place to
  change when the engine API changes.
- **Tick driver** owns timing and the list of enemies; swapping it for a smarter
  scheduler does not touch policy.

---

## 4. Target selection rules (the actual zombie behaviour)

```
for each own unit u that can attack:
    enemies = units of enemy players, visible, attackable by u

    if u has no weapon -> skip
    if u is melee (max_range < one tile):
        target = nearest(enemies)                      # F2
    else:
        in_range = enemies within u's max_range
        if in_range not empty:
            target = min_hp(in_range)                  # F3
            tie-break: nearest, then lowest index      # deterministic
        else:
            target = nearest(enemies)                  # F3 fallback

    if target: issue_attack(u, target)
    else if enemies exist but none attackable: issue_move(u, towards nearest)
```

Notes:

- **Melee**: nearest enemy regardless of hp. It has to walk anyway; the nearest
  one costs the least time.
- **Ranged**: wounded first *inside range* (it can shoot now), otherwise walk to
  the nearest. This is the "finish what you started" behaviour: a ranged unit
  does not chase a far wounded unit past a near healthy one.
- **Determinism**: ties broken by distance, then unit index - never by container
  order, which can change.

---

## 5. What "wounded" means

`hp + shields` as integers, compared as `hp + shields` (both fp8, same scale).
Shields first in real combat, but for *target choice* the total remaining pool
is what matters; a unit at 30/100 is the same priority whether it has shields or
not. Documented so it is a decision, not an accident.

---

## 6. Extension points (for later, not now)

| behaviour | where it plugs in |
|---|---|
| retreat when losing | new policy function, consulted before `choose_target` |
| focus fire across units | tick driver collects targets first, then assigns |
| kiting (ranged back off) | actuator gets `issue_move` + cooldown awareness |
| group movement | tick driver computes a rally point, actuator moves first |

Each is additive: no change to the three-layer shape.

---

## 7. Testing strategy

| level | test | needs a match? |
|---|---|---|
| policy unit tests | fake units: nearest / wounded / tie-break / no enemies | **no** |
| integration | load a unit-only map, run 300 frames, assert kills > 0 | yes |
| regression | existing suites unchanged when the bot is off | yes |

Policy tests are the bulk: they are fast and cover the rules that are easy to get
wrong. The match test only proves wiring.

---

## 8. Risks

| risk | mitigation |
|---|---|
| engine targeting differs from our policy | we call `unit_can_attack_target`, so we never order an impossible attack |
| bot fights its own ally on multi-side maps | use `unit_target_is_enemy` only |
| stale target pointers after a kill | re-choose every tick; never cache across ticks |
| nondeterminism from iteration order | tie-break on index; iterate `visible_units` in engine order |
| performance on large maps | O(own * enemies) per tick, throttled to every N frames |

---

## 9. Non-goals

- No economy: the bot never builds or mines.
- No scouting: it only reacts to what it can see.
- No Steamhammer integration: separate, documented as unusable here.

---

## 9a. Implementation log

- `bots/zombie_bot.h`: policy + actuator written. Policy is pure; 14 unit tests
  in `tests/zombie_bot_test.cpp` pass without a map.
- Two bugs found by the tests, both fixed in the design rather than papered over:
  - `unit_hp_total` summed **raw** fp8 values (12 hp == 3072 raw). Ordering was
    still correct but the numbers were meaningless; now `integer_part()`.
  - The ranged fallback used the wounded-first ordering; the design says
    "nearest". Z04 caught it.
- Third failure was **in the test**: Z04 had its distances swapped (100 was
  nearer than 400) and asserted the opposite of its own comment.
- Engine order ids confirmed by name, not memory: `AttackUnit` 10, `AttackMove`
  14, `Move` 6, `Guard` 2, `Stop` 1.

## 10. Definition of done

1. `_AI/BOT-DESIGN.md` implemented as `zombie_bot.h` (one header, no .cpp).
2. Policy unit tests pass with no map loaded.
3. On a unit-only map with both sides armed: kills occur within 300 frames.
4. `_run-tests.sh` green, bot disabled by default.

## 9b. Integration test findings (measured)

| map | result |
|---|---|
| `GenericTwoBases.scm` | **6/6** - bot acts, units die, and the outcome **differs** from an un-driven control run |
| `1Drag_v_1Zeal.scm` | 5/6 - M06 correctly fails: the map's own `Elapsed time -> Defeat` trigger kills slot 3 at frame 312 whatever the bot does |

Two traps caught by this test:

1. **No control run = false credit.** M05 first only checked "units died". A map
   with `Elapsed time -> Defeat` loses units by itself (measured: slot 1,
   154 -> 0 in 400 frames with no bot). M06 was added: run the same map with no
   bot and require the outcome to differ.
2. **Count armed units, not units.** `1Drag` slot 1 owns 139
   `Special_Map_Revealer` helpers with no weapon; a raw count called that a fight.
