#include <M5Unified.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <M5_STHS34PF80.h>
#include <LittleFS.h>
#include <math.h>

// ========================= JANUS COLONY ESP-NOW v2.12B BLIND EYE RF FUSION =========================
// v2.12B keeps the v2.12 RF/policy/miner layer and softens TMOS calibration.
// Boot warmup now lets the baseline settle gently instead of freezing on huge deltas.
#include <esp_now.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>

#define JANUS_COLONY_ENABLE 1
#define JANUS_BROADCAST_CHANNEL 0
#define COLONY_HEARTBEAT_MS 2000UL
#define COLONY_ENTROPY_MS 2500UL
#define COLONY_MASTER_TIMEOUT_MS 18000UL
#define COLONY_REMOTE_BATCH 220
#define COLONY_JOB_RANGE_DEFAULT 262144UL
#define COLONY_NO_SELF_MINING 1

// v2.5: Buzz v10.11+ Agent rewards.
// Old Buzz builds ignore this; new Buzz sends 'A','R' packets with score,
// entropySeed and targetBatch. This worker never treats rewards as ACCEPT shares.
#define COLONY_AGENT_ENABLE 1
#define COLONY_AGENT_BATCH_MIN 80
#define COLONY_AGENT_BATCH_MAX 900
#define COLONY_AGENT_REWARD_VISIBLE_MS 6000UL

// v2.7: JANUS Kenshi Bubble Bus.
// This is a compatibility layer above ESP-NOW, not a replacement for old JANUS/E2 packets.
// Idea: only hot/near/important nodes are "materialized"; quiet nodes stay virtual
// as tiny timers, flags and Markov transition hints.
#define JANUS_KENSHI_BUS_ENABLE       1
#define JANUS_KENSHI_MAX_NODES        18
#define JANUS_KENSHI_SECTORS          8
#define JANUS_KENSHI_ACTIVE_TTL_MS    9000UL
#define JANUS_KENSHI_VIRTUAL_TTL_MS   45000UL
#define JANUS_KENSHI_ALERT_TX_MS      700UL
#define JANUS_KENSHI_ACTIVE_TX_MS     1500UL
#define JANUS_KENSHI_BG_TX_MS         5000UL
#define JANUS_KENSHI_SAVE_FILE        "/eye_kenshi.bin"

// v2.8: JANUS Tachyon Prophecy + Eye Vision.
// The Python Physarious Movement Tachyon model cannot run on the Atom,
// so this is its embedded micro-brother: tiny temporal memory, viscosity,
// energy/stress gates, sector prediction and ESP-NOW prophecy exchange.
#define JANUS_TACHYON_PROPHECY_ENABLE      1
#define JANUS_TACHYON_SEQ_N                16
#define JANUS_TACHYON_REMOTE_N             10
#define JANUS_TACHYON_TX_BG_MS             2200UL
#define JANUS_TACHYON_TX_ALERT_MS          650UL
#define JANUS_TACHYON_REMOTE_TTL_MS        12000UL
#define JANUS_TACHYON_SAVE_FILE            "/eye_tachyon.bin"

#define JANUS_EYE_VISION_ENABLE            1
#define JANUS_EYE_VISION_W                 8
#define JANUS_EYE_VISION_H                 8
#define JANUS_EYE_FRAME_PIXELS             (JANUS_EYE_VISION_W * JANUS_EYE_VISION_H)
#define JANUS_EYE_VISION_DEFAULT_FRAME_MS  160
#define JANUS_EYE_VISION_IDLE_MS           12000UL

// v2.9 Eagle Focus: software aperture/AGC for the single-zone STHS34PF80.
// It does not fake distance; it makes weak far-field deltas visible without
// letting the room baseline drift into the target.
#define JANUS_EYE_EAGLE_FOCUS_ENABLE       1
#define JANUS_EYE_FOCUS_MIN_GAIN           2.20f
#define JANUS_EYE_FOCUS_MAX_GAIN           7.50f
#define JANUS_EYE_BASELINE_ALPHA_QUIET     0.0030f
#define JANUS_EYE_BASELINE_ALPHA_HOT       0.00008f
#define JANUS_EYE_NOISE_ALPHA              0.0060f
#define JANUS_EYE_PRESENCE_FLAG_LEVEL      18.0f
#define JANUS_EYE_MOTION_FLAG_LEVEL        14.0f

// v2.9I TRUTH RELEASE: only signal/core fixes. I2C/TMOS init stays exactly v2.9.
// Purpose: do not see ghosts when the room is empty after stale saved baseline.
#define JANUS_EYE_RECALIBRATE_ON_BOOT      1
#define JANUS_EYE_STUCK_RAW_ABS            16000
#define JANUS_EYE_MEMORY_DECAY             0.925f
#define JANUS_EYE_MEMORY_ATTACK            0.110f
#define JANUS_EYE_GHOST_DECAY              0.820f
#define JANUS_EYE_STALE_RELEASE_MS         1800UL
#define JANUS_EYE_NOW_HOLD_MS              900UL
#define JANUS_EYE_COOL_PRESENCE_WEIGHT     0.32f
#define JANUS_EYE_FLAG_PRESENCE_NOW        0x40
#define JANUS_EYE_FLAG_MOTION_NOW          0x80


// v2.10G JANUS BLACKBOARD + Episodic Eye Memory + SwarmSense + Atomic Motion Base scaffold.
// Atomic Motion Base v1.2 uses I2C @0x38 for 4 servos + 2 DC motors; INA226 power
// monitor is probed separately. All actuator writes are OFF by default.
// To physically move the future TMOS/PIR pan head, set JANUS_MOTION_BASE_WRITE_ENABLE to 1
// and explicitly arm through Core policy or local flag.
#define JANUS_EVENT_BUS_ENABLE              1
#define JANUS_EVENT_TX_BASE_MS              3500UL
#define JANUS_EVENT_TX_ALERT_MS             750UL
#define JANUS_EVENT_MOTION_COOLDOWN_MS      650UL
#define JANUS_EVENT_POLICY_TTL_GUARD_MS     15000UL

// v2.10G: Eye semantic memory / task need / SwarmSense mirror.
#define JANUS_EYE_EPISODE_ENABLE            1
#define JANUS_EYE_EPISODE_COUNT             16
#define JANUS_EYE_EPISODE_RECORD_MS         3000UL
#define JANUS_EYE_AI_MEMORY_TX_MS           30000UL
#define JANUS_EYE_TASK_NEED_MS              20000UL
#define JANUS_EYE_TASK_DONE_MS              12000UL
#define JANUS_EYE_SWARMSENSE_ENABLE         1
#define JANUS_EYE_SWARMSENSE_TX_MS          5000UL
#define JANUS_EYE_SWARMSENSE_ALERT_MS       1600UL
#define JANUS_EYE_RECALIBRATE_GHOST_LEVEL   0.72f
#define JANUS_EYE_RECALIBRATE_BAD_FRAMES    8
#define JANUS_EYE_QUIET_STRESS_LEVEL        1.15f

// v2.12 RuView-lite RF Fusion.
// Stable first stage: no CSI yet, only WiFi RSSI drift + ESP-NOW RX RSSI pressure.
// It gives BlindEye a camera-free "radio skin" that can be fused with TMOS/mic/IMU.
#define JANUS_RF_LITE_ENABLE                 1
#define JANUS_RF_LITE_SAMPLE_MS              120UL
#define JANUS_RF_LITE_BASELINE_ALPHA_QUIET   0.0060f
#define JANUS_RF_LITE_BASELINE_ALPHA_HOT     0.0007f
#define JANUS_RF_LITE_NOISE_ALPHA            0.0250f
#define JANUS_RF_LITE_MOTION_LEVEL_DB        3.2f
#define JANUS_RF_LITE_PRESENCE_LEVEL         0.42f
#define JANUS_RF_LITE_ANOMALY_LEVEL          1.18f
#define JANUS_RF_LITE_PACKET_TTL_MS          4500UL

// v2.12 safety/debug layer.
#define JANUS_EYE_VERSION_LABEL              "v2.14C_BASELESS_SENSOR_FALLBACK"
#define JANUS_RF_LITE_DEBUG_MS               2500UL
#define JANUS_TMOS_WARMUP_MS                 90000UL
#define JANUS_POLICY_SMOOTH_MIN_DWELL_MS     2500UL
#define JANUS_POLICY_ALERT_CONFIRM           2
#define JANUS_POLICY_RECOVER_CONFIRM         3
#define JANUS_GHOST_TASKNEED_LEVEL           0.96f
#define JANUS_GHOST_TASKNEED_HOLD_MS         45000UL
#define JANUS_GHOST_TASKNEED_COOLDOWN_MS     60000UL

// v2.12B: soft TMOS settling. During the first warmup window the baseline is
// allowed to glide toward the real room temperature field, while output is damped.
// This removes the wild several-thousand-count deltas after a bad boot angle.
#define JANUS_TMOS_WARMUP_SETTLE_ALPHA       0.0450f
#define JANUS_TMOS_WARMUP_SOFT_ALPHA         0.0180f
#define JANUS_TMOS_WARMUP_NOISE_ALPHA        0.0220f
#define JANUS_TMOS_WARMUP_OUTPUT_SCALE       0.18f
#define JANUS_TMOS_WARMUP_GAIN_MAX           4.20f
#define JANUS_TMOS_BASELINE_JUMP_LEVEL       850.0f
#define JANUS_TMOS_POSTWARM_JUMP_ALPHA       0.0012f


// Grove TMOS pins must be visible before any I2C helper function.
// Arduino IDE auto-prototypes functions, so keep these near the top.
#ifndef GROVE_SDA_PIN
#define GROVE_SDA_PIN          2
#endif
#ifndef GROVE_SCL_PIN
#define GROVE_SCL_PIN          1
#endif

// v2.12 Buzz lottery miner scheduler-only imports from RBLGANUL V31.
// This does NOT change block/header wire bytes. It only changes nonce walk order.
#define JANUS_MINER_V31_SCHEDULER_ENABLE     1
#define JANUS_MINER_V31_SECTORS              8


#define JANUS_MOTION_BASE_ENABLE            1
#define JANUS_MOTION_BASE_WRITE_ENABLE      1   // ROBOZOMBIE TEST: physical servo writes ON
#define JANUS_MOTION_BASE_I2C_ADDR          0x38
#define JANUS_MOTION_BASE_INA226_ADDR_A     0x40
#define JANUS_MOTION_BASE_INA226_ADDR_B     0x41
// Atomic Motion Base is NOT on the external TMOS Grove bus on most ATOM builds.
// Official M5Atomic-Motion library defaults to SDA=25, SCL=21 for the base MCU.
#define JANUS_MOTION_BASE_SDA_PIN           38     // ATOM S3 / S3R Atomic Motion Base I2C SDA
#define JANUS_MOTION_BASE_SCL_PIN           39     // ATOM S3 / S3R Atomic Motion Base I2C SCL
#define JANUS_MOTION_BASE_TICK_MS           80UL
#define JANUS_MOTION_BASE_POWER_MS          1000UL
#define JANUS_MOTION_BASE_STATUS_MS         2500UL
#define JANUS_MOTION_BASE_ABSENT_STATUS_MS  10000UL  // v2.14C: when Motion Base is absent, report calmly and stay sensor-only
#define JANUS_MOTION_BASE_OPTIONAL          1        // v2.14C: BlindEye must work normally without the base
#define JANUS_EYE_POWER_TX_MS                2500UL
#define JANUS_MOTION_BASE_TRACK_SERVO_CH    0   // register ch0 = physical Servo1
#define JANUS_MOTION_BASE_TRACK_MIN_DEG     20
#define JANUS_MOTION_BASE_TRACK_MAX_DEG     160
#define JANUS_MOTION_BASE_TRACK_CENTER_DEG  90
#define JANUS_MOTION_BASE_MAX_STEP_DEG      3
#define JANUS_MOTION_BASE_LOW_MV            3400
#define JANUS_MOTION_BASE_SLEEP_MV          3200
#define JANUS_MOTION_BASE_EXT_MV            4350   // USB/boost/charger rail, not raw cell voltage
#define JANUS_MOTION_BASE_FULL_MV           4170
#define JANUS_MOTION_BASE_CHG_CURRENT_MIN   2      // raw INA226 current threshold, advisory only

// v2.13 RoboZombie crawler-turret wiring for your current build:
//   S1 = 180° TMOS/PIR pan head
//   S2 = left 360° continuous servo arm
//   S4 = right 360° continuous servo arm
//   S3 = empty/reserve
// Continuous servo convention: 90 = stop, below/above 90 = rotate opposite ways.
#define JANUS_ROBOZOMBIE_ENABLE              1
#define JANUS_ROBOZOMBIE_LOCAL_TEST_ARM      1   // allows local Serial/auto head test without waiting Core policy
#define JANUS_ROBOZOMBIE_AUTO_CRAWL_ENABLE   1   // v2.14B: confident auto-crawl when signal/power is good; Serial g still toggles manual
#define JANUS_ROBOZOMBIE_HEAD_SERVO_CH       0   // physical S1
#define JANUS_ROBOZOMBIE_LEFT_SERVO_CH       1   // physical S2
#define JANUS_ROBOZOMBIE_RIGHT_SERVO_CH      3   // physical S4
#define JANUS_ROBOZOMBIE_LEFT_REVERSE        0   // flip from Serial with 'l' if left pulls wrong way
#define JANUS_ROBOZOMBIE_RIGHT_REVERSE       1   // flip from Serial with 'r' if right pulls wrong way
#define JANUS_ROBOZOMBIE_SERVO_STOP          90
#define JANUS_ROBOZOMBIE_MAX_PULL_DELTA      30  // v2.14B: stronger but confidence/battery gated
#define JANUS_ROBOZOMBIE_CENTER_DEADBAND     7
#define JANUS_ROBOZOMBIE_PULSE_MS            185UL
#define JANUS_ROBOZOMBIE_REST_MS             390UL
#define JANUS_ROBOZOMBIE_IDLE_STOP_MS        900UL
#define JANUS_ROBOZOMBIE_CONFIDENT_GAIT       1
#define JANUS_ROBOZOMBIE_AUTO_MIN_MV          3480   // auto-crawl below this only if USB/external/charging flag is present
#define JANUS_ROBOZOMBIE_LOW_BATT_PULL_CAP    28
#define JANUS_ROBOZOMBIE_PIVOT_ERR_DEG        38
#define JANUS_ROBOZOMBIE_HEAD_PRESENT         1      // set 0 if S1/head servo is physically absent
#define JANUS_ROBOZOMBIE_LEFT_LEG_PRESENT     1      // set 0 if S2 left 360 servo is absent
#define JANUS_ROBOZOMBIE_RIGHT_LEG_PRESENT    1      // set 0 if S4 right 360 servo is absent
#define JANUS_ROBOZOMBIE_ALLOW_HEADLESS       1      // missing head must not break sensors/swarm/miner
#define JANUS_ROBOZOMBIE_ALLOW_LEGLESS        1      // missing legs must not break head/sensors/swarm/miner
#define JANUS_ROBOZOMBIE_SERIAL_S_PASSIVE     1      // Serial capital S: hard stop rotors/head and keep BlindEye passive sensor-only until rearmed


uint8_t JANUS_BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// IMPORTANT FIX v2.13E:
// Do NOT use TwoWire(1) for Atomic Motion on Arduino-ESP32 3.3.x here.
// It produced "NULL TX buffer pointer" in logs. We use the official M5 approach:
// one global Wire object, switched between:
//   Grove/TMOS bus   SDA=2  SCL=1
//   Motion Base bus  SDA=38 SCL=39 on AtomS3 / AtomS3R
// Every Motion write/read selects 38/39, then immediately returns Wire to Grove 2/1.
enum JanusI2CBusMode : uint8_t { JANUS_I2C_UNKNOWN = 0, JANUS_I2C_GROVE = 1, JANUS_I2C_MOTION = 2 };
static JanusI2CBusMode janusI2cMode = JANUS_I2C_UNKNOWN;
bool motionBaseUsesMainWire = true;
bool motionWireStarted = false;

bool janusSelectI2CBus(JanusI2CBusMode mode, bool force = false) {
  uint8_t sda = GROVE_SDA_PIN;
  uint8_t scl = GROVE_SCL_PIN;
  const char* name = "GROVE";
  if (mode == JANUS_I2C_MOTION) {
    sda = JANUS_MOTION_BASE_SDA_PIN;
    scl = JANUS_MOTION_BASE_SCL_PIN;
    name = "MOTION";
  } else if (mode != JANUS_I2C_GROVE) {
    mode = JANUS_I2C_GROVE;
  }

  if (!force && janusI2cMode == mode) return true;

  // Arduino-ESP32 3.3.x warns "Bus already started" if begin() is called
  // repeatedly. More importantly, it may keep the old pins. We explicitly end()
  // before switching between Grove(2/1) and Atomic Motion(38/39).
  if (janusI2cMode != JANUS_I2C_UNKNOWN || force) {
    Wire.end();
    delayMicroseconds(250);
  }

  bool ok = Wire.begin(sda, scl, 100000);
  Wire.setTimeOut(30);
  delayMicroseconds(250);
  janusI2cMode = ok ? mode : JANUS_I2C_UNKNOWN;
  if (!ok) {
    Serial.printf("[I2C] Wire.begin %s SDA=%u SCL=%u FAIL\n", name, (unsigned)sda, (unsigned)scl);
  }
  return ok;
}
bool janusSelectGroveBus(bool force = false) { return janusSelectI2CBus(JANUS_I2C_GROVE, force); }
bool janusSelectMotionBus(bool force = false) { return janusSelectI2CBus(JANUS_I2C_MOTION, force); }

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

