// Showdown's event system (battle.ts): gathering the handlers of an event,
// ranking and sorting them, and calling them. The order handlers are
// gathered in, the sort and its shuffle of ties are all part of what a
// battle's draws depend on, so each function here follows the one it ports.
//
// Not yet: slot conditions are not in the state, so nothing is gathered
// from them. Speed is not what this is
// written for; the listener masks of docs/PLAN.md come later.

#pragma once

#include <array>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

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
// A boosts table with only some stats present (Showdown's SparseBoostsTable).
// The stats are read in Boosts' order, which is the order every Gen 3 move
// lists them in (tools/gen_tables.cjs checks that).
inline constexpr int BOOST_STATS = 7;
inline constexpr std::string_view BOOST_NAMES[BOOST_STATS] = {"atk", "def", "spa", "spd", "spe", "accuracy", "evasion"};
struct SparseBoosts {
  std::array<int8_t, BOOST_STATS> value{};
  uint8_t present = 0;
  // A stat that comes before the others (Curse lists Speed first); -1: none.
  int8_t first = -1;
  // The stat in place i of the table's order.
  constexpr int at(int i) const {
    if (first < 0) return i;
    return i == 0 ? first : i <= first ? i - 1 : i;
  }
  constexpr bool has(int stat) const { return present >> stat & 1; }
  constexpr void set(int stat, int v) { value[size_t(stat)] = int8_t(v), present |= uint8_t(1 << stat); }
  constexpr void remove(int stat) { value[size_t(stat)] = 0, present &= uint8_t(~(1 << stat)); }
  constexpr bool operator==(const SparseBoosts&) const = default;
};

struct Relay {
  enum class Kind : uint8_t { Undefined, Null, False, True, Number, Text, Boosts } kind = Kind::Undefined;
  double number = 0;
  std::string_view text; // a status or weather id, as runStatusImmunity passes
  SparseBoosts boosts;
  constexpr bool undefined() const { return kind == Kind::Undefined; }
  constexpr bool truthy() const {
    return kind == Kind::True || kind == Kind::Boosts || (kind == Kind::Number && number != 0) ||
           (kind == Kind::Text && !text.empty());
  }
  constexpr bool operator==(const Relay&) const = default;
};
namespace relay {
inline constexpr Relay undefined{};
inline constexpr Relay yes{.kind = Relay::Kind::True};
inline constexpr Relay no{.kind = Relay::Kind::False};
inline constexpr Relay null{.kind = Relay::Kind::Null};
constexpr Relay number(double v) { return {.kind = Relay::Kind::Number, .number = v}; }
constexpr Relay text(std::string_view t) { return {.kind = Relay::Kind::Text, .text = t}; }
constexpr Relay boosts(const SparseBoosts& b) { return {.kind = Relay::Kind::Boosts, .boosts = b}; }
} // namespace relay

// What an event is run on, and what holds a handler's effect.
struct Subject {
  HolderKind kind = HolderKind::None;
  Ref pokemon;      // kind Pokemon
  uint8_t side = 0; // kind Side
  constexpr bool operator==(const Subject&) const = default;
};
constexpr Subject on(Ref r) { return {HolderKind::Pokemon, r, r.side}; }
constexpr Subject on_side(int side) { return {HolderKind::Side, {}, uint8_t(side)}; }
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

