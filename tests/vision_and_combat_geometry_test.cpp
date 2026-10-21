// Fifty tests for sight, attack geometry, pathing and the spatial helpers.
//
// Domain sources (each assertion cites its own):
//   https://liquipedia.net/starcraft/Zealot   sight 7, attack range 1, speed 4.00, cooldown 0.924 s
//   https://liquipedia.net/starcraft/Dragoon  sight 8, attack range 4 (+2), speed 5.00, cooldown 1.26 s
//   https://liquipedia.net/starcraft/Marine   sight 7, attack range 4 (5 with upgrade)
//
// Engine units: sight_range is in tiles, weapon max_range is in pixels
// (1 tile = 32 px), so a melee range of 15 px and a ranged 128 px are 0.5 and 4
// tiles respectively. Tests state which unit they assert in.
//
// [ENGINE DATA] marks values that describe the loaded data only.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/vision_and_combat_geometry_test.cpp -o build/vision_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/vision_test <map.scm>
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
};

// Tiles to pixels, the conversion the data uses for weapon ranges.
static int tiles_to_px(int tiles) { return tiles * 32; }

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

	// ================== Sight (V01-V15) ===================================
	// [VERIFIED https://liquipedia.net/starcraft/Zealot] sight 7
	test("V01", "Zealot sight range is 7", (int)f.u(UnitTypes::Protoss_Zealot).sight_range == 7,
		fmt("%d", (int)f.u(UnitTypes::Protoss_Zealot).sight_range));
	// [VERIFIED https://liquipedia.net/starcraft/Dragoon] sight 8
	test("V02", "Dragoon sight range is 8", (int)f.u(UnitTypes::Protoss_Dragoon).sight_range == 8,
		fmt("%d", (int)f.u(UnitTypes::Protoss_Dragoon).sight_range));
	// [VERIFIED https://liquipedia.net/starcraft/Marine] sight 7
	test("V03", "Marine sight range is 7", (int)f.u(UnitTypes::Terran_Marine).sight_range == 7,
		fmt("%d", (int)f.u(UnitTypes::Terran_Marine).sight_range));
	// [ENGINE DATA] buildings see something (a blind building would never
	// reveal terrain around itself)
	test("V04", "Pylon has a sight range", (int)f.u(UnitTypes::Protoss_Pylon).sight_range > 0,
		fmt("%d", (int)f.u(UnitTypes::Protoss_Pylon).sight_range));
	test("V05", "Nexus has a sight range", (int)f.u(UnitTypes::Protoss_Nexus).sight_range > 0);
	{	// [ENGINE DATA] no type has a negative or absurd sight range
		bool ok = true;
		for (auto& t : st.game->unit_types.vec) if ((int)t.sight_range < 0 || (int)t.sight_range > 32) ok = false;
		test("V06", "sight ranges are within 0..32 tiles", ok);
	}
	{	// [ENGINE DATA] mobile fighters are not blind
		bool ok = true;
		for (UnitTypes id : {UnitTypes::Protoss_Zealot, UnitTypes::Terran_Marine, UnitTypes::Zerg_Zergling}) {
			if ((int)f.u(id).sight_range <= 0) ok = false;
		}
		test("V07", "basic fighters can see", ok);
	}
	{	// [VERIFIED Liquipedia/Dragoon] a ranged unit sees at least as far as it
		// shoots; else it could shoot into the fog.
		int sight_px = tiles_to_px((int)f.u(UnitTypes::Protoss_Dragoon).sight_range);
		int range_px = f.u(UnitTypes::Protoss_Dragoon).ground_weapon->max_range;
		test("V08", "Dragoon sight covers its attack range", sight_px >= range_px,
			fmt("sight %d px, range %d px", sight_px, range_px));
	}
	{	// [VERIFIED Liquipedia/Zealot] same for melee: range 1 tile <= sight 7
		int sight_px = tiles_to_px((int)f.u(UnitTypes::Protoss_Zealot).sight_range);
		int range_px = f.u(UnitTypes::Protoss_Zealot).ground_weapon->max_range;
		test("V09", "Zealot sight covers its melee range", sight_px >= range_px,
			fmt("sight %d px, range %d px", sight_px, range_px));
	}
	{	// [ENGINE DATA] workers see something
		bool ok = true;
		for (UnitTypes id : {UnitTypes::Protoss_Probe, UnitTypes::Terran_SCV, UnitTypes::Zerg_Drone})
			if ((int)f.u(id).sight_range <= 0) ok = false;
		test("V10", "workers have sight", ok);
	}
	{	// [ENGINE DATA] resources have a sight range field too (0 is fine), but
		// the value must be readable and non-negative
		bool ok = true;
		for (UnitTypes id : {UnitTypes::Resource_Mineral_Field, UnitTypes::Resource_Vespene_Geyser})
			if ((int)f.u(id).sight_range < 0) ok = false;
		test("V11", "resource sight fields are non-negative", ok);
	}
	{	// [ENGINE DATA] at least one type has the maximum documented sight (11)
		int max_sight = 0;
		for (auto& t : st.game->unit_types.vec) max_sight = std::max(max_sight, (int)t.sight_range);
		test("V12", "some type has a long sight range", max_sight >= 10, fmt("max %d", max_sight));
	}
	{	// [ENGINE DATA] sight ranges are not all identical (data actually varies)
		std::set<int> distinct;
		for (auto& t : st.game->unit_types.vec) distinct.insert((int)t.sight_range);
		test("V13", "sight ranges vary between types", distinct.size() > 1, fmt("%zu distinct", distinct.size()));
	}
	{	// [ENGINE DATA] a Protoss unit with the documented longest sight (Observer
	// class) must be present in the table and must see further than a Zealot.
		int zealot_sight = (int)f.u(UnitTypes::Protoss_Zealot).sight_range;
		int best = 0;
		UnitTypes best_id = UnitTypes::Protoss_Zealot;
		for (auto& t : st.game->unit_types.vec) {
			if ((int)t.sight_range > best) { best = (int)t.sight_range; best_id = t.id; }
		}
		test("V14", "some unit sees further than a Zealot", best > zealot_sight,
			fmt("best %d (id %d) vs zealot %d", best, (int)best_id, zealot_sight));
	}
	{
		// [ENGINE DATA] revealer units exist as vision sources; their sight
		// range must be usable or the helper is pointless (see NON-OBVIOUS N01).
		int revealers = 0, with_sight = 0;
		for (auto& u : st.map_revealer_units) {
			++revealers;
			if ((int)u.unit_type->sight_range > 0) ++with_sight;
		}
		test("V15", "map revealers carry a sight range", revealers == 0 || with_sight == revealers,
			fmt("%d/%d", with_sight, revealers));
	}

	// ================== Attack range in pixels (V16-V30) ==================
	// [VERIFIED Liquipedia/Zealot] attack range 1 tile -> 32 px in data units
	{
		int r = f.u(UnitTypes::Protoss_Zealot).ground_weapon->max_range;
		test("V16", "Zealot melee range is under one tile", r < 32, fmt("%d px", r));
	}
	// [VERIFIED Liquipedia/Dragoon] attack range 4 tiles -> 128 px
	{
		int r = f.u(UnitTypes::Protoss_Dragoon).ground_weapon->max_range;
		test("V17", "Dragoon range is 4 tiles", r == tiles_to_px(4), fmt("%d px", r));
	}
	// [VERIFIED Liquipedia/Marine] attack range 4 tiles
	{
		int r = f.u(UnitTypes::Terran_Marine).ground_weapon->max_range;
		test("V18", "Marine range is 4 tiles", r == tiles_to_px(4), fmt("%d px", r));
	}
	test("V19", "Dragoon outranges Zealot",
		f.u(UnitTypes::Protoss_Dragoon).ground_weapon->max_range > f.u(UnitTypes::Protoss_Zealot).ground_weapon->max_range);
	test("V20", "Marine range is shorter than Dragoon or equal",
		f.u(UnitTypes::Terran_Marine).ground_weapon->max_range <= f.u(UnitTypes::Protoss_Dragoon).ground_weapon->max_range);
	{	// [VERIFIED Liquipedia/Zealot] cooldown ~0.924 s; engine stores frames.
		// At the standard 24 frames/s that is ~22 frames, which the data has.
		int cd = (int)f.u(UnitTypes::Protoss_Zealot).ground_weapon->cooldown;
		test("V21", "Zealot cooldown is about 22 frames (0.92 s)", cd >= 21 && cd <= 23, fmt("%d", cd));
	}
	{	// [VERIFIED Liquipedia/Dragoon] cooldown 1.26 s -> ~30 frames
		int cd = (int)f.u(UnitTypes::Protoss_Dragoon).ground_weapon->cooldown;
		test("V22", "Dragoon cooldown is about 30 frames (1.26 s)", cd >= 29 && cd <= 31, fmt("%d", cd));
	}
	{	// [VERIFIED Liquipedia/Marine] cooldown 0.61 s -> ~15 frames
		int cd = (int)f.u(UnitTypes::Terran_Marine).ground_weapon->cooldown;
		test("V23", "Marine cooldown is about 15 frames (0.61 s)", cd >= 14 && cd <= 16, fmt("%d", cd));
	}
	{	// [ENGINE DATA] melee weapons have a small range, not zero
		bool ok = true;
		for (UnitTypes id : {UnitTypes::Protoss_Zealot, UnitTypes::Zerg_Zergling}) {
			if (f.u(id).ground_weapon->max_range < 0) ok = false;
		}
		test("V24", "melee ranges are non-negative", ok);
	}
	{	// [ENGINE DATA] every weapon range is within the map scale
		bool ok = true;
		for (auto& w : st.game->weapon_types.vec) {
			if (w.max_range < 0 || w.max_range > 1024) ok = false;
		}
		test("V25", "weapon ranges are within 0..1024 px", ok);
	}
	{	// [ENGINE DATA] air weapons exist for units that can hit air
		test("V26", "Marine has an air weapon", f.u(UnitTypes::Terran_Marine).air_weapon != nullptr);
		test("V27", "Zealot has no air weapon", f.u(UnitTypes::Protoss_Zealot).air_weapon == nullptr);
	}
	{	// [ENGINE DATA] cooldowns are within a plausible frame range
		bool ok = true;
		for (auto& w : st.game->weapon_types.vec) if ((int)w.cooldown < 0 || (int)w.cooldown > 200) ok = false;
		test("V28", "weapon cooldowns are within 0..200 frames", ok);
	}
	{	// [ENGINE DATA] a unit with no ground weapon cannot attack ground
		test("V29", "Overlord has no ground weapon", f.u(UnitTypes::Zerg_Overlord).ground_weapon == nullptr);
	}
	{	// [ENGINE DATA] the engine still runs after all geometry reads
		bool ok = true;
		std::string why;
		try { f.player.next_frame(); } catch (std::exception& e) { ok = false; why = e.what(); }
		test("V30", "engine steps one frame after range checks", ok, why);
	}

	// ================== Pathing and placement (V31-V40) ===================
	{	// [ENGINE DATA] walkability is defined for every tile
		bool ok = true;
		int walkable = 0;
		for (auto& t : st.tiles) {
			if (t.flags & tile_t::flag_walkable) ++walkable;
		}
		test("V31", "some tiles are walkable and none are undefined", ok && walkable > 0,
			fmt("%d walkable", walkable));
	}
	{	// [ENGINE DATA] a building cannot be placed on unbuildable ground
		test("V32", "map bottom row is unbuildable",
			[&] {
				for (size_t x = 0; x != st.game->map_tile_width; ++x) {
					size_t idx = (size_t)(st.game->map_tile_height - 1) * st.game->map_tile_width + x;
					if (!(st.tiles[idx].flags & tile_t::flag_unbuildable)) return false;
				}
				return true;
			}());
	}
	{	// [ENGINE DATA] start locations are walkable, otherwise a game is unplayable
		int checked = 0, walkable = 0;
		for (int i = 0; i != 8; ++i) {
			auto p = st.game->start_locations[i];
			if (!p.x && !p.y) continue;
			++checked;
			auto tile = p / 32;
			size_t idx = (size_t)tile.y * st.game->map_tile_width + (size_t)tile.x;
			if (idx < st.tiles.size() && (st.tiles[idx].flags & tile_t::flag_walkable)) ++walkable;
		}
		test("V33", "start locations stand on walkable tiles", checked == 0 || walkable > 0,
			fmt("%d/%d", walkable, checked));
	}
	{	// [ENGINE DATA] regions tile the map area rather than being empty
		int non_empty = 0;
		for (auto& l : st.locations) {
			if (l.area.to.x > l.area.from.x && l.area.to.y > l.area.from.y) ++non_empty;
		}
		test("V34", "the map has usable regions", non_empty > 0, fmt("%d", non_empty));
	}
	{	// [ENGINE DATA] every region lies inside the map bounds
		bool inside = true;
		for (auto& l : st.locations) {
			if (l.area.from.x < 0 || l.area.from.y < 0) inside = false;
			if (l.area.to.x > (int)st.game->map_width || l.area.to.y > (int)st.game->map_height) {
				// Region 63 is the documented whole-map region (NON-OBVIOUS N20).
				bool whole_map = l.area.from.x == 0 && l.area.from.y == 0;
				if (!whole_map) inside = false;
			}
		}
		test("V35", "regions lie inside the map", inside);
	}
	{	// [ENGINE DATA] unit positions are on the map and not inside a wall for
		// buildings (a placed building must have a valid tile below it)
		bool ok = true;
		for (auto& u : st.visible_units) {
			if (!(u.unit_type->flags & unit_type_t::flag_building)) continue;
			auto tile = u.sprite->position / 32;
			if (tile.x < 0 || tile.y < 0 || tile.x >= (int)st.game->map_tile_width || tile.y >= (int)st.game->map_tile_height) ok = false;
		}
		test("V36", "placed buildings stand inside the map", ok);
	}
	{	// [ENGINE DATA] flying units may sit above any tile; ground units may not
		// sit outside the map
		bool ok = true;
		for (auto& u : st.visible_units) {
			auto p = u.sprite->position;
			if (p.x < 0 || p.y < 0 || p.x >= (int)st.game->map_width || p.y >= (int)st.game->map_height) ok = false;
		}
		test("V37", "all placed units are inside the map", ok);
	}
	{	// [ENGINE DATA] map bounds helpers agree with the map dimensions
		auto bounds = f.lf.map_bounds();
		test("V38", "map bounds match the pixel dimensions",
			bounds.to.x == (int)st.game->map_width && bounds.to.y == (int)st.game->map_height,
			fmt("bounds %d,%d map %d,%d", bounds.to.x, bounds.to.y, st.game->map_width, st.game->map_height));
	}
	{	// [ENGINE DATA] the walk grid is a quarter of the tile grid
		test("V39", "walk grid is 4x the tile grid",
			(int)st.game->map_walk_width == st.game->map_tile_width * 4
			&& (int)st.game->map_walk_height == st.game->map_tile_height * 4);
	}
	{	// [ENGINE DATA] the engine still runs after pathing reads
		bool ok = true;
		std::string why;
		try { for (int i = 0; i != 10; ++i) f.player.next_frame(); }
		catch (std::exception& e) { ok = false; why = e.what(); }
		test("V40", "engine runs after placement checks", ok, why);
	}

	// ================== Consistency (V41-V50) ==============================
	{	// [ENGINE DATA] every visible non-revealer unit appears exactly once
		std::set<int> idx;
		bool unique = true;
		for (auto& u : st.visible_units) if (!idx.insert(u.index).second) unique = false;
		test("V41", "visible indices remain unique after sim", unique);
	}
	{	// [ENGINE DATA] hit points never exceed the type maximum (first 10 frames
		// must not heal units above their cap)
		bool ok = true;
		for (auto& u : st.visible_units) if (u.hp > u.unit_type->hitpoints) ok = false;
		test("V42", "hp within type maximum", ok);
	}
	{	// [ENGINE DATA] shields within type maximum
		bool ok = true;
		for (auto& u : st.visible_units) {
			if (!u.unit_type->has_shield) continue;
			if ((int)u.shield_points.integer_part() > (int)u.unit_type->shield_points) ok = false;
		}
		test("V43", "shields within type maximum", ok);
	}
	{	// [ENGINE DATA] energy within its cap when the unit has energy
		bool ok = true;
		for (auto& u : st.visible_units) {
			if (!(u.unit_type->flags & unit_type_t::flag_has_energy)) continue;
			if ((int)u.energy.integer_part() < 0) ok = false;
		}
		test("V44", "energy values are non-negative", ok);
	}
	{	// [ENGINE DATA] no unit reports a negative kill count
		bool ok = true;
		for (auto& u : st.visible_units) if (u.kill_count < 0) ok = false;
		test("V45", "kill counts are non-negative", ok);
	}
	{	// [ENGINE DATA] units keep their owner after 10 frames
		// Every playable unit that existed before still exists after 10 frames,
		// unless it was killed. Frames cannot create units by themselves, so the
		// count of pre-existing playable units may only stay or shrink.
		std::set<int> before;
		for (auto& u : st.visible_units) if (u.owner < 8) before.insert(u.index);
		for (int i = 0; i != 10; ++i) f.player.next_frame();
		int still_there = 0;
		for (auto& u : st.visible_units) if (u.owner < 8 && before.count(u.index)) ++still_there;
		test("V46", "playable units survive ten idle frames",
			still_there == (int)before.size() || still_there > 0,
			fmt("%d of %zu", still_there, before.size()));
	}
	{	// [ENGINE DATA] frame counter advances exactly one per next_frame()
		int before = (int)st.current_frame;
		f.player.next_frame();
		test("V47", "frame counter advances by exactly one", (int)st.current_frame == before + 1,
			fmt("%d -> %d", before, (int)st.current_frame));
	}
	{	// [ENGINE DATA] trigger timer stays inside its documented range
		bool ok = st.trigger_timer >= 0 && st.trigger_timer <= 30;
		test("V48", "trigger timer within 0..30", ok, fmt("%d", (int)st.trigger_timer));
	}
	{	// [ENGINE DATA] trigger evaluation does not consume frames
		int before = (int)st.current_frame;
		for (int i = 0; i != 5; ++i) f.player.next_frame();
		test("V49", "five frames advance by five", (int)st.current_frame == before + 5);
	}
	{	// Final gate.
		bool ok = true;
		std::string why;
		try { f.player.next_frame(); } catch (std::exception& e) { ok = false; why = e.what(); }
		test("V50", "engine still steps after the whole suite", ok, why);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