struct __attribute__((packed)) ShareResponse {
  uint8_t magic[2];
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
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

// v2.10G: Native SwarmSense mirror. Same S/S ABI used by Stick/TRON class nodes.
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

// v2.5: Buzz Agent reward packet. Buzz v10.11C sends this as ESP-NOW unicast.
// It is a motivation/training/entropy signal, NOT a pool ACCEPT.
struct __attribute__((packed)) JanusAgentRewardPacket {
  uint8_t magic[2];        // 'A','R'
  uint8_t version;         // 1
  char source[16];         // BuzzAgent
  char targetNode[24];     // BlindEye / all / *
  uint32_t seq;
  uint8_t rewardLevel;     // 0 observe, 1 praise, 2 boost, 3 golden/share-ticket reward
  uint8_t aiHint;          // 1 stable, 2 slow, 3 boost
  uint16_t rewardPoints;
  uint16_t targetBatch;
  uint32_t entropySeed;
  float score;
  float predictedHashRate;
  float predictionError;
  uint32_t deltaShares;
  uint32_t uptime_ms;
};


struct __attribute__((packed)) JanusKenshiPacket {
  uint8_t magic[2];        // 'K','2'
  uint8_t version;         // 1
  uint8_t flags;           // bit0=active bubble, bit1=alert, bit2=virtual summary, bit3=future motion base ready
  char nodeId[24];
  uint32_t seq;
  uint16_t worker_id;
  uint32_t uptime_ms;
  uint8_t activeBubbleNodes;
  uint8_t virtualNodes;
  uint32_t worldFlags;     // low-cost global state snapshot
  uint8_t sector;          // current inferred sector 0..7
  uint8_t predictedSector; // Markov next sector 0..7
  uint8_t jobState;        // 0 idle, 1 watch, 2 track, 3 alert, 4 learn, 5 relay
  uint8_t priority;        // 0..255
  int8_t rssi;
  float entropy;
  float activity;
  float confidence;
  float values[6];         // presence, motion, mic, shock, loss, fit
};


struct __attribute__((packed)) JanusTachyonProphecyPacket {
  uint8_t magic[2];        // 'T','P'
  uint8_t version;         // 1
  uint8_t flags;           // bit0=presence now, bit1=motion now, bit2=alert, bit3=remote-assisted
  char nodeId[24];
  uint32_t seq;
  uint16_t worker_id;
  uint32_t uptime_ms;
  uint16_t horizon_ms;
  uint8_t sector;
  uint8_t predictedSector;
  uint8_t confidence;      // 0..100
  uint8_t jobState;
  float presence_now;
  float motion_now;
  float pred_presence_1;
  float pred_motion_1;
  float pred_presence_2;
  float pred_motion_2;
  float pred_presence_3;
  float pred_motion_3;
  float event_eta_ms;
  float future_stress;
  float swarm_pressure;
};

struct __attribute__((packed)) JanusEyeVisionControlPacket {
  uint8_t magic[2];        // 'E','C' Core2 -> BlindEye
  uint8_t version;         // 1
  uint8_t enable;          // 0 stop, 1 send E/F frames
  uint8_t mode;            // 1 TMOS/PIR aperture
  uint16_t frameMs;
  uint32_t seq;
  char source[16];
  char target[16];
};

struct __attribute__((packed)) JanusEyeFramePacket {
  uint8_t magic[2];        // 'E','F' BlindEye -> Core2
  uint8_t version;         // 1
  uint8_t width;
  uint8_t height;
  uint16_t seq;
  int16_t min_x10;
  int16_t max_x10;
  uint8_t flags;           // bit0 motion, bit1 presence, bit2 synthetic/aperture
  uint8_t pixels[JANUS_EYE_FRAME_PIXELS];
};

// v2.12C: BlindEye -> Core2 power/battery telemetry from Atomic Motion Base INA226.
// This is a side-channel packet, so old swarm nodes safely ignore it.
struct __attribute__((packed)) JanusEyePowerPacket {
  uint8_t magic[2];        // 'E','B' Eye Battery / Energy
  uint8_t version;         // 1
  uint8_t flags;           // bit0 base, bit1 INA, bit2 external/USB, bit3 low, bit4 critical, bit5 charging, bit6 full, bit7 pct-estimated
  char nodeId[24];         // BlindEye
  uint32_t seq;
  uint32_t uptime_ms;
  uint16_t bus_mv;
  int16_t current_raw;
  int16_t power_raw;
  uint8_t battery_pct;     // 0..100, estimated 1S Li-ion curve; 100 if external/5V
  uint8_t source;          // 0 unknown, 1 battery, 2 external, 3 charging, 4 full/float
  uint16_t servo_angle;
  uint16_t target_angle;
  uint32_t crc;
};


// Core v6.41 JANUS BLACKBOARD packets.
// Keep byte layout identical to Core2_v6_41_JANUS_BLACKBOARD_HOME_CORTEX.
enum JanusNodeRoleId : uint8_t {
  JR_UNKNOWN = 0,
  JR_CORE    = 1,
  JR_ZIM     = 2,
  JR_BUZZ    = 3,
  JR_BEACON  = 4,
  JR_TRON    = 5,
  JR_BLIND   = 6,
  JR_AUDIO   = 7,
  JR_PYRAMID = 8,
  JR_SENSOR  = 9,
  JR_RELAY   = 10
};

enum JanusSemanticEventType : uint8_t {
  JE_NONE        = 0,
  JE_BOOT        = 1,
  JE_HEARTBEAT   = 2,
  JE_ENV         = 3,
  JE_MOTION      = 4,
  JE_PRESENCE    = 5,
  JE_SOUND       = 6,
  JE_WIFI_WEAK   = 7,
  JE_LOW_HEAP    = 8,
  JE_HASH        = 9,
  JE_SOLO_ACCEPT = 10,
  JE_SOLO_REJECT = 11,
  JE_TASK_NEED   = 12,
  JE_TASK_DONE   = 13,
  JE_DANGER      = 14,
  JE_SAFE        = 15,
  JE_POLICY      = 16,
  JE_AI_MEMORY   = 17
};

enum JanusSwarmMood : uint8_t {
  JM_IDLE    = 0,
  JM_QUIET   = 1,
  JM_ALERT   = 2,
  JM_EXPLORE = 3,
  JM_GUARD   = 4,
  JM_RECOVER = 5
};

enum JanusNodeCapability : uint16_t {
  JC_TEMP     = 0x0001,
  JC_HUM      = 0x0002,
  JC_PRESS    = 0x0004,
  JC_IMU      = 0x0008,
  JC_MIC      = 0x0010,
  JC_TMOS     = 0x0020,
  JC_AIR      = 0x0040,
  JC_HASH     = 0x0080,
  JC_AUDIO    = 0x0100,
  JC_VISION   = 0x0200,
  JC_TOUCH    = 0x0400,
  JC_RELAY    = 0x0800,
  JC_MEMORY   = 0x1000,
  JC_AI       = 0x2000,
  JC_BATTERY  = 0x4000,
  JC_RF       = 0x8000
};

struct __attribute__((packed)) JanusEventPacket {
  uint8_t magic[2];        // 'J','E'
  uint8_t version;         // 1
  uint8_t eventType;
  uint8_t nodeRole;
  uint8_t confidence;      // 0..100
  uint8_t urgency;         // 0..100
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

struct __attribute__((packed)) JanusPolicyPacket {
  uint8_t magic[2];        // 'J','P'
  uint8_t version;         // 1
  uint8_t swarmMood;
  uint8_t radioRate;       // 0 low, 1 normal, 2 high
  uint8_t buzzBudget;      // 0 hold, 1 lazy, 2 normal, 3 boost
  uint8_t sensorRate;      // 0 low, 1 normal, 2 high
  uint8_t confidence;      // 0..100
  uint16_t flags;
  uint32_t seq;
  uint32_t ttlMs;
  uint32_t quietUntilMs;
  uint16_t dominantTopic;
  uint16_t danger_x100;
  char order[40];
};

struct JanusRemoteProphecyState {
  bool active = false;
  char nodeId[24] = "";
  uint32_t lastSeenMs = 0;
  uint32_t seq = 0;
  uint8_t sector = 0;
  uint8_t predictedSector = 0;
  uint8_t confidence = 0;
  uint8_t flags = 0;
  float presence_now = 0.0f;
  float motion_now = 0.0f;
  float pred_presence_1 = 0.0f;
  float pred_motion_1 = 0.0f;
  float pred_presence_2 = 0.0f;
  float pred_motion_2 = 0.0f;
  float pred_presence_3 = 0.0f;
  float pred_motion_3 = 0.0f;
  float future_stress = 0.0f;
  float swarm_pressure = 0.0f;
};

struct JanusKenshiNode {
  bool active = false;
  char nodeId[24] = "";
  uint32_t lastSeenMs = 0;
  uint32_t seq = 0;
  uint32_t worldFlags = 0;
  float entropy = 0.0f;
  float activity = 0.0f;
  float confidence = 0.0f;
  uint8_t sector = 0;
  uint8_t predictedSector = 0;
  uint8_t jobState = 0;
  uint8_t priority = 0;
  int8_t rssi = 0;
  uint8_t flags = 0;
};

struct JanusEyeEpisode {
  uint32_t atMs = 0;
  uint8_t eventType = JE_NONE;
  uint8_t confidence = 0;
  uint8_t urgency = 0;
  uint8_t sector = 0;
  uint8_t predictedSector = 0;
  uint8_t flags = 0;
  int16_t presence_x10 = 0;
  int16_t motion_x10 = 0;
  int16_t futureStress_x100 = 0;
  int16_t servoAngle_x10 = 0;
};

struct RemoteJobState {
  bool active = false;
  uint8_t job_id[8] = {};
  uint8_t header[80] = {};
  uint8_t target[32] = {};
  uint32_t startNonce = 0;
  uint32_t rangeSize = 0;
  uint32_t nonce = 0;
  uint32_t endNonce = 0;      // kept for debug; range wrap is handled by hashesDone
  uint32_t hashesDone = 0;
  uint32_t receivedAt = 0;
  // v2.12 scheduler-only lane metadata. Header/target bytes are still provided by Buzz.
  uint8_t minerLane = 0;
  uint8_t minerSector = 0;
  uint8_t minerStrideArm = 0;
  uint32_t minerSeed = 0;
  uint32_t minerStride = 1;
  uint32_t minerStartOffset = 0;
};

// Explicit prototypes for functions with custom Janus types.
// This prevents Arduino IDE auto-prototype generation from placing these
// signatures before the struct definitions and breaking compilation.
void colonyMinerConfigureForJob(RemoteJobState& job);
uint32_t colonyNextNonceV31(const RemoteJobState& job, uint32_t i);
bool looksLikeBuzzMaster(const JanusColonyPacket& pkt);
bool agentRewardTargetsThisEye(const JanusAgentRewardPacket& ar);
void onJanusAgentReward(const JanusAgentRewardPacket& ar);
void sendShareResponse(const RemoteJobState& job, uint32_t nonce);
void onJanusPolicyPacket(const JanusPolicyPacket& jp);
const JanusEyeEpisode* janusEyeLatestEpisode();
void onJanusKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi);
void onJanusTachyonProphecy(const JanusTachyonProphecyPacket& tp, int8_t rxRssi);
bool eyeVisionTargetsThisEye(const JanusEyeVisionControlPacket& ec);
void onJanusEyeVisionControl(const JanusEyeVisionControlPacket& ec);

RemoteJobState colonyJob;
volatile bool colonyMasterSeen = false;
uint32_t colonyLastMasterMs = 0;
uint32_t colonySeq = 0;
uint32_t colonyLastHeartbeatMs = 0;
uint32_t colonyLastEntropyMs = 0;
uint32_t colonyRemoteHashrate = 0;
uint32_t colonyRemoteShares = 0;
uint32_t colonyRemoteRejects = 0;
uint16_t colonyTargetBits = 0;
uint32_t colonyJobsSeen = 0;
uint32_t colonyJobsDone = 0;
uint32_t colonyJobsExpired = 0;
uint32_t colonyBestBits = 0;
uint32_t colonyHashCounter = 0;
uint32_t colonyMinerLaneSwitches = 0;
uint32_t colonyMinerTailHits = 0;
uint32_t colonyMinerBestNonce = 0;
uint32_t colonyLastHashTickMs = 0;
uint16_t colonyWorkerId = 0;
uint8_t colonyPeerChannel = 0;
int8_t colonyLastRssi = 0;
uint32_t colonyPeerRebuilds = 0;
uint32_t colonyTxOk = 0;
uint32_t colonyTxFail = 0;
esp_err_t colonyLastTxErr = ESP_OK;
char colonyLastTxTag[16] = "-";
char colonyMode[12] = "SEEK";

// v2.5 Buzz Agent state.
uint32_t colonyAgentRewardsRx = 0;
uint32_t colonyAgentShareRewardsRx = 0;  // reward events caused by worker ticket/share delta
uint32_t colonyAgentSeq = 0;
uint8_t colonyAgentLevel = 0;
uint8_t colonyAgentHint = 1;
uint16_t colonyAgentRewardPoints = 0;
uint16_t colonyAgentTargetBatch = COLONY_REMOTE_BATCH;
uint32_t colonyAgentEntropySeed = 0xB11D3E5EUL;
uint32_t colonyAgentLastRewardMs = 0;
uint32_t colonyAgentLastDeltaShares = 0;
float colonyAgentScore = 0.0f;
float colonyAgentPredictedHash = 0.0f;
float colonyAgentPredictionError = 0.0f;
char colonyAgentSource[16] = "-";

// v2.7 Kenshi Bubble Bus state.
// "Active bubble" = hot sensor/model event. "Virtual" = quiet nodes remembered by timers.
JanusKenshiNode kenshiNodes[JANUS_KENSHI_MAX_NODES];
uint32_t kenshiSeq = 0;
uint32_t kenshiLastTxMs = 0;
uint32_t kenshiLastRxMs = 0;
uint32_t kenshiRxPackets = 0;
uint32_t kenshiTxPackets = 0;
uint32_t kenshiWorldFlags = 0;
uint8_t kenshiBubbleState = 0;    // 0 sleep, 1 virtual, 2 active, 3 alert
uint8_t kenshiJobState = 0;       // 0 idle, 1 watch, 2 track, 3 alert, 4 learn, 5 relay
uint8_t kenshiSector = 0;
uint8_t kenshiPredSector = 0;
uint8_t kenshiPriority = 0;
uint8_t kenshiActiveNodes = 0;
uint8_t kenshiVirtualNodes = 0;
float kenshiConfidence = 0.0f;
float kenshiVirtualEntropy = 0.0f;
float kenshiEventPower = 0.0f;
float kenshiMarkov[JANUS_KENSHI_SECTORS][JANUS_KENSHI_SECTORS] = {0};
uint8_t kenshiLastSector = 0;
bool kenshiStateDirty = false;


uint16_t countLeadingZeroBitsBE(const uint8_t h[32]) {
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

void writeLE32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

void doubleSha256(const uint8_t *data, size_t len, uint8_t out[32]) {
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

bool hashMeetsTargetBE(const uint8_t hash[32], const uint8_t target[32]) {
  for (int i = 0; i < 32; i++) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;
}

void hashToShareOrder(const uint8_t in[32], uint8_t out[32]) {
  // Buzz/NerdMiner-compatible worker gate:
  // raw SHA256d bytes -> reversed/display share order -> compare with big-endian target.
  for (int i = 0; i < 32; ++i) out[i] = in[31 - i];
}

uint32_t janusBitReverse32(uint32_t x) {
  x = ((x & 0x55555555UL) << 1) | ((x >> 1) & 0x55555555UL);
  x = ((x & 0x33333333UL) << 2) | ((x >> 2) & 0x33333333UL);
  x = ((x & 0x0F0F0F0FUL) << 4) | ((x >> 4) & 0x0F0F0F0FUL);
  x = ((x & 0x00FF00FFUL) << 8) | ((x >> 8) & 0x00FF00FFUL);
  x = (x << 16) | (x >> 16);
  return x;
}

uint32_t janusXorShift32(uint32_t x) {
  if (!x) x = 0xA5A5A5A5UL;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

const char* colonyMinerLaneName(uint8_t lane) {
  switch (lane) {
    case 0: return "linear";
    case 1: return "zim_reverse";
    case 2: return "bitrev";
    case 3: return "janus_center";
    case 4: return "knight";
    case 5: return "random";
    default: return "linear";
  }
}

uint32_t colonyZimStrideArmValue(uint8_t arm) {
  static const uint32_t arms[] = {
    1UL, 3UL, 5UL, 7UL, 11UL, 17UL, 29UL, 31UL, 53UL, 97UL, 257UL, 521UL,
    4099UL, 65537UL, 0x9E3779B9UL, 0xC4111903UL, 0x4F1BBCDDUL
  };
  return arms[arm % (sizeof(arms) / sizeof(arms[0]))] | 1UL;
}

void colonyMinerConfigureForJob(RemoteJobState& job) {
#if JANUS_MINER_V31_SCHEDULER_ENABLE
  uint32_t seed = micros() ^ ESP.getCycleCount() ^ colonyAgentEntropySeed ^ ((uint32_t)colonyJobsSeen << 16) ^ (uint32_t)colonyWorkerId;
  for (uint8_t i = 0; i < 8; ++i) seed = janusXorShift32(seed ^ job.job_id[i]);
  job.minerSeed = seed;
  job.minerStrideArm = (uint8_t)((seed ^ (seed >> 8) ^ colonyAgentLevel) % 17);
  job.minerStride = colonyZimStrideArmValue(job.minerStrideArm);

  // V31-inspired scheduler-only lane mix: DualLock-ish sector bias + Zim reverse/linear/knight/bitrev/random.
  uint8_t selector = (uint8_t)((seed ^ (seed >> 11) ^ (uint32_t)colonyAgentHint ^ (uint32_t)colonyJobsSeen) % 100);
  if (colonyAgentHint >= 3 || colonyAgentLevel >= 2) {
    if (selector < 42) job.minerLane = 1;       // zim_reverse
    else if (selector < 64) job.minerLane = 4;  // knight
    else if (selector < 80) job.minerLane = 2;  // bitrev
    else if (selector < 92) job.minerLane = 3;  // janus_center
    else job.minerLane = 5;                     // random baseline
  } else {
    if (selector < 38) job.minerLane = 0;       // linear proof lane
    else if (selector < 67) job.minerLane = 1;  // zim_reverse
    else if (selector < 80) job.minerLane = 2;  // bitrev
    else if (selector < 92) job.minerLane = 3;  // janus_center
    else job.minerLane = 5;
  }

  // DualLock flavor in 8 local sectors: prefer sector 6 for linear/zim, sector 7 for knight-tail probing.
  if (job.minerLane == 0 || job.minerLane == 1) job.minerSector = 6 % JANUS_MINER_V31_SECTORS;
  else if (job.minerLane == 4) job.minerSector = 7 % JANUS_MINER_V31_SECTORS;
  else job.minerSector = (uint8_t)((seed >> 24) % JANUS_MINER_V31_SECTORS);
  job.minerStartOffset = janusBitReverse32(seed ^ 0xC4111903UL);
  colonyMinerLaneSwitches++;
#else
  job.minerLane = 0;
  job.minerSector = 0;
  job.minerStrideArm = 0;
  job.minerStride = 1;
  job.minerSeed = micros();
  job.minerStartOffset = 0;
#endif
}

uint32_t colonyNextNonceV31(const RemoteJobState& job, uint32_t i) {
#if JANUS_MINER_V31_SCHEDULER_ENABLE
  uint32_t range = job.rangeSize ? job.rangeSize : COLONY_JOB_RANGE_DEFAULT;
  if (range == 0) range = 1;
  uint32_t sectors = JANUS_MINER_V31_SECTORS;
  uint32_t sector = job.minerSector % sectors;
  uint32_t sectorWidth = max<uint32_t>(1UL, range / sectors);
  uint32_t sectorStart = min<uint32_t>(range - 1UL, sector * sectorWidth);
  if (sector == sectors - 1 || sectorStart + sectorWidth > range) sectorWidth = range - sectorStart;
  sectorWidth = max<uint32_t>(1UL, sectorWidth);

  uint32_t local = 0;
  uint32_t stride = job.minerStride | 1UL;
  uint32_t seed = job.minerSeed ^ job.minerStartOffset;

  switch (job.minerLane) {
    case 1: { // zim_reverse: seeded cursor, odd reverse stride.
      uint32_t cursor = seed % sectorWidth;
      uint32_t walk = (uint32_t)(((uint64_t)(i % sectorWidth) * stride) % sectorWidth);
      local = sectorStart + ((cursor + sectorWidth - walk) % sectorWidth);
      break;
    }
    case 2: // bitrev: jump across scales.
      local = janusBitReverse32(seed + i) % range;
      break;
    case 3: { // janus_center: center, -1, +1, -2, +2...
      uint32_t step = (i + 1UL) >> 1;
      uint32_t center = sectorWidth >> 1;
      uint32_t off = (i & 1UL) ? (center + step) : (center + sectorWidth - (step % sectorWidth));
      local = sectorStart + (off % sectorWidth);
      break;
    }
    case 4: // knight: golden-ratio odd walk inside a locked sector.
      local = sectorStart + ((seed + i * 0x9E3779B9UL) % sectorWidth);
      break;
    case 5: { // random baseline, deterministic/reproducible per job.
      uint32_t x = janusXorShift32(seed + i * 0xA5A5A5A5UL);
      local = x % range;
      break;
    }
    case 0:
    default:
      local = (job.minerStartOffset + i) % range;
      break;
  }
  return job.startNonce + local;
#else
  return job.nonce;
#endif
}

bool looksLikeBuzzMaster(const JanusColonyPacket& pkt) {
  return strstr(pkt.nodeId, "Buzz") || strstr(pkt.nodeId, "BUZZ") ||
         strstr(pkt.role, "Buzz") || strstr(pkt.role, "BUZZ") ||
         strstr(pkt.role, "MASTER") || strstr(pkt.role, "Master");
}

bool agentRewardTargetsThisEye(const JanusAgentRewardPacket& ar) {
  if (ar.magic[0] != 'A' || ar.magic[1] != 'R') return false;
  if (ar.targetNode[0] == '\0') return true;
  if (!strcmp(ar.targetNode, "*")) return true;
  if (!strcasecmp(ar.targetNode, "all")) return true;
  if (!strcasecmp(ar.targetNode, "BlindEye")) return true;
  if (!strcasecmp(ar.targetNode, "atom_s3r_blind_eye")) return true;
  return strstr(ar.targetNode, "BlindEye") || strstr(ar.targetNode, "EYE");
}

uint16_t effectiveColonyRemoteBatch() {
#if COLONY_AGENT_ENABLE
  uint16_t b = colonyAgentTargetBatch ? colonyAgentTargetBatch : COLONY_REMOTE_BATCH;

  // Headless Eye keeps sensors responsive. Agent may boost, but not enough to starve sensors.
  if (colonyAgentHint == 2) b = min<uint16_t>(b, 180);
  if (colonyAgentHint == 3 && colonyMasterSeen) b = max<uint16_t>(b, COLONY_REMOTE_BATCH);

  return constrain((int)b, COLONY_AGENT_BATCH_MIN, COLONY_AGENT_BATCH_MAX);
#else
  return COLONY_REMOTE_BATCH;
#endif
}

void onJanusAgentReward(const JanusAgentRewardPacket& ar) {
#if COLONY_AGENT_ENABLE
  if (!agentRewardTargetsThisEye(ar)) return;

  colonyAgentRewardsRx++;
  colonyAgentSeq = ar.seq;
  colonyAgentLevel = ar.rewardLevel;
  colonyAgentHint = ar.aiHint ? ar.aiHint : 1;
  colonyAgentRewardPoints = ar.rewardPoints;
  colonyAgentTargetBatch = ar.targetBatch ? ar.targetBatch : COLONY_REMOTE_BATCH;
  colonyAgentTargetBatch = constrain((int)colonyAgentTargetBatch, COLONY_AGENT_BATCH_MIN, COLONY_AGENT_BATCH_MAX);
  colonyAgentEntropySeed ^= ar.entropySeed ^ micros() ^ ((uint32_t)ar.rewardLevel << 24);
  colonyAgentLastRewardMs = millis();
  colonyAgentLastDeltaShares = ar.deltaShares;
  colonyAgentScore = ar.score;
  colonyAgentPredictedHash = ar.predictedHashRate;
  colonyAgentPredictionError = ar.predictionError;
  strlcpy(colonyAgentSource, ar.source, sizeof(colonyAgentSource));

  if (ar.deltaShares > 0 || ar.rewardLevel >= 3) colonyAgentShareRewardsRx++;

  // Do not touch status/model globals here: they are declared later.
  // updateMiniGPT() consumes colonyAgent* state safely after all model globals exist.

  Serial.printf("[AGENT] RX seq=%lu lvl=%u hint=%u pts=%u batch=%u score=%.1f predH=%.1f err=%.3f dShare=%lu\n",
                (unsigned long)ar.seq,
                (unsigned)ar.rewardLevel,
                (unsigned)ar.aiHint,
                (unsigned)ar.rewardPoints,
                (unsigned)colonyAgentTargetBatch,
                ar.score,
                ar.predictedHashRate,
                ar.predictionError,
                (unsigned long)ar.deltaShares);
#endif
}

void onJanusHeartbeat(const JanusColonyPacket& pkt);
void onJanusEntropy(const EntropyReport& er, const void* opt);
void onJanusEntropyV2(const EntropyReportV2& er2);
void onJanusAgentReward(const JanusAgentRewardPacket& ar);
void sendNodeHeartbeat();
void sendNodeEntropy();
float eyeLocalEntropy();
void updateKenshiVirtualWorld();
void onJanusKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi);
void sendKenshiBubblePacket();
void kenshiBubbleTick();
void saveKenshiState();
void loadKenshiState();
void updateTachyonProphecy();
void sendTachyonProphecyPacket(bool force=false);
void onJanusTachyonProphecy(const JanusTachyonProphecyPacket& tp, int8_t rxRssi);
void tachyonProphecyTick();
void saveTachyonState();
void loadTachyonState();
void onJanusEyeVisionControl(const JanusEyeVisionControlPacket& ec);
void eyeVisionTick();
void sendEyeVisionFrame();

void onJanusPolicyPacket(const JanusPolicyPacket& jp);
bool janusEmitEyeEvent(uint8_t eventType, uint8_t confidence, uint8_t urgency,
                       int16_t a_x10, int16_t b_x10, int16_t c_x10, int16_t d_x10,
                       uint16_t topicHash, uint16_t objectHash, uint32_t ttlMs);
void janusEventTick(bool force=false);
void initMotionBase();
void motionBaseTick();
void motionBaseSafeStop();
void motionBaseStopCrawler(const char* reason);
void motionBaseCrawlerTick(uint32_t now);
bool motionBaseWriteServoAngle(uint8_t ch, uint8_t angle, const char* tag);
void handleRoboZombieSerial();
void motionBasePlanTarget();
void motionBaseSendStatusEvent(bool force=false);
void motionBaseSendPowerPacket(bool force=false);
void ensureColonyPeer();
bool janusEyeEspNowSend(const char* tag, const void* payload, size_t len, bool repairOnFail=true);
bool forceColonyPeerRebuild(const char* reason);

void janusEyeRecordEpisode(uint8_t eventType, uint8_t confidence, uint8_t urgency, uint8_t flags=0);
void janusEyeSemanticTick(uint32_t now, bool force=false);
void janusEyeSwarmSenseTick(uint32_t now, bool force=false);
void rfLiteOnPacketRssi(int8_t rssi);
void rfLiteTick(uint32_t now);
void rfLiteDebugTick(uint32_t now, bool force=false);
float rfLiteFusionScore();
bool tmosWarmupActive(uint32_t now);
uint8_t janusPolicySmoothMood(uint8_t rawMood, uint8_t confidence, uint32_t now);
const char* janusMoodName(uint8_t mood);
uint32_t colonyNextNonceV31(const RemoteJobState& job, uint32_t i);
void colonyMinerConfigureForJob(RemoteJobState& job);
const char* colonyMinerLaneName(uint8_t lane);


void sendShareResponse(const RemoteJobState& job, uint32_t nonce) {
  ShareResponse sr{};
  sr.magic[0] = 'S'; sr.magic[1] = 'R';
  memcpy(sr.job_id, job.job_id, 8);
  sr.nonce = nonce;
  sr.worker_id = colonyWorkerId;
  janusEyeEspNowSend("share", &sr, sizeof(sr), true);
  // This is a worker-found ticket sent to Buzz. Pool ACCEPT is counted by Buzz.
  colonyRemoteShares++;
}

void runRemoteMiningBatch() {
  uint32_t now = millis();

  if (!colonyJob.active) {
    if (now - colonyLastHashTickMs >= 1000) {
      colonyRemoteHashrate = 0;
      colonyHashCounter = 0;
      colonyLastHashTickMs = now;
    }
    return;
  }

  if (now - colonyJob.receivedAt > COLONY_MASTER_TIMEOUT_MS) {
    colonyJob.active = false;
    colonyJobsExpired++;
    strlcpy(colonyMode, "SEEK", sizeof(colonyMode));
    colonyRemoteHashrate = 0;
    return;
  }

  uint8_t header[80];
  uint8_t rawHash[32];
  uint8_t shareHash[32];

  uint16_t activeBatch = effectiveColonyRemoteBatch();
  for (uint16_t i = 0; i < activeBatch; i++) {
    if (colonyJob.hashesDone >= colonyJob.rangeSize) {
      colonyJob.active = false;
      colonyJobsDone++;
      strlcpy(colonyMode, "WAIT", sizeof(colonyMode));
      break;
    }

    uint32_t nonce = colonyNextNonceV31(colonyJob, colonyJob.hashesDone);
    colonyJob.nonce = nonce + 1;
    colonyJob.hashesDone++;

    memcpy(header, colonyJob.header, 80);
    writeLE32(header + 76, nonce);

    doubleSha256(header, 80, rawHash);
    hashToShareOrder(rawHash, shareHash);

    colonyHashCounter++;
    uint16_t bits = countLeadingZeroBitsBE(shareHash);
    if (bits > colonyBestBits) {
      colonyBestBits = bits;
      colonyMinerBestNonce = nonce;
    }
    if (bits >= 22) colonyMinerTailHits++;

    if ((bits >= colonyTargetBits) && hashMeetsTargetBE(shareHash, colonyJob.target)) {
      sendShareResponse(colonyJob, nonce);
      colonyJob.active = false;
      colonyJobsDone++;
      strlcpy(colonyMode, "TICKET", sizeof(colonyMode));
      break;
    }
  }

  if (now - colonyLastHashTickMs >= 1000) {
    colonyRemoteHashrate = colonyHashCounter;
    colonyHashCounter = 0;
    colonyLastHashTickMs = now;
  }
}

uint8_t getWifiChannelSafe() {
  uint8_t primary = 0; wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) != ESP_OK) return 0;
  return primary;
}

bool forceColonyPeerRebuild(const char* reason) {
#if JANUS_COLONY_ENABLE
  if (WiFi.status() != WL_CONNECTED) return false;

  uint8_t ch = getWifiChannelSafe();
  if (ch == 0 && WiFi.status() == WL_CONNECTED) ch = WiFi.channel();
  if (ch == 0) ch = 1;

  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) {
    esp_now_del_peer(JANUS_BROADCAST_MAC);
  }

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6);
  peer.channel = ch;
  peer.encrypt = false;

  esp_err_t err = esp_now_add_peer(&peer);
  if (err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST) {
    colonyPeerChannel = ch;
    colonyPeerRebuilds++;
    Serial.printf("[COLONY/EYE] peer ready ch=%u rebuilds=%lu reason=%s\n",
                  (unsigned)ch, (unsigned long)colonyPeerRebuilds, reason ? reason : "-");
    return true;
  }

  colonyPeerChannel = 0;
  Serial.printf("[COLONY/EYE] peer rebuild FAIL err=%d ch=%u reason=%s\n",
                (int)err, (unsigned)ch, reason ? reason : "-");
  return false;
#else
  (void)reason;
  return false;
#endif
}

void ensureColonyPeer() {
#if JANUS_COLONY_ENABLE
  if (WiFi.status() != WL_CONNECTED) return;

  uint8_t ch = getWifiChannelSafe();
  if (ch == 0 && WiFi.status() == WL_CONNECTED) ch = WiFi.channel();
  if (ch == 0) ch = 1;

  bool exists = esp_now_is_peer_exist(JANUS_BROADCAST_MAC);
  if (exists && colonyPeerChannel == ch) return;

  // Non-destructive fast path: only rebuild when the peer vanished or channel changed.
  forceColonyPeerRebuild(exists ? "channel-change" : "ensure");
#endif
}

bool janusEyeEspNowSend(const char* tag, const void* payload, size_t len, bool repairOnFail) {
#if JANUS_COLONY_ENABLE
  if (!payload || len == 0) return false;
  if (WiFi.status() != WL_CONNECTED) {
    colonyTxFail++;
    colonyLastTxErr = ESP_ERR_ESPNOW_IF;
    strlcpy(colonyLastTxTag, tag ? tag : "wifi-off", sizeof(colonyLastTxTag));
    return false;
  }

  ensureColonyPeer();
  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)payload, len);
  if (err == ESP_OK) {
    colonyTxOk++;
    return true;
  }

  colonyTxFail++;
  colonyLastTxErr = err;
  strlcpy(colonyLastTxTag, tag ? tag : "send", sizeof(colonyLastTxTag));
  colonyPeerChannel = 0;
  Serial.printf("[COLONY/EYE] TX FAIL tag=%s err=%d fail=%lu ch=%u\n",
                colonyLastTxTag, (int)err, (unsigned long)colonyTxFail, (unsigned)colonyPeerChannel);
  if (repairOnFail) forceColonyPeerRebuild(tag ? tag : "tx-fail");
  return false;
#else
  (void)tag; (void)payload; (void)len; (void)repairOnFail;
  return false;
#endif
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onColonyRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
#else
void onColonyRecv(const uint8_t *mac, const uint8_t *data, int len)
#endif
{
  if (!data || len < 2) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (info && info->rx_ctrl) colonyLastRssi = info->rx_ctrl->rssi;
#endif
  rfLiteOnPacketRssi(colonyLastRssi);

  if (len == sizeof(JanusPolicyPacket) && data[0] == 'J' && data[1] == 'P') {
    JanusPolicyPacket jp{};
    memcpy(&jp, data, sizeof(jp));
    onJanusPolicyPacket(jp);
    return;
  }
  if (len == sizeof(JanusEventPacket) && data[0] == 'J' && data[1] == 'E') {
    // Core consumes J/E. BlindEye only ignores echoes/other semantic events for now.
    return;
  }

  if (len == sizeof(JanusColonyPacket)) {
    JanusColonyPacket pkt{}; memcpy(&pkt, data, sizeof(pkt));
    if (memcmp(pkt.magic, "JANUS", 5) == 0 && looksLikeBuzzMaster(pkt)) {
      colonyMasterSeen = true;
      colonyLastMasterMs = millis();
      if (!colonyJob.active) strlcpy(colonyMode, "READY", sizeof(colonyMode));
    }
    onJanusHeartbeat(pkt);
    return;
  }
  if (len == sizeof(JobPacket) && data[0] == 'J' && data[1] == 'B') {
    JobPacket jp{}; memcpy(&jp, data, sizeof(jp));
    memcpy(colonyJob.job_id, jp.job_id, 8);
    memcpy(colonyJob.header, jp.header, 80);
    memcpy(colonyJob.target, jp.target, 32);
    colonyJob.startNonce = jp.start_nonce;
    colonyJob.rangeSize = jp.range_size ? jp.range_size : COLONY_JOB_RANGE_DEFAULT;
    colonyJob.nonce = jp.start_nonce;
    colonyJob.endNonce = jp.start_nonce + colonyJob.rangeSize;
    colonyJob.hashesDone = 0;
    colonyJob.receivedAt = millis();
    colonyMinerConfigureForJob(colonyJob);
    colonyJob.active = true;
    colonyTargetBits = countLeadingZeroBitsBE(colonyJob.target);
    colonyJobsSeen++;
    colonyMasterSeen = true;
    colonyLastMasterMs = millis();
    strlcpy(colonyMode, "REMOTE", sizeof(colonyMode));
    return;
  }
  if (len == sizeof(JanusAgentRewardPacket) && data[0] == 'A' && data[1] == 'R') {
    JanusAgentRewardPacket ar{};
    memcpy(&ar, data, sizeof(ar));
    onJanusAgentReward(ar);
    return;
  }
  if (len == sizeof(JanusKenshiPacket) && data[0] == 'K' && data[1] == '2') {
    JanusKenshiPacket kp{};
    memcpy(&kp, data, sizeof(kp));
    onJanusKenshiPacket(kp, colonyLastRssi);
    return;
  }
  if (len == sizeof(JanusTachyonProphecyPacket) && data[0] == 'T' && data[1] == 'P') {
    JanusTachyonProphecyPacket tp{};
    memcpy(&tp, data, sizeof(tp));
    onJanusTachyonProphecy(tp, colonyLastRssi);
    return;
  }
  if (len == sizeof(JanusEyeVisionControlPacket) && data[0] == 'E' && data[1] == 'C') {
    JanusEyeVisionControlPacket ec{};
    memcpy(&ec, data, sizeof(ec));
    onJanusEyeVisionControl(ec);
    return;
  }
  if (len == sizeof(EntropyReport) && data[0] == 'E' && data[1] == 'R') { EntropyReport er{}; memcpy(&er, data, sizeof(er)); onJanusEntropy(er, nullptr); return; }
  if (len == sizeof(EntropyReportV2) && data[0] == 'E' && data[1] == '2') { EntropyReportV2 er2{}; memcpy(&er2, data, sizeof(er2)); onJanusEntropyV2(er2); return; }
}

void initColonyNow() {
#if JANUS_COLONY_ENABLE
  colonyWorkerId = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { Serial.println("[COLONY] ESP-NOW init failed"); return; }
  esp_now_register_recv_cb(onColonyRecv);
  ensureColonyPeer();
  Serial.printf("[COLONY] ESP-NOW ready id=%u channel=%u\n", colonyWorkerId, colonyPeerChannel);
#endif
}

void colonyTick() {
  if (WiFi.status() == WL_CONNECTED) ensureColonyPeer();
  if (millis() - colonyLastMasterMs > COLONY_MASTER_TIMEOUT_MS) {
    colonyMasterSeen = false;
    if (!colonyJob.active) strlcpy(colonyMode, "SEEK", sizeof(colonyMode));
  }
  runRemoteMiningBatch();
  if (millis() - colonyLastHeartbeatMs >= COLONY_HEARTBEAT_MS) { colonyLastHeartbeatMs = millis(); sendNodeHeartbeat(); }
  if (millis() - colonyLastEntropyMs >= COLONY_ENTROPY_MS) { colonyLastEntropyMs = millis(); sendNodeEntropy(); }
  kenshiBubbleTick();
  tachyonProphecyTick();
  eyeVisionTick();
}


extern "C" {
  #include "driver/i2s_std.h"
}

// ========================= JANUS BLIND EYE =========================

#define DEVICE_ID              "atom_s3r_blind_eye"
#define DEVICE_KIND            "blind_eye_rf_fusion_v2_11_ruview_lite_buzz_miner"

#define WIFI_SSID              "JANUS_WIFI_PLACEHOLDER"
#define WIFI_PASSWORD          "JANUS_NET_PLACEHOLDER"
#define SERVER_BASE            "http://192.168.1.92:5000"

#define EP_DEVICE_DATA         "/api/device/data"
#define EP_DEVICE_COMMAND      "/api/device/command/"

#define SERVER_URL             SERVER_BASE EP_DEVICE_DATA
#define COMMAND_URL_BASE       SERVER_BASE EP_DEVICE_COMMAND

#define SENSOR_INTERVAL_MS     100
#define SEND_INTERVAL_MS       2500
#define COMMAND_INTERVAL_MS    3000
#define JANUS_HTTP_LEGACY_ENABLE 0
#define HEADLESS_DEBUG_INTERVAL_MS 5000UL
#define SAVE_INTERVAL_MS       60000UL
#define WIFI_RETRY_MS          7000UL

// Grove TMOS
#ifndef GROVE_SDA_PIN
#define GROVE_SDA_PIN          2
#endif
#ifndef GROVE_SCL_PIN
#define GROVE_SCL_PIN          1
#endif

// Bottom mic base
#define MIC_I2S_BCLK_PIN       6
#define MIC_I2S_WS_PIN         5
#define MIC_I2S_DATA_PIN       7
#define MIC_SAMPLE_RATE        16000
#define MIC_FRAME_SAMPLES      192

#define FEATURE_DIM            10
#define HIST_SIZE              48

#define MODEL_FILE             "/eye_model.bin"
#define STATE_FILE             "/eye_state.json"

// ========================= GLOBALS =========================

M5_STHS34PF80 tmos;
bool tmos_ready = false;
bool calibrated = false;

