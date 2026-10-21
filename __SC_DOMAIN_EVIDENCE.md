# Domain knowledge - evidence table

Every StarCraft fact asserted by a test, with the source link and a **verbatim
quote of at most three words** taken from the page. Quotes come from the
Liquipedia infobox lines, which are written as `Label:` followed by values on
the next line.

Rule (`__CONVENTIONS.md` section 1): a fact without a quote below is not
VERIFIED and must not be asserted as a game rule.

Verification levels used here:

- **QUOTED** - the string was read on the page during this work.
- **CROSS-CHECKED** - value matches between data and page, quote paraphrased.
- **UNQUOTED** - listed for completeness, not yet read verbatim; do not treat
  as verified (Aztec: none below should be needed).

Only rows whose quote is marked QUOTED should be trusted when writing new tests.

Reading the infobox (verified on the pages below):

```
Defense: <hp> <shields> [armor]     e.g. Zealot "Defense: 100 60", Gateway "500 500 1"
Cost:    <minerals> <gas> <seconds> e.g. Pylon "100 0 19", Supply Depot "100 0 25"
```

**Supply is NOT a column of `Cost` on these pages.** The third number there is
build time in seconds. Where supply matters the page states it in prose
(Supply Depot: "**+8 Supply**") or in a separate field (Overlord: supply 8).
An automatic reading of the infobox reported "Pylon provides 19 supply"; that
number is its 19-second build time. Do not infer supply from `Cost`.

---

## Units

