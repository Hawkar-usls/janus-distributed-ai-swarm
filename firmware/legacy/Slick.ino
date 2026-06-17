/*
  JANUS_STICKS3_WITCHHUNTER_v0_1.ino

  StickS3 WitchHunter: a Diablo-like idle RPG shell around a safe RejectTail
  observer. This node does not submit shares to Buzz or to the pool.

  Doctrine:
  - Observe stale/reject/z-tail edges.
  - Never change Buzz scheduler.
  - Never increase submit pressure.
  - Never send share-response packets.
  - Keep accepted and reject/stale-tail corpus separate.

  Controls, landscape grip with the blue button on the right:
  - Blue button: attack in game, next tab in menu.
  - Top short press: apply selected menu tab action.
  - Top long hold: open menu; long hold again exits menu.
  - Ten quick top taps outside menu: toggle IR SHA-sigil.
  - IMU tilt: manual movement. If untouched for a few seconds, the hunter takes over.
  - Top short near world objects: interact with trader, ore, chests, shrines.
*/

#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_idf_version.h>
#include <LittleFS.h>
#include <mbedtls/sha256.h>
#include <math.h>

#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"

#define MESH_WIFI_SSID     "JANUS_WIFI_PLACEHOLDER"
#define MESH_WIFI_PASSWORD "JANUS_NET_PLACEHOLDER"

#define WH_NODE_ID             "WitchHunterS3"
#define WH_VERSION             "v1.0"
#define WH_IR_TX_PIN           46
#define WH_IR_CARRIER_HZ       38256
#define WH_REMOTE_JOB_TTL_MS   6500UL
#define WH_STALE_SHADOW_MS     2600UL
#define WH_HEARTBEAT_MS        1400UL
#define WH_ENTROPY_MS          2600UL
#define WH_TAIL_EVENT_MS       180UL
#define WH_INPUT_IDLE_MS       4200UL
#define WH_LOG_PATH            "/witch_tail.jsonl"
#define WH_SAVE_PATH           "/witch_save.bin"
#define WH_SLIME_SAVE_PATH     "/witch_slime.bin"
#define WH_META_SAVE_PATH      "/witch_meta.bin"
#define WH_LOG_MAX_BYTES       524288UL
#define WH_SAVE_MAGIC          0x57483234UL
#define WH_SAVE_VERSION        2
#define WH_SLIME_MAGIC         0x534C4D35UL
#define WH_SLIME_VERSION       1
#define WH_META_MAGIC          0x57484D37UL
#define WH_META_VERSION        1
#define WH_WORLD_LIMIT         8192.0f
#define WH_TOWN_X              4096.0f
#define WH_TOWN_Y              4096.0f
#define WH_ENEMY_COUNT         12
#define WH_OBJECT_COUNT        16
#define WH_PARTICLE_COUNT      34
#define WH_FLOAT_COUNT         8
#define WH_SLIME_INPUTS        6
#define WH_SLIME_ENEMY_ACTIONS 4
#define WH_SLIME_AUTO_ACTIONS  4
#define WH_SLIME_PISTON        16

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
  uint8_t staleReason;    // 0 none, 1 ttl, 2 replaced, 3 no_submit_rule, 4 buzz_reject_delta
  uint8_t targetPass;
  uint8_t hashTail[4];
  int8_t rssi;
  uint32_t uptimeMs;
};

enum MenuTab : uint8_t {
  TAB_INV = 0,
  TAB_SKILL = 1,
  TAB_CLASS = 2,
  TAB_QUEST = 3,
  TAB_TRADE = 4,
  TAB_NPC = 5,
  TAB_MINER = 6,
  TAB_SHADOW = 7,
  TAB_SETTINGS = 8,
  TAB_COUNT = 9
};

enum WorldObjType : uint8_t {
  OBJ_NONE = 0,
  OBJ_ORE = 1,
  OBJ_CHEST = 2,
  OBJ_SHRINE = 3,
  OBJ_RELIC = 4
};

