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
  html += R"html(_navigate_fields" class="type-fields">
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
              </select>
            </div>
)html";
}
