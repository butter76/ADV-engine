// Using a move: BattleActions.runMove and useMove (battle-actions.ts), Gen
// 3's useMoveInner, tryMoveHit and modifyDamage (data/mods/gen3/scripts.ts),
// and the hit itself (moveHit, spreadMoveHit, getDamage).
//
// So far only a plain damaging move can be used: one with power, a type, one
// target, no secondary effect and no handler of its own (is_plain). Every
// event of the pipeline is run all the same, so that abilities and items
// take their part and the handlers are gathered and sorted as Showdown's.

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

// Whether the engine can use a move yet: power, a type, one target, and
// nothing else. tools/lib.cjs has the same test for the recorder.
inline bool is_plain(Move move) {
  static const std::array<bool, MOVES> plain = [] {
    std::array<bool, MOVES> table{};
    for (int i = 1; i < MOVES; i++) {
      const MoveData& m = MOVE_DATA[i];
      bool ok = m.category != Category::Status && m.base_power && m.type != Type::Unknown && m.real_move == Move::None &&
                (m.target == Target::Normal || m.target == Target::Any || m.target == Target::AllAdjacent ||
                 m.target == Target::AllAdjacentFoes) &&
                !m.secondary.chance && !m.drain && !m.recoil && !m.heal && !m.multihit[1] && !m.ohko && !m.damage &&
                !m.damage_level && m.selfdestruct == SelfDestruct::None && m.self_switch == SelfSwitch::None &&
                !m.force_switch && m.status == Condition::None && m.volatile_status == Condition::None &&
                m.side_condition == Condition::None && m.slot_condition == Condition::None &&
                m.weather == Condition::None && m.boosts == Boosts{} && m.self_boosts == Boosts{} &&
                m.self_volatile_status == Condition::None && !m.sleep_usable && !m.stalling_move && !m.calls_move &&
                !m.has_crash_damage && !m.multiaccuracy && !m.ignore_immunity;
      // No handler of its own, and no condition of its own.
      for (const Handler& h : HANDLERS) ok &= !(h.kind == EffectKind::Move && h.id == i);
      for (int k = 1; k < CONDITIONS; k++) ok &= CONDITION_NAMES[k].id != MOVE_NAMES[i].id;
      table[size_t(i)] = ok;
    }
    return table;
  }();
  return plain[uint16_t(move)];
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

// BattleActions.getDamage (battle-actions.ts) for a plain move. False when
// the target is immune; otherwise the damage.
inline Relay get_damage(Ctx& c, Ref source, Ref target) {
  Battle& b = c.b;
  Battle::ActiveMove& move = b.active_move;
  const Effect me = effect(move.id);
  if (!run_immunity(c, target, true)) return relay::no;
  double base_power = move.base_power;
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
  const double level = attacker.level;
  const double base_damage = trunc(trunc(trunc(trunc(2 * level / 5 + 2) * base_power * attack) / defense) / 50);
  return relay::number(modify_damage(c, base_damage, source, target));
}

// Battle.spreadDamage (battle.ts) for a move's damage on its one target.
inline double move_damage(Ctx& c, double amount, Ref target, Ref source) {
  Battle& b = c.b;
  const Effect me = effect(b.active_move.id);
  Pokemon& p = b.at(target);
  if (!p.hp || !p.is_active) return 0;
  if (amount != 0) amount = std::max(std::floor(amount), 1.0);
  const Relay after = run_event(c, DAMAGE, on(target), source, me, relay::number(amount), true);
  if (after.kind != Relay::Kind::Number) unported("a move's damage refused by the Damage event");
  amount = after.number;
  if (amount != 0) amount = std::max(std::floor(amount), 1.0);
  // Pokemon.damage (pokemon.ts).
  const uint16_t dealt = uint16_t(std::min<double>(amount, p.hp));
  p.hp = uint16_t(p.hp - dealt);
  if (!p.hp) faint(c, target, source, me);
  b.at(source).last_damage = dealt;
  add(c, {part::text("-damage"), part::pokemon(b, target), part::health(b, target)});
  return dealt;
}

// BattleActions.moveHit (battle-actions.ts), which is spreadMoveHit for one
// target and the move itself. False when the hit failed.
inline Relay move_hit(Ctx& c, Ref target, Ref source) {
  Battle& b = c.b;
  const Effect me = effect(b.active_move.id);
  // singleEvent('TryHit', move): no plain move has the handler.
  if (!run_event(c, events::TRY_PRIMARY_HIT, on(target), source, me).truthy()) {
    unported("a hit stopped by TryPrimaryHit");
  }
  b.active_target = target;
  const Relay calculated = get_damage(c, source, target);
  if (calculated.kind != Relay::Kind::Number) return relay::no;
  const double dealt = move_damage(c, calculated.number, target, source);
  // runMoveEffects: a plain move has no effect of its own; Hit is run.
  run_event(c, events::HIT, on(target), source, me);
  // Nothing of a plain move drops its user's stats, has a secondary effect
  // or forces a switch.
  run_event(c, events::DAMAGING_HIT, on(target), source, me, relay::number(dealt));
  // Nothing listens for EmergencyExit in Gen 3.
  return relay::number(dealt);
}

// Gen 3's BattleActions.tryMoveHit (data/mods/gen3/scripts.ts:232) on one
// Pokémon. False when the move did not hit.
inline Relay try_move_hit(Ctx& c, Ref target, Ref source) {
  Battle& b = c.b;
  Battle::ActiveMove& move = b.active_move;
  const Effect me = effect(move.id);
  set_active_move(b, source, target);
  // singleEvent('PrepareHit', move) and singleEvent('Try', move): no plain
  // move has the handlers.
  const Relay prepared = run_event(c, events::PREPARE_HIT, on(source), target, me);
  if (!prepared.truthy()) unported("a hit stopped by PrepareHit");
  if (run_event(c, events::INVULNERABILITY, on(target), source, me) == relay::no) {
    attr_last_move(c, "[miss]");
    add(c, {part::text("-miss"), part::pokemon(b, source), part::pokemon(b, target)});
    return relay::no;
  }
  // singleEvent('TryImmunity', move) has no handler either.
  const bool natural_immunity = !run_immunity(c, target, false);

  Relay accuracy = move.accuracy ? relay::number(move.accuracy) : relay::yes;
  if (accuracy.kind == Relay::Kind::Number) {
    // Accuracy and evasion stages are thirds, not halves.
    static constexpr double BOOST_TABLE[] = {1, 4.0 / 3, 5.0 / 3, 2, 7.0 / 3, 8.0 / 3, 3};
    run_event_without_handlers(c, events::MODIFY_BOOST_OF, on(source));
    int boost = std::clamp(int(b.at(source).boosts.accuracy), -6, 6);
    if (boost > 0) accuracy.number *= BOOST_TABLE[boost];
    else accuracy.number /= BOOST_TABLE[-boost];
    run_event_without_handlers(c, events::MODIFY_BOOST_OF, on(target));
    boost = std::clamp(int(b.at(target).boosts.evasion), -6, 6);
    if (boost > 0) accuracy.number /= BOOST_TABLE[boost];
    else if (boost < 0) accuracy.number *= BOOST_TABLE[-boost];
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
  // applyRecoilDamage: a plain move has no recoil. gotAttacked records the
  // hit for Counter and its kind, which are not ported.
  if (damage.kind != Relay::Kind::Number) return damage;
  each_event(c, events::UPDATE_AFTER_HIT);
  // singleEvent('AfterMoveSecondary', move) has no handler.
  run_event(c, events::AFTER_MOVE_SECONDARY, on(target), source, me);
  return damage;
}

// Gen 3's BattleActions.useMoveInner (data/mods/gen3/scripts.ts:108).
inline bool use_move(Ctx& c, Move id, Ref user, Ref target) {
  Battle& b = c.b;
  const MoveData& m = data(id);
  b.active_move = {.id = id, .type = m.type, .category = m.category, .base_power = m.base_power,
                   .accuracy = m.accuracy, .target = m.target, .crit_ratio = m.crit_ratio};
  const Effect me = effect(id);
  Pokemon& p = b.at(user);
  set_active_move(b, user, target);
  // singleEvent('ModifyMove', move): no plain move has the handler.
  if (!run_event(c, events::MODIFY_MOVE, on(user), target, me, relay::yes).truthy()) return false;
  if (p.fainted) return false;
  c.out.last_move_line = int32_t(c.out.log.size());
  add(c, {part::text("move"), part::pokemon(b, user), part::move(id), part::pokemon(b, target)});

  // Pokemon.getMoveTargets (pokemon.ts) in singles: the foe, unless it has
  // fainted (a move on one target) or has no HP (a move on all adjacent).
  const Pokemon& foe = b.at(target);
  const bool spread = m.target == Target::AllAdjacent || m.target == Target::AllAdjacentFoes;
  const bool has_target = spread ? foe.hp != 0 : !foe.fainted;
  if (has_target) {
    // Pressure: each target may make the move cost more.
    const Relay drop = run_event(c, events::DEDUCT_PP, on(target), user, me);
    if (drop.kind == Relay::Kind::Number && drop.number > 0) deduct_pp(p, id, int(drop.number));
  }
  // singleEvent('TryMove', move) has no handler.
  if (!run_event(c, events::TRY_MOVE, on(user), target, me).truthy()) return false;
  if (!has_target || (!spread && !is_adjacent(b, target, user))) {
    attr_last_move(c, "[notarget]");
    add(c, {part::text("-notarget"), part::pokemon(b, user)});
    return false;
  }
  const Relay damage = try_move_hit(c, target, user);
  const bool result = damage.kind == Relay::Kind::Number;
  if (!p.hp) faint(c, user, user, me);
  if (!result) return false; // singleEvent('MoveFail', move) has no handler
  // singleEvent('AfterMoveSecondarySelf', move) has no handler.
  run_event(c, events::AFTER_MOVE_SECONDARY_SELF, on(user), target, me);
  return true;
}

// BattleActions.runMove (battle-actions.ts) for a move the Pokémon chose.
inline void run_move(Ctx& c, Move id, Ref user) {
  Battle& b = c.b;
  if (!is_plain(id)) unported("move " + std::string(MOVE_NAMES[uint16_t(id)].id));
  Pokemon& p = b.at(user);
  const Effect me = effect(id);
  p.active_move_actions++;
  // Battle.getTarget (battle.ts) in singles: the foe's active Pokémon,
  // fainted or not.
  const Ref target{uint8_t(user.side ^ 1), b.sides[user.side ^ 1].active};
  if (run_event(c, events::OVERRIDE_ACTION, on(user), target, me) != relay::yes) unported("a move overridden");
  set_active_move(b, user, target);
  const Relay will_try = run_event(c, events::BEFORE_MOVE, on(user), target, me);
  if (!will_try.truthy()) {
    run_event(c, events::MOVE_ABORTED, on(user), target, me);
    b.has_active_move = false;
    return;
  }
  p.last_damage = 0;
  // getLockedMove: nothing locks a move yet.
  if (run_event(c, event("LockMove"), on(user), {}, {}, relay::undefined, false, true) != relay::yes) unported("a locked move");
  if (!deduct_pp(p, id)) unported("a move with no PP");
  p.last_move = id;
  p.move_this_turn = id;
  use_move(c, id, user, target);
  // singleEvent('AfterMove', move) has no handler.
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
