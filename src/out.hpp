// What the engine writes besides the battle's state, and the draws.

#pragma once

#include <vector>

#include "battle.hpp"
#include "log.hpp"

namespace adv {

// A draw as tools/trace.cjs records it: random(from, to), -1 for an argument
// not given.
struct Draw {
  int32_t from, to;
  uint32_t result;
};

// What the engine writes besides the state. `draws` is for the tests, which
// compare every draw with Showdown's; it is null otherwise.
struct Out {
  std::vector<Line> log;
  std::vector<Draw>* draws = nullptr;
};

// Battle.random(n) and Battle.random(m, n) (battle.ts:347), which
// randomChance, sample and shuffle draw through.
inline uint32_t random(Battle& b, Out& out, uint32_t n) {
  const uint32_t result = b.prng.random(n);
  if (out.draws) out.draws->push_back({int32_t(n), -1, result});
  return result;
}
inline uint32_t random(Battle& b, Out& out, uint32_t from, uint32_t to) {
  const uint32_t result = b.prng.random(from, to);
  if (out.draws) out.draws->push_back({int32_t(from), int32_t(to), result});
  return result;
}

} // namespace adv
