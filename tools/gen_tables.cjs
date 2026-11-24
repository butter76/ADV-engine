// Writes src/data/*: the engine's static tables, from the pinned Showdown's
// Gen 3 data. Every field of a move, item, ability or condition is either
// carried into a table, listed below as not needed, or listed as a constant
// with the value it must have. A field in none of the lists stops the run, so
// a new pin cannot bring in data the engine silently ignores.
//
// Usage: SHOWDOWN=... node tools/gen_tables.cjs [--format ID] [--out DIR]
'use strict';
const fs = require('fs');
const path = require('path');
const {showdown, COMMIT} = require('./lib.cjs');
const {build, isRank} = require('./dex.cjs');

const opts = {format: 'gen3ou', out: 'src/data'};
for (let i = 2; i < process.argv.length; i += 2) opts[process.argv[i].slice(2)] = process.argv[i + 1];

const S = showdown();
const {plain, callbacks} = build(S, opts.format);
const toID = text => String(text).toLowerCase().replace(/[^a-z0-9]+/g, '');

// ---------------------------------------------------------------------------
// Names.
// ---------------------------------------------------------------------------
const ident = name => {
  if (name === '???') return 'Unknown';
  const out = String(name).replace(/[’']/g, '').split(/[^A-Za-z0-9]+/).filter(Boolean)
    .map(w => w[0].toUpperCase() + w.slice(1)).join('');
  return /^[0-9]/.test(out) ? '_' + out : out;
};
const unique = (kind, list) => {
  const seen = new Map();
  for (const e of list) {
    if (seen.has(e.ident)) throw new Error(`${kind}: ${e.id} and ${seen.get(e.ident)} are both ${e.ident}`);
    seen.set(e.ident, e.id);
  }
  return list;
};
const byNum = list => list.map((e, i) => [e, i]).sort((a, b) => a[0].num - b[0].num || a[1] - b[1]).map(p => p[0]);

const species = unique('species', byNum(plain.species).map(e => ({...e, ident: ident(e.name)})));
const moves = unique('move', byNum(plain.move).map(e => ({...e, ident: ident(e.name)})));
const abilities = unique('ability', byNum(plain.ability).map(e => ({...e, ident: ident(e.name)})));
const items = unique('item', byNum(plain.item).map(e => ({...e, ident: ident(e.name)})));
const natures = unique('nature', plain.nature.map(e => ({...e, ident: ident(e.name)})));
const rules = unique('rule', plain.rule.map(e => ({...e, ident: ident(e.name.replace(/^\[Gen 3\] /, 'Format '))})));
const typeNames = [...new Set([...plain.type.map(t => t.name), ...moves.map(m => m.type)])];
const types = typeNames.map(name => ({id: toID(name) || 'unknown', name, ident: ident(name)}));

// Conditions: the standalone ones, then those a move or an ability carries
// under its own id.
const conditions = plain.condition.map(c => ({...c, ident: ident(c.id), holder: 'Standalone'}));
for (const [kind, list] of [['Move', moves], ['Ability', abilities]]) {
  for (const e of list) {
    if (!e.condition && !callbacks.some(c => c.kind === kind.toLowerCase() && c.id === e.id && c.path.startsWith('condition.'))) continue;
    conditions.push({...(e.condition || {}), id: e.id, name: e.name, ident: ident(e.name), holder: kind});
  }
}
unique('condition', conditions);
if (new Set(conditions.map(c => c.id)).size !== conditions.length) throw new Error('two conditions share an id');

const index = list => new Map(list.map((e, i) => [e.id, i]));
const at = {
  species: index(species), move: index(moves), ability: index(abilities), item: index(items),
  condition: index(conditions), type: index(types), rule: index(rules),
};
const ref = (kind, list, value) => {
  if (value === undefined || value === null || value === '') return 'None';
  const i = at[kind].get(toID(value) || 'unknown');
  if (i === undefined) throw new Error(`no ${kind} "${value}"`);
  return list[i].ident;
};

// ---------------------------------------------------------------------------
// The field lists.
// ---------------------------------------------------------------------------
const DISPLAY = ['id', 'name', 'fullname', 'effectType', 'exists', 'gen', 'isNonstandard', 'desc', 'shortDesc',
  'rating', 'contestType', 'spritenum', 'num'];
const FIELDS = {
  move: {
    used: ['type', 'category', 'basePower', 'accuracy', 'pp', 'priority', 'target', 'flags', 'critRatio', 'damage',
      'drain', 'recoil', 'heal', 'multihit', 'multiaccuracy', 'ohko', 'selfdestruct', 'selfSwitch', 'forceSwitch',
      'status', 'volatileStatus', 'sideCondition', 'slotCondition', 'weather', 'boosts', 'secondary', 'self',
      'ignoreImmunity', 'noPPBoosts', 'sleepUsable', 'stallingMove', 'callsMove', 'hasCrashDamage', 'condition'],
    // Not read by anything Gen 3 singles runs.
    unused: ['zMove', 'maxMove', 'baseMoveType', 'realMove', 'realId', 'placeholderFor', 'nonGhostTarget', 'pressureTarget',
      'isViable', 'smartTarget', 'tracksTarget', 'noSketch', 'secondaries'],
    constant: {affectsFainted: false, forceSTAB: false, hasSheerForceBoost: false, ignoreAbility: false,
      ignoreDefensive: false, ignoreNegativeOffensive: false, ignoreOffensive: false,
      ignorePositiveDefensive: false, isMax: false, isZ: false, noCopy: false, spreadHit: false,
      sourceEffect: '', struggleRecoil: false},
  },
  item: {
    used: ['isBerry', 'isChoice', 'isPokeball', 'itemUser'],
    unused: ['naturalGift', 'ignoreKlutz', 'boosts'],
    constant: {affectsFainted: false, isGem: false, isPrimalOrb: false, noCopy: false, sourceEffect: ''},
  },
  ability: {
    used: ['suppressWeather', 'condition'],
    unused: ['flags'],
    constant: {affectsFainted: false, noCopy: false, sourceEffect: ''},
  },
  condition: {
    used: ['duration', 'noCopy', 'counterMax', 'affectsFainted'],
    unused: ['ident', 'holder'],
    constant: {sourceEffect: ''},
  },
};
const unknown = [];
function check(kind, e, used) {
  const f = FIELDS[kind];
  for (const key of Object.keys(e)) {
    if (DISPLAY.includes(key) || key === 'ident' || isRank(key) || f.unused.includes(key)) continue;
    if (key in f.constant) {
      if (e[key] !== f.constant[key]) unknown.push(`${kind} ${e.id}: ${key} is ${JSON.stringify(e[key])}, not ${JSON.stringify(f.constant[key])}`);
    } else if (!f.used.includes(key)) {
      unknown.push(`${kind} ${e.id}: ${key} = ${JSON.stringify(e[key]).slice(0, 60)}`);
    } else {
      used.add(key);
    }
  }
}
const only = (what, obj, keys) => {
  for (const key of Object.keys(obj || {})) if (!keys.includes(key)) unknown.push(`${what}: ${key} = ${JSON.stringify(obj[key]).slice(0, 60)}`);
};

// ---------------------------------------------------------------------------
// Emitters.
// ---------------------------------------------------------------------------
const files = {};
const header = what => `// Generated by tools/gen_tables.cjs from Showdown ${COMMIT.slice(0, 7)}: ${what}.\n// Do not edit.\n`;
const fields = pairs => '{' + pairs.filter(Boolean).join(', ') + '}';
const STATS = ['atk', 'def', 'spa', 'spd', 'spe', 'accuracy', 'evasion'];
const boosts = b => {
  only('boosts', b, STATS);
  return fields(STATS.map(s => (b[s] ? `.${s} = ${b[s]}` : '')));
};
const fraction = f => `{${f[0]}, ${f[1]}}`;
const flag = (key, value) => (value ? `.${key} = true` : '');
const text = s => JSON.stringify(String(s));

const TARGETS = [...new Set(moves.map(m => m.target))];
const FLAGS = [...new Set(moves.flatMap(m => Object.keys(m.flags)))].sort();
// A rank (`onSwitchInPriority`) can be given for a name the effect has no
// handler under: Showdown reads an ability's `onStart` as its `onSwitchIn`
// and ranks it by that name. Those names are callbacks too.
const rankBase = key => key.replace(/(SubOrder|Order|Priority)$/, '');
const rankNames = e => [...new Set(Object.keys(e || {}).filter(isRank).map(rankBase))];
const CALLBACKS = [...new Set([
  ...callbacks.map(c => c.name),
  ...[...moves, ...abilities, ...items, ...rules, ...conditions].flatMap(e => [...rankNames(e), ...rankNames(e.condition)]),
])].sort();
// A type's damageTaken has every generation's types, capitalized, and then the
// other things a type can be immune to, in lower case.
const IMMUNITIES = [...new Set(plain.type.flatMap(t => Object.keys(t.damageTaken).filter(k => /^[a-z]/.test(k))))].sort();

// ids.hpp
{
  const list = (name, base, entries) =>
    `enum class ${name} : ${base} {\n  None,\n${entries.map(e => `  ${e},`).join('\n')}\n};\n` +
    `inline constexpr int ${name.toUpperCase()}S = ${entries.length + 1};\n`;
  const plainEnum = (name, base, entries) => `enum class ${name} : ${base} {\n${entries.map(e => `  ${e},`).join('\n')}\n};\n`;
  files['ids.hpp'] = header('every id') + `
#pragma once

#include <cstdint>

namespace adv {

${list('Type', 'uint8_t', types.map(e => e.ident))}
${list('Species', 'uint16_t', species.map(e => e.ident))}
${list('Move', 'uint16_t', moves.map(e => e.ident))}
${list('Ability', 'uint8_t', abilities.map(e => e.ident))}
${list('Item', 'uint8_t', items.map(e => e.ident))}
${list('Condition', 'uint8_t', conditions.map(e => e.ident))}
${list('Rule', 'uint8_t', rules.map(e => e.ident))}
${list('Nature', 'uint8_t', natures.map(e => e.ident))}
// A move's target.
${plainEnum('Target', 'uint8_t', TARGETS.map(ident))}
inline constexpr int TARGETS = ${TARGETS.length};

// A move's flags, as bits of MoveData::flags.
${plainEnum('MoveFlag', 'uint8_t', FLAGS.map(ident))}
inline constexpr int MOVE_FLAGS = ${FLAGS.length};

// What a type can be immune to besides a type (Showdown's damageTaken keys).
${plainEnum('Immunity', 'uint8_t', IMMUNITIES.map(ident))}
inline constexpr int IMMUNITIES = ${IMMUNITIES.length};

// Every name a handler has in Gen 3.
${plainEnum('Callback', 'uint8_t', CALLBACKS)}
inline constexpr int CALLBACKS = ${CALLBACKS.length};

} // namespace adv
`;
}

// names.inc: Showdown's id and display name of everything with an id.
{
  const table = (name, list) =>
    `inline constexpr Name ${name}[] = {\n  {"", ""},\n${list.map(e => `  {${text(e.id)}, ${text(e.name)}},`).join('\n')}\n};\n`;
  const words = (name, list) => `inline constexpr std::string_view ${name}[] = {\n${list.map(e => `  ${text(e)},`).join('\n')}\n};\n`;
  files['names.inc'] = header('ids and display names') + [
    table('TYPE_NAMES', types), table('SPECIES_NAMES', species), table('MOVE_NAMES', moves),
    table('ABILITY_NAMES', abilities), table('ITEM_NAMES', items), table('CONDITION_NAMES', conditions),
    table('RULE_NAMES', rules), table('NATURE_NAMES', natures),
    words('TARGET_NAMES', TARGETS), words('MOVE_FLAG_NAMES', FLAGS), words('IMMUNITY_NAMES', IMMUNITIES),
    words('CALLBACK_NAMES', CALLBACKS),
  ].join('\n');
}

// species.inc
files['species.inc'] = header('species') + species.map(s => {
  const b = s.baseStats;
  const abilityKeys = Object.keys(s.abilities);
  if (abilityKeys.some(k => k !== '0' && k !== '1')) unknown.push(`species ${s.id}: abilities ${abilityKeys}`);
  const ratio = s.genderRatio || {M: 0.5, F: 0.5};
  return '  ' + fields([
    `.num = ${s.num}`,
    `.types = {Type::${ref('type', types, s.types[0])}, Type::${ref('type', types, s.types[1])}}`,
    `.base = {${b.hp}, ${b.atk}, ${b.def}, ${b.spa}, ${b.spd}, ${b.spe}}`,
    `.abilities = {Ability::${ref('ability', abilities, s.abilities[0])}, Ability::${ref('ability', abilities, s.abilities[1])}}`,
    `.weight_hg = ${s.weighthg}`, s.maxHP ? `.max_hp = ${s.maxHP}` : '',
    `.gender = Gender::${{M: 'Male', F: 'Female', N: 'None', '': 'Either'}[s.gender || '']}`,
    `.female_eighths = ${Math.round(ratio.F * 8)}`,
    `.base_species = Species::${ref('species', species, s.baseSpecies)}`,
  ]) + `, // ${s.name}`;
}).join('\n') + '\n';

// moves.inc
{
  const used = new Set();
  files['moves.inc'] = header('moves') + moves.map(m => {
    check('move', m, used);
    only(`move ${m.id} secondary`, m.secondary, ['chance', 'status', 'volatileStatus', 'boosts', 'self']);
    only(`move ${m.id} secondary.self`, m.secondary && m.secondary.self, ['boosts']);
    only(`move ${m.id} self`, m.self, ['boosts', 'volatileStatus']);
    if (typeof m.ignoreImmunity !== 'boolean') unknown.push(`move ${m.id}: ignoreImmunity ${JSON.stringify(m.ignoreImmunity)}`);
    const sec = m.secondary, self = m.self;
    const multihit = m.multihit === undefined ? null : Array.isArray(m.multihit) ? m.multihit : [m.multihit, m.multihit];
    return '  ' + fields([
      `.num = ${m.num}`, m.realId !== m.id ? `.real_move = Move::${ref('move', moves, m.realId)}` : '',
      `.type = Type::${ref('type', types, m.type)}`, `.category = Category::${m.category}`,
      `.base_power = ${m.basePower}`, `.accuracy = ${m.accuracy === true ? 0 : m.accuracy}`, `.pp = ${m.pp}`,
      `.priority = ${m.priority}`, `.target = Target::${ident(m.target)}`, `.crit_ratio = ${m.critRatio}`,
      `.flags = ${Object.keys(m.flags).sort().map(f => `flag(MoveFlag::${ident(f)})`).join(' | ') || '0'}`,
      m.damage === 'level' ? '.damage_level = true' : m.damage ? `.damage = ${m.damage}` : '',
      m.drain ? `.drain = ${fraction(m.drain)}` : '', m.recoil ? `.recoil = ${fraction(m.recoil)}` : '',
      m.heal ? `.heal = ${fraction(m.heal)}` : '', multihit ? `.multihit = {${multihit[0]}, ${multihit[1]}}` : '',
      m.selfdestruct ? `.selfdestruct = SelfDestruct::${ident(m.selfdestruct)}` : '',
      m.selfSwitch ? `.self_switch = SelfSwitch::${ident(String(m.selfSwitch))}` : '',
      flag('multiaccuracy', m.multiaccuracy), flag('ohko', m.ohko), flag('force_switch', m.forceSwitch),
      flag('ignore_immunity', m.ignoreImmunity), flag('no_pp_boosts', m.noPPBoosts), flag('sleep_usable', m.sleepUsable),
      flag('stalling_move', m.stallingMove), flag('calls_move', m.callsMove), flag('has_crash_damage', m.hasCrashDamage),
      m.status ? `.status = Condition::${ref('condition', conditions, m.status)}` : '',
      m.volatileStatus ? `.volatile_status = Condition::${ref('condition', conditions, m.volatileStatus)}` : '',
      m.sideCondition ? `.side_condition = Condition::${ref('condition', conditions, m.sideCondition)}` : '',
      m.slotCondition ? `.slot_condition = Condition::${ref('condition', conditions, m.slotCondition)}` : '',
      m.weather ? `.weather = Condition::${ref('condition', conditions, m.weather)}` : '',
      m.boosts ? `.boosts = ${boosts(m.boosts)}` : '',
      sec ? '.secondary = ' + fields([
        `.chance = ${sec.chance}`,
        sec.status ? `.status = Condition::${ref('condition', conditions, sec.status)}` : '',
        sec.volatileStatus ? `.volatile_status = Condition::${ref('condition', conditions, sec.volatileStatus)}` : '',
        sec.boosts ? `.boosts = ${boosts(sec.boosts)}` : '',
        sec.self ? `.self_boosts = ${boosts(sec.self.boosts)}` : '',
      ]) : '',
      self && self.boosts ? `.self_boosts = ${boosts(self.boosts)}` : '',
      self && self.volatileStatus ? `.self_volatile_status = Condition::${ref('condition', conditions, self.volatileStatus)}` : '',
    ]) + `, // ${m.name}`;
  }).join('\n') + '\n';
  for (const key of FIELDS.move.used) if (!used.has(key)) unknown.push(`move: no move has ${key}`);
}

// items.inc, abilities.inc, conditions.inc
files['items.inc'] = header('items') + items.map(it => {
  check('item', it, new Set());
  return '  ' + fields([
    `.num = ${it.num}`, flag('is_berry', it.isBerry), flag('is_choice', it.isChoice), flag('is_pokeball', it.isPokeball),
    it.itemUser ? `.users = {${it.itemUser.filter(u => at.species.has(toID(u))).map(u => `Species::${ref('species', species, u)}`).join(', ')}}` : '',
  ]) + `, // ${it.name}`;
}).join('\n') + '\n';
files['abilities.inc'] = header('abilities') + abilities.map(a => {
  check('ability', a, new Set());
  return '  ' + fields([`.num = ${a.num}`, flag('suppress_weather', a.suppressWeather)]) + `, // ${a.name}`;
}).join('\n') + '\n';
files['conditions.inc'] = header('conditions') + conditions.map(c => {
  check('condition', c, new Set());
  return '  ' + fields([
    `.holder = Holder::${c.holder}`,
    c.effectType && c.holder === 'Standalone' ? `.kind = ConditionKind::${ident(c.effectType)}` : '',
    c.duration ? `.duration = ${c.duration}` : '', c.counterMax ? `.counter_max = ${c.counterMax}` : '',
    flag('no_copy', c.noCopy), flag('affects_fainted', c.affectsFainted),
  ]) + `, // ${c.id}`;
}).join('\n') + '\n';

// types.inc: the chart as Showdown stores it, by defender then attacker
// (0 normal, 1 super effective, 2 resisted, 3 immune), and the immunities.
{
  const row = d => {
    const t = plain.type.find(p => p.name === d.name);
    return types.map(a => (t && t.damageTaken[a.name]) || 0).join(', ');
  };
  const immune = d => {
    const t = plain.type.find(p => p.name === d.name);
    const set = IMMUNITIES.filter(k => t && t.damageTaken[k] === 3);
    for (const k of IMMUNITIES) if (t && t.damageTaken[k] && t.damageTaken[k] !== 3) unknown.push(`type ${d.name}: ${k} is ${t.damageTaken[k]}`);
    return set.map(k => `immunity(Immunity::${ident(k)})`).join(' | ') || '0';
  };
  files['types.inc'] = header('the type chart') +
    'inline constexpr uint8_t TYPE_CHART[TYPES][TYPES] = {\n  {},\n' +
    types.map(d => `  {0, ${row(d)}}, // ${d.name}`).join('\n') + '\n};\n\n' +
    'inline constexpr uint16_t TYPE_IMMUNITIES[TYPES] = {\n  0,\n' +
    types.map(d => `  ${immune(d)}, // ${d.name}`).join('\n') + '\n};\n';
}

// natures.inc
{
  const stat = s => (s ? `Stat::${ident(s)}` : 'Stat::Hp');
  files['natures.inc'] = header('natures') + natures.map(n => `  {${stat(n.plus)}, ${stat(n.minus)}}, // ${n.name}`).join('\n') + '\n';
}

// handlers.inc: which names each effect answers to when handlers are gathered
// for an event, and the ranks Showdown sorts them by. A handler inside a
// move's `secondary` or `self` is run from the move's own code and is not
// gathered, so it is not here.
{
  const KINDS = [
    ['Move', 'move', moves, c => !c.path.includes('.')],
    ['Ability', 'ability', abilities, c => !c.path.includes('.')],
    ['Item', 'item', items, c => !c.path.includes('.')],
    ['Rule', 'rule', rules, c => !c.path.includes('.')],
  ];
  const rows = [];
  const add = (kindName, list, id, name, source) => {
    rows.push({kind: kindName, ident: list[index(list).get(id)].ident, name, handled: true,
      order: source[name + 'Order'] || 0, priority: source[name + 'Priority'] || 0, subOrder: source[name + 'SubOrder'] || 0});
  };
  for (const [kindName, kind, list, own] of KINDS) {
    for (const c of callbacks.filter(c => c.kind === kind && own(c))) add(kindName, list, c.id, c.name, list.find(e => e.id === c.id));
  }
  for (const c of callbacks) {
    const nested = c.path.startsWith('condition.') && c.path.split('.').length === 2;
    if (c.kind === 'condition' && !c.path.includes('.')) add('Condition', conditions, c.id, c.name, conditions.find(e => e.id === c.id));
    else if ((c.kind === 'move' || c.kind === 'ability') && nested) add('Condition', conditions, c.id, c.name, conditions.find(e => e.id === c.id));
    else if (c.path.includes('.') && !/^(secondary|self)\./.test(c.path) && !nested) unknown.push(`handler ${c.kind} ${c.id} ${c.path}`);
  }
  // Ranks under a name with no handler.
  for (const [kindName, list] of [['Move', moves], ['Ability', abilities], ['Item', items], ['Rule', rules], ['Condition', conditions]]) {
    for (const e of list) {
      for (const name of rankNames(e)) {
        if (rows.some(r => r.kind === kindName && r.ident === e.ident && r.name === name)) continue;
        if (!e[name + 'Order'] && !e[name + 'Priority'] && !e[name + 'SubOrder']) continue; // a rank of nothing
        rows.push({kind: kindName, ident: e.ident, name, handled: false,
          order: e[name + 'Order'] || 0, priority: e[name + 'Priority'] || 0, subOrder: e[name + 'SubOrder'] || 0});
      }
    }
  }
  rows.sort((a, b) => a.kind.localeCompare(b.kind) || a.ident.localeCompare(b.ident) || a.name.localeCompare(b.name));
  files['handlers.inc'] = header('handlers') + rows.map(r =>
    `  {EffectKind::${r.kind}, uint16_t(${r.kind}::${r.ident}), Callback::${r.name}, ${r.order}, ${r.priority}, ${r.subOrder}, ${r.handled}},`).join('\n') + '\n';
}

if (unknown.length) {
  console.error(`${unknown.length} fields the generator has no place for:\n  ` + [...new Set(unknown)].slice(0, 60).join('\n  '));
  process.exit(1);
}
fs.mkdirSync(opts.out, {recursive: true});
for (const [name, body] of Object.entries(files)) fs.writeFileSync(path.join(opts.out, name), body);
console.log(`${opts.out}: ` + Object.entries(files).map(([n, b]) => `${n} ${b.split('\n').length - 1}`).join(', '));
console.log(`${species.length} species, ${moves.length} moves, ${abilities.length} abilities, ${items.length} items, ` +
  `${conditions.length} conditions, ${rules.length} rules, ${types.length} types, ${CALLBACKS.length} callback names, ${FLAGS.length} move flags`);
