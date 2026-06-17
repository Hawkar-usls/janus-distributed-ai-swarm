/*
  JANUS_YAKS_GATE_S3 v0.1

  Yaks Gate / skaY: a StickS3 hand-held SHA portal for the Janus swarm.

  Field doctrine:
  - IR is local 38 kHz optical signalling only, like a tiny remote control.
  - It does not transmit LoRa/RF and is not intended to interact with aircraft,
    drones, missiles, towers, antennas, or any military systems.
  - ADV LoRa/GNSS 868 can later become the real "sky anchor": ADV provides
    position/time/course, while Yaks Gate opens gameplay gates from that anchor.

  This sketch keeps the useful Slick miner layer:
  - Buzz J/B job receiver
  - observer-only SHA walk
  - RejectTail / StaleTail packets
  - entropy + heartbeat
  - IR SHA sigil

  It removes the old RPG shell. The game is now the miner:
  stabilize the gate by hand, let it scout while idle, and emit IR sigils
  as local swarm/game beacons. It never submits shares to Buzz or the pool.
*/

#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_idf_version.h>
#include <LittleFS.h>
#include <mbedtls/sha256.h>
#include <math.h>

#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"

#define MESH_WIFI_SSID       "JANUS_WIFI_PLACEHOLDER"
#define MESH_WIFI_PASSWORD   "JANUS_NET_PLACEHOLDER"

#define YG_NODE_ID           "YaksGateS3"
#define YG_VERSION           "v0.1"
#define YG_IR_TX_PIN         46
#define YG_IR_CARRIER_HZ     38256
#define YG_IR_AUTO_MS        9000UL
#define YG_IR_SKY_MS         18000UL
#define YG_IR_BURST_GUARD_MS 620UL
#define YG_REMOTE_JOB_TTL_MS 6500UL
#define YG_STALE_SHADOW_MS   2600UL
#define YG_HEARTBEAT_MS      1400UL
#define YG_ENTROPY_MS        2600UL
#define YG_TAIL_EVENT_MS     180UL
#define YG_INPUT_IDLE_MS     4200UL
#define YG_LOG_PATH          "/yaks_tail.jsonl"
#define YG_LOG_MAX_BYTES     524288UL
#define YG_CORPUS_PATH       "/yaks_corpus.jsonl"
#define YG_CORPUS_OLD_PATH   "/yaks_corpus.old"
#define YG_CORPUS_MAX_BYTES  524288UL
#define YG_STATE_PATH        "/yaks_state.json"
#define YG_STATE_TMP_PATH    "/yaks_state.tmp"
#define YG_STATE_SAVE_MS     12000UL
#define YG_STATE_PERIODIC_MS 90000UL
#define YG_PARTICLE_COUNT    76
#define YG_RIFT_COUNT        14
#define YG_ASCII_COLS        40
#define YG_ASCII_ROWS        17
#define YG_NAS_ENABLE        1
#define YG_NAS_BASE_URL      "http://192.168.1.92:5000"
#define YG_NAS_AUTO_MS       12000UL
#define YG_NAS_TIMEOUT_MS    260
#define YG_NAS_WARP_MS       9500UL
#define YG_NAS_BRAIN_ENABLE  1
#define YG_NAS_BRAIN_URL     "http://192.168.1.92:8008"
#define YG_NAS_BRAIN_TIMEOUT_MS 220
#define YG_BLACKSTAR_TTL_MS  65000UL
#define YG_MERCURY_WARP_TTL_MS   70000UL
#define YG_BROTHER_READY_TTL_MS  45000UL
#define YG_MERCURY_WARP_MS       9200UL
#define YG_FLASH_BUBBLE_BG_MS     7600UL
#define YG_FLASH_BUBBLE_ACTIVE_MS 2400UL
#define YG_FLASH_BUBBLE_ALERT_MS  950UL
#define YG_FLASH_BUBBLE_TTL_MS    70000UL
#define YG_IR_ESCAPE_MS           5200UL
#define YG_PN_CORTEX_MS           3200UL
#define YG_IMU_HAND_RECENTER_MS   380UL
#define YG_IMU_HAND_REARM_MS      2600UL

uint8_t JANUS_BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

M5Canvas canvas(&M5.Display);
bool canvasReady = false;
int screenW = 160;
int screenH = 80;

struct __attribute__((packed)) JanusColonyPacket {
  char magic[6];
  char nodeId[24];
  char role[12];
  uint32_t seq;
  uint32_t hashRate;
  uint32_t shares;
  uint32_t rejects;
  uint32_t bestBits;
  float diff;
  uint16_t targetBits;
  uint16_t aiBatch;
  uint8_t aiHint;
  uint32_t jobAgeMs;
  int8_t rssi;
  uint32_t uptime;
};

struct __attribute__((packed)) JobPacket {
  uint8_t magic[2];
  uint8_t job_id[8];
  uint8_t header[80];
  uint32_t start_nonce;
  uint32_t range_size;
  uint8_t target[32];
  uint32_t extranonce2;
};

struct __attribute__((packed)) EntropyReport {
  uint8_t magic[2];
  uint16_t worker_id;
  float local_entropy;
  uint8_t sensor_flags;
  float values[4];
};

struct __attribute__((packed)) EntropyReportV2 {
  uint8_t magic[2];
  uint16_t worker_id;
  char nodeId[24];
  float local_entropy;
  float prediction_error;
  float sync_hint;
  float fit;
  uint8_t sensor_flags;
  float values[8];
  uint32_t uptime_ms;
};

struct __attribute__((packed)) JanusEventPacket {
  uint8_t magic[2];
  uint8_t version;
  uint8_t eventType;
  uint8_t nodeRole;
  uint8_t confidence;
  uint8_t urgency;
  char nodeId[24];
  char kind[16];
  uint32_t seq;
  uint32_t uptimeMs;
  uint16_t topicHash;
  uint16_t objectHash;
  uint16_t capabilities;
  int16_t valueA_x10;
  int16_t valueB_x10;
  int16_t valueC_x10;
  int16_t valueD_x10;
  uint32_t eventHash;
  uint32_t ttlMs;
};

// Shared low-cost world-state packet. Layout matches Blind Eye / BH K2.
struct __attribute__((packed)) JanusKenshiPacket {
  uint8_t magic[2];        // 'K','2'
  uint8_t version;         // 1
  uint8_t flags;           // bit0=active bubble, bit1=alert, bit2=virtual summary, bit3=motion-base ready
  char nodeId[24];
  uint32_t seq;
  uint16_t worker_id;
  uint32_t uptime_ms;
  uint8_t activeBubbleNodes;
  uint8_t virtualNodes;
  uint32_t worldFlags;
  uint8_t sector;
  uint8_t predictedSector;
  uint8_t jobState;        // 0 idle, 1 watch, 2 track, 3 alert, 4 learn, 5 relay
  uint8_t priority;
  int8_t rssi;
  float entropy;
  float activity;
  float confidence;
  float values[6];         // heat/load, hashrate, bestBits, blackstar pull, stability, charge
};

// P/N Cortex: SHA-sealed p-n/silicon body-state packet for the swarm language.
// It observes heat/load/jitter/tail shape only; it never changes pool math.
struct __attribute__((packed)) JanusPnCortexPacket {
  uint8_t magic[2];        // 'P','N'
  uint8_t version;         // 1
  uint8_t role;
  uint16_t worker_id;
  char nodeId[24];
  char kind[16];
  uint32_t seq;
  uint32_t uptime_ms;
  uint32_t job_sig;
  uint32_t prev_hash;
  uint32_t packet_hash;    // SHA256d over this struct with packet_hash zeroed, low32
  uint32_t hash_rate;
  uint32_t total_hashes;
  uint16_t target_bits;
  uint16_t best_bits;
  uint8_t lane;
  uint8_t sector;
  uint8_t flags;           // bit0 job, bit1 IR, bit2 BlackStar, bit3 escape, bit4 NAS Brain, bit5 Mercury Warp
  int8_t rssi;
  uint16_t thermal_x1000;
  uint16_t load_x1000;
  uint16_t jitter_us;
  uint16_t entropy_x1000;
  uint16_t tail_x1000;
  uint16_t voltage_mv;
  uint16_t ir_phase;
  uint16_t reserved;
};

struct __attribute__((packed)) RejectTailPacket {
  uint8_t magic[2];       // 'R','T'
  uint8_t version;
  uint8_t phase;          // 1 scout, 2 wake, 3 mirror
  uint8_t strategy;       // 1 observe, 2 shadow, 3 pool_mirror
  uint8_t lane;           // 1 manual, 2 idle_ai, 3 stale_shadow
  uint8_t sector;
  char nodeId[24];
  uint32_t seq;
  uint16_t worker_id;
  uint16_t zbits;
  uint16_t targetBits;
  uint32_t jobSeq;
  uint32_t jobAgeMs;
  int32_t timeToNextJobMs;
  uint32_t timeAfterCleanJobMs;
  uint32_t nonce;
  uint32_t poolRejects;
  uint32_t staleDropped;
  uint8_t staleReason;
  uint8_t targetPass;
  uint8_t hashTail[4];
  int8_t rssi;
  uint32_t uptimeMs;
};

struct RemoteJobState {
  bool active = false;
  uint8_t jobId[8] = {0};
  uint8_t header[80] = {0};
  uint8_t target[32] = {0};
  uint32_t startNonce = 0;
  uint32_t rangeSize = 0;
  uint32_t nonce = 0;
  uint32_t endNonce = 0;
  uint32_t rxMs = 0;
  uint32_t seq = 0;
};

struct GateParticle {
  float x = 0;
  float y = 0;
  float z = 1;
  float speed = 0.2f;
  uint8_t hue = 0;
};

struct Rift {
  float x = 0;
  float y = 0;
  float z = 1;
  float spin = 0;
  uint8_t kind = 0;
};

struct GateState {
  float shipX = 0;
  float shipY = 0;
  float velX = 0;
  float velY = 0;
  float targetX = 0;
  float targetY = 0;
  float phase = 0;
  float spin = 0;
  float charge = 0.16f;
  float stability = 0.62f;
  float aperture = 0.20f;
  float heat = 0;
  float tailGlow = 0;
  float pulse = 0;
  float shake = 0;
  float skyLock = 0;
  uint32_t opens = 0;
  uint32_t sigils = 0;
  uint32_t skySigils = 0;
  bool autoPilot = true;
};

struct YaksCorpusState {
  uint32_t events = 0;
  uint32_t targetPass = 0;
  uint32_t stale = 0;
  uint32_t poolReject = 0;
  uint32_t zTail = 0;
  uint32_t lastNonce = 0;
  uint32_t lastJobSeq = 0;
  uint32_t lastEventMs = 0;
  uint16_t bestZ = 0;
  uint16_t laneHits[4] = {0, 0, 0, 0};
  float score = 0.0f;
  float red = 0.0f;
  float blue = 0.0f;
  float gold = 0.0f;
  float acid = 0.08f;
  float yeast = 0.18f;
  float bacteria = 0.22f;
  float paradox = 0.0f;
  char lastLane[8] = "BOOT";
  char lastPhase[8] = "BOOT";
};

struct BlackStarLinkState {
  bool seen = false;
  uint8_t lane = 0;
  uint8_t bestLane = 0;
  float confidence = 0.0f;
  float loss = 1.0f;
  float influence = 0.0f;
  float mercuryTorr = 760.0f;
  float torricelliVoid = 0.0f;
  float mercuryTime = 0.0f;
  float hawkingVapor = 0.0f;
  float horizonBalance = 0.0f;
  uint32_t corpus = 0;
  uint32_t seq = 0;
  uint32_t lastMs = 0;
  uint32_t mercuryMs = 0;
  char source[24] = "BH";
};

struct BrotherReadyState {
  bool anchorSeen = false;
  bool gladiusSeen = false;
  uint32_t anchorMs = 0;
  uint32_t gladiusMs = 0;
  uint16_t anchorBest = 0;
  uint16_t gladiusBest = 0;
  float anchorOxy = 0.0f;
  float gladiusOxy = 0.0f;
  float anchorVacuum = 0.0f;
  float gladiusVacuum = 0.0f;
};

struct MercuryWarpState {
  bool armed = false;
  bool launch = false;
  uint32_t armedMs = 0;
  uint32_t launchUntilMs = 0;
  uint32_t readySeq = 0;
  float charge = 0.0f;
  float vector = 0.0f;
  float reverse = 0.0f;
};

struct FlashBubbleState {
  uint8_t state = 0;       // 0 sleep, 1 virtual, 2 active, 3 alert
  uint8_t jobState = 1;    // watch/track/alert/learn/relay
  uint8_t sector = 0;
  uint8_t predictedSector = 0;
  uint8_t priority = 0;
  uint8_t activeNodes = 0;
  uint8_t virtualNodes = 0;
  uint32_t worldFlags = 0;
  uint32_t seq = 0;
  uint32_t rx = 0;
  uint32_t tx = 0;
  uint32_t irBeaconTx = 0;
  uint32_t lastRxMs = 0;
  uint32_t lastTxMs = 0;
  uint32_t lastIrBeaconMs = 0;
  float entropy = 0.0f;
  float activity = 0.0f;
  float confidence = 0.0f;
  float eventPower = 0.0f;
  float siliconHeat = 0.0f;
  float siliconLoad = 0.0f;
  char source[24] = "none";
};

enum YaksFlashWorldFlags : uint32_t {
  YF_WORLD_BLACKSTAR = 1UL << 0,
  YF_WORLD_JOB       = 1UL << 1,
  YF_WORLD_IR        = 1UL << 2,
  YF_WORLD_MANUAL    = 1UL << 3,
  YF_WORLD_NAS       = 1UL << 4,
  YF_WORLD_TAIL      = 1UL << 5,
  YF_WORLD_ESCAPE    = 1UL << 6,
  YF_WORLD_UNSTABLE  = 1UL << 7,
  YF_WORLD_SILICON   = 1UL << 8,
  YF_WORLD_HORIZON   = 1UL << 9,
  YF_WORLD_BRAIN     = 1UL << 10,
  YF_WORLD_MERCURY   = 1UL << 11,
  YF_WORLD_WARP      = 1UL << 12,
  YF_WORLD_BROTHERS  = 1UL << 13
};

RemoteJobState currentJob;
RemoteJobState staleJob;
GateParticle particles[YG_PARTICLE_COUNT];
Rift rifts[YG_RIFT_COUNT];
GateState gate;
YaksCorpusState yaksCorpus;
BlackStarLinkState blackStarLink;
BrotherReadyState brothers;
MercuryWarpState mercuryWarp;
FlashBubbleState flashBubble;

bool colonyReady = false;
bool buzzSeen = false;
uint32_t lastBuzzMs = 0;
uint32_t lastJobRxMs = 0;
uint32_t lastCleanJobMs = 0;
uint32_t jobSeq = 0;
float jobGapEmaMs = 1300.0f;
uint32_t colonySeq = 0;
uint32_t tailSeq = 0;
uint16_t workerIdCache = 0;

uint32_t observerHashrate = 0;
uint32_t observerHashesWindow = 0;
uint32_t observerHashesTotal = 0;
uint32_t hashWindowMs = 0;
uint32_t bestBits = 0;
uint32_t tailEvents = 0;
uint32_t zTailEvents = 0;
uint32_t staleTailEvents = 0;
uint32_t poolRejectEvents = 0;
uint32_t staleDropped = 0;
uint32_t buzzRejects = 0;
uint32_t lastBuzzRejects = 0;
uint32_t buzzHashrate = 0;
uint32_t buzzShares = 0;
uint32_t buzzBestBits = 0;
float buzzDiff = 0.0f;
uint16_t targetBitsNow = 22;
char statusLine[48] = "BOOT";
char lastTailLine[48] = "gate idle";
uint8_t gateHashBytes[32] = {0};
char gateAscii[YG_ASCII_ROWS][YG_ASCII_COLS + 1] = {};
uint8_t gateAsciiTone[YG_ASCII_ROWS][YG_ASCII_COLS] = {};
uint8_t murphMorseLine[YG_ASCII_COLS] = {};
uint8_t murphTesseractCols[YG_ASCII_COLS] = {};
uint32_t lastAsciiHashMs = 0;

float imuBiasGx = 0, imuBiasGy = 0, imuBiasGz = 0;
float imuNeutralAx = 0, imuNeutralAy = 0, imuNeutralAz = 1;
float imuRoll = 0, imuPitch = 0;
float imuShock = 0, imuLoss = 0, imuPredShock = 1;
float imuMoveX = 0, imuMoveY = 0;
float imuManualIntent = 0;
float imuPoseDist = 0;
bool imuReady = false;
bool imuPoseAdaptive = true;
uint8_t imuGripMode = 0; // 0 flat/landscape, 1 portrait vertical, 2 side vertical
uint32_t imuStillSinceMs = 0;
uint32_t imuLastAutoPoseMs = 0;
uint32_t imuPoseSettleUntilMs = 0;

uint32_t lastInputMs = 0;
uint32_t lastFrameMs = 0;
uint32_t lastDrawMs = 0;
uint32_t lastHbMs = 0;
uint32_t lastEntropyMs = 0;
uint32_t lastPnCortexMs = 0;
uint32_t lastTailEventMs = 0;
uint32_t lastDiagMs = 0;
uint32_t lastStateSaveMs = 0;
uint32_t lastStateTouchMs = 0;
uint32_t lastNasMs = 0;
uint32_t nasWarpUntilMs = 0;
uint32_t nasReports = 0;
uint32_t nasFails = 0;
uint32_t nasBrainReports = 0;
uint32_t nasBrainFails = 0;
uint32_t nasBrainLastOkMs = 0;
uint32_t nasBrainBackoffUntilMs = 0;
int nasLastCode = 0;
int nasBrainLastCode = 0;
bool nasOnline = false;
bool nasBrainOnline = false;

bool topDownPrev = false;
uint32_t topDownAt = 0;
bool topLongDone = false;
uint8_t topTapCount = 0;
uint32_t topTapWindowMs = 0;
uint32_t lastBrightnessStepMs = 0;
uint32_t brightnessPauseUntilMs = 0;
bool brightnessDirDown = true;
bool blueDownPrev = false;
uint32_t blueDownAt = 0;
bool blueLongDone = false;
uint8_t blueTapCount = 0;
uint32_t blueTapWindowMs = 0;

bool irSigilEnabled = false;
uint32_t lastIrMs = 0;
uint32_t lastIrBurstMs = 0;
uint32_t lastSkyIrMs = 0;
uint32_t pnCortexSeq = 0;
uint32_t pnCortexPrevHash = 0;
rmt_channel_handle_t irTxChan = NULL;
rmt_encoder_handle_t irCopyEncoder = NULL;
bool irReady = false;
bool yaksStateDirty = false;
bool yaksStateLoaded = false;

uint8_t brightnessLevels[] = {0, 2, 5, 10, 18, 30, 48, 72, 110, 160, 220};
uint8_t brightnessIdx = 6;

bool topButtonDown();

float clampf(float v, float lo, float hi) {
  if (!isfinite(v)) return lo;
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float smooth01(float x) {
  x = clampf(x, 0.0f, 1.0f);
  return x * x * (3.0f - 2.0f * x);
}

uint16_t hash16(const char* s) {
  uint16_t h = 0x811C;
  while (s && *s) {
    h ^= (uint8_t)*s++;
    h = (uint16_t)(h * 167U + 13U);
  }
  return h;
}

uint32_t mix32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7FEB352DUL;
  x ^= x >> 15;
  x *= 0x846CA68BUL;
  x ^= x >> 16;
  return x;
}

void writeLE32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

void hashToShareOrder(const uint8_t raw[32], uint8_t out[32]) {
  for (int i = 0; i < 32; ++i) out[i] = raw[31 - i];
}

uint16_t countLeadingZeroBits(const uint8_t h[32]) {
  uint16_t bits = 0;
  for (int i = 0; i < 32; ++i) {
    uint8_t b = h[i];
    if (b == 0) {
      bits += 8;
      continue;
    }
    for (int k = 7; k >= 0; --k) {
      if ((b & (1 << k)) == 0) bits++;
      else return bits;
    }
  }
  return bits;
}