enum HeroClass : uint8_t {
  CLASS_HUNTER = 0,
  CLASS_KNIGHT = 1,
  CLASS_MYSTIC = 2,
  CLASS_RANGER = 3,
  CLASS_COUNT = 4
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

struct Enemy {
  float x = 0;
  float y = 0;
  float hp = 0;
  float maxHp = 0;
  float hitT = 0;
  float attackCd = 0;
  float learnCd = 0;
  uint8_t kind = 0;
  uint8_t aiAction = 0;
  int8_t aiFeat[WH_SLIME_INPUTS] = {0};
  bool elite = false;
  bool active = false;
};

struct WorldObject {
  bool active = false;
  WorldObjType type = OBJ_NONE;
  float x = 0;
  float y = 0;
  uint8_t rarity = 0;
};

enum GearSlot : uint8_t {
  GEAR_BLADE = 0,
  GEAR_COAT = 1,
  GEAR_SEAL = 2,
  GEAR_COUNT = 3
};

struct GearItem {
  bool active = false;
  GearSlot slot = GEAR_BLADE;
  uint8_t rarity = 0;
  int atk = 0;
  int hp = 0;
  int focus = 0;
  char name[16] = "";
};

struct HunterState {
  float x = 80;
  float y = 40;
  float vx = 0;
  float vy = 0;
  float hp = 100;
  float maxHp = 100;
  float focus = 60;
  uint8_t level = 1;
  uint16_t xp = 0;
  uint16_t gold = 0;
  uint16_t ore = 0;
  uint8_t potions = 2;
  uint8_t skill = 1;
  int baseAtk = 9;
  int gearAtk = 0;
  int gearHp = 0;
  int gearFocus = 0;
  int gearScore = 0;
  float attackCd = 0;
  float slashT = 0;
  float faceX = 1;
  float faceY = 0;
  uint8_t autoAction = 0;
  int8_t autoFeat[WH_SLIME_INPUTS] = {0};
  bool autoPilot = true;
};

struct Particle {
  bool active = false;
  float x = 0;
  float y = 0;
  float vx = 0;
  float vy = 0;
  float t = 0;
  float ttl = 0;
  uint16_t color = 0;
};

struct FloatText {
  bool active = false;
  float x = 0;
  float y = 0;
  float t = 0;
  float ttl = 0;
  char text[10] = "";
  uint16_t color = 0;
};

struct QuestState {
  uint8_t type = 0;       // 0 kills, 1 ore, 2 elite, 3 relic
  uint8_t progress = 0;
  uint8_t target = 5;
  uint16_t rewardGold = 12;
  uint16_t rewardXp = 18;
  uint32_t seed = 0;
};

struct MetaState {
  uint8_t heroClass = CLASS_HUNTER;
  uint8_t activeSkill = 0;
  uint8_t enchant = 0;
  uint16_t clanRep = 0;
  uint16_t relics = 0;
  uint32_t fame = 0;
};

struct __attribute__((packed)) HunterSave {
  uint32_t magic;
  uint16_t version;
  uint16_t crc;
  float x;
  float y;
  uint8_t level;
  uint16_t xp;
  uint16_t gold;
  uint16_t ore;
  uint8_t potions;
  uint8_t skill;
  GearItem bag[4];
  GearItem gear[GEAR_COUNT];
  QuestState quest;
};

struct __attribute__((packed)) SlimeBrain {
  int8_t enemyW[WH_SLIME_ENEMY_ACTIONS][WH_SLIME_INPUTS];
  uint8_t enemyTrace[WH_SLIME_ENEMY_ACTIONS][WH_SLIME_INPUTS];
  int8_t autoW[WH_SLIME_AUTO_ACTIONS][WH_SLIME_INPUTS];
  uint8_t autoTrace[WH_SLIME_AUTO_ACTIONS][WH_SLIME_INPUTS];
  int8_t piston[WH_SLIME_PISTON];
  uint8_t pistonPos;
  uint32_t updates;
  uint16_t cleanups;
  uint16_t quantized;
};

struct __attribute__((packed)) SlimeSave {
  uint32_t magic;
  uint16_t version;
  uint16_t crc;
  SlimeBrain brain;
};

struct __attribute__((packed)) MetaSave {
  uint32_t magic;
  uint16_t version;
  uint16_t crc;
  MetaState meta;
};

RemoteJobState currentJob;
RemoteJobState staleJob;
HunterState hunter;
Enemy enemies[WH_ENEMY_COUNT];
WorldObject objects[WH_OBJECT_COUNT];
Particle particles[WH_PARTICLE_COUNT];
FloatText floatTexts[WH_FLOAT_COUNT];
QuestState quest;
GearItem bag[4];
GearItem gear[GEAR_COUNT];
SlimeBrain slime;
MetaState meta;

bool colonyReady = false;
bool buzzSeen = false;
uint32_t lastBuzzMs = 0;
uint32_t lastJobRxMs = 0;
uint32_t lastCleanJobMs = 0;
uint32_t jobSeq = 0;
float jobGapEmaMs = 1300.0f;
uint32_t lastJobGapStartMs = 0;
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
char lastTailLine[48] = "shadow idle";
char statusLine[48] = "BOOT";

float imuBiasGx = 0, imuBiasGy = 0, imuBiasGz = 0;
float imuNeutralAx = 0, imuNeutralAy = 0, imuNeutralAz = 1;
float imuRoll = 0, imuPitch = 0;
float imuShock = 0, imuLoss = 0, imuPredShock = 1;
bool imuReady = false;
float imuManualIntent = 0;
float imuMoveX = 0;
float imuMoveY = 0;
uint32_t imuPoseSettleUntilMs = 0;
bool imuPoseAdaptive = true;
float camX = WH_TOWN_X;
float camY = WH_TOWN_Y;
uint32_t worldSeed = 0xA96A5EEDUL;
float gameTime = 0.0f;
float screenShakeT = 0.0f;

uint32_t lastInputMs = 0;
uint32_t lastFrameMs = 0;
uint32_t lastDrawMs = 0;
uint32_t lastHbMs = 0;
uint32_t lastEntropyMs = 0;
uint32_t lastTailEventMs = 0;
uint32_t lastDiagMs = 0;
bool inMenu = false;
MenuTab menuTab = TAB_INV;

bool topDownPrev = false;
uint32_t topDownAt = 0;
bool topLongDone = false;
uint8_t topTapCount = 0;
uint32_t topTapWindowMs = 0;
bool saveDirty = false;
bool slimeDirty = false;
bool metaDirty = false;
uint32_t lastSaveMs = 0;
uint32_t lastSlimeSaveMs = 0;
uint32_t lastSlimeCleanupMs = 0;
uint32_t lastMetaSaveMs = 0;

bool irSigilEnabled = false;
uint32_t lastIrMs = 0;
rmt_channel_handle_t irTxChan = NULL;
rmt_encoder_handle_t irCopyEncoder = NULL;
bool irReady = false;

uint8_t brightnessLevels[] = {4, 12, 24, 42, 64, 96, 132, 178, 230};
uint8_t brightnessIdx = 4;

bool topButtonDown();
uint8_t dangerTierAt(float wx, float wy);

float clampf(float v, float lo, float hi) {
  if (!isfinite(v)) return lo;
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
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

uint32_t tileHash(int tx, int ty) {
  return mix32(worldSeed ^ (uint32_t)(tx * 73856093L) ^ (uint32_t)(ty * 19349663L));
}

float dist2f(float ax, float ay, float bx, float by) {
  float dx = ax - bx;
  float dy = ay - by;
  return dx * dx + dy * dy;
}

float curvedControl(float v, float deadzone) {
  float a = fabsf(v);
  if (a <= deadzone) return 0.0f;
  float n = (a - deadzone) / max(0.001f, 1.0f - deadzone);
  n = clampf(n * n * (3.0f - 2.0f * n), 0.0f, 1.0f);
  return v < 0 ? -n : n;
}

float walkRunControl(float v) {
  float a = fabsf(v);
  if (a < 0.235f) return 0.0f;
  float out = 0.0f;
  if (a < 0.54f) {
    float n = (a - 0.235f) / 0.305f;
    n = clampf(n * n * (3.0f - 2.0f * n), 0.0f, 1.0f);
    out = 0.13f + n * 0.23f;       // walking band
  } else {
    float n = (a - 0.54f) / 0.46f;
    n = clampf(n * n * (3.0f - 2.0f * n), 0.0f, 1.0f);
    out = 0.36f + n * 0.64f;       // run band
  }
  return v < 0 ? -out : out;
}

void capturePlayPose(const char* label, bool gyroToo = false) {
  float sax = 0, say = 0, saz = 0;
  float sgx = 0, sgy = 0, sgz = 0;
  const int samples = 42;
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
  hunter.vx = 0;
  hunter.vy = 0;
  imuPoseSettleUntilMs = millis() + 2600UL;
  if (label) snprintf(statusLine, sizeof(statusLine), "%s", label);
}

bool nearTown(float radius = 34.0f) {
  return dist2f(hunter.x, hunter.y, WH_TOWN_X, WH_TOWN_Y) <= radius * radius;
}

int worldToScreenX(float wx) {
  int sx = (int)roundf(wx - camX + screenW * 0.5f);
  if (screenShakeT > 0.0f) sx += ((millis() >> 4) & 1) ? 1 : -1;
  return sx;
}

int worldToScreenY(float wy) {
  int sy = (int)roundf(wy - camY + screenH * 0.5f);
  if (screenShakeT > 0.0f) sy += ((millis() >> 5) & 1) ? 1 : -1;
  return sy;
}

bool onScreenWorld(float wx, float wy, int pad = 10) {
  int sx = worldToScreenX(wx);
  int sy = worldToScreenY(wy);
  return sx >= -pad && sx <= screenW + pad && sy >= -pad && sy <= screenH + pad;
}

void addParticle(float x, float y, float vx, float vy, float ttl, uint16_t color) {
  for (int i = 0; i < WH_PARTICLE_COUNT; ++i) {
    if (particles[i].active) continue;
    particles[i].active = true;
    particles[i].x = x;
    particles[i].y = y;
    particles[i].vx = vx;
    particles[i].vy = vy;
    particles[i].t = 0;
    particles[i].ttl = ttl;
    particles[i].color = color;
    return;
  }
}

void addBurst(float x, float y, uint16_t color, uint8_t count, float speed) {
  for (uint8_t i = 0; i < count; ++i) {
    float a = ((float)(esp_random() % 6283)) * 0.001f;
    float s = speed * (0.35f + (float)(esp_random() % 100) * 0.010f);
    addParticle(x, y, cosf(a) * s, sinf(a) * s, 0.22f + (float)(esp_random() % 120) * 0.001f, color);
  }
}

void addFloatText(float x, float y, const char* text, uint16_t color) {
  for (int i = 0; i < WH_FLOAT_COUNT; ++i) {
    if (floatTexts[i].active) continue;
    floatTexts[i].active = true;
    floatTexts[i].x = x;
    floatTexts[i].y = y;
    floatTexts[i].t = 0;
    floatTexts[i].ttl = 0.72f;
    snprintf(floatTexts[i].text, sizeof(floatTexts[i].text), "%s", text ? text : "");
    floatTexts[i].color = color;
    return;
  }
}

void updateFx(float dt) {
  screenShakeT = max(0.0f, screenShakeT - dt);
  for (int i = 0; i < WH_PARTICLE_COUNT; ++i) {
    if (!particles[i].active) continue;
    particles[i].t += dt;
    if (particles[i].t >= particles[i].ttl) {
      particles[i].active = false;
      continue;
    }
    particles[i].x += particles[i].vx * dt;
    particles[i].y += particles[i].vy * dt;
    particles[i].vx *= 0.90f;
    particles[i].vy *= 0.90f;
  }
  for (int i = 0; i < WH_FLOAT_COUNT; ++i) {
    if (!floatTexts[i].active) continue;
    floatTexts[i].t += dt;
    floatTexts[i].y -= dt * 18.0f;
    if (floatTexts[i].t >= floatTexts[i].ttl) floatTexts[i].active = false;
  }
}

uint16_t workerId() {
  if (workerIdCache) return workerIdCache;
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  uint16_t v = ((uint16_t)mac[4] << 8) | mac[5];
  if (!v) v = 0xA96A;
  workerIdCache = v;
  return v;
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
    if (b == 0) { bits += 8; continue; }
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

const char* phaseName(uint8_t phase) {
  if (phase == 2) return "WAKE";
  if (phase == 3) return "MIRROR";
  return "SCOUT";
}

const char* tabName(MenuTab t) {
  switch (t) {
    case TAB_INV: return "INV";
    case TAB_SKILL: return "SKILL";
    case TAB_CLASS: return "CLASS";
    case TAB_QUEST: return "QUEST";
    case TAB_TRADE: return "TRADE";
    case TAB_NPC: return "NPC";
    case TAB_MINER: return "MINER";
    case TAB_SHADOW: return "SHADOW";
    case TAB_SETTINGS: return "SET";
    default: return "?";
  }
}

const char* className(uint8_t c) {
  switch (c % CLASS_COUNT) {
    case CLASS_KNIGHT: return "KNIGHT";
    case CLASS_MYSTIC: return "MYSTIC";
    case CLASS_RANGER: return "RANGER";
    default: return "HUNTER";
  }
}

const char* zoneName(uint8_t tier) {
  if (tier >= 7) return "ABYSS";
  if (tier >= 5) return "WAR";
  if (tier >= 3) return "RUIN";
  if (tier >= 1) return "WILD";
  return "HAVEN";
}

const char* gearGradeName(int score) {
  if (score >= 245) return "S";
  if (score >= 185) return "A";
  if (score >= 132) return "B";
  if (score >= 82) return "C";
  if (score >= 38) return "D";
  return "NG";
}

int classHpBonus() {
  switch (meta.heroClass % CLASS_COUNT) {
    case CLASS_KNIGHT: return 34;
    case CLASS_MYSTIC: return -8;
    case CLASS_RANGER: return 4;
    default: return 12;
  }
}

int classFocusBonus() {
  switch (meta.heroClass % CLASS_COUNT) {
    case CLASS_MYSTIC: return 38;
    case CLASS_RANGER: return 12;
    case CLASS_KNIGHT: return -6;
    default: return 8;
  }
}

int classAtkBonus() {
  switch (meta.heroClass % CLASS_COUNT) {
    case CLASS_RANGER: return 5;
    case CLASS_MYSTIC: return 2;
    case CLASS_KNIGHT: return 1;
    default: return 3;
  }
}

float classSpeedMul() {
  switch (meta.heroClass % CLASS_COUNT) {
    case CLASS_RANGER: return 1.10f;
    case CLASS_KNIGHT: return 0.92f;
    default: return 1.0f;
  }
}

const char* gearSlotName(GearSlot s) {
  switch (s) {
    case GEAR_BLADE: return "BLADE";
    case GEAR_COAT: return "COAT";
    case GEAR_SEAL: return "SEAL";
    default: return "---";
  }
}

uint16_t rarityColor(uint8_t rarity) {
  switch (rarity) {
    case 1: return canvas.color565(74, 196, 116);
    case 2: return canvas.color565(72, 150, 245);
    case 3: return canvas.color565(192, 96, 255);
    default: return canvas.color565(188, 176, 144);
  }
}

int itemScore(const GearItem& item) {
  if (!item.active) return 0;
  return item.atk * 9 + item.hp * 2 + item.focus * 5 + item.rarity * 18;
}

void clearItem(GearItem& item) {
  item.active = false;
  item.slot = GEAR_BLADE;
  item.rarity = 0;
  item.atk = 0;
  item.hp = 0;
  item.focus = 0;
  item.name[0] = '\0';
}

void recalcHunterStats() {
  int oldMax = (int)hunter.maxHp;
  hunter.gearAtk = 0;
  hunter.gearHp = 0;
  hunter.gearFocus = 0;
  hunter.gearScore = 0;
  for (int i = 0; i < GEAR_COUNT; ++i) {
    if (!gear[i].active) continue;
    hunter.gearAtk += gear[i].atk;
    hunter.gearHp += gear[i].hp;
    hunter.gearFocus += gear[i].focus;
    hunter.gearScore += itemScore(gear[i]);
  }
  hunter.maxHp = 100.0f + hunter.level * 8.0f + hunter.gearHp + classHpBonus() + meta.enchant * 3;
  if ((int)hunter.maxHp > oldMax) hunter.hp += (float)((int)hunter.maxHp - oldMax);
  hunter.hp = clampf(hunter.hp, 0.0f, hunter.maxHp);
  hunter.focus = clampf(hunter.focus, 0.0f, 100.0f + hunter.gearFocus + classFocusBonus());
}

uint16_t crc16Bytes(const uint8_t* data, size_t len) {
  uint16_t crc = 0xA96A;
  for (size_t i = 0; i < len; ++i) {
    crc ^= (uint16_t)data[i] << 8;
    for (int b = 0; b < 8; ++b) {
      crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
  }
  return crc;
}

int8_t clampI8(int v) {
  if (v < -127) return -127;
  if (v > 127) return 127;
  return (int8_t)v;
}

int8_t feat64(float v) {
  return clampI8((int)roundf(clampf(v, -64.0f, 64.0f)));
}

uint8_t slimeBondFromError(int16_t error) {
  float e = min(96.0f, fabsf((float)error)) / 48.0f;
  return (uint8_t)clampf(255.0f * expf(-e), 28.0f, 255.0f);
}

int8_t slimeQuantizeWeight(int v, uint8_t trace) {
  v = clampI8(v);
  if (trace >= 178) return (int8_t)v;
  if (trace >= 76) return (int8_t)((v / 2) * 2);
  return (int8_t)((v / 8) * 8);
}

void slimePistonPush(int8_t item) {
  slime.piston[slime.pistonPos % WH_SLIME_PISTON] = item;
  slime.pistonPos = (uint8_t)((slime.pistonPos + 1) % WH_SLIME_PISTON);
}

uint8_t slimeAvgTrace() {
  uint32_t sum = 0;
  uint16_t n = 0;
  for (int a = 0; a < WH_SLIME_ENEMY_ACTIONS; ++a) {
    for (int i = 0; i < WH_SLIME_INPUTS; ++i) { sum += slime.enemyTrace[a][i]; n++; }
  }
  for (int a = 0; a < WH_SLIME_AUTO_ACTIONS; ++a) {
    for (int i = 0; i < WH_SLIME_INPUTS; ++i) { sum += slime.autoTrace[a][i]; n++; }
  }
  return n ? (uint8_t)(sum / n) : 0;
}

void slimeCleanup() {
  uint32_t now = millis();
  if (now - lastSlimeCleanupMs < 18000UL) return;
  lastSlimeCleanupMs = now;
  for (int a = 0; a < WH_SLIME_ENEMY_ACTIONS; ++a) {
    for (int i = 0; i < WH_SLIME_INPUTS; ++i) {
      uint8_t& tr = slime.enemyTrace[a][i];
      tr = (uint8_t)((tr * 247U) >> 8);
      if (tr < 44 && abs(slime.enemyW[a][i]) < 7) {
        slime.enemyW[a][i] = 0;
        slime.cleanups++;
      }
    }
  }
  for (int a = 0; a < WH_SLIME_AUTO_ACTIONS; ++a) {
    for (int i = 0; i < WH_SLIME_INPUTS; ++i) {
      uint8_t& tr = slime.autoTrace[a][i];
      tr = (uint8_t)((tr * 247U) >> 8);
      if (tr < 44 && abs(slime.autoW[a][i]) < 7) {
        slime.autoW[a][i] = 0;
        slime.cleanups++;
      }
    }
  }
}

void slimeLearnMatrix(int8_t* weights, uint8_t* traces, uint8_t action, uint8_t actions, const int8_t feat[WH_SLIME_INPUTS], int8_t reward) {
  if (action >= actions) return;
  int base = action * WH_SLIME_INPUTS;
  int16_t error = reward >= 0 ? max(0, 32 - (int)reward) : 64 + abs((int)reward);
  uint8_t bond = slimeBondFromError(error);
  for (int i = 0; i < WH_SLIME_INPUTS; ++i) {
    uint8_t old = traces[base + i];
    traces[base + i] = (uint8_t)((old * 230U + bond * 26U) >> 8);
    int delta = ((int)feat[i] * (int)reward) / 384;
    weights[base + i] = slimeQuantizeWeight((int)weights[base + i] + delta, traces[base + i]);
  }
  slime.updates++;
  slime.quantized += (bond < 96) ? 1 : 0;
  slimePistonPush(reward);
  slimeDirty = true;
}

void slimeLearnEnemy(uint8_t action, const int8_t feat[WH_SLIME_INPUTS], int8_t reward) {
  slimeLearnMatrix(&slime.enemyW[0][0], &slime.enemyTrace[0][0], action, WH_SLIME_ENEMY_ACTIONS, feat, reward);
}

void slimeLearnAuto(uint8_t action, const int8_t feat[WH_SLIME_INPUTS], int8_t reward) {
  slimeLearnMatrix(&slime.autoW[0][0], &slime.autoTrace[0][0], action, WH_SLIME_AUTO_ACTIONS, feat, reward);
}

uint8_t slimeChoose(const int8_t* weights, const uint8_t* traces, uint8_t actions, const int8_t feat[WH_SLIME_INPUTS], uint32_t salt) {
  int32_t bestScore = INT32_MIN;
  uint8_t best = 0;
  for (uint8_t a = 0; a < actions; ++a) {
    int base = a * WH_SLIME_INPUTS;
    int32_t score = 0;
    uint16_t traceSum = 0;
    for (int i = 0; i < WH_SLIME_INPUTS; ++i) {
      score += (int16_t)weights[base + i] * (int16_t)feat[i];
      traceSum += traces[base + i];
    }
    score += (int32_t)(traceSum / WH_SLIME_INPUTS) * 3;
    score += (int32_t)((mix32(salt ^ (uint32_t)a * 0x45D9F3Bu) & 0x1F) - 16) * 7;
    if (score > bestScore) {
      bestScore = score;
      best = a;
    }
  }
  if ((mix32(salt ^ slime.updates) % 100) < 3) best = (uint8_t)(mix32(salt ^ 0xB17E) % actions);
  return best;
}

void initSlimeBrain() {
  memset(&slime, 0, sizeof(slime));
  for (int a = 0; a < WH_SLIME_ENEMY_ACTIONS; ++a) {
    for (int i = 0; i < WH_SLIME_INPUTS; ++i) slime.enemyTrace[a][i] = 160;
  }
  for (int a = 0; a < WH_SLIME_AUTO_ACTIONS; ++a) {
    for (int i = 0; i < WH_SLIME_INPUTS; ++i) slime.autoTrace[a][i] = 160;
  }

  const int8_t enemySeed[WH_SLIME_ENEMY_ACTIONS][WH_SLIME_INPUTS] = {
    { 10, 16, -8, -8,  2,  5 },  // chase
    {  2, 10, -4, -2,  5, 12 },  // flank left
    {  2, 10, -4, -2,  5, 12 },  // flank right
    { -8,  4, 18,-20,  3,-10 }   // back off / lure
  };
  const int8_t autoSeed[WH_SLIME_AUTO_ACTIONS][WH_SLIME_INPUTS] = {
    {  4, 30,  8, -6, 16, -6 },  // retreat
    {  0,  8, 30, -8, -6,  2 },  // trade
    {  2, -4,  6, 26, -8, 14 },  // scavenge
    {  3,-16, -8, -4, 24, 16 }   // hunt
  };
  memcpy(slime.enemyW, enemySeed, sizeof(enemySeed));
  memcpy(slime.autoW, autoSeed, sizeof(autoSeed));
}

void markSaveDirty() {
  saveDirty = true;
}

void markMetaDirty() {
  metaDirty = true;
}

void rollQuest(uint32_t seed = 0) {
  if (!seed) seed = esp_random() ^ millis() ^ worldSeed;
  quest.seed = seed;
  uint32_t r = mix32(seed);
  quest.type = (uint8_t)(r % 4);
  quest.progress = 0;
  if (quest.type == 2) quest.target = (uint8_t)(1 + (r % 2));
  else if (quest.type == 3) quest.target = (uint8_t)(2 + ((r >> 5) % 3));
  else quest.target = (uint8_t)(4 + ((r >> 4) % 5));
  quest.rewardGold = 8 + hunter.level * 3 + quest.target * 2 + quest.type * 8 + dangerTierAt(hunter.x, hunter.y);
  quest.rewardXp = 12 + hunter.level * 5 + quest.target * 3 + quest.type * 10 + dangerTierAt(hunter.x, hunter.y) * 2;
}

const char* questName() {
  if (quest.type == 1) return "ORE";
  if (quest.type == 2) return "ELT";
  if (quest.type == 3) return "REL";
  return "KILL";
}

void checkLevelUp() {
  while (hunter.xp >= hunter.level * 36) {
    hunter.xp -= hunter.level * 36;
    hunter.level++;
    recalcHunterStats();
    hunter.hp = hunter.maxHp;
    snprintf(statusLine, sizeof(statusLine), "LEVEL %u", (unsigned)hunter.level);
    addFloatText(hunter.x, hunter.y - 16.0f, "LEVEL", canvas.color565(168, 220, 255));
    addBurst(hunter.x, hunter.y, canvas.color565(124, 200, 255), 12, 42.0f);
    markSaveDirty();
  }
}

void creditQuest(uint8_t type, uint8_t amount) {
  if (quest.target == 0) rollQuest();
  if (quest.type != type || quest.progress >= quest.target) return;
  uint8_t old = quest.progress;
  quest.progress = min((uint8_t)(quest.progress + amount), quest.target);
  if (quest.progress != old) markSaveDirty();
  if (quest.progress >= quest.target) {
    hunter.gold += quest.rewardGold;
    hunter.xp += quest.rewardXp;
    meta.clanRep = min((uint16_t)9999, (uint16_t)(meta.clanRep + 1 + quest.type));
    meta.fame += 3 + quest.type * 2;
    markMetaDirty();
    snprintf(statusLine, sizeof(statusLine), "BOUNTY %s", questName());
    addFloatText(hunter.x, hunter.y - 18.0f, "BOUNTY", canvas.color565(255, 208, 92));
    addBurst(hunter.x, hunter.y, canvas.color565(255, 190, 80), 14, 48.0f);
    checkLevelUp();
    rollQuest(mix32(quest.seed ^ millis() ^ hunter.gold));
    markSaveDirty();
  }
}

void saveHunterState(bool force) {
  uint32_t now = millis();
  static float lastSavedX = WH_TOWN_X;
  static float lastSavedY = WH_TOWN_Y;
  if (!saveDirty && dist2f(lastSavedX, lastSavedY, hunter.x, hunter.y) > 96.0f * 96.0f) saveDirty = true;
  if (!force && (!saveDirty || now - lastSaveMs < 6500UL)) return;
  HunterSave s = {};
  s.magic = WH_SAVE_MAGIC;
  s.version = WH_SAVE_VERSION;
  s.x = hunter.x;
  s.y = hunter.y;
  s.level = hunter.level;
  s.xp = hunter.xp;
  s.gold = hunter.gold;
  s.ore = hunter.ore;
  s.potions = hunter.potions;
  s.skill = hunter.skill;
  for (int i = 0; i < 4; ++i) s.bag[i] = bag[i];
  for (int i = 0; i < GEAR_COUNT; ++i) s.gear[i] = gear[i];
  s.quest = quest;
  s.crc = 0;
  s.crc = crc16Bytes((const uint8_t*)&s, sizeof(s));
  File f = LittleFS.open(WH_SAVE_PATH, "w");
  if (f) {
    f.write((const uint8_t*)&s, sizeof(s));
    f.close();
    saveDirty = false;
    lastSaveMs = now;
    lastSavedX = hunter.x;
    lastSavedY = hunter.y;
  }
}

bool loadHunterState() {
  File f = LittleFS.open(WH_SAVE_PATH, "r");
  if (!f) return false;
  if (f.size() != sizeof(HunterSave)) {
    f.close();
    return false;
  }
  HunterSave s = {};
  size_t n = f.read((uint8_t*)&s, sizeof(s));
  f.close();
  if (n != sizeof(s) || s.magic != WH_SAVE_MAGIC || s.version != WH_SAVE_VERSION) return false;
  uint16_t got = s.crc;
  s.crc = 0;
  if (crc16Bytes((const uint8_t*)&s, sizeof(s)) != got) return false;

  hunter.x = clampf(s.x, 24.0f, WH_WORLD_LIMIT - 24.0f);
  hunter.y = clampf(s.y, 24.0f, WH_WORLD_LIMIT - 24.0f);
  hunter.level = max((uint8_t)1, s.level);
  hunter.xp = s.xp;
  hunter.gold = s.gold;
  hunter.ore = s.ore;
  hunter.potions = s.potions;
  hunter.skill = max((uint8_t)1, s.skill);
  for (int i = 0; i < 4; ++i) bag[i] = s.bag[i];
  for (int i = 0; i < GEAR_COUNT; ++i) gear[i] = s.gear[i];
  quest = s.quest;
  if (quest.target == 0 || quest.target > 12) rollQuest(mix32(worldSeed ^ hunter.gold));
  recalcHunterStats();
  camX = hunter.x;
  camY = hunter.y;
  saveDirty = false;
  snprintf(statusLine, sizeof(statusLine), "SAVE LOAD");
  return true;
}

void saveSlimeState(bool force) {
  uint32_t now = millis();
  if (!force && (!slimeDirty || now - lastSlimeSaveMs < 9000UL)) return;
  SlimeSave s = {};
  s.magic = WH_SLIME_MAGIC;
  s.version = WH_SLIME_VERSION;
  s.brain = slime;
  s.crc = 0;
  s.crc = crc16Bytes((const uint8_t*)&s, sizeof(s));
  File f = LittleFS.open(WH_SLIME_SAVE_PATH, "w");
  if (f) {
    f.write((const uint8_t*)&s, sizeof(s));
    f.close();
    slimeDirty = false;
    lastSlimeSaveMs = now;
  }
}

bool loadSlimeState() {
  File f = LittleFS.open(WH_SLIME_SAVE_PATH, "r");
  if (!f) return false;
  if (f.size() != sizeof(SlimeSave)) {
    f.close();
    return false;
  }
  SlimeSave s = {};
  size_t n = f.read((uint8_t*)&s, sizeof(s));
  f.close();
  if (n != sizeof(s) || s.magic != WH_SLIME_MAGIC || s.version != WH_SLIME_VERSION) return false;
  uint16_t got = s.crc;
  s.crc = 0;
  if (crc16Bytes((const uint8_t*)&s, sizeof(s)) != got) return false;
  slime = s.brain;
  slimeDirty = false;
  return true;
}

void saveMetaState(bool force) {
  uint32_t now = millis();
  if (!force && (!metaDirty || now - lastMetaSaveMs < 7000UL)) return;
  MetaSave s = {};
  s.magic = WH_META_MAGIC;
  s.version = WH_META_VERSION;
  s.meta = meta;
  s.crc = 0;
  s.crc = crc16Bytes((const uint8_t*)&s, sizeof(s));
  File f = LittleFS.open(WH_META_SAVE_PATH, "w");
  if (f) {
    f.write((const uint8_t*)&s, sizeof(s));
    f.close();
    metaDirty = false;
    lastMetaSaveMs = now;
  }
}

bool loadMetaState() {
  File f = LittleFS.open(WH_META_SAVE_PATH, "r");
  if (!f) return false;
  if (f.size() != sizeof(MetaSave)) {
    f.close();
    return false;
  }
  MetaSave s = {};
  size_t n = f.read((uint8_t*)&s, sizeof(s));
  f.close();
  if (n != sizeof(s) || s.magic != WH_META_MAGIC || s.version != WH_META_VERSION) return false;
  uint16_t got = s.crc;
  s.crc = 0;
  if (crc16Bytes((const uint8_t*)&s, sizeof(s)) != got) return false;
  meta = s.meta;
  meta.heroClass %= CLASS_COUNT;
  meta.enchant = min((uint8_t)12, meta.enchant);
  metaDirty = false;
  return true;
}

GearItem rollLoot(uint8_t enemyKind) {
  GearItem item;
  clearItem(item);
  item.active = true;
  item.slot = (GearSlot)(esp_random() % GEAR_COUNT);
  int roll = (int)(esp_random() % 100) + hunter.level * 3 + enemyKind * 8;
  if (roll > 105) item.rarity = 3;
  else if (roll > 78) item.rarity = 2;
  else if (roll > 48) item.rarity = 1;
  else item.rarity = 0;
  int base = 4 + hunter.level * 2 + item.rarity * 5 + enemyKind * 2 + dangerTierAt(hunter.x, hunter.y);
  if (item.slot == GEAR_BLADE) {
    item.atk = 2 + base;
    snprintf(item.name, sizeof(item.name), "BLADE+%d", item.atk);
  } else if (item.slot == GEAR_COAT) {
    item.hp = 12 + base * 3;
    snprintf(item.name, sizeof(item.name), "COAT+%d", item.hp);
  } else {
    item.atk = 1 + base / 3;
    item.hp = 5 + base;
    item.focus = 3 + item.rarity * 3;
    snprintf(item.name, sizeof(item.name), "SEAL R%u", (unsigned)item.rarity);
  }
  return item;
}

bool addLootToBag(const GearItem& item) {
  for (int i = 0; i < 4; ++i) {
    if (!bag[i].active) {
      bag[i] = item;
      snprintf(statusLine, sizeof(statusLine), "LOOT %s", gearSlotName(item.slot));
      markSaveDirty();
      return true;
    }
  }
  hunter.gold += max(2, itemScore(item) / 12);
  snprintf(statusLine, sizeof(statusLine), "BAG FULL GOLD");
  markSaveDirty();
  return false;
}

bool equipBestFromBag() {
  int best = -1;
  int gain = 0;
  for (int i = 0; i < 4; ++i) {
    if (!bag[i].active) continue;
    int cur = gear[bag[i].slot].active ? itemScore(gear[bag[i].slot]) : 0;
    int g = itemScore(bag[i]) - cur;
    if (g > gain) { gain = g; best = i; }
  }
  if (best < 0) return false;
  GearSlot slot = bag[best].slot;
  GearItem old = gear[slot];
  gear[slot] = bag[best];
  if (old.active) bag[best] = old;
  else clearItem(bag[best]);
  recalcHunterStats();
  snprintf(statusLine, sizeof(statusLine), "EQUIP %s", gearSlotName(slot));
  M5.Speaker.tone(1280 + gear[slot].rarity * 120, 34);
  markSaveDirty();
  return true;
}

uint8_t currentSector() {
  int sx = ((int)(hunter.x / 192.0f)) & 3;
  int sy = ((int)(hunter.y / 192.0f)) & 1;
  return (uint8_t)(sx + sy * 4);
}

int8_t currentRssi() {
  return (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : -127;
}

void logTailJson(uint8_t phase, uint16_t zbits, uint32_t nonce, uint8_t reason, bool targetPass) {
  File f = LittleFS.open(WH_LOG_PATH, "a");
  if (!f) return;
  uint32_t now = millis();
  f.printf("{\"ms\":%lu,\"phase\":\"%s\",\"zbits\":%u,\"strategy\":\"%s\",\"lane\":\"%s\",\"sector\":%u,\"worker\":%u,\"job_seq\":%lu,\"job_age_ms\":%lu,\"ttn_ms\":%ld,\"after_clean_ms\":%lu,\"pool_rejects\":%lu,\"reason\":%u,\"nonce\":%lu,\"target_pass\":%u}\n",
           (unsigned long)now,
           phaseName(phase),
           (unsigned)zbits,
           phase == 3 ? "pool_mirror" : (phase == 2 ? "shadow" : "observe"),
           hunter.autoPilot ? "idle_ai" : "manual",
           (unsigned)currentSector(),
           (unsigned)workerId(),
           (unsigned long)jobSeq,
           currentJob.rxMs ? (unsigned long)(now - currentJob.rxMs) : 0UL,
           (long)max(0.0f, jobGapEmaMs - (float)(now - lastJobRxMs)),
           lastCleanJobMs ? (unsigned long)(now - lastCleanJobMs) : 0UL,
           (unsigned long)buzzRejects,
           (unsigned)reason,
           (unsigned long)nonce,
           targetPass ? 1 : 0);
  f.close();
}

void cleanupTailLog() {
  static uint32_t lastCheck = 0;
  uint32_t now = millis();
  if (now - lastCheck < 30000UL) return;
  lastCheck = now;
  File f = LittleFS.open(WH_LOG_PATH, "r");
  if (!f) return;
  size_t sz = f.size();
  f.close();
  if (sz <= WH_LOG_MAX_BYTES) return;
  LittleFS.remove("/witch_tail.old");
  LittleFS.rename(WH_LOG_PATH, "/witch_tail.old");
  File nf = LittleFS.open(WH_LOG_PATH, "w");
  if (nf) {
    nf.printf("{\"ms\":%lu,\"event\":\"corpus_rotated\",\"old_bytes\":%lu}\n",
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
    Serial.printf("[WH/ESP] tx fail tag=%s err=%d\n", tag ? tag : "-", (int)err);
  }
  return err;
}

void sendTailPacket(uint8_t phase, uint8_t strategy, uint8_t lane, uint16_t zbits,
                    uint32_t nonce, uint8_t reason, bool targetPass, const uint8_t hashTail[4]) {
  uint32_t now = millis();
  if (phase != 3 && now - lastTailEventMs < WH_TAIL_EVENT_MS) return;
  lastTailEventMs = now;

  RejectTailPacket rt = {};
  rt.magic[0] = 'R'; rt.magic[1] = 'T';
  rt.version = 1;
  rt.phase = phase;
  rt.strategy = strategy;
  rt.lane = lane;
  rt.sector = currentSector();
  snprintf(rt.nodeId, sizeof(rt.nodeId), "%s", WH_NODE_ID);
  rt.seq = ++tailSeq;
  rt.worker_id = workerId();
  rt.zbits = zbits;
  rt.targetBits = targetBitsNow;
  rt.jobSeq = jobSeq;
  rt.jobAgeMs = currentJob.rxMs ? now - currentJob.rxMs : 0;
  rt.timeToNextJobMs = (int32_t)max(0.0f, jobGapEmaMs - (float)(now - lastJobRxMs));
  rt.timeAfterCleanJobMs = lastCleanJobMs ? now - lastCleanJobMs : 0;
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
  snprintf(lastTailLine, sizeof(lastTailLine), "%s z%u n%08lX", phaseName(phase), (unsigned)zbits, (unsigned long)nonce);
  logTailJson(phase, zbits, nonce, reason, targetPass);
}

void sendEvent(uint8_t eventType, const char* kind, uint8_t confidence, uint8_t urgency,
               int16_t a, int16_t b, int16_t c, int16_t d) {
  JanusEventPacket je = {};
  je.magic[0] = 'J'; je.magic[1] = 'E';
  je.version = 1;
  je.eventType = eventType;
  je.nodeRole = 11;
  je.confidence = confidence;
  je.urgency = urgency;
  snprintf(je.nodeId, sizeof(je.nodeId), "%s", WH_NODE_ID);
  snprintf(je.kind, sizeof(je.kind), "%s", kind ? kind : "witch");
  je.seq = ++colonySeq;
  je.uptimeMs = millis();
  je.topicHash = hash16(kind ? kind : "witch");
  je.objectHash = hash16("reject_tail");
  je.capabilities = 0x0008 | 0x0080 | 0x0400 | 0x1000 | 0x2000 | 0x4000 | 0x8000;
  je.valueA_x10 = a;
  je.valueB_x10 = b;
  je.valueC_x10 = c;
  je.valueD_x10 = d;
  je.eventHash = ((uint32_t)je.topicHash << 16) ^ je.objectHash ^ je.seq;
  je.ttlMs = 16000UL;
  sendEspNow("J/E", &je, sizeof(je));
}

void sendHeartbeat() {
  JanusColonyPacket pkt = {};
  memcpy(pkt.magic, "JANUS", 6);
  snprintf(pkt.nodeId, sizeof(pkt.nodeId), "%s", WH_NODE_ID);
  snprintf(pkt.role, sizeof(pkt.role), "%s", "WitchHunt");
  pkt.seq = ++colonySeq;
  pkt.hashRate = observerHashrate;
  pkt.shares = tailEvents;
  pkt.rejects = poolRejectEvents + staleTailEvents;
  pkt.bestBits = bestBits;
  pkt.diff = buzzDiff;
  pkt.targetBits = targetBitsNow;
  pkt.aiBatch = hunter.autoPilot ? 220 : 120;
  pkt.aiHint = irSigilEnabled ? 3 : (hunter.autoPilot ? 2 : 1);
  pkt.jobAgeMs = currentJob.rxMs ? millis() - currentJob.rxMs : 0;
  pkt.rssi = currentRssi();
  pkt.uptime = millis() / 1000UL;
  sendEspNow("HB", &pkt, sizeof(pkt));
}

void sendEntropy() {
  float entropy = clampf((float)bestBits / 32.0f + imuLoss * 0.5f + (float)tailEvents * 0.002f, 0.0f, 4.0f);
  float predictionError = imuLoss + (float)staleTailEvents * 0.01f;
  float syncHint = buzzSeen ? clampf(1.0f - (float)(millis() - lastBuzzMs) / 12000.0f, 0.0f, 1.0f) : 0.0f;
  float fit = clampf((float)observerHashrate / 4500.0f + (hunter.autoPilot ? 0.15f : 0.0f), 0.0f, 2.0f);
  uint8_t sensorFlags = 0x08 | 0x80; // bit3=IMU, bit7=RejectTail observer

  EntropyReport er1 = {};
  er1.magic[0] = 'E'; er1.magic[1] = 'R';
  er1.worker_id = workerId();
  er1.local_entropy = entropy;
  er1.sensor_flags = sensorFlags;
  er1.values[0] = imuRoll;
  er1.values[1] = imuPitch;
  er1.values[2] = imuShock;
  er1.values[3] = predictionError;
  sendEspNow("ER", &er1, sizeof(er1));

  EntropyReportV2 er = {};
  er.magic[0] = 'E'; er.magic[1] = '2';
  er.worker_id = workerId();
  snprintf(er.nodeId, sizeof(er.nodeId), "%s", WH_NODE_ID);
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
  er.values[5] = (float)tailEvents;
  er.values[6] = (float)staleTailEvents;
  er.values[7] = (float)buzzRejects;
  er.uptime_ms = millis();
  sendEspNow("E2", &er, sizeof(er));
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
    sendTailPacket(3, 3, hunter.autoPilot ? 2 : 1, (uint16_t)buzzBestBits, pkt.seq, 4, false, tail);
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
    float gap = (float)(now - lastJobRxMs);
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
  snprintf(statusLine, sizeof(statusLine), "BUZZ JOB z%u", (unsigned)targetBitsNow);
}

void onJanusEvent(const JanusEventPacket& je) {
  if (je.magic[0] != 'J' || je.magic[1] != 'E') return;
  if (je.eventType == 11 || strstr(je.kind, "reject") || strstr(je.kind, "stale")) {
    uint8_t tail[4] = {(uint8_t)je.valueA_x10, (uint8_t)je.valueB_x10, (uint8_t)je.seq, (uint8_t)je.urgency};
    sendTailPacket(3, 3, hunter.autoPilot ? 2 : 1, (uint16_t)max(0, (int)je.valueB_x10), je.seq, 4, false, tail);
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
  Serial.printf("[WH] ESP-NOW ready ch=%u wifi=%d worker=%u\n",
                (unsigned)WiFi.channel(), WiFi.status() == WL_CONNECTED ? 1 : 0, (unsigned)workerId());
}

void calibrateImu(bool force) {
  (void)force;
  capturePlayPose("IMU CAL", true);
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
  float mag = sqrtf(ax * ax + ay * ay + az * az);
  float gyro = sqrtf(gx * gx + gy * gy + gz * gz) * 0.010f;

  // Landscape grip, blue button to the right. The neutral pose is treated as
  // a living center so a slightly crooked hand does not become permanent left.
  float rawSide = -ny;
  float rawFwd = nx;

  if (imuPoseAdaptive && !topButtonDown() && gyro < 0.22f) {
    bool settling = now < imuPoseSettleUntilMs;
    bool quietHand = gyro < 0.075f;
    float driftLimit = (hunter.autoPilot || settling) ? 1.30f : 0.86f;
    bool smallDrift = fabsf(rawSide) < driftLimit && fabsf(rawFwd) < driftLimit;
    bool safeToRecenter = inMenu || hunter.autoPilot || settling || (quietHand && now - lastInputMs > 650UL);
    if (settling || (safeToRecenter && smallDrift)) {
      float beta = settling ? 0.22f : (quietHand ? 0.024f : 0.008f);
      imuNeutralAx = imuNeutralAx * (1.0f - beta) + ax * beta;
      imuNeutralAy = imuNeutralAy * (1.0f - beta) + ay * beta;
      imuNeutralAz = imuNeutralAz * (1.0f - beta) + az * beta;
      nx = ax - imuNeutralAx;
      ny = ay - imuNeutralAy;
      rawSide = -ny;
      rawFwd = nx;
    }
  }

  float targetRoll = clampf(rawSide * 1.26f, -1.0f, 1.0f);
  float targetPitch = clampf(rawFwd * 1.16f, -1.0f, 1.0f);
  if (now < imuPoseSettleUntilMs) {
    targetRoll = 0.0f;
    targetPitch = 0.0f;
  }
  float alpha = 0.075f + min(0.055f, dt * 1.10f);
  imuRoll = imuRoll * (1.0f - alpha) + targetRoll * alpha;
  imuPitch = imuPitch * (1.0f - alpha) + targetPitch * alpha;

  imuShock = mag + gyro;
  imuLoss = fabsf(imuShock - imuPredShock);
  imuPredShock = imuPredShock * 0.968f + imuShock * 0.032f;

  imuMoveX = walkRunControl(imuRoll);
  imuMoveY = walkRunControl(imuPitch);
  imuManualIntent = fabsf(imuMoveX) + fabsf(imuMoveY);
  if (now > imuPoseSettleUntilMs && (imuManualIntent > 0.20f || gyro > 0.38f)) {
    lastInputMs = now;
  }
}

uint8_t dangerTierAt(float wx, float wy) {
  float dx = wx - WH_TOWN_X;
  float dy = wy - WH_TOWN_Y;
  float d = sqrtf(dx * dx + dy * dy);
  return (uint8_t)clampf(floorf(d / 520.0f), 0.0f, 9.0f);
}

void fillEnemySlimeFeatures(const Enemy& e, float d2, int8_t out[WH_SLIME_INPUTS]) {
  float dist = sqrtf(max(0.0f, d2));
  float enemyHp = e.maxHp > 0.0f ? e.hp / e.maxHp : 1.0f;
  float playerHp = hunter.maxHp > 0.0f ? hunter.hp / hunter.maxHp : 1.0f;
  out[0] = 64;
  out[1] = feat64(64.0f - dist * 0.42f);                    // proximity pressure
  out[2] = feat64(64.0f - enemyHp * 128.0f);                 // enemy low hp
  out[3] = feat64(64.0f - playerHp * 128.0f);                // player low hp
  out[4] = feat64((float)dangerTierAt(e.x, e.y) * 14.0f - 42.0f);
  out[5] = e.elite ? 64 : -24;
}

uint8_t chooseEnemySlimeAction(Enemy& e, float d2) {
  fillEnemySlimeFeatures(e, d2, e.aiFeat);
  uint32_t salt = (uint32_t)((int)e.x * 73856093L) ^ (uint32_t)((int)e.y * 19349663L) ^ millis() ^ slime.updates;
  e.aiAction = slimeChoose(&slime.enemyW[0][0], &slime.enemyTrace[0][0], WH_SLIME_ENEMY_ACTIONS, e.aiFeat, salt);
  return e.aiAction;
}

void enemySlimeVector(Enemy& e, float dx, float dy, float d2, float& ox, float& oy) {
  float inv = 1.0f / sqrtf(d2 + 0.001f);
  float tx = dx * inv;
  float ty = dy * inv;
  float px = -ty;
  float py = tx;
  uint8_t action = chooseEnemySlimeAction(e, d2);
  float hpRatio = e.maxHp > 0.0f ? e.hp / e.maxHp : 1.0f;
  if (action == 1) {
    ox = tx * 0.78f + px * 0.46f;
    oy = ty * 0.78f + py * 0.46f;
  } else if (action == 2) {
    ox = tx * 0.78f - px * 0.46f;
    oy = ty * 0.78f - py * 0.46f;
  } else if (action == 3) {
    float wobble = sinf(gameTime * (1.2f + e.kind * 0.2f) + e.x * 0.017f) * 0.34f;
    if (hpRatio < 0.42f && !e.elite) {
      ox = -tx * 0.80f + px * wobble;
      oy = -ty * 0.80f + py * wobble;
    } else {
      ox = tx * 0.44f + px * (0.58f + wobble);
      oy = ty * 0.44f + py * (0.58f + wobble);
    }
  } else {
    ox = tx;
    oy = ty;
  }
  float norm = sqrtf(ox * ox + oy * oy);
  if (norm > 0.001f) {
    ox /= norm;
    oy /= norm;
  }
}

void spawnEnemy(int i) {
  enemies[i].active = true;
  enemies[i].kind = (uint8_t)(esp_random() % 3);
  float a = ((float)(esp_random() % 6283)) * 0.001f;
  float r = 72.0f + (float)(esp_random() % 128);
  enemies[i].x = clampf(hunter.x + cosf(a) * r, 12.0f, WH_WORLD_LIMIT - 12.0f);
  enemies[i].y = clampf(hunter.y + sinf(a) * r, 12.0f, WH_WORLD_LIMIT - 12.0f);
  if (nearTown(82.0f)) {
    enemies[i].x += cosf(a) * 78.0f;
    enemies[i].y += sinf(a) * 78.0f;
  }
  uint8_t tier = dangerTierAt(enemies[i].x, enemies[i].y);
  enemies[i].kind = (uint8_t)((enemies[i].kind + (tier > 3 ? 1 : 0)) % 3);
  enemies[i].elite = ((esp_random() % 100) < (uint32_t)(7 + min((int)hunter.level, 10) + tier * 2));
  enemies[i].maxHp = 18.0f + enemies[i].kind * 11.0f + hunter.level * 2.6f + tier * 6.0f;
  if (enemies[i].elite) enemies[i].maxHp *= 2.35f;
  enemies[i].hp = enemies[i].maxHp;
  enemies[i].hitT = 0;
  enemies[i].attackCd = 0.35f + (float)(esp_random() % 90) * 0.01f;
  enemies[i].learnCd = 0.2f + (float)(esp_random() % 120) * 0.01f;
  enemies[i].aiAction = 0;
  memset(enemies[i].aiFeat, 0, sizeof(enemies[i].aiFeat));
}

void spawnWorldObject(int i) {
  objects[i].active = true;
  uint32_t r0 = esp_random();
  uint8_t roll = r0 % 100;
  if (quest.type == 3 && roll < 34) objects[i].type = OBJ_RELIC;
  else objects[i].type = roll < 52 ? OBJ_ORE : (roll < 78 ? OBJ_CHEST : (roll < 92 ? OBJ_SHRINE : OBJ_RELIC));
  objects[i].rarity = (uint8_t)(mix32(r0 ^ hunter.level) % 4);
  float a = ((float)(mix32(r0) % 6283)) * 0.001f;
  float r = 42.0f + (float)(mix32(r0 ^ 0xB71D) % 190);
  objects[i].x = clampf(hunter.x + cosf(a) * r, 16.0f, WH_WORLD_LIMIT - 16.0f);
  objects[i].y = clampf(hunter.y + sinf(a) * r, 16.0f, WH_WORLD_LIMIT - 16.0f);
  if (dist2f(objects[i].x, objects[i].y, WH_TOWN_X, WH_TOWN_Y) < 2800.0f) {
    objects[i].x += cosf(a) * 70.0f;
    objects[i].y += sinf(a) * 70.0f;
  }
  uint8_t tier = dangerTierAt(objects[i].x, objects[i].y);
  if (tier > 2 && objects[i].rarity < 3 && (mix32(r0 ^ tier) & 3) == 0) objects[i].rarity++;
}

void ensureWorldPopulation() {
  for (int i = 0; i < WH_ENEMY_COUNT; ++i) {
    if (!enemies[i].active || dist2f(enemies[i].x, enemies[i].y, hunter.x, hunter.y) > 280.0f * 280.0f) {
      spawnEnemy(i);
    }
  }
  for (int i = 0; i < WH_OBJECT_COUNT; ++i) {
    if (!objects[i].active || dist2f(objects[i].x, objects[i].y, hunter.x, hunter.y) > 320.0f * 320.0f) {
      spawnWorldObject(i);
    }
  }
}

void initGame() {
  hunter.x = WH_TOWN_X + 18.0f;
  hunter.y = WH_TOWN_Y + 10.0f;
  camX = hunter.x;
  camY = hunter.y;
  for (int i = 0; i < 4; ++i) clearItem(bag[i]);
  for (int i = 0; i < GEAR_COUNT; ++i) clearItem(gear[i]);
  recalcHunterStats();
  rollQuest(worldSeed ^ esp_random());
  for (int i = 0; i < WH_ENEMY_COUNT; ++i) spawnEnemy(i);
  for (int i = 0; i < WH_OBJECT_COUNT; ++i) spawnWorldObject(i);
}

int nearestEnemy() {
  int best = -1;
  float bd = 999999.0f;
  for (int i = 0; i < WH_ENEMY_COUNT; ++i) {
    if (!enemies[i].active) continue;
    float dx = enemies[i].x - hunter.x;
    float dy = enemies[i].y - hunter.y;
    float d = dx * dx + dy * dy;
    if (d < bd) { bd = d; best = i; }
  }
  return best;
}

int nearestObject(WorldObjType prefer = OBJ_NONE, float maxD2 = 9999999.0f) {
  int best = -1;
  float bd = maxD2;
  for (int i = 0; i < WH_OBJECT_COUNT; ++i) {
    if (!objects[i].active) continue;
    if (prefer != OBJ_NONE && objects[i].type != prefer) continue;
    float d = dist2f(objects[i].x, objects[i].y, hunter.x, hunter.y);
    if (d < bd) {
      bd = d;
      best = i;
    }
  }
  return best;
}

void fillAutoSlimeFeatures(int nearestObj, int nearestMob, int8_t out[WH_SLIME_INPUTS]) {
  float hpRatio = hunter.maxHp > 0.0f ? hunter.hp / hunter.maxHp : 1.0f;
  float lootSignal = -32.0f;
  if (nearestObj >= 0) {
    float d = sqrtf(dist2f(objects[nearestObj].x, objects[nearestObj].y, hunter.x, hunter.y));
    lootSignal = 64.0f - d * 0.38f + objects[nearestObj].rarity * 8.0f;
    if (quest.type == 1 && objects[nearestObj].type == OBJ_ORE) lootSignal += 22.0f;
    if (quest.type == 3 && objects[nearestObj].type == OBJ_RELIC) lootSignal += 30.0f;
  }
  float enemySignal = -34.0f;
  if (nearestMob >= 0) {
    float d = sqrtf(dist2f(enemies[nearestMob].x, enemies[nearestMob].y, hunter.x, hunter.y));
    enemySignal = 64.0f - d * 0.34f + (enemies[nearestMob].elite ? 16.0f : 0.0f);
    if (quest.type == 2 && enemies[nearestMob].elite) enemySignal += 26.0f;
    if (quest.type == 0) enemySignal += 12.0f;
  }
  float questSignal = (quest.type == 1 || quest.type == 3) ? lootSignal * 0.65f : enemySignal * 0.65f;
  out[0] = 64;
  out[1] = feat64(64.0f - hpRatio * 128.0f);                 // low hp
  out[2] = feat64((float)((int)hunter.ore - 4) * 14.0f);      // cargo wants town
  out[3] = feat64(lootSignal);
  out[4] = feat64(enemySignal);
  out[5] = feat64(questSignal);
}

uint8_t chooseAutoSlimeAction(int nearestObj, int nearestMob) {
  fillAutoSlimeFeatures(nearestObj, nearestMob, hunter.autoFeat);
  uint32_t salt = millis() ^ ((uint32_t)hunter.gold << 16) ^ ((uint32_t)hunter.ore << 4) ^ slime.updates;
  uint8_t action = slimeChoose(&slime.autoW[0][0], &slime.autoTrace[0][0], WH_SLIME_AUTO_ACTIONS, hunter.autoFeat, salt);
  float hpRatio = hunter.maxHp > 0.0f ? hunter.hp / hunter.maxHp : 1.0f;
  if (hpRatio < 0.24f) action = 0;
  else if (hunter.ore >= 12) action = 1;
  hunter.autoAction = action;
  return action;
}

void hunterAttack() {
  if (hunter.attackCd > 0.0f) return;
  hunter.attackCd = max(0.18f, 0.48f - hunter.skill * 0.035f);
  hunter.slashT = 0.18f;
  int n = nearestEnemy();
  if (n >= 0) {
    float dx = enemies[n].x - hunter.x;
    float dy = enemies[n].y - hunter.y;
    float d2 = dx * dx + dy * dy;
    if (d2 > 1.0f) {
      float inv = 1.0f / sqrtf(d2);
      hunter.faceX = dx * inv;
      hunter.faceY = dy * inv;
    }
    if (d2 < 880.0f) {
      int dmg = hunter.baseAtk + hunter.gearAtk + hunter.skill * 3 + hunter.level + classAtkBonus() + meta.enchant * 2;
      if (meta.heroClass == CLASS_MYSTIC && hunter.focus > 12.0f) {
        hunter.focus = max(0.0f, hunter.focus - 4.0f);
        dmg += 4 + hunter.skill;
      }
      if (hunter.focus > 18.0f && ((esp_random() & 7) == 0)) {
        hunter.focus = max(0.0f, hunter.focus - 10.0f);
        dmg = (int)(dmg * 1.75f);
        snprintf(statusLine, sizeof(statusLine), "FOCUS CRIT");
      }
      enemies[n].hp -= dmg;
      enemies[n].hitT = 0.16f;
      screenShakeT = max(screenShakeT, 0.09f);
      char pop[10];
      snprintf(pop, sizeof(pop), "%d", dmg);
      addFloatText(enemies[n].x, enemies[n].y - 10.0f, pop, canvas.color565(255, 216, 118));
      addBurst(enemies[n].x, enemies[n].y, canvas.color565(255, 156, 78), 5, 34.0f);
      if (enemies[n].hp <= 0) {
        uint8_t deadKind = enemies[n].kind;
        bool wasElite = enemies[n].elite;
        float deadX = enemies[n].x;
        float deadY = enemies[n].y;
        slimeLearnEnemy(enemies[n].aiAction, enemies[n].aiFeat, wasElite ? -38 : -26);
        enemies[n].active = false;
        uint8_t zoneTier = dangerTierAt(deadX, deadY);
        hunter.xp += 8 + deadKind * 4 + zoneTier * 2 + (wasElite ? 22 : 0);
        hunter.gold += 1 + deadKind + zoneTier + (wasElite ? 9 : 0);
        meta.fame += wasElite ? 6 : 1;
        markMetaDirty();
        creditQuest(0, 1);
        if (wasElite) creditQuest(2, 1);
        if (hunter.autoPilot) slimeLearnAuto(hunter.autoAction, hunter.autoFeat, wasElite ? 30 : 20);
        addBurst(deadX, deadY, wasElite ? canvas.color565(202, 110, 255) : canvas.color565(230, 72, 64), wasElite ? 12 : 7, wasElite ? 52.0f : 38.0f);
        addFloatText(deadX, deadY - 14.0f, wasElite ? "ELITE" : "KILL", wasElite ? canvas.color565(220, 160, 255) : canvas.color565(255, 188, 92));
        if ((esp_random() % 100) < (uint32_t)(24 + deadKind * 12 + hunter.skill * 2 + (wasElite ? 35 : 0))) {
          addLootToBag(rollLoot(deadKind));
        } else if ((esp_random() & 31) == 0) {
          hunter.potions = min((uint8_t)9, (uint8_t)(hunter.potions + 1));
          snprintf(statusLine, sizeof(statusLine), "POTION DROP");
          markSaveDirty();
        }
        checkLevelUp();
        markSaveDirty();
      }
    }
  }
  M5.Speaker.tone(640 + hunter.skill * 70, 24);
}

bool interactWorld() {
  if (nearTown(32.0f)) {
    if (hunter.ore > 0) {
      hunter.gold += hunter.ore * 2;
      snprintf(statusLine, sizeof(statusLine), "SOLD ORE +%u", (unsigned)(hunter.ore * 2));
      addFloatText(hunter.x, hunter.y - 12.0f, "SOLD", canvas.color565(246, 190, 74));
      addBurst(hunter.x, hunter.y, canvas.color565(246, 190, 74), 7, 28.0f);
      hunter.ore = 0;
      M5.Speaker.tone(880, 36);
      markSaveDirty();
      if (hunter.autoPilot) slimeLearnAuto(hunter.autoAction, hunter.autoFeat, 18);
      return true;
    }
    if (hunter.gold >= 5 && hunter.potions < 9) {
      hunter.gold -= 5;
      hunter.potions++;
      snprintf(statusLine, sizeof(statusLine), "POTION BOUGHT");
      addFloatText(hunter.x, hunter.y - 12.0f, "POTION", canvas.color565(98, 220, 138));
      M5.Speaker.tone(760, 36);
      markSaveDirty();
      if (hunter.autoPilot) slimeLearnAuto(hunter.autoAction, hunter.autoFeat, 8);
      return true;
    }
    snprintf(statusLine, sizeof(statusLine), "HAVEN MARKET");
    return true;
  }

  int best = -1;
  float bd = 99999.0f;
  for (int i = 0; i < WH_OBJECT_COUNT; ++i) {
    if (!objects[i].active) continue;
    float d = dist2f(objects[i].x, objects[i].y, hunter.x, hunter.y);
    if (d < bd) { bd = d; best = i; }
  }
  if (best < 0 || bd > 22.0f * 22.0f) return false;

  WorldObject& obj = objects[best];
  if (obj.type == OBJ_ORE) {
    uint8_t gain = 1 + obj.rarity;
    hunter.ore += gain;
    hunter.xp += gain;
    creditQuest(1, gain);
    checkLevelUp();
    snprintf(statusLine, sizeof(statusLine), "ORE +%u", (unsigned)gain);
    addFloatText(obj.x, obj.y - 9.0f, "+ORE", canvas.color565(142, 236, 255));
    addBurst(obj.x, obj.y, canvas.color565(104, 222, 255), 7, 34.0f);
    M5.Speaker.tone(520, 28);
    markSaveDirty();
    if (hunter.autoPilot) slimeLearnAuto(hunter.autoAction, hunter.autoFeat, 16);
  } else if (obj.type == OBJ_CHEST) {
    hunter.gold += 3 + obj.rarity * 2;
    addLootToBag(rollLoot(obj.rarity));
    addFloatText(obj.x, obj.y - 9.0f, "LOOT", rarityColor(obj.rarity));
    addBurst(obj.x, obj.y, canvas.color565(250, 196, 84), 10, 42.0f);
    M5.Speaker.tone(1120, 42);
    markSaveDirty();
    if (hunter.autoPilot) slimeLearnAuto(hunter.autoAction, hunter.autoFeat, 22);
  } else if (obj.type == OBJ_SHRINE) {
    hunter.focus = min(100.0f + hunter.gearFocus, hunter.focus + 35.0f);
    hunter.hp = min(hunter.maxHp, hunter.hp + 18.0f);
    snprintf(statusLine, sizeof(statusLine), "SHRINE");
    addFloatText(obj.x, obj.y - 12.0f, "BLESS", canvas.color565(210, 160, 255));
    addBurst(obj.x, obj.y, canvas.color565(172, 126, 255), 11, 30.0f);
    M5.Speaker.tone(1480, 48);
    markSaveDirty();
    if (hunter.autoPilot) slimeLearnAuto(hunter.autoAction, hunter.autoFeat, 12);
  } else if (obj.type == OBJ_RELIC) {
    meta.relics = min((uint16_t)999, (uint16_t)(meta.relics + 1));
    meta.fame += 4 + obj.rarity;
    hunter.xp += 6 + obj.rarity * 4;
    hunter.gold += 2 + obj.rarity;
    creditQuest(3, 1);
    checkLevelUp();
    snprintf(statusLine, sizeof(statusLine), "RELIC +1");
    addFloatText(obj.x, obj.y - 12.0f, "RELIC", canvas.color565(255, 218, 120));
    addBurst(obj.x, obj.y, canvas.color565(255, 198, 82), 12, 38.0f);
    M5.Speaker.tone(1320, 48);
    markSaveDirty();
    markMetaDirty();
    if (hunter.autoPilot) slimeLearnAuto(hunter.autoAction, hunter.autoFeat, 20);
  }
  obj.active = false;
  spawnWorldObject(best);
  return true;
}

void updateGame(float dt) {
  gameTime += dt;
  updateFx(dt);
  hunter.autoPilot = (millis() - lastInputMs > WH_INPUT_IDLE_MS) && !inMenu;
  float mx = 0, my = 0;
  if (hunter.autoPilot) {
    float tx = hunter.x;
    float ty = hunter.y;
    bool hasAutoTarget = false;
    bool interactAtTarget = false;
    float hpRatio = hunter.hp / max(1.0f, hunter.maxHp);
    int obj = nearestObject(OBJ_NONE, 156.0f * 156.0f);
    int n = nearestEnemy();
    uint8_t action = chooseAutoSlimeAction(obj, n);

    if (action == 0 || hpRatio < 0.34f) {
      tx = WH_TOWN_X;
      ty = WH_TOWN_Y;
      hasAutoTarget = true;
      interactAtTarget = true;
      snprintf(statusLine, sizeof(statusLine), "SLIME RETREAT");
    } else if (action == 1 || hunter.ore >= 8) {
      tx = WH_TOWN_X;
      ty = WH_TOWN_Y;
      hasAutoTarget = true;
      interactAtTarget = true;
      snprintf(statusLine, sizeof(statusLine), "SLIME TRADE");
    } else if (action == 2 && obj >= 0) {
      tx = objects[obj].x;
      ty = objects[obj].y;
      hasAutoTarget = true;
      interactAtTarget = true;
      snprintf(statusLine, sizeof(statusLine), "SLIME SCAVENGE");
    } else if (n >= 0) {
      tx = enemies[n].x;
      ty = enemies[n].y;
      hasAutoTarget = true;
      if (dist2f(hunter.x, hunter.y, tx, ty) < 380.0f) hunterAttack();
      snprintf(statusLine, sizeof(statusLine), "SLIME HUNT");
    } else if (obj >= 0) {
      tx = objects[obj].x;
      ty = objects[obj].y;
      hasAutoTarget = true;
      interactAtTarget = true;
      snprintf(statusLine, sizeof(statusLine), "SLIME LOOT");
    }

    if (hasAutoTarget) {
      float dx = tx - hunter.x;
      float dy = ty - hunter.y;
      float d2 = dx * dx + dy * dy;
      float inv = 1.0f / sqrtf(d2 + 0.001f);
      mx = dx * inv;
      my = dy * inv;
      if (interactAtTarget && d2 < 24.0f * 24.0f) interactWorld();
    }
  } else if (!inMenu) {
    mx = imuMoveX * 1.04f;
    my = imuMoveY * 1.08f;
  }

  if (fabsf(mx) + fabsf(my) > 0.04f) {
    float inv = 1.0f / sqrtf(mx * mx + my * my + 0.001f);
    hunter.faceX = mx * inv;
    hunter.faceY = my * inv;
  }

  float inputMag = fabsf(mx) + fabsf(my);
  float damp = inputMag > 0.04f ? 0.78f : 0.64f;
  float moveMul = classSpeedMul();
  hunter.vx = hunter.vx * damp + mx * 84.0f * moveMul * (1.0f - damp);
  hunter.vy = hunter.vy * damp + my * 90.0f * moveMul * (1.0f - damp);
  hunter.x = clampf(hunter.x + hunter.vx * dt, 6.0f, WH_WORLD_LIMIT - 6.0f);
  hunter.y = clampf(hunter.y + hunter.vy * dt, 6.0f, WH_WORLD_LIMIT - 6.0f);
  camX = camX * 0.86f + hunter.x * 0.14f;
  camY = camY * 0.86f + hunter.y * 0.14f;
  hunter.attackCd = max(0.0f, hunter.attackCd - dt);
  hunter.slashT = max(0.0f, hunter.slashT - dt);
  hunter.focus = clampf(hunter.focus + dt * (hunter.autoPilot ? 2.0f : 4.0f), 0.0f, 100.0f + hunter.gearFocus + classFocusBonus());

  int alive = 0;
  for (int i = 0; i < WH_ENEMY_COUNT; ++i) {
    if (!enemies[i].active) continue;
    enemies[i].hitT = max(0.0f, enemies[i].hitT - dt);
    enemies[i].attackCd = max(0.0f, enemies[i].attackCd - dt);
    enemies[i].learnCd = max(0.0f, enemies[i].learnCd - dt);
    alive++;
    float dx = hunter.x - enemies[i].x;
    float dy = hunter.y - enemies[i].y;
    float d2 = dx * dx + dy * dy;
    float sx = 0.0f, sy = 0.0f;
    enemySlimeVector(enemies[i], dx, dy, d2, sx, sy);
    float sp = 7.5f + enemies[i].kind * 2.4f + dangerTierAt(enemies[i].x, enemies[i].y) * 0.35f + (enemies[i].elite ? 1.4f : 0.0f);
    if (enemies[i].aiAction == 3 && enemies[i].hp < enemies[i].maxHp * 0.42f) sp *= 0.82f;
    if (d2 < 180.0f * 180.0f) {
      enemies[i].x += sx * sp * dt;
      enemies[i].y += sy * sp * dt;
      if (enemies[i].learnCd <= 0.0f) {
        int8_t drip = d2 < 62.0f * 62.0f ? 2 : 1;
        if (enemies[i].hp < enemies[i].maxHp * 0.25f && enemies[i].aiAction != 3) drip = -2;
        slimeLearnEnemy(enemies[i].aiAction, enemies[i].aiFeat, drip);
        enemies[i].learnCd = 1.4f + (float)(esp_random() % 80) * 0.01f;
      }
    } else if (d2 > 300.0f * 300.0f) {
      spawnEnemy(i);
      continue;
    }
    if (d2 < 42.0f && !inMenu && enemies[i].attackCd <= 0.0f) {
      enemies[i].attackCd = enemies[i].elite ? 0.58f : 0.82f;
      float hit = (3.0f + enemies[i].kind * 1.8f) * (enemies[i].elite ? 1.7f : 1.0f);
      hunter.hp -= hit;
      slimeLearnEnemy(enemies[i].aiAction, enemies[i].aiFeat, enemies[i].elite ? 28 : 20);
      if (hunter.autoPilot) slimeLearnAuto(hunter.autoAction, hunter.autoFeat, -12);
      screenShakeT = max(screenShakeT, enemies[i].elite ? 0.18f : 0.10f);
      addFloatText(hunter.x, hunter.y - 12.0f, "HIT", canvas.color565(255, 94, 72));
      addBurst(hunter.x, hunter.y, canvas.color565(255, 80, 64), enemies[i].elite ? 8 : 5, enemies[i].elite ? 42.0f : 28.0f);
      if (hunter.hp <= 0) {
        hunter.hp = hunter.maxHp;
        hunter.gold = hunter.gold > 4 ? hunter.gold - 4 : 0;
        snprintf(statusLine, sizeof(statusLine), "REVIVE");
        addFloatText(hunter.x, hunter.y - 16.0f, "REVIVE", canvas.color565(164, 220, 255));
        markSaveDirty();
        if (hunter.autoPilot) slimeLearnAuto(hunter.autoAction, hunter.autoFeat, -32);
      }
    }
  }
  if (alive < 5) {
    for (int i = 0; i < WH_ENEMY_COUNT && alive < 9; ++i) {
      if (!enemies[i].active) { spawnEnemy(i); alive++; }
    }
  }
  ensureWorldPopulation();
}

uint16_t watchBitsForJob(uint16_t targetBits) {
  uint16_t floorBits = hunter.autoPilot ? 21 : 22;
  if (targetBits > 2) floorBits = max(floorBits, (uint16_t)(targetBits - 2));
  return floorBits;
}

void processOneJob(RemoteJobState& job, bool stale, uint16_t batch) {
  if (!job.active) return;
  uint32_t now = millis();
  uint32_t age = stale ? (now - job.rxMs + WH_REMOTE_JOB_TTL_MS) : (now - job.rxMs);
  if (!stale && age > WH_REMOTE_JOB_TTL_MS) {
    copyCurrentToStale(1);
    currentJob.active = false;
    return;
  }
  if (stale && now - job.rxMs > WH_STALE_SHADOW_MS) {
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
    if (job.nonce == job.endNonce) { job.active = false; break; }
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
    if (bits > bestBits) bestBits = bits;
    bool targetPass = bits >= targetBits && hashMeetsTargetBytes(shareHash, job.target);
    if (stale && targetPass) {
      sendTailPacket(2, 2, 3, bits, n, 1, true, shareHash + 28);
    } else if (!stale && bits >= watchBits) {
      sendTailPacket(1, 1, hunter.autoPilot ? 2 : 1, bits, n, targetPass ? 3 : 0, targetPass, shareHash + 28);
    }
  }
  mbedtls_sha256_free(&ctx);
}

void runObserverMiner() {
  uint16_t batch = hunter.autoPilot ? 180 : 72;
  if (inMenu) batch = 42;
  if (!buzzSeen || millis() - lastBuzzMs > 12000UL) batch = 36;
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
  tx.gpio_num = (gpio_num_t)WH_IR_TX_PIN;
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
  carrier.frequency_hz = WH_IR_CARRIER_HZ;
  carrier.duty_cycle = 0.33f;
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

void sendIrShaSigil(uint32_t seed) {
  if (!irReady || !irSigilEnabled) return;
  uint8_t buf[24];
  memset(buf, 0, sizeof(buf));
  memcpy(buf, WH_NODE_ID, min((size_t)12, strlen(WH_NODE_ID)));
  writeLE32(buf + 12, seed);
  writeLE32(buf + 16, bestBits);
  writeLE32(buf + 20, tailSeq);
  uint8_t h[32];
  doubleSha256(buf, sizeof(buf), h);

  rmt_symbol_word_t syms[48];
  size_t n = 0;
  pushSymbol(syms, n, 2560, 1024);
  for (int i = 0; i < 32 && n < 46; ++i) {
    bool bit = h[i >> 3] & (1 << (i & 7));
    uint16_t mark = bit ? 512 : 384;
    uint16_t space = bit ? 1152 : 448;
    if ((i & 7) == 0) space += 256;
    pushSymbol(syms, n, mark, space);
  }
  pushSymbol(syms, n, 256, 3600);
  rmt_transmit_config_t cfg = {};
  cfg.loop_count = 0;
  cfg.flags.queue_nonblocking = 1;
  rmt_transmit(irTxChan, irCopyEncoder, syms, n * sizeof(rmt_symbol_word_t), &cfg);
}

bool topButtonDown() {
  return M5.BtnB.isPressed() || M5.BtnPWR.isPressed();
}

void applyMenuAction() {
  switch (menuTab) {
    case TAB_INV:
      if (hunter.potions > 0 && hunter.hp < hunter.maxHp) {
        hunter.potions--;
        hunter.hp = min(hunter.maxHp, hunter.hp + 42.0f);
        snprintf(statusLine, sizeof(statusLine), "POTION");
        markSaveDirty();
      } else if (equipBestFromBag()) {
        // Status is set by equipBestFromBag().
      } else {
        snprintf(statusLine, sizeof(statusLine), "NO GEAR");
      }
      break;
    case TAB_SKILL:
      if (hunter.gold >= (uint16_t)(hunter.skill * 4)) {
        hunter.gold -= hunter.skill * 4;
        hunter.skill++;
        snprintf(statusLine, sizeof(statusLine), "SKILL %u", (unsigned)hunter.skill);
        markSaveDirty();
      } else {
        snprintf(statusLine, sizeof(statusLine), "NEED GOLD");
      }
      break;
    case TAB_CLASS:
      if (!nearTown(48.0f)) {
        snprintf(statusLine, sizeof(statusLine), "NEED HAVEN");
      } else {
        meta.heroClass = (uint8_t)((meta.heroClass + 1) % CLASS_COUNT);
        recalcHunterStats();
        hunter.hp = min(hunter.maxHp, hunter.hp + 16.0f);
        snprintf(statusLine, sizeof(statusLine), "CLASS %s", className(meta.heroClass));
        addFloatText(hunter.x, hunter.y - 14.0f, "CLASS", canvas.color565(168, 220, 255));
        markMetaDirty();
        markSaveDirty();
      }
      break;
    case TAB_QUEST:
      if (!nearTown(48.0f)) {
        snprintf(statusLine, sizeof(statusLine), "BOARD IN HAVEN");
      } else {
        rollQuest(mix32(millis() ^ hunter.gold ^ meta.fame));
        snprintf(statusLine, sizeof(statusLine), "QUEST %s", questName());
        addFloatText(hunter.x, hunter.y - 14.0f, "QUEST", canvas.color565(255, 208, 92));
        markSaveDirty();
      }
      break;
    case TAB_TRADE:
      if (!nearTown(42.0f)) {
        snprintf(statusLine, sizeof(statusLine), "NEED HAVEN");
      } else if (hunter.ore > 0) {
        hunter.gold += hunter.ore * 2;
        snprintf(statusLine, sizeof(statusLine), "ORE SOLD");
        hunter.ore = 0;
        markSaveDirty();
      } else if (hunter.gold >= 5 && hunter.potions < 9) {
        hunter.gold -= 5;
        hunter.potions++;
        snprintf(statusLine, sizeof(statusLine), "BUY POTION");
        markSaveDirty();
      } else {
        snprintf(statusLine, sizeof(statusLine), "NO TRADE");
      }
      break;
    case TAB_NPC:
      if (!nearTown(48.0f)) {
        snprintf(statusLine, sizeof(statusLine), "NPC IN HAVEN");
      } else if (hunter.gold >= (uint16_t)(10 + meta.enchant * 7) && meta.enchant < 12) {
        hunter.gold -= (uint16_t)(10 + meta.enchant * 7);
        meta.enchant++;
        recalcHunterStats();
        snprintf(statusLine, sizeof(statusLine), "ENCHANT +%u", (unsigned)meta.enchant);
        addBurst(hunter.x, hunter.y, canvas.color565(255, 210, 92), 12, 32.0f);
        markMetaDirty();
        markSaveDirty();
      } else {
        hunter.hp = hunter.maxHp;
        hunter.focus = 100.0f + hunter.gearFocus + classFocusBonus();
        snprintf(statusLine, sizeof(statusLine), "HEAL SERVICE");
        markSaveDirty();
      }
      break;
    case TAB_MINER:
      sendHeartbeat();
      sendEntropy();
      snprintf(statusLine, sizeof(statusLine), "MINER PULSE");
      M5.Speaker.tone(620, 26);
      break;
    case TAB_SHADOW:
      sendEvent(17, "tail_corpus_mark", 90, 28, (int16_t)bestBits, (int16_t)tailEvents, (int16_t)staleTailEvents, (int16_t)poolRejectEvents);
      snprintf(statusLine, sizeof(statusLine), "MARK CORPUS");
      break;
    case TAB_SETTINGS:
      brightnessIdx = (brightnessIdx + 1) % (sizeof(brightnessLevels) / sizeof(brightnessLevels[0]));
      M5.Display.setBrightness(brightnessLevels[brightnessIdx]);
      snprintf(statusLine, sizeof(statusLine), "BRIGHT %u", (unsigned)brightnessLevels[brightnessIdx]);
      break;
    default:
      break;
  }
}

void handleInput() {
  bool top = topButtonDown();
  uint32_t now = millis();

  if (top && !topDownPrev) {
    topDownAt = now;
    topLongDone = false;
  }
  if (top && !topLongDone && now - topDownAt > 760UL) {
    topLongDone = true;
    bool wasMenu = inMenu;
    inMenu = !inMenu;
    lastInputMs = now;
    if (wasMenu && !inMenu) capturePlayPose("POSE SET", false);
    else snprintf(statusLine, sizeof(statusLine), inMenu ? "MENU OPEN" : "MENU EXIT");
    M5.Speaker.tone(inMenu ? 420 : 260, 48);
  }
  if (!top && topDownPrev) {
    uint32_t held = now - topDownAt;
    if (!topLongDone && held > 35UL && held < 650UL) {
      lastInputMs = now;
      if (inMenu) {
        applyMenuAction();
      } else {
        if (!interactWorld()) {
          if (now - topTapWindowMs > 3600UL) {
            topTapWindowMs = now;
            topTapCount = 0;
          }
          topTapCount++;
          snprintf(statusLine, sizeof(statusLine), "TOP TAP %u", (unsigned)topTapCount);
          if (topTapCount >= 10) {
            topTapCount = 0;
            irSigilEnabled = !irSigilEnabled;
            snprintf(statusLine, sizeof(statusLine), irSigilEnabled ? "IR SIGIL ON" : "IR SIGIL OFF");
            if (irSigilEnabled) sendIrShaSigil(now ^ esp_random());
          }
        }
      }
    }
  }
  topDownPrev = top;

  if (M5.BtnA.wasPressed()) {
    lastInputMs = now;
    if (inMenu) {
      menuTab = (MenuTab)(((uint8_t)menuTab + 1) % TAB_COUNT);
      snprintf(statusLine, sizeof(statusLine), "TAB %s", tabName(menuTab));
      M5.Speaker.tone(510, 22);
    } else {
      hunterAttack();
    }
  }
}

uint16_t hpColor(float frac) {
  if (frac > 0.62f) return canvas.color565(72, 220, 120);
  if (frac > 0.32f) return canvas.color565(236, 184, 52);
  return canvas.color565(240, 60, 46);
}

void drawGroundShadow(int x, int y, int w, uint16_t tone = 0) {
  uint16_t c0 = tone ? tone : canvas.color565(2, 1, 3);
  uint16_t c1 = canvas.color565(7, 4, 8);
  canvas.drawFastHLine(x - w / 2 + 3, y, max(1, w - 6), c0);
  canvas.drawFastHLine(x - w / 2 + 1, y + 1, max(1, w - 2), c0);
  canvas.drawFastHLine(x - w / 2 + 4, y + 2, max(1, w - 8), c1);
}

void drawDiamond(int x, int y, int w, int h, uint16_t top, uint16_t bottom, uint16_t edge) {
  canvas.fillTriangle(x, y - h, x - w, y, x, y, top);
  canvas.fillTriangle(x, y - h, x, y, x + w, y, top);
  canvas.fillTriangle(x - w, y, x, y + h, x, y, bottom);
  canvas.fillTriangle(x, y, x + w, y, x, y + h, bottom);
  if (edge) {
    canvas.drawLine(x - w, y, x, y - h, edge);
    canvas.drawLine(x, y + h, x + w, y, edge);
  }
}

void fillQuad(int x0, int y0, int x1, int y1, int x2, int y2, int x3, int y3, uint16_t c) {
  canvas.fillTriangle(x0, y0, x1, y1, x2, y2, c);
  canvas.fillTriangle(x0, y0, x2, y2, x3, y3, c);
}

void drawSpark(int x, int y, uint16_t c) {
  canvas.drawPixel(x, y - 1, c);
  canvas.drawPixel(x - 1, y, c);
  canvas.drawPixel(x, y, c);
  canvas.drawPixel(x + 1, y, c);
  canvas.drawPixel(x, y + 1, c);
}

void drawBar(int x, int y, int w, int h, float frac, uint16_t col, uint16_t bg) {
  frac = clampf(frac, 0, 1);
  canvas.drawRect(x, y, w, h, canvas.color565(82, 58, 44));
  canvas.fillRect(x + 1, y + 1, w - 2, h - 2, bg);
  int fw = (int)((w - 2) * frac);
  if (fw > 0) canvas.fillRect(x + 1, y + 1, fw, h - 2, col);
}

void drawBatteryGauge(int x, int y) {
  int pct = M5.Power.getBatteryLevel();
  if (pct < 0 || pct > 100) pct = 100;
  uint16_t frame = canvas.color565(145, 118, 72);
  uint16_t low = canvas.color565(224, 58, 44);
  uint16_t mid = canvas.color565(234, 178, 60);
  uint16_t high = canvas.color565(84, 226, 138);
  uint16_t fill = pct <= 25 ? low : (pct <= 55 ? mid : high);
  canvas.drawRect(x, y, 24, 8, frame);
  canvas.fillRect(x + 24, y + 2, 2, 4, frame);
  canvas.fillRect(x + 1, y + 1, 22, 6, canvas.color565(8, 7, 8));
  int segs = (pct + 19) / 20;
  for (int i = 0; i < 5; ++i) {
    uint16_t c = i < segs ? fill : canvas.color565(36, 28, 24);
    canvas.fillRect(x + 2 + i * 4, y + 2, 3, 4, c);
  }
}

void drawTerrain() {
  canvas.fillScreen(canvas.color565(4, 4, 9));
  const int topHud = 15;
  const int bottomHud = 18;
  const int horizon = 22;
  const int groundBot = screenH - bottomHud;
  uint8_t tier = dangerTierAt(hunter.x, hunter.y);
  uint16_t sky0 = canvas.color565(6 + tier, 7, 16 + tier * 2);
  uint16_t sky1 = canvas.color565(16 + tier * 2, 10 + tier, 18 + tier);
  canvas.fillRect(0, topHud, screenW, horizon - topHud, sky0);
  canvas.fillRect(0, horizon, screenW, 3, sky1);
  canvas.drawFastHLine(0, horizon + 3, screenW, canvas.color565(42 + tier * 5, 28, 28));

  uint16_t far = canvas.color565(18 + tier * 2, 13, 15);
  uint16_t near = canvas.color565(44 + tier * 4, 27 + tier, 25);
  fillQuad(0, horizon + 4, screenW / 2 - 12, horizon + 4, screenW / 2 - 78, groundBot, 0, groundBot, far);
  fillQuad(screenW / 2 + 12, horizon + 4, screenW, horizon + 4, screenW, groundBot, screenW / 2 + 78, groundBot, near);

  for (int row = 0; row < 8; ++row) {
    float t0 = row / 8.0f;
    float t1 = (row + 1) / 8.0f;
    int y0 = horizon + 3 + (int)((groundBot - horizon - 3) * t0 * t0);
    int y1 = horizon + 3 + (int)((groundBot - horizon - 3) * t1 * t1);
    int w0 = 10 + (int)(screenW * 0.52f * t0);
    int w1 = 10 + (int)(screenW * 0.58f * t1);
    uint8_t shade = 14 + row * 4 + tier * 2;
    uint16_t c = (row & 1) ? canvas.color565(shade + 10, shade / 2 + 6, shade / 2 + 7)
                           : canvas.color565(shade + 5, shade / 2 + 5, shade / 2 + 8);
    fillQuad(screenW / 2 - w0, y0, screenW / 2 + w0, y0, screenW / 2 + w1, y1, screenW / 2 - w1, y1, c);
    if (row > 1 && row < 7) {
      uint16_t softEdge = canvas.color565(44 + row * 4, 30 + row * 2, 28 + row);
      canvas.drawFastHLine(max(0, screenW / 2 - w1 + 8), y1, min(screenW, max(1, w1 * 2 - 16)), softEdge);
    }
  }

  for (int i = 0; i < 18; ++i) {
    uint32_t h = mix32(worldSeed ^ (uint32_t)i * 0xA341316Cu);
    float depth = 0.18f + (float)((h >> 3) & 0x7F) / 156.0f;
    depth = clampf(depth, 0.18f, 0.96f);
    int y = horizon + 6 + (int)((groundBot - horizon - 8) * depth * depth);
    int baseX = (int)((h & 0xFF) * screenW / 256);
    int scroll = (int)(camX * (0.012f + depth * 0.045f) + camY * 0.009f);
    int x = (baseX - scroll) % (screenW + 24);
    if (x < -12) x += screenW + 24;
    x -= 12;
    if (x < -6 || x > screenW + 6 || y < horizon + 4 || y > groundBot - 1) continue;
    uint8_t shade = (uint8_t)(34 + depth * 54.0f + tier * 2);
    uint16_t rock = canvas.color565(shade, shade * 2 / 3, shade * 2 / 3);
    int sz = 1 + (int)(depth * 3.0f);
    if ((h & 3) == 0) {
      canvas.drawFastHLine(x - sz, y, sz * 2 + 2, canvas.color565(16, 10, 12));
      canvas.drawPixel(x, y - 1, rock);
    } else {
      canvas.fillRect(x, y, max(1, sz), 1, rock);
      if (sz > 2) canvas.drawPixel(x + sz, y + 1, canvas.color565(18, 10, 12));
    }
  }

  int tx = worldToScreenX(WH_TOWN_X);
  int ty = worldToScreenY(WH_TOWN_Y);
  if (tx > -50 && tx < screenW + 50 && ty > -38 && ty < screenH + 38) {
    uint16_t ring = canvas.color565(130, 90, 46);
    uint16_t fire = ((millis() >> 6) & 1) ? canvas.color565(255, 162, 48) : canvas.color565(255, 214, 92);
    drawGroundShadow(tx, ty + 23, 46, canvas.color565(3, 2, 4));
    canvas.fillTriangle(tx - 24, ty + 13, tx + 24, ty + 13, tx, ty - 23, canvas.color565(84, 36, 32));
    canvas.fillTriangle(tx - 17, ty + 12, tx + 17, ty + 12, tx, ty - 17, canvas.color565(156, 66, 42));
    canvas.fillRect(tx - 20, ty + 11, 40, 14, canvas.color565(39, 27, 25));
    canvas.drawRect(tx - 21, ty + 10, 42, 16, ring);
    canvas.fillRect(tx - 4, ty + 16, 8, 9, canvas.color565(8, 5, 7));
    canvas.fillTriangle(tx + 26, ty + 17, tx + 32, ty + 17, tx + 29, ty + 5, fire);
    canvas.drawCircle(tx + 29, ty + 16, 5, canvas.color565(92, 38, 18));
    canvas.drawPixel(tx + 29, ty + 8, canvas.color565(255, 245, 138));
  }
}

void drawWorldObject(const WorldObject& obj) {
  if (!obj.active || !onScreenWorld(obj.x, obj.y, 12)) return;
  int x = worldToScreenX(obj.x);
  int y = worldToScreenY(obj.y);
  uint16_t rare = obj.rarity >= 3 ? canvas.color565(244, 210, 88) :
                  obj.rarity == 2 ? canvas.color565(188, 118, 255) :
                  obj.rarity == 1 ? canvas.color565(88, 178, 255) :
                                    canvas.color565(146, 116, 84);
  drawGroundShadow(x, y + 5, obj.type == OBJ_SHRINE ? 18 : 13);
  if (obj.type == OBJ_ORE) {
    uint16_t c = obj.rarity >= 2 ? canvas.color565(92, 214, 255) : canvas.color565(74, 194, 218);
    uint16_t hi = canvas.color565(180, 248, 255);
    canvas.drawCircle(x, y, 8, canvas.color565(20, 60, 70));
    canvas.fillTriangle(x, y - 8, x - 5, y + 5, x + 1, y + 6, c);
    canvas.fillTriangle(x + 5, y - 5, x + 1, y + 5, x + 8, y + 4, canvas.color565(48, 140, 178));
    canvas.fillTriangle(x - 5, y - 2, x - 9, y + 5, x - 2, y + 6, canvas.color565(38, 116, 150));
    canvas.drawPixel(x, y - 3, hi);
    if (obj.rarity > 1) drawSpark(x + 5, y - 6, hi);
  } else if (obj.type == OBJ_CHEST) {
    canvas.fillRect(x - 7, y - 4, 14, 8, canvas.color565(80, 43, 24));
    canvas.fillRect(x - 6, y - 7, 12, 4, canvas.color565(118, 62, 30));
    canvas.drawRect(x - 7, y - 7, 14, 11, rare);
    canvas.drawFastHLine(x - 6, y - 1, 12, canvas.color565(32, 20, 16));
    canvas.fillRect(x - 1, y - 2, 3, 4, canvas.color565(255, 218, 92));
    if (obj.rarity >= 2) drawSpark(x + 7, y - 8, rare);
  } else if (obj.type == OBJ_SHRINE) {
    uint16_t pulse = ((millis() >> 7) & 1) ? canvas.color565(204, 140, 255) : canvas.color565(116, 76, 178);
    canvas.drawCircle(x, y - 3, 10, canvas.color565(54, 32, 78));
    canvas.fillRect(x - 4, y - 9, 8, 15, canvas.color565(60, 58, 78));
    canvas.fillTriangle(x - 7, y + 6, x + 7, y + 6, x, y + 1, canvas.color565(40, 36, 52));
    canvas.drawRect(x - 5, y - 10, 10, 16, pulse);
    canvas.drawPixel(x, y - 5, canvas.color565(245, 220, 255));
    drawSpark(x, y - 13, pulse);
  } else if (obj.type == OBJ_RELIC) {
    uint16_t pulse = ((millis() >> 6) & 1) ? canvas.color565(255, 226, 118) : canvas.color565(188, 116, 54);
    canvas.drawCircle(x, y - 2, 9, canvas.color565(82, 48, 20));
    canvas.fillTriangle(x, y - 10, x - 6, y, x + 6, y, canvas.color565(184, 104, 44));
    canvas.fillTriangle(x, y + 7, x - 6, y, x + 6, y, canvas.color565(96, 58, 30));
    canvas.drawLine(x, y - 10, x, y + 7, pulse);
    drawSpark(x, y - 12, pulse);
  }
}

void drawEnemySprite(const Enemy& e) {
  if (!e.active || !onScreenWorld(e.x, e.y, 12)) return;
  int x = worldToScreenX(e.x);
  int y = worldToScreenY(e.y);
  if (y < 20 || y > screenH - 13) return;
  bool flash = e.hitT > 0.0f && ((millis() >> 5) & 1);
  uint16_t body = e.kind == 0 ? canvas.color565(150, 40, 54) :
                  e.kind == 1 ? canvas.color565(166, 58, 174) :
                                canvas.color565(216, 104, 34);
  uint16_t shade = e.kind == 0 ? canvas.color565(72, 14, 22) :
                   e.kind == 1 ? canvas.color565(58, 20, 86) :
                                 canvas.color565(104, 42, 20);
  if (e.elite) {
    body = e.kind == 0 ? canvas.color565(214, 70, 70) :
           e.kind == 1 ? canvas.color565(226, 86, 224) :
                         canvas.color565(255, 142, 54);
    canvas.drawCircle(x, y - 1, 9, canvas.color565(230, 170, 64));
    canvas.drawPixel(x - 5, y - 9, canvas.color565(255, 224, 108));
    canvas.drawPixel(x, y - 11, canvas.color565(255, 224, 108));
    canvas.drawPixel(x + 5, y - 9, canvas.color565(255, 224, 108));
  }
  if (flash) {
    body = canvas.color565(255, 232, 178);
    shade = canvas.color565(255, 150, 92);
  }
  drawGroundShadow(x, y + 10, e.elite ? 24 : 18);
  if (e.kind == 0) {
    canvas.fillTriangle(x - 9, y - 7, x - 3, y - 2, x - 10, y - 13, shade);
    canvas.fillTriangle(x + 9, y - 7, x + 3, y - 2, x + 10, y - 13, shade);
    canvas.fillTriangle(x, y - 10, x - 9, y + 9, x + 9, y + 9, shade);
    canvas.fillCircle(x, y - 3, 7, body);
    canvas.drawLine(x - 7, y + 3, x - 13, y + 7, body);
    canvas.drawLine(x + 7, y + 3, x + 13, y + 7, body);
  } else if (e.kind == 1) {
    uint16_t aura = e.elite ? canvas.color565(226, 120, 255) : canvas.color565(92, 54, 128);
    canvas.drawCircle(x, y - 2, 10, aura);
    canvas.fillTriangle(x, y - 13, x - 10, y + 10, x + 10, y + 10, shade);
    canvas.fillTriangle(x, y - 10, x - 7, y + 7, x + 7, y + 7, body);
    canvas.drawLine(x - 9, y + 6, x - 4, y + 11, aura);
    canvas.drawLine(x + 9, y + 6, x + 4, y + 11, aura);
  } else {
    canvas.fillTriangle(x - 5, y - 4, x - 14, y - 10, x - 8, y + 4, shade);
    canvas.fillTriangle(x + 5, y - 4, x + 14, y - 10, x + 8, y + 4, shade);
    canvas.fillTriangle(x, y - 10, x - 8, y + 9, x + 8, y + 9, shade);
    canvas.fillCircle(x, y - 3, 6, body);
    canvas.drawLine(x + 5, y + 7, x + 12, y + 10, body);
  }
  canvas.drawPixel(x - 3, y - 5, canvas.color565(255, 190, 120));
  canvas.drawPixel(x + 3, y - 5, canvas.color565(255, 190, 120));
  canvas.drawFastHLine(x - 3, y + 1, 7, canvas.color565(38, 8, 12));
  if (e.maxHp > 0 && e.hp < e.maxHp) {
    float f = clampf(e.hp / e.maxHp, 0.0f, 1.0f);
    canvas.fillRect(x - 10, y + 12, 20, 2, canvas.color565(38, 8, 12));
    canvas.fillRect(x - 10, y + 12, (int)(20.0f * f), 2, e.elite ? canvas.color565(248, 174, 56) : canvas.color565(238, 72, 54));
  }
}

void drawHunterSprite() {
  int hx = worldToScreenX(hunter.x);
  int hy = worldToScreenY(hunter.y);
  uint16_t cloak = canvas.color565(34, 132, 170);
  uint16_t hood = canvas.color565(20, 42, 68);
  uint16_t trim = canvas.color565(234, 184, 84);
  drawGroundShadow(hx, hy + 12, 24);
  canvas.fillRect(hx - 6, hy - 11, 13, 5, hood);
  canvas.fillRect(hx - 5, hy - 15, 11, 7, canvas.color565(24, 58, 86));
  canvas.drawFastHLine(hx - 4, hy - 16, 9, trim);
  canvas.fillRect(hx - 7, hy - 6, 15, 5, canvas.color565(13, 31, 48));
  canvas.fillRect(hx - 6, hy - 1, 13, 11, cloak);
  canvas.fillRect(hx - 4, hy, 9, 8, canvas.color565(42, 150, 182));
  canvas.drawFastHLine(hx - 8, hy - 2, 17, trim);
  canvas.drawPixel(hx - 2, hy - 12, canvas.color565(220, 255, 255));
  canvas.drawPixel(hx + 3, hy - 12, canvas.color565(220, 255, 255));

  int armSwing = (fabsf(hunter.vx) + fabsf(hunter.vy) > 9.0f) ? (((millis() >> 6) & 1) ? 1 : -1) : 0;
  canvas.drawLine(hx - 8, hy - 1, hx - 12, hy + 5 + armSwing, canvas.color565(30, 98, 130));
  canvas.drawLine(hx + 8, hy - 1, hx + 12, hy + 5 - armSwing, canvas.color565(30, 98, 130));
  canvas.drawLine(hx - 3, hy + 10, hx - 6, hy + 15 - armSwing, canvas.color565(18, 48, 70));
  canvas.drawLine(hx + 4, hy + 10, hx + 7, hy + 15 + armSwing, canvas.color565(18, 48, 70));
  canvas.drawFastHLine(hx - 7, hy + 10, 15, canvas.color565(9, 18, 26));

  int wx = hx + (int)(hunter.faceX * 9.0f);
  int wy = hy + (int)(hunter.faceY * 9.0f);
  canvas.drawLine(hx + (int)(-hunter.faceY * 4.0f), hy + (int)(hunter.faceX * 4.0f), wx, wy, canvas.color565(72, 68, 74));
  canvas.drawLine(hx, hy, wx, wy, canvas.color565(230, 232, 220));
  if (hunter.slashT > 0.0f) {
    float f = hunter.slashT / 0.18f;
    int sx = hx + (int)(hunter.faceX * (12.0f + 8.0f * f));
    int sy = hy + (int)(hunter.faceY * (12.0f + 8.0f * f));
    int ax = hx + (int)(-hunter.faceY * 7.0f);
    int ay = hy + (int)(hunter.faceX * 7.0f);
    canvas.drawLine(ax, ay, sx, sy, canvas.color565(255, 236, 146));
    canvas.drawLine(ax + (int)(hunter.faceX * 3.0f), ay + (int)(hunter.faceY * 3.0f), sx + (int)(-hunter.faceY * 3.0f), sy + (int)(hunter.faceX * 3.0f), canvas.color565(255, 174, 74));
    canvas.drawCircle(sx, sy, 3, canvas.color565(255, 196, 82));
  }
}

void drawFx() {
  for (int i = 0; i < WH_PARTICLE_COUNT; ++i) {
    if (!particles[i].active || !onScreenWorld(particles[i].x, particles[i].y, 2)) continue;
    float left = 1.0f - particles[i].t / max(0.001f, particles[i].ttl);
    int x = worldToScreenX(particles[i].x);
    int y = worldToScreenY(particles[i].y);
    if (left > 0.62f) {
      canvas.fillRect(x - 1, y - 1, 3, 3, particles[i].color);
    } else if (left > 0.28f) {
      canvas.fillRect(x, y, 2, 2, particles[i].color);
    } else {
      canvas.drawPixel(x, y, particles[i].color);
    }
  }

  canvas.setTextSize(1);
  canvas.setTextDatum(MC_DATUM);
  for (int i = 0; i < WH_FLOAT_COUNT; ++i) {
    if (!floatTexts[i].active || !onScreenWorld(floatTexts[i].x, floatTexts[i].y, 6)) continue;
    int x = worldToScreenX(floatTexts[i].x);
    int y = worldToScreenY(floatTexts[i].y);
    canvas.setTextColor(canvas.color565(20, 10, 8), TFT_TRANSPARENT);
    canvas.drawString(floatTexts[i].text, x + 1, y + 1);
    canvas.setTextColor(floatTexts[i].color, TFT_TRANSPARENT);
    canvas.drawString(floatTexts[i].text, x, y);
  }
}

void drawHud() {
  uint16_t gold = canvas.color565(238, 186, 78);
  uint16_t ember = canvas.color565(220, 132, 38);
  uint16_t panel = canvas.color565(4, 3, 6);
  uint16_t line = canvas.color565(102, 62, 42);
  canvas.fillRect(0, 0, screenW, 13, panel);
  canvas.drawFastHLine(0, 13, screenW, line);
  canvas.drawFastHLine(0, 14, screenW, canvas.color565(18, 11, 14));
  canvas.setTextSize(1);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(gold, TFT_TRANSPARENT);
  canvas.drawString(className(meta.heroClass), 2, 2);
  canvas.setTextDatum(MC_DATUM);
  canvas.setTextColor(hunter.autoPilot ? canvas.color565(190, 120, 255) : canvas.color565(92, 220, 150), TFT_TRANSPARENT);
  char mid[18];
  snprintf(mid, sizeof(mid), "%s %s", hunter.autoPilot ? "AUTO" : "MAN", zoneName(dangerTierAt(hunter.x, hunter.y)));
  canvas.drawString(mid, screenW / 2, 6);
  drawBatteryGauge(screenW - 29, 2);

  int hudY = screenH - 18;
  canvas.fillRect(0, hudY, screenW, 18, panel);
  canvas.drawFastHLine(0, hudY, screenW, line);
  canvas.drawFastHLine(0, hudY + 1, screenW, canvas.color565(30, 18, 16));

  int hpW = screenW > 190 ? 58 : 42;
  int fpW = screenW > 190 ? 48 : 34;
  int xpW = screenW > 190 ? 46 : 31;
  drawBar(2, hudY + 3, hpW, 5, hunter.hp / hunter.maxHp, hpColor(hunter.hp / hunter.maxHp), canvas.color565(38, 8, 12));
  drawBar(5 + hpW, hudY + 3, fpW, 5, hunter.focus / max(1.0f, 100.0f + hunter.gearFocus), canvas.color565(74, 166, 255), canvas.color565(8, 18, 38));
  drawBar(8 + hpW + fpW, hudY + 3, xpW, 5, (float)hunter.xp / max(1, hunter.level * 36), gold, canvas.color565(44, 30, 8));

  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(TFT_SILVER, TFT_TRANSPARENT);
  char l[48];
  snprintf(l, sizeof(l), "L%u %s G%u O%u", (unsigned)hunter.level, gearGradeName(hunter.gearScore), (unsigned)hunter.gold, (unsigned)hunter.ore);
  canvas.drawString(l, 2, hudY + 10);
  canvas.setTextDatum(TR_DATUM);
  canvas.setTextColor(ember, TFT_TRANSPARENT);
  char r[48];
  snprintf(r, sizeof(r), "z%lu T%lu", (unsigned long)bestBits, (unsigned long)tailEvents);
  canvas.drawString(r, screenW - 1, hudY + 10);

  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(canvas.color565(180, 126, 82), TFT_TRANSPARENT);
  if (millis() < imuPoseSettleUntilMs) {
    canvas.drawString("POSE SETTLE", 2, 17);
  } else {
    canvas.drawString(nearTown(32.0f) ? "HAVEN: TOP TRADE" : statusLine, 2, 17);
  }
  canvas.setTextDatum(TR_DATUM);
  canvas.setTextColor(canvas.color565(206, 158, 82), TFT_TRANSPARENT);
  char q[18];
  snprintf(q, sizeof(q), "%s %u/%u", questName(), (unsigned)quest.progress, (unsigned)quest.target);
  canvas.drawString(q, screenW - 2, 17);
  if (irSigilEnabled) {
    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(canvas.color565(248, 156, 52), TFT_TRANSPARENT);
    canvas.drawString("IR", screenW - 2, 27);
  }
}

void drawWorld() {
  drawTerrain();
  for (int i = 0; i < WH_OBJECT_COUNT; ++i) if (objects[i].active && objects[i].y < hunter.y) drawWorldObject(objects[i]);
  for (int i = 0; i < WH_ENEMY_COUNT; ++i) if (enemies[i].active && enemies[i].y < hunter.y) drawEnemySprite(enemies[i]);
  drawHunterSprite();
  for (int i = 0; i < WH_OBJECT_COUNT; ++i) if (objects[i].active && objects[i].y >= hunter.y) drawWorldObject(objects[i]);
  for (int i = 0; i < WH_ENEMY_COUNT; ++i) if (enemies[i].active && enemies[i].y >= hunter.y) drawEnemySprite(enemies[i]);
  drawFx();
  drawHud();
}

void drawMinerClassicPanel(int x, int y, int w, int h) {
  uint32_t now = millis();
  uint16_t amber = canvas.color565(242, 174, 58);
  uint16_t dim = canvas.color565(126, 82, 28);
  uint16_t dark = canvas.color565(12, 8, 5);
  uint16_t glow = ((now >> 7) & 1) ? canvas.color565(255, 204, 86) : amber;
  bool buzzLive = buzzSeen && now - lastBuzzMs < 9000UL;
  uint32_t jobAge = currentJob.rxMs ? now - currentJob.rxMs : 0UL;
  uint32_t checked = 0;
  uint32_t range = currentJob.rangeSize ? currentJob.rangeSize : 1UL;
  if (currentJob.active && currentJob.nonce >= currentJob.startNonce) checked = currentJob.nonce - currentJob.startNonce;
  if (checked > range) checked = range;
  float prog = currentJob.active ? clampf((float)checked / (float)range, 0.0f, 1.0f) : 0.0f;

  canvas.fillRect(x + 2, y + 13, w - 4, h - 15, dark);
  canvas.drawRect(x + 3, y + 14, w - 6, h - 17, dim);
  canvas.drawFastHLine(x + 7, y + 25, w - 14, dim);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextSize(1);
  canvas.setTextColor(glow, TFT_TRANSPARENT);
  canvas.drawString("JANUS A9 OBSERVER", x + 8, y + 17);
  canvas.setTextDatum(TR_DATUM);
  canvas.setTextColor(buzzLive ? canvas.color565(116, 242, 142) : canvas.color565(220, 70, 48), TFT_TRANSPARENT);
  canvas.drawString(buzzLive ? "BUZZ" : "LOCAL", x + w - 8, y + 17);

  char a[32], b[32], c[32], d[32];
  snprintf(a, sizeof(a), "H %lu", (unsigned long)observerHashrate);
  snprintf(b, sizeof(b), "BEST %lu/%u", (unsigned long)bestBits, (unsigned)targetBitsNow);
  snprintf(c, sizeof(c), "JOB %lums", (unsigned long)jobAge);
  snprintf(d, sizeof(d), "TAIL %lu S%lu R%lu", (unsigned long)tailEvents, (unsigned long)staleTailEvents, (unsigned long)poolRejectEvents);

  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(amber, TFT_TRANSPARENT);
  canvas.drawString(a, x + 8, y + 30);
  canvas.drawString(b, x + 8, y + 40);
  canvas.setTextColor(canvas.color565(210, 140, 48), TFT_TRANSPARENT);
  canvas.drawString(c, x + 82, y + 30);
  canvas.drawString(d, x + 82, y + 40);

  int bx = x + 8;
  int by = y + h - 15;
  int bw = w - 16;
  canvas.drawRect(bx, by, bw, 6, dim);
  canvas.fillRect(bx + 1, by + 1, bw - 2, 4, canvas.color565(24, 14, 6));
  int fill = (int)((bw - 2) * prog);
  if (fill > 0) canvas.fillRect(bx + 1, by + 1, fill, 4, glow);
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(dim, TFT_TRANSPARENT);
  canvas.drawString(lastTailLine, x + 8, y + h - 25);
}

void drawMenu() {
  drawWorld();
  int x = 6, y = 19, w = screenW - 12, h = screenH - 28;
  canvas.fillRect(x, y, w, h, canvas.color565(5, 3, 5));
  canvas.drawRect(x, y, w, h, canvas.color565(214, 144, 48));
  canvas.drawFastHLine(x, y + 11, w, canvas.color565(110, 44, 32));
  canvas.setTextDatum(TL_DATUM);
  canvas.setTextColor(canvas.color565(245, 190, 86), TFT_TRANSPARENT);
  canvas.drawString(tabName(menuTab), x + 4, y + 2);
  if (menuTab == TAB_MINER) {
    drawMinerClassicPanel(x, y, w, h);
    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(canvas.color565(92, 170, 255), TFT_TRANSPARENT);
    canvas.drawString("Blue=Tab", x + w - 4, y + 2);
    return;
  }
  canvas.setTextColor(TFT_SILVER, TFT_TRANSPARENT);
  char l1[48], l2[48], l3[48];
  if (menuTab == TAB_INV) {
    int filled = 0;
    for (int i = 0; i < 4; ++i) if (bag[i].active) filled++;
    snprintf(l1, sizeof(l1), "Potion %u  Bag %d/4", (unsigned)hunter.potions, filled);
    snprintf(l2, sizeof(l2), "GS%d A%d HP+%d", hunter.gearScore, hunter.gearAtk, hunter.gearHp);
    snprintf(l3, sizeof(l3), "Top: heal/equip");
  } else if (menuTab == TAB_SKILL) {
    snprintf(l1, sizeof(l1), "Hunter skill %u", (unsigned)hunter.skill);
    snprintf(l2, sizeof(l2), "Cost %u gold", (unsigned)(hunter.skill * 4));
    snprintf(l3, sizeof(l3), "Top: upgrade");
  } else if (menuTab == TAB_CLASS) {
    snprintf(l1, sizeof(l1), "%s  +%u", className(meta.heroClass), (unsigned)meta.enchant);
    snprintf(l2, sizeof(l2), "HP%+d ATK%+d F%+d", classHpBonus(), classAtkBonus(), classFocusBonus());
    snprintf(l3, sizeof(l3), nearTown(48.0f) ? "Top: next class" : "Haven trainer");
  } else if (menuTab == TAB_QUEST) {
    snprintf(l1, sizeof(l1), "%s %u/%u", questName(), (unsigned)quest.progress, (unsigned)quest.target);
    snprintf(l2, sizeof(l2), "Reward G%u XP%u", (unsigned)quest.rewardGold, (unsigned)quest.rewardXp);
    snprintf(l3, sizeof(l3), nearTown(48.0f) ? "Top: new board" : "Board in Haven");
  } else if (menuTab == TAB_TRADE) {
    snprintf(l1, sizeof(l1), "Haven %s", nearTown(42.0f) ? "open" : "far");
    snprintf(l2, sizeof(l2), "Ore %u  Gold %u", (unsigned)hunter.ore, (unsigned)hunter.gold);
    snprintf(l3, sizeof(l3), "Top: sell/buy");
  } else if (menuTab == TAB_NPC) {
    snprintf(l1, sizeof(l1), "Clan %u Fame %lu", (unsigned)meta.clanRep, (unsigned long)meta.fame);
    snprintf(l2, sizeof(l2), "Relic %u Cost %u", (unsigned)meta.relics, (unsigned)(10 + meta.enchant * 7));
    snprintf(l3, sizeof(l3), nearTown(48.0f) ? "Top: enchant/heal" : "NPC in Haven");
  } else if (menuTab == TAB_SHADOW) {
    snprintf(l1, sizeof(l1), "tails %lu stale %lu", (unsigned long)tailEvents, (unsigned long)staleTailEvents);
    snprintf(l2, sizeof(l2), "slime %u upd %lu", (unsigned)slimeAvgTrace(), (unsigned long)slime.updates);
    snprintf(l3, sizeof(l3), "poolR %lu mark", (unsigned long)poolRejectEvents);
  } else {
    uint8_t steps = sizeof(brightnessLevels) / sizeof(brightnessLevels[0]);
    snprintf(l1, sizeof(l1), "Bright %u/%u = %u", (unsigned)(brightnessIdx + 1), (unsigned)steps, (unsigned)brightnessLevels[brightnessIdx]);
    snprintf(l2, sizeof(l2), "IR %s  IMU %.2f", irSigilEnabled ? "ON" : "OFF", imuManualIntent);
    snprintf(l3, sizeof(l3), "Top: brightness");
  }
  canvas.drawString(l1, x + 4, y + 16);
  canvas.drawString(l2, x + 4, y + 26);
  canvas.drawString(l3, x + 4, y + 36);
  canvas.setTextDatum(TR_DATUM);
  canvas.setTextColor(canvas.color565(92, 170, 255), TFT_TRANSPARENT);
  canvas.drawString("Blue=Tab", x + w - 4, y + 2);
}

void drawFrame() {
  if (!canvasReady) return;
  if (inMenu) drawMenu();
  else drawWorld();
  canvas.pushSprite(0, 0);
}

void serviceRadio() {
  uint32_t now = millis();
  if (now - lastHbMs >= WH_HEARTBEAT_MS) {
    lastHbMs = now;
    sendHeartbeat();
  }
  if (now - lastEntropyMs >= WH_ENTROPY_MS) {
    lastEntropyMs = now;
    sendEntropy();
  }
}

void serviceIr() {
  if (!irSigilEnabled || !irReady) return;
  uint32_t now = millis();
  if (now - lastIrMs < 2400UL) return;
  lastIrMs = now;
  sendIrShaSigil(now ^ observerHashesTotal ^ ((uint32_t)bestBits << 16));
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
  M5.Speaker.setVolume(18);
  LittleFS.begin(true);
  cleanupTailLog();
  initSlimeBrain();
  loadSlimeState();
  loadMetaState();

  M5.Imu.begin();
  calibrateImu(true);
  initGame();
  loadHunterState();
  ensureWorldPopulation();
  initEspNow();
  irInit();
  lastInputMs = millis();
  lastFrameMs = millis();
  sendEvent(1, "witchhunter_boot", 96, 28, (int16_t)workerId(), 0, 0, 0);
  Serial.printf("[WH] %s ready node=%s screen=%dx%d irPin=%u carrier=%uHz observer_only=1 no_s2=1\n",
                WH_VERSION, WH_NODE_ID, screenW, screenH, (unsigned)WH_IR_TX_PIN, (unsigned)WH_IR_CARRIER_HZ);
}

void loop() {
  M5.update();
  uint32_t now = millis();
  float dt = (now - lastFrameMs) / 1000.0f;
  lastFrameMs = now;
  dt = clampf(dt, 0.001f, 0.050f);

  handleInput();
  updateImu(dt);
  updateGame(dt);
  runObserverMiner();
  updateHashrate();
  serviceRadio();
  serviceIr();
  cleanupTailLog();
  slimeCleanup();
  saveHunterState(false);
  saveSlimeState(false);
  saveMetaState(false);

  if (now - lastDrawMs >= 33UL) {
    lastDrawMs = now;
    drawFrame();
  }
  if (now - lastDiagMs >= 5000UL) {
    lastDiagMs = now;
    Serial.printf("[WH] buzz=%u job=%u age=%lu H=%lu best=%lu target=%u tails=%lu stale=%lu poolR=%lu ir=%u mode=%s imu=%.2f/%.2f intent=%.2f q=%s:%u/%u slime=%u/%lu log=%s\n",
                  buzzSeen && now - lastBuzzMs < 9000UL ? 1 : 0,
                  currentJob.active ? 1 : 0,
                  currentJob.rxMs ? (unsigned long)(now - currentJob.rxMs) : 0UL,
                  (unsigned long)observerHashrate,
                  (unsigned long)bestBits,
                  (unsigned)targetBitsNow,
                  (unsigned long)tailEvents,
                  (unsigned long)staleTailEvents,
                  (unsigned long)poolRejectEvents,
                  irSigilEnabled ? 1 : 0,
                  hunter.autoPilot ? "AUTO" : (inMenu ? "MENU" : "MAN"),
                  imuRoll, imuPitch,
                  imuManualIntent,
                  questName(), (unsigned)quest.progress, (unsigned)quest.target,
                  (unsigned)slimeAvgTrace(), (unsigned long)slime.updates,
                  WH_LOG_PATH);
  }
}
