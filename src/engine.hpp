// The battle's flow, function for function as Showdown's: the constructor
// and start, the action queue, the turn loop, switching in, the end of a
// turn and the requests. So far it runs a battle up to its first request.

#pragma once

#include <cmath>

#include "callbacks.hpp"
#include "setup.hpp"

namespace adv {

namespace events {
inline constexpr EventNames BEFORE_SWITCH_IN = event("BeforeSwitchIn");
inline constexpr EventNames ENTRY_HAZARD = event("EntryHazard");
inline constexpr EventNames AFTER_ENTRY_HAZARD = event("AfterEntryHazard");
inline constexpr EventNames SWITCH_IN = event("SwitchIn");
inline constexpr EventNames UPDATE = event("Update");
inline constexpr EventNames MODIFY_BOOST = event("ModifyBoost");
inline constexpr EventNames MODIFY_SPE = event("ModifySpe");
inline constexpr EventNames DISABLE_MOVE = event("DisableMove");
inline constexpr EventNames TRAP_POKEMON = event("TrapPokemon");
inline constexpr EventNames MAYBE_TRAP_POKEMON = event("MaybeTrapPokemon");
inline constexpr EventNames LOCK_MOVE = event("LockMove");
inline constexpr Callback ON_BEGIN = callback("", "Begin");
inline constexpr Callback ON_FOE_MAYBE_TRAP_POKEMON = callback("Foe", "MaybeTrapPokemon");
} // namespace events

// Battle.getAllActive (battle.ts:1379): p1's, then p2's.
inline int all_active(const Battle& b, std::array<Ref, 2>& out, bool include_fainted = false) {
  int n = 0;
  for (uint8_t s = 0; s < 2; s++) {
    const Side& side = b.sides[s];
    if (side.active != 0xFF && (include_fainted || !side.pokemon[side.active].fainted)) out[n++] = {s, side.active};
  }
  return n;
}

// An event whose relay value is an object, which the engine cannot pass yet:
// it must find no handler.
inline void run_event_without_handlers(Ctx& c, const EventNames& e, Subject t) {
  Listeners handlers;
  find_event_handlers(c, t, e, {}, handlers);
  if (handlers.n) unported("a handler of " + std::string(e.id));
}

// Pokemon.getStat('spe', false, false) (pokemon.ts:617).
inline double get_stat_spe(Ctx& c, Ref r) {
  const Pokemon& p = c.b.at(r);
  double stat = p.stored_stats.spe;
  run_event_without_handlers(c, events::MODIFY_BOOST, on(r));
  static constexpr double BOOST_TABLE[] = {1, 1.5, 2, 2.5, 3, 3.5, 4};
  const int boost = std::clamp(int(p.boosts.spe), -6, 6);
  stat = boost >= 0 ? std::floor(stat * BOOST_TABLE[boost]) : std::floor(stat / BOOST_TABLE[-boost]);
  stat = run_event(c, events::MODIFY_SPE, on(r), {}, {}, relay::number(stat)).number;
  return stat > 10000 ? 10000 : stat;
}

// Gen 3's Pokemon.getActionSpeed (data/mods/gen3/scripts.ts:36): Quick Claw,
// when the turn's roll succeeded, is a speed of 65535.
inline uint32_t get_action_speed(Ctx& c, Ref r) {
  const double speed = get_stat_spe(c, r);
  if (c.b.quick_claw_roll && c.b.at(r).item == Item::QuickClaw) return 65535;
  return uint32_t(speed);
}
// Pokemon.updateSpeed (pokemon.ts:577).
inline void update_speed(Ctx& c, Ref r) { c.b.at(r).speed = get_action_speed(c, r); }

// ---------------------------------------------------------------------------
// The queue (battle-queue.ts).
// ---------------------------------------------------------------------------

// BattleQueue.resolveAction (battle-queue.ts:166) for the actions that are
// not moves, and Battle.getActionSpeed (battle.ts:2673) for them.
inline Action resolve_action(Ctx& c, Action a) {
  switch (a.choice) {
  case Choice::Start: a.order = 2; break;
  case Choice::InstaSwitch: a.order = 3; break;
  case Choice::BeforeTurn: a.order = 4; break;
  case Choice::RunSwitch: a.order = 101; break;
  case Choice::Switch: a.order = 103; break;
  case Choice::Residual: a.order = 300; break;
  case Choice::Move: unported("a move action");
  }
  a.speed = a.pokemon ? double(get_action_speed(c, a.pokemon)) : 1;
  return a;
}

inline void push(Queue& q, const Action& a, int at) {
  if (q.size == q.list.size()) unported("more than 16 queued actions");
  for (int i = q.size; i > at; i--) q.list[i] = q.list[i - 1];
  q.list[at] = a;
  q.size++;
}
// BattleQueue.addChoice (battle-queue.ts:307).
inline void add_choice(Ctx& c, Action a) { push(c.b.queue, resolve_action(c, a), c.b.queue.size); }

// BattleQueue.insertChoice (battle-queue.ts:375): among the queued actions it
// ties with, the place is drawn.
inline void insert_choice(Ctx& c, Action a) {
  if (a.pokemon) update_speed(c, a.pokemon);
  const Action action = resolve_action(c, a);
  Queue& q = c.b.queue;
  int first = -1, last = -1;
  for (int i = 0; i < q.size; i++) {
    const double compared = compare_priority(action, q.list[i]);
    if (compared <= 0 && first < 0) first = i;
    if (compared < 0) {
      last = i;
      break;
    }
  }
  if (first < 0) return push(q, action, q.size);
  if (last < 0) last = q.size;
  const int at = first == last ? first : int(random(c.b, c.out, uint32_t(first), uint32_t(last + 1)));
  push(q, action, at);
}

inline Action shift(Queue& q) {
  const Action a = q.list[0];
  for (int i = 1; i < q.size; i++) q.list[i - 1] = q.list[i];
  q.size--;
  return a;
}

// ---------------------------------------------------------------------------
// Faints. Nothing can faint yet.
// ---------------------------------------------------------------------------
inline void resolve_faints(Ctx& c) {
  for (const Side& s : c.b.sides) {
    if (s.active != 0xFF && !s.pokemon[s.active].hp) unported("a faint");
  }
}

// ---------------------------------------------------------------------------
// Events on every active Pokémon and on the field.
// ---------------------------------------------------------------------------

// Battle.eachEvent (battle.ts:470): the event on each active Pokémon, in
// order of speed.
inline void each_event(Ctx& c, const EventNames& e) {
  std::array<Ref, 2> actives;
  const int n = all_active(c.b, actives);
  speed_sort(c, std::span<Ref>(actives.data(), size_t(n)),
             [&](Ref a, Ref b) { return double(c.b.at(b).speed) - double(c.b.at(a).speed); });
  for (int i = 0; i < n; i++) run_event(c, e, on(actives[i]));
}

// Battle.fieldEvent('SwitchIn', targets) (battle.ts:489).
inline void field_event_switch_in(Ctx& c, std::span<const Ref> targets) {
  const EventNames& e = events::SWITCH_IN;
  Listeners handlers;
  find_field_event_handlers(c, e.field, handlers);
  for (uint8_t s = 0; s < 2; s++) {
    // The side's conditions under onSideSwitchIn would be gathered here.
    const Side& side = c.b.sides[s];
    if (side.active == 0xFF) continue;
    const Ref active{s, side.active};
    find_pokemon_event_handlers(c, active, e.any, handlers);
    if (std::find(targets.begin(), targets.end(), active) == targets.end()) continue;
    find_pokemon_event_handlers(c, active, e.on, handlers);
    // Then the side's conditions under onSwitchIn, held by the Pokémon.
    find_field_event_handlers(c, e.on, handlers, on(active));
    find_battle_event_handlers(c, e.on, handlers, on(active));
  }
  speed_sort(c, handlers);
  for (int i = 0; i < handlers.n; i++) {
    const Listener& h = handlers.list[i];
    if (h.holder.kind == HolderKind::Pokemon && c.b.at(h.holder.pokemon).fainted) continue;
    // Showdown skips a handler whose effect was replaced since the handlers
    // were gathered; nothing replaces an ability or an item yet.
    Callback name = e.on;
    if (h.holder.kind == HolderKind::Side) name = e.side;
    if (h.holder.kind == HolderKind::Field) name = e.field;
    single_event(c, name, h.callback.effect, h.state, h.holder, {}, {}, relay::undefined, h.callback);
    resolve_faints(c);
    if (c.b.ended) return;
  }
}

// ---------------------------------------------------------------------------
// Switching in (battle-actions.ts).
// ---------------------------------------------------------------------------

// Battle.initEffectState (battle.ts:3381) for a Pokémon's ability or item:
// an effect of an active Pokémon takes the next effect order.
inline EffectState init_effect_state(Battle& b, bool has_id, bool target_active) {
  EffectState state;
  if (has_id && target_active) state.effect_order = b.effect_order++;
  return state;
}

// BattleActions.switchIn (battle-actions.ts:79) into an empty position.
inline void switch_in(Ctx& c, Ref r) {
  Side& side = c.b.sides[r.side];
  Pokemon& p = c.b.at(r);
  if (side.active != 0xFF) unported("a switch out");
  p.is_active = true;
  side.active = r.index;
  p.active_turns = 0;
  for (MoveSlot& slot : p.move_slots) slot.used = false;
  p.ability_state = init_effect_state(c.b, p.ability != Ability::None, true);
  p.item_state = init_effect_state(c.b, p.item != Item::None, true);
  run_event(c, events::BEFORE_SWITCH_IN, on(r));
  add(c, {part::text("switch"), part::pokemon(c.b, r), part::details(c.b, r)});
  p.previously_switched_in++;
  run_event(c, events::ENTRY_HAZARD, on(r));
  if (!p.hp) return;
  if (c.b.turn > 0) run_event(c, events::AFTER_ENTRY_HAZARD, on(r));
  insert_choice(c, {.choice = Choice::RunSwitch, .pokemon = r});
}

// BattleActions.runSwitch (battle-actions.ts:228): every Pokémon that came in
// together runs its switch-in handlers in one event, after the speeds of all
// the active Pokémon are put in order.
inline void run_switch(Ctx& c, Ref r) {
  std::array<Ref, 2> switchers{r};
  int n = 1;
  while (c.b.queue.size && c.b.queue.list[0].choice == Choice::RunSwitch) switchers[n++] = shift(c.b.queue).pokemon;
  std::array<Ref, 2> actives;
  const int count = all_active(c.b, actives, true);
  speed_sort(c, std::span<Ref>(actives.data(), size_t(count)),
             [&](Ref a, Ref b) { return double(c.b.at(b).speed) - double(c.b.at(a).speed); });
  c.b.speed_orders = uint8_t(count);
  for (int i = 0; i < count; i++) c.b.speed_order[i] = actives[i].side;
  field_event_switch_in(c, std::span<const Ref>(switchers.data(), size_t(n)));
  for (int i = 0; i < n; i++) {
    if (c.b.at(switchers[i]).hp) c.b.at(switchers[i]).is_started = true;
  }
}

// ---------------------------------------------------------------------------
// Requests.
// ---------------------------------------------------------------------------

// Battle.canSwitch (battle.ts): how many Pokémon behind the active one have
// not fainted.
inline int can_switch(const Battle& b, int side) {
  const Side& s = b.sides[side];
  if (!s.pokemon_left) return 0;
  int n = 0;
  for (int i = 0; i < s.size; i++) n += s.pokemon[i].position >= 1 && !s.pokemon[i].fainted;
  return n;
}

// Pokemon.getMoveRequestData (pokemon.ts:1104) with getMoves (pokemon.ts:985).
// It settles the Pokémon's maybe flags, so it runs when the request is made.
inline Request get_move_request_data(Ctx& c, Ref r) {
  Pokemon& p = c.b.at(r);
  Request request{.kind = Request::Kind::Move};
  // getLockedMove (pokemon.ts:970): a priority event, ended by any return.
  if (run_event(c, events::LOCK_MOVE, on(r), {}, {}, relay::undefined, false, true) != relay::yes) {
    unported("a locked move");
  }
  const bool can_switch_in = can_switch(c.b, r.side) > 0;
  bool has_valid_move = false;
  for (int i = 0; i < p.moves; i++) {
    const MoveSlot& slot = p.move_slots[i];
    Target target = data(slot.id).target;
    if (slot.id == Move::Curse && data(p.species).types[0] != Type::Ghost && data(p.species).types[1] != Type::Ghost) {
      target = Target::Self;
    }
    // A hidden disable is not shown to the last active Pokémon of its side,
    // which in singles is every Pokémon.
    const bool disabled = slot.disabled == Disabled::Yes || (slot.disabled == Disabled::No && slot.pp == 0);
    has_valid_move |= !disabled;
    request.moves[request.n_moves++] = {slot.id, slot.pp, slot.maxpp, target, disabled};
  }
  if (!has_valid_move) unported("a request with only Struggle");
  p.maybe_locked = p.maybe_locked || p.maybe_disabled;
  request.maybe_disabled = p.maybe_disabled;
  request.maybe_locked = p.maybe_locked;
  if (can_switch_in) {
    if (p.trapped) request.trapped = true;
    else if (p.maybe_trapped) request.maybe_trapped = true;
  }
  return request;
}

// Battle.makeRequest('move') (battle.ts:1391) with getRequests (battle.ts:1432).
inline void make_move_request(Ctx& c) {
  c.b.request_state = RequestState::Move;
  for (uint8_t s = 0; s < 2; s++) {
    const Side& side = c.b.sides[s];
    c.b.requests[s] = side.pokemon_left ? get_move_request_data(c, {s, side.active}) : Request{.kind = Request::Kind::Wait};
  }
}

// ---------------------------------------------------------------------------
// The turn.
// ---------------------------------------------------------------------------

// Battle.endTurn (battle.ts:1640).
inline void end_turn(Ctx& c) {
  Battle& b = c.b;
  b.turn++;
  for (uint8_t s = 0; s < 2; s++) {
    Side& side = b.sides[s];
    if (side.active == 0xFF) continue;
    const Ref r{s, side.active};
    Pokemon& p = b.at(r);
    p.newly_switched = false;
    p.maybe_disabled = false;
    p.maybe_locked = false;
    for (MoveSlot& slot : p.move_slots) slot.disabled = Disabled::No;
    run_event(c, events::DISABLE_MOVE, on(r));
    for (int i = 0; i < p.moves; i++) {
      // singleEvent('DisableMove', move): no move of Gen 3 has the handler.
      if (has(effect(p.move_slots[i].id), events::DISABLE_MOVE.on)) unported("a move's onDisableMove");
    }
    p.trapped = p.maybe_trapped = false;
    run_event(c, events::TRAP_POKEMON, on(r));
    // No type of Gen 3 is immune to trapping, so this always runs.
    run_event(c, events::MAYBE_TRAP_POKEMON, on(r));
    // Every ability the foe's species can have is asked whether it might
    // trap. Showdown means to skip the foe's real ability, but compares a
    // name with an id, which are never equal; so that one is asked too.
    const Side& other = b.sides[s ^ 1];
    if (other.active != 0xFF && other.pokemon[other.active].hp) {
      const Ref foe{uint8_t(s ^ 1), other.active};
      for (const Ability ability : data(b.at(foe).species).abilities) {
        if (ability == Ability::None) continue;
        if (ability == Ability::SandVeil || ability == Ability::Soundproof) continue; // banned in gen3ou
        single_event(c, events::ON_FOE_MAYBE_TRAP_POKEMON, effect(ability), nullptr, on(r), foe);
      }
    }
    if (p.fainted) continue;
    p.active_turns++;
  }
  // maybeTriggerEndlessBattleClause (battle.ts) does nothing through turn 100.
  if (b.turn > 100) unported("turn 101");
  add(c, {part::text("turn"), part::number(b.turn)});
  b.quick_claw_roll = random(b, c.out, 5) < 1; // randomChance(1, 5)
  make_move_request(c);
}

// Battle.runAction (battle.ts:2719).
inline void run_action(Ctx& c, const Action& action) {
  Battle& b = c.b;
  switch (action.choice) {
  case Choice::Start:
    for (int s = 0; s < 2; s++) {
      Side& side = b.sides[s];
      if (side.pokemon_left) side.pokemon_left = side.size;
      add(c, {part::text("teamsize"), part::side(s), part::number(side.size)});
    }
    add(c, {part::text("start")});
    // BattleStart on each species' own condition, and the format's and the
    // rules' onBattleStart: Gen 3 has none of them.
    for (uint8_t s = 0; s < 2; s++) {
      if (!b.sides[s].pokemon_left) unported("a side with no Pokémon");
      switch_in(c, {s, index_at(b.sides[s], 0)});
    }
    b.mid_turn = true;
    break;
  case Choice::RunSwitch: run_switch(c, action.pokemon); break;
  default: unported("an action other than start and runSwitch");
  }

  resolve_faints(c);
  if (b.ended) return;
  // With nothing queued Gen 3 looks for fainted Pokémon to replace
  // (checkFainted); nothing has fainted.
  // Gen 3 runs Update after every action but the start and a switch.
  if (action.choice != Choice::Start && action.choice != Choice::Switch && action.choice != Choice::InstaSwitch) {
    each_event(c, events::UPDATE);
  }
}

// Battle.turnLoop (battle.ts:2997). The clock's line is not written.
inline void turn_loop(Ctx& c) {
  Battle& b = c.b;
  add(c, {part::text("")});
  b.request_state = RequestState::None;
  if (!b.mid_turn) unported("a turn after the first");
  while (b.queue.size) {
    run_action(c, shift(b.queue));
    if (b.request_state != RequestState::None || b.ended) return;
  }
  end_turn(c);
  b.mid_turn = false;
  b.queue.size = 0;
}

// ---------------------------------------------------------------------------
// The beginning.
// ---------------------------------------------------------------------------

// Battle's constructor (battle.ts): the game type's line, and each rule that
// has a handler other than its onBegin and the team validator's is put on
// the field as a pseudo-weather (battle.ts:116).
inline void construct(Ctx& c) {
  add(c, {part::text("gametype"), part::text("singles")});
  for (int rule = 1; rule < RULES; rule++) {
    bool listens = false;
    for (const Handler& h : HANDLERS) {
      if (h.kind != EffectKind::Rule || h.id != rule || !h.handled) continue;
      const std::string_view name = CALLBACK_NAMES[uint8_t(h.callback)];
      listens |= name != "onBegin" && name != "onTeamPreview" && name != "onBattleStart" && name != "onValidateRule" &&
                 name != "onValidateTeam" && name != "onChangeSet" && name != "onValidateSet";
    }
    if (!listens) continue;
    // Field.addPseudoWeather (field.ts:186): a state with no target has
    // effect order 0. No rule has onFieldStart, and nothing listens for
    // PseudoWeatherChange.
    Field& f = c.b.field;
    f.pseudo_weather[f.pseudo_weathers++] = {effect(Rule(rule)), EffectState{}};
  }
}

// Battle.start (battle.ts:1922), once both players are set.
inline void start(Ctx& c) {
  Battle& b = c.b;
  b.started = true;
  add(c, {part::text("gen"), part::number(3)});
  add(c, {part::text("tier"), part::text("[Gen 3] OU")});
  for (int rule = 1; rule < RULES; rule++) {
    single_event(c, events::ON_BEGIN, effect(Rule(rule)), nullptr, BATTLE);
  }
  // runPickTeam: Gen 3 has no team preview.
  add_choice(c, {.choice = Choice::Start});
  b.mid_turn = true;
  turn_loop(c);
}

// Battle.setPlayer (battle.ts): the side, its line, and the start when both
// are set.
inline void set_player_and_start(Ctx& c, int side, const Team& team) {
  set_player(c.b, c.out, side, team);
  c.b.sides[side].pokemon_left = team.size;
  add(c, {part::text("player"), part::side(side), part::player(side), part::text(""), part::text("")});
  if (side == 1) start(c);
}

} // namespace adv
