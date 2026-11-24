// Replays recorded Showdown games (tools/trace.cjs) on the engine and
// compares the two line by line: every draw with its arguments, its result
// and where in the log it fell, every choice and what refusing it sent,
// every log line, and every request.
//
// A game whose teams the engine cannot hold is skipped. A game that reaches
// something not yet ported stops there, after everything before it was
// compared; both are counted by reason. The battle's closing `e` line, which
// repeats the teams and the input, is not compared.
//
// Usage: replay FILE [--complete]
//
// FILE may be `-`, to read a trace that tools/trace.cjs is writing to a pipe.
//
// With --complete every game must run to its end: one that is skipped or
// stops fails the run, as a line that differs does.

#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <string>

#include "showdown/stream.hpp"
#include "view.hpp"

using namespace adv;

namespace {

struct Game {
  std::string seed;
  std::string teams[2];
  std::vector<std::string> lines; // everything after the teams
  uint64_t line = 0;
  std::string number; // from the `# game K` comment before it, if there was one
};

struct Tally {
  uint64_t games = 0, skipped = 0, stopped = 0, matched = 0, failures = 0;
  uint64_t lines = 0, draws = 0, choices = 0, requests = 0, views = 0;
  std::map<std::string, int> skips, stops;
};

void fail(const Game& g, Tally& t, size_t at, const std::string& want, const std::string& have) {
  size_t byte = 0;
  while (byte < want.size() && byte < have.size() && want[byte] == have[byte]) byte++;
  const size_t from = byte > 70 ? byte - 70 : 0;
  std::fprintf(stderr, "game %sat line %llu, its line %zu, byte %zu\n  Showdown: %s\n  engine:   %s\n",
               g.number.empty() ? "" : (g.number + " ").c_str(), static_cast<unsigned long long>(g.line), at, byte,
               want.substr(from, 170).c_str(), have.substr(from, 170).c_str());
  t.failures++;
}

void check(const Game& g, Tally& t) {
  t.games++;
  showdown::Stream stream;
  stream.names.players = {"A", "B"}; // as tools/lib.cjs names them
  uint64_t seed = 0;
  if (!g.seed.starts_with("gen5,") ||
      std::from_chars(g.seed.data() + 5, g.seed.data() + g.seed.size(), seed, 16).ec != std::errc()) {
    return fail(g, t, 0, "gen5,<16 hex digits>", g.seed);
  }
  Team teams[2];
  for (int side = 0; side < 2; side++) {
    std::string error;
    const auto team = showdown::unpack(g.teams[side], stream.names.teams[side], error);
    if (!team) {
      t.skipped++;
      t.skips[error]++;
      return;
    }
    teams[side] = *team;
  }

  // The engine's lines are compared as they come; where it has no more, the
  // trace's next line must be a choice, which is then made.
  size_t want = 0, have = 0;
  std::string stop;
  const auto compare = [&] {
    for (; have < stream.lines.size(); have++, want++) {
      const std::string& mine = stream.lines[have];
      if (want >= g.lines.size()) return fail(g, t, want, "<the end>", mine), false;
      if (g.lines[want] != mine) {
        fail(g, t, want, g.lines[want], mine);
        // With REPLAY_CONTEXT set, what each side has next.
        if (std::getenv("REPLAY_CONTEXT")) {
          for (size_t i = 0; i < 40 && (want + i < g.lines.size() || have + i < stream.lines.size()); i++) {
            const std::string a = want + i < g.lines.size() ? g.lines[want + i].substr(0, 100) : "";
            const std::string b = have + i < stream.lines.size() ? stream.lines[have + i].substr(0, 100) : "";
            std::fprintf(stderr, "    %-100s | %s\n", a.c_str(), b.c_str());
          }
        }
        return false;
      }
      t.lines++;
      t.draws += mine[0] == 'd';
      t.choices += mine[0] == 'c';
      t.requests += mine.find("|request|") != std::string::npos;
    }
    return true;
  };
  try {
    stream.start(seed, teams[0], teams[1]);
    while (compare() && want < g.lines.size()) {
      const std::string& next = g.lines[want];
      if (!next.starts_with("c p")) return fail(g, t, want, next, "<nothing>");
      stream.choose(next[3] - '1', std::string_view(next).substr(5));
    }
  } catch (const Unported& u) {
    stop = u.what;
  }
  // A failure was reported where it was found. What the engine wrote before
  // it stopped is compared too.
  const uint64_t failures = t.failures;
  if (have < stream.lines.size() && !stop.empty()) compare();
  if (t.failures != failures || have < stream.lines.size()) return;
  if (!stop.empty()) {
    t.stopped++;
    t.stops[stop]++;
    return;
  }
  // Each player's channel of the log (view.hpp) must be what Showdown's
  // stream sends that player: of a split line its own side's half, and of a
  // line for the other side alone, nothing.
  for (int side = 0; side < 2; side++) {
    View view(side);
    showdown::Names shown = stream.names;
    std::vector<std::string> all;
    for (const Line& line : stream.out.log) {
      all.clear();
      showdown::write(line, stream.battle, stream.names, all);
      const std::string& sent = all.size() == 1 ? all[0] : all[0] == (side == 0 ? "|split|p1" : "|split|p2") ? all[1] : all[2];
      Line mine;
      std::string text;
      if (view.line(line, stream.battle, mine)) {
        for (int k = 0; k < view.seen; k++) {
          shown.teams[size_t(1 - side)].names[size_t(k)] = stream.names.teams[size_t(1 - side)].names[view.index[size_t(k)]];
        }
        text = showdown::line_text(mine, stream.battle, shown);
      }
      if (text != sent) return fail(g, t, 0, sent, text);
      t.views++;
    }
  }
  t.matched++;
}

} // namespace

