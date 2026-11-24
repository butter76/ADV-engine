// Records seeded Showdown games for the engine to replay: every draw, every
// choice, and everything Showdown sent (the format is in tools/lib.cjs).
//
// Usage: SHOWDOWN=... node tools/trace.cjs --teams DIR [--games N] [--first K]
//                                          [--format ID] [--out FILE] [--sites 1] [--brief 1]
//
// --brief 1 writes the games, teams, draws and choices alone, without what
// Showdown sent: enough to replay the draws, and small enough to keep in git.
// The digest is of the full trace either way.
// --turns N stops each game after N turns. --choices switch has both players
// switch whenever they are offered a switch.
// --opening 1 writes each game up to its first choice: the teams, the battle's
// start and the first requests.
//
// Game k of a run is the same game whatever --first and --games are: its seed
// and its teams depend on k alone.
'use strict';
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const {showdown, loadTeams, lcg, record, gen5, COMMIT} = require('./lib.cjs');

const opts = {games: 100, first: 1, format: 'gen3ou', out: '', sites: '', brief: '', opening: '', turns: 0, choices: ''};
for (let i = 2; i < process.argv.length; i += 2) opts[process.argv[i].slice(2)] = process.argv[i + 1];
opts.games = +opts.games;
opts.first = +opts.first;

const S = showdown();
const teams = loadTeams(S.Teams, opts.teams);

// A draw's call site, for reading a trace by eye: the first frame above the PRNG.
const site = () => {
  const frames = new Error().stack.split('\n').slice(1)
    .filter(f => !/prng\.js|lib\.cjs|trace\.cjs/.test(f));
  const skip = /^Battle\.(random|randomChance|sample)$/;
  for (const f of frames) {
    const m = f.match(/at (?:(\S+) \()?.*?([^/]+)\.js:(\d+)/);
    if (!m) continue;
    const name = m[1] || '<anonymous>';
    if (skip.test(name)) continue;
    return /^(moves|abilities|items|conditions|scripts|rulesets)$/.test(m[2]) ? `${name}@${m[2]}:${m[3]}` : name;
  }
  return '?';
};

const digest = crypto.createHash('sha1');
let fd = null;
if (opts.out) {
  fs.mkdirSync(path.dirname(opts.out), {recursive: true});
  fd = fs.openSync(opts.out, 'w');
  fs.writeSync(fd, `# showdown ${COMMIT}\n`);
}
let updates = 0, turns = 0, lines = 0, bytes = 0, draws = 0;
for (let g = opts.first; g < opts.first + opts.games; g++) {
  // The seed of game g, and the generator its teams and choices come from.
  const rnd = lcg(Math.imul(g, 0x9E3779B1) ^ 0x2026);
  const pair = [teams[rnd(teams.length)], teams[rnd(teams.length)]];
  const seed = gen5(BigInt.asUintN(64, BigInt(g) * 0x9E3779B97F4A7C15n));
  const game = record(S, {format: opts.format, seed, teams: pair, rnd, site: opts.sites ? site : null, turns: +opts.turns, choices: opts.choices});
  const text = game.lines.join('\n') + '\n';
  digest.update(text);
  if (fd !== null) {
    let lines = game.lines;
    if (opts.opening) lines = lines.slice(0, lines.findIndex(l => l.startsWith('c '))).concat(['E']);
    if (opts.brief) lines = lines.filter(l => /^[GTdcE]/.test(l));
    fs.writeSync(fd, lines.join('\n') + '\n');
  }
  updates += game.updates;
  turns += game.turns;
  lines += game.lines.length;
  bytes += Buffer.byteLength(text);
  for (const line of game.lines) if (line.charCodeAt(0) === 100) draws++;
}
if (fd !== null) fs.closeSync(fd);
console.log(`${opts.format}: games ${opts.first}..${opts.first + opts.games - 1}, ${updates} updates, ${turns} turns, ` +
  `${draws} draws, ${lines} lines, ${(bytes / 1e6).toFixed(1)} MB, sha1 ${digest.digest('hex').slice(0, 16)}`);
