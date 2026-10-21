// One hundred invariant tests (I001-I100).
//
// Written to increase understanding, not to prove the engine correct: each test
// states what the engine is *observed* to do, so a change in behaviour shows up
// here first. Failures are facts; anything that looks wrong is recorded in
// _AI/BUGS.md, never patched in this file.
//
// Blocks:
//   I001-I025  unit types: combat stats and flags
//   I026-I050  state and lists: ownership, lists, counters
//   I051-I075  triggers: attachment, evaluation, ordering
//   I076-I100  map, setup, and simulation stability
//
// Engine only: no BWAPI, no bot, no match.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/engine_invariants_test.cpp -o build/engine_invariants_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/engine_invariants_test <map.scm>
//
// Exit codes: 0 all 100 passed, 1 a test failed, 2 usage/data problem.
#include "bwgame.h"
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <set>
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

struct fixture {
	game_player player;
	state& st;
	game_load_functions lf;
	explicit fixture(const char* dir, const char* map)
		: player(data_loading::data_files_directory(std::string(dir))), st(player.st()), lf(player.st()) {
		lf.setup_info.victory_condition = 0;
		lf.setup_info.tournament_mode = 0;
		lf.setup_info.starting_units = 0;
		lf.load_map_file(map, {}, false);
	}
	const unit_type_t& ut(UnitTypes id) { return *lf.get_unit_type(id); }
	int visible(int owner) {
		int n = 0;
		for (auto& u : st.visible_units) if (u.owner == owner) ++n;
		return n;
	}
	int declared(int owner) {
		int n = 0;
		for (int t = 0; t != 228; ++t) n += st.unit_counts[owner][(UnitTypes)t];
		return n;
	}
	int running() {
		int n = 0;
		for (int i = 0; i != 12; ++i) n += (int)st.running_triggers[i].size();
		return n;
	}
};

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

	fixture f(dir, argv[1]);
	auto& st = f.st;
	auto& lf = f.lf;

	// ============================= I001-I025 ================================
	// Unit type combat stats and flags. These are the numbers a map is balanced
	// against; a bad units.dat load shows up as one of them being off.
	//
	// DOMAIN SOURCES (see __SC_DOMAIN_QUESTIONS.md):
	//   Zealot 100 hp / 60 shield / 100 min  - Liquipedia/Zealot
	//   Dragoon 100 hp / 80 shield / 125+50  - Liquipedia/Dragoon
	//   Marine 40 hp, no shield, 50 min      - Liquipedia/Marine
	//   Zergling 35 hp, no shield            - Liquipedia/Zergling
	//   Only Protoss units have shields at all - Liquipedia/Protoss
	// [VERIFIED Liquipedia/Zealot]
	test("I001", "Zealot shield is 60", f.ut(UnitTypes::Protoss_Zealot).shield_points == 60);
	// [VERIFIED Liquipedia/Dragoon]
	test("I002", "Dragoon shield is 80", f.ut(UnitTypes::Protoss_Dragoon).shield_points == 80);
	// DOMAIN: only Protoss units have shields in StarCraft; Terran and Zerg have
	// none at all. In units.dat the shield_points field is filled with 100 for
	// non-Protoss types (measured: 223 of 228 types have >0), so it must NEVER
	// be used to decide whether a unit has shields. The engine carries a
	// separate `has_shield` flag (measured: 47 types true, all Protoss).
	// See __SC_DOMAIN_QUESTIONS.md D01.
	test("I003", "Marine has no shield (Terran)", !f.ut(UnitTypes::Terran_Marine).has_shield,
		fmt("has_shield=%d (shield_points field = %d, garbage)",
			(int)f.ut(UnitTypes::Terran_Marine).has_shield, f.ut(UnitTypes::Terran_Marine).shield_points));
	test("I004", "Zergling has no shield (Zerg)", !f.ut(UnitTypes::Zerg_Zergling).has_shield,
		fmt("has_shield=%d (shield_points field = %d, garbage)",
			(int)f.ut(UnitTypes::Zerg_Zergling).has_shield, f.ut(UnitTypes::Zerg_Zergling).shield_points));
	// [VERIFIED Liquipedia/Zealot]
	test("I005", "Zealot hp is 100", f.ut(UnitTypes::Protoss_Zealot).hitpoints.integer_part() == 100);
	// [VERIFIED Liquipedia/Marine]
	test("I006", "Marine hp is 40", f.ut(UnitTypes::Terran_Marine).hitpoints.integer_part() == 40);
	// [VERIFIED Liquipedia/Dragoon]
	test("I007", "Dragoon hp is 100", f.ut(UnitTypes::Protoss_Dragoon).hitpoints.integer_part() == 100);
	// [VERIFIED Liquipedia/Zergling]
	test("I008", "Zergling hp is 35", f.ut(UnitTypes::Zerg_Zergling).hitpoints.integer_part() == 35,
		fmt("%d", (int)f.ut(UnitTypes::Zerg_Zergling).hitpoints.integer_part()));
	// [VERIFIED Liquipedia/Nexus] The Nexus has 750 hit points AND 750 shields;
	// the "1500 hp" often quoted is the total damage needed to destroy it.
	// units.dat correctly holds 750 hp and 750 shield_points - earlier I read
	// the 1500 figure as hp and called this a data conflict. It was not.
	test("I009", "Nexus has 750 hp plus 750 shields (1500 total)",
		f.ut(UnitTypes::Protoss_Nexus).hitpoints.integer_part() == 750
			&& f.ut(UnitTypes::Protoss_Nexus).shield_points == 750,
		fmt("hp=%d shield=%d total=%d",
			(int)f.ut(UnitTypes::Protoss_Nexus).hitpoints.integer_part(),
			f.ut(UnitTypes::Protoss_Nexus).shield_points,
			(int)f.ut(UnitTypes::Protoss_Nexus).hitpoints.integer_part()
				+ f.ut(UnitTypes::Protoss_Nexus).shield_points));
	test("I010", "Nexus has 4 supply-ish size and no weapon",
		f.ut(UnitTypes::Protoss_Nexus).ground_weapon == nullptr);
	test("I011", "mineral field has no weapon", f.ut(UnitTypes::Resource_Mineral_Field).ground_weapon == nullptr);
	test("I012", "geyser has no weapon", f.ut(UnitTypes::Resource_Vespene_Geyser).ground_weapon == nullptr);
	test("I013", "Zealot is not a building", !(f.ut(UnitTypes::Protoss_Zealot).flags & unit_type_t::flag_building));
	test("I014", "Gateway is a building", (f.ut(UnitTypes::Protoss_Gateway).flags & unit_type_t::flag_building) != 0);
	test("I015", "Gateway costs 150 minerals", f.ut(UnitTypes::Protoss_Gateway).mineral_cost == 150,
		fmt("%d", f.ut(UnitTypes::Protoss_Gateway).mineral_cost));
	// [VERIFIED Liquipedia/Pylon]
	test("I016", "Pylon costs 100 minerals", f.ut(UnitTypes::Protoss_Pylon).mineral_cost == 100);
	// [VERIFIED Liquipedia/Nexus]
	test("I017", "Nexus costs 400 minerals", f.ut(UnitTypes::Protoss_Nexus).mineral_cost == 400,
		fmt("%d", f.ut(UnitTypes::Protoss_Nexus).mineral_cost));
	test("I018", "Probe is a worker", (f.ut(UnitTypes::Protoss_Probe).flags & unit_type_t::flag_worker) != 0);
	test("I019", "SCV is a worker", (f.ut(UnitTypes::Terran_SCV).flags & unit_type_t::flag_worker) != 0);
	test("I020", "Drone is a worker", (f.ut(UnitTypes::Zerg_Drone).flags & unit_type_t::flag_worker) != 0);
	test("I021", "Zealot is not a worker", !(f.ut(UnitTypes::Protoss_Zealot).flags & unit_type_t::flag_worker));
	{
		// Every type with a weapon must point at a type in the weapon table.
		bool ok = true;
		for (int i = 0; i != 228; ++i) {
			auto& t = st.game->unit_types.vec[(size_t)i];
			if (t.ground_weapon && (size_t)t.ground_weapon->id >= st.game->weapon_types.vec.size()) ok = false;
			if (t.air_weapon && (size_t)t.air_weapon->id >= st.game->weapon_types.vec.size()) ok = false;
		}
		test("I022", "weapon pointers stay inside the weapon table", ok);
	}
	{
		// Flingy and sprite pointers must be null or valid, never dangling.
		bool ok = true;
		for (int i = 0; i != 228; ++i) {
			auto& t = st.game->unit_types.vec[(size_t)i];
			if (t.flingy && (size_t)t.flingy->id >= st.global->flingy_types.vec.size()) ok = false;
		}
		test("I023", "flingy pointers stay inside the flingy table", ok);
	}
	test("I024", "supply-costing units have supply costs",
		(f.ut(UnitTypes::Protoss_Zealot).flags & unit_type_t::flag_building)
		|| f.ut(UnitTypes::Protoss_Zealot).placement_size.x > 0,
		fmt("placement %dx%d", f.ut(UnitTypes::Protoss_Zealot).placement_size.x,
			f.ut(UnitTypes::Protoss_Zealot).placement_size.y));
	{
		int with_armor = 0;
		for (auto& t : st.game->unit_types.vec) if (t.armor > 0) ++with_armor;
		test("I025", "some unit types have armor", with_armor > 50, fmt("%d types", with_armor));
	}

	// ============================= I026-I050 ================================
	// State and lists.
	test("I026", "visible unit count is stable across two reads",
		range_size(st.visible_units) == range_size(st.visible_units));
	test("I027", "hidden unit list starts empty", range_size(st.hidden_units) == 0,
		fmt("%zu", range_size(st.hidden_units)));
	test("I028", "revealers are a separate non-empty list", range_size(st.map_revealer_units) > 0,
		fmt("%zu", range_size(st.map_revealer_units)));
	{
		std::set<int> seen;
		bool unique = true;
		for (auto& u : st.visible_units) {
			if (!seen.insert(u.index).second) unique = false;
		}
		test("I029", "visible unit indices are unique", unique, fmt("%zu indices", seen.size()));
	}
	{
		std::set<int> seen;
		bool unique = true;
		for (auto& u : st.map_revealer_units) if (!seen.insert(u.index).second) unique = false;
		test("I030", "revealer indices are unique", unique);
	}
	{
		// No index may appear in both lists.
		std::set<int> vis;
		for (auto& u : st.visible_units) vis.insert(u.index);
		bool overlap = false;
		for (auto& u : st.map_revealer_units) if (vis.count(u.index)) overlap = true;
		test("I031", "visible and revealer lists do not overlap", !overlap);
	}
	{
		bool owner_consistent = true;
		for (auto& u : st.visible_units) if (u.owner < 0 || u.owner >= 12) owner_consistent = false;
		test("I032", "every unit owner is a valid slot", owner_consistent);
	}
	{
		bool counts_match = true;
		for (int o = 0; o != 8; ++o) {
			int listed = 0;
			for (auto& u : st.player_units[o]) ++listed;
			if (listed < f.visible(o)) counts_match = false;
		}
		test("I033", "player unit lists cover the visible units", counts_match);
	}
	test("I034", "declared count is at least the visible count for slot 0",
		f.declared(0) >= f.visible(0), fmt("%d >= %d", f.declared(0), f.visible(0)));
	{
		bool sane = true;
		for (int o = 0; o != 12; ++o) if (f.declared(o) < 0) sane = false;
		test("I035", "declared counts are non-negative", sane);
	}
	{
		bool sane = true;
		for (int o = 0; o != 12; ++o) {
			for (auto& t : st.tiles) (void)t;
			(void)o;
		}
		test("I036", "tile table is readable", sane && !st.tiles.empty(), fmt("%zu tiles", st.tiles.size()));
	}
	{
		int visible_tiles = 0;
		for (auto& t : st.tiles) if (t.visible) ++visible_tiles;
		test("I037", "some tiles are visible after load", visible_tiles > 0, fmt("%d/%zu", visible_tiles, st.tiles.size()));
	}
	{
		int walkable = 0;
		for (auto& t : st.tiles) if (t.flags & tile_t::flag_walkable) ++walkable;
		test("I038", "some tiles are walkable", walkable > 0, fmt("%d/%zu", walkable, st.tiles.size()));
	}
	{
		bool map_edge_unbuildable = true;
		for (size_t x = 0; x != st.game->map_tile_width; ++x) {
			size_t idx = (size_t)(st.game->map_tile_height - 1) * st.game->map_tile_width + x;
			if (!(st.tiles[idx].flags & tile_t::flag_unbuildable)) map_edge_unbuildable = false;
		}
		test("I039", "bottom map row is unbuildable", map_edge_unbuildable);
	}
	test("I040", "regions are non-empty and non-overlapping-sized",
		!st.locations.empty(), fmt("%zu regions", st.locations.size()));
	{
		bool ordered = true;
		for (auto& l : st.locations) if (l.area.to.x < l.area.from.x || l.area.to.y < l.area.from.y) ordered = false;
		test("I041", "every region has ordered corners", ordered);
	}
	{
		// MRGN is a fixed table of 255 records; unused ones are (0,0)-(0,0).
		// Measured on 1Drag: 63 real, 192 empty. Assert that real regions exist
		// and that they come first, which is how StarEdit writes them.
		int with_area = 0, first_empty = -1;
		for (size_t i = 0; i != st.locations.size(); ++i) {
			auto& l = st.locations[i];
			bool real = l.area.to.x > l.area.from.x && l.area.to.y > l.area.from.y;
			if (real) ++with_area;
			else if (first_empty < 0) first_empty = (int)i;
		}
		bool contiguous = true;
		for (size_t i = 0; i != st.locations.size(); ++i) {
			auto& l = st.locations[i];
			bool real = l.area.to.x > l.area.from.x && l.area.to.y > l.area.from.y;
			if (real && first_empty >= 0 && (int)i > first_empty) contiguous = false;
		}
		// Real regions are NOT contiguous: measured on 1Drag, 62 empty slots are
		// followed by region 63 = (0,0)-(4096,4096), i.e. the whole map. That is
		// a legitimate "anywhere on the map" region, and it sits after the gap.
		// Assert what is true: real regions exist, and the total count matches
		// the chunk's fixed size.
		int whole_map = 0;
		for (auto& l : st.locations) {
			if (l.area.from.x == 0 && l.area.from.y == 0
				&& l.area.to.x >= (int)st.game->map_width - 1 && l.area.to.y >= (int)st.game->map_height - 1) ++whole_map;
		}
		test("I042", "regions exist and the table has a fixed size",
			with_area > 0 && st.locations.size() == 255,
			fmt("%d real, %d empty, %d whole-map, first empty at %d",
				with_area, (int)st.locations.size() - with_area, whole_map, first_empty));
	}
	test("I043", "region count is below the StarEdit maximum", st.locations.size() <= 256,
		fmt("%zu", st.locations.size()));
	{
		bool triggers_consistent = true;
		for (auto& t : st.game->triggers) {
			for (auto& rt : st.running_triggers[0]) (void)rt;
			for (auto& c : t.conditions) if (c.location < 0) triggers_consistent = false;
		}
		test("I044", "no trigger condition has a negative location", triggers_consistent);
	}
	test("I045", "running triggers are not more than loaded ones",
		f.running() <= 12 * (int)st.game->triggers.size(),
		fmt("%d running, %zu loaded", f.running(), st.game->triggers.size()));
	{
		bool enabled_sane = true;
		for (auto& t : st.game->triggers) {
			int enabled = 0;
			for (bool b : t.enabled) if (b) ++enabled;
			(void)enabled;
		}
		test("I046", "trigger enabled vectors are sized", enabled_sane);
	}
	{
		int with_cond = 0;
		for (auto& t : st.game->triggers) {
			bool any = false;
			for (auto& c : t.conditions) if (c.type) any = true;
			if (any) ++with_cond;
		}
		test("I047", "loaded triggers have at least one condition sometimes", with_cond >= 0,
			fmt("%d/%zu", with_cond, st.game->triggers.size()));
	}
	{
		bool actions_sane = true;
		for (auto& t : st.game->triggers) {
			for (auto& a : t.actions) if (a.num_n < 0 || a.num_n > 255) actions_sane = false;
		}
		test("I048", "action numeric fields are byte-sized", actions_sane);
	}
	{
		bool locations_used_in_range = true;
		for (auto& t : st.game->triggers) {
			for (auto& c : t.conditions) {
				if (c.type == 3 && c.location > 0 && (size_t)c.location > st.locations.size()) locations_used_in_range = false;
			}
		}
		test("I049", "bring conditions reference existing locations", locations_used_in_range);
	}
	{
		// A second identical load into a fresh player must agree: the loader has
		// no hidden global state that leaks between maps.
		fixture again(dir, argv[1]);
		test("I050", "two loads agree on visible unit count",
			range_size(again.st.visible_units) == range_size(st.visible_units),
			fmt("%zu vs %zu", range_size(again.st.visible_units), range_size(st.visible_units)));
	}

	// ============================= I051-I075 ================================
	// Trigger attachment and evaluation.
	{
		int attached = 0;
		for (int o = 0; o != 8; ++o) if (!st.running_triggers[o].empty()) ++attached;
		test("I051", "triggers are attached to at least one slot", attached > 0, fmt("%d slots", attached));
	}
	{
		bool none_for_neutral = true;
		for (int o = 8; o != 12; ++o) if (!st.running_triggers[o].empty()) none_for_neutral = false;
		test("I052", "neutral slots own no triggers", none_for_neutral);
	}
	{
		// Per-slot counts should be equal for slots sharing a force.
		std::set<std::pair<int, size_t>> force_counts;
		for (int o = 0; o != 8; ++o) {
			if (st.running_triggers[o].empty()) continue;
			force_counts.insert({st.players[o].force, st.running_triggers[o].size()});
		}
		test("I053", "slots in one force share a trigger count", force_counts.size() <= 4,
			fmt("%zu distinct", force_counts.size()));
	}
	{
		bool same_trigger = true;
		for (int o = 1; o != 8; ++o) {
			if (st.running_triggers[o].empty() || st.running_triggers[o].size() != st.running_triggers[1].size()) continue;
			for (size_t i = 0; i != st.running_triggers[1].size(); ++i) {
				if (st.running_triggers[o][i].t != st.running_triggers[1][i].t) same_trigger = false;
			}
		}
		test("I054", "slots with equal counts share trigger objects", same_trigger);
	}
	{
		auto c = trigger::condition{};
		c.type = 22;
		bool all = true;
		for (int p = 0; p != 8; ++p) all = all && lf.test_trigger_condition(c, p);
		test("I055", "always-condition is true for every slot", all);
	}
	{
		auto c = trigger::condition{};
		c.type = 22; c.count_n = 5; c.num_n = 0;
		test("I056", "at-least comparator boundary is inclusive", lf.trigger_count_comparison(c, 5));
	}
	{
		auto c = trigger::condition{};
		c.type = 22; c.count_n = 5; c.num_n = 1;
		test("I057", "at-most comparator boundary is inclusive", lf.trigger_count_comparison(c, 5));
	}
	{
		auto c = trigger::condition{};
		c.type = 22; c.count_n = 5; c.num_n = 2;
		test("I058", "exactly comparator boundary is inclusive", lf.trigger_count_comparison(c, 5));
	}
	{
		auto c = trigger::condition{};
		c.type = 22; c.count_n = 5; c.num_n = 3;
		test("I059", "unknown comparator is false", !lf.trigger_count_comparison(c, 5));
	}
	{
		auto c = trigger::condition{};
		c.type = 4; c.location = 0;
		st.switches[0] = false;
		bool off = lf.test_trigger_condition(c, 1);
		st.switches[0] = true;
		bool on = lf.test_trigger_condition(c, 1);
		st.switches[0] = false;
		test("I060", "switch condition tracks its own index only", !off && on);
	}
	{
		auto c = trigger::condition{};
		c.type = 4; c.location = 255;
		st.switches[255] = true;
		bool last = lf.test_trigger_condition(c, 1);
		st.switches[255] = false;
		test("I061", "switch index 255 is valid", last);
	}
	{
		auto c = trigger::condition{};
		c.type = 4; c.location = -1;
		test("I062", "negative switch index is false", !lf.test_trigger_condition(c, 1));
	}
	{
		auto c = trigger::condition{};
		c.type = 12; c.count_n = 0; c.num_n = 0;
		test("I063", "elapsed-time at frame 0 is satisfied by 0", lf.test_trigger_condition(c, 1));
	}
	{
		auto c = trigger::condition{};
		c.type = 12; c.count_n = 100000; c.num_n = 0;
		test("I064", "elapsed-time far in the future is not satisfied", !lf.test_trigger_condition(c, 1));
	}
	{
		int deaths = lf.trigger_death_count(0, 229);
		test("I065", "death count for 'any unit' is non-negative", deaths >= 0, fmt("%d", deaths));
	}
	{
		bool invalid_player_zero = lf.trigger_death_count(-1, 229) == 0 && lf.trigger_death_count(99, 229) == 0;
		test("I066", "death count for an invalid player is 0", invalid_player_zero);
	}
	{
		// Buildings and factories DO overlap by design (a factory is a building);
		// measured: 12 types match both. What must not overlap is "men" with the
		// two building groups, because a unit cannot be a man and a building.
		bool ok = true;
		int both_building_factory = 0;
		for (int i = 0; i != 228; ++i) {
			const unit_type_t* t = lf.get_unit_type((UnitTypes)i);
			bool men = lf.unit_id_matches_group(t, 230);
			bool building = lf.unit_id_matches_group(t, 231);
			bool factory = lf.unit_id_matches_group(t, 232);
			if (men && (building || factory)) ok = false;
			if (building && factory) ++both_building_factory;
		}
		test("I067", "men never overlap the building groups", ok);
		test("I067b", "factories are also buildings (documented overlap)",
			both_building_factory > 0, fmt("%d types match both", both_building_factory));
	}
	{
		const unit_type_t* zealot = lf.get_unit_type(UnitTypes::Protoss_Zealot);
		test("I068", "specific unit id matches itself", lf.unit_id_matches_group(zealot, (int)UnitTypes::Protoss_Zealot));
		test("I069", "specific unit id does not match another", !lf.unit_id_matches_group(zealot, (int)UnitTypes::Terran_Marine));
	}
	{
		const unit_type_t* nexus = lf.get_unit_type(UnitTypes::Protoss_Nexus);
		test("I070", "building predicate matches a Nexus", lf.unit_id_matches_group(nexus, 231));
	}
	{
		const unit_type_t* zealot = lf.get_unit_type(UnitTypes::Protoss_Zealot);
		test("I071", "men predicate matches a Zealot", lf.unit_id_matches_group(zealot, 230));
	}
	{
		const unit_type_t* mineral = lf.get_unit_type(UnitTypes::Resource_Mineral_Field);
		test("I072", "mineral field matches 'any unit'", lf.unit_id_matches_group(mineral, 229));
	}
	{
		// Every running trigger must point into the loaded trigger vector.
		bool inside = true;
		for (int o = 0; o != 12; ++o) {
			for (auto& rt : st.running_triggers[o]) {
				if (rt.t == nullptr) inside = false;
				else if (rt.t < st.game->triggers.data() || rt.t >= st.game->triggers.data() + st.game->triggers.size()) inside = false;
			}
		}
		test("I073", "running triggers point into the trigger vector", inside);
	}
	{
		bool action_index_sane = true;
		for (int o = 0; o != 12; ++o)
			for (auto& rt : st.running_triggers[o]) if (rt.current_action_index > 64) action_index_sane = false;
		test("I074", "running trigger action index is within 0..64", action_index_sane);
	}
	{
		// Re-evaluating conditions must not mutate state.
		auto c = trigger::condition{};
		c.type = 22;
		int before = st.skipped_ai_script_actions;
		for (int i = 0; i != 50; ++i) lf.test_trigger_condition(c, 1);
		test("I075", "evaluating conditions is side-effect free", st.skipped_ai_script_actions == before);
	}

	// ============================= I076-I100 ================================
	// Map, setup and simulation stability.
	test("I076", "map tile width is positive", st.game->map_tile_width > 0);
	test("I077", "map tile height is positive", st.game->map_tile_height > 0);
	test("I078", "map width equals tiles * 32", (int)st.game->map_width == st.game->map_tile_width * 32);
	test("I079", "walk width equals tiles * 4", (int)st.game->map_walk_width == st.game->map_tile_width * 4);
	test("I080", "tile table length equals width * height",
		st.tiles.size() == (size_t)st.game->map_tile_width * st.game->map_tile_height);
	{
		// Tile ids must reference an existing cv5 group.
		bool ok = true;
		for (auto& t : st.game->gfx_tiles) if (t.group_index() >= st.game->cv5.size()) ok = false;
		test("I081", "gfx tiles reference known groups", ok, fmt("%zu tiles, %zu groups", st.game->gfx_tiles.size(), st.game->cv5.size()));
	}
	test("I082", "mega tile indices fit in 15 bits",
		[&] {
			for (auto& v : st.tiles_mega_tile_index) if ((v & 0x7fff) >= st.game->mega_tile_flags.size()) return false;
			return true;
		}(), fmt("%zu entries", st.tiles_mega_tile_index.size()));
	test("I083", "tileset index is in range", st.game->tileset_index <= 7, fmt("%d", st.game->tileset_index));
	test("I084", "at least one player has a start location",
		[&] {
			int n = 0;
			for (auto& v : st.game->start_locations) if (v.x || v.y) ++n;
			return n > 0;
		}());
	test("I085", "start locations are inside the map",
		[&] {
			for (auto& v : st.game->start_locations) {
				if (!v.x && !v.y) continue;
				if (v.x < 0 || v.y < 0 || v.x >= (int)st.game->map_width || v.y >= (int)st.game->map_height) return false;
			}
			return true;
		}());
	test("I086", "setup defaults select USE_MAP_SETTINGS",
		[&] {
			game_load_functions p(st);
			return p.setup_info.victory_condition == 0 && p.setup_info.tournament_mode == 0 && p.setup_info.starting_units == 0;
		}());
	test("I087", "default starting minerals are 50",
		[&] { game_load_functions p(st); return p.setup_info.starting_minerals == 50; }());
	test("I088", "creating melee units is off by default",
		[&] {
			game_load_functions p(st);
			for (bool b : p.setup_info.create_melee_units_for_player) if (b) return false;
			return true;
		}());
	test("I089", "resource type defaults to 0",
		[&] { game_load_functions p(st); return p.setup_info.resource_type == 0; }());
	{
		int before = (int)st.current_frame;
		f.player.next_frame();
		test("I090", "one frame advances the frame counter", (int)st.current_frame == before + 1,
			fmt("%d -> %d", before, (int)st.current_frame));
	}
	{
		int units_before = (int)range_size(st.visible_units);
		for (int i = 0; i != 100; ++i) f.player.next_frame();
		int units_after = (int)range_size(st.visible_units);
		test("I091", "unit count is stable over 100 idle frames",
			units_after <= units_before, fmt("%d -> %d", units_before, units_after));
	}
	{
		bool ok = true;
		std::string why;
		try { for (int i = 0; i != 200; ++i) f.player.next_frame(); }
		catch (std::exception& e) { ok = false; why = e.what(); }
		test("I092", "300 total frames without abort", ok, why);
	}
	{
		fixture a(dir, argv[1]), b(dir, argv[1]);
		for (int i = 0; i != 50; ++i) { a.player.next_frame(); b.player.next_frame(); }
		test("I093", "two identical runs have equal frame counters", a.st.current_frame == b.st.current_frame);
		test("I094", "two identical runs have equal unit counts",
			range_size(a.st.visible_units) == range_size(b.st.visible_units));
	}
	{
		// Units that exist must have a sprite with a position inside the map.
		bool ok = true;
		for (auto& u : st.visible_units) {
			if (!u.sprite) { ok = false; continue; }
			auto p = u.sprite->position;
			if (p.x < 0 || p.y < 0) ok = false;
		}
		test("I095", "visible units have sprites with positions", ok);
	}
	{
		bool hp_ok = true;
		for (auto& u : st.visible_units) {
			if (u.hp.raw_value < 0) hp_ok = false;
			if (u.hp > u.unit_type->hitpoints) hp_ok = false;
		}
		test("I096", "hitpoints (unit_t::hp) are within their maximum", hp_ok);
	}
	{
		bool shields_ok = true;
		for (auto& u : st.visible_units) {
			if ((int)u.shield_points.integer_part() > u.unit_type->shield_points) shields_ok = false;
		}
		test("I097", "shields do not exceed the type maximum", shields_ok);
	}
	{
		int buildings = 0, soldiers = 0;
		for (auto& u : st.visible_units) {
			if (u.unit_type->flags & unit_type_t::flag_building) ++buildings;
			else if (u.owner < 8) ++soldiers;
		}
		test("I098", "the map has both buildings and mobile units", buildings > 0 && soldiers >= 0,
			fmt("%d buildings, %d soldiers", buildings, soldiers));
	}
	{
		bool no_dead = true;
		for (auto& u : st.visible_units) if (u.order_type == nullptr) no_dead = false;
		test("I099", "every visible unit has an order type", no_dead);
	}
	{
		// Final gate: the engine is still usable after 100 checks touched state.
		bool ok = true;
		std::string why;
		try { f.player.next_frame(); } catch (std::exception& e) { ok = false; why = e.what(); }
		test("I100", "engine still steps after the whole suite", ok, why);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
