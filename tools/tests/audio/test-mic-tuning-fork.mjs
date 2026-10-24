// FORK: host regression for the microphone tuning (src/audio/mic_tuning_fork.h):
// the Q8 boost table is round(256 * 10^(dB/20)), each slot choice takes the
// right samples, the boost clips at full scale, and the speaker worker uses it
// for every capture with per-slot levels behind GET/POST /api/audio/mic.
// User 2026-10-24: on the Tab5 normal speech never reached Home Assistant's
// voice detection, and the Bridge asks Home Assistant for no gain of its own.
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const read = rel => fs.readFileSync(path.join(root, rel), 'utf8').replace(/\r\n/g, '\n');

// --- Wiring --------------------------------------------------------------
const speaker = read('src/audio/speaker.cpp');
const readMono = speaker.slice(speaker.indexOf('int readMono() {'));
assert.match(readMono.slice(0, 1600),
  /mic_tuning::toMono\(g_chunk, frames, channel, mic_tuning::boostQ8\(g_mic_boost\.load\(\)\), g_mono\);\s*meterForkLevels\(g_chunk, g_mono, frames\);/,
  'every capture (wake word and Assist) goes through the slot choice and the boost');
assert.match(readMono.slice(0, 1600), /audio_chips::Input::Es8311\s*\?\s*mic_tuning::Channel::Left/,
  'an ES8311 keeps the left slot, as upstream');
assert.match(speaker, /prefs\.putUChar\(kPrefsMicChannelKey, g_mic_channel\.load\(\)\);/);
assert.match(speaker, /prefs\.putUChar\(kPrefsMicBoostKey, g_mic_boost\.load\(\)\);/);
const web = read('src/web/server/web_admin.cpp');
assert.match(web, /server\.on\("\/api\/audio\/mic", HTTP_GET, guarded\(/);
assert.match(web, /server\.on\("\/api\/audio\/mic", HTTP_POST, guarded\(/);

// --- Pure DSP ------------------------------------------------------------
function findCompiler(candidates) {
  for (const candidate of candidates.filter(Boolean)) {
    const check = spawnSync(candidate, ['--version'], {encoding: 'utf8'});
    if (!check.error && check.status === 0) return candidate;
  }
  return null;
}
const cxx = findCompiler([process.env.CXX, 'clang++', 'g++', 'c++']);
if (!cxx) {
  console.log('SKIP: mic tuning DSP part needs a host C++ compiler (wiring checked)');
  process.exit(0);
}
const table = Array.from({length: 25}, (_, db) => Math.round(256 * 10 ** (db / 20)));
const program = `#include <cassert>
#include <cstdio>
#include "src/audio/mic_tuning_fork.h"
using namespace mic_tuning;
int main() {
  const unsigned expected[] = {${table.join(', ')}};
  for (unsigned db = 0; db <= kMaxBoostDb; ++db) assert(boostQ8(static_cast<uint8_t>(db)) == expected[db]);
  assert(boostQ8(200) == expected[kMaxBoostDb]);
  const int16_t stereo[] = {1000, 0, -2000, 600, 30000, 30000};
  int16_t out[3];
  toMono(stereo, 3, Channel::Mix, 256, out);
  assert(out[0] == 500 && out[1] == -700 && out[2] == 30000);
  toMono(stereo, 3, Channel::Left, 256, out);
  assert(out[0] == 1000 && out[1] == -2000 && out[2] == 30000);
  toMono(stereo, 3, Channel::Right, 256, out);
  assert(out[0] == 0 && out[1] == 600 && out[2] == 30000);
  toMono(stereo, 3, Channel::Left, boostQ8(12), out);  // x3.98
  assert(out[0] == 3980 && out[1] == -7960 && out[2] == 32767);
  const int16_t low[] = {-30000, -30000};
  toMono(low, 1, Channel::Mix, boostQ8(6), out);
  assert(out[0] == -32768);
  std::puts("ok");
  return 0;
}
`;
const out = path.join(root, 'build/tests/mic-tuning-fork');
fs.mkdirSync(out, {recursive: true});
const cpp = path.join(out, 'mic-tuning.cpp');
const exe = path.join(out, process.platform === 'win32' ? 'mic-tuning.exe' : 'mic-tuning');
fs.writeFileSync(cpp, program);
const build = spawnSync(cxx, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', root, cpp, '-o', exe],
  {encoding: 'utf8'});
assert.equal(build.status, 0, `mic tuning test did not compile:\n${build.stdout}${build.stderr}`);
const run = spawnSync(exe, [], {encoding: 'utf8'});
assert.equal(run.status, 0, `mic tuning test failed:\n${run.stdout}${run.stderr}`);
assert.equal(run.stdout.trim(), 'ok');
console.log('Mic tuning fork regressions passed.');
