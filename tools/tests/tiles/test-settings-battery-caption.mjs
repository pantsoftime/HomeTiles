// Regression test: the Settings-tile battery caption (fork feature).
//
// On a device that measures its own battery -- the Tab5 -- a Settings tile shows
// the battery percentage in the value slot folder tiles use for their live
// value. The caption is driven by device capability, not by configuration: the
// Settings save handler clears every sensor field and the pack/unpack policy
// forces Settings decimals to 0xFF, so the data-driven folder mechanism cannot
// be reused for this tile type without touching storage.
//
// Three things must hold:
//   1. One condition decides "this tile shows the battery", shared by the
//      renderer and the live update route, so they cannot disagree.
//   2. The caption never displays a false 0: unknown reads as "--". A
//      confident 0 is exactly how this entity misled before.
//   3. The 500 ms battery sync is change-driven. It used to rewrite the bridge's
//      whole multi-KB value map and push to every grid twice a second for a
//      value that moves a few times an hour.

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
  assert.ok(b > a, `missing end anchor after ${start}: ${end}`);
  return text.slice(a, b);
};

// --- 1. One shared condition --------------------------------------------------
const header = read('src/types/navigate/renderer.h');
assert.match(header, /bool navigate_settings_shows_battery\(const Tile& tile, GridType grid_type\);/,
  'the caption condition must be declared once for renderer and update route');

const renderer = read('src/types/navigate/renderer.cpp');
const helper = between(renderer, 'bool navigate_settings_shows_battery(', '\n}\n');
assert.match(helper, /tile\.type == TILE_SETTINGS/, 'only Settings tiles carry the caption');
assert.match(helper, /grid_type != GridType::SCREENSAVER/,
  'the screensaver grid never shows the caption, matching the folder value rule');
assert.match(helper, /batteryStateSupportsMeasurement\(\)/,
  'the caption must follow device capability so non-battery profiles never show it');

// Non-battery profiles compile the stub, whose capability is hard false.
const battery = read('src/core/power/battery_state.cpp');
assert.match(battery, /#if defined\(DEVICE_M5STACKS_TAB5\)/);
assert.match(battery, /batteryStateSupportsMeasurement\(\)\s*\{ return false;/,
  'every non-Tab5 profile must report no battery, which keeps the caption off them');

// --- Renderer: layout, font and seed -------------------------------------------
const render = between(renderer, 'lv_obj_t* render_navigate_tile(', '\n}\n');
assert.match(render, /const bool has_battery = navigate_settings_shows_battery\(tile, grid_type\);/);
assert.match(render, /bool has_value = \(tile\.sensor_entity\.length\(\) > 0 &&\s*grid_type != GridType::SCREENSAVER\) \|\|\s*has_battery;/,
  'the caption must take the folder value slot so it inherits that layout');
assert.match(render, /has_battery \? tile_layout::content_font_24\(\)/,
  'the caption is a secondary readout and uses the smaller 24 px font');
// Seed from local state, guarded, with "--" as the unknown form.
const seed = between(render, 'char seed[16] = "--";', 'lv_label_set_text(v, seed);');
assert.match(seed, /if \(has_battery && batteryStateHasDisplayPercent\(\)\)/,
  'the seed must only print a percentage when a reading exists -- never a false 0');
assert.match(seed, /"%ld %%"/, 'the seed must match update_sensor_tile_value\'s "87 %" form');

// --- Live update route -----------------------------------------------------------
const unified = read('src/ui/tabs/tiles/tab_tiles_unified.cpp');
const route = between(unified, 'void tiles_update_sensor_by_entity(', '\n}\n');
const branch = between(route, 'if (navigate_settings_shows_battery(tile, grid_type) &&', '\n    }\n');
assert.match(branch, /strcasecmp\(entity_id, kEntityInternalBatterySoc\) == 0/,
  'the caption follows the internal battery entity, not a configured sensor_entity');
assert.match(branch, /queue_sensor_tile_update\(grid_type, i, value, "%"\);/,
  'caption updates must go through the established LVGL queue');
assert.doesNotMatch(branch, /queue_sensor_popup_value/,
  'a Settings tile opens Settings; there is no sensor popup to feed');

// --- 2 & 3. The sync: no false zero, change-driven -------------------------------
const mqtt = read('src/network/mqtt/mqtt_handlers.cpp');
const pushOnce = between(mqtt, 'static void publish_battery_to_grids(', '\n}\n');
assert.match(pushOnce, /static String last_payload;/);
assert.match(pushOnce, /if \(last_payload == payload\) return;/,
  'grid pushes must be change-driven: every push logs and queues per tile');

const sync = between(mqtt, 'static void sync_internal_battery_entity()', 'static const char* kSleepOptionLabels');
assert.doesNotMatch(sync, /update_all_grids\(/,
  'the sync must reach the grids only through the change-driven helper');
assert.match(sync, /if \(!batteryStateHasDisplayPercent\(\)\) \{\s*(\/\/[^\n]*\n\s*)*publish_battery_to_grids\("unavailable"\);\s*return;/,
  'with no reading yet, readBatterySocPercent() returns 0; that must never be published');
assert.ok(
  sync.indexOf('if (!batteryStateHasDisplayPercent())') <
    sync.indexOf('publish_battery_to_grids(soc_payload);'),
  'the no-reading guard must run before a value is published');
assert.match(sync,
  /if \(haBridgeConfig\.findSensorInitialValue\(kEntityInternalBatterySoc\) != soc_payload\) \{\s*haBridgeConfig\.updateSensorValue/,
  'updateSensorValue() rebuilds the whole value map; call it only when the bridge copy differs');
assert.match(sync,
  /if \(haBridgeConfig\.findSensorName\(kEntityInternalBatterySoc\) != sensor_name \|\|[\s\S]*?\) \{\s*haBridgeConfig\.registerSensorMeta/,
  'metadata must be re-registered only when the bridge copy differs');
// Nothing may write to the bridge unconditionally.
for (const call of ['registerSensorMeta', 'updateEntityMeta', 'updateSensorValue']) {
  const lines = sync.split('\n').filter((l) => l.includes(`haBridgeConfig.${call}(`));
  assert.ok(lines.length === 1, `${call} must appear exactly once, inside its guard`);
  assert.match(lines[0], /^\s{4}haBridgeConfig\./,
    `${call} must sit inside a guard (indented), never run unconditionally`);
}

console.log('Settings battery caption regressions passed.');
