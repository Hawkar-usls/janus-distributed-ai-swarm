/*
  PEA4 / JANUS SHELL v0.1

  First Janus shell for the 4.3" ESP32-P4 board.

  This sketch does not overwrite the stock backup. It rebuilds the useful shell
  shape from vendor display/touch examples and keeps Janus swarm functions in an
  observer-only layer.

  Hardware mapped from vendor package:
  - LCD: ST7701, MIPI DSI, 480x800
  - Touch: GT911 on I2C SDA=7 SCL=8
  - Audio codec: ES8311, I2S pins reserved
  - SD_MMC: pins reserved from vendor mp3 example

  Hard rule:
  PEA4 observes, computes, displays and archives. Buzz remains the Stratum and
  pool authority. This firmware does not submit shares.
*/

#include <Arduino.h>
#include <Preferences.h>
#include <SD_MMC.h>
#include <mbedtls/sha256.h>
#include <esp_heap_caps.h>
#include "driver/i2c_master.h"
#include "pins_config.h"
#include "src/lcd/st7701_lcd.h"
#include "src/touch/gt911_touch.h"

#define PEA4_VERSION "v0.1-janus-shell"
#define PEA4_NODE_ID "PEA4"
#define PEA4_ROLE "P4_TITAN"
#define PEA4_KIND "p4_janus_shell"
#define PEA4_OBSERVER_ONLY 1

#define SD_D0 39
#define SD_D1 40
#define SD_D2 41
#define SD_D3 42
#define SD_CMD 44
#define SD_CLK 43

#define I2S_MCK_IO 13
#define I2S_BCK_IO 12
#define I2S_DI_IO 48
#define I2S_WS_IO 10
#define I2S_DO_IO 9
#define ES8311_PA 11
#define ES8311_ADDR 0x18

static st7701_lcd lcd(LCD_RST);
static gt911_touch touch(TP_I2C_SDA, TP_I2C_SCL, TP_RST, TP_INT);
static Preferences prefs;

static uint16_t *fb = nullptr;
static bool displayOk = false;
static bool touchOk = false;
static bool sdOk = false;
static uint8_t brightnessStep = 1;

enum AppPage {
  PAGE_HOME = 0,
  PAGE_CAMERA,
  PAGE_SWARM,
  PAGE_TITAN,
  PAGE_AUDIO,
  PAGE_GALLERY,
  PAGE_SETTINGS
};

static AppPage page = PAGE_HOME;
static bool needRedraw = true;
static uint32_t lastUiMs = 0;
static uint32_t lastStatusMs = 0;
static uint32_t lastMinerRateMs = 0;
static uint32_t lastStateSaveMs = 0;
static uint32_t bootMs = 0;
static uint32_t touchDownMs = 0;
static bool wasTouched = false;

struct TitanMetrics {
  uint32_t bootCount = 0;
  uint32_t seq = 0;
  uint32_t nonce = 0;
  uint32_t hashRate = 0;
  uint32_t windowHashes = 0;
  uint64_t totalHashes = 0;
  uint32_t candidates = 0;
  uint32_t bestNonce = 0;
  uint16_t bestBits = 0;
  uint16_t targetBits = 22;
  float janusIo = 0.0f;
  float tranception = 0.0f;
  float loveReal = 0.0f;
  float loveImag = 0.0f;
  float lovePhase = 0.0f;
};

static TitanMetrics mx;
static uint8_t header80[80];

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static const uint16_t C_BG = 0x0841;
static const uint16_t C_PANEL = 0x1084;
static const uint16_t C_PANEL2 = 0x18E7;
static const uint16_t C_LINE = 0x39E7;
static const uint16_t C_TEXT = 0xEF7D;
static const uint16_t C_DIM = 0x9CF3;
static const uint16_t C_CYAN = 0x07FF;
static const uint16_t C_AMBER = 0xFD20;
static const uint16_t C_GREEN = 0x07E0;
static const uint16_t C_RED = 0xF800;
static const uint16_t C_BLUE = 0x3A7F;

