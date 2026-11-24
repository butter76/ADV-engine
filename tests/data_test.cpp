// The generated tables (src/data.hpp) written one fact per line, sorted, and
// compared with the same facts read straight from Showdown
// (tools/tables_ref.cjs, kept as tests/fixtures/tables.txt).
//
// Usage: data_test FIXTURE

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "data.hpp"

using namespace adv;

namespace {

std::string str(std::string_view s) { return std::string(s); }
std::string num(long long v) { return std::to_string(v); }
std::string bit(bool b) { return b ? "1" : "0"; }
template <typename E> std::string id(const Name* names, E e) {
  return uint16_t(e) == 0 ? "-" : str(names[uint16_t(e)].id);
}
std::string boosts(const Boosts& b) {
  return num(b.atk) + "," + num(b.def) + "," + num(b.spa) + "," + num(b.spd) + "," + num(b.spe) + "," +
         num(b.accuracy) + "," + num(b.evasion);
}
std::string frac(Fraction f) { return f ? num(f.num) + "/" + num(f.den) : "-"; }
std::string join(const std::vector<std::string>& parts, char sep = ' ') {
  std::string out;
  for (const std::string& p : parts) {
    if (!out.empty()) out += sep;
    out += p;
  }
  return out;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: data_test FIXTURE\n");
    return 2;
  }
  std::vector<std::string> lines;
  static constexpr const char* STAT[] = {"hp", "atk", "def", "spa", "spd", "spe"};

  for (int i = 1; i < SPECIESS; i++) {
    const SpeciesData& s = SPECIES_DATA[i];
    static constexpr const char* GENDER[] = {"-", "M", "F", "N"};
    lines.push_back(join({"species", str(SPECIES_NAMES[i].id), num(s.num), id(TYPE_NAMES, s.types[0]),
                          id(TYPE_NAMES, s.types[1]), num(s.base[0]), num(s.base[1]), num(s.base[2]), num(s.base[3]),
                          num(s.base[4]), num(s.base[5]), id(ABILITY_NAMES, s.abilities[0]),
                          id(ABILITY_NAMES, s.abilities[1]), num(s.weight_hg), num(s.max_hp), GENDER[uint8_t(s.gender)],
                          num(s.female_eighths), id(SPECIES_NAMES, s.base_species)}));
  }

  for (int i = 1; i < MOVES; i++) {
    const MoveData& m = MOVE_DATA[i];
    static constexpr const char* CATEGORY[] = {"Physical", "Special", "Status"};
    static constexpr const char* DESTRUCT[] = {"-", "ifHit", "always"};
    static constexpr const char* SWITCH[] = {"-", "copyvolatile"};
    std::vector<std::string> flags;
    for (int f = 0; f < MOVE_FLAGS; f++) {
      if (m.flags >> f & 1) flags.push_back(str(MOVE_FLAG_NAMES[f]));
    }
    lines.push_back(join(
        {"move", str(MOVE_NAMES[i].id), str(MOVE_NAMES[m.real_move == Move::None ? i : int(m.real_move)].id), num(m.num),
         id(TYPE_NAMES, m.type), CATEGORY[uint8_t(m.category)],
         num(m.base_power), m.accuracy ? num(m.accuracy) : "true", num(m.pp), num(m.priority),
         str(TARGET_NAMES[uint8_t(m.target)]), num(m.crit_ratio), flags.empty() ? "-" : join(flags, ','),
         m.damage_level ? "level" : m.damage ? num(m.damage) : "-", frac(m.drain), frac(m.recoil), frac(m.heal),
         m.multihit[1] ? num(m.multihit[0]) + "-" + num(m.multihit[1]) : "-", DESTRUCT[uint8_t(m.selfdestruct)],
         SWITCH[uint8_t(m.self_switch)],
         bit(m.multiaccuracy) + bit(m.ohko) + bit(m.force_switch) + bit(m.ignore_immunity) + bit(m.no_pp_boosts) +
             bit(m.sleep_usable) + bit(m.stalling_move) + bit(m.calls_move) + bit(m.has_crash_damage),
         id(CONDITION_NAMES, m.status), id(CONDITION_NAMES, m.volatile_status), id(CONDITION_NAMES, m.side_condition),
         id(CONDITION_NAMES, m.slot_condition), id(CONDITION_NAMES, m.weather), boosts(m.boosts),
         num(m.secondary.chance), id(CONDITION_NAMES, m.secondary.status),
         id(CONDITION_NAMES, m.secondary.volatile_status), boosts(m.secondary.boosts),
         boosts(m.secondary.self_boosts), boosts(m.self_boosts), id(CONDITION_NAMES, m.self_volatile_status)}));
  }

