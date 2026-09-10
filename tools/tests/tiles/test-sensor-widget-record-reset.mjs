// Regression test: every renderer that registers into the shared
// SensorTileWidgets table must reset the whole record, not name fields.
//
// The table is a static array per grid type and nothing clears it when a grid is
// rebuilt, so any field a previously rendered tile left at a slot index is a
// dangling lv_obj_t. update_sensor_tile_value() is type-independent and writes
// through subtitle_label whenever it is non-null, so a renderer that sets only
// some fields inherits a freed label from the tile that used to live there.
//
// That crashed both 86-panels (load fault, MCAUSE=5, faulting address 0x30) on
// opening a folder containing folder tiles: the folder tile's value_label was
// registered while subtitle_label survived from a caption-bearing sensor tile at
// the same index. The Tab5 was unaffected only because its wider root grid left
// no folder tiles inside a folder.
//
// subtitle_label is a fork addition, so upstream's renderers cannot be expected
// to name it -- which is exactly why the rule here is "reset, don't enumerate".

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const read = (rel) => fs.readFileSync(path.join(repoRoot, rel), 'utf8');

// Every field currently in the record. If this list and the struct disagree the
// struct gained a field, which is the moment to re-check the writers below.
const widgets = read('src/types/sensor/widgets.h');
const fields = [...widgets.matchAll(/^\s+(?:lv_obj_t\*|int32_t|lv_chart_series_t\*)\s+(\w+)\s*=/gm)]
  .map((m) => m[1]);
assert.ok(fields.includes('subtitle_label'),
  'subtitle_label is the fork field that makes partial registration unsafe');
assert.ok(fields.length >= 7, `expected the full record, saw ${fields.join(', ')}`);

// Files that write into the table through the shared accessor.
const writers = [
  'src/types/navigate/renderer.cpp',
  'src/types/sensor/renderer.cpp',
  'src/types/energy/renderer.cpp',
];

for (const rel of writers) {
  const src = read(rel);
  if (!src.includes('tile_renderer_get_sensor_widgets')) continue;

  const resets = src.includes('= SensorTileWidgets{}');
  const namesSubtitle = /target\[index\]\.subtitle_label\s*=/.test(src);
  assert.ok(resets || namesSubtitle,
    `${rel} registers into SensorTileWidgets but neither resets the record ` +
    `(target[index] = SensorTileWidgets{}) nor assigns subtitle_label. One of ` +
    `the two is required or it inherits a dangling label pointer.`);
}

// update_sensor_tile_value() must keep guarding the pointer; the guard is not
// sufficient on its own (a stale pointer is non-null) but removing it would
// make even a correct reset fragile.
const renderer = read('src/tiles/runtime/tile_renderer.cpp');
assert.match(renderer, /if \(subtitle_label\) \{\s*\n\s*lv_label_set_text\(subtitle_label,/,
  'the subtitle write must stay guarded by a non-null check');

console.log(`ok  ${writers.length} SensorTileWidgets writers reset the full record`);
