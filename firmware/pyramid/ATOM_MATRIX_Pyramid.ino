/*
  JANUS_ATOM_MATRIX_ECHO_PYRAMID_BT_COLONY_KNN_v1_8L_GROVE_STICK_SENTINEL.ino

  STRICT Atom Matrix firmware for M5Stack Atom Matrix mounted on Echo/Voice Pyramid. This build intentionally refuses AtomS3/AtomS3R and refuses to compile without A2DP + M5Echo-Pyramid.

  Goals:
    - Keep Echo Pyramid Bluetooth speaker behavior.
    - Touch left: previous / next track.
    - Touch right: volume +/-.
    - Atom Matrix button: short press cycles rich light palettes; long press mute/unmute.
    - Special JANUS_MINE palette: soft turquoise breathing/heartbeat; amber flood on SHAR/share.
    - New show modes: Matrix Rain, Aurora, RF Shadow, Colony Heart, Prism Warp, Data Scanner, Amber Reactor.
    - Bluetooth pairing gate: every unknown phone must be approved by Atom button; Pyramid glows like a campfire while waiting.
    - ESP-NOW colony worker: heartbeat, JobPacket receive, micro-slice mining, ShareResponseV2, EntropyReport, reward receive.
    - HiveMetricPacket: extended RF/audio/mining/UI telemetry for Buzz/NAS-BRAIN/SlimeKNN.
    - v0.7: audio-safe BT connection: no disconnect inside BT callbacks, ESP-NOW deferred while A2DP is active, touch brightness mode, pseudo-EQ palette reactivity.
    - v0.8: audio restored through PCM bridge: A2DP decoded PCM -> queue -> M5EchoPyramid::write(); no second I2S channel, real amplitude-reactive EQ, slower LEDs while audio plays.
    - v0.9: BT-first stability build: ESP-NOW/mining fully disabled by default to keep Classic BT heap stable; re-enable later after audio is proven stable.
    - v1.7: restored working v1.3 audio path; Pyramid output fixed to safe-max 108% gain curve; Pyramid swipes control PHONE AVRCP volume; mute sends phone volume down; Atom Matrix display rotated 180 degrees for USB-port-up orientation; added MUSIC_SMILEYS.
    - v1.8I: Buzz-style always-on colony miner. ESP-NOW/mining stay alive during BT; playback only throttles batch/time-slice, audio hardstop from H2 preserved.
    - v1.8L: Home Cortex Grove/Stick sentinel: lightweight J/E + J/P observer events for the Pyramid Atom Matrix when it sits physically on the Stick Grove link.

  Notes:
    - This is a source replacement, not a patch of the closed M5Burner binary.
    - Audio is priority #1. Mining is deliberately micro-sliced and throttled under A2DP load.
    - Set JANUS_ESPNOW_CHANNEL to the same Wi-Fi channel used by Buzz/AP.

  Required Arduino libraries:
    - M5Unified
    - M5Echo-Pyramid
    - ESP32-A2DP by pschatzmann (BluetoothA2DPSink)

  v0.3E fix:
    - Serial rescue: Serial starts before M5.begin(), with BOOT0/BOOT1/BOOT2 markers.
    - If M5.begin(), BT, or Pyramid init hangs/crashes, Serial shows the last completed stage.
    - Early Atom Matrix LED test before Pyramid init.
    - I2C bus scan remains enabled for Pyramid diagnosis.
    - Adafruit NeoPixel

  Board profile:
    - Board: M5Atom / Atom Matrix / ESP32-PICO-D4 only
    - Partition: default is OK for this sketch; no SPIFFS needed.
*/

#include <Arduino.h>

// STRICT HARDWARE TARGET: Atom Matrix = classic ESP32.
// If this fires, Arduino IDE is not set to M5Atom / ESP32 Pico-D4 class board.
#if !defined(CONFIG_IDF_TARGET_ESP32)
  #error "JANUS Echo Pyramid BT build requires Atom Matrix / classic ESP32. Select Board: M5Atom. AtomS3R/ESP32-S3 has no Bluetooth Classic A2DP."
#endif

#include <M5Unified.h>
// Arduino library discovery may not add user-library include paths when the include
// is hidden behind __has_include. Keep these includes unconditional.
#include <M5EchoPyramid.h>
#define JANUS_HAS_ECHO_PYRAMID_LIB 1
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <esp_system.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <esp_bt_defs.h>
#include <esp_gap_bt_api.h>
#include <esp_a2dp_api.h>
#include <esp_avrc_api.h>
#include <driver/i2s.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// STRICT Bluetooth speaker dependency. Atom Matrix supports Classic BT/A2DP.
// Current ESP32-A2DP uses AudioTools output API on Arduino ESP32 core 3.x.
#include <AudioTools.h>
#include <BluetoothA2DPSink.h>
#define JANUS_HAS_BT_A2DP_LIB 1
#undef JANUS_ENABLE_BT_A2DP
#define JANUS_ENABLE_BT_A2DP 1

#ifndef ESP_AVRC_PT_CMD_VOL_UP
#define ESP_AVRC_PT_CMD_VOL_UP 0x41
#endif
#ifndef ESP_AVRC_PT_CMD_VOL_DOWN
#define ESP_AVRC_PT_CMD_VOL_DOWN 0x42
#endif

#if JANUS_ENABLE_BT_A2DP
// IMPORTANT v0.8: do NOT bind A2DP to AudioTools I2SStream here.
// M5EchoPyramid::begin() already owns I2S0 for codec/mic/speaker.
// We receive decoded PCM via set_stream_reader(..., false) and feed ep.write()
// from a separate task. This avoids "i2s_new_channel: no available channel"
// and prevents BTC_TASK watchdog stalls.
BluetoothA2DPSink a2dp_sink;
#endif

// =====================================================
// USER CONFIG
// =====================================================

#define JANUS_DEVICE_NAME       "ECHO_PYRAMID_BT"
#define JANUS_ROLE_NAME         "bt_worker"
#define JANUS_BT_NAME           "EchoPyramid-JANUS"

// Must match Buzz/AP channel. If Buzz is connected to JANUS_WIFI_PLACEHOLDER Wi-Fi, set this to that router channel.
#define JANUS_ESPNOW_CHANNEL    10

// Atom Matrix + Echo/Voice Pyramid profile.
// IMPORTANT: Pyramid is connected through the ATOM 9-pin header, not the external Grove port.
// Atom Matrix internal/header I2C is G21/G25; the Grove port is G26/G32.
// Previous v0.3B used Grove I2C, so the Pyramid STM32 LED/touch controller was not found.
#define EP_I2C_SDA              25
#define EP_I2C_SCL              21
#define EP_I2S_BCLK             19
#define EP_I2S_WS               33
#define EP_I2S_DOUT             22
#define EP_I2S_DIN              23

// AtomS3R example profile from M5Stack docs: ep.begin(&Wire1, 38, 39, 6, 8, 5, 7, 44100)
// #define EP_I2C_SDA           38
// #define EP_I2C_SCL           39
// #define EP_I2S_BCLK          6
// #define EP_I2S_WS            8
// #define EP_I2S_DOUT          5
// #define EP_I2S_DIN           7

#define EP_SAMPLE_RATE          44100
#define ATOM_MATRIX_LED_PIN     27
#define ATOM_MATRIX_LED_COUNT   25
#define PYRAMID_LED_COUNT       14
#define TOUCH_DEBOUNCE_MS       260
#define BUTTON_LONG_MS          700
#define BUTTON_CLEAR_ACL_BOOT_MS 7000
#define BT_APPROVAL_TIMEOUT_MS 30000
#define BT_TRUSTED_MAX_PEERS 8

// Audio-safe mining limits. v1.8I: Buzz-style always-on mining, but very tiny audio slices.
#define JANUS_BATCH_BT_PLAYING       10
#define JANUS_BATCH_BT_CONNECTED     72
#define JANUS_BATCH_BT_OFF           420
#define JANUS_BATCH_MUTED_WORKER     700
#define JANUS_BATCH_THERMAL_SAFE     16
#define JANUS_BATCH_ABSOLUTE_MAX     900
// v1.8K local distributed-load learner. Buzz may still send targetBatch rewards,
// but the Pyramid gates them through its own thermal/heap/jitter efficiency model.
#define JANUS_BATCH_DYNAMIC_MIN       32
#define JANUS_BATCH_DYNAMIC_START     96
#define JANUS_BATCH_DYNAMIC_SAFE_MAX  240
#define JANUS_BATCH_DYNAMIC_BOOST_MAX 380
#define JANUS_BATCH_DYNAMIC_TICK_MS   2400UL
#define JANUS_BATCH_SWARM_HOT_HEAP    11500UL
#define JANUS_BATCH_SWARM_WARM_HEAP   14000UL
#define JANUS_BATCH_SWARM_GOOD_HEAP   15000UL
#define JANUS_BATCH_SWARM_HOT_JITTER  13000U
#define JANUS_BATCH_SWARM_WARM_JITTER 10000U
#define JANUS_MINE_SLICE_AUDIO_MS     120   // keep A2DP clean: tiny mining pulse every 120ms while music plays
#define JANUS_MINE_SLICE_BT_IDLE_MS    45   // connected but idle phone: moderate slices
#define JANUS_MINE_BUDGET_AUDIO_US    650   // hard CPU budget per audio slice
#define JANUS_MINE_BUDGET_BT_IDLE_US 1500
#define JANUS_MINE_BUDGET_IDLE_US    4200

#define JANUS_HEARTBEAT_MS           1300
#define JANUS_ENTROPY_MS             2500
#define JANUS_HIVE_METRICS_MS        3000
#define JANUS_SWARMSENSE_MS          5000
#define JANUS_PYRAMID_HOME_CORTEX_ENABLE 1
#define JANUS_PYRAMID_EVENT_MS       8000UL
#define JANUS_PYRAMID_TASK_MS        18000UL
#define JANUS_PYRAMID_GROVE_STICK_LINK 1
#define JANUS_SERIAL_STATUS_MS       5000
#define JANUS_AUDIO_CHUNK_FRAMES      256
#define JANUS_AUDIO_QUEUE_LEN         22
#define JANUS_LED_MS_IDLE             32
#define JANUS_LED_MS_AUDIO            72
#define JANUS_ESPNOW_BOOT_DELAY_MS   15000
#define JANUS_ESPNOW_RESTART_DELAY_MS 5000
// v1.8I1: keep Classic BT discoverable first. ESP-NOW starts after pairing grace,
// or immediately after a phone has connected once. This preserves BT connection while
// still keeping Buzz-style mining alive after the audio device is established.
#define JANUS_BT_FIRST_PAIRING_GRACE_MS 45000UL
#define JANUS_BT_FIRST_AFTER_DISCONNECT_GRACE_MS 12000UL
#define JANUS_BT_PAIRING_HEAP_GUARD 42000UL
#define JANUS_BT_LOW_HEAP_GUARD      28000
#define JANUS_ESPNOW_LOW_HEAP_STOP_GUARD 9000
// v1.8K: Atom Matrix cannot keep Classic BT/A2DP heap + ESP-NOW worker together reliably.
// Give the phone a visible BT window, then fully stop BT and enter exclusive Swarm mode.
// Dynamic batch AI raises/lowers worker load from heap+jitter+RSSI+efficiency prediction.
#define JANUS_EXCLUSIVE_SWARM_MODE 1
#define JANUS_BT_IDLE_TO_SWARM_MS 45000UL
#define JANUS_BT_REBOOT_HINT_FLASH_MS 1600UL
#define BRIGHTNESS_TOUCH_MODE_MS     9000
#define BRIGHTNESS_HOLD_MS            620
#define BRIGHTNESS_REPEAT_MS          650
#define TOUCH_GESTURE_COOLDOWN_MS     820
#define TOUCH_SWIPE_MIN_MS             45
#define TOUCH_SWIPE_MAX_MS           1350
#define JANUS_SPEAKER_CODEC_SAFE_VOLUME 72
#define JANUS_SPEAKER_LOCAL_VOLUME_DEFAULT 100
#define JANUS_SPEAKER_LOCAL_VOLUME_MAX 100
#define JANUS_PHONE_VOLUME_DEFAULT     127
#define JANUS_PCM_GAIN_MAX_X100        108
#define JANUS_VISUAL_SILENCE_FLOOR     18
#define JANUS_JOB_STALE_MS           90000
#define JANUS_BT_TO_FARM_COOLDOWN_MS  18000
#define JANUS_COLONY_IDLE_ARM_MS      0UL       // v1.8I: miner is always armed after boot; playback only throttles
#define JANUS_BT_ADV_WATCHDOG_ENABLE  0
#define JANUS_LED_MS_AUDIO_LOW_HEAP   220
#define JANUS_BT_LOW_HEAP_VISUAL_GUARD 15000
#define JANUS_BT_BRIGHTNESS_CAP        85
#define JANUS_SAFE_SOFT_BRIGHTNESS      1
#define JANUS_MIN_VISIBLE_BRIGHTNESS    6
#define JANUS_PCM_SOFT_GAIN_MAX_X100   JANUS_PCM_GAIN_MAX_X100
#define JANUS_AUDIO_TASK_STACK          5120
#define JANUS_AUDIO_TASK_PRIORITY       7
#define JANUS_KNN_MEMORY                 16
#define JANUS_KNN_UPDATE_MS              1500
#define JANUS_KNN_MIN_LEARN_MS           900

// v1.8I6 BT-safe colony worker. ESP-NOW/mining works while BT is idle; no promiscuous channel switch, heap-safe telemetry.
#define JANUS_ENABLE_COLONY_ESPNOW     1

// Keep the button approval gate. If you need a pure audio smoke-test, set this to 0.
#define JANUS_BT_APPROVAL_REQUIRED     1

// Slower LED refresh under audio reduces I2C pressure while BTC_TASK is decoding.
#undef JANUS_LED_MS_AUDIO
#define JANUS_LED_MS_AUDIO             180

static const uint8_t JANUS_BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// =====================================================
// JANUS COLONY PROTOCOL
// Matches Buzz v10.11 colony structures.
// =====================================================

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
  uint8_t aiHint;     // 0 observe, 1 stable, 2 slow-down, 3 boost
  uint32_t jobAgeMs;
  int8_t rssi;
  uint32_t uptime;
};

struct __attribute__((packed)) JobPacket {
  uint8_t magic[2];       // 'J','B'
  uint8_t job_id[8];
  uint8_t header[80];
  uint32_t start_nonce;
  uint32_t range_size;
  uint8_t target[32];     // big-endian/display-order target
  uint32_t extranonce2;
};

struct __attribute__((packed)) ShareResponse {
  uint8_t magic[2];       // 'S','R' legacy Buzz/Core/TD compatible share
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
};

struct __attribute__((packed)) ShareResponseV2 {
  uint8_t magic[2];       // 'S','2' extended Buzz compatible share
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
  uint16_t bits;
  uint32_t total_hashes_l32;
  uint8_t hash_tail[4];
};

struct __attribute__((packed)) EntropyReport {
  uint8_t magic[2];       // 'E','R'
  uint16_t worker_id;
  float local_entropy;
  uint8_t sensor_flags;   // bit0=mic, bit1=tmos/touch, bit2=mag/rf, bit3=bt/audio, bit4=rf_field
  float values[4];        // rssi, touchActivity, btState, thermal/load
};

// Extended optional telemetry. Buzz/Core2 may ignore this until they learn magic 'H','M'.
// It is intentionally <= 250 bytes for ESP-NOW v1 compatibility.
struct __attribute__((packed)) HiveMetricPacket {
  uint8_t magic[2];        // 'H','M'
  uint8_t version;         // 2
  uint16_t worker_id;
  char nodeId[24];
  char kind[16];
  uint32_t seq;
  uint32_t uptime_ms;
  uint32_t free_heap;
  uint32_t min_free_heap;
  uint16_t cpu_mhz;
  uint16_t loop_jitter_us;
  uint16_t loop_max_us;
  int8_t rssi;
  uint8_t bt_flags;        // bit0 connected, bit1 playing, bit2 muted, bit3 acl_ok
  uint8_t volume;
  uint8_t palette;
  uint16_t touch_count;
  uint16_t effective_batch;
  uint32_t hash_rate;
  uint32_t total_hashes;
  uint32_t shares;
  uint32_t rejects;
  uint16_t best_bits;
  uint32_t job_age_ms;
  uint32_t nonce_remaining;
  uint8_t reward_level;
  uint8_t ai_hint;
  uint16_t target_batch;
  int16_t prediction_error_x1000;
  uint16_t entropy_x1000;
  uint16_t random_tail;
  uint16_t reserved;
};

// SwarmSensePacket v1: observe-only sensory vector for Buzz/NAS-BRAIN.
// Old Buzz builds safely ignore this unknown magic. The next Buzz/NAS step will store it.
// Goal: observe first, predict later, act only after confidence grows.
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
  uint8_t radio_mode;      // 0=BT speaker, 1=exclusive swarm
  uint8_t bt_flags;        // bit0 connected, bit1 playing, bit2 muted, bit3 acl, bit4 approval
  uint8_t palette;
  uint8_t knn_label;
  uint8_t knn_confidence;
  uint8_t ai_hint;
  uint8_t thermal_load;    // 0..100 proxy
  uint16_t effective_batch;
  uint16_t dynamic_batch;
  uint32_t hash_rate;
  uint32_t total_hashes;
  uint16_t best_bits;
  uint16_t hash_eff_x1000; // H/s per batch * 1000
  int16_t prediction_error_x1000;
  uint16_t entropy_x1000;
  uint16_t touch_delta;
  uint16_t job_age_s;
  uint16_t nonce_remaining_l16;
  uint16_t flags;          // future: IMU/audio/touch/env/rf capability bits
};


