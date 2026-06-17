#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>
#include <math.h>
#include <Preferences.h>

// ========================= JANUS DUAL SERIAL BRIDGE =========================
// ESP32-S3 camera/dev boards often have TWO USB-C ports:
//   - native USB CDC/JTAG
//   - UART bridge on GPIO43/44
// Arduino "USB CDC On Boot" decides where plain Serial goes.
// This bridge mirrors every log line to BOTH native USB Serial and UART0,
// so the monitor works no matter which USB-C port is connected.
#ifndef JANUS_UART0_TX_PIN
#define JANUS_UART0_TX_PIN 43
#endif
#ifndef JANUS_UART0_RX_PIN
#define JANUS_UART0_RX_PIN 44
#endif
// v1.12: default OFF to stop the bright blue USB-UART activity LED from blinking.
// If your Serial Monitor is empty on your chosen USB-C port, set this to 1
// or plug into the native USB CDC port.
#ifndef JANUS_UART0_MIRROR_ENABLE
#define JANUS_UART0_MIRROR_ENABLE 0
#endif
// v1.12: read commands from the UART0 USB-C even when full mirror is OFF.
// This lets the UART-port button/serial commands work without constantly blinking
// the blue USB-UART TX LED.
#ifndef JANUS_UART0_INPUT_ENABLE
#define JANUS_UART0_INPUT_ENABLE 1
#endif
// v1.12: UART0 must stay 100% quiet while full-log mode is OFF.
// No sparse status lines are emitted, so the blue USB-UART TX LED does not blink.
// Hold BOOT ~3s or send 'u' to enable full UART0 logs when needed.
#ifndef JANUS_UART0_STATUS_ENABLE
#define JANUS_UART0_STATUS_ENABLE 0
#endif
#ifndef JANUS_UART0_STATUS_MS
#define JANUS_UART0_STATUS_MS 5000UL
#endif

HardwareSerial JanusDebugUART(0);

// Runtime UART0 full-log switch. Default is OFF so the bright blue USB-UART
// activity LED stays calm. Hold BOOT for ~3 seconds or send serial command U
// to turn full UART0 logs on/off without reflashing.
bool janusUart0FullLog = (JANUS_UART0_MIRROR_ENABLE != 0);

class JanusDualSerialClass {
public:
  void begin(unsigned long baud) {
    ::Serial.begin(baud);
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE || JANUS_UART0_STATUS_ENABLE
    JanusDebugUART.begin(baud, SERIAL_8N1, JANUS_UART0_RX_PIN, JANUS_UART0_TX_PIN);
#endif
  }
  void setDebugOutput(bool en) {
    ::Serial.setDebugOutput(en);
  }
  operator bool() const {
    return true;
  }
  int available() {
    int a = ::Serial.available();
    if (a > 0) return a;
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE
    return JanusDebugUART.available();
#else
    return 0;
#endif
  }
  int read() {
    if (::Serial.available() > 0) return ::Serial.read();
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE
    return JanusDebugUART.read();
#else
    return -1;
#endif
  }
  size_t write(uint8_t c) {
    size_t a = ::Serial.write(c);
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE || JANUS_UART0_STATUS_ENABLE
    if (janusUart0FullLog) {
      size_t b = JanusDebugUART.write(c);
      return a ? a : b;
    }
#endif
    return a;
  }
  size_t write(const uint8_t *buf, size_t size) {
    size_t a = ::Serial.write(buf, size);
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE || JANUS_UART0_STATUS_ENABLE
    if (janusUart0FullLog) {
      size_t b = JanusDebugUART.write(buf, size);
      return a ? a : b;
    }
#endif
    return a;
  }
  void flush() {
    ::Serial.flush();
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE || JANUS_UART0_STATUS_ENABLE
    if (janusUart0FullLog) JanusDebugUART.flush();
#endif
  }
  template<typename T>
  void print(const T& v) {
    ::Serial.print(v);
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE || JANUS_UART0_STATUS_ENABLE
    if (janusUart0FullLog) JanusDebugUART.print(v);
#endif
  }
  template<typename T>
  void println(const T& v) {
    ::Serial.println(v);
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE || JANUS_UART0_STATUS_ENABLE
    if (janusUart0FullLog) JanusDebugUART.println(v);
#endif
  }
  void println() {
    ::Serial.println();
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE || JANUS_UART0_STATUS_ENABLE
    if (janusUart0FullLog) JanusDebugUART.println();
#endif
  }
  int printf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return n;
    ::Serial.print(buf);
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE || JANUS_UART0_STATUS_ENABLE
    if (janusUart0FullLog) JanusDebugUART.print(buf);
#endif
    return n;
  }
};

JanusDualSerialClass JanusSerial;
#define Serial JanusSerial
// ======================= END JANUS DUAL SERIAL BRIDGE =======================


#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 3
#endif

// ========================= JANUS RF_ANCHOR_AUX v1.16 RF_DOME =========================
// Companion node for Blind Eye v2.12B/v2.12C. v1.12 keeps native USB logs by default, adds runtime UART0 full-log toggle by long BOOT press, all-LED master brightness 0..96, strict aux LED suppression, and BOOT tap-cycle brightness control:
// normal = very slow soft amber->green Janus breathing; share = white-gold flash and amber/green face swap with Gladius.
// Role: camera-less RF anchor + Buzz lottery worker + ESP-NOW beacon.
// It uses V31-inspired scheduler-only nonce lanes: linear, zim_reverse, bitrev,
// janus_center, knight, random. It does NOT change Buzz job header/target bytes.

#define JANUS_NODE_ID              "RFAnchorAux"
#define JANUS_NODE_ROLE            "RF_ANCHOR"
#define JANUS_FORCE_CHANNEL        10      // from your Blind Eye log: peerCh=10
#define JANUS_USE_WIFI_STA         1
#define JANUS_WIFI_SSID            "YOUR_WIFI"
#define JANUS_WIFI_PASS            "YOUR_PASS"

#define COLONY_HEARTBEAT_MS        2000UL
#define COLONY_ENTROPY_MS          2500UL
#define SWARMSENSE_TX_MS           5000UL
#define SWARMSENSE_ALERT_MS        1600UL
#define ANCHOR_PN_CORTEX_MS        5400UL
#define JANUS_ROLE_ANCHOR_PN       14
#define MASTER_TIMEOUT_MS          18000UL
#define JOB_TIMEOUT_MS             18000UL
#define REMOTE_BATCH_BASE          320
#define REMOTE_BATCH_MIN           80
#define REMOTE_BATCH_MAX           1200
#define JOB_RANGE_DEFAULT          262144UL
#define MINER_SECTORS              8
#define RF_SAMPLE_MS               120UL
#define RF_DEBUG_MS                1000UL
#define ANCHOR_STATUS_MS           2000UL
#define ANCHOR_TX_LOG_EVERY        1UL
#define ANCHOR_RADIO_RESCUE_MIN_MS 9000UL
#define ANCHOR_RADIO_RX_BLACKOUT_MS 85000UL
#define ANCHOR_RADIO_MASTER_BLACKOUT_MS 15000UL
#define ANCHOR_PRESENCE_BURST_MIN_MS 7500UL
#define ANCHOR_PRESENCE_REFRESH_MS 14000UL
#define ANCHOR_RADIO_TX_FAIL_DELTA 5UL
#define ANCHOR_TRANCEPTION_LITE_MS 1300UL

// v1.13 RF DOME / HUMAN SONAR:
// Anchor listens for Core2 R/P pulses and builds a baseline for the Core2<->Anchor radio sleeve.
#define RF_DOME_ENABLE             1
#define RF_DOME_TX_MS              650UL
#define RF_DOME_ALERT_TX_MS        260UL
#define RF_DOME_CORE_TIMEOUT_MS    6500UL
#define RF_DOME_LEARN_ALPHA_COLD   0.040f
#define RF_DOME_LEARN_ALPHA_HOT    0.0015f
#define RF_DOME_DEFAULT_LENGTH_CM  260
#define RF_DOME_MIN_CONF_PACKETS   8UL
#define RF_DOME_LOG_MS              2000UL  // v1.19B: throttle noisy RF/DOME Serial spam while packets still TX normally

// Soft status LED. Most ESP32-S3 dev boards with a single RGB LED use GPIO21.
// If your board has a different LED pin, change ANCHOR_LED_PIN or set ENABLE to 0.
#ifndef RGB_BUILTIN
#define RGB_BUILTIN                21
#endif
#define ANCHOR_LED_ENABLE          1   // v1.05: LED back on, logs stay dual-serial
// Most ESP32-S3 DevKit-style boards use GPIO48 for the addressable RGB.
// If your board uses GPIO21, change 48 -> 21. If it has no RGB, set ENABLE to 0.
#ifndef ANCHOR_LED_PIN
#define ANCHOR_LED_PIN             48
#endif
#define ANCHOR_LED_BRIGHTNESS      38
#define ANCHOR_LED_SHARE_MS        9800UL
#define ANCHOR_LED_MAX_FLASH_MS    900UL   // v1.12: white flash when tap-cycle reaches max brightness
// JANUS FACE SYNC:
// Anchor and Gladius are two Janus faces. They keep one slow amber -> green gradient,
// opposite breathing direction, and swap directions on share/ticket events.
#define JANUS_FACE_SYNC_ENABLE     1
#define JANUS_FACE_TX_MS           2400UL
#define JANUS_FACE_PEER_TTL_MS     15000UL
#define JANUS_FACE_SWAP_MS         12000UL
#define JANUS_FACE_AMBER           0   // v1.17 legacy name: Army Men green face
#define JANUS_FACE_GREEN           1   // v1.17 legacy name: turquoise/cyan twin face
// Backward-compatible names used by the existing packet logic.
#define JANUS_FACE_CYAN            JANUS_FACE_AMBER
#define JANUS_FACE_MAGENTA         JANUS_FACE_GREEN
#define JANUS_FACE_ROLE_ANCHOR     JANUS_FACE_AMBER
#define JANUS_FACE_ROLE_GLADIUS    JANUS_FACE_GREEN

// JANUS TWIN TASK / BROTHER RACE:
// Anchor and Gladius talk directly over ESP-NOW using J/T packets. This does not
// change Buzz headers/targets or Stratum semantics; it shares LED handoff, current
// Buzz job fingerprint, progress, H/s and best tail so both brothers can race while
// avoiding dumb identical lanes when they hear the same Buzz work window.
// v1.19B: Consilium job policy + SAFE_AGE. Buzz can send many same-header nonce windows quickly;
// Anchor no longer drops a live range immediately. Same Buzz work is queued/yielded
// after a small useful slice, while brand-new pool work still replaces instantly.
#define JANUS_TWIN_TASK_ENABLE     1
#define JANUS_TWIN_TASK_TX_MS      900UL
#define JANUS_TWIN_TASK_PEER_TTL_MS 6500UL
#define JANUS_TWIN_ROLE_ANCHOR     1
#define JANUS_TWIN_ROLE_GLADIUS    2
#define JANUS_TWIN_FLAG_ACTIVE     0x0001
#define JANUS_TWIN_FLAG_SHARE      0x0002
#define JANUS_TWIN_FLAG_SAME_JOB   0x0004
#define JANUS_TWIN_FLAG_SPLIT      0x0008
#define JANUS_TWIN_FLAG_ANCHOR     0x0010
#define JANUS_TWIN_FLAG_GLADIUS    0x0020


// v1.19 CONSILIUM JOB POLICY:
// Logs showed many "seen / 0 / expired" windows because a fresh J/B packet could
// overwrite a live range after only a few dozen hashes. This policy keeps the robot
// responsive to new pool work but gives every same-fingerprint nonce range a small
// useful slice before yielding to the next queued range.
#define JANUS_JOB_QUEUE_ENABLE        1
#define JANUS_JOB_MIN_WORK_MS         280UL
#define JANUS_JOB_MIN_WORK_HASHES     4096UL
#define JANUS_JOB_QUEUE_MAX_AGE_MS    4500UL
#define JANUS_JOB_QUEUE_LOG_MS        1400UL
#define JANUS_JOB_DUP_START_DROP      1
#define JANUS_JOB_NEW_FP_REPLACE      1
#define JANUS_JOB_YIELD_AFTER_QUEUE   1

#define ANCHOR_SERIAL_WAIT_MS      4500UL
#define ANCHOR_WAIT_LOG_MS         2000UL
#define ANCHOR_RX_DEBUG_EVERY      32UL
#define ANCHOR_MINER_DEBUG_MS      1000UL

// Brightness control via one safe programmable button.
// GPIO0 is usually the BOOT button on ESP32-S3 boards.
// Short tap: brightness cycles up and then down: 0 -> max -> 0.
// Very long hold: toggles full UART0 logs/blue activity LED.
// Do not hold BOOT while plugging/resetting unless you want flash/download mode.
#define ANCHOR_BUTTON_ENABLE       1
#define ANCHOR_BUTTON_PIN          0
#define ANCHOR_BUTTON_ACTIVE_LOW   1
#define ANCHOR_BUTTON_DEBOUNCE_MS  35UL
#define ANCHOR_BUTTON_LONG_MS      750UL
#define ANCHOR_BUTTON_LOG_TOGGLE_MS 2800UL
#define ANCHOR_BUTTON_REPEAT_MS    260UL
#define ANCHOR_BRIGHTNESS_MIN      0
#define ANCHOR_BRIGHTNESS_MAX      96
#define ANCHOR_BRIGHTNESS_STEP     8
#define ANCHOR_BRIGHTNESS_PERSIST  1
#define ANCHOR_BRIGHTNESS_SAVE_MS  1500UL

// v1.12 master LED control:
// - RGB status LED obeys ledBrightness 0..96.
// - When tap-cycle reaches max brightness, RGB emits a brief white flash.
// - Optional extra LEDs are forced OFF, useful for boards where GPIO21 is a
//   separate blue status LED. The dim yellow PWR LED is often hardwired to 3V3;
//   if so firmware cannot turn it off.
#define ANCHOR_EXTRA_LED_OFF_ENABLE    1
#define ANCHOR_EXTRA_BLUE_PIN          21
#define ANCHOR_EXTRA_BLUE_ACTIVE_LOW   0
#define ANCHOR_EXTRA_YELLOW_PIN        -1
#define ANCHOR_EXTRA_YELLOW_ACTIVE_LOW 0

uint8_t JANUS_BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

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
  uint8_t version;       // 1
  uint8_t role;          // 1 Anchor, 2 Gladius
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

// Common Janus P/N Cortex packet. Observer-only: RF/silicon/body trace for Core2,
// never a scheduler override and never a pool/share wire mutation.
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
  uint8_t flags;           // bit0 job, bit1 Buzz/pool, bit2 RF dome, bit3 transition, bit4 policy/memory
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
// v1.13/v6.42C4 Core2 <-> Anchor RF dome packets.
// R/P is a small pulse from Core2. Anchor measures RSSI on reception.
// R/S is Anchor's interpreted RF-sleeve snapshot. It is not exact CSI;
// it is a low-cost ESP-NOW/RSSI human-sonar estimate for JANUS.
struct __attribute__((packed)) RfDomePingPacket {
  uint8_t magic[2];       // 'R','P'
  uint8_t version;        // 1
  uint8_t pingMode;       // 0 normal, 1 page-open active scan
  char source[16];        // Core2Home
  uint32_t seq;
  uint32_t uptimeMs;
  uint16_t pulse;
  uint8_t channel;
  uint8_t reserved;
};

struct __attribute__((packed)) RfDomeSonarPacket {
  uint8_t magic[2];       // 'R','S'
  uint8_t version;        // 1
  uint8_t flags;          // bit0 coreFresh, bit1 presence, bit2 motion, bit3 human?, bit4 pet?, bit5 learning
  char anchorId[24];
  uint32_t seq;
  uint32_t uptimeMs;
  int8_t coreRssi;
  int8_t ambientRssi;
  int16_t coreEma_x10;
  int16_t coreBase_x10;
  int16_t coreDelta_x10;
  uint16_t coreVar_x10;
  uint16_t motion_x100;
  uint16_t presence_x100;
  uint16_t human_x100;
  uint16_t pet_x100;
  uint8_t zonePct;
  uint16_t distanceCm;
  uint8_t confidence;
  uint16_t domeLengthCm;
  uint32_t packetsSeen;
  uint32_t crc;
};

// Explicit prototypes prevent Arduino .ino autoprototype from seeing custom RF Dome types too early.
uint32_t rfDomeCrc32(const void* data, size_t len);
void rfDomeOnCorePing(const RfDomePingPacket& ping, int8_t rssi);
void sendRfDome(bool force);
void sendAnchorPnCortex(bool force=false);
void anchorPresenceBurst(const char* reason);
uint8_t currentChannel();
bool ensureBuzzMasterPeer(const char* reason);
esp_err_t sendEspNowToBuzzMaster(const char* tag, const void* payload, size_t len);
void rememberBuzzMasterMac(const uint8_t* mac, const char* reason);


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

struct RemoteJobState {
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
  uint8_t minerSector = 0;
  uint8_t minerStrideArm = 0;
  uint32_t minerSeed = 0;
  uint32_t minerStride = 1;
  uint32_t minerStartOffset = 0;
};

