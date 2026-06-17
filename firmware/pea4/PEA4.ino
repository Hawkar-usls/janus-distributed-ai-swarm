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
#include <esp_mac.h>
#include <esp_timer.h>
#include "driver/i2c_master.h"
#include "pins_config.h"
#include "src/lcd/st7701_lcd.h"
#include "src/touch/gt911_touch.h"

#define PEA4_VERSION "v0.2D-camera-presence"
#define PEA4_NODE_ID "PEA4"
#define PEA4_ROLE "P4_TITAN"
#define PEA4_KIND "p4_titan_shell_dual_swarm_core"
#define PEA4_OBSERVER_ONLY 1

#define JP4_FRAME_MS 500UL
#define PEA4_PRESENCE_MS 2000UL
#define JP4_STATE_SAVE_MS 30000UL
#define JP4_PEER_TIMEOUT_MS 5000UL
#define JP4_JOB_TIMEOUT_MS 30000UL
#define JP4_PROOF_Z_THRESHOLD 24
#define JP4_ANCHOR_RING 24
#define JP4_VERIFY_QUEUE 8

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
static i2c_master_bus_handle_t touchBusHandle = NULL;

static uint16_t *fb = nullptr;
static bool displayOk = false;
static bool touchOk = false;
static bool sdOk = false;
static uint8_t brightnessStep = 1;

static const char *CAMERA_DRIVER_STACK = "ESP-IDF esp_video / MIPI-CSI";
static const char *CAMERA_SENSOR_HINT = "SC2336-class CSI module";
static const char *CAMERA_PORT_STATE = "IDF_PORT_REQUIRED";
static bool cameraReady = false;
static uint32_t cameraFrames = 0;
static uint32_t cameraLastFrameMs = 0;

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
static uint32_t lastJp4FrameMs = 0;
static uint32_t lastPresenceMs = 0;
static uint32_t bootMs = 0;
static uint32_t touchDownMs = 0;
static bool wasTouched = false;
static uint16_t touchStartX = 0;
static uint16_t touchStartY = 0;
static uint16_t lastTouchX = 0;
static uint16_t lastTouchY = 0;
static uint32_t lastTouchLogMs = 0;
static uint32_t touchEvents = 0;
static uint32_t lastNavMs = 0;
static bool touchActionTaken = false;

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

enum P4CoreRole : uint8_t {
  CORE_P4_A = 0,
  CORE_P4_B = 1,
  CORE_P4_C = 2
};

enum P4CoreMode : uint8_t {
  CORE_AUTO = 0,
  CORE_HASH = 1,
  CORE_SCOUT = 2,
  CORE_MIRROR = 3,
  CORE_VERIFY = 4,
  CORE_RELAY = 5,
  CORE_IDLE = 6
};

struct P4HashInput {
  uint32_t magic;
  uint32_t schema;
  uint32_t jobSeed;
  uint32_t salt;
  uint32_t seq;
  uint32_t nonce;
  uint8_t nodeMac[6];
  char roleTag[8];
  uint8_t reserved[18];
};

struct P4Candidate {
  bool valid = false;
  char nodeId[13] = {0};
  char role[8] = {0};
  uint32_t z = 0;
  uint32_t seq = 0;
  uint32_t nonce = 0;
  uint32_t salt = 0;
  uint32_t jobSeed = 0;
  uint8_t digest[32] = {0};
  uint32_t seenMs = 0;
};

struct P4Peer {
  bool seen = false;
  char nodeId[13] = {0};
  char role[8] = {0};
  char mode[10] = {0};
  uint32_t seq = 0;
  uint32_t nonce = 0;
  uint32_t salt = 0;
  uint32_t jobSeed = 0;
  uint32_t z = 0;
  float hps = 0.0f;
  uint8_t digest[32] = {0};
  uint32_t lastMs = 0;
};

struct P4Job {
  uint32_t seed = 0;
  uint32_t targetZ = 22;
  uint32_t startedMs = 0;
  uint32_t ttlMs = JP4_JOB_TIMEOUT_MS;
  char jobId[17] = "boot";
};

static TitanMetrics mx;
static P4CoreRole coreRole = CORE_P4_A;
static P4CoreMode coreMode = CORE_AUTO;
static P4Peer corePeer;
static P4Job coreJob;
static P4Candidate anchors[JP4_ANCHOR_RING];
static P4Candidate verifyQueue[JP4_VERIFY_QUEUE];
static uint8_t anchorHead = 0;
static uint8_t verifyHead = 0;
static uint8_t baseMac[6] = {0};
static uint8_t bestDigest[32] = {0};
static char coreNodeId[13] = "000000000000";
static char usbLine[256];
static size_t usbLineLen = 0;
static uint32_t coreSeq = 0;
static uint32_t coreNonce = 0;
static uint32_t coreSalt = 0;
static uint32_t extraSalt = 0;
static uint32_t localProofs = 0;
static uint32_t peerOk = 0;
static uint32_t peerBad = 0;
static uint32_t peerDrop = 0;
static uint32_t verifyOk = 0;
static uint32_t verifyFail = 0;
static uint32_t mirrorHits = 0;
static uint32_t bridgeTx = 0;
static uint32_t cmdCount = 0;
static uint32_t badCmdCount = 0;
static uint32_t stateLoads = 0;
static uint32_t stateSaves = 0;
static uint32_t i2cScanHits = 0;
static float intentionScore = 0.0f;
static float blackboardScore = 0.0f;

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

static uint32_t xorshift32(uint32_t x);

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

static void drawJanusBackdrop(const char *label = nullptr) {
  clear(rgb565(3, 7, 13));
  for (int y = 0; y < LCD_V_RES; y += 10) {
    uint8_t v = 8 + min(44, y / 20);
    fillRect(0, y, LCD_H_RES, 10, rgb565(2, 5 + v / 5, 12 + v / 2));
  }
  fillRect(0, 64, LCD_H_RES, 1, rgb565(28, 54, 68));
  fillRect(0, LCD_V_RES - 58, LCD_H_RES, 1, rgb565(28, 54, 68));
  for (uint16_t i = 0; i < 72; i++) {
    uint32_t h = xorshift32((uint32_t)i * 0x9E3779B9UL ^ coreJob.seed ^ (mx.seq * 17UL));
    int x = h % LCD_H_RES;
    int y = 82 + ((h >> 10) % (LCD_V_RES - 168));
    uint16_t c = (h & 0x80) ? rgb565(44, 112, 128) : rgb565(76, 82, 96);
    setPx(x, y, c);
    if ((h & 15) == 0) setPx(x + 1, y, c);
  }
  for (int i = 0; i < 9; i++) {
    int y = 112 + i * 56;
    uint16_t c = rgb565(8, 28 + i * 2, 38 + i * 3);
    fillRect(22 + i * 3, y, LCD_H_RES - 44 - i * 6, 1, c);
  }
  if (label) drawText(24, LCD_V_RES - 76, label, rgb565(58, 142, 160), 1);
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

static const char *roleName(P4CoreRole role) {
  switch (role) {
    case CORE_P4_A: return "P4_A";
    case CORE_P4_B: return "P4_B";
    case CORE_P4_C: return "P4_C";
    default: return "P4_X";
  }
}

static const char *modeName(P4CoreMode mode) {
  switch (mode) {
    case CORE_AUTO: return "AUTO";
    case CORE_HASH: return "HASH";
    case CORE_SCOUT: return "SCOUT";
    case CORE_MIRROR: return "MIRROR";
    case CORE_VERIFY: return "VERIFY";
    case CORE_RELAY: return "RELAY";
    case CORE_IDLE: return "IDLE";
    default: return "UNK";
  }
}

static P4CoreRole parseRoleName(const char *s) {
  if (!strcasecmp(s, "A") || !strcasecmp(s, "P4_A")) return CORE_P4_A;
  if (!strcasecmp(s, "B") || !strcasecmp(s, "P4_B")) return CORE_P4_B;
  if (!strcasecmp(s, "C") || !strcasecmp(s, "P4_C")) return CORE_P4_C;
  return coreRole;
}

static P4CoreMode parseModeName(const char *s) {
  if (!strcasecmp(s, "AUTO")) return CORE_AUTO;
  if (!strcasecmp(s, "HASH")) return CORE_HASH;
  if (!strcasecmp(s, "SCOUT")) return CORE_SCOUT;
  if (!strcasecmp(s, "MIRROR")) return CORE_MIRROR;
  if (!strcasecmp(s, "VERIFY")) return CORE_VERIFY;
  if (!strcasecmp(s, "RELAY")) return CORE_RELAY;
  if (!strcasecmp(s, "IDLE")) return CORE_IDLE;
  return coreMode;
}

static uint32_t fnv1a32Bytes(const uint8_t *data, size_t len) {
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < len; i++) {
    h ^= data[i];
    h *= 16777619UL;
  }
  return h;
}