// Home Cortex lightweight J/E + J/P bridge for Pyramid/Stick-Grove sentinel.
enum JanusPyramidRoleId : uint8_t {
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

enum JanusPyramidEventType : uint8_t {
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

enum JanusPyramidCapability : uint16_t {
  JC_TOUCH   = 0x0400,
  JC_RELAY   = 0x0800,
  JC_MEMORY  = 0x1000,
  JC_AI      = 0x2000,
  JC_RF      = 0x8000,
  JC_AUDIO   = 0x0100,
  JC_HASH    = 0x0080
};

struct __attribute__((packed)) JanusEventPacket {
  uint8_t magic[2];        // 'J','E'
  uint8_t version;         // 1
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

struct __attribute__((packed)) JanusPolicyPacket {
  uint8_t magic[2];        // 'J','P'
  uint8_t version;         // 1
  uint8_t swarmMood;
  uint8_t radioRate;
  uint8_t buzzBudget;
  uint8_t sensorRate;
  uint8_t confidence;
  uint16_t flags;
  uint32_t seq;
  uint32_t ttlMs;
  uint32_t quietUntilMs;   // duration ms from Core
  uint16_t dominantTopic;
  uint16_t danger_x100;
  char order[40];
};

// Buzz v10.11+ Agent reward packet. Keep layout aligned with TD_SWARM/Core/Buzz.
struct __attribute__((packed)) JanusAgentRewardPacket {
  uint8_t magic[2];        // 'A','R'
  uint8_t version;         // 1
  char source[16];         // BuzzAgent
  char targetNode[24];     // ECHO_PYRAMID_BT / all / *
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

// Older short reward layout kept only as parser fallback for our early Pyramid builds.
struct __attribute__((packed)) JanusAgentRewardPacketLegacy {
  uint8_t magic[2];
  uint8_t version;
  char targetNode[24];
  uint8_t rewardLevel;
  uint8_t aiHint;
  uint16_t rewardPoints;
  uint16_t targetBatch;
  float predictionError;
  uint32_t seq;
};

static_assert(sizeof(JobPacket) == 134, "JobPacket size mismatch");
static_assert(sizeof(ShareResponse) == 16, "ShareResponse size mismatch");
static_assert(sizeof(ShareResponseV2) == 26, "ShareResponseV2 size mismatch");

// =====================================================
// GLOBALS
// =====================================================

M5EchoPyramid ep;
bool pyramidReady = false;
TwoWire* pyramidBus = &Wire;
int pyramidActiveSda = -1;
int pyramidActiveScl = -1;
Preferences prefs;
Adafruit_NeoPixel atomMatrix(ATOM_MATRIX_LED_COUNT, ATOM_MATRIX_LED_PIN, NEO_GRB + NEO_KHZ800);

uint16_t workerId = 0;
bool espNowReady = false;
uint32_t espNowLastStopMs = 0;
uint32_t espNowLastStartTryMs = 0;
uint8_t janusEspNowHomeChannel = 0;
uint32_t janusLastChannelFixMs = 0;
uint32_t colonySeq = 0;
uint32_t totalHashes = 0;
uint32_t hashWindowStartMs = 0;
uint32_t hashWindowCount = 0;
uint32_t currentHashRate = 0;
uint32_t janusLastMineSliceMs = 0;
uint32_t shares = 0;
uint32_t rejects = 0;
uint32_t bestBits = 0;
int8_t lastRssi = -127;
uint8_t masterMac[6] = {0};
bool masterKnown = false;

volatile bool nowJobPending = false;
JobPacket pendingJob;
uint8_t pendingJobMac[6] = {0};
int8_t pendingJobRssi = -127;

JobPacket activeJob;
bool jobReady = false;
uint32_t jobReceivedMs = 0;
uint32_t nonceCursor = 0;
uint32_t nonceEnd = 0;

uint8_t rewardAiHint = 0;
uint8_t rewardLevel = 0;
uint16_t rewardTargetBatch = 260;
float lastPredictionError = 0.0f;
uint32_t lastRewardMs = 0;

bool btConnected = false;
bool btPlaying = false;
bool btA2dpStarted = false;
bool btDiscoverableForced = false;
bool janusBtEverConnected = false;
uint32_t janusBtFirstReadyMs = 0;
// v1.8J exclusive radio state. In colony mode Classic BT is stopped, freeing heap/radio for ESP-NOW.
bool janusExclusiveSwarmMode = false;
bool janusBtStoppedForSwarm = false;
uint32_t janusExclusiveSwarmEnteredMs = 0;
uint32_t janusLastBtToSwarmLogMs = 0;
uint32_t lastBtDiscoverableRefreshMs = 0;
uint32_t btLastDisconnectMs = 0;
uint32_t btLastStateChangeMs = 0;
bool janusBtFarmWindow = false;
uint32_t janusLastPlaybackActivityMs = 0;
uint32_t janusLastUserInteractionMs = 0;
bool janusColonyIdleArmed = false;
bool janusColonyAutoMineScreenDone = false;
bool muted = false;
uint8_t janusPhoneVolumeBeforeMute = 96;
bool btAclAccepted = false;
bool btAclPendingName = false;
uint32_t btAclPendingUntil = 0;
esp_bd_addr_t currentBda = {0};
bool currentBdaValid = false;
char currentRemoteName[64] = {0};

bool btApprovalPending = false;
uint32_t btApprovalUntil = 0;
esp_bd_addr_t approvalBda = {0};
bool approvalBdaValid = false;
char approvalRemoteName[64] = {0};

uint8_t volumeLevel = JANUS_SPEAKER_LOCAL_VOLUME_DEFAULT;   // local safe Pyramid output trim; iPhone volume is also respected
uint8_t brightnessLevel = 70;
bool brightnessTouchMode = false;
uint32_t brightnessTouchUntilMs = 0;
uint8_t audioEnergy = 0;
uint8_t audioBass = 0;
uint8_t audioMid = 0;
uint8_t audioTreble = 0;
uint32_t lastEqMs = 0;

struct JanusAudioChunk {
  uint16_t frames;
  int16_t mono[JANUS_AUDIO_CHUNK_FRAMES];
};
QueueHandle_t janusAudioQueue = nullptr;
TaskHandle_t janusAudioTaskHandle = nullptr;
volatile uint32_t janusPcmPackets = 0;
volatile uint32_t janusPcmBytes = 0;
volatile uint32_t janusAudioWrittenChunks = 0;
volatile uint32_t janusAudioDropChunks = 0;
volatile uint8_t janusPcmEnergy = 0;
volatile uint8_t janusPcmBass = 0;
volatile uint8_t janusPcmMid = 0;
volatile uint8_t janusPcmTreble = 0;
volatile uint16_t janusPcmGainX100 = 0;
volatile uint8_t janusPhoneVolume127 = JANUS_PHONE_VOLUME_DEFAULT;
uint32_t lastTouchMs = 0;
uint32_t buttonDownMs = 0;
bool buttonWasDown = false;
bool bootAclClearDone = false;

uint32_t sharAmberUntilMs = 0;
uint32_t rejectFlashUntilMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastEntropyMs = 0;
uint32_t lastHiveMetricsMs = 0;
uint32_t lastSwarmSenseMs = 0;
uint32_t janusPyramidLastEventMs = 0;
uint32_t janusPyramidLastTaskMs = 0;
uint32_t janusPyramidEventSeq = 0;
uint32_t janusPyramidPolicyRx = 0;
uint32_t janusPyramidPolicySeq = 0;
uint32_t janusPyramidLastPolicyMs = 0;
uint32_t janusPyramidQuietUntilMs = 0;
uint8_t janusPyramidMood = 0;
uint8_t janusPyramidRadioRate = 1;
uint8_t janusPyramidSensorRate = 1;
uint8_t janusPyramidPolicyConfidence = 0;
uint16_t janusPyramidDangerX100 = 0;
char janusPyramidOrder[40] = "-";
uint32_t lastSerialStatusMs = 0;
uint32_t lastLedMs = 0;
uint32_t touchActivity = 0;
uint32_t totalTouchEvents = 0;
uint32_t lastLoopUs = 0;
uint16_t loopJitterUsEma = 0;
uint16_t loopMaxUs = 0;
uint32_t touch3StartMs = 0;
uint32_t touch4StartMs = 0;
bool touch3HoldDone = false;
bool touch4HoldDone = false;
uint32_t touch3LastRepeatMs = 0;
uint32_t touch4LastRepeatMs = 0;

struct TouchSwipeState {
  bool active = false;
  uint8_t firstPad = 0;   // 1 = lower pad first, 2 = upper pad first, 3 = ambiguous/simultaneous
  uint32_t startMs = 0;
  bool fired = false;
};
TouchSwipeState trackSwipe;
TouchSwipeState phoneVolumeSwipe;

enum JanusKnnLabel : uint8_t {
  KNN_NEW = 0,
  KNN_GOOD,
  KNN_HOT,
  KNN_SHADOW,
  KNN_SILENT,
  KNN_UNSTABLE,
  KNN_SHARE_RICH,
  KNN_AUDIO_NODE,
  KNN_PYRAMID,
  KNN_COUNT
};

struct JanusKnnSample {
  int8_t rssi;
  uint8_t audio;
  uint8_t bt;
  uint16_t jitter;
  uint16_t heap_kb;
  uint16_t drop_delta;
  uint16_t batch;
  uint8_t label;
};

JanusKnnSample knnMemory[JANUS_KNN_MEMORY];
uint8_t knnMemoryCount = 0;
uint8_t knnMemoryHead = 0;
JanusKnnLabel localKnnLabel = KNN_NEW;
uint8_t localKnnConfidence = 0;
uint8_t localKnnAiHint = 1;
uint16_t localKnnTargetBatch = 260;
uint32_t lastKnnUpdateMs = 0;
uint32_t lastKnnLearnMs = 0;
uint32_t lastKnnDropCount = 0;
uint32_t lastKnnShareCount = 0;

// v1.8K: local batch AI state. This is the Pyramid's tiny self-learning layer:
// it predicts efficient H/s per batch and reports the result through HiveMetricPacket.
uint16_t janusDynamicBatch = JANUS_BATCH_DYNAMIC_START;
uint16_t janusDynamicBatchCeiling = JANUS_BATCH_DYNAMIC_SAFE_MAX;
float janusHashEma = 0.0f;
float janusEfficiencyEma = 0.0f;
float janusPredictedHash = 0.0f;
float janusPredictionErrorLocal = 0.0f;
uint8_t janusThermalLoad = 0;
uint32_t janusLastDynamicBatchMs = 0;
uint32_t janusLastDynamicLogMs = 0;

uint8_t matrixDrops[5] = {0, 2, 4, 1, 3};
uint8_t matrixDropSpeed[5] = {2, 3, 2, 4, 3};

// Tiny ambient Snake AI state for PAL_SNAKE_AI. It learns only in RAM: best score
// slowly biases exploration down, so it becomes calmer after longer survival.
uint8_t snakeBody[25] = {0};
uint8_t snakeLen = 3;
uint8_t snakeFood = 18;
uint8_t snakeDir = 1;
uint8_t snakeScore = 0;
uint8_t snakeBest = 0;
uint16_t snakeSteps = 0;
uint32_t snakeLastStepMs = 0;
bool snakeReady = false;

// =====================================================
// PALETTE
// =====================================================

enum JanusPalette : uint8_t {
  PAL_RAINBOW = 0,
  PAL_OCEAN,
  PAL_FIRE,
  PAL_MATRIX_RAIN,
  PAL_JANUS_MINE,
  PAL_AURORA,
  PAL_RF_SHADOW,
  PAL_COLONY_HEART,
  PAL_PRISM_WARP,
  PAL_DATA_SCANNER,
  PAL_AMBER_REACTOR,
  PAL_INFINITY_TUNNEL,
  PAL_SABER_BLUE,
  PAL_SABER_GREEN,
  PAL_SABER_RED,
  PAL_SABER_AMBER,
  PAL_SABER_VIOLET,
  PAL_SNAKE_AI,
  PAL_SPACE_MOON,
  PAL_RAIN_DROPS,
  PAL_TACHYON_RIFT,
  PAL_BIOLUMEN_REEF,
  PAL_STARFORGE,
  PAL_MUSIC_SMILEYS,
  PAL_TETRIS_STACK,
  PAL_MICRO_RACER,
  PAL_PIXEL_RUNNER,
  PAL_DOOM_RAYCAST,
  PAL_INVADER_SWARM,
  PAL_PONG_TENNIS,
  PAL_COUNT
};
JanusPalette currentPalette = PAL_JANUS_MINE;
float hueOffset = 0.0f;
const char* paletteName(JanusPalette p) {
  switch (p) {
    case PAL_RAINBOW: return "RAINBOW";
    case PAL_OCEAN: return "OCEAN";
    case PAL_FIRE: return "FIRE";
    case PAL_MATRIX_RAIN: return "MATRIX_RAIN";
    case PAL_JANUS_MINE: return "JANUS_MINE";
    case PAL_AURORA: return "AURORA";
    case PAL_RF_SHADOW: return "RF_SHADOW";
    case PAL_COLONY_HEART: return "COLONY_HEART";
    case PAL_PRISM_WARP: return "PRISM_WARP";
    case PAL_DATA_SCANNER: return "DATA_SCANNER";
    case PAL_AMBER_REACTOR: return "AMBER_REACTOR";
    case PAL_INFINITY_TUNNEL: return "INFINITY_TUNNEL";
    case PAL_SABER_BLUE: return "SABER_BLUE";
    case PAL_SABER_GREEN: return "SABER_GREEN";
    case PAL_SABER_RED: return "SABER_RED";
    case PAL_SABER_AMBER: return "SABER_AMBER";
    case PAL_SABER_VIOLET: return "SABER_VIOLET";
    case PAL_SNAKE_AI: return "SNAKE_AI";
    case PAL_SPACE_MOON: return "SPACE_MOON";
    case PAL_RAIN_DROPS: return "RAIN_DROPS";
    case PAL_TACHYON_RIFT: return "TACHYON_RIFT";
    case PAL_BIOLUMEN_REEF: return "BIOLUMEN_REEF";
    case PAL_STARFORGE: return "STARFORGE";
    case PAL_MUSIC_SMILEYS: return "MUSIC_SMILEYS";
    case PAL_TETRIS_STACK: return "TETRIS_STACK";
    case PAL_MICRO_RACER: return "MICRO_RACER";
    case PAL_PIXEL_RUNNER: return "PIXEL_RUNNER";
    case PAL_DOOM_RAYCAST: return "DOOM_RAYCAST";
    case PAL_INVADER_SWARM: return "INVADER_SWARM";
    case PAL_PONG_TENNIS: return "PONG_TENNIS";
    default: return "UNKNOWN";
  }
}

// =====================================================
// HELPERS
// =====================================================

static inline uint8_t clamp8(int v) { return (uint8_t)((v < 0) ? 0 : ((v > 255) ? 255 : v)); }
static inline uint16_t clamp16(int v, int lo, int hi) { return (uint16_t)((v < lo) ? lo : ((v > hi) ? hi : v)); }
static inline uint8_t audioGate(uint8_t idle = 80) {
  if (!btConnected && !btPlaying) return idle;
  if (!btPlaying) return JANUS_VISUAL_SILENCE_FLOOR;
  return clamp8(JANUS_VISUAL_SILENCE_FLOOR + (int)audioEnergy * 2);
}
static inline uint8_t audioScale(uint8_t value, uint8_t idle = 80) {
  uint16_t g = audioGate(idle);
  return clamp8(((uint16_t)value * g) / 160);
}
static inline uint8_t audioAdd(uint8_t base, uint8_t amountDiv = 3) {
  return clamp8((int)base + (btPlaying ? (int)audioEnergy / max<uint8_t>(1, amountDiv) : 0));
}

static inline bool gameBit(uint32_t mask, int x, int y) {
  if (x < 0 || x > 4 || y < 0 || y > 4) return false;
  return (mask >> (y * 5 + x)) & 1UL;
}

static inline void gameSetBit(uint32_t &mask, int x, int y) {
  if (x < 0 || x > 4 || y < 0 || y > 4) return;
  mask |= (1UL << (y * 5 + x));
}

static inline uint8_t gameBeatPulse(uint8_t idle = 28) {
  uint8_t a = btPlaying ? audioEnergy : (uint8_t)min<uint32_t>(90, currentHashRate / 90);
  return clamp8(idle + a / 2 + ((millis() / 180) & 1 ? 10 : 0));
}

String bdaToString(const esp_bd_addr_t bda) {
  char buf[18];
  snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
  return String(buf);
}

bool bdaEquals(const esp_bd_addr_t a, const esp_bd_addr_t b) {
  return memcmp(a, b, ESP_BD_ADDR_LEN) == 0;
}

bool parseBdaString(const String& s, esp_bd_addr_t out) {
  unsigned int b[6];
  if (sscanf(s.c_str(), "%02X:%02X:%02X:%02X:%02X:%02X", &b[0], &b[1], &b[2], &b[3], &b[4], &b[5]) != 6) return false;
  for (int i = 0; i < 6; i++) out[i] = (uint8_t)b[i];
  return true;
}

bool loadBda(const char* key, esp_bd_addr_t out) {
  String s = prefs.getString(key, "");
  if (s.length() < 17) return false;
  return parseBdaString(s, out);
}

void saveBda(const char* key, const esp_bd_addr_t bda) {
  prefs.putString(key, bdaToString(bda));
}

uint8_t btTrustedCount() {
  uint8_t n = prefs.getUChar("bt_count", 0);
  return (n > BT_TRUSTED_MAX_PEERS) ? BT_TRUSTED_MAX_PEERS : n;
}

void btPeerKey(uint8_t idx, char* out, size_t len) {
  snprintf(out, len, "bt_peer_%u", (unsigned)idx);
}

void btNameKey(uint8_t idx, char* out, size_t len) {
  snprintf(out, len, "bt_name_%u", (unsigned)idx);
}

bool loadTrustedBdaAt(uint8_t idx, esp_bd_addr_t out) {
  char key[16];
  btPeerKey(idx, key, sizeof(key));
  return loadBda(key, out);
}

bool bdaAlreadyAllowed(const esp_bd_addr_t bda, char* ownerOut, size_t ownerLen) {
  uint8_t n = btTrustedCount();
  for (uint8_t i = 0; i < n; ++i) {
    esp_bd_addr_t saved;
    if (!loadTrustedBdaAt(i, saved)) continue;
    if (bdaEquals(saved, bda)) {
      if (ownerOut && ownerLen) {
        char nk[16];
        btNameKey(i, nk, sizeof(nk));
        String nm = prefs.getString(nk, "trusted");
        snprintf(ownerOut, ownerLen, "%s", nm.c_str());
      }
      return true;
    }
  }
  return false;
}

void saveTrustedPeer(const esp_bd_addr_t bda, const char* name) {
  char owner[64] = {0};
  if (bdaAlreadyAllowed(bda, owner, sizeof(owner))) return;

  uint8_t n = btTrustedCount();
  uint8_t slot = (n < BT_TRUSTED_MAX_PEERS) ? n : 0;   // full list: rotate oldest slot 0
  char pk[16], nk[16];
  btPeerKey(slot, pk, sizeof(pk));
  btNameKey(slot, nk, sizeof(nk));
  saveBda(pk, bda);
  prefs.putString(nk, (name && name[0]) ? name : "approved");
  if (n < BT_TRUSTED_MAX_PEERS) prefs.putUChar("bt_count", n + 1);
  Serial.printf("BT TRUST LEARN | slot=%u | name=%s | bda=%s\n", (unsigned)slot, (name && name[0]) ? name : "?", bdaToString(bda).c_str());
}

void clearBtAcl() {
  for (uint8_t i = 0; i < BT_TRUSTED_MAX_PEERS; ++i) {
    char pk[16], nk[16];
    btPeerKey(i, pk, sizeof(pk));
    btNameKey(i, nk, sizeof(nk));
    prefs.remove(pk);
    prefs.remove(nk);
  }
  prefs.putUChar("bt_count", 0);
  btApprovalPending = false;
  approvalBdaValid = false;
  Serial.println("BT TRUSTED PEERS CLEARED");
  rejectFlashUntilMs = millis() + 1600;
}


void migrateLegacyTwoOwnerAcl() {
  if (btTrustedCount() > 0) return;
  esp_bd_addr_t old;
  bool migrated = false;
  if (loadBda("hawkar_bda", old)) {
    saveTrustedPeer(old, "Hawkar");
    migrated = true;
  }
  if (loadBda("rose_bda", old)) {
    saveTrustedPeer(old, "Rose Wine");
    migrated = true;
  }
  if (migrated) Serial.printf("BT ACL MIGRATED | trusted=%u\n", btTrustedCount());
}


void pyramidCtrlSetRGB(uint8_t side, uint8_t idx, uint8_t r, uint8_t g, uint8_t b) {
  if (!pyramidReady) return;
#if JANUS_SAFE_SOFT_BRIGHTNESS
  // v1.8C: do NOT spam STM32 setBrightness(). On this Pyramid/Core combo it can
  // eventually trip esp_timer_create()/NO_MEM or core aborts after long touch-holds.
  // Keep hardware brightness stable and scale RGB in software instead.
  uint8_t br = brightnessLevel;
  r = (uint8_t)((uint16_t)r * br / 100U);
  g = (uint8_t)((uint16_t)g * br / 100U);
  b = (uint8_t)((uint16_t)b * br / 100U);
#endif
  ep.ctrl().setRGB(side, idx, r, g, b);
}

void pyramidCtrlSetBrightness(uint8_t side, uint8_t value) {
  if (!pyramidReady) return;
#if JANUS_SAFE_SOFT_BRIGHTNESS
  (void)side;
  (void)value;
  return;
#else
  ep.ctrl().setBrightness(side, value);
#endif
}

bool pyramidCtrlIsPressed(uint8_t touchId) {
  if (!pyramidReady) return false;
  return ep.ctrl().isPressed(touchId);
}

void pyramidCodecMute(bool state) {
  if (!pyramidReady) return;
  ep.codec().mute(state);
}

void pyramidCodecSetVolume(uint8_t value) {
  if (!pyramidReady) return;
  ep.codec().setVolume(value);
}

void updateEffectivePcmGainFromPhone();

// v1.3: louder-but-still-protected Pyramid speaker profile.
// The AW87559 PA starts at 16.5 dB in the library; we raise it moderately and keep AGC3 protection on.
void configurePyramidLoudMode() {
  if (!pyramidReady) return;
  bool ok = true;
  // v1.3: safe PA profile. v1.1 21 dB + 150% preamp was too aggressive.
  // Keep the amplifier protected and let phone volume + local trim map into a safe PCM gain curve.
  ok &= ep.pa().setPAGain(AW87559_GAIN_16_5DB);
  ok &= ep.pa().enableAGC3(true);
  ok &= ep.pa().setAGC3Power(AW87559_AGC3_2_0W);
  ok &= ep.pa().setAGC2Power(AW87559_AGC2_3_0W);
  pyramidCodecSetVolume(JANUS_SPEAKER_CODEC_SAFE_VOLUME);
  updateEffectivePcmGainFromPhone();
  Serial.printf("PYRAMID PA PHONE-CURVE MODE | ok=%d | pa=16.5dB codec=%u%% local=%u%% phone=%u/127 pcm_gain=%u%%\n",
                ok ? 1 : 0, (unsigned)JANUS_SPEAKER_CODEC_SAFE_VOLUME,
                (unsigned)volumeLevel, (unsigned)janusPhoneVolume127, (unsigned)janusPcmGainX100);
}

uint8_t scanPyramidI2C(TwoWire &bus, const char* label, int sda, int scl) {
  Serial.printf("I2C SCAN | %s | SDA=%d SCL=%d\n", label, sda, scl);
  bus.begin(sda, scl, 100000);
  delay(40);
  uint8_t found = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; ++addr) {
    bus.beginTransmission(addr);
    uint8_t err = bus.endTransmission();
    if (err == 0) {
      found++;
      Serial.printf("I2C FOUND | %s | 0x%02X", label, addr);
      if (addr == 0x1A) Serial.print(" STM32_RGB_TOUCH");
      if (addr == 0x18) Serial.print(" ES8311_CODEC");
      if (addr == 0x40) Serial.print(" ES7210_ADC");
      if (addr == 0x5B) Serial.print(" AW87559_AMP");
      if (addr == 0x60) Serial.print(" SI5351_CLOCK");
      if (addr == 0x68) Serial.print(" ATOM_IMU_MPU6886");
      Serial.println();
    }
    delay(1);
  }
  if (!found) Serial.printf("I2C SCAN EMPTY | %s\n", label);
  return found;
}

bool busHasPyramidCore(TwoWire &bus) {
  bool hasStm = false;
  bool hasCodecOrClock = false;
  for (uint8_t addr : {uint8_t(0x1A), uint8_t(0x18), uint8_t(0x40), uint8_t(0x5B), uint8_t(0x60)}) {
    bus.beginTransmission(addr);
    if (bus.endTransmission() == 0) {
      if (addr == 0x1A) hasStm = true;
      else hasCodecOrClock = true;
    }
  }
  return hasStm || hasCodecOrClock;
}

void pyramidSetBrightness(uint8_t value) {
  if (btConnected || btPlaying) value = min<uint8_t>(value, JANUS_BT_BRIGHTNESS_CAP);
#if JANUS_SAFE_SOFT_BRIGHTNESS
  // Keep value 0 possible as a visual black-out, but do not send hardware brightness to STM32.
  // Also avoid Atom Matrix brightness=0 edge cases: matrix pixels are scaled by patterns anyway.
  brightnessLevel = value;
  uint8_t atomBr = value == 0 ? 1 : min<uint8_t>(max<uint8_t>(value, JANUS_MIN_VISIBLE_BRIGHTNESS), 80);
  atomMatrix.setBrightness(atomBr);
#else
  brightnessLevel = value;
  pyramidCtrlSetBrightness(1, brightnessLevel);
  pyramidCtrlSetBrightness(2, brightnessLevel);
  atomMatrix.setBrightness(min<uint8_t>(brightnessLevel, 80));
#endif
}

void pyramidFillRGB(uint8_t r, uint8_t g, uint8_t b) {
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    pyramidCtrlSetRGB(1, i, r, g, b);
    pyramidCtrlSetRGB(2, i, r, g, b);
  }
}

void atomFillRGB(uint8_t r, uint8_t g, uint8_t b) {
  uint32_t c = atomMatrix.Color(r, g, b);
  for (int i = 0; i < ATOM_MATRIX_LED_COUNT; ++i) atomMatrix.setPixelColor(i, c);
  atomMatrix.show();
}

void fillAllRGB(uint8_t r, uint8_t g, uint8_t b) {
  pyramidFillRGB(r, g, b);
  atomFillRGB(r, g, b);
}

void hsv2rgb(float h, float s, float v, uint8_t &r, uint8_t &g, uint8_t &b) {
  h = h - floorf(h);
  int i = int(h * 6.0f);
  float f = h * 6.0f - i;
  float p = v * (1.0f - s);
  float q = v * (1.0f - f * s);
  float t = v * (1.0f - (1.0f - f) * s);
  float rr, gg, bb;
  switch (i % 6) {
    case 0: rr = v; gg = t; bb = p; break;
    case 1: rr = q; gg = v; bb = p; break;
    case 2: rr = p; gg = v; bb = t; break;
    case 3: rr = p; gg = q; bb = v; break;
    case 4: rr = t; gg = p; bb = v; break;
    default: rr = v; gg = p; bb = q; break;
  }
  r = (uint8_t)(rr * 255.0f);
  g = (uint8_t)(gg * 255.0f);
  b = (uint8_t)(bb * 255.0f);
}

bool janusMinePaletteAllowed();

void janusOnShareDetected() {
  sharAmberUntilMs = millis() + 1150;
  Serial.println("SHAR AMBER FLASH");
}

void nextPalette() {
  do {
    currentPalette = (JanusPalette)(((uint8_t)currentPalette + 1) % PAL_COUNT);
  } while (currentPalette == PAL_JANUS_MINE && !janusMinePaletteAllowed());
  Serial.printf("PALETTE %u %s%s\n", (uint8_t)currentPalette, paletteName(currentPalette),
                (currentPalette == PAL_JANUS_MINE) ? " ACTIVE_SWARM" : "");
}

void brightnessUp() {
  brightnessLevel = min<uint8_t>(100, brightnessLevel + 8);
  pyramidSetBrightness(brightnessLevel);
  Serial.printf("BRIGHTNESS %u\n", brightnessLevel);
}

void brightnessDown() {
  brightnessLevel = (brightnessLevel >= 8) ? (brightnessLevel - 8) : 0;
  pyramidSetBrightness(brightnessLevel);
  Serial.printf("BRIGHTNESS %u\n", brightnessLevel);
}

void enterBrightnessTouchMode() {
  brightnessTouchMode = true;
  brightnessTouchUntilMs = millis() + BRIGHTNESS_TOUCH_MODE_MS;
  Serial.printf("BRIGHTNESS TOUCH MODE ON | brightness=%u\n", brightnessLevel);
  sharAmberUntilMs = millis() + 180;
}

void updateBrightnessTouchMode() {
  if (brightnessTouchMode && millis() > brightnessTouchUntilMs) {
    brightnessTouchMode = false;
    Serial.println("BRIGHTNESS TOUCH MODE OFF");
  }
}

void updateAudioEq() {
  uint32_t now = millis();
  if (now - lastEqMs < 24) return;
  lastEqMs = now;

  if (!btPlaying || muted) {
    audioEnergy = (uint8_t)((audioEnergy * 7) / 8);
    audioBass = (uint8_t)((audioBass * 7) / 8);
    audioMid = (uint8_t)((audioMid * 7) / 8);
    audioTreble = (uint8_t)((audioTreble * 7) / 8);
    return;
  }

  // v1.3: real amplitude only. No fake 60/60/60 fallback while music is silent.
  // This lets static palettes fade down when the iPhone volume is near zero or there is no sound.
  uint8_t b = janusPcmBass;
  uint8_t m = janusPcmMid;
  uint8_t h = janusPcmTreble;
  audioBass = (uint8_t)((audioBass * 5 + b) / 6);
  audioMid = (uint8_t)((audioMid * 5 + m) / 6);
  audioTreble = (uint8_t)((audioTreble * 5 + h) / 6);
  audioEnergy = (uint8_t)((audioBass + audioMid + audioTreble) / 3);
}

void updateEffectivePcmGainFromPhone() {
  // v1.7: single owner = phone volume. Pyramid stays at fixed safe max output.
  // At iPhone 127/127 the PCM gain reaches JANUS_PCM_GAIN_MAX_X100 = 108%.
  // At low phone volume it falls sharply so minimum is actually quiet.
  uint32_t phone = janusPhoneVolume127;       // 0..127 from iPhone/AVRCP
  volumeLevel = 100;                          // no independent Pyramid trim anymore
  uint32_t phoneCurve = (phone * phone + 126U) / 127U;  // 0..127
  uint32_t g = ((uint32_t)JANUS_PCM_GAIN_MAX_X100 * phoneCurve + 63U) / 127U;
  if (muted || phone == 0) g = 0;
  if (g > JANUS_PCM_GAIN_MAX_X100) g = JANUS_PCM_GAIN_MAX_X100;
  janusPcmGainX100 = (uint16_t)g;
}

void janusA2dpVolumeChanged(int volume) {
  if (volume < 0) volume = 0;
  if (volume > 127) volume = 127;
  janusPhoneVolume127 = (uint8_t)volume;
  updateEffectivePcmGainFromPhone();
  Serial.printf("PHONE_VOLUME_RX | phone=%d/127 pyramid_fixed=100%% safe_gain=%u%% max_gain=%u%%\n",
                volume, (unsigned)janusPcmGainX100, (unsigned)JANUS_PCM_GAIN_MAX_X100);
}

void setMuted(bool m) {
  muted = m;
  if (muted) {
    janusPhoneVolumeBeforeMute = janusPhoneVolume127 ? janusPhoneVolume127 : JANUS_PHONE_VOLUME_DEFAULT;
    janusPhoneVolume127 = 0;
    // Ask the phone to go down to zero. Local PCM gate also closes immediately.
    for (uint8_t i = 0; i < 18; ++i) sendAvrcPassthrough(ESP_AVRC_PT_CMD_VOL_DOWN, "MUTE_DEVICE_VOL_DOWN");
  } else {
    // Restore by asking the phone to come up close to previous level. The iPhone callback will settle exact value.
    uint8_t target = janusPhoneVolumeBeforeMute ? janusPhoneVolumeBeforeMute : JANUS_PHONE_VOLUME_DEFAULT;
    uint8_t steps = min<uint8_t>(18, (target + 7) / 8);
    for (uint8_t i = 0; i < steps; ++i) sendAvrcPassthrough(ESP_AVRC_PT_CMD_VOL_UP, "UNMUTE_DEVICE_VOL_UP");
    janusPhoneVolume127 = target;
  }
  pyramidCodecMute(false);  // do not kill the Pyramid path; phone/device volume is the mute owner.
  updateEffectivePcmGainFromPhone();
  Serial.printf("DEVICE MUTE %s | phone=%u/127 safe_gain=%u%% max_gain=%u%%\n",
                muted ? "ON" : "OFF", (unsigned)janusPhoneVolume127,
                (unsigned)janusPcmGainX100, (unsigned)JANUS_PCM_GAIN_MAX_X100);
}

void applyVolume() {
  // v1.7: Pyramid path is fixed; phone volume owns the curve.
  volumeLevel = 100;
  pyramidCodecSetVolume(JANUS_SPEAKER_CODEC_SAFE_VOLUME);
  updateEffectivePcmGainFromPhone();
  Serial.printf("SPEAKER PHONE-CURVE MAP | codec=%u%% phone=%u/127 safe_gain=%u%% max_gain=%u%%\n",
                (unsigned)JANUS_SPEAKER_CODEC_SAFE_VOLUME,
                (unsigned)janusPhoneVolume127, (unsigned)janusPcmGainX100,
                (unsigned)JANUS_PCM_GAIN_MAX_X100);
}


const char* knnLabelName(JanusKnnLabel label) {
  switch (label) {
    case KNN_NEW: return "NEW";
    case KNN_GOOD: return "GOOD";
    case KNN_HOT: return "HOT";
    case KNN_SHADOW: return "SHADOW";
    case KNN_SILENT: return "SILENT";
    case KNN_UNSTABLE: return "UNSTABLE";
    case KNN_SHARE_RICH: return "SHARE_RICH";
    case KNN_AUDIO_NODE: return "AUDIO_NODE";
    case KNN_PYRAMID: return "PYRAMID";
    default: return "UNKNOWN";
  }
}

JanusKnnLabel heuristicKnnLabel(uint16_t dropDelta) {
  if (btConnected || btPlaying) return KNN_AUDIO_NODE;

  uint32_t heap = ESP.getFreeHeap();

  // v1.8K: in exclusive Swarm mode BT is gone, so the old 28 KB BT heap guard
  // is no longer a real overheat signal. Your working log sits near 15.8 KB and
  // mines fine, so only call HOT at genuinely dangerous heap/jitter levels.
  if (espNowReady || janusExclusiveSwarmMode || janusBtStoppedForSwarm) {
    if (heap < JANUS_BATCH_SWARM_HOT_HEAP) return KNN_HOT;
    if (loopJitterUsEma > JANUS_BATCH_SWARM_HOT_JITTER || dropDelta > 5) return KNN_UNSTABLE;
    if (lastRssi != -127 && lastRssi < -88) return KNN_SHADOW;
    if (shares > lastKnnShareCount) return KNN_SHARE_RICH;
    if (jobReady) return KNN_PYRAMID;
    if (espNowReady) return KNN_GOOD;
    return KNN_SILENT;
  }

  // BT speaker mode still uses the conservative heap guard because Bluedroid/A2DP
  // needs a lot more breathing room than pure ESP-NOW.
  if (heap < JANUS_BT_LOW_HEAP_GUARD) return KNN_HOT;
  if (loopJitterUsEma > 3200 || dropDelta > 3) return KNN_UNSTABLE;
  if (lastRssi != -127 && lastRssi < -88) return KNN_SHADOW;
  if (shares > lastKnnShareCount) return KNN_SHARE_RICH;
  if (!jobReady && !espNowReady) return KNN_SILENT;
  if (pyramidReady) return KNN_PYRAMID;
  return KNN_GOOD;
}

JanusKnnSample makeKnnSample(JanusKnnLabel label, uint16_t dropDelta) {
  JanusKnnSample s = {};
  s.rssi = lastRssi;
  s.audio = audioEnergy;
  s.bt = (btConnected ? 1 : 0) | (btPlaying ? 2 : 0) | (muted ? 4 : 0);
  s.jitter = loopJitterUsEma;
  s.heap_kb = (uint16_t)min<uint32_t>(65535UL, ESP.getFreeHeap() / 1024UL);
  s.drop_delta = dropDelta;
  s.batch = effectiveBatch();
  s.label = (uint8_t)label;
  return s;
}

void knnLearn(JanusKnnLabel label) {
  uint32_t now = millis();
  if (now - lastKnnLearnMs < JANUS_KNN_MIN_LEARN_MS && label != KNN_SHARE_RICH) return;
  uint16_t dropDelta = (uint16_t)min<uint32_t>(65535UL, janusAudioDropChunks - lastKnnDropCount);
  knnMemory[knnMemoryHead] = makeKnnSample(label, dropDelta);
  knnMemoryHead = (knnMemoryHead + 1) % JANUS_KNN_MEMORY;
  if (knnMemoryCount < JANUS_KNN_MEMORY) knnMemoryCount++;
  lastKnnLearnMs = now;
}

uint32_t knnDistance(const JanusKnnSample& a, const JanusKnnSample& b) {
  uint32_t d = 0;
  d += (uint32_t)abs((int)a.rssi - (int)b.rssi) * 3U;
  d += (uint32_t)abs((int)a.audio - (int)b.audio) * 2U;
  d += (uint32_t)abs((int)a.bt - (int)b.bt) * 80U;
  d += (uint32_t)abs((int)a.jitter - (int)b.jitter) / 8U;
  d += (uint32_t)abs((int)a.heap_kb - (int)b.heap_kb) * 4U;
  d += (uint32_t)abs((int)a.drop_delta - (int)b.drop_delta) * 35U;
  d += (uint32_t)abs((int)a.batch - (int)b.batch) / 5U;
  return d;
}

void applyKnnPolicy(JanusKnnLabel label, uint8_t confidence) {
  localKnnLabel = label;
  localKnnConfidence = confidence;
  switch (label) {
    case KNN_AUDIO_NODE:
      localKnnAiHint = 2;
      localKnnTargetBatch = JANUS_BATCH_BT_PLAYING;
      break;
    case KNN_HOT:
      localKnnAiHint = 2;
      localKnnTargetBatch = max<uint16_t>(JANUS_BATCH_DYNAMIC_MIN, min<uint16_t>(janusDynamicBatch, 64));
      break;
    case KNN_UNSTABLE:
      localKnnAiHint = 2;
      localKnnTargetBatch = max<uint16_t>(JANUS_BATCH_DYNAMIC_MIN, min<uint16_t>(janusDynamicBatch, 96));
      break;
    case KNN_SHADOW:
      localKnnAiHint = 2;
      localKnnTargetBatch = max<uint16_t>((uint16_t)80, min<uint16_t>(janusDynamicBatch, 140));
      break;
    case KNN_SHARE_RICH:
      localKnnAiHint = 3;
      localKnnTargetBatch = min<uint16_t>(JANUS_BATCH_DYNAMIC_BOOST_MAX, max<uint16_t>(janusDynamicBatch + 32, 160));
      break;
    case KNN_SILENT:
      localKnnAiHint = 0;
      localKnnTargetBatch = janusDynamicBatch;
      break;
    case KNN_PYRAMID:
    case KNN_GOOD:
    default:
      localKnnAiHint = 1;
      localKnnTargetBatch = janusDynamicBatch;
      break;
  }
}

void updateLocalKnnAgent() {
  uint32_t now = millis();
  // Keep the initial Classic-BT discoverable window quiet.
  // The log showed core1 aborts while now=0 / bt=0 during repeated idle KNN updates,
  // before ESP-NOW had even started. KNN still wakes once BT connects or colony mode starts.
  if (!btConnected && !btPlaying && !espNowReady && !jobReady && now < (JANUS_ESPNOW_BOOT_DELAY_MS + 9000UL)) return;
  if (now - lastKnnUpdateMs < JANUS_KNN_UPDATE_MS) return;
  lastKnnUpdateMs = now;

  uint16_t dropDelta = (uint16_t)min<uint32_t>(65535UL, janusAudioDropChunks - lastKnnDropCount);
  JanusKnnLabel h = heuristicKnnLabel(dropDelta);
  JanusKnnSample cur = makeKnnSample(h, dropDelta);

  if (knnMemoryCount < 3) {
    knnLearn(h);
    applyKnnPolicy(h, 60);
  } else {
    uint32_t bestD[3] = {0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL};
    uint8_t bestL[3] = {0, 0, 0};
    for (uint8_t i = 0; i < knnMemoryCount; ++i) {
      uint32_t d = knnDistance(cur, knnMemory[i]);
      for (uint8_t k = 0; k < 3; ++k) {
        if (d < bestD[k]) {
          for (int m = 2; m > (int)k; --m) { bestD[m] = bestD[m - 1]; bestL[m] = bestL[m - 1]; }
          bestD[k] = d; bestL[k] = knnMemory[i].label;
          break;
        }
      }
    }
    uint8_t votes[KNN_COUNT] = {0};
    for (uint8_t k = 0; k < 3; ++k) if (bestL[k] < KNN_COUNT) votes[bestL[k]]++;
    uint8_t winner = (uint8_t)h;
    uint8_t winVotes = votes[winner];
    for (uint8_t i = 0; i < KNN_COUNT; ++i) {
      if (votes[i] > winVotes) { winner = i; winVotes = votes[i]; }
    }
    uint8_t conf = (uint8_t)min<int>(99, 50 + winVotes * 16);
    applyKnnPolicy((JanusKnnLabel)winner, conf);
    // Keep memory alive: learn the heuristic state, and let real reward/share overwrite with richer labels.
    knnLearn(h);
  }

  lastKnnDropCount = janusAudioDropChunks;
  lastKnnShareCount = shares;
  Serial.printf("SLIME KNN LOCAL | label=%s conf=%u%% hint=%u target_batch=%u heap=%lu rssi=%d audio=%u jitter=%u drop=%u\n",
                knnLabelName(localKnnLabel), (unsigned)localKnnConfidence,
                (unsigned)localKnnAiHint, (unsigned)localKnnTargetBatch,
                (unsigned long)ESP.getFreeHeap(), (int)lastRssi,
                (unsigned)audioEnergy, (unsigned)loopJitterUsEma, (unsigned)dropDelta);
}

bool sendAvrcPassthrough(uint8_t cmd, const char* label) {
#if JANUS_ENABLE_BT_A2DP
  if (!btConnected || !btAclAccepted) {
    Serial.printf("AVRCP %s IGNORED | bt=%d acl=%d\n", label ? label : "?", btConnected ? 1 : 0, btAclAccepted ? 1 : 0);
    return false;
  }
  esp_avrc_ct_send_passthrough_cmd(0, cmd, ESP_AVRC_PT_CMD_STATE_PRESSED);
  delay(18);
  esp_avrc_ct_send_passthrough_cmd(1, cmd, ESP_AVRC_PT_CMD_STATE_RELEASED);
  return true;
#else
  (void)cmd; (void)label;
  return false;
#endif
}

void trackNext() {
  if (sendAvrcPassthrough(ESP_AVRC_PT_CMD_FORWARD, "NEXT")) Serial.println("AVRCP NEXT");
}
void trackPrev() {
  if (sendAvrcPassthrough(ESP_AVRC_PT_CMD_BACKWARD, "PREV")) Serial.println("AVRCP PREV");
}

void phoneVolumeUp() {
  muted = false;
  uint8_t oldPhone = janusPhoneVolume127;
  janusPhoneVolume127 = min<uint8_t>(127, janusPhoneVolume127 + 8);
  sendAvrcPassthrough(ESP_AVRC_PT_CMD_VOL_UP, "PHONE_VOL_UP");
  applyVolume();
  Serial.printf("PHONE VOLUME UP | predict %u->%u/127 safe_gain=%u%% max_gain=%u%%\n",
                oldPhone, (unsigned)janusPhoneVolume127, (unsigned)janusPcmGainX100, (unsigned)JANUS_PCM_GAIN_MAX_X100);
}
void phoneVolumeDown() {
  uint8_t oldPhone = janusPhoneVolume127;
  janusPhoneVolume127 = (janusPhoneVolume127 >= 8) ? (janusPhoneVolume127 - 8) : 0;
  if (janusPhoneVolume127 == 0) muted = true;
  sendAvrcPassthrough(ESP_AVRC_PT_CMD_VOL_DOWN, "PHONE_VOL_DOWN");
  applyVolume();
  Serial.printf("PHONE VOLUME DOWN | predict %u->%u/127 safe_gain=%u%% max_gain=%u%%\n",
                oldPhone, (unsigned)janusPhoneVolume127, (unsigned)janusPcmGainX100, (unsigned)JANUS_PCM_GAIN_MAX_X100);
}
void speakerVolumeUp() { phoneVolumeUp(); }
void speakerVolumeDown() { phoneVolumeDown(); }
void volumeUp() { phoneVolumeUp(); }
void volumeDown() { phoneVolumeDown(); }

// =====================================================
// HASH / MINING
// =====================================================

void sha256_once(const uint8_t* data, size_t len, uint8_t out[32]) {
#if MBEDTLS_VERSION_MAJOR >= 3
  mbedtls_sha256(data, len, out, 0);
#else
  mbedtls_sha256_ret(data, len, out, 0);
#endif
}

void doubleSha256(const uint8_t* data, size_t len, uint8_t out[32]) {
  uint8_t tmp[32];
  sha256_once(data, len, tmp);
  sha256_once(tmp, 32, out);
}

void setNonceLE(uint8_t header[80], uint32_t nonce) {
  header[76] = (uint8_t)(nonce & 0xFF);
  header[77] = (uint8_t)((nonce >> 8) & 0xFF);
  header[78] = (uint8_t)((nonce >> 16) & 0xFF);
  header[79] = (uint8_t)((nonce >> 24) & 0xFF);
}

int compareBE32(const uint8_t a[32], const uint8_t b[32]) {
  for (int i = 0; i < 32; ++i) {
    if (a[i] < b[i]) return -1;
    if (a[i] > b[i]) return 1;
  }
  return 0;
}

uint16_t leadingZeroBits(const uint8_t h[32]) {
  uint16_t bits = 0;
  for (int i = 0; i < 32; ++i) {
    uint8_t v = h[i];
    if (v == 0) { bits += 8; continue; }
    for (int b = 7; b >= 0; --b) {
      if (v & (1 << b)) return bits;
      bits++;
    }
  }
  return bits;
}

uint16_t effectiveBatch() {
  if (!jobReady) return 0;
  if (millis() - jobReceivedMs > JANUS_JOB_STALE_MS) return 0;

  uint16_t b = janusDynamicBatch ? janusDynamicBatch : JANUS_BATCH_DYNAMIC_START;

  // Buzz/NAS-BRAIN may send a targetBatch reward. Treat it as a teacher signal,
  // not as an order that can overrun the Atom Matrix.
  if (rewardTargetBatch > 0 && millis() - lastRewardMs < 20000UL) {
    uint16_t rb = min<uint16_t>(rewardTargetBatch, JANUS_BATCH_DYNAMIC_BOOST_MAX);
    if (rewardAiHint == 3) b = max<uint16_t>(b, rb);
    else if (rewardAiHint == 2) b = min<uint16_t>(b, rb);
    else b = (uint16_t)((b * 3U + rb) / 4U);
  }

  uint8_t effectiveHint = rewardAiHint ? rewardAiHint : localKnnAiHint;
  if (effectiveHint == 2) {
    // v1.8K: slow down only when the local model really sees danger.
    if (ESP.getFreeHeap() < JANUS_BATCH_SWARM_HOT_HEAP || loopJitterUsEma > JANUS_BATCH_SWARM_HOT_JITTER) {
      b = min<uint16_t>(b, 64);
    } else {
      b = min<uint16_t>(b, max<uint16_t>(localKnnTargetBatch, JANUS_BATCH_DYNAMIC_MIN));
    }
  }
  if (effectiveHint == 3) b = min<uint16_t>(max<uint16_t>(b + 24, localKnnTargetBatch), JANUS_BATCH_DYNAMIC_BOOST_MAX);

  if (btPlaying) b = min<uint16_t>(b, JANUS_BATCH_BT_PLAYING);
  else if (btConnected) b = min<uint16_t>(b, JANUS_BATCH_BT_CONNECTED);

  b = min<uint16_t>(b, janusDynamicBatchCeiling);
  b = min<uint16_t>(b, JANUS_BATCH_ABSOLUTE_MAX);
  return max<uint16_t>(b, (uint16_t)JANUS_BATCH_DYNAMIC_MIN);
}

uint8_t janusCurrentWifiChannel() {
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) != ESP_OK) return 0;
  return primary;
}