RemoteJobState job;
uint16_t workerId = 0;
uint32_t seqNo = 0;
uint32_t ssSeq = 0;
uint32_t anchorPnSeq = 0;
uint32_t anchorPnLastMs = 0;
uint32_t anchorPnTx = 0;
uint32_t anchorPnFail = 0;
uint32_t anchorPnPrevHash = 0;
uint32_t anchorLoopLastMs = 0;
uint16_t anchorLoopJitterUs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastEntropyMs = 0;
uint32_t lastSwarmSenseMs = 0;
uint32_t lastHashTickMs = 0;
uint32_t lastMasterMs = 0;
uint32_t lastRfSampleMs = 0;
uint32_t lastRfDebugMs = 0;
uint32_t hashCounter = 0;
uint32_t hashRate = 0;
uint32_t shares = 0;
uint32_t rejects = 0;
uint32_t jobsSeen = 0;
uint32_t jobsDone = 0;
uint32_t jobsExpired = 0;
uint32_t jobsQueued = 0;
uint32_t jobsYielded = 0;
uint32_t jobsReplacedNewWork = 0;
uint32_t jobsDroppedDuplicate = 0;
uint32_t jobsAccepted = 0;
uint32_t lastJobQueueLogMs = 0;
RemoteJobState queuedJob;
bool queuedJobValid = false;
uint32_t queuedJobAtMs = 0;
uint32_t queuedJobFp32 = 0;
uint16_t targetBits = 0;
uint32_t bestBits = 0;
uint32_t bestNonce = 0;
uint32_t tailHits = 0;
uint32_t laneSwitches = 0;
uint32_t txOk = 0;
uint32_t txFail = 0;
int lastTxErr = 0;
int8_t lastRssi = -127;
uint8_t peerChannel = 0;
uint8_t buzzMasterMac[6] = {0};
bool buzzMasterMacKnown = false;
uint8_t buzzMasterPeerChannel = 0;
uint32_t buzzMasterMacSeenMs = 0;
uint32_t buzzMasterDirectOk = 0;
uint32_t buzzMasterDirectFail = 0;
uint32_t buzzMasterMacMissing = 0;
uint32_t buzzMasterLastLogMs = 0;
uint32_t anchorRadioLastRescueMs = 0;
uint32_t anchorRadioLastWatchMs = 0;
uint32_t anchorRadioLastTxFailSeen = 0;
uint32_t anchorRadioLastTxOkSeen = 0;
uint32_t anchorRadioRescues = 0;
uint32_t anchorPresenceBursts = 0;
uint32_t anchorLastPresenceBurstMs = 0;
uint32_t anchorLastPresenceRefreshMs = 0;
uint32_t lastStatusMs = 0;
uint32_t lastShareMs = 0;
uint16_t lastShareBits = 0;
uint32_t lastLedMs = 0;
uint8_t lastLedR = 0, lastLedG = 0, lastLedB = 0;
uint32_t janusFaceSeq = 0;
uint32_t janusFaceLastTxMs = 0;
uint32_t janusFacePeerLastMs = 0;
uint16_t janusFacePeerNode = 0;
uint8_t janusFacePeerRoleFace = 255;
uint8_t janusFacePeerFace = 255;
uint8_t janusFacePeerFlags = 0;
uint16_t janusFacePeerEventBits = 0;
int8_t janusFacePeerRssi = -127;

uint32_t janusTwinSeq = 0;
uint32_t janusTwinLastTxMs = 0;
uint32_t janusTwinPeerLastMs = 0;
uint16_t janusTwinPeerNode = 0;
uint8_t janusTwinPeerRole = 0;
uint32_t janusTwinPeerJobFp32 = 0;
uint32_t janusTwinPeerJobStart = 0;
uint32_t janusTwinPeerJobRange = 0;
uint32_t janusTwinPeerChecked = 0;
uint32_t janusTwinPeerHashRate = 0;
uint32_t janusTwinPeerBestBits = 0;
uint32_t janusTwinPeerShares = 0;
uint8_t janusTwinPeerLane = 0;
uint8_t janusTwinPeerSector = 0;
uint16_t janusTwinPeerTargetBits = 0;
uint32_t janusTwinPeerStride = 0;
uint16_t janusTwinPeerFlags = 0;
int8_t janusTwinPeerRssi = -127;
uint32_t janusTwinRx = 0;
uint32_t janusTwinTx = 0;
uint32_t janusTwinSplitApplied = 0;
uint32_t janusTwinRaceWins = 0;
uint32_t janusTwinRaceLosses = 0;
float anchorOxytocin = 50.0f;
float anchorTorricelliVacuum = 0.50f;
uint32_t anchorTorricelliLastMs = 0;
float anchorTranceptionLiteScore = 0.0f;
uint8_t anchorTranceptionHint = 1;
uint8_t anchorTranceptionLane = 0;
uint32_t anchorTranceptionLastMs = 0;
uint32_t anchorTranceptionReports = 0;
uint32_t lastMaxBrightnessFlashMs = 0;
uint8_t ledBrightness = ANCHOR_LED_BRIGHTNESS;
// Unified Janus twin control: long BOOT hold or serial U toggles UART0 full logs
// and the small separate blue/activity LED together, just like Gladius.
bool anchorSmallLedEnabled = false;
bool anchorSmallLedLastState = false;
uint32_t lastButtonSampleMs = 0;
uint32_t buttonPressStartMs = 0;
uint32_t buttonLastRepeatMs = 0;
bool buttonStablePressed = false;
bool buttonLastRawPressed = false;
bool buttonLongMode = false;
bool buttonLogToggleFired = false;
bool buttonBrightnessDirUp = true;  // v1.12: tap-cycle direction, flips at min/max.
uint32_t brightnessChangedMs = 0;
bool brightnessDirty = false;
#if ANCHOR_BRIGHTNESS_PERSIST
Preferences anchorPrefs;
bool anchorPrefsReady = false;
#endif
uint32_t heartbeatTx = 0;
uint32_t entropyTx = 0;
uint32_t swarmSenseTx = 0;
uint32_t rxSeen = 0;
uint32_t lastAnyRxMs = 0;
uint32_t rxJanus = 0;
uint32_t rxJobs = 0;
uint32_t rxAgent = 0;
uint32_t sentCbOk = 0;
uint32_t sentCbFail = 0;
uint32_t lastWaitLogMs = 0;
uint32_t lastMinerLogMs = 0;
uint32_t lastUart0StatusMs = 0;
uint32_t totalHashesLifetime = 0;
float hashRateEma = 0.0f;

uint8_t agentHint = 1;
uint8_t agentLevel = 0;
uint16_t agentBatch = REMOTE_BATCH_BASE;
uint32_t agentEntropySeed = 0xC4111903UL;
uint32_t agentRewards = 0;
float agentScore = 0.0f;
float agentPredH = 0.0f;
float agentErr = 0.0f;

float rfEma = -127.0f;
float rfBase = -127.0f;
float rfNoise = 3.0f;
float rfDrift = 0.0f;
float rfMotion = 0.0f;
float rfPresence = 0.0f;
float rfEntropy = 0.0f;
float rfPacketPressure = 0.0f;
float rfLastPacketDrift = 0.0f;
uint32_t rfRxPackets = 0;
uint32_t rfLastPacketMs = 0;
bool rfReady = false;
bool rfPresenceNow = false;
bool rfMotionNow = false;

uint32_t rfDomeSeq = 0;
uint32_t rfDomeRxPing = 0;
uint32_t rfDomeTx = 0;
uint32_t rfDomeLastPingMs = 0;
uint32_t rfDomeLastTxMs = 0;
uint32_t rfDomeLastLogMs = 0;
int8_t rfDomeCoreRssi = -127;
float rfDomeCoreEma = -127.0f;
float rfDomeCoreBase = -127.0f;
float rfDomeVar = 4.0f;
float rfDomeDelta = 0.0f;
float rfDomeMotion = 0.0f;
float rfDomePresence = 0.0f;
float rfDomeHuman = 0.0f;
float rfDomePet = 0.0f;
uint8_t rfDomeZonePct = 50;
uint16_t rfDomeDistanceCm = RF_DOME_DEFAULT_LENGTH_CM / 2;
uint8_t rfDomeConfidence = 0;
bool rfDomeReady = false;


// v1.19B SAFE AGE GUARD:
// ESP-NOW callbacks can update timestamps a few milliseconds AFTER loop() captured `now`.
// Raw `now - then` then prints 4294967xxx ms and can falsely stale-drop a queued job.
// Unsigned subtraction is still preserved for real millis() rollover; only impossible
// half-range deltas are clamped to zero.
uint32_t janusSafeAgeMs(uint32_t now, uint32_t then, uint32_t missing = 999999UL) {
  if (!then) return missing;
  uint32_t d = now - then;
  if (d > 0x7FFFFFFFUL) return 0UL;
  return d;
}

bool janusSafeElapsed(uint32_t now, uint32_t then, uint32_t intervalMs) {
  if (!then) return false;
  return janusSafeAgeMs(now, then, 0UL) >= intervalMs;
}

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

void hashToShareOrder(const uint8_t in[32], uint8_t out[32]) {
  for (int i = 0; i < 32; ++i) out[i] = in[31 - i];
}

bool hashMeetsTargetBE(const uint8_t hash[32], const uint8_t target[32]) {
  for (int i = 0; i < 32; i++) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;
}

uint32_t bitReverse32(uint32_t x) {
  x = ((x & 0x55555555UL) << 1) | ((x >> 1) & 0x55555555UL);
  x = ((x & 0x33333333UL) << 2) | ((x >> 2) & 0x33333333UL);
  x = ((x & 0x0F0F0F0FUL) << 4) | ((x >> 4) & 0x0F0F0F0FUL);
  x = ((x & 0x00FF00FFUL) << 8) | ((x >> 8) & 0x00FF00FFUL);
  x = (x << 16) | (x >> 16);
  return x;
}

uint32_t xorShift32(uint32_t x) {
  if (!x) x = 0xA5A5A5A5UL;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

const char* laneName(uint8_t lane) {
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

uint32_t strideArmValue(uint8_t arm) {
  static const uint32_t arms[] = {
    1UL, 3UL, 5UL, 7UL, 11UL, 17UL, 29UL, 31UL, 53UL, 97UL, 257UL, 521UL,
    4099UL, 65537UL, 0x9E3779B9UL, 0xC4111903UL, 0x4F1BBCDDUL
  };
  return arms[arm % (sizeof(arms) / sizeof(arms[0]))] | 1UL;
}

uint32_t janusTwinJobFp32From8(const uint8_t jid[8]) {
  uint32_t h = 0xB00B5EEDUL;
  for (uint8_t i = 0; i < 8; i++) { h ^= jid[i]; h *= 16777619UL; }
  return h;
}

bool janusTwinPeerFresh() {
#if JANUS_TWIN_TASK_ENABLE
  return janusTwinPeerLastMs && (janusSafeAgeMs(millis(), janusTwinPeerLastMs, 999999UL) < JANUS_TWIN_TASK_PEER_TTL_MS);
#else
  return false;
#endif
}

bool janusTwinSameBuzzWindow(const RemoteJobState& j) {
#if JANUS_TWIN_TASK_ENABLE
  if (!janusTwinPeerFresh()) return false;
  if (!j.active) return false;
  uint32_t fp = janusTwinJobFp32From8(j.job_id);
  if (!fp || fp != janusTwinPeerJobFp32) return false;
  // v1.19: same Buzz work means same job fingerprint, not necessarily same startNonce.
  // Buzz deliberately hands brothers different nonce windows; requiring equal start
  // made the split logic blind exactly when it was needed most.
  if (janusTwinPeerTargetBits && targetBits && janusTwinPeerTargetBits != targetBits) return false;
  return true;
#else
  (void)j;
  return false;
#endif
}

void anchorTorricelliBondTick(uint32_t now) {
  if (anchorTorricelliLastMs && janusSafeAgeMs(now, anchorTorricelliLastMs, 0UL) < 850UL) return;
  float dt = anchorTorricelliLastMs ? min(4.0f, (float)janusSafeAgeMs(now, anchorTorricelliLastMs, 0UL) / 1000.0f) : 1.0f;
  anchorTorricelliLastMs = now;

  bool twinFresh = janusTwinPeerFresh();
  bool sameJob = janusTwinSameBuzzWindow(job);
  uint32_t txTotal = txOk + txFail + 8UL;
  float radioClean = constrain(1.0f - ((float)txFail / (float)txTotal), 0.0f, 1.0f);
  uint16_t target = targetBits ? targetBits : 22;
  float progress = constrain((float)bestBits / (float)max<uint16_t>(1, target), 0.0f, 1.8f);
  bool brotherAhead = twinFresh && (janusTwinPeerBestBits > bestBits || janusTwinPeerHashRate > hashRate + 800UL);
  bool freshShare = lastShareMs && janusSafeAgeMs(now, lastShareMs, 999999UL) < 30000UL;

  float pressure = 0.0f;
  pressure += twinFresh ? 0.40f : -0.28f;
  pressure += sameJob ? 0.28f : 0.0f;
  pressure += brotherAhead ? 0.26f : 0.08f;
  pressure += freshShare ? 0.42f : 0.0f;
  pressure += radioClean * 0.18f + progress * 0.16f;
  pressure -= (txFail > txOk + 20UL) ? 0.35f : 0.0f;

  anchorOxytocin = constrain(anchorOxytocin + pressure * dt, 0.0f, 100.0f);

  float vacuumTarget = constrain(0.20f + radioClean * 0.34f + (twinFresh ? 0.22f : 0.0f) +
                                 (sameJob ? 0.16f : 0.0f) + progress * 0.08f -
                                 rfEntropy * 0.025f, 0.0f, 1.0f);
  anchorTorricelliVacuum = constrain(anchorTorricelliVacuum * 0.92f + vacuumTarget * 0.08f, 0.0f, 1.0f);
}

uint8_t anchorSchedulerHint() {
  uint8_t hint = agentHint ? agentHint : 1;
  if (anchorTranceptionHint > hint) hint = anchorTranceptionHint;
  return (uint8_t)constrain((int)hint, 1, 4);
}

void anchorTranceptionLiteTick(uint32_t now) {
  if (anchorTranceptionLastMs &&
      janusSafeAgeMs(now, anchorTranceptionLastMs, 0UL) < ANCHOR_TRANCEPTION_LITE_MS) {
    return;
  }
  anchorTranceptionLastMs = now;

  uint16_t target = targetBits ? targetBits : 22;
  float bestFit = constrain((float)bestBits / (float)max<uint16_t>(1, target), 0.0f, 1.65f);
  float hashFit = constrain((float)hashRate / 22000.0f, 0.0f, 1.35f);
  float txTotal = (float)(txOk + txFail + 8UL);
  float radioClean = constrain(1.0f - ((float)txFail / txTotal), 0.0f, 1.0f);
  float twin = janusTwinPeerFresh() ? 1.0f : 0.0f;
  float sameJob = janusTwinSameBuzzWindow(job) ? 1.0f : 0.0f;
  float oxy = constrain(anchorOxytocin / 100.0f, 0.0f, 1.0f);
  float tailFit = constrain((float)tailHits / 96.0f, 0.0f, 1.0f);
  float jobAgePenalty = 0.0f;
  if (job.active) {
    uint32_t age = janusSafeAgeMs(now, job.receivedAt, 0UL);
    jobAgePenalty = age > JOB_TIMEOUT_MS ? 0.22f : constrain((float)age / 90000.0f, 0.0f, 0.12f);
  }

  float score = 0.0f;
  score += bestFit * 0.34f;
  score += hashFit * 0.18f;
  score += radioClean * 0.14f;
  score += twin * 0.08f + sameJob * 0.08f;
  score += anchorTorricelliVacuum * 0.08f + oxy * 0.06f + tailFit * 0.04f;
  score -= jobAgePenalty;
  anchorTranceptionLiteScore = constrain(score, 0.0f, 1.50f);

  uint8_t oldHint = anchorTranceptionHint;
  if (anchorTranceptionLiteScore >= 0.98f || bestBits >= target) anchorTranceptionHint = 4;
  else if (anchorTranceptionLiteScore >= 0.78f) anchorTranceptionHint = 3;
  else if (anchorTranceptionLiteScore < 0.46f || radioClean < 0.72f) anchorTranceptionHint = 2;
  else anchorTranceptionHint = 1;

  if (anchorTranceptionHint >= 4) anchorTranceptionLane = (bestBits & 1U) ? 4 : 3;
  else if (anchorTranceptionHint >= 3) anchorTranceptionLane = (uint8_t)((workerId + bestBits + tailHits) % 3U) + 3U;
  else if (anchorTranceptionHint == 2) anchorTranceptionLane = 1;
  else anchorTranceptionLane = 0;

  anchorTranceptionReports++;
  if (oldHint != anchorTranceptionHint || (anchorTranceptionReports & 0x0FUL) == 1UL) {
    Serial.printf("[ANCHOR/TL] score=%.3f hint=%u lane=%s best=%lu/%u H=%lu clean=%.2f twin=%u same=%u oxy=%.1f vac=%.2f tail=%lu wire=frozen\n",
                  anchorTranceptionLiteScore, (unsigned)anchorTranceptionHint,
                  laneName(anchorTranceptionLane), (unsigned long)bestBits, (unsigned)target,
                  (unsigned long)hashRate, radioClean, twin > 0.5f ? 1 : 0,
                  sameJob > 0.5f ? 1 : 0, anchorOxytocin, anchorTorricelliVacuum,
                  (unsigned long)tailHits);
  }
}

void janusTwinApplyLaneSplit(RemoteJobState& j) {
#if JANUS_TWIN_TASK_ENABLE
  if (!janusTwinSameBuzzWindow(j)) return;
  uint8_t oldSector = j.minerSector;
  uint8_t oldLane = j.minerLane;
  uint32_t oldStride = j.minerStride;

  // Anchor takes the even/high-energy claws, Gladius normally takes the opposite
  // side. If peer reports the same sector, rotate away deterministically.
  static const uint8_t anchorSectors[4] = {6, 4, 2, 0};
  uint8_t pick = (uint8_t)(((j.minerSeed >> 24) ^ jobsSeen ^ janusTwinPeerNode) & 0x03);
  j.minerSector = anchorSectors[pick] % MINER_SECTORS;
  if (janusTwinPeerSector < MINER_SECTORS && j.minerSector == janusTwinPeerSector) {
    j.minerSector = anchorSectors[(pick + 1) & 0x03] % MINER_SECTORS;
  }

  // If both brothers accidentally choose the same walk style, Anchor phase-shifts
  // the lane/stride instead of brute-copying Gladius' path.
  if (j.minerLane == janusTwinPeerLane) {
    j.minerLane = (uint8_t)((j.minerLane + 2 + (workerId & 1)) % 6);
    if (j.minerLane == 0 && janusTwinPeerLane == 0) j.minerLane = 3;
  }
  if (j.minerStride == janusTwinPeerStride) {
    j.minerStrideArm = (uint8_t)((j.minerStrideArm + 5) % 17);
    j.minerStride = strideArmValue(j.minerStrideArm);
  }

  j.minerStartOffset ^= 0x6A09E667UL ^ ((uint32_t)janusTwinPeerNode << 8) ^ ((uint32_t)j.minerSector << 24);
  if (oldSector != j.minerSector || oldLane != j.minerLane || oldStride != j.minerStride) {
    janusTwinSplitApplied++;
    Serial.printf("[ANCHOR/TWIN] split Buzz window peer=%04X fp=%08lX sector %u->%u lane %s->%s stride %lu->%lu splits=%lu\n",
                  janusTwinPeerNode, (unsigned long)janusTwinPeerJobFp32,
                  (unsigned)oldSector, (unsigned)j.minerSector, laneName(oldLane), laneName(j.minerLane),
                  (unsigned long)oldStride, (unsigned long)j.minerStride, (unsigned long)janusTwinSplitApplied);
  }
#else
  (void)j;
#endif
}

void configureMinerLane(RemoteJobState& j) {
  uint32_t seed = micros() ^ ESP.getCycleCount() ^ agentEntropySeed ^ (jobsSeen << 16) ^ workerId;
  for (uint8_t i = 0; i < 8; ++i) seed = xorShift32(seed ^ j.job_id[i]);
  uint8_t localHint = anchorSchedulerHint();
  j.minerSeed = seed;
  j.minerStrideArm = (uint8_t)((seed ^ (seed >> 8) ^ agentLevel ^ anchorTranceptionHint) % 17);
  j.minerStride = strideArmValue(j.minerStrideArm);

  uint8_t selector = (uint8_t)((seed ^ (seed >> 11) ^ localHint ^ jobsSeen) % 100);
  if (localHint >= 3 || agentLevel >= 2) {
    if (selector < 42) j.minerLane = 1;
    else if (selector < 64) j.minerLane = 4;
    else if (selector < 80) j.minerLane = 2;
    else if (selector < 92) j.minerLane = 3;
    else j.minerLane = 5;
  } else {
    if (selector < 38) j.minerLane = 0;
    else if (selector < 67) j.minerLane = 1;
    else if (selector < 80) j.minerLane = 2;
    else if (selector < 92) j.minerLane = 3;
    else j.minerLane = 5;
  }

  if (j.minerLane == 0 || j.minerLane == 1) j.minerSector = 6 % MINER_SECTORS;
  else if (j.minerLane == 4) j.minerSector = 7 % MINER_SECTORS;
  else j.minerSector = (uint8_t)((seed >> 24) % MINER_SECTORS);
  if (anchorTranceptionHint >= 3 && anchorTranceptionLane <= 5 && ((seed >> 5) & 0x03U) == 0) {
    j.minerLane = anchorTranceptionLane;
  }
  j.minerStartOffset = bitReverse32(seed ^ 0xC4111903UL);
  janusTwinApplyLaneSplit(j);
  laneSwitches++;
}

uint32_t nextNonce(const RemoteJobState& j, uint32_t i) {
  uint32_t range = j.rangeSize ? j.rangeSize : JOB_RANGE_DEFAULT;
  uint32_t sectors = MINER_SECTORS;
  uint32_t sector = j.minerSector % sectors;
  uint32_t sectorWidth = max<uint32_t>(1UL, range / sectors);
  uint32_t sectorStart = min<uint32_t>(range - 1UL, sector * sectorWidth);
  if (sector == sectors - 1 || sectorStart + sectorWidth > range) sectorWidth = range - sectorStart;
  sectorWidth = max<uint32_t>(1UL, sectorWidth);

  uint32_t local = 0;
  uint32_t seed = j.minerSeed ^ j.minerStartOffset;
  uint32_t stride = j.minerStride | 1UL;

  switch (j.minerLane) {
    case 1: {
      uint32_t cursor = seed % sectorWidth;
      uint32_t walk = (uint32_t)(((uint64_t)(i % sectorWidth) * stride) % sectorWidth);
      local = sectorStart + ((cursor + sectorWidth - walk) % sectorWidth);
      break;
    }
    case 2:
      local = bitReverse32(seed + i) % range;
      break;
    case 3: {
      uint32_t step = (i + 1UL) >> 1;
      uint32_t center = sectorWidth >> 1;
      uint32_t off = (i & 1UL) ? (center + step) : (center + sectorWidth - (step % sectorWidth));
      local = sectorStart + (off % sectorWidth);
      break;
    }
    case 4:
      local = sectorStart + ((seed + i * 0x9E3779B9UL) % sectorWidth);
      break;
    case 5:
      local = xorShift32(seed + i * 0xA5A5A5A5UL) % range;
      break;
    case 0:
    default:
      local = (j.minerStartOffset + i) % range;
      break;
  }
  return j.startNonce + local;
}

uint16_t activeBatch() {
  uint16_t b = agentBatch ? agentBatch : REMOTE_BATCH_BASE;
  uint8_t localHint = anchorSchedulerHint();
  if (localHint == 2) b = min<uint16_t>(b, 220);
  if (localHint >= 3) b = max<uint16_t>(b, REMOTE_BATCH_BASE);
  if (anchorTranceptionHint >= 3 && anchorTranceptionLiteScore > 0.82f && ESP.getFreeHeap() > 90000) {
    b = (uint16_t)min<int>((int)REMOTE_BATCH_MAX, (int)b + 64);
  }
  if (anchorOxytocin > 58.0f && ESP.getFreeHeap() > 90000) {
    int oxyBoost = (int)((anchorOxytocin - 58.0f) * 3.0f);
    if (janusTwinPeerFresh()) oxyBoost += 24;
    if (janusTwinSameBuzzWindow(job) && janusTwinPeerBestBits >= bestBits) oxyBoost += 36;
    b = (uint16_t)min<int>((int)REMOTE_BATCH_MAX, (int)b + constrain(oxyBoost, 0, 180));
  } else if (anchorOxytocin < 24.0f || txFail > txOk + 30UL) {
    b = (uint16_t)max<int>((int)REMOTE_BATCH_MIN, (int)b - 80);
  }
  return constrain((int)b, REMOTE_BATCH_MIN, REMOTE_BATCH_MAX);
}

bool sendEspNow(const char* tag, const void* payload, size_t len) {
  if (!payload || !len) return false;
  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)payload, len);
  if (err == ESP_OK) { txOk++; return true; }
  txFail++;
  lastTxErr = (int)err;
  Serial.printf("[ANCHOR/TXFAIL] tag=%s err=%d fail=%lu ch=%u\n", tag ? tag : "?", lastTxErr, (unsigned long)txFail, (unsigned)peerChannel);
  return false;
}

