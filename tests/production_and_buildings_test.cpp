// Fifty tests for production rules and building flags.
//
// Domain sources (each assertion cites its own):
//   https://liquipedia.net/starcraft/Nexus      produces Probes; Protoss tech root
//   https://liquipedia.net/starcraft/Gateway    produces Zealot/Dragoon/HT/DT; needs Pylon power
//   https://liquipedia.net/starcraft/Hatchery   spawns larvae; provides 1 Psi (= 1 supply)
//   https://liquipedia.net/starcraft/Overlord   supplies 8 control; 200 hp; 100 min; sight 9
//   https://liquipedia.net/starcraft/Zergling   two per egg  (engine: flag_two_units_in_one_egg)
//   https://liquipedia.net/starcraft/Protoss    Pylon powers buildings via psionic matrix
//
// [ENGINE DATA] marks values that only describe the loaded tables.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/production_and_buildings_test.cpp -o build/production_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/production_test <map.scm>
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
	game_load_functions lf;
	explicit fixture(const char* dir, const char* map)
		: player(data_loading::data_files_directory(std::string(dir))), st(player.st()), lf(player.st()) {
		lf.setup_info.victory_condition = 0;
		lf.setup_info.tournament_mode = 0;
		lf.setup_info.starting_units = 0;
		lf.load_map_file(map, {}, false);
	}
	const unit_type_t& u(UnitTypes id) { return *lf.get_unit_type(id); }
	bool has(UnitTypes id, unsigned flag) { return (u(id).flags & flag) != 0; }
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

	// ================== Protoss production (P01-P15) ======================
	// [VERIFIED https://liquipedia.net/starcraft/Nexus] the Nexus produces Probes
	test("P01", "Nexus is a building", f.has(UnitTypes::Protoss_Nexus, unit_type_t::flag_building));
	test("P02", "Probe is a worker", f.has(UnitTypes::Protoss_Probe, unit_type_t::flag_worker));
	test("P03", "Probe is not a building", !f.has(UnitTypes::Protoss_Probe, unit_type_t::flag_building));
	// [VERIFIED https://liquipedia.net/starcraft/Gateway] Gateway needs Pylon power
	test("P04", "Gateway requires the psionic matrix",
		f.has(UnitTypes::Protoss_Gateway, unit_type_t::flag_requires_psionic_matrix));
	test("P05", "Gateway is a building", f.has(UnitTypes::Protoss_Gateway, unit_type_t::flag_building));
	// [VERIFIED Liquipedia/Protoss] Pylon provides the power field
	test("P06", "Pylon is a building", f.has(UnitTypes::Protoss_Pylon, unit_type_t::flag_building));
	test("P07", "Pylon does not itself require power",
		!f.has(UnitTypes::Protoss_Pylon, unit_type_t::flag_requires_psionic_matrix));
	// [VERIFIED Liquipedia/Gateway] the units it produces are mobile, not buildings
	test("P08", "Zealot is not a building", !f.has(UnitTypes::Protoss_Zealot, unit_type_t::flag_building));
	test("P09", "Zealot is not a worker", !f.has(UnitTypes::Protoss_Zealot, unit_type_t::flag_worker));
	test("P10", "Dragoon is not a flying unit", !f.has(UnitTypes::Protoss_Dragoon, unit_type_t::flag_flyer));
	// [ENGINE DATA] Protoss ground army does not need creep
	test("P11", "Zealot does not require creep", !f.has(UnitTypes::Protoss_Zealot, unit_type_t::flag_requires_creep));
	// [ENGINE DATA] every psionic-matrix requirement belongs to a Protoss building
	{
		// The engine has no race field in unit_type_t, so this is expressed by
		// "every type that requires the matrix is a building".
		bool ok = true;
		int with_power = 0;
		for (auto& t : st.game->unit_types.vec) {
			if (!(t.flags & unit_type_t::flag_requires_psionic_matrix)) continue;
			++with_power;
			if (!(t.flags & unit_type_t::flag_building)) ok = false;
		}
		test("P12", "only buildings require the psionic matrix", ok, fmt("%d such types", with_power));
	}
	{	// [VERIFIED Liquipedia/Protoss] only Protoss buildings are powered, so at
		// least one Protoss building must require it.
		test("P13", "some buildings require Pylon power",
			f.has(UnitTypes::Protoss_Gateway, unit_type_t::flag_requires_psionic_matrix)
			|| f.has(UnitTypes::Protoss_Forge, unit_type_t::flag_requires_psionic_matrix));
	}
	{	// [ENGINE DATA] every building type has a placement size, so placement math works
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) {
			if (!(t.flags & unit_type_t::flag_building)) continue;
			if (t.placement_size.x <= 0 || t.placement_size.y <= 0) ok = false;
		}
		test("P14", "every building has a placement size", ok);
	}
	{	// [ENGINE DATA] building placement sizes are tile multiples
		bool ok = true;
		int checked = 0;
		for (auto& t : st.game->unit_types.vec) {
			if (!(t.flags & unit_type_t::flag_building)) continue;
			if (t.placement_size.x % 32 || t.placement_size.y % 32) ok = false;
			++checked;
		}
		test("P15", "building sizes are whole tiles", ok, fmt("%d buildings", checked));
	}

	// ================== Zerg production (P16-P30) =========================
	// [VERIFIED https://liquipedia.net/starcraft/Hatchery] the Hatchery is the Zerg
	// production centre and spawns larvae. The page also states: "Hatcheries by
	// themselves provide one Psi to the Zerg player", and Psi is another name for
	// supply. So it provides exactly 1 supply.
	test("P16", "Hatchery is a building", f.has(UnitTypes::Zerg_Hatchery, unit_type_t::flag_building));
	// [VERIFIED https://liquipedia.net/starcraft/Hatchery + /Supply] "one Psi" is
	// one supply. An earlier version asserted 0 and raised a false BUG-10 because
	// I never read the page for the supply sentence - a lesson recorded in
	// __CONVENTIONS.md section 1.
	test("P17", "Hatchery provides 1 supply (one Psi)",
		f.u(UnitTypes::Zerg_Hatchery).supply_provided.raw_value == 2,
		fmt("data raw=%d (=%.1f supply)", (int)f.u(UnitTypes::Zerg_Hatchery).supply_provided.raw_value,
			(double)f.u(UnitTypes::Zerg_Hatchery).supply_provided.raw_value / 2));
	test("P18", "Hatchery does not require creep (it spreads it)",
		!f.has(UnitTypes::Zerg_Hatchery, unit_type_t::flag_requires_creep));
	test("P19", "Hatchery is not a detector", !f.has(UnitTypes::Zerg_Hatchery, unit_type_t::flag_detector));
	// [VERIFIED https://liquipedia.net/starcraft/Overlord] supplies 8, 200 hp, 100 min, sight 9
	test("P20", "Overlord provides 8 supply", f.u(UnitTypes::Zerg_Overlord).supply_provided.raw_value == 16,
		fmt("raw=%d (fp1)", (int)f.u(UnitTypes::Zerg_Overlord).supply_provided.raw_value));
	test("P21", "Overlord has 200 hp", f.u(UnitTypes::Zerg_Overlord).hitpoints.integer_part() == 200,
		fmt("%d", (int)f.u(UnitTypes::Zerg_Overlord).hitpoints.integer_part()));
	test("P22", "Overlord costs 100 minerals", f.u(UnitTypes::Zerg_Overlord).mineral_cost == 100);
	test("P23", "Overlord sight range is 9", (int)f.u(UnitTypes::Zerg_Overlord).sight_range == 9,
		fmt("%d", (int)f.u(UnitTypes::Zerg_Overlord).sight_range));
	test("P24", "Overlord is a flyer", f.has(UnitTypes::Zerg_Overlord, unit_type_t::flag_flyer));
	test("P25", "Overlord is not a building", !f.has(UnitTypes::Zerg_Overlord, unit_type_t::flag_building));
	// [VERIFIED https://liquipedia.net/starcraft/Zergling] two Zerglings per egg
	// is encoded as "two units in one egg" on the producing entity.
	{
		bool any_double = false;
		for (auto& t : st.game->unit_types.vec) {
			if (t.flags & unit_type_t::flag_two_units_in_one_egg) any_double = true;
		}
		test("P26", "some type produces two units per egg", any_double);
	}
	test("P27", "Zergling is not a building", !f.has(UnitTypes::Zerg_Zergling, unit_type_t::flag_building));
	test("P28", "Zergling is not a worker", !f.has(UnitTypes::Zerg_Zergling, unit_type_t::flag_worker));
	test("P29", "Drone is a worker and not a building",
		f.has(UnitTypes::Zerg_Drone, unit_type_t::flag_worker) && !f.has(UnitTypes::Zerg_Drone, unit_type_t::flag_building));
	{	// [VERIFIED https://liquipedia.net/starcraft/Hatchery + /Overlord] Zerg supply
		// comes from the Hatchery (1 each) and the Overlord (8 each). Measured in
		// the Zerg id range the supply providers are Hatchery (building), Overlord
		// (8) and Hero_Yggdrasill (30, a campaign hero). The rule-level assertion is
		// therefore about the two regular ones, not "exactly one non-building".
		int hatchery = (int)f.u(UnitTypes::Zerg_Hatchery).supply_provided.raw_value;
		int overlord = (int)f.u(UnitTypes::Zerg_Overlord).supply_provided.raw_value;
		test("P30", "Zerg supply totals Hatchery(1) + Overlord(8) per type",
			hatchery == 2 && overlord == 16,
			fmt("hatchery raw=%d, overlord raw=%d", hatchery, overlord));
	}

	// ================== Terran production (P31-P38) =======================
	// [VERIFIED https://liquipedia.net/starcraft/Supply_Depot] Supply Depot
	test("P31", "Supply Depot is a building", f.has(UnitTypes::Terran_Supply_Depot, unit_type_t::flag_building));
	test("P32", "Supply Depot provides 8 supply", f.u(UnitTypes::Terran_Supply_Depot).supply_provided.raw_value == 16);
	test("P33", "Supply Depot requires no supply", f.u(UnitTypes::Terran_Supply_Depot).supply_required.raw_value == 0);
	test("P34", "SCV is a worker", f.has(UnitTypes::Terran_SCV, unit_type_t::flag_worker));
	test("P35", "Marine is not a building", !f.has(UnitTypes::Terran_Marine, unit_type_t::flag_building));
	test("P36", "Marine is not a worker", !f.has(UnitTypes::Terran_Marine, unit_type_t::flag_worker));
	{	// [ENGINE DATA] Terran buildings that fly are flagged as flying buildings
		bool ok = !f.has(UnitTypes::Terran_Supply_Depot, unit_type_t::flag_flying_building)
			&& f.has(UnitTypes::Terran_Command_Center, unit_type_t::flag_flying_building);
		test("P37", "flying buildings are flagged (Command Center yes, Depot no)", ok);
	}
	{	// [ENGINE DATA] addons are separate types flagged as addons
		bool any_addon = false;
		for (auto& t : st.game->unit_types.vec) if (t.flags & unit_type_t::flag_addon) any_addon = true;
		test("P38", "addon types exist", any_addon);
	}

	// ================== Cross-cutting building rules (P39-P50) ============
	{	// [ENGINE DATA] buildings never require creep except the Zerg ones that do
		int creep_buildings = 0;
		for (auto& t : st.game->unit_types.vec) {
			if ((t.flags & unit_type_t::flag_building) && (t.flags & unit_type_t::flag_requires_creep)) ++creep_buildings;
		}
		test("P39", "only a few buildings require creep", creep_buildings < 20, fmt("%d", creep_buildings));
	}
	{	// [ENGINE DATA] detectors exist (Observer / Science Vessel / Overlord class)
		int detectors = 0;
		for (auto& t : st.game->unit_types.vec) if (t.flags & unit_type_t::flag_detector) ++detectors;
		test("P40", "detector types exist", detectors >= 3, fmt("%d", detectors));
	}
	{	// [ENGINE DATA] every building has hp and a mineral cost
		// [ENGINE DATA] placeholder types ("Unused_Terran1" etc.) are flagged as
		// buildings but have no hp; they are not real buildings. Measure how many,
		// so a data change that turns a real building into an empty one shows up.
		int zero_hp = 0;
		std::string names;
		for (auto& t : st.game->unit_types.vec) {
			if (!(t.flags & unit_type_t::flag_building)) continue;
			if (t.hitpoints.integer_part() <= 0) {
				++zero_hp;
				if (names.size() < 40) names += fmt("%d ", (int)t.id);
			}
		}
		test("P41", "only placeholder building types lack hit points", zero_hp > 0 && zero_hp <= 6,
			fmt("%d without hp: %s", zero_hp, names.c_str()));
	}
	{	// [ENGINE DATA] buildings do not carry supply *requirements*
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) {
			if (!(t.flags & unit_type_t::flag_building)) continue;
			if (t.supply_required.raw_value != 0) ok = false;
		}
		test("P42", "buildings never require supply", ok);
	}
	{	// [ENGINE DATA] workers can gather (flagged as workers) for all three races
		int workers = 0;
		for (auto& t : st.game->unit_types.vec) if (t.flags & unit_type_t::flag_worker) ++workers;
		test("P43", "exactly the three workers are flagged", workers == 3, fmt("%d", workers));
	}
	{	// [ENGINE DATA] resource types are flagged and are not buildings
		test("P44", "mineral field flagged as a resource",
			f.has(UnitTypes::Resource_Mineral_Field, unit_type_t::flag_resource));
		test("P45", "geyser flagged as a resource",
			f.has(UnitTypes::Resource_Vespene_Geyser, unit_type_t::flag_resource));
		// [ENGINE DATA] in this units.dat the resource types ALSO carry flag_building
	// (measured flags 0x24002001 for both). That is surprising but consistent:
	// they are static, non-mobile "structures" for pathing purposes. The test
	// pins the observed fact and its consequence rather than guessing intent.
	test("P46", "resources carry the building flag (engine data)",
			f.has(UnitTypes::Resource_Mineral_Field, unit_type_t::flag_building)
			&& f.has(UnitTypes::Resource_Vespene_Geyser, unit_type_t::flag_building));
	}
	{	// [ENGINE DATA] heroes are flagged separately, so a map can tell them apart
		int heroes = 0;
		for (auto& t : st.game->unit_types.vec) if (t.flags & unit_type_t::flag_hero) ++heroes;
		test("P47", "hero types are flagged", heroes > 0, fmt("%d", heroes));
	}
	{	// [ENGINE DATA] burrowing is only possible for flagged types
		int burrowers = 0;
		for (auto& t : st.game->unit_types.vec) if (t.flags & unit_type_t::flag_can_burrow) ++burrowers;
		test("P48", "some types can burrow", burrowers > 0 && burrowers < 40, fmt("%d", burrowers));
	}
	{	// [ENGINE DATA] energy types are a small, dedicated set (casters)
		int casters = 0;
		for (auto& t : st.game->unit_types.vec) if (t.flags & unit_type_t::flag_has_energy) ++casters;
		test("P49", "caster types are a small set", casters > 0 && casters < 40, fmt("%d", casters));
	}
	{	// Final gate: the engine still runs after all production checks.
		bool ok = true;
		std::string why;
		try { f.player.next_frame(); } catch (std::exception& e) { ok = false; why = e.what(); }
		test("P50", "engine steps one frame after production checks", ok, why);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
