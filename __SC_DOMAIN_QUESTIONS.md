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
| I009 | Nexus 750 hp + 750 shield | 750 + 750 = 1500 total | 750 + 750 | VERIFIED |
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

### B2. RESOLVED: "Nexus has 1500 hp" means 750 hp + 750 shields

```
Liquipedia Nexus : hit points 750, shields 750  -> 1500 damage to destroy
units.dat        : 750 hp, 750 shield_points
```

There was never a conflict. "1500 hit points" is the common shorthand for the
total damage a Nexus absorbs, which a Protoss building splits between
non-regenerating-in-combat hp and its shield pool. units.dat is correct on both
fields.

**Rule for future tests:** when a source quotes "hit points" for a Protoss
unit, check whether it is quoting hp or hp+shields. For non-Protoss units the
two figures are the same because they have no shields.

---

## C. RESOLVED: Zergling costs 50 because it is produced in pairs

```
Liquipedia Zergling : 25 minerals per Zergling, "spawn in pairs from a single Larva"
units.dat           : 50 minerals for the producing unit
build time          : 420 frames (17.5 s), Liquipedia rounds to 18 s
```

One larva produces **two** Zerglings for 50 minerals, so 50 is the cost of one
production cycle, not of one unit. Data and game agree.

**Rule for future tests:** for units produced in multiples, `units.dat` holds
the cost of the **cycle**. Do not "fix" such numbers to the per-unit figure.

---

## D0. VERIFIED values that now agree with the data

Measured from `units.dat` and cross-checked against Liquipedia. All five rows
agree, which is the strongest evidence so far that the loader is not shifted:

