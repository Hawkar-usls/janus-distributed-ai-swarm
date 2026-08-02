/*
  Golcron / Holocron Astrolabe v1.5 BH Pixel Cosmos + Adaptive Unseen Paths

  11th Janus swarm participant: LILYGO/TTGO T-Display ESP32 charm worker.

  Board profile for the small board from the photo:
  - Arduino IDE board: ESP32 Dev Module
  - Display: onboard 1.14" color ST7789 TFT, 135x240 portrait, SPI
  - Buttons: GPIO0 and GPIO35
  - Backlight: GPIO4, steady digital control only
  - PSRAM: disabled
  - Upload: UART / USB serial, 115200 first

  Role:
  - receives Buzz J/B jobs over ESP-NOW
  - scans nonce space as non-overlapping untouched slices
  - autonomously chooses each next path from real lane history without revisiting checked nonces
  - submits only valid S/2 share candidates back to Buzz
  - emits JANUS + S/S presence so Buzz can see it as a real swarm member
  - shows a color Star Forge / astrolabe field on the onboard TFT

  Math rule:
  The adaptive star map only changes traversal order inside untouched slices.
  Every slice is disjoint and every local stride is coprime, so checked nonces are not repeated.
  It does not change block
  header bytes except nonce, target bytes, hash function, or pool semantics.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#if __has_include(<esp_arduino_version.h>)
  #include <esp_arduino_version.h>
#endif
#include <mbedtls/sha256.h>
#include <SPI.h>
#include <pgmspace.h>
#include <Preferences.h>

#define ST7789_W 135
#define ST7789_H 240
#define TFT_WIDTH 135
#define TFT_HEIGHT 240
#define TFT_MISO -1
#define TFT_MOSI 19
#define TFT_SCLK 18
#define TFT_CS 5
#define TFT_DC 16
#define TFT_RST 23
#define TFT_BL 4
#define TFT_BACKLIGHT_ON HIGH
#define SPI_FREQUENCY 40000000
#define TFT_BLACK 0x0000
#define TFT_CYAN 0x07FF
#define TFT_TRANSPARENT 0xFFFF
#define TL_DATUM 0

#ifndef ESP_ARDUINO_VERSION_MAJOR
  #define ESP_ARDUINO_VERSION_MAJOR 0
#endif

#define GOLCRON_VERSION "v1.5-bh-pixel-cosmos-controls"
#define GOLCRON_NODE_ID "Golcron"
#define GOLCRON_DISPLAY_NAME "HOLOCRON ASTROLABE"
#define GOLCRON_ROLE "ASTROLABE"
#define JANUS_SWARM_CHANNEL 1
#define JANUS_ENABLE_CHANNEL_SCAN 1
#define SERIAL_BAUD 115200

#define GOLCRON_BACKLIGHT_PIN TFT_BL
#define JEDI_LED_PIN 2
#define JEDI_LED_ACTIVE_HIGH 1
#define JEDI_LED_CHANNEL 0
#define JEDI_LED_PWM_FREQ 5000
#define JEDI_LED_PWM_BITS 8
#define GOLCRON_BUTTON_A_PIN 0
#define GOLCRON_BUTTON_B_PIN 35
#define GOLCRON_BUTTON_ACTIVE_LOW 1
#define GOLCRON_BUTTON_TAP_MIN_MS 35UL
#define GOLCRON_FRAME_HOLD_MS 1200UL
#define STATUS_MS 2000UL
#define DISPLAY_MS 33UL
#define JEDI_LED_MS 24UL
#define JOB_TTL_MS 6500UL
#define RX_QUEUE_DEPTH 6
#define GOLCRON_MIN_BATCH 40
#define GOLCRON_MAX_BATCH 1200
#define STAR_FORGE_LANES 5
#define STAR_FORGE_STRONG_BITS 16
#define STAR_FORGE_SLICE_SMALL 2048UL
#define STAR_FORGE_SLICE_NORMAL 8192UL
#define STAR_FORGE_SLICE_LARGE 16384UL
#define STAR_FORGE_SLICE_HUGE 32768UL

static const uint8_t JANUS_SCAN_CHANNELS[] = {1, 6, 10};
static const uint8_t JANUS_SCAN_CHANNEL_COUNT = sizeof(JANUS_SCAN_CHANNELS) / sizeof(JANUS_SCAN_CHANNELS[0]);
static const uint8_t JEDI_PROBE_PINS[] = {12, 13, 14, 15, 17, 21, 22, 25, 26, 27, 32, 33};
static const uint8_t JEDI_PROBE_PIN_COUNT = sizeof(JEDI_PROBE_PINS) / sizeof(JEDI_PROBE_PINS[0]);
static const char *STAR_FORGE_NAMES[] = {"DAWN", "ORBIT", "KYBER", "MERCY", "GROGU"};

static const uint8_t FONT5X7[96][5] PROGMEM = {
  {0,0,0,0,0},{0,0,95,0,0},{0,7,0,7,0},{20,127,20,127,20},{36,42,127,42,18},{35,19,8,100,98},{54,73,86,32,80},{0,8,7,3,0},
  {0,28,34,65,0},{0,65,34,28,0},{42,28,127,28,42},{8,8,62,8,8},{0,128,112,48,0},{8,8,8,8,8},{0,0,96,96,0},{32,16,8,4,2},
  {62,81,73,69,62},{0,66,127,64,0},{114,73,73,73,70},{33,65,73,77,51},{24,20,18,127,16},{39,69,69,69,57},{60,74,73,73,49},{65,33,17,9,7},
  {54,73,73,73,54},{70,73,73,41,30},{0,0,20,0,0},{0,64,52,0,0},{0,8,20,34,65},{20,20,20,20,20},{0,65,34,20,8},{2,1,89,9,6},
  {62,65,93,89,78},{124,18,17,18,124},{127,73,73,73,54},{62,65,65,65,34},{127,65,65,65,62},{127,73,73,73,65},{127,9,9,9,1},{62,65,65,81,115},
  {127,8,8,8,127},{0,65,127,65,0},{32,64,65,63,1},{127,8,20,34,65},{127,64,64,64,64},{127,2,28,2,127},{127,4,8,16,127},{62,65,65,65,62},
  {127,9,9,9,6},{62,65,81,33,94},{127,9,25,41,70},{38,73,73,73,50},{3,1,127,1,3},{63,64,64,64,63},{31,32,64,32,31},{63,64,56,64,63},
  {99,20,8,20,99},{3,4,120,4,3},{97,89,73,77,67},{0,127,65,65,0},{2,4,8,16,32},{0,65,65,127,0},{4,2,1,2,4},{64,64,64,64,64},
  {0,3,7,8,0},{32,84,84,120,64},{127,40,68,68,56},{56,68,68,68,40},{56,84,84,84,24},{0,8,126,9,2},{24,164,164,156,120},
  {127,8,4,4,120},{0,68,125,64,0},{32,64,64,61,0},{127,16,40,68,0},{0,65,127,64,0},{124,4,120,4,120},{124,8,4,4,120},{56,68,68,68,56},
  {252,24,36,36,24},{24,36,36,24,252},{124,8,4,4,8},{72,84,84,84,36},{4,4,63,68,36},{60,64,64,32,124},{28,32,64,32,28},{60,64,48,64,60},{68,40,16,40,68},{76,144,144,144,124},{68,100,84,76,68},
  {0,8,54,65,0},{0,0,119,0,0},{0,65,54,8,0},{2,1,2,4,2},{124,18,17,18,124}
};

static inline uint16_t directColor565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

struct TftProfile { uint8_t madctl; uint8_t xOffset; uint8_t yOffset; const char *name; };
static const TftProfile TFT_PROFILES[] = {
  {0xC8, 52, 40, "PORT_USB_TOP_BGR"},
  {0x08, 52, 40, "PORT_USB_BOTTOM_BGR"},
  {0xC0, 52, 40, "PORT_USB_TOP_RGB"},
  {0x00, 52, 40, "PORT_USB_BOTTOM_RGB"},
  {0x68, 40, 52, "LAND_BGR_40_52"},
};
static const uint8_t TFT_PROFILE_COUNT = sizeof(TFT_PROFILES) / sizeof(TFT_PROFILES[0]);
static uint8_t tftProfileIndex = 0;
static TftProfile tftProfile = TFT_PROFILES[0];

static void tftWriteCommand(uint8_t cmd) { digitalWrite(TFT_DC, LOW); digitalWrite(TFT_CS, LOW); SPI.transfer(cmd); digitalWrite(TFT_CS, HIGH); }
static void tftWriteData8(uint8_t data) { digitalWrite(TFT_DC, HIGH); digitalWrite(TFT_CS, LOW); SPI.transfer(data); digitalWrite(TFT_CS, HIGH); }
static void tftSetWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  uint16_t x0 = x + tftProfile.xOffset, y0 = y + tftProfile.yOffset;
  uint16_t x1 = x0 + w - 1, y1 = y0 + h - 1;
  tftWriteCommand(0x2A); digitalWrite(TFT_DC, HIGH); digitalWrite(TFT_CS, LOW);
  SPI.transfer(x0 >> 8); SPI.transfer(x0 & 0xFF); SPI.transfer(x1 >> 8); SPI.transfer(x1 & 0xFF); digitalWrite(TFT_CS, HIGH);
  tftWriteCommand(0x2B); digitalWrite(TFT_DC, HIGH); digitalWrite(TFT_CS, LOW);
  SPI.transfer(y0 >> 8); SPI.transfer(y0 & 0xFF); SPI.transfer(y1 >> 8); SPI.transfer(y1 & 0xFF); digitalWrite(TFT_CS, HIGH);
  tftWriteCommand(0x2C);
}
static void tftPushPixels(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels) {
  if (!pixels || !w || !h) return;
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0)); tftSetWindow(x, y, w, h);
  digitalWrite(TFT_DC, HIGH); digitalWrite(TFT_CS, LOW);
  for (uint32_t i = 0, n = (uint32_t)w * h; i < n; i++) { uint16_t c = pixels[i]; SPI.transfer(c >> 8); SPI.transfer(c & 0xFF); }
  digitalWrite(TFT_CS, HIGH); SPI.endTransaction();
}
static void tftFillScreenRaw(uint16_t color) {
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0)); tftSetWindow(0, 0, ST7789_W, ST7789_H);
  digitalWrite(TFT_DC, HIGH); digitalWrite(TFT_CS, LOW);
  for (uint32_t i = 0; i < (uint32_t)ST7789_W * ST7789_H; i++) { SPI.transfer(color >> 8); SPI.transfer(color & 0xFF); }
  digitalWrite(TFT_CS, HIGH); SPI.endTransaction();
}
static void tftDirectInit() {
  pinMode(TFT_CS, OUTPUT); pinMode(TFT_DC, OUTPUT); pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_CS, HIGH); digitalWrite(TFT_DC, HIGH); SPI.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, TFT_CS);
  digitalWrite(TFT_RST, HIGH); delay(10); digitalWrite(TFT_RST, LOW); delay(40); digitalWrite(TFT_RST, HIGH); delay(140);
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0)); tftWriteCommand(0x01); SPI.endTransaction(); delay(150);
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0)); tftWriteCommand(0x11); SPI.endTransaction(); delay(150);
  SPI.beginTransaction(SPISettings(SPI_FREQUENCY, MSBFIRST, SPI_MODE0)); tftWriteCommand(0x3A); tftWriteData8(0x55);
  tftWriteCommand(0x36); tftWriteData8(tftProfile.madctl); tftWriteCommand(0x21); tftWriteCommand(0x13); tftWriteCommand(0x29); SPI.endTransaction(); delay(80);
}

class MiniTftSprite {
public:
  bool createSprite(int w, int h) { width = w; height = h; fb = (uint16_t *)malloc((size_t)w * h * sizeof(uint16_t)); return fb != nullptr; }
  void setColorDepth(uint8_t) {}
  uint16_t color565(uint8_t r, uint8_t g, uint8_t b) { return directColor565(r, g, b); }
  void setTextDatum(uint8_t) {}
  void setTextFont(uint8_t) {}
  void setTextColor(uint16_t fg, uint16_t bg) { textFg = fg; textBg = bg; }
  void fillSprite(uint16_t c) { fillRect(0, 0, width, height, c); }
  void pushSprite(int x, int y) { tftPushPixels(x, y, width, height, fb); }
  void drawPixel(int x, int y, uint16_t c) { if (!fb || x < 0 || y < 0 || x >= width || y >= height) return; fb[y * width + x] = c; }
  void drawFastHLine(int x, int y, int w, uint16_t c) { fillRect(x, y, w, 1, c); }
  void fillRect(int x, int y, int w, int h, uint16_t c) {
    if (!fb || w <= 0 || h <= 0) return;
    int x0 = max(0, x), y0 = max(0, y), x1 = min(width, x + w), y1 = min(height, y + h);
    for (int yy = y0; yy < y1; yy++) { uint16_t *row = fb + yy * width + x0; for (int xx = x0; xx < x1; xx++) *row++ = c; }
  }
  void drawRect(int x, int y, int w, int h, uint16_t c) { fillRect(x, y, w, 1, c); fillRect(x, y + h - 1, w, 1, c); fillRect(x, y, 1, h, c); fillRect(x + w - 1, y, 1, h, c); }
  void drawRoundRect(int x, int y, int w, int h, int, uint16_t c) { drawRect(x, y, w, h, c); }
  void fillRoundRect(int x, int y, int w, int h, int, uint16_t c) { fillRect(x, y, w, h, c); }
  void drawLine(int x0, int y0, int x1, int y1, uint16_t c) {
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    while (true) { drawPixel(x0, y0, c); if (x0 == x1 && y0 == y1) break; int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; } }
  }
  void drawCircle(int cx, int cy, int r, uint16_t c) {
    int x = -r, y = 0, err = 2 - 2 * r;
    do { drawPixel(cx - x, cy + y, c); drawPixel(cx - y, cy - x, c); drawPixel(cx + x, cy - y, c); drawPixel(cx + y, cy + x, c); int e2 = err; if (e2 <= y) err += ++y * 2 + 1; if (e2 > x || err > y) err += ++x * 2 + 1; } while (x < 0);
  }
  void fillCircle(int cx, int cy, int r, uint16_t c) { for (int y = -r; y <= r; y++) for (int x = -r; x <= r; x++) if (x * x + y * y <= r * r) drawPixel(cx + x, cy + y, c); }
  void drawChar(int x, int y, char ch) {
    if (ch < 32 || ch > 127) ch = '?'; if (ch >= 'a' && ch <= 'z') ch = ch - 'a' + 'A';
    const uint8_t *glyph = FONT5X7[ch - 32];
    for (int col = 0; col < 5; col++) { uint8_t bits = pgm_read_byte(&glyph[col]); for (int row = 0; row < 7; row++) { bool on = bits & (1 << row); if (on) drawPixel(x + col, y + row, textFg); else if (textBg != TFT_TRANSPARENT) drawPixel(x + col, y + row, textBg); } }
  }
  void drawString(const char *s, int x, int y) { int cx = x; while (*s) { drawChar(cx, y, *s++); cx += 6; } }
private:
  uint16_t *fb = nullptr; int width = 0; int height = 0; uint16_t textFg = 0xFFFF; uint16_t textBg = TFT_TRANSPARENT;
};

static MiniTftSprite charm;
static bool displayOk = false, charmReady = false, jediLedOk = false;
static int8_t jediProbePin = -1;
static bool kyberShield = false, frameVisible = true, buttonAWasDown = false, buttonBWasDown = false, buttonBLongHandled = false;
static uint32_t buttonADownMs = 0, buttonBDownMs = 0;
static uint8_t charmView = 0;
static uint32_t overlayUntilMs = 0;
static char overlayLine[48] = "PIXEL COSMOS ONLINE";
static uint32_t visualTickMs = 0;
static float visualPhase = 0.0f, orbitPhase = 0.0f, dustPhase = 0.0f, smoothForgeEnergy = 0.0f, smoothForgeHeat = 0.0f, smoothHashRateK = 0.0f;
static uint32_t displayFrames = 0, displayLastFpsMs = 0, lastRenderUs = 0, maxRenderUs = 0, loopJitterUs = 0, loopMaxUs = 0, lastLoopUs = 0;
static uint16_t displayFps = 0;
static float cosmosPulse = 0.0f;
static uint32_t visualLastJobSeq = 0, visualLastBestBits = 0;
static const uint8_t JANUS_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

struct __attribute__((packed)) JanusColonyPacket { char magic[6]; char nodeId[24]; char role[12]; uint32_t seq; uint32_t hashRate; uint32_t shares; uint32_t rejects; uint32_t bestBits; float diff; uint16_t targetBits; uint16_t aiBatch; uint8_t aiHint; uint32_t jobAgeMs; int8_t rssi; uint32_t uptime; };
struct __attribute__((packed)) JobPacket { uint8_t magic[2]; uint8_t job_id[8]; uint8_t header[80]; uint32_t start_nonce; uint32_t range_size; uint8_t target[32]; uint32_t extranonce2; };
struct __attribute__((packed)) ShareResponseV2 { uint8_t magic[2]; uint8_t job_id[8]; uint32_t nonce; uint16_t worker_id; uint16_t bits; uint32_t total_hashes_l32; uint8_t hash_tail[4]; };
struct __attribute__((packed)) SwarmSensePacket {
  uint8_t magic[2]; uint8_t version; uint16_t worker_id; char nodeId[24]; char kind[16]; uint32_t seq; uint32_t uptime_ms; uint32_t micros_tail; uint32_t free_heap;
  uint16_t loop_jitter_us; uint16_t loop_max_us; int8_t rssi; uint8_t radio_mode; uint8_t bt_flags; uint8_t palette; uint8_t knn_label; uint8_t knn_confidence;
  uint8_t ai_hint; uint8_t thermal_load; uint16_t effective_batch; uint16_t dynamic_batch;
