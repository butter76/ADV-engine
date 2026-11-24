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
inline constexpr EventNames BEFORE_TURN = event("BeforeTurn");
inline constexpr EventNames RESIDUAL = event("Residual");
inline constexpr EventNames SWITCH_OUT = event("SwitchOut");
inline constexpr EventNames BEFORE_SWITCH_OUT = event("BeforeSwitchOut");
inline constexpr EventNames FAINT = event("Faint");
inline constexpr Callback ON_END = callback("", "End");
inline constexpr Callback ON_BEGIN = callback("", "Begin");
inline constexpr Callback ON_FOE_MAYBE_TRAP_POKEMON = callback("Foe", "MaybeTrapPokemon");
} // namespace events

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
  case Choice::BeforeTurnMove: a.order = 5; break;
  case Choice::RunSwitch: a.order = 101; break;
  case Choice::Switch: a.order = 103; break;
  case Choice::Residual: a.order = 300; break;
  case Choice::Move:
    a.order = 200;
    // Battle.getActionSpeed for a move: its priority, which no plain move
    // and nothing in Gen 3 changes (ModifyPriority, FractionalPriority).
    if (has(effect(a.move), callback("", "ModifyPriority"))) unported("a move's onModifyPriority");
    a.priority = data(a.move).priority;
    break;
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
// BattleQueue.addChoice (battle-queue.ts:307). A move with a
// beforeTurnCallback comes with an action that runs it before the turn's
// moves.
inline void add_choice(Ctx& c, Action a) {
  if (a.choice == Choice::Move && has(effect(a.move), Callback::beforeTurnCallback)) {
    push(c.b.queue, resolve_action(c, {.choice = Choice::BeforeTurnMove, .pokemon = a.pokemon, .move = a.move}), c.b.queue.size);
  }
  push(c.b.queue, resolve_action(c, a), c.b.queue.size);
}

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

// BattleQueue.cancelAction (battle-queue.ts:334): a Pokémon's queued
// actions, but for a runSwitch.
inline void cancel_action(Queue& q, Ref r) {
  int kept = 0;
  for (int i = 0; i < q.size; i++) {
    if (q.list[i].pokemon == r && q.list[i].choice != Choice::RunSwitch) continue;
    q.list[kept++] = q.list[i];
  }
  q.size = uint8_t(kept);
}

// ---------------------------------------------------------------------------
// Faints and the end of the battle.
// ---------------------------------------------------------------------------

// Pokemon.clearVolatile (pokemon.ts:1530). Setting the species again puts the
// stored stats back and the speed to the stored Speed.
inline void clear_volatile(Pokemon& p, bool include_switch_flags = true) {
  p.boosts = Boosts{};
  p.ability = p.base_ability;
  p.n_volatiles = 0;
  if (include_switch_flags) p.switch_flag = p.force_switch_flag = false;
  p.newly_switched = true;
  p.being_called_back = false;
  set_species(p, p.base_species);
}

// Battle.win (battle.ts): -1 is a tie.
inline void win(Ctx& c, int side) {
  if (c.b.ended) return;
  c.b.winner = side < 0 ? int8_t(2) : int8_t(side);
  add(c, {part::text("")});
  if (side < 0) add(c, {part::text("tie")});
  else add(c, {part::text("win"), part::player(side)});
  c.b.ended = true;
  c.b.request_state = RequestState::None;
  for (Request& r : c.b.requests) r = Request{};
}
// Battle.checkWin (battle.ts:2660).
inline bool check_win(Ctx& c) {
  Battle& b = c.b;
  if (!b.sides[0].pokemon_left && !b.sides[1].pokemon_left) return win(c, -1), true;
  for (int s = 0; s < 2; s++) {
    if (!b.sides[s ^ 1].pokemon_left) return win(c, s), true;
  }
  return false;
}

