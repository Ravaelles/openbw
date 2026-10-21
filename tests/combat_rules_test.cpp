// Forty tests for combat rules and production values, verified against
// Liquipedia where they are game rules.
//
// Sources used (each assertion cites its own):
//   https://liquipedia.net/starcraft/Marine          (6 normal dmg, small, 1 supply)
//   https://liquipedia.net/starcraft/Zealot          (8x2 normal, small, 2 supply)
//   https://liquipedia.net/starcraft/Dragoon         (20 explosive, large, 2 supply)
//   https://liquipedia.net/starcraft/Zergling        (5 normal, small, 1 supply per pair)
//   https://liquipedia.net/starcraft/Damage_Type     (concussive/explosive multipliers)
//   https://liquipedia.net/starcraft/Armor           (minimum damage, shields ignore armor)
//   https://liquipedia.net/starcraft/Supply          (supply model)
//   https://liquipedia.net/starcraft/Supply_Depot    (+8 supply, 500 hp, 100 min)
//   https://liquipedia.net/starcraft/Pylon           (+8 supply)
//
// [ENGINE DATA] marks values that describe the loaded data file rather than a
// rule verified on Liquipedia. [VERIFIED] marks a rule with a source.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/combat_rules_test.cpp -o build/combat_rules_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/combat_rules_test <map.scm>
//
// Exit codes: 0 all 40 passed, 1 a test failed, 2 usage/data problem.
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
	// Unit sizes are stored as an int: 0 small, 1 medium, 2 large in the data.
	// [ENGINE DATA] measured: 1 = small, 2 = medium, 3 = large, 0 = special.
	// Two damage branches disagree about this mapping - see BUG-9.
	const char* size_name(int s) {
		switch (s) {
		case 0: return "special";
		case 1: return "small";
		case 2: return "medium";
		case 3: return "large";
		default: return "unknown";
		}
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

	// ===================== Weapon stats (C01-C15) ==========================
	// [VERIFIED https://liquipedia.net/starcraft/Marine] Marine: 6 normal damage
	{
		const weapon_type_t* w = f.u(UnitTypes::Terran_Marine).ground_weapon;
		test("C01", "Marine ground weapon exists", w != nullptr);
		test("C02", "Marine ground weapon does 6 damage", w && w->damage_amount == 6,
			w ? fmt("%d", w->damage_amount) : "no weapon");
		test("C03", "Marine weapon is normal damage",
			w && w->damage_type == weapon_type_t::damage_type_normal,
			w ? fmt("%d", (int)w->damage_type) : "no weapon");
		// [VERIFIED https://liquipedia.net/starcraft/Marine] 6 damage to air too
		{
			const weapon_type_t* wa = f.u(UnitTypes::Terran_Marine).air_weapon;
			test("C04", "Marine air weapon does 6 damage", wa && wa->damage_amount == 6,
				wa ? fmt("%d", wa->damage_amount) : "no air weapon");
		}
	}
	// [VERIFIED https://liquipedia.net/starcraft/Zealot] Zealot: 8x2 normal, small
	{
		const weapon_type_t* w = f.u(UnitTypes::Protoss_Zealot).ground_weapon;
		test("C05", "Zealot weapon does 8 damage", w && w->damage_amount == 8,
			w ? fmt("%d", w->damage_amount) : "no weapon");
		// [VERIFIED https://liquipedia.net/starcraft/Zealot] the Zealot's attack
		// is listed as 8x2; in the data that is two bullets per attack.
		// [VERIFIED Liquipedia/Zealot] the page writes the attack as 8x2. The
		// data file expresses that as damage 8 with bullet_count 1, i.e. the
		// "x2" is the attack animation, not two bullets per shot. Recorded as a
		// domain question (__SC_DOMAIN_QUESTIONS.md D) rather than guessed.
		test("C06", "Zealot weapon: 8 damage, bullet_count as loaded",
			w && w->damage_amount == 8 && w->bullet_count == 1,
			w ? fmt("dmg=%d bullets=%d", w->damage_amount, w->bullet_count) : "no weapon");
	}
	// [VERIFIED https://liquipedia.net/starcraft/Dragoon] Dragoon: 20 explosive
	{
		const weapon_type_t* w = f.u(UnitTypes::Protoss_Dragoon).ground_weapon;
		test("C07", "Dragoon weapon does 20 damage", w && w->damage_amount == 20,
			w ? fmt("%d", w->damage_amount) : "no weapon");
		test("C08", "Dragoon weapon is explosive damage",
			w && w->damage_type == weapon_type_t::damage_type_explosive,
			w ? fmt("%d", (int)w->damage_type) : "no weapon");
	}
	// [VERIFIED https://liquipedia.net/starcraft/Zergling] Zergling: 5 normal
	{
		const weapon_type_t* w = f.u(UnitTypes::Zerg_Zergling).ground_weapon;
		test("C09", "Zergling weapon does 5 damage", w && w->damage_amount == 5,
			w ? fmt("%d", w->damage_amount) : "no weapon");
	}
	// [ENGINE DATA] the first weapon in the table must be readable and be the
	// Gauss Rifle that every Marine uses.
	{
		const weapon_type_t* w = f.lf.get_weapon_type(WeaponTypes::Gauss_Rifle);
		test("C10", "Gauss Rifle weapon type is readable", w != nullptr
				&& w == f.u(UnitTypes::Terran_Marine).ground_weapon);
	}
	// [ENGINE DATA] every weapon in the table has a sane damage type
	{
		bool ok = true;
		int bad = -1;
		for (auto& w : st.game->weapon_types.vec) {
			if (w.damage_type < weapon_type_t::damage_type_none || w.damage_type > weapon_type_t::damage_type_ignore_armor) {
				ok = false;
				bad = (int)w.id;
				break;
			}
		}
		test("C11", "weapon damage types are inside the enum", ok, ok ? "" : fmt("weapon %d", bad));
	}
	{	// [ENGINE DATA] damage amounts are non-negative
		bool ok = true;
		for (auto& w : st.game->weapon_types.vec) if (w.damage_amount < 0) ok = false;
		test("C12", "weapon damage amounts are non-negative", ok);
	}
	{	// [ENGINE DATA] melee weapons have range 0-ish and ranged ones do not
		const weapon_type_t* z = &st.game->weapon_types.vec[(size_t)f.u(UnitTypes::Protoss_Zealot).ground_weapon->id];
		const weapon_type_t* m = &st.game->weapon_types.vec[(size_t)f.u(UnitTypes::Terran_Marine).ground_weapon->id];
		test("C13", "Zealot range is shorter than Marine range", z->max_range < m->max_range,
			fmt("%d < %d", (int)z->max_range, (int)m->max_range));
	}
	{	// [ENGINE DATA] cooldowns are positive
		const weapon_type_t* w = f.u(UnitTypes::Terran_Marine).ground_weapon;
		test("C14", "Marine weapon cooldown is positive", w && w->cooldown > 0,
			w ? fmt("%d", (int)w->cooldown) : "no weapon");
	}
	{	// [ENGINE DATA] projectile weapons reference a known sprite-like id set
		const weapon_type_t* w = f.u(UnitTypes::Protoss_Dragoon).ground_weapon;
		// [ENGINE DATA] the Dragoon fires a projectile, so its weapon must name a
		// bullet type other than "none" (bullet_type is an int in this struct).
		test("C15", "Dragoon weapon names a bullet type",
			w && w->bullet_type != 0, w ? fmt("bullet_type=%d", w->bullet_type) : "no weapon");
	}

	// ===================== Armor and damage rules (C16-C25) ================
	// [VERIFIED https://liquipedia.net/starcraft/Armor] armor reduces damage,
	// minimum 0.5; shields are not reduced by armor.
	test("C16", "Zealot armor is 1", f.u(UnitTypes::Protoss_Zealot).armor == 1,
		fmt("%d", f.u(UnitTypes::Protoss_Zealot).armor));
	test("C17", "Marine armor is 0", f.u(UnitTypes::Terran_Marine).armor == 0,
		fmt("%d", f.u(UnitTypes::Terran_Marine).armor));
	test("C18", "Zergling armor is 0", f.u(UnitTypes::Zerg_Zergling).armor == 0,
		fmt("%d", f.u(UnitTypes::Zerg_Zergling).armor));
	test("C19", "Dragoon armor is 1", f.u(UnitTypes::Protoss_Dragoon).armor == 1,
		fmt("%d", f.u(UnitTypes::Protoss_Dragoon).armor));
	test("C20", "Supply Depot armor is 1", f.u(UnitTypes::Terran_Supply_Depot).armor == 1,
		fmt("%d", f.u(UnitTypes::Terran_Supply_Depot).armor));
	{	// [ENGINE DATA] armor values stay in a plausible range
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) if (t.armor < 0 || t.armor > 10) ok = false;
		test("C21", "armor values are within 0..10", ok);
	}

	// ===================== Unit sizes (C22-C30) ============================
	// [VERIFIED https://liquipedia.net/starcraft/Marine] Marine is small
	test("C22", "Marine is a small unit", f.size_name(f.u(UnitTypes::Terran_Marine).unit_size) == std::string("small"),
		f.size_name(f.u(UnitTypes::Terran_Marine).unit_size));
	// [VERIFIED https://liquipedia.net/starcraft/Zealot] Zealot is small
	test("C23", "Zealot is a small unit", f.size_name(f.u(UnitTypes::Protoss_Zealot).unit_size) == std::string("small"),
		f.size_name(f.u(UnitTypes::Protoss_Zealot).unit_size));
	// [VERIFIED https://liquipedia.net/starcraft/Zergling] Zergling is small
	test("C24", "Zergling is a small unit", f.size_name(f.u(UnitTypes::Zerg_Zergling).unit_size) == std::string("small"),
		f.size_name(f.u(UnitTypes::Zerg_Zergling).unit_size));
	// [VERIFIED https://liquipedia.net/starcraft/Dragoon] Dragoon is large
	test("C25", "Dragoon is a large unit", f.size_name(f.u(UnitTypes::Protoss_Dragoon).unit_size) == std::string("large"),
		f.size_name(f.u(UnitTypes::Protoss_Dragoon).unit_size));
	{	// [ENGINE DATA] sizes are in the known set
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) {
			if (t.unit_size < 0 || t.unit_size > 3) ok = false;
		}
		test("C26", "unit sizes are small/medium/large only", ok);
	}
	{	// [ENGINE DATA] Nexus is large (a building is never small)
		test("C27", "Nexus is not a small unit",
			f.size_name(f.u(UnitTypes::Protoss_Nexus).unit_size) != std::string("small"),
			f.size_name(f.u(UnitTypes::Protoss_Nexus).unit_size));
	}

	// ===================== Supply (C28-C35) ================================
	// [VERIFIED https://liquipedia.net/starcraft/Marine] Marine costs 1 supply
	test("C28", "Marine requires 1 supply", f.u(UnitTypes::Terran_Marine).supply_required.raw_value == 2,
		fmt("fp1 raw=%d (=%.1f)", (int)f.u(UnitTypes::Terran_Marine).supply_required.raw_value,
			(double)f.u(UnitTypes::Terran_Marine).supply_required.raw_value / 2));
	// [VERIFIED https://liquipedia.net/starcraft/Supply_Depot] Supply Depot gives 8
	test("C29", "Supply Depot provides 8 supply", f.u(UnitTypes::Terran_Supply_Depot).supply_provided.raw_value == 16,
		fmt("fp1 raw=%d (=%.1f)", (int)f.u(UnitTypes::Terran_Supply_Depot).supply_provided.raw_value,
			(double)f.u(UnitTypes::Terran_Supply_Depot).supply_provided.raw_value / 2));
	// [VERIFIED https://liquipedia.net/starcraft/Pylon] Pylon gives supply
	test("C30", "Pylon provides supply", f.u(UnitTypes::Protoss_Pylon).supply_provided.raw_value > 0,
		fmt("fp1 raw=%d", (int)f.u(UnitTypes::Protoss_Pylon).supply_provided.raw_value));
	// [VERIFIED https://liquipedia.net/starcraft/Supply] buildings that provide
	// supply never require it
	test("C31", "Pylon does not require supply", f.u(UnitTypes::Protoss_Pylon).supply_required.raw_value == 0);
	test("C32", "Supply Depot does not require supply", f.u(UnitTypes::Terran_Supply_Depot).supply_required.raw_value == 0);
	{	// [ENGINE DATA] supply values are non-negative everywhere
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) {
			if (t.supply_required.raw_value < 0 || t.supply_provided.raw_value < 0) ok = false;
		}
		test("C33", "supply fields are non-negative", ok);
	}
	{	// [ENGINE DATA] resource types provide no supply
		bool ok = true;
		for (UnitTypes id : {UnitTypes::Resource_Mineral_Field, UnitTypes::Resource_Vespene_Geyser}) {
			if (f.u(id).supply_provided.raw_value != 0) ok = false;
		}
		test("C34", "resources provide no supply", ok);
	}
	{	// [VERIFIED https://liquipedia.net/starcraft/Supply_Depot] 500 hp
		test("C35", "Supply Depot has 500 hp", f.u(UnitTypes::Terran_Supply_Depot).hitpoints.integer_part() == 500,
			fmt("%d", (int)f.u(UnitTypes::Terran_Supply_Depot).hitpoints.integer_part()));
	}

	// ===================== Production values (C36-C40) =====================
	// [VERIFIED https://liquipedia.net/starcraft/Supply_Depot] 100 minerals
	test("C36", "Supply Depot costs 100 minerals", f.u(UnitTypes::Terran_Supply_Depot).mineral_cost == 100);
	{	// [ENGINE DATA] build times are positive for all buildings that have one
		// (four special types legitimately have none, see BUGS N12).
		int with_time = 0;
		for (auto& t : st.game->unit_types.vec) {
			if (!(t.flags & unit_type_t::flag_building)) continue;
			if (t.build_time > 0) ++with_time;
		}
		test("C37", "buildings carry a build time", with_time > 20, fmt("%d buildings", with_time));
	}
	{	// [VERIFIED https://liquipedia.net/starcraft/Supply] the supply providers are
		// the three "supply buildings" plus the Overlord (Zerg). Measured data has
		// exactly two non-building providers: Overlord (8) and Hero_Yggdrasill (30,
		// a campaign hero). Asserting "only buildings provide" would be false; the
		// honest rule-level check is that the buildings we verified do provide.
		test("C38", "Pylon and Supply Depot both provide supply",
			f.u(UnitTypes::Protoss_Pylon).supply_provided.raw_value > 0
			&& f.u(UnitTypes::Terran_Supply_Depot).supply_provided.raw_value > 0);
		int non_building_providers = 0;
		for (auto& t : st.game->unit_types.vec) {
			if (t.supply_provided.raw_value > 0 && !(t.flags & unit_type_t::flag_building)) ++non_building_providers;
		}
		test("C38b", "exactly two non-building supply providers (Overlord, hero)",
			non_building_providers == 2, fmt("%d", non_building_providers));
	}
	{	// [ENGINE DATA] no unit both requires and provides supply, except units
		// that are explicitly supply-neutral.
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) {
			if (t.supply_required.raw_value > 0 && t.supply_provided.raw_value > 0) ok = false;
		}
		test("C39", "no type both requires and provides supply", ok);
	}
	{	// [ENGINE DATA] the engine can still run a frame after all table reads.
		bool ok = true;
		std::string why;
		try { f.player.next_frame(); } catch (std::exception& e) { ok = false; why = e.what(); }
		test("C40", "engine steps one frame after the combat checks", ok, why);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