void janusForceEspNowChannel(uint8_t ch, const char* reason) {
  if (ch < 1 || ch > 13) ch = JANUS_ESPNOW_CHANNEL;

  // v1.8I6 IMPORTANT:
  // Do NOT use esp_wifi_set_promiscuous(true) on Atom Matrix while Classic BT/A2DP is loaded.
  // Your I5 log showed:
  //   wifi:promis buf: out of memory
  // immediately followed by heap ~= 3.6 KB and no JOB RX.
  // ESP-NOW does not require promiscuous mode here; peer.channel=0 follows the home channel.
  // In v1.8J this runs only after BT is stopped, so Wi-Fi can own the radio cleanly.
  esp_wifi_start();
  delay(25);
  esp_err_t e = esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  delay(12);
  janusEspNowHomeChannel = janusCurrentWifiChannel();
  Serial.printf("ESP-NOW CHANNEL SET NO_PROMISC | want=%u actual=%u err=%d reason=%s heap=%lu\n",
                (unsigned)ch, (unsigned)janusEspNowHomeChannel, (int)e,
                reason ? reason : "?", (unsigned long)ESP.getFreeHeap());
}

void ensurePeer(const uint8_t mac[6]) {
  if (!espNowReady) return;
  if (esp_now_is_peer_exist(mac)) return;
  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, mac, 6);
  // Critical v1.8I6:
  // channel=0 follows the current STA/home channel. Fixed channel peers caused:
  // "Peer channel is not equal to the home channel, send fail!"
  peer.channel = 0;
  peer.encrypt = false;
  esp_err_t err = esp_now_add_peer(&peer);
  if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
    Serial.printf("ESP-NOW add peer failed: %d home_ch=%u\n", (int)err, (unsigned)janusCurrentWifiChannel());
  }
}

void sendShare(uint32_t nonce, const uint8_t displayHash[32], uint16_t bits) {
  if (!espNowReady || !masterKnown) return;

  ensurePeer(masterMac);

  // Send legacy S/R first because TD/Core/Buzz all understand it.
  ShareResponse srLegacy = {};
  srLegacy.magic[0] = 'S'; srLegacy.magic[1] = 'R';
  memcpy(srLegacy.job_id, activeJob.job_id, sizeof(srLegacy.job_id));
  srLegacy.nonce = nonce;
  srLegacy.worker_id = workerId;
  esp_err_t errLegacy = esp_now_send(masterMac, (uint8_t*)&srLegacy, sizeof(srLegacy));

  // Also send extended S/2, so Buzz can use bits/hash_tail when available.
  ShareResponseV2 sr = {};
  sr.magic[0] = 'S'; sr.magic[1] = '2';
  memcpy(sr.job_id, activeJob.job_id, sizeof(sr.job_id));
  sr.nonce = nonce;
  sr.worker_id = workerId;
  sr.bits = bits;
  sr.total_hashes_l32 = totalHashes;
  memcpy(sr.hash_tail, displayHash + 28, 4);
  esp_err_t err = esp_now_send(masterMac, (uint8_t*)&sr, sizeof(sr));

  if (errLegacy == ESP_OK || err == ESP_OK) {
    shares++;
    knnLearn(KNN_SHARE_RICH);
    janusOnShareDetected();
    Serial.printf("SHARE TX | nonce=%lu bits=%u sr=%d s2=%d\n",
                  (unsigned long)nonce, (unsigned)bits, (int)errLegacy, (int)err);
  } else {
    rejects++;
    Serial.printf("SHARE SEND FAIL sr=%d s2=%d home_ch=%u\n", (int)errLegacy, (int)err, (unsigned)janusCurrentWifiChannel());
  }
}

void mineSlice() {
  if (!espNowReady) return;
  uint16_t batch = effectiveBatch();
  if (batch == 0) return;

  uint32_t nowMs = millis();
  uint32_t gapMs = 0;
  uint32_t budgetUs = JANUS_MINE_BUDGET_IDLE_US;
  if (btPlaying) {
    gapMs = JANUS_MINE_SLICE_AUDIO_MS;
    budgetUs = JANUS_MINE_BUDGET_AUDIO_US;
  } else if (btConnected) {
    gapMs = JANUS_MINE_SLICE_BT_IDLE_MS;
    budgetUs = JANUS_MINE_BUDGET_BT_IDLE_US;
  }
  if (gapMs && (nowMs - janusLastMineSliceMs < gapMs)) return;
  janusLastMineSliceMs = nowMs;

  uint32_t startUs = micros();
  uint8_t header[80];
  uint8_t rawHash[32];
  uint8_t displayHash[32];

  for (uint16_t i = 0; i < batch; ++i) {
    if (nonceCursor == nonceEnd) {
      jobReady = false;
      Serial.println("JOB RANGE DONE");
      return;
    }

    uint32_t nonce = nonceCursor++;
    memcpy(header, activeJob.header, 80);
    setNonceLE(header, nonce);
    doubleSha256(header, 80, rawHash);
    for (int j = 0; j < 32; ++j) displayHash[j] = rawHash[31 - j];

    uint16_t bits = leadingZeroBits(displayHash);
    if (bits > bestBits) bestBits = bits;
    totalHashes++;
    hashWindowCount++;

    if (compareBE32(displayHash, activeJob.target) <= 0) {
      Serial.printf("SHAR nonce=%lu bits=%u\n", (unsigned long)nonce, bits);
      sendShare(nonce, displayHash, bits);
      // Continue scanning: Buzz will verify and filter stale/duplicate shares.
    }

    // v1.8I: hard budget so PCM/A2DP never starves. This is the Buzz-style throttle.
    if ((i & 0x03) == 0x03 && (uint32_t)(micros() - startUs) >= budgetUs) break;
  }
}

uint8_t janusEstimateThermalLoad() {
  // No reliable board temperature sensor is exposed on all Atom Matrix cores.
  // This proxy uses the physical symptoms we actually see: heap pressure, loop jitter,
  // RF shadow and falling efficiency. Buzz receives it via HM.reserved.
  uint32_t heap = ESP.getFreeHeap();
  uint16_t jitter = loopJitterUsEma;
  int load = 0;
  if (heap < JANUS_BATCH_SWARM_HOT_HEAP) load += 55;
  else if (heap < JANUS_BATCH_SWARM_WARM_HEAP) load += 34;
  else if (heap < JANUS_BATCH_SWARM_GOOD_HEAP) load += 16;
  if (jitter > JANUS_BATCH_SWARM_HOT_JITTER) load += 36;
  else if (jitter > JANUS_BATCH_SWARM_WARM_JITTER) load += 18;
  else if (jitter > 7500) load += 8;
  if (lastRssi != -127 && lastRssi < -82) load += 12;
  if (janusPredictionErrorLocal > 0.45f) load += 10;
  return clamp8(load);
}

void janusDynamicBatchAiTick() {
#if JANUS_ENABLE_COLONY_ESPNOW
  if (!espNowReady || !janusExclusiveSwarmMode) return;
  uint32_t now = millis();
  if (now - janusLastDynamicBatchMs < JANUS_BATCH_DYNAMIC_TICK_MS) return;
  janusLastDynamicBatchMs = now;

  uint32_t h = currentHashRate;
  if (janusHashEma <= 0.01f) janusHashEma = (float)h;
  else janusHashEma = janusHashEma * 0.82f + (float)h * 0.18f;

  float eff = (float)h / max<float>(1.0f, (float)max<uint16_t>(janusDynamicBatch, 1));
  if (janusEfficiencyEma <= 0.01f) janusEfficiencyEma = eff;
  else janusEfficiencyEma = janusEfficiencyEma * 0.86f + eff * 0.14f;

  janusPredictedHash = janusEfficiencyEma * (float)janusDynamicBatch;
  janusPredictionErrorLocal = fabsf((float)h - janusPredictedHash) / max<float>(1.0f, (float)h + 1.0f);
  if (janusPredictionErrorLocal > 9.99f) janusPredictionErrorLocal = 9.99f;
  lastPredictionError = janusPredictionErrorLocal;

  janusThermalLoad = janusEstimateThermalLoad();

  uint16_t oldBatch = janusDynamicBatch;
  int16_t next = (int16_t)janusDynamicBatch;
  uint32_t heap = ESP.getFreeHeap();

  if (!jobReady) {
    // Keep the learned value, but slowly relax toward a useful restart batch.
    if (next < JANUS_BATCH_DYNAMIC_START) next += 4;
  } else if (janusThermalLoad >= 82 || heap < JANUS_BATCH_SWARM_HOT_HEAP || loopJitterUsEma > JANUS_BATCH_SWARM_HOT_JITTER) {
    next -= 24;
  } else if (janusThermalLoad >= 58 || heap < JANUS_BATCH_SWARM_WARM_HEAP || loopJitterUsEma > JANUS_BATCH_SWARM_WARM_JITTER) {
    next -= 8;
  } else {
    // Good RF/heap and stable efficiency: climb gradually.
    if ((lastRssi == -127 || lastRssi > -72) && heap >= JANUS_BATCH_SWARM_GOOD_HEAP) next += 12;
    else next += 6;

    // If increasing batch no longer improves H/s per batch, back off a little.
    if (janusEfficiencyEma > 1.0f && eff < janusEfficiencyEma * 0.72f && h > 0) next -= 18;
    if (rewardAiHint == 3 && millis() - lastRewardMs < 15000UL) next += 16;
    if (rewardAiHint == 2 && millis() - lastRewardMs < 15000UL) next -= 12;
  }

  if (heap > 22000 && loopJitterUsEma < 7500) janusDynamicBatchCeiling = JANUS_BATCH_DYNAMIC_BOOST_MAX;
  else if (heap > JANUS_BATCH_SWARM_GOOD_HEAP && loopJitterUsEma < JANUS_BATCH_SWARM_WARM_JITTER) janusDynamicBatchCeiling = JANUS_BATCH_DYNAMIC_SAFE_MAX;
  else janusDynamicBatchCeiling = 128;

  janusDynamicBatch = constrain((uint16_t)max<int16_t>(JANUS_BATCH_DYNAMIC_MIN, next),
                                (uint16_t)JANUS_BATCH_DYNAMIC_MIN, janusDynamicBatchCeiling);

  // Feed the local policy and Buzz HM telemetry with the learned target.
  localKnnTargetBatch = janusDynamicBatch;
  if (janusThermalLoad >= 70) localKnnAiHint = 2;
  else if (jobReady && h > janusHashEma * 1.08f) localKnnAiHint = 3;
  else localKnnAiHint = 1;

  if (oldBatch != janusDynamicBatch || now - janusLastDynamicLogMs > 9000UL) {
    janusLastDynamicLogMs = now;
    Serial.printf("SLIME BATCH AI | batch=%u->%u cap=%u H/s=%lu ema=%.1f eff=%.2f pred=%.1f err=%.3f thermal=%u heap=%lu jitter=%u rssi=%d hint=%u\n",
                  (unsigned)oldBatch, (unsigned)janusDynamicBatch, (unsigned)janusDynamicBatchCeiling,
                  (unsigned long)h, janusHashEma, janusEfficiencyEma, janusPredictedHash, janusPredictionErrorLocal,
                  (unsigned)janusThermalLoad, (unsigned long)heap, (unsigned)loopJitterUsEma,
                  (int)lastRssi, (unsigned)localKnnAiHint);
  }
#endif
}

// =====================================================
// ESP-NOW
// =====================================================

