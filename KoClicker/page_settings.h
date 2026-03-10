#pragma once

// Build settings HTML (all requested fields)
String buildSettingsHtml() {
  String html;
  html.reserve(4096);
  html += F("<!doctype html><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
            "<title>ESP32 Settings</title><style>"
            "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Arial,sans-serif;margin:20px;background:#111;color:#ddd;}"
            "h1{font-weight:600;margin:0 0 10px 0} .card{padding:12px 14px;border:1px solid #333;border-radius:8px;margin:10px 0;background:#191919;}"
            "label{display:block;font-size:13px;color:#aaa;margin-bottom:6px} input[type=text],input[type=password],input[type=number]{"
            "width:100%;padding:8px;border-radius:6px;border:1px solid #2b2b2b;background:#0f0f0f;color:#ddd;box-sizing:border-box}"
            ".row{display:flex;gap:8px;margin-top:10px} .btn{padding:8px 12px;border-radius:6px;border:0;cursor:pointer;font-weight:600}"
            ".btn.primary{background:#5fb3ff;color:#041827} .btn.ghost{background:transparent;border:1px solid #2b2b2b;color:#ddd}"
            ".status{font-size:13px;color:#aaa;margin-top:10px;min-height:18px} .path{font-family:ui-monospace,Menlo,Consolas,monospace;color:#aaa}"
            "a{color:#5fb3ff;text-decoration:none} a:hover{text-decoration:underline}"
            "</style>");

  html += F("<h1>Device Settings</h1>");
  html += F("<div class='card'><div><strong>Current mode: </strong> <span class='path'>");
  html += mode;
  html += F("</span></div><div><strong>Kindle IP Address:</strong> <span class='path'>");
  html += KoClickerIpAddress;
  html += F("</span></div></div>");

  html += F("<div class='card'>");
  html += F("<div><label for='shortClick'>Short click (ms)</label><input id='shortClick' type='number' min='1' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='longClick'>Long click (ms)</label><input id='longClick' type='number' min='1' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='sleepCutoff'>Sleep time cutoff (ms). Set to 0 to disable sleep. Minimum 2 minutes (120000)</label><input id='sleepCutoff' type='number' min='0' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='httpCallTimeout'>HTTP call timeout (ms)</label><input id='httpCallTimeout' type='number' min='1' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='httpConnectTimeout'>HTTP connect timeout (ms)</label><input id='httpConnectTimeout' type='number' min='1' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='delayBetweenCalls'>Delay between calls (ms)</label><input id='delayBetweenCalls' type='number' min='0' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='doubleClickMs'>Double-click window (ms)</label><input id='doubleClickMs' type='number' min='0' step='1'></div>");
  html += F("<hr style='border-color:#222;margin:12px 0'>");
  html += F("<div style='margin-top:10px'><label for='kindlhmipaddr'>Kindle Home IP address</label><input id='kindlhmipaddr' type='text' placeholder='192.168.x.x'></div>");
  html += F("<div><label for='hs_ssid'>Hotspot SSID</label><input id='hs_ssid' type='text'></div>");
  html += F("<div style='margin-top:10px'><label for='hs_password'>Hotspot password</label><input id='hs_password' type='password'></div>");
  html += F("<div style='margin-top:10px'><label for='hm_ssid'>Home SSID</label><input id='hm_ssid' type='text'></div>");
  html += F("<div style='margin-top:10px'><label for='hm_password'>Home password</label><input id='hm_password' type='password'></div>");
  html += F("<div style='margin-top:10px'><label for='ap_ssid'>AP SSID</label><input id='ap_ssid' type='text'></div>");
  html += F("<div style='margin-top:10px'><label for='ap_password'>AP password</label><input id='ap_password' type='password'></div>");
  html += F("<div style='margin-top:10px'><label for='ota_password'>OTA password</label><input id='ota_password' type='password'></div>");
  html += F("<div style='margin-top:10px'><label for='mac_filter'>MAC allow-list (comma-separated, leave empty to allow all)</label><input id='mac_filter' type='text' placeholder='AA:BB:CC:DD:EE:FF, 11:22:33:44:55:66'></div>");
  html += F("<div class='row'>"
            "<button id='saveBtn' class='btn primary'>Save</button>"
            "<button id='resetBtn' class='btn ghost'>Reset to defaults</button>"
            "</div>"
            "<div class='status' id='status'>Loading…</div>");
  html += F("</div>");  // card

  // Inline JS
  html += F("<script>"
            "function el(id){return document.getElementById(id)}"
            "async function fetchSettings(){"
            "  try{const r=await fetch('/api/settings'); if(!r.ok) throw 0; const j=await r.json();");

  // shortClick
  html += F("    el('shortClick').value = j.shortClick || ");
  html += DEF_SHORT_CLICK;
  html += F(";");

  // longClick
  html += F("    el('longClick').value = j.longClick || ");
  html += DEF_LONG_CLICK;
  html += F(";");

  // sleepCutoff
  html += F("    el('sleepCutoff').value = (j.sleepCutoff != null) ? j.sleepCutoff : ");
  html += DEF_SLEEP_CUTOFF;
  html += F(";");

  // kindle home IP
  html += F("    el('kindlhmipaddr').value = j.kindlhmipaddr || '';");

  // httpCallTimeout
  html += F("    el('httpCallTimeout').value = j.httpCallTimeout || ");
  html += DEF_HTTP_CALL_TIMEOUT;
  html += F(";");

  // httpConnectTimeout
  html += F("    el('httpConnectTimeout').value = j.httpConnectTimeout || ");
  html += DEF_HTTP_CONNECT_TIMEOUT;
  html += F(";");

  // delayBetweenCalls
  html += F("    el('delayBetweenCalls').value = j.delayBetweenCalls || ");
  html += DEF_DELAY_BETWEEN_CALLS;
  html += F(";");

  // doubleClickMs
  html += F("    el('doubleClickMs').value = j.doubleClickMs || ");
  html += DEF_DOUBLE_CLICK_MS;
  html += F(";");

  // hotspot SSID
  html += F("    el('hs_ssid').value = j.hs_ssid || '';");

  // hotspot password
  html += F("    el('hs_password').value = j.hs_password || '';");

  // home SSID
  html += F("    el('hm_ssid').value = j.hm_ssid || '';");

  // home password
  html += F("    el('hm_password').value = j.hm_password || '';");

  // AP SSID
  html += F("    el('ap_ssid').value = j.ap_ssid || \"");
  html += DEF_AP_SSID;
  html += F("\";");

  // AP password
  html += F("    el('ap_password').value = j.ap_password || \"");
  html += DEF_AP_PASS;
  html += F("\";");

  // OTA password
  html += F("    el('ota_password').value = j.ota_password || \"");
  html += DEF_OTA_PASS;
  html += F("\";");

  // MAC filter
  html += F("    el('mac_filter').value = j.mac_filter || '';");

  html += F("    el('status').textContent = 'Loaded.';"
            "  }catch(e){ el('status').textContent = 'Could not load settings.'; }}");

  // --- saveSettings() ---
  html += F("async function saveSettings(){"
            "  const payload = {");

  // shortClick
  html += F("    shortClick: parseInt(el('shortClick').value) || ");
  html += DEF_SHORT_CLICK;
  html += F(",");

  // longClick
  html += F("    longClick: parseInt(el('longClick').value) || ");
  html += DEF_LONG_CLICK;
  html += F(",");

  // sleepCutoff
  html += F("    sleepCutoff: parseInt(el('sleepCutoff').value, 10),");

  // kindle home IP
  html += F("    kindlhmipaddr: el('kindlhmipaddr').value || '',");

  // httpCallTimeout
  html += F("    httpCallTimeout: parseInt(el('httpCallTimeout').value) || ");
  html += DEF_HTTP_CALL_TIMEOUT;
  html += F(",");

  // httpConnectTimeout
  html += F("    httpConnectTimeout: parseInt(el('httpConnectTimeout').value) || ");
  html += DEF_HTTP_CONNECT_TIMEOUT;
  html += F(",");

  // delayBetweenCalls
  html += F("    delayBetweenCalls: parseInt(el('delayBetweenCalls').value) || ");
  html += DEF_DELAY_BETWEEN_CALLS;
  html += F(",");

  // doubleClickMs
  html += F("    doubleClickMs: parseInt(el('doubleClickMs').value) || ");
  html += DEF_DOUBLE_CLICK_MS;
  html += F(",");

  // SSIDs and passwords
  html += F("    hs_ssid: el('hs_ssid').value || '', hs_password: el('hs_password').value || '',"
            "    hm_ssid: el('hm_ssid').value || '', hm_password: el('hm_password').value || '',"
            "    ap_ssid: el('ap_ssid').value || '', ap_password: el('ap_password').value || '',"
            "    ota_password: el('ota_password').value || '',"
            "    mac_filter: el('mac_filter').value || ''"
            "  };"
            "  el('status').textContent = 'Saving…';"
            "  try{const r = await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});"
            "    if(!r.ok) throw 0; const j = await r.json();"
            "    el('status').textContent = j.message || 'Saved.';"
            "  }catch(e){ el('status').textContent = 'Save failed.'; }}");

  // --- resetDefaults() ---
  html += F("async function resetDefaults(){"
            "  if(!confirm('Reset saved settings to defaults?')) return;"
            "  el('status').textContent = 'Resetting…';"
            "  try{const r = await fetch('/api/reset',{method:'POST'}); if(!r.ok) throw 0; await fetchSettings(); }catch(e){ el('status').textContent = 'Reset failed.'; }}");

  // --- event listeners ---
  html += F("document.getElementById('saveBtn').addEventListener('click', saveSettings);"
            "document.getElementById('resetBtn').addEventListener('click', resetDefaults);"
            "window.addEventListener('load', fetchSettings);"
            "</script>");

  html += F("<div style='margin-top:8px;font-size:12px;color:#aaa'>Back to <a href='/'>index</a></div>");
  return html;
}
