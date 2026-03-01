/*
TO DO SECTION

1. Add description in webpage index
2. Link to github
3. Copyright
4. Add page with settings
5. Add option to enable/disable sleep timer

BUGS
1. logLinenl("%s", WiFi.localIP().toString().c_str()); prints 0.0.0.0 in AP use Serial.println(WiFi.softAPIP());
2. Change mode prints number

*/
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include <ArduinoJson.h>
//#include <arduino_secrets.h>
//#include <settings_Html.h>
//#include <console_Html.h>
//#include <index_Html.h>

String KoClickerVersion = "v0.0.1";

// Hardware settings
#define BUTTON 48
#define BUTTON_MODE INPUT_PULLUP

#define ON LOW
#define OFF HIGH

#define RED_LED 47
#define GREEN_LED 46
#define BLUE_LED 45

AsyncWebServer server(80);
AsyncWebSocket ws("/console-ws");  // WebSocket endpoint for the console
Preferences prefs;

//------------------------------ USER SECTION ------------------------------//
//Wi-Fi config for Phone Hot-Spot

//Wi-Fi settings will load from arduino_secrets.h
//Rename the "arduino_secrets.h.example" file to "arduino_secrets.h" and provide the values in that file

String ipAddress = "192.168.10.155";  // Home network IP Address of your Kindle
int http_CallTimeout = 750;           // HTTP web calls timeout
int http_ConnectTimeout = 750;        // HTTP Server connect timeout
int delayBetweenCalls = 400;          // A delay between change page and keep alive calls

// Preference namespace and keys
const char* PREF_NAMESPACE = "settings";
const char* K_SHORT_CLICK = "shortClick";
const char* K_LONG_CLICK = "longClick";
const char* K_SLEEP_CUTOFF = "sleepCutoff";
const char* K_IP_ADDRESS = "ipAddress";
const char* K_HTTP_CALL_TIMEOUT = "httpCallTO";
const char* K_HTTP_CONNECT_TIMEOUT = "httpConnTO";
const char* K_DELAY_BETWEEN = "delayBetween";
const char* K_HS_SSID = "hs_ssid";
const char* K_HS_PASS = "hs_pass";
const char* K_HM_SSID = "hm_ssid";
const char* K_HM_PASS = "hm_pass";
const char* K_AP_SSID = "ap_ssid";
const char* K_AP_PASS = "ap_pass";
const char* K_OTA_PASS = "ota_pass";

// Defaults
const uint32_t DEF_SHORT_CLICK = 500;
const uint32_t DEF_LONG_CLICK = 5 * 1000;
const uint32_t DEF_SLEEP_CUTOFF = 5 * 60 * 1000;
const char* DEF_IP_ADDRESS = "192.168.10.155";
const int DEF_HTTP_CALL_TIMEOUT = 750;
const int DEF_HTTP_CONNECT_TIMEOUT = 750;
const int DEF_DELAY_BETWEEN_CALLS = 400;
const char* DEF_HS_SSID = "hotspotssid";
const char* DEF_HS_PASS = "hotspotpassword";
const char* DEF_HM_SSID = "homessid";
const char* DEF_HM_PASS = "homepassword";
const char* DEF_AP_SSID = "KoClicker";
const char* DEF_AP_PASS = "12345678";
const char* DEF_OTA_PASS = "1234";

// Runtime variables (used by your application)
uint32_t shortClick = DEF_SHORT_CLICK;
uint32_t longClick = DEF_LONG_CLICK;
uint32_t sleepCutoff = DEF_SLEEP_CUTOFF;
String kindleIp = String(DEF_IP_ADDRESS);
int httpCallTimeout = DEF_HTTP_CALL_TIMEOUT;
int httpConnectTimeout = DEF_HTTP_CONNECT_TIMEOUT;
// delayBetweenCalls is already defined above
String hsSsid = String(DEF_HS_SSID);
String hsPass = String(DEF_HS_PASS);
String hmSsid = String(DEF_HM_SSID);
String hmPass = String(DEF_HM_PASS);
String apSsid = String(DEF_AP_SSID);
String apPass = String(DEF_AP_PASS);
String otaPassword = String(DEF_OTA_PASS);

//---------------------------------------------------------------------------//

String mode;
unsigned long sleep_time_reset;
bool connected;


