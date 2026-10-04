# HomeTiles — pantsoftime fork

A customization sandbox on top of [GalusPeres/HomeTiles](https://github.com/GalusPeres/HomeTiles)
(MIT). Upstream is the source of truth; this fork carries a small set of local
features and re-releases them so the panels can update themselves over the air.

## Why this fork exists

The on-device updater checks a single hard-coded repository. Running a patched
build against upstream's release feed means the next "Check for updates" would
happily install upstream firmware over the local changes. So `kRepoUrl` in
`src/core/firmware/github_update.h` points at **this** fork, and releases are published
here.

## Versioning

The updater parses tags as three numbers and installs anything numerically
greater than the running firmware. To stay clear of upstream's numbering, this
fork appends a patch digit to the upstream release it is based on:

| Upstream | This fork |
| --- | --- |
| `v0.6.3` | `v0.6.31`, `v0.6.32`, … |
| `v0.6.4` | `v0.6.41`, `v0.6.42`, … |

That keeps the base version readable and guarantees our tags sort above the
upstream release they derive from, without ever colliding with a real upstream
tag.

## Releasing

Same flow as upstream (CI does the building — no local Arduino toolchain):

1. Bump `FW_VERSION` in `version.txt`.
2. `git commit`
3. `git tag vX.Y.ZN && git push --follow-tags`

The `Firmware builds` workflow runs on the tag, builds every device profile,
and publishes the `.bin` assets to the GitHub release. Panels then see it under
**Settings → System → Check for updates**.

The workflow also runs on pull requests, which is the cheap way to compile-check
a change before releasing anything.

> Note: on ESP32-P4 boards the on-device GitHub download can occasionally fail
> (the ESP32-C6 WiFi coprocessor struggles with the long TLS transfer). The
> fallback is the web admin's manual firmware upload — see upstream
> `docs/updating.md`.

## Merging upstream

```bash
git remote add upstream https://github.com/GalusPeres/HomeTiles.git   # once
git fetch upstream --tags
git merge v0.6.9          # a RELEASE TAG, never upstream/main
```

Merge upstream **release tags**, not upstream's default branch. That branch was
renamed `master` -> `main` on 2026-09-11; the rule is unchanged, only the name.
`v0.6.81` was built from it nine commits past the `v0.6.8` tag and shipped
unreleased work; every panel then reconnect-looped and never got its entity list
back.

Do **not** resolve an unwanted upstream merge by keeping our tree and discarding
theirs. `v0.6.82` did that, which made those nine commits ancestors of our master
while dropping their content — so the next merge saw them as already merged and
silently omitted 4,664 lines of `v0.6.9`. If a merge must be rebuilt, branch from
the upstream tag and replay the fork delta onto it:

```bash
git checkout --detach v0.6.8 && git read-tree -u --reset v0.6.122   # PREVIOUS FORK TAG
git commit -m "fork delta" && git branch -f fork-delta
git checkout -B fork-vX.Y.Z1 v0.6.9 && git merge fork-delta   # base is v0.6.8
```

Replay from the **previous fork release tag**, never from `master`. Recovering
from the `v0.6.82` incident moved the live line onto `fork-vX.Y.ZN` branches and
left `master` abandoned at `v0.6.82`; the two have since diverged, so neither is
an ancestor of the other. Running the recipe above against `master` today would
replay a 73-commit-old tree and quietly revert four releases — the very failure
it is meant to prevent. `git branch -vv` and the newest `v0.6.*` tag identify the
real tip.

Conflicts are most likely in the files touched below. After merging, bump
`FW_VERSION` to the new upstream base + `1` and tag.

### MQTT receive buffer must fit the retained bridge config

> **Superseded by upstream v0.6.10 — there is no fork override here any more.**
> `kMqttBufferNormal` is upstream's 16 KB again. `readPacket()` now grows the
> receive buffer up to `kMaxInboundPacketBytes` (`UINT16_MAX`) and drains
> anything larger instead of aborting, which fixes the same failure generically.
> The account below is kept because it explains the panel-side symptom, not
> because the tree still looks like this.

The bridge publishes its config retained, so it arrives the instant the panel
subscribes — measured at 19,299 and 23,262 bytes on the panels here. In `v0.6.9`
an oversized packet stopped being skipped: `packetFitsBuffer()` failing called
`abortPacket(MQTT_MALFORMED_PACKET)`, which stops the client. Because
reconnecting also restarts the storm window that defers the grow to
`kMqttBufferLarge`, the buffer never grew and the panel looped. The fork's fix
was a 32 KB baseline; upstream's later fix made that unnecessary.

`mqttNormalBufferSize()` now returns the media tier (24 KB) or the normal tier
(16 KB), floored at `mqtt_receive_buffer_floor`, the observed high-water mark, so
a large retained config does not cause a reallocation on every repeat. The old
warning that the 24 KB media tier sat *below* the baseline no longer applies —
that was only true while the baseline was 32 KB. Guarded by
`tools/tests/network/test-mqtt-config-buffer-fit.mjs`.

## Local changes

### Live sensor value on folder tiles

Folder tiles can optionally display a live entity value alongside their icon and
title, while still navigating into the folder on tap. Configure it in the web
admin: select a folder tile and pick an entity under the (optional) sensor field.

This works because folder tiles store their navigation target in
`key_code`/`key_modifier`, leaving `sensor_entity` (and `sensor_unit`,
`sensor_decimals`, `sensor_value_font`) unused — so no storage format change was
needed and existing configurations stay compatible.

Files touched:

| File | Change |
| --- | --- |
| `src/types/navigate/renderer.cpp` / `.h` | Render the value label, register it in the shared `SensorTileWidgets` table; takes `GridType` now |
| `src/types/navigate/web_handler.cpp` | Persist `sensor_entity` / unit / decimals / font (preserved when the editor omits them) |
| `src/types/navigate/web_html.cpp` / `.h` | Entity picker + decimals + font size in the folder tile editor |
| `src/types/navigate/web_scripts.cpp` | `load` / `save` / `reset` for the new fields |
| `src/types/types_registry.cpp` | Pass `GridType` and the sensor option list through the wrappers |
| `src/network/mqtt/mqtt_handlers.cpp` | Subscribe to entities referenced by folder tiles |
| `src/ui/tabs/tiles/tab_tiles_unified.cpp` | Route cached + live state updates to folder tiles |

Value updates reuse the existing sensor pipeline unchanged —
`update_sensor_tile_value()` addresses widgets by grid index and is not
type-aware, so a folder tile that registers a `value_label` is updated like any
sensor tile.

Deliberately not changed: the icon-refresh pass in `tab_tiles_unified.cpp` still
skips folder tiles, so a folder keeps the icon you chose instead of inheriting
the Home Assistant entity icon.

### Monospace value fonts (numbered 200-203)

Sensor and folder tiles offer JetBrains Mono 20/24 and Mono Bold 20/24 next to
upstream's sizes. Their stored `sensor_value_font` numbers are **200-203**
(`src/tiles/config/sensor_value_font_fork.h`), far above upstream's choices.

They used to be 5-8. Upstream v0.7.0 gave 5 to its 28 px size, so v0.7.01
renumbered them: 6-8 are read forward on load (`clampSensorValueFont`, and the
sensor save handler for older exports), while a stored 5 now means 28 -- it was
"20 Mono", which no panel used. A `static_assert(SENSOR_VALUE_FONT_MAX < 6)` in
`tile_config.h` fails the build the day upstream claims 6; re-save every mono
tile on every panel before dropping the legacy mapping. Upstream's editor hides
every value-size option it does not list, so `syncCompactValueFontOptions()` in
`src/web/admin/tiles/layout.js` carries the mono numbers for full-size tiles.

### Half-height tiles (since v0.7.01)

- A half-height Folder or Settings tile shows neither the folder live value nor
  the battery caption -- there is no room for the line -- and still clears its
  slot in the sensor widget table, because a folder's `sensor_entity` updates
  are routed to its index regardless.
- A half-height Sensor, Binary Sensor or Energy tile whose icon is `none` uses
  the icon column for its text and centres it in a symmetric 8 px box
  (`compact_sensor_layout::apply_content()`, mirrored by an `admin.css` rule).
  Upstream reserves the column even without an icon.

Guarded by `tools/tests/tiles/test-compact-tile-fork-integration.mjs`.

### Fork fields in upstream's no-op check (since v0.8.01)

Upstream v0.8.0 skips a tile save -- and the display rebuild -- when
`tileContentEquals()` (`tile_config.h`) finds nothing changed but the cell. Every
fork-only `Tile` field must be compared there, or editing just that field is
silently dropped: `sensor_navigate_target` (a sensor tile's tap-to-folder
target) is. `tools/tests/tiles/test-tile-move-fast-path.mjs` parses the struct
and fails on any missing field.

v0.8.0 also dropped the `GridType` argument from `render_navigate_tile()`. The
fork keeps it (folder value and battery caption need the grid's widget table and
the screensaver check); the registry passes it, and upstream's 5-argument
declaration in `tile_renderer.h` is left unused, as it already was in v0.7.01.

## Regenerating the WebUI assets

`src/web/assets/admin.css` is a source. Since v0.7.0 `src/web/assets/admin.js`
is **generated**: edit the modules under `src/web/admin/` (and
`src/types/*/admin*.js`, listed in `src/web/admin/bundle.json`), never the bundle
-- `generate-web-assets.mjs` rebuilds it and an edit there is silently lost. The
firmware embeds the gzipped `src/web/generated/*.inc` blobs, and CI fails the
build if they are stale (`node tools/generate-web-assets.mjs --check`).

**Regenerate with Node 24**, the version CI uses. The generator pins the gzip
level and zeroes the mtime and OS marker, but zlib's output still differs
between Node releases, so regenerating with a newer Node rewrites *both* blobs
and CI then rejects them -- including the CSS one, which this fork never edits:

```
docker run --rm --user "$(id -u):$(id -g)" -v "$PWD":/w -w /w node:24 \
  node tools/generate-web-assets.mjs
```