// Battle.resolveFaints (battle.ts:2561), as runAction calls it: the queued
// faints in order, and then whether the battle is over.
inline bool resolve_faints(Ctx& c) {
  Battle& b = c.b;
  if (b.ended) return false;
  if (!b.faints) return check_win(c);
  while (b.faints) {
    const Battle::Faint f = b.faint_queue[0];
    for (int i = 1; i < b.faints; i++) b.faint_queue[i - 1] = b.faint_queue[i];
    b.faints--;
    Pokemon& p = b.at(f.target);
    if (p.fainted) continue;
    // Nothing listens for BeforeFaint in Gen 3. A move has run Faint already.
    if (!f.faint_event_done) run_event(c, events::FAINT, on(f.target), f.source, f.effect);
    add(c, {part::text("faint"), part::pokemon(b, f.target)});
    if (b.sides[f.target.side].pokemon_left) b.sides[f.target.side].pokemon_left--;
    single_event(c, events::ON_END, effect(p.ability), &p.ability_state, on(f.target));
    single_event(c, events::ON_END, effect(p.item), &p.item_state, on(f.target));
    clear_volatile(p, false);
    p.fainted = true;
    p.is_active = false;
    p.is_started = false;
  }
  // In Gen 3 singles a faint cancels what the Pokémon still out had queued.
  std::array<Ref, 2> actives;
  const int n = all_active(b, actives);
  for (int i = 0; i < n; i++) cancel_action(b.queue, actives[i]);
  return check_win(c);
  // Nothing listens for AfterFaint in Gen 3.
}

// ---------------------------------------------------------------------------
// Events on the field.
// ---------------------------------------------------------------------------