// ====== Console page (/console) with timestamps, auto-scroll, pause, scroll-to-bottom======
const char PAGE_CONSOLE[] PROGMEM = R"HTML(
<!doctype html>
<meta charset="utf-8">
<title>ESP32 Console</title>
<style>
  body { margin:0; font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; background:#111; color:#eee; }
  #bar { display:flex; align-items:center; gap:12px; padding:8px 10px; background:#1b1b1b; font-size:14px; flex-wrap: wrap; }
  #out { white-space:pre-wrap; height:72vh; overflow:auto; padding:10px; border-top:1px solid #2a2a2a; border-bottom:1px solid #2a2a2a; }
  #in  { width:100%; box-sizing:border-box; padding:10px; border:none; outline:none; background:#222; color:#fff; }
  label { user-select:none; }
  a { color:#5fb3ff; text-decoration:none; }
  a:hover { text-decoration:underline; }
  .sep { opacity:.3; }
  .btn { padding:6px 10px; background:#2b2b2b; color:#eee; border:1px solid #444; border-radius:6px; cursor:pointer; }
  #scrollBottom { display:none; }
  #status.badge { padding:2px 8px; border:1px solid #444; border-radius:999px; font-size:12px; }
  #buf.badge    { padding:2px 8px; border:1px solid #444; border-radius:999px; font-size:12px; background:#2b2b2b; display:none; }
  #buf.warn     { border-color:#a66; color:#f99; }
</style>

<div id="bar">
  <a href="/">index</a>
  <span class="sep">|</span>
  <label><input type="checkbox" id="ts"> Timestamps</label>
  <label><input type="checkbox" id="autoscroll"> Auto-scroll</label>
  <label><input type="checkbox" id="pause"> Pause</label>
  <button id="scrollBottom" class="btn">Scroll to bottom</button>
  <button id="clear" class="btn">Clear</button>
  <span id="status" class="badge">Connecting...</span>
  <span id="buf" class="badge"></span>
</div>

<pre id="out"></pre>
<input id="in" placeholder="type command + Enter">

<script>
// --- DOM ---
const out    = document.getElementById("out");
const inp    = document.getElementById("in");
const stat   = document.getElementById("status");
const chkTs  = document.getElementById("ts");
const chkAS  = document.getElementById("autoscroll");
const chkP   = document.getElementById("pause");
const btnCl  = document.getElementById("clear");
const btnSB  = document.getElementById("scrollBottom");
const bufUI  = document.getElementById("buf");

// --- Settings (persist) ---
chkTs.checked = localStorage.getItem("console_ts") === "1";
chkAS.checked = localStorage.getItem("console_autoscroll") !== "0"; // default ON
chkP.checked  = localStorage.getItem("console_pause") === "1";

chkTs.addEventListener("change", () => localStorage.setItem("console_ts", chkTs.checked ? "1" : "0"));
chkAS.addEventListener("change", () => { localStorage.setItem("console_autoscroll", chkAS.checked ? "1" : "0"); updateScrollBtnVisibility(); });
chkP.addEventListener("change",  () => { localStorage.setItem("console_pause", chkP.checked ? "1" : "0"); if (!chkP.checked) flushPausedBuffer(); updateScrollBtnVisibility(); updateBufStatus(); });

// --- State ---
let lineStart = true; // start of logical line?
let ws;

// --- Pause buffer (FIFO) ---
let pausedBuffer = [];      // array of strings
let pausedBytes = 0;       // total bytes buffered
let trimmedBytesTotal = 0;
let trimmedChunks = 0;
// Production cap ~200000 (≈200 KB). Lower for quicker demo of trimming.
const PAUSE_MAX_BYTES = 200000;

// ---------- Utils ----------
function kb(n) { return (n/1024).toFixed(1); }
function isAtBottom() { return (out.scrollTop + out.clientHeight) >= (out.scrollHeight - 2); }
function scrollToBottom() { out.scrollTop = out.scrollHeight; }
function updateScrollBtnVisibility() {
  btnSB.style.display = (!chkAS.checked && !isAtBottom()) ? "inline-block" : "none";
}

function tsNow() {
  const d = new Date();
  const pad = (n, w=2) => String(n).padStart(w, "0");
  return `${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}.${pad(d.getMilliseconds(),3)}`;
}

function updateBufStatus() {
  if (!chkP.checked) {
    bufUI.style.display = "none";
    bufUI.classList.remove("warn");
    return;
  }

  bufUI.style.display = "inline-block";
  bufUI.textContent = `Paused - buffering ${kb(pausedBytes)} KB, trimmed ${kb(trimmedBytesTotal)} KB (${trimmedChunks} drops)`;
  if (trimmedBytesTotal > 0) bufUI.classList.add("warn"); else bufUI.classList.remove("warn");
}

// ---------- Pause buffer ops ----------
function bufferWhilePaused(data) {
  pausedBuffer.push(data);
  pausedBytes += data.length;
  while (pausedBytes > PAUSE_MAX_BYTES && pausedBuffer.length > 0) {
    const dropped = pausedBuffer.shift();
    pausedBytes -= dropped.length;
    trimmedBytesTotal += dropped.length;
    trimmedChunks++;
  }

  updateBufStatus();
}
function flushPausedBuffer() {
  if (pausedBuffer.length === 0) return;
  const combined = pausedBuffer.join("");
  pausedBuffer = [];
  pausedBytes = 0;
  appendTextRender(combined);
  updateBufStatus();
}

// ---------- Rendering ----------
function appendTextRender(data) {
  const endsWithNL = data.endsWith("\n");
  const parts = data.split("\n");

  for (let i = 0; i < parts.length; i++) {
    const seg = parts[i];
    if (seg.length > 0) {
      if (chkTs.checked && lineStart) out.textContent += `[${tsNow()}] `;
      out.textContent += seg;
      lineStart = false;
    }
    if (i < parts.length - 1) {
      out.textContent += "\n";
      lineStart = true;
    }
  }
  if (endsWithNL) lineStart = true;

  if (chkAS.checked) scrollToBottom(); else updateScrollBtnVisibility();

  // Trim console to keep DOM fast
  if (out.textContent.length > 200000) out.textContent = out.textContent.slice(-150000);
}

function incoming(data) {
  // Called by WebSocket onmessage
  if (chkP.checked) bufferWhilePaused(data);
  else appendTextRender(data);
}
// ---------- WebSocket wiring ----------
function connect() {
  ws = new WebSocket(`ws://${location.host}/console-ws`);
  ws.onopen = () => { stat.textContent = "Connected"; };
  ws.onclose = () => { stat.textContent = "Disconnected. Reconnecting..."; setTimeout(connect, 1000); };
  ws.onerror = () => { stat.textContent = "Error"; };
  ws.onmessage = (e) => { incoming(e.data); };
}
connect();

// ---------- UI events ----------
inp.addEventListener("keydown", e => {
  if (e.key === "Enter" && ws.readyState === WebSocket.OPEN) {
    ws.send(inp.value);
    inp.value = "";
  }
});

btnCl.onclick = () => { out.textContent = ""; lineStart = true; updateScrollBtnVisibility(); };
btnSB.onclick = () => { scrollToBottom(); updateScrollBtnVisibility(); };
out.addEventListener("scroll", () => { if (!chkAS.checked) updateScrollBtnVisibility(); });
window.addEventListener("resize", updateScrollBtnVisibility);

// Initial UI state
updateScrollBtnVisibility();
updateBufStatus();
</script>
)HTML";


// ====== Index page ("/") helper ======
struct PageLink {
  const char* path;
  const char* title;
  const char* desc;
};
PageLink PAGES[] = {
  { "/settings", "Settings & configuration for KoClicker" },
  { "/console", "Console", "Interactive logs & commands (WebSocket)" },
  { "/update", "Firmware Update", "Elegant OTA Firmware update page" },
  // Add more entries here as you add pages:
  // { "/status", "Status", "Device and sensor status" },
  // { "/api/info", "API: Info", "Basic device info in JSON" },
};
const size_t PAGES_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);

String buildIndexHtml() {
  String html;
  html.reserve(1024);
  html += F("<!doctype html><meta charset='utf-8'><title>KoClicker Index</title><style>"
            "body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu,Arial,sans-serif;margin:20px;background:#111;color:#ddd;}"
            "h1{font-weight:600;} .card{padding:12px 14px;border:1px solid #333;border-radius:8px;margin:10px 0;background:#191919;}"
            "a{color:#5fb3ff;text-decoration:none;} a:hover{text-decoration:underline;}"
            ".path{font-family:ui-monospace,Menlo,Consolas,monospace;color:#aaa;}"
            "</style>");
  html += F("<h1>KoClicker");
  html += F(KoClickerVersion);
  html += F("</h1>");
  html += F("<div class='card'><div><strong>IP:</strong> <span class='path'>");
  html += WiFi.localIP().toString();
  html += F("</span></div></div>");
  for (size_t i = 0; i < PAGES_COUNT; ++i) {
    html += F("<div class='card'><div><a href='");
    html += PAGES[i].path;
    html += F("'>");
    html += PAGES[i].title;
    html += F("</a> <span class='path'>");
    html += PAGES[i].path;
    html += F("</span></div><div>");
    html += PAGES[i].desc;
    html += F("</div></div>");
  }
  return html;
}

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
  html += F("<div class='card'><div><strong>IP:</strong> <span class='path'>");
  html += WiFi.localIP().toString();
  html += F("</span></div></div>");

  html += F("<div class='card'>");
  html += F("<div><label for='shortClick'>Short click (ms)</label><input id='shortClick' type='number' min='1' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='longClick'>Long click (ms)</label><input id='longClick' type='number' min='1' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='sleepCutoff'>Sleep time cutoff (ms)</label><input id='sleepCutoff' type='number' min='1' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='ipAddress'>Kindle IP address</label><input id='ipAddress' type='text' placeholder='192.168.x.x'></div>");
  html += F("<div style='margin-top:10px'><label for='httpCallTimeout'>HTTP call timeout (ms)</label><input id='httpCallTimeout' type='number' min='1' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='httpConnectTimeout'>HTTP connect timeout (ms)</label><input id='httpConnectTimeout' type='number' min='1' step='1'></div>");
  html += F("<div style='margin-top:10px'><label for='delayBetweenCalls'>Delay between calls (ms)</label><input id='delayBetweenCalls' type='number' min='0' step='1'></div>");
  html += F("<hr style='border-color:#222;margin:12px 0'>");
  html += F("<div><label for='hs_ssid'>Hotspot SSID</label><input id='hs_ssid' type='text'></div>");
  html += F("<div style='margin-top:10px'><label for='hs_password'>Hotspot password</label><input id='hs_password' type='password'></div>");
  html += F("<div style='margin-top:10px'><label for='hm_ssid'>Home SSID</label><input id='hm_ssid' type='text'></div>");
  html += F("<div style='margin-top:10px'><label for='hm_password'>Home password</label><input id='hm_password' type='password'></div>");
  html += F("<div style='margin-top:10px'><label for='ap_ssid'>AP SSID</label><input id='ap_ssid' type='text'></div>");
  html += F("<div style='margin-top:10px'><label for='ap_password'>AP password</label><input id='ap_password' type='password'></div>");
  html += F("<div style='margin-top:10px'><label for='ota_password'>OTA password</label><input id='ota_password' type='password'></div>");
  html += F("<div class='row'>"
            "<button id='saveBtn' class='btn primary'>Save</button>"
            "<button id='resetBtn' class='btn ghost'>Reset to defaults</button>"
            "</div>"
            "<div class='status' id='status'>Loading…</div>");
  html += F("</div>"); // card

  // Inline JS
  html += F("<script>"
            "function el(id){return document.getElementById(id)}"
            "async function fetchSettings(){"
            "  try{const r=await fetch('/api/settings'); if(!r.ok) throw 0; const j=await r.json();"
            "    el('shortClick').value = j.shortClick || 500;"
            "    el('longClick').value = j.longClick || 5000;"
            "    el('sleepCutoff').value = j.sleepCutoff || 300000;"
            "    el('ipAddress').value = j.ipAddress || '192.168.10.155';"
            "    el('httpCallTimeout').value = j.httpCallTimeout || 750;"
            "    el('httpConnectTimeout').value = j.httpConnectTimeout || 750;"
            "    el('delayBetweenCalls').value = j.delayBetweenCalls || 400;"
            "    el('hs_ssid').value = j.hs_ssid || ''; el('hs_password').value = j.hs_password || '';"
            "    el('hm_ssid').value = j.hm_ssid || ''; el('hm_password').value = j.hm_password || '';"
            "    el('ap_ssid').value = j.ap_ssid || ''; el('ap_password').value = j.ap_password || '';"
            "    el('ota_password').value = j.ota_password || '';"
            "    el('status').textContent = 'Loaded.';"
            "  }catch(e){ el('status').textContent = 'Could not load settings.'; }"
            "}"
            "async function saveSettings(){"
            "  const payload = {"
            "    shortClick: parseInt(el('shortClick').value)||500,"
            "    longClick: parseInt(el('longClick').value)||5000,"
            "    sleepCutoff: parseInt(el('sleepCutoff').value)||300000,"
            "    ipAddress: el('ipAddress').value||'',"
            "    httpCallTimeout: parseInt(el('httpCallTimeout').value)||750,"
            "    httpConnectTimeout: parseInt(el('httpConnectTimeout').value)||750,"
            "    delayBetweenCalls: parseInt(el('delayBetweenCalls').value)||400,"
            "    hs_ssid: el('hs_ssid').value||'', hs_password: el('hs_password').value||'',"
            "    hm_ssid: el('hm_ssid').value||'', hm_password: el('hm_password').value||'',"
            "    ap_ssid: el('ap_ssid').value||'', ap_password: el('ap_password').value||'',"
            "    ota_password: el('ota_password').value||''"
            "  };"
            "  el('status').textContent = 'Saving…';"
            "  try{const r = await fetch('/api/settings',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(payload)});"
            "    if(!r.ok) throw 0; const j = await r.json();"
            "    el('status').textContent = j.message || 'Saved.';"
            "  }catch(e){ el('status').textContent = 'Save failed.'; }"
            "}"
            "async function resetDefaults(){"
            "  if(!confirm('Reset saved settings to defaults?')) return;"
            "  el('status').textContent = 'Resetting…';"
            "  try{const r = await fetch('/api/reset',{method:'POST'}); if(!r.ok) throw 0; await fetchSettings(); }catch(e){ el('status').textContent = 'Reset failed.'; }"
            "}"
            "document.getElementById('saveBtn').addEventListener('click', saveSettings);"
            "document.getElementById('resetBtn').addEventListener('click', resetDefaults);"
            "window.addEventListener('load', fetchSettings);"
            "</script>");

  html += F("<div style='margin-top:8px;font-size:12px;color:#aaa'>Back to <a href='/'>index</a></div>");
  return html;
}

