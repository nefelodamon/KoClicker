#pragma once

#ifdef CONFIG_IDF_TARGET_ESP32C3

#include <U8g2lib.h>
#include <Wire.h>
#include <math.h>

// Initialize the OLED display: 72x40 SSD1306 ER, Full Buffer, Hardware I2C
// SCL Pin: 6, SDA Pin: 5
U8G2_SSD1306_72X40_ER_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 6, 5);

// ── Adjust this to change the total animation duration ────────────────────────
const int TOTAL_DURATION_MS = 3000;
// ─────────────────────────────────────────────────────────────────────────────

#define DISP_W 72
#define DISP_H 40

// Phase budget proportions (must sum to 100)
#define PCT_FLIP      30
#define PCT_TYPE_CHAR 20
#define PCT_TYPE_HOLD 10
#define PCT_FILL      30
#define PCT_FADE      10

// Estimated time one sendBuffer() takes over I2C at 400 kHz for a 72×40 display.
// Used to subtract the unavoidable transfer cost from explicit delays.
#define I2C_OVERHEAD_MS 8

#define FLIP_FRAMES 60   // number of frames for the full flip cycle
#define FADE_STEPS  86   // ceil(255 / 3) contrast steps

uint8_t  textBitmap[DISP_H][DISP_W];
uint16_t pixelOrder[DISP_W * DISP_H];


// ── Helpers ───────────────────────────────────────────────────────────────────

void blitTextBitmap() {
  uint8_t *buf = u8g2.getBufferPtr();
  memset(buf, 0, DISP_W * DISP_H / 8);
  for (int py = 0; py < DISP_H; py++) {
    for (int px = 0; px < DISP_W; px++) {
      if (textBitmap[py][px])
        buf[(py / 8) * DISP_W + px] |= (1 << (py % 8));
    }
  }
}

void preRenderText() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB10_tr);
  int w = u8g2.getStrWidth("KoClicker");
  int x = (DISP_W - w) / 2;
  int y = DISP_H / 2 + u8g2.getAscent() / 2;
  u8g2.drawStr(x, y, "KoClicker");

  uint8_t *buf = u8g2.getBufferPtr();
  for (int py = 0; py < DISP_H; py++)
    for (int px = 0; px < DISP_W; px++)
      textBitmap[py][px] = (buf[(py / 8) * DISP_W + px] >> (py % 8)) & 1;
}

// ── Animation phases ──────────────────────────────────────────────────────────

// Horizontal flip: normal → squish → mirrored → squish → normal
void horizontalFlipOnce(int budgetMs) {
  const float cx         = DISP_W / 2.0f;
  int         frameDelay = max(0, budgetMs / FLIP_FRAMES - I2C_OVERHEAD_MS);

  for (int frame = 0; frame <= FLIP_FRAMES; frame++) {
    digitalWrite(RED_LED,   ((frame / 10) % 3 == 0) ? ON : OFF);
    digitalWrite(GREEN_LED, ((frame / 10) % 3 == 1) ? ON : OFF);
    digitalWrite(BLUE_LED,  ((frame / 10) % 3 == 2) ? ON : OFF);

    float a        = frame * (2.0f * (float)M_PI / FLIP_FRAMES);
    float scale    = cosf(a);
    float absScale = fabsf(scale);
    bool  mirrored = (scale < 0);

    uint8_t *buf = u8g2.getBufferPtr();
    memset(buf, 0, DISP_W * DISP_H / 8);

    if (absScale > 0.01f) {
      for (int dy = 0; dy < DISP_H; dy++) {
        for (int dx = 0; dx < DISP_W; dx++) {
          int sx = (int)(((float)dx - cx) / absScale + cx + 0.5f);
          if (mirrored) sx = DISP_W - 1 - sx;
          if (sx >= 0 && sx < DISP_W && textBitmap[dy][sx])
            buf[(dy / 8) * DISP_W + dx] |= (1 << (dy % 8));
        }
      }
    }

    u8g2.sendBuffer();
    if (frameDelay > 0) delay(frameDelay);
  }
  digitalWrite(RED_LED,   OFF);
  digitalWrite(GREEN_LED, OFF);
  digitalWrite(BLUE_LED,  OFF);
}

// Type version string character by character below "KoClicker"
void typeVersion(int charBudgetMs, int holdBudgetMs) {
  const char *version = KoClickerVersion.c_str();
  int versionLen = strlen(version);
  int charDelay  = max(0, charBudgetMs / versionLen - I2C_OVERHEAD_MS);

  u8g2.setFont(u8g2_font_ncenB08_tr);
  int vx = (DISP_W - u8g2.getStrWidth(version)) / 2;
  int vy = DISP_H - 2;

  for (int i = 1; i <= versionLen; i++) {
    char partial[16];
    strncpy(partial, version, i);
    partial[i] = '\0';

    blitTextBitmap();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(vx, vy, partial);
    u8g2.sendBuffer();
    if (charDelay > 0) delay(charDelay);
  }

  if (holdBudgetMs > 0) delay(holdBudgetMs);
}

