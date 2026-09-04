#include "src/types/navigate/web_handler.h"
#include "src/web/web_admin_tile_helpers.h"

bool apply_navigate_fields_from_request(
    WebServer& server,
    Tile& tile,
    uint16_t folder_id,
    TileConfig& tileConfig,
    String& error_message,
    uint16_t previous_target,
    TileType previous_type) {
  const bool has_arg = server.hasArg("navigate_target");
  int raw = has_arg ? server.arg("navigate_target").toInt() : -1;
  uint16_t target_id = 0;
  if (raw > 0 && tileConfig.folderExists(static_cast<uint16_t>(raw))) {
    target_id = static_cast<uint16_t>(raw);
  } else if (!has_arg && previous_target != 0 && tileConfig.folderExists(previous_target)) {
    // The web editor has no target picker and never re-sends navigate_target,
    // so a rename arrives with the arg absent. Keep the folder this tile
    // already points to instead of orphaning it and creating a duplicate.
    target_id = previous_target;
  }

  if (target_id == 0 || !tileConfig.folderExists(target_id)) {
    uint16_t new_id = 0;
    if (!tileConfig.createFolder(folder_id, tile.title, tile.icon_name, new_id)) {
      error_message = "Folder create failed";
      return false;
    }
    target_id = new_id;
  }
  tileConfig.updateFolder(target_id, tile.title, tile.icon_name);

  setNavigateTargetId(tile, target_id);

  // Optionaler Live-Wert auf der Ordner-Kachel: Das Navigationsziel liegt in
  // key_code/key_modifier, sensor_entity ist daher frei nutzbar. Fehlt der
  // Parameter komplett (z.B. beim reinen Umbenennen, wo der Editor nur einen
  // Teil der Felder sendet), bleibt die bisher hinterlegte Entity erhalten --
  // gleiche Logik wie oben beim navigate_target.
  if (server.hasArg("sensor_entity")) {
    String entity = server.arg("sensor_entity");
    entity.trim();
    tile.sensor_entity = entity;
  } else if (previous_type != TILE_FOLDER) {
    // The tile just became a folder. sensor_entity still holds whatever the
    // previous type stored there (a weather/climate entity, say) and folders
    // never cleared it, so without this the new value label would display a
    // leftover, unrelated entity.
    tile.sensor_entity = "";
  }

  if (tile.sensor_entity.length()) {
    if (server.hasArg("sensor_unit")) {
      tile.sensor_unit = server.arg("sensor_unit");
    }
    if (server.hasArg("sensor_decimals")) {
      const int decimals = server.arg("sensor_decimals").toInt();
      tile.sensor_decimals =
          decimals < 0 ? 0xFF : static_cast<uint8_t>(decimals > 9 ? 9 : decimals);
    }
    if (server.hasArg("sensor_value_font")) {
      tile.sensor_value_font =
          static_cast<uint8_t>(server.arg("sensor_value_font").toInt());
    }
  } else {
    // Ohne Entity bleibt die Kachel exakt wie bisher konfiguriert.
    tile.sensor_unit = "";
    tile.sensor_decimals = 0xFF;
    tile.sensor_value_font = 0;
  }

  tile.sensor_display_mode = 0;
  tile.sensor_gauge_min = 0;
  tile.sensor_gauge_max = 100;
  return true;
}