// Battle.fieldEvent (battle.ts:489) for SwitchIn, with the Pokémon that
// switched in, and for Residual. For Residual an effect with a duration is
// gathered whether or not it has a handler, and counted down.
inline void field_event(Ctx& c, const EventNames& e, std::span<const Ref> targets = {}) {
  const bool residual = e.id == "Residual", switch_in = e.id == "SwitchIn";
  Listeners handlers;
  find_field_event_handlers(c, e.field, handlers, FIELD, residual);
  for (uint8_t s = 0; s < 2; s++) {
    find_side_event_handlers(c, s, e.side, handlers, {}, residual);
    const Side& side = c.b.sides[s];
    if (side.active == 0xFF) continue;
    const Ref active{s, side.active};
    if (switch_in) find_pokemon_event_handlers(c, active, e.any, handlers);
    if (!targets.empty() && std::find(targets.begin(), targets.end(), active) == targets.end()) continue;
    find_pokemon_event_handlers(c, active, e.on, handlers, {}, residual);
    find_side_event_handlers(c, s, e.on, handlers, on(active));
    find_field_event_handlers(c, e.on, handlers, on(active));
    find_battle_event_handlers(c, e.on, handlers, on(active));
  }
  speed_sort(c, handlers);
  for (int i = 0; i < handlers.n; i++) {
    const Listener& h = handlers.list[i];
    if (h.holder.kind == HolderKind::Pokemon && c.b.at(h.holder.pokemon).fainted && !h.is_slot_condition) continue;
    const Effect he = h.callback.effect;
    if (residual && h.ends && h.state && h.state->duration) {
      h.state->duration--;
      if (!h.state->duration) {
        // The effect ends: a volatile is removed, the weather cleared.
        if (h.is_volatile) {
          if (Volatile* v = volatile_of(c.b.at(h.holder.pokemon), Condition(he.id)); v && &v->state == h.state) {
            remove_volatile(c, h.holder.pokemon, Condition(he.id));
          }
        } else if (h.is_slot_condition) {
          if (SideCondition* sc = slot_condition_of(c.b.sides[h.side], Condition(he.id)); sc && &sc->state == h.state) {
            remove_slot_condition(c, h.side, Condition(he.id));
          }
        } else if (h.is_side_condition) {
          if (SideCondition* sc = side_condition_of(c.b.sides[h.side], Condition(he.id)); sc && &sc->state == h.state) {
            remove_side_condition(c, h.side, Condition(he.id));
          }
        } else if (h.state == &c.b.field.weather_state) {
          clear_weather(c);
        } else {
          unported("an effect other than a volatile or the weather running out");
        }
        if (c.b.ended) return;
        continue;
      }
    }
    // A handler whose effect is gone since the handlers were gathered is
    // skipped. Only volatiles are removed so far.
    if (h.is_volatile) {
      const Volatile* v = volatile_of(c.b.at(h.holder.pokemon), Condition(he.id));
      if (!v || &v->state != h.state) continue;
    }
    if (h.is_side_condition) {
      const SideCondition* sc = side_condition_of(c.b.sides[h.side], Condition(he.id));
      if (!sc || &sc->state != h.state) continue;
    }
    if (!h.callback) continue;
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

// BattleActions.runPursuitActivation (battle-actions.ts:62). No Pursuit can
// be queued yet, so only its event runs.
inline void run_pursuit_activation(Ctx& c, Ref r) {
  run_event(c, events::BEFORE_SWITCH_OUT, on(r));
}

inline void run_switch(Ctx& c, Ref r);

// BattleActions.switchIn (battle-actions.ts:79): `r` comes in at position 0,
// in place of whatever is there. A Pokémon dragged in is not chased by
// Pursuit, is written as `drag`, and runs its switch-in at once.
inline bool switch_in(Ctx& c, Ref r, bool is_drag = false) {
  Side& side = c.b.sides[r.side];
  Pokemon& p = c.b.at(r);
  if (p.is_active) unported("a switch to a Pokémon that is already in");
  if (side.active != 0xFF) {
    const Ref old_ref{r.side, side.active};
    Pokemon& old = c.b.at(old_ref);
    if (old.hp) {
      old.being_called_back = true;
      if (!is_drag) run_pursuit_activation(c, old_ref);
      if (!run_event(c, events::SWITCH_OUT, on(old_ref)).truthy()) return false;
      if (!old.hp) unported("a Pokémon fainting as it is called back");
      single_event(c, events::ON_END, effect(old.ability), &old.ability_state, on(old_ref));
      single_event(c, events::ON_END, effect(old.item), &old.item_state, on(old_ref));
      cancel_action(c.b.queue, old_ref);
      clear_volatile(old);
    }
    old.is_active = false;
    old.is_started = false;
    old.stats_raised_this_turn = old.stats_lowered_this_turn = false;
    // The two trade places in side.pokemon. A fainted Pokémon's `fnt` status
    // is cleared.
    old.position = p.position;
    old.status_fnt = false;
    p.position = 0;
  }
  p.is_active = true;
  side.active = r.index;
  p.active_turns = 0;
  for (MoveSlot& slot : p.move_slots) slot.used = false;
  // In Gen 3 an item knocked off is lost again at every switch-in.
  if (p.item_knocked_off && p.item != Item::None) unported("a Pokémon that lost its item to Knock Off holding one again");
  p.ability_state = init_effect_state(c.b, p.ability != Ability::None, r);
  p.item_state = init_effect_state(c.b, p.item != Item::None, r);
  run_event(c, events::BEFORE_SWITCH_IN, on(r));
  add(c, {part::text(is_drag ? "drag" : "switch"), part::pokemon(c.b, r), part::details(c.b, r)});
  p.previously_switched_in++;
  run_event(c, events::ENTRY_HAZARD, on(r));
  if (!p.hp) return false;
  if (c.b.turn > 0) run_event(c, events::AFTER_ENTRY_HAZARD, on(r));
  if (is_drag) run_switch(c, r);
  else insert_choice(c, {.choice = Choice::RunSwitch, .pokemon = r});
  return true;
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
  field_event(c, events::SWITCH_IN, std::span<const Ref>(switchers.data(), size_t(n)));
  for (int i = 0; i < n; i++) {
    if (c.b.at(switchers[i]).hp) c.b.at(switchers[i]).is_started = true;
  }
}

// ---------------------------------------------------------------------------
// Requests.
// ---------------------------------------------------------------------------

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
    if (p.trapped == Trapped::Yes) request.trapped = true;
    else if (p.maybe_trapped) request.maybe_trapped = true;
  }
  return request;
}