bool janusFacePeerFresh() {
  return janusFacePeerLastMs && (janusSafeAgeMs(millis(), janusFacePeerLastMs, 999999UL) < JANUS_FACE_PEER_TTL_MS);
}

bool janusFaceShareActive() {
  bool own = lastShareMs && (janusSafeAgeMs(millis(), lastShareMs, 0UL) < JANUS_FACE_SWAP_MS);
  bool peer = janusFacePeerLastMs &&
              (janusSafeAgeMs(millis(), janusFacePeerLastMs, 999999UL) < JANUS_FACE_SWAP_MS) &&
              (janusFacePeerFlags & 0x01);
  return own || peer;
}

uint8_t janusFaceBaseFace() {
  uint8_t base = JANUS_FACE_ROLE_ANCHOR;

  // If the twin reports the same static face, split by nodeId so two identical
  // boards still cannot breathe with the same face.
  if (janusFacePeerFresh() && janusFacePeerRoleFace == JANUS_FACE_ROLE_ANCHOR && janusFacePeerNode) {
    base = (workerId < janusFacePeerNode) ? JANUS_FACE_CYAN : JANUS_FACE_MAGENTA;
  }

  return base;
}

uint8_t janusFaceCurrentFace() {
  uint8_t face = janusFaceBaseFace();
  if (janusFaceShareActive()) face ^= 1;
  return face & 1;
}

float janusFacePhase(uint32_t now) {
  uint32_t seed = ((uint32_t)workerId * 977UL) ^ 0x4A4E5553UL;
  uint32_t offs = 700UL + (seed % 2600UL);
  if (janusFacePeerFresh() && workerId > janusFacePeerNode) offs += 2600UL;
  // Slow, soft breathing. One full perceived wave is roughly 45+ seconds.
  return (sinf(((float)now + (float)offs) / 7800.0f) + 1.0f) * 0.5f;
}

void janusFaceBroadcast(bool eventNow, uint16_t eventBits) {
#if JANUS_FACE_SYNC_ENABLE
  JanusFaceSyncPacket jf{};
  jf.magic[0] = 'J';
  jf.magic[1] = 'F';
  jf.version = 1;
  jf.roleFace = JANUS_FACE_ROLE_ANCHOR;
  jf.nodeId = workerId;
  jf.seq = ++janusFaceSeq;
  jf.uptimeMs = millis();
  jf.face = janusFaceCurrentFace();
  jf.brightness = ledBrightness;
  jf.eventBits = eventNow ? eventBits : (janusFaceShareActive() ? lastShareBits : 0);
  jf.flags = 0x08; // anchor
  if (eventNow || (lastShareMs && janusSafeAgeMs(millis(), lastShareMs, 0UL) < JANUS_FACE_SWAP_MS)) jf.flags |= 0x01;
  if (janusFacePeerFresh()) jf.flags |= 0x02;
  jf.colorSeed = ((uint32_t)workerId << 16) ^ ESP.getCycleCount() ^ 0xFACEA113UL;
  jf.crc = 0;
  jf.crc = rfDomeCrc32(&jf, sizeof(jf) - 4);
  sendEspNow("J/F", &jf, sizeof(jf));
  janusFaceLastTxMs = millis();
#else
  (void)eventNow; (void)eventBits;
#endif
}

bool janusFaceReceive(const uint8_t* data, int len, int8_t rssi) {
#if JANUS_FACE_SYNC_ENABLE
  if (!data || len != (int)sizeof(JanusFaceSyncPacket) || data[0] != 'J' || data[1] != 'F') return false;

  JanusFaceSyncPacket jf{};
  memcpy(&jf, data, sizeof(jf));
  uint32_t got = jf.crc;
  jf.crc = 0;
  uint32_t calc = rfDomeCrc32(&jf, sizeof(jf) - 4);
  if (got && got != calc) return true;

  if (jf.nodeId == workerId) return true;

  bool first = !janusFacePeerLastMs || janusFacePeerNode != jf.nodeId;
  janusFacePeerLastMs = millis();
  janusFacePeerNode = jf.nodeId;
  janusFacePeerRoleFace = jf.roleFace;
  janusFacePeerFace = jf.face;
  janusFacePeerFlags = jf.flags;
  janusFacePeerEventBits = jf.eventBits;
  janusFacePeerRssi = rssi;
  lastLedMs = 0;

  if (first || (jf.flags & 0x01)) {
    Serial.printf("[ANCHOR/FACE] peer=%04X face=%u roleFace=%u flags=0x%02X bits=%u rssi=%d myFace=%u swap=%u\\n",
                  jf.nodeId, jf.face, jf.roleFace, jf.flags, jf.eventBits, (int)rssi,
                  (unsigned)janusFaceCurrentFace(), janusFaceShareActive() ? 1 : 0);
  }

  return true;
#else
  (void)data; (void)len; (void)rssi;
  return false;
#endif
}

void janusFaceTick(uint32_t now) {
#if JANUS_FACE_SYNC_ENABLE
  bool urgent = janusFaceShareActive() && (janusSafeAgeMs(now, janusFaceLastTxMs, 999999UL) > 350UL);
  if (urgent || janusSafeAgeMs(now, janusFaceLastTxMs, 999999UL) >= JANUS_FACE_TX_MS || janusFaceLastTxMs == 0) {
    janusFaceBroadcast(false, 0);
  }
#else
  (void)now;
#endif
}


void janusTwinTaskBroadcast(bool force=false) {
#if JANUS_TWIN_TASK_ENABLE
  uint32_t now = millis();
  bool active = job.active;
  bool shareActive = lastShareMs && (janusSafeAgeMs(now, lastShareMs, 0UL) < JANUS_FACE_SWAP_MS);
  bool urgent = shareActive || active || janusTwinPeerFresh();
  uint32_t interval = urgent ? JANUS_TWIN_TASK_TX_MS : (JANUS_TWIN_TASK_TX_MS * 4UL);
  if (!force && janusSafeAgeMs(now, janusTwinLastTxMs, 0UL) < interval) return;
  janusTwinLastTxMs = now;

  JanusTwinTaskPacket jt{};
  jt.magic[0] = 'J'; jt.magic[1] = 'T'; jt.version = 1;
  jt.role = JANUS_TWIN_ROLE_ANCHOR;
  jt.nodeId = workerId;
  jt.seq = ++janusTwinSeq;
  jt.uptimeMs = now;
  if (active) memcpy(jt.jobId8, job.job_id, 8);
  jt.jobFp32 = active ? janusTwinJobFp32From8(job.job_id) : 0;
  jt.jobStart = active ? job.startNonce : 0;
  jt.jobRange = active ? job.rangeSize : 0;
  jt.checked = active ? job.hashesDone : 0;
  jt.nonce = active ? job.nonce : bestNonce;
  jt.hashRate = hashRate;
  jt.bestBits = bestBits;
  jt.shares = shares;
  jt.jobsSeen = jobsSeen;
  jt.lane = active ? job.minerLane : 255;
  jt.sector = active ? job.minerSector : 255;
  jt.targetBits = targetBits;
  jt.stride = active ? job.minerStride : 0;
  jt.face = janusFaceCurrentFace();
  jt.brightness = ledBrightness;
  jt.eventBits = shareActive ? lastShareBits : 0;
  jt.flags = JANUS_TWIN_FLAG_ANCHOR;
  if (active) jt.flags |= JANUS_TWIN_FLAG_ACTIVE;
  if (shareActive) jt.flags |= JANUS_TWIN_FLAG_SHARE;
  if (janusTwinSameBuzzWindow(job)) jt.flags |= JANUS_TWIN_FLAG_SAME_JOB | JANUS_TWIN_FLAG_SPLIT;
  jt.rssi = lastRssi;
  jt.crc = 0;
  jt.crc = rfDomeCrc32(&jt, sizeof(jt) - 4);
  bool ok = sendEspNow("J/T", &jt, sizeof(jt));
  janusTwinTx++;
  if (force || shareActive || (janusTwinTx % 12UL) == 1UL || !ok) {
    const char* race = "solo";
    if (janusTwinPeerFresh() && active && jt.jobFp32 == janusTwinPeerJobFp32) {
      if (bestBits > janusTwinPeerBestBits) race = "ahead";
      else if (bestBits < janusTwinPeerBestBits) race = "behind";
      else race = (hashRate >= janusTwinPeerHashRate) ? "speed_ahead" : "speed_behind";
    }
    Serial.printf("[ANCHOR/TWIN] tx=%s seq=%lu peer=%04X fresh=%u job=%u fp=%08lX checked=%lu H=%lu best=%lu peerBest=%lu race=%s flags=0x%04X\n",
                  ok ? "OK" : "FAIL", (unsigned long)jt.seq, janusTwinPeerNode,
                  janusTwinPeerFresh() ? 1 : 0, active ? 1 : 0, (unsigned long)jt.jobFp32,
                  (unsigned long)jt.checked, (unsigned long)hashRate, (unsigned long)bestBits,
                  (unsigned long)janusTwinPeerBestBits, race, (unsigned)jt.flags);
  }
#else
  (void)force;
#endif
}

bool janusTwinTaskReceive(const uint8_t* data, int len, int8_t rssi) {
#if JANUS_TWIN_TASK_ENABLE
  if (!data || len != (int)sizeof(JanusTwinTaskPacket) || data[0] != 'J' || data[1] != 'T') return false;
  JanusTwinTaskPacket jt{};
  memcpy(&jt, data, sizeof(jt));
  uint32_t got = jt.crc;
  jt.crc = 0;
  uint32_t calc = rfDomeCrc32(&jt, sizeof(jt) - 4);
  if (got && got != calc) return true;
  if (jt.nodeId == workerId) return true;

  janusTwinRx++;
  bool first = !janusTwinPeerLastMs || janusTwinPeerNode != jt.nodeId;
  bool peerShare = (jt.flags & JANUS_TWIN_FLAG_SHARE);
  janusTwinPeerLastMs = millis();
  janusTwinPeerNode = jt.nodeId;
  janusTwinPeerRole = jt.role;
  janusTwinPeerJobFp32 = jt.jobFp32;
  janusTwinPeerJobStart = jt.jobStart;
  janusTwinPeerJobRange = jt.jobRange;
  janusTwinPeerChecked = jt.checked;
  janusTwinPeerHashRate = jt.hashRate;
  janusTwinPeerBestBits = jt.bestBits;
  janusTwinPeerShares = jt.shares;
  janusTwinPeerLane = jt.lane;
  janusTwinPeerSector = jt.sector;
  janusTwinPeerTargetBits = jt.targetBits;
  janusTwinPeerStride = jt.stride;
  janusTwinPeerFlags = jt.flags;
  janusTwinPeerRssi = rssi;

  janusFacePeerLastMs = millis();
  janusFacePeerNode = jt.nodeId;
  janusFacePeerRoleFace = (jt.role == JANUS_TWIN_ROLE_GLADIUS) ? JANUS_FACE_ROLE_GLADIUS : JANUS_FACE_ROLE_ANCHOR;
  janusFacePeerFace = jt.face;
  janusFacePeerFlags = peerShare ? 0x01 : 0x00;
  janusFacePeerEventBits = jt.eventBits;
  janusFacePeerRssi = rssi;
  lastLedMs = 0;

  if (job.active) janusTwinApplyLaneSplit(job);
  if (job.active && jt.jobFp32 == janusTwinJobFp32From8(job.job_id)) {
    if (bestBits > jt.bestBits) janusTwinRaceWins++;
    else if (bestBits < jt.bestBits) janusTwinRaceLosses++;
  }

  if (first || peerShare || (janusTwinRx % 10UL) == 1UL) {
    Serial.printf("[ANCHOR/TWIN] rx peer=%04X role=%u job=%u fp=%08lX H=%lu best=%lu shares=%lu flags=0x%04X rssi=%d myBest=%lu wins=%lu losses=%lu\n",
                  jt.nodeId, (unsigned)jt.role, (jt.flags & JANUS_TWIN_FLAG_ACTIVE) ? 1 : 0,
                  (unsigned long)jt.jobFp32, (unsigned long)jt.hashRate, (unsigned long)jt.bestBits,
                  (unsigned long)jt.shares, (unsigned)jt.flags, (int)rssi, (unsigned long)bestBits,
                  (unsigned long)janusTwinRaceWins, (unsigned long)janusTwinRaceLosses);
  }
  return true;
#else
  (void)data; (void)len; (void)rssi;
  return false;
#endif
}

