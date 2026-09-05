// Regression test: the retained bridge config must fit the buffer the panel is
// holding at the moment it subscribes.
//
// Since v0.6.9 PubSubClient::readPacket() no longer discards an oversized
// packet and keeps the session alive -- packetFitsBuffer() failing calls
// abortPacket(MQTT_MALFORMED_PACKET), which does _client->stop(). The bridge
// config is published retained, so the broker redelivers it on every connect,
// and reconnecting restarts the storm window that defers the grow to
// kMqttBufferLarge. With a 16 KB baseline that is an unbreakable loop: measured
// here at roughly one disconnect every three seconds, State=-5, with the panel
// never receiving its entity list.
//
// Observed payloads on real installs were 19,299 and 23,262 bytes, so the
// baseline has to clear those without depending on the deferred grow.

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const read = (rel) => fs.readFileSync(path.join(repoRoot, rel), 'utf8');

const networkManager = read('src/network/network_manager.cpp');

function constantKiB(name) {
  const m = networkManager.match(
    new RegExp(`constexpr\\s+uint16_t\\s+${name}\\s*=\\s*(\\d+)\\s*\\*\\s*1024`),
  );
  assert.ok(m, `${name} must stay a KiB-denominated uint16_t constant`);
  return Number(m[1]);
}

const normal = constantKiB('kMqttBufferNormal');
const media = constantKiB('kMqttBufferMedia');
const large = constantKiB('kMqttBufferLarge');

// The largest bridge config seen in the field, plus MQTT fixed/variable header
// and topic. Bridge v0.6.40 added per-sensor "state_kind" metadata to the
// sensor_meta array, which grew this by about 2.3 KB across all three panels
// (19,299 -> 21,606 and 23,262 -> 25,593). It is now past kMqttBufferMedia, so
// the 24 KiB tier alone would no longer clear it.
const OBSERVED_MAX_CONFIG_BYTES = 25593;
assert.ok(
  normal * 1024 > OBSERVED_MAX_CONFIG_BYTES,
  `kMqttBufferNormal (${normal} KiB) must exceed the largest observed retained ` +
    `bridge config (${OBSERVED_MAX_CONFIG_BYTES} bytes); otherwise the packet is ` +
    'rejected on connect and the panel reconnect-loops',
);

assert.ok(
  normal * 1024 <= 0xffff,
  'buffer sizes are uint16_t; kMqttBufferNormal must not overflow',
);

assert.ok(
  large >= normal,
  'the large window must never be smaller than the baseline',
);

// mqttNormalBufferSize() picks between the baseline and the media tier. The
// media tier is now the smaller of the two, so a plain ternary would shrink the
// buffer the moment a media tile exists and reintroduce the loop.
const sizeFn = networkManager.match(
  /uint16_t HomeTilesNetworkManager::mqttNormalBufferSize\(\) const \{[\s\S]*?\n\}/,
);
assert.ok(sizeFn, 'mqttNormalBufferSize() must exist');
assert.doesNotMatch(
  sizeFn[0],
  /return\s+mqtt_media_buffer_needed\s*\?\s*kMqttBufferMedia\s*:\s*kMqttBufferNormal\s*;/,
  'mqttNormalBufferSize() must not return the media tier unconditionally: with ' +
    `kMqttBufferMedia (${media} KiB) below kMqttBufferNormal (${normal} KiB) that ` +
    'shrinks the buffer when a media tile is configured',
);
assert.match(
  sizeFn[0],
  /kMqttBufferNormal/,
  'mqttNormalBufferSize() must floor its result at kMqttBufferNormal',
);

// Guard the upstream behaviour this test exists because of: an oversized packet
// tears the connection down rather than being skipped.
const pubsub = read('src/network/vendor/pubsubclient/PubSubClient.cpp');
assert.match(
  pubsub,
  /packetFitsBuffer\([\s\S]{0,200}?abortPacket\(MQTT_MALFORMED_PACKET\)/,
  'readPacket() still aborts the connection on an oversized packet; if upstream ' +
    'makes this non-fatal, revisit whether the baseline still needs to be 32 KiB',
);

console.log('ok  retained bridge config fits the connect-time MQTT buffer');
