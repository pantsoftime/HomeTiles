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
                               bool graph_enabled,
                               bool has_caption);

// How a two-line value is presented, decided by the tile's font rather than by
// a stored flag: fonts 5-8 are the monospace faces, which exist precisely so
// that tabular data lines up, so those keep the left-aligned block. Every other
// font is proportional and gets the headline-plus-small-caption treatment --
// "82.2 °F\n55 %" reads as a temperature with humidity under it.
//
// This deliberately carries no configuration. key_code and key_modifier look
// free on a sensor tile but are zeroed for TILE_SENSOR when the grid is
// loaded, so a flag stored there does not survive a save/load cycle.
static inline bool sensor_tile_caption_mode(const Tile& tile) {
  return tile.sensor_value_font <= 4;
}

// Optional second entity shown in the caption. Splitting one text state across
// two lines costs the tile its history graph, because a state carrying a unit
// and a second line is not numeric and Home Assistant has no numeric history
// for it. Pointing the caption at its own entity keeps the displayed entity a
// plain number, so the graph works and the caption still shows.
//
// Stored in key_macro: only TILE_TEXT uses that field, and unlike key_code and
// scene_alias it survives a sensor tile's save/load cycle intact.
static inline bool sensor_tile_has_caption_entity(const Tile& tile) {
  return tile.key_macro.length() > 0;
}

// Split a rendered value into its headline and subtitle halves.
void sensor_split_subtitle(const String& combined, String& head, String& tail);

// Y offset of the small second line, relative to the tile centre.
lv_coord_t sensor_subtitle_y(const Tile& tile);
