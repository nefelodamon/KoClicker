#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "lwip/lwip_napt.h"

// Hardware pin assignments — selected automatically by target board
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  #define BUTTON 2
  #define WAKE_PIN 2  // Must be GPIO 0-5 (RTC domain) to wake from deep sleep. Change to match your wired button.
  #define RED_LED 3
  #define GREEN_LED 4
  #define BLUE_LED 7
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  #define BUTTON 48
  #define RED_LED 47
  #define GREEN_LED 46
  #define BLUE_LED 45
#else
  #error "Unsupported board. Add pin definitions for your target."
#endif

#define BUTTON_MODE INPUT_PULLUP
#define ON LOW
#define OFF HIGH

AsyncWebServer server(80);
AsyncWebSocket ws("/console-ws");
Preferences prefs;


// Preference namespace and keys - Max 15 chars size
const char* PREF_NAMESPACE = "settings";
const char* K_SHORT_CLICK = "shortClick";
const char* K_LONG_CLICK = "longClick";
const char* K_SLEEP_CUTOFF = "sleepCutoff";
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
const char* K_MAC_FILTER = "mac_filter";
const char* K_DOUBLE_CLICK_MS = "dblClickMs";

// Defaults
const uint32_t DEF_SHORT_CLICK = 500;
const uint32_t DEF_LONG_CLICK = 5 * 1000;
const uint32_t DEF_SLEEP_CUTOFF = 5 * 60 * 1000;
const int DEF_HTTP_CALL_TIMEOUT = 750;
const int DEF_HTTP_CONNECT_TIMEOUT = 750;
const int DEF_DELAY_BETWEEN_CALLS = 400;
const char* DEF_AP_SSID = "KoClicker";
const char* DEF_AP_PASS = "12345678";
const char* DEF_OTA_PASS = "1234";
const int KOREADER_PORT = 8080;
const uint32_t DEF_DOUBLE_CLICK_MS = 300;

// Runtime variables
String KoClickerVersion = "v2.0.0";
uint32_t shortClick;
uint32_t longClick;
uint32_t sleepCutoff;
int httpCallTimeout;
int httpConnectTimeout;
int delayBetweenCalls;
String hsSsid;
String hsPass;
String hmSsid;
String hmPass;
String apSsid;
String apPass;
String otaPassword;
String kindleIpAddress;
String KoClickerIpAddress;
unsigned long sleep_time_reset;
String macFilter;
String lastConnectedMac;
bool stationAllowed = false;
int pageCounter = 0;
uint32_t doubleClickMs;
bool pendingRestart = false;
bool staConnected = false;
bool pendingWifiReconnect = false;
unsigned long wifiReconnectAt = 0;
//---------------------------------------------------------------------------//

#include "c3_oled.h"
#include "page_console.h"
#include "page_index.h"
#include "page_settings.h"
#include "page_update.h"

// API: GET /api/settings
void handleGetSettings(AsyncWebServerRequest* request) {
  DynamicJsonDocument doc(2048);
  doc["shortClick"] = prefs.getUInt(K_SHORT_CLICK, DEF_SHORT_CLICK);
  doc["longClick"] = prefs.getUInt(K_LONG_CLICK, DEF_LONG_CLICK);
  doc["sleepCutoff"] = prefs.getUInt(K_SLEEP_CUTOFF, DEF_SLEEP_CUTOFF);
  doc["httpCallTimeout"] = prefs.getInt(K_HTTP_CALL_TIMEOUT, DEF_HTTP_CALL_TIMEOUT);
  doc["httpConnectTimeout"] = prefs.getInt(K_HTTP_CONNECT_TIMEOUT, DEF_HTTP_CONNECT_TIMEOUT);
  doc["delayBetweenCalls"] = prefs.getInt(K_DELAY_BETWEEN, DEF_DELAY_BETWEEN_CALLS);
  doc["hs_ssid"] = prefs.getString(K_HS_SSID);
  doc["hs_password"] = prefs.getString(K_HS_PASS);
  doc["hm_ssid"] = prefs.getString(K_HM_SSID);
  doc["hm_password"] = prefs.getString(K_HM_PASS);
  doc["ap_ssid"] = prefs.getString(K_AP_SSID, DEF_AP_SSID);
  doc["ap_password"] = prefs.getString(K_AP_PASS, DEF_AP_PASS);
  doc["ota_password"] = prefs.getString(K_OTA_PASS, DEF_OTA_PASS);
  doc["mac_filter"] = prefs.getString(K_MAC_FILTER, "");
  doc["doubleClickMs"] = prefs.getUInt(K_DOUBLE_CLICK_MS, DEF_DOUBLE_CLICK_MS);

  String out;
  serializeJson(doc, out);
  request->send(200, "application/json", out);
}

