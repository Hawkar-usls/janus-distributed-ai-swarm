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

