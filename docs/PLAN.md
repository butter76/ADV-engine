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
   114). An effect has a constant mask, made by the compiler from the
   handler table. A Pokémon keeps the OR of the masks of its status,
   volatiles, ability and item; the battle keeps the OR of everything in
   play, and for each side the OR of what is not a Pokémon's own. An event
   tests its names against those, in line where it is run, and 92% of events
   end there; only then are handlers gathered, exactly as before. A kept
   mask is refreshed wherever an effect is *given* (`refresh_mask`, and
   `listening_stale` for the battle's); one taken away may linger until the
   turn ends, since a mask that says too much only costs time. The build
   with `ADV_CHECK_MASKS` makes every use of a kept mask first prove that it
   covers what is there, and is part of `ctest`.
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

What is tested today (`ctest`, no Node needed). Every replay test runs with
`--complete`: a game that stops at something unported fails it, as a line
that differs does.

| Test | What it shows |
|---|---|
| `prng` | 2,000,000 mixed calls give the hash Showdown's PRNG gives (`tools/prng_ref.cjs`) |
| `trace_draws` | all 20,124 draws of 50 recorded games replay from their seeds |
| `replay_openings` | 200 games up to the first choice |
| `replay_switches` | 30 games of 30 turns in which both players switch whenever they can |
| `replay_plain_moves` | 20 games of 40 turns of plain damaging moves and switches |
| `replay_any` | 60 OU games of 12 turns with any choice at all |
| `replay_regressions` | 18 whole games that once differed |
| `export` | 50 teams in Showdown's export text read as Showdown packs them (`tools/teams_ref.cjs`) |
| `replay_targeted` | 16 games of up to 99 turns between teams written to use particular moves (`tests/teams/targeted`) |
| `replay_random` | 24 games of 30 turns between teams of anything Gen 3 has, legal or not |
| `masks_targeted`, `masks_random`, `masks_regressions` | the same games on the build that checks every kept mask before it is used |
| `data` | the tables state the same 2,254 facts as Showdown, Metronome's list in its order among them (`tools/tables_ref.cjs`, written apart from the generator) |

The recorded games behind the last full run, none of them with a line
different and every one run to its end (30,124,089 lines over 13,800 games):

| Games | Teams | Turns | Lines |
|---|---|---|---|
| 1,000 + 800 + 200 | OU | up to 99 | 3,239,062 |
| 1,500 | OU, never replayed before the run | no limit | 3,993,491 |
| 600 + 2,000 + 3,000 | random | 60, 99, no limit | 8,425,138 |
| 2,500 | random, never replayed before the run | no limit | 3,931,821 |
| 600 + 1,200 | targeted | 99, no limit | 5,334,021 |
| 400 | targeted (freezing and thawing alone) | no limit | 5,200,556 |

Larger runs go through a pipe and are not stored (`tools/soak.cjs`):

| Games | Teams | Result |
|---|---|---|
| 200,000 | legal random (`randbats`) | all match, 411,647,039 lines |
| 200,000 | random | 119 differed and one stopped; all fixed since |
| 200,000 | OU | 9 differed; all fixed since |
| 1,000,000 | random, after those fixes | all match, 1,585,838,306 lines |
| 300,000 | random, on the engine after the work on its speed | all match, 475,718,933 lines |
| 1,000,000 | OU, on the engine before that work | all match, 2,587,217,193 lines |
| 150,000 | random, after `Line` was given twelve parts | one differed (Metal Powder, below); fixed since |
| 150,000 | OU, after `Line` was given twelve parts | all match, 390,181,837 lines |
| 60,000 | `tests/teams/future`, after `Line` was given twelve parts | all match, 84,659,465 lines |
| 150,000 | random, after both of the fixes below | all match, 238,051,573 lines |
| 30,000 | `tests/teams/items`, after both | all match, 72,237,408 lines |

The first six of these were run before olympus used the engine. A fault
none of them reached was found afterwards from olympus, whose games are between other teams: a
`move` line can be given more attributes than a `Line` had room for (the
user, the move, its target, `[from] lockedmove`, and three `[miss]`, two of
them from Future Sights missing at the turn's end, which `attrLastMove` adds
to whatever `move` line is the last), and the eighth part was written past
the line's end. A `Line` now holds twelve parts and refuses a thirteenth.
30,000 games between teams made to do it (`tests/teams/future`) had the line
in 34, each of which the engine before the fix got wrong and the engine
after it plays to its end; two are in `regressions.trace`. Soaks do not find
what their teams do not do: a new kind of team is worth a soak of its own.

The soak of that fix found one more, in the 131,000th of its games between
teams of anything: Metal Powder doubled the Defense of a Pokémon that had
transformed into a Ditto. Its handler was written before Transform was
ported and never had Showdown's `!pokemon.transformed`. 600 games between
teams of the species such items are for and of others who hold them and
transform (`tests/teams/items`) have 14,520 transformations: the engine
before the fix differs in 31 games, the engine after it in none. A handler
written while something it reads was not yet ported says so in a comment;
those comments are searched for when the thing is ported.

The million had one fault of its own: the fix for Sleep Talk calling Helping
Hand had not been replayed on the game that showed it, and it crashed the
checker in 15 chunks of 500 games. A fix is replayed on the game that needs
it before a long run is started; and a build with `-fsanitize=address,undefined`
is run over a stored set after any change to the move pipeline.

Of the checklist's 624 functions, 564 ran in those games on Showdown's side
(`tools/coverage.cjs`). The 60 that did not: later generations' conditions
and the validator's rules (44), nine that only doubles can reach (Follow Me,
Helping Hand, Lightning Rod's and Counter's redirection), six that Gen 3
never calls (`onModifyType` of Hidden Power and Weather Ball, Rapid Spin's
`onAfterSubDamage`, White Herb's `onAnyAfterMega`, the `toxic` volatile's
two), and Torment's `onEnd`, which nothing in Gen 3 brings about.

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
   4. ~~Sleep Talk, Beat Up, Mean Look and its kind, Truant, Struggle,
      Destiny Bond, the statuses a contact ability gives; Encore, Disable,
      Yawn, Curse from a Ghost, Belly Drum, Pain Split, Fake Out; moves that
      charge for a turn (Solar Beam, Fly, Dig, Dive, Bounce, Skull Bash,
      Razor Wind, Sky Attack), moves locked into (Outrage, Thrash, Petal
      Dance) and the turn to recharge; binding moves; Drizzle, Drought,
      Forecast and Castform's forms, Rain Dish, Color Change, Damp; White
      Herb, King's Rock, Focus Band, Lansat Berry, Mail; Stockpile, Swallow,
      Spit Up, Spite, Magnitude, Present, Revenge, False Swipe, Psywave,
      Super Fang, Dream Eater, Snore, the three weather heals, Pay Day,
      Splash, Teleport; Lock-On, Mind Reader, Foresight, Odor Sleuth,
      Nightmare, Imprison and the choices it has refused, Ingrain, Torment,
      Rage, Grudge, Attract, Charge, Focus Energy, Mud Sport, Water Sport,
      Minimize.~~ Done: 512 of the checklist's 653 callbacks are registered
      (`tools/unported.cjs`). Compared and equal, none different:
      1,860,399 lines over 1,000 OU games of up to 99 turns (990 run to the
      end), 1,103,568 lines over 600 games between the targeted teams (599
      run to the end), and 209,489 lines over 600 games between random
      teams, of which 34 run to the end.
   5. ~~One-hit KOs, Jump Kick's crash, Triple Kick, Memento; Rollout, Ice
      Ball, Fury Cutter, Uproar, Bide; Conversion, Conversion 2, Camouflage,
      Recycle, Trick, Skill Swap, Role Play, Transform, Mimic, Sketch;
      Metronome, Assist, Mirror Move, Nature Power; Magic Coat, Snatch;
      Future Sight and Doom Desire, with the newer hit steps they land
      through; Weather Ball, Tri Attack, Secret Power; Helping Hand and
      Follow Me as singles has them; Mental Herb, Liquid Ooze; the Endless
      Battle Clause and the turn limit.~~ Done: 574 of the checklist's 653
      callbacks are registered (`tools/unported.cjs`), and none of the 79
      left can run in Gen 3 singles.
