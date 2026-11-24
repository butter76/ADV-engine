# Plan

A Gen 3 singles battle engine in C++ that is a port of Pokémon Showdown at
commit `a78551b070769eb22e5261bb220d21153bf5788c` (2026-11-09, the commit
pkmn.ai's ladder runs). Given the same seed, teams and choices it must make
the same random draws in the same order and write the same log and the same
requests, bugs included.

Status (2026-11-23): steps 1 and 2 of "Order of work" are done and step 3,
widening to every move, is most of the way for what OU teams use. Over 1,000
fresh recorded games of up to 99 turns with any choice at all, 1,443,475
lines compare equal and none differ; 671 run to their end or their 99th turn
and the rest stop at something not ported, most often Sleep Talk or Beat Up.

## What was measured

All on the pinned Showdown, 1,000 `gen3ou` games on 320 OU teams with
uniformly random choices (`tools/census.cjs`), one core of an Apple Silicon
Mac. An update is one step of the battle after both players choose.

| | |
|---|---|
| Showdown's speed | 4,150 updates/s, 40 games/s |
| Draws per update | 3.6, from 41 call sites |
| `runEvent` dispatches per update | 88, of which 96% find no handler |
| `singleEvent` calls per update | 27, of which 3.2 have a callback |
| Handler lists sorted per update | 84, of which 8 have two or more entries |
| Draws spent shuffling tied handlers | 0.22 per update |
| Dispatches for events nothing in Gen 3 listens to | at least 23% |

What has to be ported (`tools/dump_dex.cjs`, the checklist in
`build/dex/callbacks.json`):

| | Entries | With handlers | Handlers | Distinct lines |
|---|---|---|---|---|
| Moves | 370 | 130 | 301 | 1,916 |
| Abilities | 76 | 70 | 104 | 594 |
| Items | 106 | 80 | 117 | 524 |
| Conditions | 36 | 32 | 109 | 639 |
| Format and rules (`gen3ou`) | 24 | 17 | 22 | 256 |
| Gen 3's own overrides of the simulator | | | 9 | 481 |
| The simulator's core (all generations, doubles included) | | | | 10,175 |

- **29 handlers are constants, not functions.** Showdown accepts
  `onLockMove: 'bide'` or `onTry: false` where it accepts a function, and a
  constant is gathered, ranked and sorted like one.
- **Inheritance is resolved by the dump.** 395 of the functions come from
  Showdown's base data unchanged, 102 from the Gen 3 files and 77 from Gen
  4's; the port never reads a mod chain.
- **OU games reach little of the checklist.** In 200 games 238 of 624
  functions ran (38%, `tools/coverage.cjs`). Zero differences on OU teams
  says nothing about the other 62%.

Draws that change nothing but must still be made:

- **Quick Claw is rolled at the end of every turn**, held or not
  (`sim/battle.ts`, `endTurn`). It is a quarter of all draws.
- **Every status set shuffles two rules.** Sleep Clause Mod and Freeze Clause
  Mod both listen to `SetStatus` with equal priority, so `speedSort` draws
  once each time.
- **`Update` runs 4.3 times per update** and sorts the active Pokémon by
  Speed each time; on a Speed tie each one draws.
- **A Pokémon with no gender in its set draws one** when the battle is built,
  before anything is logged.

## Design

1. **The structure is Showdown's.** A queue of actions, `runEvent`,
   `singleEvent`, `fieldEvent` and `eachEvent`, and one function per Showdown
   callback. Flattening an event into straight-line code is not allowed
   unless the event has no listener in Gen 3 at all.
2. **Data is generated, callbacks are written by hand.** The tables in
   `src/data/` come from `tools/gen_tables.cjs`. The checklist's entries are
   each ported, or shown unreachable in Gen 3 singles; an entry's source hash
   pins what was ported.
3. **Gathering is in Showdown's order.** A tie's shuffle depends on the list
   as gathered: status, volatiles in the order they were added, ability,
   item, species, slot conditions; then the prefixed names (`onAlly`,
   `onFoe`, `onAny`, `onSource`), the sides, the field (rules are
   pseudo-weathers there), the format. Volatiles therefore live in an
   ordered list, and effects carry Showdown's `effectOrder`.
4. **Sorting is Showdown's.** `speedSort` with its selection sort and
   shuffle; a stable sort without draws for `Invulnerability`, `TryHit`,
   `DamagingHit` and `EntryHazard`, and for redirection.
5. **A listener test comes first.** Every handler name is a bit (Gen 3 has
   110). An effect has a constant mask; a Pokémon, side or field keeps the OR
   of its effects' masks. An event tests those before gathering. Measured on
   the real event mix (`smoke/dispatch_bench.cpp`, handlers that do nothing):
   about 330 ns per update with the test, 1,400 ns without.
6. **Numbers are JavaScript's.** `double` where Showdown multiplies by a
   fraction, 32-bit truncation where it calls `trunc`, compiled with
   `-ffp-contract=off`.
7. **The PRNG is the Gen 5 LCG** (`src/prng.hpp`), which Showdown uses for a
   `gen5,` seed.
8. **The battle is a plain copyable struct**: no pointers, no allocation in
   `update`.
9. **The log is binary; text is a separate formatter.** The formatter writes
   what Showdown sends (log lines with their `|split|` lines, requests,
   errors) and is what the tests compare.
10. **Scope:** Gen 3 singles, `gen3ou` and a custom game with no validation.
    No doubles, no other generation, no team validator.

## Testing

`tools/trace.cjs` plays seeded Showdown games through `battle.choose`, as a
client would, and records in order every draw (arguments, result, and how
many log lines were written before it), every choice as tried, and everything
Showdown sent. The line format is at `record` in `tools/lib.cjs`. Clock lines
(`|t:|`) are left out. Two runs give the same bytes.

The engine replays a trace through `src/showdown/stream.hpp`, which drives it
as Showdown's stream is driven and writes the same lines; `tests/replay.cpp`
compares the two line by line. So one comparison covers:

1. **Draws**, each with its arguments, its result and how many log lines
   had been written when it was made. The first that differs names the
   divergence.
2. **Log text**, as sent.
3. **Requests and errors**, including refused choices.

The bar is zero differences over millions of games in three environments: the
OU teams, random legal teams, and random anything. Alongside it every
function of the checklist must run at least once in the recorded games
(`tools/coverage.cjs`); one the random games never reach gets a written
scenario.

What is tested today (`ctest`, no Node needed):

| Test | What it shows |
|---|---|
| `prng` | 2,000,000 mixed calls give the hash Showdown's PRNG gives (`tools/prng_ref.cjs`) |
| `trace_draws` | all 20,124 draws of 50 recorded games replay from their seeds |
| `replay_openings` | 200 games up to the first choice, line for line |
| `replay_switches` | 30 games of 30 turns in which both players switch whenever they can; 9 run to the end and 21 stop at something unported, with no line different before |
| `replay_plain_moves` | 20 games of 40 turns of plain damaging moves and switches, line for line until each ends or stops |
| `replay_any` | 60 games of 12 turns with any choice at all, line for line until each ends or stops |
| `replay_regressions` | two whole games that once differed |
| `data` | the tables state the same 1,834 facts as Showdown (`tools/tables_ref.cjs`, written apart from the generator) |

## Order of work

1. ~~The trace recorder, the generated C++ tables, and a count of how often
   each Showdown callback runs.~~ Done.
2. A skeleton that plays whole games on a restricted set: team setup and
   stats, switching, the queue, the turn loop, requests, plain damaging
   moves. The recorder generates teams from what is implemented. In slices,
   each compared with recorded games before the next starts:
   1. ~~Team setup: packed teams, stats, Hidden Power, genders, PP, and the
      `side` object of the first request.~~ Done: 200 games, 2,400 Pokémon,
      1,241 draws, 400 sides, no difference (`tests/replay.cpp`).
   2. ~~The battle's start, up to the first request: events, the queue,
      switching in, the log and its text, weather, boosts and trapping.~~
      Done: 3,200 recorded openings, no difference.
   3. ~~Turns of switches: choices and their refusals, switching out, the
      end-of-turn step, damage and healing from effects, fainting, forced
      switches, the win.~~ Done as far as games without moves go: over 150
      games of up to 100 turns, 158,321 lines compared and equal, none
      different. Every one of those games ends by stopping: 78 where a
      trapped Pokémon has to move, 52 at turn 101 (the Endless Battle Clause
      code), the rest at Trace, Cloud Nine, Flash Fire, Truant or a berry
      being eaten. Only 2 of them reach a faint before stopping, so fainting
      and forced switches are compared but thinly.
   4. ~~Moves, starting with plain damaging ones.~~ Done for the 44 plain
      moves: the move choice, `runMove`, Gen 3's `useMoveInner`,
      `tryMoveHit` and `modifyDamage`, `getDamage`, accuracy and its draw,
      critical hits, the damage roll, Pressure, volatiles and Choice Band's
      lock, and about seventy handlers of abilities and items. Over 200
      games of up to 99 turns, 334,838 lines compared and equal, none
      different; 117 run to the end and the rest stop where a Pokémon has
      only an unported move left.
3. Widen the set by how often things occur, to the whole checklist. In
   batches, by what stops the most recorded games:
   1. ~~Moves with handlers of their own, secondary effects and what a move
      does to its user, the statuses, flinching, Hidden Power, stat-changing
      and healing moves, recoil, draining, fixed damage, Explosion, Rest,
      Refresh, Curse without a Ghost, the weather moves.~~ Done.
   2. ~~Side conditions (Spikes, Reflect, Light Screen, Safeguard, Mist),
      Brick Break, Rapid Spin, Substitute, Protect, Detect, Endure, Roar and
      Whirlwind, Focus Punch and the action it queues before the turn's
      moves, Leech Seed, Taunt, Wish, Knock Off, Aromatherapy, Heal Bell,
      Haze.~~ Done.
   3. ~~Eating a berry (all of them), confusion, Baton Pass with the switch
      it asks for in the middle of a turn, Pursuit catching a switch,
      Counter and Mirror Coat, Endeavor, Perish Song, Thief, Covet, Psych
      Up, Trace, Cloud Nine and Air Lock, multi-hit moves, Reversal, Flail,
      Eruption, Low Kick, Facade, Smelling Salts.~~ Done.
   4. Next, by games stopped of 1,000: Sleep Talk (117), Beat Up (74), Mean
      Look (23), Encore (21), Solar Beam (16), Truant (20), Destiny Bond
      (13), Curse from a Ghost (9), Mimic (7), Imprison (5), Struggle (5),
      the statuses a contact ability gives (6). Then what OU teams rarely
      carry: moves locked into, one-hit KOs, two-turn moves, Disable, Yawn,
      Transform, Tri Attack, King's Rock, White Herb.
4. Zero differences in all three environments.
5. Speed: the listener masks, a benchmark against hoenn.
6. Olympus reads the engine's player stream (a separate change in olympus).

## The battle's start

Read from Showdown for slice 2, so that it need not be read again. Line
numbers are in `sim/` at the pinned commit.

- **Rules are pseudo-weathers.** The constructor adds every rule that has a
  handler other than `onBegin` and the validation ones to the field with
  `addPseudoWeather` (`battle.ts:116`), in the rule table's order. That is how
  Sleep Clause Mod's `onSetStatus` is gathered. Their `onBegin`s write the
  `|rule|` lines from `start()`.
- **`start()`** (`battle.ts`) writes `gen` and `tier`, runs the `onBegin`s,
  then queues `start` and runs the turn loop. The `start` action writes
  `teamsize` and `start`, then `switchIn` for each side's first Pokémon.
- **`switchIn`** (`battle-actions.ts`) sets `abilityState` and `itemState`
  with `initEffectState` (an `effectOrder` from a counter, only for an
  active target), runs `BeforeSwitchIn`, writes the `switch` line (a split
  line: details and health), runs `EntryHazard`, and after turn 0
  `AfterEntryHazard`. It then inserts a `runSwitch` action.
- **Inserting an action draws on a tie** (`battle-queue.ts`, `insertChoice`):
  among queued actions of equal order, priority and speed the place is
  `random(first, last + 1)`.
- **`runSwitch`** gathers every queued `runSwitch`, sorts all active Pokémon
  by speed (a shuffle on a tie), records that order as `speedOrder`, and runs
  `fieldEvent('SwitchIn')`. A handler's speed there is lowered by its
  Pokémon's place in `speedOrder`, so those handlers never tie across
  Pokémon.
- **`getCallback`** (`battle.ts`) makes an ability's or item's `onStart` its
  `onSwitchIn`. In Gen 3 an ability with `onAfterEntryHazard` answers
  `onSwitchIn` with that at turn 0 and with nothing later. The ranks are
  still read under the name `onSwitchIn`, so a rank can exist for a name the
  effect has no handler under.
- **`runEvent`** (`battle.ts:765`): the relay value starts as `true` when
  none is given; a handler's return replaces it unless it is `undefined`; a
  falsy value stops the event; a whole-number result is passed through
  `modify` with the event's modifier. A status's handler is skipped when the
  holder no longer has that status. Gen 3 has no ability or item suppression.
- **`singleEvent`** (`battle.ts:578`) calls one effect's `on<Event>`, with the
  same status check, and returns the relay value when there is no handler.
- **`fieldEvent`** (`battle.ts:489`) for `Residual` also gathers every effect
  that has a duration, handler or not, and counts it down.
- **After every action** `runAction` resolves faints, runs `Update` on each
  active Pokémon in speed order (not after `start` or a switch in Gen 3), and
  asks for switches if a Pokémon has `switchFlag`.
- **`endTurn`** clears each move slot's `disabled`, runs `DisableMove` (the
  event, then once per move slot), `TrapPokemon` and `MaybeTrapPokemon`, and
  for each foe's other possible ability `FoeMaybeTrapPokemon`; then writes
  `turn`, rolls Quick Claw (`randomChance(1, 5)`) and makes the requests.
