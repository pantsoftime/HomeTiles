#pragma once

#include "src/tiles/runtime/tile_renderer.h"

lv_obj_t* render_navigate_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index,
                               GridType grid_type);

// Fork: on a device that measures its own battery (the Tab5), a Settings tile
// carries a battery caption in the value slot folder tiles use. This is the one
// place the condition lives; the renderer and the live update route both ask it.
bool navigate_settings_shows_battery(const Tile& tile, GridType grid_type);