bool hashMeetsTargetBytes(const uint8_t hash[32], const uint8_t target[32]) {
  for (int i = 0; i < 32; ++i) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;
}

void doubleSha256(const uint8_t* data, size_t len, uint8_t out[32]) {
  uint8_t first[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, data, len);
  mbedtls_sha256_finish(&ctx, first);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, first, 32);
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);
}

uint16_t workerId() {
  if (workerIdCache) return workerIdCache;
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  uint16_t v = ((uint16_t)mac[4] << 8) | mac[5];
  if (!v) v = 0x5A59;
  workerIdCache = v;
  return v;
}

int8_t currentRssi() {
  return (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : -127;
}

uint8_t currentSector() {
  int sx = (int)floorf((gate.shipX + 1.0f) * 3.99f);
  int sy = (int)floorf((gate.shipY + 1.0f) * 1.99f);
  sx = max(0, min(7, sx));
  sy = max(0, min(3, sy));
  return (uint8_t)((sx ^ (sy << 1) ^ (bestBits & 7)) & 15);
}

const char* phaseName(uint8_t phase) {
  if (phase == 2) return "WAKE";
  if (phase == 3) return "MIRROR";
  return "SCOUT";
}

const char* laneName() {
  if (!gate.autoPilot) return "MAN";
  if (gate.stability > 0.74f) return "SKAY";
  return "AUTO";
}

uint32_t safeElapsedMs(uint32_t now, uint32_t then);

const char* blackStarLaneName(uint8_t lane) {
  switch (lane & 3) {
    case 1: return "ORBIT";
    case 2: return "LENS";
    case 3: return "HORIZON";
    default: return "LINEAR";
  }
}

bool blackStarActive(uint32_t now = millis()) {
  return blackStarLink.seen && safeElapsedMs(now, blackStarLink.lastMs) < YG_BLACKSTAR_TTL_MS;
}

float blackStarPull(uint32_t now = millis()) {
  if (!blackStarActive(now)) return 0.0f;
  float age = clampf(1.0f - (float)safeElapsedMs(now, blackStarLink.lastMs) / (float)YG_BLACKSTAR_TTL_MS, 0.0f, 1.0f);
  return clampf(blackStarLink.influence * (0.35f + age * 0.65f), 0.0f, 1.0f);
}

bool mercuryFieldActive(uint32_t now = millis()) {
  return blackStarLink.mercuryMs && safeElapsedMs(now, blackStarLink.mercuryMs) < YG_MERCURY_WARP_TTL_MS;
}

bool anchorReady(uint32_t now = millis()) {
  if (!brothers.anchorSeen || safeElapsedMs(now, brothers.anchorMs) > YG_BROTHER_READY_TTL_MS) return false;
  return brothers.anchorOxy > 0.56f || brothers.anchorVacuum > 0.62f;
}

bool gladiusReady(uint32_t now = millis()) {
  if (!brothers.gladiusSeen || safeElapsedMs(now, brothers.gladiusMs) > YG_BROTHER_READY_TTL_MS) return false;
  return brothers.gladiusOxy > 0.56f || brothers.gladiusVacuum > 0.62f;
}

bool brothersReady(uint32_t now = millis()) {
  return anchorReady(now) && gladiusReady(now);
}

float mercuryWarpPower(uint32_t now = millis()) {
  if (!mercuryWarp.launch || now >= mercuryWarp.launchUntilMs) return 0.0f;
  float age = clampf(1.0f - (float)(mercuryWarp.launchUntilMs - now) / (float)YG_MERCURY_WARP_MS, 0.0f, 1.0f);
  float tail = 1.0f - age * 0.44f;
  return clampf(mercuryWarp.charge * tail, 0.0f, 1.0f);
}

bool mercuryWarpReady(uint32_t now = millis()) {
  if (!mercuryFieldActive(now) || !brothersReady(now)) return false;
  return gate.charge > 0.62f && gate.stability > 0.34f;
}

uint32_t safeElapsedMs(uint32_t now, uint32_t then) {
  if (!then) return 0UL;
  uint32_t d = now - then;
  return (d > 0x7FFFFFFFUL) ? 0UL : d;
}

uint8_t yaksCorpusLaneSlot(uint8_t phase) {
  if (phase == 2) return 2;
  if (phase == 3) return 3;
  return gate.autoPilot ? 1 : 0;
}

const char* yaksCorpusLaneLabel(uint8_t phase) {
  if (phase == 2) return "STALE";
  if (phase == 3) return "POOL";
  return laneName();
}

void updateYaksCorpusChemistry(uint8_t phase, uint16_t zbits, bool targetPass) {
  float zFit = clampf(((float)zbits - (float)targetBitsNow + 2.0f) / 10.0f, 0.0f, 1.0f);
  float darkEdge = (phase == 2 || phase == 3) ? 1.0f : 0.0f;
  float passBoost = targetPass ? 1.0f : 0.0f;
  float eventScore = clampf(zFit * 0.62f + darkEdge * 0.16f + passBoost * 0.42f, 0.0f, 1.0f);
  yaksCorpus.score = yaksCorpus.score * 0.90f + eventScore * 0.10f;

  float stalePressure = yaksCorpus.events ? (float)(yaksCorpus.stale + yaksCorpus.poolReject) / (float)yaksCorpus.events : 0.0f;
  yaksCorpus.red = clampf(stalePressure * 1.7f + (phase == 3 ? 0.20f : 0.0f), 0.0f, 1.0f);
  yaksCorpus.blue = clampf(gate.stability * 0.72f + gate.skyLock * 0.28f, 0.0f, 1.0f);
  yaksCorpus.gold = clampf(yaksCorpus.score * 0.76f + ((float)yaksCorpus.bestZ / 40.0f) * 0.24f, 0.0f, 1.0f);

  yaksCorpus.acid = yaksCorpus.acid * 0.94f + yaksCorpus.red * 0.06f;
  yaksCorpus.yeast = yaksCorpus.yeast * 0.93f + yaksCorpus.gold * 0.07f;
  yaksCorpus.bacteria = yaksCorpus.bacteria * 0.94f + yaksCorpus.blue * 0.06f;
  yaksCorpus.paradox = clampf(fabsf(yaksCorpus.gold - yaksCorpus.red) * 0.55f + darkEdge * 0.20f, 0.0f, 1.0f);
}

float curvedControl(float v, float deadzone) {
  float a = fabsf(v);
  if (a <= deadzone) return 0.0f;
  float n = (a - deadzone) / max(0.001f, 1.0f - deadzone);
  n = smooth01(n);
  return v < 0 ? -n : n;
}

float gateControl(float v) {
  float a = fabsf(v);
  if (a < 0.145f) return 0.0f;
  float out;
  if (a < 0.55f) {
    float n = smooth01((a - 0.145f) / 0.405f);
    out = 0.06f + n * 0.32f;
  } else {
    float n = smooth01((a - 0.55f) / 0.45f);
    out = 0.38f + n * 0.62f;
  }
  return v < 0 ? -out : out;
}

void markYaksStateDirty() {
  yaksStateDirty = true;
  lastStateTouchMs = millis();
}

int findJsonValue(const String& s, const char* key) {
  String pat = String("\"") + key + "\":";
  int p = s.indexOf(pat);
  if (p < 0) return -1;
  p += pat.length();
  while (p < (int)s.length() && (s[p] == ' ' || s[p] == '\t' || s[p] == '"')) p++;
  return p;
}

uint32_t jsonU32(const String& s, const char* key, uint32_t def) {
  int p = findJsonValue(s, key);
  if (p < 0) return def;
  return (uint32_t)strtoul(s.c_str() + p, nullptr, 10);
}

uint8_t jsonU8(const String& s, const char* key, uint8_t def) {
  return (uint8_t)min(255UL, (unsigned long)jsonU32(s, key, def));
}

float jsonFloat(const String& s, const char* key, float def) {
  int p = findJsonValue(s, key);
  if (p < 0) return def;
  float v = (float)atof(s.c_str() + p);
  return isfinite(v) ? v : def;
}

bool jsonBool(const String& s, const char* key, bool def) {
  int p = findJsonValue(s, key);
  if (p < 0) return def;
  const char* c = s.c_str() + p;
  if (!strncmp(c, "true", 4) || *c == '1') return true;
  if (!strncmp(c, "false", 5) || *c == '0') return false;
  return def;
}

void jsonString(const String& s, const char* key, char* out, size_t outLen, const char* def) {
  if (!out || outLen == 0) return;
  snprintf(out, outLen, "%s", def ? def : "");
  String pat = String("\"") + key + "\":\"";
  int p = s.indexOf(pat);
  if (p < 0) return;
  p += pat.length();
  int e = s.indexOf('"', p);
  if (e < 0) return;
  size_t n = min((size_t)(e - p), outLen - 1);
  memcpy(out, s.c_str() + p, n);
  out[n] = 0;
}

bool saveYaksState(bool force = false) {
  uint32_t now = millis();
  if (!force && !yaksStateDirty && now - lastStateSaveMs < YG_STATE_PERIODIC_MS) return false;
  if (!force && yaksStateDirty && now - lastStateSaveMs < YG_STATE_SAVE_MS) return false;

  File f = LittleFS.open(YG_STATE_TMP_PATH, "w");
  if (!f) return false;
  f.printf("{\"schema\":\"yaks-state-1\",\"version\":\"%s\",\"ms\":%lu,"
           "\"best\":%lu,\"target\":%u,\"opens\":%lu,\"sigils\":%lu,\"skySigils\":%lu,"
           "\"ir\":%u,\"brightness\":%u,\"charge\":%.3f,\"stab\":%.3f,\"aperture\":%.3f,"
           "\"corp_events\":%lu,\"corp_best\":%u,\"corp_pass\":%lu,\"corp_stale\":%lu,"
           "\"corp_pool\":%lu,\"corp_ztail\":%lu,\"corp_nonce\":%lu,\"corp_job\":%lu,"
           "\"corp_score\":%.4f,\"red\":%.4f,\"blue\":%.4f,\"gold\":%.4f,"
           "\"acid\":%.4f,\"yeast\":%.4f,\"bacteria\":%.4f,\"paradox\":%.4f,"
           "\"lane\":\"%s\",\"phase\":\"%s\","
           "\"bs_seen\":%u,\"bs_lane\":%u,\"bs_best\":%u,\"bs_conf\":%.4f,\"bs_loss\":%.4f,"
           "\"bs_pull\":%.4f,\"bs_corpus\":%lu,\"bs_seq\":%lu,\"bs_source\":\"%s\","
           "\"bs_hg\":%.2f,\"bs_void\":%.4f,\"bs_time\":%.4f,\"bs_vapor\":%.4f,\"bs_balance\":%.4f,"
           "\"bro_a\":%.4f,\"bro_g\":%.4f,\"bro_av\":%.4f,\"bro_gv\":%.4f,\"warp_seq\":%lu,\"warp_charge\":%.4f,"
           "\"fb_state\":%u,\"fb_job\":%u,\"fb_sector\":%u,\"fb_pred\":%u,\"fb_prio\":%u,"
           "\"fb_world\":%lu,\"fb_seq\":%lu,\"fb_rx\":%lu,\"fb_tx\":%lu,\"fb_ir\":%lu,"
           "\"fb_conf\":%.4f,\"fb_entropy\":%.4f,\"fb_activity\":%.4f,\"fb_heat\":%.4f,\"fb_load\":%.4f,"
           "\"fb_source\":\"%s\"}\n",
           YG_VERSION, (unsigned long)now,
           (unsigned long)bestBits, (unsigned)targetBitsNow,
           (unsigned long)gate.opens, (unsigned long)gate.sigils, (unsigned long)gate.skySigils,
           irSigilEnabled ? 1 : 0, (unsigned)brightnessIdx,
           gate.charge, gate.stability, gate.aperture,
           (unsigned long)yaksCorpus.events, (unsigned)yaksCorpus.bestZ,
           (unsigned long)yaksCorpus.targetPass, (unsigned long)yaksCorpus.stale,
           (unsigned long)yaksCorpus.poolReject, (unsigned long)yaksCorpus.zTail,
           (unsigned long)yaksCorpus.lastNonce, (unsigned long)yaksCorpus.lastJobSeq,
           yaksCorpus.score, yaksCorpus.red, yaksCorpus.blue, yaksCorpus.gold,
           yaksCorpus.acid, yaksCorpus.yeast, yaksCorpus.bacteria, yaksCorpus.paradox,
           yaksCorpus.lastLane, yaksCorpus.lastPhase,
           blackStarLink.seen ? 1 : 0, (unsigned)blackStarLink.lane, (unsigned)blackStarLink.bestLane,
           blackStarLink.confidence, blackStarLink.loss, blackStarLink.influence,
           (unsigned long)blackStarLink.corpus, (unsigned long)blackStarLink.seq, blackStarLink.source,
           blackStarLink.mercuryTorr, blackStarLink.torricelliVoid, blackStarLink.mercuryTime,
           blackStarLink.hawkingVapor, blackStarLink.horizonBalance,
           brothers.anchorOxy, brothers.gladiusOxy, brothers.anchorVacuum, brothers.gladiusVacuum,
           (unsigned long)mercuryWarp.readySeq, mercuryWarp.charge,
           (unsigned)flashBubble.state, (unsigned)flashBubble.jobState,
           (unsigned)flashBubble.sector, (unsigned)flashBubble.predictedSector,
           (unsigned)flashBubble.priority, (unsigned long)flashBubble.worldFlags,
           (unsigned long)flashBubble.seq, (unsigned long)flashBubble.rx,
           (unsigned long)flashBubble.tx, (unsigned long)flashBubble.irBeaconTx,
           flashBubble.confidence, flashBubble.entropy, flashBubble.activity,
           flashBubble.siliconHeat, flashBubble.siliconLoad, flashBubble.source);
  f.close();
  LittleFS.remove(YG_STATE_PATH);
  if (!LittleFS.rename(YG_STATE_TMP_PATH, YG_STATE_PATH)) return false;
  yaksStateDirty = false;
  yaksStateLoaded = true;
  lastStateSaveMs = now;
  return true;
}

bool loadYaksState() {
  File f = LittleFS.open(YG_STATE_PATH, "r");
  if (!f) return false;
  String s = f.readString();
  f.close();
  if (s.indexOf("\"schema\":\"yaks-state-1\"") < 0) return false;

  bestBits = max(bestBits, jsonU32(s, "best", bestBits));
  targetBitsNow = (uint16_t)jsonU32(s, "target", targetBitsNow);
  gate.opens = jsonU32(s, "opens", gate.opens);
  gate.sigils = jsonU32(s, "sigils", gate.sigils);
  gate.skySigils = jsonU32(s, "skySigils", gate.skySigils);
  irSigilEnabled = jsonBool(s, "ir", irSigilEnabled);
  brightnessIdx = jsonU8(s, "brightness", brightnessIdx);
  uint8_t maxBright = (uint8_t)((sizeof(brightnessLevels) / sizeof(brightnessLevels[0])) - 1);
  if (brightnessIdx > maxBright) brightnessIdx = maxBright;
  gate.charge = clampf(jsonFloat(s, "charge", gate.charge), 0.0f, 1.0f);
  gate.stability = clampf(jsonFloat(s, "stab", gate.stability), 0.0f, 1.0f);
  gate.aperture = clampf(jsonFloat(s, "aperture", gate.aperture), 0.0f, 1.0f);

  yaksCorpus.events = jsonU32(s, "corp_events", yaksCorpus.events);
  yaksCorpus.bestZ = (uint16_t)jsonU32(s, "corp_best", yaksCorpus.bestZ);
  yaksCorpus.targetPass = jsonU32(s, "corp_pass", yaksCorpus.targetPass);
  yaksCorpus.stale = jsonU32(s, "corp_stale", yaksCorpus.stale);
  yaksCorpus.poolReject = jsonU32(s, "corp_pool", yaksCorpus.poolReject);
  yaksCorpus.zTail = jsonU32(s, "corp_ztail", yaksCorpus.zTail);
  yaksCorpus.lastNonce = jsonU32(s, "corp_nonce", yaksCorpus.lastNonce);
  yaksCorpus.lastJobSeq = jsonU32(s, "corp_job", yaksCorpus.lastJobSeq);
  yaksCorpus.score = clampf(jsonFloat(s, "corp_score", yaksCorpus.score), 0.0f, 1.0f);
  yaksCorpus.red = clampf(jsonFloat(s, "red", yaksCorpus.red), 0.0f, 1.0f);
  yaksCorpus.blue = clampf(jsonFloat(s, "blue", yaksCorpus.blue), 0.0f, 1.0f);
  yaksCorpus.gold = clampf(jsonFloat(s, "gold", yaksCorpus.gold), 0.0f, 1.0f);
  yaksCorpus.acid = clampf(jsonFloat(s, "acid", yaksCorpus.acid), 0.0f, 1.0f);
  yaksCorpus.yeast = clampf(jsonFloat(s, "yeast", yaksCorpus.yeast), 0.0f, 1.0f);
  yaksCorpus.bacteria = clampf(jsonFloat(s, "bacteria", yaksCorpus.bacteria), 0.0f, 1.0f);
  yaksCorpus.paradox = clampf(jsonFloat(s, "paradox", yaksCorpus.paradox), 0.0f, 1.0f);
  jsonString(s, "lane", yaksCorpus.lastLane, sizeof(yaksCorpus.lastLane), yaksCorpus.lastLane);
  jsonString(s, "phase", yaksCorpus.lastPhase, sizeof(yaksCorpus.lastPhase), yaksCorpus.lastPhase);
  blackStarLink.seen = jsonBool(s, "bs_seen", blackStarLink.seen);
  blackStarLink.lane = jsonU8(s, "bs_lane", blackStarLink.lane) & 3;
  blackStarLink.bestLane = jsonU8(s, "bs_best", blackStarLink.bestLane) & 3;
  blackStarLink.confidence = clampf(jsonFloat(s, "bs_conf", blackStarLink.confidence), 0.0f, 1.0f);
  blackStarLink.loss = clampf(jsonFloat(s, "bs_loss", blackStarLink.loss), 0.0f, 2.0f);
  blackStarLink.influence = clampf(jsonFloat(s, "bs_pull", blackStarLink.influence), 0.0f, 1.0f);
  blackStarLink.corpus = jsonU32(s, "bs_corpus", blackStarLink.corpus);
  blackStarLink.seq = jsonU32(s, "bs_seq", blackStarLink.seq);
  jsonString(s, "bs_source", blackStarLink.source, sizeof(blackStarLink.source), blackStarLink.source);
  blackStarLink.mercuryTorr = clampf(jsonFloat(s, "bs_hg", blackStarLink.mercuryTorr), 540.0f, 940.0f);
  blackStarLink.torricelliVoid = clampf(jsonFloat(s, "bs_void", blackStarLink.torricelliVoid), 0.0f, 1.5f);
  blackStarLink.mercuryTime = clampf(jsonFloat(s, "bs_time", blackStarLink.mercuryTime), 0.0f, 1.5f);
  blackStarLink.hawkingVapor = clampf(jsonFloat(s, "bs_vapor", blackStarLink.hawkingVapor), 0.0f, 1.5f);
  blackStarLink.horizonBalance = clampf(jsonFloat(s, "bs_balance", blackStarLink.horizonBalance), 0.0f, 1.0f);
  brothers.anchorOxy = clampf(jsonFloat(s, "bro_a", brothers.anchorOxy), 0.0f, 1.0f);
  brothers.gladiusOxy = clampf(jsonFloat(s, "bro_g", brothers.gladiusOxy), 0.0f, 1.0f);
  brothers.anchorVacuum = clampf(jsonFloat(s, "bro_av", brothers.anchorVacuum), 0.0f, 1.0f);
  brothers.gladiusVacuum = clampf(jsonFloat(s, "bro_gv", brothers.gladiusVacuum), 0.0f, 1.0f);
  mercuryWarp.readySeq = jsonU32(s, "warp_seq", mercuryWarp.readySeq);
  mercuryWarp.charge = clampf(jsonFloat(s, "warp_charge", mercuryWarp.charge), 0.0f, 1.0f);
  blackStarLink.lastMs = 0;
  blackStarLink.mercuryMs = 0;
  mercuryWarp.armed = false;
  mercuryWarp.launch = false;
  mercuryWarp.launchUntilMs = 0;
  flashBubble.state = jsonU8(s, "fb_state", flashBubble.state) & 3;
  flashBubble.jobState = jsonU8(s, "fb_job", flashBubble.jobState);
  flashBubble.sector = jsonU8(s, "fb_sector", flashBubble.sector) & 7;
  flashBubble.predictedSector = jsonU8(s, "fb_pred", flashBubble.predictedSector) & 7;
  flashBubble.priority = jsonU8(s, "fb_prio", flashBubble.priority);
  flashBubble.worldFlags = jsonU32(s, "fb_world", flashBubble.worldFlags);
  flashBubble.seq = jsonU32(s, "fb_seq", flashBubble.seq);
  flashBubble.rx = jsonU32(s, "fb_rx", flashBubble.rx);
  flashBubble.tx = jsonU32(s, "fb_tx", flashBubble.tx);
  flashBubble.irBeaconTx = jsonU32(s, "fb_ir", flashBubble.irBeaconTx);
  flashBubble.confidence = clampf(jsonFloat(s, "fb_conf", flashBubble.confidence), 0.0f, 1.5f);
  flashBubble.entropy = clampf(jsonFloat(s, "fb_entropy", flashBubble.entropy), 0.0f, 4.0f);
  flashBubble.activity = clampf(jsonFloat(s, "fb_activity", flashBubble.activity), 0.0f, 4.0f);
  flashBubble.siliconHeat = clampf(jsonFloat(s, "fb_heat", flashBubble.siliconHeat), 0.0f, 3.0f);
  flashBubble.siliconLoad = clampf(jsonFloat(s, "fb_load", flashBubble.siliconLoad), 0.0f, 3.0f);
  jsonString(s, "fb_source", flashBubble.source, sizeof(flashBubble.source), flashBubble.source);
  flashBubble.lastRxMs = 0;
  flashBubble.lastTxMs = 0;
  flashBubble.lastIrBeaconMs = 0;
  yaksStateLoaded = true;
  return true;
}

void serviceYaksState() {
  saveYaksState(false);
}

void logTailJson(uint8_t phase, uint16_t zbits, uint32_t nonce, uint8_t reason, bool targetPass) {
  File f = LittleFS.open(YG_LOG_PATH, "a");
  if (!f) return;
  uint32_t now = millis();
  f.printf("{\"ms\":%lu,\"phase\":\"%s\",\"zbits\":%u,\"lane\":\"%s\",\"sector\":%u,\"worker\":%u,\"job_seq\":%lu,\"job_age_ms\":%lu,\"ttn_ms\":%ld,\"after_clean_ms\":%lu,\"pool_rejects\":%lu,\"reason\":%u,\"nonce\":%lu,\"target_pass\":%u,\"charge\":%.3f,\"stab\":%.3f}\n",
           (unsigned long)now,
           phaseName(phase),
           (unsigned)zbits,
           laneName(),
            (unsigned)currentSector(),
            (unsigned)workerId(),
            (unsigned long)jobSeq,
            (unsigned long)safeElapsedMs(now, currentJob.rxMs),
            (long)max(0.0f, jobGapEmaMs - (float)safeElapsedMs(now, lastJobRxMs)),
            (unsigned long)safeElapsedMs(now, lastCleanJobMs),
            (unsigned long)buzzRejects,
            (unsigned)reason,
            (unsigned long)nonce,
           targetPass ? 1 : 0,
           gate.charge,
           gate.stability);
  f.close();
}

void logYaksCorpusJson(uint8_t phase, uint16_t zbits, uint32_t nonce, uint8_t reason, bool targetPass) {
  File f = LittleFS.open(YG_CORPUS_PATH, "a");
  if (!f) return;
  uint32_t now = millis();
  f.printf("{\"ms\":%lu,\"schema\":\"yaks-a9-lite-corpus-1\",\"observer_only\":1,\"wire_change_required\":0,"
           "\"phase\":\"%s\",\"lane\":\"%s\",\"sector\":%u,\"worker\":%u,"
           "\"zbits\":%u,\"best_z\":%u,\"target_bits\":%u,\"nonce\":%lu,\"job_seq\":%lu,\"job_age_ms\":%lu,"
           "\"reason\":%u,\"target_pass\":%u,\"stale\":%lu,\"poolR\":%lu,"
           "\"score\":%.3f,\"red\":%.3f,\"blue\":%.3f,\"gold\":%.3f,"
           "\"acid\":%.3f,\"yeast\":%.3f,\"bacteria\":%.3f,\"paradox\":%.3f,"
           "\"charge\":%.3f,\"stab\":%.3f}\n",
           (unsigned long)now,
           phaseName(phase),
           yaksCorpus.lastLane,
           (unsigned)currentSector(),
           (unsigned)workerId(),
           (unsigned)zbits,
           (unsigned)yaksCorpus.bestZ,
           (unsigned)targetBitsNow,
           (unsigned long)nonce,
           (unsigned long)jobSeq,
           (unsigned long)safeElapsedMs(now, currentJob.rxMs),
           (unsigned)reason,
           targetPass ? 1 : 0,
           (unsigned long)yaksCorpus.stale,
           (unsigned long)yaksCorpus.poolReject,
           yaksCorpus.score,
           yaksCorpus.red,
           yaksCorpus.blue,
           yaksCorpus.gold,
           yaksCorpus.acid,
           yaksCorpus.yeast,
           yaksCorpus.bacteria,
           yaksCorpus.paradox,
           gate.charge,
           gate.stability);
  f.close();
}

void observeYaksCorpus(uint8_t phase, uint16_t zbits, uint32_t nonce, uint8_t reason, bool targetPass) {
  uint32_t now = millis();
  yaksCorpus.events++;
  yaksCorpus.bestZ = max(yaksCorpus.bestZ, zbits);
  yaksCorpus.lastNonce = nonce;
  yaksCorpus.lastJobSeq = jobSeq;
  yaksCorpus.lastEventMs = now;
  snprintf(yaksCorpus.lastPhase, sizeof(yaksCorpus.lastPhase), "%s", phaseName(phase));
  snprintf(yaksCorpus.lastLane, sizeof(yaksCorpus.lastLane), "%s", yaksCorpusLaneLabel(phase));

  uint8_t slot = yaksCorpusLaneSlot(phase);
  if (slot < 4 && yaksCorpus.laneHits[slot] < 65535) yaksCorpus.laneHits[slot]++;
  if (phase == 2) yaksCorpus.stale++;
  else if (phase == 3) yaksCorpus.poolReject++;
  else yaksCorpus.zTail++;
  if (targetPass) yaksCorpus.targetPass++;

  updateYaksCorpusChemistry(phase, zbits, targetPass);
  logYaksCorpusJson(phase, zbits, nonce, reason, targetPass);
  markYaksStateDirty();
}

void observeNearTailCorpus(uint16_t zbits, uint32_t nonce) {
  uint16_t nearFloor = targetBitsNow > 1 ? (uint16_t)(targetBitsNow - 1) : targetBitsNow;
  nearFloor = max((uint16_t)18, nearFloor);
  if (zbits < nearFloor) return;
  if (zbits <= yaksCorpus.bestZ) return;
  observeYaksCorpus(1, zbits, nonce, 5, false);
}

void cleanupTailLog() {
  static uint32_t lastCheck = 0;
  uint32_t now = millis();
  if (now - lastCheck < 30000UL) return;
  lastCheck = now;
  File f = LittleFS.open(YG_LOG_PATH, "r");
  if (!f) return;
  size_t sz = f.size();
  f.close();
  if (sz <= YG_LOG_MAX_BYTES) return;
  LittleFS.remove("/yaks_tail.old");
  LittleFS.rename(YG_LOG_PATH, "/yaks_tail.old");
  File nf = LittleFS.open(YG_LOG_PATH, "w");
  if (nf) {
    nf.printf("{\"ms\":%lu,\"event\":\"corpus_rotated\",\"old_bytes\":%lu}\n",
              (unsigned long)now, (unsigned long)sz);
    nf.close();
  }
}

void cleanupYaksCorpusLog() {
  static uint32_t lastCheck = 0;
  uint32_t now = millis();
  if (now - lastCheck < 30000UL) return;
  lastCheck = now;
  File f = LittleFS.open(YG_CORPUS_PATH, "r");
  if (!f) return;
  size_t sz = f.size();
  f.close();
  if (sz <= YG_CORPUS_MAX_BYTES) return;
  LittleFS.remove(YG_CORPUS_OLD_PATH);
  LittleFS.rename(YG_CORPUS_PATH, YG_CORPUS_OLD_PATH);
  File nf = LittleFS.open(YG_CORPUS_PATH, "w");
  if (nf) {
    nf.printf("{\"ms\":%lu,\"event\":\"corpus_rotated\",\"old_bytes\":%lu,\"schema\":\"yaks-a9-lite-corpus-1\"}\n",
              (unsigned long)now, (unsigned long)sz);
    nf.close();
  }
}

void ensurePeer() {
  if (!colonyReady) return;
  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) return;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6);
  peer.channel = 0;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