- **A move request** (`pokemon.ts`, `getMoveRequestData`) runs `LockMove`,
  lists each slot's name, id, PP, target and `disabled`, and adds `trapped`
  or `maybeTrapped`. Hidden Power is named with its type and power, Return
  and Frustration with their power. A request has no `noCancel` in `gen3ou`,
  since Cancel Mod is on and both sides are asked.
- **A log line with a function part is a split**: `|split|pN`, the line with
  each function's `secret`, then the line with its `shared`. Health is shown
  to the other side as a percentage, rounded up, and 99 when not full.
- **A Pokémon's text is its slot when active** (`p1a: Name`) and its side
  when not (`p1: Name`), taken when the line is written.

## Open

- **A test passes when no line differs**, however early its games stop. The
  count of games that run to the end is printed, not required; that has to
  change once moves are in.
- **Unported and reached by the recorded games:** what step 3 lists as
  next, and anything past turn 100.
- **Not modelled yet, and marked in the code:** the check that skips a
  handler whose effect was replaced after gathering (done for volatiles and
  side conditions only), and the record of who attacked whom (`attackedBy`,
  for Counter and its kind).
- **Shield Dust's filter is a stand-in.** `ModifySecondaries` passes a list
  in Showdown; the engine has one secondary a move, and the handler answers
  false for "only what has a `self` is kept".
