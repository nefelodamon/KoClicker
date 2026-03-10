#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Update.h>
#include <ArduinoJson.h>

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
const char* K_HM_IP_ADDRESS = "kindleHmIpAddr";
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
const char* K_MODE = "mode";
const char* K_LAST_GOOD_IP = "last_Good_Ip";
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
String KoClickerVersion = "v1.0.5";
uint32_t shortClick;
uint32_t longClick;
uint32_t sleepCutoff;
String hmkindleIp;
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
String mode;
unsigned long sleep_time_reset;
String macFilter;
String lastConnectedMac;
bool stationAllowed = false;
int pageCounter = 0;
uint32_t doubleClickMs;
bool pendingRestart = false;
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
  doc["kindlhmipaddr"] = prefs.getString(K_HM_IP_ADDRESS);  //CHG
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
  if (doc.containsKey("kindlhmipaddr")) prefs.putString(K_HM_IP_ADDRESS, doc["kindlhmipaddr"].as<String>());  //CHG
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

void modeAnimation(unsigned char colour) {
  digitalWrite(colour, ON);
  drawOledLine("Mode:", 2);
  if (colour == BLUE_LED)        drawOledLine("Home", 3);
  else if (colour == GREEN_LED)  drawOledLine("HotSpot", 3);
  else                           drawOledLine("AccessPoint", 3);
  delay(3000);
  digitalWrite(colour, OFF);
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

void switchMode() {
  String newMode;
  drawOledLine("Chg mode to", 2);

  if (mode == "HotSpot") {
    newMode = "Home";
    drawOledLine(newMode.c_str(), 3);
    modeAnimation(BLUE_LED);
  } else if (mode == "Home") {
    newMode = "AccessPoint";
    drawOledLine(newMode.c_str(), 3);
    modeAnimation(RED_LED);
  } else {
    newMode = "HotSpot";
    drawOledLine(newMode.c_str(), 3);
    modeAnimation(GREEN_LED);
  }

  logLinenl("Changing Mode to %s", newMode.c_str());
  prefs.putString(K_MODE, newMode);
  logLinenl("Resetting board...Bye bye!");

  WiFi.disconnect();
  delay(1000);
  ESP.restart();
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
      "  mode     - WiFi mode (AP/HotSpot/Home)\n"
      "  wifi     - WiFi SSID + RSSI\n"
      "  rssi     - WiFi signal strength (dBm)\n"
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
    client->text("mode=" + mode + "\n");
  } else if (cmd == "wifi") {
    if (mode == "AccessPoint") {
      client->text("wifi=AP ssid=" + apSsid + " stations=" + String(WiFi.softAPgetStationNum()) + "\n");
    } else {
      char buf[80];
      snprintf(buf, sizeof(buf), "wifi=STA ssid=%s rssi=%d dBm status=%d\n",
               WiFi.SSID().c_str(), WiFi.RSSI(), (int)WiFi.status());
      client->text(buf);
    }
  } else if (cmd == "rssi") {
    if (mode == "AccessPoint") {
      client->text("rssi=N/A (AP mode)\n");
    } else {
      client->text("rssi=" + String(WiFi.RSSI()) + " dBm\n");
    }
  } else if (cmd == "settings") {
    char buf[256];
    snprintf(buf, sizeof(buf),
      "shortClick=%u ms  longClick=%u ms  sleepCutoff=%u ms\n"
      "httpCallTO=%d ms  httpConnTO=%d ms  delayBetween=%d ms\n"
      "kindle_hm_ip=%s\n",
      shortClick, longClick, sleepCutoff,
      httpCallTimeout, httpConnectTimeout, delayBetweenCalls,
      hmkindleIp.c_str());
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

  hmkindleIp = prefs.getString(K_HM_IP_ADDRESS);
  logLinenl("Load Preference hmkindleIp: %s", hmkindleIp.c_str());

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

  bool connected = false;
  mode = prefs.getString(K_MODE, "");

  //Set default mode if not present
  if (mode == "") {
    Serial.println("No Mode found, setting up the default mode to - HotSpot");
    prefs.putString(K_MODE, "AccessPoint");
    mode = "AccessPoint";
  } else {
    Serial.println("Mode found: " + mode);
  }

  if (digitalRead(BUTTON) == LOW) {
    Serial.println("Aborting startup and switching modes...");
    switchMode();
  }

  // Start WiFi (no waiting yet)
  if (mode == "AccessPoint") {
    WiFi.onEvent(onWiFiEvent);
    WiFi.softAP(apSsid, apPass);
    KoClickerIpAddress = WiFi.softAPIP().toString();
    Serial.println("AP Started. KoClicker IP: " + KoClickerIpAddress);
    modeAnimation(RED_LED);
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
      kindleIpAddress = hmkindleIp;
      modeAnimation(BLUE_LED);
    }
    WiFi.begin(ssid, password);
    Serial.print("Connecting to " + String(ssid) + " ...");
  }

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

  // Start web server before waiting for any client/WiFi connection
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

  // Start OTA before waiting loop so it's available immediately
  Serial.println("Starting ArduinoOTA function");
  ArduinoOTA.setHostname("KoClicker");
  ArduinoOTA.setPassword(otaPassword.c_str());
  ArduinoOTA.begin();

  drawOledLine("Connecting", 2);

  // Now wait for a client/WiFi connection
  if (mode == "AccessPoint") {
    Serial.print("Waiting for an allowed Kindle to connect");
    while (!stationAllowed) {
      if (digitalRead(BUTTON) == LOW) {
        Serial.println("\nAborting WiFi connection and switching modes...");
        switchMode();
      }
      waitingWiFi();
    }
    connected = webCall("event/", 10);

  } else {
    //Connection in progress - 1 second blink
    while (WiFi.status() != WL_CONNECTED) {
      if (digitalRead(BUTTON) == LOW) {
        Serial.println("\nAborting WiFi connection and switching modes...");
        switchMode();
      }
      waitingWiFi();
    }
  }

  drawOledLine("Connected", 2);
  drawOledLine("", 3);


  // Initial logs
  if (mode != "AccessPoint") {
    KoClickerIpAddress = WiFi.localIP().toString();
  }
  logLinenl("Wi-Fi Connection established!");
  logLinenl("KoClicker IP address: %s", KoClickerIpAddress.c_str());

  drawOledLine("Connected", 2);
  drawOledLine(KoClickerIpAddress.c_str(), 3);

  if (mode == "HotSpot") {
    kindleIpAddress = prefs.getString(K_LAST_GOOD_IP, "");
    logLinenl("Testing last Kindle HotSpot IP: %s", kindleIpAddress.c_str());
    connected = webCall("event/", 100);
    if (!connected) {
      logLinenl("Unable to connect with last Kindle HotSpot IP. Starting scan");
      IPAddress broadCast = WiFi.localIP();
      connected = false;
      logLinenl("Starting scan!");
        drawOledLine("Scanning", 2);
      for (int i = 255; i > 0; i--) {
        if (digitalRead(BUTTON) == LOW) {
          logLinenl("Aborting scan!");
          switchMode();
        }
        broadCast[3] = i;
        kindleIpAddress = broadCast.toString();
        drawOledLine(kindleIpAddress.c_str(), 3);
        logLinenl("Scanning...Testing: %s", kindleIpAddress.c_str());
        connected = webCall("event/", 10);
        if (connected) {
          prefs.putString(K_LAST_GOOD_IP, kindleIpAddress);
          break;
        }
      }
    } else {
      logLinenl("Connected with last Kindle HotSpot IP: %s", kindleIpAddress.c_str());
      drawOledLine("Connected", 2);
      drawOledLine(kindleIpAddress.c_str(), 3);
    }
  }

  sleep_time_reset = millis();
  delay(750);
  connected = webCall("event/", 100);

  if (connected) {
    logLinenl("Kindle found and connected.... Waiting for input");
    drawOledLine("Kindle", 2);
    drawOledLine("Found", 3);
    webCall("event/ToggleNightMode", 100);
    delay(1000);
    webCall("event/ToggleNightMode", 100);
  } else {
    logLinenl("Cant find Kindle after scan... Please change mode");
    drawOledLine("Kindle", 2);
    drawOledLine("Not found", 3);
  }
}

