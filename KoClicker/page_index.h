#pragma once

// ====== Index page ("/") helper ======
struct PageLink {
  const char* path;
  const char* title;
  const char* desc;
};
PageLink PAGES[] = {
  { "/settings", "Configuration", "Configure Settings for KoClicker" },
  { "/console", "Web Console", "Interactive logs & commands (WebSocket)" },
  { "/update", "Firmware Update", "OTA Firmware update page" },
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
            "footer{margin-top:40px;font-size:0.9em;color:#777;text-align:center;}"
            "</style>");

  html += F("<h1>KoClicker - ");
  html += KoClickerVersion;
  html += F("</h1>");

  // Add placeholder text under H1
  html += F("<p>KoClicker index page</p>");

  html += F("<div class='card'>");
  html += F("<div><strong>KoClicker IP:</strong> <span class='path'>");
  html += KoClickerIpAddress;
  html += F("</span></div>");
  html += F("<div><strong>Kindle IP:</strong> <span class='path'>");
  html += kindleIpAddress;
  html += F("</span></div>");
  html += F("<div><strong>Internet:</strong> <span class='path'>");
  html += staConnected ? "Connected" : "Not connected";
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

  // Add footer with copyright + link
  html += F("<footer>&copy; Nefelodamon 2026 — <a href='https://github.com/nefelodamon/KoClicker'>KoClicker GitHub</a></footer>");

  return html;
}
