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
//
// `focus` is the target the group has already decided to concentrate on (B1).
// When it is given and still present among the candidates, every ranged unit in
// the group picks it, which is what turns "each unit picks its own wounded
// enemy" into actual focus fire.
inline const unit_t* select_ranged_target(const std::vector<candidate>& in_range,
                                          const std::vector<candidate>& out_of_range,
                                          const unit_t* focus = nullptr) {
	if (focus) {
		for (const candidate& c : in_range) if (c.unit == focus) return focus;
		// A focused target out of range is still the right thing to walk to;
		// the fallback below would pick a nearer healthy unit instead.
		for (const candidate& c : out_of_range) if (c.unit == focus) return focus;
	}
	if (const unit_t* wounded = select_best(in_range, mode::ranged)) return wounded;
	return select_best(out_of_range, mode::melee);
}

inline const unit_t* select_melee_target(const std::vector<candidate>& all) {
	return select_best(all, mode::melee);
}

// The group's shared ranged target (B1): the most wounded enemy IN RANGE of the
// group as a whole, else the nearest one any unit can reach. Computed once per
// tick by the driver and passed to every ranged unit, so the group converges on
// one enemy instead of spreading damage.
inline const unit_t* select_group_ranged_target(const std::vector<candidate>& in_range,
                                                const std::vector<candidate>& out_of_range) {
	if (const unit_t* wounded = select_best(in_range, mode::ranged)) return wounded;
	return select_best(out_of_range, mode::melee);
}

// ------------------------------------------------------------ kiting (B2) ---

// A unit counts as wounded when its remaining hp+shields is below this fraction
// of the type's maximum (percent, integer). 50% is a deliberate, documented
// choice: it is the point where a ranged unit will trade range for survival.
constexpr int wounded_fraction_percent = 50;

// Kiting is allowed only until the weapon has loaded this far. The engine
// counts `ground_weapon_cooldown` DOWN every frame (bwgame.h:1267), so a larger
// value means "further from being able to shoot"; 9 is the requested bound.
constexpr int kite_cooldown_threshold = 9;

inline int unit_max_hp_total(const unit_t* u) {
	return (int)u->unit_type->hitpoints.integer_part() + (int)u->unit_type->shield_points;
}

inline bool is_wounded(const unit_t* u) {
	const int max_hp = unit_max_hp_total(u);
	if (max_hp <= 0) return false;
	return unit_hp_total(u) * 100 < max_hp * wounded_fraction_percent;
}

// B2: a wounded ranged unit may back off while its weapon is still reloading,
// and must stop kiting once the cooldown has counted down to <= 9 - from then
// on it stays and fires. Healthy units and melee units never kite.
inline bool should_kite(const unit_t* u) {
	if (is_melee(u)) return false;
	if (!is_wounded(u)) return false;
	return u->ground_weapon_cooldown > kite_cooldown_threshold;
}