// Side.clearChoice (side.ts): for a switch request, how many of the side's
// Pokémon must be replaced and can be.
inline void clear_choice(Ctx& c, int s) {
  Side& side = c.b.sides[s];
  side.choice = ChoiceState{};
  if (c.b.request_state == RequestState::Switch) {
    const int out = side.active != 0xFF && side.pokemon[side.active].switch_flag ? 1 : 0;
    const int forced = std::min(out, can_switch(c.b, s));
    side.choice.forced_switches_left = uint8_t(forced);
    side.choice.forced_passes_left = uint8_t(out - forced);
  }
  for (int i = 0; i < c.b.queue.size; i++) {
    const Action& a = c.b.queue.list[i];
    if ((a.choice == Choice::Switch || a.choice == Choice::InstaSwitch) && a.pokemon.side == s) {
      unported("a request while the side has a switch queued");
    }
  }
}

// Battle.makeRequest (battle.ts:1391) with getRequests (battle.ts:1432).
inline void make_request(Ctx& c, RequestState type) {
  Battle& b = c.b;
  b.request_state = type;
  for (int s = 0; s < 2; s++) clear_choice(c, s);
  int asked = 0;
  for (uint8_t s = 0; s < 2; s++) {
    const Side& side = b.sides[s];
    b.requests[s] = Request{.kind = Request::Kind::Wait};
    if (!side.pokemon_left) continue;
    if (type == RequestState::Move) {
      b.requests[s] = get_move_request_data(c, {s, side.active});
    } else if (side.active != 0xFF && side.pokemon[side.active].switch_flag) {
      b.requests[s] = Request{.kind = Request::Kind::Switch};
    }
    asked += b.requests[s].kind != Request::Kind::Wait;
  }
  // A request can be cancelled only under Cancel Mod, and only when both
  // sides were asked.
  for (Request& r : b.requests) {
    if (r.kind != Request::Kind::Wait && (!b.support_cancel || asked < 2)) r.no_cancel = true;
  }
  b.sent_requests = false;
}

// Side.requestState (side.ts): what the side was asked for.
inline RequestState request_state(const Battle& b, int s) {
  switch (b.requests[s].kind) {
  case Request::Kind::Move: return RequestState::Move;
  case Request::Kind::Switch: return RequestState::Switch;
  case Request::Kind::None:
  case Request::Kind::Wait: break;
  }
  return RequestState::None;
}

// Side.isChoiceDone (side.ts) in singles. A side whose one Pokémon has
// nothing to do passes by itself (getChoiceIndex).
inline bool is_choice_done(Battle& b, int s) {
  Side& side = b.sides[s];
  const RequestState state = request_state(b, s);
  if (state == RequestState::None) return true;
  if (side.choice.forced_switches_left) return false;
  if (side.choice.action == ChoiceState::Kind::None) {
    const Pokemon& p = side.pokemon[side.active];
    const bool idle = state == RequestState::Move ? p.fainted : !p.switch_flag;
    if (idle) side.choice.action = ChoiceState::Kind::Pass;
  }
  return side.choice.action != ChoiceState::Kind::None;
}

