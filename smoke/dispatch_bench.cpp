// How much event dispatch costs with and without a "does anyone listen?" test,
// on the mix of events Showdown really dispatches (census_mix.inc, cut from
// the output of tools/census.cjs). Synthetic: the handlers do nothing, and holders' effects
// are random with the measured chance of a listener per event. It times the
// dispatcher, not a battle.
//
// The design under test:
// - every callback name is a bit (Gen 3 has 108 names);
// - every effect has a constant mask of the names it defines;
// - every holder (Pokémon, side, field) keeps the OR of its effects' masks,
//   updated when an effect is added or removed;
// - an event tests those masks against the names it would look for, and only
//   gathers, sorts and calls handlers when a bit is set.
//
// Usage: dispatch_bench [UPDATES]

#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "prng.hpp"

namespace {

struct Mix { const char* name; int per_100_updates; int hit_permille; };
constexpr Mix MIX[] = {
#include "census_mix.inc"
};
constexpr int EVENTS = int(std::size(MIX));
constexpr int NAMES = 192;
constexpr int FILLERS = 512;
constexpr int EFFECTS = EVENTS + FILLERS;

struct Mask {
  uint64_t w[3]{};
  constexpr void set(int bit) { w[bit >> 6] |= uint64_t{1} << (bit & 63); }
  constexpr bool has(int bit) const { return w[bit >> 6] >> (bit & 63) & 1; }
  constexpr uint64_t overlap(const Mask& o) const { return (w[0] & o.w[0]) | (w[1] & o.w[1]) | (w[2] & o.w[2]); }
  constexpr void merge(const Mask& o) { w[0] |= o.w[0]; w[1] |= o.w[1]; w[2] |= o.w[2]; }
};

using Fn = int (*)(int);
int handler_a(int v) { return v + 1; }
int handler_b(int v) { return v ^ 5; }

// Prefixes, as findEventHandlers: on, onAlly, onFoe, onAny, onSource.
enum { ON, ALLY, FOE, ANY, SOURCE, PREFIXES };
int name_of[EVENTS][PREFIXES];
Mask effect_mask[EFFECTS];
Fn table[EFFECTS][NAMES];

// The names an event looks for on each holder.
struct Need { Mask self, foe, source, side, foe_side, field; };
Need need[EVENTS];

struct Holder {
  std::array<uint16_t, 8> effects{};
  uint8_t n = 0;
  uint16_t speed = 0;
  Mask listens;
  void add(uint16_t effect) {
    if (n == effects.size()) return;
    effects[n++] = effect;
    listens.merge(effect_mask[effect]);
  }
};
struct State { Holder mon[2], side[2], field; };

struct Handler { Fn fn; uint32_t speed; uint8_t sub_order; };

inline void gather(const Holder& h, int name, std::array<Handler, 24>& out, int& n, uint8_t sub_order) {
  if (name < 0) return;
  for (int i = 0; i < h.n; i++) {
    const uint16_t effect = h.effects[i];
    if (Fn fn = table[effect][name]) out[n++] = {fn, h.speed, sub_order};
  }
}

// findEventHandlers for a Pokémon target in singles, then speedSort and the calls.
inline int slow_path(const State& s, int event, int target, adv::PRNG& prng) {
  std::array<Handler, 24> list;
  int n = 0;
  const int* names = name_of[event];
  gather(s.mon[target], names[ON], list, n, 2);
  gather(s.mon[target], names[ALLY], list, n, 2);
  gather(s.mon[target], names[ANY], list, n, 2);
  gather(s.mon[target ^ 1], names[FOE], list, n, 2);
  gather(s.mon[target ^ 1], names[ANY], list, n, 2);
  gather(s.mon[target ^ 1], names[SOURCE], list, n, 2);
  gather(s.side[target], names[ON], list, n, 4);
  gather(s.side[target ^ 1], names[FOE], list, n, 4);
  gather(s.side[target ^ 1], names[ANY], list, n, 4);
  gather(s.field, names[ON], list, n, 5);
  // Selection sort by Speed then sub-order, ties shuffled, as speedSort.
  int sorted = 0;
  while (sorted + 1 < n) {
    int ties = 1;
    for (int i = sorted + 1; i < n; i++) {
      const auto key = [](const Handler& h) { return (uint64_t(h.speed) << 8) | uint8_t(255 - h.sub_order); };
      const uint64_t best = key(list[sorted]), mine = key(list[i]);
      if (mine > best) { std::swap(list[sorted], list[i]); ties = 1; }
      else if (mine == best) { std::swap(list[sorted + ties], list[i]); ties++; }
    }
    if (ties > 1) prng.shuffle(std::span<Handler>(list), uint32_t(sorted), uint32_t(sorted + ties));
    sorted += ties;
  }
  int relay = event;
  for (int i = 0; i < n; i++) relay = list[i].fn(relay);
  return relay + n;
}

inline bool listened(const State& s, int event, int target) {
  const Need& k = need[event];
  return (s.mon[target].listens.overlap(k.self) | s.mon[target ^ 1].listens.overlap(k.foe) |
          s.mon[target ^ 1].listens.overlap(k.source) | s.side[target].listens.overlap(k.side) |
          s.side[target ^ 1].listens.overlap(k.foe_side) | s.field.listens.overlap(k.field)) != 0;
}

} // namespace