4. Zero differences in all three environments. Reached over a million
   games for random teams of anything (see Testing); the same run for legal
   random teams and for OU teams comes next, and then a pass over the
   branches that remain `unported` inside ported handlers (below).
5. Speed. `smoke/engine_bench.cpp` replays the choices of recorded games on
   one core and counts updates as the recorder does. Where it stood and
   stands, in updates a second for the engine alone (OU games; games of
   random teams run about 5% faster):

   | Change | Updates/s |
   |---|---|
   | as first measured | 97,000 |
   | callback names resolved by the compiler (`consteval`), not matched as strings while a battle runs | 112,000 |
   | the hash map of handlers replaced by a bit for each callback of each effect | 140,000 |
   | events' names asked as flags; no 24-entry array filled for each event; a Pokémon, side or field with no bit for the name not looked through | 330,000 |
   | the masks a constant table; the rules' mask kept | 390,000 |
   | each Pokémon's mask kept on it | 430,000 |
   | one mask for the battle, tested before anything else | 495,000 |
   | a log part 16 bytes, not 48 (a line 114, not 344), and written in place | 555,000 |
   | the test for "nothing listens" put in line at every event | 760,000 |
   | the exact holders asked for an event looked up under one name | 790,000 |

   (The benchmark stopped timing the parsing of team text at the eighth row,
   which was by then about a tenth of the time.) hoenn does 3,290,000 and
   Showdown 4,150.

   Writing Showdown's text and requests as well, the engine did 41,000 and
   does 180,000: a request is some 1,500 bytes twice a turn, and it and the
   log's lines are now appended to one string where each piece had been a
   string of its own. Text is still four times the cost of the battle; a
   reader that takes the typed log and the request as the engine holds them
   pays none of it.

   The engine's profile is now flat: moves 31%, the end-of-turn event 19%,
   `Update` after each action 10%, the turn's end 9%, requests 8%. Not done:
   profile-guided optimisation, which would ask the same of whatever builds
   the engine into olympus; keeping the handler's function with a gathered
   handler, in place of looking it up again to call it.
