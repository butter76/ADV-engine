// What Showdown's event system actually does in Gen 3 singles: how often each
// event is dispatched, how often anything listens, where handler lists are
// sorted and shuffled, and which call sites draw from the PRNG.
//
// Usage: SHOWDOWN=... node tools/census.cjs --teams DIR [--games N] [--format ID] [--out FILE]
'use strict';
const fs = require('fs');
const path = require('path');
const {showdown, loadTeams, lcg, play, gen5} = require('./lib.cjs');

const opts = {games: 1000, format: 'gen3ou', out: 'build/census.json'};
for (let i = 2; i < process.argv.length; i += 2) opts[process.argv[i].slice(2)] = process.argv[i + 1];
opts.games = +opts.games;

const S = showdown();
const teams = loadTeams(S.Teams, opts.teams);
const P = S.Battle.prototype;

// ---------------------------------------------------------------------------
// Static: every callback name some Gen 3 effect defines.
// ---------------------------------------------------------------------------
const dex = S.Dex.mod('gen3');
const defined = new Map(); // callback name -> Set of effect names
const note = (owner, obj) => {
  if (!obj || typeof obj !== 'object') return;
  for (const key of Object.keys(obj)) {
    const v = obj[key];
    if (typeof v === 'function') {
      if (!defined.has(key)) defined.set(key, new Set());
      defined.get(key).add(owner);
    }
  }
};
const legal = e => e.exists && e.gen <= 3 && !e.isNonstandard;
for (const m of dex.moves.all().filter(legal)) {
  note('move:' + m.id, m);
  note('move:' + m.id, m.condition);
  note('move:' + m.id, m.self);
  for (const s of m.secondaries || []) { note('move:' + m.id, s); note('move:' + m.id, s.self); }
}
for (const a of dex.abilities.all().filter(legal)) { note('ability:' + a.id, a); note('ability:' + a.id, a.condition); }
for (const it of dex.items.all().filter(legal)) { note('item:' + it.id, it); note('item:' + it.id, it.condition); }
for (const id of Object.keys(dex.data.Conditions)) note('condition:' + id, dex.conditions.get(id));
const format = dex.formats.get(opts.format);
note('format:' + format.id, format);
for (const rule of dex.formats.getRuleTable(format).keys()) {
  const sub = dex.formats.get(rule);
  if (sub.exists) note('rule:' + sub.id, sub);
}
const PREFIXES = ['', 'Ally', 'Foe', 'Any', 'Source', 'Side', 'Field'];
const live = event => PREFIXES.some(p => defined.has('on' + p + event));

// ---------------------------------------------------------------------------
// Dynamic: wrap the dispatchers.
// ---------------------------------------------------------------------------
const run = {}, single = {}, field = {}, each = {}, sorts = {}, draws = {};
const bump = (table, key, fieldName, by = 1) => {
  const row = table[key] || (table[key] = {});
  row[fieldName] = (row[fieldName] || 0) + by;
};
const ctx = [];
let depth = 0, drawn = 0;

const wrap = (name, body) => {
  const original = P[name];
  P[name] = function (...args) { return body.call(this, original, args); };
};
wrap('runEvent', function (original, args) {
  const [event, , , sourceEffect, , onEffect] = args;
  bump(run, event, 'calls');
  if (onEffect && sourceEffect && sourceEffect['on' + event] !== undefined) bump(run, event, 'own');
  ctx.push('run:' + event);
  try { return original.apply(this, args); } finally { ctx.pop(); }
});
wrap('findEventHandlers', function (original, args) {
  depth++;
  let found;
  try { found = original.apply(this, args); } finally { depth--; }
  if (!depth && ctx.length && ctx[ctx.length - 1] === 'run:' + args[1]) {
    bump(run, args[1], 'handlers', found.length);
    if (found.length) bump(run, args[1], 'nonempty');
  }
  return found;
});
wrap('singleEvent', function (original, args) {
  const [event, effect, , , , , , custom] = args;
  bump(single, event, 'calls');
  if (custom !== undefined || (effect && effect['on' + event] !== undefined)) bump(single, event, 'has');
  return original.apply(this, args);
});
wrap('fieldEvent', function (original, args) {
  bump(field, args[0], 'calls');
  ctx.push('field:' + args[0]);
  try { return original.apply(this, args); } finally { ctx.pop(); }
});
wrap('eachEvent', function (original, args) {
  bump(each, args[0], 'calls');
  ctx.push('each:' + args[0]);
  try { return original.apply(this, args); } finally { ctx.pop(); }
});
wrap('speedSort', function (original, args) {
  let key = ctx.length ? ctx[ctx.length - 1] : null;
  if (!key) {
    const frame = (new Error().stack.split('\n')[3] || '').trim();
    key = 'other:' + (frame.match(/at (\S+)/) || [, '?'])[1];
  }
  const before = drawn;
  bump(sorts, key, 'calls');
  if (args[0].length > 1) bump(sorts, key, 'many');
  if (key.startsWith('field:')) bump(field, key.slice(6), 'handlers', args[0].length);
  const result = original.apply(this, args);
  if (drawn > before) { bump(sorts, key, 'shuffles'); bump(sorts, key, 'draws', drawn - before); }
  return result;
});