int16_t raw_presence = 0;
int16_t raw_motion = 0;
int16_t calib_presence = 0;
int16_t calib_motion = 0;

float acc_x = 0, acc_y = 0, acc_z = 0;
float gyro_x = 0, gyro_y = 0, gyro_z = 0;
float mag_x = 0, mag_y = 0, mag_z = 0;
float imu_temp = 0;
float imu_shock = 0;
float mag_norm = 0;

float tmos_presence = 0;
float tmos_motion = 0;

// v2.9 Eagle Focus runtime state.
float tmos_presence_delta = 0.0f;
float tmos_motion_delta = 0.0f;
float tmos_presence_baseline = 0.0f;
float tmos_motion_baseline = 0.0f;
float tmos_presence_noise = 12.0f;
float tmos_motion_noise = 8.0f;
float tmos_focus_gain = 3.2f;
float tmos_focus_confidence = 0.0f;
bool tmos_focus_ready = false;
uint32_t tmos_last_focus_ms = 0;
float tmos_presence_memory = 0.0f;
float tmos_motion_memory = 0.0f;
float tmos_occupancy = 0.0f;
float tmos_ghost_score = 0.0f;
uint32_t tmos_last_valid_ms = 0;
uint16_t tmos_bad_frames = 0;
bool tmos_presence_now = false;
bool tmos_motion_now = false;
float mic_rms = 0;
int wifi_rssi = -127;

// v2.12 RuView-lite RF Fusion runtime state.
float rf_rssi_ema = -127.0f;
float rf_rssi_baseline = -127.0f;
float rf_rssi_noise = 2.8f;
float rf_abs_drift = 0.0f;
float rf_motion_energy = 0.0f;
float rf_presence_score = 0.0f;
float rf_entropy = 0.0f;
float rf_packet_pressure = 0.0f;
float rf_last_packet_drift = 0.0f;
int8_t rf_last_packet_rssi = -127;
bool rf_ready = false;
bool rf_presence_now = false;
bool rf_motion_now = false;
uint32_t rf_samples = 0;
uint32_t rf_rx_packets = 0;
uint32_t rf_last_sample_ms = 0;
uint32_t rf_last_packet_ms = 0;
uint32_t rf_anomaly_count = 0;
uint32_t rf_last_debug_ms = 0;

// v2.12 TMOS warmup / ghost damping / policy smoothing runtime state.
uint32_t janusEyeBootMs = 0;
uint32_t tmosWarmupUntilMs = 0;
uint32_t tmosGhostHighSinceMs = 0;
uint32_t tmosLastGhostTaskNeedMs = 0;
uint8_t janusPolicyCandidateMood = JM_IDLE;
uint8_t janusPolicyCandidateCount = 0;
uint8_t janusPolicyRawLastMood = JM_IDLE;
uint32_t janusPolicyLastMoodChangeMs = 0;
uint32_t janusPolicySmoothedDrops = 0;
uint32_t janusPolicyAcceptedChanges = 0;


float model_w[FEATURE_DIM] = {0.10f, -0.02f, 0.08f, 0.12f, 0.05f, 0.03f, 0.06f, 0.05f, 0.04f, -0.02f};
float model_b = 0.0f;
float model_lr = 0.0020f;

float pred_activity = 0;
float activity = 0;
float loss = 0;
float fit = 0;
float fit_best = -9999.0f;
float z_activity = 0;
float z_loss = 0;
float sync_hint = 0;

// v2.8 Tachyon Prophecy / Physarious micro movement state.
float tachyonPredPresence1 = 0.0f;
float tachyonPredMotion1 = 0.0f;
float tachyonPredPresence2 = 0.0f;
float tachyonPredMotion2 = 0.0f;
float tachyonPredPresence3 = 0.0f;
float tachyonPredMotion3 = 0.0f;
float tachyonLastPredPresence1 = 0.0f;
float tachyonLastPredMotion1 = 0.0f;
float tachyonPresenceConfidence = 0.15f;
float tachyonMotionConfidence = 0.15f;
float tachyonFutureStress = 0.0f;
float tachyonEventEtaMs = 9999.0f;
float tachyonLossEma = 0.0f;
float tachyonPhysarumTrace = 0.0f;
float tachyonLangerDrag = 0.0f;
float tachyonEnergy = 0.0f;
float tachyonSwarmPressure = 0.0f;
float tachyonRemotePresence = 0.0f;
float tachyonRemoteMotion = 0.0f;
float tachyonTrendGain = 0.72f;
float tachyonMemoryGain = 0.24f;
float tachyonRemoteGain = 0.18f;
float tachyonSeqPresence[JANUS_TACHYON_SEQ_N] = {0};
float tachyonSeqMotion[JANUS_TACHYON_SEQ_N] = {0};
float tachyonSeqActivity[JANUS_TACHYON_SEQ_N] = {0};
uint8_t tachyonSeqPos = 0;
uint8_t tachyonSeqCount = 0;
uint32_t tachyonSeq = 0;
uint32_t tachyonLastTxMs = 0;
uint32_t tachyonLastRxMs = 0;
uint32_t tachyonTxPackets = 0;
uint32_t tachyonRxPackets = 0;
uint8_t tachyonRemoteCount = 0;
JanusRemoteProphecyState tachyonRemotes[JANUS_TACHYON_REMOTE_N];

// Core2 "eye of eyes" E/C -> E/F frame stream state.
bool eyeVisionEnabled = false;
uint16_t eyeVisionFrameMs = JANUS_EYE_VISION_DEFAULT_FRAME_MS;
uint16_t eyeVisionSeq = 0;
uint32_t eyeVisionLastControlMs = 0;
uint32_t eyeVisionLastFrameMs = 0;
uint32_t eyeVisionFramesTx = 0;
uint32_t eyeVisionControlsRx = 0;
uint32_t eyeVisionControlsIgnored = 0;
char eyeVisionSource[16] = "-";


// JANUS BLACKBOARD runtime state.
uint32_t janusEventSeq = 0;
uint32_t janusEventLastTxMs = 0;
uint32_t janusEventLastMotionMs = 0;
uint32_t janusEventLastPresenceMs = 0;
uint32_t janusEventLastStatusMs = 0;
uint32_t janusPolicyRx = 0;
uint32_t janusPolicySeq = 0;
uint32_t janusPolicyUntilMs = 0;
uint32_t janusPolicyQuietUntilMs = 0;
uint8_t janusPolicyMood = JM_IDLE;
uint8_t janusPolicyRadioRate = 1;
uint8_t janusPolicySensorRate = 1;
uint8_t janusPolicyBuzzBudget = 1;
uint8_t janusPolicyConfidence = 0;
char janusPolicyOrder[40] = "-";
uint16_t janusPolicySensorIntervalMs = SENSOR_INTERVAL_MS;

// v2.10G Eye episodic memory / semantic task bridge / S/S telemetry.
JanusEyeEpisode janusEyeEpisodes[JANUS_EYE_EPISODE_COUNT];
uint8_t janusEyeEpisodeHead = 0;
uint8_t janusEyeEpisodeCount = 0;
uint32_t janusEyeLastEpisodeMs = 0;
uint32_t janusEyeLastAiMemoryMs = 0;
uint32_t janusEyeLastTaskNeedMs = 0;
uint32_t janusEyeLastTaskDoneMs = 0;
uint32_t janusEyeTaskNeedTx = 0;
uint32_t janusEyeTaskDoneTx = 0;
uint32_t janusEyeAiMemoryTx = 0;
uint32_t janusEyeSwarmSenseSeq = 0;
uint32_t janusEyeSwarmSenseTx = 0;
uint32_t janusEyeSwarmSenseFail = 0;
uint32_t janusEyeLastSwarmSenseMs = 0;
bool janusEyePrevPresenceNow = false;
bool janusEyePrevMotionNow = false;

// Atomic Motion Base v1.2 scaffold/runtime state.
bool motionBasePresent = false;
bool motionBasePowerPresent = false;
bool motionBaseArmed = false;       // Core policy may arm; writes still require compile flag.
bool motionBaseTrackEnabled = true; // planner is enabled, physical writes are safe-gated.
uint8_t motionBasePowerAddr = 0;
uint32_t motionBaseLastTickMs = 0;
uint32_t motionBaseLastPowerMs = 0;
uint32_t motionBaseLastStatusMs = 0;
uint32_t motionBaseServoWrites = 0;
uint32_t motionBaseI2cErrors = 0;
int16_t motionBaseBusMv = 0;
int16_t motionBaseCurrentRaw = 0;
int16_t motionBasePowerRaw = 0;
int16_t motionBaseServoAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
int16_t motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
int16_t motionBaseLastSentAngle = -1;
int8_t motionBaseMotorSpeed[2] = {0, 0};
uint8_t motionBaseBatteryPct = 0;
uint8_t motionBasePowerFlags = 0;
uint8_t motionBasePowerSource = 0;
uint16_t motionBaseLastCellMv = 0;       // last real 1S battery voltage seen before USB/boost rail
uint8_t motionBaseLastCellPct = 0;       // kept while USB-C is connected, so we do not fake 100%
uint32_t motionBaseExternalSinceMs = 0;
uint32_t motionBaseBatterySeq = 0;
uint32_t motionBaseLastBatteryTxMs = 0;
// v2.14D: Motion Base is optional. If absent, BlindEye becomes a normal sensor/swarm node. Serial S enters PASSIVE_EYE mode.
bool motionBaseOptionalAbsent = false;
bool motionBaseEverDetected = false;
uint32_t motionBaseAbsentSinceMs = 0;
// v2.13 RoboZombie crawler runtime.
bool roboZombieLocalArm = JANUS_ROBOZOMBIE_LOCAL_TEST_ARM != 0;
bool roboZombieCrawlerManualEnable = false;
bool roboZombiePassiveMode = false;   // Serial capital S: passive eye, no actuator writes until a/g/manual target re-enables
bool roboZombieLeftReverse = JANUS_ROBOZOMBIE_LEFT_REVERSE != 0;
bool roboZombieRightReverse = JANUS_ROBOZOMBIE_RIGHT_REVERSE != 0;
bool roboZombieHeadPresent = JANUS_ROBOZOMBIE_HEAD_PRESENT != 0;
bool roboZombieLeftLegPresent = JANUS_ROBOZOMBIE_LEFT_LEG_PRESENT != 0;
bool roboZombieRightLegPresent = JANUS_ROBOZOMBIE_RIGHT_LEG_PRESENT != 0;
uint8_t roboZombieBasePull = 30;
uint32_t roboZombieCrawlerPulses = 0;
uint32_t roboZombieCrawlerLastStopMs = 0;
uint32_t roboZombieCrawlerLastPulseMs = 0;
int8_t roboZombieLastLeftSpeed = 0;
int8_t roboZombieLastRightSpeed = 0;
uint8_t roboZombieLastLeftValue = JANUS_ROBOZOMBIE_SERVO_STOP;
uint8_t roboZombieLastRightValue = JANUS_ROBOZOMBIE_SERVO_STOP;
float roboZombieGaitConfidence = 0.0f;
uint32_t roboZombieLastConfidentMs = 0;
uint32_t roboZombieAutoBlockedLowPowerMs = 0;



float hist_activity[HIST_SIZE] = {0};
float hist_loss[HIST_SIZE] = {0};
int hist_count = 0;
int hist_pos = 0;

unsigned long lastSensorAt = 0;
unsigned long lastSendAt = 0;
unsigned long lastCmdAt = 0;
unsigned long lastDebugAt = 0;
unsigned long lastSaveAt = 0;
unsigned long lastWifiTry = 0;

String statusLine = "boot";
String diagLine = "init";

i2s_chan_handle_t rx_handle = nullptr;

// ========================= HELPERS =========================

String joinUrl(const String& base, const String& path) {
  if (base.endsWith("/") && path.startsWith("/")) return base.substring(0, base.length() - 1) + path;
  if (!base.endsWith("/") && !path.startsWith("/")) return base + "/" + path;
  return base + path;
}

bool initWiFi(bool force = false) {
  if (!force && WiFi.status() == WL_CONNECTED) return true;
  if (!force && millis() - lastWifiTry < WIFI_RETRY_MS) return false;

  lastWifiTry = millis();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long until = millis() + 5000;
  while (WiFi.status() != WL_CONNECTED && millis() < until) {
    delay(80);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool ensureWiFi() {
  return WiFi.status() == WL_CONNECTED ? true : initWiFi(false);
}

bool httpGet(const String& url, String& response, int timeoutMs = 450) {
  // JANUS v2.3: HTTP disabled. ESP-NOW colony only.
  return false;
}


bool httpPostJson(const String& url, const String& payload, String& response, int timeoutMs = 800) {
  // JANUS v2.2: HTTP removed. ESP-NOW only.
  return false;
}


// ========================= TMOS =========================

void initTMOS() {
  if (tmos.begin(&Wire, STHS34PF80_I2C_ADDRESS)) tmos_ready = true;
  else if (tmos.begin(&Wire, 0x5A)) tmos_ready = true;
  else if (tmos.begin(&Wire, 0x5B)) tmos_ready = true;
  else tmos_ready = false;
}

void calibrateTMOS() {
  if (!tmos_ready) return;

  int32_t sumP = 0;
  int32_t sumM = 0;
  int32_t minP = 32767, maxP = -32768;
  int32_t minM = 32767, maxM = -32768;
  const int samples = 48;

  // Keep the sensor still and preferably aim it at an empty/neutral part of the room
  // during this boot window. If the operator is inside the beam, Eagle Focus will
  // still recover slowly, but the first minute will be less sensitive.
  for (int i = 0; i < samples; ++i) {
    tmos.getPresenceValue(&raw_presence);
    tmos.getMotionValue(&raw_motion);
    sumP += raw_presence;
    sumM += raw_motion;
    minP = min<int32_t>(minP, raw_presence);
    maxP = max<int32_t>(maxP, raw_presence);
    minM = min<int32_t>(minM, raw_motion);
    maxM = max<int32_t>(maxM, raw_motion);
    delay(28);
  }

  calib_presence = sumP / samples;
  calib_motion = sumM / samples;
  tmos_presence_baseline = (float)calib_presence;
  tmos_motion_baseline = (float)calib_motion;
  tmos_presence_noise = constrain((float)(maxP - minP) * 0.55f + 8.0f, 6.0f, 80.0f);
  tmos_motion_noise = constrain((float)(maxM - minM) * 0.55f + 5.0f, 4.0f, 70.0f);
  tmos_focus_gain = 3.2f;
  tmos_focus_confidence = 0.0f;
  tmos_presence_memory = 0.0f;
  tmos_motion_memory = 0.0f;
  tmos_occupancy = 0.0f;
  tmos_ghost_score = 0.0f;
  tmos_presence_now = false;
  tmos_motion_now = false;
  tmos_last_focus_ms = 0;
  tmos_last_valid_ms = millis();
  tmos_focus_ready = true;
  calibrated = true;
  Serial.printf("[EYE/CAL] v2.9I truth base=%.1f/%.1f noise=%.1f/%.1f room-empty=%u\n",
                tmos_presence_baseline, tmos_motion_baseline,
                tmos_presence_noise, tmos_motion_noise,
                (unsigned)JANUS_EYE_RECALIBRATE_ON_BOOT);
}

void readTMOS() {
  if (!tmos_ready) {
    tmos_presence = 0.0f;
    tmos_motion = 0.0f;
    tmos_focus_confidence = 0.0f;
    tmos_presence_now = false;
    tmos_motion_now = false;
    tmos_occupancy *= JANUS_EYE_MEMORY_DECAY;
    tmos_presence_memory *= JANUS_EYE_MEMORY_DECAY;
    tmos_motion_memory *= JANUS_EYE_MEMORY_DECAY;
    return;
  }

  tmos.getPresenceValue(&raw_presence);
  tmos.getMotionValue(&raw_motion);

  // v2.9H: защита от залипших/мусорных raw вроде 16334/16334.
  // Важно: это НЕ включает TMOS насильно и НЕ перезапускает I2C.
  if (abs((int)raw_presence) >= JANUS_EYE_STUCK_RAW_ABS ||
      abs((int)raw_motion) >= JANUS_EYE_STUCK_RAW_ABS) {
    tmos_bad_frames++;
    tmos_presence = 0.0f;
    tmos_motion = 0.0f;
    tmos_focus_confidence *= 0.80f;
    tmos_presence_now = false;
    tmos_motion_now = false;
    tmos_occupancy *= JANUS_EYE_GHOST_DECAY;
    return;
  }
  tmos_last_valid_ms = millis();

  if (!calibrated) {
    tmos_presence = 0.0f;
    tmos_motion = 0.0f;
    tmos_focus_confidence = 0.0f;
    tmos_presence_now = false;
    tmos_motion_now = false;
    return;
  }

#if JANUS_EYE_EAGLE_FOCUS_ENABLE
  if (!tmos_focus_ready) {
    tmos_presence_baseline = (float)calib_presence;
    tmos_motion_baseline = (float)calib_motion;
    tmos_presence_noise = 12.0f;
    tmos_motion_noise = 8.0f;
    tmos_focus_gain = 3.2f;
    tmos_focus_ready = true;
  }

  float rawP = (float)raw_presence;
  float rawM = (float)raw_motion;
  float dP = rawP - tmos_presence_baseline;
  float dM = rawM - tmos_motion_baseline;
  float aP = fabsf(dP);
  float aM = fabsf(dM);

  // v2.12B soft calibration:
  // - during warmup, baseline is allowed to settle toward the actual room field;
  // - after warmup, hot targets still mostly freeze baseline, but extreme jumps
  //   get a tiny release valve so the sensor can recover from a bad boot sample.
  bool warmup = tmosWarmupActive(millis());
  float hotGate = max(16.0f, tmos_presence_noise * 2.3f + tmos_motion_noise * 1.4f);
  bool hugeJump = (max(aP, aM) > JANUS_TMOS_BASELINE_JUMP_LEVEL);
  bool hot = (max(aP, aM * 1.35f) > hotGate);
  float baseAlpha = 0.0f;
  if (warmup) {
    baseAlpha = hugeJump ? JANUS_TMOS_WARMUP_SETTLE_ALPHA : JANUS_TMOS_WARMUP_SOFT_ALPHA;
  } else {
    baseAlpha = hot ? (hugeJump ? JANUS_TMOS_POSTWARM_JUMP_ALPHA : JANUS_EYE_BASELINE_ALPHA_HOT)
                    : JANUS_EYE_BASELINE_ALPHA_QUIET;
  }
  tmos_presence_baseline = tmos_presence_baseline * (1.0f - baseAlpha) + rawP * baseAlpha;
  tmos_motion_baseline   = tmos_motion_baseline   * (1.0f - baseAlpha) + rawM * baseAlpha;

  // Recompute deltas after the baseline glide. This is the key anti-wild-calibration fix.
  dP = rawP - tmos_presence_baseline;
  dM = rawM - tmos_motion_baseline;
  aP = fabsf(dP);
  aM = fabsf(dM);

  // Noise follows quiet room breathing; warmup gets a softer, faster settle.
  float noiseAlpha = warmup ? JANUS_TMOS_WARMUP_NOISE_ALPHA : JANUS_EYE_NOISE_ALPHA;
  if (!hot || warmup) {
    tmos_presence_noise = tmos_presence_noise * (1.0f - noiseAlpha) + aP * noiseAlpha;
    tmos_motion_noise   = tmos_motion_noise   * (1.0f - noiseAlpha) + aM * noiseAlpha;
  } else {
    tmos_presence_noise = tmos_presence_noise * 0.999f + min(aP, tmos_presence_noise) * 0.001f;
    tmos_motion_noise   = tmos_motion_noise   * 0.999f + min(aM, tmos_motion_noise) * 0.001f;
  }
  tmos_presence_noise = constrain(tmos_presence_noise, 4.0f, 220.0f);
  tmos_motion_noise = constrain(tmos_motion_noise, 3.0f, 200.0f);

  float targetGain = constrain(9.0f - (tmos_presence_noise + tmos_motion_noise) * 0.035f,
                               JANUS_EYE_FOCUS_MIN_GAIN, warmup ? JANUS_TMOS_WARMUP_GAIN_MAX : JANUS_EYE_FOCUS_MAX_GAIN);
  if (hot && !warmup) targetGain = min(JANUS_EYE_FOCUS_MAX_GAIN, targetGain + 0.8f);
  float gainAlpha = warmup ? 0.045f : 0.14f;
  tmos_focus_gain = tmos_focus_gain * (1.0f - gainAlpha) + targetGain * gainAlpha;

  // v2.9I TRUTH RELEASE:
  // 1) positive/warm presence is trusted directly;
  // 2) negative/cool delta is allowed only through a gated, weak path;
  // 3) focus_confidence never creates NOW by itself, so ghosts can decay.
  float warmSignal = max(0.0f, dP - (tmos_presence_noise * 0.85f + 5.0f));
  float coolSignal = max(0.0f, -dP - (tmos_presence_noise * 2.60f + 12.0f));
  float mSignal = max(0.0f, aM - (tmos_motion_noise * 0.85f + 5.0f));
  float coolGate = constrain(mSignal / max(JANUS_EYE_MOTION_FLAG_LEVEL * 2.0f, 1.0f), 0.0f, 1.0f);
  float pSignal = warmSignal + coolSignal * coolGate * JANUS_EYE_COOL_PRESENCE_WEIGHT;
  tmos_presence_delta = dP;
  tmos_motion_delta = dM;

  // Keep far-field sensitivity, but do not let a stale baseline become permanent presence.
  if (warmup) {
    pSignal *= JANUS_TMOS_WARMUP_OUTPUT_SCALE;
    mSignal *= JANUS_TMOS_WARMUP_OUTPUT_SCALE;
  }
  tmos_presence = constrain(pSignal * 0.22f * tmos_focus_gain, 0.0f, 1400.0f);
  tmos_motion   = constrain(mSignal * 0.22f * tmos_focus_gain, 0.0f, 800.0f);

  float pNorm = tmos_presence / max(JANUS_EYE_PRESENCE_FLAG_LEVEL, 1.0f);
  float mNorm = tmos_motion / max(JANUS_EYE_MOTION_FLAG_LEVEL, 1.0f);
  float evidence = max(pNorm, mNorm);

  tmos_focus_confidence = constrain(tmos_focus_confidence * 0.82f + evidence * 0.18f, 0.0f, 2.0f);

  bool currentPresence = (pNorm > 1.10f) || (pNorm > 0.72f && mNorm > 0.65f);
  bool currentMotion = (mNorm > 1.18f);
  if (warmup) {
    // During warmup we still display soft evidence, but NOW flags require strong proof.
    currentPresence = (pNorm > 2.80f) || (pNorm > 1.80f && mNorm > 1.40f);
    currentMotion = (mNorm > 2.60f);
  }
  if (currentPresence || currentMotion) tmos_last_focus_ms = millis();

  // v2.9I: NOW is current evidence only. Confidence/memory are not allowed to hallucinate NOW.
  tmos_presence_now = currentPresence;
  tmos_motion_now = currentMotion;

  if (tmos_presence_now) tmos_presence_memory = tmos_presence_memory * (1.0f - JANUS_EYE_MEMORY_ATTACK) + min(2.0f, pNorm) * JANUS_EYE_MEMORY_ATTACK;
  else tmos_presence_memory *= JANUS_EYE_MEMORY_DECAY;

  if (tmos_motion_now) tmos_motion_memory = tmos_motion_memory * (1.0f - JANUS_EYE_MEMORY_ATTACK) + min(2.0f, mNorm) * JANUS_EYE_MEMORY_ATTACK;
  else tmos_motion_memory *= JANUS_EYE_MEMORY_DECAY;

  tmos_occupancy = constrain(max(tmos_presence_memory, tmos_motion_memory), 0.0f, 2.0f);

  if (!tmos_presence_now && !tmos_motion_now) {
    if (millis() - tmos_last_focus_ms > JANUS_EYE_STALE_RELEASE_MS) {
      tmos_focus_confidence *= 0.72f;
      tmos_occupancy *= JANUS_EYE_GHOST_DECAY;
      tmos_presence_memory *= JANUS_EYE_GHOST_DECAY;
      tmos_motion_memory *= JANUS_EYE_GHOST_DECAY;
      tmos_ghost_score = constrain(tmos_ghost_score * 0.88f + 0.12f, 0.0f, 1.0f);
    }
  } else {
    tmos_ghost_score *= 0.80f;
  }
#else
  tmos_presence = constrain((raw_presence - calib_presence) / 10.0f, 0.0f, 1000.0f);
  tmos_motion   = constrain((raw_motion - calib_motion) / 10.0f, 0.0f, 500.0f);
  tmos_presence_now = tmos_presence > JANUS_EYE_PRESENCE_FLAG_LEVEL;
  tmos_motion_now = tmos_motion > JANUS_EYE_MOTION_FLAG_LEVEL;
#endif
}

// ========================= MIC =========================

bool initMicI2S() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  if (i2s_new_channel(&chan_cfg, NULL, &rx_handle) != ESP_OK) return false;

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)MIC_I2S_BCLK_PIN,
      .ws = (gpio_num_t)MIC_I2S_WS_PIN,
      .dout = I2S_GPIO_UNUSED,
      .din = (gpio_num_t)MIC_I2S_DATA_PIN,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };

  if (i2s_channel_init_std_mode(rx_handle, &std_cfg) != ESP_OK) return false;
  return i2s_channel_enable(rx_handle) == ESP_OK;
}

float readMicRms() {
  if (!rx_handle) return 0.0f;

  int32_t samples[MIC_FRAME_SAMPLES];
  size_t bytesRead = 0;
  esp_err_t err = i2s_channel_read(rx_handle, samples, sizeof(samples), &bytesRead, 10);
  if (err != ESP_OK || bytesRead == 0) return 0.0f;

  int count = bytesRead / sizeof(int32_t);
  double sumSq = 0.0;
  for (int i = 0; i < count; ++i) {
    float v = (float)samples[i] / 2147483648.0f;
    sumSq += (double)v * (double)v;
  }
  return sqrt(sumSq / (double)count);
}

// ========================= IMU =========================

void initIMU() {
  M5.Imu.init();
  delay(100);
}

void readIMUClassic() {
  if (!M5.Imu.isEnabled()) {
    acc_x = acc_y = acc_z = 0;
    gyro_x = gyro_y = gyro_z = 0;
    mag_x = mag_y = mag_z = 0;
    imu_temp = 0;
    mag_norm = 0;
    imu_shock = 0;
    return;
  }

  M5.Imu.getAccel(&acc_x, &acc_y, &acc_z);
  M5.Imu.getGyro(&gyro_x, &gyro_y, &gyro_z);

  if (!M5.Imu.getMag(&mag_x, &mag_y, &mag_z)) {
    mag_x = mag_y = mag_z = 0;
  }

  if (!M5.Imu.getTemp(&imu_temp)) {
    imu_temp = 0;
  }

  imu_shock = fabsf(acc_x) + fabsf(acc_y) + fabsf(acc_z);
  mag_norm = sqrtf(mag_x * mag_x + mag_y * mag_y + mag_z * mag_z);
}

// ========================= MODEL =========================

void buildFeatures(float x[FEATURE_DIM]) {
  x[0] = constrain(tmos_presence / 1000.0f, 0.0f, 1.0f);
  x[1] = constrain(tmos_motion / 500.0f, 0.0f, 1.0f);
  x[2] = constrain(mic_rms * 10.0f, 0.0f, 4.0f);
  x[3] = constrain(mag_norm / 200.0f, 0.0f, 4.0f);
  x[4] = constrain(imu_shock / 5.0f, 0.0f, 4.0f);
  x[5] = constrain(((float)wifi_rssi / -100.0f) + rf_presence_score * 0.35f + rf_motion_energy * 0.020f, 0.0f, 2.5f);

  // Tachyon/Physarious micro-features: future pressure, movement viscosity,
  // remote prophecies and Markov sector confidence. These are bounded so old
  // bad frames cannot destroy the tiny linear model.
  x[6] = constrain(tachyonPredPresence1 / 1000.0f, 0.0f, 2.0f);
  x[7] = constrain(tachyonPredMotion1 / 500.0f, 0.0f, 2.0f);
  x[8] = constrain(tachyonSwarmPressure + kenshiConfidence * 0.25f + tmos_focus_confidence * 0.12f + rfLiteFusionScore() * 0.25f, 0.0f, 2.4f);
  x[9] = constrain(tachyonFutureStress, 0.0f, 2.0f);
}

float predict(const float x[FEATURE_DIM]) {
  float y = model_b;
  for (int i = 0; i < FEATURE_DIM; ++i) y += model_w[i] * x[i];
  return y;
}

float computeActivity() {
  return
    tmos_presence * 0.002f +
    tmos_motion * 0.004f +
    mic_rms * 20.0f +
    mag_norm * 0.010f +
    imu_shock * 0.20f +
    tmos_focus_confidence * 0.10f +
    rf_motion_energy * 0.10f +
    rf_presence_score * 0.35f;
}

