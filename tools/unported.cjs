// Lists the checklist's callbacks (build/dex/callbacks.json, from
// tools/dump_dex.cjs) that src/callbacks.hpp does not register, by kind.
//
// Usage: node tools/unported.cjs [--show ID ...]
//
// --show prints Showdown's source, as Gen 3 resolves it, of every callback of
// the effects named, registered or not.
'use strict';
const fs = require('fs');
const path = require('path');

const root = path.join(__dirname, '..');
const {callbacks} = JSON.parse(fs.readFileSync(path.join(root, 'build/dex/callbacks.json'), 'utf8'));
const src = fs.readFileSync(path.join(root, 'src/callbacks.hpp'), 'utf8');

// A registration is `R/A/I/C(Ident, onName, fn)` or `{effect(Move::Ident), Callback::onName, &fn}`.
const registered = new Set();
for (const m of src.matchAll(/\b([RAIC])\((\w+), (\w+), \w+\)/g)) registered.add(`${m[1]} ${m[2].toLowerCase()} ${m[3]}`);
for (const m of src.matchAll(/\{effect\(Move::(\w+)\), Callback::(\w+),/g)) registered.add(`M ${m[1].toLowerCase()} ${m[2]}`);

const LETTER = {move: 'M', ability: 'A', item: 'I', condition: 'C', rule: 'R'};
const key = c => `${c.path.startsWith('condition.') ? 'C' : LETTER[c.kind]} ${c.id} ${c.path.split('.').pop()}`;

const show = process.argv[2] === '--show' ? process.argv.slice(3) : null;
if (show) {
  for (const c of callbacks) {
    if (!show.includes(c.id)) continue;
    console.log(`== ${c.kind} ${c.id} ${c.path}${registered.has(key(c)) ? '  [registered]' : ''}\n${c.source}`);
  }
} else {
  const left = new Map();
  for (const c of callbacks) {
    if (registered.has(key(c))) continue;
    const nested = c.path.includes('.');
    const group = c.kind + (nested ? '/' + c.path.slice(0, c.path.lastIndexOf('.')) : '');
    if (!left.has(group)) left.set(group, []);
    left.get(group).push(`${c.id}.${c.path.split('.').pop()}`);
  }
  let total = 0;
  for (const [group, list] of left) {
    total += list.length;
    console.log(`${group} ${list.length}\n  ${list.join(' ')}`);
  }
  console.log(`not registered: ${total} of ${callbacks.length}`);
}
