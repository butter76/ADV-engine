// The log as Showdown sends it: each Line's text, and for a line that shows a
// Pokémon's details or health, the `|split|` line and the two views of it.

#pragma once

#include <string>
#include <vector>

#include "../out.hpp"
#include "request.hpp"

namespace adv::showdown {

struct Names {
  std::array<std::string, 2> players;
  std::array<TeamText, 2> teams;
};

inline std::string side_id(int side) { return side == 0 ? "p1" : "p2"; }

[[gnu::always_inline]] inline void put_signed(Cursor& out, int32_t value) {
  if (value < 0) out.put('-');
  put_number(out, value < 0 ? unsigned(-int64_t(value)) : unsigned(value));
}
[[gnu::always_inline]] inline void put_side_id(Cursor& out, int side) { out.put(std::string_view(side == 0 ? "p1" : "p2", 2)); }

// getHealth (pokemon.ts:2082) from what a part kept: the Pokémon's own side
// sees its HP, the other a percentage rounded up, and 99 when not full.
[[gnu::always_inline]] inline void put_health(Cursor& out, const Part& p, bool secret, bool report_percentages) {
  if (!p.hp()) {
    out.put("0 fnt");
    return;
  }
  if (secret) {
    put_number(out, p.hp());
    out.put('/');
    put_number(out, p.maxhp());
  } else if (report_percentages) {
    int percentage = (100 * p.hp() + p.maxhp() - 1) / p.maxhp();
    if (percentage == 100 && p.hp() < p.maxhp()) percentage = 99;
    put_number(out, unsigned(percentage));
    out.put("/100");
  } else {
    put_number(out, unsigned(std::max(48 * p.hp() / p.maxhp(), 1)));
    out.put("/48");
  }
  if (p.status != Condition::None) {
    out.put(' ');
    out.put(CONDITION_NAMES[uint8_t(p.status)].id);
  }
}

// One part of a line as Showdown's text, appended.
[[gnu::always_inline]] inline void put_part(Cursor& out, const Part& p, bool secret, const Battle& b, const Names& names) {
  switch (p.kind) {
  case Part::Kind::Text: out.put(p.text()); return;
  case Part::Kind::Number: put_signed(out, p.number()); return;
  case Part::Kind::Side: put_side_id(out, p.number()); return;
  case Part::Kind::Player: out.put(names.players[size_t(p.number())]); return;
  case Part::Kind::TurnsLeft:
    out.put("You will auto-tie if the battle doesn't end in ");
    if (p.number() == 1) {
      out.put("1 turn");
    } else {
      put_signed(out, p.number());
      out.put(" turns");
    }
    out.put(" (on turn 1000).");
    return;
  case Part::Kind::PlayerSays:
    out.put(names.players[size_t(p.active)]);
    out.put(p.text());
    return;
  case Part::Kind::SideName:
    put_side_id(out, p.number());
    out.put(": ");
    out.put(names.players[size_t(p.number())]);
    return;
  case Part::Kind::Pokemon:
  case Part::Kind::Of:
    if (p.kind == Part::Kind::Of) out.put("[of] ");
    put_side_id(out, p.pokemon().side);
    if (p.active) out.put("a: ");
    else out.put(": ");
    out.put(names.teams[p.pokemon().side].names[p.pokemon().index]);
    return;
  case Part::Kind::Ability: out.put(ABILITY_NAMES[p.id()].name); return;
  case Part::Kind::Move: out.put(MOVE_NAMES[p.id()].name); return;
  case Part::Kind::HiddenPower:
    out.put("Hidden Power ");
    out.put(TYPE_NAMES[p.id()].name);
    return;
  case Part::Kind::FromMove:
    out.put("[from] ");
    out.put(MOVE_NAMES[p.id()].name);
    return;
  case Part::Kind::Item: out.put(ITEM_NAMES[p.id()].name); return;
  case Part::Kind::Wisher:
    out.put("[wisher] ");
    out.put(names.teams[p.pokemon().side].names[p.pokemon().index]);
    return;
  case Part::Kind::Name:
    switch (p.effect_kind()) {
    case EffectKind::Ability: out.put(ABILITY_NAMES[p.id()].name); return;
    case EffectKind::Item: out.put(ITEM_NAMES[p.id()].name); return;
    case EffectKind::Move: out.put(MOVE_NAMES[p.id()].name); return;
    case EffectKind::Condition: out.put(CONDITION_NAMES[p.id()].name); return;
    default: break;
    }
    out.put('?');
    return;
  case Part::Kind::From:
  case Part::Kind::Fullname:
    // An effect's fullname. Damage from `tox` is written as `psn`.
    if (p.kind == Part::Kind::From) out.put("[from] ");
    switch (p.effect_kind()) {
    case EffectKind::Ability:
      out.put("ability: ");
      out.put(ABILITY_NAMES[p.id()].name);
      return;
    case EffectKind::Item:
      out.put("item: ");
      out.put(ITEM_NAMES[p.id()].name);
      return;
    case EffectKind::Move:
      out.put("move: ");
      out.put(MOVE_NAMES[p.id()].name);
      return;
    case EffectKind::Condition:
      out.put(CONDITION_NAMES[Condition(p.id()) == Condition::Tox ? uint16_t(Condition::Psn) : p.id()].name);
      return;
    case EffectKind::Special:
      out.put(Special(p.id()) == Special::Recoil ? "Recoil" : Special(p.id()) == Special::Drain ? "drain" : "confusion");
      return;
    case EffectKind::Rule: break;
    }
    out.put('?');
    return;
  case Part::Kind::Health: put_health(out, p, secret, b.report_percentages); return;
  case Part::Kind::Details:
    out.put(SPECIES_NAMES[uint16_t(p.species())].name);
    if (p.level() != 100) {
      out.put(", L");
      put_number(out, p.level());
    }
    if (p.gender() == Gender::Male) out.put(", M");
    if (p.gender() == Gender::Female) out.put(", F");
    if (p.shiny) out.put(", shiny");
    out.put('|');
    put_health(out, p, secret, b.report_percentages);
    return;
  }
}
// How much room a part's text is given to begin with: what it holds of
// text or of a name, and more than any of the engine's own names is long.
inline size_t room_for(const Part& p, const Names& names) {
  switch (p.kind) {
  case Part::Kind::Text: return p.text().size();
  case Part::Kind::PlayerSays: return p.text().size() + names.players[size_t(p.active)].size();
  case Part::Kind::Player:
  case Part::Kind::SideName: return 8 + names.players[size_t(p.number())].size();
  case Part::Kind::Pokemon:
  case Part::Kind::Of:
  case Part::Kind::Wisher: return 16 + names.teams[p.pokemon().side].names[p.pokemon().index].size();
  default: return 64;
  }
}
inline size_t room_for(const Line& line, const Names& names) {
  size_t room = 0;
  for (int i = 0; i < line.n; i++) room += 1 + room_for(line.parts[size_t(i)], names);
  return room;
}
inline void append_part(std::string& out, const Part& p, bool secret, const Battle& b, const Names& names) {
  append_with(out, room_for(p, names), [&](Cursor to) {
    put_part(to, p, secret, b, names);
    return to;
  });
}
inline std::string part_text(const Part& p, bool secret, const Battle& b, const Names& names) {
  std::string out;
  append_part(out, p, secret, b, names);
  return out;
}
[[gnu::always_inline]] inline void put_line(Cursor& out, const Line& line, bool secret, const Battle& b, const Names& names) {
  for (int i = 0; i < line.n; i++) {
    out.put('|');
    put_part(out, line.parts[size_t(i)], secret, b, names);
  }
}

// Battle.add (battle.ts) and addSplit: the lines one Line becomes, each after
// `prefix`.
inline void write(const Line& line, const Battle& b, const Names& names, std::vector<std::string>& out, std::string_view prefix = {}) {
  int split = line.secret_side;
  for (int i = 0; i < line.n; i++) {
    const Part& p = line.parts[i];
    if (p.kind == Part::Kind::Details || p.kind == Part::Kind::Health) split = p.pokemon().side;
  }
  const size_t room = prefix.size() + room_for(line, names);
  const auto text = [&](bool secret) {
    append_with(out.emplace_back(), room, [&](Cursor to) {
      to.put(prefix);
      put_line(to, line, secret, b, names);
      return to;
    });
  };
  if (split < 0) return text(true);
  out.emplace_back(prefix).append(split == 0 ? "|split|p1" : "|split|p2", 9);
  text(true);
  if (line.secret_side >= 0) out.emplace_back(prefix);
  else text(false);
}

// A line of one player's view (view.hpp) as that player is sent it. `names`
// has the other side's Pokémon in the order the view numbers them.
inline void append_line(std::string& out, const Line& line, const Battle& b, const Names& names) {
  append_with(out, room_for(line, names), [&](Cursor to) {
    put_line(to, line, true, b, names);
    return to;
  });
}
inline std::string line_text(const Line& line, const Battle& b, const Names& names) {
  std::string out;
  append_line(out, line, b, names);
  return out;
}

// A request's JSON (Battle.getRequests, battle.ts:1432).
[[gnu::always_inline]] inline void put_request_json(Cursor& out, const Battle& b, int side, const Names& names) {
  const Request& r = b.requests[side];
  // What comes before the side: nothing to choose, a Pokémon to replace, or
  // the active Pokémon's moves. (One body, with no helper that is handed the
  // cursor: see Cursor.)
  if (r.kind == Request::Kind::Wait) {
    out.put("{\"wait\":true,");
  } else if (r.kind == Request::Kind::Switch) {
    out.put("{\"forceSwitch\":[true],");
  } else {
    const Pokemon& p = b.sides[side].pokemon[b.sides[side].active];
    out.put("{\"active\":[{\"moves\":[");
    for (int i = 0; i < r.n_moves; i++) {
      const RequestMove& m = r.moves[i];
      if (i) out.put(',');
      if (r.locked) {
        // Pokemon.getMoves(lockedMove): the name and the id.
        if (m.id == Move::None) {
          out.put("{\"move\":\"Recharge\",\"id\":\"recharge\"}");
        } else {
          out.put("{\"move\":");
          put_json(out, MOVE_NAMES[uint16_t(m.id)].name);
          out.put(",\"id\":");
          put_json(out, MOVE_NAMES[uint16_t(m.id)].id);
          out.put('}');
        }
        continue;
      }
      // Pokemon.getMoves (pokemon.ts:985): Hidden Power is named with its type
      // and power, Return and Frustration with their power. No move's name
      // needs escaping.
      out.put("{\"move\":\"");
      out.put(MOVE_NAMES[uint16_t(m.id)].name);
      if (m.id == Move::HiddenPower) {
        out.put(' ');
        out.put(TYPE_NAMES[uint8_t(p.hp_type)].name);
        out.put(' ');
        put_number(out, p.hp_power);
      }
      if (m.id == Move::Return) {
        out.put(' ');
        put_number(out, unsigned(std::max(p.happiness * 10 / 25, 1)));
      }
      if (m.id == Move::Frustration) {
        out.put(' ');
        put_number(out, unsigned(std::max((255 - p.happiness) * 10 / 25, 1)));
      }
      out.put("\",\"id\":\"");
      out.put(MOVE_NAMES[uint16_t(m.id)].id);
      out.put('"');
      if (!r.struggle) {
        out.put(",\"pp\":");
        put_number(out, m.pp);
        out.put(",\"maxpp\":");
        put_number(out, m.maxpp);
      }
      if (!m.no_target) {
        out.put(",\"target\":\"");
        out.put(TARGET_NAMES[uint8_t(m.target)]);
        out.put('"');
      }
      if (m.disabled) out.put(",\"disabled\":true");
      else out.put(",\"disabled\":false");
      if (m.disabled_source) out.put(",\"disabledSource\":\"\"");
      out.put('}');
    }
    out.put(']');
    if (r.maybe_disabled) out.put(",\"maybeDisabled\":true");
    if (r.maybe_locked) out.put(",\"maybeLocked\":true");
    if (r.trapped) out.put(",\"trapped\":true");
    if (r.maybe_trapped) out.put(",\"maybeTrapped\":true");
    out.put("}],");
  }
  out.put("\"side\":");
  put_side_request(out, b, side, names.players[side], names.teams[side]);
  if (r.kind != Request::Kind::Wait) {
    if (r.no_cancel) out.put(",\"noCancel\":true");
    if (r.update) out.put(",\"update\":true");
  }
  out.put('}');
}
// Appended to `out`, after `before`.
inline void append_request_json(std::string& out, const Battle& b, int side, const Names& names, std::string_view before = {}) {
  // A request is about 2,000 bytes; room for one that is not, so that few are written twice.
  append_with(out, 4096, [&](Cursor to) {
    to.put(before);
    put_request_json(to, b, side, names);
    return to;
  });
}
inline std::string request_json(const Battle& b, int side, const Names& names) {
  std::string out;
  out.reserve(2048);
  append_request_json(out, b, side, names);
  return out;
}

} // namespace adv::showdown
