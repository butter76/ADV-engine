// What Showdown makes of a file of exported teams: each team packed, a line a
// team, in the file's order. tests/export_test.cpp reads the same file with
// src/showdown/export.hpp and must get the same teams.
//
// Usage: SHOWDOWN=... node tools/teams_ref.cjs FILE > PACKED
'use strict';
const fs = require('fs');
const {showdown} = require('./lib.cjs');

const {Teams} = showdown();
const text = fs.readFileSync(process.argv[2], 'utf8');
const blocks = /^===.*===\s*$/m.test(text) ? text.split(/^===.*===[ \t\r]*$/m).slice(1) : [text];
for (const block of blocks) {
  const team = Teams.import(block.trim());
  console.log(team && team.length ? Teams.pack(team) : '');
}
