#ifndef CORE_DEBUG_LEVEL
#define CORE_DEBUG_LEVEL 0
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>
#include <math.h>
#include <ctype.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

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
    // Long status lines carry queue/radio/miner diagnostics; 512 bytes truncated
    // them silently. 768 stays modest on the ESP32-S3 loop stack.
    char buf[768];
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

// =================== JANUS RF_ANCHOR_AUX v1.20 S2 SAFE_QUEUE ===================
// Polished twin firmware aligned with the supplied Gladius sketch: Buzz S/2
// shares, deferred ESP-NOW parsing, bounded wire strings, discovery-ping handling,
// accurate H/s telemetry, Wi-Fi reconnect and quieter callback-safe logging.
// Companion node for Blind Eye/Core2. Keeps native USB logs by default, adds runtime UART0 full-log toggle by long BOOT press, all-LED master brightness 0..96, strict aux LED suppression, and BOOT tap-cycle brightness control:
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
#define ANCHOR_STATUS_MS           3000UL
#define ANCHOR_TX_LOG_EVERY        8UL
#define ANCHOR_RADIO_RESCUE_MIN_MS 9000UL
#define ANCHOR_RADIO_RX_BLACKOUT_MS 85000UL
#define ANCHOR_RADIO_MASTER_BLACKOUT_MS 15000UL
#define ANCHOR_PRESENCE_BURST_MIN_MS 7500UL
#define ANCHOR_PRESENCE_REFRESH_MS 14000UL
#define ANCHOR_RADIO_TX_FAIL_DELTA 5UL
#define ANCHOR_SWARM_REJOIN_HARD_RESTART 1
#define ANCHOR_SWARM_REJOIN_BOOT_GRACE_MS 180000UL
#define ANCHOR_SWARM_REJOIN_JOB_BLACKOUT_MS 120000UL
#define ANCHOR_SWARM_REJOIN_STATE_LOG_MS 30000UL
#define ANCHOR_SWARM_REJOIN_HARD_RESTART_MS 900000UL
#define ANCHOR_SWARM_REJOIN_HARD_MIN_RESCUES 4
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
#define ANCHOR_MINER_DEBUG_MS      2500UL
#define ANCHOR_WIFI_RECONNECT_MS    30000UL
#define ANCHOR_RX_QUEUE_DEPTH       16
#define ANCHOR_RX_MAX_LEN           250
#define ANCHOR_RX_PROCESS_BUDGET    12
#define ANCHOR_SENT_LOG_EVERY       64UL
#define ANCHOR_DISCOVERY_REPLY_MS   650UL
#define ANCHOR_MINER_RX_YIELD_HASHES 64U

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

struct __attribute__((packed)) BuzzShareResponseV2 {
  uint8_t magic[2];       // 'S','2' — current Buzz share protocol
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
  uint16_t bits;
  uint32_t total_hashes_l32;
  uint8_t hash_tail[4];   // last 4 bytes of display-order share hash
};

struct AnchorNowRxItem {
  uint8_t mac[6];
  int8_t rssi;
  uint8_t len;
  uint8_t data[ANCHOR_RX_MAX_LEN];
};

static_assert(sizeof(JobPacket) == 134, "Buzz JobPacket wire layout changed");
static_assert(offsetof(JobPacket, header) == 10, "Buzz header offset changed");
static_assert(offsetof(JobPacket, start_nonce) == 90, "Buzz start offset changed");
static_assert(offsetof(JobPacket, target) == 98, "Buzz target offset changed");
static_assert(sizeof(BuzzShareResponseV2) == 26, "Buzz S/2 wire layout changed");