void train(const float target, const float x[FEATURE_DIM]) {
  float pred = predict(x);
  float err = constrain(pred - target, -4.0f, 4.0f);

  // Physarious-style "blackhole guard": high future stress damps the step.
  float stable = 1.0f / (1.0f + tachyonFutureStress * 0.55f + tachyonLangerDrag * 0.35f);
  float lr = constrain(model_lr * stable, 0.00018f, 0.0060f);

  for (int i = 0; i < FEATURE_DIM; ++i) {
    float xi = constrain(x[i], -3.0f, 4.0f);
    model_w[i] -= lr * err * xi;
    model_w[i] = constrain(model_w[i], -3.0f, 3.0f);
  }
  model_b -= lr * err;
  model_b = constrain(model_b, -4.0f, 4.0f);

  loss = loss * 0.82f + fabsf(pred - target) * 0.18f;
}

float meanOf(float* arr, int n) {
  if (n <= 0) return 0.0f;
  float s = 0.0f;
  for (int i = 0; i < n; ++i) s += arr[i];
  return s / n;
}

float stdOf(float* arr, int n, float mean) {
  if (n <= 1) return 0.0f;
  float s = 0.0f;
  for (int i = 0; i < n; ++i) {
    float d = arr[i] - mean;
    s += d * d;
  }
  return sqrtf(s / (n - 1));
}

void pushHistory() {
  hist_activity[hist_pos] = activity;
  hist_loss[hist_pos] = loss;
  hist_pos = (hist_pos + 1) % HIST_SIZE;
  if (hist_count < HIST_SIZE) hist_count++;
}

void updateMiniGPT() {
  float x[FEATURE_DIM];
  buildFeatures(x);

  pred_activity = predict(x);
  activity = computeActivity();
  train(activity, x);

  fit = (1.0f / (1.0f + loss)) + min(2.0f, activity * 0.15f);
  if (fit > fit_best) fit_best = fit;

  if (loss > 0.12f) model_lr = min(0.006f, model_lr * 1.0008f);
  else model_lr = max(0.0003f, model_lr * 0.9992f);

  pushHistory();

  float meanA = meanOf(hist_activity, hist_count);
  float stdA = stdOf(hist_activity, hist_count, meanA);
  float meanL = meanOf(hist_loss, hist_count);
  float stdL = stdOf(hist_loss, hist_count, meanL);

  z_activity = (stdA > 1e-6f) ? (activity - meanA) / stdA : 0.0f;
  z_loss = (stdL > 1e-6f) ? (loss - meanL) / stdL : 0.0f;
  sync_hint = 1.0f / (1.0f + fabsf(pred_activity - activity) + loss);

  // v2.6: apply Buzz Agent hint here, where model globals are already declared.
  if (millis() - colonyAgentLastRewardMs < COLONY_AGENT_REWARD_VISIBLE_MS) {
    if (colonyAgentHint == 3) {
      model_lr = min(0.006f, model_lr * 1.0015f);
      sync_hint = max(sync_hint, 0.88f);
    } else if (colonyAgentHint == 2) {
      model_lr = max(0.0003f, model_lr * 0.9985f);
    }
  }

  bool imu_ok = M5.Imu.isEnabled() && (fabsf(acc_x) + fabsf(acc_y) + fabsf(acc_z) + fabsf(gyro_x) + fabsf(gyro_y) + fabsf(gyro_z)) > 0.01f;
  bool mag_ok = fabsf(mag_norm) > 0.01f;
  bool tmos_ok = tmos_ready;
  bool mic_ok = rx_handle != nullptr;

  if (!imu_ok) statusLine = "imu offline";
  else if (!tmos_ok) statusLine = "tmos missing";
  else if (millis() - colonyAgentLastRewardMs < COLONY_AGENT_REWARD_VISIBLE_MS) {
    if (colonyAgentLevel >= 3) statusLine = "agent golden";
    else if (colonyAgentLevel == 2) statusLine = "agent boost";
    else if (colonyAgentLevel == 1) statusLine = "agent praise";
    else statusLine = "agent observe";
  }
  else if (loss < 0.04f) statusLine = "eye guessed";
  else if (loss > 0.40f) statusLine = "eye training";
  else statusLine = "eye stable";

  diagLine =
    String(imu_ok ? "IMU" : "--") + " " +
    String(mag_ok ? "MAG" : "--") + " " +
    String(tmos_ok ? "TMOS" : "--") + " " +
    String(mic_ok ? "MIC" : "--");
}

// ========================= STORAGE =========================

struct EyeModelBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t dim;
  float w[FEATURE_DIM];
  float b;
  float lr;
};

void resetModelDefaults() {
  const float defaults[FEATURE_DIM] = {0.10f, -0.02f, 0.08f, 0.12f, 0.05f, 0.03f, 0.06f, 0.05f, 0.04f, -0.02f};
  memcpy(model_w, defaults, sizeof(model_w));
  model_b = 0.0f;
  model_lr = 0.0020f;
}

void saveModel() {
  LittleFS.remove(MODEL_FILE);
  File f = LittleFS.open(MODEL_FILE, "w");
  if (!f) return;
  EyeModelBlob b{};
  b.magic = 0x45594532UL; // EYE2
  b.version = 2;
  b.dim = FEATURE_DIM;
  memcpy(b.w, model_w, sizeof(model_w));
  b.b = model_b;
  b.lr = model_lr;
  f.write((uint8_t*)&b, sizeof(b));
  f.close();
}

void loadModel() {
  File f = LittleFS.open(MODEL_FILE, FILE_READ);
  if (!f) {
    resetModelDefaults();
    return;
  }

  size_t sz = f.size();
  if (sz == sizeof(EyeModelBlob)) {
    EyeModelBlob b{};
    size_t got = f.read((uint8_t*)&b, sizeof(b));
    f.close();
    if (got == sizeof(b) && b.magic == 0x45594532UL && b.version == 2 && b.dim == FEATURE_DIM) {
      memcpy(model_w, b.w, sizeof(model_w));
      model_b = b.b;
      model_lr = b.lr;
    } else {
      resetModelDefaults();
    }
  } else {
    // Legacy v2.6/v2.7 model: 6 weights + bias + lr. Preserve learned old core,
    // initialize the new tachyon features safely.
    float legacyW[6] = {0};
    float legacyB = 0.0f;
    float legacyLr = 0.0020f;
    f.read((uint8_t*)legacyW, sizeof(legacyW));
    f.read((uint8_t*)&legacyB, sizeof(legacyB));
    f.read((uint8_t*)&legacyLr, sizeof(legacyLr));
    f.close();
    resetModelDefaults();
    for (uint8_t i = 0; i < 6; ++i) model_w[i] = legacyW[i];
    model_b = legacyB;
    model_lr = legacyLr;
  }

  bool bad = !isfinite(model_b) || !isfinite(model_lr) || model_lr <= 0.0f || model_lr > 0.05f;
  for (uint8_t i = 0; i < FEATURE_DIM; ++i) if (!isfinite(model_w[i])) bad = true;
  if (bad) resetModelDefaults();
  model_lr = constrain(model_lr, 0.00018f, 0.0060f);
}

void saveState() {
  LittleFS.remove(STATE_FILE);
  File f = LittleFS.open(STATE_FILE, "w");
  if (!f) return;

  StaticJsonDocument<512> doc;
  doc["calibrated"] = calibrated;
  doc["fit_best"] = fit_best;
  doc["model_lr"] = model_lr;
  doc["calib_presence"] = calib_presence;
  doc["calib_motion"] = calib_motion;
  doc["tachyon_pg"] = tachyonPresenceConfidence;
  doc["tachyon_mg"] = tachyonMotionConfidence;
  doc["tachyon_tg"] = tachyonTrendGain;
  doc["tachyon_mg2"] = tachyonMemoryGain;
  doc["tachyon_rg"] = tachyonRemoteGain;
  serializeJson(doc, f);
  f.close();
}

void loadState() {
  File f = LittleFS.open(STATE_FILE, FILE_READ);
  if (!f) return;

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, f) != DeserializationError::Ok) {
    f.close();
    return;
  }
  f.close();

  calibrated = doc["calibrated"] | calibrated;
  fit_best = doc["fit_best"] | fit_best;
  model_lr = doc["model_lr"] | model_lr;
  calib_presence = doc["calib_presence"] | calib_presence;
  calib_motion = doc["calib_motion"] | calib_motion;
  tachyonPresenceConfidence = doc["tachyon_pg"] | tachyonPresenceConfidence;
  tachyonMotionConfidence = doc["tachyon_mg"] | tachyonMotionConfidence;
  tachyonTrendGain = constrain(doc["tachyon_tg"] | tachyonTrendGain, 0.10f, 1.50f);
  tachyonMemoryGain = constrain(doc["tachyon_mg2"] | tachyonMemoryGain, 0.05f, 1.00f);
  tachyonRemoteGain = constrain(doc["tachyon_rg"] | tachyonRemoteGain, 0.00f, 0.80f);
}

// ========================= IO =========================

String buildPayload() {
  StaticJsonDocument<1024> doc;
  doc["device_id"] = DEVICE_ID;
  JsonObject d = doc["data"].to<JsonObject>();

  d["kind"] = DEVICE_KIND;
  d["acc_x"] = acc_x;
  d["acc_y"] = acc_y;
  d["acc_z"] = acc_z;
  d["gyro_x"] = gyro_x;
  d["gyro_y"] = gyro_y;
  d["gyro_z"] = gyro_z;
  d["mag_x"] = mag_x;
  d["mag_y"] = mag_y;
  d["mag_z"] = mag_z;
  d["mag_norm"] = mag_norm;
  d["temp"] = imu_temp;
  d["shock"] = imu_shock;
  d["tmos_presence"] = tmos_presence;
  d["tmos_motion"] = tmos_motion;
  d["mic_rms"] = mic_rms;
  d["wifi_rssi"] = wifi_rssi;
  d["rf_ready"] = rf_ready;
  d["rf_presence_now"] = rf_presence_now;
  d["rf_motion_now"] = rf_motion_now;
  d["rf_presence_score"] = rf_presence_score;
  d["rf_motion_energy"] = rf_motion_energy;
  d["rf_abs_drift"] = rf_abs_drift;
  d["rf_noise"] = rf_rssi_noise;
  d["rf_entropy"] = rf_entropy;
  d["rf_packets"] = rf_rx_packets;
  d["calibrated"] = calibrated;
  d["uptime_ms"] = millis();
  d["activity"] = activity;
  d["pred_activity"] = pred_activity;
  d["loss"] = loss;
  d["fit"] = fit;
  d["fit_best"] = fit_best;
  d["model_lr"] = model_lr;
  d["z_activity"] = z_activity;
  d["z_loss"] = z_loss;
  d["sync_hint"] = sync_hint;
  d["tachyon_pred_presence_1"] = tachyonPredPresence1;
  d["tachyon_pred_motion_1"] = tachyonPredMotion1;
  d["tachyon_pred_presence_3"] = tachyonPredPresence3;
  d["tachyon_pred_motion_3"] = tachyonPredMotion3;
  d["tachyon_eta_ms"] = tachyonEventEtaMs;
  d["tachyon_conf_p"] = tachyonPresenceConfidence;
  d["tachyon_conf_m"] = tachyonMotionConfidence;
  d["tachyon_stress"] = tachyonFutureStress;
  d["tachyon_swarm_pressure"] = tachyonSwarmPressure;
  d["vision_enabled"] = eyeVisionEnabled;
  d["vision_frames"] = eyeVisionFramesTx;
  d["motion_base_present"] = motionBasePresent;
  d["motion_base_power"] = motionBasePowerPresent;
  d["motion_base_armed"] = motionBaseArmed;
  d["motion_base_servo_angle"] = motionBaseServoAngle;
  d["motion_base_target_angle"] = motionBaseTargetAngle;
  d["motion_base_bus_mv"] = motionBaseBusMv;
  d["motion_base_battery_pct"] = motionBaseBatteryPct;
  d["motion_base_power_flags"] = motionBasePowerFlags;
  d["motion_base_i2c_errors"] = motionBaseI2cErrors;
  d["janus_policy_mood"] = janusPolicyMood;
  d["janus_policy_order"] = janusPolicyOrder;
  d["status"] = statusLine;
  d["diag"] = diagLine;
  d["colony_mode"] = colonyMode;
  d["colony_hashrate"] = colonyRemoteHashrate;
  d["colony_tickets"] = colonyRemoteShares;
  d["agent_rewards"] = colonyAgentRewardsRx;
  d["agent_aok"] = colonyAgentShareRewardsRx;
  d["agent_points"] = colonyAgentRewardPoints;
  d["agent_level"] = colonyAgentLevel;
  d["agent_hint"] = colonyAgentHint;
  d["agent_score"] = colonyAgentScore;
  d["agent_pred_hash"] = colonyAgentPredictedHash;
  d["agent_pred_err"] = colonyAgentPredictionError;
  d["agent_batch"] = effectiveColonyRemoteBatch();
  d["kenshi_state"] = kenshiBubbleState;
  d["kenshi_job"] = kenshiJobState;
  d["kenshi_sector"] = kenshiSector;
  d["kenshi_next"] = kenshiPredSector;
  d["kenshi_priority"] = kenshiPriority;
  d["kenshi_conf"] = kenshiConfidence;
  d["kenshi_active"] = kenshiActiveNodes;
  d["kenshi_virtual"] = kenshiVirtualNodes;
  d["kenshi_rx"] = kenshiRxPackets;
  d["kenshi_tx"] = kenshiTxPackets;

  String out;
  serializeJson(doc, out);
  return out;
}

void sendTelemetry() {
  // JANUS v2.2: HTTP removed. ESP-NOW only.
}



void applyCommand(const String& cmd) {
  if (cmd == "CALIBRATE") calibrateTMOS();
  else if (cmd == "PING") sendTelemetry();
  else if (cmd == "REBOOT") ESP.restart();
}

void fetchCommand() {
  // JANUS v2.2: HTTP removed. ESP-NOW only.
}






// ========================= JANUS BLACKBOARD EVENT BUS + MOTION BASE =========================

uint16_t janusHash16(const char* s) {
  uint16_t h = 21661U;
  if (!s) return h;
  while (*s) {
    h ^= (uint8_t)*s++;
    h = (uint16_t)(h * 16719U);
  }
  return h ? h : 1;
}

uint16_t janusEyeCapabilities() {
  uint16_t caps = JC_IMU | JC_MIC | JC_TMOS | JC_VISION | JC_AI | JC_RF;
  if (motionBasePresent) caps |= JC_RELAY;     // actuator base attached
  if (motionBasePowerPresent) caps |= JC_BATTERY;
  return caps;
}

const char* janusEyeEventKind(uint8_t eventType) {
  switch (eventType) {
    case JE_BOOT: return "eye_boot";
    case JE_HEARTBEAT: return "eye_status";
    case JE_ENV: return "eye_env";
    case JE_MOTION: return "eye_motion";
    case JE_PRESENCE: return "eye_presence";
    case JE_WIFI_WEAK: return "eye_wifi_weak";
    case JE_LOW_HEAP: return "eye_low_heap";
    case JE_TASK_NEED: return "eye_task_need";
    case JE_TASK_DONE: return "eye_task_done";
    case JE_DANGER: return "eye_danger";
    case JE_SAFE: return "eye_safe";
    case JE_AI_MEMORY: return "eye_memory";
    default: return "eye_tmos_motion";
  }
}

bool janusEmitEyeEvent(uint8_t eventType, uint8_t confidence, uint8_t urgency,
                       int16_t a_x10, int16_t b_x10, int16_t c_x10, int16_t d_x10,
                       uint16_t topicHash, uint16_t objectHash, uint32_t ttlMs) {
#if JANUS_EVENT_BUS_ENABLE
  JanusEventPacket ev{};
  ev.magic[0] = 'J'; ev.magic[1] = 'E';
  ev.version = 1;
  ev.eventType = eventType;
  ev.nodeRole = JR_BLIND;
  ev.confidence = constrain((int)confidence, 0, 100);
  ev.urgency = constrain((int)urgency, 0, 100);
  strlcpy(ev.nodeId, "BlindEye", sizeof(ev.nodeId));
  strlcpy(ev.kind, janusEyeEventKind(eventType), sizeof(ev.kind));
  ev.seq = ++janusEventSeq;
  ev.uptimeMs = millis();
  ev.topicHash = topicHash ? topicHash : janusHash16("blind_eye");
  ev.objectHash = objectHash;
  ev.capabilities = janusEyeCapabilities();
  ev.valueA_x10 = a_x10;
  ev.valueB_x10 = b_x10;
  ev.valueC_x10 = c_x10;
  ev.valueD_x10 = d_x10;
  ev.eventHash = ((uint32_t)eventType << 24) ^ ((uint32_t)ev.topicHash << 8) ^ ev.seq ^ (uint32_t)colonyWorkerId;
  ev.ttlMs = ttlMs ? ttlMs : 7000UL;
  bool ok = janusEyeEspNowSend("J/E", &ev, sizeof(ev), true);
  if (ok) janusEventLastTxMs = millis();
  return ok;
#else
  (void)eventType; (void)confidence; (void)urgency; (void)a_x10; (void)b_x10; (void)c_x10; (void)d_x10; (void)topicHash; (void)objectHash; (void)ttlMs;
  return false;
#endif
}

const char* janusMoodName(uint8_t mood) {
  switch (mood) {
    case JM_IDLE: return "IDLE";
    case JM_QUIET: return "QUIET";
    case JM_ALERT: return "ALERT";
    case JM_EXPLORE: return "EXPLORE";
    case JM_GUARD: return "GUARD";
    case JM_RECOVER: return "RECOVER";
    default: return "?";
  }
}

uint8_t janusPolicySmoothMood(uint8_t rawMood, uint8_t confidence, uint32_t now) {
  if (rawMood > JM_RECOVER) rawMood = JM_IDLE;
  if (janusPolicyRx <= 1) {
    janusPolicyCandidateMood = rawMood;
    janusPolicyCandidateCount = 1;
    janusPolicyRawLastMood = rawMood;
    janusPolicyLastMoodChangeMs = now;
    return rawMood;
  }

  if (rawMood == janusPolicyMood) {
    janusPolicyCandidateMood = rawMood;
    janusPolicyCandidateCount = 0;
    return janusPolicyMood;
  }

  if (rawMood != janusPolicyCandidateMood) {
    janusPolicyCandidateMood = rawMood;
    janusPolicyCandidateCount = 1;
  } else if (janusPolicyCandidateCount < 255) {
    janusPolicyCandidateCount++;
  }
  janusPolicyRawLastMood = rawMood;

  uint8_t needed = 1;
  if (rawMood == JM_ALERT || rawMood == JM_GUARD) needed = JANUS_POLICY_ALERT_CONFIRM;
  else if (rawMood == JM_RECOVER || rawMood == JM_QUIET) needed = JANUS_POLICY_RECOVER_CONFIRM;

  bool dwellOk = (now - janusPolicyLastMoodChangeMs) >= JANUS_POLICY_SMOOTH_MIN_DWELL_MS;
  bool strongOverride = (confidence >= 70 && (rawMood == JM_ALERT || rawMood == JM_GUARD));
  if ((janusPolicyCandidateCount >= needed && dwellOk) || strongOverride) {
    janusPolicyLastMoodChangeMs = now;
    janusPolicyAcceptedChanges++;
    janusPolicyCandidateCount = 0;
    return rawMood;
  }

  janusPolicySmoothedDrops++;
  return janusPolicyMood;
}

void onJanusPolicyPacket(const JanusPolicyPacket& jp) {
#if JANUS_EVENT_BUS_ENABLE
  if (jp.magic[0] != 'J' || jp.magic[1] != 'P' || jp.version != 1) return;
  if (jp.seq && jp.seq == janusPolicySeq) return;
  janusPolicyRx++;
  janusPolicySeq = jp.seq;
  uint32_t now = millis();
  uint8_t rawMood = constrain((int)jp.swarmMood, 0, (int)JM_RECOVER);
  uint8_t smoothedMood = janusPolicySmoothMood(rawMood, jp.confidence, now);
  bool moodAccepted = (smoothedMood == rawMood);
  janusPolicyMood = smoothedMood;
  if (moodAccepted || jp.confidence >= 65 || janusPolicyRx < 3) {
    janusPolicyRadioRate = constrain((int)jp.radioRate, 0, 2);
    janusPolicySensorRate = constrain((int)jp.sensorRate, 0, 2);
  }
  janusPolicyBuzzBudget = constrain((int)jp.buzzBudget, 0, 3);
  janusPolicyConfidence = constrain((int)jp.confidence, 0, 100);
  janusPolicyUntilMs = now + (jp.ttlMs ? jp.ttlMs : JANUS_EVENT_POLICY_TTL_GUARD_MS);
  janusPolicyQuietUntilMs = jp.quietUntilMs
    ? now + min((uint32_t)jp.quietUntilMs, 60000UL)
    : 0;
  strlcpy(janusPolicyOrder, jp.order[0] ? jp.order : "-", sizeof(janusPolicyOrder));

  if (janusPolicySensorRate == 0) janusPolicySensorIntervalMs = 220;
  else if (janusPolicySensorRate == 2) janusPolicySensorIntervalMs = 70;
  else janusPolicySensorIntervalMs = SENSOR_INTERVAL_MS;

  // Core can arm future tracker only when explicitly confident. Compile-time write gate still wins.
  motionBaseArmed = motionBasePresent && !roboZombiePassiveMode && (janusPolicyMood == JM_GUARD || janusPolicyMood == JM_ALERT) && jp.confidence >= 55 && !(jp.flags & 0x0001);
  Serial.printf("[EYE/POLICY] rx=%lu raw=%s mood=%s radio=%u sensor=%u conf=%u armed=%u smoothDrop=%lu accept=%lu order=%s\n",
                (unsigned long)janusPolicyRx, janusMoodName(rawMood), janusMoodName(janusPolicyMood),
                (unsigned)janusPolicyRadioRate, (unsigned)janusPolicySensorRate,
                (unsigned)janusPolicyConfidence, motionBaseArmed ? 1 : 0,
                (unsigned long)janusPolicySmoothedDrops, (unsigned long)janusPolicyAcceptedChanges, janusPolicyOrder);
#endif
}

uint32_t janusEventIntervalNow() {
  if (tmos_motion_now || tmos_presence_now || tachyonFutureStress > 0.90f || kenshiBubbleState >= 2) return JANUS_EVENT_TX_ALERT_MS;
  if (janusPolicyRadioRate == 0 || (janusPolicyQuietUntilMs && millis() < janusPolicyQuietUntilMs)) return JANUS_EVENT_TX_BASE_MS * 2UL;
  if (janusPolicyRadioRate == 2) {
    uint32_t fast = JANUS_EVENT_TX_BASE_MS / 2UL;
    return fast < 900UL ? 900UL : fast;
  }
  return JANUS_EVENT_TX_BASE_MS;
}

void janusEventTick(bool force) {
#if JANUS_EVENT_BUS_ENABLE
  uint32_t now = millis();

  if (force || now - janusEventLastStatusMs >= janusEventIntervalNow()) {
    janusEventLastStatusMs = now;
    uint8_t conf = (uint8_t)constrain((int)((tachyonPresenceConfidence * 0.5f + tachyonMotionConfidence * 0.5f) * 100.0f), 10, 100);
    janusEmitEyeEvent(JE_HEARTBEAT, conf, kenshiPriority,
                      (int16_t)constrain((int)(tmos_presence * 10.0f), -32768, 32767),
                      (int16_t)constrain((int)(tmos_motion * 10.0f), -32768, 32767),
                      (int16_t)constrain((int)(tachyonFutureStress * 100.0f), -32768, 32767),
                      (int16_t)constrain((int)(motionBaseServoAngle * 10), -32768, 32767),
                      janusHash16("home_eye"), janusHash16("blind_eye"), 8000UL);
  }

  if (tmos_motion_now && now - janusEventLastMotionMs >= JANUS_EVENT_MOTION_COOLDOWN_MS) {
    janusEventLastMotionMs = now;
    janusEmitEyeEvent(JE_MOTION, (uint8_t)constrain((int)(tachyonMotionConfidence * 100.0f), 30, 100), 86,
                      (int16_t)constrain((int)(tmos_motion * 10.0f), -32768, 32767),
                      (int16_t)constrain((int)(tachyonPredMotion1 * 10.0f), -32768, 32767),
                      (int16_t)motionBaseTargetAngle,
                      (int16_t)kenshiPredSector,
                      janusHash16("motion"), janusHash16("blind_eye_tmos"), 4500UL);
  }

  if (tmos_presence_now && now - janusEventLastPresenceMs >= JANUS_EVENT_MOTION_COOLDOWN_MS) {
    janusEventLastPresenceMs = now;
    janusEmitEyeEvent(JE_PRESENCE, (uint8_t)constrain((int)(tachyonPresenceConfidence * 100.0f), 30, 100), 78,
                      (int16_t)constrain((int)(tmos_presence * 10.0f), -32768, 32767),
                      (int16_t)constrain((int)(tachyonPredPresence1 * 10.0f), -32768, 32767),
                      (int16_t)motionBaseTargetAngle,
                      (int16_t)kenshiSector,
                      janusHash16("presence"), janusHash16("blind_eye_tmos"), 4500UL);
  }

  if (wifi_rssi != -127 && wifi_rssi < -74) {
    static uint32_t lastWeak = 0;
    if (now - lastWeak > 12000UL) {
      lastWeak = now;
      janusEmitEyeEvent(JE_WIFI_WEAK, 80, 52, (int16_t)(wifi_rssi * 10), 0, 0, 0,
                        janusHash16("radio"), janusHash16("blind_eye_wifi"), 10000UL);
    }
  }

  if (rf_ready && (rf_motion_now || rf_presence_now)) {
    static uint32_t lastRfEvent = 0;
    if (now - lastRfEvent > 1800UL) {
      lastRfEvent = now;
      uint8_t rfConf = (uint8_t)constrain((int)(rf_presence_score * 64.0f + rf_motion_energy * 4.0f), 25, 100);
      uint8_t rfUrg = rf_motion_now ? 70 : 48;
      janusEmitEyeEvent(rf_motion_now ? JE_MOTION : JE_PRESENCE, rfConf, rfUrg,
                        (int16_t)constrain((int)(rf_presence_score * 1000.0f), -32768, 32767),
                        (int16_t)constrain((int)(rf_motion_energy * 100.0f), -32768, 32767),
                        (int16_t)constrain((int)(rf_abs_drift * 100.0f), -32768, 32767),
                        (int16_t)constrain((int)(rf_entropy * 100.0f), -32768, 32767),
                        janusHash16("rf_eye"), janusHash16("blind_eye_ruview_lite"), 5000UL);
    }
  }

  if (ESP.getFreeHeap() < 65000) {
    static uint32_t lastHeap = 0;
    if (now - lastHeap > 15000UL) {
      lastHeap = now;
      janusEmitEyeEvent(JE_LOW_HEAP, 75, 55, (int16_t)(ESP.getFreeHeap() / 1024), 0, 0, 0,
                        janusHash16("heap"), janusHash16("blind_eye_heap"), 10000UL);
    }
  }

  janusEyeSemanticTick(now, force);
#endif
}

void janusEyeRecordEpisode(uint8_t eventType, uint8_t confidence, uint8_t urgency, uint8_t flags) {
#if JANUS_EYE_EPISODE_ENABLE
  uint32_t now = millis();
  JanusEyeEpisode& ep = janusEyeEpisodes[janusEyeEpisodeHead];
  ep.atMs = now;
  ep.eventType = eventType;
  ep.confidence = constrain((int)confidence, 0, 100);
  ep.urgency = constrain((int)urgency, 0, 100);
  ep.sector = kenshiSector;
  ep.predictedSector = kenshiPredSector;
  ep.flags = flags;
  ep.presence_x10 = (int16_t)constrain((int)(tmos_presence * 10.0f), -32768, 32767);
  ep.motion_x10 = (int16_t)constrain((int)(tmos_motion * 10.0f), -32768, 32767);
  ep.futureStress_x100 = (int16_t)constrain((int)(tachyonFutureStress * 100.0f), -32768, 32767);
  ep.servoAngle_x10 = (int16_t)constrain((int)(motionBaseServoAngle * 10), -32768, 32767);
  janusEyeEpisodeHead = (janusEyeEpisodeHead + 1) % JANUS_EYE_EPISODE_COUNT;
  if (janusEyeEpisodeCount < JANUS_EYE_EPISODE_COUNT) janusEyeEpisodeCount++;
  janusEyeLastEpisodeMs = now;
#else
  (void)eventType; (void)confidence; (void)urgency; (void)flags;
#endif
}

const JanusEyeEpisode* janusEyeLatestEpisode() {
#if JANUS_EYE_EPISODE_ENABLE
  if (janusEyeEpisodeCount == 0) return nullptr;
  uint8_t idx = (janusEyeEpisodeHead == 0) ? (JANUS_EYE_EPISODE_COUNT - 1) : (janusEyeEpisodeHead - 1);
  return &janusEyeEpisodes[idx];
#else
  return nullptr;
#endif
}

void janusEyeEmitAiMemory(uint32_t now, bool force) {
#if JANUS_EVENT_BUS_ENABLE && JANUS_EYE_EPISODE_ENABLE
  if (!force && now - janusEyeLastAiMemoryMs < JANUS_EYE_AI_MEMORY_TX_MS) return;
  const JanusEyeEpisode* ep = janusEyeLatestEpisode();
  if (!ep) return;
  janusEyeLastAiMemoryMs = now;
  bool ok = janusEmitEyeEvent(JE_AI_MEMORY, ep->confidence, ep->urgency,
                              ep->presence_x10, ep->motion_x10, ep->futureStress_x100,
                              (int16_t)(((uint16_t)ep->sector << 8) | ep->predictedSector),
                              janusHash16("eye_memory"), janusHash16("last_episode"), 60000UL);
  if (ok) janusEyeAiMemoryTx++;
#else
  (void)now; (void)force;
#endif
}