6. Olympus reads the engine's player stream (a separate change in olympus,
   on its branch `adv-engine`). What it needed here:
   - `src/view.hpp`: the log as one player is sent it, as Lines. A View
     gives a Pokémon of the other side its number (the order the log first
     named them) in place of its index in a team the player has not seen,
     and that side's HP as the text would have it. `tests/replay.cpp` holds
     each player's view of every recorded game to what Showdown's stream
     sends that player: of a split line its own side's half, and of a line
     for the other side alone, nothing.
   - `src/showdown/export.hpp`: teams in Showdown's export text
     (`Teams.import`), which is how team pools are kept. `tests/export_test.cpp`
     holds it to what Showdown packs of the same text, on the 50 sample
     teams in `tests/teams/ou` (and by hand on the other 281 OU teams and
     the targeted ones: all equal).
   - The CMake target `adv` carries `-ffp-contract=off` and nothing else, and
     a project that takes the engine in gets the target without the tests.
   Olympus's own checks of its tokens ran the engine on a million games
   between teams of anything with no stop at something unported, and are
   what found the line above.

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
  or `maybeTrapped`. A Pokémon locked into a move is shown that move alone,
  as a name and an id (`Recharge` for the turn after Hyper Beam), and is
  trapped. Hidden Power is named with its type and power, Return
  and Frustration with their power. A request has no `noCancel` in `gen3ou`,
  since Cancel Mod is on and both sides are asked.
- **A log line with a function part is a split**: `|split|pN`, the line with
  each function's `secret`, then the line with its `shared`. Health is shown
  to the other side as a percentage, rounded up, and 99 when not full.
