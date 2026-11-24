// Shared by the Node tools: the pinned Showdown, team files, and a driver that
// plays a battle with uniformly random choices.
//
// SHOWDOWN must point at a built checkout of the frozen commit (dist/ present).
'use strict';
const fs = require('fs');
const path = require('path');

const COMMIT = 'a78551b070769eb22e5261bb220d21153bf5788c';

function showdown() {
  const root = process.env.SHOWDOWN;
  if (!root) throw new Error('set SHOWDOWN to a built checkout of ' + COMMIT);
  const head = fs.readFileSync(path.join(root, '.git', 'HEAD'), 'utf8').trim();
  if (head !== COMMIT) throw new Error(`SHOWDOWN is at ${head}, not ${COMMIT}`);
  const sim = name => require(path.join(root, 'dist', 'sim', name));
  return {
    root,
    Battle: sim('battle.js').Battle,
    Teams: sim('teams.js').Teams,
    Dex: sim('dex.js').Dex,
    PRNG: sim('prng.js').PRNG,
  };
}

// Team files in Showdown's export text, one team per `=== ... ===` header.
function loadTeams(Teams, dir) {
  const teams = [];
  for (const file of fs.readdirSync(dir).sort()) {
    if (!file.endsWith('.txt')) continue;
    const blocks = fs.readFileSync(path.join(dir, file), 'utf8').split(/^=== .*===$/m).slice(1);
    for (const block of blocks) {
      const team = Teams.import(block.trim());
      if (team && team.length) teams.push(team);
    }
  }
  if (!teams.length) throw new Error('no teams in ' + dir);
  return teams;
}

// The driver's own generator, apart from the battle's, so that choices do not
// depend on how many draws the battle made.
function lcg(seed) {
  let s = seed >>> 0;
  return n => {
    s = (Math.imul(s, 1103515245) + 12345) >>> 0;
    return (s >>> 8) % n;
  };
}

// One choice for a side from its request. A refused choice (a hidden trap or a
// hidden disable) is retried.
function choose(side, rnd) {
  const r = side.activeRequest;
  if (!r || r.wait) return null;
  if (r.teamPreview) return side.choose('default') && 'default';
  const bench = [];
  r.side.pokemon.forEach((p, i) => {
    if (!p.active && !p.condition.endsWith(' fnt')) bench.push(i + 1);
  });
  const options = [];
  if (r.forceSwitch) {
    for (const b of bench) options.push('switch ' + b);
  } else {
    const a = r.active[0];
    a.moves.forEach((m, i) => { if (!m.disabled) options.push('move ' + (i + 1)); });
    if (!a.trapped) for (const b of bench) options.push('switch ' + b);
  }
  for (let tries = 0; tries < 32 && options.length; tries++) {
    const pick = options[rnd(options.length)];
    if (side.choose(pick)) return pick;
  }
  if (!side.choose('default')) throw new Error('no choice accepted');
  return 'default';
}

// Plays one battle. `each(battle, choices)` runs after every committed update.
function play({Battle}, {format, seed, teams, rnd, each, before}) {
  const battle = new Battle({formatid: format, seed});
  if (before) before(battle);
  battle.setPlayer('p1', {name: 'A', team: teams[0]});
  battle.setPlayer('p2', {name: 'B', team: teams[1]});
  let updates = 0;
  while (!battle.ended) {
    const choices = battle.sides.map(side => choose(side, rnd));
    if (!battle.allChoicesDone()) throw new Error('stuck at turn ' + battle.turn);
    battle.commitChoices();
    updates++;
    if (each) each(battle, choices);
  }
  return {battle, updates};
}