void janusEyeEmitTaskNeed(uint8_t urgency, const char* object, int16_t a, int16_t b, int16_t c, int16_t d) {
#if JANUS_EVENT_BUS_ENABLE
  bool ok = janusEmitEyeEvent(JE_TASK_NEED, 82, urgency, a, b, c, d,
                              janusHash16("eye_task_need"), janusHash16(object ? object : "eye_need"), 30000UL);
  if (ok) janusEyeTaskNeedTx++;
#else
  (void)urgency; (void)object; (void)a; (void)b; (void)c; (void)d;
#endif
}

void janusEyeEmitTaskDone(uint8_t confidence, const char* object, int16_t a, int16_t b, int16_t c, int16_t d) {
#if JANUS_EVENT_BUS_ENABLE
  bool ok = janusEmitEyeEvent(JE_TASK_DONE, confidence, 22, a, b, c, d,
                              janusHash16("eye_task_done"), janusHash16(object ? object : "eye_done"), 20000UL);
  if (ok) janusEyeTaskDoneTx++;
#else
  (void)confidence; (void)object; (void)a; (void)b; (void)c; (void)d;
#endif
}

void janusEyeSemanticTick(uint32_t now, bool force) {
#if JANUS_EVENT_BUS_ENABLE
  bool warmup = tmosWarmupActive(now);
  bool currentHot = !warmup && (tmos_presence_now || tmos_motion_now);
  bool wasHot = !warmup && (janusEyePrevPresenceNow || janusEyePrevMotionNow);

  if (!warmup && (tmos_motion_now || tmos_presence_now) &&
      (force || now - janusEyeLastEpisodeMs >= JANUS_EYE_EPISODE_RECORD_MS)) {
    uint8_t eventType = tmos_motion_now ? JE_MOTION : JE_PRESENCE;
    uint8_t conf = (uint8_t)constrain((int)((tachyonPresenceConfidence * 0.5f + tachyonMotionConfidence * 0.5f) * 100.0f), 30, 100);
    uint8_t urg = tmos_motion_now ? 86 : 78;
    uint8_t flags = 0;
    if (tmos_presence_now) flags |= 0x01;
    if (tmos_motion_now) flags |= 0x02;
    if (motionBasePresent) flags |= 0x04;
    if (motionBasePowerPresent) flags |= 0x08;
    if (rf_presence_now || rf_motion_now) flags |= 0x10;
    janusEyeRecordEpisode(eventType, conf, urg, flags);
  }

  if (wasHot && !currentHot && now - janusEyeLastTaskDoneMs >= JANUS_EYE_TASK_DONE_MS) {
    janusEyeLastTaskDoneMs = now;
    janusEyeEmitTaskDone(76, "eye_area_clear",
                         (int16_t)constrain((int)(tmos_occupancy * 100.0f), -32768, 32767),
                         (int16_t)constrain((int)(tmos_ghost_score * 100.0f), -32768, 32767),
                         (int16_t)kenshiSector, (int16_t)kenshiPredSector);
  }

  bool ghostHigh = tmos_ghost_score >= JANUS_GHOST_TASKNEED_LEVEL;
  if (ghostHigh && !tmosGhostHighSinceMs) tmosGhostHighSinceMs = now;
  if (!ghostHigh) tmosGhostHighSinceMs = 0;

  if (!warmup && (force || now - janusEyeLastTaskNeedMs >= JANUS_EYE_TASK_NEED_MS)) {
    bool emittedNeed = false;
    bool ghostNeedAllowed = (tmos_bad_frames >= JANUS_EYE_RECALIBRATE_BAD_FRAMES) ||
      (ghostHigh && tmosGhostHighSinceMs &&
       now - tmosGhostHighSinceMs >= JANUS_GHOST_TASKNEED_HOLD_MS &&
       now - tmosLastGhostTaskNeedMs >= JANUS_GHOST_TASKNEED_COOLDOWN_MS);
    if (ghostNeedAllowed) {
      janusEyeLastTaskNeedMs = now;
      tmosLastGhostTaskNeedMs = now;
      emittedNeed = true;
      janusEyeEmitTaskNeed(82, "eye_needs_recalibration",
                           (int16_t)tmos_bad_frames,
                           (int16_t)constrain((int)(tmos_ghost_score * 100.0f), 0, 32767),
                           (int16_t)constrain((int)(tmos_focus_confidence * 100.0f), 0, 32767),
                           (int16_t)constrain((int)(tmos_occupancy * 100.0f), 0, 32767));
    } else if (tachyonFutureStress >= JANUS_EYE_QUIET_STRESS_LEVEL && !currentHot) {
      janusEyeLastTaskNeedMs = now;
      emittedNeed = true;
      janusEyeEmitTaskNeed(66, "eye_needs_quiet",
                           (int16_t)constrain((int)(tachyonFutureStress * 100.0f), 0, 32767),
                           (int16_t)constrain((int)(loss * 1000.0f), 0, 32767),
                           (int16_t)kenshiSector, (int16_t)kenshiPredSector);
    } else if (motionBasePresent && motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV && !(motionBasePowerFlags & 0x04)) {
      janusEyeLastTaskNeedMs = now;
      emittedNeed = true;
      janusEyeEmitTaskNeed(78, "motionbase_power_low", motionBaseBusMv, motionBaseCurrentRaw, motionBasePowerRaw, motionBaseServoAngle);
    } else if (motionBasePresent && !motionBasePowerPresent) {
      janusEyeLastTaskNeedMs = now;
      emittedNeed = true;
      janusEyeEmitTaskNeed(42, "motionbase_needs_power_monitor",
                           (int16_t)motionBasePresent, (int16_t)motionBasePowerPresent, (int16_t)motionBaseI2cErrors, 0);
    } else if ((motionBasePresent || motionBaseEverDetected) && motionBaseI2cErrors > 20) {
      janusEyeLastTaskNeedMs = now;
      emittedNeed = true;
      janusEyeEmitTaskNeed(62, "motionbase_i2c_errors",
                           (int16_t)constrain((int)motionBaseI2cErrors, 0, 32767), motionBaseBusMv, motionBaseCurrentRaw, 0);
    }
    (void)emittedNeed;
  }

  janusEyeEmitAiMemory(now, force);
  janusEyePrevPresenceNow = tmos_presence_now;
  janusEyePrevMotionNow = tmos_motion_now;
#else
  (void)now; (void)force;
#endif
}

void janusEyeSwarmSenseTick(uint32_t now, bool force) {
#if JANUS_EYE_SWARMSENSE_ENABLE
  uint32_t interval = (tmos_motion_now || tmos_presence_now || rf_motion_now || rf_presence_now || tachyonFutureStress > 0.90f || kenshiPriority >= 120)
                      ? JANUS_EYE_SWARMSENSE_ALERT_MS
                      : JANUS_EYE_SWARMSENSE_TX_MS;
  if (!force && now - janusEyeLastSwarmSenseMs < interval) return;
  janusEyeLastSwarmSenseMs = now;

  SwarmSensePacket ss{};
  ss.magic[0] = 'S'; ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = colonyWorkerId;
  strlcpy(ss.nodeId, "BlindEye", sizeof(ss.nodeId));
  strlcpy(ss.kind, "eye_rf_fusion", sizeof(ss.kind));
  ss.seq = ++janusEyeSwarmSenseSeq;
  ss.uptime_ms = now;
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = (uint16_t)constrain((int32_t)(now - lastSensorAt), 0L, 65535L);
  ss.loop_max_us = (uint16_t)constrain((int32_t)janusPolicySensorIntervalMs, 0L, 65535L);
  ss.rssi = (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : -127;
  ss.radio_mode = janusPolicyRadioRate;
  ss.bt_flags = 0;
  if (janusPolicyRx && now < janusPolicyUntilMs) ss.bt_flags |= 0x01;
  if (tmos_presence_now) ss.bt_flags |= 0x02;
  if (tmos_motion_now) ss.bt_flags |= 0x04;
  if (motionBasePresent) ss.bt_flags |= 0x08;
  if (motionBasePowerPresent) ss.bt_flags |= 0x10;
  if (motionBasePowerPresent && motionBaseBatteryPct <= 20) ss.bt_flags |= 0x80;
  if (motionBaseArmed) ss.bt_flags |= 0x20;
  if (eyeVisionEnabled || rf_ready) ss.bt_flags |= 0x40;
  if (tmosWarmupActive(now)) ss.bt_flags |= 0x04;
  if ((!tmosWarmupActive(now) && tmos_ghost_score > 0.70f) || rf_entropy > JANUS_RF_LITE_ANOMALY_LEVEL) ss.bt_flags |= 0x80;
  ss.palette = rf_ready ? 2 : (eyeVisionEnabled ? 1 : 0);
  ss.knn_label = kenshiSector;
  ss.knn_confidence = (uint8_t)constrain((int)((kenshiConfidence * 0.72f + rf_presence_score * 0.28f) * 100.0f), 0, 100);
  ss.ai_hint = (tachyonFutureStress > 0.90f || kenshiPriority > 140 || rf_entropy > JANUS_RF_LITE_ANOMALY_LEVEL) ? 3 : ((loss > 0.25f) ? 2 : 1);
  ss.thermal_load = (uint8_t)constrain((int)roundf(imu_temp), 0, 100);
  ss.effective_batch = effectiveColonyRemoteBatch();
  ss.dynamic_batch = effectiveColonyRemoteBatch();
  ss.hash_rate = colonyRemoteHashrate;
  ss.total_hashes = colonyJob.active ? colonyJob.hashesDone : colonyJobsDone;
  ss.best_bits = colonyBestBits;
  ss.hash_eff_x1000 = (uint16_t)constrain((int32_t)(sync_hint * 1000.0f), 0L, 65535L);
  ss.prediction_error_x1000 = (int16_t)constrain((int32_t)((loss + rf_abs_drift * 0.015f) * 1000.0f), -32768L, 32767L);
  ss.entropy_x1000 = (uint16_t)constrain((int32_t)(eyeLocalEntropy() * 100.0f), 0L, 65535L);
  ss.touch_delta = (uint16_t)constrain((int32_t)max(max(tmos_presence, tmos_motion), rf_presence_score * 100.0f + rf_motion_energy * 12.0f), 0L, 65535L);
  ss.job_age_s = colonyJob.active ? (uint16_t)min(65535UL, (now - colonyJob.receivedAt) / 1000UL) : 65535U;
  ss.nonce_remaining_l16 = (colonyJob.active && colonyJob.rangeSize > colonyJob.hashesDone) ? (uint16_t)((colonyJob.rangeSize - colonyJob.hashesDone) & 0xFFFF) : 0;
  ss.flags = ((uint16_t)(motionBasePresent ? 1 : 0) << 15) |
             ((uint16_t)(motionBasePowerPresent ? 1 : 0) << 14) |
             ((uint16_t)(eyeVisionEnabled ? 1 : 0) << 13) |
             ((uint16_t)kenshiJobState << 8) |
             (uint16_t)(kenshiPredSector & 0xFF);

  if (janusEyeEspNowSend("S/S", &ss, sizeof(ss), true)) janusEyeSwarmSenseTx++;
  else janusEyeSwarmSenseFail++;
#else
  (void)now; (void)force;
#endif
}

bool motionI2cWrite8(uint8_t addr, uint8_t reg, uint8_t value) {
  if (!janusSelectMotionBus()) { motionBaseI2cErrors++; return false; }
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  uint8_t err = Wire.endTransmission();
  if (err) motionBaseI2cErrors++;
  janusSelectGroveBus();
  return err == 0;
}

bool motionI2cRead16(uint8_t addr, uint8_t reg, uint16_t& value) {
  if (!janusSelectMotionBus()) { motionBaseI2cErrors++; return false; }
  Wire.beginTransmission(addr);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);
  if (err) { motionBaseI2cErrors++; janusSelectGroveBus(); return false; }
  uint8_t got = Wire.requestFrom((int)addr, 2);
  if (got != 2) { motionBaseI2cErrors++; janusSelectGroveBus(); return false; }
  value = ((uint16_t)Wire.read() << 8) | Wire.read();
  janusSelectGroveBus();
  return true;
}

bool motionI2cProbeOnSelectedBus(uint8_t addr) {
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission();
  return err == 0;
}

bool motionI2cProbe(uint8_t addr) {
  if (!janusSelectMotionBus()) { motionBaseI2cErrors++; return false; }
  bool ok = motionI2cProbeOnSelectedBus(addr);
  janusSelectGroveBus();
  return ok;
}

void motionBaseScanCurrentBus(const char* name) {
  Serial.printf("[MOTION/I2C] scan %s:", name ? name : "bus");
  bool any = false;
  for (uint8_t a = 0x08; a <= 0x77; ++a) {
    if (motionI2cProbeOnSelectedBus(a)) {
      Serial.printf(" 0x%02X", (unsigned)a);
      any = true;
    }
    delay(1);
  }
  if (!any) Serial.print(" none");
  Serial.println();
}

void motionBaseScanBuses() {
  if (janusSelectMotionBus(true)) motionBaseScanCurrentBus("MotionBase SDA38/SCL39");
  if (janusSelectGroveBus(true)) motionBaseScanCurrentBus("Grove/TMOS SDA2/SCL1");
}


bool motionBasePowerOkForActuators() {
#if JANUS_MOTION_BASE_ENABLE
  if (!motionBasePowerPresent) return true;
  if (motionBaseBusMv <= 0) return true;
  if (motionBasePowerFlags & 0x04) return true; // USB/external rail present
  return motionBaseBusMv >= JANUS_MOTION_BASE_SLEEP_MV;
#else
  return false;
#endif
}

bool motionBaseWriteServoAngle(uint8_t ch, uint8_t angle, const char* tag) {
#if JANUS_MOTION_BASE_ENABLE && JANUS_MOTION_BASE_WRITE_ENABLE
  if (!motionBasePresent) return false;
  if (!motionBasePowerOkForActuators()) return false;
  uint8_t reg = 0x00 + (ch & 0x03);
  bool ok = motionI2cWrite8(JANUS_MOTION_BASE_I2C_ADDR, reg, (uint8_t)constrain((int)angle, 0, 180));
  if (ok) motionBaseServoWrites++;
  else Serial.printf("[ROBOZOMBIE] servo write FAIL tag=%s ch=S%u val=%u i2cErr=%lu\n",
                     tag ? tag : "servo", (unsigned)((ch & 0x03) + 1), (unsigned)angle,
                     (unsigned long)motionBaseI2cErrors);
  return ok;
#else
  (void)ch; (void)angle; (void)tag;
  return false;
#endif
}

bool roboZombieAnyLegPresent() {
#if JANUS_ROBOZOMBIE_ENABLE
  return roboZombieLeftLegPresent || roboZombieRightLegPresent;
#else
  return false;
#endif
}

const char* roboZombieBodyModeName() {
#if JANUS_ROBOZOMBIE_ENABLE
  // v2.14C: configured servos are not enough. If the ATOMIC Motion Base is absent,
  // the eye is intentionally a sensor-only node and must not pretend to be FULL.
  if (!motionBasePresent) return "BASELESS_SENSOR";
  if (roboZombieHeadPresent && roboZombieAnyLegPresent()) return "FULL";
  if (roboZombieHeadPresent && !roboZombieAnyLegPresent()) return "HEAD_ONLY";
  if (!roboZombieHeadPresent && roboZombieAnyLegPresent()) return "CRAWLER_ONLY";
  return "SENSOR_ONLY";
#else
  return "OFF";
#endif
}

uint8_t roboZombieSpeedToServoValue(int8_t speed, bool reverse) {
  speed = (int8_t)constrain((int)speed, -100, 100);
  if (reverse) speed = (int8_t)-speed;
  int delta = (int)roundf((float)speed * (float)JANUS_ROBOZOMBIE_MAX_PULL_DELTA / 100.0f);
  return (uint8_t)constrain(JANUS_ROBOZOMBIE_SERVO_STOP + delta, 0, 180);
}

float roboZombieComputeConfidence() {
#if JANUS_ROBOZOMBIE_ENABLE
  float tmos = constrain(max(tmos_presence_memory, tmos_motion_memory), 0.0f, 2.0f);
  float rf = rf_ready ? constrain(rfLiteFusionScore() / 1.8f, 0.0f, 1.8f) : 0.0f;
  float tach = constrain(tachyonFutureStress * 0.65f + tachyonPredMotion1 / 95.0f, 0.0f, 1.6f);
  float bubble = constrain(kenshiConfidence + (kenshiBubbleState >= 2 ? 0.35f : 0.0f), 0.0f, 1.4f);
  float nowHit = (tmos_motion_now || tmos_presence_now || rf_motion_now || rf_presence_now) ? 0.45f : 0.0f;
  float raw = tmos * 0.35f + rf * 0.28f + tach * 0.22f + bubble * 0.15f + nowHit;
  roboZombieGaitConfidence = constrain(roboZombieGaitConfidence * 0.78f + raw * 0.22f, 0.0f, 2.4f);
  if (roboZombieGaitConfidence > 0.55f) roboZombieLastConfidentMs = millis();
  return roboZombieGaitConfidence;
#else
  return 0.0f;
#endif
}

void motionBaseStopCrawler(const char* reason) {
#if JANUS_ROBOZOMBIE_ENABLE
  roboZombieLastLeftSpeed = 0;
  roboZombieLastRightSpeed = 0;
  roboZombieLastLeftValue = JANUS_ROBOZOMBIE_SERVO_STOP;
  roboZombieLastRightValue = JANUS_ROBOZOMBIE_SERVO_STOP;
#if JANUS_MOTION_BASE_WRITE_ENABLE
  if (motionBasePresent && motionBasePowerOkForActuators()) {
    if (roboZombieLeftLegPresent) motionBaseWriteServoAngle(JANUS_ROBOZOMBIE_LEFT_SERVO_CH, JANUS_ROBOZOMBIE_SERVO_STOP, "crawl-stop-L");
    if (roboZombieRightLegPresent) motionBaseWriteServoAngle(JANUS_ROBOZOMBIE_RIGHT_SERVO_CH, JANUS_ROBOZOMBIE_SERVO_STOP, "crawl-stop-R");
  }
#endif
  roboZombieCrawlerLastStopMs = millis();
  if (reason && reason[0] && (motionBasePresent || !motionBaseOptionalAbsent)) Serial.printf("[ROBOZOMBIE] crawler stop reason=%s mode=%s Lp=%u Rp=%u\n",
                                         reason, roboZombieBodyModeName(),
                                         roboZombieLeftLegPresent ? 1 : 0,
                                         roboZombieRightLegPresent ? 1 : 0);
#else
  (void)reason;
#endif
}

void motionBaseCrawlerWriteSpeeds(int8_t leftSpeed, int8_t rightSpeed, const char* tag) {
#if JANUS_ROBOZOMBIE_ENABLE
  // If one leg is missing, the other one may still pulse gently; if both are missing,
  // this becomes a harmless no-op and the rest of BlindEye keeps running.
  if (!roboZombieLeftLegPresent) leftSpeed = 0;
  if (!roboZombieRightLegPresent) rightSpeed = 0;

  roboZombieLastLeftSpeed = leftSpeed;
  roboZombieLastRightSpeed = rightSpeed;
  roboZombieLastLeftValue = roboZombieSpeedToServoValue(leftSpeed, roboZombieLeftReverse);
  roboZombieLastRightValue = roboZombieSpeedToServoValue(rightSpeed, roboZombieRightReverse);
#if JANUS_MOTION_BASE_WRITE_ENABLE
  if (motionBasePresent && motionBasePowerOkForActuators()) {
    if (roboZombieLeftLegPresent) motionBaseWriteServoAngle(JANUS_ROBOZOMBIE_LEFT_SERVO_CH, roboZombieLastLeftValue, "crawl-L");
    if (roboZombieRightLegPresent) motionBaseWriteServoAngle(JANUS_ROBOZOMBIE_RIGHT_SERVO_CH, roboZombieLastRightValue, "crawl-R");
  }
#endif
  Serial.printf("[ROBOZOMBIE] pulse tag=%s mode=%s Lp=%u L=%d val=%u rev=%u Rp=%u R=%d val=%u rev=%u target=%d angle=%d pull=%u conf=%.2f\n",
                tag ? tag : "crawl", roboZombieBodyModeName(),
                roboZombieLeftLegPresent ? 1 : 0,
                (int)leftSpeed, (unsigned)roboZombieLastLeftValue, roboZombieLeftReverse ? 1 : 0,
                roboZombieRightLegPresent ? 1 : 0,
                (int)rightSpeed, (unsigned)roboZombieLastRightValue, roboZombieRightReverse ? 1 : 0,
                (int)motionBaseTargetAngle, (int)motionBaseServoAngle,
                (unsigned)roboZombieBasePull, roboZombieGaitConfidence);
#else
  (void)leftSpeed; (void)rightSpeed; (void)tag;
#endif
}

void motionBaseCrawlerTick(uint32_t now) {
#if JANUS_ROBOZOMBIE_ENABLE
  static bool pulseActive = false;
  static uint32_t phaseUntilMs = 0;

  if (!motionBasePresent) {
    // v2.14C: no base = штатный sensor-only mode. No actuator writes, no stop spam.
    pulseActive = false;
    phaseUntilMs = now + JANUS_ROBOZOMBIE_REST_MS;
    roboZombieLastLeftValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    roboZombieLastRightValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    return;
  }

  if (!roboZombieAnyLegPresent()) {
    // Modular build: no legs installed. Keep sensors/head/swarm/miner alive.
    pulseActive = false;
    phaseUntilMs = now + JANUS_ROBOZOMBIE_REST_MS;
    roboZombieLastLeftValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    roboZombieLastRightValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    return;
  }

  bool powerOk = motionBasePowerOkForActuators();
  float conf = roboZombieComputeConfidence();
  bool hot = tmos_motion_now || tmos_presence_now || rf_motion_now || rf_presence_now ||
             tachyonPredMotion1 > 30.0f || tachyonFutureStress > 0.72f || kenshiBubbleState >= 2 || conf > 0.72f;

  bool externalOrCharging = (motionBasePowerFlags & (0x04 | 0x20 | 0x40)) != 0;
  bool autoPowerOk = externalOrCharging || !motionBasePowerPresent || motionBaseBusMv <= 0 || motionBaseBusMv >= JANUS_ROBOZOMBIE_AUTO_MIN_MV;
  if (!autoPowerOk) roboZombieAutoBlockedLowPowerMs = now;

  bool autoAllowed = (JANUS_ROBOZOMBIE_AUTO_CRAWL_ENABLE != 0) && hot && autoPowerOk && (motionBaseArmed || roboZombieLocalArm);
  bool manualAllowed = roboZombieCrawlerManualEnable && !roboZombiePassiveMode;
  if (roboZombiePassiveMode) autoAllowed = false;
  bool allowed = motionBasePresent && !roboZombiePassiveMode && powerOk && (motionBaseArmed || roboZombieLocalArm) && (manualAllowed || autoAllowed);

  if (!allowed) {
    if (now - roboZombieCrawlerLastStopMs >= JANUS_ROBOZOMBIE_IDLE_STOP_MS ||
        roboZombieLastLeftValue != JANUS_ROBOZOMBIE_SERVO_STOP || roboZombieLastRightValue != JANUS_ROBOZOMBIE_SERVO_STOP) {
      motionBaseStopCrawler(powerOk ? (autoPowerOk ? "idle" : "auto-low-batt") : "low-power");
    }
    pulseActive = false;
    phaseUntilMs = now + JANUS_ROBOZOMBIE_REST_MS;
    return;
  }

  if (pulseActive) {
    if (now < phaseUntilMs) return;
    motionBaseStopCrawler("pulse-end");
    pulseActive = false;
    uint16_t rest = JANUS_ROBOZOMBIE_REST_MS;
    if (conf > 1.15f || manualAllowed) rest = (uint16_t)max(250, (int)JANUS_ROBOZOMBIE_REST_MS - 110);
    if (motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV && !externalOrCharging) rest += 180;
    phaseUntilMs = now + rest;
    return;
  }

  if (now < phaseUntilMs) return;

  int16_t err = motionBaseTargetAngle - JANUS_MOTION_BASE_TRACK_CENTER_DEG;
  int8_t base = (int8_t)constrain((int)roboZombieBasePull, 12, 78);

  // Confidence makes it braver; weak battery makes it polite.
  if (!manualAllowed) {
    base = (int8_t)constrain((int)roundf(16.0f + conf * 19.0f), 14, (int)roboZombieBasePull);
  }
  if (motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV && !externalOrCharging) {
    if (base > JANUS_ROBOZOMBIE_LOW_BATT_PULL_CAP) base = JANUS_ROBOZOMBIE_LOW_BATT_PULL_CAP;
  }

  int8_t soft = (int8_t)max(7, base / 3);
  int8_t pivot = (int8_t)max(6, base / 4);
  int8_t left = base;
  int8_t right = base;
  const char* tag = "forward-conf";

  if (err < -JANUS_ROBOZOMBIE_PIVOT_ERR_DEG && conf > 0.85f) {
    left = -pivot;
    right = base;
    tag = "pivot-left";
  } else if (err > JANUS_ROBOZOMBIE_PIVOT_ERR_DEG && conf > 0.85f) {
    left = base;
    right = -pivot;
    tag = "pivot-right";
  } else if (err < -JANUS_ROBOZOMBIE_CENTER_DEADBAND) {
    left = soft;
    right = base;
    tag = "turn-left";
  } else if (err > JANUS_ROBOZOMBIE_CENTER_DEADBAND) {
    left = base;
    right = soft;
    tag = "turn-right";
  }

  // One-legged fallback: don't try to pivot with a missing opposite leg.
  if (roboZombieLeftLegPresent && !roboZombieRightLegPresent) {
    left = (err > JANUS_ROBOZOMBIE_CENTER_DEADBAND) ? base : soft;
    right = 0;
    tag = "one-leg-left";
  } else if (!roboZombieLeftLegPresent && roboZombieRightLegPresent) {
    left = 0;
    right = (err < -JANUS_ROBOZOMBIE_CENTER_DEADBAND) ? base : soft;
    tag = "one-leg-right";
  }

  motionBaseCrawlerWriteSpeeds(left, right, tag);
  roboZombieCrawlerPulses++;
  roboZombieCrawlerLastPulseMs = now;
  pulseActive = true;
  uint16_t pulse = JANUS_ROBOZOMBIE_PULSE_MS;
  if (conf > 1.25f || manualAllowed) pulse = (uint16_t)min(240, (int)JANUS_ROBOZOMBIE_PULSE_MS + 35);
  phaseUntilMs = now + pulse;
#else
  (void)now;
#endif
}