static uint32_t fnv1a32Text(const char *s) {
  return fnv1a32Bytes((const uint8_t *)s, strlen(s));
}

static uint32_t xorshift32(uint32_t x) {
  if (!x) x = 0xA5A5F00DUL;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

static bool parseHex32(const char *s, uint32_t *out) {
  while (*s == ' ' || *s == '\t') s++;
  if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
  uint32_t v = 0;
  int n = 0;
  while (*s) {
    char c = *s++;
    uint8_t x;
    if (c >= '0' && c <= '9') x = c - '0';
    else if (c >= 'a' && c <= 'f') x = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') x = c - 'A' + 10;
    else break;
    v = (v << 4) | x;
    if (++n >= 8) break;
  }
  if (!n) return false;
  *out = v;
  return true;
}

static bool parseU32(const char *s, uint32_t *out) {
  if (!s || !*s) return false;
  char *endp = nullptr;
  unsigned long v = strtoul(s, &endp, 10);
  if (endp == s) return false;
  *out = (uint32_t)v;
  return true;
}

static void digestToHex(const uint8_t digest[32], char out[65]) {
  static const char *hex = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    out[i * 2] = hex[(digest[i] >> 4) & 0x0F];
    out[i * 2 + 1] = hex[digest[i] & 0x0F];
  }
  out[64] = '\0';
}

static bool hexToDigest(const char *hex, uint8_t digest[32]) {
  if (!hex || strlen(hex) < 64) return false;
  for (int i = 0; i < 32; i++) {
    char a = hex[i * 2];
    char b = hex[i * 2 + 1];
    uint8_t hi, lo;
    if (a >= '0' && a <= '9') hi = a - '0';
    else if (a >= 'a' && a <= 'f') hi = a - 'a' + 10;
    else if (a >= 'A' && a <= 'F') hi = a - 'A' + 10;
    else return false;
    if (b >= '0' && b <= '9') lo = b - '0';
    else if (b >= 'a' && b <= 'f') lo = b - 'a' + 10;
    else if (b >= 'A' && b <= 'F') lo = b - 'A' + 10;
    else return false;
    digest[i] = (hi << 4) | lo;
  }
  return true;
}

static bool nodeIdToMac(const char *id, uint8_t mac[6]) {
  if (!id || strlen(id) < 12) return false;
  char tmp[3] = {0, 0, 0};
  for (int i = 0; i < 6; i++) {
    tmp[0] = id[i * 2];
    tmp[1] = id[i * 2 + 1];
    uint32_t v = 0;
    if (!parseHex32(tmp, &v)) return false;
    mac[i] = (uint8_t)v;
  }
  return true;
}

static void sha256Bytes(const void *data, size_t len, uint8_t out[32]) {
  mbedtls_sha256((const unsigned char *)data, len, out, 0);
}

static uint8_t hashCandidate(const uint8_t mac[6], const char *role, uint32_t jobSeed, uint32_t salt, uint32_t seq, uint32_t nonce, uint8_t digest[32]) {
  P4HashInput input;
  memset(&input, 0, sizeof(input));
  input.magic = 0x4A503443UL;
  input.schema = 0x00010001UL;
  input.jobSeed = jobSeed;
  input.salt = salt;
  input.seq = seq;
  input.nonce = nonce;
  memcpy(input.nodeMac, mac, 6);
  strncpy(input.roleTag, role, sizeof(input.roleTag) - 1);
  sha256Bytes(&input, sizeof(input), digest);
  return (uint8_t)bitsFromHash(digest);
}

