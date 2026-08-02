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