void handleRoboZombieSerial() {
#if JANUS_ROBOZOMBIE_ENABLE
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n' || c == ' ') continue;
    switch (c) {
      case 'a': case 'A':
        if (roboZombiePassiveMode) {
          roboZombiePassiveMode = false;
          motionBaseTrackEnabled = true;
          Serial.println("[ROBOZOMBIE] PASSIVE_EYE OFF by arm key");
        }
        roboZombieLocalArm = !roboZombieLocalArm;
        if (!roboZombieLocalArm) motionBaseStopCrawler("local-disarm");
        Serial.printf("[ROBOZOMBIE] localArm=%u passive=%u\n", roboZombieLocalArm ? 1 : 0, roboZombiePassiveMode ? 1 : 0);
        break;
      case 'g': case 'G':
        if (roboZombiePassiveMode) {
          roboZombiePassiveMode = false;
          motionBaseTrackEnabled = true;
          roboZombieLocalArm = true;
          Serial.println("[ROBOZOMBIE] PASSIVE_EYE OFF by crawl key");
        }
        roboZombieCrawlerManualEnable = !roboZombieCrawlerManualEnable;
        if (!roboZombieCrawlerManualEnable) motionBaseStopCrawler("manual-off");
        Serial.printf("[ROBOZOMBIE] crawlerManual=%u passive=%u\n", roboZombieCrawlerManualEnable ? 1 : 0, roboZombiePassiveMode ? 1 : 0);
        break;
      case 'h': case 'H':
        roboZombieHeadPresent = !roboZombieHeadPresent;
        motionBaseLastSentAngle = -1;
        Serial.printf("[ROBOZOMBIE] headPresent=%u mode=%s\n", roboZombieHeadPresent ? 1 : 0, roboZombieBodyModeName());
        break;
      case 'j': case 'J':
        roboZombieLeftLegPresent = !roboZombieLeftLegPresent;
        motionBaseStopCrawler("left-present-toggle");
        Serial.printf("[ROBOZOMBIE] leftLegPresent=%u mode=%s\n", roboZombieLeftLegPresent ? 1 : 0, roboZombieBodyModeName());
        break;
      case 'k': case 'K':
        roboZombieRightLegPresent = !roboZombieRightLegPresent;
        motionBaseStopCrawler("right-present-toggle");
        Serial.printf("[ROBOZOMBIE] rightLegPresent=%u mode=%s\n", roboZombieRightLegPresent ? 1 : 0, roboZombieBodyModeName());
        break;
      case 'S':
        // HARD PASSIVE SENSOR MODE: stop S2/S4 rotors, disarm local/Core motion,
        // freeze head writes, but keep TMOS/RF/ESP-NOW/miner/learning alive.
        roboZombiePassiveMode = true;
        roboZombieCrawlerManualEnable = false;
        roboZombieLocalArm = false;
        motionBaseArmed = false;
        motionBaseTrackEnabled = false;
        motionBaseTargetAngle = motionBaseServoAngle;
        motionBaseStopCrawler("SERIAL-S-PASSIVE");
        Serial.println("[ROBOZOMBIE] PASSIVE_EYE ON: S2/S4 rotors stopped, head frozen, sensors/swarm alive. Press a/g/1/2/3 to wake.");
        break;
      case 's': case '0':
        roboZombieCrawlerManualEnable = false;
        motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
        motionBaseStopCrawler("serial-stop");
        Serial.println("[ROBOZOMBIE] STOP once + head center. Capital S = persistent PASSIVE_EYE.");
        break;
      case '1':
        if (roboZombiePassiveMode) { roboZombiePassiveMode = false; motionBaseTrackEnabled = true; roboZombieLocalArm = true; Serial.println("[ROBOZOMBIE] PASSIVE_EYE OFF by manual target"); }
        motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_MIN_DEG;
        motionBaseTrackEnabled = true;
        Serial.printf("[ROBOZOMBIE] manual target LEFT %d\n", (int)motionBaseTargetAngle);
        break;
      case '2':
        if (roboZombiePassiveMode) { roboZombiePassiveMode = false; motionBaseTrackEnabled = true; roboZombieLocalArm = true; Serial.println("[ROBOZOMBIE] PASSIVE_EYE OFF by manual target"); }
        motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
        motionBaseTrackEnabled = true;
        Serial.printf("[ROBOZOMBIE] manual target CENTER %d\n", (int)motionBaseTargetAngle);
        break;
      case '3':
        if (roboZombiePassiveMode) { roboZombiePassiveMode = false; motionBaseTrackEnabled = true; roboZombieLocalArm = true; Serial.println("[ROBOZOMBIE] PASSIVE_EYE OFF by manual target"); }
        motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_MAX_DEG;
        motionBaseTrackEnabled = true;
        Serial.printf("[ROBOZOMBIE] manual target RIGHT %d\n", (int)motionBaseTargetAngle);
        break;
      case '+': case '=':
        roboZombieBasePull = (uint8_t)constrain((int)roboZombieBasePull + 4, 10, 70);
        Serial.printf("[ROBOZOMBIE] pull=%u\n", (unsigned)roboZombieBasePull);
        break;
      case '-': case '_':
        roboZombieBasePull = (uint8_t)constrain((int)roboZombieBasePull - 4, 10, 70);
        Serial.printf("[ROBOZOMBIE] pull=%u\n", (unsigned)roboZombieBasePull);
        break;
      case 'l': case 'L':
        roboZombieLeftReverse = !roboZombieLeftReverse;
        motionBaseStopCrawler("flip-left");
        Serial.printf("[ROBOZOMBIE] leftReverse=%u\n", roboZombieLeftReverse ? 1 : 0);
        break;
      case 'r': case 'R':
        roboZombieRightReverse = !roboZombieRightReverse;
        motionBaseStopCrawler("flip-right");
        Serial.printf("[ROBOZOMBIE] rightReverse=%u\n", roboZombieRightReverse ? 1 : 0);
        break;
      case 'p': case 'P':
        Serial.printf("[ROBOZOMBIE] mode=%s map S1=head S2=left360 S4=right360 headP=%u leftP=%u rightP=%u arm=%u crawl=%u passive=%u Lrev=%u Rrev=%u pull=%u conf=%.2f Lval=%u Rval=%u base=%u power=%u mv=%d pct=%u flags=0x%02X pulses=%lu\n",
                      roboZombieBodyModeName(), roboZombieHeadPresent ? 1 : 0,
                      roboZombieLeftLegPresent ? 1 : 0, roboZombieRightLegPresent ? 1 : 0,
                      roboZombieLocalArm ? 1 : 0, roboZombieCrawlerManualEnable ? 1 : 0, roboZombiePassiveMode ? 1 : 0,
                      roboZombieLeftReverse ? 1 : 0, roboZombieRightReverse ? 1 : 0,
                      (unsigned)roboZombieBasePull, roboZombieGaitConfidence,
                      (unsigned)roboZombieLastLeftValue, (unsigned)roboZombieLastRightValue,
                      motionBasePresent ? 1 : 0, motionBasePowerPresent ? 1 : 0,
                      (int)motionBaseBusMv, (unsigned)motionBaseBatteryPct,
                      (unsigned)motionBasePowerFlags, (unsigned long)roboZombieCrawlerPulses);
        break;
      default:
        Serial.printf("[ROBOZOMBIE] keys: S passive, s stop, a arm/wake, g crawl/wake, h head-present, j left-present, k right-present, 1/2/3 target, +/- pull, l/r reverse, p print. got='%c'\n", c);
        break;
    }
  }
#endif
}

void initMotionBase() {
#if JANUS_MOTION_BASE_ENABLE
  // Official M5Atomic-Motion wiring for AtomS3 / AtomS3R:
  // Motion Base MCU = I2C 0x38 on SDA=38 / SCL=39.
  // Servo registers: 0..3. DC motor registers: 32..33.
  // v2.14C: the base is OPTIONAL. If it is not present, BlindEye continues as
  // a normal TMOS/RF/ESP-NOW swarm sensor. No endless fault mode.
  motionBaseScanBuses();

  motionBaseUsesMainWire = true;
  motionWireStarted = janusSelectMotionBus(true);
  motionBasePresent = motionWireStarted && motionI2cProbeOnSelectedBus(JANUS_MOTION_BASE_I2C_ADDR);
  motionBaseEverDetected = motionBaseEverDetected || motionBasePresent;
  motionBaseOptionalAbsent = !motionBasePresent;
  if (motionBaseOptionalAbsent && !motionBaseAbsentSinceMs) motionBaseAbsentSinceMs = millis();

  motionBasePowerPresent = false;
  motionBasePowerAddr = 0;
  motionBaseServoAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
  motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
  motionBaseLastSentAngle = -1;
  motionBaseMotorSpeed[0] = 0;
  motionBaseMotorSpeed[1] = 0;

  if (!motionBasePresent) {
    // Do not count absence as an I2C fault. It is a supported hardware profile.
    motionBaseI2cErrors = 0;
    motionBaseBusMv = 0;
    motionBaseCurrentRaw = 0;
    motionBasePowerRaw = 0;
    motionBasePowerFlags = 0;
    motionBasePowerSource = 5;   // local name: NOBASE
    motionBaseBatteryPct = 0;
    motionBaseArmed = false;
    roboZombieCrawlerManualEnable = false;
    roboZombieLastLeftSpeed = 0;
    roboZombieLastRightSpeed = 0;
    roboZombieLastLeftValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    roboZombieLastRightValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    janusSelectGroveBus(true);

    Serial.printf("[MOTIONBASE] OPTIONAL ABSENT -> штатный SENSOR MODE. expected=0x%02X on SDA%u/SCL%u; TMOS/RF/ESP-NOW/miner continue.\n",
                  (unsigned)JANUS_MOTION_BASE_I2C_ADDR,
                  (unsigned)JANUS_MOTION_BASE_SDA_PIN,
                  (unsigned)JANUS_MOTION_BASE_SCL_PIN);
    Serial.printf("[ROBOZOMBIE] mode=%s base=0 headConfigured=%u leftConfigured=%u rightConfigured=%u. Actuators disabled, sensor brain alive.\n",
                  roboZombieBodyModeName(),
                  roboZombieHeadPresent ? 1 : 0,
                  roboZombieLeftLegPresent ? 1 : 0,
                  roboZombieRightLegPresent ? 1 : 0);
    motionBaseSendPowerPacket(true);
    return;
  }

  if (motionI2cProbeOnSelectedBus(JANUS_MOTION_BASE_INA226_ADDR_A)) {
    motionBasePowerPresent = true;
    motionBasePowerAddr = JANUS_MOTION_BASE_INA226_ADDR_A;
  } else if (motionI2cProbeOnSelectedBus(JANUS_MOTION_BASE_INA226_ADDR_B)) {
    motionBasePowerPresent = true;
    motionBasePowerAddr = JANUS_MOTION_BASE_INA226_ADDR_B;
  }
  janusSelectGroveBus(true);

  motionBaseSafeStop();
  motionBaseReadPower();
  motionBaseUpdateBatteryState();

  Serial.printf("[MOTIONBASE] Atomic Motion Base v1.2 OFFICIAL present=%u power=%u addr=0x%02X bus=WireMux-38/39 batt=%u%% mv=%d flags=0x%02X writes=%u trackServo=S%u span=%d..%d i2cErr=%lu\n",
                motionBasePresent ? 1 : 0, motionBasePowerPresent ? 1 : 0,
                motionBasePowerAddr,
                (unsigned)motionBaseBatteryPct, (int)motionBaseBusMv, (unsigned)motionBasePowerFlags,
                (unsigned)JANUS_MOTION_BASE_WRITE_ENABLE,
                (unsigned)(JANUS_MOTION_BASE_TRACK_SERVO_CH + 1),
                JANUS_MOTION_BASE_TRACK_MIN_DEG, JANUS_MOTION_BASE_TRACK_MAX_DEG,
                (unsigned long)motionBaseI2cErrors);

  Serial.printf("[ROBOZOMBIE] modular mode=%s headP=%u leftP=%u rightP=%u auto=%u pull=%u keys: S passive, s stop, a/g wake, h/j/k toggle installed parts, g manual crawl\n",
                roboZombieBodyModeName(), roboZombieHeadPresent ? 1 : 0,
                roboZombieLeftLegPresent ? 1 : 0, roboZombieRightLegPresent ? 1 : 0,
                (unsigned)JANUS_ROBOZOMBIE_AUTO_CRAWL_ENABLE, (unsigned)roboZombieBasePull);

#endif
}


void motionBaseSafeStop() {
#if JANUS_MOTION_BASE_ENABLE
  motionBaseMotorSpeed[0] = 0;
  motionBaseMotorSpeed[1] = 0;
#if JANUS_MOTION_BASE_WRITE_ENABLE
  if (motionBasePresent) {
    motionI2cWrite8(JANUS_MOTION_BASE_I2C_ADDR, 0x20, 0);
    motionI2cWrite8(JANUS_MOTION_BASE_I2C_ADDR, 0x21, 0);
  }
#endif
  motionBaseStopCrawler("safe-stop");
#endif
}

uint8_t motionBaseEstimateBatteryPct(uint16_t mv) {
  if (mv == 0) return 0;
  // Atomic Motion Base usually sees a 1S Li-ion/LiPo pack through INA226.
  // If the bus is above Li-ion range, treat it as external/boost/USB-like power.
  if (mv >= 4400) return 100;
  struct P { uint16_t mv; uint8_t pct; } curve[] = {
    {4200,100}, {4120,92}, {4060,84}, {4000,76}, {3940,68}, {3880,60},
    {3820,52}, {3760,44}, {3710,36}, {3660,28}, {3600,20}, {3520,12},
    {3440,6}, {3350,2}, {3200,0}
  };
  if (mv >= curve[0].mv) return curve[0].pct;
  for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
    if (mv >= curve[i].mv) {
      uint16_t hiMv = curve[i - 1].mv, loMv = curve[i].mv;
      uint8_t hiPct = curve[i - 1].pct, loPct = curve[i].pct;
      float t = (float)(mv - loMv) / (float)max((int)(hiMv - loMv), 1);
      return (uint8_t)constrain((int)roundf((float)loPct + t * (float)(hiPct - loPct)), 0, 100);
    }
  }
  return 0;
}

void motionBaseUpdateBatteryState() {
#if JANUS_MOTION_BASE_ENABLE
  motionBasePowerFlags = 0;
  if (!motionBasePresent) {
    // v2.14C: base is optional and absent is not a battery fault.
    motionBasePowerSource = 5; // NOBASE
    motionBaseBatteryPct = 0;
    motionBaseBusMv = 0;
    motionBaseCurrentRaw = 0;
    motionBasePowerRaw = 0;
    return;
  }
  if (motionBasePresent) motionBasePowerFlags |= 0x01;
  if (motionBasePowerPresent) motionBasePowerFlags |= 0x02;

  if (motionBasePowerPresent && motionBaseBusMv > 0) {
    const bool external = motionBaseBusMv >= JANUS_MOTION_BASE_EXT_MV;
    const bool rawCurrentSeen = abs((int)motionBaseCurrentRaw) >= JANUS_MOTION_BASE_CHG_CURRENT_MIN;

    if (!external) {
      // Real 1S cell range. This is the only moment where INA bus voltage can be used
      // as a believable battery percentage.
      motionBasePowerSource = 1;
      motionBaseLastCellMv = (uint16_t)motionBaseBusMv;
      motionBaseBatteryPct = motionBaseEstimateBatteryPct(motionBaseLastCellMv);
      motionBaseLastCellPct = motionBaseBatteryPct;
      motionBaseExternalSinceMs = 0;
      if (motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV) motionBasePowerFlags |= 0x08;
      if (motionBaseBusMv < JANUS_MOTION_BASE_SLEEP_MV) motionBasePowerFlags |= 0x10;
    } else {
      // USB-C / boost / charger rail. Do NOT blindly report 100%: the INA226 now sees
      // the powered rail, not necessarily the bare cell voltage. Keep last known cell
      // estimate and mark the packet as charge-aware.
      motionBasePowerFlags |= 0x04; // external/USB present
      if (!motionBaseExternalSinceMs) motionBaseExternalSinceMs = millis();

      uint8_t heldPct = motionBaseLastCellMv ? motionBaseLastCellPct : motionBaseEstimateBatteryPct(JANUS_MOTION_BASE_FULL_MV);
      bool estimateOnly = motionBaseLastCellMv == 0;
      if (estimateOnly) motionBasePowerFlags |= 0x80;

      bool likelyFull = (!estimateOnly && heldPct >= 96) || (motionBaseBusMv >= 5000 && !rawCurrentSeen && heldPct >= 92);
      bool likelyCharging = !likelyFull;

      if (likelyCharging) {
        motionBasePowerSource = 3; // charging / USB-C attached
        motionBasePowerFlags |= 0x20;
      } else {
        motionBasePowerSource = 4; // full/float/external hold
        motionBasePowerFlags |= 0x40;
      }
      motionBaseBatteryPct = constrain((int)heldPct, 0, 100);
    }
  } else {
    motionBasePowerSource = 0;
    motionBaseBatteryPct = 0;
  }
#else
  motionBasePowerFlags = 0; motionBasePowerSource = 0; motionBaseBatteryPct = 0;
#endif
}

uint32_t janusEyePowerCrc32(const void* data, size_t len) {
  const uint8_t* p = (const uint8_t*)data;
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 16777619UL; }
  return h;
}


const char* motionBasePowerSourceName(uint8_t src, uint8_t flags) {
  if (src == 5 || !motionBasePresent) return "NOBASE";
  if (flags & 0x20) return "CHG";
  if (flags & 0x40) return "FULL";
  if (src == 3) return "CHG";
  if (src == 4) return "FULL";
  if (src == 2 || (flags & 0x04)) return "EXT";
  if (src == 1) return "BAT";
  return "UNK";
}

void motionBaseSendPowerPacket(bool force) {
#if JANUS_MOTION_BASE_ENABLE
  uint32_t now = millis();
  uint32_t txInterval = motionBasePresent ? JANUS_EYE_POWER_TX_MS : JANUS_MOTION_BASE_ABSENT_STATUS_MS;
  if (!force && now - motionBaseLastBatteryTxMs < txInterval) return;
  motionBaseLastBatteryTxMs = now;
  motionBaseUpdateBatteryState();

  JanusEyePowerPacket eb{};
  eb.magic[0] = 'E'; eb.magic[1] = 'B';
  eb.version = 1;
  eb.flags = motionBasePowerFlags;
  strlcpy(eb.nodeId, "BlindEye", sizeof(eb.nodeId));
  eb.seq = ++motionBaseBatterySeq;
  eb.uptime_ms = now;
  eb.bus_mv = (uint16_t)constrain((int)motionBaseBusMv, 0, 65535);
  eb.current_raw = motionBaseCurrentRaw;
  eb.power_raw = motionBasePowerRaw;
  eb.battery_pct = motionBaseBatteryPct;
  eb.source = motionBasePowerSource;
  eb.servo_angle = (uint16_t)constrain((int)motionBaseServoAngle, 0, 65535);
  eb.target_angle = (uint16_t)constrain((int)motionBaseTargetAngle, 0, 65535);
  eb.crc = 0;
  eb.crc = janusEyePowerCrc32(&eb, sizeof(eb));
  bool ok = janusEyeEspNowSend("E/B", &eb, sizeof(eb), true);
  if (force || eb.seq <= 3 || (eb.seq % 20UL) == 0) {
    Serial.printf("[EYE/BATT] tx=%s seq=%lu pct=%u mv=%u flags=0x%02X src=%u/%s cell=%umV cur=%d pwr=%d\n",
                  ok ? "OK" : "FAIL", (unsigned long)eb.seq, (unsigned)eb.battery_pct,
                  (unsigned)eb.bus_mv, (unsigned)eb.flags, (unsigned)eb.source,
                  motionBasePowerSourceName(eb.source, eb.flags), (unsigned)motionBaseLastCellMv,
                  (int)eb.current_raw, (int)eb.power_raw);
  }
#else
  (void)force;
#endif
}

void motionBaseReadPower() {
#if JANUS_MOTION_BASE_ENABLE
  if (!motionBasePowerPresent || !motionBasePowerAddr) return;
  uint16_t bus = 0, current = 0, power = 0;
  if (motionI2cRead16(motionBasePowerAddr, 0x02, bus)) {
    // INA226 bus voltage LSB is 1.25mV.
    motionBaseBusMv = (int16_t)constrain((int)((uint32_t)bus * 125UL / 100UL), 0, 32767);
  }
  if (motionI2cRead16(motionBasePowerAddr, 0x04, current)) motionBaseCurrentRaw = (int16_t)current;
  if (motionI2cRead16(motionBasePowerAddr, 0x03, power)) motionBasePowerRaw = (int16_t)power;
  motionBaseUpdateBatteryState();
#endif
}

int16_t motionBaseSectorToAngle(uint8_t sector) {
  sector %= JANUS_KENSHI_SECTORS;
  float t = (JANUS_KENSHI_SECTORS <= 1) ? 0.5f : ((float)sector / (float)(JANUS_KENSHI_SECTORS - 1));
  return (int16_t)constrain((int)roundf(JANUS_MOTION_BASE_TRACK_MIN_DEG + t * (JANUS_MOTION_BASE_TRACK_MAX_DEG - JANUS_MOTION_BASE_TRACK_MIN_DEG)),
                            JANUS_MOTION_BASE_TRACK_MIN_DEG, JANUS_MOTION_BASE_TRACK_MAX_DEG);
}

void motionBasePlanTarget() {
#if JANUS_MOTION_BASE_ENABLE
  if (!motionBaseTrackEnabled) {
    motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
    return;
  }

  bool hot = tmos_motion_now || tmos_presence_now || tachyonPredMotion1 > 38.0f || tachyonFutureStress > 0.85f || kenshiBubbleState >= 2;
  if (hot) {
    uint8_t sector = (tachyonFutureStress > 0.75f) ? kenshiPredSector : kenshiSector;
    int16_t sectorAngle = motionBaseSectorToAngle(sector);
    float memory = constrain(max(tmos_motion_memory, tmos_presence_memory), 0.0f, 1.0f);
    float alpha = constrain(0.22f + memory * 0.38f + tachyonFutureStress * 0.14f, 0.20f, 0.74f);
    motionBaseTargetAngle = (int16_t)constrain((int)roundf(motionBaseTargetAngle * (1.0f - alpha) + sectorAngle * alpha),
                                               JANUS_MOTION_BASE_TRACK_MIN_DEG, JANUS_MOTION_BASE_TRACK_MAX_DEG);
  } else {
    // No target: relax toward center slowly.
    motionBaseTargetAngle = (int16_t)roundf(motionBaseTargetAngle * 0.96f + JANUS_MOTION_BASE_TRACK_CENTER_DEG * 0.04f);
  }

  if (motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV && !(motionBasePowerFlags & 0x04)) {
    // Low battery: do not chase aggressively.
    motionBaseTargetAngle = (int16_t)roundf(motionBaseTargetAngle * 0.90f + JANUS_MOTION_BASE_TRACK_CENTER_DEG * 0.10f);
  }
#endif
}

void motionBaseTick() {
#if JANUS_MOTION_BASE_ENABLE
  uint32_t now = millis();

  if (!motionBasePresent) {
    // v2.14C: no ATOMIC Motion Base attached. Stay in штатный sensor-only mode.
    // Keep telemetry alive for Core2, but do not touch the missing actuator bus.
    motionBaseArmed = false;
    roboZombieCrawlerManualEnable = false;
    motionBaseServoAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
    motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
    motionBaseSendPowerPacket(false);
    motionBaseSendStatusEvent(false);
    return;
  }

  if (now - motionBaseLastPowerMs >= JANUS_MOTION_BASE_POWER_MS) {
    motionBaseLastPowerMs = now;
    motionBaseReadPower();
  }

  if (now - motionBaseLastTickMs < JANUS_MOTION_BASE_TICK_MS) return;
  motionBaseLastTickMs = now;

  if (roboZombiePassiveMode) {
    motionBaseTargetAngle = motionBaseServoAngle;
    // Keep passive mode quiet: do not spam stop writes/logs every 80 ms.
    if (roboZombieCrawlerManualEnable ||
        roboZombieLastLeftValue != JANUS_ROBOZOMBIE_SERVO_STOP ||
        roboZombieLastRightValue != JANUS_ROBOZOMBIE_SERVO_STOP) {
      roboZombieCrawlerManualEnable = false;
      motionBaseStopCrawler("passive-s");
    }
    motionBaseSendStatusEvent(false);
    return;
  }

  motionBasePlanTarget();

  int16_t diff = motionBaseTargetAngle - motionBaseServoAngle;
  if (diff > JANUS_MOTION_BASE_MAX_STEP_DEG) diff = JANUS_MOTION_BASE_MAX_STEP_DEG;
  if (diff < -JANUS_MOTION_BASE_MAX_STEP_DEG) diff = -JANUS_MOTION_BASE_MAX_STEP_DEG;
  motionBaseServoAngle = constrain((int)(motionBaseServoAngle + diff), JANUS_MOTION_BASE_TRACK_MIN_DEG, JANUS_MOTION_BASE_TRACK_MAX_DEG);

#if JANUS_MOTION_BASE_WRITE_ENABLE
  bool powerOk = motionBasePowerOkForActuators();
  bool headAllowed = motionBasePresent && !roboZombiePassiveMode && roboZombieHeadPresent && (motionBaseArmed || roboZombieLocalArm) && powerOk;
  if (headAllowed && abs(motionBaseServoAngle - motionBaseLastSentAngle) >= 1) {
    if (motionBaseWriteServoAngle(JANUS_MOTION_BASE_TRACK_SERVO_CH, (uint8_t)constrain((int)motionBaseServoAngle, 0, 180), "head-S1")) {
      motionBaseLastSentAngle = motionBaseServoAngle;
    }
  } else if (!roboZombieHeadPresent) {
    // Modular build: no head servo installed. Planner still runs for Core2 telemetry and leg steering.
    motionBaseLastSentAngle = motionBaseServoAngle;
  }
#else
  // Dry-run: planner runs, no physical write.
  motionBaseLastSentAngle = motionBaseServoAngle;
#endif

  motionBaseCrawlerTick(now);
  motionBaseSendStatusEvent(false);
#endif
}

void motionBaseSendStatusEvent(bool force) {
#if JANUS_MOTION_BASE_ENABLE && JANUS_EVENT_BUS_ENABLE
  uint32_t now = millis();
  if (!force && now - motionBaseLastStatusMs < JANUS_MOTION_BASE_STATUS_MS) return;
  motionBaseLastStatusMs = now;

  uint8_t conf = motionBasePresent ? 82 : 78;
  uint8_t urg = motionBasePresent ? 18 : 4;
  uint8_t eventType = motionBasePresent ? JE_ENV : JE_SAFE;
  uint16_t topic = motionBasePresent ? janusHash16("motion_base") : janusHash16("motion_base_absent");
  uint16_t object = motionBasePresent ? janusHash16("blind_eye_pan") : janusHash16("sensor_only");
  if (motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV && !(motionBasePowerFlags & 0x04)) urg = 72;
  if (motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_SLEEP_MV && !(motionBasePowerFlags & 0x04)) urg = 90;
  if (urg >= 72) eventType = JE_DANGER;

  janusEmitEyeEvent(eventType, conf, urg,
                    (int16_t)(motionBaseServoAngle * 10),
                    (int16_t)(motionBaseTargetAngle * 10),
                    motionBaseBusMv,
                    motionBaseCurrentRaw,
                    topic, object, motionBasePresent ? 9000UL : 20000UL);
#endif
}

// ========================= JANUS KENSHI BUBBLE BUS =========================
// Inspired by the "visible bubble / virtual world" idea: ESP-NOW remains simple,
// but the Eye stops thinking every peer must be fully simulated every tick.
// Quiet peers are compressed into virtual timers and a few world-state flags.
// Hot peers/events materialize into active bubble packets.

enum JanusKenshiWorldFlags : uint32_t {
  K_WORLD_PRESENCE = 1UL << 0,
  K_WORLD_MOTION   = 1UL << 1,
  K_WORLD_MIC      = 1UL << 2,
  K_WORLD_SHOCK    = 1UL << 3,
  K_WORLD_MASTER   = 1UL << 4,
  K_WORLD_JOB      = 1UL << 5,
  K_WORLD_AGENT    = 1UL << 6,
  K_WORLD_UNSTABLE = 1UL << 7,
  K_WORLD_TRAINING = 1UL << 8,
  K_WORLD_LOW_SYNC = 1UL << 9,
  K_WORLD_RF       = 1UL << 10
};

uint8_t kenshiFindNodeSlot(const char* nodeId) {
  if (!nodeId || !nodeId[0]) nodeId = "node";
  for (uint8_t i = 0; i < JANUS_KENSHI_MAX_NODES; ++i) {
    if (kenshiNodes[i].active && strncmp(kenshiNodes[i].nodeId, nodeId, sizeof(kenshiNodes[i].nodeId)) == 0) return i;
  }

  uint8_t freeSlot = 255;
  uint8_t oldestSlot = 0;
  uint32_t oldestAge = 0;
  uint32_t now = millis();

  for (uint8_t i = 0; i < JANUS_KENSHI_MAX_NODES; ++i) {
    if (!kenshiNodes[i].active && freeSlot == 255) freeSlot = i;
    uint32_t age = now - kenshiNodes[i].lastSeenMs;
    if (age > oldestAge) {
      oldestAge = age;
      oldestSlot = i;
    }
  }
  return freeSlot != 255 ? freeSlot : oldestSlot;
}

void kenshiDecayAndCountNodes() {
  uint32_t now = millis();
  kenshiActiveNodes = 0;
  kenshiVirtualNodes = 0;
  float eSum = 0.0f;
  float wSum = 0.0f;

  for (uint8_t i = 0; i < JANUS_KENSHI_MAX_NODES; ++i) {
    JanusKenshiNode& n = kenshiNodes[i];
    if (!n.active) continue;
    uint32_t age = now - n.lastSeenMs;
    if (age > JANUS_KENSHI_VIRTUAL_TTL_MS) {
      n.active = false;
      continue;
    }

    bool hot = (age <= JANUS_KENSHI_ACTIVE_TTL_MS) && (n.priority >= 80 || (n.flags & 0x03));
    if (hot) kenshiActiveNodes++;
    else kenshiVirtualNodes++;

    float w = constrain(1.0f - (float)age / (float)JANUS_KENSHI_VIRTUAL_TTL_MS, 0.05f, 1.0f);
    w *= constrain(0.35f + n.confidence * 0.65f, 0.10f, 1.25f);
    eSum += n.entropy * w;
    wSum += w;
  }

  kenshiVirtualEntropy = (wSum > 0.001f) ? (eSum / wSum) : eyeLocalEntropy();
}

uint8_t kenshiInferSector() {
  static float lastPresence = 0.0f;
  static float lastMotion = 0.0f;
  static float drift = 0.0f;

  float dp = tmos_presence - lastPresence;
  float dm = tmos_motion - lastMotion;
  lastPresence = tmos_presence;
  lastMotion = tmos_motion;

  // Blind Eye has a single TMOS channel today, so direction is inferred from temporal
  // motion shape + IMU/mag phase. When Motion Base arrives, this same sector index
  // can drive servo angle directly.
  drift = drift * 0.82f + (dm * 0.035f + dp * 0.012f + gyro_z * 0.04f + mag_norm * 0.001f) * 0.18f;

  float phase = drift + (float)(colonyAgentEntropySeed & 0xFF) * 0.003f + (float)(millis() & 0x3FF) * 0.0004f;
  int sector = (int)floorf(fmodf(fabsf(phase) * 1.37f + kenshiLastSector * 0.63f, (float)JANUS_KENSHI_SECTORS));
  sector = constrain(sector, 0, JANUS_KENSHI_SECTORS - 1);

  if (tmos_motion < 2.0f && tmos_presence < 4.0f) sector = kenshiLastSector; // no fake turning when nothing moves
  return (uint8_t)sector;
}