// The callback named "on" + prefix + event, if Gen 3 has that name. Looked
// up by the compiler: a name is never matched while a battle runs.
consteval Callback callback(std::string_view prefix, std::string_view event) {
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
  // What runEvent asks of the event's name, answered once.
  bool prefixed = true;        // looked for under the Ally, Foe, Any and Source names too
  bool left_to_right = false;  // sorted by order and priority alone, with no shuffle
  bool weather = false;        // the Weather event itself
  bool residual_or_end = false;
  bool always_look = false;    // SwitchIn: an ability or item answers it under another name
  bool only_on = false;        // looked for under its plain name alone
  // A bit for each of the names it is looked for under.
  std::array<uint64_t, 2> names{};
};
consteval EventNames event(std::string_view id) {
  EventNames e{id, callback("", id), callback("Ally", id), callback("Foe", id), callback("Any", id),
               callback("Source", id), callback("Side", id), callback("Field", id)};
  e.prefixed = id != "BeforeTurn" && id != "Update" && id != "Weather" && id != "WeatherChange";
  e.left_to_right = id == "Invulnerability" || id == "TryHit" || id == "DamagingHit" || id == "EntryHazard";
  e.weather = id == "Weather";
  e.residual_or_end = id == "Residual" || id == "End";
  e.always_look = id == "SwitchIn";
  const auto none = [](Callback name) { return uint8_t(name) == 0xFF; };
  e.only_on = !e.prefixed || (none(e.ally) && none(e.foe) && none(e.any) && none(e.source));
  for (const Callback name : {e.on, e.ally, e.foe, e.any, e.source}) {
    if (e.only_on && name != e.on) continue;
    if (!none(name)) e.names[uint8_t(name) >> 6] |= uint64_t{1} << (uint8_t(name) & 63);
  }
  return e;
}

namespace detail {
struct Entry {
  const Handler* row = nullptr; // the ranks, and whether Showdown has a handler
  Fn fn = nullptr;              // the port of it, if there is one yet
};
// Every effect's entries, found without hashing: for each effect a bit for
// each callback that has an entry (nearly every lookup ends there, with
// none), and where its few entries are.
struct Table {
  struct Of {
    std::array<uint64_t, 2> mask{};
    uint16_t first = 0, count = 0;
  };
  static constexpr int SIZES[] = {MOVES, ABILITYS, ITEMS, CONDITIONS, RULES, 4};
  static constexpr int BASE[] = {0, MOVES, MOVES + ABILITYS, MOVES + ABILITYS + ITEMS, MOVES + ABILITYS + ITEMS + CONDITIONS,
                                 MOVES + ABILITYS + ITEMS + CONDITIONS + RULES};
  static constexpr int EFFECTS = MOVES + ABILITYS + ITEMS + CONDITIONS + RULES + 4;
  std::array<Of, EFFECTS> of{};
  std::vector<Entry> entries;
  std::vector<Callback> names;
  static constexpr int index(Effect e) { return BASE[uint8_t(e.kind)] + e.id; }
};
inline const Table& table() {
  static const Table t = [] {
    // Gathered by effect first, then laid out with each effect's together.
    std::vector<std::vector<std::pair<Callback, Entry>>> by(Table::EFFECTS);
    const auto at = [&](Effect e, Callback c) -> Entry& {
      auto& list = by[size_t(Table::index(e))];
      for (auto& [name, entry] : list) {
        if (name == c) return entry;
      }
      list.push_back({c, Entry{}});
      return list.back().second;
    };
    for (const Handler& h : HANDLERS) at({h.kind, h.id}, h.callback).row = &h;
    for (const Ported& p : ported()) at(p.effect, p.callback).fn = p.fn;
    Table out;
    for (int i = 0; i < Table::EFFECTS; i++) {
      Table::Of& o = out.of[size_t(i)];
      o.first = uint16_t(out.entries.size());
      o.count = uint16_t(by[size_t(i)].size());
      for (const auto& [name, entry] : by[size_t(i)]) {
        o.mask[uint8_t(name) >> 6] |= uint64_t{1} << (uint8_t(name) & 63);
        out.entries.push_back(entry);
        out.names.push_back(name);
      }
    }
    return out;
  }();
  return t;
}
inline const Entry* entry(Effect e, Callback c) {
  if (!e || c == NO_CALLBACK) return nullptr;
  const Table& t = table();
  const Table::Of& o = t.of[size_t(Table::index(e))];
  if (!(o.mask[uint8_t(c) >> 6] >> (uint8_t(c) & 63) & 1)) return nullptr;
  for (int i = o.first;; i++) {
    if (t.names[size_t(i)] == c) return &t.entries[size_t(i)];
  }
}
} // namespace detail

// A set of callbacks, a bit each.
using Mask = std::array<uint64_t, 2>;
// For each effect, a bit for each callback Showdown has a handler under: the
// handler table, turned on its side by the compiler.
inline constexpr std::array<Mask, detail::Table::EFFECTS> MASKS = [] {
  std::array<Mask, detail::Table::EFFECTS> masks{};
  for (const Handler& h : HANDLERS) {
    if (h.handled) masks[size_t(detail::Table::index({h.kind, h.id}))][uint8_t(h.callback) >> 6] |= uint64_t{1} << (uint8_t(h.callback) & 63);
  }
  return masks;
}();
constexpr const Mask& mask_of(Effect e) { return MASKS[size_t(detail::Table::index(e))]; }