void acceptPendingJobIfAny() {
  if (!espNowReady) return;
  if (!nowJobPending) return;
  noInterrupts();
  JobPacket jp = pendingJob;
  uint8_t mac[6]; memcpy(mac, pendingJobMac, 6);
  int8_t rssi = pendingJobRssi;
  nowJobPending = false;
  interrupts();

  activeJob = jp;
  memcpy(masterMac, mac, 6);
  masterKnown = true;
  ensurePeer(masterMac);
  lastRssi = rssi;
  jobReceivedMs = millis();
  nonceCursor = activeJob.start_nonce;
  nonceEnd = activeJob.start_nonce + activeJob.range_size;
  jobReady = activeJob.range_size > 0;
  Serial.printf("JOB RX range=%lu start=%lu rssi=%d\n", (unsigned long)activeJob.range_size, (unsigned long)activeJob.start_nonce, (int)rssi);
}

void applyReward(const JanusAgentRewardPacket& ar) {
  bool targetAll = strncmp(ar.targetNode, "all", 3) == 0 || ar.targetNode[0] == 0;
  bool targetMe = strncmp(ar.targetNode, JANUS_DEVICE_NAME, sizeof(ar.targetNode)) == 0 || strstr(ar.targetNode, "ECHO") != nullptr;
  if (!targetAll && !targetMe) return;

  rewardLevel = ar.rewardLevel;
  rewardAiHint = ar.aiHint;
  if (ar.targetBatch > 0) rewardTargetBatch = min<uint16_t>(ar.targetBatch, JANUS_BATCH_ABSOLUTE_MAX);
  lastPredictionError = ar.predictionError;
  lastRewardMs = millis();

  Serial.printf("REWARD lvl=%u hint=%u batch=%u err=%.4f\n", rewardLevel, rewardAiHint, rewardTargetBatch, lastPredictionError);
  if (rewardLevel >= 3) janusOnShareDetected();
}

uint16_t janusPyramidHash16(const char* s) {
  uint16_t h = 21661U;
  if (!s) return h;
  while (*s) {
    h ^= (uint8_t)*s++;
    h = (uint16_t)(h * 16719U);
  }
  return h ? h : 1;
}

uint16_t janusPyramidCapabilities() {
  uint16_t caps = JC_TOUCH | JC_RF | JC_AUDIO | JC_HASH | JC_AI;
#if JANUS_PYRAMID_GROVE_STICK_LINK
  caps |= JC_RELAY;   // physical Grove/Stick companion sentinel
#endif
  return caps;
}

bool janusPyramidEmitEvent(uint8_t eventType, const char* kind, uint8_t confidence, uint8_t urgency,
                           int16_t a_x10, int16_t b_x10, int16_t c_x10, int16_t d_x10,
                           uint16_t topicHash, uint16_t objectHash, uint32_t ttlMs) {
#if JANUS_PYRAMID_HOME_CORTEX_ENABLE
  if (!espNowReady) return false;
  JanusEventPacket ev = {};
  ev.magic[0] = 'J'; ev.magic[1] = 'E';
  ev.version = 1;
  ev.eventType = eventType;
  ev.nodeRole = JR_PYRAMID;
  ev.confidence = confidence;
  ev.urgency = urgency;
  snprintf(ev.nodeId, sizeof(ev.nodeId), "%s", JANUS_DEVICE_NAME);
  snprintf(ev.kind, sizeof(ev.kind), "%s", kind && kind[0] ? kind : "pyramid_grove");
  ev.seq = ++janusPyramidEventSeq;
  ev.uptimeMs = millis();
  ev.topicHash = topicHash ? topicHash : janusPyramidHash16("pyramid");
  ev.objectHash = objectHash;
  ev.capabilities = janusPyramidCapabilities();
  ev.valueA_x10 = a_x10;
  ev.valueB_x10 = b_x10;
  ev.valueC_x10 = c_x10;
  ev.valueD_x10 = d_x10;
  ev.eventHash = ((uint32_t)eventType << 24) ^ ((uint32_t)ev.topicHash << 8) ^ ev.seq ^ (uint32_t)workerId;
  ev.ttlMs = ttlMs ? ttlMs : 12000UL;
  esp_err_t e = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&ev, sizeof(ev));
  return e == ESP_OK;
#else
  (void)eventType; (void)kind; (void)confidence; (void)urgency; (void)a_x10; (void)b_x10; (void)c_x10; (void)d_x10; (void)topicHash; (void)objectHash; (void)ttlMs;
  return false;
#endif
}

void handlePyramidPolicy(const JanusPolicyPacket& jp) {
#if JANUS_PYRAMID_HOME_CORTEX_ENABLE
  if (jp.magic[0] != 'J' || jp.magic[1] != 'P' || jp.version != 1) return;
  if (jp.seq && jp.seq == janusPyramidPolicySeq) return;
  janusPyramidPolicySeq = jp.seq;
  janusPyramidPolicyRx++;
  janusPyramidLastPolicyMs = millis();
  janusPyramidMood = jp.swarmMood;
  janusPyramidRadioRate = jp.radioRate;
  janusPyramidSensorRate = jp.sensorRate;
  janusPyramidPolicyConfidence = jp.confidence;
  janusPyramidDangerX100 = jp.danger_x100;
  janusPyramidQuietUntilMs = jp.quietUntilMs ? millis() + min<uint32_t>(jp.quietUntilMs, 60000UL) : 0;
  snprintf(janusPyramidOrder, sizeof(janusPyramidOrder), "%s", jp.order[0] ? jp.order : "-");
  Serial.printf("PYRAMID POLICY RX | n=%lu mood=%u radio=%u sensor=%u danger=%.2f order=%s\n",
                (unsigned long)janusPyramidPolicyRx, (unsigned)janusPyramidMood,
                (unsigned)janusPyramidRadioRate, (unsigned)janusPyramidSensorRate,
                (float)janusPyramidDangerX100 / 100.0f, janusPyramidOrder);
#endif
}

void pyramidBlackboardTick() {
#if JANUS_PYRAMID_HOME_CORTEX_ENABLE
  if (!espNowReady) return;
  uint32_t now = millis();
  if (janusPyramidQuietUntilMs && now < janusPyramidQuietUntilMs) return;

  uint32_t interval = JANUS_PYRAMID_EVENT_MS;
  if (janusPyramidRadioRate == 2 || janusPyramidMood == 2 || janusPyramidMood == 4) interval = 3500UL;
  else if (janusPyramidRadioRate == 0) interval = JANUS_PYRAMID_EVENT_MS * 2UL;

  if (now - janusPyramidLastEventMs >= interval) {
    janusPyramidLastEventMs = now;
    uint8_t flags = 0;
    if (janusExclusiveSwarmMode || janusBtStoppedForSwarm) flags |= 0x01;
    if (btConnected || btPlaying) flags |= 0x02;
#if JANUS_PYRAMID_GROVE_STICK_LINK
    flags |= 0x04;
#endif
    janusPyramidEmitEvent(JE_HEARTBEAT, "pyramid_grove_stick", 86, janusThermalLoad > 70 ? 58 : 24,
                          (int16_t)min<uint32_t>(32767UL, ESP.getFreeHeap() / 1024UL),
                          (int16_t)min<uint32_t>(32767UL, currentHashRate / 10UL),
                          (int16_t)flags,
                          (int16_t)janusThermalLoad,
                          janusPyramidHash16("pyramid"), janusPyramidHash16("stick_grove"), 14000UL);
  }

  if (now - janusPyramidLastTaskMs >= JANUS_PYRAMID_TASK_MS) {
    janusPyramidLastTaskMs = now;
    if (ESP.getFreeHeap() < JANUS_BATCH_SWARM_HOT_HEAP || janusThermalLoad >= 82) {
      janusPyramidEmitEvent(JE_TASK_NEED, "pyramid_cooldown", 82, 72,
                            (int16_t)min<uint32_t>(32767UL, ESP.getFreeHeap() / 1024UL),
                            (int16_t)loopJitterUsEma,
                            (int16_t)janusThermalLoad,
                            (int16_t)effectiveBatch(),
                            janusPyramidHash16("thermal"), janusPyramidHash16("pyramid"), 20000UL);
    } else if (janusExclusiveSwarmMode || janusBtStoppedForSwarm) {
      janusPyramidEmitEvent(JE_SAFE, "pyramid_swarm_ready", 84, 22,
                            (int16_t)min<uint32_t>(32767UL, ESP.getFreeHeap() / 1024UL),
                            (int16_t)currentHashRate,
                            (int16_t)effectiveBatch(),
                            (int16_t)janusPyramidPolicyRx,
                            janusPyramidHash16("radio"), janusPyramidHash16("pyramid"), 20000UL);
    }
  }
#endif
}


#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onNowRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  const uint8_t* mac = info ? info->src_addr : JANUS_BROADCAST_MAC;
  int8_t rssi = (info && info->rx_ctrl) ? info->rx_ctrl->rssi : -127;
#else
void onNowRecv(const uint8_t* mac, const uint8_t* data, int len) {
  int8_t rssi = -127;
#endif
  if (!data || len < 2) return;

  if (data[0] == 'J' && data[1] == 'P' && len >= (int)sizeof(JanusPolicyPacket)) {
    JanusPolicyPacket jp = {};
    memcpy(&jp, data, sizeof(jp));
    handlePyramidPolicy(jp);
    return;
  }

  if (data[0] == 'J' && data[1] == 'B' && len >= (int)sizeof(JobPacket)) {
    JobPacket jp = {};
    memcpy(&jp, data, sizeof(jp));
    noInterrupts();
    pendingJob = jp;
    memcpy(pendingJobMac, mac, 6);
    pendingJobRssi = rssi;
    nowJobPending = true;
    interrupts();
    return;
  }

  if (data[0] == 'A' && data[1] == 'R') {
    if (len >= (int)sizeof(JanusAgentRewardPacket)) {
      JanusAgentRewardPacket ar = {};
      memcpy(&ar, data, sizeof(ar));
      applyReward(ar);
      return;
    }
    if (len >= (int)sizeof(JanusAgentRewardPacketLegacy)) {
      JanusAgentRewardPacketLegacy lr = {};
      memcpy(&lr, data, sizeof(lr));
      JanusAgentRewardPacket ar = {};
      ar.magic[0] = 'A'; ar.magic[1] = 'R';
      ar.version = lr.version;
      snprintf(ar.source, sizeof(ar.source), "%s", "legacy");
      memcpy(ar.targetNode, lr.targetNode, sizeof(ar.targetNode));
      ar.seq = lr.seq;
      ar.rewardLevel = lr.rewardLevel;
      ar.aiHint = lr.aiHint;
      ar.rewardPoints = lr.rewardPoints;
      ar.targetBatch = lr.targetBatch;
      ar.predictionError = lr.predictionError;
      ar.uptime_ms = millis();
      applyReward(ar);
      return;
    }
  }
}

void janusAudioHardStop(const char* reason, bool muteCodec);
void initBluetoothAudio();
void stopEspNow(const char* reason);
void janusEnterExclusiveSwarmMode(const char* reason);
void janusReturnToBtByRestart(const char* reason);
bool janusMinePaletteAllowed();

#if ESP_IDF_VERSION_MAJOR >= 5
void onNowSent(const wifi_tx_info_t* tx_info, esp_now_send_status_t status) {
  (void)tx_info;
  if (status != ESP_NOW_SEND_SUCCESS) {
    // Keep quiet; ESP-NOW can be lossy while BT A2DP is active.
  }
}
#else
void onNowSent(const uint8_t* mac, esp_now_send_status_t status) {
  (void)mac;
  if (status != ESP_NOW_SEND_SUCCESS) {
    // Keep quiet; ESP-NOW can be lossy while BT A2DP is active.
  }
}
#endif

bool janusShouldDeferEspNowForBtPairing() {
#if !JANUS_ENABLE_BT_A2DP
  return false;
#else
#if JANUS_EXCLUSIVE_SWARM_MODE
  // In exclusive swarm mode BT/A2DP is fully stopped; ESP-NOW is allowed to own the radio.
  if (janusBtStoppedForSwarm || janusExclusiveSwarmMode) return false;
#endif
  uint32_t now = millis();
  if (!btA2dpStarted) return true;
  if (btApprovalPending) return true;

  // Stable Atom Matrix policy: BT request/connection/audio owns the radio.
  if (btConnected || btAclAccepted || btPlaying) return true;

  if (btLastDisconnectMs && now - btLastDisconnectMs < JANUS_BT_FIRST_AFTER_DISCONNECT_GRACE_MS) return true;
  if (!janusBtEverConnected && janusBtFirstReadyMs && now - janusBtFirstReadyMs < JANUS_BT_FIRST_PAIRING_GRACE_MS) return true;

  if (!espNowReady) {
    if (!janusBtEverConnected && ESP.getFreeHeap() < JANUS_BT_PAIRING_HEAP_GUARD) return true;
    if (ESP.getFreeHeap() < 36000) return true;
  }
  return false;
#endif
}


bool janusMinePaletteAllowed() {
#if JANUS_ENABLE_COLONY_ESPNOW
  return janusExclusiveSwarmMode || espNowReady || jobReady;
#else
  return false;
#endif
}

void janusEnterExclusiveSwarmMode(const char* reason) {
#if JANUS_ENABLE_BT_A2DP && JANUS_EXCLUSIVE_SWARM_MODE
  if (janusBtStoppedForSwarm) return;
  if (btConnected || btPlaying || btApprovalPending) return;

  Serial.printf("RADIO MODE -> SWARM | reason=%s | stopping A2DP/ClassicBT heap=%lu\n",
                reason ? reason : "?", (unsigned long)ESP.getFreeHeap());

  janusAudioHardStop("enter_swarm_mode", true);
  if (janusAudioQueue) xQueueReset(janusAudioQueue);

  // Stop discoverability first so a phone cannot attach during radio handoff.
  esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
  delay(80);

  // ESP32-A2DP by pschatzmann exposes end(false). Do not release memory permanently;
  // for BT return we reboot into the clean speaker path rather than trying to hot-rebuild Bluedroid.
  a2dp_sink.end(false);
  delay(220);

  if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_ENABLED) {
    esp_bluedroid_disable();
    delay(180);
  }
  if (esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_ENABLED) {
    esp_bt_controller_disable();
    delay(180);
  }

  btA2dpStarted = false;
  btDiscoverableForced = false;
  btConnected = false;
  btPlaying = false;
  btAclAccepted = false;
  btApprovalPending = false;
  approvalBdaValid = false;
  currentBdaValid = false;
  muted = false;
  updateEffectivePcmGainFromPhone();

  janusBtStoppedForSwarm = true;
  janusExclusiveSwarmMode = true;
  janusExclusiveSwarmEnteredMs = millis();
  currentPalette = PAL_JANUS_MINE;
  Serial.printf("RADIO MODE SWARM READY | bt_off=1 heap=%lu | long press Atom to reboot back to BT speaker\n",
                (unsigned long)ESP.getFreeHeap());
#else
  (void)reason;
#endif
}

void janusReturnToBtByRestart(const char* reason) {
#if JANUS_EXCLUSIVE_SWARM_MODE
  Serial.printf("RADIO MODE -> BT REQUEST | reason=%s | stopping swarm and rebooting into speaker mode\n", reason ? reason : "?");
  stopEspNow("return_to_bt_request");
  jobReady = false;
  janusExclusiveSwarmMode = false;
  janusBtStoppedForSwarm = false;
  // Campfire means: user requested Bluetooth. After reboot the phone can connect normally.
  fillAllRGB(220, 82, 0);
  delay(260);
  fillAllRGB(255, 140, 18);
  delay(360);
  ESP.restart();
#else
  (void)reason;
#endif
}

void initEspNow() {
  if (espNowReady) return;
  // v1.8I6: Bluetooth request/connection wins. Mute is session-safe; mining resumes after disconnect only with enough heap.
  if (janusShouldDeferEspNowForBtPairing()) {
    static uint32_t lastDeferLog = 0;
    uint32_t now = millis();
    if (now - lastDeferLog > 5000) {
      lastDeferLog = now;
      Serial.printf("ESP-NOW START DEFERRED | bt_first_pairing bt=%d play=%d ever=%d heap=%lu\n",
                    btConnected ? 1 : 0, btPlaying ? 1 : 0, janusBtEverConnected ? 1 : 0,
                    (unsigned long)ESP.getFreeHeap());
    }
    return;
  }
  uint32_t startGuard = janusBtStoppedForSwarm ? 16000UL : 36000UL;
  if (ESP.getFreeHeap() < startGuard) {
    Serial.printf("ESP-NOW START DEFERRED | low_heap start_guard=%lu heap=%lu bt_off=%d\n",
                  (unsigned long)startGuard, (unsigned long)ESP.getFreeHeap(), janusBtStoppedForSwarm ? 1 : 0);
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  esp_wifi_set_ps(WIFI_PS_NONE);
  janusForceEspNowChannel(JANUS_ESPNOW_CHANNEL, "init");

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onNowRecv);
  esp_now_register_send_cb(onNowSent);
  // v1.8I4+ FIX: espNowReady must be true before ensurePeer(), otherwise
  // ensurePeer() returns early and broadcast heartbeat/HM packets never leave the Pyramid.
  espNowReady = true;
  ensurePeer(JANUS_BROADCAST_MAC);
  Serial.printf("ESP-NOW READY want_ch=%u home_ch=%u broadcast_peer=0-follow-home\n",
                (unsigned)JANUS_ESPNOW_CHANNEL, (unsigned)janusCurrentWifiChannel());
}

void stopEspNow(const char* reason) {
  if (!espNowReady) return;
  esp_now_deinit();
  // Do not tear WiFi fully down while Classic BT/Bluedroid is alive.
  // On Arduino ESP32 core 3.x this can trip BT fixed_queue asserts after disconnect.
  espNowReady = false;
  nowJobPending = false;
  jobReady = false;
  espNowLastStopMs = millis();
  Serial.printf("ESP-NOW STOP | reason=%s\n", reason ? reason : "?");
}

void manageEspNowLifecycle() {
#if !JANUS_ENABLE_COLONY_ESPNOW
  if (espNowReady) stopEspNow("audio_first_build");
  return;
#else
  uint32_t now = millis();

#if JANUS_EXCLUSIVE_SWARM_MODE
  // If BT is active or approval is pending, colony must be off.
  if (btConnected || btPlaying || btApprovalPending || btAclAccepted) {
    if (espNowReady) stopEspNow("bt_request_or_audio_active");
    return;
  }

  // After the BT-first pairing window, fully stop Classic BT/A2DP and only then start ESP-NOW.
  if (!janusBtStoppedForSwarm) {
    if (now < JANUS_ESPNOW_BOOT_DELAY_MS) return;
    uint32_t btReadyAge = janusBtFirstReadyMs ? (now - janusBtFirstReadyMs) : now;
    if (btReadyAge < JANUS_BT_IDLE_TO_SWARM_MS) {
      if (now - janusLastBtToSwarmLogMs > 7000UL) {
        janusLastBtToSwarmLogMs = now;
        Serial.printf("SWARM WAIT | BT pairing window %lus/%lus heap=%lu\n",
                      (unsigned long)(btReadyAge / 1000UL),
                      (unsigned long)(JANUS_BT_IDLE_TO_SWARM_MS / 1000UL),
                      (unsigned long)ESP.getFreeHeap());
      }
      return;
    }
    janusEnterExclusiveSwarmMode("bt_idle_window_elapsed");
    espNowLastStopMs = 0;
    espNowLastStartTryMs = 0;
  }
#else
  if (espNowReady && ESP.getFreeHeap() < JANUS_ESPNOW_LOW_HEAP_STOP_GUARD) {
    Serial.printf("ESP-NOW STOP | reason=low_heap_guard heap=%lu guard=%u\n",
                  (unsigned long)ESP.getFreeHeap(), (unsigned)JANUS_ESPNOW_LOW_HEAP_STOP_GUARD);
    stopEspNow("low_heap_guard");
    return;
  }
  if (janusShouldDeferEspNowForBtPairing()) {
    if (espNowReady) stopEspNow("bt_request_or_audio_active");
    return;
  }
  if (now < JANUS_ESPNOW_BOOT_DELAY_MS) return;
#endif

  if (espNowReady && ESP.getFreeHeap() < JANUS_ESPNOW_LOW_HEAP_STOP_GUARD) {
    Serial.printf("ESP-NOW STOP | reason=low_heap_guard heap=%lu guard=%u\n",
                  (unsigned long)ESP.getFreeHeap(), (unsigned)JANUS_ESPNOW_LOW_HEAP_STOP_GUARD);
    stopEspNow("low_heap_guard");
    return;
  }

  janusColonyIdleArmed = true;
  janusBtFarmWindow = true;

  if (!janusColonyAutoMineScreenDone) {
    janusColonyAutoMineScreenDone = true;
    currentPalette = PAL_JANUS_MINE;
    Serial.println("MINER ARMED | exclusive SWARM mode; BT stack is stopped, JANUS_MINE now indicates real mining/ESP-NOW");
  }

  if (espNowReady) return;
  if (now - espNowLastStopMs < JANUS_ESPNOW_RESTART_DELAY_MS) return;
  if (now - espNowLastStartTryMs < 3000) return;
  espNowLastStartTryMs = now;
  initEspNow();
#endif
}

void janusEspNowChannelMaintenance() {
#if JANUS_ENABLE_COLONY_ESPNOW
  if (!espNowReady) return;
  if (btConnected || btPlaying || btApprovalPending) return;
  uint32_t now = millis();
  if (now - janusLastChannelFixMs < 5000UL) return;
  janusLastChannelFixMs = now;
  uint8_t ch = janusCurrentWifiChannel();
  if (ch != JANUS_ESPNOW_CHANNEL) {
    janusForceEspNowChannel(JANUS_ESPNOW_CHANNEL, "maintenance");
    if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) esp_now_del_peer(JANUS_BROADCAST_MAC);
    ensurePeer(JANUS_BROADCAST_MAC);
  }
#endif
}

void sendHeartbeat() {
  if (!espNowReady) return;
  JanusColonyPacket p = {};
  memcpy(p.magic, "JANUS", 5);
  snprintf(p.nodeId, sizeof(p.nodeId), "%s", JANUS_DEVICE_NAME);
  snprintf(p.role, sizeof(p.role), "%s", JANUS_ROLE_NAME);
  p.seq = ++colonySeq;
  p.hashRate = currentHashRate;
  p.shares = shares;
  p.rejects = rejects;
  p.bestBits = bestBits;
  p.diff = 0.0f;
  p.targetBits = bestBits;
  p.aiBatch = effectiveBatch();
  p.aiHint = rewardAiHint ? rewardAiHint : localKnnAiHint;
  p.jobAgeMs = jobReady ? (millis() - jobReceivedMs) : 0xFFFFFFFFUL;
  p.rssi = lastRssi;
  p.uptime = millis();

  esp_err_t e = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&p, sizeof(p));
  static uint32_t lastHbErrMs = 0;
  if (e != ESP_OK && millis() - lastHbErrMs > 2500UL) {
    lastHbErrMs = millis();
    Serial.printf("ESP-NOW HEARTBEAT SEND FAIL err=%d home_ch=%u heap=%lu\n",
                  (int)e, (unsigned)janusCurrentWifiChannel(), (unsigned long)ESP.getFreeHeap());
  }
}

void sendEntropy() {
  if (!espNowReady) return;
  EntropyReport er = {};
  er.magic[0] = 'E'; er.magic[1] = 'R';
  er.worker_id = workerId;
  float entropy = (float)(esp_random() & 0xFFFF) / 65535.0f;
  entropy += (float)(touchActivity & 0xFF) / 2048.0f;
  entropy += (float)(lastRssi + 127) / 1024.0f;
  er.local_entropy = entropy;
  er.sensor_flags = 0x02 | 0x08 | 0x10; // touch + bt/audio + rf_field
  er.values[0] = (float)lastRssi;
  er.values[1] = (float)touchActivity;
  er.values[2] = btPlaying ? 2.0f : (btConnected ? 1.0f : 0.0f);
  er.values[3] = (float)effectiveBatch();
  esp_err_t e = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&er, sizeof(er));
  static uint32_t lastErErrMs = 0;
  if (e != ESP_OK && millis() - lastErErrMs > 2500UL) {
    lastErErrMs = millis();
    Serial.printf("ESP-NOW ENTROPY SEND FAIL err=%d home_ch=%u heap=%lu\n",
                  (int)e, (unsigned)janusCurrentWifiChannel(), (unsigned long)ESP.getFreeHeap());
  }
  touchActivity = 0;
}

uint16_t computeEntropyX1000() {
  uint32_t r = esp_random();
  uint32_t mix = r ^ micros() ^ (totalHashes * 2654435761UL) ^ ((uint32_t)(lastRssi + 127) << 8) ^ totalTouchEvents;
  return (uint16_t)(mix % 1000);
}

void updateLoopJitter() {
  uint32_t nowUs = micros();
  if (lastLoopUs == 0) { lastLoopUs = nowUs; return; }
  uint32_t dt = nowUs - lastLoopUs;
  lastLoopUs = nowUs;
  uint16_t jitter = (uint16_t)min<uint32_t>(65535UL, dt > 1000 ? dt - 1000 : 1000 - dt);
  loopJitterUsEma = (uint16_t)((loopJitterUsEma * 7UL + jitter) / 8UL);
  if (jitter > loopMaxUs) loopMaxUs = jitter;
}