int main(int argc, char** argv) {
  const long updates = argc > 1 ? std::atol(argv[1]) : 2'000'000;
  adv::PRNG setup{0x1234};

  // Names: `on` for every event, and a prefixed name for about one in six.
  int names = EVENTS;
  for (int e = 0; e < EVENTS; e++) {
    name_of[e][ON] = e;
    for (int p = 1; p < PREFIXES; p++) name_of[e][p] = setup.chance(1, 6) && names < NAMES - 24 ? names++ : -1;
  }
  // Effect e listens to `on` event e. Fillers listen to names no scheduled
  // event looks for (the last 24), as an item or ability with nothing to do
  // with the event at hand.
  for (int e = 0; e < EVENTS; e++) { effect_mask[e].set(e); table[e][e] = (e & 1) ? handler_a : handler_b; }
  for (int f = EVENTS; f < EFFECTS; f++) {
    for (int k = 0; k < 2; k++) {
      const int name = NAMES - 24 + int(setup.random(24));
      effect_mask[f].set(name);
      table[f][name] = handler_a;
    }
  }
  for (int e = 0; e < EVENTS; e++) {
    const int* n = name_of[e];
    Need& k = need[e];
    for (const int p : {ON, ALLY, ANY}) if (n[p] >= 0) k.self.set(n[p]);
    for (const int p : {FOE, ANY}) if (n[p] >= 0) { k.foe.set(n[p]); k.foe_side.set(n[p]); }
    if (n[SOURCE] >= 0) k.source.set(n[SOURCE]);
    k.side.set(n[ON]);
    k.field.set(n[ON]);
  }

  // States: a few fillers on every holder, and for each event a listener on a
  // Pokémon with the measured chance.
  std::vector<State> states(4096);
  for (State& s : states) {
    for (Holder* h : {&s.mon[0], &s.mon[1]}) {
      h->speed = uint16_t(100 + setup.random(8) * 20);
      for (uint32_t i = 0, n = 2 + setup.random(3); i < n; i++) h->add(uint16_t(EVENTS + setup.random(FILLERS)));
    }
    for (Holder* h : {&s.side[0], &s.side[1]}) if (setup.chance(1, 3)) h->add(uint16_t(EVENTS + setup.random(FILLERS)));
    for (int i = 0; i < 3; i++) s.field.add(uint16_t(EVENTS + setup.random(FILLERS)));
    for (int e = 0; e < EVENTS; e++) {
      // On both, since the schedule alternates the target and `on` is looked
      // for on the target alone.
      if (setup.chance(uint32_t(MIX[e].hit_permille), 1000)) { s.mon[0].add(uint16_t(e)); s.mon[1].add(uint16_t(e)); }
    }
  }

  // The schedule: 100 updates' worth of events in a fixed shuffled order.
  std::vector<uint8_t> schedule;
  for (int e = 0; e < EVENTS; e++) schedule.insert(schedule.end(), size_t(MIX[e].per_100_updates), uint8_t(e));
  setup.shuffle(std::span<uint8_t>(schedule), 0, uint32_t(schedule.size()));
  const double per_update = double(schedule.size()) / 100;

  for (const bool precheck : {true, false}) {
    adv::PRNG prng{1};
    uint64_t sink = 0, hits = 0, dispatches = 0;
    const auto start = std::chrono::steady_clock::now();
    size_t at = 0;
    for (long u = 0; u < updates; u++) {
      const State& s = states[size_t(u) & 4095];
      const size_t end = size_t(double(u + 1) * per_update) - size_t(double(u) * per_update);
      for (size_t i = 0; i < end; i++) {
        const int event = schedule[at];
        at = at + 1 == schedule.size() ? 0 : at + 1;
        const int target = int(i & 1);
        dispatches++;
        if (precheck && !listened(s, event, target)) continue;
        const int r = slow_path(s, event, target, prng);
        sink += uint64_t(r);
        hits += r != event;
      }
    }
    const double ns = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count();
    std::printf("%-26s %6.1f dispatches/update, %4.1f%% reach a handler: %6.1f ns/update, %5.2f ns/dispatch (sink %llu)\n",
                precheck ? "mask test first" : "always gather", double(dispatches) / double(updates),
                100.0 * double(hits) / double(dispatches), ns / double(updates), ns / double(dispatches),
                static_cast<unsigned long long>(sink));
  }
}
