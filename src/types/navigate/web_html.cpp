#include "src/types/navigate/web_html.h"
#include "src/web/web_admin_utils.h"
#include "src/core/config_manager.h"
#include "src/core/i18n.h"

void append_navigate_fields_html(String& html,
                                 const String& tab_id,
                                 const String& navigateOptionsHtml,
                                 const std::vector<String>& sensorOptions) {
  (void)navigateOptionsHtml;
  const auto& tr = i18n::strings(configManager.getConfig().language);

  html += R"html(
            <!-- Navigate Fields -->
            <div id=")html";
  html += tab_id;
  html += R"html(_navigate_fields" class="type-fields folder-pin-fields is-hidden">
              <label>)html";
  html += tr.sensor_entity;
  html += R"html( (optional)</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_navigate_sensor_entity">
                <option value="">)html";
  html += tr.no_selection;
  html += R"html(</option>
)html";

  for (const auto& opt : sensorOptions) {
    html += "<option value=\"";
    appendHtmlEscaped(html, opt);
    html += "\">";
    String label = humanizeIdentifier(opt, true) + " - " + opt;
    appendHtmlEscaped(html, label);
    html += "</option>";
  }

  html += R"html(
              </select>
              <label>)html";
  html += tr.sensor_decimals;
  html += R"html(</label>
              <input type="number" id=")html";
  html += tab_id;
  html += R"html(_navigate_sensor_decimals" min="0" max="6" step="1" placeholder="0">
              <label>)html";
  html += tr.sensor_value_size;
  html += R"html(</label>
              <select id=")html";
  html += tab_id;
  html += R"html(_navigate_sensor_value_font">
                <option value="0">28 (Default)</option>
                <option value="1">20</option>
                <option value="2">24</option>
                <option value="3">32</option>
                <option value="4">40</option>
                <option value="5">20 Mono</option>
                <option value="6">24 Mono</option>
                <option value="7">20 Mono Bold</option>
                <option value="8">24 Mono Bold</option>
              </select>
              <label class="folder-pin-toggle">
                <input type="checkbox" id=")html";
  html += tab_id;
  html += R"html(_folder_pin_enabled">
                <span>)html";
  html += tr.folder_pin_enable;
  html += R"html(</span>
              </label>
              <label class="folder-pin-label is-hidden" for=")html";
  html += tab_id;
  html += R"html(_folder_pin">)html";
  html += tr.folder_pin_label;
  html += R"html(</label>
              <div class="folder-pin-control">
                <div class="password-field folder-pin-password">
                  <input type="password" id=")html";
  html += tab_id;
  html += R"html(_folder_pin" inputmode="numeric" pattern="[0-9]{4,8}"
                       maxlength="8" autocomplete="new-password" placeholder=")html";
  html += tr.folder_pin_placeholder;
  html += R"html(">
                  <button type="button" class="password-toggle" data-label-show=")html";
  html += tr.password_show;
  html += R"html(" data-label-hide=")html";
  html += tr.password_hide;
  html += R"html(" onclick="togglePasswordVisibility(')html";
  html += tab_id;
  html += R"html(_folder_pin', this)">)html";
  html += tr.password_show;
  html += R"html(</button>
                </div>
                <button type="button" class="btn btn-secondary" id=")html";
  html += tab_id;
  html += R"html(_folder_pin_apply">)html";
  html += tr.folder_pin_apply;
  html += R"html(</button>
              </div>
              <span class="settings-note folder-pin-status" id=")html";
  html += tab_id;
  html += R"html(_folder_pin_status"></span>
            </div>
)html";
}