esp_err_t sendEspNow(const char* tag, const void* data, size_t len) {
  if (!colonyReady || !data || len == 0) return ESP_ERR_INVALID_STATE;
  ensurePeer();
  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)data, len);
  if (err != ESP_OK) {
    if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) esp_now_del_peer(JANUS_BROADCAST_MAC);
    ensurePeer();
    err = esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)data, len);
  }
  if (err != ESP_OK) {
    Serial.printf("[YAKS/ESP] tx fail tag=%s err=%d\n", tag ? tag : "-", (int)err);
  }
  return err;
}

void sendEvent(uint8_t eventType, const char* kind, uint8_t confidence, uint8_t urgency,
               int16_t a, int16_t b, int16_t c, int16_t d) {
  JanusEventPacket je = {};
  je.magic[0] = 'J'; je.magic[1] = 'E';
  je.version = 1;
  je.eventType = eventType;
  je.nodeRole = 12;
  je.confidence = confidence;
  je.urgency = urgency;
  snprintf(je.nodeId, sizeof(je.nodeId), "%s", YG_NODE_ID);
  snprintf(je.kind, sizeof(je.kind), "%s", kind ? kind : "yaks_gate");
  je.seq = ++colonySeq;
  je.uptimeMs = millis();
  je.topicHash = hash16(kind ? kind : "yaks_gate");
  je.objectHash = hash16("skay_gate");
  je.capabilities = 0x0008 | 0x0080 | 0x0400 | 0x1000 | 0x2000 | 0x4000 | 0x8000;
  je.valueA_x10 = a;
  je.valueB_x10 = b;
  je.valueC_x10 = c;
  je.valueD_x10 = d;
  je.eventHash = ((uint32_t)je.topicHash << 16) ^ je.objectHash ^ je.seq;
  je.ttlMs = 16000UL;
  sendEspNow("J/E", &je, sizeof(je));
}

void armMercuryWarp(const char* reason) {
  uint32_t now = millis();
  if (mercuryWarp.launch && now < mercuryWarp.launchUntilMs) return;
  if (!mercuryWarpReady(now)) return;

  float brotherOxy = clampf((brothers.anchorOxy + brothers.gladiusOxy) * 0.5f, 0.0f, 1.0f);
  float brotherVac = clampf((brothers.anchorVacuum + brothers.gladiusVacuum) * 0.5f, 0.0f, 1.0f);
  float mercuryFit = clampf(1.0f - fabsf(blackStarLink.mercuryTorr - 760.0f) / 240.0f, 0.0f, 1.0f);

  mercuryWarp.armed = true;
  mercuryWarp.launch = true;
  mercuryWarp.armedMs = now;
  mercuryWarp.launchUntilMs = now + YG_MERCURY_WARP_MS;
  mercuryWarp.readySeq++;
  mercuryWarp.charge = clampf(0.42f + gate.charge * 0.24f + brotherOxy * 0.18f +
                              blackStarLink.mercuryTime * 0.12f + blackStarLink.horizonBalance * 0.10f, 0.0f, 1.0f);
  mercuryWarp.vector = clampf((blackStarLink.torricelliVoid * 0.44f + blackStarLink.mercuryTime * 0.34f +
                               brotherVac * 0.22f), 0.0f, 1.0f);
  mercuryWarp.reverse = clampf(0.35f + mercuryFit * 0.24f + blackStarLink.hawkingVapor * 0.18f +
                               (blackStarLink.bestLane == 3 ? 0.18f : 0.0f) + yaksCorpus.gold * 0.08f, 0.0f, 1.0f);

  gate.charge = min(1.0f, gate.charge + 0.10f + mercuryWarp.charge * 0.12f);
  gate.stability = min(1.0f, gate.stability + 0.035f + mercuryWarp.reverse * 0.040f);
  gate.aperture = min(1.0f, gate.aperture + 0.10f + mercuryWarp.vector * 0.12f);
  gate.tailGlow = min(1.0f, gate.tailGlow + 0.48f);
  gate.pulse = min(1.0f, gate.pulse + 0.70f);
  gate.shake = min(1.0f, gate.shake + 0.20f);

  snprintf(statusLine, sizeof(statusLine), "Hg WARP READY");
  sendEvent(16, "yaks_mercury_warp", 98, 92,
            (int16_t)(blackStarLink.mercuryTorr),
            (int16_t)(mercuryWarp.charge * 1000.0f),
            (int16_t)(mercuryWarp.vector * 1000.0f),
            (int16_t)(mercuryWarp.reverse * 1000.0f));
  Serial.printf("[YAKS/WARP] launch reason=%s seq=%lu Hg=%.1f void=%.2f time=%.2f vapor=%.2f A=%.2f G=%.2f vac=%.2f/%.2f charge=%.2f vector=%.2f reverse=%.2f\n",
                reason ? reason : "-",
                (unsigned long)mercuryWarp.readySeq,
                blackStarLink.mercuryTorr,
                blackStarLink.torricelliVoid,
                blackStarLink.mercuryTime,
                blackStarLink.hawkingVapor,
                brothers.anchorOxy,
                brothers.gladiusOxy,
                brothers.anchorVacuum,
                brothers.gladiusVacuum,
                mercuryWarp.charge,
                mercuryWarp.vector,
                mercuryWarp.reverse);
  markYaksStateDirty();
}

void sendTailPacket(uint8_t phase, uint8_t strategy, uint8_t lane, uint16_t zbits,
                    uint32_t nonce, uint8_t reason, bool targetPass, const uint8_t hashTail[4]) {
  uint32_t now = millis();
  if (phase != 3 && now - lastTailEventMs < YG_TAIL_EVENT_MS) return;
  lastTailEventMs = now;

  RejectTailPacket rt = {};
  rt.magic[0] = 'R'; rt.magic[1] = 'T';
  rt.version = 1;
  rt.phase = phase;
  rt.strategy = strategy;
  rt.lane = lane;
  rt.sector = currentSector();
  snprintf(rt.nodeId, sizeof(rt.nodeId), "%s", YG_NODE_ID);
  rt.seq = ++tailSeq;
  rt.worker_id = workerId();
  rt.zbits = zbits;
  rt.targetBits = targetBitsNow;
  rt.jobSeq = jobSeq;
  rt.jobAgeMs = safeElapsedMs(now, currentJob.rxMs);
  rt.timeToNextJobMs = (int32_t)max(0.0f, jobGapEmaMs - (float)safeElapsedMs(now, lastJobRxMs));
  rt.timeAfterCleanJobMs = safeElapsedMs(now, lastCleanJobMs);
  rt.nonce = nonce;
  rt.poolRejects = buzzRejects;
  rt.staleDropped = staleDropped;
  rt.staleReason = reason;
  rt.targetPass = targetPass ? 1 : 0;
  if (hashTail) memcpy(rt.hashTail, hashTail, 4);
  rt.rssi = currentRssi();
  rt.uptimeMs = now;
  sendEspNow("R/T", &rt, sizeof(rt));

  tailEvents++;
  if (phase == 2) staleTailEvents++;
  else if (phase == 3) poolRejectEvents++;
  else zTailEvents++;

  gate.tailGlow = min(1.0f, gate.tailGlow + 0.20f + (float)zbits * 0.008f);
  gate.charge = min(1.0f, gate.charge + 0.010f + (float)max(0, (int)zbits - 18) * 0.003f);
  if (targetPass) gate.stability = min(1.0f, gate.stability + 0.12f);
  snprintf(lastTailLine, sizeof(lastTailLine), "%s z%u n%08lX", phaseName(phase), (unsigned)zbits, (unsigned long)nonce);
  logTailJson(phase, zbits, nonce, reason, targetPass);
  observeYaksCorpus(phase, zbits, nonce, reason, targetPass);
}

void sendHeartbeat() {
  JanusColonyPacket pkt = {};
  memcpy(pkt.magic, "JANUS", 6);
  snprintf(pkt.nodeId, sizeof(pkt.nodeId), "%s", YG_NODE_ID);
  snprintf(pkt.role, sizeof(pkt.role), "%s", "YaksGate");
  pkt.seq = ++colonySeq;
  pkt.hashRate = observerHashrate;
  pkt.shares = gate.opens;
  pkt.rejects = poolRejectEvents + staleTailEvents;
  pkt.bestBits = bestBits;
  pkt.diff = buzzDiff;
  pkt.targetBits = targetBitsNow;
  pkt.aiBatch = gate.autoPilot ? 220 : 120;
  pkt.aiHint = irSigilEnabled ? 4 : (gate.autoPilot ? 2 : 1);
  pkt.jobAgeMs = safeElapsedMs(millis(), currentJob.rxMs);
  pkt.rssi = currentRssi();
  pkt.uptime = millis() / 1000UL;
  sendEspNow("HB", &pkt, sizeof(pkt));
}

void sendEntropy() {
  float entropy = clampf((float)bestBits / 32.0f + imuLoss * 0.45f + gate.charge + gate.tailGlow * 0.25f, 0.0f, 4.0f);
  float predictionError = imuLoss + (float)staleTailEvents * 0.01f + (1.0f - gate.stability) * 0.2f;
  float syncHint = buzzSeen ? clampf(1.0f - (float)safeElapsedMs(millis(), lastBuzzMs) / 12000.0f, 0.0f, 1.0f) : 0.0f;
  float fit = clampf((float)observerHashrate / 9000.0f + gate.stability * 0.45f, 0.0f, 2.0f);
  uint8_t sensorFlags = 0x08 | 0x80; // bit3=IMU, bit7=RejectTail observer

  EntropyReport er1 = {};
  er1.magic[0] = 'E'; er1.magic[1] = 'R';
  er1.worker_id = workerId();
  er1.local_entropy = entropy;
  er1.sensor_flags = sensorFlags;
  er1.values[0] = imuRoll;
  er1.values[1] = imuPitch;
  er1.values[2] = imuShock;
  er1.values[3] = gate.stability;
  sendEspNow("ER", &er1, sizeof(er1));

  EntropyReportV2 er = {};
  er.magic[0] = 'E'; er.magic[1] = '2';
  er.worker_id = workerId();
  snprintf(er.nodeId, sizeof(er.nodeId), "%s", YG_NODE_ID);
  er.local_entropy = entropy;
  er.prediction_error = predictionError;
  er.sync_hint = syncHint;
  er.fit = fit;
  er.sensor_flags = sensorFlags;
  er.values[0] = imuRoll;
  er.values[1] = imuPitch;
  er.values[2] = imuShock;
  er.values[3] = imuLoss;
  er.values[4] = (float)bestBits;
  er.values[5] = gate.charge;
  er.values[6] = gate.stability;
  er.values[7] = (float)gate.opens;
  er.uptime_ms = millis();
  sendEspNow("E2", &er, sizeof(er));
}