// A draw's call site: the first two frames above the PRNG.
const site = () => {
  const frames = new Error().stack.split('\n').slice(1)
    .filter(f => !f.includes('prng.js') && !f.includes('census.cjs'))
    .map(f => {
      const m = f.match(/at (?:(\S+) \()?.*?([^/]+)\.js:(\d+)/);
      if (!m) return '?';
      const name = m[1] || '<anonymous>';
      return /^(moves|abilities|items|conditions|scripts|rulesets)$/.test(m[2]) ? `${name} ${m[2]}:${m[3]}` : name;
    });
  let key = frames[0];
  if (key === 'Battle.speedSort') key += ' < ' + (ctx.length ? ctx[ctx.length - 1] : frames[1]);
  else if (/^Battle\.(random|randomChance|sample)$/.test(key)) key = frames[1] + (ctx.length ? ' in ' + ctx[ctx.length - 1] : '');
  return key;
};

let updates = 0, turns = 0;
const rnd = lcg(20261123);
for (let g = 0; g < opts.games; g++) {
  const pair = [teams[rnd(teams.length)], teams[rnd(teams.length)]];
  const {battle, updates: n} = play(S, {
    format: opts.format, seed: gen5(g + 1), teams: pair, rnd,
    before(b) {
      const random = b.prng.random;
      b.prng.random = function (...a) { drawn++; bump(draws, site(), 'n'); return random.apply(this, a); };
    },
  });
  updates += n;
  turns += battle.turn;
}

// ---------------------------------------------------------------------------
// Report.
// ---------------------------------------------------------------------------
const per = n => (n || 0) / updates;
const fmt = (n, d = 2) => per(n).toFixed(d).padStart(8);
const total = (table, f) => Object.values(table).reduce((s, r) => s + (r[f] || 0), 0);
console.log(`${opts.format}: ${opts.games} games, ${updates} updates, ${(turns / opts.games).toFixed(1)} turns per game, ${teams.length} teams`);
console.log(`defined callback names in Gen 3: ${defined.size}`);

console.log('\nrunEvent, per update (own = the source effect\'s own handler, added after the search)');
console.log('event'.padEnd(26) + '   calls nonempty handlers      own  listened  shuffles');
let dead = 0, empty = 0;
for (const [event, r] of Object.entries(run).sort((a, b) => b[1].calls - a[1].calls)) {
  const isLive = live(event);
  if (!isLive) dead += r.calls;
  const none = r.calls - Math.max(r.nonempty || 0, r.own || 0);
  empty += Math.max(0, none);
  const s = sorts['run:' + event] || {};
  console.log(event.padEnd(26) + fmt(r.calls) + fmt(r.nonempty) + fmt(r.handlers) + fmt(r.own) +
    (isLive ? '       yes' : '     never') + fmt(s.shuffles, 3));
}
const runCalls = total(run, 'calls');
console.log(`total ${per(runCalls).toFixed(1)} per update; ${(100 * dead / runCalls).toFixed(1)}% are events nothing in Gen 3 listens to; ` +
  `${(100 * empty / runCalls).toFixed(1)}% find no handler`);

console.log('\nsingleEvent, per update');
console.log('event'.padEnd(26) + '   calls  has callback');
for (const [event, r] of Object.entries(single).sort((a, b) => b[1].calls - a[1].calls)) {
  console.log(event.padEnd(26) + fmt(r.calls) + fmt(r.has));
}
console.log(`total ${per(total(single, 'calls')).toFixed(1)} per update, ${per(total(single, 'has')).toFixed(1)} with a callback`);

console.log('\nfieldEvent and eachEvent, per update');
for (const [event, r] of Object.entries(field)) console.log(('field ' + event).padEnd(26) + fmt(r.calls) + '  handlers' + fmt(r.handlers));
for (const [event, r] of Object.entries(each)) console.log(('each ' + event).padEnd(26) + fmt(r.calls));

console.log('\nspeedSort, per update');
console.log('where'.padEnd(34) + '   calls  2+ items shuffles    draws');
for (const [key, r] of Object.entries(sorts).sort((a, b) => (b[1].draws || 0) - (a[1].draws || 0) || b[1].calls - a[1].calls).slice(0, 24)) {
  console.log(key.padEnd(34) + fmt(r.calls) + fmt(r.many) + fmt(r.shuffles, 3) + fmt(r.draws, 3));
}
console.log(`total ${per(total(sorts, 'calls')).toFixed(1)} sorts, ${per(total(sorts, 'many')).toFixed(2)} of 2+, ${per(total(sorts, 'draws')).toFixed(3)} draws per update`);

console.log('\nPRNG draws by call site, per update');
const allDraws = total(draws, 'n');
for (const [key, r] of Object.entries(draws).sort((a, b) => b[1].n - a[1].n).slice(0, 40)) {
  console.log(fmt(r.n, 3) + ' ' + (100 * r.n / allDraws).toFixed(1).padStart(5) + '%  ' + key);
}
console.log(`total ${per(allDraws).toFixed(2)} draws per update over ${Object.keys(draws).length} sites`);

fs.mkdirSync(path.dirname(opts.out), {recursive: true});
fs.writeFileSync(opts.out, JSON.stringify({
  opts, updates, turns, run, single, field, each, sorts, draws,
  defined: Object.fromEntries([...defined].map(([k, v]) => [k, [...v].sort()])),
}, null, 1));
