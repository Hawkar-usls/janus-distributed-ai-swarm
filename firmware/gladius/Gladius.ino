/*
  JANUS_GLADIUS_STA_TEXTCAST_ORACLE_MINER_v1_12_TEXTCAST_GEX_LANGUAGE.ino

  Гладиус без веб-морды и без отдельной точки подключения.

  Что делает:
    - подключается к домашнему интернету:
        Wi-Fi: JANUS_WIFI_PLACEHOLDER
        Pass:  JANUS_NET_PLACEHOLDER

    - НЕ поднимает softAP для подключения;
    - НЕ держит веб-интерфейс;
    - НЕ спамит OXY/PRESENT уведомлениями;
    - общается наружу через fake SSID beacon-поля:
        1) JANUS ORACLE ...
        2) GLAD MINER ...
        3) GLAD SHARE ... только когда реально нашёл share

    - слушает ESP-NOW на канале текущей Wi-Fi сети;
    - принимает старые задания J/G или J/2 от Core/старой ветки;
    - принимает реальные задания Buzz J/B: header[80] + target[32] + nonce range;
    - для Buzz отправляет S/2 share telemetry, для TailGEX/self-job оставляет G/M память;
    - отправляет JANUS heartbeat и S/S SwarmSense, чтобы Buzz видел Gladius как worker;
    - держит адаптивный micro-miner;
    - использует Gemini как ORACLE, но редко и с cooldown;
    - LED управляется как Anchor:
        нормальный режим: зелёно-бирюзовое дыхание
        найден share: белый пик -> зеркальный магента/фиолетовый спад
        Wi-Fi lost: красный мягкий пульс
        job received: янтарный импульс

    - TailGEX allocator-lite:
        измеряет хвостовую чувствительность lane без изменения targetBits/job/wire;
        мягко меняет только lane/stride/batch;
        сохраняет память в NVS и отправляет G/M memory packets Buzz/Core.

  Serial 115200 команды для обслуживания:
    status
    gex
    gex save
    gex send
    gex reset
    say <text>
    oracle
    mode auto
    mode manual
    mode quiet
    job demo
    target 22
    led +
    led -
    led off
    led 72
    reboot

  Arduino IDE:
    Board: ESP32S3 Dev Module
    CPU: 240MHz
    USB CDC On Boot: Enabled
    Arduino-ESP32 3.3.x
*/

#define CORE_DEBUG_LEVEL 0

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_system.h>
#include <esp_log.h>
#include <esp_rom_sys.h>

#include <Preferences.h>
#include <mbedtls/sha256.h>
#include <math.h>

// ============================================================
// Config
// ============================================================

#define GLADIUS_VERSION "GLADIUS_STA_TEXTCAST_ORACLE_MINER_v1_23_PARRY_ONELINE_MINER_BEACON"
#define GLADIUS_NODE_NAME "Gladius"
#define GLADIUS_NODE_ROLE "GLADIUS_STA_TEXTCAST_ORACLE_MINER"

static const char* TOY_WIFI_SSID = "JANUS_WIFI_PLACEHOLDER";
static const char* TOY_WIFI_PASS = "JANUS_NET_PLACEHOLDER";

// Ключ не выводится в логи и не показывается в fake SSID.
static const char* GEMINI_API_KEY = "PUT_YOUR_GEMINI_KEY_HERE";
static const char* GEMINI_MODEL = "gemini-2.0-flash";
static const char* GEMINI_HOST = "https://generativelanguage.googleapis.com/v1beta/models/";

static const uint8_t FALLBACK_CHANNEL = 10;
static const uint32_t SELF_JOB_DISCOVERY_DELAY_MS = 8000UL;
static const uint32_t GLADIUS_WIFI_RECONNECT_MIN_MS = 30000UL;
static const uint32_t GLADIUS_SWARM_CHANNEL_HOLD_MS = 4500UL;
static const uint32_t GLADIUS_RADIO_RESCUE_MIN_MS = 9000UL;
static const uint32_t GLADIUS_RADIO_RX_BLACKOUT_MS = 85000UL;
static const uint32_t GLADIUS_RADIO_MASTER_BLACKOUT_MS = 15000UL;
static const uint32_t GLADIUS_PRESENCE_BURST_MIN_MS = 7500UL;
static const uint32_t GLADIUS_PRESENCE_REFRESH_MS = 14000UL;
static const uint32_t GLADIUS_RADIO_TX_FAIL_DELTA = 5UL;

// Fake SSID антиспам:
static const uint32_t ORACLE_MIN_INTERVAL_MS = 300000UL;      // минимум 5 минут между AI-запросами
static const uint32_t ORACLE_FORCED_INTERVAL_MS = 180000UL;   // минимум 3 минуты даже на важное событие
static const uint32_t MINER_FIELD_INTERVAL_MS = 10000UL;      // v1.23: miner fake SSID всегда живой, обновление раз в 10 сек
static const uint32_t SHARE_FIELD_LIFETIME_MS = 25000UL;      // share fake SSID живёт 25 сек
static const uint32_t EVENT_FIELD_LIFETIME_MS = 18000UL;      // обычный event fake SSID живёт 18 сек
static const uint32_t MACHINE_FIELD_INTERVAL_MS = 2500UL;     // машинный JG|... обновляется спокойно
static const uint32_t SWARM_ADAPT_INTERVAL_MS = 12000UL;      // адаптация по соседям без резких скачков
static const uint32_t NEIGHBOR_TTL_MS = 35000UL;              // сосед жив, если маяк был недавно
static const uint32_t TAIL_GEX_SAVE_INTERVAL_MS = 120000UL;   // flash-save редко, чтобы не убивать NVS
static const uint32_t TAIL_GEX_SEND_INTERVAL_MS = 60000UL;     // отправка памяти Buzz/Core
static const uint32_t TAIL_GEX_RECALC_INTERVAL_MS = 5000UL;    // мягкий пересчёт allocator-слоя
static const uint32_t SELF_JOB_BOOT_DELAY_MS = 60000UL;          // если Buzz молчит минуту, стартуем внутренний ночной job
static const uint32_t SELF_JOB_RESTART_DELAY_MS = 15000UL;      // пауза между self-job
static const uint8_t SELF_JOB_TARGET_BITS = 18;                 // ESP32-friendly внутренний proof target
static const uint32_t SELF_JOB_RANGE = 1048576UL;               // хватает на ночной прогон без частых перезапусков
static const uint8_t TAIL_GEX_EXPLORATION_FLOOR_PCT = 18;      // entropy floor: не даём одному lane съесть scheduler
static const uint8_t TAIL_GEX_MAX_WEIGHT_PCT = 55;             // мягкий потолок веса хвостового лидера
// v1.19 TailGEX Reward Forge:
// GEX теперь получает мягкую награду не только за полный SHARE,
// но и за хороший хвост, рост bestBits, удачный twin-split с Anchor и OXY reward от Buzz.
// Это не меняет target/header/wire — только локальный scheduler lane/stride/batch.
static const uint8_t TAIL_GEX_REWARD_MIN_Z = 14;
static const uint8_t TAIL_GEX_REWARD_LOG_Z = 20;
static const uint8_t TAIL_GEX_REWARD_MAX_STRENGTH = 24;
// v1.20: unlock the scheduler wheel. v1.19 was recording rewards, but the
// pure density formula was too conservative on short ESP32 Buzz slices, so
// weight stayed 0%. These limits keep GEX gentle but actually audible.
static const uint8_t TAIL_GEX_UNLOCK_MIN_WEIGHT_PCT = 10;
static const uint8_t TAIL_GEX_UNLOCK_MIN_CONF_PCT = 35;
static const uint8_t TAIL_GEX_SPLIT_HINT_Z = 16;
static const uint8_t TAIL_GEX_SPLIT_HINT_STRENGTH = 1;

// v1.21 QUIET_RACE:
// The v1.20 control layer worked, but FACE/TEXTCAST/RX/JOB logs became too loud
// during active Buzz races. Keep the same wire protocol and mining logic, but
// reduce serial/log pressure so the miner loop gets more uninterrupted time.
static const uint32_t GLADIUS_FACE_LOG_MIN_MS = 5000UL;
static const uint32_t GLADIUS_FACE_LOG_KEEPALIVE_MS = 9000UL;
// v1.22 QUIET_PACT_SPRINT + GLAD_PARRY:
// - stronger FACE calm, lane-pact cooldown, no pact while BuzzAgent sprint is hot.
// - ORACLE HTTP is deferred while Buzz jobs are active.
// - fake-SSID PARRY field appears for ~1 minute when a station probes/auths/assocs our virtual beacons.
static const uint32_t GLADIUS_TWIN_LANE_PACT_COOLDOWN_MS = 1600UL;
static const uint32_t GLADIUS_REWARD_SPRINT_MS = 26000UL;
static const uint32_t GLADIUS_AFTER_SHARE_PACT_GUARD_MS = 9000UL;
static const uint32_t GLADIUS_PARRY_FIELD_MS = 60000UL;
static const uint32_t GLADIUS_PARRY_SAME_STA_COOLDOWN_MS = 65000UL;
static const uint32_t GLADIUS_PARRY_LOG_MIN_MS = 5000UL;
// v1.23 PARRY one-line: only one temporary PARRY SSID at a time; do not scroll it.
static const uint32_t GLADIUS_PARRY_FIELD_STATIC_SHIFT_MS = 120000UL;
static const uint32_t GLADIUS_PARRY_FIELD_BEACON_MS = 850UL;
static const bool GLADIUS_MINER_FIELD_ALWAYS_ON = true;
static const uint32_t GLADIUS_SWARM_SIGNAL_FIELD_MIN_MS = 12000UL;
static const uint32_t GLADIUS_ACTIVE_JOB_EVENT_FIELD_MS = 2500UL;
static const uint32_t GLADIUS_JOBQ_YIELD_LOG_EVERY = 6UL;
static const uint32_t GLADIUS_BUZZ_JOB_LOG_EVERY = 8UL;
static const uint32_t GLADIUS_DISCOVERY_LOG_EVERY = 12UL;
static const uint32_t GLADIUS_RX_FOREIGN_LOG_EVERY = 900UL;
static const uint32_t GLADIUS_RX_UNKNOWN_LOG_EVERY = 160UL;
static const uint32_t GLADIUS_MINER_REPORT_MS_IDLE = 7000UL;
static const uint32_t GLADIUS_MINER_REPORT_MS_ACTIVE = 9000UL;

static const uint8_t TEXTCAST_GEX_REMOTE_MIN_CONF = 18;              // ниже этого соседский хвост считается шумом
static const int16_t TEXTCAST_GEX_REMOTE_MIN_TAIL_X100 = 120;        // T=+1.20 — минимум для мягкого копирования lane
static const int8_t TEXTCAST_GEX_CLOSE_RSSI = -66;                   // близкий сосед: лучше разойтись по lanes
static const uint8_t TEXTCAST_GEX_MAX_BATCH_BOOST = 14;              // максимум batch boost от JG2-соседей
static const uint32_t BUZZ_BRIDGE_HEARTBEAT_MS = 2200UL;       // JANUS heartbeat для Buzz worker table
static const uint32_t BUZZ_BRIDGE_SWARMSENSE_MS = 4800UL;      // S/S telemetry для Core/Buzz/NAS
static const uint32_t GLADIUS_PN_CORTEX_MS = 5200UL;           // observer-only silicon/body trace
static const uint8_t JANUS_ROLE_GLADIUS_PN = 13;
static const uint32_t BUZZ_JOB_TIMEOUT_MS = 18000UL;           // real Buzz job freshness guard
static const uint16_t BUZZ_BRIDGE_BATCH_MAX = 72;              // верхний предел локального ESP32 batch
// v1.18: Buzz sends many slices of the same Stratum job. Gladius must not reset
// on every same-fingerprint J/B packet, otherwise it never chews a useful window.
static const uint32_t GLADIUS_JOBQ_MIN_HOLD_MS = 320UL;
static const uint32_t GLADIUS_JOBQ_MIN_HASHES = 4096UL;
static const uint32_t GLADIUS_JOBQ_MAX_AGE_MS = 6500UL;
static const uint32_t GLADIUS_JOBQ_LOG_EVERY = 12UL;

// LED
static const int GLADIUS_RGB_LED_PIN = 48;

// Anchor-compatible LED control.
// Short BOOT tap: brightness walks 0 -> max -> 0.
// Very long BOOT hold: toggles UART0 full logs and the small auxiliary/activity LED.
// SHARE: white-gold flash, then soft amber/green face-swap with Anchor.
static const uint8_t GLADIUS_LED_BRIGHTNESS_DEFAULT = 38;
static const uint8_t GLADIUS_BRIGHTNESS_MIN = 0;
static const uint8_t GLADIUS_BRIGHTNESS_MAX = 96;
static const uint8_t GLADIUS_BRIGHTNESS_STEP = 8;
static const uint32_t GLADIUS_LED_SHARE_MS = 9800UL;
static const uint32_t GLADIUS_LED_MAX_FLASH_MS = 900UL;
static const uint32_t GLADIUS_BRIGHTNESS_SAVE_MS = 1500UL;

static const int GLADIUS_BUTTON_PIN = 0;
static const bool GLADIUS_BUTTON_ACTIVE_LOW = true;
static const uint32_t GLADIUS_BUTTON_DEBOUNCE_MS = 35UL;
static const uint32_t GLADIUS_BUTTON_LOG_TOGGLE_MS = 2800UL;

static const int GLADIUS_EXTRA_LED_PIN = 21;
static const bool GLADIUS_EXTRA_LED_ACTIVE_LOW = false;

// JANUS FACE SYNC:
// Gladius and Anchor are treated as two Janus faces.
// Normal: both breathe on one slow amber -> green gradient, but in opposite phase.
// Share/proof: white-gold flash, then the two devices swap gradient direction for a few seconds.
#define JANUS_FACE_SYNC_ENABLE 1
static const uint32_t JANUS_FACE_TX_MS = 2400UL;
static const uint32_t JANUS_FACE_PEER_TTL_MS = 15000UL;
static const uint32_t JANUS_FACE_SWAP_MS = 12000UL;
static const uint8_t JANUS_FACE_AMBER = 0; // v1.16 legacy name: Army Men green face
static const uint8_t JANUS_FACE_GREEN = 1; // v1.16 legacy name: turquoise/cyan twin face
// Backward-compatible names used by the existing face packet logic.
static const uint8_t JANUS_FACE_CYAN = JANUS_FACE_AMBER;
static const uint8_t JANUS_FACE_MAGENTA = JANUS_FACE_GREEN;
static const uint8_t JANUS_FACE_ROLE_GLADIUS = JANUS_FACE_GREEN;
static const uint8_t JANUS_FACE_ROLE_ANCHOR = JANUS_FACE_AMBER;

// JANUS TWIN TASK / BROTHER RACE:
// Direct ESP-NOW pact with Anchor. J/T packets share current Buzz job fingerprint,
// progress, H/s, best tail and SHARE LED handoff. It keeps Buzz wire protocol clean:
// no pool header/target mutation, only local scheduler coordination.
#define JANUS_TWIN_TASK_ENABLE 1
static const uint32_t JANUS_TWIN_TASK_TX_MS = 900UL;
static const uint32_t JANUS_TWIN_TASK_PEER_TTL_MS = 6500UL;
static const uint8_t JANUS_TWIN_ROLE_ANCHOR = 1;
static const uint8_t JANUS_TWIN_ROLE_GLADIUS = 2;
static const uint16_t JANUS_TWIN_FLAG_ACTIVE = 0x0001;
static const uint16_t JANUS_TWIN_FLAG_SHARE = 0x0002;
static const uint16_t JANUS_TWIN_FLAG_SAME_JOB = 0x0004;
static const uint16_t JANUS_TWIN_FLAG_SPLIT = 0x0008;
static const uint16_t JANUS_TWIN_FLAG_ANCHOR = 0x0010;
static const uint16_t JANUS_TWIN_FLAG_GLADIUS = 0x0020;


// ============================================================
// Types
// ============================================================

struct __attribute__((packed)) JanusJobLite {
  uint8_t magic[2];      // 'J','G' or 'J','2'
  uint8_t version;
  uint8_t targetBits;
  uint32_t jobId;
  uint32_t startNonce;
  uint32_t range;
  uint32_t seedA;
  uint32_t seedB;
  uint32_t seedC;
  uint32_t seedD;
  uint8_t lane;
  uint8_t arm;
  uint16_t flags;
  uint32_t stride;
  uint32_t expiresMs;
  uint32_t crc;
};

// Buzz v10.11 colony ABI. This is the real pool-compatible path:
// Buzz sends a complete 80-byte block header template and a big-endian share target.
// Gladius may choose nonce order, lane, stride and batch, but MUST NOT alter header bytes
// except header[76..79] = nonce little-endian.
struct __attribute__((packed)) BuzzJobPacket {
  uint8_t magic[2];       // 'J','B'
  uint8_t job_id[8];      // compact fingerprint of current Stratum job id
  uint8_t header[80];     // Bitcoin block header template; nonce overwritten locally
  uint32_t start_nonce;
  uint32_t range_size;
  uint8_t target[32];     // share target, big-endian compare
  uint32_t extranonce2;
};

struct __attribute__((packed)) BuzzShareResponseV2 {
  uint8_t magic[2];       // 'S','2'
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
  uint16_t bits;
  uint32_t total_hashes_l32;
  uint8_t hash_tail[4];   // last 4 bytes of display-order share hash
};

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

// Common Janus P/N Cortex packet. Observer-only: it describes physical/computational
// traces of honest SHA work and never changes Buzz J/B, S/2 or pool target math.
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
  uint32_t packet_hash;
  uint32_t hash_rate;
  uint32_t total_hashes;
  uint16_t target_bits;
  uint16_t best_bits;
  uint8_t lane;
  uint8_t sector;
  uint8_t flags;           // bit0 job, bit1 Buzz/pool, bit2 BlackStar context, bit3 transition, bit4 policy/memory
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

struct __attribute__((packed)) JanusAgentRewardPacket {
  uint8_t magic[2];       // 'A','R'
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

struct __attribute__((packed)) GladiusStatusPacket {
  uint8_t magic[2];      // 'G','L'
  uint8_t version;
  uint8_t nodeRole;
  uint16_t nodeId;
  uint32_t uptimeMs;
  uint32_t seq;
  uint32_t jobId;
  uint32_t hps;
  uint32_t hpsEma;
  uint32_t totalHashes;
  uint32_t shares;
  uint32_t jobsSeen;
  uint32_t txOk;
  uint32_t txFail;
  uint8_t bestZ;
  uint8_t targetBits;
  uint8_t lane;
  uint8_t rfState;
  uint8_t oxytocin;
  uint8_t led;
  int8_t rssi;
  uint8_t tickerMode;
  uint32_t crc;
};

struct __attribute__((packed)) GladiusSharePacket {
  uint8_t magic[2];      // 'G','S'
  uint8_t version;
  uint8_t nodeRole;
  uint16_t nodeId;
  uint32_t uptimeMs;
  uint32_t seq;
  uint32_t jobId;
  uint32_t nonce;
  uint32_t startNonce;
  uint32_t range;
  uint32_t totalHashes;
  uint8_t zbits;
  uint8_t targetBits;
  uint8_t lane;
  uint8_t arm;
  uint8_t hash8[8];
  uint32_t crc;
};

struct __attribute__((packed)) JanusFaceSyncPacket {
  uint8_t magic[2];      // 'J','F'
  uint8_t version;       // 1
  uint8_t roleFace;      // static/default face of this device
  uint16_t nodeId;
  uint32_t seq;
  uint32_t uptimeMs;
  uint8_t face;          // currently displayed face: 0 cyan, 1 magenta
  uint8_t brightness;
  uint16_t eventBits;    // share/proof bits if active
  uint8_t flags;         // bit0 own share/proof glow, bit1 peer known, bit2 gladius, bit3 anchor
  uint8_t reserved;
  uint32_t colorSeed;
  uint32_t crc;
};

struct __attribute__((packed)) JanusTwinTaskPacket {
  uint8_t magic[2];      // 'J','T' = Janus Twin task/race packet
  uint8_t version;
  uint8_t role;
  uint16_t nodeId;
  uint32_t seq;
  uint32_t uptimeMs;
  uint8_t jobId8[8];
  uint32_t jobFp32;
  uint32_t jobStart;
  uint32_t jobRange;
  uint32_t checked;
  uint32_t nonce;
  uint32_t hashRate;
  uint32_t bestBits;
  uint32_t shares;
  uint32_t jobsSeen;
  uint8_t lane;
  uint8_t sector;
  uint16_t targetBits;
  uint32_t stride;
  uint8_t face;
  uint8_t brightness;
  uint16_t eventBits;
  uint16_t flags;
  int8_t rssi;
  uint8_t reserved;
  uint32_t crc;
};

struct __attribute__((packed)) GladiusCommandPacket {
  uint8_t magic[2];      // 'G','C'
  uint8_t version;
  uint16_t nodeId;
  uint32_t seq;
  uint32_t uptimeMs;
  char command[96];
  uint32_t crc;
};

struct __attribute__((packed)) GladiusMemoryPacket {
  uint8_t magic[2];      // 'G','M'
  uint8_t version;
  uint8_t nodeRole;
  uint16_t nodeId;
  uint32_t uptimeMs;
  uint32_t seq;
  uint32_t jobId;
  uint32_t totalHashes;
  uint32_t shares;
  uint32_t jobsSeen;
  uint8_t bestZ;
  uint8_t targetBits;
  uint8_t activeLane;
  uint8_t gexTopLane;
  int16_t gexTailX100;
  uint8_t gexConfidenceX100;
  uint8_t gexWeightPct;
  uint8_t gexEntropyFloorPct;
  uint32_t memoryEpoch;
  uint16_t flags;        // 1 periodic, 2 save, 4 share, 8 job, 16 high-tail
  uint32_t crc;
};

struct TailGexLaneStats {
  uint32_t hashes;
  // ESP32 micro-tail bins: нужны, чтобы за ночь реально накопить статистику.
  uint16_t z16;
  uint16_t z18;
  uint16_t z20;
  uint16_t z22;
  // Full-tail bins: совместимость с большой V34/V35 логикой.
  uint16_t z24;
  uint16_t z28;
  uint16_t z30;
  uint16_t z32;
  uint16_t z33;
  uint16_t z34;
  uint16_t z35;
  uint16_t z38;
  uint8_t bestZ;
  float densityEma;
  float tailGex;
  float confidence;
  int8_t sign;
  uint8_t weightPct;
};

struct __attribute__((packed)) TailGexPersist {
  uint32_t magic;        // 'GEX1'
  uint16_t version;
  uint16_t nodeId;
  uint32_t memoryEpoch;
  uint32_t totalHashes;
  uint32_t shares;
  uint8_t bestZ;
  uint8_t topLane;
  uint8_t topWeightPct;
  uint8_t reserved;
  uint32_t laneHashes[7];
  uint16_t laneZ16[7];
  uint16_t laneZ18[7];
  uint16_t laneZ20[7];
  uint16_t laneZ22[7];
  uint16_t laneZ24[7];
  uint16_t laneZ28[7];
  uint16_t laneZ30[7];
  uint16_t laneZ32[7];
  uint16_t laneZ33[7];
  uint16_t laneZ34[7];
  uint16_t laneZ35[7];
  uint16_t laneZ38[7];
  uint8_t laneBestZ[7];
  int16_t laneTailX100[7];
  uint8_t laneConfX100[7];
  uint32_t crc;
};

struct NowRxItem {
  uint8_t mac[6];
  int8_t rssi;
  uint8_t len;
  uint8_t data[250];
};

struct WifiEventItem {
  uint8_t type; // 1 connected, 2 got_ip, 3 disconnected
  int32_t reason;
  uint32_t ip;
};

struct SniffEvent {
  uint8_t eventType; // 0=neighbor beacon, 1=GLAD parry interest/probe/auth/assoc
  uint8_t subtype;
  uint8_t bssid[6];
  uint8_t sta[6];
  int8_t rssi;
  uint8_t channel;
  uint8_t ssidLen;
  char ssid[33];
};

struct SwarmNeighbor {
  bool used;
  uint8_t bssid[6];
  uint16_t nodeId;
  uint32_t lastSeenMs;
  int8_t rssi;
  uint16_t hpsEma;
  uint8_t bestZ;
  uint16_t shares;
  uint16_t jobIdShort;
  uint8_t oxy;
  uint16_t seen;