- **A handler in a move's secondary or `self` is not in the handler table.**
  Three moves have one. Rapid Spin's and Baton Pass's are called by name
  from the pipeline; Tri Attack is refused.
- **Two differences were found only by long games** (a fainted Pokémon's
  status blocking Freeze Clause, and a Perish Song faint written after
  `upkeep`), in 4 of 1,000 games of up to 99 turns. Both are fixed and kept
  as `tests/fixtures/regressions.trace`. Short games would not have shown
  them; the bar of millions of long games matters.
- **No teams of its own.** The tools take `--teams DIR`; so far that has been
  hoenn's `teams/ou`. The random-legal and random-anything environments do
  not exist yet, and they are what will reach the 62% OU does not.
- **No script fetches Showdown.** `SHOWDOWN` must point at a built checkout.
- **A full trace is about 475 KB a game**, nearly all of it requests. Millions
  of games have to be piped to the checker, not stored. `--brief 1` keeps
  draws and choices only (10 KB a game).
- **Reachable conditions are over-counted.** `dump_dex.cjs` keeps any
  condition whose id appears as a word in reachable source, so `dynamax`,
  `arceus` and `commanding` are kept. The 23% of dispatches that could be
  removed outright is a lower bound.
- **Constant handlers are not counted by `coverage.cjs`**; it wraps functions.
- **Endless Battle Clause** has code in both the rule and the core and can
  end a game; it has to be ported, and random games rarely reach it.
