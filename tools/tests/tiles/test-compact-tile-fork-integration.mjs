// Regression test: fork features that must survive upstream's compact tiles.
//
// Upstream added half-height compact sensor tiles (9f998b5). Merging them into
// the fork produced two hazards that a clean textual merge would not reveal:
//
// 1. Upstream folded each renderer's font switch into
//    tile_layout::value_font_for_choice(), which knows upstream's choices only.
//    The fork's monospace choices lived in the sensor renderer's own switch, so
//    taking upstream's side of that conflict silently reset every mono tile to
//    the default font. The mono cases belong in the shared helper. v0.7.0 then
//    gave choice 5 to its 28 px size, which the fork had used for "20 Mono", so
//    the mono choices moved to 200-203 (sensor_value_font_fork.h).
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
  SENSOR_VALUE_FONT_MONO_20: 'mono_font_20()', SENSOR_VALUE_FONT_MONO_24: 'mono_font_24()',
  SENSOR_VALUE_FONT_MONO_BOLD_20: 'mono_bold_font_20()', SENSOR_VALUE_FONT_MONO_BOLD_24: 'mono_bold_font_24()',
};
for (const [choice, face] of Object.entries(expected)) {
  assert.ok(helper.includes(`case ${choice}: return ${face};`),
    `value_font_for_choice() must map ${choice} to ${face}, or mono tiles fall back to the default font`);
}
assert.match(helper, /case 5: return content_font_28\(\);/,
  'choice 5 is upstream\'s 28 px size since v0.7.0; the fork must not reclaim it');

