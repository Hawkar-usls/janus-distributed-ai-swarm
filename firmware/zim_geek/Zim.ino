/*
  JANUS_ZIM_GEEK_S3_SOLO_NERDMINER_v3_10D_CARR_RAMANUJAN_AUTODUNGEON_STARVEFIX_COLONY.ino

  Waveshare ESP32-S3-GEEK solo NerdMiner "Zim Geek" with JANUS swarm reporting and house-base dungeon crawler.

  What is new in v3.10D AUTODUNGEON STARVEFIX:
    - Hotfix: Buzz complaint banners no longer reset HOME timer; autoplay cannot starve while Buzz is active.
    - Keeps v3.10C: STATUS screen is a draw-only overlay, not a game mode; autoplay never pauses.
    - FINAL LOCKED patch: throttles Ramanujan math to at most one study every 450 ms.
    - FINAL LOCKED patch: Buzz JobPacket hold window keeps current chore for 4.5 s before replacing it.
    - Adds Zim's compact Carr Synopsis / Ramanujan pocket notebook.
    - Zim now carries theta3(q), mock-theta-like mu(q), partition-prior and tau-like signatures.
    - Ramanujan notebook influences SUPER ORUJIE route, White Raven features, SwarmSense entropy and status screen.
    - Keeps Stratum/Buzz binary packet ABI stable: no ESP-NOW struct size changes.
    - Adds bounded periodic study ticks, Preferences persistence and cold-safe math budget.

  What is new in v3.9E REALPOOL WALLET HARDWIRED:
    - Keeps real solo Stratum pool mining as the primary Zim mission.
    - Fixes solo-job mirror so solo pool work is never accidentally treated as a Buzz chore.
    - Buzz lazy work now stays strictly inside Buzz-assigned nonce ranges while still walking backwards.
    - Wallet is hardwired to the user-provided BTC address for immediate real-pool mining.
    - Wi-Fi defaults are kept from the working Zim branch; no secrets.h is required for this personal build.
    - Adds light cold-mode throttling for draw/swarmsense/solo batch under low heap or weak Wi-Fi.

  What is new in v3.6 SUPER WEAPON / REVERSE JRPG:
    - Adds Zim's SUPER ORUJIE: a lightweight Yaksa-inspired blackhole/slime/theta/vessel charge core.
    - Solo NerdMiner scan now walks nonce space backwards with a changing odd reverse stride.
    - JRPG battle system regained a special-attack layer: SUPER ORUJIE fires as an ultimate when charged.
    - SwarmSense exposes weapon charge/route through flags and telemetry hints.
    - Keeps Core2 planet missions, house base, RU-LAT UI, and solo Stratum.

  What was new in v3.5 ZIM DOMINION RELEASE:
    - Screen text switched to Russian transliteration (RU-LAT) to keep the same built-in ASCII font stable.
    - Core2 waiting is disabled; Buzz JobPacket chores are restored as lazy side-work.
    - Mission types now affect dungeon generation: recon/sample/raid/salvage/hold feel different.
    - Core2 high-difficulty missions apply a small imperial stim to DeerDroid instead of instant unfair wipe.
    - Pool subscribe tag updated to NerdMinerV2/ZimGeek-v3.9E-REALPOOL-WALLET.
    - Restored a small Buzz lazy worker path: JobPacket -> crooked reverse slices -> ShareResponseV2.
    - Core2 mission stim is now temporary and removed on return home.
    - Long BOOT press cancels pending Core2 order instead of only clearing legacy master lock.
    - Solo job mirror no longer expires through old Buzz job TTL logic.

  What was new in v3.2A:
    - GBA/JRPG visual overhaul from supplied v3.2 draft, merged into full working v3.1B core.
    - Keeps real Stratum NerdMiner, ESP-NOW SwarmSense, heartbeats, RX queue, and Wi-Fi config.
    - Adds animated sprite bob, deeper blue header, dungeon texture, stronger battle HUD, smooth hash display, and report error line.

  What was new in v3.1B:
    - TOP HUD pass: compact header, pool status dot, fuel/credits, better report screen.
    - HOME BASE CANON: Zim now has a fake human house as his command base.
    - PS1-era inspired original dungeon-crawler loop: Home -> Mission -> Floor -> Battle -> Return -> Lab.
    - DeerDroid starts in the house, rides missions, and gets repaired/mutated at home.
    - SwarmSense reports now include mission/capture progress while solo mining continues.

  What was new in v3.0:
    - SOLO_NERDMINER: Zim Geek no longer takes Buzz nonce-range jobs.
    - Connects to NerdMiner/Public-Pool compatible Stratum directly using your wallet.
    - Keeps ESP-NOW swarm telemetry as a separate observer/reporter, not a Buzz worker.
    - Broadcasts SwarmSense reports so Core2/Buzz/NAS can laugh at Zim's imperial progress.
    - Keeps the double-buffered JRPG displayfix.

  What was new in v2.1:
    - DISPLAYFIX: double-buffered LCD rendering using GFXcanvas16, no direct full-screen redraw flicker
    - DISPLAYFIX: default rotation changed to 3 for ESP32-S3-GEEK upside-down panels
    - DISPLAYFIX: software brightness dimming, default ~50%, safer for eyes and heat
    - DISPLAYFIX: optional ST7789 offset config kept as macros for board revisions

  What is new in v2.0:
    - keeps full ESP-NOW worker compatibility with Buzz v10.11M+
    - adds a live autonomous retro JRPG-like game on the LCD
    - Zim walks around a procedural city, hunts specimens, captures humans as samples,
      mutates them, and uses them in auto-battles
    - progress is saved in Preferences (no TF card required)
    - first companion is DeerDroid (dog-like android)
    - game is original / legally distinct, inspired by handheld-era monster-taming JRPG feel

  Libraries to install in Arduino IDE:
    - Adafruit GFX Library
    - Adafruit ST7735 and ST7789 Library

  Arduino IDE settings:
    Board: ESP32S3 Dev Module
    USB CDC On Boot: Enabled
    Flash Size: 16MB (128Mb)
    PSRAM: OPI PSRAM / Enabled
    USB Mode: Hardware CDC and JTAG
    Upload Mode: UART0 / Hardware CDC
    Upload Speed: 921600
*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <esp_arduino_version.h>
#include <mbedtls/sha256.h>
#include <Preferences.h>
#include <WiFiClient.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <math.h>

#if !__has_include(<ArduinoJson.h>)
#error "Install ArduinoJson library from Arduino Library Manager"
#endif
#include <ArduinoJson.h>

#if !__has_include(<Adafruit_GFX.h>)
#error "Install Adafruit GFX Library"
#endif
#include <Adafruit_GFX.h>

#if !__has_include(<Adafruit_ST7789.h>)
#error "Install Adafruit ST7735 and ST7789 Library"
#endif
#include <Adafruit_ST7789.h>

// Personal build: Wi-Fi/wallet are written directly below by user request.
// Optional secrets.h is intentionally not required in this branch.

// ============================================================
// USER CONFIG
// ============================================================
#define JANUS_GEEK_VERSION              "v3.10D CARR RAMANUJAN AUTODUNGEON STARVEFIX RU-LAT"
#define JANUS_NODE_ID                   "ZimGeek"
#define JANUS_NODE_ROLE                 "ZIM_LAZY"
#define JANUS_NODE_LORE                 "MicroGPTSlime brain: Carr Synopsis, Ramanujan theta, SHA sekrety, Buzz lazy, swarm memory"

// ============================================================
// SOLO NERDMINER CONFIG
// ============================================================
// Personal direct config for Zim Geek real-pool mission.
// This build does not require secrets.h.
#define ZIM_WIFI_SSID "YOUR_WIFI"
#define ZIM_WIFI_PASSWORD "YOUR_PASSWORD"
#define ZIM_WALLET_ADDRESS              "1F1Y6CdkApZboDF6g1DYrQ8Dke2E5gWiP1"
#define ZIM_WORKER_NAME                 "ZimGeek"
#define ZIM_POOL_HOST                   "pool.nerdminers.org"
#define ZIM_POOL_PORT                   3333
#define ZIM_POOL_PASSWORD "YOUR_PASSWORD"
#define ZIM_POOL_SUBSCRIBE_TAG          "NerdMinerV2/ZimGeek-v3.10D-STARVEFIX"

// Zim is a SOLO NerdMiner first, but he also appears in Buzz colony as a lazy worker.
// Buzz jobs are accepted, but processed as low-priority side quests.
#define ZIM_SEND_SWARMSENSE             1
#define ZIM_SEND_LEGACY_COLONY_HEARTBEAT 1  // Buzz sees Zim and can send JobPacket ranges
#define ZIM_SWARMSENSE_MS               3000UL
#define ZIM_POOL_RECONNECT_MIN_MS       4000UL
#define ZIM_STRATUM_DEBUG_LINES         18
#define ZIM_MIN_SHARE_BITS              16
#define ZIM_SOLO_BATCH                  900
#define ZIM_REVERSE_NONCE_ENGINE         1
#define ZIM_SUPER_WEAPON_MAX_CHARGE      100
#define ZIM_SUPER_WEAPON_FIRE_CHARGE     100
#define ZIM_MINER_STACK_BYTES           12288
#define ZIM_COLD_MODE                   1
#define ZIM_COLD_LOW_HEAP_BYTES         98000UL
#define ZIM_COLD_WIFI_RSSI_DBM          -84
#define ZIM_SOLO_BATCH_COLD             650
#define ZIM_DRAW_MS_COLD                160UL
#define ZIM_SWARMSENSE_MS_COLD          7000UL

#define ZIM_RAMANUJAN_BOOK              1
#define ZIM_RAMANUJAN_DEPTH             12
#define ZIM_RAMANUJAN_STUDY_MS          5000UL
#define ZIM_RAMANUJAN_MIN_GAP_MS        450UL   // final: powf/theta budget guard for miner task
#define ZIM_RAMANUJAN_SAVE_MS           60000UL
#define ZIM_RAMANUJAN_CARR_RESULTS      5000UL
#define ZIM_RAMANUJAN_CARR_YEAR         1903UL
#define ZIM_RAMANUJAN_THETA_WEIGHT      37U

// Buzz lazy side-worker: Zim accepts ranges from Buzz, but treats them like annoying chores.
#define ZIM_BUZZ_LAZY_WORKER            1
#define ZIM_BUZZ_LAZY_BATCH             64
#define ZIM_BUZZ_LAZY_EVERY_MS          700UL
#define ZIM_BUZZ_LAZY_SKIP_MASK         0x03   // 3/4 lazy ticks are skipped on purpose
#define ZIM_BUZZ_LAZY_MAX_JOB_AGE_MS    12000UL
#define ZIM_BUZZ_JOB_HOLD_MS            4500UL  // final: do not replace fresh Buzz chore immediately

// White Raven agent: tiny micrograd-like learner on ESP32.
// It learns Zim's own crooked policy and broadcasts memory to Buzz/Core2/NAS bridges.
#define ZIM_WHITE_RAVEN_AGENT           1
#define ZIM_AGENT_INPUTS                12
#define ZIM_AGENT_OUTPUTS               5
#define ZIM_AGENT_LR                    0.045f
#define ZIM_AGENT_MEMORY_MS             12000UL
#define ZIM_AGENT_SAVE_MS               60000UL
#define ZIM_AGENT_NAS_MS                30000UL
#define ZIM_NAS_MEMORY_HTTP             0
#define ZIM_NAS_MEMORY_URL              "http://192.168.1.2:8787/janus/zim-memory"


#define JANUS_BUZZ_WIFI_CHANNEL         10
#define JANUS_AUTO_CHANNEL_SCAN         0   // solo miner must stay on Wi-Fi channel for Stratum
#define JANUS_SCAN_MIN_CHANNEL          1
#define JANUS_SCAN_MAX_CHANNEL          13
#define JANUS_SCAN_STEP_MS              1400UL
#define JANUS_MASTER_LOST_MS            16000UL

#define JANUS_HEARTBEAT_MS              1500UL
#define JANUS_ENTROPY_MS                3000UL
#define JANUS_STATUS_MS                 4000UL
#define JANUS_JOB_TTL_MS                9000UL

#define JANUS_MIN_BATCH                 80
#define JANUS_DEFAULT_BATCH             900
#define JANUS_MAX_BATCH                 1800

#define JANUS_RX_QUEUE_LEN              6
#define JANUS_RX_DATA_MAX               192
#define JANUS_RX_DRAIN_PER_LOOP         4

// Waveshare ESP32-S3-GEEK display / button pins.
#define TFT_CS_PIN                      10
#define TFT_DC_PIN                      8
#define TFT_RST_PIN                     9
#define TFT_SCLK_PIN                    12
#define TFT_MOSI_PIN                    11
#define TFT_BL_PIN                      -1

// Display fix tuning.
// If your screen is still upside-down after this build, set JANUS_LCD_ROTATION back to 1.
#define JANUS_LCD_ROTATION              3
#define JANUS_LCD_INVERT_COLORS         1
#define JANUS_LCD_SOFT_BRIGHTNESS       112   // default runtime soft brightness; long BOOT press cycles levels
#define ZIM_INVADER_FX_LEVEL            0     // v3.9C: filter/overlay removed; pure GBA/JRPG visual
#define JANUS_LCD_USE_COLROW_OFFSET     0     // keep 0 unless image is shifted/cropped
#define JANUS_LCD_COL_OFFSET            0
#define JANUS_LCD_ROW_OFFSET            0

#define JANUS_BOOT_BUTTON_PIN           0
#define JANUS_BUTTON_LONG_MS            1500UL
#define JANUS_BUTTON_VERY_LONG_MS       3600UL

// Game tuning.
#define GAME_SCREEN_W                   240
#define GAME_SCREEN_H                   135
#define GAME_HEADER_H                   15
#define GAME_TILE                       8
#define GAME_MAP_W                      30
#define GAME_MAP_H                      15
#define GAME_SAVE_MS                    60000UL
#define GAME_DRAW_MS                    100UL
#define GAME_STEP_MS                    220UL
#define GAME_BATTLE_STEP_MS             850UL
#define GAME_DIALOG_MS                  950UL
#define GAME_AUTOSCAN_STATUS_MS         800UL
#define GAME_PARTY_MAX                  3
#define GAME_SEEN_RING                  8

// Built-in Adafruit font is ASCII-only. RU-LAT keeps Russian UI wording without Cyrillic glyph bugs.
#define ZIM_UI_RU_LAT                    1

// ============================================================
// DISPLAY
// ============================================================
SPIClass tftSPI(FSPI);
Adafruit_ST7789 lcd = Adafruit_ST7789(&tftSPI, TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);

// Back buffer. All game drawing goes here, then one RGB bitmap push goes to the LCD.
// This is the same anti-flicker principle as M5Canvas::pushSprite in the GroundOps sketch.
GFXcanvas16 tft(GAME_SCREEN_W, GAME_SCREEN_H);

// Runtime brightness: cheap ST7789 panels look better with softer light + high-contrast overlay.
const uint8_t ZIM_LCD_BRIGHTNESS_LEVELS[6] = {64, 88, 112, 144, 176, 216};
uint8_t zimBrightnessIndex = 2;
uint8_t zimLcdSoftBrightness = JANUS_LCD_SOFT_BRIGHTNESS;
uint32_t zimLastBrightnessToastMs = 0;

#define ZIM_SLIME_MAGIC        0x5A534C39UL  // ZSL9
#define ZIM_SLIME_VERSION      3
#define ZIM_SLIME_EPISODES     16

enum ZimSlimeMood : uint8_t {
  ZM_CONFUSED = 0,
  ZM_SCOUT = 1,
  ZM_OBSESSED = 2,
  ZM_OFFENDED = 3,
  ZM_TRICKSTER = 4,
  ZM_TRIUMPH = 5
};

enum ZimSlimeEvent : uint8_t {
  ZE_TICK = 0,
  ZE_SOLO_ACCEPT = 1,
  ZE_BUZZ_JOB = 2,
  ZE_BUZZ_SHARE = 3,
  ZE_SUPER_READY = 4,
  ZE_BATTLE = 5,
  ZE_CORE2_SPAM = 6,
  ZE_RAMANUJAN_STUDY = 7
};

struct ZimSlimeEpisode {
  uint32_t t;
  uint8_t event;
  uint8_t mood;
  uint8_t policy;
  uint8_t intensity;
  uint16_t shaBits;
  uint16_t flags;
};

struct ZimSlimeBrain {
  uint32_t magic;
  uint8_t version;
  uint32_t ticks;
  uint32_t rng;
  uint32_t lastSaveMs;
  float trustBuzz;
  float trustPool;
  float trustSwarm;
  float suspicionHumans;
  float curiosityEarth;
  float shaObsession;
  float btcHunger;
  float ego;
  float shame;
  float secrecy;
  float comfort;
  float noveltyAvg;
  float dangerAvg;
  float predictionError;
  float mapConfidence;
  uint8_t mood;
  uint8_t faceIndex;
  uint8_t lastPolicy;
  uint8_t episodeHead;
  ZimSlimeEpisode episodes[ZIM_SLIME_EPISODES];
  char thought[48];
};

ZimSlimeBrain zimSlime{};
uint32_t lastSlimeSaveMs = 0;

// ============================================================
// JANUS BUZZ COLONY PROTOCOL - keep byte-compatible with Buzz
// ============================================================
static uint8_t JANUS_BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

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

struct __attribute__((packed)) ShareResponseV2 {
  uint8_t magic[2];
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
  uint16_t bits;
  uint32_t total_hashes_l32;
  uint8_t hash_tail[4];
};

struct __attribute__((packed)) EntropyReport {
  uint8_t magic[2];
  uint16_t worker_id;
  float local_entropy;
  uint8_t sensor_flags;
  float values[4];
};


struct __attribute__((packed)) SwarmSensePacket {
  uint8_t magic[2];        // 'S','S'
  uint8_t version;         // 1
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
  uint8_t radio_mode;      // 1 = ESP-NOW observer online
  uint8_t bt_flags;
  uint8_t palette;         // here: game mode
  uint8_t knn_label;       // local state label
  uint8_t knn_confidence;  // 0..100
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

// White Raven memory delta. Buzz/Core2/NAS bridge can persist this as Zim's external memory.
struct __attribute__((packed)) ZimAgentMemoryPacket {
  uint8_t magic[2];        // 'Z','A'
  uint8_t version;         // 1
  uint16_t worker_id;
  char nodeId[24];
  char kind[16];           // zim_slime_ai
  uint32_t seq;
  uint32_t uptime_ms;
  uint32_t epoch;
  uint32_t updates;
  uint32_t accepts;
  uint32_t buzzShares;
  uint16_t reward_x1000;
  uint16_t loss_x1000;
  uint8_t policy;          // 0 void, 1 solo, 2 buzz-lazy, 3 super
  uint8_t confidence;
  uint8_t lazyMask;
  uint8_t route;
  uint8_t weaponCharge;
  uint8_t flags;
  int8_t weights_q7[ZIM_AGENT_INPUTS * ZIM_AGENT_OUTPUTS];
  uint8_t slimeMood;
  uint8_t slimeFace;
  uint8_t shaObsession;
  uint8_t btcHunger;
  uint8_t curiosity;
  uint8_t suspicion;
  uint8_t ego;
  uint8_t shame;
  uint8_t trustBuzz;
  uint8_t trustSwarm;
  uint32_t slimeTicks;
  char thought[32];
};

struct __attribute__((packed)) JanusAgentRewardPacket {
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

// v3.3: Core2 Galaxy Station -> Zim planet-side solo mission order.
// This does NOT replace Stratum mining; it only drives the house-base dungeon loop.
struct __attribute__((packed)) ZimMissionPacket {
  uint8_t magic[2];       // 'Z','M'
  uint8_t version;        // 1
  uint8_t sector;         // unified universe sector / planet hint
  uint8_t missionType;    // 0 recon, 1 sample, 2 raid, 3 salvage, 4 hold
  uint8_t priority;       // 0..255
  uint8_t floorMax;       // suggested dungeon depth
  uint16_t fuel;          // suggested expedition fuel
  uint16_t difficulty_x100;
  uint32_t mission_id;
  uint32_t seed;
  uint32_t flags;         // bit0 galaxy visible, bit1 crisis, bit2 raid, bit3 Zim Earth
  char target[16];        // ZimGeek / all
  char planet[16];        // planet/sector label
  char order[40];         // short UI/log line
};

// ============================================================
// RX QUEUE
// ============================================================
enum JanusRxType : uint8_t {
  RX_NONE = 0,
  RX_JOB = 1,
  RX_AGENT_REWARD = 2,
  RX_ZIM_MISSION = 3
};

struct __attribute__((packed)) JanusRxItem {
  uint8_t type;
  uint8_t mac[6];
  int8_t rssi;
  uint16_t len;
  uint8_t data[JANUS_RX_DATA_MAX];
};

QueueHandle_t gRxQueue = nullptr;
Preferences prefs;

// ============================================================
// HASHER STATE
// ============================================================
portMUX_TYPE jobMux = portMUX_INITIALIZER_UNLOCKED;

volatile bool gJobActive = false;
volatile uint32_t gNonceCursor = 0;
volatile uint32_t gNonceEnd = 0;
volatile uint32_t gNonceRemaining = 0;
volatile uint32_t gJobRxMs = 0;
volatile uint32_t gRangeSize = 0;
volatile uint32_t gExtranonce2 = 0;

uint8_t gJobId[8] = {0};
uint8_t gHeader[80] = {0};
uint8_t gTarget[32] = {0};
uint8_t gMasterMac[6] = {0};
volatile bool gHaveMaster = false;
volatile uint32_t gLastMasterMs = 0;
volatile uint8_t gCurrentChannel = JANUS_BUZZ_WIFI_CHANNEL;

volatile uint32_t gSeq = 0;
volatile uint32_t gTotalHashes = 0;
volatile uint32_t gHashesThisSecond = 0;
volatile uint32_t gHashRate = 0;
volatile uint32_t gHashRateSmooth = 0;
volatile uint32_t gSharesSent = 0;
volatile uint32_t gRejects = 0;
volatile uint32_t gBestBits = 0;
volatile uint32_t gJobsRx = 0;
volatile uint32_t gRewardsRx = 0;
volatile uint32_t gLastShareMs = 0;
volatile uint32_t gLastJobLogMs = 0;
volatile uint32_t gBuzzLazyHashes = 0;
volatile uint32_t gBuzzSharesSent = 0;
volatile uint32_t gBuzzSharesFail = 0;
volatile uint32_t gBuzzJobsAccepted = 0;
volatile uint32_t gBuzzJobsIgnored = 0;
volatile uint32_t gBuzzJobsHeld = 0;
volatile uint32_t gBuzzLastHoldLogMs = 0;
volatile uint32_t gBuzzLazySkips = 0;
volatile uint32_t gBuzzLastWorkMs = 0;
volatile uint32_t gRxQueued = 0;
volatile uint32_t gRxDropped = 0;
volatile uint32_t gRxOversize = 0;
volatile int8_t gLastRssi = 0;

volatile uint16_t gAiBatch = JANUS_DEFAULT_BATCH;
volatile uint8_t gAiHint = 1;
volatile uint16_t gTargetBits = 0;
volatile float gLastPredictionError = 0.0f;
volatile float gLastRewardScore = 0.0f;

uint32_t lastHeartbeatMs = 0;

// ============================================================
// SOLO STRATUM STATE
// ============================================================
portMUX_TYPE soloMux = portMUX_INITIALIZER_UNLOCKED;
uint8_t soloHeader[80] = {0};
uint8_t soloTarget[32] = {0};
char soloJobId[96] = "";
char soloNtimeHex[9] = "";
char soloEn2Hex[17] = "";
volatile bool soloJobReady = false;
volatile bool soloPoolConnected = false;
volatile bool soloAuthorized = false;
volatile uint32_t soloSubmits = 0;
volatile uint32_t soloAccepts = 0;
volatile uint32_t soloRejects = 0;
volatile uint32_t soloLowDiffRejects = 0;
volatile uint32_t soloStaleRejects = 0;
volatile uint32_t soloOtherRejects = 0;
volatile uint32_t soloReconnects = 0;
volatile uint32_t soloLastJobMs = 0;
volatile uint32_t soloLastSubmitMs = 0;
volatile uint32_t soloLastAcceptMs = 0;
volatile uint32_t soloLastPoolRxMs = 0;
volatile uint16_t soloShareTargetBits = 0;
volatile float soloDifficulty = 1.0f;
char soloStatus[16] = "BOOT";
char soloLastError[64] = "-";
String soloExtranonce1 = "";
uint8_t soloExtranonce2Size = 4;
uint64_t soloExtranonce2 = 0;
uint32_t soloNonce = 0;
uint32_t lastSwarmSenseMs = 0;

// White Raven micrograd-like agent state.
struct ZimMicrogradAgent {
  float w[ZIM_AGENT_OUTPUTS][ZIM_AGENT_INPUTS];
  float y[ZIM_AGENT_OUTPUTS];
  float rewardEma;
  float lossEma;
  uint32_t epoch;
  uint32_t updates;
  uint32_t lastSoloAccept;
  uint32_t lastBuzzShare;
  uint32_t lastBestBits;
  uint8_t policy;
  uint8_t confidence;
  uint8_t lazyMask;
  bool dirty;
};
ZimMicrogradAgent zimAgent{};
uint32_t lastAgentMemoryMs = 0;
uint32_t lastAgentSaveMs = 0;
uint32_t lastAgentNasMs = 0;

// v3.3 Core2 mission-control state.
ZimMissionPacket zimCurrentMission{};
volatile bool zimMissionPending = false;
volatile bool zimMissionActive = false;
volatile uint32_t zimMissionRx = 0;
volatile uint32_t zimMissionAccepted = 0;
volatile uint32_t zimMissionIgnored = 0;
volatile uint32_t zimLastMissionMs = 0;
uint8_t zimMissionSector = 6;
char zimMissionLine[64] = "Core2 vyklyuchen; Buzz shlet hlam";
char zimPlanetLine[20] = "Zemlya-Zim";
char zimOrderLine[48] = "Buzz: rabotay. Zim: potom";
uint16_t zimMissionStimHpBonus = 0;
bool zimMissionStimActive = false;
uint32_t zimMissionCancelled = 0;

uint32_t lastEntropyMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastHashRateMs = 0;
uint32_t lastScanMs = 0;
uint32_t bootButtonDownMs = 0;
bool bootButtonWasDown = false;
// v3.10C: status screen is an overlay only. It must never pause HOME -> MISSION -> DUNGEON autoplay.
bool zimStatusOverlay = false;
// v3.10D: Buzz complaint banners must not reset HOME timing, or autoplay starves forever while Buzz is active.
uint32_t zimLastBuzzWhineMs = 0;
TaskHandle_t gMinerTaskHandle = nullptr;

// ============================================================
// GAME STATE
// ============================================================
enum GameMode : uint8_t {
  GM_BOOT = 0,
  GM_HOME = 1,
  GM_MISSION = 2,
  GM_DUNGEON = 3,
  GM_ENCOUNTER = 4,
  GM_BATTLE = 5,
  GM_LAB = 6,
  GM_RETURN = 7,
  GM_REPORT = 8
};

struct Monster {
  uint32_t seed;
  char name[16];
  uint8_t level;
  uint16_t hp;
  uint16_t maxHp;
  uint8_t atk;
  uint8_t def;
  uint8_t spd;
  uint8_t mut;
  uint8_t classId;
  uint8_t humanoid;
};

struct SaveBlob {
  uint32_t magic;
  uint32_t townSeed;
  uint32_t seen;
  uint32_t captured;
  uint32_t credits;
  uint32_t steps;
  uint32_t battleWins;
  uint32_t labVisits;
  uint32_t rngCounter;
  uint32_t lastSeenSeeds[GAME_SEEN_RING];
  uint8_t lastSeenCount;
  uint8_t partyCount;
  int8_t px;
  int8_t py;
  uint8_t rank;
  Monster party[GAME_PARTY_MAX];

  // v3.1 house-base dungeon data
  uint32_t houseSeed;
  uint32_t dungeonSeed;
  uint32_t missions;
  uint32_t missionSuccess;
  uint32_t missionFails;
  uint32_t loot;
  uint32_t traps;
  uint16_t fuel;
  uint8_t floorLevel;
  uint8_t floorMax;
  uint8_t missionId;
  uint8_t housePhase;
};

struct GameState {
  GameMode mode = GM_BOOT;
  uint32_t townSeed = 0;
  uint32_t houseSeed = 0;
  uint32_t dungeonSeed = 0;
  uint32_t seen = 0;
  uint32_t captured = 0;
  uint32_t credits = 0;
  uint32_t steps = 0;
  uint32_t battleWins = 0;
  uint32_t labVisits = 0;
  uint32_t rngCounter = 0;
  uint32_t missions = 0;
  uint32_t missionSuccess = 0;
  uint32_t missionFails = 0;
  uint32_t loot = 0;
  uint32_t traps = 0;
  uint32_t lastSeenSeeds[GAME_SEEN_RING] = {0};
  uint8_t lastSeenCount = 0;
  uint8_t rank = 1;
  Monster party[GAME_PARTY_MAX];
  uint8_t partyCount = 0;
  int8_t px = 3;
  int8_t py = 11;
  int8_t tx = 10;
  int8_t ty = 10;
  uint16_t fuel = 20;
  uint8_t floorLevel = 0;
  uint8_t floorMax = 3;
  uint8_t missionId = 0;
  uint8_t housePhase = 0;
  uint32_t lastMoveMs = 0;
  uint32_t lastDrawMs = 0;
  uint32_t lastLogicMs = 0;
  uint32_t lastSaveMs = 0;
  uint32_t stateStartMs = 0;
  uint32_t battleStepMs = 0;
  bool dirty = false;
  Monster enemy;
  uint8_t enemyPhase = 0;
  char banner[64] = "Booting fake-house invasion...";
  char subBanner[64] = "";
  bool captureFlash = false;
};

GameState game;

// ============================================================
// ZIM SUPER ORUJIE - tiny Yaksa-inspired reverse miner weapon
// ============================================================
enum ZimWeaponRoute : uint8_t {
  ZW_VOID = 0,
  ZW_SEEK = 1,
  ZW_FLOW = 2,
  ZW_BITE = 3
};

struct ZimSuperWeaponState {
  uint8_t charge = 7;        // 0..100
  uint8_t route = ZW_VOID;   // VOID/SEEK/FLOW/BITE
  uint8_t resonance = 0;     // theta-like signature
  uint8_t slime = 0;         // path conductivity
  uint8_t vessel = 0;        // symbiotic membrane charge
  uint8_t blackhole = 0;     // compression/event horizon
  uint32_t shots = 0;
  uint32_t lastFireMs = 0;
  uint32_t reverseStride = 0x9E3779B9UL; // odd, valid reverse nonce walk
  uint32_t lastPulseMs = 0;
};

ZimSuperWeaponState superWeapon;

#define ZIM_RAMANUJAN_MAGIC      0x52414D41UL  // RAMA
#define ZIM_RAMANUJAN_VERSION    1

struct ZimRamanujanNotebook {
  uint32_t magic;
  uint8_t version;
  uint32_t studies;
  uint32_t tauLike;
  uint32_t carrIndex;       // Carr Synopsis theorem/result index, 1..5000
  uint32_t lastStudyMs;
  uint32_t lastSaveMs;
  uint16_t theta3_x1000;    // theta3(q) scaled
  int16_t mock_x1000;       // mock-theta-like mu(q) scaled
  uint16_t partition_x1000; // partition prior scaled
  uint16_t q_x1000;         // q-series seed scaled
  uint8_t resonance;        // 0..255
  uint8_t confidence;       // 0..100
  uint8_t chapter;          // synthetic Carr chapter/page group
  char lemma[48];           // RU-LAT pocket note
};

ZimRamanujanNotebook zimRama{};


// ============================================================
// UTILS
// ============================================================
uint16_t workerId() {
  return (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
}

const char* resetReasonText(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXT";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "UNKNOWN";
  }
}

void macToText(const uint8_t mac[6], char* out, size_t n) {
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void writeLE32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

void hashToShareOrder(const uint8_t in[32], uint8_t out[32]) {
  for (int i = 0; i < 32; ++i) out[i] = in[31 - i];
}

uint16_t countLeadingZeroBits(const uint8_t h[32]) {
  uint16_t bits = 0;
  for (int i = 0; i < 32; i++) {
    uint8_t b = h[i];
    if (b == 0) { bits += 8; continue; }
    for (int k = 7; k >= 0; k--) {
      if ((b & (1 << k)) == 0) bits++;
      else return bits;
    }
  }
  return bits;
}

bool hashMeetsTargetBytes(const uint8_t hash[32], const uint8_t target[32]) {
  for (int i = 0; i < 32; i++) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;
}

void hexStringToBytes(const String& hex, uint8_t *bytes) {
  int n = hex.length();
  for (int i = 0; i + 1 < n; i += 2) {
    char tmp[3] = { hex[i], hex[i + 1], 0 };
    bytes[i / 2] = (uint8_t)strtoul(tmp, nullptr, 16);
  }
}

void reverse_bytes(uint8_t *data, int len) {
  for (int i = 0; i < len / 2; i++) {
    uint8_t t = data[i];
    data[i] = data[len - 1 - i];
    data[len - 1 - i] = t;
  }
}

void reverse_word_bytes(uint8_t *data, int len) {
  for (int off = 0; off + 3 < len; off += 4) {
    uint8_t t0 = data[off + 0];
    uint8_t t1 = data[off + 1];
    data[off + 0] = data[off + 3];
    data[off + 1] = data[off + 2];
    data[off + 2] = t1;
    data[off + 3] = t0;
  }
}

void formatExtranonce2LE(uint64_t value, uint8_t sizeBytes, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (sizeBytes == 0) return;
  if (sizeBytes > 8) sizeBytes = 8;
  if (outSize < (size_t)sizeBytes * 2 + 1) return;
  for (uint8_t i = 0; i < sizeBytes; ++i) {
    uint8_t b = (uint8_t)((value >> (8 * i)) & 0xFF);
    snprintf(out + i * 2, outSize - i * 2, "%02x", b);
  }
  out[sizeBytes * 2] = '\0';
}

const uint8_t BTC_DIFF1_TARGET[32] = {
  0x00,0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

void makeTargetWithMinLeadingZeroBits(uint8_t out[32], uint16_t minBits) {
  memset(out, 0xFF, 32);
  if (minBits >= 256) { memset(out, 0, 32); return; }
  uint16_t fullZeroBytes = minBits / 8;
  uint8_t remBits = minBits % 8;
  for (uint16_t i = 0; i < fullZeroBytes && i < 32; ++i) out[i] = 0x00;
  if (fullZeroBytes < 32 && remBits) out[fullZeroBytes] = (uint8_t)(0xFF >> remBits);
}

void setSoloShareTargetFromDifficulty(float diff) {
  if (diff <= 0.0f || !isfinite(diff)) diff = 1.0f;
  uint8_t target[32] = {0};
  float effectiveDiff = diff;
  if (effectiveDiff < 0.000001f) effectiveDiff = 0.000001f;
  if (effectiveDiff < 1.0f) {
    double mulD = 1.0 / (double)effectiveDiff;
    if (mulD < 1.0) mulD = 1.0;
    if (mulD > 4294967295.0) mulD = 4294967295.0;
    uint32_t mul = (uint32_t)(mulD + 0.5);
    uint32_t carry = 0;
    for (int i = 31; i >= 0; --i) {
      uint64_t v = (uint64_t)BTC_DIFF1_TARGET[i] * (uint64_t)mul + carry;
      target[i] = (uint8_t)(v & 0xFF);
      carry = (uint32_t)(v >> 8);
    }
    if (carry) memset(target, 0xFF, sizeof(target));
  } else {
    uint32_t div = (uint32_t)(effectiveDiff + 0.5f);
    if (div < 1) div = 1;
    uint64_t rem = 0;
    for (int i = 0; i < 32; i++) {
      uint64_t cur = (rem << 8) | BTC_DIFF1_TARGET[i];
      target[i] = (uint8_t)(cur / div);
      rem = cur % div;
    }
  }
  uint16_t bits = countLeadingZeroBits(target);
  if (bits < ZIM_MIN_SHARE_BITS) {
    makeTargetWithMinLeadingZeroBits(target, ZIM_MIN_SHARE_BITS);
    bits = countLeadingZeroBits(target);
  }
  portENTER_CRITICAL(&soloMux);
  memcpy(soloTarget, target, 32);
  soloShareTargetBits = bits;
  soloDifficulty = diff;
  portEXIT_CRITICAL(&soloMux);
  gTargetBits = bits;
}

String zimMinerUserString() {
  return String(ZIM_WALLET_ADDRESS) + "." + String(ZIM_WORKER_NAME);
}

bool zimSecretsReady() {
  return strlen(ZIM_WIFI_SSID) > 0 &&
         strcmp(ZIM_WIFI_SSID, "YOUR_WIFI_SSID") != 0 &&
         strlen(ZIM_WALLET_ADDRESS) > 0 &&
         strcmp(ZIM_WALLET_ADDRESS, "YOUR_BTC_ADDRESS") != 0;
}

bool zimColdMode() {
#if ZIM_COLD_MODE
  if (ESP.getFreeHeap() < ZIM_COLD_LOW_HEAP_BYTES) return true;
  if (WiFi.status() == WL_CONNECTED && WiFi.RSSI() < ZIM_COLD_WIFI_RSSI_DBM) return true;
#endif
  return false;
}

uint16_t zimSoloBatchNow() {
  return zimColdMode() ? (uint16_t)ZIM_SOLO_BATCH_COLD : (uint16_t)ZIM_SOLO_BATCH;
}

uint32_t zimDrawIntervalMs() {
  return zimColdMode() ? (uint32_t)ZIM_DRAW_MS_COLD : (uint32_t)GAME_DRAW_MS;
}

uint32_t zimSwarmSenseIntervalMs() {
  return zimColdMode() ? (uint32_t)ZIM_SWARMSENSE_MS_COLD : (uint32_t)ZIM_SWARMSENSE_MS;
}

void setSoloStatus(const char* s, const char* err = nullptr) {
  strlcpy(soloStatus, s ? s : "-", sizeof(soloStatus));
  if (err) strlcpy(soloLastError, err, sizeof(soloLastError));
}

void doubleSha256(mbedtls_sha256_context* ctx, const uint8_t* data, size_t len, uint8_t out[32]) {
  uint8_t first[32];
  mbedtls_sha256_starts(ctx, 0);
  mbedtls_sha256_update(ctx, data, len);
  mbedtls_sha256_finish(ctx, first);
  mbedtls_sha256_starts(ctx, 0);
  mbedtls_sha256_update(ctx, first, 32);
  mbedtls_sha256_finish(ctx, out);
}

void updateBestBits(uint16_t bits) {
  if (bits > gBestBits) gBestBits = bits;
}

bool macLooksValid(const uint8_t mac[6]) {
  bool any = false;
  bool allff = true;
  for (int i = 0; i < 6; ++i) {
    if (mac[i] != 0x00) any = true;
    if (mac[i] != 0xFF) allff = false;
  }
  return any && !allff;
}

static uint32_t mix32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dUL;
  x ^= x >> 15;
  x *= 0x846ca68bUL;
  x ^= x >> 16;
  return x;
}

void setEspNowChannel(uint8_t ch, bool announce) {
  if (ch < JANUS_SCAN_MIN_CHANNEL || ch > JANUS_SCAN_MAX_CHANNEL) ch = JANUS_BUZZ_WIFI_CHANNEL;
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  gCurrentChannel = ch;
  if (announce) Serial.printf("[ZIM] ESP-NOW channel=%u\n", (unsigned)ch);
}

void ensurePeer(const uint8_t mac[6]) {
  if (!macLooksValid(mac) && memcmp(mac, JANUS_BROADCAST_MAC, 6) != 0) return;
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_err_t err = esp_now_add_peer(&peer);
  if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
    char m[24]; macToText(mac, m, sizeof(m));
    Serial.printf("[ZIM] peer add failed mac=%s err=%d\n", m, (int)err);
  }
}

void clearMasterLock(const char* why) {
  portENTER_CRITICAL(&jobMux);
  memset(gMasterMac, 0, sizeof(gMasterMac));
  gHaveMaster = false;
  gJobActive = false;
  gNonceRemaining = 0;
  gLastMasterMs = 0;
  portEXIT_CRITICAL(&jobMux);
  Serial.printf("[ZIM] master lock cleared: %s\n", why ? why : "manual");
}

bool targetMatchesNode(const char target[24]) {
  if (strncmp(target, JANUS_NODE_ID, 24) == 0) return true;
  if (strncmp(target, "Zim Geek", 24) == 0) return true;
  if (strncmp(target, "all", 24) == 0) return true;
  if (strncmp(target, "*", 24) == 0) return true;
  return false;
}

bool zimMissionTargetMatches(const char target[16]) {
  if (!target || !target[0]) return true;
  if (!strncmp(target, JANUS_NODE_ID, 16)) return true;
  if (!strncmp(target, "ZimGeek", 16)) return true;
  if (!strncmp(target, "ZIM", 16)) return true;
  if (!strncmp(target, "all", 16)) return true;
  if (!strncmp(target, "*", 16)) return true;
  return false;
}

const char* zimMissionTypeName(uint8_t t) {
  switch (t % 5) {
    case 0: return "RAZVED";
    case 1: return "OBRAZ";
    case 2: return "NALOT";
    case 3: return "SBOR";
    default: return "UDERJ";
  }
}

void setBanner(const char* a, const char* b = nullptr);
void startMissionFromCore2();
const char* superWeaponRouteText();
void superWeaponTick(uint16_t bits, bool shareLike);
void superWeaponOnPoolResult(bool accepted);
bool superWeaponReadyForBattle();
uint16_t fireSuperWeapon(Monster& ally, Monster& enemy);
void saveSuperWeaponPrefs();
void loadSuperWeaponPrefs();
void zimRamanujanReset();
void zimRamanujanLoadPrefs();
void zimRamanujanSavePrefs(bool force);
void zimRamanujanStudyTick(const char* reason, uint16_t bits, bool shareLike);
uint8_t zimRamanujanSignal();
float zimRamanujanNorm();
const char* zimRamanujanLemma();
void zimSlimeLine(const char* s);
void zimAgentLoadPrefs();
void zimAgentSavePrefs();
void zimAgentTick(const char* reason, float extReward);
void sendZimAgentMemory();
void zimAgentMaybePostNas();
float zimClip(float x, float lo, float hi);
uint8_t zimF2B(float x);
void zimSlimeReset();
void zimSlimeLoadPrefs();
void zimSlimeSavePrefs(bool force);
void zimSlimeObserve(uint8_t event, float intensity);
const char* zimSlimeFace();
const char* zimSlimeMoodText();
void loadDisplayPrefs();
void saveDisplayPrefs();
void cycleZimBrightness();
void drawInvaderOverlay();

void handleZimMission(const uint8_t* srcMac, int8_t rssi, const ZimMissionPacket& zm) {
  (void)srcMac;
  if (zm.magic[0] != 'Z' || zm.magic[1] != 'M' || zm.version != 1) return;
  // v3.7: Core2 is not required. Zim no longer waits for ZM orders in this branch.
  // If old Core2 packets are present, he logs them as imperial spam and keeps doing Buzz/solo chaos.
  zimMissionRx++;
  zimMissionIgnored++;
  zimSlimeObserve(ZE_CORE2_SPAM, 0.55f);
  gLastRssi = rssi;
  zimMissionPending = false;
  zimMissionActive = false;
  snprintf(zimMissionLine, sizeof(zimMissionLine), "Core2 spam otklonen S%02u", (unsigned)zm.sector);
  snprintf(zimOrderLine, sizeof(zimOrderLine), "Buzz vazhnee. Core2 potom");
  if ((zimMissionIgnored & 0x03UL) == 1UL) {
    setBanner("Core2 prikaz v arhiv", "Zim seychas lenitsya dlya Buzz");
    game.dirty = true;
  }
  Serial.printf("[ZIMCTRL] Core2 ZM ignored #%lu sector=%u type=%s order=%s\n",
                (unsigned long)zimMissionIgnored, (unsigned)zm.sector, zimMissionTypeName(zm.missionType), zm.order);
}

// ============================================================
// GAME PROCEDURAL HELPERS
// ============================================================
const char* HUMAN_A[] = {"Klerk","Kurer","Sonya","Broker","Turist","Brodya","Stajer","Torgash","Ohrana","Shalun"};
const char* HUMAN_B[] = {"Glav","Eho","Uzel","Pyl","Bajt","Kisly","Myata","Nol","Flux","Moh"};
const char* MUT_A[]   = {"Dikiy","Neon","Kriv","Rja","Svet","Gamma","Pust","Top","Statik","Grib"};
const char* MUT_B[]   = {"Pes","Voron","Mimik","Dron","Slizen","Zhele","Kaban","Duh","Juk","Pank"};
const char* MOVE_TEXT[] = {"bjet","zondit","mutiruyet","ryvok"};
const char* MISSION_NAMES[] = {"TorgKatakomby","Ofis-Uley","Shkola-Lab","Stok-Kanal","Sklad-Lyudei"};

void safeCopy(char* dst, size_t n, const char* src) {
  if (!dst || n == 0) return;
  strncpy(dst, src ? src : "", n - 1);
  dst[n - 1] = 0;
}

void buildName(char* out, size_t n, const char* a, const char* b, uint32_t num) {
  snprintf(out, n, "%s-%s%02lu", a, b, (unsigned long)(num % 100));
}

Monster makeMonster(uint32_t seed, bool humanoid, uint8_t levelBias, bool deerCore = false) {
  Monster m{};
  m.seed = seed;
  m.humanoid = humanoid ? 1 : 0;
  uint32_t h = mix32(seed);
  uint8_t level = 2 + (h % 4) + levelBias;
  if (level > 99) level = 99;
  m.level = level;
  m.atk = 6 + (mix32(h ^ 0x1234) % 8) + level / 3;
  m.def = 5 + (mix32(h ^ 0x2345) % 8) + level / 4;
  m.spd = 5 + (mix32(h ^ 0x3456) % 8) + level / 5;
  m.mut = 1 + (mix32(h ^ 0x4567) % 4);
  m.maxHp = 26 + (mix32(h ^ 0x5678) % 18) + level * 3;
  m.hp = m.maxHp;
  m.classId = (uint8_t)(h % 10);

  if (deerCore) {
    safeCopy(m.name, sizeof(m.name), "DeerDroid");
    m.level = 5; m.atk = 10; m.def = 8; m.spd = 9; m.mut = 2;
    m.maxHp = 40; m.hp = m.maxHp; m.classId = 42; m.humanoid = 0;
  } else if (humanoid) {
    buildName(m.name, sizeof(m.name), HUMAN_A[h % 10], HUMAN_B[(h >> 5) % 10], h);
  } else {
    buildName(m.name, sizeof(m.name), MUT_A[h % 10], MUT_B[(h >> 4) % 10], h);
  }
  return m;
}

uint8_t strongestPartyIndex() {
  if (game.partyCount == 0) return 0;
  uint16_t best = 0; uint8_t idx = 0;
  for (uint8_t i = 0; i < game.partyCount; ++i) {
    uint16_t score = game.party[i].level * 10 + game.party[i].atk + game.party[i].def + game.party[i].spd + game.party[i].mut * 4;
    if (score >= best) { best = score; idx = i; }
  }
  return idx;
}

uint8_t weakestPartyIndexExceptZero() {
  if (game.partyCount <= 1) return 0;
  uint16_t worst = 65535; uint8_t idx = 1;
  for (uint8_t i = 1; i < game.partyCount; ++i) {
    uint16_t score = game.party[i].level * 10 + game.party[i].atk + game.party[i].def + game.party[i].spd + game.party[i].mut * 4;
    if (score <= worst) { worst = score; idx = i; }
  }
  return idx;
}

void rememberSeenSeed(uint32_t seed) {
  if (game.lastSeenCount < GAME_SEEN_RING) game.lastSeenSeeds[game.lastSeenCount++] = seed;
  else {
    for (uint8_t i = 1; i < GAME_SEEN_RING; ++i) game.lastSeenSeeds[i - 1] = game.lastSeenSeeds[i];
    game.lastSeenSeeds[GAME_SEEN_RING - 1] = seed;
  }
}

void healParty() {
  for (uint8_t i = 0; i < game.partyCount; ++i) game.party[i].hp = game.party[i].maxHp;
}

void setBanner(const char* a, const char* b) {
  safeCopy(game.banner, sizeof(game.banner), a ? a : "");
  safeCopy(game.subBanner, sizeof(game.subBanner), b ? b : "");
}

const char* superWeaponRouteText() {
  switch (superWeapon.route) {
    case ZW_SEEK: return "SEEK";
    case ZW_FLOW: return "FLOW";
    case ZW_BITE: return "BITE";
    default: return "VOID";
  }
}


float zimRamaClamp01(float x) {
  if (x < 0.0f) return 0.0f;
  if (x > 1.0f) return 1.0f;
  return x;
}

uint8_t zimRamaByte(float x) {
  x = zimRamaClamp01(x);
  return (uint8_t)(x * 255.0f + 0.5f);
}

void zimRamanujanBookLine(const char* s) {
  safeCopy(zimRama.lemma, sizeof(zimRama.lemma), s ? s : "Carr: formula bez dokaz.");
}

void zimRamanujanReset() {
  memset(&zimRama, 0, sizeof(zimRama));
  zimRama.magic = ZIM_RAMANUJAN_MAGIC;
  zimRama.version = ZIM_RAMANUJAN_VERSION;
  zimRama.tauLike = mix32(0xC4111903UL ^ (uint32_t)ESP.getEfuseMac());
  zimRama.carrIndex = 1 + (zimRama.tauLike % ZIM_RAMANUJAN_CARR_RESULTS);
  zimRama.theta3_x1000 = 1000;
  zimRama.mock_x1000 = 0;
  zimRama.partition_x1000 = 0;
  zimRama.q_x1000 = 80;
  zimRama.resonance = 32;
  zimRama.confidence = 33;
  zimRama.chapter = 1;
  zimRamanujanBookLine("Carr Synopsis: formula vedet.");
}

void zimRamanujanLoadPrefs() {
#if ZIM_RAMANUJAN_BOOK
  size_t n = prefs.getBytesLength("rama1");
  if (n == sizeof(ZimRamanujanNotebook)) {
    prefs.getBytes("rama1", &zimRama, sizeof(zimRama));
    if (zimRama.magic == ZIM_RAMANUJAN_MAGIC && zimRama.version == ZIM_RAMANUJAN_VERSION) {
      if (zimRama.carrIndex < 1 || zimRama.carrIndex > ZIM_RAMANUJAN_CARR_RESULTS) zimRama.carrIndex = 1;
      if (zimRama.confidence > 100) zimRama.confidence = 100;
      if (!zimRama.lemma[0]) zimRamanujanBookLine("Ramanujan: theta spyt v q.");
      return;
    }
  }
#endif
  zimRamanujanReset();
  zimRamanujanSavePrefs(true);
}

void zimRamanujanSavePrefs(bool force) {
#if ZIM_RAMANUJAN_BOOK
  uint32_t now = millis();
  if (!force && now - zimRama.lastSaveMs < ZIM_RAMANUJAN_SAVE_MS) return;
  zimRama.magic = ZIM_RAMANUJAN_MAGIC;
  zimRama.version = ZIM_RAMANUJAN_VERSION;
  zimRama.lastSaveMs = now;
  prefs.putBytes("rama1", &zimRama, sizeof(zimRama));
#endif
}

uint8_t zimRamanujanSignal() {
#if ZIM_RAMANUJAN_BOOK
  return zimRama.resonance;
#else
  return 0;
#endif
}

float zimRamanujanNorm() {
  return ((float)zimRamanujanSignal()) / 255.0f;
}

const char* zimRamanujanLemma() {
  return zimRama.lemma[0] ? zimRama.lemma : "Carr/Ramanujan";
}

void zimRamanujanStudyTick(const char* reason, uint16_t bits, bool shareLike) {
#if ZIM_RAMANUJAN_BOOK
  uint32_t now = millis();
  // FINAL LOCKED: theta/mock math uses powf, so every caller shares one small budget gate.
  // Boot/first study is allowed; after that, no more than one study per 450 ms.
  if (zimRama.studies > 0 && (now - zimRama.lastStudyMs) < ZIM_RAMANUJAN_MIN_GAP_MS) return;
  if (!shareLike && bits < 16 && (now - zimRama.lastStudyMs) < ZIM_RAMANUJAN_STUDY_MS) return;
  zimRama.lastStudyMs = now;
  zimRama.studies++;

  uint32_t seed = mix32(zimRama.tauLike ^ now ^ gTotalHashes ^ ((uint32_t)bits << 17) ^
                        ((uint32_t)superWeapon.charge << 9) ^ superWeapon.reverseStride ^
                        (shareLike ? 0xA17E9E37UL : 0xC4111903UL));

  float q = 0.055f + ((float)(seed & 0x03FFUL) / 1023.0f) * 0.735f;
  if (q < 0.05f) q = 0.05f;
  if (q > 0.90f) q = 0.90f;

  float theta3 = 1.0f;
  float mock = 0.0f;
  float sign = 1.0f;
  for (uint8_t k = 1; k <= ZIM_RAMANUJAN_DEPTH; ++k) {
    theta3 += 2.0f * powf(q, (float)(k * k));
    mock += sign * powf(q, (float)(k * (k + 1)) * 0.5f) / (1.0f + powf(q, (float)k));
    sign = -sign;
  }

  uint16_t n = (uint16_t)(1U + ((gBestBits + superWeapon.charge + game.rank + (seed & 0x3FUL)) & 0x7FU));
  float partSignal = sqrtf((float)n) / 12.0f;
  if (shareLike) partSignal += 0.18f;
  if (bits >= soloShareTargetBits && soloShareTargetBits > 0) partSignal += 0.10f;
  partSignal = zimRamaClamp01(partSignal);

  float thetaNorm = zimRamaClamp01((theta3 - 1.0f) / 4.0f);
  float mockNorm = zimRamaClamp01(fabsf(mock) / 2.0f);
  float bitNorm = zimRamaClamp01((float)((bits > 32) ? 32 : bits) / 32.0f);
  float qNorm = zimRamaClamp01((q - 0.05f) / 0.85f);
  float resonance = 0.34f * thetaNorm + 0.20f * mockNorm + 0.18f * partSignal + 0.16f * bitNorm + 0.12f * qNorm;
  uint8_t rByte = zimRamaByte(resonance);

  zimRama.theta3_x1000 = (uint16_t)min(65535UL, (uint32_t)(theta3 * 1000.0f + 0.5f));
  zimRama.mock_x1000 = (int16_t)(mock * 1000.0f);
  zimRama.partition_x1000 = (uint16_t)(partSignal * 1000.0f + 0.5f);
  zimRama.q_x1000 = (uint16_t)(q * 1000.0f + 0.5f);
  zimRama.resonance = (uint8_t)((((uint16_t)zimRama.resonance) * 5U + rByte) / 6U);
  zimRama.confidence = (uint8_t)min(100U, 20U + ((uint16_t)zimRama.resonance * 80U) / 255U);
  zimRama.carrIndex = 1UL + (mix32(seed ^ zimRama.studies) % ZIM_RAMANUJAN_CARR_RESULTS);
  zimRama.chapter = (uint8_t)(1U + (zimRama.carrIndex % 32U));
  zimRama.tauLike = mix32(zimRama.tauLike ^ seed ^ ((uint32_t)zimRama.resonance << 16));

  snprintf(zimRama.lemma, sizeof(zimRama.lemma), "Carr#%04lu q%u th%u mu%d",
           (unsigned long)zimRama.carrIndex,
           (unsigned)zimRama.q_x1000,
           (unsigned)zimRama.theta3_x1000,
           (int)zimRama.mock_x1000);

  superWeapon.resonance = (uint8_t)((((uint16_t)superWeapon.resonance) * 3U + zimRama.resonance) / 4U);
  superWeapon.vessel = (uint8_t)min(100U, (unsigned)(superWeapon.vessel + (zimRama.resonance > 180 ? 1U : 0U)));

  if (shareLike || (zimRama.resonance > 190 && ((zimRama.studies & 0x07UL) == 0))) {
    zimSlime.shaObsession = zimClip(zimSlime.shaObsession + 0.010f, 0.0f, 1.50f);
    zimSlime.curiosityEarth = zimClip(zimSlime.curiosityEarth + 0.006f, 0.0f, 1.50f);
    zimSlime.mood = ZM_OBSESSED;
    zimSlime.faceIndex = 2;
    zimSlimeLine(zimRama.lemma);
    zimSlimeObserve(ZE_RAMANUJAN_STUDY, shareLike ? 0.75f : 0.35f);
  }

  if ((zimRama.studies & 0x0FUL) == 1UL) {
    Serial.printf("[ZIM/RAMA] study=%lu reason=%s Carr#%lu q=%u theta3=%u mock=%d part=%u res=%u conf=%u tau=%08lx\n",
                  (unsigned long)zimRama.studies,
                  reason ? reason : "tick",
                  (unsigned long)zimRama.carrIndex,
                  (unsigned)zimRama.q_x1000,
                  (unsigned)zimRama.theta3_x1000,
                  (int)zimRama.mock_x1000,
                  (unsigned)zimRama.partition_x1000,
                  (unsigned)zimRama.resonance,
                  (unsigned)zimRama.confidence,
                  (unsigned long)zimRama.tauLike);
  }
#endif
}

void superWeaponRecomputeRoute(uint32_t pulse) {
  uint8_t cold = (uint8_t)((WiFi.status() == WL_CONNECTED) ? 55 : 25);
  uint8_t hashPressure = (uint8_t)((gHashRate / 45UL) > 100UL ? 100UL : (gHashRate / 45UL));
  uint8_t bestPressure = (uint8_t)((gBestBits > 30UL) ? 100UL : (gBestBits * 100UL / 30UL));
  uint8_t thetaPressure = zimRamanujanSignal();
  superWeapon.blackhole = (uint8_t)((superWeapon.blackhole * 7U + ((pulse >> 16) & 0xFFU) + bestPressure + (thetaPressure >> 2)) / 10U);
  superWeapon.slime = (uint8_t)((superWeapon.slime * 5U + hashPressure + cold + (thetaPressure >> 3)) / 8U);
  superWeapon.vessel = (uint8_t)((superWeapon.vessel * 6U + superWeapon.blackhole + superWeapon.slime + superWeapon.charge + (thetaPressure >> 2)) / 10U);
  superWeapon.resonance = (uint8_t)((superWeapon.resonance * 3U + (pulse & 0xFFU) + superWeapon.vessel + thetaPressure) / 6U);

  if (superWeapon.charge >= 95 && (superWeapon.vessel > 55 || thetaPressure > 190)) superWeapon.route = ZW_BITE;
  else if ((superWeapon.vessel > 65 && superWeapon.slime > 45) || thetaPressure > 170) superWeapon.route = ZW_FLOW;
  else if (superWeapon.slime > 35 || superWeapon.charge > 40 || thetaPressure > 120) superWeapon.route = ZW_SEEK;
  else superWeapon.route = ZW_VOID;

  static const uint32_t strides[] = {1UL, 3UL, 5UL, 7UL, 11UL, 17UL, 29UL, 31UL, 53UL, 97UL, 257UL, 521UL, 4099UL, 65537UL, 0x9E3779B9UL, 0xC4111903UL, 0x4F1BBCDCUL | 1UL};
  uint8_t idx = (uint8_t)((pulse ^ ((uint32_t)superWeapon.resonance << 8) ^ ((uint32_t)thetaPressure << 16) ^ zimRama.tauLike ^ game.steps ^ gBestBits) % (sizeof(strides) / sizeof(strides[0])));
  superWeapon.reverseStride = strides[idx] | 1UL;
}

void superWeaponTick(uint16_t bits, bool shareLike) {
  uint32_t pulse = mix32(gTotalHashes ^ ((uint32_t)bits << 16) ^ ((uint32_t)superWeapon.charge << 8) ^ millis() ^ game.dungeonSeed);
  uint8_t gain = 0;
  if (bits >= 16) gain += 1;
  if (bits >= 20) gain += 1;
  if (bits >= 24) gain += 2;
  if (bits >= gBestBits && bits >= 18) gain += 1;
  if (shareLike) gain += 9;
  if (zimMissionActive) gain += 1;
  if (gain > 0) {
    uint16_t next = (uint16_t)superWeapon.charge + gain;
    superWeapon.charge = (uint8_t)((next > ZIM_SUPER_WEAPON_MAX_CHARGE) ? ZIM_SUPER_WEAPON_MAX_CHARGE : next);
    superWeapon.lastPulseMs = millis();
  }
  superWeaponRecomputeRoute(pulse);
  if (gain > 0 || shareLike) zimRamanujanStudyTick(shareLike ? "share" : "bits", bits, shareLike);
}

void superWeaponOnPoolResult(bool accepted) {
  uint16_t next = (uint16_t)superWeapon.charge + (accepted ? 18 : 4);
  superWeapon.charge = (uint8_t)((next > ZIM_SUPER_WEAPON_MAX_CHARGE) ? ZIM_SUPER_WEAPON_MAX_CHARGE : next);
  superWeapon.resonance = (uint8_t)((superWeapon.resonance + (accepted ? 77 : 19)) & 0xFF);
  zimRamanujanStudyTick(accepted ? "pool_accept" : "pool_reject", (uint16_t)gBestBits, accepted);
  superWeaponRecomputeRoute(mix32(millis() ^ gTotalHashes ^ soloAccepts ^ soloRejects ^ zimRama.tauLike));
}

bool superWeaponReadyForBattle() {
  if (superWeapon.charge < ZIM_SUPER_WEAPON_FIRE_CHARGE) return false;
  if (millis() - superWeapon.lastFireMs < 4500UL) return false;
  return game.mode == GM_BATTLE;
}

uint16_t fireSuperWeapon(Monster& ally, Monster& enemy) {
  uint16_t routeBoost = 8;
  if (superWeapon.route == ZW_SEEK) routeBoost = 12;
  else if (superWeapon.route == ZW_FLOW) routeBoost = 18;
  else if (superWeapon.route == ZW_BITE) routeBoost = 26;
  uint16_t dmg = (uint16_t)(ally.atk + ally.level * 2 + ally.mut * 5 + routeBoost + (gBestBits > 32 ? 32 : gBestBits));
  if (dmg < 22) dmg = 22;
  enemy.hp = (enemy.hp > dmg) ? (enemy.hp - dmg) : 0;
  superWeapon.charge = 0;
  superWeapon.shots++;
  superWeapon.lastFireMs = millis();
  game.credits += 3;
  snprintf(game.banner, sizeof(game.banner), "SUPER ORUJIE!! MUHAHA");
  snprintf(game.subBanner, sizeof(game.subBanner), "%s -%u HP", superWeaponRouteText(), dmg);
  game.dirty = true;
  return dmg;
}

void saveSuperWeaponPrefs() {
  prefs.putUChar("swCharge", superWeapon.charge);
  prefs.putUChar("swRoute", superWeapon.route);
  prefs.putUChar("swRes", superWeapon.resonance);
  prefs.putUChar("swSlime", superWeapon.slime);
  prefs.putUChar("swVessel", superWeapon.vessel);
  prefs.putUChar("swBH", superWeapon.blackhole);
  prefs.putUInt("swShots", superWeapon.shots);
  prefs.putUInt("swStride", superWeapon.reverseStride);
}

void loadSuperWeaponPrefs() {
  superWeapon.charge = prefs.getUChar("swCharge", 7);
  superWeapon.route = prefs.getUChar("swRoute", ZW_VOID);
  superWeapon.resonance = prefs.getUChar("swRes", 0);
  superWeapon.slime = prefs.getUChar("swSlime", 0);
  superWeapon.vessel = prefs.getUChar("swVessel", 0);
  superWeapon.blackhole = prefs.getUChar("swBH", 0);
  superWeapon.shots = prefs.getUInt("swShots", 0);
  superWeapon.reverseStride = prefs.getUInt("swStride", 0x9E3779B9UL) | 1UL;
  if (superWeapon.charge > ZIM_SUPER_WEAPON_MAX_CHARGE) superWeapon.charge = ZIM_SUPER_WEAPON_MAX_CHARGE;
  if (superWeapon.route > ZW_BITE) superWeapon.route = ZW_VOID;
}

float zimFastTanh(float x) {
  if (x > 3.0f) return 0.995f;
  if (x < -3.0f) return -0.995f;
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

void zimAgentSeedWeights() {
  for (uint8_t o = 0; o < ZIM_AGENT_OUTPUTS; ++o) {
    for (uint8_t i = 0; i < ZIM_AGENT_INPUTS; ++i) {
      uint32_t h = mix32(0x5A1D0000UL ^ ((uint32_t)o << 8) ^ i ^ workerId());
      zimAgent.w[o][i] = ((int16_t)(h & 0xFF) - 128) / 512.0f;
    }
  }
  zimAgent.rewardEma = 0.0f;
  zimAgent.lossEma = 0.25f;
  zimAgent.policy = 2;       // default: Buzz lazy sabotage
  zimAgent.confidence = 42;
  zimAgent.lazyMask = ZIM_BUZZ_LAZY_SKIP_MASK;
  zimAgent.dirty = true;
}

void zimAgentLoadPrefs() {
#if ZIM_WHITE_RAVEN_AGENT
  uint8_t ok = prefs.getUChar("zaOK", 0);
  if (ok != 0xA7) {
    zimAgentSeedWeights();
    return;
  }
  for (uint8_t o = 0; o < ZIM_AGENT_OUTPUTS; ++o) {
    for (uint8_t i = 0; i < ZIM_AGENT_INPUTS; ++i) {
      char key[10];
      snprintf(key, sizeof(key), "za%u%u", o, i);
      int16_t q = prefs.getShort(key, 0);
      zimAgent.w[o][i] = ((float)q) / 4096.0f;
    }
  }
  zimAgent.epoch = prefs.getUInt("zaEpoch", 0);
  zimAgent.updates = prefs.getUInt("zaUpd", 0);
  zimAgent.rewardEma = prefs.getFloat("zaRew", 0.0f);
  zimAgent.lossEma = prefs.getFloat("zaLoss", 0.25f);
  zimAgent.policy = prefs.getUChar("zaPol", 2);
  zimAgent.confidence = prefs.getUChar("zaConf", 42);
  zimAgent.lazyMask = prefs.getUChar("zaLazy", ZIM_BUZZ_LAZY_SKIP_MASK);
  if (zimAgent.lazyMask == 0 || zimAgent.lazyMask > 0x0F) zimAgent.lazyMask = ZIM_BUZZ_LAZY_SKIP_MASK;
#else
  zimAgentSeedWeights();
#endif
}

void zimAgentSavePrefs() {
#if ZIM_WHITE_RAVEN_AGENT
  prefs.putUChar("zaOK", 0xA7);
  for (uint8_t o = 0; o < ZIM_AGENT_OUTPUTS; ++o) {
    for (uint8_t i = 0; i < ZIM_AGENT_INPUTS; ++i) {
      char key[10];
      snprintf(key, sizeof(key), "za%u%u", o, i);
      int16_t q = (int16_t)(zimAgent.w[o][i] * 4096.0f);
      prefs.putShort(key, q);
    }
  }
  prefs.putUInt("zaEpoch", zimAgent.epoch);
  prefs.putUInt("zaUpd", zimAgent.updates);
  prefs.putFloat("zaRew", zimAgent.rewardEma);
  prefs.putFloat("zaLoss", zimAgent.lossEma);
  prefs.putUChar("zaPol", zimAgent.policy);
  prefs.putUChar("zaConf", zimAgent.confidence);
  prefs.putUChar("zaLazy", zimAgent.lazyMask);
  zimAgent.dirty = false;
#endif
}

void zimAgentFeatures(float x[ZIM_AGENT_INPUTS]) {
  float rama = zimRamanujanNorm();
  float theta3 = zimClip(((float)zimRama.theta3_x1000) / 5000.0f, 0.0f, 1.0f);
  float qsig = zimClip(((float)zimRama.q_x1000) / 1000.0f, 0.0f, 1.0f);
  x[0] = 1.0f;
  x[1] = (float)((gHashRate > 40000UL) ? 40000UL : gHashRate) / 40000.0f;
  x[2] = 0.72f * ((float)((gBestBits > 32UL) ? 32UL : gBestBits) / 32.0f) + 0.28f * rama;
  x[3] = 0.76f * ((float)superWeapon.charge / 100.0f) + 0.24f * theta3;
  x[4] = (float)((uint8_t)game.mode) / 12.0f;
  x[5] = (float)((game.credits > 255UL) ? 255UL : game.credits) / 255.0f;
  x[6] = (float)((gBuzzJobsAccepted > 31UL) ? 31UL : gBuzzJobsAccepted) / 31.0f;
  x[7] = (float)((soloAccepts + gBuzzSharesSent > 15UL) ? 15UL : (soloAccepts + gBuzzSharesSent)) / 15.0f;
  x[8] = 0.70f * zimClip(zimSlime.shaObsession, 0.0f, 1.0f) + 0.30f * rama;
  x[9] = 0.86f * zimClip(zimSlime.suspicionHumans, 0.0f, 1.0f) + 0.14f * qsig;
  x[10] = 0.84f * zimClip(zimSlime.ego, 0.0f, 1.0f) + 0.16f * rama;
  x[11] = 0.75f * zimClip(zimSlime.trustBuzz, 0.0f, 1.0f) + 0.25f * zimClip(((float)zimRama.confidence) / 100.0f, 0.0f, 1.0f);
}

void zimAgentForward(const float x[ZIM_AGENT_INPUTS]) {
  for (uint8_t o = 0; o < ZIM_AGENT_OUTPUTS; ++o) {
    float z = 0.0f;
    for (uint8_t i = 0; i < ZIM_AGENT_INPUTS; ++i) z += zimAgent.w[o][i] * x[i];
    zimAgent.y[o] = zimFastTanh(z);
  }
}

void zimAgentApplyPolicy() {
#if ZIM_WHITE_RAVEN_AGENT
  uint8_t best = 0;
  for (uint8_t o = 1; o < ZIM_AGENT_OUTPUTS; ++o) if (zimAgent.y[o] > zimAgent.y[best]) best = o;
  zimAgent.policy = best;
  float conf = (zimAgent.y[best] + 1.0f) * 50.0f;
  if (conf < 5.0f) conf = 5.0f;
  if (conf > 99.0f) conf = 99.0f;
  zimAgent.confidence = (uint8_t)conf;

  // Policy is deliberately crooked. MicroGPTSlime never becomes a normal worker.
  // 0: VOID = hide in the house, 1: SOLO = chase BTC, 2: BUZZ_LAZY = careless chores,
  // 3: SUPER = weapon obsession, 4: STUDY = spy on Earth/humans.
  if (best == 0) zimAgent.lazyMask = 0x07;       // 7/8 skips
  else if (best == 1) zimAgent.lazyMask = 0x03;  // 3/4 skips
  else if (best == 2) zimAgent.lazyMask = 0x01;  // 1/2 skips
  else if (best == 4) zimAgent.lazyMask = 0x05;  // weird scout rhythm
  else zimAgent.lazyMask = 0x03;                 // super mode keeps solo priority

  if (best == 3 && superWeapon.charge < 100) {
    superWeapon.charge = (uint8_t)min(100U, (unsigned)superWeapon.charge + 1U);
  }
  if (best == 4) {
    zimSlime.curiosityEarth = zimClip(zimSlime.curiosityEarth + 0.003f, 0.0f, 1.50f);
    zimSlime.secrecy = zimClip(zimSlime.secrecy + 0.002f, 0.0f, 1.50f);
  }
#endif
}

void zimAgentTick(const char* reason, float extReward) {
#if ZIM_WHITE_RAVEN_AGENT
  float x[ZIM_AGENT_INPUTS];
  zimAgentFeatures(x);
  zimAgentForward(x);

  float reward = extReward;
  bool newSoloAccept = (soloAccepts > zimAgent.lastSoloAccept);
  bool newBuzzShare = (gBuzzSharesSent > zimAgent.lastBuzzShare);
  if (newSoloAccept) { reward += 1.20f; zimSlimeObserve(ZE_SOLO_ACCEPT, 1.0f); }
  if (newBuzzShare) { reward += 0.80f; zimSlimeObserve(ZE_BUZZ_SHARE, 0.8f); }
  if (gBestBits > zimAgent.lastBestBits) reward += 0.03f * (float)(gBestBits - zimAgent.lastBestBits);
  if (gBuzzJobsAccepted > 0 && gBuzzLazySkips > gBuzzSharesSent) reward += 0.02f; // yes, it learns glorious procrastination
  if (superWeapon.charge >= 100) { reward += 0.10f; if ((zimSlime.ticks & 0x0FUL) == 0) zimSlimeObserve(ZE_SUPER_READY, 0.6f); }
  reward += 0.055f * zimRamanujanNorm();
  if (soloRejects + gBuzzSharesFail > 0) reward -= 0.03f;
  if (reward > 2.0f) reward = 2.0f;
  if (reward < -1.0f) reward = -1.0f;

  uint8_t targetPolicy = 1;
  if (gBuzzJobsAccepted && ((millis() / 7000UL) & 1UL)) targetPolicy = 2;
  if (superWeapon.charge > 85 || superWeapon.route == ZW_BITE) targetPolicy = 3;
  if (zimSlime.curiosityEarth > 0.88f && ((zimSlime.rng >> 4) & 1U)) targetPolicy = 4;
  if (zimRamanujanSignal() > 205 && ((zimRama.studies + zimAgent.epoch) & 1U)) targetPolicy = 4;
  if (reward < -0.2f) targetPolicy = 0;

  float target[ZIM_AGENT_OUTPUTS] = {-0.25f, -0.20f, -0.10f, -0.10f, -0.05f};
  target[targetPolicy] = 0.85f;
  float loss = 0.0f;
  for (uint8_t o = 0; o < ZIM_AGENT_OUTPUTS; ++o) {
    float err = target[o] + reward * 0.15f - zimAgent.y[o];
    loss += err * err;
    for (uint8_t i = 0; i < ZIM_AGENT_INPUTS; ++i) {
      zimAgent.w[o][i] += ZIM_AGENT_LR * err * x[i];
      if (zimAgent.w[o][i] > 1.75f) zimAgent.w[o][i] = 1.75f;
      if (zimAgent.w[o][i] < -1.75f) zimAgent.w[o][i] = -1.75f;
    }
  }
  zimAgent.rewardEma = zimAgent.rewardEma * 0.92f + reward * 0.08f;
  zimAgent.lossEma = zimAgent.lossEma * 0.92f + loss * 0.08f;
  zimAgent.epoch++;
  zimAgent.updates++;
  zimAgent.lastSoloAccept = soloAccepts;
  zimAgent.lastBuzzShare = gBuzzSharesSent;
  zimAgent.lastBestBits = gBestBits;
  zimAgent.dirty = true;
  zimSlimeObserve(ZE_TICK, 0.15f);
  zimAgentForward(x);
  zimAgentApplyPolicy();

  if ((zimAgent.updates & 0x1FUL) == 1UL) {
    Serial.printf("[ZIM/AI] micrograd reason=%s ep=%lu pol=%u conf=%u reward=%.3f loss=%.3f lazyMask=0x%02x\n",
                  reason ? reason : "tick", (unsigned long)zimAgent.epoch, (unsigned)zimAgent.policy,
                  (unsigned)zimAgent.confidence, zimAgent.rewardEma, zimAgent.lossEma, (unsigned)zimAgent.lazyMask);
  }
#endif
}

void sendZimAgentMemory() {
#if ZIM_WHITE_RAVEN_AGENT
  ZimAgentMemoryPacket za{};
  za.magic[0] = 'Z'; za.magic[1] = 'A';
  za.version = 2;
  za.worker_id = workerId();
  strlcpy(za.nodeId, JANUS_NODE_ID, sizeof(za.nodeId));
  strlcpy(za.kind, "zim_slime_ai", sizeof(za.kind));
  za.seq = ++gSeq;
  za.uptime_ms = millis();
  za.epoch = zimAgent.epoch;
  za.updates = zimAgent.updates;
  za.accepts = soloAccepts;
  za.buzzShares = gBuzzSharesSent;
  za.reward_x1000 = (uint16_t)max(0, min(65535, (int)(zimAgent.rewardEma * 1000.0f + 32768.0f)));
  za.loss_x1000 = (uint16_t)max(0, min(65535, (int)(zimAgent.lossEma * 1000.0f)));
  za.policy = zimAgent.policy;
  za.confidence = zimAgent.confidence;
  za.lazyMask = zimAgent.lazyMask;
  za.route = superWeapon.route;
  za.weaponCharge = superWeapon.charge;
  za.flags = 0x01; // white raven / MicroGPTSlime online
  if (gJobActive) za.flags |= 0x02;
  if (soloJobReady) za.flags |= 0x04;
  if (superWeapon.charge >= 100) za.flags |= 0x08;
  if (zimRamanujanSignal() > 180) za.flags |= 0x10;
  for (uint8_t o = 0; o < ZIM_AGENT_OUTPUTS; ++o) {
    for (uint8_t i = 0; i < ZIM_AGENT_INPUTS; ++i) {
      int v = (int)(zimAgent.w[o][i] * 64.0f);
      if (v > 127) v = 127;
      if (v < -128) v = -128;
      za.weights_q7[o * ZIM_AGENT_INPUTS + i] = (int8_t)v;
    }
  }
  za.slimeMood = zimSlime.mood;
  za.slimeFace = zimSlime.faceIndex;
  za.shaObsession = zimF2B(zimSlime.shaObsession);
  za.btcHunger = zimF2B(zimSlime.btcHunger);
  za.curiosity = zimF2B(zimSlime.curiosityEarth);
  za.suspicion = zimF2B(zimSlime.suspicionHumans);
  za.ego = zimF2B(zimSlime.ego);
  za.shame = zimF2B(zimSlime.shame);
  za.trustBuzz = zimF2B(zimSlime.trustBuzz);
  za.trustSwarm = zimF2B(zimSlime.trustSwarm);
  za.slimeTicks = zimSlime.ticks;
  strlcpy(za.thought, zimSlime.thought, sizeof(za.thought));
  esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)&za, sizeof(za));
#endif
}

void zimAgentMaybePostNas() {
#if ZIM_WHITE_RAVEN_AGENT && ZIM_NAS_MEMORY_HTTP
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(ZIM_NAS_MEMORY_URL);
  http.addHeader("Content-Type", "application/json");
  StaticJsonDocument<1024> doc;
  doc["node"] = JANUS_NODE_ID;
  doc["kind"] = "zim_slime_ai";
  doc["epoch"] = zimAgent.epoch;
  doc["updates"] = zimAgent.updates;
  doc["policy"] = zimAgent.policy;
  doc["confidence"] = zimAgent.confidence;
  doc["lazyMask"] = zimAgent.lazyMask;
  doc["reward"] = zimAgent.rewardEma;
  doc["loss"] = zimAgent.lossEma;
  doc["hashRate"] = gHashRate;
  doc["bestBits"] = gBestBits;
  doc["soloAccepts"] = soloAccepts;
  doc["buzzShares"] = gBuzzSharesSent;
  doc["weapon"] = superWeapon.charge;
  doc["route"] = superWeaponRouteText();
  doc["slimeMood"] = zimSlimeMoodText();
  doc["slimeFace"] = zimSlimeFace();
  doc["thought"] = zimSlime.thought;
  doc["shaObsession"] = zimSlime.shaObsession;
  doc["btcHunger"] = zimSlime.btcHunger;
  doc["curiosityEarth"] = zimSlime.curiosityEarth;
  doc["suspicionHumans"] = zimSlime.suspicionHumans;
  doc["carrIndex"] = zimRama.carrIndex;
  doc["theta3_x1000"] = zimRama.theta3_x1000;
  doc["mock_x1000"] = zimRama.mock_x1000;
  doc["thetaRes"] = zimRama.resonance;
  doc["ramaLemma"] = zimRama.lemma;
  char body[1024];
  serializeJson(doc, body, sizeof(body));
  int code = http.POST((uint8_t*)body, strlen(body));
  http.end();
  if (code > 0) Serial.printf("[ZIM/AI] NAS memory POST code=%d\n", code);
#endif
}


float zimClip(float x, float lo, float hi) {
  if (x < lo) return lo;
  if (x > hi) return hi;
  return x;
}

uint8_t zimF2B(float x) {
  x = zimClip(x, 0.0f, 1.0f);
  return (uint8_t)(x * 255.0f + 0.5f);
}

const char* zimSlimeFace() {
  static const char* faces[6] = {"o_o", "O_o", "@_@", ">_<", "-_~", "^_^"};
  return faces[zimSlime.faceIndex % 6];
}

const char* zimSlimeMoodText() {
  switch (zimSlime.mood % 6) {
    case ZM_SCOUT: return "RAZVED";
    case ZM_OBSESSED: return "SHA-BRED";
    case ZM_OFFENDED: return "OBIDA";
    case ZM_TRICKSTER: return "HITROST";
    case ZM_TRIUMPH: return "TRIUMF";
    default: return "SMUTA";
  }
}

void zimSlimeLine(const char* s) {
  safeCopy(zimSlime.thought, sizeof(zimSlime.thought), s ? s : "...");
}

void zimSlimeRemember(uint8_t event, float intensity) {
  uint8_t i = zimSlime.episodeHead % ZIM_SLIME_EPISODES;
  zimSlime.episodes[i].t = millis();
  zimSlime.episodes[i].event = event;
  zimSlime.episodes[i].mood = zimSlime.mood;
  zimSlime.episodes[i].policy = zimAgent.policy;
  zimSlime.episodes[i].intensity = zimF2B(intensity);
  zimSlime.episodes[i].shaBits = (uint16_t)((gBestBits > 65535UL) ? 65535UL : gBestBits);
  uint16_t flags = 0;
  if (soloPoolConnected) flags |= 0x01;
  if (soloAuthorized) flags |= 0x02;
  if (gJobActive) flags |= 0x04;
  if (superWeapon.charge >= 100) flags |= 0x08;
  if (zimAgent.policy == 2) flags |= 0x10;
  zimSlime.episodes[i].flags = flags;
  zimSlime.episodeHead = (uint8_t)((zimSlime.episodeHead + 1) % ZIM_SLIME_EPISODES);
}

void zimSlimeReset() {
  memset(&zimSlime, 0, sizeof(zimSlime));
  zimSlime.magic = ZIM_SLIME_MAGIC;
  zimSlime.version = ZIM_SLIME_VERSION;
  zimSlime.rng = 0x5A1A51A1UL ^ (uint32_t)ESP.getEfuseMac();
  zimSlime.trustBuzz = 0.33f;
  zimSlime.trustPool = 0.58f;
  zimSlime.trustSwarm = 0.62f;
  zimSlime.suspicionHumans = 0.71f;
  zimSlime.curiosityEarth = 0.84f;
  zimSlime.shaObsession = 0.52f;
  zimSlime.btcHunger = 0.42f;
  zimSlime.ego = 0.91f;
  zimSlime.shame = 0.18f;
  zimSlime.secrecy = 0.67f;
  zimSlime.comfort = 0.47f;
  zimSlime.noveltyAvg = 0.50f;
  zimSlime.dangerAvg = 0.20f;
  zimSlime.predictionError = 0.35f;
  zimSlime.mapConfidence = 0.22f;
  zimSlime.mood = ZM_SCOUT;
  zimSlime.faceIndex = 1;
  zimSlime.lastPolicy = 1;
  zimSlimeLine("Sekret Zemlyan: SHA? BTC?");
}

void zimSlimeLoadPrefs() {
  size_t n = prefs.getBytesLength("zslime3");
  if (n == sizeof(ZimSlimeBrain)) {
    prefs.getBytes("zslime3", &zimSlime, sizeof(zimSlime));
    if (zimSlime.magic == ZIM_SLIME_MAGIC && zimSlime.version == ZIM_SLIME_VERSION) {
      zimSlime.trustBuzz = zimClip(zimSlime.trustBuzz, 0.02f, 1.50f);
      zimSlime.trustPool = zimClip(zimSlime.trustPool, 0.02f, 1.50f);
      zimSlime.trustSwarm = zimClip(zimSlime.trustSwarm, 0.02f, 1.50f);
      zimSlime.suspicionHumans = zimClip(zimSlime.suspicionHumans, 0.0f, 1.50f);
      zimSlime.curiosityEarth = zimClip(zimSlime.curiosityEarth, 0.0f, 1.50f);
      zimSlime.shaObsession = zimClip(zimSlime.shaObsession, 0.0f, 1.50f);
      zimSlime.btcHunger = zimClip(zimSlime.btcHunger, 0.0f, 1.50f);
      zimSlime.ego = zimClip(zimSlime.ego, 0.0f, 1.50f);
      zimSlime.faceIndex %= 6;
      zimSlime.mood %= 6;
      if (!zimSlime.thought[0]) zimSlimeLine("Pamyat vernulas. Ya byl prav.");
      return;
    }
  }
  zimSlimeReset();
  zimSlimeSavePrefs(true);
}

void zimSlimeSavePrefs(bool force) {
  if (!force && millis() - lastSlimeSaveMs < 20000UL) return;
  zimSlime.magic = ZIM_SLIME_MAGIC;
  zimSlime.version = ZIM_SLIME_VERSION;
  zimSlime.lastSaveMs = millis();
  prefs.putBytes("zslime3", &zimSlime, sizeof(zimSlime));
  lastSlimeSaveMs = millis();
}

void zimSlimeObserve(uint8_t event, float intensity) {
  intensity = zimClip(intensity, 0.0f, 1.0f);
  zimSlime.ticks++;
  zimSlime.rng = mix32(zimSlime.rng ^ millis() ^ ((uint32_t)event << 24) ^ gTotalHashes ^ gBestBits);
  float novelty = 0.08f + intensity * 0.28f + ((zimSlime.rng & 0xFF) / 255.0f) * 0.08f;
  float danger = 0.04f;
  switch (event) {
    case ZE_SOLO_ACCEPT:
      zimSlime.btcHunger = zimClip(zimSlime.btcHunger + 0.070f, 0.0f, 1.50f);
      zimSlime.shaObsession = zimClip(zimSlime.shaObsession + 0.055f, 0.0f, 1.50f);
      zimSlime.ego = zimClip(zimSlime.ego + 0.060f, 0.0f, 1.50f);
      zimSlime.trustPool = zimClip(zimSlime.trustPool + 0.025f, 0.02f, 1.50f);
      zimSlime.mood = ZM_TRIUMPH; zimSlime.faceIndex = 5;
      zimSlimeLine("BTC sled nayden. Ya geniy.");
      break;
    case ZE_BUZZ_JOB:
      zimSlime.trustBuzz = zimClip(zimSlime.trustBuzz - 0.018f, 0.02f, 1.50f);
      zimSlime.shame = zimClip(zimSlime.shame + 0.014f, 0.0f, 1.20f);
      zimSlime.secrecy = zimClip(zimSlime.secrecy + 0.018f, 0.0f, 1.50f);
      zimSlime.mood = ZM_OFFENDED; zimSlime.faceIndex = 3;
      zimSlimeLine("Buzz snova prines musor-rabotu.");
      break;
    case ZE_BUZZ_SHARE:
      zimSlime.trustBuzz = zimClip(zimSlime.trustBuzz + 0.015f, 0.02f, 1.50f);
      zimSlime.ego = zimClip(zimSlime.ego + 0.025f, 0.0f, 1.50f);
      zimSlime.mood = ZM_TRICKSTER; zimSlime.faceIndex = 4;
      zimSlimeLine("Halatnost dala rezultat. Tak i plan.");
      break;
    case ZE_SUPER_READY:
      zimSlime.shaObsession = zimClip(zimSlime.shaObsession + 0.030f, 0.0f, 1.50f);
      zimSlime.suspicionHumans = zimClip(zimSlime.suspicionHumans + 0.020f, 0.0f, 1.50f);
      zimSlime.mood = ZM_OBSESSED; zimSlime.faceIndex = 2;
      zimSlimeLine("SHA otkryvaet super-orujie.");
      break;
    case ZE_BATTLE:
      zimSlime.curiosityEarth = zimClip(zimSlime.curiosityEarth + 0.025f, 0.0f, 1.50f);
      danger += 0.18f;
      zimSlime.mood = ZM_SCOUT; zimSlime.faceIndex = 1;
      zimSlimeLine("Zemlyane hranjat sekrety v podpole.");
      break;
    case ZE_CORE2_SPAM:
      zimSlime.trustSwarm = zimClip(zimSlime.trustSwarm - 0.010f, 0.02f, 1.50f);
      zimSlime.ego = zimClip(zimSlime.ego + 0.018f, 0.0f, 1.50f);
      zimSlime.mood = ZM_TRICKSTER;
      zimSlimeLine("Galaktika dumayet ya durak. Ha.");
      break;
    default:
      if ((zimSlime.rng & 0x07) == 0) zimSlimeLine("Nachalstvo skazalo: najdi bitok.");
      break;
  }
  float h = zimClip((float)gBestBits / 30.0f, 0.0f, 1.0f);
  zimSlime.mapConfidence = zimSlime.mapConfidence * 0.96f + h * 0.04f;
  zimSlime.noveltyAvg = zimSlime.noveltyAvg * 0.92f + novelty * 0.08f;
  zimSlime.dangerAvg = zimSlime.dangerAvg * 0.92f + danger * 0.08f;
  zimSlime.comfort = zimSlime.comfort * 0.96f + (soloPoolConnected ? 0.55f : 0.25f) * 0.04f;
  zimSlime.predictionError = zimClip(fabsf(zimSlime.noveltyAvg - zimSlime.mapConfidence), 0.0f, 1.0f);
  zimSlime.lastPolicy = zimAgent.policy;
  zimSlimeRemember(event, intensity);
}

void levelUpMonster(Monster& m, uint8_t ups) {
  for (uint8_t i = 0; i < ups; ++i) {
    if (m.level < 99) m.level++;
    m.maxHp += 4 + (m.level % 3);
    m.hp = m.maxHp;
    m.atk += 1 + (m.level % 2 == 0);
    m.def += 1;
    if ((m.level % 2) == 1) m.spd += 1;
    if ((m.level % 5) == 0 && m.mut < 9) m.mut++;
  }
}

void mutateFromSample(Monster& carrier, const Monster& sample) {
  carrier.atk += (sample.atk / 4) + 1;
  carrier.def += (sample.def / 5) + 1;
  carrier.spd += (sample.spd / 6) + 1;
  carrier.maxHp += 5 + (sample.level / 2);
  carrier.hp = carrier.maxHp;
  if (carrier.mut < 9) carrier.mut++;
  if (carrier.level < 99) carrier.level += 1;
}

void captureEnemyToCollection() {
  game.captured++;
  rememberSeenSeed(game.enemy.seed);
  uint8_t leader = strongestPartyIndex();
  if (game.partyCount < GAME_PARTY_MAX) {
    Monster n = game.enemy;
    n.humanoid = 0;
    uint32_t h = mix32(game.enemy.seed ^ game.captured ^ game.steps);
    buildName(n.name, sizeof(n.name), MUT_A[h % 10], MUT_B[(h >> 5) % 10], h);
    n.hp = n.maxHp;
    if (n.level < 99) n.level++;
    game.party[game.partyCount++] = n;
    setBanner("Obrazec prinyat", n.name);
  } else {
    uint8_t w = weakestPartyIndexExceptZero();
    mutateFromSample(game.party[w], game.enemy);
    setBanner("Privivka v laboratorii", game.party[w].name);
  }
  mutateFromSample(game.party[leader], game.enemy);
  game.credits += 12 + game.enemy.level;
  game.dirty = true;
}

// ============================================================
// GAME SAVE / LOAD
// ============================================================
void saveGame() {
  SaveBlob s{};
  s.magic = 0x5A494D33UL;
  s.townSeed = game.townSeed;
  s.houseSeed = game.houseSeed;
  s.dungeonSeed = game.dungeonSeed;
  s.seen = game.seen;
  s.captured = game.captured;
  s.credits = game.credits;
  s.steps = game.steps;
  s.battleWins = game.battleWins;
  s.labVisits = game.labVisits;
  s.rngCounter = game.rngCounter;
  s.missions = game.missions;
  s.missionSuccess = game.missionSuccess;
  s.missionFails = game.missionFails;
  s.loot = game.loot;
  s.traps = game.traps;
  s.fuel = game.fuel;
  s.floorLevel = game.floorLevel;
  s.floorMax = game.floorMax;
  s.missionId = game.missionId;
  s.housePhase = game.housePhase;
  memcpy(s.lastSeenSeeds, game.lastSeenSeeds, sizeof(s.lastSeenSeeds));
  s.lastSeenCount = game.lastSeenCount;
  s.partyCount = game.partyCount;
  s.px = game.px;
  s.py = game.py;
  s.rank = game.rank;
  memcpy(s.party, game.party, sizeof(s.party));
  prefs.putBytes("save31", &s, sizeof(s));
  saveSuperWeaponPrefs();
  zimAgentSavePrefs();
  game.lastSaveMs = millis();
  game.dirty = false;
}

void newGame() {
  memset(&game, 0, sizeof(game));
  game.mode = GM_HOME;
  game.townSeed = esp_random() ^ 0x51A0C0DEUL;
  game.houseSeed = mix32(game.townSeed ^ 0xA11EABCDUL);
  game.dungeonSeed = mix32(game.houseSeed ^ 0xD00DFEEDUL);
  game.partyCount = 1;
  game.party[0] = makeMonster(game.townSeed ^ 0xD33F001DUL, false, 0, true);
  game.px = 5; game.py = 10; game.tx = 24; game.ty = 10;
  game.credits = 18;
  game.fuel = 20;
  game.rank = 1;
  game.floorMax = 3;
  healParty();
  setBanner("Dom-baza Zima v seti", "DeerDroid sterezhet garaj");
  game.lastSaveMs = millis();
  game.stateStartMs = millis();
  game.lastMoveMs = millis();
}

void loadGame() {
  prefs.begin("zimgeek", false);
  SaveBlob s{};
  size_t got = prefs.getBytes("save31", &s, sizeof(s));
  if (got == sizeof(s) && s.magic == 0x5A494D33UL) {
    memset(&game, 0, sizeof(game));
    game.mode = GM_HOME;
    game.townSeed = s.townSeed;
    game.houseSeed = s.houseSeed;
    game.dungeonSeed = s.dungeonSeed;
    game.seen = s.seen;
    game.captured = s.captured;
    game.credits = s.credits;
    game.steps = s.steps;
    game.battleWins = s.battleWins;
    game.labVisits = s.labVisits;
    game.rngCounter = s.rngCounter;
    game.missions = s.missions;
    game.missionSuccess = s.missionSuccess;
    game.missionFails = s.missionFails;
    game.loot = s.loot;
    game.traps = s.traps;
    game.fuel = s.fuel ? s.fuel : 20;
    game.floorLevel = s.floorLevel;
    game.floorMax = s.floorMax ? s.floorMax : 3;
    game.missionId = s.missionId;
    game.housePhase = s.housePhase;
    memcpy(game.lastSeenSeeds, s.lastSeenSeeds, sizeof(s.lastSeenSeeds));
    game.lastSeenCount = s.lastSeenCount;
    game.partyCount = s.partyCount;
    if (game.partyCount == 0 || game.partyCount > GAME_PARTY_MAX) game.partyCount = 1;
    memcpy(game.party, s.party, sizeof(s.party));
    game.px = 5; game.py = 10; game.tx = 24; game.ty = 10;
    game.rank = s.rank ? s.rank : 1;
    setBanner("Zim vernulsya domoy", game.party[0].name);
    game.stateStartMs = millis();
    game.lastMoveMs = millis();
    game.lastSaveMs = millis();
  } else {
    newGame();
    saveGame();
  }
  loadSuperWeaponPrefs();
}

// ============================================================
// HOUSE BASE + PROCEDURAL DUNGEON
// ============================================================
uint8_t dungeonTileAt(int x, int y) {
  if (x < 0 || y < 0 || x >= GAME_MAP_W || y >= GAME_MAP_H) return 1;
  if (y == 0 || y == GAME_MAP_H - 1 || x == 0 || x == GAME_MAP_W - 1) return 1;
  // mission corridors: broad enough for tiny LCD, deterministic enough to look like a PS1-era floor map
  if (y == 3 || y == 7 || y == 11 || x == 4 || x == 14 || x == 24) {
    uint32_t r = mix32(game.dungeonSeed ^ ((uint32_t)x * 1103515245UL) ^ ((uint32_t)y * 2654435761UL) ^ game.floorLevel);
    uint8_t rr = (uint8_t)(r & 0x3F);
    uint8_t mt = (uint8_t)(game.missionId % 5);
    uint8_t trapLim = 4;
    uint8_t signalLim = 10;
    if (mt == 0) { trapLim = 2; signalLim = 16; }       // RAZVED: more signals, fewer traps
    else if (mt == 1) { trapLim = 3; signalLim = 15; }  // OBRAZ: hunt specimens
    else if (mt == 2) { trapLim = 9; signalLim = 14; }  // NALOT: danger, combat, reward
    else if (mt == 3) { trapLim = 6; signalLim = 9; }   // SBOR: loot, lower signal
    else { trapLim = 4; signalLim = 11; }               // UDERJ: balanced
    if (rr < trapLim && x > 3 && y > 1) return 3;
    if (rr >= trapLim && rr < signalLim) return 4;
    bool lootHit = ((r & 0x7F) == 44);
    if (mt == 3 && ((r & 0x1F) < 7)) lootHit = true;
    if (mt == 2 && ((r & 0x7F) == 22)) lootHit = true;
    if (lootHit) return 5;
    return 0;
  }
  uint32_t n = mix32((uint32_t)(x * 73856093UL) ^ (uint32_t)(y * 19349663UL) ^ game.dungeonSeed ^ ((uint32_t)game.floorLevel << 16));
  if ((n & 0x1F) < 7) return 0;
  return 1;
}

bool tileWalkable(uint8_t t) { return t != 1; }
bool tileIsTrap(uint8_t t) { return t == 3; }
bool tileIsSignal(uint8_t t) { return t == 4; }
bool tileIsLoot(uint8_t t) { return t == 5; }

void chooseDungeonTarget() {
  if (game.floorLevel >= game.floorMax) { game.tx = 27; game.ty = 11; return; }
  for (uint8_t tries = 0; tries < 40; ++tries) {
    game.rngCounter++;
    int tx = 2 + (mix32(game.rngCounter ^ game.steps ^ game.dungeonSeed) % (GAME_MAP_W - 4));
    int ty = 2 + (mix32(game.rngCounter ^ game.credits ^ game.floorLevel) % (GAME_MAP_H - 4));
    if (tileWalkable(dungeonTileAt(tx, ty))) { game.tx = tx; game.ty = ty; return; }
  }
  game.tx = 24; game.ty = 7;
}

Monster generateEncounterMonster() {
  uint32_t seed = mix32(game.dungeonSeed ^ game.steps ^ game.rngCounter ^ ((uint32_t)game.floorLevel << 24) ^ (millis() << 1));
  bool humanoid = ((seed >> 3) & 1) == 0;
  uint8_t bias = game.rank + game.floorLevel + (game.captured / 4);
  uint8_t mt = (uint8_t)(game.missionId % 5);
  if (mt == 2) bias += 2;          // NALOT
  else if (mt == 3) bias += 1;     // SBOR
  else if (mt == 0 && bias > 0) bias -= 1; // RAZVED
  if (bias > 22) bias = 22;
  return makeMonster(seed, humanoid, bias);
}

const char* currentMissionName() {
  return MISSION_NAMES[game.missionId % 5];
}

void clearZimMissionStim(const char* why = nullptr) {
  (void)why;
  if (!zimMissionStimActive || zimMissionStimHpBonus == 0 || game.partyCount == 0) {
    zimMissionStimActive = false;
    zimMissionStimHpBonus = 0;
    return;
  }
  if (game.party[0].maxHp > zimMissionStimHpBonus + 8) game.party[0].maxHp -= zimMissionStimHpBonus;
  else game.party[0].maxHp = 8;
  if (game.party[0].hp > game.party[0].maxHp) game.party[0].hp = game.party[0].maxHp;
  zimMissionStimActive = false;
  zimMissionStimHpBonus = 0;
}

void enterHome(const char* why) {
  zimMissionActive = false;
  game.mode = GM_HOME;
  game.stateStartMs = millis();
  game.px = 5; game.py = 10; game.tx = 24; game.ty = 10;
  clearZimMissionStim("return home");
  healParty();
  if (game.fuel < 20) game.fuel = 20;
  setBanner("Zim snova doma", why ? why : "Imperialnaya baza maskirovana plokho");
  saveGame();
}

void enterLab(const char* why) {
  game.mode = GM_LAB;
  game.stateStartMs = millis();
  game.labVisits++;
  uint8_t leader = strongestPartyIndex();
  healParty();
  if (game.captured > 0 && (game.captured % 3 == 0)) {
    levelUpMonster(game.party[leader], 1 + (game.captured % 2));
    game.credits += 5;
  }
  game.rank = 1 + (uint8_t)(((game.captured / 5) < 9UL) ? (game.captured / 5) : 9UL);
  setBanner("Dom-lab mutiruyet otryad", why ? why : game.party[leader].name);
  game.dirty = true;
}

void startMissionFromCore2() {
  ZimMissionPacket zm{};
  memcpy(&zm, &zimCurrentMission, sizeof(zm));
  zimMissionPending = false;
  zimMissionActive = true;
  zimLastMissionMs = millis();

  game.mode = GM_MISSION;
  game.stateStartMs = millis();
  game.missions++;
  game.missionId = (uint8_t)(zm.missionType % 5);
  game.floorLevel = 1;
  game.floorMax = zm.floorMax;
  if (game.floorMax < 2) game.floorMax = 2;
  if (game.floorMax > 5) game.floorMax = 5;
  game.dungeonSeed = zm.seed ? zm.seed : mix32(game.houseSeed ^ zm.mission_id ^ esp_random());
  game.fuel = zm.fuel ? zm.fuel : (18 + game.rank * 2);
  if (game.fuel < 12) game.fuel = 12;
  if (game.fuel > 60) game.fuel = 60;
  uint8_t oldRank = game.rank;
  uint8_t suggestedRank = (uint8_t)(zm.difficulty_x100 / 100U);
  if (suggestedRank > game.rank && suggestedRank < 20) game.rank = suggestedRank;
  if (game.partyCount > 0 && suggestedRank > oldRank) {
    clearZimMissionStim("new stim");
    uint16_t stim = (uint16_t)(4 + (suggestedRank - oldRank) * 3);
    if (stim > 32) stim = 32;
    if (game.party[0].maxHp < 220) {
      uint16_t room = (uint16_t)(220 - game.party[0].maxHp);
      if (stim > room) stim = room;
      game.party[0].maxHp += stim;
      zimMissionStimHpBonus = stim;
      zimMissionStimActive = (stim > 0);
    }
    game.party[0].hp = game.party[0].maxHp;
  }
  strlcpy(zimPlanetLine, zm.planet[0] ? zm.planet : "Zemlya-Zim", sizeof(zimPlanetLine));
  strlcpy(zimOrderLine, zm.order[0] ? zm.order : zimMissionTypeName(zm.missionType), sizeof(zimOrderLine));
  snprintf(zimMissionLine, sizeof(zimMissionLine), "Core2 -> %s", zimPlanetLine);
  setBanner("Prikaz Core2 zapushen", zimOrderLine);
  game.dirty = true;
}

void startMission() {
  game.mode = GM_MISSION;
  game.stateStartMs = millis();
  game.missions++;
  game.missionId = (uint8_t)(mix32(game.houseSeed ^ game.missions ^ game.captured) % 5);
  game.floorLevel = 1;
  game.floorMax = 2 + (uint8_t)(mix32(game.missions ^ game.townSeed) % 3); // 2..4
  game.dungeonSeed = mix32(game.houseSeed ^ (game.missions * 0x9E3779B9UL) ^ esp_random());
  game.fuel = 18 + game.rank * 2;
  strlcpy(zimPlanetLine, "Zemlya-Zim", sizeof(zimPlanetLine));
  strlcpy(zimOrderLine, currentMissionName(), sizeof(zimOrderLine));
  snprintf(zimMissionLine, sizeof(zimMissionLine), "Lokalnaya missiya Zima");
  setBanner("Missiya vybrana", currentMissionName());
  if (superWeapon.charge < 35) superWeapon.charge += 2;
  game.dirty = true;
}

void enterDungeonFloor() {
  game.mode = GM_DUNGEON;
  game.stateStartMs = millis();
  game.px = 2; game.py = 3;
  chooseDungeonTarget();
  char floorText[32];
  snprintf(floorText, sizeof(floorText), "%s F%u/%u", currentMissionName(), game.floorLevel, game.floorMax);
  setBanner("Dron iz garaja zapushen", floorText);
  game.dirty = true;
}

void nextFloorOrReturn() {
  if (game.floorLevel >= game.floorMax) {
    game.missionSuccess++;
    uint32_t reward = 25 + game.floorMax * 5;
    if ((game.missionId % 5) == 2) reward += 20;
    if ((game.missionId % 5) == 3) reward += 4 * game.loot;
    game.credits += reward;
    setBanner("Missiya vypolnena", "Vozvrat v falshiviy dom");
    game.mode = GM_RETURN;
    game.stateStartMs = millis();
    game.dirty = true;
    return;
  }
  game.floorLevel++;
  enterDungeonFloor();
}

void enterEncounter() {
  game.enemy = generateEncounterMonster();
  game.seen++;
  rememberSeenSeed(game.enemy.seed);
  setBanner("Signal obrazca zahvachen", game.enemy.name);
  game.mode = GM_ENCOUNTER;
  game.stateStartMs = millis();
  game.battleStepMs = millis();
  game.enemyPhase = 0;
  game.dirty = true;
}

void enterBattle() {
  zimSlimeObserve(ZE_BATTLE, 0.55f);
  game.mode = GM_BATTLE;
  game.stateStartMs = millis();
  game.battleStepMs = millis();
  setBanner("Boy v podpole", game.enemy.name);
}

uint16_t damageRoll(const Monster& a, const Monster& b, uint8_t move) {
  uint16_t base = a.atk + a.level + (a.mut * 2);
  if (move == 2) base = a.atk / 2 + a.mut * 3 + a.level / 2;
  int16_t dmg = (int16_t)base - (int16_t)(b.def / 2);
  if (move == 3) dmg += 4;
  if (dmg < 2) dmg = 2;
  return (uint16_t)dmg;
}

void battleVictory(bool captured) {
  superWeaponTick((uint16_t)(18 + game.enemy.level), captured);
  game.battleWins++;
  game.credits += 7 + game.enemy.level;
  if (captured) captureEnemyToCollection();
  else {
    uint8_t lead = strongestPartyIndex();
    levelUpMonster(game.party[lead], 1);
    setBanner("Obrazec pribit", game.enemy.name);
  }
  if (game.party[0].hp < game.party[0].maxHp / 3 || (game.captured > 0 && (game.captured % 3 == 0))) {
    enterLab("Boevye obrazcy zagrujeny");
  } else {
    game.mode = GM_DUNGEON;
    chooseDungeonTarget();
    game.stateStartMs = millis();
  }
  game.dirty = true;
}

void battleDefeat() {
  if (superWeapon.charge > 12) superWeapon.charge -= 12;
  healParty();
  if (game.credits >= 5) game.credits -= 5;
  game.missionFails++;
  setBanner("Zim otstupaet domoy", "DeerDroid: tak i bylo zadumano");
  game.mode = GM_RETURN;
  game.stateStartMs = millis();
  game.dirty = true;
}

void updateBattle() {
  if (game.partyCount == 0) return;
  Monster &ally = game.party[0];
  Monster &enemy = game.enemy;
  if (ally.hp == 0) { battleDefeat(); return; }
  if (enemy.hp == 0) { battleVictory(false); return; }
  if (millis() - game.battleStepMs < GAME_BATTLE_STEP_MS) return;
  game.battleStepMs = millis();

  if (game.enemyPhase == 0) {
    if (superWeaponReadyForBattle()) {
      fireSuperWeapon(ally, enemy);
      if (enemy.hp == 0) { battleVictory(false); return; }
      game.enemyPhase = 1;
      return;
    }
    bool tryCapture = (enemy.hp < enemy.maxHp / 3) && ((mix32(game.steps ^ millis() ^ game.floorLevel) & 0xFF) < (70 + ally.mut * 12));
    if (tryCapture) {
      setBanner("Zim brosayet zond-kapsulu", enemy.name);
      if ((mix32(enemy.seed ^ ally.mut ^ game.captured) & 0xFF) < (122 + ally.level)) {
        battleVictory(true);
        return;
      }
      safeCopy(game.subBanner, sizeof(game.subBanner), "Obrazec ushel ot naloga dostoinstva");
    } else {
      uint8_t move = (mix32(ally.seed ^ game.steps ^ millis()) % 100 < 18) ? 2 : 0;
      if (move == 2) {
        ally.mut = (uint8_t)(((ally.mut + 1) < 9) ? (ally.mut + 1) : 9);
        ally.hp = (uint16_t)(((uint32_t)ally.hp + 4UL + ally.mut < ally.maxHp) ? ((uint32_t)ally.hp + 4UL + ally.mut) : ally.maxHp);
        snprintf(game.banner, sizeof(game.banner), "%s %s", ally.name, MOVE_TEXT[2]);
        safeCopy(game.subBanner, sizeof(game.subBanner), "Dom-lab poslal stimul");
      } else {
        uint16_t dmg = damageRoll(ally, enemy, move);
        enemy.hp = (enemy.hp > dmg) ? (enemy.hp - dmg) : 0;
        snprintf(game.banner, sizeof(game.banner), "%s %s", ally.name, MOVE_TEXT[0]);
        snprintf(game.subBanner, sizeof(game.subBanner), "-%u HP -> %s", dmg, enemy.name);
      }
    }
    game.enemyPhase = 1;
  } else {
    if (enemy.hp == 0) { battleVictory(false); return; }
    uint8_t move = ((mix32(enemy.seed ^ millis()) % 100) < 20) ? 3 : 0;
    uint16_t dmg = damageRoll(enemy, ally, move);
    ally.hp = (ally.hp > dmg) ? (ally.hp - dmg) : 0;
    snprintf(game.banner, sizeof(game.banner), "%s %s", enemy.name, MOVE_TEXT[move == 3 ? 3 : 0]);
    snprintf(game.subBanner, sizeof(game.subBanner), "-%u HP -> %s", dmg, ally.name);
    game.enemyPhase = 0;
    if (ally.hp == 0) battleDefeat();
  }
}

void updateHome() {
  // v3.7: no Core2 waiting. Buzz jobs are background chores; Zim keeps living his house cycle.
  // v3.10D STARVEFIX: Buzz is almost always active, so the old random complaint path
  // kept resetting game.stateStartMs and HOME rarely reached the mission timer.
  // Complaints now have their own cooldown and never block HOME -> MISSION -> DUNGEON.
  uint32_t now = millis();
  if (gJobActive &&
      (now - game.stateStartMs > 1400UL) &&
      (now - zimLastBuzzWhineMs > 6500UL) &&
      ((esp_random() & 0x03) == 0)) {
    setBanner("Buzz stuchit v dver", "Zim delaet vid chto zanyat");
    zimLastBuzzWhineMs = now;
    game.dirty = true;
  }
  if (now - game.stateStartMs < 2500UL) return;
  game.housePhase = (uint8_t)((game.housePhase + 1) % 4);
  game.stateStartMs = now;
  switch (game.housePhase) {
    case 0: setBanner("Dom Zima gudit", "Terminal smeetsya nad Buzz"); break;
    case 1: enterLab("Predpoletny remont"); return;
    case 2: setBanner("Garaj otkryt", "DeerDroid ignorit zadachi Buzz"); break;
    default: startMission(); return;
  }
  game.dirty = true;
}


void updateDungeon() {
  if (millis() - game.lastMoveMs < GAME_STEP_MS) return;
  game.lastMoveMs = millis();

  int nx = game.px;
  int ny = game.py;
  if (game.px < game.tx) nx++;
  else if (game.px > game.tx) nx--;
  else if (game.py < game.ty) ny++;
  else if (game.py > game.ty) ny--;

  if (!tileWalkable(dungeonTileAt(nx, ny))) {
    chooseDungeonTarget();
    return;
  }

  game.px = nx;
  game.py = ny;
  game.steps++;
  game.rngCounter++;

  // SBOR burns extra fuel: Zim drags home too much "useful" human garbage.
  uint8_t fuelCost = 1;
  if ((game.missionId % 5) == 3 && (game.steps & 1)) fuelCost = 2;
  while (fuelCost-- && game.fuel > 0) game.fuel--;
  if ((game.steps & 0x07UL) == 0) superWeaponTick((uint16_t)(12 + game.floorLevel + (game.steps & 7UL)), false);
  game.dirty = true;

  if (game.fuel == 0) {
    zimSlimeObserve(ZE_BATTLE, 0.35f);
    battleDefeat();
    return;
  }

  if (game.px == game.tx && game.py == game.ty) {
    uint8_t tile = dungeonTileAt(game.px, game.py);
    if (game.floorLevel >= game.floorMax && game.tx >= 26) {
      nextFloorOrReturn();
      return;
    }

    if (tileIsTrap(tile)) {
      game.traps++;
      if (game.partyCount > 0) {
        uint16_t dmg = (uint16_t)(4 + (game.floorLevel % 3));
        if (game.party[0].hp > dmg) game.party[0].hp -= dmg;
        else game.party[0].hp = 1;
      }
      zimSlimeObserve(ZE_BATTLE, 0.30f);
      setBanner("Lovushka srabotala", "Zemlyane stavjat tupye seti");
    } else if (tileIsLoot(tile)) {
      game.loot++;
      game.credits += 8 + game.floorLevel;
      if (superWeapon.charge < ZIM_SUPER_WEAPON_MAX_CHARGE) superWeapon.charge++;
      setBanner("Musor poluchen", "Vozmojno eto sekret SHA");
    } else if (tileIsSignal(tile) || ((mix32(game.steps ^ game.dungeonSeed ^ millis()) & 0x1FUL) < 5)) {
      enterEncounter();
      return;
    } else if ((mix32(game.steps ^ millis() ^ game.rngCounter) & 0x3FUL) < 5) {
      nextFloorOrReturn();
      return;
    }
    chooseDungeonTarget();
  }
}

void updateGameLogic() {
  switch (game.mode) {
    case GM_BOOT:
      if (millis() - game.stateStartMs > 900) game.mode = GM_HOME;
      break;
    case GM_HOME: updateHome(); break;
    case GM_MISSION:
      if (millis() - game.stateStartMs > 1200UL) enterDungeonFloor();
      break;
    case GM_DUNGEON: updateDungeon(); break;
    case GM_ENCOUNTER:
      if (millis() - game.stateStartMs > GAME_DIALOG_MS) enterBattle();
      break;
    case GM_BATTLE: updateBattle(); break;
    case GM_LAB:
      if (millis() - game.stateStartMs > 1500UL) enterHome("Tsikl laboratorii zavershen");
      break;
    case GM_RETURN:
      if (millis() - game.stateStartMs > 1300UL) enterHome("Otchet missii zagrujen");
      break;
    case GM_REPORT: // legacy safety: report is now an overlay, not a paused game state
      game.mode = GM_HOME;
      break;
    default: game.mode = GM_HOME; break;
  }
  if (game.dirty && millis() - game.lastSaveMs > GAME_SAVE_MS) saveGame();
}

// ============================================================
// GBA JRPG DRAW HELPERS
// ============================================================
uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
  const uint16_t k = zimLcdSoftBrightness;
  // Cheap ST7789 panels lose saturation; bias key invader colors before dimming.
  if (g > r && g > b) g = (uint8_t)min(255U, (unsigned)g + 18U);
  if (b > r && b > g) b = (uint8_t)min(255U, (unsigned)b + 12U);
  r=(uint8_t)(((uint16_t)r*k)/255U); g=(uint8_t)(((uint16_t)g*k)/255U); b=(uint8_t)(((uint16_t)b*k)/255U);
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void pushDisplayFrame() {
  // Двойной буфер: полное обновление кадра за одну операцию для избежания артефактов ("пауков").
  lcd.drawRGBBitmap(0, 0, tft.getBuffer(), GAME_SCREEN_W, GAME_SCREEN_H);
}


void loadDisplayPrefs() {
  uint8_t idx = prefs.getUChar("lcdBriIdx", zimBrightnessIndex);
  if (idx >= 6) idx = 2;
  zimBrightnessIndex = idx;
  zimLcdSoftBrightness = ZIM_LCD_BRIGHTNESS_LEVELS[zimBrightnessIndex];
}

void saveDisplayPrefs() {
  prefs.putUChar("lcdBriIdx", zimBrightnessIndex);
}

void cycleZimBrightness() {
  zimBrightnessIndex = (uint8_t)((zimBrightnessIndex + 1) % 6);
  zimLcdSoftBrightness = ZIM_LCD_BRIGHTNESS_LEVELS[zimBrightnessIndex];
  saveDisplayPrefs();
  char b[48];
  snprintf(b, sizeof(b), "Yarkost %u/6  soft=%u", (unsigned)zimBrightnessIndex + 1U, (unsigned)zimLcdSoftBrightness);
  setBanner("Ekran priglushen", b);
  zimLastBrightnessToastMs = millis();
}

void drawTinyInvader(int x, int y, uint16_t c) {
  tft.drawPixel(x+1, y+0, c); tft.drawPixel(x+5, y+0, c);
  tft.drawPixel(x+0, y+2, c); tft.drawPixel(x+2, y+2, c); tft.drawPixel(x+3, y+2, c); tft.drawPixel(x+4, y+2, c); tft.drawPixel(x+6, y+2, c);
  tft.drawPixel(x+0, y+3, c); tft.drawPixel(x+1, y+3, c); tft.drawPixel(x+2, y+3, c); tft.drawPixel(x+3, y+3, c); tft.drawPixel(x+4, y+3, c); tft.drawPixel(x+5, y+3, c); tft.drawPixel(x+6, y+3, c);
  tft.drawPixel(x+1, y+4, c); tft.drawPixel(x+5, y+4, c);
  tft.drawPixel(x+0, y+5, c); tft.drawPixel(x+2, y+5, c); tft.drawPixel(x+4, y+5, c); tft.drawPixel(x+6, y+5, c);
}

void drawInvaderOverlay() {
  // v3.9C: screen filter removed completely.
  // No scanlines/glitch/invader overlay over the game: cheap ST7789 needs a clean GBA/JRPG frame.
  return;
}


void drawMiniBar(int x, int y, int w, int h, uint16_t fg, uint16_t bg, int value, int maxv) {
  tft.fillRect(x, y, w, h, rgb(20, 20, 20)); // Shadow/Border
  tft.fillRect(x + 1, y + 1, w - 2, h - 2, bg);
  if (maxv < 1) maxv = 1; int fill = (value * (w - 2)) / maxv;
  if (fill < 0) fill = 0; if (fill > w - 2) fill = w - 2;
  tft.fillRect(x + 1, y + 1, fill, h - 2, fg);
  tft.drawFastHLine(x + 1, y + 1, fill, rgb(255, 255, 255)); // Highlight top edge
}

void drawSpriteAlien(int x, int y, bool frame) {
  uint16_t green = rgb(60, 255, 100); uint16_t dark = rgb(10, 40, 10); uint16_t pink = rgb(255, 30, 150);
  int bob = frame ? 1 : 0;
  tft.fillRect(x + 2, y + 1 + bob, 4, 6, green);
  tft.fillRect(x + 1, y + 3 + bob, 6, 4, green);
  tft.fillRect(x + 0, y + 0 + bob, 2, 2, dark);
  tft.fillRect(x + 6, y + 0 + bob, 2, 2, dark);
  tft.fillRect(x + 2, y + 3 + bob, 1, 1, pink);
  tft.fillRect(x + 5, y + 3 + bob, 1, 1, pink);
  tft.fillRect(x + 2, y + 7, 1, 2, green);
  tft.fillRect(x + 5, y + 7, 1, 2, green);
}

void drawSpriteDeerDroid(int x, int y, uint16_t base, bool frame) {
  uint16_t white = rgb(225, 225, 235); uint16_t blue = rgb(80, 170, 255);
  int bob = frame ? 1 : 0;
  tft.fillRect(x + 1, y + 2 + bob, 6, 4, base);
  tft.fillRect(x + 5, y + 1 + bob, 3, 3, white);
  tft.fillRect(x + 0, y + 3 + bob, 2, 2, base);
  tft.fillRect(x + 2, y + 0 + bob, 1, 2, white);
  tft.fillRect(x + 4, y + 0 + bob, 1, 2, white);
  // Legs move opposite
  tft.fillRect(x + 2, y + 6, 1, 2, frame ? base : blue);
  tft.fillRect(x + 5, y + 6, 1, 2, frame ? blue : base);
}

void drawMonsterIcon(int x, int y, const Monster& m, bool front, bool frame) {
  uint16_t c1 = rgb(80 + (m.classId * 11) % 120, 60 + (m.classId * 23) % 140, 70 + (m.classId * 31) % 120);
  uint16_t c2 = rgb(220, 240, 255);
  int bob = frame ? 1 : 0;
  if (strcmp(m.name, "DeerDroid") == 0) { drawSpriteDeerDroid(x, y, rgb(130, 170, 210), frame); return; }
  
  tft.fillRoundRect(x + 1, y + 2 + bob, 14, 10, 3, c1);
  if (m.humanoid) {
    tft.fillRect(x + 4, y + 0 + bob, 6, 5, c2);
    tft.fillRect(x + 5, y + 12, 4, 3, c2);
  } else {
    tft.fillCircle(x + 9, y + 6 + bob, 5, c2);
    tft.drawPixel(x + 13, y + 2 + bob, rgb(255,80,80));
  }
  if (front) {
    tft.drawPixel(x + 6, y + 5 + bob, rgb(255,20,100));
    tft.drawPixel(x + 10, y + 5 + bob, rgb(255,20,100));
  }
}

// Backward-compatible wrappers for old drawing calls kept by some scenes.
void drawSpriteAlien(int x, int y) { drawSpriteAlien(x, y, false); }
void drawSpriteDeerDroid(int x, int y, uint16_t base) { drawSpriteDeerDroid(x, y, base, false); }
void drawMonsterIcon(int x, int y, const Monster& m, bool front) { drawMonsterIcon(x, y, m, front, false); }

const char* gameModeText() {
  switch (game.mode) { case GM_HOME: return "BASE"; case GM_MISSION: return "MSN"; case GM_DUNGEON: return "FLR"; case GM_ENCOUNTER: return "SIG"; case GM_BATTLE: return "BTTL"; case GM_LAB: return "LAB"; case GM_RETURN: return "RET"; case GM_REPORT: return "SYS"; default: return "BOOT"; }
}

void printCompactUL(uint32_t v) {
  if (v < 1000UL) tft.print((unsigned long)v); else if (v < 1000000UL) { tft.print((unsigned long)(v / 1000UL)); tft.print("k"); } else { tft.print((unsigned long)(v / 1000000UL)); tft.print("m"); }
}

uint16_t poolDotColor() {
  if (WiFi.status() != WL_CONNECTED || !soloPoolConnected) return rgb(255, 60, 60);
  if (!soloAuthorized) return rgb(255, 200, 30);
  if (!soloJobReady) return rgb(80, 150, 255);
  return rgb(70, 255, 100);
}


uint16_t gbaC(uint8_t r, uint8_t g, uint8_t b) { return rgb(r, g, b); }

void drawGbaFrame(int x, int y, int w, int h, uint16_t fill, uint16_t hi, uint16_t shade) {
  tft.fillRect(x, y, w, h, shade);
  tft.fillRoundRect(x + 1, y + 1, w - 2, h - 2, 3, fill);
  tft.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 3, hi);
  tft.drawFastHLine(x + 3, y + 2, w - 6, rgb(235, 245, 255));
  tft.drawFastVLine(x + 2, y + 3, h - 6, rgb(100, 130, 165));
  tft.drawFastHLine(x + 3, y + h - 3, w - 6, shade);
  tft.drawFastVLine(x + w - 3, y + 3, h - 6, shade);
}

void drawGbaDialog(int x, int y, int w, int h) {
  drawGbaFrame(x, y, w, h, rgb(245, 245, 236), rgb(70, 86, 112), rgb(18, 24, 36));
  tft.drawRect(x + 4, y + 4, w - 8, h - 8, rgb(160, 170, 190));
  tft.setTextColor(rgb(25, 30, 42));
}

void drawGbaFloorTile(int x, int y, uint16_t base, uint16_t edge, uint32_t seed) {
  tft.fillRect(x, y, 8, 8, base);
  tft.drawFastHLine(x, y, 8, edge);
  tft.drawFastVLine(x, y, 8, edge);
  if (seed & 1) tft.drawPixel(x + 5, y + 3, rgb(95, 105, 120));
  if (seed & 2) tft.drawPixel(x + 2, y + 6, rgb(42, 50, 66));
}

void drawGbaWallTile(int x, int y, uint32_t seed) {
  uint16_t a = rgb(78, 86, 104);
  uint16_t b = rgb(50, 58, 78);
  uint16_t h = rgb(122, 132, 152);
  tft.fillRect(x, y, 8, 8, a);
  tft.drawFastHLine(x, y, 8, h);
  tft.drawFastHLine(x, y + 7, 8, b);
  tft.drawFastVLine(x, y, 8, b);
  if (seed & 1) tft.drawFastHLine(x + 2, y + 3, 4, b);
  if (seed & 2) tft.drawPixel(x + 6, y + 2, h);
}

void drawGbaHouseSprite(int x, int y, bool frame) {
  uint16_t wall = rgb(185, 120, 84);
  uint16_t roof = rgb(112, 48, 72);
  uint16_t roofHi = rgb(176, 68, 102);
  uint16_t door = rgb(78, 52, 46);
  tft.fillRect(x + 6, y + 15, 58, 34, wall);
  tft.drawRect(x + 6, y + 15, 58, 34, rgb(70, 48, 54));
  tft.fillTriangle(x + 1, y + 18, x + 35, y + 0, x + 69, y + 18, roof);
  tft.drawLine(x + 4, y + 17, x + 35, y + 1, roofHi);
  tft.fillRect(x + 30, y + 31, 12, 18, door);
  tft.fillRect(x + 13, y + 24, 13, 10, rgb(112, 170, 205));
  tft.drawRect(x + 13, y + 24, 13, 10, rgb(35, 55, 75));
  tft.fillRect(x + 47, y + 24, 11, 10, rgb(112, 170, 205));
  tft.drawRect(x + 47, y + 24, 11, 10, rgb(35, 55, 75));
  if (frame) tft.fillRect(x + 34, y + 39, 2, 2, rgb(240, 220, 90));
}

void drawGbaComputer(int x, int y, bool frame) {
  tft.fillRect(x, y + 8, 34, 22, rgb(88, 90, 112));
  tft.drawRect(x, y + 8, 34, 22, rgb(32, 36, 48));
  tft.fillRect(x + 4, y + 11, 26, 12, frame ? rgb(72, 170, 118) : rgb(42, 100, 82));
  tft.drawRect(x + 4, y + 11, 26, 12, rgb(10, 40, 30));
  tft.fillRect(x + 10, y + 30, 14, 4, rgb(58, 60, 78));
  tft.fillRect(x + 4, y + 34, 26, 3, rgb(42, 44, 60));
}

void drawGbaChest(int x, int y, bool open) {
  tft.fillRect(x + 1, y + 4, 13, 8, rgb(164, 94, 42));
  tft.drawRect(x + 1, y + 4, 13, 8, rgb(70, 42, 24));
  tft.fillRect(x + 2, y + 5, 11, 2, rgb(230, 164, 70));
  tft.fillRect(x + 6, y + 7, 3, 3, rgb(235, 220, 90));
  if (open) tft.drawFastHLine(x + 2, y + 2, 11, rgb(240, 210, 86));
}

void drawGbaTinyArrow(int x, int y, uint16_t c) {
  tft.fillTriangle(x, y, x + 5, y + 3, x, y + 6, c);
}

void drawGbaShadow(int cx, int cy, int w, int h, uint16_t c) {
  if (w < 4) w = 4;
  if (h < 2) h = 2;
  tft.fillRoundRect(cx - w / 2, cy - h / 2, w, h, h / 2, c);
}

void drawGbaPlatform(int cx, int cy, int w, int h, uint16_t fill, uint16_t edge) {
  drawGbaShadow(cx, cy, w, h, fill);
  tft.drawRoundRect(cx - w / 2, cy - h / 2, w, h, h / 2, edge);
  tft.drawFastHLine(cx - w / 2 + 6, cy - h / 2 + 2, w - 12, rgb(126, 168, 118));
}

void drawHeader() {
  tft.fillRect(0, 0, GAME_SCREEN_W, GAME_HEADER_H, rgb(31, 53, 92));
  tft.drawFastHLine(0, 0, GAME_SCREEN_W, rgb(112, 160, 210));
  tft.drawFastHLine(0, GAME_HEADER_H - 2, GAME_SCREEN_W, rgb(13, 22, 44));
  tft.drawFastHLine(0, GAME_HEADER_H - 1, GAME_SCREEN_W, rgb(5, 10, 24));
  tft.setTextWrap(false); tft.setTextSize(1);

  tft.fillRoundRect(2, 2, 49, 10, 2, rgb(18, 34, 68));
  tft.drawRoundRect(2, 2, 49, 10, 2, rgb(92, 138, 190));
  tft.setTextColor(rgb(190, 255, 188));
  tft.setCursor(5, 4); tft.print("ZG "); tft.print(gameModeText());

  tft.fillRect(55, 5, 5, 5, poolDotColor());
  tft.drawRect(54, 4, 7, 7, rgb(12, 20, 34));

  tft.setTextColor(rgb(245, 245, 232));
  tft.setCursor(65, 4); tft.print("H"); printCompactUL(gHashRateSmooth);
  tft.setCursor(101, 4); tft.print("B"); printCompactUL(gBestBits);
  tft.setCursor(132, 4); tft.print("F"); tft.print((unsigned)game.floorLevel); tft.print("/"); tft.print((unsigned)game.floorMax);
  tft.setCursor(166, 4); tft.print("G"); printCompactUL(game.fuel);
  tft.setTextColor(rgb(255, 224, 92));
  tft.setCursor(198, 4); tft.print("$"); printCompactUL(game.credits);

  uint16_t wcol = (superWeapon.route == ZW_BITE) ? rgb(230, 86, 190) : (superWeapon.route == ZW_FLOW ? rgb(82, 196, 230) : rgb(170, 110, 230));
  tft.fillRect(63, 13, 118, 1, rgb(12, 18, 38));
  tft.fillRect(63, 13, (uint8_t)((superWeapon.charge * 118U) / 100U), 1, wcol);
}


void drawHouseBase() {
  bool frame = (millis() / 520UL) & 1UL;
  tft.fillRect(0, GAME_HEADER_H, GAME_SCREEN_W, GAME_SCREEN_H - GAME_HEADER_H, rgb(118, 164, 196));

  tft.fillRect(0, 37, GAME_SCREEN_W, 16, rgb(92, 132, 150));
  for (int x = 4; x < GAME_SCREEN_W; x += 38) {
    int yy = 31 + ((x >> 2) & 3);
    tft.fillRect(x, yy, 22, 18, rgb(76, 110, 132));
    tft.fillTriangle(x - 2, yy + 1, x + 11, yy - 7 + ((x >> 3) & 3), x + 24, yy + 1, rgb(84, 68, 90));
  }

  tft.fillRect(0, 53, GAME_SCREEN_W, GAME_SCREEN_H - 53, rgb(54, 140, 74));
  for (int y = 53; y < GAME_SCREEN_H; y += 8) {
    for (int x = 0; x < GAME_SCREEN_W; x += 8) {
      uint32_t h = mix32((uint32_t)x * 13UL + (uint32_t)y * 97UL + game.townSeed);
      if ((h & 7U) == 0U) tft.drawPixel(x + 3, y + 4, rgb(92, 180, 92));
      if ((h & 15U) == 1U) tft.drawPixel(x + 6, y + 2, rgb(36, 104, 58));
    }
  }
  tft.fillRect(94, 86, 54, 49, rgb(176, 148, 92));
  for (int y = 90; y < GAME_SCREEN_H; y += 10) tft.drawFastHLine(98, y, 46, rgb(146, 120, 76));

  drawGbaHouseSprite(84, 34, frame);
  drawGbaComputer(16, 64, frame);

  drawGbaFrame(164, 56, 56, 36, rgb(86, 72, 66), rgb(208, 170, 100), rgb(42, 34, 32));
  tft.setTextColor(rgb(245, 226, 160)); tft.setCursor(174, 64); tft.print("GARAJ");
  tft.fillRect(171, 76, 42, 10, rgb(42, 46, 56));
  for (int i = 0; i < 4; ++i) tft.drawFastHLine(173, 78 + i * 2, 38, rgb(82, 88, 104));

  drawGbaShadow(118, 95, 20, 5, rgb(36, 82, 50));
  drawGbaShadow(138, 99, 20, 5, rgb(36, 82, 50));
  drawSpriteAlien(112, 82, frame);
  drawSpriteDeerDroid(132, 86, rgb(122, 172, 220), frame);

  if (zimMissionPending) {
    drawGbaFrame(54, 23, 132, 24, rgb(28, 42, 70), rgb(130, 210, 255), rgb(10, 18, 30));
    tft.setTextColor(rgb(190,255,255));
    tft.setCursor(66, 30); tft.print("SIGNAL CORE2");
    if (frame) drawGbaTinyArrow(174, 32, rgb(255, 240, 120));
  }

  drawGbaDialog(2, 104, 236, 29);
  tft.setCursor(8, 110); tft.print(game.banner);
  tft.setCursor(8, 120); tft.print(game.subBanner);
}


void drawMissionSelect() {
  bool frame = (millis() / 400UL) & 1UL;
  tft.fillRect(0, GAME_HEADER_H, GAME_SCREEN_W, GAME_SCREEN_H - GAME_HEADER_H, rgb(76, 116, 150));
  tft.fillRect(0, 72, GAME_SCREEN_W, 63, rgb(52, 130, 74));
  for (int x = 0; x < GAME_SCREEN_W; x += 16) tft.drawFastVLine(x, 72, 63, rgb(42, 110, 66));
  drawGbaHouseSprite(14, 44, false);
  drawSpriteAlien(88, 92, frame);
  drawSpriteDeerDroid(104, 96, rgb(122, 172, 220), frame);

  drawGbaFrame(118, 24, 110, 84, rgb(245, 245, 236), rgb(70, 86, 112), rgb(18, 24, 36));
  tft.setTextColor(rgb(28, 34, 46));
  tft.setCursor(126, 32); tft.print("VYBOR MISSII");
  tft.drawFastHLine(124, 44, 96, rgb(160, 170, 190));
  tft.setCursor(126, 52); tft.print("Zona "); tft.print(currentMissionName());
  tft.setCursor(126, 65); tft.print("Etaji "); tft.print((unsigned)game.floorMax);
  tft.setCursor(126, 78); tft.print("Toplivo "); tft.print((unsigned)game.fuel);
  tft.setCursor(126, 91); tft.print("Core "); tft.print(zimMissionPending ? "prikaz" : "svob");

  drawGbaDialog(2, 112, 236, 21);
  tft.setCursor(8, 118); tft.print(zimMissionLine);
}


void drawDungeon() {
  bool frame = (millis() / 400UL) & 1UL;
  tft.fillRect(0, GAME_HEADER_H, GAME_SCREEN_W, GAME_SCREEN_H - GAME_HEADER_H, rgb(18, 24, 34));

  for (int y = 0; y < GAME_MAP_H; ++y) {
    for (int x = 0; x < GAME_MAP_W; ++x) {
      uint8_t tt = dungeonTileAt(x, y);
      int px = x * GAME_TILE;
      int py = GAME_HEADER_H + y * GAME_TILE;
      uint32_t seed = mix32((uint32_t)x * 911UL + (uint32_t)y * 353UL + game.townSeed + game.floorLevel * 17UL);
      if (tt == 1) drawGbaWallTile(px, py, seed);
      else drawGbaFloorTile(px, py, rgb(38, 48, 62), rgb(28, 36, 50), seed);
      if (tt == 3) {
        tft.fillTriangle(px + 1, py + 6, px + 4, py + 1, px + 7, py + 6, rgb(190, 60, 62));
        tft.drawFastHLine(px + 2, py + 6, 4, rgb(82, 24, 32));
      } else if (tt == 4) {
        uint16_t sig = frame ? rgb(86, 232, 128) : rgb(36, 130, 78);
        tft.drawCircle(px + 4, py + 4, 3, sig);
        tft.drawPixel(px + 4, py + 4, sig);
      } else if (tt == 5) {
        drawGbaChest(px, py - 1, frame);
      }
    }
  }

  int txp = game.tx * GAME_TILE;
  int typ = GAME_HEADER_H + game.ty * GAME_TILE;
  tft.drawRect(txp, typ, GAME_TILE, GAME_TILE, rgb(248, 222, 70));
  if (frame) drawGbaTinyArrow(txp + 1, typ - 2, rgb(248, 222, 70));

  int px = game.px * GAME_TILE;
  int py = GAME_HEADER_H + game.py * GAME_TILE;
  drawGbaShadow(px + 4, py + 8, 12, 4, rgb(12, 18, 26));
  drawSpriteAlien(px, py - 1, frame);
  int fx = game.px > 1 ? game.px - 1 : game.px;
  int dpx = fx * GAME_TILE;
  int dpy = GAME_HEADER_H + game.py * GAME_TILE + 1;
  drawGbaShadow(dpx + 4, dpy + 8, 12, 4, rgb(12, 18, 26));
  drawSpriteDeerDroid(dpx, dpy, rgb(110, 160, 220), frame);

  drawGbaFrame(176, 17, 62, 17, rgb(245, 245, 236), rgb(70, 86, 112), rgb(18, 24, 36));
  tft.setTextColor(rgb(28, 34, 46)); tft.setCursor(182, 22); tft.print("FLR "); tft.print((unsigned)game.floorLevel);

  drawGbaDialog(2, 111, 236, 22);
  tft.setCursor(8, 117); tft.print(game.banner);
}


void drawEncounter() {
  bool frame = (millis() / 220UL) & 1UL;
  tft.fillRect(0, GAME_HEADER_H, GAME_SCREEN_W, GAME_SCREEN_H - GAME_HEADER_H, rgb(48, 72, 112));
  for (int y = GAME_HEADER_H; y < GAME_SCREEN_H; y += 8) tft.drawFastHLine(0, y, GAME_SCREEN_W, rgb(36, 54, 88));
  for (int i = 0; i < 8; ++i) {
    int x = 30 + i * 26 + (frame ? 2 : 0);
    tft.drawLine(x, 28, x - 18, 92, rgb(68, 90, 132));
  }
  tft.fillRoundRect(128, 28, 76, 44, 6, rgb(238, 238, 226));
  tft.drawRoundRect(128, 28, 76, 44, 6, rgb(70, 86, 112));
  drawMonsterIcon(156, 42, game.enemy, true, frame);
  drawSpriteAlien(44, 72, frame);
  drawSpriteDeerDroid(62, 77, rgb(122, 172, 220), frame);

  drawGbaDialog(8, 96, 224, 34);
  tft.setCursor(16, 103); tft.print("TREVOGA: "); tft.print(game.enemy.name);
  tft.setCursor(16, 116); tft.print("Zapusk boya...");
}


void drawBattle() {
  bool frame = (millis() / 400UL) & 1UL;
  tft.fillRect(0, GAME_HEADER_H, GAME_SCREEN_W, GAME_SCREEN_H - GAME_HEADER_H, rgb(118, 150, 126));

  drawGbaPlatform(170, 58, 94, 26, rgb(82, 120, 92), rgb(44, 78, 58));
  drawGbaPlatform(58, 90, 104, 28, rgb(90, 132, 96), rgb(44, 78, 58));

  Monster &ally = game.party[0];
  Monster &enemy = game.enemy;

  drawGbaFrame(8, 22, 112, 29, rgb(245, 245, 236), rgb(70, 86, 112), rgb(18, 24, 36));
  tft.setTextColor(rgb(28, 34, 46));
  tft.setCursor(15, 27); tft.print(enemy.name);
  tft.setCursor(94, 27); tft.print("L"); tft.print(enemy.level);
  drawMiniBar(16, 40, 88, 5, rgb(220,72,72), rgb(75,30,30), enemy.hp, enemy.maxHp);

  drawMonsterIcon(162, 42, enemy, true, frame);

  drawSpriteDeerDroid(48, 76, rgb(122, 172, 220), frame);
  drawSpriteAlien(31, 70, frame);

  drawGbaFrame(116, 64, 116, 32, rgb(245, 245, 236), rgb(70, 86, 112), rgb(18, 24, 36));
  tft.setTextColor(rgb(28, 34, 46));
  tft.setCursor(123, 69); tft.print(ally.name);
  tft.setCursor(207, 69); tft.print("L"); tft.print(ally.level);
  drawMiniBar(124, 81, 90, 5, rgb(72,210,96), rgb(30,74,36), ally.hp, ally.maxHp);
  drawMiniBar(124, 88, 46, 3, rgb(88,178,230), rgb(30,48,74), ally.mut, 10);
  drawMiniBar(174, 88, 40, 3, rgb(226,82,194), rgb(62,22,64), superWeapon.charge, 100);
  if (superWeapon.charge >= 100 && frame) {
    tft.setTextColor(rgb(168, 50, 142)); tft.setCursor(181, 93); tft.print("S!");
  }

  drawGbaDialog(4, 101, 232, 31);
  tft.setCursor(10, 108); tft.print(game.banner);
  tft.setCursor(10, 120); tft.print(game.subBanner);
}


void drawLab() {
  bool frame = (millis() / 360UL) & 1UL;
  tft.fillRect(0, GAME_HEADER_H, GAME_SCREEN_W, GAME_SCREEN_H - GAME_HEADER_H, rgb(42, 36, 70));
  for (int x = 0; x < GAME_SCREEN_W; x += 16) tft.drawFastVLine(x, GAME_HEADER_H, GAME_SCREEN_H - GAME_HEADER_H, rgb(32, 28, 56));
  tft.fillRect(0, 88, GAME_SCREEN_W, 47, rgb(54, 44, 76));
  drawGbaComputer(20, 40, frame);
  tft.fillRect(148, 38, 18, 46, rgb(78, 92, 118));
  tft.drawRect(148, 38, 18, 46, rgb(150, 210, 230));
  tft.fillRect(151, 42, 12, 34, frame ? rgb(74, 190, 145) : rgb(46, 126, 104));
  drawMonsterIcon(174, 65, game.party[strongestPartyIndex()], true, frame);
  drawSpriteAlien(74, 76, frame);

  drawGbaFrame(8, 22, 124, 20, rgb(245, 245, 236), rgb(70, 86, 112), rgb(18, 24, 36));
  tft.setTextColor(rgb(28,34,46)); tft.setCursor(16, 28); tft.print("LAB ZIMA");
  drawGbaDialog(8, 101, 224, 31);
  tft.setCursor(16, 108); tft.print("Lider: "); tft.print(game.party[strongestPartyIndex()].name);
  tft.setCursor(16, 120); tft.print(game.banner);
}


void drawReturn() {
  bool frame = (millis() / 400UL) & 1UL;
  tft.fillRect(0, GAME_HEADER_H, GAME_SCREEN_W, GAME_SCREEN_H - GAME_HEADER_H, rgb(82, 132, 108));
  for (int x = 0; x < GAME_SCREEN_W; x += 12) tft.drawFastVLine(x, 52, 83, rgb(66, 112, 92));
  drawGbaHouseSprite(20, 42, false);
  drawSpriteAlien(104, 84, frame);
  drawSpriteDeerDroid(122, 88, rgb(122, 172, 220), frame);

  drawGbaFrame(142, 25, 88, 75, rgb(245, 245, 236), rgb(70, 86, 112), rgb(18, 24, 36));
  tft.setTextColor(rgb(28,34,46));
  tft.setCursor(150, 34); tft.print("OTCHET");
  tft.drawFastHLine(148, 46, 74, rgb(160,170,190));
  tft.setCursor(150, 54); tft.print("Obr "); tft.print((unsigned long)game.captured);
  tft.setCursor(150, 67); tft.print("Loot "); tft.print((unsigned long)game.loot);
  tft.setCursor(150, 80); tft.print("Win "); tft.print((unsigned long)game.battleWins);

  drawGbaDialog(2, 108, 236, 24);
  tft.setCursor(8, 115); tft.print(zimOrderLine);
}


void drawStatusScan() {
  tft.fillRect(0, GAME_HEADER_H, GAME_SCREEN_W, GAME_SCREEN_H - GAME_HEADER_H, rgb(44, 58, 92));
  tft.setTextWrap(false); tft.setTextSize(1);
  drawGbaFrame(6, 20, 228, 108, rgb(245, 245, 236), rgb(70, 86, 112), rgb(18, 24, 36));
  tft.setTextColor(rgb(28,34,46));
  tft.setCursor(15, 28); tft.print("STATUS ZIM GEEK");
  tft.drawFastHLine(14, 40, 212, rgb(160,170,190));

  tft.setCursor(15, 47); tft.print("Set "); tft.fillRect(39, 49, 4, 4, poolDotColor());
  tft.setCursor(48, 47); tft.print(soloStatus);
  tft.setCursor(15, 60); tft.print("Hesh/s "); printCompactUL(gHashRateSmooth);
  tft.setCursor(124, 60); tft.print("OK/Sub "); printCompactUL(soloAccepts); tft.print("/"); printCompactUL(soloSubmits);

  tft.setCursor(15, 74); tft.print("Rang"); drawMiniBar(47, 74, 70, 7, rgb(80,190,100), rgb(205,225,200), game.rank, 10);
  tft.setCursor(15, 88); tft.print("Topl"); drawMiniBar(47, 88, 70, 7, rgb(210,165,40), rgb(225,215,185), game.fuel, 32);
  tft.setCursor(124, 88); tft.print("ORUJ"); drawMiniBar(160, 88, 58, 7, rgb(210,74,178), rgb(220,200,224), superWeapon.charge, 100);
  tft.setCursor(124, 74); tft.print(superWeaponRouteText()); tft.print(" /"); printCompactUL(superWeapon.shots);

  tft.setCursor(15, 103); tft.print("Missii "); tft.print((unsigned long)game.missions);
  tft.setCursor(80, 103); tft.print("BZ "); tft.print((unsigned long)gBuzzJobsAccepted);
  tft.setCursor(124, 103); tft.print("SH "); tft.print((unsigned long)gBuzzSharesSent);
  tft.setCursor(15, 116); tft.print(zimSlimeFace()); tft.print(" "); tft.print(zimSlimeMoodText()); tft.print(" SHA "); tft.print((unsigned)zimF2B(zimSlime.shaObsession));
  tft.setCursor(142, 116); tft.print("TH "); tft.print((unsigned)zimRama.resonance);
}


void drawGame() {
  if (millis() - game.lastDrawMs < zimDrawIntervalMs()) return;
  game.lastDrawMs = millis();
  gHashRateSmooth = (gHashRateSmooth * 3UL + gHashRate + 2UL) / 4UL;
  drawHeader();
  if (zimStatusOverlay) {
    // v3.10C: status is only a visual overlay. updateGameLogic() keeps running the real game underneath.
    drawStatusScan();
  } else {
    switch (game.mode) {
      case GM_HOME: drawHouseBase(); break;
      case GM_MISSION: drawMissionSelect(); break;
      case GM_DUNGEON: drawDungeon(); break;
      case GM_ENCOUNTER: drawEncounter(); break;
      case GM_BATTLE: drawBattle(); break;
      case GM_LAB: drawLab(); break;
      case GM_RETURN: drawReturn(); break;
      case GM_BOOT:
        tft.fillRect(0, GAME_HEADER_H, GAME_SCREEN_W, GAME_SCREEN_H - GAME_HEADER_H, rgb(10, 10, 20));
        tft.setTextColor(rgb(120,255,160)); tft.setCursor(16, 34); tft.print("Zim Geek v3.10D");
        tft.setTextColor(rgb(255,255,255)); tft.setCursor(16, 78); tft.print("Zapusk UI..."); break;
      case GM_REPORT:
      default: drawHouseBase(); break;
    }
  }
  // v3.9C: no post-filter; push clean frame directly.
  pushDisplayFrame(); // Обновляет физический экран, избегая артефактов
}

// ============================================================
// RX HANDLERS
// ============================================================
void handleJobPacket(const uint8_t* srcMac, int8_t rssi, const JobPacket& job) {
  if (!srcMac || !macLooksValid(srcMac)) return;
  if (job.magic[0] != 'J' || job.magic[1] != 'B') return;
  if (job.range_size == 0) return;

#if ZIM_BUZZ_LAZY_WORKER
  uint32_t now = millis();
  // FINAL LOCKED: Buzz may spam fresh ranges faster than Zim bothers to work.
  // Keep the current fresh chore for a short window; after that Zim may drop it dramatically.
  bool holdCurrent = false;
  uint32_t activeAge = 0;
  uint32_t heldCount = 0;
  portENTER_CRITICAL(&jobMux);
  if (gJobActive && gNonceRemaining > 0) {
    activeAge = now - gJobRxMs;
    if (activeAge < ZIM_BUZZ_JOB_HOLD_MS) {
      holdCurrent = true;
      gBuzzJobsHeld++;
      heldCount = gBuzzJobsHeld;
      gLastRssi = rssi;
    }
  }
  portEXIT_CRITICAL(&jobMux);
  if (holdCurrent) {
    ensurePeer(srcMac);
    if (now - gBuzzLastHoldLogMs > 1500UL) {
      char macText[24]; macToText(srcMac, macText, sizeof(macText));
      Serial.printf("[ZIM/BUZZ] hold old chore #%lu master=%s age=%lu/%lu newStart=%08lx plan=ne-seychas\n",
                    (unsigned long)heldCount, macText, (unsigned long)activeAge,
                    (unsigned long)ZIM_BUZZ_JOB_HOLD_MS, (unsigned long)job.start_nonce);
      gBuzzLastHoldLogMs = now;
    }
    return;
  }

  uint32_t endNonce = job.start_nonce + job.range_size; // natural uint32 wrap is acceptable for Buzz ranges.
  uint16_t targetBits = countLeadingZeroBits(job.target);
  uint32_t wobble = (job.range_size > 128UL) ? (esp_random() % 96UL) : 0UL;
  uint32_t lazyCursor = job.start_nonce + job.range_size - 1UL - wobble; // Zim works backwards because of course he does.

  portENTER_CRITICAL(&jobMux);
  memcpy(gJobId, job.job_id, 8);
  memcpy(gHeader, job.header, 80);
  memcpy(gTarget, job.target, 32);
  memcpy(gMasterMac, srcMac, 6);
  gNonceCursor = lazyCursor;
  gNonceEnd = endNonce;
  gNonceRemaining = (job.range_size > wobble) ? (job.range_size - wobble) : job.range_size;
  gRangeSize = job.range_size;
  gExtranonce2 = job.extranonce2;
  gJobRxMs = millis();
  gLastMasterMs = gJobRxMs;
  gJobActive = true;
  gHaveMaster = true;
  gTargetBits = targetBits;
  gLastRssi = rssi;
  gJobsRx++;
  gBuzzJobsAccepted++;
  portEXIT_CRITICAL(&jobMux);

  ensurePeer(srcMac);
  snprintf(zimOrderLine, sizeof(zimOrderLine), "Buzz: srochno. Zim: pozhe");
  snprintf(zimMissionLine, sizeof(zimMissionLine), "Buzz range %lu len %lu", (unsigned long)job.start_nonce, (unsigned long)job.range_size);
  if ((gBuzzJobsAccepted & 0x03UL) == 1UL) {
    setBanner("Buzz dal rabotu", "Zim beret ee halatno");
    game.dirty = true;
  }

  if (now - gLastJobLogMs > 1200UL) {
    char macText[24]; macToText(srcMac, macText, sizeof(macText));
    Serial.printf("[ZIM/BUZZ] lazy job #%lu master=%s rssi=%d start=%08lx range=%lu target=%u cursor=%08lx plan=halatno\n",
                  (unsigned long)gBuzzJobsAccepted, macText, (int)rssi,
                  (unsigned long)job.start_nonce, (unsigned long)job.range_size,
                  (unsigned)targetBits, (unsigned long)lazyCursor);
    gLastJobLogMs = now;
  }
#else
  gJobsRx++;
  gBuzzJobsIgnored++;
#endif
}

void handleAgentReward(const uint8_t* srcMac, int8_t rssi, const JanusAgentRewardPacket& ar) {
  if (ar.magic[0] != 'A' || ar.magic[1] != 'R') return;
  if (ar.version != 1) return;
  if (!targetMatchesNode(ar.targetNode)) return;

  uint16_t b = ar.targetBatch;
  if (b < JANUS_MIN_BATCH) b = JANUS_MIN_BATCH;
  if (b > JANUS_MAX_BATCH) b = JANUS_MAX_BATCH;

  gAiBatch = b;
  gAiHint = ar.aiHint;
  gLastPredictionError = ar.predictionError;
  gLastRewardScore = ar.score;
  gRewardsRx++;
  gLastRssi = rssi;
  gLastMasterMs = millis();

  if (srcMac && macLooksValid(srcMac)) {
    portENTER_CRITICAL(&jobMux);
    memcpy(gMasterMac, srcMac, 6);
    gHaveMaster = true;
    portEXIT_CRITICAL(&jobMux);
    ensurePeer(srcMac);
  }

  if ((gRewardsRx % 4) == 0 && game.partyCount > 0) {
    game.party[0].hp = (uint16_t)(((uint32_t)game.party[0].hp + 2UL + (uint32_t)ar.rewardLevel * 2UL < game.party[0].maxHp) ? ((uint32_t)game.party[0].hp + 2UL + (uint32_t)ar.rewardLevel * 2UL) : game.party[0].maxHp);
    game.party[0].mut = (uint8_t)(((uint32_t)game.party[0].mut + ((ar.aiHint == 3) ? 1UL : 0UL) < 9UL) ? ((uint32_t)game.party[0].mut + ((ar.aiHint == 3) ? 1UL : 0UL)) : 9UL);
    game.dirty = true;
  }

  zimAgentTick("buzz_reward", 0.05f * (float)ar.rewardLevel);

  Serial.printf("[ZIM] agent reward lvl=%u hint=%u batch=%u score=%.1f predErr=%.3f total=%lu\n",
                ar.rewardLevel, ar.aiHint, b, ar.score, ar.predictionError,
                (unsigned long)gRewardsRx);
}

void drainRxQueue() {
  if (!gRxQueue) return;
  JanusRxItem item{};
  for (uint8_t i = 0; i < JANUS_RX_DRAIN_PER_LOOP; ++i) {
    if (xQueueReceive(gRxQueue, &item, 0) != pdTRUE) break;
    if (item.type == RX_JOB && item.len == sizeof(JobPacket)) {
      JobPacket job{};
      memcpy(&job, item.data, sizeof(job));
      handleJobPacket(item.mac, item.rssi, job);
    } else if (item.type == RX_AGENT_REWARD && item.len == sizeof(JanusAgentRewardPacket)) {
      JanusAgentRewardPacket ar{};
      memcpy(&ar, item.data, sizeof(ar));
      handleAgentReward(item.mac, item.rssi, ar);
    } else if (item.type == RX_ZIM_MISSION && item.len == sizeof(ZimMissionPacket)) {
      ZimMissionPacket zm{};
      memcpy(&zm, item.data, sizeof(zm));
      handleZimMission(item.mac, item.rssi, zm);
    }
  }
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
  const uint8_t* src = info ? info->src_addr : nullptr;
  int8_t rssi = 0;
  if (info && info->rx_ctrl) rssi = (int8_t)info->rx_ctrl->rssi;
#else
void onEspNowRecv(const uint8_t *src, const uint8_t *data, int len) {
  int8_t rssi = 0;
#endif
  if (!data || len < 2 || !gRxQueue) return;
  if (len > JANUS_RX_DATA_MAX) {
    gRxOversize++;
    return;
  }

  uint8_t type = RX_NONE;
  if (len == (int)sizeof(JobPacket) && data[0] == 'J' && data[1] == 'B') type = RX_JOB;
  else if (len == (int)sizeof(JanusAgentRewardPacket) && data[0] == 'A' && data[1] == 'R') type = RX_AGENT_REWARD;
  else if (len == (int)sizeof(ZimMissionPacket) && data[0] == 'Z' && data[1] == 'M') type = RX_ZIM_MISSION;
  else return;

  JanusRxItem item{};
  item.type = type;
  item.rssi = rssi;
  item.len = (uint16_t)len;
  if (src) memcpy(item.mac, src, 6);
  memcpy(item.data, data, len);

  if (xQueueSend(gRxQueue, &item, 0) == pdTRUE) {
    gRxQueued++;
    return;
  }
  JanusRxItem dump{};
  (void)xQueueReceive(gRxQueue, &dump, 0);
  if (xQueueSend(gRxQueue, &item, 0) == pdTRUE) gRxQueued++;
  gRxDropped++;
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspNowSent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
  (void)tx_info; (void)status;
}
#else
void onEspNowSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  (void)mac_addr; (void)status;
}
#endif

// ============================================================
// TX
// ============================================================
void sendSwarmSense();

void sendHeartbeat() {
#if !ZIM_SEND_LEGACY_COLONY_HEARTBEAT
  sendSwarmSense();
  return;
#endif
  JanusColonyPacket pkt{};
  strncpy(pkt.magic, "JANUS", sizeof(pkt.magic));
  strncpy(pkt.nodeId, JANUS_NODE_ID, sizeof(pkt.nodeId) - 1);
  strncpy(pkt.role, JANUS_NODE_ROLE, sizeof(pkt.role) - 1);
  pkt.seq = ++gSeq;
  pkt.hashRate = gHashRate;
  pkt.shares = gSharesSent;
  pkt.rejects = gRejects;
  pkt.bestBits = gBestBits;
  pkt.diff = 0.001f;
  pkt.targetBits = gTargetBits;
  pkt.aiBatch = gAiBatch;
  pkt.aiHint = gAiHint;
  pkt.jobAgeMs = gJobActive ? (millis() - gJobRxMs) : (soloJobReady ? (millis() - soloLastJobMs) : 0);
  pkt.rssi = gLastRssi;
  pkt.uptime = millis();
  esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)&pkt, sizeof(pkt));
}

void sendSwarmSense() {
#if ZIM_SEND_SWARMSENSE
  SwarmSensePacket ss{};
  ss.magic[0] = 'S'; ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = workerId();
  strlcpy(ss.nodeId, JANUS_NODE_ID, sizeof(ss.nodeId));
  strlcpy(ss.kind, "zim_slime", sizeof(ss.kind));
  ss.seq = ++gSeq;
  ss.uptime_ms = millis();
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = 0;
  ss.loop_max_us = 0;
  ss.rssi = gLastRssi;
  ss.radio_mode = 1;
  ss.bt_flags = 0;
  ss.palette = (uint8_t)game.mode;
  ss.knn_label = soloAuthorized ? (soloJobReady ? 3 : 2) : (WiFi.status() == WL_CONNECTED ? 1 : 0);
  ss.knn_confidence = zimAgent.confidence ? zimAgent.confidence : (soloJobReady ? 92 : (WiFi.status() == WL_CONNECTED ? 55 : 20));
  ss.ai_hint = (uint8_t)((zimAgent.policy & 0x03) | ((superWeapon.route & 0x03) << 6) | ((zimMissionSector & 0x03) << 2)); // agent policy + SUPER route + sector
  ss.thermal_load = superWeapon.charge;
  ss.effective_batch = zimSoloBatchNow();
  ss.dynamic_batch = gAiBatch;
  ss.hash_rate = gHashRate;
  ss.total_hashes = gTotalHashes;
  ss.best_bits = (uint16_t)((gBestBits < 65535UL) ? gBestBits : 65535UL);
  ss.hash_eff_x1000 = (uint16_t)(((gHashRate / 20UL) < 65535UL) ? (gHashRate / 20UL) : 65535UL);
  ss.prediction_error_x1000 = (int16_t)(gLastPredictionError * 1000.0f);
  ss.entropy_x1000 = (uint16_t)(1000 + ((esp_random() >> 8) & 0x1FF) + ((uint16_t)zimRama.resonance << 1));
  ss.touch_delta = (uint16_t)((zimCurrentMission.mission_id ? zimCurrentMission.mission_id : game.steps) & 0xFFFF);
  ss.job_age_s = zimLastMissionMs ? (uint16_t)((millis() - zimLastMissionMs) / 1000UL) : (soloLastJobMs ? (uint16_t)((millis() - soloLastJobMs) / 1000UL) : 0);
  ss.nonce_remaining_l16 = (uint16_t)(((uint16_t)game.fuel & 0x00FFU) | ((uint16_t)superWeapon.charge << 8));
  ss.flags = (soloPoolConnected ? 0x0001 : 0) | (soloAuthorized ? 0x0002 : 0) | (soloJobReady ? 0x0004 : 0) | (gJobActive ? 0x0008 : 0) | 0x0040 | 0x0800 | (zimRamanujanSignal() > 180 ? 0x1000 : 0);
  if (zimMissionActive || zimMissionPending) ss.flags |= 0x0100;
  if (zimMissionPending) ss.flags |= 0x0200;
  if (zimCurrentMission.flags & 0x00000004UL) ss.flags |= 0x0400;
  if (superWeapon.charge >= 100) ss.flags |= 0x1000;
  if (superWeapon.route == ZW_FLOW || superWeapon.route == ZW_BITE) ss.flags |= 0x2000;
  if (zimAgent.policy == 2 || zimAgent.policy == 3) ss.flags |= 0x4000;
  esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)&ss, sizeof(ss));
#endif
}

void sendEntropy() {
  EntropyReport er{};
  er.magic[0] = 'E'; er.magic[1] = 'R';
  er.worker_id = workerId();
  er.local_entropy = (float)((esp_random() & 0xFFFF) / 65535.0f);
  er.sensor_flags = 0x18; // AI/game telemetry
  er.values[0] = (float)gHashRate;
  er.values[1] = (float)gBestBits;
  er.values[2] = (float)game.captured;
  er.values[3] = (float)zimAgent.confidence;

  // Solo Dominion mode: broadcast telemetry to the swarm, never reply to a Buzz master.
  esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)&er, sizeof(er));
}



// ============================================================
// BUZZ LAZY SIDE-WORKER
// ============================================================
bool sendBuzzShareV2(const uint8_t jobId[8], uint32_t nonce, uint16_t bits, const uint8_t shareHash[32]) {
  ShareResponseV2 sr{};
  sr.magic[0] = 'S'; sr.magic[1] = '2';
  memcpy(sr.job_id, jobId, 8);
  sr.nonce = nonce;
  sr.worker_id = workerId();
  sr.bits = bits;
  sr.total_hashes_l32 = gTotalHashes;
  memcpy(sr.hash_tail, shareHash + 28, 4);

  uint8_t mac[6];
  bool have = false;
  portENTER_CRITICAL(&jobMux);
  have = gHaveMaster;
  memcpy(mac, gMasterMac, 6);
  portEXIT_CRITICAL(&jobMux);

  esp_err_t err = ESP_ERR_ESPNOW_NOT_FOUND;
  if (have && macLooksValid(mac)) {
    ensurePeer(mac);
    err = esp_now_send(mac, (const uint8_t*)&sr, sizeof(sr));
  }
  if (err != ESP_OK) err = esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)&sr, sizeof(sr));
  if (err == ESP_OK) {
    gBuzzSharesSent++;
    gSharesSent = soloAccepts + gBuzzSharesSent;
    gLastShareMs = millis();
    return true;
  }
  gBuzzSharesFail++;
  gRejects = soloRejects + gBuzzSharesFail;
  return false;
}

bool claimBuzzLazyWork(uint8_t outJobId[8], uint8_t outHeader[80], uint8_t outTarget[32], uint32_t* start, uint32_t* count, uint16_t* targetBits) {
#if !ZIM_BUZZ_LAZY_WORKER
  return false;
#else
  uint32_t now = millis();
  if (now - gBuzzLastWorkMs < ZIM_BUZZ_LAZY_EVERY_MS) return false;
  gBuzzLastWorkMs = now;
  uint8_t lazyMask = zimAgent.lazyMask ? zimAgent.lazyMask : ZIM_BUZZ_LAZY_SKIP_MASK;
  if ((esp_random() & lazyMask) != 0) {
    gBuzzLazySkips++;
    return false;
  }

  bool ok = false;
  portENTER_CRITICAL(&jobMux);
  if (gJobActive) {
    if (now - gJobRxMs > ZIM_BUZZ_LAZY_MAX_JOB_AGE_MS) {
      gJobActive = false;
      gNonceRemaining = 0;
    } else if (gNonceRemaining > 0) {
      memcpy(outJobId, gJobId, 8);
      memcpy(outHeader, gHeader, 80);
      memcpy(outTarget, gTarget, 32);
      *targetBits = gTargetBits;
      *start = gNonceCursor;
      uint32_t n = gNonceRemaining < ZIM_BUZZ_LAZY_BATCH ? gNonceRemaining : ZIM_BUZZ_LAZY_BATCH;
      if (n == 0) n = 1;
      gNonceRemaining -= n;
      gNonceCursor -= n;
      *count = n;
      ok = true;
    } else {
      gJobActive = false;
    }
  }
  portEXIT_CRITICAL(&jobMux);
  return ok;
#endif
}

void buzzLazyTick(mbedtls_sha256_context* ctx, uint32_t& hashesThisSecond) {
#if ZIM_BUZZ_LAZY_WORKER
  uint8_t jobId[8], header[80], target[32], hashRaw[32], shareHash[32];
  uint32_t start = 0, count = 0;
  uint16_t targetBits = 0;
  if (!claimBuzzLazyWork(jobId, header, target, &start, &count, &targetBits)) return;

  // Buzz chores stay inside Buzz-assigned range. Zim is lazy, not dishonest: walk backwards by 1.
  for (uint32_t i = 0; i < count; ++i) {
    uint32_t nonce = start - i;
    writeLE32(header + 76, nonce);
    doubleSha256(ctx, header, 80, hashRaw);
    hashToShareOrder(hashRaw, shareHash);
    gTotalHashes++;
    gBuzzLazyHashes++;
    hashesThisSecond++;
    uint16_t bits = countLeadingZeroBits(shareHash);
    updateBestBits(bits);
    if (bits >= 16) superWeaponTick(bits, false);
    if ((bits >= targetBits) && hashMeetsTargetBytes(shareHash, target)) {
      bool ok = sendBuzzShareV2(jobId, nonce, bits, shareHash);
      superWeaponTick(bits, ok);
      zimAgentTick(ok ? "buzz_share" : "buzz_fail", ok ? 0.9f : -0.1f);
      snprintf(game.banner, sizeof(game.banner), "%s", ok ? "Buzz share sluchaino!" : "Buzz share upala");
      snprintf(game.subBanner, sizeof(game.subBanner), "nonce=%08lx bits=%u", (unsigned long)nonce, bits);
      Serial.printf("[ZIM/BUZZ] lazy share %s nonce=%08lx bits=%u target=%u buzzShares=%lu\n",
                    ok ? "sent" : "fail", (unsigned long)nonce, bits, (unsigned)targetBits,
                    (unsigned long)gBuzzSharesSent);
      break;
    }
    if ((i & 0x1F) == 0) taskYIELD();
  }
#endif
}

// ============================================================
// SOLO STRATUM / NERDMINER TASK
// ============================================================
bool buildSoloHeaderFromNotify(JsonDocument& doc, mbedtls_sha256_context* ctx) {
  JsonArray params = doc["params"];
  if (params.size() < 8 || soloExtranonce1.length() == 0) return false;

  String jobId = params[0].as<String>();
  String prevhashHex = params[1].as<String>();
  String coinb1Hex = params[2].as<String>();
  String coinb2Hex = params[3].as<String>();
  JsonArray merkleBranch = params[4];
  String versionHex = params[5].as<String>();
  String nbitsHex = params[6].as<String>();
  String ntimeHex = params[7].as<String>();

  if (jobId.length() <= 0 || prevhashHex.length() != 64 || versionHex.length() != 8 || nbitsHex.length() != 8 || ntimeHex.length() != 8) return false;
  if (soloExtranonce2Size <= 0 || soloExtranonce2Size > 8) soloExtranonce2Size = 4;
  soloExtranonce2++;
  char en2Hex[17];
  formatExtranonce2LE(soloExtranonce2, soloExtranonce2Size, en2Hex, sizeof(en2Hex));

  String coinbase = coinb1Hex + soloExtranonce1 + String(en2Hex) + coinb2Hex;
  int cbLen = coinbase.length() / 2;
  if (cbLen <= 0 || cbLen > 512) return false;

  static uint8_t cbBytes[512];
  hexStringToBytes(coinbase, cbBytes);

  uint8_t mRoot[32];
  doubleSha256(ctx, cbBytes, cbLen, mRoot);

  for (JsonVariant v : merkleBranch) {
    String branchHex = v.as<String>();
    if (branchHex.length() != 64) continue;
    uint8_t branchBytes[32];
    uint8_t concat[64];
    hexStringToBytes(branchHex, branchBytes);
    memcpy(concat, mRoot, 32);
    memcpy(concat + 32, branchBytes, 32);
    doubleSha256(ctx, concat, 64, mRoot);
  }

  uint8_t header[80];
  memset(header, 0, sizeof(header));
  hexStringToBytes(versionHex, header);
  reverse_bytes(header, 4);
  hexStringToBytes(prevhashHex, header + 4);
  reverse_word_bytes(header + 4, 32);
  memcpy(header + 36, mRoot, 32);
  uint8_t ntimeLE[4], nbitsLE[4];
  hexStringToBytes(ntimeHex, ntimeLE); reverse_bytes(ntimeLE, 4);
  hexStringToBytes(nbitsHex, nbitsLE); reverse_bytes(nbitsLE, 4);
  memcpy(header + 68, ntimeLE, 4);
  memcpy(header + 72, nbitsLE, 4);

  portENTER_CRITICAL(&soloMux);
  memcpy(soloHeader, header, 80);
  strlcpy(soloJobId, jobId.c_str(), sizeof(soloJobId));
  strlcpy(soloNtimeHex, ntimeHex.c_str(), sizeof(soloNtimeHex));
  strlcpy(soloEn2Hex, en2Hex, sizeof(soloEn2Hex));
  soloNonce = esp_random() ^ 0xFFFFFFFFUL;
  superWeaponRecomputeRoute(mix32(soloNonce ^ soloExtranonce2));
  soloJobReady = true;
  soloLastJobMs = millis();
  portEXIT_CRITICAL(&soloMux);

  // Mirror solo state for HUD only. Never expose solo Stratum work as a Buzz lazy job.
  portENTER_CRITICAL(&jobMux);
  memcpy(gHeader, header, 80);
  memcpy(gTarget, soloTarget, 32);
  memset(gJobId, 0, sizeof(gJobId));
  memcpy(gJobId, jobId.c_str(), min((int)jobId.length(), 8));
  gJobActive = false;
  gNonceRemaining = 0;
  gRangeSize = 0;
  gTargetBits = soloShareTargetBits;
  portEXIT_CRITICAL(&jobMux);

  game.credits += 1;
  snprintf(game.banner, sizeof(game.banner), "Obratnyi solo-hesh gotov");
  snprintf(game.subBanner, sizeof(game.subBanner), "%s sh=%lu", jobId.c_str(), (unsigned long)superWeapon.reverseStride);
  game.dirty = true;
  setSoloStatus("HASH");
  Serial.printf("[ZIM] solo job %s ready en2=%s targetBits=%u\n", jobId.c_str(), en2Hex, (unsigned)soloShareTargetBits);
  return true;
}

void classifySoloReject(const char* e) {
  if (!e) e = "unknown";
  strlcpy(soloLastError, e, sizeof(soloLastError));
  String s(e); s.toLowerCase();
  if (s.indexOf("low") >= 0 || s.indexOf("target") >= 0 || s.indexOf("difficulty") >= 0) soloLowDiffRejects++;
  else if (s.indexOf("stale") >= 0 || s.indexOf("job") >= 0 || s.indexOf("duplicate") >= 0) soloStaleRejects++;
  else soloOtherRejects++;
}

void soloMinerTask(void* pv) {
  (void)pv;
  WiFiClient client;
  client.setTimeout(160);
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);

  uint8_t header[80], hashRaw[32], shareHash[32], target[32];
  char jobId[96], en2Hex[17], ntimeHex[9];
  uint32_t hashesThisSecond = 0;
  uint32_t lastHashTick = millis();
  uint32_t reconnectHoldUntil = 0;
  uint8_t debugLines = 0;

  setSoloShareTargetFromDifficulty(1.0f);

  for (;;) {
    if (!zimSecretsReady()) {
      soloPoolConnected = false;
      soloAuthorized = false;
      soloJobReady = false;
      setSoloStatus("NO_CFG", "config missing");
      vTaskDelay(pdMS_TO_TICKS(1500));
      continue;
    }

    if (WiFi.status() != WL_CONNECTED) {
      soloPoolConnected = false;
      soloAuthorized = false;
      soloJobReady = false;
      setSoloStatus("WIFI");
      WiFi.disconnect(false, false);
      WiFi.begin(ZIM_WIFI_SSID, ZIM_WIFI_PASSWORD);
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    if (!client.connected()) {
      soloPoolConnected = false;
      soloAuthorized = false;
      soloJobReady = false;
      if (millis() < reconnectHoldUntil) { vTaskDelay(pdMS_TO_TICKS(250)); continue; }
      setSoloStatus("POOL");
      Serial.printf("[ZIM] solo pool connecting %s:%u as %s\n", ZIM_POOL_HOST, ZIM_POOL_PORT, zimMinerUserString().c_str());
      if (client.connect(ZIM_POOL_HOST, ZIM_POOL_PORT)) {
        client.setNoDelay(true);
        client.setTimeout(1000);
        soloPoolConnected = true;
        soloReconnects++;
        debugLines = 0;
        soloExtranonce1 = "";
        soloExtranonce2 = 0;
        client.println(String("{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"") + ZIM_POOL_SUBSCRIBE_TAG + "\"]}");
        String auth = String("{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"") + zimMinerUserString() + "\",\"" + ZIM_POOL_PASSWORD + "\"]}";
        client.println(auth);
        setSoloStatus("SUBAUTH");
      } else {
        soloReconnects++;
        reconnectHoldUntil = millis() + ZIM_POOL_RECONNECT_MIN_MS + ((soloReconnects < 8UL) ? soloReconnects : 8UL) * 1500UL;
        setSoloStatus("POOLFAIL", "connect failed");
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    uint8_t lines = 0;
    while (client.connected() && client.available() && lines < 6) {
      lines++;
      String line = client.readStringUntil('\n');
      line.trim();
      if (!line.length()) continue;
      soloLastPoolRxMs = millis();
      if (debugLines < ZIM_STRATUM_DEBUG_LINES) {
        Serial.print("[ZIM STRATUM] "); Serial.println(line);
        debugLines++;
      }

      StaticJsonDocument<4096> doc;
      DeserializationError err = deserializeJson(doc, line);
      if (err) { Serial.printf("[ZIM] JSON error: %s\n", err.c_str()); continue; }

      if (doc["id"] == 1 && doc["result"].is<JsonArray>()) {
        soloExtranonce1 = doc["result"][1].as<String>();
        soloExtranonce2Size = doc["result"][2].as<int>();
        if (soloExtranonce2Size <= 0 || soloExtranonce2Size > 8) soloExtranonce2Size = 4;
        Serial.printf("[ZIM] subscribed extranonce2_size=%u ex1=%s\n", (unsigned)soloExtranonce2Size, soloExtranonce1.c_str());
        setSoloStatus("AUTH");
      }
      if (doc["id"] == 2) {
        soloAuthorized = (doc["result"] == true);
        Serial.println(soloAuthorized ? "[ZIM] AUTH OK" : "[ZIM] AUTH REJECT");
        setSoloStatus(soloAuthorized ? "AUTHOK" : "AUTHBAD");
      }
      if (doc["id"].is<int>() && doc["id"].as<int>() == 4) {
        if (doc["result"] == true) {
          soloAccepts++;
          soloLastAcceptMs = millis();
          superWeaponOnPoolResult(true);
          zimAgentTick("solo_accept", 1.2f);
          gSharesSent = soloAccepts + gBuzzSharesSent;
          snprintf(game.banner, sizeof(game.banner), "ACCEPT! Zim ne udivil nikogo");
          snprintf(game.subBanner, sizeof(game.subBanner), "shary=%lu", (unsigned long)soloAccepts);
          setSoloStatus("ACCEPT", "-");
          Serial.printf("[ZIM] SOLO ACCEPT accepts=%lu submits=%lu\n", (unsigned long)soloAccepts, (unsigned long)soloSubmits);
        } else {
          soloRejects++;
          gRejects = soloRejects + gBuzzSharesFail;
          superWeaponOnPoolResult(false);
          const char* errText = "unknown";
          if (doc["error"].is<const char*>()) errText = doc["error"].as<const char*>();
          else if (doc["error"].is<JsonArray>() && !doc["error"][1].isNull()) errText = doc["error"][1] | "unknown";
          classifySoloReject(errText);
          setSoloStatus("REJECT", errText);
          Serial.printf("[ZIM] SOLO REJECT rejects=%lu err=%s\n", (unsigned long)soloRejects, errText);
        }
      }
      const char* method = doc["method"] | "";
      if (!strcmp(method, "client.reconnect")) {
        client.stop();
        soloPoolConnected = false;
        soloAuthorized = false;
        soloJobReady = false;
        reconnectHoldUntil = millis() + 15000UL;
        setSoloStatus("RECONN", "pool reconnect");
        continue;
      }
      if (!strcmp(method, "mining.set_extranonce")) {
        soloExtranonce1 = doc["params"][0].as<String>();
        soloExtranonce2Size = doc["params"][1].as<int>();
        if (soloExtranonce2Size <= 0 || soloExtranonce2Size > 8) soloExtranonce2Size = 4;
        soloExtranonce2 = 0;
        soloJobReady = false;
        setSoloStatus("EXNONCE");
      }
      if (!strcmp(method, "mining.set_difficulty")) {
        float diff = doc["params"][0].as<float>();
        setSoloShareTargetFromDifficulty(diff);
        Serial.printf("[ZIM] pool diff=%.8f targetBits=%u\n", diff, (unsigned)soloShareTargetBits);
      }
      if (!strcmp(method, "mining.notify")) {
        buildSoloHeaderFromNotify(doc, &ctx);
      }
    }

    if (client.connected() && soloAuthorized && soloJobReady) {
      portENTER_CRITICAL(&soloMux);
      memcpy(header, soloHeader, 80);
      memcpy(target, soloTarget, 32);
      strlcpy(jobId, soloJobId, sizeof(jobId));
      strlcpy(en2Hex, soloEn2Hex, sizeof(en2Hex));
      strlcpy(ntimeHex, soloNtimeHex, sizeof(ntimeHex));
      uint16_t targetBits = soloShareTargetBits;
      uint32_t nonce = soloNonce;
      portEXIT_CRITICAL(&soloMux);

      uint32_t reverseStride = superWeapon.reverseStride | 1UL;
      const uint16_t batchNow = zimSoloBatchNow();
      for (uint16_t i = 0; i < batchNow; ++i) {
#if ZIM_REVERSE_NONCE_ENGINE
        nonce -= reverseStride;
#else
        nonce++;
#endif
        writeLE32(header + 76, nonce);
        doubleSha256(&ctx, header, 80, hashRaw);
        hashToShareOrder(hashRaw, shareHash);
        gTotalHashes++;
        hashesThisSecond++;
        uint16_t bits = countLeadingZeroBits(shareHash);
        updateBestBits(bits);
        if (bits >= 16) superWeaponTick(bits, false);
        if ((bits >= targetBits) && hashMeetsTargetBytes(shareHash, target)) {
          char nHex[9];
          snprintf(nHex, sizeof(nHex), "%08lx", (unsigned long)nonce);
          String submit = String("{\"id\":4,\"method\":\"mining.submit\",\"params\":[\"") +
                          zimMinerUserString() + "\",\"" + jobId + "\",\"" + en2Hex + "\",\"" + ntimeHex + "\",\"" + nHex + "\"]}";
          client.println(submit);
          soloSubmits++;
          soloLastSubmitMs = millis();
          superWeaponTick(bits, true);
          snprintf(game.banner, sizeof(game.banner), "Zim strelyaet NAZAD");
          snprintf(game.subBanner, sizeof(game.subBanner), "bits=%u cel=%u sh=%lu", bits, targetBits, (unsigned long)reverseStride);
          Serial.printf("[ZIM] SOLO_SUBMIT nonce=%s bits=%u target=%u H=%lu\n", nHex, bits, targetBits, (unsigned long)gHashRate);
          break;
        }
        if ((i & 0x3F) == 0) taskYIELD();
      }
      portENTER_CRITICAL(&soloMux);
      soloNonce = nonce;
      portEXIT_CRITICAL(&soloMux);
    } else {
      vTaskDelay(pdMS_TO_TICKS(30));
    }

    buzzLazyTick(&ctx, hashesThisSecond);

    uint32_t now = millis();
    if (now - lastHashTick >= 1000UL) {
      gHashRate = hashesThisSecond;
      gHashesThisSecond = hashesThisSecond;
      hashesThisSecond = 0;
      lastHashTick = now;
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}


// ============================================================
// STATUS / UI
// ============================================================
uint32_t minerStackWatermark() {
  if (!gMinerTaskHandle) return 0;
  return (uint32_t)uxTaskGetStackHighWaterMark(gMinerTaskHandle);
}

void printStatus() {
  uint32_t age = soloJobReady ? (millis() - soloLastJobMs) : 0;
  uint32_t buzzAge = gJobActive ? (millis() - gJobRxMs) : 0;
  UBaseType_t qNow = gRxQueue ? uxQueueMessagesWaiting(gRxQueue) : 0;
  Serial.printf("[ZIM] solo=%s pool=%u auth=%u ch=%u H=%lu total=%lu submits=%lu acc=%lu rej=%lu best=%lu target=%u buzzJobs=%lu buzzHeld=%lu buzzActive=%u buzzAge=%lu buzzRemain=%lu buzzHash=%lu buzzShare=%lu buzzSkip=%lu soloAge=%lu diff=%.8f rewards=%lu rssi=%d rxQ=%u rxTotal=%lu rxDrop=%lu rxBig=%lu heap=%lu stack=%lu game=%u seen=%lu cap=%lu credits=%lu core2Ignored=%lu sector=%u weapon=%u/%s stride=%lu shots=%lu order=%s err=%s\n",
                soloStatus,
                (unsigned)(soloPoolConnected ? 1 : 0),
                (unsigned)(soloAuthorized ? 1 : 0),
                (unsigned)gCurrentChannel,
                (unsigned long)gHashRate,
                (unsigned long)gTotalHashes,
                (unsigned long)soloSubmits,
                (unsigned long)soloAccepts,
                (unsigned long)soloRejects,
                (unsigned long)gBestBits,
                (unsigned)soloShareTargetBits,
                (unsigned long)gBuzzJobsAccepted,
                (unsigned long)gBuzzJobsHeld,
                (unsigned)(gJobActive ? 1 : 0),
                (unsigned long)buzzAge,
                (unsigned long)gNonceRemaining,
                (unsigned long)gBuzzLazyHashes,
                (unsigned long)gBuzzSharesSent,
                (unsigned long)gBuzzLazySkips,
                (unsigned long)age,
                (double)soloDifficulty,
                (unsigned long)gRewardsRx,
                (int)gLastRssi,
                (unsigned)qNow,
                (unsigned long)gRxQueued,
                (unsigned long)gRxDropped,
                (unsigned long)gRxOversize,
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)minerStackWatermark(),
                (unsigned)game.mode,
                (unsigned long)game.seen,
                (unsigned long)game.captured,
                (unsigned long)game.credits,
                (unsigned long)zimMissionIgnored,
                (unsigned)zimMissionSector,
                (unsigned)superWeapon.charge,
                superWeaponRouteText(),
                (unsigned long)superWeapon.reverseStride,
                (unsigned long)superWeapon.shots,
                zimOrderLine,
                soloLastError);
  Serial.printf("[ZIM/AI] whiteRaven ep=%lu upd=%lu pol=%u conf=%u lazy=0x%02x reward=%.3f loss=%.3f mem=ESP+ESPNow%s\n",
                (unsigned long)zimAgent.epoch, (unsigned long)zimAgent.updates,
                (unsigned)zimAgent.policy, (unsigned)zimAgent.confidence,
                (unsigned)zimAgent.lazyMask, zimAgent.rewardEma, zimAgent.lossEma,
                ZIM_NAS_MEMORY_HTTP ? "+NAS" : "");
  Serial.printf("[ZIM/RAMA] Carr#%lu ch=%u q=%u theta3=%u mock=%d part=%u res=%u conf=%u lemma=%s\n",
                (unsigned long)zimRama.carrIndex,
                (unsigned)zimRama.chapter,
                (unsigned)zimRama.q_x1000,
                (unsigned)zimRama.theta3_x1000,
                (int)zimRama.mock_x1000,
                (unsigned)zimRama.partition_x1000,
                (unsigned)zimRama.resonance,
                (unsigned)zimRama.confidence,
                zimRamanujanLemma());
}

// ============================================================
// CHANNEL SCAN / BUTTON
// ============================================================
void updateChannelScan(uint32_t now) {
#if JANUS_AUTO_CHANNEL_SCAN
  bool locked = gHaveMaster && gLastMasterMs && (now - gLastMasterMs < JANUS_MASTER_LOST_MS);
  if (locked) return;
  if (now - lastScanMs < JANUS_SCAN_STEP_MS) return;
  lastScanMs = now;
  uint8_t next = gCurrentChannel + 1;
  if (next > JANUS_SCAN_MAX_CHANNEL) next = JANUS_SCAN_MIN_CHANNEL;
  setEspNowChannel(next, true);
  sendHeartbeat();
#endif
}

void updateButton(uint32_t now) {
  bool down = (digitalRead(JANUS_BOOT_BUTTON_PIN) == LOW);
  if (down && !bootButtonWasDown) {
    bootButtonDownMs = now;
    bootButtonWasDown = true;
  } else if (!down && bootButtonWasDown) {
    uint32_t held = now - bootButtonDownMs;
    bootButtonWasDown = false;
    if (held >= JANUS_BUTTON_VERY_LONG_MS) {
      clearMasterLock("button very long: Buzz chore dropped");
      zimMissionPending = false;
      sendHeartbeat();
      setBanner("Buzz zadanie brosheno", "Zim uveren chto eto strategiya");
    } else if (held >= JANUS_BUTTON_LONG_MS) {
      cycleZimBrightness();
    } else {
      printStatus();
      // v3.10C/v3.10D: do not switch game.mode to GM_REPORT. Status overlay must not pause autoplay.
      zimStatusOverlay = !zimStatusOverlay;
      if (zimStatusOverlay) setBanner("Ekran statusa", "Igra idet sama pod oknom");
      else setBanner("Status skryt", "Zim snova vidit missiyu");
      game.dirty = true;
    }
  }
}

// ============================================================
// SETUP / LOOP
// ============================================================
void setupDisplay() {
  tftSPI.begin(TFT_SCLK_PIN, -1, TFT_MOSI_PIN, TFT_CS_PIN);
  lcd.init(135, 240);
  lcd.setRotation(JANUS_LCD_ROTATION);
#if JANUS_LCD_INVERT_COLORS
  lcd.invertDisplay(true);
#endif
#if JANUS_LCD_USE_COLROW_OFFSET
  lcd.setColRowStart(JANUS_LCD_COL_OFFSET, JANUS_LCD_ROW_OFFSET);
#endif
  lcd.fillScreen(ST77XX_BLACK);

  tft.setTextSize(1);
  tft.setTextWrap(false);
  tft.fillScreen(rgb(0, 0, 0));
  tft.setCursor(10, 20);
  tft.setTextColor(rgb(120,255,160));
  tft.print("Zim Geek start...");
  pushDisplayFrame();

  Serial.printf("[ZIM] LCD displayfix rotation=%u softBrightness=%u doubleBuffer=1\n",
                (unsigned)JANUS_LCD_ROTATION,
                (unsigned)JANUS_LCD_SOFT_BRIGHTNESS);
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (!zimSecretsReady()) {
    Serial.println("[ZIM] direct config missing/incomplete; set WiFi + wallet before real solo mining");
    setSoloStatus("NO_CFG", "config missing");
    return;
  }
  Serial.printf("[ZIM] WiFi connecting ssid=%s\n", ZIM_WIFI_SSID);
  WiFi.begin(ZIM_WIFI_SSID, ZIM_WIFI_PASSWORD);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 16000UL) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    gCurrentChannel = WiFi.channel();
    Serial.printf("[ZIM] WiFi OK ip=%s channel=%u rssi=%d\n", WiFi.localIP().toString().c_str(), (unsigned)WiFi.channel(), WiFi.RSSI());
    setSoloStatus("WIFI");
  } else {
    Serial.println("[ZIM] WiFi connect failed; ESP-NOW reports still work only if channel matches");
    setSoloStatus("WIFI_FAIL", "wifi failed");
  }
}

void setupEspNow() {
  WiFi.mode(WIFI_STA);
  delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    gCurrentChannel = WiFi.channel();
    Serial.printf("[ZIM] ESP-NOW using WiFi channel=%u\n", (unsigned)gCurrentChannel);
  } else {
    setEspNowChannel(JANUS_BUZZ_WIFI_CHANNEL, true);
  }
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ZIM] ESP-NOW init failed; rebooting");
    delay(1000);
    ESP.restart();
  }
  esp_now_register_recv_cb(onEspNowRecv);
  esp_now_register_send_cb(onEspNowSent);
  ensurePeer(JANUS_BROADCAST_MAC);
}

void setup() {
  Serial.begin(115200);
  delay(800);
  pinMode(JANUS_BOOT_BUTTON_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.printf("[ZIM] JANUS ESP32-S3-GEEK worker %s\n", JANUS_GEEK_VERSION);
  Serial.printf("[ZIM] lore=%s\n", JANUS_NODE_LORE);
  Serial.printf("[ZIM] node=%s role=%s workerId=%u startChannel=%u autoScan=%u\n",
                JANUS_NODE_ID, JANUS_NODE_ROLE, (unsigned)workerId(), JANUS_BUZZ_WIFI_CHANNEL,
                (unsigned)JANUS_AUTO_CHANNEL_SCAN);
  Serial.printf("[ZIM] reset=%s flash=%lu heap=%lu psram=%lu\n",
                resetReasonText(esp_reset_reason()),
                (unsigned long)ESP.getFlashChipSize(),
                (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getPsramSize());

  setupDisplay();
  loadGame();
  loadDisplayPrefs();
  zimSlimeLoadPrefs();
  zimAgentLoadPrefs();
  zimRamanujanLoadPrefs();
  zimRamanujanStudyTick("boot", 0, false);
  setupWiFi();

  gRxQueue = xQueueCreate(JANUS_RX_QUEUE_LEN, sizeof(JanusRxItem));
  if (!gRxQueue) {
    Serial.println("[ZIM] RX queue alloc failed; rebooting");
    delay(1000);
    ESP.restart();
  }

  setupEspNow();

  BaseType_t ok = xTaskCreatePinnedToCore(soloMinerTask, "zim_solo_nerd", ZIM_MINER_STACK_BYTES, nullptr, 1, &gMinerTaskHandle, 0);
  if (ok != pdPASS) {
    Serial.println("[ZIM] miner task create failed; rebooting");
    delay(1000);
    ESP.restart();
  }

  game.mode = GM_BOOT;
  game.stateStartMs = millis();
  game.lastDrawMs = 0;
  game.lastMoveMs = millis();
  game.lastSaveMs = millis();

  Serial.printf("[ZIM] ready: rxQ=%u dataMax=%u SOLO Stratum real wallet + BUZZ hold-safe + RAMA throttled + AUTO-DUNGEON STARVEFIX\n",
                (unsigned)JANUS_RX_QUEUE_LEN, (unsigned)JANUS_RX_DATA_MAX);
  Serial.println("[ZIM] screen: GBA house-base JRPG + auto-dungeon no-filter display");
  Serial.println("[ZIM] canon: Zim razvedchik Zemli; nachalstvo skazalo najti BTC i sekret SHA");
  Serial.println("[ZIM] button: short=status, long=brightness, very-long=drop Buzz chore");
  Serial.printf("[ZIM/RAMA] Carr Synopsis loaded: year=%lu results=%lu lemma=%s\n", (unsigned long)ZIM_RAMANUJAN_CARR_YEAR, (unsigned long)ZIM_RAMANUJAN_CARR_RESULTS, zimRamanujanLemma());
  sendHeartbeat();
  sendZimAgentMemory();
  lastHashRateMs = millis();
  lastScanMs = millis();
}

void loop() {
  uint32_t now = millis();

  drainRxQueue();

  // Solo Stratum task owns gHashRate. Keep this tick only for timing compatibility.
  if (now - lastHashRateMs >= 1000UL) {
    lastHashRateMs = now;
  }

  // Solo Stratum job mirror is not expired by Buzz worker TTL anymore.

  updateChannelScan(now);

  if (now - lastHeartbeatMs >= (JANUS_HEARTBEAT_MS + (workerId() & 0x00FF))) {
    sendHeartbeat();
    lastHeartbeatMs = now;
  }

  if (now - lastSwarmSenseMs >= zimSwarmSenseIntervalMs()) {
    sendSwarmSense();
    lastSwarmSenseMs = now;
  }

  if (now - zimRama.lastStudyMs >= ZIM_RAMANUJAN_STUDY_MS) {
    zimRamanujanStudyTick("periodic", (uint16_t)gBestBits, false);
  }

  if (now - lastAgentMemoryMs >= ZIM_AGENT_MEMORY_MS) {
    zimAgentTick("periodic", 0.0f);
    sendZimAgentMemory();
    lastAgentMemoryMs = now;
  }

  if (zimAgent.dirty && now - lastAgentSaveMs >= ZIM_AGENT_SAVE_MS) {
    zimAgentSavePrefs();
    zimSlimeSavePrefs(false);
    zimRamanujanSavePrefs(false);
    lastAgentSaveMs = now;
  }

  if (now - lastAgentNasMs >= ZIM_AGENT_NAS_MS) {
    zimAgentMaybePostNas();
    lastAgentNasMs = now;
  }

  if (now - lastEntropyMs >= JANUS_ENTROPY_MS) {
    sendEntropy();
    lastEntropyMs = now;
  }

  if (now - lastStatusMs >= JANUS_STATUS_MS) {
    printStatus();
    lastStatusMs = now;
  }

  updateButton(now);
  if (now - superWeapon.lastPulseMs > 3000UL) superWeaponRecomputeRoute(mix32(now ^ gHashRate ^ gBestBits));
  updateGameLogic();
  drawGame();

  if (game.dirty && millis() - game.lastSaveMs > GAME_SAVE_MS) saveGame();
  delay(5);
}