// API: GET /api/settings
void handleGetSettings(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(1024);
  doc["shortClick"] = prefs.getUInt(K_SHORT_CLICK, DEF_SHORT_CLICK);
  doc["longClick"] = prefs.getUInt(K_LONG_CLICK, DEF_LONG_CLICK);
  doc["sleepCutoff"] = prefs.getUInt(K_SLEEP_CUTOFF, DEF_SLEEP_CUTOFF);
  doc["ipAddress"] = prefs.getString(K_IP_ADDRESS, DEF_IP_ADDRESS);
  doc["httpCallTimeout"] = prefs.getInt(K_HTTP_CALL_TIMEOUT, DEF_HTTP_CALL_TIMEOUT);
  doc["httpConnectTimeout"] = prefs.getInt(K_HTTP_CONNECT_TIMEOUT, DEF_HTTP_CONNECT_TIMEOUT);
  doc["delayBetweenCalls"] = prefs.getInt(K_DELAY_BETWEEN, DEF_DELAY_BETWEEN_CALLS);
  doc["hs_ssid"] = prefs.getString(K_HS_SSID, DEF_HS_SSID);
  doc["hs_password"] = prefs.getString(K_HS_PASS, DEF_HS_PASS);
  doc["hm_ssid"] = prefs.getString(K_HM_SSID, DEF_HM_SSID);
  doc["hm_password"] = prefs.getString(K_HM_PASS, DEF_HM_PASS);
  doc["ap_ssid"] = prefs.getString(K_AP_SSID, DEF_AP_SSID);
  doc["ap_password"] = prefs.getString(K_AP_PASS, DEF_AP_PASS);
  doc["ota_password"] = prefs.getString(K_OTA_PASS, DEF_OTA_PASS);

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

// API: POST /api/settings
// Saves preferences
void handlePostSettings(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
    return;
  }

  // Store into Preferences
  if (doc.containsKey("shortClick")) prefs.putUInt(K_SHORT_CLICK, doc["shortClick"]);
  if (doc.containsKey("longClick")) prefs.putUInt(K_LONG_CLICK, doc["longClick"]);
  if (doc.containsKey("sleepCutoff")) prefs.putUInt(K_SLEEP_CUTOFF, doc["sleepCutoff"]);
  if (doc.containsKey("ipAddress")) prefs.putString(K_IP_ADDRESS, doc["ipAddress"].as<String>());
  if (doc.containsKey("httpCallTimeout")) prefs.putInt(K_HTTP_CALL_TIMEOUT, doc["httpCallTimeout"]);
  if (doc.containsKey("httpConnectTimeout")) prefs.putInt(K_HTTP_CONNECT_TIMEOUT, doc["httpConnectTimeout"]);
  if (doc.containsKey("delayBetweenCalls")) prefs.putInt(K_DELAY_BETWEEN, doc["delayBetweenCalls"]);

  if (doc.containsKey("hs_ssid")) prefs.putString(K_HS_SSID, doc["hs_ssid"].as<String>());
  if (doc.containsKey("hs_password")) prefs.putString(K_HS_PASS, doc["hs_password"].as<String>());
  if (doc.containsKey("hm_ssid")) prefs.putString(K_HM_SSID, doc["hm_ssid"].as<String>());
  if (doc.containsKey("hm_password")) prefs.putString(K_HM_PASS, doc["hm_password"].as<String>());
  if (doc.containsKey("ap_ssid")) prefs.putString(K_AP_SSID, doc["ap_ssid"].as<String>());
  if (doc.containsKey("ap_password")) prefs.putString(K_AP_PASS, doc["ap_password"].as<String>());
  if (doc.containsKey("ota_password")) prefs.putString(K_OTA_PASS, doc["ota_password"].as<String>());

  // Reload runtime variables
  loadPreferences();

  request->send(200, "application/json", "{\"ok\":true,\"message\":\"Saved.\"}");
}

