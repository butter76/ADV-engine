// How fast the engine plays recorded games: the choices of a trace
// (tools/trace.cjs, any form of it: only the G, T and c lines are read) are
// made again, first on the engine alone, which keeps its log as typed lines,
// and then through the stream that also writes Showdown's text and requests.
//
// An update is counted as the recorder counts one: a choice after which the
// log or the turn had changed. Times are of the process's CPU, so that a busy
// machine changes them little.
//
// Usage: engine_bench FILE [REPEATS] [alone|text]

#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

#include "showdown/stream.hpp"

using namespace adv;

namespace {

struct Game {
  uint64_t seed = 0;
  std::string packed[2];
  // The teams as the engine takes them, made before the clock starts: turning
  // Showdown's packed text into sets is not the engine's work.
  Team teams[2];
  showdown::TeamText text[2];
  std::vector<std::string> choices; // `p1 move 2`
};

struct Count {
  uint64_t games = 0, updates = 0, choices = 0, turns = 0, lines = 0, stopped = 0;
};

// The engine alone: Stream::choose without what it writes.
void play_alone(const Game& g, Count& n) {
  showdown::Stream s;
  s.out.draws = nullptr;
  s.battle.prng.seed = g.seed;
  construct(s.ctx);
  set_player_and_start(s.ctx, 0, g.teams[0]);
  set_player_and_start(s.ctx, 1, g.teams[1]);
  for (const std::string& c : g.choices) {
    const int side = c[1] - '1';
    const std::string_view what = std::string_view(c).substr(3);
    const size_t lines = s.out.log.size();
    const uint16_t turn = s.battle.turn;
    int slot = 0;
    if (what.starts_with("switch ")) {
      std::from_chars(what.data() + 7, what.data() + what.size(), slot);
      choose_switch(s.ctx, side, slot - 1);
    } else if (what.starts_with("move ")) {
      std::from_chars(what.data() + 5, what.data() + what.size(), slot);
      choose_move(s.ctx, side, slot - 1);
    } else if (s.battle.requests[size_t(side)].kind == Request::Kind::Switch) {
      choose_switch(s.ctx, side, -1);
    } else {
      choose_move(s.ctx, side, -1);
    }
    s.battle.refused_move[size_t(side)] = Move::None;
    s.battle.struggle_notice[size_t(side)] = -1;
    n.updates += s.out.log.size() != lines || s.battle.turn != turn;
  }
  n.choices += g.choices.size();
  n.lines += s.out.log.size();
  n.turns += s.battle.turn;
}

void play_text(const Game& g, Count& n) {
  showdown::Stream s;
  for (int i = 0; i < 2; i++) s.names.teams[i] = g.text[i];
  s.start(g.seed, g.teams[0], g.teams[1]);
  for (const std::string& c : g.choices) {
    const size_t lines = s.out.log.size();
    const uint16_t turn = s.battle.turn;
    s.choose(c[1] - '1', std::string_view(c).substr(3));
    s.lines.clear();
    n.updates += s.out.log.size() != lines || s.battle.turn != turn;
  }
  n.choices += g.choices.size();
  n.lines += s.out.log.size();
  n.turns += s.battle.turn;
}

} // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: engine_bench FILE [REPEATS] [alone|text]\n");
    return 2;
  }
  std::ifstream in(argv[1]);
  if (!in) {
    std::fprintf(stderr, "cannot open %s\n", argv[1]);
    return 2;
  }
  const int repeats = argc > 2 ? std::atoi(argv[2]) : 1;
  const std::string only = argc > 3 ? argv[3] : "";
  std::vector<Game> games;
  std::string line;
  while (std::getline(in, line)) {
    if (line.starts_with("G ")) {
      games.push_back({});
      games.back().seed = std::stoull(line.substr(line.rfind(',') + 1), nullptr, 16);
    } else if (line.starts_with("T p1 ") || line.starts_with("T p2 ")) {
      const int side = line[3] - '1';
      Game& g = games.back();
      g.packed[side] = line.substr(5);
      std::string error;
      const auto team = showdown::unpack(g.packed[side], g.text[side], error);
      if (!team) {
        std::fprintf(stderr, "a team the engine cannot hold: %s\n", error.c_str());
        return 2;
      }
      g.teams[side] = *team;
    } else if (line.starts_with("c p")) {
      games.back().choices.push_back(line.substr(2));
    }
  }
  for (const char* mode : {"alone", "text"}) {
    if (!only.empty() && only != mode) continue;
    Count n;
    const std::clock_t start = std::clock();
    for (int r = 0; r < repeats; r++) {
      for (const Game& g : games) {
        try {
          if (mode[0] == 'a') play_alone(g, n);
          else play_text(g, n);
        } catch (const Unported&) {
          n.stopped++;
        }
        n.games++;
      }
    }
    const double seconds = double(std::clock() - start) / CLOCKS_PER_SEC;
    std::printf("%-5s %8.0f updates/s %7.0f games/s %8.0f turns/s %9.0f log lines/s  (%llu games, %llu updates, %.2f s of CPU%s)\n",
                mode, double(n.updates) / seconds, double(n.games) / seconds, double(n.turns) / seconds, double(n.lines) / seconds,
                static_cast<unsigned long long>(n.games), static_cast<unsigned long long>(n.updates), seconds,
                n.stopped ? ", SOME GAMES STOPPED" : "");
  }
}
