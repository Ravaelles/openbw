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

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
