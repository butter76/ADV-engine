// What handlers call: the parts of Showdown's Battle, Field and Pokemon that
// a callback reaches through `this`, each a port of the method it is named
// for.

#pragma once

#include <algorithm>

#include "events.hpp"

namespace adv {

// Battle.add (battle.ts).
inline void add(Ctx& c, std::initializer_list<Part> parts) {
  Line line;
  bool split = false;
  for (const Part& p : parts) {
    line.parts[line.n++] = p;
    split |= p.kind == Part::Kind::Details || p.kind == Part::Kind::Health;
  }
  c.out.log.push_back(line);
  c.out.log_lines += split ? 3 : 1;
}
// Battle.addSplit(side, secret) with no shared line.
inline void add_split(Ctx& c, int side, std::initializer_list<Part> parts) {
  add(c, parts);
  c.out.log.back().secret_side = int8_t(side);
  c.out.log_lines += 2;
}
// Battle.hint(hint, false, side) (battle.ts): a hint that is not kept from
// being repeated.
inline void hint(Ctx& c, std::string_view text, int side) { add_split(c, side, {part::text("-hint"), part::text(text)}); }

// Battle.getAllActive (battle.ts:1379): p1's, then p2's.
inline int all_active(const Battle& b, std::array<Ref, 2>& out, bool include_fainted = false) {
  int n = 0;
  for (uint8_t s = 0; s < 2; s++) {
    const Side& side = b.sides[s];
    if (side.active != 0xFF && (include_fainted || !side.pokemon[side.active].fainted)) out[n++] = {s, side.active};
  }
  return n;
}

// Battle.eachEvent (battle.ts:470): the event on each active Pokémon, in
// order of speed. With no effect given it passes the current one.
inline void each_event(Ctx& c, const EventNames& e, Effect source_effect = {}) {
  if (!source_effect) source_effect = c.effect;
  std::array<Ref, 2> actives;
  const int n = all_active(c.b, actives);
  speed_sort(c, std::span<Ref>(actives.data(), size_t(n)),
             [&](Ref a, Ref b) { return double(c.b.at(b).speed) - double(c.b.at(a).speed); });
  for (int i = 0; i < n; i++) run_event(c, e, on(actives[i]), {}, source_effect);
}

// An event whose relay value is an object, which the engine cannot pass yet:
// it must find no handler.
inline void run_event_without_handlers(Ctx& c, const EventNames& e, Subject t) {
  Listeners handlers;
  find_event_handlers(c, t, e, {}, handlers);
  if (handlers.n) unported("a handler of " + std::string(e.id));
}

// Battle.initEffectState (battle.ts:3381) for a Pokémon's ability or item:
// an effect of an active Pokémon takes the next effect order.
inline EffectState init_effect_state(Battle& b, bool has_id, Ref target) {
  EffectState state;
  state.target = HolderKind::Pokemon;
  state.target_pokemon = target;
  if (has_id && b.at(target).is_active) state.effect_order = b.effect_order++;
  return state;
}

// ---------------------------------------------------------------------------
// Pokemon (pokemon.ts).
// ---------------------------------------------------------------------------

// Pokemon.side.foes(): the other side's active Pokémon, if it has HP.
inline Ref foe_of(const Battle& b, Ref r) {
  const Side& other = b.sides[r.side ^ 1];
  if (other.active == 0xFF || !other.pokemon[other.active].hp) return {};
  return {uint8_t(r.side ^ 1), other.active};
}
// Pokemon.hasType. Nothing in Gen 3 listens for the Type event getTypes runs.
inline bool has_type(const Pokemon& p, Type type) { return p.types[0] == type || p.types[1] == type; }
// Pokemon.isAdjacent in singles: two different Pokémon, neither fainted.
inline bool is_adjacent(const Battle& b, Ref a, Ref other) {
  return !b.at(a).fainted && !b.at(other).fainted && a != other;
}
// Dex.getImmunity(what, pokemon) (dex.ts) for something that is not a type.
inline bool get_immunity(const Pokemon& p, Immunity to) {
  return !(p.types[0] != Type::None && immune(p.types[0], to)) && !(p.types[1] != Type::None && immune(p.types[1], to));
}

// Pokemon.isGrounded (pokemon.ts) as Gen 3 can reach it: a Flying type is
// not, unless immunity is negated; Levitate gives null, which is falsy.
inline bool is_grounded(const Pokemon& p, bool negate_immunity = false) {
  if (!negate_immunity && has_type(p, Type::Flying)) return false;
  if (p.ability == Ability::Levitate) return false;
  return true;
}

inline constexpr EventNames IMMUNITY = event("Immunity");
inline constexpr EventNames SET_WEATHER = event("SetWeather");
inline constexpr EventNames WEATHER_CHANGE = event("WeatherChange");
inline constexpr EventNames WEATHER = event("Weather");
inline constexpr EventNames TRY_BOOST = event("TryBoost");
inline constexpr Callback ON_FIELD_START = callback("", "FieldStart");

// Pokemon.runStatusImmunity (pokemon.ts) without a message. `what` is the id
// Showdown passes ('trapped', 'frz', 'sandstorm'); `natural` is whether the
// Pokémon's types leave it open to that.
inline bool run_status_immunity(Ctx& c, Ref r, std::string_view what, bool natural = true) {
  if (c.b.at(r).fainted) return false;
  if (!natural) return false;
  return run_event(c, IMMUNITY, on(r), {}, {}, relay::text(what)).truthy();
}

// Pokemon.tryTrap (pokemon.ts). No type of Gen 3 is immune to trapping.
inline bool try_trap(Ctx& c, Ref r, bool hidden = false) {
  if (!run_status_immunity(c, r, "trapped")) return false;
  Pokemon& p = c.b.at(r);
  if (p.trapped != Trapped::No && hidden) return true;
  p.trapped = hidden ? Trapped::Hidden : Trapped::Yes;
  return true;
}

// Pokemon.getCappedBoost (pokemon.ts): each change, cut to what keeps the
// stage within -6 and 6. A change of 0 is dropped.
inline SparseBoosts get_capped_boost(const Pokemon& p, const SparseBoosts& boosts) {
  const int8_t* current = &p.boosts.atk;
  SparseBoosts capped;
  for (int stat = 0; stat < BOOST_STATS; stat++) {
    if (!boosts.has(stat) || !boosts.value[size_t(stat)]) continue;
    capped.set(stat, std::clamp(current[stat] + boosts.value[size_t(stat)], -6, 6) - current[stat]);
  }
  return capped;
}

// ---------------------------------------------------------------------------
// Field (field.ts).
// ---------------------------------------------------------------------------

// Pokemon.effectiveWeather (pokemon.ts:2212) and Field.effectiveWeather.
inline Condition effective_weather(const Battle& b) {
  return suppressing_weather(b) ? Condition::None : b.field.weather;
}

// Field.setWeather (field.ts) from a handler: the source is the event's
// target and the source effect the handler's own effect.
inline bool set_weather(Ctx& c, Condition weather) {
  Field& f = c.b.field;
  const Effect source_effect = c.effect;
  const Ref source = c.event.target.kind == HolderKind::Pokemon ? c.event.target.pokemon : Ref{};
  if (f.weather == weather) {
    // Weather an ability set has no duration and is not set again; any
    // other is not replaced by itself either.
    if (source_effect.kind != EffectKind::Ability || f.weather_state.duration == 0) return false;
  }
  if (source && !run_event(c, SET_WEATHER, on(source), source, effect(weather)).truthy()) {
    unported("a weather refused by SetWeather");
  }
  const Condition previous = f.weather;
  const EffectState previous_state = f.weather_state;
  f.weather = weather;
  f.weather_state = EffectState{};
  f.weather_state.duration = data(weather).duration;
  // durationCallback(source, source, sourceEffect).
  if (has(effect(weather), Callback::durationCallback)) {
    if (!source) unported("a weather set with no source");
    f.weather_state.duration =
        uint8_t(call(c, {effect(weather), Callback::durationCallback}, relay::undefined, on(source), source, source_effect).number);
  }
  if (!single_event(c, ON_FIELD_START, effect(weather), &f.weather_state, FIELD, source, source_effect).truthy()) {
    f.weather = previous;
    f.weather_state = previous_state;
    return false;
  }
  each_event(c, WEATHER_CHANGE, source_effect);
  return true;
}

// ---------------------------------------------------------------------------
// Battle.boost (battle.ts): from a handler, so the effect is the handler's.
// ---------------------------------------------------------------------------
inline void boost(Ctx& c, SparseBoosts boosts, Ref target, Ref source, bool is_secondary = false, bool is_self = false) {
  const Effect effect = c.effect;
  Pokemon& p = c.b.at(target);
  if (!p.hp || !p.is_active) return;
  // Nothing listens for ChangeBoost in Gen 3.
  boosts = get_capped_boost(p, boosts);
  const Relay tried = run_event(c, TRY_BOOST, on(target), source, effect, relay::boosts(boosts));
  if (tried.kind != Relay::Kind::Boosts) unported("a TryBoost handler that returns something other than the boosts");
  boosts = tried.boosts;
  bool success = false;
  bool boosted = is_secondary;
  int8_t* stage = &p.boosts.atk;
  for (int stat = 0; stat < BOOST_STATS; stat++) {
    if (!boosts.has(stat)) continue;
    // boostBy: the change, capped again, is applied.
    SparseBoosts one;
    one.set(stat, boosts.value[size_t(stat)]);
    int by = get_capped_boost(p, one).value[size_t(stat)];
    stage[stat] = int8_t(stage[stat] + by);
    std::string_view message = "-boost";
    if (boosts.value[size_t(stat)] < 0 || stage[stat] == -6) {
      message = "-unboost";
      by = -by;
    }
    const std::initializer_list<Part> line = {part::text(message), part::pokemon(c.b, target),
                                              part::text(BOOST_NAMES[stat]), part::number(by)};
    if (by) {
      success = true;
      if (effect.kind == EffectKind::Move || effect.kind == EffectKind::Item) unported("a boost from a move or an item");
      if (effect) {
        if (effect.kind == EffectKind::Ability && !boosted) {
          add(c, {part::text("-ability"), part::pokemon(c.b, target), part::ability(Ability(effect.id)), part::text("boost")});
          boosted = true;
        }
        add(c, line);
      }
      // Nothing listens for AfterEachBoost in Gen 3.
    } else if (effect.kind == EffectKind::Ability && effect) {
      if (is_secondary || is_self) add(c, line);
    } else if (!is_secondary && !is_self) {
      add(c, line);
    }
  }
  // Nothing listens for AfterBoost in Gen 3.
  if (success) {
    for (int stat = 0; stat < BOOST_STATS; stat++) {
      if (boosts.has(stat) && boosts.value[size_t(stat)] > 0) p.stats_raised_this_turn = true;
      if (boosts.has(stat) && boosts.value[size_t(stat)] < 0) p.stats_lowered_this_turn = true;
    }
  }
}

// ---------------------------------------------------------------------------
// Damage, healing and fainting.
// ---------------------------------------------------------------------------
inline constexpr EventNames DAMAGE = event("Damage");
inline constexpr EventNames TRY_HEAL = event("TryHeal");

// Pokemon.faint (pokemon.ts): the Pokémon joins the faint queue.
inline void faint(Ctx& c, Ref r, Ref source, Effect effect) {
  Pokemon& p = c.b.at(r);
  if (p.fainted || p.faint_queued) return;
  p.hp = 0;
  p.switch_flag = false;
  p.faint_queued = true;
  if (c.b.faints == c.b.faint_queue.size()) unported("more than four queued faints");
  c.b.faint_queue[c.b.faints++] = {r, source, effect};
}

// Battle.damage (battle.ts) from a handler, which is spreadDamage for one
// target: the target is the event's, the source the event's, and the effect
// the handler's. The amount is Showdown's number, floored to at least 1.
inline void damage(Ctx& c, double amount) {
  if (c.event.target.kind != HolderKind::Pokemon) unported("damage with no Pokémon target");
  const Ref target = c.event.target.pokemon;
  const Ref source = c.event.source;
  const Effect effect = c.effect;
  Pokemon& p = c.b.at(target);
  if (!p.hp || !p.is_active) return;
  if (amount != 0) amount = std::max(std::floor(amount), 1.0);
  if (type_of(effect) == EffectType::Weather) {
    const Immunity to = Condition(effect.id) == Condition::Sandstorm ? Immunity::Sandstorm : Immunity::Hail;
    if (!run_status_immunity(c, target, CONDITION_NAMES[effect.id].id, get_immunity(p, to))) return;
  }
  const Relay after = run_event(c, DAMAGE, on(target), source, effect, relay::number(amount), true);
  if (after.kind != Relay::Kind::Number) return; // the event refused the damage
  amount = after.number;
  if (amount != 0) amount = std::max(std::floor(amount), 1.0);
  // Pokemon.damage (pokemon.ts).
  if (amount > 0) {
    const uint16_t d = uint16_t(std::min<double>(amount, p.hp));
    p.hp = uint16_t(p.hp - d);
    if (!p.hp) faint(c, target, source, effect);
  }
  if (effect.kind == EffectKind::Move || effect.kind == EffectKind::Rule) unported("damage from a move");
  if (effect.kind == EffectKind::Condition &&
      (Condition(effect.id) == Condition::Partiallytrapped || Condition(effect.id) == Condition::Confusion)) {
    unported("damage from binding or confusion");
  }
  if (source && (source != target || effect.kind == EffectKind::Ability)) {
    add(c, {part::text("-damage"), part::pokemon(c.b, target), part::health(c.b, target), part::from(effect), part::of(c.b, source)});
  } else {
    add(c, {part::text("-damage"), part::pokemon(c.b, target), part::health(c.b, target), part::from(effect)});
  }
}

// Battle.damage(amount, target, source) from a handler: as above, with the
// target and the source given (Rough Skin hurts the attacker).
inline void damage(Ctx& c, double amount, Ref target, Ref source) {
  const auto event = c.event;
  c.event.target = on(target);
  c.event.source = source;
  damage(c, amount);
  c.event = event;
}

// Battle.heal (battle.ts) from a handler, with the same target, source and
// effect as damage. It returns what was healed, 0 when nothing was.
inline double heal(Ctx& c, double amount) {
  if (c.event.target.kind != HolderKind::Pokemon) unported("healing with no Pokémon target");
  const Ref target = c.event.target.pokemon;
  const Ref source = c.event.source;
  const Effect effect = c.effect;
  if (amount && amount <= 1) amount = 1;
  amount = std::floor(amount);
  const Relay tried = run_event(c, TRY_HEAL, on(target), source, effect, relay::number(amount));
  if (!tried.truthy()) return 0;
  amount = tried.number;
  Pokemon& p = c.b.at(target);
  if (!p.hp || !p.is_active || p.hp >= p.maxhp) return 0;
  const uint16_t before = p.hp;
  p.hp = uint16_t(std::min<double>(p.hp + amount, p.maxhp)); // Pokemon.heal
  if (effect.kind == EffectKind::Move) unported("healing from a move");
  if (source && source != target) {
    add(c, {part::text("-heal"), part::pokemon(c.b, target), part::health(c.b, target), part::from(effect), part::of(c.b, source)});
  } else {
    add(c, {part::text("-heal"), part::pokemon(c.b, target), part::health(c.b, target), part::from(effect)});
  }
  // Nothing listens for Heal in Gen 3.
  return p.hp - before;
}

// ---------------------------------------------------------------------------
// Volatiles (pokemon.ts).
// ---------------------------------------------------------------------------
inline constexpr EventNames TRY_ADD_VOLATILE = event("TryAddVolatile");
inline constexpr Callback ON_RESTART = callback("", "Restart");
inline constexpr Callback ON_VOLATILE_END = callback("", "End");

inline Volatile* volatile_of(Pokemon& p, Condition id) {
  for (int i = 0; i < p.n_volatiles; i++) {
    if (p.volatiles[i].id == id) return &p.volatiles[i];
  }
  return nullptr;
}

// Pokemon.addVolatile(status) from a handler: the source is the event's, or
// the Pokémon itself, and the source effect the handler's own.
inline bool add_volatile(Ctx& c, Ref r, Condition id, Ref source = {}) {
  Pokemon& p = c.b.at(r);
  if (!p.hp && !data(id).affects_fainted) return false;
  if (!source) source = c.event.source;
  const Effect source_effect = c.effect;
  if (!source) source = r;
  if (Volatile* v = volatile_of(p, id)) {
    if (!has(effect(id), ON_RESTART)) return false;
    return single_event(c, ON_RESTART, effect(id), &v->state, on(r), source, source_effect).truthy();
  }
  if (!run_status_immunity(c, r, CONDITION_NAMES[uint8_t(id)].id)) return false;
  if (!run_event(c, TRY_ADD_VOLATILE, on(r), source, source_effect, relay::text(CONDITION_NAMES[uint8_t(id)].id)).truthy()) {
    return false;
  }
  if (p.n_volatiles == p.volatiles.size()) unported("more than eight volatiles");
  Volatile& v = p.volatiles[p.n_volatiles++];
  v.id = id;
  v.state = init_effect_state(c.b, true, r);
  v.state.source = source;
  v.state.source_effect = source_effect;
  v.state.duration = data(id).duration;
  if (has(effect(id), Callback::durationCallback)) unported("a volatile with a durationCallback");
  if (!single_event(c, ON_START, effect(id), &v.state, on(r), source, source_effect).truthy()) {
    // The volatile is taken off again; it is the last unless its onStart
    // added another.
    for (int i = 0; i < p.n_volatiles; i++) {
      if (p.volatiles[i].id != id) continue;
      for (int k = i + 1; k < p.n_volatiles; k++) p.volatiles[k - 1] = p.volatiles[k];
      p.n_volatiles--;
      break;
    }
    return false;
  }
  return true;
}

// Pokemon.removeVolatile.
inline bool remove_volatile(Ctx& c, Ref r, Condition id) {
  Pokemon& p = c.b.at(r);
  if (!p.hp) return false;
  Volatile* v = volatile_of(p, id);
  if (!v) return false;
  single_event(c, ON_VOLATILE_END, effect(id), &v->state, on(r));
  for (int i = 0; i < p.n_volatiles; i++) {
    if (p.volatiles[i].id != id) continue;
    for (int k = i + 1; k < p.n_volatiles; k++) p.volatiles[k - 1] = p.volatiles[k];
    p.n_volatiles--;
    break;
  }
  return true;
}

} // namespace adv