static const uint8_t FONT5X7[96][5] PROGMEM = {
  {0,0,0,0,0},{0,0,95,0,0},{0,7,0,7,0},{20,127,20,127,20},{36,42,127,42,18},{35,19,8,100,98},{54,73,86,32,80},{0,8,7,3,0},
  {0,28,34,65,0},{0,65,34,28,0},{42,28,127,28,42},{8,8,62,8,8},{0,128,112,48,0},{8,8,8,8,8},{0,0,96,96,0},{32,16,8,4,2},
  {62,81,73,69,62},{0,66,127,64,0},{114,73,73,73,70},{33,65,73,77,51},{24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,49},{65,33,17,9,7},
  {54,73,73,73,54},{70,73,73,41,30},{0,0,20,0,0},{0,64,52,0,0},{0,8,20,34,65},{20,20,20,20,20},{0,65,34,20,8},{2,1,89,9,6},
  {62,65,93,89,78},{124,18,17,18,124},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,65,62},{127,73,73,73,65},{127,9,9,9,1},{62,65,65,81,115},
  {127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},{127,2,28,2,127},{127,4,8,16,127},{62,65,65,65,62},
  {127,9,9,9,6},{62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{3,1,127,1,3},{63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},
  {99,20,8,20,99},{3,4,120,4,3},{97,89,73,77,67},{0,127,65,65,0},{2,4,8,16,32},{0,65,65,127,0},{4,2,1,2,4},{64,64,64,64,64},
  {0,3,7,8,0},{32,84,84,120,64},{127,40,68,68,56},{56,68,68,68,40},{56,68,68,40,127},{56,84,84,84,24},{0,8,126,9,2},{24,164,164,156,120},
  {127,8,4,4,120},{0,68,125,64,0},{32,64,64,61,0},{127,16,40,68,0},{0,65,127,64,0},{124,4,120,4,120},{124,8,4,4,120},{56,68,68,68,56},
  {252,24,36,36,24},{24,36,36,24,252},{124,8,4,4,8},{72,84,84,84,36},{4,4,63,68,36},{60,64,64,32,124},{28,32,64,32,28},{60,64,48,64,60},{68,40,16,40,68},{76,144,144,144,124},{68,100,84,76,68},
  {0,8,54,65,0},{0,0,119,0,0},{0,65,54,8,0},{2,1,2,4,2},{124,18,17,18,124}
};

static void setPx(int x, int y, uint16_t c) {
  if (!fb || x < 0 || y < 0 || x >= LCD_H_RES || y >= LCD_V_RES) return;
  fb[y * LCD_H_RES + x] = c;
}

static void fillRect(int x, int y, int w, int h, uint16_t c) {
  if (!fb || w <= 0 || h <= 0) return;
  int x0 = max(0, x);
  int y0 = max(0, y);
  int x1 = min((int)LCD_H_RES, x + w);
  int y1 = min((int)LCD_V_RES, y + h);
  for (int yy = y0; yy < y1; yy++) {
    uint16_t *row = fb + yy * LCD_H_RES + x0;
    for (int xx = x0; xx < x1; xx++) *row++ = c;
  }
}

static void drawRect(int x, int y, int w, int h, uint16_t c) {
  fillRect(x, y, w, 1, c);
  fillRect(x, y + h - 1, w, 1, c);
  fillRect(x, y, 1, h, c);
  fillRect(x + w - 1, y, 1, h, c);
}