void janusTwinTaskTick(uint32_t now) {
#if JANUS_TWIN_TASK_ENABLE
  (void)now;
  janusTwinTaskBroadcast(false);
#else
  (void)now;
#endif
}



uint32_t janusJobFp32(const RemoteJobState& j) {
  return janusTwinJobFp32From8(j.job_id);
}

uint32_t janusJobFp32FromPacket(const JobPacket& jp) {
  return janusTwinJobFp32From8(jp.job_id);
}

RemoteJobState janusJobBuildFromPacket(const JobPacket& jp) {
  RemoteJobState j{};
  memcpy(j.job_id, jp.job_id, 8);
  memcpy(j.header, jp.header, 80);
  memcpy(j.target, jp.target, 32);
  j.startNonce = jp.start_nonce;
  j.rangeSize = jp.range_size ? jp.range_size : JOB_RANGE_DEFAULT;
  j.nonce = jp.start_nonce;
  j.hashesDone = 0;
  j.receivedAt = millis();
  j.active = true;
  return j;
}

bool janusJobSameStart(const RemoteJobState& a, const RemoteJobState& b) {
  return janusJobFp32(a) == janusJobFp32(b) && a.startNonce == b.startNonce && a.rangeSize == b.rangeSize;
}

void janusJobAccept(const RemoteJobState& incoming, const char* reason) {
  job = incoming;
  job.receivedAt = millis();
  job.hashesDone = 0;
  job.nonce = job.startNonce;
  job.active = true;
  configureMinerLane(job);
  targetBits = countLeadingZeroBitsBE(job.target);
  jobsAccepted++;
  lastMasterMs = millis();
  Serial.printf("[ANCHOR/JOB] accept=%lu seen=%lu reason=%s start=%08lX range=%lu fp=%08lX targetBits=%u lane=%s/s%u stride=%lu arm=%u q=%u qAge=%lums\n",
                (unsigned long)jobsAccepted, (unsigned long)jobsSeen, reason ? reason : "?",
                (unsigned long)job.startNonce, (unsigned long)job.rangeSize, (unsigned long)janusJobFp32(job),
                (unsigned)targetBits, laneName(job.minerLane), (unsigned)job.minerSector,
                (unsigned long)job.minerStride, (unsigned)job.minerStrideArm,
                queuedJobValid ? 1 : 0, (unsigned long)(queuedJobValid ? (janusSafeAgeMs(millis(), queuedJobAtMs, 0UL)) : 0UL));
  janusTwinTaskBroadcast(true);
}

void janusJobQueue(const RemoteJobState& incoming, const char* reason) {
#if JANUS_JOB_QUEUE_ENABLE
  queuedJob = incoming;
  queuedJob.active = true;
  queuedJob.hashesDone = 0;
  queuedJob.nonce = queuedJob.startNonce;
  queuedJobAtMs = millis();
  queuedJobFp32 = janusJobFp32(queuedJob);
  queuedJobValid = true;
  jobsQueued++;
  uint32_t now = millis();
  if (janusSafeAgeMs(now, lastJobQueueLogMs, 999999UL) >= JANUS_JOB_QUEUE_LOG_MS || jobsQueued <= 3UL) {
    lastJobQueueLogMs = now;
    Serial.printf("[ANCHOR/JOBQ] queued=%lu reason=%s fp=%08lX start=%08lX activeStart=%08lX activeChecked=%lu age=%lums min=%lums/%luH\n",
                  (unsigned long)jobsQueued, reason ? reason : "?", (unsigned long)queuedJobFp32,
                  (unsigned long)queuedJob.startNonce, (unsigned long)job.startNonce,
                  (unsigned long)job.hashesDone, (unsigned long)(job.active ? janusSafeAgeMs(now, job.receivedAt, 0UL) : 0UL),
                  (unsigned long)JANUS_JOB_MIN_WORK_MS, (unsigned long)JANUS_JOB_MIN_WORK_HASHES);
  }
#else
  (void)incoming; (void)reason;
#endif
}

bool janusJobPromoteQueued(const char* reason) {
#if JANUS_JOB_QUEUE_ENABLE
  if (!queuedJobValid) return false;
  RemoteJobState q = queuedJob;
  uint32_t qAge = janusSafeAgeMs(millis(), queuedJobAtMs, 0UL);
  queuedJobValid = false;
  queuedJobAtMs = 0;
  queuedJobFp32 = 0;
  jobsYielded++;
  Serial.printf("[ANCHOR/JOBQ] promote=%lu reason=%s qAge=%lums start=%08lX fp=%08lX\n",
                (unsigned long)jobsYielded, reason ? reason : "?", (unsigned long)qAge,
                (unsigned long)q.startNonce, (unsigned long)janusJobFp32(q));
  janusJobAccept(q, reason ? reason : "queue_promote");
  return true;
#else
  (void)reason;
  return false;
#endif
}

bool janusJobReadyToYield(uint32_t now) {
#if JANUS_JOB_QUEUE_ENABLE
  if (!job.active || !queuedJobValid) return false;
  if (janusJobFp32(job) != queuedJobFp32) return true; // safety: new work waiting
  uint32_t age = janusSafeAgeMs(now, job.receivedAt, 0UL);
  return (age >= JANUS_JOB_MIN_WORK_MS) || (job.hashesDone >= JANUS_JOB_MIN_WORK_HASHES);
#else
  (void)now;
  return false;
#endif
}

void janusJobHandlePacket(const JobPacket& jp) {
  uint32_t now = millis();
  rxJobs++;
  jobsSeen++;
  lastMasterMs = now;

  RemoteJobState incoming = janusJobBuildFromPacket(jp);
  uint32_t incomingFp = janusJobFp32(incoming);

  if (!job.active) {
    janusJobAccept(incoming, "idle_accept");
    return;
  }

  uint32_t currentFp = janusJobFp32(job);
  bool newPoolWork = (incomingFp != currentFp);

#if JANUS_JOB_DUP_START_DROP
  if (!newPoolWork && janusJobSameStart(job, incoming)) {
    jobsDroppedDuplicate++;
    if ((jobsDroppedDuplicate % 8UL) == 1UL) {
      Serial.printf("[ANCHOR/JOBQ] dupDrop=%lu fp=%08lX start=%08lX activeChecked=%lu\n",
                    (unsigned long)jobsDroppedDuplicate, (unsigned long)incomingFp,
                    (unsigned long)incoming.startNonce, (unsigned long)job.hashesDone);
    }
    return;
  }
  if (queuedJobValid && !newPoolWork && janusJobSameStart(queuedJob, incoming)) {
    jobsDroppedDuplicate++;
    return;
  }
#endif

#if JANUS_JOB_NEW_FP_REPLACE
  if (newPoolWork) {
    jobsReplacedNewWork++;
    queuedJobValid = false;
    Serial.printf("[ANCHOR/JOB] newfp_replace=%lu oldFp=%08lX newFp=%08lX oldChecked=%lu oldAge=%lums\n",
                  (unsigned long)jobsReplacedNewWork, (unsigned long)currentFp, (unsigned long)incomingFp,
                  (unsigned long)job.hashesDone, (unsigned long)(janusSafeAgeMs(now, job.receivedAt, 0UL)));
    janusJobAccept(incoming, "new_pool_work");
    return;
  }
#endif

#if JANUS_JOB_QUEUE_ENABLE
  uint32_t age = janusSafeAgeMs(now, job.receivedAt, 0UL);
  bool usefulSliceDone = (age >= JANUS_JOB_MIN_WORK_MS) || (job.hashesDone >= JANUS_JOB_MIN_WORK_HASHES);
  if (!usefulSliceDone) {
    janusJobQueue(incoming, "same_fp_hold_current");
    return;
  }

  // We have already given the current range a useful slice. Yield cleanly to the
  // newest same-fingerprint range instead of pretending the old one expired.
  jobsYielded++;
  Serial.printf("[ANCHOR/JOBQ] yield=%lu reason=same_fp_slice_done oldStart=%08lX oldChecked=%lu oldAge=%lums newStart=%08lX fp=%08lX\n",
                (unsigned long)jobsYielded, (unsigned long)job.startNonce, (unsigned long)job.hashesDone,
                (unsigned long)age, (unsigned long)incoming.startNonce, (unsigned long)incomingFp);
  queuedJobValid = false;
  janusJobAccept(incoming, "same_fp_yield");
#else
  janusJobAccept(incoming, "queue_disabled_replace");
#endif
}

void janusJobHousekeeping(uint32_t now) {
#if JANUS_JOB_QUEUE_ENABLE
  if (queuedJobValid && janusSafeElapsed(now, queuedJobAtMs, JANUS_JOB_QUEUE_MAX_AGE_MS)) {
    Serial.printf("[ANCHOR/JOBQ] stale_drop fp=%08lX start=%08lX age=%lums\n",
                  (unsigned long)queuedJobFp32, (unsigned long)queuedJob.startNonce,
                  (unsigned long)(janusSafeAgeMs(now, queuedJobAtMs, 0UL)));
    queuedJobValid = false;
    queuedJobAtMs = 0;
    queuedJobFp32 = 0;
  }
  if (!job.active) {
    janusJobPromoteQueued("idle_promote");
  } else if (janusJobReadyToYield(now)) {
    janusJobPromoteQueued("scheduled_yield");
  }
#else
  (void)now;
#endif
}

void anchorLedFlashShare(uint16_t bits) {
  lastShareMs = millis();
  lastShareBits = bits;
  lastLedMs = 0;
  janusFaceBroadcast(true, bits);
  janusTwinTaskBroadcast(true);
}

void sendShare(uint32_t nonce, uint16_t bits) {
  ShareResponse sr{};
  sr.magic[0] = 'S'; sr.magic[1] = 'R';
  memcpy(sr.job_id, job.job_id, 8);
  sr.nonce = nonce;
  sr.worker_id = workerId;
  bool ok = sendEspNow("S/R", &sr, sizeof(sr));
  esp_err_t directErr = sendEspNowToBuzzMaster("S/R-direct", &sr, sizeof(sr));
  if (ok || directErr == ESP_OK) shares++;
  anchorLedFlashShare(bits);
  Serial.printf("[ANCHOR/SHARE] tx=%s direct=%d share=%lu nonce=%08lX bits=%u targetBits=%u lane=%s/s%u H=%lu best=%lu tx=%lu/%lu directCnt=%lu/%lu\n",
                ok ? "OK" : "FAIL", (int)directErr, (unsigned long)shares, (unsigned long)nonce, (unsigned)bits,
                (unsigned)targetBits, laneName(job.minerLane), (unsigned)job.minerSector,
                (unsigned long)hashRate, (unsigned long)bestBits, (unsigned long)txOk, (unsigned long)txFail,
                (unsigned long)buzzMasterDirectOk, (unsigned long)buzzMasterDirectFail);
}

void runMining() {
  uint32_t now = millis();
  if (!job.active) {
    janusJobPromoteQueued("miner_idle");
    if (now - lastHashTickMs >= 1000) {
      hashRate = 0;
      hashCounter = 0;
      lastHashTickMs = now;
    }
    return;
  }
  if (janusSafeElapsed(now, job.receivedAt, JOB_TIMEOUT_MS)) {
    Serial.printf("[ANCHOR/JOB] timeout fp=%08lX start=%08lX checked=%lu age=%lums q=%u\n",
                  (unsigned long)janusJobFp32(job), (unsigned long)job.startNonce,
                  (unsigned long)job.hashesDone, (unsigned long)(janusSafeAgeMs(now, job.receivedAt, 0UL)), queuedJobValid ? 1 : 0);
    job.active = false;
    jobsExpired++;
    hashRate = 0;
    janusJobPromoteQueued("timeout_promote");
    return;
  }

  uint8_t header[80];
  uint8_t rawHash[32];
  uint8_t shareHash[32];
  uint16_t batch = activeBatch();

  for (uint16_t i = 0; i < batch; ++i) {
    if (job.hashesDone >= job.rangeSize) {
      job.active = false;
      jobsDone++;
      Serial.printf("[ANCHOR/JOB] range_done done=%lu fp=%08lX start=%08lX checked=%lu best=%lu q=%u\n",
                    (unsigned long)jobsDone, (unsigned long)janusJobFp32(job),
                    (unsigned long)job.startNonce, (unsigned long)job.hashesDone,
                    (unsigned long)bestBits, queuedJobValid ? 1 : 0);
      janusJobPromoteQueued("range_done_promote");
      break;
    }
    uint32_t nonce = nextNonce(job, job.hashesDone);
    job.nonce = nonce + 1;
    job.hashesDone++;

    memcpy(header, job.header, 80);
    writeLE32(header + 76, nonce);
    doubleSha256(header, 80, rawHash);
    hashToShareOrder(rawHash, shareHash);

    hashCounter++;
    totalHashesLifetime++;
    uint16_t bits = countLeadingZeroBitsBE(shareHash);
    if (bits > bestBits) {
      bestBits = bits;
      bestNonce = nonce;
      if (bits >= 16) {
        float lift = 0.55f + (float)(bits - 15) * 0.18f;
        if (janusTwinSameBuzzWindow(job)) lift += (janusTwinPeerBestBits >= bits) ? 1.20f : 0.45f;
        anchorOxytocin = constrain(anchorOxytocin + lift, 0.0f, 100.0f);
      }
      if (bits >= 16) {
        Serial.printf("[ANCHOR/BEST] bits=%u nonce=%08lX lane=%s/s%u stride=%lu arm=%u checked=%lu/%lu fp=%08lX\n",
                      (unsigned)bits, (unsigned long)nonce, laneName(job.minerLane), (unsigned)job.minerSector,
                      (unsigned long)job.minerStride, (unsigned)job.minerStrideArm,
                      (unsigned long)job.hashesDone, (unsigned long)job.rangeSize, (unsigned long)janusJobFp32(job));
      }
    }
    if (bits >= 22) tailHits++;

    if ((bits >= targetBits) && hashMeetsTargetBE(shareHash, job.target)) {
      anchorOxytocin = constrain(anchorOxytocin + 12.0f, 0.0f, 100.0f);
      anchorTorricelliVacuum = constrain(anchorTorricelliVacuum + 0.08f, 0.0f, 1.0f);
      sendShare(nonce, bits);
      job.active = false;
      jobsDone++;
      Serial.printf("[ANCHOR/TICKET] nonce=%08lX bits=%u lane=%s/s%u fp=%08lX\n", (unsigned long)nonce, (unsigned)bits, laneName(job.minerLane), (unsigned)job.minerSector, (unsigned long)janusJobFp32(job));
      janusJobPromoteQueued("ticket_promote");
      break;
    }
  }

  if (now - lastHashTickMs >= 1000) {
    hashRate = hashCounter;
    hashRateEma = (hashRateEma <= 0.1f) ? (float)hashRate : (hashRateEma * 0.78f + (float)hashRate * 0.22f);
    hashCounter = 0;
    lastHashTickMs = now;
  }

  janusJobHousekeeping(now);
}

bool validRssi(int v) { return v < -5 && v > -126; }

uint32_t rfDomeCrc32(const void* data, size_t len) {
  const uint8_t* p = (const uint8_t*)data;
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < len; i++) { h ^= p[i]; h *= 16777619UL; }
  return h;
}

void rfDomeOnCorePing(const RfDomePingPacket& ping, int8_t rssi) {
#if RF_DOME_ENABLE
  (void)ping;
  if (!validRssi((int)rssi)) return;
  uint32_t now = millis();
  rfDomeRxPing++;
  rfDomeLastPingMs = now;
  rfDomeCoreRssi = rssi;
  if (!rfDomeReady) { rfDomeCoreEma = (float)rssi; rfDomeCoreBase = (float)rssi; rfDomeVar = 4.0f; rfDomeDelta = 0.0f; rfDomeReady = true; }
  float prev = rfDomeCoreEma;
  rfDomeCoreEma = rfDomeCoreEma * 0.78f + (float)rssi * 0.22f;
  float step = fabsf((float)rssi - prev);
  rfDomeDelta = fabsf(rfDomeCoreEma - rfDomeCoreBase);
  bool hot = (step > 2.5f) || (rfDomeDelta > max(3.8f, sqrtf(max(rfDomeVar, 1.0f)) * 2.15f));
  float alpha = hot ? RF_DOME_LEARN_ALPHA_HOT : RF_DOME_LEARN_ALPHA_COLD;
  rfDomeCoreBase = rfDomeCoreBase * (1.0f - alpha) + rfDomeCoreEma * alpha;
  if (!hot) rfDomeVar = rfDomeVar * 0.965f + (step * step) * 0.035f;
  rfDomeVar = constrain(rfDomeVar, 1.0f, 160.0f);
  float noise = sqrtf(max(rfDomeVar, 1.0f));
  float motionKick = constrain((step + rfDomeDelta * 0.36f) / max(1.4f, noise * 0.82f), 0.0f, 9.0f);
  float presenceKick = constrain(rfDomeDelta / max(2.8f, noise * 1.95f), 0.0f, 6.0f);
  rfDomeMotion = constrain(rfDomeMotion * 0.70f + motionKick * 0.30f, 0.0f, 10.0f);
  rfDomePresence = constrain(rfDomePresence * 0.86f + presenceKick * 0.14f, 0.0f, 6.0f);
  float motionFast = constrain((rfDomeMotion - 1.0f) / 4.0f, 0.0f, 1.0f);
  float bodySlow = constrain((rfDomePresence - 0.35f) / 2.6f, 0.0f, 1.0f);
  rfDomeHuman = constrain(rfDomeHuman * 0.82f + (bodySlow * 0.72f + motionFast * 0.28f) * 0.18f, 0.0f, 1.0f);
  rfDomePet = constrain(rfDomePet * 0.84f + (motionFast * (1.0f - bodySlow * 0.45f)) * 0.16f, 0.0f, 1.0f);
  float signedDelta = rfDomeCoreBase - rfDomeCoreEma;
  float centerPull = constrain((signedDelta + 5.0f) / 10.0f, 0.0f, 1.0f);
  float wobble = 0.5f + 0.5f * sinf((float)now * 0.0042f + rfDomeMotion * 0.31f);
  float zone = 50.0f + (centerPull - 0.5f) * 36.0f + (wobble - 0.5f) * 12.0f;
  rfDomeZonePct = (uint8_t)constrain((int)zone, 5, 95);
  rfDomeDistanceCm = (uint16_t)constrain((int)((RF_DOME_DEFAULT_LENGTH_CM * (uint32_t)rfDomeZonePct) / 100UL), 20, 900);
  float conf = 8.0f + min(38.0f, (float)rfDomeRxPing * 1.7f) + rfDomePresence * 13.0f + rfDomeMotion * 4.5f + rfDomeHuman * 18.0f;
  rfDomeConfidence = (uint8_t)constrain((int)conf, 0, 100);
#endif
}

