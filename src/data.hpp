// The static data of Gen 3 as the pinned Showdown has it. The tables under
// data/ are generated (tools/gen_tables.cjs); this file gives them their
// shapes. An index is the enum's value, and entry 0 is None.

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "data/ids.hpp"

namespace adv {

enum class Stat : uint8_t { Hp, Atk, Def, Spa, Spd, Spe };
enum class Category : uint8_t { Physical, Special, Status };
enum class Gender : uint8_t { Either, Male, Female, None };
enum class SelfDestruct : uint8_t { None, IfHit, Always };
enum class SelfSwitch : uint8_t { None, Copyvolatile };

// Showdown's id and display name.
struct Name {
  std::string_view id;
  std::string_view name;
};

struct Boosts {
  int8_t atk = 0, def = 0, spa = 0, spd = 0, spe = 0, accuracy = 0, evasion = 0;
  constexpr bool operator==(const Boosts&) const = default;
};

// [numerator, denominator], or {0, 0} for none.
struct Fraction {
  uint8_t num = 0, den = 0;
  constexpr explicit operator bool() const { return den != 0; }
};

struct SpeciesData {
  uint16_t num = 0;
  std::array<Type, 2> types{};
  std::array<uint8_t, 6> base{}; // by Stat
  std::array<Ability, 2> abilities{};
  uint16_t weight_hg = 0;
  uint16_t max_hp = 0;            // Shedinja's 1; 0: from the formula
  Gender gender = Gender::Either; // the one gender it can have, or Either
  uint8_t female_eighths = 4;     // of Either: how many in eight are female
  Species base_species = Species::None;
};

constexpr uint64_t flag(MoveFlag f) { return uint64_t{1} << uint8_t(f); }
static_assert(MOVE_FLAGS <= 64);

struct Secondary {
  uint8_t chance = 0; // 0: the move has none
  Condition status = Condition::None;
  Condition volatile_status = Condition::None;
  Boosts boosts;
  Boosts self_boosts;
};

struct MoveData {
  uint16_t num = 0;
  // A typed Hidden Power is the move Hidden Power to Showdown (the id
  // `hiddenpower`), with a type; None for every other move.
  Move real_move = Move::None;
  Type type = Type::None;
  Category category = Category::Status;
  uint8_t base_power = 0;
  uint8_t accuracy = 0; // 0: Showdown's `true`, no accuracy check
  uint8_t pp = 0;
  int8_t priority = 0;
  Target target = Target::Normal;
  uint8_t crit_ratio = 0;
  uint64_t flags = 0;
  uint8_t damage = 0;        // a fixed amount
  bool damage_level = false; // Showdown's `damage: 'level'`
  Fraction drain, recoil, heal;
  std::array<uint8_t, 2> multihit{}; // least and most hits, or {0, 0}
  SelfDestruct selfdestruct = SelfDestruct::None;
  SelfSwitch self_switch = SelfSwitch::None;
  bool multiaccuracy = false, ohko = false, force_switch = false, ignore_immunity = false;
  bool no_pp_boosts = false, sleep_usable = false, stalling_move = false, calls_move = false;
  bool has_crash_damage = false;
  Condition status = Condition::None;
  Condition volatile_status = Condition::None;
  Condition side_condition = Condition::None;
  Condition slot_condition = Condition::None;
  Condition weather = Condition::None;
  Boosts boosts;
  Secondary secondary;
  Boosts self_boosts;
  Condition self_volatile_status = Condition::None;
};

struct ItemData {
  uint16_t num = 0;
  bool is_berry = false, is_choice = false, is_pokeball = false;
  std::array<Species, 2> users{}; // Showdown's itemUser, among Gen 3's species
};

struct AbilityData {
  uint16_t num = 0;
  bool suppress_weather = false;
};

// What carries a condition's definition: Showdown's conditions table, or a
// move or ability of the same id.
enum class Holder : uint8_t { Standalone, Move, Ability };
enum class ConditionKind : uint8_t { Condition, Status, Weather };

struct ConditionData {
  Holder holder = Holder::Standalone;
  ConditionKind kind = ConditionKind::Condition;
  uint8_t duration = 0;
  uint8_t counter_max = 0;
  bool no_copy = false, affects_fainted = false;
};

struct NatureData {
  Stat plus = Stat::Hp, minus = Stat::Hp; // Hp and Hp: neutral
};

// Which table an effect's id is an index of.
enum class EffectKind : uint8_t { Move, Ability, Item, Condition, Rule, Special };
// Effects Showdown makes up by name where one is needed and none exists.
enum class Special : uint16_t { None, Recoil, Drain, Confused };

// One handler as Showdown gathers it: who answers to which name, and the
// `Order`, `Priority` and `SubOrder` it is sorted by. `handled` is false for
// ranks given under a name the effect has no handler for (an ability's
// `onSwitchInPriority` when its handler is `onStart`).
struct Handler {
  EffectKind kind;
  uint16_t id;
  Callback callback;
  int32_t order;
  int32_t priority;
  int32_t sub_order;
  bool handled;
};

constexpr uint16_t immunity(Immunity i) { return uint16_t(1u << uint8_t(i)); }

#include "data/names.inc"
#include "data/types.inc"

inline constexpr SpeciesData SPECIES_DATA[SPECIESS] = {
  {},
#include "data/species.inc"
};
inline constexpr MoveData MOVE_DATA[MOVES] = {
  {},
#include "data/moves.inc"
};
inline constexpr ItemData ITEM_DATA[ITEMS] = {
  {},
#include "data/items.inc"
};
inline constexpr AbilityData ABILITY_DATA[ABILITYS] = {
  {},
#include "data/abilities.inc"
};
inline constexpr ConditionData CONDITION_DATA[CONDITIONS] = {
  {},
#include "data/conditions.inc"
};
inline constexpr NatureData NATURE_DATA[NATURES] = {
  {},
#include "data/natures.inc"
};
// Sorted by kind, then the effect's name, then the callback's.
inline constexpr Handler HANDLERS[] = {
#include "data/handlers.inc"
};

constexpr const SpeciesData& data(Species s) { return SPECIES_DATA[uint16_t(s)]; }
constexpr const MoveData& data(Move m) { return MOVE_DATA[uint16_t(m)]; }
constexpr const ItemData& data(Item i) { return ITEM_DATA[uint8_t(i)]; }
constexpr const AbilityData& data(Ability a) { return ABILITY_DATA[uint8_t(a)]; }
constexpr const ConditionData& data(Condition c) { return CONDITION_DATA[uint8_t(c)]; }
constexpr const NatureData& data(Nature n) { return NATURE_DATA[uint8_t(n)]; }

// Showdown's `damageTaken` of a defending type against an attacking one:
// 0 normal, 1 super effective, 2 resisted, 3 immune.
constexpr uint8_t damage_taken(Type defender, Type attacker) { return TYPE_CHART[uint8_t(defender)][uint8_t(attacker)]; }
constexpr bool immune(Type type, Immunity to) { return TYPE_IMMUNITIES[uint8_t(type)] & immunity(to); }

} // namespace adv
