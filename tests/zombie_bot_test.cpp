// Unit tests for the zombie bot policy - no map, no match, no engine loop.
//
// The policy is pure data-in / data-out (see bots/zombie_bot.h), so these tests
// build tiny unit_t objects and check the chosen target directly. That is the
// bulk of the testing: the rules are where bugs hide, and they are cheap to
// exercise here.
//
// Rules under test:
//   melee  -> nearest enemy
//   ranged -> most wounded IN RANGE, else nearest
//   ties   -> distance, then unit index (determinism, never container order)
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/zombie_bot_test.cpp -o build/zombie_bot_test -lz -lbz2
// Run: ./build/zombie_bot_test          (does not need UMS_TEST_DATA_DIR)
//
// Exit codes: 0 all passed, 1 a check failed.
#include "bots/zombie_bot.h"

#include <cstdarg>
#include <cstdio>
#include <memory>
#include <vector>

using namespace zombie_bot;
using bwgame::fp8;
using bwgame::unit_t;
using bwgame::unit_type_t;
using bwgame::weapon_type_t;
using bwgame::xy;

static int failures = 0;
static int total = 0;

static void test(const char* id, const char* name, bool ok, const char* detail = "") {
	++total;
	if (ok) printf("ok   %s %s\n", id, name);
	else {
		printf("FAIL %s %s%s%s\n", id, name, *detail ? ": " : "", detail);
		++failures;
	}
}

// A test fixture owns the unit types and units it creates, so nothing dangles.
struct world {
	std::vector<std::unique_ptr<unit_type_t>> types;
	std::vector<std::unique_ptr<weapon_type_t>> weapons;
	std::vector<std::unique_ptr<unit_t>> units;

	unit_type_t* type(int range) {
		auto t = std::make_unique<unit_type_t>();
		t->id = bwgame::UnitTypes::Protoss_Zealot;
		weapons.push_back(std::make_unique<weapon_type_t>());
		weapon_type_t* w = weapons.back().get();
		w->max_range = range;
		w->damage_amount = 5;
		t->ground_weapon = w;
		// Max hp matters for the kiting rules: is_wounded compares against it and
		// returns false when it is 0 (a real type always has hp). The first
		// version of the fixture forgot this, so every "wounded" test was
		// measuring a division by zero guard instead of the rule.
		t->hitpoints = fp8::integer(100);
		types.push_back(std::move(t));
		return types.back().get();
	}

	// hp/shields in whole points; stored as fp8 like the real engine.
	unit_t* unit(unit_type_t* t, int x, int hp, int shields, int index) {
		auto u = std::make_unique<unit_t>();
		u->unit_type = t;
		u->sprite = nullptr;   // policy never dereferences the sprite
		u->hp = fp8::integer(hp);
		u->shield_points = fp8::integer(shields);
		u->index = index;
		// position lives on the sprite in the engine; policy uses the candidate's
		// precomputed distance, so no sprite is needed here.
		(void)x;
		units.push_back(std::move(u));
		return units.back().get();
	}
};