void sendRfDome(bool force) {
#if RF_DOME_ENABLE
  uint32_t now = millis();
  bool coreFresh = rfDomeLastPingMs && janusSafeAgeMs(now, rfDomeLastPingMs, 999999UL) < RF_DOME_CORE_TIMEOUT_MS;
  bool alert = (rfDomePresence > 0.45f || rfDomeMotion > 1.20f || rfDomeHuman > 0.28f || rfDomePet > 0.32f);
  uint32_t interval = alert ? RF_DOME_ALERT_TX_MS : RF_DOME_TX_MS;
  if (!force && janusSafeAgeMs(now, rfDomeLastTxMs, 0UL) < interval) return;
  rfDomeLastTxMs = now;
  RfDomeSonarPacket rs{};
  rs.magic[0] = 'R'; rs.magic[1] = 'S'; rs.version = 1;
  if (coreFresh) rs.flags |= 0x01;
  if (rfDomePresence > 0.45f) rs.flags |= 0x02;
  if (rfDomeMotion > 1.20f) rs.flags |= 0x04;
  if (rfDomeHuman > 0.34f) rs.flags |= 0x08;
  if (rfDomePet > 0.38f) rs.flags |= 0x10;
  if (rfDomeRxPing < RF_DOME_MIN_CONF_PACKETS) rs.flags |= 0x20;
  strlcpy(rs.anchorId, JANUS_NODE_ID, sizeof(rs.anchorId));
  rs.seq = ++rfDomeSeq; rs.uptimeMs = now; rs.coreRssi = rfDomeCoreRssi; rs.ambientRssi = lastRssi;
  rs.coreEma_x10 = (int16_t)constrain((int)(rfDomeCoreEma * 10.0f), -32768, 32767);
  rs.coreBase_x10 = (int16_t)constrain((int)(rfDomeCoreBase * 10.0f), -32768, 32767);
  rs.coreDelta_x10 = (int16_t)constrain((int)(rfDomeDelta * 10.0f), -32768, 32767);
  rs.coreVar_x10 = (uint16_t)constrain((int)(rfDomeVar * 10.0f), 0, 65535);
  rs.motion_x100 = (uint16_t)constrain((int)(rfDomeMotion * 100.0f), 0, 65535);
  rs.presence_x100 = (uint16_t)constrain((int)(rfDomePresence * 100.0f), 0, 65535);
  rs.human_x100 = (uint16_t)constrain((int)(rfDomeHuman * 100.0f), 0, 10000);
  rs.pet_x100 = (uint16_t)constrain((int)(rfDomePet * 100.0f), 0, 10000);
  rs.zonePct = rfDomeZonePct; rs.distanceCm = rfDomeDistanceCm; rs.confidence = rfDomeConfidence; rs.domeLengthCm = RF_DOME_DEFAULT_LENGTH_CM; rs.packetsSeen = rfDomeRxPing;
  rs.crc = 0; rs.crc = rfDomeCrc32(&rs, sizeof(rs));
  bool ok = sendEspNow("R/S", &rs, sizeof(rs));
  rfDomeTx++;
  bool rfDomeShouldLog = force || !ok || ((rfDomeTx % 12UL) == 1UL);
  if (alert && janusSafeElapsed(now, rfDomeLastLogMs, RF_DOME_LOG_MS)) rfDomeShouldLog = true;
  if (rfDomeShouldLog) {
    rfDomeLastLogMs = now;
    Serial.printf("[RF/DOME] tx=%s seq=%lu coreFresh=%u rssi=%d ema=%.1f base=%.1f d=%.1f var=%.1f P=%.2f M=%.2f human=%.2f pet=%.2f zone=%u dist=%ucm conf=%u flags=0x%02X pings=%lu log=throttle/%lums\n",
                  ok ? "OK" : "FAIL", (unsigned long)rs.seq, coreFresh ? 1 : 0, (int)rs.coreRssi,
                  rfDomeCoreEma, rfDomeCoreBase, rfDomeDelta, rfDomeVar, rfDomePresence, rfDomeMotion, rfDomeHuman, rfDomePet,
                  (unsigned)rs.zonePct, (unsigned)rs.distanceCm, (unsigned)rs.confidence, (unsigned)rs.flags, (unsigned long)rfDomeRxPing,
                  (unsigned long)RF_DOME_LOG_MS);
  }
#endif
}

void rfOnPacketRssi(int8_t rssi) {
  if (!validRssi((int)rssi)) return;
  rfRxPackets++;
  rfLastPacketMs = millis();
  if (validRssi((int)lastRssi)) rfLastPacketDrift = fabsf((float)rssi - (float)lastRssi);
  lastRssi = rssi;
  float kick = constrain(rfLastPacketDrift / 12.0f, 0.0f, 1.5f);
  rfPacketPressure = constrain(rfPacketPressure * 0.82f + kick * 0.18f, 0.0f, 2.0f);
}

