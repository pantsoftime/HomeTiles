// Regression test: fork features that must survive upstream's compact tiles.
//
// Upstream added half-height compact sensor tiles (9f998b5). Merging them into
// the fork produced two hazards that a clean textual merge would not reveal:
//
// 1. Upstream folded each renderer's font switch into
//    tile_layout::value_font_for_choice(), which knows choices 1-4 only. The
//    fork's monospace choices 5-8 lived in the sensor renderer's own switch, so
//    taking upstream's side of that conflict silently reset every mono tile to
//    the default font. The mono cases belong in the shared helper.
//
// 2. The fork moved value placement into sensor_apply_value_layout(), which
//    update_sensor_tile_value() re-runs on EVERY value update. Upstream's
//    compact layout positions the value once, at render time. Without a guard,
//    the first live update re-centres a compact tile's value -- invisible until
//    a value changes. The helper must leave compact tiles alone, and a compact
//    tile gets no caption label (it has no room for a second line).

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const read = (rel) => fs.readFileSync(path.join(repoRoot, rel), 'utf8');
const between = (text, start, end) => {
  const a = text.indexOf(start);
  assert.ok(a >= 0, `missing anchor: ${start}`);
  const b = text.indexOf(end, a + start.length);
  assert.ok(b > a, `missing end anchor after ${start}`);
  return text.slice(a, b);
};

// --- 1. Mono fonts live in the shared helper ----------------------------------
const fonts = read('src/tiles/runtime/tile_renderer_fonts.h');
const helper = between(fonts, 'inline const lv_font_t* value_font_for_choice(', '\n}\n');
const expected = {
  5: 'mono_font_20()', 6: 'mono_font_24()',
  7: 'mono_bold_font_20()', 8: 'mono_bold_font_24()',
};
for (const [choice, face] of Object.entries(expected)) {
  assert.ok(helper.includes(`case ${choice}: return ${face};`),
    `value_font_for_choice() must map choice ${choice} to ${face}, or mono tiles fall back to the default font`);
}

const sensor = read('src/types/sensor/renderer.cpp');
assert.match(sensor, /return tile_layout::value_font_for_choice\(tile\.sensor_value_font, FONT_VALUE\);/,
  'the sensor renderer must use the shared helper so it gets every font choice');

// --- 2. Compact tiles keep their render-time layout ---------------------------
const layout = between(sensor, 'void sensor_apply_value_layout(', '\n}\n');
const guard = layout.indexOf('if (tile_geometry::compact(tile.type, tile.span_w, tile.span_h)) return;');
assert.ok(guard >= 0, 'sensor_apply_value_layout() must return early for compact tiles');
for (const call of ['lv_obj_align(', 'lv_obj_set_style_text_align(']) {
  const first = layout.indexOf(call);
  assert.ok(first < 0 || guard < first,
    `the compact guard must run before the first ${call} -- the helper re-runs on every update`);
}

const render = between(sensor, 'const bool compact =', 'compact_sensor_layout::apply(');
assert.match(render, /!gauge_enabled && !graph_enabled && !compact\) \{/,
  'a compact tile must not get a caption label: it has no room for a second line');

// The update path is the reason the guard matters: prove it still re-applies.
const runtime = read('src/tiles/runtime/tile_renderer.cpp');
const update = between(runtime, 'void update_sensor_tile_value(', '\n}\n');
assert.match(update, /sensor_apply_value_layout\(/,
  'if the update path stops re-applying the layout, revisit whether the compact guard is still needed');

// --- 3. Fork code that read locals upstream's geometry rework removed ---------
// Both merged without a textual conflict and broke every profile's compile.
// Climate: span_w/span_h locals are gone; the tile's own float spans remain.
const climate = read('src/types/climate/renderer.cpp');
const reserve = between(climate, 'const lv_coord_t caption_reserve =', ';');
assert.match(reserve, /tile\.span_w == 1 && tile\.span_h == 1/,
  'the climate caption reserve must read the tile spans; the span_w/span_h locals no longer exist');

// Weather: value_row_y moved into the forecast-only branch, and the
// no-forecast value row is now centred in the real card. The humidity lift must
// be centre-relative in both the renderer and the update path, or the value row
// jumps back to a top-anchored position on the first update.
const weather = read('src/types/weather/renderer.cpp');
assert.match(weather, /widgets\.value_row_base_y = tile_layout::scale\(28\);/,
  'the renderer must store the centre offset of the no-forecast value row');
const weatherUpdate = between(runtime, 'if (widgets.humidity_label) {', 'if (widgets.icon_label) {');
assert.match(weatherUpdate, /lv_obj_align\(value_row, LV_ALIGN_CENTER, 0,\s*widgets\.value_row_base_y/,
  'the humidity lift must re-align from the centre, like the renderer');

// --- 4. Fork: a compact tile without an icon uses the icon column -----------
// Upstream reserved the icon column whether or not an icon existed; on the Tab5
// that left the title and value 81 of 168 px. With the icon set to "none" the
// text now starts where the compact Clock pads (8 px). Device and Web preview
// must agree (AGENTS.md section 4), so both halves are pinned together.
const compactLayout = read('src/tiles/runtime/compact_sensor_layout.h');
assert.match(compactLayout, /const int text_x = icon \? height \+ margin : margin \* 2;/,
  'without an icon the compact text must start at the 8 px Clock padding, not after an empty disc');
const css = read('src/web/assets/admin.css');
assert.match(css,
  /\.tile\.sensor-compact:not\(:has\(> \.tile-icon\)\) > \.tile-title,\s*\.tile\.sensor-compact:not\(:has\(> \.tile-icon\)\) > \.tile-value \{[^}]*left:calc\(2 \* var\(--compact-inset\)\);/,
  'the Web preview must shift icon-less compact text exactly like the device');

console.log('Compact-tile fork integration regressions passed.');