uint8_t flashPredictSector(uint8_t sector, uint32_t now) {
  uint8_t drift = 0;
  if (blackStarActive(now)) drift += (blackStarLink.bestLane + 1) & 3;
  if (gate.skyLock > 0.55f) drift += 2;
  if (flashBubble.eventPower > 0.85f) drift += 1;
  if (gate.autoPilot) drift += (uint8_t)((gate.opens + bestBits) & 1);
  return (uint8_t)((sector + drift) & 7);
}

bool flashEscapeReady(uint32_t now) {
  if (mercuryWarp.launch && now < mercuryWarp.launchUntilMs) return true;
  if (!blackStarActive(now)) return false;
  float pull = blackStarPull(now);
  bool horizonLane = (blackStarLink.bestLane == 3) || (blackStarLink.lane == 3);
  bool aligned = gate.skyLock > 0.62f || gate.charge > 0.94f;
  bool unstable = gate.stability < 0.30f;
  bool strongTrace = flashBubble.siliconLoad > 0.70f && flashBubble.siliconHeat > 0.52f;
  return (horizonLane && (pull > 0.28f || aligned || unstable)) ||
         (pull > 0.55f && (aligned || strongTrace)) ||
         (unstable && pull > 0.34f && flashBubble.siliconLoad > 0.62f);
}

void updateFlashBubbleWorld() {
  uint32_t now = millis();
  bool freshK2 = flashBubble.lastRxMs && safeElapsedMs(now, flashBubble.lastRxMs) < YG_FLASH_BUBBLE_TTL_MS;
  bool freshJob = currentJob.active && safeElapsedMs(now, currentJob.rxMs) < YG_REMOTE_JOB_TTL_MS;
  bool bsActive = blackStarActive(now);
  float bsPull = blackStarPull(now);
  float warpPower = mercuryWarpPower(now);
  float syncHint = buzzSeen ? clampf(1.0f - (float)safeElapsedMs(now, lastBuzzMs) / 12000.0f, 0.0f, 1.0f) : 0.0f;

  flashBubble.siliconLoad = clampf((float)observerHashrate / 9000.0f + (freshJob ? 0.24f : 0.0f) + gate.charge * 0.42f + bsPull * 0.30f + warpPower * 0.42f, 0.0f, 3.0f);
  flashBubble.siliconHeat = clampf(gate.heat * 0.85f + flashBubble.siliconLoad * 0.32f + (irSigilEnabled ? 0.08f : 0.0f) + blackStarLink.hawkingVapor * 0.10f, 0.0f, 3.0f);
  float localEntropy = clampf((float)bestBits / 32.0f + yaksCorpus.score + gate.tailGlow * 0.40f + bsPull * 0.70f + imuLoss * 0.20f + warpPower * 0.55f, 0.0f, 4.0f);
  float localActivity = clampf(gate.tailGlow + gate.pulse * 0.65f + imuManualIntent * 0.70f + flashBubble.siliconLoad * 0.28f + bsPull * 0.60f + warpPower * 0.70f, 0.0f, 4.0f);
  float localConfidence = clampf(gate.stability * 0.44f + syncHint * 0.20f + (1.0f / (1.0f + yaksCorpus.paradox)) * 0.14f + bsPull * 0.22f + warpPower * 0.22f, 0.0f, 1.5f);

  flashBubble.entropy = flashBubble.entropy * 0.84f + localEntropy * 0.16f;
  flashBubble.activity = flashBubble.activity * 0.82f + localActivity * 0.18f;
  flashBubble.confidence = flashBubble.confidence * 0.86f + localConfidence * 0.14f;
  flashBubble.eventPower = flashBubble.eventPower * 0.84f + (localActivity + localEntropy * 0.24f + bsPull) * 0.16f;

  uint8_t localSector = (uint8_t)((currentSector() + (uint8_t)(gate.opens & 7) + (uint8_t)(blackStarLink.bestLane & 3)) & 7);
  if (bsActive && blackStarLink.bestLane == 3) localSector = (uint8_t)((localSector + 3) & 7);
  flashBubble.sector = localSector;
  flashBubble.predictedSector = flashPredictSector(localSector, now);

  flashBubble.worldFlags = 0;
  if (bsActive) flashBubble.worldFlags |= YF_WORLD_BLACKSTAR;
  if (freshJob) flashBubble.worldFlags |= YF_WORLD_JOB;
  if (irSigilEnabled) flashBubble.worldFlags |= YF_WORLD_IR;
  if (!gate.autoPilot) flashBubble.worldFlags |= YF_WORLD_MANUAL;
  if (nasOnline) flashBubble.worldFlags |= YF_WORLD_NAS;
  if (nasBrainOnline) flashBubble.worldFlags |= YF_WORLD_BRAIN;
  if (mercuryFieldActive(now)) flashBubble.worldFlags |= YF_WORLD_MERCURY;
  if (brothersReady(now)) flashBubble.worldFlags |= YF_WORLD_BROTHERS;
  if (mercuryWarp.launch && now < mercuryWarp.launchUntilMs) flashBubble.worldFlags |= YF_WORLD_WARP;
  if (tailEvents || gate.tailGlow > 0.20f) flashBubble.worldFlags |= YF_WORLD_TAIL;
  if (flashEscapeReady(now)) flashBubble.worldFlags |= YF_WORLD_ESCAPE;
  if (gate.stability < 0.38f || staleTailEvents > poolRejectEvents + 8) flashBubble.worldFlags |= YF_WORLD_UNSTABLE;
  if (flashBubble.siliconLoad > 0.10f) flashBubble.worldFlags |= YF_WORLD_SILICON;
  if (blackStarLink.bestLane == 3 || (freshK2 && strstr(flashBubble.source, "Core"))) flashBubble.worldFlags |= YF_WORLD_HORIZON;

  uint8_t remoteActive = freshK2 ? 1 : 0;
  uint8_t virtualNodes = freshK2 ? max((uint8_t)1, flashBubble.virtualNodes) : 0;
  flashBubble.activeNodes = (uint8_t)constrain((int)remoteActive + (bsActive ? 1 : 0) + (freshJob ? 1 : 0) + (irSigilEnabled ? 1 : 0), 0, 15);
  flashBubble.virtualNodes = (uint8_t)constrain((int)virtualNodes + (gate.autoPilot ? 2 : 0) + (nasBrainOnline ? 1 : 0), 0, 32);

  int pr = (int)(flashBubble.eventPower * 42.0f + flashBubble.confidence * 58.0f + bsPull * 72.0f + flashBubble.siliconLoad * 18.0f + warpPower * 58.0f);
  if (flashBubble.worldFlags & YF_WORLD_ESCAPE) pr += 42;
  if (freshJob) pr += 12;
  if (freshK2) pr += 10;
  flashBubble.priority = (uint8_t)constrain(pr, 0, 255);

  if ((flashBubble.worldFlags & YF_WORLD_ESCAPE) || gate.stability < 0.22f ||
      (flashBubble.eventPower > 2.35f && flashBubble.priority > 190)) {
    flashBubble.state = 3;
    flashBubble.jobState = 3;
  } else if (bsActive || irSigilEnabled || flashBubble.priority > 90 || gate.tailGlow > 0.24f) {
    flashBubble.state = 2;
    flashBubble.jobState = 2;
  } else if (freshK2 || yaksCorpus.events > 0 || gate.autoPilot) {
    flashBubble.state = 1;
    flashBubble.jobState = 4;
  } else {
    flashBubble.state = 0;
    flashBubble.jobState = 1;
  }

  if (flashEscapeReady(now)) flashBubble.jobState = 5;
}

void onJanusKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi) {
  if (kp.magic[0] != 'K' || kp.magic[1] != '2' || kp.version != 1) return;
  if (strncmp(kp.nodeId, YG_NODE_ID, sizeof(kp.nodeId)) == 0) return;
  uint32_t now = millis();
  flashBubble.rx++;
  flashBubble.lastRxMs = now;
  snprintf(flashBubble.source, sizeof(flashBubble.source), "%s", kp.nodeId[0] ? kp.nodeId : "K2");
  flashBubble.activeNodes = max(flashBubble.activeNodes, kp.activeBubbleNodes);
  flashBubble.virtualNodes = max(flashBubble.virtualNodes, kp.virtualNodes);
  flashBubble.sector = kp.sector & 7;
  flashBubble.predictedSector = kp.predictedSector & 7;
  flashBubble.priority = max(flashBubble.priority, kp.priority);
  flashBubble.entropy = flashBubble.entropy * 0.82f + clampf(kp.entropy, 0.0f, 4.0f) * 0.18f;
  flashBubble.activity = flashBubble.activity * 0.84f + clampf(kp.activity, 0.0f, 4.0f) * 0.16f;
  flashBubble.confidence = flashBubble.confidence * 0.86f + clampf(kp.confidence, 0.0f, 1.5f) * 0.14f;
  flashBubble.eventPower = flashBubble.eventPower * 0.86f + (kp.priority / 255.0f + flashBubble.activity * 0.22f) * 0.14f;

  bool hot = kp.priority >= 120 || (kp.flags & 0x03);
  if (hot) {
    gate.tailGlow = min(1.0f, gate.tailGlow + 0.08f + clampf(kp.confidence, 0.0f, 1.0f) * 0.08f);
    gate.pulse = min(1.0f, gate.pulse + 0.10f);
    gate.stability = clampf(gate.stability + clampf(kp.confidence, 0.0f, 1.0f) * 0.010f - (kp.flags & 0x02 ? 0.006f : 0.0f), 0.0f, 1.0f);
    snprintf(statusLine, sizeof(statusLine), "K2 %s P%u", flashBubble.source, (unsigned)kp.priority);
    markYaksStateDirty();
  }
  (void)rxRssi;
}

void sendFlashBubblePacket(bool force = false) {
  uint32_t now = millis();
  updateFlashBubbleWorld();
  uint32_t interval = YG_FLASH_BUBBLE_BG_MS;
  if (flashBubble.state >= 3 || flashBubble.priority >= 170) interval = YG_FLASH_BUBBLE_ALERT_MS;
  else if (flashBubble.state >= 2 || flashBubble.priority >= 90) interval = YG_FLASH_BUBBLE_ACTIVE_MS;
  if (!force && now - flashBubble.lastTxMs < interval) return;

  JanusKenshiPacket kp = {};
  kp.magic[0] = 'K'; kp.magic[1] = '2';
  kp.version = 1;
  kp.flags = 0;
  if (flashBubble.state >= 2) kp.flags |= 0x01;
  if (flashBubble.state >= 3) kp.flags |= 0x02;
  if (flashBubble.virtualNodes > 0) kp.flags |= 0x04;
  kp.flags |= 0x08; // IMU-guided motion base compatible.
  snprintf(kp.nodeId, sizeof(kp.nodeId), "%s", YG_NODE_ID);
  kp.seq = ++flashBubble.seq;
  kp.worker_id = workerId();
  kp.uptime_ms = now;
  kp.activeBubbleNodes = flashBubble.activeNodes;
  kp.virtualNodes = flashBubble.virtualNodes;
  kp.worldFlags = flashBubble.worldFlags;
  kp.sector = flashBubble.sector;
  kp.predictedSector = flashBubble.predictedSector;
  kp.jobState = flashBubble.jobState;
  kp.priority = flashBubble.priority;
  kp.rssi = currentRssi();
  kp.entropy = flashBubble.entropy;
  kp.activity = flashBubble.activity;
  kp.confidence = flashBubble.confidence;
  kp.values[0] = flashBubble.siliconHeat;
  kp.values[1] = (float)observerHashrate;
  kp.values[2] = (float)bestBits;
  kp.values[3] = blackStarPull(now);
  kp.values[4] = gate.stability;
  kp.values[5] = gate.charge;
  if (sendEspNow("K2", &kp, sizeof(kp)) == ESP_OK) {
    flashBubble.tx++;
    flashBubble.lastTxMs = now;
  }
}

void serviceFlashBubble() {
  sendFlashBubblePacket(false);
}

uint32_t yaksPnJobSig() {
  uint32_t sig = jobSeq ^ mix32(observerHashesTotal) ^ mix32(gate.opens + 0x9E3779B9UL);
  if (currentJob.rxMs) {
    for (uint8_t i = 0; i < sizeof(currentJob.jobId); ++i) {
      sig = mix32(sig ^ ((uint32_t)currentJob.jobId[i] << ((i & 3) * 8)));
    }
    sig ^= currentJob.startNonce ^ mix32(currentJob.rangeSize);
  }
  return mix32(sig);
}

uint32_t yaksPnPacketHash(JanusPnCortexPacket& pn) {
  uint32_t saved = pn.packet_hash;
  pn.packet_hash = 0;
  uint8_t h[32];
  doubleSha256((const uint8_t*)&pn, sizeof(pn), h);
  pn.packet_hash = saved;
  return ((uint32_t)h[0]) | ((uint32_t)h[1] << 8) | ((uint32_t)h[2] << 16) | ((uint32_t)h[3] << 24);
}

void sendPnCortex() {
  uint32_t now = millis();
  updateFlashBubbleWorld();
  float warpPower = mercuryWarpPower(now);

  JanusPnCortexPacket pn = {};
  pn.magic[0] = 'P'; pn.magic[1] = 'N';
  pn.version = 1;
  pn.role = 12; // Yaks Gate / StickS3 optical portal.
  pn.worker_id = workerId();
  snprintf(pn.nodeId, sizeof(pn.nodeId), "%s", YG_NODE_ID);
  snprintf(pn.kind, sizeof(pn.kind), "%s", "yaks_gate");
  pn.seq = ++pnCortexSeq;
  pn.uptime_ms = now;
  pn.job_sig = yaksPnJobSig();
  pn.prev_hash = pnCortexPrevHash;
  pn.hash_rate = observerHashrate;
  pn.total_hashes = observerHashesTotal;
  pn.target_bits = targetBitsNow;
  pn.best_bits = bestBits > 65535UL ? 65535U : (uint16_t)bestBits;
  pn.lane = blackStarActive(now) ? (uint8_t)(blackStarLink.bestLane & 3) : (gate.autoPilot ? 2 : 1);
  pn.sector = currentSector();
  pn.flags = 0;
  if (currentJob.active && safeElapsedMs(now, currentJob.rxMs) < YG_REMOTE_JOB_TTL_MS) pn.flags |= 0x01;
  if (irSigilEnabled) pn.flags |= 0x02;
  if (blackStarActive(now)) pn.flags |= 0x04;
  if (flashEscapeReady(now)) pn.flags |= 0x08;
  if (nasBrainOnline) pn.flags |= 0x10;
  if (mercuryWarp.armed || warpPower > 0.0f) pn.flags |= 0x20;
  pn.rssi = currentRssi();
  pn.thermal_x1000 = (uint16_t)constrain((int)((flashBubble.siliconHeat + blackStarLink.hawkingVapor * 0.18f) * 1000.0f), 0, 65535);
  pn.load_x1000 = (uint16_t)constrain((int)((flashBubble.siliconLoad + warpPower * 0.45f) * 1000.0f), 0, 65535);
  pn.jitter_us = (uint16_t)constrain((int)(imuLoss * 10000.0f + (1.0f - gate.stability) * 1400.0f), 0, 65535);
  pn.entropy_x1000 = (uint16_t)constrain((int)((flashBubble.entropy + blackStarLink.mercuryTime * 0.30f) * 1000.0f), 0, 65535);
  pn.tail_x1000 = (uint16_t)constrain((int)((yaksCorpus.score + gate.tailGlow * 0.30f + blackStarPull(now) * 0.25f + mercuryWarp.reverse * 0.25f) * 1000.0f), 0, 65535);
  pn.voltage_mv = 0; // StickS3 battery API differs across cores; keep ABI truthful instead of guessing.
  pn.ir_phase = (uint16_t)((irSigilEnabled ? 0x8000U : 0U) |
                           ((uint16_t)(lastIrBurstMs & 0x3FFFU) ^
                            (uint16_t)(blackStarLink.mercuryTorr * 3.0f) ^
                            (uint16_t)(mercuryWarp.readySeq * 97UL)));
  pn.reserved = (uint16_t)constrain((int)(warpPower * 1000.0f), 0, 1000);
  pn.packet_hash = yaksPnPacketHash(pn);

  if (sendEspNow("P/N", &pn, sizeof(pn)) == ESP_OK) {
    pnCortexPrevHash = pn.packet_hash;
  }
}

void onBuzzHeartbeat(const JanusColonyPacket& pkt) {
  if (memcmp(pkt.magic, "JANUS", 5) != 0) return;
  if (strncmp(pkt.role, "BuzzLighter", 11) != 0) return;
  buzzSeen = true;
  lastBuzzMs = millis();
  buzzHashrate = pkt.hashRate;
  buzzShares = pkt.shares;
  buzzBestBits = pkt.bestBits;
  buzzDiff = pkt.diff;
  if (pkt.targetBits) targetBitsNow = pkt.targetBits;
  buzzRejects = pkt.rejects;
  if (lastBuzzRejects && buzzRejects > lastBuzzRejects) {
    uint32_t delta = buzzRejects - lastBuzzRejects;
    uint8_t tail[4] = {(uint8_t)delta, (uint8_t)(buzzBestBits & 0xFF), (uint8_t)(pkt.seq & 0xFF), (uint8_t)currentSector()};
    sendTailPacket(3, 3, gate.autoPilot ? 2 : 1, (uint16_t)buzzBestBits, pkt.seq, 4, false, tail);
    sendEvent(11, "pool_reject_mirror", 82, 62, (int16_t)delta, (int16_t)buzzBestBits, (int16_t)buzzRejects, 0);
  }
  lastBuzzRejects = buzzRejects;
}

void copyCurrentToStale(uint8_t reason) {
  if (!currentJob.active) return;
  staleJob = currentJob;
  staleJob.active = true;
  staleJob.rxMs = millis();
  staleDropped++;
  if (reason == 2) lastCleanJobMs = millis();
}

void onJobPacket(const JobPacket& job) {
  uint32_t now = millis();
  if (lastJobRxMs) {
    float gap = (float)safeElapsedMs(now, lastJobRxMs);
    jobGapEmaMs = jobGapEmaMs * 0.82f + gap * 0.18f;
  }
  if (currentJob.active) copyCurrentToStale(2);

  memset(&currentJob, 0, sizeof(currentJob));
  currentJob.active = true;
  memcpy(currentJob.jobId, job.job_id, 8);
  memcpy(currentJob.header, job.header, 80);
  memcpy(currentJob.target, job.target, 32);
  currentJob.startNonce = job.start_nonce;
  currentJob.rangeSize = job.range_size;
  currentJob.nonce = job.start_nonce;
  currentJob.endNonce = job.start_nonce + job.range_size;
  currentJob.rxMs = now;
  currentJob.seq = ++jobSeq;
  targetBitsNow = countLeadingZeroBits(currentJob.target);
  lastJobRxMs = now;
  lastCleanJobMs = now;
  buzzSeen = true;
  lastBuzzMs = now;

  uint32_t seed = ((uint32_t)job.job_id[0] << 24) ^ ((uint32_t)job.job_id[3] << 16) ^ job.start_nonce ^ mix32(job.range_size);
  gate.targetX = ((int32_t)(mix32(seed) & 2047) - 1024) / 1600.0f;
  gate.targetY = ((int32_t)(mix32(seed ^ 0xA5A55A5AUL) & 2047) - 1024) / 1900.0f;
  gate.pulse = min(1.0f, gate.pulse + 0.32f);
  snprintf(statusLine, sizeof(statusLine), "JOB z%u GATE", (unsigned)targetBitsNow);
}

bool isBlackStarEvent(const JanusEventPacket& je) {
  return strstr(je.kind, "blackstar") || strstr(je.kind, "garg") ||
         strstr(je.kind, "bh_gate") || strstr(je.kind, "BH");
}

