// The `|request|` JSON a player is sent, byte for byte as Showdown's
// JSON.stringify writes it. So far: the `side` object
// (Side.getRequestData, side.ts:357).

#pragma once

#include <string>

#include "team.hpp"

namespace adv::showdown {

// Everything here is appended to one string: a request is some 1,500 bytes,
// written twice a turn, and was most of the time of writing a battle as text
// when each piece was a string of its own.

// A JSON string as JSON.stringify writes one: quotes, backslashes and control
// characters escaped, everything else as it is.
inline void append_json(std::string& out, std::string_view text) {
  out += '"';
  // Nearly every string needs no escaping.
  bool plain = true;
  for (const char c : text) plain &= c != '"' && c != '\\' && static_cast<unsigned char>(c) >= 0x20;
  if (plain) {
    out += text;
    out += '"';
    return;
  }
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
  out += '"';
}
inline std::string json(std::string_view text) {
  std::string out;
  append_json(out, text);
  return out;
}
// A whole number that is not negative, in decimal.
inline void append_number(std::string& out, unsigned value) {
  char digits[10];
  int n = 0;
  do digits[n++] = char('0' + value % 10);
  while (value /= 10);
  while (n) out += digits[--n];
}

// Pokemon.details (getUpdatedDetails, pokemon.ts:557): set once, from the
// species the Pokémon came as.
inline void append_details(std::string& out, const Pokemon& p) {
  out += SPECIES_NAMES[uint16_t(p.base_species)].name;
  if (p.level != 100) {
    out += ", L";
    append_number(out, p.level);
  }
  if (p.gender == Gender::Male) out += ", M";
  if (p.gender == Gender::Female) out += ", F";
  if (p.set.shiny) out += ", shiny";
}
inline std::string details(const Pokemon& p) {
  std::string out;
  append_details(out, p);
  return out;
}

// getHealth().secret (pokemon.ts:2082): what the Pokémon's own side is shown.
inline void append_secret_health(std::string& out, const Pokemon& p) {
  if (!p.hp) {
    out += "0 fnt";
    return;
  }
  append_number(out, p.hp);
  out += '/';
  append_number(out, p.maxhp);
  if (p.status != Condition::None) {
    out += ' ';
    out += CONDITION_NAMES[uint8_t(p.status)].id;
  }
}

// A move's id as getSwitchRequestData (pokemon.ts:1174) lists it: Hidden
// Power with its type, Return and Frustration with their power.
inline void append_request_move(std::string& out, const Pokemon& p, Move move) {
  out += MOVE_NAMES[uint16_t(move)].id;
  if (move == Move::HiddenPower) out += TYPE_NAMES[uint8_t(p.hp_type)].id;
  if (move == Move::Return) append_number(out, unsigned(std::max(p.happiness * 10 / 25, 1)));
  if (move == Move::Frustration) append_number(out, unsigned(std::max((255 - p.happiness) * 10 / 25, 1)));
}

// Side.getRequestData: the side's Pokémon in their current order. None of
// what is written between quotes here needs escaping but the names, which
// are the player's.
inline void append_side_request(std::string& out, const Battle& b, int side, std::string_view player, const TeamText& text) {
  const Side& s = b.sides[side];
  const std::string_view id = side == 0 ? "p1" : "p2";
  out += "{\"name\":";
  append_json(out, player);
  out += ",\"id\":\"";
  out += id;
  out += "\",\"pokemon\":[";
  for (int position = 0; position < s.size; position++) {
    // The Pokémon at this position, and its index in the team as given.
    int i = 0;
    while (s.pokemon[i].position != position) i++;
    const Pokemon& p = s.pokemon[i];
    if (position) out += ',';
    out += "{\"ident\":\"";
    out += id;
    out += ": ";
    // The name, escaped as the whole string would be.
    const size_t quote = out.size();
    append_json(out, text.names[i]);
    out.erase(quote, 1); // the opening quote: the string began before the name
    out += ",\"details\":\"";
    append_details(out, p);
    out += "\",\"condition\":\"";
    append_secret_health(out, p);
    out += position < 1 ? "\",\"active\":true" : "\",\"active\":false";
    out += ",\"stats\":{\"atk\":";
    append_number(out, p.base_stored_stats.atk);
    out += ",\"def\":";
    append_number(out, p.base_stored_stats.def);
    out += ",\"spa\":";
    append_number(out, p.base_stored_stats.spa);
    out += ",\"spd\":";
    append_number(out, p.base_stored_stats.spd);
    out += ",\"spe\":";
    append_number(out, p.base_stored_stats.spe);
    out += "},\"moves\":[";
    for (int m = 0; m < p.moves; m++) {
      out += m ? ",\"" : "\"";
      append_request_move(out, p, p.move_slots[m].id);
      out += '"';
    }
    out += "],\"baseAbility\":\"";
    out += ABILITY_NAMES[uint8_t(p.base_ability)].id;
    out += "\",\"item\":\"";
    out += ITEM_NAMES[uint8_t(p.item)].id;
    out += "\",\"pokeball\":";
    append_json(out, text.pokeballs[i]);
    out += '}';
  }
  out += "]}";
}

} // namespace adv::showdown