  // TEXTCAST v2 / JG2 хвостовой язык соседей.
  // Это НЕ share и НЕ wire path. Только scheduler scent: lane/stride/batch.
  uint8_t textcastVersion;
  uint8_t lane;
  int16_t tailX100;
  uint8_t confidence;
  uint8_t gexWeightPct;
  bool hasGex;
  bool sameJob;
};

struct TextcastField {
  bool enabled;
  uint8_t bssid[6];
  String text;
  String source;
  size_t bytePos;
  uint32_t lastShiftMs;
  uint32_t lastBeaconMs;
  uint32_t shiftMs;
  uint32_t beaconMs;
  uint32_t untilMs;
  uint32_t sent;
};

// ============================================================
// Globals
// ============================================================

static Preferences prefs;

static String logBuf;
static portMUX_TYPE logMux = portMUX_INITIALIZER_UNLOCKED;

static QueueHandle_t nowQueue = nullptr;
static QueueHandle_t wifiQueue = nullptr;
static QueueHandle_t sniffQueue = nullptr;

static uint16_t nodeId = 0;
static uint32_t gladiusSeq = 0;

static volatile bool wifiOnline = false;
static volatile bool oracleInFlight = false;
static volatile uint8_t wifiChannel = FALLBACK_CHANNEL;
static uint32_t lastWifiConnectMs = 0;
static uint32_t lastWifiLostMs = 0;
static uint32_t wifiReconnectAttempts = 0;

// LED
static uint8_t ledBrightness = GLADIUS_LED_BRIGHTNESS_DEFAULT;
static bool ledOff = false; // derived from ledBrightness == 0, kept for packet compatibility
static uint32_t lastShareMs = 0;
static uint32_t gladiusLastBuzzAgentRewardMs = 0;
static uint16_t lastShareBits = 0;
static uint32_t ledShareFlashUntilMs = 0;
static uint32_t lastMaxBrightnessFlashMs = 0;
static uint32_t ledJobPulseUntilMs = 0;
static uint32_t lastLedMs = 0;
static uint8_t lastLedR = 0, lastLedG = 0, lastLedB = 0;

static uint32_t janusFaceSeq = 0;
static uint32_t janusFaceLastTxMs = 0;
static uint32_t janusFacePeerLastMs = 0;
static uint16_t janusFacePeerNode = 0;
static uint8_t janusFacePeerRoleFace = 255;
static uint8_t janusFacePeerFace = 255;
static uint8_t janusFacePeerFlags = 0;
static uint16_t janusFacePeerEventBits = 0;
static int8_t janusFacePeerRssi = -127;

static uint32_t janusTwinSeq = 0;
static uint32_t janusTwinLastTxMs = 0;
static uint32_t janusTwinPeerLastMs = 0;
static uint16_t janusTwinPeerNode = 0;
static uint8_t janusTwinPeerRole = 0;
static uint32_t janusTwinPeerJobFp32 = 0;
static uint32_t janusTwinPeerJobStart = 0;
static uint32_t janusTwinPeerJobRange = 0;
static uint32_t janusTwinPeerChecked = 0;
static uint32_t janusTwinPeerHashRate = 0;
static uint32_t janusTwinPeerBestBits = 0;
static uint32_t janusTwinPeerShares = 0;
static uint8_t janusTwinPeerLane = 0;
static uint8_t janusTwinPeerSector = 0;
static uint16_t janusTwinPeerTargetBits = 0;
static uint32_t janusTwinPeerStride = 0;
static uint16_t janusTwinPeerFlags = 0;
static int8_t janusTwinPeerRssi = -127;
static uint32_t janusTwinRx = 0;
static uint32_t janusTwinTx = 0;
static uint32_t janusTwinSplitApplied = 0;
static uint32_t janusTwinRaceWins = 0;
static uint32_t janusTwinRaceLosses = 0;

static uint32_t lastButtonSampleMs = 0;
static uint32_t buttonPressStartMs = 0;
static uint32_t buttonLastRepeatMs = 0;
static bool buttonStablePressed = false;
static bool buttonLastRawPressed = false;
static bool buttonLongMode = false;
static bool buttonLogToggleFired = false;
static bool buttonBrightnessDirUp = true;
static uint32_t brightnessChangedMs = 0;
static bool brightnessDirty = false;

static bool extraLedEnabled = false;
static bool extraLedLastState = false;
static bool janusUart0FullLog = false; // OFF = UART0/blue activity LED stays quiet; long BOOT or command u toggles it.

// TextCast fields
enum TextcastMode : uint8_t {
  TEXTCAST_AUTO = 0,
  TEXTCAST_MANUAL = 1,
  TEXTCAST_QUIET = 2
};

enum TextFieldId : uint8_t {
  FIELD_ORACLE = 0,
  FIELD_MINER = 1,
  FIELD_EVENT = 2,
  FIELD_MACHINE = 3,
  FIELD_PARRY = 4,
  FIELD_COUNT = 5
};

static TextcastMode textMode = TEXTCAST_AUTO;
static TextcastField fields[FIELD_COUNT];

static const uint8_t TEXTCAST_MAX_SSID_BYTES = 32;
static const uint8_t TEXTCAST_GAP_SPACES = 6;

// Oracle
static uint32_t lastOracleRequestMs = 0;
static uint32_t oracleOk = 0;
static uint32_t oracleFail = 0;
static bool oracleWanted = false;
static bool oracleForced = false;
static String oracleReason = "boot";
static String oracleLastText = "JANUS ORACLE WAKES";

// ESP-NOW / RF
static const uint8_t ESPNOW_BROADCAST[6] = {0xff,0xff,0xff,0xff,0xff,0xff};

static uint32_t espRx = 0;
static uint32_t espTxOk = 0;
static uint32_t espTxFail = 0;
static uint32_t espUnknown = 0;
static int8_t lastRssi = -127;
static float rssiEma = -80.0f;
static float rfPresence = 0.0f;
static float rfMotion = 0.0f;
static float rfEntropy = 0.0f;
static uint32_t lastRxMs = 0;
static uint32_t lastBuzzMasterMs = 0;
static uint8_t buzzMasterMac[6] = {0};
static bool buzzMasterMacKnown = false;
static uint8_t buzzMasterPeerChannel = 0;
static uint32_t buzzMasterMacSeenMs = 0;
static uint32_t buzzMasterDirectOk = 0;
static uint32_t buzzMasterDirectFail = 0;
static uint32_t buzzMasterMacMissing = 0;
static uint32_t buzzMasterLastLogMs = 0;
static uint32_t radioLastRescueMs = 0;
static uint32_t radioLastWatchMs = 0;
static uint32_t radioLastTxOkSeen = 0;
static uint32_t radioLastTxFailSeen = 0;
static uint32_t radioRescueCount = 0;
static uint32_t gladiusPresenceBursts = 0;
static uint32_t gladiusLastPresenceBurstMs = 0;
static uint32_t gladiusLastPresenceRefreshMs = 0;

enum RfState : uint8_t {
  RF_IDLE = 0,
  RF_MOTION = 1,
  RF_PRESENT = 2,
  RF_ANOMALY = 3
};

static RfState rfState = RF_IDLE;
static float oxytocin = 50.0f;
static float torricelliVacuum = 0.50f;
static uint32_t torricelliBondLastMs = 0;
static uint32_t torricelliOxyBoosts = 0;
static float gladiusTranceptionLiteScore = 0.0f;
static uint8_t gladiusTranceptionHint = 1;
static uint8_t gladiusTranceptionLane = 1; // LANE_ZIM_REVERSE, enum is declared below
static uint32_t gladiusTranceptionLastMs = 0;
static uint32_t gladiusTranceptionReports = 0;

// Miner
enum JanusLane : uint8_t {
  LANE_LINEAR = 0,
  LANE_ZIM_REVERSE = 1,
  LANE_ZIM_BANDIT = 2,
  LANE_JANUS_CENTER = 3,
  LANE_KNIGHT = 4,
  LANE_BITREV = 5,
  LANE_RANDOM = 6
};

static const uint32_t ZIM_STRIDE_ARMS[] = {
  1, 3, 5, 7, 11, 17, 29, 31, 53, 97, 257, 521, 4099, 65537,
  0x9E3779B9UL, 0xC4111903UL, 0x4F1BBCDDUL
};
static const uint8_t ZIM_STRIDE_ARM_COUNT = sizeof(ZIM_STRIDE_ARMS) / sizeof(ZIM_STRIDE_ARMS[0]);

static volatile bool minerHasJob = false;
static volatile bool currentJobIsSelf = false;
static volatile bool currentJobIsBuzz = false;          // true only for real Buzz J/B pool-compatible jobs
static uint8_t currentBuzzJobId[8] = {0};
static uint8_t currentBuzzHeader[80] = {0};
static uint8_t currentBuzzTarget[32] = {0};
static uint32_t currentBuzzExtranonce2 = 0;
static uint32_t buzzJobsSeen = 0;
static BuzzJobPacket queuedBuzzJob;
static uint8_t queuedBuzzJobMac[6] = {0};
static bool queuedBuzzJobValid = false;
static uint32_t queuedBuzzJobFp32 = 0;
static uint32_t queuedBuzzJobQueuedMs = 0;
static uint32_t buzzJobQAccepted = 0;
static uint32_t buzzJobQQueued = 0;
static uint32_t buzzJobQPromoted = 0;
static uint32_t buzzJobQYielded = 0;
static uint32_t buzzJobQDupDrop = 0;
static uint32_t buzzJobQStaleDrop = 0;
static uint32_t buzzSharesSent = 0;
static uint32_t buzzWeakTickets = 0;
static uint32_t buzzAgentRewards = 0;
static uint8_t buzzAgentHint = 1;
static uint8_t buzzAgentLevel = 0;
static uint16_t buzzAgentBatch = 0;
static uint32_t buzzEntropySeed = 0;
static uint32_t colonyHeartbeatSeq = 0;
static uint32_t swarmSenseSeq = 0;
static uint32_t lastColonyHeartbeatMs = 0;
static uint32_t lastSwarmSenseTxMs = 0;
static uint32_t gladiusPnLastMs = 0;
static uint32_t gladiusPnTx = 0;
static uint32_t gladiusPnFail = 0;
static uint32_t gladiusPnPrevHash = 0;
static uint32_t gladiusStatusTaskLastMs = 0;
static uint16_t gladiusLoopJitterUs = 0;
static volatile uint8_t minerLoadPct = 50;

static JanusJobLite currentJob;
static portMUX_TYPE jobMux = portMUX_INITIALIZER_UNLOCKED;

static uint32_t jobsSeen = 0;
static uint32_t realJobsSeen = 0;
static uint32_t selfJobsMade = 0;
static uint32_t sharesFound = 0;
static uint32_t selfProofsFound = 0;
static uint32_t totalHashes = 0;
static uint32_t roundHashes = 0;
static uint32_t lastHps = 0;
static float hpsEma = 0.0f;
static uint8_t bestZ = 0;
static uint32_t bestNonce = 0;
static uint32_t reportBestJobId = 0;
static uint8_t reportBestZ = 0;
static uint32_t reportBestNonce = 0;
static uint32_t reportBestMs = 0;
static uint8_t activeLane = LANE_ZIM_REVERSE;
static uint8_t activeArm = 5;
static uint32_t activeStride = 17;

// Swarm language / passive TextCast sensing
static SwarmNeighbor swarmNeighbors[12];
static uint32_t sniffRx = 0;
static uint32_t sniffJanus = 0;
static uint32_t sniffDrop = 0;
static uint32_t machineBeaconUpdates = 0;
static uint32_t lastMachineFieldMs = 0;
static uint32_t lastSwarmAdaptMs = 0;
static uint8_t swarmNeighborCount = 0;
static uint8_t swarmAvgOxy = 50;
static uint8_t swarmBestRemoteZ = 0;
static uint8_t swarmSameJobCount = 0;
static uint8_t swarmBatchScalePct = 100;
static uint8_t swarmPreferredLane = LANE_ZIM_REVERSE;
static uint32_t swarmPreferredStride = 17;
static uint8_t swarmTextcastGexPeers = 0;
static uint8_t swarmBestGexLane = LANE_ZIM_REVERSE;
static int16_t swarmBestGexX100 = 0;
static uint8_t swarmBestGexConfidence = 0;
static uint16_t swarmBestGexNode = 0;
static uint8_t swarmLaneSplitCount = 0;
static String lastMachineBeacon = "";

// Tail GEX / Tachyon allocator-lite memory.
// Это scheduler-only слой: targetBits, wire, job bytes и submit-протокол не меняет.
static TailGexLaneStats tailGex[7];
static uint32_t tailGexMemoryEpoch = 0;
static uint32_t tailGexLastSaveMs = 0;
static uint32_t tailGexLastSendMs = 0;
static uint32_t tailGexLastRecalcMs = 0;
static uint32_t tailGexObservations = 0;
static uint32_t tailGexProtectedExplores = 0;
static uint32_t tailGexLeaderUses = 0;
static uint8_t tailGexTopLane = LANE_ZIM_REVERSE;
static int16_t tailGexTopX100 = 0;
static uint8_t tailGexTopConfidenceX100 = 0;
static uint8_t tailGexTopWeightPct = 0;
static bool tailGexDirty = false;
static uint32_t tailGexRewardEvents = 0;
static uint32_t tailGexNearMissEvents = 0;
static uint32_t tailGexSplitRewardEvents = 0;
static uint32_t tailGexOxyRewardEvents = 0;
static uint32_t tailGexSelfTailEvents = 0;
static uint32_t tailGexPeerHintEvents = 0;
static uint32_t tailGexWeightUnlockEvents = 0;
static float tailGexRewardBoost[LANE_RANDOM + 1] = {0};
static uint32_t tailGexLastRewardLogMs = 0;

static uint32_t gladiusFaceLastLogMs = 0;
static uint16_t gladiusFaceLastPeer = 0;
static uint8_t gladiusFaceLastFlags = 0;
static uint8_t gladiusFaceLastBits = 0;
static uint8_t gladiusFaceLastMyFace = 255;
static uint8_t gladiusFaceLastSwap = 255;
static uint32_t gladiusFaceSuppressed = 0;
static uint32_t gladiusLastSwarmSignalFieldMs = 0;
static uint32_t gladiusLastBuzzJobEventFieldMs = 0;
static uint32_t gladiusBuzzDiscoveryPings = 0;
static uint32_t gladiusTwinLastLanePactMs = 0;
static uint32_t gladiusTwinLanePactCooldownDrops = 0;
static uint32_t gladiusTwinLanePactSprintDrops = 0;
static uint32_t gladiusParryEvents = 0;
static uint32_t gladiusParrySuppressed = 0;
static uint32_t gladiusParryLastMs = 0;
static uint32_t gladiusParryLastLogMs = 0;
static uint8_t gladiusParryLastSta[6] = {0};
static String gladiusParryLastName = "";


static uint32_t minerLastReportMs = 0;
static uint32_t minerLastFieldMs = 0;
static uint32_t minerLastHashMs = 0;
static uint32_t minerJobStartedMs = 0;
static uint32_t lastRealJobMs = 0;
static uint32_t lastSelfJobMs = 0;
static uint32_t workerCursor = 0;

// Serial command state
static String lastCommand = "";
static String lastCommandResult = "ready";

// ============================================================
// Logging
// ============================================================

static void addLog(const String& s) {
  String line = s;
  if (!line.endsWith("\n")) line += "\n";

  Serial.print(line);
  // UART0 mirror is deliberately optional: when OFF, the small USB-UART activity LED stays calm.
  if (janusUart0FullLog) esp_rom_printf("%s", line.c_str());

  portENTER_CRITICAL(&logMux);
  logBuf += line;
  if (logBuf.length() > 16000) {
    logBuf.remove(0, logBuf.length() - 16000);
  }
  portEXIT_CRITICAL(&logMux);
}

static void logf(const char* tag, const char* fmt, ...) {
  char tmp[520];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(tmp, sizeof(tmp), fmt, ap);
  va_end(ap);
  addLog(String("[") + tag + "] " + tmp);
}

static String macToString(const uint8_t* m);

static bool sendNowBroadcast(const char* tag, const uint8_t* data, size_t len) {
  esp_err_t err = esp_now_send(ESPNOW_BROADCAST, data, len);
  if (err != ESP_OK) {
    espTxFail++;
    if ((espTxFail % 8UL) == 1UL) {
      logf("ESPNOW/TXFAIL", "tag=%s err=%d fail=%lu ok=%lu rescues=%lu",
           tag ? tag : "?", (int)err, (unsigned long)espTxFail,
           (unsigned long)espTxOk, (unsigned long)radioRescueCount);
    }
    return false;
  }
  return true;
}

static bool janusMacUsable(const uint8_t* mac) {
  if (!mac) return false;
  bool allZero = true;
  bool allFf = true;
  for (uint8_t i = 0; i < 6; ++i) {
    if (mac[i] != 0x00) allZero = false;
    if (mac[i] != 0xFF) allFf = false;
  }
  return !allZero && !allFf;
}

static void rememberBuzzMasterMac(const uint8_t* mac, const char* reason) {
  uint32_t now = nowMs();
  if (!janusMacUsable(mac)) {
    buzzMasterMacMissing++;
    return;
  }
  bool changed = !buzzMasterMacKnown || memcmp(buzzMasterMac, mac, 6) != 0;
  memcpy(buzzMasterMac, mac, 6);
  buzzMasterMacKnown = true;
  buzzMasterMacSeenMs = now;
  if (changed) buzzMasterPeerChannel = 0;

  if (changed || now - buzzMasterLastLogMs > 30000UL) {
    buzzMasterLastLogMs = now;
    logf("RADIO/MASTER", "mac=%s reason=%s ch=%u direct=%lu/%lu missing=%lu",
         macToString(buzzMasterMac).c_str(),
         reason ? reason : "-",
         (unsigned)wifiChannel,
         (unsigned long)buzzMasterDirectOk,
         (unsigned long)buzzMasterDirectFail,
         (unsigned long)buzzMasterMacMissing);
  }
}

static bool ensureBuzzMasterPeer(const char* reason) {
  if (!buzzMasterMacKnown || !janusMacUsable(buzzMasterMac)) return false;
  uint8_t ch = wifiChannel ? wifiChannel : FALLBACK_CHANNEL;
  bool exists = esp_now_is_peer_exist(buzzMasterMac);
  if (exists && buzzMasterPeerChannel == ch) return true;
  if (exists) esp_now_del_peer(buzzMasterMac);

  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, buzzMasterMac, 6);
  peer.channel = ch;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;

  esp_err_t err = esp_now_add_peer(&peer);
  if (err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST) {
    buzzMasterPeerChannel = ch;
    return true;
  }

  buzzMasterPeerChannel = 0;
  logf("RADIO/MASTER", "peer add fail err=%d mac=%s ch=%u reason=%s",
       (int)err, macToString(buzzMasterMac).c_str(), (unsigned)ch, reason ? reason : "-");
  return false;
}

static esp_err_t sendNowToBuzzMaster(const char* tag, const uint8_t* data, size_t len) {
  if (!data || len == 0) return ESP_ERR_ESPNOW_ARG;
  if (!buzzMasterMacKnown || !janusMacUsable(buzzMasterMac)) return ESP_ERR_INVALID_STATE;
  uint32_t now = nowMs();
  if (buzzMasterMacSeenMs && now - buzzMasterMacSeenMs > 180000UL) return ESP_ERR_INVALID_STATE;
  if (!ensureBuzzMasterPeer(tag ? tag : "direct")) return ESP_ERR_ESPNOW_NOT_FOUND;
  esp_err_t err = esp_now_send(buzzMasterMac, data, len);
  if (err == ESP_OK) buzzMasterDirectOk++;
  else buzzMasterDirectFail++;
  return err;
}

// ============================================================
// Utility
// ============================================================

static uint32_t nowMs() {
  return (uint32_t)millis();
}

static void gladiusReportBestReset(uint32_t jobId) {
  reportBestJobId = jobId;
  reportBestZ = 0;
  reportBestNonce = 0;
  reportBestMs = nowMs();
}

static void gladiusReportBestObserve(uint32_t jobId, uint8_t z, uint32_t nonce) {
  if (!jobId) jobId = currentJob.jobId;
  if (reportBestJobId != jobId) gladiusReportBestReset(jobId);
  if (z > reportBestZ || (z == reportBestZ && reportBestNonce == 0)) {
    reportBestZ = z;
    reportBestNonce = nonce;
    reportBestMs = nowMs();
  }
}

static uint8_t gladiusReportBestBits() {
  if (currentJobIsBuzz && currentJob.jobId && reportBestJobId == currentJob.jobId && reportBestZ > bestZ) {
    return reportBestZ;
  }
  return bestZ;
}

static uint32_t gladiusReportBestNonce() {
  uint8_t reportBits = gladiusReportBestBits();
  if (reportBits == reportBestZ && reportBestNonce) return reportBestNonce;
  return bestNonce;
}

static uint32_t janusSafeAgeMs(uint32_t now, uint32_t then, uint32_t fallback = 999999UL) {
  if (then == 0) return fallback;
  if (now >= then) return now - then;
  return 0;
}

static bool janusSafeElapsed(uint32_t now, uint32_t then, uint32_t interval) {
  if (then == 0) return interval == 0;
  return janusSafeAgeMs(now, then, 0) >= interval;
}

static bool gladiusBuzzJobActiveNow() {
  bool active = false;
  portENTER_CRITICAL(&jobMux);
  active = minerHasJob && currentJobIsBuzz;
  portEXIT_CRITICAL(&jobMux);
  return active;
}

static bool gladiusRewardSprintActiveNow() {
  uint32_t now = nowMs();
  return janusSafeAgeMs(now, gladiusLastBuzzAgentRewardMs, 999999UL) < GLADIUS_REWARD_SPRINT_MS ||
         janusSafeAgeMs(now, lastShareMs, 999999UL) < GLADIUS_AFTER_SHARE_PACT_GUARD_MS;
}

static String macToString(const uint8_t* m) {
  char b[24];
  snprintf(b, sizeof(b), "%02X:%02X:%02X:%02X:%02X:%02X", m[0], m[1], m[2], m[3], m[4], m[5]);
  return String(b);
}

static uint32_t fnv1a32(const uint8_t* data, size_t len, uint32_t h = 2166136261UL) {
  for (size_t i = 0; i < len; i++) {
    h ^= data[i];
    h *= 16777619UL;
  }
  return h;
}

static uint32_t crc32ish(const void* data, size_t len) {
  return fnv1a32((const uint8_t*)data, len, 0x811C9DC5UL);
}

static bool janusFacePeerFresh() {
  return janusFacePeerLastMs && (nowMs() - janusFacePeerLastMs < JANUS_FACE_PEER_TTL_MS);
}

static bool janusFaceShareActive() {
  bool own = lastShareMs && (nowMs() - lastShareMs < JANUS_FACE_SWAP_MS);
  bool peer = janusFacePeerLastMs &&
              (nowMs() - janusFacePeerLastMs < JANUS_FACE_SWAP_MS) &&
              (janusFacePeerFlags & 0x01);
  return own || peer;
}

static uint8_t janusFaceBaseFace() {
  uint8_t base = JANUS_FACE_ROLE_GLADIUS;

  // If the twin reports the same static face, split by nodeId so two identical
  // boards still cannot breathe with the same face.
  if (janusFacePeerFresh() && janusFacePeerRoleFace == JANUS_FACE_ROLE_GLADIUS && janusFacePeerNode) {
    base = (nodeId < janusFacePeerNode) ? JANUS_FACE_CYAN : JANUS_FACE_MAGENTA;
  }

  return base;
}

static uint8_t janusFaceCurrentFace() {
  uint8_t face = janusFaceBaseFace();
  if (janusFaceShareActive()) face ^= 1;
  return face & 1;
}

static float janusFacePhase(uint32_t now) {
  uint32_t seed = ((uint32_t)nodeId * 977UL) ^ 0x4A4E5553UL;
  uint32_t offs = 700UL + (seed % 2600UL);
  if (janusFacePeerFresh() && nodeId > janusFacePeerNode) offs += 2600UL;
  // Slow, soft breathing. One full perceived wave is roughly 45+ seconds.
  return (sinf(((float)now + (float)offs) / 7800.0f) + 1.0f) * 0.5f;
}

static void janusFaceBroadcast(bool eventNow, uint16_t eventBits) {
#if JANUS_FACE_SYNC_ENABLE
  JanusFaceSyncPacket jf;
  memset(&jf, 0, sizeof(jf));
  jf.magic[0] = 'J';
  jf.magic[1] = 'F';
  jf.version = 1;
  jf.roleFace = JANUS_FACE_ROLE_GLADIUS;
  jf.nodeId = nodeId;
  jf.seq = ++janusFaceSeq;
  jf.uptimeMs = nowMs();
  jf.face = janusFaceCurrentFace();
  jf.brightness = ledBrightness;
  jf.eventBits = eventNow ? eventBits : (janusFaceShareActive() ? lastShareBits : 0);
  jf.flags = 0x04; // gladius
  if (eventNow || (lastShareMs && nowMs() - lastShareMs < JANUS_FACE_SWAP_MS)) jf.flags |= 0x01;
  if (janusFacePeerFresh()) jf.flags |= 0x02;
  jf.colorSeed = ((uint32_t)nodeId << 16) ^ ESP.getCycleCount() ^ 0xFACE0137UL;
  jf.crc = 0;
  jf.crc = crc32ish(&jf, sizeof(jf) - 4);
  sendNowBroadcast("J/F", (const uint8_t*)&jf, sizeof(jf));
  janusFaceLastTxMs = nowMs();
#endif
}

static bool janusFaceReceive(const uint8_t* data, int len, int8_t rssi) {
#if JANUS_FACE_SYNC_ENABLE
  if (!data || len != (int)sizeof(JanusFaceSyncPacket) || data[0] != 'J' || data[1] != 'F') return false;

  JanusFaceSyncPacket jf;
  memcpy(&jf, data, sizeof(jf));
  uint32_t got = jf.crc;
  jf.crc = 0;
  uint32_t calc = crc32ish(&jf, sizeof(jf) - 4);
  if (got && got != calc) return true; // it was J/F, but corrupted; consume silently.

  if (jf.nodeId == nodeId) return true;

  bool first = !janusFacePeerLastMs || janusFacePeerNode != jf.nodeId;
  janusFacePeerLastMs = nowMs();
  janusFacePeerNode = jf.nodeId;
  janusFacePeerRoleFace = jf.roleFace;
  janusFacePeerFace = jf.face;
  janusFacePeerFlags = jf.flags;
  janusFacePeerEventBits = jf.eventBits;
  janusFacePeerRssi = rssi;
  lastLedMs = 0;

  uint32_t now = nowMs();
  uint8_t myFaceNow = janusFaceCurrentFace();
  uint8_t swapNow = janusFaceShareActive() ? 1 : 0;
  bool majorChange = first ||
                     gladiusFaceLastPeer != jf.nodeId ||
                     (jf.eventBits > 0 && gladiusFaceLastBits != jf.eventBits) ||
                     gladiusFaceLastSwap != swapNow;
  bool minorChange = gladiusFaceLastFlags != jf.flags ||
                     gladiusFaceLastMyFace != myFaceNow;
  uint32_t faceLogAge = janusSafeAgeMs(now, gladiusFaceLastLogMs, 999999UL);
  bool eventActive = (jf.flags & 0x01) != 0;
  if ((majorChange && faceLogAge >= 450UL) ||
      (minorChange && faceLogAge >= GLADIUS_FACE_LOG_MIN_MS) ||
      (eventActive && faceLogAge >= GLADIUS_FACE_LOG_MIN_MS) ||
      faceLogAge >= GLADIUS_FACE_LOG_KEEPALIVE_MS) {
    logf("FACE", "peer=%04X face=%u roleFace=%u flags=0x%02X bits=%u rssi=%d myFace=%u swap=%u quietDrop=%lu calm=1",
         jf.nodeId, jf.face, jf.roleFace, jf.flags, jf.eventBits, rssi,
         myFaceNow, swapNow, (unsigned long)gladiusFaceSuppressed);
    gladiusFaceLastLogMs = now;
    gladiusFaceLastPeer = jf.nodeId;
    gladiusFaceLastFlags = jf.flags;
    gladiusFaceLastBits = jf.eventBits;
    gladiusFaceLastMyFace = myFaceNow;
    gladiusFaceLastSwap = swapNow;
    gladiusFaceSuppressed = 0;
  } else {
    gladiusFaceSuppressed++;
  }

  return true;
#else
  (void)data; (void)len; (void)rssi;
  return false;
#endif
}

static void janusFaceTick(uint32_t now) {
#if JANUS_FACE_SYNC_ENABLE
  bool urgent = janusFaceShareActive() && (now - janusFaceLastTxMs > 350UL);
  if (urgent || now - janusFaceLastTxMs >= JANUS_FACE_TX_MS || janusFaceLastTxMs == 0) {
    janusFaceBroadcast(false, 0);
  }
#else
  (void)now;
#endif
}


static bool janusTwinPeerFresh() {
#if JANUS_TWIN_TASK_ENABLE
  return janusTwinPeerLastMs && janusSafeAgeMs(nowMs(), janusTwinPeerLastMs, 999999UL) < JANUS_TWIN_TASK_PEER_TTL_MS;
#else
  return false;
#endif
}

static bool janusTwinSameJobNow() {
#if JANUS_TWIN_TASK_ENABLE
  if (!janusTwinPeerFresh()) return false;
  if (!minerHasJob || !currentJobIsBuzz) return false;
  // v1.18: same Buzz work is identified by the stable J/B fingerprint.
  // Buzz deliberately gives brothers different startNonce windows, so start/range
  // must NOT break SAME_JOB/SPLIT.
  if (!currentJob.jobId || currentJob.jobId != janusTwinPeerJobFp32) return false;
  return true;
#else
  return false;
#endif
}

static void janusTwinTaskBroadcast(bool force=false) {
#if JANUS_TWIN_TASK_ENABLE
  uint32_t now = nowMs();
  bool active = minerHasJob && currentJobIsBuzz;
  bool shareActive = lastShareMs && (now - lastShareMs < JANUS_FACE_SWAP_MS);
  bool urgent = shareActive || active || janusTwinPeerFresh();
  uint32_t interval = urgent ? JANUS_TWIN_TASK_TX_MS : (JANUS_TWIN_TASK_TX_MS * 4UL);
  if (!force && now - janusTwinLastTxMs < interval) return;
  janusTwinLastTxMs = now;

  JanusTwinTaskPacket jt;
  memset(&jt, 0, sizeof(jt));
  jt.magic[0] = 'J'; jt.magic[1] = 'T'; jt.version = 1;
  jt.role = JANUS_TWIN_ROLE_GLADIUS;
  jt.nodeId = nodeId;
  jt.seq = ++janusTwinSeq;
  jt.uptimeMs = now;
  if (active) memcpy(jt.jobId8, currentBuzzJobId, 8);
  jt.jobFp32 = active ? currentJob.jobId : 0;
  jt.jobStart = active ? currentJob.startNonce : 0;
  jt.jobRange = active ? currentJob.range : 0;
  jt.checked = active ? workerCursor : 0;
  uint8_t reportBestBits = gladiusReportBestBits();
  jt.nonce = gladiusReportBestNonce();
  jt.hashRate = lastHps;
  jt.bestBits = reportBestBits;
  jt.shares = sharesFound;
  jt.jobsSeen = jobsSeen;
  jt.lane = active ? currentJob.lane : 255;
  jt.sector = 0;
  jt.targetBits = active ? currentJob.targetBits : 0;
  jt.stride = active ? currentJob.stride : 0;
  jt.face = janusFaceCurrentFace();
  jt.brightness = ledBrightness;
  jt.eventBits = shareActive ? lastShareBits : 0;
  jt.flags = JANUS_TWIN_FLAG_GLADIUS;
  if (active) jt.flags |= JANUS_TWIN_FLAG_ACTIVE;
  if (shareActive) jt.flags |= JANUS_TWIN_FLAG_SHARE;
  if (janusTwinSameJobNow()) jt.flags |= JANUS_TWIN_FLAG_SAME_JOB | JANUS_TWIN_FLAG_SPLIT;
  jt.rssi = lastRssi;
  jt.crc = 0;
  jt.crc = crc32ish(&jt, sizeof(jt) - 4);
  esp_err_t err = esp_now_send(ESPNOW_BROADCAST, (const uint8_t*)&jt, sizeof(jt));
  if (err == ESP_OK) espTxOk++; else espTxFail++;
  janusTwinTx++;
  if (force || shareActive || (janusTwinTx % 12UL) == 1UL || err != ESP_OK) {
    const char* race = "solo";
    if (janusTwinPeerFresh() && active && currentJob.jobId == janusTwinPeerJobFp32) {
      if (reportBestBits > janusTwinPeerBestBits) race = "ahead";
      else if (reportBestBits < janusTwinPeerBestBits) race = "behind";
      else race = (lastHps >= janusTwinPeerHashRate) ? "speed_ahead" : "speed_behind";
    }
    logf("TWIN", "tx=%s seq=%lu peer=%04X fresh=%u job=%u fp=%08lX checked=%lu H=%lu best=%u peerBest=%lu race=%s flags=0x%04X",
         err == ESP_OK ? "OK" : "FAIL", (unsigned long)jt.seq, janusTwinPeerNode,
         janusTwinPeerFresh() ? 1 : 0, active ? 1 : 0, (unsigned long)jt.jobFp32,
         (unsigned long)jt.checked, (unsigned long)lastHps, reportBestBits,
         (unsigned long)janusTwinPeerBestBits, race, jt.flags);
  }
#else
  (void)force;
#endif
}

static bool janusTwinTaskReceive(const uint8_t* data, int len, int8_t rssi) {
#if JANUS_TWIN_TASK_ENABLE
  if (!data || len != (int)sizeof(JanusTwinTaskPacket) || data[0] != 'J' || data[1] != 'T') return false;
  JanusTwinTaskPacket jt;
  memcpy(&jt, data, sizeof(jt));
  uint32_t got = jt.crc;
  jt.crc = 0;
  uint32_t calc = crc32ish(&jt, sizeof(jt) - 4);
  if (got && got != calc) return true;
  if (jt.nodeId == nodeId) return true;

  janusTwinRx++;
  bool first = !janusTwinPeerLastMs || janusTwinPeerNode != jt.nodeId;
  bool peerShare = (jt.flags & JANUS_TWIN_FLAG_SHARE);
  uint32_t prevPeerH = janusTwinPeerHashRate;
  uint32_t prevPeerShares = janusTwinPeerShares;
  uint32_t prevPeerBest = janusTwinPeerBestBits;
  bool peerZeroIsTransition = (jt.hashRate == 0 && prevPeerH > 0 &&
                               (peerShare || jt.shares > prevPeerShares || jt.bestBits >= prevPeerBest || jt.bestBits >= 20));
  janusTwinPeerLastMs = nowMs();
  janusTwinPeerNode = jt.nodeId;
  janusTwinPeerRole = jt.role;
  janusTwinPeerJobFp32 = jt.jobFp32;
  janusTwinPeerJobStart = jt.jobStart;
  janusTwinPeerJobRange = jt.jobRange;
  janusTwinPeerChecked = jt.checked;
  janusTwinPeerHashRate = peerZeroIsTransition ? prevPeerH : jt.hashRate;
  janusTwinPeerBestBits = jt.bestBits;
  janusTwinPeerShares = jt.shares;
  janusTwinPeerLane = jt.lane;
  janusTwinPeerSector = jt.sector;
  janusTwinPeerTargetBits = jt.targetBits;
  janusTwinPeerStride = jt.stride;
  janusTwinPeerFlags = jt.flags;
  janusTwinPeerRssi = rssi;

  janusFacePeerLastMs = nowMs();
  janusFacePeerNode = jt.nodeId;
  janusFacePeerRoleFace = (jt.role == JANUS_TWIN_ROLE_ANCHOR) ? JANUS_FACE_ROLE_ANCHOR : JANUS_FACE_ROLE_GLADIUS;
  janusFacePeerFace = jt.face;
  janusFacePeerFlags = peerShare ? 0x01 : 0x00;
  janusFacePeerEventBits = jt.eventBits;
  janusFacePeerRssi = rssi;
  lastLedMs = 0;

  if (minerHasJob && currentJobIsBuzz && currentJob.jobId == jt.jobFp32) {
    uint8_t myReportBestBits = gladiusReportBestBits();
    if (myReportBestBits > jt.bestBits) janusTwinRaceWins++;
    else if (myReportBestBits < jt.bestBits) janusTwinRaceLosses++;
  }

  if (first || peerShare || (janusTwinRx % 10UL) == 1UL) {
    uint8_t myReportBestBits = gladiusReportBestBits();
    logf("TWIN", "rx peer=%04X role=%u job=%u fp=%08lX H=%lu best=%lu shares=%lu flags=0x%04X rssi=%d myBest=%u wins=%lu losses=%lu",
         jt.nodeId, jt.role, (jt.flags & JANUS_TWIN_FLAG_ACTIVE) ? 1 : 0,
         (unsigned long)jt.jobFp32, (unsigned long)janusTwinPeerHashRate, (unsigned long)jt.bestBits,
         (unsigned long)jt.shares, jt.flags, rssi, myReportBestBits,
         (unsigned long)janusTwinRaceWins, (unsigned long)janusTwinRaceLosses);
  }
  return true;
#else
  (void)data; (void)len; (void)rssi;
  return false;
#endif
}

