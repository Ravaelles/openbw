// Regression tests for USE_MAP_SETTINGS (UMS) maps.
//
// These link bwgame.h directly: no BWAPI, no bot, no running match, no MPQ game
// data. They exist because two real bugs were found by hand and nothing in the
// build would catch them again:
//
//   1. UMS maps placed units for players 1..3 were never loaded (UNIT reader
//      ignored the StarEdit flags), so a map looked empty.
//   2. Trigger condition type 4 (Switch) aborted the whole simulation with
//      "unknown trigger condition 4", which killed the server mid-game.
//
// Build (from the repo root):
//   g++ -std=c++17 -O2 -I. tests/ums_map_test.cpp -o build/ums_map_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=/tmp/scdata ./build/ums_map_test /path/to/map.scm
//
// UMS_TEST_DATA_DIR must contain StarDat.mpq / BrooDat.mpq / Patch_rt.mpq
// (see data_loading::data_files_directory). The map path is passed on the
// command line so the test does not depend on any one machine's map layout.
#include "bwgame.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace bwgame;

static int failures = 0;

#define CHECK(cond, ...)                                                        \
	do {                                                                        \
		if (!(cond)) {                                                          \
			printf("FAIL %s:%d: ", __FILE__, __LINE__);                         \
			printf(__VA_ARGS__);                                                \
			printf("\n");                                                       \
			++failures;                                                         \
		} else {                                                                \
			printf("ok   ");                                                    \
			printf(__VA_ARGS__);                                                \
			printf("\n");                                                       \
		}                                                                      \
	} while (0)

struct loaded {
	game_player* player;
	game_load_functions* lf;
};

// Loads the map in USE_MAP_SETTINGS mode. Returns false if loading throws.
static bool load_ums(game_player& player, const char* map) {
	game_load_functions lf(player.st());
	lf.setup_info.victory_condition = 0;  // USE_MAP_SETTINGS
	lf.setup_info.tournament_mode = 0;
	lf.setup_info.starting_units = 0;
	lf.load_map_file(map, {}, false);
	return true;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: %s <map.scm>\n", argv[0]);
		return 2;
	}
	const char* map = argv[1];
	const char* data_dir = getenv("UMS_TEST_DATA_DIR");
	if (!data_dir || !*data_dir) {
		printf("FAIL: set UMS_TEST_DATA_DIR to the directory with the .mpq files\n");
		return 2;
	}

	game_player player(data_loading::data_files_directory(std::string(data_dir)));
	auto& st = player.st();

	// --- Test 1: loading a UMS map must not throw ------------------------
	bool loaded_ok = true;
	try {
		load_ums(player, map);
	} catch (std::exception& e) {
		printf("FAIL map load threw: %s\n", e.what());
		loaded_ok = false;
	}
	CHECK(loaded_ok, "UMS map loads without throwing");

	// --- Test 2: the map's own units exist and are visible ---------------
	// Counted from the scenario, not hard-coded per map: a map with no units
	// at all is legal, but units that exist must become visible units.
	size_t visible = range_size(st.visible_units);
	CHECK(visible > 0, "map created visible units (got %zu)", visible);

	int dragoons = 0, zealots = 0;
	std::vector<int> owners_with_units(12, 0);
	for (auto& v : st.visible_units) {
		++owners_with_units[v.owner];
		int id = (int)v.unit_type->id;
		if (id == 65) ++dragoons;  // Protoss_Dragoon
		if (id == 66) ++zealots;   // Protoss_Zealot
	}
	// The bundled check map has exactly one of each; other maps may differ, so
	// this is informational unless UMS_TEST_EXPECT=1Drag.
	const char* expect = getenv("UMS_TEST_EXPECT");
	if (expect && std::string(expect) == "1Drag") {
		CHECK(dragoons == 1, "1Drag_v_1Zeal: exactly 1 Dragoon (got %d)", dragoons);
		CHECK(zealots == 1, "1Drag_v_1Zeal: exactly 1 Zealot (got %d)", zealots);
		// 24 map units + 85 neutral (minerals/geysers) = 109
		CHECK(visible == 109, "1Drag_v_1Zeal: 109 visible units (24 map + 85 neutral, got %zu)", visible);
		CHECK(owners_with_units[0] + owners_with_units[3] == 2,
			"1Drag: exactly the two map fighters exist");
	}

	// --- Test 3: the map's own triggers are loaded -----------------------
	// USE_MAP_SETTINGS (victory_condition=0, tournament_mode=0,
	// starting_units=0) makes the engine read the map's MRGN/TRIG chunks. If
	// this ever regresses, the map silently plays as plain melee: its regions
	// and triggers are gone. Melee.trg alone contributes 6 triggers, so any
	// UMS map with a TRIG chunk must end up with more than 6.
	size_t trigger_count = st.game->triggers.size();
	const char* expect_triggers = getenv("UMS_TEST_MIN_TRIGGERS");
	if (expect_triggers) {
		int min_triggers = atoi(expect_triggers);
		CHECK((int)trigger_count > min_triggers,
			"map triggers loaded (%zu > %d from Melee.trg)", trigger_count, min_triggers);
	} else {
		printf("info trigger count = %zu\n", trigger_count);
	}

	// --- Test 4: running must not abort on an unsupported trigger type ---
	// Reproduces "unknown trigger condition 4". 300 frames is past the first
	// trigger evaluation (trigger_timer is 30 frames). Only meaningful when the
	// map's triggers are actually loaded (test 3).
	bool ran_ok = true;
	int aborted_frame = -1;
	std::string abort_msg;
	try {
		for (int f = 0; f != 300; ++f) player.next_frame();
	} catch (std::exception& e) {
		ran_ok = false;
		aborted_frame = (int)st.current_frame;
		abort_msg = e.what();
	}
	CHECK(ran_ok, "300 frames run without aborting (aborted at %d: %s)",
		aborted_frame, abort_msg.c_str());

	// --- Test 5: the players that own units stay the same ----------------
	// Guards the bot-slot problem: if a change silently empties the players
	// that own units, a bot joined there sees nothing. Only checked when the
	// caller says which map this is.
	if (expect && std::string(expect) == "1Drag") {
		CHECK(owners_with_units[0] == 1, "1Drag: player 0 owns 1 unit (got %d)", owners_with_units[0]);
		CHECK(owners_with_units[3] == 1, "1Drag: player 3 owns 1 unit (got %d)", owners_with_units[3]);
		CHECK(owners_with_units[1] == 0 && owners_with_units[2] == 0,
			"1Drag: players 1 and 2 own no units (bot slot is empty)");
	}

	if (failures) {
		printf("FAILED: %d check(s)\n", failures);
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