// API: POST /api/reset
void handleReset(AsyncWebServerRequest *request) {
  prefs.clear(); // clears all keys in this namespace
  request->send(200, "application/json", "{\"ok\":true,\"message\":\"Preferences cleared. Restarting AP...\"}");
  delay(500);
  WiFi.softAP(DEF_AP_SSID, DEF_AP_PASS);
}


void startupAnimation() {
  for (int i = 1; i < 10; i++) {
    digitalWrite(RED_LED, ON);
    delay(100);
    digitalWrite(RED_LED, OFF);
    digitalWrite(GREEN_LED, ON);
    delay(100);
    digitalWrite(GREEN_LED, OFF);
    digitalWrite(BLUE_LED, ON);
    delay(100);
    digitalWrite(BLUE_LED, OFF);
  }
}

void modeAnimation(unsigned char colour) {
  digitalWrite(colour, ON);
  delay(3000);
  digitalWrite(colour, OFF);
}

void sleep() {
  logLinenl("Going to sleep... Goodnight!");
  for (int i = 1; i < 10; i++) {
    digitalWrite(RED_LED, ON);
    delay(200);
    digitalWrite(RED_LED, OFF);
    delay(200);
  }

  digitalWrite(RED_LED, ON);
  digitalWrite(GREEN_LED, ON);
  digitalWrite(BLUE_LED, ON);

  esp_deep_sleep_start();
}