// Saves preferences
void handlePostSettings(AsyncWebServerRequest* request, uint8_t* data, size_t len, size_t index, size_t total) {
  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(doc, data, len);
  if (err) {
    request->send(400, "application/json", "{\"ok\":false,\"error\":\"invalid_json\"}");
    return;
  }

  // Store into Preferences
  if (doc.containsKey("shortClick")) prefs.putUInt(K_SHORT_CLICK, doc["shortClick"]);
  if (doc.containsKey("longClick")) prefs.putUInt(K_LONG_CLICK, doc["longClick"]);
  if (doc.containsKey("sleepCutoff")) {
    uint32_t sc = doc["sleepCutoff"];
    if (sc > 0 && sc < 120000) sc = 120000;
    prefs.putUInt(K_SLEEP_CUTOFF, sc);
  }
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
  if (doc.containsKey("mac_filter")) prefs.putString(K_MAC_FILTER, doc["mac_filter"].as<String>());
  if (doc.containsKey("doubleClickMs")) prefs.putUInt(K_DOUBLE_CLICK_MS, doc["doubleClickMs"]);

  // Reload runtime variables
  loadPreferences();

  request->send(200, "application/json", "{\"ok\":true,\"message\":\"Saved.\"}");
}

// API: POST /api/reset
void handleReset(AsyncWebServerRequest* request) {
  prefs.clear();
  request->send(200, "application/json", "{\"ok\":true,\"message\":\"Preferences cleared. Restarting...\"}");
  pendingRestart = true;
}

void blinkLed(int pin, int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, ON);
    delay(onMs);
    digitalWrite(pin, OFF);
    delay(offMs);
  }
}

void blinkAllLeds(int times, int onMs, int offMs) {
  for (int i = 0; i < times; i++) {
    digitalWrite(RED_LED, ON);
    digitalWrite(GREEN_LED, ON);
    digitalWrite(BLUE_LED, ON);
    delay(onMs);
    digitalWrite(RED_LED, OFF);
    digitalWrite(GREEN_LED, OFF);
    digitalWrite(BLUE_LED, OFF);
    delay(offMs);
  }
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

void waitingWiFi() {
  delay(1000);
  drawOledLine("|", 3);
  digitalWrite(GREEN_LED, ON);
  Serial.print(".");
  delay(1000);
  drawOledLine("--", 3);
  digitalWrite(GREEN_LED, OFF);
  Serial.print(".");
}

void sleep() {
  logLinenl("Going to sleep... Goodnight!");
  drawOledLine("Going to", 2);
  drawOledLine("Sleep", 3);
  blinkLed(RED_LED, 9, 200, 200);

  digitalWrite(RED_LED, ON);
  digitalWrite(GREEN_LED, ON);
  digitalWrite(BLUE_LED, ON);

  drawOledLine("", 0);

  #ifdef WAKE_PIN
    esp_deep_sleep_enable_gpio_wakeup(1ULL << WAKE_PIN, ESP_GPIO_WAKEUP_GPIO_LOW);
  #endif
    esp_deep_sleep_start();
}

void connectToUpstreamWifi() {
  if (hmSsid.length() == 0 && hsSsid.length() == 0) {
    logLinenl("No upstream WiFi networks configured.");
    return;
  }

  logLinenl("Scanning for upstream WiFi networks...");
  int n = WiFi.scanNetworks();
  logLinenl("Scan complete. %d networks found.", n);

  bool net1Found = false, net2Found = false;
  for (int i = 0; i < n; i++) {
    if (hmSsid.length() > 0 && WiFi.SSID(i) == hmSsid) net1Found = true;
    if (hsSsid.length() > 0 && WiFi.SSID(i) == hsSsid) net2Found = true;
  }
  WiFi.scanDelete();

  String foundSsid, foundPass;
  if (net1Found) {
    foundSsid = hmSsid;
    foundPass = hmPass;
    logLinenl("WiFi Network 1 found: %s", foundSsid.c_str());
  } else if (net2Found) {
    foundSsid = hsSsid;
    foundPass = hsPass;
    logLinenl("WiFi Network 2 found: %s", foundSsid.c_str());
  } else {
    logLinenl("No configured upstream networks visible. Retrying in 30s.");
    pendingWifiReconnect = true;
    wifiReconnectAt = millis() + 30000;
    return;
  }

  WiFi.begin(foundSsid.c_str(), foundPass.c_str());
}

void pageTurn(int direction, int waitTime) {
  logLinenl(direction == 1 ? "Next page" : "Previous page");
  String url = "event/GotoViewRel/" + String(direction);
  if (direction == 1){
    drawOledLine("Page-->", 2);
    drawOledLine(" ", 3);
  }else{
    drawOledLine("<--Page", 2);
    drawOledLine(" ", 3);
  }
  bool success = webCall(url, waitTime);
  if (success) {
    pageCounter += direction;
    delay(delayBetweenCalls);
    webCall("broadcast/InputEvent", waitTime);
    drawOledLine("OK", 3);
  }else{
    drawOledLine("Error", 3);
  }
}

bool webCall(const String& endpoint, int waitTime) {
  bool success = false;
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(httpCallTimeout);
  http.setConnectTimeout(httpConnectTimeout);

  String url = "http://" + kindleIpAddress + ":" + KOREADER_PORT + "/koreader/" + endpoint;
  http.begin(client, url.c_str());
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    logLine("%d", httpResponseCode);
    success = true;
    digitalWrite(GREEN_LED, ON);
    delay(waitTime);
    digitalWrite(GREEN_LED, OFF);
  } else {
    logLine("Error code: %d", httpResponseCode);
    digitalWrite(RED_LED, ON);
    delay(waitTime);
    digitalWrite(RED_LED, OFF);
  }
  logLine(" - ");
  logLinenl("%s", url.c_str());

  http.end();
  return success;
}

