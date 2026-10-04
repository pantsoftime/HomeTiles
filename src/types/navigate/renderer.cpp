#include "src/ui/shared/ui_surface_style.h"
#include "src/types/navigate/renderer.h"
#include "src/tiles/runtime/tile_renderer_shared.h"
#include "src/tiles/runtime/tile_renderer_fonts.h"
#include "src/tiles/runtime/tile_icon_disc.h"
#include "src/tiles/runtime/compact_sensor_layout.h"
#include "src/tiles/runtime/tile_icon_source.h"
#include "src/tiles/icons/mdi_icons.h"
#include "src/tiles/config/tile_config.h"
#include "src/ui/ui_manager.h"
#include "src/core/power/battery_state.h"
#include "src/ui/shared/icon_lock_mark.h"
#include "src/core/config/config_manager.h"
#include <Arduino.h>

struct NavigateEventData {
  uint8_t target_kind;
  uint16_t target_folder_id;
  String title;
  String icon_name;
  uint32_t bg_color;
};

static uint16_t navFolderIdFromTile(const Tile& tile) {
  return static_cast<uint16_t>((static_cast<uint16_t>(tile.key_modifier) << 8) | tile.key_code);
}

// A PIN-protected Folder or Settings tile shows a lock in its icon
// (icon_lock_mark.h, user 2026-10-02); the rim takes the icon's disc over the
// tile card. Half-height icons sit inside their disc, taller ones above it.
static void lock_mark_event_cb(lv_event_t* event) {
  lv_obj_t* icon = static_cast<lv_obj_t*>(lv_event_get_current_target(event));
  if (lv_event_get_code(event) == LV_EVENT_REFR_EXT_DRAW_SIZE) {
    icon_lock_mark::ext_draw_size(event, icon);
    return;
  }
  if (lv_event_get_code(event) != LV_EVENT_DRAW_POST) return;
  lv_obj_t* disc = tile_icon_disc::disc_of(icon);
  lv_obj_t* card = lv_obj_get_parent(disc ? disc : icon);
  const lv_color_t under = card ? lv_obj_get_style_bg_color(card, LV_PART_MAIN) : lv_color_black();
  icon_lock_mark::draw(lv_event_get_layer(event), icon, icon_lock_mark::behind(disc, under));
}

static bool navigate_tile_locked(const Tile& tile) {
  if (tile.type == TILE_SETTINGS) return configManager.getConfig().settings_pin_enabled;
  return tile.type == TILE_FOLDER && tileConfig.isFolderPinEnabled(navFolderIdFromTile(tile));
}

// Schriftgroesse des optionalen Live-Werts -- gleiche Auswahl wie bei
// Sensor-Kacheln (sensor_value_font), nur mit kleinerem Default, weil sich der
// Wert die Kachel mit Icon und Titel teilt.
static const lv_font_t* get_navigate_value_font(const Tile& tile) {
  return tile_layout::value_font_for_choice(tile.sensor_value_font,
                                            tile_layout::content_font_28());
}

// Icon offset (from the card centre) for a tile that also shows a value. The
// icon sits high to leave room for the value line, and v0.7.0 draws a disc of
// header_diameter() centred on it. Place the icon so that disc starts inset()
// below the card top -- the same gap the corner discs of Sensor tiles keep.
// The fixed -48 it replaced left the disc flush with the card edge on the Tab5
// and the 4B. Measured from the one-cell height so a taller tile keeps the
// icon, value and title together around its centre, as before.
static lv_coord_t navigate_value_icon_y(const String& icon_char) {
  lv_point_t icon_size{};
  lv_text_get_size(&icon_size, icon_char.c_str(), FONT_MDI_ICONS, 0, 0, LV_COORD_MAX,
                   LV_TEXT_FLAG_NONE);
  if (icon_size.x <= 0) {
    icon_size.x = lv_font_get_glyph_width(FONT_MDI_ICONS, tile_icon_disc::kMdiReferenceGlyph, 0);
  }
  const int disc = tile_icon_disc::header_diameter(icon_size.x);
  return static_cast<lv_coord_t>((2 * tile_icon_disc::inset() + disc - GRID_CELL_H) / 2);
}