void loop() {

  if (pendingRestart) { delay(100); ESP.restart(); }

  digitalWrite(RED_LED, OFF);
  digitalWrite(GREEN_LED, OFF);
  digitalWrite(BLUE_LED, OFF);

  oledTick();

  unsigned long sleep_Timer = millis() - sleep_time_reset;
  //Sleep if innactive or not connected to WiFi or Kindle disconnected. Do not sleep if sleepCutoff is 0
  if (sleepCutoff > 0 && (sleep_Timer > sleepCutoff || (mode == "AccessPoint" ? WiFi.softAPgetStationNum() == 0 : WiFi.status() != WL_CONNECTED))) {
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
      int modeLed = (mode == "AccessPoint") ? RED_LED : (mode == "HotSpot") ? GREEN_LED : BLUE_LED;
      digitalWrite(modeLed, ((millis() / 500) % 2 == 0) ? ON : OFF);
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
        drawOledLine("Changing", 2);
        drawOledLine("Mode", 3);
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
      // Show page counter on long click
      char buf[16];
      snprintf(buf, sizeof(buf), "Pages: %d", pageCounter);
      logLinenl("Long click - %s", buf);
      drawOledLine(buf, 2);
      drawOledLine(" ", 3);
    } else {
      // Switch mode in super long click
      logLinenl("Super long click detected");
      switchMode();
    }
  }
}