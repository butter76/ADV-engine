// Using a move: BattleActions.runMove and useMove (battle-actions.ts), Gen
// 3's useMoveInner, tryMoveHit and modifyDamage (data/mods/gen3/scripts.ts),
// and the hit itself (moveHit, spreadMoveHit, getDamage).
//
// A move on one target or on its user can be used, with its own handlers,
// its secondary effect, and the stat changes, healing and status it gives.
// What the pipeline does not act on yet is listed in unsupported().

#pragma once

#include <cmath>

#include "sim.hpp"

namespace adv {

inline bool resolve_faints(Ctx& c); // engine.hpp
inline bool check_win(Ctx& c);

namespace events {
inline constexpr EventNames OVERRIDE_ACTION = event("OverrideAction");
inline constexpr EventNames BEFORE_MOVE = event("BeforeMove");
inline constexpr EventNames MOVE_ABORTED = event("MoveAborted");
inline constexpr EventNames AFTER_MOVE = event("AfterMove");
inline constexpr EventNames MODIFY_MOVE = event("ModifyMove");
inline constexpr EventNames DEDUCT_PP = event("DeductPP");
inline constexpr EventNames TRY_MOVE = event("TryMove");
inline constexpr EventNames PREPARE_HIT = event("PrepareHit");
inline constexpr EventNames INVULNERABILITY = event("Invulnerability");
inline constexpr EventNames NEGATE_IMMUNITY = event("NegateImmunity");
inline constexpr EventNames MODIFY_ACCURACY = event("ModifyAccuracy");
inline constexpr EventNames ACCURACY = event("Accuracy");
inline constexpr EventNames TRY_HIT = event("TryHit");
inline constexpr EventNames TRY_PRIMARY_HIT = event("TryPrimaryHit");
inline constexpr EventNames MODIFY_CRIT_RATIO = event("ModifyCritRatio");
inline constexpr EventNames CRITICAL_HIT = event("CriticalHit");
inline constexpr EventNames BASE_POWER = event("BasePower");
inline constexpr EventNames MODIFY_ATK = event("ModifyAtk");
inline constexpr EventNames MODIFY_SPA = event("ModifySpA");
inline constexpr EventNames MODIFY_DEF = event("ModifyDef");
inline constexpr EventNames MODIFY_SPD = event("ModifySpD");
inline constexpr EventNames MODIFY_DAMAGE_PHASE1 = event("ModifyDamagePhase1");
inline constexpr EventNames WEATHER_MODIFY_DAMAGE = event("WeatherModifyDamage");
inline constexpr EventNames MODIFY_DAMAGE = event("ModifyDamage");
inline constexpr EventNames HIT = event("Hit");
inline constexpr EventNames DAMAGING_HIT = event("DamagingHit");
inline constexpr EventNames AFTER_MOVE_SECONDARY = event("AfterMoveSecondary");
inline constexpr EventNames AFTER_MOVE_SECONDARY_SELF = event("AfterMoveSecondarySelf");
inline constexpr EventNames FAINT_BY_MOVE = event("Faint");
inline constexpr EventNames UPDATE_AFTER_HIT = event("Update");
inline constexpr EventNames MODIFY_BOOST_OF = event("ModifyBoost");
} // namespace events

// Whether the pipeline below can run a move yet. A move's own handlers are
// not part of the test: reaching one that is not ported stops the replay by
// itself. What is tested is the data the pipeline does not act on yet.
inline const char* unsupported(Move move) {
  const MoveData& m = data(move);
  if (m.type == Type::Unknown && m.category != Category::Status) return "a typeless move that does damage";
  if (m.target != Target::Normal && m.target != Target::Any && m.target != Target::AllAdjacent &&
      m.target != Target::AllAdjacentFoes && m.target != Target::Self && m.target != Target::All &&
      m.target != Target::FoeSide && m.target != Target::AllySide && m.target != Target::AllyTeam) {
    return "a move on a team or a random target";
  }
  if (m.multihit[1]) return "a move that hits more than once";
  if (m.ohko) return "a one-hit KO move";
  if (m.selfdestruct == SelfDestruct::IfHit) return "a move that faints its user if it hits";
  if (m.self_switch != SelfSwitch::None) return "a move that switches its user out";
  if (m.sleep_usable || m.calls_move || m.has_crash_damage || m.multiaccuracy) return "a move of a special kind";
  if (m.self_volatile_status != Condition::None) return "a move that gives its user a volatile";
  if (move == Move::BatonPass || move == Move::TriAttack) return "a move with a handler in its secondary";
  return nullptr;
}

// Battle.trunc of a number that is not negative: JavaScript's `>>> 0`.
inline double trunc(double v) { return double(uint32_t(uint64_t(v))); }

// Battle.setActiveMove (battle.ts).
inline void set_active_move(Battle& b, Ref pokemon, Ref target) {
  b.has_active_move = true;
  b.active_pokemon = pokemon;
  b.active_target = target ? target : pokemon;
}
// Battle.attrLastMove (battle.ts): an attribute on the last `move` line.
// `[still]` also blanks that line's target.
inline void attr_last_move(Ctx& c, std::string_view attribute) {
  if (c.out.last_move_line < 0) return;
  Line& line = c.out.log[size_t(c.out.last_move_line)];
  if (attribute == "[still]") line.parts[3] = part::text("");
  line.parts[line.n++] = part::text(attribute);
}

// Pokemon.deductPP (pokemon.ts): how much PP the move lost.
inline int deduct_pp(Pokemon& p, Move move, int amount = 1) {
  for (int i = 0; i < p.moves; i++) {
    MoveSlot& slot = p.move_slots[i];
    if (slot.id != move) continue;
    slot.used = true;
    if (!slot.pp) return 0;
    amount = std::min<int>(amount, slot.pp);
    slot.pp = uint8_t(slot.pp - amount);
    return amount;
  }
  return 0;
}

// Pokemon.runImmunity(move, message) (pokemon.ts): whether the move's type
// can hit the Pokémon. With a message, an immunity is written.
inline bool run_immunity(Ctx& c, Ref target, bool message) {
  const Type type = c.b.active_move.type;
  const Pokemon& p = c.b.at(target);
  // A move that ignores immunity, as every status move does by default.
  if (data(c.b.active_move.id).ignore_immunity) return true;
  if (type == Type::None || type == Type::Unknown) return true;
  const bool negate = !run_event(c, events::NEGATE_IMMUNITY, on(target)).truthy();
  bool levitating = false, not_immune;
  if (type == Type::Ground) {
    // isGrounded: false for a Flying type, null under Levitate.
    not_immune = negate || !has_type(p, Type::Flying);
    if (not_immune && p.ability == Ability::Levitate) not_immune = false, levitating = true;
  } else {
    not_immune = negate || (!(p.types[0] != Type::None && damage_taken(p.types[0], type) == 3) &&
                            !(p.types[1] != Type::None && damage_taken(p.types[1], type) == 3));
  }
  if (not_immune) return true;
  if (!message) return false;
  if (levitating) add(c, {part::text("-immune"), part::pokemon(c.b, target), part::from(effect(Ability::Levitate))});
  else add(c, {part::text("-immune"), part::pokemon(c.b, target)});
  return false;
}

// Pokemon.calculateStat (pokemon.ts) for an attacking or defending stat. An
// event that changes the boosts has no listener yet.
inline double calculate_stat(Ctx& c, Ref r, uint16_t stored, int boost) {
  run_event_without_handlers(c, events::MODIFY_BOOST_OF, on(r));
  static constexpr double BOOST_TABLE[] = {1, 1.5, 2, 2.5, 3, 3.5, 4};
  boost = std::clamp(boost, -6, 6);
  return boost >= 0 ? std::floor(stored * BOOST_TABLE[boost]) : std::floor(stored / BOOST_TABLE[-boost]);
}

// Pokemon.runEffectiveness (pokemon.ts): the sum over the target's types of
// 1 for super effective and -1 for resisted. Nothing in Gen 3 listens for
// Effectiveness, and no plain move has the handler.
inline int run_effectiveness(const Battle& b, Ref target) {
  const Pokemon& p = b.at(target);
  int total = 0;
  for (const Type t : p.types) {
    if (t == Type::None) continue;
    const uint8_t taken = damage_taken(t, b.active_move.type);
    total += taken == 1 ? 1 : taken == 2 ? -1 : 0;
  }
  return total;
}

// Gen 3's BattleActions.modifyDamage (data/mods/gen3/scripts.ts:51).
inline double modify_damage(Ctx& c, double base_damage, Ref source, Ref target) {
  Battle& b = c.b;
  Battle::ActiveMove& move = b.active_move;
  const Effect me = effect(move.id);
  const Pokemon& user = b.at(source);
  if (user.status == Condition::Brn && base_damage && move.category == Category::Physical && user.ability != Ability::Guts) {
    base_damage = modify(base_damage, 0.5);
  }
  base_damage = run_event(c, events::MODIFY_DAMAGE_PHASE1, on(source), target, me, relay::number(base_damage)).number;
  // In singles no hit is a spread hit.
  base_damage = run_event(c, events::WEATHER_MODIFY_DAMAGE, on(source), target, me, relay::number(base_damage), false, true).number;
  if (move.category == Category::Physical && !std::floor(base_damage)) base_damage = 1;
  base_damage += 2;
  if (move.crit) base_damage = modify(base_damage, 2);
  base_damage = std::floor(base_damage); // nothing listens for ModifyDamagePhase2
  if (move.type != Type::Unknown) {
    base_damage = modify(base_damage, has_type(user, move.type) ? 1.5 : 1); // nor for ModifySTAB
  }
  const int type_mod = std::clamp(run_effectiveness(b, target), -6, 6);
  move.type_mod = int8_t(type_mod);
  if (type_mod > 0) {
    add(c, {part::text("-supereffective"), part::pokemon(b, target)});
    for (int i = 0; i < type_mod; i++) base_damage *= 2;
  }
  if (type_mod < 0) {
    add(c, {part::text("-resisted"), part::pokemon(b, target)});
    for (int i = 0; i > type_mod; i--) base_damage = std::floor(base_damage / 2);
  }
  if (move.crit) add(c, {part::text("-crit"), part::pokemon(b, target)});
  base_damage = run_event(c, events::MODIFY_DAMAGE, on(source), target, me, relay::number(base_damage)).number;
  // Battle.randomizer (battle.ts): 85 to 100 in a hundred.
  base_damage = trunc(trunc(base_damage * (100 - double(random(b, c.out, 16)))) / 100);
  return std::floor(base_damage) ? std::floor(base_damage) : 1;
}

// Showdown's result of a hit: a number of damage, undefined for a hit that
// did something else, false for one that failed, null for one that failed
// and has said so.
inline int result_rank(const Relay& r) {
  switch (r.kind) {
  case Relay::Kind::Undefined: return 0;
  case Relay::Kind::Null: return 2;
  case Relay::Kind::False:
  case Relay::Kind::True: return 3;
  default: return 4;
  }
}
// BattleActions.combineResults (battle-actions.ts).
inline Relay combine_results(const Relay& left, const Relay& right) {
  if (result_rank(left) > result_rank(right)) return left;
  if (left.truthy() && !right.truthy() && !(right.kind == Relay::Kind::Number)) return left;
  if (left.kind == Relay::Kind::Number && right.kind == Relay::Kind::Number) return relay::number(left.number + right.number);
  return right;
}
inline Relay as_result(bool b) { return b ? relay::yes : relay::no; }

// Which of a move's effects a hit applies (the `moveData` of moveHit): the
// move itself, its secondary effect, what it does to its user, or what its
// secondary effect does to its user.
enum class HitEffect : uint8_t { Move, Secondary, Self, SecondarySelf };

struct HitData {
  SparseBoosts boosts;
  Condition status = Condition::None, volatile_status = Condition::None;
  Fraction heal;
  bool has_self = false; // the effect has a `self` of its own
};
inline SparseBoosts sparse(const Boosts& b) {
  SparseBoosts out;
  const int8_t* v = &b.atk;
  for (int i = 0; i < BOOST_STATS; i++) {
    if (v[i]) out.set(i, v[i]);
  }
  return out;
}
// Battle.canSwitch (battle.ts): how many Pokémon behind the active one have
// not fainted.
inline int can_switch(const Battle& b, int side) {
  const Side& s = b.sides[side];
  if (!s.pokemon_left) return 0;
  int n = 0;
  for (int i = 0; i < s.size; i++) n += s.pokemon[i].position >= 1 && !s.pokemon[i].fainted;
  return n;
}

// A move on the field or on a side: it has no Pokémon for a target.
constexpr bool is_wide(Target t) {
  return t == Target::All || t == Target::FoeSide || t == Target::AllySide || t == Target::AllyTeam;
}

inline void rapidspin_self_onHit(Ctx& c, Ref pokemon); // callbacks.hpp

inline HitData hit_data(const Battle::ActiveMove& active, HitEffect effect_) {
  const MoveData& m = data(active.id);
  switch (effect_) {
  case HitEffect::Move:
    // Rapid Spin's `self` is a handler alone.
    return {sparse(m.boosts), m.status, active.volatile_status, m.heal,
            !(active.self_boosts == Boosts{}) || active.id == Move::RapidSpin};
  case HitEffect::Secondary:
    return {sparse(m.secondary.boosts), m.secondary.status, m.secondary.volatile_status, {},
            !(m.secondary.self_boosts == Boosts{})};
  case HitEffect::Self: {
    SparseBoosts self = sparse(active.self_boosts);
    self.first = active.self_boosts_first;
    return {self, Condition::None, Condition::None, {}, false};
  }
  case HitEffect::SecondarySelf: return {sparse(m.secondary.self_boosts), Condition::None, Condition::None, {}, false};
  }
  return {};
}

// A move's own handler, when it has one (singleEvent on the move).
inline Relay move_event(Ctx& c, Callback name, Ref holder, Ref source, Relay value = relay::undefined) {
  const Effect me = effect(c.b.active_move.id);
  return single_event(c, name, me, nullptr, on(holder), source, me, value);
}
namespace events {
inline constexpr Callback ON_MODIFY_MOVE = callback("", "ModifyMove");
inline constexpr Callback ON_TRY_MOVE = callback("", "TryMove");
inline constexpr Callback ON_USE_MOVE_MESSAGE = callback("", "UseMoveMessage");
inline constexpr Callback ON_PREPARE_HIT = callback("", "PrepareHit");
inline constexpr Callback ON_TRY = callback("", "Try");
inline constexpr Callback ON_TRY_IMMUNITY = callback("", "TryImmunity");
inline constexpr Callback ON_TRY_HIT = callback("", "TryHit");
inline constexpr Callback ON_HIT = callback("", "Hit");
inline constexpr Callback ON_AFTER_HIT = callback("", "AfterHit");
inline constexpr Callback ON_MOVE_FAIL = callback("", "MoveFail");
inline constexpr Callback ON_AFTER_MOVE = callback("", "AfterMove");
inline constexpr Callback ON_AFTER_MOVE_SECONDARY = callback("", "AfterMoveSecondary");
inline constexpr Callback ON_AFTER_MOVE_SECONDARY_SELF = callback("", "AfterMoveSecondarySelf");
inline constexpr Callback ON_EFFECTIVENESS = callback("", "Effectiveness");
inline constexpr EventNames MODIFY_SECONDARIES = event("ModifySecondaries");
} // namespace events

// BattleActions.getDamage (battle-actions.ts) for the move itself. Undefined
// for a move with no power, false when the target is immune.
inline Relay get_damage(Ctx& c, Ref source, Ref target) {
  Battle& b = c.b;
  Battle::ActiveMove& move = b.active_move;
  const Effect me = effect(move.id);
  if (!run_immunity(c, target, true)) return relay::no;
  if (has(me, Callback::damageCallback)) unported("a move with a damageCallback");
  if (data(move.id).damage_level) return relay::number(b.at(source).level);
  if (data(move.id).damage) return relay::number(data(move.id).damage);
  double base_power = move.base_power;
  if (has(me, Callback::basePowerCallback)) {
    const Relay called = call(c, {me, Callback::basePowerCallback}, relay::undefined, on(source), target, me);
    if (called.kind != Relay::Kind::Number) unported("a basePowerCallback that returns no number");
    base_power = called.number;
  }
  if (!base_power) return relay::undefined;
  base_power = std::max(std::floor(base_power), 1.0);
  int crit_ratio = int(run_event(c, events::MODIFY_CRIT_RATIO, on(source), target, me, relay::number(move.crit_ratio)).number);
  crit_ratio = std::clamp(crit_ratio, 0, 5);
  static constexpr uint32_t CRIT_MULT[] = {0, 16, 8, 4, 3, 2};
  move.crit = false;
  if (crit_ratio) move.crit = random(b, c.out, CRIT_MULT[crit_ratio]) < 1; // randomChance(1, n)
  if (move.crit) move.crit = run_event(c, events::CRITICAL_HIT, on(target), {}, me).truthy();
  const Relay power = run_event(c, events::BASE_POWER, on(source), target, me, relay::number(base_power), true);
  if (!power.truthy()) return relay::number(0);
  base_power = std::max(std::floor(power.number), 1.0);

  const Pokemon& attacker = b.at(source);
  const Pokemon& defender = b.at(target);
  const bool physical = move.category == Category::Physical;
  int atk_boosts = physical ? attacker.boosts.atk : attacker.boosts.spa;
  int def_boosts = physical ? defender.boosts.def : defender.boosts.spd;
  // A critical hit ignores the attacker's drops and the defender's raises.
  if (move.crit && atk_boosts < 0) atk_boosts = 0;
  if (move.crit && def_boosts > 0) def_boosts = 0;
  double attack = calculate_stat(c, source, physical ? attacker.stored_stats.atk : attacker.stored_stats.spa, atk_boosts);
  double defense = calculate_stat(c, target, physical ? defender.stored_stats.def : defender.stored_stats.spd, def_boosts);
  attack = run_event(c, physical ? events::MODIFY_ATK : events::MODIFY_SPA, on(source), target, me, relay::number(attack)).number;
  defense = run_event(c, physical ? events::MODIFY_DEF : events::MODIFY_SPD, on(target), source, me, relay::number(defense)).number;
  // Explosion and Self-Destruct halve the target's Defense.
  if ((move.id == Move::Explosion || move.id == Move::SelfDestruct) && physical) defense = std::max(std::floor(defense / 2), 1.0);
  const double level = attacker.level;
  const double base_damage = trunc(trunc(trunc(trunc(2 * level / 5 + 2) * base_power * attack) / defense) / 50);
  return relay::number(modify_damage(c, base_damage, source, target));
}

// Battle.spreadDamage (battle.ts) for a move's damage on its one target.
inline Relay move_damage(Ctx& c, double amount, Ref target, Ref source) {
  Battle& b = c.b;
  const Effect me = effect(b.active_move.id);
  Pokemon& p = b.at(target);
  if (!p.hp) return relay::number(0);
  if (!p.is_active) return relay::no;
  if (amount != 0) amount = std::max(std::floor(amount), 1.0);
  const Relay after = run_event(c, DAMAGE, on(target), source, me, relay::number(amount), true);
  if (after.kind != Relay::Kind::Number) return after;
  amount = after.number;
  if (amount != 0) amount = std::max(std::floor(amount), 1.0);
  // Pokemon.damage (pokemon.ts).
  const uint16_t dealt = uint16_t(std::min<double>(amount, p.hp));
  p.hp = uint16_t(p.hp - dealt);
  if (!p.hp) faint(c, target, source, me);
  b.at(source).last_damage = dealt;
  add(c, {part::text("-damage"), part::pokemon(b, target), part::health(b, target)});
  // A draining move heals its user by a part of what it dealt.
  if (const Fraction drain = data(b.active_move.id).drain; dealt && drain) {
    heal(c, std::max(std::floor(double(dealt) * drain.num / drain.den), 1.0), source, target, effect(Special::Drain));
  }
  return relay::number(dealt);
}

inline Relay move_hit(Ctx& c, Ref target, Ref source, HitEffect which = HitEffect::Move);

// BattleActions.runMoveEffects (battle-actions.ts) for one target: what the
// effect does besides damage. `hit` says whether the target is still a
// target; the result replaces `damage`.
inline Relay run_move_effects(Ctx& c, Relay damage, bool hit, Ref target, Ref source, HitEffect which) {
  // `target` is no Pokémon when a Substitute took the hit: the effect then
  // does nothing to it, and counts as having done something.
  Battle& b = c.b;
  const Move id = b.active_move.id;
  const Effect me = effect(id);
  const HitData d = hit_data(b.active_move, which);
  const bool is_secondary = which == HitEffect::Secondary || which == HitEffect::SecondarySelf;
  const bool is_self = which == HitEffect::Self || which == HitEffect::SecondarySelf;
  // A move on the user's team runs its onHit as a move on a Pokémon does.
  const bool on_field = is_wide(data(id).target) && data(id).target != Target::AllyTeam && !is_self;
  Relay did_anything = damage;
  if (hit && !target) {
    damage = combine_results(damage, relay::yes);
    did_anything = combine_results(did_anything, relay::yes);
  } else if (hit) {
    Relay did_something = relay::undefined;
    bool stop = false;
    if (d.boosts.present && !b.at(target).fainted) {
      did_something = combine_results(did_something, boost(c, d.boosts, target, source, me, is_secondary, is_self));
    }
    if (d.heal && !b.at(target).fainted) {
      Pokemon& p = b.at(target);
      if (p.hp >= p.maxhp) {
        add(c, {part::text("-fail"), part::pokemon(b, target), part::text("heal")});
        attr_last_move(c, "[still]");
        damage = combine_results(damage, relay::no);
        did_anything = combine_results(did_anything, relay::null);
        stop = true;
      } else {
        // Battle.heal(amount, target, source, move), amount floored.
        const double amount = std::floor(double(p.base_maxhp) * d.heal.num / d.heal.den);
        const Relay tried = run_event(c, TRY_HEAL, on(target), source, me, relay::number(std::max(amount, amount ? 1.0 : 0.0)));
        if (!tried.truthy()) unported("a healing move stopped by TryHeal");
        p.hp = uint16_t(std::min<double>(p.hp + tried.number, p.maxhp));
        add(c, {part::text("-heal"), part::pokemon(b, target), part::health(b, target)});
        did_something = relay::yes;
      }
    }
    if (!stop && d.status != Condition::None) {
      const bool set = try_set_status(c, target, d.status, source, me);
      if (!set && data(id).status != Condition::None) {
        damage = combine_results(damage, relay::no);
        did_anything = combine_results(did_anything, relay::null);
        stop = true;
      } else {
        did_something = combine_results(did_something, as_result(set));
      }
    }
    if (!stop && d.volatile_status != Condition::None) {
      const auto parent = c.effect;
      c.effect = me; // addVolatile(status, source, move)
      const bool added = add_volatile(c, target, d.volatile_status, source);
      c.effect = parent;
      did_something = combine_results(did_something, as_result(added));
    }
    if (!stop && which == HitEffect::Move && data(id).side_condition != Condition::None) {
      // The side of the target: the user's own for a move on its side.
      const int side = data(id).target == Target::AllySide ? source.side : target.side;
      did_something = combine_results(did_something, as_result(add_side_condition(c, side, data(id).side_condition, source, me)));
    }
    if (!stop && which == HitEffect::Move && data(id).force_switch) {
      did_something = combine_results(did_something, as_result(can_switch(b, target.side) > 0));
    }
    if (!stop && which == HitEffect::Move && data(id).slot_condition != Condition::None) {
      did_something = combine_results(did_something, as_result(add_slot_condition(c, target.side, data(id).slot_condition, source, me)));
    }
    if (!stop && which == HitEffect::Move && data(id).weather != Condition::None) {
      did_something = combine_results(did_something, as_result(set_weather(c, data(id).weather, source, me)));
    }
    if (!stop) {
      if (on_field) {
        // Its onHitField; no move of Gen 3 has an onHitSide.
        if (data(id).target == Target::All && has(me, callback("", "HitField"))) {
          did_something = combine_results(did_something, move_event(c, callback("", "HitField"), target, source));
        }
      } else {
        if (which == HitEffect::Move && b.active_move.on_hit && has(me, events::ON_HIT)) {
          const Relay result = move_event(c, events::ON_HIT, target, source);
          did_something = combine_results(did_something, result);
        }
        if (!is_self && !is_secondary) run_event(c, events::HIT, on(target), source, me);
      }
      if (did_something.undefined()) did_something = relay::yes;
      damage = combine_results(damage, did_something.kind == Relay::Kind::Null ? relay::no : did_something);
      did_anything = combine_results(did_anything, did_something);
    }
  }
  if (!did_anything.truthy() && did_anything.kind != Relay::Kind::Number && !d.has_self) {
    if (!is_self && !is_secondary && did_anything == relay::no) {
      add(c, {part::text("-fail"), part::pokemon(b, source)});
      attr_last_move(c, "[still]");
    }
  }
  return damage;
}

// BattleActions.moveHit (battle-actions.ts), which is spreadMoveHit for one
// target. Showdown's `true` comes back as undefined.
inline Relay move_hit(Ctx& c, Ref target, Ref source, HitEffect which) {
  Battle& b = c.b;
  const Move id = b.active_move.id;
  const Effect me = effect(id);
  const MoveData& m = data(id);
  const bool is_secondary = which == HitEffect::Secondary || which == HitEffect::SecondarySelf;
  const bool is_self = which == HitEffect::Self || which == HitEffect::SecondarySelf;
  const HitData d = hit_data(b.active_move, which);
  const bool on_field = is_wide(m.target) && !is_self;
  Relay damage = relay::yes;
  bool hit = true;
  if (on_field) {
    if (has(me, callback("", "TryHitField"))) unported("a move's onTryHitField");
  } else if (which == HitEffect::Move && has(me, events::ON_TRY_HIT)) {
    const Relay tried = move_event(c, events::ON_TRY_HIT, target, source);
    if (!tried.truthy()) {
      if (tried == relay::no) {
        add(c, {part::text("-fail"), part::pokemon(b, source)});
        attr_last_move(c, "[still]");
      }
      return relay::no;
    }
  }
  if (!is_secondary && !is_self && !on_field && target) {
    damage = run_event(c, events::TRY_PRIMARY_HIT, on(target), source, me);
    // A Substitute took the hit (HIT_SUBSTITUTE, which is 0): the move has
    // no target from here on, and its damage is 0.
    if (damage.kind == Relay::Kind::Number) {
      damage = relay::number(0);
      target = Ref{};
    }
  }
  if (target && is_secondary && !d.has_self) damage = relay::yes;
  if (!damage.truthy() && damage.kind != Relay::Kind::Number) hit = false;

  // getSpreadDamage, then spreadDamage. With no target the damage is 0.
  if (hit && !target) {
    damage = relay::number(0);
  } else if (hit) {
    b.active_target = target;
    damage = which == HitEffect::Move ? get_damage(c, source, target) : relay::undefined;
    if (damage == relay::no || damage.kind == Relay::Kind::Null) {
      damage = relay::no;
      hit = false;
    }
  }
  if (hit && target && damage.kind == Relay::Kind::Number) {
    damage = move_damage(c, damage.number, target, source);
    if (damage == relay::no) hit = false;
  }
  damage = run_move_effects(c, damage, hit, target, source, which);
  if (!damage.truthy() && damage.kind != Relay::Kind::Number) hit = false;

  // selfDrops: what the effect does to the user. A move's own stat changes
  // are drawn for, though they have no chance to miss.
  if (d.has_self && hit) {
    if (which == HitEffect::Move && id == Move::RapidSpin) {
      // moveHit(source, source, move, move.self, false, true): its onHit.
      const auto parent = c.event;
      c.event = {"Hit", on(source), source, me, 1};
      const Effect current = c.effect;
      c.effect = me;
      rapidspin_self_onHit(c, source);
      c.effect = current;
      c.event = parent;
    } else if (which == HitEffect::Move) {
      if (!b.active_move.self_dropped) {
        random(b, c.out, 100);
        move_hit(c, source, source, HitEffect::Self);
        b.active_move.self_dropped = true;
      }
    } else {
      move_hit(c, source, source, HitEffect::SecondarySelf);
    }
  }
  // secondaries: each is drawn for. A chance on a stat change is taken
  // modulo 256, as the games overflow it.
  if (which == HitEffect::Move && b.active_move.secondary_chance && hit) {
    // With no target the event is run on the battle, where nothing listens.
    const Relay kept = target ? run_event(c, events::MODIFY_SECONDARIES, on(target), source, me, relay::yes) : relay::yes;
    const bool own_only = kept == relay::no; // Shield Dust keeps only what has a `self`
    if (!own_only || !(m.secondary.self_boosts == Boosts{})) {
      const uint32_t roll = random(b, c.out, 100);
      const bool overflow = !(m.secondary.boosts == Boosts{}) || !(m.secondary.self_boosts == Boosts{});
      if (roll < uint32_t(overflow ? b.active_move.secondary_chance % 256 : b.active_move.secondary_chance)) {
        move_hit(c, target, source, HitEffect::Secondary);
      }
    }
  }
  // forceSwitch: the target is flagged to be dragged out when the action is
  // over, unless something holds it in.
  if (which == HitEffect::Move && m.force_switch && hit && target && b.at(target).hp && b.at(source).hp &&
      can_switch(b, target.side)) {
    const Relay dragged = run_event(c, event("DragOut"), on(target), source, me);
    if (dragged.truthy()) {
      b.at(target).force_switch_flag = true;
    } else if (dragged == relay::no && m.category == Category::Status) {
      add(c, {part::text("-fail"), part::pokemon(b, source)});
      attr_last_move(c, "[still]");
      damage = relay::no;
    }
  }
  if (!damage.truthy() && damage.kind != Relay::Kind::Number) hit = false;
  if (hit && target && damage.kind == Relay::Kind::Number && !is_secondary && !is_self) {
    if (has(me, events::ON_AFTER_HIT) && b.at(source).hp) move_event(c, events::ON_AFTER_HIT, target, source);
    run_event(c, events::DAMAGING_HIT, on(target), source, me, damage);
    // Nothing listens for EmergencyExit in Gen 3.
  }
  return damage == relay::yes ? relay::undefined : damage;
}

// BattleActions.applyRecoilDamage (data/mods/gen4/scripts.ts:179).
inline void apply_recoil_damage(Ctx& c, double dealt, Ref user) {
  const Fraction recoil = data(c.b.active_move.id).recoil;
  if (!recoil) return;
  damage(c, std::max(std::floor(dealt * recoil.num / recoil.den), 1.0), user, user, effect(Special::Recoil));
}

// BattleQueue.willAct (battle-queue.ts): whether a move or a switch is still
// queued.
inline bool will_act(const Battle& b) {
  for (int i = 0; i < b.queue.size; i++) {
    const Choice choice = b.queue.list[i].choice;
    if (choice == Choice::Move || choice == Choice::Switch || choice == Choice::InstaSwitch) return true;
  }
  return false;
}

// Gen 3's BattleActions.tryMoveHit (data/mods/gen3/scripts.ts:232) on one
// Pokémon. False or null when the move did not hit.
inline Relay try_move_hit(Ctx& c, Ref target, Ref source) {
  Battle& b = c.b;
  Battle::ActiveMove& move = b.active_move;
  const Effect me = effect(move.id);
  set_active_move(b, source, target);
  Relay prepared = move_event(c, events::ON_PREPARE_HIT, target, source);
  if (prepared.truthy()) prepared = run_event(c, events::PREPARE_HIT, on(source), target, me);
  if (!prepared.truthy()) {
    if (prepared == relay::no) {
      add(c, {part::text("-fail"), part::pokemon(b, source)});
      attr_last_move(c, "[still]");
    }
    return relay::no;
  }
  if (!move_event(c, events::ON_TRY, source, target).truthy()) return relay::no;
  if (is_wide(data(move.id).target)) {
    // Nothing listens for TryHitField or TryHitSide in Gen 3.
    return move_hit(c, target, source);
  }
  if (run_event(c, events::INVULNERABILITY, on(target), source, me) == relay::no) {
    attr_last_move(c, "[miss]");
    add(c, {part::text("-miss"), part::pokemon(b, source), part::pokemon(b, target)});
    return relay::no;
  }
  bool natural_immunity = false;
  if (!run_immunity(c, target, false)) natural_immunity = true;
  else if (move_event(c, events::ON_TRY_IMMUNITY, target, source) == relay::no) natural_immunity = true;

  Relay accuracy = move.accuracy ? relay::number(move.accuracy) : relay::yes;
  if (accuracy.kind == Relay::Kind::Number) {
    // Accuracy and evasion stages are thirds, not halves.
    static constexpr double BOOST_TABLE[] = {1, 4.0 / 3, 5.0 / 3, 2, 7.0 / 3, 8.0 / 3, 3};
    run_event_without_handlers(c, events::MODIFY_BOOST_OF, on(source));
    int boost_ = std::clamp(int(b.at(source).boosts.accuracy), -6, 6);
    if (boost_ > 0) accuracy.number *= BOOST_TABLE[boost_];
    else accuracy.number /= BOOST_TABLE[-boost_];
    run_event_without_handlers(c, events::MODIFY_BOOST_OF, on(target));
    boost_ = std::clamp(int(b.at(target).boosts.evasion), -6, 6);
    if (boost_ > 0) accuracy.number /= BOOST_TABLE[boost_];
    else if (boost_ < 0) accuracy.number *= BOOST_TABLE[-boost_];
  }
  accuracy = run_event(c, events::MODIFY_ACCURACY, on(target), source, me, accuracy);
  accuracy = run_event(c, events::ACCURACY, on(target), source, me, accuracy);
  bool hit = true;
  if (accuracy != relay::yes) {
    if (accuracy.kind != Relay::Kind::Number) unported("an accuracy that is neither true nor a number");
    hit = double(random(b, c.out, 100)) < accuracy.number; // randomChance(accuracy, 100)
  }
  if (hit) {
    const Relay tried = run_event(c, events::TRY_HIT, on(target), source, me);
    if (!tried.truthy()) {
      if (tried == relay::no) {
        add(c, {part::text("-fail"), part::pokemon(b, source)});
        attr_last_move(c, "[still]");
      }
      return relay::no;
    }
    if (natural_immunity) {
      add(c, {part::text("-immune"), part::pokemon(b, target)});
      return relay::no;
    }
  } else {
    if (natural_immunity) {
      add(c, {part::text("-immune"), part::pokemon(b, target)});
    } else {
      attr_last_move(c, "[miss]");
      add(c, {part::text("-miss"), part::pokemon(b, source), part::pokemon(b, target)});
    }
    return relay::no;
  }

  move.total_damage = 0;
  b.at(source).last_damage = 0;
  const Relay damage = move_hit(c, target, source);
  move.total_damage = damage.kind == Relay::Kind::Number ? uint16_t(damage.number) : 0;
  if (move.total_damage) apply_recoil_damage(c, move.total_damage, source);
  // gotAttacked records the hit for Counter and its kind, which are not
  // ported.
  // A hit that dealt no damage, a status move's included, ends here.
  if (damage.kind != Relay::Kind::Number) return damage;
  each_event(c, events::UPDATE_AFTER_HIT);
  move_event(c, events::ON_AFTER_MOVE_SECONDARY, target, source);
  run_event(c, events::AFTER_MOVE_SECONDARY, on(target), source, me);
  return damage;
}

// Gen 3's BattleActions.useMoveInner (data/mods/gen3/scripts.ts:108).
inline bool use_move(Ctx& c, Move id, Ref user, Ref target) {
  Battle& b = c.b;
  const MoveData& m = data(id);
  b.active_move = {.id = id, .type = m.type, .category = m.category, .base_power = m.base_power,
                   .accuracy = m.accuracy, .target = m.target, .crit_ratio = m.crit_ratio,
                   .secondary_chance = m.secondary.chance, .self_boosts = m.self_boosts,
                   .volatile_status = m.volatile_status, .on_hit = true};
  const Effect me = effect(id);
  Pokemon& p = b.at(user);
  const auto self_targeted = [&] {
    const Target t = b.active_move.target;
    return t == Target::Self || t == Target::All || t == Target::AllySide || t == Target::AllyTeam;
  };
  if (self_targeted()) target = user;
  set_active_move(b, user, target);
  // The move's own onModifyMove, then everyone's; they change the active
  // move in place.
  move_event(c, events::ON_MODIFY_MOVE, user, target);
  if (!run_event(c, events::MODIFY_MOVE, on(user), target, me, relay::yes).truthy()) return false;
  // A handler may have changed the move's target (Curse without a Ghost).
  if (b.active_move.target != m.target) target = self_targeted() ? user : Ref{uint8_t(user.side ^ 1), b.sides[user.side ^ 1].active};
  if (p.fainted) return false;
  c.out.last_move_line = int32_t(c.out.log.size());
  add(c, {part::text("move"), part::pokemon(b, user), part::move(id), part::pokemon(b, target)});

  // Pokemon.getMoveTargets (pokemon.ts) in singles: the target, unless it
  // has fainted (a move on one target) or has no HP (a move on all adjacent).
  const Pokemon& aimed = b.at(target);
  const Target aim = b.active_move.target;
  const bool spread = aim == Target::AllAdjacent || aim == Target::AllAdjacentFoes;
  const bool has_target = spread ? aimed.hp != 0 : !aimed.fainted;
  // Pressure: each target may make the move cost more. A move on the field
  // has every Pokémon for a target, the user first.
  int extra_pp = 0;
  const auto pressure = [&](Ref on_) {
    const Relay drop = run_event(c, events::DEDUCT_PP, on(on_), user, me);
    if (drop.kind == Relay::Kind::Number) extra_pp += int(drop.number);
  };
  if (aim == Target::All) {
    pressure(user);
    if (b.sides[user.side ^ 1].active != 0xFF) pressure({uint8_t(user.side ^ 1), b.sides[user.side ^ 1].active});
  } else if (m.flags & flag(MoveFlag::Mustpressure)) {
    // Spikes: the foes with HP, though a move on their side has no target.
    if (const Ref foe = foe_of(b, user)) pressure(foe);
  } else if (aim == Target::FoeSide) {
    // Any other move on the foe's side has no target for Pressure.
  } else if (has_target) {
    pressure(target);
  }
  if (extra_pp > 0) deduct_pp(p, id, extra_pp);
  if (!move_event(c, events::ON_TRY_MOVE, user, target).truthy() ||
      !run_event(c, events::TRY_MOVE, on(user), target, me).truthy()) {
    return false;
  }
  move_event(c, events::ON_USE_MOVE_MESSAGE, user, target);
  if (m.selfdestruct == SelfDestruct::Always) faint(c, user, user, me);
  const bool aimed_at_foe = aim == Target::Normal;
  if (!is_wide(aim) && (!has_target || (aimed_at_foe && !is_adjacent(b, target, user)))) {
    attr_last_move(c, "[notarget]");
    add(c, {part::text("-notarget"), part::pokemon(b, user)});
    return false;
  }
  const Relay damage = try_move_hit(c, target, user);
  const bool result = damage.kind == Relay::Kind::Number || damage.undefined() || damage.truthy();
  if (!p.hp) faint(c, user, user, me);
  if (!result) {
    move_event(c, events::ON_MOVE_FAIL, target, user);
    return false;
  }
  move_event(c, events::ON_AFTER_MOVE_SECONDARY_SELF, user, target);
  run_event(c, events::AFTER_MOVE_SECONDARY_SELF, on(user), target, me);
  return true;
}

// BattleActions.runMove (battle-actions.ts) for a move the Pokémon chose.
inline void run_move(Ctx& c, Move id, Ref user) {
  Battle& b = c.b;
  if (const char* why = unsupported(id)) unported(std::string(why) + ": " + std::string(MOVE_NAMES[uint16_t(id)].id));
  Pokemon& p = b.at(user);
  const Effect me = effect(id);
  p.active_move_actions++;
  // Battle.getTarget (battle.ts) in singles: the user for a move on itself,
  // else the foe's active Pokémon, fainted or not.
  const bool on_self = data(id).target == Target::Self || data(id).target == Target::All ||
                       data(id).target == Target::AllySide || data(id).target == Target::AllyTeam;
  const Ref target = on_self ? user : Ref{uint8_t(user.side ^ 1), b.sides[user.side ^ 1].active};
  if (run_event(c, events::OVERRIDE_ACTION, on(user), target, me) != relay::yes) unported("a move overridden");
  b.active_move = {.id = id, .type = data(id).type, .category = data(id).category};
  set_active_move(b, user, target);
  const Relay will_try = run_event(c, events::BEFORE_MOVE, on(user), target, me);
  if (!will_try.truthy()) {
    run_event(c, events::MOVE_ABORTED, on(user), target, me);
    b.has_active_move = false;
    return;
  }
  if (has(me, Callback::beforeMoveCallback)) unported("a move with a beforeMoveCallback");
  p.last_damage = 0;
  // getLockedMove: nothing locks a move yet.
  if (run_event(c, event("LockMove"), on(user), {}, {}, relay::undefined, false, true) != relay::yes) unported("a locked move");
  if (!deduct_pp(p, id)) unported("a move with no PP");
  p.last_move = id;
  p.move_this_turn = id;
  use_move(c, id, user, target);
  move_event(c, events::ON_AFTER_MOVE, user, target);
  run_event(c, events::AFTER_MOVE, on(user), target, me);
  // In Gen 3 a move runs the Faint event for what it made faint.
  for (int i = 0; i < b.faints; i++) {
    Battle::Faint& f = b.faint_queue[i];
    if (f.faint_event_done) continue;
    f.faint_event_done = true;
    run_event(c, events::FAINT_BY_MOVE, on(f.target), f.source, f.effect);
  }
  resolve_faints(c);
  check_win(c);
}

} // namespace adv
