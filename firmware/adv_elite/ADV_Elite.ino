#include <M5Unified.h>
#include <M5Cardputer.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>
#include <Preferences.h>
#include <SPI.h>
#include <SD.h>
#include <math.h>
#include <ctype.h>
#include <M5UnitENV.h>
#include <TinyGPSPlus.h>

#ifndef ADV_CAP_LORA_ENABLE
#define ADV_CAP_LORA_ENABLE 1
#endif

#if ADV_CAP_LORA_ENABLE
#include <RadioLib.h>
#include "utility/PI4IOE5V6408_Class.hpp"
#endif

// =====================================================
// JANUS CARDPUTER ADV ELITE ZERO v0.20 SKY ANCHOR
// Clean manual-only game core. No Janus AI, no station automation,
// no hidden autopilot. Buzz worker is explicit and toggleable.
// v0.20: Cardputer ADV becomes an explicit Sky Anchor: richer P/N flags,
//        JSA3 LoRa sky-seal, compact health line, no raw GNSS coordinates.
// v0.19: ESP-NOW rescue for swarm visibility, richer ADV sky-anchor flags,
//        real-only ENV remains isolated from game/miner telemetry.
// v0.18: Elite-1984 seed galaxy lattice + SD known-cosmos landmark layer,
//        map route picker, economy-aware station prices, sky seal galaxy context.
// v0.17: Cardputer ADV LoRa/GNSS CAP sky-anchor, PN cortex telemetry,
//        low-duty LoRa sky seals, Beacon terminal SKY row.
// v0.16: SD-first A9 corpus, NVS fallback, terminal save/prune, corpus telemetry.
// v0.15: legacy A9 lane policy default, near-hit diagnostics, v0.11-style miner screen.
// v0.14: miner terminal tabs: amber A9 deck + legacy Beacon-style dashboard.
// v0.13: amber Pip-Boy/New-Vegas-style mining deck, share LED jackpot signal,
//        sound on 8/9, throttle restored to +/-/0.
// v0.12: sane controls pass: reduced sensitivity and stable one-shot command keys.
// v0.11: RBLGANUL A9 ESP32 port: broad mixture nonce lanes, random-control accounting,
//        Zim reverse odd-stride, dual-lock probes and same-job stale guard.
// v0.10: optional overnight Buzz ESP-NOW worker, J/B jobs -> S/2 shares.
// v0.9: station tabs with Enter confirm, pirate den, belt encounters.
// v0.8: free local 3D orientation, reverse thrust, cruise, cleaner HUD.
// v0.7: cockpit views, strict mode gates, compact Elite-style scanner.
// v0.6: explicit throttle controls; flight axes no longer change thrust.
// v0.4: soft IMU aim assist with manual zero.
// v0.3: decoupled flight axes, cockpit hardpoint laser beams.
// v0.2: inertial yaw/pitch/roll, seamless local space, steady LED.
// =====================================================

static constexpr int16_t SCREEN_W = 240;
static constexpr int16_t SCREEN_H = 135;
static constexpr uint32_t LOGIC_MS = 33;
static constexpr uint8_t ROCK_COUNT = 14;
static constexpr uint8_t ENEMY_COUNT = 7;
static constexpr uint8_t TRAFFIC_COUNT = 6;
static constexpr uint8_t MINER_COUNT = 5;
static constexpr uint8_t STAR_COUNT = 82;
static constexpr uint16_t GALAXY_SYSTEM_COUNT = 256;
static constexpr uint8_t GALAXY_COUNT = 8;
static constexpr uint8_t COSMOS_LANDMARK_COUNT = 18;
static constexpr uint8_t COSMOS_CACHE_MAX = 32;
static constexpr uint16_t GALAXY_BASE_JUMP_RANGE = 42;
static constexpr float THROTTLE_REVERSE_MAX = -0.22f;
static constexpr float THROTTLE_FORWARD_MAX = 0.66f;
static constexpr float THROTTLE_STEP = 0.055f;

static constexpr uint8_t BUZZ_ESPNOW_CHANNEL = 10;
static constexpr uint32_t BUZZ_HEARTBEAT_MS = 2000UL;
static constexpr uint32_t BUZZ_JOB_TIMEOUT_MS = 90000UL;
static constexpr uint32_t BUZZ_MINER_DEBUG_MS = 2000UL;
static constexpr uint32_t BUZZ_CORPUS_SAVE_MS = 300000UL;
static constexpr uint32_t BUZZ_CORPUS_PRUNE_MS = 1800000UL;
static constexpr uint32_t BUZZ_CORPUS_MAGIC = 0xA9C0DE16UL;
static constexpr uint16_t BUZZ_CORPUS_VERSION = 1;
static constexpr uint32_t BUZZ_CORPUS_MAX_LOG_BYTES = 262144UL;
static constexpr uint32_t SWARMSENSE_TX_MS = 3500UL;
static constexpr uint32_t PILOTLINK_TX_MS = 1200UL;
static constexpr uint32_t ADV_SKYANCHOR_TX_MS = 5200UL;
static constexpr uint32_t ADV_SKY_HEALTH_MS = 15000UL;
static constexpr uint32_t ADV_CAP_LORA_TX_MS = 45000UL;
static constexpr uint32_t ADV_CAP_LORA_TX_TIMEOUT_MS = 3500UL;
static constexpr uint32_t BUZZ_JOB_RANGE_DEFAULT = 262144UL;
static constexpr uint8_t BUZZ_MINER_SECTORS = 12;
static constexpr uint8_t BUZZ_A9_STRATEGY_COUNT = 7;
static constexpr uint8_t BUZZ_A9_STRIDE_ARM_COUNT = 17;
static constexpr uint16_t BUZZ_BATCH_NIGHT = 180;
static constexpr uint16_t BUZZ_BATCH_ACTIVE = 140;
static constexpr uint16_t BUZZ_BATCH_MIN = 40;
static constexpr uint16_t BUZZ_BATCH_MAX = 420;
static constexpr uint32_t BUZZ_NIGHT_BUDGET_US = 2200UL;
static constexpr uint32_t BUZZ_ACTIVE_BUDGET_US = 900UL;
static constexpr uint32_t KEY_HELD_CONTROL_MS = 34UL;
static constexpr uint32_t KEY_STATION_NAV_MS = 220UL;
static constexpr uint32_t KEY_MINER_TAB_MS = 220UL;
static constexpr uint32_t KEY_CHAR_COOLDOWN_MS = 170UL;
static constexpr uint32_t KEY_ENTER_COOLDOWN_MS = 280UL;
static constexpr uint32_t BUZZ_SHARE_LED_FLASH_MS = 2600UL;
static constexpr uint32_t BEACON_ENV_TX_MS = 4200UL;
static constexpr uint32_t ENV_REAL_TTL_MS = 15000UL;
static constexpr uint32_t ENV_QMP_REAL_TTL_MS = 30000UL;
static constexpr uint32_t BUZZ_ESPNOW_RESCUE_COOLDOWN_MS = 12000UL;
static constexpr uint32_t BUZZ_ESPNOW_MASTER_STALE_RESCUE_MS = 28000UL;
static constexpr uint16_t BUZZ_ESPNOW_FAIL_STREAK_RESCUE = 18;
static const char BUZZ_NODE_ID[] = "CardputerElite";
static const char BUZZ_NODE_ROLE[] = "CARD_A9";
static const char A9_CORPUS_DIR[] = "/janus/a9";
static const char A9_CORPUS_FILE[] = "/janus/a9/corpus.bin";
static const char A9_CORPUS_TMP_FILE[] = "/janus/a9/corpus.tmp";
static const char A9_CORPUS_BAK_FILE[] = "/janus/a9/corpus.bak";
static const char A9_CORPUS_LOG_FILE[] = "/janus/a9/session.jsonl";
static const char A9_CORPUS_LOG_OLD_FILE[] = "/janus/a9/session.old";
static const char KNOWN_COSMOS_FILE[] = "/janus/cosmos/known.csv";
static constexpr uint8_t SD_SPI_SCK_PIN = 40;
static constexpr uint8_t SD_SPI_MISO_PIN = 39;
static constexpr uint8_t SD_SPI_MOSI_PIN = 14;
static constexpr uint8_t SD_SPI_CS_PIN = 12;
static constexpr uint8_t GROVE_SDA_PIN = 2;
static constexpr uint8_t GROVE_SCL_PIN = 1;
static constexpr uint8_t SHT30_ADDR = 0x44;
static constexpr uint32_t ENV_READ_MS = 1800UL;
static constexpr uint8_t ADV_CAP_GNSS_RX_PIN = 15;
static constexpr uint8_t ADV_CAP_GNSS_TX_PIN = 13;
static constexpr uint32_t ADV_CAP_GNSS_BAUD = 115200UL;
static constexpr uint8_t ADV_CAP_LORA_NSS_PIN = 5;
static constexpr uint8_t ADV_CAP_LORA_IRQ_PIN = 4;
static constexpr uint8_t ADV_CAP_LORA_RST_PIN = 3;
static constexpr uint8_t ADV_CAP_LORA_BUSY_PIN = 6;
static constexpr float ADV_CAP_LORA_FREQ_MHZ = 868.0f;
static constexpr float ADV_CAP_LORA_BW_KHZ = 125.0f;
static constexpr uint8_t ADV_CAP_LORA_SF = 9;
static constexpr uint8_t ADV_CAP_LORA_CR = 7;
static constexpr uint8_t ADV_CAP_LORA_SYNC_WORD = 0x34;
static constexpr int8_t ADV_CAP_LORA_TX_POWER_DBM = 10;
static constexpr uint16_t ADV_CAP_LORA_PREAMBLE_LEN = 8;
static constexpr float ADV_CAP_LORA_TCXO_VOLT = 1.6f;

QMP6988 janusQmp6988;

M5Canvas canvas(&M5.Display);

enum ViewMode : uint8_t {
  VIEW_FLIGHT,
  VIEW_DOCKED,
  VIEW_MAP,
  VIEW_STATUS,
  VIEW_MINER
};

enum MinerTab : uint8_t {
  MINER_TAB_A9,
  MINER_TAB_BEACON,
  MINER_TAB_COUNT
};

enum BeaconMenuRow : uint8_t {
  BEACON_ROW_BUZZ,
  BEACON_ROW_PROFILE,
  BEACON_ROW_ENV,
  BEACON_ROW_SKY,
  BEACON_ROW_LED,
  BEACON_ROW_IMU,
  BEACON_ROW_A9_MODE,
  BEACON_ROW_A9_SAVE,
  BEACON_ROW_A9_PRUNE,
  BEACON_ROW_A9_TAB,
  BEACON_ROW_COUNT
};

enum CockpitView : uint8_t {
  LOOK_FORWARD,
  LOOK_REAR,
  LOOK_LEFT,
  LOOK_RIGHT
};

enum SpaceZone : uint8_t {
  ZONE_STATION,
  ZONE_BELT,
  ZONE_PIRATE_DEN
};

enum NavTarget : uint8_t {
  NAV_NONE,
  NAV_STATION,
  NAV_BELT,
  NAV_PIRATE_DEN
};

enum StationTab : uint8_t {
  ST_TAB_DOCK,
  ST_TAB_MARKET,
  ST_TAB_BAR,
  ST_TAB_SHIP,
  ST_TAB_COUNT
};

enum TargetKind : uint8_t {
  TARGET_NONE,
  TARGET_ENEMY,
  TARGET_ROCK,
  TARGET_MINER,
  TARGET_TRAFFIC
};

enum HitSide : uint8_t {
  HIT_FRONT,
  HIT_REAR,
  HIT_LEFT,
  HIT_RIGHT
};

enum Faction : uint8_t {
  FAC_ZOVEON_LEAGUE,
  FAC_FREE_MINERS,
  FAC_MACHINE_REMNANTS,
  FACTION_COUNT
};

enum CosmosLandmarkType : uint8_t {
  COSMOS_STAR,
  COSMOS_PULSAR,
  COSMOS_BLACK_HOLE,
  COSMOS_NEBULA,
  COSMOS_GALAXY,
  COSMOS_LAB
};

enum BuzzA9Strategy : uint8_t {
  A9_LINEAR,
  A9_ZIM_REVERSE,
  A9_ZIM_BANDIT,
  A9_RANDOM,
  A9_JANUS,
  A9_KNIGHT,
  A9_BITREV
};

enum BuzzA9Lane : uint8_t {
  A9_LANE_JANUS_DISPATCHER,
  A9_LANE_DUAL_LOCK,
  A9_LANE_ZIM_S6,
  A9_LANE_RANDOM_BASELINE,
  A9_LANE_EXPLOIT_BEST,
  A9_LANE_SURVIVE_LINEAR
};

struct EliteSeed6 {
  uint16_t w0;
  uint16_t w1;
  uint16_t w2;
};

struct EliteSystem {
  char name[16];
  uint8_t x;
  uint8_t y;
  uint8_t economy;
  uint8_t government;
  uint8_t techLevel;
  uint8_t danger;
  uint8_t population;
  uint16_t radius;
  uint32_t signature;
};

struct CosmosLandmark {
  char name[18];
  uint8_t galaxy;
  uint8_t x;
  uint8_t y;
  uint8_t type;
  uint8_t danger;
  uint8_t science;
  uint16_t influence;
};

struct Body {
  bool alive;
  float x;
  float y;
  float z;
  float vx;
  float vy;
  float vz;
  int hp;
};

struct Star {
  float x;
  float y;
  float z;
};

struct Vec3 {
  float x;
  float y;
  float z;
};

struct TargetPick {
  TargetKind kind;
  int index;
  float score;
};

