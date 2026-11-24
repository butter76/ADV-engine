// Gen 3 as the pinned Showdown resolves it, for the tools that dump it,
// generate C++ from it or count its callbacks: static data with every mod's
// inheritance applied, and the handlers apart from it.
'use strict';
const crypto = require('crypto');

// A handler is any function, or a constant under an event name: Showdown
// accepts `onLockMove: 'bide'` or `onTry: false` where it accepts a function.
// `onXPriority`, `onXOrder` and `onXSubOrder` are data about a handler.
const isRank = key => /^on[A-Z].*(Priority|Order|SubOrder)$/.test(key);
const isHandler = (key, v) =>
  typeof v === 'function' || (/^on[A-Z]/.test(key) && !isRank(key) && v !== undefined);

const TABLES = {move: 'moves', ability: 'abilities', item: 'items', condition: 'conditions'};

function build(S, formatId) {
  const dex = S.Dex.mod('gen3');
  const legal = e => e.exists && e.gen <= 3 && !e.isNonstandard;

  // Which generation's files a function comes from: the newest mod, walking
  // up from gen3, that still has this exact function. 9 is the base data.
  const later = [4, 5, 6, 7, 8].map(g => S.Dex.mod('gen' + g)).concat([S.Dex]);
  const walk = (obj, keys) => keys.reduce((o, k) => (o == null ? o : o[k]), obj);
  const origin = (kind, id, keys, fn) => {
    if (!TABLES[kind] || typeof fn !== 'function') return 0;
    let gen = 3;
    for (const [i, d] of later.entries()) {
      const other = d[TABLES[kind]].get(id);
      if (!other || walk(other, keys) !== fn) break;
      gen = i + 4;
    }
    return gen;
  };

  const callbacks = [];
  const strip = (kind, id, obj, keys = []) => {
    if (Array.isArray(obj)) return obj.map((v, i) => strip(kind, id, v, keys.concat(i)));
    if (!obj || typeof obj !== 'object') return obj;
    const out = {};
    for (const key of Object.keys(obj)) {
      const v = obj[key];
      // A move's `secondaries` is `[secondary]` again.
      if (key === 'secondaries' && obj.secondary) {
        if (v.length !== 1 || v[0] !== obj.secondary) throw new Error(`${id}: secondaries is not [secondary]`);
        continue;
      }
      if (kind === 'species' && key === 'learnset') continue;
      // Fling is Gen 4's; a format's rule table is the rules again.
      if ((kind === 'item' && key === 'fling') || (kind === 'rule' && key === 'ruleTable')) continue;
      if (isHandler(key, v)) {
        const constant = typeof v !== 'function';
        const source = constant ? JSON.stringify(v) : v.toString();
        callbacks.push({
          kind, id, path: keys.concat(key).join('.'), name: key, constant,
          lines: source.split('\n').length,
          hash: crypto.createHash('sha1').update(source).digest('hex').slice(0, 12),
          gen: origin(kind, id, keys.concat(key), v),
          source,
        });
      } else if (v !== undefined) {
        out[key] = strip(kind, id, v, keys.concat(key));
      }
    }
    return out;
  };

  const format = dex.formats.get(formatId);
  const rules = [...dex.formats.getRuleTable(format).keys()].map(r => dex.formats.get(r)).filter(r => r.exists);
  const plain = {
    species: dex.species.all().filter(legal).map(e => strip('species', e.id, e)),
    move: dex.moves.all().filter(legal).map(e => strip('move', e.id, e)),
    ability: dex.abilities.all().filter(legal).map(e => strip('ability', e.id, e)),
    item: dex.items.all().filter(legal).map(e => strip('item', e.id, e)),
    condition: Object.keys(dex.data.Conditions).map(id => strip('condition', id, dex.conditions.get(id))),
    rule: [format, ...rules].map(e => strip('rule', e.id, e)),
    type: dex.types.all().filter(t => t.exists && t.gen <= 3 && !t.isNonstandard).map(t => strip('type', t.id, t)),
    nature: dex.natures.all().map(n => strip('nature', n.id, n)),
  };
  return {dex, plain, callbacks, format, rules};
}

module.exports = {build, isHandler, isRank, TABLES};
