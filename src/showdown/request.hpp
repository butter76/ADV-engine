// The `|request|` JSON a player is sent, byte for byte as Showdown's
// JSON.stringify writes it. So far: the `side` object
// (Side.getRequestData, side.ts:357).

#pragma once

#include <string>

#include "team.hpp"

namespace adv::showdown {

// A JSON string as JSON.stringify writes one: quotes, backslashes and control
// characters escaped, everything else as it is.
inline std::string json(std::string_view text) {
  std::string out = "\"";
  for (const char c : text) {
    switch (c) {
    case '"': out += "\\\""; break;
    case '\\': out += "\\\\"; break;
    case '\n': out += "\\n"; break;
    case '\r': out += "\\r"; break;
    case '\t': out += "\\t"; break;
    case '\b': out += "\\b"; break;
    case '\f': out += "\\f"; break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        static constexpr char HEX[] = "0123456789abcdef";
        out += "\\u00";
        out += HEX[c >> 4];
        out += HEX[c & 15];
      } else {
        out += c;
      }
    }
  }
  return out + "\"";
}

// Pokemon.details (getUpdatedDetails, pokemon.ts:557): set once, from the
// species the Pokémon came as.
inline std::string details(const Pokemon& p) {
  std::string out(SPECIES_NAMES[uint16_t(p.base_species)].name);
  if (p.level != 100) out += ", L" + std::to_string(p.level);
  if (p.gender == Gender::Male) out += ", M";
  if (p.gender == Gender::Female) out += ", F";
  if (p.set.shiny) out += ", shiny";
  return out;
}

// getHealth().secret (pokemon.ts:2082): what the Pokémon's own side is shown.
inline std::string secret_health(const Pokemon& p) {
  if (!p.hp) return "0 fnt";
  std::string out = std::to_string(p.hp) + "/" + std::to_string(p.maxhp);
  if (p.status != Condition::None) out += " " + std::string(CONDITION_NAMES[uint8_t(p.status)].id);
  return out;
}

// A move's id as getSwitchRequestData (pokemon.ts:1174) lists it: Hidden
// Power with its type, Return and Frustration with their power.
inline std::string request_move(const Pokemon& p, Move move) {
  std::string id(MOVE_NAMES[uint16_t(move)].id);
  if (move == Move::HiddenPower) return id + std::string(TYPE_NAMES[uint8_t(p.hp_type)].id);
  if (move == Move::Return) return id + std::to_string(std::max(p.happiness * 10 / 25, 1));
  if (move == Move::Frustration) return id + std::to_string(std::max((255 - p.happiness) * 10 / 25, 1));
  return id;
}

// Side.getRequestData: the side's Pokémon in their current order.
inline std::string side_request(const Battle& b, int side, std::string_view player, const TeamText& text) {
  const Side& s = b.sides[side];
  const std::string id = side == 0 ? "p1" : "p2";
  std::string out = "{\"name\":" + json(player) + ",\"id\":\"" + id + "\",\"pokemon\":[";
  for (int position = 0; position < s.size; position++) {
    // The Pokémon at this position, and its index in the team as given.
    int i = 0;
    while (s.pokemon[i].position != position) i++;
    const Pokemon& p = s.pokemon[i];
    if (position) out += ",";
    out += "{\"ident\":" + json(id + ": " + text.names[i]) + ",\"details\":" + json(details(p)) +
           ",\"condition\":" + json(secret_health(p)) + ",\"active\":" + (position < 1 ? "true" : "false") +
           ",\"stats\":{\"atk\":" + std::to_string(p.base_stored_stats.atk) +
           ",\"def\":" + std::to_string(p.base_stored_stats.def) +
           ",\"spa\":" + std::to_string(p.base_stored_stats.spa) +
           ",\"spd\":" + std::to_string(p.base_stored_stats.spd) +
           ",\"spe\":" + std::to_string(p.base_stored_stats.spe) + "},\"moves\":[";
    for (int m = 0; m < p.moves; m++) out += (m ? "," : "") + json(request_move(p, p.move_slots[m].id));
    out += "],\"baseAbility\":" + json(ABILITY_NAMES[uint8_t(p.base_ability)].id) +
           ",\"item\":" + json(ITEM_NAMES[uint8_t(p.item)].id) + ",\"pokeball\":" + json(text.pokeballs[i]) + "}";
  }
  return out + "]}";
}

} // namespace adv::showdown
