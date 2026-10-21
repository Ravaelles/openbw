# Non-obvious engine behaviour

Things this engine does that surprise you on first contact. Not defects - they
cost time if unknown. Defects live in `_AI/BUGS.md`; domain assumptions and
their verification live in `__SC_DOMAIN_QUESTIONS.md`; working rules live in
`_AI/CONVENTIONS.md`.

Everything below was measured on the reference maps (`1Drag_v_1Zeal.scm`,
`M&M_v_ZealDrag.scx`) unless stated otherwise.

---

### N01 - Map revealers are real units, in a third list

`Special_Map_Revealer` (id 101) units are StarEdit helpers. They are ordinary
`UNIT` records, so `unit_counts[owner][101]` counts them, but the engine keeps
them in `state::map_revealer_units`, **not** in `visible_units` and not in
`hidden_units`. Measured: `1Drag` has 109 visible, 0 hidden, **301 revealers**;
`M&M` has 189 visible, 0 hidden, 266 revealers. Any "how many units does this
player have" question must pick the right list, and a slot can report 140
"units" while owning zero army.

### N02 - `unit_counts` includes revealers

Consequence of N01: BWAPI's participating-player test reads `unit_counts`, so a
slot with only revealers looks present. The slot report separates them; the
participation test did not (fixed for revealers only).

### N03 - MRGN has 255 regions, not 256

StarEdit documents 256 locations. `MRGN` in both reference maps is 5100 bytes =
**255** records, and the engine stores 255 (`locations=255`). Location index 256
is therefore always unusable, and a map that expects the last location to exist
silently gets the previous one.

### N04 - The engine loads only Melee.trg's triggers, never the map's

`triggers=6` for `1Drag` and `5` for `M&M` after loading, exactly the Melee.trg
set. The map's own `TRIG` chunk (86400 bytes = 36 records in `1Drag`) is parsed
but the resulting triggers never reach `game_state::triggers`. Reported here for
the record; the trigger tests work against whatever is loaded.

### N05 - Trigger attachment is per playable controller, not per "human"

After the UMS remap a map's "Computer" players are `controller_computer_game`,
not `controller_occupied`. Matching only the latter silently drops **every**
trigger on maps whose featured players are Computer in StarEdit - measured
0 vs 5 running triggers per slot on `1Drag`.

### N06 - A map can end the game on purpose after N seconds

`1Drag` declares `Elapsed time 300 s -> Defeat`. At the trigger tick rate that
is frame 312. "The game stopped" therefore needs a diagnostic before anyone
suspects the engine; see BUG-4.

### N07 - `callOnFrame()` blocks in `read()`

The server's per-frame handshake writes byte 2 and then **blocks** waiting for
byte 1 from the client. Calling it twice in one update sends a second 2 before
the client processed the first, and the match freezes. Exactly one call per
update, always after `updateSharedMemory()`.

### N08 - The client needs the shared segment to stay mapped

Unlinking `/dev/shm/bwapi_shared_memory_<pid>` after accept keeps the server's
mapping alive but makes the object unopenable for the client, which then sees
`playerCount = 0`. That was one of the "no units" reports.

### N09 - `playerCount` and events are only published when a client is attached

`updateSharedMemory()` used to run only in the attached branch, so a client
attaching mid-game started from `playerCount = 0` and an empty event queue.
Events were additionally cleared at the end of every update, so MatchStart
could never be delivered.

### N10 - `hitpoints` is fixed point (fp8)

`hitpoints.raw_value == 25600` means **100** hp. Comparing against 100 directly
is a common mistake; use `integer_part()`.

### N11 - Costs are BroodWar costs

The `units.dat` in the data files is BroodWar: Zergling is **50/0**, not 25/0.
Zealot 100/0, Dragoon 125/50, Marine 50/0 all match.

### N12 - Four building types have no build time