static void readBaseMac() {
  if (esp_read_mac(baseMac, ESP_MAC_BASE) != ESP_OK) {
    uint32_t a = esp_random();
    uint32_t b = esp_random();
    memcpy(baseMac, &a, 4);
    memcpy(baseMac + 4, &b, 2);
  }
  snprintf(coreNodeId, sizeof(coreNodeId), "%02X%02X%02X%02X%02X%02X",
           baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
}

static uint32_t peerAgeMs() {
  if (!corePeer.seen) return 0xFFFFFFFFUL;
  return millis() - corePeer.lastMs;
}

static void setJob(uint32_t seed, uint32_t targetZ, const char *jobId, bool resetBest = true) {
  coreJob.seed = seed;
  coreJob.targetZ = targetZ ? targetZ : JP4_PROOF_Z_THRESHOLD;
  coreJob.startedMs = millis();
  coreJob.ttlMs = JP4_JOB_TIMEOUT_MS;
  if (jobId && *jobId) {
    strncpy(coreJob.jobId, jobId, sizeof(coreJob.jobId) - 1);
    coreJob.jobId[sizeof(coreJob.jobId) - 1] = '\0';
  } else {
    snprintf(coreJob.jobId, sizeof(coreJob.jobId), "%08lx", (unsigned long)seed);
  }
  mx.targetBits = (uint16_t)coreJob.targetZ;
  if (resetBest) {
    mx.bestBits = 0;
    mx.bestNonce = 0;
    memset(bestDigest, 0, sizeof(bestDigest));
  }
}

static void rememberAnchor(bool local, bool verified, const char *nodeId, const char *role, uint32_t z, uint32_t seq, uint32_t nonce, uint32_t salt, uint32_t jobSeed, const uint8_t digest[32]) {
  P4Candidate *a = &anchors[anchorHead];
  anchorHead = (anchorHead + 1) % JP4_ANCHOR_RING;
  memset(a, 0, sizeof(*a));
  a->valid = true;
  strncpy(a->nodeId, nodeId, sizeof(a->nodeId) - 1);
  strncpy(a->role, role, sizeof(a->role) - 1);
  a->z = z;
  a->seq = seq;
  a->nonce = nonce;
  a->salt = salt;
  a->jobSeed = jobSeed;
  memcpy(a->digest, digest, 32);
  a->seenMs = millis();
  blackboardScore = blackboardScore * 0.85f + (float)z * (verified ? 0.20f : 0.12f) + (local ? 0.4f : 0.0f);
}

static void queueVerify(const P4Candidate &cand) {
  verifyQueue[verifyHead] = cand;
  verifyQueue[verifyHead].valid = true;
  verifyHead = (verifyHead + 1) % JP4_VERIFY_QUEUE;
}

static bool popVerify(P4Candidate *cand) {
  for (uint8_t i = 0; i < JP4_VERIFY_QUEUE; i++) {
    if (verifyQueue[i].valid) {
      *cand = verifyQueue[i];
      verifyQueue[i].valid = false;
      return true;
    }
  }
  return false;
}

static void verifyOneCandidate() {
  P4Candidate cand;
  if (!popVerify(&cand)) return;
  uint8_t mac[6], digest[32];
  if (!nodeIdToMac(cand.nodeId, mac)) {
    verifyFail++;
    return;
  }
  uint8_t z = hashCandidate(mac, cand.role, cand.jobSeed, cand.salt, cand.seq, cand.nonce, digest);
  bool ok = (z == cand.z) && memcmp(digest, cand.digest, 32) == 0;
  if (ok) {
    verifyOk++;
    if (z >= 16) mirrorHits++;
    rememberAnchor(false, true, cand.nodeId, cand.role, cand.z, cand.seq, cand.nonce, cand.salt, cand.jobSeed, cand.digest);
  } else {
    verifyFail++;
  }
}

static P4CoreMode activeWorkerMode() {
  if (coreMode != CORE_AUTO) return coreMode;
  if (coreRole == CORE_P4_A) return CORE_SCOUT;
  if (coreRole == CORE_P4_B) return (peerAgeMs() < JP4_PEER_TIMEOUT_MS) ? CORE_VERIFY : CORE_MIRROR;
  return CORE_RELAY;
}

static void emitProof(uint32_t z, uint32_t seq, uint32_t nonce, uint32_t salt, uint32_t jobSeed, const uint8_t digest[32]) {
  char digestHex[65];
  digestToHex(digest, digestHex);
  Serial.printf("{\"kind\":\"JANUS_P4_PROOF\",\"ver\":\"%s\",\"node\":\"%s\",\"node_hex\":\"%s\",\"role\":\"%s\",\"z\":%lu,\"seq\":%lu,\"nonce\":%lu,\"salt\":\"0x%08lX\",\"job_seed\":\"0x%08lX\",\"digest\":\"%s\"}\n",
                PEA4_VERSION, PEA4_NODE_ID, coreNodeId, roleName(coreRole),
                (unsigned long)z, (unsigned long)seq, (unsigned long)nonce,
                (unsigned long)salt, (unsigned long)jobSeed, digestHex);
}

static void runMinerSlice(uint32_t budgetUs) {
  uint32_t start = micros();
  uint8_t digest[32];
  P4CoreMode workMode = activeWorkerMode();

  if (workMode == CORE_VERIFY || workMode == CORE_MIRROR || workMode == CORE_RELAY) {
    verifyOneCandidate();
  }
  if (workMode == CORE_IDLE || workMode == CORE_RELAY) return;

  if (millis() - coreJob.startedMs > coreJob.ttlMs) {
    setJob(xorshift32(coreJob.seed ^ esp_random() ^ (uint32_t)esp_timer_get_time()), coreJob.targetZ, "local", false);
  }

  while ((uint32_t)(micros() - start) < budgetUs) {
    coreNonce++;
    if (!coreNonce) {
      coreSeq++;
      coreSalt = xorshift32(coreSalt ^ coreSeq ^ esp_random());
    }

    uint32_t modeSalt = coreSalt ^ extraSalt;
    uint32_t jobSeed = coreJob.seed;
    if ((workMode == CORE_MIRROR || workMode == CORE_VERIFY) && corePeer.seen) {
      modeSalt ^= corePeer.salt ^ 0xB16B00B5UL;
      jobSeed ^= corePeer.jobSeed ^ 0x51A7E11AUL;
    }

    uint8_t z = hashCandidate(baseMac, roleName(coreRole), jobSeed, modeSalt, coreSeq, coreNonce, digest);
    mx.windowHashes++;
    mx.totalHashes++;
    mx.nonce = coreNonce;

    if (z > mx.bestBits) {
      mx.bestBits = z;
      mx.bestNonce = coreNonce;
      memcpy(bestDigest, digest, sizeof(bestDigest));
      mx.candidates++;
      rememberAnchor(true, true, coreNodeId, roleName(coreRole), z, coreSeq, coreNonce, modeSalt, jobSeed, digest);
      if (z >= coreJob.targetZ || z >= JP4_PROOF_Z_THRESHOLD) {
        localProofs++;
        emitProof(z, coreSeq, coreNonce, modeSalt, jobSeed, digest);
      }
      needRedraw = true;
    }
    if (z >= coreJob.targetZ) mx.candidates++;
  }
}

static void updateMathSignals() {
  float bestNorm = min(1.0f, mx.bestBits / 32.0f);
  float hNorm = min(1.0f, mx.hashRate / 70000.0f);
  float memNorm = min(1.0f, log10f((float)mx.totalHashes + 10.0f) / 10.0f);
  float peerNorm = (corePeer.seen && peerAgeMs() < JP4_PEER_TIMEOUT_MS) ? min(1.0f, corePeer.z / 32.0f) : 0.0f;
  mx.janusIo = 0.45f * bestNorm + 0.35f * hNorm + 0.20f * memNorm;
  mx.tranception = 0.50f * bestNorm + 0.25f * peerNorm + 0.20f * memNorm + 0.05f * sinf(mx.seq * 0.11f);
  intentionScore = intentionScore * 0.92f + ((float)mx.bestBits * 0.55f + (float)corePeer.z * 0.25f + (peerNorm > 0.0f ? 8.0f : 0.0f)) * 0.08f;
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
  prefs.begin("pea4", false);
  mx.bootCount = prefs.getUInt("boots", 0) + 1;
  uint8_t role = prefs.getUChar("role", 255);
  uint8_t mode = prefs.getUChar("mode", 255);
  if (role <= CORE_P4_C) coreRole = (P4CoreRole)role;
  if (mode <= CORE_IDLE) coreMode = (P4CoreMode)mode;
  mx.bestBits = prefs.getUShort("best", 0);
  mx.bestNonce = prefs.getUInt("nonce", 0);
  mx.candidates = prefs.getUInt("cand", 0);
  uint32_t hi = prefs.getUInt("th_hi", 0);
  uint32_t lo = prefs.getUInt("th_lo", 0);
  mx.totalHashes = ((uint64_t)hi << 32) | lo;
  if (prefs.getBytesLength("digest") == sizeof(bestDigest)) prefs.getBytes("digest", bestDigest, sizeof(bestDigest));
  uint32_t savedSeed = prefs.getUInt("job_seed", 0);
  uint32_t savedTarget = prefs.getUInt("job_target", 22);
  String savedJob = prefs.getString("job_id", "");
  coreSeq = prefs.getUInt("core_seq", esp_random());
  coreNonce = prefs.getUInt("core_nonce", esp_random());
  coreSalt = prefs.getUInt("core_salt", esp_random() ^ fnv1a32Text(PEA4_NODE_ID));
  intentionScore = prefs.getFloat("intent", 0.0f);
  blackboardScore = prefs.getFloat("bb", 0.0f);
  prefs.putUInt("boots", mx.bootCount);
  stateLoads++;
  setJob(savedSeed ? savedSeed : (fnv1a32Text(coreNodeId) ^ esp_random()), savedTarget ? savedTarget : 22, savedJob.length() ? savedJob.c_str() : "boot", false);
}

static void saveState() {
  prefs.putUChar("role", (uint8_t)coreRole);
  prefs.putUChar("mode", (uint8_t)coreMode);
  prefs.putUShort("best", mx.bestBits);
  prefs.putUInt("nonce", mx.bestNonce);
  prefs.putUInt("cand", mx.candidates);
  prefs.putUInt("th_hi", (uint32_t)(mx.totalHashes >> 32));
  prefs.putUInt("th_lo", (uint32_t)(mx.totalHashes & 0xFFFFFFFFULL));
  prefs.putBytes("digest", bestDigest, sizeof(bestDigest));
  prefs.putUInt("job_seed", coreJob.seed);
  prefs.putUInt("job_target", coreJob.targetZ);
  prefs.putString("job_id", coreJob.jobId);
  prefs.putUInt("core_seq", coreSeq);
  prefs.putUInt("core_nonce", coreNonce);
  prefs.putUInt("core_salt", coreSalt);
  prefs.putFloat("intent", intentionScore);
  prefs.putFloat("bb", blackboardScore);
  stateSaves++;
}

static void emitJP4Frame(Stream &out) {
  char digestHex[65];
  digestToHex(bestDigest, digestHex);
  char body[260];
  snprintf(body, sizeof(body),
           "JP4,1,%s,%s,%s,%lu,%lu,%08lX,%08lX,%u,%s,%.1f",
           coreNodeId,
           roleName(coreRole),
           modeName(activeWorkerMode()),
           (unsigned long)coreSeq,
           (unsigned long)coreNonce,
           (unsigned long)(coreSalt ^ extraSalt),
           (unsigned long)coreJob.seed,
           (unsigned)mx.bestBits,
           digestHex,
           (float)mx.hashRate);
  uint32_t csum = fnv1a32Text(body);
  out.print(body);
  out.print('*');
  out.println(csum, HEX);
  bridgeTx++;
}

static void emitPresence(Stream &out) {
  char digestHex[17];
  char fullDigest[65];
  digestToHex(bestDigest, fullDigest);
  memcpy(digestHex, fullDigest, 16);
  digestHex[16] = '\0';
  out.printf("[PEA4/PN] node=%s node_hex=%s kind=p4_titan_shell role=%s mode=%s H=%lu best=%u/%u jp4=%lu radio=0 needs_bridge=1 cam=%d cam_frames=%lu cam_age=%lums stack=\"%s\" sensor=\"%s\" psram=%luK sd=%d touch=%d digest=%s\n",
             PEA4_NODE_ID,
             coreNodeId,
             roleName(coreRole),
             modeName(activeWorkerMode()),
             (unsigned long)mx.hashRate,
             mx.bestBits,
             mx.targetBits,
             (unsigned long)bridgeTx,
             cameraReady ? 1 : 0,
             (unsigned long)cameraFrames,
             cameraLastFrameMs ? (unsigned long)(millis() - cameraLastFrameMs) : 0UL,
             CAMERA_DRIVER_STACK,
             CAMERA_SENSOR_HINT,
             (unsigned long)(ESP.getFreePsram() / 1024),
             sdOk ? 1 : 0,
             touchOk ? 1 : 0,
             digestHex);
}

static bool parseJP4Frame(char *line) {
  char *star = strchr(line, '*');
  if (!star) {
    peerBad++;
    return false;
  }
  *star = '\0';
  uint32_t sent = 0;
  if (!parseHex32(star + 1, &sent) || sent != fnv1a32Text(line)) {
    peerBad++;
    return false;
  }

  char *fields[12] = {0};
  uint8_t count = 0;
  char *tok = strtok(line, ",");
  while (tok && count < 12) {
    fields[count++] = tok;
    tok = strtok(nullptr, ",");
  }
  if (count != 12 || strcmp(fields[0], "JP4") != 0 || strcmp(fields[1], "1") != 0) {
    peerBad++;
    return false;
  }

  uint32_t seq = 0, nonce = 0, salt = 0, jobSeed = 0, z = 0;
  if (!parseU32(fields[5], &seq) ||
      !parseU32(fields[6], &nonce) ||
      !parseHex32(fields[7], &salt) ||
      !parseHex32(fields[8], &jobSeed) ||
      !parseU32(fields[9], &z) ||
      !hexToDigest(fields[10], corePeer.digest)) {
    peerBad++;
    return false;
  }

  strncpy(corePeer.nodeId, fields[2], sizeof(corePeer.nodeId) - 1);
  strncpy(corePeer.role, fields[3], sizeof(corePeer.role) - 1);
  strncpy(corePeer.mode, fields[4], sizeof(corePeer.mode) - 1);
  corePeer.seq = seq;
  corePeer.nonce = nonce;
  corePeer.salt = salt;
  corePeer.jobSeed = jobSeed;
  corePeer.z = z;
  corePeer.hps = atof(fields[11]);
  corePeer.lastMs = millis();
  corePeer.seen = true;
  peerOk++;

  if (z >= 12) {
    P4Candidate cand;
    memset(&cand, 0, sizeof(cand));
    cand.valid = true;
    strncpy(cand.nodeId, corePeer.nodeId, sizeof(cand.nodeId) - 1);
    strncpy(cand.role, corePeer.role, sizeof(cand.role) - 1);
    cand.z = z;
    cand.seq = seq;
    cand.nonce = nonce;
    cand.salt = salt;
    cand.jobSeed = jobSeed;
    memcpy(cand.digest, corePeer.digest, sizeof(cand.digest));
    cand.seenMs = corePeer.lastMs;
    queueVerify(cand);
  }
  needRedraw = true;
  return true;
}

static void emitCoreStatus(Stream &out, const char *kind) {
  char digestHex[65];
  digestToHex(bestDigest, digestHex);
  out.printf("{\"kind\":\"%s\",\"ver\":\"%s\",\"node\":\"%s\",\"node_hex\":\"%s\",\"role\":\"%s\",\"mode\":\"%s\",\"page\":%d,\"hps\":%lu,\"total\":%llu,\"best_z\":%u,\"best_nonce\":\"0x%08lX\",\"target\":%u,\"job\":\"%s\",\"job_seed\":\"0x%08lX\",\"peer_seen\":%d,\"peer_age\":%lu,\"peer_node\":\"%s\",\"peer_role\":\"%s\",\"peer_z\":%lu,\"peer_hps\":%.1f,\"peer_ok\":%lu,\"peer_bad\":%lu,\"verify_ok\":%lu,\"verify_fail\":%lu,\"proofs\":%lu,\"bridge_tx\":%lu,\"radio\":0,\"needs_bridge\":1,\"camera_ready\":%d,\"camera_frames\":%lu,\"camera_stack\":\"%s\",\"camera_state\":\"%s\",\"state_loads\":%lu,\"state_saves\":%lu,\"intent\":%.2f,\"blackboard\":%.2f,\"sd\":%d,\"touch\":%d,\"touch_events\":%lu,\"i2c_hits\":%lu,\"psram_kb\":%lu,\"digest\":\"%s\"}\n",
             kind, PEA4_VERSION, PEA4_NODE_ID, coreNodeId,
             roleName(coreRole), modeName(activeWorkerMode()), (int)page,
             (unsigned long)mx.hashRate,
             (unsigned long long)mx.totalHashes,
             mx.bestBits,
             (unsigned long)mx.bestNonce,
             mx.targetBits,
             coreJob.jobId,
             (unsigned long)coreJob.seed,
             corePeer.seen ? 1 : 0,
             (unsigned long)peerAgeMs(),
             corePeer.nodeId,
             corePeer.role,
             (unsigned long)corePeer.z,
             corePeer.hps,
             (unsigned long)peerOk,
             (unsigned long)peerBad,
             (unsigned long)verifyOk,
             (unsigned long)verifyFail,
             (unsigned long)localProofs,
             (unsigned long)bridgeTx,
             cameraReady ? 1 : 0,
             (unsigned long)cameraFrames,
             CAMERA_DRIVER_STACK,
             CAMERA_PORT_STATE,
             (unsigned long)stateLoads,
             (unsigned long)stateSaves,
             intentionScore,
             blackboardScore,
             sdOk ? 1 : 0,
             touchOk ? 1 : 0,
             (unsigned long)touchEvents,
             (unsigned long)i2cScanHits,
             (unsigned long)(ESP.getFreePsram() / 1024),
             digestHex);
}

static void resetBestState() {
  mx.bestBits = 0;
  mx.bestNonce = 0;
  mx.candidates = 0;
  memset(bestDigest, 0, sizeof(bestDigest));
  localProofs = 0;
  verifyOk = 0;
  verifyFail = 0;
  mirrorHits = 0;
  needRedraw = true;
}

static void handleCommand(char *line, Stream &out) {
  while (*line == ' ' || *line == '\t') line++;
  size_t n = strlen(line);
  while (n && (line[n - 1] == ' ' || line[n - 1] == '\t' || line[n - 1] == '\r')) line[--n] = '\0';
  if (!n) return;

  if (!strncmp(line, "JP4,", 4)) {
    parseJP4Frame(line);
    return;
  }

  cmdCount++;
  if (!strcasecmp(line, "help") || !strcasecmp(line, "?")) {
    out.println("[PEA4/CMD] help status json jp4 page 0..6 role A|B|C mode AUTO|HASH|SCOUT|MIRROR|VERIFY|RELAY|IDLE job HEX [target] seed HEX save reset clearstate");
  } else if (!strcasecmp(line, "status")) {
    printStatus();
  } else if (!strcasecmp(line, "json")) {
    emitCoreStatus(out, "JANUS_P4_STATUS");
  } else if (!strcasecmp(line, "jp4")) {
    emitJP4Frame(out);
  } else if (!strncasecmp(line, "page ", 5)) {
    uint32_t p = 0;
    if (parseU32(line + 5, &p) && p <= PAGE_SETTINGS) {
      page = (AppPage)p;
      needRedraw = true;
      out.printf("[PEA4/CMD] page=%lu\n", (unsigned long)p);
    } else {
      badCmdCount++;
      out.println("[PEA4/CMD] bad page");
    }
  } else if (!strncasecmp(line, "role ", 5)) {
    coreRole = parseRoleName(line + 5);
    saveState();
    needRedraw = true;
    out.printf("[PEA4/CMD] role=%s\n", roleName(coreRole));
  } else if (!strncasecmp(line, "mode ", 5)) {
    coreMode = parseModeName(line + 5);
    saveState();
    needRedraw = true;
    out.printf("[PEA4/CMD] mode=%s\n", modeName(coreMode));
  } else if (!strncasecmp(line, "seed ", 5)) {
    uint32_t v = 0;
    if (parseHex32(line + 5, &v)) {
      extraSalt ^= v;
      out.printf("[PEA4/CMD] extra_salt=0x%08lX\n", (unsigned long)extraSalt);
    } else {
      badCmdCount++;
      out.println("[PEA4/CMD] bad seed");
    }
  } else if (!strncasecmp(line, "job ", 4)) {
    char *seedText = line + 4;
    char *targetText = strchr(seedText, ' ');
    uint32_t seed = 0;
    uint32_t target = mx.targetBits;
    if (targetText) {
      *targetText++ = '\0';
      parseU32(targetText, &target);
    }
    if (parseHex32(seedText, &seed)) {
      char id[17];
      snprintf(id, sizeof(id), "%08lX", (unsigned long)seed);
      setJob(seed, target, id, true);
      saveState();
      needRedraw = true;
      out.printf("[PEA4/CMD] job=%s target=%lu\n", coreJob.jobId, (unsigned long)coreJob.targetZ);
    } else {
      badCmdCount++;
      out.println("[PEA4/CMD] bad job");
    }
  } else if (!strcasecmp(line, "save")) {
    saveState();
    out.printf("[PEA4/CMD] saved=%lu\n", (unsigned long)stateSaves);
  } else if (!strcasecmp(line, "reset")) {
    resetBestState();
    saveState();
    out.println("[PEA4/CMD] best reset");
  } else if (!strcasecmp(line, "clearstate")) {
    prefs.clear();
    resetBestState();
    out.println("[PEA4/CMD] nvs cleared, reboot recommended");
  } else {
    badCmdCount++;
    out.printf("[PEA4/CMD] unknown=%s\n", line);
  }
}

static void pumpLineInput(Stream &in, char *buf, size_t &len) {
  while (in.available()) {
    char c = (char)in.read();
    if (c == '\n' || c == '\r') {
      if (len) {
        buf[len] = '\0';
        handleCommand(buf, in);
        len = 0;
      }
    } else if (len + 1 < 256) {
      buf[len++] = c;
    } else {
      len = 0;
      badCmdCount++;
    }
  }
}

static void scanTouchI2c() {
  i2cScanHits = 0;
  if (!touchBusHandle) {
    Serial.println("[PEA4/I2C] scan skipped: no bus");
    return;
  }
  Serial.print("[PEA4/I2C] scan:");
  for (uint8_t addr = 0x08; addr < 0x78; addr++) {
    esp_err_t err = i2c_master_probe(touchBusHandle, addr, 35);
    if (err == ESP_OK) {
      i2cScanHits++;
      Serial.printf(" 0x%02X", addr);
    }
  }
  if (!i2cScanHits) Serial.print(" none");
  Serial.println();
}

static void probeSd() {
  if (!SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0, SD_D1, SD_D2, SD_D3)) {
    Serial.println("[PEA4/SD] pin map failed");
    sdOk = false;
    return;
  }
  sdOk = SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT, 5);
  if (sdOk) {
    Serial.printf("[PEA4/SD] ok=1 type=%u sizeMB=%llu\n", SD_MMC.cardType(), SD_MMC.cardSize() / (1024ULL * 1024ULL));
  } else {
    Serial.println("[PEA4/SD] ok=0 no-card-or-not-ready; continuing without SD");
  }
}