void sendHiveMetrics() {
  if (!espNowReady) return;
  HiveMetricPacket hm = {};
  hm.magic[0] = 'H'; hm.magic[1] = 'M';
  hm.version = 2;
  hm.worker_id = workerId;
  snprintf(hm.nodeId, sizeof(hm.nodeId), "%s", JANUS_DEVICE_NAME);
  snprintf(hm.kind, sizeof(hm.kind), "%s", JANUS_ROLE_NAME);
  hm.seq = ++colonySeq;
  hm.uptime_ms = millis();
  hm.free_heap = ESP.getFreeHeap();
  hm.min_free_heap = ESP.getMinFreeHeap();
  hm.cpu_mhz = ESP.getCpuFreqMHz();
  hm.loop_jitter_us = loopJitterUsEma;
  hm.loop_max_us = loopMaxUs;
  hm.rssi = lastRssi;
  hm.bt_flags = (btConnected ? 0x01 : 0) | (btPlaying ? 0x02 : 0) | (muted ? 0x04 : 0) | (btAclAccepted ? 0x08 : 0) | (btApprovalPending ? 0x10 : 0);
  hm.volume = volumeLevel;
  hm.palette = (uint8_t)currentPalette;
  hm.touch_count = (uint16_t)min<uint32_t>(65535UL, totalTouchEvents);
  hm.effective_batch = effectiveBatch();
  hm.hash_rate = currentHashRate;
  hm.total_hashes = totalHashes;
  hm.shares = shares;
  hm.rejects = rejects;
  hm.best_bits = bestBits;
  hm.job_age_ms = jobReady ? (millis() - jobReceivedMs) : 0xFFFFFFFFUL;
  hm.nonce_remaining = (jobReady && nonceEnd >= nonceCursor) ? (nonceEnd - nonceCursor) : 0;
  hm.reward_level = rewardLevel;
  hm.ai_hint = rewardAiHint ? rewardAiHint : localKnnAiHint;
  hm.target_batch = janusDynamicBatch ? janusDynamicBatch : (rewardTargetBatch ? rewardTargetBatch : localKnnTargetBatch);
  hm.prediction_error_x1000 = (int16_t)max<int>(-32768, min<int>(32767, (int)(janusPredictionErrorLocal * 1000.0f)));
  hm.entropy_x1000 = computeEntropyX1000();
  hm.random_tail = (uint16_t)(esp_random() & 0xFFFF);
  hm.reserved = (uint16_t)janusThermalLoad | (JANUS_PYRAMID_GROVE_STICK_LINK ? 0x0100 : 0x0000);
  esp_err_t e = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&hm, sizeof(hm));
  static uint32_t lastHmErrMs = 0;
  if (e != ESP_OK && millis() - lastHmErrMs > 2500UL) {
    lastHmErrMs = millis();
    Serial.printf("ESP-NOW HIVE SEND FAIL err=%d home_ch=%u heap=%lu\n",
                  (int)e, (unsigned)janusCurrentWifiChannel(), (unsigned long)ESP.getFreeHeap());
  }
  loopMaxUs = 0;
}

void sendSwarmSense() {
  if (!espNowReady) return;
  SwarmSensePacket ss = {};
  ss.magic[0] = 'S'; ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = workerId;
  snprintf(ss.nodeId, sizeof(ss.nodeId), "%s", JANUS_DEVICE_NAME);
  snprintf(ss.kind, sizeof(ss.kind), "%s", JANUS_ROLE_NAME);
  ss.seq = ++colonySeq;
  ss.uptime_ms = millis();
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = loopJitterUsEma;
  ss.loop_max_us = loopMaxUs;
  ss.rssi = lastRssi;
  ss.radio_mode = (janusExclusiveSwarmMode || janusBtStoppedForSwarm) ? 1 : 0;
  ss.bt_flags = (btConnected ? 0x01 : 0) | (btPlaying ? 0x02 : 0) | (muted ? 0x04 : 0) | (btAclAccepted ? 0x08 : 0) | (btApprovalPending ? 0x10 : 0);
  ss.palette = (uint8_t)currentPalette;
  ss.knn_label = (uint8_t)localKnnLabel;
  ss.knn_confidence = localKnnConfidence;
  ss.ai_hint = rewardAiHint ? rewardAiHint : localKnnAiHint;
  ss.thermal_load = janusThermalLoad;
  ss.effective_batch = effectiveBatch();
  ss.dynamic_batch = janusDynamicBatch;
  ss.hash_rate = currentHashRate;
  ss.total_hashes = totalHashes;
  ss.best_bits = (uint16_t)min<uint32_t>(65535UL, bestBits);
  ss.hash_eff_x1000 = (uint16_t)min<uint32_t>(65535UL, (uint32_t)((currentHashRate * 1000UL) / max<uint16_t>(1, effectiveBatch())));
  ss.prediction_error_x1000 = (int16_t)max<int>(-32768, min<int>(32767, (int)(janusPredictionErrorLocal * 1000.0f)));
  ss.entropy_x1000 = computeEntropyX1000();
  ss.touch_delta = (uint16_t)min<uint32_t>(65535UL, touchActivity);
  ss.job_age_s = jobReady ? (uint16_t)min<uint32_t>(65535UL, (millis() - jobReceivedMs) / 1000UL) : 0xFFFF;
  ss.nonce_remaining_l16 = (jobReady && nonceEnd >= nonceCursor) ? (uint16_t)((nonceEnd - nonceCursor) & 0xFFFF) : 0;
  ss.flags = 0;
  ss.flags |= 0x0001; // touch present
  ss.flags |= 0x0002; // RF/RSSI present
  ss.flags |= 0x0004; // clock drift observable via uptime/micros tail
  ss.flags |= 0x0008; // heap/jitter thermal proxy present
  ss.flags |= 0x0010; // audio/BT state present, even if BT is exclusive
  ss.flags |= 0x0020; // physical Grove/Stick companion sentinel
  if (janusPyramidPolicyRx) ss.flags |= 0x0040; // Home Cortex policy has been heard
  esp_err_t e = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&ss, sizeof(ss));
  static uint32_t lastSsErrMs = 0;
  if (e != ESP_OK && millis() - lastSsErrMs > 2500UL) {
    lastSsErrMs = millis();
    Serial.printf("ESP-NOW SWARMSENSE SEND FAIL err=%d home_ch=%u heap=%lu\n",
                  (int)e, (unsigned)janusCurrentWifiChannel(), (unsigned long)ESP.getFreeHeap());
  }
}


// =====================================================
// A2DP PCM BRIDGE -> PYRAMID I2S OWNER
// =====================================================

static inline uint8_t pcmTo8(uint32_t v, uint8_t shift) {
  v >>= shift;
  return (uint8_t)(v > 255 ? 255 : v);
}

static inline int16_t softLimit16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

void janusA2dpPcmCallback(const uint8_t *data, uint32_t len) {
  if (!data || len < 4 || janusAudioQueue == nullptr) return;

  janusPcmPackets++;
  janusPcmBytes += len;

  const int16_t *s16 = reinterpret_cast<const int16_t*>(data);
  uint32_t stereoFrames = len / 4;  // L16 + R16
  uint32_t pos = 0;
  int16_t prev = 0;
  uint32_t sumAbs = 0, sumLow = 0, sumDiff = 0;
  uint32_t count = 0;

  while (pos < stereoFrames) {
    JanusAudioChunk chunk;
    uint16_t n = (uint16_t)min<uint32_t>(JANUS_AUDIO_CHUNK_FRAMES, stereoFrames - pos);
    chunk.frames = n;

    for (uint16_t i = 0; i < n; ++i) {
      int16_t l = s16[(pos + i) * 2 + 0];
      int16_t r = s16[(pos + i) * 2 + 1];
      int32_t mono32 = ((int32_t)l + (int32_t)r) / 2;
      // v1.1: gentle software preamp. This raises perceived loudness without touching A2DP transport volume.
      int32_t boosted = (mono32 * (int32_t)janusPcmGainX100) / 100;
      int16_t mono = softLimit16(boosted);
      chunk.mono[i] = mono;
      uint32_t a = (uint32_t)abs((int)mono);
      sumAbs += a;
      sumLow += (a > 400 ? a : 0);
      sumDiff += (uint32_t)abs((int)mono - (int)prev);
      prev = mono;
      count++;
    }

    if (btConnected && btPlaying && btAclAccepted && !muted && janusPcmGainX100 > 0) {
      if (xQueueSend(janusAudioQueue, &chunk, 0) != pdTRUE) {
        janusAudioDropChunks++;
      }
    }
    pos += n;
  }

  if (count) {
    uint32_t avg = sumAbs / count;
    uint32_t low = sumLow / count;
    uint32_t dif = sumDiff / count;
    janusPcmEnergy = pcmTo8(avg, 5);
    janusPcmBass = pcmTo8(low, 5);
    janusPcmTreble = pcmTo8(dif, 5);
    uint32_t mid = (avg > (dif / 2)) ? (avg - dif / 2) : avg / 2;
    janusPcmMid = pcmTo8(mid, 5);
  }
}

void janusAudioPlaybackTask(void *arg) {
  (void)arg;
  JanusAudioChunk chunk;
  for (;;) {
    if (xQueueReceive(janusAudioQueue, &chunk, pdMS_TO_TICKS(100)) == pdTRUE) {
      if (pyramidReady && btConnected && btPlaying && btAclAccepted && !muted && janusPcmGainX100 > 0 && chunk.frames > 0) {
        ep.write(chunk.mono, chunk.frames);
        janusAudioWrittenChunks++;
      }
    }
    taskYIELD();
  }
}

void initJanusAudioBridge() {
  if (janusAudioQueue == nullptr) {
    janusAudioQueue = xQueueCreate(JANUS_AUDIO_QUEUE_LEN, sizeof(JanusAudioChunk));
  }
  if (janusAudioTaskHandle == nullptr && janusAudioQueue != nullptr) {
    xTaskCreatePinnedToCore(janusAudioPlaybackTask, "janus_audio", JANUS_AUDIO_TASK_STACK, nullptr, JANUS_AUDIO_TASK_PRIORITY, &janusAudioTaskHandle, 1);
  }
  Serial.printf("BT PCM BRIDGE | queue=%p task=%p chunk=%u len=%u\n", janusAudioQueue, janusAudioTaskHandle, JANUS_AUDIO_CHUNK_FRAMES, JANUS_AUDIO_QUEUE_LEN);
}

void janusAudioHardStop(const char* reason, bool muteCodec) {
  // v1.8H2: kill the classic "last buffer scream" after A2DP disconnect/idle.
  // Pyramid I2S/codec can keep the last DMA/PCM residue alive if we only flip btPlaying=false.
  if (janusAudioQueue) xQueueReset(janusAudioQueue);

  janusPcmEnergy = 0;
  janusPcmBass = 0;
  janusPcmMid = 0;
  janusPcmTreble = 0;
  audioEnergy = 0;
  audioBass = 0;
  audioMid = 0;
  audioTreble = 0;

  if (pyramidReady) {
    static int16_t silence[JANUS_AUDIO_CHUNK_FRAMES];
    memset(silence, 0, sizeof(silence));
    // Push several zero blocks so the Pyramid output buffer cannot loop stale samples.
    for (uint8_t i = 0; i < 6; ++i) {
      ep.write(silence, JANUS_AUDIO_CHUNK_FRAMES);
      delay(2);
    }
  }

  if (muteCodec) pyramidCodecMute(true);
  Serial.printf("BT AUDIO HARDSTOP | reason=%s | queue_reset=%d | codec_mute=%d\n",
                reason ? reason : "?", janusAudioQueue ? 1 : 0, muteCodec ? 1 : 0);
}

// =====================================================
// BLUETOOTH APPROVAL GATE / A2DP
// =====================================================

void janusBtRejectCurrent(const char* reason) {
  Serial.printf("BT REJECT SOFT | reason=%s | name=%s | bda=%s\n", reason, currentRemoteName, currentBdaValid ? bdaToString(currentBda).c_str() : "?");
  rejectFlashUntilMs = millis() + 1400;
  btAclAccepted = false;
  btApprovalPending = false;
  approvalBdaValid = false;
  // Audio-safe policy: do NOT disconnect from inside/near BT callbacks.
  // On core 3.x this could trip host_recv_pkt_cb while HCI packets are still arriving.
  pyramidCodecMute(true);
  // Do not force remote/iPhone volume to zero; local gate blocks PCM output.
}

void beginBtApproval(const esp_bd_addr_t bda, const char* name, const char* reason) {
  stopEspNow("bt_approval_request");
  memcpy(approvalBda, bda, ESP_BD_ADDR_LEN);
  approvalBdaValid = true;
  snprintf(approvalRemoteName, sizeof(approvalRemoteName), "%s", (name && name[0]) ? name : "unknown");
  btApprovalPending = true;
  btAclAccepted = false;
  btApprovalUntil = millis() + BT_APPROVAL_TIMEOUT_MS;
  pyramidCodecMute(true);  // phone may connect, but speaker stays silent until approved
  Serial.printf("BT APPROVAL REQUEST | reason=%s | name=%s | bda=%s | press Atom button to approve\n",
                reason, approvalRemoteName, bdaToString(approvalBda).c_str());
}

void approveBtPending() {
  if (!btApprovalPending) return;
  if (approvalBdaValid) {
    saveTrustedPeer(approvalBda, approvalRemoteName);
    memcpy(currentBda, approvalBda, ESP_BD_ADDR_LEN);
    currentBdaValid = true;
    snprintf(currentRemoteName, sizeof(currentRemoteName), "%s", approvalRemoteName);
  } else {
    snprintf(currentRemoteName, sizeof(currentRemoteName), "%s", approvalRemoteName[0] ? approvalRemoteName : "approved_session");
    Serial.println("BT APPROVED SESSION ONLY | remote BDA unavailable from library callback");
  }
  btAclAccepted = true;
  btApprovalPending = false;
  approvalBdaValid = false;
  sharAmberUntilMs = millis() + 450;  // tiny warm acceptance flash
  if (janusAudioQueue) xQueueReset(janusAudioQueue);
  pyramidCodecMute(muted);
  applyVolume();
  Serial.printf("BT APPROVED | name=%s | bda=%s\n", currentRemoteName, currentBdaValid ? bdaToString(currentBda).c_str() : "session-only");
}

void rejectBtPending() {
  if (!btApprovalPending) return;
  btApprovalPending = false;
  if (approvalBdaValid) memcpy(currentBda, approvalBda, ESP_BD_ADDR_LEN);
  currentBdaValid = approvalBdaValid;
  janusBtRejectCurrent("button_reject");
}

void janusBtGapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
  if (event == ESP_BT_GAP_READ_REMOTE_NAME_EVT) {
    if (!param) return;
    const char* nm = (const char*)param->read_rmt_name.rmt_name;
    if (!nm) nm = "";
    snprintf(currentRemoteName, sizeof(currentRemoteName), "%s", nm);
    if (btApprovalPending) snprintf(approvalRemoteName, sizeof(approvalRemoteName), "%s", nm[0] ? nm : "unknown");
    btAclPendingName = false;
    Serial.printf("BT REMOTE NAME | name=%s | pending=%d\n", currentRemoteName, btApprovalPending ? 1 : 0);
  }
}

#if JANUS_ENABLE_BT_A2DP
void btConnectionStateChanged(esp_a2d_connection_state_t state, void* ptr) {
  if (state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
    stopEspNow("bt_connected");
    btLastStateChangeMs = millis();
    janusBtFarmWindow = false;
    btConnected = true;
    janusBtEverConnected = true;
    // v1.8I6: mute is session-local. If the phone reconnects after a mute/disconnect,
    // do not keep Pyramid gain locked at 0 while iPhone volume is back at 127/127.
    if (muted) {
      muted = false;
      Serial.println("DEVICE MUTE AUTO-CLEAR | reason=bt_reconnect");
    }
    btAclAccepted = false;
    currentBdaValid = false;
    currentRemoteName[0] = 0;

    if (ptr) {
      memcpy(currentBda, ptr, ESP_BD_ADDR_LEN);
      currentBdaValid = true;
      char owner[64] = {0};
      if (bdaAlreadyAllowed(currentBda, owner, sizeof(owner))) {
        btAclAccepted = true;
        btApprovalPending = false;
        snprintf(currentRemoteName, sizeof(currentRemoteName), "%s", owner);
        if (janusAudioQueue) xQueueReset(janusAudioQueue);
        pyramidCodecMute(muted);
        applyVolume();
        Serial.printf("BT TRUST OK | owner=%s | bda=%s\n", owner, bdaToString(currentBda).c_str());
      } else {
        beginBtApproval(currentBda, "unknown", "new_device");
        btAclPendingName = true;
        btAclPendingUntil = millis() + 3500;
        esp_bt_gap_read_remote_name(currentBda);
      }
    } else {
#if JANUS_BT_APPROVAL_REQUIRED
      Serial.println("BT connected | BDA unavailable; requesting button approval for session-only trust");
      btApprovalPending = true;
      btApprovalUntil = millis() + BT_APPROVAL_TIMEOUT_MS;
      approvalBdaValid = false;
      snprintf(approvalRemoteName, sizeof(approvalRemoteName), "%s", "session_only");
      pyramidCodecMute(true);
      // Do not touch the phone volume while waiting for approval.
#else
      Serial.println("BT connected | BDA unavailable; auto-approving session for audio smoke-test");
      btAclAccepted = true;
      btApprovalPending = false;
      approvalBdaValid = false;
      snprintf(currentRemoteName, sizeof(currentRemoteName), "%s", "session_auto");
      pyramidCodecMute(muted);
      applyVolume();
#endif
    }
  } else if (state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
    btLastDisconnectMs = millis();
    btLastStateChangeMs = btLastDisconnectMs;
    janusBtFarmWindow = false;
    janusColonyIdleArmed = false;
    janusColonyAutoMineScreenDone = false;
    janusLastPlaybackActivityMs = btLastDisconnectMs;
    btConnected = false;
    btPlaying = false;
    btAclAccepted = false;
    currentBdaValid = false;
    btAclPendingName = false;
    btApprovalPending = false;
    approvalBdaValid = false;
    janusAudioHardStop("a2dp_disconnected", true);
    if (muted) {
      muted = false;
      updateEffectivePcmGainFromPhone();
      Serial.println("DEVICE MUTE AUTO-CLEAR | reason=bt_disconnected");
    }
    Serial.println("BT DISCONNECTED");
  }
}

void btAudioStateChanged(esp_a2d_audio_state_t state, void* ptr) {
  (void)ptr;
  btPlaying = (state == ESP_A2D_AUDIO_STATE_STARTED);
  if (btPlaying) {
    janusLastPlaybackActivityMs = millis();
    // v1.8I2: A2DP audio owns the radio/CPU. Stop colony to protect sound.
    stopEspNow("a2dp_started");
    if (janusAudioQueue) xQueueReset(janusAudioQueue);
    if (btAclAccepted && !muted) pyramidCodecMute(false);
    updateEffectivePcmGainFromPhone();
    Serial.println("MINER PAUSE | reason=a2dp_started | BT audio priority");
  } else {
    janusLastPlaybackActivityMs = millis();
    janusAudioHardStop("a2dp_audio_idle", false);
  }
  if (btPlaying && !btAclAccepted) {
    pyramidCodecMute(true);
    // Do not touch the phone volume while waiting for approval.
    Serial.println("BT AUDIO BLOCKED UNTIL BUTTON APPROVAL");
  }
  if (!btPlaying) {
    audioEnergy = audioBass = audioMid = audioTreble = 0;
  }
  Serial.printf("BT AUDIO %s\n", btPlaying ? "PLAYING" : "IDLE");
}

#endif


void janusForceBtDiscoverable(const char* reason) {
#if JANUS_ENABLE_BT_A2DP
  esp_err_t nerr = esp_bt_gap_set_device_name(JANUS_BT_NAME);
  esp_err_t serr = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
  btDiscoverableForced = (serr == ESP_OK);
  Serial.printf("BT DISCOVERABLE FORCE | reason=%s | name=%s | name_err=%d | scan_err=%d | bluedroid=%d | controller=%d\n",
                reason ? reason : "?", JANUS_BT_NAME, (int)nerr, (int)serr,
                (int)esp_bluedroid_get_status(), (int)esp_bt_controller_get_status());
#endif
}

void updateBtAdvertiseWatchdog() {
#if JANUS_ENABLE_BT_A2DP
#if JANUS_BT_ADV_WATCHDOG_ENABLE
  if (!btA2dpStarted || btConnected) return;
  if (janusBtFarmWindow || espNowReady) return;
  uint32_t now = millis();
  if (btLastDisconnectMs && (now - btLastDisconnectMs < JANUS_BT_TO_FARM_COOLDOWN_MS + 5000)) return;
  if (ESP.getFreeHeap() < JANUS_BT_LOW_HEAP_GUARD) return;
  if (now - lastBtDiscoverableRefreshMs < 30000) return;
  lastBtDiscoverableRefreshMs = now;
  janusForceBtDiscoverable("watchdog");
#endif
#endif
}

void initBluetoothAudio() {
#if JANUS_ENABLE_BT_A2DP
  Serial.printf("BT A2DP COMPILED | lib=1 | name=%s | heap_before=%lu\n", JANUS_BT_NAME, (unsigned long)ESP.getFreeHeap());
  Serial.printf("BT STATUS BEFORE | bluedroid=%d controller=%d\n", (int)esp_bluedroid_get_status(), (int)esp_bt_controller_get_status());

  // Register GAP before A2DP start so we can see auth/name/pairing events.
  esp_err_t gapErr = esp_bt_gap_register_callback(janusBtGapCallback);
  Serial.printf("BT GAP CALLBACK | err=%d\n", (int)gapErr);

  // v0.8: Pyramid already owns I2S. A2DP must NOT create another I2S channel.
  // We request decoded PCM through stream_reader and write it with ep.write()
  // from janusAudioPlaybackTask.
  initJanusAudioBridge();
  a2dp_sink.set_output_active(false);
  a2dp_sink.set_stream_reader(janusA2dpPcmCallback, false);
  a2dp_sink.set_on_volumechange(janusA2dpVolumeChanged);
  a2dp_sink.set_avrc_rn_volumechange(janusA2dpVolumeChanged);
  a2dp_sink.set_avrc_rn_volumechange_completed(janusA2dpVolumeChanged);
  a2dp_sink.set_max_write_delay_ms(0);
  Serial.printf("BT PCM BRIDGE ACTIVE | bck=%d ws=%d dout=%d rate=%d | no_audiotools_i2s\n",
                EP_I2S_BCLK, EP_I2S_WS, EP_I2S_DOUT, EP_SAMPLE_RATE);

  a2dp_sink.set_on_connection_state_changed(btConnectionStateChanged);
  a2dp_sink.set_on_audio_state_changed(btAudioStateChanged);
  a2dp_sink.set_auto_reconnect(false);

  // Start A2DP first, then explicitly force Classic BT discoverable/connectable.
  // Some core 3.x builds start A2DP but do not remain visible unless GAP scan mode is refreshed.
  a2dp_sink.start(JANUS_BT_NAME);
  btA2dpStarted = true;
  delay(350);
  janusForceBtDiscoverable("after_a2dp_start");
  janusBtFirstReadyMs = millis();
  applyVolume();
  Serial.printf("BT A2DP READY | visible_name=%s | heap_after=%lu | bt_first_grace=%lus\n", JANUS_BT_NAME, (unsigned long)ESP.getFreeHeap(), (unsigned long)(JANUS_BT_FIRST_PAIRING_GRACE_MS / 1000UL));
#else
  Serial.println("BT A2DP DISABLED BY MACRO | BluetoothA2DPSink.h was not compiled in. Use full ESP32-A2DP library or flash BT-only test.");
#endif
}


void updateBtAclTimeout() {
  if (btAclPendingName && millis() > btAclPendingUntil) {
    btAclPendingName = false;
    Serial.println("BT NAME REQUEST TIMEOUT; button approval can still continue");
  }
  if (btApprovalPending && millis() > btApprovalUntil) {
    janusBtRejectCurrent("approval_timeout");
  }
}

// =====================================================
// TOUCH / BUTTON
// =====================================================

void noteTouchGesture(const char* name) {
  touchActivity++;
  totalTouchEvents++;
  lastTouchMs = millis();
  if (name && name[0]) Serial.printf("GESTURE %s\n", name);
}

void resetSwipeIfReleased(TouchSwipeState &s, bool lower, bool upper) {
  if (!lower && !upper) {
    s.active = false;
    s.firstPad = 0;
    s.startMs = 0;
    s.fired = false;
  }
}

void processTwoPadSwipe(TouchSwipeState &s, bool lower, bool upper, const char* upName, const char* downName,
                        void (*onUp)(), void (*onDown)()) {
  uint32_t now = millis();

  if (!lower && !upper) {
    resetSwipeIfReleased(s, lower, upper);
    return;
  }

  if (!s.active) {
    s.active = true;
    s.startMs = now;
    s.fired = false;
    if (lower && !upper) s.firstPad = 1;       // lower -> upper = swipe up
    else if (upper && !lower) s.firstPad = 2;  // upper -> lower = swipe down
    else s.firstPad = 3;                       // both at once: ignore until release, avoids false skips
    return;
  }

  if (s.fired) return;
  uint32_t age = now - s.startMs;
  if (age > TOUCH_SWIPE_MAX_MS) { s.fired = true; return; }
  if (age < TOUCH_SWIPE_MIN_MS) return;
  if (now - lastTouchMs < TOUCH_GESTURE_COOLDOWN_MS) return;

  if (s.firstPad == 1 && upper) {
    noteTouchGesture(upName);
    onUp();
    s.fired = true;
  } else if (s.firstPad == 2 && lower) {
    noteTouchGesture(downName);
    onDown();
    s.fired = true;
  }
}

void handleBrightnessHoldsOnVolumeSide(bool lower, bool upper) {
  uint32_t now = millis();

  // v1.3 requested inversion:
  //   hold lower TP3 = brightness UP
  //   hold upper TP4 = brightness DOWN
  if (lower && !upper) {
    if (!touch3StartMs) {
      touch3StartMs = now;
      touch3LastRepeatMs = 0;
      touch3HoldDone = false;
    }
    if ((now - touch3StartMs >= BRIGHTNESS_HOLD_MS) &&
        (!touch3LastRepeatMs || now - touch3LastRepeatMs >= BRIGHTNESS_REPEAT_MS)) {
      touch3LastRepeatMs = now;
      touch3HoldDone = true;
      phoneVolumeSwipe.fired = true;
      noteTouchGesture("BRIGHTNESS_UP_HOLD");
      brightnessUp();
    }
  } else {
    touch3StartMs = 0;
    touch3LastRepeatMs = 0;
    touch3HoldDone = false;
  }

  if (upper && !lower) {
    if (!touch4StartMs) {
      touch4StartMs = now;
      touch4LastRepeatMs = 0;
      touch4HoldDone = false;
    }
    if ((now - touch4StartMs >= BRIGHTNESS_HOLD_MS) &&
        (!touch4LastRepeatMs || now - touch4LastRepeatMs >= BRIGHTNESS_REPEAT_MS)) {
      touch4LastRepeatMs = now;
      touch4HoldDone = true;
      phoneVolumeSwipe.fired = true;
      noteTouchGesture("BRIGHTNESS_DOWN_HOLD");
      brightnessDown();
    }
  } else {
    touch4StartMs = 0;
    touch4LastRepeatMs = 0;
    touch4HoldDone = false;
  }
}

