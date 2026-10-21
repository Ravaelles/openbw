// Zombie bot - a minimal, in-process opponent for unit-only test maps.
//
// ARCHITECTURE (see _AI/BOT-DESIGN.md):
//
//   tick driver  ->  policy (pure)  ->  actuator (only place that issues orders)
//
// The policy is a set of free functions over plain data: given a unit and a list
// of candidate targets they return a pointer, with no engine mutation. That is
// what makes the behaviour testable without loading a map, and it is where all
// the "zombie" logic lives.
//
// RULES
//   melee  (weapon range < one tile): target = NEAREST enemy.
//   ranged (range >= one tile):      target = MOST WOUNDED enemy already in
//                                    range; if none in range, NEAREST enemy.
//   ties: nearest first, then lowest unit index - never container order, so the
//   bot is deterministic and a replay of the same state gives the same orders.
//
// NO ECONOMY, NO SCOUTING. It reacts to what it can see and nothing else.
#pragma once
#include <algorithm>
#include <limits>
#include <vector>

#include "bwgame.h"

namespace zombie_bot {

using bwgame::fp8;
using bwgame::unit_t;

// ---------------------------------------------------------------- policy ----

enum class mode { melee, ranged };

// A weapon range below one tile means "has to walk into contact".
constexpr int melee_range_threshold = 32;   // pixels; 1 tile

struct candidate {
	const unit_t* unit = nullptr;
	int distance_sq = 0;   // squared pixel distance; squared avoids sqrt
	int hp_total = 0;      // hp + shields, in fp8 raw units (same scale)
};

// What counts as "wounded" for target choice: remaining hp plus shields.
//
// Both are fp8, but they are summed in WHOLE POINTS (integer_part), not in raw
// units: fp8::integer(12).raw_value is 12<<8 == 3072, so comparing raw values
// would still order correctly but would make the numbers meaningless in a log.
// Measured while writing Z14.
inline int unit_hp_total(const unit_t* u) {
	return (int)u->hp.integer_part() + (int)u->shield_points.integer_part();
}

inline int xy_distance_sq(bwgame::xy a, bwgame::xy b) {
	long long dx = (long long)a.x - b.x;
	long long dy = (long long)a.y - b.y;
	long long d = dx * dx + dy * dy;
	if (d > (long long)std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
	return (int)d;
}

// True when the unit is expected to fight at contact range.
inline bool is_melee(const unit_t* u) {
	const bwgame::weapon_type_t* w = u->unit_type->ground_weapon;
	if (!w && u->unit_type->air_weapon) w = u->unit_type->air_weapon;
	if (!w) return true;                       // no weapon: treat as melee
	return w->max_range < melee_range_threshold;
}

using score_fn = bool (*)(const candidate&, const candidate&);

// Ordering used by both modes: "is a better than b?".
//   melee :          nearer,       then lower index
//   ranged:          weaker first, then nearer, then lower index
inline bool better_melee(const candidate& a, const candidate& b) {
	if (a.distance_sq != b.distance_sq) return a.distance_sq < b.distance_sq;
	return a.unit->index < b.unit->index;
}

inline bool better_ranged(const candidate& a, const candidate& b) {
	if (a.hp_total != b.hp_total) return a.hp_total < b.hp_total;
	if (a.distance_sq != b.distance_sq) return a.distance_sq < b.distance_sq;
	return a.unit->index < b.unit->index;
}

// Pure selection over already-filtered candidates.
//
// `in_range_only` restricts the choice to units the caller considers reachable
// right now (used by the ranged "wounded in range" pass). When the caller knows
// nothing about range it passes false and gets the plain rule.
inline const unit_t* select_best(const std::vector<candidate>& candidates, mode m) {
	const unit_t* best = nullptr;
	candidate best_c{};
	for (const candidate& c : candidates) {
		if (!c.unit) continue;
		if (!best) {
			best = c.unit;
			best_c = c;
			continue;
		}
		bool better = (m == mode::melee) ? better_melee(c, best_c) : better_ranged(c, best_c);
		if (better) {
			best = c.unit;
			best_c = c;
		}
	}
	return best;
}

// The two-pass ranged rule, expressed as data in and data out so it can be
// unit-tested without a game: `in_range` and `out_of_range` are disjoint sets.
inline const unit_t* select_ranged_target(const std::vector<candidate>& in_range,
                                          const std::vector<candidate>& out_of_range) {
	if (const unit_t* wounded = select_best(in_range, mode::ranged)) return wounded;
	return select_best(out_of_range, mode::melee);
}

inline const unit_t* select_melee_target(const std::vector<candidate>& all) {
	return select_best(all, mode::melee);
}

// --------------------------------------------------------------- actuator ---

// The only place that issues orders. Kept separate so policy tests never touch
// the engine and so a future engine API change is a one-place edit.
inline void issue_attack(bwgame::state_functions& f, unit_t* u, const unit_t* target) {
	f.set_unit_order(u, f.get_order_type(bwgame::Orders::AttackUnit),
		bwgame::order_target_t(const_cast<unit_t*>(target)));
}

inline void issue_move(bwgame::state_functions& f, unit_t* u, bwgame::xy pos) {
	f.set_unit_order(u, f.get_order_type(bwgame::Orders::Move), bwgame::order_target_t(pos));
}

// ------------------------------------------------------------ tick driver ---
//
// Decides WHEN to think and WHICH units to think about. Deliberately dumb: it
// re-evaluates every `think_interval` frames so a replay of the same state
// produces the same orders, and it never caches a target across ticks (a cached
// pointer would go stale the moment that unit dies).
namespace detail {

// True when `u` belongs to an enemy of `owner` and can be attacked at all.
inline bool is_hostile(const bwgame::state& st, int owner, const bwgame::unit_t* u) {
	if (!u || u->owner == owner) return false;
	if (u->owner < 0 || u->owner >= 12) return false;
	return true;
}

// Squared distance between two units, using sprite positions (the engine keeps
// the authoritative position there).
inline int unit_distance_sq(const bwgame::unit_t* a, const bwgame::unit_t* b) {
	if (!a->sprite || !b->sprite) return std::numeric_limits<int>::max();
	return xy_distance_sq(a->sprite->position, b->sprite->position);
}

} // namespace detail

// One thinking step for `owner`. Returns true when at least one order was issued,
// so callers/tests can tell "acted" from "nothing to do".
//
// Rebuilds the candidate list every call and never caches a target pointer: a
// cached pointer would dangle the moment that unit dies.
inline bool tick(bwgame::state_functions& f, bwgame::state& st, int owner) {
	bool issued = false;

	std::vector<const bwgame::unit_t*> enemies;
	for (bwgame::unit_t& e : st.visible_units) {
		if (!detail::is_hostile(st, owner, &e)) continue;
		if (f.unit_dying(&e)) continue;
		enemies.push_back(&e);
	}
	if (enemies.empty()) return false;   // no opponent on this map: never crash

	for (bwgame::unit_t& u : st.player_units[owner]) {
		bwgame::unit_t* self = &u;
		if (f.unit_dying(self)) continue;
		if (self->owner != owner) continue;
		if (!self->sprite) continue;
		if (self->unit_type->flags & bwgame::unit_type_t::flag_building) continue;
		// Only armed units take part; an unarmed worker is ignored.
		if (!self->unit_type->ground_weapon && !self->unit_type->air_weapon) continue;

		const mode m = is_melee(self) ? mode::melee : mode::ranged;

		const bwgame::weapon_type_t* wp = self->unit_type->ground_weapon;
		if (!wp) wp = self->unit_type->air_weapon;
		const int range_px = wp ? (int)wp->max_range : 0;
		const int range_sq = range_px * range_px;

		std::vector<candidate> in_range, out_of_range;
		for (const bwgame::unit_t* e : enemies) {
			const int d = detail::unit_distance_sq(self, e);
			candidate c{e, d, unit_hp_total(e)};
			if (d <= range_sq) in_range.push_back(c);
			else out_of_range.push_back(c);
		}

		const bwgame::unit_t* target = nullptr;
		if (m == mode::melee) {
			std::vector<candidate> all = in_range;
			all.insert(all.end(), out_of_range.begin(), out_of_range.end());
			target = select_melee_target(all);
		} else {
			target = select_ranged_target(in_range, out_of_range);
		}

		if (target) {
			issue_attack(f, self, target);
			issued = true;
		}
	}
	return issued;
}

} // namespace zombie_bot
