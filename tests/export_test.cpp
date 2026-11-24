// Teams in Showdown's export text, read by src/showdown/export.hpp, against
// what Showdown packs of the same text (tools/teams_ref.cjs), read by
// `unpack`: the same sets, names and balls.
//
// Usage: export_test TEAMS PACKED

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#include "showdown/export.hpp"

using namespace adv;

namespace {

std::string differs(const Set& a, const Set& b) {
  if (a.species != b.species) return "species";
  if (a.level != b.level) return "level";
  if (a.gender != b.gender) return "gender";
  if (a.happiness != b.happiness) return "happiness";
  if (a.item != b.item) return "item";
  if (a.ability != b.ability) return "ability";
  if (a.moves != b.moves) return "moves";
  if (a.nature != b.nature) return "nature";
  if (a.evs != b.evs) return "EVs";
  if (a.ivs != b.ivs) return "IVs";
  if (a.hp_type != b.hp_type) return "Hidden Power type";
  if (a.shiny != b.shiny) return "shiny";
  return "";
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::fprintf(stderr, "usage: export_test TEAMS PACKED\n");
    return 2;
  }
  std::ifstream teams_file(argv[1]), packed_file(argv[2]);
  if (!teams_file || !packed_file) {
    std::fprintf(stderr, "cannot open %s or %s\n", argv[1], argv[2]);
    return 2;
  }
  const std::string text{std::istreambuf_iterator<char>(teams_file), std::istreambuf_iterator<char>()};
  const std::vector<showdown::Exported> exported = showdown::parse_exports(text);
  size_t at = 0, failures = 0, sets = 0, refused = 0;
  std::string line;
  for (; std::getline(packed_file, line); at++) {
    if (at >= exported.size()) break;
    const showdown::Exported& mine = exported[at];
    showdown::TeamText names;
    std::string error;
    const auto want = showdown::unpack(line, names, error);
    if (!want || !mine.team) {
      // What one cannot hold the other must not either.
      if (bool(want) != bool(mine.team)) {
        std::fprintf(stderr, "team %zu (%s): %s\n", at, mine.name.c_str(),
                     want ? ("refused here: " + mine.error).c_str() : ("refused packed: " + error).c_str());
        failures++;
      }
      refused++;
      continue;
    }
    if (want->size != mine.team->size) {
      std::fprintf(stderr, "team %zu (%s): %d Pokémon, packed %d\n", at, mine.name.c_str(), mine.team->size, want->size);
      failures++;
      continue;
    }
    for (int i = 0; i < want->size; i++, sets++) {
      std::string what = differs(mine.team->sets[size_t(i)], want->sets[size_t(i)]);
      if (what.empty() && mine.text.names[size_t(i)] != names.names[size_t(i)]) what = "name";
      if (what.empty() && mine.text.pokeballs[size_t(i)] != names.pokeballs[size_t(i)]) what = "ball";
      if (what.empty()) continue;
      std::fprintf(stderr, "team %zu (%s), Pokémon %d: the %s differs\n", at, mine.name.c_str(), i + 1, what.c_str());
      failures++;
    }
  }
  if (at != exported.size() || std::getline(packed_file, line)) {
    std::fprintf(stderr, "%zu teams read, and not as many packed\n", exported.size());
    failures++;
  }
  std::printf("%zu teams, %zu sets equal, %zu teams neither holds, %zu failures\n", at, sets, refused, failures);
  return failures || !sets ? 1 : 0;
}
