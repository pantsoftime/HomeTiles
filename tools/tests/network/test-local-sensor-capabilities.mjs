import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import {resolve} from 'node:path';
import {fileURLToPath} from 'node:url';

const root = resolve(fileURLToPath(new URL('../../..', import.meta.url)));
const read = path => readFileSync(resolve(root, path), 'utf8');
const mqtt = read('src/network/mqtt/mqtt_handlers.cpp');
const bridge = read('src/network/bridge/ha_bridge_config.cpp');
const battery = read('src/core/power/battery_state.cpp');
assert.match(battery, /batteryStateSupportsMeasurement\(\)\s*\{ return false;/);
assert.match(battery, /#if defined\(DEVICE_M5STACKS_TAB5\)/,
  'Real battery telemetry must stay guarded by the exact device profile');
assert.doesNotMatch(mqtt, /__has_include\(<OneWire.h>\)/,
  'Installing a library must not enable unverified GPIO probing');
assert.equal((mqtt.match(/sync_external_temp_entity\(/g) || []).length, 1,
  'The legacy synthetic sensor must not run on connect or in the periodic service');
const batterySync = mqtt.slice(
  mqtt.indexOf('static void sync_internal_battery_entity()'),
  mqtt.indexOf('static const char* kSleepOptionLabels'));
assert.ok(batterySync.length > 0, 'sync_internal_battery_entity() must exist');
assert.match(batterySync, /if \(!batteryStateSupportsMeasurement\(\)\) \{\s*return;/,
  'Unsupported profiles must not register the internal battery entity');
assert.match(batterySync, /if \(batteryStateIsBatteryMissing\(\)\) \{\s*return;/,
  'A missing battery must not publish a synthetic value');
assert.ok(
  batterySync.indexOf('const int soc = readBatterySocPercent();') <
    batterySync.indexOf('batteryStateIsBatteryMissing()'),
  'The battery must be polled before the missing flag is tested, otherwise a '
  + 'latched missing state can never clear once a battery is inserted');
const snapshot = mqtt.slice(mqtt.indexOf('void mqttPublishHomeSnapshot()'), mqtt.indexOf('void mqttPublishDeviceSettings()'));
assert.match(snapshot, /if \(batteryStateSupportsMeasurement\(\)\)/);
assert.match(snapshot, /TopicKey::SENSOR_SOC\), "", true/,
  'Unsupported telemetry must remove its retained synthetic value');
assert.doesNotMatch(mqtt, /normalized.startsWith\("sensor.tab5_"\)/,
  'Real user sensors must not be intercepted because their names start with Tab5');
assert.match(bridge, /batteryStateSupportsMeasurement\(\) \? "true" : "false"/);
assert.match(bridge, /legacy_external_temperature\\":false/);
assert.match(bridge, /configured_sensors_text/);
assert.match(bridge, /hardwareIo.appendBridgeJson\(json\)/,
  'Explicitly configured physical channels must still be announced');
const io = read('src/io/hardware_io.cpp');
assert.match(io, /local_io/);
console.log('Local sensor capability and feedback regressions passed.');