struct __attribute__((packed)) BuzzColonyPacket {
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

struct __attribute__((packed)) SwarmSensePacket {
  uint8_t magic[2];
  uint8_t version;
  uint16_t worker_id;
  char nodeId[24];
  char kind[16];
  uint32_t seq;
  uint32_t uptime_ms;
  uint32_t micros_tail;
  uint32_t free_heap;
  uint16_t loop_jitter_us;
  uint16_t loop_max_us;
  int8_t rssi;
  uint8_t radio_mode;
  uint8_t bt_flags;
  uint8_t palette;
  uint8_t knn_label;
  uint8_t knn_confidence;
  uint8_t ai_hint;
  uint8_t thermal_load;
  uint16_t effective_batch;
  uint16_t dynamic_batch;
  uint32_t hash_rate;
  uint32_t total_hashes;
  uint16_t best_bits;
  uint16_t hash_eff_x1000;
  int16_t prediction_error_x1000;
  uint16_t entropy_x1000;
  uint16_t touch_delta;
  uint16_t job_age_s;
  uint16_t nonce_remaining_l16;
  uint16_t flags;
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

struct __attribute__((packed)) JanusAiNodePacket {
  uint8_t magic[2];        // 'A','I'
  uint8_t version;         // 1
  uint8_t flags;           // bit0=has SD, bit1=can archive, bit2=master-ish
  char nodeId[24];
  char role[16];
  uint32_t seq;
  uint32_t uptime_ms;
  float entropy;
  float prediction_error;
  float sync;
  float fit;
  float attention;
  float values[6];         // temp, humidity, pressure, entropy, online-ish, rssi
};

struct __attribute__((packed)) JanusPilotLinkPacket {
  uint8_t magic[2];
  uint8_t version;
  char nodeId[24];
  uint32_t seq;
  uint8_t galaxy;
  uint8_t system;
  uint8_t sector;
  uint8_t mode;
  uint16_t distance;
  uint16_t credits_l16;
  uint16_t kills;
  uint16_t shield_x10;
  uint16_t energy_x10;
  uint8_t mech_level;
  uint8_t mech_heat;
  uint16_t mech_armor;
  uint8_t surface_active;
  uint8_t objective;
  uint8_t threat;
  int8_t rssi;
  uint32_t uptime_ms;
};

struct __attribute__((packed)) JanusPnCortexPacket {
  uint8_t magic[2];       // 'P','N'
  uint8_t version;
  uint8_t role;
  uint16_t worker_id;
  char nodeId[24];
  char kind[16];
  uint32_t seq;
  uint32_t uptime_ms;
  uint32_t job_sig;
  uint32_t prev_hash;
  uint32_t packet_hash;
  uint32_t hash_rate;
  uint32_t total_hashes;
  uint16_t target_bits;
  uint16_t best_bits;
  uint8_t lane;
  uint8_t sector;
  uint8_t flags;
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

struct __attribute__((packed)) BuzzJobPacket {
  uint8_t magic[2];
  uint8_t job_id[8];
  uint8_t header[80];
  uint32_t start_nonce;
  uint32_t range_size;
  uint8_t target[32];
  uint32_t extranonce2;
};

struct __attribute__((packed)) BuzzShareResponseV2 {
  uint8_t magic[2];
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
  uint16_t bits;
  uint32_t total_hashes_l32;
  uint8_t hash_tail[4];
};

struct __attribute__((packed)) BuzzAgentRewardPacket {
  uint8_t magic[2];
  uint8_t version;
  char source[16];
  char targetNode[24];
  uint32_t seq;
  uint8_t rewardLevel;
  uint8_t aiHint;
  uint16_t rewardPoints;
  uint16_t targetBatch;
  uint32_t entropySeed;
  float score;
  float predictedHashRate;
  float predictionError;
  uint32_t deltaShares;
  uint32_t uptime_ms;
};

struct BuzzRemoteJobState {
  bool active = false;
  uint8_t job_id[8] = {};
  uint8_t header[80] = {};
  uint8_t target[32] = {};
  uint32_t startNonce = 0;
  uint32_t rangeSize = 0;
  uint32_t nonce = 0;
  uint32_t hashesDone = 0;
  uint32_t receivedAt = 0;
  uint8_t minerLane = 0;
  uint8_t minerStrategy = 0;
  uint8_t minerSector = 0;
  uint8_t minerStrideArm = 0;
  uint32_t minerSeed = 0;
  uint32_t minerStride = 1;
  uint32_t minerStartOffset = 0;
};

struct __attribute__((packed)) A9CorpusBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t size;
  uint32_t savedAtMs;
  uint32_t bootCount;
  uint32_t saveCount;
  uint32_t pruneCount;
  uint32_t corpusHashes;
  uint32_t corpusShares;
  uint32_t corpusBestBits;
  uint32_t near20;
  uint32_t near21;
  uint32_t near22;
  uint32_t compareFails;
  uint32_t strategyHashes[BUZZ_A9_STRATEGY_COUNT];
  uint32_t strategyBest[BUZZ_A9_STRATEGY_COUNT];
  uint32_t strategyShares[BUZZ_A9_STRATEGY_COUNT];
  uint32_t sectorHashes[BUZZ_MINER_SECTORS];
  uint32_t sectorBest[BUZZ_MINER_SECTORS];
  uint32_t sectorShares[BUZZ_MINER_SECTORS];
  uint32_t strideHashes[BUZZ_A9_STRIDE_ARM_COUNT];
  uint32_t strideBest[BUZZ_A9_STRIDE_ARM_COUNT];
  uint32_t strideShares[BUZZ_A9_STRIDE_ARM_COUNT];
  uint32_t broadHashes;
  uint32_t broadBest;
  uint32_t broadShares;
  uint32_t randomHashes;
  uint32_t randomBest;
  uint32_t randomShares;
  uint32_t checksum;
};

ViewMode viewMode = VIEW_FLIGHT;
MinerTab minerTab = MINER_TAB_A9;
BeaconMenuRow beaconMenuRow = BEACON_ROW_BUZZ;
CockpitView cockpitView = LOOK_FORWARD;
SpaceZone zone = ZONE_STATION;
NavTarget navTarget = NAV_NONE;
uint8_t currentGalaxy = 0;
uint8_t currentSystem = 7;   // Elite's iconic Lave slot in galaxy 1.
uint8_t targetSystem = 7;
uint8_t galaxyCursorSystem = 7;
uint8_t cosmosCacheCount = 0;
uint8_t cosmosMapPage = 0;
uint32_t knownCosmosCount = 0;
uint32_t knownCosmosBrightCount = 0;
uint32_t knownCosmosLastScanMs = 0;
uint32_t galaxySeedSignature = 0;

Body rocks[ROCK_COUNT];
Body enemies[ENEMY_COUNT];
Body traffic[TRAFFIC_COUNT];
Body miners[MINER_COUNT];
Star stars[STAR_COUNT];
EliteSystem galaxySystems[GALAXY_SYSTEM_COUNT];
CosmosLandmark cosmosCache[COSMOS_CACHE_MAX];
uint8_t BUZZ_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

Vec3 shipRightVec = {1.0f, 0.0f, 0.0f};
Vec3 shipUpVec = {0.0f, 1.0f, 0.0f};
Vec3 shipForwardVec = {0.0f, 0.0f, 1.0f};
float shipYaw = 0.0f;
float shipPitch = 0.0f;
float shipRoll = 0.0f;
float shipYawRate = 0.0f;
float shipPitchRate = 0.0f;
float shipRollRate = 0.0f;
float shipSpeed = 0.08f;
float shipX = 0.0f;
float shipY = 0.0f;
float shipZ = -360.0f;
float shieldPct = 100.0f;
float hullPct = 100.0f;
float energyPct = 100.0f;
float fuelPct = 86.0f;
float laserHeat = 0.0f;
float reactorLoad = 0.32f;
float stationDistance = 360.0f;
float navDistance = 0.0f;
float pirateDenDistance = 0.0f;
const Vec3 stationWorld = {0.0f, 0.0f, 0.0f};
const Vec3 beltWorld = {0.0f, 0.0f, 3600.0f};
const Vec3 pirateDenWorld = {720.0f, -90.0f, 4320.0f};

uint32_t credits = 1000;
uint16_t ore = 0;
uint16_t kills = 0;
uint8_t cargoMax = 12;
uint8_t brightnessLevels[] = {0, 8, 16, 28, 44, 64, 90, 125, 170, 220};
uint8_t brightnessIndex = 6;
uint8_t gameVolume = 96;
uint8_t powerMode = 0; // 0 balanced, 1 systems, 2 weapons, 3 engines
StationTab stationTab = ST_TAB_DOCK;
uint8_t stationRow = 0;
int8_t factionRep[FACTION_COUNT] = {12, 0, -8};
Faction stationFaction = FAC_ZOVEON_LEAGUE;

bool docked = false;
bool dockingRun = false;
bool cruiseMode = false;
bool shipLedEnabled = true;
bool envSensorsEnabled = true;
bool advSkyAnchorEnabled = true;
bool advCapGnssStarted = false;
bool advCapGnssFix = false;
bool advCapLoRaReady = false;
bool advCapLoRaEnabled = true;
bool advCapLoRaTxActive = false;
bool pirateDenKnown = false;
bool escortMission = false;
bool buzzEspNowReady = false;
bool buzzMinerEnabled = true;
bool buzzNightMode = true;
bool buzzLegacyA9Mode = true;
BuzzRemoteJobState buzzJob;
uint32_t dockingStartMs = 0;
uint32_t lastLogicMs = 0;
uint32_t lastFrameMs = 0;
uint32_t lastKeyMs = 0;
uint32_t lastHeldMs = 0;
uint32_t lastStationNavMs = 0;
uint32_t lastMinerNavMs = 0;
uint32_t lastLedMs = 0;
uint32_t hitFlashUntilMs = 0;
uint32_t lastEnemyShotMs = 0;
uint32_t lastEncounterMs = 0;
uint32_t shieldOfflineUntilMs = 0;
uint32_t buzzSeq = 0;
uint32_t buzzLastHeartbeatMs = 0;
uint32_t swarmSenseSeq = 0;
uint32_t swarmSenseLastTxMs = 0;
uint32_t swarmSenseTxOk = 0;
uint32_t swarmSenseTxFail = 0;
uint32_t beaconEnvSeq = 0;
uint32_t beaconEnvLastTxMs = 0;
uint32_t beaconEnvTxOk = 0;
uint32_t beaconEnvTxFail = 0;
uint32_t pilotLinkSeq = 0;
uint32_t pilotLinkLastTxMs = 0;
uint32_t pilotLinkTxOk = 0;
uint32_t pilotLinkTxFail = 0;
uint32_t advSkySeq = 0;
uint32_t advSkyLastTxMs = 0;
uint32_t advSkyLastHealthMs = 0;
uint32_t advSkyTxOk = 0;
uint32_t advSkyTxFail = 0;
uint32_t advSkyPrevHash = 0xA9A9C0DEUL;
uint32_t advCapLastGnssReadMs = 0;
uint32_t advCapLastFixMs = 0;
uint32_t advCapLastLoRaTxMs = 0;
uint32_t advCapLoRaTxStartMs = 0;
uint32_t advCapLoRaTxOk = 0;
uint32_t advCapLoRaTxFail = 0;
uint32_t advCapGeoSig = 0;
uint32_t buzzLastHashTickMs = 0;
uint32_t buzzLastDebugMs = 0;
uint32_t buzzLastMasterMs = 0;
uint32_t buzzHashCounter = 0;
uint32_t buzzHashRate = 0;
uint32_t buzzTotalHashes = 0;
uint32_t buzzShares = 0;
uint32_t buzzShareFlashUntilMs = 0;
uint32_t buzzLastShareNonce = 0;
uint32_t buzzRejects = 0;
uint32_t buzzJobsSeen = 0;
uint32_t buzzJobsDone = 0;
uint32_t buzzJobsExpired = 0;
uint32_t buzzJobsDeferred = 0;
uint32_t buzzJobsReplaced = 0;
uint32_t buzzRxSeen = 0;
uint32_t buzzRxJobs = 0;
uint32_t buzzRxRewards = 0;
uint32_t buzzTxOk = 0;
uint32_t buzzTxFail = 0;
uint32_t buzzTxFailStreak = 0;
uint32_t buzzEspNowRescues = 0;
uint32_t buzzLastEspNowRescueMs = 0;
uint32_t buzzBestBits = 0;
uint32_t buzzBestNonce = 0;
uint32_t buzzLaneSwitches = 0;
uint32_t buzzNear20 = 0;
uint32_t buzzNear21 = 0;
uint32_t buzzNear22 = 0;
uint32_t buzzTargetCompareFails = 0;
uint32_t buzzAgentEntropySeed = 0xC4111903UL;
uint32_t buzzA9StrategyHashes[BUZZ_A9_STRATEGY_COUNT] = {};
uint32_t buzzA9StrategyBest[BUZZ_A9_STRATEGY_COUNT] = {};
uint32_t buzzA9StrategyShares[BUZZ_A9_STRATEGY_COUNT] = {};
uint32_t buzzA9SectorHashes[BUZZ_MINER_SECTORS] = {};
uint32_t buzzA9SectorBest[BUZZ_MINER_SECTORS] = {};
uint32_t buzzA9SectorShares[BUZZ_MINER_SECTORS] = {};
uint32_t buzzA9StrideHashes[BUZZ_A9_STRIDE_ARM_COUNT] = {};
uint32_t buzzA9StrideBest[BUZZ_A9_STRIDE_ARM_COUNT] = {};
uint32_t buzzA9StrideShares[BUZZ_A9_STRIDE_ARM_COUNT] = {};
uint32_t buzzA9BroadHashes = 0;
uint32_t buzzA9BroadBest = 0;
uint32_t buzzA9BroadShares = 0;
uint32_t buzzA9RandomHashes = 0;
uint32_t buzzA9RandomBest = 0;
uint32_t buzzA9RandomShares = 0;
uint32_t buzzCorpusHashes = 0;
uint32_t buzzCorpusShares = 0;
uint32_t buzzCorpusBestBits = 0;
uint32_t buzzCorpusBoots = 0;
uint32_t buzzCorpusSaves = 0;
uint32_t buzzCorpusPrunes = 0;
uint32_t buzzCorpusDirtyHashes = 0;
uint32_t buzzLastCorpusSaveMs = 0;
uint32_t buzzLastCorpusPruneMs = 0;
uint16_t buzzWorkerId = 0;
uint16_t buzzTargetBits = 0;
uint16_t buzzAgentBatch = BUZZ_BATCH_NIGHT;
uint16_t buzzMiningBatch = BUZZ_BATCH_NIGHT;
uint8_t buzzAgentHint = 1;
uint8_t buzzAgentLevel = 0;
int8_t buzzLastRssi = -127;
int buzzLastTxErr = 0;
float buzzHashRateEma = 0.0f;
float buzzAgentScore = 0.0f;
float buzzAgentPredH = 0.0f;
float buzzAgentErr = 0.0f;
float advCapSkyLock = 0.0f;
float advCapLat = 0.0f;
float advCapLng = 0.0f;
float advCapCourseDeg = 0.0f;
float advCapSpeedKmph = 0.0f;
float advCapHdop = 99.9f;
uint16_t advCapSatellites = 0;
int16_t advCapLoRaLastState = 0;
bool envShtReady = false;
bool envQmpReady = false;
float envTempC = 0.0f;
float envHumidity = 0.0f;
float envPredTempC = 0.0f;
float envPredHumidity = 0.0f;
float envPressureHpa = 0.0f;
float envPredPressureHpa = 0.0f;
float envPressureLoss = 0.0f;
uint32_t envLastReadMs = 0;
uint32_t envShtReads = 0;
uint32_t envQmpReads = 0;
uint32_t envShtLastOkMs = 0;
uint32_t envQmpLastOkMs = 0;
uint8_t lastLedR = 255;
uint8_t lastLedG = 255;
uint8_t lastLedB = 255;
bool ledColorValid = false;
bool imuReady = false;
bool imuAssistEnabled = true;
bool buzzCorpusSdReady = false;
bool buzzCorpusSdTried = false;
bool buzzCorpusLoaded = false;
bool buzzCorpusDirty = false;
bool buzzCorpusNvsFallback = false;
float imuBiasGx = 0.0f;
float imuBiasGy = 0.0f;
float imuBiasGz = 0.0f;
float imuFiltYaw = 0.0f;
float imuFiltPitch = 0.0f;
uint8_t laserFlash = 0;
int16_t laserBeamX = SCREEN_W / 2;
int16_t laserBeamY = SCREEN_H / 2;
bool laserBeamHit = false;
uint8_t alertFlash = 0;
HitSide lastHitSide = HIT_FRONT;

String statusLine = "Ruchnoi polet. AI OFF.";

TinyGPSPlus advCapGps;

#if ADV_CAP_LORA_ENABLE
SX1262 advCapRadio = new Module(ADV_CAP_LORA_NSS_PIN, ADV_CAP_LORA_IRQ_PIN, ADV_CAP_LORA_RST_PIN, ADV_CAP_LORA_BUSY_PIN);
volatile bool advCapLoRaDone = false;
#endif

float clampf(float v, float lo, float hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

float throttlePercent() {
  if (shipSpeed >= 0.0f) return clampf(shipSpeed * 100.0f / THROTTLE_FORWARD_MAX, 0.0f, 100.0f);
  return -clampf(fabsf(shipSpeed) * 100.0f / fabsf(THROTTLE_REVERSE_MAX), 0.0f, 100.0f);
}

float throttleAbsPercent() {
  return fabsf(throttlePercent());
}

int clampi(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

uint32_t janusMix32(uint32_t h, uint32_t v) {
  h ^= v + 0x9E3779B9UL + (h << 6) + (h >> 2);
  h ^= h >> 16;
  h *= 0x7FEB352DUL;
  h ^= h >> 15;
  h *= 0x846CA68BUL;
  h ^= h >> 16;
  return h;
}

float frandRange(float lo, float hi) {
  return lo + (hi - lo) * ((float)random(0, 10000) / 10000.0f);
}

float wrapAngle(float a) {
  while (a > PI) a -= TWO_PI;
  while (a < -PI) a += TWO_PI;
  return a;
}

float dist3(float ax, float ay, float az, float bx, float by, float bz) {
  float dx = ax - bx;
  float dy = ay - by;
  float dz = az - bz;
  return sqrtf(dx * dx + dy * dy + dz * dz);
}

float dot3(const Vec3 &a, const Vec3 &b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

uint16_t buzzCountLeadingZeroBitsBE(const uint8_t h[32]) {
  uint16_t bits = 0;
  for (uint8_t i = 0; i < 32; ++i) {
    uint8_t v = h[i];
    if (v == 0) {
      bits += 8;
      continue;
    }
    for (int b = 7; b >= 0; --b) {
      if (v & (1U << b)) return bits;
      bits++;
    }
  }
  return bits;
}

void buzzWriteLE32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

void buzzDoubleSha256(const uint8_t *data, size_t len, uint8_t out[32]) {
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

void buzzHashToShareOrder(const uint8_t in[32], uint8_t out[32]) {
  for (uint8_t i = 0; i < 32; ++i) out[i] = in[31 - i];
}

bool buzzHashMeetsTargetBE(const uint8_t hash[32], const uint8_t target[32]) {
  for (uint8_t i = 0; i < 32; ++i) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;
}

uint32_t buzzBitReverse32(uint32_t x) {
  x = ((x & 0x55555555UL) << 1) | ((x >> 1) & 0x55555555UL);
  x = ((x & 0x33333333UL) << 2) | ((x >> 2) & 0x33333333UL);
  x = ((x & 0x0F0F0F0FUL) << 4) | ((x >> 4) & 0x0F0F0F0FUL);
  x = ((x & 0x00FF00FFUL) << 8) | ((x >> 8) & 0x00FF00FFUL);
  return (x << 16) | (x >> 16);
}

uint32_t buzzXorShift32(uint32_t x) {
  if (!x) x = 0xA5A5A5A5UL;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

const char* buzzLaneName(uint8_t lane) {
  switch (lane) {
    case A9_LANE_DUAL_LOCK: return "dual_lock";
    case A9_LANE_ZIM_S6: return "zim_reverse_s6";
    case A9_LANE_RANDOM_BASELINE: return "random_baseline";
    case A9_LANE_EXPLOIT_BEST: return "exploit_best";
    case A9_LANE_SURVIVE_LINEAR: return "survive_linear";
    default: return "janus_dispatcher";
  }
}

const char* buzzStrategyName(uint8_t strategy) {
  switch (strategy) {
    case A9_ZIM_REVERSE: return "zim_reverse";
    case A9_ZIM_BANDIT: return "zim_bandit";
    case A9_RANDOM: return "random";
    case A9_JANUS: return "janus";
    case A9_KNIGHT: return "knight";
    case A9_BITREV: return "bitrev";
    default: return "linear";
  }
}

uint32_t buzzStrideArmValue(uint8_t arm) {
  static const uint32_t arms[] = {
    1UL, 3UL, 5UL, 7UL, 11UL, 17UL, 29UL, 31UL, 53UL, 97UL, 257UL, 521UL,
    4099UL, 65537UL, 0x9E3779B9UL, 0xC4111903UL, 0x4F1BBCDDUL
  };
  return arms[arm % (sizeof(arms) / sizeof(arms[0]))] | 1UL;
}

uint32_t buzzA9Score(uint32_t hashes, uint32_t best, uint32_t shares) {
  uint32_t sample = min(hashes / 2048UL, 500UL);
  uint32_t score = best * 72UL + shares * 900UL + sample;
  return score;
}

uint8_t buzzA9TopStrategy() {
  uint8_t bestIdx = A9_ZIM_REVERSE;
  uint32_t bestScore = 0;
  for (uint8_t i = 0; i < BUZZ_A9_STRATEGY_COUNT; ++i) {
    uint32_t s = buzzA9Score(buzzA9StrategyHashes[i], buzzA9StrategyBest[i], buzzA9StrategyShares[i]);
    if (s > bestScore) {
      bestScore = s;
      bestIdx = i;
    }
  }
  return bestIdx;
}

uint8_t buzzA9TopSector() {
  uint8_t bestIdx = 6;
  uint32_t bestScore = 0;
  for (uint8_t i = 0; i < BUZZ_MINER_SECTORS; ++i) {
    uint32_t s = buzzA9Score(buzzA9SectorHashes[i], buzzA9SectorBest[i], buzzA9SectorShares[i]);
    if (s > bestScore) {
      bestScore = s;
      bestIdx = i;
    }
  }
  return bestIdx;
}

uint8_t buzzA9PickStrideArm(uint32_t seed) {
  if ((seed & 0x03) == 0) return (uint8_t)(seed % BUZZ_A9_STRIDE_ARM_COUNT);
  uint8_t bestIdx = 0;
  uint32_t bestScore = 0;
  for (uint8_t i = 0; i < BUZZ_A9_STRIDE_ARM_COUNT; ++i) {
    uint32_t s = buzzA9Score(buzzA9StrideHashes[i], buzzA9StrideBest[i], buzzA9StrideShares[i]);
    if (s > bestScore) {
      bestScore = s;
      bestIdx = i;
    }
  }
  return bestIdx;
}

uint64_t buzzA9XorShift64Star(uint64_t x) {
  if (!x) x = 0x9E3779B97F4A7C15ULL;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  return x * 0x2545F4914F6CDD1DULL;
}

uint32_t buzzA9RangeMod(uint64_t v, uint32_t range) {
  if (!range) return 0;
  return (uint32_t)(v % (uint64_t)range);
}

uint32_t buzzA9SectorOffset(uint32_t range, uint8_t sector) {
  if (!range) return 0;
  sector %= BUZZ_MINER_SECTORS;
  return (uint32_t)(((uint64_t)range * (uint64_t)sector) / (uint64_t)BUZZ_MINER_SECTORS);
}

uint8_t buzzA9BroadStrategy(uint32_t seed) {
  switch (seed % 7UL) {
    case 0: return A9_ZIM_REVERSE;
    case 1: return A9_ZIM_BANDIT;
    case 2: return A9_LINEAR;
    case 3: return A9_JANUS;
    case 4: return A9_KNIGHT;
    case 5: return A9_BITREV;
    default: return A9_RANDOM;
  }
}

uint32_t buzzCorpusChecksum(const A9CorpusBlob &blob) {
  const uint8_t *p = (const uint8_t*)&blob;
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < sizeof(A9CorpusBlob) - sizeof(blob.checksum); ++i) {
    h ^= p[i];
    h *= 16777619UL;
  }
  return h ^ 0xA9A9C016UL;
}

void buzzCorpusFillBlob(A9CorpusBlob &blob) {
  memset(&blob, 0, sizeof(blob));
  blob.magic = BUZZ_CORPUS_MAGIC;
  blob.version = BUZZ_CORPUS_VERSION;
  blob.size = sizeof(A9CorpusBlob);
  blob.savedAtMs = millis();
  blob.bootCount = buzzCorpusBoots;
  blob.saveCount = buzzCorpusSaves;
  blob.pruneCount = buzzCorpusPrunes;
  blob.corpusHashes = buzzCorpusHashes;
  blob.corpusShares = buzzCorpusShares;
  blob.corpusBestBits = buzzCorpusBestBits;
  blob.near20 = buzzNear20;
  blob.near21 = buzzNear21;
  blob.near22 = buzzNear22;
  blob.compareFails = buzzTargetCompareFails;
  memcpy(blob.strategyHashes, buzzA9StrategyHashes, sizeof(blob.strategyHashes));
  memcpy(blob.strategyBest, buzzA9StrategyBest, sizeof(blob.strategyBest));
  memcpy(blob.strategyShares, buzzA9StrategyShares, sizeof(blob.strategyShares));
  memcpy(blob.sectorHashes, buzzA9SectorHashes, sizeof(blob.sectorHashes));
  memcpy(blob.sectorBest, buzzA9SectorBest, sizeof(blob.sectorBest));
  memcpy(blob.sectorShares, buzzA9SectorShares, sizeof(blob.sectorShares));
  memcpy(blob.strideHashes, buzzA9StrideHashes, sizeof(blob.strideHashes));
  memcpy(blob.strideBest, buzzA9StrideBest, sizeof(blob.strideBest));
  memcpy(blob.strideShares, buzzA9StrideShares, sizeof(blob.strideShares));
  blob.broadHashes = buzzA9BroadHashes;
  blob.broadBest = buzzA9BroadBest;
  blob.broadShares = buzzA9BroadShares;
  blob.randomHashes = buzzA9RandomHashes;
  blob.randomBest = buzzA9RandomBest;
  blob.randomShares = buzzA9RandomShares;
  blob.checksum = buzzCorpusChecksum(blob);
}

bool buzzCorpusBlobValid(const A9CorpusBlob &blob) {
  if (blob.magic != BUZZ_CORPUS_MAGIC) return false;
  if (blob.version != BUZZ_CORPUS_VERSION) return false;
  if (blob.size != sizeof(A9CorpusBlob)) return false;
  return blob.checksum == buzzCorpusChecksum(blob);
}

bool buzzCorpusApplyBlob(const A9CorpusBlob &blob) {
  if (!buzzCorpusBlobValid(blob)) return false;
  buzzCorpusBoots = blob.bootCount;
  buzzCorpusSaves = blob.saveCount;
  buzzCorpusPrunes = blob.pruneCount;
  buzzCorpusHashes = blob.corpusHashes;
  buzzCorpusShares = blob.corpusShares;
  buzzCorpusBestBits = blob.corpusBestBits;
  buzzNear20 = blob.near20;
  buzzNear21 = blob.near21;
  buzzNear22 = blob.near22;
  buzzTargetCompareFails = blob.compareFails;
  memcpy(buzzA9StrategyHashes, blob.strategyHashes, sizeof(buzzA9StrategyHashes));
  memcpy(buzzA9StrategyBest, blob.strategyBest, sizeof(buzzA9StrategyBest));
  memcpy(buzzA9StrategyShares, blob.strategyShares, sizeof(buzzA9StrategyShares));
  memcpy(buzzA9SectorHashes, blob.sectorHashes, sizeof(buzzA9SectorHashes));
  memcpy(buzzA9SectorBest, blob.sectorBest, sizeof(buzzA9SectorBest));
  memcpy(buzzA9SectorShares, blob.sectorShares, sizeof(buzzA9SectorShares));
  memcpy(buzzA9StrideHashes, blob.strideHashes, sizeof(buzzA9StrideHashes));
  memcpy(buzzA9StrideBest, blob.strideBest, sizeof(buzzA9StrideBest));
  memcpy(buzzA9StrideShares, blob.strideShares, sizeof(buzzA9StrideShares));
  buzzA9BroadHashes = blob.broadHashes;
  buzzA9BroadBest = blob.broadBest;
  buzzA9BroadShares = blob.broadShares;
  buzzA9RandomHashes = blob.randomHashes;
  buzzA9RandomBest = blob.randomBest;
  buzzA9RandomShares = blob.randomShares;
  if (buzzCorpusBestBits > buzzBestBits) buzzBestBits = buzzCorpusBestBits;
  return true;
}

const char* buzzCorpusStoreName() {
  if (buzzCorpusNvsFallback) return "NVS";
  if (buzzCorpusSdReady) return "SD";
  return "--";
}

bool buzzCorpusEnsureSd(bool forceRetry) {
  if (buzzCorpusSdReady) return true;
  if (buzzCorpusSdTried && !forceRetry) return false;
  buzzCorpusSdTried = true;

  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);
  if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000U)) {
    Serial.println("[A9/CORPUS] SD begin failed");
    buzzCorpusSdReady = false;
    return false;
  }
  if (SD.cardType() == CARD_NONE) {
    Serial.println("[A9/CORPUS] SD card absent");
    buzzCorpusSdReady = false;
    return false;
  }
  if (!SD.exists("/janus")) SD.mkdir("/janus");
  if (!SD.exists(A9_CORPUS_DIR)) SD.mkdir(A9_CORPUS_DIR);
  buzzCorpusSdReady = true;
  Serial.printf("[A9/CORPUS] SD ready cs=%u size=%lluMB\n",
                (unsigned)SD_SPI_CS_PIN, (unsigned long long)(SD.cardSize() / (1024ULL * 1024ULL)));
  return true;
}

EliteSeed6 eliteBaseSeed() {
  // Bytes: 4A 5A 48 02 53 B7, stored as three little-endian 16-bit seeds.
  return {0x5A4A, 0x0248, 0xB753};
}

uint64_t eliteSeedTo48(const EliteSeed6 &s) {
  return ((uint64_t)s.w0) | ((uint64_t)s.w1 << 16) | ((uint64_t)s.w2 << 32);
}

EliteSeed6 eliteSeedFrom48(uint64_t v) {
  EliteSeed6 s{};
  s.w0 = (uint16_t)(v & 0xFFFFULL);
  s.w1 = (uint16_t)((v >> 16) & 0xFFFFULL);
  s.w2 = (uint16_t)((v >> 32) & 0xFFFFULL);
  return s;
}

EliteSeed6 eliteGalaxySeed(uint8_t galaxyIndex) {
  uint64_t v = eliteSeedTo48(eliteBaseSeed()) & 0x0000FFFFFFFFFFFFULL;
  for (uint8_t i = 0; i < (galaxyIndex & 7); ++i) {
    v = ((v << 1) | (v >> 47)) & 0x0000FFFFFFFFFFFFULL;
  }
  return eliteSeedFrom48(v);
}

void eliteTwist(EliteSeed6 &s) {
  uint16_t t = (uint16_t)(s.w0 + s.w1 + s.w2);
  s.w0 = s.w1;
  s.w1 = s.w2;
  s.w2 = t;
}

uint8_t eliteLo(uint16_t w) { return (uint8_t)(w & 0xFF); }
uint8_t eliteHi(uint16_t w) { return (uint8_t)(w >> 8); }

const char* eliteNamePair(uint8_t idx) {
  static const char* pairs[32] = {
    "", "AN", "XE", "GE", "ZA", "CE", "BI", "SO",
    "US", "ES", "AR", "MA", "IN", "DI", "RE", "A",
    "ER", "AT", "EN", "BE", "RA", "LA", "VE", "TI",
    "ED", "OR", "QU", "AN", "TE", "IS", "RI", "ON"
  };
  return pairs[idx & 31];
}

void eliteMakeName(const EliteSeed6 &seed, char *out, size_t outLen) {
  if (!out || outLen == 0) return;
  out[0] = '\0';
  EliteSeed6 s = seed;
  uint8_t pairs = (eliteLo(seed.w0) & 0x40) ? 4 : 3;
  for (uint8_t i = 0; i < pairs; ++i) {
    uint8_t token = eliteHi(s.w2) & 31;
    const char *p = eliteNamePair(token);
    if (p[0]) strlcat(out, p, outLen);
    eliteTwist(s);
  }
  if (!out[0]) strlcpy(out, "RA", outLen);
  for (char *p = out; *p; ++p) *p = (char)tolower((unsigned char)*p);
  out[0] = (char)toupper((unsigned char)out[0]);
}

uint32_t eliteSystemSignature(const EliteSeed6 &seed, uint8_t galaxyIndex, uint8_t sysIndex) {
  uint32_t h = 0xE117E198UL;
  h = janusMix32(h, seed.w0);
  h = janusMix32(h, seed.w1);
  h = janusMix32(h, seed.w2);
  h = janusMix32(h, ((uint32_t)galaxyIndex << 8) | sysIndex);
  return h;
}

void eliteFillSystem(uint8_t galaxyIndex, uint8_t sysIndex, EliteSeed6 seed, EliteSystem &sys) {
  eliteMakeName(seed, sys.name, sizeof(sys.name));
  sys.x = eliteHi(seed.w1);
  sys.y = eliteHi(seed.w0);
  sys.economy = (eliteHi(seed.w0) >> 1) & 7;
  sys.government = (eliteHi(seed.w1) >> 3) & 7;
  sys.techLevel = 1 + ((sys.economy ^ 7) & 7) + (sys.government >> 1) + ((eliteLo(seed.w2) >> 6) & 3);
  if (sys.techLevel > 15) sys.techLevel = 15;
  sys.danger = (uint8_t)clampi((7 - sys.government) + (sys.economy < 3 ? 1 : 0), 0, 9);
  sys.population = (uint8_t)clampi((sys.techLevel * 3) + sys.economy + sys.government + 1, 1, 99);
  sys.radius = (uint16_t)(256 + (((uint16_t)eliteLo(seed.w2) << 1) | (eliteHi(seed.w2) & 1)));
  sys.signature = eliteSystemSignature(seed, galaxyIndex, sysIndex);
}

void eliteGenerateGalaxy(uint8_t galaxyIndex) {
  EliteSeed6 s = eliteGalaxySeed(galaxyIndex);
  galaxySeedSignature = (uint32_t)eliteSeedTo48(s) ^ (uint32_t)(eliteSeedTo48(s) >> 24);
  for (uint16_t i = 0; i < GALAXY_SYSTEM_COUNT; ++i) {
    eliteFillSystem(galaxyIndex, (uint8_t)i, s, galaxySystems[i]);
    eliteTwist(s);
    eliteTwist(s);
    eliteTwist(s);
    eliteTwist(s);
  }
}

uint16_t eliteDistanceSystems(uint8_t a, uint8_t b) {
  const EliteSystem &sa = galaxySystems[a];
  const EliteSystem &sb = galaxySystems[b];
  int dx = (int)sa.x - (int)sb.x;
  int dy = (int)sa.y - (int)sb.y;
  return (uint16_t)sqrtf((float)(dx * dx + dy * dy));
}

uint16_t eliteJumpRangeNow() {
  uint16_t range = GALAXY_BASE_JUMP_RANGE;
  range += (uint16_t)clampi((cargoMax - 12) * 2, 0, 36);
  if (advCapGnssFreshNow(millis())) range += 6;
  if (advCapSkyLock > 0.70f) range += 6;
  if (buzzBestBits > buzzTargetBits) range += (uint16_t)min(10UL, buzzBestBits - buzzTargetBits);
  return range;
}

bool eliteTargetReachable(uint8_t sys) {
  return eliteDistanceSystems(currentSystem, sys) <= eliteJumpRangeNow();
}

uint8_t eliteNearestInterestingSystem(uint8_t from, uint32_t seed) {
  uint8_t best = from;
  int bestScore = -32768;
  for (uint16_t i = 0; i < GALAXY_SYSTEM_COUNT; ++i) {
    if (i == from) continue;
    uint16_t d = eliteDistanceSystems(from, (uint8_t)i);
    if (d > eliteJumpRangeNow()) continue;
    const EliteSystem &s = galaxySystems[i];
    int score = (int)s.techLevel * 11 - (int)s.danger * 9 - (int)d;
    score += (int)(janusMix32(seed, s.signature) & 31UL);
    if (score > bestScore) {
      bestScore = score;
      best = (uint8_t)i;
    }
  }
  return best;
}

const char* cosmosTypeName(uint8_t type) {
  switch (type) {
    case COSMOS_PULSAR: return "PULSAR";
    case COSMOS_BLACK_HOLE: return "BH";
    case COSMOS_NEBULA: return "NEB";
    case COSMOS_GALAXY: return "GALAXY";
    case COSMOS_LAB: return "LAB";
    default: return "STAR";
  }
}

void knownCosmosFallback() {
  static const CosmosLandmark builtins[] = {
    {"SOL",0,132,96,COSMOS_STAR,1,70,900},
    {"SIRIUS",0,126,87,COSMOS_STAR,1,74,830},
    {"VEGA",0,58,45,COSMOS_STAR,1,78,760},
    {"BETELGEUSE",0,34,69,COSMOS_STAR,3,70,700},
    {"RIGEL",0,28,101,COSMOS_STAR,3,72,690},
    {"POLARIS",0,72,18,COSMOS_STAR,1,80,640},
    {"CRAB-PSR",0,88,74,COSMOS_PULSAR,5,95,980},
    {"VELA-PSR",0,154,188,COSMOS_PULSAR,6,93,960},
    {"SGR-A",0,202,130,COSMOS_BLACK_HOLE,9,100,1200},
    {"GARGANTUA",0,214,119,COSMOS_LAB,8,100,1300},
    {"CYGNUS-X1",0,70,52,COSMOS_BLACK_HOLE,8,96,1040},
    {"M31",0,12,24,COSMOS_GALAXY,4,90,860},
    {"ORION-NEB",0,38,112,COSMOS_NEBULA,2,88,780},
    {"LMC",0,184,224,COSMOS_GALAXY,4,82,820},
    {"SMC",0,198,232,COSMOS_GALAXY,4,82,810},
    {"J0437-4715",0,176,210,COSMOS_PULSAR,5,96,940},
    {"M87",0,224,44,COSMOS_BLACK_HOLE,9,100,1100},
    {"WOLF359",0,124,92,COSMOS_STAR,2,68,620}
  };
  cosmosCacheCount = min((size_t)COSMOS_CACHE_MAX, sizeof(builtins) / sizeof(builtins[0]));
  memcpy(cosmosCache, builtins, sizeof(CosmosLandmark) * cosmosCacheCount);
  knownCosmosCount = cosmosCacheCount;
  knownCosmosBrightCount = cosmosCacheCount;
}

void knownCosmosScanSd(bool force) {
  uint32_t now = millis();
  if (!force && knownCosmosLastScanMs && now - knownCosmosLastScanMs < 60000UL) return;
  knownCosmosLastScanMs = now;
  knownCosmosCount = 0;
  knownCosmosBrightCount = 0;
  cosmosCacheCount = 0;

  if (!buzzCorpusEnsureSd(false)) {
    knownCosmosFallback();
    return;
  }
  File f = SD.open(KNOWN_COSMOS_FILE, FILE_READ);
  if (!f) {
    knownCosmosFallback();
    Serial.printf("[CARD/COSMOS] no %s, fallback landmarks=%u\n", KNOWN_COSMOS_FILE, (unsigned)cosmosCacheCount);
    return;
  }

  char line[128];
  uint16_t pos = 0;
  while (f.available()) {
    char c = (char)f.read();
    if (c == '\r') continue;
    if (c != '\n' && pos < sizeof(line) - 1) {
      line[pos++] = c;
      continue;
    }
    line[pos] = '\0';
    pos = 0;
    if (!line[0] || line[0] == '#') continue;

    char *fields[8] = {};
    uint8_t n = 0;
    char *ctx = nullptr;
    for (char *p = strtok_r(line, ",", &ctx); p && n < 8; p = strtok_r(nullptr, ",", &ctx)) fields[n++] = p;
    if (n < 4) continue;
    knownCosmosCount++;
    int type = atoi(fields[1]);
    int x = atoi(fields[2]);
    int y = atoi(fields[3]);
    int science = (n > 4) ? atoi(fields[4]) : 60;
    int danger = (n > 5) ? atoi(fields[5]) : (type == COSMOS_BLACK_HOLE ? 8 : 2);
    int influence = (n > 6) ? atoi(fields[6]) : (science * 10);
    bool important = (type == COSMOS_PULSAR || type == COSMOS_BLACK_HOLE || type == COSMOS_LAB || science >= 80 || influence >= 800);
    if (important) knownCosmosBrightCount++;
    if (important && cosmosCacheCount < COSMOS_CACHE_MAX) {
      CosmosLandmark &lm = cosmosCache[cosmosCacheCount++];
      memset(&lm, 0, sizeof(lm));
      strlcpy(lm.name, fields[0], sizeof(lm.name));
      lm.galaxy = currentGalaxy;
      lm.type = (uint8_t)clampi(type, 0, 255);
      lm.x = (uint8_t)clampi(x, 0, 255);
      lm.y = (uint8_t)clampi(y, 0, 255);
      lm.science = (uint8_t)clampi(science, 0, 100);
      lm.danger = (uint8_t)clampi(danger, 0, 9);
      lm.influence = (uint16_t)clampi(influence, 0, 65535);
    }
  }
  f.close();
  if (cosmosCacheCount == 0) knownCosmosFallback();
  Serial.printf("[CARD/COSMOS] known=%lu bright=%lu cache=%u file=%s\n",
                (unsigned long)knownCosmosCount, (unsigned long)knownCosmosBrightCount,
                (unsigned)cosmosCacheCount, KNOWN_COSMOS_FILE);
}

void galaxyInit() {
  currentGalaxy &= 7;
  eliteGenerateGalaxy(currentGalaxy);
  currentSystem = 7;
  targetSystem = eliteNearestInterestingSystem(currentSystem, galaxySeedSignature);
  galaxyCursorSystem = targetSystem;
  knownCosmosScanSd(true);
}

const EliteSystem& eliteCurrentSystem() {
  return galaxySystems[currentSystem];
}

const EliteSystem& eliteTargetSystem() {
  return galaxySystems[targetSystem];
}

const EliteSystem& eliteCursorSystem() {
  return galaxySystems[galaxyCursorSystem];
}

const char* eliteEconomyName(uint8_t e) {
  static const char* names[8] = {"AGRI", "AGRI+", "MIX", "IND-", "IND", "TECH", "HIGH", "CORE"};
  return names[e & 7];
}

const char* eliteGovernmentName(uint8_t g) {
  static const char* names[8] = {"ANAR", "FEUD", "MULTI", "DICT", "COMM", "CONF", "CORP", "DEMO"};
  return names[g & 7];
}

void galaxyApplySystemContext() {
  const EliteSystem &sys = eliteCurrentSystem();
  if (sys.government <= 1) stationFaction = FAC_MACHINE_REMNANTS;
  else if (sys.economy <= 2) stationFaction = FAC_FREE_MINERS;
  else stationFaction = FAC_ZOVEON_LEAGUE;
  pirateDenKnown = sys.danger >= 6;
}

uint32_t eliteOreUnitPrice() {
  const EliteSystem &sys = eliteCurrentSystem();
  int price = 28 + (int)sys.techLevel * 3 + (int)sys.danger * 2;
  if (sys.economy >= 4) price += 14;
  if (sys.economy <= 1) price -= 8;
  return (uint32_t)clampi(price, 18, 96);
}

uint32_t eliteFuelUnitPrice() {
  const EliteSystem &sys = eliteCurrentSystem();
  int price = 18 + (7 - (int)sys.economy) * 4 + (int)sys.danger * 2 - (int)sys.techLevel;
  return (uint32_t)clampi(price, 12, 72);
}

uint8_t eliteJumpFuelCost(uint8_t sys) {
  uint16_t d = eliteDistanceSystems(currentSystem, sys);
  uint32_t range = (uint32_t)eliteJumpRangeNow();
  if (range < 1UL) range = 1UL;
  int cost = 5 + (int)(d * 52UL / range);
  cost += galaxySystems[sys].danger;
  return (uint8_t)clampi(cost, 6, 68);
}

const CosmosLandmark* nearestCosmosLandmark(uint8_t sysIndex) {
  if (cosmosCacheCount == 0) return nullptr;
  const EliteSystem &sys = galaxySystems[sysIndex];
  const CosmosLandmark *best = nullptr;
  uint32_t bestScore = 0xFFFFFFFFUL;
  for (uint8_t i = 0; i < cosmosCacheCount; ++i) {
    const CosmosLandmark &lm = cosmosCache[i];
    if ((lm.galaxy & 7) != currentGalaxy) continue;
    int dx = (int)sys.x - (int)lm.x;
    int dy = (int)sys.y - (int)lm.y;
    uint32_t d2 = (uint32_t)(dx * dx + dy * dy);
    uint32_t score = (d2 << 1) + (uint32_t)max(0, 100 - (int)lm.science);
    if (score < bestScore) {
      bestScore = score;
      best = &lm;
    }
  }
  return best;
}

void eliteResetLocalSpaceForSystem() {
  docked = true;
  dockingRun = false;
  cruiseMode = false;
  viewMode = VIEW_DOCKED;
  cockpitView = LOOK_FORWARD;
  navTarget = NAV_NONE;
  zone = ZONE_STATION;
  shipX = stationWorld.x;
  shipY = stationWorld.y;
  shipZ = stationWorld.z - 80.0f;
  shipSpeed = 0.0f;
  resetShipOrientation();
  updateZoneByPosition();
  clearBodies();
  resetStars();
  applyDockServices(false);
}

bool eliteExecuteHyperspace(uint8_t sys) {
  if (sys == currentSystem) {
    statusLine = "My uzhe zdes";
    return false;
  }
  if (!eliteTargetReachable(sys)) {
    statusLine = "Skachok daleko: nuzhen drive";
    return false;
  }
  uint8_t cost = eliteJumpFuelCost(sys);
  if (fuelPct < (float)cost) {
    statusLine = String("Topliva nado ") + String((unsigned)cost) + "%";
    return false;
  }
  fuelPct = clampf(fuelPct - (float)cost, 0.0f, 100.0f);
  currentSystem = sys;
  targetSystem = eliteNearestInterestingSystem(currentSystem, janusMix32(galaxySeedSignature, sys));
  galaxyCursorSystem = currentSystem;
  galaxyApplySystemContext();
  eliteResetLocalSpaceForSystem();
  const EliteSystem &cur = eliteCurrentSystem();
  statusLine = String("Pribytie ") + cur.name;
  Serial.printf("[CARD/GALAXY] jump g=%u sys=%u name=%s fuelCost=%u econ=%s gov=%s tech=%u danger=%u\n",
                (unsigned)currentGalaxy + 1U, (unsigned)currentSystem, cur.name,
                (unsigned)cost, eliteEconomyName(cur.economy), eliteGovernmentName(cur.government),
                (unsigned)cur.techLevel, (unsigned)cur.danger);
  return true;
}

bool buzzCorpusReadBlobFromSd(const char *path, A9CorpusBlob &blob) {
  File f = SD.open(path, FILE_READ);
  if (!f) return false;
  if (f.size() != sizeof(A9CorpusBlob)) {
    f.close();
    return false;
  }
  size_t n = f.read((uint8_t*)&blob, sizeof(blob));
  f.close();
  return n == sizeof(blob) && buzzCorpusBlobValid(blob);
}

bool buzzCorpusLoadFromSd() {
  if (!buzzCorpusEnsureSd(false)) return false;
  A9CorpusBlob blob;
  if (buzzCorpusReadBlobFromSd(A9_CORPUS_FILE, blob) || buzzCorpusReadBlobFromSd(A9_CORPUS_BAK_FILE, blob)) {
    buzzCorpusLoaded = buzzCorpusApplyBlob(blob);
    buzzCorpusNvsFallback = false;
    return buzzCorpusLoaded;
  }
  return false;
}

bool buzzCorpusSaveToSd() {
  if (!buzzCorpusEnsureSd(false)) return false;
  A9CorpusBlob blob;
  buzzCorpusFillBlob(blob);
  if (SD.exists(A9_CORPUS_TMP_FILE)) SD.remove(A9_CORPUS_TMP_FILE);
  File f = SD.open(A9_CORPUS_TMP_FILE, FILE_WRITE);
  if (!f) return false;
  size_t n = f.write((const uint8_t*)&blob, sizeof(blob));
  f.flush();
  f.close();
  if (n != sizeof(blob)) {
    SD.remove(A9_CORPUS_TMP_FILE);
    return false;
  }
  if (SD.exists(A9_CORPUS_BAK_FILE)) SD.remove(A9_CORPUS_BAK_FILE);
  if (SD.exists(A9_CORPUS_FILE)) SD.rename(A9_CORPUS_FILE, A9_CORPUS_BAK_FILE);
  if (!SD.rename(A9_CORPUS_TMP_FILE, A9_CORPUS_FILE)) {
    SD.remove(A9_CORPUS_TMP_FILE);
    return false;
  }
  return true;
}

bool buzzCorpusLoadFromNvs() {
  Preferences prefs;
  if (!prefs.begin("janus_a9", true)) return false;
  size_t len = prefs.getBytesLength("blob");
  if (len != sizeof(A9CorpusBlob)) {
    prefs.end();
    return false;
  }
  A9CorpusBlob blob;
  size_t n = prefs.getBytes("blob", &blob, sizeof(blob));
  prefs.end();
  if (n != sizeof(blob)) return false;
  buzzCorpusLoaded = buzzCorpusApplyBlob(blob);
  buzzCorpusNvsFallback = buzzCorpusLoaded;
  return buzzCorpusLoaded;
}

bool buzzCorpusSaveToNvs() {
  Preferences prefs;
  if (!prefs.begin("janus_a9", false)) return false;
  A9CorpusBlob blob;
  buzzCorpusFillBlob(blob);
  size_t n = prefs.putBytes("blob", &blob, sizeof(blob));
  prefs.end();
  buzzCorpusNvsFallback = n == sizeof(blob);
  return buzzCorpusNvsFallback;
}

void buzzCorpusAppendLog(const char *eventName) {
  if (!buzzCorpusSdReady) return;
  File f = SD.open(A9_CORPUS_LOG_FILE, FILE_APPEND);
  if (!f) return;
  f.printf("{\"t\":%lu,\"event\":\"%s\",\"hashes\":%lu,\"best\":%lu,\"shares\":%lu,\"save\":%lu,\"prune\":%lu,\"near\":[%lu,%lu,%lu]}\n",
           (unsigned long)millis(), eventName,
           (unsigned long)buzzCorpusHashes, (unsigned long)buzzCorpusBestBits,
           (unsigned long)buzzCorpusShares, (unsigned long)buzzCorpusSaves,
           (unsigned long)buzzCorpusPrunes, (unsigned long)buzzNear20,
           (unsigned long)buzzNear21, (unsigned long)buzzNear22);
  bool rotate = f.size() > BUZZ_CORPUS_MAX_LOG_BYTES;
  f.close();
  if (rotate) {
    if (SD.exists(A9_CORPUS_LOG_OLD_FILE)) SD.remove(A9_CORPUS_LOG_OLD_FILE);
    SD.rename(A9_CORPUS_LOG_FILE, A9_CORPUS_LOG_OLD_FILE);
  }
}

bool buzzA9SaveCorpus(bool force) {
  uint32_t now = millis();
  if (!force && !buzzCorpusDirty) return true;
  if (!force && now - buzzLastCorpusSaveMs < BUZZ_CORPUS_SAVE_MS) return true;

  buzzCorpusSaves++;
  if (force) buzzCorpusEnsureSd(true);
  bool savedSd = buzzCorpusSaveToSd();
  bool savedNvs = savedSd ? true : buzzCorpusSaveToNvs();
  if (savedSd || savedNvs) {
    if (savedSd) buzzCorpusNvsFallback = false;
    buzzCorpusDirty = false;
    buzzCorpusDirtyHashes = 0;
    buzzLastCorpusSaveMs = now;
    buzzCorpusAppendLog(savedSd ? "save_sd" : "save_nvs");
    Serial.printf("[A9/CORPUS] save sd=%u nvs=%u hashes=%lu best=%lu shares=%lu saves=%lu prunes=%lu\n",
                  savedSd ? 1 : 0, (!savedSd && savedNvs) ? 1 : 0,
                  (unsigned long)buzzCorpusHashes, (unsigned long)buzzCorpusBestBits,
                  (unsigned long)buzzCorpusShares, (unsigned long)buzzCorpusSaves,
                  (unsigned long)buzzCorpusPrunes);
    return true;
  }

  if (buzzCorpusSaves > 0) buzzCorpusSaves--;
  Serial.println("[A9/CORPUS] save failed");
  return false;
}

bool buzzA9LoadCorpus() {
  bool loadedSd = buzzCorpusLoadFromSd();
  bool loadedNvs = loadedSd ? false : buzzCorpusLoadFromNvs();
  buzzCorpusLoaded = loadedSd || loadedNvs;
  buzzCorpusBoots++;
  buzzAgentEntropySeed ^= buzzCorpusBestBits * 0x45D9F3BUL;
  buzzAgentEntropySeed ^= buzzCorpusBoots * 0x9E3779B9UL;
  buzzCorpusDirty = true;
  buzzLastCorpusSaveMs = millis();
  buzzLastCorpusPruneMs = millis();
  Serial.printf("[A9/CORPUS] boot sd=%u loaded=%u src=%s hashes=%lu best=%lu shares=%lu boots=%lu\n",
                buzzCorpusSdReady ? 1 : 0, buzzCorpusLoaded ? 1 : 0,
                loadedSd ? "SD" : (loadedNvs ? "NVS" : "--"),
                (unsigned long)buzzCorpusHashes, (unsigned long)buzzCorpusBestBits,
                (unsigned long)buzzCorpusShares, (unsigned long)buzzCorpusBoots);
  return buzzCorpusLoaded;
}

uint32_t buzzPruneTable(uint32_t *hashes, uint32_t *best, uint32_t *shares, uint8_t count, bool aggressive) {
  uint32_t removed = 0;
  uint32_t weakBits = buzzTargetBits > 3 ? (uint32_t)buzzTargetBits - (aggressive ? 1UL : 2UL) : 20UL;
  if (weakBits < 16UL) weakBits = 16UL;
  uint32_t minHashes = aggressive ? 4096UL : 32768UL;
  uint32_t staleHashes = aggressive ? 131072UL : 524288UL;

  for (uint8_t i = 0; i < count; ++i) {
    if (hashes[i] < minHashes) continue;
    bool weak = shares[i] == 0 && best[i] < weakBits;
    bool stale = shares[i] == 0 && hashes[i] > staleHashes && best[i] + 2UL < buzzCorpusBestBits;
    if (!weak && !stale) continue;

    uint32_t keep = aggressive ? hashes[i] / 5UL : hashes[i] / 2UL;
    if (keep < 512UL) keep = 0;
    removed += hashes[i] - keep;
    hashes[i] = keep;
    if (best[i] > (aggressive ? 2UL : 1UL)) best[i] -= aggressive ? 2UL : 1UL;
    else best[i] = 0;
  }
  return removed;
}

uint32_t buzzPruneScalar(uint32_t &hashes, uint32_t &best, uint32_t &shares, bool aggressive) {
  return buzzPruneTable(&hashes, &best, &shares, 1, aggressive);
}

uint32_t buzzA9PruneCorpus(bool aggressive) {
  uint32_t removed = 0;
  removed += buzzPruneTable(buzzA9StrategyHashes, buzzA9StrategyBest, buzzA9StrategyShares, BUZZ_A9_STRATEGY_COUNT, aggressive);
  removed += buzzPruneTable(buzzA9SectorHashes, buzzA9SectorBest, buzzA9SectorShares, BUZZ_MINER_SECTORS, aggressive);
  removed += buzzPruneTable(buzzA9StrideHashes, buzzA9StrideBest, buzzA9StrideShares, BUZZ_A9_STRIDE_ARM_COUNT, aggressive);
  removed += buzzPruneScalar(buzzA9BroadHashes, buzzA9BroadBest, buzzA9BroadShares, aggressive);
  removed += buzzPruneScalar(buzzA9RandomHashes, buzzA9RandomBest, buzzA9RandomShares, aggressive);

  if (removed > buzzCorpusHashes) buzzCorpusHashes = 0;
  else buzzCorpusHashes -= removed;
  buzzCorpusPrunes++;
  buzzCorpusDirty = true;
  buzzLastCorpusPruneMs = millis();
  if (buzzCorpusSdReady) {
    if (SD.exists(A9_CORPUS_TMP_FILE)) SD.remove(A9_CORPUS_TMP_FILE);
    if (aggressive && SD.exists(A9_CORPUS_LOG_OLD_FILE)) SD.remove(A9_CORPUS_LOG_OLD_FILE);
  }
  buzzCorpusAppendLog(aggressive ? "prune_manual" : "prune_auto");
  statusLine = removed ? String("A9 corpus prune -") + String((unsigned long)removed) : "A9 corpus clean";
  Serial.printf("[A9/CORPUS] prune aggressive=%u removed=%lu hashes=%lu best=%lu shares=%lu count=%lu\n",
                aggressive ? 1 : 0, (unsigned long)removed,
                (unsigned long)buzzCorpusHashes, (unsigned long)buzzCorpusBestBits,
                (unsigned long)buzzCorpusShares, (unsigned long)buzzCorpusPrunes);
  return removed;
}

void buzzCorpusTick(uint32_t now) {
  if (buzzCorpusDirty && now - buzzLastCorpusSaveMs >= BUZZ_CORPUS_SAVE_MS) buzzA9SaveCorpus(false);
  if (now - buzzLastCorpusPruneMs >= BUZZ_CORPUS_PRUNE_MS) {
    buzzA9PruneCorpus(false);
    buzzA9SaveCorpus(false);
  }
}

void buzzA9Observe(uint16_t bits, bool shareFound) {
  uint8_t st = buzzJob.minerStrategy % BUZZ_A9_STRATEGY_COUNT;
  uint8_t sec = buzzJob.minerSector % BUZZ_MINER_SECTORS;
  uint8_t arm = buzzJob.minerStrideArm % BUZZ_A9_STRIDE_ARM_COUNT;

  buzzCorpusHashes++;
  buzzCorpusDirtyHashes++;
  if (bits > buzzCorpusBestBits) {
    buzzCorpusBestBits = bits;
    buzzCorpusDirty = true;
  }
  if (bits >= 20 || buzzCorpusDirtyHashes >= 32768UL) buzzCorpusDirty = true;

  buzzA9StrategyHashes[st]++;
  buzzA9SectorHashes[sec]++;
  buzzA9StrideHashes[arm]++;
  if (bits > buzzA9StrategyBest[st]) buzzA9StrategyBest[st] = bits;
  if (bits > buzzA9SectorBest[sec]) buzzA9SectorBest[sec] = bits;
  if (bits > buzzA9StrideBest[arm]) buzzA9StrideBest[arm] = bits;

  bool randomControl = (buzzJob.minerLane == A9_LANE_RANDOM_BASELINE);
  if (randomControl) {
    buzzA9RandomHashes++;
    if (bits > buzzA9RandomBest) buzzA9RandomBest = bits;
  } else {
    buzzA9BroadHashes++;
    if (bits > buzzA9BroadBest) buzzA9BroadBest = bits;
  }

  if (shareFound) {
    buzzA9StrategyShares[st]++;
    buzzA9SectorShares[sec]++;
    buzzA9StrideShares[arm]++;
    if (randomControl) buzzA9RandomShares++;
    else buzzA9BroadShares++;
    buzzCorpusShares++;
    buzzCorpusDirty = true;
  }
}

void buzzConfigureMinerLane(BuzzRemoteJobState &j) {
  uint32_t seed = micros() ^ esp_random() ^ buzzAgentEntropySeed ^ j.startNonce ^ buzzWorkerId;
  for (uint8_t i = 0; i < 8; ++i) seed = buzzXorShift32(seed ^ j.job_id[i]);
  j.minerSeed = seed;
  j.minerStrideArm = buzzA9PickStrideArm(seed);
  j.minerStride = buzzStrideArmValue(j.minerStrideArm);
  j.minerStartOffset = buzzA9RangeMod(seed, j.rangeSize ? j.rangeSize : BUZZ_JOB_RANGE_DEFAULT);

  if (buzzLegacyA9Mode) {
    uint8_t phase = (uint8_t)((buzzJobsSeen + (seed & 3UL)) % 6UL);
    j.minerStrideArm = 0;
    j.minerStride = 1;
    switch (phase) {
      case 0:
        j.minerLane = A9_LANE_JANUS_DISPATCHER;
        j.minerStrategy = A9_ZIM_BANDIT;
        j.minerSector = 4;
        break;
      case 1:
        j.minerLane = A9_LANE_EXPLOIT_BEST;
        j.minerStrategy = A9_JANUS;
        j.minerSector = 4;
        break;
      case 2:
        j.minerLane = A9_LANE_ZIM_S6;
        j.minerStrategy = A9_ZIM_REVERSE;
        j.minerSector = 6;
        j.minerStrideArm = 5;
        j.minerStride = buzzStrideArmValue(j.minerStrideArm);
        break;
      case 3:
        j.minerLane = A9_LANE_DUAL_LOCK;
        j.minerStrategy = A9_LINEAR;
        j.minerSector = 6;
        break;
      case 4:
        j.minerLane = A9_LANE_JANUS_DISPATCHER;
        j.minerStrategy = A9_BITREV;
        j.minerSector = 8;
        break;
      default:
        j.minerLane = A9_LANE_RANDOM_BASELINE;
        j.minerStrategy = A9_RANDOM;
        j.minerSector = 4;
        break;
    }
    buzzLaneSwitches++;
    return;
  }

  uint8_t roll = (uint8_t)(seed % 100UL);
  bool stressed = (!buzzNightMode) || shieldPct < 45.0f || energyPct < 38.0f || laserHeat > 70.0f;
  if (stressed && roll < 55) j.minerLane = A9_LANE_SURVIVE_LINEAR;
  else if (roll < 8) j.minerLane = A9_LANE_RANDOM_BASELINE;
  else if (roll < 28) j.minerLane = A9_LANE_DUAL_LOCK;
  else if (roll < 45) j.minerLane = A9_LANE_ZIM_S6;
  else if (buzzAgentHint >= 3 && roll < 68) j.minerLane = A9_LANE_EXPLOIT_BEST;
  else j.minerLane = A9_LANE_JANUS_DISPATCHER;

  switch (j.minerLane) {
    case A9_LANE_RANDOM_BASELINE:
      j.minerStrategy = A9_RANDOM;
      j.minerSector = (uint8_t)((seed >> 8) % BUZZ_MINER_SECTORS);
      break;
    case A9_LANE_DUAL_LOCK:
      if ((seed & 0x03) == 0) {
        j.minerStrategy = A9_KNIGHT;
        j.minerSector = 11;
      } else if (seed & 0x04) {
        j.minerStrategy = A9_ZIM_REVERSE;
        j.minerSector = 6;
      } else {
        j.minerStrategy = A9_LINEAR;
        j.minerSector = 6;
      }
      break;
    case A9_LANE_ZIM_S6:
      j.minerStrategy = A9_ZIM_REVERSE;
      j.minerSector = 6;
      break;
    case A9_LANE_EXPLOIT_BEST:
      j.minerStrategy = buzzA9TopStrategy();
      j.minerSector = buzzA9TopSector();
      break;
    case A9_LANE_SURVIVE_LINEAR:
      j.minerStrategy = (seed & 1) ? A9_LINEAR : A9_BITREV;
      j.minerSector = (uint8_t)((seed >> 12) % BUZZ_MINER_SECTORS);
      break;
    default:
      j.minerStrategy = buzzA9BroadStrategy(seed);
      j.minerSector = (uint8_t)((seed >> 16) % BUZZ_MINER_SECTORS);
      break;
  }

  if (j.minerStrategy != A9_ZIM_REVERSE && j.minerStrategy != A9_ZIM_BANDIT) {
    j.minerStrideArm = 0;
    j.minerStride = 1;
  }
  buzzLaneSwitches++;
}

uint32_t buzzNextNonce(const BuzzRemoteJobState &j, uint32_t i) {
  uint32_t range = j.rangeSize ? j.rangeSize : BUZZ_JOB_RANGE_DEFAULT;
  if (!range) return j.startNonce + i;

  uint32_t sectorOff = buzzA9SectorOffset(range, j.minerSector);
  uint32_t origin = buzzA9RangeMod((uint64_t)j.minerStartOffset + sectorOff, range);
  uint32_t off = 0;

  switch (j.minerStrategy) {
    case A9_ZIM_REVERSE: {
      uint32_t walk = buzzA9RangeMod((uint64_t)i * (uint64_t)(j.minerStride | 1UL), range);
      off = (origin + range - walk) % range;
      break;
    }
    case A9_ZIM_BANDIT: {
      uint32_t wobble = buzzBitReverse32((j.minerSeed ^ (i * 0xA5A5A5A5UL)) & 0xFFFFFFFFUL) & 0xFFFFUL;
      uint32_t walk = buzzA9RangeMod((uint64_t)i * (uint64_t)(j.minerStride | 1UL), range);
      off = buzzA9RangeMod((uint64_t)origin + wobble + range - walk, range);
      break;
    }
    case A9_RANDOM: {
      uint64_t x = ((uint64_t)j.minerSeed << 32) ^ (uint64_t)(i + 1UL) ^ 0x9E3779B97F4A7C15ULL;
      off = buzzA9RangeMod((uint64_t)origin + buzzA9XorShift64Star(x), range);
      break;
    }
    case A9_JANUS: {
      uint32_t center = buzzA9RangeMod((uint64_t)origin + range / 2UL, range);
      uint32_t step = (i + 1UL) / 2UL;
      off = (i & 1UL) ? ((center + step) % range) : ((center + range - (step % range)) % range);
      break;
    }
    case A9_KNIGHT: {
      uint32_t stride2 = ((uint64_t)range * 61803ULL) / 100000ULL;
      stride2 |= 1UL;
      if (!stride2) stride2 = 1UL;
      off = buzzA9RangeMod((uint64_t)origin + (uint64_t)i * (uint64_t)stride2, range);
      break;
    }
    case A9_BITREV:
      off = buzzA9RangeMod((uint64_t)origin + buzzBitReverse32((j.minerSeed + i) & 0xFFFFFFFFUL), range);
      break;
    default:
      off = (origin + i) % range;
      break;
  }

  return j.startNonce + off;
}

uint16_t buzzActiveBatch() {
  return constrain((int)buzzMiningBatch, BUZZ_BATCH_MIN, BUZZ_BATCH_MAX);
}

bool buzzSendEspNow(const void *payload, size_t len) {
  if (!buzzEspNowReady || !payload || !len) return false;
  esp_err_t err = esp_now_send(BUZZ_BROADCAST_MAC, (const uint8_t*)payload, len);
  if (err == ESP_OK) {
    buzzTxOk++;
    buzzTxFailStreak = 0;
    return true;
  }
  buzzTxFail++;
  buzzTxFailStreak++;
  buzzLastTxErr = (int)err;
  return false;
}

void buzzSendShare(uint32_t nonce, uint16_t bits, const uint8_t shareHash[32]) {
  BuzzShareResponseV2 sr{};
  sr.magic[0] = 'S';
  sr.magic[1] = '2';
  memcpy(sr.job_id, buzzJob.job_id, 8);
  sr.nonce = nonce;
  sr.worker_id = buzzWorkerId;
  sr.bits = bits;
  sr.total_hashes_l32 = buzzTotalHashes;
  memcpy(sr.hash_tail, shareHash + 28, 4);
  if (buzzSendEspNow(&sr, sizeof(sr))) {
    buzzShares++;
    buzzLastShareNonce = nonce;
    buzzShareFlashUntilMs = millis() + BUZZ_SHARE_LED_FLASH_MS;
    statusLine = "A9 SHARE JACKPOT";
  }
}

bool buzzAgentTargetsThisNode(const BuzzAgentRewardPacket &ar) {
  if (ar.magic[0] != 'A' || ar.magic[1] != 'R') return false;
  if (ar.targetNode[0] == '\0') return true;
  if (!strcmp(ar.targetNode, "*")) return true;
  if (!strcasecmp(ar.targetNode, "all")) return true;
  if (!strcasecmp(ar.targetNode, BUZZ_NODE_ID)) return true;
  if (strstr(ar.targetNode, "Card") || strstr(ar.targetNode, "Elite")) return true;
  return false;
}

void buzzAcceptJobPacket(const BuzzJobPacket &jp) {
  buzzLastMasterMs = millis();
  if (jp.range_size == 0) return;

  if (buzzJob.active) {
    bool sameJob = memcmp(buzzJob.job_id, jp.job_id, 8) == 0;
    if (sameJob) {
      buzzJobsDeferred++;
      return;
    }
    buzzJobsReplaced++;
  }

  memcpy(buzzJob.job_id, jp.job_id, 8);
  memcpy(buzzJob.header, jp.header, 80);
  memcpy(buzzJob.target, jp.target, 32);
  buzzJob.startNonce = jp.start_nonce;
  buzzJob.rangeSize = jp.range_size ? jp.range_size : BUZZ_JOB_RANGE_DEFAULT;
  buzzJob.nonce = jp.start_nonce;
  buzzJob.hashesDone = 0;
  buzzJob.receivedAt = millis();
  buzzConfigureMinerLane(buzzJob);
  buzzJob.active = true;
  buzzTargetBits = buzzCountLeadingZeroBitsBE(buzzJob.target);
  buzzJobsSeen++;
  buzzRxJobs++;
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void buzzOnRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
#else
void buzzOnRecv(const uint8_t *mac, const uint8_t *data, int len)
#endif
{
  if (!data || len < 2) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (info && info->rx_ctrl) buzzLastRssi = info->rx_ctrl->rssi;
#endif
  buzzRxSeen++;

  if (len == sizeof(BuzzColonyPacket)) {
    BuzzColonyPacket pkt{};
    memcpy(&pkt, data, sizeof(pkt));
    if (memcmp(pkt.magic, "JANUS", 5) == 0 &&
        (strstr(pkt.nodeId, "Buzz") || strstr(pkt.role, "MASTER") || strstr(pkt.role, "Buzz"))) {
      buzzLastMasterMs = millis();
    }
    return;
  }

  if (len == sizeof(BuzzJobPacket) && data[0] == 'J' && data[1] == 'B') {
    BuzzJobPacket jp{};
    memcpy(&jp, data, sizeof(jp));
    buzzAcceptJobPacket(jp);
    return;
  }

  if (len == sizeof(BuzzAgentRewardPacket) && data[0] == 'A' && data[1] == 'R') {
    BuzzAgentRewardPacket ar{};
    memcpy(&ar, data, sizeof(ar));
    if (!buzzAgentTargetsThisNode(ar)) return;
    buzzRxRewards++;
    buzzAgentLevel = ar.rewardLevel;
    buzzAgentHint = ar.aiHint ? ar.aiHint : 1;
    buzzAgentBatch = ar.targetBatch ? ar.targetBatch : (buzzNightMode ? BUZZ_BATCH_NIGHT : BUZZ_BATCH_ACTIVE);
    buzzAgentBatch = constrain((int)buzzAgentBatch, BUZZ_BATCH_MIN, BUZZ_BATCH_MAX);
    buzzAgentEntropySeed ^= ar.entropySeed ^ micros() ^ ((uint32_t)ar.rewardLevel << 24);
    buzzAgentScore = ar.score;
    buzzAgentPredH = ar.predictedHashRate;
    buzzAgentErr = ar.predictionError;
  }
}

void buzzEnsureBroadcastPeer() {
  if (!buzzEspNowReady) return;
  if (esp_now_is_peer_exist(BUZZ_BROADCAST_MAC)) return;
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, BUZZ_BROADCAST_MAC, 6);
  peer.channel = BUZZ_ESPNOW_CHANNEL;
  peer.encrypt = false;
#if defined(WIFI_IF_STA)
  peer.ifidx = WIFI_IF_STA;
#endif
  esp_now_add_peer(&peer);
}

void buzzSetupEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_channel(BUZZ_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() != ESP_OK) {
    buzzEspNowReady = false;
    Serial.println("[CARD/BUZZ] ESP-NOW init failed");
    return;
  }
  buzzEspNowReady = true;
  esp_now_register_recv_cb(buzzOnRecv);
  buzzEnsureBroadcastPeer();
  buzzWorkerId = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
  buzzAgentEntropySeed ^= (uint32_t)ESP.getEfuseMac() ^ micros();
  Serial.printf("[CARD/BUZZ] ready id=%u ch=%u mac=%llX\n",
                (unsigned)buzzWorkerId, (unsigned)BUZZ_ESPNOW_CHANNEL,
                (unsigned long long)ESP.getEfuseMac());
}

void buzzRescueEspNow(const char *reason) {
  uint32_t now = millis();
  if (now - buzzLastEspNowRescueMs < BUZZ_ESPNOW_RESCUE_COOLDOWN_MS) return;
  buzzLastEspNowRescueMs = now;
  buzzEspNowRescues++;
  Serial.printf("[CARD/BUZZ/RESCUE] reason=%s n=%lu err=%d failStreak=%lu tx=%lu/%lu rx=%lu masterAge=%lums\n",
                reason ? reason : "unknown",
                (unsigned long)buzzEspNowRescues, buzzLastTxErr,
                (unsigned long)buzzTxFailStreak,
                (unsigned long)buzzTxOk, (unsigned long)buzzTxFail,
                (unsigned long)buzzRxSeen,
                (unsigned long)(buzzLastMasterMs ? now - buzzLastMasterMs : 999999UL));
  esp_now_deinit();
  buzzEspNowReady = false;
  buzzTxFailStreak = 0;
  delay(3);
  buzzSetupEspNow();
}

void buzzEspNowRescueTick(uint32_t now) {
  if (!buzzEspNowReady) return;
  if (buzzTxFailStreak >= BUZZ_ESPNOW_FAIL_STREAK_RESCUE) {
    buzzRescueEspNow("tx-fail-streak");
    return;
  }
  if (buzzLastMasterMs && buzzRxSeen > 0 && now - buzzLastMasterMs > BUZZ_ESPNOW_MASTER_STALE_RESCUE_MS &&
      now - buzzLastEspNowRescueMs > BUZZ_ESPNOW_RESCUE_COOLDOWN_MS) {
    buzzRescueEspNow("master-stale");
  }
}

void buzzSendHeartbeat() {
  if (!buzzEspNowReady) return;
  uint32_t now = millis();
  if (now - buzzLastHeartbeatMs < BUZZ_HEARTBEAT_MS) return;
  buzzLastHeartbeatMs = now;
  buzzEnsureBroadcastPeer();

  BuzzColonyPacket pkt{};
  memcpy(pkt.magic, "JANUS", 6);
  strlcpy(pkt.nodeId, BUZZ_NODE_ID, sizeof(pkt.nodeId));
  strlcpy(pkt.role, BUZZ_NODE_ROLE, sizeof(pkt.role));
  pkt.seq = ++buzzSeq;
  pkt.hashRate = buzzHashRate;
  pkt.shares = buzzShares;
  pkt.rejects = buzzRejects;
  pkt.bestBits = buzzBestBits;
  pkt.diff = 0.0f;
  pkt.targetBits = buzzTargetBits;
  pkt.aiBatch = buzzActiveBatch();
  pkt.aiHint = buzzAgentHint;
  pkt.jobAgeMs = buzzJob.active ? now - buzzJob.receivedAt : 0;
  pkt.rssi = buzzLastRssi;
  pkt.uptime = now / 1000UL;
  buzzSendEspNow(&pkt, sizeof(pkt));
}

bool envShtFreshNow(uint32_t now) {
  return envShtLastOkMs && (now - envShtLastOkMs <= ENV_REAL_TTL_MS) &&
         isfinite(envTempC) && envTempC > -40.0f && envTempC < 90.0f &&
         isfinite(envHumidity) && envHumidity >= 0.0f && envHumidity <= 100.0f;
}

bool envQmpFreshNow(uint32_t now) {
  return envQmpLastOkMs && (now - envQmpLastOkMs <= ENV_QMP_REAL_TTL_MS) &&
         isfinite(envPressureHpa) && envPressureHpa > 300.0f && envPressureHpa < 1200.0f;
}

void buzzSendSwarmSense() {
  if (!buzzEspNowReady) return;
  uint32_t now = millis();
  if (now - swarmSenseLastTxMs < SWARMSENSE_TX_MS) return;
  swarmSenseLastTxMs = now;
  buzzEnsureBroadcastPeer();
  bool shtFresh = envShtFreshNow(now);
  bool qmpFresh = envQmpFreshNow(now);

  SwarmSensePacket ss{};
  ss.magic[0] = 'S';
  ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = buzzWorkerId;
  // Keep S/S out of Core2's fixed Beacon slot. It carries game/miner load, not ENV.
  strlcpy(ss.nodeId, "A9FieldSense", sizeof(ss.nodeId));
  strlcpy(ss.kind, "A9FieldSense", sizeof(ss.kind));
  ss.seq = ++swarmSenseSeq;
  ss.uptime_ms = now;
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.rssi = buzzLastRssi;
  ss.radio_mode = buzzEspNowReady ? 1 : 0;
  ss.bt_flags = (uint8_t)((docked ? 0x01 : 0x00) | (buzzJob.active ? 0x02 : 0x00) |
                          (shtFresh ? 0x04 : 0x00) | (qmpFresh ? 0x08 : 0x00) |
                          (imuReady ? 0x10 : 0x00) | (buzzMinerEnabled ? 0x20 : 0x00));
  ss.palette = (uint8_t)viewMode;
  ss.knn_label = docked ? 1 : (buzzJob.active ? 3 : 2);
  uint32_t bestForConfidence = buzzBestBits > 30UL ? 30UL : buzzBestBits;
  ss.knn_confidence = (uint8_t)constrain((int)(62 + bestForConfidence + (shtFresh ? 4 : 0) + (qmpFresh ? 4 : 0)), 0, 100);
  ss.ai_hint = buzzAgentHint ? buzzAgentHint : 1;
  ss.thermal_load = (uint8_t)constrain((int)(reactorLoad * 54.0f + laserHeat * 0.36f + (100.0f - energyPct) * 0.12f), 0, 100);
  ss.effective_batch = buzzActiveBatch();
  ss.dynamic_batch = buzzMiningBatch;
  ss.hash_rate = buzzHashRate;
  ss.total_hashes = buzzTotalHashes;
  ss.best_bits = (uint16_t)(buzzBestBits > 65535UL ? 65535UL : buzzBestBits);
  float activeBatch = (float)buzzActiveBatch();
  if (activeBatch < 1.0f) activeBatch = 1.0f;
  ss.hash_eff_x1000 = (uint16_t)constrain((int)((float)buzzHashRate / activeBatch * 1000.0f), 0, 65535);
  ss.prediction_error_x1000 = (int16_t)constrain((int)(buzzAgentErr * 1000.0f), -32768, 32767);
  ss.entropy_x1000 = (uint16_t)constrain((int)(beaconLegacyEntropy() * 1000.0f), 0, 65535);
  ss.touch_delta = (uint16_t)constrain((int)(fabsf(shipYawRate) * 80.0f + fabsf(shipPitchRate) * 80.0f + fabsf(shipRollRate) * 60.0f), 0, 65535);
  ss.job_age_s = (uint16_t)constrain((int)(buzzJob.active ? (now - buzzJob.receivedAt) / 1000UL : 0), 0, 65535);
  ss.nonce_remaining_l16 = (uint16_t)((buzzJob.active && buzzJob.rangeSize > buzzJob.hashesDone) ? (buzzJob.rangeSize - buzzJob.hashesDone) : 0);
  ss.flags = 0;
  if (buzzEspNowReady) ss.flags |= 0x0001;
  if (qmpFresh || shtFresh) ss.flags |= 0x0008;
  if (buzzMinerEnabled) ss.flags |= 0x0020;
  if (shipLedEnabled) ss.flags |= 0x0040;
  if (M5.Power.getBatteryLevel() >= 0) ss.flags |= 0x0080;
  if (docked) ss.flags |= 0x0100;
  if (buzzLegacyA9Mode) ss.flags |= 0x0200;

  if (buzzSendEspNow(&ss, sizeof(ss))) {
    swarmSenseTxOk++;
    if ((swarmSenseTxOk % 10UL) == 0) {
      char pText[12];
      if (qmpFresh) snprintf(pText, sizeof(pText), "%.1f", envPressureHpa);
      else strlcpy(pText, "--", sizeof(pText));
      Serial.printf("[CARD/SWARM] SS tx=%lu node=%s kind=%s H=%lu best=%lu T=%.1f P=%s env=%u/%u\n",
                    (unsigned long)swarmSenseTxOk, ss.nodeId, ss.kind,
                    (unsigned long)buzzHashRate, (unsigned long)buzzBestBits,
                    envTempC, pText, shtFresh ? 1 : 0, qmpFresh ? 1 : 0);
    }
  } else {
    swarmSenseTxFail++;
    Serial.printf("[CARD/SWARM] SS fail=%lu err=%d\n", (unsigned long)swarmSenseTxFail, buzzLastTxErr);
  }
}

void buzzSendBeaconEnvTelemetry() {
  if (!buzzEspNowReady) return;
  if (!envSensorsEnabled) return;
  uint32_t now = millis();
  if (now - beaconEnvLastTxMs < BEACON_ENV_TX_MS) return;
  beaconEnvLastTxMs = now;
  buzzEnsureBroadcastPeer();
  readEnvSensors();
  now = millis();

  bool shtFresh = envShtFreshNow(now);
  bool qmpFresh = envQmpFreshNow(now);
  if (!shtFresh) {
    beaconEnvTxFail++;
    if ((beaconEnvTxFail % 5UL) == 1) {
      Serial.printf("[CARD/ENV] hold real-only sht=%u age=%lums qmp=%u age=%lums\n",
                    envShtReady ? 1 : 0,
                    envShtLastOkMs ? (unsigned long)(now - envShtLastOkMs) : 999999UL,
                    envQmpReady ? 1 : 0,
                    envQmpLastOkMs ? (unsigned long)(now - envQmpLastOkMs) : 999999UL);
    }
    return;
  }

  const float entropy = beaconLegacyEntropy();
  const float loss = beaconLegacyLoss();
  const float fit = beaconLegacyFit();
  const float m2r = beaconLegacyM2R();
  const float motion = constrain(fabsf(shipYawRate) + fabsf(shipPitchRate) + fabsf(shipRollRate), 0.0f, 9.99f);
  float sync = 0.18f;
  if (shtFresh && qmpFresh) sync = 0.98f;
  else if (shtFresh) sync = 0.82f;

  uint8_t sensorFlags = 0;
  if (shtFresh) sensorFlags |= 0x01;
  if (qmpFresh) sensorFlags |= 0x02;
  if (imuReady) sensorFlags |= 0x10;
  if (buzzMinerEnabled) sensorFlags |= 0x20;
  if (buzzCorpusSdReady) sensorFlags |= 0x40;

  EntropyReportV2 er{};
  er.magic[0] = 'E';
  er.magic[1] = '2';
  er.worker_id = buzzWorkerId;
  strlcpy(er.nodeId, BUZZ_NODE_ID, sizeof(er.nodeId));
  er.local_entropy = entropy;
  er.prediction_error = loss;
  er.sync_hint = sync;
  er.fit = fit;
  er.sensor_flags = sensorFlags;
  er.values[0] = envTempC;
  er.values[1] = envHumidity;
  er.values[2] = entropy;
  er.values[3] = m2r;
  er.values[4] = motion;
  er.values[5] = loss;
  er.values[6] = (float)buzzBestBits;
  er.values[7] = (float)buzzLastRssi;
  er.uptime_ms = now;

  JanusAiNodePacket ai{};
  ai.magic[0] = 'A';
  ai.magic[1] = 'I';
  ai.version = 1;
  ai.flags = (uint8_t)((buzzCorpusSdReady ? 0x01 : 0x00) |
                       ((buzzCorpusSdReady || buzzCorpusNvsFallback) ? 0x02 : 0x00) |
                       (buzzEspNowReady ? 0x04 : 0x00));
  strlcpy(ai.nodeId, BUZZ_NODE_ID, sizeof(ai.nodeId));
  strlcpy(ai.role, "BeaconADV", sizeof(ai.role));
  ai.seq = ++beaconEnvSeq;
  ai.uptime_ms = now;
  ai.entropy = entropy;
  ai.prediction_error = loss;
  ai.sync = sync;
  ai.fit = fit;
  ai.attention = constrain(fabsf(envTempC - envPredTempC) * 0.16f +
                           fabsf(envHumidity - envPredHumidity) * 0.012f +
                           envPressureLoss * 0.035f + motion * 0.08f,
                           0.0f, 1.0f);
  ai.values[0] = envTempC;
  ai.values[1] = envHumidity;
  ai.values[2] = qmpFresh ? envPressureHpa : 0.0f;
  ai.values[3] = entropy;
  ai.values[4] = (float)beaconLegacyAiNodes();
  ai.values[5] = (float)buzzLastRssi;

  bool okE2 = buzzSendEspNow(&er, sizeof(er));
  bool okAi = qmpFresh ? buzzSendEspNow(&ai, sizeof(ai)) : true;
  if (okE2 && okAi) {
    beaconEnvTxOk++;
    if ((beaconEnvTxOk % 5UL) == 0) {
      char pText[12];
      if (qmpFresh) snprintf(pText, sizeof(pText), "%.1f", envPressureHpa);
      else strlcpy(pText, "--", sizeof(pText));
      Serial.printf("[CARD/ENV] tx=%lu T=%.1f H=%.1f P=%s E=%.2f M2R=%.2f sync=%.2f flags=0x%02X\n",
                    (unsigned long)beaconEnvTxOk,
                    er.values[0], er.values[1], pText, entropy, m2r, sync, sensorFlags);
    }
  } else {
    beaconEnvTxFail++;
    Serial.printf("[CARD/ENV] fail=%lu e2=%u ai=%u err=%d\n",
                  (unsigned long)beaconEnvTxFail, okE2 ? 1 : 0, okAi ? 1 : 0, buzzLastTxErr);
  }
}

uint8_t elitePilotSector() {
  if (zone == ZONE_PIRATE_DEN) return 4;
  if (zone == ZONE_BELT) return 2;
  if (stationDistance < 1300.0f) return 0;

  float radial = dist3(shipX, shipY, shipZ, stationWorld.x, stationWorld.y, stationWorld.z);
  uint32_t ring = (uint32_t)(radial / 850.0f);
  uint32_t xBand = (uint32_t)(fabsf(shipX) / 900.0f);
  uint32_t yBand = (shipY > 0.0f) ? 1UL : 0UL;
  uint32_t zBand = (shipZ > 0.0f) ? 3UL : 9UL;
  return (uint8_t)((ring + xBand + yBand + zBand) & 0x0F);
}

uint8_t elitePilotObjective() {
  if (navTarget == NAV_STATION) return 1;
  if (navTarget == NAV_BELT) return 2;
  if (navTarget == NAV_PIRATE_DEN) return 3;
  if (docked) return 4;
  if (zone == ZONE_BELT) return 5;
  if (zone == ZONE_PIRATE_DEN) return 6;
  return 0;
}

uint8_t elitePilotThreat() {
  uint8_t alive = 0;
  for (uint8_t i = 0; i < ENEMY_COUNT; ++i) {
    if (enemies[i].alive) alive++;
  }
  int threat = alive * 18 + (zone == ZONE_PIRATE_DEN ? 42 : 0) + (hullPct < 45.0f ? 30 : 0) + (shieldPct < 25.0f ? 20 : 0);
  return (uint8_t)constrain(threat, 0, 255);
}

uint16_t elitePilotDistance() {
  float d = stationDistance;
  float origin = dist3(shipX, shipY, shipZ, stationWorld.x, stationWorld.y, stationWorld.z);
  if (origin > d) d = origin;
  if (d < 0.0f) d = 0.0f;
  if (d > 65535.0f) d = 65535.0f;
  return (uint16_t)d;
}

void buzzSendPilotLink() {
  if (!buzzEspNowReady) return;
  uint32_t now = millis();
  if (now - pilotLinkLastTxMs < PILOTLINK_TX_MS) return;
  pilotLinkLastTxMs = now;
  buzzEnsureBroadcastPeer();

  JanusPilotLinkPacket pl{};
  pl.magic[0] = 'P';
  pl.magic[1] = 'L';
  pl.version = 1;
  strlcpy(pl.nodeId, BUZZ_NODE_ID, sizeof(pl.nodeId));
  pl.seq = ++pilotLinkSeq;
  pl.galaxy = currentGalaxy;
  pl.system = currentSystem;
  pl.sector = elitePilotSector();
  pl.mode = docked ? 1 : 0;
  pl.distance = elitePilotDistance();
  pl.credits_l16 = (uint16_t)(credits & 0xFFFFUL);
  pl.kills = kills;
  pl.shield_x10 = (uint16_t)constrain((int)(shieldPct * 10.0f), 0, 1000);
  pl.energy_x10 = (uint16_t)constrain((int)(energyPct * 10.0f), 0, 1000);
  uint16_t level = 1 + (uint16_t)(kills / 5U) + (uint16_t)(cargoMax / 8U);
  if (level > 15U) level = 15U;
  pl.mech_level = (uint8_t)level;
  pl.mech_heat = (uint8_t)constrain((int)laserHeat, 0, 100);
  pl.mech_armor = (uint16_t)constrain((int)(hullPct * 10.0f), 0, 1000);
  pl.surface_active = (zone == ZONE_BELT || zone == ZONE_PIRATE_DEN) ? 1 : 0;
  pl.objective = elitePilotObjective();
  pl.threat = elitePilotThreat();
  pl.rssi = buzzLastRssi;
  pl.uptime_ms = now;

  if (buzzSendEspNow(&pl, sizeof(pl))) {
    pilotLinkTxOk++;
    if ((pilotLinkTxOk <= 3UL) || ((pilotLinkTxOk % 20UL) == 0)) {
      Serial.printf("[CARD/PILOT] PL tx=%lu g=%u sys=%u sector=%u mode=%u obj=%u threat=%u dist=%u shield=%u energy=%u\n",
                    (unsigned long)pilotLinkTxOk, (unsigned)pl.galaxy + 1U, (unsigned)pl.system,
                    (unsigned)pl.sector, (unsigned)pl.mode,
                    (unsigned)pl.objective, (unsigned)pl.threat, (unsigned)pl.distance,
                    (unsigned)pl.shield_x10, (unsigned)pl.energy_x10);
    }
  } else {
    pilotLinkTxFail++;
    Serial.printf("[CARD/PILOT] PL fail=%lu err=%d\n", (unsigned long)pilotLinkTxFail, buzzLastTxErr);
  }
}

#if ADV_CAP_LORA_ENABLE
void IRAM_ATTR advCapLoRaSetFlag() {
  advCapLoRaDone = true;
}
#endif

bool advCapGnssFreshNow(uint32_t now) {
  return advCapGnssFix && advCapLastFixMs && (now - advCapLastFixMs < 7000UL);
}

uint32_t advSkyJobSig() {
  uint32_t h = 0x51A9C0DEUL;
  if (buzzJob.active) {
    for (uint8_t i = 0; i < 8; ++i) h = janusMix32(h, buzzJob.job_id[i]);
    h = janusMix32(h, buzzJob.startNonce);
    h = janusMix32(h, buzzJob.rangeSize);
    h = janusMix32(h, buzzJob.hashesDone);
  } else {
    h = janusMix32(h, buzzTotalHashes);
    h = janusMix32(h, millis() / 5000UL);
  }
  h = janusMix32(h, advCapGeoSig);
  h = janusMix32(h, ((uint32_t)currentGalaxy << 24) | ((uint32_t)currentSystem << 8) | targetSystem);
  h = janusMix32(h, galaxySystems[currentSystem].signature);
  h = janusMix32(h, (uint32_t)knownCosmosCount);
  h = janusMix32(h, ((uint32_t)buzzBestBits << 16) | buzzTargetBits);
  h = janusMix32(h, buzzWorkerId);
  return h;
}

uint8_t advSkySector() {
  uint32_t h = 0x534B5941UL;
  uint32_t now = millis();
  if (advCapGnssFreshNow(now)) {
    int32_t latKey = (int32_t)lroundf((advCapLat + 90.0f) * 1000.0f);
    int32_t lngKey = (int32_t)lroundf((advCapLng + 180.0f) * 1000.0f);
    h = janusMix32(h, (uint32_t)latKey);
    h = janusMix32(h, (uint32_t)lngKey);
    h = janusMix32(h, (uint32_t)advCapSatellites);
    h = janusMix32(h, (uint32_t)(advCapCourseDeg / 22.5f));
  } else {
    h = janusMix32(h, ((uint32_t)currentGalaxy << 16) | ((uint32_t)currentSystem << 8) | targetSystem);
    h = janusMix32(h, galaxySystems[currentSystem].signature);
    h = janusMix32(h, (uint32_t)elitePilotSector());
    h = janusMix32(h, (uint32_t)elitePilotDistance());
    h = janusMix32(h, (uint32_t)(shipYaw * 1000.0f));
  }
  h = janusMix32(h, buzzJob.active ? buzzJob.startNonce : buzzTotalHashes);
  return (uint8_t)(h & 0x0F);
}

uint8_t advSkyLane() {
  bool fix = advCapGnssFreshNow(millis());
  if (fix && advCapLoRaReady) return 3;  // SKY_LOCK
  if (fix) return 2;                     // GNSS_ORBIT
  if (advCapLoRaReady) return 1;         // LORA_BEACON
  return 0;                              // LOCAL_ONLY
}

const char* advSkyLaneName(uint8_t lane) {
  switch (lane) {
    case 3: return "SKY_LOCK";
    case 2: return "GNSS";
    case 1: return "LORA";
    default: return "LOCAL";
  }
}

uint8_t advSkyFlags(uint32_t now) {
  uint8_t flags = 0;
  if (buzzJob.active) flags |= 0x01;
  if (advCapLoRaReady) flags |= 0x02;
  if (advCapGnssFreshNow(now)) flags |= 0x04;
  if (advCapSkyLock > 0.66f) flags |= 0x08;
  if (envShtFreshNow(now)) flags |= 0x10;
  if (envQmpFreshNow(now)) flags |= 0x20;
  if (eliteTargetReachable(targetSystem)) flags |= 0x40;
  if (buzzCorpusSdReady) flags |= 0x80;
  return flags;
}

void advCapReadGnss() {
  if (!advCapGnssStarted) return;
  uint16_t budget = 0;
  while (Serial1.available() && budget < 220) {
    advCapGps.encode((char)Serial1.read());
    budget++;
  }
  if (budget) advCapLastGnssReadMs = millis();

  if (advCapGps.satellites.isValid()) {
    advCapSatellites = (uint16_t)constrain((int)advCapGps.satellites.value(), 0, 99);
  }
  if (advCapGps.hdop.isValid()) {
    advCapHdop = (float)advCapGps.hdop.hdop();
  }
  if (advCapGps.location.isValid() && advCapGps.location.age() < 5000UL) {
    advCapLat = (float)advCapGps.location.lat();
    advCapLng = (float)advCapGps.location.lng();
    advCapCourseDeg = advCapGps.course.isValid() ? (float)advCapGps.course.deg() : advCapCourseDeg;
    advCapSpeedKmph = advCapGps.speed.isValid() ? (float)advCapGps.speed.kmph() : advCapSpeedKmph;
    advCapLastFixMs = millis();
    advCapGnssFix = true;

    uint32_t h = 0x474E5353UL;
    h = janusMix32(h, (uint32_t)lroundf((advCapLat + 90.0f) * 100000.0f));
    h = janusMix32(h, (uint32_t)lroundf((advCapLng + 180.0f) * 100000.0f));
    h = janusMix32(h, (uint32_t)(advCapHdop * 100.0f));
    h = janusMix32(h, (uint32_t)advCapSatellites);
    h = janusMix32(h, advCapGps.time.isValid() ? advCapGps.time.value() : millis());
    advCapGeoSig = h;
  } else if (advCapLastFixMs && millis() - advCapLastFixMs > 12000UL) {
    advCapGnssFix = false;
  }
}

void advCapUpdateSkyLock(uint32_t now) {
  float target = advSkyAnchorEnabled ? 0.10f : 0.0f;
  bool fix = advCapGnssFreshNow(now);
  if (fix) {
    target += 0.38f;
    target += min((float)advCapSatellites, 12.0f) * 0.025f;
    target += clampf((6.0f - min(advCapHdop, 6.0f)) * 0.035f, 0.0f, 0.20f);
  }
  if (advCapLoRaReady) target += 0.16f;
  if (buzzJob.active) target += 0.06f;
  if (envShtFreshNow(now)) target += 0.04f;
  if (envQmpFreshNow(now)) target += 0.03f;
  if (buzzHashRate > 2500UL) target += 0.04f;
  advCapSkyLock = advCapSkyLock * 0.86f + clampf(target, 0.0f, 1.0f) * 0.14f;
}

uint32_t advPacketHash32(JanusPnCortexPacket &pn) {
  pn.packet_hash = 0;
  uint8_t h[32];
  buzzDoubleSha256((const uint8_t*)&pn, sizeof(pn), h);
  return ((uint32_t)h[0]) | ((uint32_t)h[1] << 8) | ((uint32_t)h[2] << 16) | ((uint32_t)h[3] << 24);
}

void advSendPnCortex() {
  if (!buzzEspNowReady || !advSkyAnchorEnabled) return;
  uint32_t now = millis();
  if (now - advSkyLastTxMs < ADV_SKYANCHOR_TX_MS) return;
  advSkyLastTxMs = now;
  buzzEnsureBroadcastPeer();

  JanusPnCortexPacket pn{};
  pn.magic[0] = 'P';
  pn.magic[1] = 'N';
  pn.version = 1;
  pn.role = 9; // ADV sky anchor / pilot node.
  pn.worker_id = buzzWorkerId;
  strlcpy(pn.nodeId, BUZZ_NODE_ID, sizeof(pn.nodeId));
  strlcpy(pn.kind, "adv_sky_anchor", sizeof(pn.kind));
  pn.seq = ++advSkySeq;
  pn.uptime_ms = now;
  pn.job_sig = advSkyJobSig();
  pn.prev_hash = advSkyPrevHash;
  pn.hash_rate = buzzHashRate;
  pn.total_hashes = buzzTotalHashes;
  pn.target_bits = buzzTargetBits;
  pn.best_bits = (uint16_t)min(buzzBestBits, 65535UL);
  pn.lane = advSkyLane();
  pn.sector = advSkySector();
  pn.flags = advSkyFlags(now);
  pn.rssi = buzzLastRssi;

  float thermal = reactorLoad * 0.70f + (laserHeat / 100.0f) * 0.30f;
  if (envShtFreshNow(now)) thermal += clampf((envTempC - 20.0f) / 80.0f, 0.0f, 0.35f);
  float load = clampf((float)buzzHashRate / 9000.0f + reactorLoad * 0.34f + (advCapLoRaTxActive ? 0.08f : 0.0f), 0.0f, 3.0f);
  float jitter = fabsf(shipYawRate) * 210.0f + fabsf(shipPitchRate) * 210.0f + fabsf(shipRollRate) * 160.0f;
  int16_t mv = M5.Power.getBatteryVoltage();
  pn.thermal_x1000 = (uint16_t)constrain((int)(clampf(thermal, 0.0f, 3.0f) * 1000.0f), 0, 65535);
  pn.load_x1000 = (uint16_t)constrain((int)(load * 1000.0f), 0, 65535);
  pn.jitter_us = (uint16_t)constrain((int)jitter, 0, 65535);
  pn.entropy_x1000 = (uint16_t)constrain((int)(beaconLegacyEntropy() * 1000.0f), 0, 65535);
  pn.tail_x1000 = (uint16_t)constrain((int)(advCapSkyLock * 1000.0f), 0, 1000);
  pn.voltage_mv = mv > 0 ? (uint16_t)mv : 0;
  pn.ir_phase = (uint16_t)(((advCapSatellites & 0x3F) << 8) | ((uint8_t)constrain((int)(advCapSkyLock * 255.0f), 0, 255)));
  pn.reserved = (uint16_t)(((uint16_t)(currentGalaxy & 0x07) << 13) |
                           ((uint16_t)currentSystem << 5) |
                           (uint16_t)(targetSystem & 0x1F));
  pn.packet_hash = advPacketHash32(pn);
  advSkyPrevHash = pn.packet_hash;

  if (buzzSendEspNow(&pn, sizeof(pn))) {
    advSkyTxOk++;
    if ((advSkyTxOk <= 3UL) || ((advSkyTxOk % 8UL) == 0)) {
      Serial.printf("[CARD/SKY] PN tx=%lu lane=%s lock=%u fix=%u sat=%u lora=%u sec=%u flags=0x%02X H=%lu best=%lu/%u sig=%08lX\n",
                    (unsigned long)advSkyTxOk, advSkyLaneName(pn.lane),
                    (unsigned)constrain((int)(advCapSkyLock * 100.0f), 0, 100),
                    advCapGnssFreshNow(now) ? 1 : 0, (unsigned)advCapSatellites,
                    advCapLoRaReady ? 1 : 0, (unsigned)pn.sector,
                    (unsigned)pn.flags, (unsigned long)buzzHashRate, (unsigned long)buzzBestBits,
                    (unsigned)buzzTargetBits, (unsigned long)pn.job_sig);
    }
  } else {
    advSkyTxFail++;
    Serial.printf("[CARD/SKY] PN fail=%lu err=%d\n", (unsigned long)advSkyTxFail, buzzLastTxErr);
  }
}

size_t advBuildSkySeal(char *out, size_t outLen) {
  uint32_t now = millis();
  uint32_t sig = advSkyJobSig();
  uint8_t sector = advSkySector();
  int lockPct = constrain((int)(advCapSkyLock * 100.0f), 0, 100);
  bool fixFresh = advCapGnssFreshNow(now);
  uint8_t flags = advSkyFlags(now);
  uint32_t fixAge = advCapLastFixMs ? (now - advCapLastFixMs) : 999999UL;
  uint8_t courseBucket = fixFresh ? (uint8_t)constrain((int)(advCapCourseDeg / 22.5f), 0, 15) : 0;
  char hdopText[8];
  if (fixFresh && isfinite(advCapHdop)) snprintf(hdopText, sizeof(hdopText), "%.1f", advCapHdop);
  else strlcpy(hdopText, "--", sizeof(hdopText));
  const char *fix = fixFresh ? "G" : "-";
  const char *job = buzzJob.active ? "J" : "-";
  const char *env = envShtFreshNow(now) ? (envQmpFreshNow(now) ? "SQ" : "S-") : "--";
  const char *route = eliteTargetReachable(targetSystem) ? "R" : "-";
  return (size_t)snprintf(out, outLen,
                          "JSA3|n=%s|k=adv_sky_anchor|q=%lu|g=%u|s=%u|t=%u|sec=%u|lane=%s|f=%02X|%s%s%s|env=%s|sat=%u|hdop=%s|age=%lu|c=%u|lock=%d|best=%lu/%u|sig=%08lX",
                          BUZZ_NODE_ID, (unsigned long)advSkySeq,
                          (unsigned)currentGalaxy + 1U, (unsigned)currentSystem, (unsigned)targetSystem,
                          (unsigned)sector, advSkyLaneName(advSkyLane()), (unsigned)flags,
                          fix, job, route, env, (unsigned)advCapSatellites, hdopText,
                          (unsigned long)fixAge, (unsigned)courseBucket, lockPct,
                          (unsigned long)buzzBestBits, (unsigned)buzzTargetBits,
                          (unsigned long)sig);
}

void advCapMaybeFinishLoRa(uint32_t now) {
#if ADV_CAP_LORA_ENABLE
  if (!advCapLoRaTxActive) return;
  if (!advCapLoRaDone && now - advCapLoRaTxStartMs < ADV_CAP_LORA_TX_TIMEOUT_MS) return;
  int16_t state = advCapRadio.finishTransmit();
  advCapLoRaLastState = state;
  advCapLoRaTxActive = false;
  advCapLoRaDone = false;
  advCapRadio.startReceive();
  if (state == RADIOLIB_ERR_NONE) {
    advCapLoRaTxOk++;
  } else {
    advCapLoRaTxFail++;
    Serial.printf("[CARD/SKY/LORA] finish fail=%d ok=%lu fail=%lu\n",
                  (int)state, (unsigned long)advCapLoRaTxOk, (unsigned long)advCapLoRaTxFail);
  }
#else
  (void)now;
#endif
}

void advCapMaybeSendLoRa(uint32_t now) {
#if ADV_CAP_LORA_ENABLE
  if (!advSkyAnchorEnabled || !advCapLoRaEnabled || !advCapLoRaReady || advCapLoRaTxActive) return;
  if (now - advCapLastLoRaTxMs < ADV_CAP_LORA_TX_MS) return;

  char msg[196];
  size_t len = advBuildSkySeal(msg, sizeof(msg));
  if (len >= sizeof(msg)) len = sizeof(msg) - 1;
  advCapLoRaDone = false;
  int16_t state = advCapRadio.startTransmit((const uint8_t*)msg, len);
  advCapLoRaLastState = state;
  if (state == RADIOLIB_ERR_NONE) {
    advCapLastLoRaTxMs = now;
    advCapLoRaTxStartMs = now;
    advCapLoRaTxActive = true;
    Serial.printf("[CARD/SKY/LORA] sky-seal tx len=%u lock=%u sat=%u sig=%08lX\n",
                  (unsigned)len, (unsigned)constrain((int)(advCapSkyLock * 100.0f), 0, 100),
                  (unsigned)advCapSatellites, (unsigned long)advSkyJobSig());
  } else {
    advCapLoRaTxFail++;
    advCapLastLoRaTxMs = now;
    Serial.printf("[CARD/SKY/LORA] start fail=%d ok=%lu fail=%lu\n",
                  (int)state, (unsigned long)advCapLoRaTxOk, (unsigned long)advCapLoRaTxFail);
  }
#else
  (void)now;
#endif
}

void advCapBegin() {
  Serial1.begin(ADV_CAP_GNSS_BAUD, SERIAL_8N1, ADV_CAP_GNSS_RX_PIN, ADV_CAP_GNSS_TX_PIN);
  advCapGnssStarted = true;
  Serial.printf("[CARD/CAP] GNSS UART rx=G%u tx=G%u baud=%lu\n",
                (unsigned)ADV_CAP_GNSS_RX_PIN, (unsigned)ADV_CAP_GNSS_TX_PIN,
                (unsigned long)ADV_CAP_GNSS_BAUD);

#if ADV_CAP_LORA_ENABLE
  SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, ADV_CAP_LORA_NSS_PIN);
  int16_t state = advCapRadio.begin(ADV_CAP_LORA_FREQ_MHZ, ADV_CAP_LORA_BW_KHZ,
                                    ADV_CAP_LORA_SF, ADV_CAP_LORA_CR,
                                    ADV_CAP_LORA_SYNC_WORD, ADV_CAP_LORA_TX_POWER_DBM,
                                    ADV_CAP_LORA_PREAMBLE_LEN, ADV_CAP_LORA_TCXO_VOLT, false);
  advCapLoRaLastState = state;
  if (state == RADIOLIB_ERR_NONE) {
    advCapLoRaReady = true;
    advCapRadio.setPacketSentAction(advCapLoRaSetFlag);
    advCapRadio.startReceive();
    Serial.printf("[CARD/CAP] LoRa SX1262 ready freq=%.1fMHz sf=%u cr=%u pwr=%ddBm txEvery=%lus pins nss=%u irq=%u rst=%u busy=%u\n",
                  ADV_CAP_LORA_FREQ_MHZ, (unsigned)ADV_CAP_LORA_SF, (unsigned)ADV_CAP_LORA_CR,
                  (int)ADV_CAP_LORA_TX_POWER_DBM, (unsigned long)(ADV_CAP_LORA_TX_MS / 1000UL),
                  (unsigned)ADV_CAP_LORA_NSS_PIN, (unsigned)ADV_CAP_LORA_IRQ_PIN,
                  (unsigned)ADV_CAP_LORA_RST_PIN, (unsigned)ADV_CAP_LORA_BUSY_PIN);
  } else {
    advCapLoRaReady = false;
    Serial.printf("[CARD/CAP] LoRa SX1262 not ready state=%d, sky anchor continues on ESP-NOW/GNSS\n", (int)state);
  }
#else
  Serial.println("[CARD/CAP] LoRa compile flag OFF, sky anchor uses ESP-NOW/GNSS only");
#endif
}

void advSkyHealthTick(uint32_t now) {
  if (now - advSkyLastHealthMs < ADV_SKY_HEALTH_MS) return;
  advSkyLastHealthMs = now;

  bool fixFresh = advCapGnssFreshNow(now);
  char ageText[16];
  if (advCapLastFixMs) snprintf(ageText, sizeof(ageText), "%lu", (unsigned long)(now - advCapLastFixMs));
  else strlcpy(ageText, "--", sizeof(ageText));

  char hdopText[8];
  if (fixFresh && isfinite(advCapHdop)) snprintf(hdopText, sizeof(hdopText), "%.1f", advCapHdop);
  else strlcpy(hdopText, "--", sizeof(hdopText));

  char courseText[8];
  if (fixFresh && isfinite(advCapCourseDeg)) snprintf(courseText, sizeof(courseText), "%.0f", advCapCourseDeg);
  else strlcpy(courseText, "--", sizeof(courseText));

  bool shtFresh = envShtFreshNow(now);
  bool qmpFresh = envQmpFreshNow(now);
  uint8_t flags = advSkyFlags(now);
  const char* curName = galaxySystems[currentSystem].name;
  const char* tgtName = galaxySystems[targetSystem].name;

  Serial.printf("[CARD/SKY/ANCHOR] v=0.20 kind=adv_sky_anchor lane=%s flags=0x%02X lock=%u fix=%u age=%s sat=%u hdop=%s course=%s lora=%u st=%d ok=%lu fail=%lu pn=%lu/%lu route=%u g=%u sys=%u/%s target=%u/%s H=%lu best=%lu/%u fs=%u resc=%lu env=%c%c sd=%u\n",
                advSkyLaneName(advSkyLane()), (unsigned)flags,
                (unsigned)constrain((int)(advCapSkyLock * 100.0f), 0, 100),
                fixFresh ? 1 : 0, ageText, (unsigned)advCapSatellites, hdopText, courseText,
                advCapLoRaReady ? 1 : 0, (int)advCapLoRaLastState,
                (unsigned long)advCapLoRaTxOk, (unsigned long)advCapLoRaTxFail,
                (unsigned long)advSkyTxOk, (unsigned long)advSkyTxFail,
                eliteTargetReachable(targetSystem) ? 1 : 0,
                (unsigned)currentGalaxy + 1U, (unsigned)currentSystem, curName,
                (unsigned)targetSystem, tgtName,
                (unsigned long)buzzHashRate, (unsigned long)buzzBestBits, (unsigned)buzzTargetBits,
                (unsigned)buzzTxFailStreak, (unsigned long)buzzEspNowRescues,
                shtFresh ? 'S' : '-', qmpFresh ? 'Q' : '-', buzzCorpusSdReady ? 1 : 0);
}

void advCapTick() {
  uint32_t now = millis();
  advCapMaybeFinishLoRa(now);
  if (!advSkyAnchorEnabled) return;
  advCapReadGnss();
  advCapUpdateSkyLock(now);
  advSendPnCortex();
  advCapMaybeSendLoRa(now);
  advSkyHealthTick(now);
}

void buzzAdaptMiningBatch(uint32_t now) {
  static uint32_t lastAdaptMs = 0;
  if (now - lastAdaptMs < 2000UL) return;
  lastAdaptMs = now;

  int target = buzzNightMode ? 180 : 140;
  bool masterPresent = buzzLastMasterMs && (now - buzzLastMasterMs < 9000UL);
  if (!buzzJob.active || !masterPresent || !buzzMinerEnabled) {
    target = 60;
  } else {
    target = buzzNightMode ? 190 : 150;
    if (buzzAgentHint == 3) target += 70;
    else if (buzzAgentHint == 2) target += 30;
    if (buzzAgentBatch >= BUZZ_BATCH_MIN && buzzAgentBatch <= BUZZ_BATCH_MAX) {
      target = (target * 2 + (int)buzzAgentBatch) / 3;
    }
    if (docked) target += 35;
    if (viewMode == VIEW_MAP || viewMode == VIEW_DOCKED || viewMode == VIEW_MINER) target += 10;
    if (!docked && viewMode == VIEW_FLIGHT) target -= 25;
    if (shieldPct < 45.0f || energyPct < 38.0f || laserHeat > 70.0f) target -= 80;
    if (millis() < hitFlashUntilMs) target -= 90;
    if (buzzHashRate > 2800 && buzzNightMode) target += 20;
    else if (buzzHashRate < 900 && buzzAgentHint < 3) target -= 20;
    if (!buzzNightMode) target = min(target, 220);
  }

  target = constrain(target, BUZZ_BATCH_MIN, BUZZ_BATCH_MAX);
  buzzMiningBatch = (uint16_t)((buzzMiningBatch * 3 + target) / 4);
}

void buzzRunMining() {
  uint32_t now = millis();
  if (!buzzMinerEnabled || !buzzEspNowReady) {
    if (now - buzzLastHashTickMs >= 1000UL) {
      buzzHashRate = 0;
      buzzHashCounter = 0;
      buzzLastHashTickMs = now;
    }
    return;
  }

  if (!buzzJob.active) {
    if (now - buzzLastHashTickMs >= 1000UL) {
      buzzHashRate = 0;
      buzzHashCounter = 0;
      buzzLastHashTickMs = now;
    }
    return;
  }

  if (now - buzzJob.receivedAt > BUZZ_JOB_TIMEOUT_MS) {
    buzzJob.active = false;
    buzzJobsExpired++;
    buzzHashRate = 0;
    return;
  }

  uint8_t header[80];
  uint8_t rawHash[32];
  uint8_t shareHash[32];
  uint16_t batch = buzzActiveBatch();
  uint32_t budgetUs = buzzNightMode ? BUZZ_NIGHT_BUDGET_US : BUZZ_ACTIVE_BUDGET_US;
  uint32_t sliceStart = micros();
  uint16_t checkedThisSlice = 0;

  while (checkedThisSlice < batch && (uint32_t)(micros() - sliceStart) < budgetUs) {
    if (buzzJob.hashesDone >= buzzJob.rangeSize) {
      buzzJob.active = false;
      buzzJobsDone++;
      break;
    }

    uint32_t nonce = buzzNextNonce(buzzJob, buzzJob.hashesDone);
    buzzJob.nonce = nonce + 1;
    buzzJob.hashesDone++;
    checkedThisSlice++;

    memcpy(header, buzzJob.header, 80);
    buzzWriteLE32(header + 76, nonce);
    buzzDoubleSha256(header, 80, rawHash);
    buzzHashToShareOrder(rawHash, shareHash);

    buzzHashCounter++;
    buzzTotalHashes++;
    uint16_t bits = buzzCountLeadingZeroBitsBE(shareHash);
    if (bits > buzzBestBits) {
      buzzBestBits = bits;
      buzzBestNonce = nonce;
    }

    if (bits >= 20) buzzNear20++;
    if (bits >= 21) buzzNear21++;
    if (bits >= 22) buzzNear22++;
    bool bitsCandidate = (bits >= buzzTargetBits);
    bool targetOk = bitsCandidate && buzzHashMeetsTargetBE(shareHash, buzzJob.target);
    if (bitsCandidate && !targetOk) buzzTargetCompareFails++;
    bool shareOk = targetOk;
    buzzA9Observe(bits, shareOk);

    if (shareOk) {
      buzzSendShare(nonce, bits, shareHash);
      buzzJob.active = false;
      buzzJobsDone++;
      buzzA9SaveCorpus(true);
      break;
    }
  }

  if (now - buzzLastHashTickMs >= 1000UL) {
    buzzHashRate = buzzHashCounter;
    buzzHashRateEma = (buzzHashRateEma <= 0.1f) ? (float)buzzHashRate : (buzzHashRateEma * 0.78f + (float)buzzHashRate * 0.22f);
    buzzHashCounter = 0;
    buzzLastHashTickMs = now;
  }
}

void buzzMinerTick() {
  uint32_t now = millis();
  buzzEspNowRescueTick(now);
  buzzSendHeartbeat();
  buzzSendSwarmSense();
  buzzSendBeaconEnvTelemetry();
  buzzSendPilotLink();
  advCapTick();
  knownCosmosScanSd(false);
  buzzAdaptMiningBatch(millis());
  buzzRunMining();

  now = millis();
  buzzCorpusTick(now);
  if (now - buzzLastDebugMs >= BUZZ_MINER_DEBUG_MS) {
    buzzLastDebugMs = now;
    Serial.printf("[CARD/A9] ready=%u on=%u night=%u mode=%s job=%u H=%lu ema=%.0f total=%lu best=%lu target=%u shares=%lu near=%lu/%lu/%lu cmpFail=%lu corpus=%lu/%lu/%lu store=%s save=%lu prune=%lu dirty=%u jobs=%lu/%lu/%lu defer=%lu repl=%lu rx=%lu/%lu tx=%lu/%lu fs=%lu resc=%lu masterAge=%lums lane=%s strat=%s/s%u/a%u checked=%lu/%lu broad=%lu/%lu/%lu rand=%lu/%lu/%lu\n",
                  buzzEspNowReady ? 1 : 0, buzzMinerEnabled ? 1 : 0, buzzNightMode ? 1 : 0,
                  buzzLegacyA9Mode ? "legacy" : "broad",
                  buzzJob.active ? 1 : 0, (unsigned long)buzzHashRate, buzzHashRateEma,
                  (unsigned long)buzzTotalHashes,
                  (unsigned long)buzzBestBits, (unsigned)buzzTargetBits, (unsigned long)buzzShares,
                  (unsigned long)buzzNear20, (unsigned long)buzzNear21, (unsigned long)buzzNear22,
                  (unsigned long)buzzTargetCompareFails,
                  (unsigned long)buzzCorpusHashes, (unsigned long)buzzCorpusBestBits,
                  (unsigned long)buzzCorpusShares, buzzCorpusStoreName(),
                  (unsigned long)buzzCorpusSaves, (unsigned long)buzzCorpusPrunes,
                  buzzCorpusDirty ? 1 : 0,
                  (unsigned long)buzzJobsSeen, (unsigned long)buzzJobsDone, (unsigned long)buzzJobsExpired,
                  (unsigned long)buzzJobsDeferred, (unsigned long)buzzJobsReplaced,
                  (unsigned long)buzzRxSeen, (unsigned long)buzzRxJobs,
                  (unsigned long)buzzTxOk, (unsigned long)buzzTxFail,
                  (unsigned long)buzzTxFailStreak, (unsigned long)buzzEspNowRescues,
                  (unsigned long)(buzzLastMasterMs ? now - buzzLastMasterMs : 999999UL),
                  buzzLaneName(buzzJob.minerLane), buzzStrategyName(buzzJob.minerStrategy),
                  (unsigned)buzzJob.minerSector, (unsigned)buzzJob.minerStrideArm,
                  (unsigned long)buzzJob.hashesDone, (unsigned long)buzzJob.rangeSize,
                  (unsigned long)buzzA9BroadHashes, (unsigned long)buzzA9BroadBest, (unsigned long)buzzA9BroadShares,
                  (unsigned long)buzzA9RandomHashes, (unsigned long)buzzA9RandomBest, (unsigned long)buzzA9RandomShares);
  }
}

Vec3 cross3(const Vec3 &a, const Vec3 &b) {
  return {
    a.y * b.z - a.z * b.y,
    a.z * b.x - a.x * b.z,
    a.x * b.y - a.y * b.x
  };
}

void normalizeVec(Vec3 &v) {
  float len = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
  if (len < 0.0001f) return;
  v.x /= len;
  v.y /= len;
  v.z /= len;
}

void rotateVecAroundAxis(Vec3 &v, const Vec3 &axis, float angle) {
  float c = cosf(angle);
  float s = sinf(angle);
  float d = dot3(axis, v);
  Vec3 cr = cross3(axis, v);
  v = {
    v.x * c + cr.x * s + axis.x * d * (1.0f - c),
    v.y * c + cr.y * s + axis.y * d * (1.0f - c),
    v.z * c + cr.z * s + axis.z * d * (1.0f - c)
  };
}

void orthonormalizeShipBasis() {
  normalizeVec(shipForwardVec);
  shipRightVec = cross3(shipUpVec, shipForwardVec);
  normalizeVec(shipRightVec);
  shipUpVec = cross3(shipForwardVec, shipRightVec);
  normalizeVec(shipUpVec);
  shipYaw = atan2f(shipForwardVec.x, shipForwardVec.z);
  shipPitch = asinf(clampf(shipForwardVec.y, -1.0f, 1.0f));
}

void resetShipOrientation() {
  shipRightVec = {1.0f, 0.0f, 0.0f};
  shipUpVec = {0.0f, 1.0f, 0.0f};
  shipForwardVec = {0.0f, 0.0f, 1.0f};
  shipYaw = 0.0f;
  shipPitch = 0.0f;
  shipRoll = 0.0f;
  shipYawRate = 0.0f;
  shipPitchRate = 0.0f;
  shipRollRate = 0.0f;
}

void applyShipRotation(float dt) {
  float yawAngle = shipYawRate * dt;
  float pitchAngle = shipPitchRate * dt;
  float rollAngle = shipRollRate * dt;

  if (fabsf(yawAngle) > 0.00001f) {
    rotateVecAroundAxis(shipForwardVec, shipUpVec, yawAngle);
    rotateVecAroundAxis(shipRightVec, shipUpVec, yawAngle);
  }
  if (fabsf(pitchAngle) > 0.00001f) {
    rotateVecAroundAxis(shipForwardVec, shipRightVec, pitchAngle);
    rotateVecAroundAxis(shipUpVec, shipRightVec, pitchAngle);
  }
  if (fabsf(rollAngle) > 0.00001f) {
    rotateVecAroundAxis(shipRightVec, shipForwardVec, rollAngle);
    rotateVecAroundAxis(shipUpVec, shipForwardVec, rollAngle);
    shipRoll = wrapAngle(shipRoll + rollAngle);
  }
  orthonormalizeShipBasis();
}

void getShipBasis(Vec3 &right, Vec3 &up, Vec3 &forward) {
  right = shipRightVec;
  up = shipUpVec;
  forward = shipForwardVec;
}

void getCameraBasis(Vec3 &right, Vec3 &up, Vec3 &forward) {
  Vec3 shipRight, shipUp, shipForward;
  getShipBasis(shipRight, shipUp, shipForward);
  up = shipUp;

  switch (cockpitView) {
    case LOOK_REAR:
      right = {-shipRight.x, -shipRight.y, -shipRight.z};
      forward = {-shipForward.x, -shipForward.y, -shipForward.z};
      break;
    case LOOK_LEFT:
      right = {shipForward.x, shipForward.y, shipForward.z};
      forward = {-shipRight.x, -shipRight.y, -shipRight.z};
      break;
    case LOOK_RIGHT:
      right = {-shipForward.x, -shipForward.y, -shipForward.z};
      forward = {shipRight.x, shipRight.y, shipRight.z};
      break;
    default:
      right = shipRight;
      forward = shipForward;
      break;
  }
}

void updateZoneByPosition() {
  stationDistance = dist3(shipX, shipY, shipZ, stationWorld.x, stationWorld.y, stationWorld.z);
  float beltDistance = dist3(shipX, shipY, shipZ, beltWorld.x, beltWorld.y, beltWorld.z);
  pirateDenDistance = dist3(shipX, shipY, shipZ, pirateDenWorld.x, pirateDenWorld.y, pirateDenWorld.z);
  if (pirateDenDistance < 900.0f) zone = ZONE_PIRATE_DEN;
  else if (beltDistance < 1050.0f) zone = ZONE_BELT;
  else zone = ZONE_STATION;

  if (navTarget == NAV_BELT) navDistance = beltDistance;
  else if (navTarget == NAV_STATION) navDistance = stationDistance;
  else if (navTarget == NAV_PIRATE_DEN) navDistance = pirateDenDistance;
  else navDistance = 0.0f;
}

uint16_t dim565(uint16_t color, float k) {
  k = clampf(k, 0.0f, 1.0f);
  uint8_t r = ((color >> 11) & 0x1F) * 255 / 31;
  uint8_t g = ((color >> 5) & 0x3F) * 255 / 63;
  uint8_t b = (color & 0x1F) * 255 / 31;
  return canvas.color565((uint8_t)(r * k), (uint8_t)(g * k), (uint8_t)(b * k));
}

const char* factionName(Faction f) {
  switch (f) {
    case FAC_FREE_MINERS: return "Free Miners";
    case FAC_MACHINE_REMNANTS: return "Machine Rem";
    default: return "Zoveon League";
  }
}

const char* cockpitViewName() {
  switch (cockpitView) {
    case LOOK_REAR: return "REAR";
    case LOOK_LEFT: return "LEFT";
    case LOOK_RIGHT: return "RIGHT";
    default: return "FWD";
  }
}

bool stationFactionAllied() {
  return factionRep[(uint8_t)stationFaction] > 0;
}

void adjustRep(Faction f, int8_t delta) {
  int v = (int)factionRep[(uint8_t)f] + delta;
  if (v > 50) v = 50;
  if (v < -50) v = -50;
  factionRep[(uint8_t)f] = (int8_t)v;
}

const char* stationTabName(StationTab tab) {
  switch (tab) {
    case ST_TAB_MARKET: return "MARKET";
    case ST_TAB_BAR: return "BAR";
    case ST_TAB_SHIP: return "SHIP";
    default: return "DOK";
  }
}

const char* minerTabName(MinerTab tab) {
  return tab == MINER_TAB_BEACON ? "BEACON" : "A9";
}

const char* beaconRowName(BeaconMenuRow row) {
  switch (row) {
    case BEACON_ROW_PROFILE: return "PROFILE";
    case BEACON_ROW_ENV: return "ENV/QMP";
    case BEACON_ROW_SKY: return "SKY CAP";
    case BEACON_ROW_LED: return "LED";
    case BEACON_ROW_IMU: return "IMU";
    case BEACON_ROW_A9_MODE: return "A9 MODE";
    case BEACON_ROW_A9_SAVE: return "A9 SAVE";
    case BEACON_ROW_A9_PRUNE: return "A9 PRUNE";
    case BEACON_ROW_A9_TAB: return "A9 TAB";
    default: return "BUZZ";
  }
}

String beaconRowState(BeaconMenuRow row) {
  switch (row) {
    case BEACON_ROW_PROFILE: return buzzNightMode ? "NIGHT" : "ACTIVE";
    case BEACON_ROW_ENV:
      return String(envSensorsEnabled ? "ON " : "OFF ") +
             String(envShtFreshNow(millis()) ? "S" : "-") +
             String(envQmpFreshNow(millis()) ? "Q" : "-");
    case BEACON_ROW_SKY:
      return String(advSkyAnchorEnabled ? "ON " : "OFF ") +
             String(advCapGnssFreshNow(millis()) ? "G" : "-") +
             String(advCapLoRaReady ? "L" : "-") +
             String(" ") + String((int)constrain((int)(advCapSkyLock * 100.0f), 0, 100));
    case BEACON_ROW_LED: return shipLedEnabled ? "ON" : "OFF";
    case BEACON_ROW_IMU: return imuAssistEnabled ? "ASSIST" : "OFF";
    case BEACON_ROW_A9_MODE: return buzzLegacyA9Mode ? "LEGACY" : "BROAD";
    case BEACON_ROW_A9_SAVE:
      return String(buzzCorpusStoreName()) + String(buzzCorpusDirty ? "*" : " OK");
    case BEACON_ROW_A9_PRUNE:
      return String(buzzCorpusPrunes) + String(" RUN");
    case BEACON_ROW_A9_TAB: return "OPEN";
    default: return buzzMinerEnabled ? "ON" : "OFF";
  }
}

void minerMoveTab(int8_t delta) {
  int next = (int)minerTab + delta;
  while (next < 0) next += MINER_TAB_COUNT;
  while (next >= MINER_TAB_COUNT) next -= MINER_TAB_COUNT;
  minerTab = (MinerTab)next;
  statusLine = minerTab == MINER_TAB_BEACON ? "Beacon legacy tab" : "RBLGANUL A9 deck";
}

void beaconMoveRow(int8_t delta) {
  int next = (int)beaconMenuRow + delta;
  while (next < 0) next += BEACON_ROW_COUNT;
  while (next >= BEACON_ROW_COUNT) next -= BEACON_ROW_COUNT;
  beaconMenuRow = (BeaconMenuRow)next;
  statusLine = String("Beacon ") + beaconRowName(beaconMenuRow);
}

void beaconInteract() {
  switch (beaconMenuRow) {
    case BEACON_ROW_BUZZ:
      buzzMinerEnabled = !buzzMinerEnabled;
      if (!buzzMinerEnabled) {
        buzzJob.active = false;
        buzzHashRate = 0;
        buzzHashCounter = 0;
      }
      statusLine = buzzMinerEnabled ? "Beacon: Buzz ON" : "Beacon: Buzz OFF";
      break;
    case BEACON_ROW_PROFILE:
      buzzNightMode = !buzzNightMode;
      buzzAgentBatch = buzzNightMode ? BUZZ_BATCH_NIGHT : BUZZ_BATCH_ACTIVE;
      statusLine = buzzNightMode ? "Beacon: NIGHT" : "Beacon: ACTIVE";
      break;
    case BEACON_ROW_ENV:
      envSensorsEnabled = !envSensorsEnabled;
      if (envSensorsEnabled) {
        envLastReadMs = millis() - ENV_READ_MS;
        if (!envQmpReady) initQMP6988Optional();
        readEnvSensors();
      }
      statusLine = envSensorsEnabled ? "Beacon: ENV/QMP ON" : "Beacon: ENV/QMP OFF";
      break;
    case BEACON_ROW_SKY:
      advSkyAnchorEnabled = !advSkyAnchorEnabled;
      if (advSkyAnchorEnabled) {
        advSkyLastTxMs = 0;
        advCapLastGnssReadMs = 0;
      }
      statusLine = advSkyAnchorEnabled ? "Beacon: SKY CAP ON" : "Beacon: SKY CAP OFF";
      break;
    case BEACON_ROW_LED:
      shipLedEnabled = !shipLedEnabled;
      if (!shipLedEnabled) setShipLed(0, 0, 0);
      statusLine = shipLedEnabled ? "Beacon: LED ON" : "Beacon: LED OFF";
      break;
    case BEACON_ROW_IMU:
      imuAssistEnabled = !imuAssistEnabled;
      imuFiltYaw = 0.0f;
      imuFiltPitch = 0.0f;
      shipYawRate *= 0.25f;
      shipPitchRate *= 0.25f;
      statusLine = imuAssistEnabled ? "Beacon: IMU assist" : "Beacon: IMU OFF";
      break;
    case BEACON_ROW_A9_MODE:
      buzzLegacyA9Mode = !buzzLegacyA9Mode;
      buzzJob.active = false;
      statusLine = buzzLegacyA9Mode ? "A9 mode: LEGACY" : "A9 mode: BROAD";
      break;
    case BEACON_ROW_A9_SAVE: {
      bool ok = buzzA9SaveCorpus(true);
      statusLine = ok ? String("A9 corpus saved ") + buzzCorpusStoreName() : "A9 corpus save failed";
      break;
    }
    case BEACON_ROW_A9_PRUNE:
      buzzA9PruneCorpus(true);
      buzzA9SaveCorpus(true);
      break;
    case BEACON_ROW_A9_TAB:
      minerTab = MINER_TAB_A9;
      statusLine = "RBLGANUL A9 deck";
      break;
    default:
      break;
  }
}

float beaconLegacyEntropy() {
  uint32_t now = millis();
  float target = max(1.0f, (float)buzzTargetBits);
  float bitsSignal = ((float)buzzBestBits / target) * 4.2f;
  float hashSignal = constrain(buzzHashRateEma / 2400.0f, 0.0f, 3.0f);
  float radioSignal = (buzzEspNowReady ? 0.35f : 0.0f) + ((now - buzzLastMasterMs < 9000UL) ? 0.55f : 0.0f);
  float shareSignal = min(2.0f, (float)buzzShares * 0.22f);
  float envSignal = envShtFreshNow(now) ? (fabsf(envTempC - envPredTempC) * 0.18f + fabsf(envHumidity - envPredHumidity) * 0.018f) : 0.0f;
  float qmpSignal = envQmpFreshNow(now) ? constrain(envPressureLoss * 0.05f, 0.0f, 1.2f) : 0.0f;
  return clampf(bitsSignal + hashSignal + radioSignal + shareSignal + envSignal + qmpSignal, 0.0f, 10.0f);
}

float beaconLegacyLoss() {
  float txTotal = (float)(buzzTxOk + buzzTxFail + 1UL);
  float txLoss = (float)buzzTxFail / txTotal;
  float masterLoss = (buzzLastMasterMs && millis() - buzzLastMasterMs < 9000UL) ? 0.0f : 1.0f;
  float jobLoss = buzzJob.active ? 0.0f : 0.28f;
  return clampf(txLoss * 4.0f + masterLoss + jobLoss, 0.0f, 9.99f);
}

float beaconLegacyFit() {
  float h = constrain(buzzHashRateEma / 8000.0f, 0.0f, 1.8f);
  float best = (float)buzzBestBits / max(1.0f, (float)buzzTargetBits);
  float shares = min(1.5f, (float)buzzShares * 0.18f);
  return clampf(h + best + shares - beaconLegacyLoss() * 0.18f, -9.0f, 9.0f);
}

float beaconLegacyM2R() {
  float r = (float)(buzzA9BroadBest + 1UL) / (float)(buzzA9RandomBest + 1UL);
  return clampf(r, 0.05f, 20.0f);
}

uint8_t beaconLegacyTheta() {
  uint32_t mix = buzzAgentEntropySeed ^ buzzTotalHashes ^ ((uint32_t)buzzBestBits << 16) ^ ((uint32_t)buzzShares * 131UL);
  return (uint8_t)constrain((int)((buzzBestBits * 7UL + (mix & 0x7F)) & 0xFF), 0, 255);
}

uint8_t beaconLegacyAiNodes() {
  uint8_t n = 0;
  if (buzzEspNowReady) n++;
  if (buzzLastMasterMs && millis() - buzzLastMasterMs < 9000UL) n++;
  if (buzzRxRewards) n++;
  if (buzzShares) n++;
  return n;
}

uint8_t stationRowCount(StationTab tab) {
  switch (tab) {
    case ST_TAB_MARKET: return 4;
    case ST_TAB_BAR: return 4;
    case ST_TAB_SHIP: return 4;
    default: return 4;
  }
}

const char* stationRowName(StationTab tab, uint8_t row) {
  switch (tab) {
    case ST_TAB_MARKET:
      switch (row) {
        case 0: return "Prodat rudu";
        case 1: return "Kupit toplivo";
        case 2: return "Cargo rack +4";
        default: return "Ceny rynku";
      }
    case ST_TAB_BAR:
      switch (row) {
        case 0: return "Sluh: poyas";
        case 1: return "Sluh: logovo";
        case 2: return "Escort miners";
        default: return "Novosti frakcii";
      }
    case ST_TAB_SHIP:
      switch (row) {
        case 0: return "LED indikator";
        case 1: return "IMU zero";
        case 2: return "Status korablya";
        default: return "Karta galaktiki";
      }
    default:
      switch (row) {
        case 0: return "Vylet";
        case 1: return "Remont+toplivo";
        case 2: return "Status korablya";
        default: return "Karta galaktiki";
      }
  }
}

void clampStationRow() {
  uint8_t rows = stationRowCount(stationTab);
  if (stationRow >= rows) stationRow = rows - 1;
}

void stationMoveTab(int8_t delta) {
  int next = (int)stationTab + delta;
  while (next < 0) next += ST_TAB_COUNT;
  while (next >= ST_TAB_COUNT) next -= ST_TAB_COUNT;
  stationTab = (StationTab)next;
  stationRow = 0;
  statusLine = String("Vkladka ") + stationTabName(stationTab);
}

void stationMoveRow(int8_t delta) {
  uint8_t rows = stationRowCount(stationTab);
  int next = (int)stationRow + delta;
  while (next < 0) next += rows;
  while (next >= rows) next -= rows;
  stationRow = (uint8_t)next;
}

void applyDockServices(bool emergency) {
  shieldPct = 100.0f;
  energyPct = 100.0f;
  laserHeat = 0.0f;
  shieldOfflineUntilMs = 0;

  if (stationFactionAllied()) {
    hullPct = 100.0f;
    fuelPct = 100.0f;
    statusLine = emergency ? "Soyuz spas: remont+toplivo" : "Soyuz servis: free repair+fuel";
  } else {
    if (emergency) hullPct = max(hullPct, 35.0f);
    fuelPct = max(fuelPct, 12.0f);
    statusLine = emergency ? "Neutral evac: bazoviy remont" : "Dock OK: neutral servis";
  }
}

void applyBrightness() {
  M5.Display.setBrightness(brightnessLevels[brightnessIndex]);
  if (M5.Led.isEnabled()) {
    uint8_t ledBrightness = (uint8_t)map(brightnessLevels[brightnessIndex], 0, 220, 8, 96);
    M5.Led.setBrightness(ledBrightness);
    ledColorValid = false;
  }
}

void applyGameVolume() {
  M5Cardputer.Speaker.setVolume(gameVolume);
}

void changeGameVolume(int delta) {
  int next = (int)gameVolume + delta;
  gameVolume = (uint8_t)constrain(next, 0, 255);
  applyGameVolume();
  statusLine = String("Zvuk ") + String((unsigned)gameVolume);
}

bool readSHT30(float &temp, float &hum) {
  Wire.beginTransmission(SHT30_ADDR);
  Wire.write(0x2C);
  Wire.write(0x06);
  if (Wire.endTransmission() != 0) return false;
  delay(8);
  Wire.requestFrom(SHT30_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;
  uint8_t data[6];
  for (uint8_t i = 0; i < 6; ++i) data[i] = Wire.read();
  uint16_t rawTemp = ((uint16_t)data[0] << 8) | data[1];
  uint16_t rawHum = ((uint16_t)data[3] << 8) | data[4];
  temp = -45.0f + 175.0f * (float)rawTemp / 65535.0f;
  hum = 100.0f * (float)rawHum / 65535.0f;
  return isfinite(temp) && isfinite(hum) && temp > -40.0f && temp < 90.0f && hum >= 0.0f && hum <= 100.0f;
}

void initQMP6988Optional() {
  bool ok = false;
  uint32_t now = millis();
#ifdef QMP6988_SLAVE_ADDRESS_L
  ok = janusQmp6988.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, GROVE_SDA_PIN, GROVE_SCL_PIN, 400000U);
  Serial.printf("[ENV/QMP] begin QMP6988_SLAVE_ADDRESS_L=0x%02X -> %d\n", QMP6988_SLAVE_ADDRESS_L, ok ? 1 : 0);
#endif
  if (!ok) {
    ok = janusQmp6988.begin(&Wire, 0x70, GROVE_SDA_PIN, GROVE_SCL_PIN, 400000U);
    Serial.printf("[ENV/QMP] begin 0x70 -> %d\n", ok ? 1 : 0);
  }
  if (!ok) {
    ok = janusQmp6988.begin(&Wire, 0x56, GROVE_SDA_PIN, GROVE_SCL_PIN, 400000U);
    Serial.printf("[ENV/QMP] begin 0x56 -> %d\n", ok ? 1 : 0);
  }
  envQmpReady = ok;
  if (envQmpReady) {
    janusQmp6988.update();
    float p = janusQmp6988.pressure / 100.0f;
    if (p > 300.0f && p < 1200.0f) {
      envPressureHpa = p;
      envPredPressureHpa = p;
      envQmpLastOkMs = now;
    }
  }
}

void readEnvQmp() {
  static uint32_t lastQmpRetryMs = 0;
  if (!envQmpReady) {
    if (millis() - lastQmpRetryMs > 8000UL) {
      lastQmpRetryMs = millis();
      initQMP6988Optional();
    }
    return;
  }

  janusQmp6988.update();
  float p = janusQmp6988.pressure / 100.0f;
  if (p > 300.0f && p < 1200.0f) {
    if (envPredPressureHpa <= 0.1f) envPredPressureHpa = p;
    envPressureHpa = (envQmpReads == 0) ? p : (envPressureHpa * 0.92f + p * 0.08f);
    envPressureLoss = fabsf(envPredPressureHpa - envPressureHpa);
    envPredPressureHpa = envPredPressureHpa * 0.985f + envPressureHpa * 0.015f;
    envQmpLastOkMs = millis();
    envQmpReads++;
  } else {
    if (!envQmpLastOkMs || millis() - envQmpLastOkMs > ENV_QMP_REAL_TTL_MS) envQmpReady = false;
  }
}

void readEnvSensors() {
  if (!envSensorsEnabled) {
    envShtReady = false;
    envQmpReady = false;
    return;
  }
  uint32_t now = millis();
  if (now - envLastReadMs < ENV_READ_MS) return;
  envLastReadMs = now;

  float temp = envTempC;
  float hum = envHumidity;
  if (readSHT30(temp, hum)) {
    envShtReady = true;
    envShtLastOkMs = now;
    if (envShtReads == 0) {
      envTempC = temp;
      envHumidity = hum;
      envPredTempC = temp;
      envPredHumidity = hum;
    } else {
      float prevTemp = envTempC;
      float prevHum = envHumidity;
      envTempC = envTempC * 0.82f + temp * 0.18f;
      envHumidity = envHumidity * 0.82f + hum * 0.18f;
      envPredTempC = envTempC + (envTempC - prevTemp) * 0.35f;
      envPredHumidity = envHumidity + (envHumidity - prevHum) * 0.35f;
    }
    envShtReads++;
  } else if (!envShtLastOkMs || now - envShtLastOkMs > ENV_REAL_TTL_MS) {
    envShtReady = false;
  }

  readEnvQmp();
  if (envQmpLastOkMs && now - envQmpLastOkMs > ENV_QMP_REAL_TTL_MS) envQmpReady = false;
}

void setShipLed(uint8_t r, uint8_t g, uint8_t b) {
  if (!M5.Led.isEnabled()) return;
  if (ledColorValid && lastLedR == r && lastLedG == g && lastLedB == b) return;
  lastLedR = r;
  lastLedG = g;
  lastLedB = b;
  ledColorValid = true;
  M5.Led.setAllColor(r, g, b);
  M5.Led.display();
}

void hullLedColor(uint8_t &r, uint8_t &g, uint8_t &b) {
  float hp = clampf(hullPct, 0.0f, 100.0f);
  if (hp >= 50.0f) {
    float t = (hp - 50.0f) / 50.0f; // yellow -> green
    r = (uint8_t)(220.0f * (1.0f - t));
    g = (uint8_t)(120.0f + 70.0f * t);
    b = 0;
  } else {
    float t = hp / 50.0f; // red -> yellow
    r = 220;
    g = (uint8_t)(120.0f * t);
    b = 0;
  }
}

void hitLedColor(HitSide side, uint8_t &r, uint8_t &g, uint8_t &b) {
  switch (side) {
    case HIT_LEFT:  r = 0;   g = 70;  b = 255; break;
    case HIT_RIGHT: r = 255; g = 130; b = 0;   break;
    case HIT_REAR:  r = 150; g = 0;   b = 255; break;
    default:        r = 255; g = 24;  b = 0;   break;
  }
}

void updateShipLed() {
  if (millis() - lastLedMs < 80UL) return;
  lastLedMs = millis();

  if (!shipLedEnabled) {
    setShipLed(0, 0, 0);
    return;
  }

  if (viewMode == VIEW_MINER) {
    uint32_t now = millis();
    if (now < buzzShareFlashUntilMs) {
      if ((now / 120UL) & 1UL) setShipLed(255, 210, 70);
      else setShipLed(255, 72, 0);
    } else {
      setShipLed(180, 82, 0);
    }
    return;
  }

  uint8_t r = 0, g = 0, b = 0;
  if (millis() < hitFlashUntilMs) {
    hitLedColor(lastHitSide, r, g, b);
  } else if (shieldPct > 1.0f && millis() >= shieldOfflineUntilMs) {
    float k = 0.35f + 0.65f * (shieldPct / 100.0f);
    r = 0;
    g = (uint8_t)(150.0f * k);
    b = (uint8_t)(165.0f * k);
  } else {
    hullLedColor(r, g, b);
  }

  setShipLed(r, g, b);
}

HitSide hitSideFromBody(const Body &b) {
  Vec3 right, up, forward;
  getShipBasis(right, up, forward);
  Vec3 rel = {b.x - shipX, b.y - shipY, b.z - shipZ};
  float side = dot3(rel, right);
  float ahead = dot3(rel, forward);
  if (fabsf(side) > 80.0f) return (side < 0.0f) ? HIT_LEFT : HIT_RIGHT;
  if (ahead < 0.0f) return HIT_REAR;
  return HIT_FRONT;
}

void registerIncomingHit(HitSide side, float amount) {
  lastHitSide = side;
  hitFlashUntilMs = millis() + 520UL;
  alertFlash = 6;

  if (shieldPct > 0.0f) {
    float absorbed = min(shieldPct, amount);
    shieldPct = clampf(shieldPct - amount, 0.0f, 100.0f);
    float bleed = max(0.0f, amount - absorbed);
    if (bleed > 0.0f) hullPct = clampf(hullPct - bleed * 1.4f, 0.0f, 100.0f);
    if (shieldPct <= 0.0f) shieldOfflineUntilMs = millis() + 4500UL;
    statusLine = "Shchit prinyal udar";
  } else {
    hullPct = clampf(hullPct - amount * 1.6f, 0.0f, 100.0f);
    shieldOfflineUntilMs = millis() + 4500UL;
    statusLine = "Korpus povrezhden";
  }
}

float imuDeadzone(float v, float dz) {
  if (fabsf(v) < dz) return 0.0f;
  return (v > 0.0f) ? (v - dz) : (v + dz);
}

void calibrateImuZero() {
  if (!imuReady) return;
  float sgx = 0.0f, sgy = 0.0f, sgz = 0.0f;
  const int samples = 42;
  for (int i = 0; i < samples; ++i) {
    float gx = 0.0f, gy = 0.0f, gz = 0.0f;
    M5.Imu.getGyroData(&gx, &gy, &gz);
    sgx += gx;
    sgy += gy;
    sgz += gz;
    delay(4);
  }
  imuBiasGx = sgx / samples;
  imuBiasGy = sgy / samples;
  imuBiasGz = sgz / samples;
  imuFiltYaw = 0.0f;
  imuFiltPitch = 0.0f;
  shipYawRate = 0.0f;
  shipPitchRate = 0.0f;
  shipRollRate = 0.0f;
  statusLine = "IMU zero + rates clear";
}

void applySoftImuAssist(float dt) {
  if (!imuReady || !imuAssistEnabled || docked || viewMode != VIEW_FLIGHT) return;

  float gx = 0.0f, gy = 0.0f, gz = 0.0f;
  if (!M5.Imu.getGyroData(&gx, &gy, &gz)) return;

  gx = imuDeadzone(gx - imuBiasGx, 0.55f);
  gy = imuDeadzone(gy - imuBiasGy, 0.55f);
  gz = imuDeadzone(gz - imuBiasGz, 0.55f);

  float yawAssist = clampf(gz * 0.010f, -0.20f, 0.20f);
  float pitchAssist = clampf(gy * 0.0075f, -0.14f, 0.14f);
  imuFiltYaw = imuFiltYaw * 0.88f + yawAssist * 0.12f;
  imuFiltPitch = imuFiltPitch * 0.88f + pitchAssist * 0.12f;

  shipYawRate = clampf(shipYawRate + imuFiltYaw * dt * 1.6f, -3.4f, 3.4f);
  shipPitchRate = clampf(shipPitchRate + imuFiltPitch * dt * 1.45f, -3.0f, 3.0f);
}

void resetStars() {
  for (uint8_t i = 0; i < STAR_COUNT; ++i) {
    stars[i] = {
      shipX + frandRange(-1600, 1600),
      shipY + frandRange(-900, 900),
      shipZ + frandRange(-1600, 1600)
    };
  }
}

void resetBody(Body &b, bool rock) {
  b.alive = true;
  if (rock) {
    b.x = beltWorld.x + frandRange(-760, 760);
    b.y = beltWorld.y + frandRange(-230, 230);
    b.z = beltWorld.z + frandRange(-760, 760);
  } else {
    Vec3 right, up, forward;
    getShipBasis(right, up, forward);
    float side = frandRange(-220, 220);
    float high = frandRange(-90, 90);
    float ahead = frandRange(360, 720);
    b.x = shipX + right.x * side + up.x * high + forward.x * ahead;
    b.y = shipY + right.y * side + up.y * high + forward.y * ahead;
    b.z = shipZ + right.z * side + up.z * high + forward.z * ahead;
  }
  b.vx = rock ? frandRange(-1.2f, 1.2f) : frandRange(-3.0f, 3.0f);
  b.vy = rock ? frandRange(-0.6f, 0.6f) : frandRange(-1.6f, 1.6f);
  b.vz = rock ? frandRange(-1.2f, 1.2f) : frandRange(-3.0f, 3.0f);
  b.hp = rock ? random(2, 5) : random(3, 7);
}

void resetTrafficBody(Body &b) {
  b.alive = true;
  float angle = frandRange(0.0f, TWO_PI);
  float radius = frandRange(260.0f, 980.0f);
  float lane = frandRange(-120.0f, 120.0f);
  b.x = stationWorld.x + cosf(angle) * radius;
  b.y = stationWorld.y + lane;
  b.z = stationWorld.z + sinf(angle) * radius;
  float tangent = frandRange(4.0f, 10.0f) * ((random(0, 2) == 0) ? -1.0f : 1.0f);
  b.vx = -sinf(angle) * tangent;
  b.vy = frandRange(-0.45f, 0.45f);
  b.vz = cosf(angle) * tangent;
  b.hp = 4;
}

void resetMinerBody(Body &b) {
  b.alive = true;
  float angle = frandRange(0.0f, TWO_PI);
  float radius = frandRange(210.0f, 820.0f);
  b.x = beltWorld.x + cosf(angle) * radius + frandRange(-120.0f, 120.0f);
  b.y = beltWorld.y + frandRange(-170.0f, 170.0f);
  b.z = beltWorld.z + sinf(angle) * radius + frandRange(-120.0f, 120.0f);
  float drift = frandRange(2.0f, 6.0f);
  b.vx = -sinf(angle) * drift + frandRange(-1.0f, 1.0f);
  b.vy = frandRange(-0.35f, 0.35f);
  b.vz = cosf(angle) * drift + frandRange(-1.0f, 1.0f);
  b.hp = 5;
}

void armPirateBody(Body &b, float x, float y, float z) {
  b.alive = true;
  b.x = x;
  b.y = y;
  b.z = z;
  Vec3 chase = {shipX - b.x, shipY - b.y, shipZ - b.z};
  normalizeVec(chase);
  float speed = frandRange(18.0f, 34.0f);
  b.vx = chase.x * speed + frandRange(-2.5f, 2.5f);
  b.vy = chase.y * speed + frandRange(-1.5f, 1.5f);
  b.vz = chase.z * speed + frandRange(-2.5f, 2.5f);
  b.hp = random(4, 8);
}

bool spawnPirateNear(const Vec3 &origin, float radius, const char *label) {
  for (uint8_t i = 0; i < ENEMY_COUNT; ++i) {
    if (enemies[i].alive) continue;
    float a = frandRange(0.0f, TWO_PI);
    float r = frandRange(radius * 0.45f, radius);
    armPirateBody(enemies[i],
                  origin.x + cosf(a) * r,
                  origin.y + frandRange(-150.0f, 150.0f),
                  origin.z + sinf(a) * r);
    if (label) statusLine = label;
    return true;
  }
  return false;
}

void clearBodies() {
  for (uint8_t i = 0; i < ROCK_COUNT; ++i) rocks[i].alive = false;
  for (uint8_t i = 0; i < ENEMY_COUNT; ++i) enemies[i].alive = false;
  for (uint8_t i = 0; i < TRAFFIC_COUNT; ++i) traffic[i].alive = false;
  for (uint8_t i = 0; i < MINER_COUNT; ++i) miners[i].alive = false;
}

void spawnEnemy() {
  Vec3 right, up, forward;
  getShipBasis(right, up, forward);
  Vec3 origin = {
    shipX + forward.x * 540.0f + right.x * frandRange(-180.0f, 180.0f),
    shipY + forward.y * 540.0f + up.y * frandRange(-80.0f, 80.0f),
    shipZ + forward.z * 540.0f + right.z * frandRange(-180.0f, 180.0f)
  };
  if (!spawnPirateNear(origin, 180.0f, "Kontakt: pirat")) statusLine = "Radar uzhe polon";
}

bool projectPoint(float x, float y, float z, int16_t &sx, int16_t &sy, float &scale) {
  Vec3 right, up, forward;
  getCameraBasis(right, up, forward);
  Vec3 rel = {x - shipX, y - shipY, z - shipZ};
  float camX = dot3(rel, right);
  float camY = dot3(rel, up);
  float camZ = dot3(rel, forward);
  if (camZ < 8.0f) return false;
  scale = 95.0f / camZ;
  sx = (int16_t)(SCREEN_W / 2 + camX * scale);
  sy = (int16_t)(SCREEN_H / 2 - camY * scale);
  return sx > -40 && sx < SCREEN_W + 40 && sy > -35 && sy < SCREEN_H + 35;
}

void setFlightView(const char *label) {
  if (!docked) viewMode = VIEW_FLIGHT;
  if (label) statusLine = label;
}

void setCockpitView(CockpitView look) {
  if (docked) {
    statusLine = "Obzor tolko posle vyleta";
    return;
  }
  cockpitView = look;
  viewMode = VIEW_FLIGHT;
  statusLine = "Obzor " + String(cockpitViewName());
}

void launchFromDock() {
  docked = false;
  dockingRun = false;
  viewMode = VIEW_FLIGHT;
  cockpitView = LOOK_FORWARD;
  zone = ZONE_STATION;
  if (navTarget != NAV_PIRATE_DEN) navTarget = NAV_BELT;
  shipX = 0.0f;
  shipY = 0.0f;
  shipZ = -360.0f;
  shipSpeed = 0.08f;
  cruiseMode = false;
  resetShipOrientation();
  updateZoneByPosition();
  clearBodies();
  resetStars();
  statusLine = "Vylet: ruchnoi kurs";
}

void startDockOrLaunch() {
  if (docked) {
    launchFromDock();
    return;
  }

  updateZoneByPosition();
  if (stationDistance > 520.0f) {
    navTarget = NAV_STATION;
    statusLine = "MAYAK ST: leti ruchkami";
    return;
  }

  dockingRun = true;
  dockingStartMs = millis();
  shipSpeed = 0.06f;
  cruiseMode = false;
  viewMode = VIEW_FLIGHT;
  statusLine = "Stykovka: derzhi kurs";
}

void setBeltCourse() {
  if (docked) {
    statusLine = "Snachala vylet";
    return;
  }
  navTarget = NAV_BELT;
  dockingRun = false;
  statusLine = "MAYAK BELT: leti ruchkami";
}

void setStationCourse() {
  if (docked) {
    statusLine = "Uzhe v doke";
    return;
  }
  navTarget = NAV_STATION;
  dockingRun = false;
  statusLine = "MAYAK ST: leti ruchkami";
}

void setPirateDenCourse() {
  if (docked) {
    pirateDenKnown = true;
    navTarget = NAV_PIRATE_DEN;
    statusLine = "Koord logova v nav";
    return;
  }
  pirateDenKnown = true;
  navTarget = NAV_PIRATE_DEN;
  dockingRun = false;
  statusLine = "MAYAK LOGOVO: opasno";
}

void dockRepairRefuel() {
  if (stationFactionAllied()) {
    applyDockServices(false);
    return;
  }

  uint32_t cost = (uint32_t)((100.0f - hullPct) * 2.0f + (100.0f - fuelPct) * 1.0f);
  if (cost < 20UL) cost = 20UL;
  if (credits < cost) {
    statusLine = String("Nuzhno ") + String((unsigned long)cost) + "cr";
    return;
  }
  credits -= cost;
  hullPct = 100.0f;
  fuelPct = 100.0f;
  shieldPct = 100.0f;
  energyPct = 100.0f;
  laserHeat = 0.0f;
  shieldOfflineUntilMs = 0;
  statusLine = "Servis oplachen";
}

void stationInteract() {
  clampStationRow();

  if (stationTab == ST_TAB_DOCK) {
    if (stationRow == 0) launchFromDock();
    else if (stationRow == 1) dockRepairRefuel();
    else if (stationRow == 2) {
      viewMode = VIEW_STATUS;
      statusLine = "Status korablya";
    } else {
      viewMode = VIEW_MAP;
      statusLine = "Karta galaktiki";
    }
    return;
  }

  if (stationTab == ST_TAB_MARKET) {
    if (stationRow == 0) {
      if (ore == 0) {
        statusLine = "Rudy net";
      } else {
        uint32_t value = (uint32_t)ore * eliteOreUnitPrice();
        credits += value;
        ore = 0;
        adjustRep(FAC_FREE_MINERS, 1);
        statusLine = String("Ruda prodana +") + String((unsigned long)value) + "cr";
      }
    } else if (stationRow == 1) {
      if (fuelPct >= 99.0f) {
        statusLine = "Bak uzhe polon";
      } else if (credits >= eliteFuelUnitPrice()) {
        credits -= eliteFuelUnitPrice();
        fuelPct = clampf(fuelPct + 25.0f, 0.0f, 100.0f);
        statusLine = "Toplivo +25";
      } else {
        statusLine = "Malo kreditov";
      }
    } else if (stationRow == 2) {
      if (cargoMax >= 28) {
        statusLine = "Tryum max";
      } else if (credits >= 450UL) {
        credits -= 450UL;
        cargoMax += 4;
        statusLine = "Tryum rasshiren";
      } else {
        statusLine = "Nuzhno 450cr";
      }
    } else {
      statusLine = String("Ruda ") + String((unsigned long)eliteOreUnitPrice()) +
                   "cr Fuel " + String((unsigned long)eliteFuelUnitPrice()) + "cr";
    }
    return;
  }

  if (stationTab == ST_TAB_BAR) {
    if (stationRow == 0) {
      navTarget = NAV_BELT;
      statusLine = "Shahtery videli bogatiy poyas";
    } else if (stationRow == 1) {
      setPirateDenCourse();
    } else if (stationRow == 2) {
      escortMission = true;
      navTarget = NAV_BELT;
      adjustRep(FAC_FREE_MINERS, 1);
      statusLine = "Kontrakt: escort miners";
    } else {
      statusLine = "Pirati davyat poyas";
    }
    return;
  }

  if (stationRow == 0) {
    shipLedEnabled = !shipLedEnabled;
    if (!shipLedEnabled) setShipLed(0, 0, 0);
    statusLine = shipLedEnabled ? "LED indikator ON" : "LED indikator OFF";
  } else if (stationRow == 1) {
    calibrateImuZero();
  } else if (stationRow == 2) {
    viewMode = VIEW_STATUS;
    statusLine = "Status korablya";
  } else {
    viewMode = VIEW_MAP;
    statusLine = "Karta galaktiki";
  }
}

void setPower(uint8_t mode) {
  powerMode = mode;
  if (mode == 1) statusLine = "Pitanie: SYS";
  else if (mode == 2) statusLine = "Pitanie: WEP";
  else if (mode == 3) statusLine = "Pitanie: ENG";
  else statusLine = "Pitanie: BAL";
}

void setThrottle(float speed, const char *label) {
  if (docked) {
    statusLine = "Tyaga tolko posle vyleta";
    return;
  }
  if (viewMode == VIEW_MAP || viewMode == VIEW_STATUS || viewMode == VIEW_MINER) {
    statusLine = "Vernis v kabinu 1-4";
    return;
  }
  dockingRun = false;
  shipSpeed = clampf(speed, THROTTLE_REVERSE_MAX, THROTTLE_FORWARD_MAX);
  if (shipSpeed <= 0.0f) cruiseMode = false;
  viewMode = VIEW_FLIGHT;
  if (label) {
    statusLine = label;
  } else {
    statusLine = "Tyaga " + String((int)throttlePercent()) + "%";
  }
}

void changeThrottle(float delta) {
  setThrottle(shipSpeed + delta, nullptr);
}

void toggleCruise() {
  if (docked || viewMode == VIEW_MAP || viewMode == VIEW_STATUS || viewMode == VIEW_MINER) {
    statusLine = docked ? "Cruise tolko posle vyleta" : "Vernis v kabinu 1-4";
    return;
  }
  cruiseMode = !cruiseMode;
  if (cruiseMode && shipSpeed < 0.30f) shipSpeed = 0.30f;
  if (shipSpeed <= 0.0f) cruiseMode = false;
  statusLine = cruiseMode ? "CRUISE ON" : "CRUISE OFF";
}

void nudgeFlight(float yaw, float pitch, float roll, float throttle, const char *label) {
  if (docked || viewMode == VIEW_MAP || viewMode == VIEW_STATUS || viewMode == VIEW_MINER) return;
  dockingRun = false;
  shipYawRate = clampf(shipYawRate + yaw, -3.4f, 3.4f);
  shipPitchRate = clampf(shipPitchRate + pitch, -3.0f, 3.0f);
  shipRollRate = clampf(shipRollRate + roll, -3.9f, 3.9f);
  shipSpeed = clampf(shipSpeed + throttle, THROTTLE_REVERSE_MAX, THROTTLE_FORWARD_MAX);
  viewMode = VIEW_FLIGHT;
  if (label) statusLine = label;
}

void considerTargetList(TargetPick &best, Body *list, uint8_t count, TargetKind kind, float maxScore) {
  for (uint8_t i = 0; i < count; ++i) {
    if (!list[i].alive) continue;
    int16_t sx, sy;
    float sc;
    if (!projectPoint(list[i].x, list[i].y, list[i].z, sx, sy, sc)) continue;
    float dx = sx - SCREEN_W / 2;
    float dy = sy - SCREEN_H / 2;
    float d = dist3(shipX, shipY, shipZ, list[i].x, list[i].y, list[i].z);
    float score = sqrtf(dx * dx + dy * dy) + d * 0.004f;
    if (score < best.score && score < maxScore) {
      best.kind = kind;
      best.index = i;
      best.score = score;
    }
  }
}

TargetPick findWeaponTarget() {
  TargetPick best = {TARGET_NONE, -1, 9999.0f};
  considerTargetList(best, enemies, ENEMY_COUNT, TARGET_ENEMY, 32.0f);
  considerTargetList(best, rocks, ROCK_COUNT, TARGET_ROCK, 36.0f);
  considerTargetList(best, miners, MINER_COUNT, TARGET_MINER, 32.0f);
  considerTargetList(best, traffic, TRAFFIC_COUNT, TARGET_TRAFFIC, 30.0f);
  return best;
}

void setBeamToBody(const Body &b) {
  int16_t sx, sy;
  float sc;
  if (projectPoint(b.x, b.y, b.z, sx, sy, sc)) {
    laserBeamX = sx;
    laserBeamY = sy;
    laserBeamHit = true;
  }
}

void fireWeapon() {
  if (docked) {
    statusLine = "Oruzhie v doke off";
    return;
  }
  if (laserHeat > 92.0f || energyPct < 4.0f) {
    statusLine = "Lazer peregret";
    return;
  }

  laserFlash = 5;
  laserBeamX = SCREEN_W / 2 + random(-3, 4);
  laserBeamY = SCREEN_H / 2 + random(-2, 3);
  laserBeamHit = false;
  laserHeat = clampf(laserHeat + 12.0f, 0.0f, 100.0f);
  energyPct = clampf(energyPct - 2.0f, 0.0f, 100.0f);

  TargetPick target = findWeaponTarget();
  if (target.kind == TARGET_ENEMY) {
    int e = target.index;
    setBeamToBody(enemies[e]);
    enemies[e].hp -= 2;
    statusLine = "Popadanie po celi";
    if (enemies[e].hp <= 0) {
      enemies[e].alive = false;
      kills++;
      credits += 120;
      statusLine = "Cel unichtozhena +120cr";
    }
    return;
  }

  if (target.kind == TARGET_ROCK) {
    int r = target.index;
    setBeamToBody(rocks[r]);
    rocks[r].hp--;
    statusLine = "Burenie asteroida";
    if (rocks[r].hp <= 0) {
      rocks[r].alive = false;
      if (ore < cargoMax) {
        ore++;
        statusLine = "Ruda v tryume";
      } else {
        statusLine = "Tryum polon";
      }
    }
    return;
  }

  if (target.kind == TARGET_MINER) {
    int m = target.index;
    setBeamToBody(miners[m]);
    miners[m].hp -= 2;
    adjustRep(FAC_FREE_MINERS, -2);
    statusLine = "Ataka shahtera: rep -";
    if (miners[m].hp <= 0) {
      miners[m].alive = false;
      kills++;
      adjustRep(FAC_FREE_MINERS, -5);
      statusLine = "Shahter sbity. Plokhaya slava";
      spawnPirateNear({shipX, shipY, shipZ}, 650.0f, nullptr);
    }
    return;
  }

  if (target.kind == TARGET_TRAFFIC) {
    int t = target.index;
    setBeamToBody(traffic[t]);
    traffic[t].hp -= 2;
    adjustRep(FAC_ZOVEON_LEAGUE, -2);
    statusLine = "Ataka grazhdanskogo: rep -";
    if (traffic[t].hp <= 0) {
      traffic[t].alive = false;
      kills++;
      adjustRep(FAC_ZOVEON_LEAGUE, -6);
      statusLine = "Grazhdanskiy sbity";
    }
    return;
  }

  statusLine = "Ogon v pustotu";
}

void updateBodies(float dt) {
  float beltDistance = dist3(shipX, shipY, shipZ, beltWorld.x, beltWorld.y, beltWorld.z);
  bool inBeltField = (zone == ZONE_BELT || zone == ZONE_PIRATE_DEN);
  if (stationDistance < 2200.0f) {
    for (uint8_t i = 0; i < TRAFFIC_COUNT; ++i) {
      if (!traffic[i].alive) {
        if (random(0, 1000) < 20) resetTrafficBody(traffic[i]);
        continue;
      }
      traffic[i].x += traffic[i].vx * dt;
      traffic[i].y += traffic[i].vy * dt;
      traffic[i].z += traffic[i].vz * dt;
      float dStation = dist3(traffic[i].x, traffic[i].y, traffic[i].z, stationWorld.x, stationWorld.y, stationWorld.z);
      float dShip = dist3(shipX, shipY, shipZ, traffic[i].x, traffic[i].y, traffic[i].z);
      if (dStation > 1350.0f || dShip > 2600.0f) resetTrafficBody(traffic[i]);
    }
  } else {
    for (uint8_t i = 0; i < TRAFFIC_COUNT; ++i) traffic[i].alive = false;
  }

  if (inBeltField) {
    for (uint8_t i = 0; i < ROCK_COUNT; ++i) {
      if (!rocks[i].alive) {
        if (random(0, 1000) < 25) resetBody(rocks[i], true);
        continue;
      }
      rocks[i].x += rocks[i].vx * dt;
      rocks[i].y += rocks[i].vy * dt;
      rocks[i].z += rocks[i].vz * dt;
      float dShip = dist3(shipX, shipY, shipZ, rocks[i].x, rocks[i].y, rocks[i].z);
      float dBelt = dist3(beltWorld.x, beltWorld.y, beltWorld.z, rocks[i].x, rocks[i].y, rocks[i].z);
      if (dShip > 1600.0f || dBelt > 1250.0f) resetBody(rocks[i], true);
    }
  } else if (beltDistance > 1350.0f && pirateDenDistance > 1350.0f) {
    for (uint8_t i = 0; i < ROCK_COUNT; ++i) rocks[i].alive = false;
  }

  if (inBeltField) {
    for (uint8_t i = 0; i < MINER_COUNT; ++i) {
      if (!miners[i].alive) {
        if (random(0, 1000) < (escortMission ? 34 : 18)) resetMinerBody(miners[i]);
        continue;
      }
      miners[i].x += miners[i].vx * dt;
      miners[i].y += miners[i].vy * dt;
      miners[i].z += miners[i].vz * dt;
      float dShip = dist3(shipX, shipY, shipZ, miners[i].x, miners[i].y, miners[i].z);
      float dBelt = dist3(beltWorld.x, beltWorld.y, beltWorld.z, miners[i].x, miners[i].y, miners[i].z);
      if (dShip > 1700.0f || dBelt > 1500.0f) miners[i].alive = false;
    }
  } else if (beltDistance > 1800.0f) {
    for (uint8_t i = 0; i < MINER_COUNT; ++i) miners[i].alive = false;
  }

  uint32_t now = millis();
  if (inBeltField && now - lastEncounterMs > 2800UL) {
    lastEncounterMs = now;
    Vec3 shipOrigin = {shipX, shipY, shipZ};
    if (zone == ZONE_PIRATE_DEN) {
      if (random(0, 1000) < 360) spawnPirateNear(pirateDenWorld, 760.0f, "Logovo: piraty na skanere");
    } else {
      if (random(0, 1000) < (escortMission ? 180 : 95)) {
        spawnPirateNear(shipOrigin, 780.0f, escortMission ? "Escort: piraty idut" : "Sluchainaya vstrecha: piraty");
      } else if (random(0, 1000) < 160) {
        for (uint8_t i = 0; i < MINER_COUNT; ++i) {
          if (!miners[i].alive) {
            resetMinerBody(miners[i]);
            statusLine = "Shahtery rabotayut v poyase";
            break;
          }
        }
      }
    }
  }

  for (uint8_t i = 0; i < ENEMY_COUNT; ++i) {
    if (!enemies[i].alive) continue;
    enemies[i].x += enemies[i].vx * dt;
    enemies[i].y += enemies[i].vy * dt;
    enemies[i].z += enemies[i].vz * dt;
    float dEnemy = dist3(shipX, shipY, shipZ, enemies[i].x, enemies[i].y, enemies[i].z);
    if (dEnemy < 24.0f) {
      registerIncomingHit(hitSideFromBody(enemies[i]), 4.0f);
      resetBody(enemies[i], false);
      statusLine = "Vrag proshel blizko";
    }
    if (dEnemy > 1700.0f) enemies[i].alive = false;

    if (millis() - lastEnemyShotMs > 720UL &&
        dEnemy > 80.0f && dEnemy < 720.0f &&
        random(0, 1000) < 18) {
      lastEnemyShotMs = millis();
      registerIncomingHit(hitSideFromBody(enemies[i]), frandRange(2.0f, 5.5f));
    }
  }
}

void updateDocking() {
  if (!dockingRun) return;
  uint32_t elapsed = millis() - dockingStartMs;
  updateZoneByPosition();
  shipYawRate *= 0.90f;
  shipPitchRate *= 0.90f;
  shipRollRate *= 0.90f;
  shipSpeed = clampf(shipSpeed * 0.985f, 0.035f, 0.12f);
  statusLine = "Stykovka...";

  int16_t sx, sy;
  float sc;
  bool slotVisible = projectPoint(stationWorld.x, stationWorld.y, stationWorld.z, sx, sy, sc);
  float slotError = slotVisible ? sqrtf((sx - SCREEN_W / 2) * (sx - SCREEN_W / 2) + (sy - SCREEN_H / 2) * (sy - SCREEN_H / 2)) : 999.0f;

  if (stationDistance > 620.0f || slotError > 46.0f) {
    dockingRun = false;
    registerIncomingHit(HIT_FRONT, 2.0f);
    statusLine = "Stykovka sorvana";
    return;
  }

  if (elapsed > 5200UL) {
    docked = true;
    dockingRun = false;
    viewMode = VIEW_DOCKED;
    zone = ZONE_STATION;
    navTarget = NAV_NONE;
    shipSpeed = 0.0f;
    cruiseMode = false;
    shipX = stationWorld.x;
    shipY = stationWorld.y;
    shipZ = stationWorld.z - 80.0f;
    resetShipOrientation();
    updateZoneByPosition();
    clearBodies();
    applyDockServices(false);
  }
}

void updateSim(float dt) {
  if (docked) return;

  applySoftImuAssist(dt);

  applyShipRotation(dt);
  float rotDamp = powf(0.93f, dt * 60.0f);
  shipYawRate *= rotDamp;
  shipPitchRate *= rotDamp;
  shipRollRate *= rotDamp;

  Vec3 right, up, forward;
  getShipBasis(right, up, forward);
  float cruise = shipSpeed * 420.0f;
  if (cruiseMode && shipSpeed > 0.0f) cruise *= 1.68f;
  if (powerMode == 3) cruise *= 1.16f;
  if (fuelPct <= 0.0f) cruise *= 0.18f;
  shipX += forward.x * cruise * dt;
  shipY += forward.y * cruise * dt;
  shipZ += forward.z * cruise * dt;
  updateZoneByPosition();
  if (pirateDenDistance < 1150.0f) pirateDenKnown = true;

  updateDocking();

  float engineRegen = (powerMode == 1) ? 8.0f : 5.0f;
  float heatCool = (powerMode == 2) ? 8.0f : 12.0f;
  energyPct = clampf(energyPct + engineRegen * dt, 0.0f, 100.0f);
  if (millis() >= shieldOfflineUntilMs) {
    shieldPct = clampf(shieldPct + ((powerMode == 1) ? 5.0f : 2.0f) * dt, 0.0f, 100.0f);
  } else {
    shieldPct = 0.0f;
  }
  float thrustLoad = fabsf(shipSpeed);
  fuelPct = clampf(fuelPct - (0.012f + thrustLoad * (cruiseMode ? 0.065f : 0.038f)) * dt, 0.0f, 100.0f);
  if (fuelPct <= 0.0f) {
    if (fabsf(shipSpeed) > 0.045f) shipSpeed = (shipSpeed < 0.0f) ? -0.045f : 0.045f;
    cruiseMode = false;
    if (!dockingRun) statusLine = "Toplivo na nule";
  }
  laserHeat = clampf(laserHeat - heatCool * dt, 0.0f, 100.0f);
  reactorLoad = clampf(0.22f + thrustLoad * (cruiseMode ? 1.55f : 1.15f) + laserHeat * 0.004f, 0.0f, 1.0f);

  updateBodies(dt);

  if (hullPct <= 0.0f || energyPct <= 0.0f) {
    docked = true;
    dockingRun = false;
    viewMode = VIEW_DOCKED;
    zone = ZONE_STATION;
    navTarget = NAV_NONE;
    shipX = stationWorld.x;
    shipY = stationWorld.y;
    shipZ = stationWorld.z - 80.0f;
    updateZoneByPosition();
    shipSpeed = 0.0f;
    cruiseMode = false;
    resetShipOrientation();
    clearBodies();
    applyDockServices(true);
  }
}

void drawBar(int x, int y, int w, int h, float pct, uint16_t color, const char *label) {
  pct = clampf(pct, 0.0f, 100.0f);
  canvas.drawRect(x, y, w, h, dim565(TFT_WHITE, 0.32f));
  canvas.fillRect(x + 1, y + 1, (int)((w - 2) * pct / 100.0f), h - 2, color);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(x + w + 3, y - 1);
  canvas.printf("%s%02d", label, (int)pct);
}

void drawStars() {
  for (uint8_t i = 0; i < STAR_COUNT; ++i) {
    float d = dist3(shipX, shipY, shipZ, stars[i].x, stars[i].y, stars[i].z);
    if (d > 2100.0f || d < 80.0f) {
      stars[i].x = shipX + frandRange(-1600, 1600);
      stars[i].y = shipY + frandRange(-900, 900);
      stars[i].z = shipZ + frandRange(-1600, 1600);
    }
    int16_t sx, sy;
    float sc;
    if (!projectPoint(stars[i].x, stars[i].y, stars[i].z, sx, sy, sc)) continue;
    uint16_t c = dim565(TFT_WHITE, clampf(0.16f + sc * 4.2f, 0.15f, 0.85f));
    canvas.drawPixel(sx, sy, c);
  }
}

void drawStation() {
  int16_t sx, sy;
  float sc;
  if (!projectPoint(stationWorld.x, stationWorld.y, stationWorld.z, sx, sy, sc)) return;
  int r = clampi((int)(95.0f * sc), 5, 34);
  uint16_t c = dockingRun ? TFT_CYAN : dim565(TFT_CYAN, 0.75f);
  canvas.drawRect(sx - r, sy - r / 2, r * 2, r, c);
  canvas.drawRect(sx - r / 2, sy - r / 4, r, r / 2, dim565(TFT_WHITE, 0.7f));
  canvas.drawLine(sx - r, sy, sx - r - 8, sy - 4, dim565(TFT_CYAN, 0.5f));
  canvas.drawLine(sx + r, sy, sx + r + 8, sy + 4, dim565(TFT_CYAN, 0.5f));
}

void drawNavMarker() {
  if (navTarget == NAV_NONE) return;
  Vec3 target = stationWorld;
  uint16_t c = TFT_CYAN;
  if (navTarget == NAV_BELT) {
    target = beltWorld;
    c = TFT_ORANGE;
  } else if (navTarget == NAV_PIRATE_DEN) {
    target = pirateDenWorld;
    c = TFT_RED;
  }
  int16_t sx, sy;
  float sc;
  if (projectPoint(target.x, target.y, target.z, sx, sy, sc)) {
    canvas.drawCircle(sx, sy, 9, c);
    canvas.drawLine(sx - 13, sy, sx - 5, sy, c);
    canvas.drawLine(sx + 5, sy, sx + 13, sy, c);
    canvas.drawLine(sx, sy - 13, sx, sy - 5, c);
    canvas.drawLine(sx, sy + 5, sx, sy + 13, c);
    return;
  }

  Vec3 right, up, forward;
  getCameraBasis(right, up, forward);
  Vec3 rel = {target.x - shipX, target.y - shipY, target.z - shipZ};
  float side = dot3(rel, right);
  float high = dot3(rel, up);
  int ax = SCREEN_W / 2 + (side > 0 ? 96 : -96);
  int ay = SCREEN_H / 2 - clampi((int)(high * 0.05f), -42, 42);
  canvas.drawTriangle(ax, ay, ax + (side > 0 ? -8 : 8), ay - 5, ax + (side > 0 ? -8 : 8), ay + 5, c);
}

void drawPirateDen() {
  if (!pirateDenKnown && pirateDenDistance > 1800.0f && navTarget != NAV_PIRATE_DEN) return;
  int16_t sx, sy;
  float sc;
  if (!projectPoint(pirateDenWorld.x, pirateDenWorld.y, pirateDenWorld.z, sx, sy, sc)) return;
  int r = clampi((int)(58.0f * sc), 5, 18);
  canvas.drawCircle(sx, sy, r + 3, dim565(TFT_RED, 0.45f));
  canvas.drawCircle(sx, sy, r, TFT_RED);
  canvas.drawLine(sx - r, sy, sx + r, sy, dim565(TFT_RED, 0.65f));
  canvas.drawLine(sx, sy - r, sx, sy + r, dim565(TFT_RED, 0.65f));
  canvas.fillCircle(sx, sy, 2, TFT_BLACK);
}

void drawBody(const Body &b, bool rock) {
  if (!b.alive) return;
  int16_t sx, sy;
  float sc;
  if (!projectPoint(b.x, b.y, b.z, sx, sy, sc)) return;
  int r = rock ? clampi((int)(34.0f * sc), 3, 15) : clampi((int)(28.0f * sc), 4, 12);
  uint16_t c = rock ? dim565(TFT_ORANGE, 0.78f) : TFT_RED;
  if (rock) {
    canvas.drawCircle(sx, sy, r, c);
    canvas.drawPixel(sx - r / 2, sy + 1, dim565(TFT_WHITE, 0.5f));
  } else {
    canvas.drawTriangle(sx, sy - r, sx - r, sy + r, sx + r, sy + r, c);
    canvas.drawLine(sx - r, sy + r, sx + r, sy + r, dim565(TFT_WHITE, 0.45f));
  }
}

void drawTrafficBody(const Body &b) {
  if (!b.alive) return;
  int16_t sx, sy;
  float sc;
  if (!projectPoint(b.x, b.y, b.z, sx, sy, sc)) return;
  int r = clampi((int)(22.0f * sc), 3, 9);
  uint16_t c = dim565(TFT_CYAN, 0.78f);
  canvas.drawTriangle(sx, sy - r, sx - r, sy + r, sx + r, sy + r, c);
  canvas.drawPixel(sx, sy, TFT_WHITE);
}

void drawMinerBody(const Body &b) {
  if (!b.alive) return;
  int16_t sx, sy;
  float sc;
  if (!projectPoint(b.x, b.y, b.z, sx, sy, sc)) return;
  int r = clampi((int)(24.0f * sc), 3, 10);
  uint16_t c = dim565(TFT_GREEN, 0.78f);
  canvas.drawRect(sx - r, sy - r / 2, r * 2, r, c);
  canvas.drawLine(sx - r - 3, sy, sx - r, sy, dim565(TFT_YELLOW, 0.75f));
  canvas.drawLine(sx + r, sy, sx + r + 3, sy, dim565(TFT_YELLOW, 0.75f));
  canvas.drawPixel(sx, sy, TFT_WHITE);
}

void drawCrosshair() {
  int cx = SCREEN_W / 2;
  int cy = SCREEN_H / 2;
  uint16_t c = laserFlash ? TFT_RED : TFT_GREEN;
  canvas.drawLine(cx - 10, cy, cx - 3, cy, c);
  canvas.drawLine(cx + 3, cy, cx + 10, cy, c);
  canvas.drawLine(cx, cy - 10, cx, cy - 3, c);
  canvas.drawLine(cx, cy + 3, cx, cy + 10, c);
  canvas.drawCircle(cx, cy, 13, dim565(c, 0.35f));
}

void drawLaserBeams() {
  if (!laserFlash) return;

  int ex = clampi(laserBeamX, 14, SCREEN_W - 14);
  int ey = clampi(laserBeamY, 12, SCREEN_H - 18);
  int leftX = 74;
  int rightX = SCREEN_W - 74;
  int gunY = SCREEN_H - 10;
  uint16_t outer = laserBeamHit ? TFT_ORANGE : dim565(TFT_RED, 0.72f);
  uint16_t core = laserBeamHit ? TFT_WHITE : dim565(TFT_CYAN, 0.92f);
  uint16_t glow = laserBeamHit ? dim565(TFT_YELLOW, 0.45f) : dim565(TFT_BLUE, 0.38f);

  canvas.drawLine(leftX - 1, gunY, ex - 2, ey, glow);
  canvas.drawLine(rightX + 1, gunY, ex + 2, ey, glow);
  canvas.drawLine(leftX, gunY, ex, ey, outer);
  canvas.drawLine(rightX, gunY, ex, ey, outer);
  canvas.drawLine(leftX + 1, gunY - 1, ex, ey, core);
  canvas.drawLine(rightX - 1, gunY - 1, ex, ey, core);
  canvas.fillCircle(leftX, gunY, 2, dim565(TFT_CYAN, 0.62f));
  canvas.fillCircle(rightX, gunY, 2, dim565(TFT_CYAN, 0.62f));
  if (laserBeamHit) {
    canvas.drawCircle(ex, ey, 4, TFT_YELLOW);
    canvas.drawPixel(ex, ey, TFT_WHITE);
  }
  laserFlash--;
}

void drawScannerContact(float wx, float wy, float wz, uint16_t color, bool threat) {
  const int x = 78;
  const int y = 91;
  const int w = 70;
  const int h = 30;
  const int cx = x + w / 2;
  const int cy = y + h / 2 + 2;

  Vec3 right, up, forward;
  getShipBasis(right, up, forward);
  Vec3 rel = {wx - shipX, wy - shipY, wz - shipZ};
  float side = dot3(rel, right);
  float ahead = dot3(rel, forward);
  float high = dot3(rel, up);
  float d = sqrtf(side * side + ahead * ahead + high * high);
  if (d > 1800.0f) return;

  int px = clampi((int)(cx + side * 0.050f), x + 3, x + w - 4);
  int py = clampi((int)(cy - ahead * 0.030f), y + 4, y + h - 4);
  int pz = clampi((int)(py - high * 0.040f), y + 3, y + h - 3);
  uint16_t dim = dim565(color, threat ? 0.72f : 0.48f);
  canvas.drawLine(px, py, px, pz, dim);
  if (threat) {
    canvas.fillTriangle(px, pz - 3, px - 3, pz + 3, px + 3, pz + 3, color);
  } else {
    canvas.fillCircle(px, pz, 2, color);
  }
}

void drawScanner() {
  const int x = 78;
  const int y = 91;
  const int w = 70;
  const int h = 30;
  const int cx = x + w / 2;
  const int cy = y + h / 2 + 2;
  uint16_t grid = dim565(TFT_CYAN, 0.34f);

  canvas.drawRect(x, y, w, h, dim565(TFT_WHITE, 0.20f));
  canvas.drawLine(x + 4, cy, x + w - 5, cy, grid);
  canvas.drawLine(cx, y + 4, cx, y + h - 4, grid);
  canvas.drawLine(x + 9, y + 5, x + w - 10, y + 5, dim565(TFT_CYAN, 0.20f));
  canvas.drawLine(x + 5, y + h - 5, x + w - 6, y + h - 5, dim565(TFT_CYAN, 0.20f));
  canvas.drawPixel(cx, cy, TFT_GREEN);

  if (stationDistance < 1800.0f) {
    drawScannerContact(stationWorld.x, stationWorld.y, stationWorld.z, TFT_CYAN, false);
  }
  if ((pirateDenKnown || navTarget == NAV_PIRATE_DEN || pirateDenDistance < 1800.0f) && pirateDenDistance < 2200.0f) {
    drawScannerContact(pirateDenWorld.x, pirateDenWorld.y, pirateDenWorld.z, TFT_RED, true);
  }
  for (uint8_t i = 0; i < ROCK_COUNT; ++i) {
    if (rocks[i].alive) drawScannerContact(rocks[i].x, rocks[i].y, rocks[i].z, TFT_ORANGE, false);
  }
  for (uint8_t i = 0; i < MINER_COUNT; ++i) {
    if (miners[i].alive) drawScannerContact(miners[i].x, miners[i].y, miners[i].z, TFT_GREEN, false);
  }
  for (uint8_t i = 0; i < TRAFFIC_COUNT; ++i) {
    if (traffic[i].alive) drawScannerContact(traffic[i].x, traffic[i].y, traffic[i].z, TFT_GREEN, false);
  }
  for (uint8_t i = 0; i < ENEMY_COUNT; ++i) {
    if (enemies[i].alive) drawScannerContact(enemies[i].x, enemies[i].y, enemies[i].z, TFT_RED, true);
  }

  canvas.setTextColor(dim565(TFT_CYAN, 0.72f), TFT_BLACK);
  canvas.setCursor(x + 2, y - 8);
  canvas.print("SCAN");
}

void drawMiniBar(int x, int y, float pct, uint16_t color, const char *label) {
  pct = clampf(pct, 0.0f, 100.0f);
  canvas.setTextColor(dim565(TFT_WHITE, 0.72f), TFT_BLACK);
  canvas.setCursor(x, y - 1);
  canvas.print(label);
  canvas.drawRect(x + 10, y, 34, 5, dim565(TFT_WHITE, 0.22f));
  canvas.fillRect(x + 11, y + 1, (int)(32.0f * pct / 100.0f), 3, color);
}

void drawThrottleScale() {
  const int x = 224;
  const int y = 17;
  const int h = 89;
  const int mid = y + 58;
  const int w = 9;
  uint16_t frame = dim565(TFT_WHITE, 0.34f);
  canvas.drawRect(x, y, w, h, frame);
  canvas.drawLine(x - 3, mid, x + w + 2, mid, TFT_WHITE);
  canvas.drawLine(x - 2, y + 8, x + w + 1, y + 8, dim565(TFT_CYAN, 0.55f));
  canvas.drawLine(x - 2, y + h - 9, x + w + 1, y + h - 9, dim565(TFT_ORANGE, 0.55f));

  if (shipSpeed >= 0.0f) {
    int fill = (int)((mid - y - 2) * throttlePercent() / 100.0f);
    canvas.fillRect(x + 2, mid - fill, w - 3, fill, cruiseMode ? TFT_GREEN : TFT_CYAN);
  } else {
    int fill = (int)((y + h - mid - 2) * throttleAbsPercent() / 100.0f);
    canvas.fillRect(x + 2, mid + 1, w - 3, fill, TFT_ORANGE);
  }

  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(x - 12, y - 10);
  canvas.print("THR");
  canvas.setCursor(x - 16, y + h + 2);
  canvas.printf("%+03d", (int)throttlePercent());
}

void drawFlight() {
  canvas.fillScreen(TFT_BLACK);
  drawStars();
  drawStation();
  drawPirateDen();
  drawNavMarker();
  for (uint8_t i = 0; i < ROCK_COUNT; ++i) drawBody(rocks[i], true);
  for (uint8_t i = 0; i < MINER_COUNT; ++i) drawMinerBody(miners[i]);
  for (uint8_t i = 0; i < TRAFFIC_COUNT; ++i) drawTrafficBody(traffic[i]);
  for (uint8_t i = 0; i < ENEMY_COUNT; ++i) drawBody(enemies[i], false);
  drawCrosshair();

  drawLaserBeams();
  if (alertFlash) {
    canvas.drawRect(0, 0, SCREEN_W, SCREEN_H, TFT_RED);
    alertFlash--;
  }

  canvas.setTextSize(1);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.setCursor(3, 2);
  canvas.print("ELITE ZERO v0.18");
  canvas.setCursor(149, 2);
  canvas.printf("%s %s", cockpitViewName(), cruiseMode ? "CRU" : "MAN");

  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.setCursor(3, 13);
  if (navTarget == NAV_BELT) canvas.printf("BELT %.0fm", navDistance);
  else if (navTarget == NAV_STATION) canvas.printf("ST %.0fm", navDistance);
  else if (navTarget == NAV_PIRATE_DEN) canvas.printf("LOGOVO %.0fm", navDistance);
  else canvas.printf("ST %.0fm", stationDistance);

  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(3, 24);
  canvas.printf("G%u %s  SPD %.2f", (unsigned)currentGalaxy + 1U, eliteCurrentSystem().name, shipSpeed);
  canvas.setCursor(3, 34);
  canvas.printf("R %.1fV ORE %u/%u", 10.8f + reactorLoad * 2.4f, ore, cargoMax);

  drawMiniBar(3, 48, shieldPct, TFT_BLUE, "S");
  drawMiniBar(3, 58, energyPct, TFT_GREEN, "E");
  drawMiniBar(3, 68, hullPct, TFT_YELLOW, "C");
  drawMiniBar(3, 78, fuelPct, TFT_CYAN, "F");
  drawMiniBar(3, 88, laserHeat, TFT_RED, "H");

  drawScanner();
  drawThrottleScale();

  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(3, SCREEN_H - 10);
  canvas.print(statusLine.substring(0, 38));
}

void drawDocked() {
  canvas.fillScreen(TFT_BLACK);
  canvas.drawRect(2, 2, SCREEN_W - 4, SCREEN_H - 4, TFT_CYAN);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.setCursor(6, 6);
  canvas.printf("G%u/%03u %s", (unsigned)currentGalaxy + 1U, (unsigned)currentSystem, eliteCurrentSystem().name);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(150, 6);
  canvas.printf("%lucr", (unsigned long)credits);

  const int tabY = 20;
  const int tabW = 58;
  for (uint8_t i = 0; i < ST_TAB_COUNT; ++i) {
    int x = 4 + i * tabW;
    bool selected = ((StationTab)i == stationTab);
    uint16_t frame = selected ? TFT_CYAN : dim565(TFT_WHITE, 0.32f);
    if (selected) canvas.fillRect(x, tabY, tabW - 3, 12, dim565(TFT_CYAN, 0.42f));
    canvas.drawRect(x, tabY, tabW - 3, 12, frame);
    canvas.setTextColor(selected ? TFT_BLACK : dim565(TFT_WHITE, 0.82f), selected ? dim565(TFT_CYAN, 0.42f) : TFT_BLACK);
    canvas.setCursor(x + 4, tabY + 3);
    canvas.print(stationTabName((StationTab)i));
  }

  uint8_t rows = stationRowCount(stationTab);
  for (uint8_t i = 0; i < rows; ++i) {
    int y = 40 + i * 14;
    bool selected = (i == stationRow);
    if (selected) canvas.fillRect(7, y - 2, SCREEN_W - 14, 12, dim565(TFT_YELLOW, 0.50f));
    canvas.setTextColor(selected ? TFT_BLACK : TFT_WHITE, selected ? dim565(TFT_YELLOW, 0.50f) : TFT_BLACK);
    canvas.setCursor(12, y);
    canvas.print(selected ? ">" : " ");
    canvas.setCursor(23, y);
    canvas.print(stationRowName(stationTab, i));
  }

  canvas.setTextColor(dim565(TFT_CYAN, 0.85f), TFT_BLACK);
  canvas.setCursor(8, 94);
  canvas.printf("Ore %u/%u Fuel %.0f Hull %.0f", ore, cargoMax, fuelPct, hullPct);
  canvas.setCursor(8, 106);
  canvas.printf("%s %s TL%u D%u", eliteEconomyName(eliteCurrentSystem().economy),
                eliteGovernmentName(eliteCurrentSystem().government),
                (unsigned)eliteCurrentSystem().techLevel, (unsigned)eliteCurrentSystem().danger);
  canvas.setCursor(8, 119);
  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.print(statusLine.substring(0, 36));
}

void drawMap() {
  canvas.fillScreen(TFT_BLACK);
  const int mapX = 4;
  const int mapY = 16;
  const int mapW = 170;
  const int mapH = 96;
  const int infoX = 181;

  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.setCursor(4, 4);
  canvas.printf("ELITE GALAXY G%u", (unsigned)currentGalaxy + 1U);

  canvas.drawRect(mapX - 1, mapY - 1, mapW + 2, mapH + 2, dim565(TFT_CYAN, 0.42f));
  uint16_t grid = dim565(TFT_WHITE, 0.12f);
  for (int gx = 1; gx < 4; ++gx) canvas.drawLine(mapX + gx * mapW / 4, mapY, mapX + gx * mapW / 4, mapY + mapH, grid);
  for (int gy = 1; gy < 3; ++gy) canvas.drawLine(mapX, mapY + gy * mapH / 3, mapX + mapW, mapY + gy * mapH / 3, grid);

  for (uint16_t i = 0; i < GALAXY_SYSTEM_COUNT; ++i) {
    const EliteSystem &s = galaxySystems[i];
    int px = mapX + (int)s.x * (mapW - 1) / 255;
    int py = mapY + (int)s.y * (mapH - 1) / 255;
    uint16_t c = s.danger >= 7 ? dim565(TFT_RED, 0.55f) :
                 (s.techLevel >= 11 ? dim565(TFT_CYAN, 0.66f) : dim565(TFT_WHITE, 0.48f));
    canvas.drawPixel(px, py, c);
  }

  uint16_t rangePx = (uint16_t)clampi((int)(eliteJumpRangeNow() * mapW / 255), 4, 42);
  const EliteSystem &cur = eliteCurrentSystem();
  int curX = mapX + (int)cur.x * (mapW - 1) / 255;
  int curY = mapY + (int)cur.y * (mapH - 1) / 255;
  canvas.drawCircle(curX, curY, rangePx, dim565(TFT_GREEN, 0.24f));

  for (uint8_t i = 0; i < cosmosCacheCount; ++i) {
    const CosmosLandmark &lm = cosmosCache[i];
    if ((lm.galaxy & 7) != currentGalaxy) continue;
    int px = mapX + (int)lm.x * (mapW - 1) / 255;
    int py = mapY + (int)lm.y * (mapH - 1) / 255;
    uint16_t c = TFT_YELLOW;
    if (lm.type == COSMOS_BLACK_HOLE || lm.type == COSMOS_LAB) c = TFT_MAGENTA;
    else if (lm.type == COSMOS_PULSAR) c = TFT_CYAN;
    else if (lm.type == COSMOS_NEBULA) c = TFT_ORANGE;
    else if (lm.type == COSMOS_GALAXY) c = TFT_BLUE;
    canvas.drawLine(px - 2, py, px + 2, py, c);
    canvas.drawLine(px, py - 2, px, py + 2, c);
  }

  int tgtX = mapX + (int)eliteTargetSystem().x * (mapW - 1) / 255;
  int tgtY = mapY + (int)eliteTargetSystem().y * (mapH - 1) / 255;
  canvas.drawLine(curX, curY, tgtX, tgtY, eliteTargetReachable(targetSystem) ? dim565(TFT_GREEN, 0.58f) : dim565(TFT_RED, 0.48f));
  canvas.drawRect(tgtX - 3, tgtY - 3, 7, 7, TFT_YELLOW);
  canvas.fillCircle(curX, curY, 3, TFT_GREEN);

  int csrX = mapX + (int)eliteCursorSystem().x * (mapW - 1) / 255;
  int csrY = mapY + (int)eliteCursorSystem().y * (mapH - 1) / 255;
  canvas.drawCircle(csrX, csrY, 5, TFT_WHITE);

  const EliteSystem &t = eliteCursorSystem();
  const CosmosLandmark *lm = nearestCosmosLandmark(galaxyCursorSystem);
  uint16_t dist = eliteDistanceSystems(currentSystem, galaxyCursorSystem);
  bool reachable = eliteTargetReachable(galaxyCursorSystem);
  char shortName[10];
  strlcpy(shortName, t.name, sizeof(shortName));
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(infoX, 17);
  canvas.printf("%03u", (unsigned)galaxyCursorSystem);
  canvas.setCursor(infoX, 28);
  canvas.print(shortName);
  canvas.setCursor(infoX, 40);
  canvas.printf("D%u TL%u", (unsigned)t.danger, (unsigned)t.techLevel);
  canvas.setCursor(infoX, 52);
  canvas.print(eliteEconomyName(t.economy));
  canvas.setCursor(infoX, 64);
  canvas.print(eliteGovernmentName(t.government));
  canvas.setCursor(infoX, 76);
  canvas.setTextColor(reachable ? TFT_GREEN : TFT_RED, TFT_BLACK);
  canvas.printf("%u/%u", (unsigned)dist, (unsigned)eliteJumpRangeNow());
  canvas.setCursor(infoX, 88);
  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.printf("F%u", (unsigned)eliteJumpFuelCost(galaxyCursorSystem));
  canvas.setCursor(infoX, 100);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.printf("K%lu", (unsigned long)knownCosmosCount);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(4, 116);
  if (lm) {
    canvas.printf("%s %s sci%u", cosmosTypeName(lm->type), lm->name, (unsigned)lm->science);
  } else {
    canvas.printf("Known cosmos overlay: %lu bright", (unsigned long)knownCosmosBrightCount);
  }
}

void drawStatus() {
  canvas.fillScreen(TFT_BLACK);
  canvas.setTextColor(TFT_CYAN, TFT_BLACK);
  canvas.setCursor(4, 4);
  canvas.printf("SHIP STATUS G%u/%03u", (unsigned)currentGalaxy + 1U, (unsigned)currentSystem);
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(4, 20);
  canvas.printf("System: %s", eliteCurrentSystem().name);
  canvas.setCursor(4, 32);
  canvas.printf("%s %s TL%u D%u", eliteEconomyName(eliteCurrentSystem().economy),
                eliteGovernmentName(eliteCurrentSystem().government),
                (unsigned)eliteCurrentSystem().techLevel, (unsigned)eliteCurrentSystem().danger);
  canvas.setCursor(4, 44);
  canvas.printf("Shield %.0f Hull %.0f Energy %.0f", shieldPct, hullPct, energyPct);
  canvas.setCursor(4, 56);
  canvas.printf("Fuel %.0f Laser heat %.0f", fuelPct, laserHeat);
  canvas.setCursor(4, 68);
  canvas.printf("Reactor %.1fV load %.0f%%", 10.8f + reactorLoad * 2.4f, reactorLoad * 100.0f);
  canvas.setCursor(4, 80);
  canvas.printf("Target: %03u %s", (unsigned)targetSystem, eliteTargetSystem().name);
  canvas.setCursor(4, 92);
  canvas.printf("Jump %u/%u FuelCost %u", (unsigned)eliteDistanceSystems(currentSystem, targetSystem),
                (unsigned)eliteJumpRangeNow(), (unsigned)eliteJumpFuelCost(targetSystem));
  canvas.setCursor(4, 104);
  canvas.printf("Known %lu Bright %lu CAP %s", (unsigned long)knownCosmosCount,
                (unsigned long)knownCosmosBrightCount, advCapGnssFreshNow(millis()) ? "GNSS" : "LOCAL");
  canvas.setCursor(4, 116);
  canvas.printf("Rep %+d Kills %u LED %s", factionRep[(uint8_t)stationFaction], kills, shipLedEnabled ? "ON" : "OFF");
}

void drawMinerTabStrip(uint16_t primary, uint16_t secondary, uint16_t activeBg) {
  const int tabY = 5;
  const int tabW = 52;
  for (uint8_t i = 0; i < MINER_TAB_COUNT; ++i) {
    int x = 6 + i * (tabW + 4);
    bool selected = (minerTab == (MinerTab)i);
    if (selected) canvas.fillRect(x, tabY, tabW, 11, activeBg);
    canvas.drawRect(x, tabY, tabW, 11, selected ? primary : secondary);
    canvas.setTextColor(selected ? TFT_BLACK : secondary, selected ? activeBg : TFT_BLACK);
    canvas.setCursor(x + 6, tabY + 3);
    canvas.print(minerTabName((MinerTab)i));
  }
}

void drawBeaconLegacyStatus() {
  canvas.fillScreen(TFT_BLACK);
  const uint16_t amber = 0xFDA0;
  const uint16_t amberDim = dim565(amber, 0.46f);
  const uint16_t amberSoft = dim565(amber, 0.72f);
  const uint16_t secondary = dim565(TFT_WHITE, 0.60f);
  uint32_t now = millis();
  uint32_t masterAge = buzzLastMasterMs ? now - buzzLastMasterMs : 999999UL;
  bool masterOn = buzzLastMasterMs && masterAge < 9000UL;
  int bat = M5.Power.getBatteryLevel();
  if (bat < 0 || bat > 100) bat = 0;

  float entropy = beaconLegacyEntropy();
  float loss = beaconLegacyLoss();
  float fit = beaconLegacyFit();
  float m2r = beaconLegacyM2R();
  float mi = clampf(((float)buzzBestBits / max(1.0f, (float)buzzTargetBits)) * 1.6f, 0.0f, 9.99f);
  uint8_t theta = beaconLegacyTheta();
  uint8_t aiNodes = beaconLegacyAiNodes();
  bool shtFresh = envShtFreshNow(now);
  bool qmpFresh = envQmpFreshNow(now);

  canvas.setTextSize(1);
  canvas.drawFastHLine(0, 18, SCREEN_W, amberDim);
  drawMinerTabStrip(amber, amberDim, amber);

  canvas.setTextColor(amber, TFT_BLACK);
  canvas.setCursor(2, 20);
  canvas.print("JANUS BEACON v4.5 LEGACY");
  canvas.setTextColor(secondary, TFT_BLACK);
  canvas.setCursor(136, 2);
  canvas.printf("V:%03u", (unsigned)gameVolume);
  canvas.setCursor(176, 2);
  canvas.printf("BAT:%02d", bat);

  canvas.setCursor(102, 20);
  canvas.printf("AI:%02u", (unsigned)aiNodes);
  canvas.setCursor(130, 20);
  canvas.printf("TH:%03u", (unsigned)theta);
  canvas.setCursor(172, 20);
  canvas.printf("%s %s", buzzNightMode ? "NIGHT" : "ACTIVE", buzzMinerEnabled ? "ON" : "OFF");

  canvas.setTextColor(amberSoft, TFT_BLACK);
  canvas.setCursor(2, 34);
  if (shtFresh) canvas.printf("T %.1f>%.1f", envTempC, envPredTempC);
  else canvas.print("T -- > --");
  canvas.setCursor(2, 46);
  if (shtFresh) canvas.printf("H %.0f>%.0f", envHumidity, envPredHumidity);
  else canvas.print("H -- > --");
  canvas.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  canvas.setCursor(2, 58);
  if (qmpFresh) canvas.printf("P %.1f", envPressureHpa);
  else canvas.print("P --");

  canvas.setTextColor(TFT_YELLOW, TFT_BLACK);
  canvas.setCursor(2, 70);  canvas.printf("E %.2f", entropy);
  canvas.setCursor(2, 82);  canvas.printf("M2R %.2f", m2r);
  canvas.setTextColor(TFT_ORANGE, TFT_BLACK);
  canvas.setCursor(2, 94);  canvas.printf("LOSS %.2f", loss);
  canvas.setCursor(2, 106); canvas.printf("MI %.2f", mi);

  canvas.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  canvas.setCursor(2, 118);
  canvas.printf("IMU %s", imuReady ? "ON" : "--");

  canvas.setTextColor(amber, TFT_BLACK);
  canvas.setCursor(84, 34);  canvas.printf("F1 %.1f", 432.0f + entropy);
  canvas.setCursor(84, 46);  canvas.printf("F2 %.1f", 439.8f + fit);
  canvas.setTextColor(TFT_MAGENTA, TFT_BLACK);
  canvas.setCursor(84, 58);  canvas.printf("PF1 %.1f", 432.0f + entropy + loss * 0.7f);
  canvas.setCursor(84, 70);  canvas.printf("PF2 %.1f", 439.8f + fit + m2r * 0.2f);
  canvas.setTextColor(TFT_GREEN, TFT_BLACK);
  canvas.setCursor(84, 82);  canvas.printf("FIT %.2f", fit);
  canvas.setCursor(84, 94);  canvas.printf("BEST %lu/%u", (unsigned long)buzzBestBits, (unsigned)buzzTargetBits);
  canvas.setTextColor(secondary, TFT_BLACK);
  canvas.setCursor(84, 106); canvas.printf("C2:-- M:%s", masterOn ? "ON" : "--");
  canvas.setCursor(84, 118); canvas.printf("FUT %.1f/%.1f", entropy + fit * 0.1f, entropy + m2r * 0.05f);

  canvas.setTextColor(masterOn ? TFT_CYAN : secondary, TFT_BLACK);
  canvas.setCursor(162, 34);  canvas.printf("BUZZ %s", masterOn ? "ON" : "--");
  canvas.setCursor(162, 46);  canvas.printf("H %lu", (unsigned long)buzzHashRate);
  canvas.setCursor(162, 58);  canvas.printf("EMA %.0f", buzzHashRateEma);
  canvas.setCursor(162, 70);  canvas.printf("SH %lu", (unsigned long)buzzShares);
  canvas.setCursor(162, 82);  canvas.printf("JOB %lu/%lu", (unsigned long)buzzJobsSeen, (unsigned long)buzzJobsDone);
  canvas.setCursor(162, 94);  canvas.printf("RX %lu", (unsigned long)buzzRxSeen);
  canvas.setCursor(162, 106); canvas.printf("TX %lu/%lu", (unsigned long)buzzTxOk, (unsigned long)buzzTxFail);
  canvas.setCursor(162, 118); canvas.printf("HEAP %luK", (unsigned long)(ESP.getFreeHeap() / 1024UL));

  canvas.drawRect(0, 132, SCREEN_W, 3, amberDim);
  int fill = clampi((int)(SCREEN_W * entropy / 10.0f), 0, SCREEN_W);
  canvas.fillRect(0, 132, fill, 3, amber);
  canvas.fillRect(0, 122, SCREEN_W, 10, TFT_BLACK);
  canvas.drawFastHLine(0, 121, SCREEN_W, amberDim);
  canvas.setTextColor(amber, TFT_BLACK);
  canvas.setCursor(2, 124);
  canvas.print(">");
  canvas.print(beaconRowName(beaconMenuRow));
  canvas.print(" ");
  canvas.print(beaconRowState(beaconMenuRow));
  canvas.setTextColor(secondary, TFT_BLACK);
  canvas.setCursor(132, 124);
  canvas.print("ENT E/S A/D");
}

void drawMinerStatus() {
  if (minerTab == MINER_TAB_BEACON) {
    drawBeaconLegacyStatus();
    return;
  }

  canvas.fillScreen(TFT_BLACK);
  uint32_t now = millis();
  uint32_t masterAge = buzzLastMasterMs ? now - buzzLastMasterMs : 999999UL;
  uint32_t jobAge = buzzJob.active ? now - buzzJob.receivedAt : 0UL;
  uint32_t left = (buzzJob.active && buzzJob.rangeSize > buzzJob.hashesDone) ? (buzzJob.rangeSize - buzzJob.hashesDone) : 0UL;
  bool jackpot = now < buzzShareFlashUntilMs;

  uint16_t frame = jackpot ? TFT_YELLOW : (buzzJob.active ? TFT_GREEN : dim565(TFT_CYAN, 0.70f));
  uint16_t primary = jackpot ? TFT_YELLOW : TFT_CYAN;
  canvas.drawRect(2, 2, SCREEN_W - 4, SCREEN_H - 4, frame);
  canvas.setTextColor(primary, TFT_BLACK);
  canvas.setCursor(5, 5);
  canvas.print(jackpot ? "RBLGANUL A9 SHARE" : "RBLGANUL A9 MINER");
  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(160, 5);
  canvas.printf("CH%u", (unsigned)BUZZ_ESPNOW_CHANNEL);
  canvas.setCursor(196, 5);
  canvas.print(">BEACON");

  canvas.setTextColor(TFT_WHITE, TFT_BLACK);
  canvas.setCursor(6, 20);
  canvas.printf("Radio:%s Buzz:%lus", buzzEspNowReady ? "OK" : "NO", (unsigned long)(masterAge / 1000UL));
  canvas.setCursor(6, 32);
  canvas.printf("Mode:%s %s %s", buzzMinerEnabled ? "ON" : "OFF", buzzNightMode ? "NIGHT" : "ACTIVE",
                buzzLegacyA9Mode ? "LEGACY" : "BROAD");
  canvas.setCursor(6, 44);
  canvas.printf("Job:%s age:%lus", buzzJob.active ? "ACTIVE" : "WAIT", (unsigned long)(jobAge / 1000UL));
  canvas.setCursor(6, 56);
  canvas.printf("H/s:%lu ema:%.0f", (unsigned long)buzzHashRate, buzzHashRateEma);
  canvas.setCursor(6, 68);
  canvas.printf("Best:%lu target:%u", (unsigned long)buzzBestBits, (unsigned)buzzTargetBits);
  canvas.setCursor(6, 80);
  canvas.printf("Shares:%lu jobs:%lu/%lu/%lu", (unsigned long)buzzShares,
                (unsigned long)buzzJobsSeen, (unsigned long)buzzJobsDone, (unsigned long)buzzJobsExpired);
  canvas.setCursor(6, 92);
  canvas.printf("Chk:%lu left:%lu", (unsigned long)buzzJob.hashesDone, (unsigned long)left);
  canvas.setCursor(6, 104);
  canvas.printf("A9:%s s%u a%u", buzzStrategyName(buzzJob.minerStrategy),
                (unsigned)buzzJob.minerSector, (unsigned)buzzJob.minerStrideArm);
  canvas.setCursor(6, 116);
  canvas.printf("Near:%lu/%lu/%lu C:%lu", (unsigned long)buzzNear20, (unsigned long)buzzNear21,
                (unsigned long)buzzNear22, (unsigned long)buzzTargetCompareFails);

  canvas.setTextColor(dim565(TFT_CYAN, 0.75f), TFT_BLACK);
  canvas.setCursor(126, 32);
  canvas.printf("Bt%u D%lu", (unsigned)buzzActiveBatch(), (unsigned long)buzzJobsDeferred);
  canvas.setCursor(126, 44);
  canvas.printf("L:%s", buzzLaneName(buzzJob.minerLane));
  canvas.setCursor(126, 56);
  canvas.printf("Br%lu R%lu", (unsigned long)buzzA9BroadBest, (unsigned long)buzzA9RandomBest);
  canvas.setCursor(126, 68);
  canvas.printf("TX:%lu/%lu", (unsigned long)buzzTxOk, (unsigned long)buzzTxFail);
  canvas.setCursor(126, 80);
  canvas.printf("Cp%lu/%lu", (unsigned long)buzzCorpusBestBits, (unsigned long)buzzCorpusShares);
  canvas.setCursor(126, 92);
  canvas.printf("%s Sv%lu", buzzCorpusStoreName(), (unsigned long)buzzCorpusSaves);
  canvas.setCursor(126, 104);
  canvas.printf("Pr%lu %s", (unsigned long)buzzCorpusPrunes, buzzCorpusDirty ? "*" : "OK");

  int barW = 0;
  if (buzzJob.active && buzzJob.rangeSize > 0) {
    barW = (int)(220UL * buzzJob.hashesDone / buzzJob.rangeSize);
    barW = clampi(barW, 0, 220);
  }
  canvas.drawRect(10, 126, 220, 5, dim565(TFT_WHITE, 0.25f));
  canvas.fillRect(11, 127, max(0, barW - 2), 3, jackpot ? TFT_YELLOW : TFT_GREEN);
}

void renderFrame() {
  if (viewMode == VIEW_MINER) drawMinerStatus();
  else
  if (docked && viewMode == VIEW_MAP) drawMap();
  else if (docked && viewMode == VIEW_STATUS) drawStatus();
  else if (viewMode == VIEW_DOCKED || docked) drawDocked();
  else if (viewMode == VIEW_MAP) drawMap();
  else if (viewMode == VIEW_STATUS) drawStatus();
  else drawFlight();
  canvas.pushSprite(0, 0);
}

bool keyDown(char lo, char hi) {
  return M5Cardputer.Keyboard.isKeyPressed(lo) || M5Cardputer.Keyboard.isKeyPressed(hi);
}

void handleStationNav() {
  if (!docked || viewMode != VIEW_DOCKED) return;
  if (millis() - lastStationNavMs < KEY_STATION_NAV_MS) return;

  bool moved = false;
  if (keyDown('e', 'E') || keyDown('w', 'W') || keyDown(';', ':')) {
    stationMoveRow(-1);
    moved = true;
  } else if (keyDown('s', 'S') || keyDown('/', '?')) {
    stationMoveRow(1);
    moved = true;
  } else if (keyDown('a', 'A') || keyDown(',', '<')) {
    stationMoveTab(-1);
    moved = true;
  } else if (keyDown('d', 'D') || keyDown('.', '>')) {
    stationMoveTab(1);
    moved = true;
  }

  if (moved) lastStationNavMs = millis();
}

void galaxyMoveCursor(int8_t dx, int8_t dy) {
  const EliteSystem &cur = eliteCursorSystem();
  int best = -1;
  uint32_t bestScore = 0xFFFFFFFFUL;
  for (uint16_t i = 0; i < GALAXY_SYSTEM_COUNT; ++i) {
    if (i == galaxyCursorSystem) continue;
    const EliteSystem &s = galaxySystems[i];
    int vx = (int)s.x - (int)cur.x;
    int vy = (int)s.y - (int)cur.y;
    if (dx < 0 && vx >= 0) continue;
    if (dx > 0 && vx <= 0) continue;
    if (dy < 0 && vy >= 0) continue;
    if (dy > 0 && vy <= 0) continue;
    uint32_t dist = (uint32_t)(vx * vx + vy * vy);
    uint32_t bias = (uint32_t)(abs((dy ? vx : vy)) * 14);
    uint32_t score = dist + bias;
    if (score < bestScore) {
      bestScore = score;
      best = (int)i;
    }
  }
  if (best >= 0) galaxyCursorSystem = (uint8_t)best;
  else {
    int step = (dx < 0 || dy < 0) ? -1 : 1;
    int next = (int)galaxyCursorSystem + step;
    while (next < 0) next += GALAXY_SYSTEM_COUNT;
    while (next >= GALAXY_SYSTEM_COUNT) next -= GALAXY_SYSTEM_COUNT;
    galaxyCursorSystem = (uint8_t)next;
  }
  targetSystem = galaxyCursorSystem;
  const EliteSystem &t = eliteCursorSystem();
  statusLine = String("Route ") + t.name;
}

void handleGalaxyMapNav() {
  if (viewMode != VIEW_MAP) return;
  if (millis() - lastStationNavMs < KEY_STATION_NAV_MS) return;

  bool moved = false;
  if (keyDown('e', 'E') || keyDown('w', 'W') || keyDown(';', ':')) {
    galaxyMoveCursor(0, -1);
    moved = true;
  } else if (keyDown('s', 'S') || keyDown('/', '?')) {
    galaxyMoveCursor(0, 1);
    moved = true;
  } else if (keyDown('a', 'A') || keyDown(',', '<')) {
    galaxyMoveCursor(-1, 0);
    moved = true;
  } else if (keyDown('d', 'D') || keyDown('.', '>')) {
    galaxyMoveCursor(1, 0);
    moved = true;
  }

  if (moved) lastStationNavMs = millis();
}

void galaxyMapInteract() {
  targetSystem = galaxyCursorSystem;
  if (!docked) {
    statusLine = String("Route locked ") + eliteTargetSystem().name;
    return;
  }
  eliteExecuteHyperspace(galaxyCursorSystem);
}

bool closeCurrentPanel() {
  if (viewMode == VIEW_MINER || viewMode == VIEW_MAP || viewMode == VIEW_STATUS) {
    if (docked) {
      viewMode = VIEW_DOCKED;
      statusLine = "Dock menu";
    } else {
      viewMode = VIEW_FLIGHT;
      statusLine = "Cockpit";
    }
    return true;
  }

  if (!docked && viewMode != VIEW_FLIGHT) {
    viewMode = VIEW_FLIGHT;
    statusLine = "Cockpit";
    return true;
  }

  return false;
}

void handleMinerNav() {
  if (viewMode != VIEW_MINER) return;
  if (millis() - lastMinerNavMs < KEY_MINER_TAB_MS) return;

  bool moved = false;
  if (minerTab == MINER_TAB_BEACON) {
    if (keyDown('e', 'E') || keyDown('w', 'W') || keyDown(';', ':')) {
      beaconMoveRow(-1);
      moved = true;
    } else if (keyDown('s', 'S') || keyDown('/', '?')) {
      beaconMoveRow(1);
      moved = true;
    } else if (keyDown('a', 'A') || keyDown(',', '<')) {
      minerMoveTab(-1);
      moved = true;
    } else if (keyDown('d', 'D') || keyDown('.', '>')) {
      minerMoveTab(1);
      moved = true;
    }
  } else {
    if (keyDown('a', 'A') || keyDown(',', '<')) {
      minerMoveTab(-1);
      moved = true;
    } else if (keyDown('d', 'D') || keyDown('.', '>')) {
      minerMoveTab(1);
      moved = true;
    }
  }

  if (moved) lastMinerNavMs = millis();
}

void handleHeldKeys() {
  if (millis() - lastHeldMs < KEY_HELD_CONTROL_MS) return;
  lastHeldMs = millis();
  if (docked || viewMode == VIEW_MAP || viewMode == VIEW_STATUS || viewMode == VIEW_MINER) return;

  bool any = false;
  float yaw = 0.0f;
  float pitch = 0.0f;
  float roll = 0.0f;
  float throttle = 0.0f;

  if (keyDown('e', 'E')) { pitch -= 0.012f; any = true; }
  if (keyDown('s', 'S')) { pitch += 0.012f; any = true; }
  if (keyDown('a', 'A')) { yaw -= 0.014f; any = true; }
  if (keyDown('d', 'D')) { yaw += 0.014f; any = true; }
  if (keyDown('q', 'Q')) { roll -= 0.013f; any = true; }
  if (keyDown('w', 'W')) { roll += 0.013f; any = true; }

  if (any) {
    if (cockpitView != LOOK_FORWARD) cockpitView = LOOK_FORWARD;
    nudgeFlight(yaw, pitch, roll, throttle, nullptr);
  }
}

void handleChar(char raw) {
  char c = (char)tolower((unsigned char)raw);
  if (c == 'b') {
    viewMode = VIEW_MINER;
    statusLine = "RBLGANUL A9 deck";
    return;
  }
  if (viewMode == VIEW_MINER && minerTab == MINER_TAB_BEACON && (c == 'k' || c == 'n')) {
    return;
  }
  if (c == 'k') {
    buzzMinerEnabled = !buzzMinerEnabled;
    if (!buzzMinerEnabled) {
      buzzJob.active = false;
      buzzHashRate = 0;
      buzzHashCounter = 0;
    }
    viewMode = VIEW_MINER;
    statusLine = buzzMinerEnabled ? "Buzz miner ON" : "Buzz miner OFF";
    return;
  }
  if (c == 'n') {
    buzzNightMode = !buzzNightMode;
    buzzAgentBatch = buzzNightMode ? BUZZ_BATCH_NIGHT : BUZZ_BATCH_ACTIVE;
    viewMode = VIEW_MINER;
    statusLine = buzzNightMode ? "Buzz NIGHT profile" : "Buzz ACTIVE profile";
    return;
  }
  if (c == '8' || c == '*') {
    changeGameVolume(-12);
    return;
  }
  if (c == '9' || c == '(') {
    changeGameVolume(12);
    return;
  }

  if (viewMode == VIEW_MINER) {
    switch (c) {
      case 'e':
      case 'w':
      case 's':
      case 'a':
      case 'd':
      case ';':
      case ':':
      case ',':
      case '<':
      case '.':
      case '>':
      case '/':
      case '?':
        return;
      default:
        break;
    }
  }

  if (viewMode == VIEW_MAP) {
    switch (c) {
      case 'e':
      case 'w':
      case 's':
      case 'a':
      case 'd':
      case ';':
      case ':':
      case ',':
      case '<':
      case '.':
      case '>':
      case '/':
      case '?':
        return;
      case 'm':
      case 'g':
        closeCurrentPanel();
        return;
      default:
        break;
    }
  }

  if (docked && viewMode == VIEW_DOCKED) {
    switch (c) {
      case 'e':
      case 'w':
      case 's':
      case 'a':
      case 'd':
      case ';':
      case ':':
      case '/':
      case '?':
      case ',':
      case '<':
      case '.':
      case '>':
        return;
      case 'm':
      case 'g':
        viewMode = VIEW_MAP;
        statusLine = "Karta galaktiki";
        return;
      case 'i':
      case '5':
        viewMode = VIEW_STATUS;
        statusLine = "Status korablya";
        return;
      case 'o':
        launchFromDock();
        return;
      case 'l':
        shipLedEnabled = !shipLedEnabled;
        if (!shipLedEnabled) setShipLed(0, 0, 0);
        statusLine = shipLedEnabled ? "LED indikator ON" : "LED indikator OFF";
        return;
      case 'r':
        calibrateImuZero();
        return;
      default:
        statusLine = "Enter = vybor";
        return;
    }
  }

  switch (c) {
    case '1':
      setCockpitView(LOOK_FORWARD);
      return;
    case '2':
      setCockpitView(LOOK_REAR);
      return;
    case '3':
      setCockpitView(LOOK_LEFT);
      return;
    case '4':
      setCockpitView(LOOK_RIGHT);
      return;
    case '5':
    case 'i':
      viewMode = VIEW_STATUS;
      statusLine = "Status korablya";
      return;
    case 'm':
    case 'g':
      viewMode = VIEW_MAP;
      statusLine = "Karta galaktiki";
      return;
    case 'o':
      startDockOrLaunch();
      return;
    case 'x':
      setBeltCourse();
      return;
    case 'h':
      setStationCourse();
      return;
    case 'p':
      setPirateDenCourse();
      return;
    case 'l':
      shipLedEnabled = !shipLedEnabled;
      if (!shipLedEnabled) setShipLed(0, 0, 0);
      statusLine = shipLedEnabled ? "LED indikator ON" : "LED indikator OFF";
      return;
    case 'u':
      if (!docked) spawnEnemy();
      else statusLine = "Kontakty tolko v kosmose";
      return;
    case 'r':
      calibrateImuZero();
      return;
    case 't':
      imuAssistEnabled = !imuAssistEnabled;
      imuFiltYaw = 0.0f;
      imuFiltPitch = 0.0f;
      shipYawRate *= 0.25f;
      shipPitchRate *= 0.25f;
      statusLine = imuAssistEnabled ? "IMU assist ON" : "IMU assist OFF";
      return;
    case 'c':
      toggleCruise();
      return;
    case 'f':
    case ' ':
      if (viewMode != VIEW_FLIGHT && !docked) viewMode = VIEW_FLIGHT;
      fireWeapon();
      return;
    case 'e':
      if (!docked && viewMode != VIEW_FLIGHT) viewMode = VIEW_FLIGHT;
      return;
    case 's':
      if (!docked && viewMode != VIEW_FLIGHT) viewMode = VIEW_FLIGHT;
      return;
    case 'a':
      if (!docked && viewMode != VIEW_FLIGHT) viewMode = VIEW_FLIGHT;
      return;
    case 'd':
      if (!docked && viewMode != VIEW_FLIGHT) viewMode = VIEW_FLIGHT;
      return;
    case 'q':
      if (!docked && viewMode != VIEW_FLIGHT) viewMode = VIEW_FLIGHT;
      return;
    case 'w':
      if (!docked && viewMode != VIEW_FLIGHT) viewMode = VIEW_FLIGHT;
      return;
    case ';':
    case ':':
      setPower(3);
      return;
    case ',':
    case '<':
      setPower(1);
      return;
    case '.':
    case '>':
      setPower(2);
      return;
    case '/':
    case '?':
      setPower(0);
      return;
    case '[':
    case '{':
      if (brightnessIndex > 0) brightnessIndex--;
      applyBrightness();
      statusLine = "Yarkost -";
      return;
    case ']':
    case '}':
      if (brightnessIndex + 1 < sizeof(brightnessLevels)) brightnessIndex++;
      applyBrightness();
      statusLine = "Yarkost +";
      return;
    case 'j':
      statusLine = "AI v ZERO ne vklyuchen";
      return;
    case '8':
    case '*':
      changeGameVolume(-12);
      return;
    case '9':
    case '(':
      changeGameVolume(12);
      return;
    case '+':
    case '=':
      changeThrottle(THROTTLE_STEP);
      return;
    case '-':
    case '_':
      changeThrottle(-THROTTLE_STEP);
      return;
    case '0':
      setThrottle(0.0f, "Tyaga 0 / neutral");
      return;
    default:
      statusLine = "Net komandy";
      return;
  }
}

void pollInput() {
  M5Cardputer.update();
  if (viewMode == VIEW_MAP) handleGalaxyMapNav();
  else if (viewMode == VIEW_MINER) handleMinerNav();
  else if (docked) handleStationNav();
  else handleHeldKeys();
  auto keys = M5Cardputer.Keyboard.keysState();

  static char lastWordChar = 0;
  static bool prevEnter = false;
  static bool prevEsc = false;
  uint32_t now = millis();

  bool escNow = keys.opt || M5Cardputer.Keyboard.isKeyPressed((char)0x1B) || M5Cardputer.Keyboard.isKeyPressed((char)0);
  if (escNow && !prevEsc && now - lastKeyMs > KEY_ENTER_COOLDOWN_MS) {
    closeCurrentPanel();
    lastKeyMs = now;
  }
  prevEsc = escNow;

  if (keys.word.size()) {
    char raw = keys.word[0];
    char current = (char)tolower((unsigned char)raw);
    if (current != lastWordChar && now - lastKeyMs > KEY_CHAR_COOLDOWN_MS) {
      handleChar(raw);
      lastKeyMs = now;
      lastWordChar = current;
    }
  } else {
    lastWordChar = 0;
  }

  bool enterNow = keys.enter;
  if (enterNow && !prevEnter && now - lastKeyMs > KEY_ENTER_COOLDOWN_MS) {
    if (viewMode == VIEW_MAP) {
      galaxyMapInteract();
    } else if (viewMode == VIEW_MINER && minerTab == MINER_TAB_BEACON) {
      beaconInteract();
    } else if (docked) {
      if (viewMode != VIEW_DOCKED) {
        viewMode = VIEW_DOCKED;
        statusLine = "Dock menu";
      } else {
        stationInteract();
      }
    } else {
      fireWeapon();
    }
    lastKeyMs = now;
  }
  prevEnter = enterNow;
}

void setup() {
  auto cfg = M5.config();
  cfg.led_brightness = 64;
  M5Cardputer.begin(cfg, true);
  Serial.begin(115200);
  delay(100);
  Wire.begin(GROVE_SDA_PIN, GROVE_SCL_PIN, 400000U);
  initQMP6988Optional();
  envLastReadMs = millis() - ENV_READ_MS;
  M5.Display.setRotation(1);
  M5.Display.setColorDepth(16);
  M5.Led.begin();
  M5.Led.setAutoDisplay(true);
  M5Cardputer.Speaker.begin();
  applyGameVolume();
  imuReady = M5.Imu.begin();
  brightnessIndex = 3;
  applyBrightness();

  canvas.setColorDepth(16);
  canvas.createSprite(SCREEN_W, SCREEN_H);
  canvas.setTextSize(1);

  randomSeed((uint32_t)micros());
  galaxyInit();
  galaxyApplySystemContext();
  calibrateImuZero();
  updateZoneByPosition();
  resetStars();
  clearBodies();
  docked = true;
  viewMode = VIEW_MINER;
  navTarget = NAV_NONE;
  shipSpeed = 0.0f;
  shipX = stationWorld.x;
  shipY = stationWorld.y;
  shipZ = stationWorld.z - 80.0f;
  updateZoneByPosition();
  stationTab = ST_TAB_DOCK;
  stationRow = 0;
  applyDockServices(false);
  statusLine = "Rblganul A9 worker ready";
  buzzA9LoadCorpus();
  advCapBegin();
  buzzSetupEspNow();
  readEnvSensors();
  lastLogicMs = millis();
  lastFrameMs = millis();

  uint32_t setupNow = millis();
  Serial.printf("[ELITE_ZERO] v0.20 sky-anchor boot g=%u sys=%u/%s target=%u/%s known=%lu bright=%lu imu=%d worker=%u ch=%u sht=%u qmp=%u capGnss=%u capLoRa=%u mode=%s corpus=%lu/%lu/%lu store=%s\n",
                (unsigned)currentGalaxy + 1U, (unsigned)currentSystem, eliteCurrentSystem().name,
                (unsigned)targetSystem, eliteTargetSystem().name,
                (unsigned long)knownCosmosCount, (unsigned long)knownCosmosBrightCount,
                imuReady ? 1 : 0, (unsigned)buzzWorkerId, (unsigned)BUZZ_ESPNOW_CHANNEL,
                envShtFreshNow(setupNow) ? 1 : 0, envQmpFreshNow(setupNow) ? 1 : 0,
                advCapGnssStarted ? 1 : 0, advCapLoRaReady ? 1 : 0,
                buzzLegacyA9Mode ? "legacy" : "broad",
                (unsigned long)buzzCorpusHashes, (unsigned long)buzzCorpusBestBits,
                (unsigned long)buzzCorpusShares, buzzCorpusStoreName());
  Serial.println("[ELITE_ZERO] AI=OFF BuzzWorker=ON Beacon rows=terminal Enter, SKY CAP=ADV/SKY low-duty LoRa/GNSS anchor");
  updateShipLed();
  renderFrame();
}

void loop() {
  pollInput();
  readEnvSensors();
  buzzMinerTick();
  updateShipLed();

  uint32_t now = millis();
  while (now - lastLogicMs >= LOGIC_MS) {
    lastLogicMs += LOGIC_MS;
    updateSim(LOGIC_MS / 1000.0f);
  }

  if (now - lastFrameMs >= 33UL) {
    lastFrameMs = now;
    renderFrame();
  }

  delay(1);
}