static void janusTwinTaskTick(uint32_t now) {
#if JANUS_TWIN_TASK_ENABLE
  (void)now;
  janusTwinTaskBroadcast(false);
#else
  (void)now;
#endif
}


static uint32_t bitReverse32(uint32_t x) {
  x = ((x & 0x55555555UL) << 1) | ((x >> 1) & 0x55555555UL);
  x = ((x & 0x33333333UL) << 2) | ((x >> 2) & 0x33333333UL);
  x = ((x & 0x0F0F0F0FUL) << 4) | ((x >> 4) & 0x0F0F0F0FUL);
  x = ((x & 0x00FF00FFUL) << 8) | ((x >> 8) & 0x00FF00FFUL);
  x = (x << 16) | (x >> 16);
  return x;
}

static uint8_t leadingZeroBits(const uint8_t hash[32]) {
  uint8_t z = 0;
  for (int i = 0; i < 32; i++) {
    uint8_t b = hash[i];
    if (b == 0) {
      z += 8;
    } else {
      for (int bit = 7; bit >= 0; bit--) {
        if (b & (1 << bit)) return z;
        z++;
      }
    }
  }
  return 255;
}

static uint16_t countLeadingZeroBitsBE(const uint8_t hashBE[32]) {
  return (uint16_t)leadingZeroBits(hashBE);
}

static void writeLE32(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void hashToShareOrder(const uint8_t rawDigest[32], uint8_t outBE[32]) {
  for (int i = 0; i < 32; ++i) outBE[i] = rawDigest[31 - i];
}

static bool hashMeetsTargetBE(const uint8_t hashBE[32], const uint8_t targetBE[32]) {
  for (int i = 0; i < 32; ++i) {
    if (hashBE[i] < targetBE[i]) return true;
    if (hashBE[i] > targetBE[i]) return false;
  }
  return true;
}

static uint32_t jobId32From8(const uint8_t jid[8]) {
  return fnv1a32(jid, 8, 0xB00B5EEDUL);
}

static void sha256d(const uint8_t* data, size_t len, uint8_t out[32]) {
  uint8_t tmp[32];

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, data, len);
  mbedtls_sha256_finish(&ctx, tmp);
  mbedtls_sha256_free(&ctx);

  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, tmp, 32);
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);
}

static size_t utf8CharLenAt(const String& s, size_t idx) {
  if (idx >= s.length()) return 0;
  uint8_t c = (uint8_t)s[idx];
  if ((c & 0x80) == 0) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  if ((c & 0xF8) == 0xF0) return 4;
  return 1;
}

static bool isUtf8Continuation(uint8_t c) {
  return (c & 0xC0) == 0x80;
}

static size_t clampUtf8BoundaryBack(const String& s, size_t idx) {
  if (idx > s.length()) idx = s.length();
  while (idx > 0 && isUtf8Continuation((uint8_t)s[idx])) idx--;
  return idx;
}

static size_t nextUtf8Boundary(const String& s, size_t idx) {
  if (idx >= s.length()) return s.length();
  size_t n = utf8CharLenAt(s, idx);
  if (n == 0) return s.length();
  size_t out = idx + n;
  if (out > s.length()) out = s.length();
  return out;
}

static String jsonEscape(const String& in) {
  String out;
  out.reserve(in.length() + 16);

  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '\\') out += "\\\\";
    else if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') out += "\\r";
    else if (c == '\t') out += "\\t";
    else out += c;
  }

  return out;
}

static String sanitizeSSIDAscii(String in, size_t maxBytes) {
  in.trim();

  String out;
  out.reserve(maxBytes + 2);

  for (size_t i = 0; i < in.length() && out.length() < maxBytes; i++) {
    char c = in[i];

    if (c >= 'a' && c <= 'z') c = c - 32;

    bool ok = false;
    if (c >= 'A' && c <= 'Z') ok = true;
    if (c >= '0' && c <= '9') ok = true;
    if (c == ' ' || c == '-' || c == '_' || c == ':' || c == '.' || c == '/') ok = true;

    if (ok) out += c;
  }

  out.trim();
  if (out.length() == 0) out = "JANUS LISTENS";
  if (out.length() > maxBytes) out.remove(maxBytes);

  return out;
}

static String parseGeminiText(const String& response) {
  int k = response.indexOf("\"text\"");
  if (k < 0) return "";

  int colon = response.indexOf(':', k);
  if (colon < 0) return "";

  int q = response.indexOf('"', colon + 1);
  if (q < 0) return "";

  String out;
  bool esc = false;

  for (int i = q + 1; i < (int)response.length(); i++) {
    char c = response[i];

    if (esc) {
      if (c == 'n' || c == 'r' || c == 't') out += ' ';
      else out += c;
      esc = false;
      continue;
    }

    if (c == '\\') {
      esc = true;
      continue;
    }

    if (c == '"') break;
    out += c;
  }

  out.trim();
  return out;
}


static bool findMachineToken(const String& ssid, const char* key, String& tok) {
  int start = 0;
  int keyLen = strlen(key);

  while (start <= (int)ssid.length()) {
    int end = ssid.indexOf('|', start);
    if (end < 0) end = ssid.length();

    String part = ssid.substring(start, end);
    part.trim();

    if (part.length() > keyLen) {
      bool match = true;
      for (int i = 0; i < keyLen; i++) {
        if (part[i] != key[i]) { match = false; break; }
      }

      if (match) {
        int p = keyLen;
        if (p < (int)part.length() && part[p] == ':') p++;
        if (p < (int)part.length()) {
          tok = part.substring(p);
          tok.trim();
          return tok.length() > 0;
        }
      }
    }

    if (end >= (int)ssid.length()) break;
    start = end + 1;
  }

  return false;
}

static bool parseUnsignedAfterKey(const String& ssid, const char* key, uint32_t& out, bool hexMode) {
  String tok;
  if (!findMachineToken(ssid, key, tok)) return false;

  char* endp = nullptr;
  uint32_t v = strtoul(tok.c_str(), &endp, hexMode ? 16 : 10);
  if (endp == tok.c_str()) return false;

  out = v;
  return true;
}

static bool parseSignedTailAfterKey(const String& ssid, const char* key, int16_t& outX100) {
  String tok;
  if (!findMachineToken(ssid, key, tok)) return false;

  tok.trim();
  if (tok.length() == 0) return false;

  int sign = +1;
  char c0 = tok[0];
  if (c0 == 'N' || c0 == '-' || c0 == 'n') { sign = -1; tok.remove(0, 1); }
  else if (c0 == 'P' || c0 == '+' || c0 == 'p') { sign = +1; tok.remove(0, 1); }

  tok.trim();
  if (tok.length() == 0) return false;

  char* endp = nullptr;
  long mag = strtol(tok.c_str(), &endp, 10);
  if (endp == tok.c_str()) return false;
  mag = constrain((int)mag, 0, 99);
  outX100 = (int16_t)(sign * mag * 100);
  return true;
}

static bool isOwnTextcastBssid(const uint8_t bssid[6]) {
  for (uint8_t i = 0; i < FIELD_COUNT; i++) {
    if (memcmp(bssid, fields[i].bssid, 6) == 0) return true;
  }
  return false;
}

// ============================================================
// LED: mirrored Anchor style
// ============================================================

static bool auxLedPinValid(int pin) {
  return pin >= 0 && pin <= 48 && pin != 19 && pin != 20 && pin != GLADIUS_RGB_LED_PIN;
}

static void extraLedWriteRaw(bool on) {
  if (!auxLedPinValid(GLADIUS_EXTRA_LED_PIN)) return;
  bool level = GLADIUS_EXTRA_LED_ACTIVE_LOW ? !on : on;
  digitalWrite(GLADIUS_EXTRA_LED_PIN, level ? HIGH : LOW);
  extraLedLastState = on;
}

static void setupExtraLed() {
  if (!auxLedPinValid(GLADIUS_EXTRA_LED_PIN)) {
    logf("LED", "extra LED disabled pin=%d", GLADIUS_EXTRA_LED_PIN);
    return;
  }

  pinMode(GLADIUS_EXTRA_LED_PIN, OUTPUT);
  extraLedWriteRaw(false);
  logf("LED", "extra LED pin=%d enabledByLongHold=%u", GLADIUS_EXTRA_LED_PIN, extraLedEnabled ? 1 : 0);
}

static void extraLedTick(uint32_t now) {
  if (!auxLedPinValid(GLADIUS_EXTRA_LED_PIN)) return;

  if (!extraLedEnabled) {
    if (extraLedLastState) extraLedWriteRaw(false);
    return;
  }

  bool on = false;

  // Дополнительный LED ведёт себя как "второй нерв": включается долгим зажатием
  // и подчёркивает важные состояния, но не спамит эфир/логи.
  if (lastMaxBrightnessFlashMs && now - lastMaxBrightnessFlashMs < GLADIUS_LED_MAX_FLASH_MS) {
    on = true;
  } else if (lastShareMs && now - lastShareMs < GLADIUS_LED_SHARE_MS) {
    on = ((now / 90UL) % 2UL) == 0;
  } else if (now < ledJobPulseUntilMs) {
    on = ((now / 180UL) % 2UL) == 0;
  } else if (!wifiOnline) {
    on = ((now / 650UL) % 2UL) == 0;
  } else {
    // slow heartbeat
    uint32_t m = now % 2200UL;
    on = (m < 90UL) || (m > 260UL && m < 340UL);
  }

  if (on != extraLedLastState) extraLedWriteRaw(on);
}

static void ledWriteRaw(uint8_t r, uint8_t g, uint8_t b) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  rgbLedWrite(GLADIUS_RGB_LED_PIN, r, g, b);
#else
  neopixelWrite(GLADIUS_RGB_LED_PIN, r, g, b);
#endif
}

static uint8_t scaleLed(uint8_t v) {
  return (uint8_t)(((uint16_t)v * (uint16_t)ledBrightness) / 255U);
}

static void clampLedBrightness() {
  if (ledBrightness < GLADIUS_BRIGHTNESS_MIN) ledBrightness = GLADIUS_BRIGHTNESS_MIN;
  if (ledBrightness > GLADIUS_BRIGHTNESS_MAX) ledBrightness = GLADIUS_BRIGHTNESS_MAX;
  ledOff = (ledBrightness == 0);
}

static void triggerMaxBrightnessFlash(const char* reason) {
  lastMaxBrightnessFlashMs = nowMs();
  lastLedMs = 0;
  logf("LED", "max brightness reached -> white flash %lums reason=%s next_taps=dim_down",
       (unsigned long)GLADIUS_LED_MAX_FLASH_MS, reason ? reason : "?");
}

static void setLedBrightness(uint8_t value, const char* reason) {
  uint8_t old = ledBrightness;
  ledBrightness = value;
  clampLedBrightness();

  if (ledBrightness == old) return;

  brightnessChangedMs = nowMs();
  brightnessDirty = true;
  lastLedMs = 0;

  logf("LED", "brightness=%u old=%u reason=%s dir=%s hint='tap cycles 0->max->0 / very long toggles UART logs + small LED'",
       (unsigned)ledBrightness, (unsigned)old, reason ? reason : "?",
       buttonBrightnessDirUp ? "up" : "down");

  if (old < GLADIUS_BRIGHTNESS_MAX && ledBrightness >= GLADIUS_BRIGHTNESS_MAX) {
    triggerMaxBrightnessFlash(reason);
  }
}

static void stepLedBrightness(int delta, const char* reason) {
  int v = (int)ledBrightness + delta;
  if (v > GLADIUS_BRIGHTNESS_MAX) v = GLADIUS_BRIGHTNESS_MAX;
  if (v < GLADIUS_BRIGHTNESS_MIN) v = GLADIUS_BRIGHTNESS_MIN;
  setLedBrightness((uint8_t)v, reason);
}

static void tapCycleLedBrightness(const char* reason) {
  // Anchor logic: each tap walks brightness up/down. Direction flips at max/min.
  if (ledBrightness >= GLADIUS_BRIGHTNESS_MAX) buttonBrightnessDirUp = false;
  if (ledBrightness <= GLADIUS_BRIGHTNESS_MIN) buttonBrightnessDirUp = true;

  stepLedBrightness(buttonBrightnessDirUp ? GLADIUS_BRIGHTNESS_STEP : -GLADIUS_BRIGHTNESS_STEP,
                    reason ? reason : "button_tap_cycle");

  if (ledBrightness >= GLADIUS_BRIGHTNESS_MAX) buttonBrightnessDirUp = false;
  if (ledBrightness <= GLADIUS_BRIGHTNESS_MIN) buttonBrightnessDirUp = true;
}

static void brightnessSaveTick(uint32_t now) {
  if (brightnessDirty && brightnessChangedMs && now - brightnessChangedMs >= GLADIUS_BRIGHTNESS_SAVE_MS) {
    prefs.putUChar("led", ledBrightness);
    prefs.putBool("ledOff", ledOff);
    brightnessDirty = false;
    logf("LED", "brightness saved=%u", (unsigned)ledBrightness);
  }
}

static void toggleExtraLed(const char* reason) {
  extraLedEnabled = !extraLedEnabled;
  prefs.putBool("extraLed", extraLedEnabled);
  logf("LED", "extraLed=%u reason=%s pin=%d", extraLedEnabled ? 1 : 0, reason ? reason : "?", GLADIUS_EXTRA_LED_PIN);
  if (!extraLedEnabled) extraLedWriteRaw(false);
  else {
    // visible acknowledgement
    extraLedWriteRaw(true);
    ledJobPulseUntilMs = nowMs() + 900UL;
  }
}

static void setUart0FullLog(bool enable, const char* reason) {
  bool old = janusUart0FullLog;
  janusUart0FullLog = enable;
  extraLedEnabled = enable; // same behavior as Anchor: long hold controls logs + small blinking LED.
  prefs.putBool("uart_full", janusUart0FullLog);
  prefs.putBool("extraLed", extraLedEnabled);
  if (!extraLedEnabled) extraLedWriteRaw(false);
  else {
    extraLedWriteRaw(true);
    ledJobPulseUntilMs = nowMs() + 900UL;
  }
  logf("UART0", "fullLog=%u old=%u reason=%s smallLed=%s uartTxWhenOff=0 persisted=1",
       janusUart0FullLog ? 1 : 0, old ? 1 : 0, reason ? reason : "?",
       janusUart0FullLog ? "active_on_uart_tx" : "quiet");
}

static void toggleUart0FullLog(const char* reason) {
  setUart0FullLog(!janusUart0FullLog, reason);
}

// v1.17A compilefix: ledTick must be non-static in .ino, otherwise Arduino auto-prototype declares it extern.
void ledTick() {
  uint32_t now = nowMs();

  if (now - lastLedMs < 70UL) {
    extraLedTick(now);
    return;
  }
  lastLedMs = now;

  if (ledBrightness == 0) {
    if (lastLedR || lastLedG || lastLedB) {
      lastLedR = 0; lastLedG = 0; lastLedB = 0;
      ledWriteRaw(0, 0, 0);
    }
    extraLedTick(now);
    return;
  }

  // v1.16 JANUS TWIN FACE:
  //   face 0 = Army Men green plastic soldier
  //   face 1 = turquoise/cyan twin face
  // Normal: Anchor holds face 0, Gladius holds face 1.
  // SHARE on either brother: both receive J/F and smoothly exchange faces.
  const float armyR0 = 18.0f, armyG0 = 118.0f, armyB0 = 26.0f;
  const float armyR1 = 34.0f, armyG1 = 164.0f, armyB1 = 42.0f;
  const float turqR0 = 0.0f,  turqG0 = 126.0f, turqB0 = 118.0f;
  const float turqR1 = 10.0f, turqG1 = 228.0f, turqB1 = 214.0f;

  bool swapActive = janusFaceShareActive();
  uint8_t normalFace = janusFaceBaseFace() & 1;
  uint8_t swappedFace = normalFace ^ 1;

  float phase = (sinf((float)now / 1900.0f) + 1.0f) * 0.5f;
  float soft = phase * phase * (3.0f - 2.0f * phase);

  bool normalTurq = (normalFace != JANUS_FACE_AMBER);
  bool swappedTurq = (swappedFace != JANUS_FACE_AMBER);

  float fromR = normalTurq ? (turqR0 + (turqR1 - turqR0) * soft) : (armyR0 + (armyR1 - armyR0) * soft);
  float fromG = normalTurq ? (turqG0 + (turqG1 - turqG0) * soft) : (armyG0 + (armyG1 - armyG0) * soft);
  float fromB = normalTurq ? (turqB0 + (turqB1 - turqB0) * soft) : (armyB0 + (armyB1 - armyB0) * soft);

  float toR = swappedTurq ? (turqR0 + (turqR1 - turqR0) * soft) : (armyR0 + (armyR1 - armyR0) * soft);
  float toG = swappedTurq ? (turqG0 + (turqG1 - turqG0) * soft) : (armyG0 + (armyG1 - armyG0) * soft);
  float toB = swappedTurq ? (turqB0 + (turqB1 - turqB0) * soft) : (armyB0 + (armyB1 - armyB0) * soft);

  float swapMix = 0.0f;
  float flash = 0.0f;
  if (swapActive) {
    uint32_t refMs = lastShareMs ? lastShareMs : janusFacePeerLastMs;
    float age = (float)(now - refMs) / (float)JANUS_FACE_SWAP_MS;
    age = constrain(age, 0.0f, 1.0f);

    // 0..28%: перелив туда, 28..72%: держим поменянные лица,
    // 72..100%: мягко возвращаемся к своим штатным лицам.
    if (age < 0.28f) {
      swapMix = age / 0.28f;
      swapMix = swapMix * swapMix * (3.0f - 2.0f * swapMix);
    } else if (age > 0.72f) {
      swapMix = (1.0f - age) / 0.28f;
      swapMix = constrain(swapMix, 0.0f, 1.0f);
      swapMix = swapMix * swapMix * (3.0f - 2.0f * swapMix);
    } else {
      swapMix = 1.0f;
    }

    // Short pearl flash at the beginning: visible SHARE ignition without dirty red/amber colors.
    flash = constrain((0.16f - age) / 0.16f, 0.0f, 1.0f);
    flash = flash * flash;
  }

  float r = fromR * (1.0f - swapMix) + toR * swapMix;
  float g = fromG * (1.0f - swapMix) + toG * swapMix;
  float b = fromB * (1.0f - swapMix) + toB * swapMix;

  // Job pulse should not corrupt the twin colors. It only adds a tiny living brightness wave.
  if (now < ledJobPulseUntilMs && !swapActive) {
    float pulse = (sinf((float)now / 125.0f) + 1.0f) * 0.5f;
    float boost = 1.0f + 0.10f * pulse;
    r *= boost; g *= boost; b *= boost;
  }

  if (flash > 0.0f) {
    r = r * (1.0f - flash) + 245.0f * flash;
    g = g * (1.0f - flash) + 255.0f * flash;
    b = b * (1.0f - flash) + 225.0f * flash;
  }

  // Max brightness marker: visible white peak, then fade.
  if (lastMaxBrightnessFlashMs && now - lastMaxBrightnessFlashMs < GLADIUS_LED_MAX_FLASH_MS) {
    float age = (float)(now - lastMaxBrightnessFlashMs) / (float)GLADIUS_LED_MAX_FLASH_MS;
    float glow = (age < 0.35f) ? 1.0f : constrain((1.0f - age) / 0.65f, 0.0f, 1.0f);
    r = r * (1.0f - glow) + 255.0f * glow;
    g = g * (1.0f - glow) + 255.0f * glow;
    b = b * (1.0f - glow) + 255.0f * glow;
  }

  uint8_t rr = scaleLed((uint8_t)constrain((int)r, 0, 255));
  uint8_t gg = scaleLed((uint8_t)constrain((int)g, 0, 255));
  uint8_t bb = scaleLed((uint8_t)constrain((int)b, 0, 255));

  if (rr != lastLedR || gg != lastLedG || bb != lastLedB) {
    lastLedR = rr; lastLedG = gg; lastLedB = bb;
    ledWriteRaw(rr, gg, bb);
  }

  extraLedTick(now);
}

static void setLed(uint8_t b) {
  setLedBrightness((uint8_t)constrain((int)b, GLADIUS_BRIGHTNESS_MIN, GLADIUS_BRIGHTNESS_MAX), "serial_set");
}

static void ledPlus() {
  stepLedBrightness(GLADIUS_BRIGHTNESS_STEP, "serial_plus");
}

static void ledMinus() {
  stepLedBrightness(-GLADIUS_BRIGHTNESS_STEP, "serial_minus");
}

static void ledOffNow() {
  setLedBrightness(0, "serial_off");
}

static void setupBrightnessButton() {
  pinMode(GLADIUS_BUTTON_PIN, INPUT_PULLUP);
  bool rawPressed = GLADIUS_BUTTON_ACTIVE_LOW ? (digitalRead(GLADIUS_BUTTON_PIN) == LOW) : (digitalRead(GLADIUS_BUTTON_PIN) == HIGH);
  buttonStablePressed = rawPressed;
  buttonLastRawPressed = rawPressed;
  buttonPressStartMs = rawPressed ? nowMs() : 0;
  logf("BUTTON", "enabled pin=%u mode=tap_cycle_brightness veryLong(~%lums)=toggle_uart0_logs_and_small_led min=%u max=%u step=%u savedBrightness=%u dir=%s uartFull=%u smallLed=%u",
       (unsigned)GLADIUS_BUTTON_PIN,
       (unsigned long)GLADIUS_BUTTON_LOG_TOGGLE_MS,
       (unsigned)GLADIUS_BRIGHTNESS_MIN,
       (unsigned)GLADIUS_BRIGHTNESS_MAX,
       (unsigned)GLADIUS_BRIGHTNESS_STEP,
       (unsigned)ledBrightness,
       buttonBrightnessDirUp ? "up" : "down",
       janusUart0FullLog ? 1 : 0,
       extraLedEnabled ? 1 : 0);
}

static void brightnessButtonTick(uint32_t now) {
  bool rawPressed = GLADIUS_BUTTON_ACTIVE_LOW ? (digitalRead(GLADIUS_BUTTON_PIN) == LOW) : (digitalRead(GLADIUS_BUTTON_PIN) == HIGH);

  if (rawPressed != buttonLastRawPressed) {
    buttonLastRawPressed = rawPressed;
    lastButtonSampleMs = now;
  }

  if ((now - lastButtonSampleMs) < GLADIUS_BUTTON_DEBOUNCE_MS) return;

  if (rawPressed != buttonStablePressed) {
    buttonStablePressed = rawPressed;

    if (buttonStablePressed) {
      buttonPressStartMs = now;
      buttonLastRepeatMs = now;
      buttonLongMode = false;
      buttonLogToggleFired = false;
    } else {
      if (buttonLogToggleFired) {
        // Very long press already toggled UART0 logs/small LED. Do not also change brightness.
      } else {
        // Anchor behavior: normal tap/release cycles brightness directionally.
        tapCycleLedBrightness("button_tap_cycle");
      }

      buttonPressStartMs = 0;
      buttonLongMode = false;
      buttonLogToggleFired = false;
    }
  }

  if (buttonStablePressed && buttonPressStartMs && !buttonLogToggleFired &&
      (now - buttonPressStartMs >= GLADIUS_BUTTON_LOG_TOGGLE_MS)) {
    buttonLogToggleFired = true;
    buttonLongMode = true;
    toggleUart0FullLog("button_very_long_toggle");
  }
}

static void bootButtonTick() {
  uint32_t now = nowMs();
  brightnessButtonTick(now);
  brightnessSaveTick(now);
}
// ============================================================
// TextCast fake SSID fields
// ============================================================

static void fieldMakeBssid(uint8_t fieldId, uint8_t out[6]) {
  // Не используем esp_wifi_get_mac здесь: Wi-Fi может ещё не быть поднят.
  // Берём стабильный eFuse MAC и делаем отдельный locally-administered BSSID для каждого fake SSID поля.
  uint64_t m = ESP.getEfuseMac();
  uint8_t mac[6];
  mac[0] = (uint8_t)(m >> 40);
  mac[1] = (uint8_t)(m >> 32);
  mac[2] = (uint8_t)(m >> 24);
  mac[3] = (uint8_t)(m >> 16);
  mac[4] = (uint8_t)(m >> 8);
  mac[5] = (uint8_t)(m);

  out[0] = 0x02;           // locally administered
  out[1] = mac[1];
  out[2] = mac[2];
  out[3] = 0x47;           // G
  out[4] = 0x4C + fieldId; // L/M/N
  out[5] = mac[5] ^ (0x31 + fieldId * 0x13);
}

static void fieldSet(uint8_t id, const String& text, uint32_t durationMs, uint32_t shiftMs, uint32_t beaconMs) {
  if (id >= FIELD_COUNT) return;

  String t = text;
  t.trim();
  if (t.length() == 0) t = "JANUS";

  fields[id].enabled = true;
  fields[id].text = t;
  fields[id].source = "";
  for (uint8_t i = 0; i < TEXTCAST_GAP_SPACES; i++) fields[id].source += ' ';
  fields[id].source += t;
  for (uint8_t i = 0; i < TEXTCAST_GAP_SPACES + 2; i++) fields[id].source += ' ';

  fields[id].bytePos = 0;
  fields[id].lastShiftMs = 0;
  fields[id].lastBeaconMs = 0;
  fields[id].shiftMs = shiftMs;
  fields[id].beaconMs = beaconMs;
  fields[id].untilMs = durationMs ? nowMs() + durationMs : 0;

  // v1.21: field changes still happen, but Serial logging is quieter during Buzz races.
  static String lastLoggedText[FIELD_COUNT];
  static uint32_t lastLoggedMs[FIELD_COUNT] = {0};
  uint32_t logAge = janusSafeAgeMs(nowMs(), lastLoggedMs[id], 999999UL);
  bool logChange = (lastLoggedText[id] != t);
  uint32_t minLogMs = (id == FIELD_MACHINE || id == FIELD_EVENT) ? 3500UL : 9000UL;
  if (logChange || logAge >= minLogMs) {
    logf("TEXTCAST", "field=%u text='%s'", id, t.c_str());
    lastLoggedText[id] = t;
    lastLoggedMs[id] = nowMs();
  }
}

static String fieldFrame(struct TextcastField& f, bool advance) {
  if (f.source.length() == 0) return "JANUS";

  if (f.bytePos >= f.source.length()) f.bytePos = 0;
  f.bytePos = clampUtf8BoundaryBack(f.source, f.bytePos);

  String out;
  out.reserve(40);

  size_t p = f.bytePos;
  while (p < f.source.length()) {
    size_t n = utf8CharLenAt(f.source, p);
    if (n == 0) break;
    if (out.length() + n > TEXTCAST_MAX_SSID_BYTES) break;
    out += f.source.substring(p, p + n);
    p += n;
  }

  out.trim();

  if (out.length() == 0) out = f.text;
  if (out.length() > TEXTCAST_MAX_SSID_BYTES) out.remove(TEXTCAST_MAX_SSID_BYTES);

  if (advance) {
    f.bytePos = nextUtf8Boundary(f.source, f.bytePos);
    if (f.bytePos >= f.source.length()) f.bytePos = 0;
  }

  return out;
}

static void sendVirtualBeacon(struct TextcastField& f, const String& ssid) {
  if (!f.enabled) return;
  if (ssid.length() == 0) return;

  uint8_t ssidLen = (uint8_t)min((size_t)TEXTCAST_MAX_SSID_BYTES, ssid.length());

  uint8_t frame[160];
  memset(frame, 0, sizeof(frame));
  int pos = 0;

  // 802.11 beacon header
  frame[pos++] = 0x80;
  frame[pos++] = 0x00;
  frame[pos++] = 0x00;
  frame[pos++] = 0x00;

  for (int i = 0; i < 6; i++) frame[pos++] = 0xFF;       // DA
  for (int i = 0; i < 6; i++) frame[pos++] = f.bssid[i];  // SA
  for (int i = 0; i < 6; i++) frame[pos++] = f.bssid[i];  // BSSID

  uint16_t seqCtl = (uint16_t)(((f.sent + gladiusSeq) & 0x0FFF) << 4);
  frame[pos++] = seqCtl & 0xFF;
  frame[pos++] = (seqCtl >> 8) & 0xFF;

  for (int i = 0; i < 8; i++) frame[pos++] = 0x00;        // timestamp

  frame[pos++] = 0x64; // beacon interval 100 TU
  frame[pos++] = 0x00;

  frame[pos++] = 0x01; // capability ESS/open
  frame[pos++] = 0x04;

  frame[pos++] = 0x00; // SSID tag
  frame[pos++] = ssidLen;
  memcpy(frame + pos, ssid.c_str(), ssidLen);
  pos += ssidLen;

  static const uint8_t rates[] = {0x82, 0x84, 0x8b, 0x96, 0x0c, 0x12, 0x18, 0x24};
  frame[pos++] = 0x01;
  frame[pos++] = sizeof(rates);
  memcpy(frame + pos, rates, sizeof(rates));
  pos += sizeof(rates);

  frame[pos++] = 0x03; // DS parameter set
  frame[pos++] = 0x01;
  frame[pos++] = wifiChannel;

  esp_err_t err = esp_wifi_80211_tx(WIFI_IF_STA, frame, pos, false);
  if (err != ESP_OK) {
    // fallback with system sequence
    err = esp_wifi_80211_tx(WIFI_IF_STA, frame, pos, true);
  }

  if (err == ESP_OK) {
    f.sent++;
    if (f.sent == 0) f.sent = 1;
  }
}

static void textcastTick() {
  if (textMode == TEXTCAST_QUIET && !GLADIUS_MINER_FIELD_ALWAYS_ON) return;

  uint32_t now = nowMs();

  for (uint8_t i = 0; i < FIELD_COUNT; i++) {
    if (textMode == TEXTCAST_QUIET && GLADIUS_MINER_FIELD_ALWAYS_ON && i != FIELD_MINER) continue;
    TextcastField& f = fields[i];

    if (!f.enabled) continue;

    if (f.untilMs != 0 && (int32_t)(now - f.untilMs) > 0) {
      f.enabled = false;
      continue;
    }

    bool advance = false;

    if (now - f.lastShiftMs >= f.shiftMs || f.lastShiftMs == 0) {
      advance = true;
      f.lastShiftMs = now;
    }

    if (now - f.lastBeaconMs >= f.beaconMs || f.lastBeaconMs == 0) {
      String ssid = fieldFrame(f, advance);
      sendVirtualBeacon(f, ssid);
      f.lastBeaconMs = now;
    }
  }
}