static void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  while (true) {
    setPx(x0, y0, c);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

static void drawCircle(int cx, int cy, int r, uint16_t c) {
  int x = -r, y = 0, err = 2 - 2 * r;
  do {
    setPx(cx - x, cy + y, c); setPx(cx - y, cy - x, c);
    setPx(cx + x, cy - y, c); setPx(cx + y, cy + x, c);
    int e2 = err;
    if (e2 <= y) err += ++y * 2 + 1;
    if (e2 > x || err > y) err += ++x * 2 + 1;
  } while (x < 0);
}

static void drawChar(int x, int y, char ch, uint16_t c, int s = 2) {
  if (ch < 32 || ch > 127) ch = '?';
  if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
  const uint8_t *glyph = FONT5X7[ch - 32];
  for (int col = 0; col < 5; col++) {
    uint8_t bits = pgm_read_byte(&glyph[col]);
    for (int row = 0; row < 7; row++) {
      if (bits & (1 << row)) fillRect(x + col * s, y + row * s, s, s, c);
    }
  }
}

static void drawText(int x, int y, const char *text, uint16_t c, int s = 2) {
  int cx = x;
  while (*text) {
    if (*text == '\n') { cx = x; y += 9 * s; text++; continue; }
    drawChar(cx, y, *text++, c, s);
    cx += 6 * s;
  }
}

static void drawTextf(int x, int y, uint16_t c, int s, const char *fmt, ...) {
  char buf[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  drawText(x, y, buf, c, s);
}

static void clear(uint16_t c = C_BG) {
  fillRect(0, 0, LCD_H_RES, LCD_V_RES, c);
}

static void flush() {
  if (displayOk && fb) lcd.lcd_draw_bitmap(0, 0, LCD_H_RES, LCD_V_RES, fb);
}

static uint16_t bitsFromHash(const uint8_t h[32]) {
  uint16_t bits = 0;
  for (int i = 0; i < 32; i++) {
    if (h[i] == 0) { bits += 8; continue; }
    for (int b = 7; b >= 0; b--) {
      if (h[i] & (1 << b)) return bits;
      bits++;
    }
  }
  return bits;
}

static void sha256d(const uint8_t *data, size_t len, uint8_t out[32]) {
  uint8_t tmp[32];
  mbedtls_sha256(data, len, tmp, 0);
  mbedtls_sha256(tmp, 32, out, 0);
}

static void seedHeader() {
  for (int i = 0; i < 80; i++) header80[i] = (uint8_t)(0xA5 ^ (i * 37) ^ (ESP.getEfuseMac() >> ((i & 7) * 8)));
}

static void runMinerSlice(uint32_t budgetUs) {
  uint32_t start = micros();
  uint8_t hash[32];
  while ((uint32_t)(micros() - start) < budgetUs) {
    header80[76] = (uint8_t)(mx.nonce);
    header80[77] = (uint8_t)(mx.nonce >> 8);
    header80[78] = (uint8_t)(mx.nonce >> 16);
    header80[79] = (uint8_t)(mx.nonce >> 24);
    sha256d(header80, sizeof(header80), hash);
    uint16_t b = bitsFromHash(hash);
    if (b > mx.bestBits) {
      mx.bestBits = b;
      mx.bestNonce = mx.nonce;
      mx.candidates++;
      needRedraw = true;
    }
    if (b >= mx.targetBits) mx.candidates++;
    mx.nonce++;
    mx.windowHashes++;
    mx.totalHashes++;
  }
}

static void updateMathSignals() {
  float bestNorm = min(1.0f, mx.bestBits / 32.0f);
  float hNorm = min(1.0f, mx.hashRate / 70000.0f);
  float memNorm = min(1.0f, log10f((float)mx.totalHashes + 10.0f) / 10.0f);
  mx.janusIo = 0.45f * bestNorm + 0.35f * hNorm + 0.20f * memNorm;
  mx.tranception = 0.60f * bestNorm + 0.25f * memNorm + 0.15f * sinf(mx.seq * 0.11f);
  mx.loveReal = mx.janusIo;
  mx.loveImag = memNorm;
  mx.lovePhase = atan2f(mx.loveImag, max(0.001f, mx.loveReal));
}

static void updateRates() {
  uint32_t now = millis();
  if (now - lastMinerRateMs >= 1000) {
    mx.hashRate = mx.windowHashes * 1000UL / max(1UL, now - lastMinerRateMs);
    mx.windowHashes = 0;
    lastMinerRateMs = now;
    mx.seq++;
    updateMathSignals();
    needRedraw = true;
  }
}

static void loadState() {
  prefs.begin("pea4shell", false);
  mx.bootCount = prefs.getUInt("boots", 0) + 1;
  mx.bestBits = prefs.getUShort("best", 0);
  mx.bestNonce = prefs.getUInt("nonce", 0);
  mx.candidates = prefs.getUInt("cand", 0);
  uint32_t hi = prefs.getUInt("th_hi", 0);
  uint32_t lo = prefs.getUInt("th_lo", 0);
  mx.totalHashes = ((uint64_t)hi << 32) | lo;
  prefs.putUInt("boots", mx.bootCount);
}

static void saveState() {
  prefs.putUShort("best", mx.bestBits);
  prefs.putUInt("nonce", mx.bestNonce);
  prefs.putUInt("cand", mx.candidates);
  prefs.putUInt("th_hi", (uint32_t)(mx.totalHashes >> 32));
  prefs.putUInt("th_lo", (uint32_t)(mx.totalHashes & 0xFFFFFFFFULL));
}

static void probeSd() {
  if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3)) {
    Serial.println("[PEA4/SD] pin map failed");
    sdOk = false;
    return;
  }
  sdOk = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5);
  Serial.printf("[PEA4/SD] ok=%d type=%u sizeMB=%llu\n", sdOk ? 1 : 0, sdOk ? SD_MMC.cardType() : 0, sdOk ? (SD_MMC.cardSize() / (1024ULL * 1024ULL)) : 0ULL);
}

