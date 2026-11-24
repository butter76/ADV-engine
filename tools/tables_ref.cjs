// The reference side of tests/data_test.cpp: Gen 3's static data read straight
// from the pinned Showdown and written one fact per line, sorted. The C++
// tables must print the same lines. It shares no code with
// tools/gen_tables.cjs, so an error in the generator's encoding shows here.
//
// Usage: SHOWDOWN=... node tools/tables_ref.cjs [--format ID] > tests/fixtures/tables.txt
'use strict';
const {showdown} = require('./lib.cjs');

const opts = {format: 'gen3ou'};
for (let i = 2; i < process.argv.length; i += 2) opts[process.argv[i].slice(2)] = process.argv[i + 1];

const {Dex} = showdown();
const dex = Dex.mod('gen3');
const legal = e => e.exists && e.gen <= 3 && !e.isNonstandard;
const id = v => (v === undefined || v === null || v === '' ? '-' : String(v).toLowerCase().replace(/[^a-z0-9]+/g, '') || 'unknown');
const bit = v => (v ? '1' : '0');
const STATS = ['atk', 'def', 'spa', 'spd', 'spe', 'accuracy', 'evasion'];
const boosts = b => STATS.map(s => (b && b[s]) || 0).join(',');
const frac = f => (f ? `${f[0]}/${f[1]}` : '-');
const lines = [];

const species = dex.species.all().filter(legal);
const here = new Set(species.map(s => s.id));
for (const s of species) {
  const b = s.baseStats;
  const ratio = s.genderRatio || {F: 0.5};
  lines.push(['species', s.id, s.num, id(s.types[0]), id(s.types[1]), b.hp, b.atk, b.def, b.spa, b.spd, b.spe,
    id(s.abilities[0]), id(s.abilities[1]), s.weighthg, s.maxHP || 0, s.gender || '-', Math.round(ratio.F * 8), id(s.baseSpecies)].join(' '));
}

for (const m of dex.moves.all().filter(legal)) {
  const sec = m.secondary || {}, self = m.self || {};
  const hits = m.multihit === undefined ? '-' : Array.isArray(m.multihit) ? m.multihit.join('-') : `${m.multihit}-${m.multihit}`;
  lines.push(['move', id(m.name), m.id, m.num, id(m.type), m.category, m.basePower, m.accuracy === true ? 'true' : m.accuracy, m.pp,
    m.priority, m.target, m.critRatio, Object.keys(m.flags).sort().join(',') || '-',
    m.damage === undefined || m.damage === null ? '-' : m.damage, frac(m.drain), frac(m.recoil), frac(m.heal), hits,
    m.selfdestruct || '-', m.selfSwitch || '-',
    [m.multiaccuracy, m.ohko, m.forceSwitch, m.ignoreImmunity, m.noPPBoosts, m.sleepUsable, m.stallingMove, m.callsMove,
      m.hasCrashDamage].map(bit).join(''),
    id(m.status), id(m.volatileStatus), id(m.sideCondition), id(m.slotCondition), id(m.weather), boosts(m.boosts),
    sec.chance || 0, id(sec.status), id(sec.volatileStatus), boosts(sec.boosts), boosts(sec.self && sec.self.boosts),
    boosts(self.boosts), id(self.volatileStatus)].join(' '));
}

for (const it of dex.items.all().filter(legal)) {
  const users = (it.itemUser || []).map(id).filter(u => here.has(u));
  lines.push(['item', it.id, it.num, bit(it.isBerry) + bit(it.isChoice) + bit(it.isPokeball), users.join(',') || '-'].join(' '));
}
for (const a of dex.abilities.all().filter(legal)) lines.push(['ability', a.id, a.num, bit(a.suppressWeather)].join(' '));

const condition = (cid, holder, c) => lines.push(['condition', cid, holder,
  holder === 'standalone' ? c.effectType.toLowerCase() : 'condition', c.duration || 0, c.counterMax || 0, bit(c.noCopy),
  bit(c.affectsFainted)].join(' '));
for (const cid of Object.keys(dex.data.Conditions)) condition(cid, 'standalone', dex.conditions.get(cid));
for (const m of dex.moves.all().filter(legal)) if (m.condition) condition(m.id, 'move', m.condition);
for (const a of dex.abilities.all().filter(legal)) if (a.condition) condition(a.id, 'ability', a.condition);

const types = dex.types.all().filter(t => t.exists && t.gen <= 3 && !t.isNonstandard);
for (const d of types) {
  for (const [key, value] of Object.entries(d.damageTaken)) {
    if (!value) continue;
    if (/^[a-z]/.test(key)) lines.push(`immune ${id(d.name)} ${key} ${value}`);
    else if (types.some(t => t.name === key)) lines.push(`chart ${id(d.name)} ${id(key)} ${value}`);
  }
}
for (const n of dex.natures.all()) lines.push(`nature ${n.id} ${n.plus || '-'} ${n.minus || '-'}`);

// Handlers that are gathered for an event: an effect's own, and those of the
// condition it carries.
const rank = /^on[A-Z].*(Priority|Order|SubOrder)$/;
const handlers = (kind, eid, e) => {
  for (const key of Object.keys(e || {})) {
    const v = e[key];
    if (rank.test(key) || v === undefined) continue;
    if (typeof v !== 'function' && !/^on[A-Z]/.test(key)) continue;
    lines.push(['handler', kind, eid, key, e[key + 'Order'] || 0, e[key + 'Priority'] || 0, e[key + 'SubOrder'] || 0].join(' '));
  }
  // A rank given under a name the effect has no handler for.
  const bases = new Set(Object.keys(e || {}).filter(k => rank.test(k)).map(k => k.replace(/(SubOrder|Order|Priority)$/, '')));
  for (const key of bases) {
    if (e[key] !== undefined) continue;
    if (!e[key + 'Order'] && !e[key + 'Priority'] && !e[key + 'SubOrder']) continue;
    lines.push(['rank', kind, eid, key, e[key + 'Order'] || 0, e[key + 'Priority'] || 0, e[key + 'SubOrder'] || 0].join(' '));
  }
};
for (const m of dex.moves.all().filter(legal)) { handlers('move', id(m.name), m); handlers('condition', m.id, m.condition); }
for (const a of dex.abilities.all().filter(legal)) { handlers('ability', a.id, a); handlers('condition', a.id, a.condition); }
for (const it of dex.items.all().filter(legal)) handlers('item', it.id, it);
for (const cid of Object.keys(dex.data.Conditions)) handlers('condition', cid, dex.conditions.get(cid));
const format = dex.formats.get(opts.format);
for (const r of [format, ...[...dex.formats.getRuleTable(format).keys()].map(k => dex.formats.get(k)).filter(k => k.exists)]) {
  handlers('rule', r.id, r);
}

lines.sort();
process.stdout.write(lines.join('\n') + '\n');
