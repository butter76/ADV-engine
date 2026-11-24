// Teams in Showdown's export text (Teams.import, sim/teams.ts:560), the form
// people write and share them in, and a file of them: one team under each
// `=== [format] name ===` line.
//
// A team read here is the team `unpack` (team.hpp) reads from what Showdown
// packs of the same text; tests/export_test.cpp holds the two equal.

#pragma once

#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "team.hpp"

namespace adv::showdown {

namespace detail {

inline std::string_view trim(std::string_view text) {
  // JavaScript's trim: white space and line ends.
  const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f'; };
  while (!text.empty() && space(text.front())) text.remove_prefix(1);
  while (!text.empty() && space(text.back())) text.remove_suffix(1);
  return text;
}

// parseInt: the digits a text begins with, after spaces and a sign. No digits
// is NaN, which is `missing`.
inline std::optional<long> parse_int(std::string_view text) {
  text = trim(text);
  bool negative = false;
  if (!text.empty() && (text[0] == '+' || text[0] == '-')) {
    negative = text[0] == '-';
    text.remove_prefix(1);
  }
  if (text.empty() || text[0] < '0' || text[0] > '9') return std::nullopt;
  long value = 0;
  for (const char c : text) {
    if (c < '0' || c > '9') break;
    value = std::min(value * 10 + (c - '0'), 1000000L);
  }
  return negative ? -value : value;
}

// Dex.stats.getID: a stat by its id or one of the names people write.
inline int stat_index(std::string_view name) {
  const std::string id = to_id(name);
  static constexpr std::pair<std::string_view, int> names[] = {
      {"hp", 0},  {"hitpoints", 0}, {"atk", 1}, {"attack", 1},  {"def", 2},      {"defense", 2},
      {"spa", 3}, {"spatk", 3},     {"spattack", 3}, {"specialattack", 3}, {"spc", 3}, {"special", 3}, {"satk", 3},
      {"spd", 4}, {"spdef", 4},     {"spdefense", 4}, {"specialdefense", 4}, {"sdef", 4},
      {"spe", 5}, {"speed", 5}};
  for (const auto& [text, index] : names) {
    if (text == id) return index;
  }
  return -1;
}

} // namespace detail

// One team of a text. On text the engine cannot hold, `error` says what.
inline std::optional<Team> parse_export(std::string_view text, TeamText& names, std::string& error) {
  using detail::trim;
  Team team;
  bool open = false; // a set is being read
  bool happiness_given = false;
  std::string name; // the open set's nickname
  const auto fail = [&](std::string_view what, std::string_view value) {
    error = std::string(what) + " \"" + std::string(value) + "\"";
    return std::nullopt;
  };
  // What unpack does once a set's fields are read.
  const auto close = [&] {
    if (!open) return;
    open = false;
    Set& set = team.sets[team.size - 1];
    const SpeciesData& s = data(set.species);
    std::string_view shown = name;
    if (name.empty() || to_id(name) == SPECIES_NAMES[uint16_t(set.species)].id) {
      shown = SPECIES_NAMES[uint16_t(s.base_species)].name;
    }
    names.names[team.size - 1] = std::string(utf16_prefix(shown, 20));
  };

  for (std::string_view line : split(text, '\n')) {
    line = trim(line);
    if (line.empty() || line == "---") {
      close();
      continue;
    }
    if (line.starts_with("===")) continue;
    if (!open) {
      if (team.size == TEAM_SIZE) return error = "more than six Pokémon", std::nullopt;
      open = true;
      happiness_given = false;
      Set& set = team.sets[team.size];
      set = Set{};
      names.pokeballs[team.size] = "pokeball";
      team.size++;
      // `Name (Species) (M) @ Item`
      if (const size_t at = line.find(" @ "); at != std::string_view::npos) {
        std::string_view item = line.substr(at + 3);
        // `line.split(' @ ')` keeps what is between the first and the second.
        if (const size_t more = item.find(" @ "); more != std::string_view::npos) item = item.substr(0, more);
        line = line.substr(0, at);
        if (!item.empty() && to_id(item) != "noitem") {
          if ((set.item = Item(find(ITEM_NAMES, to_id(item)))) == Item::None) return fail("unknown item", item);
        }
      }
      if (line.ends_with(" (M)")) set.gender = Gender::Male, line.remove_suffix(4);
      if (line.ends_with(" (F)")) set.gender = Gender::Female, line.remove_suffix(4);
      std::string_view species = line;
      name.clear();
      if (line.ends_with(")") && line.find('(') != std::string_view::npos) {
        // `line.slice(0, -1).split('(')`: the first two pieces.
        const std::vector<std::string_view> pieces = split(line.substr(0, line.size() - 1), '(');
        name = std::string(trim(pieces[0]));
        species = pieces[1];
      }
      if ((set.species = Species(find(SPECIES_NAMES, to_id(species)))) == Species::None) return fail("unknown species", species);
      continue;
    }

    Set& set = team.sets[team.size - 1];
    if (line.starts_with("Trait: ") || line.starts_with("Ability: ")) {
      const std::string_view ability = line.substr(line.find(' ') + 1);
      if ((set.ability = Ability(find(ABILITY_NAMES, to_id(ability)))) == Ability::None) return fail("unknown ability", ability);
    } else if (line == "Shiny: Yes") {
      set.shiny = true;
    } else if (line.starts_with("Level: ")) {
      // `+line`, which is NaN for anything that is not a number.
      const std::string_view value = trim(line.substr(7));
      const int level = value.empty() ? 0 : number(value);
      if (level > MAX_LEVEL) return fail("level over 100", value);
      set.level = uint8_t(level >= 1 ? level : 100);
    } else if (line.starts_with("Happiness: ")) {
      const std::string_view value = trim(line.substr(11));
      set.happiness = uint8_t(std::clamp(value.empty() ? 0 : number(value), 0, 255));
      happiness_given = true;
    } else if (line.starts_with("Pokeball: ")) {
      if (const std::string ball = to_id(line.substr(10)); !ball.empty()) names.pokeballs[team.size - 1] = ball;
    } else if (line.starts_with("Hidden Power: ")) {
      const std::string_view type = line.substr(14);
      if ((set.hp_type = Type(find(TYPE_NAMES, to_id(type)))) == Type::None) return fail("unknown Hidden Power type", type);
    } else if (line.starts_with("EVs: ") || line.starts_with("IVs: ")) {
      const bool evs = line[0] == 'E';
      for (std::string_view part : split(line.substr(5), '/')) {
        part = trim(part);
        const size_t space = part.find(' ');
        if (space == std::string_view::npos) continue;
        std::string_view stat = part.substr(space + 1);
        if (const size_t more = stat.find(' '); more != std::string_view::npos) stat = stat.substr(0, more);
        const int index = detail::stat_index(stat);
        if (index < 0) continue;
        const std::optional<long> value = detail::parse_int(part.substr(0, space));
        // A missing IV is 31; a missing EV is NaN, which packs as nothing.
        if (evs) set.evs[size_t(index)] = uint8_t(std::clamp(value.value_or(0), 0L, 255L));
        else set.ivs[size_t(index)] = uint8_t(std::clamp(value.value_or(31), 0L, 31L));
      }
    } else if (const size_t at = std::min(line.find(" Nature"), line.find(" nature"));
               at != std::string_view::npos && at > 0 &&
               std::all_of(line.begin(), line.begin() + long(at), [](char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); })) {
      const std::string_view nature = line.substr(0, at);
      if (nature != "undefined" && (set.nature = Nature(find(NATURE_NAMES, to_id(nature)))) == Nature::None) {
        return fail("unknown nature", nature);
      }
    } else if (line[0] == '-' || line[0] == '~') {
      line.remove_prefix(line.size() > 1 && line[1] == ' ' ? 2 : 1);
      std::string move(line);
      // `Hidden Power [Ice]` is the move Hidden Power Ice. (import gives a set
      // IVs before it reads a line, so the type never sets them.)
      if (line.starts_with("Hidden Power [")) move = "Hidden Power " + std::string(line.substr(14, line.size() - 15));
      if (line == "Frustration" && !happiness_given) set.happiness = 0, happiness_given = true;
      size_t slot = 0;
      while (slot < MOVE_SLOTS && set.moves[slot] != Move::None) slot++;
      if (slot == MOVE_SLOTS) return fail("more than four moves", line);
      if ((set.moves[slot] = Move(find(MOVE_NAMES, to_id(move)))) == Move::None) return fail("unknown move", line);
    }
  }
  close();
  if (team.size == 0) return error = "no Pokémon", std::nullopt;
  for (int i = 0; i < team.size; i++) {
    if (team.sets[size_t(i)].ability == Ability::None) return error = "a Pokémon with no ability", std::nullopt;
  }
  return team;
}

// One team of a file of them.
struct Exported {
  std::string name; // its header line, without the `===`
  std::optional<Team> team;
  TeamText text;
  std::string error; // why there is no team
};

// The teams of a file: what is under each `=== ... ===` line, or the whole
// text as one team if it has no such line.
inline std::vector<Exported> parse_exports(std::string_view text) {
  std::vector<Exported> out;
  std::string_view name;
  size_t begin = 0;
  bool any = false;
  const auto flush = [&](size_t end) {
    const std::string_view body = text.substr(begin, end - begin);
    if (!any && detail::trim(body).empty()) return;
    Exported one;
    one.name = std::string(name);
    one.team = parse_export(body, one.text, one.error);
    out.push_back(std::move(one));
  };
  for (size_t at = 0; at < text.size();) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    const std::string_view line = detail::trim(text.substr(at, end - at));
    if (line.starts_with("===") && line.ends_with("===") && line.size() >= 6) {
      flush(at);
      any = true;
      name = detail::trim(line.substr(3, line.size() - 6));
      begin = std::min(end + 1, text.size());
    }
    at = end + 1;
  }
  flush(text.size());
  return out;
}

} // namespace adv::showdown