void switchMode() {
  String newMode;

  if (mode == "HotSpot") {
    newMode = "Home";
    modeAnimation(BLUE_LED);
  } else if (mode == "Home") {
    newMode = "AccessPoint";
    modeAnimation(RED_LED);
  } else {
    newMode = "HotSpot";
    modeAnimation(GREEN_LED);
  }

  logLinenl("Changing Mode to %s", newMode.c_str());
  prefs.putString("mode", newMode);
  logLinenl("Resetting board...Bye bye!");

  WiFi.disconnect();
  delay(3000);
  ESP.restart();
}

void pageTurn(int direction, int waitTime) {
  String url;
  if (direction == 1) {
    logLinenl("Next page");
    url = "event/GotoViewRel/1";
  } else {
    logLinenl("Previous page");
    url = "event/GotoViewRel/-1";
  }

  bool success = webCall(url, waitTime);
  if (success) {
    delay(delayBetweenCalls);
    webCall("broadcast/InputEvent", waitTime);
  }
}

bool webCall(String function, int waitTime) {
  bool success = false;
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(httpCallTimeout);
  http.setConnectTimeout(httpConnectTimeout);

  String serverName = "http://" + ipAddress + ":8080/koreader/" + function;
  http.begin(client, serverName.c_str());
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    logLine("%d", httpResponseCode);
    success = true;
    digitalWrite(GREEN_LED, ON);
    delay(waitTime);
    digitalWrite(GREEN_LED, OFF);
  } else {
    logLine("Error code: ");
    logLine("%d", httpResponseCode);
    digitalWrite(RED_LED, ON);
    delay(waitTime);
    digitalWrite(RED_LED, OFF);
  }
  logLine(" - ");
  logLinenl("%s", serverName.c_str());

  http.end();
  return success;
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.print("Station connected! MAC: ");
      // The MAC address is available immediately upon connection
      for (int i = 0; i < 6; i++) {
        Serial.printf("%02X%c", info.wifi_ap_staconnected.mac[i], i < 5 ? ':' : '\n');
      }
      break;

    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
      Serial.print("IP Assigned to station: ");
      // The assigned IP address is inside the event info structure
      Serial.println(IPAddress(info.wifi_ap_staipassigned.ip.addr));
      ipAddress = IPAddress(info.wifi_ap_staipassigned.ip.addr).toString();
      digitalWrite(RED_LED, ON);
      digitalWrite(GREEN_LED, ON);
      digitalWrite(BLUE_LED, ON);
      delay(400);
      digitalWrite(RED_LED, OFF);
      digitalWrite(GREEN_LED, OFF);
      digitalWrite(BLUE_LED, OFF);
      delay(200);
      digitalWrite(RED_LED, ON);
      digitalWrite(GREEN_LED, ON);
      digitalWrite(BLUE_LED, ON);
      delay(400);
      digitalWrite(RED_LED, OFF);
      digitalWrite(GREEN_LED, OFF);
      digitalWrite(BLUE_LED, OFF);
      delay(200);
      break;

    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.println("Station disconnected.");
      break;

    default:
      break;
  }
}

