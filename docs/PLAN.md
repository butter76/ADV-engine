# Plan

A Gen 3 singles battle engine in C++ that is a port of Pokémon Showdown at
commit `a78551b070769eb22e5261bb220d21153bf5788c` (2026-11-09, the commit
pkmn.ai's ladder runs). Given the same seed, teams and choices it must make
the same random draws in the same order and write the same log and the same
requests, bugs included.

Status (2026-11-23): step 1 of "Order of work" is done. There is a PRNG, the
static tables, a trace recorder and the tools that measure Showdown. There is
no battle yet.

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

The engine will replay a trace and be compared at three levels, finest first:

1. **Draws.** The first draw that differs names the divergence.
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
| `data` | the tables state the same 1,834 facts as Showdown (`tools/tables_ref.cjs`, written apart from the generator) |

## Order of work

1. ~~The trace recorder, the generated C++ tables, and a count of how often
   each Showdown callback runs.~~ Done.
2. A skeleton that plays whole games on a restricted set: team setup and
   stats, switching, the queue, the turn loop, requests, plain damaging
   moves. The recorder generates teams from what is implemented.
3. Widen the set by how often things occur, to the whole checklist.
4. Zero differences in all three environments.
5. Speed: the listener masks, a benchmark against hoenn.
6. Olympus reads the engine's player stream (a separate change in olympus).

## Open

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

## Running it

```
cmake -S . -B build/release && cmake --build build/release -j && ctest --test-dir build/release
```

The Node tools need `SHOWDOWN`, a built checkout of the pinned commit, and
most need `--teams DIR`:

| Tool | What it does |
|---|---|
| `tools/trace.cjs` | records games (`--games`, `--first`, `--out`, `--brief 1`, `--sites 1`) |
| `tools/coverage.cjs` | counts runs of each checklist function over the same games |
| `tools/census.cjs` | events, sorts and draws per update, to `build/census.json` |
| `tools/dump_dex.cjs` | the data as JSON and the checklist, to `build/dex/` |
| `tools/gen_tables.cjs` | writes `src/data/`; stops on a field it has no place for |
| `tools/tables_ref.cjs` | writes `tests/fixtures/tables.txt` |
| `tools/prng_ref.cjs` | prints the line `tests/prng_test.cpp` must print |
