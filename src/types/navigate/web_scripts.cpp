#include "src/types/navigate/web_scripts.h"

void append_navigate_scripts(String& html) {
  html += R"html(
  <script>
  function normalizeIconName(value) {
    let icon = String(value || '').trim().toLowerCase();
    if (icon.startsWith('mdi:')) icon = icon.substring(4);
    else if (icon.startsWith('mdi-')) icon = icon.substring(4);
    return icon;
  }
  function formatFolderLabel(name, folderId) {
    let label = String(name || '').trim();
    if (!label.length) label = t('folderPrefix') + folderId;
    return label;
  }
  function updateFolderTabUi(folderId, name, icon) {
    if (folderId === undefined || folderId === null) return;
    const folderNum = parseInt(folderId, 10);
    if (isNaN(folderNum)) return;
    const label = formatFolderLabel(name, folderNum);
    const iconName = normalizeIconName(icon);
    const tabId = tabByFolder[folderNum];
    if (tabId) {
      const tabEl = document.getElementById('tab-tiles-' + tabId);
      if (tabEl) {
        tabEl.dataset.folderName = label;
        tabEl.dataset.folderIcon = iconName;
      }
      const btns = Array.from(document.querySelectorAll('.tab-btn'));
      const btn = btns.find(b => b.getAttribute('onclick')?.includes('tab-tiles-' + tabId));
      if (btn) {
        const labelEl = btn.querySelector('span');
        if (labelEl) labelEl.textContent = label;
        let iconEl = btn.querySelector('i.mdi');
        if (iconName) {
          if (!iconEl) {
            iconEl = document.createElement('i');
            iconEl.className = 'mdi';
            iconEl.style.fontSize = '24px';
            if (labelEl) btn.insertBefore(iconEl, labelEl);
            else btn.appendChild(iconEl);
          }
          iconEl.className = 'mdi mdi-' + iconName;
          iconEl.style.fontSize = '24px';
        } else if (iconEl) {
          iconEl.remove();
        }
      }
    }
    document.querySelectorAll('select[id$="_navigate_target"]').forEach(select => {
      const opt = select.querySelector('option[value="' + folderNum + '"]');
      if (opt) opt.textContent = label;
    });
  }

  function loadNavigateFields(tab, data) {
    const prefix = tab;
    const entEl = document.getElementById(prefix + '_navigate_sensor_entity');
    if (entEl) entEl.value = (data && data.sensor_entity) ? data.sensor_entity : '';
    const decEl = document.getElementById(prefix + '_navigate_sensor_decimals');
    if (decEl) {
      const dec = (data && data.sensor_decimals !== undefined && data.sensor_decimals !== null)
        ? Number(data.sensor_decimals) : -1;
      decEl.value = (dec >= 0 && dec <= 6) ? dec : '';
    }
    const fontEl = document.getElementById(prefix + '_navigate_sensor_value_font');
    if (fontEl) fontEl.value = String((data && data.sensor_value_font) || '0');
  }

  function saveNavigateFields(tab, formData) {
    const prefix = tab;
    const navEl = document.getElementById(prefix + '_navigate_target');
    if (navEl) {
      formData.append('navigate_target', navEl.value || '0');
    }
    // Optionaler Live-Wert. Immer mitsenden, damit das Leeren der Auswahl den
    // Wert auch wirklich entfernt (leerer String -> Feld wird zurueckgesetzt).
    const entEl = document.getElementById(prefix + '_navigate_sensor_entity');
    if (entEl) {
      formData.append('sensor_entity', entEl.value || '');
      const decEl = document.getElementById(prefix + '_navigate_sensor_decimals');
      const decRaw = decEl ? String(decEl.value).trim() : '';
      formData.append('sensor_decimals', decRaw.length ? decRaw : '-1');
      const fontEl = document.getElementById(prefix + '_navigate_sensor_value_font');
      formData.append('sensor_value_font', fontEl ? (fontEl.value || '0') : '0');
    }
  }

  function resetNavigateFields(tab) {
    const prefix = tab;
    const entEl = document.getElementById(prefix + '_navigate_sensor_entity');
    if (entEl) entEl.value = '';
    const decEl = document.getElementById(prefix + '_navigate_sensor_decimals');
    if (decEl) decEl.value = '';
    const fontEl = document.getElementById(prefix + '_navigate_sensor_value_font');
    if (fontEl) fontEl.value = '0';
  }
  </script>
)html";
}
