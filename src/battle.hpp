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
constexpr Effect effect(Special s) { return {EffectKind::Special, uint16_t(s)}; }

// What an effect's state is the state of, once it has run a handler
// (runEvent sets `effectState.target` to the handler's holder).
enum class HolderKind : uint8_t { None, Pokemon, Side, Field, Battle };

// Showdown's EffectState, as far as the port reads it.
struct EffectState {
  uint32_t effect_order = 0;
  uint8_t duration = 0;
  HolderKind target = HolderKind::None;
  Ref target_pokemon; // the target, when it is a Pokémon
  Ref source;
  Effect source_effect;
  Move move = Move::None; // Choice lock: the move locked into
  int8_t time = 0;         // sleep, confusion: turns left
  int8_t skipped_time = 0; // sleep: turns slept through with a move usable asleep
  int8_t stage = 0;        // toxic poison: sixteenths of damage
  uint8_t layers = 0;      // Spikes
  uint16_t counter = 0;    // stall: one in this many that Protect works again
  uint16_t hp = 0;         // a Substitute's
  bool lost_focus = false; // Focus Punch: its user was hit
  bool has_slot = false;   // Counter, Mirror Coat: something hit that can be answered
  uint16_t damage = 0;     // and twice what it dealt
  bool ending = false;     // Cloud Nine, Air Lock: the ability is on its way out
  // A volatile tied to one on another Pokémon (Mean Look's `trapped` and its
  // user's `trapper`): when the other leaves, this one goes.
  Ref linked;
  Condition linked_status = Condition::None;
  Ref last_damage_source; // Bide: who last hurt its user
  uint8_t target_side = 0; // the target, when it is a side
};

// A volatile of a Pokémon (Pokemon.volatiles), kept in the order they were
// added, which is the order their handlers are gathered in.
struct Volatile {
  Condition id = Condition::None;
  EffectState state;
};

enum class Disabled : uint8_t { No, Yes, Hidden };
// Pokemon.trapped: false, true, or 'hidden' (a trap the player is not told of
// until a switch is refused).
enum class Trapped : uint8_t { No, Yes, Hidden };
// Pokemon.staleness, for the Endless Battle Clause.
enum class Staleness : uint8_t { None, Internal, External };