// ============================================================
// Swarm machine TextCast language: JG2|...
// ============================================================

static uint8_t laneAwayFrom(uint8_t lane, uint16_t salt) {
  if (lane > LANE_RANDOM) lane = LANE_ZIM_REVERSE;
  return (uint8_t)((lane + 2 + (salt % 4)) % (LANE_RANDOM + 1));
}

static String buildMachineBeacon() {
  uint8_t b = bestZ;
  uint16_t j = (uint16_t)(currentJob.jobId & 0xFFFF);
  uint8_t lane = tailGexTopLane <= LANE_RANDOM ? tailGexTopLane : activeLane;
  if (lane > LANE_RANDOM) lane = LANE_ZIM_REVERSE;

  int16_t tx = tailGexTopX100;
  uint8_t mag = (uint8_t)min(99, abs((int)tx) / 100);
  char sign = tx >= 0 ? 'P' : 'N';
  uint8_t conf = (uint8_t)min(100, (int)tailGexTopConfidenceX100);

  char buf[33];
  // 32 bytes max:
  // JG2|NFFFF|JFFFF|L6|B99|TP09|C99
  snprintf(buf, sizeof(buf), "JG2|N%04X|J%04X|L%u|B%u|T%c%02u|C%u", nodeId, j, lane, b, sign, mag, conf);
  String out = String(buf);

  // Emergency fallback to old v1/v1.11 syntax if the line ever becomes too long.
  if (out.length() > 32) {
    uint8_t g = (uint8_t)min(99, abs((int)tailGexTopX100) / 100);
    snprintf(buf, sizeof(buf), "JG|N%04X|B%u|J%04X|G%c%u", nodeId, b, j, sign, g);
    out = String(buf);
  }
  if (out.length() > 32) out.remove(32);
  return out;
}

static void updateMachineField(bool force) {
  uint32_t now = nowMs();

  if (!force && now - lastMachineFieldMs < MACHINE_FIELD_INTERVAL_MS) return;
  lastMachineFieldMs = now;

  String m = buildMachineBeacon();
  if (!force && m == lastMachineBeacon) return;

  lastMachineBeacon = m;
  machineBeaconUpdates++;
  fieldSet(FIELD_MACHINE, m, 0, 60000, 1350);
}

static bool parseMachineBeacon(const String& ssid, struct SwarmNeighbor& n) {
  bool v2 = ssid.startsWith("JG2|");
  bool v1 = ssid.startsWith("JG|");
  if (!v2 && !v1) return false;

  uint32_t v = 0;
  int16_t sx = 0;

  n.textcastVersion = v2 ? 2 : 1;
  n.lane = 255;
  n.tailX100 = 0;
  n.confidence = 0;
  n.gexWeightPct = 0;
  n.hasGex = false;
  n.sameJob = false;

  // Поддержка compact "NFFFF" и verbose "N:FFFF".
  if (parseUnsignedAfterKey(ssid, "N", v, true)) n.nodeId = (uint16_t)v;
  else return false;

  if (parseUnsignedAfterKey(ssid, "H", v, false)) n.hpsEma = (uint16_t)min(65535UL, v);
  if (parseUnsignedAfterKey(ssid, "B", v, false)) n.bestZ = (uint8_t)min(255UL, v);
  if (parseUnsignedAfterKey(ssid, "S", v, false)) n.shares = (uint16_t)min(65535UL, v);
  if (parseUnsignedAfterKey(ssid, "J", v, true)) n.jobIdShort = (uint16_t)v;
  if (parseUnsignedAfterKey(ssid, "O", v, false)) n.oxy = (uint8_t)min(100UL, v);
  else n.oxy = 50;

  if (parseUnsignedAfterKey(ssid, "L", v, false)) {
    n.lane = (uint8_t)min((uint32_t)LANE_RANDOM, v);
    n.hasGex = true;
  }

  if (parseUnsignedAfterKey(ssid, "C", v, false)) {
    n.confidence = (uint8_t)min(100UL, v);
    n.hasGex = true;
  }

  // New v2: T[P/N/+/‑]NN, old v1: G[P/N]N.
  if (parseSignedTailAfterKey(ssid, "T", sx) || parseSignedTailAfterKey(ssid, "G", sx)) {
    n.tailX100 = sx;
    n.hasGex = true;
  }

  if (n.hasGex) {
    uint8_t mag = (uint8_t)min(99, abs((int)n.tailX100) / 100);
    n.gexWeightPct = (uint8_t)constrain((int)TAIL_GEX_EXPLORATION_FLOOR_PCT + mag + (int)n.confidence / 4, 0, (int)TAIL_GEX_MAX_WEIGHT_PCT);
  }

  return true;
}

static int findNeighborSlot(const uint8_t bssid[6], uint16_t remoteNodeId) {
  int freeSlot = -1;
  uint32_t oldestAge = 0;
  int oldestSlot = 0;
  uint32_t now = nowMs();

  for (uint8_t i = 0; i < 12; i++) {
    if (swarmNeighbors[i].used) {
      if (memcmp(swarmNeighbors[i].bssid, bssid, 6) == 0 || swarmNeighbors[i].nodeId == remoteNodeId) return i;

      uint32_t age = now - swarmNeighbors[i].lastSeenMs;
      if (age > oldestAge) {
        oldestAge = age;
        oldestSlot = i;
      }
    } else if (freeSlot < 0) {
      freeSlot = i;
    }
  }

  if (freeSlot >= 0) return freeSlot;
  return oldestSlot;
}

static void updateNeighborFromBeacon(const struct SniffEvent& ev) {
  String ssid = String(ev.ssid);

  SwarmNeighbor parsed;
  memset(&parsed, 0, sizeof(parsed));

  if (!parseMachineBeacon(ssid, parsed)) return;
  if (parsed.nodeId == nodeId) return;
  if (isOwnTextcastBssid(ev.bssid)) return;

  int slot = findNeighborSlot(ev.bssid, parsed.nodeId);
  SwarmNeighbor& n = swarmNeighbors[slot];

  if (!n.used) {
    memset(&n, 0, sizeof(n));
    n.used = true;
    memcpy(n.bssid, ev.bssid, 6);
    logf("TEXTCAST/RX", "new neighbor node=%04X bssid=%s ssid=%s", parsed.nodeId, macToString(ev.bssid).c_str(), ssid.c_str());
  }

  n.nodeId = parsed.nodeId;
  n.lastSeenMs = nowMs();
  n.rssi = ev.rssi;
  n.hpsEma = parsed.hpsEma;
  n.bestZ = parsed.bestZ;
  n.shares = parsed.shares;
  n.jobIdShort = parsed.jobIdShort;
  n.oxy = parsed.oxy;
  n.textcastVersion = parsed.textcastVersion;
  n.lane = parsed.lane;
  n.tailX100 = parsed.tailX100;
  n.confidence = parsed.confidence;
  n.gexWeightPct = parsed.gexWeightPct;
  n.hasGex = parsed.hasGex;
  n.sameJob = false;
  n.seen++;

  if (n.hasGex && (n.seen <= 2 || n.confidence >= 35 || abs((int)n.tailX100) >= 400)) {
    logf("TEXTCAST/GEX", "rx node=%04X v%u rssi=%d job=%04X lane=%s tail=%d conf=%u best=%u",
         n.nodeId,
         n.textcastVersion,
         n.rssi,
         n.jobIdShort,
         laneName(n.lane),
         (int)n.tailX100,
         n.confidence,
         n.bestZ);
  }

  sniffJanus++;
}

static void rebuildSwarmStatsAndAdapt() {
  uint32_t now = nowMs();
  uint16_t count = 0;
  uint16_t oxySum = 0;
  uint8_t remoteBest = 0;
  uint8_t sameJob = 0;
  uint8_t gexPeers = 0;
  uint8_t closeSameLane = 0;
  uint16_t myJobShort = (uint16_t)(currentJob.jobId & 0xFFFF);

  int32_t bestGexScore = -2147480000L;
  uint8_t bestGexLane = LANE_ZIM_REVERSE;
  int16_t bestGexX100 = 0;
  uint8_t bestGexConf = 0;
  uint16_t bestGexNode = 0;
  int8_t bestGexRssi = -127;
  bool bestGexSameJob = false;

  for (uint8_t i = 0; i < 12; i++) {
    SwarmNeighbor& n = swarmNeighbors[i];

    if (!n.used) continue;

    if (now - n.lastSeenMs > NEIGHBOR_TTL_MS) {
      n.used = false;
      continue;
    }

    count++;
    oxySum += n.oxy;
    if (n.bestZ > remoteBest) remoteBest = n.bestZ;

    n.sameJob = (myJobShort != 0 && n.jobIdShort == myJobShort);
    if (n.sameJob) sameJob++;

    if (n.hasGex && n.lane <= LANE_RANDOM && n.confidence >= TEXTCAST_GEX_REMOTE_MIN_CONF) {
      gexPeers++;

      if (n.rssi >= TEXTCAST_GEX_CLOSE_RSSI && n.sameJob && n.lane == activeLane) {
        closeSameLane++;
      }

      int32_t score = (int32_t)n.tailX100 + (int32_t)n.confidence * 5L + (int32_t)n.bestZ * 20L;
      if (n.sameJob) score += 220;
      if (n.rssi >= TEXTCAST_GEX_CLOSE_RSSI) score += 60;
      if (n.tailX100 < 0) score -= 180;

      if (score > bestGexScore) {
        bestGexScore = score;
        bestGexLane = n.lane;
        bestGexX100 = n.tailX100;
        bestGexConf = n.confidence;
        bestGexNode = n.nodeId;
        bestGexRssi = n.rssi;
        bestGexSameJob = n.sameJob;
      }
    }
  }

#if JANUS_TWIN_TASK_ENABLE
  // v1.20: BrotherLink is not a fake SSID, but ADAPT must still know that
  // Anchor is beside us on the same Buzz window. This fixes logs like
  // neighbors=0/sameJob=0 while [TWIN] is clearly fresh.
  if (janusTwinPeerFresh()) {
    count++;
    if (janusTwinPeerBestBits > remoteBest) remoteBest = (uint8_t)min((uint32_t)255, janusTwinPeerBestBits);
    bool twinSame = (currentJob.jobId != 0 && currentJob.jobId == janusTwinPeerJobFp32);
    if (twinSame) sameJob++;
    if (janusTwinPeerLane <= LANE_RANDOM) {
      gexPeers++;
      if (janusTwinPeerRssi >= TEXTCAST_GEX_CLOSE_RSSI && twinSame && janusTwinPeerLane == activeLane) closeSameLane++;
      int32_t twinScore = (int32_t)janusTwinPeerBestBits * 22L + (twinSame ? 240L : 0L) + (janusTwinPeerRssi >= TEXTCAST_GEX_CLOSE_RSSI ? 80L : 0L);
      if (twinScore > bestGexScore) {
        bestGexScore = twinScore;
        bestGexLane = janusTwinPeerLane;
        bestGexX100 = 0;
        bestGexConf = twinSame ? 78 : 45;
        bestGexNode = janusTwinPeerNode;
        bestGexRssi = janusTwinPeerRssi;
        bestGexSameJob = twinSame;
      }
    }
  }
#endif

  swarmNeighborCount = (uint8_t)min(255, (int)count);
  swarmAvgOxy = count ? (uint8_t)(oxySum / count) : 50;
  swarmBestRemoteZ = remoteBest;
  swarmSameJobCount = sameJob;
  swarmTextcastGexPeers = gexPeers;
  swarmBestGexLane = bestGexLane;
  swarmBestGexX100 = bestGexX100;
  swarmBestGexConfidence = bestGexConf;
  swarmBestGexNode = bestGexNode;

  uint8_t newLane = currentJob.lane <= LANE_RANDOM ? currentJob.lane : LANE_ZIM_REVERSE;
  uint32_t newStride = currentJob.stride ? currentJob.stride : 17;
  uint8_t newScale = 100;

  if (count >= 5) newScale = 70;
  if (swarmAvgOxy < 30) newScale = min(newScale, (uint8_t)60);
  if (!wifiOnline) newScale = min(newScale, (uint8_t)55);

  // Старое правило: если много узлов на одном job, расходиться, не топтаться в одном nonce-следе.
  if (sameJob >= 2) {
    newLane = LANE_KNIGHT;
    newStride = 65537;
  }

  if (remoteBest > bestZ + 2) {
    newLane = LANE_RANDOM;
    newStride = 0x9E3779B9UL;
  }

  // TEXTCAST v2 / JG2: соседский хвост — это scent, не приказ.
  // Копируем lane только если хвост положительный и уверенность достаточная.
  if (gexPeers > 0 &&
      bestGexConf >= TEXTCAST_GEX_REMOTE_MIN_CONF &&
      bestGexX100 >= TEXTCAST_GEX_REMOTE_MIN_TAIL_X100 &&
      bestGexLane <= LANE_RANDOM) {
    newLane = bestGexLane;
    switch (newLane) {
      case LANE_LINEAR: newStride = 1; break;
      case LANE_ZIM_REVERSE: newStride = 17; break;
      case LANE_ZIM_BANDIT: newStride = 4099; break;
      case LANE_JANUS_CENTER: newStride = 257; break;
      case LANE_KNIGHT: newStride = 65537; break;
      case LANE_BITREV: newStride = 521; break;
      case LANE_RANDOM:
      default: newStride = 0x9E3779B9UL; break;
    }
    uint8_t boost = (uint8_t)min((int)TEXTCAST_GEX_MAX_BATCH_BOOST, (int)(bestGexConf / 8));
    newScale = min((uint8_t)130, (uint8_t)(newScale + boost));
  }

  // Если близкий сосед на том же job и той же lane — расходиться, а не дублировать поиск.
  if (closeSameLane > 0 && bestGexSameJob) {
    newLane = laneAwayFrom(bestGexLane, nodeId ^ bestGexNode);
    switch (newLane) {
      case LANE_LINEAR: newStride = 1; break;
      case LANE_ZIM_REVERSE: newStride = 17; break;
      case LANE_ZIM_BANDIT: newStride = 4099; break;
      case LANE_JANUS_CENTER: newStride = 257; break;
      case LANE_KNIGHT: newStride = 65537; break;
      case LANE_BITREV: newStride = 521; break;
      case LANE_RANDOM:
      default: newStride = 0x9E3779B9UL; break;
    }
    swarmLaneSplitCount++;
    logf("TEXTCAST/SPLIT", "closeSameLane=%u peer=%04X peerLane=%s -> myLane=%s rssi=%d",
         closeSameLane,
         bestGexNode,
         laneName(bestGexLane),
         laneName(newLane),
         bestGexRssi);
  }

  if (bestZ + 2 >= currentJob.targetBits && currentJob.targetBits > 0) {
    newLane = LANE_ZIM_BANDIT;
    newStride = 4099;
    newScale = min((uint8_t)120, (uint8_t)(newScale + 15));
  }

  bool changed = (newLane != swarmPreferredLane) || (newStride != swarmPreferredStride) || (newScale != swarmBatchScalePct);

  swarmPreferredLane = newLane;
  swarmPreferredStride = newStride | 1UL;
  swarmBatchScalePct = newScale;

  if (changed) {
    logf("ADAPT", "neighbors=%u avgOxy=%u remoteBest=%u sameJob=%u gexPeers=%u bestGex=%d/%u@%04X lane=%s stride=%lu batchScale=%u split=%lu",
         swarmNeighborCount,
         swarmAvgOxy,
         swarmBestRemoteZ,
         swarmSameJobCount,
         swarmTextcastGexPeers,
         (int)swarmBestGexX100,
         swarmBestGexConfidence,
         swarmBestGexNode,
         laneName(swarmPreferredLane),
         (unsigned long)swarmPreferredStride,
         swarmBatchScalePct,
         (unsigned long)swarmLaneSplitCount);
  }
}


static bool gladiusInterestingParrySsid(const char* ssid) {
  if (!ssid || !ssid[0]) return false;
  String s = String(ssid);
  s.toUpperCase();
  s.trim();
  if (s.length() == 0) return false;
  return s.startsWith("GLAD") || s.startsWith("JANUS") || s.startsWith("JG") ||
         s.indexOf("GLADIUS") >= 0 || s.indexOf("GLAD") >= 0 || s.indexOf("JG2") >= 0;
}

static uint32_t gladiusOui24(const uint8_t mac[6]) {
  return ((uint32_t)mac[0] << 16) | ((uint32_t)mac[1] << 8) | (uint32_t)mac[2];
}

static const char* gladiusParryVendorGuess(const uint8_t mac[6]) {
  // Современные телефоны часто используют random/private MAC. Тогда честно пишем PRIVATE?.
  if (mac[0] & 0x02) return "PRIVATE?";
  uint32_t oui = gladiusOui24(mac);
  switch (oui) {
    // Apple / iPhone / iPad / Mac common OUIs. Неполный список, поэтому знак вопроса.
    case 0x0017F2: case 0x0019E3: case 0x001B63: case 0x001EC2: case 0x0023DF:
    case 0x0025BC: case 0x0026BB: case 0x28CFDA: case 0x3C0754: case 0x40A6D9:
    case 0x60F81D: case 0x70DEE2: case 0x7CD1C3: case 0x8C8590: case 0xA4C361:
    case 0xB8E856: case 0xDCA632: case 0xF0D1A9: case 0xF4F5D8:
      return "APPLE?";
    // Samsung common OUIs.
    case 0x001632: case 0x002490: case 0x00265D: case 0x4C3C16: case 0x5C0A5B:
    case 0x701124: case 0x84A466: case 0xA0CBFD: case 0xBC4486: case 0xCC051B:
    case 0xE8E5D6:
      return "SAMSUNG?";
    // Xiaomi / Redmi / Poco common OUIs.
    case 0x28E31F: case 0x50EC50: case 0x64B473: case 0x7C1DD9: case 0xA086C6:
    case 0xD4970B: case 0xF0B429:
      return "XIAOMI?";
    // Huawei / Honor common OUIs.
    case 0x001E10: case 0x0022A1: case 0x2446C8: case 0x8C34FD: case 0xA08D16:
    case 0xC88447:
      return "HUAWEI?";
    // ESP / boards / IoT.
    case 0x240AC4: case 0x30AEA4: case 0x7CDFA1: case 0xAC67B2: case 0xC8F09E:
    case 0xEC94CB:
      return "ESP?";
    // Common PC Wi-Fi vendors.
    case 0x001500: case 0x001B21: case 0x001E67: case 0x3C58C2: case 0x40A6B7:
    case 0xA0A8CD: case 0xF8B156:
      return "PC?";
    default:
      return "DEVICE?";
  }
}

static bool gladiusTextFieldStillAlive(uint8_t id, uint32_t now) {
  if (id >= FIELD_COUNT) return false;
  if (!fields[id].enabled) return false;
  if (fields[id].untilMs == 0) return true;
  return (int32_t)(now - fields[id].untilMs) <= 0;
}

static void gladiusHandleParryEvent(const SniffEvent& ev) {
  uint32_t now = nowMs();
  bool sameSta = memcmp(gladiusParryLastSta, ev.sta, 6) == 0;
  if (sameSta && janusSafeAgeMs(now, gladiusParryLastMs, 999999UL) < GLADIUS_PARRY_SAME_STA_COOLDOWN_MS) return;

  // v1.23: one active PARRY line only. Do not flicker several fake SSIDs when many devices probe.
  if (!sameSta && gladiusTextFieldStillAlive(FIELD_PARRY, now)) {
    gladiusParrySuppressed++;
    uint32_t logAge = janusSafeAgeMs(now, gladiusParryLastLogMs, 999999UL);
    if (logAge >= GLADIUS_PARRY_LOG_MIN_MS) {
      gladiusParryLastLogMs = now;
      logf("PARRY", "busy keep='%s' newSta=%s suppressed=%lu",
           fields[FIELD_PARRY].text.c_str(), macToString(ev.sta).c_str(), (unsigned long)gladiusParrySuppressed);
    }
    return;
  }

  memcpy(gladiusParryLastSta, ev.sta, 6);
  gladiusParryLastMs = now;
  gladiusParryEvents++;

  char tail[9];
  snprintf(tail, sizeof(tail), "%02X%02X%02X", ev.sta[3], ev.sta[4], ev.sta[5]);
  const char* guess = gladiusParryVendorGuess(ev.sta);
  gladiusParryLastName = String(guess) + " " + String(tail);

  // One static SSID line, no scrolling spam: e.g. "GLAD PARRY APPLE? A1B2C3".
  String msg = String("GLAD PARRY ") + guess + " " + tail;
  msg = sanitizeSSIDAscii(msg, 31);
  fieldSet(FIELD_PARRY, msg, GLADIUS_PARRY_FIELD_MS, GLADIUS_PARRY_FIELD_STATIC_SHIFT_MS, GLADIUS_PARRY_FIELD_BEACON_MS);

  uint32_t logAge = janusSafeAgeMs(now, gladiusParryLastLogMs, 999999UL);
  if (logAge >= GLADIUS_PARRY_LOG_MIN_MS) {
    gladiusParryLastLogMs = now;
    logf("PARRY", "guess='%s' sta=%s subtype=0x%02X rssi=%d target='%s' field='%s' n=%lu suppressed=%lu",
         guess, macToString(ev.sta).c_str(), ev.subtype, ev.rssi, ev.ssid, msg.c_str(),
         (unsigned long)gladiusParryEvents, (unsigned long)gladiusParrySuppressed);
  }
}

static bool gladiusExtractSsidIe(const uint8_t* p, int len, int pos, SniffEvent& ev) {
  while (pos + 2 <= len && pos < 230) {
    uint8_t id = p[pos];
    uint8_t sl = p[pos + 1];
    if (pos + 2 + sl > len) break;
    if (id == 0) {
      ev.ssidLen = min((uint8_t)32, sl);
      if (ev.ssidLen > 0) {
        memcpy(ev.ssid, p + pos + 2, ev.ssidLen);
        ev.ssid[ev.ssidLen] = '\0';
      } else {
        ev.ssid[0] = '\0';
      }
      return true;
    }
    pos += 2 + sl;
  }
  return false;
}

static void promiscRx(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT || !sniffQueue) return;

  wifi_promiscuous_pkt_t* pkt = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t* p = pkt->payload;
  int len = pkt->rx_ctrl.sig_len;

  if (!p || len < 24) return;

  uint8_t subtype = p[0] & 0xFC;

  // Beacon frame subtype: keep the existing JG/JG2 neighbor language.
  if (subtype == 0x80) {
    if (len < 38) return;
    SniffEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.eventType = 0;
    ev.subtype = subtype;
    memcpy(ev.bssid, p + 16, 6);
    memcpy(ev.sta, p + 10, 6);
    ev.rssi = (int8_t)pkt->rx_ctrl.rssi;
    ev.channel = wifiChannel;
    if (gladiusExtractSsidIe(p, len, 36, ev) && ev.ssidLen > 0) {
      xQueueSend(sniffQueue, &ev, 0);
    }
    return;
  }

  // GLAD_PARRY: somebody showed interest in our virtual beacons.
  // Raw Wi-Fi management frames usually do not reveal friendly device names.
  // v1.23 outputs one possible vendor/type guess + MAC tail in one temporary SSID line.
  if (subtype == 0x40 || subtype == 0x00 || subtype == 0x20 || subtype == 0xB0) {
    SniffEvent ev;
    memset(&ev, 0, sizeof(ev));
    ev.eventType = 1;
    ev.subtype = subtype;
    memcpy(ev.sta, p + 10, 6);       // transmitter/source station
    memcpy(ev.bssid, p + 16, 6);     // BSSID/third address for mgmt frames
    ev.rssi = (int8_t)pkt->rx_ctrl.rssi;
    ev.channel = wifiChannel;

    bool targetedOwnBssid = isOwnTextcastBssid(p + 4) || isOwnTextcastBssid(p + 16);
    bool interestingSsid = false;

    if (subtype == 0x40) {           // Probe Request: IEs start right after 24-byte header.
      gladiusExtractSsidIe(p, len, 24, ev);
      interestingSsid = gladiusInterestingParrySsid(ev.ssid);
    } else if (subtype == 0x00) {    // Association Request: fixed params 4 bytes, then IEs.
      gladiusExtractSsidIe(p, len, 28, ev);
      interestingSsid = gladiusInterestingParrySsid(ev.ssid);
    } else if (subtype == 0x20) {    // Reassociation Request: fixed params 10 bytes, then IEs.
      gladiusExtractSsidIe(p, len, 34, ev);
      interestingSsid = gladiusInterestingParrySsid(ev.ssid);
    } else {
      strlcpy(ev.ssid, "OWN_BSSID_AUTH", sizeof(ev.ssid));
      ev.ssidLen = strlen(ev.ssid);
    }

    if (targetedOwnBssid || interestingSsid) {
      xQueueSend(sniffQueue, &ev, 0);
    }
    return;
  }
}

static void startTextcastSniffer() {
  sniffQueue = xQueueCreate(16, sizeof(SniffEvent));
  if (!sniffQueue) {
    logf("SNIFF", "queue fail");
    return;
  }

  esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(&promiscRx);
  esp_wifi_set_promiscuous(true);
  logf("SNIFF", "promiscuous beacon listener enabled");
}