void onBlackStarTrace(const JanusEventPacket& je) {
  uint32_t now = millis();
  uint16_t packed = (uint16_t)je.valueA_x10;
  uint8_t lane = (uint8_t)((packed >> 8) & 3);
  uint8_t bestLane = (uint8_t)(packed & 3);
  uint16_t traceBest = (uint16_t)max(0, (int)je.valueB_x10);
  uint32_t traceHashrate = (uint32_t)max(0, (int)je.valueC_x10) * 32UL;
  uint16_t heatLoad = (uint16_t)max(0, (int)je.valueD_x10);
  float heat = clampf((float)((heatLoad >> 8) & 0x7F) / 127.0f, 0.0f, 1.0f);
  float load = clampf((float)(heatLoad & 0x7F) / 127.0f, 0.0f, 1.0f);

  blackStarLink.seen = true;
  blackStarLink.lane = lane;
  blackStarLink.bestLane = bestLane;
  blackStarLink.lastMs = now;
  snprintf(blackStarLink.source, sizeof(blackStarLink.source), "%s", je.nodeId[0] ? je.nodeId : "BH");

  flashBubble.siliconHeat = flashBubble.siliconHeat * 0.72f + heat * 0.28f;
  flashBubble.siliconLoad = flashBubble.siliconLoad * 0.72f + load * 0.28f;
  flashBubble.eventPower = min(3.0f, flashBubble.eventPower + heat * 0.08f + load * 0.10f + (float)traceBest * 0.002f);
  flashBubble.priority = max(flashBubble.priority, (uint8_t)constrain((int)(load * 70.0f + heat * 54.0f + traceBest), 0, 255));
  gate.tailGlow = min(1.0f, gate.tailGlow + load * 0.035f + heat * 0.025f);
  if (traceBest > bestBits) gate.pulse = min(1.0f, gate.pulse + 0.06f);
  snprintf(statusLine, sizeof(statusLine), "BS TRACE H%lu", (unsigned long)traceHashrate);
  markYaksStateDirty();
}

void onBlackStarMercury(const JanusEventPacket& je) {
  uint32_t now = millis();
  float hg = clampf((float)je.valueA_x10, 540.0f, 940.0f);
  float tv = clampf((float)je.valueB_x10 / 1000.0f, 0.0f, 1.5f);
  float mt = clampf((float)je.valueC_x10 / 1000.0f, 0.0f, 1.5f);
  float hv = clampf((float)je.valueD_x10 / 1000.0f, 0.0f, 1.5f);
  float hgFit = clampf(1.0f - fabsf(hg - 760.0f) / 260.0f, 0.0f, 1.0f);
  float balance = clampf(hgFit * 0.42f + mt * 0.24f + (1.0f - min(1.0f, tv)) * 0.18f +
                         (float)je.confidence / 255.0f * 0.16f, 0.0f, 1.0f);

  blackStarLink.seen = true;
  blackStarLink.confidence = max(blackStarLink.confidence, (float)je.confidence / 255.0f);
  blackStarLink.influence = max(blackStarLink.influence, clampf(balance + tv * 0.14f, 0.0f, 1.0f));
  blackStarLink.mercuryTorr = hg;
  blackStarLink.torricelliVoid = tv;
  blackStarLink.mercuryTime = mt;
  blackStarLink.hawkingVapor = hv;
  blackStarLink.horizonBalance = balance;
  blackStarLink.seq = je.seq;
  blackStarLink.lastMs = now;
  blackStarLink.mercuryMs = now;
  snprintf(blackStarLink.source, sizeof(blackStarLink.source), "%s", je.nodeId[0] ? je.nodeId : "BH");

  gate.tailGlow = min(1.0f, gate.tailGlow + 0.16f + tv * 0.12f);
  gate.charge = min(1.0f, gate.charge + 0.010f + mt * 0.016f + balance * 0.012f);
  gate.stability = clampf(gate.stability + balance * 0.018f - hv * 0.006f, 0.0f, 1.0f);
  gate.pulse = min(1.0f, gate.pulse + 0.24f);
  if ((je.seq & 1U) == 0U) gate.skyLock = min(1.0f, gate.skyLock + balance * 0.030f);
  snprintf(statusLine, sizeof(statusLine), "Hg %.0f V%.2f", hg, tv);

  if (mercuryWarpReady(now)) armMercuryWarp("blackstar_mercury");
  markYaksStateDirty();
}

void onBlackStarGate(const JanusEventPacket& je) {
  uint32_t now = millis();
  uint16_t packed = (uint16_t)je.valueA_x10;
  uint8_t lane = (uint8_t)((packed >> 8) & 3);
  uint8_t bestLane = (uint8_t)(packed & 3);
  float conf = clampf((float)je.valueB_x10 / 1000.0f, 0.0f, 1.0f);
  float loss = clampf((float)je.valueC_x10 / 1000.0f, 0.0f, 2.0f);
  float pull = clampf(conf * (1.0f - min(0.85f, loss * 0.55f)) + (float)je.urgency / 500.0f, 0.0f, 1.0f);

  blackStarLink.seen = true;
  blackStarLink.lane = lane;
  blackStarLink.bestLane = bestLane;
  blackStarLink.confidence = conf;
  blackStarLink.loss = loss;
  blackStarLink.influence = pull;
  blackStarLink.corpus = (uint32_t)max(0, (int)je.valueD_x10);
  blackStarLink.seq = je.seq;
  blackStarLink.lastMs = now;
  snprintf(blackStarLink.source, sizeof(blackStarLink.source), "%s", je.nodeId[0] ? je.nodeId : "BH");

  uint8_t chosen = bestLane;
  float a = ((float)((blackStarLink.corpus ^ je.seq) & 1023) / 1023.0f) * 6.28318f;
  if (chosen == 1) {
    gate.targetX = cosf(a) * (0.42f + pull * 0.22f);
    gate.targetY = sinf(a) * (0.28f + pull * 0.14f);
    gate.spin += 0.16f + pull * 0.20f;
  } else if (chosen == 2) {
    gate.targetX = sinf(a * 0.7f) * (0.24f + pull * 0.20f);
    gate.targetY = sinf(a * 1.3f) * 0.12f;
    gate.aperture = min(1.0f, gate.aperture + 0.035f + pull * 0.050f);
  } else if (chosen == 3) {
    gate.targetX = sinf(a) * (0.36f + pull * 0.18f);
    gate.targetY = -0.42f - pull * 0.20f;
    gate.skyLock = min(1.0f, gate.skyLock + 0.055f + pull * 0.070f);
  } else {
    gate.targetX *= 0.45f;
    gate.targetY *= 0.45f;
  }

  gate.tailGlow = min(1.0f, gate.tailGlow + 0.12f + pull * 0.22f);
  gate.charge = min(1.0f, gate.charge + 0.006f + pull * 0.018f);
  gate.stability = clampf(gate.stability + conf * 0.020f - loss * 0.010f, 0.0f, 1.0f);
  gate.pulse = min(1.0f, gate.pulse + 0.22f + pull * 0.18f);
  snprintf(statusLine, sizeof(statusLine), "BH->YAKS %s", blackStarLaneName(chosen));
  markYaksStateDirty();
}

void onJanusEvent(const JanusEventPacket& je) {
  if (je.magic[0] != 'J' || je.magic[1] != 'E') return;
  if (strstr(je.kind, "blackstar_mercury")) {
    onBlackStarMercury(je);
    return;
  }
  if (strstr(je.kind, "blackstar_trace")) {
    onBlackStarTrace(je);
    return;
  }
  if (isBlackStarEvent(je)) {
    onBlackStarGate(je);
    return;
  }
  if (je.eventType == 11 || strstr(je.kind, "reject") || strstr(je.kind, "stale")) {
    uint8_t tail[4] = {(uint8_t)je.valueA_x10, (uint8_t)je.valueB_x10, (uint8_t)je.seq, (uint8_t)je.urgency};
    sendTailPacket(3, 3, gate.autoPilot ? 2 : 1, (uint16_t)max(0, (int)je.valueB_x10), je.seq, 4, false, tail);
  }
}

void onPnCortexPacket(const JanusPnCortexPacket& pn) {
  if (pn.magic[0] != 'P' || pn.magic[1] != 'N' || pn.version == 0) return;
  uint32_t now = millis();
  bool isAnchor = pn.role == 14 || strstr(pn.kind, "anchor") || strstr(pn.nodeId, "Anchor");
  bool isGladius = pn.role == 13 || strstr(pn.kind, "gladius") || strstr(pn.nodeId, "Gladius");
  bool isBlackStar = strstr(pn.kind, "blackstar") || strstr(pn.nodeId, "BH") || strstr(pn.nodeId, "Black");
  float oxy = clampf((float)pn.reserved / 1000.0f, 0.0f, 1.0f);
  float vacuum = clampf((float)pn.entropy_x1000 / 6000.0f + (float)pn.tail_x1000 / 12000.0f, 0.0f, 1.0f);

  if (isAnchor) {
    brothers.anchorSeen = true;
    brothers.anchorMs = now;
    brothers.anchorBest = pn.best_bits;
    brothers.anchorOxy = brothers.anchorOxy * 0.74f + oxy * 0.26f;
    brothers.anchorVacuum = brothers.anchorVacuum * 0.78f + vacuum * 0.22f;
    if (pn.flags & 0x20) brothers.anchorOxy = max(brothers.anchorOxy, 0.64f);
  } else if (isGladius) {
    brothers.gladiusSeen = true;
    brothers.gladiusMs = now;
    brothers.gladiusBest = pn.best_bits;
    brothers.gladiusOxy = brothers.gladiusOxy * 0.74f + oxy * 0.26f;
    brothers.gladiusVacuum = brothers.gladiusVacuum * 0.78f + vacuum * 0.22f;
    if (pn.flags & 0x20) brothers.gladiusOxy = max(brothers.gladiusOxy, 0.64f);
  } else if (isBlackStar) {
    blackStarLink.seen = true;
    blackStarLink.mercuryMs = now;
    blackStarLink.lastMs = now;
    blackStarLink.torricelliVoid = clampf((float)pn.reserved / 1000.0f, 0.0f, 1.0f);
    blackStarLink.mercuryTime = clampf((float)pn.load_x1000 / 2600.0f, 0.0f, 1.0f);
    blackStarLink.hawkingVapor = clampf((float)pn.thermal_x1000 / 3200.0f, 0.0f, 1.0f);
    blackStarLink.horizonBalance = max(blackStarLink.horizonBalance, clampf((float)pn.entropy_x1000 / 2600.0f, 0.0f, 1.0f));
  }

  if (isAnchor || isGladius || isBlackStar) {
    flashBubble.rx++;
    flashBubble.lastRxMs = now;
    gate.tailGlow = min(1.0f, gate.tailGlow + 0.035f);
    if (brothersReady(now)) {
      gate.pulse = min(1.0f, gate.pulse + 0.08f);
      snprintf(statusLine, sizeof(statusLine), "BROTHERS READY");
    }
    if (mercuryWarpReady(now)) armMercuryWarp(isBlackStar ? "blackstar_pn" : "brother_pn");
  }
}

#if ESP_IDF_VERSION_MAJOR >= 5
void onEspNowRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  (void)info;
#else
void onEspNowRecv(const uint8_t* mac, const uint8_t* data, int len) {
  (void)mac;
#endif
  if (!data || len <= 0) return;
  if (len == (int)sizeof(JanusPnCortexPacket) && data[0] == 'P' && data[1] == 'N') {
    JanusPnCortexPacket pn = {};
    memcpy(&pn, data, sizeof(pn));
    onPnCortexPacket(pn);
    return;
  }
  if (len == (int)sizeof(JanusColonyPacket)) {
    JanusColonyPacket pkt = {};
    memcpy(&pkt, data, sizeof(pkt));
    onBuzzHeartbeat(pkt);
    return;
  }
  if (len == (int)sizeof(JobPacket) && data[0] == 'J' && data[1] == 'B') {
    JobPacket job = {};
    memcpy(&job, data, sizeof(job));
    onJobPacket(job);
    return;
  }
  if (len == (int)sizeof(JanusEventPacket) && data[0] == 'J' && data[1] == 'E') {
    JanusEventPacket je = {};
    memcpy(&je, data, sizeof(je));
    onJanusEvent(je);
    return;
  }
  if (len == (int)sizeof(JanusKenshiPacket) && data[0] == 'K' && data[1] == '2') {
    JanusKenshiPacket kp = {};
    memcpy(&kp, data, sizeof(kp));
    onJanusKenshiPacket(kp, currentRssi());
    return;
  }
}

void initEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(MESH_WIFI_SSID, MESH_WIFI_PASSWORD);
  uint32_t until = millis() + 2300UL;
  while (WiFi.status() != WL_CONNECTED && millis() < until) {
    delay(20);
  }
  esp_now_deinit();
  if (esp_now_init() != ESP_OK) {
    colonyReady = false;
    snprintf(statusLine, sizeof(statusLine), "ESP-NOW FAIL");
    return;
  }
  esp_now_register_recv_cb(onEspNowRecv);
  colonyReady = true;
  ensurePeer();
  Serial.printf("[YAKS] ESP-NOW ready ch=%u wifi=%d worker=%u\n",
                (unsigned)WiFi.channel(), WiFi.status() == WL_CONNECTED ? 1 : 0, (unsigned)workerId());
}

bool topButtonDown() {
  return M5.BtnB.isPressed() || M5.BtnPWR.isPressed();
}

void refreshImuGripMode() {
  float ax = fabsf(imuNeutralAx);
  float ay = fabsf(imuNeutralAy);
  float az = fabsf(imuNeutralAz);
  if (az >= ax && az >= ay) imuGripMode = 0;
  else if (ay >= ax) imuGripMode = 1;
  else imuGripMode = 2;
}

const char* imuGripName() {
  if (imuGripMode == 1) return "VERT";
  if (imuGripMode == 2) return "SIDE";
  return "FLAT";
}

void adoptImuPose(float ax, float ay, float az, const char* label, uint32_t settleMs) {
  imuNeutralAx = ax;
  imuNeutralAy = ay;
  imuNeutralAz = az;
  refreshImuGripMode();
  imuRoll = 0.0f;
  imuPitch = 0.0f;
  imuMoveX = 0.0f;
  imuMoveY = 0.0f;
  imuManualIntent = 0.0f;
  imuPoseDist = 0.0f;
  gate.velX = 0.0f;
  gate.velY = 0.0f;
  imuPoseSettleUntilMs = millis() + settleMs;
  imuLastAutoPoseMs = millis();
  if (label) snprintf(statusLine, sizeof(statusLine), "%s %s", label, imuGripName());
}

void mapImuToGate(float nx, float ny, float nz, float& rawSide, float& rawFwd) {
  if (imuGripMode == 1) {
    // Portrait/vertical: left-right belongs to the X axis; up-down comes from tilt in/out of the screen.
    rawSide = nx;
    rawFwd = -nz;
  } else if (imuGripMode == 2) {
    // Side vertical: depth becomes left/right; vertical hand motion stays vertical.
    float sx = imuNeutralAx >= 0.0f ? 1.0f : -1.0f;
    rawSide = -nz * sx;
    rawFwd = -ny;
  } else {
    // Flat/shallow grip: rotate the old basis so physical right is right, not up.
    rawSide = nx;
    rawFwd = -ny;
  }
}

void applyBrightness() {
  uint8_t value = brightnessLevels[brightnessIdx];
  M5.Display.setBrightness(value);
  snprintf(statusLine, sizeof(statusLine), "BRIGHT %u", (unsigned)value);
  markYaksStateDirty();
}

void stepBrightness() {
  uint32_t now = millis();
  if (now < brightnessPauseUntilMs) return;
  if (brightnessDirDown) {
    if (brightnessIdx > 0) {
      brightnessIdx--;
    } else {
      brightnessDirDown = false;
      brightnessPauseUntilMs = now + 520UL;
      return;
    }
  } else {
    uint8_t maxIdx = (uint8_t)((sizeof(brightnessLevels) / sizeof(brightnessLevels[0])) - 1);
    if (brightnessIdx < maxIdx) {
      brightnessIdx++;
    } else {
      brightnessDirDown = true;
      brightnessPauseUntilMs = now + 520UL;
      return;
    }
  }
  applyBrightness();
}

void capturePlayPose(const char* label, bool gyroToo = false) {
  float sax = 0, say = 0, saz = 0;
  float sgx = 0, sgy = 0, sgz = 0;
  const int samples = 48;
  for (int i = 0; i < samples; ++i) {
    float ax = 0, ay = 0, az = 1, gx = 0, gy = 0, gz = 0;
    M5.Imu.getAccelData(&ax, &ay, &az);
    M5.Imu.getGyroData(&gx, &gy, &gz);
    sax += ax; say += ay; saz += az;
    sgx += gx; sgy += gy; sgz += gz;
    delay(3);
  }
  imuNeutralAx = sax / samples;
  imuNeutralAy = say / samples;
  imuNeutralAz = saz / samples;
  refreshImuGripMode();
  if (gyroToo) {
    imuBiasGx = sgx / samples;
    imuBiasGy = sgy / samples;
    imuBiasGz = sgz / samples;
  }
  imuRoll = 0;
  imuPitch = 0;
  imuManualIntent = 0;
  imuMoveX = 0;
  imuMoveY = 0;
  gate.velX = 0;
  gate.velY = 0;
  imuPoseSettleUntilMs = millis() + 2600UL;
  imuLastAutoPoseMs = millis();
  if (label) snprintf(statusLine, sizeof(statusLine), "%s %s", label, imuGripName());
}

void calibrateImu(bool force) {
  (void)force;
  capturePlayPose("POSE SET", true);
  imuPoseSettleUntilMs = millis() + 3600UL;
  imuReady = true;
}

void updateImu(float dt) {
  float ax = 0, ay = 0, az = 1, gx = 0, gy = 0, gz = 0;
  M5.Imu.getAccelData(&ax, &ay, &az);
  M5.Imu.getGyroData(&gx, &gy, &gz);
  gx -= imuBiasGx; gy -= imuBiasGy; gz -= imuBiasGz;

  uint32_t now = millis();
  float nx = ax - imuNeutralAx;
  float ny = ay - imuNeutralAy;
  float nz = az - imuNeutralAz;
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float gyro = sqrtf(gx * gx + gy * gy + gz * gz) * 0.010f;
  imuPoseDist = sqrtf(nx * nx + ny * ny + nz * nz);

  bool buttonsIdle = !topButtonDown() && !M5.BtnA.isPressed();
  bool stableHand = buttonsIdle && gyro < 0.16f && fabsf(mag - 1.0f) < 0.42f;
  if (stableHand) {
    if (!imuStillSinceMs) imuStillSinceMs = now;
  } else {
    imuStillSinceMs = 0;
  }

  if (imuPoseAdaptive && stableHand && imuPoseDist > 0.78f &&
      imuStillSinceMs && now - imuStillSinceMs > YG_IMU_HAND_RECENTER_MS &&
      now - imuLastAutoPoseMs > YG_IMU_HAND_REARM_MS) {
    adoptImuPose(ax, ay, az, "HAND POSE", 900UL);
    nx = ny = nz = 0.0f;
  }

  float rawSide = 0.0f;
  float rawFwd = 0.0f;
  mapImuToGate(nx, ny, nz, rawSide, rawFwd);

  if (imuPoseAdaptive && buttonsIdle && gyro < 0.22f) {
    bool settling = now < imuPoseSettleUntilMs;
    bool quietHand = gyro < 0.070f;
    float driftLimit = (gate.autoPilot || settling) ? 1.25f : 0.72f;
    bool smallDrift = imuPoseDist < driftLimit;
    bool centeredHand = quietHand && imuManualIntent < 0.22f && now - lastInputMs > 700UL;
    bool safeToRecenter = gate.autoPilot || settling || centeredHand;
    if (settling || (safeToRecenter && smallDrift)) {
      float beta = settling ? 0.22f : (centeredHand ? 0.030f : 0.010f);
      imuNeutralAx = imuNeutralAx * (1.0f - beta) + ax * beta;
      imuNeutralAy = imuNeutralAy * (1.0f - beta) + ay * beta;
      imuNeutralAz = imuNeutralAz * (1.0f - beta) + az * beta;
      refreshImuGripMode();
      nx = ax - imuNeutralAx;
      ny = ay - imuNeutralAy;
      nz = az - imuNeutralAz;
      imuPoseDist = sqrtf(nx * nx + ny * ny + nz * nz);
      mapImuToGate(nx, ny, nz, rawSide, rawFwd);
    }
  }

  float targetRoll = clampf(rawSide * 0.94f, -1.0f, 1.0f);
  float targetPitch = clampf(rawFwd * 0.88f, -1.0f, 1.0f);
  if (now < imuPoseSettleUntilMs) {
    targetRoll = 0.0f;
    targetPitch = 0.0f;
  }
  float alpha = 0.055f + min(0.045f, dt * 0.95f);
  imuRoll = imuRoll * (1.0f - alpha) + targetRoll * alpha;
  imuPitch = imuPitch * (1.0f - alpha) + targetPitch * alpha;

  imuShock = mag + gyro;
  imuLoss = fabsf(imuShock - imuPredShock);
  imuPredShock = imuPredShock * 0.970f + imuShock * 0.030f;

  imuMoveX = gateControl(imuRoll);
  imuMoveY = gateControl(imuPitch);
  imuManualIntent = fabsf(imuMoveX) + fabsf(imuMoveY);
  if (now > imuPoseSettleUntilMs && (imuManualIntent > 0.17f || gyro > 0.40f)) {
    lastInputMs = now;
  }
}