// A handler found for a name: which of the effect's handlers answers.
struct Found {
  Effect effect;
  Callback actual = NO_CALLBACK; // the handler that runs; NO_CALLBACK: none
  constexpr explicit operator bool() const { return actual != NO_CALLBACK; }
};

// Whether Showdown has a handler of the effect under the name.
constexpr bool has(Effect e, Callback c) {
  return c != NO_CALLBACK && (mask_of(e)[uint8_t(c) >> 6] >> (uint8_t(c) & 63) & 1);
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

// Field.suppressingWeather (field.ts:106): Cloud Nine or Air Lock is out.
inline bool suppressing_weather(const Battle& b) {
  for (const Side& s : b.sides) {
    if (s.active == 0xFF) continue;
    const Pokemon& p = s.pokemon[s.active];
    if (!p.fainted && data(p.ability).suppress_weather && !p.ability_state.ending) return true;
  }
  return false;
}

// Calls a found handler.
inline Relay call(Ctx& c, Found f, Relay value, Subject t, Ref source, Effect source_effect) {
  const detail::Entry* entry = detail::entry(f.effect, f.actual);
  if (!entry || !entry->fn) {
    if (f.effect.kind == EffectKind::Special) unported("a handler of a made-up effect");
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
  case EffectKind::Special: return EffectType::Condition;
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
  bool ends = false;      // Showdown's `end`: the effect can be ended when its duration runs out
  bool is_volatile = false;
  bool is_side_condition = false;
  bool is_slot_condition = false;
  uint8_t side = 0; // of a side or slot condition
};
// The handlers gathered for one event. Nearly every event gathers none, so
// the 24 places are left as they are until one is written.
struct Listeners {
  union {
    std::array<Listener, 24> list;
  };
  int n = 0;
  Listeners() {}
  void push(const Listener& l) {
    if (n == int(list.size())) unported("more than 24 handlers of one event");
    std::construct_at(&list[size_t(n++)], l);
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

// Which callback names end in SwitchIn or in RedirectTarget.
struct NameFlags {
  std::array<bool, CALLBACKS> switch_in{}, redirect{};
};
inline constexpr NameFlags NAME_FLAGS = [] {
  NameFlags f;
  for (int i = 0; i < CALLBACKS; i++) {
    f.switch_in[size_t(i)] = CALLBACK_NAMES[i].ends_with("SwitchIn");
    f.redirect[size_t(i)] = CALLBACK_NAMES[i].ends_with("RedirectTarget");
  }
  return f;
}();

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
  const bool switch_in = NAME_FLAGS.switch_in[uint8_t(name)];
  if ((switch_in || NAME_FLAGS.redirect[uint8_t(name)]) && l.state) l.effect_order = l.state->effect_order;
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
// With `by_duration` (fieldEvent's Residual) an effect that has a duration is
// gathered whether or not it has the handler.
inline void find_pokemon_event_handlers(Ctx& c, Ref r, Callback name, Listeners& out, Subject holder = {},
                                        bool by_duration = false) {
  if (name == NO_CALLBACK) return;
  Pokemon& p = c.b.at(r);
  if (holder.kind == HolderKind::None) holder = on(r);
  const auto add = [&](Effect e, EffectState* state, bool is_volatile) {
    if (!e) return; // none, or a volatile that was removed
    const Found f = get_callback(c.b, true, e, name);
    if (!f && !(by_duration && state->duration)) return;
    Listener l = resolve_priority(c, {{e, f.actual}, state, holder}, name);
    l.ends = true;
    l.is_volatile = is_volatile;
    out.push(l);
  };
  add(effect(p.status), &p.status_state, false);
  for (int i = 0; i < p.n_volatiles; i++) add(effect(p.volatiles[i].id), &p.volatiles[i].state, true);
  add(effect(p.ability), &p.ability_state, false);
  add(effect(p.item), &p.item_state, false);
  // The conditions of the Pokémon's slot, held by the Pokémon.
  Side& side = c.b.sides[r.side];
  for (int i = 0; p.position == 0 && i < side.n_slot_conditions; i++) {
    SideCondition& sc = side.slot_conditions[i];
    if (sc.id == Condition::None) continue;
    const Found f = get_callback(c.b, true, effect(sc.id), name);
    if (!f && !(by_duration && sc.state.duration)) continue;
    Listener l = resolve_priority(c, {{effect(sc.id), f.actual}, &sc.state, holder}, name);
    if (!l.sub_order || l.sub_order == 4) l.sub_order = 3;
    l.ends = true;
    l.is_slot_condition = true;
    l.side = r.side;
    out.push(l);
  }
}

// Battle.findSideEventHandlers (battle.ts:1230): the side's conditions, in
// the order they were added.
inline void find_side_event_handlers(Ctx& c, int side, Callback name, Listeners& out, Subject holder = {},
                                     bool by_duration = false) {
  if (name == NO_CALLBACK) return;
  const bool own = holder.kind == HolderKind::None;
  if (own) holder = on_side(side);
  Side& s = c.b.sides[side];
  for (int i = 0; i < s.n_side_conditions; i++) {
    SideCondition& sc = s.side_conditions[i];
    if (sc.id == Condition::None) continue; // removed
    const Found f = get_callback(c.b, false, effect(sc.id), name);
    if (!f && !(by_duration && sc.state.duration)) continue;
    Listener l = resolve_priority(c, {{effect(sc.id), f.actual}, &sc.state, holder}, name);
    l.ends = own;
    l.is_side_condition = true;
    l.side = uint8_t(side);
    out.push(l);
  }
}

// Battle.findFieldEventHandlers (battle.ts:1195): the pseudo-weathers, the weather.
inline void find_field_event_handlers(Ctx& c, Callback name, Listeners& out, Subject holder = FIELD,
                                      bool by_duration = false) {
  if (name == NO_CALLBACK) return;
  const auto add = [&](Effect e, EffectState* state) {
    if (!e) return;
    const Found f = get_callback(c.b, false, e, name);
    if (!f && !(by_duration && state->duration)) return;
    Listener l = resolve_priority(c, {{e, f.actual}, state, holder}, name);
    l.ends = holder.kind == HolderKind::Field;
    out.push(l);
  };
  for (int i = 0; i < c.b.field.pseudo_weathers; i++) add(c.b.field.pseudo_weather[i].effect, &c.b.field.pseudo_weather[i].state);
  add(effect(c.b.field.weather), &c.b.field.weather_state);
}

// Battle.findBattleEventHandlers (battle.ts:1172): the format.
inline void find_battle_event_handlers(Ctx& c, Callback name, Listeners& out, Subject holder = BATTLE) {
  if (const Found f = get_callback(c.b, false, effect(Rule::FormatOU), name)) {
    out.push(resolve_priority(c, {f, nullptr, holder}, name));
  }
}

// The callbacks that something a holder has could answer: the masks of its
// effects together. Nearly every event finds none, and these say so without
// looking at each effect. They are worked out when asked, from the state as
// it is, so nothing has to keep them up to date.
inline void merge(Mask& into, const Mask& m) { into[0] |= m[0], into[1] |= m[1]; }
// Whether to look under a name. onSwitchIn is always looked for: an ability
// or an item answers it with another of its handlers (get_callback).
inline bool listens(const Mask& m, Callback name) {
  return name != NO_CALLBACK && (name == ON_SWITCH_IN || (m[uint8_t(name) >> 6] >> (uint8_t(name) & 63) & 1));
}
// A Pokémon's own effects: kept on it (Pokemon::mask), and brought up to date
// by refresh_mask wherever an effect is given to it.
inline Mask own_mask(const Pokemon& p) {
  Mask m = mask_of(effect(p.status));
  for (int i = 0; i < p.n_volatiles; i++) merge(m, mask_of(effect(p.volatiles[i].id)));
  merge(m, mask_of(effect(p.ability)));
  merge(m, mask_of(effect(p.item)));
  return m;
}
inline void refresh_mask(Battle& b, Pokemon& p) {
  p.mask = own_mask(p);
  b.listening_stale = true;
}
// With ADV_CHECK_MASKS, every use of a kept mask first makes sure that it
// covers what the Pokémon has now.
inline void check_mask([[maybe_unused]] const Pokemon& p) {
#ifdef ADV_CHECK_MASKS
  if (const Mask now = own_mask(p); (now[0] & ~p.mask[0]) || (now[1] & ~p.mask[1])) unported("a Pokémon's mask left out of date");
#endif
}
inline Mask pokemon_mask(const Battle& b, Ref r) {
  const Pokemon& p = b.at(r);
  check_mask(p);
  Mask m = p.mask;
  const Side& side = b.sides[r.side];
  for (int i = 0; p.position == 0 && i < side.n_slot_conditions; i++) merge(m, mask_of(effect(side.slot_conditions[i].id)));
  return m;
}
inline Mask side_mask(const Battle& b, int side) {
  Mask m{};
  const Side& s = b.sides[side];
  for (int i = 0; i < s.n_side_conditions; i++) merge(m, mask_of(effect(s.side_conditions[i].id)));
  return m;
}
inline Mask field_mask(const Battle& b) {
  Mask m = mask_of(effect(b.field.weather));
  merge(m, b.field.pseudo_weather_mask);
  return m;
}

// What is not a Pokémon's own, for a side (Battle::listening_side), and
// everything in play together (Battle::listening).
inline Mask fresh_listening_side(const Battle& b, int s) {
  Mask m = field_mask(b);
  merge(m, mask_of(effect(Rule::FormatOU)));
  const Side& side = b.sides[s];
  for (int i = 0; i < side.n_slot_conditions; i++) merge(m, mask_of(effect(side.slot_conditions[i].id)));
  merge(m, side_mask(b, s));
  return m;
}
inline Mask fresh_listening(const Battle& b) {
  Mask m{};
  for (int s = 0; s < 2; s++) {
    const Side& side = b.sides[s];
    if (side.active != 0xFF) merge(m, side.pokemon[side.active].mask);
    merge(m, fresh_listening_side(b, s));
  }
  return m;
}
[[gnu::noinline]] inline void refresh_listening(Battle& b) {
  b.listening = fresh_listening(b);
  for (int s = 0; s < 2; s++) b.listening_side[size_t(s)] = fresh_listening_side(b, s);
  b.listening_stale = false;
}
inline const Mask& listening(Battle& b) {
  if (b.listening_stale) [[unlikely]] refresh_listening(b);
#ifdef ADV_CHECK_MASKS
  for (const Side& side : b.sides) {
    if (side.active != 0xFF) check_mask(side.pokemon[side.active]);
  }
  const auto covers = [](const Mask& kept, const Mask& now) { return !((now[0] & ~kept[0]) | (now[1] & ~kept[1])); };
  if (!covers(b.listening, fresh_listening(b)) || !covers(b.listening_side[0], fresh_listening_side(b, 0)) ||
      !covers(b.listening_side[1], fresh_listening_side(b, 1))) {
    unported("the battle's mask left out of date");
  }
#endif
  return b.listening;
}

// Battle.findEventHandlers (battle.ts:1049) for a Pokémon in singles, or for
// the battle. BeforeTurn, Update and Weather are not looked for under the
// prefixed names.
inline void find_event_handlers(Ctx& c, Subject t, const EventNames& e, Ref source, Listeners& out) {
  const bool prefixed = e.prefixed;
  int target_side = -1;
  // The source is nearly always the target or its foe, whose masks are made
  // anyway.
  Mask source_mask{};
  bool have_source_mask = false;
  if (t.kind == HolderKind::Pokemon && (c.b.at(t.pokemon).is_active || (source && c.b.at(source).is_active))) {
    const Ref self = t.pokemon;
    const Mask mine = pokemon_mask(c.b, self);
    if (source == self) source_mask = mine, have_source_mask = true;
    if (listens(mine, e.on)) find_pokemon_event_handlers(c, self, e.on, out);
    if (prefixed) {
      // alliesAndSelf(), then foes(): the active Pokémon of each side that
      // have HP.
      if (c.b.at(self).is_active && c.b.at(self).hp) {
        if (listens(mine, e.ally)) find_pokemon_event_handlers(c, self, e.ally, out);
        if (listens(mine, e.any)) find_pokemon_event_handlers(c, self, e.any, out);
      }
      const Side& other = c.b.sides[self.side ^ 1];
      if (other.active != 0xFF && other.pokemon[other.active].hp && (e.foe != NO_CALLBACK || e.any != NO_CALLBACK)) {
        const Ref foe{uint8_t(self.side ^ 1), other.active};
        const Mask theirs = pokemon_mask(c.b, foe);
        if (source == foe) source_mask = theirs, have_source_mask = true;
        if (listens(theirs, e.foe)) find_pokemon_event_handlers(c, foe, e.foe, out);
        if (listens(theirs, e.any)) find_pokemon_event_handlers(c, foe, e.any, out);
      }
    }
    target_side = self.side;
  } else if (t.kind != HolderKind::Battle && t.kind != HolderKind::Pokemon) {
    unported("an event on a side or the field");
  }
  if (source && prefixed && e.source != NO_CALLBACK) {
    if (!have_source_mask) source_mask = pokemon_mask(c.b, source);
    if (listens(source_mask, e.source)) find_pokemon_event_handlers(c, source, e.source, out);
  }
  // The sides' conditions, p1's then p2's: on<Event> of the target's own
  // side, onFoe<Event> of the other, and onAny<Event> of both.
  for (int side = 0; target_side >= 0 && side < 2; side++) {
    if (!c.b.sides[side].n_side_conditions) continue;
    const Mask conditions = side_mask(c.b, side);
    if (side == target_side) {
      if (listens(conditions, e.on)) find_side_event_handlers(c, side, e.on, out);
    } else if (prefixed && listens(conditions, e.foe)) {
      find_side_event_handlers(c, side, e.foe, out);
    }
    if (prefixed && listens(conditions, e.any)) find_side_event_handlers(c, side, e.any, out);
  }
  if (listens(field_mask(c.b), e.on)) find_field_event_handlers(c, e.on, out);
  if (listens(mask_of(effect(Rule::FormatOU)), e.on)) find_battle_event_handlers(c, e.on, out);
}

// Battle.modify (battle.ts:2348) with a modifier in 4096ths, as runEvent
// applies an event's modifier to a whole number.
// Showdown's `tr` is JavaScript's `>>> 0`, which keeps the low 32 bits.
constexpr double modify(double value, double modifier) {
  const uint32_t m = uint32_t(modifier * 4096);
  return double(uint32_t((double(uint32_t(uint64_t(value * m))) + 2048 - 1) / 4096));
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
  const bool has_relay = !value.undefined();
  if (!has_relay) value = relay::yes;
  // Most effects have no handler under the name, and the value comes back
  // as it is whichever of these stops the event; so that is asked first.
  const Found f = custom ? custom : Found{e, has(e, name) ? name : NO_CALLBACK};
  if (!f) return value;
  if (c.event_depth >= 8) unported("an event stack eight deep");
  if (type_of(e) == EffectType::Status && t.kind == HolderKind::Pokemon &&
      c.b.at(t.pokemon).status != Condition(e.id)) {
    return value;
  }
  const std::string_view id = CALLBACK_NAMES[uint8_t(name)].substr(2);
  if (type_of(e) == EffectType::Weather && id != "FieldStart" && id != "FieldResidual" && id != "FieldEnd" &&
      suppressing_weather(c.b)) {
    return value;
  }

  const auto parent_effect = c.effect;
  const auto parent_state = c.effect_state;
  const auto parent_event = c.event;
  EffectState fresh;
  c.effect = e;
  c.effect_state = state ? state : &fresh;
  c.event = {id, t, source, source_effect, 1};
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
// With no handler an event's value comes back as it was, passed through the
// modifier of an event that modified nothing.
inline Relay unheard(Relay value) {
  if (value.undefined() || value.kind == Relay::Kind::Null) return relay::yes;
  if (value.kind == Relay::Kind::Number && value.number >= 0 && value.number == double(uint64_t(value.number))) {
    value.number = modify(value.number, 1);
  }
  return value;
}
inline Relay run_event_heard(Ctx& c, const EventNames& e, Subject t, Ref source, Effect source_effect, Relay value,
                             bool on_effect, bool fast_exit);
// Nearly every event ends in this part: none of its names is among those
// that anything in play, the target or the source could answer. It is put
// in line wherever an event is run; the rest is not.
[[gnu::always_inline]] inline Relay run_event(Ctx& c, const EventNames& e, Subject t, Ref source = {}, Effect source_effect = {},
                       Relay value = relay::undefined, bool on_effect = false, bool fast_exit = false) {
  if (!e.always_look) {
    Mask m = listening(c.b);
    if (t.kind == HolderKind::Pokemon) check_mask(c.b.at(t.pokemon));
    if (source) check_mask(c.b.at(source));
    if (e.only_on && t.kind == HolderKind::Pokemon) {
      // Only the target's own effects, its side's and the field's are asked.
      m = c.b.listening_side[t.pokemon.side];
      merge(m, c.b.at(t.pokemon).mask);
    } else {
      if (t.kind == HolderKind::Pokemon) merge(m, c.b.at(t.pokemon).mask);
      if (source) merge(m, c.b.at(source).mask);
    }
    if (!((m[0] & e.names[0]) | (m[1] & e.names[1])) && !(on_effect && has(source_effect, e.on))) return unheard(value);
  }
  return run_event_heard(c, e, t, source, source_effect, value, on_effect, fast_exit);
}
[[gnu::noinline]] inline Relay run_event_heard(Ctx& c, const EventNames& e, Subject t, Ref source, Effect source_effect,
                                               Relay value, bool on_effect, bool fast_exit) {
  if (c.event_depth >= 8) unported("an event stack eight deep");
  Listeners handlers;
  if (on_effect && has(source_effect, e.on)) {
    handlers.push(resolve_priority(c, {{source_effect, e.on}, nullptr, t}, e.on));
  }
  find_event_handlers(c, t, e, source, handlers);

  // Four events are sorted left to right, and a priority event by its own
  // order; neither shuffles ties, and JavaScript's sort keeps their order.
  const auto stable_sort = [&](auto less) {
    for (int i = 1; i < handlers.n; i++) {
      for (int k = i; k > 0 && less(handlers.list[k], handlers.list[k - 1]); k--) {
        std::swap(handlers.list[k], handlers.list[k - 1]);
      }
    }
  };
  if (handlers.n < 2) {
    // Nothing to put in order, and so nothing drawn.
  } else if (e.left_to_right) {
    // Battle.compareLeftToRightOrder: order, then priority; one target, so
    // no index to tell them apart.
    stable_sort([](const Listener& a, const Listener& b) {
      const double oa = a.order ? a.order : 4294967296.0, ob = b.order ? b.order : 4294967296.0;
      return oa != ob ? oa < ob : a.priority > b.priority;
    });
  } else if (fast_exit) {
    // Battle.compareRedirectOrder: priority, speed, then the holders'
    // abilities in the order they came into effect.
    stable_sort([&](const Listener& a, const Listener& b) {
      if (a.priority != b.priority) return a.priority > b.priority;
      if (a.speed != b.speed) return a.speed > b.speed;
      if (a.holder.kind != HolderKind::Pokemon || b.holder.kind != HolderKind::Pokemon) return false;
      return c.b.at(a.holder.pokemon).ability_state.effect_order < c.b.at(b.holder.pokemon).ability_state.effect_order;
    });
  } else {
    speed_sort(c, handlers);
  }

  const bool has_relay = !value.undefined() && value.kind != Relay::Kind::Null;
  if (!has_relay) value = relay::yes;
  if (!handlers.n) return unheard(value);
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
    if ((type_of(he) == EffectType::Weather || e.weather) && !e.residual_or_end && suppressing_weather(c.b)) {
      continue;
    }
    const auto parent_effect = c.effect;
    const auto parent_state = c.effect_state;
    EffectState fresh;
    c.effect = he;
    c.effect_state = h.state ? h.state : &fresh;
    c.effect_state->target = h.holder.kind;
    c.effect_state->target_pokemon = h.holder.pokemon;
    if (h.holder.kind == HolderKind::Side) c.effect_state->target_side = h.holder.side;
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
