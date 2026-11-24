// Building a side from its team: Side's and Pokemon's constructors
// (side.ts:229, pokemon.ts:330) and what they call.

#pragma once

#include <algorithm>
#include <cassert>

#include "out.hpp"

namespace adv {

// Battle.statModify (battle.ts:2370). `tr(x, 16)` keeps 16 bits, so a stat of
// 596 or more with a boosting nature wraps, as Showdown's does.
constexpr uint16_t stat_modify(const SpeciesData& species, const Set& set, Stat stat) {
  const uint32_t base = species.base[uint8_t(stat)];
  const uint32_t iv = set.ivs[uint8_t(stat)], ev = set.evs[uint8_t(stat)];
  if (stat == Stat::Hp) return uint16_t((2 * base + iv + ev / 4 + 100) * set.level / 100 + 10);
  uint32_t value = (2 * base + iv + ev / 4) * set.level / 100 + 5;
  const NatureData& nature = data(set.nature);
  if (nature.plus != nature.minus) {
    if (nature.plus == stat) value = ((value * 110) & 0xFFFF) / 100;
    if (nature.minus == stat) value = ((value * 90) & 0xFFFF) / 100;
  }
  return uint16_t(value);
}

// Dex.getHiddenPower for Gen 3 and later (dex.ts:324). The stats are read in
// Showdown's order: HP, Atk, Def, Spe, SpA, SpD.
constexpr void hidden_power(const std::array<uint8_t, 6>& ivs, Type& type, uint8_t& power) {
  constexpr Type TYPES_BY_INDEX[16] = {
      Type::Fighting, Type::Flying, Type::Poison, Type::Ground, Type::Rock,     Type::Bug,
      Type::Ghost,    Type::Steel,  Type::Fire,   Type::Water,  Type::Grass,    Type::Electric,
      Type::Psychic,  Type::Ice,    Type::Dragon, Type::Dark,
  };
  constexpr Stat ORDER[6] = {Stat::Hp, Stat::Atk, Stat::Def, Stat::Spe, Stat::Spa, Stat::Spd};
  uint32_t type_x = 0, power_x = 0, bit = 1;
  for (const Stat s : ORDER) {
    const uint32_t iv = ivs[uint8_t(s)];
    type_x += bit * (iv % 2);
    power_x += bit * ((iv / 2) % 2);
    bit *= 2;
  }
  type = TYPES_BY_INDEX[type_x * 15 / 63];
  power = uint8_t(power_x * 40 / 63 + 30);
}

// Pokemon.setSpecies (pokemon.ts:1409) as the constructor reaches it, through
// clearVolatile: the stats of the species, and full HP the first time.
// ModifySpecies, which it runs first, has no listener in Gen 3.
constexpr void set_species(Pokemon& p, Species species) {
  p.species = species;
  const SpeciesData& s = data(species);
  p.types = s.types;
  uint16_t hp = stat_modify(s, p.set, Stat::Hp);
  if (s.max_hp) hp = s.max_hp;
  if (!p.maxhp) p.base_maxhp = p.maxhp = p.hp = hp;
  p.base_stored_stats = {stat_modify(s, p.set, Stat::Atk), stat_modify(s, p.set, Stat::Def),
                         stat_modify(s, p.set, Stat::Spa), stat_modify(s, p.set, Stat::Spd),
                         stat_modify(s, p.set, Stat::Spe)};
  p.stored_stats = p.base_stored_stats;
  p.speed = p.stored_stats.spe;
}

// Pokemon's constructor (pokemon.ts:330).
inline void init_pokemon(Battle& b, Out& out, Pokemon& p, const Set& set) {
  p = Pokemon{};
  p.set = set;
  p.base_species = set.species;
  p.level = set.level;
  assert(p.level >= 1 && p.level <= MAX_LEVEL);
  // The set's gender, else the species' one gender, else a draw.
  const Gender of_species = data(set.species).gender;
  if (set.gender != Gender::Either) p.gender = set.gender;
  else if (of_species != Gender::Either) p.gender = of_species;
  else p.gender = random(b, out, 2) == 0 ? Gender::Male : Gender::Female; // sample(['M', 'F'])
  p.happiness = set.happiness;

  for (const Move given : set.moves) {
    if (given == Move::None) continue;
    Move move = given;
    // A typed Hidden Power names the type, unless the set already does.
    if (data(given).real_move != Move::None) {
      if (p.set.hp_type == Type::None) p.set.hp_type = data(given).type;
      move = data(given).real_move;
    }
    const MoveData& m = data(move);
    const uint8_t pp = m.no_pp_boosts ? m.pp : uint8_t(m.pp * 8 / 5); // calculatePP with 3 PP Ups (battle.ts:2390)
    if (!m.no_pp_boosts) p.pp_ups |= uint8_t(1 << p.moves);
    p.move_slots[p.moves++] = {move, pp, pp};
  }

  Type computed;
  hidden_power(p.set.ivs, computed, p.hp_power);
  p.hp_type = p.set.hp_type != Type::None ? p.set.hp_type : computed;
  p.base_hp_type = p.hp_type;
  p.base_hp_power = p.hp_power;
  p.base_moves = p.moves;
  p.base_ability = p.ability = set.ability;
  p.item = set.item;
  set_species(p, p.base_species);
  p.hp = p.maxhp;
}

// Side's constructor (side.ts:229): the team's Pokémon in order, each at its
// index.
inline void set_player(Battle& b, Out& out, int side, const Team& team) {
  Side& s = b.sides[side];
  s = Side{};
  s.size = team.size;
  for (int i = 0; i < team.size; i++) {
    init_pokemon(b, out, s.pokemon[i], team.sets[i]);
    s.pokemon[i].position = uint8_t(i);
  }
}

} // namespace adv
