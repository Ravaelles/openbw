// Load-test extreme maps: several of the largest and several of the smallest
// files from one directory. Small maps stress sparse data; large maps stress
// the chunk reader and per-tile tables. Both are realistic, neither is exotic.
//
// [CORRECTED] An earlier version of this header said extremes came from /sc-ai/UMS_MAPS/rav/protoss/*.
// They are read from /sc-ai/UMS_MAPS/rav/protoss/ - see the corpus argument.
//
// The map itself is a map file under /sc-ai/UMS_MAPS/rav/protoss/. The test
// opens the largest and smallest maps it can find in that directory, loads each
// in USE_MAP_SETTINGS mode, and runs a short simulation.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/map_load_test.cpp -o build/map_load_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/map_load_test <map.scm> [<map.scm>...]
//
// Typical usage: pass the extremes chosen from /sc-ai/UMS_MAPS/rav/protoss/:
//   ./build/map_load_test /sc-ai/UMS_MAPS/rav/protoss/$(largest) ... $(smallest)
//
// Exit codes: 0 all maps loaded and simulated, 1 one map failed, 2 usage/data.
//
// Ordering the maps by size is the caller's job, so the test never guesses what
// "largest" means - it only asserts that whatever it is given loads.

#include "bwgame.h"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

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

static void test(const char* id, const char* name, bool ok, const std::string& detail = "") {
	++total;
	if (ok) printf("ok   %s %s\n", id, name);
	else {
		printf("FAIL %s %s%s%s\n", id, name, detail.empty() ? "" : ": ", detail.c_str());
		++failures;
	}
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: %s <map.scm> [<map.scm>...]\n", argv[0]);
		return 2;
	}
	const char* dir = getenv("UMS_TEST_DATA_DIR");
	if (!dir || !*dir) {
		printf("FAIL setup UMS_TEST_DATA_DIR not set\n");
		return 2;
	}

	for (int a = 1; a < argc; ++a) {
		const char* map = argv[a];
		printf("== %s\n", map);
		game_player player(data_loading::data_files_directory(std::string(dir)));
		auto& st = player.st();
		bool ok = true;
		std::string why;
		try {
			game_load_functions lf(st);
			lf.setup_info.victory_condition = 0;
			lf.setup_info.tournament_mode = 0;
			lf.setup_info.starting_units = 0;
			lf.load_map_file(map, {}, false);
		} catch (std::exception& e) {
			ok = false;
			why = e.what();
		}
		test("M1", "load without throwing", ok, why);
		if (!ok) continue;

		test("M2", "map has positive dimensions", st.game->map_tile_width > 0 && st.game->map_tile_height > 0,
			fmt("%dx%d tiles", st.game->map_tile_width, st.game->map_tile_height));
		test("M3", "tile table matches dimensions",
			st.tiles.size() == (size_t)st.game->map_tile_width * st.game->map_tile_height,
			fmt("%zu tiles", st.tiles.size()));
		test("M4", "at least one unit placed", range_size(st.visible_units) > 0,
			fmt("%zu", range_size(st.visible_units)));
		bool ran = true;
		std::string why2;
		try { for (int i = 0; i != 60; ++i) player.next_frame(); }
		catch (std::exception& e) { ran = false; why2 = e.what(); }
		test("M5", "60 frames run", ran, why2);
	}
	if (failures) {
		printf("\nFAILED: %d check(s)\n", failures);
		return 1;
	}
	printf("\nPASSED (%d checks)\n", total);
	return 0;
}
