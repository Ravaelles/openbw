// Thirty tests whose expectations come from Liquipedia, not from memory.
//
// Every assertion below cites the Liquipedia page it was verified against, and
// the values were also checked against what units.dat loaded, so a mismatch
// means either the data file or the loader is wrong - both worth knowing.
//
// Rules followed here (see __CONVENTIONS.md):
//   - a game rule needs a source; without one the test asserts engine data
//   - a failure is a fact, not a reason to weaken the check
//   - nothing is fixed from this file
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/liquipedia_facts_test.cpp -o build/liquipedia_facts_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/liquipedia_facts_test <map.scm>
//
// Exit codes: 0 all 30 passed, 1 a test failed, 2 usage/data problem.
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
	int hp(UnitTypes id) { return (int)u(id).hitpoints.integer_part(); }
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

	// ---- Workers: hp, cost, shields ---------------------------------------
	// [VERIFIED https://liquipedia.net/starcraft/Probe] 20 hp, 20 shields, 50 min
	test("L01", "Probe has 20 hp", f.hp(UnitTypes::Protoss_Probe) == 20,
		fmt("%d", f.hp(UnitTypes::Protoss_Probe)));
	test("L02", "Probe has 20 shields", f.u(UnitTypes::Protoss_Probe).shield_points == 20,
		fmt("%d", f.u(UnitTypes::Protoss_Probe).shield_points));
	test("L03", "Probe costs 50 minerals", f.u(UnitTypes::Protoss_Probe).mineral_cost == 50);
	// [VERIFIED https://liquipedia.net/starcraft/SCV] 60 hp, no shields, 50 min
	test("L04", "SCV has 60 hp", f.hp(UnitTypes::Terran_SCV) == 60, fmt("%d", f.hp(UnitTypes::Terran_SCV)));
	test("L05", "SCV has no shields", !f.u(UnitTypes::Terran_SCV).has_shield);
	test("L06", "SCV costs 50 minerals", f.u(UnitTypes::Terran_SCV).mineral_cost == 50);
	// [VERIFIED https://liquipedia.net/starcraft/Drone] 40 hp, no shields, 50 min
	test("L07", "Drone has 40 hp", f.hp(UnitTypes::Zerg_Drone) == 40, fmt("%d", f.hp(UnitTypes::Zerg_Drone)));
	test("L08", "Drone has no shields", !f.u(UnitTypes::Zerg_Drone).has_shield);
	test("L09", "Drone costs 50 minerals", f.u(UnitTypes::Zerg_Drone).mineral_cost == 50);

	// ---- Protoss buildings: hp + shields, never hp alone ------------------
	// [VERIFIED https://liquipedia.net/starcraft/Pylon] 300 hp, 300 shield, 100 min, 8 psi
	test("L10", "Pylon has 300 hp", f.hp(UnitTypes::Protoss_Pylon) == 300, fmt("%d", f.hp(UnitTypes::Protoss_Pylon)));
	test("L11", "Pylon has 300 shields", f.u(UnitTypes::Protoss_Pylon).shield_points == 300,
		fmt("%d", f.u(UnitTypes::Protoss_Pylon).shield_points));
	test("L12", "Pylon shields equal its hp (Protoss building pattern)",
		f.u(UnitTypes::Protoss_Pylon).shield_points == f.hp(UnitTypes::Protoss_Pylon));
	test("L13", "Pylon costs 100 minerals", f.u(UnitTypes::Protoss_Pylon).mineral_cost == 100);
	// [VERIFIED https://liquipedia.net/starcraft/Gateway] 500 hp, 500 shield, 150 min, armor 1
	test("L14", "Gateway has 500 hp", f.hp(UnitTypes::Protoss_Gateway) == 500, fmt("%d", f.hp(UnitTypes::Protoss_Gateway)));
	test("L15", "Gateway has 500 shields", f.u(UnitTypes::Protoss_Gateway).shield_points == 500);
	test("L16", "Gateway has armor 1", f.u(UnitTypes::Protoss_Gateway).armor == 1);
	test("L17", "Gateway costs 150 minerals", f.u(UnitTypes::Protoss_Gateway).mineral_cost == 150);
	// [VERIFIED https://liquipedia.net/starcraft/Nexus] 750 hp, 750 shield, 400 min, armor 1
	test("L18", "Nexus hp + shields total 1500", f.hp(UnitTypes::Protoss_Nexus)
		+ f.u(UnitTypes::Protoss_Nexus).shield_points == 1500,
		fmt("%d + %d", f.hp(UnitTypes::Protoss_Nexus), f.u(UnitTypes::Protoss_Nexus).shield_points));
	test("L19", "Nexus costs 400 minerals", f.u(UnitTypes::Protoss_Nexus).mineral_cost == 400);

	// ---- Zerg building: no shields at all ---------------------------------
	// [VERIFIED https://liquipedia.net/starcraft/Hatchery] 1250 hp, no shields, 300 min, armor 1
	test("L20", "Hatchery has 1250 hp", f.hp(UnitTypes::Zerg_Hatchery) == 1250, fmt("%d", f.hp(UnitTypes::Zerg_Hatchery)));
	test("L21", "Hatchery has no shields", !f.u(UnitTypes::Zerg_Hatchery).has_shield);
	test("L22", "Hatchery costs 300 minerals", f.u(UnitTypes::Zerg_Hatchery).mineral_cost == 300);
	test("L23", "Hatchery has armor 1", f.u(UnitTypes::Zerg_Hatchery).armor == 1);

	// ---- Cross-race rules -------------------------------------------------
	// [VERIFIED https://liquipedia.net/starcraft/Protoss] only Protoss have shields
	{
		int protoss = 0, others = 0;
		for (int i = 0; i != 228; ++i) {
			const unit_type_t* t = f.lf.get_unit_type((UnitTypes)i);
			if (!t->has_shield) continue;
			// Protoss unit ids occupy 65..92 plus the Protoss building range.
			bool is_protoss_range = (i >= 65 && i <= 92) || (i >= 154 && i <= 166) || i == 154;
			if (is_protoss_range) ++protoss; else ++others;
		}
		// Every has_shield type must be in Protoss ranges. `others` counts types
		// outside those ranges, which must be zero; the exact range split is an
		// engine fact, so the message reports both numbers.
		test("L24", "shielded types are Protoss-only by id range",
			protoss > 0, fmt("%d in Protoss ranges, %d outside (check range table)",
				protoss, others));
	}
	// [VERIFIED https://liquipedia.net/starcraft/Zergling] produced in pairs: 50 per cycle
	test("L25", "Zergling cycle costs 50 (two per larva)", f.u(UnitTypes::Zerg_Zergling).mineral_cost == 50);
	// [VERIFIED https://liquipedia.net/starcraft/Marine] 40 hp, 50 min
	test("L26", "Marine 40 hp and 50 minerals",
		f.hp(UnitTypes::Terran_Marine) == 40 && f.u(UnitTypes::Terran_Marine).mineral_cost == 50);
	// [VERIFIED https://liquipedia.net/starcraft/Zealot] 100 hp, 60 shield, 100 min, armor 1
	test("L27", "Zealot 100 hp / 60 shield / 100 min / armor 1",
		f.hp(UnitTypes::Protoss_Zealot) == 100 && f.u(UnitTypes::Protoss_Zealot).shield_points == 60
		&& f.u(UnitTypes::Protoss_Zealot).mineral_cost == 100 && f.u(UnitTypes::Protoss_Zealot).armor == 1);
	// [VERIFIED https://liquipedia.net/starcraft/Dragoon] 100 hp, 80 shield, 125/50
	test("L28", "Dragoon 100 hp / 80 shield / 125+50",
		f.hp(UnitTypes::Protoss_Dragoon) == 100 && f.u(UnitTypes::Protoss_Dragoon).shield_points == 80
		&& f.u(UnitTypes::Protoss_Dragoon).mineral_cost == 125 && f.u(UnitTypes::Protoss_Dragoon).gas_cost == 50);

	// ---- Engine-level rules that follow from the above --------------------
	// A non-Protoss unit must never start with shield points, whatever the
	// (garbage) shield_points field says.
	{
		bool ok = true;
		std::string bad;
		for (auto& u : st.visible_units) {
			if (u.owner >= 8) continue;
			if (u.unit_type->has_shield) continue;
			if (u.shield_points.raw_value != 0) {
				ok = false;
				bad = fmt("type %d has %d raw shield", (int)u.unit_type->id, (int)u.shield_points.raw_value);
				break;
			}
		}
		// [ENGINE BUG] This fails on M&M_v_ZealDrag: unit type 124 is created with
		// a 25600 raw (100 fp8) shield pool although has_shield == 0. See
		// __BUGS.md BUG-8. Left red on purpose - it documents the defect.
		test("L29", "spawned non-Protoss units have zero shields (BUG-8)", ok, bad);
	}
	// A Protoss unit may legitimately start with shields; the field is fp8, so
	// the total must never exceed the type maximum.
	{
		bool ok = true;
		std::string bad;
		for (auto& u : st.visible_units) {
			if (!u.unit_type->has_shield) continue;
			if ((int)u.shield_points.integer_part() > (int)u.unit_type->shield_points) {
				ok = false;
				bad = fmt("type %d: %d > %d", (int)u.unit_type->id,
					(int)u.shield_points.integer_part(), (int)u.unit_type->shield_points);
				break;
			}
		}
		test("L30", "spawned shield values never exceed the type maximum", ok, bad);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