void resetParticle(GateParticle& p, bool far = true) {
  uint32_t r = esp_random();
  float a = (float)(r & 0x3FFF) * 0.0003835f;
  float rad = 0.10f + (float)((r >> 14) & 0x3FF) / 1024.0f * 1.30f;
  p.x = cosf(a) * rad;
  p.y = sinf(a) * rad * 0.72f;
  p.z = far ? (0.65f + (float)((r >> 24) & 0xFF) / 255.0f * 1.55f) : 0.18f;
  p.speed = 0.20f + (float)((r >> 8) & 0x7F) / 127.0f * 0.55f;
  p.hue = (uint8_t)((r >> 16) & 7);
}

void resetRift(Rift& r, bool far = true) {
  uint32_t x = esp_random();
  float a = (float)(x & 0x3FFF) * 0.0003835f;
  float rad = 0.32f + (float)((x >> 14) & 0x3FF) / 1024.0f * 1.10f;
  r.x = cosf(a) * rad;
  r.y = sinf(a) * rad * 0.62f;
  r.z = far ? (0.85f + (float)((x >> 24) & 0xFF) / 255.0f * 1.70f) : 0.24f;
  r.spin = (float)((x >> 8) & 0xFF) * 0.024f;
  r.kind = (uint8_t)((x >> 20) & 3);
}

void initGateWorld() {
  for (int i = 0; i < YG_PARTICLE_COUNT; ++i) resetParticle(particles[i], true);
  for (int i = 0; i < YG_RIFT_COUNT; ++i) resetRift(rifts[i], true);
  gate.shipX = 0;
  gate.shipY = 0;
  gate.stability = 0.62f;
  gate.charge = 0.16f;
  gate.aperture = 0.20f;
}

void fireGatePulse(bool manual) {
  gate.pulse = min(1.0f, gate.pulse + (manual ? 0.45f : 0.28f));
  gate.heat = min(1.0f, gate.heat + (manual ? 0.16f : 0.08f));
  gate.charge = min(1.0f, gate.charge + (manual ? 0.035f : 0.020f));
  gate.stability = min(1.0f, gate.stability + (manual ? 0.035f : 0.018f));
  snprintf(statusLine, sizeof(statusLine), manual ? "SHA PULSE" : "GATE PULSE");
  sendEvent(12, manual ? "yaks_manual_pulse" : "yaks_gate_pulse", 88, manual ? 42 : 24,
            (int16_t)(gate.charge * 1000.0f), (int16_t)(gate.stability * 1000.0f), (int16_t)bestBits, (int16_t)tailEvents);
  M5.Speaker.tone(manual ? 980 : 620, manual ? 30 : 18);
}

void openGate() {
  gate.opens++;
  gate.charge = 0.18f;
  gate.stability = max(0.50f, gate.stability - 0.12f);
  gate.aperture = 1.0f;
  gate.pulse = 1.0f;
  gate.shake = 0.26f;
  snprintf(statusLine, sizeof(statusLine), "skaY OPEN %lu", (unsigned long)gate.opens);
  sendEvent(13, "yaks_gate_open", 96, 72,
            (int16_t)gate.opens, (int16_t)bestBits, (int16_t)observerHashrate, (int16_t)(gate.stability * 1000.0f));
  markYaksStateDirty();
}

int postJsonShort(const String& url, const char* payload, uint16_t timeoutMs) {
  HTTPClient http;
  http.setTimeout(timeoutMs);
  http.setConnectTimeout(timeoutMs);
  if (!http.begin(url)) return -1000;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST((uint8_t*)payload, strlen(payload));
  http.end();
  return code;
}

void serviceNasBrain() {
#if YG_NAS_ENABLE
  uint32_t now = millis();
  if (!gate.autoPilot) return;
  if (now - lastInputMs < YG_INPUT_IDLE_MS + 900UL) return;
  if (now - lastNasMs < YG_NAS_AUTO_MS) return;
  lastNasMs = now;
  if (WiFi.status() != WL_CONNECTED) {
    nasOnline = false;
    nasFails++;
    return;
  }

  char payload[1664];
  snprintf(payload, sizeof(payload),
           "{\"node_id\":\"%s\",\"role\":\"yaks_gate\",\"kind\":\"auto_handoff\","
           "\"auto\":1,\"warp_request\":1,\"observer_only\":1,"
           "\"gate\":{\"charge\":%.3f,\"stability\":%.3f,\"aperture\":%.3f,\"opens\":%lu,\"sigils\":%lu},"
           "\"miner\":{\"hashrate\":%lu,\"best_bits\":%lu,\"target_bits\":%u,\"tails\":%lu,\"stale\":%lu},"
           "\"corpus\":{\"events\":%lu,\"best_z\":%u,\"target_pass\":%lu,\"lane\":\"%s\",\"score\":%.3f,\"stale\":%lu,\"poolR\":%lu},"
           "\"blackstar\":{\"active\":%u,\"source\":\"%s\",\"lane\":\"%s\",\"best_lane\":\"%s\",\"confidence\":%.3f,\"loss\":%.3f,\"pull\":%.3f,\"corpus\":%lu},"
           "\"flash_bubble\":{\"state\":%u,\"job\":%u,\"sector\":%u,\"pred\":%u,\"priority\":%u,\"flags\":%lu,"
           "\"rx\":%lu,\"tx\":%lu,\"ir_beacon\":%lu,\"source\":\"%s\",\"entropy\":%.3f,\"activity\":%.3f,\"confidence\":%.3f,"
           "\"heat\":%.3f,\"load\":%.3f},"
           "\"sovereign\":{\"red\":%.3f,\"blue\":%.3f,\"gold\":%.3f},"
           "\"kombucha\":{\"acid\":%.3f,\"yeast\":%.3f,\"bacteria\":%.3f,\"paradox\":%.3f},"
           "\"swarm\":{\"buzz\":%u,\"job\":%u,\"job_age_ms\":%lu},"
           "\"imu\":{\"roll\":%.3f,\"pitch\":%.3f,\"intent\":%.3f}}",
           YG_NODE_ID,
           gate.charge, gate.stability, gate.aperture,
           (unsigned long)gate.opens, (unsigned long)gate.sigils,
           (unsigned long)observerHashrate, (unsigned long)bestBits, (unsigned)targetBitsNow,
           (unsigned long)tailEvents, (unsigned long)staleTailEvents,
           (unsigned long)yaksCorpus.events, (unsigned)yaksCorpus.bestZ,
           (unsigned long)yaksCorpus.targetPass, yaksCorpus.lastLane, yaksCorpus.score,
           (unsigned long)yaksCorpus.stale, (unsigned long)yaksCorpus.poolReject,
           blackStarActive(now) ? 1 : 0, blackStarLink.source,
           blackStarLaneName(blackStarLink.lane), blackStarLaneName(blackStarLink.bestLane),
           blackStarLink.confidence, blackStarLink.loss, blackStarPull(now), (unsigned long)blackStarLink.corpus,
           (unsigned)flashBubble.state, (unsigned)flashBubble.jobState,
           (unsigned)flashBubble.sector, (unsigned)flashBubble.predictedSector,
           (unsigned)flashBubble.priority, (unsigned long)flashBubble.worldFlags,
           (unsigned long)flashBubble.rx, (unsigned long)flashBubble.tx,
           (unsigned long)flashBubble.irBeaconTx, flashBubble.source,
           flashBubble.entropy, flashBubble.activity, flashBubble.confidence,
           flashBubble.siliconHeat, flashBubble.siliconLoad,
           yaksCorpus.red, yaksCorpus.blue, yaksCorpus.gold,
           yaksCorpus.acid, yaksCorpus.yeast, yaksCorpus.bacteria, yaksCorpus.paradox,
           buzzSeen && safeElapsedMs(now, lastBuzzMs) < 9000UL ? 1 : 0,
           currentJob.active ? 1 : 0,
           (unsigned long)safeElapsedMs(now, currentJob.rxMs),
           imuRoll, imuPitch, imuManualIntent);

  String url = String(YG_NAS_BASE_URL) + "/api/swarm/telemetry";
  int code = postJsonShort(url, payload, YG_NAS_TIMEOUT_MS);
  nasLastCode = code;
  if (code >= 200 && code < 300) {
    nasOnline = true;
    nasReports++;
    nasWarpUntilMs = now + YG_NAS_WARP_MS;
    gate.pulse = min(1.0f, gate.pulse + 0.20f);
    gate.stability = min(1.0f, gate.stability + 0.022f);
    snprintf(statusLine, sizeof(statusLine), "NAS WARP");
    sendEvent(16, "yaks_nas_handoff", 82, 38,
              (int16_t)nasReports, (int16_t)bestBits,
              (int16_t)(gate.charge * 1000.0f), (int16_t)(gate.stability * 1000.0f));
  } else {
    nasOnline = false;
    nasFails++;
  }

#if YG_NAS_BRAIN_ENABLE
  if (now >= nasBrainBackoffUntilMs) {
    char brainPayload[1180];
    uint32_t jobAgeMs = safeElapsedMs(now, currentJob.rxMs);
    snprintf(brainPayload, sizeof(brainPayload),
             "{\"type\":\"swarm_sense\",\"version\":1,\"source\":\"yaks_gate\","
             "\"node_id\":\"%s\",\"kind\":\"YAKS_GATE\",\"role\":\"yaks_gate\","
             "\"hash_rate\":%lu,\"best_bits\":%lu,\"target_bits\":%u,\"rssi\":%d,"
             "\"job_age_s\":%.2f,\"shares\":%lu,\"rejects\":%lu,"
             "\"observer_only\":1,\"wire_change_required\":0,"
             "\"corpus_events\":%lu,\"corpus_best_z\":%u,\"corpus_score\":%.3f,"
             "\"blackstar_active\":%u,\"blackstar_lane\":\"%s\",\"blackstar_best\":\"%s\","
             "\"blackstar_confidence\":%.3f,\"blackstar_loss\":%.3f,\"blackstar_pull\":%.3f,"
             "\"mercury_torr\":%.1f,\"torricelli_void\":%.3f,\"mercury_time\":%.3f,"
             "\"hawking_vapor\":%.3f,\"horizon_balance\":%.3f,"
             "\"brother_anchor\":%.3f,\"brother_gladius\":%.3f,\"mercury_warp\":%.3f,"
             "\"flash_state\":%u,\"flash_priority\":%u,\"flash_sector\":%u,\"flash_pred\":%u,"
             "\"flash_flags\":%lu,\"flash_heat\":%.3f,\"flash_load\":%.3f,\"flash_ir_beacon\":%lu,"
             "\"gate_charge\":%.3f,\"gate_stability\":%.3f,\"ir\":%u,\"mode\":\"%s\"}",
             YG_NODE_ID,
             (unsigned long)observerHashrate,
             (unsigned long)bestBits,
             (unsigned)targetBitsNow,
             (int)currentRssi(),
             (float)jobAgeMs / 1000.0f,
             (unsigned long)gate.opens,
             (unsigned long)(poolRejectEvents + staleTailEvents),
             (unsigned long)yaksCorpus.events,
             (unsigned)yaksCorpus.bestZ,
             yaksCorpus.score,
             blackStarActive(now) ? 1 : 0,
             blackStarLaneName(blackStarLink.lane),
             blackStarLaneName(blackStarLink.bestLane),
             blackStarLink.confidence,
             blackStarLink.loss,
             blackStarPull(now),
             blackStarLink.mercuryTorr,
             blackStarLink.torricelliVoid,
             blackStarLink.mercuryTime,
             blackStarLink.hawkingVapor,
             blackStarLink.horizonBalance,
             brothers.anchorOxy,
             brothers.gladiusOxy,
             mercuryWarpPower(now),
             (unsigned)flashBubble.state,
             (unsigned)flashBubble.priority,
             (unsigned)flashBubble.sector,
             (unsigned)flashBubble.predictedSector,
             (unsigned long)flashBubble.worldFlags,
             flashBubble.siliconHeat,
             flashBubble.siliconLoad,
             (unsigned long)flashBubble.irBeaconTx,
             gate.charge,
             gate.stability,
             irSigilEnabled ? 1 : 0,
             laneName());
    String brainUrl = String(YG_NAS_BRAIN_URL) + "/api/swarm/sense";
    int brainCode = postJsonShort(brainUrl, brainPayload, YG_NAS_BRAIN_TIMEOUT_MS);
    nasBrainLastCode = brainCode;
    if (brainCode >= 200 && brainCode < 300) {
      nasBrainOnline = true;
      nasBrainReports++;
      nasBrainLastOkMs = now;
      nasBrainBackoffUntilMs = 0;
    } else {
      nasBrainFails++;
      nasBrainOnline = false;
      nasBrainBackoffUntilMs = now + (nasBrainLastOkMs != 0 && safeElapsedMs(now, nasBrainLastOkMs) < 60000UL ? 18000UL : 36000UL);
    }
  } else {
    nasBrainOnline = false;
  }
#endif
#endif
}

void updateGate(float dt) {
  uint32_t now = millis();
  gate.autoPilot = (now - lastInputMs > YG_INPUT_IDLE_MS);
  bool nasWarp = gate.autoPilot && now < nasWarpUntilMs;
  if (mercuryWarp.launch && now >= mercuryWarp.launchUntilMs) {
    mercuryWarp.launch = false;
    mercuryWarp.charge *= 0.72f;
  }
  float mercuryBoost = mercuryWarpPower(now);
  float warpBoost = clampf((nasWarp ? 1.0f : 0.0f) + mercuryBoost, 0.0f, 1.65f);
  float corpusBoost = clampf(yaksCorpus.score, 0.0f, 1.0f);
  float bsPull = blackStarPull(now);
  gate.phase += dt * (0.55f + gate.stability * 0.85f + gate.tailGlow * 0.45f + warpBoost * 1.15f + corpusBoost * 0.16f + bsPull * 0.55f);
  gate.spin += dt * (0.78f + gate.charge * 0.80f + warpBoost * 1.35f + yaksCorpus.gold * 0.12f + bsPull * 0.40f);

  if (mercuryBoost > 0.0f) {
    float reverseAngle = gate.phase * 0.41f + mercuryWarp.vector * 6.28318f;
    float reverseRadius = 0.18f + mercuryWarp.reverse * 0.34f;
    gate.targetX = gate.targetX * 0.84f - cosf(reverseAngle) * reverseRadius * 0.16f;
    gate.targetY = gate.targetY * 0.84f - sinf(reverseAngle) * reverseRadius * 0.14f;
    gate.skyLock = min(1.0f, gate.skyLock + dt * (0.050f + mercuryBoost * 0.090f));
    gate.heat = min(1.0f, gate.heat + dt * mercuryBoost * 0.035f);
  }

  if (gate.autoPilot) {
    float ax = (gate.targetX * 0.52f - gate.shipX) * (0.55f + gate.stability * 0.35f + warpBoost * 1.25f);
    float ay = (gate.targetY * 0.52f - gate.shipY) * (0.55f + gate.stability * 0.35f + warpBoost * 1.25f);
    gate.velX += ax * dt;
    gate.velY += ay * dt;
  } else {
    gate.velX += imuMoveX * dt * 2.80f;
    gate.velY += imuMoveY * dt * 2.35f;
  }

  gate.velX *= powf(0.060f, dt);
  gate.velY *= powf(0.060f, dt);
  gate.shipX = clampf(gate.shipX + gate.velX * dt, -1.0f, 1.0f);
  gate.shipY = clampf(gate.shipY + gate.velY * dt, -1.0f, 1.0f);

  float errX = gate.shipX - gate.targetX * 0.40f;
  float errY = gate.shipY - gate.targetY * 0.40f;
  float err = sqrtf(errX * errX + errY * errY);
  float focus = clampf(1.0f - err * 1.25f, 0.0f, 1.0f);
  float buzzFit = (buzzSeen && safeElapsedMs(now, lastBuzzMs) < 9000UL) ? 1.0f : 0.36f;
  float bestFit = clampf(((float)bestBits - 18.0f) / 10.0f, 0.0f, 1.0f);

  gate.stability += (focus * 0.36f + buzzFit * 0.10f + warpBoost * 0.030f - 0.16f - gate.heat * 0.08f) * dt;
  gate.stability = clampf(gate.stability, 0.0f, 1.0f);
  gate.charge += (0.006f + bestFit * 0.026f + gate.stability * 0.018f + gate.tailGlow * 0.014f + warpBoost * 0.055f + corpusBoost * 0.006f + bsPull * 0.018f) * dt;
  gate.charge = clampf(gate.charge, 0.0f, 1.0f);
  gate.aperture += ((gate.charge * 0.65f + gate.stability * 0.35f) - gate.aperture) * dt * 1.55f;
  gate.heat = max(0.0f, gate.heat - dt * 0.18f);
  gate.tailGlow = max(0.0f, gate.tailGlow - dt * 0.78f);
  gate.pulse = max(0.0f, gate.pulse - dt * 1.25f);
  gate.shake = max(0.0f, gate.shake - dt * 1.60f);

  bool handAiming = !gate.autoPilot && imuManualIntent < 0.20f && now > imuPoseSettleUntilMs;
  float skyTarget = handAiming ? 1.0f : 0.0f;
  gate.skyLock += (skyTarget - gate.skyLock) * dt * (handAiming ? 1.20f : 0.80f);
  gate.skyLock = clampf(gate.skyLock, 0.0f, 1.0f);
  if (gate.skyLock > 0.72f) {
    gate.stability = min(1.0f, gate.stability + dt * 0.020f);
    gate.charge = min(1.0f, gate.charge + dt * 0.010f);
    if ((millis() & 0x7FF) < 34) snprintf(statusLine, sizeof(statusLine), "SKY LOCK");
  }

  if ((gate.charge >= 0.995f && gate.stability > 0.58f) ||
      (mercuryBoost > 0.55f && gate.charge > 0.82f && gate.stability > 0.42f)) {
    openGate();
  }

  float speed = 0.28f + gate.charge * 0.42f + (gate.autoPilot ? 0.04f : imuManualIntent * 0.12f);
  for (int i = 0; i < YG_PARTICLE_COUNT; ++i) {
    particles[i].z -= dt * speed * particles[i].speed;
    particles[i].x += sinf(gate.phase + particles[i].hue) * dt * 0.018f * (1.0f + gate.tailGlow);
    particles[i].y += cosf(gate.phase * 0.7f + particles[i].hue) * dt * 0.012f;
    if (particles[i].z < 0.10f || fabsf(particles[i].x) > 1.9f || fabsf(particles[i].y) > 1.4f) resetParticle(particles[i], true);
  }
  for (int i = 0; i < YG_RIFT_COUNT; ++i) {
    rifts[i].z -= dt * (0.16f + gate.charge * 0.20f);
    rifts[i].spin += dt * (0.6f + rifts[i].kind * 0.22f);
    if (rifts[i].z < 0.16f) resetRift(rifts[i], true);
  }
}