static void drawHeader(const char *title) {
  fillRect(0, 0, LCD_H_RES, 64, rgb565(9, 15, 24));
  fillRect(0, 62, LCD_H_RES, 2, C_CYAN);
  drawText(16, 14, title, C_TEXT, 3);
  drawTextf(320, 18, C_DIM, 2, "H%lu B%u", (unsigned long)mx.hashRate, mx.bestBits);
}

static void drawFooter() {
  fillRect(0, LCD_V_RES - 42, LCD_H_RES, 42, rgb565(8, 10, 14));
  fillRect(0, LCD_V_RES - 42, LCD_H_RES, 1, C_LINE);
  drawTextf(14, LCD_V_RES - 29, C_DIM, 1, "observer=%d submit=0 sd=%d psram=%luK", PEA4_OBSERVER_ONLY, sdOk ? 1 : 0, (unsigned long)(ESP.getFreePsram() / 1024));
  drawTextf(324, LCD_V_RES - 29, C_AMBER, 1, "seq=%lu", (unsigned long)mx.seq);
}

static void tile(int x, int y, int w, int h, const char *name, const char *sub, uint16_t accent) {
  fillRect(x, y, w, h, C_PANEL);
  drawRect(x, y, w, h, C_LINE);
  fillRect(x, y, 6, h, accent);
  drawText(x + 18, y + 18, name, C_TEXT, 2);
  drawText(x + 18, y + 48, sub, C_DIM, 1);
  drawCircle(x + w - 34, y + 34, 18, accent);
  drawLine(x + w - 46, y + 34, x + w - 22, y + 34, accent);
  drawLine(x + w - 34, y + 22, x + w - 34, y + 46, accent);
}

static void drawHome() {
  clear();
  drawHeader("JANUS PEA4");
  drawText(18, 82, "P4 TITAN SHELL / STOCK-SAFE BRINGUP", C_AMBER, 2);
  tile(18, 126, 210, 132, "CAMERA", "vendor IDF pipeline", C_CYAN);
  tile(252, 126, 210, 132, "SWARM", "PN cortex / nodes", C_GREEN);
  tile(18, 282, 210, 132, "TITAN", "sha probe / memory", C_BLUE);
  tile(252, 282, 210, 132, "AUDIO", "ES8311 pins mapped", C_AMBER);
  tile(18, 438, 210, 132, "CORPUS", "SD/FATFS archive", rgb565(180, 130, 255));
  tile(252, 438, 210, 132, "SETTINGS", "display / touch / restore", rgb565(255, 120, 90));
  drawText(24, 606, "ROLE: DISPLAY + ARCHIVE + CAMERA PREPROCESS + NAS EDGE", C_TEXT, 1);
  drawText(24, 628, "BUZZ STAYS POOL MASTER. PEA4 DOES NOT SUBMIT SHARES.", C_DIM, 1);
  drawFooter();
}

