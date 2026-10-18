#pragma once

#include <Arduino.h>
#include <vector>

void append_navigate_fields_html(String& html,
                                 const String& tab_id,
                                 const String& navigateOptionsHtml);
void append_back_fields_html(String& html, const String& tab_id);
