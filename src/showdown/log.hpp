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

inline void append_signed(std::string& out, int32_t value) {
  if (value < 0) out += '-';
  append_number(out, value < 0 ? unsigned(-int64_t(value)) : unsigned(value));
}
inline void append_side_id(std::string& out, int side) { out += side == 0 ? "p1" : "p2"; }

// getHealth (pokemon.ts:2082) from what a part kept: the Pokémon's own side
// sees its HP, the other a percentage rounded up, and 99 when not full.
inline void append_health(std::string& out, const Part& p, bool secret, bool report_percentages) {
  if (!p.hp()) {
    out += "0 fnt";
    return;
  }
  if (secret) {
    append_number(out, p.hp());
    out += '/';
    append_number(out, p.maxhp());
  } else if (report_percentages) {
    int percentage = (100 * p.hp() + p.maxhp() - 1) / p.maxhp();
    if (percentage == 100 && p.hp() < p.maxhp()) percentage = 99;
    append_number(out, unsigned(percentage));
    out += "/100";
  } else {
    append_number(out, unsigned(std::max(48 * p.hp() / p.maxhp(), 1)));
    out += "/48";
  }
  if (p.status != Condition::None) {
    out += ' ';
    out += CONDITION_NAMES[uint8_t(p.status)].id;
  }
}

// One part of a line as Showdown's text, appended.
inline void append_part(std::string& out, const Part& p, bool secret, const Battle& b, const Names& names) {
  const auto slot = [&](std::string_view lead) {
    out += lead;
    append_side_id(out, p.pokemon().side);
    out += p.active ? "a: " : ": ";
    out += names.teams[p.pokemon().side].names[p.pokemon().index];
  };
  switch (p.kind) {
  case Part::Kind::Text: out += p.text(); return;
  case Part::Kind::Number: append_signed(out, p.number()); return;
  case Part::Kind::Side: append_side_id(out, p.number()); return;
  case Part::Kind::Player: out += names.players[size_t(p.number())]; return;
  case Part::Kind::TurnsLeft:
    out += "You will auto-tie if the battle doesn't end in ";
    if (p.number() == 1) {
      out += "1 turn";
    } else {
      append_signed(out, p.number());
      out += " turns";
    }
    out += " (on turn 1000).";
    return;
  case Part::Kind::PlayerSays:
    out += names.players[size_t(p.active)];
    out += p.text();
    return;
  case Part::Kind::SideName:
    append_side_id(out, p.number());
    out += ": ";
    out += names.players[size_t(p.number())];
    return;
  case Part::Kind::Pokemon: slot(""); return;
  case Part::Kind::Of: slot("[of] "); return;
  case Part::Kind::Ability: out += ABILITY_NAMES[p.id()].name; return;
  case Part::Kind::Move: out += MOVE_NAMES[p.id()].name; return;
  case Part::Kind::HiddenPower:
    out += "Hidden Power ";
    out += TYPE_NAMES[p.id()].name;
    return;
  case Part::Kind::FromMove:
    out += "[from] ";
    out += MOVE_NAMES[p.id()].name;
    return;
  case Part::Kind::Item: out += ITEM_NAMES[p.id()].name; return;
  case Part::Kind::Wisher:
    out += "[wisher] ";
    out += names.teams[p.pokemon().side].names[p.pokemon().index];
    return;
  case Part::Kind::Name:
    switch (p.effect_kind()) {
    case EffectKind::Ability: out += ABILITY_NAMES[p.id()].name; return;
    case EffectKind::Item: out += ITEM_NAMES[p.id()].name; return;
    case EffectKind::Move: out += MOVE_NAMES[p.id()].name; return;
    case EffectKind::Condition: out += CONDITION_NAMES[p.id()].name; return;
    default: break;
    }
    out += '?';
    return;
  case Part::Kind::From:
  case Part::Kind::Fullname:
    // An effect's fullname. Damage from `tox` is written as `psn`.
    if (p.kind == Part::Kind::From) out += "[from] ";
    switch (p.effect_kind()) {
    case EffectKind::Ability:
      out += "ability: ";
      out += ABILITY_NAMES[p.id()].name;
      return;
    case EffectKind::Item:
      out += "item: ";
      out += ITEM_NAMES[p.id()].name;
      return;
    case EffectKind::Move:
      out += "move: ";
      out += MOVE_NAMES[p.id()].name;
      return;
    case EffectKind::Condition:
      out += CONDITION_NAMES[Condition(p.id()) == Condition::Tox ? uint16_t(Condition::Psn) : p.id()].name;
      return;
    case EffectKind::Special:
      out += Special(p.id()) == Special::Recoil ? "Recoil" : Special(p.id()) == Special::Drain ? "drain" : "confusion";
      return;
    case EffectKind::Rule: break;
    }
    out += '?';
    return;
  case Part::Kind::Health: append_health(out, p, secret, b.report_percentages); return;
  case Part::Kind::Details:
    out += SPECIES_NAMES[uint16_t(p.species())].name;
    if (p.level() != 100) {
      out += ", L";
      append_number(out, p.level());
    }
    if (p.gender() == Gender::Male) out += ", M";
    if (p.gender() == Gender::Female) out += ", F";
    if (p.shiny) out += ", shiny";
    out += '|';
    append_health(out, p, secret, b.report_percentages);
    return;
  }
}
inline std::string part_text(const Part& p, bool secret, const Battle& b, const Names& names) {
  std::string out;
  append_part(out, p, secret, b, names);
  return out;
}

