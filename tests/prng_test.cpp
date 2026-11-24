// The same script of calls as tools/prng_ref.cjs, on src/prng.hpp. The two
// must print the same line.

#include <array>
#include <cstdint>
#include <cstdio>

#include "prng.hpp"

int main() {
  uint64_t hash = 0xcbf29ce484222325;
  const auto mix = [&](uint64_t v) { hash = (hash ^ v) * 0x100000001b3; };
  uint64_t calls = 0;
  const std::array<int, 8> digits{3, 1, 4, 1, 5, 9, 2, 6};

  for (uint64_t s = 1; s <= 500; s++) {
    adv::PRNG prng{s * 0x9E3779B97F4A7C15};
    for (uint32_t i = 0; i < 4000; i++) {
      const uint32_t k = uint32_t((s * 31 + i * 7) % 97);
      switch (i % 6) {
      case 0: mix(prng.random(1u << 16)); break;
      case 1: mix(prng.random(k + 1)); break;
      case 2: mix(prng.random(k, k + 1 + (i % 16))); break;
      case 3: mix(prng.chance(k, 100) ? 1 : 0); break;
      case 4: mix(uint64_t(prng.sample(std::span<const int>(digits.data(), 1 + (k % 8))))); break;
      case 5: {
        std::array<int, 8> list{0, 1, 2, 3, 4, 5, 6, 7};
        const uint32_t start = k % 4, end = start + 2 + (i % 4);
        prng.shuffle(std::span<int>(list), start, end);
        for (const int v : list) mix(uint64_t(v));
        break;
      }
      }
      calls++;
    }
    mix(prng.seed & 0xffffffff);
  }
  std::printf("%llu calls, hash %016llx\n", static_cast<unsigned long long>(calls),
              static_cast<unsigned long long>(hash));
}
