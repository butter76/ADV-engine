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
  capped.first = boosts.first;
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
inline bool set_weather(Ctx& c, Condition weather, Ref source, Effect source_effect) {
  Field& f = c.b.field;
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
// From a handler: the source is the event's target and the source effect the
// handler's own effect.
inline bool set_weather(Ctx& c, Condition weather) {
  return set_weather(c, weather, c.event.target.kind == HolderKind::Pokemon ? c.event.target.pokemon : Ref{}, c.effect);
}
// Field.clearWeather (field.ts:91).
inline bool clear_weather(Ctx& c) {
  Field& f = c.b.field;
  if (f.weather == Condition::None) return false;
  single_event(c, callback("", "FieldEnd"), effect(f.weather), &f.weather_state, FIELD);
  f.weather = Condition::None;
  f.weather_state = EffectState{};
  each_event(c, WEATHER_CHANGE);
  return true;
}

// ---------------------------------------------------------------------------
// Battle.boost (battle.ts). It returns Showdown's result: true when a stage
// changed, null when none did, 0 for a target with no HP and false for one
// that is not active.
// ---------------------------------------------------------------------------
inline Relay boost(Ctx& c, SparseBoosts boosts, Ref target, Ref source, Effect effect, bool is_secondary = false,
                   bool is_self = false) {
  Pokemon& p = c.b.at(target);
  if (!p.hp) return relay::number(0);
  if (!p.is_active) return relay::no;
  // Nothing listens for ChangeBoost in Gen 3.
  boosts = get_capped_boost(p, boosts);
  const Relay tried = run_event(c, TRY_BOOST, on(target), source, effect, relay::boosts(boosts));
  if (tried.kind != Relay::Kind::Boosts) unported("a TryBoost handler that returns something other than the boosts");
  boosts = tried.boosts;
  bool success = false;
  bool boosted = is_secondary;
  int8_t* stage = &p.boosts.atk;
  for (int place = 0; place < BOOST_STATS; place++) {
    const int stat = boosts.at(place);
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
      if (effect) {
        if (effect.kind == EffectKind::Move) {
          if (Move(effect.id) == Move::BellyDrum) unported("a boost from Belly Drum");
          add(c, line);
        } else if (effect.kind == EffectKind::Item) {
          c.out.log.push_back(Line{});
          Line& l = c.out.log.back();
          for (const Part& part_ : line) l.parts[l.n++] = part_;
          l.parts[l.n++] = part::from(effect);
          c.out.log_lines++;
        } else {
          if (effect.kind == EffectKind::Ability && !boosted) {
            add(c, {part::text("-ability"), part::pokemon(c.b, target), part::ability(Ability(effect.id)), part::text("boost")});
            boosted = true;
          }
          add(c, line);
        }
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
  return success ? relay::yes : relay::null;
}
// From a handler: the effect is the handler's own.
inline Relay boost(Ctx& c, SparseBoosts boosts, Ref target, Ref source, bool is_secondary = false, bool is_self = false) {
  return boost(c, boosts, target, source, c.effect, is_secondary, is_self);
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
inline double damage(Ctx& c, double amount) {
  if (c.event.target.kind != HolderKind::Pokemon) unported("damage with no Pokémon target");
  const Ref target = c.event.target.pokemon;
  const Ref source = c.event.source;
  const Effect effect = c.effect;
  Pokemon& p = c.b.at(target);
  if (!p.hp || !p.is_active) return 0;
  if (amount != 0) amount = std::max(std::floor(amount), 1.0);
  if (type_of(effect) == EffectType::Weather) {
    const Immunity to = Condition(effect.id) == Condition::Sandstorm ? Immunity::Sandstorm : Immunity::Hail;
    if (!run_status_immunity(c, target, CONDITION_NAMES[effect.id].id, get_immunity(p, to))) return 0;
  }
  const Relay after = run_event(c, DAMAGE, on(target), source, effect, relay::number(amount), true);
  if (after.kind != Relay::Kind::Number) return 0; // the event refused the damage
  amount = after.number;
  if (amount != 0) amount = std::max(std::floor(amount), 1.0);
  // Pokemon.damage (pokemon.ts).
  uint16_t dealt = 0;
  if (amount > 0) {
    dealt = uint16_t(std::min<double>(amount, p.hp));
    p.hp = uint16_t(p.hp - dealt);
    if (!p.hp) faint(c, target, source, effect);
  }
  if (effect.kind == EffectKind::Move || effect.kind == EffectKind::Rule) unported("damage from a move outside its hit");
  if (effect.kind == EffectKind::Condition &&
      (Condition(effect.id) == Condition::Partiallytrapped || Condition(effect.id) == Condition::Confusion)) {
    unported("damage from binding or confusion");
  }
  if (source && (source != target || effect.kind == EffectKind::Ability)) {
    add(c, {part::text("-damage"), part::pokemon(c.b, target), part::health(c.b, target), part::from(effect), part::of(c.b, source)});
  } else {
    add(c, {part::text("-damage"), part::pokemon(c.b, target), part::health(c.b, target), part::from(effect)});
  }
  return dealt;
}

// Battle.damage(amount, target, source) from a handler: as above, with the
// target and the source given (Rough Skin hurts the attacker).
inline double damage(Ctx& c, double amount, Ref target, Ref source) {
  const auto event = c.event;
  c.event.target = on(target);
  c.event.source = source;
  const double dealt = damage(c, amount);
  c.event = event;
  return dealt;
}
// And with the effect given (a move's recoil).
inline double damage(Ctx& c, double amount, Ref target, Ref source, Effect effect) {
  const Effect current = c.effect;
  c.effect = effect;
  const double dealt = damage(c, amount, target, source);
  c.effect = current;
  return dealt;
}

// Battle.directDamage(amount) (battle.ts) from a handler: damage that no
// event can change, on the event's target.
inline void direct_damage(Ctx& c, double amount) {
  if (c.event.target.kind != HolderKind::Pokemon) unported("direct damage with no Pokémon target");
  const Ref target = c.event.target.pokemon;
  Pokemon& p = c.b.at(target);
  if (!p.hp || !amount) return;
  amount = std::max(std::floor(amount), 1.0);
  const uint16_t dealt = uint16_t(std::min<double>(amount, p.hp));
  p.hp = uint16_t(p.hp - dealt);
  if (!p.hp) faint(c, target, c.event.source, c.effect);
  add(c, {part::text("-damage"), part::pokemon(c.b, target), part::health(c.b, target)});
}

// Battle.heal (battle.ts) from a handler, with the same target, source and
// effect as damage. It returns what was healed, 0 when nothing was.
inline double heal(Ctx& c, double amount, Ref target, Ref source, Effect effect) {
  if (amount && amount <= 1) amount = 1;
  amount = std::floor(amount);
  const Relay tried = run_event(c, TRY_HEAL, on(target), source, effect, relay::number(amount));
  if (!tried.truthy()) return 0;
  amount = tried.number;
  Pokemon& p = c.b.at(target);
  if (!p.hp || !p.is_active || p.hp >= p.maxhp) return 0;
  const uint16_t before = p.hp;
  p.hp = uint16_t(std::min<double>(p.hp + amount, p.maxhp)); // Pokemon.heal
  if (effect == adv::effect(Condition::Wish)) {
    // Wish writes its own line.
  } else if (effect == adv::effect(Move::Rest) || effect == adv::effect(Condition::LeechSeed)) {
    add(c, {part::text("-heal"), part::pokemon(c.b, target), part::health(c.b, target), part::text("[silent]")});
  } else if (effect.kind == EffectKind::Move) {
    add(c, {part::text("-heal"), part::pokemon(c.b, target), part::health(c.b, target)});
  } else if (source && (source != target || effect == adv::effect(Special::Drain))) {
    add(c, {part::text("-heal"), part::pokemon(c.b, target), part::health(c.b, target), part::from(effect), part::of(c.b, source)});
  } else {
    add(c, {part::text("-heal"), part::pokemon(c.b, target), part::health(c.b, target), part::from(effect)});
  }
  // Nothing listens for Heal in Gen 3.
  return p.hp - before;
}
// From a handler: the target and the source are the event's and the effect
// the handler's own.
inline double heal(Ctx& c, double amount) {
  if (c.event.target.kind != HolderKind::Pokemon) unported("healing with no Pokémon target");
  return heal(c, amount, c.event.target.pokemon, c.event.source, c.effect);
}

// ---------------------------------------------------------------------------
// Volatiles (pokemon.ts).
// ---------------------------------------------------------------------------
inline constexpr EventNames TRY_ADD_VOLATILE = event("TryAddVolatile");
inline constexpr Callback ON_RESTART = callback("", "Restart");
inline constexpr Callback ON_VOLATILE_END = callback("", "End");

// A removed volatile leaves its slot behind with no id until the turn ends,
// so that a handler gathered before the removal still has its state, as
// Showdown's still holds the object.
inline Volatile* volatile_of(Pokemon& p, Condition id) {
  if (id == Condition::None) return nullptr;
  for (int i = 0; i < p.n_volatiles; i++) {
    if (p.volatiles[i].id == id) return &p.volatiles[i];
  }
  return nullptr;
}
inline void compact_volatiles(Pokemon& p) {
  int kept = 0;
  for (int i = 0; i < p.n_volatiles; i++) {
    if (p.volatiles[i].id != Condition::None) p.volatiles[kept++] = p.volatiles[i];
  }
  p.n_volatiles = uint8_t(kept);
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
    if (Volatile* added = volatile_of(p, id)) added->id = Condition::None;
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
  if (Volatile* still = volatile_of(p, id)) still->id = Condition::None;
  return true;
}

// ---------------------------------------------------------------------------
// Side conditions (side.ts). A removed one keeps its slot until the turn
// ends, as a removed volatile does.
// ---------------------------------------------------------------------------
inline SideCondition* side_condition_of(Side& s, Condition id) {
  if (id == Condition::None) return nullptr;
  for (int i = 0; i < s.n_side_conditions; i++) {
    if (s.side_conditions[i].id == id) return &s.side_conditions[i];
  }
  return nullptr;
}
inline void compact_side_conditions(Side& s) {
  int kept = 0;
  for (int i = 0; i < s.n_side_conditions; i++) {
    if (s.side_conditions[i].id != Condition::None) s.side_conditions[kept++] = s.side_conditions[i];
  }
  s.n_side_conditions = uint8_t(kept);
}

// Side.addSideCondition(status, source, sourceEffect).
inline bool add_side_condition(Ctx& c, int side, Condition id, Ref source, Effect source_effect) {
  Side& s = c.b.sides[side];
  if (SideCondition* sc = side_condition_of(s, id)) {
    if (!has(effect(id), callback("", "SideRestart"))) return false;
    return single_event(c, callback("", "SideRestart"), effect(id), &sc->state, on_side(side), source, source_effect).truthy();
  }
  if (s.n_side_conditions == s.side_conditions.size()) unported("more than six side conditions");
  SideCondition& sc = s.side_conditions[s.n_side_conditions++];
  sc.id = id;
  sc.state = EffectState{};
  sc.state.effect_order = c.b.effect_order++; // a side's effect always takes an order
  sc.state.target = HolderKind::Side;
  sc.state.target_side = uint8_t(side);
  sc.state.source = source;
  sc.state.duration = data(id).duration;
  if (has(effect(id), Callback::durationCallback)) {
    // durationCallback(the side's active Pokémon, source, sourceEffect).
    const Ref active{uint8_t(side), s.active};
    sc.state.duration = uint8_t(call(c, {effect(id), Callback::durationCallback}, relay::undefined, on(active), source, source_effect).number);
  }
  if (!single_event(c, callback("", "SideStart"), effect(id), &sc.state, on_side(side), source, source_effect).truthy()) {
    if (SideCondition* added = side_condition_of(s, id)) added->id = Condition::None;
    return false;
  }
  // Nothing listens for SideConditionStart in Gen 3.
  return true;
}

// Side.addSlotCondition and removeSlotCondition, for the side's one slot.
inline SideCondition* slot_condition_of(Side& s, Condition id) {
  for (int i = 0; id != Condition::None && i < s.n_slot_conditions; i++) {
    if (s.slot_conditions[i].id == id) return &s.slot_conditions[i];
  }
  return nullptr;
}
inline bool add_slot_condition(Ctx& c, int side, Condition id, Ref source, Effect source_effect) {
  Side& s = c.b.sides[side];
  if (slot_condition_of(s, id)) {
    if (has(effect(id), ON_RESTART)) unported("a slot condition's onRestart");
    return false;
  }
  int kept = 0;
  for (int i = 0; i < s.n_slot_conditions; i++) {
    if (s.slot_conditions[i].id != Condition::None) s.slot_conditions[kept++] = s.slot_conditions[i];
  }
  s.n_slot_conditions = uint8_t(kept);
  if (s.n_slot_conditions == s.slot_conditions.size()) unported("more than two slot conditions");
  SideCondition& sc = s.slot_conditions[s.n_slot_conditions++];
  sc.id = id;
  sc.state = EffectState{};
  sc.state.effect_order = c.b.effect_order++;
  sc.state.target = HolderKind::Side;
  sc.state.target_side = uint8_t(side);
  sc.state.source = source;
  sc.state.duration = data(id).duration;
  const Ref active{uint8_t(side), s.active};
  if (!single_event(c, ON_START, effect(id), &sc.state, on(active), source, source_effect).truthy()) {
    if (SideCondition* added = slot_condition_of(s, id)) added->id = Condition::None;
    return false;
  }
  return true;
}
inline bool remove_slot_condition(Ctx& c, int side, Condition id) {
  Side& s = c.b.sides[side];
  SideCondition* sc = slot_condition_of(s, id);
  if (!sc) return false;
  single_event(c, ON_VOLATILE_END, effect(id), &sc->state, on(Ref{uint8_t(side), s.active}));
  if (SideCondition* still = slot_condition_of(s, id)) still->id = Condition::None;
  return true;
}

// Side.removeSideCondition.
inline bool remove_side_condition(Ctx& c, int side, Condition id) {
  Side& s = c.b.sides[side];
  SideCondition* sc = side_condition_of(s, id);
  if (!sc) return false;
  single_event(c, callback("", "SideEnd"), effect(id), &sc->state, on_side(side));
  if (SideCondition* still = side_condition_of(s, id)) still->id = Condition::None;
  return true;
}

// ---------------------------------------------------------------------------
// Statuses (pokemon.ts).
// ---------------------------------------------------------------------------
inline constexpr EventNames SET_STATUS = event("SetStatus");
inline constexpr EventNames AFTER_SET_STATUS = event("AfterSetStatus");

inline void attr_last_move(Ctx& c, std::string_view attribute); // moves.hpp

inline Immunity status_immunity(Condition status) {
  switch (status) {
  case Condition::Brn: return Immunity::Brn;
  case Condition::Frz: return Immunity::Frz;
  case Condition::Psn: return Immunity::Psn;
  case Condition::Tox: return Immunity::Tox;
  default: return Immunity(0xFF);
  }
}

// Pokemon.setStatus(status, source, sourceEffect). `status` None clears it.
// With no source it is the event's or the Pokémon itself; with no source
// effect, the current effect.
inline bool set_status(Ctx& c, Ref r, Condition status, Ref source = {}, Effect source_effect = {}) {
  Pokemon& p = c.b.at(r);
  if (!p.hp) return false;
  if (!p.is_active && status != Condition::None) return false;
  if (!source) source = c.event.source;
  if (!source_effect) source_effect = c.effect;
  if (!source) source = r;
  const bool from_status_move = source_effect.kind == EffectKind::Move && source_effect &&
                                data(Move(source_effect.id)).status != Condition::None;
  if (p.status == status && !p.status_fnt) {
    if (status == Condition::None) return false;
    if (from_status_move && data(Move(source_effect.id)).status == p.status) {
      add(c, {part::text("-fail"), part::pokemon(c.b, r), part::text(CONDITION_NAMES[uint8_t(p.status)].id)});
    } else if (from_status_move) {
      add(c, {part::text("-fail"), part::pokemon(c.b, source)});
      attr_last_move(c, "[still]");
    }
    return false;
  }
  if (status != Condition::None) {
    // Toxic poison is immune to what poison is.
    const Condition as = status == Condition::Tox ? Condition::Psn : status;
    const Immunity to = status_immunity(as);
    const bool natural = to == Immunity(0xFF) || get_immunity(p, to);
    if (!run_status_immunity(c, r, CONDITION_NAMES[uint8_t(as)].id, natural)) {
      if (from_status_move) add(c, {part::text("-immune"), part::pokemon(c.b, r)});
      return false;
    }
    if (!run_event(c, SET_STATUS, on(r), source, source_effect, relay::text(CONDITION_NAMES[uint8_t(status)].id)).truthy()) {
      return false;
    }
  }
  const Condition previous = p.status;
  const EffectState previous_state = p.status_state;
  p.status = status;
  p.status_fnt = false;
  p.status_state = status == Condition::None ? EffectState{} : init_effect_state(c.b, true, r);
  p.status_state.source = source;
  if (status == Condition::None) return true;
  if (!single_event(c, ON_START, effect(status), &p.status_state, on(r), source, source_effect).truthy()) {
    p.status = previous;
    p.status_state = previous_state;
    return false;
  }
  return run_event(c, AFTER_SET_STATUS, on(r), source, source_effect, relay::text(CONDITION_NAMES[uint8_t(status)].id)).truthy();
}
// Pokemon.trySetStatus: the status it has, if it has one, is "set" again.
inline bool try_set_status(Ctx& c, Ref r, Condition status, Ref source = {}, Effect source_effect = {}) {
  const Pokemon& p = c.b.at(r);
  return set_status(c, r, p.status != Condition::None ? p.status : status, source, source_effect);
}
// Pokemon.cureStatus and clearStatus. Nightmare, which sleep carries, is not
// ported.
inline bool cure_status(Ctx& c, Ref r, bool silent = false) {
  Pokemon& p = c.b.at(r);
  if (!p.hp || p.status == Condition::None) return false;
  add(c, {part::text("-curestatus"), part::pokemon(c.b, r), part::text(CONDITION_NAMES[uint8_t(p.status)].id),
          part::text(silent ? "[silent]" : "[msg]")});
  set_status(c, r, Condition::None);
  return true;
}
inline bool clear_status(Ctx& c, Ref r) {
  Pokemon& p = c.b.at(r);
  if (!p.hp || p.status == Condition::None) return false;
  set_status(c, r, Condition::None);
  return true;
}

} // namespace adv
