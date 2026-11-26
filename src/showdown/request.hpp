// The `|request|` JSON a player is sent, byte for byte as Showdown's
// JSON.stringify writes it. So far: the `side` object
// (Side.getRequestData, side.ts:357).

#pragma once

#include <algorithm>
#include <cstring>
#include <string>

#include "team.hpp"

namespace adv::showdown {

// Everything here is written into one string's own room, through a Cursor: a
// request is some 1,500 bytes, written twice a turn, in about 250 pieces.
// When each piece was a string of its own that was most of the time of
// writing a battle as text; when each was appended to one string, the test
// for room and the call that each append makes still were.

// Where text is being written, and where its room ends. A piece that does
// not fit is left out, and nothing more is written; what was written is
// then thrown away and written again with more room (append_with).
//
// Two pointers and nothing else, so that a writer is handed one and hands
// it back in registers; the helpers below take it by reference and are put
// in line, so that it stays there. Kept in memory, every piece written read
// the cursor back and stored it again.
struct Cursor {
  char* at;
  char* end; // null once something did not fit
  bool overflowed() const { return end == nullptr; }
  [[gnu::always_inline]] bool room(size_t n) {
    if (size_t(end - at) >= n) return true;
    at = end = nullptr;
    return false;
  }
  // The pieces are short, a name or a few characters of JSON, and there are
  // hundreds to a request: each is copied here, where a piece whose length
  // the compiler knows becomes a store or two, and not by a call. A longer
  // one is copied as two blocks that overlap, both of them within it.
  [[gnu::always_inline]] void put(std::string_view text) {
    const size_t n = text.size();
    if (!room(n)) return;
    const char* from = text.data();
    if (n >= 16) {
      if (n <= 32) {
        std::memcpy(at, from, 16);
        std::memcpy(at + n - 16, from + n - 16, 16);
      } else {
        std::memcpy(at, from, n);
      }
    } else if (n >= 8) {
      std::memcpy(at, from, 8);
      std::memcpy(at + n - 8, from + n - 8, 8);
    } else if (n >= 4) {
      std::memcpy(at, from, 4);
      std::memcpy(at + n - 4, from + n - 4, 4);
    } else {
      for (size_t i = 0; i < n; i++) at[i] = from[i];
    }
    at += n;
  }
  [[gnu::always_inline]] void put(char c) {
    if (room(1)) *at++ = c;
  }
};
// Appends what `write` writes to `out`, which is given `room` more bytes
// first, and as many again for as long as that is not enough. `write` takes
// a Cursor and gives back where it ended; it only reads what it writes from,
// so it can run again.
template <typename Write> void append_with(std::string& out, size_t room, Write&& write) {
  const size_t used = out.size();
  for (room = std::max<size_t>(room, 1);; room *= 2) {
    bool fits = true;
    out.resize_and_overwrite(used + room, [&](char* data, size_t size) {
      const Cursor to = write(Cursor{data + used, data + size});
      fits = !to.overflowed();
      return fits ? size_t(to.at - data) : used;
    });
    if (fits) return;
  }
}

// A JSON string as JSON.stringify writes one: quotes, backslashes and control
// characters escaped, everything else as it is.
// The text and the quote that closes it: what follows a string's opening
// quote, or follows more of the same string.
[[gnu::always_inline]] inline void put_json_rest(Cursor& out, std::string_view text) {
  // Nearly every string needs no escaping.
  bool plain = true;
  for (const char c : text) plain &= c != '"' && c != '\\' && static_cast<unsigned char>(c) >= 0x20;
  if (plain) {
    out.put(text);
    out.put('"');
    return;
  }
  for (const char c : text) {
    switch (c) {
    case '"': out.put("\\\""); break;
    case '\\': out.put("\\\\"); break;
    case '\n': out.put("\\n"); break;
    case '\r': out.put("\\r"); break;
    case '\t': out.put("\\t"); break;
    case '\b': out.put("\\b"); break;
    case '\f': out.put("\\f"); break;
    default:
      if (static_cast<unsigned char>(c) < 0x20) {
        static constexpr char HEX[] = "0123456789abcdef";
        out.put("\\u00");
        out.put(HEX[c >> 4]);
        out.put(HEX[c & 15]);
      } else {
        out.put(c);
      }
    }
  }
  out.put('"');
}
[[gnu::always_inline]] inline void put_json(Cursor& out, std::string_view text) {
  out.put('"');
  put_json_rest(out, text);
}
inline void append_json(std::string& out, std::string_view text) {
  append_with(out, text.size() + 2, [&](Cursor to) {
    put_json(to, text);
    return to;
  });
}
inline std::string json(std::string_view text) {
  std::string out;
  append_json(out, text);
  return out;
}
// A whole number that is not negative, in decimal.
[[gnu::always_inline]] inline void put_number(Cursor& out, unsigned value) {
  char digits[10];
  int n = 0;
  do digits[n++] = char('0' + value % 10);
  while (value /= 10);
  if (!out.room(size_t(n))) return;
  while (n) *out.at++ = digits[--n];
}
inline void append_number(std::string& out, unsigned value) {
  append_with(out, 10, [&](Cursor to) {
    put_number(to, value);
    return to;
  });
}

// Pokemon.details (getUpdatedDetails, pokemon.ts:557): set once, from the
// species the Pokémon came as.
[[gnu::always_inline]] inline void put_details(Cursor& out, const Pokemon& p) {
  out.put(SPECIES_NAMES[uint16_t(p.base_species)].name);
  if (p.level != 100) {
    out.put(", L");
    put_number(out, p.level);
  }
  if (p.gender == Gender::Male) out.put(", M");
  if (p.gender == Gender::Female) out.put(", F");
  if (p.set.shiny) out.put(", shiny");
}
inline void append_details(std::string& out, const Pokemon& p) {
  append_with(out, 64, [&](Cursor to) {
    put_details(to, p);
    return to;
  });
}
inline std::string details(const Pokemon& p) {
  std::string out;
  append_details(out, p);
  return out;
}

// getHealth().secret (pokemon.ts:2082): what the Pokémon's own side is shown.
[[gnu::always_inline]] inline void put_secret_health(Cursor& out, const Pokemon& p) {
  if (!p.hp) {
    out.put("0 fnt");
    return;
  }
  put_number(out, p.hp);
  out.put('/');
  put_number(out, p.maxhp);
  if (p.status != Condition::None) {
    out.put(' ');
    out.put(CONDITION_NAMES[uint8_t(p.status)].id);
  }
}
inline void append_secret_health(std::string& out, const Pokemon& p) {
  append_with(out, 32, [&](Cursor to) {
    put_secret_health(to, p);
    return to;
  });
}

// A move's id as getSwitchRequestData (pokemon.ts:1174) lists it: Hidden
// Power with its type, Return and Frustration with their power.
[[gnu::always_inline]] inline void put_request_move(Cursor& out, const Pokemon& p, Move move) {
  out.put(MOVE_NAMES[uint16_t(move)].id);
  if (move == Move::HiddenPower) out.put(TYPE_NAMES[uint8_t(p.hp_type)].id);
  if (move == Move::Return) put_number(out, unsigned(std::max(p.happiness * 10 / 25, 1)));
  if (move == Move::Frustration) put_number(out, unsigned(std::max((255 - p.happiness) * 10 / 25, 1)));
}
inline void append_request_move(std::string& out, const Pokemon& p, Move move) {
  append_with(out, 48, [&](Cursor to) {
    put_request_move(to, p, move);
    return to;
  });
}

// Side.getRequestData: the side's Pokémon in their current order. None of
// what is written between quotes here needs escaping but the names, which
// are the player's.
[[gnu::always_inline]] inline void put_side_request(Cursor& out, const Battle& b, int side, std::string_view player, const TeamText& text) {
  const Side& s = b.sides[side];
  const std::string_view id = side == 0 ? "p1" : "p2";
  out.put("{\"name\":");
  put_json(out, player);
  out.put(",\"id\":\"");
  out.put(id);
  out.put("\",\"pokemon\":[");
  for (int position = 0; position < s.size; position++) {
    // The Pokémon at this position, and its index in the team as given.
    int i = 0;
    while (s.pokemon[i].position != position) i++;
    const Pokemon& p = s.pokemon[i];
    if (position) out.put(',');
    out.put("{\"ident\":\"");
    out.put(id);
    out.put(": ");
    // The name, escaped as the whole string is: the string began before it.
    put_json_rest(out, text.names[i]);
    out.put(",\"details\":\"");
    put_details(out, p);
    out.put("\",\"condition\":\"");
    put_secret_health(out, p);
    if (position < 1) out.put("\",\"active\":true");
    else out.put("\",\"active\":false");
    out.put(",\"stats\":{\"atk\":");
    put_number(out, p.base_stored_stats.atk);
    out.put(",\"def\":");
    put_number(out, p.base_stored_stats.def);
    out.put(",\"spa\":");
    put_number(out, p.base_stored_stats.spa);
    out.put(",\"spd\":");
    put_number(out, p.base_stored_stats.spd);
    out.put(",\"spe\":");
    put_number(out, p.base_stored_stats.spe);
    out.put("},\"moves\":[");
    for (int m = 0; m < p.moves; m++) {
      if (m) out.put(',');
      out.put('"');
      put_request_move(out, p, p.move_slots[m].id);
      out.put('"');
    }
    out.put("],\"baseAbility\":\"");
    out.put(ABILITY_NAMES[uint8_t(p.base_ability)].id);
    out.put("\",\"item\":\"");
    out.put(ITEM_NAMES[uint8_t(p.item)].id);
    out.put("\",\"pokeball\":");
    put_json(out, text.pokeballs[i]);
    out.put('}');
  }
  out.put("]}");
}
inline void append_side_request(std::string& out, const Battle& b, int side, std::string_view player, const TeamText& text) {
  append_with(out, 1536, [&](Cursor to) {
    put_side_request(to, b, side, player, text);
    return to;
  });
}

} // namespace adv::showdown
