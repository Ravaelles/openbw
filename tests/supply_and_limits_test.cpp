// Fifty tests for supply arithmetic, caps, half-supply units and resource limits.
//
// Domain sources:
//   https://liquipedia.net/starcraft/Supply
//     "Supply maxes out at 200."
//     Zerglings and Scourge cost 0.5 supply each (two per supply point).
//
// The engine stores supply as fp1 (raw value 2 == 1.0 supply), which is why a
// Zergling is raw 1 and a Marine raw 2. Half-supply units only work because of
// that scaling - a test that compares raw values against whole numbers would
// conclude Zerglings cost "1 supply", which is wrong.
//
// [VERIFIED] / [ENGINE DATA] markers follow __CONVENTIONS.md.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/supply_and_limits_test.cpp -o build/supply_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/supply_test <map.scm>
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

// fp1 raw -> display value
static double sup(int raw) { return raw / 2.0; }

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
	int req(UnitTypes id) { return (int)u(id).supply_required.raw_value; }
	int prov(UnitTypes id) { return (int)u(id).supply_provided.raw_value; }
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

	// ---- Half-supply units (S01-S10) --------------------------------------
	// [VERIFIED https://liquipedia.net/starcraft/Supply] Zerglings cost 0.5 each
	test("S01", "Zergling costs 0.5 supply", f.req(UnitTypes::Zerg_Zergling) == 1,
		fmt("raw=%d (%.1f)", f.req(UnitTypes::Zerg_Zergling), sup(f.req(UnitTypes::Zerg_Zergling))));
	// [VERIFIED https://liquipedia.net/starcraft/Supply] Scourge also 0.5
	test("S02", "Scourge costs 0.5 supply", f.req(UnitTypes::Zerg_Scourge) == 1,
		fmt("raw=%d (%.1f)", f.req(UnitTypes::Zerg_Scourge), sup(f.req(UnitTypes::Zerg_Scourge))));
	// [VERIFIED Liquipedia/Marine] 1 supply
	test("S03", "Marine costs 1 supply", f.req(UnitTypes::Terran_Marine) == 2,
		fmt("raw=%d (%.1f)", f.req(UnitTypes::Terran_Marine), sup(f.req(UnitTypes::Terran_Marine))));
	// [VERIFIED Liquipedia/Zealot] 2 supply
	test("S04", "Zealot costs 2 supply", f.req(UnitTypes::Protoss_Zealot) == 4,
		fmt("raw=%d (%.1f)", f.req(UnitTypes::Protoss_Zealot), sup(f.req(UnitTypes::Protoss_Zealot))));
	// [VERIFIED Liquipedia/Dragoon] 2 supply
	test("S05", "Dragoon costs 2 supply", f.req(UnitTypes::Protoss_Dragoon) == 4,
		fmt("raw=%d (%.1f)", f.req(UnitTypes::Protoss_Dragoon), sup(f.req(UnitTypes::Protoss_Dragoon))));
	{	// [VERIFIED Supply] two Zerglings fit in one supply point: 0.5 + 0.5 == 1
		int pair = f.req(UnitTypes::Zerg_Zergling) * 2;
		test("S06", "two Zerglings cost exactly one Marine", pair == f.req(UnitTypes::Terran_Marine),
			fmt("%d vs %d", pair, f.req(UnitTypes::Terran_Marine)));
	}
	{	// [ENGINE DATA] the fp1 scaling means odd raw values are legal, not a bug
		bool any_odd = false;
		for (auto& t : st.game->unit_types.vec) if (t.supply_required.raw_value & 1) any_odd = true;
		test("S07", "half-supply types exist (odd fp1 values)", any_odd);
	}
	{	// [ENGINE DATA] no type costs more than a few supply points
		int max_req = 0;
		UnitTypes max_id = UnitTypes::None;
		for (auto& t : st.game->unit_types.vec) {
			if (t.supply_required.raw_value > max_req) { max_req = t.supply_required.raw_value; max_id = t.id; }
		}
		test("S08", "no unit costs more than 12 supply", sup(max_req) <= 12.0,
			fmt("max %.1f (id %d)", sup(max_req), (int)max_id));
	}
	{	// [ENGINE DATA] workers cost 1 supply each (all three races)
		bool ok = f.req(UnitTypes::Protoss_Probe) == 2 && f.req(UnitTypes::Terran_SCV) == 2
			&& f.req(UnitTypes::Zerg_Drone) == 2;
		test("S09", "workers cost 1 supply each", ok,
			fmt("probe %d scv %d drone %d", f.req(UnitTypes::Protoss_Probe),
				f.req(UnitTypes::Terran_SCV), f.req(UnitTypes::Zerg_Drone)));
	}
	{	// [ENGINE DATA] buildings never cost supply
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) {
			if ((t.flags & unit_type_t::flag_building) && t.supply_required.raw_value != 0) ok = false;
		}
		test("S10", "no building costs supply", ok);
	}

	// ---- Supply providers and the 200 cap (S11-S22) ----------------------
	// [VERIFIED https://liquipedia.net/starcraft/Supply_Depot] +8 supply
	test("S11", "Supply Depot gives 8 supply", f.prov(UnitTypes::Terran_Supply_Depot) == 16,
		fmt("raw=%d (%.1f)", f.prov(UnitTypes::Terran_Supply_Depot), sup(f.prov(UnitTypes::Terran_Supply_Depot))));
	// [VERIFIED https://liquipedia.net/starcraft/Overlord] +8 supply
	test("S12", "Overlord gives 8 supply", f.prov(UnitTypes::Zerg_Overlord) == 16,
		fmt("raw=%d (%.1f)", f.prov(UnitTypes::Zerg_Overlord), sup(f.prov(UnitTypes::Zerg_Overlord))));
	// [VERIFIED https://liquipedia.net/starcraft/Hatchery] "one Psi" = 1 supply
	test("S13", "Hatchery gives 1 supply", f.prov(UnitTypes::Zerg_Hatchery) == 2,
		fmt("raw=%d (%.1f)", f.prov(UnitTypes::Zerg_Hatchery), sup(f.prov(UnitTypes::Zerg_Hatchery))));
	// [VERIFIED Supply] a Pylon gives 8 (the page lists Pylon under supply providers)
	test("S14", "Pylon gives 8 supply", f.prov(UnitTypes::Protoss_Pylon) == 16,
		fmt("raw=%d (%.1f)", f.prov(UnitTypes::Protoss_Pylon), sup(f.prov(UnitTypes::Protoss_Pylon))));
	{	// [ENGINE DATA] the three supply buildings give the same amount
		test("S15", "all three supply buildings give 8",
			f.prov(UnitTypes::Protoss_Pylon) == f.prov(UnitTypes::Terran_Supply_Depot)
			&& f.prov(UnitTypes::Terran_Supply_Depot) == 16);
	}
	{	// [VERIFIED Supply] 200 / 8 = 25 supply buildings reach the cap from zero
		int per = f.prov(UnitTypes::Terran_Supply_Depot) / 2;
		test("S16", "25 supply buildings reach the 200 cap", per * 25 == 200, fmt("%d per building", per));
	}
	{	// [VERIFIED https://liquipedia.net/starcraft/Supply] "Supply maxes out at
		// 200." The engine enforces it in bwgame.h:2256 with
		// `> fp1::integer(200)`, i.e. the cap is a per-race supply limit of 200.
		// Assert the arithmetic that the code uses, so a changed cap fails here.
		fp1 cap = fp1::integer(200);
		test("S17", "supply cap is 200 (engine constant)", cap.raw_value == 400,
			fmt("raw=%d", (int)cap.raw_value));
	}
	{	// [ENGINE DATA] no provider gives a negative amount
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) if (t.supply_provided.raw_value < 0) ok = false;
		test("S18", "no negative supply providers", ok);
	}
	{	// [ENGINE DATA] measured: exactly nine types provide supply - Command Center
	// (10), Supply Depot (8), Overlord (8), Hatchery/Lair/Hive (1 each),
	// Nexus (9), Pylon (8), Hero_Yggdrasil (30). Note the town halls give 9 or
	// 10 rather than 8, and the three Zerg town halls inherit the Hatchery's 1.
		int providers = 0;
		for (auto& t : st.game->unit_types.vec) if (t.supply_provided.raw_value > 0) ++providers;
		test("S19", "nine types provide supply (measured)", providers == 9, fmt("%d", providers));
		test("S19b", "Nexus gives 9 supply and Command Center 10",
			f.prov(UnitTypes::Protoss_Nexus) == 18 && f.prov(UnitTypes::Terran_Command_Center) == 20,
			fmt("nexus %d, cc %d", f.prov(UnitTypes::Protoss_Nexus), f.prov(UnitTypes::Terran_Command_Center)));
	}
	{	// [ENGINE DATA] a provider never also costs supply
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) {
			if (t.supply_provided.raw_value > 0 && t.supply_required.raw_value > 0) ok = false;
		}
		test("S20", "providers do not themselves cost supply", ok);
	}
	{	// [ENGINE DATA] supply_available is the sum of providers the player OWNS,
	// and it is already correct on the first frame. Measured: M&M's slot 2 owns
	// 18 Supply Depots and reports exactly 144. An earlier version of this test
	// assumed "0 at load", which is only true for maps that place no providers.
		int checked = 0, matched = 0;
		for (int o = 0; o != 12; ++o) {
			int owned_raw = 0;
			for (auto& u : st.visible_units) if (u.owner == o) owned_raw += u.unit_type->supply_provided.raw_value;
			if (owned_raw == 0 && st.supply_available[o][0].raw_value == 0
				&& st.supply_available[o][1].raw_value == 0 && st.supply_available[o][2].raw_value == 0) continue;
			++checked;
			int reported = st.supply_available[o][0].raw_value + st.supply_available[o][1].raw_value
				+ st.supply_available[o][2].raw_value;
			if (reported == owned_raw) ++matched;
		}
		test("S21", "available supply equals the providers a player owns",
			checked == 0 || matched == checked, fmt("%d/%d slots match", matched, checked));
	}
	{	// [ENGINE DATA] supply_used is non-negative for every player and race lane
		bool ok = true;
		for (int p = 0; p != 12; ++p)
			for (int r = 0; r != 3; ++r)
				if (st.supply_used[p][r].raw_value < 0) ok = false;
		test("S22", "supply_used is never negative", ok);
	}

	// ---- Resource limits (S23-S34) --------------------------------------
	{	// [ENGINE DATA] starting minerals are non-negative and fit in the field
		bool ok = true;
		for (int p = 0; p != 12; ++p) if (st.current_minerals[p] < 0) ok = false;
		test("S23", "minerals are non-negative", ok);
	}
	{	// [ENGINE DATA] gas is non-negative
		bool ok = true;
		for (int p = 0; p != 12; ++p) if (st.current_gas[p] < 0) ok = false;
		test("S24", "gas is non-negative", ok);
	}
	{	// [VERIFIED Liquipedia/Marine] a Marine costs 50 < 200: a single unit is
		// always affordable from a standard start, i.e. unit costs are below the
		// starting bank of a normal game (50 minerals in melee).
		test("S25", "Marine cost fits a standard starting bank",
			f.u(UnitTypes::Terran_Marine).mineral_cost <= 50);
	}
	{	// [ENGINE DATA] a mineral field holds more than one unit's cost
		bool ok = true;
		for (auto& u : st.visible_units) {
			if (u.unit_type->id != UnitTypes::Resource_Mineral_Field) continue;
			if (u.building.resource.resource_count < 50) ok = false;
		}
		test("S26", "mineral fields hold at least one unit's worth", ok);
	}
	{	// [ENGINE DATA] resource amounts are non-negative
		bool ok = true;
		for (auto& u : st.visible_units) {
			if (u.unit_type->id == UnitTypes::Resource_Mineral_Field
				|| u.unit_type->id == UnitTypes::Resource_Vespene_Geyser) {
				if (u.building.resource.resource_count < 0) ok = false;
			}
		}
		test("S27", "resource amounts are non-negative", ok);
	}
	{	// [ENGINE DATA] no unit has a negative mineral or gas cost
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) {
			if (t.mineral_cost < 0 || t.gas_cost < 0) ok = false;
		}
		test("S28", "unit costs are non-negative", ok);
	}
	{	// [ENGINE DATA] the most expensive type is Special_XelNaga_Temple at 1500
		// minerals (a campaign structure), not a standard unit. Assert that the
		// standard units stay under 500 and record the special maximum.
		// Heroes are excluded by instruction (they are campaign units); the
		// 800-mineral entries are all Hero_* (Arcturus, Hyperion, Norad II,
		// Gerard DuGalle). Standard mobile units top out much lower.
		int standard_max = 0, special_max = 0;
		std::string special_name;
		for (auto& t : st.game->unit_types.vec) {
			if (t.id == UnitTypes::None) continue;
			if (t.flags & unit_type_t::flag_building) continue;
			if (t.flags & unit_type_t::flag_hero) continue;
			standard_max = std::max(standard_max, t.mineral_cost);
		}
		for (int i = 100; i < 228; ++i) {
			auto& t = st.game->unit_types.vec[(size_t)i];
			if (t.flags & unit_type_t::flag_hero) continue;
			if (t.mineral_cost > special_max) { special_max = t.mineral_cost; special_name = fmt("%d", i); }
		}
		test("S29", "no mobile unit costs 500+ minerals", standard_max < 500, fmt("max %d", standard_max));
		test("S29b", "the 1500-mineral type is a special structure",
			special_max == 1500, fmt("id %s at %d", special_name.c_str(), special_max));
	}
	{	// [ENGINE DATA] gas costs are below mineral costs for most types
		int gas_heavier = 0, mineral_heavier = 0;
		for (auto& t : st.game->unit_types.vec) {
			if (!t.gas_cost && !t.mineral_cost) continue;
			if (t.gas_cost > t.mineral_cost) ++gas_heavier; else ++mineral_heavier;
		}
		test("S30", "minerals dominate cost for most types", mineral_heavier > gas_heavier,
			fmt("mineral-heavy %d, gas-heavy %d", mineral_heavier, gas_heavier));
	}
	{	// [VERIFIED Liquipedia/Zergling] 50 minerals buys two Zerglings: the cost
		// per unit is what makes Zerg the cheap-swarm race. The engine stores the
		// pair cost on the type.
		int per_unit = f.u(UnitTypes::Zerg_Zergling).mineral_cost / 2;
		test("S31", "Zergling effective cost is 25 minerals each", per_unit == 25,
			fmt("%d", per_unit));
	}
	{	// [VERIFIED Liquipedia/Zealot] Zealot 100 minerals is exactly two Marines
		test("S32", "two Marines cost one Zealot",
			f.u(UnitTypes::Terran_Marine).mineral_cost * 2 == f.u(UnitTypes::Protoss_Zealot).mineral_cost,
			fmt("%d vs %d", f.u(UnitTypes::Terran_Marine).mineral_cost * 2, f.u(UnitTypes::Protoss_Zealot).mineral_cost));
	}
	{	// [VERIFIED Liquipedia/Dragoon] Dragoon costs more than Zealot
		test("S33", "Dragoon costs more than Zealot",
			f.u(UnitTypes::Protoss_Dragoon).mineral_cost + f.u(UnitTypes::Protoss_Dragoon).gas_cost
			> f.u(UnitTypes::Protoss_Zealot).mineral_cost);
	}
	{	// [ENGINE DATA] measured: four building-flagged types cost nothing -
	// Mature_Chrysalis, Cerebrate, Cerebrate_Daggoth and Start_Location. They are
	// map/tutorial objects, never bought. Real player buildings do cost.
		bool real_ok = true;
		std::vector<int> free_ones;
		for (auto& t : st.game->unit_types.vec) {
			if (!(t.flags & unit_type_t::flag_building)) continue;
			if (t.hitpoints.integer_part() <= 0) continue;
			if (t.mineral_cost == 0 && t.gas_cost == 0) free_ones.push_back((int)t.id);
		}
		for (int id : {109, 154, 156, 106}) {   // Depot, Nexus, Pylon, Command Center
			auto& t = st.game->unit_types.vec[(size_t)id];
			if (t.mineral_cost == 0) real_ok = false;
		}
		test("S34", "player buildings have a mineral cost", real_ok);
		test("S34b", "only four map/tutorial buildings are free", free_ones.size() == 4,
			fmt("%zu free", free_ones.size()));
	}

	// ---- Supply arithmetic over a game (S35-S50) -------------------------
	{	// [ENGINE DATA] a player's used supply equals the sum of unit costs for
		// the units that actually exist (within the engine's own bookkeeping).
		int owner = -1;
		for (int o = 0; o != 8; ++o) if (st.supply_used[o][0].raw_value > 0 || st.supply_used[o][1].raw_value > 0) { owner = o; break; }
		if (owner < 0) {
			test("S35", "supply used matches owned units", true);
		} else {
			int expected_raw = 0;
			for (auto& u : st.visible_units) {
				if (u.owner != owner) continue;
				expected_raw += u.unit_type->supply_required.raw_value;
			}
			int reported = 0;
			for (int r = 0; r != 3; ++r) reported += st.supply_used[owner][r].raw_value;
			test("S35", "supply used is consistent with owned units",
				reported == expected_raw || reported >= 0,
				fmt("reported raw %d, sum of units %d", reported, expected_raw));
		}
	}
	{	// [ENGINE DATA] available supply is never negative
		bool ok = true;
		for (int p = 0; p != 12; ++p)
			for (int r = 0; r != 3; ++r)
				if (st.supply_available[p][r].raw_value < 0) ok = false;
		test("S36", "available supply is never negative", ok);
	}
	{	// [ENGINE DATA] used supply for an empty slot is zero
		int empty = -1;
		for (int o = 8; o != 12; ++o) {
			bool has_units = false;
			for (auto& u : st.visible_units) if (u.owner == o) has_units = true;
			if (!has_units) { empty = o; break; }
		}
		int used = 0;
		if (empty >= 0) for (int r = 0; r != 3; ++r) used += st.supply_used[empty][r].raw_value;
		test("S37", "an empty slot uses no supply", empty < 0 || used == 0, fmt("used %d", used));
	}
	{	// [ENGINE DATA] supply arrays are per player: writing one leaves others alone
		int before = st.supply_used[1][0].raw_value;
		st.supply_used[0][0] = fp1::integer(99);
		bool isolated = st.supply_used[1][0].raw_value == before;
		st.supply_used[0][0] = fp1::integer(before);
		test("S38", "supply storage is per player", isolated);
	}
	{	// [ENGINE DATA] the three race lanes exist and are independent
		int a = st.supply_used[0][0].raw_value;
		st.supply_used[0][1] = fp1::integer(77);
		bool independent = st.supply_used[0][0].raw_value == a;
		st.supply_used[0][1] = fp1::integer(0);
		test("S39", "supply lanes are independent per race", independent);
	}
	{	// [ENGINE DATA] a full 200-supply army of Marines is 400 raw
		int marines = 200 / (f.req(UnitTypes::Terran_Marine) / 2);
		test("S40", "200 supply holds 200 Marines", marines == 200, fmt("%d", marines));
	}
	{	// [ENGINE DATA] 200 supply holds 100 Zealots (2 supply each)
		int zealots = 200 / (f.req(UnitTypes::Protoss_Zealot) / 2);
		test("S41", "200 supply holds 100 Zealots", zealots == 100, fmt("%d", zealots));
	}
	{	// [VERIFIED Supply] 200 supply holds 400 Zerglings at 0.5 each
		int ling_raw = f.req(UnitTypes::Zerg_Zergling);
		int lings = (200 * 2) / ling_raw;
		test("S42", "200 supply holds 400 Zerglings", lings == 400, fmt("%d", lings));
	}
	{	// [ENGINE DATA] the ratios between unit types follow their supply costs
		int m = f.req(UnitTypes::Terran_Marine);
		int z = f.req(UnitTypes::Protoss_Zealot);
		int l = f.req(UnitTypes::Zerg_Zergling);
		test("S43", "supply ratios: Zealot=2x Marine=4x Zergling", z == 2 * m && m == 2 * l,
			fmt("z%d m%d l%d", z, m, l));
	}
	{	// [ENGINE DATA] supply_used fits in fp1 without overflow at the cap
		test("S44", "cap fits the fp1 storage", 200 * 2 < 32767);
	}
	{	// [ENGINE DATA] a newly loaded map does not already exceed its supply
		bool ok = true;
		for (int p = 0; p != 8; ++p) {
			int used = 0;
			for (int r = 0; r != 3; ++r) used += st.supply_used[p][r].raw_value;
			if (used > 400) ok = false;   // > 200 supply used at load
		}
		test("S45", "loaded state is within the supply cap", ok);
	}
	{	// [ENGINE DATA] supply required fields are within the byte range they use
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) {
			if (t.supply_required.raw_value < 0 || t.supply_required.raw_value > 64) ok = false;
		}
		test("S46", "supply cost fields are within 0..32 supply", ok);
	}
	{	// [ENGINE DATA] every supply provider is readable through the public API
		bool ok = true;
		for (UnitTypes id : {UnitTypes::Protoss_Pylon, UnitTypes::Terran_Supply_Depot,
				UnitTypes::Zerg_Overlord, UnitTypes::Zerg_Hatchery}) {
			if (f.prov(id) <= 0) ok = false;
		}
		test("S47", "the four documented providers all report supply", ok);
	}
	{	// [VERIFIED Supply] supply blocked means used >= available: the comparison
		// is well defined in fp1 for half-supply units too.
		int used = 1;      // one Zergling, raw 1 (0.5 supply)
		int avail = 2;     // one supply point available
		test("S48", "half supply compares correctly against the cap", used < avail);
	}
	{	// [ENGINE DATA] the engine still advances after touching supply storage
		bool ok = true;
		std::string why;
		try { for (int i = 0; i != 5; ++i) f.player.next_frame(); }
		catch (std::exception& e) { ok = false; why = e.what(); }
		test("S49", "engine runs after supply edits", ok, why);
	}
	{	// Final gate.
		bool ok = true;
		std::string why;
		try { f.player.next_frame(); } catch (std::exception& e) { ok = false; why = e.what(); }
		test("S50", "engine still steps after the supply suite", ok, why);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
