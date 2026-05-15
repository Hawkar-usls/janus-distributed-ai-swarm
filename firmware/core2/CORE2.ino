/*
  JANUS_CORE2_GALAXY_STATION_v6_40_KENSHI_TACHYON_PROPHECY.ino

  Core2 = главный домашний узел JANUS и галактическая станция роя:
    - ESP-NOW collector для Blind Eye / ADV Beacon / Buzz / EchoMic / Swarm / StickS3
    - локальный SGP30 TVOC/eCO2 на Core2 PORT.A
    - touch UI: HOME -> device detail pages
    - Buzz page: Prev / Play-Pause / Next / Vol- / Vol+
    - tactile touch feedback, brightness controls, Zaporizhzhia weather page
    - вместо старой игры/визуализатора теперь JANUS GALAXY STATION v3.1 CINEMATIC
    - Core2 = центральная станция Януса в процедурном секторе нашей галактики
    - устройства ESP-NOW роя = планеты/станции/пилоты, их телеметрия становится экономикой
    - Янус сам играет в космосим: станция, планета, пилоты, экономика, новости и mind feed
    - при открытой вкладке GALAXY Core2 участвует в SHA256-работе Buzz
    - когда вкладка скрыта, майнер отключается, но мир продолжает жить медленнее
    - v6.21A: SwarmSensePacket v1 observe-only: Core2 сам шлёт NAS/Buzz свою среду и принимает SS от других узлов
    - v6.21B: AIR page для дома: понятные пояснения eCO2/TVOC/SGP30, статус и совет проветривания
    - v6.22: MESH list scroll up/down, RSSI projection tab, Core2 SwarmSense TX diagnostics
    - v6.23: Unified Elite/RTS universe: Core2 = station commander, Stick = Janus pilot, ESP-NOW nodes = living stations/sectors
    - v6.24: Janus Universe final Core2 layer:
        * Janus commander now rotates between swarm stations and services/develops them.
        * Station backdrop planet changes according to the currently serviced orbit/sector.
        * Stick pilot position is tracked as the real Janus location proxy until Stick sends native universe packets.
        * Lightweight SD archive stores Universe/SwarmSense training snapshots for future distributed AI learning.
    - v6.25: diplomacy/galaxy-cluster final layer:
        * Factions now have rival blocs: helping one can worsen relations with enemies.
        * Station camera has real orbit/pitch/zoom taps instead of zoom-only feel.
        * Galaxy chart became a selectable Janus cluster + real Milky Way system projection.
        * Core2 stores selected galaxy/sector/faction state for the future Stick pilot link.
    - v6.31: ZIM MISSION CONTROL:
        * Core2 Galaxy Station treats Zim Geek as the Zim Earth / fake-house planet node.
        * Core2 broadcasts ZM mission orders to Zim; Zim stays solo NerdMiner but runs planet-side missions.
        * Zim reports progress back through SwarmSense, feeding the galaxy economy.
    - v6.33: REAL TMOS EYE + AUDIO CLEAN:
        * Core2 accepts ZimAgentMemoryPacket 'Z','A' and stores Zim's slime-brain status as station memory.
        * Zim is mapped as the exiled Earth scout; Core2 is his absurd imperial command layer.
        * BlindEye page now shows a lightweight TMOS/PIR thermal-vision animation from telemetry.
        * Core2 sends 'E','C' eye-vision control while PAGE_EYE is open, mirroring AUDIO 'A','C'.
        * Optional future BlindEye 'E','F' 8x8 frames are parsed; without them Core2 synthesizes a GIF-like view.
    - v6.36: ULAW20 LOUD FULLFILES audio path:
        * Core2 requests 8 kHz / 20 ms u-law frames by default.
        * Receiver keeps ADPCM fallback but boosts speech gain and raises speaker max volume.
        * AUDIO page has Vol-/Vol+ for runtime loudness; old low saved volume is auto-bumped.
        * SD raw mirror remains disabled by default to avoid blocking live audio.
    - v6.38: AUDIO SNAPSHOT BUFFER:
        * Core2 keeps a much deeper RX queue for ATOM speech clips.
        * Playback starts after a real prebuffer, not on fragile live frame timing.
    - v6.38B: DRAMFIX:
        * AUDIO RX queue moved out of .bss into heap/PSRAM.
        * ULAW20 frame size corrected to 160 samples.
        * Snapshot queue kept deep enough without overflowing Core2 DRAM.
    - v6.40: KENSHI + TACHYON PROPHECY BUS:
        * Core2 now understands BlindEye/Swarm 'T','P' prophecy packets and 'K','2' virtual bubble packets.
        * Core2 answers with its own Core2Home prophecy so BlindEye TP rx is no longer zero once Core2 is flashed.
        * Remote prophecies feed the galaxy/universe layer, MESH registry, and Core2's future-stress predictor.

    - v6.39: RAMANUJAN THETA PASSIVE MINER:
        * Core2 passive Buzz-worker receives a lightweight Ramanujan theta notebook.
        * Finite phi(q), psi(q), Euler f(-q) and mock-like signal feed miner scheduling.
        * Optional theta nonce-walk changes search order only; SHA256 proof/target check stays exact.

    - v6.30: AUDIO RADIO monitor:
        * Core2 sends AC ON only while PAGE_AUDIO is open and live monitor is enabled.
        * Core2 accepts AF ADPCM4 8 kHz mono ESP-NOW frames, queues them, and plays them on M5.Speaker.
        * Leaving AUDIO sends AC OFF, so ATOM/EchoBase stops streaming and ESP-NOW stays free.

  SGP30 wiring:
    Core2 PORT.A Grove:
      SDA = GPIO32
      SCL = GPIO33
      5V
      GND

  Arduino libraries:
    - M5Unified
    - ArduinoJson
    - Adafruit SGP30 Sensor
    - Adafruit BusIO
*/

#include <Arduino.h>
#include <M5Unified.h>
#include <SD.h>
#include <WiFi.h>
#include <Wire.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Adafruit_SGP30.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>
#include <math.h>
#include "esp_heap_caps.h"

#ifndef ESP_ARDUINO_VERSION_MAJOR
#define ESP_ARDUINO_VERSION_MAJOR 2
#endif

// ========================= JANUS / WIFI =========================

#define DEVICE_ID               "core2_home_node"
#define DEVICE_KIND             "core2_home_colony_sgp30_touch"
#define WIFI_SSID "YOUR_WIFI"
#define WIFI_PASSWORD "YOUR_PASSWORD"

#define JANUS_COLONY_ENABLE     1
#define JANUS_BROADCAST_CHANNEL 0

#define COLONY_HEARTBEAT_MS     2000UL
#define COLONY_ENTROPY_MS       2500UL
#define CORE2_SWARMSENSE_MS     3000UL
#define COLONY_PEER_FIX_MS      1500UL
#define NODE_TIMEOUT_MS         15000UL
#define DRAW_INTERVAL_MS        120UL

// v6.20B: universal ESP-NOW colony registry layered over the stable Core2 code, compile-safe signatures.
// SD archive is intentionally NOT integrated here: the prior SD build stalled UI/touch/FPS.
// Core2 mining remains gated by GALAXY tab to preserve responsiveness.
#define CORE2_STABLE_BUILD       1
#define CORE2_UNIVERSAL_COLONY   1
#define CORE2_SWARMSENSE_OBSERVE  1
#define CORE2_SD_ARCHIVE_ENABLE    1       // lightweight append-only Universe training log; if SD mount fails, firmware continues
#define CORE2_SD_CS                4       // M5Stack Core2 TF-card CS
#define CORE2_UNIVERSE_ARCHIVE_MS  60000UL
#define CORE2_MAX_COLONY_NODES    24
#define CORE2_HAPTIC_ENABLE      0       // no constant vibration; touch response stays visual/UI
#define CORE2_MINER_MAX_BATCH    64      // safe upper limit for Core2 worker batch
#define CORE2_MINER_LOW_BATCH    12
#define CORE2_MINER_SHARE_BEEP   1

// v6.39 RAMANUJAN THETA PASSIVE MINER
// This does NOT bypass SHA256 and does NOT fake shares.
// It gives the very slow Core2 worker a mathematical scheduling/telemetry layer:
// finite Ramanujan theta functions shape batch, AI confidence and nonce order.
#define CORE2_RAMANUJAN_THETA_ENABLE      1
#define CORE2_RAMANUJAN_THETA_MS          777UL
#define CORE2_RAMANUJAN_DEPTH             10
#define CORE2_RAMANUJAN_NONCE_WALK        1
#define CORE2_RAMANUJAN_SERIAL_MS         9000UL
#define SGP30_INTERVAL_MS       1000UL
#define SGP30_BASELINE_MS       300000UL
#define BUZZ_CURRENT_MS         3000UL
#define WEATHER_INTERVAL_MS      600000UL
#define HAPTIC_PULSE_MS          28UL

// v6.30 AUDIO RADIO RX: Core2 is only a receiver while PAGE_AUDIO is open.
// ATOM/EchoBase starts mic streaming only after AC enable=1 and stops after AC enable=0.
// v6.36: u-law 20 ms + louder jitter queue is the speech-first path; ADPCM remains as fallback.
#define JANUS_AUDIO_LIVE_ENABLE        1
#define JANUS_AUDIO_CODEC_ULAW         1
#define JANUS_AUDIO_CODEC_ADPCM4       2
#define JANUS_AUDIO_CODEC_ACTIVE       JANUS_AUDIO_CODEC_ULAW   // v6.36: speech-first louder path
#define JANUS_AUDIO_SAMPLE_RATE        8000
#define JANUS_AUDIO_FRAME_MS           20      // u-law 20 ms = 160 bytes, fits one ESP-NOW frame
#define JANUS_AUDIO_FRAME_SAMPLES      160      // v6.38B: true 20 ms @ 8 kHz; avoids wasting DRAM
#define JANUS_AUDIO_FRAME_MAX_BYTES    180
#define JANUS_AUDIO_CONTROL_REPEAT_MS  450UL
#define JANUS_AUDIO_IDLE_TIMEOUT_MS    9000UL
#define JANUS_AUDIO_PLAY_VOLUME        216
#define JANUS_AUDIO_RX_GAIN_Q8         288     // v6.38: snapshot-buffer RX, TX sends buffered speech clips
#define JANUS_AUDIO_RX_QUEUE_N         64      // v6.38B: 1.28 sec deep buffer, heap/PSRAM allocated
#define JANUS_AUDIO_PLAY_CHANNEL       0
#define JANUS_AUDIO_START_BUFFER       12      // v6.38B: ~240 ms prebuffer for snapshot clips
#define JANUS_AUDIO_NOISE_GATE          0
#define JANUS_AUDIO_SOFT_LIMIT          26000
#define JANUS_AUDIO_VOLUME_MIN          32
#define JANUS_AUDIO_VOLUME_MAX          255
#define JANUS_AUDIO_VOLUME_STEP         16
#define JANUS_AUDIO_PLAY_CHUNK_FRAMES   4       // v6.38: 4x20 ms chunks after snapshot prebuffer
#define JANUS_AUDIO_SD_CAPTURE          0       // v6.35: off by default; SD writes can add live jitter
#define JANUS_AUDIO_SD_CAPTURE_MAX      (8UL * 1024UL * 1024UL)
#define JANUS_AUDIO_SD_CAPTURE_PATH     "/janus/audio_radio.raw"
#define JANUS_EYE_VISION_ENABLE        1
#define JANUS_EYE_VISION_CONTROL_MS    850UL
#define JANUS_EYE_VISION_IDLE_MS       2400UL
#define JANUS_EYE_VISION_W             8
#define JANUS_EYE_VISION_H             8
#define JANUS_EYE_FRAME_PIXELS         (JANUS_EYE_VISION_W * JANUS_EYE_VISION_H)
#define JANUS_EYE_VISION_FRAME_MS      160

// v6.40 KENSHI + TACHYON PROPHECY BUS
// Matches BlindEye v2.8 / Swarm v8.30 packet ABI. This is observe-and-reply:
// it does not change SHA validity; it lets predictors exchange forecasts.
#define JANUS_TACHYON_PROPHECY_ENABLE       1
#define JANUS_TACHYON_PROPHECY_TX_MS        1800UL
#define JANUS_KENSHI_BUBBLE_TX_MS           2600UL
#define CORE2_REMOTE_PROPHECY_SLOTS         12
#define CORE2_TACHYON_HORIZON_MS            2200


uint8_t JANUS_BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

// Core2 Grove Port.A
#define CORE2_PORTA_SDA         32
#define CORE2_PORTA_SCL         33
#define SGP30_ADDR              0x58

// NAS music service used by Buzz.
static const char* JANUS_MUSIC_CURRENT_URL = "http://192.168.1.92:8095/api/music/current";
static const char* JANUS_MUSIC_NEXT_URL    = "http://192.168.1.92:8095/api/music/next";
static const char* JANUS_MUSIC_PREV_URL    = "http://192.168.1.92:8095/api/music/prev";

// Open-Meteo weather for Zaporizhzhia. No API key.
static const char* ZP_WEATHER_URL = "https://api.open-meteo.com/v1/forecast?latitude=47.85&longitude=35.12&current=temperature_2m,apparent_temperature,precipitation,weather_code,wind_speed_10m&timezone=auto";

// ========================= JANUS COLONY PACKETS =========================

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

// v6.21A / observe-first: универсальный сенсорный пакет роя.
// Buzz N3A принимает magic 'S','S' и форвардит JSON в NAS /api/swarm/sense.
// Core2 также принимает чужие S/S и показывает их в MESH как живые sensory nodes.
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
  uint8_t radio_mode;      // 1 = ESP-NOW observer/worker online
  uint8_t bt_flags;
  uint8_t palette;         // here: UI page
  uint8_t knn_label;       // local lightweight state label
  uint8_t knn_confidence;  // 0..100
  uint8_t ai_hint;
  uint8_t thermal_load;    // 0..100 proxy
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
  uint16_t flags;          // bitfield: rf/clock/thermal/air/touch/miner/display/battery
};


// v6.26: Core2 -> ATOMS3R ground orders. Keep this layout identical to ATOM GroundOps.
struct __attribute__((packed)) GroundOrderPacket {
  uint8_t magic[2];       // 'G','O'
  uint8_t version;        // 1
  uint8_t mode;           // 0 base defense, 1 hero/mecha raid
  uint8_t sector;         // galaxy/planet sector hint
  uint8_t priority;       // 0..255
  uint16_t flags;
  uint32_t mission_id;
  char target[16];
};

// v6.31: Core2 -> Zim Geek planet-side solo mission order.
// Zim keeps solo Stratum mining; this packet only drives his house-base dungeon mission loop.
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

// v6.26: Stick3S -> Core2 pilot/mecha position report.
struct __attribute__((packed)) JanusPilotLinkPacket {
  uint8_t magic[2];       // 'P','L'
  uint8_t version;        // 1
  char nodeId[24];
  uint32_t seq;
  uint8_t galaxy;
  uint8_t system;
  uint8_t sector;
  uint8_t mode;           // 0 flight, 1 docked, 2 surface/mecha
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

uint32_t core2GroundOrderSeq = 0;
uint32_t core2LastGroundOrderMs = 0;
uint32_t core2PilotLinkRx = 0;
uint32_t core2LastPilotLinkMs = 0;

// v6.31 Zim mission-control telemetry.
uint32_t core2ZimMissionSeq = 0;
uint32_t core2LastZimMissionMs = 0;
uint32_t core2ZimMissionTx = 0;
uint32_t core2ZimMissionFail = 0;
char core2ZimLastOrder[80] = "Zim Earth awaiting Core2 order";

struct __attribute__((packed)) JanusControlPacket {
  uint8_t magic[2];        // 'J','C'
  uint8_t version;         // 1
  char source[16];         // Core2Home
  char target[16];         // Buzz
  char command[16];        // play_pause / next / prev / vol_up / vol_down / volume_set
  int32_t value;
  uint32_t seq;
  uint32_t uptime_ms;
};

struct __attribute__((packed)) JanusBuzzStatusPacket {
  uint8_t magic[2];        // 'B','S'
  uint8_t version;         // 1
  char nodeId[24];         // Buzz worker name
  char track[96];          // current track from Buzz itself
  uint8_t playing;
  uint8_t paused;
  uint8_t volume;          // Audio.h 0..21
  uint8_t brightness;      // Buzz LCD/LED brightness percent
  uint32_t hashRate;
  uint32_t shares;
  uint32_t rejects;
  uint32_t bestBits;
  float diff;
  uint32_t uptime_ms;
};

// v6.30 AUDIO RADIO packets.
// Core2 -> ATOM/EchoBase: 'A','C' control. ATOM -> Core2: 'A','F' compressed audio frame.
// Codec 1 = 8-bit G.711 u-law. Codec 2 = 4-bit IMA ADPCM, self-resyncing each frame.
struct __attribute__((packed)) JanusAudioControlPacket {
  uint8_t magic[2];        // 'A','C'
  uint8_t version;         // 1
  uint8_t enable;          // 0 stop, 1 stream while Core2 is on AUDIO page
  uint8_t codec;           // 1 = u-law, 2 = ADPCM4
  uint16_t sampleRate;     // 8000
  uint16_t frameMs;        // 40 for ADPCM radio mode
  uint32_t seq;
  char source[16];         // Core2Home
  char target[16];         // EchoMic / ATOM_SWARM
};

struct __attribute__((packed)) JanusAudioFramePacket {
  uint8_t magic[2];        // 'A','F'
  uint8_t version;         // 2 for ADPCM radio mode
  uint8_t codec;           // 1 = u-law, 2 = ADPCM4
  uint16_t seq;
  uint16_t sampleRate;
  uint16_t samples;        // decoded PCM samples in this frame
  int16_t predictor;       // ADPCM starting predictor
  uint8_t stepIndex;       // ADPCM starting step index
  uint8_t flags;           // bit0: speech/open gate
  uint8_t data[JANUS_AUDIO_FRAME_MAX_BYTES];
};


struct __attribute__((packed)) JanusEyeVisionControlPacket {
  uint8_t magic[2];        // 'E','C' Core2 -> BlindEye
  uint8_t version;         // 1
  uint8_t enable;          // 0 stop, 1 send visual telemetry / frame snapshots
  uint8_t mode;            // 1 = TMOS/PIR thermal presence map
  uint16_t frameMs;        // suggested frame period
  uint32_t seq;
  char source[16];         // Core2Home
  char target[16];         // BlindEye
};

struct __attribute__((packed)) JanusEyeFramePacket {
  uint8_t magic[2];        // 'E','F' BlindEye -> Core2
  uint8_t version;         // 1
  uint8_t width;           // 1..8
  uint8_t height;          // 1..8
  uint16_t seq;
  int16_t min_x10;         // optional temperature/energy scale
  int16_t max_x10;
  uint8_t flags;           // bit0: motion, bit1: presence, bit2: synthetic/source hint
  uint8_t pixels[JANUS_EYE_FRAME_PIXELS]; // 0..255 heat/intensity
};


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
  float values[6];         // presence/audio, motion, air/temp, risk/surprise, pred error/stress, fit/mood
};

struct __attribute__((packed)) JanusTachyonProphecyPacket {
  uint8_t magic[2];        // 'T','P'
  uint8_t version;         // 1
  uint8_t flags;           // bit0=presence/audio now, bit1=motion now, bit2=alert, bit3=remote-assisted
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

// Zim -> Core2/Buzz/NAS memory packet. Keep ABI aligned with Zim Geek v3.10D.
struct __attribute__((packed)) ZimAgentMemoryPacket {
  uint8_t magic[2];        // 'Z','A'
  uint8_t version;         // 1
  uint16_t worker_id;
  char nodeId[24];
  char kind[16];
  uint32_t seq;
  uint32_t uptime_ms;
  uint32_t epoch;
  uint32_t updates;
  uint32_t accepts;
  uint32_t buzzShares;
  uint16_t reward_x1000;
  uint16_t loss_x1000;
  uint8_t policy;
  uint8_t confidence;
  uint8_t lazyMask;
  uint8_t route;
  uint8_t weaponCharge;
  uint8_t flags;
  int8_t weights_q7[60];  // ZIM_AGENT_INPUTS(12) * ZIM_AGENT_OUTPUTS(5)
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


struct __attribute__((packed)) JobPacket {
  uint8_t magic[2];        // 'J','B' Buzz -> workers
  uint8_t job_id[8];
  uint8_t header[80];      // block header template; nonce is bytes 76..79 LE
  uint32_t start_nonce;
  uint32_t range_size;
  uint8_t target[32];      // big-endian share target from Buzz
  uint32_t extranonce2;
};

struct __attribute__((packed)) ShareResponse {
  uint8_t magic[2];        // 'S','R' worker -> Buzz
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
};

struct RemoteJobState {
  bool active = false;
  uint8_t job_id[8] = {};
  uint8_t header[80] = {};
  uint8_t target[32] = {};
  uint32_t startNonce = 0;
  uint32_t rangeSize = 0;
  uint32_t nonce = 0;
  uint32_t endNonce = 0;
  uint32_t receivedAt = 0;
  uint32_t extranonce2 = 0;
  // v6.39: theta-walk cursor. nonce remains progress cursor for legacy UI,
  // actual tested nonce is generated by thetaOffset/thetaStride.
  uint32_t thetaCursor = 0;
  uint32_t thetaOffset = 0;
  uint32_t thetaStride = 1;
};

// ========================= RX QUEUE: callback короткий, без String/Serial/UI =========================

#define RX_QUEUE_N 48
#define RX_MAX_LEN 250

struct RxFrame {
  uint16_t len;
  int8_t rssi;
  uint8_t mac[6];
  uint8_t data[RX_MAX_LEN];
};

volatile uint8_t rxHead = 0;
volatile uint8_t rxTail = 0;
volatile uint32_t rxDropped = 0;
portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;
RxFrame rxQueue[RX_QUEUE_N];

void queueRxFrame(const uint8_t* data, int len, int8_t rssi, const uint8_t* mac = nullptr) {
  if (!data || len <= 0 || len > RX_MAX_LEN) return;

  portENTER_CRITICAL_ISR(&rxMux);
  uint8_t next = (uint8_t)((rxHead + 1) % RX_QUEUE_N);
  if (next == rxTail) {
    rxDropped++;
    portEXIT_CRITICAL_ISR(&rxMux);
    return;
  }

  rxQueue[rxHead].len = (uint16_t)len;
  rxQueue[rxHead].rssi = rssi;
  if (mac) memcpy(rxQueue[rxHead].mac, mac, 6);
  else memset(rxQueue[rxHead].mac, 0, 6);
  memcpy(rxQueue[rxHead].data, data, len);
  rxHead = next;
  portEXIT_CRITICAL_ISR(&rxMux);
}

bool popRxFrame(RxFrame& out) {
  portENTER_CRITICAL(&rxMux);
  if (rxTail == rxHead) {
    portEXIT_CRITICAL(&rxMux);
    return false;
  }
  out = rxQueue[rxTail];
  rxTail = (uint8_t)((rxTail + 1) % RX_QUEUE_N);
  portEXIT_CRITICAL(&rxMux);
  return true;
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onColonyRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
#else
void onColonyRecv(const uint8_t *mac, const uint8_t *data, int len)
#endif
{
  int8_t rssi = 0;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (info && info->rx_ctrl) rssi = info->rx_ctrl->rssi;
#endif
  const uint8_t* srcMac = nullptr;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (info) srcMac = info->src_addr;
#else
  srcMac = mac;
#endif
  queueRxFrame(data, len, rssi, srcMac);
}

// ========================= STATE =========================

struct RemoteNode {
  bool online = false;
  uint32_t lastMs = 0;
  char nodeId[24] = "";
  char role[12] = "";
  uint8_t mac[6] = {0,0,0,0,0,0};
  uint16_t worker = 0;
  int8_t rssi = 0;

  float entropy = 0.0f;
  float loss = 0.0f;
  float sync = 0.0f;
  float fit = 0.0f;

  // Common sensor slots
  float v0 = 0.0f;
  float v1 = 0.0f;
  float v2 = 0.0f;
  float v3 = 0.0f;
  float v4 = 0.0f;
  float v5 = 0.0f;
  float v6 = 0.0f;
  float v7 = 0.0f;

  // Heartbeat/legacy miner slots
  uint32_t hashRate = 0;
  uint32_t shares = 0;
  uint32_t rejects = 0;
  uint32_t bestBits = 0;
  float diff = 0.0f;

  uint32_t age() const {
    if (!lastMs) return 0xFFFFFFFFUL;
    return millis() - lastMs;
  }

  bool refresh() {
    online = (lastMs > 0 && millis() - lastMs < NODE_TIMEOUT_MS);
    return online;
  }

  void touch(const char* id) {
    online = true;
    lastMs = millis();
    if (id && id[0]) strlcpy(nodeId, id, sizeof(nodeId));
  }
};

RemoteNode eye;
RemoteNode beacon;
RemoteNode buzz;
RemoteNode audioNode;
RemoteNode swarm;
RemoteNode stick;
RemoteNode unknownNode;

// v6.20: universal colony registry.
// Core2 no longer needs a firmware edit when a new ESP-NOW device appears.
// Fixed nodes above stay as compatibility mirrors for existing pages/galaxy logic.
struct UniversalNode {
  bool used = false;
  bool online = false;
  uint8_t semanticSlot = 6;   // 0 eye, 1 beacon, 2 buzz, 3 audio, 4 swarm, 5 stick, 6 unknown/future
  uint32_t firstMs = 0;
  uint32_t lastMs = 0;
  uint32_t packets = 0;
  char nodeId[24] = "";
  char role[12] = "";
  uint8_t mac[6] = {0,0,0,0,0,0};
  uint16_t worker = 0;
  int8_t rssi = 0;

  float entropy = 0.0f;
  float loss = 0.0f;
  float sync = 0.0f;
  float fit = 0.0f;
  float v[8] = {0,0,0,0,0,0,0,0};

  uint32_t hashRate = 0;
  uint32_t shares = 0;
  uint32_t rejects = 0;
  uint32_t bestBits = 0;
  float diff = 0.0f;
  uint16_t targetBits = 0;
  uint16_t aiBatch = 0;
  uint8_t aiHint = 0;
  uint32_t jobAgeMs = 0;
  uint32_t uptime = 0;
};

UniversalNode colonyNodes[CORE2_MAX_COLONY_NODES];
uint8_t colonyKnownCount = 0;
uint8_t colonyOnlineCount = 0;
uint8_t colonyFutureCount = 0;
uint32_t colonyNewNodeEvents = 0;
uint32_t colonyLastRosterMs = 0;
char colonyTopNode[24] = "-";

Adafruit_SGP30 sgp;
Preferences prefs;
M5Canvas canvas(&M5.Display);

bool wifiOk = false;
bool espnowOk = false;
bool sgpReady = false;
bool speakerMuted = true;
bool canvasReady = false;

// ========================= AUDIO LIVE STATE =========================

bool janusAudioLiveUserEnabled = true;     // user toggle on AUDIO page; page gate still controls actual stream
bool janusAudioLiveSentState = false;      // last AC state sent to ATOM/EchoBase
bool janusAudioSeqSeen = false;
bool janusAudioPlayActive = false;
bool janusAudioWarmed = false;
uint32_t janusAudioControlSeq = 0;
uint32_t janusAudioLastControlMs = 0;
uint32_t janusAudioLastFrameMs = 0;
uint32_t janusAudioLastPlayMs = 0;
uint32_t janusAudioFramesRx = 0;
uint32_t janusAudioFramesQueued = 0;
uint32_t janusAudioFramesPlayed = 0;
uint32_t janusAudioFramesDropped = 0;
uint32_t janusAudioSeqGaps = 0;
uint32_t janusAudioDuplicateDrops = 0;
uint32_t janusAudioQueueOverruns = 0;
uint32_t janusAudioUnderruns = 0;
uint16_t janusAudioLastSeq = 0;
uint16_t janusAudioLastSamples = 0;
uint16_t janusAudioLastRate = JANUS_AUDIO_SAMPLE_RATE;
int8_t janusAudioLastRssi = -127;
uint8_t janusAudioQHead = 0;
uint8_t janusAudioQTail = 0;
uint8_t janusAudioQCount = 0;
uint8_t janusAudioPlayingSlot = 255;
int16_t (*janusAudioPcmQ)[JANUS_AUDIO_FRAME_SAMPLES] = nullptr;  // v6.38B: heap/PSRAM, not .bss
uint16_t janusAudioQLen[JANUS_AUDIO_RX_QUEUE_N];
uint16_t janusAudioQRate[JANUS_AUDIO_RX_QUEUE_N];
char janusAudioStatusLine[64] = "AUDIO OFF";
uint8_t janusAudioPlayVolume = JANUS_AUDIO_PLAY_VOLUME;
int16_t janusAudioPlayChunk[JANUS_AUDIO_FRAME_SAMPLES * JANUS_AUDIO_PLAY_CHUNK_FRAMES];
uint16_t janusAudioPlayChunkLen = 0;
uint16_t janusAudioPlayChunkRate = JANUS_AUDIO_SAMPLE_RATE;
uint32_t janusAudioSdCaptureBytes = 0;
uint32_t janusAudioSdCaptureWrites = 0;
bool janusAudioSdCaptureEnabled = true;
uint8_t janusAudioLastCodec = 0;

// ========================= BLIND EYE VISION STATE =========================

bool janusEyeVisionUserEnabled = true;
bool janusEyeVisionSentState = false;
bool janusEyeVisionSeqSeen = false;
uint32_t janusEyeVisionControlSeq = 0;
uint32_t janusEyeVisionLastControlMs = 0;
uint32_t janusEyeVisionLastFrameMs = 0;
uint32_t janusEyeVisionLastRealFrameMs = 0;
uint32_t janusEyeVisionLastTelemetryMs = 0;
uint32_t janusEyeVisionFramesRx = 0;
uint32_t janusEyeVisionSynthFrames = 0;
uint32_t janusEyeVisionControlFail = 0;
uint16_t janusEyeVisionLastSeq = 0;
int8_t janusEyeVisionLastRssi = -127;
uint8_t janusEyeVisionW = JANUS_EYE_VISION_W;
uint8_t janusEyeVisionH = JANUS_EYE_VISION_H;
uint8_t janusEyeVisionPixels[JANUS_EYE_FRAME_PIXELS] = {0};
uint8_t janusEyeVisionFlags = 0;
char janusEyeVisionStatusLine[72] = "EYE SENSOR OFF";

// ========================= KENSHI / TACHYON PROPHECY STATE =========================

struct Core2RemoteProphecyState {
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
  float event_eta_ms = 9999.0f;
  float future_stress = 0.0f;
  float swarm_pressure = 0.0f;
  int8_t rssi = 0;
};

Core2RemoteProphecyState core2RemoteProphecies[CORE2_REMOTE_PROPHECY_SLOTS];

uint32_t core2TachyonSeq = 0;
uint32_t core2TachyonRx = 0;
uint32_t core2TachyonTx = 0;
uint32_t core2KenshiRx = 0;
uint32_t core2KenshiTx = 0;
uint32_t core2LastTachyonTxMs = 0;
uint32_t core2LastKenshiTxMs = 0;

float core2PredPresence1 = 0.0f;
float core2PredMotion1 = 0.0f;
float core2PredPresence2 = 0.0f;
float core2PredMotion2 = 0.0f;
float core2PredPresence3 = 0.0f;
float core2PredMotion3 = 0.0f;
float core2FutureStress = 0.0f;
float core2RemotePressure = 0.0f;
float core2TachyonConfidence = 0.0f;
float core2TachyonLastPresence = 0.0f;
float core2TachyonLastMotion = 0.0f;
uint8_t core2TachyonSector = 0;
uint8_t core2TachyonPredictedSector = 0;
uint8_t core2TachyonJobState = 1;
char core2TachyonLine[96] = "TP BUS WAIT";

// ========================= ZIM COMMAND / MEMORY STATE =========================

uint32_t core2ZimAgentRx = 0;
uint32_t core2LastZimAgentMs = 0;
uint32_t core2ZimAgentArchiveRows = 0;
uint8_t core2ZimLastPolicy = 0;
uint8_t core2ZimLastConfidence = 0;
uint8_t core2ZimLastWeapon = 0;
uint8_t core2ZimLastMood = 0;
char core2ZimBrainLine[96] = "Zim memory awaiting first ZA packet";
char core2ZimThought[40] = "-";

uint16_t colonyWorkerId = 0;
uint8_t colonyPeerChannel = 0;
uint32_t colonySeq = 0;
uint32_t controlSeq = 0;
uint32_t colonyLastPeerFixMs = 0;
uint32_t colonyLastHeartbeatMs = 0;
uint32_t colonyLastEntropyMs = 0;
uint32_t core2LastSwarmSenseMs = 0;
uint32_t core2SwarmSenseSeq = 0;
uint32_t core2SwarmSenseTx = 0;
uint32_t core2SwarmSenseRx = 0;
uint32_t core2SwarmSenseBad = 0;
uint32_t core2SwarmSenseTxFail = 0;
int8_t meshScroll = 0;
uint32_t core2LastLoopStartUs = 0;
uint16_t core2LoopJitterUs = 0;
uint16_t core2LoopMaxUs = 0;
uint16_t core2TouchCounter = 0;
uint16_t core2LastTouchCounterSent = 0;
uint32_t rxPackets = 0;
uint32_t er2Packets = 0;
uint32_t heartbeatPackets = 0;

RemoteJobState coreJob;
uint32_t coreRemoteHashrate = 0;
uint32_t coreHashCounter = 0;
uint32_t coreLastHashTickMs = 0;
uint32_t coreBestBits = 0;
uint32_t coreSharesSent = 0;
uint32_t coreJobsSeen = 0;
uint32_t coreJobExpired = 0;
uint32_t coreLastShareMs = 0;
uint32_t coreLastJobMs = 0;
uint16_t coreTargetBits = 0;
char coreJobText[18] = "--------";
float coreAiSkill = 0.50f;   // теперь это "neural confidence" JGPT Slime, оставлено для совместимости пакетов.

struct CoreRamanujanThetaState {
  uint32_t studies = 0;
  uint32_t lastMs = 0;
  uint32_t lastLogMs = 0;
  uint32_t seed = 0xC0A2F00DUL;
  uint32_t stride = 1;
  uint32_t offset = 0;
  uint16_t carrIndex = 1;
  uint16_t lemma = 0;
  float q = 0.20f;
  float phi = 1.0f;
  float psi = 1.0f;
  float euler = 1.0f;
  float mock = 0.0f;
  float resonance = 0.0f;
  float confidence = 0.0f;
  char line[88] = "RAMA THETA WAIT";
};

CoreRamanujanThetaState coreTheta;

// ========================= JANUS GALAXY STATION + JGPT ADMIN =========================
// Старый SIM LOT / чистый визуализатор удалён из активного поведения.
// PAGE_SPACE оставлена как имя совместимости, но теперь это Elite-like симуляция станции:
// Core2 = центральная станция, ESP-NOW swarm = планеты/станции/пилоты, телеметрия = экономика.

#define JANUS_SPACE_W           40
#define JANUS_SPACE_H           24
#define JANUS_SPACE_MAGIC       0x53505732UL  // "SPW2"
#define JANUS_SLIME_VERSION     2
#define JANUS_SLIME_EPISODES    16
#define JANUS_PI                3.14159265f
#define JANUS_TWO_PI            6.28318531f

struct SpaceCell {
  uint8_t occ;       // препятствие / плотность среды
  uint8_t conf;      // уверенность
  uint8_t motion;    // движение
  uint8_t sound;     // звук
  uint8_t air;       // TVOC/eCO2/атмосфера
  uint8_t presence;  // присутствие / тепло / "кто-то рядом"
};

struct Pose2D {
  float x;
  float y;
  float yaw;
  float vx;
  float vy;
  float vyaw;
  float confidence;
};

struct SlimeEpisode {
  uint32_t t;
  uint8_t eventId;
  uint8_t faceIndex;
  uint8_t intensity;
  uint8_t nodeMask;
};

struct SlimeBrain {
  uint32_t magic;
  uint8_t version;
  uint32_t ticks;
  uint32_t rng;
  uint32_t lastSaveMs;

  // Микро-модель: доверие к типам сенсоров и малые предикторы состояния среды.
  float trustEye;
  float trustMic;
  float trustAir;
  float trustRssi;
  float trustSwarm;
  float predPresence;
  float predMotion;
  float predSound;
  float predAir;
  float predRisk;

  // Память состояния.
  float noveltyAvg;
  float dangerAvg;
  float comfortAvg;
  float stability;
  float curiosity;
  float empathy;
  float lastPresence;
  float lastMotion;
  float lastSound;
  float lastAir;
  float mapConfidence;

  uint8_t emotion;
  uint8_t faceIndex;
  uint8_t lastEventId;
  uint8_t episodeHead;
  SlimeEpisode episodes[JANUS_SLIME_EPISODES];
};

struct SmileEntry {
  const char* face;
  const char* mood;
  const char* hint;
};

static const SmileEntry SLIME_SMILES[] = {
  {":)",   "calm",    "map stable"},
  {"^_^",  "happy",   "swarm sync"},
  {"o_o",  "watch",   "new signal"},
  {":O",   "alert",   "motion spike"},
  {":/",   "doubt",   "low confidence"},
  {"T_T",  "bad air", "air alert"},
  {">_>",  "scan",    "tracking"},
  {"<3",   "bond",    "you tapped"},
  {"-_-",  "sleep",   "quiet room"},
  {"*_*",  "learn",   "pattern learned"},
  {"?_?",  "lost",    "need swarm"},
  {"0_0",  "wow",     "big novelty"}
};
#define SLIME_SMILE_COUNT (sizeof(SLIME_SMILES) / sizeof(SLIME_SMILES[0]))

SpaceCell spaceMap[JANUS_SPACE_H][JANUS_SPACE_W];
Pose2D selfPose;
SlimeBrain slime;
char slimeLine[64] = "JGPT slime boot";
char spaceEvent[48] = "space boot";
uint32_t spaceLastTickMs = 0;
uint32_t spaceLastSaveMs = 0;
uint32_t spaceObservationCount = 0;
float spaceNovelty = 0.0f;
float spaceMotion = 0.0f;
float spacePresence = 0.0f;
float spaceSound = 0.0f;
float spaceAir = 0.0f;
float spaceRisk = 0.0f;
float spaceConfidence = 0.0f;
float spaceUserYaw = 0.0f;
uint8_t spaceNodeMask = 0;

#define JANUS_SPACE_NODE_SLOTS 7
float spaceNodeX[JANUS_SPACE_NODE_SLOTS];
float spaceNodeY[JANUS_SPACE_NODE_SLOTS];
float spaceNodeConf[JANUS_SPACE_NODE_SLOTS];
float spaceNodeSignal[JANUS_SPACE_NODE_SLOTS];
float spaceNodeMotion[JANUS_SPACE_NODE_SLOTS];

// GALAXY STATION: при открытии вкладки Core2 снова берёт малый mining-budget,
// а визуализатор продолжает работать как пространственный мозг роя.
bool coreWorkerWasEnabled = false;
uint32_t coreWorkerStoppedMs = 0;
uint32_t coreWorkerStartedMs = 0;

uint16_t eco2 = 400;
uint16_t tvoc = 0;
uint16_t rawH2 = 0;
uint16_t rawEthanol = 0;
float airEntropy = 0.0f;
float airTrend = 0.0f;
float airScore = 0.0f;
uint32_t lastSgpAt = 0;
uint32_t lastBaselineAt = 0;
uint32_t sgpWarmupStart = 0;

char buzzTrack[96] = "waiting for Buzz status";
uint32_t buzzTrackLastMs = 0;
bool buzzDesiredPlaying = true;
uint8_t buzzDesiredVolume = 14;
uint32_t lastHttpAt = 0;

uint8_t coreBrightness = 180;
uint32_t hapticOffAt = 0;
uint32_t touchLastMs = 0;

bool weatherReady = false;
float wxTemp = 0.0f;
float wxFeels = 0.0f;
float wxWind = 0.0f;
float wxPrecip = 0.0f;
int wxCode = -1;
char wxText[24] = "weather --";
uint32_t wxLastOkMs = 0;
uint32_t lastWeatherAt = 0;

String eventLine = "boot";
String statusLine = "JANUS HOME NODE";

enum UiPage : uint8_t {
  PAGE_HOME = 0,
  PAGE_AIR,
  PAGE_EYE,
  PAGE_BEACON,
  PAGE_BUZZ,
  PAGE_SPACE,
  PAGE_AUDIO,
  PAGE_MESH,
  PAGE_RSSI,
  PAGE_SYSTEM,
  PAGE_WEATHER
};

UiPage page = PAGE_HOME;

// ========================= HELPERS =========================

float clipf(float v, float lo, float hi) {
  if (!isfinite(v)) return lo;
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

String compactU(uint32_t v) {
  char b[16];
  if (v < 1000UL) snprintf(b, sizeof(b), "%lu", (unsigned long)v);
  else if (v < 1000000UL) snprintf(b, sizeof(b), "%lu.%luK", (unsigned long)(v/1000UL), (unsigned long)((v%1000UL)/100UL));
  else snprintf(b, sizeof(b), "%lu.%luM", (unsigned long)(v/1000000UL), (unsigned long)((v%1000000UL)/100000UL));
  return String(b);
}


// ========================= JANUS GALAXY STATION SIM =========================
// PAGE_SPACE compatibility layer: on screen this is now the galaxy / station management mode.
// It is clean-room, Elite-inspired: no external assets, no copyrighted game data.

#define JANUS_GALAXY_MAGIC      0x47414C33UL  // "GAL3"
#define JANUS_GALAXY_VERSION    61
#define JANUS_GALAXY_NODES      7
#define JANUS_GALAXY_GOODS      4
#define JANUS_CREW_N            8
#define JANUS_WORKSHOP_N        5
#define JANUS_MODULE_N          9

// Forward declarations used by the in-game miner tab.
// The real implementations live below with the ESP-NOW/home telemetry helpers.
float homeEntropy();
float homeSync();
// Arduino/ESP32 C++ needs this before the JanusGalaxyStationSim class because
// several inline member methods call it before the real implementation below.
bool coreWorkerEnabled();

class JanusGalaxyStationSim {
public:
  enum Good : uint8_t { ORE = 0, FOOD = 1, DATA = 2, ENERGY = 3 };
  enum View : uint8_t { VIEW_STATION = 0, VIEW_GALAXY = 1, VIEW_OPS = 2, VIEW_FLEET = 3, VIEW_MINER = 4, VIEW_CODEX = 5, VIEW_MODULES = 6 };
  enum CrewJob : uint8_t { JOB_IDLE = 0, JOB_MINE, JOB_HAUL, JOB_BUILD, JOB_REPAIR, JOB_RESEARCH, JOB_SECURITY, JOB_TRADE, JOB_REST };
  enum CrewRole : uint8_t { ROLE_MANAGER = 0, ROLE_MINER, ROLE_ENGINEER, ROLE_SCIENTIST, ROLE_GUARD, ROLE_TRADER, ROLE_MEDIC, ROLE_CHRONICLER };
  enum StationModuleKind : uint8_t { MOD_BAR = 0, MOD_DOCKS, MOD_BAZAAR, MOD_SHIPYARD, MOD_RESEARCH, MOD_BUSINESS, MOD_HOTEL, MOD_MEDBAY, MOD_SHOPS };

  struct Planet {
    char name[14];
    char role[8];
    float angle;
    float orbit;
    float stock[JANUS_GALAXY_GOODS];
    float demand[JANUS_GALAXY_GOODS];
    float production[JANUS_GALAXY_GOODS];
    float wealth;
    float relation;
    float risk;
    float signal;
    float pulse;
    bool online;
    uint32_t lastSeen;
    int8_t rssi;
  };

  struct SaveState {
    uint32_t magic;
    uint8_t version;
    uint32_t ticks;
    uint32_t rng;
    uint8_t simSpeedIndex;   // legacy slot: now always automatic, not user adjustable
    uint8_t selected;
    uint8_t soundMuted;
    uint8_t stationLevel;
    uint32_t missionsDone;
    float credits;
    float reputation;
    float adminSkill;
    float stationHull;
    float morale;
    float cargo[JANUS_GALAXY_GOODS];
    float planetStock[JANUS_GALAXY_NODES][JANUS_GALAXY_GOODS];
    float planetRelation[JANUS_GALAXY_NODES];
    uint8_t viewMode;
    uint8_t selectedShip;
    uint16_t visualSeed;
    uint16_t newsSeed;

    // v3.3 Station Fortress layer: tiny autonomous society living inside the Core2 station.
    uint16_t colonyDay;
    uint16_t legendCount;
    float colonyFood;
    float colonyOre;
    float colonyParts;
    float colonyKnowledge;
    float colonyDanger;
    float stationOrder;
    float stationCulture;
    float crewMood[JANUS_CREW_N];
    float crewStress[JANUS_CREW_N];
    float crewSkill[JANUS_CREW_N];
    float crewStamina[JANUS_CREW_N];
    uint8_t crewJob[JANUS_CREW_N];
    uint8_t crewRole[JANUS_CREW_N];
    uint16_t crewStory[JANUS_CREW_N];
    float workshopProgress[JANUS_WORKSHOP_N];

    // v6.0 Codex / personal quest memory.
    uint8_t crewQuestStep[JANUS_CREW_N];
    float crewQuestProgress[JANUS_CREW_N];
    uint8_t allianceMask;
    uint16_t codexBattles;
    uint16_t codexContracts;
    uint16_t codexTechs;
    uint16_t codexAlliances;
    uint8_t gateInstalled;
    uint8_t gateLevel;
    uint8_t moduleLevel[JANUS_MODULE_N];
    float stationQuality;
  };

  struct Ship {
    uint8_t from;
    uint8_t to;
    uint8_t kind;      // 0 cargo, 1 scout, 2 patrol, 3 miner, 4 courier
    uint8_t state;     // 0 outbound, 1 inbound, 2 docked
    float phase;
    float speed;
    float cargo;
    float wobble;
    float heat;
  };

  struct Crew {
    char name[12];
    uint8_t role;
    uint8_t job;
    float mood;
    float stress;
    float skill;
    float stamina;
    uint16_t story;
  };

  struct Workshop {
    char name[14];
    uint8_t kind;
    float progress;
    float glow;
  };

  Planet p[JANUS_GALAXY_NODES];
  SaveState s{};
  Ship ships[9];
  Crew crew[JANUS_CREW_N];
  Workshop workshops[JANUS_WORKSHOP_N];
  char janusLine[96] = "Янус: запускаю орбитальный менеджер";
  char missionLine[96] = "Станция ожидает сигналы роя";
  char newsLine[160] = "ГАЛАКТИЧЕСКАЯ СВОДКА: Core2 Station принимает первый доковый рейс";
  char focusLine[64] = "Фокус: станция Core2";
  char colonyLine[96] = "Колония станции: экипаж просыпается в жилом кольце";
  char legendLine[120] = "Летопись: первый цикл станции записан в ядро Януса";
  uint32_t lastMs = 0;
  uint32_t lastSaveMs = 0;
  uint32_t lastMissionMs = 0;
  uint32_t lastNewsMs = 0;
  uint32_t lastMindMs = 0;
  uint32_t lastColonyEventMs = 0;
  float sectorHeat = 0.0f;
  float tradeFlow = 0.0f;
  float galaxyConfidence = 0.0f;
  float routePhase = 0.0f;
  uint8_t routeFrom = 0;
  uint8_t routeTo = 1;

  // v3.4: ESP-NOW swarm nodes are active Entropy Wardens.
  // No new packets required: this interprets existing telemetry as game-world anti-chaos labor.
  float swarmAntiEntropy[JANUS_GALAXY_NODES]{};
  float swarmLearning[JANUS_GALAXY_NODES]{};
  float swarmEntropyPressure[JANUS_GALAXY_NODES]{};
  float swarmShield = 0.0f;
  float swarmLearningTotal = 0.0f;
  float swarmEntropyTotal = 0.0f;
  char swarmLine[120] = "Рой: узлы готовятся бороться с энтропией станции";
  float camOrbit = 0.0f;
  float camPitch = 0.0f;
  float camZoom = 1.0f;

  // v5.12: lightweight gameplay drama layer, no extra packet requirements.
  float combatFlash = 0.0f;
  float dockFlash = 0.0f;
  float contractProgress = 0.0f;
  float marketPulse = 0.0f;
  float techProgress = 0.0f;
  uint16_t pirateKills = 0;
  uint16_t contractsDone = 0;
  uint8_t activeContract = 0;
  uint8_t techUnlocked = 0;
  uint8_t contractCrew = 0;
  uint8_t contractGood = 0;
  uint8_t lastCombatOutcome = 0;
  uint8_t lastAlertType = 255;
  uint32_t lastCombatMs = 0;
  uint32_t lastContractMs = 0;
  uint32_t lastAlertMs = 0;
  char contractLine[96] = "Контракт: док ожидает первую задачу Януса";
  char combatLine[96] = "Безопасность: патрули прогревают сенсоры";
  char techLine[96] = "Технологии: лаборатория ждёт данных роя";
  char codexLine[128] = "Кодекс: летопись Януса ожидает первых глав";
  uint8_t selectedCodexCrew = 0;
  uint8_t selectedModule = 0;
  bool moduleDetailOpen = false;
  bool perfMode = true;              // target: never fall below ~35 FPS on Core2
  float fpsSmoothGame = 40.0f;
  uint32_t lastPerfModeMs = 0;
  uint32_t lastQuestCheckMs = 0;


  // v6.23 Unified Janus Universe layer.
  // This is intentionally runtime/lightweight and compile-safe: no new external protocol required yet.
  // Core2 acts as RTS station command, Stick/Elite is interpreted as the Janus pilot, and each ESP-NOW
  // node feeds a sector with signal, resources, threat and construction pressure. The schema is simple
  // enough to mirror later into Stick or NAS as the shared game database.
  static const uint8_t UNIVERSE_SECTORS = 16;
  uint8_t universeOwner[UNIVERSE_SECTORS] = {};       // 0 neutral, 1 Janus, 2 pirates, 3 thargoids
  float universeInfluence[UNIVERSE_SECTORS] = {};     // Janus influence 0..1.5
  float universeThreat[UNIVERSE_SECTORS] = {};        // pirate/thargoid pressure 0..1.5
  float universeSupply[UNIVERSE_SECTORS] = {};        // resource/order pressure 0..1.5
  float universeBuild[UNIVERSE_SECTORS] = {};         // station construction progress 0..1
  float universeProspect[UNIVERSE_SECTORS] = {};      // sector quality/interest 0..1
  uint8_t universeStationLevel[UNIVERSE_SECTORS] = {}; // mini outposts controlled by Janus
  uint8_t universeSelectedSector = 0;
  uint8_t universeBuildTarget = 0;
  float universeJanusInfluence = 0.0f;
  float universeThargoidPressure = 0.0f;
  float universePilotImpact = 0.0f;
  float universeTradeDemand = 0.0f;
  uint32_t universeLastDecisionMs = 0;
  uint32_t universeEpoch = 0;
  char universeLine[128] = "Universe: Core2 command awaiting swarm data";

  // v6.24: station commander layer.
  // Janus is not a static station anymore: Core2 rotates the commander focus between stations,
  // services them, orders resources, and keeps a persistent pilot location that Stick will own next.
  uint8_t universeServiceSector = 0;
  uint8_t universePilotSector = 0;
  float universeServiceProgress = 0.0f;
  float universePilotX = 0.0f;
  float universePilotY = 0.0f;
  float universePilotZ = 0.0f;
  float universePartyPower = 0.0f;
  uint32_t universePilotDistance = 0;
  uint32_t universeLastServiceSwitchMs = 0;
  char universePilotLine[128] = "Pilot: Stick link pending";
  char universeStationLine[128] = "Service: Core2 station command online";

  // v6.25: diplomacy and galaxy-cluster navigation.
  // Faction ids: 0 JANUS, 1 MIC/Moon-Echo, 2 MER/Beacon, 3 CORP/Buzz, 4 PIR, 5 THG, 6 INDEP.
  uint8_t universeFaction[UNIVERSE_SECTORS] = {};
  uint8_t galaxyClusterIndex = 0;      // 0 = real Milky Way anchor, 1..5 = Janus generated clusters
  uint8_t galaxySelectedStar = 0;
  float galaxyMapOrbit = 0.0f;
  float galaxyMapPitch = 0.0f;
  float galaxyMapZoom = 1.0f;
  uint32_t diplomacyEpoch = 0;

  static uint16_t grgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
  }

  uint16_t dim(uint16_t c, float k) {
    k = clipf(k, 0.0f, 1.0f);
    uint8_t r5 = (c >> 11) & 0x1F;
    uint8_t g6 = (c >> 5) & 0x3F;
    uint8_t b5 = c & 0x1F;
    uint8_t r = (uint8_t)clipf((float)((r5 << 3) | (r5 >> 2)) * k, 0.0f, 255.0f);
    uint8_t g = (uint8_t)clipf((float)((g6 << 2) | (g6 >> 4)) * k, 0.0f, 255.0f);
    uint8_t b = (uint8_t)clipf((float)((b5 << 3) | (b5 >> 2)) * k, 0.0f, 255.0f);
    return grgb(r, g, b);
  }

  float rnd01() {
    s.rng = s.rng * 1664525UL + 1013904223UL + rxPackets + (uint32_t)(airEntropy * 1000.0f) + s.newsSeed;
    return (float)(s.rng & 0xFFFF) / 65535.0f;
  }

  const char* goodName(uint8_t g) {
    static const char* names[4] = {"руда", "пища", "данные", "энергия"};
    return names[g & 3];
  }

  const char* techName(uint8_t t) {
    static const char* names[7] = {"нет", "Реактор", "Ангар", "Гидропоника", "Сенсорная решётка", "Лаборатория", "Антиэнтропия"};
    return names[(t <= 6) ? t : 6];
  }

  const char* contractName(uint8_t c) {
    static const char* names[5] = {"нет", "доставка", "разведка", "охота", "ремонт"};
    return names[(c <= 4) ? c : 4];
  }


  const char* universeOwnerName(uint8_t owner) {
    if (owner == 1) return "JANUS";
    if (owner == 2) return "PIR";
    if (owner == 3) return "THG";
    return "NEU";
  }

  uint16_t universeOwnerColor(uint8_t owner) {
    if (owner == 1) return pxCyan();
    if (owner == 2) return pxRed();
    if (owner == 3) return grgb(178, 96, 255);
    return grgb(112, 122, 142);
  }

  const char* factionName(uint8_t f) {
    static const char* names[7] = {"JANUS", "MIC", "MER", "CORP", "PIR", "THG", "FREE"};
    return names[(f < 7) ? f : 6];
  }

  uint16_t factionColor(uint8_t f) {
    switch (f % 7) {
      case 0: return pxCyan();
      case 1: return grgb(110, 210, 255);
      case 2: return pxGreen();
      case 3: return pxGold();
      case 4: return pxRed();
      case 5: return grgb(190, 95, 255);
      default: return grgb(170, 180, 196);
    }
  }

  uint8_t planetFaction(uint8_t planetIdx) {
    static const uint8_t f[JANUS_GALAXY_NODES] = {1, 2, 3, 1, 6, 0, 4};
    return f[planetIdx % JANUS_GALAXY_NODES];
  }

  int8_t factionAffinity(uint8_t a, uint8_t b) {
    if (a == b) return 100;
    if (a == 0) { if (b == 4 || b == 5) return -100; return 34; }
    if (b == 0) { if (a == 4 || a == 5) return -100; return 34; }
    if ((a == 4 && b == 5) || (a == 5 && b == 4)) return -82;
    if ((a == 1 && b == 3) || (a == 3 && b == 1)) return -44;
    if ((a == 2 && b == 4) || (a == 4 && b == 2)) return -58;
    if ((a == 2 && b == 5) || (a == 5 && b == 2)) return -72;
    if (a == 6 || b == 6) return 6;
    return 18;
  }

  uint8_t currentAllianceCount() {
    uint8_t n = 0;
    for (uint8_t i = 0; i < JANUS_GALAXY_NODES; i++) if (s.allianceMask & (1 << i)) n++;
    return n;
  }

  bool allianceCompatible(uint8_t idx) {
    idx %= JANUS_GALAXY_NODES;
    uint8_t nf = planetFaction(idx);
    bool already = (s.allianceMask & (1 << idx)) != 0;
    for (uint8_t i = 0; i < JANUS_GALAXY_NODES; i++) {
      if (i == idx) continue;
      if ((s.allianceMask & (1 << i)) && factionAffinity(nf, planetFaction(i)) < -55) return false;
    }
    return already || currentAllianceCount() < 3;
  }

  void applyDiplomacyRipple(uint8_t target, float delta, const char* reason) {
    target %= JANUS_GALAXY_NODES;
    uint8_t tf = planetFaction(target);
    diplomacyEpoch++;
    for (uint8_t i = 0; i < JANUS_GALAXY_NODES; i++) {
      float k = (i == target) ? 1.0f : ((float)factionAffinity(tf, planetFaction(i)) / 100.0f) * 0.52f;
      p[i].relation = clipf(p[i].relation + delta * k, -1.0f, 1.0f);
      if (k < -0.25f) p[i].risk = clipf(p[i].risk + fabsf(delta) * 0.05f, 0.02f, 1.0f);
    }
    snprintf(codexLine, sizeof(codexLine), "Diplomacy: %s %+d with %s ripples through rivals", factionName(tf), (int)(delta * 100.0f), reason ? reason : "event");
  }

  void seedUniverseFactions() {
    for (uint8_t i = 0; i < UNIVERSE_SECTORS; i++) {
      uint8_t h = hash8(i * 17 + 3, i * 29 + 9, 0xA5);
      if (i == 0 || i == 15) universeFaction[i] = 0;
      else if (h < 36) universeFaction[i] = 1;
      else if (h < 72) universeFaction[i] = 2;
      else if (h < 108) universeFaction[i] = 3;
      else if (h < 152) universeFaction[i] = 6;
      else if (h < 205) universeFaction[i] = 4;
      else universeFaction[i] = 5;
    }
  }

  uint8_t nodeSectorSlot(uint8_t nodeIdx) {
    // Stable bridge between Core2 RTS map and Stick/Elite style galaxy sectors.
    return (uint8_t)((nodeIdx * 3 + 1) % UNIVERSE_SECTORS);
  }

  const char* universePlanetName(uint8_t sector) {
    static const char* names[16] = {
      "AURORA", "DUNE", "OCEAN", "EMBER", "ICE", "TOXIC", "FOREST", "VOID",
      "CRYSTAL", "STORM", "ASH", "NEBULA", "GARDEN", "RUST", "VIOLET", "JANUS"
    };
    return names[sector & 15];
  }

  uint16_t universePlanetHi(uint8_t sector) {
    switch (sector & 15) {
      case 0: return grgb(98, 220, 255);
      case 1: return grgb(238, 176, 84);
      case 2: return grgb(64, 150, 238);
      case 3: return grgb(255, 104, 54);
      case 4: return grgb(178, 232, 255);
      case 5: return grgb(142, 255, 118);
      case 6: return grgb(88, 230, 142);
      case 7: return grgb(80, 70, 116);
      case 8: return grgb(170, 236, 255);
      case 9: return grgb(126, 128, 255);
      case 10: return grgb(178, 92, 66);
      case 11: return grgb(210, 118, 255);
      case 12: return grgb(142, 236, 156);
      case 13: return grgb(206, 118, 70);
      case 14: return grgb(190, 110, 255);
      default: return grgb(255, 218, 112);
    }
  }

  uint16_t universePlanetMid(uint8_t sector) {
    return dim(universePlanetHi(sector), 0.56f + 0.10f * ((sector & 3) == 0));
  }

  uint16_t universePlanetShade(uint8_t sector) {
    if ((sector & 15) == 7) return grgb(10, 8, 28);
    if ((sector & 15) == 11) return grgb(26, 12, 42);
    return dim(universePlanetHi(sector), 0.22f);
  }

  uint8_t chooseUniverseServiceSector() {
    float bestScore = -9999.0f;
    uint8_t best = universeServiceSector;
    for (uint8_t i = 0; i < UNIVERSE_SECTORS; i++) {
      float owned = universeOwner[i] == 1 ? 0.22f : -0.06f;
      float station = universeStationLevel[i] ? 0.25f + 0.04f * universeStationLevel[i] : 0.10f;
      float repairNeed = universeThreat[i] * 0.44f + (1.0f - clipf(universeSupply[i], 0.0f, 1.0f)) * 0.20f;
      float buildNeed = (i == universeBuildTarget) ? 0.32f : 0.0f;
      float frontier = universeProspect[i] * 0.18f + universeInfluence[i] * 0.12f;
      float score = owned + station + repairNeed + buildNeed + frontier;
      if (i == universeServiceSector) score += 0.05f;
      if (score > bestScore) { bestScore = score; best = i; }
    }
    return best;
  }

  void updateUniversePilotPosition(float dt) {
    bool pilotOnline = stick.refresh();
    float move = pilotOnline ? clipf(fabsf(stick.v0) / 260.0f + fabsf(stick.v1) / 260.0f + fabsf(stick.v2) / 360.0f + stick.fit * 0.14f, 0.0f, 1.4f) : 0.0f;
    universePartyPower = universePartyPower * 0.94f + (pilotOnline ? clipf(move + signalFromRssi(stick.rssi) * 0.35f, 0.0f, 1.5f) : 0.0f) * 0.06f;
    if (pilotOnline) {
      universePilotDistance += (uint32_t)max(1.0f, move * dt * 42.0f + signalFromRssi(stick.rssi) * dt * 8.0f);
      universePilotSector = (uint8_t)((nodeSectorSlot(5) + (universePilotDistance / 240UL)) % UNIVERSE_SECTORS);
      float a = (float)universePilotSector * JANUS_TWO_PI / (float)UNIVERSE_SECTORS + (float)(universePilotDistance % 240UL) * 0.0042f;
      float r = 1.0f + 0.055f * (float)(universePilotDistance % 900UL);
      universePilotX = cosf(a) * r;
      universePilotY = sinf(a) * r;
      universePilotZ = (float)((int)(universePilotSector % 5) - 2) * 0.35f + stick.v2 * 0.002f;
      snprintf(universePilotLine, sizeof(universePilotLine), "Pilot: Stick/Janus S%02u %s %.1fkly party %.0f%%", universePilotSector, universePlanetName(universePilotSector), (float)universePilotDistance / 1000.0f, universePartyPower * 100.0f);
    } else {
      snprintf(universePilotLine, sizeof(universePilotLine), "Pilot: last S%02u %s %.1fkly, waiting Stick", universePilotSector, universePlanetName(universePilotSector), (float)universePilotDistance / 1000.0f);
    }
  }

  void updateUniverseService(float dt, bool foreground) {
    uint32_t now = millis();
    if (now - universeLastServiceSwitchMs > 15000UL || universeServiceSector >= UNIVERSE_SECTORS) {
      uint8_t next = chooseUniverseServiceSector();
      if (next != universeServiceSector) universeServiceProgress = 0.0f;
      universeServiceSector = next;
      universeSelectedSector = next;
      universeLastServiceSwitchMs = now;
      snprintf(universeStationLine, sizeof(universeStationLine), "Service: Janus moved command to S%02u %s", next, universePlanetName(next));
    }

    uint8_t sidx = universeServiceSector % UNIVERSE_SECTORS;
    float stationBonus = 0.08f + 0.035f * universeStationLevel[sidx] + swarmShield * 0.08f + (foreground ? 0.055f : 0.018f);
    float rate = (0.0025f + stationBonus + universeSupply[sidx] * 0.0045f - universeThreat[sidx] * 0.0020f) * dt;
    universeServiceProgress = clipf(universeServiceProgress + rate, 0.0f, 1.0f);
    universeInfluence[sidx] = clipf(universeInfluence[sidx] + (0.0040f + stationBonus * 0.010f) * dt, 0.0f, 1.5f);
    universeSupply[sidx] = clipf(universeSupply[sidx] + (0.0020f + s.cargo[ENERGY] * 0.000006f + s.cargo[ORE] * 0.000005f) * dt, 0.0f, 1.5f);
    universeThreat[sidx] = clipf(universeThreat[sidx] - (0.0020f + universePartyPower * 0.0022f + stationBonus * 0.0030f) * dt, 0.0f, 1.5f);
    if (universeServiceProgress >= 1.0f) {
      universeServiceProgress = 0.0f;
      if (universeOwner[sidx] != 1) universeOwner[sidx] = 1;
      else if (universeStationLevel[sidx] < 5 && universeSupply[sidx] > 0.25f && universeThreat[sidx] < 0.72f) universeStationLevel[sidx]++;
      universeInfluence[sidx] = clipf(universeInfluence[sidx] + 0.10f, 0.0f, 1.5f);
      universeSupply[sidx] = clipf(universeSupply[sidx] - 0.06f, 0.0f, 1.5f);
      s.credits = max(0.0f, s.credits - 8.0f);
      snprintf(universeStationLine, sizeof(universeStationLine), "Service: S%02u %s upgraded/secured Lv.%u", sidx, universePlanetName(sidx), universeStationLevel[sidx]);
      snprintf(newsLine, sizeof(newsLine), "UNIVERSE: Janus serviced S%02u %s, stations linked to swarm economy", sidx, universePlanetName(sidx));
      chooseUniverseBuildTarget();
      playAlert(3);
    } else {
      snprintf(universeStationLine, sizeof(universeStationLine), "Service: S%02u %s %.0f%% sup %.0f%% thr %.0f%%", sidx, universePlanetName(sidx), universeServiceProgress * 100.0f, universeSupply[sidx] * 100.0f, universeThreat[sidx] * 100.0f);
    }
  }

  void resetUniverseLayer() {
    for (uint8_t i = 0; i < UNIVERSE_SECTORS; i++) {
      universeOwner[i] = 0;
      universeInfluence[i] = 0.08f + (float)((i * 17 + 5) & 15) / 200.0f;
      universeThreat[i] = 0.05f + (float)((i * 11 + 3) & 7) / 60.0f;
      universeSupply[i] = 0.12f + (float)((i * 13 + 9) & 15) / 120.0f;
      universeBuild[i] = 0.0f;
      universeProspect[i] = 0.20f + (float)((i * 19 + 7) & 15) / 40.0f;
      universeStationLevel[i] = 0;
    }
    seedUniverseFactions();
    universeOwner[0] = 1;
    universeFaction[0] = 0;
    universeStationLevel[0] = 1;
    universeInfluence[0] = 0.65f;
    universeBuildTarget = 0;
    universeSelectedSector = 0;
    universeServiceSector = 0;
    universePilotSector = nodeSectorSlot(5);
    universePilotDistance = 0;
    universeServiceProgress = 0.0f;
    universePartyPower = 0.0f;
    snprintf(universeLine, sizeof(universeLine), "Universe: Core2 Station claims sector 00; Stick pilot link pending");
    snprintf(universeStationLine, sizeof(universeStationLine), "Service: Janus starts at Core2 Station S00");
    snprintf(universePilotLine, sizeof(universePilotLine), "Pilot: Stick/Janus spawn S%02u", universePilotSector);
  }

  void loadUniverseLayer() {
    uint32_t magic = prefs.getUInt("univMagic", 0);
    if (magic != 0x554E4931UL) { resetUniverseLayer(); saveUniverseLayer(true); return; }
    prefs.getBytes("univOwn", universeOwner, sizeof(universeOwner));
    prefs.getBytes("univLvl", universeStationLevel, sizeof(universeStationLevel));
    prefs.getBytes("univInf", universeInfluence, sizeof(universeInfluence));
    prefs.getBytes("univThr", universeThreat, sizeof(universeThreat));
    prefs.getBytes("univSup", universeSupply, sizeof(universeSupply));
    prefs.getBytes("univBld", universeBuild, sizeof(universeBuild));
    if (prefs.getBytesLength("univFac") == sizeof(universeFaction)) prefs.getBytes("univFac", universeFaction, sizeof(universeFaction));
    else seedUniverseFactions();
    universeBuildTarget = prefs.getUChar("univTarget", 0) % UNIVERSE_SECTORS;
    universeSelectedSector = prefs.getUChar("univSel", universeBuildTarget) % UNIVERSE_SECTORS;
    universeServiceSector = prefs.getUChar("univSvc", universeSelectedSector) % UNIVERSE_SECTORS;
    universePilotSector = prefs.getUChar("univPilot", nodeSectorSlot(5)) % UNIVERSE_SECTORS;
    universePilotDistance = prefs.getULong("univPDist", 0);
    universeServiceProgress = prefs.getFloat("univSvcProg", 0.0f);
    galaxyClusterIndex = prefs.getUChar("galClus", 0) % 6;
    galaxySelectedStar = prefs.getUChar("galStar", 0) % 24;
    galaxyMapOrbit = prefs.getFloat("galYaw", 0.0f);
    galaxyMapPitch = prefs.getFloat("galPitch", 0.0f);
    galaxyMapZoom = prefs.getFloat("galZoom", 1.0f);
    snprintf(universeLine, sizeof(universeLine), "Universe: shared RTS DB loaded, target S%02u", universeBuildTarget);
    snprintf(universeStationLine, sizeof(universeStationLine), "Service: restored S%02u %s", universeServiceSector, universePlanetName(universeServiceSector));
    snprintf(universePilotLine, sizeof(universePilotLine), "Pilot: restored S%02u %.1fkly", universePilotSector, (float)universePilotDistance / 1000.0f);
  }

  void saveUniverseLayer(bool force=false) {
    static uint32_t lastUniverseSaveMs = 0;
    if (!force && millis() - lastUniverseSaveMs < 30000UL) return;
    prefs.putUInt("univMagic", 0x554E4931UL);
    prefs.putBytes("univOwn", universeOwner, sizeof(universeOwner));
    prefs.putBytes("univLvl", universeStationLevel, sizeof(universeStationLevel));
    prefs.putBytes("univInf", universeInfluence, sizeof(universeInfluence));
    prefs.putBytes("univThr", universeThreat, sizeof(universeThreat));
    prefs.putBytes("univSup", universeSupply, sizeof(universeSupply));
    prefs.putBytes("univBld", universeBuild, sizeof(universeBuild));
    prefs.putBytes("univFac", universeFaction, sizeof(universeFaction));
    prefs.putUChar("univTarget", universeBuildTarget);
    prefs.putUChar("univSel", universeSelectedSector);
    prefs.putUChar("univSvc", universeServiceSector);
    prefs.putUChar("univPilot", universePilotSector);
    prefs.putULong("univPDist", universePilotDistance);
    prefs.putFloat("univSvcProg", universeServiceProgress);
    prefs.putUChar("galClus", galaxyClusterIndex);
    prefs.putUChar("galStar", galaxySelectedStar);
    prefs.putFloat("galYaw", galaxyMapOrbit);
    prefs.putFloat("galPitch", galaxyMapPitch);
    prefs.putFloat("galZoom", galaxyMapZoom);
    lastUniverseSaveMs = millis();
  }

  float nodeStationPower(uint8_t nodeIdx) {
    RemoteNode* n = nodeFor(nodeIdx);
    bool on = n->refresh();
    float sig = on ? signalFromRssi(n->rssi) : 0.0f;
    float mining = on ? clipf((float)n->hashRate / 6000.0f, 0.0f, 1.2f) : 0.0f;
    float entropy = on ? clipf(n->entropy * 0.050f, 0.0f, 1.0f) : 0.0f;
    float roleBoost = 0.0f;
    if (nodeIdx == 2) roleBoost += 0.30f; // Buzz forge/master
    if (nodeIdx == 3) roleBoost += 0.18f; // Echo/Pyramid audio moon
    if (nodeIdx == 5) roleBoost += 0.24f; // Stick pilot
    return clipf(sig * 0.52f + mining * 0.22f + entropy * 0.12f + roleBoost, 0.0f, 1.5f);
  }

  void chooseUniverseBuildTarget() {
    float bestScore = -9999.0f;
    uint8_t best = universeBuildTarget;
    for (uint8_t i = 0; i < UNIVERSE_SECTORS; i++) {
      float stationNeed = universeStationLevel[i] ? (0.22f - 0.035f * universeStationLevel[i]) : 0.48f;
      float score = universeProspect[i] * 0.34f + universeSupply[i] * 0.26f + universeInfluence[i] * 0.24f + stationNeed - universeThreat[i] * 0.48f;
      if (i == universeBuildTarget) score += 0.08f;
      if (score > bestScore) { bestScore = score; best = i; }
    }
    universeBuildTarget = best;
    universeSelectedSector = best;
    snprintf(universeLine, sizeof(universeLine), "Janus RTS: build order S%02u  %s  supply %.0f%% threat %.0f%%", best, universeOwnerName(universeOwner[best]), universeSupply[best]*100.0f, universeThreat[best]*100.0f);
  }

  void updateUnifiedUniverse(float dt, bool foreground) {
    universeEpoch++;
    float janusSum = 0.0f;
    float thgSum = 0.0f;
    float supplySum = 0.0f;

    // ESP-NOW devices become stations/sectors. Their telemetry literally feeds the game economy.
    for (uint8_t n = 0; n < JANUS_GALAXY_NODES; n++) {
      uint8_t sidx = nodeSectorSlot(n);
      float power = nodeStationPower(n);
      universeProspect[sidx] = universeProspect[sidx] * 0.985f + clipf(power, 0.0f, 1.0f) * 0.015f;
      universeInfluence[sidx] = clipf(universeInfluence[sidx] + (power * 0.0065f + swarmAntiEntropy[n] * 0.0045f - universeThreat[sidx] * 0.0025f) * dt, 0.0f, 1.5f);
      universeSupply[sidx] = clipf(universeSupply[sidx] + (p[n].stock[ENERGY] * 0.000010f + p[n].stock[ORE] * 0.000008f + power * 0.0020f) * dt, 0.0f, 1.5f);
      universeThreat[sidx] = clipf(universeThreat[sidx] + (p[n].risk * 0.0020f + swarmEntropyPressure[n] * 0.0030f - power * 0.0022f) * dt, 0.0f, 1.5f);
      if (power > 0.36f && universeThreat[sidx] < 0.62f && universeFaction[sidx] != 5) universeOwner[sidx] = 1;
    }

    // Stick is the first-person Janus pilot: movement/combat telemetry reduces enemy pressure.
    // Until Stick sends a native Universe packet, Core2 derives a stable real-location proxy from Stick telemetry.
    updateUniversePilotPosition(dt);
    float pilotMove = stick.refresh() ? clipf(fabsf(stick.v0) / 260.0f + fabsf(stick.v1) / 260.0f + stick.fit * 0.20f, 0.0f, 1.2f) : 0.0f;
    universePilotImpact = universePilotImpact * 0.90f + pilotMove * 0.10f;
    if (pilotMove > 0.16f) {
      uint8_t ps = universePilotSector % UNIVERSE_SECTORS;
      universeThreat[ps] = clipf(universeThreat[ps] - (0.010f + pilotMove * 0.010f) * dt, 0.0f, 1.5f);
      universeInfluence[ps] = clipf(universeInfluence[ps] + (0.006f + pilotMove * 0.004f) * dt, 0.0f, 1.5f);
      if (universeThreat[ps] < 0.82f || universeFaction[ps] == 0) universeOwner[ps] = 1;
    }
    updateUniverseService(dt, foreground);

    // Thargoid pressure uses the same cause/effect idea as Stick: risk, bad air, RF instability, lost nodes.
    float airPenalty = clipf(airScore / 6.0f, 0.0f, 1.0f);
    float lostPenalty = clipf(1.0f - galaxyConfidence, 0.0f, 1.0f);
    for (uint8_t i = 0; i < UNIVERSE_SECTORS; i++) {
      float farNoise = ((hash8(i, universeEpoch & 255, s.visualSeed) & 7) == 0) ? 0.0018f : 0.0f;
      universeThreat[i] = clipf(universeThreat[i] + (airPenalty * 0.0014f + lostPenalty * 0.0012f + farNoise - universeInfluence[i] * 0.0010f) * dt, 0.0f, 1.5f);
      if (universeThreat[i] > 0.88f && universeInfluence[i] < 0.42f) universeOwner[i] = 3;
      else if (universeThreat[i] > 0.62f && universeInfluence[i] < 0.35f) universeOwner[i] = 2;
      else if (universeInfluence[i] > universeThreat[i] + 0.18f) universeOwner[i] = 1;

      if (i == universeBuildTarget) {
        float buildRate = (0.0020f + universeSupply[i] * 0.0040f + swarmLearningTotal * 0.0020f - universeThreat[i] * 0.0025f) * dt * (foreground ? 1.35f : 0.55f);
        universeBuild[i] = clipf(universeBuild[i] + buildRate, 0.0f, 1.0f);
        if (universeBuild[i] >= 1.0f) {
          universeBuild[i] = 0.0f;
          if (universeStationLevel[i] < 5) universeStationLevel[i]++;
          universeOwner[i] = 1;
          universeInfluence[i] = clipf(universeInfluence[i] + 0.18f, 0.0f, 1.5f);
          s.credits = max(0.0f, s.credits - 28.0f);
          snprintf(newsLine, sizeof(newsLine), "UNIVERSE: Janus completed outpost S%02u Lv.%u; swarm route expanded", i, universeStationLevel[i]);
          snprintf(universeLine, sizeof(universeLine), "RTS: new station online S%02u Lv.%u, ordering resources", i, universeStationLevel[i]);
          chooseUniverseBuildTarget();
          playAlert(1);
        }
      }

      janusSum += universeInfluence[i];
      thgSum += universeThreat[i];
      supplySum += universeSupply[i];
    }
    universeJanusInfluence = clipf(janusSum / (float)UNIVERSE_SECTORS, 0.0f, 1.5f);
    universeThargoidPressure = clipf(thgSum / (float)UNIVERSE_SECTORS, 0.0f, 1.5f);
    universeTradeDemand = clipf(supplySum / (float)UNIVERSE_SECTORS, 0.0f, 1.5f);

    if (millis() - universeLastDecisionMs > 9000UL) {
      universeLastDecisionMs = millis();
      chooseUniverseBuildTarget();
    }
    saveUniverseLayer(false);
  }

  const char* questTitle(uint8_t crewIdx, uint8_t step) {
    static const char* q[JANUS_CREW_N][3] = {
      {"станция L5", "1000 кредитов", "ангар Януса"},
      {"200 руды", "3 рейса", "реактор"},
      {"корпус 90%", "детали 80", "сенсорная решётка"},
      {"знания 80", "лаборатория", "антиэнтропия"},
      {"2 победы", "опасность <25", "союзный патруль"},
      {"5 контрактов", "отношения 70", "торговый союз"},
      {"мораль 80", "еда 120", "медотсек легенды"},
      {"3 легенды", "культура 1.0", "кодекс станции"}
    };
    return q[crewIdx % JANUS_CREW_N][step % 3];
  }

  float questMetric(uint8_t crewIdx, uint8_t step) {
    switch (crewIdx % JANUS_CREW_N) {
      case 0: return step == 0 ? s.stationLevel / 5.0f : (step == 1 ? s.credits / 1000.0f : techUnlocked / 2.0f);
      case 1: return step == 0 ? s.colonyOre / 200.0f : (step == 1 ? s.missionsDone / 3.0f : techUnlocked / 1.0f);
      case 2: return step == 0 ? s.stationHull / 0.90f : (step == 1 ? s.colonyParts / 80.0f : techUnlocked / 4.0f);
      case 3: return step == 0 ? s.colonyKnowledge / 80.0f : (step == 1 ? techUnlocked / 5.0f : techUnlocked / 6.0f);
      case 4: return step == 0 ? pirateKills / 2.0f : (step == 1 ? (1.0f - clipf(s.colonyDanger / 0.25f,0,1)) : s.codexAlliances / 1.0f);
      case 5: return step == 0 ? contractsDone / 5.0f : (step == 1 ? clipf((p[s.selected % JANUS_GALAXY_NODES].relation + 1.0f) * 0.5f / 0.70f,0,1) : s.codexAlliances / 1.0f);
      case 6: return step == 0 ? s.morale / 0.80f : (step == 1 ? s.colonyFood / 120.0f : s.legendCount / 4.0f);
      default: return step == 0 ? s.legendCount / 3.0f : (step == 1 ? s.stationCulture / 1.0f : (float)(s.codexBattles + s.codexContracts + s.codexTechs) / 12.0f);
    }
  }

  void completePersonalQuest(uint8_t crewIdx) {
    uint8_t c = crewIdx % JANUS_CREW_N;
    uint8_t step = s.crewQuestStep[c];
    if (step >= 3) return;
    s.crewQuestStep[c] = step + 1;
    s.crewQuestProgress[c] = 0.0f;
    s.credits += 35.0f + c * 3.0f + step * 18.0f;
    s.colonyKnowledge += 2.0f + step;
    s.stationCulture = clipf(s.stationCulture + 0.06f, 0.0f, 5.0f);
    crew[c].skill = clipf(crew[c].skill + 0.035f, 0.0f, 5.0f);
    crew[c].mood = clipf(crew[c].mood + 0.05f, 0.0f, 1.6f);
    s.legendCount++;
    snprintf(codexLine, sizeof(codexLine), "Кодекс: %s завершил главу %u — %s", crew[c].name, (unsigned)(step + 1), questTitle(c, step));
    snprintf(newsLine, sizeof(newsLine), "GALNET: личная история %s вошла в кодекс станции", crew[c].name);
    snprintf(legendLine, sizeof(legendLine), "Летопись #%u: %s открыл главу '%s'", s.legendCount, crew[c].name, questTitle(c, step));
    playAlert(1);
  }

  void updateAlliances() {
    for (int i = 0; i < JANUS_GALAXY_NODES; i++) {
      uint8_t bit = 1 << i;
      bool allied = (s.allianceMask & bit) != 0;
      if (allied && (p[i].relation < 0.48f || !allianceCompatible(i))) {
        s.allianceMask &= ~bit;
        p[i].risk = clipf(p[i].risk + 0.025f, 0.02f, 1.0f);
        snprintf(newsLine, sizeof(newsLine), "GALNET: pact with %s cools down after rival pressure", p[i].name);
        continue;
      }
      if (!allied && p[i].relation > 0.82f && allianceCompatible(i)) {
        s.allianceMask |= bit;
        s.codexAlliances++;
        s.reputation = clipf(s.reputation + 0.035f, 0.0f, 5.0f);
        p[i].risk = clipf(p[i].risk - 0.08f, 0.02f, 1.0f);
        applyDiplomacyRipple((uint8_t)i, 0.055f, "new alliance");
        snprintf(codexLine, sizeof(codexLine), "Кодекс: союз с %s / faction %s", p[i].name, factionName(planetFaction(i)));
        snprintf(newsLine, sizeof(newsLine), "GALNET: %s allied; rivals react, diplomacy is no longer free", p[i].name);
        playAlert(3);
      }
      if (s.allianceMask & bit) {
        p[i].relation = clipf(p[i].relation + 0.00003f, -1.0f, 1.0f);
        p[i].risk = clipf(p[i].risk - 0.00005f, 0.02f, 1.0f);
      }
    }
  }

  void updatePersonalQuests() {
    if (millis() - lastQuestCheckMs < 900UL) return;
    lastQuestCheckMs = millis();
    for (int i = 0; i < JANUS_CREW_N; i++) {
      uint8_t step = s.crewQuestStep[i];
      if (step >= 3) { s.crewQuestProgress[i] = 1.0f; continue; }
      float q = clipf(questMetric(i, step), 0.0f, 1.0f);
      s.crewQuestProgress[i] = q;
      if (q >= 1.0f) completePersonalQuest(i);
    }
  }

  void playAlert(uint8_t type) {
    if (speakerMuted) return;
    uint32_t now = millis();
    if (type == lastAlertType && now - lastAlertMs < 180UL) return;
    lastAlertMs = now;
    lastAlertType = type;
    if (type == 0) M5.Speaker.tone(600, 30);       // combat ping
    else if (type == 1) M5.Speaker.tone(900, 80);  // success
    else if (type == 2) M5.Speaker.tone(300, 150); // alarm
    else M5.Speaker.tone(1000, 35);                // discovery / tech
  }

  const char* shipKind(uint8_t k) {
    static const char* names[5] = {"грузовик", "разведчик", "патруль", "майнер", "курьер"};
    return names[k % 5];
  }

  const char* roleName(uint8_t r) {
    static const char* names[8] = {"управл.", "шахтёр", "инжен.", "учёный", "страж", "торгов.", "медик", "летопис."};
    return names[r & 7];
  }

  const char* jobName(uint8_t j) {
    static const char* names[9] = {"думает", "добыча", "носит", "строит", "чинит", "исслед.", "охрана", "торг", "отдых"};
    return names[j % 9];
  }

  const char* viewName() const {
    if (s.viewMode == VIEW_GALAXY) return "КАРТА";
    if (s.viewMode == VIEW_OPS) return "ОПЕРАЦИИ";
    if (s.viewMode == VIEW_FLEET) return "ФЛОТ";
    if (s.viewMode == VIEW_MINER) return "МАЙНЕР";
    if (s.viewMode == VIEW_CODEX) return "КОДЕКС";
    if (s.viewMode == VIEW_MODULES) return "МОДУЛИ";
    return "СТАНЦИЯ";
  }

  const char* speedLabel() { return "AUTO"; }

  const char* moduleName(uint8_t idx) const {
    static const char* names[JANUS_MODULE_N] = {"Бар", "Доки", "Базар", "Верфь", "R&D", "Бизнес", "Отель", "Медпункт", "Магазины"};
    return names[idx % JANUS_MODULE_N];
  }
  const char* moduleShort(uint8_t idx) const {
    static const char* names[JANUS_MODULE_N] = {"BAR", "DOC", "BZR", "YRD", "RND", "BIZ", "HOT", "MED", "SHP"};
    return names[idx % JANUS_MODULE_N];
  }
  uint16_t moduleColor(uint8_t idx) const {
    switch (idx % JANUS_MODULE_N) {
      case MOD_BAR: return grgb(210, 132, 78);
      case MOD_DOCKS: return pxAmber();
      case MOD_BAZAAR: return pxGreen();
      case MOD_SHIPYARD: return grgb(160, 170, 210);
      case MOD_RESEARCH: return pxCyan();
      case MOD_BUSINESS: return grgb(152, 206, 170);
      case MOD_HOTEL: return grgb(220, 184, 126);
      case MOD_MEDBAY: return grgb(170, 238, 210);
      case MOD_SHOPS: return grgb(208, 150, 210);
    }
    return pxCyan();
  }
  const char* moduleDesc(uint8_t idx) const {
    static const char* d[JANUS_MODULE_N] = {
      "Бар повышает мораль и культуру станции.",
      "Доки ускоряют трафик флота и поток рейсов.",
      "Базар усиливает торговлю и доходы с рынка.",
      "Верфь ускоряет обслуживание и выпуск кораблей.",
      "R&D ускоряет исследования и tech progress.",
      "Бизнес-центр режет gate fee и усиливает репутацию.",
      "Отель повышает комфорт, поток гостей и morale.",
      "Медпункт держит корпус и экипаж в стабильности.",
      "Магазины дают бытовой комфорт и торговый бонус."
    };
    return d[idx % JANUS_MODULE_N];
  }
  float moduleBonusValue(uint8_t idx) const {
    switch (idx % JANUS_MODULE_N) {
      case MOD_BAR: return s.moduleLevel[idx] * 4.0f;
      case MOD_DOCKS: return s.moduleLevel[idx] * 2.0f;
      case MOD_BAZAAR: return s.moduleLevel[idx] * 3.0f;
      case MOD_SHIPYARD: return s.moduleLevel[idx] * 2.5f;
      case MOD_RESEARCH: return s.moduleLevel[idx] * 5.0f;
      case MOD_BUSINESS: return s.moduleLevel[idx] * 3.0f;
      case MOD_HOTEL: return s.moduleLevel[idx] * 4.0f;
      case MOD_MEDBAY: return s.moduleLevel[idx] * 4.0f;
      case MOD_SHOPS: return s.moduleLevel[idx] * 2.0f;
    }
    return 0.0f;
  }
  float moduleUpgradeCost(uint8_t idx) const {
    return 90.0f + (float)(s.moduleLevel[idx % JANUS_MODULE_N] + 1) * 58.0f + idx * 16.0f + s.stationLevel * 8.0f;
  }
  float gateInstallCost() const { return 520.0f + (float)s.stationLevel * 120.0f; }
  float gateUpgradeCost() const { return 180.0f + (float)(s.gateLevel + 1) * 180.0f; }
  float jumpFee(uint8_t from, uint8_t to) const {
    int hops = abs((int)to - (int)from) + 1;
    float fee = 16.0f + hops * (10.0f - min(6, (int)s.gateLevel));
    fee *= 1.0f - 0.03f * s.moduleLevel[MOD_BUSINESS];
    return clipf(fee, 8.0f, 120.0f);
  }
  void recomputeStationQuality() {
    float sum = 0.0f;
    for (int i = 0; i < JANUS_MODULE_N; i++) sum += (float)s.moduleLevel[i];
    float base = sum / (JANUS_MODULE_N * 5.0f);
    float gate = s.gateInstalled ? (0.08f + s.gateLevel * 0.025f) : 0.0f;
    s.stationQuality = clipf(0.12f + base * 0.78f + s.stationLevel * 0.015f + gate, 0.0f, 1.5f);
  }
  void infrastructureBonuses(float dt) {
    s.stationCulture = clipf(s.stationCulture + (0.0004f * s.moduleLevel[MOD_BAR] + 0.0003f * s.moduleLevel[MOD_HOTEL]) * dt, 0.0f, 5.0f);
    s.stationOrder = clipf(s.stationOrder + (0.00028f * s.moduleLevel[MOD_MEDBAY] + 0.00025f * s.moduleLevel[MOD_BUSINESS]) * dt, 0.0f, 1.5f);
    s.morale = clipf(s.morale + (0.00022f * s.moduleLevel[MOD_BAR] + 0.00022f * s.moduleLevel[MOD_HOTEL] + 0.00018f * s.moduleLevel[MOD_SHOPS] + 0.00016f * s.moduleLevel[MOD_MEDBAY]) * dt, 0.0f, 1.5f);
    s.colonyKnowledge += 0.0018f * s.moduleLevel[MOD_RESEARCH] * dt;
    techProgress = clipf(techProgress + 0.00042f * s.moduleLevel[MOD_RESEARCH] * dt, 0.0f, 1.0f);
    tradeFlow = tradeFlow * 0.998f + (0.0020f * (s.moduleLevel[MOD_DOCKS] + s.moduleLevel[MOD_BAZAAR] + s.moduleLevel[MOD_BUSINESS])) * dt;
  }
  void gateHubScreen(int& x, int& y) {
    int ox, oy; stationCam(ox, oy);
    x = 52 + ox / 3;
    y = 100 + oy / 4;
  }
  int pickModuleUpgrade() {
    float best = -9999.0f;
    int bestIdx = 0;
    for (int i = 0; i < JANUS_MODULE_N; i++) {
      if (s.moduleLevel[i] >= 5) continue;
      float need = 0.0f;
      if (i == MOD_DOCKS) need += 0.35f + tradeFlow * 0.04f;
      if (i == MOD_BAZAAR) need += 0.25f + (1.0f - clipf(s.credits / 1600.0f, 0.0f, 1.0f)) * 0.10f;
      if (i == MOD_SHIPYARD) need += 0.20f + (9 - (int)(s.selectedShip % 9)) * 0.01f;
      if (i == MOD_RESEARCH) need += 0.24f + (1.0f - techProgress) * 0.08f;
      if (i == MOD_BAR) need += 0.18f + (1.0f - s.morale) * 0.18f;
      if (i == MOD_BUSINESS) need += 0.18f + s.reputation * 0.03f;
      if (i == MOD_HOTEL) need += 0.12f + (1.0f - s.morale) * 0.10f;
      if (i == MOD_MEDBAY) need += 0.20f + (1.0f - s.stationHull) * 0.22f;
      if (i == MOD_SHOPS) need += 0.10f + s.stationCulture * 0.02f;
      float low = (5 - s.moduleLevel[i]) * 0.20f;
      float score = need + low - moduleUpgradeCost(i) * 0.00045f + rnd01() * 0.08f;
      if (score > best) { best = score; bestIdx = i; }
    }
    return bestIdx;
  }
  void janusManageInfrastructure() {
    recomputeStationQuality();
    if (!s.gateInstalled) {
      float cost = gateInstallCost();
      if (s.credits > cost + 90.0f && (rnd01() < 0.16f + s.adminSkill * 0.03f)) {
        s.credits -= cost;
        s.gateInstalled = 1;
        s.gateLevel = 1;
        recomputeStationQuality();
        snprintf(janusLine, sizeof(janusLine), "Янус: купил разгонные врата за %.0fcr", cost);
        snprintf(newsLine, sizeof(newsLine), "GALNET: у станции построены разгонные врата, прыжки теперь платные");
        return;
      }
    } else if (s.gateLevel < 4) {
      float cost = gateUpgradeCost();
      if (s.credits > cost + 60.0f && rnd01() < 0.11f + 0.02f * s.moduleLevel[MOD_BUSINESS]) {
        s.credits -= cost;
        s.gateLevel++;
        recomputeStationQuality();
        snprintf(janusLine, sizeof(janusLine), "Янус: модернизировал врата до Mk.%u", s.gateLevel);
        snprintf(newsLine, sizeof(newsLine), "GALNET: пропускная способность jump-gate станции выросла");
        return;
      }
    }
    int m = pickModuleUpgrade();
    float cost = moduleUpgradeCost(m);
    if (s.credits > cost + 35.0f && (rnd01() < 0.18f + 0.02f * s.adminSkill)) {
      s.credits -= cost;
      s.moduleLevel[m] = min(5, (int)s.moduleLevel[m] + 1);
      recomputeStationQuality();
      snprintf(janusLine, sizeof(janusLine), "Янус: улучшил модуль %s до Lv.%u", moduleName(m), s.moduleLevel[m]);
      snprintf(newsLine, sizeof(newsLine), "GALNET: станция расширила %s, качество станции %.0f%%", moduleName(m), clipf(s.stationQuality / 1.2f, 0.0f, 1.0f) * 100.0f);
    }
  }

  void initCrewNamesAndWorkshops() {
    const char* names[JANUS_CREW_N] = {"Мира", "Бор", "Тэс", "Кай", "Люкс", "Нора", "Вик", "Ом"};
    const char* shops[JANUS_WORKSHOP_N] = {"Реактор", "Верфь", "Гидропон", "Архив", "Мастерск"};
    for (int i = 0; i < JANUS_CREW_N; i++) strlcpy(crew[i].name, names[i], sizeof(crew[i].name));
    for (int i = 0; i < JANUS_WORKSHOP_N; i++) {
      strlcpy(workshops[i].name, shops[i], sizeof(workshops[i].name));
      workshops[i].kind = i;
      workshops[i].glow = 0.2f + i * 0.1f;
    }
  }

  void defaultColonySave() {
    s.colonyDay = 1;
    s.legendCount = 1;
    s.colonyFood = 44.0f;
    s.colonyOre = 18.0f;
    s.colonyParts = 12.0f;
    s.colonyKnowledge = 4.0f;
    s.colonyDanger = 0.12f;
    s.stationOrder = 0.58f;
    s.stationCulture = 0.12f;
    for (int i = 0; i < JANUS_CREW_N; i++) {
      s.crewRole[i] = i & 7;
      s.crewJob[i] = JOB_IDLE;
      s.crewMood[i] = 0.62f + 0.03f * (i % 3);
      s.crewStress[i] = 0.12f + 0.02f * (i % 4);
      s.crewSkill[i] = 0.25f + 0.04f * i;
      s.crewStamina[i] = 0.72f + 0.02f * (i % 2);
      s.crewStory[i] = 1 + i;
    }
    for (int i = 0; i < JANUS_WORKSHOP_N; i++) s.workshopProgress[i] = 0.05f * i;
    for (int i = 0; i < JANUS_CREW_N; i++) { s.crewQuestStep[i] = 0; s.crewQuestProgress[i] = 0.0f; }
    s.allianceMask = 0;
    s.codexBattles = 0;
    s.codexContracts = 0;
    s.codexTechs = 0;
    s.codexAlliances = 0;
  }

  void buildCrewFromSave() {
    initCrewNamesAndWorkshops();
    for (int i = 0; i < JANUS_CREW_N; i++) {
      crew[i].role = s.crewRole[i] & 7;
      crew[i].job = s.crewJob[i] % 9;
      crew[i].mood = clipf(s.crewMood[i], 0.0f, 1.4f);
      crew[i].stress = clipf(s.crewStress[i], 0.0f, 1.5f);
      crew[i].skill = clipf(s.crewSkill[i], 0.05f, 5.0f);
      crew[i].stamina = clipf(s.crewStamina[i], 0.0f, 1.2f);
      crew[i].story = s.crewStory[i];
    }
    for (int i = 0; i < JANUS_WORKSHOP_N; i++) workshops[i].progress = clipf(s.workshopProgress[i], 0.0f, 1.0f);
    recomputeStationQuality();
  }

  void begin() {
    initPlanets();
    loadUniverseLayer();
    size_t n = prefs.getBytesLength("galaxy340");
    if (n == sizeof(SaveState)) {
      prefs.getBytes("galaxy340", &s, sizeof(s));
      if (s.magic == JANUS_GALAXY_MAGIC && s.version == JANUS_GALAXY_VERSION) {
        clampSave();
        buildCrewFromSave();
        for (int i = 0; i < JANUS_GALAXY_NODES; i++) {
          for (int g = 0; g < JANUS_GALAXY_GOODS; g++) p[i].stock[g] = s.planetStock[i][g];
          p[i].relation = s.planetRelation[i];
        }
        seedShips();
        snprintf(janusLine, sizeof(janusLine), "Янус: память станции восстановлена, уровень %u", s.stationLevel);
        snprintf(newsLine, sizeof(newsLine), "ГАЛАКТИЧЕСКАЯ СВОДКА: архив станции загружен, торговые коридоры просыпаются");
        return;
      }
    }
    resetSave();
    buildCrewFromSave();
    seedShips();
    save(true);
  }

  void initPlanets() {
    const char* names[JANUS_GALAXY_NODES] = {"Sol-Eye", "Orion-Bcn", "Buzz Forge", "Echo Moon", "Swarm Ark", "Stick Runner", "Zim Earth"};
    const char* roles[JANUS_GALAXY_NODES] = {"EYE", "BCN", "BUZ", "MIC", "SW", "STK", "ZIM"};
    for (int i = 0; i < JANUS_GALAXY_NODES; i++) {
      strlcpy(p[i].name, names[i], sizeof(p[i].name));
      strlcpy(p[i].role, roles[i], sizeof(p[i].role));
      p[i].angle = (float)i * 0.897f + 0.24f;
      p[i].orbit = 34.0f + (float)((i * 19) % 62);
      p[i].wealth = 0.45f + 0.08f * i;
      p[i].relation = 0.35f + 0.04f * i;
      p[i].risk = 0.15f;
      p[i].signal = 0.0f;
      p[i].pulse = 0.0f;
      p[i].online = false;
      p[i].lastSeen = 0;
      p[i].rssi = -127;
      for (int g = 0; g < JANUS_GALAXY_GOODS; g++) {
        p[i].stock[g] = 40.0f + 11.0f * i + 7.0f * g;
        p[i].demand[g] = 0.35f + 0.12f * ((i + g) % 4);
        p[i].production[g] = 0.08f + 0.02f * ((i * 3 + g) % 5);
      }
    }
    p[0].production[DATA] += 0.45f;   p[0].demand[ENERGY] += 0.30f;
    p[1].production[FOOD] += 0.36f;   p[1].demand[DATA] += 0.22f;
    p[2].production[ENERGY] += 0.54f; p[2].production[ORE] += 0.28f;
    p[3].production[DATA] += 0.34f;   p[3].demand[ENERGY] += 0.25f;
    p[4].production[FOOD] += 0.22f;   p[4].production[DATA] += 0.22f;
    p[5].production[ENERGY] += 0.18f; p[5].demand[ORE] += 0.22f;
    p[6].production[ORE] += 0.25f;    p[6].risk += 0.18f;
  }

  void resetSave() {
    memset(&s, 0, sizeof(s));
    recomputeStationQuality();
    s.magic = JANUS_GALAXY_MAGIC;
    s.version = JANUS_GALAXY_VERSION;
    s.rng = 0xC02E2026UL ^ (uint32_t)ESP.getEfuseMac();
    s.visualSeed = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
    s.newsSeed = 1337;
    s.simSpeedIndex = 0;
    s.viewMode = VIEW_STATION;
    s.selected = 0;
    s.selectedShip = 0;
    s.stationLevel = 1;
    s.credits = 420.0f;
    s.reputation = 0.25f;
    s.adminSkill = 0.12f;
    s.stationHull = 1.0f;
    s.morale = 0.62f;
    s.cargo[ORE] = 12.0f;
    s.cargo[FOOD] = 10.0f;
    s.cargo[DATA] = 6.0f;
    s.cargo[ENERGY] = 18.0f;
    for (int i = 0; i < JANUS_GALAXY_NODES; i++) {
      for (int g = 0; g < JANUS_GALAXY_GOODS; g++) s.planetStock[i][g] = p[i].stock[g];
      s.planetRelation[i] = p[i].relation;
    }
    defaultColonySave();
    s.gateInstalled = 0;
    s.gateLevel = 0;
    for (int i = 0; i < JANUS_MODULE_N; i++) s.moduleLevel[i] = 0;
    s.moduleLevel[MOD_DOCKS] = 1;
    s.moduleLevel[MOD_BAZAAR] = 1;
    s.moduleLevel[MOD_MEDBAY] = 1;
    recomputeStationQuality();
    snprintf(janusLine, sizeof(janusLine), "Янус: беру станцию под управление");
    snprintf(missionLine, sizeof(missionLine), "Цель: связать Core2 RTS и Stick Pilot в одну вселенную");
    snprintf(colonyLine, sizeof(colonyLine), "Экипаж: восемь жителей станции ждут первых приказов Януса");
  }

  void clampSave() {
    if (s.viewMode > VIEW_MODULES) s.viewMode = VIEW_STATION;
    if (s.selected >= JANUS_GALAXY_NODES) s.selected = 0;
    if (s.selectedShip >= 9) s.selectedShip = 0;
    if (s.stationLevel < 1) s.stationLevel = 1;
    if (s.stationLevel > 18) s.stationLevel = 18;
    s.credits = clipf(s.credits, 0.0f, 999999.0f);
    s.reputation = clipf(s.reputation, 0.0f, 5.0f);
    s.adminSkill = clipf(s.adminSkill, 0.0f, 5.0f);
    s.stationHull = clipf(s.stationHull, 0.05f, 1.0f);
    s.morale = clipf(s.morale, 0.0f, 1.5f);
    s.colonyFood = clipf(s.colonyFood, 0.0f, 9999.0f);
    s.colonyOre = clipf(s.colonyOre, 0.0f, 9999.0f);
    s.colonyParts = clipf(s.colonyParts, 0.0f, 9999.0f);
    s.colonyKnowledge = clipf(s.colonyKnowledge, 0.0f, 9999.0f);
    s.colonyDanger = clipf(s.colonyDanger, 0.0f, 1.5f);
    for (int i = 0; i < JANUS_CREW_N; i++) {
      if (s.crewQuestStep[i] > 3) s.crewQuestStep[i] = 0;
      s.crewQuestProgress[i] = clipf(s.crewQuestProgress[i], 0.0f, 1.0f);
    }
    s.stationOrder = clipf(s.stationOrder, 0.0f, 1.5f);
    s.stationCulture = clipf(s.stationCulture, 0.0f, 5.0f);
    for (int i = 0; i < JANUS_CREW_N; i++) {
      s.crewJob[i] %= 9; s.crewRole[i] &= 7;
      s.crewMood[i] = clipf(s.crewMood[i], 0.0f, 1.4f);
      s.crewStress[i] = clipf(s.crewStress[i], 0.0f, 1.5f);
      s.crewSkill[i] = clipf(s.crewSkill[i], 0.05f, 5.0f);
      s.crewStamina[i] = clipf(s.crewStamina[i], 0.0f, 1.2f);
    }
    for (int i = 0; i < JANUS_WORKSHOP_N; i++) s.workshopProgress[i] = clipf(s.workshopProgress[i], 0.0f, 1.0f);
    s.gateInstalled = s.gateInstalled ? 1 : 0;
    if (s.gateLevel > 4) s.gateLevel = 4;
    for (int i = 0; i < JANUS_MODULE_N; i++) if (s.moduleLevel[i] > 5) s.moduleLevel[i] = 5;
    s.stationQuality = clipf(s.stationQuality, 0.0f, 1.5f);
    for (int g = 0; g < JANUS_GALAXY_GOODS; g++) s.cargo[g] = clipf(s.cargo[g], 0.0f, 9999.0f);
  }

  void save(bool force=false) {
    if (!force && millis() - lastSaveMs < 18000UL) return;
    for (int i = 0; i < JANUS_GALAXY_NODES; i++) {
      for (int g = 0; g < JANUS_GALAXY_GOODS; g++) s.planetStock[i][g] = p[i].stock[g];
      s.planetRelation[i] = p[i].relation;
    }
    for (int i = 0; i < JANUS_CREW_N; i++) {
      s.crewRole[i] = crew[i].role;
      s.crewJob[i] = crew[i].job;
      s.crewMood[i] = crew[i].mood;
      s.crewStress[i] = crew[i].stress;
      s.crewSkill[i] = crew[i].skill;
      s.crewStamina[i] = crew[i].stamina;
      s.crewStory[i] = crew[i].story;
    }
    for (int i = 0; i < JANUS_WORKSHOP_N; i++) s.workshopProgress[i] = workshops[i].progress;
    s.magic = JANUS_GALAXY_MAGIC;
    s.version = JANUS_GALAXY_VERSION;
    prefs.putBytes("galaxy340", &s, sizeof(s));
    lastSaveMs = millis();
  }

  RemoteNode* nodeFor(int idx) {
    switch (idx) {
      case 0: return &eye;
      case 1: return &beacon;
      case 2: return &buzz;
      case 3: return &audioNode;
      case 4: return &swarm;
      case 5: return &stick;
      default: return &unknownNode;
    }
  }

  float signalFromRssi(int8_t rssi) {
    if (rssi == 0 || rssi < -120) return 0.0f;
    return clipf(((float)rssi + 96.0f) / 55.0f, 0.0f, 1.0f);
  }

  const char* guardianName(uint8_t i) {
    static const char* names[JANUS_GALAXY_NODES] = {
      "Око-предиктор", "Нав-маяк", "Кузня-порядок", "Эхо-слух", "Ковчег-рой", "Пилот-вестник", "Тёмный сторож"
    };
    return names[i % JANUS_GALAXY_NODES];
  }

  uint16_t guardianColor(uint8_t i) {
    switch (i % JANUS_GALAXY_NODES) {
      case 0: return grgb(90, 230, 255);   // Blind Eye: perception
      case 1: return grgb(95, 255, 150);   // Beacon: routes
      case 2: return grgb(255, 190, 80);   // Buzz: production
      case 3: return grgb(205, 130, 255);  // Echo: anomalies
      case 4: return grgb(120, 160, 255);  // Swarm: cohesion
      case 5: return grgb(255, 245, 120);  // Stick: action/pilot
      default:return grgb(175, 185, 205);
    }
  }

  float telemetryMagnitude(RemoteNode* n) {
    float v = fabsf(n->v0) + fabsf(n->v1) + fabsf(n->v2);
    return clipf(v / 1800.0f, 0.0f, 1.0f);
  }

  void seedShips() {
    for (int i = 0; i < 9; i++) {
      ships[i].from = i % JANUS_GALAXY_NODES;
      ships[i].to = (i * 2 + 3) % JANUS_GALAXY_NODES;
      if (ships[i].to == ships[i].from) ships[i].to = (ships[i].to + 1) % JANUS_GALAXY_NODES;
      ships[i].kind = i % 5;
      ships[i].state = i % 3;
      ships[i].phase = (float)(i * 13 % 100) / 100.0f;
      ships[i].speed = 0.028f + 0.006f * (float)(i % 4);
      ships[i].cargo = 8.0f + (float)(i * 3);
      ships[i].wobble = (float)(i * 37 % 360) * JANUS_PI / 180.0f;
      ships[i].heat = 0.2f;
    }
  }

  void syncFromSwarm() {
    float online = 0.0f;
    float antiSum = 0.0f;
    float learnSum = 0.0f;
    float entropySum = 0.0f;
    uint8_t bestGuardian = 0;
    float bestAnti = -1.0f;

    sectorHeat = clipf(airScore * 0.12f + spaceMotion * 0.18f, 0.0f, 1.5f);
    for (int i = 0; i < JANUS_GALAXY_NODES; i++) {
      RemoteNode* n = nodeFor(i);
      bool on = n->refresh();
      p[i].online = on;
      p[i].lastSeen = n->lastMs;
      p[i].rssi = n->rssi;
      float sig = on ? signalFromRssi(n->rssi) : 0.0f;
      float sensor = on ? telemetryMagnitude(n) : 0.0f;
      float entropyPulse = on ? clipf(n->entropy * 0.055f + fabsf(n->loss) * 0.42f + sensor * 0.34f + sectorHeat * 0.10f, 0.0f, 1.6f) : 0.0f;

      // Interpretation layer: existing telemetry becomes anti-entropy work in the game world.
      // Good link + fresh sensor change = useful learning signal. High loss/noise = entropy pressure.
      float guardianBias = 1.0f;
      if (i == 0) guardianBias += sensor * 0.36f;                 // Blind Eye observes threats
      else if (i == 1) guardianBias += sig * 0.30f;                // Beacon stabilizes routes
      else if (i == 2) guardianBias += p[i].stock[ENERGY] * 0.001f;// Buzz powers order
      else if (i == 3) guardianBias += spaceSound * 0.18f;         // EchoMic maps anomalies
      else if (i == 4) guardianBias += galaxyConfidence * 0.25f;   // swarm consensus
      else if (i == 5) guardianBias += spaceMotion * 0.20f;        // Stick is active pilot input

      float anti = on ? clipf((0.20f + sig * 0.54f + sensor * 0.22f + p[i].relation * 0.06f) * guardianBias - fabsf(n->loss) * 0.18f, 0.0f, 1.35f) : swarmAntiEntropy[i] * 0.94f;
      float learn = on ? clipf(sensor * 0.38f + sig * 0.22f + anti * 0.30f + entropyPulse * 0.10f, 0.0f, 1.30f) : swarmLearning[i] * 0.92f;

      swarmAntiEntropy[i] = swarmAntiEntropy[i] * 0.82f + anti * 0.18f;
      swarmLearning[i] = swarmLearning[i] * 0.86f + learn * 0.14f;
      swarmEntropyPressure[i] = swarmEntropyPressure[i] * 0.84f + entropyPulse * 0.16f;

      p[i].signal = p[i].signal * 0.86f + sig * 0.14f;
      p[i].pulse = p[i].pulse * 0.90f + entropyPulse * 0.10f;
      if (on) online += 1.0f;

      // Entropy Wardens directly affect sector risk and relations.
      p[i].risk = clipf(p[i].risk * 0.992f + (on ? (n->loss * 0.06f + sectorHeat * 0.035f - swarmAntiEntropy[i] * 0.0065f) : 0.002f), 0.02f, 1.0f);
      float rivalDrag = ((s.allianceMask != 0) && !(s.allianceMask & (1 << i)) && !allianceCompatible(i)) ? 0.00055f : 0.0f;
      p[i].relation = clipf(p[i].relation * 0.9994f + (on ? 0.00045f + swarmAntiEntropy[i] * 0.00025f - rivalDrag : -0.00025f), -1.0f, 1.0f);

      antiSum += swarmAntiEntropy[i];
      learnSum += swarmLearning[i];
      entropySum += swarmEntropyPressure[i];
      if (swarmAntiEntropy[i] > bestAnti) { bestAnti = swarmAntiEntropy[i]; bestGuardian = i; }
    }
    swarmShield = clipf(antiSum / (float)JANUS_GALAXY_NODES, 0.0f, 1.3f);
    swarmLearningTotal = clipf(learnSum / (float)JANUS_GALAXY_NODES, 0.0f, 1.3f);
    swarmEntropyTotal = clipf(entropySum / (float)JANUS_GALAXY_NODES, 0.0f, 1.6f);

    galaxyConfidence = clipf(online / (float)JANUS_GALAXY_NODES * 0.60f + s.reputation * 0.10f + s.adminSkill * 0.04f + swarmShield * 0.18f, 0.0f, 1.0f);
    s.adminSkill = clipf(s.adminSkill + swarmLearningTotal * 0.00009f, 0.0f, 5.0f);
    s.stationOrder = clipf(s.stationOrder + (swarmShield - swarmEntropyTotal * 0.35f) * 0.00032f, 0.0f, 1.5f);

    snprintf(swarmLine, sizeof(swarmLine), "Рой: %s держит порядок %.0f%%, обучение %.0f%%", guardianName(bestGuardian), swarmShield * 100.0f, swarmLearningTotal * 100.0f);

    spaceConfidence = galaxyConfidence;
    spaceRisk = clipf(sectorHeat + averageRisk() * 0.7f + swarmEntropyTotal * 0.16f - swarmShield * 0.24f, 0.0f, 1.2f);
    spaceNovelty = clipf(tradeFlow * 0.03f + (1.0f - galaxyConfidence) * 0.18f + swarmLearningTotal * 0.22f, 0.0f, 1.0f);
    spacePresence = clipf(online / 5.0f, 0.0f, 1.5f);
    spaceAir = clipf(airScore / 5.0f, 0.0f, 1.5f);
    spaceSound = audioNode.refresh() ? clipf(audioNode.v0 / 1400.0f, 0.0f, 1.5f) : spaceSound * 0.96f;
    spaceMotion = clipf(stick.refresh() ? fabsf(stick.v0) / 260.0f + fabsf(stick.v1) / 260.0f : spaceMotion * 0.96f, 0.0f, 1.5f);
  }

  float averageRisk() {
    float r = 0.0f;
    for (int i = 0; i < JANUS_GALAXY_NODES; i++) r += p[i].risk;
    return r / (float)JANUS_GALAXY_NODES;
  }

  float simMul(bool foreground) {
    return foreground ? 1.20f : 0.16f;   // fixed by design: observer cannot tune the game speed
  }

  void update(bool foreground) {
    uint32_t now = millis();
    if (!lastMs) lastMs = now;
    uint32_t dtMs = now - lastMs;
    if (dtMs > 500UL) dtMs = 500UL;
    lastMs = now;
    float dt = (float)dtMs / 1000.0f * simMul(foreground);

    syncFromSwarm();
    updateUnifiedUniverse(dt, foreground);
    simulateEconomy(dt);
    simulateColony(dt, foreground);
    updateShips(dt);
    simulateLivingSystems(dt, foreground);
    janusAdminThink(dt, foreground);
    proceduralNews(false);
    routePhase += dt * 0.035f;
    if (routePhase > 100000.0f) routePhase = 0.0f;
    s.ticks++;
    save(false);
  }

  void simulateEconomy(float dt) {
    for (int i = 0; i < JANUS_GALAXY_NODES; i++) {
      float onlineBoost = p[i].online ? 1.0f + p[i].signal * 0.85f : 0.18f;
      for (int g = 0; g < JANUS_GALAXY_GOODS; g++) {
        float prod = p[i].production[g] * onlineBoost * (1.0f + p[i].relation * 0.08f);
        float use = p[i].demand[g] * (0.15f + p[i].wealth * 0.12f + p[i].risk * 0.10f);
        p[i].stock[g] = clipf(p[i].stock[g] + (prod - use) * dt, 0.0f, 9999.0f);
      }
      p[i].wealth = clipf(p[i].wealth + (p[i].online ? 0.0015f : -0.0007f) * dt + p[i].relation * 0.0004f * dt - p[i].risk * 0.0012f * dt, 0.05f, 3.0f);
      p[i].risk = clipf(p[i].risk - 0.002f * dt + sectorHeat * 0.0008f * dt, 0.02f, 1.0f);
    }
    s.morale = clipf(s.morale + (galaxyConfidence - averageRisk()) * 0.0015f * dt, 0.0f, 1.5f);
    if (airScore > 4.0f) s.stationHull = clipf(s.stationHull - airScore * 0.000035f * dt, 0.05f, 1.0f);
  }

  void updateShips(float dt) {
    float trafficBonus = 0.7f + s.adminSkill * 0.08f + s.moduleLevel[MOD_DOCKS] * 0.020f + s.moduleLevel[MOD_SHIPYARD] * 0.025f + (s.gateInstalled ? 0.10f + s.gateLevel * 0.03f : 0.0f);
    for (int i = 0; i < 9; i++) {
      ships[i].phase += ships[i].speed * dt * trafficBonus;
      ships[i].heat = clipf(ships[i].heat * 0.96f + (ships[i].state == 2 ? 0.06f : 0.34f) * 0.04f, 0.0f, 1.0f);
      if (ships[i].phase >= 1.0f) {
        ships[i].phase -= 1.0f;
        if (ships[i].state == 2) {
          ships[i].state = 0;
          ships[i].to = (ships[i].from + 1 + (uint8_t)(rnd01() * 5.0f)) % JANUS_GALAXY_NODES;
          if (ships[i].to == ships[i].from) ships[i].to = (ships[i].to + 1) % JANUS_GALAXY_NODES;
        } else if (ships[i].state == 0) {
          completeShipRun(i);
          ships[i].from = ships[i].to;
          ships[i].state = 1;
          ships[i].phase = 0.0f;
        } else {
          ships[i].state = 2;
          ships[i].phase = 0.0f;
        }
      }
    }
  }

  void simulateLivingSystems(float dt, bool foreground) {
    uint32_t now = millis();
    combatFlash = clipf(combatFlash - dt * 1.6f, 0.0f, 1.0f);
    dockFlash = clipf(dockFlash - dt * 1.8f, 0.0f, 1.0f);
    marketPulse = clipf(marketPulse * 0.992f + tradeFlow * 0.00025f, 0.0f, 1.0f);
    infrastructureBonuses(dt);

    float risk = averageRisk();
    bool patrolReady = (risk > 0.48f || s.colonyDanger > 0.55f || sectorHeat > 0.55f);
    if (patrolReady && now - lastCombatMs > (foreground ? 6800UL : 23000UL)) {
      lastCombatMs = now;
      uint8_t target = (uint8_t)(rnd01() * JANUS_GALAXY_NODES) % JANUS_GALAXY_NODES;
      bool allyHelp = ((s.allianceMask & (1 << target)) != 0) || p[target].relation > 0.62f;
      float patrolPower = 0.36f + s.stationLevel * 0.018f + swarmShield * 0.34f + s.adminSkill * 0.035f;
      if (allyHelp) patrolPower += 0.15f + p[target].relation * 0.08f;
      float piratePower = p[target].risk + sectorHeat * 0.35f + rnd01() * 0.32f;
      bool elitePirate = (p[target].risk > 0.78f && rnd01() > 0.62f);
      if (elitePirate) piratePower += 0.18f;
      combatFlash = 1.0f;
      routeFrom = 0;
      routeTo = target;
      playAlert(0);
      if (patrolPower >= piratePower) {
        lastCombatOutcome = 1;
        pirateKills++;
        s.codexBattles++;
        float loot = 18.0f + p[target].risk * 44.0f + (allyHelp ? 9.0f : 0.0f);
        s.credits += loot;
        s.reputation = clipf(s.reputation + 0.012f + (allyHelp ? 0.004f : 0.0f), 0.0f, 5.0f);
        p[target].risk = clipf(p[target].risk - 0.09f - swarmShield * 0.04f, 0.02f, 1.0f);
        applyDiplomacyRipple(target, 0.024f + (allyHelp ? 0.010f : 0.0f), "anti-pirate patrol");
        s.colonyDanger = clipf(s.colonyDanger - 0.035f, 0.0f, 2.0f);
        snprintf(combatLine, sizeof(combatLine), "Бой: патруль отбил рейд у %s, трофеи +%.0fcr", p[target].name, loot);
        snprintf(newsLine, sizeof(newsLine), allyHelp ? "GALNET: союзники %s помогли патрулю Януса отбить пиратов" : "GALNET: Янус провёл анти-пиратскую операцию у %s", p[target].name);
        playAlert(1);
      } else {
        lastCombatOutcome = 2;
        s.stationHull = clipf(s.stationHull - (elitePirate ? 0.045f : 0.025f), 0.05f, 1.0f);
        s.morale = clipf(s.morale - 0.025f, 0.0f, 1.5f);
        s.colonyDanger = clipf(s.colonyDanger + 0.045f, 0.0f, 2.0f);
        p[target].risk = clipf(p[target].risk + 0.055f, 0.02f, 1.0f);
        applyDiplomacyRipple(target, -0.010f, "failed patrol");
        snprintf(combatLine, sizeof(combatLine), elitePirate ? "Бой: элитные пираты ударили по докам %s, корпус %.0f%%" : "Бой: пираты прорвались у %s, корпус станции %.0f%%", p[target].name, s.stationHull * 100.0f);
        snprintf(newsLine, sizeof(newsLine), "GALNET: тревога у %s — пираты проверяют реакцию станции", p[target].name);
        playAlert(2);
      }
    }

    // Contract board: Янус сам назначает члена экипажа и тип ресурса.
    float crewBoost = crew[contractCrew % JANUS_CREW_N].skill * 0.0011f + crew[contractCrew % JANUS_CREW_N].stamina * 0.0008f;
    float contractSpeed = 0.0038f + galaxyConfidence * 0.004f + swarmLearningTotal * 0.004f + s.adminSkill * 0.0007f + crewBoost;
    if (activeContract == 3 && pirateKills > 0) contractSpeed += 0.0015f;
    if (activeContract == 4) contractSpeed += clipf(s.colonyParts / 80.0f, 0.0f, 0.0015f);

    if (activeContract == 0 && now - lastContractMs > 7600UL) {
      activeContract = 1 + (uint8_t)(rnd01() * 4.0f);
      contractCrew = (uint8_t)(rnd01() * JANUS_CREW_N) % JANUS_CREW_N;
      contractGood = (uint8_t)(rnd01() * JANUS_GALAXY_GOODS) & 3;
      contractProgress = 0.0f;
      lastContractMs = now;
      snprintf(contractLine, sizeof(contractLine), "%s: %s / %s", crew[contractCrew].name, contractName(activeContract), goodName(contractGood));
      snprintf(missionLine, sizeof(missionLine), "Квест: %s ведёт задачу '%s'", crew[contractCrew].name, contractName(activeContract));
    }
    if (activeContract != 0) {
      contractProgress = clipf(contractProgress + contractSpeed * dt, 0.0f, 1.0f);
      if (contractProgress >= 1.0f) {
        contractsDone++;
        s.codexContracts++;
        float reward = 24.0f + activeContract * 9.0f + s.adminSkill * 3.0f + crew[contractCrew].skill * 4.0f;
        s.credits += reward;
        s.reputation = clipf(s.reputation + 0.006f + activeContract * 0.002f, 0.0f, 5.0f);
        s.colonyKnowledge += 0.18f + swarmLearningTotal * 0.2f;
        s.stationCulture = clipf(s.stationCulture + 0.015f, 0.0f, 2.0f);
        crew[contractCrew].skill = clipf(crew[contractCrew].skill + 0.022f, 0.0f, 5.0f);
        crew[contractCrew].mood = clipf(crew[contractCrew].mood + 0.035f, 0.0f, 1.6f);
        crew[contractCrew].stamina = clipf(crew[contractCrew].stamina - 0.030f, 0.0f, 1.2f);
        applyDiplomacyRipple(s.selected % JANUS_GALAXY_NODES, 0.016f, "contract");
        techProgress = clipf(techProgress + 0.09f + swarmLearningTotal * 0.04f, 0.0f, 1.0f);
        snprintf(contractLine, sizeof(contractLine), "%s завершил '%s': +%.0fcr", crew[contractCrew].name, contractName(activeContract), reward);
        snprintf(legendLine, sizeof(legendLine), "Летопись: %s сделал контракт #%u легендой дока", crew[contractCrew].name, contractsDone);
        snprintf(newsLine, sizeof(newsLine), "GALNET: %s закрыл контракт #%u, рынок повысил доверие", crew[contractCrew].name, contractsDone);
        activeContract = 0;
        contractProgress = 0.0f;
        playAlert(1);
      }
    }

    if (techProgress >= 1.0f && techUnlocked < 6) {
      techProgress = 0.0f;
      techUnlocked++;
      s.codexTechs++;
      if (s.stationLevel < 18) s.stationLevel++;
      if (techUnlocked == 1) s.stationHull = clipf(s.stationHull + 0.10f, 0.05f, 1.0f);
      else if (techUnlocked == 2) s.cargo[ENERGY] += 16.0f;
      else if (techUnlocked == 3) s.colonyFood += 24.0f;
      else if (techUnlocked == 4) swarmShield = clipf(swarmShield + 0.06f, 0.0f, 1.0f);
      else if (techUnlocked == 5) s.colonyKnowledge += 22.0f;
      else if (techUnlocked == 6) s.stationOrder = clipf(s.stationOrder + 0.12f, 0.0f, 1.5f);
      s.stationOrder = clipf(s.stationOrder + 0.08f, 0.0f, 1.5f);
      snprintf(techLine, sizeof(techLine), "Технология: открыт узел %s", techName(techUnlocked));
      snprintf(newsLine, sizeof(newsLine), "GALNET: %s — Янус обновил станционную архитектуру", techName(techUnlocked));
      playAlert(3);
    }

    updateAlliances();
    updatePersonalQuests();
  }

  void completeShipRun(int i) {
    uint8_t from = ships[i].from % JANUS_GALAXY_NODES;
    uint8_t to = ships[i].to % JANUS_GALAXY_NODES;
    uint8_t good = (ships[i].kind + from + to) & 3;
    float amount = clipf(ships[i].cargo * (0.45f + p[from].signal), 1.0f, p[from].stock[good]);
    p[from].stock[good] = clipf(p[from].stock[good] - amount, 0.0f, 9999.0f);
    p[to].stock[good] = clipf(p[to].stock[good] + amount * 0.92f, 0.0f, 9999.0f);
    float marketBoost = 1.0f + 0.026f * s.moduleLevel[MOD_BAZAAR] + 0.020f * s.moduleLevel[MOD_DOCKS] + 0.030f * s.moduleLevel[MOD_BUSINESS] + 0.018f * s.moduleLevel[MOD_SHOPS];
    float profit = amount * (1.0f + p[to].demand[good] - p[from].demand[good]) * (0.8f + s.adminSkill * 0.05f) * marketBoost;
    float fee = s.gateInstalled ? jumpFee(from, to) : 0.0f;
    if (profit > 0.0f) s.credits += max(0.0f, profit - fee);
    s.reputation = clipf(s.reputation + 0.0006f + amount * 0.00003f, 0.0f, 5.0f);
    tradeFlow = tradeFlow * 0.86f + amount * 0.14f;
    routeFrom = from;
    routeTo = to;
    snprintf(missionLine, sizeof(missionLine), s.gateInstalled ? "%s доставил %s: %s -> %s / gate fee %.0fcr" : "%s доставил %s: %s -> %s", shipKind(ships[i].kind), goodName(good), p[from].name, p[to].name, fee);
  }

  void assignCrewJob(int i) {
    uint8_t r = crew[i].role;
    if (crew[i].stamina < 0.18f || crew[i].stress > 1.10f) { crew[i].job = JOB_REST; return; }
    if (s.colonyFood < 18.0f) { crew[i].job = (r == ROLE_TRADER || r == ROLE_MEDIC) ? JOB_HAUL : JOB_TRADE; return; }
    if (s.stationHull < 0.62f && (r == ROLE_ENGINEER || r == ROLE_MANAGER)) { crew[i].job = JOB_REPAIR; return; }
    if (averageRisk() + s.colonyDanger > 0.74f && (r == ROLE_GUARD || r == ROLE_MANAGER)) { crew[i].job = JOB_SECURITY; return; }
    if (s.colonyOre < 28.0f || r == ROLE_MINER) { crew[i].job = JOB_MINE; return; }
    if (s.colonyParts < 22.0f || (r == ROLE_ENGINEER && s.credits > 140.0f)) { crew[i].job = JOB_BUILD; return; }
    if (r == ROLE_SCIENTIST || r == ROLE_CHRONICLER) { crew[i].job = JOB_RESEARCH; return; }
    if (r == ROLE_TRADER) { crew[i].job = JOB_TRADE; return; }
    crew[i].job = (rnd01() < 0.55f) ? JOB_HAUL : JOB_IDLE;
  }

  void generateColonyLegend(int i, const char* reason) {
    const char* relics[8] = {"гайку", "осколок льда", "пустой контейнер", "синий диод", "кофейный фильтр", "чип памяти", "зуб астероида", "старый болт"};
    const char* moods[6] = {"торжественно", "подозрительно", "с гордостью", "почти героически", "без свидетелей", "как будто так и надо"};
    uint8_t r = (uint8_t)(rnd01() * 8.0f) & 7;
    uint8_t m = (uint8_t)(rnd01() * 6.0f) % 6;
    crew[i].story++;
    s.legendCount++;
    s.stationCulture = clipf(s.stationCulture + 0.012f + crew[i].skill * 0.002f, 0.0f, 5.0f);
    snprintf(legendLine, sizeof(legendLine), "Летопись #%u: %s %s отметил %s и назвал %s артефактом", s.legendCount, crew[i].name, moods[m], reason, relics[r]);
    snprintf(newsLine, sizeof(newsLine), "GALNET: на Core2 Station родилась легенда — %s теперь хранит %s как символ смены", crew[i].name, relics[r]);
  }

  void simulateColony(float dt, bool foreground) {
    float foodDrain = 0.018f * dt * JANUS_CREW_N;
    s.colonyFood = clipf(s.colonyFood - foodDrain + p[1].production[FOOD] * 0.12f * dt, 0.0f, 9999.0f);
    s.colonyDanger = clipf(s.colonyDanger * 0.995f + averageRisk() * 0.006f + sectorHeat * 0.004f + swarmEntropyTotal * 0.0025f - swarmShield * 0.0065f, 0.0f, 1.5f);
    float moodSum = 0.0f;

    for (int i = 0; i < JANUS_CREW_N; i++) {
      if (((s.ticks + i * 11) % 97) == 0) assignCrewJob(i);
      float skill = 0.5f + crew[i].skill;
      switch (crew[i].job) {
        case JOB_MINE:
          s.colonyOre += 0.030f * skill * dt;
          crew[i].stamina -= 0.010f * dt;
          crew[i].stress += (s.colonyDanger + 0.04f) * 0.004f * dt;
          workshops[0].glow = 0.9f;
          break;
        case JOB_HAUL:
          s.colonyFood += 0.018f * skill * dt;
          s.colonyParts += 0.010f * skill * dt;
          crew[i].stamina -= 0.006f * dt;
          break;
        case JOB_BUILD:
          if (s.colonyOre > 0.1f) { s.colonyOre -= 0.018f * dt; s.colonyParts += 0.016f * skill * dt; }
          workshops[1].progress += 0.010f * skill * dt;
          crew[i].stamina -= 0.008f * dt;
          break;
        case JOB_REPAIR:
          if (s.colonyParts > 0.05f) { s.colonyParts -= 0.012f * dt; s.stationHull = clipf(s.stationHull + 0.0009f * skill * dt, 0.05f, 1.0f); }
          crew[i].stress += 0.002f * dt;
          break;
        case JOB_RESEARCH:
          s.colonyKnowledge += (0.018f + swarmLearningTotal * 0.010f) * skill * dt;
          s.adminSkill = clipf(s.adminSkill + 0.00013f * skill * dt, 0.0f, 5.0f);
          workshops[3].glow = 1.0f;
          break;
        case JOB_SECURITY:
          s.colonyDanger = clipf(s.colonyDanger - (0.006f + swarmShield * 0.005f) * skill * dt, 0.0f, 1.5f);
          for (int n = 0; n < JANUS_GALAXY_NODES; n++) p[n].risk = clipf(p[n].risk - 0.0007f * skill * dt, 0.02f, 1.0f);
          crew[i].stress += 0.006f * dt;
          break;
        case JOB_TRADE:
          s.credits += 0.025f * skill * dt * (1.0f + galaxyConfidence + 0.05f * s.moduleLevel[MOD_BAZAAR] + 0.04f * s.moduleLevel[MOD_BUSINESS] + 0.03f * s.moduleLevel[MOD_SHOPS]);
          tradeFlow = tradeFlow * 0.985f + 0.02f * skill;
          break;
        case JOB_REST:
          crew[i].stamina = clipf(crew[i].stamina + 0.020f * dt, 0.0f, 1.2f);
          crew[i].stress = clipf(crew[i].stress - 0.018f * dt, 0.0f, 1.5f);
          break;
        default:
          crew[i].stress = clipf(crew[i].stress - 0.003f * dt, 0.0f, 1.5f);
          break;
      }
      crew[i].skill = clipf(crew[i].skill + 0.00015f * dt + (crew[i].job == JOB_RESEARCH ? 0.00020f * dt : 0.0f) + swarmLearningTotal * 0.00008f * dt, 0.05f, 5.0f);
      crew[i].mood = clipf(0.55f + crew[i].stamina * 0.28f + s.stationOrder * 0.12f + s.stationCulture * 0.035f + swarmShield * 0.08f + s.moduleLevel[MOD_BAR] * 0.012f + s.moduleLevel[MOD_HOTEL] * 0.014f + s.moduleLevel[MOD_MEDBAY] * 0.012f - crew[i].stress * 0.33f - (s.colonyFood < 10.0f ? 0.22f : 0.0f), 0.0f, 1.4f);
      moodSum += crew[i].mood;
    }

    for (int w = 0; w < JANUS_WORKSHOP_N; w++) {
      workshops[w].progress = clipf(workshops[w].progress, 0.0f, 1.0f);
      workshops[w].glow = clipf(workshops[w].glow * 0.96f + workshops[w].progress * 0.04f, 0.0f, 1.0f);
      if (workshops[w].progress >= 1.0f) {
        workshops[w].progress = 0.0f;
        if (w == 1 && s.colonyParts > 6.0f) { s.colonyParts -= 6.0f; if (s.stationLevel < 18) s.stationLevel++; }
        else if (w == 3) s.colonyKnowledge += 1.0f;
        generateColonyLegend(w % JANUS_CREW_N, workshops[w].name);
      }
    }

    s.morale = clipf(moodSum / (float)JANUS_CREW_N, 0.0f, 1.5f);
    s.stationOrder = clipf(s.stationOrder + (s.morale - s.colonyDanger) * 0.0009f * dt, 0.0f, 1.5f);
    if (s.ticks % 720 == 0) s.colonyDay++;

    uint32_t now = millis();
    if (foreground && now - lastColonyEventMs > 5200UL) {
      lastColonyEventMs = now;
      int i = (int)(rnd01() * JANUS_CREW_N) % JANUS_CREW_N;
      snprintf(colonyLine, sizeof(colonyLine), "%s/%s: %s, настроение %.0f%%", crew[i].name, roleName(crew[i].role), jobName(crew[i].job), crew[i].mood * 100.0f);
      if (rnd01() < 0.28f) generateColonyLegend(i, jobName(crew[i].job));
    }
  }

  void janusAdminThink(float dt, bool foreground) {
    uint32_t now = millis();
    uint32_t delayMs = foreground ? 2400UL : 11000UL;
    if (now - lastMissionMs < delayMs) return;
    lastMissionMs = now;

    float bestScore = -999.0f;
    uint8_t bestFrom = 0, bestTo = 1, bestGood = 0;
    for (int a = 0; a < JANUS_GALAXY_NODES; a++) {
      for (int b = 0; b < JANUS_GALAXY_NODES; b++) if (a != b) {
        for (int g = 0; g < JANUS_GALAXY_GOODS; g++) {
          float surplus = p[a].stock[g] - 45.0f - p[a].demand[g] * 32.0f;
          float need = 55.0f + p[b].demand[g] * 45.0f - p[b].stock[g];
          float route = (p[a].signal + p[b].signal) * 0.42f + (p[a].relation + p[b].relation) * 0.16f;
          float score = surplus * 0.42f + need * 0.58f + route * 34.0f - (p[a].risk + p[b].risk) * 18.0f + rnd01() * 3.0f;
          if (score > bestScore) { bestScore = score; bestFrom = a; bestTo = b; bestGood = g; }
        }
      }
    }

    if (averageRisk() > 0.60f) {
      patrolRisk();
    } else if (s.credits > upgradeCost() && s.stationLevel < 18 && (rnd01() < 0.16f + s.adminSkill * 0.02f)) {
      upgradeStation("Янус");
    } else if (bestScore > 8.0f) {
      dispatchTrade(bestFrom, bestTo, bestGood);
    } else {
      dispatchScout();
    }
    janusManageInfrastructure();
    s.adminSkill = clipf(s.adminSkill + 0.0012f + galaxyConfidence * 0.0009f + swarmLearningTotal * 0.0011f, 0.0f, 5.0f);
  }

  float upgradeCost() { return 160.0f + (float)s.stationLevel * 92.0f; }

  void dispatchTrade(uint8_t from, uint8_t to, uint8_t good) {
    int freeShip = 0;
    float best = 999.0f;
    for (int i = 0; i < 9; i++) {
      if (ships[i].state == 2) { freeShip = i; break; }
      if (ships[i].phase < best) { best = ships[i].phase; freeShip = i; }
    }
    ships[freeShip].from = from;
    ships[freeShip].to = to;
    ships[freeShip].kind = (good == ORE) ? 3 : ((good == DATA) ? 4 : 0);
    ships[freeShip].state = 0;
    ships[freeShip].phase = 0.02f;
    ships[freeShip].cargo = 10.0f + s.stationLevel * 1.7f + p[from].signal * 14.0f;
    snprintf(janusLine, sizeof(janusLine), "Янус: отправляю %s за товаром '%s'", shipKind(ships[freeShip].kind), goodName(good));
    snprintf(missionLine, sizeof(missionLine), "Рейс: %s -> %s, приоритет экономики", p[from].name, p[to].name);
    s.missionsDone++;
  }

  void upgradeStation(const char* source) {
    float cost = upgradeCost();
    if (s.credits < cost) {
      snprintf(janusLine, sizeof(janusLine), "Янус: коплю ресурсы на модуль станции");
      return;
    }
    s.credits -= cost;
    s.stationLevel++;
    s.stationHull = clipf(s.stationHull + 0.20f, 0.05f, 1.0f);
    s.morale = clipf(s.morale + 0.05f, 0.0f, 1.5f);
    s.adminSkill = clipf(s.adminSkill + 0.035f, 0.0f, 5.0f);
    snprintf(janusLine, sizeof(janusLine), "Янус: достраиваю доковый модуль L%u", s.stationLevel);
    snprintf(missionLine, sizeof(missionLine), "Инженеры: станция расширяет склады и швартовые огни");
    if (!speakerMuted) M5.Speaker.tone(1568, 38);
    save(true);
  }

  void patrolRisk() {
    uint8_t worst = 0;
    float wr = -1.0f;
    for (int i = 0; i < JANUS_GALAXY_NODES; i++) if (p[i].risk > wr) { wr = p[i].risk; worst = i; }
    int ship = (s.selectedShip + 1) % 9;
    ships[ship].from = worst;
    ships[ship].to = 0;
    ships[ship].kind = 2;
    ships[ship].state = 0;
    ships[ship].phase = 0.03f;
    p[worst].risk = clipf(p[worst].risk - 0.032f - s.adminSkill * 0.006f, 0.02f, 1.0f);
    s.reputation = clipf(s.reputation + 0.0020f, 0.0f, 5.0f);
    snprintf(janusLine, sizeof(janusLine), "Янус: отправляю патруль в сектор %s", p[worst].name);
    snprintf(missionLine, sizeof(missionLine), "Безопасность: риск %.2f, пилоты получили маршрут", p[worst].risk);
  }

  void dispatchScout() {
    int ship = (s.selectedShip + 2) % 9;
    ships[ship].from = s.selected % JANUS_GALAXY_NODES;
    ships[ship].to = (ships[ship].from + 3 + (uint8_t)(rnd01() * 3.0f)) % JANUS_GALAXY_NODES;
    ships[ship].kind = 1;
    ships[ship].state = 0;
    ships[ship].phase = 0.02f;
    snprintf(janusLine, sizeof(janusLine), "Янус: разведчик проверяет дальние сигналы");
    snprintf(missionLine, sizeof(missionLine), "Разведка: ищем аномалии и новые торговые окна");
  }

  void proceduralNews(bool force) {
    uint32_t now = millis();
    if (!force && now - lastNewsMs < 7200UL) return;
    lastNewsMs = now;
    s.newsSeed += 17;
    uint8_t a = (uint8_t)(rnd01() * JANUS_GALAXY_NODES) % JANUS_GALAXY_NODES;
    uint8_t b = (uint8_t)(rnd01() * JANUS_GALAXY_NODES) % JANUS_GALAXY_NODES;
    uint8_t g = (uint8_t)(rnd01() * 4.0f) & 3;
    uint8_t mode = (uint8_t)(rnd01() * 16.0f) % 16;
    if (mode == 0) snprintf(newsLine, sizeof(newsLine), "ГАЛАКТИЧЕСКАЯ ГАЗЕТА: %s объявляет дефицит товара '%s', пилоты ворчат, но летят", p[a].name, goodName(g));
    else if (mode == 1) snprintf(newsLine, sizeof(newsLine), "СВОДКА: докеры Core2 Station поставили рекорд разгрузки. Янус сделал вид, что так и планировал");
    else if (mode == 2) snprintf(newsLine, sizeof(newsLine), "НОВОСТИ РОЯ: между %s и %s открыт тихий торговый коридор", p[a].name, p[b].name);
    else if (mode == 3) snprintf(newsLine, sizeof(newsLine), "АНОМАЛИЯ: датчики %s услышали странное эхо, Echo Moon просит не паниковать", p[a].name);
    else if (mode == 4) snprintf(newsLine, sizeof(newsLine), "БИРЖА: цена на '%s' прыгает после ночного рейса Stick Runner", goodName(g));
    else if (mode == 5) snprintf(newsLine, sizeof(newsLine), "КОЛОНКА МНЕНИЙ: Buzz Forge утверждает, что лучший дипломатический аргумент — хороший реактор");
    else if (mode == 6) snprintf(newsLine, sizeof(newsLine), "РЕПОРТАЖ: Янус обучает менеджеров не терять груз в гиперпространстве. Успех частичный");
    else if (mode == 7) snprintf(newsLine, sizeof(newsLine), "СВОДКА БЕЗОПАСНОСТИ: патруль проверил сектор %s, подозрительная пыль задержана", p[a].name);
    else if (mode == 8) snprintf(newsLine, sizeof(newsLine), "СТАНЦИОННАЯ ЛЕТОПИСЬ: %s завершил задачу '%s' и стал на %.0f%% опытнее", crew[a % JANUS_CREW_N].name, jobName(crew[a % JANUS_CREW_N].job), crew[a % JANUS_CREW_N].skill * 20.0f);
    else if (mode == 9) snprintf(newsLine, sizeof(newsLine), "ВНУТРЕННИЙ РЫНОК: гидропонный отсек спорит с шахтёрами о цене обеда. Янус делает вид, что это экономика");
    else if (mode == 10) snprintf(newsLine, sizeof(newsLine), "АРХИВ: %s записал легенду станции #%u. Историки роя хлопают диодами", crew[(a + b) % JANUS_CREW_N].name, s.legendCount);
    else if (mode == 12) snprintf(newsLine, sizeof(newsLine), "РОЙ ПРОТИВ ЭНТРОПИИ: %s стабилизирует хаос станции на %.0f%%", guardianName(a), swarmAntiEntropy[a] * 100.0f);
    else if (mode == 13) snprintf(newsLine, sizeof(newsLine), "ОБУЧЕНИЕ ЯНУСА: телеметрия %s дала модели новый паттерн поведения роя", guardianName(b));
    else if (mode == 14) snprintf(newsLine, sizeof(newsLine), "СЕНСОРНАЯ ВАХТА: ESP-NOW узлы спорят с энтропией. Пока побеждает порядок: %.0f%%", swarmShield * 100.0f);
    else snprintf(newsLine, sizeof(newsLine), "СЛУХИ ДОКА: кто-то видел пирата, который пытался купить страховку у Buzz Forge");
  }

  void nextView() {
    s.viewMode = (s.viewMode + 1) % 7;
    snprintf(focusLine, sizeof(focusLine), "Экран: %s", viewName());
    if (!speakerMuted) M5.Speaker.tone(1175, 22);
  }

  void prevView() {
    s.viewMode = (s.viewMode + 6) % 7;
    snprintf(focusLine, sizeof(focusLine), "Экран: %s", viewName());
    if (!speakerMuted) M5.Speaker.tone(988, 22);
  }

  void cycleSpeed() { nextView(); }    // legacy hardware path: no speed control anymore
  void adminAction() { nextView(); }   // legacy hardware path: no direct admin control anymore

  void touch(int x, int y) {
    if (y > 210 && x < 64) { prevView(); return; }
    if (y > 210 && x > 256) { nextView(); return; }
    if (s.viewMode == VIEW_STATION) touchStation(x, y);
    else if (s.viewMode == VIEW_GALAXY) touchGalaxy(x, y);
    else if (s.viewMode == VIEW_OPS) touchOps(x, y);
    else if (s.viewMode == VIEW_FLEET) touchFleet(x, y);
    else if (s.viewMode == VIEW_MINER) touchMiner(x, y);
    else if (s.viewMode == VIEW_CODEX) touchCodex(x, y);
    else touchModules(x, y);
  }

  void touchStation(int x, int y) {
    if (hypotf(x - 128.0f, y - 112.0f) < 42.0f) {
      snprintf(focusLine, sizeof(focusLine), "Фокус: Core2 Station, док L%u", s.stationLevel);
      snprintf(janusLine, sizeof(janusLine), "Янус: проверяю доки, склады и маршруты пилотов");
      return;
    }
    if (hypotf(x - 248.0f, y - 105.0f) < 55.0f) {
      s.selected = 0;
      snprintf(focusLine, sizeof(focusLine), "Фокус: планета у станции");
      snprintf(janusLine, sizeof(janusLine), "Янус: планета стабильна, орбита пригодна для торговли");
      return;
    }
    int hit = hitShip(x, y);
    if (hit >= 0) {
      s.selectedShip = hit;
      snprintf(focusLine, sizeof(focusLine), "Фокус: %s #%d", shipKind(ships[hit].kind), hit + 1);
      snprintf(janusLine, sizeof(janusLine), "Янус: слежу за рейсом %s -> %s", p[ships[hit].from].name, p[ships[hit].to].name);
      return;
    }
    // v6.25 manual camera: left/right orbit, top/bottom pitch, center tap recenters.
    // Edge bands now rotate the camera instead of feeling like zoom-only control.
    if (y > 42 && y < 205) {
      if (x < 78)  { nudgeCamera(-0.24f, 0.0f, 0.0f); return; }
      if (x > 242) { nudgeCamera( 0.24f, 0.0f, 0.0f); return; }
      if (y < 86)  { nudgeCamera(0.0f, -0.12f, 0.0f); return; }
      if (y > 168) { nudgeCamera(0.0f,  0.12f, 0.0f); return; }
      if (x > 126 && x < 194 && y > 92 && y < 158) {
        camOrbit *= 0.80f; camPitch *= 0.80f; camZoom += (1.0f - camZoom) * 0.25f;
        snprintf(focusLine, sizeof(focusLine), "Камера: центрирую обзор станции");
        return;
      }
      if (x >= 78 && x <= 126) { nudgeCamera(0.0f, 0.0f, -0.09f); return; }
      if (x >= 194 && x <= 242) { nudgeCamera(0.0f, 0.0f, 0.09f); return; }
    }
  }

  const char* galaxyClusterName(uint8_t g) {
    static const char* names[6] = {"SOL ANCHOR", "JANUS ARK", "ECHO VEIL", "BUZZ FORGE", "THARGOID RIFT", "OUTER DARK"};
    return names[g % 6];
  }

  const char* milkySystemName(uint8_t i) {
    static const char* names[18] = {
      "Sol", "Alpha Cen", "Barnard", "Sirius", "Epsilon Eri", "Tau Ceti",
      "Procyon", "Vega", "Luyten", "Ross 128", "TRAPPIST", "Wolf 359",
      "Lalande", "Gliese 876", "Kapteyn", "Kepler-22", "Teegarden", "Janus Gate"
    };
    return names[i % 18];
  }

  const char* janusSystemName(uint8_t g, uint8_t i) {
    static const char* a[8] = {"Ark", "Forge", "Echo", "Blind", "Beacon", "Pyramid", "Stick", "Slime"};
    static const char* b[8] = {"Prime", "Halo", "Dust", "Mirror", "Delta", "Node", "Aegis", "Rift"};
    static char out[18];
    snprintf(out, sizeof(out), "%s-%s", a[(i + g) & 7], b[(i * 3 + g) & 7]);
    return out;
  }

  const char* selectedGalaxySystemName() {
    if (galaxyClusterIndex == 0 && galaxySelectedStar < 18) return milkySystemName(galaxySelectedStar);
    return janusSystemName(galaxyClusterIndex, galaxySelectedStar);
  }

  uint8_t galaxyStarCount() const { return (galaxyClusterIndex == 0) ? 18 : 22; }

  uint8_t galaxyStarSector(uint8_t i) {
    if (galaxyClusterIndex == 0) return (i * 5 + 3) & 15;
    return (i * 7 + galaxyClusterIndex * 3 + 1) & 15;
  }

  uint16_t galaxyStarColor(uint8_t i) {
    if (galaxyClusterIndex == 0) {
      if (i == 0) return pxGold();
      if (i == 3 || i == 6 || i == 7) return grgb(180, 210, 255);
      if (i == 10 || i == 16) return grgb(255, 132, 86);
      return grgb(210, 230, 255);
    }
    uint8_t f = universeFaction[galaxyStarSector(i)];
    return factionColor(f);
  }

  void galaxySystemScreen(uint8_t i, int cx, int cy, int& sx, int& sy) {
    uint32_t seed = 0x9E3779B9UL ^ ((uint32_t)galaxyClusterIndex * 0xA511E9B3UL) ^ ((uint32_t)i * 0x45D9F3BUL);
    seed ^= (seed >> 16); seed *= 2246822519UL; seed ^= (seed >> 13);
    float u = (float)(seed & 0x3FF) / 1023.0f;
    float v = (float)((seed >> 10) & 0x3FF) / 1023.0f;
    float spiral = (float)i * 0.83f + (float)galaxyClusterIndex * 0.71f;
    float rad = (14.0f + v * 78.0f + (float)(i % 4) * 4.0f) * clipf(galaxyMapZoom, 0.60f, 1.52f);
    float a = u * JANUS_TWO_PI + galaxyMapOrbit + sinf(spiral) * 0.42f;
    float ell = 0.50f + 0.18f * cosf(galaxyMapPitch);
    sx = cx + (int)(cosf(a) * rad);
    sy = cy + (int)(sinf(a) * rad * ell + sinf(galaxyMapPitch) * rad * 0.22f);
  }

  void nudgeGalaxyCamera(float yaw, float pitch, float zoom) {
    galaxyMapOrbit += yaw;
    if (galaxyMapOrbit > JANUS_PI) galaxyMapOrbit -= JANUS_TWO_PI;
    if (galaxyMapOrbit < -JANUS_PI) galaxyMapOrbit += JANUS_TWO_PI;
    galaxyMapPitch = clipf(galaxyMapPitch + pitch, -1.0f, 1.0f);
    galaxyMapZoom = clipf(galaxyMapZoom + zoom, 0.62f, 1.60f);
    snprintf(focusLine, sizeof(focusLine), "Galaxy cam: yaw %.0f pitch %.0f zoom %.0f%%", galaxyMapOrbit * 57.3f, galaxyMapPitch * 57.3f, galaxyMapZoom * 100.0f);
  }

  void touchGalaxy(int x, int y) {
    if (y > 154 && y < 190) {
      if (x < 72) { galaxyClusterIndex = (galaxyClusterIndex + 5) % 6; galaxySelectedStar = 0; snprintf(focusLine, sizeof(focusLine), "Galaxy: %s", galaxyClusterName(galaxyClusterIndex)); return; }
      if (x > 248) { galaxyClusterIndex = (galaxyClusterIndex + 1) % 6; galaxySelectedStar = 0; snprintf(focusLine, sizeof(focusLine), "Galaxy: %s", galaxyClusterName(galaxyClusterIndex)); return; }
    }
    if (y > 44 && y < 154) {
      if (x < 58) { nudgeGalaxyCamera(-0.22f, 0.0f, 0.0f); return; }
      if (x > 262) { nudgeGalaxyCamera( 0.22f, 0.0f, 0.0f); return; }
      if (y < 72) { nudgeGalaxyCamera(0.0f, -0.12f, 0.0f); return; }
      if (y > 132) { nudgeGalaxyCamera(0.0f, 0.12f, 0.0f); return; }
      if (x > 126 && x < 194 && y > 82 && y < 126) { galaxyMapOrbit *= 0.85f; galaxyMapPitch *= 0.85f; galaxyMapZoom += (1.0f - galaxyMapZoom) * 0.25f; return; }
    }

    int cx = 160, cy = 105;
    float best = 99999.0f;
    int hit = -1;
    for (int i = 0; i < galaxyStarCount(); i++) {
      int px, py; galaxySystemScreen(i, cx, cy, px, py);
      float d = hypotf((float)(x - px), (float)(y - py));
      if (d < best) { best = d; hit = i; }
    }
    if (hit >= 0 && best < 18.0f) {
      galaxySelectedStar = (uint8_t)hit;
      universeSelectedSector = galaxyStarSector(galaxySelectedStar);
      s.selected = galaxySelectedStar % JANUS_GALAXY_NODES;
      snprintf(focusLine, sizeof(focusLine), "Galaxy: %s / S%02u", selectedGalaxySystemName(), universeSelectedSector);
      snprintf(janusLine, sizeof(janusLine), "Янус: выбираю систему %s, faction %s", selectedGalaxySystemName(), factionName(universeFaction[universeSelectedSector]));
      proceduralNews(true);
    }
  }

  void touchOps(int x, int y) {
    if (y > 45 && y < 180) {
      uint8_t row = (uint8_t)clipf((float)(y - 50) / 18.0f, 0.0f, (float)(JANUS_GALAXY_NODES - 1));
      s.selected = row;
      snprintf(focusLine, sizeof(focusLine), "Фокус: %s / рынок", p[row].name);
      snprintf(janusLine, sizeof(janusLine), "Янус: смотрю склад %s: O%.0f F%.0f D%.0f E%.0f", p[row].name, p[row].stock[0], p[row].stock[1], p[row].stock[2], p[row].stock[3]);
    }
  }

  void touchCodex(int x, int y) {
    if (y > 48 && y < 178) {
      selectedCodexCrew = (uint8_t)clipf((float)(y - 50) / 14.0f, 0.0f, (float)(JANUS_CREW_N - 1));
      snprintf(focusLine, sizeof(focusLine), "Кодекс: %s", crew[selectedCodexCrew].name);
      snprintf(janusLine, sizeof(janusLine), "Янус: читаю личную ветку %s, глава %u/3", crew[selectedCodexCrew].name, s.crewQuestStep[selectedCodexCrew]);
    }
  }

  void touchFleet(int x, int y) {
    if (y > 58 && y < 118) {
      uint8_t idx = (uint8_t)clipf((float)(x - 12) / 36.0f, 0.0f, 8.0f);
      s.selectedShip = idx;
      snprintf(focusLine, sizeof(focusLine), "Флот: %s #%u", shipKind(ships[idx].kind), idx + 1);
      snprintf(janusLine, sizeof(janusLine), "Янус: маршрут %s -> %s, груз %.0f", p[ships[idx].from].name, p[ships[idx].to].name, ships[idx].cargo);
      return;
    }
    if (y > 122 && y < 190) {
      uint8_t idx = (uint8_t)clipf((float)(y - 126) / 14.0f, 0.0f, (float)(JANUS_GALAXY_NODES - 1));
      s.selected = idx;
      snprintf(focusLine, sizeof(focusLine), "Рынок: %s", p[idx].name);
      snprintf(janusLine, sizeof(janusLine), "Янус: сверяю цены узла %s, риск %.0f%%", p[idx].name, p[idx].risk * 100.0f);
    }
  }

  void touchMiner(int x, int y) {
    if (x < 160) {
      snprintf(focusLine, sizeof(focusLine), "Майнер: Core2 %s, H %lu", coreWorkerEnabled() ? "работает" : "пауза", (unsigned long)coreRemoteHashrate);
      snprintf(janusLine, sizeof(janusLine), "Янус: майнинг — фоновый реактор станции, best %lu bits", (unsigned long)coreBestBits);
    } else {
      snprintf(focusLine, sizeof(focusLine), "Buzz: H %s S%lu R%lu", compactU(buzz.hashRate).c_str(), (unsigned long)buzz.shares, (unsigned long)buzz.rejects);
      snprintf(janusLine, sizeof(janusLine), "Янус: сверяю работу Buzz и свежесть job %s", coreJobText);
    }
  }

  void touchModules(int x, int y) {
    if (moduleDetailOpen) {
      if (x < 74 || x > 246 || y < 58 || y > 178) moduleDetailOpen = false;
      return;
    }
    if (x < 126 && y > 58 && y < 178) {
      selectedModule = (uint8_t)clipf((float)(y - 60) / 13.0f, 0.0f, (float)(JANUS_MODULE_N - 1));
      moduleDetailOpen = true;
      snprintf(focusLine, sizeof(focusLine), "Модуль: %s Lv.%u", moduleName(selectedModule), s.moduleLevel[selectedModule]);
      snprintf(janusLine, sizeof(janusLine), "Янус: открываю внутренний вид модуля %s", moduleName(selectedModule));
      return;
    }
    if (x > 180 && y > 58 && y < 184) {
      int dx = x - 246, dy = y - 102;
      if (dx*dx + dy*dy < 28*28) {
        snprintf(focusLine, sizeof(focusLine), "Gate: %s Mk.%u", s.gateInstalled ? "online" : "offline", s.gateLevel);
        snprintf(janusLine, sizeof(janusLine), "Янус: %s", s.gateInstalled ? "контролирую поток через разгонные врата" : "коплю кредиты на установку разгонных врат");
        return;
      }
      dx = x - 216; dy = y - 126;
      if (dx*dx + dy*dy < 50*50) {
        snprintf(focusLine, sizeof(focusLine), "Модель станции: качество %02d", (int)(clipf(s.stationQuality / 1.2f, 0.0f, 1.0f) * 100.0f));
        snprintf(janusLine, sizeof(janusLine), "Янус: модульная схема станции показывает качество %02d%%", (int)(clipf(s.stationQuality / 1.2f, 0.0f, 1.0f) * 100.0f));
      }
    }
  }

  void planetScreen(int i, int cx, int cy, int& px, int& py) {
    float a = p[i].angle + routePhase * (0.003f + i * 0.00035f); // almost static, no carousel effect
    float r = p[i].orbit;
    px = cx + (int)(cosf(a) * r);
    py = cy + (int)(sinf(a) * r * 0.58f);
  }

  void stationAnchor(int node, int& x, int& y) {
    int px, py;
    planetScreen(node, 160, 118, px, py);
    x = px;
    y = py;
  }
  void shipScreen(int i, int& x, int& y) {
    int sx = 116, sy = 118;
    int nx, ny, gx = sx, gy = sy;
    uint8_t node = (ships[i].state == 0) ? ships[i].to : ships[i].from;
    stationAnchor(node, nx, ny);
    if (s.gateInstalled) gateHubScreen(gx, gy);
    float k = clipf(ships[i].phase, 0.0f, 1.0f);
    if (ships[i].state == 2) {
      float a = ships[i].wobble + k * JANUS_TWO_PI;
      x = sx + (int)(cosf(a) * 8.0f);
      y = sy + 10 + (int)(sinf(a) * 4.0f);
      return;
    }
    if (!s.gateInstalled) {
      if (ships[i].state == 0) {
        x = sx + (int)((nx - sx) * k);
        y = sy + (int)((ny - sy) * k + sinf(k * JANUS_PI + ships[i].wobble) * 8.0f);
      } else {
        x = nx + (int)((sx - nx) * k);
        y = ny + (int)((sy - ny) * k + sinf(k * JANUS_PI + ships[i].wobble) * 8.0f);
      }
      return;
    }
    if (ships[i].state == 0) {
      if (k < 0.30f) {
        float t = k / 0.30f;
        x = sx + (int)((gx - sx) * t);
        y = sy + (int)((gy - sy) * t + sinf(t * JANUS_PI + ships[i].wobble) * 3.0f);
      } else {
        float t = (k - 0.30f) / 0.70f;
        x = gx + (int)((nx - gx) * t);
        y = gy + (int)((ny - gy) * t + sinf(t * JANUS_PI + ships[i].wobble) * 7.0f);
      }
    } else {
      if (k < 0.70f) {
        float t = k / 0.70f;
        x = nx + (int)((gx - nx) * t);
        y = ny + (int)((gy - ny) * t + sinf(t * JANUS_PI + ships[i].wobble) * 7.0f);
      } else {
        float t = (k - 0.70f) / 0.30f;
        x = gx + (int)((sx - gx) * t);
        y = gy + (int)((sy - gy) * t + sinf(t * JANUS_PI + ships[i].wobble) * 3.0f);
      }
    }
  }


  int hitShip(int x, int y) {
    for (int i = 0; i < 9; i++) {
      int sx, sy;
      shipScreen(i, sx, sy);
      if (hypotf((float)(x - sx), (float)(y - sy)) < 10.0f) return i;
    }
    return -1;
  }

  uint16_t planetColor(int i) {
    if (!p[i].online) return grgb(72, 82, 105);
    switch (i) {
      case 0: return grgb(80, 220, 255);
      case 1: return grgb(80, 230, 145);
      case 2: return grgb(255, 185, 65);
      case 3: return grgb(195, 120, 255);
      case 4: return grgb(90, 145, 255);
      case 5: return grgb(255, 240, 105);
      default: return grgb(150, 155, 170);
    }
  }

  void drawText(int x, int y, uint16_t c, uint16_t bg, const char* txt) {
    canvas.setTextSize(1);
    canvas.setTextColor(c, bg);
    canvas.setCursor(x, y);
    canvas.print(txt);
  }

  void drawTextf(int x, int y, uint16_t c, uint16_t bg, const char* fmt, ...) {
    char b[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof(b), fmt, ap);
    va_end(ap);
    drawText(x, y, c, bg, b);
  }


  void drawClippedText(int x, int y, uint16_t c, uint16_t bg, const char* txt, uint8_t maxChars) {
    static char b[80];
    if (maxChars >= sizeof(b)) maxChars = sizeof(b) - 1;
    uint8_t n = 0;
    while (txt[n] && n < maxChars) { b[n] = txt[n]; n++; }
    b[n] = 0;
    drawText(x, y, c, bg, b);
  }


  // ========================= v5.10 PIXEL ART / NOITA-IN-SPACE VISUAL LAYER =========================
  // Core idea: keep simulation logic intact, but render the world as pixel-art:
  // dithered space, crystal glow, sprite ships, particle sparks, compact HUD.

  uint16_t pxBg() const { return grgb(1, 2, 8); }
  uint16_t pxPanel() const { return grgb(5, 7, 13); }
  uint16_t pxFrame() const { return grgb(84, 58, 28); }
  uint16_t pxAmber() const { return grgb(255, 178, 72); }
  uint16_t pxGold() const { return grgb(255, 224, 118); }
  uint16_t pxCyan() const { return grgb(94, 208, 255); }
  uint16_t pxGreen() const { return grgb(104, 235, 158); }
  uint16_t pxRed() const { return grgb(255, 92, 76); }
  uint16_t pxViolet() const { return grgb(180, 122, 255); }

  uint8_t hash8(int x, int y, uint8_t seed = 0) const {
    uint8_t v = (uint8_t)(x * 17 + y * 31 + seed * 53);
    v ^= (v >> 3);
    v = (uint8_t)(v * 29 + 37);
    return v;
  }

  uint16_t mixPix(uint16_t a, uint16_t b, float t) {
    t = clipf(t, 0.0f, 1.0f);
    int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31;
    int br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
    int rr = (int)(ar + (br - ar) * t);
    int rg = (int)(ag + (bg - ag) * t);
    int rb = (int)(ab + (bb - ab) * t);
    return (rr << 11) | (rg << 5) | rb;
  }

  void pix(int x, int y, uint16_t c, int s = 1) {
    if (s <= 1) canvas.drawPixel(x, y, c);
    else canvas.fillRect(x, y, s, s, c);
  }

  void pixCross(int x, int y, uint16_t c) {
    canvas.drawPixel(x, y, c);
    canvas.drawPixel(x - 1, y, dim(c, 0.7f));
    canvas.drawPixel(x + 1, y, dim(c, 0.7f));
    canvas.drawPixel(x, y - 1, dim(c, 0.55f));
    canvas.drawPixel(x, y + 1, dim(c, 0.55f));
  }

  void drawPanelFrame(int x, int y, int w, int h, uint16_t bg, uint16_t edge) {
    // Pixel-art box: no round corners, strong silhouette, slight dithered border.
    canvas.fillRect(x, y, w, h, bg);
    canvas.drawRect(x, y, w, h, edge);
    canvas.drawRect(x + 2, y + 2, w - 4, h - 4, dim(edge, 0.45f));
    canvas.drawPixel(x + 1, y + 1, pxGold());
    canvas.drawPixel(x + w - 2, y + 1, pxGold());
    canvas.drawPixel(x + 1, y + h - 2, dim(pxGold(), 0.6f));
    canvas.drawPixel(x + w - 2, y + h - 2, dim(pxGold(), 0.6f));
  }

  void drawMiniBar(int x, int y, int w, float v, uint16_t col) {
    v = clipf(v, 0.0f, 1.0f);
    canvas.fillRect(x, y, w, 5, pxPanel());
    canvas.drawRect(x, y, w, 5, dim(pxFrame(), 0.75f));
    canvas.fillRect(x + 1, y + 1, (int)((w - 2) * v), 3, col);
    if (v > 0.65f) canvas.drawPixel(x + w - 3, y + 2, pxGold());
  }

  void drawMetricLine(int x, int y, const char* label, float v, uint16_t col) {
    drawText(x, y, dim(col, 0.92f), pxPanel(), label);
    drawMiniBar(x + 48, y + 2, 50, v, col);
  }

  void drawSparkline(int x, int y, int w, int h, float seed, uint16_t col) {
    int px0 = x, py0 = y + h / 2;
    for (int i = 0; i < w; i++) {
      float t = seed * 0.37f + i * 0.24f + routePhase * 0.0012f;
      float v = 0.5f + 0.34f * sinf(t) + 0.10f * sinf(t * 2.6f + seed);
      int ny = y + h - 1 - (int)(clipf(v, 0.0f, 1.0f) * (float)(h - 1));
      if (i > 0) canvas.drawLine(px0, py0, x + i, ny, col);
      px0 = x + i; py0 = ny;
    }
  }

  void drawWireCube(int x, int y, int w, int h, int dx, int dy, uint16_t front, uint16_t side, uint16_t edge) {
    // Keep legacy calls safe, but pixelize them.
    canvas.fillRect(x, y, w, h, front);
    canvas.drawRect(x, y, w, h, edge);
    canvas.fillRect(x + dx, y - dy, w, h, side);
    canvas.drawRect(x + dx, y - dy, w, h, dim(edge, 0.75f));
    canvas.drawLine(x, y, x + dx, y - dy, edge);
    canvas.drawLine(x + w, y, x + w + dx, y - dy, edge);
    canvas.drawLine(x, y + h, x + dx, y + h - dy, edge);
    canvas.drawLine(x + w, y + h, x + w + dx, y + h - dy, edge);
  }

  void drawDitherDisc(int cx, int cy, int r, uint16_t hi, uint16_t mid, uint16_t sh, uint8_t seed) {
    int rr = r * r;
    for (int yy = -r; yy <= r; yy++) {
      int span = (int)sqrtf((float)(rr - yy * yy));
      for (int xx = -span; xx <= span; xx++) {
        uint8_t h = hash8(xx + r, yy + r, seed);
        float d = sqrtf((float)(xx * xx + yy * yy)) / (float)max(1, r);
        uint16_t c = (d < 0.42f) ? hi : ((d < 0.78f) ? mid : sh);
        if (xx > r * 0.18f && yy < r * 0.22f) c = mixPix(c, pxBg(), 0.50f);
        if ((h & 3) == 0 || d < 0.55f) canvas.drawPixel(cx + xx, cy + yy, c);
      }
    }
  }

  void drawPixelGlow(int x, int y, uint16_t c, int r, uint8_t phase) {
    // Cheap glow. In perfMode no sin/cos loops, preserving 35+ FPS target.
    if (perfMode) {
      canvas.drawPixel(x, y, c);
      canvas.drawPixel(x - 1, y, dim(c, 0.62f));
      canvas.drawPixel(x + 1, y, dim(c, 0.62f));
      canvas.drawPixel(x, y - 1, dim(c, 0.46f));
      canvas.drawPixel(x, y + 1, dim(c, 0.46f));
      if ((phase & 3) == 0) canvas.drawCircle(x, y, min(4, max(2, r / 3)), dim(c, 0.26f));
      return;
    }
    for (int rr = min(r, 10); rr >= 2; rr -= 3) {
      uint16_t cc = dim(c, 0.24f + 0.055f * rr);
      int dots = rr * 3;
      for (int i = 0; i < dots; i += 2) {
        float a = (float)i * JANUS_TWO_PI / (float)dots + phase * 0.11f;
        int px = x + (int)(cosf(a) * rr);
        int py = y + (int)(sinf(a) * rr * 0.72f);
        canvas.drawPixel(px, py, cc);
      }
    }
    pixCross(x, y, c);
  }

  void drawCinematicBackground() {
    canvas.fillRect(0, 0, 320, 240, pxBg());
    uint32_t now = millis();
    // Perf background: deterministic pixel starfield + sparse dust, no per-pixel trig.
    int starCount = perfMode ? 70 : 118;
    int drift = (now / 11000UL) % 320;
    for (int i = 0; i < starCount; i++) {
      uint32_t seed = (uint32_t)i * 1103515245UL + s.visualSeed * 37UL;
      int x = (int)((seed + drift * (1 + (i & 1))) % 320UL);
      int y = 18 + (int)((seed >> 9) % 176UL);
      uint16_t c = (i & 7) == 0 ? grgb(132, 160, 222) : ((i & 3) == 0 ? grgb(70, 88, 126) : grgb(30, 40, 66));
      canvas.drawPixel(x, y, c);
      if (!perfMode && (i & 15) == 0 && x < 319) canvas.drawPixel(x + 1, y, dim(c, 0.55f));
    }
    int dustCount = perfMode ? 90 : 190;
    uint8_t phase = (now / 250) & 63;
    for (int i = 0; i < dustCount; i++) {
      int x = (i * 37 + s.visualSeed * 11 + phase) % 320;
      int y = 42 + ((i * 23 + (phase << 1)) % 108);
      uint8_t h = hash8(x, y, phase);
      if ((h & 7) < (perfMode ? 2 : 3)) canvas.drawPixel(x, y, (h & 16) ? grgb(32, 24, 58) : grgb(16, 36, 62));
    }
  }
  void drawTopHud(bool minerActive) {
    static uint32_t prevFrameMs = 0;
    uint32_t now = millis();
    if (prevFrameMs) {
      float dt = (float)(now - prevFrameMs);
      if (dt > 0.0f) fpsSmoothGame += ((1000.0f / dt) - fpsSmoothGame) * 0.16f;
    }
    prevFrameMs = now;
    if (now - lastPerfModeMs > 700UL) {
      if (fpsSmoothGame < 37.0f) perfMode = true;
      else if (fpsSmoothGame > 43.0f) perfMode = false;
      lastPerfModeMs = now;
    }
    uint16_t bg = pxPanel();
    drawPanelFrame(4, 4, 312, 15, bg, pxFrame());
    drawTextf(10, 8, pxGold(), bg, "JANUS PIXEL / %s", viewName());
    drawTextf(140, 8, minerActive ? pxAmber() : grgb(120, 126, 136), bg, "%s", minerActive ? "MINING" : "IDLE");
    drawTextf(204, 8, pxGreen(), bg, "J%02d", (int)(universeJanusInfluence * 100.0f));
    drawTextf(246, 8, universeThargoidPressure > 0.55f ? pxRed() : (perfMode ? pxAmber() : grgb(120,150,170)), bg, universeThargoidPressure > 0.55f ? "THG" : (perfMode ? "FAST" : "ART"));
    drawTextf(278, 8, fpsSmoothGame < 35.0f ? pxRed() : pxCyan(), bg, "%02d", (int)clipf(fpsSmoothGame, 0.0f, 99.0f));
  }

  void drawMindFeed() {
    uint16_t bg = grgb(4, 6, 11);
    drawPanelFrame(8, 23, 304, 15, bg, dim(pxFrame(), 0.85f));
    uint32_t phase = (millis() / 4000UL) % 6UL;
    const char* tag = "MIND";
    const char* line = janusLine;
    uint16_t tc = grgb(210, 222, 230);
    if (phase == 1) { tag = "TASK"; line = missionLine; tc = pxCyan(); }
    else if (phase == 2) { tag = "SWARM"; line = swarmLine; tc = pxGreen(); }
    else if (phase == 3) { tag = "UNIV"; line = universeLine; tc = pxCyan(); }
    else if (phase == 4) { tag = "GALNET"; line = newsLine; tc = pxGold(); }
    else if (phase == 5) { tag = "OPS"; line = contractLine; tc = pxAmber(); }
    drawTextf(14, 27, pxAmber(), bg, "%s>", tag);
    drawClippedText(58, 27, tc, bg, line, 40);
  }
  void drawNewsTicker() {
    // v5.12: нижний ticker отключён. Он вызывал заливку/перекрытие из-за text background.
    // GALNET теперь безопасно ротируется в верхнем feed через drawMindFeed().
  }


  void drawNavButtons() {
    uint16_t bg = pxPanel();
    drawPanelFrame(5, 220, 54, 16, bg, pxFrame());
    drawText(13, 224, pxCyan(), bg, "< ВИД");
    drawPanelFrame(261, 220, 54, 16, bg, pxFrame());
    drawText(269, 224, pxCyan(), bg, "ВИД >");
  }

  void stationCam(int& ox, int& oy) {
    float t = (float)millis() * 0.000075f;
    ox = (int)(sinf(t + camOrbit * 0.7f) * 2.0f + sinf(camOrbit) * 16.0f);
    oy = (int)(cosf(t * 0.87f + camPitch * 0.9f) * 2.0f + sinf(camPitch) * 10.0f);
  }
  float camScale() const { return clipf(camZoom, 0.72f, 1.45f); }

  void nudgeCamera(float dx, float dy, float dz) {
    camOrbit += dx;
    if (camOrbit > JANUS_PI) camOrbit -= JANUS_TWO_PI;
    if (camOrbit < -JANUS_PI) camOrbit += JANUS_TWO_PI;
    camPitch = clipf(camPitch + dy, -1.10f, 1.10f);
    camZoom = clipf(camZoom + dz, 0.70f, 1.62f);
    snprintf(focusLine, sizeof(focusLine), "Камера: yaw %.0f pitch %.0f zoom %.0f%%", camOrbit * 57.3f, camPitch * 57.3f, camZoom * 100.0f);
  }

  void drawSectorGrid() {
    int horizon = 130 + (int)(camPitch * 9.0f);
    uint16_t grid = grgb(42, 30, 18);
    for (int i = 0; i < 6; i++) {
      float f = (float)i / 5.0f;
      int y = horizon + 5 + (int)(f * f * 45.0f);
      int hw = 16 + (int)(f * 122.0f);
      canvas.drawLine(160 - hw, y, 160 + hw, y, dim(grid, 0.58f + f * 0.34f));
    }
    for (int i = -5; i <= 5; i++) {
      canvas.drawLine(160 + i * 16, horizon + 2, 160 + i * 31, 190, dim(grid, 0.75f));
    }
  }

  void drawSectorAnnotations() {
    drawText(18, 90, grgb(150, 125, 82), pxBg(), "dock corridor");
    drawText(228, 174, grgb(132, 132, 146), pxBg(), "ore field");
    if (averageRisk() > 0.34f) drawText(258, 142, pxRed(), pxBg(), "pirates");
  }

  void drawModuleSprite(int x, int y, uint8_t idx, uint8_t lv, bool selected, float z) {
    if (lv == 0) return;
    uint16_t col = moduleColor(idx);
    uint16_t edge = selected ? pxGold() : dim(col, 0.85f);
    int w = max(6, (int)((6 + (lv > 2 ? 2 : 0)) * z));
    int h = max(4, (int)((4 + (lv > 3 ? 1 : 0)) * z));
    switch (idx) {
      case MOD_BAR:
        drawWireCube(x - w/2, y - h/2, w, h, max(2,(int)(2*z)), max(1,(int)(2*z)), dim(col,0.40f), dim(col,0.26f), edge);
        canvas.drawPixel(x, y - h/2 - 2, pxGold());
        break;
      case MOD_DOCKS:
        canvas.fillRect(x - w/2, y - h/2, w + 6, h, dim(col, 0.34f));
        canvas.drawRect(x - w/2, y - h/2, w + 6, h, edge);
        canvas.drawLine(x + w/2 + 5, y - h/2, x + w/2 + 10, y - h/2 - 4, edge);
        canvas.drawLine(x + w/2 + 5, y + h/2, x + w/2 + 10, y + h/2 + 4, edge);
        break;
      case MOD_BAZAAR:
        canvas.fillRect(x - w/2, y - h/2, w, h, dim(col, 0.36f));
        canvas.drawRect(x - w/2, y - h/2, w, h, edge);
        canvas.drawLine(x - w/2, y, x + w/2, y, edge);
        break;
      case MOD_SHIPYARD:
        drawWireCube(x - w/2, y - h/2, w + 4, h + 2, max(3,(int)(3*z)), max(2,(int)(2*z)), dim(col,0.34f), dim(col,0.24f), edge);
        canvas.drawLine(x + w/2 + 2, y - h/2 - 2, x + w/2 + 9, y - h/2 - 8, edge);
        break;
      case MOD_RESEARCH:
        drawWireCube(x - w/2, y - h/2, w, h, max(2,(int)(2*z)), max(2,(int)(2*z)), dim(col,0.28f), dim(col,0.18f), edge);
        canvas.drawLine(x, y - h/2 - 1, x, y - h/2 - 7, edge);
        canvas.drawPixel(x, y - h/2 - 8, pxCyan());
        break;
      case MOD_BUSINESS:
        canvas.fillRect(x - w/2, y - h/2 - 2, w, h + 4, dim(col, 0.32f));
        canvas.drawRect(x - w/2, y - h/2 - 2, w, h + 4, edge);
        canvas.drawFastVLine(x, y - h/2 - 3, h + 6, dim(edge,0.6f));
        break;
      case MOD_HOTEL:
        drawWireCube(x - w/2, y - h/2 - 1, w, h + 3, max(2,(int)(2*z)), max(1,(int)(2*z)), dim(col,0.36f), dim(col,0.24f), edge);
        canvas.drawPixel(x - 1, y, pxGold()); canvas.drawPixel(x + 1, y, pxGold());
        break;
      case MOD_MEDBAY:
        canvas.fillRect(x - w/2, y - h/2, w, h, dim(col, 0.30f));
        canvas.drawRect(x - w/2, y - h/2, w, h, edge);
        canvas.drawFastHLine(x - 2, y, 5, edge);
        canvas.drawFastVLine(x, y - 2, 5, edge);
        break;
      case MOD_SHOPS:
        canvas.fillRect(x - w/2, y - h/2, w, h, dim(col, 0.30f));
        canvas.drawRect(x - w/2, y - h/2, w, h, edge);
        canvas.drawPixel(x - 2, y - 1, pxGold()); canvas.drawPixel(x + 2, y + 1, pxGold());
        break;
    }
    if (lv >= 4) drawPixelGlow(x, y, col, 4 + (selected ? 2 : 0), idx + 41);
  }
  void drawStationModulesVisual(int cx, int cy, float z) {
    static const int mx[JANUS_MODULE_N] = {-26, 22, -20, 26, 0, 15, -30, -6, 6};
    static const int my[JANUS_MODULE_N] = {-18, -10, 18, 16, -28, -25, 2, 26, 30};
    for (int i = 0; i < JANUS_MODULE_N; i++) {
      if (s.moduleLevel[i] == 0) continue;
      int x = cx + (int)(mx[i] * z);
      int y = cy + (int)(my[i] * z * 0.90f);
      drawModuleSprite(x, y, i, s.moduleLevel[i], i == selectedModule, z);
      if (i == selectedModule) {
        canvas.drawLine(cx, cy, x, y, dim(pxGold(), 0.35f));
        if (!perfMode) drawPixelGlow(x, y, pxGold(), 6, i + 12);
      }
    }
  }
  void drawModuleSchematic(int cx, int cy, float scale, int highlight) {
    canvas.drawCircle(cx, cy, max(8, (int)(16 * scale)), dim(pxCyan(), 0.50f));
    canvas.drawCircle(cx, cy, max(5, (int)(10 * scale)), dim(pxAmber(), 0.40f));
    static const int mx[JANUS_MODULE_N] = {-26, 24, -18, 26, 0, 14, -28, -8, 8};
    static const int my[JANUS_MODULE_N] = {-18, -8, 16, 18, -28, -24, 2, 26, 30};
    for (int i = 0; i < JANUS_MODULE_N; i++) {
      int x = cx + (int)(mx[i] * scale);
      int y = cy + (int)(my[i] * scale * 0.88f);
      uint16_t col = (s.moduleLevel[i] > 0) ? moduleColor(i) : grgb(74, 84, 102);
      canvas.drawLine(cx, cy, x, y, dim(col, 0.28f));
      drawModuleSprite(x, y, i, (s.moduleLevel[i] ? s.moduleLevel[i] : 1), i == highlight, max(0.65f, scale));
      if (s.moduleLevel[i] == 0) canvas.drawRect(x - 4, y - 3, 8, 6, dim(col,0.75f));
    }
  }

  void drawStationSilhouetteUpgrades(int cx, int cy, float z) {
    // Ultra-cheap exterior silhouette growth: a few rectangles/pixels based on module levels.
    uint32_t blink = millis() / 180;
    if (s.moduleLevel[MOD_DOCKS] > 0) {
      int len = 8 + s.moduleLevel[MOD_DOCKS] * 2;
      canvas.drawFastHLine(cx - 46, cy - 8, len, pxAmber());
      canvas.drawFastHLine(cx - 46, cy + 8, len, dim(pxAmber(),0.75f));
      canvas.drawFastHLine(cx + 34, cy - 8, len, pxAmber());
      canvas.drawFastHLine(cx + 34, cy + 8, len, dim(pxAmber(),0.75f));
    }
    if (s.moduleLevel[MOD_SHIPYARD] > 0) {
      int w = 10 + s.moduleLevel[MOD_SHIPYARD] * 2;
      canvas.drawRect(cx - w/2, cy + 31, w, 7, grgb(160,170,210));
      if ((blink & 3) == 0) canvas.drawPixel(cx + w/2 - 2, cy + 34, pxAmber());
    }
    if (s.moduleLevel[MOD_BAR] >= 2 || s.moduleLevel[MOD_HOTEL] >= 2) {
      int windows = 3 + min(3, (int)(s.moduleLevel[MOD_BAR] + s.moduleLevel[MOD_HOTEL]) / 2);
      for (int i = 0; i < windows; i++) canvas.drawPixel(cx - 10 + i * 4, cy - 13, pxGold());
    }
    if (s.moduleLevel[MOD_RESEARCH] > 0) {
      int h = 9 + s.moduleLevel[MOD_RESEARCH] * 2;
      canvas.drawFastVLine(cx, cy - 36 - h, h, pxCyan());
      canvas.drawCircle(cx, cy - 38 - h, 2, pxCyan());
    }
    if (s.moduleLevel[MOD_BUSINESS] >= 2 || s.moduleLevel[MOD_BAZAAR] >= 2) {
      uint16_t ad1 = ((blink & 1) ? pxGreen() : pxCyan());
      uint16_t ad2 = ((blink & 1) ? pxCyan() : pxGreen());
      canvas.drawPixel(cx - 34, cy - 1, ad1); canvas.drawPixel(cx - 36, cy + 2, ad2);
      canvas.drawPixel(cx + 36, cy - 1, ad2); canvas.drawPixel(cx + 34, cy + 2, ad1);
    }
  }

  void drawStationCore() {
    int ox, oy; stationCam(ox, oy);
    float z = camScale();
    int cx = 116 + ox;
    int cy = 118 + oy;
    float rot = (float)millis() * 0.00020f + camOrbit * 0.25f;
    float radius = 30.0f * z;
    int fx[8], fy[8], bx[8], by[8];

    for (int i = 0; i < 8; i++) {
      float a = rot + (float)i * JANUS_PI / 4.0f + JANUS_PI / 8.0f;
      fx[i] = cx + (int)(cosf(a) * radius);
      fy[i] = cy + (int)(sinf(a) * radius * 0.74f);
      bx[i] = fx[i] + (int)(10.0f * z);
      by[i] = fy[i] - (int)(6.0f * z);
    }

    // Pixel glow first.
    if (!perfMode || ((millis()/240)&1)==0) drawPixelGlow(cx, cy, coreWorkerEnabled() ? pxAmber() : pxCyan(), perfMode ? 5 : 10, (millis()/100)&31);

    // Dithered back face.
    for (int i = 0; i < 8; i++) {
      int j = (i + 1) & 7;
      canvas.drawLine(bx[i], by[i], bx[j], by[j], grgb(48, 58, 84));
      canvas.drawLine(fx[i], fy[i], bx[i], by[i], grgb(66, 76, 104));
      canvas.drawLine(fx[i], fy[i], fx[j], fy[j], pxCyan());
    }
    canvas.drawLine(fx[0], fy[0], fx[4], fy[4], grgb(82, 112, 150));
    canvas.drawLine(fx[2], fy[2], fx[6], fy[6], grgb(82, 112, 150));
    canvas.drawRect(cx - (int)(9*z), cy - (int)(6*z), max(12,(int)(18*z)), max(8,(int)(13*z)), pxAmber());
    canvas.drawPixel(cx, cy, grgb(240, 250, 255));
    drawStationModulesVisual(cx, cy, z);
    drawStationSilhouetteUpgrades(cx, cy, z);
    drawText(cx - 20, cy + 40, pxCyan(), pxBg(), "JANUS HUB");
  }
  void drawPlanetVista() {
    int ox, oy; stationCam(ox, oy);
    float z = camScale();
    uint8_t sidx = universeServiceSector % UNIVERSE_SECTORS;
    int px = 250 + ox / 3;
    int py = 98 + oy / 4;
    int pr = max(23, (int)(48.0f * z));
    uint16_t hi = universePlanetHi(sidx);
    uint16_t mid = universePlanetMid(sidx);
    uint16_t sh = universePlanetShade(sidx);

    // Planet/background now follows the station Janus is servicing.
    canvas.fillCircle(px, py, pr, sh);
    canvas.fillCircle(px - pr/5, py - pr/6, pr - 6, mid);
    canvas.fillCircle(px + pr/3, py - pr/5, pr * 2 / 3, dim(pxBg(), 0.92f));
    if (!perfMode) canvas.drawCircle(px, py, pr + 2, dim(hi, 0.52f));
    for (int i = -2; i <= 2; i++) {
      int yy = py + i * max(4, pr / 7);
      uint16_t band = mixPix(mid, hi, 0.20f + 0.10f * abs(i));
      if ((sidx & 3) == 1) band = grgb(224, 170, 88);      // desert bands
      if ((sidx & 3) == 2) band = grgb(88, 178, 220);      // ocean bands
      if ((sidx & 7) == 5) band = grgb(112, 238, 96);      // toxic clouds
      canvas.drawFastHLine(px - pr/2 + abs(i)*3, yy, pr - abs(i)*7, band);
    }
    for (int i = 0; i < (perfMode ? 7 : 14); i++) {
      int lx = px - pr/4 + ((i * 13 + (millis()/450) + sidx * 5) % max(6, pr/2));
      int ly = py - pr/5 + ((i * 7 + sidx * 3) % max(6, pr/3));
      canvas.drawPixel(lx, ly, (sidx & 1) ? pxCyan() : pxGold());
    }
    canvas.drawEllipse(px, py + (int)(5*z), (int)(pr * 1.25f), max(4,(int)(pr*0.22f)), dim(hi, 0.42f));
    drawTextf(px - 34, py + pr + 10, grgb(176, 198, 214), pxBg(), "%s S%02u", universePlanetName(sidx), sidx);
  }
  void drawAsteroidField() {
    int ox, oy; stationCam(ox, oy);
    int bx = 224 + (int)(ox * 0.45f);
    int by = 153 + (int)(oy * 0.40f);
    int n = perfMode ? 14 : 24;
    int drift = (millis() / 260) & 31;
    for (int i = 0; i < n; i++) {
      uint32_t seed = (uint32_t)i * 3266489917UL + s.visualSeed * 19UL;
      int x = bx + (int)(((seed >> 8) % 92) - 46) + ((i & 1) ? drift/3 : -drift/4);
      int y = by + (int)(((seed >> 19) % 28) - 14);
      uint16_t c = (i % 5 == 0) ? grgb(174, 134, 70) : grgb(86, 82, 84);
      pix(x, y, c, (!perfMode && (i % 9 == 0)) ? 2 : 1);
      if (!perfMode && (i & 7) == 0) pixCross(x, y, pxGold());
    }
  }


  void drawShipTriangle(int x, int y, float a, uint16_t col) {
    // Pixel sprite ship instead of smooth triangle.
    int dx = (int)roundf(cosf(a));
    int dy = (int)roundf(sinf(a));
    if (abs(dx) < abs(dy)) dx = 0; else dy = 0;
    uint16_t shade = dim(col, 0.55f);
    pix(x, y, col);
    pix(x + dx*2, y + dy*2, grgb(245,245,255));
    pix(x - dy, y + dx, shade);
    pix(x + dy, y - dx, shade);
    pix(x - dx, y - dy, col);
    pix(x - dx*2, y - dy*2, pxAmber());
    if ((millis()/160)&1) pix(x - dx*3, y - dy*3, pxGold());
  }

  void drawRouteBeacon(int x, int y, uint16_t col, uint8_t seed) {
    int p = ((millis() / 260) + seed) & 3;
    canvas.drawPixel(x, y, col);
    canvas.drawCircle(x, y, 2 + (p & 1), dim(col, 0.28f));
    canvas.drawPixel(x - 2, y, dim(col, 0.45f));
    canvas.drawPixel(x + 2, y, dim(col, 0.45f));
    canvas.drawPixel(x, y - 2, dim(col, 0.45f));
    canvas.drawPixel(x, y + 2, dim(col, 0.45f));
  }
  void drawJumpGate(int x, int y, uint16_t col, bool stationGate, uint8_t seed) {
    int r = stationGate ? 18 : 9;
    int pulse = ((millis() / 180) + seed) & 7;
    uint16_t frame = stationGate ? pxAmber() : dim(col, 0.88f);
    uint16_t dark = dim(col, 0.25f);
    uint16_t glow = dim(col, 0.55f);

    canvas.drawCircle(x, y, r, dim(frame, 0.85f));
    canvas.drawCircle(x, y, r - 3, dark);
    if (!perfMode) canvas.drawCircle(x, y, r - 7, dim(col, 0.24f));
    // no trig in perf mode: fixed energy nodes
    canvas.drawPixel(x - r + 6, y, glow); canvas.drawPixel(x + r - 6, y, glow);
    canvas.drawPixel(x, y - r + 6, glow); canvas.drawPixel(x, y + r - 6, glow);
    if (!perfMode) { canvas.drawPixel(x - 5, y - 5, glow); canvas.drawPixel(x + 5, y + 5, glow); }
    if (stationGate) {
      canvas.drawRect(x - r - 12, y - 9, 7, 17, dim(frame,0.90f));
      canvas.drawRect(x + r + 5, y - 9, 7, 17, dim(frame,0.90f));
      canvas.fillRect(x - 6, y + r + 4, 12, 5, dim(frame, 0.38f));
      canvas.drawRect(x - 6, y + r + 4, 12, 5, dim(frame, 0.85f));
      canvas.drawLine(x - r - 5, y, x - r + 3, y, frame);
      canvas.drawLine(x + r - 3, y, x + r + 7, y, frame);
      if (!perfMode) {
        canvas.drawLine(x - r - 5, y + 7, x - 4, y + r + 4, dim(frame,0.55f));
        canvas.drawLine(x + r + 7, y + 7, x + 4, y + r + 4, dim(frame,0.55f));
      }
    } else {
      canvas.fillRect(x - r, y - 2, 5, 5, dim(frame, 0.62f));
      canvas.fillRect(x + r - 5, y - 2, 5, 5, dim(frame, 0.62f));
      canvas.fillRect(x - 2, y - r, 5, 5, dim(frame, 0.62f));
      canvas.fillRect(x - 2, y + r - 5, 5, 5, dim(frame, 0.62f));
    }
    canvas.fillCircle(x, y, stationGate ? 3 : 2, dim(col, 0.45f + (pulse & 1) * 0.15f));
    if (!perfMode && (stationGate || pulse == 0)) drawPixelGlow(x, y, col, stationGate ? 9 : 6, seed);
  }

  void drawRouteBanner() {
    int idx = s.selectedShip % 9;
    uint8_t node = (ships[idx].state == 0) ? ships[idx].to : ships[idx].from;
    const char* dir = (ships[idx].state == 0) ? "ВЫЛЕТ" : (ships[idx].state == 1 ? "ВОЗВРАТ" : "ДОК");
    uint16_t bg = pxPanel();
    drawPanelFrame(68, 41, 182, 14, bg, pxFrame());
    if (s.gateInstalled && ships[idx].state != 2) drawTextf(74, 45, pxGold(), bg, "%s: %s / gate %.0fcr", dir, p[node].name, jumpFee(ships[idx].from, ships[idx].to));
    else drawTextf(74, 45, pxGold(), bg, "%s: %s", dir, p[node].name);
  }
  void drawSectorTraffic() {
    int ox, oy; stationCam(ox, oy);
    const int sx = 116 + ox, sy = 118 + oy;
    const int px = 250 + ox / 3, py = 98 + oy / 4;
    int gx, gy; gateHubScreen(gx, gy);

    // Local jump gate now lives FAR from the station and must be installed/upgraded by Janus.
    if (s.gateInstalled) {
      drawJumpGate(gx, gy, pxAmber(), true, s.gateLevel);
      canvas.drawLine(sx, sy, gx, gy, dim(pxAmber(), 0.10f));
      for (int i = 0; i < 2 + min(2, (int)s.gateLevel); i++) {
        float f = fmodf((float)millis() * 0.00016f + i * 0.27f, 1.0f);
        int x = sx + (int)((gx - sx) * f);
        int y = sy + (int)((gy - sy) * f + sinf(f * JANUS_PI + i) * 4.0f);
        canvas.drawPixel(x, y, dim(pxAmber(), 0.42f));
      }
    } else {
      canvas.drawCircle(gx, gy, 15, dim(pxAmber(), 0.28f));
      canvas.drawCircle(gx, gy, 10, dim(pxAmber(), 0.16f));
      drawTextf(gx - 22, gy + 18, grgb(180,176,128), pxBg(), "gate %.0fcr", gateInstallCost());
    }

    float risk = averageRisk();
    int pirateCount = (risk > 0.60f) ? 4 : ((risk > 0.34f) ? 2 : 1);
    for (int i = 0; i < pirateCount; i++) {
      float f = fmodf((float)millis() * 0.00009f + i * 0.23f, 1.0f);
      int x = 266 + (int)(cosf(f * JANUS_TWO_PI + i * 0.6f) * 18.0f);
      int y = 148 + (int)(sinf(f * JANUS_TWO_PI + i) * 10.0f);
      drawShipTriangle(x, y, 3.14159f + f * 1.2f, pxRed());
      if (!perfMode && ((((millis()/220)+i)&7)==0)) drawPixelGlow(x, y, pxRed(), 6, i);
    }

    if (risk > 0.28f) {
      float f = fmodf((float)millis() * 0.00014f, 1.0f);
      int x = sx + (int)((274 - sx) * f);
      int y = sy + (int)((150 - sy) * f + sinf(f * JANUS_PI) * 8.0f);
      drawShipTriangle(x, y, atan2f(150.0f - y, 274.0f - x), pxGreen());
    }
  }
  void drawShipsStation() {
    int ox, oy; stationCam(ox, oy);
    int sx = 116 + ox, sy = 118 + oy;
    int localGateX, localGateY; gateHubScreen(localGateX, localGateY);

    for (int i = 0; i < 9; i++) {
      int x, y; shipScreen(i, x, y);
      x += ox / 2; y += oy / 2;
      uint8_t node = (ships[i].state == 0) ? ships[i].to : ships[i].from;
      int tx, ty; stationAnchor(node, tx, ty);
      float a = atan2f((float)(ty - sy), (float)(tx - sx));
      uint16_t col = (i == s.selectedShip) ? pxGold() : ((ships[i].kind == 2) ? pxRed() : (ships[i].state == 0 ? pxCyan() : (ships[i].state == 1 ? pxGreen() : grgb(170, 190, 210))));

      if (s.gateInstalled) {
        canvas.drawLine(sx, sy, localGateX, localGateY, dim(col, (i == s.selectedShip) ? 0.22f : 0.06f));
        canvas.drawLine(localGateX, localGateY, tx, ty, dim(col, (i == s.selectedShip) ? 0.24f : 0.07f));
      } else {
        canvas.drawLine(sx, sy, tx, ty, dim(col, (i == s.selectedShip) ? 0.24f : 0.08f));
      }

      if (i == s.selectedShip) {
        if (s.gateInstalled && !perfMode) drawJumpGate(localGateX, localGateY, pxAmber(), true, s.gateLevel + 7);
        drawRouteBeacon(tx, ty, (ships[i].state == 0) ? pxCyan() : pxGreen(), i + 3);
        drawText(tx + 9, ty - 3, pxGold(), pxBg(), p[node].name);
      } else {
        drawRouteBeacon(tx, ty, dim(col, 0.65f), i + 3);
      }

      if (ships[i].state != 2) {
        for (int k = 1; k <= 3; k++) canvas.drawPixel(x - (int)(cosf(a)*k*2), y - (int)(sinf(a)*k*2), dim(col, 0.10f + k*0.08f));
      }

      if (ships[i].phase < 0.05f || ships[i].phase > 0.95f) {
        int fx = (ships[i].state == 0) ? (s.gateInstalled ? localGateX : sx) : tx;
        int fy = (ships[i].state == 0) ? (s.gateInstalled ? localGateY : sy) : ty;
        // Mooring rings: 3 cheap concentric circles, phase-driven, no extra timers.
        int rr0 = 2 + (int)(fabsf(ships[i].phase - 0.5f) * 3.0f);
        canvas.drawCircle(fx, fy, rr0, dim(col, 0.70f));
        if (!perfMode) { canvas.drawCircle(fx, fy, rr0 + 2, dim(col, 0.42f)); canvas.drawCircle(fx, fy, rr0 + 4, dim(col, 0.24f)); }
      }

      drawShipTriangle(x, y, a + ships[i].wobble*0.20f, col);
      if (!perfMode && ((((millis()/220)+i)&3)==0 && i == s.selectedShip)) drawPixelGlow(x, y, pxGold(), 8, i);
    }
    drawRouteBanner();
  }
  void drawStationFortressLayer() {
    int x0 = 8, y0 = 202;
    uint16_t bg = pxPanel();
    drawPanelFrame(x0, y0, 135, 14, bg, pxFrame());
    drawText(x0 + 6, y0 + 3, pxAmber(), bg, "STATION");
    drawTextf(x0 + 54, y0 + 3, pxCyan(), bg, "L%u Q%02d", s.stationLevel, (int)(clipf(s.stationQuality / 1.2f, 0.0f, 1.0f) * 100.0f));
    drawTextf(x0 + 6, y0 + 9, grgb(202,214,228), bg, "h%02d gate %s", (int)(s.stationHull * 100.0f), s.gateInstalled ? "ON" : "OFF");
  }
  void drawEntropyWardensPanel() {
    int x0 = 149, y0 = 202;
    uint16_t bg = pxPanel();
    drawPanelFrame(x0, y0, 163, 14, bg, pxFrame());
    drawText(x0 + 6, y0 + 3, pxGreen(), bg, "ESP-NOW");
    drawTextf(x0 + 58, y0 + 3, grgb(210,224,228), bg, "shield %02d learn %02d", (int)(swarmShield * 100.0f), (int)(swarmLearningTotal * 100.0f));
    drawTextf(x0 + 6, y0 + 9, pxCyan(), bg, "kills %u mods %u/9", pirateKills, (unsigned)(s.moduleLevel[0]+s.moduleLevel[1]+s.moduleLevel[2]+s.moduleLevel[3]+s.moduleLevel[4]+s.moduleLevel[5]+s.moduleLevel[6]+s.moduleLevel[7]+s.moduleLevel[8]));
  }

  void drawStationOverlay() {
    int x0 = 204, y0 = 44;
    uint16_t bg = pxPanel();
    drawPanelFrame(x0, y0, 108, 50, bg, pxFrame());
    drawText(x0 + 8, y0 + 5, pxAmber(), bg, "JANUS AI");
    drawTextf(x0 + 70, y0 + 5, grgb(180,196,214), bg, "%d%%", (int)(galaxyConfidence * 100.0f));
    drawMetricLine(x0 + 8, y0 + 16, "learn", swarmLearningTotal, pxGreen());
    drawMetricLine(x0 + 8, y0 + 25, "risk", clipf(spaceRisk, 0.0f, 1.0f), pxAmber());
    drawMetricLine(x0 + 8, y0 + 34, "tech", techProgress, pxCyan());
    drawTextf(x0 + 8, y0 + 43, grgb(190,205,220), bg, "Q%02d gate %s", (int)(clipf(s.stationQuality / 1.2f,0,1)*100.0f), s.gateInstalled ? "on" : "off");
  }
  void drawStationView(bool minerActive) {
    drawCinematicBackground();
    drawPlanetVista();
    drawAsteroidField();
    drawSectorGrid();
    drawSectorTraffic();
    drawStationCore();
    drawShipsStation();
    if (combatFlash > 0.02f) {
      uint16_t c = dim(pxRed(), combatFlash);
      int ax = 238 + (int)(sinf(routePhase * 3.0f) * 16.0f);
      int ay = 132 + (int)(cosf(routePhase * 4.0f) * 10.0f);
      int bx = 278 + (int)(cosf(routePhase * 2.0f) * 12.0f);
      int by = 150 + (int)(sinf(routePhase * 5.0f) * 9.0f);
      canvas.drawLine(ax, ay, bx, by, c);
      pixCross(ax, ay, pxGold());
      pixCross(bx, by, c);
      if (combatFlash > 0.65f) {
        if (!perfMode) drawPixelGlow(bx, by, pxRed(), 8, (millis()/60)&31);
        drawText(136, 54, pxRed(), pxBg(), "БОЙ!");
      }
    }
    drawTopHud(minerActive);
    drawMindFeed();
    drawStationOverlay();
    drawUniverseBuildOrders(204, 98, 108, 70, pxPanel());
    drawSectorAnnotations();
    drawStationFortressLayer();
    drawEntropyWardensPanel();
    drawNavButtons();
    drawNewsTicker();
  }



  void drawUniverseSectorMiniMap(int x0, int y0, int w, int h, uint16_t bg) {
    drawPanelFrame(x0, y0, w, h, bg, pxFrame());
    drawText(x0 + 6, y0 + 4, pxAmber(), bg, "UNIFIED RTS");
    int cell = min((w - 18) / 4, (h - 20) / 4);
    if (cell < 5) cell = 5;
    int gx0 = x0 + 8;
    int gy0 = y0 + 17;
    for (uint8_t i = 0; i < UNIVERSE_SECTORS; i++) {
      int gx = gx0 + (i % 4) * (cell + 2);
      int gy = gy0 + (i / 4) * (cell + 2);
      uint16_t c = universeOwnerColor(universeOwner[i]);
      float inf = clipf(universeInfluence[i], 0.0f, 1.0f);
      float th = clipf(universeThreat[i], 0.0f, 1.0f);
      canvas.fillRect(gx, gy, cell, cell, dim(c, 0.18f + inf * 0.45f));
      canvas.drawRect(gx, gy, cell, cell, (i == universeBuildTarget) ? pxGold() : dim(c, 0.70f));
      canvas.drawPixel(gx + 1, gy + 1, factionColor(universeFaction[i]));
      if (th > 0.55f) canvas.drawLine(gx, gy + cell - 1, gx + cell - 1, gy, pxRed());
      if (universeStationLevel[i] > 0) {
        int cx = gx + cell / 2;
        int cy = gy + cell / 2;
        canvas.drawPixel(cx, cy, grgb(240,250,255));
        if (universeStationLevel[i] > 2 && cx + 1 < gx + cell) canvas.drawPixel(cx + 1, cy, pxCyan());
      }
    }
    int tx = x0 + 8 + 4 * (cell + 2) + 8;
    drawTextf(tx, y0 + 18, pxCyan(), bg, "J %02d%%", (int)(universeJanusInfluence * 100.0f));
    drawTextf(tx, y0 + 29, pxRed(), bg, "T %02d%%", (int)(universeThargoidPressure * 100.0f));
    drawTextf(tx, y0 + 40, pxGold(), bg, "B S%02u", universeBuildTarget);
    drawMiniBar(tx, y0 + 52, max(32, w - (tx - x0) - 10), universeBuild[universeBuildTarget], pxGold());
  }

  void drawUniverseBuildOrders(int x0, int y0, int w, int h, uint16_t bg) {
    drawPanelFrame(x0, y0, w, h, bg, pxFrame());
    uint8_t sidx = universeBuildTarget;
    drawTextf(x0 + 6, y0 + 5, pxAmber(), bg, "RTS ORDER S%02u %s/%s", sidx, universeOwnerName(universeOwner[sidx]), factionName(universeFaction[sidx]));
    drawTextf(x0 + 6, y0 + 17, grgb(210,224,236), bg, "build L%u %.0f%%  supply %.0f%%", universeStationLevel[sidx], universeBuild[sidx]*100.0f, universeSupply[sidx]*100.0f);
    drawMiniBar(x0 + 6, y0 + 29, w - 12, universeBuild[sidx], pxGold());
    drawTextf(x0 + 6, y0 + 39, grgb(190,210,226), bg, "influence %.0f%%  threat %.0f%%", universeInfluence[sidx]*100.0f, universeThreat[sidx]*100.0f);
    drawMiniBar(x0 + 6, y0 + 51, (w - 16) / 2, clipf(universeInfluence[sidx],0,1), pxCyan());
    drawMiniBar(x0 + 9 + (w - 16) / 2, y0 + 51, (w - 16) / 2, clipf(universeThreat[sidx],0,1), pxRed());
    drawTextf(x0 + 6, y0 + 60, pxCyan(), bg, "svc S%02u pilot S%02u %.1fk", universeServiceSector, universePilotSector, (float)universePilotDistance / 1000.0f);
  }

  void drawGalaxyMap(bool minerActive) {
    drawCinematicBackground();
    drawTopHud(minerActive);
    drawMindFeed();

    int mx = 10, my = 46, mw = 300, mh = 145;
    int cx = mx + mw / 2;
    int cy = my + 63;
    uint16_t bg = pxPanel();
    drawPanelFrame(mx, my, mw, mh, bg, pxFrame());
    drawText(mx + 8, my + 6, pxAmber(), bg, "JANUS CLUSTER / MILKY DB");
    drawTextf(mx + 180, my + 6, pxCyan(), bg, "%s", galaxyClusterName(galaxyClusterIndex));

    // Deterministic Elite-like procedural projection: stable seed, no external assets.
    for (int i = 0; i < 96; i++) {
      uint32_t h = 0x45D9F3BUL ^ (uint32_t)(i * 1103515245UL) ^ ((uint32_t)galaxyClusterIndex * 2654435761UL);
      int x = mx + 10 + (int)((h >> 7) % (mw - 20));
      int y = my + 18 + (int)((h >> 17) % (mh - 40));
      if ((h & 7) == 0) canvas.drawPixel(x, y, grgb(42, 48, 76));
      else if ((h & 31) == 0) canvas.drawPixel(x, y, grgb(70, 78, 110));
    }

    for (int r = 22; r <= 102; r += 20) {
      int er = (int)(r * (0.42f + 0.12f * cosf(galaxyMapPitch)));
      canvas.drawEllipse(cx, cy, (int)(r * galaxyMapZoom), max(8, er), grgb(22, 26, 42));
    }

    if (galaxyClusterIndex == 0) {
      pixCross(cx, cy, pxGold());
      drawText(cx + 8, cy - 4, pxGold(), bg, "SOL");
    } else {
      drawPixelGlow(cx, cy, factionColor(galaxyClusterIndex % 7), 9, (millis()/140)&31);
      pixCross(cx, cy, pxCyan());
      drawText(cx + 8, cy - 4, pxCyan(), bg, "JANUS HUB");
    }

    uint8_t count = galaxyStarCount();
    for (uint8_t i = 0; i < count; i++) {
      int sx, sy; galaxySystemScreen(i, cx, cy, sx, sy);
      if (sx < mx + 4 || sx > mx + mw - 4 || sy < my + 18 || sy > my + mh - 8) continue;
      uint8_t sec = galaxyStarSector(i);
      uint16_t col = galaxyStarColor(i);
      bool sel = (i == galaxySelectedStar);
      int rr = 2 + ((i + galaxyClusterIndex) % 3);
      if (sec == universeBuildTarget) canvas.drawCircle(sx, sy, rr + 5, pxGold());
      if (sec == universePilotSector) canvas.drawCircle(sx, sy, rr + 7, pxCyan());
      if (universeThreat[sec] > 0.70f) canvas.drawCircle(sx, sy, rr + 4, pxRed());
      if (sel) drawPixelGlow(sx, sy, pxGold(), 8, i);
      drawDitherDisc(sx, sy, rr, col, dim(col, 0.55f), bg, i + galaxyClusterIndex * 17);
      if (sel || i < 6 || (galaxyClusterIndex == 0 && i < 12)) {
        const char* nm = (galaxyClusterIndex == 0) ? milkySystemName(i) : janusSystemName(galaxyClusterIndex, i);
        drawText(sx + 5, sy - 2, sel ? pxGold() : dim(col, 0.90f), bg, nm);
      }
    }

    uint8_t sec = galaxyStarSector(galaxySelectedStar);
    drawPanelFrame(12, 154, 176, 55, bg, pxFrame());
    drawTextf(18, 160, pxAmber(), bg, "SELECT %s", selectedGalaxySystemName());
    drawTextf(18, 171, grgb(204,220,228), bg, "S%02u %s own:%s", sec, universePlanetName(sec), universeOwnerName(universeOwner[sec]));
    drawTextf(18, 182, factionColor(universeFaction[sec]), bg, "faction %s  rel %.0f%%", factionName(universeFaction[sec]), clipf((p[s.selected % JANUS_GALAXY_NODES].relation + 1.0f) * 50.0f, 0.0f, 100.0f));
    drawTextf(18, 193, pxCyan(), bg, "yaw %.0f zoom %.0f  star %u/%u", galaxyMapOrbit * 57.3f, galaxyMapZoom * 100.0f, galaxySelectedStar + 1, count);

    drawUniverseSectorMiniMap(192, 154, 118, 55, bg);
    drawText(16, 210, pxCyan(), pxBg(), "left/right: galaxy  edges: rotate  center: select");
    drawNavButtons();
    drawNewsTicker();
  }

  void drawPlanet(int i, int cx, int cy) {
    int px, py; planetScreen(i, cx, cy, px, py);
    uint16_t col = planetColor(i);
    int rr = 4 + (int)clipf(p[i].signal * 5.0f + p[i].wealth * 0.8f, 0.0f, 7.0f);
    if (!perfMode && (int)s.selected == i) drawPixelGlow(px, py, pxGold(), rr+7, i);
    if (p[i].risk > 0.55f) canvas.drawCircle(px, py, rr + 5, pxRed());
    drawDitherDisc(px, py, rr, col, dim(col,0.65f), grgb(8,10,16), i);
    drawText(px - 12, py + rr + 5, p[i].online ? grgb(210,226,238) : grgb(110,118,138), pxBg(), p[i].role);
  }

  void drawBar(int x, int y, int w, float v, uint16_t col) {
    drawMiniBar(x, y, w, v, col);
  }
  void drawOpsView(bool minerActive) {
    drawCinematicBackground();
    drawTopHud(minerActive);
    drawMindFeed();
    uint16_t bg = pxPanel();

    drawPanelFrame(10, 46, 142, 150, bg, pxFrame());
    drawText(18, 52, pxAmber(), bg, "MODEL TELEMETRY");
    drawTextf(18, 68, grgb(220,232,245), bg, "understanding %3d%%", (int)(galaxyConfidence*100.0f));
    drawBar(18, 79, 118, galaxyConfidence, pxCyan());
    drawTextf(18, 92, grgb(220,232,245), bg, "swarm learn   %3d%%", (int)(swarmLearningTotal*100.0f));
    drawBar(18, 103, 118, clipf(swarmLearningTotal,0,1), pxGreen());
    drawTextf(18, 116, grgb(220,232,245), bg, "anti-entropy  %3d%%", (int)(swarmShield*100.0f));
    drawBar(18, 127, 118, swarmShield, pxAmber());
    drawTextf(18, 140, grgb(220,232,245), bg, "combat wins %u", pirateKills);
    drawBar(18, 151, 118, clipf((float)pirateKills / 16.0f,0,1), pxRed());
    drawTextf(18, 164, grgb(220,232,245), bg, "tech %s", techName(techUnlocked));
    drawBar(18, 175, 118, techProgress, pxCyan());
    drawTextf(18, 187, grgb(190,210,226), bg, "risk %02d morale %02d", (int)(clipf(spaceRisk,0,1)*100.0f), (int)(s.morale*100.0f));

    drawPanelFrame(160, 46, 150, 150, bg, pxFrame());
    drawText(168, 52, pxAmber(), bg, "STATION INFRA");
    drawTextf(168, 66, pxGold(), bg, "credits %.0f quality %02d", s.credits, (int)(clipf(s.stationQuality / 1.2f,0,1)*100.0f));
    drawTextf(168, 78, grgb(180,220,210), bg, "gate %s mk%u fee %.0f", s.gateInstalled ? "online" : "offline", s.gateLevel, s.gateInstalled ? jumpFee(0, routeTo) : gateInstallCost());
    for (int r = 0; r < 3; r++) {
      for (int c = 0; c < 3; c++) {
        int idx = r * 3 + c;
        int tx = 168 + c * 46;
        int ty = 94 + r * 18;
        drawTextf(tx, ty, (s.moduleLevel[idx] > 0) ? pxCyan() : grgb(120,130,142), bg, "%s L%u", moduleShort(idx), s.moduleLevel[idx]);
        drawMiniBar(tx, ty + 9, 36, s.moduleLevel[idx] / 5.0f, (idx == MOD_RESEARCH) ? pxCyan() : ((idx == MOD_DOCKS || idx == MOD_SHIPYARD) ? pxAmber() : pxGreen()));
      }
    }
    drawClippedText(168, 154, grgb(220,230,242), bg, janusLine, 22);
    drawClippedText(168, 166, grgb(156,186,220), bg, contractLine, 22);
    drawClippedText(168, 178, pxGreen(), bg, techLine, 22);
    drawTextf(168, 190, grgb(160,190,220), bg, "rep %.2f admin %.2f", s.reputation, s.adminSkill);

    drawNavButtons();
    drawNewsTicker();
  }
  void drawFleetView(bool minerActive) {
    drawCinematicBackground();
    drawTopHud(minerActive);
    drawMindFeed();

    uint16_t bg = pxPanel();
    drawPanelFrame(10, 46, 300, 58, bg, pxFrame());
    drawText(18, 52, pxAmber(), bg, "FLEET / MARKET / CONTRACTS");
    for (int i = 0; i < 9; i++) {
      int bx = 16 + i * 33, by = 82;
      uint16_t col = (i == s.selectedShip) ? pxGold() : ((ships[i].kind == 2) ? pxRed() : pxCyan());
      drawShipTriangle(bx+9, by, ships[i].phase*JANUS_TWO_PI + ships[i].wobble, col);
      canvas.drawRect(bx, by-11, 20, 18, dim(col, i == s.selectedShip ? 0.58f : 0.28f));
      drawMiniBar(bx, by+10, 20, clipf(ships[i].heat,0,1), pxAmber());
      drawTextf(bx+6, by+16, grgb(184,214,236), bg, "%u", i+1);
    }

    Ship &sh = ships[s.selectedShip % 9];
    drawPanelFrame(10, 112, 142, 80, bg, pxFrame());
    drawText(18, 118, pxAmber(), bg, "SELECTED SHIP");
    drawTextf(18, 130, grgb(220,232,245), bg, "%s #%u", shipKind(sh.kind), (unsigned)(s.selectedShip+1));
    drawTextf(18, 142, grgb(190,220,210), bg, "%s -> %s", p[sh.from].name, p[sh.to].name);
    drawTextf(18, 148, grgb(182,212,226), bg, "state %s gate %.0fcr", sh.state == 0 ? "out" : (sh.state == 1 ? "in" : "dock"), s.gateInstalled ? jumpFee(sh.from, sh.to) : 0.0f);
    drawTextf(18, 160, grgb(190,220,210), bg, "cargo %.0f heat %02d%%", sh.cargo, (int)(clipf(sh.heat,0,1)*100.0f));
    drawTextf(18, 172, grgb(190,220,210), bg, "phase %02d%% speed %02d", (int)(clipf(sh.phase,0,1)*100.0f), (int)(sh.speed*100.0f));
    drawSparkline(18, 184, 122, 8, sh.phase*6.0f + sh.wobble, pxCyan());

    int n = s.selected % JANUS_GALAXY_NODES;
    drawPanelFrame(160, 112, 150, 80, bg, pxFrame());
    drawText(168, 118, pxAmber(), bg, "MARKET / DIPLOMACY");
    drawTextf(168, 130, grgb(220,232,245), bg, "%s / %s", p[n].name, p[n].role);
    drawTextf(168, 142, grgb(190,220,210), bg, "wealth %02d relation %02d", (int)(clipf(p[n].wealth,0,1)*100.0f), (int)(clipf((p[n].relation+1.0f)*0.5f,0,1)*100.0f));
    drawTextf(168, 154, grgb(190,220,210), bg, "ore %.0f food %.0f", p[n].stock[0], p[n].stock[1]);
    drawTextf(168, 166, grgb(190,220,210), bg, "data %.0f energy %.0f", p[n].stock[2], p[n].stock[3]);
    drawMiniBar(168, 179, 40, clipf(p[n].stock[0]/220.0f,0,1), grgb(150,120,90));
    drawMiniBar(211, 179, 40, clipf(p[n].stock[1]/220.0f,0,1), pxGreen());
    drawMiniBar(254, 179, 40, clipf(p[n].stock[2]/220.0f,0,1), pxCyan());

    drawPanelFrame(10, 196, 300, 18, bg, pxFrame());
    drawText(16, 200, pxAmber(), bg, activeContract ? crew[contractCrew].name : "contract");
    drawMiniBar(68, 201, 58, contractProgress, pxGold());
    drawTextf(132, 200, pxCyan(), bg, "done %u", contractsDone);
    drawText(184, 200, pxAmber(), bg, techName(techUnlocked));
    drawMiniBar(238, 201, 36, techProgress, pxCyan());
    drawTextf(280, 200, pxGreen(), bg, "%u", techUnlocked);

    drawNavButtons();
    drawNewsTicker();
  }


  void drawMinerView(bool minerActive) {
    drawCinematicBackground();
    drawTopHud(minerActive);
    drawMindFeed();
    uint16_t bg = pxPanel();

    uint32_t now = millis();
    uint32_t jobAge = coreJob.receivedAt ? (now - coreJob.receivedAt) : 0;
    uint32_t shareAge = coreLastShareMs ? (now - coreLastShareMs) : 0;
    uint32_t runAge = coreWorkerStartedMs ? (now - coreWorkerStartedMs) : 0;
    float jobFresh = coreJob.receivedAt ? clipf(1.0f - ((float)jobAge / 18000.0f), 0.0f, 1.0f) : 0.0f;
    float rangeProgress = 0.0f;
    if (coreJob.rangeSize > 0) {
      uint32_t done = min(coreJob.thetaCursor, coreJob.rangeSize);
      rangeProgress = clipf((float)done / (float)coreJob.rangeSize, 0.0f, 1.0f);
    }
    float bestProgress = coreTargetBits ? clipf((float)coreBestBits / (float)coreTargetBits, 0.0f, 1.0f) : clipf((float)coreBestBits / 64.0f, 0.0f, 1.0f);
    float buzzBest = coreTargetBits ? clipf((float)buzz.bestBits / (float)coreTargetBits, 0.0f, 1.0f) : clipf((float)buzz.bestBits / 64.0f, 0.0f, 1.0f);

    drawPanelFrame(10, 46, 145, 150, bg, pxFrame());
    drawText(18, 52, pxAmber(), bg, "CORE2 MINER");
    drawTextf(18, 66, grgb(220,232,245), bg, "state %s", coreWorkerEnabled() ? (coreJob.active ? "ACTIVE" : "WAIT") : "OFF");
    drawTextf(18, 78, grgb(200,216,230), bg, "hash %s H/s", compactU(coreRemoteHashrate).c_str());
    drawMiniBar(18, 90, 118, clipf((float)coreRemoteHashrate / 1200.0f, 0.0f, 1.0f), pxCyan());
    drawTextf(18, 102, grgb(200,216,230), bg, "best %lu / target %u", (unsigned long)coreBestBits, (unsigned)coreTargetBits);
    drawMiniBar(18, 114, 118, bestProgress, pxGold());
    drawTextf(18, 126, grgb(200,216,230), bg, "shares %lu jobs %lu", (unsigned long)coreSharesSent, (unsigned long)coreJobsSeen);
    drawTextf(18, 138, grgb(200,216,230), bg, "expired %lu ai %.2f", (unsigned long)coreJobExpired, coreAiSkill);
    drawTextf(18, 150, pxCyan(), bg, "theta r%.2f q%.2f", coreTheta.resonance, coreTheta.q);
    drawTextf(18, 162, grgb(200,216,230), bg, "run %lus job %lus", (unsigned long)(runAge/1000UL), (unsigned long)(jobAge/1000UL));
    drawMiniBar(18, 174, 118, jobFresh, jobFresh > 0.35f ? pxGreen() : pxRed());
    drawTextf(18, 184, grgb(200,216,230), bg, "nonce %.0f%%", rangeProgress * 100.0f);
    drawMiniBar(18, 192, 118, rangeProgress, pxAmber());

    drawPanelFrame(165, 46, 145, 150, bg, pxFrame());
    drawText(173, 52, pxAmber(), bg, "BUZZ / SWARM POOL");
    drawTextf(173, 66, grgb(220,232,245), bg, "buzz %s", buzz.online ? "ONLINE" : "OFFLINE");
    drawTextf(173, 78, grgb(200,216,230), bg, "hash %s H/s", compactU(buzz.hashRate).c_str());
    drawMiniBar(173, 90, 118, clipf((float)buzz.hashRate / 5000.0f, 0.0f, 1.0f), pxCyan());
    drawTextf(173, 102, grgb(200,216,230), bg, "shares %lu rej %lu", (unsigned long)buzz.shares, (unsigned long)buzz.rejects);
    drawTextf(173, 114, grgb(200,216,230), bg, "best %lu diff %.2f", (unsigned long)buzz.bestBits, buzz.diff);
    drawMiniBar(173, 126, 118, buzzBest, pxGold());
    drawTextf(173, 138, grgb(200,216,230), bg, "job %s", coreJobText);
    drawTextf(173, 150, grgb(200,216,230), bg, "range %lu", (unsigned long)coreJob.rangeSize);
    drawTextf(173, 162, grgb(200,216,230), bg, "last share %lus", (unsigned long)(shareAge/1000UL));
    drawMiniBar(173, 174, 118, clipf(homeSync(),0,1), pxGreen());
    drawTextf(173, 184, grgb(170,196,218), bg, "sync %.2f entropy %.2f", homeSync(), homeEntropy());

    // Pixel reactor strip: visual pulse of mining load without covering the scene.
    int y = 203;
    for (int i = 0; i < 38; i++) {
      int x = 12 + i * 8;
      uint8_t h = hash8(x, y, (millis()/110) + i);
      uint16_t col = ((h & 3) == 0) ? pxGold() : (((h & 7) < 4) ? pxCyan() : grgb(28, 34, 48));
      if (!minerActive) col = dim(col, 0.35f);
      canvas.drawPixel(x, y + (h & 3), col);
      if ((h & 15) == 0) canvas.drawPixel(x+1, y + (h & 3), dim(col,0.55f));
    }

    drawNavButtons();
    drawNewsTicker();
  }


  void drawModuleInterior(int x, int y, uint8_t idx) {
    uint16_t c = moduleColor(idx);
    uint16_t wall = grgb(20, 24, 34);
    canvas.fillRect(x, y, 62, 36, grgb(8, 10, 16));
    canvas.drawRect(x, y, 62, 36, dim(c, 0.70f));
    canvas.drawFastHLine(x + 2, y + 29, 58, wall);
    switch (idx % JANUS_MODULE_N) {
      case MOD_BAR:
        canvas.fillRect(x + 8, y + 20, 32, 4, dim(c,0.55f));
        canvas.fillRect(x + 12, y + 13, 4, 7, pxGold()); canvas.fillRect(x + 30, y + 13, 4, 7, pxGreen());
        canvas.drawPixel(x + 46, y + 18, pxCyan()); canvas.drawPixel(x + 50, y + 18, pxCyan());
        break;
      case MOD_MEDBAY:
        canvas.fillRect(x + 9, y + 20, 28, 5, grgb(170,238,210));
        canvas.drawFastHLine(x + 43, y + 14, 9, pxRed()); canvas.drawFastVLine(x + 47, y + 10, 9, pxRed());
        canvas.drawLine(x + 52, y + 8, x + 52, y + 23, dim(c,0.8f));
        break;
      case MOD_SHIPYARD:
        canvas.drawRect(x + 12, y + 17, 28, 8, dim(c,0.8f));
        canvas.drawLine(x + 12, y + 25, x + 6, y + 30, dim(c,0.6f));
        canvas.drawPixel(x + 46, y + 17, pxAmber()); canvas.drawPixel(x + 48, y + 15, pxGold());
        break;
      case MOD_RESEARCH:
        canvas.drawRect(x + 11, y + 12, 16, 12, pxCyan());
        canvas.drawLine(x + 36, y + 24, x + 48, y + 10, pxCyan());
        canvas.drawCircle(x + 50, y + 9, 3, pxCyan());
        break;
      case MOD_DOCKS:
        canvas.drawRect(x + 8, y + 12, 46, 16, dim(c,0.8f));
        canvas.drawFastVLine(x + 24, y + 12, 16, pxAmber()); canvas.drawFastVLine(x + 40, y + 12, 16, pxAmber());
        canvas.drawPixel(x + 18, y + 20, pxCyan()); canvas.drawPixel(x + 46, y + 20, pxGreen());
        break;
      default:
        for (int i = 0; i < 6; i++) canvas.fillRect(x + 9 + i*8, y + 14 + (i&1)*5, 4, 4, dim(c,0.55f));
        canvas.drawFastHLine(x + 9, y + 25, 44, dim(c,0.75f));
        break;
    }
  }
  void drawModuleDetailOverlay() {
    if (!moduleDetailOpen) return;
    uint16_t bg = grgb(4, 6, 12);
    drawPanelFrame(72, 56, 176, 124, bg, pxFrame());
    drawTextf(82, 64, pxGold(), bg, "%s / Lv.%u", moduleName(selectedModule), s.moduleLevel[selectedModule]);
    drawTextf(82, 76, moduleColor(selectedModule), bg, "next %.0fcr  bonus %.0f%%", moduleUpgradeCost(selectedModule), moduleBonusValue(selectedModule));
    drawClippedText(82, 90, grgb(208,224,236), bg, moduleDesc(selectedModule), 27);
    drawModuleInterior(129, 110, selectedModule);
    drawText(92, 164, grgb(150,170,190), bg, "tap outside to close");
  }

  void drawModulesView(bool minerActive) {
    drawCinematicBackground();
    drawTopHud(minerActive);
    drawMindFeed();
    uint16_t bg = pxPanel();

    drawPanelFrame(10, 44, 118, 150, bg, pxFrame());
    drawText(18, 50, pxAmber(), bg, "MODULES");
    for (int i = 0; i < JANUS_MODULE_N; i++) {
      int y = 64 + i * 13;
      uint16_t c = (i == selectedModule) ? pxGold() : moduleColor(i);
      drawTextf(18, y, c, bg, "%s", moduleName(i));
      drawTextf(78, y, grgb(186,208,228), bg, "L%u", s.moduleLevel[i]);
      drawMiniBar(92, y + 2, 24, s.moduleLevel[i] / 5.0f, c);
    }

    drawPanelFrame(132, 44, 178, 150, bg, pxFrame());
    drawText(140, 50, pxAmber(), bg, "STATION MANAGEMENT");
    drawTextf(140, 64, moduleColor(selectedModule), bg, "%s / Lv.%u", moduleName(selectedModule), s.moduleLevel[selectedModule]);
    drawTextf(140, 76, grgb(184,210,228), bg, "next %.0fcr  bonus %.0f%%", moduleUpgradeCost(selectedModule), moduleBonusValue(selectedModule));
    drawTextf(140, 88, grgb(184,210,228), bg, "quality %02d  gate %s mk%u", (int)(clipf(s.stationQuality / 1.2f,0,1)*100.0f), s.gateInstalled ? "on" : "off", s.gateLevel);
    drawClippedText(140, 102, grgb(208,224,236), bg, moduleDesc(selectedModule), 26);
    drawModuleSchematic(216, 126, 1.0f, selectedModule);
    if (s.gateInstalled) {
      drawJumpGate(246, 102, pxAmber(), true, s.gateLevel + 3);
      drawTextf(220, 168, pxGold(), bg, "gate fee %.0fcr", jumpFee(routeFrom, routeTo));
    } else {
      canvas.drawCircle(246, 102, 18, dim(pxAmber(), 0.28f));
      canvas.drawCircle(246, 102, 12, dim(pxAmber(), 0.16f));
      drawTextf(214, 168, pxAmber(), bg, "install %.0fcr", gateInstallCost());
    }
    drawTextf(140, 180, grgb(154,188,210), bg, "Janus buys upgrades automatically");

    drawNavButtons();
    drawModuleDetailOverlay();
  }

  void drawCodexView(bool minerActive) {
    drawCinematicBackground();
    drawTopHud(minerActive);
    drawMindFeed();
    uint16_t bg = pxPanel();
    drawPanelFrame(10, 44, 300, 150, bg, pxFrame());
    drawText(18, 50, pxAmber(), bg, "CODEX / QUESTS / ALLIANCES");
    drawTextf(18, 62, pxCyan(), bg, "battles %u  contracts %u  tech %u  alliances %u", s.codexBattles, s.codexContracts, s.codexTechs, s.codexAlliances);
    drawText(18, 75, grgb(210,224,236), bg, codexLine);

    int y = 92;
    for (int i = 0; i < JANUS_CREW_N; i++) {
      uint16_t c = (i == selectedCodexCrew) ? pxGold() : grgb(184,204,220);
      uint8_t step = s.crewQuestStep[i];
      float q = s.crewQuestProgress[i];
      drawTextf(18, y, c, bg, "%s %s", crew[i].name, roleName(crew[i].role));
      if (step >= 3) {
        drawText(88, y, pxGreen(), bg, "COMPLETE");
        drawMiniBar(176, y + 2, 96, 1.0f, pxGreen());
      } else {
        drawTextf(88, y, grgb(210,224,236), bg, "G%u %s", (unsigned)(step + 1), questTitle(i, step));
        drawMiniBar(206, y + 2, 80, q, (i == selectedCodexCrew) ? pxGold() : pxCyan());
      }
      y += 12;
    }

    drawPanelFrame(18, 176, 284, 18, bg, pxFrame());
    uint8_t a = s.selected % JANUS_GALAXY_NODES;
    bool allied = (s.allianceMask & (1 << a)) != 0;
    drawTextf(24, 181, allied ? pxGreen() : pxAmber(), bg, "diplomacy: %s rel %.0f%%  %s", p[a].name, clipf((p[a].relation + 1.0f) * 50.0f, 0.0f, 100.0f), allied ? "ALLY" : "no pact");
    drawNavButtons();
  }

  void draw(bool minerActive) {
    if (s.viewMode == VIEW_GALAXY) drawGalaxyMap(minerActive);
    else if (s.viewMode == VIEW_OPS) drawOpsView(minerActive);
    else if (s.viewMode == VIEW_FLEET) drawFleetView(minerActive);
    else if (s.viewMode == VIEW_MINER) drawMinerView(minerActive);
    else if (s.viewMode == VIEW_CODEX) drawCodexView(minerActive);
    else if (s.viewMode == VIEW_MODULES) drawModulesView(minerActive);
    else drawStationView(minerActive);
  }

  void drawLog() {
    // v3.1: integrated into cinematic HUD and news ticker.
  }
};

JanusGalaxyStationSim galaxy;

bool core2SdArchiveOk = false;
uint32_t core2SdArchiveRows = 0;
uint32_t core2LastUniverseArchiveMs = 0;

void initCore2SdArchive() {
#if CORE2_SD_ARCHIVE_ENABLE
  // SD is optional. If it fails, Core2 still runs as a live station + ESP-NOW node.
  if (SD.begin(CORE2_SD_CS)) {
    core2SdArchiveOk = true;
    if (!SD.exists("/janus")) SD.mkdir("/janus");
    File f = SD.open("/janus/core2_universe.csv", FILE_APPEND);
    if (f) {
      if (f.size() == 0) {
        f.println("ms,epoch,cluster,star,build_target,service_sector,pilot_sector,pilot_kly,janus_inf,thargoid,supply,service_prog,hash,best,shares,rssi,eco2,tvoc,online,heap");
      }
      f.close();
    }
    Serial.println("[SD] Janus Universe archive ready /janus/core2_universe.csv");
  } else {
    core2SdArchiveOk = false;
    Serial.println("[SD] archive unavailable; continuing without SD training log");
  }
#else
  core2SdArchiveOk = false;
#endif
}

void appendCore2UniverseArchive() {
#if CORE2_SD_ARCHIVE_ENABLE
  uint32_t now = millis();
  if (!core2SdArchiveOk || now - core2LastUniverseArchiveMs < CORE2_UNIVERSE_ARCHIVE_MS) return;
  core2LastUniverseArchiveMs = now;
  File f = SD.open("/janus/core2_universe.csv", FILE_APPEND);
  if (!f) { core2SdArchiveOk = false; Serial.println("[SD] universe archive open failed"); return; }
  String row;
  row.reserve(220);
  row += String(now); row += ',';
  row += String(galaxy.universeEpoch); row += ',';
  row += String(galaxy.galaxyClusterIndex); row += ',';
  row += String(galaxy.galaxySelectedStar); row += ',';
  row += String(galaxy.universeBuildTarget); row += ',';
  row += String(galaxy.universeServiceSector); row += ',';
  row += String(galaxy.universePilotSector); row += ',';
  row += String((float)galaxy.universePilotDistance / 1000.0f, 3); row += ',';
  row += String(galaxy.universeJanusInfluence, 4); row += ',';
  row += String(galaxy.universeThargoidPressure, 4); row += ',';
  row += String(galaxy.universeTradeDemand, 4); row += ',';
  row += String(galaxy.universeServiceProgress, 4); row += ',';
  row += String(coreRemoteHashrate); row += ',';
  row += String(coreBestBits); row += ',';
  row += String(coreSharesSent); row += ',';
  row += String(wifiOk ? WiFi.RSSI() : -127); row += ',';
  row += String(eco2); row += ',';
  row += String(tvoc); row += ',';
  row += String(colonyOnlineCount); row += ',';
  row += String(ESP.getFreeHeap());
  f.println(row);
  f.close();
  core2SdArchiveRows++;
#endif
}

void setCoreBrightness(uint8_t b, bool save = true) {
  coreBrightness = constrain(b, (uint8_t)25, (uint8_t)255);
  M5.Display.setBrightness(coreBrightness);
  if (save) prefs.putUChar("bright", coreBrightness);
}

void brightnessStep(int delta) {
  int v = (int)coreBrightness + delta;
  if (v < 25) v = 25;
  if (v > 255) v = 255;
  setCoreBrightness((uint8_t)v, true);
  eventLine = String("brightness ") + String((int)((coreBrightness * 100) / 255)) + "%";
}

void hapticPulse(uint8_t power, uint16_t ms) {
#if CORE2_HAPTIC_ENABLE
  M5.Power.setVibration(power);
  hapticOffAt = millis() + ms;
#else
  (void)power;
  (void)ms;
  M5.Power.setVibration(0);
  hapticOffAt = 0;
#endif
}

void hapticTick() {
#if CORE2_HAPTIC_ENABLE
  if (hapticOffAt && millis() >= hapticOffAt) {
    M5.Power.setVibration(0);
    hapticOffAt = 0;
  }
#else
  // safety: if older firmware left motor latched, keep it off
  if (hapticOffAt || (millis() & 0x3FF) < 3) {
    M5.Power.setVibration(0);
    hapticOffAt = 0;
  }
#endif
}

int pickNodeIndex(const String& id) {
  // v6.20 semantic slots for existing UI pages.
  // Unknown/future devices still go into the universal registry and are visible on MESH.
  if (id.indexOf("BlindEye") >= 0 || id.indexOf("blind_eye") >= 0 || id.indexOf("EYE_BLIND") >= 0 || id.indexOf("Eye") >= 0) return 0;
  if (id.indexOf("Beacon") >= 0 || id.indexOf("Cardputer") >= 0 || id.indexOf("ADV") >= 0 || id.indexOf("BCN") >= 0) return 1;
  if (id.indexOf("Buzz") >= 0 || id.indexOf("Lighter") >= 0 || id.indexOf("Miner") >= 0 || id.indexOf("Pool") >= 0) return 2;
  if (id.indexOf("EchoMic") >= 0 || id.indexOf("Audio") >= 0 || id.indexOf("Mic") >= 0) return 3;
  if (id.indexOf("ATOM_SWARM_TRON") >= 0 || id.indexOf("ATOM") >= 0 || id.indexOf("Tron") >= 0 ||
      id.indexOf("Throne") >= 0 || id.indexOf("Swarm") >= 0 || id.indexOf("TD") >= 0 || id.indexOf("SWRM") >= 0) return 4;
  if (id.indexOf("Stick3") >= 0 || id.indexOf("StickS3") >= 0 || id.indexOf("Stick") >= 0 ||
      id.indexOf("M5Stick") >= 0 || id.indexOf("AlienRogue") >= 0 || id.indexOf("Rogue") >= 0 ||
      id.indexOf("STK") >= 0) return 5;
  if (id.indexOf("ZimGeek") >= 0 || id.indexOf("Zim") >= 0 || id.indexOf("ZIM_SOLO") >= 0 ||
      id.indexOf("zim_solo") >= 0 || id.indexOf("ZIM") >= 0) return 6;
  return 6;
}

void* semanticNodeSlot(uint8_t idx) {
  switch (idx) {
    case 0: return &eye;
    case 1: return &beacon;
    case 2: return &buzz;
    case 3: return &audioNode;
    case 4: return &swarm;
    case 5: return &stick;
    default: return &unknownNode;
  }
}

const char* semanticSlotName(uint8_t idx) {
  switch (idx) {
    case 0: return "EYE";
    case 1: return "BCN";
    case 2: return "BUZ";
    case 3: return "MIC";
    case 4: return "SWRM";
    case 5: return "STK";
    default: return "ZIM";
  }
}

bool macIsZero(const uint8_t mac[6]) {
  if (!mac) return true;
  for (int i = 0; i < 6; i++) if (mac[i]) return false;
  return true;
}

void formatMacShort(const uint8_t mac[6], char* out, size_t outLen) {
  if (!out || outLen == 0) return;
  if (!mac || macIsZero(mac)) { strlcpy(out, "--:--", outLen); return; }
  snprintf(out, outLen, "%02X:%02X:%02X", mac[3], mac[4], mac[5]);
}


int universalFindNode(const char* id, const char* role) {
  if ((!id || !id[0]) && (!role || !role[0])) return -1;
  for (int i = 0; i < CORE2_MAX_COLONY_NODES; i++) {
    if (!colonyNodes[i].used) continue;
    if (id && id[0] && !strncmp(colonyNodes[i].nodeId, id, sizeof(colonyNodes[i].nodeId))) return i;
    if (role && role[0] && !strncmp(colonyNodes[i].role, role, sizeof(colonyNodes[i].role)) &&
        (!id || !id[0] || !colonyNodes[i].nodeId[0])) return i;
  }
  return -1;
}

int universalAllocNode(const char* id, const char* role) {
  int freeIdx = -1;
  int oldestIdx = 0;
  uint32_t oldest = 0xFFFFFFFFUL;

  for (int i = 0; i < CORE2_MAX_COLONY_NODES; i++) {
    if (!colonyNodes[i].used && freeIdx < 0) freeIdx = i;
    if (colonyNodes[i].used && colonyNodes[i].lastMs < oldest) {
      oldest = colonyNodes[i].lastMs;
      oldestIdx = i;
    }
  }

  int idx = (freeIdx >= 0) ? freeIdx : oldestIdx;
  memset(&colonyNodes[idx], 0, sizeof(colonyNodes[idx]));
  colonyNodes[idx].used = true;
  colonyNodes[idx].firstMs = millis();
  if (id && id[0]) strlcpy(colonyNodes[idx].nodeId, id, sizeof(colonyNodes[idx].nodeId));
  if (role && role[0]) strlcpy(colonyNodes[idx].role, role, sizeof(colonyNodes[idx].role));
  String key = String(colonyNodes[idx].nodeId) + String(colonyNodes[idx].role);
  colonyNodes[idx].semanticSlot = (uint8_t)pickNodeIndex(key);
  colonyNewNodeEvents++;
  eventLine = String("new node ") + (colonyNodes[idx].nodeId[0] ? colonyNodes[idx].nodeId : colonyNodes[idx].role);
  return idx;
}

void universalRecountNodes() {
  uint8_t known = 0;
  uint8_t online = 0;
  uint8_t future = 0;
  uint32_t bestScore = 0;
  strlcpy(colonyTopNode, "-", sizeof(colonyTopNode));

  uint32_t now = millis();
  for (int i = 0; i < CORE2_MAX_COLONY_NODES; i++) {
    if (!colonyNodes[i].used) continue;
    known++;
    colonyNodes[i].online = (colonyNodes[i].lastMs > 0 && now - colonyNodes[i].lastMs < NODE_TIMEOUT_MS);
    if (colonyNodes[i].online) {
      online++;
      if (colonyNodes[i].semanticSlot == 6) future++;
      uint32_t score = colonyNodes[i].hashRate / 25UL + colonyNodes[i].bestBits * 6UL + colonyNodes[i].shares * 90UL + colonyNodes[i].packets;
      if (score >= bestScore) {
        bestScore = score;
        strlcpy(colonyTopNode, colonyNodes[i].nodeId[0] ? colonyNodes[i].nodeId : colonyNodes[i].role, sizeof(colonyTopNode));
      }
    }
  }

  colonyKnownCount = known;
  colonyOnlineCount = online;
  colonyFutureCount = future;
  colonyLastRosterMs = now;
}

int universalRememberNodeEx(const char* id, const char* role, int8_t rssi, const uint8_t* mac) {
  int idx = universalFindNode(id, role);
  if (idx < 0) idx = universalAllocNode(id, role);
  if (idx < 0) return -1;

  if (id && id[0]) strlcpy(colonyNodes[idx].nodeId, id, sizeof(colonyNodes[idx].nodeId));
  if (role && role[0]) strlcpy(colonyNodes[idx].role, role, sizeof(colonyNodes[idx].role));
  String key = String(colonyNodes[idx].nodeId) + String(colonyNodes[idx].role);
  colonyNodes[idx].semanticSlot = (uint8_t)pickNodeIndex(key);
  colonyNodes[idx].lastMs = millis();
  colonyNodes[idx].online = true;
  colonyNodes[idx].packets++;
  if (rssi) colonyNodes[idx].rssi = rssi;
  if (mac && !macIsZero(mac)) memcpy(colonyNodes[idx].mac, mac, 6);
  universalRecountNodes();
  return idx;
}

int universalRememberNode(const char* id, const char* role, int8_t rssi) {
  return universalRememberNodeEx(id, role, rssi, nullptr);
}

void universalMirrorToFixedSlot(int idx) {
  if (idx < 0 || idx >= CORE2_MAX_COLONY_NODES || !colonyNodes[idx].used) return;

  RemoteNode* n = (RemoteNode*)semanticNodeSlot(colonyNodes[idx].semanticSlot);
  if (!n) return;

  // For semantic pages, keep the most recent node of that family.
  n->touch(colonyNodes[idx].nodeId[0] ? colonyNodes[idx].nodeId : colonyNodes[idx].role);
  strlcpy(n->role, colonyNodes[idx].role, sizeof(n->role));
  n->worker = colonyNodes[idx].worker;
  n->rssi = colonyNodes[idx].rssi;
  n->entropy = colonyNodes[idx].entropy;
  n->loss = colonyNodes[idx].loss;
  n->sync = colonyNodes[idx].sync;
  n->fit = colonyNodes[idx].fit;
  n->v0 = colonyNodes[idx].v[0];
  n->v1 = colonyNodes[idx].v[1];
  n->v2 = colonyNodes[idx].v[2];
  n->v3 = colonyNodes[idx].v[3];
  n->v4 = colonyNodes[idx].v[4];
  n->v5 = colonyNodes[idx].v[5];
  n->v6 = colonyNodes[idx].v[6];
  n->v7 = colonyNodes[idx].v[7];
  n->hashRate = colonyNodes[idx].hashRate;
  n->shares = colonyNodes[idx].shares;
  n->rejects = colonyNodes[idx].rejects;
  n->bestBits = colonyNodes[idx].bestBits;
  n->diff = colonyNodes[idx].diff;
}

float universalMeanEntropy() {
  float sum = 0.0f;
  int n = 0;
  uint32_t now = millis();
  for (int i = 0; i < CORE2_MAX_COLONY_NODES; i++) {
    if (!colonyNodes[i].used || !colonyNodes[i].lastMs || now - colonyNodes[i].lastMs >= NODE_TIMEOUT_MS) continue;
    sum += colonyNodes[i].entropy;
    n++;
  }
  return n ? sum / (float)n : 0.0f;
}

float universalMeanSync() {
  float sum = 0.0f;
  int n = 0;
  uint32_t now = millis();
  for (int i = 0; i < CORE2_MAX_COLONY_NODES; i++) {
    if (!colonyNodes[i].used || !colonyNodes[i].lastMs || now - colonyNodes[i].lastMs >= NODE_TIMEOUT_MS) continue;
    if (colonyNodes[i].sync <= 0.0f) continue;
    sum += colonyNodes[i].sync;
    n++;
  }
  return n ? sum / (float)n : 0.0f;
}

uint8_t getWifiChannelSafe() {
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) != ESP_OK) return 0;
  return primary;
}

void ensureColonyPeer() {
  if (!espnowOk) return;
  if (millis() - colonyLastPeerFixMs < COLONY_PEER_FIX_MS) return;
  colonyLastPeerFixMs = millis();

  uint8_t ch = getWifiChannelSafe();
  if (ch == 0) ch = JANUS_BROADCAST_CHANNEL;

  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC) && colonyPeerChannel == ch) return;
  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) esp_now_del_peer(JANUS_BROADCAST_MAC);

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6);
  peer.channel = ch;
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) == ESP_OK) {
    colonyPeerChannel = ch;
    Serial.printf("[COLONY] peer channel=%u\n", ch);
  }
}

void initWiFiEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  uint32_t until = millis() + 5000;
  while (WiFi.status() != WL_CONNECTED && millis() < until) {
    delay(50);
  }

  wifiOk = WiFi.status() == WL_CONNECTED;
  Serial.printf("[WIFI] %s channel=%u rssi=%d\n", wifiOk ? "OK" : "OFF", getWifiChannelSafe(), wifiOk ? WiFi.RSSI() : -127);

  if (esp_now_init() != ESP_OK) {
    espnowOk = false;
    Serial.println("[COLONY] ESP-NOW init failed");
    return;
  }

  espnowOk = true;
  esp_now_register_recv_cb(onColonyRecv);
  ensureColonyPeer();

  colonyWorkerId = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
  Serial.printf("[COLONY] Core2Home ready worker=%u channel=%u\n", colonyWorkerId, colonyPeerChannel);
}

// ========================= SGP30 =========================

bool i2cProbe(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void scanI2C() {
  Serial.println("[I2C] Core2 Port.A scan SDA=32 SCL=33");
  bool any = false;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[I2C] found 0x%02X\n", a);
      any = true;
    }
  }
  if (!any) Serial.println("[I2C] none");
}

void initSGP30() {
  Wire.begin(CORE2_PORTA_SDA, CORE2_PORTA_SCL, 100000U);
  Wire.setClock(100000U);
  delay(50);
  scanI2C();

  if (!i2cProbe(SGP30_ADDR)) {
    sgpReady = false;
    eventLine = "SGP30 missing 0x58";
    Serial.println("[SGP30] no ACK at 0x58");
    return;
  }

  sgpReady = sgp.begin(&Wire);
  if (!sgpReady) {
    eventLine = "SGP30 begin failed";
    Serial.println("[SGP30] begin failed");
    return;
  }

  sgp.IAQinit();
  sgpWarmupStart = millis();

  uint16_t eco2Base = prefs.getUShort("eco2base", 0);
  uint16_t tvocBase = prefs.getUShort("tvocbase", 0);
  if (eco2Base != 0 && tvocBase != 0) {
    sgp.setIAQBaseline(eco2Base, tvocBase);
    Serial.printf("[SGP30] loaded baseline eCO2=0x%04X TVOC=0x%04X\n", eco2Base, tvocBase);
  }

  eventLine = "SGP30 ready";
  Serial.println("[SGP30] ready");
}

void readSGP30() {
  if (!sgpReady) {
    static uint32_t lastRetry = 0;
    if (millis() - lastRetry > 10000UL) {
      lastRetry = millis();
      initSGP30();
    }
    return;
  }

  if (!sgp.IAQmeasure()) {
    eventLine = "SGP30 read fail";
    Serial.println("[SGP30] IAQmeasure failed");
    return;
  }

  uint16_t oldEco2 = eco2;
  uint16_t oldTvoc = tvoc;

  eco2 = sgp.eCO2;
  tvoc = sgp.TVOC;

  if (sgp.IAQmeasureRaw()) {
    rawH2 = sgp.rawH2;
    rawEthanol = sgp.rawEthanol;
  }

  float d = fabsf((float)eco2 - (float)oldEco2) / 100.0f + fabsf((float)tvoc - (float)oldTvoc) / 80.0f;
  airTrend = airTrend * 0.86f + d * 0.14f;

  float eco2Drive = clipf(((float)eco2 - 400.0f) / 1600.0f, 0.0f, 4.0f);
  float tvocDrive = clipf((float)tvoc / 1000.0f, 0.0f, 4.0f);
  airScore = clipf(eco2Drive * 0.58f + tvocDrive * 0.72f + airTrend * 0.20f, 0.0f, 10.0f);
  airEntropy = clipf(airEntropy * 0.80f + (airScore + airTrend) * 0.20f, 0.0f, 10.0f);

  if (tvoc > 600 || eco2 > 1500) eventLine = "AIR ALERT";
  else if (tvoc > 220 || eco2 > 900) eventLine = "air watch";
  else eventLine = "air stable";

  if (millis() - lastBaselineAt > SGP30_BASELINE_MS && millis() - sgpWarmupStart > 120000UL) {
    lastBaselineAt = millis();
    uint16_t eco2Base = 0, tvocBase = 0;
    if (sgp.getIAQBaseline(&eco2Base, &tvocBase)) {
      prefs.putUShort("eco2base", eco2Base);
      prefs.putUShort("tvocbase", tvocBase);
      Serial.printf("[SGP30] saved baseline eCO2=0x%04X TVOC=0x%04X\n", eco2Base, tvocBase);
    }
  }
}

// Manual prototypes: keeps Arduino preprocessor from guessing badly.
void sendCore2Heartbeat();
void sendCore2Entropy();
void sendCore2SwarmSense();
void initCore2SdArchive();
void appendCore2UniverseArchive();
void handleSwarmSenseRaw(const uint8_t* data, uint16_t len, int8_t rxRssi, const uint8_t* mac = nullptr);
void processRxFrames();
float homeEntropy();
float homeSync();
void runCore2MiningBatch();
void updateSwarmSpace();
void spaceInit();
void spaceResetNodeLayout();
void spaceSave(bool force = false);
void spaceSetEvent(uint8_t eventId, float intensity, const char* label);
void drawScreen();
void handleInput();
void sendBuzzControl(const char* cmd, int32_t value = 0);
void sendJanusAudioControl(bool enable, bool force = false);
void janusAudioLiveTick();
void janusAudioPlaybackTick();
bool janusEyeVisionShouldListen();
void sendJanusEyeVisionControl(bool enable, bool force = false);
void janusEyeVisionTick();
bool handleJanusEyeFrameRaw(const uint8_t* data, uint16_t len, int8_t rxRssi);
void handleJanusTachyonProphecyRaw(const uint8_t* data, uint16_t len, int8_t rxRssi, const uint8_t* mac = nullptr);
void handleJanusKenshiRaw(const uint8_t* data, uint16_t len, int8_t rxRssi, const uint8_t* mac = nullptr);
void core2TachyonProphecyTick();
bool coreWorkerEnabled();
void janusEyeVisionSynthesizeFromEye(bool force = false);
void handleZimAgentMemoryRaw(const uint8_t* data, uint16_t len, int8_t rxRssi, const uint8_t* mac = nullptr);
bool janusAudioShouldListen();
void handleJanusAudioFrameRaw(const uint8_t* data, uint16_t len, int8_t rxRssi);
void updateWeather(bool force = false);
void hapticPulse(uint8_t power = 90, uint16_t ms = HAPTIC_PULSE_MS);

// ========================= MUSIC / BUZZ CONTROL =========================

bool httpGetRaw(const char* url, String* out, uint16_t timeoutMs = 900) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (millis() - lastHttpAt < 180UL) return false;
  lastHttpAt = millis();

  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  if (!http.begin(url)) return false;
  int code = http.GET();
  if (out) *out = (code > 0) ? http.getString() : "";
  http.end();
  return code >= 200 && code < 300;
}

const char* weatherCodeText(int code) {
  if (code == 0) return "clear";
  if (code == 1 || code == 2) return "partly cloudy";
  if (code == 3) return "cloudy";
  if (code == 45 || code == 48) return "fog";
  if ((code >= 51 && code <= 67) || (code >= 80 && code <= 82)) return "rain";
  if ((code >= 71 && code <= 77) || (code >= 85 && code <= 86)) return "snow";
  if (code >= 95) return "thunder";
  return "weather";
}

bool httpGetWeather(String& out, uint16_t timeoutMs = 2500) {
  if (WiFi.status() != WL_CONNECTED) return false;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  if (!http.begin(client, ZP_WEATHER_URL)) return false;
  int code = http.GET();
  out = (code > 0) ? http.getString() : "";
  http.end();
  return code >= 200 && code < 300;
}

void parseWeather(const String& payload) {
  StaticJsonDocument<2048> doc;
  if (deserializeJson(doc, payload) != DeserializationError::Ok) {
    eventLine = "weather json fail";
    return;
  }
  JsonObject cur = doc["current"];
  if (cur.isNull()) {
    eventLine = "weather empty";
    return;
  }
  wxTemp = cur["temperature_2m"] | wxTemp;
  wxFeels = cur["apparent_temperature"] | wxFeels;
  wxPrecip = cur["precipitation"] | wxPrecip;
  wxWind = cur["wind_speed_10m"] | wxWind;
  wxCode = cur["weather_code"] | wxCode;
  strlcpy(wxText, weatherCodeText(wxCode), sizeof(wxText));
  weatherReady = true;
  wxLastOkMs = millis();
  eventLine = String("Zaporizhzhia ") + String(wxTemp, 1) + "C " + wxText;
}

void updateWeather(bool force) {
  if (!force && weatherReady && millis() - lastWeatherAt < WEATHER_INTERVAL_MS) return;
  if (WiFi.status() != WL_CONNECTED) return;
  lastWeatherAt = millis();
  String body;
  if (httpGetWeather(body)) parseWeather(body);
  else eventLine = "weather HTTP fail";
}

void parseBuzzCurrent(const String& payload) {
  if (!payload.length()) return;

  StaticJsonDocument<1536> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (!err) {
    JsonObject root = doc.as<JsonObject>();
    JsonObject obj = root;
    if (root.containsKey("payload") && root["payload"].is<JsonObject>()) obj = root["payload"].as<JsonObject>();
    if (obj.containsKey("data") && obj["data"].is<JsonObject>()) obj = obj["data"].as<JsonObject>();

    const char* name =
      obj["filename"] |
      obj["title"] |
      obj["name"] |
      obj["track"] |
      obj["current"] |
      obj["song"] |
      obj["path"] |
      obj["url"] |
      nullptr;

    if (name && name[0]) {
      strlcpy(buzzTrack, name, sizeof(buzzTrack));
      buzzTrackLastMs = millis();
    }
    return;
  }

  int p = payload.indexOf("\"filename\"");
  if (p < 0) p = payload.indexOf("\"title\"");
  if (p >= 0) {
    int c = payload.indexOf(':', p);
    int q1 = payload.indexOf('"', c + 1);
    int q2 = payload.indexOf('"', q1 + 1);
    if (q1 > 0 && q2 > q1) {
      String s = payload.substring(q1 + 1, q2);
      s.replace("\\/", "/");
      s.toCharArray(buzzTrack, sizeof(buzzTrack));
      buzzTrackLastMs = millis();
    }
  }
}

void updateBuzzCurrent(bool force = false) {
  if (!force && millis() - buzzTrackLastMs < BUZZ_CURRENT_MS) return;
  String body;
  if (httpGetRaw(JANUS_MUSIC_CURRENT_URL, &body, 900)) {
    parseBuzzCurrent(body);
  }
}

void callMusicEndpoint(const char* label, const char* url) {
  String body;
  bool ok = httpGetRaw(url, &body, 900);
  eventLine = String(label) + (ok ? " HTTP OK" : " HTTP fail");
  if (ok) updateBuzzCurrent(true);
}

void sendBuzzControl(const char* cmd, int32_t value) {
  if (!espnowOk || !cmd || !cmd[0]) return;
  ensureColonyPeer();

  JanusControlPacket cp{};
  cp.magic[0] = 'J';
  cp.magic[1] = 'C';
  cp.version = 1;
  strlcpy(cp.source, "Core2Home", sizeof(cp.source));
  strlcpy(cp.target, "Buzz", sizeof(cp.target));
  strlcpy(cp.command, cmd, sizeof(cp.command));
  cp.value = value;
  cp.seq = ++controlSeq;
  cp.uptime_ms = millis();

  esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&cp, sizeof(cp));
  eventLine = String("Buzz cmd ") + cmd;
}

// ========================= AUDIO LIVE CONTROL / PLAYBACK =========================

bool janusAudioShouldListen() {
#if JANUS_AUDIO_LIVE_ENABLE
  return espnowOk && janusAudioLiveUserEnabled && page == PAGE_AUDIO;
#else
  return false;
#endif
}

int16_t janusULawToPcm(uint8_t uVal) {
  uVal = ~uVal;
  int t = ((uVal & 0x0F) << 3) + 0x84;
  t <<= ((unsigned)uVal & 0x70) >> 4;
  return (uVal & 0x80) ? (int16_t)(0x84 - t) : (int16_t)(t - 0x84);
}

static const int16_t janusAdpcmStepTable[89] = {
  7,8,9,10,11,12,13,14,16,17,19,21,23,25,28,31,34,37,41,45,
  50,55,60,66,73,80,88,97,107,118,130,143,157,173,190,209,230,253,279,307,
  337,371,408,449,494,544,598,658,724,796,876,963,1060,1166,1282,1411,1552,1707,1878,2066,
  2272,2499,2749,3024,3327,3660,4026,4428,4871,5358,5894,6484,7132,7845,8630,9493,10442,11487,
  12635,13899,15289,16818,18500,20350,22385,24623,27086,29794,32767
};
static const int8_t janusAdpcmIndexTable[16] = {
  -1,-1,-1,-1,2,4,6,8,-1,-1,-1,-1,2,4,6,8
};

int16_t janusAdpcmDecodeNibble(uint8_t code, int16_t& predictor, uint8_t& index) {
  int step = janusAdpcmStepTable[index];
  int diffq = step >> 3;
  if (code & 4) diffq += step;
  if (code & 2) diffq += step >> 1;
  if (code & 1) diffq += step >> 2;

  int pred = predictor;
  if (code & 8) pred -= diffq;
  else pred += diffq;
  if (pred > 32767) pred = 32767;
  if (pred < -32768) pred = -32768;
  predictor = (int16_t)pred;

  int idx = (int)index + janusAdpcmIndexTable[code & 0x0F];
  if (idx < 0) idx = 0;
  if (idx > 88) idx = 88;
  index = (uint8_t)idx;
  return predictor;
}

int16_t janusAudioBoostSample(int16_t s) {
  // v6.35: speech-first cleanup.
  // Earlier path was trying to be a radio monitor; for intelligibility we avoid hard gating,
  // use a slow DC tracker, soft limiting and a tiny one-pole smoother.
  static int32_t dc = 0;
  static int32_t smooth = 0;
  static int32_t lastOut = 0;

  int32_t x = (int32_t)s;
  dc += (x - dc) >> 8;                 // slow DC removal without eating consonants
  int32_t v = x - dc;
  v = (v * (int32_t)JANUS_AUDIO_RX_GAIN_Q8) >> 8;

  if (abs((int)v) < JANUS_AUDIO_NOISE_GATE) v = 0;

  if (v > JANUS_AUDIO_SOFT_LIMIT) v = JANUS_AUDIO_SOFT_LIMIT + ((v - JANUS_AUDIO_SOFT_LIMIT) >> 3);
  if (v < -JANUS_AUDIO_SOFT_LIMIT) v = -JANUS_AUDIO_SOFT_LIMIT + ((v + JANUS_AUDIO_SOFT_LIMIT) >> 3);
  if (v > 30000) v = 30000;
  if (v < -30000) v = -30000;

  // v6.37: lighter smoothing. v6.36 was safe, but could make speech too thick.
  // Keep edge grit down without turning vowels into a cave voice.
  smooth += (v - smooth) >> 1;
  int32_t out = smooth;
  const int32_t maxStep = 15000;
  int32_t d = out - lastOut;
  if (d > maxStep) out = lastOut + maxStep;
  else if (d < -maxStep) out = lastOut - maxStep;
  lastOut = out;
  return (int16_t)out;
}
bool janusAudioEnsureBuffers() {
  if (janusAudioPcmQ) return true;
  const size_t bytes = (size_t)JANUS_AUDIO_RX_QUEUE_N * (size_t)JANUS_AUDIO_FRAME_SAMPLES * sizeof(int16_t);
#if defined(MALLOC_CAP_SPIRAM)
  janusAudioPcmQ = (int16_t (*)[JANUS_AUDIO_FRAME_SAMPLES])heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
  if (!janusAudioPcmQ) {
    janusAudioPcmQ = (int16_t (*)[JANUS_AUDIO_FRAME_SAMPLES])heap_caps_malloc(bytes, MALLOC_CAP_8BIT);
  }
  if (!janusAudioPcmQ) {
    snprintf(janusAudioStatusLine, sizeof(janusAudioStatusLine), "AUDIO BUF FAIL");
    Serial.printf("[AUDIO] RX queue alloc FAIL bytes=%u heap=%u psram=%u\n",
                  (unsigned)bytes, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
    return false;
  }
  memset(janusAudioPcmQ, 0, bytes);
  Serial.printf("[AUDIO] RX queue alloc OK frames=%u samples=%u bytes=%u heap=%u psram=%u\n",
                (unsigned)JANUS_AUDIO_RX_QUEUE_N, (unsigned)JANUS_AUDIO_FRAME_SAMPLES,
                (unsigned)bytes, (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getFreePsram());
  return true;
}

void janusAudioQueueReset(bool keepStats) {
  (void)keepStats;
  janusAudioQHead = 0;
  janusAudioQTail = 0;
  janusAudioQCount = 0;
  janusAudioPlayingSlot = 255;
  janusAudioPlayActive = false;
  janusAudioWarmed = false;
  janusAudioPlayChunkLen = 0;
  janusAudioLastCodec = 0;
}

bool janusAudioSeqIsOldOrDuplicate(uint16_t seq) {
  if (!janusAudioSeqSeen) return false;
  int16_t diff = (int16_t)(seq - janusAudioLastSeq);
  return diff <= 0;
}

void janusAudioQueuePush(const int16_t* pcm, uint16_t samples, uint16_t rate) {
  if (!pcm || samples == 0) return;
  if (!janusAudioEnsureBuffers()) { janusAudioFramesDropped++; return; }
  samples = min(samples, (uint16_t)JANUS_AUDIO_FRAME_SAMPLES);
  if (janusAudioQCount >= JANUS_AUDIO_RX_QUEUE_N) {
    // v6.36: live audio should not build latency. Drop the oldest frame and keep newest speech.
    janusAudioFramesDropped++;
    janusAudioQueueOverruns++;
    janusAudioQTail = (uint8_t)((janusAudioQTail + 1) % JANUS_AUDIO_RX_QUEUE_N);
    if (janusAudioQCount > 0) janusAudioQCount--;
  }
  uint8_t slot = janusAudioQHead;
  memcpy(janusAudioPcmQ[slot], pcm, samples * sizeof(int16_t));
  janusAudioQLen[slot] = samples;
  janusAudioQRate[slot] = rate ? rate : JANUS_AUDIO_SAMPLE_RATE;
  janusAudioQHead = (uint8_t)((janusAudioQHead + 1) % JANUS_AUDIO_RX_QUEUE_N);
  janusAudioQCount++;
  janusAudioFramesQueued++;
}

void janusAudioSetVolumeDelta(int delta) {
  int v = (int)janusAudioPlayVolume + delta;
  if (v < JANUS_AUDIO_VOLUME_MIN) v = JANUS_AUDIO_VOLUME_MIN;
  if (v > JANUS_AUDIO_VOLUME_MAX) v = JANUS_AUDIO_VOLUME_MAX;
  janusAudioPlayVolume = (uint8_t)v;
  M5.Speaker.setVolume(janusAudioPlayVolume);
  prefs.putUChar("audVol", janusAudioPlayVolume);
  snprintf(janusAudioStatusLine, sizeof(janusAudioStatusLine), "AUDIO VOL %u", (unsigned)janusAudioPlayVolume);
  eventLine = String("AUDIO volume ") + String((unsigned)janusAudioPlayVolume);
}

void janusAudioCaptureChunkToSd(const int16_t* pcm, uint16_t samples, uint16_t rate) {
#if JANUS_AUDIO_SD_CAPTURE && CORE2_SD_ARCHIVE_ENABLE
  (void)rate;
  if (!core2SdArchiveOk || !janusAudioSdCaptureEnabled || !pcm || samples == 0) return;
  if (janusAudioSdCaptureBytes > JANUS_AUDIO_SD_CAPTURE_MAX) {
    SD.remove(JANUS_AUDIO_SD_CAPTURE_PATH);
    janusAudioSdCaptureBytes = 0;
  }
  File f = SD.open(JANUS_AUDIO_SD_CAPTURE_PATH, FILE_APPEND);
  if (!f) return;
  size_t bytes = (size_t)samples * sizeof(int16_t);
  size_t wr = f.write((const uint8_t*)pcm, bytes);
  f.close();
  janusAudioSdCaptureBytes += wr;
  janusAudioSdCaptureWrites++;
#endif
}

void janusAudioPlaybackTick() {
#if JANUS_AUDIO_LIVE_ENABLE
  if (!janusAudioEnsureBuffers()) return;
  if (!janusAudioShouldListen()) {
    janusAudioQueueReset(true);
    return;
  }

  bool playingNow = M5.Speaker.isPlaying(JANUS_AUDIO_PLAY_CHANNEL);
  if (janusAudioPlayActive && !playingNow) {
    janusAudioPlayingSlot = 255;
    janusAudioPlayActive = false;
    janusAudioPlayChunkLen = 0;
  }

  if (janusAudioPlayActive || M5.Speaker.isPlaying(JANUS_AUDIO_PLAY_CHANNEL)) return;

  if (!janusAudioWarmed) {
    if (janusAudioQCount < JANUS_AUDIO_START_BUFFER) return;
    janusAudioWarmed = true;
  }

  if (janusAudioQCount == 0) {
    janusAudioUnderruns++;
    janusAudioWarmed = false;
    return;
  }

  // v6.36: spool several small frames into one stable speech chunk.
  // This removes most boundary clicks and makes ESP-NOW jitter less audible.
  uint16_t outLen = 0;
  uint16_t outRate = janusAudioQRate[janusAudioQTail] ? janusAudioQRate[janusAudioQTail] : JANUS_AUDIO_SAMPLE_RATE;
  uint8_t frames = 0;
  while (janusAudioQCount > 0 && frames < JANUS_AUDIO_PLAY_CHUNK_FRAMES) {
    uint8_t slot = janusAudioQTail;
    uint16_t rate = janusAudioQRate[slot] ? janusAudioQRate[slot] : JANUS_AUDIO_SAMPLE_RATE;
    if (frames > 0 && rate != outRate) break;
    uint16_t n = janusAudioQLen[slot];
    if (n > JANUS_AUDIO_FRAME_SAMPLES) n = JANUS_AUDIO_FRAME_SAMPLES;
    if ((uint32_t)outLen + n > (uint32_t)JANUS_AUDIO_FRAME_SAMPLES * JANUS_AUDIO_PLAY_CHUNK_FRAMES) break;
    memcpy(&janusAudioPlayChunk[outLen], janusAudioPcmQ[slot], n * sizeof(int16_t));
    outLen += n;
    janusAudioQTail = (uint8_t)((janusAudioQTail + 1) % JANUS_AUDIO_RX_QUEUE_N);
    if (janusAudioQCount > 0) janusAudioQCount--;
    frames++;
  }

  if (outLen == 0) {
    janusAudioUnderruns++;
    janusAudioWarmed = false;
    return;
  }

  janusAudioPlayChunkLen = outLen;
  janusAudioPlayChunkRate = outRate;
  janusAudioCaptureChunkToSd(janusAudioPlayChunk, outLen, outRate);
  janusAudioPlayingSlot = 0;
  janusAudioPlayActive = true;
  janusAudioLastPlayMs = millis();
  M5.Speaker.setVolume(janusAudioPlayVolume);
  bool ok = M5.Speaker.playRaw(janusAudioPlayChunk, janusAudioPlayChunkLen, janusAudioPlayChunkRate, false, 1, JANUS_AUDIO_PLAY_CHANNEL, false);
  if (ok) {
    janusAudioFramesPlayed += frames;
    audioNode.v4 = (float)janusAudioFramesPlayed;
  } else {
    janusAudioFramesDropped++;
    janusAudioPlayActive = false;
    janusAudioPlayingSlot = 255;
    janusAudioPlayChunkLen = 0;
  }
#endif
}

void sendJanusAudioControl(bool enable, bool force) {
#if JANUS_AUDIO_LIVE_ENABLE
  if (!espnowOk) return;
  uint32_t now = millis();
  if (!force && janusAudioLiveSentState == enable && now - janusAudioLastControlMs < JANUS_AUDIO_CONTROL_REPEAT_MS) return;
  ensureColonyPeer();

  JanusAudioControlPacket ac{};
  ac.magic[0] = 'A';
  ac.magic[1] = 'C';
  ac.version = 1;
  ac.enable = enable ? 1 : 0;
  ac.codec = JANUS_AUDIO_CODEC_ACTIVE;
  ac.sampleRate = JANUS_AUDIO_SAMPLE_RATE;
  ac.frameMs = JANUS_AUDIO_FRAME_MS;
  ac.seq = ++janusAudioControlSeq;
  strlcpy(ac.source, "Core2Home", sizeof(ac.source));
  strlcpy(ac.target, "EchoMic", sizeof(ac.target));

  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&ac, sizeof(ac));
  janusAudioLastControlMs = now;
  if (err == ESP_OK) {
    janusAudioLiveSentState = enable;
    if (!enable) janusAudioQueueReset(true);
    snprintf(janusAudioStatusLine, sizeof(janusAudioStatusLine), enable ? ((JANUS_AUDIO_CODEC_ACTIVE == JANUS_AUDIO_CODEC_ULAW) ? "AUDIO REQUEST ULAW20" : "AUDIO REQUEST ADPCM") : "AUDIO REQUEST OFF");
    eventLine = enable ? ((JANUS_AUDIO_CODEC_ACTIVE == JANUS_AUDIO_CODEC_ULAW) ? "AUDIO u-law speech ON" : "AUDIO radio listen ON") : "AUDIO listen OFF";
  } else {
    snprintf(janusAudioStatusLine, sizeof(janusAudioStatusLine), "AUDIO CTRL FAIL %d", (int)err);
  }
#endif
}

void janusAudioLiveTick() {
#if JANUS_AUDIO_LIVE_ENABLE
  bool want = janusAudioShouldListen();
  uint32_t now = millis();

  if (want != janusAudioLiveSentState || (want && now - janusAudioLastControlMs >= JANUS_AUDIO_CONTROL_REPEAT_MS)) {
    sendJanusAudioControl(want, want != janusAudioLiveSentState);
  }

  janusAudioPlaybackTick();

  if (!want) {
    if (now - janusAudioLastFrameMs > JANUS_AUDIO_IDLE_TIMEOUT_MS) snprintf(janusAudioStatusLine, sizeof(janusAudioStatusLine), "AUDIO OFF");
    return;
  }

  if (janusAudioLastFrameMs == 0 || now - janusAudioLastFrameMs > JANUS_AUDIO_IDLE_TIMEOUT_MS) {
    snprintf(janusAudioStatusLine, sizeof(janusAudioStatusLine), "AUDIO RADIO WAIT q%u", (unsigned)janusAudioQCount);
    janusAudioWarmed = false;
  } else {
    snprintf(janusAudioStatusLine, sizeof(janusAudioStatusLine), "AUDIO %s %uHz q%u/%u V%u", (janusAudioLastCodec == JANUS_AUDIO_CODEC_ULAW ? "ULAW" : "ADPCM"), (unsigned)janusAudioLastRate, (unsigned)janusAudioQCount, (unsigned)JANUS_AUDIO_RX_QUEUE_N, (unsigned)janusAudioPlayVolume);
  }
#endif
}


bool janusEyeVisionShouldListen() {
#if JANUS_EYE_VISION_ENABLE
  return page == PAGE_EYE && janusEyeVisionUserEnabled && espnowOk;
#else
  return false;
#endif
}

void sendJanusEyeVisionControl(bool enable, bool force) {
#if JANUS_EYE_VISION_ENABLE
  if (!espnowOk) return;
  uint32_t now = millis();
  if (!force && janusEyeVisionSentState == enable && now - janusEyeVisionLastControlMs < JANUS_EYE_VISION_CONTROL_MS) return;
  ensureColonyPeer();

  JanusEyeVisionControlPacket ec{};
  ec.magic[0] = 'E';
  ec.magic[1] = 'C';
  ec.version = 1;
  ec.enable = enable ? 1 : 0;
  ec.mode = 1;
  ec.frameMs = JANUS_EYE_VISION_FRAME_MS;
  ec.seq = ++janusEyeVisionControlSeq;
  strlcpy(ec.source, "Core2Home", sizeof(ec.source));
  strlcpy(ec.target, "BlindEye", sizeof(ec.target));

  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&ec, sizeof(ec));
  janusEyeVisionLastControlMs = now;
  if (err == ESP_OK) {
    janusEyeVisionSentState = enable;
    snprintf(janusEyeVisionStatusLine, sizeof(janusEyeVisionStatusLine), enable ? "EYE SENSOR REQUEST" : "EYE SENSOR OFF");
    eventLine = enable ? "BlindEye sensor-field ON" : "BlindEye sensor-field OFF";
  } else {
    janusEyeVisionControlFail++;
    snprintf(janusEyeVisionStatusLine, sizeof(janusEyeVisionStatusLine), "EYE CTRL FAIL %d", (int)err);
  }
#endif
}

bool handleJanusEyeFrameRaw(const uint8_t* data, uint16_t len, int8_t rxRssi) {
#if JANUS_EYE_VISION_ENABLE
  if (!data || len != sizeof(JanusEyeFramePacket)) return false;
  JanusEyeFramePacket ef{};
  memcpy(&ef, data, sizeof(ef));
  if (ef.magic[0] != 'E' || ef.magic[1] != 'F' || ef.version != 1) return false;
  if (ef.width < 1 || ef.width > JANUS_EYE_VISION_W || ef.height < 1 || ef.height > JANUS_EYE_VISION_H) return false;

  if (janusEyeVisionSeqSeen && (uint16_t)(ef.seq - janusEyeVisionLastSeq) == 0) return true;
  janusEyeVisionSeqSeen = true;
  janusEyeVisionLastSeq = ef.seq;
  janusEyeVisionW = ef.width;
  janusEyeVisionH = ef.height;
  janusEyeVisionFlags = ef.flags;
  memcpy(janusEyeVisionPixels, ef.pixels, sizeof(janusEyeVisionPixels));
  janusEyeVisionFramesRx++;
  janusEyeVisionLastFrameMs = millis();
  janusEyeVisionLastRealFrameMs = janusEyeVisionLastFrameMs;
  janusEyeVisionLastRssi = rxRssi;
  eye.touch("BlindEye");
  eye.rssi = rxRssi;
  if (ef.flags & 0x02) eye.v1 = (eye.v1 > 100.0f) ? eye.v1 : 100.0f;
  if (ef.flags & 0x01) eye.v2 = (eye.v2 > 100.0f) ? eye.v2 : 100.0f;
  snprintf(janusEyeVisionStatusLine, sizeof(janusEyeVisionStatusLine), "EYE REAL FRAME %ux%u #%u", (unsigned)ef.width, (unsigned)ef.height, (unsigned)ef.seq);
  return true;
#else
  (void)data; (void)len; (void)rxRssi; return false;
#endif
}

void janusEyeVisionSynthesizeFromEye(bool force) {
#if JANUS_EYE_VISION_ENABLE
  uint32_t now = millis();
  if (!janusEyeVisionShouldListen()) return;
  if (!force && now - janusEyeVisionLastFrameMs < (uint32_t)JANUS_EYE_VISION_FRAME_MS) return;

  // If BlindEye later sends real multi-zone E/F frames, keep showing them and do not overwrite.
  if (janusEyeVisionLastRealFrameMs && now - janusEyeVisionLastRealFrameMs < JANUS_EYE_VISION_IDLE_MS) return;

  // v6.33: this is NOT a fake moving target and not a decorative GIF.
  // Existing BlindEye firmware sends real STHS34PF80/TMOS scalar telemetry in E2:
  //   v1=presence, v2=motion, v5=activity, v6=predicted activity, loss=prediction error.
  // A single-zone TMOS sensor has no direction pixels, so Core2 renders its real field-of-view
  // as a monochrome aperture/occupancy image: intensity = actual presence/motion confidence.
  bool fresh = (janusEyeVisionLastTelemetryMs != 0 && now - janusEyeVisionLastTelemetryMs < JANUS_EYE_VISION_IDLE_MS);
  if (!eye.refresh() || !fresh) {
    for (uint8_t i = 0; i < JANUS_EYE_FRAME_PIXELS; ++i) {
      uint8_t old = janusEyeVisionPixels[i];
      janusEyeVisionPixels[i] = (old > 3) ? (uint8_t)(old - 3) : 0;
    }
    janusEyeVisionFlags = 0;
    janusEyeVisionW = JANUS_EYE_VISION_W;
    janusEyeVisionH = JANUS_EYE_VISION_H;
    janusEyeVisionLastFrameMs = now;
    snprintf(janusEyeVisionStatusLine, sizeof(janusEyeVisionStatusLine), "EYE WAIT SENSOR  age:%lus", (unsigned long)(eye.age() / 1000UL));
    return;
  }

  float presence = clipf(eye.v1 / 100.0f, 0.0f, 1.35f);
  float motion = clipf(eye.v2 / 100.0f, 0.0f, 1.35f);
  float activity = clipf(eye.v5 / 100.0f, 0.0f, 1.35f);
  float predict = clipf(eye.v6 / 100.0f, 0.0f, 1.35f);
  float err = clipf(fabsf(eye.loss), 0.0f, 1.0f);
  float signal = clipf(0.56f * presence + 0.28f * motion + 0.18f * activity + 0.10f * predict, 0.0f, 1.75f);
  float motionBias = clipf(motion + activity * 0.45f, 0.0f, 1.40f);

  for (int y = 0; y < JANUS_EYE_VISION_H; ++y) {
    for (int x = 0; x < JANUS_EYE_VISION_W; ++x) {
      float fx = ((float)x - 3.5f) / 3.5f;
      float fy = ((float)y - 3.5f) / 3.5f;
      float r2 = fx * fx + fy * fy * 0.82f;
      float aperture = clipf(1.0f - r2, 0.0f, 1.0f);
      float center = clipf(1.0f - fabsf(fx) * 0.72f, 0.0f, 1.0f);
      float depth = clipf(((float)y + 1.0f) / (float)JANUS_EYE_VISION_H, 0.0f, 1.0f);

      // Presence feels like a soft body inside the lens. Motion opens the outer rings.
      float field = 0.04f;
      field += signal * (0.66f * aperture + 0.20f * center + 0.14f * depth);
      field += motionBias * 0.20f * clipf(1.0f - fabsf(r2 - 0.42f) * 1.9f, 0.0f, 1.0f);
      field -= err * 0.05f;
      field = clipf(field, 0.0f, 1.0f);

      uint8_t target = (uint8_t)(field * 255.0f + 0.5f);
      uint8_t old = janusEyeVisionPixels[y * JANUS_EYE_VISION_W + x];
      // Fast rise, slow fall: this is sensor persistence, not fake motion.
      uint8_t out = (target > old) ? (uint8_t)((old + target * 3U) / 4U) : (uint8_t)((old * 5U + target) / 6U);
      janusEyeVisionPixels[y * JANUS_EYE_VISION_W + x] = out;
    }
  }

  janusEyeVisionW = JANUS_EYE_VISION_W;
  janusEyeVisionH = JANUS_EYE_VISION_H;
  janusEyeVisionFlags = (presence > 0.10f ? 0x02 : 0) | (motion > 0.10f ? 0x01 : 0);
  janusEyeVisionSynthFrames++;   // kept as a counter name; now means E2 field frames, not fake GIF.
  janusEyeVisionLastFrameMs = now;
  snprintf(janusEyeVisionStatusLine, sizeof(janusEyeVisionStatusLine), "EYE REAL FIELD P%.0f M%.0f A%.0f", eye.v1, eye.v2, eye.v5);
#else
  (void)force;
#endif
}

void janusEyeVisionTick() {
#if JANUS_EYE_VISION_ENABLE
  bool want = janusEyeVisionShouldListen();
  uint32_t now = millis();
  if (want != janusEyeVisionSentState || (want && now - janusEyeVisionLastControlMs >= JANUS_EYE_VISION_CONTROL_MS)) {
    sendJanusEyeVisionControl(want, want != janusEyeVisionSentState);
  }
  if (!want) {
    if (now - janusEyeVisionLastFrameMs > JANUS_EYE_VISION_IDLE_MS) snprintf(janusEyeVisionStatusLine, sizeof(janusEyeVisionStatusLine), "EYE SENSOR OFF");
    return;
  }
  janusEyeVisionSynthesizeFromEye(false);
#endif
}

void handleJanusAudioFrameRaw(const uint8_t* data, uint16_t len, int8_t rxRssi) {
#if JANUS_AUDIO_LIVE_ENABLE
  const uint16_t headerLen = sizeof(JanusAudioFramePacket) - JANUS_AUDIO_FRAME_MAX_BYTES;
  if (!data || len < headerLen || data[0] != 'A' || data[1] != 'F') return;

  JanusAudioFramePacket af{};
  uint16_t copyLen = min((uint16_t)sizeof(af), len);
  memcpy(&af, data, copyLen);

  if (af.codec != JANUS_AUDIO_CODEC_ULAW && af.codec != JANUS_AUDIO_CODEC_ADPCM4) { janusAudioFramesDropped++; return; }
  if (af.codec == JANUS_AUDIO_CODEC_ADPCM4 && af.version != 2) { janusAudioFramesDropped++; return; }
  if (af.codec == JANUS_AUDIO_CODEC_ULAW && af.version != 1) { janusAudioFramesDropped++; return; }
  if (af.samples == 0 || af.samples > JANUS_AUDIO_FRAME_SAMPLES) { janusAudioFramesDropped++; return; }

  uint16_t payloadBytes = (af.codec == JANUS_AUDIO_CODEC_ADPCM4) ? (uint16_t)((af.samples + 1) / 2) : af.samples;
  if (payloadBytes > JANUS_AUDIO_FRAME_MAX_BYTES || len < headerLen + payloadBytes) { janusAudioFramesDropped++; return; }

  if (janusAudioSeqIsOldOrDuplicate(af.seq)) {
    janusAudioDuplicateDrops++;
    janusAudioFramesDropped++;
    return;
  }
  if (janusAudioSeqSeen) {
    uint16_t expected = janusAudioLastSeq + 1;
    if (af.seq != expected) janusAudioSeqGaps += (uint16_t)(af.seq - expected);
  }
  janusAudioSeqSeen = true;
  janusAudioLastSeq = af.seq;

  if (janusAudioLastCodec != 0 && janusAudioLastCodec != af.codec) {
    janusAudioQueueReset(true);
    janusAudioSeqSeen = false;
  }
  janusAudioLastCodec = af.codec;

  janusAudioFramesRx++;
  janusAudioLastFrameMs = millis();
  janusAudioLastRssi = rxRssi;
  janusAudioLastRate = af.sampleRate ? af.sampleRate : JANUS_AUDIO_SAMPLE_RATE;
  janusAudioLastSamples = af.samples;

  audioNode.touch("EchoMicLive");
  strlcpy(audioNode.role, "AudioRadio", sizeof(audioNode.role));
  audioNode.rssi = rxRssi;
  audioNode.v0 = (float)af.samples;
  audioNode.v3 = (float)janusAudioFramesRx;
  audioNode.v4 = (float)janusAudioFramesPlayed;
  audioNode.v5 = (float)janusAudioFramesDropped;
  audioNode.v6 = (float)janusAudioQCount;
  audioNode.v7 = (float)janusAudioSeqGaps;
  audioNode.entropy = clipf(audioNode.entropy * 0.92f + 0.08f * ((float)payloadBytes / (float)JANUS_AUDIO_FRAME_MAX_BYTES), 0.0f, 10.0f);

  if (!janusAudioShouldListen()) {
    janusAudioFramesDropped++;
    return;
  }

  int16_t decoded[JANUS_AUDIO_FRAME_SAMPLES];
  if (af.codec == JANUS_AUDIO_CODEC_ADPCM4) {
    int16_t pred = af.predictor;
    uint8_t idx = (af.stepIndex <= 88) ? af.stepIndex : 0;
    for (uint16_t i = 0; i < af.samples; i++) {
      uint8_t packed = af.data[i >> 1];
      uint8_t nib = (i & 1) ? (packed >> 4) : (packed & 0x0F);
      decoded[i] = janusAudioBoostSample(janusAdpcmDecodeNibble(nib, pred, idx));
    }
  } else {
    for (uint16_t i = 0; i < af.samples; i++) decoded[i] = janusAudioBoostSample(janusULawToPcm(af.data[i]));
  }

  janusAudioQueuePush(decoded, af.samples, janusAudioLastRate);
#endif
}

// ========================= PACKET PROCESS =========================

void handleHeartbeat(const JanusColonyPacket& pkt, int8_t rxRssi) {
  String id = String(pkt.nodeId);
  if (!id.length()) id = String(pkt.role);

  int uidx = universalRememberNode(pkt.nodeId, pkt.role, rxRssi ? rxRssi : pkt.rssi);
  if (uidx >= 0) {
    colonyNodes[uidx].hashRate = pkt.hashRate;
    colonyNodes[uidx].shares = pkt.shares;
    colonyNodes[uidx].rejects = pkt.rejects;
    colonyNodes[uidx].bestBits = pkt.bestBits;
    colonyNodes[uidx].diff = pkt.diff;
    colonyNodes[uidx].targetBits = pkt.targetBits;
    colonyNodes[uidx].aiBatch = pkt.aiBatch;
    colonyNodes[uidx].aiHint = pkt.aiHint;
    colonyNodes[uidx].jobAgeMs = pkt.jobAgeMs;
    colonyNodes[uidx].uptime = pkt.uptime;
    universalRecountNodes();
    universalMirrorToFixedSlot(uidx);
  }

  heartbeatPackets++;

  String roleStr = String(pkt.role);
  if (roleStr.indexOf("BuzzLighter") >= 0 || id.indexOf("Buzz") >= 0) eventLine = "Buzz master seen";
  else if (roleStr.indexOf("Stick") >= 0 || id.indexOf("Stick3") >= 0 || id.indexOf("StickS3") >= 0) eventLine = "Stick3 worker seen";
  else if (roleStr.indexOf("ATOM") >= 0 || roleStr.indexOf("Tron") >= 0 || id.indexOf("ATOM") >= 0) eventLine = "ATOM/TRON worker seen";
}

void handleEntropyV1(const EntropyReport& er, int8_t rxRssi) {
  int uidx = universalRememberNode("LegacyER", "ER", rxRssi);
  if (uidx >= 0) {
    colonyNodes[uidx].worker = er.worker_id;
    colonyNodes[uidx].entropy = er.local_entropy;
    colonyNodes[uidx].v[0] = er.values[0];
    colonyNodes[uidx].v[1] = er.values[1];
    colonyNodes[uidx].v[2] = er.values[2];
    colonyNodes[uidx].loss = er.values[3];
    universalMirrorToFixedSlot(uidx);
  }
}

void handleEntropyV2(const EntropyReportV2& er2, int8_t rxRssi) {
  String id = String(er2.nodeId);

  int uidx = universalRememberNode(er2.nodeId, "", rxRssi);
  if (uidx >= 0) {
    colonyNodes[uidx].worker = er2.worker_id;
    colonyNodes[uidx].entropy = er2.local_entropy;
    colonyNodes[uidx].loss = er2.prediction_error;
    colonyNodes[uidx].sync = er2.sync_hint;
    colonyNodes[uidx].fit = er2.fit;
    for (int i = 0; i < 8; i++) colonyNodes[uidx].v[i] = er2.values[i];

    // Some workers put mining stats into values[5]/[6]/[7].
    if (er2.values[5] > 0.0f) colonyNodes[uidx].hashRate = (uint32_t)max(0.0f, er2.values[5]);
    if (er2.values[6] > 0.0f) colonyNodes[uidx].bestBits = (uint32_t)max(0.0f, er2.values[6]);

    universalRecountNodes();
    universalMirrorToFixedSlot(uidx);
  }

  er2Packets++;

  if (id.indexOf("BlindEye") >= 0) {
    janusEyeVisionLastTelemetryMs = millis();
    // v6.33: while not on the BlindEYE page we collect only metrics.
    // No background vision rendering and no fake animation load.
    if (page == PAGE_EYE && janusEyeVisionUserEnabled) janusEyeVisionSynthesizeFromEye(true);
    eventLine = String("Eye TM ") + String(er2.values[1], 0) + "/" + String(er2.values[2], 0);
  } else if (id.indexOf("Beacon") >= 0 || id.indexOf("ADV") >= 0) {
    eventLine = "Beacon telemetry";
  } else if (id.indexOf("EchoMic") >= 0 || id.indexOf("Audio") >= 0 || id.indexOf("Swarm") >= 0 || id.indexOf("TD") >= 0 || id.indexOf("ATOM") >= 0 || id.indexOf("Tron") >= 0) {
    eventLine = String("Swarm mic ") + String(er2.values[0], 0);
  } else if (id.indexOf("Stick3") >= 0 || id.indexOf("StickS3") >= 0 || id.indexOf("Stick") >= 0 || id.indexOf("AlienRogue") >= 0) {
    eventLine = "Stick3 telemetry";
  } else if (uidx >= 0) {
    eventLine = String("Node telemetry ") + (colonyNodes[uidx].nodeId[0] ? colonyNodes[uidx].nodeId : "future");
  }
}



// ========================= KENSHI / TACHYON PROPHECY BUS =========================

bool core2IsSelfNode(const char* nodeId) {
  if (!nodeId || !nodeId[0]) return false;
  return strncmp(nodeId, "Core2Home", 9) == 0 || strncmp(nodeId, "Core2Galaxy", 11) == 0 || strncmp(nodeId, DEVICE_ID, strlen(DEVICE_ID)) == 0;
}

uint8_t core2FindRemoteProphecySlot(const char* nodeId) {
  if (!nodeId || !nodeId[0]) nodeId = "node";
  for (uint8_t i = 0; i < CORE2_REMOTE_PROPHECY_SLOTS; ++i) {
    if (core2RemoteProphecies[i].active &&
        strncmp(core2RemoteProphecies[i].nodeId, nodeId, sizeof(core2RemoteProphecies[i].nodeId)) == 0) {
      return i;
    }
  }

  uint8_t freeSlot = 255;
  uint8_t oldestSlot = 0;
  uint32_t oldest = 0xFFFFFFFFUL;
  for (uint8_t i = 0; i < CORE2_REMOTE_PROPHECY_SLOTS; ++i) {
    if (!core2RemoteProphecies[i].active && freeSlot == 255) freeSlot = i;
    if (core2RemoteProphecies[i].lastSeenMs < oldest) {
      oldest = core2RemoteProphecies[i].lastSeenMs;
      oldestSlot = i;
    }
  }
  return freeSlot != 255 ? freeSlot : oldestSlot;
}

void core2RememberProphecyInRegistry(const char* id, const char* role, int8_t rxRssi, const uint8_t* mac,
                                     uint16_t worker, float entropy, float loss, float sync, float fit,
                                     float v0, float v1, float v2, float v3, float v4, float v5, float v6, float v7) {
  int uidx = universalRememberNodeEx(id && id[0] ? id : "TPNode", role && role[0] ? role : "Tachyon", rxRssi, mac);
  if (uidx < 0) return;
  colonyNodes[uidx].worker = worker;
  colonyNodes[uidx].entropy = isfinite(entropy) ? entropy : 0.0f;
  colonyNodes[uidx].loss = isfinite(loss) ? loss : 0.0f;
  colonyNodes[uidx].sync = isfinite(sync) ? sync : 0.0f;
  colonyNodes[uidx].fit = isfinite(fit) ? fit : 0.0f;
  colonyNodes[uidx].v[0] = isfinite(v0) ? v0 : 0.0f;
  colonyNodes[uidx].v[1] = isfinite(v1) ? v1 : 0.0f;
  colonyNodes[uidx].v[2] = isfinite(v2) ? v2 : 0.0f;
  colonyNodes[uidx].v[3] = isfinite(v3) ? v3 : 0.0f;
  colonyNodes[uidx].v[4] = isfinite(v4) ? v4 : 0.0f;
  colonyNodes[uidx].v[5] = isfinite(v5) ? v5 : 0.0f;
  colonyNodes[uidx].v[6] = isfinite(v6) ? v6 : 0.0f;
  colonyNodes[uidx].v[7] = isfinite(v7) ? v7 : 0.0f;
  universalRecountNodes();
  universalMirrorToFixedSlot(uidx);
}

void core2FeedUniverseFromPrediction(const char* nodeId, uint8_t sector, uint8_t predictedSector, float stress, float confidence, float presence, float motion) {
  uint8_t s0 = sector % JanusGalaxyStationSim::UNIVERSE_SECTORS;
  uint8_t s1 = predictedSector % JanusGalaxyStationSim::UNIVERSE_SECTORS;
  float c = clipf(confidence, 0.0f, 1.2f);
  float threatPush = clipf(stress * 0.010f + motion * 0.0009f, 0.0f, 0.035f);
  float supplyPush = clipf(presence * 0.00007f + c * 0.006f, 0.0f, 0.025f);

  galaxy.universeThreat[s1] = clipf(galaxy.universeThreat[s1] * 0.992f + threatPush, 0.0f, 1.5f);
  galaxy.universeSupply[s0] = clipf(galaxy.universeSupply[s0] + supplyPush, 0.0f, 1.5f);
  galaxy.universeInfluence[s0] = clipf(galaxy.universeInfluence[s0] + c * 0.0025f, 0.0f, 1.5f);
  galaxy.universeSelectedSector = s1;

  if (nodeId && (strstr(nodeId, "BlindEye") || strstr(nodeId, "Eye"))) {
    snprintf(galaxy.universePilotLine, sizeof(galaxy.universePilotLine),
             "Eye prophecy: S%02u->S%02u conf%u stress%.2f", (unsigned)s0, (unsigned)s1, (unsigned)(c * 100.0f), stress);
  }
}

void handleJanusTachyonProphecyRaw(const uint8_t* data, uint16_t len, int8_t rxRssi, const uint8_t* mac) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  if (!data || len != sizeof(JanusTachyonProphecyPacket)) return;
  JanusTachyonProphecyPacket tp{};
  memcpy(&tp, data, sizeof(tp));
  if (tp.magic[0] != 'T' || tp.magic[1] != 'P' || tp.version != 1) return;
  if (core2IsSelfNode(tp.nodeId)) return;

  uint8_t slot = core2FindRemoteProphecySlot(tp.nodeId);
  Core2RemoteProphecyState& r = core2RemoteProphecies[slot];
  r.active = true;
  strlcpy(r.nodeId, tp.nodeId[0] ? tp.nodeId : "TPNode", sizeof(r.nodeId));
  r.lastSeenMs = millis();
  r.seq = tp.seq;
  r.sector = tp.sector & 0x0F;
  r.predictedSector = tp.predictedSector & 0x0F;
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
  r.event_eta_ms = isfinite(tp.event_eta_ms) ? tp.event_eta_ms : 9999.0f;
  r.future_stress = isfinite(tp.future_stress) ? tp.future_stress : 0.0f;
  r.swarm_pressure = isfinite(tp.swarm_pressure) ? tp.swarm_pressure : 0.0f;
  r.rssi = rxRssi;

  core2TachyonRx++;
  float sync = clipf((float)tp.confidence / 100.0f, 0.0f, 1.0f);
  float entropy = clipf(tp.presence_now * 0.0015f + tp.motion_now * 0.16f + tp.future_stress * 0.8f, 0.0f, 20.0f);
  core2RemotePressure = core2RemotePressure * 0.86f + clipf(tp.future_stress + tp.swarm_pressure, 0.0f, 5.0f) * 0.14f;

  core2RememberProphecyInRegistry(r.nodeId, "TachyonTP", rxRssi, mac, tp.worker_id,
                                  entropy, tp.future_stress, sync, sync,
                                  tp.presence_now, tp.motion_now, tp.pred_presence_1, tp.pred_motion_1,
                                  tp.event_eta_ms, tp.future_stress, (float)tp.sector, (float)tp.predictedSector);
  core2FeedUniverseFromPrediction(r.nodeId, tp.sector, tp.predictedSector, tp.future_stress, sync, tp.presence_now, tp.motion_now);

  String id = String(r.nodeId);
  if (id.indexOf("BlindEye") >= 0 || id.indexOf("Eye") >= 0) {
    janusEyeVisionLastTelemetryMs = millis();
    snprintf(janusEyeVisionStatusLine, sizeof(janusEyeVisionStatusLine), "EYE TP RX #%lu C%u S%u>%u", (unsigned long)core2TachyonRx, (unsigned)tp.confidence, (unsigned)tp.sector, (unsigned)tp.predictedSector);
  }
  snprintf(core2TachyonLine, sizeof(core2TachyonLine), "TP RX %s C%u S%u>%u rx/tx %lu/%lu",
           r.nodeId, (unsigned)tp.confidence, (unsigned)tp.sector, (unsigned)tp.predictedSector,
           (unsigned long)core2TachyonRx, (unsigned long)core2TachyonTx);
#else
  (void)data; (void)len; (void)rxRssi; (void)mac;
#endif
}

void handleJanusKenshiRaw(const uint8_t* data, uint16_t len, int8_t rxRssi, const uint8_t* mac) {
  if (!data || len != sizeof(JanusKenshiPacket)) return;
  JanusKenshiPacket kp{};
  memcpy(&kp, data, sizeof(kp));
  if (kp.magic[0] != 'K' || kp.magic[1] != '2' || kp.version != 1) return;
  if (core2IsSelfNode(kp.nodeId)) return;
  char kpNode[24];
  strlcpy(kpNode, kp.nodeId[0] ? kp.nodeId : "Kenshi", sizeof(kpNode));

  core2KenshiRx++;
  float sync = clipf(kp.confidence, 0.0f, 1.0f);
  core2RemotePressure = core2RemotePressure * 0.90f + clipf(kp.entropy * 0.03f + kp.values[4] * 0.25f, 0.0f, 4.0f) * 0.10f;

  core2RememberProphecyInRegistry(kpNode, "KenshiV2", rxRssi ? rxRssi : kp.rssi, mac, kp.worker_id,
                                  kp.entropy, kp.values[4], sync, kp.values[5],
                                  kp.values[0], kp.values[1], kp.values[2], kp.values[3],
                                  (float)kp.priority, (float)kp.worldFlags, (float)kp.sector, (float)kp.predictedSector);
  core2FeedUniverseFromPrediction(kpNode, kp.sector, kp.predictedSector, kp.values[4], sync, kp.values[0], kp.values[1]);

  if (kp.flags & 0x02) eventLine = String("Kenshi alert ") + kpNode;
  snprintf(core2TachyonLine, sizeof(core2TachyonLine), "K2 RX %s B%u/%u rx/tx %lu/%lu",
           kpNode, (unsigned)kp.activeBubbleNodes, (unsigned)kp.virtualNodes,
           (unsigned long)core2KenshiRx, (unsigned long)core2KenshiTx);
}

void updateCore2TachyonPrediction() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  uint32_t now = millis();
  float remoteP = 0.0f;
  float remoteM = 0.0f;
  float remoteStress = 0.0f;
  float wsum = 0.0f;

  for (uint8_t i = 0; i < CORE2_REMOTE_PROPHECY_SLOTS; ++i) {
    Core2RemoteProphecyState& r = core2RemoteProphecies[i];
    if (!r.active) continue;
    uint32_t age = now - r.lastSeenMs;
    if (age > 24000UL) continue;
    float ageW = clipf(1.0f - (float)age / 24000.0f, 0.05f, 1.0f);
    float confW = clipf((float)r.confidence / 100.0f, 0.08f, 1.0f);
    float w = ageW * confW;
    remoteP += r.pred_presence_1 * w;
    remoteM += r.pred_motion_1 * w;
    remoteStress += r.future_stress * w;
    wsum += w;
  }
  if (wsum > 0.001f) {
    remoteP /= wsum;
    remoteM /= wsum;
    remoteStress /= wsum;
  }

  bool eyeOn = eye.refresh();
  bool audOn = audioNode.refresh();
  float eyePresence = eyeOn ? max(eye.v1, eye.entropy * 85.0f) : 0.0f;
  float eyeMotion = eyeOn ? max(eye.v2, eye.v5 * 18.0f) : 0.0f;
  float audioPresence = audOn ? max(audioNode.v0, audioNode.entropy * 130.0f) : 0.0f;

  float presenceNow = clipf(max(eyePresence, audioPresence) + spacePresence * 180.0f + (float)eco2 * 0.035f + (float)tvoc * 0.075f, 0.0f, 9000.0f);
  float motionNow = clipf(max(eyeMotion, spaceMotion * 160.0f) + spaceNovelty * 70.0f + coreTheta.resonance * 30.0f + (float)coreBestBits * 1.4f, 0.0f, 3000.0f);

  float trendP = clipf(presenceNow - core2TachyonLastPresence, -2200.0f, 2200.0f);
  float trendM = clipf(motionNow - core2TachyonLastMotion, -900.0f, 900.0f);
  core2TachyonLastPresence = presenceNow;
  core2TachyonLastMotion = motionNow;

  core2PredPresence1 = max(0.0f, presenceNow * 0.60f + (presenceNow + trendP * 1.15f) * 0.22f + remoteP * 0.18f);
  core2PredMotion1 = max(0.0f, motionNow * 0.58f + (motionNow + trendM * 1.10f) * 0.24f + remoteM * 0.18f);
  core2PredPresence2 = max(0.0f, core2PredPresence1 * 0.72f + (presenceNow + trendP * 1.70f) * 0.14f + remoteP * 0.14f);
  core2PredMotion2 = max(0.0f, core2PredMotion1 * 0.72f + (motionNow + trendM * 1.55f) * 0.16f + remoteM * 0.12f);
  core2PredPresence3 = max(0.0f, core2PredPresence2 * 0.76f + homeEntropy() * 42.0f + remoteP * 0.10f);
  core2PredMotion3 = max(0.0f, core2PredMotion2 * 0.76f + spaceRisk * 22.0f + remoteM * 0.10f);

  core2FutureStress = clipf(spaceRisk * 0.80f + spaceNovelty * 0.42f + airEntropy * 0.28f + coreTheta.mock * 0.22f + remoteStress * 0.34f + core2RemotePressure * 0.18f, 0.0f, 3.0f);
  core2TachyonConfidence = clipf(0.34f + homeSync() * 0.34f + spaceConfidence * 0.18f + (wsum > 0.01f ? 0.14f : 0.0f), 0.0f, 1.0f);

  core2TachyonSector = (uint8_t)((uint32_t)(homeEntropy() * 2.0f + spaceMotion * 4.0f + coreTheta.resonance * 5.0f + (float)coreBestBits + (float)(now / 9000UL)) & 0x0F);
  core2TachyonPredictedSector = (uint8_t)((core2TachyonSector + (core2PredMotion1 > motionNow + 45.0f ? 1 : 0) + (core2FutureStress > 1.12f ? 2 : 0)) & 0x0F);
  core2TachyonJobState = core2FutureStress > 1.12f ? 3 : (page == PAGE_EYE ? 2 : (core2TachyonConfidence > 0.72f ? 4 : 1));

  snprintf(core2TachyonLine, sizeof(core2TachyonLine), "TP Core P%.0f/%.0f M%.0f/%.0f C%u rx%lu",
           presenceNow, core2PredPresence1, motionNow, core2PredMotion1,
           (unsigned)(core2TachyonConfidence * 100.0f), (unsigned long)core2TachyonRx);
#endif
}

void sendCore2TachyonProphecyPacket(bool force) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  if (!espnowOk) return;
  uint32_t now = millis();
  if (!force && now - core2LastTachyonTxMs < JANUS_TACHYON_PROPHECY_TX_MS) return;
  ensureColonyPeer();

  JanusTachyonProphecyPacket tp{};
  tp.magic[0] = 'T'; tp.magic[1] = 'P';
  tp.version = 1;
  tp.flags = 0;
  if (core2TachyonLastPresence > 260.0f || spacePresence > 0.25f) tp.flags |= 0x01;
  if (core2TachyonLastMotion > 60.0f || spaceMotion > 0.20f) tp.flags |= 0x02;
  if (core2FutureStress > 1.12f || spaceRisk > 0.65f) tp.flags |= 0x04;
  if (core2TachyonRx > 0) tp.flags |= 0x08;
  strlcpy(tp.nodeId, "Core2Home", sizeof(tp.nodeId));
  tp.seq = ++core2TachyonSeq;
  tp.worker_id = colonyWorkerId;
  tp.uptime_ms = now;
  tp.horizon_ms = CORE2_TACHYON_HORIZON_MS;
  tp.sector = core2TachyonSector;
  tp.predictedSector = core2TachyonPredictedSector;
  tp.confidence = (uint8_t)clipf(core2TachyonConfidence * 100.0f, 0.0f, 100.0f);
  tp.jobState = core2TachyonJobState;
  tp.presence_now = core2TachyonLastPresence;
  tp.motion_now = core2TachyonLastMotion;
  tp.pred_presence_1 = core2PredPresence1;
  tp.pred_motion_1 = core2PredMotion1;
  tp.pred_presence_2 = core2PredPresence2;
  tp.pred_motion_2 = core2PredMotion2;
  tp.pred_presence_3 = core2PredPresence3;
  tp.pred_motion_3 = core2PredMotion3;
  tp.event_eta_ms = (core2FutureStress > 1.12f) ? 420.0f : 9999.0f;
  tp.future_stress = core2FutureStress;
  tp.swarm_pressure = core2RemotePressure;

  if (esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&tp, sizeof(tp)) == ESP_OK) {
    core2TachyonTx++;
    core2LastTachyonTxMs = now;
  }
#endif
}

void sendCore2KenshiPacket(bool force) {
  if (!espnowOk) return;
  uint32_t now = millis();
  if (!force && now - core2LastKenshiTxMs < JANUS_KENSHI_BUBBLE_TX_MS) return;
  ensureColonyPeer();

  JanusKenshiPacket kp{};
  kp.magic[0] = 'K'; kp.magic[1] = '2';
  kp.version = 1;
  kp.flags = 0x04; // Core2 is the strategic/virtual world bubble.
  if (page == PAGE_EYE || page == PAGE_SPACE || core2TachyonLastPresence > 260.0f) kp.flags |= 0x01;
  if (core2FutureStress > 1.12f || spaceRisk > 0.65f) kp.flags |= 0x02;
  kp.flags |= 0x08; // motion-base / future Eye compatible
  strlcpy(kp.nodeId, "Core2Home", sizeof(kp.nodeId));
  kp.seq = ++core2TachyonSeq;
  kp.worker_id = colonyWorkerId;
  kp.uptime_ms = now;
  kp.activeBubbleNodes = (uint8_t)clipf((float)colonyOnlineCount + (page == PAGE_EYE ? 1.0f : 0.0f), 0.0f, 255.0f);
  kp.virtualNodes = CORE2_MAX_COLONY_NODES;
  kp.worldFlags = ((uint32_t)kp.flags) | ((uint32_t)page << 8) | ((uint32_t)coreBestBits << 16) | ((uint32_t)slime.emotion << 24);
  kp.sector = core2TachyonSector;
  kp.predictedSector = core2TachyonPredictedSector;
  kp.jobState = core2TachyonJobState;
  kp.priority = (uint8_t)clipf(core2FutureStress * 55.0f + core2TachyonConfidence * 80.0f + spaceRisk * 50.0f, 0.0f, 255.0f);
  kp.rssi = wifiOk ? WiFi.RSSI() : -127;
  kp.entropy = homeEntropy();
  kp.activity = core2TachyonLastMotion * 0.01f + spaceMotion;
  kp.confidence = core2TachyonConfidence;
  kp.values[0] = core2TachyonLastPresence;
  kp.values[1] = core2TachyonLastMotion;
  kp.values[2] = (float)eco2;
  kp.values[3] = spaceRisk;
  kp.values[4] = core2FutureStress;
  kp.values[5] = spaceConfidence;

  if (esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&kp, sizeof(kp)) == ESP_OK) {
    core2KenshiTx++;
    core2LastKenshiTxMs = now;
  }
}

void core2TachyonProphecyTick() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  updateCore2TachyonPrediction();
  sendCore2TachyonProphecyPacket(false);
  sendCore2KenshiPacket(false);
#endif
}


uint8_t core2SwarmSenseLabel(uint8_t thermal, uint16_t jitter, int8_t rssi) {
  if (thermal > 78 || jitter > 18000) return 2;       // HOT / unstable load
  if (rssi < -82) return 4;                           // SHADOW / weak RF
  if (core2SwarmSenseTx < 8) return 8;                // LEARNING
  return 1;                                           // GOOD
}

void handleSwarmSenseRaw(const uint8_t* data, uint16_t len, int8_t rxRssi, const uint8_t* mac) {
#if CORE2_SWARMSENSE_OBSERVE
  if (!data || len != sizeof(SwarmSensePacket)) { core2SwarmSenseBad++; return; }
  SwarmSensePacket ss{};
  memcpy(&ss, data, sizeof(ss));
  if (ss.magic[0] != 'S' || ss.magic[1] != 'S' || ss.version != 1 || !ss.nodeId[0]) {
    core2SwarmSenseBad++;
    return;
  }

  core2SwarmSenseRx++;
  int uidx = universalRememberNodeEx(ss.nodeId, ss.kind[0] ? ss.kind : "SwarmSense", rxRssi ? rxRssi : ss.rssi, mac);
  if (uidx >= 0) {
    colonyNodes[uidx].worker = ss.worker_id;
    colonyNodes[uidx].hashRate = ss.hash_rate;
    colonyNodes[uidx].shares = 0;
    colonyNodes[uidx].rejects = 0;
    colonyNodes[uidx].bestBits = ss.best_bits;
    colonyNodes[uidx].targetBits = coreTargetBits;
    colonyNodes[uidx].aiBatch = ss.dynamic_batch ? ss.dynamic_batch : ss.effective_batch;
    colonyNodes[uidx].aiHint = ss.ai_hint;
    colonyNodes[uidx].jobAgeMs = (uint32_t)ss.job_age_s * 1000UL;
    colonyNodes[uidx].uptime = ss.uptime_ms / 1000UL;
    colonyNodes[uidx].entropy = (float)ss.entropy_x1000 / 1000.0f;
    colonyNodes[uidx].loss = clipf((float)ss.prediction_error_x1000 / 1000.0f, -9.0f, 9.0f);
    colonyNodes[uidx].sync = clipf(1.0f - ((float)ss.thermal_load / 130.0f) - ((float)ss.loop_jitter_us / 40000.0f), 0.0f, 1.0f);
    colonyNodes[uidx].fit = (float)ss.knn_confidence / 100.0f;
    colonyNodes[uidx].v[0] = ss.thermal_load;
    colonyNodes[uidx].v[1] = ss.dynamic_batch;
    colonyNodes[uidx].v[2] = ss.hash_eff_x1000;
    colonyNodes[uidx].v[3] = ss.loop_jitter_us;
    colonyNodes[uidx].v[4] = ss.free_heap;
    colonyNodes[uidx].v[5] = ss.flags;
    colonyNodes[uidx].v[6] = ss.touch_delta;
    colonyNodes[uidx].v[7] = ss.radio_mode;
    universalRecountNodes();
    universalMirrorToFixedSlot(uidx);
  }

  String ssId = String(ss.nodeId) + String(ss.kind);
  if (ssId.indexOf("ZimGeek") >= 0 || ssId.indexOf("zim_solo") >= 0 || ssId.indexOf("ZIM_SOLO") >= 0) {
    uint8_t zSector = ss.ai_hint % JanusGalaxyStationSim::UNIVERSE_SECTORS;
    galaxy.universePilotSector = zSector;
    galaxy.universeSelectedSector = zSector;
    galaxy.universePilotDistance += (uint32_t)(1 + (ss.hash_rate / 500UL) + (ss.best_bits / 8U));
    galaxy.universePartyPower = clipf(galaxy.universePartyPower * 0.88f + clipf((float)ss.knn_confidence / 80.0f + (float)ss.best_bits / 80.0f, 0.0f, 1.8f) * 0.12f, 0.0f, 2.0f);
    galaxy.universeInfluence[zSector] = clipf(galaxy.universeInfluence[zSector] + 0.0025f + (float)ss.best_bits * 0.00006f, 0.0f, 1.5f);
    galaxy.universeSupply[zSector] = clipf(galaxy.universeSupply[zSector] + (float)(ss.nonce_remaining_l16 & 0x00FF) * 0.000025f, 0.0f, 1.5f);
    if (ss.flags & 0x0100) galaxy.universeThreat[zSector] = clipf(galaxy.universeThreat[zSector] - 0.0030f, 0.0f, 1.5f);
    snprintf(galaxy.universePilotLine, sizeof(galaxy.universePilotLine),
             "Zim: S%02u mode%u H%lu B%u fuel%u", (unsigned)zSector, (unsigned)ss.palette,
             (unsigned long)ss.hash_rate, (unsigned)ss.best_bits, (unsigned)ss.nonce_remaining_l16);
    snprintf(galaxy.p[6].name, sizeof(galaxy.p[6].name), "Zim Earth");
    snprintf(galaxy.p[6].role, sizeof(galaxy.p[6].role), "ZIM");
    galaxy.p[6].online = true;
    galaxy.p[6].signal = clipf((float)ss.knn_confidence / 100.0f, 0.0f, 1.0f);
    galaxy.p[6].risk = clipf(galaxy.universeThreat[zSector], 0.02f, 1.0f);
    eventLine = String("Zim report S") + String(zSector) + " H:" + compactU(ss.hash_rate);
  }

  if (millis() - colonyLastRosterMs < 400UL) {
    eventLine = String("SS ") + ss.nodeId + " H:" + compactU(ss.hash_rate) + " T:" + String(ss.thermal_load);
  }
#endif
}

void archiveZimAgentMemory(const ZimAgentMemoryPacket& za) {
#if CORE2_SD_ARCHIVE_ENABLE
  if (!core2SdArchiveOk) return;
  File f = SD.open("/janus/zim_memory.csv", FILE_APPEND);
  if (!f) return;
  if (f.size() == 0) {
    f.println("ms,seq,epoch,updates,accepts,buzzShares,policy,confidence,weapon,mood,sha,btc,curiosity,suspicion,ego,trustBuzz,trustSwarm,thought");
  }
  String row;
  row.reserve(220);
  row += String(millis()); row += ',';
  row += String(za.seq); row += ',';
  row += String(za.epoch); row += ',';
  row += String(za.updates); row += ',';
  row += String(za.accepts); row += ',';
  row += String(za.buzzShares); row += ',';
  row += String(za.policy); row += ',';
  row += String(za.confidence); row += ',';
  row += String(za.weaponCharge); row += ',';
  row += String(za.slimeMood); row += ',';
  row += String(za.shaObsession); row += ',';
  row += String(za.btcHunger); row += ',';
  row += String(za.curiosity); row += ',';
  row += String(za.suspicion); row += ',';
  row += String(za.ego); row += ',';
  row += String(za.trustBuzz); row += ',';
  row += String(za.trustSwarm); row += ',';
  String th = String(za.thought);
  th.replace(',', ';');
  row += th;
  f.println(row);
  f.close();
  core2ZimAgentArchiveRows++;
#endif
}

void handleZimAgentMemoryRaw(const uint8_t* data, uint16_t len, int8_t rxRssi, const uint8_t* mac) {
  if (!data || len != sizeof(ZimAgentMemoryPacket)) return;
  ZimAgentMemoryPacket za{};
  memcpy(&za, data, sizeof(za));
  if (za.magic[0] != 'Z' || za.magic[1] != 'A' || za.version != 1) return;

  core2ZimAgentRx++;
  core2LastZimAgentMs = millis();
  core2ZimLastPolicy = za.policy;
  core2ZimLastConfidence = za.confidence;
  core2ZimLastWeapon = za.weaponCharge;
  core2ZimLastMood = za.slimeMood;
  strlcpy(core2ZimThought, za.thought[0] ? za.thought : "-", sizeof(core2ZimThought));
  snprintf(core2ZimBrainLine, sizeof(core2ZimBrainLine),
           "ZimBrain pol%u conf%u gun%u SHA%u BTC%u: %.31s",
           (unsigned)za.policy, (unsigned)za.confidence, (unsigned)za.weaponCharge,
           (unsigned)za.shaObsession, (unsigned)za.btcHunger, core2ZimThought);

  int uidx = universalRememberNodeEx(za.nodeId[0] ? za.nodeId : "ZimGeek", za.kind[0] ? za.kind : "zim_slime_ai", rxRssi, mac);
  if (uidx >= 0) {
    colonyNodes[uidx].worker = za.worker_id;
    colonyNodes[uidx].entropy = ((float)za.shaObsession + (float)za.btcHunger + (float)za.curiosity) / 255.0f;
    colonyNodes[uidx].loss = (float)za.loss_x1000 / 1000.0f;
    colonyNodes[uidx].sync = (float)za.trustSwarm / 255.0f;
    colonyNodes[uidx].fit = (float)za.confidence / 100.0f;
    colonyNodes[uidx].v[0] = za.weaponCharge;
    colonyNodes[uidx].v[1] = za.policy;
    colonyNodes[uidx].v[2] = za.slimeMood;
    colonyNodes[uidx].v[3] = za.shaObsession;
    colonyNodes[uidx].v[4] = za.btcHunger;
    colonyNodes[uidx].v[5] = za.curiosity;
    colonyNodes[uidx].v[6] = za.trustBuzz;
    colonyNodes[uidx].v[7] = za.trustSwarm;
    universalRecountNodes();
    universalMirrorToFixedSlot(uidx);
  }

  uint8_t zSector = (uint8_t)((za.policy + za.slimeMood + za.route + (za.seq & 0x0F)) % JanusGalaxyStationSim::UNIVERSE_SECTORS);
  galaxy.universePilotSector = zSector;
  galaxy.universeSelectedSector = zSector;
  galaxy.universePilotDistance += 1 + za.accepts + (za.weaponCharge / 8U);
  galaxy.universePartyPower = clipf(galaxy.universePartyPower * 0.90f + ((float)za.confidence / 100.0f + (float)za.weaponCharge / 120.0f) * 0.10f, 0.0f, 2.0f);
  galaxy.universeInfluence[zSector] = clipf(galaxy.universeInfluence[zSector] + 0.0040f + (float)za.confidence * 0.000020f, 0.0f, 1.5f);
  galaxy.universeSupply[zSector] = clipf(galaxy.universeSupply[zSector] + (float)za.trustSwarm * 0.000018f, 0.0f, 1.5f);
  snprintf(galaxy.p[6].name, sizeof(galaxy.p[6].name), "Zim Earth");
  snprintf(galaxy.p[6].role, sizeof(galaxy.p[6].role), "ZIM");
  galaxy.p[6].online = true;
  galaxy.p[6].signal = clipf((float)za.confidence / 100.0f, 0.0f, 1.0f);
  snprintf(galaxy.universePilotLine, sizeof(galaxy.universePilotLine),
           "Zim Earth: pol%u gun%u mood%u / Core2 command laughs",
           (unsigned)za.policy, (unsigned)za.weaponCharge, (unsigned)za.slimeMood);
  eventLine = String("Zim memory ") + String(core2ZimThought);
  archiveZimAgentMemory(za);
}


void handleBuzzStatus(const JanusBuzzStatusPacket& bs, int8_t rxRssi) {
  int uidx = universalRememberNode(bs.nodeId, "BuzzLighter", rxRssi);
  if (uidx >= 0) {
    colonyNodes[uidx].hashRate = bs.hashRate;
    colonyNodes[uidx].shares = bs.shares;
    colonyNodes[uidx].rejects = bs.rejects;
    colonyNodes[uidx].bestBits = bs.bestBits;
    colonyNodes[uidx].diff = bs.diff;
    universalRecountNodes();
    universalMirrorToFixedSlot(uidx);
  }

  buzz.touch(bs.nodeId);
  strlcpy(buzz.role, "BuzzLighter", sizeof(buzz.role));
  buzz.rssi = rxRssi;
  buzz.hashRate = bs.hashRate;
  buzz.shares = bs.shares;
  buzz.rejects = bs.rejects;
  buzz.bestBits = bs.bestBits;
  buzz.diff = bs.diff;
  buzzDesiredPlaying = bs.playing && !bs.paused;
  buzzDesiredVolume = bs.volume;
  if (bs.track[0]) {
    strlcpy(buzzTrack, bs.track, sizeof(buzzTrack));
    buzzTrackLastMs = millis();
  }
  eventLine = String("Buzz status: ") + (buzzDesiredPlaying ? "play" : "pause");
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

void coreWriteLE32(uint8_t* p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

void coreHashToShareOrder(const uint8_t in[32], uint8_t out[32]) {
  for (int i = 0; i < 32; i++) out[i] = in[31 - i];
}

void doubleSha256Core(const uint8_t* data, size_t len, uint8_t out[32]) {
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

bool jobIdSame(const uint8_t a[8], const uint8_t b[8]) {
  for (int i = 0; i < 8; i++) if (a[i] != b[i]) return false;
  return true;
}

void formatCoreJobId(const uint8_t id[8]) {
  snprintf(coreJobText, sizeof(coreJobText), "%02X%02X%02X%02X", id[0], id[1], id[2], id[3]);
}

void coreRamanujanThetaTick(uint32_t now);
void coreConfigureThetaForJob(RemoteJobState& job);
uint32_t coreThetaNonceForCursor(const RemoteJobState& job, uint32_t cursor);

bool coreWorkerEnabled() {
  // Mining is now gated by the spatial visualizer: when GALAXY STATION is open,
  // Core2 contributes a small SHA256 batch while the map keeps rendering.
  return page == PAGE_SPACE;
}

void stopCore2Space(const char* reason) {
  if (coreJob.active || coreRemoteHashrate || coreHashCounter) {
    Serial.printf("[CORE2 SPACE] stop reason=%s job=%s H=%lu best=%lu\n",
                  reason ? reason : "-", coreJobText,
                  (unsigned long)coreRemoteHashrate, (unsigned long)coreBestBits);
  }
  coreJob.active = false;
  coreHashCounter = 0;
  coreRemoteHashrate = 0;
  coreWorkerStoppedMs = millis();
}

void startCore2SpaceIfPossible(const char* reason) {
  coreWorkerStartedMs = millis();
  coreHashCounter = 0;
  coreRemoteHashrate = 0;

  // If a fresh Buzz job was cached while the tab was closed, start it now.
  if (!coreJob.active && coreJob.rangeSize > 0 && coreJob.receivedAt > 0 && millis() - coreJob.receivedAt < 18000UL) {
    coreJob.nonce = coreJob.startNonce;
    coreJob.endNonce = coreJob.startNonce + coreJob.rangeSize;
    if (coreJob.endNonce < coreJob.startNonce) coreJob.endNonce = 0xFFFFFFFFUL;
    coreConfigureThetaForJob(coreJob);
    coreJob.active = true;
    eventLine = String("Core2 space cached ") + coreJobText;
  } else {
    eventLine = "Core2 space waiting swarm";
  }

  Serial.printf("[CORE2 SPACE] start reason=%s cached=%d job=%s\n",
                reason ? reason : "-", coreJob.active ? 1 : 0, coreJobText);
}

void updateCore2SpaceGate() {
  bool enabled = coreWorkerEnabled();
  if (enabled && !coreWorkerWasEnabled) {
    startCore2SpaceIfPossible("swarm-space-open");
    hapticPulse(80, 24);
  } else if (!enabled && coreWorkerWasEnabled) {
    stopCore2Space("swarm-space-closed");
    hapticPulse(45, 18);
  }
  coreWorkerWasEnabled = enabled;
}


uint32_t coreGcd32(uint32_t a, uint32_t b) {
  while (b) {
    uint32_t t = a % b;
    a = b;
    b = t;
  }
  return a ? a : 1;
}

uint32_t coreMix32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7feb352dUL;
  x ^= x >> 15;
  x *= 0x846ca68bUL;
  x ^= x >> 16;
  return x;
}

float corePowSmall(float q, uint16_t e) {
  float r = 1.0f;
  for (uint16_t i = 0; i < e; i++) r *= q;
  return r;
}

void coreRamanujanThetaTick(uint32_t now) {
#if CORE2_RAMANUJAN_THETA_ENABLE
  if (coreTheta.studies > 0 && now - coreTheta.lastMs < CORE2_RAMANUJAN_THETA_MS) return;
  coreTheta.lastMs = now;
  coreTheta.studies++;

  float entropy = clipf(homeEntropy(), 0.0f, 4.0f);
  float sync = clipf(homeSync(), 0.0f, 1.5f);
  float best = clipf((float)coreBestBits / 64.0f, 0.0f, 1.0f);
  float buzzBest = clipf((float)buzz.bestBits / 64.0f, 0.0f, 1.0f);
  float jobFresh = coreJob.receivedAt ? clipf(1.0f - ((float)(now - coreJob.receivedAt) / 18000.0f), 0.0f, 1.0f) : 0.0f;

  // q must stay inside the unit circle. This is the ESP32-safe finite notebook,
  // not a CAS and not an infinite loop.
  float q = 0.10f + entropy * 0.035f + sync * 0.08f + best * 0.24f + buzzBest * 0.08f + jobFresh * 0.10f;
  q = clipf(q, 0.08f, 0.86f);

  float phi = 1.0f;   // phi(q) = 1 + 2 sum q^(n*n)
  float psi = 1.0f;   // psi(q) = sum q^(n(n+1)/2), starts at n=0 -> 1
  float euler = 1.0f; // finite product for f(-q) = prod(1-q^n)
  float mock = 0.0f;  // tiny mock-theta-like third-order echo
  float denom = 1.0f;

  for (uint8_t n = 1; n <= CORE2_RAMANUJAN_DEPTH; n++) {
    phi += 2.0f * corePowSmall(q, (uint16_t)n * (uint16_t)n);
    psi += corePowSmall(q, ((uint16_t)n * ((uint16_t)n + 1)) / 2);
    euler *= (1.0f - corePowSmall(q, n));
    float one = 1.0f + corePowSmall(q, n);
    denom *= one * one;
    mock += corePowSmall(q, (uint16_t)n * (uint16_t)n) / max(0.0001f, denom);
  }

  float modBalance = fabsf(phi - psi) + (1.0f - euler) + mock * 1.7f;
  float resonance = clipf(modBalance / 4.75f, 0.0f, 1.0f);
  float confidence = clipf(0.18f + sync * 0.32f + jobFresh * 0.22f + resonance * 0.28f - entropy * 0.035f, 0.0f, 1.0f);

  uint32_t seed = coreMix32((uint32_t)(q * 1000000.0f) ^ (coreBestBits << 17) ^ (buzz.bestBits << 7) ^ rxPackets ^ millis());
  static const uint16_t primes[] = {1,3,5,7,11,13,17,19,23,29,31,37,41,43,47,53,59,61,67,71,73,79,83,89,97,101,103,107,109,113,127,131};
  uint8_t pi = (uint8_t)((seed ^ (uint32_t)(resonance * 255.0f)) % (sizeof(primes) / sizeof(primes[0])));
  uint32_t stride = primes[pi];

  coreTheta.q = q;
  coreTheta.phi = phi;
  coreTheta.psi = psi;
  coreTheta.euler = euler;
  coreTheta.mock = mock;
  coreTheta.resonance = resonance;
  coreTheta.confidence = confidence;
  coreTheta.seed = seed;
  coreTheta.stride = stride;
  coreTheta.offset = seed;
  coreTheta.carrIndex = (uint16_t)(1 + (seed % 5000UL));
  coreTheta.lemma = (uint16_t)((seed >> 16) ^ (uint32_t)(resonance * 4096.0f));
  snprintf(coreTheta.line, sizeof(coreTheta.line), "Carr#%u q%.2f ph%.2f ps%.2f mk%.2f rs%.2f", (unsigned)coreTheta.carrIndex, q, phi, psi, mock, resonance);

  // Tiny confidence coupling only. This does not change target rules.
  coreAiSkill = clipf(coreAiSkill * 0.997f + confidence * 0.010f + resonance * 0.006f, 0.05f, 3.0f);

  if (now - coreTheta.lastLogMs > CORE2_RAMANUJAN_SERIAL_MS) {
    coreTheta.lastLogMs = now;
    Serial.printf("[CORE2/RAMA] study=%lu Carr#%u q=%.3f phi=%.3f psi=%.3f euler=%.3f mock=%.3f res=%.3f conf=%.3f stride=%lu\n",
                  (unsigned long)coreTheta.studies, (unsigned)coreTheta.carrIndex,
                  q, phi, psi, euler, mock, resonance, confidence, (unsigned long)stride);
  }
#endif
}

void coreConfigureThetaForJob(RemoteJobState& job) {
#if CORE2_RAMANUJAN_THETA_ENABLE
  coreRamanujanThetaTick(millis());
  job.thetaCursor = 0;
  job.thetaStride = max(1UL, coreTheta.stride);
  if (job.rangeSize > 1) {
    while (coreGcd32(job.thetaStride, job.rangeSize) != 1) job.thetaStride += 2;
    job.thetaOffset = coreTheta.offset % job.rangeSize;
  } else {
    job.thetaOffset = 0;
    job.thetaStride = 1;
  }
  job.nonce = job.startNonce;
#else
  job.thetaCursor = 0;
  job.thetaOffset = 0;
  job.thetaStride = 1;
#endif
}

uint32_t coreThetaNonceForCursor(const RemoteJobState& job, uint32_t cursor) {
#if CORE2_RAMANUJAN_THETA_ENABLE && CORE2_RAMANUJAN_NONCE_WALK
  if (job.rangeSize == 0) return job.startNonce + cursor;
  uint32_t pos = (uint32_t)(((uint64_t)cursor * (uint64_t)max(1UL, job.thetaStride) + (uint64_t)job.thetaOffset) % (uint64_t)job.rangeSize);
  return job.startNonce + pos;
#else
  return job.startNonce + cursor;
#endif
}

void handleJobPacket(const JobPacket& jp) {
  if (jp.range_size == 0) return;
  memcpy(coreJob.job_id, jp.job_id, 8);
  memcpy(coreJob.header, jp.header, 80);
  memcpy(coreJob.target, jp.target, 32);
  coreJob.startNonce = jp.start_nonce;
  coreJob.rangeSize = jp.range_size;
  coreJob.nonce = jp.start_nonce;
  coreJob.endNonce = jp.start_nonce + jp.range_size;
  if (coreJob.endNonce < coreJob.startNonce) coreJob.endNonce = 0xFFFFFFFFUL;
  coreJob.receivedAt = millis();
  coreJob.extranonce2 = jp.extranonce2;
  coreConfigureThetaForJob(coreJob);
  coreJob.active = coreWorkerEnabled();
  coreTargetBits = countLeadingZeroBitsBE(coreJob.target);
  formatCoreJobId(coreJob.job_id);
  coreLastJobMs = millis();
  coreJobsSeen++;
  Serial.printf("[CORE2 MINER] JOB rx=%s start=%08lX range=%lu targetBits=%u active=%d\n",
                coreJobText, (unsigned long)coreJob.startNonce, (unsigned long)coreJob.rangeSize,
                (unsigned)coreTargetBits, coreJob.active ? 1 : 0);
  eventLine = coreJob.active ? String("Core2 space mining ") + coreJobText : String("Buzz job cached ") + coreJobText;
}

void sendCoreShare(const RemoteJobState& job, uint32_t nonce) {
  if (!espnowOk) return;
  ensureColonyPeer();
  ShareResponse sr{};
  sr.magic[0] = 'S';
  sr.magic[1] = 'R';
  memcpy(sr.job_id, job.job_id, 8);
  sr.nonce = nonce;
  sr.worker_id = colonyWorkerId;
  esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&sr, sizeof(sr));
  coreSharesSent++;
  coreLastShareMs = millis();
  eventLine = String("Core2 SHARE nonce=") + String(nonce, HEX);
#if CORE2_MINER_SHARE_BEEP
  if (!speakerMuted) M5.Speaker.tone(1568, 70);
#endif
  Serial.printf("[CORE2 MINER] SHARE sent job=%s nonce=%08lX total=%lu\n",
                coreJobText, (unsigned long)nonce, (unsigned long)coreSharesSent);
}

void runCore2MiningBatch() {
  if (!coreWorkerEnabled()) {
    if (coreJob.active || coreHashCounter || coreRemoteHashrate) stopCore2Space("gate-off");
    return;
  }
  if (!coreJob.active) return;

  uint32_t now = millis();
  if (now - coreJob.receivedAt > 18000UL) {
    coreJob.active = false;
    coreJobExpired++;
    eventLine = "Core2 space job timeout";
    return;
  }

  coreRamanujanThetaTick(now);

  // Keep UI responsive: small adaptive batch, boosted when the map is confident.
  // v6.39: theta resonance can spend a few extra hashes only while the station is already open.
  uint16_t batch = CORE2_MINER_LOW_BATCH;
  batch += (uint16_t)clipf(homeEntropy() * 3.0f, 0.0f, 18.0f);
  batch += (uint16_t)clipf(homeSync() * 12.0f, 0.0f, 22.0f);
  batch += (uint16_t)clipf(spaceConfidence * 12.0f, 0.0f, 16.0f);
  batch += (uint16_t)clipf(coreTheta.resonance * 10.0f + coreTheta.confidence * 6.0f, 0.0f, 16.0f);
  if (batch > CORE2_MINER_MAX_BATCH) batch = CORE2_MINER_MAX_BATCH;

  uint8_t hash[32];
  uint8_t shareHash[32];
  for (uint16_t i = 0; i < batch; i++) {
    if (coreJob.thetaCursor >= coreJob.rangeSize) {
      coreJob.active = false;
      eventLine = "Core2 theta range done";
      break;
    }
    uint32_t cursor = coreJob.thetaCursor++;
    uint32_t nonce = coreThetaNonceForCursor(coreJob, cursor);
    coreJob.nonce = coreJob.startNonce + coreJob.thetaCursor;  // legacy progress cursor
    coreWriteLE32(coreJob.header + 76, nonce);
    doubleSha256Core(coreJob.header, 80, hash);
    coreHashToShareOrder(hash, shareHash);
    coreHashCounter++;

    // v6.19 miner fix: same gate as working Buzz v10.6.
    // Only reversed/display-order hash is compared with Buzz target.
    // Do NOT use raw hash or max(bitsA,bitsB); that can create false shares.
    uint16_t bits = countLeadingZeroBitsBE(shareHash);
    if (bits > coreBestBits) {
      coreBestBits = bits;
      if (bits >= 20) {
        Serial.printf("[CORE2 MINER] BEST bits=%u target=%u job=%s nonce=%08lX\n",
                      (unsigned)bits, (unsigned)coreTargetBits, coreJobText, (unsigned long)nonce);
      }
    }

    bool targetOk = (bits >= coreTargetBits) && hashMeetsTargetBE(shareHash, coreJob.target);
    if (targetOk) {
      sendCoreShare(coreJob, nonce);
      coreJob.active = false;
      coreAiSkill = clipf(coreAiSkill + 0.025f, 0.05f, 3.0f);
      break;
    }
  }

  if (now - coreLastHashTickMs >= 1000UL) {
    coreRemoteHashrate = coreHashCounter;
    coreHashCounter = 0;
    coreLastHashTickMs = now;
  }
}

uint8_t f2b(float v) {
  return (uint8_t)clipf(v * 255.0f, 0.0f, 255.0f);
}

uint8_t decayByte(uint8_t v, uint8_t d) {
  return (v > d) ? (uint8_t)(v - d) : 0;
}

uint8_t blendMax(uint8_t oldV, uint8_t newV) {
  return newV > oldV ? newV : oldV;
}

float spaceRand01() {
  slime.rng = slime.rng * 1664525UL + 1013904223UL + (uint32_t)(airEntropy * 997.0f) + rxPackets;
  return (float)(slime.rng & 0xFFFF) / 65535.0f;
}

const char* slimeFace() {
  return SLIME_SMILES[slime.faceIndex % SLIME_SMILE_COUNT].face;
}

const char* slimeMood() {
  return SLIME_SMILES[slime.faceIndex % SLIME_SMILE_COUNT].mood;
}

const char* spaceEventName(uint8_t id) {
  switch (id) {
    case 1: return "quiet map";
    case 2: return "new signal";
    case 3: return "motion";
    case 4: return "air alert";
    case 5: return "low confidence";
    case 6: return "swarm sync";
    case 7: return "user tap";
    case 8: return "learning";
    case 9: return "tracking";
    default: return "listening";
  }
}

void slimeRemember(uint8_t eventId, float intensity) {
  uint8_t i = slime.episodeHead % JANUS_SLIME_EPISODES;
  slime.episodes[i].t = millis() / 1000UL;
  slime.episodes[i].eventId = eventId;
  slime.episodes[i].faceIndex = slime.faceIndex;
  slime.episodes[i].intensity = f2b(clipf(intensity, 0.0f, 1.0f));
  slime.episodes[i].nodeMask = spaceNodeMask;
  slime.episodeHead = (uint8_t)((slime.episodeHead + 1) % JANUS_SLIME_EPISODES);
}

void spaceSave(bool force) {
  if (!force && millis() - spaceLastSaveMs < 20000UL) return;
  slime.magic = JANUS_SPACE_MAGIC;
  slime.version = JANUS_SLIME_VERSION;
  slime.lastSaveMs = millis();
  prefs.putBytes("slime2", &slime, sizeof(slime));
  spaceLastSaveMs = millis();
  Serial.printf("[GALAXY STATION] saved ticks=%lu conf=%.2f risk=%.2f face=%s\n",
                (unsigned long)slime.ticks, slime.mapConfidence, slime.predRisk, slimeFace());
}

void spaceResetMap() {
  memset(spaceMap, 0, sizeof(spaceMap));
  spaceObservationCount = 0;
  spaceResetNodeLayout();
}

void spaceResetBrain() {
  memset(&slime, 0, sizeof(slime));
  slime.magic = JANUS_SPACE_MAGIC;
  slime.version = JANUS_SLIME_VERSION;
  slime.rng = 0x51A1E202UL ^ (uint32_t)ESP.getEfuseMac();
  slime.trustEye = 0.72f;
  slime.trustMic = 0.58f;
  slime.trustAir = 0.66f;
  slime.trustRssi = 0.48f;
  slime.trustSwarm = 0.62f;
  slime.comfortAvg = 0.55f;
  slime.stability = 0.38f;
  slime.curiosity = 0.74f;
  slime.empathy = 0.50f;
  slime.faceIndex = 2; // o_o
  slime.lastEventId = 0;
  selfPose.x = 0.0f;
  selfPose.y = 0.0f;
  selfPose.yaw = 0.0f;
  selfPose.confidence = 0.35f;
  strlcpy(slimeLine, "o_o first room scan", sizeof(slimeLine));
  strlcpy(spaceEvent, "new slime brain", sizeof(spaceEvent));
}

void spaceInit() {
  spaceResetMap();
  size_t n = prefs.getBytesLength("slime2");
  if (n == sizeof(SlimeBrain)) {
    prefs.getBytes("slime2", &slime, sizeof(slime));
    if (slime.magic == JANUS_SPACE_MAGIC && slime.version == JANUS_SLIME_VERSION) {
      slime.trustEye = clipf(slime.trustEye, 0.05f, 2.0f);
      slime.trustMic = clipf(slime.trustMic, 0.05f, 2.0f);
      slime.trustAir = clipf(slime.trustAir, 0.05f, 2.0f);
      slime.trustRssi = clipf(slime.trustRssi, 0.05f, 2.0f);
      slime.trustSwarm = clipf(slime.trustSwarm, 0.05f, 2.0f);
      slime.faceIndex %= SLIME_SMILE_COUNT;
      snprintf(slimeLine, sizeof(slimeLine), "%s memory loaded", slimeFace());
      strlcpy(spaceEvent, "slime memory loaded", sizeof(spaceEvent));
    } else {
      spaceResetBrain();
      spaceSave(true);
    }
  } else {
    spaceResetBrain();
    spaceSave(true);
  }
}

uint8_t onlineNodeCount() {
  universalRecountNodes();
  if (colonyKnownCount > 0) return colonyOnlineCount;

  uint8_t c = 0;
  if (eye.refresh()) c++;
  if (beacon.refresh()) c++;
  if (buzz.refresh()) c++;
  if (audioNode.refresh()) c++;
  if (swarm.refresh()) c++;
  if (stick.refresh()) c++;
  if (unknownNode.refresh()) c++;
  return c;
}

float nodeSignalRaw(bool online, uint32_t lastMs, int8_t rssi) {
  if (!online && lastMs == 0) return 0.0f;
  float r = (float)rssi;
  if (r == 0.0f) r = -84.0f;
  return clipf((r + 96.0f) / 56.0f, 0.0f, 1.0f);
}

float nodeSlotAngle(uint8_t idx) {
  // Stable semantic bearings: Eye/front, Beacon/front-left, Buzz/front-right,
  // Mic/Swarm side sensors, Stick/mobile node, Unknown/back.
  static const float base[JANUS_SPACE_NODE_SLOTS] = {
    -JANUS_PI * 0.50f, -JANUS_PI * 0.78f, -JANUS_PI * 0.22f,
    JANUS_PI * 0.95f, 0.0f, JANUS_PI * 0.50f, JANUS_PI * 0.78f
  };
  if (idx >= JANUS_SPACE_NODE_SLOTS) idx = JANUS_SPACE_NODE_SLOTS - 1;
  return base[idx] + selfPose.yaw + spaceUserYaw;
}

void spaceResetNodeLayout() {
  for (uint8_t i = 0; i < JANUS_SPACE_NODE_SLOTS; i++) {
    float a = nodeSlotAngle(i);
    float r = 7.0f + i * 0.8f;
    spaceNodeX[i] = JANUS_SPACE_W * 0.5f + cosf(a) * r;
    spaceNodeY[i] = JANUS_SPACE_H * 0.5f + sinf(a) * r * 0.72f;
    spaceNodeConf[i] = 0.0f;
    spaceNodeSignal[i] = 0.0f;
    spaceNodeMotion[i] = 0.0f;
  }
}

void spaceUpdateNodeLayout(uint8_t idx, bool online, uint32_t lastMs, int8_t rssi, float entropyRaw, float lossRaw, float syncRaw, float v0, float v1, float v2, float basePresence, float baseMotion, float baseSound, float baseAir, float sensorTrust) {
  if (idx >= JANUS_SPACE_NODE_SLOTS) return;
  if (!online || lastMs == 0 || millis() - lastMs >= NODE_TIMEOUT_MS) {
    spaceNodeConf[idx] *= 0.985f;
    spaceNodeMotion[idx] *= 0.96f;
    return;
  }

  float sig = nodeSignalRaw(online, lastMs, rssi);
  float entropy = clipf(entropyRaw / 8.0f, 0.0f, 1.0f);
  float loss = clipf(lossRaw, 0.0f, 1.0f);
  float conf = clipf(0.18f + sig * 0.48f + sensorTrust * 0.20f + syncRaw * 0.10f - loss * 0.08f, 0.0f, 1.0f);

  // Physical device placement is deliberately stable: without UWB/ToF we cannot know exact XY.
  // So RSSI estimates distance, while device role / learned sector estimates bearing.
  // Mobile Stick tilt is used as an active scan beam, not as the Stick marker position.
  float a = nodeSlotAngle(idx);
  float tx, ty;
  float radius = 3.5f + (1.0f - sig) * 14.5f + entropy * 3.0f;
  tx = JANUS_SPACE_W * 0.5f + cosf(a) * radius;
  ty = JANUS_SPACE_H * 0.5f + sinf(a) * radius * 0.72f;

  tx = clipf(tx, 1.0f, JANUS_SPACE_W - 2.0f);
  ty = clipf(ty, 1.0f, JANUS_SPACE_H - 2.0f);

  float keep = spaceNodeConf[idx] > 0.02f ? 0.86f : 0.0f;
  spaceNodeX[idx] = spaceNodeX[idx] * keep + tx * (1.0f - keep);
  spaceNodeY[idx] = spaceNodeY[idx] * keep + ty * (1.0f - keep);
  spaceNodeConf[idx] = spaceNodeConf[idx] * 0.82f + conf * 0.18f;
  spaceNodeSignal[idx] = spaceNodeSignal[idx] * 0.84f + sig * 0.16f;
  spaceNodeMotion[idx] = spaceNodeMotion[idx] * 0.72f + clipf(baseMotion + fabsf(v0) / 380.0f, 0.0f, 1.5f) * 0.28f;
}

void paintSpaceBlob(int cx, int cy, int radius, float occ, float motion, float sound, float air, float presence, float conf) {
  if (radius < 1) radius = 1;
  for (int yy = cy - radius; yy <= cy + radius; yy++) {
    if (yy < 0 || yy >= JANUS_SPACE_H) continue;
    for (int xx = cx - radius; xx <= cx + radius; xx++) {
      if (xx < 0 || xx >= JANUS_SPACE_W) continue;
      float dx = (float)(xx - cx);
      float dy = (float)(yy - cy);
      float d = sqrtf(dx * dx + dy * dy);
      if (d > radius) continue;
      float f = 1.0f - d / (float)radius;
      SpaceCell& c = spaceMap[yy][xx];
      c.occ      = blendMax(c.occ,      f2b(occ * f));
      c.motion   = blendMax(c.motion,   f2b(motion * f));
      c.sound    = blendMax(c.sound,    f2b(sound * f));
      c.air      = blendMax(c.air,      f2b(air * f));
      c.presence = blendMax(c.presence, f2b(presence * f));
      c.conf     = blendMax(c.conf,     f2b(conf * f));
    }
  }
}

void paintSpaceRay(int x0, int y0, int x1, int y1, float freeConf, float echo, float sound, float motion) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  int x = x0, y = y0;
  uint8_t step = 0;
  while (true) {
    if (x >= 0 && x < JANUS_SPACE_W && y >= 0 && y < JANUS_SPACE_H) {
      SpaceCell& c = spaceMap[y][x];
      float fade = clipf(1.0f - (float)step / 34.0f, 0.08f, 1.0f);
      c.conf = blendMax(c.conf, f2b(freeConf * fade));
      c.sound = blendMax(c.sound, f2b(sound * fade * 0.35f));
      c.motion = blendMax(c.motion, f2b(motion * fade * 0.25f));
      // The ray itself is weak occupancy. Strong occupancy is painted at echoes / reflections.
      c.occ = blendMax(c.occ, f2b(echo * fade * 0.10f));
    }
    if (x == x1 && y == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x += sx; }
    if (e2 <= dx) { err += dx; y += sy; }
    if (++step > 64) break;
  }
}

void paintNetworkEcho(uint8_t idx, int nodeX, int nodeY, float v0, float v1, float v2, float presence, float motion, float sound, float conf) {
  float beamA = nodeSlotAngle(idx);
  // StickS3 sends hand tilt in v1/v2; use that as an active beam direction.
  if (idx == 5 && (fabsf(v1) + fabsf(v2)) > 0.05f) {
    beamA = atan2f(clipf(v2, -1.0f, 1.0f), clipf(v1, -1.0f, 1.0f)) + spaceUserYaw;
  }
  float strength = clipf(presence * 0.55f + motion * 0.65f + sound * 0.45f + fabsf(v0) / 1800.0f, 0.0f, 1.5f);
  if (strength < 0.035f) return;

  int ex = (int)roundf(nodeX + cosf(beamA) * (3.0f + strength * 7.0f));
  int ey = (int)roundf(nodeY + sinf(beamA) * (2.0f + strength * 5.0f));
  ex = constrain(ex, 0, JANUS_SPACE_W - 1);
  ey = constrain(ey, 0, JANUS_SPACE_H - 1);

  paintSpaceRay(JANUS_SPACE_W / 2, JANUS_SPACE_H / 2, nodeX, nodeY, conf * 0.45f, 0.04f, sound, motion);
  paintSpaceRay(nodeX, nodeY, ex, ey, conf * 0.35f, strength * 0.24f, sound, motion);
  paintSpaceBlob(ex, ey, 1 + (int)(strength * 4.0f), strength * 0.30f, motion, sound, 0.0f, presence, conf);
}

void spaceDecay() {
  for (int y = 0; y < JANUS_SPACE_H; y++) {
    for (int x = 0; x < JANUS_SPACE_W; x++) {
      SpaceCell& c = spaceMap[y][x];
      c.occ = decayByte(c.occ, 1);
      c.conf = decayByte(c.conf, 1);
      c.motion = decayByte(c.motion, 4);
      c.sound = decayByte(c.sound, 5);
      c.air = decayByte(c.air, 1);
      c.presence = decayByte(c.presence, 2);
    }
  }
}

void spaceUpdatePose(float dt) {
  // Без отдельной одометрии Core2 остаётся центром карты. Движение оценивается по изменению RSSI/датчиков.
  float motionHint = fabsf(spaceMotion - slime.lastMotion) + fabsf(spacePresence - slime.lastPresence) * 0.35f;
  selfPose.vyaw = selfPose.vyaw * 0.94f + (motionHint - 0.08f) * 0.025f;
  selfPose.yaw += selfPose.vyaw * dt + spaceUserYaw * 0.018f;
  if (selfPose.yaw > JANUS_TWO_PI) selfPose.yaw -= JANUS_TWO_PI;
  if (selfPose.yaw < -JANUS_TWO_PI) selfPose.yaw += JANUS_TWO_PI;
  selfPose.confidence = clipf(selfPose.confidence * 0.96f + spaceConfidence * 0.04f, 0.0f, 1.0f);
}

void assimilateNodeSample(bool online, uint32_t lastMs, int8_t rssi, float entropyRaw, float lossRaw, float syncRaw, uint8_t idx, float basePresence, float baseMotion, float baseSound, float baseAir, float v0, float v1, float v2, float sensorTrust) {
  if (!online || lastMs == 0 || millis() - lastMs >= NODE_TIMEOUT_MS) {
    spaceUpdateNodeLayout(idx, online, lastMs, rssi, entropyRaw, lossRaw, syncRaw, v0, v1, v2, basePresence, baseMotion, baseSound, baseAir, sensorTrust);
    return;
  }
  spaceNodeMask |= (1 << idx);
  spaceUpdateNodeLayout(idx, online, lastMs, rssi, entropyRaw, lossRaw, syncRaw, v0, v1, v2, basePresence, baseMotion, baseSound, baseAir, sensorTrust);

  float sig = nodeSignalRaw(online, lastMs, rssi);
  float entropy = clipf(entropyRaw / 8.0f, 0.0f, 1.0f);
  float occ = clipf(0.10f + entropy * 0.22f + basePresence * 0.28f + sig * 0.15f, 0.0f, 1.0f);
  int cx = (int)roundf(spaceNodeX[idx]);
  int cy = (int)roundf(spaceNodeY[idx]);
  int radius = 2 + (int)(entropy * 3.0f) + (int)(baseMotion * 2.0f);
  paintSpaceBlob(cx, cy, radius, occ, baseMotion, baseSound, baseAir, basePresence, spaceNodeConf[idx]);
  paintNetworkEcho(idx, cx, cy, v0, v1, v2, basePresence, baseMotion, baseSound, spaceNodeConf[idx]);
  spaceObservationCount++;
}

void spaceAssimilateSwarm() {
  spaceNodeMask = 0;

  float eyePresence = eye.refresh() ? clipf((eye.v1 + eye.v2) / 1200.0f, 0.0f, 1.4f) : 0.0f;
  float eyeMotion = eye.refresh() ? clipf((fabsf(eye.v3) + fabsf(eye.v4) + fabsf(eye.v5)) / 1800.0f, 0.0f, 1.0f) : 0.0f;
  float micSound = audioNode.refresh() ? clipf(audioNode.v0 / 1600.0f, 0.0f, 1.5f) : 0.0f;
  float swarmSound = swarm.refresh() ? clipf(swarm.v0 / 1600.0f, 0.0f, 1.2f) : 0.0f;
  float localAir = clipf(airScore / 5.0f, 0.0f, 1.5f);

  spacePresence = clipf(eyePresence * slime.trustEye + nodeSignalRaw(beacon.online, beacon.lastMs, beacon.rssi) * 0.18f + nodeSignalRaw(stick.online, stick.lastMs, stick.rssi) * 0.14f, 0.0f, 1.5f);
  spaceSound = clipf((micSound + swarmSound) * 0.5f * slime.trustMic, 0.0f, 1.5f);
  spaceAir = clipf(localAir * slime.trustAir, 0.0f, 1.5f);
  spaceMotion = clipf(eyeMotion * 0.66f + spaceSound * 0.32f + fabsf(airTrend) * 0.07f, 0.0f, 1.5f);

  // Local air halo around the device itself.
  paintSpaceBlob(JANUS_SPACE_W / 2, JANUS_SPACE_H / 2, 4 + (int)(spaceAir * 5.0f), 0.02f, 0.0f, 0.0f, spaceAir, 0.0f, 0.34f + spaceAir * 0.2f);

  assimilateNodeSample(eye.online, eye.lastMs, eye.rssi, eye.entropy, eye.loss, eye.sync, 0, eyePresence, eyeMotion, 0.0f, 0.0f, eye.v0, eye.v1, eye.v2, slime.trustEye);
  assimilateNodeSample(beacon.online, beacon.lastMs, beacon.rssi, beacon.entropy, beacon.loss, beacon.sync, 1, nodeSignalRaw(beacon.online, beacon.lastMs, beacon.rssi) * 0.25f, 0.04f, 0.0f, clipf(beacon.v2 / 4.0f, 0.0f, 0.8f), beacon.v0, beacon.v1, beacon.v2, slime.trustRssi);
  assimilateNodeSample(buzz.online, buzz.lastMs, buzz.rssi, buzz.entropy, buzz.loss, buzz.sync, 2, nodeSignalRaw(buzz.online, buzz.lastMs, buzz.rssi) * 0.18f, 0.05f, 0.0f, 0.0f, buzz.v0, buzz.v1, buzz.v2, slime.trustRssi);
  assimilateNodeSample(audioNode.online, audioNode.lastMs, audioNode.rssi, audioNode.entropy, audioNode.loss, audioNode.sync, 3, 0.08f, clipf(micSound * 0.25f, 0.0f, 1.0f), micSound, 0.0f, audioNode.v0, audioNode.v1, audioNode.v2, slime.trustMic);
  assimilateNodeSample(swarm.online, swarm.lastMs, swarm.rssi, swarm.entropy, swarm.loss, swarm.sync, 4, 0.10f, clipf(swarmSound * 0.25f, 0.0f, 1.0f), swarmSound, 0.0f, swarm.v0, swarm.v1, swarm.v2, slime.trustSwarm);
  assimilateNodeSample(stick.online, stick.lastMs, stick.rssi, stick.entropy, stick.loss, stick.sync, 5, nodeSignalRaw(stick.online, stick.lastMs, stick.rssi) * 0.20f, clipf(fabsf(stick.v1) + fabsf(stick.v2), 0.0f, 1.0f), 0.0f, 0.0f, stick.v0, stick.v1, stick.v2, slime.trustRssi);
  assimilateNodeSample(unknownNode.online, unknownNode.lastMs, unknownNode.rssi, unknownNode.entropy, unknownNode.loss, unknownNode.sync, 6, nodeSignalRaw(unknownNode.online, unknownNode.lastMs, unknownNode.rssi) * 0.15f, 0.02f, 0.0f, 0.0f, unknownNode.v0, unknownNode.v1, unknownNode.v2, 0.35f);
}

float mapMeanConfidence() {
  uint32_t sum = 0;
  uint16_t n = 0;
  for (int y = 0; y < JANUS_SPACE_H; y++) {
    for (int x = 0; x < JANUS_SPACE_W; x++) {
      uint8_t m = max(max(spaceMap[y][x].occ, spaceMap[y][x].motion), max(spaceMap[y][x].sound, spaceMap[y][x].presence));
      if (m > 8) {
        sum += spaceMap[y][x].conf;
        n++;
      }
    }
  }
  if (!n) return 0.0f;
  return clipf((float)sum / (float)n / 255.0f, 0.0f, 1.0f);
}

void spaceChooseSmile(uint8_t eventId, float intensity) {
  uint8_t face = 0;
  if (onlineNodeCount() == 0) face = 10;              // ?_?
  else if (spaceAir > 0.85f) face = 5;                // T_T
  else if (spaceMotion > 0.72f) face = 3;             // :O
  else if (spaceNovelty > 0.62f) face = 11;           // 0_0
  else if (spaceConfidence < 0.22f) face = 4;         // :/
  else if (eventId == 7) face = 7;                    // <3
  else if (slime.stability > 0.70f && spaceRisk < 0.28f) face = 1; // ^_^
  else if (spaceSound > 0.45f) face = 6;              // >_>
  else if (spacePresence < 0.05f && spaceSound < 0.05f) face = 8;  // -_-
  else if (eventId == 8) face = 9;                    // *_*
  else face = 2;                                      // o_o

  slime.faceIndex = face % SLIME_SMILE_COUNT;
  slime.emotion = slime.faceIndex;
  slime.lastEventId = eventId;
  snprintf(slimeLine, sizeof(slimeLine), "%s %s | %s", slimeFace(), slimeMood(), spaceEventName(eventId));
  slimeRemember(eventId, intensity);
}

void spaceSetEvent(uint8_t eventId, float intensity, const char* label) {
  strlcpy(spaceEvent, label ? label : spaceEventName(eventId), sizeof(spaceEvent));
  spaceChooseSmile(eventId, intensity);
  eventLine = String("JGPT ") + slimeFace() + " " + spaceEvent;
}

void slimeLearnMicroModels() {
  float obsPresence = clipf(spacePresence, 0.0f, 1.0f);
  float obsMotion = clipf(spaceMotion, 0.0f, 1.0f);
  float obsSound = clipf(spaceSound, 0.0f, 1.0f);
  float obsAir = clipf(spaceAir, 0.0f, 1.0f);
  float obsRisk = clipf(obsMotion * 0.38f + obsAir * 0.32f + obsPresence * 0.18f + (1.0f - spaceConfidence) * 0.12f, 0.0f, 1.0f);

  float predErr = (fabsf(slime.predPresence - obsPresence) + fabsf(slime.predMotion - obsMotion) + fabsf(slime.predSound - obsSound) + fabsf(slime.predAir - obsAir)) * 0.25f;
  spaceNovelty = clipf(predErr * 1.65f + fabsf(obsRisk - slime.predRisk) * 0.55f, 0.0f, 1.0f);
  spaceRisk = obsRisk;
  spaceConfidence = clipf(mapMeanConfidence() * 0.65f + onlineNodeCount() * 0.055f + slime.stability * 0.20f, 0.0f, 1.0f);

  // Малые предикторы: экспоненциальная память поведения роя.
  slime.predPresence = slime.predPresence * 0.965f + obsPresence * 0.035f;
  slime.predMotion = slime.predMotion * 0.965f + obsMotion * 0.035f;
  slime.predSound = slime.predSound * 0.955f + obsSound * 0.045f;
  slime.predAir = slime.predAir * 0.985f + obsAir * 0.015f;
  slime.predRisk = slime.predRisk * 0.970f + obsRisk * 0.030f;

  // Доверие к сенсорам растёт, если они согласуются с общей картой, и падает при шуме.
  float consensus = clipf(1.0f - predErr, 0.0f, 1.0f);
  slime.trustEye = clipf(slime.trustEye * 0.997f + (consensus + obsPresence * 0.18f) * 0.003f, 0.05f, 1.65f);
  slime.trustMic = clipf(slime.trustMic * 0.997f + (consensus + obsSound * 0.16f) * 0.003f, 0.05f, 1.65f);
  slime.trustAir = clipf(slime.trustAir * 0.998f + (1.0f - fabsf(obsAir - slime.predAir)) * 0.002f, 0.05f, 1.65f);
  slime.trustRssi = clipf(slime.trustRssi * 0.998f + (float)onlineNodeCount() * 0.00045f, 0.05f, 1.65f);
  slime.trustSwarm = clipf(slime.trustSwarm * 0.997f + (consensus + obsMotion * 0.10f) * 0.003f, 0.05f, 1.65f);

  slime.noveltyAvg = slime.noveltyAvg * 0.96f + spaceNovelty * 0.04f;
  slime.dangerAvg = slime.dangerAvg * 0.97f + obsRisk * 0.03f;
  slime.comfortAvg = slime.comfortAvg * 0.985f + (1.0f - obsRisk) * 0.015f;
  slime.stability = clipf(slime.stability * 0.985f + (1.0f - predErr) * 0.015f, 0.0f, 1.0f);
  slime.curiosity = clipf(slime.curiosity * 0.990f + spaceNovelty * 0.018f, 0.05f, 1.6f);
  slime.mapConfidence = spaceConfidence;
  coreAiSkill = clipf(coreAiSkill * 0.985f + (spaceConfidence + slime.stability) * 0.0075f, 0.05f, 3.0f);

  uint8_t eventId = 1;
  float intensity = max(max(obsRisk, obsMotion), spaceNovelty);
  if (onlineNodeCount() == 0) eventId = 5;
  else if (obsAir > 0.85f) eventId = 4;
  else if (obsMotion > 0.72f) eventId = 3;
  else if (spaceNovelty > 0.62f) eventId = 2;
  else if (spaceConfidence > 0.58f && slime.stability > 0.62f) eventId = 6;
  else if ((slime.ticks % 160UL) == 0) eventId = 8;

  if (eventId != slime.lastEventId || intensity > 0.78f || (slime.ticks % 240UL) == 0) {
    spaceSetEvent(eventId, intensity, spaceEventName(eventId));
  }

  slime.lastPresence = obsPresence;
  slime.lastMotion = obsMotion;
  slime.lastSound = obsSound;
  slime.lastAir = obsAir;
}

void updateSwarmSpace() {
  // Compatibility name: PAGE_SPACE is now JANUS GALAXY STATION.
  // The galaxy lives even when hidden; mining remains gated by page == PAGE_SPACE.
  uint32_t now = millis();
  if (spaceLastTickMs == 0) spaceLastTickMs = now;
  uint32_t dtMs = now - spaceLastTickMs;
  if (dtMs > 600UL) dtMs = 600UL;
  spaceLastTickMs = now;

  galaxy.update(page == PAGE_SPACE);
  appendCore2UniverseArchive();
  slime.ticks++;
  coreAiSkill = clipf(coreAiSkill * 0.990f + (galaxy.s.adminSkill + galaxy.galaxyConfidence) * 0.005f, 0.05f, 5.0f);
  spaceSave(false);
}

void processRxFrames() {
  RxFrame f;
  while (popRxFrame(f)) {
    rxPackets++;

    if (f.len == sizeof(JanusColonyPacket)) {
      JanusColonyPacket pkt{};
      memcpy(&pkt, f.data, sizeof(pkt));
      if (memcmp(pkt.magic, "JANUS", 5) == 0) handleHeartbeat(pkt, f.rssi);
      continue;
    }

    if (f.len == sizeof(JobPacket) && f.data[0] == 'J' && f.data[1] == 'B') {
      JobPacket jp{};
      memcpy(&jp, f.data, sizeof(jp));
      handleJobPacket(jp);
      continue;
    }

    if (f.len == sizeof(JanusBuzzStatusPacket) && f.data[0] == 'B' && f.data[1] == 'S') {
      JanusBuzzStatusPacket bs{};
      memcpy(&bs, f.data, sizeof(bs));
      handleBuzzStatus(bs, f.rssi);
      continue;
    }

    if (f.len == sizeof(JanusPilotLinkPacket) && f.data[0] == 'P' && f.data[1] == 'L') {
      handlePilotLinkRaw(f.data, f.len, f.rssi);
      continue;
    }

    if (f.len == sizeof(ZimAgentMemoryPacket) && f.data[0] == 'Z' && f.data[1] == 'A') {
      handleZimAgentMemoryRaw(f.data, f.len, f.rssi, f.mac);
      continue;
    }

    if (f.len == sizeof(JanusEyeFramePacket) && f.data[0] == 'E' && f.data[1] == 'F') {
      handleJanusEyeFrameRaw(f.data, f.len, f.rssi);
      continue;
    }

    if (f.len == sizeof(JanusTachyonProphecyPacket) && f.data[0] == 'T' && f.data[1] == 'P') {
      handleJanusTachyonProphecyRaw(f.data, f.len, f.rssi, f.mac);
      continue;
    }

    if (f.len == sizeof(JanusKenshiPacket) && f.data[0] == 'K' && f.data[1] == '2') {
      handleJanusKenshiRaw(f.data, f.len, f.rssi, f.mac);
      continue;
    }

    if (f.len == sizeof(SwarmSensePacket) && f.data[0] == 'S' && f.data[1] == 'S') {
      handleSwarmSenseRaw(f.data, f.len, f.rssi, f.mac);
      continue;
    }

    if (f.len >= (int)(sizeof(JanusAudioFramePacket) - JANUS_AUDIO_FRAME_MAX_BYTES) && f.data[0] == 'A' && f.data[1] == 'F') {
      handleJanusAudioFrameRaw(f.data, f.len, f.rssi);
      continue;
    }

    if (f.len == sizeof(EntropyReportV2) && f.data[0] == 'E' && f.data[1] == '2') {
      EntropyReportV2 er2{};
      memcpy(&er2, f.data, sizeof(er2));
      handleEntropyV2(er2, f.rssi);
      continue;
    }

    if (f.len == sizeof(EntropyReport) && f.data[0] == 'E' && f.data[1] == 'R') {
      EntropyReport er{};
      memcpy(&er, f.data, sizeof(er));
      handleEntropyV1(er, f.rssi);
      continue;
    }
  }
}

// ========================= TX =========================

float homeEntropy() {
  float e = airEntropy + spaceNovelty * 3.0f + spaceMotion * 1.8f + spaceSound * 1.2f;
  if (eye.refresh()) e += clipf(eye.entropy * 0.08f + eye.loss * 0.8f, 0.0f, 4.0f);
  if (beacon.refresh()) e += clipf(beacon.entropy * 0.06f, 0.0f, 3.0f);
  if (audioNode.refresh()) e += clipf(audioNode.entropy * 0.07f, 0.0f, 3.0f);
  e += clipf(universalMeanEntropy() * 0.045f, 0.0f, 4.0f);
  return clipf(e, 0.01f, 20.0f);
}

float homeSync() {
  float s = 0.20f + spaceConfidence * 0.45f + slime.stability * 0.25f;
  int c = 1;
  if (eye.refresh()) { s += eye.sync; c++; }
  if (beacon.refresh()) { s += beacon.sync; c++; }
  if (audioNode.refresh()) { s += audioNode.sync; c++; }
  float us = universalMeanSync();
  if (us > 0.0f) { s += us; c++; }
  return clipf(s / (float)c, 0.0f, 1.5f);
}

void sendCore2Heartbeat() {
  if (!espnowOk) return;
  ensureColonyPeer();

  JanusColonyPacket pkt{};
  memcpy(pkt.magic, "JANUS", 6);
  strlcpy(pkt.nodeId, "Core2Home", sizeof(pkt.nodeId));
  strlcpy(pkt.role, "Core2Galaxy", sizeof(pkt.role));
  pkt.seq = ++colonySeq;
  pkt.hashRate = coreRemoteHashrate;
  pkt.shares = coreSharesSent;
  pkt.rejects = rxDropped;
  pkt.bestBits = coreBestBits;
  pkt.diff = spaceRisk;
  pkt.targetBits = coreTargetBits;
  pkt.aiBatch = (uint16_t)clipf(80.0f + coreAiSkill * 90.0f + homeEntropy() * 6.0f, 80.0f, 700.0f);
  pkt.aiHint = slime.emotion;
  pkt.jobAgeMs = millis() - spaceLastTickMs;
  pkt.rssi = wifiOk ? WiFi.RSSI() : -127;
  pkt.uptime = millis() / 1000;
  esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
}

void sendCore2Entropy() {
  if (!espnowOk) return;
  ensureColonyPeer();

  EntropyReportV2 er2{};
  er2.magic[0] = 'E';
  er2.magic[1] = '2';
  er2.worker_id = colonyWorkerId;
  strlcpy(er2.nodeId, "Core2Galaxy", sizeof(er2.nodeId));
  er2.local_entropy = homeEntropy();
  er2.prediction_error = spaceNovelty;
  er2.sync_hint = homeSync();
  er2.fit = spaceConfidence;
  er2.sensor_flags = 0x7B; // env + galaxy sim + JGPT admin brain
  er2.values[0] = (float)eco2;
  er2.values[1] = (float)tvoc;
  er2.values[2] = spacePresence;
  er2.values[3] = spaceMotion;
  er2.values[4] = spaceSound;
  er2.values[5] = spaceAir;
  er2.values[6] = spaceRisk;
  er2.values[7] = (float)(wifiOk ? WiFi.RSSI() : -127);
  er2.uptime_ms = millis();
  esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&er2, sizeof(er2));
}


void sendCore2SwarmSense() {
#if CORE2_SWARMSENSE_OBSERVE
  if (!espnowOk) return;
  ensureColonyPeer();

  uint32_t now = millis();
  uint16_t dynamicBatch = (uint16_t)clipf(80.0f + coreAiSkill * 90.0f + homeEntropy() * 6.0f, 80.0f, 700.0f);
  uint16_t effectiveBatch = coreWorkerEnabled() ? min((uint16_t)CORE2_MINER_MAX_BATCH, dynamicBatch) : 0;
  uint8_t thermal = (uint8_t)clipf(spaceRisk * 42.0f + airScore * 6.0f + (coreWorkerEnabled() ? 14.0f : 0.0f) + core2LoopJitterUs / 950.0f, 0.0f, 100.0f);
  uint16_t entropy = (uint16_t)clipf(homeEntropy() * 1000.0f, 0.0f, 65535.0f);
  uint16_t hashEff = 0;
  if (dynamicBatch > 0) hashEff = (uint16_t)clipf(((float)coreRemoteHashrate / (float)dynamicBatch) * 1000.0f, 0.0f, 65535.0f);
  uint16_t touchDelta = core2TouchCounter - core2LastTouchCounterSent;
  core2LastTouchCounterSent = core2TouchCounter;

  uint16_t nonceLeft = 0;
  if (coreJob.active && coreJob.rangeSize >= coreJob.thetaCursor) nonceLeft = (uint16_t)((coreJob.rangeSize - coreJob.thetaCursor) & 0xFFFF);

  SwarmSensePacket ss{};
  ss.magic[0] = 'S';
  ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = colonyWorkerId;
  strlcpy(ss.nodeId, "Core2Home", sizeof(ss.nodeId));
  strlcpy(ss.kind, "core2_station", sizeof(ss.kind));
  ss.seq = ++core2SwarmSenseSeq;
  ss.uptime_ms = now;
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = core2LoopJitterUs;
  ss.loop_max_us = core2LoopMaxUs;
  ss.rssi = wifiOk ? WiFi.RSSI() : -127;
  ss.radio_mode = 1;
  ss.bt_flags = 0;
  ss.palette = (uint8_t)page;
  ss.knn_label = core2SwarmSenseLabel(thermal, core2LoopJitterUs, ss.rssi);
  ss.knn_confidence = (uint8_t)clipf(homeSync() * 72.0f + spaceConfidence * 28.0f, 0.0f, 100.0f);
  ss.ai_hint = slime.emotion;
  ss.thermal_load = thermal;
  ss.effective_batch = effectiveBatch;
  ss.dynamic_batch = dynamicBatch;
  ss.hash_rate = coreRemoteHashrate;
  ss.total_hashes = coreHashCounter;
  ss.best_bits = coreBestBits;
  ss.hash_eff_x1000 = hashEff;
  ss.prediction_error_x1000 = (int16_t)clipf((spaceNovelty + coreTheta.resonance * 0.08f) * 1000.0f, -32768.0f, 32767.0f);
  ss.entropy_x1000 = (uint16_t)clipf((float)entropy + coreTheta.mock * 180.0f + coreTheta.resonance * 120.0f, 0.0f, 65535.0f);
  ss.touch_delta = touchDelta;
  ss.job_age_s = coreLastJobMs ? (uint16_t)min(65535UL, (now - coreLastJobMs) / 1000UL) : 0;
  ss.nonce_remaining_l16 = nonceLeft;
  ss.flags = 0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0040 | 0x0080; // rf clock thermal air touch display battery
  if (coreWorkerEnabled()) ss.flags |= 0x0020;

  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&ss, sizeof(ss));
  if (err == ESP_OK) {
    core2SwarmSenseTx++;
    if (core2SwarmSenseTx <= 3 || (core2SwarmSenseTx % 20UL) == 0) {
      Serial.printf("[SWARMSENSE] Core2 TX seq=%lu H=%lu batch=%u rssi=%d\n", (unsigned long)ss.seq, (unsigned long)ss.hash_rate, (unsigned)ss.dynamic_batch, ss.rssi);
    }
  } else {
    core2SwarmSenseTxFail++;
    core2SwarmSenseBad++;
    Serial.printf("[SWARMSENSE] Core2 TX FAIL err=%d tx=%lu fail=%lu\n", (int)err, (unsigned long)core2SwarmSenseTx, (unsigned long)core2SwarmSenseTxFail);
  }
#endif
}


void handlePilotLinkRaw(const uint8_t* data, int len, int8_t rxRssi) {
  if (!data || len != (int)sizeof(JanusPilotLinkPacket)) return;
  JanusPilotLinkPacket pl{};
  memcpy(&pl, data, sizeof(pl));
  if (pl.magic[0] != 'P' || pl.magic[1] != 'L' || pl.version != 1) return;
  core2PilotLinkRx++;
  core2LastPilotLinkMs = millis();
  galaxy.universePilotSector = pl.sector % JanusGalaxyStationSim::UNIVERSE_SECTORS;
  galaxy.universeSelectedSector = galaxy.universePilotSector;
  galaxy.universePilotDistance = pl.distance;
  galaxy.universePartyPower = clipf(galaxy.universePartyPower * 0.82f + clipf((float)pl.shield_x10 / 1000.0f + (float)pl.energy_x10 / 1200.0f + (float)pl.mech_level * 0.08f, 0.0f, 1.8f) * 0.18f, 0.0f, 2.0f);
  snprintf(galaxy.universePilotLine, sizeof(galaxy.universePilotLine),
           "PilotLink: %s S%02u sys%u mode%u mech%u armor%u rssi%d",
           pl.nodeId, (unsigned)galaxy.universePilotSector, (unsigned)pl.system,
           (unsigned)pl.mode, (unsigned)pl.mech_level, (unsigned)pl.mech_armor, (int)rxRssi);
  if ((core2PilotLinkRx <= 3) || (core2PilotLinkRx % 20UL == 0)) {
    Serial.printf("[PILOTLINK] RX %s sector=%u sys=%u mode=%u mech=%u armor=%u rssi=%d\n",
                  pl.nodeId, (unsigned)pl.sector, (unsigned)pl.system, (unsigned)pl.mode,
                  (unsigned)pl.mech_level, (unsigned)pl.mech_armor, (int)rxRssi);
  }
}

void sendCore2GroundOrderTick() {
  if (!espnowOk) return;
  uint32_t now = millis();

  // v6.27: orders are strategic pulses, not UI spam. Stick receives them as live contracts.
  if (now - core2LastGroundOrderMs < 22000UL) return;
  core2LastGroundOrderMs = now;
  ensureColonyPeer();

  uint8_t sector = galaxy.universeServiceSector % JanusGalaxyStationSim::UNIVERSE_SECTORS;
  float threat = galaxy.universeThreat[sector];
  float supply = galaxy.universeSupply[sector];
  float influence = galaxy.universeInfluence[sector];
  bool pilotOnline = core2LastPilotLinkMs && (now - core2LastPilotLinkMs < 26000UL);
  bool baseCrisis = (threat > 0.66f || galaxy.universeOwner[sector] != 1 || galaxy.universeStationLevel[sector] == 0);
  bool raidWindow = pilotOnline && !baseCrisis && ((core2GroundOrderSeq % 3UL) != 1UL || supply > 0.50f);

  GroundOrderPacket go{};
  go.magic[0] = 'G'; go.magic[1] = 'O';
  go.version = 1;
  go.mode = raidWindow ? 1 : 0;
  go.sector = sector;
  go.priority = (uint8_t)clipf(70.0f + threat * 105.0f + supply * 26.0f + influence * 12.0f + (page == PAGE_SPACE ? 18.0f : 0.0f), 0.0f, 255.0f);
  go.flags = 0;
  if (page == PAGE_SPACE) go.flags |= 0x0001;
  if (pilotOnline) go.flags |= 0x0002;
  if (threat > influence) go.flags |= 0x0004;
  if (raidWindow) go.flags |= 0x0008;
  go.mission_id = (++core2GroundOrderSeq) ^ ((uint32_t)sector << 24) ^ (now & 0x00FFFFFFUL);
  snprintf(go.target, sizeof(go.target), "%s", raidWindow ? "HERO_RAID" : "BASE_HOLD");

  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&go, sizeof(go));
  if (err == ESP_OK) {
    Serial.printf("[GROUNDOPS] ORDER TX mode=%u sector=%u prio=%u mission=%lu target=%s pilot=%u crisis=%u\n",
                  (unsigned)go.mode, (unsigned)go.sector, (unsigned)go.priority,
                  (unsigned long)go.mission_id, go.target, (unsigned)pilotOnline, (unsigned)baseCrisis);
  } else {
    Serial.printf("[GROUNDOPS] ORDER TX FAIL err=%d\n", (int)err);
  }
}


const char* core2ZimMissionTypeName(uint8_t t) {
  switch (t % 5) {
    case 0: return "RECON";
    case 1: return "SAMPLE";
    case 2: return "RAID";
    case 3: return "SALVAGE";
    default: return "HOLD";
  }
}

void sendCore2ZimMissionTick() {
  if (!espnowOk) return;
  uint32_t now = millis();

  // Zim gets lore-like imperial orders: not nonce jobs, only planet-side solo missions.
  if (now - core2LastZimMissionMs < 18000UL) return;
  core2LastZimMissionMs = now;
  ensureColonyPeer();

  uint8_t sector = galaxy.universeServiceSector % JanusGalaxyStationSim::UNIVERSE_SECTORS;
  float threat = galaxy.universeThreat[sector];
  float supply = galaxy.universeSupply[sector];
  float influence = galaxy.universeInfluence[sector];
  bool crisis = (threat > 0.66f || galaxy.universeOwner[sector] != 1 || galaxy.universeStationLevel[sector] == 0);

  uint8_t mtype = 0;
  if (crisis) mtype = 2;                    // raid / hostile sector
  else if (supply < 0.32f) mtype = 3;       // salvage resources
  else if (influence < 0.48f) mtype = 1;    // collect samples / local intel
  else if ((core2ZimMissionSeq % 5UL) == 4) mtype = 4;
  else mtype = 0;

  ZimMissionPacket zm{};
  zm.magic[0] = 'Z'; zm.magic[1] = 'M';
  zm.version = 1;
  zm.sector = sector;
  zm.missionType = mtype;
  zm.priority = (uint8_t)clipf(60.0f + threat * 120.0f + supply * 24.0f + (page == PAGE_SPACE ? 20.0f : 0.0f), 0.0f, 255.0f);
  zm.floorMax = (uint8_t)clipf(2.0f + threat * 3.0f + (float)(core2ZimMissionSeq % 2UL), 2.0f, 5.0f);
  zm.fuel = (uint16_t)clipf(16.0f + supply * 14.0f + influence * 10.0f + (float)zm.floorMax * 3.0f, 14.0f, 48.0f);
  zm.difficulty_x100 = (uint16_t)clipf(100.0f + threat * 180.0f + (float)galaxy.universeStationLevel[sector] * 22.0f, 100.0f, 999.0f);
  zm.mission_id = (++core2ZimMissionSeq) ^ ((uint32_t)sector << 24) ^ (now & 0x00FFFFFFUL);
  zm.seed = esp_random() ^ zm.mission_id ^ ((uint32_t)sector << 16) ^ (uint32_t)galaxy.s.ticks;
  zm.flags = 0x00000008UL;                  // Zim Earth order bit
  if (page == PAGE_SPACE) zm.flags |= 0x00000001UL;
  if (crisis) zm.flags |= 0x00000002UL;
  if (mtype == 2) zm.flags |= 0x00000004UL;
  snprintf(zm.target, sizeof(zm.target), "ZimGeek");
  snprintf(zm.planet, sizeof(zm.planet), "%s", galaxy.universePlanetName(sector));
  snprintf(zm.order, sizeof(zm.order), "%s %s F%u", core2ZimMissionTypeName(mtype), zm.planet, (unsigned)zm.floorMax);

  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&zm, sizeof(zm));
  if (err == ESP_OK) {
    core2ZimMissionTx++;
    snprintf(core2ZimLastOrder, sizeof(core2ZimLastOrder), "Zim -> S%02u %s %s", (unsigned)sector, zm.planet, core2ZimMissionTypeName(mtype));
    snprintf(galaxy.universePilotLine, sizeof(galaxy.universePilotLine), "Zim: ordered S%02u %s / %s", (unsigned)sector, zm.planet, core2ZimMissionTypeName(mtype));
    Serial.printf("[ZIMCTRL] ZM TX #%lu sector=%u type=%s prio=%u floors=%u fuel=%u mission=%lu planet=%s\n",
                  (unsigned long)core2ZimMissionTx, (unsigned)zm.sector, core2ZimMissionTypeName(zm.missionType),
                  (unsigned)zm.priority, (unsigned)zm.floorMax, (unsigned)zm.fuel,
                  (unsigned long)zm.mission_id, zm.planet);
  } else {
    core2ZimMissionFail++;
    Serial.printf("[ZIMCTRL] ZM TX FAIL err=%d fail=%lu\n", (int)err, (unsigned long)core2ZimMissionFail);
  }
}

// ========================= UI COLORS / DRAW PRIMITIVES =========================

uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

uint16_t dimColor(uint16_t c, float k) {
  k = clipf(k, 0.0f, 1.0f);
  uint8_t r5 = (c >> 11) & 0x1F;
  uint8_t g6 = (c >> 5) & 0x3F;
  uint8_t b5 = c & 0x1F;
  uint8_t r = (uint8_t)clipf((float)((r5 << 3) | (r5 >> 2)) * k, 0.0f, 255.0f);
  uint8_t g = (uint8_t)clipf((float)((g6 << 2) | (g6 >> 4)) * k, 0.0f, 255.0f);
  uint8_t b = (uint8_t)clipf((float)((b5 << 3) | (b5 >> 2)) * k, 0.0f, 255.0f);
  return rgb565(r, g, b);
}

uint16_t colorBg()      { return rgb565(4, 5, 10); }
uint16_t colorPanel()   { return rgb565(12, 15, 28); }
uint16_t colorPanel2()  { return rgb565(20, 24, 42); }
uint16_t colorText()    { return 0xFFFF; }
uint16_t colorDim()     { return rgb565(110, 125, 145); }
uint16_t colorGood()    { return rgb565(60, 220, 145); }
uint16_t colorWarn()    { return rgb565(255, 190, 70); }
uint16_t colorBad()     { return rgb565(255, 80, 80); }
uint16_t colorCyan()    { return rgb565(70, 205, 255); }
uint16_t colorPurple()  { return rgb565(190, 110, 255); }
uint16_t colorBlue()    { return rgb565(90, 140, 255); }

uint16_t airColor() {
  if (tvoc > 600 || eco2 > 1500) return colorBad();
  if (tvoc > 220 || eco2 > 900) return colorWarn();
  return colorGood();
}

uint8_t airLevel() {
  if (!sgpReady) return 0;             // sensor missing / warming
  if (tvoc > 600 || eco2 > 1500) return 3;  // bad
  if (tvoc > 220 || eco2 > 900) return 2;   // watch
  return 1;                            // good
}

const char* airLevelTitle() {
  switch (airLevel()) {
    case 1: return "GOOD / dyshat legko";
    case 2: return "WATCH / luchshe provetrit";
    case 3: return "VENT NOW / nuzhen svezhiy vozduh";
    default: return "WARMUP / datchik greetsya";
  }
}

const char* airLevelAdvice() {
  switch (airLevel()) {
    case 1: return "OK: vozduh spokoynyy, okno ne obyazatelno.";
    case 2: return "Sovet: 5-10 min provetrit, osobenno esli est zapah.";
    case 3: return "Deystvie: otkryt okno/dver, ubrat istochnik zapaha.";
    default: return "Pervye 1-2 min smotret ostorozhno: SGP30 razogrev.";
  }
}

const char* eco2Explain() {
  if (!sgpReady) return "eCO2 poka net: SGP30 ne gotov.";
  if (eco2 > 1500) return "eCO2 vysokiy: vozduh vydyhan, mozhno byt dushno.";
  if (eco2 > 900) return "eCO2 rastet: lyudi/dyhanie nakaplivayutsya.";
  return "eCO2 norma: po ocenke sensoru vozduh svezhiy.";
}

const char* tvocExplain() {
  if (!sgpReady) return "TVOC poka net: proveryaem datchik.";
  if (tvoc > 600) return "TVOC vysokiy: duhi/himiya/eda/dym ili chistka.";
  if (tvoc > 220) return "TVOC zameten: est zapah ili letuchie veshchestva.";
  return "TVOC nizkiy: malo zapahov i letuchih veshchestv.";
}

const char* trendExplain() {
  if (airTrend > 0.35f) return "Trend +: kachestvo vozduha uhudshaetsya.";
  if (airTrend < -0.35f) return "Trend -: vozduh uluchshaetsya posle provetrivaniya.";
  return "Trend 0: pokazaniya pochti stabilny.";
}

uint16_t airBarColorForValue(uint16_t value, uint16_t warn, uint16_t bad) {
  if (value >= bad) return colorBad();
  if (value >= warn) return colorWarn();
  return colorGood();
}

uint16_t pageAccent() {
  switch (page) {
    case PAGE_AIR: return airColor();
    case PAGE_EYE: return colorCyan();
    case PAGE_BEACON: return colorGood();
    case PAGE_BUZZ: return colorWarn();
    case PAGE_SPACE: return colorCyan();
    case PAGE_AUDIO: return colorPurple();
    case PAGE_MESH: return colorBlue();
    case PAGE_RSSI: return colorCyan();
    case PAGE_SYSTEM: return colorDim();
    case PAGE_WEATHER: return colorBlue();
    default: return colorPurple();
  }
}

void cPrint(int x, int y, uint16_t fg, uint16_t bg, const String& s, uint8_t size = 1) {
  canvas.setTextSize(size);
  canvas.setTextColor(fg, bg);
  canvas.setCursor(x, y);
  canvas.print(s);
}

void cPrintf(int x, int y, uint16_t fg, uint16_t bg, uint8_t size, const char* fmt, ...) {
  char b[128];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof(b), fmt, ap);
  va_end(ap);
  cPrint(x, y, fg, bg, String(b), size);
}

void drawButton(int x, int y, int w, int h, const char* label, uint16_t accent, bool active = true) {
  uint16_t fill = active ? rgb565(18, 21, 36) : rgb565(10, 11, 18);
  uint16_t edge = active ? accent : colorDim();
  canvas.fillRoundRect(x, y, w, h, 8, fill);
  canvas.drawRoundRect(x, y, w, h, 8, edge);
  int tx = x + 7;
  int ty = y + (h - 8) / 2;
  cPrint(tx, ty, active ? edge : colorDim(), fill, label, 1);
}

void drawBackHeader(const char* title, const char* sub, uint16_t accent) {
  canvas.fillRect(0, 0, 320, 30, rgb565(2, 3, 8));
  canvas.drawFastHLine(0, 29, 320, accent);

  drawButton(6, 4, 54, 21, "HOME", accent, true);
  cPrint(68, 6, colorText(), rgb565(2,3,8), title, 1);
  cPrint(68, 18, colorDim(), rgb565(2,3,8), sub, 1);

  cPrintf(244, 6, wifiOk ? colorGood() : colorBad(), rgb565(2,3,8), 1, "W:%s", wifiOk ? "OK" : "--");
  cPrintf(280, 6, espnowOk ? colorGood() : colorBad(), rgb565(2,3,8), 1, "N:%s", espnowOk ? "OK" : "--");
  cPrintf(244, 18, colorDim(), rgb565(2,3,8), 1, "ch:%u", colonyPeerChannel);
}

void drawHomeHeader() {
  canvas.fillRect(0, 0, 320, 30, rgb565(2, 3, 8));
  canvas.drawFastHLine(0, 29, 320, colorPurple());
  universalRecountNodes();
  cPrintf(8, 5, colorText(), rgb565(2,3,8), 1, "JANUS CORE2 v6.35  RX:%lu E2:%lu N:%u/%u",
          (unsigned long)rxPackets, (unsigned long)er2Packets, (unsigned)colonyOnlineCount, (unsigned)colonyKnownCount);
  cPrintf(8, 18, colorDim(), rgb565(2,3,8), 1, "tap cards  bat:%d%% br:%u%% drop:%lu", M5.Power.getBatteryLevel(), (unsigned)((coreBrightness * 100) / 255), (unsigned long)rxDropped);
  cPrintf(240, 18, wifiOk ? colorGood() : colorBad(), rgb565(2,3,8), 1, "W:%s", wifiOk ? "OK" : "--");
  cPrintf(278, 18, espnowOk ? colorGood() : colorBad(), rgb565(2,3,8), 1, "N:%s", espnowOk ? "OK" : "--");
}

void drawAirPanelHome() {
  canvas.fillRoundRect(8, 38, 304, 76, 12, colorPanel2());
  canvas.drawRoundRect(8, 38, 304, 76, 12, airColor());

  cPrint(18, 46, colorDim(), colorPanel2(), "LOCAL AIR / SGP30  tap", 1);

  canvas.setTextSize(3);
  canvas.setTextColor(airColor(), colorPanel2());
  canvas.setCursor(18, 64);
  canvas.printf("%u", eco2);
  cPrint(92, 80, airColor(), colorPanel2(), "eCO2 ppm", 1);

  canvas.setTextSize(3);
  canvas.setTextColor(airColor(), colorPanel2());
  canvas.setCursor(168, 64);
  canvas.printf("%u", tvoc);
  cPrint(234, 80, airColor(), colorPanel2(), "TVOC ppb", 1);

  int barW = constrain((int)(airScore * 28.0f), 0, 284);
  canvas.fillRoundRect(18, 100, 284, 6, 3, rgb565(30, 35, 50));
  if (barW > 0) canvas.fillRoundRect(18, 100, barW, 6, 3, airColor());
  if (weatherReady) cPrintf(18, 108, colorDim(), colorPanel2(), 1, "ZP %.1fC feels %.1f wind %.0f", wxTemp, wxFeels, wxWind);
  else cPrint(18, 108, colorDim(), colorPanel2(), "ZP weather: tap AIR", 1);
}

void drawCard(int x, int y, int w, int h, const char* title, bool online, uint16_t accent) {
  canvas.fillRoundRect(x, y, w, h, 8, colorPanel());
  canvas.drawRoundRect(x, y, w, h, 8, online ? accent : colorDim());
  cPrintf(x + 8, y + 6, online ? accent : colorDim(), colorPanel(), 1, "%s %s", title, online ? "ON" : "OFF");
}

void drawNodeGridHome() {
  bool eyeOn = eye.refresh();
  bool beaconOn = beacon.refresh();
  bool buzzOn = buzz.refresh();
  bool audOn = audioNode.refresh();
  bool swarmOn = swarm.refresh();
  bool stickOn = stick.refresh();

  drawCard(8, 122, 148, 48, "BLIND EYE", eyeOn, colorCyan());
  cPrintf(16, 142, colorText(), colorPanel(), 1, "TM %.0f/%.0f  A %.1f", eye.v1, eye.v2, eye.v5);
  cPrintf(16, 155, colorText(), colorPanel(), 1, "MAG %.0f  L %.3f", eye.v3, eye.loss);

  drawCard(164, 122, 148, 48, "ADV BEACON", beaconOn, colorGood());
  cPrintf(172, 142, colorText(), colorPanel(), 1, "T %.1f H %.0f", beacon.v0, beacon.v1);
  cPrintf(172, 155, colorText(), colorPanel(), 1, "E %.2f M2R %.1f", beacon.v2, beacon.v3);

  drawCard(8, 178, 72, 48, "BUZZ", buzzOn, colorWarn());
  cPrintf(16, 198, colorText(), colorPanel(), 1, "H %s", compactU(buzz.hashRate).c_str());
  cPrintf(16, 211, colorText(), colorPanel(), 1, "B%lu S%lu", (unsigned long)buzz.bestBits, (unsigned long)buzz.shares);

  drawCard(86, 178, 72, 48, "GALAXY", onlineNodeCount() > 0, colorCyan());
  cPrintf(94, 198, colorText(), colorPanel(), 1, "L%u %.0fcr", galaxy.s.stationLevel, galaxy.s.credits);
  cPrintf(94, 211, colorText(), colorPanel(), 1, "AI %.1f R%.1f", galaxy.s.adminSkill, galaxy.s.reputation);

  drawCard(164, 178, 72, 48, "AUDIO", audOn, colorPurple());
  cPrintf(172, 198, colorText(), colorPanel(), 1, "MIC %.0f", audioNode.v0);
  cPrintf(172, 211, colorText(), colorPanel(), 1, "E %.2f", audioNode.entropy);

  universalRecountNodes();
  drawCard(242, 178, 70, 48, "MESH", colonyOnlineCount > 0, colorBlue());
  cPrintf(250, 198, colorText(), colorPanel(), 1, "N %u/%u", (unsigned)colonyOnlineCount, (unsigned)colonyKnownCount);
  cPrintf(250, 211, colorText(), colorPanel(), 1, "NEW %lu", (unsigned long)colonyNewNodeEvents);
}

void drawFooter() {
  canvas.fillRect(0, 229, 320, 11, rgb565(2, 3, 8));
  cPrintf(8, 230, pageAccent(), rgb565(2,3,8), 1, "%s | N %u/%u E %.2f sync %.2f | H %lu best %lu",
          eventLine.c_str(), (unsigned)colonyOnlineCount, (unsigned)colonyKnownCount,
          homeEntropy(), homeSync(), (unsigned long)coreRemoteHashrate, (unsigned long)coreBestBits);
}

void drawHomePage() {
  drawHomeHeader();
  drawAirPanelHome();
  drawNodeGridHome();
  drawFooter();
}

void drawAirDetail() {
  uint16_t accent = airColor();
  drawBackHeader("AIR / SGP30", "domashniy vozduh: status + ponyatnoe obyasnenie", accent);

  // Big friendly status card for home use.
  canvas.fillRoundRect(10, 34, 300, 47, 10, colorPanel2());
  canvas.drawRoundRect(10, 34, 300, 47, 10, accent);
  cPrint(20, 42, colorDim(), colorPanel2(), "AIR STATUS", 1);
  cPrintf(20, 56, accent, colorPanel2(), 2, "%s", airLevelTitle());
  cPrintf(20, 72, colorText(), colorPanel2(), 1, "%s", airLevelAdvice());

  // Two main values: eCO2 and TVOC.
  canvas.fillRoundRect(10, 84, 145, 48, 10, colorPanel2());
  canvas.drawRoundRect(10, 84, 145, 48, 10, airBarColorForValue(eco2, 900, 1500));
  cPrint(20, 90, colorDim(), colorPanel2(), "eCO2 ppm", 1);
  canvas.setTextSize(3);
  canvas.setTextColor(airBarColorForValue(eco2, 900, 1500), colorPanel2());
  canvas.setCursor(20, 103);
  canvas.printf("%u", eco2);
  cPrint(88, 111, colorDim(), colorPanel2(), "dyhanie", 1);

  canvas.fillRoundRect(165, 84, 145, 48, 10, colorPanel2());
  canvas.drawRoundRect(165, 84, 145, 48, 10, airBarColorForValue(tvoc, 220, 600));
  cPrint(175, 90, colorDim(), colorPanel2(), "TVOC ppb", 1);
  canvas.setTextSize(3);
  canvas.setTextColor(airBarColorForValue(tvoc, 220, 600), colorPanel2());
  canvas.setCursor(175, 103);
  canvas.printf("%u", tvoc);
  cPrint(244, 111, colorDim(), colorPanel2(), "zapahi", 1);

  int y = 138;
  cPrintf(18, y, colorText(), colorBg(), 1, "%s", eco2Explain()); y += 12;
  cPrintf(18, y, colorText(), colorBg(), 1, "%s", tvocExplain()); y += 12;
  cPrintf(18, y, colorText(), colorBg(), 1, "%s", trendExplain()); y += 12;
  cPrintf(18, y, colorDim(), colorBg(), 1, "SGP30: eCO2 ne real CO2, eto ocenki po VOC/zapaham."); y += 12;
  cPrintf(18, y, colorDim(), colorBg(), 1, "raw H2:%u  Ethanol:%u  score:%.2f", rawH2, rawEthanol, airScore); y += 12;
  cPrintf(18, y, colorDim(), colorBg(), 1, "porogi: OK <900ppm/<220ppb, vent >1500/>600");

  drawButton(18, 207, 120, 20, "WEATHER", accent, true);
  drawButton(150, 207, 142, 20, sgpReady ? "SGP30 OK" : "SGP30 WARM", sgpReady ? colorGood() : colorWarn(), true);

  drawFooter();
}

uint16_t eyeHeatColor(uint8_t v) {
  // v6.33: monochrome TMOS sight. No heatmap colors, no fake camera palette.
  uint8_t g;
  if (v < 10) g = 2;
  else if (v < 35) g = 8;
  else if (v < 75) g = 22;
  else if (v < 120) g = 48;
  else if (v < 170) g = 86;
  else if (v < 220) g = 138;
  else g = 210;
  return rgb565(g, g, g);
}

void drawEyeVisionHeatmap(int x, int y, int w, int h) {
  janusEyeVisionSynthesizeFromEye(false);
  canvas.fillRoundRect(x, y, w, h, 10, rgb565(2, 3, 7));
  canvas.drawRoundRect(x, y, w, h, 10, eye.refresh() ? colorCyan() : colorDim());
  cPrint(x + 8, y + 6, colorDim(), rgb565(2,3,7), "TMOS/PIR FIELD", 1);

  int gridX = x + 8;
  int gridY = y + 20;
  int gridW = w - 16;
  int gridH = h - 33;
  int cw = gridW / JANUS_EYE_VISION_W;
  int ch = gridH / JANUS_EYE_VISION_H;
  if (cw < 4) cw = 4;
  if (ch < 4) ch = 4;

  // Lens aperture background: this is what a single-zone TMOS sensor can honestly show.
  canvas.fillRect(gridX, gridY, JANUS_EYE_VISION_W * cw, JANUS_EYE_VISION_H * ch, rgb565(0,0,0));
  for (int yy = 0; yy < JANUS_EYE_VISION_H; ++yy) {
    for (int xx = 0; xx < JANUS_EYE_VISION_W; ++xx) {
      float fx = ((float)xx - 3.5f) / 3.5f;
      float fy = ((float)yy - 3.5f) / 3.5f;
      bool inside = (fx * fx + fy * fy * 0.82f) <= 1.08f;
      uint8_t v = inside ? janusEyeVisionPixels[yy * JANUS_EYE_VISION_W + xx] : 0;
      uint16_t c = eyeHeatColor(v);
      int px = gridX + xx * cw;
      int py = gridY + yy * ch;
      canvas.fillRect(px, py, cw - 1, ch - 1, c);
    }
  }

  // Aperture outline only, no reticle/aim/scanline.
  int ox = gridX + (JANUS_EYE_VISION_W * cw) / 2;
  int oy = gridY + (JANUS_EYE_VISION_H * ch) / 2;
  int rr = min(JANUS_EYE_VISION_W * cw, JANUS_EYE_VISION_H * ch) / 2 - 2;
  canvas.drawEllipse(ox, oy, rr, (int)(rr * 0.82f), rgb565(58, 58, 68));
  canvas.drawEllipse(ox, oy, rr - 1, (int)(rr * 0.82f) - 1, rgb565(18, 18, 24));

  const char* src = (janusEyeVisionLastRealFrameMs && millis() - janusEyeVisionLastRealFrameMs < JANUS_EYE_VISION_IDLE_MS) ? "REAL EF" : "REAL E2 FIELD";
  cPrintf(x + 8, y + h - 11, colorDim(), rgb565(2,3,7), 1, "%s rx:%lu field:%lu", src, (unsigned long)janusEyeVisionFramesRx, (unsigned long)janusEyeVisionSynthFrames);
}

void drawEyeDetail() {
  bool on = eye.refresh();
  drawBackHeader("BLIND EYE", "real TMOS/PIR field + metrics", colorCyan());

  drawEyeVisionHeatmap(10, 36, 188, 134);

  canvas.fillRoundRect(206, 36, 104, 134, 10, colorPanel2());
  canvas.drawRoundRect(206, 36, 104, 134, 10, on ? colorCyan() : colorDim());
  cPrintf(214, 44, on ? colorCyan() : colorDim(), colorPanel2(), 1, "%s RSSI:%d", on ? "ONLINE" : "OFFLINE", eye.rssi);
  cPrintf(214, 57, colorDim(), colorPanel2(), 1, "age:%lus", (unsigned long)(eye.age() / 1000UL));
  canvas.setTextSize(2);
  canvas.setTextColor(colorCyan(), colorPanel2());
  canvas.setCursor(214, 75);
  canvas.printf("P%.0f", eye.v1);
  canvas.setCursor(214, 96);
  canvas.printf("M%.0f", eye.v2);
  cPrintf(214, 121, colorText(), colorPanel2(), 1, "MAG %.0f", eye.v3);
  cPrintf(214, 134, colorText(), colorPanel2(), 1, "ACT %.0f", eye.v5);
  cPrintf(214, 147, colorText(), colorPanel2(), 1, "LOSS %.3f", eye.loss);
  cPrintf(214, 160, colorDim(), colorPanel2(), 1, "EC:%s", janusEyeVisionSentState ? "ON" : "--");

  canvas.fillRoundRect(10, 176, 300, 28, 8, colorPanel());
  canvas.drawRoundRect(10, 176, 300, 28, 8, colorCyan());
  cPrintf(18, 184, colorText(), colorPanel(), 1, "%s", janusEyeVisionStatusLine);
  cPrintf(18, 196, colorDim(), colorPanel(), 1, "HOME stops EC; outside page only metrics, no vision load");

  drawButton(18, 210, 106, 22, janusEyeVisionUserEnabled ? "EYE ON" : "EYE OFF", colorCyan(), true);
  drawButton(134, 210, 86, 22, "MESH", colorBlue(), true);
  drawButton(230, 210, 72, 22, "HOME", colorCyan(), true);

  drawFooter();
}

void drawBeaconDetail() {
  bool on = beacon.refresh();
  drawBackHeader("ADV BEACON", "Cardputer polevoy hub", colorGood());

  int y = 44;
  cPrintf(18, y, on ? colorGood() : colorDim(), colorBg(), 1, "online:%s age:%lu ms RSSI:%d", on ? "yes" : "no", (unsigned long)beacon.age(), beacon.rssi); y += 18;
  cPrintf(18, y, colorText(), colorBg(), 1, "temperature / temperatura: %.2f C", beacon.v0); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "humidity / vlazhnost: %.1f %%", beacon.v1); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "entropy / entropiya: %.3f", beacon.entropy > 0 ? beacon.entropy : beacon.v2); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "M2R: %.3f", beacon.v3); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "loss / oshibka: %.4f", beacon.loss); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "fit / podgonka: %.3f", beacon.fit); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "worker:%u node:%s", beacon.worker, beacon.nodeId);

  drawFooter();
}

void drawTrackLine(int x, int y, int w, const char* s) {
  String t = s && s[0] ? String(s) : String("track unknown");
  t.replace("\\/", "/");
  int slash = t.lastIndexOf('/');
  if (slash >= 0 && slash < (int)t.length() - 1) t = t.substring(slash + 1);
  if (t.length() > 36) t = t.substring(0, 33) + "...";
  canvas.fillRoundRect(x, y, w, 22, 6, rgb565(10, 12, 22));
  canvas.drawRoundRect(x, y, w, 22, 6, colorWarn());
  cPrint(x + 8, y + 7, colorText(), rgb565(10,12,22), t, 1);
}


void drawPixelCampScene(int x, int y, int w, int h, bool musicActive) {
  uint32_t t = millis();
  uint16_t sky1 = rgb565(8, 10, 24);
  uint16_t sky2 = rgb565(18, 22, 42);
  uint16_t sky3 = rgb565(31, 24, 35);
  canvas.fillRoundRect(x, y, w, h, 10, sky1);
  canvas.fillRect(x + 2, y + 16, w - 4, 22, sky2);
  canvas.fillRect(x + 2, y + 38, w - 4, h - 40, sky3);

  int mx = x + w - 52;
  int my = y + 12;
  canvas.fillCircle(mx + 18, my + 14, 16, rgb565(126, 128, 122));
  canvas.fillCircle(mx + 10, my + 8, 3, rgb565(80, 82, 86));
  canvas.fillCircle(mx + 25, my + 17, 4, rgb565(85, 84, 88));

  for (int i = 0; i < 24; i++) {
    int sx = x + 8 + ((i * 37 + (slime.rng & 127)) % max(1, w - 16));
    int sy = y + 5 + ((i * 19 + (slime.rng >> 4)) % 42);
    if (sx > mx && sx < mx + 42 && sy > my && sy < my + 38) continue;
    canvas.drawPixel(sx, sy, (i & 3) ? colorDim() : colorText());
  }

  int ground = y + h - 16;
  canvas.fillRect(x + 2, ground, w - 4, 14, rgb565(15, 12, 12));
  canvas.drawFastHLine(x + 2, ground, w - 4, colorWarn());

  for (int i = 0; i < 10; i++) {
    int bx = x + 8 + i * 22;
    int bh = 18 + ((i * 13 + (slime.rng & 31)) % 28);
    if (bx < x + w - 6) canvas.fillRect(bx, ground - bh, 13 + (i % 3) * 4, bh, rgb565(10, 12, 22));
  }

  int fx = x + w / 2 - 12;
  int fy = ground - 30;
  int flick = (int)(sinf(t * 0.018f) * 3.0f);
  canvas.fillRect(fx - 10, fy + 27, 48, 4, rgb565(90, 42, 18));
  canvas.fillRect(fx + 7, fy + 8 - flick, 8, 24 + flick, colorBad());
  canvas.fillRect(fx + 1, fy + 14, 7, 18, rgb565(255, 120, 32));
  canvas.fillRect(fx + 17, fy + 16, 6, 16, colorWarn());
  canvas.fillRect(fx + 10, fy + 13 - flick, 3, 7, colorText());
  for (int i = 0; i < 5; i++) {
    int sx = fx + 24 + (int)(sinf(t * 0.002f + i) * (4 + i));
    int sy = fy + 2 - i * 7 - ((t / 260 + i) % 6);
    canvas.drawFastHLine(sx, sy, 5, rgb565(80, 80, 85));
  }

  int kx = x + 55;
  int ky = ground - 55 + ((t / 400) & 1);
  canvas.fillRect(kx + 12, ky + 17, 17, 22, rgb565(90, 96, 105));
  canvas.fillRect(kx + 14, ky + 4, 15, 13, colorWarn());
  canvas.fillRect(kx + 16, ky + 9, 10, 3, colorText());
  canvas.fillRect(kx + 21, ky + 9, 7, 3, colorBg());
  canvas.fillRect(kx + 4, ky + 23, 17, 4, rgb565(120, 120, 120));
  canvas.drawFastVLine(kx + 34, ky + 4, 38, colorWarn());
  canvas.drawFastHLine(kx + 29, ky + 15, 12, colorWarn());

  int sx = x + w - 92;
  int sy = ground - 53;
  int nod = musicActive ? (int)(sinf(t * 0.025f) * 2.0f) : 0;
  canvas.fillRect(sx + 10, sy + 20, 20, 24, rgb565(55, 58, 64));
  canvas.fillRect(sx + 13, sy + 10 + nod, 15, 13, colorBg());
  canvas.fillRect(sx + 16, sy + 14 + nod, 3, 3, musicActive ? colorWarn() : colorBad());
  canvas.fillRect(sx + 23, sy + 14 + nod, 3, 3, musicActive ? colorWarn() : colorBad());
  canvas.fillRect(sx + 1, sy + 31, 16, 12, rgb565(210, 120, 28));
  canvas.fillRect(sx + 16, sy + 34, 35, 3, colorWarn());
  canvas.drawFastHLine(sx + 6, sy + 34, 43, musicActive ? colorText() : colorWarn());
  canvas.drawFastHLine(sx + 6, sy + 37, 43, colorWarn());
}

void drawSpaceFace(int x, int y, int w, int h) {
  canvas.fillRoundRect(x, y, w, h, 10, colorPanel2());
  uint16_t edge = (spaceRisk > 0.70f) ? colorBad() : (spaceNovelty > 0.55f ? colorWarn() : colorCyan());
  canvas.drawRoundRect(x, y, w, h, 10, edge);
  cPrint(x + 10, y + 7, colorDim(), colorPanel2(), "JGPT SLIME", 1);
  canvas.setTextSize(3);
  canvas.setTextColor(edge, colorPanel2());
  canvas.setCursor(x + 12, y + 23);
  canvas.print(slimeFace());
  cPrintf(x + 78, y + 25, colorText(), colorPanel2(), 1, "%s", slimeMood());
  cPrintf(x + 78, y + 39, colorDim(), colorPanel2(), 1, "nov %.0f risk %.0f", spaceNovelty * 100.0f, spaceRisk * 100.0f);
  cPrintf(x + 10, y + 61, colorDim(), colorPanel2(), 1, "%s", slimeLine);
}

void drawSpaceNodeMarker(int cx, int cy, const char* label, bool online, uint16_t col) {
  canvas.drawCircle(cx, cy, online ? 6 : 4, online ? col : colorDim());
  if (online) canvas.fillCircle(cx, cy, 2, col);
  cPrint(cx - 4, cy - 3, online ? col : colorDim(), colorBg(), label, 1);
}

void drawSwarmSpaceMap(int x, int y, int w, int h) {
  canvas.fillRoundRect(x, y, w, h, 10, rgb565(2, 4, 10));
  canvas.drawRoundRect(x, y, w, h, 10, colorCyan());

  int cellW = max(1, w / JANUS_SPACE_W);
  int cellH = max(1, h / JANUS_SPACE_H);
  int ox = x + (w - cellW * JANUS_SPACE_W) / 2;
  int oy = y + (h - cellH * JANUS_SPACE_H) / 2;
  int ccx0 = ox + (JANUS_SPACE_W / 2) * cellW;
  int ccy0 = oy + (JANUS_SPACE_H / 2) * cellH;

  // Scanner grid: range rings + sweeping beam. This is a network echo-locator view,
  // not a decorative particle cloud.
  for (int rr = 18; rr < min(w, h); rr += 24) {
    canvas.drawCircle(ccx0, ccy0, rr, rgb565(10, 22, 34));
  }
  float sweep = (float)(millis() % 3200UL) / 3200.0f * JANUS_TWO_PI + selfPose.yaw;
  int sx = ccx0 + (int)(cosf(sweep) * (w * 0.42f));
  int sy = ccy0 + (int)(sinf(sweep) * (h * 0.42f));
  canvas.drawLine(ccx0, ccy0, sx, sy, rgb565(25, 85, 94));

  for (int yy = 0; yy < JANUS_SPACE_H; yy++) {
    for (int xx = 0; xx < JANUS_SPACE_W; xx++) {
      SpaceCell& c = spaceMap[yy][xx];
      uint8_t m = max(max(c.occ, c.motion), max(c.sound, max(c.air, c.presence)));
      if (m < 5) continue;
      uint8_t r = (uint8_t)clipf((float)c.occ * 0.82f + (float)c.air * 0.70f + (float)c.motion * 0.25f, 0.0f, 255.0f);
      uint8_t g = (uint8_t)clipf((float)c.presence * 0.84f + (float)c.conf * 0.44f, 0.0f, 255.0f);
      uint8_t b = (uint8_t)clipf((float)c.sound * 0.90f + (float)c.motion * 0.62f + (float)c.conf * 0.28f, 0.0f, 255.0f);
      canvas.fillRect(ox + xx * cellW, oy + yy * cellH, cellW, cellH, rgb565(r, g, b));
    }
  }

  int ccx = ox + (JANUS_SPACE_W / 2) * cellW;
  int ccy = oy + (JANUS_SPACE_H / 2) * cellH;
  canvas.drawFastHLine(ccx - 10, ccy, 21, colorDim());
  canvas.drawFastVLine(ccx, ccy - 10, 21, colorDim());
  canvas.fillCircle(ccx, ccy, 5, colorText());
  canvas.drawCircle(ccx, ccy, 9, colorCyan());
  cPrint(ccx + 10, ccy - 4, colorText(), rgb565(2,4,10), "Core2", 1);

  // Stable node positions learned by the spatial layer. RSSI controls radial depth;
  // StickS3 can also move its marker using its own tilt vector.
  const char* labs[JANUS_SPACE_NODE_SLOTS] = { "EYE", "BCN", "BUZ", "MIC", "SW", "STK", "UNK" };
  uint16_t cols[JANUS_SPACE_NODE_SLOTS] = { colorCyan(), colorGood(), colorWarn(), colorPurple(), colorBlue(), colorBlue(), colorDim() };
  for (int i = 0; i < JANUS_SPACE_NODE_SLOTS; i++) {
    bool on = (spaceNodeMask & (1 << i)) != 0;
    if (!on && spaceNodeConf[i] < 0.03f) continue;
    int nx = ox + (int)roundf(spaceNodeX[i] * cellW);
    int ny = oy + (int)roundf(spaceNodeY[i] * cellH);
    nx = constrain(nx, x + 10, x + w - 10);
    ny = constrain(ny, y + 10, y + h - 10);
    uint16_t link = on ? rgb565(35, 65, 92) : rgb565(18, 24, 34);
    canvas.drawLine(ccx, ccy, nx, ny, link);
    int aura = 7 + (int)(spaceNodeSignal[i] * 8.0f);
    if (on) canvas.drawCircle(nx, ny, aura, dimColor(cols[i], 0.55f));
    drawSpaceNodeMarker(nx, ny, labs[i], on, cols[i]);
  }

  cPrintf(x + 8, y + h - 13, colorDim(), rgb565(2,4,10), 1,
          "scan %.0f%% nodes %u miner %luH best%lu", spaceConfidence * 100.0f, onlineNodeCount(), (unsigned long)coreRemoteHashrate, (unsigned long)coreBestBits);
}

void drawSpaceBars(int x, int y, int w) {
  auto bar = [&](int yy, const char* label, float v, uint16_t col) {
    cPrint(x, yy, colorDim(), colorBg(), label, 1);
    canvas.drawRoundRect(x + 58, yy, w - 62, 8, 4, rgb565(38, 43, 58));
    int bw = (int)clipf(v * (float)(w - 64), 0.0f, (float)(w - 64));
    if (bw > 0) canvas.fillRoundRect(x + 59, yy + 1, bw, 6, 3, col);
  };
  bar(y,      "presence", spacePresence, colorCyan());
  bar(y + 12, "motion",   spaceMotion,   colorWarn());
  bar(y + 24, "sound",    spaceSound,    colorPurple());
  bar(y + 36, "air",      spaceAir,      airColor());
  bar(y + 48, "risk",     spaceRisk,     spaceRisk > 0.65f ? colorBad() : colorGood());
}

void drawSpaceMemoryPanel(int x, int y, int w, int h) {
  canvas.fillRoundRect(x, y, w, h, 10, colorPanel());
  canvas.drawRoundRect(x, y, w, h, 10, colorBlue());
  cPrint(x + 8, y + 7, colorBlue(), colorPanel(), "micro-model memory", 1);
  cPrintf(x + 8, y + 22, colorText(), colorPanel(), 1, "eye %.2f mic %.2f air %.2f", slime.trustEye, slime.trustMic, slime.trustAir);
  cPrintf(x + 8, y + 36, colorText(), colorPanel(), 1, "rssi %.2f swarm %.2f", slime.trustRssi, slime.trustSwarm);
  cPrintf(x + 8, y + 50, colorDim(), colorPanel(), 1, "pred P%.2f M%.2f S%.2f A%.2f", slime.predPresence, slime.predMotion, slime.predSound, slime.predAir);
  cPrintf(x + 8, y + 64, colorDim(), colorPanel(), 1, "stable %.2f curious %.2f", slime.stability, slime.curiosity);
}

void drawLegacySpaceScene(int x, int y, int w, int h) {
  // Compatibility wrapper: the legacy visual scene now renders the GALAXY STATION map.
  drawSwarmSpaceMap(x, y, w, h);
}

void drawBuzzDetail() {
  bool on = buzz.refresh();
  drawBackHeader("BUZZ CONTROL", "camp mirror + music", colorWarn());
  updateBuzzCurrent(false);

  canvas.fillRoundRect(10, 38, 300, 42, 12, colorPanel2());
  canvas.drawRoundRect(10, 38, 300, 42, 12, on ? colorWarn() : colorDim());
  cPrintf(20, 46, on ? colorWarn() : colorDim(), colorPanel2(), 1, "Buzz:%s  H:%s  best:%lu  S:%lu R:%lu",
          on ? "online" : "offline", compactU(buzz.hashRate).c_str(), (unsigned long)buzz.bestBits, (unsigned long)buzz.shares, (unsigned long)buzz.rejects);
  cPrintf(20, 61, colorText(), colorPanel2(), 1, "vol:%u  play:%s  diff:%.6f", buzzDesiredVolume, buzzDesiredPlaying ? "yes" : "no", buzz.diff);

  drawTrackLine(10, 88, 300, buzzTrack);

  drawButton(10, 118, 57, 28, "PREV", colorWarn(), true);
  drawButton(72, 118, 80, 28, buzzDesiredPlaying ? "PAUSE" : "PLAY", colorWarn(), true);
  drawButton(157, 118, 57, 28, "NEXT", colorWarn(), true);
  drawButton(219, 118, 42, 28, "V-", colorWarn(), true);
  drawButton(266, 118, 42, 28, "V+", colorWarn(), true);

  drawPixelCampScene(10, 154, 300, 68, buzzDesiredPlaying);
  drawFooter();
}


void drawSpaceDetail() {
  // v3.1 cinematic observer mode: no admin speed controls, no intrusive panels.
  // The only on-screen controls are the bottom-left / bottom-right screen switchers.
  galaxy.draw(coreWorkerEnabled());
}

void drawAudioDetail() {
  bool on = audioNode.refresh();
  bool liveWant = janusAudioShouldListen();
  bool liveFresh = liveWant && janusAudioLastFrameMs && millis() - janusAudioLastFrameMs < JANUS_AUDIO_IDLE_TIMEOUT_MS;
  drawBackHeader("SWARM / AUDIO RADIO", "EchoBase mic u-law/ADPCM speech monitor", colorPurple());

  int y = 38;
  uint16_t liveCol = liveFresh ? colorGood() : (liveWant ? colorWarn() : colorDim());
  canvas.fillRoundRect(12, y, 296, 42, 8, colorPanel());
  canvas.drawRoundRect(12, y, 296, 42, 8, liveCol);
  cPrintf(18, y + 7, liveCol, colorPanel(), 1, "%s  user:%s sent:%s",
          janusAudioStatusLine, janusAudioLiveUserEnabled ? "ON" : "OFF", janusAudioLiveSentState ? "ON" : "OFF");
  cPrintf(18, y + 20, colorText(), colorPanel(), 1, "rx:%lu q:%lu play:%lu drop:%lu gaps:%lu dup:%lu",
          (unsigned long)janusAudioFramesRx, (unsigned long)janusAudioFramesQueued,
          (unsigned long)janusAudioFramesPlayed, (unsigned long)janusAudioFramesDropped,
          (unsigned long)janusAudioSeqGaps, (unsigned long)janusAudioDuplicateDrops);
  cPrintf(18, y + 32, colorDim(), colorPanel(), 1, "q:%u/%u over:%lu under:%lu rssi:%d age:%lums",
          (unsigned)janusAudioQCount, (unsigned)JANUS_AUDIO_RX_QUEUE_N,
          (unsigned long)janusAudioQueueOverruns, (unsigned long)janusAudioUnderruns,
          janusAudioLastRssi, (unsigned long)(janusAudioLastFrameMs ? millis() - janusAudioLastFrameMs : 0));
  y += 52;

  cPrintf(18, y, on ? colorPurple() : colorDim(), colorBg(), 1, "node:%s age:%lu ms RSSI:%d", on ? "online" : "offline", (unsigned long)audioNode.age(), audioNode.rssi); y += 15;
  cPrintf(18, y, colorText(), colorBg(), 1, "samples:%u rate:%u  queue:%.0f", (unsigned)janusAudioLastSamples, (unsigned)janusAudioLastRate, audioNode.v6); y += 15;
  cPrintf(18, y, colorDim(), colorBg(), 1, "v6.35: u-law 20ms, speech buffer, SD mirror off."); y += 15;
  cPrintf(18, y, colorDim(), colorBg(), 1, "vol:%u  chunk:%u smp  sd:%luK", (unsigned)janusAudioPlayVolume, (unsigned)janusAudioPlayChunkLen, (unsigned long)(janusAudioSdCaptureBytes / 1024UL)); y += 17;

  drawButton(18, 166, 56, 28, "V-", colorPurple(), true);
  canvas.fillRoundRect(84, 166, 132, 28, 7, janusAudioLiveUserEnabled ? colorPanel2() : colorPanel());
  canvas.drawRoundRect(84, 166, 132, 28, 7, janusAudioLiveUserEnabled ? colorGood() : colorDim());
  cPrintf(103, 176, janusAudioLiveUserEnabled ? colorGood() : colorDim(), janusAudioLiveUserEnabled ? colorPanel2() : colorPanel(), 1, janusAudioLiveUserEnabled ? "LIVE AUTO" : "LIVE OFF");
  drawButton(226, 166, 56, 28, "V+", colorPurple(), true);

  cPrintf(18, 205, colorDim(), colorBg(), 1, "Touch V-/V+ volume. BtnB LIVE. BtnC sends AC pulse.");
  drawFooter();
}

int meshBuildOrderedIndices(int out[], int maxN) {
  int n = 0;
  uint32_t now = millis();
  for (int pass = 0; pass < 2; pass++) {
    for (int i = 0; i < CORE2_MAX_COLONY_NODES && n < maxN; i++) {
      if (!colonyNodes[i].used) continue;
      bool on = colonyNodes[i].lastMs && now - colonyNodes[i].lastMs < NODE_TIMEOUT_MS;
      if ((pass == 0 && !on) || (pass == 1 && on)) continue;
      out[n++] = i;
    }
  }
  return n;
}

// v6.22A compile fix:
// Arduino .ino auto-prototype can place a prototype for functions before struct
// UniversalNode is declared. Keep UniversalNode out of the function signature.
const char* nodeDisplayNameByIndex(int idx) {
  if (idx < 0 || idx >= CORE2_MAX_COLONY_NODES) return "node";
  if (colonyNodes[idx].nodeId[0]) return colonyNodes[idx].nodeId;
  if (colonyNodes[idx].role[0]) return colonyNodes[idx].role;
  return "node";
}

void drawMeshNodeRow(int row, int idx, int y, uint32_t now) {
  UniversalNode& n = colonyNodes[idx];
  bool on = n.lastMs && now - n.lastMs < NODE_TIMEOUT_MS;
  uint16_t col = on ? colorText() : colorDim();
  if (n.semanticSlot == 0 && on) col = colorCyan();
  else if (n.semanticSlot == 1 && on) col = colorGood();
  else if (n.semanticSlot == 2 && on) col = colorWarn();
  else if (n.semanticSlot == 3 && on) col = colorPurple();
  else if (n.semanticSlot == 4 && on) col = colorBlue();
  else if (n.semanticSlot == 5 && on) col = colorGood();

  char macShort[16];
  formatMacShort(n.mac, macShort, sizeof(macShort));
  uint32_t ageS = n.lastMs ? (now - n.lastMs) / 1000UL : 9999UL;
  uint16_t panel = (row & 1) ? colorPanel() : colorPanel2();
  uint16_t rssiCol = (n.rssi > -75) ? colorGood() : ((n.rssi > -88) ? colorWarn() : rgb565(230,70,80));

  canvas.fillRoundRect(10, y - 2, 300, 31, 6, panel);
  canvas.drawRoundRect(10, y - 2, 300, 31, 6, on ? dimColor(col, 0.90f) : colorDim());
  cPrintf(16, y + 2, col, panel, 1, "%02d %-4s %-18.18s %s", idx, semanticSlotName(n.semanticSlot), nodeDisplayNameByIndex(idx), on ? "ON" : "--");
  cPrintf(218, y + 2, rssiCol, panel, 1, "%ddB", n.rssi);
  cPrintf(260, y + 2, colorDim(), panel, 1, "%lus", (unsigned long)ageS);

  cPrintf(16, y + 15, colorDim(), panel, 1, "H:%s best:%lu b:%u th:%02d jit:%u mac:%s",
          compactU(n.hashRate).c_str(), (unsigned long)n.bestBits, (unsigned)n.aiBatch,
          (int)n.v[0], (unsigned)n.v[3], macShort);
}

void drawMeshDetail() {
  universalRecountNodes();
  int order[CORE2_MAX_COLONY_NODES];
  int total = meshBuildOrderedIndices(order, CORE2_MAX_COLONY_NODES);
  if (meshScroll < 0) meshScroll = 0;
  int maxScroll = max(0, total - 4);
  if (meshScroll > maxScroll) meshScroll = maxScroll;

  drawBackHeader("MESH / ROSTER", "ordered nodes, scroll list, observe-only", colorBlue());

  int y = 36;
  cPrintf(14, y, colorText(), colorBg(), 1, "online:%u known:%u future:%u rx:%lu drop:%lu",
          (unsigned)colonyOnlineCount, (unsigned)colonyKnownCount, (unsigned)colonyFutureCount,
          (unsigned long)rxPackets, (unsigned long)rxDropped);
  y += 13;
  cPrintf(14, y, colorDim(), colorBg(), 1, "Core2 ESP:%s SS tx:%lu rx:%lu fail:%lu bad:%lu",
          espnowOk ? "ON" : "OFF", (unsigned long)core2SwarmSenseTx, (unsigned long)core2SwarmSenseRx,
          (unsigned long)core2SwarmSenseTxFail, (unsigned long)core2SwarmSenseBad);
  y += 13;
  cPrintf(14, y, colorDim(), colorBg(), 1, "top:%s scroll:%d/%d  tap RSSI for projection", colonyTopNode, meshScroll, maxScroll);
  y += 12;

  if (total <= 0) {
    cPrint(18, y + 18, colorDim(), colorBg(), "No ESP-NOW workers yet. Buzz/Stick/ATOM/EYE appear here automatically.", 1);
  } else {
    int visible = min(4, total - meshScroll);
    for (int r = 0; r < visible; r++) {
      drawMeshNodeRow(r, order[meshScroll + r], y + r * 36, millis());
    }
  }

  drawButton(18, 204, 76, 23, "UP", colorBlue(), meshScroll > 0);
  drawButton(104, 204, 76, 23, "DOWN", colorBlue(), meshScroll < maxScroll);
  drawButton(190, 204, 112, 23, "RSSI MAP", colorCyan(), true);

  drawFooter();
}

void drawRssiMatrixDetail() {
  universalRecountNodes();
  int order[CORE2_MAX_COLONY_NODES];
  int total = meshBuildOrderedIndices(order, CORE2_MAX_COLONY_NODES);
  drawBackHeader("RSSI MATRIX", "Core2-centric RF projection: closer = stronger", colorCyan());

  int cx = 160;
  int cy = 122;
  canvas.drawCircle(cx, cy, 26, colorDim());
  canvas.drawCircle(cx, cy, 54, dimColor(colorBlue(), 0.55f));
  canvas.drawCircle(cx, cy, 82, dimColor(colorBlue(), 0.30f));
  canvas.drawFastHLine(34, cy, 252, dimColor(colorDim(), 0.50f));
  canvas.drawFastVLine(cx, 42, 154, dimColor(colorDim(), 0.50f));
  canvas.fillCircle(cx, cy, 5, colorWarn());
  cPrint(cx - 18, cy + 9, colorWarn(), colorBg(), "CORE2", 1);

  int shown = min(total, 10);
  for (int k = 0; k < shown; k++) {
    UniversalNode& n = colonyNodes[order[k]];
    bool on = n.lastMs && millis() - n.lastMs < NODE_TIMEOUT_MS;
    float strength = clipf(((float)n.rssi + 95.0f) / 55.0f, 0.05f, 1.0f);
    int radius = (int)(88.0f - strength * 62.0f);
    float a = ((float)k / max(1, shown)) * 6.2831853f + (float)n.semanticSlot * 0.23f;
    int x = cx + (int)(cosf(a) * radius);
    int y2 = cy + (int)(sinf(a) * radius * 0.72f);
    uint16_t col = on ? (strength > 0.68f ? colorGood() : (strength > 0.35f ? colorWarn() : rgb565(230,70,80))) : colorDim();
    canvas.drawLine(cx, cy, x, y2, dimColor(col, on ? 0.60f : 0.25f));
    canvas.fillCircle(x, y2, 4, col);
    canvas.drawCircle(x, y2, 7, dimColor(col, 0.60f));
    cPrintf(x - 18, y2 + 9, col, colorBg(), 1, "%s", semanticSlotName(n.semanticSlot));
    cPrintf(x - 20, y2 + 19, colorDim(), colorBg(), 1, "%ddB", n.rssi);
  }

  int y = 198;
  cPrintf(18, y, colorText(), colorBg(), 1, "nodes:%d  strong>-55  ok>-75  shadow<-88", total); y += 12;
  cPrintf(18, y, colorDim(), colorBg(), 1, "Core2 view now; next Buzz/NAS will add pairwise edges.");
  drawButton(214, 204, 86, 23, "MESH", colorBlue(), true);
  drawFooter();
}

void drawSystemDetail() {
  drawBackHeader("SYSTEM", "Core2 local state", colorDim());

  int y = 44;
  cPrintf(18, y, colorText(), colorBg(), 1, "WiFi:%s RSSI:%d channel:%u", wifiOk ? "OK" : "OFF", wifiOk ? WiFi.RSSI() : -127, getWifiChannelSafe()); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "ESP-NOW:%s peerCh:%u worker:%u", espnowOk ? "OK" : "OFF", colonyPeerChannel, colonyWorkerId); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "RX:%lu ER2:%lu HB:%lu DROP:%lu", (unsigned long)rxPackets, (unsigned long)er2Packets, (unsigned long)heartbeatPackets, (unsigned long)rxDropped); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "SS tx:%lu rx:%lu fail:%lu bad:%lu jit:%uus", (unsigned long)core2SwarmSenseTx, (unsigned long)core2SwarmSenseRx, (unsigned long)core2SwarmSenseTxFail, (unsigned long)core2SwarmSenseBad, (unsigned)core2LoopJitterUs); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "SGP30:%s eCO2:%u TVOC:%u", sgpReady ? "OK" : "OFF", eco2, tvoc); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "battery:%d%% mute:%s bright:%u%%", M5.Power.getBatteryLevel(), speakerMuted ? "on" : "off", (unsigned)((coreBrightness * 100) / 255)); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "heap:%lu psram:%lu", (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getFreePsram()); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "SD:%s archiveRows:%lu pilot:S%02u svc:S%02u", core2SdArchiveOk ? "OK" : "OFF", (unsigned long)core2SdArchiveRows, galaxy.universePilotSector, galaxy.universeServiceSector); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "slime:%s conf:%.0f%% risk:%.0f%%", slimeFace(), spaceConfidence * 100.0f, spaceRisk * 100.0f); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "miner job:%s H:%s best:%lu shares:%lu", coreJobText, compactU(coreRemoteHashrate).c_str(), (unsigned long)coreBestBits, (unsigned long)coreSharesSent); y += 18;

  drawButton(18, y, 86, 28, "SEND", colorDim(), true);
  drawButton(116, y, 86, 28, speakerMuted ? "UNMUTE" : "MUTE", colorDim(), true);
  drawButton(214, y, 86, 28, "BUZZ?", colorDim(), true); y += 36;
  drawButton(18, y, 86, 28, "B-", colorWarn(), true);
  drawButton(116, y, 86, 28, "B+", colorWarn(), true);
  drawButton(214, y, 86, 28, "WX", colorBlue(), true);

  drawFooter();
}

void drawWeatherDetail() {
  drawBackHeader("ZAPORIZHZHIA WEATHER", "Open-Meteo + local air", colorBlue());

  canvas.fillRoundRect(10, 40, 300, 86, 12, colorPanel2());
  canvas.drawRoundRect(10, 40, 300, 86, 12, weatherReady ? colorBlue() : colorDim());
  cPrint(22, 49, colorDim(), colorPanel2(), "Zaporizhzhia / pogoda snaruji", 1);

  canvas.setTextSize(4);
  canvas.setTextColor(weatherReady ? colorBlue() : colorDim(), colorPanel2());
  canvas.setCursor(22, 68);
  if (weatherReady) canvas.printf("%.1f", wxTemp);
  else canvas.print("--");
  cPrint(122, 86, colorBlue(), colorPanel2(), "C", 1);

  cPrintf(166, 62, colorText(), colorPanel2(), 1, "feels: %.1f C", wxFeels);
  cPrintf(166, 78, colorText(), colorPanel2(), 1, "wind:  %.1f km/h", wxWind);
  cPrintf(166, 94, colorText(), colorPanel2(), 1, "rain:  %.1f mm", wxPrecip);
  cPrintf(166, 110, colorBlue(), colorPanel2(), 1, "%s", weatherReady ? wxText : "tap REFRESH");

  int y = 142;
  cPrintf(18, y, colorText(), colorBg(), 1, "local eCO2:%u ppm  TVOC:%u ppb", eco2, tvoc); y += 16;
  cPrintf(18, y, colorText(), colorBg(), 1, "airScore: %.2f  trend: %.2f", airScore, airTrend); y += 18;
  drawButton(18, y, 102, 28, "REFRESH", colorBlue(), true);
  drawButton(132, y, 76, 28, "B-", colorWarn(), true);
  drawButton(220, y, 76, 28, "B+", colorWarn(), true);

  drawFooter();
}

void drawScreen() {
  eye.refresh();
  beacon.refresh();
  buzz.refresh();
  audioNode.refresh();
  swarm.refresh();
  stick.refresh();
  universalRecountNodes();

  if (!canvasReady) return;

  canvas.startWrite();
  canvas.fillScreen(colorBg());

  switch (page) {
    case PAGE_AIR: drawAirDetail(); break;
    case PAGE_EYE: drawEyeDetail(); break;
    case PAGE_BEACON: drawBeaconDetail(); break;
    case PAGE_BUZZ: drawBuzzDetail(); break;
    case PAGE_SPACE: drawSpaceDetail(); break;
    case PAGE_AUDIO: drawAudioDetail(); break;
    case PAGE_MESH: drawMeshDetail(); break;
    case PAGE_RSSI: drawRssiMatrixDetail(); break;
    case PAGE_SYSTEM: drawSystemDetail(); break;
    case PAGE_WEATHER: drawWeatherDetail(); break;
    default: drawHomePage(); break;
  }

  canvas.endWrite();
  canvas.pushSprite(0, 0);
}

// ========================= INPUT =========================

bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

void touchHome(int x, int y) {
  if (inRect(x, y, 8, 38, 304, 76)) { page = PAGE_AIR; eventLine = "air detail"; return; }
  if (inRect(x, y, 8, 122, 148, 48)) { page = PAGE_EYE; janusEyeVisionSeqSeen = false; sendJanusEyeVisionControl(janusEyeVisionShouldListen(), true); eventLine = "BlindEye vision page"; return; }
  if (inRect(x, y, 164, 122, 148, 48)) { page = PAGE_BEACON; eventLine = "beacon detail"; return; }
  if (inRect(x, y, 8, 178, 72, 48)) { page = PAGE_BUZZ; eventLine = "buzz detail"; updateBuzzCurrent(true); return; }
  if (inRect(x, y, 86, 178, 72, 48)) { page = PAGE_SPACE; eventLine = "GALAXY STATION opened"; return; }
  if (inRect(x, y, 164, 178, 72, 48)) { page = PAGE_AUDIO; janusAudioSeqSeen = false; sendJanusAudioControl(janusAudioShouldListen(), true); eventLine = "AUDIO LIVE page"; return; }
  if (inRect(x, y, 242, 178, 70, 48)) { page = PAGE_MESH; eventLine = "mesh detail"; return; }
  if (y < 32 && x > 250) { page = PAGE_SYSTEM; eventLine = "system detail"; return; }
}

void touchEye(int x, int y) {
  if (inRect(x, y, 18, 210, 106, 22)) {
    janusEyeVisionUserEnabled = !janusEyeVisionUserEnabled;
    janusEyeVisionSeqSeen = false;
    sendJanusEyeVisionControl(janusEyeVisionShouldListen(), true);
    eventLine = janusEyeVisionUserEnabled ? "BlindEye vision enabled" : "BlindEye vision disabled";
    return;
  }
  if (inRect(x, y, 134, 210, 86, 22)) { page = PAGE_MESH; eventLine = "mesh roster"; return; }
  if (inRect(x, y, 230, 210, 72, 22)) { page = PAGE_HOME; sendJanusEyeVisionControl(false, true); eventLine = "home"; return; }
}

void touchBuzz(int x, int y) {
  if (inRect(x, y, 10, 118, 57, 28)) {
    callMusicEndpoint("prev", JANUS_MUSIC_PREV_URL);
    sendBuzzControl("prev", 0);
    return;
  }
  if (inRect(x, y, 72, 118, 80, 28)) {
    buzzDesiredPlaying = !buzzDesiredPlaying;
    sendBuzzControl("play_pause", buzzDesiredPlaying ? 1 : 0);
    if (!speakerMuted) M5.Speaker.tone(buzzDesiredPlaying ? 1200 : 600, 55);
    return;
  }
  if (inRect(x, y, 157, 118, 57, 28)) {
    callMusicEndpoint("next", JANUS_MUSIC_NEXT_URL);
    sendBuzzControl("next", 0);
    return;
  }
  if (inRect(x, y, 219, 118, 42, 28)) {
    if (buzzDesiredVolume > 0) buzzDesiredVolume--;
    sendBuzzControl("vol_down", -1);
    return;
  }
  if (inRect(x, y, 266, 118, 42, 28)) {
    if (buzzDesiredVolume < 21) buzzDesiredVolume++;
    sendBuzzControl("vol_up", +1);
    return;
  }
}

void touchAir(int x, int y) {
  if (inRect(x, y, 18, 202, 120, 28) || inRect(x, y, 18, 207, 120, 20)) {
    page = PAGE_WEATHER;
    updateWeather(true);
    eventLine = "weather detail";
    return;
  }
}

void touchWeather(int x, int y) {
  if (inRect(x, y, 18, 176, 102, 28)) {
    updateWeather(true);
    return;
  }
  if (inRect(x, y, 132, 176, 76, 28)) {
    brightnessStep(-20);
    return;
  }
  if (inRect(x, y, 220, 176, 76, 28)) {
    brightnessStep(+20);
    return;
  }
}


void touchSpace(int x, int y) {
  // Compatibility name: this is now the JANUS GALAXY STATION touch handler.
  galaxy.touch(x, y);
}

void touchAudio(int x, int y) {
  if (inRect(x, y, 18, 166, 56, 28)) {
    janusAudioSetVolumeDelta(-JANUS_AUDIO_VOLUME_STEP);
    return;
  }
  if (inRect(x, y, 226, 166, 56, 28)) {
    janusAudioSetVolumeDelta(+JANUS_AUDIO_VOLUME_STEP);
    return;
  }
  if (inRect(x, y, 84, 166, 132, 28)) {
    janusAudioLiveUserEnabled = !janusAudioLiveUserEnabled;
    janusAudioSeqSeen = false;
    janusAudioQueueReset(true);
    sendJanusAudioControl(janusAudioShouldListen(), true);
    eventLine = janusAudioLiveUserEnabled ? "AUDIO live enabled" : "AUDIO live disabled";
    return;
  }
}

void touchSystem(int x, int y) {
  if (inRect(x, y, 18, 146, 86, 28)) {
    sendCore2Entropy();
    sendCore2Heartbeat();
    sendCore2SwarmSense();
    eventLine = "manual ESP-NOW + SS pulse";
    return;
  }
  if (inRect(x, y, 116, 146, 86, 28)) {
    speakerMuted = !speakerMuted;
    eventLine = speakerMuted ? "mute on" : "mute off";
    if (!speakerMuted) M5.Speaker.tone(880, 60);
    return;
  }
  if (inRect(x, y, 214, 146, 86, 28)) {
    sendBuzzControl("status", 0);
    updateBuzzCurrent(true);
    return;
  }
  if (inRect(x, y, 18, 182, 86, 28)) {
    brightnessStep(-20);
    return;
  }
  if (inRect(x, y, 116, 182, 86, 28)) {
    brightnessStep(+20);
    return;
  }
  if (inRect(x, y, 214, 182, 86, 28)) {
    page = PAGE_WEATHER;
    updateWeather(true);
    return;
  }
}

void touchMesh(int x, int y) {
  universalRecountNodes();
  int order[CORE2_MAX_COLONY_NODES];
  int total = meshBuildOrderedIndices(order, CORE2_MAX_COLONY_NODES);
  int maxScroll = max(0, total - 4);
  if (inRect(x, y, 18, 204, 76, 26)) {
    if (meshScroll > 0) meshScroll--;
    eventLine = "mesh scroll up";
    return;
  }
  if (inRect(x, y, 104, 204, 76, 26)) {
    if (meshScroll < maxScroll) meshScroll++;
    eventLine = "mesh scroll down";
    return;
  }
  if (inRect(x, y, 190, 204, 112, 26)) {
    page = PAGE_RSSI;
    eventLine = "RSSI projection";
    return;
  }
}

void touchRssiMatrix(int x, int y) {
  if (inRect(x, y, 214, 204, 86, 26)) {
    page = PAGE_MESH;
    eventLine = "mesh roster";
    return;
  }
}

void handleTouch() {
  auto t = M5.Touch.getDetail();
  if (!t.wasPressed()) return;

  uint32_t now = millis();
  if (now - touchLastMs < 80UL) return;
  touchLastMs = now;
  core2TouchCounter++;

  hapticPulse(); // no-op unless CORE2_HAPTIC_ENABLE=1

  int x = t.x;
  int y = t.y;
  if (x < 0 || y < 0 || x >= 320 || y >= 240) return;

  if (page != PAGE_HOME && inRect(x, y, 6, 4, 54, 21)) {
    page = PAGE_HOME;
    sendJanusAudioControl(false, true);
    sendJanusEyeVisionControl(false, true);
    eventLine = "home";
    return;
  }

  if (page == PAGE_HOME) touchHome(x, y);
  else if (page == PAGE_AIR) touchAir(x, y);
  else if (page == PAGE_EYE) touchEye(x, y);
  else if (page == PAGE_BUZZ) touchBuzz(x, y);
  else if (page == PAGE_SPACE) touchSpace(x, y);
  else if (page == PAGE_AUDIO) touchAudio(x, y);
  else if (page == PAGE_MESH) touchMesh(x, y);
  else if (page == PAGE_RSSI) touchRssiMatrix(x, y);
  else if (page == PAGE_SYSTEM) touchSystem(x, y);
  else if (page == PAGE_WEATHER) touchWeather(x, y);
}

void handleButtons() {
  if (M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed()) hapticPulse(70, 22);

  if (M5.BtnA.wasPressed()) {
    page = (page == PAGE_HOME) ? PAGE_AIR : PAGE_HOME;
    if (page == PAGE_HOME) {
      sendJanusAudioControl(false, true);
      sendJanusEyeVisionControl(false, true);
    }
    eventLine = page == PAGE_HOME ? "home" : "detail";
  }

  if (M5.BtnB.wasPressed()) {
    if (page == PAGE_BUZZ) {
      buzzDesiredPlaying = !buzzDesiredPlaying;
      sendBuzzControl("play_pause", buzzDesiredPlaying ? 1 : 0);
    } else if (page == PAGE_SPACE) {
      galaxy.prevView();
    } else if (page == PAGE_AUDIO) {
      janusAudioLiveUserEnabled = !janusAudioLiveUserEnabled;
      janusAudioSeqSeen = false;
      janusAudioQueueReset(true);
      sendJanusAudioControl(janusAudioShouldListen(), true);
      eventLine = janusAudioLiveUserEnabled ? "AUDIO live enabled" : "AUDIO live disabled";
    } else if (page == PAGE_EYE) {
      janusEyeVisionUserEnabled = !janusEyeVisionUserEnabled;
      janusEyeVisionSeqSeen = false;
      sendJanusEyeVisionControl(janusEyeVisionShouldListen(), true);
      eventLine = janusEyeVisionUserEnabled ? "EYE vision enabled" : "EYE vision disabled";
    } else {
      speakerMuted = !speakerMuted;
      eventLine = speakerMuted ? "mute on" : "mute off";
      if (!speakerMuted) M5.Speaker.tone(880, 60);
    }
  }

  if (M5.BtnC.wasPressed()) {
    if (page == PAGE_HOME) page = PAGE_SYSTEM;
    else if (page == PAGE_BUZZ) {
      callMusicEndpoint("next", JANUS_MUSIC_NEXT_URL);
      sendBuzzControl("next", 0);
    } else if (page == PAGE_SPACE) {
      galaxy.nextView();
    } else if (page == PAGE_AUDIO) {
      sendJanusAudioControl(janusAudioShouldListen(), true);
      eventLine = janusAudioShouldListen() ? "AUDIO AC ON pulse" : "AUDIO AC OFF pulse";
    } else {
      sendCore2Entropy();
      sendCore2Heartbeat();
      sendCore2SwarmSense();
      eventLine = "manual ESP-NOW + SS pulse";
      if (!speakerMuted) M5.Speaker.tone(1320, 60);
    }
  }
}

void handleInput() {
  M5.update();
  handleTouch();
  handleButtons();
}

// ========================= MAIN =========================

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);
  M5.Power.setVibration(0);
  Serial.begin(115200);

  M5.Display.setRotation(1);
  M5.Display.setBrightness(coreBrightness);
  M5.Display.fillScreen(TFT_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(0xFFFF, TFT_BLACK);
  M5.Display.setCursor(8, 8);
  M5.Display.println("[JANUS] Core2 v6.40 TP KENSHI");

  canvas.setColorDepth(16);
  if (canvas.createSprite(320, 240) != nullptr) {
    canvasReady = true;
    canvas.setTextFont(1);
  } else {
    canvasReady = false;
    M5.Display.println("[UI] canvas alloc failed");
  }

  prefs.begin("janus_core2", false);
  janusAudioEnsureBuffers();
  coreBrightness = prefs.getUChar("bright", 180);
  janusAudioPlayVolume = prefs.getUChar("audVol", JANUS_AUDIO_PLAY_VOLUME);
  if (janusAudioPlayVolume < JANUS_AUDIO_VOLUME_MIN || janusAudioPlayVolume > JANUS_AUDIO_VOLUME_MAX) janusAudioPlayVolume = JANUS_AUDIO_PLAY_VOLUME;
  // v6.36: old prefs from quiet builds may keep volume around 112. Bump once to the new usable default.
  if (janusAudioPlayVolume < 160) janusAudioPlayVolume = JANUS_AUDIO_PLAY_VOLUME;
  setCoreBrightness(coreBrightness, false);
  initCore2SdArchive();
  spaceInit();
  galaxy.begin();

  initSGP30();
  initWiFiEspNow();

  M5.Speaker.setVolume(janusAudioPlayVolume);
  speakerMuted = true;

  Serial.println("[JANUS] Core2 v6.40B KENSHI TACHYON PROPHECY + RAMA THETA");
  Serial.println("[CORE2/RAMA] theta notebook active: phi/psi/euler/mock finite scheduler, SHA target exact");
  Serial.println("[CORE2/TP] prophecy bus active: RX/answer T,P + K,2 for BlindEye/Swarm predictors");
  eventLine = "Core2 v6.40 TP Kenshi Rama";
  updateBuzzCurrent(true);
  updateWeather(true);
  drawScreen();
}

void loop() {
  uint32_t loopStartUs = micros();
  if (core2LastLoopStartUs) {
    uint32_t d = loopStartUs - core2LastLoopStartUs;
    uint16_t j = (uint16_t)min(65535UL, d);
    core2LoopJitterUs = (uint16_t)((core2LoopJitterUs * 7UL + j) / 8UL);
    if (j > core2LoopMaxUs) core2LoopMaxUs = j;
  }
  core2LastLoopStartUs = loopStartUs;
  uint32_t now = millis();

  handleInput();
  updateCore2SpaceGate();
  hapticTick();
  processRxFrames();
  janusAudioLiveTick();
  janusEyeVisionTick();
  core2TachyonProphecyTick();

  if (now - lastSgpAt >= SGP30_INTERVAL_MS) {
    lastSgpAt = now;
    readSGP30();
  }

  updateSwarmSpace();
  runCore2MiningBatch();

  if (page == PAGE_BUZZ && now - buzzTrackLastMs >= BUZZ_CURRENT_MS) {
    updateBuzzCurrent(false);
  }

  updateWeather(false);

  wifiOk = WiFi.status() == WL_CONNECTED;
  if (!wifiOk) {
    static uint32_t lastWifiRetry = 0;
    if (now - lastWifiRetry > 7000UL) {
      lastWifiRetry = now;
      WiFi.disconnect(false);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  }

  ensureColonyPeer();

  if (now - colonyLastHeartbeatMs >= COLONY_HEARTBEAT_MS) {
    colonyLastHeartbeatMs = now;
    sendCore2Heartbeat();
  }

  if (now - colonyLastEntropyMs >= COLONY_ENTROPY_MS) {
    colonyLastEntropyMs = now;
    sendCore2Entropy();
  }

  if (now - core2LastSwarmSenseMs >= CORE2_SWARMSENSE_MS) {
    core2LastSwarmSenseMs = now;
    sendCore2SwarmSense();
    core2LoopMaxUs = core2LoopJitterUs;
  }

  sendCore2GroundOrderTick();
  sendCore2ZimMissionTick();

  static uint32_t lastDraw = 0;
  if (now - lastDraw >= DRAW_INTERVAL_MS) {
    lastDraw = now;
    drawScreen();
  }

  delay(1);
}
