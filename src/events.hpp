// Showdown's event system (battle.ts): gathering the handlers of an event,
// ranking and sorting them, and calling them. The order handlers are
// gathered in, the sort and its shuffle of ties are all part of what a
// battle's draws depend on, so each function here follows the one it ports.
//
// Not yet: volatiles, side conditions and slot conditions are not in the
// state, so nothing is gathered from them. Speed is not what this is
// written for; the listener masks of docs/PLAN.md come later.

#pragma once

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>

#include "out.hpp"

namespace adv {

// A handler was reached that has not been ported. Until the checklist is
// done this is how a replay stops, naming what it needs.
struct Unported {
  std::string what;
};
[[noreturn]] inline void unported(std::string what) { throw Unported{std::move(what)}; }

// What handlers return and events pass along (Showdown's relayVar): nothing,
// a boolean or a number.
struct Relay {
  enum class Kind : uint8_t { Undefined, Null, False, True, Number } kind = Kind::Undefined;
  double number = 0;
  constexpr bool undefined() const { return kind == Kind::Undefined; }
  constexpr bool truthy() const { return kind == Kind::True || (kind == Kind::Number && number != 0); }
  constexpr bool operator==(const Relay&) const = default;
};
namespace relay {
inline constexpr Relay undefined{};
inline constexpr Relay yes{Relay::Kind::True, 0};
inline constexpr Relay no{Relay::Kind::False, 0};
inline constexpr Relay null{Relay::Kind::Null, 0};
constexpr Relay number(double v) { return {Relay::Kind::Number, v}; }
} // namespace relay

// What an event is run on, and what holds a handler's effect.
struct Subject {
  HolderKind kind = HolderKind::None;
  Ref pokemon;      // kind Pokemon
  uint8_t side = 0; // kind Side
  constexpr bool operator==(const Subject&) const = default;
};
constexpr Subject on(Ref r) { return {HolderKind::Pokemon, r, r.side}; }
inline constexpr Subject FIELD{HolderKind::Field, {}, 0};
inline constexpr Subject BATTLE{HolderKind::Battle, {}, 0};

// The engine's working state while it runs: Showdown's `this.effect`,
// `this.effectState`, `this.event` and `this.eventDepth`.
struct Ctx {
  Ctx(Battle& battle, Out& output) : b(battle), out(output) {}
  Battle& b;
  Out& out;
  Effect effect;
  EffectState* effect_state = nullptr;
  struct {
    std::string_view id;
    Subject target;
    Ref source;
    Effect effect;
    double modifier = 1;
  } event;
  int event_depth = 0;
};

// A handler: (relay, target, source, sourceEffect), with the handler's own
// effect and state in ctx.effect and ctx.effect_state. A handler declared
// without a relay argument ignores the one it is given.
using Fn = Relay (*)(Ctx&, Relay, Subject, Ref source, Effect source_effect);

struct Ported {
  Effect effect;
  Callback callback;
  Fn fn;
};
// Every handler ported so far (callbacks.hpp).
std::span<const Ported> ported();

inline constexpr Callback NO_CALLBACK = Callback(0xFF);

// The callback named "on" + prefix + event, if Gen 3 has that name.
constexpr Callback callback(std::string_view prefix, std::string_view event) {
  for (int i = 0; i < CALLBACKS; i++) {
    const std::string_view name = CALLBACK_NAMES[i];
    if (name.size() == 2 + prefix.size() + event.size() && name.starts_with("on") &&
        name.substr(2, prefix.size()) == prefix && name.substr(2 + prefix.size()) == event) {
      return Callback(i);
    }
  }
  return NO_CALLBACK;
}

// The names an event is looked for under.
struct EventNames {
  std::string_view id;
  Callback on, ally, foe, any, source, side, field;
};
constexpr EventNames event(std::string_view id) {
  return {id, callback("", id), callback("Ally", id), callback("Foe", id), callback("Any", id),
          callback("Source", id), callback("Side", id), callback("Field", id)};
}

namespace detail {
struct Entry {
  const Handler* row = nullptr; // the ranks, and whether Showdown has a handler
  Fn fn = nullptr;              // the port of it, if there is one yet
};
constexpr uint32_t key(Effect e, Callback c) { return uint32_t(e.kind) << 24 | uint32_t(e.id) << 8 | uint32_t(c); }
inline const std::unordered_map<uint32_t, Entry>& entries() {
  static const std::unordered_map<uint32_t, Entry> map = [] {
    std::unordered_map<uint32_t, Entry> m;
    for (const Handler& h : HANDLERS) m[key({h.kind, h.id}, h.callback)].row = &h;
    for (const Ported& p : ported()) m[key(p.effect, p.callback)].fn = p.fn;
    return m;
  }();
  return map;
}
inline const Entry* entry(Effect e, Callback c) {
  if (!e || c == NO_CALLBACK) return nullptr;
  const auto it = entries().find(key(e, c));
  return it == entries().end() ? nullptr : &it->second;
}
} // namespace detail

// A handler found for a name: which of the effect's handlers answers.
struct Found {
  Effect effect;
  Callback actual = NO_CALLBACK; // the handler that runs; NO_CALLBACK: none
  constexpr explicit operator bool() const { return actual != NO_CALLBACK; }
};

inline bool has(Effect e, Callback c) {
  const detail::Entry* entry = detail::entry(e, c);
  return entry && entry->row && entry->row->handled;
}

inline constexpr Callback ON_SWITCH_IN = callback("", "SwitchIn");
inline constexpr Callback ON_ANY_SWITCH_IN = callback("Any", "SwitchIn");
inline constexpr Callback ON_START = callback("", "Start");
inline constexpr Callback ON_AFTER_ENTRY_HAZARD = callback("", "AfterEntryHazard");

// Battle.getCallback (battle.ts:1025). For a Pokémon's `onSwitchIn` an
// ability's or an item's `onStart` stands in, and in Gen 3 an ability with
// `onAfterEntryHazard` answers with that at turn 0 and with nothing after.
inline Found get_callback(const Battle& b, bool on_pokemon, Effect e, Callback name) {
  Found found{e, has(e, name) ? name : NO_CALLBACK};
  if (on_pokemon && name == ON_SWITCH_IN && (e.kind == EffectKind::Ability || e.kind == EffectKind::Item)) {
    if (!found && !has(e, ON_ANY_SWITCH_IN) && has(e, ON_START)) found.actual = ON_START;
    if (e.kind == EffectKind::Ability && has(e, ON_AFTER_ENTRY_HAZARD)) {
      found.actual = b.turn == 0 ? ON_AFTER_ENTRY_HAZARD : NO_CALLBACK;
    }
  }
  return found;
}

// Calls a found handler.
inline Relay call(Ctx& c, Found f, Relay value, Subject t, Ref source, Effect source_effect) {
  const detail::Entry* entry = detail::entry(f.effect, f.actual);
  if (!entry || !entry->fn) {
    static constexpr const Name* NAMES[] = {MOVE_NAMES, ABILITY_NAMES, ITEM_NAMES, CONDITION_NAMES, RULE_NAMES};
    static constexpr const char* KIND[] = {"move", "ability", "item", "condition", "rule"};
    unported(std::string(KIND[uint8_t(f.effect.kind)]) + " " +
             std::string(NAMES[uint8_t(f.effect.kind)][f.effect.id].id) + " " +
             std::string(CALLBACK_NAMES[uint8_t(f.actual)]));
  }
  return entry->fn(c, value, t, source, source_effect);
}

// Showdown's effectType, as far as events tell them apart.
enum class EffectType : uint8_t { Move, Ability, Item, Condition, Status, Weather, Rule };
constexpr EffectType type_of(Effect e) {
  switch (e.kind) {
  case EffectKind::Move: return EffectType::Move;
  case EffectKind::Ability: return EffectType::Ability;
  case EffectKind::Item: return EffectType::Item;
  case EffectKind::Rule: return EffectType::Rule;
  case EffectKind::Condition: break;
  }
  switch (CONDITION_DATA[e.id].kind) {
  case ConditionKind::Status: return EffectType::Status;
  case ConditionKind::Weather: return EffectType::Weather;
  case ConditionKind::Condition: break;
  }
  return EffectType::Condition;
}

// A gathered handler (Showdown's EventListener).
struct Listener {
  Found callback;
  EffectState* state = nullptr;
  Subject holder;
  uint32_t order = 0; // 0 sorts last
  int32_t priority = 0;
  double speed = 0;
  int32_t sub_order = 0;
  uint32_t effect_order = 0;
};
struct Listeners {
  std::array<Listener, 24> list;
  int n = 0;
  void push(const Listener& l) {
    if (n == int(list.size())) unported("more than 24 handlers of one event");
    list[n++] = l;
  }
};

// Battle.comparePriority (battle.ts:409): less than 0 when a goes first.
template <typename A, typename B> constexpr double compare_priority(const A& a, const B& b) {
  const double order = double(a.order ? a.order : 4294967296.0) - double(b.order ? b.order : 4294967296.0);
  if (order != 0) return order;
  if (b.priority != a.priority) return double(b.priority) - double(a.priority);
  if (b.speed != a.speed) return b.speed - a.speed;
  if constexpr (requires { a.sub_order; }) {
    if (a.sub_order != b.sub_order) return double(a.sub_order) - double(b.sub_order);
    if (a.effect_order != b.effect_order) return double(a.effect_order) - double(b.effect_order);
  }
  return 0;
}

// Battle.speedSort (battle.ts:434): a selection sort that shuffles each run
// of ties with the battle's PRNG.
template <typename T, typename Compare> void speed_sort(Ctx& c, std::span<T> list, Compare compare) {
  if (list.size() < 2) return;
  size_t sorted = 0;
  std::array<size_t, 32> next{};
  while (sorted + 1 < list.size()) {
    size_t ties = 1;
    next[0] = sorted;
    for (size_t i = sorted + 1; i < list.size(); i++) {
      const double delta = compare(list[next[0]], list[i]);
      if (delta < 0) continue;
      if (delta > 0) ties = 0;
      next[ties++] = i;
    }
    for (size_t i = 0; i < ties; i++) {
      if (next[i] != sorted + i) std::swap(list[sorted + i], list[next[i]]);
    }
    // PRNG.shuffle(list, sorted, sorted + ties).
    for (size_t start = sorted, end = sorted + ties; ties > 1 && start + 1 < end; start++) {
      const size_t at = random(c.b, c.out, uint32_t(start), uint32_t(end));
      if (at != start) std::swap(list[start], list[at]);
    }
    sorted += ties;
  }
}
inline void speed_sort(Ctx& c, Listeners& l) {
  speed_sort(c, std::span<Listener>(l.list.data(), size_t(l.n)),
             [](const Listener& a, const Listener& b) { return compare_priority(a, b); });
}

// Battle.resolvePriority (battle.ts:957): a handler's ranks. The ranks are
// the effect's under the name the handler was looked for, whichever of its
// handlers answers.
inline Listener resolve_priority(Ctx& c, Listener l, Callback name) {
  if (const detail::Entry* e = detail::entry(l.callback.effect, name); e && e->row) {
    l.order = uint32_t(e->row->order);
    l.priority = e->row->priority;
    l.sub_order = e->row->sub_order;
  }
  if (!l.sub_order) {
    switch (type_of(l.callback.effect)) {
    case EffectType::Condition:
      l.sub_order = 2;
      if (l.state && l.state->target == HolderKind::Side) l.sub_order = 4; // a slot condition would be 3
      if (l.state && l.state->target == HolderKind::Field) l.sub_order = 5;
      break;
    case EffectType::Weather:
    case EffectType::Rule: l.sub_order = 5; break;
    case EffectType::Ability: l.sub_order = 7; break;
    case EffectType::Item: l.sub_order = 8; break;
    case EffectType::Move:
    case EffectType::Status: break;
    }
  }
  const std::string_view text = CALLBACK_NAMES[uint8_t(name)];
  const bool switch_in = text.ends_with("SwitchIn");
  if ((switch_in || text.ends_with("RedirectTarget")) && l.state) l.effect_order = l.state->effect_order;
  if (l.holder.kind == HolderKind::Pokemon) {
    const Pokemon& p = c.b.at(l.holder.pokemon);
    l.speed = p.speed;
    if (switch_in) {
      // Speeds were settled before the handlers: a Pokémon's place in that
      // order lowers its handlers' speed by a fraction.
      int place = -1;
      for (int i = 0; i < c.b.speed_orders; i++) {
        if (c.b.speed_order[i] == l.holder.pokemon.side) place = i;
      }
      l.speed -= double(place) / 2;
    }
  }
  return l;
}

// Battle.findPokemonEventHandlers (battle.ts:1111): the status, the
// volatiles in the order they were added, the ability, the item, the
// species, the slot conditions.
inline void find_pokemon_event_handlers(Ctx& c, Ref r, Callback name, Listeners& out, Subject holder = {}) {
  if (name == NO_CALLBACK) return;
  Pokemon& p = c.b.at(r);
  if (holder.kind == HolderKind::None) holder = on(r);
  const auto add = [&](Effect e, EffectState* state) {
    if (const Found f = get_callback(c.b, true, e, name)) out.push(resolve_priority(c, {f, state, holder}, name));
  };
  add(effect(p.status), &p.status_state);
  add(effect(p.ability), &p.ability_state);
  add(effect(p.item), &p.item_state);
}

// Battle.findFieldEventHandlers (battle.ts:1195): the pseudo-weathers, the weather.
inline void find_field_event_handlers(Ctx& c, Callback name, Listeners& out, Subject holder = FIELD) {
  if (name == NO_CALLBACK) return;
  for (int i = 0; i < c.b.field.pseudo_weathers; i++) {
    PseudoWeather& w = c.b.field.pseudo_weather[i];
    if (const Found f = get_callback(c.b, false, w.effect, name)) out.push(resolve_priority(c, {f, &w.state, holder}, name));
  }
  if (const Found f = get_callback(c.b, false, effect(c.b.field.weather), name)) {
    out.push(resolve_priority(c, {f, &c.b.field.weather_state, holder}, name));
  }
}

// Battle.findBattleEventHandlers (battle.ts:1172): the format.
inline void find_battle_event_handlers(Ctx& c, Callback name, Listeners& out, Subject holder = BATTLE) {
  if (const Found f = get_callback(c.b, false, effect(Rule::FormatOU), name)) {
    out.push(resolve_priority(c, {f, nullptr, holder}, name));
  }
}

// Battle.findEventHandlers (battle.ts:1049) for a Pokémon in singles, or for
// the battle. BeforeTurn, Update and Weather are not looked for under the
// prefixed names.
inline void find_event_handlers(Ctx& c, Subject t, const EventNames& e, Ref source, Listeners& out) {
  const bool prefixed = e.id != "BeforeTurn" && e.id != "Update" && e.id != "Weather" && e.id != "WeatherChange";
  if (t.kind == HolderKind::Pokemon && (c.b.at(t.pokemon).is_active || (source && c.b.at(source).is_active))) {
    const Ref self = t.pokemon;
    find_pokemon_event_handlers(c, self, e.on, out);
    if (prefixed) {
      // alliesAndSelf(), then foes(): the active Pokémon of each side that
      // have HP.
      if (c.b.at(self).is_active && c.b.at(self).hp) {
        find_pokemon_event_handlers(c, self, e.ally, out);
        find_pokemon_event_handlers(c, self, e.any, out);
      }
      const Side& other = c.b.sides[self.side ^ 1];
      if (other.active != 0xFF && other.pokemon[other.active].hp) {
        const Ref foe{uint8_t(self.side ^ 1), other.active};
        find_pokemon_event_handlers(c, foe, e.foe, out);
        find_pokemon_event_handlers(c, foe, e.any, out);
      }
    }
    // The sides' conditions would be gathered here: on<Event> of its own
    // side's, onFoe<Event> and onAny<Event> of the other's.
  } else if (t.kind != HolderKind::Battle && t.kind != HolderKind::Pokemon) {
    unported("an event on a side or the field");
  }
  if (source && prefixed) find_pokemon_event_handlers(c, source, e.source, out);
  find_field_event_handlers(c, e.on, out);
  find_battle_event_handlers(c, e.on, out);
}

// Battle.modify (battle.ts:2348) with a modifier in 4096ths, as runEvent
// applies an event's modifier to a whole number.
constexpr double modify(double value, double modifier) {
  const uint32_t m = uint32_t(modifier * 4096);
  return double(uint32_t((double(uint32_t(value * m)) + 2048 - 1) / 4096));
}

// Battle.chainModify (battle.ts:2337).
inline void chain_modify(Ctx& c, double numerator, double denominator = 1) {
  const uint32_t previous = uint32_t(c.event.modifier * 4096);
  const uint32_t next = uint32_t(numerator * 4096 / denominator);
  c.event.modifier = double((uint64_t(previous) * next + 2048) >> 12) / 4096;
}

// Battle.singleEvent (battle.ts:578): one effect's handler for a name.
inline Relay single_event(Ctx& c, Callback name, Effect e, EffectState* state, Subject t, Ref source = {},
                          Effect source_effect = {}, Relay value = relay::undefined, Found custom = {}) {
  if (c.event_depth >= 8) unported("an event stack eight deep");
  const bool has_relay = !value.undefined();
  if (!has_relay) value = relay::yes;
  if (type_of(e) == EffectType::Status && t.kind == HolderKind::Pokemon &&
      c.b.at(t.pokemon).status != Condition(e.id)) {
    return value;
  }
  const Found f = custom ? custom : Found{e, has(e, name) ? name : NO_CALLBACK};
  if (!f) return value;

  const auto parent_effect = c.effect;
  const auto parent_state = c.effect_state;
  const auto parent_event = c.event;
  EffectState fresh;
  c.effect = e;
  c.effect_state = state ? state : &fresh;
  c.event = {CALLBACK_NAMES[uint8_t(name)].substr(2), t, source, source_effect, 1};
  c.event_depth++;
  const Relay returned = call(c, f, has_relay ? value : relay::undefined, t, source, source_effect);
  c.event_depth--;
  c.effect = parent_effect;
  c.effect_state = parent_state;
  c.event = parent_event;
  return returned.undefined() ? value : returned;
}

// Battle.runEvent (battle.ts:765) for one target. A falsy return ends the
// event; with fast_exit (priorityEvent) any return does.
inline Relay run_event(Ctx& c, const EventNames& e, Subject t, Ref source = {}, Effect source_effect = {},
                       Relay value = relay::undefined, bool on_effect = false, bool fast_exit = false) {
  if (c.event_depth >= 8) unported("an event stack eight deep");
  Listeners handlers;
  if (on_effect && has(source_effect, e.on)) {
    handlers.push(resolve_priority(c, {{source_effect, e.on}, nullptr, t}, e.on));
  }
  find_event_handlers(c, t, e, source, handlers);

  if (e.id == "Invulnerability" || e.id == "TryHit" || e.id == "DamagingHit" || e.id == "EntryHazard" || fast_exit) {
    if (handlers.n > 1) unported("an event sorted without a shuffle: " + std::string(e.id));
  } else {
    speed_sort(c, handlers);
  }

  const bool has_relay = !value.undefined() && value.kind != Relay::Kind::Null;
  if (!has_relay) value = relay::yes;
  const auto parent_event = c.event;
  c.event = {e.id, t, source, source_effect, 1};
  c.event_depth++;
  for (int i = 0; i < handlers.n; i++) {
    const Listener& h = handlers.list[i];
    const Effect he = h.callback.effect;
    if (type_of(he) == EffectType::Status && h.holder.kind == HolderKind::Pokemon &&
        c.b.at(h.holder.pokemon).status != Condition(he.id)) {
      continue;
    }
    // Showdown skips a weather's handlers under Air Lock here; nothing sets
    // a weather yet.
    const auto parent_effect = c.effect;
    const auto parent_state = c.effect_state;
    EffectState fresh;
    c.effect = he;
    c.effect_state = h.state ? h.state : &fresh;
    c.effect_state->target = h.holder.kind;
    const Relay returned = call(c, h.callback, has_relay ? value : relay::undefined, t, source, source_effect);
    c.effect = parent_effect;
    c.effect_state = parent_state;
    if (!returned.undefined()) {
      value = returned;
      if (!value.truthy() || fast_exit) break;
    }
  }
  c.event_depth--;
  if (value.kind == Relay::Kind::Number && value.number >= 0 && value.number == double(uint64_t(value.number))) {
    value.number = modify(value.number, c.event.modifier);
  }
  c.event = parent_event;
  return value;
}

} // namespace adv
