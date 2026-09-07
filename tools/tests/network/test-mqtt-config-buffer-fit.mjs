// Regression test: the retained bridge config must survive arriving on connect.
//
// History. The bridge publishes its config retained, so the broker delivers it
// the instant a panel subscribes, and it has outgrown the 16 KB baseline --
// measured at 24,383 / 24,388 / 28,168 bytes on the three panels here after Bridge v0.6.44. In
// v0.6.9 an oversized PUBLISH called abortPacket(MQTT_MALFORMED_PACKET), which
// stops the client; because reconnecting also restarts the storm window that
// defers the buffer grow, the panel reconnect-looped about every three seconds
// and never received its entity list.
//
// This fork briefly fixed that by raising kMqttBufferNormal to 32 KB. v0.6.10
// replaced that with a better fix -- grow the receive buffer on demand and
// drain rather than disconnect -- so the fork patch was dropped. This test now
// guards UPSTREAM's mechanism instead of our constant, because that mechanism
// is the only thing standing between a large config and the reconnect loop.

import assert from 'node:assert/strict';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..', '..', '..');
const read = (rel) => fs.readFileSync(path.join(repoRoot, rel), 'utf8');

// Largest retained config observed in the field, after Bridge v0.6.40 added
// per-sensor state_kind metadata (+2.3 KB across every panel).
const OBSERVED_MAX_CONFIG_BYTES = 28168;

const safety = read('src/network/mqtt/mqtt_packet_safety.h');
const pubsub = read('src/network/vendor/pubsubclient/PubSubClient.cpp');

const maxInbound = safety.match(/kMaxInboundPacketBytes\s*=\s*([A-Z0-9_]+)/);
assert.ok(maxInbound, 'kMaxInboundPacketBytes must exist');
assert.equal(maxInbound[1], 'UINT16_MAX',
  'the on-demand grow ceiling must stay at the full 16-bit buffer size');
assert.ok(65535 > OBSERVED_MAX_CONFIG_BYTES,
  `grow ceiling must clear the largest observed config (${OBSERVED_MAX_CONFIG_BYTES} bytes)`);

// An oversized PUBLISH must grow the buffer rather than abort the connection.
assert.match(pubsub, /if\s*\(!fits\s*&&\s*!this->stream\)/,
  'readPacket() must still special-case a packet that does not fit');
assert.match(pubsub, /setBufferSize\(capacity\)[\s\S]{0,200}?fits\s*=\s*true/,
  'an oversized PUBLISH must attempt an on-demand buffer grow');
assert.match(pubsub, /if\s*\(!isPublish\)\s*return abortPacket\(MQTT_MALFORMED_PACKET\)/,
  'only a non-PUBLISH oversize may tear the connection down');

// And when it still does not fit, it must be drained, not disconnected, up to
// a hard bound. Losing this is what produced the v0.6.81 reconnect loop.
assert.match(safety, /kMaxDiscardPacketBytes/, 'the drain bound must exist');
assert.match(pubsub, /packetBytes > hometiles_mqtt::kMaxDiscardPacketBytes[\s\S]{0,120}?abortPacket\(MQTT_PACKET_TOO_LARGE\)/,
  'a too-large packet must abort only past the discard bound, not on first overflow');

// The fork no longer overrides the baseline; upstream's 16 KB plus on-demand
// growth is the supported path. If this ever needs raising again, raise it
// deliberately rather than by accident.
const nm = read('src/network/network_manager.cpp');
const normal = nm.match(/constexpr\s+uint16_t\s+kMqttBufferNormal\s*=\s*(\d+)\s*\*\s*1024/);
assert.ok(normal, 'kMqttBufferNormal must stay a KiB-denominated constant');
assert.equal(Number(normal[1]), 16,
  'baseline is upstream 16 KiB; the fork override was dropped in favour of on-demand growth');

console.log('ok  retained bridge config survives arrival on connect');
