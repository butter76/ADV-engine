// The reference side of tests/prng_test.cpp: a fixed script of PRNG calls on
// Showdown's own PRNG, reduced to one hash. Both sides must print the same
// line.
//
// Usage: SHOWDOWN=... node tools/prng_ref.cjs
'use strict';
const {showdown, gen5} = require('./lib.cjs');
const {PRNG} = showdown();

let hash = 0xcbf29ce484222325n;
const mix = v => { hash = BigInt.asUintN(64, (hash ^ BigInt(v)) * 0x100000001b3n); };
let calls = 0;

for (let s = 1; s <= 500; s++) {
  // Seeds spread over all 64 bits.
  const prng = new PRNG(gen5(BigInt.asUintN(64, BigInt(s) * 0x9E3779B97F4A7C15n)));
  for (let i = 0; i < 4000; i++) {
    const k = (s * 31 + i * 7) % 97;
    switch (i % 6) {
    case 0: mix(prng.random(2 ** 16)); break;
    case 1: mix(prng.random(k + 1)); break;
    case 2: mix(prng.random(k, k + 1 + (i % 16))); break;
    case 3: mix(prng.randomChance(k, 100) ? 1 : 0); break;
    case 4: mix(prng.sample([3, 1, 4, 1, 5, 9, 2, 6].slice(0, 1 + (k % 8)))); break;
    case 5: {
      // As speedSort: a run of ties in the middle of a list.
      const list = [0, 1, 2, 3, 4, 5, 6, 7];
      const start = k % 4, end = start + 2 + (i % 4);
      prng.shuffle(list, start, end);
      for (const v of list) mix(v);
      break;
    }
    }
    calls++;
  }
  mix(BigInt('0x' + prng.getSeed().split(',').map(n => (+n).toString(16).padStart(4, '0')).join('')) & 0xffffffffn);
}
console.log(`${calls} calls, hash ${hash.toString(16).padStart(16, '0')}`);