- **A Pokémon's text is its slot when active** (`p1a: Name`) and its side
  when not (`p1: Name`), taken when the line is written.

## Open

- **Branches still `unported` inside ported code**, none reached in 13,800
  games: a stalling move's ninth success in a row (a draw from 2^32), a
  forced switch for a side that has a switch queued, a Pokémon fainting as
  its own move switches it out, a healing move stopped by `TryHeal`, a
  second Pokémon tied to one trapping volatile, Beat Up's Attack handler
  with no ally left (Showdown would crash there), and the fixed limits
  (16 queued actions, 8 volatiles, 24 handlers of one event).
- **A mod inherits from the next generation's.** Gen 3 takes
  `hitStepAccuracy` and `hitStepInvulnerabilityEvent` from Gen 4's mod, not
  from `sim/battle-actions.ts`; a Future Sight's accuracy was wrong by a
  truncation until the games showed it. Before porting anything from
  `sim/`, look for it in `data/mods/gen4/scripts.ts`.
- **Read, do not recall.** The turn limit was first written from memory as
  `turn >= 1000`; the pinned source says `> 1000`. Games that ran to turn
  1000 showed it.
- **Not modelled yet, and marked in the code:** the check that skips a
  handler whose effect was replaced after gathering (done for volatiles and
  side conditions only).
- **`attackedBy` is one record for each Pokémon of the other side**, not
  Showdown's list of every hit: the last hit each gave, and whether any it
  gave this turn did damage. One record in all was not enough: a Future
  Sight landing is a hit from a Pokémon that may have left, and Showdown
  forgets it at the end of the turn and falls back on the hit before.
- **A Pokémon can have a move twice.** Two typed Hidden Powers in a set are
  two slots of one id (only teams of anything have this). Showdown finds
  "the move's slot" as the first with the id (PP is taken from it, Encore
  looks at it), asks whether a move is enabled of any slot with the id, and
  marks the first entry with the id in a corrected request.
- **A locked move's target is not kept.** Showdown stores the slot a charging
  or locked move was aimed at; in singles every case comes out as the foe.
- **Each kind of team found what the others could not.** Targeted teams in
  batch 4: a last move kept after leaving, a refused volatile reported as a
  failure, Nightmare outlasting sleep, Imprison's refused choice and its
  Struggle. Random teams in batch 5: `setStatus` returning null under
  Uproar, Mental Herb, a multi-hit move called asleep, Focus Band against a
  confused Pokémon's own hit, Destiny Bond and Endure against a future
  move, the type a transformed Pokémon is "known" to have, PP Ups of a
  copied move, a future move's faint written after `upkeep`. Long targeted
  games: Yawn through Safeguard, a move with no PP shown as disabled under
  a hidden disable, the Struggle notice naming a Pokémon that has since
  left, and the turn limit. The first 200,000 random games through
  `tools/soak.cjs` (119 differed, about one in 1,700): the record of who
  attacked, a doubled Hidden Power under Imprison and Encore, a transformed
  Hidden Power's name, Freeze Clause counting a fainted Pokémon whose
  switch was already queued, Wonder Guard under Foresight, and Sleep Talk
  calling a move on an ally.
- **Shield Dust's filter is a stand-in.** `ModifySecondaries` passes a list
  in Showdown; the engine has one secondary a move, and the handler answers
  false for "only what has a `self` is kept".
- **A handler in a move's secondary or `self` is not in the handler table.**
  Three moves have one, each called by name from the pipeline: Rapid
  Spin's, Baton Pass's and Tri Attack's.
- **A made-up move is the active move with some fields overridden.** Bide's
  release and a landing Future Sight are objects Showdown builds on the
  spot; the engine sets `fixed_damage` and reads the rest from the real
  move's row, which happens to agree but for `ignoreImmunity` (handled).
- **Mimic and Transform keep the slots they cover** (`base_move_slots`,
  one bit a slot), where Showdown keeps a second list of slot objects.