// ====== Logging helpers ======
// Send text exactly as-is (NO newline)
void logLine(const char* format, ...) {
  char loc_res[256];
  va_list arg;
  va_start(arg, format);
  vsnprintf(loc_res, sizeof(loc_res), format, arg);
  va_end(arg);

  String s = String(loc_res);
  ws.textAll(s);
  Serial.print(s);
}

// Send text + newline
void logLinenl(const char* format, ...) {
  char loc_res[256];
  va_list arg;
  va_start(arg, format);
  vsnprintf(loc_res, sizeof(loc_res), format, arg);
  va_end(arg);

  String s = String(loc_res);
  ws.textAll(s + "\n");
  Serial.println(s);
}

// ====== Command handler ======
void handleCommand(AsyncWebSocketClient* client, String cmd) {
  cmd.trim();
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") {
    client->text(
      "Commands:\n"
      "  help    - show this help\n"
      "  uptime  - show uptime (ms)\n"
      "  heap    - show free heap (bytes)\n");
  } else if (cmd == "uptime") {
    client->text(String("uptime_ms=") + millis() + "\n");
  } else if (cmd == "heap") {
    client->text(String("free_heap=") + ESP.getFreeHeap() + "\n");
  } else if (cmd.length() == 0) {
    // ignore blank
  } else {
    client->text("Unknown command. Type 'help'\n");
  }

  client->text("> ");
}