// What a refused choice did: nothing, or changed the request, which is then
// sent again (Side.emitChoiceError).
enum class Refusal : uint8_t { None, Invalid, Unavailable };

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
    if (b.turn != 1) p.stats_raised_this_turn = p.stats_lowered_this_turn = false;
    p.maybe_disabled = false;
    p.maybe_locked = false;
    for (MoveSlot& slot : p.move_slots) slot.disabled = Disabled::No;
    run_event(c, events::DISABLE_MOVE, on(r));
    for (int i = 0; i < p.moves; i++) {
      // singleEvent('DisableMove', move): no move of Gen 3 has the handler.
      if (has(effect(p.move_slots[i].id), events::DISABLE_MOVE.on)) unported("a move's onDisableMove");
    }
    p.trapped = Trapped::No;
    p.maybe_trapped = false;
    run_event(c, events::TRAP_POKEMON, on(r));
    // No type of Gen 3 is immune to trapping, so this always runs.
    run_event(c, events::MAYBE_TRAP_POKEMON, on(r));
    // Every ability the foe's species can have is asked whether it might
    // trap. Showdown means to skip the foe's real ability, but compares a
    // name with an id, which are never equal; so that one is asked too.
    if (const Ref foe = foe_of(b, r)) {
      for (const Ability ability : data(b.at(foe).species).abilities) {
        if (ability == Ability::None) continue;
        if (ability == Ability::SandVeil || ability == Ability::Soundproof) continue; // banned in gen3ou
        single_event(c, events::ON_FOE_MAYBE_TRAP_POKEMON, effect(ability), nullptr, on(r), foe);
      }
    }
    if (p.fainted) continue;
    p.active_turns++;
  }
  for (Side& side : b.sides) {
    for (int i = 0; i < side.size; i++) compact_volatiles(side.pokemon[i]);
    compact_side_conditions(side);
  }
  // maybeTriggerEndlessBattleClause (battle.ts) does nothing through turn 100.
  if (b.turn > 100) unported("turn 101");
  add(c, {part::text("turn"), part::number(b.turn)});
  b.quick_claw_roll = random(b, c.out, 5) < 1; // randomChance(1, 5)
  make_request(c, RequestState::Move);
}

// Gen 3's Battle.checkFainted (data/mods/gen3/scripts.ts:16): a fainted
// Pokémon whose side can replace it is flagged to be switched.
inline void check_fainted(Ctx& c) {
  for (int s = 0; s < 2; s++) {
    Side& side = c.b.sides[s];
    if (side.active == 0xFF || !can_switch(c.b, s)) continue;
    Pokemon& p = side.pokemon[side.active];
    if (!p.fainted) continue;
    // One whose switch is already queued is left alone.
    bool queued = false;
    for (int i = 0; i < c.b.queue.size; i++) {
      const Action& a = c.b.queue.list[i];
      queued |= (a.choice == Choice::Switch || a.choice == Choice::InstaSwitch) && a.pokemon == Ref{uint8_t(s), side.active};
    }
    if (queued) continue;
    p.status_fnt = true;
    p.switch_flag = true;
  }
}

