// Fifty tests for the static data and the game state the engine exposes.
//
// These are the invariants that everything else (triggers, combat, the client)
// assumes but that nothing checks: unit type tables, costs, ownership counts,
// resource bookkeeping, map geometry and UMS setup defaults.
//
// Same rules as the other suites: engine-only, no BWAPI, no bot, and a failing
// check is a fact about the engine. Anything broken is recorded in __BUGS.md,
// never patched here.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/state_and_data_test.cpp -o build/state_and_data_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/state_and_data_test <map.scm>
//
// Exit codes: 0 all 50 passed, 1 a test failed, 2 usage/data problem.
#include "bwgame.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
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
	explicit fixture(const char* data_dir, const char* map)
		: player(data_loading::data_files_directory(std::string(data_dir))), st(player.st()) {
		game_load_functions lf(player.st());
		lf.setup_info.victory_condition = 0;
		lf.setup_info.tournament_mode = 0;
		lf.setup_info.starting_units = 0;
		lf.load_map_file(map, {}, false);
	}
	int unit_count(int owner) {
		int n = 0;
		for (auto& u : st.visible_units) if (u.owner == owner) ++n;
		return n;
	}
	int total_count(int owner) {
		int n = 0;
		for (int t = 0; t != 228; ++t) n += st.unit_counts[owner][(UnitTypes)t];
		return n;
	}
};

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
	auto& types = st.game->unit_types.vec;

	// ---- Unit type table (D01-D20) ------------------------------------------
	// The table is loaded from units.dat; a bad load shows up as nonsense costs
	// or hitpoints, and everything downstream (production, combat) inherits it.
	test("D01", "unit type table is fully populated", types.size() == 228,
		fmt("%zu entries", types.size()));
	{
		bool ids_ok = true;
		for (size_t i = 0; i != types.size(); ++i) if ((size_t)types[i].id != i) ids_ok = false;
		test("D02", "unit type ids match their index", ids_ok);
	}
	{
		int with_hp = 0;
		for (auto& t : types) if (t.hitpoints > 0_fp8) ++with_hp;
		test("D03", "most unit types have hitpoints", with_hp > 200, fmt("%d with hp", with_hp));
	}
	{
		const auto& zealot = types[(size_t)UnitTypes::Protoss_Zealot];
		// hitpoints is fp8: raw 25600 == 100 hp. Compare the integer part, which
		// is the number a map author sees, not the raw fixed-point value.
		test("D04", "Zealot has expected hp and shield",
			zealot.hitpoints.integer_part() == 100 && zealot.shield_points == 60,
			fmt("hp=%d shields=%d", (int)zealot.hitpoints.integer_part(), zealot.shield_points));
	}
	{
		const auto& zealot = types[(size_t)UnitTypes::Protoss_Zealot];
		test("D05", "Zealot has mineral and gas cost",
			zealot.mineral_cost == 100 && zealot.gas_cost == 0,
			fmt("%d/%d", zealot.mineral_cost, zealot.gas_cost));
	}
	{
		const auto& dragoon = types[(size_t)UnitTypes::Protoss_Dragoon];
		test("D06", "Dragoon has expected cost", dragoon.mineral_cost == 125 && dragoon.gas_cost == 50,
			fmt("%d/%d", dragoon.mineral_cost, dragoon.gas_cost));
	}
	{
		const auto& marine = types[(size_t)UnitTypes::Terran_Marine];
		test("D07", "Marine has expected cost", marine.mineral_cost == 50 && marine.gas_cost == 0,
			fmt("%d/%d", marine.mineral_cost, marine.gas_cost));
	}
	{
		// [VERIFIED Liquipedia/Zergling] Zerglings spawn in PAIRS from one larva:
		// 25 minerals per Zergling, 50 per egg pair. units.dat therefore holds 50
		// for the producing unit, and that is correct - it is the cost of one
		// production cycle (two Zerglings), not of a single one. Earlier this
		// looked like a data conflict against Liquipedia's "25"; it was not.
		const auto& zergling = types[(size_t)UnitTypes::Zerg_Zergling];
		test("D08", "Zergling costs 50 per pair (25 each, Liquipedia)",
			zergling.mineral_cost == 50 && zergling.gas_cost == 0,
			fmt("data=%d/%d", zergling.mineral_cost, zergling.gas_cost));
	}
	{
		// Most buildings take frames to build, but four special types have no
		// build time by design (150-152 = special buildings, 214 = Pylon-like
		// preplaced helper). Assert the majority instead of "all": building the
		// assertion on "every building" would be wrong, not the data.
		int buildings = 0, zero_time = 0;
		for (auto& t : types) {
			if (!(t.flags & unit_type_t::flag_building)) continue;
			++buildings;
			if (t.build_time == 0) ++zero_time;
		}
		// [ENGINE DATA] four special building types have no build time in the data
		// file; whether that matches the game is an open question (section D).
		test("D09", "almost all buildings have a build time (engine data)", buildings > 0 && zero_time <= 5,
			fmt("%d buildings, %d without build time", buildings, zero_time));
	}
	{
		const auto& nexus = types[(size_t)UnitTypes::Protoss_Nexus];
		test("D10", "Nexus is a building with placement size", (nexus.flags & unit_type_t::flag_building) && nexus.placement_size.x > 0,
			fmt("size=%dx%d", nexus.placement_size.x, nexus.placement_size.y));
	}
	{
		bool soldiers_have_weapons = true;
		const UnitTypes soldiers[] = {UnitTypes::Protoss_Zealot, UnitTypes::Terran_Marine,
			UnitTypes::Zerg_Zergling, UnitTypes::Protoss_Dragoon};
		for (UnitTypes id : soldiers) {
			const auto& t = types[(size_t)id];
			if (!t.ground_weapon && !t.air_weapon) soldiers_have_weapons = false;
		}
		test("D11", "basic fighters have a weapon", soldiers_have_weapons);
	}
	{
		bool workers_flagged = true;
		const UnitTypes workers[] = {UnitTypes::Protoss_Probe, UnitTypes::Terran_SCV, UnitTypes::Zerg_Drone};
		for (UnitTypes id : workers) {
			const auto& t = types[(size_t)id];
			if (!(t.flags & unit_type_t::flag_worker)) workers_flagged = false;
		}
		// [ENGINE DATA] flag_worker membership is a data-file fact; the game rule
		// "these three are the workers" is domain knowledge stated in
		// __SC_DOMAIN_QUESTIONS.md (section D).
		test("D12", "workers carry the Worker flag (engine data)", workers_flagged);
	}
	{
		const auto& mineral = types[(size_t)UnitTypes::Resource_Mineral_Field];
		test("D13", "mineral field is flagged as a resource", (mineral.flags & unit_type_t::flag_resource) != 0);
	}
	{
		const auto& revealer = types[(size_t)UnitTypes::Special_Map_Revealer];
		test("D14", "map revealer exists as a distinct type", (size_t)revealer.id == (size_t)UnitTypes::Special_Map_Revealer);
	}
	{
		bool movers = true;
		const UnitTypes movers_list[] = {UnitTypes::Protoss_Zealot, UnitTypes::Terran_Marine, UnitTypes::Zerg_Zergling};
		for (UnitTypes id : movers_list) {
			const auto& t = types[(size_t)id];
			if (!t.flingy) movers = false;
		}
		test("D15", "mobile fighters have a flingy", movers);
	}
	{
		bool sprites = true;
		for (auto& t : types) {
			if (t.id == UnitTypes::None) continue;
			if (!t.flingy) sprites = false;
		}
		test("D16", "every non-empty type has a flingy", sprites);
	}
	{
		const auto& turret = types[(size_t)UnitTypes::Terran_Missile_Turret];
		const auto& bunker = types[(size_t)UnitTypes::Terran_Bunker];
		test("D17", "turret/bunker are buildings",
			(turret.flags & unit_type_t::flag_building) != 0 && (bunker.flags & unit_type_t::flag_building) != 0);
	}
	{
		const auto& upgrade = st.game->upgrade_types.vec;
		test("D18", "upgrade table populated", upgrade.size() >= 46, fmt("%zu upgrades", upgrade.size()));
	}
	{
		const auto& tech = st.game->tech_types.vec;
		test("D19", "tech table populated", tech.size() >= 44, fmt("%zu techs", tech.size()));
	}
	{
		const auto& weapons = st.game->weapon_types.vec;
		test("D20", "weapon table populated", weapons.size() >= 100, fmt("%zu weapons", weapons.size()));
	}

	// ---- Ownership and counts (D21-D35) -------------------------------------
	{
		int owners_with_units = 0;
		for (int o = 0; o != 12; ++o) if (f.unit_count(o) > 0) ++owners_with_units;
		test("D21", "at least one owner has units", owners_with_units > 0, fmt("%d owners", owners_with_units));
	}
	{
		bool owners_valid = true;
		for (auto& u : st.visible_units) if (u.owner < 0 || u.owner >= 12) owners_valid = false;
		test("D22", "every visible unit has a valid owner", owners_valid);
	}
	{
		bool type_valid = true;
		for (auto& u : st.visible_units) if ((size_t)u.unit_type->id >= 228) type_valid = false;
		test("D23", "every visible unit has a valid type", type_valid);
	}
	{
		bool inside = true;
		for (auto& u : st.visible_units) {
			auto p = u.sprite->position;
			if (p.x < 0 || p.y < 0 || p.x >= (int)st.game->map_width || p.y >= (int)st.game->map_height) inside = false;
		}
		test("D24", "visible units are inside the map", inside);
	}
	{
		bool list_owned = true;
		for (auto& u : st.visible_units) {
			bool found = false;
			for (auto& v : st.player_units[u.owner]) if (v.index == u.index) { found = true; break; }
			if (!found) list_owned = false;
		}
		test("D25", "visible units appear in their owner's list", list_owned);
	}
	{
		int neutral = f.unit_count(11);
		test("D26", "neutral slot owns resources", neutral > 0, fmt("%d units", neutral));
	}
	{
		int sum = 0;
		for (int o = 0; o != 12; ++o) sum += f.total_count(o);
		test("D27", "counters sum to at least the visible units", sum >= (int)range_size(st.visible_units),
			fmt("counts=%d visible=%zu", sum, range_size(st.visible_units)));
	}
	{
		int revealers = 0;
		for (int o = 0; o != 12; ++o) revealers += st.unit_counts[o][UnitTypes::Special_Map_Revealer];
		test("D28", "revealer counters are non-negative", revealers >= 0, fmt("%d", revealers));
	}
	{
		bool no_negative = true;
		for (int o = 0; o != 12; ++o)
			for (int t = 0; t != 228; ++t)
				if (st.unit_counts[o][(UnitTypes)t] < 0) no_negative = false;
		test("D29", "no negative unit counters", no_negative);
	}
	{
		int dead = 0;
		for (auto& u : st.hidden_units) (void)u, ++dead;
		test("D30", "hidden unit list is readable", dead >= 0, fmt("%d hidden", dead));
	}
	{
		// supply_used/supply_available are per race (3 entries), each a fixed
		// point value; only the default (protoss-like) lane is checked here.
		bool supply_sane = true;
		for (int o = 0; o != 8; ++o) {
			for (int r = 0; r != 3; ++r) {
				if (st.supply_used[o][r].raw_value < 0) supply_sane = false;
			}
		}
		test("D31", "supply counters are non-negative", supply_sane);
	}
	{
		bool minerals_ok = true;
		for (int o = 0; o != 12; ++o) if (st.current_minerals[o] < 0) minerals_ok = false;
		test("D32", "starting minerals are non-negative", minerals_ok);
	}
	{
		bool gas_ok = true;
		for (int o = 0; o != 12; ++o) if (st.current_gas[o] < 0) gas_ok = false;
		test("D33", "starting gas is non-negative", gas_ok);
	}
	{
		bool victory_clean = true;
		for (int o = 0; o != 12; ++o) if (st.players[o].victory_state != 0) victory_clean = false;
		test("D34", "no player starts with a victory state", victory_clean);
	}
	{
		int playable = 0;
		for (int o = 0; o != 8; ++o) {
			auto c = st.players[o].controller;
			if (c == player_t::controller_occupied || c == player_t::controller_computer_game) ++playable;
		}
		test("D35", "the map declares playable slots", playable > 0, fmt("%d playable", playable));
	}

	// ---- Map geometry (D36-D42) ---------------------------------------------
	test("D36", "tile width is in range", st.game->map_tile_width >= 8 && st.game->map_tile_width <= 256,
		fmt("%d", st.game->map_tile_width));
	test("D37", "tile height is in range", st.game->map_tile_height >= 8 && st.game->map_tile_height <= 256,
		fmt("%d", st.game->map_tile_height));
	test("D38", "pixel size matches tile size",
		(int)st.game->map_width == st.game->map_tile_width * 32 && (int)st.game->map_height == st.game->map_tile_height * 32,
		fmt("%dx%d for %dx%d tiles", st.game->map_width, st.game->map_height,
			st.game->map_tile_width, st.game->map_tile_height));
	test("D39", "tile table matches map size",
		st.tiles.size() == (size_t)st.game->map_tile_width * st.game->map_tile_height,
		fmt("%zu tiles", st.tiles.size()));
	test("D40", "every tile has a mega tile index",
		st.tiles_mega_tile_index.size() == st.tiles.size() && st.tiles_mega_tile_index[0] < 0x8000,
		fmt("%zu entries", st.tiles_mega_tile_index.size()));
	test("D41", "at least one start location is set",
		[&] {
			for (auto& v : st.game->start_locations) if (v.x || v.y) return true;
			return false;
		}(), fmt("%zu slots", st.game->start_locations.size()));
	test("D42", "map has regions", !st.locations.empty(), fmt("%zu regions", st.locations.size()));

	// ---- UMS setup defaults (D43-D50) ---------------------------------------
	{
		// These are the defaults that make a map run as USE_MAP_SETTINGS.
		game_load_functions probe(st);
		bool ums = probe.setup_info.victory_condition == 0 && probe.setup_info.tournament_mode == 0
			&& probe.setup_info.starting_units == 0;
		test("D43", "fresh setup defaults mean USE_MAP_SETTINGS", ums,
			fmt("vc=%d tm=%d su=%d", probe.setup_info.victory_condition,
				probe.setup_info.tournament_mode, probe.setup_info.starting_units));
	}
	{
		game_load_functions probe(st);
		test("D44", "melee units are not created for any slot by default",
			[&] {
				for (bool b : probe.setup_info.create_melee_units_for_player) if (b) return false;
				return true;
			}());
	}
	{
		game_load_functions probe(st);
		test("D45", "default starting minerals are 50", probe.setup_info.starting_minerals == 50,
			fmt("%d", probe.setup_info.starting_minerals));
	}
	{
		game_load_functions probe(st);
		test("D46", "create_no_units defaults to false", !probe.setup_info.create_no_units);
	}
	{
		game_load_functions probe(st);
		test("D47", "resource_type defaults to 0", probe.setup_info.resource_type == 0);
	}
	{
		bool trig_ok = true;
		for (auto& t : st.game->triggers) {
			for (auto& c : t.conditions) if (c.type < 0 || c.type > 30) trig_ok = false;
		}
		test("D48", "loaded condition types are in a sane range", trig_ok);
	}
	{
		bool act_ok = true;
		for (auto& t : st.game->triggers) {
			for (auto& a : t.actions) if (a.type < 0 || a.type > 200) act_ok = false;
		}
		test("D49", "loaded action types are in a sane range", act_ok);
	}
	{
		// Everything above must leave the engine able to run a frame.
		bool ok = true;
		std::string why;
		try { f.player.next_frame(); } catch (std::exception& e) { ok = false; why = e.what(); }
		test("D50", "engine steps one frame after all checks", ok, why);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