  for (int i = 1; i < ITEMS; i++) {
    const ItemData& it = ITEM_DATA[i];
    std::vector<std::string> users;
    for (const Species s : it.users) {
      if (s != Species::None) users.push_back(str(SPECIES_NAMES[uint16_t(s)].id));
    }
    lines.push_back(join({"item", str(ITEM_NAMES[i].id), num(it.num),
                          bit(it.is_berry) + bit(it.is_choice) + bit(it.is_pokeball),
                          users.empty() ? "-" : join(users, ',')}));
  }
  for (int i = 1; i < ABILITYS; i++) {
    lines.push_back(join({"ability", str(ABILITY_NAMES[i].id), num(ABILITY_DATA[i].num),
                          bit(ABILITY_DATA[i].suppress_weather)}));
  }
  for (int i = 1; i < CONDITIONS; i++) {
    const ConditionData& c = CONDITION_DATA[i];
    static constexpr const char* HOLDER[] = {"standalone", "move", "ability"};
    static constexpr const char* KIND[] = {"condition", "status", "weather"};
    lines.push_back(join({"condition", str(CONDITION_NAMES[i].id), HOLDER[uint8_t(c.holder)], KIND[uint8_t(c.kind)],
                          num(c.duration), num(c.counter_max), bit(c.no_copy), bit(c.affects_fainted)}));
  }

  for (int d = 1; d < TYPES; d++) {
    for (int a = 1; a < TYPES; a++) {
      if (TYPE_CHART[d][a]) lines.push_back(join({"chart", str(TYPE_NAMES[d].id), str(TYPE_NAMES[a].id), num(TYPE_CHART[d][a])}));
    }
    for (int k = 0; k < IMMUNITIES; k++) {
      if (immune(Type(d), Immunity(k))) lines.push_back(join({"immune", str(TYPE_NAMES[d].id), str(IMMUNITY_NAMES[k]), "3"}));
    }
  }
  for (int i = 1; i < NATURES; i++) {
    const NatureData& n = NATURE_DATA[i];
    const bool neutral = n.plus == Stat::Hp;
    lines.push_back(join({"nature", str(NATURE_NAMES[i].id), neutral ? "-" : STAT[uint8_t(n.plus)],
                          neutral ? "-" : STAT[uint8_t(n.minus)]}));
  }

  for (const Handler& h : HANDLERS) {
    static constexpr const char* KIND[] = {"move", "ability", "item", "condition", "rule"};
    const Name* names[] = {MOVE_NAMES, ABILITY_NAMES, ITEM_NAMES, CONDITION_NAMES, RULE_NAMES};
    lines.push_back(join({h.handled ? "handler" : "rank", KIND[uint8_t(h.kind)], str(names[uint8_t(h.kind)][h.id].id),
                          str(CALLBACK_NAMES[uint8_t(h.callback)]), num(h.order), num(h.priority), num(h.sub_order)}));
  }

  std::sort(lines.begin(), lines.end());
  std::ifstream fixture(argv[1]);
  if (!fixture) {
    std::fprintf(stderr, "cannot open %s\n", argv[1]);
    return 2;
  }
  std::string want;
  size_t at = 0, differences = 0;
  while (std::getline(fixture, want)) {
    const std::string have = at < lines.size() ? lines[at] : "<nothing>";
    if (have != want && differences++ < 10) {
      std::fprintf(stderr, "line %zu\n  Showdown: %s\n  tables:   %s\n", at + 1, want.c_str(), have.c_str());
    }
    at++;
  }
  if (at != lines.size()) {
    std::fprintf(stderr, "Showdown has %zu facts, the tables %zu\n", at, lines.size());
    differences++;
  }
  std::printf("%zu facts, %zu differences\n", lines.size(), differences);
  return differences ? 1 : 0;
}