bool navigate_settings_shows_battery(const Tile& tile, GridType grid_type) {
  // A half-height Settings tile has no room for the caption line.
  return tile.type == TILE_SETTINGS && grid_type != GridType::SCREENSAVER &&
         !tile_geometry::compact_icon_title(tile.type, tile.span_w, tile.span_h) &&
         batteryStateSupportsMeasurement();
}

lv_obj_t* render_navigate_tile(lv_obj_t* parent, int col, int row, const Tile& tile, uint8_t index,
                               GridType grid_type) {
  lv_obj_t* btn = lv_button_create(parent);
  ui_surface_style::apply_radius(btn, tile_layout::scale_480(22), 0);
  lv_obj_set_style_border_width(btn, 0, 0);

  // Without an explicit color, all navigation types use the global default
  // tile color like the other HomeTiles tiles.
  uint32_t btn_color = tileBgColorOrDefault(tile, tileDefaultBgColor());
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(btn_color), LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_FOCUSED);

  // Pressed state: 10% brighter.
  uint32_t pressed_color = brighten_rgb_color(btn_color, 0x10);
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_color(btn, lv_color_hex(pressed_color), LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_bg_grad_dir(btn, LV_GRAD_DIR_NONE, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_outline_width(btn, 0, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_FOCUSED);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_PRESSED);
  lv_obj_set_style_outline_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN | (LV_STATE_FOCUSED | LV_STATE_PRESSED));
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
  disable_pressed_button_animation(btn);

  place_tile_card(btn, col, row, tile);

  // Optional icon label when icon_name is set.
  lv_obj_t* icon_lbl = nullptr;
  String iconChar;
  if (tile.icon_name.length() > 0 && FONT_MDI_ICONS != nullptr) {
    iconChar = getMdiChar(tile.icon_name);
  }
  bool has_icon = iconChar.length() > 0;
  // Without an own icon a protected tile shows the lock as its icon.
  const bool locked = navigate_tile_locked(tile);
  const bool lock_is_icon = locked && !has_icon && FONT_MDI_ICONS != nullptr;
  if (lock_is_icon) {
    iconChar = getMdiChar("lock");
    has_icon = iconChar.length() > 0;
  }
  bool has_title = tile.title.length() > 0;
  // A half-height navigation tile uses the half-height Sensor header: the
  // icon in the concentric corner disc and the title, if any, beside it.
  const bool compact = tile_geometry::compact_icon_title(tile.type, tile.span_w, tile.span_h);
  // Optionaler Live-Wert: Ordner-Kacheln legen ihr Navigationsziel in
  // key_code/key_modifier ab, sensor_entity ist daher frei und wird hier fuer
  // eine mitlaufende Sensor-Anzeige genutzt (z.B. ein Ordner, der zusaetzlich
  // eine Anzahl oder einen Messwert aus seinem Inhalt zeigt).
  //
  // A Settings tile on a battery-measuring device takes the same value slot for
  // its battery caption, so it inherits the folder layout unchanged.
  //
  // A half-height tile has no room for a third line, so it shows neither; the
  // same rule keeps captions off half-height Sensor tiles.
  const bool has_battery = navigate_settings_shows_battery(tile, grid_type);
  bool has_value = !compact &&
                   ((tile.sensor_entity.length() > 0 &&
                     grid_type != GridType::SCREENSAVER) ||
                    has_battery);

  // This tile owns its slot in the sensor widget table. The table is static and
  // nothing clears it when a grid is rebuilt, so every field a previously
  // rendered tile left at this index is a dangling lv_obj_t. Naming fields
  // individually is how the 86-panels came to crash on opening a folder that
  // contains folder tiles: subtitle_label survived from a caption-bearing
  // sensor tile at the same index, and update_sensor_tile_value() called
  // lv_label_set_text() on freed memory (load fault, MCAUSE=5). The slot is
  // cleared even without a value: a half-height folder tile keeps its
  // sensor_entity, so live updates are still routed to this index.
  SensorTileWidgets* widgets = tile_renderer_get_sensor_widgets(grid_type);
  if (widgets && index < TILES_PER_GRID) widgets[index] = SensorTileWidgets{};

  if (has_icon) {
    icon_lbl = lv_label_create(btn);
    if (icon_lbl) {
      set_label_style(icon_lbl, lv_color_white(), FONT_MDI_ICONS);
      lv_label_set_text(icon_lbl, iconChar.c_str());
      tile_icon_source::apply_initial(icon_lbl, tile);

      // Center icon and title on two lines, or the icon alone on one line.
      // A third line appears when the tile also shows a value.
      //
      // With a value the icon sits as high as its disc allows
      // (navigate_value_icon_y) and the value follows just below the disc.
      // Value and title looked cramped, but the free space is BELOW the title
      // (~20 px unused at the bottom), not above the value: lifting the value
      // visibly pushed tiles with a short value off-centre even though they
      // never had the problem. So only the title moves down.
      if (!compact) {
        if (has_value) {
          lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0, navigate_value_icon_y(iconChar));
        } else if (has_title) {
          lv_obj_align(icon_lbl, LV_ALIGN_CENTER, 0,
                       tile_layout::scale_i16(-20));
        } else {
          lv_obj_center(icon_lbl);  // Center the icon when there is no title.
        }
        tile_icon_disc::add_round(btn, icon_lbl);
      }
    }
  }

  // Optional value label, only when an entity is configured.
  if (has_value) {
    lv_obj_t* v = lv_label_create(btn);
    if (v) {
      // The battery caption is a secondary readout, so it uses the smaller 24 px
      // choice rather than the folder default. The Settings save handler forces
      // sensor_value_font to 0, so a per-tile font choice could not survive anyway.
      set_label_style(v, lv_color_white(),
                      has_battery ? tile_layout::content_font_24()
                                  : get_navigate_value_font(tile));
      lv_label_set_long_mode(v, LV_LABEL_LONG_CLIP);
      lv_obj_set_width(v, LV_PCT(100));
      lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_CENTER, 0);
      // Seed the caption from local battery state: it is measured on this device,
      // so there is nothing to wait for, and a grid rebuild must not blank it until
      // the next percentage change. Matches update_sensor_tile_value()'s "87 %".
      char seed[16] = "--";
      if (has_battery && batteryStateHasDisplayPercent()) {
        snprintf(seed, sizeof(seed), "%ld %%",
                 static_cast<long>(batteryStateDisplayPercent()));
      }
      lv_label_set_text(v, seed);
      lv_obj_align(v, LV_ALIGN_CENTER, 0,
                   tile_layout::scale(has_icon ? 6 : -12));

      // Register in the same widget table the sensor tiles update through --
      // update_sensor_tile_value() works purely off the grid index and is
      // therefore type-independent. The record was reset above.
      if (widgets && index < TILES_PER_GRID) widgets[index].value_label = v;
    }
  }

  // Show the title label only when a title is set.
  lv_obj_t* title_lbl = nullptr;
  if (has_title) {
    lv_obj_t* l = lv_label_create(btn);
    title_lbl = l;
    if (l) {
      set_label_style(l, lv_color_white(), tile_layout::header_title_font());
      hometiles_title::tile(l, tile.title.c_str(), false);

      // Position below the icon, or center when there is no icon.
      if (compact) {
        // compact_sensor_layout places it beside the disc below.
      } else if (has_value) {
        lv_obj_align(l, LV_ALIGN_CENTER, 0, tile_layout::scale(55));
      } else if (icon_lbl) {
        lv_obj_align(l, LV_ALIGN_CENTER, 0, tile_layout::scale(35));
      } else {
        lv_obj_center(l);  // Center the title when there is no icon.
      }
    }
  }
  if (compact) compact_sensor_layout::apply(btn, icon_lbl, title_lbl, nullptr, tile);
  if (locked && !lock_is_icon && icon_lbl) {
    lv_obj_add_event_cb(icon_lbl, lock_mark_event_cb, LV_EVENT_DRAW_POST, nullptr);
    lv_obj_add_event_cb(icon_lbl, lock_mark_event_cb, LV_EVENT_REFR_EXT_DRAW_SIZE, nullptr);
    lv_obj_refresh_ext_draw_size(icon_lbl);
  }

  // Event handler for tab navigation.
  static constexpr uint8_t NAV_KIND_FOLDER = 0;
  static constexpr uint8_t NAV_KIND_SETTINGS = 1;
  static constexpr uint8_t NAV_KIND_BACK = 2;
  uint8_t target_kind = NAV_KIND_FOLDER;
  uint16_t target_folder = 0;
  if (tile.type == TILE_SETTINGS) {
    target_kind = NAV_KIND_SETTINGS;
    uiManager.setSettingsGestureStyle(tile.title, tile.icon_name, btn_color);
  } else if (tile.type == TILE_BACK) {
    target_kind = NAV_KIND_BACK;
  } else {
    target_kind = NAV_KIND_FOLDER;
    target_folder = navFolderIdFromTile(tile);
  }
  Serial.printf("[Navigate] Render navigation tile - kind=%u, folder=%u\n",
                static_cast<unsigned>(target_kind),
                static_cast<unsigned>(target_folder));

  NavigateEventData* event_data = new NavigateEventData{
    target_kind,
    target_folder,
    tile.title,
    tile.icon_name,
    btn_color
  };

  lv_obj_add_event_cb(
      btn,
      [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
        NavigateEventData* data = static_cast<NavigateEventData*>(lv_event_get_user_data(e));
        if (!data) return;
        if (data->target_kind == NAV_KIND_SETTINGS) {
          Serial.printf("[Tile] Navigation CLICKED! Settings, title: %s\n", data->title.c_str());
          uiManager.requestSettingsAccess(data->title, data->icon_name,
                                          data->bg_color);
        } else if (data->target_kind == NAV_KIND_BACK) {
          uint16_t current = tileConfig.getActiveFolderId();
          uint16_t parent = tileConfig.getFolderParent(current);
          Serial.printf("[Tile] Navigation CLICKED! Back to %u, title: %s\n",
                        static_cast<unsigned>(parent), data->title.c_str());
          uiManager.switchToFolder(parent);
        } else {
          Serial.printf("[Tile] Navigation CLICKED! Folder %u, title: %s\n",
                        static_cast<unsigned>(data->target_folder_id), data->title.c_str());
          // A PIN popup shows the icon in the color the tile shows right now
          // (fixed or from the source entity).
          lv_obj_t* icon = tile_icon_source::card_icon(
              static_cast<lv_obj_t*>(lv_event_get_current_target(e)));
          const uint32_t icon_color =
              icon ? lv_color_to_u32(lv_obj_get_style_text_color(icon, LV_PART_MAIN)) & 0xFFFFFF
                   : 0xFFFFFF;
          // The PIN popup also inherits a rules tint of the tile.
          const uint32_t popup_color = tile_icon_source::popup_background(
              static_cast<lv_obj_t*>(lv_event_get_current_target(e)), data->bg_color);
          uiManager.requestFolderAccess(data->target_folder_id, data->title,
                                        data->icon_name,
                                        popup_color, icon_color);
        }
      },
      LV_EVENT_CLICKED,
      event_data);
  lv_obj_add_event_cb(
      btn,
      [](lv_event_t* e) {
        if (lv_event_get_code(e) != LV_EVENT_DELETE) return;
        NavigateEventData* data = static_cast<NavigateEventData*>(lv_event_get_user_data(e));
        delete data;
      },
      LV_EVENT_DELETE,
      event_data);

  return btn;
}