uint16_t watchBitsForJob(uint16_t targetBits) {
  uint16_t floorBits = gate.autoPilot ? 21 : 22;
  if (targetBits > 2) floorBits = max(floorBits, (uint16_t)(targetBits - 2));
  return floorBits;
}

void processOneJob(RemoteJobState& job, bool stale, uint16_t batch) {
  if (!job.active) return;
  uint32_t now = millis();
  uint32_t age = stale ? (safeElapsedMs(now, job.rxMs) + YG_REMOTE_JOB_TTL_MS) : safeElapsedMs(now, job.rxMs);
  if (!stale && age > YG_REMOTE_JOB_TTL_MS) {
    copyCurrentToStale(1);
    currentJob.active = false;
    return;
  }
  if (stale && safeElapsedMs(now, job.rxMs) > YG_STALE_SHADOW_MS) {
    job.active = false;
    return;
  }
  if (job.nonce == job.endNonce) {
    job.active = false;
    return;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  uint8_t header[80];
  uint8_t hash1[32];
  uint8_t hash2[32];
  uint8_t shareHash[32];
  uint16_t targetBits = countLeadingZeroBits(job.target);
  uint16_t watchBits = watchBitsForJob(targetBits);

  for (uint16_t i = 0; i < batch; ++i) {
    if (job.nonce == job.endNonce) {
      job.active = false;
      break;
    }
    memcpy(header, job.header, 80);
    uint32_t n = job.nonce++;
    writeLE32(header + 76, n);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, header, 80);
    mbedtls_sha256_finish(&ctx, hash1);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, hash1, 32);
    mbedtls_sha256_finish(&ctx, hash2);
    hashToShareOrder(hash2, shareHash);
    observerHashesTotal++;
    observerHashesWindow++;
    uint16_t bits = countLeadingZeroBits(shareHash);
    if (bits > bestBits) {
      bestBits = bits;
      markYaksStateDirty();
    }
    observeNearTailCorpus(bits, n);
    bool targetPass = bits >= targetBits && hashMeetsTargetBytes(shareHash, job.target);
    if (stale && targetPass) {
      sendTailPacket(2, 2, 3, bits, n, 1, true, shareHash + 28);
    } else if (!stale && bits >= watchBits) {
      sendTailPacket(1, 1, gate.autoPilot ? 2 : 1, bits, n, targetPass ? 3 : 0, targetPass, shareHash + 28);
    }
  }
  mbedtls_sha256_free(&ctx);
}

void runObserverMiner() {
  bool nasWarp = gate.autoPilot && millis() < nasWarpUntilMs;
  uint16_t batch = gate.autoPilot ? (nasWarp ? 230 : 190) : 82;
  if (!buzzSeen || safeElapsedMs(millis(), lastBuzzMs) > 12000UL) batch = 42;
  processOneJob(currentJob, false, batch);
  processOneJob(staleJob, true, batch / 2 + 8);
}

void updateHashrate() {
  uint32_t now = millis();
  if (!hashWindowMs) hashWindowMs = now;
  if (now - hashWindowMs >= 1000UL) {
    observerHashrate = observerHashesWindow;
    observerHashesWindow = 0;
    hashWindowMs = now;
  }
}

void irInit() {
  rmt_tx_channel_config_t tx = {};
  tx.gpio_num = (gpio_num_t)YG_IR_TX_PIN;
  tx.clk_src = RMT_CLK_SRC_DEFAULT;
  tx.resolution_hz = 1000000;
  tx.mem_block_symbols = 96;
  tx.trans_queue_depth = 2;
  tx.flags.invert_out = 0;
  tx.flags.with_dma = 0;
  if (rmt_new_tx_channel(&tx, &irTxChan) != ESP_OK) {
    irReady = false;
    return;
  }
  rmt_carrier_config_t carrier = {};
  carrier.frequency_hz = YG_IR_CARRIER_HZ;
  carrier.duty_cycle = 0.22f;
  carrier.flags.polarity_active_low = 0;
  rmt_apply_carrier(irTxChan, &carrier);
  rmt_copy_encoder_config_t enc = {};
  if (rmt_new_copy_encoder(&enc, &irCopyEncoder) != ESP_OK) {
    irReady = false;
    return;
  }
  irReady = (rmt_enable(irTxChan) == ESP_OK);
}

void pushSymbol(rmt_symbol_word_t* syms, size_t& n, uint16_t mark, uint16_t space) {
  syms[n].level0 = 1;
  syms[n].duration0 = mark;
  syms[n].level1 = 0;
  syms[n].duration1 = space;
  n++;
}

bool sendIrShaSigil(uint32_t seed, bool skyBeacon = false, bool escapeBeacon = false) {
  if (!irReady) return false;
  uint32_t now = millis();
  if (now - lastIrBurstMs < YG_IR_BURST_GUARD_MS) return false;
  bool longBeacon = skyBeacon || escapeBeacon;
  uint32_t beaconGap = escapeBeacon ? YG_IR_ESCAPE_MS : YG_IR_SKY_MS;
  if (longBeacon && now - lastSkyIrMs < beaconGap) return false;
  uint8_t buf[28];
  memset(buf, 0, sizeof(buf));
  memcpy(buf, YG_NODE_ID, min((size_t)12, strlen(YG_NODE_ID)));
  uint32_t salt = escapeBeacon ? 0xE5CA9E32UL : (skyBeacon ? 0x5A59A11CUL : 0x1A2B3C4DUL);
  writeLE32(buf + 12, seed ^ salt ^ ((uint32_t)flashBubble.sector << 24) ^ ((uint32_t)flashBubble.predictedSector << 16));
  writeLE32(buf + 16, bestBits);
  writeLE32(buf + 20, tailSeq ^ flashBubble.seq ^ (flashBubble.worldFlags << 1));
  writeLE32(buf + 24, (uint32_t)(gate.charge * 65535.0f) ^ ((uint32_t)(gate.stability * 65535.0f) << 16) ^ (uint32_t)(flashBubble.siliconHeat * 4096.0f));
  uint8_t h[32];
  doubleSha256(buf, sizeof(buf), h);

  rmt_symbol_word_t syms[48];
  size_t n = 0;
  pushSymbol(syms, n, escapeBeacon ? 2240 : (skyBeacon ? 1920 : 1600), escapeBeacon ? 820 : (skyBeacon ? 960 : 720));
  for (int i = 0; i < 32 && n < 46; ++i) {
    bool bit = h[i >> 3] & (1 << (i & 7));
    bool twist = h[(i + 11) & 31] & (1 << ((i + 3) & 7));
    uint16_t mark = bit ? 392 : 248;
    uint16_t space = bit ? 920 : 360;
    if (skyBeacon || escapeBeacon) {
      mark += twist ? 96 : 0;
      space += ((i & 3) == 0) ? 180 : 0;
    }
    if (escapeBeacon) {
      mark += ((h[(i + 19) & 31] >> (i & 3)) & 1) ? 62 : 0;
      space += ((i + flashBubble.sector) & 7) == 0 ? 240 : 0;
    }
    if ((i & 7) == 0) space += 170;
    pushSymbol(syms, n, mark, space);
  }
  pushSymbol(syms, n, 220, escapeBeacon ? 6400 : (skyBeacon ? 5200 : 3600));
  rmt_transmit_config_t cfg = {};
  cfg.loop_count = 0;
  cfg.flags.queue_nonblocking = 1;
  rmt_transmit(irTxChan, irCopyEncoder, syms, n * sizeof(rmt_symbol_word_t), &cfg);
  lastIrBurstMs = now;
  if (longBeacon) lastSkyIrMs = now;
  if (escapeBeacon) {
    flashBubble.irBeaconTx++;
    flashBubble.lastIrBeaconMs = now;
  }
  gate.sigils++;
  gate.pulse = min(1.0f, gate.pulse + 0.20f);
  markYaksStateDirty();
  return true;
}

void handleInput() {
  bool top = topButtonDown();
  bool blue = M5.BtnA.isPressed();
  uint32_t now = millis();

  if (top && !topDownPrev) {
    topDownAt = now;
    topLongDone = false;
    lastInputMs = now;
  }
  if (top && now - topDownAt > 420UL && now - lastBrightnessStepMs > 150UL) {
    topLongDone = true;
    lastBrightnessStepMs = now;
    stepBrightness();
    lastInputMs = now;
  }
  if (!top && topDownPrev) {
    uint32_t held = now - topDownAt;
    if (!topLongDone && held > 35UL && held < 520UL) {
      lastInputMs = now;
      fireGatePulse(true);
      if (now - topTapWindowMs > 3600UL) {
        topTapWindowMs = now;
        topTapCount = 0;
      }
      topTapCount++;
      if (topTapCount >= 10) {
        topTapCount = 0;
        irSigilEnabled = !irSigilEnabled;
        snprintf(statusLine, sizeof(statusLine), irSigilEnabled ? "IR SIGIL ON" : "IR SIGIL OFF");
        markYaksStateDirty();
        if (irSigilEnabled) sendIrShaSigil(now ^ esp_random(), false);
      }
    }
  }
  topDownPrev = top;

  if (blue && !blueDownPrev) {
    blueDownAt = now;
    blueLongDone = false;
  }
  if (blue && !blueLongDone && now - blueDownAt > 1200UL) {
    blueLongDone = true;
    lastInputMs = now;
    gate.stability = min(1.0f, gate.stability + 0.055f);
    gate.charge = min(1.0f, gate.charge + 0.035f);
    snprintf(statusLine, sizeof(statusLine), "GATE HOLD");
  }
  if (!blue && blueDownPrev) {
    uint32_t held = now - blueDownAt;
    if (!blueLongDone && held > 25UL && held < 850UL) {
      lastInputMs = now;
      if (now - blueTapWindowMs > 3600UL) {
        blueTapWindowMs = now;
        blueTapCount = 0;
      }
      blueTapCount++;
      fireGatePulse(true);
      bool sent = false;
      if (irSigilEnabled || gate.skyLock > 0.55f) {
        sent = sendIrShaSigil(now ^ observerHashesTotal ^ ((uint32_t)bestBits << 16), gate.skyLock > 0.55f);
      }
      if (gate.skyLock > 0.55f && irSigilEnabled) {
        if (sent) {
          gate.skySigils++;
          sendEvent(15, "yaks_sky_beacon", 94, 58, (int16_t)bestBits, (int16_t)gate.skySigils, (int16_t)(gate.skyLock * 1000.0f), (int16_t)(gate.stability * 1000.0f));
          snprintf(statusLine, sizeof(statusLine), "SKY BEACON %lu", (unsigned long)gate.skySigils);
        } else {
          snprintf(statusLine, sizeof(statusLine), "SKY HOLD");
        }
      } else if (sent) {
        sendEvent(14, "yaks_ir_sigil", 90, 44, (int16_t)bestBits, (int16_t)gate.sigils, (int16_t)(gate.charge * 1000.0f), 0);
      }
    }
  }
  blueDownPrev = blue;

  if (blue) {
    lastInputMs = now;
  }
}

uint16_t particleColor(uint8_t hue, float z, float glow) {
  uint8_t v = (uint8_t)clampf(80.0f + (1.0f - z / 2.4f) * 155.0f + glow * 80.0f, 40.0f, 255.0f);
  switch (hue & 7) {
    case 0: return canvas.color565(v / 3, v, v);
    case 1: return canvas.color565(v, v / 2, 72);
    case 2: return canvas.color565(92, v, 255);
    case 3: return canvas.color565(255, v, 92);
    case 4: return canvas.color565(v / 2, 172, v);
    default: return canvas.color565(v, v, v);
  }
}

int activeAsciiCols() {
  int cols = screenW / 6;
  if (cols < 8) cols = 8;
  if (cols > YG_ASCII_COLS) cols = YG_ASCII_COLS;
  return cols;
}

int activeAsciiRows() {
  int rows = screenH / 8;
  if (rows < 6) rows = 6;
  if (rows > YG_ASCII_ROWS) rows = YG_ASCII_ROWS;
  return rows;
}

uint8_t murphHashBit(uint16_t bitIndex) {
  uint8_t b = gateHashBytes[(bitIndex >> 3) & 31];
  return (uint8_t)((b >> (bitIndex & 7)) & 1U);
}

void updateHashAsciiArt(uint32_t now) {
  if (now - lastAsciiHashMs < 190UL && lastAsciiHashMs) return;
  lastAsciiHashMs = now;
  int cols = activeAsciiCols();
  int rows = activeAsciiRows();

  uint8_t seed[112];
  memset(seed, 0, sizeof(seed));
  memcpy(seed, YG_NODE_ID, min((size_t)12, strlen(YG_NODE_ID)));
  if (currentJob.active || currentJob.rxMs) {
    memcpy(seed + 12, currentJob.jobId, 8);
    memcpy(seed + 20, currentJob.target, 16);
    writeLE32(seed + 36, currentJob.nonce);
    writeLE32(seed + 40, currentJob.startNonce);
    writeLE32(seed + 44, currentJob.rangeSize);
  }
  writeLE32(seed + 48, observerHashesTotal);
  writeLE32(seed + 52, bestBits);
  writeLE32(seed + 56, tailSeq);
  writeLE32(seed + 60, (uint32_t)(gate.charge * 100000.0f));
  writeLE32(seed + 64, (uint32_t)(gate.stability * 100000.0f));
  writeLE32(seed + 68, (uint32_t)(gate.skyLock * 100000.0f));
  writeLE32(seed + 72, now / 240UL);
  writeLE32(seed + 76, mix32(now ^ observerHashesTotal ^ ((uint32_t)bestBits << 13)));
  writeLE32(seed + 80, ((uint32_t)blackStarLink.lane << 24) ^ ((uint32_t)blackStarLink.bestLane << 16) ^ blackStarLink.corpus);
  writeLE32(seed + 84, (uint32_t)(blackStarPull(now) * 100000.0f));
  writeLE32(seed + 88, blackStarLink.seq ^ (uint32_t)(blackStarLink.mercuryTorr * 100.0f) ^
                       ((uint32_t)(blackStarLink.torricelliVoid * 1000.0f) << 16));
  writeLE32(seed + 92, hash16(blackStarLink.source) ^
                       ((uint32_t)(blackStarLink.mercuryTime * 1000.0f) << 16) ^
                       (uint32_t)(blackStarLink.hawkingVapor * 1000.0f));
  writeLE32(seed + 96, flashBubble.worldFlags ^
                       ((uint32_t)(brothers.anchorOxy * 1000.0f) << 16) ^
                       (uint32_t)(brothers.gladiusOxy * 1000.0f));
  writeLE32(seed + 100, flashBubble.seq ^
                        ((uint32_t)(mercuryWarpPower(now) * 1000.0f) << 16) ^
                        (uint32_t)(mercuryWarp.readySeq & 0xFFFFUL));
  writeLE32(seed + 104, flashBubble.irBeaconTx ^ ((uint32_t)flashBubble.priority << 16));
  writeLE32(seed + 108, (uint32_t)(flashBubble.siliconHeat * 65535.0f) ^ ((uint32_t)(flashBubble.siliconLoad * 65535.0f) << 12));
  doubleSha256(seed, sizeof(seed), gateHashBytes);

  float bsPull = blackStarPull(now);
  float siliconTrace = clampf(flashBubble.siliconHeat * 0.34f + flashBubble.siliconLoad * 0.42f + bsPull * 0.55f + gate.skyLock * 0.24f, 0.0f, 3.0f);
  uint32_t murphClock = now / (uint32_t)max(90.0f, 210.0f + bsPull * 420.0f + siliconTrace * 70.0f);
  for (int col = 0; col < cols; ++col) {
    uint16_t bitBase = (uint16_t)(col + ((murphClock & 31UL) * 3UL) + ((uint32_t)bestBits << 1));
    uint8_t b0 = murphHashBit(bitBase);
    uint8_t b1 = murphHashBit(bitBase + 47);
    murphMorseLine[col] = b0 ? (b1 ? 2 : 1) : 0; // 1 dot, 2 dash, zero silence.
    uint8_t v = gateHashBytes[(col * 5 + murphClock) & 31];
    murphTesseractCols[col] = (uint8_t)(((v + col * 13 + (uint8_t)(siliconTrace * 19.0f)) & 31) < (uint8_t)(4 + min(10, (int)(siliconTrace * 4.0f))));
  }

  static const char density[] = " .'`,:;~-_=+*xX#%@";
  static const char streamChars[] = "/\\|!li1[]{}()";
  static const char stoneChars[] = "vVwWmM#%&";
  static const char runeChars[] = "$@&%#*+=-:^~";
  static const char hex[] = "0123456789ABCDEF";
  float flashGlow = clampf((flashBubble.siliconHeat + flashBubble.siliconLoad) * 0.22f + (flashBubble.state >= 2 ? 0.10f : 0.0f), 0.0f, 0.75f);
  float visualStability = max(gate.stability, 0.10f + imuManualIntent * 0.10f + bsPull * 0.06f);
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      float nx = ((float)col - ((float)cols - 1.0f) * 0.5f) / max(1.0f, ((float)cols - 1.0f) * 0.5f);
      float ny = ((float)row - ((float)rows - 1.0f) * 0.5f) / max(1.0f, ((float)rows - 1.0f) * 0.5f);
      uint8_t hv = gateHashBytes[(row * 7 + col * 3 + (now / 240UL)) & 31];
      uint8_t hv2 = gateHashBytes[(row * 11 + col * 5 + 13 + (now / 480UL)) & 31];
      uint8_t hv3 = gateHashBytes[(row * 3 + col * 13 + 7 + (now / 960UL)) & 31];
      uint32_t cellMix = mix32(((uint32_t)hv << 24) ^ ((uint32_t)hv2 << 12) ^ (uint32_t)(row * 131 + col * 977) ^ (now / 96UL));

      float phase = gate.spin * 1.48f + (float)(now & 8191) * 0.00105f;
      float tiltX = clampf(imuRoll * 0.10f, -0.16f, 0.16f);
      float tiltY = clampf(imuPitch * 0.10f, -0.12f, 0.12f);
      float sx = (nx + tiltX) * (1.08f + gate.skyLock * 0.10f);
      float sy = (ny + tiltY) * (0.98f + (1.0f - visualStability) * 0.08f);
      float noise = ((float)hv / 255.0f - 0.5f);
      float d = sqrtf(sx * sx + sy * sy * 1.34f);
      float a = atan2f(sy * 1.20f, sx);

      float gateRadius = 0.40f + gate.aperture * 0.23f + noise * 0.050f + bsPull * 0.035f + flashGlow * 0.018f;
      float ring = 1.0f - fabsf(d - gateRadius) * (7.5f + gate.charge * 3.0f);
      float innerRing = 1.0f - fabsf(d - (0.19f + gate.tailGlow * 0.055f)) * 12.0f;
      float lens = expf(-fabsf(sy + sinf(sx * (5.2f + bsPull * 1.4f) + phase) * (0.045f + bsPull * 0.018f)) * (6.2f - bsPull * 0.8f)) *
                   (0.30f + 0.72f * (1.0f - fabsf(sx)));
      float spiral = (sinf(a * (7.0f + (float)(hv & 3)) + phase * 2.1f + d * 9.0f) + 1.0f) * 0.5f;
      float rain = ((float)((hv2 + row * 19 + (now / 64UL)) & 63) / 63.0f) * (ny < -0.08f ? 0.45f : 0.16f);
      float scan = (((cellMix >> 8) & 15) == ((now >> 7) & 15)) ? 0.25f : 0.0f;
      float horizon = 0.50f + sinf(nx * 4.8f + (float)hv3 * 0.022f + phase * 0.12f) * 0.055f;
      float ridge = max(0.0f, 1.0f - fabsf(ny - horizon) * 19.0f);
      float ground = ny > horizon ? 0.48f + (ny - horizon) * 1.85f : 0.0f;
      float beam = expf(-fabsf(nx - sinf(phase * 0.25f) * 0.040f) * 18.0f) *
                   clampf(0.68f - fabsf(ny), 0.0f, 1.0f);
      float leftAnchor = (fabsf(nx + 0.82f) < 0.030f && ny < 0.46f) ? 0.62f : 0.0f;
      float rightAnchor = (fabsf(nx - 0.84f) < 0.026f && ny < 0.28f) ? 0.54f : 0.0f;
      float anchorWire = (ny < -0.36f && fabsf(ny + 0.54f - sinf(nx * 9.0f + phase * 0.4f) * 0.035f) < 0.018f) ? 0.34f : 0.0f;
      float coreVoid = d < 0.145f ? (0.145f - d) * (8.5f + visualStability * 2.0f) : 0.0f;
      float fracture = ((1.0f - visualStability) + flashGlow * 0.45f) * ((cellMix & 7) == 0 ? 0.52f : 0.0f);
      float clockA = ((float)(murphClock & 63UL) / 64.0f) * 6.2831853f + (float)(gateHashBytes[(murphClock + 9) & 31] & 15) * 0.017f;
      float watchHand = (d > 0.145f && d < 0.46f && fabsf(sinf(a - clockA)) * d < 0.014f + siliconTrace * 0.004f) ? 0.58f : 0.0f;
      float mazeX = fabsf(fmodf((sx + 1.35f) * (5.0f + (float)((hv & 3))) + (float)((hv2 & 7)) * 0.071f, 1.0f) - 0.5f);
      float mazeY = fabsf(fmodf((sy + 1.20f) * (4.0f + (float)((hv2 & 3))) + (float)((hv3 & 7)) * 0.083f, 1.0f) - 0.5f);
      bool siliconMaze = siliconTrace > 0.18f && ny > -0.18f && (mazeX < 0.040f || mazeY < 0.035f) && ((cellMix >> 13) & 3);
      bool tesseractThread = murphTesseractCols[col] && row > 1 && row < rows - 2 && ((row + (int)murphClock + (hv & 3)) & 3) != 0;
      bool murphPulse = (row == rows - 2 || row == rows - 3) && murphMorseLine[col] && ((col + (int)(now >> 7)) & (murphMorseLine[col] == 2 ? 1 : 3)) == 0;
      bool siliconSign = siliconMaze && ((cellMix ^ now ^ observerHashesTotal) & 15) == 0;

      float intensity = clampf(
        max(max(ring, innerRing * 0.72f), lens) +
        spiral * (0.24f + bsPull * 0.12f) + rain + scan + ground + ridge * (0.42f + bsPull * 0.10f) +
        beam * (0.24f + gate.pulse * 0.34f + gate.charge * 0.16f) +
        leftAnchor + rightAnchor + anchorWire + fracture + watchHand + (siliconMaze ? 0.20f + siliconTrace * 0.06f : 0.0f) + (tesseractThread ? 0.18f : 0.0f) + (murphPulse ? 0.55f : 0.0f) -
        coreVoid,
        0.0f, 1.0f
      );

      bool voidCore = d < 0.138f && intensity < 0.66f;
      char ch;
      if (voidCore) {
        ch = ' ';
      } else if (murphPulse) {
        ch = murphMorseLine[col] == 2 ? '=' : '.';
      } else if (watchHand > 0.42f) {
        ch = ((cellMix >> 4) & 1) ? '/' : '\\';
      } else if (siliconSign) {
        ch = runeChars[(hv ^ hv2 ^ hv3) % (sizeof(runeChars) - 1)];
      } else if (siliconMaze) {
        ch = ((cellMix >> 9) & 1) ? '+' : '#';
      } else if (tesseractThread) {
        ch = ((row + col + (int)murphClock) & 1) ? '|' : '!';
      } else if (leftAnchor > 0.55f || rightAnchor > 0.50f) {
        ch = (row & 1) ? '|' : '#';
      } else if (anchorWire > 0.2f) {
        ch = ((col + row) & 1) ? '-' : '=';
      } else if (ny > horizon + 0.030f) {
        ch = stoneChars[(hv + col + row) % (sizeof(stoneChars) - 1)];
      } else if (ny < -0.08f && rain > 0.25f) {
        ch = streamChars[(hv2 + row + col) % (sizeof(streamChars) - 1)];
      } else if (ring > 0.52f || innerRing > 0.54f) {
        ch = hex[(hv >> ((col & 1) ? 0 : 4)) & 0x0F];
      } else if (beam > 0.34f || fracture > 0.20f) {
        ch = runeChars[(hv3 + row * 3 + col) % (sizeof(runeChars) - 1)];
      } else {
        int denIdx = (int)clampf(intensity * (float)(sizeof(density) - 2), 0.0f, (float)(sizeof(density) - 2));
        ch = density[denIdx];
      }
      if (!voidCore && ((hv3 ^ row ^ col ^ (now >> 7)) & 47) == 0) ch = hex[hv3 & 0x0F];
      gateAscii[row][col] = ch;
      gateAsciiTone[row][col] = (uint8_t)clampf(34.0f + intensity * 218.0f + ((float)(hv & 31) * 1.15f), 0.0f, 255.0f);
    }
    gateAscii[row][cols] = 0;
  }
}

