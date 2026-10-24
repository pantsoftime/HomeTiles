// FORK: host regression for the listening dot (src/ui/shared/listening_dot_fork.h):
// it shows while speaker::assistCapturing() (wake word handoff and Assist
// capture), lies on the system layer, starts with the UI, and its place
// (listening_dot_place_fork.h) keeps clear of the first tile's corner arc and
// of both screen edges for every grid margin and tile radius in range.
// User 2026-10-24: "when the panel is listening can you add a dot in the upper
// left corner ... between the rounded rectangle tile and the corner".
import assert from 'node:assert/strict';
import {spawnSync} from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import {fileURLToPath} from 'node:url';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '../../..');
const read = rel => fs.readFileSync(path.join(root, rel), 'utf8').replace(/\r\n/g, '\n');

// --- Wiring --------------------------------------------------------------
const dot = read('src/ui/shared/listening_dot_fork.h');
assert.match(dot, /inline void poll\(lv_timer_t\*\) \{ show\(speaker::assistCapturing\(\)\); \}/);
assert.match(dot, /obj = lv_obj_create\(lv_layer_sys\(\)\);/, 'above popups and the screensaver');
assert.match(dot, /listening_dot_place::place\(GRID_PAD_LEFT, GRID_PAD_TOP,\s*ui_surface_style::radius\(tile_layout::scale_480\(22\)\)\)/,
  'the shown margin and the tile radius camera_indicator.h uses');
const speaker = read('src/audio/speaker.cpp');
assert.match(speaker, /bool assistCapturing\(\) \{\s*return static_cast<assist::PublicState>\(g_assist_state\.load\(\)\) != assist::PublicState::Idle;/);
const manager = read('src/ui/ui_manager.cpp');
assert.match(manager, /listening_dot::init\(\);\s*\/\/[^\n]*\n\s*camera_indicator::init\(\);/,
  'started with the UI, before camera_indicator (pinned last by upstream)');
assert.match(manager, /camera_indicator::invalidateVisible\(\);\s*listening_dot::invalidateVisible\(\);/);

// --- Geometry ------------------------------------------------------------
function findCompiler(candidates) {
  for (const candidate of candidates.filter(Boolean)) {
    const check = spawnSync(candidate, ['--version'], {encoding: 'utf8'});
    if (!check.error && check.status === 0) return candidate;
  }
  return null;
}
const cxx = findCompiler([process.env.CXX, 'clang++', 'g++', 'c++']);
if (!cxx) {
  console.log('SKIP: listening dot geometry needs a host C++ compiler (wiring checked)');
  process.exit(0);
}
const program = `#include <cassert>
#include <cmath>
#include <cstdio>
#include "src/ui/shared/listening_dot_place_fork.h"
using namespace listening_dot_place;
int main() {
  // Tab5: margin 18/16 px, outer radius ~36 px -> a dot of about 14 px.
  const Place tab5 = place(18, 16, 36);
  assert(tab5.radius >= 6 && tab5.radius <= 8);
  // Every grid margin from 4 px and tile radius from tile_radius::kMinimum
  // (15 px on 480x480 panels) up.
  for (int pad_left = 4; pad_left <= 24; ++pad_left) {
    for (int pad_top = 4; pad_top <= 24; ++pad_top) {
      for (int r = 15; r <= 60; ++r) {
        const Place p = place(pad_left, pad_top, r);
        // Inside the screen, at least a pixel from both edges.
        assert(p.center_x - p.radius >= 1 && p.center_y - p.radius >= 1);
        // Clear of the tile's corner arc (centre pad + r, radius r), and of its
        // straight edges: the dot stays inside the corner square.
        const double dx = pad_left + r - p.center_x;
        const double dy = pad_top + r - p.center_y;
        assert(std::sqrt(dx * dx + dy * dy) >= r + p.radius + 1.0);
        assert(p.center_x + p.radius <= pad_left + r && p.center_y + p.radius <= pad_top + r);
      }
    }
  }
  std::puts("ok");
  return 0;
}
`;
const out = path.join(root, 'build/tests/listening-dot-fork');
fs.mkdirSync(out, {recursive: true});
const cpp = path.join(out, 'listening-dot.cpp');
const exe = path.join(out, process.platform === 'win32' ? 'listening-dot.exe' : 'listening-dot');
fs.writeFileSync(cpp, program);
const build = spawnSync(cxx, ['-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', root, cpp, '-o', exe],
  {encoding: 'utf8'});
assert.equal(build.status, 0, `listening dot test did not compile:\n${build.stdout}${build.stderr}`);
const run = spawnSync(exe, [], {encoding: 'utf8'});
assert.equal(run.status, 0, `listening dot test failed:\n${run.stdout}${run.stderr}`);
assert.equal(run.stdout.trim(), 'ok');
console.log('Listening dot fork regressions passed.');