// Battle.runAction (battle.ts:2719). True when the turn stops for a request
// or the battle's end.
inline bool run_action(Ctx& c, const Action& action) {
  Battle& b = c.b;
  const bool is_switch = action.choice == Choice::Switch || action.choice == Choice::InstaSwitch;
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
  case Choice::Switch:
    // CheckShow is run on Natural Cure for a Pokémon with a status; it has
    // no handler in Gen 3.
    switch_in(c, action.target);
    break;
  case Choice::InstaSwitch: switch_in(c, action.target); break;
  case Choice::RunSwitch: run_switch(c, action.pokemon); break;
  case Choice::BeforeTurn: each_event(c, events::BEFORE_TURN); break;
  case Choice::BeforeTurnMove: {
    // The move's beforeTurnCallback(pokemon, target).
    if (!b.at(action.pokemon).is_active || b.at(action.pokemon).fainted) return false;
    const Ref foe{uint8_t(action.pokemon.side ^ 1), b.sides[action.pokemon.side ^ 1].active};
    call(c, {effect(action.move), Callback::beforeTurnCallback}, relay::undefined, on(action.pokemon), foe, effect(action.move));
    break;
  }
  case Choice::Residual:
    add(c, {part::text("")});
    for (uint8_t s = 0; s < 2; s++) {
      if (b.sides[s].active != 0xFF && !b.sides[s].pokemon[b.sides[s].active].fainted) update_speed(c, {s, b.sides[s].active});
    }
    field_event(c, events::RESIDUAL);
    if (!b.ended) add(c, {part::text("upkeep")});
    break;
  case Choice::Move:
    // A Pokémon that left or fainted does not move, and nothing follows.
    if (!b.at(action.pokemon).is_active || b.at(action.pokemon).fainted) return false;
    run_move(c, action.move, action.pokemon);
    break;
  }
  b.has_active_move = false; // clearActiveMove

  // A Pokémon flagged by Roar or Whirlwind is dragged out now
  // (BattleActions.dragIn): another of its side comes in, drawn from those
  // that can, even when there is only one.
  for (uint8_t s = 0; s < 2; s++) {
    Side& side = b.sides[s];
    if (side.active == 0xFF || !side.pokemon[side.active].force_switch_flag) continue;
    Pokemon& out_ = side.pokemon[side.active];
    if (out_.hp) {
      const int choices = can_switch(b, s);
      if (choices) {
        const int pick = int(random(b, c.out, uint32_t(choices)));
        int position = 0;
        for (int seen = -1; seen < pick;) {
          position++;
          if (!side.pokemon[index_at(side, position)].fainted) seen++;
        }
        const Ref in{s, index_at(side, position)};
        if (run_event(c, event("DragOut"), on(Ref{s, side.active})).truthy()) switch_in(c, in, true);
      }
    }
    out_.force_switch_flag = false;
  }

  resolve_faints(c);
  if (b.ended) return true;

  // Gen 3 looks for fainted Pokémon to replace when nothing is queued, after
  // a switch, and before a move or the residual step.
  const bool queued = b.queue.size > 0;
  const Choice next = queued ? b.queue.list[0].choice : Choice::Start;
  if (!queued || is_switch || next == Choice::Move || next == Choice::Residual) check_fainted(c);

  bool switches = false;
  for (int s = 0; s < 2; s++) {
    Side& side = b.sides[s];
    if (side.active == 0xFF || !side.pokemon[side.active].switch_flag) continue;
    if (!can_switch(b, s)) side.pokemon[side.active].switch_flag = false;
    else if (side.pokemon[side.active].hp) unported("a Pokémon with HP flagged to switch");
    else switches = true;
  }
  if (switches) {
    make_request(c, RequestState::Switch);
    return true;
  }
  // Gen 3 runs Update after every action but the start and a switch.
  if (action.choice != Choice::Start && !is_switch) each_event(c, events::UPDATE);
  return false;
}

// Battle.turnLoop (battle.ts:2997). The clock's line is not written.
inline void turn_loop(Ctx& c) {
  Battle& b = c.b;
  add(c, {part::text("")});
  b.request_state = RequestState::None;
  if (!b.mid_turn) {
    insert_choice(c, {.choice = Choice::BeforeTurn});
    add_choice(c, {.choice = Choice::Residual});
    b.mid_turn = true;
  }
  while (b.queue.size) {
    run_action(c, shift(b.queue));
    if (b.request_state != RequestState::None || b.ended) return;
  }
  end_turn(c);
  b.mid_turn = false;
  b.queue.size = 0;
}

// ---------------------------------------------------------------------------
// Choices (side.ts, battle.ts).
// ---------------------------------------------------------------------------

// Battle.commitChoices (battle.ts): the chosen actions are queued and sorted,
// ahead of whatever the turn still had queued, and the turn goes on.
inline void commit_choices(Ctx& c) {
  Battle& b = c.b;
  for (uint8_t s = 0; s < 2; s++) {
    if (b.sides[s].active != 0xFF && !b.sides[s].pokemon[b.sides[s].active].fainted) update_speed(c, {s, b.sides[s].active});
  }
  const Queue old = b.queue;
  b.queue.size = 0;
  for (uint8_t s = 0; s < 2; s++) {
    Side& side = b.sides[s];
    const ChoiceState choice = side.choice;
    if (choice.action == ChoiceState::Kind::Switch || choice.action == ChoiceState::Kind::InstaSwitch) {
      const Ref out{s, side.active};
      // resolveAction: a switch clears the flag that asked for it.
      b.at(out).switch_flag = false;
      add_choice(c, {.choice = choice.action == ChoiceState::Kind::Switch ? Choice::Switch : Choice::InstaSwitch,
                     .pokemon = out, .target = {s, choice.target}});
    } else if (choice.action == ChoiceState::Kind::Move) {
      add_choice(c, {.choice = Choice::Move, .pokemon = {s, side.active}, .move = choice.move});
    }
  }
  // clearRequest.
  b.request_state = RequestState::None;
  for (int s = 0; s < 2; s++) {
    b.requests[s] = Request{};
    b.sides[s].choice = ChoiceState{};
  }
  speed_sort(c, std::span<Action>(b.queue.list.data(), b.queue.size),
             [](const Action& a, const Action& b2) { return compare_priority(a, b2); });
  for (int i = 0; i < old.size; i++) push(b.queue, old.list[i], b.queue.size);
  turn_loop(c);
}