static const char *pageShortName(AppPage p) {
  switch (p) {
    case PAGE_HOME: return "HOME";
    case PAGE_CAMERA: return "CAM";
    case PAGE_SWARM: return "SWR";
    case PAGE_TITAN: return "TIT";
    case PAGE_AUDIO: return "AUD";
    case PAGE_GALLERY: return "COR";
    case PAGE_SETTINGS: return "SET";
    default: return "---";
  }
}

static void drawHeader(const char *title) {
  fillRect(0, 0, LCD_H_RES, 68, rgb565(7, 12, 18));
  fillRect(0, 66, LCD_H_RES, 2, rgb565(28, 78, 92));
  int titleX = 16;
  if (page != PAGE_HOME) {
    fillRect(12, 14, 70, 34, rgb565(18, 34, 44));
    drawRect(12, 14, 70, 34, rgb565(54, 112, 128));
    drawText(26, 24, "<", C_TEXT, 2);
    drawText(44, 25, "BACK", C_DIM, 1);
    titleX = 98;
  }
  drawText(titleX, 14, title, C_TEXT, 3);
  drawTextf(286, 14, C_DIM, 1, "%s/%s", roleName(coreRole), modeName(activeWorkerMode()));
  drawTextf(286, 34, C_AMBER, 1, "H%lu B%u/%u", (unsigned long)mx.hashRate, mx.bestBits, mx.targetBits);
}

