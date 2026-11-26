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

  Stream() : ctx(battle, out) {
    out.draws = &draws;
    out.log.reserve(2048); // a game of random choices writes about a thousand lines
  }

  // Battle.sendUpdates (battle.ts:3325): the new log lines, then the requests
  // if they were not sent yet. The draws made since the last call come
  // first, as the recorder writes them when they are made.
  void send_updates() {
    for (; sent_draws < draws.size(); sent_draws++) {
      const Draw& d = draws[sent_draws];
      append_with(lines.emplace_back(), 48, [&](Cursor to) {
        to.put("d ");
        for (const int32_t argument : {d.from, d.to}) {
          if (argument < 0) to.put('-');
          else put_number(to, unsigned(argument));
          to.put(' ');
        }
        put_number(to, d.result);
        to.put(' ');
        put_number(to, d.log_lines);
        return to;
      });
    }
    if (sent_log < out.log.size()) {
      lines.push_back("U");
      for (; sent_log < out.log.size(); sent_log++) write(out.log[sent_log], battle, names, lines, "u ");
    }
    if (!battle.sent_requests) {
      for (int side = 0; side < 2; side++) send_request(side);
      battle.sent_requests = true;
    }
  }
  void send_request(int side) {
    append_request_json(lines.emplace_back(), battle, side, names, side == 0 ? "s p1 |request|" : "s p2 |request|");
  }

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
    append_with(lines.emplace_back(), 8 + text.size(), [&](Cursor to) {
      to.put(std::string_view(side == 0 ? "c p1 " : "c p2 ", 5));
      to.put(text);
      return to;
    });
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
    if (battle.struggle_notice[size_t(side)] >= 0) {
      // Side.chooseMove tells the side alone that it has no move left.
      // It names the Pokémon that was asked, which the turn may since have
      // switched out.
      lines.push_back("s " + side_id(side) + " |-activate|" + side_id(side) + "a: " +
                      names.teams[side].names[size_t(battle.struggle_notice[size_t(side)])] + "|move: Struggle");
      battle.struggle_notice[size_t(side)] = -1;
    }
    if (refusal != Refusal::None) {
      // Side.emitChoiceError (side.ts:527).
      std::string why = "Can't switch: The active Pokémon is trapped";
      if (const Move refused = battle.refused_move[size_t(side)]; refused != Move::None) {
        why = "Can't move: " + names.teams[side].names[battle.sides[side].active] + "'s " + std::string(MOVE_NAMES[uint16_t(refused)].name) +
              " is disabled";
        battle.refused_move[size_t(side)] = Move::None;
      }
      lines.push_back("s " + side_id(side) + " |error|[" + (refusal == Refusal::Unavailable ? "Unavailable" : "Invalid") + " choice] " + why);
      if (refusal == Refusal::Unavailable) send_request(side);
    }
    send_updates();
  }
};

} // namespace adv::showdown