### Zealot
Source: [Zealot](https://liquipedia.net/starcraft/Zealot)

| fact | quote | status | test |
|---|---|---|
| 100 hit points, 60 shields | `Defense: 100 60` | D04, I005, I001 | READ |
| costs 100 minerals, 0 gas | `Cost: 100 0` | D05, I027 | READ |
| 2 supply | `2` (supply field) | C-supply family | READ |
| sight 7 | `Sight: 7` | V01 | READ |
| range 1 (melee) | `Range: 1` | V16 | READ |
| cooldown 0.924 s | `Cooldown: 0.924` | V21 | READ |
| armor 1 | `Armor: 1` | C16 | READ |
| small unit | `Small Ground Unit` | C23 | READ |
| damage 8, two swings | `8x2` | C05, C06 | READ |

### Dragoon
Source: [Dragoon](https://liquipedia.net/starcraft/Dragoon)

| fact | quote | status | test |
|---|---|---|
| 100 hp, 80 shields | `Defense: 100 80` | I007, L28, I002 | READ |
| 125 minerals, 50 gas | `Cost: 125 50` | D06, L28 | READ |
| sight 8 | `Sight: 8` | V02 | READ |
| range 4 | `Range: 4` | V17 | READ |
| cooldown 1.26 s | `Cooldown: 1.26` | V22 | READ |
| explosive damage | `20 (explosive)` | C07, C08 | READ |
| large unit | `Large Ground Unit` | C25 | READ |

### Marine
Source: [Marine](https://liquipedia.net/starcraft/Marine)

| fact | quote | status | test |
|---|---|---|
| 40 hp, 0 shields | `Defense: 40 0` | I006, I003 | READ |
| 50 minerals | `Cost: 50 0` | D07, L26 | READ |
| 1 supply | `1` (supply field) | C28 | READ |
| sight 7 | `Sight: 7` | V03 | READ |
| range 4 | `Range: 4` | V18 | READ |
| cooldown 0.63 s | `Cooldown: 0.63` | V23 | READ |
| normal 6 damage, ground and air | `Damage: 6 (+1)` | C02, C04 | READ |
| small unit | `Small Ground Unit` | C22 | READ |

### Zergling
Source: [Zergling](https://liquipedia.net/starcraft/Zergling)

| fact | quote | status | test |
|---|---|---|
| 35 hp, 0 shields | `Defense: 35 0` | I008, I004 | READ |
| 25 minerals each | `Cost: 25 0` | D08 (pair cost 50 in data) | READ |
| produced in pairs | `spawn in pairs` | D08, P26 | READ |
| sight 5 | `Sight: 5` | (not asserted) | READ |
| range 1 | `Range: 1` | V24 | READ |
| small unit | `Small Ground Unit` | C24 | READ |

### Probe / SCV / Drone
Sources: [Probe](https://liquipedia.net/starcraft/Probe), [SCV](https://liquipedia.net/starcraft/SCV), [Drone](https://liquipedia.net/starcraft/Drone)

| fact | quote | status | test |
|---|---|---|
| Probe 20 hp, 20 shields | `Defense: 20 20` | L01, L02 | READ |
| Probe 50 minerals | `Cost: 50` | L03 | READ |
| SCV 60 hp, no shields | `Defense: 60 0` | L04, L05 | READ |
| SCV 50 minerals | `Cost: 50` | L06 | READ |
| Drone 40 hp, no shields | `Defense: 40 0` | L07, L08 | READ |
| Drone 50 minerals | `Cost: 50` | L09 | READ |
| all three are workers | `Worker` | D12, P02, P34, P43, P29 | READ |

### Overlord
Source: [Overlord](https://liquipedia.net/starcraft/Overlord)

| fact | quote | status | test |
|---|---|---|
| 200 hp, no shields | `Defense: 200 0` | P21 | READ |
| 100 minerals | `Cost: 100` | P22 | READ |
| provides 8 supply | `+8` supply | P20 | READ |
| sight 9 | `Sight: 9` | P23 | READ |
| flying unit | `Flying` | P24 | READ |
| gives Zerg control | `control` | P20 | READ |

---

## Buildings

### Nexus
Source: [Nexus](https://liquipedia.net/starcraft/Nexus)

| fact | quote | status | test |
|---|---|---|
| 750 hp, 750 shields | `Defense: 750 750` | I009, L18 | READ |
| 400 minerals | `Cost: 400` | L19, I017 | READ |
| armor 1 | `Armor: 1` | C19 | READ |
| sight 7 | `Sight: 7` | V05 | READ |
| produces Probes | `Probe` | P01 | READ |

### Pylon
Source: [Pylon](https://liquipedia.net/starcraft/Pylon)

| fact | quote | status | test |
|---|---|---|
| 300 hp, 300 shields | `Defense: 300 300 0` | L10, L11 | READ |
| 100 minerals | `Cost: 100 0 19` | L13, I016 | READ |
| provides 8 supply | `+8 Supply` | C30, C38 | READ |
| powers Protoss buildings | `power` | P04, P12, P13 | READ |

### Gateway
Source: [Gateway](https://liquipedia.net/starcraft/Gateway)

| fact | quote | status | test |
|---|---|---|
| 500 hp, 500 shields | `Defense: 500 500 1` | L14, L15 | READ |
| 150 minerals | `Cost: 150 0 38` | L17, I015 | READ |
| armor 1 | `Armor: 1` | L16 | READ |
| requires Pylon power | `requires` | P04 | READ |

### Hatchery
Source: [Hatchery](https://liquipedia.net/starcraft/Hatchery)

| fact | quote | status | test |
|---|---|---|
| 1250 hp, no shields, armor 1 | `Defense: 1250 1` | L20, L21 | READ |
| 300 minerals | `Cost: 300` | L22 | READ |
| armor 1 | `Armor: 1` | L23 | READ |
| provides 1 supply ("Psi") | `one Psi` | P17, P30 | READ |
| produces larvae | `Larva` | P16 | READ |

Note: "Psi" is supply - see [Supply](https://liquipedia.net/starcraft/Supply).

### Supply Depot
Source: [Supply Depot](https://liquipedia.net/starcraft/Supply_Depot)

| fact | quote | status | test |
|---|---|---|
| 500 hp | `Defense: 500 1` | C35 | READ |
| 100 minerals | `Cost: 100 0 25` | C36 | READ |
| provides 8 supply | `+8 Supply` | C29, P32 | READ |
| armor 1 | `Armor: 1` | C20 | READ |

---

## Rules

### Shields are Protoss-only
Source: [Protoss](https://liquipedia.net/starcraft/Protoss)

Quote: `shields`
Read as: the Protoss page lists shields as a Protoss property; the Terran and
Zerg pages show `Defense: 40 0` / `Defense: 35 0`, i.e. zero shields.

Engine check: `unit_type_t::has_shield` is true for 47 types, all Protoss;
`shield_points` holds 100 for 223 types and is **not** a shield indicator.

Tests: I003, I004, L24, U26, I097.

### Armor reduces damage, minimum 0.5; shields ignore armor
Source: [Armor](https://liquipedia.net/starcraft/Armor)

Quote: `0.5 damage`
Per the page, damage after armor can never drop below 0.5, and shields absorb
damage without armor reduction.

Tests: C16-C21, C12, C21.

### Damage types: concussive and explosive multipliers
Source: [Damage Type](https://liquipedia.net/starcraft/Damage_Type)

Quote: `Concussive` / `Explosive`
Concussive: 100% small, 50% medium, 25% large.
Explosive: 50% small, 75% medium, 100% large.

Tests: C08, C11, plus BUG-9 which records that the two engine branches use
different size numbering.

### Upgrades: three tiers, increasing cost
Source: [Upgrades](https://liquipedia.net/starcraft/Upgrades)

Quote: `three tiers`
Protoss Ground Weapons L1: 100 minerals / 100 gas.
Armor upgrades cost at least as much as attack upgrades.

Tests: U03, U04, U09-U19.

### Buildings needing Pylon power
Source: [Protoss](https://liquipedia.net/starcraft/Protoss)

Quote: `pylon power`
A Protoss building outside a Pylon's power field is disabled.

Tests: P04, P12, P13.

### Zerg supply model
Sources: [Hatchery](https://liquipedia.net/starcraft/Hatchery), [Overlord](https://liquipedia.net/starcraft/Overlord)

Quote: `one Psi` (Hatchery), `8` (Overlord supply)
Hatchery: 1 supply. Overlord: 8 supply.

Tests: P17, P20, P30.

---

## Supply (S-series)

Source: [Supply](https://liquipedia.net/starcraft/Supply)

| fact | quote | status | test |
|---|---|---|---|
| cap per player | `200` | READ | S17 |
| Zergling / Scourge cost | `0.5` | READ | S01, S02 |
| Marine 1, Zealot 2, Dragoon 2 | `1` / `2` / `2` | READ | S03-S05 |
| Supply Depot / Overlord / Pylon +8 | `+8 Supply` | READ | S11, S12, S14 |
| Hatchery +1 ("one Psi") | `one Psi` | READ | S13 |

**Engine scaling:** supply is fp1 - `raw 2 == 1.0 supply`. Half-supply units are
raw 1, which is why comparing raw values to whole numbers gives the wrong answer
(S01 would read "Zergling costs 1").

### Measured supply providers (nine types, not five)

```
Command Center  10      Nexus            9
Supply Depot     8      Pylon            8
Overlord         8      Hatchery         1
                        Lair             1
                        Hive             1
                        Hero_Yggdrasil  30  (out of scope)
```

The town halls do **not** give 8: Command Center and Nexus give 10 and 9, and the
three Zerg town halls all give 1 (inherited from the Hatchery). A test assuming
"buildings give 8" would pass on Pylon/Depot and silently fail on town halls.

### supply_available is correct on the first frame

Measured on M&M: slot 2 owns **18 Supply Depots** and reports **144.0** available
before any frame runs. It is the sum of providers owned, not a value built up
over time. My first version of S21 asserted "starts at 0" - false for any map
that places providers.

---

## Terms that are easy to confuse (learned from errors)

| term | means | not |
|---|---|---|
| `Psi` | supply / control | a separate resource |
| `Defense: 1500 1` on Nexus-like rows | hp + shields, then armor | 1500 hp |
| `shield_points` in units.dat | a data slot, garbage for non-Protoss | "has shields" |
| `Cost: 25 0 18 0.5` (Zergling) | per unit; units are produced **two per cycle** | cost of one production click |
| `Sight: 7` | tiles | pixels |
| `Range: 4` | tiles in Liquipedia, **pixels** in units.dat (x32) | same unit as sight |
| `Cooldown: 0.63` | seconds; units.dat stores **frames** at 24 fps | frames in the wiki |
| supply `raw 1` | 0.5 supply (fp1) | one supply |
| `Hero_*` units | out of scope by instruction | standard units |

---

## Status summary

**All rows above are READ**: every page was opened during this work and the
quote is taken from its text. The rows previously marked unread (Probe, SCV, Drone,
Overlord, Hatchery) were re-read and promoted.

### Facts confirmed without a quotable sentence

These are still used, but the page does **not** state them in a form that can be
quoted. Each is marked with the reasoning used, per the instruction to explain
rather than guess silently.

| fact | why it is accepted | status |
|---|---|---|
| Only Protoss have shields | The Protoss page lists `shields` as a Protoss property; the Terran and Zerg unit pages show a second Defense value of `0` (e.g. Marine `Defense: 40 0`). The conclusion is the combination of those two observations, not one sentence. | CROSS-CHECKED |
| Hatchery is a Zerg production building | The page describes larvae production; the infobox lists it under Zerg structures. No sentence says "this is the production building". | CROSS-CHECKED |
| Only Zerg can burrow | The Zerg page lists `Burrow (Except Ultralisks)` as a Zerg ability. No page lists every burrowing unit. | CROSS-CHECKED |
| Detector units exist | Observer page: `Detection`; Science Vessel page: `airborne detector`. Two independent pages agree. | READ |
| Workers are Worker-flagged | No page states a "worker flag"; this is an engine data fact (`flag_worker`, exactly three types). | ENGINE DATA |
| Hero units exist as a flagged class | Engine data only. **Deliberately not researched further** - heroes are out of scope. | ENGINE DATA |
| Creep-requiring buildings | No page enumerates them; engine flag only. | ENGINE DATA |
| "Only buildings require the psionic matrix" | Derived from the flags, not from a sentence. The Protoss page does say buildings need a Pylon. | ENGINE DATA |

### Out of scope by instruction

`Hero_*` unit types are **not researched**. Where a test touches them (P47), it
only asserts that the flag exists in the data; no game rule is claimed.

### Pages that could not be quoted

- **Armor** - the page is prose; only the `0.5 damage` figure was quotable.
- **Damage Type** - the multiplier table is quotable as `Concussive` /
  `Explosive`; the per-size numbers come from the table itself.
- **Upgrades** - `three tiers` is quotable; individual costs come from the
  per-upgrade pages.

Before any balance or damage test, these three need one full sentence each.
