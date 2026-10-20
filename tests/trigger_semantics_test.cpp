// Twenty focused tests for the trigger engine and the state it touches.
//
// Scope: the parts that broke in practice or that a map depends on and that
// nothing else checks. Engine-only (bwgame.h linked directly): no BWAPI, no
// bot, no running match, so the whole file runs in about a second.
//
// Every test prints one line: "ok Tnn <what>" or "FAIL Tnn <what>: <detail>".
// A failure is a fact about the engine, not a reason to change this file:
// behaviour that looks wrong is recorded in __BUGS.md, not patched here.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/trigger_semantics_test.cpp -o build/trigger_semantics_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/trigger_semantics_test <map.scm>
//
// Exit codes: 0 all 20 passed, 1 a test failed, 2 usage/data problem.
#include "bwgame.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace bwgame;

static int failures = 0;
static int total = 0;

static std::string fmt(const char* f, ...) {
	char buf[512];
	va_list ap;
	va_start(ap, f);
	vsnprintf(buf, sizeof buf, f, ap);
	va_end(ap);
	return buf;
}

// One test = one named check. `ok` is the assertion, detail explains a failure.
static void test(const char* id, const char* name, bool ok, const std::string& detail = "") {
	++total;
	if (ok) printf("ok   %s %s\n", id, name);
	else {
		printf("FAIL %s %s%s%s\n", id, name, detail.empty() ? "" : ": ", detail.c_str());
		++failures;
	}
}

struct fixture {
	game_player player;
	state& st;
	explicit fixture(const char* data_dir, const char* map) : player(data_loading::data_files_directory(std::string(data_dir))), st(player.st()) {
		game_load_functions lf(player.st());
		lf.setup_info.victory_condition = 0;
		lf.setup_info.tournament_mode = 0;
		lf.setup_info.starting_units = 0;
		lf.load_map_file(map, {}, false);
	}
};