// Where a kiting unit steps: directly away from `threat`, by `distance` pixels,
// clamped to the map by the caller. Pure, so it is unit-tested without a game.
inline bwgame::xy kite_position(bwgame::xy self, bwgame::xy threat, int distance) {
	const long long dx = (long long)self.x - threat.x;
	const long long dy = (long long)self.y - threat.y;
	const long long len_sq = dx * dx + dy * dy;
	if (len_sq == 0) return bwgame::xy{self.x + distance, self.y};   // no direction: step east
	// Integer sqrt without <cmath>: enough precision for a retreat step.
	long long len = 0;
	while ((len + 1) * (len + 1) <= len_sq) ++len;
	if (len == 0) len = 1;
	return bwgame::xy{
		self.x + (int)(dx * distance / len),
		self.y + (int)(dy * distance / len)};
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

// A unit that takes part in the fight: alive, not a building, armed, has a sprite.
inline bool is_combatant(const bwgame::state_functions& f, const bwgame::unit_t* u) {
	if (!u) return false;
	if (f.unit_dying(u)) return false;
	if (!u->sprite) return false;
	if (u->unit_type->flags & bwgame::unit_type_t::flag_building) return false;
	if (!u->unit_type->ground_weapon && !u->unit_type->air_weapon) return false;
	return true;
}

inline int weapon_range_sq(const bwgame::unit_t* u) {
	const bwgame::weapon_type_t* wp = u->unit_type->ground_weapon;
	if (!wp) wp = u->unit_type->air_weapon;
	const int r = wp ? (int)wp->max_range : 0;
	return r * r;
}

// Rally point (B3): the centre of mass of the owner's combatants. Units with no
// enemy to attack gather here instead of standing where the map dropped them,
// so the group forms up and moves as one body.
inline bool rally_point(const bwgame::state& st, const bwgame::state_functions& f,
	int owner, bwgame::xy* out) {
	long long sx = 0, sy = 0;
	int n = 0;
	for (const bwgame::unit_t& u : st.player_units[owner]) {
		if (!is_combatant(f, &u)) continue;
		sx += u.sprite->position.x;
		sy += u.sprite->position.y;
		++n;
	}
	if (n == 0) return false;
	*out = bwgame::xy{(int)(sx / n), (int)(sy / n)};
	return true;
}

// One thinking step for `owner`. Returns true when at least one order was issued,
// so callers/tests can tell "acted" from "nothing to do".
//
// Two phases, in this order, because focus fire needs the whole group's view
// before anyone is ordered:
//
//   1. COLLECT  - build the enemy list and, for ranged units, the group's shared
//                 target (most wounded in range of the group, else nearest).
//   2. ACT      - each unit: kite if wounded and still reloading (B2), else
//                 attack its target (melee: nearest; ranged: the group target),
//                 else move to the rally point (B3).
//
// Never retreats: a zombie always attacks or advances.
inline bool tick(bwgame::state_functions& f, bwgame::state& st, int owner) {
	bool issued = false;

	std::vector<const bwgame::unit_t*> enemies;
	for (bwgame::unit_t& e : st.visible_units) {
		if (!detail::is_hostile(st, owner, &e)) continue;
		if (f.unit_dying(&e)) continue;
		enemies.push_back(&e);
	}

	// ---- phase 1: group target for ranged units (B1) ----------------------
	// One shared target for the whole ranged group: the most wounded enemy in
	// range of ANY of our ranged units, else the nearest enemy overall. Computed
	// with the same pure policy the single units use, so behaviour stays
	// consistent between one unit and many.
	std::vector<candidate> group_in_range, group_out_of_range;
	for (const bwgame::unit_t* e : enemies) {
		int best_d = std::numeric_limits<int>::max();
		bool in_range = false;
		for (const bwgame::unit_t& u : st.player_units[owner]) {
			if (!is_combatant(f, &u)) continue;
			if (is_melee(&u)) continue;
			const int d = detail::unit_distance_sq(&u, e);
			if (d < best_d) best_d = d;
			if (d <= weapon_range_sq(&u)) in_range = true;
		}
		if (best_d == std::numeric_limits<int>::max()) continue;   // no ranged unit at all
		candidate c{e, best_d, unit_hp_total(e)};
		(in_range ? group_in_range : group_out_of_range).push_back(c);
	}
	const bwgame::unit_t* focus = select_group_ranged_target(group_in_range, group_out_of_range);

	// ---- phase 2: orders -------------------------------------------------
	bwgame::xy rally{};
	const bool have_rally = rally_point(st, f, owner, &rally);

	for (bwgame::unit_t& u : st.player_units[owner]) {
		bwgame::unit_t* self = &u;
		if (!is_combatant(f, self)) continue;
		if (self->owner != owner) continue;

		const mode m = is_melee(self) ? mode::melee : mode::ranged;

		if (enemies.empty()) {
			// Nothing to kill: form up on the rally point (B3).
			if (have_rally) { issue_move(f, self, rally); issued = true; }
			continue;
		}

		// B2: a wounded ranged unit steps away while reloading, and only until
		// the cooldown reaches the threshold.
		if (should_kite(self)) {
			const bwgame::unit_t* threat = select_melee_target([&] {
				std::vector<candidate> all;
				for (const bwgame::unit_t* e : enemies)
					all.push_back({e, detail::unit_distance_sq(self, e), unit_hp_total(e)});
				return all;
			}());
			if (threat) {
				const bwgame::xy pos = kite_position(self->sprite->position, threat->sprite->position, 64);
				issue_move(f, self, pos);
				issued = true;
				continue;
			}
		}

		const int range_sq = weapon_range_sq(self);
		std::vector<candidate> in_range, out_of_range;
		for (const bwgame::unit_t* e : enemies) {
			const int d = detail::unit_distance_sq(self, e);
			candidate c{e, d, unit_hp_total(e)};
			(d <= range_sq ? in_range : out_of_range).push_back(c);
		}

		const bwgame::unit_t* target = nullptr;
		if (m == mode::melee) {
			std::vector<candidate> all = in_range;
			all.insert(all.end(), out_of_range.begin(), out_of_range.end());
			target = select_melee_target(all);
		} else {
			target = select_ranged_target(in_range, out_of_range, focus);
		}

		if (target) {
			issue_attack(f, self, target);
			issued = true;
		} else if (have_rally) {
			issue_move(f, self, rally);
			issued = true;
		}
	}
	return issued;
}

} // namespace zombie_bot