// Scatter white pixels across the screen randomly until full, then fade to black
void randomFillAndFade(int fillBudgetMs, int fadeBudgetMs) {
  for (int i = 0; i < DISP_W * DISP_H; i++) pixelOrder[i] = i;
  for (int i = DISP_W * DISP_H - 1; i > 0; i--) {
    int      j   = random(i + 1);
    uint16_t tmp = pixelOrder[i];
    pixelOrder[i] = pixelOrder[j];
    pixelOrder[j] = tmp;
  }

  int pixelsPerStep = max(1, (DISP_W * DISP_H) * I2C_OVERHEAD_MS / max(1, fillBudgetMs));

  uint8_t *buf = u8g2.getBufferPtr();
  for (int i = 0; i < DISP_W * DISP_H; i++) {
    int px = pixelOrder[i] % DISP_W;
    int py = pixelOrder[i] / DISP_W;
    buf[(py / 8) * DISP_W + px] |= (1 << (py % 8));
    if (i % pixelsPerStep == pixelsPerStep - 1) u8g2.sendBuffer();
  }
  u8g2.sendBuffer();

  int fadeStepDelay = max(0, fadeBudgetMs / FADE_STEPS);
  for (int c = 255; c >= 0; c -= 3) {
    u8g2.setContrast(c);
    if (fadeStepDelay > 0) delay(fadeStepDelay);
  }

  u8g2.clearBuffer();
  u8g2.sendBuffer();
  u8g2.setContrast(255);
}

// ── OLED status display ───────────────────────────────────────────────────────

#define OLED_X_OFFSET 0
#define OLED_Y_OFFSET 9
#define OLED_X(x) ((x) + OLED_X_OFFSET)
#define OLED_Y(y) ((y) + OLED_Y_OFFSET)
#define OLED_CLEAR_AFTER_MS 3000

bool           oledActive       = false;
unsigned long  oledLastDrawTime = 0;

// Sets the largest font that fits text within DISP_W
void setFittingFont(const char* text) {
  u8g2.setFont(u8g2_font_6x13_tf);
  if (u8g2.getStrWidth(text) <= DISP_W) return;
  u8g2.setFont(u8g2_font_5x7_tf);
  if (u8g2.getStrWidth(text) <= DISP_W) return;
  u8g2.setFont(u8g2_font_4x6_tf);
}

void drawOledLine(const char* text, int line) {
  u8g2.setFont(u8g2_font_6x13_tf);
  int asc   = u8g2.getAscent();
  int lineH = asc - u8g2.getDescent();

  // Always refresh line 1 (mode + signal + page counter)
  u8g2.setDrawColor(0);
  u8g2.drawBox(0, OLED_Y(0) - asc, DISP_W, lineH);
  u8g2.setDrawColor(1);
  // Left: AP+ if upstream connected, AP if not
  const char* modeLabel = staConnected ? "AP+" : "AP";
  u8g2.drawStr(OLED_X(0), OLED_Y(0), modeLabel);
  // Right: page counter
  char cntBuf[8];
  snprintf(cntBuf, sizeof(cntBuf), "%d", pageCounter);
  u8g2.drawStr(DISP_W - u8g2.getStrWidth(cntBuf), OLED_Y(0), cntBuf);
  // Centre: AP station count (Kindle connected indicator)
  char sigBuf[8];
  snprintf(sigBuf, sizeof(sigBuf), "%dST", WiFi.softAPgetStationNum());
  u8g2.drawStr((DISP_W - u8g2.getStrWidth(sigBuf)) / 2, OLED_Y(0), sigBuf);

  // line == 0: clear entire screen
  if (line == 0) {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    oledActive = false;
    return;
  }

  // Clear and draw only the specified line
  int y = (line == 2) ? OLED_Y(15) : (line == 3) ? OLED_Y(30) : -1;
  if (y >= 0) {
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, y - asc, DISP_W, lineH);
    u8g2.setDrawColor(1);
    setFittingFont(text);
    u8g2.drawStr(OLED_X(0), y, text);
    u8g2.setFont(u8g2_font_6x13_tf);  // restore default font
  }

  u8g2.sendBuffer();
  oledActive       = true;
  oledLastDrawTime = millis();
}

void oledTick() {
  if (oledActive && millis() - oledLastDrawTime > OLED_CLEAR_AFTER_MS)
    drawOledLine("", 0);
}

// ── Entry point ───────────────────────────────────────────────────────────────

void boot_animation() {
  u8g2.begin();
  u8g2.setContrast(255);
  u8g2.setBusClock(400000);
  u8g2.setFont(u8g2_font_ncenB10_tr);
  preRenderText();

  horizontalFlipOnce(TOTAL_DURATION_MS * PCT_FLIP      / 100);
  typeVersion(       TOTAL_DURATION_MS * PCT_TYPE_CHAR / 100,
                     TOTAL_DURATION_MS * PCT_TYPE_HOLD / 100);
  randomFillAndFade( TOTAL_DURATION_MS * PCT_FILL      / 100,
                     TOTAL_DURATION_MS * PCT_FADE      / 100);
}

#else

inline void drawOledLine(const char*, int) {}
inline void oledTick() {}

#endif // CONFIG_IDF_TARGET_ESP32C3