static void drawCamera() {
  clear();
  drawHeader("CAMERA");
  fillRect(26, 100, 428, 320, rgb565(4, 8, 12));
  drawRect(26, 100, 428, 320, C_CYAN);
  for (int i = 0; i < 80; i++) {
    int x = 36 + (i * 41 + mx.seq * 7) % 408;
    int y = 112 + (i * 73 + mx.bestBits * 9) % 292;
    setPx(x, y, rgb565(40 + (i * 3) % 120, 180, 210));
    setPx(x + 1, y, C_CYAN);
  }
  drawText(42, 450, "CAMERA STOCK FUNCTION: PRESERVED IN BACKUP", C_TEXT, 2);
  drawText(42, 486, "ARDUINO V0.1: UI SLOT + SENSOR PIPELINE RESERVED", C_DIM, 1);
  drawText(42, 508, "NEXT: PORT VENDOR ESP-IDF BROOKESIA CAMERA APP", C_DIM, 1);
  drawFooter();
}

static void drawSwarm() {
  clear();
  drawHeader("SWARM");
  drawText(30, 92, "PN CORTEX BRIDGE", C_AMBER, 2);
  drawTextf(30, 132, C_TEXT, 2, "node=%s role=%s", PEA4_NODE_ID, PEA4_ROLE);
  drawTextf(30, 172, C_TEXT, 2, "hash=%lu best=%u/%u", (unsigned long)mx.hashRate, mx.bestBits, mx.targetBits);
  drawTextf(30, 212, C_DIM, 2, "janus_io=%.2f tr=%.2f", mx.janusIo, mx.tranception);
  drawTextf(30, 252, C_DIM, 2, "love=%.2f+i%.2f ph=%.2f", mx.loveReal, mx.loveImag, mx.lovePhase);
  drawText(30, 318, "PEA4 listens, renders, stores and preprocesses.", C_TEXT, 1);
  drawText(30, 340, "No target mutation. No extra pool pressure.", C_DIM, 1);
  drawFooter();
}

static void drawTitan() {
  clear();
  drawHeader("TITAN");
  drawTextf(28, 96, C_TEXT, 2, "boots=%lu uptime=%lus", (unsigned long)mx.bootCount, (unsigned long)((millis() - bootMs) / 1000));
  drawTextf(28, 136, C_TEXT, 2, "total=%llu", (unsigned long long)mx.totalHashes);
  drawTextf(28, 176, C_TEXT, 2, "best=%u nonce=%08lX", mx.bestBits, (unsigned long)mx.bestNonce);
  drawTextf(28, 216, C_TEXT, 2, "candidates=%lu", (unsigned long)mx.candidates);
  drawTextf(28, 256, C_DIM, 2, "heap=%luK psram=%luK", (unsigned long)(ESP.getFreeHeap() / 1024), (unsigned long)(ESP.getFreePsram() / 1024));
  fillRect(28, 326, 424, 28, rgb565(30, 28, 18));
  int bar = min(424, (int)(mx.bestBits * 424 / 40));
  fillRect(28, 326, bar, 28, C_AMBER);
  drawRect(28, 326, 424, 28, C_LINE);
  drawText(28, 390, "TRANCEPTION-LITE: RETRIEVAL SCORE ONLY", C_DIM, 1);
  drawText(28, 412, "JANUS-IO: STRUCTURED OBSERVER TELEMETRY", C_DIM, 1);
  drawFooter();
}