// Battle.add (battle.ts) and addSplit: the lines one Line becomes, each after
// `prefix`.
inline void write(const Line& line, const Battle& b, const Names& names, std::vector<std::string>& out, std::string_view prefix = {}) {
  int split = line.secret_side;
  for (int i = 0; i < line.n; i++) {
    const Part& p = line.parts[i];
    if (p.kind == Part::Kind::Details || p.kind == Part::Kind::Health) split = p.pokemon().side;
  }
  const auto text = [&](bool secret) {
    std::string t(prefix);
    for (int i = 0; i < line.n; i++) {
      t += '|';
      append_part(t, line.parts[i], secret, b, names);
    }
    return t;
  };
  if (split < 0) return out.push_back(text(true));
  out.push_back(std::string(prefix) + (split == 0 ? "|split|p1" : "|split|p2"));
  out.push_back(text(true));
  out.push_back(line.secret_side >= 0 ? std::string(prefix) : text(false));
}

// A line of one player's view (view.hpp) as that player is sent it. `names`
// has the other side's Pokémon in the order the view numbers them.
inline void append_line(std::string& out, const Line& line, const Battle& b, const Names& names) {
  for (int i = 0; i < line.n; i++) {
    out += '|';
    append_part(out, line.parts[size_t(i)], true, b, names);
  }
}
inline std::string line_text(const Line& line, const Battle& b, const Names& names) {
  std::string out;
  append_line(out, line, b, names);
  return out;
}

// A request's JSON (Battle.getRequests, battle.ts:1432), appended to `out`.
inline void append_request_json(std::string& out, const Battle& b, int side, const Names& names) {
  const Request& r = b.requests[side];
  const auto side_and_tail = [&](bool tail) {
    out += "\"side\":";
    append_side_request(out, b, side, names.players[side], names.teams[side]);
    if (tail && r.no_cancel) out += ",\"noCancel\":true";
    if (tail && r.update) out += ",\"update\":true";
    out += '}';
  };
  if (r.kind == Request::Kind::Wait) {
    out += "{\"wait\":true,";
    return side_and_tail(false);
  }
  if (r.kind == Request::Kind::Switch) {
    out += "{\"forceSwitch\":[true],";
    return side_and_tail(true);
  }
  const Pokemon& p = b.sides[side].pokemon[b.sides[side].active];
  out += "{\"active\":[{\"moves\":[";
  for (int i = 0; i < r.n_moves; i++) {
    const RequestMove& m = r.moves[i];
    if (i) out += ',';
    if (r.locked) {
      // Pokemon.getMoves(lockedMove): the name and the id.
      if (m.id == Move::None) {
        out += "{\"move\":\"Recharge\",\"id\":\"recharge\"}";
      } else {
        out += "{\"move\":";
        append_json(out, MOVE_NAMES[uint16_t(m.id)].name);
        out += ",\"id\":";
        append_json(out, MOVE_NAMES[uint16_t(m.id)].id);
        out += '}';
      }
      continue;
    }
    // Pokemon.getMoves (pokemon.ts:985): Hidden Power is named with its type
    // and power, Return and Frustration with their power. No move's name
    // needs escaping.
    out += "{\"move\":\"";
    out += MOVE_NAMES[uint16_t(m.id)].name;
    if (m.id == Move::HiddenPower) {
      out += ' ';
      out += TYPE_NAMES[uint8_t(p.hp_type)].name;
      out += ' ';
      append_number(out, p.hp_power);
    }
    if (m.id == Move::Return) {
      out += ' ';
      append_number(out, unsigned(std::max(p.happiness * 10 / 25, 1)));
    }
    if (m.id == Move::Frustration) {
      out += ' ';
      append_number(out, unsigned(std::max((255 - p.happiness) * 10 / 25, 1)));
    }
    out += "\",\"id\":\"";
    out += MOVE_NAMES[uint16_t(m.id)].id;
    out += '"';
    if (!r.struggle) {
      out += ",\"pp\":";
      append_number(out, m.pp);
      out += ",\"maxpp\":";
      append_number(out, m.maxpp);
    }
    if (!m.no_target) {
      out += ",\"target\":\"";
      out += TARGET_NAMES[uint8_t(m.target)];
      out += '"';
    }
    out += m.disabled ? ",\"disabled\":true" : ",\"disabled\":false";
    if (m.disabled_source) out += ",\"disabledSource\":\"\"";
    out += '}';
  }
  out += ']';
  if (r.maybe_disabled) out += ",\"maybeDisabled\":true";
  if (r.maybe_locked) out += ",\"maybeLocked\":true";
  if (r.trapped) out += ",\"trapped\":true";
  if (r.maybe_trapped) out += ",\"maybeTrapped\":true";
  out += "}],";
  side_and_tail(true);
}
inline std::string request_json(const Battle& b, int side, const Names& names) {
  std::string out;
  out.reserve(2048);
  append_request_json(out, b, side, names);
  return out;
}

} // namespace adv::showdown