uint8_t kenshiPredictNextSector(uint8_t sector) {
  sector %= JANUS_KENSHI_SECTORS;
  float best = -1.0f;
  uint8_t bestIdx = sector;

  for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) {
    float v = kenshiMarkov[sector][j];
    if (j == sector) v += 0.04f; // inertia
    if (j == (uint8_t)((sector + 1) % JANUS_KENSHI_SECTORS)) v += constrain(tmos_motion / 900.0f, 0.0f, 0.10f);
    if (v > best) {
      best = v;
      bestIdx = j;
    }
  }

  return bestIdx;
}

void kenshiTrainMarkov(uint8_t from, uint8_t to, float strength) {
  from %= JANUS_KENSHI_SECTORS;
  to %= JANUS_KENSHI_SECTORS;
  strength = constrain(strength, 0.02f, 1.0f);

  for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) {
    kenshiMarkov[from][j] *= 0.992f;
  }
  kenshiMarkov[from][to] += 0.020f + strength * 0.050f;

  // Normalize row softly so it remains a probability-like memory.
  float s = 0.0f;
  for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) s += kenshiMarkov[from][j];
  if (s > 2.5f) {
    for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) kenshiMarkov[from][j] /= s;
  }
  kenshiStateDirty = true;
}

void updateKenshiVirtualWorld() {
#if JANUS_KENSHI_BUS_ENABLE
  kenshiDecayAndCountNodes();

  float sensorPower =
    constrain(tmos_presence / 350.0f, 0.0f, 1.4f) +
    constrain(tmos_motion / 180.0f, 0.0f, 1.6f) +
    constrain(mic_rms * 9.0f, 0.0f, 1.2f) +
    constrain(imu_shock / 5.0f, 0.0f, 1.1f) +
    constrain(rf_motion_energy / 8.0f + rf_presence_score * 0.50f, 0.0f, 1.2f) +
    constrain(loss * 1.4f, 0.0f, 1.0f);

  if (colonyMasterSeen) sensorPower += 0.18f;
  if (colonyJob.active) sensorPower += 0.22f;
  if (millis() - colonyAgentLastRewardMs < COLONY_AGENT_REWARD_VISIBLE_MS) sensorPower += 0.20f;

  kenshiEventPower = kenshiEventPower * 0.82f + sensorPower * 0.18f;
  kenshiSector = kenshiInferSector();

  if (kenshiSector != kenshiLastSector || kenshiEventPower > 0.40f) {
    kenshiTrainMarkov(kenshiLastSector, kenshiSector, kenshiEventPower);
    kenshiLastSector = kenshiSector;
  }
  kenshiPredSector = kenshiPredictNextSector(kenshiSector);

  kenshiWorldFlags = 0;
  if (tmos_presence > 10.0f) kenshiWorldFlags |= K_WORLD_PRESENCE;
  if (tmos_motion > 6.0f)    kenshiWorldFlags |= K_WORLD_MOTION;
  if (mic_rms > 0.015f)      kenshiWorldFlags |= K_WORLD_MIC;
  if (imu_shock > 1.30f)     kenshiWorldFlags |= K_WORLD_SHOCK;
  if (colonyMasterSeen)      kenshiWorldFlags |= K_WORLD_MASTER;
  if (colonyJob.active)      kenshiWorldFlags |= K_WORLD_JOB;
  if (millis() - colonyAgentLastRewardMs < COLONY_AGENT_REWARD_VISIBLE_MS) kenshiWorldFlags |= K_WORLD_AGENT;
  if (loss > 0.35f)          kenshiWorldFlags |= K_WORLD_UNSTABLE;
  if (loss > 0.12f)          kenshiWorldFlags |= K_WORLD_TRAINING;
  if (sync_hint < 0.45f)     kenshiWorldFlags |= K_WORLD_LOW_SYNC;
  if (rf_presence_now || rf_motion_now) kenshiWorldFlags |= K_WORLD_RF;

  if (kenshiEventPower > 2.05f || fabsf(z_activity) > 3.2f || fabsf(z_loss) > 3.0f) {
    kenshiBubbleState = 3;
    kenshiJobState = 3; // alert
  } else if (tmos_motion > 9.0f || tmos_presence > 25.0f || mic_rms > 0.02f || rf_motion_now || rf_presence_now) {
    kenshiBubbleState = 2;
    kenshiJobState = 2; // track
  } else if (loss > 0.12f || kenshiVirtualNodes > 0) {
    kenshiBubbleState = 1;
    kenshiJobState = 4; // learn/virtual schedule
  } else {
    kenshiBubbleState = 0;
    kenshiJobState = 1; // watch
  }

  float conf = sync_hint * 0.55f + fit * 0.18f + (1.0f / (1.0f + loss)) * 0.27f;
  kenshiConfidence = constrain(kenshiConfidence * 0.86f + conf * 0.14f, 0.0f, 1.5f);

  int p = (int)(kenshiEventPower * 58.0f + kenshiConfidence * 42.0f + kenshiActiveNodes * 8);
  if (kenshiBubbleState == 3) p += 55;
  if (colonyJob.active) p += 18;
  if (tachyonFutureStress > 0.85f) p += (int)(tachyonFutureStress * 22.0f);
  if (tachyonRemoteCount > 0) p += 6;
  kenshiPriority = (uint8_t)constrain(p, 0, 255);
#endif
}

void onJanusKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi) {
#if JANUS_KENSHI_BUS_ENABLE
  if (kp.magic[0] != 'K' || kp.magic[1] != '2' || kp.version != 1) return;
  if (strncmp(kp.nodeId, "BlindEye", sizeof(kp.nodeId)) == 0) return;

  uint8_t slot = kenshiFindNodeSlot(kp.nodeId);
  JanusKenshiNode& n = kenshiNodes[slot];
  n.active = true;
  strlcpy(n.nodeId, kp.nodeId[0] ? kp.nodeId : "node", sizeof(n.nodeId));
  n.lastSeenMs = millis();
  n.seq = kp.seq;
  n.worldFlags = kp.worldFlags;
  n.entropy = isfinite(kp.entropy) ? kp.entropy : 0.0f;
  n.activity = isfinite(kp.activity) ? kp.activity : 0.0f;
  n.confidence = isfinite(kp.confidence) ? kp.confidence : 0.0f;
  n.sector = kp.sector % JANUS_KENSHI_SECTORS;
  n.predictedSector = kp.predictedSector % JANUS_KENSHI_SECTORS;
  n.jobState = kp.jobState;
  n.priority = kp.priority;
  n.rssi = rxRssi ? rxRssi : kp.rssi;
  n.flags = kp.flags;

  if (n.priority > 96 || (kp.flags & 0x03)) {
    kenshiTrainMarkov(kenshiSector, n.predictedSector, constrain(n.confidence, 0.05f, 1.0f));
  }

  kenshiRxPackets++;
  kenshiLastRxMs = millis();
#endif
}

void sendKenshiBubblePacket() {
#if JANUS_KENSHI_BUS_ENABLE
  JanusKenshiPacket kp{};
  kp.magic[0] = 'K';
  kp.magic[1] = '2';
  kp.version = 1;
  kp.flags = 0;
  if (kenshiBubbleState >= 2) kp.flags |= 0x01;
  if (kenshiBubbleState >= 3) kp.flags |= 0x02;
  if (kenshiVirtualNodes > 0) kp.flags |= 0x04;
  if (motionBasePresent) kp.flags |= 0x08; // Atomic Motion Base / servo planner present
  strlcpy(kp.nodeId, "BlindEye", sizeof(kp.nodeId));
  kp.seq = ++kenshiSeq;
  kp.worker_id = colonyWorkerId;
  kp.uptime_ms = millis();
  kp.activeBubbleNodes = kenshiActiveNodes;
  kp.virtualNodes = kenshiVirtualNodes;
  kp.worldFlags = kenshiWorldFlags;
  kp.sector = kenshiSector;
  kp.predictedSector = kenshiPredSector;
  kp.jobState = kenshiJobState;
  kp.priority = kenshiPriority;
  kp.rssi = (int8_t)wifi_rssi;
  kp.entropy = eyeLocalEntropy();
  kp.activity = activity;
  kp.confidence = kenshiConfidence;
  kp.values[0] = tmos_presence;
  kp.values[1] = tmos_motion;
  kp.values[2] = mic_rms;
  kp.values[3] = imu_shock;
  kp.values[4] = loss;
  kp.values[5] = fit + (motionBasePresent ? (float)motionBaseServoAngle / 180.0f : 0.0f);

  if (janusEyeEspNowSend("K2", &kp, sizeof(kp), true)) {
    kenshiTxPackets++;
    kenshiLastTxMs = millis();
  }
#endif
}

void kenshiBubbleTick() {
#if JANUS_KENSHI_BUS_ENABLE
  uint32_t now = millis();
  uint32_t interval = JANUS_KENSHI_BG_TX_MS;
  if (kenshiBubbleState >= 3 || kenshiPriority >= 170) interval = JANUS_KENSHI_ALERT_TX_MS;
  else if (kenshiBubbleState >= 2 || kenshiPriority >= 90) interval = JANUS_KENSHI_ACTIVE_TX_MS;

  if (now - kenshiLastTxMs >= interval) sendKenshiBubblePacket();
#endif
}

struct JanusKenshiSaveBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t sectors;
  uint8_t lastSector;
  uint8_t reserved[3];
  float markov[JANUS_KENSHI_SECTORS][JANUS_KENSHI_SECTORS];
};

void saveKenshiState() {
#if JANUS_KENSHI_BUS_ENABLE
  if (!kenshiStateDirty) return;
  JanusKenshiSaveBlob b{};
  b.magic = 0x4B324559UL; // K2EY
  b.version = 1;
  b.sectors = JANUS_KENSHI_SECTORS;
  b.lastSector = kenshiLastSector;
  memcpy(b.markov, kenshiMarkov, sizeof(kenshiMarkov));

  LittleFS.remove(JANUS_KENSHI_SAVE_FILE);
  File f = LittleFS.open(JANUS_KENSHI_SAVE_FILE, "w");
  if (!f) return;
  f.write((const uint8_t*)&b, sizeof(b));
  f.close();
  kenshiStateDirty = false;
#endif
}

void loadKenshiState() {
#if JANUS_KENSHI_BUS_ENABLE
  File f = LittleFS.open(JANUS_KENSHI_SAVE_FILE, FILE_READ);
  if (!f) {
    for (uint8_t i = 0; i < JANUS_KENSHI_SECTORS; ++i) {
      for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) {
        kenshiMarkov[i][j] = (i == j) ? 0.18f : 0.02f;
      }
    }
    return;
  }

  JanusKenshiSaveBlob b{};
  size_t got = f.read((uint8_t*)&b, sizeof(b));
  f.close();

  if (got != sizeof(b) || b.magic != 0x4B324559UL || b.version != 1 || b.sectors != JANUS_KENSHI_SECTORS) {
    for (uint8_t i = 0; i < JANUS_KENSHI_SECTORS; ++i) {
      for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) {
        kenshiMarkov[i][j] = (i == j) ? 0.18f : 0.02f;
      }
    }
    return;
  }

  memcpy(kenshiMarkov, b.markov, sizeof(kenshiMarkov));
  kenshiLastSector = b.lastSector % JANUS_KENSHI_SECTORS;
#endif
}



// ========================= JANUS TACHYON PROPHECY + EYE VISION =========================

float tachyonSeqGet(float* arr, uint8_t back, float fallback) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  if (tachyonSeqCount == 0) return fallback;
  if (back >= tachyonSeqCount) back = tachyonSeqCount - 1;
  int idx = (int)tachyonSeqPos - 1 - (int)back;
  while (idx < 0) idx += JANUS_TACHYON_SEQ_N;
  return arr[idx % JANUS_TACHYON_SEQ_N];
#else
  return fallback;
#endif
}

float tachyonWeightedMemory(float* arr, float fallback) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  if (tachyonSeqCount == 0) return fallback;
  float sum = 0.0f;
  float wsum = 0.0f;
  uint8_t n = min<uint8_t>(tachyonSeqCount, 8);
  for (uint8_t i = 0; i < n; ++i) {
    float w = 1.0f / (1.0f + i * 0.55f);
    sum += tachyonSeqGet(arr, i, fallback) * w;
    wsum += w;
  }
  return (wsum > 0.001f) ? (sum / wsum) : fallback;
#else
  return fallback;
#endif
}

uint8_t tachyonFindRemoteSlot(const char* nodeId) {
  uint8_t freeSlot = 255;
  uint8_t oldestSlot = 0;
  uint32_t oldestAge = 0;
  for (uint8_t i = 0; i < JANUS_TACHYON_REMOTE_N; ++i) {
    if (tachyonRemotes[i].active && nodeId && nodeId[0] && strncmp(tachyonRemotes[i].nodeId, nodeId, sizeof(tachyonRemotes[i].nodeId)) == 0) return i;
    if (!tachyonRemotes[i].active && freeSlot == 255) freeSlot = i;
    uint32_t age = millis() - tachyonRemotes[i].lastSeenMs;
    if (age > oldestAge) { oldestAge = age; oldestSlot = i; }
  }
  return (freeSlot != 255) ? freeSlot : oldestSlot;
}

void tachyonBlendRemoteProphecies() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  uint32_t now = millis();
  float p = 0.0f;
  float m = 0.0f;
  float sp = 0.0f;
  float wsum = 0.0f;
  uint8_t count = 0;

  for (uint8_t i = 0; i < JANUS_TACHYON_REMOTE_N; ++i) {
    JanusRemoteProphecyState& r = tachyonRemotes[i];
    if (!r.active) continue;
    uint32_t age = now - r.lastSeenMs;
    if (age > JANUS_TACHYON_REMOTE_TTL_MS) {
      r.active = false;
      continue;
    }

    float ageW = constrain(1.0f - (float)age / (float)JANUS_TACHYON_REMOTE_TTL_MS, 0.0f, 1.0f);
    float cW = constrain((float)r.confidence / 100.0f, 0.05f, 1.0f);
    float w = ageW * cW;
    p += r.pred_presence_1 * w;
    m += r.pred_motion_1 * w;
    sp += (r.future_stress + r.swarm_pressure) * 0.5f * w;
    wsum += w;
    count++;
  }

  tachyonRemoteCount = count;
  if (wsum > 0.001f) {
    tachyonRemotePresence = tachyonRemotePresence * 0.86f + (p / wsum) * 0.14f;
    tachyonRemoteMotion = tachyonRemoteMotion * 0.86f + (m / wsum) * 0.14f;
    tachyonSwarmPressure = tachyonSwarmPressure * 0.88f + constrain(sp / wsum, 0.0f, 2.0f) * 0.12f;
  } else {
    tachyonRemotePresence *= 0.96f;
    tachyonRemoteMotion *= 0.96f;
    tachyonSwarmPressure *= 0.94f;
  }
#endif
}

void tachyonPushSequence() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  tachyonSeqPresence[tachyonSeqPos] = constrain(tmos_presence, 0.0f, 3000.0f);
  tachyonSeqMotion[tachyonSeqPos] = constrain(tmos_motion, 0.0f, 2000.0f);
  tachyonSeqActivity[tachyonSeqPos] = constrain(activity, 0.0f, 20.0f);
  tachyonSeqPos = (tachyonSeqPos + 1) % JANUS_TACHYON_SEQ_N;
  if (tachyonSeqCount < JANUS_TACHYON_SEQ_N) tachyonSeqCount++;
#endif
}

void updateTachyonProphecy() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  tachyonBlendRemoteProphecies();

  float prevP = tachyonSeqGet(tachyonSeqPresence, 0, tmos_presence);
  float prevP2 = tachyonSeqGet(tachyonSeqPresence, 1, prevP);
  float prevM = tachyonSeqGet(tachyonSeqMotion, 0, tmos_motion);
  float prevM2 = tachyonSeqGet(tachyonSeqMotion, 1, prevM);

  float errP = fabsf(tachyonLastPredPresence1 - tmos_presence) / max(80.0f, max(tmos_presence, tachyonLastPredPresence1) + 1.0f);
  float errM = fabsf(tachyonLastPredMotion1 - tmos_motion) / max(60.0f, max(tmos_motion, tachyonLastPredMotion1) + 1.0f);
  tachyonLossEma = tachyonLossEma * 0.90f + constrain((errP + errM) * 0.5f, 0.0f, 5.0f) * 0.10f;

  tachyonPresenceConfidence = constrain(tachyonPresenceConfidence * 0.92f + (1.0f / (1.0f + errP * 2.5f)) * 0.08f, 0.0f, 1.0f);
  tachyonMotionConfidence = constrain(tachyonMotionConfidence * 0.92f + (1.0f / (1.0f + errM * 2.5f)) * 0.08f, 0.0f, 1.0f);

  float lr = 0.0045f;
  tachyonTrendGain = constrain(tachyonTrendGain + lr * ((errP + errM) < 0.45f ? 0.003f : -0.010f), 0.18f, 1.20f);
  tachyonMemoryGain = constrain(tachyonMemoryGain + lr * ((tachyonSeqCount > 6 && tachyonLossEma < 0.55f) ? 0.004f : -0.004f), 0.08f, 0.75f);
  tachyonRemoteGain = constrain(tachyonRemoteGain + lr * ((tachyonRemoteCount > 0 && tachyonLossEma < 0.75f) ? 0.004f : -0.003f), 0.00f, 0.45f);

  float trendP = constrain(tmos_presence - prevP2, -700.0f, 700.0f);
  float trendM = constrain(tmos_motion - prevM2, -500.0f, 500.0f);
  float memP = tachyonWeightedMemory(tachyonSeqPresence, tmos_presence);
  float memM = tachyonWeightedMemory(tachyonSeqMotion, tmos_motion);

  // Physarious-inspired tissue/energy gates: moving parts later will use these as servo caution.
  float balance = fabsf(acc_x) + fabsf(acc_y) + fabsf(acc_z - 1.0f);
  tachyonEnergy = tachyonEnergy * 0.86f + constrain(imu_shock * 0.35f + mic_rms * 14.0f + tmos_motion * 0.006f + balance * 0.20f, 0.0f, 8.0f) * 0.14f;
  tachyonLangerDrag = tachyonLangerDrag * 0.90f + constrain(balance * 0.20f + loss * 0.40f + tachyonEnergy * 0.06f, 0.0f, 2.0f) * 0.10f;
  tachyonPhysarumTrace = tachyonPhysarumTrace * 0.965f + constrain(tmos_motion / 700.0f + tmos_presence / 1600.0f, 0.0f, 2.0f) * 0.035f;
  if (tachyonPhysarumTrace < 0.025f) tachyonPhysarumTrace *= 0.65f;

  float drag = 1.0f / (1.0f + tachyonLangerDrag * 0.35f);
  float remoteP = tachyonRemotePresence * tachyonRemoteGain;
  float remoteM = tachyonRemoteMotion * tachyonRemoteGain;

  float nextP = tmos_presence * 0.56f + memP * tachyonMemoryGain + (tmos_presence + trendP * tachyonTrendGain) * 0.26f + remoteP * 0.18f;
  float nextM = tmos_motion * 0.54f + memM * tachyonMemoryGain + (tmos_motion + trendM * tachyonTrendGain) * 0.30f + remoteM * 0.18f;
  nextP *= drag;
  nextM *= drag;

  tachyonPredPresence1 = constrain(tachyonPredPresence1 * 0.68f + nextP * 0.32f, 0.0f, 3000.0f);
  tachyonPredMotion1 = constrain(tachyonPredMotion1 * 0.68f + nextM * 0.32f, 0.0f, 2000.0f);
  tachyonPredPresence2 = constrain(tachyonPredPresence2 * 0.74f + (tachyonPredPresence1 + trendP * 0.45f) * 0.26f, 0.0f, 3000.0f);
  tachyonPredMotion2 = constrain(tachyonPredMotion2 * 0.74f + (tachyonPredMotion1 + trendM * 0.45f) * 0.26f, 0.0f, 2000.0f);
  tachyonPredPresence3 = constrain(tachyonPredPresence3 * 0.80f + (tachyonPredPresence2 + trendP * 0.22f) * 0.20f, 0.0f, 3000.0f);
  tachyonPredMotion3 = constrain(tachyonPredMotion3 * 0.80f + (tachyonPredMotion2 + trendM * 0.22f) * 0.20f, 0.0f, 2000.0f);

  float pNorm = constrain(max(tachyonPredPresence1, tmos_presence) / 850.0f, 0.0f, 2.0f);
  float mNorm = constrain(max(tachyonPredMotion1, tmos_motion) / 420.0f, 0.0f, 2.0f);
  tachyonFutureStress = constrain(tachyonFutureStress * 0.84f + (pNorm * 0.34f + mNorm * 0.42f + tachyonSwarmPressure * 0.14f + tachyonLossEma * 0.10f) * 0.16f, 0.0f, 2.5f);

  float futurePower = max(tachyonPredPresence1 / 850.0f, tachyonPredMotion1 / 420.0f);
  if (futurePower > 0.95f) tachyonEventEtaMs = 450.0f + (1.0f - min(1.0f, futurePower - 0.95f)) * 650.0f;
  else if (max(tachyonPredPresence2 / 850.0f, tachyonPredMotion2 / 420.0f) > 0.95f) tachyonEventEtaMs = 1200.0f;
  else if (max(tachyonPredPresence3 / 850.0f, tachyonPredMotion3 / 420.0f) > 0.95f) tachyonEventEtaMs = 2200.0f;
  else tachyonEventEtaMs = 9999.0f;

  tachyonLastPredPresence1 = tachyonPredPresence1;
  tachyonLastPredMotion1 = tachyonPredMotion1;

  tachyonPushSequence();
#endif
}

void onJanusTachyonProphecy(const JanusTachyonProphecyPacket& tp, int8_t rxRssi) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  if (tp.magic[0] != 'T' || tp.magic[1] != 'P' || tp.version != 1) return;
  if (strncmp(tp.nodeId, "BlindEye", sizeof(tp.nodeId)) == 0) return;

  uint8_t slot = tachyonFindRemoteSlot(tp.nodeId);
  JanusRemoteProphecyState& r = tachyonRemotes[slot];
  r.active = true;
  strlcpy(r.nodeId, tp.nodeId[0] ? tp.nodeId : "node", sizeof(r.nodeId));
  r.lastSeenMs = millis();
  r.seq = tp.seq;
  r.sector = tp.sector % JANUS_KENSHI_SECTORS;
  r.predictedSector = tp.predictedSector % JANUS_KENSHI_SECTORS;
  r.confidence = tp.confidence;
  r.flags = tp.flags;
  r.presence_now = isfinite(tp.presence_now) ? tp.presence_now : 0.0f;
  r.motion_now = isfinite(tp.motion_now) ? tp.motion_now : 0.0f;
  r.pred_presence_1 = isfinite(tp.pred_presence_1) ? tp.pred_presence_1 : 0.0f;
  r.pred_motion_1 = isfinite(tp.pred_motion_1) ? tp.pred_motion_1 : 0.0f;
  r.pred_presence_2 = isfinite(tp.pred_presence_2) ? tp.pred_presence_2 : 0.0f;
  r.pred_motion_2 = isfinite(tp.pred_motion_2) ? tp.pred_motion_2 : 0.0f;
  r.pred_presence_3 = isfinite(tp.pred_presence_3) ? tp.pred_presence_3 : 0.0f;
  r.pred_motion_3 = isfinite(tp.pred_motion_3) ? tp.pred_motion_3 : 0.0f;
  r.future_stress = isfinite(tp.future_stress) ? tp.future_stress : 0.0f;
  r.swarm_pressure = isfinite(tp.swarm_pressure) ? tp.swarm_pressure : 0.0f;

  // Remote futures influence the local Markov gate only when sender is confident.
  if (tp.confidence > 45) kenshiTrainMarkov(kenshiSector, r.predictedSector, constrain((float)tp.confidence / 100.0f, 0.08f, 1.0f));

  tachyonRxPackets++;
  tachyonLastRxMs = millis();
  (void)rxRssi;
#endif
}

void sendTachyonProphecyPacket(bool force) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  uint32_t now = millis();
  uint32_t interval = (tachyonFutureStress > 0.95f || kenshiBubbleState >= 3) ? JANUS_TACHYON_TX_ALERT_MS : JANUS_TACHYON_TX_BG_MS;
  if (!force && now - tachyonLastTxMs < interval) return;

  JanusTachyonProphecyPacket tp{};
  tp.magic[0] = 'T'; tp.magic[1] = 'P';
  tp.version = 1;
  tp.flags = 0;
  if (tmos_presence_now) tp.flags |= 0x01;
  if (tmos_motion_now) tp.flags |= 0x02;
  if (tachyonFutureStress > 0.95f || kenshiBubbleState >= 3) tp.flags |= 0x04;
  if (tachyonRemoteCount > 0) tp.flags |= 0x08;
  strlcpy(tp.nodeId, "BlindEye", sizeof(tp.nodeId));
  tp.seq = ++tachyonSeq;
  tp.worker_id = colonyWorkerId;
  tp.uptime_ms = now;
  tp.horizon_ms = 2200;
  tp.sector = kenshiSector;
  tp.predictedSector = kenshiPredSector;
  tp.confidence = (uint8_t)constrain((int)((tachyonPresenceConfidence * 0.5f + tachyonMotionConfidence * 0.5f) * 100.0f), 0, 100);
  tp.jobState = kenshiJobState;
  tp.presence_now = tmos_presence;
  tp.motion_now = tmos_motion;
  tp.pred_presence_1 = tachyonPredPresence1;
  tp.pred_motion_1 = tachyonPredMotion1;
  tp.pred_presence_2 = tachyonPredPresence2;
  tp.pred_motion_2 = tachyonPredMotion2;
  tp.pred_presence_3 = tachyonPredPresence3;
  tp.pred_motion_3 = tachyonPredMotion3;
  tp.event_eta_ms = tachyonEventEtaMs;
  tp.future_stress = tachyonFutureStress;
  tp.swarm_pressure = tachyonSwarmPressure;

  if (janusEyeEspNowSend("TP", &tp, sizeof(tp), true)) {
    tachyonTxPackets++;
    tachyonLastTxMs = now;
  }
#else
  (void)force;
#endif
}

void tachyonProphecyTick() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  sendTachyonProphecyPacket(false);
#endif
}

struct JanusTachyonSaveBlob {
  uint32_t magic;
  uint16_t version;
  float trendGain;
  float memoryGain;
  float remoteGain;
  float pConfidence;
  float mConfidence;
};

void saveTachyonState() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  JanusTachyonSaveBlob b{};
  b.magic = 0x54505932UL; // TPY2
  b.version = 1;
  b.trendGain = tachyonTrendGain;
  b.memoryGain = tachyonMemoryGain;
  b.remoteGain = tachyonRemoteGain;
  b.pConfidence = tachyonPresenceConfidence;
  b.mConfidence = tachyonMotionConfidence;
  LittleFS.remove(JANUS_TACHYON_SAVE_FILE);
  File f = LittleFS.open(JANUS_TACHYON_SAVE_FILE, "w");
  if (!f) return;
  f.write((const uint8_t*)&b, sizeof(b));
  f.close();
#endif
}

void loadTachyonState() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  File f = LittleFS.open(JANUS_TACHYON_SAVE_FILE, FILE_READ);
  if (!f) return;
  JanusTachyonSaveBlob b{};
  size_t got = f.read((uint8_t*)&b, sizeof(b));
  f.close();
  if (got != sizeof(b) || b.magic != 0x54505932UL || b.version != 1) return;
  tachyonTrendGain = constrain(b.trendGain, 0.18f, 1.20f);
  tachyonMemoryGain = constrain(b.memoryGain, 0.08f, 0.75f);
  tachyonRemoteGain = constrain(b.remoteGain, 0.0f, 0.45f);
  tachyonPresenceConfidence = constrain(b.pConfidence, 0.0f, 1.0f);
  tachyonMotionConfidence = constrain(b.mConfidence, 0.0f, 1.0f);
#endif
}

bool eyeVisionTargetsThisEye(const JanusEyeVisionControlPacket& ec) {
#if JANUS_EYE_VISION_ENABLE
  if (ec.magic[0] != 'E' || ec.magic[1] != 'C' || ec.version != 1) return false;
  if (ec.target[0] == 0) return true;
  if (!strcasecmp(ec.target, "all")) return true;
  if (!strcasecmp(ec.target, "*")) return true;
  if (!strcasecmp(ec.target, "BlindEye")) return true;
  if (!strcasecmp(ec.target, "atom_s3r_blind_eye")) return true;
  return strstr(ec.target, "BlindEye") || strstr(ec.target, "EYE");
#else
  return false;
#endif
}

void onJanusEyeVisionControl(const JanusEyeVisionControlPacket& ec) {
#if JANUS_EYE_VISION_ENABLE
  if (!eyeVisionTargetsThisEye(ec)) {
    eyeVisionControlsIgnored++;
    return;
  }
  eyeVisionControlsRx++;
  eyeVisionEnabled = ec.enable != 0;
  eyeVisionFrameMs = ec.frameMs ? constrain((int)ec.frameMs, 90, 800) : JANUS_EYE_VISION_DEFAULT_FRAME_MS;
  eyeVisionLastControlMs = millis();
  strlcpy(eyeVisionSource, ec.source[0] ? ec.source : "Core2Home", sizeof(eyeVisionSource));
  if (eyeVisionEnabled) eyeVisionLastFrameMs = 0;
#endif
}

