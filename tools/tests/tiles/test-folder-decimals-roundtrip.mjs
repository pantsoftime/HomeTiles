// Regression test: a folder tile's decimal count must survive pack/unpack, and
// must not collide with anything else stored in the packed tile.
//
// It used to live in PackedTileV7.reserved[1]. Upstream v0.6.10 claimed
// reserved[1] and [2] for view_id and wrote them AFTER the folder-decimals
// write, so the count was destroyed on save and then read back as view_id's low
// byte -- folder tiles rendered "6" as their value's decimal count on a grid
// whose view ids happened to be 7. It now lives in sensor_decimals, biased
// clear of the legacy Settings/Back sentinels.

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const src = fs.readFileSync(path.join(repoRoot, 'src/tiles/config/tile_config.cpp'), 'utf8');

const num = (name) => {
  const m = src.match(new RegExp(`constexpr uint8_t ${name}\\s*=\\s*(\\d+)`));
  assert.ok(m, `${name} must exist`);
  return Number(m[1]);
};
const SETTINGS = num('LEGACY_NAV_KIND_SETTINGS');
const BACK = num('LEGACY_NAV_KIND_BACK');
const BIAS = num('kFolderDecimalsBias');

assert.ok(BIAS > SETTINGS && BIAS > BACK,
  'the bias must lift every encoded count above both legacy nav sentinels');
assert.ok(6 + BIAS < 0xff, 'the largest biased count must stay inside the byte');

// Model the encode/decode pair exactly as the firmware implements it.
const clampDecimals = (v) => (v === 0xff ? 0xff : v > 6 ? 6 : v);
const encode = (d) => { const c = clampDecimals(d); return c === 0xff ? 0 : c + BIAS; };
const decode = (s) => { if (s < BIAS) return 0xff; const v = s - BIAS; return v > 6 ? 0xff : v; };

for (let d = 0; d <= 6; ++d) {
  const stored = encode(d);
  assert.equal(decode(stored), d, `decimals ${d} must round-trip`);
  assert.notEqual(stored, SETTINGS, `decimals ${d} must not encode as the Settings sentinel`);
  assert.notEqual(stored, BACK, `decimals ${d} must not encode as the Back sentinel`);
}
assert.equal(decode(encode(0xff)), 0xff, 'unset must round-trip as unset');

// Values a tile can carry from older firmware, or from a tile that never had a
// count, must all read as "unset" rather than as a bogus count.
for (const legacy of [0, SETTINGS, BACK, 0xff]) {
  assert.equal(decode(legacy), 0xff, `stored ${legacy} must decode as unset`);
}

// The count must no longer touch reserved[]; view_id owns those bytes now.
assert.doesNotMatch(src, /out\.reserved\[1\]\s*=\s*\(folder_decimals/,
  'folder decimals must not be written into reserved[1] -- view_id owns it');
assert.match(src, /out\.reserved\[1\] = static_cast<uint8_t>\(in\.view_id\);/,
  'view_id must still own reserved[1]');
assert.match(src, /out\.sensor_decimals = unbiasFolderDecimals\(in\.sensor_decimals\);/,
  'unpack must decode folder decimals from sensor_decimals');

console.log('ok  folder decimals round-trip clear of the nav sentinels and view_id');
