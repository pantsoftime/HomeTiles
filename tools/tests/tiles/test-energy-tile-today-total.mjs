// FORK: an Energy tile keeps today's total from the Bridge's day answer.
// Its entity is also a plain sensor; for a lifetime counter (an Envoy's solar
// source) the live state and cache restores used to replace today's total
// with the lifetime total. Both paths must prefer the day answer.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const read = (relativePath) =>
  fs.readFileSync(path.join(repoRoot, relativePath), 'utf8');

const dataHeader = read('src/types/energy/energy_data.h');
const data = read('src/types/energy/energy_data.cpp');
const tiles = read('src/ui/tabs/tiles/tab_tiles_unified.cpp');

assert.match(dataHeader, /bool energy_day_total_text\(const String& id, String& out\);/,
             'energy_data.h must declare energy_day_total_text');

const helperStart = data.indexOf('bool energy_day_total_text(');
assert.ok(helperStart >= 0, 'energy_day_total_text must be defined');
const helperBody = data.slice(helperStart, data.indexOf('\n}\n', helperStart));
assert.ok(helperBody.includes('energy_find_entry(id, "day", entry)'),
          'The total must come from the day answer');
assert.ok(helperBody.includes('isfinite(entry.total)'),
          'A non-finite total must fall back to the entity state');
assert.ok(helperBody.includes('format_energy_total(entry.total, entry.is_cost)'),
          'The total must be formatted like queue_energy_tile_update_for_entry');

const pickStart = tiles.indexOf('static bool energy_tile_today_value(');
assert.ok(pickStart >= 0, 'energy_tile_today_value must exist');
const pickBody = tiles.slice(pickStart, tiles.indexOf('\n}\n', pickStart));
assert.ok(pickBody.includes('tile.type == TILE_ENERGY') &&
          pickBody.includes('energy_day_total_text(tile.sensor_entity, out)'),
          'Only Energy tiles take the day total');

// Cache restore (folder open, page cache): the day total wins over the cache.
const enqueueStart = tiles.indexOf('static inline void enqueue_cached_tile_state(');
assert.ok(enqueueStart > pickStart, 'enqueue_cached_tile_state must follow the helper');
const enqueueBody = tiles.slice(enqueueStart, tiles.indexOf('\n}\n', enqueueStart));
assert.ok(enqueueBody.includes(
            'if (!energy_tile_today_value(tile, payload) && !get_cached_or_initial_payload(tile, payload)) return;'),
          'Cache restore must try the day total before the cached entity state');

// Live state: the Energy tile gets the day total, everything after the value
// update (icon sources, switches, media) still runs for the tile.
const liveIndex = tiles.indexOf('const bool energy_kept = energy_tile_today_value(tile, energy_today);');
assert.ok(liveIndex > enqueueStart, 'The live state path must check for the day total');
const liveLine = tiles.indexOf(
  'queue_sensor_tile_update(grid_type, i, energy_kept ? energy_today.c_str() : value, unit_cstr);', liveIndex);
assert.ok(liveLine > liveIndex, 'The live state path must queue the day total for Energy tiles');
const liveTail = tiles.slice(liveIndex, tiles.indexOf('tile_icon_source_matches(tile, entity_id)', liveIndex));
assert.ok(!/\bcontinue;/.test(liveTail),
          'The live state path must not skip the rest of the loop for Energy tiles');

console.log('PASS: energy tile keeps today\'s total over the entity state');
