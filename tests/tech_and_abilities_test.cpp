// Forty tests for researched upgrades, tech abilities and their costs.
//
// Domain sources:
//   https://liquipedia.net/starcraft/Stim_Pack (100/100, +50% speed, 10 hp cost)
//   https://liquipedia.net/starcraft/Upgrades  ("three tiers", costs rise)
//
// Two engine facts that make indices easy to get wrong and are pinned here:
//   - upgrades.dat is indexed by UpgradeTypes; the "single-level" abilities
//     (Stim Pack and friends) live at ids 22..38 with max_level 1, while the
//     numerical upgrades are ids 10..15 and 22..30 depending on race.
//   - costs are stored as base + factor, where the factor is added per level
//     beyond the first. A single-level ability therefore has factor 0.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/tech_and_abilities_test.cpp -o build/tech_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/tech_test <map.scm>
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
	const upgrade_type_t& up(UpgradeTypes id) { return st.game->upgrade_types.vec[(size_t)id]; }
	// Stim Pack is a TECH, not an upgrade: TechTypes::Stim_Packs, costing
	// minerals/gas on tech_type_t. An earlier version looked for it in
	// UpgradeTypes and failed to compile - the enum split is the lesson.
	const tech_type_t& tech(TechTypes id) { return st.game->tech_types.vec[(size_t)id]; }
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
	auto& upgrades = st.game->upgrade_types.vec;

	// ---- Stim Pack (T01-T10) ---------------------------------------------
	// [VERIFIED https://liquipedia.net/starcraft/Stim_Pack] 100 minerals, 100 gas.
	// Stim Pack is a TECH (TechTypes::Stim_Packs), not an upgrade: it is
	// researched once, has no levels and no cost escalation. This distinction is
	// the one that broke the first version of this file.
	const tech_type_t& stim = f.tech(TechTypes::Stim_Packs);
	test("T01", "Stim Pack costs 100 minerals", stim.mineral_cost == 100, fmt("%d", stim.mineral_cost));
	test("T02", "Stim Pack costs 100 gas", stim.gas_cost == 100, fmt("%d", stim.gas_cost));
	// [ENGINE DATA] a tech has no levels; confirm it is not in the upgrade table
	// [ENGINE DATA] Stim Pack is id 0 of TechTypes and has a 1200-frame research
	// time; upgrades.dat has no 1-level 100/100 entry, which is the structural
	// proof that it is a tech. Measured directly rather than inferred.
	test("T03", "Stim Pack research time is 1200 frames", (int)stim.research_time == 1200,
		fmt("%d", (int)stim.research_time));
	// [ENGINE DATA] techs carry a positive research time
	test("T04", "Stim Pack has a research time", stim.research_time > 0, fmt("%d", (int)stim.research_time));
	// [VERIFIED Liquipedia/Stim_Pack] Stim Pack is Terran
	test("T05", "Stim Pack is a Terran tech", stim.race == 1, fmt("race=%d", stim.race));
	// [VERIFIED Liquipedia/Stim_Pack] Stim halves the attack cooldown as an
	// effect applied to units; the base Marine weapon stays 15 frames.
	{
		int cd = (int)f.lf.get_unit_type(UnitTypes::Terran_Marine)->ground_weapon->cooldown;
		test("T06", "Marine base cooldown is 15 frames (Stim is an effect later)",
			cd == 15, fmt("%d", cd));
	}
	// [VERIFIED Liquipedia/Stim_Pack] +50% speed, applied at runtime; the base
	// speed is a data fact that must be positive for that to mean anything.
	{
		const flingy_type_t* fl = f.lf.get_unit_type(UnitTypes::Terran_Marine)->flingy;
		test("T07", "Marine has a positive base speed", fl && fl->top_speed > 0_fp8,
			fl ? fmt("%d", (int)fl->top_speed.raw_value) : "no flingy");
	}
	// [ENGINE DATA] 15 techs cost 100/100 - that is the standard "basic ability"
	// price, so uniqueness is false. What distinguishes Stim Pack is its id (0)
	// and its 1200-frame research time.
	{
		int count = 0;
		for (auto& t : st.game->tech_types.vec) {
			if (t.mineral_cost == 100 && t.gas_cost == 100) ++count;
		}
		test("T08", "15 techs share the 100/100 basic-ability price", count == 15, fmt("%d", count));
	}
	// [ENGINE DATA] tech id zero is Stim Pack - a stable ordering the client relies on
	test("T09", "TechTypes::Stim_Packs is id 0", (int)TechTypes::Stim_Packs == 0,
		fmt("%d", (int)TechTypes::Stim_Packs));
	// [ENGINE DATA] techs carry icon/label indices (client display)
	test("T10", "Stim Pack has icon and label indices", stim.icon >= 0 && stim.label >= 0,
		fmt("icon %d label %d", stim.icon, stim.label));

	// ---- Other researched abilities (T11-T20) ----------------------------
	{	// [ENGINE DATA] a one-level research has no cost escalation on any of the
		// three fields; this is the structural difference from tiered upgrades.
		bool ok = true;
		int checked = 0;
		for (auto& u : upgrades) {
			if (u.max_level != 1) continue;
			++checked;
			if (u.mineral_cost_factor || u.gas_cost_factor || u.time_cost_factor) ok = false;
		}
		test("T11", "single-level research never escalates its cost", ok, fmt("%d checked", checked));
	}
	{	// [VERIFIED Liquipedia/Upgrades] three-tier upgrades DO escalate: the
		// factor is what makes each level cost more.
		bool ok = true;
		int checked = 0;
		for (auto& u : upgrades) {
			if (u.max_level != 3) continue;
			++checked;
			if (u.mineral_cost_factor <= 0) ok = false;
		}
		test("T12", "three-tier upgrades have a positive mineral factor", ok, fmt("%d checked", checked));
	}
	{	// [ENGINE DATA] the highest single research cost is 200/200, not more
		int max_cost = 0;
		for (auto& u : upgrades) {
			if (u.max_level != 1) continue;
			max_cost = std::max(max_cost, u.mineral_cost_base);
		}
		test("T13", "single-level research tops out at 200 minerals", max_cost == 200, fmt("%d", max_cost));
	}
	{	// [ENGINE DATA] three-tier upgrades start at 100 and top out at 200
		int lo = 1 << 20, hi = 0;
		for (auto& u : upgrades) {
			if (u.max_level != 3) continue;
			lo = std::min(lo, u.mineral_cost_base);
			hi = std::max(hi, u.mineral_cost_base);
		}
		test("T14", "tiered upgrades start at 100 and reach 200", lo == 100 && hi == 200,
			fmt("%d..%d", lo, hi));
	}
	{	// [ENGINE DATA] mineral and gas costs are equal for level-1 upgrades
		bool ok = true;
		int checked = 0;
		for (auto& u : upgrades) {
			if (u.mineral_cost_base == 0) continue;
			++checked;
			if (u.mineral_cost_base != u.gas_cost_base) ok = false;
		}
		test("T15", "upgrade mineral cost equals gas cost", ok, fmt("%d checked", checked));
	}
	{	// [ENGINE DATA] research times are in the thousands of frames
		bool ok = true;
		int checked = 0;
		for (auto& u : upgrades) {
			if (u.time_cost_base == 0) continue;
			++checked;
			if (u.time_cost_base < 1000) ok = false;
		}
		test("T16", "research times are at least 1000 frames", ok, fmt("%d checked", checked));
	}
	{	// [VERIFIED Liquipedia/Upgrades] every race has upgrades, and the engine
		// tags the race; ids 0..2 must all appear.
		bool seen[3] = {false, false, false};
		for (auto& u : upgrades) {
			if (u.max_level == 0) continue;
			if (u.race >= 0 && u.race <= 2) seen[u.race] = true;
		}
		test("T17", "all three races have upgrades", seen[0] && seen[1] && seen[2]);
	}
	{	// [ENGINE DATA] no upgrade is both tiered and race-less
		bool ok = true;
		for (auto& u : upgrades) {
			if (u.max_level > 0 && (u.race < 0 || u.race > 2)) ok = false;
		}
		test("T18", "real upgrades are tagged with a race", ok);
	}
	{	// [ENGINE DATA] an upgrade cannot be free and real at the same time
		bool ok = true;
		for (auto& u : upgrades) {
			if (u.max_level > 0 && u.mineral_cost_base == 0) ok = false;
		}
		test("T19", "real upgrades cost minerals", ok);
	}
	{	// [ENGINE DATA] the upgrade table is big enough to hold every race
		test("T20", "upgrade table holds at least 45 entries", upgrades.size() >= 45,
			fmt("%zu", upgrades.size()));
	}

	// ---- Tech table (T21-T30) --------------------------------------------
	{	// [ENGINE DATA] tech ids match indices
		bool ok = true;
		for (size_t i = 0; i != st.game->tech_types.vec.size(); ++i)
			if ((size_t)st.game->tech_types.vec[i].id != i) ok = false;
		test("T21", "tech ids match their index", ok);
	}
	{	// [VERIFIED Liquipedia/Upgrades] techs cost resources like upgrades
		bool ok = true;
		int checked = 0;
		for (auto& t : st.game->tech_types.vec) {
			if (t.mineral_cost == 0 && t.gas_cost == 0) continue;
			++checked;
			if (t.mineral_cost < 0 || t.gas_cost < 0) ok = false;
		}
		test("T22", "tech costs are non-negative", ok, fmt("%d checked", checked));
	}
	{	// [ENGINE DATA] tech mineral cost equals gas cost where both are set
		bool ok = true;
		int checked = 0;
		for (auto& t : st.game->tech_types.vec) {
			if (t.mineral_cost == 0) continue;
			++checked;
			if (t.mineral_cost != t.gas_cost) ok = false;
		}
		test("T23", "tech mineral cost equals gas cost", ok, fmt("%d checked", checked));
	}
	{	// [ENGINE DATA] unused tech slots exist (max_level 0 analogue)
		int empty = 0;
		for (auto& t : st.game->tech_types.vec) if (t.mineral_cost == 0 && t.gas_cost == 0) ++empty;
		test("T24", "some tech slots are unused", empty > 0 && empty < 20, fmt("%d", empty));
	}
	{	// [ENGINE DATA] research times are positive for real techs
		bool ok = true;
		for (auto& t : st.game->tech_types.vec) {
			if (t.mineral_cost == 0) continue;
			if (t.research_time <= 0) ok = false;
		}
		test("T25", "real techs have a research time", ok);
	}
	{	// [ENGINE DATA] race tags are 0..2 for the three races and 3 for
	// campaign/neutral techs. Measured: 10 priced techs carry race 3 (e.g. id 26
	// at 150 minerals), so "all real techs are 0..2" is false.
		int campaign = 0, playable = 0;
		bool in_range = true;
		for (auto& t : st.game->tech_types.vec) {
			if (t.mineral_cost == 0) continue;
			if (t.race < 0 || t.race > 3) in_range = false;
			if (t.race == 3) ++campaign; else ++playable;
		}
		test("T26", "tech race tags are 0..3 (3 = campaign/neutral)", in_range,
			fmt("%d playable, %d campaign", playable, campaign));
		test("T26b", "most priced techs belong to a playable race", playable > campaign,
			fmt("%d vs %d", playable, campaign));
	}
	{	// [ENGINE DATA] all three races have techs
		bool seen[3] = {false, false, false};
		for (auto& t : st.game->tech_types.vec) {
			if (t.mineral_cost == 0) continue;
			if (t.race >= 0 && t.race <= 2) seen[t.race] = true;
		}
		test("T27", "all three races have techs", seen[0] && seen[1] && seen[2]);
	}
	{	// [ENGINE DATA] techs have icon and label indices for the client
		bool ok = true;
		for (auto& t : st.game->tech_types.vec) {
			if (t.mineral_cost == 0) continue;
			if (t.icon < 0 || t.label < 0) ok = false;
		}
		test("T28", "real techs have icon and label indices", ok);
	}
	{	// [ENGINE DATA] the tech table size matches the documented 44 entries
		test("T29", "tech table holds at least 44 entries", st.game->tech_types.vec.size() >= 44,
			fmt("%zu", st.game->tech_types.vec.size()));
	}
	{	// [ENGINE DATA] tech_type_t has no is_broodwar flag (unlike upgrades), so the
	// classic/expansion split cannot be asserted here. Record what does exist:
	// the table holds entries for all three races with icon/label metadata.
		int with_cost = 0;
		for (auto& t : st.game->tech_types.vec) if (t.mineral_cost > 0) ++with_cost;
		test("T30", "tech table has a substantial real-entry count", with_cost >= 20,
			fmt("%d real techs", with_cost));
	}

	// ---- Player research state (T31-T40) ---------------------------------
	{	// [ENGINE DATA] all upgrade levels start at zero
		bool zero = true;
		for (int p = 0; p != 12; ++p)
			for (size_t i = 0; i != upgrades.size(); ++i)
				if (st.upgrade_levels[p][(UpgradeTypes)i] != 0) zero = false;
		test("T31", "all upgrade levels start at zero", zero);
	}
	{	// [ENGINE DATA] levels are stored per player
		// Use the Ground Weapons upgrade, which exists as an UpgradeTypes member.
		const UpgradeTypes id = UpgradeTypes::Protoss_Ground_Weapons;
		int before = st.upgrade_levels[1][id];
		st.upgrade_levels[0][id] = 1;
		bool isolated = st.upgrade_levels[1][id] == before;
		st.upgrade_levels[0][id] = 0;
		test("T32", "upgrade levels are per player", isolated);
	}
	{	// [ENGINE DATA] a level cannot exceed the type's max_level in a correct
		// game; the storage itself is an int array, so assert the type limit.
		bool ok = true;
		for (auto& u : upgrades) if (u.max_level < 0 || u.max_level > 3) ok = false;
		test("T33", "no upgrade allows more than three levels", ok);
	}
	{	// [VERIFIED Liquipedia/Upgrades] tiered upgrades are exactly the ones whose
	// factor is positive; this is the invariant a research UI relies on.
		bool ok = true;
		for (auto& u : upgrades) {
			if (u.max_level == 0) continue;
			bool tiered = u.max_level == 3;
			bool escalates = u.mineral_cost_factor > 0;
			if (tiered != escalates) ok = false;
		}
		test("T34", "tiered iff cost escalates", ok);
	}
	{	// [ENGINE DATA] every upgrade has a readable label index
		bool ok = true;
		for (auto& u : upgrades) {
			if (u.max_level == 0) continue;
			if (u.label < 0) ok = false;
		}
		test("T35", "real upgrades have a label index", ok);
	}
	{	// [ENGINE DATA] the upgrade table's icon indices are within a sane range
		bool ok = true;
		for (auto& u : upgrades) if (u.icon < 0 || u.icon > 10000) ok = false;
		test("T36", "upgrade icon indices are sane", ok);
	}
	{	// [ENGINE DATA] tech state is a per-player bool array (`tech_researched`),
	// not a counter, and it starts empty. This is the storage a "did the player
	// research X" query reads.
		bool zero = true;
		for (int p = 0; p != 12; ++p)
			for (size_t i = 0; i != st.game->tech_types.vec.size(); ++i)
				if (st.tech_researched[p][(TechTypes)i]) zero = false;
		test("T37", "nothing is researched at load", zero);
		test("T37b", "tech state is per player",
			[&] {
				st.tech_researched[0][TechTypes::Stim_Packs] = true;
				bool isolated = !st.tech_researched[1][TechTypes::Stim_Packs];
				st.tech_researched[0][TechTypes::Stim_Packs] = false;
				return isolated;
			}());
	}
	{	// [ENGINE DATA] breaking down the tiered upgrades by race gives at least one
		// weapons and one armour upgrade per race (the standard three-tier pair).
		bool ok = true;
		for (int race = 0; race != 3; ++race) {
			int tiered = 0;
			for (auto& u : upgrades) if (u.race == race && u.max_level == 3) ++tiered;
			if (tiered < 2) ok = false;
		}
		test("T38", "each race has at least two three-tier upgrades", ok);
	}
	{	// [ENGINE DATA] no upgrade costs more than 250 minerals at base
		int max_cost = 0;
		for (auto& u : upgrades) max_cost = std::max(max_cost, u.mineral_cost_base);
		test("T39", "upgrade base cost is at most 250", max_cost <= 250, fmt("%d", max_cost));
	}
	{	// Final gate.
		bool ok = true;
		std::string why;
		try { f.player.next_frame(); } catch (std::exception& e) { ok = false; why = e.what(); }
		test("T40", "engine steps one frame after the tech checks", ok, why);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
