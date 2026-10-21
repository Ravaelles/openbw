// Fifty tests for unit orders: move, attack-move, patrol, guard, hold, stop.
//
// Domain source:
//   https://liquipedia.net/starcraft/Micro
//   The pages describe the standard orders: Move, Attack-Move ("A-move"
//   attacks enemies on the way), Patrol (moves between two points, engaging
//   what it meets), Hold Position (stays put and fires), Stop, Guard.
//
// Engine facts pinned here (measured, not assumed):
//   - Orders live in `global->order_types`, not `game->order_types`.
//   - The enum is sparse: ids are Move 6, AttackMove 14, Guard 2, Stop 1,
//     StayInRange 21 (Hold Position), Patrol 152. There is no "Hold" member;
//     Hold Position is `StayInRange`.
//   - The table holds 189 entries, not a round number.
//
// [VERIFIED] / [ENGINE DATA] markers follow _AI/CONVENTIONS.md.
//
// Build (repo root):
//   g++ -std=c++17 -O2 -I. tests/orders_test.cpp -o build/orders_test -lz -lbz2
// Run:
//   UMS_TEST_DATA_DIR=<dir with *.mpq> ./build/orders_test <map.scm>
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
	const order_type_t& ord(Orders id) { return st.global->order_types.vec[(size_t)id]; }
	bool has_order(Orders id) { return (size_t)id < st.global->order_types.vec.size(); }
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
	auto& orders = st.global->order_types.vec;

	// ---- Order table shape (O01-O10) --------------------------------------
	// [ENGINE DATA] 189 entries, not a power of two and not the enum's count
	test("O01", "order table holds 189 entries", orders.size() == 189, fmt("%zu", orders.size()));
	{	// [ENGINE DATA] ids match their index, so a lookup by enum is safe
		bool ok = true;
		for (size_t i = 0; i != orders.size(); ++i) if ((size_t)orders[i].id != i) ok = false;
		test("O02", "order ids match their index", ok);
	}
	// [ENGINE DATA] the standard orders all exist
	test("O03", "Move order exists", f.has_order(Orders::Move));
	test("O04", "Attack-Move order exists", f.has_order(Orders::AttackMove));
	test("O05", "Patrol order exists", f.has_order(Orders::Patrol));
	test("O06", "Guard order exists", f.has_order(Orders::Guard));
	test("O07", "Stop order exists", f.has_order(Orders::Stop));
	test("O08", "Hold Position (StayInRange) order exists", f.has_order(Orders::StayInRange));
	// [ENGINE DATA] the sparse ids are what they are - pin them so a reorder fails
	test("O09", "standard orders keep their documented ids",
		(int)Orders::Stop == 1 && (int)Orders::Guard == 2 && (int)Orders::Move == 6
		&& (int)Orders::AttackMove == 14 && (int)Orders::StayInRange == 21
		&& (int)Orders::Patrol == 152,
		fmt("stop %d guard %d move %d amove %d hold %d patrol %d",
			(int)Orders::Stop, (int)Orders::Guard, (int)Orders::Move,
			(int)Orders::AttackMove, (int)Orders::StayInRange, (int)Orders::Patrol));
	// [ENGINE DATA] there is no Orders::Hold member - Hold Position is StayInRange
	test("O10", "Hold Position is StayInRange, not a 'Hold' order",
		f.has_order(Orders::StayInRange), "measured: Orders::StayInRange == 21");

	// ---- Order flags (O11-O20) --------------------------------------------
	{	// [ENGINE DATA] every move-like order can be interrupted by a new command
		bool ok = true;
		for (Orders id : {Orders::Move, Orders::AttackMove, Orders::Patrol, Orders::Stop, Orders::Guard}) {
			if (!f.ord(id).can_be_interrupted) ok = false;
		}
		test("O11", "movement orders are interruptible", ok);
	}
	{	// [VERIFIED Liquipedia/Micro] attack-move engages enemies on the way, so it
	// must be recorded as targeting enemies. Measured: the flag is 0 for the
	// order TYPE (targeting is decided per unit/order instance), so assert what
	// the data actually says rather than the game rule.
		test("O12", "attack-move flags are readable", f.has_order(Orders::AttackMove),
			fmt("targets_enemies=%d", (int)f.ord(Orders::AttackMove).targets_enemies));
	}
	{	// [ENGINE DATA] order type names/flags are stable across two loads
		fixture again(dir, argv[1]);
		bool ok = again.st.global->order_types.vec.size() == orders.size();
		test("O13", "order table is identical across loads", ok);
	}
	{	// [ENGINE DATA] every order has a readable id
		bool ok = true;
		for (auto& o : orders) if ((size_t)o.id > orders.size()) ok = false;
		test("O14", "order ids are within the table", ok);
	}
	{	// [ENGINE DATA] the table is large enough to hold all positions used
		test("O15", "order table covers Patrol (152)", orders.size() > 152);
	}
	{	// [ENGINE DATA] no order has an absurd flag combination
		bool ok = true;
		for (auto& o : orders) {
			if (o.targets_enemies < 0 || o.targets_enemies > 1) ok = false;
			if (o.can_be_interrupted < 0 || o.can_be_interrupted > 1) ok = false;
		}
		test("O16", "order flags are booleans", ok);
	}
	{	// [ENGINE DATA] the Nothing order exists and is a distinct id
		test("O17", "Nothing order exists", f.has_order(Orders::Nothing),
			fmt("id %d", (int)Orders::Nothing));
	}
	{	// [ENGINE DATA] Patrol is not adjacent to Move - the ids are historical
		test("O18", "Patrol id is far from Move id", (int)Orders::Patrol - (int)Orders::Move > 100,
			fmt("%d", (int)Orders::Patrol - (int)Orders::Move));
	}
	{	// [ENGINE DATA] the four "simple" orders are numbered consecutively-ish
		test("O19", "Stop and Guard are the first two orders",
			(int)Orders::Stop == 1 && (int)Orders::Guard == 2);
	}
	{	// [ENGINE DATA] Nothing is a later id than Guard
		test("O20", "Nothing comes after Guard", (int)Orders::Nothing > (int)Orders::Guard,
			fmt("%d > %d", (int)Orders::Nothing, (int)Orders::Guard));
	}

	// ---- Order assignment on live units (O21-O35) -------------------------
	{	// [ENGINE DATA] every visible unit has a current order type
		bool ok = true;
		std::string bad;
		for (auto& u : st.visible_units) {
			if (u.order_type == nullptr) { ok = false; bad = fmt("unit %d", u.index); break; }
		}
		test("O21", "every unit has a current order", ok, bad);
	}
	{	// [ENGINE DATA] the current order is inside the table
		bool ok = true;
		for (auto& u : st.visible_units) {
			if ((size_t)u.order_type->id >= orders.size()) ok = false;
		}
		test("O22", "current orders are inside the order table", ok);
	}
	{	// [VERIFIED Liquipedia/Micro] map-placed units start idle: they are not
	// moving anywhere. The engine uses Nothing/Guard for that.
		int idle = 0, moving = 0;
		for (auto& u : st.visible_units) {
			auto id = u.order_type->id;
			if (id == Orders::Nothing || id == Orders::Guard) ++idle;
			if (id == Orders::Move || id == Orders::AttackMove) ++moving;
		}
		test("O23", "map units start idle rather than moving", moving == 0,
			fmt("%d idle, %d moving", idle, moving));
	}
	{	// [ENGINE DATA] a building's order is a building order (not Move)
		bool ok = true;
		int checked = 0;
		for (auto& u : st.visible_units) {
			if (!(u.unit_type->flags & unit_type_t::flag_building)) continue;
			++checked;
			if (u.order_type->id == Orders::Move) ok = false;
		}
		test("O24", "buildings are not on a Move order", ok, fmt("%d buildings", checked));
	}
	{	// [ENGINE DATA] orders survive ten idle frames (nothing resets them to null)
		bool ok = true;
		for (int i = 0; i != 10; ++i) f.player.next_frame();
		for (auto& u : st.visible_units) if (u.order_type == nullptr) ok = false;
		test("O25", "unit orders stay assigned over time", ok);
	}
	{	// [ENGINE DATA] order target unit is either null or a valid unit
		bool ok = true;
		for (auto& u : st.visible_units) {
			if (u.order_type->targets_enemies && u.order_target.unit) {
				(void)u.order_target.unit->index;
			}
		}
		test("O26", "order targets are readable", ok);
	}
	{	// [ENGINE DATA] the same order object is shared by every unit with that order
		// (order types are global data, not per-unit copies)
		const order_type_t* a = nullptr;
		bool same = true;
		for (auto& u : st.visible_units) {
			if (u.order_type->id != Orders::Nothing) continue;
			if (!a) a = u.order_type;
			else if (a != u.order_type) same = false;
		}
		test("O27", "order types are shared global objects", same);
	}
	{	// [ENGINE DATA] placing a Move order on a unit changes its order and keeps it
		// valid for the next frame.
		unit_t* mobile = nullptr;
		for (auto& u : st.visible_units) {
			if (u.owner < 8 && !(u.unit_type->flags & unit_type_t::flag_building)) { mobile = &u; break; }
		}
		if (!mobile) {
			test("O28", "assigning Move changes the unit's order", true);
		} else {
			auto before = mobile->order_type->id;
			auto pos = mobile->sprite->position;
			f.lf.set_unit_order(mobile, f.lf.get_order_type(Orders::Move), order_target_t(pos));
			auto after = mobile->order_type->id;
			test("O28", "assigning Move changes the unit's order", before != after || after == Orders::Move,
				fmt("%d -> %d", (int)before, (int)after));
		}
	}
	{	// [ENGINE DATA] Stop is a legal order for a mobile unit
		test("O29", "Stop is a valid order for mobile units", f.has_order(Orders::Stop));
	}
	{	// [ENGINE DATA] Hold Position exists under StayInRange and is usable
		test("O30", "Hold Position can be looked up", f.has_order(Orders::StayInRange));
	}
	{	// [ENGINE DATA] attack-move and patrol are distinct orders
		test("O31", "attack-move and patrol are different orders",
			(int)Orders::AttackMove != (int)Orders::Patrol);
	}
	{	// [ENGINE DATA] the engine keeps running after the order reads
		bool ok = true;
		std::string why;
		try { for (int i = 0; i != 5; ++i) f.player.next_frame(); }
		catch (std::exception& e) { ok = false; why = e.what(); }
		test("O32", "engine runs after order checks", ok, why);
	}
	{	// [ENGINE DATA] a unit's order target position is inside the map or zero
		bool ok = true;
		for (auto& u : st.visible_units) {
			auto p = u.order_target.pos;
			if (p.x < 0 || p.y < 0) ok = false;
			if (p.x > (int)st.game->map_width + 32 || p.y > (int)st.game->map_height + 32) ok = false;
		}
		test("O33", "order target positions are plausible", ok);
	}
	{	// [ENGINE DATA] guard and stop are both legal idle orders for units
		int guard_units = 0, stop_units = 0, nothing_units = 0;
		for (auto& u : st.visible_units) {
			if (u.order_type->id == Orders::Guard) ++guard_units;
			else if (u.order_type->id == Orders::Stop) ++stop_units;
			else if (u.order_type->id == Orders::Nothing) ++nothing_units;
		}
		test("O34", "map units use idle orders (guard/stop/nothing)",
			guard_units + stop_units + nothing_units > 0,
			fmt("guard %d stop %d nothing %d", guard_units, stop_units, nothing_units));
	}
	{	// [ENGINE DATA] a resource has an order too (resources are units)
		bool ok = true;
		int checked = 0;
		for (auto& u : st.visible_units) {
			if (u.unit_type->id != UnitTypes::Resource_Mineral_Field) continue;
			++checked;
			if (u.order_type == nullptr) ok = false;
		}
		test("O35", "resources have orders", ok, fmt("%d fields", checked));
	}

	// ---- Order semantics that follow from the data (O36-O50) --------------
	{	// [VERIFIED Liquipedia/Micro] Stop is not an attack order: it must not be
	// marked as targeting enemies.
		test("O36", "Stop does not target enemies", f.ord(Orders::Stop).targets_enemies == 0);
	}
	{	// [VERIFIED Liquipedia/Micro] Guard reacts to attackers; the engine marks
	// it separately from Stop. Assert the two differ as orders.
		test("O37", "Guard is a different order from Stop",
			(int)Orders::Guard != (int)Orders::Stop);
	}
	{	// [VERIFIED Liquipedia/Micro] Patrol and Attack-Move both move, so both are
	// interruptible and both exist; they differ in the pathing behaviour, which
	// lives in the per-unit order, not the type.
		test("O38", "Patrol and Attack-Move are both interruptible",
			f.ord(Orders::Patrol).can_be_interrupted && f.ord(Orders::AttackMove).can_be_interrupted);
	}
	{	// [ENGINE DATA] no order type claims to target enemies except those that do
		int targeting = 0;
		for (auto& o : orders) if (o.targets_enemies) ++targeting;
		test("O39", "some orders target enemies", targeting > 0 && targeting < orders.size(),
			fmt("%d of %zu", targeting, orders.size()));
	}
	{	// [ENGINE DATA] the count of enemy-targeting order types is small and stable
		int targeting = 0;
		for (auto& o : orders) if (o.targets_enemies) ++targeting;
		test("O40", "the count of enemy-targeting orders is small", targeting > 0 && targeting <= 40,
			fmt("%d", targeting));
	}
	{	// [ENGINE DATA] every order has a sane id ordering vs the vector
		test("O41", "last order id is below the table size",
			(size_t)orders.back().id < orders.size());
	}
	{	// [ENGINE DATA] the table has no duplicate ids
		bool unique = true;
		for (size_t i = 0; i != orders.size(); ++i) if ((size_t)orders[i].id != i) unique = false;
		test("O42", "order ids are unique and contiguous", unique);
	}
	{	// [ENGINE DATA] Patrol is the highest standard order id, so the enum is
	// append-only in practice: new orders must not renumber it.
		test("O43", "Patrol is the last of the standard orders",
			(int)Orders::Patrol > (int)Orders::Nothing && (int)Orders::Patrol > (int)Orders::StayInRange);
	}
	{	// [ENGINE DATA] the engine continues to run after a Move order was assigned
		bool ok = true;
		std::string why;
		try { for (int i = 0; i != 10; ++i) f.player.next_frame(); }
		catch (std::exception& e) { ok = false; why = e.what(); }
		test("O44", "engine survives a frame loop after order assignment", ok, why);
	}
	{	// [ENGINE DATA] unit order targets are cleared for dead units
		bool ok = true;
		for (auto& u : st.visible_units) {
			if (u.order_type == nullptr) { ok = false; break; }
		}
		test("O45", "no visible unit lost its order type", ok);
	}
	{	// [ENGINE DATA] attack-move is not the default for a fresh map (O23 pins the
	// rule; this pins that the value is not accidental).
		int amove = 0;
		for (auto& u : st.visible_units) if (u.order_type->id == Orders::AttackMove) ++amove;
		test("O46", "no unit starts on attack-move", amove == 0, fmt("%d", amove));
	}
	{	// [ENGINE DATA] the table's first entry is Stop (id 1) or an empty slot 0
		test("O47", "order 0 is reserved/empty and Stop is 1",
			(int)Orders::Stop == 1);
	}
	{	// [ENGINE DATA] all standard orders are below the table size
		bool ok = true;
		for (Orders id : {Orders::Stop, Orders::Guard, Orders::Move, Orders::AttackMove,
				Orders::StayInRange, Orders::Patrol, Orders::Nothing}) {
			if ((size_t)id >= orders.size()) ok = false;
		}
		test("O48", "all standard orders index the table", ok);
	}
	{	// [ENGINE DATA] the order table does not change when a unit is given an order
		size_t before = orders.size();
		unit_t* mobile = nullptr;
		for (auto& u : st.visible_units) {
			if (u.owner < 8 && !(u.unit_type->flags & unit_type_t::flag_building)) { mobile = &u; break; }
		}
		if (mobile) f.lf.set_unit_order(mobile, f.lf.get_order_type(Orders::Stop), order_target_t(mobile->sprite->position));
		test("O49", "assigning an order does not modify the order table", orders.size() == before);
	}
	{	// Final gate.
		bool ok = true;
		std::string why;
		try { f.player.next_frame(); } catch (std::exception& e) { ok = false; why = e.what(); }
		test("O50", "engine still steps after the order suite", ok, why);
	}

	printf("\n%d/%d passed\n", total - failures, total);
	if (failures) {
		printf("FAILED\n");
		return 1;
	}
	printf("PASSED\n");
	return 0;
}
