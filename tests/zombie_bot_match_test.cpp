// Integration test: run the zombie bot on a real map and judge it by ENGINE
// STATE, not by log lines (see _AI/CONVENTIONS.md section 3).
//
// What it proves:
//   1. the bot can be ticked on a loaded map without throwing;
//   2. it issues orders (units leave their idle order);
//   3. after enough frames, units actually die on at least one side.
//
// It needs a map whose players own armed units. Pass one on the command line;
// with no argument the test exits 2 rather than guessing a map.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/zombie_bot_match_test.cpp -o build/zombie_bot_match_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<mpq dir> ./build/zombie_bot_match_test <map.scm>
//
// Exit codes: 0 passed, 1 failed, 2 usage problem.
#include "bots/zombie_bot.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using bwgame::game_load_functions;
using bwgame::game_player;
using bwgame::unit_t;

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

static int alive_units(bwgame::state& st, int owner) {
	int n = 0;
	for (unit_t& u : st.player_units[owner]) if (u.order_type && u.order_type->id != bwgame::Orders::Die) ++n;
	return n;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: %s <map.scm>\n", argv[0]);
		return 2;
	}
	const char* dir = getenv("UMS_TEST_DATA_DIR");
	if (!dir || !*dir) {
		printf("FAIL setup UMS_TEST_DATA_DIR not set\n");
		return 2;
	}

	game_player player(bwgame::data_loading::data_files_directory(std::string(dir)));
	auto& st = player.st();
	auto& f = player.funcs();
	{
		game_load_functions lf(st);
		lf.setup_info.victory_condition = 0;
		lf.setup_info.tournament_mode = 0;
		lf.setup_info.starting_units = 0;
		lf.load_map_file(argv[1], {}, false);
	}

	// Pick two owners that both have ARMED, MOBILE units. Counting raw units is
	// not enough: 1Drag_v_1Zeal's slot 1 is 139 Special_Map_Revealer helpers,
	// which the bot correctly ignores (they have no weapon), so a plain count
	// would call that map a fight when it has no fighters at all.
	auto armed_units = [&](int owner) {
		int n = 0;
		for (unit_t& u : st.player_units[owner]) {
			if (u.order_type && u.order_type->id == bwgame::Orders::Die) continue;
			if (u.unit_type->flags & bwgame::unit_type_t::flag_building) continue;
			if (!u.unit_type->ground_weapon && !u.unit_type->air_weapon) continue;
			++n;
		}
		return n;
	};
	int a = -1, b = -1;
	for (int o = 0; o != 8; ++o) {
		if (armed_units(o) == 0) continue;
		if (a < 0) a = o;
		else if (b < 0) { b = o; break; }
	}
	test("M01", "map has two players with ARMED units", a >= 0 && b >= 0,
		"need two slots owning mobile armed units to run a fight");

	if (a < 0 || b < 0) {
		printf("\n%d/%d passed (skipped)\n", total - failures, total);
		return 1;
	}

	// Make them enemies explicitly; maps do not always declare it.
	st.alliances[a][b] = 0;
	st.alliances[b][a] = 0;

	const int a0 = alive_units(st, a);
	const int b0 = alive_units(st, b);
	char detail[160];
	snprintf(detail, sizeof detail, "slot %d: %d units, slot %d: %d units", a, a0, b, b0);
	test("M02", "both sides start armed and populated", a0 > 0 && b0 > 0, detail);

	// Tick the bot for both sides and advance the match. Orders are re-issued
	// periodically; 400 frames is long enough for contact on a small map.
	bool acted = false;
	bool threw = false;
	std::string why;
	for (int frame = 0; frame != 400; ++frame) {
		try {
			if (frame % 10 == 0) {
				if (zombie_bot::tick(f, st, a)) acted = true;
				if (zombie_bot::tick(f, st, b)) acted = true;
			}
			player.next_frame();
		} catch (std::exception& e) {
			threw = true;
			why = e.what();
			break;
		}
		if (alive_units(st, a) == 0 || alive_units(st, b) == 0) break;
	}

	test("M03", "bot issues orders", acted);
	test("M04", "match runs without throwing", !threw, why.c_str());

	const int a1 = alive_units(st, a);
	const int b1 = alive_units(st, b);
	snprintf(detail, sizeof detail, "slot %d: %d -> %d, slot %d: %d -> %d", a, a0, a1, b, b0, b1);
	test("M05", "units die during the bot run", a1 < a0 || b1 < b0, detail);

	// A differential check: the same map, same frames, NO bot. Maps with a
	// "Elapsed time -> Defeat" trigger lose units on their own (measured:
	// GenericTwoBases slot 1 goes 154 -> 0 by frame 400), so M05 alone would
	// credit the bot for the map's own behaviour. The bot is only proven when it
	// changes the outcome, or when nothing dies without it and something dies with it.
	{
		game_player ctl(bwgame::data_loading::data_files_directory(std::string(dir)));
		auto& cst = ctl.st();
		{
			game_load_functions lf(cst);
			lf.setup_info.victory_condition = 0;
			lf.setup_info.tournament_mode = 0;
			lf.setup_info.starting_units = 0;
			lf.load_map_file(argv[1], {}, false);
		}
		cst.alliances[a][b] = 0;
		cst.alliances[b][a] = 0;
		const int ca0 = alive_units(cst, a), cb0 = alive_units(cst, b);
		for (int i = 0; i != 400; ++i) ctl.next_frame();
		const int ca1 = alive_units(cst, a), cb1 = alive_units(cst, b);

		char d2[200];
		snprintf(d2, sizeof d2, "no bot: slot %d %d->%d, slot %d %d->%d",
			a, ca0, ca1, b, cb0, cb1);
		const bool maps_own_losses = (ca1 < ca0) || (cb1 < cb0);
		const bool bot_changed = (a1 != ca1) || (b1 != cb1);
		test("M06", "bot output differs from an un-driven match (control run)",
			!maps_own_losses || bot_changed, d2);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