static void drawFooter() {
  const int h = 58;
  const int y = LCD_V_RES - h;
  fillRect(0, y, LCD_H_RES, h, rgb565(6, 10, 15));
  fillRect(0, y, LCD_H_RES, 1, rgb565(35, 72, 84));
  const int zone = LCD_H_RES / 7;
  for (int i = 0; i < 7; i++) {
    AppPage p = (AppPage)i;
    int x = i * zone;
    bool active = page == p;
    if (active) fillRect(x + 3, y + 7, zone - 6, 24, rgb565(22, 54, 66));
    drawRect(x + 3, y + 7, zone - 6, 24, active ? C_CYAN : rgb565(28, 44, 54));
    drawText(x + 16, y + 16, pageShortName(p), active ? C_TEXT : C_DIM, 1);
  }
  drawTextf(14, y + 39, C_DIM, 1, "obs=%d sd=%d psram=%luK peer=%s",
            PEA4_OBSERVER_ONLY, sdOk ? 1 : 0,
            (unsigned long)(ESP.getFreePsram() / 1024),
            (corePeer.seen && peerAgeMs() < JP4_PEER_TIMEOUT_MS) ? "LIVE" : "WAIT");
  drawTextf(302, y + 39, C_AMBER, 1, "t%u,%u", lastTouchX, lastTouchY);
  drawTextf(400, y + 39, C_AMBER, 1, "j%lu", (unsigned long)bridgeTx);
}