// Returns true if mac is in the comma-separated macFilter list, or if the list is empty.
bool isMacAllowed(const String& mac) {
  if (macFilter.length() == 0) return true;
  String needle = mac;
  needle.toUpperCase();
  String haystack = macFilter;
  haystack.toUpperCase();
  int start = 0;
  while (start < (int)haystack.length()) {
    int comma = haystack.indexOf(',', start);
    String entry = (comma == -1) ? haystack.substring(start) : haystack.substring(start, comma);
    entry.trim();
    if (entry.length() > 0 && entry == needle) return true;
    if (comma == -1) break;
    start = comma + 1;
  }
  return false;
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED: {
      char mac[18];
      snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
        info.wifi_ap_staconnected.mac[0], info.wifi_ap_staconnected.mac[1],
        info.wifi_ap_staconnected.mac[2], info.wifi_ap_staconnected.mac[3],
        info.wifi_ap_staconnected.mac[4], info.wifi_ap_staconnected.mac[5]);
      lastConnectedMac = mac;
      logLinenl("Station connected! MAC: %s", mac);
      if (isMacAllowed(mac)) {
        stationAllowed = true;
        logLinenl("MAC %s allowed.", mac);
      } else {
        logLinenl("MAC %s not in allow list, waiting for an allowed device.", mac);
      }
      break;
    }

    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
      if (isMacAllowed(lastConnectedMac)) {
        kindleIpAddress = IPAddress(info.wifi_ap_staipassigned.ip.addr).toString();
        logLinenl("IP Assigned to station: %s", kindleIpAddress.c_str());
        blinkAllLeds(2, 400, 200);
      } else {
        logLinenl("IP assigned to non Kindle MAC %s, ignoring.", lastConnectedMac.c_str());
      }
      break;

    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      logLinenl("Station disconnected.");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      staConnected = true;
      KoClickerIpAddress = WiFi.localIP().toString();
      logLinenl("Upstream WiFi connected! IP: %s", KoClickerIpAddress.c_str());
      ip_napt_enable(WiFi.softAPIP(), 1);
      logLinenl("NAT enabled - Kindle now has internet access.");
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      if (staConnected) {
        staConnected = false;
        ip_napt_enable(WiFi.softAPIP(), 0);
        logLinenl("Upstream WiFi disconnected. Retrying in 10s.");
        pendingWifiReconnect = true;
        wifiReconnectAt = millis() + 10000;
      }
      break;

    default:
      break;
  }
}

