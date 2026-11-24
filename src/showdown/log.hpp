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

// getHealth (pokemon.ts:2082) from what a part kept: the Pokémon's own side
// sees its HP, the other a percentage rounded up, and 99 when not full.
inline std::string health_text(const Part& p, bool secret, bool report_percentages) {
  if (!p.hp) return "0 fnt";
  std::string out;
  if (secret) {
    out = std::to_string(p.hp) + "/" + std::to_string(p.maxhp);
  } else if (report_percentages) {
    int percentage = (100 * p.hp + p.maxhp - 1) / p.maxhp;
    if (percentage == 100 && p.hp < p.maxhp) percentage = 99;
    out = std::to_string(percentage) + "/100";
  } else {
    out = std::to_string(std::max(48 * p.hp / p.maxhp, 1)) + "/48";
  }
  if (p.status != Condition::None) out += " " + std::string(CONDITION_NAMES[uint8_t(p.status)].id);
  return out;
}

inline std::string part_text(const Part& p, bool secret, const Battle& b, const Names& names) {
  switch (p.kind) {
  case Part::Kind::Text: return std::string(p.text);
  case Part::Kind::Number: return std::to_string(p.number);
  case Part::Kind::Side: return side_id(p.number);
  case Part::Kind::Player: return names.players[size_t(p.number)];
  case Part::Kind::TurnsLeft:
    return "You will auto-tie if the battle doesn't end in " + (p.number == 1 ? std::string("1 turn") : std::to_string(p.number) + " turns") +
           " (on turn 1000).";
  case Part::Kind::PlayerSays: return names.players[size_t(p.number)] + std::string(p.text);
  case Part::Kind::SideName: return side_id(p.number) + ": " + names.players[size_t(p.number)];
  case Part::Kind::Pokemon:
    return side_id(p.pokemon.side) + (p.active ? "a: " : ": ") + names.teams[p.pokemon.side].names[p.pokemon.index];
  case Part::Kind::Ability: return std::string(ABILITY_NAMES[p.id].name);
  case Part::Kind::Move: return std::string(MOVE_NAMES[p.id].name);
  case Part::Kind::HiddenPower: return "Hidden Power " + std::string(TYPE_NAMES[p.id].name);
  case Part::Kind::FromMove: return "[from] " + std::string(MOVE_NAMES[p.id].name);
  case Part::Kind::Item: return std::string(ITEM_NAMES[p.id].name);
  case Part::Kind::Wisher: return "[wisher] " + names.teams[p.pokemon.side].names[p.pokemon.index];
  case Part::Kind::Name:
    switch (p.effect_kind) {
    case EffectKind::Ability: return std::string(ABILITY_NAMES[p.id].name);
    case EffectKind::Item: return std::string(ITEM_NAMES[p.id].name);
    case EffectKind::Move: return std::string(MOVE_NAMES[p.id].name);
    case EffectKind::Condition: return std::string(CONDITION_NAMES[p.id].name);
    default: break;
    }
    return "?";
  case Part::Kind::From:
  case Part::Kind::Fullname: {
    // An effect's fullname. Damage from `tox` is written as `psn`.
    const std::string lead = p.kind == Part::Kind::From ? "[from] " : "";
    switch (p.effect_kind) {
    case EffectKind::Ability: return lead + "ability: " + std::string(ABILITY_NAMES[p.id].name);
    case EffectKind::Item: return lead + "item: " + std::string(ITEM_NAMES[p.id].name);
    case EffectKind::Move: return lead + "move: " + std::string(MOVE_NAMES[p.id].name);
    case EffectKind::Condition: return lead + std::string(CONDITION_NAMES[Condition(p.id) == Condition::Tox ? uint16_t(Condition::Psn) : p.id].name);
    case EffectKind::Special:
      return lead + (Special(p.id) == Special::Recoil ? "Recoil" : Special(p.id) == Special::Drain ? "drain" : "confusion");
    case EffectKind::Rule: break;
    }
    return lead + "?";
  }
  case Part::Kind::Of:
    return "[of] " + side_id(p.pokemon.side) + (p.active ? "a: " : ": ") + names.teams[p.pokemon.side].names[p.pokemon.index];
  case Part::Kind::Health: return health_text(p, secret, b.report_percentages);
  case Part::Kind::Details: {
    std::string out(SPECIES_NAMES[uint16_t(p.species)].name);
    if (p.level != 100) out += ", L" + std::to_string(p.level);
    if (p.gender == Gender::Male) out += ", M";
    if (p.gender == Gender::Female) out += ", F";
    if (p.shiny) out += ", shiny";
    return out + "|" + health_text(p, secret, b.report_percentages);
  }
  }
  return "";
}