static void tile(int x, int y, int w, int h, const char *name, const char *sub, uint16_t accent) {
  fillRect(x, y, w, h, rgb565(8, 16, 24));
  fillRect(x + 2, y + 2, w - 4, h - 4, rgb565(12, 20, 30));
  drawRect(x, y, w, h, rgb565(34, 72, 84));
  fillRect(x, y, 7, h, accent);
  fillRect(x + 7, y, w - 7, 2, accent);
  drawText(x + 20, y + 18, name, C_TEXT, 2);
  drawText(x + 20, y + 50, sub, C_DIM, 1);
  int bx = x + w - 58;
  int by = y + 24;
  fillRect(bx, by, 34, 44, rgb565(10, 22, 30));
  drawRect(bx, by, 34, 44, accent);
  fillRect(bx + 7, by + 8, 20, 4, accent);
  fillRect(bx + 7, by + 20, 20, 4, rgb565(44, 92, 104));
  fillRect(bx + 7, by + 32, 20, 4, rgb565(30, 58, 70));
  int fill = min(w - 34, max(8, (int)((mx.bestBits + 1) * (w - 34) / 40)));
  fillRect(x + 18, y + h - 22, fill, 5, accent);
  fillRect(x + 18 + fill, y + h - 22, w - 36 - fill, 5, rgb565(18, 36, 40));
}

static void drawHome() {
  drawJanusBackdrop("PEA4 / TITAN NODE / TOUCH COCKPIT");
  drawHeader("JANUS PEA4");
  drawText(18, 82, "P4 SHELL + DUAL SWARM CORE / 2-IN-1", C_AMBER, 2);
  tile(18, 126, 210, 132, "CAMERA", "vendor IDF pipeline", C_CYAN);
  tile(252, 126, 210, 132, "SWARM", "PN cortex / nodes", C_GREEN);
  tile(18, 282, 210, 132, "TITAN", "P4 A/B/C core", C_BLUE);
  tile(252, 282, 210, 132, "AUDIO", "ES8311 pins mapped", C_AMBER);
  tile(18, 438, 210, 132, "CORPUS", "SD/FATFS archive", rgb565(180, 130, 255));
  tile(252, 438, 210, 132, "SETTINGS", "display / touch / restore", rgb565(255, 120, 90));
  drawText(24, 606, "ROLE: DISPLAY + ARCHIVE + CAMERA PREPROCESS + P4 VERIFY NODE", C_TEXT, 1);
  drawText(24, 628, "BUZZ STAYS POOL MASTER. PEA4 DOES NOT SUBMIT SHARES.", C_DIM, 1);
  drawTextf(24, 650, C_DIM, 1, "SERIAL: help/status/json/jp4 role mode job seed save");
  drawFooter();
}

static void drawCamera() {
  drawJanusBackdrop("CAMERA / MIPI CSI BRINGUP");
  drawHeader("CAMERA");
  fillRect(26, 98, 428, 302, rgb565(3, 8, 12));
  drawRect(26, 98, 428, 302, C_CYAN);
  fillRect(36, 108, 408, 282, rgb565(8, 14, 20));
  for (int y = 116; y < 382; y += 18) {
    uint16_t c = (y / 18) & 1 ? rgb565(10, 28, 34) : rgb565(8, 20, 28);
    fillRect(42, y, 396, 6, c);
  }
  int cx = 240;
  int cy = 246;
  drawCircle(cx, cy, 82, rgb565(20, 92, 112));
  drawCircle(cx, cy, 54, C_CYAN);
  drawCircle(cx, cy, 18, C_AMBER);
  drawText(114, 198, "MIPI-CSI CAMERA", C_TEXT, 2);
  drawText(112, 232, cameraReady ? "STREAM ACTIVE" : "DRIVER PORT PENDING", cameraReady ? C_GREEN : C_AMBER, 2);
  drawText(92, 292, "VENDOR IDF CAMERA STACK REQUIRED", C_DIM, 1);

  fillRect(28, 424, 424, 128, rgb565(8, 16, 24));
  drawRect(28, 424, 424, 128, rgb565(34, 72, 84));
  drawTextf(44, 446, C_TEXT, 1, "stack: %s", CAMERA_DRIVER_STACK);
  drawTextf(44, 474, C_TEXT, 1, "sensor: %s", CAMERA_SENSOR_HINT);
  drawTextf(44, 502, C_TEXT, 1, "state: %s", CAMERA_PORT_STATE);
  drawTextf(44, 530, C_DIM, 1, "frames=%lu  bridge=JP4/PN serial  radio=0", (unsigned long)cameraFrames);

  drawText(34, 590, "P4 Arduino shell is live. Real preview needs IDF port.", C_TEXT, 1);
  drawText(34, 614, "Buzz visibility needs companion C6/S3 ESP-NOW bridge.", C_DIM, 1);
  drawFooter();
}