uint8_t eyeVisionSectorIntensity(uint8_t x, uint8_t y, uint8_t sector, float power) {
  // 8 sector ray field. This is an aperture image, not fake camera pixels:
  // it visualizes scalar TMOS/motion plus predicted direction for Core2.
  static const int8_t sx[8] = {4, 6, 7, 6, 4, 1, 0, 1};
  static const int8_t sy[8] = {0, 1, 4, 6, 7, 6, 4, 1};
  int dx = (int)x - sx[sector & 7];
  int dy = (int)y - sy[sector & 7];
  float d2 = (float)(dx * dx + dy * dy);
  float v = power * 255.0f / (1.0f + d2 * 0.42f);
  return (uint8_t)constrain((int)v, 0, 255);
}

void sendEyeVisionFrame() {
#if JANUS_EYE_VISION_ENABLE
  if (!eyeVisionEnabled) return;
  JanusEyeFramePacket ef{};
  ef.magic[0] = 'E'; ef.magic[1] = 'F';
  ef.version = 1;
  ef.width = JANUS_EYE_VISION_W;
  ef.height = JANUS_EYE_VISION_H;
  ef.seq = ++eyeVisionSeq;
  ef.min_x10 = 0;
  ef.max_x10 = (int16_t)constrain((int)(max(tmos_presence, tmos_motion) * 10.0f), 0, 32767);
  ef.flags = 0x04; // synthetic aperture from real scalar sensor data
  if (tmos_motion_now || (tachyonPredMotion1 > 38.0f && tmos_motion_memory > 0.45f)) ef.flags |= 0x01;
  if (tmos_presence_now || (tachyonPredPresence1 > 46.0f && tmos_presence_memory > 0.45f)) ef.flags |= 0x02;
  if (tmos_presence_now) ef.flags |= JANUS_EYE_FLAG_PRESENCE_NOW;
  if (tmos_motion_now) ef.flags |= JANUS_EYE_FLAG_MOTION_NOW;

  float pPower = constrain(max(tmos_presence, tachyonPredPresence1 * tmos_presence_memory) / 260.0f, 0.0f, 1.0f);
  float mPower = constrain(max(tmos_motion, tachyonPredMotion1 * tmos_motion_memory) / 180.0f, 0.0f, 1.0f);
  if (!tmos_presence_now && !tmos_motion_now && tmos_occupancy < 0.18f) {
    pPower *= 0.18f;
    mPower *= 0.18f;
  }
  float base = constrain(0.04f + pPower * 0.55f + mPower * 0.35f + tachyonFutureStress * 0.03f + tmos_occupancy * 0.05f, 0.0f, 1.0f);
  uint8_t curSector = kenshiSector & 7;
  uint8_t nextSector = kenshiPredSector & 7;

  for (uint8_t y = 0; y < JANUS_EYE_VISION_H; ++y) {
    for (uint8_t x = 0; x < JANUS_EYE_VISION_W; ++x) {
      float cx = (float)x - 3.5f;
      float cy = (float)y - 3.5f;
      float r2 = cx * cx + cy * cy;
      int aperture = (int)(base * 120.0f / (1.0f + r2 * 0.12f));
      int rayNow = eyeVisionSectorIntensity(x, y, curSector, mPower) / 2;
      int rayNext = eyeVisionSectorIntensity(x, y, nextSector, constrain(tachyonFutureStress, 0.0f, 1.0f)) / 3;
      int pulse = (int)(18.0f + 12.0f * sinf((float)millis() * 0.004f + (float)(x + y)));
      ef.pixels[y * JANUS_EYE_VISION_W + x] = (uint8_t)constrain(aperture + rayNow + rayNext + pulse, 0, 255);
    }
  }

  if (janusEyeEspNowSend("E/F", &ef, sizeof(ef), true)) {
    eyeVisionFramesTx++;
    eyeVisionLastFrameMs = millis();
  }
#endif
}

void eyeVisionTick() {
#if JANUS_EYE_VISION_ENABLE
  uint32_t now = millis();
  if (eyeVisionEnabled && now - eyeVisionLastControlMs > JANUS_EYE_VISION_IDLE_MS) {
    eyeVisionEnabled = false;
  }
  if (!eyeVisionEnabled) return;
  if (now - eyeVisionLastFrameMs >= eyeVisionFrameMs) sendEyeVisionFrame();
#endif
}



// ========================= JANUS RF FUSION / RUVIEW-LITE =========================
// Stable Arduino layer: RSSI drift and ESP-NOW RX signal pressure.
// CSI phase processing will be a later compile-time experimental layer.

bool rfLiteValidRssi(int v) {
  return v < -5 && v > -126;
}

float rfLiteFusionScore() {
#if JANUS_RF_LITE_ENABLE
  return constrain(rf_presence_score + rf_motion_energy * 0.070f + rf_packet_pressure * 0.35f, 0.0f, 2.8f);
#else
  return 0.0f;
#endif
}

void rfLiteOnPacketRssi(int8_t rssi) {
#if JANUS_RF_LITE_ENABLE
  if (!rfLiteValidRssi((int)rssi)) return;
  rf_rx_packets++;
  rf_last_packet_ms = millis();
  if (rf_last_packet_rssi != -127) {
    rf_last_packet_drift = fabsf((float)rssi - (float)rf_last_packet_rssi);
  }
  rf_last_packet_rssi = rssi;
  float packetKick = constrain(rf_last_packet_drift / 12.0f, 0.0f, 1.5f);
  rf_packet_pressure = constrain(rf_packet_pressure * 0.82f + packetKick * 0.18f, 0.0f, 2.0f);
#else
  (void)rssi;
#endif
}

void rfLiteTick(uint32_t now) {
#if JANUS_RF_LITE_ENABLE
  if (now - rf_last_sample_ms < JANUS_RF_LITE_SAMPLE_MS) return;
  rf_last_sample_ms = now;

  int rssiNow = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
  if (!rfLiteValidRssi(rssiNow)) {
    rf_presence_score *= 0.94f;
    rf_motion_energy *= 0.90f;
    rf_entropy *= 0.92f;
    rf_packet_pressure *= 0.96f;
    rf_presence_now = false;
    rf_motion_now = false;
    return;
  }

  if (!rf_ready || rf_rssi_ema < -126.0f || rf_rssi_baseline < -126.0f) {
    rf_rssi_ema = (float)rssiNow;
    rf_rssi_baseline = (float)rssiNow;
    rf_rssi_noise = 2.8f;
    rf_abs_drift = 0.0f;
    rf_motion_energy = 0.0f;
    rf_presence_score = 0.0f;
    rf_entropy = 0.0f;
    rf_packet_pressure = 0.0f;
    rf_ready = true;
    rf_samples = 1;
    return;
  }

  float prevEma = rf_rssi_ema;
  rf_rssi_ema = rf_rssi_ema * 0.86f + (float)rssiNow * 0.14f;
  float instantStep = fabsf((float)rssiNow - prevEma);
  rf_abs_drift = fabsf(rf_rssi_ema - rf_rssi_baseline);

  bool hot = (instantStep > JANUS_RF_LITE_MOTION_LEVEL_DB) ||
             (rf_abs_drift > max(4.5f, rf_rssi_noise * 1.85f)) ||
             (rf_packet_pressure > 0.65f);

  float baseAlpha = hot ? JANUS_RF_LITE_BASELINE_ALPHA_HOT : JANUS_RF_LITE_BASELINE_ALPHA_QUIET;
  rf_rssi_baseline = rf_rssi_baseline * (1.0f - baseAlpha) + rf_rssi_ema * baseAlpha;

  if (!hot) {
    rf_rssi_noise = rf_rssi_noise * (1.0f - JANUS_RF_LITE_NOISE_ALPHA) + instantStep * JANUS_RF_LITE_NOISE_ALPHA;
  } else {
    rf_rssi_noise = rf_rssi_noise * 0.996f + min(instantStep, rf_rssi_noise) * 0.004f;
  }
  rf_rssi_noise = constrain(rf_rssi_noise, 1.2f, 12.0f);

  float motionNorm = constrain((instantStep + rf_last_packet_drift * 0.55f) / max(JANUS_RF_LITE_MOTION_LEVEL_DB, 0.5f), 0.0f, 5.0f);
  float presenceNorm = constrain(rf_abs_drift / max(rf_rssi_noise * 2.2f + 1.0f, 1.0f), 0.0f, 4.0f);
  float packetAge = (rf_last_packet_ms == 0) ? 99999.0f : (float)((now >= rf_last_packet_ms) ? (now - rf_last_packet_ms) : 0UL);
  float packetFresh = constrain(1.0f - packetAge / (float)JANUS_RF_LITE_PACKET_TTL_MS, 0.0f, 1.0f);

  rf_motion_energy = constrain(rf_motion_energy * 0.78f + motionNorm * 0.22f + rf_packet_pressure * 0.08f, 0.0f, 12.0f);
  rf_presence_score = constrain(rf_presence_score * 0.88f + (presenceNorm + packetFresh * rf_packet_pressure * 0.35f) * 0.12f, 0.0f, 3.0f);
  rf_entropy = constrain(rf_entropy * 0.86f + (motionNorm * 0.22f + presenceNorm * 0.30f + rf_packet_pressure * 0.24f) * 0.14f, 0.0f, 4.0f);

  rf_presence_now = rf_presence_score > JANUS_RF_LITE_PRESENCE_LEVEL;
  rf_motion_now = rf_motion_energy > 1.05f || motionNorm > 1.65f;
  if (rf_entropy > JANUS_RF_LITE_ANOMALY_LEVEL && (rf_motion_now || rf_presence_now)) rf_anomaly_count++;

  rf_packet_pressure *= 0.985f;
  rf_last_packet_drift *= 0.92f;
  rf_samples++;
#else
  (void)now;
#endif
}

bool tmosWarmupActive(uint32_t now) {
  return tmosWarmupUntilMs && now < tmosWarmupUntilMs;
}

void rfLiteDebugTick(uint32_t now, bool force) {
#if JANUS_RF_LITE_ENABLE
  if (!force && now - rf_last_debug_ms < JANUS_RF_LITE_DEBUG_MS) return;
  rf_last_debug_ms = now;
  uint32_t packetAge = rf_last_packet_ms ? ((now >= rf_last_packet_ms) ? (now - rf_last_packet_ms) : 0UL) : 999999UL;
  uint32_t warmLeft = tmosWarmupActive(now) ? (tmosWarmupUntilMs - now) : 0;
  Serial.printf("[EYE/RF] ready=%u rssi=%d ema=%.1f base=%.1f noise=%.1f drift=%.1f P=%.2f M=%.2f entropy=%.2f pkt=%lu age=%lums pr=%.2f last=%d anomaly=%lu warmup=%lus lane=%s/s%u stride=%lu arm=%u tail=%lu bestN=%08lX\n",
                rf_ready ? 1 : 0,
                (int)((WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127),
                rf_rssi_ema,
                rf_rssi_baseline,
                rf_rssi_noise,
                rf_abs_drift,
                rf_presence_score,
                rf_motion_energy,
                rf_entropy,
                (unsigned long)rf_rx_packets,
                (unsigned long)packetAge,
                rf_packet_pressure,
                (int)rf_last_packet_rssi,
                (unsigned long)rf_anomaly_count,
                (unsigned long)(warmLeft / 1000UL),
                colonyMinerLaneName(colonyJob.minerLane),
                (unsigned)colonyJob.minerSector,
                (unsigned long)colonyJob.minerStride,
                (unsigned)colonyJob.minerStrideArm,
                (unsigned long)colonyMinerTailHits,
                (unsigned long)colonyMinerBestNonce);
#else
  (void)now; (void)force;
#endif
}

// ========================= JANUS COLONY EYE HOOKS =========================
float eyeLocalEntropy() {
  float agentEntropy = (float)(colonyAgentEntropySeed & 0xFFFF) / 65535.0f;
  return constrain(tmos_presence * 0.002f + tmos_motion * 0.004f + mic_rms * 20.0f +
                   mag_norm * 0.010f + imu_shock * 0.20f + loss * 1.5f +
                   rf_entropy * 0.85f + rf_motion_energy * 0.06f + rf_presence_score * 0.35f +
                   agentEntropy * 0.75f + (float)colonyAgentLevel * 0.10f + tachyonFutureStress * 0.45f,
                   0.0f, 9999.0f);
}

void onJanusHeartbeat(const JanusColonyPacket& pkt) {}
void onJanusEntropy(const EntropyReport& er, const void* opt) {}
void onJanusEntropyV2(const EntropyReportV2& er2) {}

void sendNodeHeartbeat() {
  JanusColonyPacket pkt{};
  memcpy(pkt.magic, "JANUS", 6);
  strlcpy(pkt.nodeId, "BlindEye", sizeof(pkt.nodeId));
  strlcpy(pkt.role, "EYE_BLIND", sizeof(pkt.role));
  pkt.seq = ++colonySeq;
  pkt.hashRate = colonyRemoteHashrate;
  pkt.shares = colonyRemoteShares;
  pkt.rejects = colonyRemoteRejects;
  pkt.bestBits = colonyBestBits;
  pkt.diff = 0.0f;
  pkt.targetBits = colonyTargetBits;
  pkt.aiBatch = effectiveColonyRemoteBatch();

  uint8_t localHint = (loss > 0.25f || kenshiBubbleState >= 3 || tachyonFutureStress > 1.20f) ? 2 : ((sync_hint > 0.85f || kenshiPriority > 120 || tachyonFutureStress > 0.70f) ? 3 : 1);
  if (millis() - colonyAgentLastRewardMs < COLONY_AGENT_REWARD_VISIBLE_MS && colonyAgentHint > localHint) {
    localHint = colonyAgentHint;
  }
  pkt.aiHint = localHint;
  pkt.jobAgeMs = colonyJob.active ? (millis() - colonyJob.receivedAt) : 0xFFFFFFFFUL;
  pkt.rssi = colonyLastRssi;
  pkt.uptime = millis();
  janusEyeEspNowSend("HB", &pkt, sizeof(pkt), true);
}

void sendNodeEntropy() {
  static uint32_t lastEntropyDbg = 0;
  if (millis() - lastEntropyDbg > 5000) {
    lastEntropyDbg = millis();
    Serial.printf("[COLONY] TX entropy worker=%u\n", colonyWorkerId);
  }
  static uint32_t dbgLastEntropyTx = 0;
  EntropyReport er{};
  er.magic[0] = 'E'; er.magic[1] = 'R';
  er.worker_id = colonyWorkerId;
  er.local_entropy = eyeLocalEntropy();
  er.sensor_flags = 0xE7; // mic + TMOS + IMU/mag + Kenshi + JANUS event/motion-base metadata
  er.values[0] = mic_rms;
  er.values[1] = tmos_presence;
  er.values[2] = mag_norm;
  er.values[3] = rf_ready ? rf_presence_score : (motionBasePresent ? (float)motionBaseServoAngle : loss);
  janusEyeEspNowSend("E/R", &er, sizeof(er), true);

  EntropyReportV2 er2{};
  er2.magic[0] = 'E'; er2.magic[1] = '2';
  er2.worker_id = colonyWorkerId;
  strlcpy(er2.nodeId, "BlindEye", sizeof(er2.nodeId));
  er2.local_entropy = er.local_entropy;
  er2.prediction_error = loss;
  er2.sync_hint = sync_hint;
  er2.fit = fit;
  er2.sensor_flags = er.sensor_flags;
  er2.values[0] = mic_rms;
  er2.values[1] = tmos_presence;
  er2.values[2] = tmos_motion;
  er2.values[3] = mag_norm;
  er2.values[4] = imu_shock;
  er2.values[5] = activity;
  er2.values[6] = pred_activity;
  er2.values[7] = rf_entropy;       // v2.12: RF fusion entropy / radio anomaly score
  er2.uptime_ms = millis();
  janusEyeEspNowSend("E2", &er2, sizeof(er2), true);
}

// ========================= HEADLESS STATUS =========================
// Blind EYE has no physical screen. There is no display output.
// Status is exposed through ESP-NOW heartbeat/entropy packets and optional Serial debug.

void printHeadlessStatus() {
  Serial.printf("[EYE] mode=%s wifi=%s rssi=%d act=%.2f pred=%.2f loss=%.3f fit=%.2f diag=%s status=%s\n",
                colonyMode,
                WiFi.status() == WL_CONNECTED ? "OK" : "OFF",
                wifi_rssi,
                activity,
                pred_activity,
                loss,
                fit,
                diagLine.c_str(),
                statusLine.c_str());

  Serial.printf("[EYE] focus rawP=%d rawM=%d dP=%.1f dM=%.1f base=%.1f/%.1f noise=%.1f/%.1f gain=%.2f conf=%.2f\n",
                raw_presence, raw_motion,
                tmos_presence_delta, tmos_motion_delta,
                tmos_presence_baseline, tmos_motion_baseline,
                tmos_presence_noise, tmos_motion_noise,
                tmos_focus_gain, tmos_focus_confidence);

  Serial.printf("[EYE] v29I truth P/M=%u/%u occ=%.2f mem=%.2f/%.2f ghost=%.2f bad=%u validAgo=%lums warmup=%lus flags=0x%02X\n",
                tmos_presence_now ? 1 : 0,
                tmos_motion_now ? 1 : 0,
                tmos_occupancy,
                tmos_presence_memory,
                tmos_motion_memory,
                tmos_ghost_score,
                (unsigned)tmos_bad_frames,
                (unsigned long)(millis() - tmos_last_valid_ms),
                (unsigned long)(tmosWarmupActive(millis()) ? (tmosWarmupUntilMs - millis()) / 1000UL : 0UL),
                (unsigned)((tmos_presence_now ? JANUS_EYE_FLAG_PRESENCE_NOW : 0) |
                           (tmos_motion_now ? JANUS_EYE_FLAG_MOTION_NOW : 0)));

  rfLiteDebugTick(millis(), true);

  Serial.printf("[EYE] miner H=%lu best=%lu target=%u tickets=%lu jobs=%lu done=%lu exp=%lu lane=%s/s%u stride=%lu arm=%u switches=%lu tail=%lu bestN=%08lX\n",
                (unsigned long)colonyRemoteHashrate,
                (unsigned long)colonyBestBits,
                (unsigned)colonyTargetBits,
                (unsigned long)colonyRemoteShares,
                (unsigned long)colonyJobsSeen,
                (unsigned long)colonyJobsDone,
                (unsigned long)colonyJobsExpired,
                colonyMinerLaneName(colonyJob.minerLane),
                (unsigned)colonyJob.minerSector,
                (unsigned long)colonyJob.minerStride,
                (unsigned)colonyJob.minerStrideArm,
                (unsigned long)colonyMinerLaneSwitches,
                (unsigned long)colonyMinerTailHits,
                (unsigned long)colonyMinerBestNonce);

  Serial.printf("[EYE] agent rewards=%lu aok=%lu lvl=%u hint=%u pts=%u batch=%u score=%.1f predH=%.1f err=%.3f entropy=%04lX\n",
                (unsigned long)colonyAgentRewardsRx,
                (unsigned long)colonyAgentShareRewardsRx,
                (unsigned)colonyAgentLevel,
                (unsigned)colonyAgentHint,
                (unsigned)colonyAgentRewardPoints,
                (unsigned)effectiveColonyRemoteBatch(),
                colonyAgentScore,
                colonyAgentPredictedHash,
                colonyAgentPredictionError,
                (unsigned long)(colonyAgentEntropySeed & 0xFFFF));

  Serial.printf("[EYE] kenshi bubble=%u job=%u sector=%u->%u prio=%u conf=%.2f active=%u virtual=%u rx=%lu tx=%lu flags=0x%08lX\n",
                (unsigned)kenshiBubbleState,
                (unsigned)kenshiJobState,
                (unsigned)kenshiSector,
                (unsigned)kenshiPredSector,
                (unsigned)kenshiPriority,
                kenshiConfidence,
                (unsigned)kenshiActiveNodes,
                (unsigned)kenshiVirtualNodes,
                (unsigned long)kenshiRxPackets,
                (unsigned long)kenshiTxPackets,
                (unsigned long)kenshiWorldFlags);

  Serial.printf("[EYE] tachyon P %.0f/%.0f/%.0f M %.0f/%.0f/%.0f eta=%.0f stress=%.2f conf=%.2f/%.2f rem=%u TP rx/tx=%lu/%lu vision=%u frames=%lu ctrl=%lu\n",
                tachyonPredPresence1, tachyonPredPresence2, tachyonPredPresence3,
                tachyonPredMotion1, tachyonPredMotion2, tachyonPredMotion3,
                tachyonEventEtaMs, tachyonFutureStress, tachyonPresenceConfidence, tachyonMotionConfidence,
                (unsigned)tachyonRemoteCount,
                (unsigned long)tachyonRxPackets, (unsigned long)tachyonTxPackets,
                eyeVisionEnabled ? 1 : 0,
                (unsigned long)eyeVisionFramesTx,
                (unsigned long)eyeVisionControlsRx);

  Serial.printf("[EYE] blackboard ev=%lu pol=%lu mood=%s raw=%s radio=%u sensor=%u smoothDrop=%lu order=%s | motionBase present=%u power=%u armed=%u write=%u angle=%d target=%d mv=%d curRaw=%d i2cErr=%lu servoWr=%lu\n",
                (unsigned long)janusEventSeq,
                (unsigned long)janusPolicyRx,
                janusMoodName(janusPolicyMood),
                janusMoodName(janusPolicyRawLastMood),
                (unsigned)janusPolicyRadioRate,
                (unsigned)janusPolicySensorRate,
                (unsigned long)janusPolicySmoothedDrops,
                janusPolicyOrder,
                motionBasePresent ? 1 : 0,
                motionBasePowerPresent ? 1 : 0,
                motionBaseArmed ? 1 : 0,
                (unsigned)JANUS_MOTION_BASE_WRITE_ENABLE,
                (int)motionBaseServoAngle,
                (int)motionBaseTargetAngle,
                (int)motionBaseBusMv,
                (int)motionBaseCurrentRaw,
                (unsigned long)motionBaseI2cErrors,
                (unsigned long)motionBaseServoWrites);

  Serial.printf("[EYE] semantic episodes=%u memTx=%lu needTx=%lu doneTx=%lu ss=%lu/%lu lastEpAgo=%lums ghost=%.2f ghostSince=%lums warmup=%lus motionBase=%u/%u bus=%dmV write=%u\n",
                (unsigned)janusEyeEpisodeCount,
                (unsigned long)janusEyeAiMemoryTx,
                (unsigned long)janusEyeTaskNeedTx,
                (unsigned long)janusEyeTaskDoneTx,
                (unsigned long)janusEyeSwarmSenseTx,
                (unsigned long)janusEyeSwarmSenseFail,
                (unsigned long)(janusEyeLastEpisodeMs ? millis() - janusEyeLastEpisodeMs : 0),
                tmos_ghost_score,
                (unsigned long)(tmosGhostHighSinceMs ? millis() - tmosGhostHighSinceMs : 0UL),
                (unsigned long)(tmosWarmupActive(millis()) ? (tmosWarmupUntilMs - millis()) / 1000UL : 0UL),
                motionBasePresent ? 1 : 0,
                motionBasePowerPresent ? 1 : 0,
                (int)motionBaseBusMv,
                (unsigned)JANUS_MOTION_BASE_WRITE_ENABLE);

  Serial.printf("[EYE] radio txOk=%lu txFail=%lu lastErr=%d lastTag=%s peerCh=%u rebuilds=%lu K2tx=%lu TPtx=%lu EFtx=%lu pol=%lu\n",
                (unsigned long)colonyTxOk,
                (unsigned long)colonyTxFail,
                (int)colonyLastTxErr,
                colonyLastTxTag,
                (unsigned)colonyPeerChannel,
                (unsigned long)colonyPeerRebuilds,
                (unsigned long)kenshiTxPackets,
                (unsigned long)tachyonTxPackets,
                (unsigned long)eyeVisionFramesTx,
                (unsigned long)janusPolicyRx);
}

// ========================= MAIN =========================

void readSensors() {
  readIMUClassic();
  readTMOS();
  mic_rms = readMicRms();
  wifi_rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
  rfLiteTick(millis());
  updateMiniGPT();
  updateTachyonProphecy();
  updateKenshiVirtualWorld();
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);  // Used for IMU/buttons/power only. Blind EYE is headless: no display output.
  Serial.begin(115200);
  Serial.println("JANUS Blind Eye v2.14D ROBOZOMBIE PASSIVE-S MODE S1/S2/S4 / baseless fallback / HEADLESS");

  LittleFS.begin(true);
  loadState();
  loadModel();
  loadKenshiState();
  loadTachyonState();

  janusSelectGroveBus(true);
  Serial.printf("[I2C] WireMux ready: Grove/TMOS SDA=%u SCL=%u, MotionBase SDA=%u SCL=%u addr=0x%02X\n",
                (unsigned)GROVE_SDA_PIN, (unsigned)GROVE_SCL_PIN,
                (unsigned)JANUS_MOTION_BASE_SDA_PIN, (unsigned)JANUS_MOTION_BASE_SCL_PIN,
                (unsigned)JANUS_MOTION_BASE_I2C_ADDR);
  initIMU();
  initMotionBase();
  janusSelectGroveBus(true);
  initTMOS();
  initMicI2S();
  initWiFi(true);
  initColonyNow();

  janusEyeBootMs = millis();
  tmosWarmupUntilMs = janusEyeBootMs + JANUS_TMOS_WARMUP_MS;
  janusPolicyLastMoodChangeMs = janusEyeBootMs;
  Serial.printf("[EYE/TMOS] warmup=%lus softSettle=%.3f/%.3f outScale=%.2f jump=%.0f ghost_task_gate=level%.2f hold%lus cooldown%lus\n",
                (unsigned long)(JANUS_TMOS_WARMUP_MS / 1000UL),
                JANUS_TMOS_WARMUP_SETTLE_ALPHA,
                JANUS_TMOS_WARMUP_SOFT_ALPHA,
                JANUS_TMOS_WARMUP_OUTPUT_SCALE,
                JANUS_TMOS_BASELINE_JUMP_LEVEL,
                JANUS_GHOST_TASKNEED_LEVEL,
                (unsigned long)(JANUS_GHOST_TASKNEED_HOLD_MS / 1000UL),
                (unsigned long)(JANUS_GHOST_TASKNEED_COOLDOWN_MS / 1000UL));

#if JANUS_EYE_RECALIBRATE_ON_BOOT
  // v2.9I: do not trust old saved TMOS baseline. Start empty-room truth each boot.
  calibrated = false;
  tmos_focus_ready = false;
  tmos_presence_memory = 0.0f;
  tmos_motion_memory = 0.0f;
  tmos_occupancy = 0.0f;
  tmos_ghost_score = 0.0f;
  tmos_presence_now = false;
  tmos_motion_now = false;
  if (tmos_ready) {
    calibrateTMOS();
  }
#else
  if (!calibrated && tmos_ready) {
    calibrateTMOS();
  }
#endif

  statusLine = M5.Imu.isEnabled() ? "imu ready + rf fusion + kenshi tachyon + blackboard" : "imu disabled";
  janusEmitEyeEvent(JE_BOOT, 92, 35,
                    (int16_t)(tmos_ready ? 1 : 0),
                    (int16_t)(motionBasePresent ? 1 : 0),
                    motionBaseBusMv,
                    (int16_t)(JANUS_MOTION_BASE_WRITE_ENABLE ? 1 : 0),
                    janusHash16("boot"), janusHash16("blind_eye"), 15000UL);
  motionBaseSendStatusEvent(true);
  motionBaseSendPowerPacket(true);
  janusEyeEmitTaskDone(90, "eye_boot_ready",
                       (int16_t)(tmos_ready ? 1 : 0),
                       (int16_t)(motionBasePresent ? 1 : 0),
                       motionBaseBusMv,
                       (int16_t)(motionBasePowerPresent ? 1 : 0));
  janusEyeSwarmSenseTick(millis(), true);
}

void loop() {
  M5.update();
  handleRoboZombieSerial();
  unsigned long now = millis();

  if (now - lastSensorAt >= janusPolicySensorIntervalMs) {
    lastSensorAt = now;
    readSensors();
  }

  // HTTP legacy fully disabled: no sendTelemetry(), no fetchCommand().
  // Blind Eye communicates with Beacon/Buzz only through ESP-NOW colony packets.

  if (now - lastDebugAt >= HEADLESS_DEBUG_INTERVAL_MS) {
    lastDebugAt = now;
    printHeadlessStatus();
  }

  if (now - lastSaveAt >= SAVE_INTERVAL_MS) {
    lastSaveAt = now;
    saveModel();
    saveState();
    saveKenshiState();
    saveTachyonState();
  }

  colonyTick();
  motionBaseTick();
  motionBaseSendPowerPacket(false);
  janusEventTick(false);
  janusEyeSwarmSenseTick(now, false);
  rfLiteDebugTick(now, false);

  if (WiFi.status() != WL_CONNECTED) {
    initWiFi(false);
  }

  delay(1);
}