// ====== WebSocket events ======
void onWsEvent(
  AsyncWebSocket* serverPtr,
  AsyncWebSocketClient* client,
  AwsEventType type,
  void* arg,
  uint8_t* data,
  size_t len) {
  if (type == WS_EVT_CONNECT) {
    client->text("ESP32 console connected\n ");
  } else if (type == WS_EVT_DATA) {
    String cmd;
    cmd.reserve(len);
    for (size_t i = 0; i < len; i++) cmd += (char)data[i];
    handleCommand(client, cmd);
  }
}

// Load preferences into runtime variables
void loadPreferences() {
  shortClick = prefs.getUInt(K_SHORT_CLICK, DEF_SHORT_CLICK);
  longClick = prefs.getUInt(K_LONG_CLICK, DEF_LONG_CLICK);
  sleepCutoff = prefs.getUInt(K_SLEEP_CUTOFF, DEF_SLEEP_CUTOFF);
  kindleIp = prefs.getString(K_IP_ADDRESS, DEF_IP_ADDRESS);
  httpCallTimeout = prefs.getInt(K_HTTP_CALL_TIMEOUT, DEF_HTTP_CALL_TIMEOUT);
  httpConnectTimeout = prefs.getInt(K_HTTP_CONNECT_TIMEOUT, DEF_HTTP_CONNECT_TIMEOUT);
  delayBetweenCalls = prefs.getInt(K_DELAY_BETWEEN, DEF_DELAY_BETWEEN_CALLS);
  hsSsid = prefs.getString(K_HS_SSID, DEF_HS_SSID);
  hsPass = prefs.getString(K_HS_PASS, DEF_HS_PASS);
  hmSsid = prefs.getString(K_HM_SSID, DEF_HM_SSID);
  hmPass = prefs.getString(K_HM_PASS, DEF_HM_PASS);
  apSsid = prefs.getString(K_AP_SSID, DEF_AP_SSID);
  apPass = prefs.getString(K_AP_PASS, DEF_AP_PASS);
  otaPassword = prefs.getString(K_OTA_PASS, DEF_OTA_PASS);
}

