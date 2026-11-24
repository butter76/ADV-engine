// The log as one player is sent it.
//
// The engine's log is omniscient: a Line names a Pokémon by its place in its
// team and holds its HP as it is. Showdown sends each player their own
// channel of it (Battle.addSplit, and extractChannelMessages in
// sim/battle.ts): their own Pokémon's HP as numbers and the other side's as a
// share, and of a line written for one side alone, an empty line to the other.
//
// A View makes that channel, as Lines. One of them holds only what its player
// is shown, so that whatever reads it cannot learn more:
//
//   - a Pokémon of the other side is its number, the order in which the log
//     first named that side's Pokémon to this player, in place of its index
//     in a team the player has not seen;
//   - the other side's health is what the text would say: out of 100 under
//     the HP Percentage Mod, out of 48 without it.
//
// The player's own Pokémon are as the engine has them. showdown/log.hpp's
// `line_text` writes a View's line as Showdown's text.

#pragma once

#include <array>
#include <cstdint>

#include "battle.hpp"
#include "log.hpp"

namespace adv {

struct View {
  uint8_t side = 0;
  // Of the other side's Pokémon, by its index in its team: its number, or -1
  // while the log has not named it. And the index of each number.
  std::array<int8_t, TEAM_SIZE> number{-1, -1, -1, -1, -1, -1};
  std::array<uint8_t, TEAM_SIZE> index{};
  uint8_t seen = 0;

  explicit constexpr View(int s) : side(uint8_t(s)) {}

  // The other side's Pokémon as this player knows it.
  constexpr Ref shown(Ref r) {
    if (r.side == side || !r) return r;
    if (number[r.index] < 0) {
      number[r.index] = int8_t(seen);
      index[seen++] = r.index;
    }
    return {r.side, uint8_t(number[r.index])};
  }

  // getHealth (pokemon.ts:2082) for a side that is not the Pokémon's own: a
  // percentage rounded up, and 99 when not full; or 48ths, and at least one.
  static constexpr void shown_health(Part& p, bool report_percentages) {
    Part::Big::Who& who = p.big.who;
    if (!who.hp) {
      who.maxhp = report_percentages ? 100 : 48;
      return;
    }
    if (report_percentages) {
      int percentage = (100 * who.hp + who.maxhp - 1) / who.maxhp;
      if (percentage == 100 && who.hp < who.maxhp) percentage = 99;
      who.hp = uint16_t(percentage);
      who.maxhp = 100;
    } else {
      who.hp = uint16_t(std::max(48 * who.hp / who.maxhp, 1));
      who.maxhp = 48;
    }
  }

  // `in` as this player is sent it. False for a line written for the other
  // side alone: the player is sent an empty line in its place.
  constexpr bool line(const Line& in, const Battle& b, Line& out) {
    if (in.secret_side >= 0 && in.secret_side != side) return false;
    out = in;
    out.secret_side = -1;
    for (int i = 0; i < out.n; i++) {
      Part& p = out.parts[size_t(i)];
      switch (p.kind) {
      case Part::Kind::Details:
      case Part::Kind::Health:
        if (p.big.who.pokemon.side != side) shown_health(p, b.report_percentages);
        [[fallthrough]];
      case Part::Kind::Pokemon:
      case Part::Kind::Of:
      case Part::Kind::Wisher: p.big.who.pokemon = shown(p.big.who.pokemon); break;
      default: break;
      }
    }
    return true;
  }
};

} // namespace adv
