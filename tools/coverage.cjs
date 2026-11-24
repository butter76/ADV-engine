// How often each handler of the checklist (build/dex/callbacks.json) runs in
// recorded games, so that "zero differences" can be read against what the
// games exercised. Functions are counted by wrapping them in Showdown's data
// before any battle is built; a constant handler cannot be wrapped and is
// reported apart.
//
// The games are those of tools/trace.cjs with the same arguments, and the
// digest printed must equal its digest: the wrapping changes nothing.
//
// Usage: SHOWDOWN=... node tools/coverage.cjs --teams DIR|random [--games N] [--first K]
//                                             [--format ID] [--out FILE]
'use strict';
const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const {showdown, loadTeams, randomTeam, lcg, record, gen5} = require('./lib.cjs');

const opts = {games: 100, first: 1, format: 'gen3ou', out: 'build/coverage.json', checklist: 'build/dex/callbacks.json'};
for (let i = 2; i < process.argv.length; i += 2) opts[process.argv[i].slice(2)] = process.argv[i + 1];
opts.games = +opts.games;
opts.first = +opts.first;

const S = showdown();
const checklist = JSON.parse(fs.readFileSync(opts.checklist, 'utf8')).callbacks;

// Wrap in the raw tables, which every Move, Ability, Item, Condition and
// Format object is later built from. A mod's table shares its parent's
// entries where it changes nothing, so one function can be reached twice.
const RAW = {move: 'Moves', ability: 'Abilities', item: 'Items', condition: 'Conditions', rule: 'Rulesets'};
const counts = new Map();
const wrapped = new WeakMap();
const missing = [];
const key = c => `${c.kind}:${c.id}:${c.path}`;
for (const dex of [S.Dex.mod('gen3'), S.Dex]) {
  for (const c of checklist) {
    if (c.constant) continue;
    const entry = dex.data[RAW[c.kind]][c.id];
    const keys = c.path.split('.');
    const holder = keys.slice(0, -1).reduce((o, k) => (o == null ? o : o[k]), entry);
    const fn = holder && holder[keys[keys.length - 1]];
    if (typeof fn !== 'function') {
      if (dex !== S.Dex) missing.push(key(c));
      continue;
    }
    if (wrapped.has(fn)) continue; // already a wrapper, or shared and wrapped
    const k = key(c);
    counts.set(k, counts.get(k) || 0);
    const wrapper = function (...args) {
      counts.set(k, counts.get(k) + 1);
      return fn.apply(this, args);
    };
    wrapped.set(wrapper, true);
    holder[keys[keys.length - 1]] = wrapper;
  }
}

const teams = opts.teams === 'random' ? null : loadTeams(S.Teams, opts.teams);
const digest = crypto.createHash('sha1');
let updates = 0;
for (let g = opts.first; g < opts.first + opts.games; g++) {
  const rnd = lcg(Math.imul(g, 0x9E3779B1) ^ 0x2026);
  const pair = teams ? [teams[rnd(teams.length)], teams[rnd(teams.length)]] : [randomTeam(S, rnd), randomTeam(S, rnd)];
  const seed = gen5(BigInt.asUintN(64, BigInt(g) * 0x9E3779B97F4A7C15n));
  const game = record(S, {format: opts.format, seed, teams: pair, rnd});
  digest.update(game.lines.join('\n') + '\n');
  updates += game.updates;
}

const functions = checklist.filter(c => !c.constant);
const rows = functions.map(c => ({kind: c.kind, id: c.id, path: c.path, runs: counts.get(key(c)) || 0, wrapped: counts.has(key(c))}));
const hit = rows.filter(r => r.runs > 0);
console.log(`${opts.format}: games ${opts.first}..${opts.first + opts.games - 1}, ${updates} updates, sha1 ${digest.digest('hex').slice(0, 16)}`);
console.log(`${functions.length} functions in the checklist, ${rows.filter(r => r.wrapped).length} wrapped, ` +
  `${hit.length} ran (${(100 * hit.length / functions.length).toFixed(1)}%), ${checklist.length - functions.length} constants not counted`);
if (missing.length) console.log(`not found in Showdown's raw data (${missing.length}): ${missing.slice(0, 12).join(' ')}`);
for (const kind of ['move', 'ability', 'item', 'condition', 'rule']) {
  const mine = rows.filter(r => r.kind === kind);
  const never = mine.filter(r => !r.runs);
  console.log(`${kind.padEnd(10)} ${String(mine.length - never.length).padStart(4)} of ${String(mine.length).padStart(4)} ran; never: ` +
    never.slice(0, 10).map(r => `${r.id}.${r.path}`).join(' ') + (never.length > 10 ? ` +${never.length - 10}` : ''));
}
const top = [...hit].sort((a, b) => b.runs - a.runs).slice(0, 12);
console.log('most run, per update: ' + top.map(r => `${r.id}.${r.path} ${(r.runs / updates).toFixed(2)}`).join(', '));

fs.mkdirSync(path.dirname(opts.out), {recursive: true});
fs.writeFileSync(opts.out, JSON.stringify({opts, updates, rows}, null, 1));