void handleTouch() {
  // Physical map from your photo:
  //   Right side / CH1 / track side: lower TP1, upper TP2.
  //     swipe up   TP1 -> TP2 = next track
  //     swipe down TP2 -> TP1 = previous track
  //   Left side / CH2 / volume side: lower TP3, upper TP4.
  //     swipe up   TP3 -> TP4 = local Pyramid volume up
  //     swipe down TP4 -> TP3 = local Pyramid volume down
  //     v1.3 inverted brightness holds: hold TP3 = brightness up, hold TP4 = brightness down.
  updateBrightnessTouchMode();

  bool t1 = pyramidCtrlIsPressed(1);  // right/lower
  bool t2 = pyramidCtrlIsPressed(2);  // right/upper
  bool t3 = pyramidCtrlIsPressed(3);  // left/lower
  bool t4 = pyramidCtrlIsPressed(4);  // left/upper

  if (t1 || t2 || t3 || t4) janusLastUserInteractionMs = millis();

  processTwoPadSwipe(trackSwipe, t1, t2, "TRACK_NEXT_SWIPE_UP", "TRACK_PREV_SWIPE_DOWN", trackNext, trackPrev);
  processTwoPadSwipe(phoneVolumeSwipe, t3, t4, "PHONE_VOLUME_DOWN_SWIPE", "PHONE_VOLUME_UP_SWIPE", phoneVolumeDown, phoneVolumeUp);
  handleBrightnessHoldsOnVolumeSide(t3, t4);
}

void handleButton() {
  M5.update();
  bool down = M5.BtnA.isPressed();
  uint32_t now = millis();

  if (down && !buttonWasDown) {
    janusLastUserInteractionMs = now;
    buttonDownMs = now;
    buttonWasDown = true;
  }

  if (!down && buttonWasDown) {
    janusLastUserInteractionMs = now;
    uint32_t held = now - buttonDownMs;
    buttonWasDown = false;

    if (btApprovalPending) {
      if (held >= BUTTON_LONG_MS) rejectBtPending();
      else approveBtPending();
      return;
    }

    if (held >= BUTTON_LONG_MS) {
      if (janusExclusiveSwarmMode || janusBtStoppedForSwarm) {
        janusReturnToBtByRestart("atom_long_press_from_swarm");
        return;
      }
      setMuted(!muted);
    } else {
      nextPalette();
    }
  }
}

void bootAclClearCheck() {
  if (bootAclClearDone) return;
  M5.update();
  if (M5.BtnA.isPressed()) {
    uint32_t start = millis();
    while (M5.BtnA.isPressed() && millis() - start < BUTTON_CLEAR_ACL_BOOT_MS) {
      fillAllRGB(60, 0, 80);
      M5.update();
      delay(20);
    }
    if (millis() - start >= BUTTON_CLEAR_ACL_BOOT_MS) clearBtAcl();
  }
  bootAclClearDone = true;
}

// =====================================================
// LED PALETTES
// =====================================================

static inline uint8_t atomRotIndex(uint8_t i) {
  // v1.7: rotate Atom Matrix 180° because the device is used with USB/power port as the top side.
  return (uint8_t)(ATOM_MATRIX_LED_COUNT - 1 - i);
}

static inline void atomSetPixel(uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
  if (i >= ATOM_MATRIX_LED_COUNT) return;
  atomMatrix.setPixelColor(atomRotIndex(i), atomMatrix.Color(r, g, b));
}

void atomSetXY(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
  if (x >= 5 || y >= 5) return;
  uint8_t rx = 4 - x;
  uint8_t ry = 4 - y;
  atomMatrix.setPixelColor(ry * 5 + rx, atomMatrix.Color(r, g, b));
}

void pyramidSetSidePixel(uint8_t side, uint8_t i, uint8_t r, uint8_t g, uint8_t b) {
  if (i >= PYRAMID_LED_COUNT) return;
  pyramidCtrlSetRGB(side, i, r, g, b);
}

void drawRainbow() {
  hueOffset += 0.006f;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t r, g, b;
    hsv2rgb(hueOffset + (float)i / PYRAMID_LED_COUNT, 0.95f, 0.65f, r, g, b);
    pyramidCtrlSetRGB(1, i, r, g, b);
    pyramidCtrlSetRGB(2, i, r, g, b);
  }
  for (int i = 0; i < ATOM_MATRIX_LED_COUNT; ++i) {
    uint8_t r, g, b;
    hsv2rgb(hueOffset + (float)i / ATOM_MATRIX_LED_COUNT, 0.95f, 0.35f, r, g, b);
    atomMatrix.setPixelColor(atomRotIndex(i), atomMatrix.Color(r, g, b));
  }
  atomMatrix.show();
}

void drawOcean() {
  uint8_t v = (uint8_t)(60 + 40 * (sinf(millis() * 0.0025f) + 1.0f));
  fillAllRGB(0, audioScale(v, 90), audioScale(clamp8(v + 40), 90));
}

void drawFire() {
  uint8_t phase = (millis() >> 5) & 0xFF;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t heat = 80 + ((i * 23 + phase * 7 + (esp_random() & 0x1F)) & 0x7F);
    pyramidCtrlSetRGB(1, i, heat, heat / 3, 0);
    pyramidCtrlSetRGB(2, i, heat, heat / 4, 0);
  }
  for (int i = 0; i < ATOM_MATRIX_LED_COUNT; ++i) {
    uint8_t heat = 50 + ((i * 13 + phase * 5) & 0x5F);
    atomMatrix.setPixelColor(atomRotIndex(i), atomMatrix.Color(heat, heat / 3, 0));
  }
  atomMatrix.show();
}

void drawMatrixRain() {
  // Five falling columns on Atom Matrix, mirrored as flowing code streams on Pyramid sides.
  uint8_t eqBoost = btPlaying ? (uint8_t)(audioTreble / 3) : 0;
  uint32_t tick = millis() / 75;
  for (uint8_t x = 0; x < 5; ++x) {
    uint8_t head = (matrixDrops[x] + tick / matrixDropSpeed[x]) % 7;
    for (uint8_t y = 0; y < 5; ++y) {
      int d = (int)head - (int)y;
      if (d < 0) d += 7;
      uint8_t g = clamp8(((d == 0) ? 190 : (d == 1 ? 95 : (d == 2 ? 34 : 5))) + eqBoost);
      uint8_t b = clamp8(((d == 0) ? 90 : (d == 1 ? 28 : 4)) + audioMid / 5);
      atomSetXY(x, y, 0, g, b);
    }
  }
  atomMatrix.show();

  uint8_t phase = (millis() >> 4) & 0xFF;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t flowA = (i * 19 + phase) & 0x3F;
    uint8_t flowB = (i * 23 + 63 - phase) & 0x3F;
    uint8_t g1 = (flowA < 5) ? 190 : (flowA < 12 ? 70 : 12);
    uint8_t g2 = (flowB < 5) ? 170 : (flowB < 12 ? 60 : 10);
    pyramidCtrlSetRGB(1, i, 0, g1, g1 / 3);
    pyramidCtrlSetRGB(2, i, 0, g2, g2 / 3);
  }
}

void drawAurora() {
  float t = millis() * 0.0014f;
  uint8_t eq = btPlaying ? (uint8_t)(audioEnergy / 4) : 0;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    float w = (sinf(t + i * 0.63f) + 1.0f) * 0.5f;
    uint8_t r = (uint8_t)(12 + 24 * w);
    uint8_t g = clamp8((int)(45 + 100 * w) + eq);
    uint8_t b = clamp8((int)(65 + 145 * (1.0f - w * 0.35f)) + audioTreble / 6);
    pyramidCtrlSetRGB(1, i, r, g, b);
    pyramidCtrlSetRGB(2, i, b / 5, g, b);
  }
  for (int i = 0; i < ATOM_MATRIX_LED_COUNT; ++i) {
    float w = (sinf(t + i * 0.39f) + 1.0f) * 0.5f;
    atomMatrix.setPixelColor(atomRotIndex(i), atomMatrix.Color((uint8_t)(5 + 18*w), (uint8_t)(30 + 70*w), (uint8_t)(55 + 95*w)));
  }
  atomMatrix.show();
}

void drawRfShadow() {
  // RSSI-driven palette: strong link = calm blue/green, weak link = violet/red shadows.
  int rssi = lastRssi <= -120 ? -92 : lastRssi;
  uint8_t quality = clamp8(map(rssi, -95, -35, 0, 180));
  uint8_t shadow = 180 - quality;
  uint32_t phase = millis() / 90;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t sparkle = ((i * 13 + phase) & 0x1F) == 0 ? 45 : 0;
    pyramidCtrlSetRGB(1, i, shadow / 2 + sparkle, quality, 80 + quality / 2);
    pyramidCtrlSetRGB(2, i, shadow / 2, quality / 2 + sparkle, 120 + quality / 3);
  }
  for (int i = 0; i < ATOM_MATRIX_LED_COUNT; ++i) {
    bool edge = (i < 5 || i >= 20 || i % 5 == 0 || i % 5 == 4);
    atomMatrix.setPixelColor(atomRotIndex(i), atomMatrix.Color(edge ? shadow : shadow / 3, edge ? quality / 2 : quality, 90 + quality / 3));
  }
  atomMatrix.show();
}

void drawColonyHeart() {
  float beat = (sinf(millis() * 0.0042f) + 1.0f) * 0.5f;
  uint8_t v = (uint8_t)(28 + 110 * beat);
  uint8_t gold = (uint8_t)(20 + 50 * beat);
  if (jobReady) v = clamp8(v + 35);
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t wave = (uint8_t)(v * (0.45f + 0.55f * ((sinf(millis()*0.003f + i*0.7f)+1.0f)*0.5f)));
    pyramidCtrlSetRGB(1, i, gold, wave, wave);
    pyramidCtrlSetRGB(2, i, gold / 2, wave, wave + 10);
  }
  int heartPixels[] = {2,6,7,8,10,11,12,13,14,16,17,18,22};
  atomFillRGB(0, 0, 0);
  for (uint8_t k=0; k<sizeof(heartPixels)/sizeof(heartPixels[0]); ++k) {
    atomMatrix.setPixelColor(atomRotIndex(heartPixels[k]), atomMatrix.Color(gold, v, v));
  }
  atomMatrix.show();
}

void drawPrismWarp() {
  hueOffset += 0.011f;
  float twist = sinf(millis() * 0.0017f) * 0.18f;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t r,g,b;
    hsv2rgb(hueOffset + twist + i * 0.055f, 0.90f, 0.70f, r, g, b);
    pyramidCtrlSetRGB(1, i, r, g, b);
    hsv2rgb(hueOffset - twist + i * 0.047f + 0.33f, 0.88f, 0.65f, r, g, b);
    pyramidCtrlSetRGB(2, i, r, g, b);
  }
  for (int i = 0; i < ATOM_MATRIX_LED_COUNT; ++i) {
    uint8_t r,g,b;
    hsv2rgb(hueOffset + (i%5)*0.07f + (i/5)*0.04f, 0.95f, 0.38f, r, g, b);
    atomMatrix.setPixelColor(atomRotIndex(i), atomMatrix.Color(r, g, b));
  }
  atomMatrix.show();
}

void drawDataScanner() {
  uint8_t pos = (millis() / 55) % PYRAMID_LED_COUNT;
  uint8_t load = clamp8(map((int)effectiveBatch(), 0, JANUS_BATCH_ABSOLUTE_MAX, 25, 160));
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t tail = (i == pos) ? 220 : ((i + 1 == pos || i + 2 == pos) ? 80 : 12);
    pyramidCtrlSetRGB(1, i, 0, tail, load);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, 0, tail / 2, load + tail / 3);
  }
  for (uint8_t y = 0; y < 5; ++y) {
    for (uint8_t x = 0; x < 5; ++x) {
      uint8_t scan = (x == (millis()/120)%5) ? 150 : 12;
      atomSetXY(x, y, 0, scan, 50 + load/3);
    }
  }
  atomMatrix.show();
}

void drawAmberReactor() {
  float w = (sinf(millis() * 0.005f) + 1.0f) * 0.5f;
  uint8_t base = clamp8((int)(40 + 120 * w) + (btPlaying ? audioBass / 3 : 0));
  if (rewardLevel >= 2) base = clamp8(base + 45);
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t spark = ((esp_random() + i) & 0x3F) == 0 ? 80 : 0;
    pyramidCtrlSetRGB(1, i, base + spark, base/2 + spark/3, 2);
    pyramidCtrlSetRGB(2, i, base, base/3 + spark/2, 0);
  }
  for (int i=0;i<ATOM_MATRIX_LED_COUNT;i++) {
    bool core = (i == 12 || i == 7 || i == 11 || i == 13 || i == 17);
    uint8_t v = core ? clamp8(base + 70) : base/2;
    atomMatrix.setPixelColor(atomRotIndex(i), atomMatrix.Color(v, v/2, 0));
  }
  atomMatrix.show();
}


static inline uint8_t rainStrengthNow() {
  if (btPlaying) return clamp8(50 + audioEnergy / 2);
  return 72;
}

void drawInfinityTunnel() {
  // v1.3: This is the strongest possible "infinity mirror" simulation on a 5x5 Atom Matrix.
  // Real optical depth needs a one-way mirror + reflective cavity. Here we fake it with:
  //   1) bright outer frame,
  //   2) dimmer inner reflected frame,
  //   3) fully black vanishing center,
  //   4) moving corner/edge glints that travel as if reflections are receding.
  uint32_t now = millis();
  uint8_t eq = btPlaying ? (uint8_t)(audioEnergy / 3) : 22;
  uint8_t edgePhase = (now / 82) % 16;
  uint8_t depthPulse = (uint8_t)(25 + 25 * ((sinf(now * 0.0032f) + 1.0f) * 0.5f));

  atomFillRGB(0, 0, 0);

  auto edgeIndex = [](uint8_t x, uint8_t y) -> int {
    if (y == 0) return x;
    if (x == 4) return 4 + y;
    if (y == 4) return 8 + (4 - x);
    if (x == 0) return 12 + (4 - y);
    return -1;
  };

  for (uint8_t y = 0; y < 5; ++y) {
    for (uint8_t x = 0; x < 5; ++x) {
      if (x == 2 && y == 2) { atomSetXY(x, y, 0, 0, 0); continue; }

      int ei = edgeIndex(x, y);
      if (ei >= 0) {
        int dist = abs((int)ei - (int)edgePhase);
        dist = min(dist, 16 - dist);
        uint8_t glint = (dist == 0) ? 165 : (dist == 1 ? 92 : (dist == 2 ? 36 : 0));
        uint8_t v = clamp8(54 + eq + depthPulse / 2 + glint);
        // colder blue outside, little green in the glow
        atomSetXY(x, y, 0, clamp8(v / 2), v);
      } else {
        // inner frame: 8 pixels around the black center, much dimmer and phase-shifted.
        uint8_t innerWave = (uint8_t)(18 + 32 * ((sinf(now * 0.0045f + x * 1.7f + y * 0.9f) + 1.0f) * 0.5f));
        uint8_t v = clamp8(innerWave + eq / 2);
        // corners of the inner square are slightly brighter to mimic repeated reflections.
        bool innerCorner = (x != 2 && y != 2);
        if (innerCorner) v = clamp8(v + 18);
        atomSetXY(x, y, 0, clamp8(v / 3), v);
      }
    }
  }
  atomMatrix.show();

  // Pyramid CH1/CH2: use your visible corner indices 0, 6, 7, 13 as "mirror frame corners".
  // The rest becomes receding rails so the Pyramid body matches the Atom center illusion.
  uint8_t rail = (now / 70) % PYRAMID_LED_COUNT;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    int d1 = abs(i - rail);
    d1 = min(d1, PYRAMID_LED_COUNT - d1);
    bool corner = (i == 0 || i == 6 || i == 7 || i == 13);
    uint8_t pulse = clamp8((d1 == 0 ? 150 : (d1 == 1 ? 72 : (d1 == 2 ? 28 : 0))) + eq);
    uint8_t base = corner ? clamp8(60 + eq + depthPulse) : clamp8(8 + eq / 3);
    uint8_t v = clamp8(base + pulse);

    pyramidCtrlSetRGB(1, i, 0, clamp8(v / 2), v);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, 0, clamp8(v / 2), v);
  }
}


void drawSaberRain(uint8_t saberR, uint8_t saberG, uint8_t saberB) {
  uint32_t now = millis();
  uint8_t rain = rainStrengthNow();
  uint8_t moderate = btPlaying ? (uint8_t)max(18, (int)(audioEnergy / 6)) : 24;
  // Pyramid: mirrored saber glow with rainfall sparks.
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    float glowF = 0.45f + 0.55f * ((sinf(now*0.0034f + i*0.7f)+1.0f)*0.5f);
    uint8_t coreR = clamp8((int)(saberR * glowF) + moderate);
    uint8_t coreG = clamp8((int)(saberG * glowF) + moderate);
    uint8_t coreB = clamp8((int)(saberB * glowF) + moderate);
    bool spark = ((esp_random() + i + (now/30)) % max(6, 32 - rain/6)) == 0;
    if (spark) {
      coreR = clamp8(coreR + 120);
      coreG = clamp8(coreG + 120);
      coreB = clamp8(coreB + 120);
    }
    pyramidCtrlSetRGB(1, i, coreR, coreG, coreB);
    pyramidCtrlSetRGB(2, i, coreR, coreG, coreB);
  }

  // Matrix: center-column saber with depth glow and rain impacts.
  for (uint8_t y = 0; y < 5; ++y) {
    for (uint8_t x = 0; x < 5; ++x) {
      int dx = abs((int)x - 2);
      uint8_t glow = (dx == 0) ? 255 : (dx == 1 ? 92 : 18);
      uint8_t r = clamp8((saberR * glow) / 255);
      uint8_t g = clamp8((saberG * glow) / 255);
      uint8_t b = clamp8((saberB * glow) / 255);
      // Rain falling diagonally, brighter splash when it intersects blade.
      bool drop = (((int)(now / 85) + x * 3 + y * 5) % 7) == 0;
      bool impact = drop && dx <= 1;
      if (impact) {
        r = clamp8(r + 140); g = clamp8(g + 140); b = clamp8(b + 140);
      } else if (drop) {
        r = clamp8(r + 18); g = clamp8(g + 18); b = clamp8(b + 30);
      }
      atomSetXY(x, y, r, g, b);
    }
  }
  atomMatrix.show();
}


void snakeReset() {
  snakeLen = 3;
  snakeBody[0] = 12; snakeBody[1] = 11; snakeBody[2] = 10;
  snakeFood = (uint8_t)(esp_random() % 25);
  snakeDir = 1;
  snakeScore = 0;
  snakeSteps = 0;
  snakeReady = true;
}

bool snakeContains(uint8_t cell, uint8_t upto) {
  for (uint8_t i = 0; i < upto && i < snakeLen; ++i) if (snakeBody[i] == cell) return true;
  return false;
}

int snakeManhattan(uint8_t a, uint8_t b) {
  int ax = a % 5, ay = a / 5, bx = b % 5, by = b / 5;
  return abs(ax - bx) + abs(ay - by);
}

uint8_t snakeNextCell(uint8_t head, uint8_t dir) {
  int x = head % 5, y = head / 5;
  if (dir == 0) y--; else if (dir == 1) x++; else if (dir == 2) y++; else x--;
  if (x < 0 || x > 4 || y < 0 || y > 4) return 255;
  return (uint8_t)(y * 5 + x);
}

void drawSnakeAI() {
  uint32_t now = millis();
  if (!snakeReady) snakeReset();
  uint16_t stepMs = btPlaying ? max<uint16_t>(110, 280 - audioEnergy) : 320;
  if (now - snakeLastStepMs >= stepMs) {
    snakeLastStepMs = now;
    uint8_t head = snakeBody[0];
    uint8_t bestDir = snakeDir;
    int bestScoreLocal = 999;
    uint8_t explore = (uint8_t)max<int>(3, 30 - snakeBest * 2);
    for (uint8_t d = 0; d < 4; ++d) {
      uint8_t n = snakeNextCell(head, d);
      if (n == 255) continue;
      if (snakeContains(n, snakeLen - 1)) continue;
      int score = snakeManhattan(n, snakeFood) * 8;
      if (d == snakeDir) score -= 2;
      score += (int)(esp_random() % explore);
      if (score < bestScoreLocal) { bestScoreLocal = score; bestDir = d; }
    }
    uint8_t next = snakeNextCell(head, bestDir);
    if (next == 255 || snakeContains(next, snakeLen - 1)) {
      if (snakeScore > snakeBest) snakeBest = snakeScore;
      snakeReset();
    } else {
      snakeDir = bestDir;
      bool ate = (next == snakeFood);
      uint8_t oldLen = snakeLen;
      if (ate && snakeLen < 22) snakeLen++;
      for (int i = snakeLen - 1; i > 0; --i) snakeBody[i] = snakeBody[i - 1];
      snakeBody[0] = next;
      if (ate) {
        snakeScore++;
        if (snakeScore > snakeBest) snakeBest = snakeScore;
        do { snakeFood = (uint8_t)(esp_random() % 25); } while (snakeContains(snakeFood, snakeLen));
      } else {
        snakeLen = oldLen;
      }
      snakeSteps++;
    }
  }

  uint8_t glow = audioAdd(65, 2);
  atomFillRGB(0, 0, 0);
  atomMatrix.setPixelColor(atomRotIndex(snakeFood), atomMatrix.Color(255, 120, 20));
  for (uint8_t i = 0; i < snakeLen; ++i) {
    uint8_t v = clamp8(glow + (snakeLen - i) * 5);
    uint8_t b = (i == 0) ? clamp8(v + 60) : v / 2;
    atomMatrix.setPixelColor(atomRotIndex(snakeBody[i]), atomMatrix.Color(0, v, b));
  }
  atomMatrix.show();

  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t wave = clamp8(12 + ((i * 9 + now / 12) & 0x3F) + audioEnergy / 3);
    bool headSpark = (i == (snakeBody[0] % PYRAMID_LED_COUNT));
    pyramidCtrlSetRGB(1, i, headSpark ? 255 : 0, wave, headSpark ? 40 : wave / 2);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, 0, wave / 2, wave);
  }
}

void drawSpaceMoon() {
  uint32_t now = millis();
  uint8_t twinkle = audioGate(90);
  atomFillRGB(0, 0, 4);
  // Moon on Atom: bright center/crescent, dark sky around it.
  atomSetXY(2, 2, 130, 150, 170);
  atomSetXY(1, 2, 35, 50, 70);
  atomSetXY(2, 1, 70, 90, 110);
  atomSetXY(3, 2, 220, 220, 200);
  atomSetXY(2, 3, 90, 110, 130);
  if ((now / 450) & 1) atomSetXY(0, 0, 20, 30, clamp8(90 + twinkle));
  if ((now / 700) & 1) atomSetXY(4, 4, 30, 20, clamp8(80 + twinkle));
  atomMatrix.show();

  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint32_t r = (esp_random() >> (i % 7));
    uint8_t dust = ((r + now / 60 + i * 19) & 0x3F) < (btPlaying ? 5 + audioEnergy / 20 : 4) ? 150 : 8;
    uint8_t neb = clamp8(14 + audioTreble / 5 + (uint8_t)(18 * ((sinf(now * 0.0015f + i) + 1.0f) * 0.5f)));
    pyramidCtrlSetRGB(1, i, dust / 4, neb / 2, clamp8(neb + dust));
    pyramidCtrlSetRGB(2, i, clamp8(neb / 2 + dust / 5), 0, clamp8(neb + dust / 2));
  }
}

void drawRainDrops() {
  uint32_t now = millis();
  uint8_t density = btPlaying ? clamp8(4 + audioEnergy / 12) : 8;
  atomFillRGB(0, 0, 0);
  for (uint8_t x = 0; x < 5; ++x) {
    uint8_t y = (uint8_t)((now / (95 + x * 17) + x * 2) % 6);
    if (y < 5) atomSetXY(x, y, 20, 60, 150);
    if (y > 0 && y < 5) atomSetXY(x, y - 1, 4, 15, 45);
  }
  atomMatrix.show();
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    bool drop = (((now / 55) + i * 7 + (esp_random() & 0xF)) % max<uint8_t>(3, 16 - density)) == 0;
    uint8_t tail = (uint8_t)(20 + audioMid / 4);
    pyramidCtrlSetRGB(1, i, 0, drop ? 120 : tail / 3, drop ? 255 : tail);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, 0, drop ? 90 : tail / 4, drop ? 220 : tail);
  }
}

void drawTachyonRift() {
  uint32_t now = millis();
  float t = now * 0.004f;
  for (uint8_t y = 0; y < 5; ++y) {
    for (uint8_t x = 0; x < 5; ++x) {
      float d = fabsf((float)x - 2.0f) + fabsf((float)y - 2.0f);
      uint8_t v = clamp8((int)(90 + 90 * sinf(t + d * 1.7f)) + audioEnergy / 2);
      atomSetXY(x, y, v / 2, 0, v);
    }
  }
  atomMatrix.show();
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t v = clamp8(40 + (int)(120 * ((sinf(t + i * 0.65f) + 1.0f) * 0.5f)) + audioTreble / 2);
    pyramidCtrlSetRGB(1, i, v, 0, clamp8(v + 50));
    pyramidCtrlSetRGB(2, i, 0, clamp8(v / 2), clamp8(v + 40));
  }
}

