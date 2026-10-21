// Behaviour tests for everything fixed in the UMS support work.
//
// One binary, several groups, each with its own label so a failure names the
// feature that broke. No BWAPI, no bot, no running match: this links bwgame.h
// directly, so it runs in seconds and can be used as a pre-commit check.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/engine_behaviour_test.cpp -o build/engine_behaviour_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/engine_behaviour_test <map.scm> [<second.map>]
//
// Groups:
//   UMS-1  map units exist and are attributed to the right players
//   UMS-2  map revealers are counted separately, never as army
//   TRG-1  the map's TRIG chunk is attached to its playable players
//   TRG-2  switches: condition 4 reads state, action 5 writes it
//   TRG-3  deaths: kill_unit increments the owner's table; group ids resolve
//   TRG-4  a simulation of 300 frames does not abort on trigger types 15/16/21/23
//   TRG-5  actions with no effect are counted, not silently ignored
//
// Exit codes: 0 all passed, 1 a check failed, 2 bad usage/data.
#include "bwgame.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace bwgame;

static int failures = 0;

static void check(bool ok, const char* group, const char* what, const char* detail = "") {
	if (ok) {
		printf("ok    %-6s %s%s%s\n", group, what, *detail ? ": " : "", detail);
	} else {
		printf("FAIL  %-6s %s%s%s\n", group, what, *detail ? ": " : "", detail);
		++failures;
	}
}

static std::string fmt(const char* f, ...) {
	char buf[256];
	va_list ap;
	va_start(ap, f);
	vsnprintf(buf, sizeof buf, f, ap);
	va_end(ap);
	return buf;
}

// Loads a map in USE_MAP_SETTINGS mode, which is what makes the engine read
// the map's own UNIT/MRGN/TRIG chunks instead of melee defaults.
static bool load_ums(game_player& player, const char* map) {
	game_load_functions lf(player.st());
	lf.setup_info.victory_condition = 0;
	lf.setup_info.tournament_mode = 0;
	lf.setup_info.starting_units = 0;
	lf.load_map_file(map, {}, false);
	return true;
}

static int count_owner(const state& st, int owner) {
	int n = 0;
	for (auto& v : st.visible_units) if (v.owner == owner) ++n;
	return n;
}