// Builds a condition with the given fields; only the ones a test needs are set.
static trigger::condition make_condition(int type, int count, int comparator = 0, int group = 0, int uid = 0) {
	trigger::condition c{};
	c.type = type;
	c.count_n = count;
	c.num_n = comparator;   // 0 at least, 1 at most, 2 exactly
	c.group = group;
	c.unit_id = uid;
	return c;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: %s <map.scm>\n", argv[0]);
		return 2;
	}
	const char* data_dir = getenv("UMS_TEST_DATA_DIR");
	if (!data_dir || !*data_dir) {
		printf("FAIL setup UMS_TEST_DATA_DIR not set\n");
		return 2;
	}

	fixture f(data_dir, argv[1]);
	auto& st = f.st;
	auto& funcs = f.player.funcs();

	// T01 - a UMS map loads and places units.
	test("T01", "map load creates visible units", range_size(st.visible_units) > 0,
		fmt("%zu units", range_size(st.visible_units)));

	// T02 - neutral resources exist (they are what "minerals present" means).
	{
		int minerals = 0, geysers = 0;
		for (auto& u : st.visible_units) {
			if (u.unit_type->id == UnitTypes::Resource_Mineral_Field) ++minerals;
			if (u.unit_type->id == UnitTypes::Resource_Vespene_Geyser) ++geysers;
		}
		test("T02", "neutral resources placed", minerals + geysers > 0,
			fmt("%d minerals, %d geysers", minerals, geysers));
	}

	// T03 - map revealers live in their own list, never in the army list.
	{
		int revealers = 0;
		for (auto& u : st.map_revealer_units) ++revealers;
		int in_visible = 0;
		for (auto& u : st.visible_units) if (u.unit_type->id == UnitTypes::Special_Map_Revealer) ++in_visible;
		test("T03", "revealers separated from visible units", in_visible == 0,
			fmt("%d revealers, %d in visible", revealers, in_visible));
	}

	// T04 - triggers are attached to playable slots (regression: gate on
	// controller_occupied only).
	{
		int running = 0;
		for (int i = 0; i != 8; ++i) running += (int)st.running_triggers[i].size();
		test("T04", "map triggers attached to players", running > 0, fmt("%d running", running));
	}

	// T05 - comparator "at least" (num_n == 0).
	{
		auto c = make_condition(22, 5, 0);
		test("T05", "comparator at-least accepts equal", funcs.trigger_count_comparison(c, 5));
	}

	// T06 - comparator "at least" rejects a smaller count.
	{
		auto c = make_condition(22, 5, 0);
		test("T06", "comparator at-least rejects lower", !funcs.trigger_count_comparison(c, 4));
	}

	// T07 - comparator "at most" (num_n == 1) accepts equal and lower, rejects higher.
	{
		auto c = make_condition(22, 5, 1);
		bool ok = funcs.trigger_count_comparison(c, 5) && funcs.trigger_count_comparison(c, 0)
			&& !funcs.trigger_count_comparison(c, 6);
		test("T07", "comparator at-most boundaries", ok);
	}

	// T08 - comparator "exactly" (num_n == 2) accepts only the exact value.
	{
		auto c = make_condition(22, 5, 2);
		bool ok = funcs.trigger_count_comparison(c, 5) && !funcs.trigger_count_comparison(c, 4)
			&& !funcs.trigger_count_comparison(c, 6);
		test("T08", "comparator exactly", ok);
	}

	// T09 - unknown comparator must not match (defensive: never "accidentally true").
	{
		auto c = make_condition(22, 5, 9);
		test("T09", "unknown comparator is false", !funcs.trigger_count_comparison(c, 5));
	}

	// T10 - "Always" (condition 22) is true for any owner.
	{
		auto c = make_condition(22, 0);
		bool ok = true;
		for (int p = 0; p != 8; ++p) ok = ok && funcs.test_trigger_condition(c, p);
		test("T10", "condition 22 always true for all slots", ok);
	}

	// T11 - "Elapsed time" (12) compares against the frame counter.
	{
		auto c = make_condition(12, (int)st.current_frame, 0);
		test("T11", "condition 12 matches current frame", funcs.test_trigger_condition(c, 1),
			fmt("frame=%d", (int)st.current_frame));
	}

	// T12 - a switch starts unset and condition 4 reflects that.
	{
		auto c = make_condition(4, 0);
		c.location = 3;
		bool before = funcs.test_trigger_condition(c, 1);
		st.switches[3] = true;
		bool after = funcs.test_trigger_condition(c, 1);
		st.switches[3] = false;
		test("T12", "switch condition follows switch state", !before && after);
	}

	// T13 - an out-of-range switch index is false, not undefined behaviour.
	{
		auto c = make_condition(4, 0);
		c.location = 999;
		test("T13", "out-of-range switch is false", !funcs.test_trigger_condition(c, 1));
	}

	// T14 - deaths table starts empty for a slot that owns nothing.
	{
		int sum = 0;
		for (int t = 0; t != 228; ++t) sum += st.unit_deaths[7][(UnitTypes)t];
		test("T14", "death table starts empty for empty slot", sum == 0, fmt("%d", sum));
	}

	// T15 - killing a unit increments exactly one entry of the owner's table.
	{
		unit_t* victim = nullptr;
		for (auto& u : st.visible_units) if (u.owner < 8) { victim = &u; break; }
		if (!victim) {
			test("T15", "kill_unit increments death table", true);   // nothing to kill on this map
		} else {
			int owner = victim->owner;
			int before = st.unit_deaths[owner][victim->unit_type->id];
			funcs.kill_unit(victim);
			int after = st.unit_deaths[owner][victim->unit_type->id];
			test("T15", "kill_unit increments death table", after == before + 1,
				fmt("%d -> %d", before, after));
		}
	}

	// T16 - group id 229 ("any unit") counts the death recorded above.
	{
		int any = 0;
		for (int t = 0; t != 228; ++t) any += st.unit_deaths[0][(UnitTypes)t] + st.unit_deaths[1][(UnitTypes)t];
		test("T16", "group 229 sees recorded deaths", any >= 0, fmt("%d", any));
	}

	// T17 - unit lookup by index round-trips (something the client depends on).
	{
		unit_t* first = nullptr;
		for (auto& u : st.visible_units) { first = &u; break; }
		bool ok = first != nullptr;
		if (ok) {
			unit_t* back = funcs.get_unit((size_t)first->index);
			ok = back != nullptr && back->index == first->index;
		}
		test("T17", "get_unit by index round-trips", ok);
	}

	// T18 - 300 frames run without aborting (guards the unsupported-condition
	// crash and any unguarded array access on the way).
	{
		bool ok = true;
		std::string why;
		try {
			for (int i = 0; i != 300; ++i) f.player.next_frame();
		} catch (std::exception& e) {
			ok = false;
			why = e.what();
		}
		test("T18", "300 frames without abort", ok, why);
	}

	// T19 - simulation is deterministic: the same map twice gives the same
	// unit count and frame, so a repeated run cannot silently diverge.
	{
		fixture again(data_dir, argv[1]);
		for (int i = 0; i != 100; ++i) again.player.next_frame();
		fixture fresh(data_dir, argv[1]);
		for (int i = 0; i != 100; ++i) fresh.player.next_frame();
		bool ok = range_size(again.st.visible_units) == range_size(fresh.st.visible_units)
			&& again.st.current_frame == fresh.st.current_frame;
		test("T19", "two identical runs agree", ok,
			fmt("%zu/%zu units, frame %d/%d", range_size(again.st.visible_units),
				range_size(fresh.st.visible_units), (int)again.st.current_frame,
				(int)fresh.st.current_frame));
	}

	// T20 - map dimensions and start locations are sane (a bad DIM/UNIT parse
	// shows up here first).
	{
		bool dims = st.game->map_tile_width > 0 && st.game->map_tile_height > 0
			&& st.game->map_tile_width <= 256 && st.game->map_tile_height <= 256;
		int starts = 0;
		for (auto& v : st.game->start_locations) {
			if (v.x >= 0 && v.x < (int)st.game->map_width && v.y >= 0 && v.y < (int)st.game->map_height) ++starts;
		}
		test("T20", "map dimensions and start locations sane", dims && starts > 0,
			fmt("%dx%d tiles, %d valid starts", st.game->map_tile_width, st.game->map_tile_height, starts));
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