struct MoveSlot {
  Move id = Move::None; // Hidden Power, never one of its typed moves
  uint8_t pp = 0, maxpp = 0;
  Disabled disabled = Disabled::No;
  bool used = false;
  // A slot Mimic or Sketch made has no target written on it, and a request
  // lists it without one; Transform copies a slot's target as it finds it.
  bool no_target = false;
  // A Hidden Power slot that Transform made is named with its type.
  bool typed_name = false;
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
  // Pokemon.baseMoveSlots, as far as it differs: the slots that Mimic or
  // Transform put another move over (`virtual_slots`, a bit each) are kept
  // here as they were, and put back when the Pokémon leaves.
  std::array<MoveSlot, MOVE_SLOTS> base_move_slots{};
  uint8_t base_moves = 0, virtual_slots = 0;
  uint8_t pp_ups = 0; // Pokemon.ppUps: a bit for each move it came with that has PP Ups
  Type hp_type = Type::None, base_hp_type = Type::None;
  uint8_t hp_power = 0, base_hp_power = 0;
  Stats base_stored_stats, stored_stats;
  Boosts boosts;
  std::array<Type, 2> types{};
  uint16_t hp = 0, maxhp = 0, base_maxhp = 0;
  Ability base_ability = Ability::None, ability = Ability::None;
  Item item = Item::None;
  Condition status = Condition::None;
  EffectState ability_state, item_state, status_state;
  std::array<Volatile, 8> volatiles{};
  uint8_t n_volatiles = 0;
  uint8_t position = 0;
  // Showdown's speed is a number: Gen 3 gives a Quick Claw holder 65535.
  uint32_t speed = 0;
  bool is_active = false, is_started = false, fainted = false, faint_queued = false;
  bool being_called_back = false;
  // Showdown's status `fnt`, which a fainted Pokémon has while its side is
  // asked for a replacement.
  bool status_fnt = false;
  Trapped trapped = Trapped::No;
  bool maybe_trapped = false, maybe_disabled = false, maybe_locked = false;
  bool stats_raised_this_turn = false, stats_lowered_this_turn = false;
  bool known_type = true, newly_switched = false;
  bool switch_flag = false, force_switch_flag = false;
  // The move that flagged the switch (Showdown's switchFlag as a string):
  // Baton Pass.
  Move switch_flag_move = Move::None;
  bool skip_pursuit = false;
  bool pursuit_activated = false;
  bool truant_turn = false;
  bool item_knocked_off = false;
  bool used_item_this_turn = false;
  bool transformed = false;
  // The callbacks that its status, volatiles, ability or item could answer
  // (events.hpp), refreshed wherever one of them is given. It may say more
  // than is so, after one is taken away, and never less.
  std::array<uint64_t, 2> mask{};
  Item last_item = Item::None; // the item it last used up, for Recycle
  // Staleness: what it has, what eating a healing berry will give it, and
  // (until it leaves) whether Skill Swap was used on it by a foe.
  Staleness staleness = Staleness::None, pending_staleness = Staleness::None;
  bool volatile_staleness = false;
  // Pokemon.lastMoveUsed: the last move it began, called ones included, and
  // the type that move had in the end.
  Move last_move_used = Move::None;
  Type last_move_used_type = Type::None;
  // Pokemon.attackedBy. Showdown keeps every hit taken, and drops those of a
  // Pokémon that has left when the turn ends. Here: for each Pokémon of the
  // other side, by its place in its team, the last hit it gave (`order` 0:
  // none, else when among all hits taken), and whether any hit it gave this
  // turn did damage. A landing Future Sight is a hit from one that may have
  // left, which is why one record is not enough.
  struct AttackedBy {
    Move move = Move::None;
    uint16_t damage = 0;
    bool this_turn = false, damaged_this_turn = false;
    uint32_t order = 0;
  };
  std::array<AttackedBy, TEAM_SIZE> attacked_by{};
  uint32_t attacks = 0;
  uint8_t active_turns = 0, previously_switched_in = 0;
  Move last_move = Move::None, move_this_turn = Move::None;
  uint16_t last_damage = 0;
  uint8_t active_move_actions = 0;
};

// What a side has chosen so far (Side.choice, side.ts). In singles a side
// chooses one action.
struct ChoiceState {
  bool cant_undo = false, error = false;
  uint8_t forced_switches_left = 0, forced_passes_left = 0;
  enum class Kind : uint8_t { None, Pass, Switch, InstaSwitch, Move } action = Kind::None;
  uint8_t target = 0xFF; // the team index of the Pokémon to switch to
  Move move = Move::None;
};

// A condition of a side (Side.sideConditions), in the order they were added.
struct SideCondition {
  Condition id = Condition::None;
  EffectState state;
};
// Wish is a condition of the side's one slot (Side.slotConditions): it is
// gathered with the active Pokémon's handlers and outlasts a faint.

struct Side {
  std::array<Pokemon, TEAM_SIZE> pokemon{};
  std::array<SideCondition, 6> side_conditions{};
  uint8_t n_side_conditions = 0;
  std::array<SideCondition, 2> slot_conditions{};
  uint8_t n_slot_conditions = 0;
  uint8_t size = 0;
  uint8_t pokemon_left = 0;
  uint8_t active = 0xFF; // the team index of the Pokémon out, or none
  ChoiceState choice;
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
  // The callbacks the pseudo-weathers answer, together (events.hpp): they
  // are the format's rules, set once when the battle is made.
  std::array<uint64_t, 2> pseudo_weather_mask{};
  Condition weather = Condition::None;
  EffectState weather_state;
};

enum class Choice : uint8_t { Start, BeforeTurn, BeforeTurnMove, RunSwitch, Switch, InstaSwitch, Move, Residual };

// An action of the queue (battle-queue.ts). A missing order sorts last, as
// Showdown's `order || 4294967296`.
struct Action {
  Choice choice = Choice::Start;
  uint32_t order = 0;
  int32_t priority = 0;
  double speed = 0;
  Ref pokemon;
  Ref target; // a switch: the Pokémon coming in
  Move move = Move::None;
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
  bool no_target = false;
  bool disabled_source = false; // a refused choice wrote an empty disabledSource on it
};