// The mono numbers sit clear of upstream's, and old fork numbers read forward.
const monoHeader = read('src/tiles/config/sensor_value_font_fork.h');
for (const [name, value] of [['MONO_20', 200], ['MONO_24', 201], ['MONO_BOLD_20', 202], ['MONO_BOLD_24', 203]]) {
  assert.match(monoHeader, new RegExp(`SENSOR_VALUE_FONT_${name} = ${value};`),
    `stored tiles carry ${value}; renumbering it would change their font`);
}
const legacy = between(monoHeader, 'inline uint8_t sensor_value_font_from_legacy_fork(', '\n}\n');
for (const [old, now] of [[6, 'MONO_24'], [7, 'MONO_BOLD_20'], [8, 'MONO_BOLD_24']]) {
  assert.match(legacy, new RegExp(`case ${old}: return SENSOR_VALUE_FONT_${now};`),
    `fork releases before v0.7.01 stored ${now} as ${old}; it must still load as monospace`);
}
assert.doesNotMatch(legacy, /case 5:/, '5 is upstream\'s 28 px choice now, never a legacy mono number');
const tileConfigH = read('src/tiles/config/tile_config.h');
assert.match(tileConfigH, /static_assert\(SENSOR_VALUE_FONT_MAX < 6,/,
  'the legacy mapping is only safe while upstream stops at 5; the merge must fail loudly when it does not');
const clamp = between(read('src/tiles/config/tile_config.cpp'), 'static uint8_t clampSensorValueFont(', '\n}\n');
assert.ok(clamp.indexOf('sensor_value_font_from_legacy_fork(val)') >= 0 &&
  clamp.indexOf('sensor_value_font_from_legacy_fork(val)') < clamp.indexOf('SENSOR_VALUE_FONT_MAX'),
  'every unpack path must map old numbers forward before the range check discards them');
assert.match(clamp, /!sensor_value_font_is_mono\(val\)/, 'the unpack clamp must keep the mono choices');
const sensorHandler = read('src/types/sensor/web_handler.cpp');
assert.match(sensorHandler, /sensor_value_font_is_mono\(raw\)/, 'the sensor save handler must accept the mono choices');
for (const file of ['src/types/sensor/web_html.cpp', 'src/types/navigate/web_html.cpp']) {
  const html = read(file);
  for (const value of [200, 201, 202, 203]) {
    assert.ok(html.includes(`<option value="${value}">`), `${file} must offer mono choice ${value}`);
  }
  assert.doesNotMatch(html, /<option value="[5-8]">\d+ Mono/, `${file} still offers a mono font under an upstream number`);
}
// Upstream's editor hides every value-size option it does not list.
const adminJs = read('src/web/admin/tiles/layout.js');  // bundled into admin.js
const sync = between(adminJs, 'function syncCompactValueFontOptions(', '\n  }\n');
assert.match(sync, /const forkMono = \['200', '201', '202', '203'\];/);
assert.match(sync, /\['0', '1', '2', '3', '4', \.\.\.forkMono\]/,
  'full-size tiles must keep the mono options visible in the editor');

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

// v0.7.0 made Folder and Settings tiles half-height capable. The fork's folder
// value and Settings battery caption need a third line, so a half-height tile
// shows neither -- and its widget slot is still cleared, because live updates
// for a folder's sensor_entity are routed to its index regardless.
const navigate = read('src/types/navigate/renderer.cpp');
const battery = between(navigate, 'bool navigate_settings_shows_battery(', '\n}\n');
assert.match(battery, /!tile_geometry::compact_icon_title\(tile\.type, tile\.span_w, tile\.span_h\)/,
  'the shared battery condition must exclude half-height tiles so the update route agrees with the renderer');
const navRender = between(navigate, 'lv_obj_t* render_navigate_tile(', '\n}\n');
assert.match(navRender, /bool has_value = !compact &&/);
const reset = navRender.indexOf('widgets[index] = SensorTileWidgets{};');
assert.ok(reset >= 0 && reset < navRender.indexOf('if (has_value) {'),
  'the widget slot must be cleared before, and independently of, the value label');

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

// Without an icon the text is centred in a symmetric 8 px box: left-aligned
// text across the full width read as misplaced once the icon column was gone.
assert.match(compactLayout,
  /lv_obj_set_style_text_align\(label, icon \? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER, 0\);/,
  'icon-less compact text must be centred; text beside an icon stays left-aligned');
const noIconRule = between(css, '.tile.sensor-compact:not(:has(> .tile-icon)) > .tile-value {', '}');
assert.match(noIconRule, /right:calc\(2 \* var\(--compact-inset\)\);/,
  'the preview box must be symmetric (8 px each side) for centring to match the device');
assert.match(noIconRule, /text-align:center;/,
  'the Web preview must centre icon-less compact text like the device');

// --- 5. Folder/Settings value tiles keep their icon disc inside the card --------
// v0.7.0 draws a header_diameter() disc centred on every centred icon. The fork
// lifts the icon of a tile with a value line, and the old fixed -48 left that
// disc flush with the card's top edge (Tab5, 4B). The offset now comes from the
// disc itself, so its top gap is inset() -- the corner discs' gap -- everywhere.
const iconY = between(navigate, 'static lv_coord_t navigate_value_icon_y(', '\n}\n');
assert.match(iconY, /tile_icon_disc::header_diameter\(icon_size\.x\)/,
  'use the disc size add_round() will use for a centred icon');
assert.match(iconY, /\(2 \* tile_icon_disc::inset\(\) \+ disc - GRID_CELL_H\) \/ 2/,
  'the disc top must sit inset() below the card top');
assert.match(navRender, /if \(has_value\) \{\s*lv_obj_align\(icon_lbl, LV_ALIGN_CENTER, 0, navigate_value_icon_y\(iconChar\)\);/,
  'the value layout must place its icon from the disc geometry');
assert.doesNotMatch(navRender, /scale_i16\(-48\)/, 'the fixed lift that clipped the disc is gone');

// --- 6. Cover header: "Closed", not "Closed · 0 %", and one line ------------
// v0.8.0's one-cell Cover header leaves ~81 px beside the disc; the position at
// an end stop overflowed it and the label wrapped into the position bar.
const cover = read('src/types/cover/renderer.cpp');
const coverLine = between(cover, 'String cover_state_line(', '\n}\n');
assert.match(coverLine, /has_position && \(dragging \|\| \(position > 0 && position < 100\)\)/,
  'an end stop shows the state alone; the number shows in between and while dragging');
assert.match(cover, /set_state_line\(widget, cover_state_line\(state, true, value, true\)\);/,
  'the drag path keeps the number the finger is choosing');
assert.match(cover, /if \(!tall && !compact && widget\.state_label && text\.state_font\) \{\s*lv_obj_set_height\(widget\.state_label, lv_font_get_line_height\(text\.state_font\)\);/,
  'the one-cell state line is one line high, so LONG_DOT ends it instead of wrapping into the bar');
const coverPreview = between(read('src/types/cover/admin.js'), 'function coverPreviewStateLine(', '\n  }\n');
assert.match(coverPreview, /state\.position > 0 && state\.position < 100/,
  'the Web preview must drop the end-stop position like the device');

console.log('Compact-tile fork integration regressions passed.');