void rfTick(uint32_t now) {
  if (now - lastRfSampleMs < RF_SAMPLE_MS) return;
  lastRfSampleMs = now;
  int rssiNow = validRssi((int)lastRssi) ? lastRssi : ((WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127);
  if (!validRssi(rssiNow)) {
    rfPresence *= 0.94f;
    rfMotion *= 0.90f;
    rfEntropy *= 0.92f;
    rfPacketPressure *= 0.96f;
    rfPresenceNow = false;
    rfMotionNow = false;
    return;
  }
  if (!rfReady) {
    rfEma = (float)rssiNow;
    rfBase = (float)rssiNow;
    rfNoise = 3.0f;
    rfReady = true;
    return;
  }
  float prev = rfEma;
  rfEma = rfEma * 0.86f + (float)rssiNow * 0.14f;
  float step = fabsf((float)rssiNow - prev);
  rfDrift = fabsf(rfEma - rfBase);
  bool hot = step > 3.0f || rfDrift > max(4.5f, rfNoise * 1.85f) || rfPacketPressure > 0.65f;
  float alpha = hot ? 0.0007f : 0.0060f;
  rfBase = rfBase * (1.0f - alpha) + rfEma * alpha;
  if (!hot) rfNoise = rfNoise * 0.975f + step * 0.025f;
  rfNoise = constrain(rfNoise, 1.2f, 12.0f);

  float motionNorm = constrain((step + rfLastPacketDrift * 0.55f) / 3.2f, 0.0f, 5.0f);
  float presenceNorm = constrain(rfDrift / max(rfNoise * 2.2f + 1.0f, 1.0f), 0.0f, 4.0f);
  rfMotion = constrain(rfMotion * 0.78f + motionNorm * 0.22f + rfPacketPressure * 0.08f, 0.0f, 12.0f);
  rfPresence = constrain(rfPresence * 0.88f + (presenceNorm + rfPacketPressure * 0.20f) * 0.12f, 0.0f, 3.0f);
  rfEntropy = constrain(rfEntropy * 0.86f + (motionNorm * 0.22f + presenceNorm * 0.30f + rfPacketPressure * 0.24f) * 0.14f, 0.0f, 4.0f);
  rfPresenceNow = rfPresence > 0.42f;
  rfMotionNow = rfMotion > 1.05f || motionNorm > 1.65f;
  rfPacketPressure *= 0.985f;
  rfLastPacketDrift *= 0.92f;
}

float localEntropy() {
  float agent = (float)(agentEntropySeed & 0xFFFF) / 65535.0f;
  return constrain(rfEntropy * 0.85f + rfMotion * 0.08f + rfPresence * 0.35f + rfPacketPressure * 0.50f + agent * 0.75f + (float)agentLevel * 0.10f, 0.0f, 9999.0f);
}

void sendHeartbeat() {
  JanusColonyPacket pkt{};
  memcpy(pkt.magic, "JANUS", 6);
  strlcpy(pkt.nodeId, JANUS_NODE_ID, sizeof(pkt.nodeId));
  strlcpy(pkt.role, JANUS_NODE_ROLE, sizeof(pkt.role));
  pkt.seq = ++seqNo;
  pkt.hashRate = hashRate;
  pkt.shares = shares;
  pkt.rejects = rejects;
  pkt.bestBits = bestBits;
  pkt.diff = 0.0f;
  pkt.targetBits = targetBits;
  pkt.aiBatch = activeBatch();
  pkt.aiHint = anchorSchedulerHint();
  pkt.jobAgeMs = job.active ? janusSafeAgeMs(millis(), job.receivedAt, 0UL) : 0;
  pkt.rssi = lastRssi;
  pkt.uptime = millis();
  bool ok = sendEspNow("JANUS", &pkt, sizeof(pkt));
  esp_err_t directErr = sendEspNowToBuzzMaster("JANUS-direct", &pkt, sizeof(pkt));
  heartbeatTx++;
  if ((ANCHOR_TX_LOG_EVERY <= 1UL) || ((heartbeatTx % ANCHOR_TX_LOG_EVERY) == 1UL) || !ok || (buzzMasterMacKnown && directErr != ESP_OK)) {
    Serial.printf("[ANCHOR/HB] tx=%s direct=%d known=%u n=%lu seq=%lu ch=%u peerCh=%u masterAge=%lums H=%lu best=%lu shares=%lu directCnt=%lu/%lu missing=%lu\n",
                  ok ? "OK" : "FAIL", (int)directErr, buzzMasterMacKnown ? 1 : 0,
                  (unsigned long)heartbeatTx, (unsigned long)pkt.seq,
                  (unsigned)peerChannel, (unsigned)buzzMasterPeerChannel,
                  (unsigned long)(lastMasterMs ? janusSafeAgeMs(millis(), lastMasterMs, 999999UL) : 999999UL),
                  (unsigned long)hashRate, (unsigned long)bestBits, (unsigned long)shares,
                  (unsigned long)buzzMasterDirectOk, (unsigned long)buzzMasterDirectFail,
                  (unsigned long)buzzMasterMacMissing);
  }
}

void sendEntropy() {
  EntropyReportV2 er{};
  er.magic[0] = 'E'; er.magic[1] = '2';
  er.worker_id = workerId;
  strlcpy(er.nodeId, JANUS_NODE_ID, sizeof(er.nodeId));
  er.local_entropy = localEntropy();
  er.prediction_error = rfDrift * 0.015f;
  er.sync_hint = constrain((rfReady ? 0.55f : 0.0f) + anchorTorricelliVacuum * 0.34f + anchorOxytocin * 0.0016f, 0.0f, 1.0f);
  er.fit = constrain(anchorOxytocin / 100.0f, 0.0f, 1.0f);
  er.sensor_flags = 0x88; // RF + worker telemetry
  er.values[0] = rfPresence;
  er.values[1] = rfMotion;
  er.values[2] = rfEntropy;
  er.values[3] = rfDrift;
  er.values[4] = rfNoise;
  er.values[5] = rfPacketPressure;
  er.values[6] = (float)hashRate;
  er.values[7] = (float)bestBits;
  er.uptime_ms = millis();
  bool ok = sendEspNow("E2", &er, sizeof(er));
  entropyTx++;
  if ((ANCHOR_TX_LOG_EVERY <= 1UL) || ((entropyTx % ANCHOR_TX_LOG_EVERY) == 1UL) || !ok) {
    Serial.printf("[ANCHOR/E2] tx=%s n=%lu entropy=%.2f rfP=%.2f rfM=%.2f drift=%.1f H=%lu best=%lu\n",
                  ok ? "OK" : "FAIL", (unsigned long)entropyTx, er.local_entropy,
                  rfPresence, rfMotion, rfDrift, (unsigned long)hashRate, (unsigned long)bestBits);
  }
}

void sendSwarmSense(bool force=false) {
  uint32_t now = millis();
  uint32_t interval = (rfPresenceNow || rfMotionNow || bestBits >= 22) ? SWARMSENSE_ALERT_MS : SWARMSENSE_TX_MS;
  if (!force && now - lastSwarmSenseMs < interval) return;
  lastSwarmSenseMs = now;

  SwarmSensePacket ss{};
  ss.magic[0] = 'S'; ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = workerId;
  strlcpy(ss.nodeId, JANUS_NODE_ID, sizeof(ss.nodeId));
  strlcpy(ss.kind, "rf_anchor_aux", sizeof(ss.kind));
  ss.seq = ++ssSeq;
  ss.uptime_ms = now;
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = 0;
  ss.loop_max_us = RF_SAMPLE_MS;
  ss.rssi = lastRssi;
  ss.radio_mode = 1;
  ss.bt_flags = 0;
  if (rfPresenceNow) ss.bt_flags |= 0x01;
  if (rfMotionNow) ss.bt_flags |= 0x02;
  if (janusTwinPeerFresh()) ss.bt_flags |= 0x20;
  if (rfReady) ss.bt_flags |= 0x40;
  if (rfEntropy > 1.18f) ss.bt_flags |= 0x80;
  ss.palette = 2;
  ss.knn_label = job.minerSector;
  ss.knn_confidence = (uint8_t)constrain(max((int)(rfPresence * 100.0f), (int)anchorOxytocin), 0, 100);
  ss.ai_hint = max<uint8_t>((anchorOxytocin > 72.0f || bestBits >= 22 || rfEntropy > 1.18f) ? 3 : 1,
                            anchorSchedulerHint());
  ss.thermal_load = 0;
  ss.effective_batch = activeBatch();
  ss.dynamic_batch = activeBatch();
  ss.hash_rate = hashRate;
  ss.total_hashes = job.hashesDone;
  ss.best_bits = bestBits;
  ss.hash_eff_x1000 = (uint16_t)constrain((int)hashRate, 0, 65535);
  ss.prediction_error_x1000 = (int16_t)constrain((int)(rfDrift * 15.0f), -32768, 32767);
  ss.entropy_x1000 = (uint16_t)constrain((int)(localEntropy() * 100.0f), 0, 65535);
  ss.touch_delta = (uint16_t)constrain((int)(rfPresence * 100.0f + rfMotion * 12.0f), 0, 65535);
  ss.job_age_s = job.active ? (uint16_t)min(65535UL, (janusSafeAgeMs(now, job.receivedAt, 0UL)) / 1000UL) : 65535U;
  ss.nonce_remaining_l16 = (job.active && job.rangeSize > job.hashesDone) ? (uint16_t)((job.rangeSize - job.hashesDone) & 0xFFFF) : 0;
  ss.flags = ((uint16_t)job.minerLane << 8) | (uint16_t)(job.minerSector & 0xFF);
  if (anchorOxytocin > 64.0f) ss.flags |= 0x0020;
  bool ok = sendEspNow("S/S", &ss, sizeof(ss));
  swarmSenseTx++;
  if (force || (ANCHOR_TX_LOG_EVERY <= 1UL) || ((swarmSenseTx % ANCHOR_TX_LOG_EVERY) == 1UL) || !ok) {
    Serial.printf("[ANCHOR/SENSE] tx=%s n=%lu flags=0x%02X kind=%s rfP=%.2f rfM=%.2f ent=%.2f job=%u batch=%u\n",
                  ok ? "OK" : "FAIL", (unsigned long)swarmSenseTx, (unsigned)ss.bt_flags,
                  ss.kind, rfPresence, rfMotion, rfEntropy, job.active ? 1 : 0, (unsigned)activeBatch());
  }
}

uint16_t anchorScaleX1000(float v, float lo, float hi) {
  if (hi <= lo) return 0;
  float span = hi - lo;
  return (uint16_t)constrain((int)((v - lo) * 1000.0f / span + 0.5f), 0, 65535);
}

uint32_t anchorCurrentJobSignature() {
  uint32_t h = 0xA9C40A11UL ^ ((uint32_t)workerId << 16) ^ janusJobFp32(job);
  h ^= job.startNonce ^ job.rangeSize ^ job.minerSeed ^ job.minerStride;
  h ^= ((uint32_t)job.minerLane << 24) ^ ((uint32_t)job.minerSector << 16) ^
       ((uint32_t)job.minerStrideArm << 8) ^ (uint32_t)targetBits;
  return rfDomeCrc32(&h, sizeof(h));
}

void sendAnchorPnCortex(bool force) {
  uint32_t now = millis();
  if (!force && anchorPnLastMs && janusSafeAgeMs(now, anchorPnLastMs, 0UL) < ANCHOR_PN_CORTEX_MS) return;
  anchorPnLastMs = now;

  uint16_t batch = activeBatch();
  float heapPressure = 1.0f - constrain((float)ESP.getFreeHeap() / 240000.0f, 0.0f, 1.0f);
  float hashLoad = constrain((float)hashRate / 14000.0f, 0.0f, 2.5f);
  float batchLoad = constrain((float)batch / (float)REMOTE_BATCH_MAX, 0.0f, 1.3f);
  float rfBody = constrain(rfPresence * 0.34f + rfMotion * 0.10f + rfEntropy * 0.20f + rfPacketPressure * 0.16f, 0.0f, 4.0f);
  float dome = constrain(rfDomePresence * 0.30f + rfDomeMotion * 0.08f + rfDomeHuman * 0.34f + rfDomePet * 0.22f, 0.0f, 4.0f);
  float oxy = constrain(anchorOxytocin / 100.0f, 0.0f, 1.0f);
  float thermal = constrain(0.10f + hashLoad * 0.42f + batchLoad * 0.18f + heapPressure * 0.22f + rfBody * 0.10f + oxy * 0.08f, 0.0f, 4.0f);
  float load = constrain(0.12f + hashLoad * 0.52f + batchLoad * 0.25f + (job.active ? 0.16f : 0.0f) + dome * 0.10f + oxy * 0.16f, 0.0f, 4.0f);
  float entropy = constrain(localEntropy() * 0.20f + rfEntropy * 0.45f + dome * 0.25f + anchorTorricelliVacuum * 0.42f + oxy * 0.28f, 0.0f, 6.0f);
  uint16_t targetForTail = targetBits ? targetBits : 22;
  if (targetForTail < 1) targetForTail = 1;
  float tail = constrain(((float)bestBits / (float)targetForTail) + (float)tailHits / 120.0f, 0.0f, 6.0f);

  JanusPnCortexPacket pn{};
  pn.magic[0] = 'P'; pn.magic[1] = 'N';
  pn.version = 1;
  pn.role = JANUS_ROLE_ANCHOR_PN;
  pn.worker_id = workerId;
  strlcpy(pn.nodeId, JANUS_NODE_ID, sizeof(pn.nodeId));
  strlcpy(pn.kind, "anchor_pn_lab", sizeof(pn.kind));
  pn.seq = ++anchorPnSeq;
  pn.uptime_ms = now;
  pn.job_sig = anchorCurrentJobSignature();
  pn.prev_hash = anchorPnPrevHash;
  pn.hash_rate = hashRate;
  pn.total_hashes = totalHashesLifetime;
  pn.target_bits = targetBits ? targetBits : 22;
  pn.best_bits = (uint16_t)min(65535UL, bestBits);
  pn.lane = job.minerLane;
  pn.sector = job.minerSector;
  pn.flags = 0;
  if (job.active) pn.flags |= 0x01;
  if (lastMasterMs && janusSafeAgeMs(now, lastMasterMs, 999999UL) < MASTER_TIMEOUT_MS) pn.flags |= 0x02;
  if (rfDomeReady || rfDomeRxPing) pn.flags |= 0x04;
  if (janusTwinPeerFresh()) pn.flags |= 0x08;
  if (agentLevel || agentEntropySeed) pn.flags |= 0x10;
  if (anchorOxytocin > 64.0f || anchorTorricelliVacuum > 0.66f) pn.flags |= 0x20;
  if (anchorTranceptionHint >= 3) pn.flags |= 0x40;
  pn.rssi = lastRssi;
  pn.thermal_x1000 = anchorScaleX1000(thermal, 0.0f, 4.0f);
  pn.load_x1000 = anchorScaleX1000(load, 0.0f, 4.0f);
  pn.jitter_us = anchorLoopJitterUs;
  pn.entropy_x1000 = anchorScaleX1000(entropy, 0.0f, 6.0f);
  pn.tail_x1000 = anchorScaleX1000(tail, 0.0f, 6.0f);
  pn.voltage_mv = 0;
  pn.ir_phase = (uint16_t)((pn.job_sig ^ pn.prev_hash ^ ((uint32_t)rfDomeZonePct << 8) ^
                            ((uint32_t)bestBits << 3) ^ job.minerStride ^
                            ((uint32_t)(anchorOxytocin * 10.0f) << 1) ^
                            ((uint32_t)(anchorTranceptionLiteScore * 1000.0f) << 4) ^
                            ((uint32_t)anchorTranceptionLane << 12)) & 0xFFFFUL);
  pn.reserved = (uint16_t)constrain((int)(anchorOxytocin * 10.0f), 0, 1000);
  pn.packet_hash = 0;
  pn.packet_hash = rfDomeCrc32(&pn, sizeof(pn));
  anchorPnPrevHash = pn.packet_hash;

  bool ok = sendEspNow("P/N", &pn, sizeof(pn));
  if (ok) {
    anchorPnTx++;
    if ((anchorPnTx & 0x07UL) == 1UL) {
      Serial.printf("[ANCHOR/PN] tx=%lu lane=%s/s%u H=%lu best=%u/%u heat=%.2f load=%.2f rf=%.2f tail=%.2f oxy=%.1f vac=%.2f tl=%.2f/%u flags=0x%02X\n",
                    (unsigned long)anchorPnTx, laneName(pn.lane), (unsigned)pn.sector,
                    (unsigned long)pn.hash_rate, (unsigned)pn.best_bits, (unsigned)pn.target_bits,
                    thermal, load, rfBody, tail, anchorOxytocin, anchorTorricelliVacuum,
                    anchorTranceptionLiteScore, (unsigned)anchorTranceptionHint, (unsigned)pn.flags);
    }
  } else {
    anchorPnFail++;
    if ((anchorPnFail & 0x07UL) == 1UL) {
      Serial.printf("[ANCHOR/PN] fail=%lu lastErr=%d\n", (unsigned long)anchorPnFail, lastTxErr);
    }
  }
}

void anchorPresenceBurst(const char* reason) {
  uint32_t now = millis();
  if (anchorLastPresenceBurstMs &&
      janusSafeAgeMs(now, anchorLastPresenceBurstMs, 0UL) < ANCHOR_PRESENCE_BURST_MIN_MS) {
    return;
  }
  anchorLastPresenceBurstMs = now;
  anchorPresenceBursts++;

  lastHeartbeatMs = 0;
  lastEntropyMs = 0;
  lastSwarmSenseMs = 0;
  anchorPnLastMs = 0;
  sendHeartbeat();
  sendEntropy();
  sendSwarmSense(true);
  sendAnchorPnCortex(true);
  janusTwinTaskBroadcast(true);

  uint32_t masterAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  uint32_t anyRxAge = lastAnyRxMs ? janusSafeAgeMs(now, lastAnyRxMs, 999999UL) : 999999UL;
  Serial.printf("[ANCHOR/PRESENCE] burst=%lu reason=%s masterAge=%lums anyRxAge=%lums H=%lu best=%lu job=%u tx=%lu/%lu direct=%lu/%lu known=%u cb=%lu/%lu heap=%lu\n",
                (unsigned long)anchorPresenceBursts,
                reason ? reason : "?",
                (unsigned long)masterAge,
                (unsigned long)anyRxAge,
                (unsigned long)hashRate,
                (unsigned long)bestBits,
                job.active ? 1 : 0,
                (unsigned long)txOk,
                (unsigned long)txFail,
                (unsigned long)buzzMasterDirectOk,
                (unsigned long)buzzMasterDirectFail,
                buzzMasterMacKnown ? 1 : 0,
                (unsigned long)sentCbOk,
                (unsigned long)sentCbFail,
                (unsigned long)ESP.getFreeHeap());
}

void rfDebug(uint32_t now, bool force=false) {
  if (!force && now - lastRfDebugMs < RF_DEBUG_MS) return;
  lastRfDebugMs = now;
  uint32_t age = rfLastPacketMs ? janusSafeAgeMs(now, rfLastPacketMs, 999999UL) : 999999UL;
  Serial.printf("[ANCHOR/RF] ready=%u rssi=%d ema=%.1f base=%.1f noise=%.1f drift=%.1f P=%.2f M=%.2f entropy=%.2f pkt=%lu age=%lums pr=%.2f lane=%s/s%u stride=%lu arm=%u H=%lu best=%lu tail=%lu tx=%lu/%lu\n",
                rfReady ? 1 : 0, (int)lastRssi, rfEma, rfBase, rfNoise, rfDrift,
                rfPresence, rfMotion, rfEntropy, (unsigned long)rfRxPackets, (unsigned long)age,
                rfPacketPressure, laneName(job.minerLane), (unsigned)job.minerSector,
                (unsigned long)job.minerStride, (unsigned)job.minerStrideArm,
                (unsigned long)hashRate, (unsigned long)bestBits, (unsigned long)tailHits,
                (unsigned long)txOk, (unsigned long)txFail);
}


bool auxLedPinValid(int pin) {
  if (pin < 0 || pin > 48) return false;
#if ANCHOR_LED_ENABLE
  if (pin == ANCHOR_LED_PIN) return false; // do not fight the addressable RGB pin
#endif
  return true;
}

void anchorSmallLedWriteRaw(bool on) {
  if (!auxLedPinValid(ANCHOR_EXTRA_BLUE_PIN)) return;
  bool level = ANCHOR_EXTRA_BLUE_ACTIVE_LOW ? !on : on;
  digitalWrite(ANCHOR_EXTRA_BLUE_PIN, level ? HIGH : LOW);
  anchorSmallLedLastState = on;
}

void auxLedOffPin(int pin, bool activeLow) {
  if (!auxLedPinValid(pin)) return;
  digitalWrite(pin, activeLow ? HIGH : LOW);
}

void setupExtraLeds() {
#if ANCHOR_EXTRA_LED_OFF_ENABLE
  if (auxLedPinValid(ANCHOR_EXTRA_BLUE_PIN)) {
    pinMode(ANCHOR_EXTRA_BLUE_PIN, OUTPUT);
    anchorSmallLedWriteRaw(false);
  }
  if (auxLedPinValid(ANCHOR_EXTRA_YELLOW_PIN)) {
    pinMode(ANCHOR_EXTRA_YELLOW_PIN, OUTPUT);
    auxLedOffPin(ANCHOR_EXTRA_YELLOW_PIN, ANCHOR_EXTRA_YELLOW_ACTIVE_LOW);
  }
  Serial.printf("[ANCHOR/EXTRA_LED] smallLedPin=%d smallLed=%u tiedToUART0FullLog=1 yellowPin=%d note='PWR LED may be hardware-only'\n",
                (int)ANCHOR_EXTRA_BLUE_PIN, anchorSmallLedEnabled ? 1 : 0, (int)ANCHOR_EXTRA_YELLOW_PIN);
#else
  Serial.println("[ANCHOR/EXTRA_LED] disabled");
#endif
}

void extraLedTick(uint32_t now) {
#if ANCHOR_EXTRA_LED_OFF_ENABLE
  bool on = false;
  if (anchorSmallLedEnabled) {
    if (lastMaxBrightnessFlashMs && janusSafeAgeMs(now, lastMaxBrightnessFlashMs, 0UL) < ANCHOR_LED_MAX_FLASH_MS) {
      on = true;
    } else if (lastShareMs && janusSafeAgeMs(now, lastShareMs, 0UL) < ANCHOR_LED_SHARE_MS) {
      on = ((now / 110UL) % 2UL) == 0; // SHAR/Share nervous blink
    } else if (job.active) {
      on = ((now / 520UL) % 2UL) == 0; // worker alive blink
    } else {
      uint32_t m = now % 2400UL;       // calm double heartbeat
      on = (m < 75UL) || (m > 245UL && m < 315UL);
    }
  }
  if (on != anchorSmallLedLastState) anchorSmallLedWriteRaw(on);
  auxLedOffPin(ANCHOR_EXTRA_YELLOW_PIN, ANCHOR_EXTRA_YELLOW_ACTIVE_LOW);
#else
  (void)now;
#endif
}

void anchorLedWrite(uint8_t r, uint8_t g, uint8_t b) {
#if ANCHOR_LED_ENABLE
  // Arduino-ESP32 3.x: use rgbLedWrite() to avoid the noisy deprecated
  // neopixelWrite() warning spam in Serial Monitor. Older cores fall back.
  #if ESP_ARDUINO_VERSION_MAJOR >= 3
    rgbLedWrite(ANCHOR_LED_PIN, r, g, b);
  #else
    neopixelWrite(ANCHOR_LED_PIN, r, g, b);
  #endif
#else
  (void)r; (void)g; (void)b;
#endif
}

uint8_t scaleLed(uint8_t v) {
  return (uint8_t)(((uint16_t)v * (uint16_t)ledBrightness) / 255U);
}

void clampLedBrightness() {
  if (ledBrightness < ANCHOR_BRIGHTNESS_MIN) ledBrightness = ANCHOR_BRIGHTNESS_MIN;
  if (ledBrightness > ANCHOR_BRIGHTNESS_MAX) ledBrightness = ANCHOR_BRIGHTNESS_MAX;
}

void triggerMaxBrightnessFlash(const char* reason) {
  lastMaxBrightnessFlashMs = millis();
  lastLedMs = 0; // apply immediately on next LED tick
  Serial.printf("[ANCHOR/LED] max brightness reached -> white flash %lums reason=%s next_taps=dim_down\n",
                (unsigned long)ANCHOR_LED_MAX_FLASH_MS, reason ? reason : "?");
}

void setLedBrightness(uint8_t value, const char* reason) {
  uint8_t old = ledBrightness;
  ledBrightness = value;
  clampLedBrightness();
  if (ledBrightness == old) return;
  brightnessChangedMs = millis();
  brightnessDirty = true;
  // Force next LED frame to apply the new brightness immediately.
  lastLedMs = 0;
  Serial.printf("[ANCHOR/LED] brightness=%u old=%u reason=%s dir=%s hint='tap cycles 0->max->0 / long toggles UART logs / serial +/-'\n",
                (unsigned)ledBrightness, (unsigned)old, reason ? reason : "?",
                buttonBrightnessDirUp ? "up" : "down");
  if (old < ANCHOR_BRIGHTNESS_MAX && ledBrightness >= ANCHOR_BRIGHTNESS_MAX) {
    triggerMaxBrightnessFlash(reason);
  }
}

void stepLedBrightness(int delta, const char* reason) {
  int v = (int)ledBrightness + delta;
  // v1.12: no wrap-around. Tap-cycle can dim all software-controlled LEDs to 0.
  if (v > ANCHOR_BRIGHTNESS_MAX) v = ANCHOR_BRIGHTNESS_MAX;
  if (v < ANCHOR_BRIGHTNESS_MIN) v = ANCHOR_BRIGHTNESS_MIN;
  setLedBrightness((uint8_t)v, reason);
}

void tapCycleLedBrightness(const char* reason) {
  // v1.12: one-button brightness control. Every short tap moves brightness by
  // ANCHOR_BRIGHTNESS_STEP. Direction flips at MAX and MIN, so repeated taps
  // walk 0 -> max -> 0 without using hold-to-dim.
  if (ledBrightness >= ANCHOR_BRIGHTNESS_MAX) buttonBrightnessDirUp = false;
  if (ledBrightness <= ANCHOR_BRIGHTNESS_MIN) buttonBrightnessDirUp = true;

  stepLedBrightness(buttonBrightnessDirUp ? ANCHOR_BRIGHTNESS_STEP : -ANCHOR_BRIGHTNESS_STEP,
                    reason ? reason : "button_tap_cycle");

  if (ledBrightness >= ANCHOR_BRIGHTNESS_MAX) buttonBrightnessDirUp = false;
  if (ledBrightness <= ANCHOR_BRIGHTNESS_MIN) buttonBrightnessDirUp = true;
}

void loadLedBrightness() {
#if ANCHOR_BRIGHTNESS_PERSIST
  anchorPrefsReady = anchorPrefs.begin("janusAnc", false);
  if (anchorPrefsReady) {
    ledBrightness = anchorPrefs.getUChar("led_bri", ANCHOR_LED_BRIGHTNESS);
    janusUart0FullLog = anchorPrefs.getBool("uart_full", (JANUS_UART0_MIRROR_ENABLE != 0));
    anchorSmallLedEnabled = anchorPrefs.getBool("small_led", janusUart0FullLog);
  }
#endif
  anchorSmallLedEnabled = janusUart0FullLog || anchorSmallLedEnabled;
  clampLedBrightness();
  buttonBrightnessDirUp = (ledBrightness < ANCHOR_BRIGHTNESS_MAX);
}

void brightnessSaveTick(uint32_t now) {
#if ANCHOR_BRIGHTNESS_PERSIST
  if (brightnessDirty && anchorPrefsReady && brightnessChangedMs && now - brightnessChangedMs >= ANCHOR_BRIGHTNESS_SAVE_MS) {
    anchorPrefs.putUChar("led_bri", ledBrightness);
    brightnessDirty = false;
    Serial.printf("[ANCHOR/LED] brightness saved=%u\n", (unsigned)ledBrightness);
  }
#else
  (void)now;
#endif
}


void setUart0FullLog(bool enable, const char* reason) {
#if JANUS_UART0_MIRROR_ENABLE || JANUS_UART0_INPUT_ENABLE || JANUS_UART0_STATUS_ENABLE
  bool old = janusUart0FullLog;
  janusUart0FullLog = enable;
  anchorSmallLedEnabled = enable; // same control on both twins: logs + small LED together.
#if ANCHOR_BRIGHTNESS_PERSIST
  if (anchorPrefsReady) {
    anchorPrefs.putBool("uart_full", janusUart0FullLog);
    anchorPrefs.putBool("small_led", anchorSmallLedEnabled);
  }
#endif
  if (!anchorSmallLedEnabled) anchorSmallLedWriteRaw(false);
  else anchorSmallLedWriteRaw(true); // visible acknowledgement
  Serial.printf("[ANCHOR/UART0] fullLog=%u old=%u reason=%s smallLed=%s uartTxWhenOff=0 persisted=1\n",
                janusUart0FullLog ? 1 : 0, old ? 1 : 0, reason ? reason : "?",
                anchorSmallLedEnabled ? "enabled_blink" : "off");
#else
  (void)enable; (void)reason;
#endif
}

void toggleUart0FullLog(const char* reason) {
  setUart0FullLog(!janusUart0FullLog, reason);
}

void setupBrightnessButton() {
#if ANCHOR_BUTTON_ENABLE
  pinMode(ANCHOR_BUTTON_PIN, INPUT_PULLUP);
  bool rawPressed = ANCHOR_BUTTON_ACTIVE_LOW ? (digitalRead(ANCHOR_BUTTON_PIN) == LOW) : (digitalRead(ANCHOR_BUTTON_PIN) == HIGH);
  buttonStablePressed = rawPressed;
  buttonLastRawPressed = rawPressed;
  buttonPressStartMs = rawPressed ? millis() : 0;
  Serial.printf("[ANCHOR/BUTTON] enabled pin=%u mode=tap_cycle_brightness veryLong(~%lums)=toggle_uart_logs min=%u max=%u step=%u savedBrightness=%u dir=%s uartFull=%u\n",
                (unsigned)ANCHOR_BUTTON_PIN, (unsigned long)ANCHOR_BUTTON_LOG_TOGGLE_MS,
                (unsigned)ANCHOR_BRIGHTNESS_MIN, (unsigned)ANCHOR_BRIGHTNESS_MAX,
                (unsigned)ANCHOR_BRIGHTNESS_STEP, (unsigned)ledBrightness,
                buttonBrightnessDirUp ? "up" : "down", janusUart0FullLog ? 1 : 0);
#else
  Serial.println("[ANCHOR/BUTTON] disabled");
#endif
}

void brightnessButtonTick(uint32_t now) {
#if ANCHOR_BUTTON_ENABLE
  bool rawPressed = ANCHOR_BUTTON_ACTIVE_LOW ? (digitalRead(ANCHOR_BUTTON_PIN) == LOW) : (digitalRead(ANCHOR_BUTTON_PIN) == HIGH);
  if (rawPressed != buttonLastRawPressed) {
    buttonLastRawPressed = rawPressed;
    lastButtonSampleMs = now;
  }
  if ((now - lastButtonSampleMs) < ANCHOR_BUTTON_DEBOUNCE_MS) return;
  if (rawPressed != buttonStablePressed) {
    buttonStablePressed = rawPressed;
    if (buttonStablePressed) {
      buttonPressStartMs = now;
      buttonLastRepeatMs = now;
      buttonLongMode = false;
      buttonLogToggleFired = false;
    } else {
      uint32_t held = buttonPressStartMs ? (now - buttonPressStartMs) : 0UL;
      if (buttonLogToggleFired) {
        // Very long press already toggled UART logs. Do not also change brightness.
      } else {
        // v1.12: every normal tap/release cycles brightness up/down.
        // A medium hold below the UART-toggle threshold is treated as a normal tap.
        tapCycleLedBrightness("button_tap_cycle");
      }
      buttonPressStartMs = 0;
      buttonLongMode = false;
      buttonLogToggleFired = false;
    }
  }
  if (buttonStablePressed && buttonPressStartMs && !buttonLogToggleFired && (now - buttonPressStartMs >= ANCHOR_BUTTON_LOG_TOGGLE_MS)) {
    buttonLogToggleFired = true;
    buttonLongMode = true;
    toggleUart0FullLog("button_very_long_toggle");
  }
#else
  (void)now;
#endif
}


void anchorLedTick(uint32_t now) {
#if ANCHOR_LED_ENABLE
  if (now - lastLedMs < 70UL) return;
  lastLedMs = now;

  if (ledBrightness == 0) {
    if (lastLedR || lastLedG || lastLedB) {
      lastLedR = 0; lastLedG = 0; lastLedB = 0;
      anchorLedWrite(0, 0, 0);
    }
    extraLedTick(now);
    return;
  }

  // v1.17 JANUS TWIN FACE:
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
    float age = (float)janusSafeAgeMs(now, refMs, 0UL) / (float)JANUS_FACE_SWAP_MS;
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

  if (flash > 0.0f) {
    r = r * (1.0f - flash) + 245.0f * flash;
    g = g * (1.0f - flash) + 255.0f * flash;
    b = b * (1.0f - flash) + 225.0f * flash;
  }

  // v1.12: max brightness marker. A short white flash tells you the next taps
  // will start walking brightness back down.
  if (lastMaxBrightnessFlashMs && janusSafeAgeMs(now, lastMaxBrightnessFlashMs, 0UL) < ANCHOR_LED_MAX_FLASH_MS) {
    float age = (float)janusSafeAgeMs(now, lastMaxBrightnessFlashMs, 0UL) / (float)ANCHOR_LED_MAX_FLASH_MS;
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
    anchorLedWrite(rr, gg, bb);
  }
  extraLedTick(now);
#else
  (void)now;
#endif
}

void anchorStatusTick(uint32_t now, bool force=false) {
  if (!force && now - lastStatusMs < ANCHOR_STATUS_MS) return;
  lastStatusMs = now;
  uint32_t masterAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  uint32_t jobAge = job.active ? janusSafeAgeMs(now, job.receivedAt, 0UL) : 0UL;
  Serial.printf("[ANCHOR/STATUS] id=%u ch=%u masterAge=%lums job=%u q=%u jobAge=%lums jobs=%lu/%lu/%lu accepted=%lu queued=%lu yielded=%lu repl=%lu H=%lu best=%lu nonce=%08lX shares=%lu rfP=%.2f rfM=%.2f ent=%.2f oxy=%.1f vac=%.2f tx=%lu/%lu heap=%lu led=%u,%u,%u bri=%u shareGlow=%lums maxFlash=%lums\n",
                (unsigned)workerId, (unsigned)peerChannel, (unsigned long)masterAge,
                job.active ? 1 : 0, queuedJobValid ? 1 : 0, (unsigned long)jobAge,
                (unsigned long)jobsSeen, (unsigned long)jobsDone, (unsigned long)jobsExpired,
                (unsigned long)jobsAccepted, (unsigned long)jobsQueued, (unsigned long)jobsYielded, (unsigned long)jobsReplacedNewWork,
                (unsigned long)hashRate, (unsigned long)bestBits, (unsigned long)bestNonce,
                (unsigned long)shares, rfPresence, rfMotion, rfEntropy, anchorOxytocin, anchorTorricelliVacuum,
                (unsigned long)txOk, (unsigned long)txFail, (unsigned long)ESP.getFreeHeap(),
                (unsigned)lastLedR, (unsigned)lastLedG, (unsigned)lastLedB, (unsigned)ledBrightness,
                (unsigned long)((lastShareMs && janusSafeAgeMs(now, lastShareMs, 0UL) < ANCHOR_LED_SHARE_MS) ? (ANCHOR_LED_SHARE_MS - janusSafeAgeMs(now, lastShareMs, 0UL)) : 0UL),
                (unsigned long)((lastMaxBrightnessFlashMs && janusSafeAgeMs(now, lastMaxBrightnessFlashMs, 0UL) < ANCHOR_LED_MAX_FLASH_MS) ? (ANCHOR_LED_MAX_FLASH_MS - janusSafeAgeMs(now, lastMaxBrightnessFlashMs, 0UL)) : 0UL));
}

bool looksLikeBuzz(const JanusColonyPacket& pkt) {
  return strstr(pkt.nodeId, "Buzz") || strstr(pkt.nodeId, "BUZZ") || strstr(pkt.role, "MASTER") || strstr(pkt.role, "Buzz");
}

bool agentTargetsThisNode(const JanusAgentRewardPacket& ar) {
  if (ar.magic[0] != 'A' || ar.magic[1] != 'R') return false;
  if (ar.targetNode[0] == '\0') return true;
  if (!strcmp(ar.targetNode, "*")) return true;
  if (!strcasecmp(ar.targetNode, "all")) return true;
  if (!strcasecmp(ar.targetNode, JANUS_NODE_ID)) return true;
  if (strstr(ar.targetNode, "Anchor") || strstr(ar.targetNode, "RF")) return true;
  return false;
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
#else
void onRecv(const uint8_t *mac, const uint8_t *data, int len)
#endif
{
  if (!data || len < 2) return;
  const uint8_t* srcMac = nullptr;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (info) srcMac = info->src_addr;
  if (info && info->rx_ctrl) lastRssi = info->rx_ctrl->rssi;
#else
  srcMac = mac;
#endif
  rfOnPacketRssi(lastRssi);
  rxSeen++;
  lastAnyRxMs = millis();
  if ((rxSeen % ANCHOR_RX_DEBUG_EVERY) == 1UL) {
    Serial.printf("[ANCHOR/RX] n=%lu len=%d magic=%02X%02X rssi=%d masterAge=%lums job=%u\n",
                  (unsigned long)rxSeen, len, data[0], data[1], (int)lastRssi,
                  (unsigned long)(lastMasterMs ? janusSafeAgeMs(millis(), lastMasterMs, 999999UL) : 999999UL), job.active ? 1 : 0);
  }

  if (janusFaceReceive(data, len, lastRssi)) {
    // Janus twin face sync packet consumed.
    return;
  }
  if (janusTwinTaskReceive(data, len, lastRssi)) {
    // Janus twin task/race packet consumed.
    return;
  }

  if (len == sizeof(RfDomePingPacket) && data[0] == 'R' && data[1] == 'P') {
    RfDomePingPacket rp{};
    memcpy(&rp, data, sizeof(rp));
    if (rp.source[0] == '\0' || strstr(rp.source, "Core2") || strstr(rp.source, "CORE2")) {
      rfDomeOnCorePing(rp, lastRssi);
    }
    return;
  }

  if (len == sizeof(JanusColonyPacket)) {
    JanusColonyPacket pkt{};
    memcpy(&pkt, data, sizeof(pkt));
    if (memcmp(pkt.magic, "JANUS", 5) == 0) {
      rxJanus++;
      if (looksLikeBuzz(pkt)) {
        lastMasterMs = millis();
        rememberBuzzMasterMac(srcMac, "buzz-heartbeat");
        if ((rxJanus % 8UL) == 1UL) {
          Serial.printf("[ANCHOR/BUZZ] rxJanus=%lu node=%s role=%s H=%lu best=%lu shares=%lu rssi=%d\n",
                        (unsigned long)rxJanus, pkt.nodeId, pkt.role, (unsigned long)pkt.hashRate,
                        (unsigned long)pkt.bestBits, (unsigned long)pkt.shares, (int)lastRssi);
        }
      }
    }
    return;
  }

  if (len == sizeof(JobPacket) && data[0] == 'J' && data[1] == 'B') {
    JobPacket jp{};
    memcpy(&jp, data, sizeof(jp));
    rememberBuzzMasterMac(srcMac, "buzz-job");
    janusJobHandlePacket(jp);
    return;
  }

  if (len == sizeof(JanusAgentRewardPacket) && data[0] == 'A' && data[1] == 'R') {
    JanusAgentRewardPacket ar{};
    memcpy(&ar, data, sizeof(ar));
    if (!agentTargetsThisNode(ar)) return;
    rxAgent++;
    agentRewards++;
    agentLevel = ar.rewardLevel;
    agentHint = ar.aiHint ? ar.aiHint : 1;
    agentBatch = ar.targetBatch ? ar.targetBatch : REMOTE_BATCH_BASE;
    agentBatch = constrain((int)agentBatch, REMOTE_BATCH_MIN, REMOTE_BATCH_MAX);
    agentEntropySeed ^= ar.entropySeed ^ micros() ^ ((uint32_t)ar.rewardLevel << 24);
    agentScore = ar.score;
    agentPredH = ar.predictedHashRate;
    agentErr = ar.predictionError;
    Serial.printf("[ANCHOR/AGENT] rx=%lu lvl=%u hint=%u batch=%u score=%.1f predH=%.1f err=%.3f dShare=%lu\n",
                  (unsigned long)agentRewards, (unsigned)agentLevel, (unsigned)agentHint,
                  (unsigned)agentBatch, agentScore, agentPredH, agentErr, (unsigned long)ar.deltaShares);
    return;
  }
}

uint8_t currentChannel() {
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) == ESP_OK && primary) return primary;
  return JANUS_FORCE_CHANNEL;
}