static void drawSwarm() {
  drawJanusBackdrop("JP4 / SWARM OBSERVER");
  drawHeader("SWARM");
  drawText(30, 92, "JP4 SERIAL / COMPANION BRIDGE", C_AMBER, 2);
  drawTextf(30, 132, C_TEXT, 2, "node=%s", coreNodeId);
  drawTextf(30, 172, C_TEXT, 2, "role=%s mode=%s", roleName(coreRole), modeName(activeWorkerMode()));
  drawTextf(30, 212, C_TEXT, 2, "peer=%s age=%lums", corePeer.seen ? corePeer.nodeId : "NONE", (unsigned long)peerAgeMs());
  drawTextf(30, 252, C_DIM, 2, "peer role=%s z=%lu H=%.0f", corePeer.role, (unsigned long)corePeer.z, corePeer.hps);
  drawTextf(30, 292, C_DIM, 2, "rx ok/bad=%lu/%lu verify=%lu/%lu", (unsigned long)peerOk, (unsigned long)peerBad, (unsigned long)verifyOk, (unsigned long)verifyFail);
  drawTextf(30, 332, C_DIM, 2, "proof=%lu bridge=%lu drop=%lu", (unsigned long)localProofs, (unsigned long)bridgeTx, (unsigned long)peerDrop);
  drawText(30, 394, "JP4 frame: observer telemetry for C6/S3 bridge.", C_TEXT, 1);
  drawText(30, 416, "No target mutation. No extra pool pressure.", C_DIM, 1);
  drawFooter();
}

static void drawTitan() {
  drawJanusBackdrop("TITAN CORE / HASH OBSERVER");
  drawHeader("TITAN");
  drawTextf(28, 96, C_TEXT, 2, "boots=%lu uptime=%lus", (unsigned long)mx.bootCount, (unsigned long)((millis() - bootMs) / 1000));
  drawTextf(28, 136, C_TEXT, 2, "job=%s seed=%08lX", coreJob.jobId, (unsigned long)coreJob.seed);
  drawTextf(28, 176, C_TEXT, 2, "best=%u nonce=%08lX", mx.bestBits, (unsigned long)mx.bestNonce);
  drawTextf(28, 216, C_TEXT, 2, "total=%llu cand=%lu", (unsigned long long)mx.totalHashes, (unsigned long)mx.candidates);
  drawTextf(28, 256, C_DIM, 2, "heap=%luK psram=%luK", (unsigned long)(ESP.getFreeHeap() / 1024), (unsigned long)(ESP.getFreePsram() / 1024));
  drawTextf(28, 296, C_DIM, 2, "intent=%.2f bb=%.2f", intentionScore, blackboardScore);
  fillRect(28, 326, 424, 28, rgb565(30, 28, 18));
  int bar = min(424, (int)(mx.bestBits * 424 / 40));
  fillRect(28, 326, bar, 28, C_AMBER);
  drawRect(28, 326, 424, 28, C_LINE);
  drawTextf(28, 390, C_DIM, 1, "TRANCEPTION-LITE %.2f / JANUS-IO %.2f", mx.tranception, mx.janusIo);
  drawTextf(28, 412, C_DIM, 1, "LOVE-TACHYON %.2f+i%.2f phase %.2f", mx.loveReal, mx.loveImag, mx.lovePhase);
  drawText(28, 434, "MATH TERMS ONLY: scoring fields, not pool math.", C_DIM, 1);
  drawFooter();
}

static void drawAudio() {
  drawJanusBackdrop("AUDIO / ES8311 SLOT");
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
  drawJanusBackdrop("CORPUS / SD SLOT");
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
  drawJanusBackdrop("SETTINGS / RESTORE / DIAG");
  drawHeader("SETTINGS");
  drawTextf(28, 98, C_TEXT, 2, "chip=%s rev=%u", ESP.getChipModel(), ESP.getChipRevision());
  drawTextf(28, 138, C_TEXT, 2, "flash=%luMB sketch=%luK", (unsigned long)(ESP.getFlashChipSize() / 1024 / 1024), (unsigned long)(ESP.getSketchSize() / 1024));
  drawTextf(28, 178, C_TEXT, 2, "display=%d touch=%d sd=%d", displayOk ? 1 : 0, touchOk ? 1 : 0, sdOk ? 1 : 0);
  drawTextf(28, 218, C_TEXT, 2, "backlight=%u role=%s", brightnessStep, roleName(coreRole));
  drawTextf(28, 258, C_TEXT, 2, "mode=%s saves=%lu", modeName(coreMode), (unsigned long)stateSaves);
  drawTextf(28, 298, C_TEXT, 2, "touch ev=%lu i2c=%lu", (unsigned long)touchEvents, (unsigned long)i2cScanHits);
  drawText(28, 320, "STOCK RESTORE IMAGES:", C_AMBER, 2);
  drawText(28, 358, "P4_STOCK_BACKUP_20260617_015417", C_DIM, 1);
  drawText(28, 392, "SERIAL COMMANDS:", C_AMBER, 1);
  drawText(28, 414, "page 0..6  role A/B/C  mode AUTO/HASH/VERIFY", C_DIM, 1);
  drawText(28, 436, "json  jp4  save  reset  clearstate", C_DIM, 1);
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

static void setPage(AppPage next, const char *reason) {
  if (page == next) return;
  page = next;
  needRedraw = true;
  lastNavMs = millis();
  Serial.printf("[PEA4/NAV] page=%d reason=%s\n", (int)page, reason ? reason : "nav");
}

static bool navCooldownReady(uint32_t now) {
  return now - lastNavMs > 220;
}

static bool handleHomeTap(uint16_t x, uint16_t y) {
  const int xs[2] = {18, 252};
  const int ys[3] = {126, 282, 438};
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 2; col++) {
      if (x >= xs[col] && x < xs[col] + 210 && y >= ys[row] && y < ys[row] + 132) {
        int idx = row * 2 + col;
        setPage((AppPage)(idx + 1), "home-tile");
        return true;
      }
    }
  }
  return false;
}

static bool handleFooterTap(uint16_t x, uint16_t y) {
  const int navY = LCD_V_RES - 58;
  if (y < navY) return false;
  uint8_t idx = min(6, (int)((uint32_t)x * 7UL / LCD_H_RES));
  setPage((AppPage)idx, "footer");
  return true;
}

static void gotoNextPage() {
  uint8_t p = (uint8_t)page;
  p = (p + 1) % 7;
  setPage((AppPage)p, "swipe-next");
}

static bool isBackRescueZone(uint16_t x, uint16_t y) {
  if (page == PAGE_HOME) return false;
  if (y < 132) return true;            // header / visible BACK strip
  if (x < 96 && y < 360) return true;  // broad left escape strip
  if (y > LCD_V_RES - 96) return true; // mirrored/top confusion rescue
  return false;
}

static bool handleImmediateTouchNav(uint16_t x, uint16_t y, uint32_t now) {
  if (!navCooldownReady(now)) return false;
  if (handleFooterTap(x, y)) return true;
  if (isBackRescueZone(x, y)) {
    setPage(PAGE_HOME, "back-rescue");
    return true;
  }
  if (page == PAGE_HOME && handleHomeTap(x, y)) return true;
  return false;
}

