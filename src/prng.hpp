// Showdown's PRNG with a `gen5,` seed (sim/prng.ts at a78551b): the Gen 5
// games' 64-bit LCG, and the helpers the battle draws through. Every helper
// makes the same number of draws with the same arguments as Showdown's, and
// returns the same value.
//
// Showdown's default seed is `sodium,` (ChaCha20). A battle never sees which
// generator it has, so the engine has the simpler one and the tests seed
// Showdown to match.

#pragma once

#include <cassert>
#include <cstdint>
#include <span>
#include <utility>

namespace adv {

struct PRNG {
  uint64_t seed;

  // Gen5RNG.next: advance, then the upper 32 bits.
  constexpr uint32_t next() {
    seed = seed * 0x5D588B656C078965 + 0x00269EC3;
    return uint32_t(seed >> 32);
  }

  // random(n): Math.floor(result * n / 2 ** 32). In doubles that is exact
  // while result * n is below 2 ** 53, so n must be below 2 ** 21.
  constexpr uint32_t random(uint32_t n) {
    assert(n < (1u << 21));
    return uint32_t(uint64_t(next()) * n >> 32);
  }

  // random(m, n): an integer in [m, n). Showdown's `if (!to)` makes
  // random(m, 0) mean random(m), which no caller relies on.
  constexpr uint32_t random(uint32_t from, uint32_t to) {
    assert(to > from);
    return random(to - from) + from;
  }

  // randomChance(numerator, denominator).
  constexpr bool chance(uint32_t numerator, uint32_t denominator) {
    return random(denominator) < numerator;
  }

  // sample(items): one draw, even for a single item.
  template <typename T> constexpr const T& sample(std::span<const T> items) {
    assert(!items.empty());
    return items[random(uint32_t(items.size()))];
  }

  // shuffle(items, start, end): Fisher-Yates from the front, one draw for
  // every position but the last.
  template <typename T> constexpr void shuffle(std::span<T> items, uint32_t start, uint32_t end) {
    while (start + 1 < end) {
      const uint32_t next_index = random(start, end);
      if (start != next_index) std::swap(items[start], items[next_index]);
      start++;
    }
  }
};

static_assert(sizeof(PRNG) == 8);

} // namespace adv