bool janusMacUsable(const uint8_t* mac) {
  if (!mac) return false;
  bool allZero = true;
  bool allFF = true;
  for (int i = 0; i < 6; ++i) {
    allZero = allZero && (mac[i] == 0x00);
    allFF = allFF && (mac[i] == 0xFF);
  }
  return !allZero && !allFF;
}

void janusFormatMac(const uint8_t* mac, char* out, size_t outLen) {
  if (!out || outLen == 0) return;
  if (!janusMacUsable(mac)) {
    strlcpy(out, "--:--:--:--:--:--", outLen);
    return;
  }
  snprintf(out, outLen, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void rememberBuzzMasterMac(const uint8_t* mac, const char* reason) {
  uint32_t now = millis();
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
    char buf[24];
    janusFormatMac(buzzMasterMac, buf, sizeof(buf));
    Serial.printf("[ANCHOR/MASTER] mac=%s reason=%s changed=%u ch=%u direct=%lu/%lu missing=%lu\n",
                  buf, reason ? reason : "?", changed ? 1 : 0, (unsigned)currentChannel(),
                  (unsigned long)buzzMasterDirectOk, (unsigned long)buzzMasterDirectFail,
                  (unsigned long)buzzMasterMacMissing);
  }
}

bool ensureBuzzMasterPeer(const char* reason) {
  if (!buzzMasterMacKnown || !janusMacUsable(buzzMasterMac)) return false;
  uint8_t ch = currentChannel();
  if (esp_now_is_peer_exist(buzzMasterMac) && buzzMasterPeerChannel == ch) return true;
  if (esp_now_is_peer_exist(buzzMasterMac)) esp_now_del_peer(buzzMasterMac);

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, buzzMasterMac, 6);
  peer.channel = ch;
  peer.ifidx = WIFI_IF_STA;
  peer.encrypt = false;
  esp_err_t err = esp_now_add_peer(&peer);
  if (err == ESP_OK || err == ESP_ERR_ESPNOW_EXIST) {
    buzzMasterPeerChannel = ch;
    if (err == ESP_ERR_ESPNOW_EXIST) buzzMasterPeerChannel = ch;
    char buf[24];
    janusFormatMac(buzzMasterMac, buf, sizeof(buf));
    Serial.printf("[ANCHOR/MASTER] peer ready mac=%s ch=%u reason=%s\n", buf, (unsigned)ch, reason ? reason : "?");
    return true;
  }

  buzzMasterPeerChannel = 0;
  char buf[24];
  janusFormatMac(buzzMasterMac, buf, sizeof(buf));
  Serial.printf("[ANCHOR/MASTER] peer add fail mac=%s err=%d ch=%u reason=%s\n", buf, (int)err, (unsigned)ch, reason ? reason : "?");
  return false;
}

esp_err_t sendEspNowToBuzzMaster(const char* tag, const void* payload, size_t len) {
  if (!payload || !len || !buzzMasterMacKnown || !janusMacUsable(buzzMasterMac)) return ESP_ERR_INVALID_STATE;
  if (!ensureBuzzMasterPeer(tag ? tag : "direct")) {
    buzzMasterDirectFail++;
    return ESP_ERR_ESPNOW_NOT_INIT;
  }
  esp_err_t err = esp_now_send(buzzMasterMac, (const uint8_t*)payload, len);
  if (err == ESP_OK) {
    buzzMasterDirectOk++;
  } else {
    buzzMasterDirectFail++;
    buzzMasterPeerChannel = 0;
    Serial.printf("[ANCHOR/MASTER/TXFAIL] tag=%s err=%d direct=%lu/%lu ch=%u peerCh=%u\n",
                  tag ? tag : "?", (int)err,
                  (unsigned long)buzzMasterDirectOk, (unsigned long)buzzMasterDirectFail,
                  (unsigned)currentChannel(), (unsigned)buzzMasterPeerChannel);
  }
  return err;
}

void ensurePeer() {
  uint8_t ch = currentChannel();
  bool broadcastReady = esp_now_is_peer_exist(JANUS_BROADCAST_MAC) && peerChannel == ch;
  if (!broadcastReady) {
    if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) esp_now_del_peer(JANUS_BROADCAST_MAC);
    esp_now_peer_info_t peer{};
    memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6);
    peer.channel = ch;
    peer.ifidx = WIFI_IF_STA;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) == ESP_OK) {
      peerChannel = ch;
      Serial.printf("[ANCHOR] peer ready ch=%u\n", (unsigned)ch);
    }
  }
  if (buzzMasterMacKnown) ensureBuzzMasterPeer("ensure");
}

void setupWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
#if JANUS_USE_WIFI_STA
  if (strlen(JANUS_WIFI_SSID) > 0 && strcmp(JANUS_WIFI_SSID, "YOUR_WIFI") != 0) {
    WiFi.begin(JANUS_WIFI_SSID, JANUS_WIFI_PASS);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 7000UL) {
      delay(100);
    }
    Serial.printf("[ANCHOR/WIFI] sta=%s rssi=%d ch=%u ip=%s\n",
                  WiFi.status() == WL_CONNECTED ? "OK" : "FAIL",
                  WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : -127,
                  (unsigned)currentChannel(),
                  WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "-");
  }
#endif
  if (WiFi.status() != WL_CONNECTED) {
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(JANUS_FORCE_CHANNEL, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);
    Serial.printf("[ANCHOR/WIFI] offline ESP-NOW channel=%u\n", (unsigned)JANUS_FORCE_CHANNEL);
  }
}


#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onSent(const wifi_tx_info_t *info, esp_now_send_status_t status)
#else
void onSent(const uint8_t *mac_addr, esp_now_send_status_t status)
#endif
{
  if (status == ESP_NOW_SEND_SUCCESS) sentCbOk++;
  else sentCbFail++;
  if (status != ESP_NOW_SEND_SUCCESS || ((sentCbOk + sentCbFail) % 20UL) == 1UL) {
    Serial.printf("[ANCHOR/SENT] cb=%s ok=%lu fail=%lu txLocal=%lu/%lu ch=%u\n",
                  status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL",
                  (unsigned long)sentCbOk, (unsigned long)sentCbFail,
                  (unsigned long)txOk, (unsigned long)txFail, (unsigned)peerChannel);
  }
}

// Arduino IDE sometimes fails to auto-generate a prototype for functions
// declared later when default parameters are involved. Keep this explicit.
void minerDebugTick(uint32_t now, bool force);
void uart0StatusTick(uint32_t now, bool force);