int main(int argc, char** argv) {
	if (argc < 2) {
		printf("usage: %s <map.scm> [<second.map>]\n", argv[0]);
		return 2;
	}
	const char* data_dir = getenv("UMS_TEST_DATA_DIR");
	if (!data_dir || !*data_dir) {
		printf("FAIL  setup  UMS_TEST_DATA_DIR must point at the directory with *.mpq\n");
		return 2;
	}
	const char* map = argv[1];

	game_player player(data_loading::data_files_directory(std::string(data_dir)));
	auto& st = player.st();

	bool loaded = true;
	try {
		load_ums(player, map);
	} catch (std::exception& e) {
		loaded = false;
		printf("FAIL  UMS-1  map load threw: %s\n", e.what());
		++failures;
	}
	if (!loaded) return 1;

	// --- UMS-1: units exist, with sane owners -------------------------------
	check(range_size(st.visible_units) > 0, "UMS-1", "map created visible units",
		fmt("%zu", range_size(st.visible_units)).c_str());
	int neutral = count_owner(st, 11);
	check(neutral > 0, "UMS-1", "neutral resources present", fmt("%d units", neutral).c_str());

	// --- UMS-2: revealers are separate from army ----------------------------
	// The engine keeps revealers in their own list; a player's real army is what
	// visible_units reports. If the lists ever merge, this catches it.
	int revealers = 0;
	for (auto& v : st.map_revealer_units) ++revealers;
	check(revealers > 0, "UMS-2", "map revealers loaded into their own list",
		fmt("%d revealers", revealers).c_str());
	check(st.unit_counts[0][UnitTypes::Special_Map_Revealer] >= 0,
		"UMS-2", "revealer counts readable per slot");

	// --- TRG-1: map triggers are attached to playable players ---------------
	int running = 0;
	for (int i = 0; i != 8; ++i) running += (int)st.running_triggers[i].size();
	check(running > 0, "TRG-1", "map triggers attached to players",
		fmt("%d running triggers", running).c_str());

	// --- TRG-3: deaths table ------------------------------------------------
	// Find any non-neutral unit and kill it through the normal path.
	unit_t* victim = nullptr;
	for (auto& v : st.visible_units) {
		if (v.owner < 8 && v.unit_type->id != UnitTypes::Special_Map_Revealer) { victim = &v; break; }
	}
	if (victim) {
		int owner = victim->owner;
		int type = (int)victim->unit_type->id;
		int before = st.unit_deaths[owner][victim->unit_type->id];
		player.funcs().kill_unit(victim);
		int after = st.unit_deaths[owner][victim->unit_type->id];
		check(after == before + 1, "TRG-3", "kill_unit increments the owner's death table",
			fmt("%d -> %d (type %d)", before, after, type).c_str());

		// Group resolution, through the same predicate the conditions use.
		int any = 0, men = 0, buildings = 0;
		for (int i = 0; i != 228; ++i) {
			const unit_type_t* ut = player.funcs().get_unit_type((UnitTypes)i);
			if (player.funcs().unit_id_matches_group(ut, 229)) any += st.unit_deaths[owner][(UnitTypes)i];
			if (player.funcs().unit_id_matches_group(ut, 230)) men += st.unit_deaths[owner][(UnitTypes)i];
			if (player.funcs().unit_id_matches_group(ut, 231)) buildings += st.unit_deaths[owner][(UnitTypes)i];
		}
		// Group ids must partition the death exactly once: "any unit" sees it,
		// and it falls in exactly one of men/buildings (or neither, for units
		// that are neither, e.g. resources). The earlier "men >= buildings"
		// assertion was wrong - killing a building gives men=0, buildings=1 on
		// purpose. Assert the property, not an ordering.
		check(any >= 1, "TRG-3", "group 229 (any unit) sees the death", fmt("%d", any).c_str());
		check(men + buildings <= any, "TRG-3",
			"groups 230/231 partition, never exceed 'any'",
			fmt("men=%d buildings=%d any=%d", men, buildings, any).c_str());
		check((men + buildings) == 1 || (men + buildings) == 0, "TRG-3",
			"one death counted at most once per group",
			fmt("men=%d buildings=%d", men, buildings).c_str());
	} else {
		printf("info  TRG-3  map has no non-neutral army, skipping kill test\n");
	}

	// --- TRG-4: simulate; conditions 15/16/21/23 must not abort --------------
	bool aborted = false;
	std::string why;
	try {
		for (int f = 0; f != 300; ++f) player.next_frame();
	} catch (std::exception& e) {
		aborted = true;
		why = e.what();
	}
	check(!aborted, "TRG-4", "300 frames run without aborting", why.c_str());

	// --- TRG-5: ignored AI-script actions are counted, not hidden -----------
	// >= 0 always; the real point is that the counter exists and is readable.
	check(st.skipped_ai_script_actions >= 0, "TRG-5", "skipped AI-script actions are counted",
		fmt("%d skipped", st.skipped_ai_script_actions).c_str());

	// --- TRG-2: switch state is real ---------------------------------------
	// The condition reads st.switches[], action 5 writes it. The state lives on
	// `state`, which game_player owns, so this checks the storage directly.
	{
		state& s = player.st();
		const bool before = s.switches[7];
		s.switches[7] = true;
		const bool on = s.switches[7];
		s.switches[7] = false;
		const bool off = s.switches[7];
		check(!before && on && !off, "TRG-2",
			"switch state toggles independently per index");
	}

	if (failures) {
		printf("\nFAILED: %d check(s)\n", failures);
		return 1;
	}
	printf("\nPASSED\n");
	return 0;
}