int main() {
	world w;
	unit_type_t* melee = w.type(15);    // < one tile
	unit_type_t* ranged = w.type(128);  // 4 tiles

	// ---- melee: nearest wins ------------------------------------------------
	{
		unit_t* self = w.unit(melee, 0, 100, 0, 1);
		(void)self;
		std::vector<candidate> c;
		c.push_back({w.unit(melee, 0, 100, 0, 10), 100, 100});   // dist 10
		c.push_back({w.unit(melee, 0, 100, 0, 11), 25, 100});    // dist 5
		c.push_back({w.unit(melee, 0, 100, 0, 12), 900, 100});   // dist 30
		const unit_t* t = select_melee_target(c);
		test("Z01", "melee picks the nearest enemy", t && t->index == 11,
			t ? "" : "null");
	}

	// ---- melee ignores hp: a wounded far unit is not preferred --------------
	{
		std::vector<candidate> c;
		c.push_back({w.unit(melee, 0, 5, 0, 20), 4, 5});     // very wounded, far
		c.push_back({w.unit(melee, 0, 100, 0, 21), 1, 100});  // healthy, near
		const unit_t* t = select_melee_target(c);
		test("Z02", "melee ignores wounds, takes the nearest", t && t->index == 21);
	}

	// ---- ranged: most wounded in range -------------------------------------
	{
		std::vector<candidate> in_range;
		in_range.push_back({w.unit(ranged, 0, 80, 20, 30), 25, 100});
		in_range.push_back({w.unit(ranged, 0, 10, 0, 31), 100, 10});   // wounded
		in_range.push_back({w.unit(ranged, 0, 40, 40, 32), 4, 80});
		std::vector<candidate> out_of_range;
		out_of_range.push_back({w.unit(ranged, 0, 1, 0, 33), 900, 1}); // most wounded overall
		const unit_t* t = select_ranged_target(in_range, out_of_range);
		test("Z03", "ranged picks the most wounded IN RANGE", t && t->index == 31);
	}

	// ---- ranged: nothing in range -> nearest --------------------------------
	{
		std::vector<candidate> out;
	// distance_sq is squared pixels: 400 (20 px) is nearer than 1600 (40 px).
	// The wounded unit is deliberately FARTHER; the fallback must ignore wounds
	// and take the nearest, index 40. The first version had the numbers swapped
	// and asserted the opposite of its own comment.
		out.push_back({w.unit(ranged, 0, 100, 0, 40), 400, 100});    // nearer
		out.push_back({w.unit(ranged, 0, 20, 0, 41), 1600, 20});     // wounded, farther
		const unit_t* t = select_ranged_target({}, out);
		test("Z04", "ranged with empty range takes the nearest, not the wounded", t && t->index == 40);
	}

	// ---- hp+shields is the wound metric ------------------------------------
	{
		std::vector<candidate> c;
		// 50 hp + 0 shield = 50 total vs 30 hp + 10 = 40 total: the second is worse off
		c.push_back({w.unit(ranged, 0, 50, 0, 50), 10, 50});
		c.push_back({w.unit(ranged, 0, 30, 10, 51), 20, 40});
		const unit_t* t = select_ranged_target(c, {});
		test("Z05", "wound metric is hp + shields", t && t->index == 51);
	}

	// ---- determinism: equal hp and distance -> lower index ----------------
	{
		std::vector<candidate> c;
		c.push_back({w.unit(ranged, 0, 30, 0, 61), 16, 30});
		c.push_back({w.unit(ranged, 0, 30, 0, 60), 16, 30});
		const unit_t* t = select_ranged_target(c, {});
		test("Z06", "ties resolve to the lower unit index", t && t->index == 60);
	}

	// ---- determinism: same input twice gives the same answer ---------------
	{
		std::vector<candidate> c;
		c.push_back({w.unit(ranged, 0, 10, 0, 70), 9, 10});
		c.push_back({w.unit(ranged, 0, 10, 0, 71), 9, 10});
		const unit_t* a = select_ranged_target(c, {});
		const unit_t* b = select_ranged_target(c, {});
		test("Z07", "selection is deterministic across calls", a == b);
	}

	// ---- empty input -------------------------------------------------------
	{
		test("Z08", "no candidates -> no target", select_melee_target({}) == nullptr);
		test("Z09", "no ranged and no fallback -> no target",
			select_ranged_target({}, {}) == nullptr);
	}

	// ---- null safety -------------------------------------------------------
	{
		std::vector<candidate> c;
		c.push_back({nullptr, 0, 0});
		c.push_back({w.unit(melee, 0, 100, 0, 80), 5, 100});
		const unit_t* t = select_melee_target(c);
		test("Z10", "null candidates are skipped", t && t->index == 80);
	}

	// ---- melee/ranged classification --------------------------------------
	{
		unit_t* m = w.unit(melee, 0, 100, 0, 90);
		unit_t* r = w.unit(ranged, 0, 100, 0, 91);
		test("Z11", "range 15 is melee", is_melee(m));
		test("Z12", "range 128 is ranged", !is_melee(r));
	}

	// ---- distance helper ---------------------------------------------------
	{
		test("Z13", "xy_distance_sq is symmetric and zero for same point",
			xy_distance_sq({10, 20}, {10, 20}) == 0
			&& xy_distance_sq({0, 0}, {3, 4}) == 25
			&& xy_distance_sq({3, 4}, {0, 0}) == 25);
	}

	// ---- hp total ----------------------------------------------------------
	{
		unit_t* u = w.unit(melee, 0, 12, 34, 92);
		test("Z14", "hp_total adds hp and shields", unit_hp_total(u) == 46);
	}

	// ======================= new behaviours (B1-B3) =======================

	// ---- B1: focus fire - the group converges on one target ---------------
	{
		std::vector<candidate> in_range;
		in_range.push_back({w.unit(ranged, 0, 50, 0, 100), 10, 50});
		in_range.push_back({w.unit(ranged, 0, 10, 0, 101), 90, 10});   // most wounded
		const unit_t* focus = w.units[0].get();   // any existing unit as a probe
		(void)focus;
		// The group picks the wounded one...
		const unit_t* g = select_group_ranged_target(in_range, {});
		test("Z15", "group target is the most wounded in range", g && g->index == 101);
		// ...and every unit that is told to focus it returns exactly that pointer.
		const unit_t* u1 = select_ranged_target(in_range, {}, g);
		const unit_t* u2 = select_ranged_target(in_range, {}, g);
		test("Z16", "focus target is honoured over local choice", u1 == g && u2 == g);
	}

	// ---- B1: focus overrides a unit's own wounded preference --------------
	{
		std::vector<candidate> in_range;
		in_range.push_back({w.unit(ranged, 0, 40, 0, 110), 5, 40});
		in_range.push_back({w.unit(ranged, 0, 8, 0, 111), 5, 8});    // this unit would prefer 111
		const unit_t* focus = in_range[0].unit;                       // but the group says 110
		const unit_t* t = select_ranged_target(in_range, {}, focus);
		test("Z17", "unit follows the group target, not its own best", t == focus);
	}

	// ---- B1: a focus target out of range is still chased ------------------
	{
		std::vector<candidate> in_range, out;
		in_range.push_back({w.unit(ranged, 0, 100, 0, 120), 5, 100});
		out.push_back({w.unit(ranged, 0, 20, 0, 121), 900, 20});
		const unit_t* focus = out[0].unit;   // the group decided on the far unit
		const unit_t* t = select_ranged_target(in_range, out, focus);
		test("Z18", "focused target is chased even from out of range", t == focus);
	}

	// ---- B1: a dead focus target falls back to normal choice -------------
	{
		std::vector<candidate> in_range;
		in_range.push_back({w.unit(ranged, 0, 60, 0, 130), 5, 60});

		unit_t dangling{};   // not in any candidate list: simulates a dead target
		dangling.index = 999;
		const unit_t* t = select_ranged_target(in_range, {}, &dangling);
		test("Z19", "unknown focus pointer falls back to local choice", t && t->index == 130);
	}

	// ---- B2: kiting rules -------------------------------------------------
	{
		unit_t* r = w.unit(ranged, 0, 20, 0, 140);   // 20 of 100 hp = wounded
		r->ground_weapon_cooldown = 20;
		test("Z20", "wounded ranged unit kites while reloading", should_kite(r));
		r->ground_weapon_cooldown = 10;
		test("Z21", "kiting stops at cooldown 9 (10 -> still kites)", should_kite(r));
		r->ground_weapon_cooldown = 9;
		test("Z22", "kiting stops at cooldown 9 exactly", !should_kite(r));
		r->ground_weapon_cooldown = 0;
		test("Z23", "a loaded weapon never kites", !should_kite(r));
	}
	{
		unit_t* r = w.unit(ranged, 0, 90, 0, 141);   // healthy
		r->ground_weapon_cooldown = 20;
		test("Z24", "healthy ranged units never kite", !should_kite(r));
	}
	{
		unit_t* m = w.unit(melee, 0, 5, 0, 142);     // badly wounded melee
		m->ground_weapon_cooldown = 20;
		test("Z25", "melee units never kite even when wounded", !should_kite(m));
	}
	{
		// harness-measured: 60% is not wounded, 40% is
		unit_t* a = w.unit(ranged, 0, 60, 0, 143);
		unit_t* b = w.unit(ranged, 0, 40, 0, 144);
		test("Z26", "wounded threshold is below 50% of hp+shields", !is_wounded(a) && is_wounded(b));
		unit_t* c = w.unit(ranged, 0, 40, 40, 145);   // 80 of 100 total
		test("Z27", "shields count towards not being wounded", !is_wounded(c));
	}

	// ---- B2: kite direction is away from the threat ----------------------
	{
		const xy pos = kite_position({100, 100}, {50, 100}, 40);
		test("Z28", "kite step moves away from the threat", pos.x == 140 && pos.y == 100);
		const xy same = kite_position({100, 100}, {100, 100}, 40);
		test("Z29", "kite from the same point still yields a step", same.x == 140);
		const xy diag = kite_position({100, 100}, {100, 60}, 40);
		test("Z30", "kite step is clamped to the requested distance",
			diag.x == 100 && diag.y == 140);
	}

	// ---- no retreat: nothing in the policy ever reduces aggression --------
	{
		// A badly outnumbered, badly wounded unit still gets a target; there is
		// deliberately no "flee" branch (B4: a zombie always attacks).
		std::vector<candidate> many;
		for (int i = 0; i != 10; ++i) many.push_back({w.unit(melee, 0, 100, 0, 150 + i), 1 + i, 100});
		const unit_t* t = select_melee_target(many);
		test("Z31", "no retreat: outnumbered units still attack", t != nullptr);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
