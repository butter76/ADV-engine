// A battle driven as Showdown's BattleStream drives one, writing what
// tools/trace.cjs records in the same lines: draws, choices as tried, log
// updates, and what each side is sent. A test compares the two line by line.

#pragma once

#include <charconv>
#include <string>
#include <vector>

#include "../engine.hpp"
#include "log.hpp"

namespace adv::showdown {

struct Stream {
  Battle battle;
  Out out;
  std::vector<Draw> draws;
  Ctx ctx;
  Names names;
  std::vector<std::string> lines;
  size_t sent_log = 0, sent_draws = 0;

  Stream() : ctx(battle, out) { out.draws = &draws; }

  // Battle.sendUpdates (battle.ts:3325): the new log lines, then the requests
  // if they were not sent yet. The draws made since the last call come
  // first, as the recorder writes them when they are made.
  void send_updates() {
    for (; sent_draws < draws.size(); sent_draws++) {
      const Draw& d = draws[sent_draws];
      lines.push_back("d " + (d.from < 0 ? std::string("-") : std::to_string(d.from)) + " " +
                      (d.to < 0 ? std::string("-") : std::to_string(d.to)) + " " + std::to_string(d.result) + " " +
                      std::to_string(d.log_lines));
    }
    if (sent_log < out.log.size()) {
      lines.push_back("U");
      std::vector<std::string> text;
      for (; sent_log < out.log.size(); sent_log++) write(out.log[sent_log], battle, names, text);
      for (const std::string& t : text) lines.push_back("u " + t);
    }
    if (!battle.sent_requests) {
      for (int side = 0; side < 2; side++) send_request(side);
      battle.sent_requests = true;
    }
  }
  void send_request(int side) { lines.push_back("s " + side_id(side) + " |request|" + request_json(battle, side, names)); }

  // `>start`, `>player p1` and `>player p2` written as one chunk.
  void start(uint64_t seed, const Team& p1, const Team& p2) {
    battle.prng.seed = seed;
    construct(ctx);
    set_player_and_start(ctx, 0, p1);
    set_player_and_start(ctx, 1, p2);
    send_updates();
  }

  // `>p1 switch 3`: Side.choose's text (side.ts), then the updates.
  void choose(int side, std::string_view text) {
    lines.push_back("c " + side_id(side) + " " + std::string(text));
    Refusal refusal = Refusal::None;
    if (text.starts_with("switch ")) {
      int slot = 0;
      std::from_chars(text.data() + 7, text.data() + text.size(), slot);
      refusal = choose_switch(ctx, side, slot - 1);
    } else if (text.starts_with("move ")) {
      int slot = 0;
      std::from_chars(text.data() + 5, text.data() + text.size(), slot);
      refusal = choose_move(ctx, side, slot - 1);
    } else if (text == "default" && battle.requests[side].kind == Request::Kind::Switch) {
      refusal = choose_switch(ctx, side, -1);
    } else if (text == "default" && battle.requests[side].kind == Request::Kind::Move) {
      refusal = choose_move(ctx, side, -1);
    } else {
      unported("the choice \"" + std::string(text.substr(0, text.find(' '))) + "\"");
    }
    if (refusal != Refusal::None) {
      // Side.emitChoiceError (side.ts:527).
      lines.push_back("s " + side_id(side) + " |error|[" + (refusal == Refusal::Unavailable ? "Unavailable" : "Invalid") +
                      " choice] Can't switch: The active Pokémon is trapped");
      if (refusal == Refusal::Unavailable) send_request(side);
    }
    send_updates();
  }
};

} // namespace adv::showdown