static void drawAudio() {
  clear();
  drawHeader("AUDIO");
  drawText(30, 100, "ES8311 AUDIO MAP", C_AMBER, 2);
  drawTextf(30, 146, C_TEXT, 2, "BCK=%d WS=%d DO=%d DI=%d", I2S_BCK_IO, I2S_WS_IO, I2S_DO_IO, I2S_DI_IO);
  drawTextf(30, 186, C_TEXT, 2, "MCK=%d PA=%d ADDR=0x%02X", I2S_MCK_IO, ES8311_PA, ES8311_ADDR);
  drawText(30, 248, "V0.1 DOES NOT START CODEC PLAYBACK.", C_DIM, 1);
  drawText(30, 270, "REASON: SHELL BRINGUP FIRST, NO SD CARD YET.", C_DIM, 1);
  drawText(30, 316, "NEXT: MP3 PLAYER TILE FROM VENDOR EXAMPLE.", C_TEXT, 1);
  drawFooter();
}

static void drawGallery() {
  clear();
  drawHeader("CORPUS");
  drawTextf(30, 100, C_TEXT, 2, "SD_MMC=%s", sdOk ? "OK" : "NO CARD");
  drawTextf(30, 142, C_TEXT, 2, "pins D0-D3=%d,%d,%d,%d", SD_D0, SD_D1, SD_D2, SD_D3);
  drawTextf(30, 182, C_TEXT, 2, "CMD=%d CLK=%d", SD_CMD, SD_CLK);
  drawText(30, 242, "FUTURE PATHS:", C_AMBER, 2);
  drawText(46, 290, "/janus/corpus", C_DIM, 2);
  drawText(46, 326, "/janus/camera", C_DIM, 2);
  drawText(46, 362, "/janus/titan", C_DIM, 2);
  drawFooter();
}

static void drawSettings() {
  clear();
  drawHeader("SETTINGS");
  drawTextf(28, 98, C_TEXT, 2, "chip=%s rev=%u", ESP.getChipModel(), ESP.getChipRevision());
  drawTextf(28, 138, C_TEXT, 2, "flash=%luMB sketch=%luK", (unsigned long)(ESP.getFlashChipSize() / 1024 / 1024), (unsigned long)(ESP.getSketchSize() / 1024));
  drawTextf(28, 178, C_TEXT, 2, "display=%d touch=%d sd=%d", displayOk ? 1 : 0, touchOk ? 1 : 0, sdOk ? 1 : 0);
  drawTextf(28, 218, C_TEXT, 2, "backlight=%u", brightnessStep);
  drawText(28, 280, "STOCK RESTORE IMAGES:", C_AMBER, 2);
  drawText(28, 318, "P4_STOCK_BACKUP_20260617_015417", C_DIM, 1);
  drawText(28, 352, "P4 STOCK FUNCTIONS ARE SAFE AS FLASH BACKUP.", C_DIM, 1);
  drawText(28, 374, "JANUS SHELL REBUILDS THEM STEP BY STEP.", C_DIM, 1);
  drawFooter();
}

static void drawPage() {
  switch (page) {
    case PAGE_CAMERA: drawCamera(); break;
    case PAGE_SWARM: drawSwarm(); break;
    case PAGE_TITAN: drawTitan(); break;
    case PAGE_AUDIO: drawAudio(); break;
    case PAGE_GALLERY: drawGallery(); break;
    case PAGE_SETTINGS: drawSettings(); break;
    default: drawHome(); break;
  }
  flush();
  needRedraw = false;
}

static void handleHomeTap(uint16_t x, uint16_t y) {
  const int xs[2] = {18, 252};
  const int ys[3] = {126, 282, 438};
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 2; col++) {
      if (x >= xs[col] && x < xs[col] + 210 && y >= ys[row] && y < ys[row] + 132) {
        int idx = row * 2 + col;
        page = (AppPage)(idx + 1);
        needRedraw = true;
        return;
      }
    }
  }
}