// ====== Logging helpers ======
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
      "  help     - show this help\n"
      "  version  - firmware version\n"
      "  uptime   - time since boot\n"
      "  heap     - free / min-free heap\n"
      "  ip       - device IP address\n"
      "  kindle   - current Kindle IP\n"
      "  mode     - AP+STA connection status\n"
      "  wifi     - AP stations + upstream WiFi info\n"
      "  rssi     - upstream WiFi signal strength (dBm)\n"
      "  settings - dump key runtime settings\n"
      "  sleep    - sleep cutoff + idle time\n"
      "  restart  - restart the device\n");
  } else if (cmd == "version") {
    client->text("version=" + KoClickerVersion + "\n");
  } else if (cmd == "uptime") {
    unsigned long ms = millis();
    unsigned long s = ms / 1000;
    unsigned long m = s / 60;
    unsigned long h = m / 60;
    char buf[48];
    snprintf(buf, sizeof(buf), "uptime=%luh %lum %lus (%lu ms)\n", h, m % 60, s % 60, ms);
    client->text(buf);
  } else if (cmd == "heap") {
    char buf[64];
    snprintf(buf, sizeof(buf), "free_heap=%u  min_free=%u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
    client->text(buf);
  } else if (cmd == "ip") {
    client->text("device_ip=" + KoClickerIpAddress + "\n");
  } else if (cmd == "kindle") {
    client->text("kindle_ip=" + (kindleIpAddress.length() ? kindleIpAddress : String("(not set)")) + "\n");
  } else if (cmd == "mode") {
    String status = "AP+STA  ap_stations=" + String(WiFi.softAPgetStationNum());
    status += "  sta=" + String(staConnected ? "connected" : "disconnected") + "\n";
    client->text(status);
  } else if (cmd == "wifi") {
    char buf[160];
    snprintf(buf, sizeof(buf), "ap: ssid=%s stations=%d\nsta: ssid=%s rssi=%d dBm connected=%s\n",
             apSsid.c_str(), WiFi.softAPgetStationNum(),
             WiFi.SSID().c_str(), WiFi.RSSI(), staConnected ? "yes" : "no");
    client->text(buf);
  } else if (cmd == "rssi") {
    if (staConnected) {
      client->text("rssi=" + String(WiFi.RSSI()) + " dBm\n");
    } else {
      client->text("rssi=N/A (upstream not connected)\n");
    }
  } else if (cmd == "settings") {
    char buf[256];
    snprintf(buf, sizeof(buf),
      "shortClick=%u ms  longClick=%u ms  sleepCutoff=%u ms\n"
      "httpCallTO=%d ms  httpConnTO=%d ms  delayBetween=%d ms\n",
      shortClick, longClick, sleepCutoff,
      httpCallTimeout, httpConnectTimeout, delayBetweenCalls);
    client->text(buf);
  } else if (cmd == "sleep") {
    unsigned long idle = millis() - sleep_time_reset;
    char buf[80];
    snprintf(buf, sizeof(buf), "sleep_cutoff=%u ms  idle=%lu ms  remaining=%ld ms\n",
             sleepCutoff, idle, sleepCutoff > 0 ? (long)sleepCutoff - (long)idle : -1L);
    client->text(buf);
  } else if (cmd == "restart") {
    client->text("Restarting...\n");
    delay(100);
    ESP.restart();
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
  logLinenl("Loading stored Preferences from memory");

  shortClick = prefs.getUInt(K_SHORT_CLICK, DEF_SHORT_CLICK);
  logLinenl("Load Preference shortClick: %u", shortClick);

  longClick = prefs.getUInt(K_LONG_CLICK, DEF_LONG_CLICK);
  logLinenl("Load Preference longClick: %u", longClick);

  sleepCutoff = prefs.getUInt(K_SLEEP_CUTOFF, DEF_SLEEP_CUTOFF);
  logLinenl("Load Preference sleepCutoff: %u", sleepCutoff);

  httpCallTimeout = prefs.getInt(K_HTTP_CALL_TIMEOUT, DEF_HTTP_CALL_TIMEOUT);
  logLinenl("Load Preference httpCallTimeout: %d", httpCallTimeout);

  httpConnectTimeout = prefs.getInt(K_HTTP_CONNECT_TIMEOUT, DEF_HTTP_CONNECT_TIMEOUT);
  logLinenl("Load Preference httpConnectTimeout: %d", httpConnectTimeout);

  delayBetweenCalls = prefs.getInt(K_DELAY_BETWEEN, DEF_DELAY_BETWEEN_CALLS);
  logLinenl("Load Preference delayBetweenCalls: %d", delayBetweenCalls);

  hsSsid = prefs.getString(K_HS_SSID);
  logLinenl("Load Preference hsSsid: %s", hsSsid.c_str());

  hsPass = prefs.getString(K_HS_PASS);
  logLinenl("Load Preference hsPass: %s", hsPass.c_str());

  hmSsid = prefs.getString(K_HM_SSID);
  logLinenl("Load Preference hmSsid: %s", hmSsid.c_str());

  hmPass = prefs.getString(K_HM_PASS);
  logLinenl("Load Preference hmPass: %s", hmPass.c_str());

  apSsid = prefs.getString(K_AP_SSID, DEF_AP_SSID);
  logLinenl("Load Preference apSsid: %s", apSsid.c_str());

  apPass = prefs.getString(K_AP_PASS, DEF_AP_PASS);
  logLinenl("Load Preference apPass: %s", apPass.c_str());

  otaPassword = prefs.getString(K_OTA_PASS, DEF_OTA_PASS);
  logLinenl("Load Preference otaPassword: %s", otaPassword.c_str());

  macFilter = prefs.getString(K_MAC_FILTER, "");
  logLinenl("Load Preference macFilter: %s", macFilter.c_str());

  doubleClickMs = prefs.getUInt(K_DOUBLE_CLICK_MS, DEF_DOUBLE_CLICK_MS);
  logLinenl("Load Preference doubleClickMs: %u", doubleClickMs);
}

void setup() {
  Serial.begin(115200);
  prefs.begin(PREF_NAMESPACE, false);  // RW
  loadPreferences();
  pinMode(BUTTON, BUTTON_MODE);
  delay(200);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLUE_LED, OUTPUT);

  Serial.println("KoClicker Starting...");

#ifdef CONFIG_IDF_TARGET_ESP32C3
  boot_animation();
#else
  startupAnimation();
#endif

  // Register WiFi event handler before starting WiFi
  WiFi.onEvent(onWiFiEvent);

  // Always start AP with fixed IP
  WiFi.softAPConfig(
    IPAddress(192, 168, 100, 1),  // AP IP
    IPAddress(192, 168, 100, 1),  // gateway
    IPAddress(255, 255, 255, 0)     // subnet
  );
  WiFi.softAP(apSsid.c_str(), apPass.c_str());
  KoClickerIpAddress = WiFi.softAPIP().toString();
  logLinenl("AP started. SSID: %s  IP: %s", apSsid.c_str(), KoClickerIpAddress.c_str());
  drawOledLine("AP started", 2);
  drawOledLine(KoClickerIpAddress.c_str(), 3);
  delay(500);

  // Start STA upstream connection attempt (non-blocking)
  connectToUpstreamWifi();

  // OTA update page
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", buildUpdateHtml());
  });
  server.on("/update", HTTP_POST,
    [](AsyncWebServerRequest* req) {
      bool ok = !Update.hasError();
      req->send(ok ? 200 : 500, "text/plain", ok ? "OK" : Update.errorString());
      if (ok) { delay(500); ESP.restart(); }
    },
    NULL,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      if (index == 0 && !Update.begin(total)) Update.printError(Serial);
      if (!Update.hasError()) Update.write(data, len);
      if (index + len == total) Update.end(true);
    }
  );

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

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/html", buildSettingsHtml());
  });
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on(
    "/api/settings", HTTP_POST, [](AsyncWebServerRequest* request) {}, NULL, handlePostSettings);
  server.on("/api/reset", HTTP_POST, handleReset);

  server.begin();

  Serial.println("Starting ArduinoOTA function");
  ArduinoOTA.setHostname("KoClicker");
  ArduinoOTA.setPassword(otaPassword.c_str());
  ArduinoOTA.begin();
  MDNS.addService("http", "tcp", 80);

  // Wait for Kindle to connect to AP
  drawOledLine("Waiting", 2);
  drawOledLine("for Kindle", 3);
  Serial.print("Waiting for an allowed Kindle to connect");
  while (!stationAllowed) {
    waitingWiFi();
  }

  bool connected = webCall("event/", 100);

  sleep_time_reset = millis();
  delay(750);
  connected = webCall("event/", 100);

  if (connected) {
    logLinenl("Kindle found and connected. Waiting for input.");
    drawOledLine("Kindle", 2);
    drawOledLine("Found", 3);
    webCall("event/ToggleNightMode", 100);
    delay(1000);
    webCall("event/ToggleNightMode", 100);
  } else {
    logLinenl("Can't find Kindle after connection. Check KOReader HTTP server.");
    drawOledLine("Kindle", 2);
    drawOledLine("Not found", 3);
  }
}

