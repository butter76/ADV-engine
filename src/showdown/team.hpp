// Showdown's packed team text to a Team (Teams.unpack, teams.ts:215), and the
// text of a team that the battle's state does not hold: nicknames and balls.
// The engine never reads text; this is for whatever feeds it Showdown's.

#pragma once

#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "../battle.hpp"

namespace adv::showdown {

// What the formatter needs of a team besides its sets.
struct TeamText {
  std::array<std::string, TEAM_SIZE> names;     // Pokemon.name
  std::array<std::string, TEAM_SIZE> pokeballs; // Pokemon.pokeball, an id
};

// Showdown's toID: lower case, letters and digits only.
inline std::string to_id(std::string_view text) {
  std::string id;
  for (const char c : text) {
    if (c >= 'A' && c <= 'Z') id += char(c - 'A' + 'a');
    else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) id += c;
  }
  return id;
}

// The entry of a names table with this id, or 0.
template <size_t N> int find(const Name (&names)[N], std::string_view id) {
  for (size_t i = 1; i < N; i++) {
    if (names[i].id == id) return int(i);
  }
  return 0;
}

inline std::vector<std::string_view> split(std::string_view text, char by) {
  std::vector<std::string_view> parts;
  for (;;) {
    const size_t at = text.find(by);
    parts.push_back(text.substr(0, at));
    if (at == std::string_view::npos) return parts;
    text.remove_prefix(at + 1);
  }
}

// JavaScript's `Number(text) || 0` for the whole numbers a team holds.
inline int number(std::string_view text) {
  int value = 0;
  const auto r = std::from_chars(text.data(), text.data() + text.size(), value);
  return r.ec == std::errc() && r.ptr == text.data() + text.size() ? value : 0;
}

// JavaScript's `text.substr(0, units)`: a prefix of that many UTF-16 units.
inline std::string_view utf16_prefix(std::string_view text, int units) {
  size_t at = 0;
  while (at < text.size()) {
    const unsigned char c = static_cast<unsigned char>(text[at]);
    const size_t bytes = c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    const int cost = bytes == 4 ? 2 : 1;
    if (cost > units) break; // half of a pair is not kept, where JavaScript keeps it
    units -= cost;
    at += bytes;
  }
  return text.substr(0, at);
}

// One team. On text the engine cannot hold, `error` says what and nothing is
// returned: an unknown name, a level over MAX_LEVEL, more than four moves, or
// an ability given as a slot ('0', '1'), which Showdown resolves in its
// current generation's table.
inline std::optional<Team> unpack(std::string_view packed, TeamText& text, std::string& error) {
  Team team;
  for (const std::string_view entry : split(packed, ']')) {
    if (team.size == TEAM_SIZE) return error = "more than six Pokémon", std::nullopt;
    const std::vector<std::string_view> f = split(entry, '|');
    if (f.size() != 12) return error = "a set has " + std::to_string(f.size()) + " fields", std::nullopt;
    Set set;
    const auto fail = [&](std::string_view what, std::string_view value) {
      error = std::string(what) + " \"" + std::string(value) + "\"";
      return std::nullopt;
    };

    // The species is its own field, or the name when that field is empty.
    const std::string_view species_text = f[1].empty() ? f[0] : f[1];
    set.species = Species(find(SPECIES_NAMES, to_id(species_text)));
    if (set.species == Species::None) return fail("unknown species", species_text);
    if (!f[2].empty() && (set.item = Item(find(ITEM_NAMES, to_id(f[2])))) == Item::None) return fail("unknown item", f[2]);
    if (f[3].empty() || f[3] == "0" || f[3] == "1" || f[3] == "H" || f[3] == "S") return fail("ability given as a slot", f[3]);
    if ((set.ability = Ability(find(ABILITY_NAMES, to_id(f[3])))) == Ability::None) return fail("unknown ability", f[3]);

    const std::vector<std::string_view> moves = split(f[4], ',');
    if (moves.size() > MOVE_SLOTS) return fail("more than four moves", f[4]);
    for (size_t i = 0; i < moves.size(); i++) {
      if ((set.moves[i] = Move(find(MOVE_NAMES, to_id(moves[i])))) == Move::None) return fail("unknown move", moves[i]);
    }
    if (!f[5].empty() && (set.nature = Nature(find(NATURE_NAMES, to_id(f[5])))) == Nature::None) return fail("unknown nature", f[5]);

    // EVs default to 0 and IVs to 31; an IV field with fewer than six entries
    // leaves the rest 0, as `Number(undefined) || 0` does.
    if (!f[6].empty()) {
      const std::vector<std::string_view> evs = split(f[6], ',');
      for (size_t i = 0; i < 6; i++) set.evs[i] = uint8_t(std::clamp(i < evs.size() ? number(evs[i]) : 0, 0, 255));
    }
    if (f[7] == "M") set.gender = Gender::Male;
    if (f[7] == "F") set.gender = Gender::Female;
    if (f[7] == "N") set.gender = Gender::None;
    if (!f[8].empty()) {
      const std::vector<std::string_view> ivs = split(f[8], ',');
      for (size_t i = 0; i < 6; i++) {
        const int iv = i >= ivs.size() ? 0 : ivs[i].empty() ? 31 : number(ivs[i]);
        set.ivs[i] = uint8_t(std::clamp(iv, 0, 31));
      }
    }
    set.shiny = !f[9].empty();
    if (!f[10].empty()) {
      const int level = number(f[10]);
      if (level > MAX_LEVEL) return fail("level over 100", f[10]);
      set.level = uint8_t(level >= 1 ? level : 100);
    }
    std::string pokeball = "pokeball";
    if (!f[11].empty()) {
      const std::vector<std::string_view> misc = split(f[11], ',');
      if (!misc[0].empty()) set.happiness = uint8_t(std::clamp(number(misc[0]), 0, 255));
      if (misc.size() > 1 && !misc[1].empty()) {
        if ((set.hp_type = Type(find(TYPE_NAMES, to_id(misc[1])))) == Type::None) return fail("unknown Hidden Power type", misc[1]);
      }
      if (misc.size() > 2 && !to_id(misc[2]).empty()) pokeball = to_id(misc[2]);
    }

    // Pokemon's constructor (pokemon.ts:330): a set with no name of its own
    // is named for its base species, and a name is cut to 20 units.
    const SpeciesData& s = data(set.species);
    std::string_view name = f[0];
    if (f[1].empty() || name.empty() || name == SPECIES_NAMES[uint16_t(set.species)].name) {
      name = SPECIES_NAMES[uint16_t(s.base_species)].name;
    }
    text.names[team.size] = std::string(utf16_prefix(name, 20));
    text.pokeballs[team.size] = pokeball;
    team.sets[team.size++] = set;
  }
  return team;
}

} // namespace adv::showdown
