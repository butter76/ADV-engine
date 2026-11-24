// Gen 3 as the pinned Showdown resolves it: the data tables with every mod's
// inheritance applied, and the list of callbacks a port has to carry.
//
// Usage: SHOWDOWN=... node tools/dump_dex.cjs [--format ID] [--out DIR] [--census FILE]
'use strict';
const fs = require('fs');
const path = require('path');
const {showdown, COMMIT} = require('./lib.cjs');
const {build} = require('./dex.cjs');

const opts = {format: 'gen3ou', out: 'build/dex', census: 'build/census.json'};
for (let i = 2; i < process.argv.length; i += 2) opts[process.argv[i].slice(2)] = process.argv[i + 1];

const S = showdown();
const {dex, plain, callbacks} = build(S, opts.format);

// ---------------------------------------------------------------------------
// Which standalone conditions Gen 3 can reach: named by the simulator's own
// source, or by the source or static fields of something already reachable.
// A coarse test (a string literal anywhere counts), so it only over-includes.
// ---------------------------------------------------------------------------
const conditionIds = new Set(plain.condition.map(c => c.id));
const core = ['battle', 'battle-actions', 'pokemon', 'side', 'field', 'battle-queue']
  .map(f => fs.readFileSync(path.join(S.root, 'dist', 'sim', f + '.js'), 'utf8')).join('\n');
const scripts = JSON.stringify(dex.data.Scripts, (k, v) => (typeof v === 'function' ? v.toString() : v));
const words = text => new Set(text.toLowerCase().match(/[a-z0-9]+/g) || []);
const reach = new Set();
const frontier = [];
const absorb = text => { for (const w of words(text)) if (conditionIds.has(w) && !reach.has(w)) { reach.add(w); frontier.push(w); } };
absorb(core);
absorb(scripts);
for (const kind of ['move', 'ability', 'item', 'rule']) absorb(JSON.stringify(plain[kind]));
for (const c of callbacks) if (c.kind !== 'condition') absorb(c.source);
while (frontier.length) {
  const id = frontier.pop();
  absorb(JSON.stringify(plain.condition.find(c => c.id === id)));
  for (const c of callbacks) if (c.kind === 'condition' && c.id === id) absorb(c.source);
}
const reachable = c => c.kind !== 'condition' || reach.has(c.id);

// ---------------------------------------------------------------------------
// Write and report.
// ---------------------------------------------------------------------------
fs.mkdirSync(opts.out, {recursive: true});
for (const [kind, list] of Object.entries(plain)) {
  fs.writeFileSync(path.join(opts.out, kind + '.json'), JSON.stringify(list, null, 1));
}
const kept = callbacks.filter(reachable);
fs.writeFileSync(path.join(opts.out, 'callbacks.json'), JSON.stringify({commit: COMMIT, format: opts.format, callbacks: kept}, null, 1));

console.log(`Showdown ${COMMIT.slice(0, 7)}, ${opts.format}`);
for (const kind of ['species', 'move', 'ability', 'item', 'condition', 'rule']) {
  const mine = callbacks.filter(c => c.kind === kind);
  const live = mine.filter(reachable);
  const distinct = new Map(live.map(c => [c.hash, c.lines]));
  console.log(`${kind.padEnd(10)} ${String(plain[kind].length).padStart(4)} entries, ${String(new Set(live.map(c => c.id)).size).padStart(4)} with code, ` +
    `${String(live.length).padStart(4)} handlers (${live.filter(c => c.constant).length} constants), ` +
    `${String([...distinct.values()].reduce((a, b) => a + b, 0)).padStart(5)} distinct lines` +
    (kind === 'condition' ? `  (${reach.size} of ${conditionIds.size} reachable)` : ''));
}
console.log('unreachable conditions: ' + [...conditionIds].filter(id => !reach.has(id)).join(' '));
console.log('rules with code: ' + [...new Set(callbacks.filter(c => c.kind === 'rule').map(c => c.id))].join(' '));

const byGen = {};
for (const c of kept) if (c.gen) byGen[c.gen] = (byGen[c.gen] || 0) + 1;
console.log('callbacks by the generation whose files define them (9 = the base data): ' +
  Object.entries(byGen).map(([g, n]) => `gen${g} ${n}`).join(', '));

const byName = {};
for (const c of kept) byName[c.name] = (byName[c.name] || 0) + 1;
const names = Object.entries(byName).sort((a, b) => b[1] - a[1]);
console.log(`${names.length} callback names; the most used: ` + names.slice(0, 14).map(([n, k]) => `${n} ${k}`).join(', '));

// With the census: how many dispatches are for events no reachable effect
// listens to, under any prefix.
if (fs.existsSync(opts.census)) {
  const census = JSON.parse(fs.readFileSync(opts.census, 'utf8'));
  const listened = new Set(kept.map(c => c.name));
  const PREFIXES = ['', 'Ally', 'Foe', 'Any', 'Source', 'Side', 'Field'];
  let all = 0, dead = 0;
  const deadEvents = [];
  for (const [event, r] of Object.entries(census.run)) {
    all += r.calls;
    if (!PREFIXES.some(p => listened.has('on' + p + event))) { dead += r.calls; deadEvents.push([event, r.calls]); }
  }
  deadEvents.sort((a, b) => b[1] - a[1]);
  console.log(`runEvent: ${(100 * dead / all).toFixed(1)}% of dispatches are for ${deadEvents.length} events nothing reachable listens to: ` +
    deadEvents.slice(0, 12).map(([e, n]) => `${e} ${(n / census.updates).toFixed(2)}`).join(', '));
  let sAll = 0, sDead = 0;
  for (const [event, r] of Object.entries(census.single)) {
    sAll += r.calls;
    if (!listened.has('on' + event)) sDead += r.calls;
  }
  console.log(`singleEvent: ${(100 * sDead / sAll).toFixed(1)}% of calls are for names nothing reachable defines`);
}