// Battle.add (battle.ts) and addSplit: the lines one Line becomes.
inline void write(const Line& line, const Battle& b, const Names& names, std::vector<std::string>& out) {
  int split = line.secret_side;
  for (int i = 0; i < line.n; i++) {
    const Part& p = line.parts[i];
    if (p.kind == Part::Kind::Details || p.kind == Part::Kind::Health) split = p.pokemon.side;
  }
  const auto text = [&](bool secret) {
    std::string t;
    for (int i = 0; i < line.n; i++) t += "|" + part_text(line.parts[i], secret, b, names);
    return t;
  };
  if (split < 0) return out.push_back(text(true));
  out.push_back("|split|" + side_id(split));
  out.push_back(text(true));
  out.push_back(line.secret_side >= 0 ? "" : text(false));
}

// A request's JSON (Battle.getRequests, battle.ts:1432).
inline std::string request_json(const Battle& b, int side, const Names& names) {
  const Request& r = b.requests[side];
  const std::string side_json = "\"side\":" + side_request(b, side, names.players[side], names.teams[side]);
  if (r.kind == Request::Kind::Wait) return "{\"wait\":true," + side_json + "}";
  const std::string tail = std::string(r.no_cancel ? ",\"noCancel\":true" : "") + (r.update ? ",\"update\":true" : "") + "}";
  if (r.kind == Request::Kind::Switch) return "{\"forceSwitch\":[true]," + side_json + tail;
  const Pokemon& p = b.sides[side].pokemon[b.sides[side].active];
  std::string out = "{\"active\":[{\"moves\":[";
  for (int i = 0; i < r.n_moves; i++) {
    const RequestMove& m = r.moves[i];
    if (r.locked) {
      // Pokemon.getMoves(lockedMove): the name and the id.
      out += m.id == Move::None ? std::string("{\"move\":\"Recharge\",\"id\":\"recharge\"}")
                                : "{\"move\":" + json(MOVE_NAMES[uint16_t(m.id)].name) + ",\"id\":" + json(MOVE_NAMES[uint16_t(m.id)].id) + "}";
      continue;
    }
    // Pokemon.getMoves (pokemon.ts:985): Hidden Power is named with its type
    // and power, Return and Frustration with their power.
    std::string name(MOVE_NAMES[uint16_t(m.id)].name);
    if (m.id == Move::HiddenPower) name += " " + std::string(TYPE_NAMES[uint8_t(p.hp_type)].name) + " " + std::to_string(p.hp_power);
    if (m.id == Move::Return) name += " " + std::to_string(std::max(p.happiness * 10 / 25, 1));
    if (m.id == Move::Frustration) name += " " + std::to_string(std::max((255 - p.happiness) * 10 / 25, 1));
    out += std::string(i ? "," : "") + "{\"move\":" + json(name) + ",\"id\":" + json(MOVE_NAMES[uint16_t(m.id)].id) +
           (r.struggle ? std::string() : ",\"pp\":" + std::to_string(m.pp) + ",\"maxpp\":" + std::to_string(m.maxpp)) +
           (m.no_target ? std::string() : ",\"target\":" + json(TARGET_NAMES[uint8_t(m.target)])) + ",\"disabled\":" +
           (m.disabled ? "true" : "false") +
           (m.disabled_source ? ",\"disabledSource\":\"\"" : "") + "}";
  }
  out += "]";
  if (r.maybe_disabled) out += ",\"maybeDisabled\":true";
  if (r.maybe_locked) out += ",\"maybeLocked\":true";
  if (r.trapped) out += ",\"trapped\":true";
  if (r.maybe_trapped) out += ",\"maybeTrapped\":true";
  return out + "}]," + side_json + tail;
}

} // namespace adv::showdown