// Side.chooseSwitch (side.ts) to the Pokémon at a position of the side's
// list, and then Battle.choose: once both sides are done the turn runs.
// `position` is -1 for the first Pokémon that can come in, as a default
// choice makes for a switch request.
inline Refusal choose_switch(Ctx& c, int s, int position) {
  Battle& b = c.b;
  Side& side = b.sides[s];
  const RequestState state = request_state(b, s);
  if (state == RequestState::None) unported("a choice from a side that was not asked");
  if (side.choice.cant_undo) unported("a choice after one that cannot be undone");
  clear_choice(c, s);
  if (position < 0) {
    if (state != RequestState::Switch) unported("a default choice for a move request");
    for (position = 1; position < side.size && side.pokemon[index_at(side, position)].fainted;) position++;
  }
  if (position < 1 || position >= side.size) unported("a switch to a position that cannot be switched to");
  const uint8_t target = index_at(side, position);
  if (side.pokemon[target].fainted) unported("a switch to a fainted Pokémon");
  Pokemon& active = side.pokemon[side.active];
  if (state == RequestState::Move) {
    if (active.trapped != Trapped::No) {
      // The request is corrected the first time, and the choice is merely
      // invalid after that.
      Request& r = b.requests[s];
      side.choice.error = true;
      if (!r.maybe_trapped && r.trapped) return Refusal::Invalid;
      r.maybe_trapped = false;
      r.trapped = true;
      r.update = true;
      return Refusal::Unavailable;
    }
    if (active.maybe_trapped) side.choice.cant_undo = true;
    side.choice.action = ChoiceState::Kind::Switch;
  } else {
    side.choice.forced_switches_left--;
    side.choice.action = ChoiceState::Kind::InstaSwitch;
  }
  side.choice.target = target;
  if (is_choice_done(b, 0) && is_choice_done(b, 1)) commit_choices(c);
  return Refusal::None;
}

// Side.chooseMove (side.ts) for the move in a slot of the request, with no
// target given, as singles needs none.
inline Refusal choose_move(Ctx& c, int s, int slot) {
  Battle& b = c.b;
  Side& side = b.sides[s];
  if (request_state(b, s) != RequestState::Move) unported("a move chosen for a request that is not for a move");
  if (side.choice.cant_undo) unported("a choice after one that cannot be undone");
  clear_choice(c, s);
  const Ref r{uint8_t(s), side.active};
  // chooseMove makes the request's data again, and asks twice more whether
  // the Pokémon is locked into a move.
  const Request request = get_move_request_data(c, r);
  // A default choice is the first move that is not disabled.
  for (int i = 0; slot < 0 && i < request.n_moves; i++) {
    if (!request.moves[i].disabled) slot = i;
  }
  if (slot < 0 || slot >= request.n_moves) unported("a move the Pokémon does not have");
  for (int i = 0; i < 2; i++) {
    if (run_event(c, events::LOCK_MOVE, on(r), {}, {}, relay::undefined, false, true) != relay::yes) unported("a locked move");
  }
  if (request.moves[slot].disabled) unported("a disabled move chosen");
  side.choice.action = ChoiceState::Kind::Move;
  side.choice.move = request.moves[slot].id;
  if (b.at(r).maybe_disabled) side.choice.cant_undo = true;
  if (is_choice_done(b, 0) && is_choice_done(b, 1)) commit_choices(c);
  return Refusal::None;
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