static void gladiusHoldSwarmChannel(const char* reason, bool force = false) {
  static uint32_t lastHoldMs = 0;
  uint32_t now = nowMs();
  if (!force && lastHoldMs && now - lastHoldMs < GLADIUS_SWARM_CHANNEL_HOLD_MS) return;
  if (!force && WiFi.status() == WL_CONNECTED) return;
  lastHoldMs = now;
  wifiChannel = FALLBACK_CHANNEL;

  esp_wifi_set_promiscuous(false);
  esp_wifi_set_channel(FALLBACK_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous_rx_cb(&promiscRx);
  esp_wifi_set_promiscuous(true);

  if (force || ((now / GLADIUS_SWARM_CHANNEL_HOLD_MS) % 4UL) == 0UL) {
    logf("WIFI", "swarm channel hold ch=%u reason=%s", (unsigned)FALLBACK_CHANNEL, reason ? reason : "?");
  }
}

static void swarmSniffTask(void* arg) {
  (void)arg;

  SniffEvent ev;

  while (true) {
    if (xQueueReceive(sniffQueue, &ev, pdMS_TO_TICKS(250))) {
      sniffRx++;

      if (ev.eventType == 1) {
        gladiusHandleParryEvent(ev);
        continue;
      }

      String ssid = String(ev.ssid);

      if (!(ssid.startsWith("JG|") || ssid.startsWith("JG2|"))) {
        sniffDrop++;
        continue;
      }

      updateNeighborFromBeacon(ev);
    }
  }
}

static void swarmAdaptTask(void* arg) {
  (void)arg;

  while (true) {
    if (nowMs() - lastSwarmAdaptMs >= SWARM_ADAPT_INTERVAL_MS) {
      lastSwarmAdaptMs = nowMs();
      rebuildSwarmStatsAndAdapt();
      updateMachineField(true);
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

// ============================================================
// Wi-Fi + Gemini oracle
// ============================================================

static void requestOracle(const String& reason, bool forced) {
  oracleReason = reason;
  oracleWanted = true;
  oracleForced = oracleForced || forced;
}

static String buildSwarmSummary() {
  String s;
  s.reserve(500);

  s += "Node Gladius in Janus swarm. ";
  s += "WiFi ";
  s += wifiOnline ? "online" : "offline";
  s += ". HPS ";
  s += String(lastHps);
  s += ", bestZ ";
  s += String(bestZ);
  s += ", shares ";
  s += String(sharesFound);
  s += ", jobs ";
  s += String(jobsSeen);
  s += ", rfMotion ";
  s += String(rfMotion, 1);
  s += ", rfPresence ";
  s += String(rfPresence, 1);
  s += ", espNowRx ";
  s += String(espRx);
  s += ", textcastNeighbors ";
  s += String(swarmNeighborCount);
  s += ", remoteBestZ ";
  s += String(swarmBestRemoteZ);
  s += ", sameJobNeighbors ";
  s += String(swarmSameJobCount);
  s += ", tailGexTop ";
  s += laneName(tailGexTopLane);
  s += " ";
  s += String(tailGexTopX100);
  s += ", tailGexConfidence ";
  s += String(tailGexTopConfidenceX100);
  s += ", reason ";
  s += oracleReason;
  s += ". Make a short radio-status SSID.";

  return s;
}

static String askGeminiForSSID(const String& summary) {
  if (!wifiOnline || WiFi.status() != WL_CONNECTED) return "";

  String url = String(GEMINI_HOST) + GEMINI_MODEL + ":generateContent?key=" + GEMINI_API_KEY;

  String prompt = "Return only one ASCII Wi-Fi SSID string, max 28 bytes. ";
  prompt += "No quotes, no markdown, no explanation. ";
  prompt += "It should sound like a compact Janus swarm oracle. State: ";
  prompt += summary;

  String payload = "{";
  payload += "\"contents\":[{\"parts\":[{\"text\":\"";
  payload += jsonEscape(prompt);
  payload += "\"}]}],";
  payload += "\"generationConfig\":{\"temperature\":0.65,\"maxOutputTokens\":16}";
  payload += "}";

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(6500);

  HTTPClient https;
  https.setTimeout(6500);

  oracleInFlight = true;

  bool beginOk = https.begin(client, url);
  if (!beginOk) {
    oracleInFlight = false;
    return "";
  }

  https.addHeader("Content-Type", "application/json");
  int code = https.POST(payload);
  String response = "";

  if (code > 0) {
    response = https.getString();
  }

  https.end();
  oracleInFlight = false;

  if (code != 200) {
    logf("ORACLE", "http=%d fail", code);
    return "";
  }

  String text = parseGeminiText(response);
  text = sanitizeSSIDAscii(text, 28);

  return text;
}

static String localOracleLine() {
  if (!wifiOnline) return "TOY CASTLE LINK LOST";

  if (lastShareMs && nowMs() - lastShareMs < GLADIUS_LED_SHARE_MS) {
    return "GLAD CLAIMS A SHARE";
  }

  if (minerHasJob) {
    String s = "GLAD MINES ";
    s += String(lastHps);
    s += "H BEST";
    s += String(bestZ);
    return sanitizeSSIDAscii(s, 28);
  }

  if (espRx > 0) {
    String s = "SWARM HEARD ";
    s += String(espRx);
    s += " SIGNALS";
    return sanitizeSSIDAscii(s, 28);
  }

  return "JANUS GLADIUS LISTENS";
}

static void oracleTask(void* arg) {
  (void)arg;

  while (true) {
    if (oracleWanted && textMode != TEXTCAST_QUIET) {
      if (gladiusBuzzJobActiveNow()) {
        // v1.22: never spend Wi-Fi/HTTP time while Buzz is feeding real pool jobs.
        vTaskDelay(pdMS_TO_TICKS(2000));
        continue;
      }
      uint32_t now = nowMs();
      uint32_t minGap = oracleForced ? ORACLE_FORCED_INTERVAL_MS : ORACLE_MIN_INTERVAL_MS;

      if (lastOracleRequestMs == 0 || now - lastOracleRequestMs >= minGap) {
        oracleWanted = false;
        oracleForced = false;
        lastOracleRequestMs = now;

        String summary = buildSwarmSummary();
        logf("ORACLE", "request reason=%s", oracleReason.c_str());

        String ai = askGeminiForSSID(summary);

        if (ai.length() > 0) {
          oracleOk++;
          oracleLastText = String("JANUS ") + ai;
          fieldSet(FIELD_ORACLE, oracleLastText, 0, 3600, 1050);
          logf("ORACLE", "ok text='%s'", oracleLastText.c_str());
        } else {
          oracleFail++;
          oracleLastText = localOracleLine();
          fieldSet(FIELD_ORACLE, oracleLastText, 0, 3600, 1050);
          logf("ORACLE", "fallback text='%s'", oracleLastText.c_str());
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static void wifiEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
  (void)arg;

  if (!wifiQueue) return;

  WifiEventItem ev;
  memset(&ev, 0, sizeof(ev));

  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
    ev.type = 1;
    xQueueSend(wifiQueue, &ev, 0);
  } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
    wifi_event_sta_disconnected_t* d = (wifi_event_sta_disconnected_t*)event_data;
    ev.type = 3;
    ev.reason = d ? d->reason : 0;
    xQueueSend(wifiQueue, &ev, 0);
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* ip = (ip_event_got_ip_t*)event_data;
    ev.type = 2;
    ev.ip = ip ? ip->ip_info.ip.addr : 0;
    xQueueSend(wifiQueue, &ev, 0);
  }
}

static void startEspNow(const char* reason);

static void wifiMonitorTask(void* arg) {
  (void)arg;

  WifiEventItem ev;

  while (true) {
    if (xQueueReceive(wifiQueue, &ev, pdMS_TO_TICKS(1000))) {
      if (ev.type == 1) {
        wifiOnline = false;
        fieldSet(FIELD_EVENT, "TOY CASTLE CONNECTING", EVENT_FIELD_LIFETIME_MS, 3200, 800);
        logf("WIFI", "STA_CONNECTED");
      } else if (ev.type == 2) {
        wifiOnline = true;
        lastWifiConnectMs = nowMs();

        uint8_t ch = FALLBACK_CHANNEL;
        wifi_second_chan_t second;
        esp_wifi_get_channel(&ch, &second);
        wifiChannel = ch ? ch : FALLBACK_CHANNEL;

        IPAddress ip(ev.ip);
        logf("WIFI", "GOT_IP ip=%s ch=%u rssi=%d", ip.toString().c_str(), wifiChannel, WiFi.RSSI());

        fieldSet(FIELD_EVENT, "TOY CASTLE LINK OK", EVENT_FIELD_LIFETIME_MS, 3200, 800);
        startEspNow("wifi-got-ip");
        updateMachineField(true);
        requestOracle("wifi online", true);
      } else if (ev.type == 3) {
        wifiOnline = false;
        lastWifiLostMs = nowMs();

        logf("WIFI", "DISCONNECTED reason=%ld", (long)ev.reason);
        fieldSet(FIELD_EVENT, "TOY CASTLE LINK LOST", EVENT_FIELD_LIFETIME_MS, 3200, 700);

        gladiusHoldSwarmChannel("sta_disconnect", true);
      }
    }

    if (WiFi.status() != WL_CONNECTED) {
      wifiOnline = false;

      static uint32_t lastReconnectTry = 0;
      gladiusHoldSwarmChannel("sta_offline", false);
      if (nowMs() - lastReconnectTry > GLADIUS_WIFI_RECONNECT_MIN_MS) {
        lastReconnectTry = nowMs();
        wifiReconnectAttempts++;
        logf("WIFI", "reconnect attempt=%lu", (unsigned long)wifiReconnectAttempts);
        WiFi.disconnect(false, false);
        delay(20);
        WiFi.begin(TOY_WIFI_SSID, TOY_WIFI_PASS);
      }
    } else {
      wifiOnline = true;

      static uint32_t lastRssiUpdate = 0;
      if (nowMs() - lastRssiUpdate > 5000UL) {
        lastRssiUpdate = nowMs();
        lastRssi = (int8_t)WiFi.RSSI();
      }
    }
  }
}

static void startWifiSta() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  esp_wifi_set_ps(WIFI_PS_NONE);

  esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifiEventHandler, nullptr);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifiEventHandler, nullptr);

  WiFi.begin(TOY_WIFI_SSID, TOY_WIFI_PASS);

  logf("WIFI", "connecting ssid='%s'", TOY_WIFI_SSID);
}

// ============================================================
// ESP-NOW
// ============================================================

static uint8_t adaptiveBatchSize();
static void sendColonyHeartbeatPacket(bool force);
static void sendSwarmSensePacket(bool force);
static void sendGladiusPnCortex(bool force);
static uint8_t gladiusSchedulerHint();
static void gladiusTranceptionLiteTick(uint32_t now);
static void tailGexSendMemoryIfDue(bool force, uint16_t flags);
static void gladiusPresenceBurst(const char* reason);
static void gladiusRadioWatchdog();


static void rfUpdateFromPacket(int8_t rssi) {
  uint32_t now = nowMs();
  espRx++;
  lastRxMs = now;
  lastRssi = rssi;

  float old = rssiEma;
  if (rssiEma < -120.0f) rssiEma = rssi;
  rssiEma = rssiEma * 0.88f + (float)rssi * 0.12f;

  float drift = fabsf(rssiEma - old);
  rfMotion = rfMotion * 0.78f + drift * 0.22f;
  rfPresence = rfPresence * 0.90f + constrain((float)(rssi + 95) / 55.0f, 0.0f, 1.0f) * 0.10f;
  rfEntropy = rfEntropy * 0.92f + min(1.0f, drift / 12.0f) * 0.08f;

  if (rfMotion > 2.0f) rfState = RF_MOTION;
  else if (rfPresence > 0.45f) rfState = RF_PRESENT;
  else rfState = RF_IDLE;

  float rfBond = (rfState == RF_PRESENT) ? 0.06f : ((rfState == RF_MOTION) ? 0.04f : 0.02f);
  oxytocin += rfBond;
  oxytocin -= (rfEntropy > 0.90f ? 0.08f : 0.0f);
  oxytocin = constrain(oxytocin, 0.0f, 100.0f);
}

static void onNowSent(const wifi_tx_info_t* tx_info, esp_now_send_status_t status) {
  (void)tx_info;
  if (status == ESP_NOW_SEND_SUCCESS) espTxOk++;
  else espTxFail++;
}

static void onNowRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (!data || len <= 0 || len > 250 || !nowQueue) return;

  NowRxItem item;
  memset(&item, 0, sizeof(item));

  if (info && info->src_addr) {
    memcpy(item.mac, info->src_addr, 6);
  }

  int8_t rssi = -127;
  if (info && info->rx_ctrl) {
    rssi = (int8_t)info->rx_ctrl->rssi;
  }

  item.rssi = rssi;
  item.len = (uint8_t)len;
  memcpy(item.data, data, len);

  xQueueSend(nowQueue, &item, 0);
}

static void startEspNow(const char* reason = "boot") {
  if (!nowQueue) nowQueue = xQueueCreate(16, sizeof(NowRxItem));

  esp_now_deinit();
  delay(10);

  esp_err_t err = esp_now_init();
  if (err != ESP_OK) {
    logf("ESPNOW", "init fail err=%d reason=%s", (int)err, reason ? reason : "?");
    return;
  }

  esp_now_register_recv_cb(onNowRecv);
  esp_now_register_send_cb(onNowSent);

  esp_now_peer_info_t peer;
  memset(&peer, 0, sizeof(peer));
  memcpy(peer.peer_addr, ESPNOW_BROADCAST, 6);
  peer.channel = wifiChannel ? wifiChannel : FALLBACK_CHANNEL;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;

  if (!esp_now_is_peer_exist(ESPNOW_BROADCAST)) {
    err = esp_now_add_peer(&peer);
    logf("ESPNOW", "add broadcast peer err=%d", (int)err);
  }
  if (buzzMasterMacKnown) ensureBuzzMasterPeer(reason);
}

static void gladiusRadioRescue(const char* reason) {
  uint32_t now = nowMs();
  if (now - radioLastRescueMs < GLADIUS_RADIO_RESCUE_MIN_MS) return;
  radioLastRescueMs = now;
  radioRescueCount++;
  logf("RADIO/RESCUE", "reason=%s n=%lu wifi=%u ch=%u rxAge=%lu tx=%lu/%lu heap=%lu",
       reason ? reason : "?", (unsigned long)radioRescueCount,
       wifiOnline ? 1 : 0, (unsigned)wifiChannel,
       (unsigned long)(lastRxMs ? janusSafeAgeMs(now, lastRxMs, 999999UL) : 999999UL),
       (unsigned long)espTxOk, (unsigned long)espTxFail,
       (unsigned long)ESP.getFreeHeap());
  gladiusHoldSwarmChannel("radio_rescue", true);
  startEspNow(reason ? reason : "radio-rescue");
  gladiusPresenceBurst(reason ? reason : "radio-rescue");
  radioLastTxOkSeen = espTxOk;
  radioLastTxFailSeen = espTxFail;
}

static void gladiusRadioWatchdog() {
  uint32_t now = nowMs();
  if (now - radioLastWatchMs < 2500UL) return;
  radioLastWatchMs = now;

  bool peerMissing = !esp_now_is_peer_exist(ESPNOW_BROADCAST);
  bool txFailStreak = (espTxFail >= radioLastTxFailSeen + GLADIUS_RADIO_TX_FAIL_DELTA &&
                       espTxOk == radioLastTxOkSeen);
  // Broadcast ESP-NOW tx can stay "OK" while no peers answer, so RX silence alone
  // must be enough to rebuild the radio path after an overnight blackout.
  bool rxBlackout = (now > GLADIUS_RADIO_RX_BLACKOUT_MS &&
                     (!lastRxMs || janusSafeAgeMs(now, lastRxMs, 999999UL) > GLADIUS_RADIO_RX_BLACKOUT_MS));
  uint32_t masterAge = lastBuzzMasterMs ? janusSafeAgeMs(now, lastBuzzMasterMs, 999999UL) : 999999UL;
  bool masterBlackout = (now > GLADIUS_RADIO_MASTER_BLACKOUT_MS &&
                         masterAge > GLADIUS_RADIO_MASTER_BLACKOUT_MS);
  bool directBlackout = buzzMasterMacKnown && buzzMasterDirectFail >= buzzMasterDirectOk + 8UL &&
                        masterAge > 4500UL;

  if (peerMissing) gladiusRadioRescue("peer-missing");
  else if (txFailStreak) gladiusRadioRescue("tx-fail-streak");
  else if (directBlackout) gladiusRadioRescue("buzz-direct-blackout");
  else if (masterBlackout) gladiusRadioRescue("master-blackout");
  else if (rxBlackout) gladiusRadioRescue("rx-blackout");

  if (masterBlackout) gladiusPresenceBurst("master-blackout");

  if (espTxOk != radioLastTxOkSeen || espTxFail != radioLastTxFailSeen) {
    radioLastTxOkSeen = espTxOk;
    radioLastTxFailSeen = espTxFail;
  }
}

static void sendStatusPacket() {
  GladiusStatusPacket p;
  memset(&p, 0, sizeof(p));

  p.magic[0] = 'G';
  p.magic[1] = 'L';
  p.version = 1;
  p.nodeRole = 7;
  p.nodeId = nodeId;
  p.uptimeMs = nowMs();
  p.seq = ++gladiusSeq;
  p.jobId = currentJob.jobId;
  p.hps = lastHps;
  p.hpsEma = (uint32_t)hpsEma;
  p.totalHashes = totalHashes;
  p.shares = sharesFound;
  p.jobsSeen = jobsSeen;
  p.txOk = espTxOk;
  p.txFail = espTxFail;
  p.bestZ = gladiusReportBestBits();
  p.targetBits = currentJob.targetBits;
  p.lane = activeLane;
  p.rfState = (uint8_t)rfState;
  p.oxytocin = (uint8_t)constrain((int)oxytocin, 0, 100);
  p.led = ledBrightness;
  p.rssi = lastRssi;
  p.tickerMode = (uint8_t)textMode;
  p.crc = 0;
  p.crc = crc32ish(&p, sizeof(p) - 4);

  sendNowBroadcast("G/L", (uint8_t*)&p, sizeof(p));
}

static void sendColonyHeartbeatPacket(bool force) {
  uint32_t now = nowMs();
  if (!force && lastColonyHeartbeatMs && now - lastColonyHeartbeatMs < BUZZ_BRIDGE_HEARTBEAT_MS) return;
  lastColonyHeartbeatMs = now;

  JanusColonyPacket p;
  memset(&p, 0, sizeof(p));
  memcpy(p.magic, "JANUS", 6);
  snprintf(p.nodeId, sizeof(p.nodeId), "Gladius");
  snprintf(p.role, sizeof(p.role), "GLADIUS");
  p.seq = ++colonyHeartbeatSeq;
  p.hashRate = lastHps;
  p.shares = buzzSharesSent;
  p.rejects = buzzWeakTickets;
  p.bestBits = gladiusReportBestBits();
  p.diff = 0.0f;
  p.targetBits = currentJob.targetBits;
  p.aiBatch = (uint16_t)minerLoadPct;
  p.aiHint = gladiusSchedulerHint();
  p.jobAgeMs = minerJobStartedMs ? (now - minerJobStartedMs) : 0;
  p.rssi = lastRssi;
  p.uptime = now / 1000UL;
  bool bcastOk = sendNowBroadcast("JANUS", (uint8_t*)&p, sizeof(p));
  esp_err_t directErr = sendNowToBuzzMaster("JANUS-direct", (uint8_t*)&p, sizeof(p));
  static uint32_t lastDirectLogMs = 0;
  if (force || now - lastDirectLogMs > 20000UL || (!bcastOk && directErr != ESP_OK)) {
    lastDirectLogMs = now;
    logf("RADIO/MASTER", "hb bcast=%u direct=%d known=%u ch=%u peerCh=%u H=%lu best=%u masterAge=%lu direct=%lu/%lu",
         bcastOk ? 1U : 0U,
         (int)directErr,
         buzzMasterMacKnown ? 1U : 0U,
         (unsigned)wifiChannel,
         (unsigned)buzzMasterPeerChannel,
         (unsigned long)p.hashRate,
         (unsigned)p.bestBits,
         (unsigned long)(lastBuzzMasterMs ? janusSafeAgeMs(now, lastBuzzMasterMs, 999999UL) : 999999UL),
         (unsigned long)buzzMasterDirectOk,
         (unsigned long)buzzMasterDirectFail);
  }
}

static void sendSwarmSensePacket(bool force) {
  uint32_t now = nowMs();
  if (!force && lastSwarmSenseTxMs && now - lastSwarmSenseTxMs < BUZZ_BRIDGE_SWARMSENSE_MS) return;
  lastSwarmSenseTxMs = now;

  SwarmSensePacket ss;
  memset(&ss, 0, sizeof(ss));
  ss.magic[0] = 'S';
  ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = nodeId;
  snprintf(ss.nodeId, sizeof(ss.nodeId), "Gladius");
  snprintf(ss.kind, sizeof(ss.kind), "%s", currentJobIsBuzz ? "buzz_worker" : "tailgex");
  ss.seq = ++swarmSenseSeq;
  ss.uptime_ms = now;
  ss.micros_tail = (uint32_t)micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = 0;
  ss.loop_max_us = 0;
  ss.rssi = lastRssi;
  ss.radio_mode = wifiOnline ? 2 : 1;
  ss.bt_flags = currentJobIsBuzz ? 0x02 : (currentJobIsSelf ? 0x01 : 0x00);
  if (janusTwinPeerFresh()) ss.bt_flags |= 0x20;
  ss.palette = activeLane;
  ss.knn_label = tailGexTopLane;
  ss.knn_confidence = (uint8_t)constrain(max((int)tailGexTopConfidenceX100, (int)oxytocin), 0, 100);
  ss.ai_hint = gladiusSchedulerHint();
  ss.thermal_load = minerLoadPct;
  ss.effective_batch = adaptiveBatchSize();
  ss.dynamic_batch = ss.effective_batch;
  ss.hash_rate = lastHps;
  ss.total_hashes = totalHashes;
  ss.best_bits = gladiusReportBestBits();
  {
    uint32_t denom = ss.dynamic_batch ? (uint32_t)ss.dynamic_batch : 1UL;
    ss.hash_eff_x1000 = (uint16_t)constrain((int)((hpsEma * 1000.0f) / (float)denom), 0, 65535);
  }
  ss.prediction_error_x1000 = tailGexTopX100 * 10;
  ss.entropy_x1000 = (uint16_t)constrain((int)((rfEntropy + torricelliVacuum * 0.35f) * 1000.0f), 0, 65535);
  ss.touch_delta = (uint16_t)constrain((int)(rfMotion * 100.0f), 0, 65535);
  ss.job_age_s = minerJobStartedMs ? (uint16_t)min(65535UL, (now - minerJobStartedMs) / 1000UL) : 0;
  ss.nonce_remaining_l16 = (minerHasJob && currentJob.range > workerCursor) ? (uint16_t)((currentJob.range - workerCursor) & 0xFFFF) : 0;
  ss.flags = (currentJobIsBuzz ? 0x0001 : 0) | (currentJobIsSelf ? 0x0002 : 0) | (tailGexTopWeightPct ? 0x0004 : 0);
  if (oxytocin > 64.0f) ss.flags |= 0x0020;
  if (gladiusTranceptionHint >= 3) ss.flags |= 0x0040;
  sendNowBroadcast("S/S", (uint8_t*)&ss, sizeof(ss));
}

static uint16_t gladiusScaleX1000(float v, float lo, float hi) {
  if (hi <= lo) return 0;
  float span = hi - lo;
  return (uint16_t)constrain((int)((v - lo) * 1000.0f / span + 0.5f), 0, 65535);
}

static uint32_t gladiusCurrentJobSignature() {
  uint32_t h = 0x6C414432UL ^ ((uint32_t)nodeId << 16);
  h ^= currentJob.jobId;
  h ^= currentJob.startNonce ^ currentJob.range ^ currentJob.stride;
  h ^= currentJob.seedA ^ currentJob.seedB ^ currentJob.seedC ^ currentJob.seedD;
  h ^= ((uint32_t)currentJob.targetBits << 24) ^ ((uint32_t)currentJob.lane << 16) ^
       ((uint32_t)currentJob.arm << 8) ^ (uint32_t)currentJob.version;
  return crc32ish(&h, sizeof(h));
}

static void sendGladiusPnCortex(bool force) {
  uint32_t now = nowMs();
  if (!force && gladiusPnLastMs && now - gladiusPnLastMs < GLADIUS_PN_CORTEX_MS) return;
  gladiusPnLastMs = now;

  uint16_t target = currentJob.targetBits ? currentJob.targetBits : 22;
  uint16_t best = gladiusReportBestBits();
  uint16_t batch = adaptiveBatchSize();
  float heapPressure = 1.0f - constrain((float)ESP.getFreeHeap() / 240000.0f, 0.0f, 1.0f);
  float hashLoad = constrain((float)lastHps / 18000.0f, 0.0f, 2.5f);
  float batchLoad = constrain((float)batch / 96.0f, 0.0f, 2.0f);
  float gexConfidence = (float)tailGexTopConfidenceX100 / 100.0f;
  float gexTail = max(0.0f, (float)tailGexTopX100 / 10000.0f);
  float oxy = constrain(oxytocin / 100.0f, 0.0f, 1.0f);
  float thermal = constrain(0.12f + hashLoad * 0.46f + batchLoad * 0.18f + heapPressure * 0.28f + gexConfidence * 0.10f + oxy * 0.08f, 0.0f, 4.0f);
  float load = constrain(0.10f + hashLoad * 0.62f + batchLoad * 0.24f + (currentJobIsBuzz ? 0.18f : 0.0f) + oxy * 0.18f, 0.0f, 4.0f);
  float entropy = constrain(rfEntropy * 0.55f + gexConfidence * 0.90f + (float)tailGexTopWeightPct / 80.0f + torricelliVacuum * 0.45f + oxy * 0.25f, 0.0f, 6.0f);
  float tail = constrain(((float)best / (float)max((uint16_t)1, target)) + gexTail + gexConfidence * 0.35f + oxy * 0.10f, 0.0f, 6.0f);

  JanusPnCortexPacket pn{};
  pn.magic[0] = 'P'; pn.magic[1] = 'N';
  pn.version = 1;
  pn.role = JANUS_ROLE_GLADIUS_PN;
  pn.worker_id = nodeId;
  strlcpy(pn.nodeId, GLADIUS_NODE_NAME, sizeof(pn.nodeId));
  strlcpy(pn.kind, "gladius_pn_lab", sizeof(pn.kind));
  pn.seq = ++gladiusSeq;
  pn.uptime_ms = now;
  pn.job_sig = gladiusCurrentJobSignature();
  pn.prev_hash = gladiusPnPrevHash;
  pn.hash_rate = lastHps;
  pn.total_hashes = totalHashes;
  pn.target_bits = target;
  pn.best_bits = best;
  pn.lane = tailGexTopLane;
  pn.sector = currentJob.arm;
  pn.flags = 0;
  if (minerHasJob) pn.flags |= 0x01;
  if (currentJobIsBuzz) pn.flags |= 0x02;
  if (tailGexTopWeightPct || tailGexTopConfidenceX100 >= 18) pn.flags |= 0x04;
  if (gladiusRewardSprintActiveNow()) pn.flags |= 0x08;
  if (tailGexMemoryEpoch) pn.flags |= 0x10;
  if (oxytocin > 64.0f || torricelliVacuum > 0.66f) pn.flags |= 0x20;
  if (gladiusTranceptionHint >= 3) pn.flags |= 0x40;
  pn.rssi = lastRssi;
  pn.thermal_x1000 = gladiusScaleX1000(thermal, 0.0f, 4.0f);
  pn.load_x1000 = gladiusScaleX1000(load, 0.0f, 4.0f);
  pn.jitter_us = gladiusLoopJitterUs;
  pn.entropy_x1000 = gladiusScaleX1000(entropy, 0.0f, 6.0f);
  pn.tail_x1000 = gladiusScaleX1000(tail, 0.0f, 6.0f);
  pn.voltage_mv = 0;
  pn.ir_phase = (uint16_t)((pn.job_sig ^ pn.prev_hash ^ ((uint32_t)tailGexTopLane << 11) ^
                            ((uint32_t)tailGexTopWeightPct << 3) ^ (uint32_t)bestNonce ^
                            ((uint32_t)(torricelliVacuum * 1000.0f) << 1) ^
                            (uint32_t)(oxytocin * 10.0f) ^
                            ((uint32_t)(gladiusTranceptionLiteScore * 1000.0f) << 4) ^
                            ((uint32_t)gladiusTranceptionLane << 12)) & 0xFFFFUL);
  pn.reserved = (uint16_t)constrain((int)(oxytocin * 10.0f), 0, 1000);
  pn.packet_hash = 0;
  pn.packet_hash = crc32ish(&pn, sizeof(pn));
  gladiusPnPrevHash = pn.packet_hash;

  esp_err_t err = esp_now_send(ESPNOW_BROADCAST, (uint8_t*)&pn, sizeof(pn));
  if (err == ESP_OK) {
    gladiusPnTx++;
    if ((gladiusPnTx & 0x07UL) == 1UL) {
      logf("GLADIUS/PN", "tx=%lu lane=%s sector=%u H=%lu best=%u/%u heat=%.2f load=%.2f tail=%.2f oxy=%.1f vac=%.2f tl=%.2f/%u flags=0x%02X",
           (unsigned long)gladiusPnTx, laneName(pn.lane), (unsigned)pn.sector,
           (unsigned long)pn.hash_rate, (unsigned)pn.best_bits, (unsigned)pn.target_bits,
           thermal, load, tail, oxytocin, torricelliVacuum,
           gladiusTranceptionLiteScore, (unsigned)gladiusTranceptionHint, (unsigned)pn.flags);
    }
  } else {
    espTxFail++;
    gladiusPnFail++;
    if ((gladiusPnFail & 0x07UL) == 1UL) {
      logf("GLADIUS/PN", "fail=%lu err=%d", (unsigned long)gladiusPnFail, (int)err);
    }
  }
}

static void gladiusPresenceBurst(const char* reason) {
  uint32_t now = nowMs();
  if (gladiusLastPresenceBurstMs &&
      janusSafeAgeMs(now, gladiusLastPresenceBurstMs, 0UL) < GLADIUS_PRESENCE_BURST_MIN_MS) {
    return;
  }
  gladiusLastPresenceBurstMs = now;
  gladiusPresenceBursts++;

  lastColonyHeartbeatMs = 0;
  lastSwarmSenseTxMs = 0;
  gladiusPnLastMs = 0;
  sendColonyHeartbeatPacket(true);
  sendSwarmSensePacket(true);
  sendGladiusPnCortex(true);
  janusTwinTaskBroadcast(true);
  tailGexSendMemoryIfDue(true, 0x0008);

  uint32_t masterAge = lastBuzzMasterMs ? janusSafeAgeMs(now, lastBuzzMasterMs, 999999UL) : 999999UL;
  logf("GLADIUS/PRESENCE", "burst=%lu reason=%s masterAge=%lums rxAge=%lums H=%lu best=%u job=%u tx=%lu/%lu direct=%lu/%lu known=%u heap=%lu",
       (unsigned long)gladiusPresenceBursts,
       reason ? reason : "?",
       (unsigned long)masterAge,
       (unsigned long)(lastRxMs ? janusSafeAgeMs(now, lastRxMs, 999999UL) : 999999UL),
       (unsigned long)lastHps,
       gladiusReportBestBits(),
       (minerHasJob && currentJobIsBuzz) ? 1 : 0,
       (unsigned long)espTxOk,
       (unsigned long)espTxFail,
       (unsigned long)buzzMasterDirectOk,
       (unsigned long)buzzMasterDirectFail,
       buzzMasterMacKnown ? 1U : 0U,
       (unsigned long)ESP.getFreeHeap());
}

static void broadcastCommand(const String& cmd) {
  GladiusCommandPacket p;
  memset(&p, 0, sizeof(p));

  p.magic[0] = 'G';
  p.magic[1] = 'C';
  p.version = 1;
  p.nodeId = nodeId;
  p.seq = ++gladiusSeq;
  p.uptimeMs = nowMs();

  String c = cmd;
  c.trim();
  c.toCharArray(p.command, sizeof(p.command));

  p.crc = 0;
  p.crc = crc32ish(&p, sizeof(p) - 4);

  sendNowBroadcast("G/M", (uint8_t*)&p, sizeof(p));
}

// ============================================================
// Miner
// ============================================================

static const char* laneName(uint8_t lane) {
  switch (lane) {
    case LANE_LINEAR: return "linear";
    case LANE_ZIM_REVERSE: return "zim_reverse";
    case LANE_ZIM_BANDIT: return "zim_bandit";
    case LANE_JANUS_CENTER: return "janus_center";
    case LANE_KNIGHT: return "knight";
    case LANE_BITREV: return "bitrev";
    case LANE_RANDOM: return "random";
    default: return "unknown";
  }
}

static uint8_t gladiusSchedulerHint() {
  uint8_t hint = buzzAgentHint ? buzzAgentHint : 1;
  if ((oxytocin > 72.0f || torricelliVacuum > 0.66f) && hint < 4) hint = 4;
  if (gladiusTranceptionHint > hint) hint = gladiusTranceptionHint;
  return (uint8_t)constrain((int)hint, 1, 4);
}

static void gladiusTranceptionLiteTick(uint32_t now) {
  if (gladiusTranceptionLastMs &&
      janusSafeAgeMs(now, gladiusTranceptionLastMs, 0UL) < 1300UL) {
    return;
  }
  gladiusTranceptionLastMs = now;

  uint16_t target = currentJob.targetBits ? currentJob.targetBits : 22;
  uint16_t best = gladiusReportBestBits();
  uint16_t safeTarget = target ? target : 1;
  float bestFit = constrain((float)best / (float)safeTarget, 0.0f, 1.65f);
  float hashFit = constrain((float)lastHps / 22000.0f, 0.0f, 1.35f);
  float txTotal = (float)(espTxOk + espTxFail + 8UL);
  float radioClean = constrain(1.0f - ((float)espTxFail / txTotal), 0.0f, 1.0f);
  float twin = janusTwinPeerFresh() ? 1.0f : 0.0f;
  float sameJob = janusTwinSameJobNow() ? 1.0f : 0.0f;
  float oxy = constrain(oxytocin / 100.0f, 0.0f, 1.0f);
  float gexConf = constrain((float)tailGexTopConfidenceX100 / 100.0f, 0.0f, 1.0f);
  float gexTail = constrain(max(0.0f, (float)tailGexTopX100 / 600.0f), 0.0f, 1.0f);
  float jobAgePenalty = 0.0f;
  if (minerHasJob && minerJobStartedMs) {
    uint32_t age = janusSafeAgeMs(now, minerJobStartedMs, 0UL);
    jobAgePenalty = age > BUZZ_JOB_TIMEOUT_MS ? 0.22f : constrain((float)age / 90000.0f, 0.0f, 0.12f);
  }

  float score = 0.0f;
  score += bestFit * 0.31f;
  score += hashFit * 0.16f;
  score += radioClean * 0.13f;
  score += twin * 0.08f + sameJob * 0.08f;
  score += torricelliVacuum * 0.07f + oxy * 0.06f;
  score += gexConf * 0.07f + gexTail * 0.04f;
  score -= jobAgePenalty;
  gladiusTranceptionLiteScore = constrain(score, 0.0f, 1.50f);

  uint8_t oldHint = gladiusTranceptionHint;
  if (gladiusTranceptionLiteScore >= 0.98f || best >= target) gladiusTranceptionHint = 4;
  else if (gladiusTranceptionLiteScore >= 0.78f) gladiusTranceptionHint = 3;
  else if (gladiusTranceptionLiteScore < 0.46f || radioClean < 0.72f) gladiusTranceptionHint = 2;
  else gladiusTranceptionHint = 1;

  if (gladiusTranceptionHint >= 3 && tailGexTopLane <= LANE_RANDOM && tailGexTopConfidenceX100 >= 18) {
    gladiusTranceptionLane = tailGexTopLane;
  } else if (gladiusTranceptionHint >= 4) {
    gladiusTranceptionLane = (best & 1U) ? LANE_ZIM_BANDIT : LANE_RANDOM;
  } else if (gladiusTranceptionHint == 2) {
    gladiusTranceptionLane = LANE_ZIM_REVERSE;
  } else {
    gladiusTranceptionLane = activeLane <= LANE_RANDOM ? activeLane : LANE_ZIM_REVERSE;
  }

  gladiusTranceptionReports++;
  if (oldHint != gladiusTranceptionHint || (gladiusTranceptionReports & 0x0FUL) == 1UL) {
    logf("GLADIUS/TL", "score=%.3f hint=%u lane=%s best=%u/%u H=%lu clean=%.2f twin=%u same=%u gex=%d/%u oxy=%.1f vac=%.2f wire=frozen",
         gladiusTranceptionLiteScore, (unsigned)gladiusTranceptionHint,
         laneName(gladiusTranceptionLane), (unsigned)best, (unsigned)target,
         (unsigned long)lastHps, radioClean, twin > 0.5f ? 1 : 0,
         sameJob > 0.5f ? 1 : 0, (int)tailGexTopX100,
         (unsigned)tailGexTopConfidenceX100, oxytocin, torricelliVacuum);
  }
}

// ============================================================
// Tail GEX: Gamma Exposure analogue for JANUS lanes
// ============================================================

static uint32_t tailGexCrcNoCrc(struct TailGexPersist& p) {
  uint32_t old = p.crc;
  p.crc = 0;
  uint32_t c = crc32ish(&p, sizeof(p));
  p.crc = old;
  return c;
}

static uint32_t tailGexDefaultStride(uint8_t lane) {
  switch (lane) {
    case LANE_LINEAR: return 1;
    case LANE_ZIM_REVERSE: return 17;
    case LANE_ZIM_BANDIT: return 4099;
    case LANE_JANUS_CENTER: return 257;
    case LANE_KNIGHT: return 65537;
    case LANE_BITREV: return 521;
    case LANE_RANDOM:
    default: return 0x9E3779B9UL;
  }
}

static void tailGexRecalcLane(uint8_t lane) {
  if (lane > LANE_RANDOM) return;

  TailGexLaneStats& s = tailGex[lane];
  if (s.hashes == 0) {
    s.tailGex = 0.0f;
    s.confidence = 0.0f;
    s.sign = 0;
    s.weightPct = 0;
    return;
  }

  float tailPts =
      // Micro-tail: ESP32 ночь должна давать сигнал, а не ждать невозможные z33+.
      (float)s.z16 * 0.08f +
      (float)s.z18 * 0.18f +
      (float)s.z20 * 0.45f +
      (float)s.z22 * 0.85f +
      // Full-tail: если вдруг поймали высокий хвост — это сильный якорь.
      (float)s.z24 * 1.6f +
      (float)s.z28 * 4.0f +
      (float)s.z30 * 7.0f +
      (float)s.z32 * 11.0f +
      (float)s.z33 * 18.0f +
      (float)s.z34 * 30.0f +
      (float)s.z35 * 48.0f +
      (float)s.z38 * 140.0f;

  float kh = max(1.0f, (float)s.hashes / 1024.0f);
  float density = tailPts / kh;
  float prev = s.densityEma;
  if (s.densityEma <= 0.0001f) s.densityEma = density;
  else s.densityEma = s.densityEma * 0.92f + density * 0.08f;

  float gamma = s.densityEma - prev;
  float exposure = min(1.0f, (float)s.hashes / 4096.0f);
  float sample = min(1.0f, ((float)s.z16 + (float)s.z18 + (float)s.z20 + (float)s.z22 + 1.0f) / 18.0f);
  float rareBoost = (s.bestZ >= 33) ? 1.0f : ((s.bestZ >= 24) ? 0.78f : ((s.bestZ >= 20) ? 0.55f : 0.32f));
  s.confidence = constrain(exposure * (0.42f + 0.58f * sample) * rareBoost, 0.0f, 1.0f);

  if (s.bestZ >= 24 || s.densityEma > 1.2f || gamma > 0.08f) {
    s.sign = +1;
  } else if (s.hashes > 8192 && s.z18 == 0 && s.bestZ < 18) {
    s.sign = -1;
  } else {
    s.sign = 0;
  }

  // v1.20: rewardBoost is the small local "ferment" from real Gladius tails,
  // Buzz OXY rewards and tiny twin split hints. It decays slowly and exists only
  // in RAM, so it cannot poison persisted GEX forever.
  float boost = (lane <= LANE_RANDOM) ? tailGexRewardBoost[lane] : 0.0f;
  if (lane <= LANE_RANDOM && boost > 0.0001f) {
    tailGexRewardBoost[lane] = max(0.0f, boost * 0.996f - 0.00005f);
  }

  float shapedConf = constrain(s.confidence + min(0.65f, boost * 0.55f), 0.0f, 1.0f);
  float raw = (s.densityEma + gamma * 4.0f + boost) * max(0.08f, shapedConf);
  if (s.sign < 0 && boost < 0.08f) raw = -fabsf(raw);
  if (s.sign == 0 && boost < 0.04f) raw *= 0.25f;

  s.tailGex = constrain(raw, -99.0f, 99.0f);

  bool unlockReady = (tailGexRewardEvents >= 12UL &&
                      s.confidence * 100.0f >= (float)TAIL_GEX_UNLOCK_MIN_CONF_PCT &&
                      (s.tailGex > 0.01f || boost > 0.035f || s.bestZ >= 16));

  if ((s.tailGex > 0.01f && shapedConf > 0.08f) || unlockReady) {
    int w = (int)(s.tailGex * 2.2f) + (int)(shapedConf * 20.0f) + (int)(boost * 45.0f);
    if (unlockReady) w = max(w, (int)TAIL_GEX_UNLOCK_MIN_WEIGHT_PCT);
    s.weightPct = (uint8_t)constrain(w, unlockReady ? (int)TAIL_GEX_UNLOCK_MIN_WEIGHT_PCT : 1, (int)TAIL_GEX_MAX_WEIGHT_PCT);
  } else {
    s.weightPct = 0;
  }
}

static void tailGexRecalcAll(bool logChange) {
  uint8_t oldTop = tailGexTopLane;
  int16_t oldScore = tailGexTopX100;

  float bestScore = -9999.0f;
  uint8_t bestLane = LANE_ZIM_REVERSE;

  for (uint8_t i = 0; i <= LANE_RANDOM; i++) {
    tailGexRecalcLane(i);
    float score = tailGex[i].tailGex;
    // v1.19: if scores are tied near zero, do not let lane 0/linear
    // win forever only because it is first in the array. Prefer active lane.
    if (score > bestScore || (fabsf(score - bestScore) < 0.0001f && i == activeLane)) {
      bestScore = score;
      bestLane = i;
    }
  }

  tailGexTopLane = bestLane;
  tailGexTopX100 = (int16_t)constrain((int)(tailGex[bestLane].tailGex * 100.0f), -32768, 32767);
  tailGexTopConfidenceX100 = (uint8_t)constrain((int)(tailGex[bestLane].confidence * 100.0f), 0, 100);
  tailGexTopWeightPct = tailGex[bestLane].weightPct;

  if (logChange && (oldTop != tailGexTopLane || abs((int)oldScore - (int)tailGexTopX100) > 40)) {
    logf("TAILGEX", "top=%s gex=%.2f conf=%u%% weight=%u%% bestZ=%u hashes=%lu",
         laneName(tailGexTopLane),
         tailGex[tailGexTopLane].tailGex,
         tailGexTopConfidenceX100,
         tailGexTopWeightPct,
         tailGex[tailGexTopLane].bestZ,
         (unsigned long)tailGex[tailGexTopLane].hashes);
  }
}

static void tailGexObserveHash(uint8_t lane, uint8_t z) {
  if (lane > LANE_RANDOM) lane = LANE_RANDOM;
  TailGexLaneStats& s = tailGex[lane];

  s.hashes++;
  tailGexObservations++;
  if (z > s.bestZ) s.bestZ = z;
  if (z >= 16 && s.z16 < 65535) s.z16++;
  if (z >= 18 && s.z18 < 65535) s.z18++;
  if (z >= 20 && s.z20 < 65535) s.z20++;
  if (z >= 22 && s.z22 < 65535) s.z22++;
  if (z >= 24 && s.z24 < 65535) s.z24++;
  if (z >= 28 && s.z28 < 65535) s.z28++;
  if (z >= 30 && s.z30 < 65535) s.z30++;
  if (z >= 32 && s.z32 < 65535) s.z32++;
  if (z >= 33 && s.z33 < 65535) s.z33++;
  if (z >= 34 && s.z34 < 65535) s.z34++;
  if (z >= 35 && s.z35 < 65535) s.z35++;
  if (z >= 38 && s.z38 < 65535) s.z38++;

  if ((s.hashes & 0x7F) == 0 || z >= 20) {
    tailGexRecalcLane(lane);
    tailGexDirty = true;
  }
}

static void tailGexAddCounter(uint16_t& value, uint16_t add) {
  uint32_t v = (uint32_t)value + (uint32_t)add;
  value = (uint16_t)((v > 65535UL) ? 65535UL : v);
}

static uint8_t tailGexRewardStrengthFor(uint8_t z, uint8_t targetBits) {
  int target = targetBits ? (int)targetBits : 22;
  if ((int)z >= target + 2) return 18;
  if ((int)z >= target) return 14;
  if ((int)z >= target - 1) return 10;
  if ((int)z >= 20) return 7;
  if ((int)z >= 18) return 4;
  if ((int)z >= 16) return 3;
  if ((int)z >= TAIL_GEX_REWARD_MIN_Z) return 1;
  return 0;
}

static void tailGexRewardLane(uint8_t lane, uint8_t z, const char* reason, uint8_t strength) {
  if (lane > LANE_RANDOM) lane = LANE_RANDOM;
  if (z < TAIL_GEX_REWARD_MIN_Z && strength == 0) return;
  if (strength == 0) strength = 1;
  if (strength > TAIL_GEX_REWARD_MAX_STRENGTH) strength = TAIL_GEX_REWARD_MAX_STRENGTH;

  bool isSplit = reason && strstr(reason, "split");
  bool isOxy = reason && strstr(reason, "oxy");
  bool isSelfSignal = !isSplit && !isOxy;

  // v1.20: Anchor's best is a direction hint, not Gladius' own near-hit.
  // Keep split useful for avoiding duplicated lanes, but do not let it fake z20/z22 near-tail wins.
  uint8_t learnedZ = z;
  if (isSplit && learnedZ > TAIL_GEX_SPLIT_HINT_Z) learnedZ = TAIL_GEX_SPLIT_HINT_Z;
  if (isSplit && strength > TAIL_GEX_SPLIT_HINT_STRENGTH) strength = TAIL_GEX_SPLIT_HINT_STRENGTH;

  TailGexLaneStats& s = tailGex[lane];
  if (learnedZ > s.bestZ) s.bestZ = learnedZ;

  // Virtual tail evidence: this is reward shaping for the allocator only.
  // It does not fabricate shares and never touches target/header/wire bytes.
  if (learnedZ >= 16) tailGexAddCounter(s.z16, strength);
  if (learnedZ >= 18) tailGexAddCounter(s.z18, strength);
  if (learnedZ >= 20) tailGexAddCounter(s.z20, strength);
  if (learnedZ >= 22) tailGexAddCounter(s.z22, strength);
  if (learnedZ >= 24) tailGexAddCounter(s.z24, strength);
  if (learnedZ >= 28) tailGexAddCounter(s.z28, strength);
  if (learnedZ >= 30) tailGexAddCounter(s.z30, strength);
  if (learnedZ >= 32) tailGexAddCounter(s.z32, strength);
  if (learnedZ >= 33) tailGexAddCounter(s.z33, strength);
  if (learnedZ >= 34) tailGexAddCounter(s.z34, strength);
  if (learnedZ >= 35) tailGexAddCounter(s.z35, strength);
  if (learnedZ >= 38) tailGexAddCounter(s.z38, strength);

  float zScore = max(0.0f, (float)learnedZ - 13.0f);
  float impulse = 0.018f * zScore + 0.022f * (float)strength;
  if (isSplit) impulse *= 0.38f;
  else if (isOxy) impulse *= 0.72f;
  else impulse *= 1.18f;

  s.densityEma = constrain(s.densityEma + impulse, 0.0f, 40.0f);
  s.confidence = constrain(s.confidence + 0.008f * (float)strength + 0.005f * zScore + (isSelfSignal ? 0.012f : 0.0f), 0.0f, 1.0f);

  float boostAdd = impulse * (isSplit ? 0.55f : (isOxy ? 0.95f : 1.75f));
  if (isSelfSignal && learnedZ >= TAIL_GEX_REWARD_MIN_Z) boostAdd += 0.025f;
  tailGexRewardBoost[lane] = constrain(tailGexRewardBoost[lane] + boostAdd, 0.0f, 2.40f);

  tailGexRecalcLane(lane);

  // Re-apply a tiny post-recalc lift, because low-exposure ESP32 slices can be
  // too short for pure density math to move the top lane.
  s.tailGex = constrain(s.tailGex + impulse * (0.35f + s.confidence) + tailGexRewardBoost[lane] * 0.12f, -99.0f, 99.0f);
  if (s.tailGex > 0.01f && s.confidence > 0.08f) {
    int w = (int)(s.tailGex * 2.3f) + (int)(s.confidence * 22.0f) + (int)(tailGexRewardBoost[lane] * 45.0f);
    if (tailGexRewardEvents >= 12UL && s.confidence * 100.0f >= (float)TAIL_GEX_UNLOCK_MIN_CONF_PCT) {
      w = max(w, (int)TAIL_GEX_UNLOCK_MIN_WEIGHT_PCT);
      tailGexWeightUnlockEvents++;
    }
    s.weightPct = (uint8_t)constrain(w, (int)1, (int)TAIL_GEX_MAX_WEIGHT_PCT);
  }

  tailGexRewardEvents++;
  if (!isSplit && learnedZ >= 20) tailGexNearMissEvents++;
  if (isSplit) { tailGexSplitRewardEvents++; tailGexPeerHintEvents++; }
  if (isOxy) tailGexOxyRewardEvents++;
  if (isSelfSignal && learnedZ >= TAIL_GEX_REWARD_MIN_Z) tailGexSelfTailEvents++;
  tailGexDirty = true;
  tailGexRecalcAll(false);

  uint32_t now = nowMs();
  if (z >= TAIL_GEX_REWARD_LOG_Z || tailGexRewardEvents == 1 || (now - tailGexLastRewardLogMs) > 30000UL) {
    tailGexLastRewardLogMs = now;
    logf("TAILGEX/RWD", "reason=%s lane=%s z=%u rawZ=%u str=%u top=%s x100=%d conf=%u%% weight=%u%% events=%lu self=%lu near=%lu split=%lu oxy=%lu unlock=%lu",
         reason ? reason : "tail",
         laneName(lane),
         (unsigned)learnedZ,
         (unsigned)z,
         (unsigned)strength,
         laneName(tailGexTopLane),
         (int)tailGexTopX100,
         tailGexTopConfidenceX100,
         tailGexTopWeightPct,
         (unsigned long)tailGexRewardEvents,
         (unsigned long)tailGexSelfTailEvents,
         (unsigned long)tailGexNearMissEvents,
         (unsigned long)tailGexSplitRewardEvents,
         (unsigned long)tailGexOxyRewardEvents,
         (unsigned long)tailGexWeightUnlockEvents);
  }
}

static void tailGexReset(const char* reason) {
  memset(tailGex, 0, sizeof(tailGex));
  tailGexMemoryEpoch++;
  tailGexTopLane = LANE_ZIM_REVERSE;
  tailGexTopX100 = 0;
  tailGexTopConfidenceX100 = 0;
  tailGexTopWeightPct = 0;
  tailGexDirty = true;
  prefs.remove("gexState");
  logf("TAILGEX", "reset reason=%s", reason ? reason : "manual");
}

static bool tailGexLoad() {
  TailGexPersist p;
  memset(&p, 0, sizeof(p));

  size_t n = prefs.getBytes("gexState", &p, sizeof(p));
  if (n != sizeof(p)) {
    logf("TAILGEX", "no persisted state");
    return false;
  }

  uint32_t calc = tailGexCrcNoCrc(p);
  if (p.magic != 0x31584547UL || p.version != 1 || p.crc != calc) {
    logf("TAILGEX", "persist invalid magic=%08lX version=%u crc=%08lX calc=%08lX",
         (unsigned long)p.magic, p.version, (unsigned long)p.crc, (unsigned long)calc);
    return false;
  }

  memset(tailGex, 0, sizeof(tailGex));
  tailGexMemoryEpoch = p.memoryEpoch;
  for (uint8_t i = 0; i <= LANE_RANDOM; i++) {
    tailGex[i].hashes = p.laneHashes[i];
    tailGex[i].z16 = p.laneZ16[i];
    tailGex[i].z18 = p.laneZ18[i];
    tailGex[i].z20 = p.laneZ20[i];
    tailGex[i].z22 = p.laneZ22[i];
    tailGex[i].z24 = p.laneZ24[i];
    tailGex[i].z28 = p.laneZ28[i];
    tailGex[i].z30 = p.laneZ30[i];
    tailGex[i].z32 = p.laneZ32[i];
    tailGex[i].z33 = p.laneZ33[i];
    tailGex[i].z34 = p.laneZ34[i];
    tailGex[i].z35 = p.laneZ35[i];
    tailGex[i].z38 = p.laneZ38[i];
    tailGex[i].bestZ = p.laneBestZ[i];
    tailGex[i].tailGex = (float)p.laneTailX100[i] / 100.0f;
    tailGex[i].densityEma = fabsf(tailGex[i].tailGex);
    tailGex[i].confidence = (float)p.laneConfX100[i] / 100.0f;
  }

  tailGexRecalcAll(false);
  logf("TAILGEX", "loaded epoch=%lu top=%s gex=%.2f conf=%u%%",
       (unsigned long)tailGexMemoryEpoch,
       laneName(tailGexTopLane),
       tailGex[tailGexTopLane].tailGex,
       tailGexTopConfidenceX100);
  return true;
}

static void tailGexSave(const char* reason, bool important) {
  uint32_t now = nowMs();
  if (!important && !tailGexDirty) return;
  if (!important && tailGexLastSaveMs && now - tailGexLastSaveMs < TAIL_GEX_SAVE_INTERVAL_MS) return;

  TailGexPersist p;
  memset(&p, 0, sizeof(p));

  p.magic = 0x31584547UL; // GEX1
  p.version = 1;
  p.nodeId = nodeId;
  p.memoryEpoch = ++tailGexMemoryEpoch;
  p.totalHashes = totalHashes;
  p.shares = sharesFound;
  p.bestZ = gladiusReportBestBits();
  p.topLane = tailGexTopLane;
  p.topWeightPct = tailGexTopWeightPct;

  for (uint8_t i = 0; i <= LANE_RANDOM; i++) {
    p.laneHashes[i] = tailGex[i].hashes;
    p.laneZ16[i] = tailGex[i].z16;
    p.laneZ18[i] = tailGex[i].z18;
    p.laneZ20[i] = tailGex[i].z20;
    p.laneZ22[i] = tailGex[i].z22;
    p.laneZ24[i] = tailGex[i].z24;
    p.laneZ28[i] = tailGex[i].z28;
    p.laneZ30[i] = tailGex[i].z30;
    p.laneZ32[i] = tailGex[i].z32;
    p.laneZ33[i] = tailGex[i].z33;
    p.laneZ34[i] = tailGex[i].z34;
    p.laneZ35[i] = tailGex[i].z35;
    p.laneZ38[i] = tailGex[i].z38;
    p.laneBestZ[i] = tailGex[i].bestZ;
    p.laneTailX100[i] = (int16_t)constrain((int)(tailGex[i].tailGex * 100.0f), -32768, 32767);
    p.laneConfX100[i] = (uint8_t)constrain((int)(tailGex[i].confidence * 100.0f), 0, 100);
  }

  p.crc = tailGexCrcNoCrc(p);
  prefs.putBytes("gexState", &p, sizeof(p));

  tailGexLastSaveMs = now;
  tailGexDirty = false;

  logf("TAILGEX", "saved epoch=%lu reason=%s top=%s gex=%.2f weight=%u%%",
       (unsigned long)tailGexMemoryEpoch,
       reason ? reason : "auto",
       laneName(tailGexTopLane),
       tailGex[tailGexTopLane].tailGex,
       tailGexTopWeightPct);
}

static void sendMemoryPacket(uint16_t flags) {
  GladiusMemoryPacket p;
  memset(&p, 0, sizeof(p));

  p.magic[0] = 'G';
  p.magic[1] = 'M';
  p.version = 1;
  p.nodeRole = 7;
  p.nodeId = nodeId;
  p.uptimeMs = nowMs();
  p.seq = ++gladiusSeq;
  p.jobId = currentJob.jobId;
  p.totalHashes = totalHashes;
  p.shares = sharesFound;
  p.jobsSeen = jobsSeen;
  p.bestZ = gladiusReportBestBits();
  p.targetBits = currentJob.targetBits;
  p.activeLane = activeLane;
  p.gexTopLane = tailGexTopLane;
  p.gexTailX100 = tailGexTopX100;
  p.gexConfidenceX100 = tailGexTopConfidenceX100;
  p.gexWeightPct = tailGexTopWeightPct;
  p.gexEntropyFloorPct = TAIL_GEX_EXPLORATION_FLOOR_PCT;
  p.memoryEpoch = tailGexMemoryEpoch;
  p.flags = flags;
  p.crc = 0;
  p.crc = crc32ish(&p, sizeof(p) - 4);

  sendNowBroadcast("T/G", (uint8_t*)&p, sizeof(p));
}

static void tailGexSendMemoryIfDue(bool force, uint16_t flags) {
  uint32_t now = nowMs();
  if (!force && tailGexLastSendMs && now - tailGexLastSendMs < TAIL_GEX_SEND_INTERVAL_MS) return;

  tailGexLastSendMs = now;
  sendMemoryPacket(flags);
  logf("MEM", "sent G/M flags=0x%04X epoch=%lu top=%s gex=%d conf=%u%%",
       flags,
       (unsigned long)tailGexMemoryEpoch,
       laneName(tailGexTopLane),
       (int)tailGexTopX100,
       tailGexTopConfidenceX100);
}

static uint8_t tailGexChooseLane(uint8_t baseLane) {
  if (baseLane > LANE_RANDOM) baseLane = LANE_ZIM_REVERSE;

  uint8_t roll = (uint8_t)(esp_random() % 100);

  if (roll < TAIL_GEX_EXPLORATION_FLOOR_PCT) {
    tailGexProtectedExplores++;
    return (uint8_t)(esp_random() % (LANE_RANDOM + 1));
  }

  if (gladiusTranceptionHint >= 3 &&
      gladiusTranceptionLane <= LANE_RANDOM &&
      roll < (uint8_t)min(100, 22 + (int)(gladiusTranceptionLiteScore * 38.0f))) {
    return gladiusTranceptionLane;
  }

  if (tailGexTopWeightPct > 0 &&
      tailGexTopConfidenceX100 >= 18 &&
      tailGexTopLane <= LANE_RANDOM &&
      roll < (uint8_t)min(100, (int)TAIL_GEX_EXPLORATION_FLOOR_PCT + (int)tailGexTopWeightPct)) {
    tailGexLeaderUses++;
    return tailGexTopLane;
  }

  return baseLane;
}

static void janusTwinApplyBuzzLanePact(JanusJobLite& j) {
#if JANUS_TWIN_TASK_ENABLE
  if (!janusTwinPeerFresh()) return;
  if (janusTwinPeerRole != JANUS_TWIN_ROLE_ANCHOR) return;
  if (!j.jobId || j.jobId != janusTwinPeerJobFp32) return;

  uint32_t now = nowMs();
  if (gladiusRewardSprintActiveNow()) {
    gladiusTwinLanePactSprintDrops++;
    return;
  }
  uint32_t pactAge = janusSafeAgeMs(now, gladiusTwinLastLanePactMs, 999999UL);
  if (pactAge < GLADIUS_TWIN_LANE_PACT_COOLDOWN_MS) {
    gladiusTwinLanePactCooldownDrops++;
    return;
  }
  gladiusTwinLastLanePactMs = now;

  uint8_t oldLane = j.lane;
  uint32_t oldStride = j.stride;
  uint8_t pick = (uint8_t)((nodeId ^ janusTwinPeerNode ^ j.jobId ^ now) % 3);
  if (pick == 0) j.lane = LANE_BITREV;
  else if (pick == 1) j.lane = LANE_JANUS_CENTER;
  else j.lane = LANE_RANDOM;
  j.arm = (uint8_t)((j.arm + 5 + pick) % ZIM_STRIDE_ARM_COUNT);
  j.stride = (j.lane == LANE_RANDOM) ? (ZIM_STRIDE_ARMS[j.arm] | 1UL) : tailGexDefaultStride(j.lane);
  j.seedD ^= 0xBB67AE85UL ^ ((uint32_t)janusTwinPeerNode << 16);
  janusTwinSplitApplied++;
  logf("TWIN", "lane pact with Anchor peer=%04X fp=%08lX lane %u->%u stride %lu->%lu splits=%lu cdDrop=%lu sprintDrop=%lu",
       janusTwinPeerNode, (unsigned long)j.jobId, oldLane, j.lane,
       (unsigned long)oldStride, (unsigned long)j.stride, (unsigned long)janusTwinSplitApplied,
       (unsigned long)gladiusTwinLanePactCooldownDrops, (unsigned long)gladiusTwinLanePactSprintDrops);
  if (janusTwinPeerBestBits >= 16) {
    // v1.20: this is peer scent only. Do not count Anchor's z21 as Gladius' own near-miss.
    tailGexRewardLane(j.lane, TAIL_GEX_SPLIT_HINT_Z, "twin_split", TAIL_GEX_SPLIT_HINT_STRENGTH);
  }
#else
  (void)j;
#endif
}

static String tailGexSummaryLine() {
  String s;
  s.reserve(220);
  s += "gex top=";
  s += laneName(tailGexTopLane);
  s += " x100=";
  s += String(tailGexTopX100);
  s += " conf=";
  s += String(tailGexTopConfidenceX100);
  s += "% weight=";
  s += String(tailGexTopWeightPct);
  s += "% best=";
  s += String(tailGex[tailGexTopLane].bestZ);
  s += " z18=";
  s += String(tailGex[tailGexTopLane].z18);
  s += " z20=";
  s += String(tailGex[tailGexTopLane].z20);
  s += " z22=";
  s += String(tailGex[tailGexTopLane].z22);
  s += " z33=";
  s += String(tailGex[tailGexTopLane].z33);
  s += " z35=";
  s += String(tailGex[tailGexTopLane].z35);
  s += " z38=";
  s += String(tailGex[tailGexTopLane].z38);
  s += " epoch=";
  s += String(tailGexMemoryEpoch);
  return s;
}

static uint32_t nextNonceForLane(const struct JanusJobLite& j, uint32_t i) {
  uint32_t range = j.range ? j.range : 262144UL;
  uint32_t start = j.startNonce;
  uint32_t seed = j.seedA ^ j.seedB ^ j.seedC ^ j.seedD ^ j.jobId;
  uint32_t stride = j.stride ? (j.stride | 1UL) : (ZIM_STRIDE_ARMS[j.arm % ZIM_STRIDE_ARM_COUNT] | 1UL);
  uint8_t lane = j.lane;

  switch (lane) {
    case LANE_LINEAR:
      return start + (i % range);

    case LANE_ZIM_REVERSE:
      return start + ((seed - i * stride) % range);

    case LANE_ZIM_BANDIT: {
      uint32_t wobble = bitReverse32(seed ^ (i * 0xA5A5A5A5UL)) & 0xFFFFUL;
      return start + ((seed + wobble - i * stride) % range);
    }

    case LANE_JANUS_CENTER: {
      uint32_t center = range / 2;
      uint32_t step = (i + 1) / 2;
      uint32_t off = center + ((i & 1) ? step : (range - (step % range)));
      return start + (off % range);
    }

    case LANE_KNIGHT: {
      uint32_t stride2 = 2654435761UL | 1UL;
      return start + ((seed + i * stride2) % range);
    }

    case LANE_BITREV:
      return start + (bitReverse32(seed + i) % range);

    case LANE_RANDOM:
    default: {
      uint32_t x = seed + 0x9E3779B9UL * (i + 1);
      x ^= x >> 16;
      x *= 0x7FEB352DUL;
      x ^= x >> 15;
      x *= 0x846CA68BUL;
      x ^= x >> 16;
      return start + (x % range);
    }
  }
}

static void buildHashInput(const struct JanusJobLite& j, uint32_t nonce, uint8_t* buf, size_t& len) {
  uint32_t words[16];

  words[0] = 0x474C4144UL; // GLAD
  words[1] = j.jobId;
  words[2] = j.seedA;
  words[3] = j.seedB;
  words[4] = j.seedC;
  words[5] = j.seedD;
  words[6] = j.startNonce;
  words[7] = j.range;
  words[8] = nonce;
  words[9] = ((uint32_t)j.targetBits << 24) | ((uint32_t)j.lane << 16) | ((uint32_t)j.arm << 8) | (uint32_t)j.version;
  words[10] = nodeId;
  words[11] = nowMs();
  words[12] = j.stride;
  words[13] = totalHashes;
  words[14] = 0x54455854UL; // TEXT
  words[15] = 0x43415354UL; // CAST

  memcpy(buf, words, sizeof(words));
  len = sizeof(words);
}

static void sendBuzzSharePacket(const struct JanusJobLite& j, const uint8_t buzzJobId[8], uint32_t nonce, uint16_t bits, const uint8_t shareHashBE[32]) {
  BuzzShareResponseV2 p;
  memset(&p, 0, sizeof(p));
  p.magic[0] = 'S';
  p.magic[1] = '2';
  memcpy(p.job_id, buzzJobId, 8);
  p.nonce = nonce;
  p.worker_id = nodeId;
  p.bits = bits;
  p.total_hashes_l32 = totalHashes;
  memcpy(p.hash_tail, shareHashBE + 28, 4);

  esp_err_t err = esp_now_send(ESPNOW_BROADCAST, (uint8_t*)&p, sizeof(p));
  esp_err_t directErr = sendNowToBuzzMaster("S2-direct", (uint8_t*)&p, sizeof(p));
  if (err == ESP_OK || directErr == ESP_OK) {
    sharesFound++;
    buzzSharesSent++;
  } else {
    espTxFail++;
  }

  lastShareMs = nowMs();
  lastShareBits = bits;
  ledShareFlashUntilMs = lastShareMs + SHARE_FIELD_LIFETIME_MS;
  lastLedMs = 0;
  janusFaceBroadcast(true, bits);
  janusTwinTaskBroadcast(true);

  fieldSet(FIELD_EVENT, "GLAD BUZZ S2 SHARE", SHARE_FIELD_LIFETIME_MS, 2600, 420);
  tailGexRecalcAll(true);
  tailGexSave("buzz_share", true);
  tailGexSendMemoryIfDue(true, 0x0004 | (bits >= 24 ? 0x0010 : 0));
  sendColonyHeartbeatPacket(true);
  sendSwarmSensePacket(true);
  updateMachineField(true);

  logf("BUZZ/S2", "tx=%s direct=%d job=%08lX nonce=%08lX bits=%u target=%u lane=%s total=%lu buzzShares=%lu",
       err == ESP_OK ? "OK" : "FAIL",
       (int)directErr,
       (unsigned long)j.jobId,
       (unsigned long)nonce,
       (unsigned)bits,
       j.targetBits,
       laneName(j.lane),
       (unsigned long)totalHashes,
       (unsigned long)buzzSharesSent);
}

static void sendSharePacket(const struct JanusJobLite& j, uint32_t nonce, uint8_t z, const uint8_t hash[32]) {
  bool selfJob = currentJobIsSelf && (currentJob.jobId == j.jobId);

  GladiusSharePacket p;
  memset(&p, 0, sizeof(p));

  p.magic[0] = 'G';
  p.magic[1] = 'S';
  p.version = 1;
  p.nodeRole = 7;
  p.nodeId = nodeId;
  p.uptimeMs = nowMs();
  p.seq = ++gladiusSeq;
  p.jobId = j.jobId;
  p.nonce = nonce;
  p.startNonce = j.startNonce;
  p.range = j.range;
  p.totalHashes = totalHashes;
  p.zbits = z;
  p.targetBits = j.targetBits;
  p.lane = j.lane;
  p.arm = j.arm;
  memcpy(p.hash8, hash, 8);
  p.crc = 0;
  p.crc = crc32ish(&p, sizeof(p) - 4);

  // Внутренний self-job нужен для ночной TailGEX памяти.
  // Его proof НЕ отправляем как G/S share, чтобы потом Buzz/Core не спутали его с реальным pool job.
  if (!selfJob) {
    sendNowBroadcast("A/R", (uint8_t*)&p, sizeof(p));
    sharesFound++;
  } else {
    selfProofsFound++;
  }

  lastShareMs = nowMs();
  lastShareBits = z;
  ledShareFlashUntilMs = lastShareMs + SHARE_FIELD_LIFETIME_MS;
  lastLedMs = 0;
  janusFaceBroadcast(true, z);
  janusTwinTaskBroadcast(true);

  char msg[48];
  if (selfJob) snprintf(msg, sizeof(msg), "GLAD SELF Z%u BEST%u", z, bestZ);
  else snprintf(msg, sizeof(msg), "GLAD SHARE Z%u BEST%u", z, bestZ);

  fieldSet(FIELD_EVENT, msg, SHARE_FIELD_LIFETIME_MS, 2600, 420);
  tailGexRecalcAll(true);
  tailGexSave(selfJob ? "self_proof" : "share", true);
  tailGexSendMemoryIfDue(true, (selfJob ? 0x0020 : 0x0004) | (z >= 24 ? 0x0010 : 0));
  updateMachineField(true);

  if (!selfJob && z >= j.targetBits + 4) {
    requestOracle("high share", true);
  }

  logf(selfJob ? "PROOF" : "SHARE",
       "job=%08lX nonce=%08lX z=%u target=%u lane=%s total=%lu selfProofs=%lu realShares=%lu",
       (unsigned long)j.jobId,
       (unsigned long)nonce,
       z,
       j.targetBits,
       laneName(j.lane),
       (unsigned long)totalHashes,
       (unsigned long)selfProofsFound,
       (unsigned long)sharesFound);
}


static void makeSelfJob(const char* reason) {
  JanusJobLite j;
  memset(&j, 0, sizeof(j));

  uint32_t seed = esp_random() ^ nowMs() ^ totalHashes ^ ((uint32_t)nodeId << 16);

  j.magic[0] = 'J';
  j.magic[1] = 'G';
  j.version = 1;
  j.targetBits = SELF_JOB_TARGET_BITS;
  j.jobId = 0x51000000UL ^ seed;  // Q = quiet/local proof job
  j.startNonce = esp_random();
  j.range = SELF_JOB_RANGE;
  j.seedA = seed;
  j.seedB = esp_random();
  j.seedC = esp_random() ^ 0x474C4144UL;
  j.seedD = esp_random() ^ 0x54474558UL; // TGEX
  j.lane = tailGexTopLane <= LANE_RANDOM ? tailGexTopLane : LANE_ZIM_REVERSE;
  if (j.lane == LANE_LINEAR) j.lane = LANE_ZIM_REVERSE;
  j.arm = (uint8_t)(esp_random() % ZIM_STRIDE_ARM_COUNT);
  j.flags = 0x51; // self/local
  j.stride = tailGexDefaultStride(j.lane);
  j.expiresMs = 0;
  j.crc = 0;
  j.crc = crc32ish(&j, sizeof(j) - 4);

  portENTER_CRITICAL(&jobMux);
  currentJob = j;
  minerHasJob = true;
  currentJobIsSelf = true;
  currentJobIsBuzz = false;
  selfJobsMade++;
  jobsSeen++;
  workerCursor = 0;
  bestZ = 0;
  bestNonce = 0;
  gladiusReportBestReset(j.jobId);
  minerJobStartedMs = nowMs();
  lastSelfJobMs = minerJobStartedMs;
  activeLane = j.lane;
  activeArm = j.arm;
  activeStride = j.stride;
  portEXIT_CRITICAL(&jobMux);

  ledJobPulseUntilMs = nowMs() + 4500UL;
  fieldSet(FIELD_EVENT, "GLAD SELF JOB TAILGEX", EVENT_FIELD_LIFETIME_MS, 3000, 700);
  updateMachineField(true);

  logf("SELFJOB", "start reason=%s job=%08lX range=%lu target=%u lane=%s stride=%lu",
       reason ? reason : "fallback",
       (unsigned long)j.jobId,
       (unsigned long)j.range,
       j.targetBits,
       laneName(j.lane),
       (unsigned long)j.stride);
}

static void ensureSelfJobFallback() {
  if (minerHasJob) return;

  uint32_t now = nowMs();

  if (now < SELF_JOB_BOOT_DELAY_MS) return;

  if (lastSelfJobMs != 0 && now - lastSelfJobMs < SELF_JOB_RESTART_DELAY_MS) return;

  // Если Buzz/Core уже кормили real job, fallback всё равно остаётся,
  // но только когда job реально закончился/пропал.
  makeSelfJob(lastRealJobMs ? "idle_after_real_job" : "no_buzz_job");
}

static void makeDemoJob() {
  JanusJobLite j;
  memset(&j, 0, sizeof(j));

  j.magic[0] = 'J';
  j.magic[1] = 'G';
  j.version = 1;
  j.targetBits = 22;
  j.jobId = esp_random();
  j.startNonce = esp_random();
  j.range = 262144;
  j.seedA = esp_random();
  j.seedB = esp_random();
  j.seedC = esp_random();
  j.seedD = esp_random();
  j.lane = LANE_ZIM_REVERSE;
  j.arm = 5;
  j.flags = 0;
  j.stride = 17;
  j.expiresMs = 600000;
  j.crc = 0;
  j.crc = crc32ish(&j, sizeof(j) - 4);

  portENTER_CRITICAL(&jobMux);
  currentJob = j;
  minerHasJob = true;
  currentJobIsSelf = true;
  currentJobIsBuzz = false;
  selfJobsMade++;
  jobsSeen++;
  workerCursor = 0;
  bestZ = 0;
  bestNonce = 0;
  gladiusReportBestReset(j.jobId);
  minerJobStartedMs = nowMs();
  activeLane = j.lane;
  activeArm = j.arm;
  activeStride = j.stride;
  portEXIT_CRITICAL(&jobMux);

  ledJobPulseUntilMs = nowMs() + 6500UL;
  fieldSet(FIELD_EVENT, "GLAD DEMO SELF JOB", EVENT_FIELD_LIFETIME_MS, 3000, 700);

  logf("JOB", "demo job=%08lX start=%08lX range=%lu target=%u lane=%s stride=%lu",
       (unsigned long)j.jobId,
       (unsigned long)j.startNonce,
       (unsigned long)j.range,
       j.targetBits,
       laneName(j.lane),
       (unsigned long)j.stride);
}

static void applyBuzzJobImmediate(const struct BuzzJobPacket& bj, const uint8_t* fromMac) {
  if (bj.magic[0] != 'J' || bj.magic[1] != 'B') return;
  lastBuzzMasterMs = nowMs();
  rememberBuzzMasterMac(fromMac, "buzz-job");

  if (bj.range_size == 0) {
    // Buzz live-discovery ping: announce ourselves, but do not overwrite active unique work.
    // If Buzz only probes for a while, keep TailGEX warm locally instead of sitting at H=0.
    if (!minerHasJob &&
        nowMs() >= SELF_JOB_DISCOVERY_DELAY_MS &&
        (lastSelfJobMs == 0 || nowMs() - lastSelfJobMs >= SELF_JOB_RESTART_DELAY_MS)) {
      makeSelfJob("buzz_discovery_hold");
    }
    sendColonyHeartbeatPacket(true);
    sendSwarmSensePacket(true);
    sendGladiusPnCortex(true);
    janusTwinTaskBroadcast(true);
    gladiusBuzzDiscoveryPings++;
    if ((gladiusBuzzDiscoveryPings % GLADIUS_DISCOVERY_LOG_EVERY) == 1UL) {
      logf("BUZZ/JB", "discovery ping from=%s start=%08lX range=0 n=%lu",
           fromMac ? macToString(fromMac).c_str() : "unknown",
           (unsigned long)bj.start_nonce,
           (unsigned long)gladiusBuzzDiscoveryPings);
    }
    return;
  }

  buzzJobQAccepted++;

  JanusJobLite nj;
  memset(&nj, 0, sizeof(nj));
  uint32_t seed = fnv1a32(bj.header, 80, jobId32From8(bj.job_id) ^ bj.start_nonce ^ bj.range_size ^ bj.extranonce2);

  nj.magic[0] = 'J';
  nj.magic[1] = 'B';
  nj.version = 2;
  nj.targetBits = (uint8_t)constrain((int)countLeadingZeroBitsBE(bj.target), 1, 64);
  nj.jobId = jobId32From8(bj.job_id);
  nj.startNonce = bj.start_nonce;
  nj.range = bj.range_size ? bj.range_size : 262144UL;
  if (nj.range > 4194304UL) nj.range = 4194304UL;
  nj.seedA = seed;
  nj.seedB = fnv1a32(bj.target, 32, seed ^ 0x31564C44UL);      // V31 DualLock lane seed
  nj.seedC = bj.extranonce2 ^ 0x54434859UL;                    // tachyon-ish phase
  nj.seedD = totalHashes ^ nowMs() ^ ((uint32_t)nodeId << 16);
  nj.lane = tailGexChooseLane(LANE_ZIM_REVERSE);
  nj.arm = (uint8_t)((seed ^ (seed >> 8) ^ buzzEntropySeed) % ZIM_STRIDE_ARM_COUNT);
  nj.flags = 0xB2; // Buzz bridge + pool-compatible real header path
  nj.stride = ZIM_STRIDE_ARMS[nj.arm] | 1UL;
  if (tailGexTopWeightPct > 0 && tailGexTopConfidenceX100 >= 18) {
    nj.lane = tailGexTopLane;
    nj.stride = tailGexDefaultStride(nj.lane);
  }
  janusTwinApplyBuzzLanePact(nj);
  nj.expiresMs = BUZZ_JOB_TIMEOUT_MS;
  nj.crc = 0;
  nj.crc = crc32ish(&nj, sizeof(nj) - 4);

  portENTER_CRITICAL(&jobMux);
  currentJob = nj;
  memcpy(currentBuzzJobId, bj.job_id, 8);
  memcpy(currentBuzzHeader, bj.header, 80);
  memcpy(currentBuzzTarget, bj.target, 32);
  currentBuzzExtranonce2 = bj.extranonce2;
  minerHasJob = true;
  currentJobIsSelf = false;
  currentJobIsBuzz = true;
  realJobsSeen++;
  buzzJobsSeen++;
  jobsSeen++;
  workerCursor = 0;
  if (reportBestJobId != nj.jobId) gladiusReportBestReset(nj.jobId);
  bestZ = 0;
  bestNonce = 0;
  minerJobStartedMs = nowMs();
  lastRealJobMs = minerJobStartedMs;
  activeLane = nj.lane;
  activeArm = nj.arm;
  activeStride = nj.stride;
  portEXIT_CRITICAL(&jobMux);

  ledJobPulseUntilMs = nowMs() + 6500UL;
  uint32_t jobEventAge = janusSafeAgeMs(nowMs(), gladiusLastBuzzJobEventFieldMs, 999999UL);
  if (jobEventAge >= GLADIUS_ACTIVE_JOB_EVENT_FIELD_MS || bestZ + 2 >= nj.targetBits) {
    gladiusLastBuzzJobEventFieldMs = nowMs();
    fieldSet(FIELD_EVENT, "GLAD BUZZ J/B REAL JOB", EVENT_FIELD_LIFETIME_MS, 3000, 650);
  }
  sendColonyHeartbeatPacket(true);
  sendSwarmSensePacket(true);
  janusTwinTaskBroadcast(true);
  updateMachineField(false);
  tailGexSendMemoryIfDue(true, 0x0008);

  if ((buzzJobQAccepted % GLADIUS_BUZZ_JOB_LOG_EVERY) == 1UL ||
      bestZ + 2 >= nj.targetBits ||
      (lastShareMs && janusSafeAgeMs(nowMs(), lastShareMs, 999999UL) < JANUS_FACE_SWAP_MS)) {
    logf("BUZZ/JB", "from=%s jid32=%08lX start=%08lX range=%lu targetBits=%u lane=%s stride=%lu arm=%u en2=%08lX jobq=%lu/%lu/%lu dup=%lu stale=%lu quiet=1",
         fromMac ? macToString(fromMac).c_str() : "unknown",
         (unsigned long)nj.jobId,
         (unsigned long)nj.startNonce,
         (unsigned long)nj.range,
         nj.targetBits,
         laneName(nj.lane),
         (unsigned long)nj.stride,
         nj.arm,
         (unsigned long)bj.extranonce2,
         (unsigned long)buzzJobQAccepted,
         (unsigned long)buzzJobQQueued,
         (unsigned long)buzzJobQPromoted,
         (unsigned long)buzzJobQDupDrop,
         (unsigned long)buzzJobQStaleDrop);
  }
}

static void gladiusQueueBuzzJob(const BuzzJobPacket& bj, const uint8_t* fromMac, uint32_t fp, const char* reason) {
  portENTER_CRITICAL(&jobMux);
  queuedBuzzJob = bj;
  if (fromMac) memcpy(queuedBuzzJobMac, fromMac, 6);
  else memset(queuedBuzzJobMac, 0, sizeof(queuedBuzzJobMac));
  queuedBuzzJobFp32 = fp;
  queuedBuzzJobQueuedMs = nowMs();
  queuedBuzzJobValid = true;
  buzzJobQQueued++;
  portEXIT_CRITICAL(&jobMux);

  if ((buzzJobQQueued % GLADIUS_JOBQ_LOG_EVERY) == 1UL) {
    logf("BUZZ/JOBQ", "queue reason=%s fp=%08lX start=%08lX queued=%lu activeChecked=%lu best=%u H=%lu",
         reason ? reason : "same_fp_hold",
         (unsigned long)fp,
         (unsigned long)bj.start_nonce,
         (unsigned long)buzzJobQQueued,
         (unsigned long)workerCursor,
         bestZ,
         (unsigned long)lastHps);
  }
}

static bool gladiusPromoteQueuedBuzzJobIfReady(const char* reason, bool force=false) {
  BuzzJobPacket q;
  uint8_t qmac[6];
  uint32_t qfp = 0;
  uint32_t qage = 0;
  bool promote = false;
  bool stale = false;

  uint32_t now = nowMs();
  portENTER_CRITICAL(&jobMux);
  if (queuedBuzzJobValid) {
    qage = janusSafeAgeMs(now, queuedBuzzJobQueuedMs, 0);
    bool active = minerHasJob && currentJobIsBuzz;
    uint32_t activeAge = janusSafeAgeMs(now, minerJobStartedMs, 0);
    bool sameActive = active && currentJob.jobId == queuedBuzzJobFp32;
    bool minimumSliceDone = (activeAge >= GLADIUS_JOBQ_MIN_HOLD_MS) || (workerCursor >= GLADIUS_JOBQ_MIN_HASHES);
    bool oldEnough = activeAge >= GLADIUS_JOBQ_MAX_AGE_MS;

    stale = (qage > BUZZ_JOB_TIMEOUT_MS);
    promote = force || !active || !sameActive || minimumSliceDone || oldEnough;

    if (stale) {
      queuedBuzzJobValid = false;
      buzzJobQStaleDrop++;
    } else if (promote) {
      q = queuedBuzzJob;
      memcpy(qmac, queuedBuzzJobMac, 6);
      qfp = queuedBuzzJobFp32;
      queuedBuzzJobValid = false;
      buzzJobQPromoted++;
      if (active && sameActive) buzzJobQYielded++;
    }
  }
  portEXIT_CRITICAL(&jobMux);

  if (stale) {
    logf("BUZZ/JOBQ", "stale_drop reason=%s qAge=%lums stale=%lu", reason ? reason : "?", (unsigned long)qage, (unsigned long)buzzJobQStaleDrop);
    return false;
  }
  if (!promote) return false;

  logf("BUZZ/JOBQ", "promote reason=%s fp=%08lX start=%08lX promoted=%lu yielded=%lu qAge=%lums",
       reason ? reason : "ready",
       (unsigned long)qfp,
       (unsigned long)q.start_nonce,
       (unsigned long)buzzJobQPromoted,
       (unsigned long)buzzJobQYielded,
       (unsigned long)qage);
  applyBuzzJobImmediate(q, qmac);
  return true;
}

static void applyBuzzJob(const struct BuzzJobPacket& bj, const uint8_t* fromMac) {
  if (bj.magic[0] != 'J' || bj.magic[1] != 'B') return;

  if (bj.range_size == 0) {
    applyBuzzJobImmediate(bj, fromMac);
    return;
  }

  uint32_t fp = jobId32From8(bj.job_id);
  bool active = false;
  bool sameFp = false;
  bool duplicateSlice = false;
  uint32_t activeAge = 0;
  uint32_t checked = 0;

  portENTER_CRITICAL(&jobMux);
  active = minerHasJob && currentJobIsBuzz;
  if (active) {
    sameFp = (currentJob.jobId == fp);
    duplicateSlice = sameFp && currentJob.startNonce == bj.start_nonce && currentJob.range == (bj.range_size ? bj.range_size : 262144UL);
    activeAge = janusSafeAgeMs(nowMs(), minerJobStartedMs, 0);
    checked = workerCursor;
  }
  portEXIT_CRITICAL(&jobMux);

  if (duplicateSlice) {
    buzzJobQDupDrop++;
    if ((buzzJobQDupDrop % GLADIUS_JOBQ_LOG_EVERY) == 1UL) {
      logf("BUZZ/JOBQ", "dup_drop fp=%08lX start=%08lX dup=%lu checked=%lu age=%lums",
           (unsigned long)fp,
           (unsigned long)bj.start_nonce,
           (unsigned long)buzzJobQDupDrop,
           (unsigned long)checked,
           (unsigned long)activeAge);
    }
    return;
  }

  if (active && sameFp) {
    bool keepChewing = (activeAge < GLADIUS_JOBQ_MIN_HOLD_MS) && (checked < GLADIUS_JOBQ_MIN_HASHES);
    if (keepChewing) {
      gladiusQueueBuzzJob(bj, fromMac, fp, "same_fp_hold_current");
      return;
    }
    buzzJobQYielded++;
    if ((buzzJobQYielded % GLADIUS_JOBQ_YIELD_LOG_EVERY) == 1UL ||
        checked >= 20000UL ||
        bestZ + 1 >= currentJob.targetBits) {
      logf("BUZZ/JOBQ", "yield sameFp=%08lX oldChecked=%lu oldAge=%lums nextStart=%08lX yielded=%lu quiet=1",
           (unsigned long)fp,
           (unsigned long)checked,
           (unsigned long)activeAge,
           (unsigned long)bj.start_nonce,
           (unsigned long)buzzJobQYielded);
    }
  }

  applyBuzzJobImmediate(bj, fromMac);
}

static bool agentTargetsGladius(const JanusAgentRewardPacket& ar) {
  if (!ar.targetNode[0]) return true;
  if (!strncmp(ar.targetNode, "Gladius", sizeof(ar.targetNode))) return true;
  if (!strncmp(ar.targetNode, GLADIUS_NODE_NAME, sizeof(ar.targetNode))) return true;
  return false;
}

static void applyAgentReward(const JanusAgentRewardPacket& ar) {
  if (!agentTargetsGladius(ar)) return;
  buzzAgentRewards++;
  lastBuzzMasterMs = nowMs();
  gladiusLastBuzzAgentRewardMs = nowMs();
  buzzAgentHint = ar.aiHint ? ar.aiHint : 1;
  buzzAgentLevel = ar.rewardLevel;
  buzzAgentBatch = ar.targetBatch;
  buzzEntropySeed ^= ar.entropySeed ? ar.entropySeed : (ar.seq ^ nowMs());

  if (buzzAgentHint == 2) swarmBatchScalePct = 75;
  else if (buzzAgentHint >= 3) swarmBatchScalePct = 130;
  else swarmBatchScalePct = 100;

  if (ar.entropySeed) {
    swarmPreferredLane = (uint8_t)(ar.entropySeed % (LANE_RANDOM + 1));
    swarmPreferredStride = ZIM_STRIDE_ARMS[(ar.entropySeed >> 8) % ZIM_STRIDE_ARM_COUNT] | 1UL;
  }

  logf("AGENT", "reward src=%s lvl=%u hint=%u pts=%u batch=%u score=%.2f pred=%.1f err=%.3f",
       ar.source,
       (unsigned)ar.rewardLevel,
       (unsigned)ar.aiHint,
       (unsigned)ar.rewardPoints,
       (unsigned)ar.targetBatch,
       ar.score,
       ar.predictedHashRate,
       ar.predictionError);

  if (ar.deltaShares > 0 || ar.rewardLevel >= 2 || ar.rewardPoints >= 10) {
    uint8_t z = bestZ >= 16 ? bestZ : (uint8_t)18;
    uint8_t strength = (uint8_t)constrain((int)ar.rewardLevel + (int)(ar.rewardPoints / 8) + (ar.deltaShares ? 4 : 0), 2, 14);
    tailGexRewardLane(activeLane, z, "oxy_reward", strength);
  }
}

static void gladiusTorricelliBondTick(uint32_t now) {
  if (torricelliBondLastMs && janusSafeAgeMs(now, torricelliBondLastMs, 0UL) < 850UL) return;
  float dt = torricelliBondLastMs ? min(4.0f, (float)janusSafeAgeMs(now, torricelliBondLastMs, 0UL) / 1000.0f) : 1.0f;
  torricelliBondLastMs = now;

  bool twinFresh = janusTwinPeerFresh();
  bool sameJob = janusTwinSameJobNow();
  uint32_t txTotal = espTxOk + espTxFail + 8UL;
  float radioClean = constrain(1.0f - ((float)espTxFail / (float)txTotal), 0.0f, 1.0f);
  uint16_t target = currentJob.targetBits ? currentJob.targetBits : 22;
  uint8_t best = gladiusReportBestBits();
  float progress = constrain((float)best / (float)max<uint16_t>(1, target), 0.0f, 1.8f);
  bool anchorAhead = twinFresh && (janusTwinPeerBestBits > best || janusTwinPeerHashRate > lastHps + 800UL);
  bool freshShare = lastShareMs && janusSafeAgeMs(now, lastShareMs, 999999UL) < 30000UL;

  float pressure = 0.0f;
  pressure += twinFresh ? 0.34f : -0.24f;
  pressure += sameJob ? 0.30f : 0.0f;
  pressure += anchorAhead ? 0.36f : 0.10f;
  pressure += freshShare ? 0.46f : 0.0f;
  pressure += radioClean * 0.14f + progress * 0.22f;
  pressure += (float)tailGexTopConfidenceX100 * 0.0025f;
  pressure -= (espTxFail > espTxOk + 24UL) ? 0.34f : 0.0f;

  oxytocin = constrain(oxytocin + pressure * dt, 0.0f, 100.0f);

  float vacuumTarget = constrain(0.18f + radioClean * 0.30f + (twinFresh ? 0.20f : 0.0f) +
                                 (sameJob ? 0.18f : 0.0f) + progress * 0.11f +
                                 (float)tailGexTopWeightPct * 0.0020f - rfEntropy * 0.020f, 0.0f, 1.0f);
  torricelliVacuum = constrain(torricelliVacuum * 0.91f + vacuumTarget * 0.09f, 0.0f, 1.0f);

  if (sameJob && oxytocin > 72.0f && minerHasJob && tailGexTopLane <= LANE_RANDOM) {
    static uint32_t lastOxyLaneRewardMs = 0;
    if (!lastOxyLaneRewardMs || janusSafeAgeMs(now, lastOxyLaneRewardMs, 0UL) > 12000UL) {
      lastOxyLaneRewardMs = now;
      uint8_t z = best >= 17 ? best : (uint8_t)17;
      uint8_t strength = (uint8_t)constrain((int)((oxytocin - 60.0f) / 5.0f) + (anchorAhead ? 3 : 1), 2, 10);
      tailGexRewardLane(activeLane <= LANE_RANDOM ? activeLane : tailGexTopLane, z, "torricelli_oxy", strength);
      torricelliOxyBoosts++;
    }
  }
}

static void applyJob(const struct JanusJobLite& j, const uint8_t* fromMac) {
  JanusJobLite nj = j;

  if (nj.targetBits == 0 || nj.targetBits > 64) nj.targetBits = 22;
  if (nj.range == 0 || nj.range > 4000000UL) nj.range = 262144UL;
  if (nj.lane > LANE_RANDOM) nj.lane = LANE_ZIM_REVERSE;
  if (nj.arm >= ZIM_STRIDE_ARM_COUNT) nj.arm = nj.arm % ZIM_STRIDE_ARM_COUNT;
  if (nj.stride == 0) nj.stride = ZIM_STRIDE_ARMS[nj.arm] | 1UL;

  portENTER_CRITICAL(&jobMux);
  currentJob = nj;
  minerHasJob = true;
  currentJobIsSelf = false;
  currentJobIsBuzz = false;
  realJobsSeen++;
  jobsSeen++;
  workerCursor = 0;
  bestZ = 0;
  bestNonce = 0;
  gladiusReportBestReset(nj.jobId);
  minerJobStartedMs = nowMs();
  lastRealJobMs = minerJobStartedMs;
  activeLane = nj.lane;
  activeArm = nj.arm;
  activeStride = nj.stride;
  portEXIT_CRITICAL(&jobMux);

  ledJobPulseUntilMs = nowMs() + 6500UL;

  logf("JOB", "from=%s job=%08lX start=%08lX range=%lu target=%u lane=%s stride=%lu arm=%u",
       fromMac ? macToString(fromMac).c_str() : "unknown",
       (unsigned long)nj.jobId,
       (unsigned long)nj.startNonce,
       (unsigned long)nj.range,
       nj.targetBits,
       laneName(nj.lane),
       (unsigned long)nj.stride,
       nj.arm);

  char msg[48];
  snprintf(msg, sizeof(msg), "GLAD JOB %08lX", (unsigned long)nj.jobId);
  fieldSet(FIELD_EVENT, msg, EVENT_FIELD_LIFETIME_MS, 3000, 650);
  tailGexSave("job", false);
  tailGexSendMemoryIfDue(true, 0x0008);
  requestOracle("new job from swarm", false);
}

static void updateMinerField(bool force) {
  uint32_t now = nowMs();

  if (!force && now - minerLastFieldMs < MINER_FIELD_INTERVAL_MS) return;
  minerLastFieldMs = now;

  String msg;

  if (minerHasJob) {
    msg = currentJobIsBuzz ? "GLAD BUZZ " : (currentJobIsSelf ? "GLAD SELF " : "GLAD ");
    msg += String(lastHps);
    msg += "H BEST";
    msg += String(bestZ);
    msg += " T";
    msg += String(currentJob.targetBits);
  } else {
    msg = "GLAD WAIT BUZZ JOB";
  }

  fieldSet(FIELD_MINER, msg, 0, 3600, 1150);
  updateMachineField(false);
}

static uint8_t adaptiveBatchSize() {
  if (oracleInFlight) return 4;

  uint8_t b = currentJobIsBuzz ? 32 : 24;

  if (!wifiOnline) b = 10;
  else if (ESP.getFreeHeap() < 70000) b = 10;
  else if (lastRssi < -78) b = 14;

  // v1.18: lastHps is real hashes/sec, not a tiny 0..100 load number.
  // Old >55 rule pinned Gladius to batch=16 forever. Keep it calmer only when
  // it is already very fast, otherwise let Buzz-agent batch breathe.
  if (lastHps < 2000) b = currentJobIsBuzz ? 40 : 32;
  if (lastHps > 18000) b = currentJobIsBuzz ? 24 : 16;
  if (minerHasJob && bestZ + 2 >= currentJob.targetBits) b = currentJobIsBuzz ? 48 : 36;
  uint8_t localHint = gladiusSchedulerHint();
  if (currentJobIsBuzz && localHint == 2 && b > 18) b = 18;
  if (currentJobIsBuzz && localHint >= 3 && b < 36) b = 36;

  uint16_t scaled = ((uint16_t)b * (uint16_t)swarmBatchScalePct) / 100;
  if (currentJobIsBuzz && buzzAgentBatch > 0) {
    // Buzz targetBatch is sized for larger workers. Map it softly into the ESP32-safe batch window.
    uint16_t mapped = map((long)constrain((int)buzzAgentBatch, 80, 1800), 80, 1800, 8, BUZZ_BRIDGE_BATCH_MAX);
    scaled = (scaled + mapped) / 2;
  }

  // TailGEX не гонит GPU-style brute force. Он мягко даёт воздух хвостовому лидеру
  // и режет обороты при отрицательной хвостовой экспозиции.
  if (tailGexTopX100 > 80 && tailGexTopConfidenceX100 > 30) {
    scaled = (uint16_t)min(64, (int)scaled + 6);
  } else if (tailGexTopX100 < -40 && tailGexTopConfidenceX100 > 35) {
    scaled = (uint16_t)max(4, (int)scaled - 8);
  }

  if (gladiusTranceptionHint >= 3 && gladiusTranceptionLiteScore > 0.82f && ESP.getFreeHeap() > 85000) {
    scaled = (uint16_t)min((int)(currentJobIsBuzz ? BUZZ_BRIDGE_BATCH_MAX : 48), (int)scaled + 6);
  } else if (gladiusTranceptionHint == 2 && gladiusTranceptionLiteScore < 0.50f) {
    scaled = (uint16_t)max(4, (int)scaled - 4);
  }

  if (oxytocin > 58.0f && ESP.getFreeHeap() > 85000) {
    int oxyBoost = (int)((oxytocin - 58.0f) * 0.55f);
    if (janusTwinPeerFresh()) oxyBoost += 5;
    if (janusTwinSameJobNow() && janusTwinPeerBestBits >= gladiusReportBestBits()) oxyBoost += 7;
    scaled = (uint16_t)min((int)(currentJobIsBuzz ? BUZZ_BRIDGE_BATCH_MAX : 48), (int)scaled + constrain(oxyBoost, 0, 18));
  } else if (oxytocin < 24.0f || espTxFail > espTxOk + 30UL) {
    scaled = (uint16_t)max(4, (int)scaled - 8);
  }

  // v1.21: after BuzzAgent confirms a useful share, allow a short controlled sprint.
  if (currentJobIsBuzz &&
      localHint >= 3 &&
      janusSafeAgeMs(nowMs(), gladiusLastBuzzAgentRewardMs, 999999UL) < GLADIUS_REWARD_SPRINT_MS &&
      ESP.getFreeHeap() > 85000) {
    scaled = (uint16_t)min((int)BUZZ_BRIDGE_BATCH_MAX, (int)scaled + 12);
  }

  b = (uint8_t)constrain((int)scaled, 4, currentJobIsBuzz ? (int)BUZZ_BRIDGE_BATCH_MAX : 48);

  return b;
}

static void minerTask(void* arg) {
  (void)arg;

  uint32_t hpsWindowStart = nowMs();
  uint32_t hpsWindowHashes = 0;

  uint8_t buf[80];
  uint8_t hash[32];
  size_t len = 0;

  while (true) {
    JanusJobLite j;
    bool hasJob = false;
    bool buzzJob = false;
    bool selfJob = false;
    uint8_t buzzHeader[80];
    uint8_t buzzTarget[32];
    uint8_t buzzJobId[8];

    gladiusPromoteQueuedBuzzJobIfReady("miner_tick");

    portENTER_CRITICAL(&jobMux);
    hasJob = minerHasJob;
    if (hasJob) {
      j = currentJob;
      buzzJob = currentJobIsBuzz;
      selfJob = currentJobIsSelf;
      if (buzzJob) {
        memcpy(buzzHeader, currentBuzzHeader, 80);
        memcpy(buzzTarget, currentBuzzTarget, 32);
        memcpy(buzzJobId, currentBuzzJobId, 8);
      }
    }
    portEXIT_CRITICAL(&jobMux);

    if (hasJob && buzzJob && j.expiresMs && minerJobStartedMs && janusSafeAgeMs(nowMs(), minerJobStartedMs, 0) > j.expiresMs) {
      portENTER_CRITICAL(&jobMux);
      if (currentJob.jobId == j.jobId && currentJobIsBuzz) {
        minerHasJob = false;
        currentJobIsSelf = false;
        currentJobIsBuzz = false;
      }
      portEXIT_CRITICAL(&jobMux);
      buzzWeakTickets++;
      logf("BUZZ/JB", "expired jid32=%08lX checked=%lu/%lu best=%u", (unsigned long)j.jobId, (unsigned long)workerCursor, (unsigned long)j.range, bestZ);
      continue;
    }

    // Swarm-language adaptation: targetBits не трогаем, меняем только локальный обход nonce.
    if (hasJob && swarmNeighborCount > 0) {
      j.lane = swarmPreferredLane;
      j.stride = swarmPreferredStride;
    }

    // TailGEX scheduler-only allocator: мягко выбирает lane по хвостовой чувствительности.
    if (hasJob) {
      uint8_t chosenLane = tailGexChooseLane(j.lane);
      if (chosenLane != j.lane) {
        j.lane = chosenLane;
        j.stride = tailGexDefaultStride(chosenLane);
      }
      activeLane = j.lane;
      activeStride = j.stride;
    }

    if (!hasJob) {
      updateMinerField(false);
      vTaskDelay(pdMS_TO_TICKS(200));
      continue;
    }

    gladiusTorricelliBondTick(nowMs());
    uint8_t batch = adaptiveBatchSize();
    minerLoadPct = constrain((int)batch * 3, 5, 100);

    for (uint8_t n = 0; n < batch; n++) {
      uint32_t i = workerCursor++;

      if (j.range && i >= j.range) {
        portENTER_CRITICAL(&jobMux);
        if (currentJob.jobId == j.jobId) {
          minerHasJob = false;
          currentJobIsSelf = false;
          currentJobIsBuzz = false;
        }
        portEXIT_CRITICAL(&jobMux);

        logf("MINER", "job done job=%08lX checked=%lu best=%u nonce=%08lX",
             (unsigned long)j.jobId,
             (unsigned long)j.range,
             bestZ,
             (unsigned long)bestNonce);

        fieldSet(FIELD_EVENT, "GLAD JOB DONE WAIT BUZZ", EVENT_FIELD_LIFETIME_MS, 3000, 700);
        tailGexRecalcAll(true);
        tailGexSave("job_done", true);
        tailGexSendMemoryIfDue(true, 0x0002);
        break;
      }

      uint32_t nonce = nextNonceForLane(j, i);
      uint8_t z = 0;
      bool targetOk = false;

      if (buzzJob) {
        uint8_t realHeader[80];
        uint8_t rawHash[32];
        uint8_t shareHash[32];
        memcpy(realHeader, buzzHeader, 80);
        writeLE32(realHeader + 76, nonce);
        sha256d(realHeader, 80, rawHash);
        hashToShareOrder(rawHash, shareHash);
        z = (uint8_t)constrain((int)countLeadingZeroBitsBE(shareHash), 0, 255);
        targetOk = (z >= j.targetBits) && hashMeetsTargetBE(shareHash, buzzTarget);

        tailGexObserveHash(j.lane, z);

        totalHashes++;
        roundHashes++;
        hpsWindowHashes++;
        minerLastHashMs = nowMs();

        if (z > bestZ) {
          bestZ = z;
          bestNonce = nonce;
          gladiusReportBestObserve(j.jobId, z, nonce);
          uint8_t strength = tailGexRewardStrengthFor(z, j.targetBits);
          if (strength > 0) tailGexRewardLane(j.lane, z, "best_up", strength);
          if (bestZ + 2 >= j.targetBits) updateMinerField(true);
        }

        if (z >= j.targetBits && !targetOk) {
          // DualLock: zbits is only a rarity signal; exact target compare is the wire truth.
          buzzWeakTickets++;
        }

        if (targetOk) {
          tailGexRewardLane(j.lane, z, "buzz_share", 18);
          sendBuzzSharePacket(j, buzzJobId, nonce, z, shareHash);
          portENTER_CRITICAL(&jobMux);
          if (currentJob.jobId == j.jobId && currentJobIsBuzz) {
            minerHasJob = false;
            currentJobIsSelf = false;
            currentJobIsBuzz = false;
          }
          portEXIT_CRITICAL(&jobMux);
          break;
        }
      } else {
        buildHashInput(j, nonce, buf, len);
        sha256d(buf, len, hash);
        z = leadingZeroBits(hash);

        tailGexObserveHash(j.lane, z);

        totalHashes++;
        roundHashes++;
        hpsWindowHashes++;
        minerLastHashMs = nowMs();

        if (z > bestZ) {
          bestZ = z;
          bestNonce = nonce;
          gladiusReportBestObserve(j.jobId, z, nonce);
          uint8_t strength = tailGexRewardStrengthFor(z, j.targetBits);
          if (strength > 0) tailGexRewardLane(j.lane, z, selfJob ? "self_best" : "best_up", strength);
          if (bestZ + 2 >= j.targetBits) updateMinerField(true);
        }

        if (z >= j.targetBits) {
          tailGexRewardLane(j.lane, z, selfJob ? "self_proof" : "share", 16);
          sendSharePacket(j, nonce, z, hash);
        }
      }
    }

    uint32_t now = nowMs();

    if (now - hpsWindowStart >= 1000) {
      lastHps = (uint32_t)((hpsWindowHashes * 1000UL) / max(1UL, now - hpsWindowStart));
      if (hpsEma <= 0.1f) hpsEma = lastHps;
      else hpsEma = hpsEma * 0.82f + (float)lastHps * 0.18f;

      hpsWindowStart = now;
      hpsWindowHashes = 0;
    }

    updateMinerField(false);

    uint32_t reportMs = (buzzJob && tailGexTopWeightPct >= 25) ? GLADIUS_MINER_REPORT_MS_ACTIVE : GLADIUS_MINER_REPORT_MS_IDLE;
    if (now - minerLastReportMs >= reportMs) {
      minerLastReportMs = now;
      logf("MINER", "H=%lu ema=%lu job=%08lX checked=%lu/%lu best=%u nonce=%08lX shares=%lu lane=%s stride=%lu batch=%u",
           (unsigned long)lastHps,
           (unsigned long)hpsEma,
           (unsigned long)j.jobId,
           (unsigned long)workerCursor,
           (unsigned long)j.range,
           bestZ,
           (unsigned long)bestNonce,
           (unsigned long)sharesFound,
           laneName(j.lane),
           (unsigned long)j.stride,
           batch);
      logf("TAILGEX", "top=%s x100=%d conf=%u%% weight=%u%% explores=%lu leaderUses=%lu rewards=%lu self=%lu near=%lu splitR=%lu peerHint=%lu oxyR=%lu torOxy=%lu unlock=%lu boost=%.2f oxy=%.1f vac=%.2f",
           laneName(tailGexTopLane),
           (int)tailGexTopX100,
           tailGexTopConfidenceX100,
           tailGexTopWeightPct,
           (unsigned long)tailGexProtectedExplores,
           (unsigned long)tailGexLeaderUses,
           (unsigned long)tailGexRewardEvents,
           (unsigned long)tailGexSelfTailEvents,
           (unsigned long)tailGexNearMissEvents,
           (unsigned long)tailGexSplitRewardEvents,
           (unsigned long)tailGexPeerHintEvents,
           (unsigned long)tailGexOxyRewardEvents,
           (unsigned long)torricelliOxyBoosts,
           (unsigned long)tailGexWeightUnlockEvents,
           (tailGexTopLane <= LANE_RANDOM ? tailGexRewardBoost[tailGexTopLane] : 0.0f),
           oxytocin, torricelliVacuum);
    }

    vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// ============================================================
// ESP-NOW parser
// ============================================================


static bool looksLikeKnownForeignJanusPacket(const uint8_t* data, int len) {
  if (!data || len < 2) return false;
  uint16_t m = ((uint16_t)data[0] << 8) | data[1];
  switch (m) {
    case 0x5353: // S/S SwarmSense
    case 0x4532: // E2 entropy
    case 0x5253: // R/S RF dome sonar
    case 0x5250: // R/P RF dome ping
    case 0x4A45: // J/E event
    case 0x4A41: // J/A or adjacent Janus agent packet
    case 0x5450: // T/P prophecy
    case 0x4B32: // K2 prophecy answer
    case 0x484D: // H/M memory/map
    case 0x504C: // P/L pilot/order
    case 0x4552: // E/R entropy reward/report
    case 0x4253: // B/S Buzz status
      return true;
    default:
      return false;
  }
}

static bool gladiusFieldContains(const char* field, size_t len, const char* needle) {
  if (!field || !needle) return false;
  char tmp[32];
  size_t n = min(len, sizeof(tmp) - 1);
  memcpy(tmp, field, n);
  tmp[n] = '\0';
  return strstr(tmp, needle) != nullptr;
}

static bool gladiusConsumeBuzzColony(const uint8_t* data, int len, const uint8_t* fromMac) {
  if (!data || len != (int)sizeof(JanusColonyPacket)) return false;
  JanusColonyPacket pkt;
  memcpy(&pkt, data, sizeof(pkt));
  if (memcmp(pkt.magic, "JANUS", 5) != 0) return false;

  bool isBuzz = gladiusFieldContains(pkt.nodeId, sizeof(pkt.nodeId), "Buzz") ||
                gladiusFieldContains(pkt.nodeId, sizeof(pkt.nodeId), "BUZZ") ||
                gladiusFieldContains(pkt.role, sizeof(pkt.role), "MASTER") ||
                gladiusFieldContains(pkt.role, sizeof(pkt.role), "Buzz") ||
                gladiusFieldContains(pkt.role, sizeof(pkt.role), "BUZZ");
  if (!isBuzz) return false;
  lastBuzzMasterMs = nowMs();
  rememberBuzzMasterMac(fromMac, "buzz-heartbeat");
  return true;
}

static bool looksLikeBuzzJob(const uint8_t* data, int len) {
  return data && len == (int)sizeof(BuzzJobPacket) && data[0] == 'J' && data[1] == 'B';
}

static bool looksLikeJob(const uint8_t* data, int len) {
  if (len < (int)sizeof(JanusJobLite)) return false;
  if (data[0] == 'J' && (data[1] == 'G' || data[1] == '2')) return true;
  return false;
}

static void nowParserTask(void* arg) {
  (void)arg;

  NowRxItem item;

  while (true) {
    if (xQueueReceive(nowQueue, &item, pdMS_TO_TICKS(200))) {
      rfUpdateFromPacket(item.rssi);

      if (janusFaceReceive(item.data, item.len, item.rssi)) {
        // Janus twin face sync packet consumed.
      } else if (janusTwinTaskReceive(item.data, item.len, item.rssi)) {
        // Janus twin task/race packet consumed.
      } else if (gladiusConsumeBuzzColony(item.data, item.len, item.mac)) {
        // Buzz heartbeat/master packet consumed for radio freshness.
      } else if (looksLikeBuzzJob(item.data, item.len)) {
        BuzzJobPacket bj;
        memcpy(&bj, item.data, sizeof(BuzzJobPacket));
        applyBuzzJob(bj, item.mac);
      } else if (item.len == (int)sizeof(JanusAgentRewardPacket) && item.data[0] == 'A' && item.data[1] == 'R') {
        JanusAgentRewardPacket ar;
        memcpy(&ar, item.data, sizeof(ar));
        applyAgentReward(ar);
      } else if (looksLikeJob(item.data, item.len)) {
        JanusJobLite j;
        memcpy(&j, item.data, sizeof(JanusJobLite));

        uint32_t oldCrc = j.crc;
        j.crc = 0;
        uint32_t calc = crc32ish(&j, sizeof(j) - 4);

        if (oldCrc == 0 || oldCrc == calc) {
          j.crc = oldCrc;
          applyJob(j, item.mac);
        } else {
          espUnknown++;
          logf("RX", "job crc mismatch from=%s old=%08lX calc=%08lX len=%u",
               macToString(item.mac).c_str(),
               (unsigned long)oldCrc,
               (unsigned long)calc,
               item.len);
        }
      } else {
        espUnknown++;
        bool knownForeign = looksLikeKnownForeignJanusPacket(item.data, item.len);
        uint32_t logEvery = knownForeign ? GLADIUS_RX_FOREIGN_LOG_EVERY : GLADIUS_RX_UNKNOWN_LOG_EVERY;
        if ((espUnknown % logEvery) == 1 && item.len >= 2) {
          logf("RX", "%s magic=%02X%02X len=%u rssi=%d from=%s",
               knownForeign ? "foreign" : "unknown",
               item.data[0],
               item.data[1],
               item.len,
               item.rssi,
               macToString(item.mac).c_str());
        }

        if (espRx == 1 || espRx % 240 == 0) {
          uint32_t age = janusSafeAgeMs(nowMs(), gladiusLastSwarmSignalFieldMs, 999999UL);
          if (age >= GLADIUS_SWARM_SIGNAL_FIELD_MIN_MS) {
            gladiusLastSwarmSignalFieldMs = nowMs();
            fieldSet(FIELD_EVENT, "SWARM SIGNAL HEARD", EVENT_FIELD_LIFETIME_MS, 3000, 800);
          }
          requestOracle("swarm signal", false);
        }
      }
    }
  }
}

// ============================================================
// Status task
// ============================================================

static void statusTask(void* arg) {
  (void)arg;

  while (true) {
    uint32_t statusNow = nowMs();
    if (gladiusStatusTaskLastMs) {
      int32_t driftMs = (int32_t)(statusNow - gladiusStatusTaskLastMs) - 2500;
      if (driftMs < 0) driftMs = -driftMs;
      gladiusLoopJitterUs = (uint16_t)min(65535UL, (uint32_t)driftMs * 1000UL);
    }
    gladiusStatusTaskLastMs = statusNow;

    ensureSelfJobFallback();
    gladiusRadioWatchdog();
    if (statusNow - gladiusLastPresenceRefreshMs >= GLADIUS_PRESENCE_REFRESH_MS) {
      gladiusLastPresenceRefreshMs = statusNow;
      gladiusPresenceBurst("ttl-refresh");
    }
    gladiusTorricelliBondTick(statusNow);
    gladiusTranceptionLiteTick(statusNow);
    sendStatusPacket();
    sendColonyHeartbeatPacket(false);
    sendSwarmSensePacket(false);
    sendGladiusPnCortex(false);
    if (nowMs() - tailGexLastRecalcMs >= TAIL_GEX_RECALC_INTERVAL_MS) {
      tailGexLastRecalcMs = nowMs();
      tailGexRecalcAll(false);
    }
    tailGexSave("periodic", false);
    tailGexSendMemoryIfDue(false, 0x0001);
    updateMachineField(false);

    if (textMode == TEXTCAST_AUTO) {
      if (!fields[FIELD_ORACLE].enabled) {
        fieldSet(FIELD_ORACLE, localOracleLine(), 0, 3800, 1200);
      }

      if (!gladiusBuzzJobActiveNow() &&
          (lastOracleRequestMs == 0 || nowMs() - lastOracleRequestMs > ORACLE_MIN_INTERVAL_MS)) {
        requestOracle("periodic", false);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(2500));
  }
}

// ============================================================
// Serial commands
// ============================================================

static String runCommand(String cmd) {
  cmd.trim();
  lastCommand = cmd;

  if (cmd.length() == 0) return "empty command";

  String lower = cmd;
  lower.toLowerCase();

  if (lower == "help" || lower == "?") {
    return "commands: status, gex, gex save|send|reset, self job, say <text>, oracle, mode auto|manual|quiet, job demo, target <bits>, led +|-|off|extra|0..96, u/logs=toggle UART0 logs+smallLED, textcast, reboot";
  }

  if (lower == "status" || lower == "stats") {
    String s;
    s += "wifi="; s += wifiOnline ? "on" : "off";
    s += " ch="; s += String(wifiChannel);
    s += " H="; s += String(lastHps);
    s += " best="; s += String(bestZ);
    s += " shares="; s += String(sharesFound);
    s += " selfProofs="; s += String(selfProofsFound);
    s += " jobs="; s += String(jobsSeen);
    s += " realJobs="; s += String(realJobsSeen);
    s += " buzzJobs="; s += String(buzzJobsSeen);
    s += " jobq="; s += String(buzzJobQAccepted); s += "/"; s += String(buzzJobQQueued); s += "/"; s += String(buzzJobQPromoted); s += "/"; s += String(buzzJobQYielded);
    s += " buzzS2="; s += String(buzzSharesSent);
    s += " weak="; s += String(buzzWeakTickets);
    s += " selfJobs="; s += String(selfJobsMade);
    s += " rx="; s += String(espRx);
    s += " oracle="; s += String(oracleOk); s += "/"; s += String(oracleFail);
    s += " nb="; s += String(swarmNeighborCount);
    s += " rz="; s += String(swarmBestRemoteZ);
    s += " gexPeers="; s += String(swarmTextcastGexPeers);
    s += " gexLane="; s += laneName(swarmBestGexLane);
    s += " gexT="; s += String(swarmBestGexX100);
    s += " split="; s += String(swarmLaneSplitCount);
    s += " sniff="; s += String(sniffJanus); s += "/"; s += String(sniffRx);
    s += " batchScale="; s += String(swarmBatchScalePct);
    s += " ";
    s += tailGexSummaryLine();
    return s;
  }

  if (lower == "gex") {
    return tailGexSummaryLine();
  }

  if (lower == "gex save") {
    tailGexRecalcAll(true);
    tailGexSave("serial", true);
    return "tailgex saved";
  }

  if (lower == "gex send") {
    tailGexRecalcAll(true);
    tailGexSendMemoryIfDue(true, 0x0002);
    return "tailgex memory sent";
  }

  if (lower == "gex reset") {
    tailGexReset("serial");
    return "tailgex reset";
  }

  if (lower == "self job" || lower == "selfjob") {
    makeSelfJob("serial");
    return "self job started";
  }

  if (lower.startsWith("say ")) {
    String text = sanitizeSSIDAscii(cmd.substring(4), 28);
    textMode = TEXTCAST_MANUAL;
    prefs.putUChar("mode", (uint8_t)textMode);
    fieldSet(FIELD_ORACLE, String("JANUS ") + text, 0, 3600, 1000);
    broadcastCommand(cmd);
    return "manual oracle field set";
  }

  if (lower == "textcast" || lower == "jg2") {
    String s;
    s += "machine="; s += buildMachineBeacon();
    s += " peers="; s += String(swarmTextcastGexPeers);
    s += " bestNode="; s += String(swarmBestGexNode, HEX);
    s += " bestLane="; s += laneName(swarmBestGexLane);
    s += " tail="; s += String(swarmBestGexX100);
    s += " conf="; s += String(swarmBestGexConfidence);
    s += " splits="; s += String(swarmLaneSplitCount);
    return s;
  }

  if (lower == "oracle") {
    requestOracle("serial", true);
    return "oracle requested";
  }

  if (lower.startsWith("mode ")) {
    String m = lower.substring(5);
    m.trim();

    if (m == "auto") textMode = TEXTCAST_AUTO;
    else if (m == "manual") textMode = TEXTCAST_MANUAL;
    else if (m == "quiet" || m == "off") textMode = TEXTCAST_QUIET;
    else return "bad mode";

    prefs.putUChar("mode", (uint8_t)textMode);
    return "mode set";
  }

  if (lower == "job demo") {
    makeDemoJob();
    return "demo job started";
  }

  if (lower.startsWith("target ")) {
    int z = lower.substring(7).toInt();
    z = constrain(z, 8, 64);

    portENTER_CRITICAL(&jobMux);
    currentJob.targetBits = (uint8_t)z;
    portEXIT_CRITICAL(&jobMux);

    return "targetBits=" + String(z);
  }

  if (lower == "led +" || lower == "led plus") {
    ledPlus();
    return "led +";
  }

  if (lower == "led -" || lower == "led minus") {
    ledMinus();
    return "led -";
  }

  if (lower == "led off") {
    ledOffNow();
    return "led off";
  }

  if (lower == "led extra" || lower == "led aux") {
    toggleExtraLed("serial_toggle");
    return String("extra led=") + (extraLedEnabled ? "on" : "off");
  }

  if (lower == "led extra on" || lower == "led aux on") {
    if (!extraLedEnabled) toggleExtraLed("serial_on");
    return "extra led on";
  }

  if (lower == "led extra off" || lower == "led aux off") {
    if (extraLedEnabled) toggleExtraLed("serial_off");
    return "extra led off";
  }

  if (lower == "u" || lower == "uart" || lower == "uart logs" || lower == "logs") {
    toggleUart0FullLog("serial_u_toggle");
    return String("uart0 full logs=") + (janusUart0FullLog ? "on" : "off") + " smallLed=" + (extraLedEnabled ? "on" : "off");
  }

  if (lower.startsWith("led ")) {
    int b = lower.substring(4).toInt();
    b = constrain(b, 0, 120);
    setLed((uint8_t)b);
    return "led=" + String(b);
  }

  if (lower == "clear") {
    portENTER_CRITICAL(&logMux);
    logBuf = "";
    portEXIT_CRITICAL(&logMux);
    return "log cleared";
  }

  if (lower == "reboot") {
    delay(100);
    ESP.restart();
    return "rebooting";
  }

  broadcastCommand(cmd);
  return "broadcasted to swarm";
}

// ============================================================
// Low-rate service task
// ============================================================

static void serviceTask(void* arg) {
  (void)arg;

  while (true) {
    textcastTick();
    janusFaceTick(nowMs());
    janusTwinTaskTick(nowMs());
    ledTick();
    bootButtonTick();
    vTaskDelay(pdMS_TO_TICKS(15));
  }
}

// ============================================================
// Setup / loop
// ============================================================

void setup() {
  Serial.begin(115200);
  delay(700);

  esp_log_level_set("*", ESP_LOG_NONE);

  logBuf.reserve(18000);

  uint64_t chipMac64 = ESP.getEfuseMac();
  nodeId = (uint16_t)(((chipMac64 >> 32) ^ (chipMac64 >> 16) ^ chipMac64) & 0xFFFF);
  if (nodeId == 0) nodeId = (uint16_t)(esp_random() & 0xFFFF);

  prefs.begin("gladius", false);
  ledBrightness = prefs.getUChar("led", GLADIUS_LED_BRIGHTNESS_DEFAULT);
  ledOff = prefs.getBool("ledOff", ledBrightness == 0);
  if (ledOff) ledBrightness = 0;
  clampLedBrightness();
  buttonBrightnessDirUp = (ledBrightness < GLADIUS_BRIGHTNESS_MAX);
  janusUart0FullLog = prefs.getBool("uart_full", false);
  extraLedEnabled = prefs.getBool("extraLed", janusUart0FullLog);
  if (janusUart0FullLog) extraLedEnabled = true;
  textMode = (TextcastMode)prefs.getUChar("mode", (uint8_t)TEXTCAST_AUTO);
  tailGexLoad();

  addLog("");
  addLog("============================================================");
  addLog(String(GLADIUS_VERSION));
  addLog("NO AP / NO WEB UI");
  addLog("STA: JANUS_WIFI_PLACEHOLDER");
  addLog("PUBLIC OUTPUT: fake SSID fields only");
  addLog("FIELDS: ORACLE + MINER + SHARE/EVENT + MACHINE JG2|...");
  addLog("SWARM_LANGUAGE: passive fake-SSID sniff + neighbor table");
  addLog("TAIL_GEX: lane tail exposure memory + Buzz/Core G/M memory packets + TEXTCAST/JG2 scent");
  addLog("BUZZ_BRIDGE: accepts J/B real header jobs and returns S/2 exact target shares");
  addLog("TAILGEX v1.23: QUIET_PACT_SPRINT + GLAD_PARRY one-line + always-on miner SSID");
  addLog("FACE_SYNC: Anchor green / Gladius turquoise; J/T twin race shares Buzz progress and SHARE swaps faces");
  addLog("QUIET_PACT_SPRINT: FACE calm, TWIN pact cooldown, ORACLE deferred, reward sprint kept hot");
  addLog("GLAD_PARRY: one temporary SSID line with possible vendor guess; miner stats SSID stays always active");
  addLog("TRANCEPTION_LITE: scheduler-only fitness hint; SHA/header/target/S2 frozen");
  addLog("DUALLOCK: zbits is signal; target[32] compare is truth gate");
  addLog("SELF_JOB: if Buzz is silent, Gladius mines local proof-job for night TailGEX");
  addLog("============================================================");

  setupExtraLed();
  setupBrightnessButton();
  lastLedMs = 0;

  for (uint8_t i = 0; i < FIELD_COUNT; i++) {
    memset(&fields[i], 0, sizeof(fields[i]));
    fieldMakeBssid(i, fields[i].bssid);
  }

  fieldSet(FIELD_ORACLE, "JANUS GLADIUS BOOT", 0, 3600, 1000);
  fieldSet(FIELD_MINER, "GLAD WAIT BUZZ JOB", 0, 3600, 1150);
  fieldSet(FIELD_MACHINE, "JG2|BOOT", 0, 60000, 1350);
  updateMachineField(true);

  memset(&currentJob, 0, sizeof(currentJob));
  currentJob.targetBits = 22;
  currentJob.lane = LANE_ZIM_REVERSE;
  currentJob.arm = 5;
  currentJob.stride = 17;
  activeLane = currentJob.lane;
  activeArm = currentJob.arm;
  activeStride = currentJob.stride;

  wifiQueue = xQueueCreate(8, sizeof(WifiEventItem));
  startWifiSta();
  startEspNow();
  gladiusPresenceBurst("boot");
  startTextcastSniffer();

  xTaskCreatePinnedToCore(wifiMonitorTask, "glad_wifi", 4096, nullptr, 3, nullptr, 0);
  xTaskCreatePinnedToCore(nowParserTask, "glad_now", 4096, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(swarmSniffTask, "glad_sniff", 4096, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(swarmAdaptTask, "glad_adapt", 4096, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(serviceTask, "glad_service", 4096, nullptr, 2, nullptr, 0);
  xTaskCreatePinnedToCore(statusTask, "glad_status", 4096, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(oracleTask, "glad_oracle", 8192, nullptr, 1, nullptr, 0);
  xTaskCreatePinnedToCore(minerTask, "glad_miner", 8192, nullptr, 1, nullptr, 1);

  requestOracle("boot", true);

  logf("BOOT", "nodeId=%u heap=%lu mode=%u led=%u uartFull=%u smallLed=%u face=green_turquoise_twin",
       nodeId,
       (unsigned long)ESP.getFreeHeap(),
       (uint8_t)textMode,
       ledBrightness,
       janusUart0FullLog ? 1 : 0,
       extraLedEnabled ? 1 : 0);
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd.length()) {
      String res = runCommand(cmd);
      logf("SERIAL", "'%s' -> %s", cmd.c_str(), res.c_str());
    }
  }

  delay(150);
}