static void handleTouch() {
  uint16_t x = 0, y = 0;
  bool down = touchOk && touch.getTouch(&x, &y);
  uint32_t now = millis();
  if (down) {
    lastTouchX = x;
    lastTouchY = y;
    if (!wasTouched) {
      touchDownMs = now;
      touchStartX = x;
      touchStartY = y;
      touchEvents++;
      touchActionTaken = handleImmediateTouchNav(x, y, now);
    }
    if (!touchActionTaken && page != PAGE_HOME && now - touchDownMs > 850 && navCooldownReady(now)) {
      setPage(PAGE_HOME, "long-press-home");
      touchActionTaken = true;
    }
    if (now - lastTouchLogMs > 700) {
      Serial.printf("[PEA4/TOUCH] down x=%u y=%u page=%d ev=%lu\n", x, y, (int)page, (unsigned long)touchEvents);
      lastTouchLogMs = now;
    }
  }
  if (!down && wasTouched) {
    if (now - touchDownMs > 30) {
      int dx = (int)lastTouchX - (int)touchStartX;
      int dy = (int)lastTouchY - (int)touchStartY;
      Serial.printf("[PEA4/TOUCH] tap x=%u y=%u page=%d dt=%lums\n",
                    lastTouchX, lastTouchY, (int)page, (unsigned long)(now - touchDownMs));
      if (touchActionTaken) {
        // Immediate navigation already handled on press.
      } else if (abs(dx) > 110 && abs(dy) < 90) {
        if (dx > 0 && page != PAGE_HOME) {
          setPage(PAGE_HOME, "swipe-back");
          Serial.println("[PEA4/NAV] swipe-back home");
        } else if (dx < 0) {
          gotoNextPage();
          Serial.printf("[PEA4/NAV] swipe-next page=%d\n", (int)page);
        }
      } else if (handleFooterTap(lastTouchX, lastTouchY)) {
        // handled by footer
      } else if (page != PAGE_HOME && lastTouchY < 78 && lastTouchX < 96) {
        setPage(PAGE_HOME, "back-button");
      } else if (page == PAGE_HOME) {
        handleHomeTap(lastTouchX, lastTouchY);
      } else if (page == PAGE_SETTINGS && lastTouchY > 200 && lastTouchY < 280) {
        brightnessStep = brightnessStep ? 0 : 1;
        lcd.example_bsp_set_lcd_backlight(brightnessStep);
        needRedraw = true;
      }
    }
  }
  if (!down) touchActionTaken = false;
  wasTouched = down;
}

static void printStatus() {
  Serial.printf("[PEA4/CORE] v=%s node=%s hex=%s role=%s mode=%s page=%d H=%lu best=%u/%u cand=%lu proofs=%lu peer=%d/%s z=%lu age=%lums ok/bad=%lu/%lu verify=%lu/%lu sd=%d touch=%d ev=%lu i2c=%lu psram=%luK obs=%d submit=0 radio=0 needs_bridge=1 cam=%d/%s camFrames=%lu io=%.2f tr=%.2f love=%.2f+i%.2f ph=%.2f saves=%lu\n",
                PEA4_VERSION, PEA4_NODE_ID, coreNodeId, roleName(coreRole), modeName(activeWorkerMode()), (int)page,
                (unsigned long)mx.hashRate, mx.bestBits, mx.targetBits,
                (unsigned long)mx.candidates,
                (unsigned long)localProofs,
                corePeer.seen ? 1 : 0, corePeer.nodeId, (unsigned long)corePeer.z, (unsigned long)peerAgeMs(),
                (unsigned long)peerOk, (unsigned long)peerBad,
                (unsigned long)verifyOk, (unsigned long)verifyFail,
                sdOk ? 1 : 0, touchOk ? 1 : 0,
                (unsigned long)touchEvents,
                (unsigned long)i2cScanHits,
                (unsigned long)(ESP.getFreePsram() / 1024), PEA4_OBSERVER_ONLY,
                cameraReady ? 1 : 0, CAMERA_PORT_STATE, (unsigned long)cameraFrames,
                mx.janusIo, mx.tranception, mx.loveReal, mx.loveImag, mx.lovePhase,
                (unsigned long)stateSaves);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  bootMs = millis();
  readBaseMac();
  loadState();

  Serial.printf("[PEA4/SHELL] boot v=%s node=%s hex=%s role=%s mode=%s kind=%s observer_only=%d\n",
                PEA4_VERSION, PEA4_NODE_ID, coreNodeId, roleName(coreRole), modeName(coreMode), PEA4_KIND, PEA4_OBSERVER_ONLY);
  Serial.printf("[PEA4/HW] chip=%s rev=%u flash=%lu psram=%lu stock_backup=P4_STOCK_BACKUP_20260617_015417\n",
                ESP.getChipModel(), ESP.getChipRevision(),
                (unsigned long)ESP.getFlashChipSize(), (unsigned long)ESP.getPsramSize());

  fb = (uint16_t *)heap_caps_malloc((size_t)LCD_H_RES * LCD_V_RES * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!fb) {
    Serial.println("[PEA4/DISPLAY] framebuffer allocation failed; enable PSRAM");
    return;
  }

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
  esp_err_t i2cErr = i2c_new_master_bus(&i2cBusConf, &touchBusHandle);
  Serial.printf("[PEA4/I2C] bus=%d\n", (int)i2cErr);
  if (i2cErr == ESP_ERR_INVALID_STATE) {
    i2c_master_get_bus_handle(1, &touchBusHandle);
    Serial.printf("[PEA4/I2C] reused bus handle=%d\n", touchBusHandle ? 1 : 0);
  }
  scanTouchI2c();

  lcd.begin();
  displayOk = true;
  touch.begin();
  touch.set_rotation(0);
  touchOk = true;
  probeSd();

  Serial.println("[PEA4/SHELL] stock-like Janus launcher ready");
  Serial.println("[PEA4/CORE] 2-in-1 enabled: launcher + P4_A/P4_B/P4_C dual swarm core + JP4 serial bridge");
  Serial.println("[PEA4/CORE] commands: help status json jp4 role A/B/C mode AUTO/HASH/SCOUT/MIRROR/VERIFY/RELAY/IDLE job HEX [target]");
  needRedraw = true;
  lastMinerRateMs = millis();
  lastJp4FrameMs = millis();
}

void loop() {
  if (!displayOk || !fb) {
    pumpLineInput(Serial, usbLine, usbLineLen);
    delay(1000);
    return;
  }

  pumpLineInput(Serial, usbLine, usbLineLen);
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
  if (now - lastJp4FrameMs > JP4_FRAME_MS) {
    emitJP4Frame(Serial);
    lastJp4FrameMs = now;
  }
  if (now - lastPresenceMs > PEA4_PRESENCE_MS) {
    emitPresence(Serial);
    lastPresenceMs = now;
  }
  if (corePeer.seen && peerAgeMs() > JP4_PEER_TIMEOUT_MS * 3UL) {
    corePeer.seen = false;
    peerDrop++;
    needRedraw = true;
  }
  if (now - lastStateSaveMs > JP4_STATE_SAVE_MS) {
    saveState();
    lastStateSaveMs = now;
  }
}