void loop() {

  if (pendingRestart) { delay(100); ESP.restart(); }

  // Handle pending upstream WiFi reconnection
  if (pendingWifiReconnect && millis() >= wifiReconnectAt) {
    pendingWifiReconnect = false;
    connectToUpstreamWifi();
  }

  digitalWrite(RED_LED, OFF);
  digitalWrite(GREEN_LED, OFF);
  digitalWrite(BLUE_LED, OFF);

  oledTick();

  unsigned long sleep_Timer = millis() - sleep_time_reset;
  // Sleep on inactivity or when Kindle disconnects from AP. Skip if sleepCutoff is 0.
  if (sleepCutoff > 0 && (sleep_Timer > sleepCutoff || WiFi.softAPgetStationNum() == 0)) {
    sleep();
  }

  // Countdown display: show time remaining when within 2 minutes of sleep
  if (sleepCutoff > 0 && sleep_Timer < sleepCutoff) {
    unsigned long remaining = sleepCutoff - sleep_Timer;
    if (remaining <= 120000) {
      static unsigned long lastCountdownMs = 0;
      if (millis() - lastCountdownMs >= 1000) {
        lastCountdownMs = millis();
        unsigned long secs = remaining / 1000;
        char buf[6];
        snprintf(buf, sizeof(buf), "%02lu:%02lu", secs / 60, secs % 60);
        drawOledLine("Sleeping in", 2);
        drawOledLine(buf, 3);
      }
    }
    if (remaining <= 60000) {
      // Green blink if internet available, blue blink if AP only
      int sleepLed = staConnected ? GREEN_LED : BLUE_LED;
      digitalWrite(sleepLed, ((millis() / 500) % 2 == 0) ? ON : OFF);
    }
  }

  ArduinoOTA.handle();

  if (digitalRead(BUTTON) == LOW) {
    unsigned long duration = 0;
    unsigned long startTime = millis();

    while (digitalRead(BUTTON) == LOW) {
      delay(10);
      sleep_time_reset = millis();
      duration = millis() - startTime;
      if (duration < shortClick) {
        digitalWrite(BLUE_LED, ON);
      } else if (duration < longClick) {
        digitalWrite(BLUE_LED, ON);
        digitalWrite(RED_LED, ON);
      } else {
        drawOledLine("Hold to", 2);
        drawOledLine("Restart", 3);
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
      // Wait for possible double click
      bool doubleClick = false;
      unsigned long releaseTime = millis();
      while (millis() - releaseTime < doubleClickMs) {
        sleep_time_reset = millis();
        if (digitalRead(BUTTON) == LOW) {
          doubleClick = true;
          while (digitalRead(BUTTON) == LOW) delay(10);
          break;
        }
        delay(10);
      }
      if (doubleClick) {
        // Previous page on double click
        pageTurn(-1, 150);
      } else {
        // Next page on single click
        pageTurn(1, 150);
      }
    } else if (duration < longClick) {
      // Show average time per page on long click
      int absPages = abs(pageCounter);
      char avgBuf[16];
      if (absPages > 0) {
        unsigned long avgSec = (millis() / 1000) / (unsigned long)absPages;
        if (avgSec >= 60) {
          snprintf(avgBuf, sizeof(avgBuf), "%lum%02lus", avgSec / 60, avgSec % 60);
        } else {
          snprintf(avgBuf, sizeof(avgBuf), "%lus", avgSec);
        }
      } else {
        snprintf(avgBuf, sizeof(avgBuf), "---");
      }
      logLinenl("Long click - avg/page: %s  pages: %d", avgBuf, pageCounter);
      drawOledLine("Avg/page:", 2);
      drawOledLine(avgBuf, 3);
    } else {
      // Restart on super-long press
      logLinenl("Super long click - restarting.");
      drawOledLine("Restarting", 2);
      drawOledLine("...", 3);
      delay(1000);
      ESP.restart();
    }
  }
}