void setup() {
  Serial.begin(115200);
  prefs.begin(PREF_NAMESPACE, false); // RW
  loadPreferences();
  //  prefs.begin("KoClicker");
  pinMode(BUTTON, BUTTON_MODE);
  delay(200);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  startupAnimation();

  Serial.println("KoClicker Starting...");


  mode = prefs.getString("mode", "");

  //Set default mode if not present
  if (mode == "") {
    Serial.println("No Mode found, setting up the default mode to - HotSpot");
    prefs.putString("mode", "AccessPoint");
    mode = "AccessPoint";
  } else {
    Serial.print("Mode found: ");
    Serial.println(mode);
  }

  if (digitalRead(BUTTON) == LOW) {
    Serial.println("Aborting startup and switching modes...");
    switchMode();
  }

  if (mode == "AccessPoint") {
    WiFi.onEvent(onWiFiEvent);
    WiFi.softAP(apSsid, apPass);
    Serial.print("AP Started. IP: ");
    Serial.println(WiFi.softAPIP());
    modeAnimation(RED_LED);
    Serial.print("Waiting for a Kindle to connect");
    while (WiFi.softAPgetStationNum() == 0) {
      if (digitalRead(BUTTON) == LOW) {
        Serial.println("Aborting WiFi connection and switching modes...");
        switchMode();
      }
      delay(1000);
      digitalWrite(GREEN_LED, ON);
      Serial.print(".");
      delay(1000);
      digitalWrite(GREEN_LED, OFF);
      Serial.print(".");
    }
    connected = webCall("event/", 10);

  } else {
    //Connect to WiFi
    const char* ssid;
    const char* password;
    if (mode == "HotSpot") {
      ssid = hsSsid.c_str();
      password = hsPass.c_str();
      modeAnimation(GREEN_LED);
    } else {
      ssid = hmSsid.c_str();
      password = hmPass.c_str();
      modeAnimation(BLUE_LED);
    }
    WiFi.begin(ssid, password);
    Serial.print("Connecting to ");
    Serial.print(ssid);
    Serial.print(" ...");

    //Connection in progress - 1 second blink
    while (WiFi.status() != WL_CONNECTED) {
      if (digitalRead(BUTTON) == LOW) {
        Serial.println("Aborting WiFi connection and switching modes...");
        switchMode();
      }
      delay(1000);
      digitalWrite(GREEN_LED, ON);
      Serial.print(".");
      delay(1000);
      digitalWrite(GREEN_LED, OFF);
      Serial.print(".");
    }
  }
    Serial.println("Starting OTA function");
    ArduinoOTA.setHostname("KoClicker");
    ArduinoOTA.setPassword(otaPassword.c_str());
    ArduinoOTA.begin();
    ElegantOTA.begin(&server);
    // WebSocket endpoint and event hook
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    // Index page at "/"
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
      String idx = buildIndexHtml();
      req->send(200, "text/html", idx);
    });

    // Console page at "/console"
    server.on("/console", HTTP_GET, [](AsyncWebServerRequest* req) {
      req->send_P(200, "text/html", PAGE_CONSOLE);
    });

    // Example: add your own endpoints and they'll appear in the index if you add them to PAGES[]
    // server.on("/status", HTTP_GET, [](AsyncWebServerRequest* req){ req->send(200, "text/plain", "OK"); });

    server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* req){ req->send(200, "text/html", buildSettingsHtml()); });
    server.on("/api/settings", HTTP_GET, handleGetSettings);
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL, handlePostSettings);
    server.on("/api/reset", HTTP_POST, handleReset);

    server.begin();

    // Initial logs
    logLinenl("ESP32 ready");
    logLine("IP: ");
    logLinenl(WiFi.localIP().toString().c_str());
    logLinenl("Wi-Fi Connection established!");
    digitalWrite(GREEN_LED, ON);
    delay(3000);
    digitalWrite(GREEN_LED, OFF);
    logLine("KoClicker IP address: ");
    logLinenl("%s", WiFi.localIP().toString().c_str());

  if (mode == "HotSpot") {
    ipAddress = prefs.getString("last_Good_Ip", "");
    logLinenl("Testing last Kindle HotSpot IP: %s", ipAddress.c_str());
    connected = webCall("event/", 100);
    if (!connected) {
      logLinenl("Unable to connect with last Kindle HotSpot IP. Starting scan");
      IPAddress broadCast = WiFi.localIP();
      connected = false;
      logLinenl("Starting scan!");
      for (int i = 255; i > 0; i--) {
        if (digitalRead(BUTTON) == LOW) {
          logLinenl("Aborting scan!");
          switchMode();
        }
        broadCast[3] = i;
        ipAddress = broadCast.toString();
        connected = webCall("event/", 10);
        if (connected) {
          prefs.putString("last_Good_Ip", ipAddress);
          break;
        }
      }
    } else {
      logLinenl("Connected with last Kindle HotSpot IP: %s", ipAddress.c_str());
    }
  }


  delay(2000);
  connected = webCall("event/", 100);

  if (connected) {
    logLinenl("Kindle found.... Waiting for input");
    webCall("event/ToggleNightMode", 100);
    delay(1000);
    webCall("event/ToggleNightMode", 100);
  } else {
    logLinenl("Cant find Kindle after scan... Please change mode");
  }
}

void loop() {

  digitalWrite(RED_LED, OFF);
  digitalWrite(GREEN_LED, OFF);
  digitalWrite(BLUE_LED, OFF);

  unsigned long sleep_Timer = millis() - sleep_time_reset;
  //Sleep if innactive
  if (sleep_Timer > sleepCutoff || (mode == "AccessPoint" ? WiFi.softAPgetStationNum() == 0 : WiFi.status() != WL_CONNECTED)) sleep();

  ArduinoOTA.handle();
  // ElegantOTA.loop() is not needed when using ESPAsyncWebServer with ELEGANTOTA_USE_ASYNC_WEBSERVER=1
  

  if (digitalRead(BUTTON) == LOW) {
    unsigned long duration;
    unsigned long startTime = millis();

    while (digitalRead(BUTTON) == LOW) {
      delay(10);
      sleep_time_reset = millis();
      duration = millis() - startTime;
      //Serial.println(duration);
      if (duration < shortClick) {
        digitalWrite(BLUE_LED, ON);
      } else if (duration < longClick) {
        digitalWrite(BLUE_LED, ON);
        digitalWrite(RED_LED, ON);
      } else {
        digitalWrite(RED_LED, ON);
        digitalWrite(GREEN_LED, ON);
        digitalWrite(BLUE_LED, ON);
        delay(200);
        digitalWrite(RED_LED, OFF);
        digitalWrite(GREEN_LED, OFF);
        digitalWrite(BLUE_LED, OFF);
        delay(200);
      }
    }
    digitalWrite(RED_LED, OFF);
    digitalWrite(GREEN_LED, OFF);
    digitalWrite(BLUE_LED, OFF);

    if (duration < shortClick) {
      // Next page on click
      pageTurn(1, 150);
    } else if (duration < longClick) {
      // Previous page on long click
      pageTurn(-1, 150);
    } else {
      // Switch mode in super long click
      logLinenl("Super long click detected");
      switchMode();
    }
  }
}