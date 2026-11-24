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
    Pokemon,  // `p1a: Name` when active, `p1: Name` when not
    Details,  // getFullDetails: details and health, split by viewer
    Health,   // getHealth: split by viewer
    Ability,  // an ability's name
    Move,     // a move's name
    Item,     // an item's name
    Wisher,   // `[wisher] Name`
    From,     // `[from] ` and an effect's full name
    FromMove, // `[from] ` and a move's bare name
    Of,       // `[of] p1a: Name`
  } kind = Kind::Text;
  std::string_view text;
  int32_t number = 0;
  Ref pokemon;
  bool active = false;
  // Details and Health: what the text is made from.
  Species species = Species::None;
  uint8_t level = 0;
  Gender gender = Gender::None;
  bool shiny = false;
  uint16_t hp = 0, maxhp = 0;
  Condition status = Condition::None;
  uint16_t id = 0;
  EffectKind effect_kind = EffectKind::Move;
};

struct Line {
  std::array<Part, 7> parts{};
  uint8_t n = 0;
  // addSplit(side, secret) with no shared line: this side is sent the line
  // and the others an empty one. -1: an ordinary line.
  int8_t secret_side = -1;
};

namespace part {
constexpr Part text(std::string_view t) { return {.kind = Part::Kind::Text, .text = t}; }
constexpr Part number(int32_t v) { return {.kind = Part::Kind::Number, .number = v}; }
constexpr Part side(int s) { return {.kind = Part::Kind::Side, .number = s}; }
constexpr Part player(int s) { return {.kind = Part::Kind::Player, .number = s}; }
constexpr Part side_name(int s) { return {.kind = Part::Kind::SideName, .number = s}; }
constexpr Part ability(Ability a) { return {.kind = Part::Kind::Ability, .id = uint16_t(a)}; }
constexpr Part move(adv::Move m) { return {.kind = Part::Kind::Move, .id = uint16_t(m)}; }
constexpr Part item(adv::Item i) { return {.kind = Part::Kind::Item, .id = uint16_t(i)}; }
constexpr Part wisher(Ref r) { return {.kind = Part::Kind::Wisher, .pokemon = r}; }
constexpr Part from(Effect e) { return {.kind = Part::Kind::From, .id = e.id, .effect_kind = e.kind}; }
constexpr Part from_ability(Ability a) { return from(effect(a)); }
constexpr Part from_move(adv::Move m) { return {.kind = Part::Kind::FromMove, .id = uint16_t(m)}; }
inline Part pokemon(const Battle& b, Ref r) {
  return {.kind = Part::Kind::Pokemon, .pokemon = r, .active = b.at(r).is_active};
}
inline Part of(const Battle& b, Ref r) {
  return {.kind = Part::Kind::Of, .pokemon = r, .active = b.at(r).is_active};
}
inline Part health(const Battle& b, Ref r) {
  const Pokemon& p = b.at(r);
  return {.kind = Part::Kind::Health, .pokemon = r, .hp = p.hp, .maxhp = p.maxhp, .status = p.status};
}
inline Part details(const Battle& b, Ref r) {
  const Pokemon& p = b.at(r);
  Part d = health(b, r);
  d.kind = Part::Kind::Details;
  d.species = p.species;
  d.level = p.level;
  d.gender = p.gender;
  d.shiny = p.set.shiny;
  return d;
}
} // namespace part

} // namespace adv