`flag_building` is set on more types than have a `build_time`. Four special
types (the preplaced/special buildings) legitimately have 0. "Every building has
a build time" is a wrong assertion; the test allows up to five exceptions.

### N13 - `supply_used` / `supply_available` are per race

Three entries per player, not one scalar. Reading `[player]` gives an array and
silently compares pointers if you are not careful.

### N14 - `unit_counts` can exceed what exists

Declared counts include units that never get placed (revealers, but also
records that fail placement). The slot report distinguishes "declared",
"visible" and "revealers" for this reason.

### N15 - A "Waiting" state machine has two owners

The wait action sets `trigger_waiting`, but only `process_triggers()` clears it
and advances `trigger_wait_timers`. Testing the action in isolation therefore
cannot exercise the resume path - see BUG-7.

### N17 - Only Protoss have shields, and `shield_points` is garbage for everyone else

In StarCraft **only the Protoss race has shields**; Terran and Zerg have none
([Liquipedia: Protoss](https://liquipedia.net/starcraft/Protoss)).

`unit_type_t::shield_points` does **not** encode that. Measured over 228 types:

```
shield_points > 0   : 223 types   (Terran and Zerg included, value 100)
has_shield == true  :  47 types   (all Protoss)
Marine  : shield_points=100, has_shield=0      <- no shields in the game
Zergling: shield_points=100, has_shield=0      <- no shields in the game
Zealot  : shield_points=60,  has_shield=1
```

So `shield_points > 0` is **wrong** for "has shields" (99 of 228 types would be
misreported). Use `unit_type_t::has_shield`. This correction came from a
review: an earlier version of this file claimed the 100 was "a data slot" and
treating it as shields was merely redundant - that was wrong.

### N18 - `unit_t::hp` and `unit_t::shield_points` are fp8, the type table's are integers

`unit_t::hp.raw_value == 25600` is 100 hp; `unit_type_t::hitpoints` is the same
value in fp8 too, but `unit_type_t::shield_points` is a plain `int`. Comparing
the two requires `integer_part()` on one side. Mixing them silently compares
fixed-point raw values with integer hit points.

### N19 - Group 231 (buildings) and 232 (factories) overlap by design

Twelve unit types match **both** `Men/Buildings/Factory` predicates: 106-116
(Terran factories) and 130-133, 154, 155, 160, 167 (Zerg/Protoss production
buildings). A factory is also a building. Any code that assumes the three group
ids are disjoint will double-count those types; `trigger_death_count()` sums per
type, so it counts each death once, but a filter written as
`if (matches buildings) ... else if (matches factory) ...` silently drops them.

### N20 - `MRGN` is 255 entries with mostly empty regions

The chunk is fixed-size: 255 records. Only the first N are real; the rest are
zeroed `(0,0)-(0,0)`. Measured on `1Drag`: **63 real regions, 192 empty**. The
engine stores all of them, so `locations.size()` is 255 and "region 200" exists
but has zero area. A trigger that references an unused location index silently
matches an empty area.

### N16 - `get_unit(index)` vs `get_unit(unit_id_32)`

The raw overload indexes `units_container` directly; the id overload converts
through `id.index()` and subtracts one. Passing `unit->index` into the id
overload does not round-trip (BUG-3). Always use the raw overload with
`unit->index`.

---

---

### N21 - Native StarCraft AI is only partially supported

A map slot marked `controller_computer` means "the game's built-in AI plays
this player". OpenBW has no such AI. Since BUG-12 the engine no longer aborts:
it skips the AI orders, counts them in `state::skipped_native_ai_orders` and
logs once.

Consequences that matter when reading test results:

- the map **loads** fully and the match runs;
- the AI player is **idle** - no building, no attacks, no defence;
- trigger chains seeded by AI behaviour never fire;
- **check the counter** before calling such a map "working": `0` means no native
  AI was needed, `> 0` means part of the map was not simulated.

Measured on the extreme corpus: 10/10 maps hostable, all relying on the idle
computer player.
