// Replays recorded Showdown games (tools/trace.cjs) on the engine and
// compares everything the engine does with what Showdown did, up to each
// game's first choice:
//
// - every draw, in order, with its arguments;
// - every log line Showdown sent;
// - both players' first requests, byte for byte.
//
// A game whose teams the engine cannot hold is skipped, and a game that
// reaches a handler not yet ported stops there; both are counted by reason.
//
// Usage: replay FILE

#include <cstdio>
#include <fstream>
#include <map>
#include <string>

#include "engine.hpp"
#include "showdown/log.hpp"

using namespace adv;

namespace {

struct Game {
  std::string seed;
  std::string teams[2];
  std::vector<Draw> draws;
  std::vector<std::string> log;      // the `u` lines
  std::vector<std::string> sent[2];  // the `s` lines of each side
  uint64_t line = 0;
};

struct Tally {
  uint64_t games = 0, skipped = 0, stopped = 0, matched = 0, failures = 0;
  uint64_t pokemon = 0, draws = 0, lines = 0, requests = 0;
  std::map<std::string, int> skips, stops;
};

bool fail(const Game& g, Tally& t, const std::string& what, const std::string& want, const std::string& have) {
  size_t at = 0;
  while (at < want.size() && at < have.size() && want[at] == have[at]) at++;
  const size_t from = at > 70 ? at - 70 : 0;
  std::fprintf(stderr, "game at line %llu: %s differs at byte %zu\n  Showdown: %s\n  engine:   %s\n",
               static_cast<unsigned long long>(g.line), what.c_str(), at, want.substr(from, 160).c_str(),
               have.substr(from, 160).c_str());
  t.failures++;
  return false;
}

bool check(const Game& g, Tally& t) {
  t.games++;
  Battle battle;
  std::vector<Draw> draws;
  Out out;
  out.draws = &draws;
  if (!g.seed.starts_with("gen5,") ||
      std::from_chars(g.seed.data() + 5, g.seed.data() + g.seed.size(), battle.prng.seed, 16).ec != std::errc()) {
    return fail(g, t, "the seed", "gen5,<16 hex digits>", g.seed);
  }
  showdown::Names names;
  names.players = {"A", "B"}; // as tools/lib.cjs names them
  Team teams[2];
  for (int side = 0; side < 2; side++) {
    std::string error;
    const auto team = showdown::unpack(g.teams[side], names.teams[side], error);
    if (!team) {
      t.skipped++;
      t.skips[error.substr(0, error.find('"'))]++;
      return true;
    }
    teams[side] = *team;
    t.pokemon += team->size;
  }

  Ctx c{battle, out};
  try {
    construct(c);
    set_player_and_start(c, 0, teams[0]);
    set_player_and_start(c, 1, teams[1]);
  } catch (const Unported& u) {
    t.stopped++;
    t.stops[u.what]++;
    return true;
  }

  for (size_t i = 0; i < std::max(draws.size(), g.draws.size()); i++) {
    const auto text = [&](const std::vector<Draw>& list) {
      if (i >= list.size()) return std::string("no draw");
      return "random(" + std::to_string(list[i].from) + ", " + std::to_string(list[i].to) + ") = " + std::to_string(list[i].result);
    };
    if (text(g.draws) != text(draws)) return fail(g, t, "draw " + std::to_string(i), text(g.draws), text(draws));
  }
  t.draws += draws.size();

  std::vector<std::string> log;
  for (const Line& line : out.log) showdown::write(line, battle, names, log);
  for (size_t i = 0; i < std::max(log.size(), g.log.size()); i++) {
    const std::string want = i < g.log.size() ? g.log[i] : "<no line>", have = i < log.size() ? log[i] : "<no line>";
    if (want != have) return fail(g, t, "log line " + std::to_string(i), want, have);
  }
  t.lines += log.size();

  for (int side = 0; side < 2; side++) {
    const std::string have = "|request|" + showdown::request_json(battle, side, names);
    if (g.sent[side].size() != 1) return fail(g, t, "what p" + std::to_string(side + 1) + " was sent", "one request", std::to_string(g.sent[side].size()) + " lines");
    if (g.sent[side][0] != have) return fail(g, t, "p" + std::to_string(side + 1) + "'s request", g.sent[side][0], have);
    t.requests++;
  }
  t.matched++;
  return true;
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::fprintf(stderr, "usage: replay FILE\n");
    return 2;
  }
  std::ifstream in(argv[1]);
  if (!in) {
    std::fprintf(stderr, "cannot open %s\n", argv[1]);
    return 2;
  }
  Tally t;
  Game g;
  std::string line;
  uint64_t n = 0;
  while (std::getline(in, line)) {
    n++;
    if (line.starts_with("G ")) {
      g = Game{};
      g.line = n;
      g.seed = line.substr(line.rfind(' ') + 1);
    } else if (line.starts_with("T p1 ")) {
      g.teams[0] = line.substr(5);
    } else if (line.starts_with("T p2 ")) {
      g.teams[1] = line.substr(5);
    } else if (line.starts_with("d ")) {
      // d <from> <to> <result> <log lines>
      Draw d{-1, -1, 0};
      char from[16], to[16];
      if (std::sscanf(line.c_str(), "d %15s %15s %u", from, to, &d.result) == 3) {
        if (from[0] != '-') d.from = std::atoi(from);
        if (to[0] != '-') d.to = std::atoi(to);
        g.draws.push_back(d);
      }
    } else if (line.starts_with("u ")) {
      g.log.push_back(line.substr(2));
    } else if (line.starts_with("s p1 ") || line.starts_with("s p2 ")) {
      g.sent[line[3] - '1'].push_back(line.substr(5));
    } else if (line == "E") {
      check(g, t);
    }
  }
  std::printf("%llu games: %llu matched, %llu stopped at something not ported, %llu skipped, %llu failures\n",
              static_cast<unsigned long long>(t.games), static_cast<unsigned long long>(t.matched),
              static_cast<unsigned long long>(t.stopped), static_cast<unsigned long long>(t.skipped),
              static_cast<unsigned long long>(t.failures));
  std::printf("compared in the matched games: %llu draws, %llu log lines, %llu requests\n",
              static_cast<unsigned long long>(t.draws), static_cast<unsigned long long>(t.lines),
              static_cast<unsigned long long>(t.requests));
  for (const auto& [why, count] : t.stops) std::printf("  stopped %d: %s\n", count, why.c_str());
  for (const auto& [why, count] : t.skips) std::printf("  skipped %d: %s\n", count, why.c_str());
  return t.failures || !t.matched ? 1 : 0;
}