int main(int argc, char** argv) {
  const bool complete = argc == 3 && std::string(argv[2]) == "--complete";
  if (argc != 2 && !complete) {
    std::fprintf(stderr, "usage: replay FILE [--complete]\n");
    return 2;
  }
  std::ifstream file;
  if (std::string(argv[1]) != "-") {
    file.open(argv[1]);
    if (!file) {
      std::fprintf(stderr, "cannot open %s\n", argv[1]);
      return 2;
    }
  }
  std::istream& in = file.is_open() ? static_cast<std::istream&>(file) : std::cin;
  Tally t;
  Game g;
  std::string line, number;
  uint64_t n = 0;
  while (std::getline(in, line)) {
    n++;
    if (line.starts_with("# game ")) {
      number = line.substr(7);
    } else if (line.starts_with("G ")) {
      g = Game{};
      g.line = n;
      g.number = number;
      g.seed = line.substr(line.rfind(' ') + 1);
    } else if (line.starts_with("T p1 ")) {
      g.teams[0] = line.substr(5);
    } else if (line.starts_with("T p2 ")) {
      g.teams[1] = line.substr(5);
    } else if (line == "E") {
      check(g, t);
    } else if (line.starts_with("d ")) {
      // d <from> <to> <result> <log lines> [<call site>]
      size_t end = 0;
      for (int spaces = 0; end < line.size(); end++) {
        if (line[end] == ' ' && ++spaces == 5) break;
      }
      g.lines.push_back(line.substr(0, end));
    } else if (!line.starts_with("e ") && !line.starts_with("#")) {
      g.lines.push_back(line);
    }
  }
  std::printf("%llu games: %llu matched to the end, %llu stopped at something not ported, %llu skipped, %llu failures\n",
              static_cast<unsigned long long>(t.games), static_cast<unsigned long long>(t.matched),
              static_cast<unsigned long long>(t.stopped), static_cast<unsigned long long>(t.skipped),
              static_cast<unsigned long long>(t.failures));
  std::printf("lines compared and equal: %llu, of them %llu draws, %llu choices, %llu requests\n",
              static_cast<unsigned long long>(t.lines), static_cast<unsigned long long>(t.draws),
              static_cast<unsigned long long>(t.choices), static_cast<unsigned long long>(t.requests));
  std::printf("lines of the players' views equal: %llu\n", static_cast<unsigned long long>(t.views));
  for (const auto& [why, count] : t.stops) std::printf("  stopped %d: %s\n", count, why.c_str());
  for (const auto& [why, count] : t.skips) std::printf("  skipped %d: %s\n", count, why.c_str());
  if (complete && (t.stopped || t.skipped)) return 1;
  return t.failures || !(t.matched + t.stopped) ? 1 : 0;
}
