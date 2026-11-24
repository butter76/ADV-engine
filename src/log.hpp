// The battle's log: one Line for each call of Showdown's `add`, holding the
// parts it was called with, typed. Text is made from it elsewhere
// (showdown/log.hpp); a reader that wants ids reads the parts themselves.
//
// A Pokémon, its details and its health are kept as they were when the line
// was written, since Showdown turns them to text then.

#pragma once

#include <array>
#include <cstdint>
#include <string_view>

#include "battle.hpp"

namespace adv {

struct Part {
  enum class Kind : uint8_t {
    Text,     // a literal of Showdown's source
    Number,
    Side,     // `p1`
    SideName, // a side as text: `p1: Name`
    Player,   // the player's name
    PlayerSays, // the player's name and then a literal, in one field
    TurnsLeft,  // the warning that the battle will be tied at turn 1000
    Pokemon,  // `p1a: Name` when active, `p1: Name` when not
    Details,  // getFullDetails: details and health, split by viewer
    Health,   // getHealth: split by viewer
    Ability,  // an ability's name
    Move,     // a move's name
    HiddenPower, // `Hidden Power ` and a type's name
    Item,     // an item's name
    Wisher,   // `[wisher] Name`
    From,     // `[from] ` and an effect's full name
    Fullname, // an effect's full name: `move: Wrap`
    Name,     // an effect's name: `Wrap`
    FromMove, // `[from] ` and a move's bare name
    Of,       // `[of] p1a: Name`
  } kind = Kind::Text;
  // Sixteen bytes: nearly five million lines a second are written, so what a
  // part holds is kept small. Which fields mean anything depends on the kind.
  bool active = false;                // Pokemon, Of; the side of PlayerSays
  bool shiny = false;                 // Details
  Condition status = Condition::None; // Details, Health
  union Small {
    uint32_t length = 0; // Text, PlayerSays: of the text
    struct Of {
      uint8_t level;
      Gender gender;
      EffectKind effect_kind;
    } of; // Details: level and gender; From, Fullname, Name: the effect's kind
  } small;
  union Big {
    const char* chars = nullptr; // Text, PlayerSays
    int32_t number;              // Number, Side, SideName, Player, TurnsLeft
    struct Who {
      Ref pokemon;
      Species species;
      uint16_t hp, maxhp;
    } who;       // Pokemon, Of, Wisher, Details, Health
    uint16_t id; // an ability, move, item, type or effect
  } big;

  constexpr std::string_view text() const { return {big.chars, small.length}; }
  constexpr int32_t number() const { return big.number; }
  constexpr Ref pokemon() const { return big.who.pokemon; }
  constexpr Species species() const { return big.who.species; }
  constexpr uint16_t hp() const { return big.who.hp; }
  constexpr uint16_t maxhp() const { return big.who.maxhp; }
  constexpr uint8_t level() const { return small.of.level; }
  constexpr Gender gender() const { return small.of.gender; }
  constexpr uint16_t id() const { return big.id; }
  constexpr EffectKind effect_kind() const { return small.of.effect_kind; }
};
static_assert(sizeof(Part) == 16);

// A line's parts. The most Showdown writes is a `move` line that every
// attribute was added to: the user, the move and its target, what it was
// used from, `[still]`, a `[miss]` for each step of the hit that found it
// had missed, and one more for each Future Sight that then misses before
// another move is used (attrLastMove adds to whatever `move` line is the
// last). More than this is refused, not written past the end.
inline constexpr int LINE_PARTS = 12;

struct Line {
  std::array<Part, LINE_PARTS> parts{};
  uint8_t n = 0;
  // addSplit(side, secret) with no shared line: this side is sent the line
  // and the others an empty one. -1: an ordinary line.
  int8_t secret_side = -1;
};

namespace part {
constexpr Part text(std::string_view t) {
  return {.kind = Part::Kind::Text, .small = {.length = uint32_t(t.size())}, .big = {.chars = t.data()}};
}
constexpr Part of_number(Part::Kind kind, int32_t v) { return {.kind = kind, .big = {.number = v}}; }
constexpr Part of_id(Part::Kind kind, uint16_t id, EffectKind effect_kind = EffectKind::Move) {
  return {.kind = kind, .small = {.of = {0, Gender::None, effect_kind}}, .big = {.id = id}};
}
constexpr Part number(int32_t v) { return of_number(Part::Kind::Number, v); }
constexpr Part side(int s) { return of_number(Part::Kind::Side, s); }
constexpr Part player(int s) { return of_number(Part::Kind::Player, s); }
constexpr Part turns_left(int n) { return of_number(Part::Kind::TurnsLeft, n); }
constexpr Part player_says(int s, std::string_view t) {
  Part p = text(t);
  p.kind = Part::Kind::PlayerSays;
  p.active = s != 0;
  return p;
}
constexpr Part side_name(int s) { return of_number(Part::Kind::SideName, s); }
constexpr Part ability(Ability a) { return of_id(Part::Kind::Ability, uint16_t(a)); }
constexpr Part move(adv::Move m) { return of_id(Part::Kind::Move, uint16_t(m)); }
constexpr Part hidden_power(Type t) { return of_id(Part::Kind::HiddenPower, uint16_t(t)); }
constexpr Part item(adv::Item i) { return of_id(Part::Kind::Item, uint16_t(i)); }
constexpr Part from(Effect e) { return of_id(Part::Kind::From, e.id, e.kind); }
constexpr Part fullname(Effect e) { return of_id(Part::Kind::Fullname, e.id, e.kind); }
constexpr Part name(Effect e) { return of_id(Part::Kind::Name, e.id, e.kind); }
constexpr Part from_ability(Ability a) { return from(effect(a)); }
constexpr Part from_move(adv::Move m) { return of_id(Part::Kind::FromMove, uint16_t(m)); }
constexpr Part wisher(Ref r) { return {.kind = Part::Kind::Wisher, .big = {.who = {r, Species::None, 0, 0}}}; }
inline Part pokemon(const Battle& b, Ref r) {
  return {.kind = Part::Kind::Pokemon, .active = b.at(r).is_active, .big = {.who = {r, Species::None, 0, 0}}};
}
inline Part of(const Battle& b, Ref r) {
  return {.kind = Part::Kind::Of, .active = b.at(r).is_active, .big = {.who = {r, Species::None, 0, 0}}};
}
inline Part health(const Battle& b, Ref r) {
  const Pokemon& p = b.at(r);
  return {.kind = Part::Kind::Health, .status = p.status, .big = {.who = {r, Species::None, p.hp, p.maxhp}}};
}
inline Part details(const Battle& b, Ref r) {
  const Pokemon& p = b.at(r);
  // Pokemon.details is not changed by a form taken in battle.
  return {.kind = Part::Kind::Details, .shiny = p.set.shiny, .status = p.status,
          .small = {.of = {p.level, p.gender, EffectKind::Move}}, .big = {.who = {r, p.base_species, p.hp, p.maxhp}}};
}
} // namespace part

} // namespace adv