void drawBiolumenReef() {
  uint32_t now = millis();
  uint8_t pulse = audioAdd(40, 2);
  for (uint8_t y = 0; y < 5; ++y) {
    for (uint8_t x = 0; x < 5; ++x) {
      uint8_t v = clamp8(10 + pulse + (uint8_t)(40 * ((sinf(now * 0.002f + x * 0.9f + y) + 1.0f) * 0.5f)));
      atomSetXY(x, y, v / 8, v, v / 2);
    }
  }
  atomMatrix.show();
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t v = clamp8(16 + pulse + (i * 11 + now / 30) % 80);
    pyramidCtrlSetRGB(1, i, v / 10, v, clamp8(v / 2 + 40));
    pyramidCtrlSetRGB(2, i, v / 12, clamp8(v / 2 + 30), v);
  }
}

void drawStarforge() {
  uint32_t now = millis();
  uint8_t core = audioAdd(85, 2);
  atomFillRGB(0, 0, 0);
  const uint8_t ring[] = {6,7,8,11,13,16,17,18};
  for (uint8_t i=0;i<sizeof(ring);++i) atomMatrix.setPixelColor(atomRotIndex(ring[i]), atomMatrix.Color(core, core/4, 0));
  atomMatrix.setPixelColor(atomRotIndex(12), atomMatrix.Color(255, clamp8(140 + audioBass), 30));
  atomMatrix.show();
  for (int i=0;i<PYRAMID_LED_COUNT;i++) {
    uint8_t spark = ((esp_random() + now/20 + i*13) & 0x3F) < 2 ? 180 : 0;
    uint8_t v = clamp8(core + spark/2 + (uint8_t)(50 * ((sinf(now*0.003f+i) + 1.0f) * 0.5f)));
    pyramidCtrlSetRGB(1, i, v, v/3, 0);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT-1-i, v, v/5, spark/4);
  }
}


void drawMusicSmileys() {
  // v1.7: tiny 5x5 mood faces, driven by audio energy. USB-port-up rotation is handled by atomSetXY().
  uint8_t mood = 0;
  if (audioEnergy > 95) mood = 3;
  else if (audioBass > 80) mood = 2;
  else if (audioTreble > 70) mood = 1;
  else mood = (millis() / 1600) % 4;

  uint8_t r = 40, g = 160, b = 120;
  if (mood == 1) { r = 80; g = 180; b = 255; }      // cool
  if (mood == 2) { r = 255; g = 170; b = 40; }      // bass grin
  if (mood == 3) { r = 255; g = 60;  b = 150; }     // party

  uint8_t v = audioGate(85);
  r = clamp8((r * v) / 120);
  g = clamp8((g * v) / 120);
  b = clamp8((b * v) / 120);

  atomFillRGB(0, 0, 0);
  // eyes
  atomSetXY(1, 1, r, g, b);
  atomSetXY(3, 1, r, g, b);

  if (mood == 0) {          // smile
    atomSetXY(1, 3, r, g, b); atomSetXY(2, 4, r, g, b); atomSetXY(3, 3, r, g, b);
  } else if (mood == 1) {   // wink
    atomSetXY(3, 1, 0, 0, 0); atomSetXY(2, 3, r, g, b); atomSetXY(1, 4, r/2, g/2, b/2); atomSetXY(3, 4, r/2, g/2, b/2);
  } else if (mood == 2) {   // wide grin
    atomSetXY(0, 3, r/2, g/2, b/2); atomSetXY(1, 4, r, g, b); atomSetXY(2, 4, r, g, b); atomSetXY(3, 4, r, g, b); atomSetXY(4, 3, r/2, g/2, b/2);
  } else {                  // surprise / beat hit
    atomSetXY(2, 3, r, g, b); atomSetXY(2, 4, r, g, b);
    atomSetXY(0, 0, b, r/2, g); atomSetXY(4, 0, b, r/2, g);
    atomSetXY(0, 4, b, r/2, g); atomSetXY(4, 4, b, r/2, g);
  }
  atomMatrix.show();

  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    float wave = (sinf(millis() * 0.004f + i * 0.7f) + 1.0f) * 0.5f;
    uint8_t halo = clamp8(20 + (int)(90 * wave) + audioEnergy / 2);
    pyramidCtrlSetRGB(1, i, clamp8((r * halo) / 180), clamp8((g * halo) / 180), clamp8((b * halo) / 180));
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, clamp8((b * halo) / 180), clamp8((r * halo) / 220), clamp8((g * halo) / 180));
  }
}


void drawTetrisStack() {
  // Retro falling blocks. Tiny board state lives in RAM only, like an old pocket Tetris.
  static uint32_t board = 0;
  static int px = 2, py = 0;
  static uint8_t kind = 0;
  static uint32_t lastStep = 0;

  auto pieceCell = [](uint8_t k, int x, int y) -> bool {
    switch (k % 5) {
      case 0: return x == 0 && (y == 0 || y == 1);                 // I2
      case 1: return (x == 0 || x == 1) && y == 0;                 // --
      case 2: return (x == 0 && y == 0) || (x == 0 && y == 1) || (x == 1 && y == 1); // L
      case 3: return (x == 0 && y == 1) || (x == 1 && y == 0) || (x == 1 && y == 1); // J
      default: return (x == 0 || x == 1) && (y == 0 || y == 1);     // O
    }
  };

  uint16_t speedMs = btPlaying ? max<uint16_t>(120, 360 - audioBass) : (uint16_t)300;
  if (millis() - lastStep > speedMs) {
    lastStep = millis();
    bool hit = false;
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
      if (!pieceCell(kind, x, y)) continue;
      int nx = px + x, ny = py + y + 1;
      if (ny > 4 || gameBit(board, nx, ny)) hit = true;
    }
    if (!hit) {
      py++;
    } else {
      for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) if (pieceCell(kind, x, y)) gameSetBit(board, px + x, py + y);
      // Clear full rows and collapse downward.
      for (int row = 4; row >= 0; --row) {
        bool full = true;
        for (int x = 0; x < 5; ++x) if (!gameBit(board, x, row)) full = false;
        if (full) {
          uint32_t next = 0;
          for (int y = 4; y >= 0; --y) {
            int srcY = (y < row) ? y - 1 : y;
            if (srcY < 0) continue;
            for (int x = 0; x < 5; ++x) if (gameBit(board, x, srcY)) gameSetBit(next, x, y);
          }
          board = next;
          sharAmberUntilMs = millis() + 180;
        }
      }
      kind = (kind + 1 + (esp_random() & 1)) % 5;
      px = (int)(esp_random() % 4);
      py = 0;
      bool blocked = false;
      for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) if (pieceCell(kind, x, y) && gameBit(board, px+x, py+y)) blocked = true;
      if (blocked) board = 0;
    }
  }

  atomFillRGB(0, 0, 0);
  for (uint8_t y = 0; y < 5; ++y) for (uint8_t x = 0; x < 5; ++x) {
    if (gameBit(board, x, y)) {
      uint8_t r, g, b; hsv2rgb((x * 0.13f + y * 0.08f + 0.08f), 0.9f, 0.55f, r, g, b);
      atomSetXY(x, y, r, g, b);
    }
  }
  uint8_t pr, pg, pb; hsv2rgb((kind * 0.17f + millis() * 0.0002f), 0.95f, 0.85f, pr, pg, pb);
  for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) if (pieceCell(kind, x, y)) atomSetXY(px + x, py + y, pr, pg, pb);
  atomMatrix.show();

  uint8_t pulse = gameBeatPulse(34);
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t col = (i + kind * 3) % 7;
    uint8_t r,g,b; hsv2rgb(col / 7.0f + millis() * 0.00025f, 0.88f, 0.18f + pulse / 255.0f, r, g, b);
    pyramidCtrlSetRGB(1, i, r, g, b);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, b, r / 2, g);
  }
}

void drawMicroRacer() {
  // 3-lane pseudo-racer: road scrolls toward the player, obstacles dodge through perspective.
  uint32_t now = millis();
  uint8_t phase = (now / 115) % 16;
  int carLane = (int)((sinf(now * 0.0017f) + 1.0f) * 1.49f); // 0..2 autopilot
  uint8_t roadGlow = gameBeatPulse(26);
  atomFillRGB(0, 0, 0);
  for (uint8_t y = 0; y < 5; ++y) {
    int left = max(0, 2 - (int)y / 2);
    int right = min(4, 2 + (int)y / 2);
    atomSetXY(left, y, roadGlow / 2, roadGlow / 2, roadGlow);
    atomSetXY(right, y, roadGlow / 2, roadGlow / 2, roadGlow);
    if (((phase + y) & 3) == 0) atomSetXY(2, y, 80, 80, 80);
  }
  uint8_t obstacleY = phase % 5;
  uint8_t obstacleLane = (phase / 2 + (now / 1300)) % 3;
  int ox = 1 + obstacleLane;
  atomSetXY(ox, obstacleY, 255, 80, 20);
  atomSetXY(1 + carLane, 4, 30, 220, 255);
  atomSetXY(1 + carLane, 3, 0, 70, 140);
  atomMatrix.show();

  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t lane = abs(i - PYRAMID_LED_COUNT / 2);
    uint8_t tail = clamp8(20 + roadGlow + max(0, 90 - lane * 18));
    bool nitro = ((i + now / 45) % 9) == 0;
    pyramidCtrlSetRGB(1, i, nitro ? 255 : tail / 4, nitro ? 180 : tail / 3, tail);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, tail / 5, nitro ? 150 : tail / 2, nitro ? 255 : tail);
  }
}

void drawPixelRunner() {
  // A tiny 2D platformer: runner jumps over procedural ledges and coins.
  uint32_t now = millis();
  uint8_t scroll = (now / 180) % 5;
  float jump = fabsf(sinf(now * 0.0042f));
  uint8_t heroY = (jump > 0.62f) ? 2 : (jump > 0.35f ? 3 : 4);
  atomFillRGB(0, 0, 0);
  // sky / stars
  if ((now / 500) & 1) atomSetXY(4, 0, 40, 40, 90);
  if ((now / 700) & 1) atomSetXY(0, 1, 30, 30, 70);
  // moving ground and platforms
  for (uint8_t x = 0; x < 5; ++x) {
    atomSetXY(x, 4, 0, 70, 35);
    if (((x + scroll) % 4) == 0) atomSetXY(x, 3, 90, 55, 15);
  }
  // hero and coin
  atomSetXY(1, heroY, 255, 210, 40);
  if (heroY < 4) atomSetXY(1, heroY + 1, 255, 120, 20);
  uint8_t coinX = (4 + 5 - scroll) % 5;
  atomSetXY(coinX, 2, 255, 170, 0);
  atomMatrix.show();

  uint8_t p = gameBeatPulse(24);
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    bool coin = ((i + now / 80) % 11) == 0;
    uint8_t r = coin ? 255 : 0;
    uint8_t g = coin ? 160 : clamp8(35 + p);
    uint8_t b = coin ? 0 : clamp8(70 + p / 2);
    pyramidCtrlSetRGB(1, i, r, g, b);
    pyramidCtrlSetRGB(2, i, b / 2, g, r / 3);
  }
}

void drawDoomRaycast() {
  // 5x5 fake raycaster: corridor walls, demon pixel, muzzle flash. Very Doom-ish, but tiny and safe.
  uint32_t now = millis();
  uint8_t pulse = gameBeatPulse(18);
  uint8_t step = (now / 230) % 8;
  bool fire = ((now / 700) % 5) == 0 || (btPlaying && audioBass > 80);
  atomFillRGB(0, 0, 0);
  for (uint8_t y = 0; y < 5; ++y) {
    uint8_t depth = 5 - y;
    uint8_t wall = clamp8(30 + depth * 28 + pulse / 2);
    atomSetXY(0, y, wall, wall / 4, wall / 5);
    atomSetXY(4, y, wall, wall / 4, wall / 5);
    if (y > 0 && y < 4) {
      atomSetXY(1, y, wall / 3, wall / 6, wall / 8);
      atomSetXY(3, y, wall / 3, wall / 6, wall / 8);
    }
  }
  // far lights / door
  atomSetXY(2, 0, 20, 90, 40);
  atomSetXY(2, 1, 10, 45, 20);
  // demon/cyber-eye
  if (step < 6) {
    atomSetXY(2, 2, 180, 0, 0);
    atomSetXY(1, 2, 55, 0, 0); atomSetXY(3, 2, 55, 0, 0);
  }
  // weapon / muzzle
  atomSetXY(2, 4, fire ? 255 : 110, fire ? 190 : 80, fire ? 30 : 10);
  if (fire) { atomSetXY(2, 3, 255, 220, 60); atomSetXY(1, 4, 200, 80, 20); atomSetXY(3, 4, 200, 80, 20); }
  atomMatrix.show();

  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    int d = abs(i - PYRAMID_LED_COUNT / 2);
    uint8_t lava = clamp8(25 + pulse + max(0, 100 - d * 18));
    bool shot = fire && ((i + now / 35) % 5 == 0);
    pyramidCtrlSetRGB(1, i, shot ? 255 : lava, shot ? 220 : lava / 4, shot ? 60 : 0);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, shot ? 255 : lava / 2, shot ? 180 : lava / 5, shot ? 40 : 0);
  }
}

void drawInvaderSwarm() {
  // Space-invaders style swarm: little enemies march, player shoots upward.
  uint32_t now = millis();
  uint8_t step = (now / 300) % 10;
  int shift = (step < 5) ? step - 2 : 7 - step;
  atomFillRGB(0, 0, 0);
  for (uint8_t row = 0; row < 2; ++row) {
    for (uint8_t k = 0; k < 3; ++k) {
      int x = k * 2 + shift / 2;
      int y = row;
      if (x >= 0 && x < 5) {
        uint8_t r = row ? 120 : 80;
        uint8_t g = clamp8(120 + gameBeatPulse(10));
        atomSetXY(x, y, r, g, 30);
      }
    }
  }
  uint8_t shipX = 2 + (((now / 900) & 1) ? 1 : -1);
  atomSetXY(shipX, 4, 40, 160, 255);
  uint8_t laserY = 3 - ((now / 90) % 4);
  if (laserY < 5) atomSetXY(shipX, laserY, 255, 255, 80);
  if (((now / 600) % 6) == 0) { atomSetXY(2, 1, 255, 80, 0); atomSetXY(2, 2, 255, 160, 30); }
  atomMatrix.show();

  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    bool laser = ((i + now / 55) % 7) == 0;
    uint8_t swarm = clamp8(20 + gameBeatPulse(20) + ((i * 13 + now / 40) & 0x3F));
    pyramidCtrlSetRGB(1, i, laser ? 255 : 20, laser ? 255 : swarm, laser ? 60 : swarm / 4);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, laser ? 255 : swarm / 2, laser ? 120 : swarm, laser ? 30 : 20);
  }
}


void drawPongTennis() {
  // Classic 5x5 Pong/Tennis: top and bottom two-pixel paddles defend a one-pixel ball.
  // It is autopiloted like a tiny old handheld game. Audio/hashrate make the rallies faster.
  static int bx = 2, by = 2;
  static int vx = 1, vy = 1;
  static int topPad = 1;
  static int botPad = 2;
  static uint8_t topScore = 0, botScore = 0;
  static uint32_t lastStep = 0;
  static uint32_t flashUntil = 0;

  uint32_t now = millis();
  uint8_t pace = gameBeatPulse(18);
  uint16_t stepMs = btPlaying ? max<uint16_t>(85, 210 - audioEnergy) : max<uint16_t>(95, 215 - min<uint32_t>(115, currentHashRate / 70));

  if (now - lastStep > stepMs) {
    lastStep = now;

    // Old-console autopilot: paddles chase the ball but with a little delay/imperfection.
    int targetTop = bx - 1 + (((now / 900) & 1) ? 0 : (vx > 0 ? 1 : 0));
    int targetBot = bx - 1 + (((now / 700) & 1) ? (vx < 0 ? -1 : 0) : 0);
    if (targetTop > topPad) topPad++; else if (targetTop < topPad) topPad--;
    if (targetBot > botPad) botPad++; else if (targetBot < botPad) botPad--;
    topPad = constrain(topPad, 0, 3);
    botPad = constrain(botPad, 0, 3);

    int nbx = bx + vx;
    int nby = by + vy;

    if (nbx < 0) { nbx = 1; vx = 1; }
    if (nbx > 4) { nbx = 3; vx = -1; }

    bool hit = false;
    if (nby <= 0) {
      if (nbx >= topPad && nbx <= topPad + 1) {
        nby = 1; vy = 1; hit = true;
        vx += (nbx == topPad) ? -1 : 1;
        vx = constrain(vx, -1, 1); if (vx == 0) vx = ((now >> 4) & 1) ? 1 : -1;
      } else {
        botScore = (botScore + 1) % 5;
        bx = 2; by = 2; vx = ((now >> 7) & 1) ? 1 : -1; vy = 1;
        flashUntil = now + 260;
      }
    } else if (nby >= 4) {
      if (nbx >= botPad && nbx <= botPad + 1) {
        nby = 3; vy = -1; hit = true;
        vx += (nbx == botPad) ? -1 : 1;
        vx = constrain(vx, -1, 1); if (vx == 0) vx = ((now >> 5) & 1) ? 1 : -1;
      } else {
        topScore = (topScore + 1) % 5;
        bx = 2; by = 2; vx = ((now >> 6) & 1) ? 1 : -1; vy = -1;
        flashUntil = now + 260;
      }
    }

    if (!hit && nby > 0 && nby < 4) {
      bx = nbx;
      by = nby;
    } else if (hit) {
      bx = nbx;
      by = nby;
      flashUntil = now + 90;
    }
  }

  atomFillRGB(0, 0, 0);

  // Center court line with slow phosphor glow.
  for (uint8_t x = 0; x < 5; ++x) {
    if (((x + now / 250) & 1) == 0) atomSetXY(x, 2, 0, 35, 28);
  }

  bool flash = now < flashUntil;
  uint8_t paddle = flash ? 255 : clamp8(95 + pace);
  uint8_t ballR = flash ? 255 : 240;
  uint8_t ballG = flash ? 210 : clamp8(145 + pace / 2);

  // Top and bottom two-pixel paddles.
  atomSetXY(topPad, 0, 0, paddle, 255);
  atomSetXY(topPad + 1, 0, 0, paddle, 255);
  atomSetXY(botPad, 4, 255, clamp8(paddle / 2), 0);
  atomSetXY(botPad + 1, 4, 255, clamp8(paddle / 2), 0);

  // Ball.
  atomSetXY(bx, by, ballR, ballG, 30);

  // Tiny score sparks in corners.
  if (topScore) atomSetXY(0, 0, clamp8(40 + topScore * 35), 0, 80);
  if (botScore) atomSetXY(4, 4, clamp8(40 + botScore * 35), 40, 0);

  atomMatrix.show();

  // Pyramid becomes an arcade cabinet border + moving ball echo.
  int echo = (now / 70) % PYRAMID_LED_COUNT;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    int d = abs(i - echo);
    d = min(d, PYRAMID_LED_COUNT - d);
    uint8_t trail = (d == 0) ? 210 : (d == 1 ? 90 : (d == 2 ? 35 : 8));
    uint8_t rail = clamp8(20 + pace / 2);
    bool corner = (i == 0 || i == 6 || i == 7 || i == 13);
    pyramidCtrlSetRGB(1, i, flash ? 255 : (corner ? 80 : rail), clamp8(trail + rail), corner ? 255 : trail / 2);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, clamp8(trail + rail), flash ? 210 : rail / 2, corner ? 60 : 0);
  }
}



bool janusFarmModeActive() {
  if (btPlaying || btApprovalPending) return false;
  if (janusColonyIdleArmed) return true;     // AFK miner ritual visual
  if (espNowReady && currentPalette == PAL_JANUS_MINE) return true;
  if (!jobReady) return false;
  if (millis() - jobReceivedMs > JANUS_JOB_STALE_MS) return false;
  return currentPalette == PAL_JANUS_MINE;
}

void janusRitualBaseColor(uint8_t &r, uint8_t &g, uint8_t &b) {
  uint32_t now = millis();
  float wave = (sinf(now * 0.004f) + 1.0f) * 0.5f;
  switch (currentPalette) {
    case PAL_FIRE:          r = 255; g = clamp8(48 + wave * 90); b = 0; break;
    case PAL_OCEAN:         r = 0; g = clamp8(90 + wave * 70); b = 255; break;
    case PAL_MATRIX_RAIN:   r = 0; g = clamp8(170 + wave * 70); b = 40; break;
    case PAL_SABER_BLUE:    r = 30; g = 115; b = 255; break;
    case PAL_SABER_GREEN:   r = 25; g = 255; b = 80; break;
    case PAL_SABER_RED:     r = 255; g = 38; b = 34; break;
    case PAL_SABER_AMBER:   r = 255; g = 170; b = 35; break;
    case PAL_SABER_VIOLET:  r = 165; g = 55; b = 255; break;
    case PAL_AMBER_REACTOR: r = 255; g = 135; b = 10; break;
    case PAL_SPACE_MOON:    r = 35; g = 65; b = 180; break;
    case PAL_RAIN_DROPS:    r = 20; g = 130; b = 255; break;
    default:                r = 0; g = clamp8(150 + wave * 80); b = clamp8(170 + wave * 70); break;
  }
}

static inline void janusRitualPixel(uint8_t x, uint8_t y, char c, uint8_t br, uint8_t bg, uint8_t bb, uint8_t pulse) {
  if (c == ' ') return;
  uint8_t r = 0, g = 0, b = 0;
  switch (c) {
    case 'O': r = br; g = bg; b = bb; break;                         // portal eye
    case '+': r = 255; g = clamp8(160 + pulse); b = 0; break;         // alpha/omega threshold
    case '-': r = clamp8(br / 4); g = clamp8(bg / 2); b = clamp8(bb / 2); break;
    case '|': r = clamp8(br / 3); g = clamp8(bg / 2); b = clamp8(bb / 2); break;
    case '/': case '\\': r = clamp8(180 + pulse / 3); g = clamp8(180 + pulse / 3); b = clamp8(190 + pulse / 4); break;
    case 'o': r = 255; g = clamp8(180 + pulse / 2); b = 35; break;    // witness head
    case '.': case '\'': r = clamp8(br / 3); g = clamp8(bg / 3); b = clamp8(bb / 2); break;
    default: r = br; g = bg; b = bb; break;
  }
  atomSetXY(x, y, r, g, b);
}

void drawJanusWitnessZoom(bool topWitness, uint8_t br, uint8_t bg, uint8_t bb, uint8_t pulse) {
  // Large readable 5x5 witness frame.
  // topWitness  = \o/  : raised arms, head high, legs down.
  // bottomWitness = /o\ : mirrored/inverted witness under the threshold.
  atomFillRGB(0, 0, 0);
  uint8_t wr = clamp8(190 + pulse / 3), wg = clamp8(190 + pulse / 3), wb = clamp8(200 + pulse / 4);
  uint8_t hr = 255, hg = clamp8(170 + pulse), hb = 12;
  uint8_t ar = clamp8(wr + 35), ag = clamp8(wg + 35), ab = clamp8(wb + 35);
  bool flick = ((millis() / 180) & 1);
  if (topWitness) {
    // Readable \o/ : arms on top row, head below, body/legs below.
    atomSetXY(0, 0, ar, ag, ab); atomSetXY(1, 0, ar, ag, ab);
    atomSetXY(3, 0, ar, ag, ab); atomSetXY(4, 0, ar, ag, ab);
    atomSetXY(2, 1, hr, hg, hb);                         // o
    atomSetXY(2, 2, wr, wg, wb);                         // body
    atomSetXY(1, 3, wr, wg, wb); atomSetXY(3, 3, wr, wg, wb);
    atomSetXY(1, 4, br/3, bg/3, bb/2); atomSetXY(3, 4, br/3, bg/3, bb/2);
    if (flick) { atomSetXY(2, 0, 255, clamp8(160 + pulse), 0); } // tiny crown/threshold spark
  } else {
    // Readable /o\ mirrored below: legs/axis above, head low, arms on bottom row.
    atomSetXY(1, 0, br/3, bg/3, bb/2); atomSetXY(3, 0, br/3, bg/3, bb/2);
    atomSetXY(1, 1, wr, wg, wb); atomSetXY(3, 1, wr, wg, wb);
    atomSetXY(2, 2, wr, wg, wb);                         // body
    atomSetXY(2, 3, hr, hg, hb);                         // o
    atomSetXY(0, 4, ar, ag, ab); atomSetXY(1, 4, ar, ag, ab);
    atomSetXY(3, 4, ar, ag, ab); atomSetXY(4, 4, ar, ag, ab);
    if (flick) { atomSetXY(2, 4, 255, clamp8(160 + pulse), 0); }
  }
  atomMatrix.show();
}

void drawJanusPortalZoom(bool leftPortal, uint8_t br, uint8_t bg, uint8_t bb, uint8_t pulse) {
  // Large readable portal frame with an O-eye.
  atomFillRGB(0, 0, 0);
  uint8_t pr = br, pg = bg, pb = bb;
  uint8_t er = 255, eg = clamp8(140 + pulse), eb = 0;
  atomSetXY(2, 0, pr, pg, pb);
  atomSetXY(1, 1, pr, pg, pb); atomSetXY(3, 1, pr, pg, pb);
  atomSetXY(0, 2, pr, pg, pb); atomSetXY(2, 2, er, eg, eb); atomSetXY(4, 2, pr, pg, pb);
  atomSetXY(1, 3, pr, pg, pb); atomSetXY(3, 3, pr, pg, pb);
  atomSetXY(2, 4, pr, pg, pb);
  if (leftPortal) atomSetXY(4, 2, clamp8(br/3), clamp8(bg/2), clamp8(bb/2));
  else atomSetXY(0, 2, clamp8(br/3), clamp8(bg/2), clamp8(bb/2));
  atomMatrix.show();
}


