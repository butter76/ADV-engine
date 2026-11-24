// Records games and checks them against the engine without storing them:
// tools/trace.cjs writes each chunk of games to a pipe that tests/replay.cpp
// reads, several chunks at a time. Every game must run to its end with no
// line different.
//
// Usage: SHOWDOWN=... node tools/soak.cjs --teams DIR|random|randbats --games N
//            [--first K] [--jobs J] [--chunk C] [--turns T] [--replay PATH] [--out FILE]
//
// --replay is the built checker (default build/release/replay). What differs,
// stops or is skipped is written to --out (default build/soak/<teams>.txt)
// with the number of its game, which `tools/trace.cjs --first K --games 1`
// records again. The exit status is 1 if there was any.
'use strict';
const fs = require('fs');
const os = require('os');
const path = require('path');
const {spawn} = require('child_process');

const root = path.join(__dirname, '..');
const opts = {games: 1000, first: 1, jobs: Math.max(1, os.cpus().length - 2), chunk: 500, turns: 0,
  replay: path.join(root, 'build/release/replay'), out: ''};
for (let i = 2; i < process.argv.length; i += 2) opts[process.argv[i].slice(2)] = process.argv[i + 1];
for (const k of ['games', 'first', 'jobs', 'chunk', 'turns']) opts[k] = +opts[k];
if (!opts.teams) throw new Error('--teams DIR|random|randbats');
if (!fs.existsSync(opts.replay)) throw new Error(`no checker at ${opts.replay}: build it first`);
const report = opts.out || path.join(root, 'build/soak', path.basename(opts.teams) + '.txt');
fs.mkdirSync(path.dirname(report), {recursive: true});
fs.writeFileSync(report, '');

const total = {games: 0, matched: 0, stopped: 0, skipped: 0, failures: 0, lines: 0, broken: 0};
const started = Date.now();
let next = opts.first, running = 0, lastNote = 0;
const end = opts.first + opts.games;

// One chunk: games [from, from + count).
function run(from, count) {
  return new Promise(resolve => {
    const quote = s => `'${String(s).replace(/'/g, `'\\''`)}'`;
    const record = ['node', quote(path.join(__dirname, 'trace.cjs')), '--teams', quote(opts.teams), '--first', from,
      '--games', count, '--turns', opts.turns, '--out', '-'].join(' ');
    const child = spawn('/bin/sh', ['-c', `${record} 2>/dev/null | ${quote(opts.replay)} - --complete`],
      {stdio: ['ignore', 'pipe', 'pipe']});
    let out = '', err = '';
    child.stdout.on('data', d => { out += d; });
    child.stderr.on('data', d => { if (err.length < 1 << 20) err += d; });
    child.on('close', () => {
      const m = out.match(/^(\d+) games: (\d+) matched to the end, (\d+) stopped at something not ported, (\d+) skipped, (\d+) failures\nlines compared and equal: (\d+)/);
      if (!m || +m[1] !== count) {
        total.broken++;
        fs.appendFileSync(report, `chunk ${from}..${from + count - 1}: the run broke\n${out}${err}\n`);
      } else {
        total.games += +m[1], total.matched += +m[2], total.stopped += +m[3], total.skipped += +m[4], total.failures += +m[5];
        total.lines += +m[6];
        const notes = out.split('\n').filter(l => /^  (stopped|skipped) /.test(l)).join('\n');
        if (err || notes) fs.appendFileSync(report, `chunk ${from}..${from + count - 1}:\n${notes ? notes + '\n' : ''}${err}\n`);
      }
      resolve();
    });
  });
}

function note(done) {
  const seconds = (Date.now() - started) / 1000;
  console.log(`${opts.teams}: ${total.games} of ${opts.games} games, ${total.matched} matched to the end, ${total.failures} differ, ` +
    `${total.stopped} stopped, ${total.skipped} skipped, ${total.broken} chunks broke; ${total.lines} lines equal; ` +
    `${seconds.toFixed(0)} s${done ? '' : `, ${(total.games / seconds).toFixed(0)} games/s`}`);
}

(async () => {
  await new Promise(resolve => {
    const fill = () => {
      while (running < opts.jobs && next < end) {
        const from = next, count = Math.min(opts.chunk, end - next);
        next += count;
        running++;
        run(from, count).then(() => {
          running--;
          if (Date.now() - lastNote > 60000) lastNote = Date.now(), note(false);
          fill();
        });
      }
      if (!running && next >= end) resolve();
    };
    fill();
  });
  note(true);
  const bad = total.failures + total.stopped + total.skipped + total.broken;
  if (bad) console.log(`see ${report}`);
  process.exit(bad ? 1 : 0);
})();