- **Choice parsing** (`sim/side.ts`, 1,406 lines) decides what is refused
  and what the request says next; how much of it is needed is not yet read.
- **pkmn.bot** runs an unknown Showdown commit. The engine follows one commit.

## Conventions

- **Showdown's names, in snake_case.** `baseStoredStats` is
  `base_stored_stats`; a function is named for the one it ports and cites
  its file and line.
- **The battle holds no text.** Nicknames and balls belong to the formatter
  (`src/showdown/`), which also turns packed teams into sets and the state
  into Showdown's text and JSON.
- **Levels stop at 100.** Showdown allows 9,999; HP would pass 16 bits.
- **The tests' draws** go through `Out::draws`, null otherwise.
- **A handler is a function of the same name.** `callbacks.hpp` has one
  function for each Showdown callback ported, registered against the
  generated handler table. Reaching a handler that has none throws
  `Unported` with its name, and so does a branch of a ported one that needs
  something the engine lacks. A replay that stops this way is counted by
  what stopped it, never as a match.
- **The log is typed parts.** A `Line` is one call of Showdown's `add`, its
  parts kept as what they were (a Pokémon, its details, its health, a
  number, a literal). Text is made from it in `src/showdown/log.hpp`.
- **Dispatch is not yet fast.** Handlers are found through a hash map; the
  listener masks of design rule 5 are step 5.

## Running it

```
cmake -S . -B build/release && cmake --build build/release -j && ctest --test-dir build/release
```

The Node tools need `SHOWDOWN`, a built checkout of the pinned commit, and
most need `--teams DIR`:

| Tool | What it does |
|---|---|
| `tools/trace.cjs` | records games (`--games`, `--first`, `--out`, `--turns N`, `--choices switch` or `plain`, `--opening 1`, `--brief 1`, `--sites 1`) |
| `tools/coverage.cjs` | counts runs of each checklist function over the same games |
| `tools/census.cjs` | events, sorts and draws per update, to `build/census.json` |
| `tools/dump_dex.cjs` | the data as JSON and the checklist, to `build/dex/` |
| `tools/gen_tables.cjs` | writes `src/data/`; stops on a field it has no place for |
| `tools/tables_ref.cjs` | writes `tests/fixtures/tables.txt` |
| `tools/prng_ref.cjs` | prints the line `tests/prng_test.cpp` must print |
