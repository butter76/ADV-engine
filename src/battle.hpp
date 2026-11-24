// The battle's state. Fields carry Showdown's names in snake_case
// (`baseStoredStats` is `base_stored_stats`), so that a function here reads
// against the Showdown function it is a port of. Citations are to the pinned
// commit: `battle.ts`, `pokemon.ts`, `side.ts` and so on are under sim/.
//
// Only what the ported code reads is here; the structs grow with the port.

#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "data.hpp"
#include "prng.hpp"

namespace adv {

inline constexpr int TEAM_SIZE = 6;
inline constexpr int MOVE_SLOTS = 4;
inline constexpr int MAX_LEVEL = 100; // Showdown allows 9999; HP then passes 65535

// A Pokémon as its team gives it (Showdown's PokemonSet), without the text:
// the nickname and the ball are the formatter's (showdown/team.hpp).
struct Set {
  Species species = Species::None;
  uint8_t level = 100;
  Gender gender = Gender::Either; // Either: the set names none
  uint8_t happiness = 255;
  Item item = Item::None;
  Ability ability = Ability::None;
  std::array<Move, MOVE_SLOTS> moves{};
  Nature nature = Nature::None; // None: neutral
  std::array<uint8_t, 6> evs{};
  std::array<uint8_t, 6> ivs{31, 31, 31, 31, 31, 31};
  Type hp_type = Type::None; // None: the set names none
  bool shiny = false;
};

struct Team {
  std::array<Set, TEAM_SIZE> sets{};
  uint8_t size = 0;
};

// Showdown's stats other than HP, in its order.
struct Stats {
  uint16_t atk = 0, def = 0, spa = 0, spd = 0, spe = 0;
};

// A Pokémon: its side and its index in the team as given. The index never
// changes; `position` is where Showdown's `side.pokemon` has it now.
struct Ref {
  uint8_t side = 0, index = 0xFF;
  constexpr explicit operator bool() const { return index != 0xFF; }
  constexpr bool operator==(const Ref&) const = default;
};

// Any effect: which table, and its id there. kind None is no effect.
struct Effect {
  EffectKind kind = EffectKind::Move;
  uint16_t id = 0; // 0: none
  constexpr explicit operator bool() const { return id != 0; }
  constexpr bool operator==(const Effect&) const = default;
};
constexpr Effect effect(Move m) { return {EffectKind::Move, uint16_t(m)}; }
constexpr Effect effect(Ability a) { return {EffectKind::Ability, uint16_t(a)}; }
constexpr Effect effect(Item i) { return {EffectKind::Item, uint16_t(i)}; }
constexpr Effect effect(Condition c) { return {EffectKind::Condition, uint16_t(c)}; }
constexpr Effect effect(Rule r) { return {EffectKind::Rule, uint16_t(r)}; }

// What an effect's state is the state of, once it has run a handler
// (runEvent sets `effectState.target` to the handler's holder).
enum class HolderKind : uint8_t { None, Pokemon, Side, Field, Battle };

// Showdown's EffectState, as far as the port reads it.
struct EffectState {
  uint32_t effect_order = 0;
  uint8_t duration = 0;
  HolderKind target = HolderKind::None;
};

enum class Disabled : uint8_t { No, Yes, Hidden };

struct MoveSlot {
  Move id = Move::None; // Hidden Power, never one of its typed moves
  uint8_t pp = 0, maxpp = 0;
  Disabled disabled = Disabled::No;
  bool used = false;
};

struct Pokemon {
  Set set;
  Species base_species = Species::None;
  Species species = Species::None;
  uint8_t level = 0;
  Gender gender = Gender::None; // Male, Female or None
  uint8_t happiness = 0;
  std::array<MoveSlot, MOVE_SLOTS> move_slots{};
  uint8_t moves = 0; // how many slots are filled
  Type hp_type = Type::None;
  uint8_t hp_power = 0;
  Stats base_stored_stats, stored_stats;
  Boosts boosts;
  uint16_t hp = 0, maxhp = 0, base_maxhp = 0;
  Ability base_ability = Ability::None, ability = Ability::None;
  Item item = Item::None;
  Condition status = Condition::None;
  EffectState ability_state, item_state, status_state;
  uint8_t position = 0;
  // Showdown's speed is a number: Gen 3 gives a Quick Claw holder 65535.
  uint32_t speed = 0;
  bool is_active = false, is_started = false, fainted = false;
  bool trapped = false, maybe_trapped = false, maybe_disabled = false, maybe_locked = false;
  bool known_type = true, newly_switched = false;
  bool switch_flag = false, force_switch_flag = false;
  uint8_t active_turns = 0, previously_switched_in = 0;
};

struct Side {
  std::array<Pokemon, TEAM_SIZE> pokemon{};
  uint8_t size = 0;
  uint8_t pokemon_left = 0;
  uint8_t active = 0xFF; // the team index of the Pokémon out, or none
};

// A rule or a condition on the field (Field.pseudoWeather), in the order
// they were added.
struct PseudoWeather {
  Effect effect;
  EffectState state;
};

struct Field {
  std::array<PseudoWeather, 8> pseudo_weather{};
  uint8_t pseudo_weathers = 0;
  Condition weather = Condition::None;
  EffectState weather_state;
};

enum class Choice : uint8_t { Start, BeforeTurn, RunSwitch, Switch, InstaSwitch, Move, Residual };

// An action of the queue (battle-queue.ts). A missing order sorts last, as
// Showdown's `order || 4294967296`.
struct Action {
  Choice choice = Choice::Start;
  uint32_t order = 0;
  int32_t priority = 0;
  double speed = 0;
  Ref pokemon;
};

struct Queue {
  std::array<Action, 16> list{};
  uint8_t size = 0;
};

enum class RequestState : uint8_t { None, Move, Switch };

// One listed move of a request (Pokemon.getMoves).
struct RequestMove {
  Move id = Move::None;
  uint8_t pp = 0, maxpp = 0;
  Target target = Target::Normal;
  bool disabled = false;
};

// What a side was asked (Battle.getRequests), kept because making it changes
// the state and the text is written later.
struct Request {
  enum class Kind : uint8_t { None, Move, Switch, Wait } kind = Kind::None;
  std::array<RequestMove, MOVE_SLOTS> moves{};
  uint8_t n_moves = 0;
  bool trapped = false, maybe_trapped = false, maybe_disabled = false, maybe_locked = false;
};

struct Battle {
  PRNG prng{0};
  std::array<Side, 2> sides{};
  Field field;
  Queue queue;
  std::array<Request, 2> requests{};
  uint16_t turn = 0;
  uint32_t effect_order = 0;
  RequestState request_state = RequestState::None;
  bool started = false, ended = false, mid_turn = false;
  bool report_percentages = false, support_cancel = false, quick_claw_roll = false;
  // Battle.speedOrder: the active Pokémon's field positions (0 for p1's, 1
  // for p2's) as the last runSwitch sorted them.
  std::array<uint8_t, 2> speed_order{};
  uint8_t speed_orders = 0;

  Pokemon& at(Ref r) { return sides[r.side].pokemon[r.index]; }
  const Pokemon& at(Ref r) const { return sides[r.side].pokemon[r.index]; }
};

// The team index of the Pokémon at a position of `side.pokemon`.
constexpr uint8_t index_at(const Side& s, int position) {
  for (uint8_t i = 0; i < s.size; i++) {
    if (s.pokemon[i].position == position) return i;
  }
  return 0xFF;
}

} // namespace adv