| unit | hp | shield_points | has_shield | armor | Liquipedia |
|---|---|---|---|---|---|
| Nexus | 750 | 750 | 1 | 1 | 750 hp + 750 shield ([Nexus](https://liquipedia.net/starcraft/Nexus)) |
| Zealot | 100 | 60 | 1 | 1 | 100 + 60, armor 1 ([Zealot](https://liquipedia.net/starcraft/Zealot)) |
| Marine | 40 | (100, unused) | 0 | 0 | 40 hp, no shield ([Marine](https://liquipedia.net/starcraft/Marine)) |
| Zergling | 35 | (100, unused) | 0 | 0 | 35 hp, no shield ([Zergling](https://liquipedia.net/starcraft/Zergling)) |
| Hatchery | 1250 | (100, unused) | 0 | 1 | 1250 hp ([Hatchery](https://liquipedia.net/starcraft/Hatchery)) |

Note on Liquipedia's "Defense: 1250 1": the trailing number is **armor**, not a
shield. Only Protoss rows carry a real shield value.

---

## C2. VERIFIED: Liquipedia facts used by the L-series tests

`tests/liquipedia_facts_test.cpp` asserts only values confirmed on these pages:

| unit | fact | page |
|---|---|---|
| Probe | 20 hp, 20 shields, 50 min | [Probe](https://liquipedia.net/starcraft/Probe) |
| SCV | 60 hp, no shields, 50 min | [SCV](https://liquipedia.net/starcraft/SCV) |
| Drone | 40 hp, no shields, 50 min | [Drone](https://liquipedia.net/starcraft/Drone) |
| Pylon | 300 hp, 300 shields, 100 min | [Pylon](https://liquipedia.net/starcraft/Pylon) |
| Gateway | 500 hp, 500 shields, 150 min, armor 1 | [Gateway](https://liquipedia.net/starcraft/Gateway) |
| Nexus | 750 hp + 750 shields = 1500, 400 min | [Nexus](https://liquipedia.net/starcraft/Nexus) |
| Hatchery | 1250 hp, no shields, 300 min, armor 1 | [Hatchery](https://liquipedia.net/starcraft/Hatchery) |

All seven agree with `units.dat`, which strengthens the earlier conclusion that
the loader is not field-shifted.

---

## C3. VERIFIED for the C-series (combat rules)

| fact | value | source |
|---|---|---|
| Marine ground/air damage | 6, normal | [Marine](https://liquipedia.net/starcraft/Marine) |
| Marine supply | 1 | [Marine](https://liquipedia.net/starcraft/Marine) |
| Zealot damage | 8x2, normal, small unit | [Zealot](https://liquipedia.net/starcraft/Zealot) |
| Dragoon damage | 20, explosive, large unit | [Dragoon](https://liquipedia.net/starcraft/Dragoon) |
| Zergling damage | 5, normal, small unit | [Zergling](https://liquipedia.net/starcraft/Zergling) |
| Concussive multipliers | 100/50/25 vs small/medium/large | [Damage Type](https://liquipedia.net/starcraft/Damage_Type) |
| Explosive multipliers | 50/75/100 vs small/medium/large | [Damage Type](https://liquipedia.net/starcraft/Damage_Type) |
| Supply Depot | 500 hp, 100 min, +8 supply, armor 1 | [Supply Depot](https://liquipedia.net/starcraft/Supply_Depot) |

### Cleared up: "8x2" is not two bullets

Liquipedia writes the Zealot attack as `8x2`, but `units.dat` has damage 8 with
`bullet_count == 1`. The `x2` is the two-swing animation, not two projectiles.
C06 asserts the data (`8 damage, 1 bullet`) and records this here so the number
is not "fixed" later.

### Unresolved: unit_size numbering

`unit_size` is **1 = small, 2 = medium, 3 = large, 0 = special** (measured:
Marine/Zealot/Zergling = 1, Dragoon/Nexus = 3). The explosive damage branch
tests 0/1/2 and the concussive branch tests 0/2/3 - they cannot both be right.
See _AI/BUGS.md BUG-9. **Not resolved**: which numbering StarCraft's own engine
uses is not documented on Liquipedia.

---

## C4. VERIFIED for the U-series (upgrades)

Source: [Liquipedia: Upgrades](https://liquipedia.net/starcraft/Upgrades)

| fact | value | test |
|---|---|---|
| numerical upgrades have 3 tiers | 3 | U11, U13 |
| Protoss Ground Weapons L1 | 100 minerals / 100 gas | U09, U10 |
| Protoss Ground Armor L1 | 100 minerals | U12 |
| Protoss Plasma Shields L1 | 200 minerals | U14 |
| Terran Infantry Weapons L1 | 100 / 100 | U15, U16 |
| Zerg Carapace L1 | 150 / 150 | U17, U18 |
| armor upgrades cost >= attack upgrades | true | U19 |

### Clarified: not every upgrade has three levels

Measured histogram over the 61 upgrade entries in `upgrades.dat`:

```
max_level 0 : 11   (unused slots)
max_level 1 : 34   (single-level abilities, e.g. Stim Pack)
max_level 3 : 16   (numerical upgrades: weapons, armor, shields, carapace)
```

"Most upgrades have 3 levels" is false and U04 was corrected. What *is* true and
verified: the **numerical** upgrades have exactly 3 tiers, and those are the ones
Liquipedia describes as three-tier.

### Note: unused slots are not upgrades

The 11 entries with `max_level == 0` are empty placeholders. They also have a
zero research time and race id 3 ("none"), which is why U29 now asserts "every
**real** upgrade has a research time" instead of "every row".

---

## C5. VERIFIED for the V-series (sight and attack geometry)

| fact | value | source / test |
|---|---|---|
| Zealot sight | 7 tiles | [Zealot](https://liquipedia.net/starcraft/Zealot) / V01 |
| Dragoon sight | 8 tiles | [Dragoon](https://liquipedia.net/starcraft/Dragoon) / V02 |
| Marine sight | 7 tiles | [Marine](https://liquipedia.net/starcraft/Marine) / V03 |
| Zealot attack range | 1 tile (melee) | [Zealot](https://liquipedia.net/starcraft/Zealot) / V16 |
| Dragoon attack range | 4 tiles (128 px) | [Dragoon](https://liquipedia.net/starcraft/Dragoon) / V17 |
| Marine attack range | 4 tiles (128 px) | [Marine](https://liquipedia.net/starcraft/Marine) / V18 |
| Zealot cooldown | 0.924 s ~ 22 frames | [Zealot](https://liquipedia.net/starcraft/Zealot) / V21 |
| Dragoon cooldown | 1.26 s ~ 30 frames | [Dragoon](https://liquipedia.net/starcraft/Dragoon) / V22 |
| Marine cooldown | 0.61 s ~ 15 frames | [Marine](https://liquipedia.net/starcraft/Marine) / V23 |

**Engine units:** `sight_range` is in **tiles**, `weapon max_range` is in
**pixels** (1 tile = 32 px), and `cooldown` is in **frames** at 24 fps. Mixing
these three is the most common way to write a nonsense assertion; every V test
names the unit it uses.

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
| Trigger tick rate | I063, I064 | Triggers are evaluated every 30 frames (engine constant). Confirm this matches StarCraft's "trigger evaluation period". **Not verified.** |
| Shield regeneration | none yet | Protoss shields regenerate (~7 hp/s after a delay). No test covers it; when one is added it needs a source. |
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