void drawHashAsciiScreen() {
  updateHashAsciiArt(millis());
  int cols = activeAsciiCols();
  int rows = activeAsciiRows();
  int x0 = max(0, (screenW - cols * 6) / 2);
  int y0 = max(0, (screenH - rows * 8) / 2);
  canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM);
  for (int row = 0; row < rows; ++row) {
    for (int col = 0; col < cols; ++col) {
      char ch = gateAscii[row][col];
      if (ch == ' ') continue;
      uint8_t t = gateAsciiTone[row][col];
      uint16_t color = (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')
                       ? canvas.color565(t, max(80, (int)t - 32), 42 + (t / 5))
                       : ((ch == 'v' || ch == 'V' || ch == 'w' || ch == 'W' || ch == 'm' || ch == 'M')
                          ? canvas.color565(54 + (t / 9), 88 + (t / 5), 74 + (t / 7))
                          : canvas.color565(32 + (t / 7), 105 + (t / 3), t));
      if (gate.skyLock > 0.55f && ((row + col + (millis() >> 8)) & 3) == 0) {
        color = canvas.color565(90, 220, 255);
      }
      float bs = blackStarPull(millis());
      if (bs > 0.08f && ((row + col + blackStarLink.bestLane) & 5) == 0) {
        if (blackStarLink.bestLane == 2) color = canvas.color565(120 + (int)(bs * 80), 210, 255);
        else if (blackStarLink.bestLane == 3) color = canvas.color565(255, 195 + (int)(bs * 40), 88);
        else if (blackStarLink.bestLane == 1) color = canvas.color565(180, 235, 170);
        else color = canvas.color565(220, 220, 190);
      }
      canvas.setTextColor(color, TFT_TRANSPARENT);
      canvas.setCursor(x0 + col * 6, y0 + row * 8);
      canvas.print(ch);
    }
  }
}

void drawBattery(int x, int y) {
  int pct = M5.Power.getBatteryLevel();
  if (pct < 0 || pct > 100) pct = 0;
  uint16_t edge = canvas.color565(86, 150, 180);
  uint16_t fill = pct > 50 ? canvas.color565(82, 220, 170) : (pct > 20 ? canvas.color565(242, 192, 74) : canvas.color565(245, 78, 74));
  canvas.drawRect(x, y, 18, 8, edge);
  canvas.fillRect(x + 18, y + 2, 2, 4, edge);
  int w = map(pct, 0, 100, 0, 14);
  if (w > 0) canvas.fillRect(x + 2, y + 2, w, 4, fill);
}

void drawGateField() {
  int cx = screenW / 2;
  int cy = screenH / 2 + 2;
  if (gate.shake > 0.0f) {
    cx += ((millis() >> 4) & 1) ? 1 : -1;
    cy += ((millis() >> 5) & 1) ? 1 : -1;
  }
  uint16_t bg = canvas.color565(3, 6, 12);
  canvas.fillSprite(bg);

  drawHashAsciiScreen();

  int tx = cx + (int)(gate.targetX * 34.0f);
  int ty = cy + (int)(gate.targetY * 20.0f);
  uint16_t targetCol = canvas.color565(210, 176, 72);
  canvas.setTextSize(1);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(targetCol, TFT_TRANSPARENT);
  canvas.drawString("+", tx, ty);

  int sx = cx + (int)(gate.shipX * 48.0f);
  int sy = cy + (int)(gate.shipY * 27.0f);
  uint16_t shipCol = gate.autoPilot ? canvas.color565(94, 170, 255) : canvas.color565(255, 232, 146);
  canvas.setTextColor(shipCol, TFT_TRANSPARENT);
  canvas.drawString(gate.autoPilot ? "A" : "Y", sx, sy);
  if (gate.pulse > 0.18f) {
    canvas.setTextColor(canvas.color565(255, 236, 168), TFT_TRANSPARENT);
    canvas.drawString("*", sx, sy - 8);
  }
}

void drawHud() {
  uint16_t line = canvas.color565(28, 72, 94);
  uint16_t text = canvas.color565(188, 232, 230);
  uint16_t gold = canvas.color565(238, 182, 72);
  canvas.drawFastHLine(0, 11, screenW, line);
  canvas.drawFastHLine(0, screenH - 10, screenW, line);

  canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(text, TFT_TRANSPARENT);
  canvas.drawString("YAKS", 2, 1);
  canvas.setTextColor(gold, TFT_TRANSPARENT);
  canvas.drawString("skaY", 30, 1);
  canvas.setTextDatum(TC_DATUM);
  canvas.setTextColor(gate.autoPilot ? canvas.color565(96, 178, 255) : canvas.color565(255, 218, 118), TFT_TRANSPARENT);
  canvas.drawString(laneName(), screenW / 2, 1);
  drawBattery(screenW - 22, 1);

  int barX = 2;
  int barY = screenH - 8;
  int barW = 52;
  canvas.drawRect(barX, barY, barW, 6, canvas.color565(50, 86, 98));
  canvas.fillRect(barX + 1, barY + 1, (int)((barW - 2) * gate.charge), 4, gold);
  canvas.drawRect(barX + 58, barY, 42, 6, canvas.color565(50, 86, 98));
  canvas.fillRect(barX + 59, barY + 1, (int)(40 * gate.stability), 4, canvas.color565(72, 214, 184));

  canvas.setTextDatum(TR_DATUM);
  canvas.setTextColor(text, TFT_TRANSPARENT);
  char right[40];
  snprintf(right, sizeof(right), "H%lu B%lu", (unsigned long)observerHashrate, (unsigned long)bestBits);
  canvas.drawString(right, screenW - 2, screenH - 9);

  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(canvas.color565(122, 182, 182), TFT_TRANSPARENT);
  char small[48];
  uint32_t now = millis();
  bool nasWarp = gate.autoPilot && now < nasWarpUntilMs;
  bool mw = mercuryWarpPower(now) > 0.0f;
  snprintf(small, sizeof(small), "%s %s%s%s%s",
           mw ? "HG" : (nasWarp ? "NAS" : (buzzSeen && safeElapsedMs(now, lastBuzzMs) < 9000UL ? "BUZZ" : "LOCAL")),
           irSigilEnabled ? "IR" : "--",
           brothersReady(now) ? " BRO" : (gate.skyLock > 0.45f ? " SKY" : ""),
           flashEscapeReady(now) ? " ESC" : "",
           mw ? " MWARP" : (nasWarp ? " WARP" : ""));
  canvas.drawString(small, 104, screenH - 9);

  if (gate.pulse > 0.35f) {
    canvas.setTextDatum(TC_DATUM);
    canvas.setTextColor(canvas.color565(255, 236, 168), TFT_TRANSPARENT);
    canvas.drawString(statusLine, screenW / 2, 13);
  }
}

void drawFrame() {
  if (!canvasReady) return;
  drawGateField();
  drawHud();
  canvas.pushSprite(0, 0);
}

void serviceRadio() {
  uint32_t now = millis();
  if (now - lastHbMs >= YG_HEARTBEAT_MS) {
    lastHbMs = now;
    sendHeartbeat();
  }
  if (now - lastEntropyMs >= YG_ENTROPY_MS) {
    lastEntropyMs = now;
    sendEntropy();
  }
  if (now - lastPnCortexMs >= YG_PN_CORTEX_MS) {
    lastPnCortexMs = now;
    sendPnCortex();
  }
}

void serviceIr() {
  if (!irSigilEnabled || !irReady) return;
  uint32_t now = millis();
  bool escape = flashEscapeReady(now);
  uint32_t interval = escape ? YG_IR_ESCAPE_MS : YG_IR_AUTO_MS;
  if (now - lastIrMs < interval) return;
  lastIrMs = now;
  bool sent = sendIrShaSigil(now ^ observerHashesTotal ^ ((uint32_t)bestBits << 16) ^
                             flashBubble.worldFlags ^ ((uint32_t)(mercuryWarpPower(now) * 65535.0f) << 1) ^
                             (mercuryWarp.readySeq * 0x45D9F3BUL), escape, escape);
  if (sent && escape) {
    gate.skySigils++;
    sendEvent(15, "yaks_escape_beacon", 96, 78,
              (int16_t)bestBits, (int16_t)gate.skySigils,
              (int16_t)(blackStarPull(now) * 1000.0f), (int16_t)flashBubble.priority);
    snprintf(statusLine, sizeof(statusLine), "IR ESCAPE BEACON");
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  Serial.begin(115200);
  delay(120);
  M5.Display.setRotation(1);
  M5.Display.setColorDepth(16);
  M5.Display.setBrightness(brightnessLevels[brightnessIdx]);
  screenW = M5.Display.width();
  screenH = M5.Display.height();
  canvas.setColorDepth(16);
  canvasReady = canvas.createSprite(screenW, screenH) != nullptr;
  if (canvasReady) canvas.setTextSize(1);
  M5.Speaker.begin();
  M5.Speaker.setVolume(10);
  LittleFS.begin(true);
  cleanupTailLog();
  cleanupYaksCorpusLog();

  M5.Imu.begin();
  calibrateImu(true);
  initGateWorld();
  bool stateOk = loadYaksState();
  M5.Display.setBrightness(brightnessLevels[brightnessIdx]);
  initEspNow();
  irInit();
  lastInputMs = millis();
  lastFrameMs = millis();
  sendEvent(1, "yaks_gate_boot", 96, 28, (int16_t)workerId(), 0, 0, 0);
  Serial.printf("[YAKS] %s ready node=%s screen=%dx%d irPin=%u carrier=%uHz observer_only=1 no_s2=1\n",
                YG_VERSION, YG_NODE_ID, screenW, screenH, (unsigned)YG_IR_TX_PIN, (unsigned)YG_IR_CARRIER_HZ);
  Serial.printf("[YAKS/STATE] loaded=%u path=%s best=%lu gate=%lu ir=%u bright=%u corp=%lu z%u score=%.3f bs=%u/%s Hg=%.0f void=%.2f warp=%lu fb=%u/%u sec=%u>%u tx=%lu rx=%lu irb=%lu\n",
                stateOk ? 1 : 0, YG_STATE_PATH,
                (unsigned long)bestBits, (unsigned long)gate.opens,
                irSigilEnabled ? 1 : 0, (unsigned)brightnessIdx,
                (unsigned long)yaksCorpus.events, (unsigned)yaksCorpus.bestZ, yaksCorpus.score,
                blackStarLink.seen ? 1 : 0, blackStarLaneName(blackStarLink.bestLane),
                blackStarLink.mercuryTorr, blackStarLink.torricelliVoid,
                (unsigned long)mercuryWarp.readySeq,
                (unsigned)flashBubble.state, (unsigned)flashBubble.jobState,
                (unsigned)flashBubble.sector, (unsigned)flashBubble.predictedSector,
                (unsigned long)flashBubble.tx, (unsigned long)flashBubble.rx,
                (unsigned long)flashBubble.irBeaconTx);
  Serial.println("[YAKS/DOCTRINE] StickS3 only: ESP-NOW swarm + local IR 38kHz optical sigil, no LoRa/RF transmitter here");
  Serial.println("[YAKS/SKY] ADV LoRa/GNSS 868 is a future separate sky-anchor node: position/time/course for game gates");
  Serial.println("[YAKS/FLASH] K2 silicon-flash bubble enabled: heat/load/hash traces feed the gate, IR is local escape beacon only");
  Serial.println("[YAKS/WARP] Mercury-BlackStar reverse drive: launch only after BH mercury field + Anchor/Gladius readiness");
}

void loop() {
  M5.update();
  uint32_t now = millis();
  float dt = (now - lastFrameMs) / 1000.0f;
  lastFrameMs = now;
  dt = clampf(dt, 0.001f, 0.050f);

  handleInput();
  updateImu(dt);
  updateGate(dt);
  serviceFlashBubble();
  serviceNasBrain();
  runObserverMiner();
  updateHashrate();
  serviceRadio();
  serviceIr();
  cleanupTailLog();
  cleanupYaksCorpusLog();
  serviceYaksState();

  if (now - lastDrawMs >= 33UL) {
    lastDrawMs = now;
    drawFrame();
  }
  if (now - lastDiagMs >= 5000UL) {
    lastDiagMs = now;
    Serial.printf("[YAKS] buzz=%u job=%u age=%lu H=%lu best=%lu target=%u tails=%lu stale=%lu poolR=%lu corp=%lu z%u score=%.2f gate=%lu charge=%.2f stab=%.2f ir=%u mode=%s bs=%u/%s pull=%.2f Hg=%.0f/%.2f/%.2f warp=%u/%.2f A=%.2f G=%.2f fb=%u/%u sec=%u>%u pr=%u k2=%lu/%lu irb=%lu heat=%.2f load=%.2f brain=%u/%d state=%u/%u grip=%s pose=%.2f imu=%.2f/%.2f intent=%.2f log=%s corpus=%s\n",
                  buzzSeen && safeElapsedMs(now, lastBuzzMs) < 9000UL ? 1 : 0,
                  currentJob.active ? 1 : 0,
                  (unsigned long)safeElapsedMs(now, currentJob.rxMs),
                  (unsigned long)observerHashrate,
                  (unsigned long)bestBits,
                  (unsigned)targetBitsNow,
                  (unsigned long)tailEvents,
                  (unsigned long)staleTailEvents,
                  (unsigned long)poolRejectEvents,
                  (unsigned long)yaksCorpus.events,
                  (unsigned)yaksCorpus.bestZ,
                  yaksCorpus.score,
                  (unsigned long)gate.opens,
                  gate.charge,
                  gate.stability,
                  irSigilEnabled ? 1 : 0,
                  laneName(),
                  blackStarActive(now) ? 1 : 0,
                  blackStarLaneName(blackStarLink.bestLane),
                  blackStarPull(now),
                  blackStarLink.mercuryTorr,
                  blackStarLink.torricelliVoid,
                  blackStarLink.mercuryTime,
                  mercuryWarp.launch && now < mercuryWarp.launchUntilMs ? 1 : 0,
                  mercuryWarpPower(now),
                  brothers.anchorOxy,
                  brothers.gladiusOxy,
                  (unsigned)flashBubble.state,
                  (unsigned)flashBubble.jobState,
                  (unsigned)flashBubble.sector,
                  (unsigned)flashBubble.predictedSector,
                  (unsigned)flashBubble.priority,
                  (unsigned long)flashBubble.rx,
                  (unsigned long)flashBubble.tx,
                  (unsigned long)flashBubble.irBeaconTx,
                  flashBubble.siliconHeat,
                  flashBubble.siliconLoad,
                  nasBrainOnline ? 1 : 0,
                  nasBrainLastCode,
                  yaksStateLoaded ? 1 : 0,
                  yaksStateDirty ? 1 : 0,
                  imuGripName(),
                  imuPoseDist,
                  imuRoll, imuPitch,
                  imuManualIntent,
                  YG_LOG_PATH,
                  YG_CORPUS_PATH);
  }
}