static void handleTouch() {
  uint16_t x = 0, y = 0;
  bool down = touchOk && touch.getTouch(&x, &y);
  uint32_t now = millis();
  if (down && !wasTouched) touchDownMs = now;
  if (!down && wasTouched) {
    if (now - touchDownMs > 30) {
      if (page != PAGE_HOME && y < 90) {
        page = PAGE_HOME;
        needRedraw = true;
      } else if (page == PAGE_HOME) {
        handleHomeTap(x, y);
      } else if (page == PAGE_SETTINGS && y > 200 && y < 260) {
        brightnessStep = brightnessStep ? 0 : 1;
        lcd.example_bsp_set_lcd_backlight(brightnessStep);
        needRedraw = true;
      }
    }
  }
  wasTouched = down;
}

static void printStatus() {
  Serial.printf("[PEA4/SHELL] v=%s node=%s role=%s page=%d H=%lu best=%u/%u cand=%lu sd=%d touch=%d psram=%luK obs=%d submit=0 io=%.2f tr=%.2f love=%.2f+i%.2f ph=%.2f\n",
                PEA4_VERSION, PEA4_NODE_ID, PEA4_ROLE, (int)page,
                (unsigned long)mx.hashRate, mx.bestBits, mx.targetBits,
                (unsigned long)mx.candidates, sdOk ? 1 : 0, touchOk ? 1 : 0,
                (unsigned long)(ESP.getFreePsram() / 1024), PEA4_OBSERVER_ONLY,
                mx.janusIo, mx.tranception, mx.loveReal, mx.loveImag, mx.lovePhase);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  bootMs = millis();
  seedHeader();
  loadState();

  Serial.printf("[PEA4/SHELL] boot v=%s node=%s role=%s kind=%s observer_only=%d\n", PEA4_VERSION, PEA4_NODE_ID, PEA4_ROLE, PEA4_KIND, PEA4_OBSERVER_ONLY);
  Serial.printf("[PEA4/HW] chip=%s rev=%u flash=%lu psram=%lu stock_backup=P4_STOCK_BACKUP_20260617_015417\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                (unsigned long)ESP.getFlashChipSize(), (unsigned long)ESP.getPsramSize());

  fb = (uint16_t *)heap_caps_malloc((size_t)LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!fb) {
    Serial.println("[PEA4/DISPLAY] framebuffer allocation failed; enable PSRAM");
    return;
  }

  i2c_master_bus_handle_t i2cHandle = NULL;
  i2c_master_bus_config_t i2cBusConf = {
    .i2c_port = I2C_NUM_1,
    .sda_io_num = (gpio_num_t)TP_I2C_SDA,
    .scl_io_num = (gpio_num_t)TP_I2C_SCL,
    .clk_source = I2C_CLK_SRC_DEFAULT,
    .glitch_ignore_cnt = 7,
    .intr_priority = 0,
    .trans_queue_depth = 0,
    .flags = {
      .enable_internal_pullup = 1,
    },
  };
  esp_err_t i2cErr = i2c_new_master_bus(&i2cBusConf, &i2cHandle);
  Serial.printf("[PEA4/I2C] bus=%d\n", (int)i2cErr);

  lcd.begin();
  displayOk = true;
  touch.begin();
  touchOk = true;
  probeSd();

  Serial.println("[PEA4/SHELL] stock-like Janus launcher ready");
  needRedraw = true;
  lastMinerRateMs = millis();
}

void loop() {
  if (!displayOk || !fb) {
    delay(1000);
    return;
  }

  runMinerSlice(1800);
  updateRates();
  handleTouch();

  uint32_t now = millis();
  if (needRedraw || now - lastUiMs > 1000) {
    drawPage();
    lastUiMs = now;
  }
  if (now - lastStatusMs > 5000) {
    printStatus();
    lastStatusMs = now;
  }
  if (now - lastStateSaveMs > 30000) {
    saveState();
    lastStateSaveMs = now;
  }
}
