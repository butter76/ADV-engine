// Replays the draws of recorded Showdown games (tools/trace.cjs) through
// src/prng.hpp: from each game's seed, every draw in order must return what
// Showdown's did. It checks the generator on the calls real battles make, and
// the trace reader the engine's replay will be built on.
//
// Usage: trace_draws FILE   (or - for stdin)

#include <charconv>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

#include "prng.hpp"

namespace {

// The next space-separated word of `line`, which it is removed from.
std::string_view word(std::string_view& line) {
  const size_t end = line.find(' ');
  const std::string_view w = line.substr(0, end);
  line.remove_prefix(end == std::string_view::npos ? line.size() : end + 1);
  return w;
}

// A number, or -1 for `-` (an argument Showdown's caller did not give).
int64_t number(std::string_view w) {
  if (w == "-") return -1;
  int64_t v = 0;
  const auto r = std::from_chars(w.data(), w.data() + w.size(), v);
  if (r.ec != std::errc() || r.ptr != w.data() + w.size()) return -2;
  return v;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: trace_draws FILE\n");
    return 2;
  }
  std::ifstream file;
  if (std::string_view(argv[1]) != "-") {
    file.open(argv[1]);
    if (!file) {
      std::fprintf(stderr, "cannot open %s\n", argv[1]);
      return 2;
    }
  }
  std::istream& in = file.is_open() ? file : std::cin;

  adv::PRNG prng{0};
  uint64_t games = 0, draws = 0, lines = 0, bare = 0, ranged = 0;
  bool seeded = false;
  std::string text;
  while (std::getline(in, text)) {
    lines++;
    std::string_view line = text;
    const std::string_view tag = word(line);
    if (tag == "G") {
      word(line); // the format
      const std::string_view seed = line;
      uint64_t value = 0;
      const auto r = seed.starts_with("gen5,")
                         ? std::from_chars(seed.data() + 5, seed.data() + seed.size(), value, 16)
                         : std::from_chars_result{seed.data(), std::errc::invalid_argument};
      if (r.ec != std::errc() || seed.size() != 21) {
        std::fprintf(stderr, "line %llu: not a gen5 seed: %s\n", static_cast<unsigned long long>(lines), text.c_str());
        return 1;
      }
      prng.seed = value;
      seeded = true;
      games++;
    } else if (tag == "d") {
      const int64_t from = number(word(line)), to = number(word(line)), result = number(word(line));
      if (!seeded || from < -1 || to < -1 || result < 0) {
        std::fprintf(stderr, "line %llu: bad draw: %s\n", static_cast<unsigned long long>(lines), text.c_str());
        return 1;
      }
      int64_t mine;
      if (from == -1) {
        mine = prng.next(); // random(): the 32-bit value itself
        bare++;
      } else if (to == -1 || to == 0) {
        mine = prng.random(uint32_t(from)); // random(n), and random(m, 0) by Showdown's `!to`
      } else {
        mine = prng.random(uint32_t(from), uint32_t(to));
        ranged++;
      }
      if (mine != result) {
        std::fprintf(stderr, "line %llu, game %llu, draw %llu: Showdown drew %lld, we drew %lld: %s\n",
                     static_cast<unsigned long long>(lines), static_cast<unsigned long long>(games),
                     static_cast<unsigned long long>(draws), static_cast<long long>(result),
                     static_cast<long long>(mine), text.c_str());
        return 1;
      }
      draws++;
    } else if (tag == "E") {
      seeded = false;
    }
  }
  std::printf("%llu games, %llu draws replayed (%llu with a range, %llu bare), %llu lines\n",
              static_cast<unsigned long long>(games), static_cast<unsigned long long>(draws),
              static_cast<unsigned long long>(ranged), static_cast<unsigned long long>(bare),
              static_cast<unsigned long long>(lines));
  return games ? 0 : 1;
}