// Plays one battle through `battle.choose`, as a client would, and returns
// what a replay has to reproduce, in the order it happened:
//
//   G <format> <seed>            the game
//   T <side> <packed team>
//   d <from> <to> <result> <n>   a draw: random(from, to), with `-` for an
//                                argument not given; n is how many log lines
//                                the battle had written before it. A bare
//                                random() is recorded as its 32-bit integer.
//   c <side> <choice>            a choice as it was tried; a refusal shows as
//                                the `s` lines that follow it
//   U                            Showdown sent an update: the `u` lines after it
//   u <line>                     a log line, `|split|` lines included; `|t:|`
//                                lines, which hold the wall clock, are left out
//                                and not counted
//   s <side> <line>              a line sent to one side: a request or an error
//   e <json>                     the battle's end
//   E                            the game is over
//
// As BattleStream does after each chunk it is written, `sendUpdates` runs
// after the players are set and after every choice.
function record({Battle, Teams}, {format, seed, teams, rnd, site, turns, choices}) {
  const out = [`G ${format} ${seed}`];
  const battle = new Battle({
    formatid: format, seed,
    send(type, data) {
      const lines = Array.isArray(data) ? data : String(data).split('\n');
      if (type === 'update') {
        out.push('U');
        for (const line of lines) if (!line.startsWith('|t:|')) out.push('u ' + line);
      } else if (type === 'sideupdate') {
        for (const line of lines.slice(1)) out.push(`s ${lines[0]} ${line}`);
      } else if (type === 'end') {
        out.push('e ' + lines.join(''));
      } else {
        throw new Error('unexpected send: ' + type);
      }
    },
  });
  // Log lines written so far, without the clock's.
  let seen = 0, written = 0;
  const count = () => {
    for (; seen < battle.log.length; seen++) if (!battle.log[seen].startsWith('|t:|')) written++;
    return written;
  };
  const random = battle.prng.random;
  battle.prng.random = function (from, to) {
    const result = random.call(this, from, to);
    const shown = from === undefined ? Math.round(result * 2 ** 32) : result;
    out.push(`d ${from === undefined ? '-' : from} ${to === undefined ? '-' : to} ${shown} ${count()}` +
      (site ? ' ' + site() : ''));
    return result;
  };
  teams.forEach((team, i) => out.push(`T p${i + 1} ${Teams.pack(team)}`));
  battle.setPlayer('p1', {name: 'A', team: teams[0]});
  battle.setPlayer('p2', {name: 'B', team: teams[1]});
  battle.sendUpdates();

  let updates = 0;
  // `turns` stops the game once that many turns have been played; `choices`
  // of 'switch' picks a switch whenever one is offered.
  while (!battle.ended && !(turns && battle.turn > turns)) {
    let chose = false;
    for (const side of battle.sides) {
      const r = side.activeRequest;
      if (battle.ended || !r || r.wait || side.isChoiceDone()) continue;
      const bench = [];
      r.side.pokemon.forEach((p, i) => { if (!p.active && !p.condition.endsWith(' fnt')) bench.push(i + 1); });
      const options = [];
      if (r.forceSwitch) {
        for (const b of bench) options.push('switch ' + b);
      } else if (r.active) {
        const a = r.active[0];
        const switches = a.trapped ? [] : bench.map(b => 'switch ' + b);
        // Moves first, then switches: the order the draws of earlier traces depend on.
        if (choices !== 'switch' || !switches.length) a.moves.forEach((m, i) => { if (!m.disabled) options.push('move ' + (i + 1)); });
        options.push(...switches);
      }
      const turn = battle.turn, sent = battle.log.length;
      let done = false;
      for (let tries = 0; tries < 32 && options.length && !done; tries++) {
        const pick = options[rnd(options.length)];
        out.push(`c ${side.id} ${pick}`);
        done = battle.choose(side.id, pick);
        battle.sendUpdates();
        // A player who only switches moves once a switch is refused.
        if (!done && choices === 'switch') {
          options.length = 0;
          side.activeRequest.active[0].moves.forEach((m, i) => { if (!m.disabled) options.push('move ' + (i + 1)); });
        }
      }
      if (!done) {
        out.push(`c ${side.id} default`);
        if (!battle.choose(side.id, 'default')) throw new Error('no choice accepted at turn ' + battle.turn);
        battle.sendUpdates();
      }
      chose = true;
      if (battle.turn !== turn || battle.log.length !== sent || battle.ended) updates++;
    }
    if (!chose && !battle.ended) throw new Error('stuck at turn ' + battle.turn);
  }
  out.push('E');
  return {lines: out, updates, turns: battle.turn};
}

const gen5 = n => 'gen5,' + BigInt(n).toString(16).padStart(16, '0');

module.exports = {COMMIT, showdown, loadTeams, lcg, choose, play, record, gen5};
