#pragma once

#include "src/tiles/tile_renderer.h"

lv_obj_t* render_sensor_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index, GridType grid_type);

// Position/justify a sensor tile's value label. Shared by the renderer and the
// live-update path: whether a value is a single number or a multi-line block
// is a property of the *current* text, not of the tile, so the choice has to be
// re-made every time the text changes (see the note in the .cpp).
void sensor_apply_value_layout(lv_obj_t* value_label,
                               const Tile& tile,
                               bool multiline,
                               bool gauge_enabled,
                               bool graph_enabled);

// Opt-in second line. key_code is unused by sensor tiles, so it carries the
// flag without touching the stored config format. When set, a value payload
// of "82.2 °F\n55 %" renders the first line at the normal size and the rest
// small underneath -- both come from the tile's single entity, so an HA
// template sensor decides what the second line says.
static inline bool sensor_tile_has_subtitle(const Tile& tile) {
  return tile.key_code == 1;
}

// Split a rendered value into its headline and subtitle halves.
void sensor_split_subtitle(const String& combined, String& head, String& tail);

// Y offset of the small second line, relative to the tile centre.
lv_coord_t sensor_subtitle_y(const Tile& tile);