void serialCommandTick() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\r' || c == '\n') continue;
    if (c == '?' || c == 'h' || c == 'H') {
      Serial.println("[ANCHOR/CMD] keys: s=status r=rf m=miner +=brighter -=dimmer 0=all_leds_off 9=max l=led u=toggle_uart_full_logs+smallLED b=heartbeat e=entropy p=sense j=job x=reboot ?=help; BOOT tap cycles brightness 0->max->0, very long toggles logs+smallLED");
    } else if (c == 's' || c == 'S') {
      anchorStatusTick(millis(), true);
    } else if (c == 'r' || c == 'R') {
      rfDebug(millis(), true);
    } else if (c == 'm' || c == 'M') {
      minerDebugTick(millis(), true);
    } else if (c == '+') {
      buttonBrightnessDirUp = true;
      stepLedBrightness(ANCHOR_BRIGHTNESS_STEP, "serial_plus");
    } else if (c == '-') {
      buttonBrightnessDirUp = false;
      stepLedBrightness(-ANCHOR_BRIGHTNESS_STEP, "serial_minus");
    } else if (c == '0') {
      buttonBrightnessDirUp = true;
      setLedBrightness(0, "serial_all_leds_off");
    } else if (c == '9') {
      buttonBrightnessDirUp = false;
      setLedBrightness(ANCHOR_BRIGHTNESS_MAX, "serial_max_brightness");
    } else if (c == 'u' || c == 'U') {
      toggleUart0FullLog("serial_u_toggle");
    } else if (c == 'l' || c == 'L') {
      Serial.printf("[ANCHOR/LED] brightness=%u rgb=%u,%u,%u shareGlow=%lums maxFlash=%lums buttonPin=%u extraBluePin=%d extraYellowPin=%d uartFull=%u smallLed=%u uartMirrorDefault=%u uartInput=%u uartStatus=%u tapDir=%s\n",
                    (unsigned)ledBrightness, (unsigned)lastLedR, (unsigned)lastLedG, (unsigned)lastLedB,
                    (unsigned long)((lastShareMs && janusSafeAgeMs(millis(), lastShareMs, 0UL) < ANCHOR_LED_SHARE_MS) ? (ANCHOR_LED_SHARE_MS - janusSafeAgeMs(millis(), lastShareMs, 0UL)) : 0UL),
                    (unsigned long)((lastMaxBrightnessFlashMs && janusSafeAgeMs(millis(), lastMaxBrightnessFlashMs, 0UL) < ANCHOR_LED_MAX_FLASH_MS) ? (ANCHOR_LED_MAX_FLASH_MS - janusSafeAgeMs(millis(), lastMaxBrightnessFlashMs, 0UL)) : 0UL),
                    (unsigned)ANCHOR_BUTTON_PIN, (int)ANCHOR_EXTRA_BLUE_PIN, (int)ANCHOR_EXTRA_YELLOW_PIN,
                    janusUart0FullLog ? 1 : 0, anchorSmallLedEnabled ? 1 : 0, (unsigned)JANUS_UART0_MIRROR_ENABLE,
                    (unsigned)JANUS_UART0_INPUT_ENABLE, (unsigned)JANUS_UART0_STATUS_ENABLE,
                    buttonBrightnessDirUp ? "up" : "down");
    } else if (c == 'b' || c == 'B') {
      sendHeartbeat();
    } else if (c == 'e' || c == 'E') {
      sendEntropy();
    } else if (c == 'p' || c == 'P') {
      sendSwarmSense(true);
    } else if (c == 'j' || c == 'J') {
      Serial.printf("[ANCHOR/JOBSTATE] active=%u seen=%lu accepted=%lu done=%lu exp=%lu q=%u queued=%lu yielded=%lu repl=%lu dup=%lu age=%lums start=%08lX range=%lu doneHashes=%lu lane=%s/s%u stride=%lu arm=%u targetBits=%u qStart=%08lX qAge=%lums\n",
                    job.active ? 1 : 0, (unsigned long)jobsSeen, (unsigned long)jobsAccepted, (unsigned long)jobsDone, (unsigned long)jobsExpired,
                    queuedJobValid ? 1 : 0, (unsigned long)jobsQueued, (unsigned long)jobsYielded, (unsigned long)jobsReplacedNewWork, (unsigned long)jobsDroppedDuplicate,
                    (unsigned long)(job.active ? janusSafeAgeMs(millis(), job.receivedAt, 0UL) : 0UL), (unsigned long)job.startNonce,
                    (unsigned long)job.rangeSize, (unsigned long)job.hashesDone, laneName(job.minerLane),
                    (unsigned)job.minerSector, (unsigned long)job.minerStride, (unsigned)job.minerStrideArm,
                    (unsigned)targetBits, (unsigned long)(queuedJobValid ? queuedJob.startNonce : 0UL),
                    (unsigned long)(queuedJobValid ? janusSafeAgeMs(millis(), queuedJobAtMs, 0UL) : 0UL));
    } else if (c == 'x' || c == 'X') {
      Serial.println("[ANCHOR/CMD] rebooting");
      delay(100);
      ESP.restart();
    } else {
      Serial.printf("[ANCHOR/CMD] got='%c' ; press ? for help\n", c);
    }
  }
}

void anchorWaitTick(uint32_t now) {
  if (now - lastWaitLogMs < ANCHOR_WAIT_LOG_MS) return;
  lastWaitLogMs = now;
  uint32_t masterAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  Serial.printf("[ANCHOR/WAIT] alive uptime=%lums ch=%u rx=%lu janus=%lu jobs=%lu agent=%lu masterAge=%lums job=%u q=%u H=%lu best=%lu rfReady=%u rfP=%.2f rfM=%.2f tx=%lu/%lu heap=%lu\n",
                (unsigned long)now, (unsigned)peerChannel, (unsigned long)rxSeen,
                (unsigned long)rxJanus, (unsigned long)rxJobs, (unsigned long)rxAgent,
                (unsigned long)masterAge, job.active ? 1 : 0, queuedJobValid ? 1 : 0, (unsigned long)hashRate,
                (unsigned long)bestBits, rfReady ? 1 : 0, rfPresence, rfMotion,
                (unsigned long)txOk, (unsigned long)txFail, (unsigned long)ESP.getFreeHeap());
}


void minerDebugTick(uint32_t now, bool force=false) {
  if (!force && now - lastMinerLogMs < ANCHOR_MINER_DEBUG_MS) return;
  lastMinerLogMs = now;
  uint32_t jobAge = job.active ? janusSafeAgeMs(now, job.receivedAt, 0UL) : 0UL;
  uint32_t left = 0;
  if (job.active && job.rangeSize > job.hashesDone) left = job.rangeSize - job.hashesDone;
  uint16_t batch = activeBatch();
  Serial.printf("[ANCHOR/MINER] speed=%luH/s ema=%.0fH/s active=%u q=%u batch=%u checked=%lu/%lu left=%lu lane=%s/s%u stride=%lu arm=%u best=%lu target=%u nonce=%08lX shares=%lu jobs=%lu/%lu/%lu accepted=%lu queued=%lu yielded=%lu repl=%lu dup=%lu total=%lu tail=%lu\n",
                (unsigned long)hashRate, hashRateEma, job.active ? 1 : 0, queuedJobValid ? 1 : 0, (unsigned)batch,
                (unsigned long)job.hashesDone, (unsigned long)job.rangeSize, (unsigned long)left,
                laneName(job.minerLane), (unsigned)job.minerSector,
                (unsigned long)job.minerStride, (unsigned)job.minerStrideArm,
                (unsigned long)bestBits, (unsigned)targetBits, (unsigned long)bestNonce,
                (unsigned long)shares, (unsigned long)jobsSeen, (unsigned long)jobsDone,
                (unsigned long)jobsExpired, (unsigned long)jobsAccepted, (unsigned long)jobsQueued,
                (unsigned long)jobsYielded, (unsigned long)jobsReplacedNewWork, (unsigned long)jobsDroppedDuplicate,
                (unsigned long)totalHashesLifetime, (unsigned long)tailHits);
}


void uart0StatusTick(uint32_t now, bool force=false) {
#if JANUS_UART0_STATUS_ENABLE
  // v1.12 hard rule: if full UART0 logging is OFF, UART0 TX must stay silent.
  // This prevents even one short status line from flashing the blue USB-UART LED.
  if (!janusUart0FullLog) return;
  if (!force && now - lastUart0StatusMs < JANUS_UART0_STATUS_MS) return;
  lastUart0StatusMs = now;
  uint32_t masterAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  JanusDebugUART.printf("[ANCHOR/UART0] v1.12 fullLogStatus ch=%u job=%u H=%lu best=%lu target=%u shares=%lu rfP=%.2f rfM=%.2f bright=%u led=%u,%u,%u tx=%lu/%lu masterAge=%lums heap=%lu cmd=?/s/r/m/+/-/0/9/l/u\\n",
                        (unsigned)peerChannel, job.active ? 1 : 0,
                        (unsigned long)hashRate, (unsigned long)bestBits,
                        (unsigned)targetBits, (unsigned long)shares,
                        rfPresence, rfMotion, (unsigned)ledBrightness,
                        (unsigned)lastLedR, (unsigned)lastLedG, (unsigned)lastLedB,
                        (unsigned long)txOk, (unsigned long)txFail,
                        (unsigned long)masterAge, (unsigned long)ESP.getFreeHeap());
#else
  (void)now; (void)force;
#endif
}

void setupEspNow() {
  esp_now_deinit();
  delay(10);
  if (esp_now_init() != ESP_OK) {
    Serial.println("[ANCHOR] ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onRecv);
  esp_now_register_send_cb(onSent);
  peerChannel = 0;
  ensurePeer();
  Serial.printf("[ANCHOR] ESP-NOW ready id=%u channel=%u\n", workerId, peerChannel);
}

void anchorRadioRescue(const char* reason) {
  uint32_t now = millis();
  if (now - anchorRadioLastRescueMs < ANCHOR_RADIO_RESCUE_MIN_MS) return;
  anchorRadioLastRescueMs = now;
  anchorRadioRescues++;
  uint8_t beforeCh = peerChannel;
  uint32_t rxAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  Serial.printf("[ANCHOR/RADIO/RESCUE] reason=%s n=%lu ch=%u cur=%u rxAge=%lums tx=%lu/%lu direct=%lu/%lu known=%u cb=%lu/%lu err=%d\n",
                reason ? reason : "?", (unsigned long)anchorRadioRescues,
                (unsigned)beforeCh, (unsigned)currentChannel(), (unsigned long)rxAge,
                (unsigned long)txOk, (unsigned long)txFail,
                (unsigned long)buzzMasterDirectOk, (unsigned long)buzzMasterDirectFail,
                buzzMasterMacKnown ? 1 : 0,
                (unsigned long)sentCbOk, (unsigned long)sentCbFail, lastTxErr);
  setupEspNow();
  anchorPresenceBurst(reason ? reason : "radio-rescue");
  anchorRadioLastTxFailSeen = txFail;
  anchorRadioLastTxOkSeen = txOk;
}

void anchorRadioWatchdog(uint32_t now) {
  if (now - anchorRadioLastWatchMs < 2500UL) return;
  anchorRadioLastWatchMs = now;

  uint8_t ch = currentChannel();
  bool peerMissing = !esp_now_is_peer_exist(JANUS_BROADCAST_MAC);
  bool channelMismatch = (peerChannel != 0 && peerChannel != ch);
  bool txFailStreak = (txFail >= anchorRadioLastTxFailSeen + ANCHOR_RADIO_TX_FAIL_DELTA &&
                       txOk == anchorRadioLastTxOkSeen);
  uint32_t anyRxAge = lastAnyRxMs ? janusSafeAgeMs(now, lastAnyRxMs, 999999UL) : 999999UL;
  // Broadcast ESP-NOW tx can stay "OK" while no peers answer, so RX silence alone
  // must be enough to rebuild the radio path after an overnight blackout.
  bool rxBlackout = (now > ANCHOR_RADIO_RX_BLACKOUT_MS &&
                     anyRxAge > ANCHOR_RADIO_RX_BLACKOUT_MS);
  uint32_t masterAge = lastMasterMs ? janusSafeAgeMs(now, lastMasterMs, 999999UL) : 999999UL;
  bool masterBlackout = (now > ANCHOR_RADIO_MASTER_BLACKOUT_MS &&
                         masterAge > ANCHOR_RADIO_MASTER_BLACKOUT_MS);
  bool directBlackout = (buzzMasterMacKnown &&
                         buzzMasterDirectFail >= buzzMasterDirectOk + 8UL &&
                         masterAge > 4500UL);

  if (peerMissing) anchorRadioRescue("peer-missing");
  else if (channelMismatch) anchorRadioRescue("channel-mismatch");
  else if (txFailStreak) anchorRadioRescue("tx-fail-streak");
  else if (directBlackout) anchorRadioRescue("buzz-direct-blackout");
  else if (masterBlackout) anchorRadioRescue("master-blackout");
  else if (rxBlackout) anchorRadioRescue("rx-blackout");

  if (masterBlackout) anchorPresenceBurst("master-blackout");

  if ((txOk != anchorRadioLastTxOkSeen) || (txFail != anchorRadioLastTxFailSeen)) {
    anchorRadioLastTxOkSeen = txOk;
    anchorRadioLastTxFailSeen = txFail;
  }
}

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  uint32_t serialStart = millis();
  while (!Serial && (millis() - serialStart < ANCHOR_SERIAL_WAIT_MS)) {
    delay(10);
  }
  delay(800);
  Serial.println();
  Serial.println("================ JANUS ANCHOR BOOT ================");
  Serial.println("JANUS RF_ANCHOR_AUX v1.19B SAFE_AGE + RF_DOME_LOG_THROTTLE / Buzz lottery worker / ESP-NOW brother race with Gladius / anti-overwrite job queue / RF sleeve human sonar");
  Serial.println("[ANCHOR/SERIAL] native USB CDC full logs; UART0 TX is fully silent while fullLog=0. Hold BOOT ~3s or send U to toggle full UART0 logs/blue activity");
  Serial.println("[ANCHOR/TL] tranception_lite ready: scheduler-only fitness, SHA/header/target/S2 frozen");
  workerId = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
  agentEntropySeed ^= (uint32_t)ESP.getEfuseMac() ^ micros();
  loadLedBrightness();
  setupBrightnessButton();
  setupExtraLeds();
  Serial.printf("[ANCHOR/BOOT] id=%u mac=%llX ledPin=%u led=%u brightness=%u defaultBrightness=%u channel=%u uart0TX=%u uart0RX=%u uartFull=%u smallLed=%u uartMirrorDefault=%u uartInput=%u uartStatus=%u tapDir=%s\n",
                (unsigned)workerId, (unsigned long long)ESP.getEfuseMac(),
                (unsigned)ANCHOR_LED_PIN, (unsigned)ANCHOR_LED_ENABLE,
                (unsigned)ledBrightness, (unsigned)ANCHOR_LED_BRIGHTNESS,
                (unsigned)JANUS_FORCE_CHANNEL, (unsigned)JANUS_UART0_TX_PIN, (unsigned)JANUS_UART0_RX_PIN,
                janusUart0FullLog ? 1 : 0, anchorSmallLedEnabled ? 1 : 0, (unsigned)JANUS_UART0_MIRROR_ENABLE,
                (unsigned)JANUS_UART0_INPUT_ENABLE, (unsigned)JANUS_UART0_STATUS_ENABLE,
                buttonBrightnessDirUp ? "up" : "down");
#if ANCHOR_LED_ENABLE
  Serial.printf("[ANCHOR/LED] enabled normal=armymen_green twin=gladius_turquoise shareGlow=pearl_green_turquoise_swap twinRace=J/T maxFlash=white/%lums pin=%u brightness=%u buttonPin=%u minBrightness=0 tapCyclesBrightness=1 veryLongButtonTogglesUartLogsPlusSmallLed=%lums\n", (unsigned long)ANCHOR_LED_MAX_FLASH_MS, (unsigned)ANCHOR_LED_PIN, (unsigned)ledBrightness, (unsigned)ANCHOR_BUTTON_PIN, (unsigned long)ANCHOR_BUTTON_LOG_TOGGLE_MS);
#else
  Serial.println("[ANCHOR/LED] disabled");
#endif
  if (ANCHOR_LED_ENABLE && ANCHOR_LED_PIN > 48) {
    Serial.printf("[ANCHOR/LED] WARN pin=%u looks invalid for ESP32-S3; set ANCHOR_LED_PIN to 48 or 21 if LED stays dark\n", (unsigned)ANCHOR_LED_PIN);
  }
  anchorLedWrite(0, scaleLed(96), scaleLed(35));
  extraLedTick(millis());
  setupWiFi();
  setupEspNow();
  anchorPresenceBurst("boot");
  sendRfDome(true);
  rfDebug(millis(), true);
  anchorStatusTick(millis(), true);
  minerDebugTick(millis(), true);
  uart0StatusTick(millis(), true);
}

void loop() {
  uint32_t now = millis();
  if (anchorLoopLastMs) {
    int32_t driftMs = (int32_t)(now - anchorLoopLastMs);
    if (driftMs > 1) driftMs -= 1;
    if (driftMs < 0) driftMs = -driftMs;
    anchorLoopJitterUs = (uint16_t)min(65535UL, (uint32_t)driftMs * 1000UL);
  }
  anchorLoopLastMs = now;

  serialCommandTick();
  anchorWaitTick(now);
  ensurePeer();
  anchorRadioWatchdog(now);
  if (now - anchorLastPresenceRefreshMs >= ANCHOR_PRESENCE_REFRESH_MS) {
    anchorLastPresenceRefreshMs = now;
    anchorPresenceBurst("ttl-refresh");
  }
  rfTick(now);
  brightnessButtonTick(now);
  brightnessSaveTick(now);
  janusFaceTick(now);
  janusTwinTaskTick(now);
  anchorTorricelliBondTick(now);
  anchorTranceptionLiteTick(now);
  anchorLedTick(now);
  extraLedTick(now);
  runMining();
  minerDebugTick(now, false);
  uart0StatusTick(now, false);

  janusJobHousekeeping(now);

  if (now - lastHeartbeatMs >= COLONY_HEARTBEAT_MS) {
    lastHeartbeatMs = now;
    sendHeartbeat();
  }
  if (now - lastEntropyMs >= COLONY_ENTROPY_MS) {
    lastEntropyMs = now;
    sendEntropy();
  }
  sendSwarmSense(false);
  sendAnchorPnCortex(false);
  sendRfDome(false);
  rfDebug(now, false);
  anchorStatusTick(now, false);

  if (WiFi.status() != WL_CONNECTED) {
    // Keep fixed ESP-NOW channel in offline mode.
    static uint32_t lastChSet = 0;
    if (now - lastChSet > 10000UL) {
      lastChSet = now;
      esp_wifi_set_promiscuous(true);
      esp_wifi_set_channel(JANUS_FORCE_CHANNEL, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous(false);
    }
  }
  delay(1);
}