void drawSharFirewoodCoinflip() {
  uint32_t now = millis();
  uint32_t left = (sharAmberUntilMs > now) ? (sharAmberUntilMs - now) : 0;
  float t = left > 1150 ? 1.0f : (float)left / 1150.0f;
  uint8_t blast = clamp8(80 + (int)(170.0f * t));
  uint8_t ember = clamp8(35 + (int)(120.0f * t));
  uint8_t flip = (now / 90) & 7;

  // Pyramid: firewood thrown into a campfire — amber wave plus sparks.
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    int waveA = abs((int)i - (int)flip);
    int waveB = abs((int)(PYRAMID_LED_COUNT - 1 - i) - (int)flip);
    uint8_t wave = clamp8(max(0, 150 - min(waveA, waveB) * 28));
    uint8_t spark = ((esp_random() + i * 37 + now) & 0x0F) < 3 ? clamp8(80 + (esp_random() & 0x7F)) : 0;
    uint8_t r = clamp8(blast + wave + spark);
    uint8_t g = clamp8(ember + wave / 2 + spark / 3);
    uint8_t b = clamp8((spark > 0) ? 12 : 0);
    pyramidCtrlSetRGB(1, i, r, g, b);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, r, clamp8(g * 0.72f), b);
  }

  // Atom: coinflip / gold disk frames on 5x5.
  atomFillRGB(0, 0, 0);
  uint8_t r = 255;
  uint8_t g = clamp8(120 + blast / 2);
  uint8_t b = 0;
  if (flip == 0 || flip == 4) {
    // full coin face
    const uint8_t pix[] = {7, 11, 12, 13, 17};
    for (uint8_t k = 0; k < sizeof(pix); ++k) atomMatrix.setPixelColor(atomRotIndex(pix[k]), atomMatrix.Color(r, g, b));
    atomMatrix.setPixelColor(atomRotIndex(12), atomMatrix.Color(255, 230, 40));
  } else if (flip == 1 || flip == 5) {
    // vertical spinning edge
    atomSetXY(2, 0, r, g, b); atomSetXY(2, 1, r, g, b); atomSetXY(2, 2, 255, 230, 40); atomSetXY(2, 3, r, g, b); atomSetXY(2, 4, r, g, b);
  } else if (flip == 2 || flip == 6) {
    // horizontal flying ember/log
    atomSetXY(0, 2, r, g/2, 0); atomSetXY(1, 2, r, g, b); atomSetXY(2, 2, 255, 240, 60); atomSetXY(3, 2, r, g, b); atomSetXY(4, 2, r, g/2, 0);
  } else {
    // burst sparks
    atomSetXY(2, 2, 255, 240, 60);
    atomSetXY(1, 1, r, g, b); atomSetXY(3, 1, r, g, b); atomSetXY(1, 3, r, g, b); atomSetXY(3, 3, r, g, b);
    atomSetXY(2, 0, 120, 60, 0); atomSetXY(0, 2, 120, 60, 0); atomSetXY(4, 2, 120, 60, 0); atomSetXY(2, 4, 120, 60, 0);
  }
  atomMatrix.show();
}

void drawFarmPortalJanus() {
  uint32_t now = millis();
  if (now < sharAmberUntilMs) {
    drawSharFirewoodCoinflip();
    return;
  }

  uint8_t br, bg, bb;
  janusRitualBaseColor(br, bg, bb);
  uint8_t pulse = clamp8(55 + (int)(110 * ((sinf(now * 0.011f) + 1.0f) * 0.5f)));
  uint8_t mode = (now / 820) % 18; // more detailed walk across the ritual art

  // Virtual ritual canvas, camera-windowed into the 5x5 Atom Matrix:
  //      .  \o/  .
  //      /\  |  /\
  //   | O |--+--| O |
  //      \/  |  \/
  //      '  /o\  '
  static const char* art[] = {
    "                   ",
    "    .   \\o/   .   ",
    "       / \\        ",
    "    /\\   |   /\\   ",
    "   | O |--+--| O |",
    "    \\/   |   \\/   ",
    "       /o\\        ",
    "    '       '      ",
    "                   "
  };
  const uint8_t W = 19, H = 9;

  // Hold the readable symbols longer so the witness is visible on the 5x5 display.
  if (mode == 0 || mode == 1) { drawJanusWitnessZoom(true, br, bg, bb, pulse); }
  else if (mode == 4 || mode == 5) { drawJanusPortalZoom(true, br, bg, bb, pulse); }
  else if (mode == 8 || mode == 9) { drawJanusPortalZoom(false, br, bg, bb, pulse); }
  else if (mode == 13 || mode == 14) { drawJanusWitnessZoom(false, br, bg, bb, pulse); }
  else {
    int vx = 0, vy = 0;
    uint32_t fine = (now / 180) % 64;
    if (mode == 2 || mode == 3) {             // camera drops from \o/ down to bridge
      vx = 6;
      vy = min<int>(4, fine / 8);
    } else if (mode == 6 || mode == 7) {      // left portal -> center plus
      vx = min<int>(7, fine / 8);
      vy = 2 + ((fine / 16) & 1);
    } else if (mode == 10 || mode == 11) {    // right portal -> center plus
      vx = 12 - min<int>(7, fine / 8);
      vy = 2 + ((fine / 16) & 1);
    } else if (mode == 12) {                  // center random top/bottom choice
      vx = 6;
      vy = ((esp_random() ^ now) & 1) ? 0 : 4;
    } else if (mode == 15 || mode == 16) {    // bottom witness back upward
      vx = 6;
      vy = 4 - min<int>(4, fine / 8);
    } else {                                  // full left/right scan over all symbols
      vx = (fine < 32) ? min<int>(12, fine / 2) : (12 - min<int>(12, (fine - 32) / 2));
      vy = 2;
    }

    atomFillRGB(0, 0, 0);
    for (uint8_t y = 0; y < 5; ++y) {
      for (uint8_t x = 0; x < 5; ++x) {
        uint8_t ax = min<uint8_t>(W - 1, vx + x);
        uint8_t ay = min<uint8_t>(H - 1, vy + y);
        janusRitualPixel(x, y, art[ay][ax], br, bg, bb, pulse);
      }
    }
    // Hot center marker when camera crosses the alpha/omega threshold.
    if (vx <= 8 && vx + 4 >= 8 && vy <= 4 && vy + 4 >= 4) {
      atomSetXY(8 - vx, 4 - vy, 255, clamp8(165 + pulse), 0);
    }
    atomMatrix.show();
  }

  // Pyramid: current palette base + two portals + moving camera spark + alpha/omega seam.
  uint8_t seam = clamp8(85 + (currentHashRate & 0x3F) + pulse / 3);
  uint8_t camera = (now / 95) % PYRAMID_LED_COUNT;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    int dL = abs(i - 2);
    int dR = abs(i - (PYRAMID_LED_COUNT - 3));
    int dC = abs(i - (int)PYRAMID_LED_COUNT / 2);
    int dCam = abs(i - (int)camera);
    uint8_t portalGlow = clamp8(max(0, 95 - min(dL, dR) * 24));
    uint8_t seamGlow = clamp8(max(0, 120 - dC * 38));
    uint8_t camGlow = clamp8(max(0, 85 - dCam * 30));
    uint8_t r = clamp8((int)br / 4 + seamGlow + camGlow / 2);
    uint8_t g = clamp8((int)bg / 3 + portalGlow + seamGlow / 3 + camGlow / 3);
    uint8_t b = clamp8((int)bb / 3 + portalGlow + camGlow);
    if (dC == 0) { r = clamp8(r + seam); g = clamp8(g + seam / 2); }
    pyramidCtrlSetRGB(1, i, r, g, b);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, r, g, b);
  }
}

void drawAudioSafeMinimal() {
  uint8_t v = btPlaying ? clamp8(20 + audioEnergy / 2) : 38;
  uint8_t b = btPlaying ? clamp8(40 + audioTreble / 2) : 66;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    pyramidCtrlSetRGB(1, i, 0, v, b);
    pyramidCtrlSetRGB(2, i, 0, v, b);
  }
  atomFillRGB(0, 0, 0);
  atomSetXY(2,1,0,v,b); atomSetXY(2,2,0,clamp8(v+40),clamp8(b+40)); atomSetXY(2,3,0,v,b);
  atomMatrix.show();
}

void drawJanusMine() {
  uint32_t now = millis();

  if (now < rejectFlashUntilMs) {
    uint8_t pulse = ((now / 80) & 1) ? 180 : 40;
    fillAllRGB(pulse, 0, pulse / 2);
    return;
  }

  if (now < sharAmberUntilMs) {
    float t = (float)(sharAmberUntilMs - now) / 1150.0f;
    uint8_t v = clamp8(90 + (int)(140.0f * t));
    fillAllRGB(v, (uint8_t)(v * 0.52f), 6);
    return;
  }

  // Turquoise breathing + double heartbeat.
  float breathF = (sinf(now * 0.0030f) + 1.0f) * 0.5f;
  uint8_t breath = audioScale(clamp8((int)(32 + 88 * breathF) + (btPlaying ? audioEnergy / 4 : 0)), 95);
  uint32_t phase = now % 2300;
  if (phase < 80 || (phase > 170 && phase < 250)) breath = clamp8(breath + 85);

  uint8_t r = 0;
  uint8_t g = breath;
  uint8_t b = (uint8_t)(breath * 0.82f);

  pyramidFillRGB(r, g, b);
  for (int i = 0; i < ATOM_MATRIX_LED_COUNT; ++i) {
    bool core = (i == 12 || i == 7 || i == 11 || i == 13 || i == 17);
    uint8_t vv = core ? clamp8(breath + 40) : (uint8_t)(breath * 0.45f);
    atomMatrix.setPixelColor(atomRotIndex(i), atomMatrix.Color(0, vv, (uint8_t)(vv * 0.85f)));
  }
  atomMatrix.show();
}

void drawBtApprovalCampfire() {
  uint32_t now = millis();
  uint8_t phase = (now >> 4) & 0xFF;
  for (int i = 0; i < PYRAMID_LED_COUNT; ++i) {
    uint8_t flicker = 60 + ((phase * 5 + i * 29 + (esp_random() & 0x3F)) & 0x7F);
    uint8_t r = clamp8(flicker + 80);
    uint8_t g = clamp8(flicker / 2 + 18);
    uint8_t b = (i & 1) ? 2 : 0;
    pyramidCtrlSetRGB(1, i, r, g, b);
    pyramidCtrlSetRGB(2, PYRAMID_LED_COUNT - 1 - i, r, g / 2, 0);
  }

  // Atom Matrix: warm approval glyph / campfire core.
  atomFillRGB(0, 0, 0);
  const uint8_t flamePixels[] = {7, 11, 12, 13, 16, 17, 18, 21, 22, 23};
  for (uint8_t k = 0; k < sizeof(flamePixels); ++k) {
    uint8_t v = 80 + ((phase + k * 37 + (esp_random() & 0x1F)) & 0x7F);
    atomMatrix.setPixelColor(atomRotIndex(flamePixels[k]), atomMatrix.Color(clamp8(v + 80), clamp8(v / 2), 0));
  }
  // Center white/yellow spark means: press to approve.
  if ((now / 350) & 1) atomMatrix.setPixelColor(atomRotIndex(12), atomMatrix.Color(255, 180, 25));
  atomMatrix.show();
}

void updateLeds() {
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t interval = btPlaying ? JANUS_LED_MS_AUDIO : JANUS_LED_MS_IDLE;
  if ((btConnected || btPlaying) && freeHeap < JANUS_BT_LOW_HEAP_VISUAL_GUARD) interval = JANUS_LED_MS_AUDIO_LOW_HEAP;
  if (millis() - lastLedMs < interval) return;
  lastLedMs = millis();

  if (btApprovalPending) {
    drawBtApprovalCampfire();
    return;
  }

  // v1.8J: JANUS_MINE is an indicator of real colony mode, not a fake idle palette.
  if (currentPalette == PAL_JANUS_MINE && !janusMinePaletteAllowed()) {
    currentPalette = PAL_RAINBOW;
  }

  if ((btConnected || btPlaying) && freeHeap < JANUS_BT_LOW_HEAP_VISUAL_GUARD) {
    drawAudioSafeMinimal();
    return;
  }

  // SHAR always gets a visible coinflip/firewood burst, whatever palette the user is on.
  if (millis() < sharAmberUntilMs) {
    drawSharFirewoodCoinflip();
    return;
  }

  // Mining does not force the UI forever. JANUS_MINE shows the ritual miner screen;
  // if the user cycles to another palette, mining continues silently in the background.
  if (currentPalette == PAL_JANUS_MINE && janusFarmModeActive()) {
    drawFarmPortalJanus();
    return;
  }

  switch (currentPalette) {
    case PAL_RAINBOW: drawRainbow(); break;
    case PAL_OCEAN: drawOcean(); break;
    case PAL_FIRE: drawFire(); break;
    case PAL_MATRIX_RAIN: drawMatrixRain(); break;
    case PAL_JANUS_MINE: drawJanusMine(); break;
    case PAL_AURORA: drawAurora(); break;
    case PAL_RF_SHADOW: drawRfShadow(); break;
    case PAL_COLONY_HEART: drawColonyHeart(); break;
    case PAL_PRISM_WARP: drawPrismWarp(); break;
    case PAL_DATA_SCANNER: drawDataScanner(); break;
    case PAL_AMBER_REACTOR: drawAmberReactor(); break;
    case PAL_INFINITY_TUNNEL: drawInfinityTunnel(); break;
    case PAL_SABER_BLUE: drawSaberRain(40, 120, 255); break;
    case PAL_SABER_GREEN: drawSaberRain(40, 255, 90); break;
    case PAL_SABER_RED: drawSaberRain(255, 40, 40); break;
    case PAL_SABER_AMBER: drawSaberRain(255, 180, 45); break;
    case PAL_SABER_VIOLET: drawSaberRain(170, 70, 255); break;
    case PAL_SNAKE_AI: drawSnakeAI(); break;
    case PAL_SPACE_MOON: drawSpaceMoon(); break;
    case PAL_RAIN_DROPS: drawRainDrops(); break;
    case PAL_TACHYON_RIFT: drawTachyonRift(); break;
    case PAL_BIOLUMEN_REEF: drawBiolumenReef(); break;
    case PAL_STARFORGE: drawStarforge(); break;
    case PAL_MUSIC_SMILEYS: drawMusicSmileys(); break;
    case PAL_TETRIS_STACK: drawTetrisStack(); break;
    case PAL_MICRO_RACER: drawMicroRacer(); break;
    case PAL_PIXEL_RUNNER: drawPixelRunner(); break;
    case PAL_DOOM_RAYCAST: drawDoomRaycast(); break;
    case PAL_INVADER_SWARM: drawInvaderSwarm(); break;
    case PAL_PONG_TENNIS: drawPongTennis(); break;
    default: drawJanusMine(); break;
  }
}

// =====================================================
// INIT / LOOP
// =====================================================

void initPyramid() {
  Serial.printf("PYRAMID INIT | Atom Matrix profile | primary Wire SDA=%d SCL=%d | I2S BCLK=%d WS=%d DOUT=%d DIN=%d\n",
                EP_I2C_SDA, EP_I2C_SCL, EP_I2S_BCLK, EP_I2S_WS, EP_I2S_DOUT, EP_I2S_DIN);

  pyramidReady = false;

  // IMPORTANT for Atom Matrix:
  // M5.begin() initializes the primary Wire bus on G21/G25 for the onboard MPU6886.
  // The Pyramid header uses the same SCL/SDA signals, so try &Wire first.
  scanPyramidI2C(Wire, "Wire primary Atom header", EP_I2C_SDA, EP_I2C_SCL);

  bool ok = false;
  if (busHasPyramidCore(Wire)) {
    Serial.println("PYRAMID BUS CANDIDATE | Wire primary has Pyramid-like devices; calling ep.begin(&Wire,...)");
    ok = ep.begin(&Wire, EP_I2C_SDA, EP_I2C_SCL, EP_I2S_BCLK, EP_I2S_WS, EP_I2S_DOUT, EP_I2S_DIN, EP_SAMPLE_RATE);
    if (ok) {
      pyramidBus = &Wire;
      pyramidActiveSda = EP_I2C_SDA;
      pyramidActiveScl = EP_I2C_SCL;
    }
  }

  // Fallback 1: some cores leave Wire busy; try second I2C peripheral on same physical pins.
  if (!ok) {
    Wire1.end();
    scanPyramidI2C(Wire1, "Wire1 fallback Atom header", EP_I2C_SDA, EP_I2C_SCL);
    if (busHasPyramidCore(Wire1)) {
      Serial.println("PYRAMID BUS CANDIDATE | Wire1 sees Pyramid-like devices; calling ep.begin(&Wire1,...)");
      ok = ep.begin(&Wire1, EP_I2C_SDA, EP_I2C_SCL, EP_I2S_BCLK, EP_I2S_WS, EP_I2S_DOUT, EP_I2S_DIN, EP_SAMPLE_RATE);
      if (ok) {
        pyramidBus = &Wire1;
        pyramidActiveSda = EP_I2C_SDA;
        pyramidActiveScl = EP_I2C_SCL;
      }
    }
  }

  // Fallback 2: external Grove I2C. This usually is NOT the internal Pyramid bus,
  // but scanning it makes wrong wiring/power obvious in Serial logs.
  if (!ok) {
    Wire1.end();
    scanPyramidI2C(Wire1, "Wire1 Grove fallback", 26, 32);
    if (busHasPyramidCore(Wire1)) {
      Serial.println("PYRAMID BUS CANDIDATE | Grove fallback sees Pyramid-like devices; calling ep.begin(&Wire1,26,32,...)");
      ok = ep.begin(&Wire1, 26, 32, EP_I2S_BCLK, EP_I2S_WS, EP_I2S_DOUT, EP_I2S_DIN, EP_SAMPLE_RATE);
      if (ok) {
        pyramidBus = &Wire1;
        pyramidActiveSda = 26;
        pyramidActiveScl = 32;
      }
    }
  }

  if (!ok) {
    Serial.println("PYRAMID INIT FAIL | Atom is alive but Pyramid I2C devices were not initialized");
    Serial.println("CHECK 1: power the base from the Pyramid bottom USB-C / 5V input, not only from Atom USB");
    Serial.println("CHECK 2: reseat Atom Matrix vertically into the 9-pin header");
    Serial.println("CHECK 3: Serial should show 0x1A STM32_RGB_TOUCH, 0x18 codec, 0x40 ADC, 0x5B amp or 0x60 clock");
    atomFillRGB(90, 0, 0);
    return;
  }

  pyramidReady = true;
  Serial.printf("PYRAMID INIT OK | SDA=%d SCL=%d | running boot light test\n", pyramidActiveSda, pyramidActiveScl);
  configurePyramidLoudMode();
  pyramidCodecSetVolume(JANUS_SPEAKER_CODEC_SAFE_VOLUME);
  pyramidCodecMute(false);
  pyramidSetBrightness(brightnessLevel);

  // Visible boot test: cyan -> amber -> mine turquoise.
  pyramidFillRGB(0, 120, 120);
  atomFillRGB(0, 70, 70);
  delay(220);
  pyramidFillRGB(180, 80, 0);
  atomFillRGB(120, 50, 0);
  delay(220);
  fillAllRGB(0, 70, 60);
}

void initAtomMatrix() {
  atomMatrix.begin();
  atomMatrix.setBrightness(50);
  atomFillRGB(0, 40, 35);
}

void updateHashrate() {
  uint32_t now = millis();
  if (hashWindowStartMs == 0) hashWindowStartMs = now;
  uint32_t dt = now - hashWindowStartMs;
  if (dt >= 2000) {
    currentHashRate = (uint32_t)((hashWindowCount * 1000ULL) / dt);
    hashWindowCount = 0;
    hashWindowStartMs = now;
  }
}

void serialStatus() {
  uint32_t now = millis();
  if (now - lastSerialStatusMs < JANUS_SERIAL_STATUS_MS) return;
  lastSerialStatusMs = now;
  Serial.printf("JANUS ECHO | a2dp=%d disc=%d now=%d bt=%d play=%d mute=%d acl=%d approve=%d trusted=%u job=%d batch=%u H/s=%lu shares=%lu best=%lu rssi=%d palette=%u:%s bright=%u eq=%u/%u/%u pcm=%lu drop=%lu wr=%lu gain=%u%% phone=%u/127 local=%u%% knn=%s conf=%u hint=%u heap=%lu jitter=%u\n",
                btA2dpStarted, btDiscoverableForced, espNowReady, btConnected, btPlaying, muted, btAclAccepted, btApprovalPending, btTrustedCount(), jobReady,
                effectiveBatch(), (unsigned long)currentHashRate, (unsigned long)shares,
                (unsigned long)bestBits, (int)lastRssi, (unsigned)currentPalette, paletteName(currentPalette),
                brightnessLevel, audioBass, audioMid, audioTreble,
                (unsigned long)janusPcmPackets, (unsigned long)janusAudioDropChunks, (unsigned long)janusAudioWrittenChunks,
                (unsigned)janusPcmGainX100, (unsigned)janusPhoneVolume127, (unsigned)volumeLevel,
                knnLabelName(localKnnLabel), (unsigned)localKnnConfidence, (unsigned)(rewardAiHint ? rewardAiHint : localKnnAiHint),
                (unsigned long)ESP.getFreeHeap(), loopJitterUsEma);
}

void setup() {
  // SERIAL RESCUE: start UART before any M5/Pyramid/BT init.
  // If the monitor is silent with this build, the issue is port/baud/upload, not Pyramid init.
  Serial.begin(115200);
  delay(80);
  Serial.println();
  Serial.println("================ JANUS ECHO v1.8K3 PONG_TENNIS_RETRO_FINAL BOOT0 SERIAL OK ================");
  Serial.printf("BOOT0 | chip=%s rev=%u cpu=%uMHz heap=%lu sdk=%s\n",
                ESP.getChipModel(), ESP.getChipRevision(), ESP.getCpuFreqMHz(),
                (unsigned long)ESP.getFreeHeap(), ESP.getSdkVersion());
  Serial.flush();

  auto cfg = M5.config();
  Serial.println("BOOT1 | calling M5.begin()");
  Serial.flush();
  M5.begin(cfg);
  Serial.println("BOOT2 | M5.begin() returned");
  Serial.flush();
  delay(120);

  prefs.begin("janus_echo", false);
  Serial.println("BOOT3 | Preferences opened");
  migrateLegacyTwoOwnerAcl();
  uint64_t mac = ESP.getEfuseMac();
  workerId = (uint16_t)(mac & 0xFFFF);
  randomSeed((uint32_t)esp_random());
  Serial.printf("BOOT4 | workerId=%u efuse=%04X%08X\n", workerId, (uint16_t)(mac >> 32), (uint32_t)mac);
  Serial.flush();

  Serial.println("BOOT5 | initAtomMatrix()");
  initAtomMatrix();
  atomFillRGB(0, 30, 20);
  delay(120);
  atomFillRGB(30, 14, 0);
  delay(120);
  atomFillRGB(0, 20, 30);
  Serial.println("BOOT6 | Atom Matrix LED test done");
  Serial.flush();

  Serial.println("BOOT7 | bootAclClearCheck()");
  bootAclClearCheck();
  Serial.println("BOOT8 | initPyramid()");
  Serial.flush();
  initPyramid();
  Serial.println("BOOT9 | initPyramid() returned");
  Serial.flush();

  Serial.println("BOOT10 | initBluetoothAudio()");
  Serial.flush();
  initBluetoothAudio();
  Serial.println("BOOT11 | initBluetoothAudio() returned");
  currentPalette = PAL_RAINBOW;  // JANUS_MINE appears only when ESP-NOW/swarm mode is actually active.
  Serial.flush();

  Serial.println("BOOT12 | ESP-NOW colony enabled; EXCLUSIVE RADIO + dynamic batch AI + SwarmSense observe");
  Serial.flush();
  espNowLastStopMs = millis();
  janusLastPlaybackActivityMs = millis();
  janusLastUserInteractionMs = millis();
  Serial.println("BOOT13 | miner starts after BT stop; long Atom press requests BT; SwarmSense + J/E observe before control");
  Serial.flush();

  Serial.printf("JANUS Echo Pyramid online | node=%s worker=%u channel=%u\n", JANUS_DEVICE_NAME, workerId, JANUS_ESPNOW_CHANNEL);
  Serial.println("BT gate: unknown phones require Atom button approval. Short press approve, long press reject. Hold Atom button 7s at boot to clear trusted devices.");
  Serial.println("Controls: right side TP1->TP2 next / TP2->TP1 previous; left side swipes steer PHONE volume curve; mute controls device/phone volume; hold TP3/TP4 brightness down/up.");
  Serial.println("HiveMetricPacket HM/v2 + SwarmSense + Pyramid Grove/Stick J/E sentinel enabled.");
  Serial.println("================ JANUS ECHO v1.8L GROVE_STICK_SENTINEL BOOT COMPLETE ================");
  Serial.flush();
}

void loop() {
  updateLoopJitter();
  handleButton();
  handleTouch();
  updateAudioEq();
  updateLocalKnnAgent();
  updateBtAclTimeout();
  updateBtAdvertiseWatchdog();
  manageEspNowLifecycle();
  janusEspNowChannelMaintenance();
#if JANUS_ENABLE_COLONY_ESPNOW
  acceptPendingJobIfAny();
  mineSlice();
  updateHashrate();
  janusDynamicBatchAiTick();
#else
  currentHashRate = 0;
#endif

  uint32_t now = millis();
#if JANUS_ENABLE_COLONY_ESPNOW
  if (now - lastHeartbeatMs >= JANUS_HEARTBEAT_MS) {
    lastHeartbeatMs = now;
    sendHeartbeat();
  }
  if (now - lastEntropyMs >= JANUS_ENTROPY_MS) {
    lastEntropyMs = now;
    sendEntropy();
  }
  if (now - lastSwarmSenseMs >= JANUS_SWARMSENSE_MS) {
    lastSwarmSenseMs = now;
    sendSwarmSense();
  }
  if (now - lastHiveMetricsMs >= JANUS_HIVE_METRICS_MS) {
    lastHiveMetricsMs = now;
    sendHiveMetrics();
  }
  pyramidBlackboardTick();
#endif

  updateLeds();
  serialStatus();
  delay(1);
}