// What a side was asked (Battle.getRequests), kept because making it changes
// the state and the text is written later.
struct Request {
  enum class Kind : uint8_t { None, Move, Switch, Wait } kind = Kind::None;
  std::array<RequestMove, MOVE_SLOTS> moves{};
  uint8_t n_moves = 0;
  bool trapped = false, maybe_trapped = false, maybe_disabled = false, maybe_locked = false;
  bool no_cancel = false;
  bool struggle = false; // no move can be used: the one listed is Struggle
  // The Pokémon is locked into the one move listed, which is named with
  // nothing else; Move::None there is the turn spent recharging.
  bool locked = false;
  bool update = false; // sent again after a refused choice changed it
};

struct Battle {
  PRNG prng{0};
  std::array<Side, 2> sides{};
  Field field;
  Queue queue;
  std::array<Request, 2> requests{};
  // The callbacks that anything now in play could answer (events.hpp): the
  // active Pokémon's masks, the sides' and the field's conditions and the
  // format's, together. An event none of whose names is here has no handler,
  // which is nearly every event. Made again when marked stale, as it is
  // wherever something is added to what it covers.
  std::array<uint64_t, 2> listening{};
  // And what is not a Pokémon's own, for each side: its conditions, the
  // field's and the format's. With the target's own mask this is all that
  // can answer an event looked for under its plain name alone.
  std::array<std::array<uint64_t, 2>, 2> listening_side{};
  bool listening_stale = true;
  std::array<Move, 2> refused_move{}; // the disabled move a side was just refused
  uint16_t turn = 0;
  uint32_t effect_order = 0;
  RequestState request_state = RequestState::None;
  bool started = false, ended = false, mid_turn = false;
  bool report_percentages = false, support_cancel = false, quick_claw_roll = false;
  // Battle.faintQueue: who fainted, and from what. A move runs the Faint
  // event itself before the faints are resolved.
  struct Faint {
    Ref target, source;
    Effect effect;
    bool faint_event_done = false;
  };
  std::array<Faint, 4> faint_queue{};
  uint8_t faints = 0;
  bool sent_requests = true;
  // Showdown told a side, and it alone, that its Pokémon will Struggle; whatever
  // writes the sides' lines clears it.
  // The Pokémon a side was just told has no move left but Struggle, by its
  // place in the team; -1 for none.
  std::array<int8_t, 2> struggle_notice{-1, -1};
  // Battle.activeMove, activePokemon and activeTarget: the move in use, as a
  // copy that its use may change.
  struct ActiveMove {
    Move id = Move::None;
    Type type = Type::None;
    Category category = Category::Status;
    uint8_t base_power = 0;
    uint8_t accuracy = 0; // 0: always hits
    Target target = Target::Normal;
    uint8_t crit_ratio = 0;
    bool crit = false;   // moveHitData of the one target
    int8_t type_mod = 0;
    uint16_t total_damage = 0;
    bool self_dropped = false; // its stat changes to its user were applied
    // What a handler may change of the move's data.
    uint8_t secondary_chance = 0;
    Boosts self_boosts;
    int8_t self_boosts_first = -1; // the stat its self boosts list first, if not the usual order
    Condition volatile_status = Condition::None;
    bool on_hit = false; // it still has its onHit
    uint8_t hit = 0;           // which hit of a multi-hit move this is, from 1
    bool has_bounced = false;  // Magic Coat sent it back
    bool never_crit = false;   // Future Sight's damage, worked out when it is used
    uint32_t fixed_damage = 0; // Bide: the damage of the move it makes up to release
    uint8_t magnitude = 0;     // Magnitude: what it drew
    bool heals = false;        // Present: it heals a quarter in place of damage
    uint8_t flinch_chance = 0; // King's Rock: a second secondary, to flinch
    Effect source_effect; // what the move was used from: a move that called it, or `lockedmove`
    // Beat Up: the Pokémon that still have a hit to give, by team index, and
    // how many hits the move has.
    std::array<uint8_t, TEAM_SIZE> allies{};
    uint8_t n_allies = 0;
    uint8_t hits = 0; // 0: as the move's data says
  };
  ActiveMove active_move;
  // The last move another move had used (Sleep Talk's pick): a move's
  // AfterMove is run for it.
  Move called_move = Move::None;
  bool has_active_move = false;
  Ref active_pokemon, active_target;
  int8_t winner = -1; // 0 or 1 once the battle ends with one; 2: a tie
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