- **Two differences were found only by long games** (a fainted Pokémon's
  status blocking Freeze Clause, and a Perish Song faint written after
  `upkeep`), in 4 of 1,000 games of up to 99 turns. Both are fixed and kept
  as `tests/fixtures/regressions.trace`. Short games would not have shown
  them; the bar of millions of long games matters.
- **Teams.** The tools take `--teams DIR` (hoenn's `teams/ou` so far, and
  the repository's `tests/teams/targeted`), `--teams random`, which makes
  teams of anything Gen 3 has (any species, any four moves, any item, an
  ability that three times in four is the species' own), or `--teams
  randbats`, which takes Showdown's own Gen 3 random-battle teams: legal
  sets at balanced levels. Those name Unown's letter forms, so the tables
  hold each cosmetic form as a species (419 in all).
- **No script fetches Showdown.** `SHOWDOWN` must point at a built checkout.
- **A full trace is about 475 KB a game**, nearly all of it requests, so
  large runs are piped: `tools/soak.cjs` has `tools/trace.cjs --out -` write
  each chunk of games to a `replay -` that reads it, several chunks at a
  time. On 12 cores that checks about 175 games a second; recording is
  nearly all of the time. A game that differs is named by its number and
  can be recorded again alone.
- **Reachable conditions are over-counted.** `dump_dex.cjs` keeps any
  condition whose id appears as a word in reachable source, so `dynamax`,
  `arceus` and `commanding` are kept. The 23% of dispatches that could be
  removed outright is a lower bound.
- **Constant handlers are not counted by `coverage.cjs`**; it wraps functions.
- **The Endless Battle Clause is ported but has never fired** in a recorded
  game: it needs a Pokémon that had Skill Swap used on it by a foe, a stale
  Pokémon across from it, and more than 100 turns. Staleness is tracked as
  Showdown tracks it, its quirks included (every item given marks its
  holder, and Trick never marks it as coming from the other side).
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
- **A mask is refreshed where an effect is given.** Anything new that sets a
  status, adds a volatile, or changes an ability or an item calls
  `refresh_mask`; anything that adds a side or slot condition, changes the
  weather or the active Pokémon sets `listening_stale`. Forgetting one makes
  events miss handlers; the `masks_*` tests stop on it and say so.

## Running it

```
cmake -S . -B build/release && cmake --build build/release -j && ctest --test-dir build/release
```

The Node tools need `SHOWDOWN`, a built checkout of the pinned commit, and
most need `--teams DIR`:

| Tool | What it does |
|---|---|
| `tests/replay.cpp` (`replay FILE [--complete]`) | compares a recorded trace with the engine, line by line; `-` reads a pipe |
| `smoke/engine_bench.cpp` (`engine_bench FILE [REPEATS] [alone\|text]`) | how fast the engine plays the choices of a recorded trace |
| `tools/soak.cjs` | records and checks games through a pipe, in parallel (`--teams`, `--games`, `--first`, `--jobs`, `--chunk`, `--turns`); the report goes to `build/soak/` |
| `tools/trace.cjs` | records games (`--teams DIR`, `random` or `randbats`, `--out FILE` or `-`, `--games`, `--first`, `--turns N`, `--choices switch` or `plain`, `--opening 1`, `--brief 1`, `--sites 1`) |
| `tools/coverage.cjs` | counts runs of each checklist function over the same games (`--teams DIR`, `random` or `randbats`) |
| `tools/census.cjs` | events, sorts and draws per update, to `build/census.json` |
| `tools/dump_dex.cjs` | the data as JSON and the checklist, to `build/dex/` |
| `tools/unported.cjs` | the checklist's callbacks that `src/callbacks.hpp` does not register; `--show ID` prints Showdown's source for an effect's |
| `tools/gen_tables.cjs` | writes `src/data/`; stops on a field it has no place for |
| `tools/tables_ref.cjs` | writes `tests/fixtures/tables.txt` |
| `tools/teams_ref.cjs` | packs a file of exported teams as Showdown does, a line a team: what `tests/export_test.cpp` compares with |
| `tools/prng_ref.cjs` | prints the line `tests/prng_test.cpp` must print |
