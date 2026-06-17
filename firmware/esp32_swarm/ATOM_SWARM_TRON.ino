#include <M5Unified.h>
#include <M5EchoBase.h>
#include <math.h>
#include <Wire.h>
#include <M5UnitENV.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <Preferences.h>

constexpr uint32_t JANUS_REMOTE_JOB_TTL_MS = 45000;     // v8.31E4S: hold Buzz remote jobs longer; prevents REMOTE/LOCAL flicker.
constexpr uint32_t JANUS_REMOTE_LOCAL_GRACE_MS = 20000;   // v8.31E4S: stay in REMOTE_WAIT between Buzz ranges instead of jumping local.

// v8.30 KENSHI TACHYON AUDIOFIX: Core2 opens AUDIO page -> sends A/C ON.
// TRON streams EchoBase mic as 20 ms u-law speech frames only while Core2 asks for it.
// Goal: intelligible speech over ESP-NOW: ULAW20, anti-clip TX gain, self-contained frames.
#define JANUS_AUDIO_LIVE_TX_ENABLE     1
#define JANUS_AUDIO_CODEC_ULAW         1
#define JANUS_AUDIO_CODEC_ADPCM4       2
#define JANUS_AUDIO_CODEC_ACTIVE       JANUS_AUDIO_CODEC_ULAW
#define JANUS_AUDIO_SAMPLE_RATE        8000
#define JANUS_AUDIO_FRAME_MS           20
#define JANUS_AUDIO_FRAME_SAMPLES      160      // 20 ms @ 8 kHz
#define JANUS_AUDIO_FRAME_MAX_BYTES    180      // u-law payload: 160 samples -> 160 bytes + spare
#define JANUS_AUDIO_SNAPSHOT_MODE      1        // v8.29: buffered speech clip mode, not fragile live monitor
#define JANUS_AUDIO_SNAPSHOT_FRAMES    20       // v8.30: 0.40 s clip. Lower latency; Core2 still gets prebuffer.
#define JANUS_AUDIO_SNAPSHOT_SEND_GAP_MS 10UL   // send clip faster than realtime; Core2 buffers then plays
#define JANUS_AUDIO_TX_PCM_SAMPLES     (JANUS_AUDIO_FRAME_SAMPLES * 2)  // 20 ms @ 16 kHz -> downsample to 8 kHz
#define JANUS_AUDIO_CONTROL_TIMEOUT_MS 12000UL
#define JANUS_AUDIO_AGC_TARGET_ABS     9800.0f
#define JANUS_AUDIO_AGC_MIN_GAIN       0.60f
#define JANUS_AUDIO_AGC_MAX_GAIN       12.0f
#define JANUS_AUDIO_NOISE_GATE_ABS     6.0f

// v8.30: Kenshi/Tachyon bus compatible with Blind Eye v2.8.
// Swarm can now hear/send prophecy packets and use the first available memory-rich node as shared context.
#define JANUS_TACHYON_PROPHECY_ENABLE  1
#define JANUS_TACHYON_TX_BG_MS         3000UL
#define JANUS_TACHYON_TX_ALERT_MS      900UL
#define JANUS_KENSHI_BUBBLE_TX_MS      4200UL
#define JANUS_SWARM_MEMORY_SAVE_MS     60000UL
#define JANUS_SWARM_MEMORY_SLOTS       12
#define JANUS_SWARM_REMOTE_PROPHECIES  8


// v8.31E4S STABLE SWARM PATCH:
// TRON reports semantic home events to Core2 v6.41 Home Cortex.
// Old nodes ignore J/E and J/P safely.
// Adds attention gating, memory-mirror events, node-aging/homeostasis and safer unattended fatigue behavior.
// v8.31D added semantic de-duplication, hard cool-homeostasis and cleaner node-aging telemetry.
// v8.31E adds a soft mic floor equalizer/guard so EchoBase noise floor cannot self-mute TRON audio events.
#define JANUS_BLACKBOARD_EVENT_ENABLE       1
#define JANUS_BLACKBOARD_EVENT_HEARTBEAT_MS 5000UL
#define JANUS_BLACKBOARD_EVENT_ENV_MS       6500UL
#define JANUS_BLACKBOARD_EVENT_SOUND_MS     2600UL
#define JANUS_BLACKBOARD_EVENT_HASH_MS      8000UL
#define JANUS_BLACKBOARD_EVENT_ALERT_MS     1200UL
#define JANUS_BLACKBOARD_WIFI_WEAK_RSSI     -72
#define JANUS_BLACKBOARD_LOW_HEAP_BYTES     70000UL
#define JANUS_BLACKBOARD_MIN_ATTENTION      18
#define JANUS_BLACKBOARD_MEMORY_MS          45000UL
#define JANUS_BLACKBOARD_TASK_MS            25000UL
#define JANUS_TRON_HOMEOSTASIS_ENABLE       1
#define JANUS_BLACKBOARD_DEDUP_ENABLE        1
#define JANUS_BLACKBOARD_DEDUP_SLOTS         12
#define JANUS_TRON_COOL_FATIGUE_CAP          0.82f
#define JANUS_TRON_REAL_COMBAT_SURPRISE      0.42f

// v8.31E: smart mic floor equalizer.
// The previous adaptive floor could climb to 650 after quiet/loud transitions and then
// gate real sound down to zero. This guard lets the floor rise very slowly, fall faster,
// caps long-term self-mute, and preserves short peak transients for semantic SOUND events.
#define JANUS_TRON_MIC_FLOOR_GUARD_ENABLE   1
#define JANUS_TRON_MIC_FLOOR_MIN            70.0f
#define JANUS_TRON_MIC_FLOOR_BOOT           180.0f
#define JANUS_TRON_MIC_FLOOR_SOFT_MAX       320.0f
#define JANUS_TRON_MIC_FLOOR_HARD_MAX       360.0f
#define JANUS_TRON_MIC_FLOOR_RISE_ALPHA     0.0025f
#define JANUS_TRON_MIC_FLOOR_FALL_ALPHA     0.0180f
#define JANUS_TRON_MIC_FLOOR_DECAY_ALPHA    0.0060f
#define JANUS_TRON_MIC_GATE_MUL             1.12f
#define JANUS_TRON_MIC_PEAK_GATE_MUL        0.18f
#define JANUS_TRON_MIC_STUCK_RMS            42.0f
#define JANUS_TRON_MIC_STUCK_FLOOR          260.0f
#define JANUS_TRON_MIC_SIGNAL_ALPHA         0.22f
#define JANUS_TRON_MIC_RMS_ALPHA            0.18f

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

volatile uint32_t janusBlackboardEventSeq = 0;
volatile uint32_t janusBlackboardEventTx = 0;
volatile uint32_t janusBlackboardEventFail = 0;
volatile uint32_t janusBlackboardEventSkip = 0;
volatile uint8_t  janusBlackboardLastAttention = 0;
volatile uint32_t janusBlackboardPolicyRx = 0;
volatile uint32_t janusBlackboardLastPolicyMs = 0;
volatile uint32_t janusBlackboardQuietUntilMs = 0;
volatile uint8_t  janusBlackboardMood = JM_IDLE;
volatile uint8_t  janusBlackboardRadioRate = 1;
volatile uint8_t  janusBlackboardSensorRate = 1;
volatile uint8_t  janusBlackboardPolicyConfidence = 0;
volatile uint16_t janusBlackboardDangerX100 = 0;
char janusBlackboardOrder[40] = "-";


// ==============================================================================
// JANUS: GROUNDOPS v8.30-AUDIO-SNAPSHOT-SPEECH-TX (RTS BASE + ALIEN SHOOTER HERO RAID + THRONE DEFENSE + ADAPTIVE MINER)
// Roles: Core2 = galactic president/commander; Stick3S = pilot/smuggler/mech carrier; ATOMS3R = planetary command post, base defender and hero raid executor.
// Hardware: M5Stack ATOM S3R + ATOM Echo + ENV/BPS Sensor (QMP6988)
// Architecture: EchoBase Mic Telemetry + ESP-NOW AudioLive TX Smooth/AGC, Original Graphics, Single Remote/Local Mining, BPS Telemetry
// ==============================================================================

namespace Config {
  constexpr int SCREEN_W = 128;
  constexpr int SCREEN_H = 128;
  constexpr int MAX_UNITS = 120;
  constexpr int MAX_RESOURCES = 40;
  constexpr int MAX_PARTICLES = 150;
  constexpr int MAX_SPITS = 42;

  constexpr float PICKUP_DIST_SQ = 4.0f;       
  constexpr float BASE_UNLOAD_DIST_SQ = 25.0f; 
  constexpr float SIGHT_SQ = 2500.0f;          
  constexpr float RANGE_MELEE_SQ = 100.0f;     
  constexpr float RANGE_SPIT_SQ = 225.0f;      
  constexpr float BASE_ATTACK_RANGE_SQ = 300.0f; 
  constexpr float SEPARATION_DIST_SQ = 16.0f;

  constexpr float MAX_DT = 0.05f;
  constexpr float FIXED_DT = 1.0f / 30.0f;
  constexpr float MAX_FRAME_DT = 0.25f;
  constexpr uint8_t MAX_CATCHUP_STEPS = 4;
  constexpr float SPIT_LIFETIME = 0.2f;
  constexpr float BASE_BUILD_COOLDOWN = 1.0f;  
  constexpr float BOREDOM_TIME = 5.0f; 
}

enum class Role : uint8_t { SCAVENGER, GUARD, ARCHER, MAGE, BRUTE, REPAIR, HEALER, HERO, TOWER };
enum class GameState : uint8_t { PLAYING, BETWEEN_WAVES, GAME_OVER };

struct Base {
  float x, y;
  float hp, maxHp;
  int resources;
  uint8_t team;
  uint16_t color;
  float buildCooldown;
};

struct Unit {
  bool active;
  float x, y;
  float vx, vy;
  float hp, maxHp;
  float mana, maxMana;   // v8.15B: hero mana pool, drawn as blue bar under HP
  float speed, dmg, rangeSq;
  uint8_t team;      // 0 = Janus defenders, 1 = wave enemies
  Role role;
  int targetResId;
  bool carrying;
  float cooldown;
  float skillCooldown;   // legacy aggregate indicator only; real skill CDs are per-skill below
  float skillCd[11];      // v8.15B: independent cooldown for each hero skill
  float skillCdMax[11];
  float idleTime;    // used as carried loot / small state memory
  uint16_t heroXp;
  uint8_t heroLv;
  uint8_t heroClass;
  uint16_t skillMask;
  uint16_t lastSkillUsed;
  float skillCooldownMax;
  uint8_t visualSeed;
};

struct Resource { bool active; float x, y; int amount; };
struct Particle { bool active; float x, y, vx, vy, life; uint16_t color; };
struct SpitFX { bool active; float startX, startY, endX, endY, life, maxLife; uint16_t color; };

struct BgDrop {
  float x, y;
  float radius;
  float speed;
  float phase;
};

struct SwarmMicroAI {
  uint32_t tick = 0;
  uint8_t mode[2] = {0, 0};
  int16_t energy[2] = {70, 70};
  int16_t stress[2] = {0, 0};
  int16_t curiosity[2] = {40, 40};
  int16_t confidence[2] = {50, 50};
  uint32_t lastMs = 0;
};


// ==============================================================================
// JANUS NERD MINER / PUBLIC-POOL.IO
// Worker visible in pool as: 1F1Y6CdkApZboDF6g1DYrQ8Dke2E5gWiP1.Swarm_<chipid>
// Set your WiFi credentials before flashing.
// ==============================================================================
const char* WIFI_SSID = "JANUS_WIFI_PLACEHOLDER";
const char* WIFI_PASS = "JANUS_NET_PLACEHOLDER";

const char* POOL_HOST = "public-pool.io";
const uint16_t POOL_PORT = 3333;   // public-pool.io TCP stratum; TLS 4333 needs WiFiClientSecure
const char* BTC_WALLET = "1F1Y6CdkApZboDF6g1DYrQ8Dke2E5gWiP1";
char BTC_WORKER[32] = "Swarm";
char MINER_USER[96] = "";

volatile uint32_t minerRealHashrate = 0;   // LOCAL/POOL H/s (direct public-pool worker)
volatile uint32_t minerRemoteHashrate = 0; // REMOTE H/s (Buzz colony job worker)
float hudRemoteHashrate = 0.0f;
float hudLocalHashrate = 0.0f;
volatile uint32_t minerShares = 0;
volatile double minerCurrentDiff = 1.0;
volatile bool stratumConnected = false;
TaskHandle_t minerTaskHandle = NULL;

// Local real-hash fallback.
// If Stratum has not produced a job yet, we still run a REAL double-SHA256 local workload.
volatile uint32_t minerLocalHashrate = 0;
volatile uint32_t minerBestBits = 0;
volatile uint64_t minerTotalHashes = 0;
volatile bool minerLocalFallback = true;
volatile uint32_t minerSubmitAttempts = 0;
volatile uint32_t minerSubmitRejects = 0;
constexpr uint16_t MIN_POOL_SHARE_BITS = 16; // debug only; v6 submits only shares that meet pool target
volatile uint16_t minerShareTargetBits = 0;
uint8_t minerShareTarget[32];
volatile uint32_t minerLastPoolConnectMs = 0;
volatile uint32_t minerLastJobMs = 0;
volatile uint32_t minerLastSubmitMs = 0;
volatile uint32_t minerLastAcceptMs = 0;
volatile uint32_t minerWifiReconnects = 0;
char minerStatus[20] = "BOOT";
uint32_t lastWifiKickMs = 0;
uint32_t minerWifiRetryDelayMs = 20000UL;  // E4S1 compilefix: macro is defined later

// v8.12B CORE0->GAME reward bridge.
// This mutex is declared here because janusMinerGameReward() is above
// the main miner shared-state mutex in the file.
portMUX_TYPE minerGameRewardMux = portMUX_INITIALIZER_UNLOCKED;

// kind: 1 remote/Buzz share sent, 2 local pool candidate submitted,
//       3 pool ACCEPT, 4 local fallback omen, 5 Buzz Agent gameplay reward.
volatile uint32_t minerGameRewardSeq = 0;
volatile uint8_t  minerGameRewardKind = 0;
volatile uint16_t minerGameRewardBits = 0;
volatile uint32_t minerGameRewardAtMs = 0;

// v8.22: личное состояние устройства.
// Это не "фейковая удача" и не подмена pool ACCEPT.
// Это внутренний агентный слой: личная энтропия, настроение, усталость и бодрость.
portMUX_TYPE janusPersonalMux = portMUX_INITIALIZER_UNLOCKED;
volatile float janusPersonalEntropy = 0.22f;  // 0..1: внутренний хаос/новизна/перегруз
volatile float janusMood = 0.62f;             // 0..1: эмоциональный тон агента
volatile float janusFatigue = 0.10f;          // 0..1: накопленная усталость от жары/боёв/хеша
volatile float janusVigor = 0.76f;            // 0..1: бодрость/готовность
volatile uint8_t janusMoodCode = 1;           // 0 tired, 1 ready, 2 focused, 3 excited, 4 stressed, 5 proud
volatile uint32_t janusPersonalLastTickMs = 0;

// v8.31E4 PERSONALITY HYSTERESIS:
// Stable unattended work must not pin TRON forever at PROUD/M100/V100.
// Real ACCEPT/mission rewards may still create a temporary proud state, but calm operation
// slowly pulls mood/vigor back into a focused/ready band.
#define JANUS_TRON_PERSONALITY_HYSTERESIS_ENABLE 1
#define JANUS_TRON_STRONG_REWARD_HOLD_MS         90000UL
#define JANUS_TRON_MOOD_CALM_SOFT_CAP            0.84f
#define JANUS_TRON_VIGOR_CALM_SOFT_CAP           0.88f
#define JANUS_TRON_ENTROPY_CALM_TARGET           0.18f
#define JANUS_TRON_MOOD_HOME                     0.62f
#define JANUS_TRON_VIGOR_HOME                    0.74f

volatile uint32_t janusPersonalLastStrongRewardMs = 0;
volatile uint32_t janusPersonalLastStressMs = 0;
char janusPersonalLine[40] = "READY E22 M62 F10 V76";

float janusClampF(float v, float lo, float hi) {
  if (!isfinite(v)) return lo;
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

uint8_t janusMoodCodeFrom(float mood, float fatigue, float vigor, float entropy) {
  // v8.31E4S: stable-swarm patch keeps E4 personality hysteresis and adds peer/WiFi/UI guards.
  // v8.31E4: hysteresis-friendly mood bands.
  // PROUD is now reserved for a real reward spike, not ordinary stable uptime.
  if (fatigue > 0.88f || vigor < 0.18f) return 0;      // tired
  if (entropy > 0.78f || mood < 0.34f) return 4;       // stressed
  if (mood > 0.86f && vigor > 0.64f && entropy < 0.66f) return 5; // proud
  if (vigor > 0.78f && entropy > 0.42f) return 3;      // excited/alive
  if (mood > 0.55f && entropy < 0.66f) return 2;       // focused
  return 1;                                            // ready
}

const char* janusMoodName(uint8_t code) {
  switch (code) {
    case 0: return "TIRED";
    case 2: return "FOCUS";
    case 3: return "ALIVE";
    case 4: return "TENSE";
    case 5: return "PROUD";
    default: return "READY";
  }
}

void janusRefreshPersonalLine() {
  float e, m, f, v;
  uint8_t code;
  portENTER_CRITICAL(&janusPersonalMux);
  e = janusPersonalEntropy;
  m = janusMood;
  f = janusFatigue;
  v = janusVigor;
  code = janusMoodCode;
  portEXIT_CRITICAL(&janusPersonalMux);
  snprintf(janusPersonalLine, sizeof(janusPersonalLine), "%s E%02d M%02d F%02d V%02d",
           janusMoodName(code), (int)(e * 100.0f), (int)(m * 100.0f),
           (int)(f * 100.0f), (int)(v * 100.0f));
}

void janusPersonalSnapshot(float& entropy, float& mood, float& fatigue, float& vigor, uint8_t& code) {
  portENTER_CRITICAL(&janusPersonalMux);
  entropy = janusPersonalEntropy;
  mood = janusMood;
  fatigue = janusFatigue;
  vigor = janusVigor;
  code = janusMoodCode;
  portEXIT_CRITICAL(&janusPersonalMux);
}

void janusPersonalReset() {
  portENTER_CRITICAL(&janusPersonalMux);
  janusPersonalEntropy = 0.22f;
  janusMood = 0.62f;
  janusFatigue = 0.10f;
  janusVigor = 0.76f;
  janusMoodCode = janusMoodCodeFrom(janusMood, janusFatigue, janusVigor, janusPersonalEntropy);
  janusPersonalLastTickMs = millis();
  janusPersonalLastStrongRewardMs = 0;
  janusPersonalLastStressMs = 0;
  portEXIT_CRITICAL(&janusPersonalMux);
  janusRefreshPersonalLine();
}

// kind: 1 Buzz ticket sent, 2 pool candidate sent, 3 direct pool ACCEPT,
//       4 local hash omen, 5 Buzz-confirmed pool ACCEPT, 6 reject, 7 mission complete, 8 Buzz praise only.
void janusPersonalNudge(uint8_t kind, uint16_t bits) {
  float bitEnergy = janusClampF((float)bits / 64.0f, 0.0f, 1.4f);
  uint32_t now = millis();

  portENTER_CRITICAL(&janusPersonalMux);
  if (kind == 5) {
    // Buzz-confirmed pool ACCEPT: real reward, allowed to create temporary PROUD state.
    janusPersonalLastStrongRewardMs = now;
    janusMood += 0.18f + bitEnergy * 0.08f;
    janusVigor += 0.15f + bitEnergy * 0.05f;
    janusFatigue -= 0.10f;
    janusPersonalEntropy = janusPersonalEntropy * 0.72f + 0.18f;
  } else if (kind == 3) {
    // Direct pool ACCEPT: real reward.
    janusPersonalLastStrongRewardMs = now;
    janusMood += 0.15f + bitEnergy * 0.06f;
    janusVigor += 0.11f + bitEnergy * 0.04f;
    janusFatigue -= 0.08f;
    janusPersonalEntropy = janusPersonalEntropy * 0.76f + 0.20f;
  } else if (kind == 2) {
    janusMood += 0.020f;
    janusVigor += 0.006f;
    janusFatigue += 0.020f;
    janusPersonalEntropy += 0.030f + bitEnergy * 0.012f;
  } else if (kind == 1) {
    janusMood += 0.012f;
    janusFatigue += 0.018f;
    janusPersonalEntropy += 0.022f + bitEnergy * 0.010f;
  } else if (kind == 6) {
    janusPersonalLastStressMs = now;
    janusMood -= 0.080f;
    janusVigor -= 0.050f;
    janusFatigue += 0.070f;
    janusPersonalEntropy += 0.095f;
  } else if (kind == 8) {
    // v8.31E4: praise-only is morale, not a real achievement.
    // It can lift a flat node, but cannot accumulate to permanent M100/V100.
    if (janusMood < JANUS_TRON_MOOD_CALM_SOFT_CAP) janusMood += 0.010f + bitEnergy * 0.004f;
    if (janusVigor < JANUS_TRON_VIGOR_CALM_SOFT_CAP) janusVigor += 0.004f;
    janusFatigue -= 0.002f;
    janusPersonalEntropy += 0.006f;
  } else if (kind == 7) {
    // Mission complete: real but softer than pool ACCEPT.
    janusPersonalLastStrongRewardMs = now;
    janusMood += 0.050f;
    janusVigor += 0.025f;
    janusFatigue -= 0.035f;
    janusPersonalEntropy *= 0.90f;
  } else {
    janusMood += 0.006f;
    janusFatigue += 0.006f;
    janusPersonalEntropy += 0.014f;
  }

#if JANUS_TRON_PERSONALITY_HYSTERESIS_ENABLE
  // Hard safety caps for non-real-reward nudges. Strong rewards keep their 90 s afterglow.
  bool strongFresh = (janusPersonalLastStrongRewardMs && now - janusPersonalLastStrongRewardMs < JANUS_TRON_STRONG_REWARD_HOLD_MS);
  if (!strongFresh && kind != 3 && kind != 5 && kind != 7) {
    if (janusMood > JANUS_TRON_MOOD_CALM_SOFT_CAP) {
      janusMood = JANUS_TRON_MOOD_CALM_SOFT_CAP + (janusMood - JANUS_TRON_MOOD_CALM_SOFT_CAP) * 0.35f;
    }
    if (janusVigor > JANUS_TRON_VIGOR_CALM_SOFT_CAP) {
      janusVigor = JANUS_TRON_VIGOR_CALM_SOFT_CAP + (janusVigor - JANUS_TRON_VIGOR_CALM_SOFT_CAP) * 0.35f;
    }
  }
#endif

  janusPersonalEntropy = janusClampF(janusPersonalEntropy, 0.02f, 1.00f);
  janusMood = janusClampF(janusMood, 0.02f, 1.00f);
  janusFatigue = janusClampF(janusFatigue, 0.00f, 1.00f);
  janusVigor = janusClampF(janusVigor, 0.00f, 1.00f);
  janusMoodCode = janusMoodCodeFrom(janusMood, janusFatigue, janusVigor, janusPersonalEntropy);
  portEXIT_CRITICAL(&janusPersonalMux);
  janusRefreshPersonalLine();
}

void janusPersonalMetabolism(uint32_t now, uint32_t totalHashRate, bool combat, float tempC, float gameSurprise) {
  static uint32_t lastRejects = 0;
  if (janusPersonalLastTickMs && now - janusPersonalLastTickMs < 750UL) return;
  uint32_t last = janusPersonalLastTickMs ? janusPersonalLastTickMs : now;
  float dt = janusClampF((float)(now - last) / 1000.0f, 0.05f, 3.0f);
  janusPersonalLastTickMs = now;

  float load = janusClampF((float)totalHashRate / 22000.0f, 0.0f, 1.8f);
  float heat = (tempC > 28.0f) ? janusClampF((tempC - 28.0f) / 12.0f, 0.0f, 1.0f) : 0.0f;
  float surprise = janusClampF(gameSurprise / 3.0f, 0.0f, 1.0f);
  uint32_t rejectsNow = minerSubmitRejects;
  uint32_t rejectsPrev = lastRejects;
  if (rejectsNow > lastRejects) janusPersonalNudge(6, 0);
  lastRejects = rejectsNow;

  portENTER_CRITICAL(&janusPersonalMux);
#if JANUS_TRON_HOMEOSTASIS_ENABLE
  // v8.31D: hard cool-homeostasis. The game state is PLAYING almost all the time,
  // so treating it as constant combat made TRON age into F100 while the hardware,
  // radio and miner were healthy. Only surprise/stress counts as real combat now.
  bool realCombat = combat && (surprise > JANUS_TRON_REAL_COMBAT_SURPRISE);
  bool calmPolicy = (janusBlackboardMood == JM_IDLE || janusBlackboardMood == JM_QUIET || janusBlackboardMood == JM_RECOVER);
  bool radioStressHigh = (janusBlackboardDangerX100 > 72);
  float radioStress = radioStressHigh ? 0.010f : 0.0f;
  float recovery = (!realCombat && heat < 0.10f && surprise < 0.34f) ? 0.030f : 0.0f;
  if (calmPolicy && !realCombat && heat < 0.12f) recovery += 0.014f;
  if (load > 0.20f && load < 1.25f && !radioStressHigh && heat < 0.12f) recovery += 0.006f;
  float fatigueDelta = (0.0045f * load + 0.014f * heat + radioStress + (realCombat ? 0.010f : -0.014f) - recovery) * dt;
  janusFatigue += fatigueDelta;
  bool coolStable = (!realCombat && heat < 0.12f && !radioStressHigh && rejectsNow == rejectsPrev);
  if (coolStable && janusFatigue > JANUS_TRON_COOL_FATIGUE_CAP) {
    janusFatigue -= 0.050f * dt;
    if (janusFatigue < JANUS_TRON_COOL_FATIGUE_CAP) janusFatigue = JANUS_TRON_COOL_FATIGUE_CAP;
  }

  // v8.31E4: calm work should recharge TRON, but not inflate it to permanent M100/V100.
  float vigorAboveHome = (janusVigor > JANUS_TRON_VIGOR_HOME) ? (janusVigor - JANUS_TRON_VIGOR_HOME) : 0.0f;
  janusVigor += ((load > 0.05f ? 0.0015f : -0.0050f) -
                 heat * 0.010f -
                 janusFatigue * 0.004f +
                 recovery * 0.18f -
                 vigorAboveHome * 0.018f) * dt;
  janusPersonalEntropy += (surprise * 0.012f + load * 0.002f + heat * 0.010f - (coolStable ? 0.024f : 0.012f)) * dt;
  janusMood += ((JANUS_TRON_MOOD_HOME - janusMood) * 0.020f +
                (janusVigor - JANUS_TRON_VIGOR_HOME) * 0.004f -
                janusFatigue * 0.004f -
                heat * 0.005f) * dt;

#if JANUS_TRON_PERSONALITY_HYSTERESIS_ENABLE
  bool strongFresh = (janusPersonalLastStrongRewardMs &&
                      now - janusPersonalLastStrongRewardMs < JANUS_TRON_STRONG_REWARD_HOLD_MS);
  bool calmStableNoReward = coolStable && calmPolicy && !strongFresh;
  if (calmStableNoReward) {
    if (janusMood > JANUS_TRON_MOOD_CALM_SOFT_CAP) {
      janusMood -= ((janusMood - JANUS_TRON_MOOD_CALM_SOFT_CAP) * 0.14f + 0.0025f) * dt;
    }
    if (janusVigor > JANUS_TRON_VIGOR_CALM_SOFT_CAP) {
      janusVigor -= ((janusVigor - JANUS_TRON_VIGOR_CALM_SOFT_CAP) * 0.16f + 0.0020f) * dt;
    }
    if (janusPersonalEntropy > JANUS_TRON_ENTROPY_CALM_TARGET) {
      janusPersonalEntropy -= (janusPersonalEntropy - JANUS_TRON_ENTROPY_CALM_TARGET) * 0.080f * dt;
    }
  }
#endif
#else
  janusFatigue += (0.010f * load + 0.012f * heat + (combat ? 0.006f : -0.012f)) * dt;
  float vigorAboveHome = (janusVigor > JANUS_TRON_VIGOR_HOME) ? (janusVigor - JANUS_TRON_VIGOR_HOME) : 0.0f;
  janusVigor += ((load > 0.05f ? 0.002f : -0.010f) - heat * 0.010f - janusFatigue * 0.010f - vigorAboveHome * 0.015f) * dt;
  janusPersonalEntropy += (surprise * 0.014f + load * 0.004f + heat * 0.010f - 0.014f) * dt;
  janusMood += ((JANUS_TRON_MOOD_HOME - janusMood) * 0.016f + (janusVigor - JANUS_TRON_VIGOR_HOME) * 0.004f - janusFatigue * 0.010f - heat * 0.005f) * dt;
#if JANUS_TRON_PERSONALITY_HYSTERESIS_ENABLE
  bool strongFresh = (janusPersonalLastStrongRewardMs &&
                      now - janusPersonalLastStrongRewardMs < JANUS_TRON_STRONG_REWARD_HOLD_MS);
  if (!combat && heat < 0.12f && !strongFresh) {
    if (janusMood > JANUS_TRON_MOOD_CALM_SOFT_CAP) janusMood -= ((janusMood - JANUS_TRON_MOOD_CALM_SOFT_CAP) * 0.12f + 0.0020f) * dt;
    if (janusVigor > JANUS_TRON_VIGOR_CALM_SOFT_CAP) janusVigor -= ((janusVigor - JANUS_TRON_VIGOR_CALM_SOFT_CAP) * 0.14f + 0.0020f) * dt;
  }
#endif
#endif
  janusPersonalEntropy = janusClampF(janusPersonalEntropy, 0.02f, 1.00f);
  janusMood = janusClampF(janusMood, 0.02f, 1.00f);
  janusFatigue = janusClampF(janusFatigue, 0.00f, 1.00f);
  janusVigor = janusClampF(janusVigor, 0.00f, 1.00f);
  janusMoodCode = janusMoodCodeFrom(janusMood, janusFatigue, janusVigor, janusPersonalEntropy);
  portEXIT_CRITICAL(&janusPersonalMux);
  janusRefreshPersonalLine();
}

void janusMinerGameReward(uint8_t kind, uint16_t bits) {
  janusPersonalNudge(kind, bits);
  portENTER_CRITICAL(&minerGameRewardMux);
  minerGameRewardKind = kind;
  minerGameRewardBits = bits;
  minerGameRewardAtMs = millis();
  minerGameRewardSeq++;
  portEXIT_CRITICAL(&minerGameRewardMux);
}


// ============================================================================
// JANUS COLONY BUS v6.0
// - Buzz-compatible ESP-NOW protocol.
// - Swarm can run LOCAL Stratum or REMOTE worker mode from Buzz JobPacket.
// - Game/mic/BPS "surprise" becomes EntropyReport fuel for colony AI.
// ============================================================================
#define JANUS_ENABLE_COLONY 1
#define JANUS_COLONY_PULSE_MS 1500UL
#define JANUS_COLONY_ENTROPY_MS 2400UL
#define JANUS_ECHOMIC_KEEPALIVE_MS 1200UL   // v8.31E4: keep Core2 AUDIO slot alive even when live A/F is idle
#define JANUS_MASTER_TIMEOUT_MS 60000UL
#define JANUS_COLONY_PEER_REBUILD_MIN_MS 15000UL
#define JANUS_COLONY_WIFI_RETRY_BASE_MS 20000UL
#define JANUS_COLONY_WIFI_RETRY_MAX_MS 120000UL

char JANUS_NODE_ID[24] = "ATOM_SWARM_TRON";
uint8_t JANUS_BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

struct __attribute__((packed)) JanusColonyPacket {
  char magic[6];       // "JANUS"
  char nodeId[24];
  char role[12];       // BuzzLighter / GroundOps / Swarm
  uint32_t seq;
  uint32_t hashRate;
  uint32_t shares;
  uint32_t rejects;
  uint32_t bestBits;
  float diff;
  uint16_t targetBits;
  uint16_t aiBatch;
  uint8_t aiHint;      // 0 observe, 1 stable, 2 slow-down, 3 boost
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
  uint8_t target[32];
  uint32_t extranonce2;
};

struct __attribute__((packed)) ShareResponse {
  uint8_t magic[2];       // 'S','R'
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
};

// v8.16: future Core2/Buzz mission order. Current Core2 may ignore this until patched,
// but ATOM is already ready to accept explicit ground orders.
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

volatile uint32_t janusGroundOrderSeq = 0;
volatile uint8_t janusGroundOrderMode = 0;
volatile uint8_t janusGroundOrderSector = 0;
volatile uint8_t janusGroundOrderPriority = 0;
volatile uint32_t janusGroundOrderMission = 0;
volatile uint32_t janusGroundOrderAtMs = 0;
volatile uint16_t janusGroundOrderFlags = 0;
char janusGroundOrderTarget[16] = "BASE_HOLD";

struct __attribute__((packed)) EntropyReport {
  uint8_t magic[2];       // 'E','R'
  uint16_t worker_id;
  float local_entropy;
  uint8_t sensor_flags;   // bit0=mic, bit1=bps/env, bit2=game/ai, bit3=prediction
  float values[4];        // v[0]=mic_rms, v[1]=pressure_hpa, v[2]=game_surprise, v[3]=prediction_error
};

struct __attribute__((packed)) EntropyReportV2 {
  uint8_t magic[2];       // 'E','2'
  uint16_t worker_id;
  char nodeId[24];
  float local_entropy;
  float prediction_error;
  float sync_hint;
  float fit;
  uint8_t sensor_flags;   // bit0=mic, bit1=bps/env, bit2=game/ai, bit3=prediction
  float values[8];        // v0=mic_rms, v1=pressure_hpa, v2=temp_c, v3=game_surprise, v4=prediction_error, v5=hashrate, v6=bestBits, v7=agent_mood_x100
  uint32_t uptime_ms;
};

// v8.22C: EchoMic compatibility bridge + Buzz Agent reward packet shared with Blind Eye and Stick.
// Praise packets are only visual morale. Real game buffs require deltaShares>0 from Buzz after pool ACCEPT.
struct __attribute__((packed)) JanusAgentRewardPacket {
  uint8_t magic[2];        // 'A','R'
  uint8_t version;         // 1
  char source[16];         // BuzzAgent
  char targetNode[24];     // ATOM_SWARM_TRON / Swarm / GroundOps / all / *
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

// v8.26 AUDIO LIVE RADIO: packets compatible by magic; codec 1 u-law is the speech-first path, codec 2 remains fallback.
// Core2 -> TRON: 'A','C' control. TRON -> Core2: 'A','F' compressed audio frame.
struct __attribute__((packed)) JanusAudioControlPacket {
  uint8_t magic[2];        // 'A','C'
  uint8_t version;         // 1
  uint8_t enable;          // 0 stop, 1 stream while Core2 is on AUDIO page
  uint8_t codec;           // 1 = u-law, 2 = ADPCM4
  uint16_t sampleRate;     // 8000
  uint16_t frameMs;        // 40 for ADPCM radio mode
  uint32_t seq;
  char source[16];         // Core2Home
  char target[16];         // EchoMic / ATOM_SWARM / all
};

struct __attribute__((packed)) JanusAudioFramePacket {
  uint8_t magic[2];        // 'A','F'
  uint8_t version;         // 2 for ADPCM radio mode
  uint8_t codec;           // 1 = u-law, 2 = ADPCM4
  uint16_t seq;
  uint16_t sampleRate;
  uint16_t samples;        // decoded PCM samples in this frame
  int16_t predictor;       // ADPCM starting predictor for this frame
  uint8_t stepIndex;       // ADPCM starting step index for this frame
  uint8_t flags;           // bit0: speech/open gate
  uint8_t data[JANUS_AUDIO_FRAME_MAX_BYTES];
};

// v8.30: shared low-cost world-state packets. Layout matches Blind Eye v2.8.
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
  float values[6];         // mic, pressure, temp, game surprise, pred error, fit/mood
};

struct __attribute__((packed)) JanusTachyonProphecyPacket {
  uint8_t magic[2];        // 'T','P'
  uint8_t version;         // 1
  uint8_t flags;           // bit0=presence/audio now, bit1=motion/game now, bit2=alert, bit3=remote-assisted
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
  int8_t rssi = 0;
};

struct SwarmRecentMemorySlot {
  bool used = false;
  char nodeId[24] = "";
  uint32_t lastSeenMs = 0;
  float entropy = 0.0f;
  float predError = 0.0f;
  float sync = 0.0f;
  float fit = 0.0f;
  float v0 = 0.0f;
  float v1 = 0.0f;
  float v2 = 0.0f;
  float v3 = 0.0f;
  uint8_t kind = 0;        // 1=E2, 2=TP, 3=Kenshi
};

volatile uint32_t colonySeq = 0;
volatile uint32_t colonyPeersSeen = 0;
volatile uint32_t colonyLastRxMs = 0;
volatile uint32_t colonyLastTxMs = 0;
volatile uint32_t colonyLastEntropyMs = 0;
volatile uint32_t colonyTxFail = 0;
volatile uint32_t colonyTxOk = 0;
volatile uint8_t  colonyPeerChannel = 0;        // v8.31E2/E3: cached ESP-NOW broadcast peer channel
volatile uint32_t colonyPeerRebuilds = 0;       // v8.31E2: diagnostics for peer self-healing
volatile uint32_t colonyEchoMicTxFail = 0;      // v8.31E2: AUDIO/EchoMic mirror delivery watchdog
volatile uint32_t colonyEchoMicTxOk = 0;
volatile uint32_t colonyEchoMicHbTxOk = 0;      // v8.31E4: separate EchoMic JANUS heartbeat watchdog
volatile uint32_t colonyEchoMicHbTxFail = 0;
uint32_t colonyPeerLastFixMs = 0;
uint32_t colonyPeerLastForceMs = 0;
volatile uint32_t colonyPeerRebuildSuppressed = 0;
uint32_t colonyLastEchoMicKeepaliveMs = 0;
volatile uint16_t colonyMiningBatch = 180;
volatile uint8_t colonyAIMode = 0;
volatile uint8_t colonyConfidence = 50;
volatile uint32_t colonyBestPeerBits = 0;
volatile uint32_t colonyBestPeerHash = 0;
volatile uint32_t colonyEntropyRx = 0;
volatile float colonyPeerEntropy = 0.0f;
volatile float swarmLocalEntropy = 0.0f;
volatile float swarmPredictionError = 0.0f;
volatile float swarmMicRms = 0.0f;
volatile float swarmMicPeak = 0.0f;
volatile uint32_t swarmMicFrames = 0;
volatile uint32_t swarmMicFails = 0;
volatile float swarmPressureHpa = 0.0f;
volatile float swarmTempC = 0.0f;
volatile float swarmGameSurprise = 0.0f;
char colonyLastPeer[24] = "none";

// v8.22: Buzz Agent feedback mirrored into the game loop.
// Only Buzz-confirmed pool ACCEPT packets (deltaShares>0, rewardLevel>=3) become buffs.
volatile uint32_t colonyAgentRewardsRx = 0;
volatile uint32_t colonyAgentShareRewardsRx = 0;      // Buzz-confirmed pool ACCEPT rewards only
volatile uint32_t colonyAgentPointsTotal = 0;
volatile uint32_t colonyAgentLastRewardMs = 0;
volatile uint32_t colonyAgentLastSeq = 0;
volatile uint8_t colonyAgentLevel = 0;
volatile uint8_t colonyAgentHint = 1;
volatile uint16_t colonyAgentTargetBatch = 0;
float colonyAgentScore = 0.0f;
float colonyAgentPredictionError = 0.0f;
char colonyAgentSource[16] = "-";

// v8.28 AUDIO ULAW20 VOICEFORMANT TX state.
volatile bool janusAudioTxEnabled = false;
volatile uint32_t janusAudioTxLastControlMs = 0;
volatile uint32_t janusAudioTxControlsRx = 0;
volatile uint32_t janusAudioTxFrames = 0;
volatile uint32_t janusAudioTxSendFail = 0;
volatile uint32_t janusAudioTxReadFail = 0;
uint32_t janusAudioTxLastFrameMs = 0;
uint32_t janusAudioTxLastLogMs = 0;
uint32_t janusAudioTxLastControlLogMs = 0;
uint16_t janusAudioTxSeq = 0;
float janusAudioTxDc = 0.0f;
float janusAudioTxPreemphPrev = 0.0f;
float janusAudioTxSpeechLp = 0.0f;
float janusAudioTxLastShaped = 0.0f;
float janusAudioTxAgcGain = 3.0f;
float janusAudioTxNoiseFloor = 80.0f;
char janusAudioTxSource[16] = "-";
int16_t janusAudioTxAdpcmPredictor = 0;
uint8_t janusAudioTxAdpcmIndex = 0;
bool janusAudioTxAdpcmReady = false;
#if JANUS_AUDIO_SNAPSHOT_MODE
static uint8_t janusAudioSnapData[JANUS_AUDIO_SNAPSHOT_FRAMES][JANUS_AUDIO_FRAME_MAX_BYTES];
static uint16_t janusAudioSnapPayload[JANUS_AUDIO_SNAPSHOT_FRAMES];
static uint16_t janusAudioSnapSamples[JANUS_AUDIO_SNAPSHOT_FRAMES];
static uint8_t janusAudioSnapFlags[JANUS_AUDIO_SNAPSHOT_FRAMES];
uint8_t janusAudioSnapCaptureIdx = 0;
uint8_t janusAudioSnapSendIdx = 0;
uint8_t janusAudioSnapReadyFrames = 0;
bool janusAudioSnapSending = false;
uint32_t janusAudioSnapLastSendMs = 0;
volatile uint32_t janusAudioSnapClips = 0;
#endif

// v8.30: tiny shared memory and prophecy state. This lets a memory-poor rover use the swarm's last known states.
JanusRemoteProphecyState swarmRemoteProphecies[JANUS_SWARM_REMOTE_PROPHECIES];
SwarmRecentMemorySlot swarmRecentMemory[JANUS_SWARM_MEMORY_SLOTS];
Preferences janusSwarmMemoryPrefs;
uint32_t tachyonProphecySeq = 0;
uint32_t tachyonProphecyRx = 0;
uint32_t tachyonProphecyTx = 0;
uint32_t tachyonLastTxMs = 0;
uint32_t kenshiBubbleSeq = 0;
uint32_t kenshiBubbleRx = 0;
uint32_t kenshiBubbleTx = 0;
uint32_t kenshiLastTxMs = 0;
uint32_t lastSwarmMemorySaveMs = 0;
uint8_t swarmVirtualSector = 0;
uint8_t swarmPredictedSector = 0;
uint8_t swarmJobState = 1;
float swarmPredMic1 = 0.0f;
float swarmPredMic2 = 0.0f;
float swarmPredMic3 = 0.0f;
float swarmPredMotion1 = 0.0f;
float swarmPredMotion2 = 0.0f;
float swarmPredMotion3 = 0.0f;
float swarmFutureStress = 0.0f;
float swarmRemotePressure = 0.0f;
float swarmProphecyConfidence = 0.0f;
char swarmMemoryLine[48] = "MEM LOCAL";

// REMOTE worker state from Buzz master.
volatile bool colonyMasterPresent = false;
volatile uint32_t colonyLastMasterMs = 0;
volatile bool remoteJobActive = false;
volatile uint32_t remoteJobRxMs = 0;
uint8_t remoteJobId[8] = {};
uint8_t remoteHeader[80] = {};
uint8_t remoteTarget[32] = {};
uint32_t remoteStartNonce = 0;
uint32_t remoteRangeSize = 0;
uint32_t remoteNonce = 0;
uint32_t remoteRangeEnd = 0;
uint32_t remoteSharesSent = 0;

// v8.0: hot shared state is touched by loop(), ESP-NOW callbacks and miner task.
// Keep this sketch single-file for Arduino, but protect the worst offenders.
portMUX_TYPE swarmSharedMux = portMUX_INITIALIZER_UNLOCKED;

uint16_t getColonyMiningBatch() {
  portENTER_CRITICAL(&swarmSharedMux);
  uint16_t v = colonyMiningBatch;
  portEXIT_CRITICAL(&swarmSharedMux);
  return v;
}

void setColonyMiningBatch(uint16_t v) {
  v = constrain(v, (uint16_t)60, (uint16_t)700);
  portENTER_CRITICAL(&swarmSharedMux);
  colonyMiningBatch = v;
  portEXIT_CRITICAL(&swarmSharedMux);
}

void adjustColonyMiningBatch(int delta) {
  portENTER_CRITICAL(&swarmSharedMux);
  int v = (int)colonyMiningBatch + delta;
  if (v < 60) v = 60;
  if (v > 700) v = 700;
  colonyMiningBatch = (uint16_t)v;
  portEXIT_CRITICAL(&swarmSharedMux);
}

bool getRemoteJobActive() {
  portENTER_CRITICAL(&swarmSharedMux);
  bool v = remoteJobActive;
  portEXIT_CRITICAL(&swarmSharedMux);
  return v;
}

void setRemoteJobActive(bool v) {
  portENTER_CRITICAL(&swarmSharedMux);
  remoteJobActive = v;
  portEXIT_CRITICAL(&swarmSharedMux);
}

void setMinerStatus(const char* s) {
  portENTER_CRITICAL(&swarmSharedMux);
  strlcpy(minerStatus, s ? s : "?", sizeof(minerStatus));
  portEXIT_CRITICAL(&swarmSharedMux);
}

void copyMinerStatus(char* out, size_t outLen) {
  if (!out || outLen == 0) return;
  portENTER_CRITICAL(&swarmSharedMux);
  strlcpy(out, minerStatus, outLen);
  portEXIT_CRITICAL(&swarmSharedMux);
}


uint32_t getMinerSharesSafe() {
  portENTER_CRITICAL(&swarmSharedMux);
  uint32_t v = minerShares;
  portEXIT_CRITICAL(&swarmSharedMux);
  return v;
}

uint32_t getMinerRejectsSafe() {
  portENTER_CRITICAL(&swarmSharedMux);
  uint32_t v = minerSubmitRejects;
  portEXIT_CRITICAL(&swarmSharedMux);
  return v;
}

uint32_t getMinerBestBitsSafe() {
  portENTER_CRITICAL(&swarmSharedMux);
  uint32_t v = minerBestBits;
  portEXIT_CRITICAL(&swarmSharedMux);
  return v;
}

uint32_t incMinerSharesSafe() {
  portENTER_CRITICAL(&swarmSharedMux);
  uint32_t v = ++minerShares;
  portEXIT_CRITICAL(&swarmSharedMux);
  return v;
}

uint32_t incMinerRejectsSafe() {
  portENTER_CRITICAL(&swarmSharedMux);
  uint32_t v = ++minerSubmitRejects;
  portEXIT_CRITICAL(&swarmSharedMux);
  return v;
}

uint32_t updateMinerBestBitsSafe(uint32_t bits) {
  // v8.10 HOTFIX:
  // v8.01 accidentally called updateMinerBestBitsSafe(bits) from inside itself.
  // That infinite recursion overflowed the SwarmMiner task stack and caused
  // "Stack canary watchpoint triggered (SwarmMiner)" reboots.
  portENTER_CRITICAL(&swarmSharedMux);
  if (bits > minerBestBits) minerBestBits = bits;
  uint32_t v = minerBestBits;
  portEXIT_CRITICAL(&swarmSharedMux);
  return v;
}

void copyMinerCoreStats(uint32_t& shares, uint32_t& rejects, uint32_t& bestBits) {
  portENTER_CRITICAL(&swarmSharedMux);
  shares = minerShares;
  rejects = minerSubmitRejects;
  bestBits = minerBestBits;
  portEXIT_CRITICAL(&swarmSharedMux);
}

uint16_t swarmWorkerId() {
  return (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
}

bool jobIdEquals(const uint8_t a[8], const uint8_t b[8]) {
  for (int i=0;i<8;i++) if (a[i] != b[i]) return false;
  return true;
}

bool hashMeetsTargetBytes(const uint8_t hash[32], const uint8_t target[32]) {
  for (int i=0;i<32;i++) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;
}



bool remoteJobFresh() {
#if JANUS_ENABLE_COLONY
  return getRemoteJobActive() && remoteJobRxMs > 0 && (millis() - remoteJobRxMs <= JANUS_REMOTE_JOB_TTL_MS);
#else
  return false;
#endif
}

bool shouldStayRemoteMode() {
#if JANUS_ENABLE_COLONY
  if (!colonyMasterPresent) return false;
  if (remoteJobFresh()) return true;
  if (remoteJobRxMs > 0 && (millis() - remoteJobRxMs <= JANUS_REMOTE_LOCAL_GRACE_MS)) return true;
  return false;
#else
  return false;
#endif
}

// v8.17: local adaptive miner governor for the ground command post.
// Buzz still gives the baseline batch, but ATOMS3R protects UI/ESP-NOW/thermal load.
uint16_t getAdaptiveMiningBatch() {
  uint16_t b = getColonyMiningBatch();
  uint32_t now = millis();

  // Live audio needs a steadier loop and cleaner ESP-NOW airtime. Keep mining alive, but lighter while Core2 listens.
  if (janusAudioTxEnabled && b > 12) b = 12;

  // Recent high-priority ground orders mean the screen/AI is busy: keep hashing alive but lighter.
  bool recentCrisis = (janusGroundOrderAtMs > 0 && now - janusGroundOrderAtMs < 90000UL && janusGroundOrderPriority >= 210);
  if (recentCrisis && b > 420) b = 420;

  // Hero raid / sabotage needs more frame time than static base defense.
  if (janusGroundOrderMode && b > 360) b = 360;

  // ENV temperature is not chip temperature, but it is a useful homeostatic proxy.
  if (swarmTempC > 31.5f && b > 260) b = 260;
  else if (swarmTempC > 29.5f && b > 420) b = 420;

  // If Buzz went away, do not burn the device on local fallback.
  if (!shouldStayRemoteMode() && !stratumConnected && b > 180) b = 180;

  if (janusAudioTxEnabled) return constrain(b, (uint16_t)16, (uint16_t)120);
  return constrain(b, (uint16_t)60, (uint16_t)700);
}

uint8_t minerStatusCode() {
  if (getMinerSharesSafe() > 0) return 4;
  if (!stratumConnected) return WiFi.status() == WL_CONNECTED ? 1 : 0;
  if (minerLastJobMs > 0) return 3;
  return 2;
}

bool janusAgentRewardTargetsThisSwarm(const char* targetNode) {
  if (!targetNode || !targetNode[0]) return true;
  if (!strcasecmp(targetNode, "all")) return true;
  if (!strcmp(targetNode, "*")) return true;
  if (!strncasecmp(targetNode, JANUS_NODE_ID, sizeof(JANUS_NODE_ID))) return true;
  if (strstr(targetNode, "ATOM_SWARM") || strstr(targetNode, "Swarm") || strstr(targetNode, "GroundOps")) return true;
  return false;
}

// Use void* in the public signature so Arduino .ino auto-prototypes cannot fail
// with: 'JanusAgentRewardPacket' does not name a type.
void janusApplyAgentRewardPacket(const void* pktPtr) {
  if (!pktPtr) return;
  const JanusAgentRewardPacket& ar = *(const JanusAgentRewardPacket*)pktPtr;
  if (ar.magic[0] != 'A' || ar.magic[1] != 'R' || ar.version != 1) return;
  if (!janusAgentRewardTargetsThisSwarm(ar.targetNode)) return;

  const bool realPoolAccept = (ar.deltaShares > 0 && ar.rewardLevel >= 3);

  colonyAgentRewardsRx++;
  if (realPoolAccept) colonyAgentShareRewardsRx += ar.deltaShares;
  colonyAgentPointsTotal += realPoolAccept ? ar.rewardPoints : 0;
  colonyAgentLastRewardMs = millis();
  colonyAgentLastSeq = ar.seq;
  colonyAgentLevel = ar.rewardLevel;
  colonyAgentHint = ar.aiHint ? ar.aiHint : 1;
  uint16_t safeAgentBatch = (uint16_t)constrain((int)ar.targetBatch, 60, 700);
  colonyAgentTargetBatch = safeAgentBatch;
  colonyAgentScore = ar.score;
  colonyAgentPredictionError = ar.predictionError;
  strlcpy(colonyAgentSource, ar.source[0] ? ar.source : "BuzzAgent", sizeof(colonyAgentSource));

  // Real buff path: only Buzz-confirmed public-pool ACCEPT may tune the worker or reward the game.
  if (realPoolAccept) {
    if (ar.targetBatch >= 60 && ar.targetBatch <= 700) {
      uint16_t cur = getColonyMiningBatch();
      setColonyMiningBatch((uint16_t)((cur * 2 + ar.targetBatch) / 3));
    }
    if (ar.aiHint == 2 && getColonyMiningBatch() > 80) adjustColonyMiningBatch(-8);
    else if (ar.aiHint == 3) adjustColonyMiningBatch(+12);

    uint32_t dShareCap = ar.deltaShares > 4UL ? 4UL : ar.deltaShares;
    uint16_t bits = (uint16_t)constrain(18 + (int)ar.rewardLevel * 6 + (int)ar.rewardPoints / 18 + (int)dShareCap * 10, 16, 96);
    janusMinerGameReward(5, bits);
    Serial.printf("[AGENT] POOL_ACCEPT_BUFF rx src=%s target=%s lvl=%u pts=%u batch=%u safe=%u dShares=%lu bits=%u score=%.2f err=%.3f\n",
                  colonyAgentSource, ar.targetNode, (unsigned)ar.rewardLevel,
                  (unsigned)ar.rewardPoints, (unsigned)ar.targetBatch, (unsigned)safeAgentBatch,
                  (unsigned long)ar.deltaShares, (unsigned)bits, ar.score, ar.predictionError);
    return;
  }

  // Praise path: visible feedback only. No gold, no XP, no batch boost, no game buff.
  // Route visual praise through the existing miner-event mailbox.
  uint16_t praiseBits = (uint16_t)constrain(8 + (int)ar.rewardLevel * 5 + (int)ar.rewardPoints / 32 + (int)(ar.score / 12.0f), 8, 96);
  janusMinerGameReward(8, praiseBits);
  Serial.printf("[AGENT] praise-only rx src=%s target=%s lvl=%u pts=%u batch=%u safe=%u dShares=%lu score=%.2f err=%.3f\n",
                colonyAgentSource, ar.targetNode, (unsigned)ar.rewardLevel,
                (unsigned)ar.rewardPoints, (unsigned)ar.targetBatch, (unsigned)safeAgentBatch,
                (unsigned long)ar.deltaShares, ar.score, ar.predictionError);
}

bool janusAudioControlTargetsThisNode(const char* target) {
  if (!target || !target[0]) return true;
  if (!strcasecmp(target, "all")) return true;
  if (!strcmp(target, "*")) return true;
  if (!strncasecmp(target, JANUS_NODE_ID, sizeof(JANUS_NODE_ID))) return true;
  if (!strcasecmp(target, "EchoMic")) return true;
  if (strstr(target, "ATOM") || strstr(target, "SWARM") || strstr(target, "Swarm") || strstr(target, "GroundOps")) return true;
  return false;
}

// Use void* in the public signature so Arduino .ino auto-prototypes cannot break on packet type ordering.
void janusHandleAudioControlPacket(const void* pktPtr) {
#if JANUS_AUDIO_LIVE_TX_ENABLE
  if (!pktPtr) return;
  const JanusAudioControlPacket& ac = *(const JanusAudioControlPacket*)pktPtr;
  if (ac.magic[0] != 'A' || ac.magic[1] != 'C' || ac.version != 1) return;
  if (ac.codec != JANUS_AUDIO_CODEC_ULAW && ac.codec != JANUS_AUDIO_CODEC_ADPCM4) return;
  if (!janusAudioControlTargetsThisNode(ac.target)) return;

  uint32_t now = millis();
  janusAudioTxControlsRx++;
  janusAudioTxLastControlMs = now;
  strlcpy(janusAudioTxSource, ac.source[0] ? ac.source : "Core2Home", sizeof(janusAudioTxSource));
  bool oldState = janusAudioTxEnabled;
  janusAudioTxEnabled = ac.enable != 0;
  if (!janusAudioTxEnabled) {
#if JANUS_AUDIO_SNAPSHOT_MODE
    janusAudioSnapCaptureIdx = 0;
    janusAudioSnapSendIdx = 0;
    janusAudioSnapReadyFrames = 0;
    janusAudioSnapSending = false;
    janusAudioSnapLastSendMs = 0;
#endif
    if (oldState || now - janusAudioTxLastControlLogMs > 2500UL) {
      janusAudioTxLastControlLogMs = now;
      Serial.printf("[AUDIO-TX] Core2 AC OFF src=%s seq=%lu\n", janusAudioTxSource, (unsigned long)ac.seq);
    }
  } else {
#if JANUS_AUDIO_SNAPSHOT_MODE
    if (!oldState) {
      janusAudioSnapCaptureIdx = 0;
      janusAudioSnapSendIdx = 0;
      janusAudioSnapReadyFrames = 0;
      janusAudioSnapSending = false;
      janusAudioSnapLastSendMs = 0;
    }
#endif
    if (!oldState || now - janusAudioTxLastControlLogMs > 5000UL) {
      janusAudioTxLastControlLogMs = now;
      Serial.printf("[AUDIO-TX] Core2 AC ON src=%s target=%s seq=%lu codec=%u %uHz/%ums gain=%.1f\n",
                    janusAudioTxSource, ac.target, (unsigned long)ac.seq, (unsigned)ac.codec,
                    (unsigned)ac.sampleRate, (unsigned)ac.frameMs, janusAudioTxAgcGain);
    }
  }
#endif
}

void swarmRememberNodeState(const char* nodeId, uint8_t kind, float entropy, float predErr, float sync, float fit, float v0, float v1, float v2, float v3);
void onJanusTachyonProphecy(const JanusTachyonProphecyPacket& tp, int8_t rxRssi);
void onJanusKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi);
void tachyonProphecyTick();
void loadSwarmMemoryState();
void saveSwarmMemoryState(bool force=false);


void janusColonyEnsurePeer();
void janusColonyForcePeerRebuild(const char* reason);

// v8.31E1 COMPILEFIX: janusBlackboardTronTick() is placed before the
// EchoBase globals/helpers are defined lower in the sketch. Declare them here so
// Arduino ESP32 core 3.x / GCC 14 sees the mic guard state used by semantic audio events.
extern float micNoiseFloor;
extern float micSignal;
extern float micRawRms;
extern float micLastGated;
extern uint32_t micFloorGuardHits;
extern uint32_t micFloorLastClampMs;

static inline float janusTronMicMaxF(float a, float b);
float janusTronUpdateMicFloor(float rmsRaw, float peakToPeak, uint32_t nowMs);
float janusTronComputeGatedMic(float rmsRaw, float peakToPeak, float floorNow);

uint16_t janusBbHash16(const char* s) {
  uint16_t h = 0x811C;
  if (!s) return h;
  while (*s) {
    h ^= (uint8_t)(*s++);
    h = (uint16_t)(h * 167U + 13U);
  }
  return h;
}

int16_t janusBbX10(float v, float lo, float hi) {
  if (!isfinite(v)) v = 0.0f;
  v = constrain(v, lo, hi);
  return (int16_t)roundf(v * 10.0f);
}

uint8_t janusBbConfidenceFrom01(float v, uint8_t lo=10, uint8_t hi=96) {
  if (!isfinite(v)) v = 0.0f;
  v = constrain(v, 0.0f, 1.0f);
  return (uint8_t)constrain((int)lo + (int)roundf(v * (float)(hi - lo)), 0, 100);
}

const char* janusBbMoodName(uint8_t mood) {
  switch (mood) {
    case JM_QUIET: return "QUIET";
    case JM_ALERT: return "ALERT";
    case JM_EXPLORE: return "EXPLORE";
    case JM_GUARD: return "GUARD";
    case JM_RECOVER: return "RECOVER";
    default: return "IDLE";
  }
}

void janusHandleBlackboardPolicyPacket(const JanusPolicyPacket& jp) {
#if JANUS_BLACKBOARD_EVENT_ENABLE
  if (jp.magic[0] != 'J' || jp.magic[1] != 'P' || jp.version != 1) return;
  janusBlackboardPolicyRx++;
  janusBlackboardLastPolicyMs = millis();
  janusBlackboardMood = jp.swarmMood;
  janusBlackboardRadioRate = jp.radioRate;
  janusBlackboardSensorRate = jp.sensorRate;
  janusBlackboardPolicyConfidence = jp.confidence;
  janusBlackboardDangerX100 = jp.danger_x100;
  uint32_t now = millis();
  uint32_t quietForMs = jp.quietUntilMs;
  if (quietForMs > 60000UL) quietForMs = 60000UL;
  janusBlackboardQuietUntilMs = quietForMs ? (now + quietForMs) : 0;
  strlcpy(janusBlackboardOrder, jp.order[0] ? jp.order : "-", sizeof(janusBlackboardOrder));

  static uint32_t lastPolicyLogMs = 0;
  if (now - lastPolicyLogMs > 3000UL) {
    lastPolicyLogMs = now;
    Serial.printf("[BLACKBOARD/TRON] policy rx=%lu mood=%s radio=%u sensor=%u conf=%u danger=%.2f order=%s\n",
                  (unsigned long)janusBlackboardPolicyRx,
                  janusBbMoodName(janusBlackboardMood),
                  (unsigned)janusBlackboardRadioRate,
                  (unsigned)janusBlackboardSensorRate,
                  (unsigned)janusBlackboardPolicyConfidence,
                  (float)janusBlackboardDangerX100 / 100.0f,
                  janusBlackboardOrder);
  }
#endif
}

uint16_t janusTronCapabilities() {
  // ATOM_SWARM_TRON currently exposes pressure/temp ENV, mic/audio, hash, game/AI and relay memory.
  // IMU flag is advertised as a reserved capability scaffold for the shared JANUS node map.
  return JC_TEMP | JC_PRESS | JC_MIC | JC_AUDIO | JC_HASH | JC_RELAY | JC_MEMORY | JC_AI | JC_RF | JC_IMU;
}

uint32_t janusBbMix32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dUL;
  x ^= x >> 15; x *= 0x846ca68bUL;
  x ^= x >> 16;
  return x;
}

uint8_t janusBbAttentionScore(uint8_t eventType, uint8_t confidence, uint8_t urgency, uint32_t ttlMs) {
  // v8.31C: tiny attention engine. It keeps the binary semantic bus useful without
  // flooding ESP-NOW with low-value repetitions. Core still receives all important
  // events, while weak ambient sound/env chatter is automatically thinned.
  uint16_t score = ((uint16_t)confidence * 3U + (uint16_t)urgency * 5U) / 8U;
  if (eventType == JE_DANGER || eventType == JE_SOLO_REJECT || eventType == JE_WIFI_WEAK || eventType == JE_LOW_HEAP) score += 26;
  else if (eventType == JE_SOLO_ACCEPT || eventType == JE_TASK_NEED) score += 22;
  else if (eventType == JE_BOOT || eventType == JE_HEARTBEAT) score += 12;
  else if (eventType == JE_SOUND || eventType == JE_MOTION || eventType == JE_PRESENCE) score += 8;
  else if (eventType == JE_AI_MEMORY) score += 5;
  if (janusBlackboardMood == JM_ALERT || janusBlackboardMood == JM_GUARD) score += 7;
  if (janusBlackboardMood == JM_RECOVER && (eventType == JE_LOW_HEAP || eventType == JE_WIFI_WEAK || eventType == JE_TASK_NEED)) score += 10;
  if (ttlMs && ttlMs < 15000UL) score += 4;
  if (score > 100U) score = 100U;
  return (uint8_t)score;
}

bool janusBbCriticalEvent(uint8_t eventType) {
  return eventType == JE_BOOT || eventType == JE_HEARTBEAT || eventType == JE_DANGER ||
         eventType == JE_SOLO_ACCEPT || eventType == JE_SOLO_REJECT ||
         eventType == JE_WIFI_WEAK || eventType == JE_LOW_HEAP || eventType == JE_TASK_NEED;
}

struct JanusBbRecentEvent {
  uint8_t eventType;
  uint16_t topicHash;
  uint16_t objectHash;
  int16_t a;
  int16_t b;
  int16_t c;
  int16_t d;
  uint32_t lastMs;
};

JanusBbRecentEvent janusBbRecent[JANUS_BLACKBOARD_DEDUP_SLOTS];
uint8_t janusBbRecentCursor = 0;

uint16_t janusBbAbsDiff16(int16_t x, int16_t y) {
  int32_t d = (int32_t)x - (int32_t)y;
  if (d < 0) d = -d;
  return (uint16_t)(d > 32767 ? 32767 : d);
}

uint32_t janusBbDedupWindowMs(uint8_t eventType) {
  switch (eventType) {
    case JE_ENV:       return 16000UL;
    case JE_SOUND:     return 5200UL;
    case JE_PRESENCE:  return 9000UL;
    case JE_MOTION:    return 4500UL;
    case JE_HASH:      return 12000UL;
    case JE_AI_MEMORY: return 40000UL;
    case JE_TASK_DONE: return 50000UL;
    default:           return 0UL;
  }
}

uint16_t janusBbDedupDelta(uint8_t eventType) {
  switch (eventType) {
    case JE_ENV:       return 4;     // ~0.4C / 0.4hPa in x10 fields
    case JE_SOUND:     return 420;   // raw Echo RMS/peak snapshot change
    case JE_PRESENCE:  return 260;
    case JE_MOTION:    return 8;
    case JE_HASH:      return 45;    // about 450 H/s after /10 packing
    case JE_AI_MEMORY: return 3;
    case JE_TASK_DONE: return 8;
    default:           return 0;
  }
}

bool janusBbShouldDedup(uint8_t eventType, uint16_t topicHash, uint16_t objectHash,
                        int16_t a, int16_t b, int16_t c, int16_t d) {
#if JANUS_BLACKBOARD_DEDUP_ENABLE
  uint32_t window = janusBbDedupWindowMs(eventType);
  uint16_t delta = janusBbDedupDelta(eventType);
  if (window == 0 || delta == 0) return false;
  uint32_t now = millis();
  for (uint8_t i = 0; i < JANUS_BLACKBOARD_DEDUP_SLOTS; ++i) {
    JanusBbRecentEvent& r = janusBbRecent[i];
    if (r.lastMs == 0) continue;
    if (r.eventType != eventType || r.topicHash != topicHash || r.objectHash != objectHash) continue;
    if (now - r.lastMs > window) continue;
    uint32_t drift = (uint32_t)janusBbAbsDiff16(r.a, a) + janusBbAbsDiff16(r.b, b) +
                     janusBbAbsDiff16(r.c, c) + janusBbAbsDiff16(r.d, d);
    if (drift <= delta) return true;
  }
#endif
  return false;
}

void janusBbRememberEvent(uint8_t eventType, uint16_t topicHash, uint16_t objectHash,
                          int16_t a, int16_t b, int16_t c, int16_t d) {
#if JANUS_BLACKBOARD_DEDUP_ENABLE
  uint8_t slot = janusBbRecentCursor++ % JANUS_BLACKBOARD_DEDUP_SLOTS;
  janusBbRecent[slot].eventType = eventType;
  janusBbRecent[slot].topicHash = topicHash;
  janusBbRecent[slot].objectHash = objectHash;
  janusBbRecent[slot].a = a;
  janusBbRecent[slot].b = b;
  janusBbRecent[slot].c = c;
  janusBbRecent[slot].d = d;
  janusBbRecent[slot].lastMs = millis();
#endif
}

uint32_t janusBbEventChecksum(const JanusEventPacket& je) {
  // Not a transport CRC enforced by Core yet; it is a deterministic checksum packed
  // into eventHash for future protocol validation while keeping J/E v1 ABI unchanged.
  uint32_t h = 0x4A45564FUL;
  const uint8_t* b = (const uint8_t*)&je;
  const size_t n = sizeof(JanusEventPacket) - sizeof(uint32_t) - sizeof(uint32_t); // skip eventHash+ttl tail
  for (size_t i = 0; i < n; ++i) {
    h ^= b[i];
    h *= 16777619UL;
  }
  h ^= je.ttlMs;
  return janusBbMix32(h);
}

bool janusEmitBlackboardEvent(uint8_t eventType, const char* kind, uint8_t confidence, uint8_t urgency,
                              uint16_t topicHash, uint16_t objectHash,
                              int16_t a, int16_t b, int16_t c, int16_t d, uint32_t ttlMs) {
#if JANUS_BLACKBOARD_EVENT_ENABLE
  uint8_t attention = janusBbAttentionScore(eventType, confidence, urgency, ttlMs);
  janusBlackboardLastAttention = attention;
  if (!janusBbCriticalEvent(eventType) && attention < JANUS_BLACKBOARD_MIN_ATTENTION) {
    janusBlackboardEventSkip++;
    return false;
  }
  if (!janusBbCriticalEvent(eventType) && janusBbShouldDedup(eventType, topicHash, objectHash, a, b, c, d)) {
    janusBlackboardEventSkip++;
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) return false;
  janusColonyEnsurePeer();

  JanusEventPacket je{};
  je.magic[0] = 'J'; je.magic[1] = 'E';
  je.version = 1;
  je.eventType = eventType;
  je.nodeRole = JR_TRON;
  je.confidence = confidence > 100 ? 100 : confidence;
  je.urgency = urgency > 100 ? 100 : urgency;
  strlcpy(je.nodeId, JANUS_NODE_ID, sizeof(je.nodeId));
  strlcpy(je.kind, kind && kind[0] ? kind : "tron", sizeof(je.kind));
  je.seq = ++janusBlackboardEventSeq;
  je.uptimeMs = millis();
  je.topicHash = topicHash;
  je.objectHash = objectHash;
  je.capabilities = janusTronCapabilities();
  je.valueA_x10 = a;
  je.valueB_x10 = b;
  je.valueC_x10 = c;
  je.valueD_x10 = d;
  je.ttlMs = ttlMs;
  je.eventHash = janusBbEventChecksum(je);

  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&je, sizeof(je));
  if (err == ESP_OK) {
    janusBbRememberEvent(eventType, topicHash, objectHash, a, b, c, d);
    janusBlackboardEventTx++;
    return true;
  }
  janusBlackboardEventFail++;
  static uint32_t lastBbFailLogMs = 0;
  uint32_t failNow = millis();
  if (failNow - lastBbFailLogMs > 2500UL) {
    lastBbFailLogMs = failNow;
    Serial.printf("[BLACKBOARD/TRON] tx fail err=%d fail=%lu ch=%u wifi=%d\n",
                  (int)err,
                  (unsigned long)janusBlackboardEventFail,
                  (unsigned)colonyPeerChannel,
                  (int)WiFi.status());
  }
  janusColonyForcePeerRebuild("blackboard-tx-fail");
#endif
  return false;
}

void janusBlackboardBootEvent() {
#if JANUS_BLACKBOARD_EVENT_ENABLE
  janusEmitBlackboardEvent(JE_BOOT, "tron_boot", 88, 30,
                           janusBbHash16("boot"), janusBbHash16(JANUS_NODE_ID),
                           (int16_t)WiFi.RSSI(), (int16_t)(ESP.getFreeHeap() / 1024), 0, 0, 45000UL);
#endif
}

void janusBlackboardTronTick() {
#if JANUS_BLACKBOARD_EVENT_ENABLE
  uint32_t now = millis();
  static uint32_t lastHeartbeatMs = 0;
  static uint32_t lastEnvMs = 0;
  static uint32_t lastSoundMs = 0;
  static uint32_t lastHashMs = 0;
  static uint32_t lastMemoryMs = 0;
  static uint32_t lastTaskMs = 0;
  static uint32_t lastAlertMs = 0;
  static uint32_t lastAcceptSeen = 0;
  static uint32_t lastRejectSeen = 0;
  static uint32_t lastDiagMs = 0;

  uint32_t policyAge = janusBlackboardLastPolicyMs ? now - janusBlackboardLastPolicyMs : 0xFFFFFFFFUL;
  uint32_t radioMul = (janusBlackboardRadioRate == 0 || now < janusBlackboardQuietUntilMs) ? 2UL : (janusBlackboardRadioRate >= 2 ? 1UL : 1UL);
  uint32_t sensorMul = (janusBlackboardSensorRate == 0 || now < janusBlackboardQuietUntilMs) ? 2UL : 1UL;

  int8_t rssi = (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : 0;
  uint32_t totalH = minerRealHashrate + minerRemoteHashrate + minerLocalHashrate;
  uint32_t shares = getMinerSharesSafe() + colonyAgentShareRewardsRx;
  uint32_t rejects = getMinerRejectsSafe();
  uint32_t best = getMinerBestBitsSafe();
  float pEntropy = 0.0f, pMood = 0.0f, pFatigue = 0.0f, pVigor = 0.0f;
  uint8_t pMoodCode = 0;
  janusPersonalSnapshot(pEntropy, pMood, pFatigue, pVigor, pMoodCode);

  if (now - lastHeartbeatMs >= JANUS_BLACKBOARD_EVENT_HEARTBEAT_MS * radioMul) {
    lastHeartbeatMs = now;
    uint8_t conf = (WiFi.status() == WL_CONNECTED) ? 84 : 28;
    janusEmitBlackboardEvent(JE_HEARTBEAT, "tron_state", conf, 22,
                             janusBbHash16("tron"), janusBbHash16("heartbeat"),
                             (int16_t)constrain((int32_t)totalH / 10, -32768L, 32767L),
                             (int16_t)best,
                             (int16_t)rssi,
                             (int16_t)getAdaptiveMiningBatch(),
                             18000UL);
  }

  if (now - lastEnvMs >= JANUS_BLACKBOARD_EVENT_ENV_MS * sensorMul) {
    lastEnvMs = now;
    uint8_t conf = (swarmPressureHpa > 100.0f || swarmTempC > 0.1f) ? 82 : 35;
    janusEmitBlackboardEvent(JE_ENV, "tron_env", conf, 36,
                             janusBbHash16("home-env"), janusBbHash16("tron-bps"),
                             janusBbX10(swarmTempC, -40.0f, 85.0f),
                             janusBbX10(swarmPressureHpa, 0.0f, 1600.0f),
                             janusBbX10(swarmPredictionError, 0.0f, 20.0f),
                             janusBbX10(swarmLocalEntropy, 0.0f, 20.0f),
                             24000UL);
  }

  // v8.31B COMPILEFIX2: Arduino ESP32 core 3.x / GCC 14 refuses std::max
  // when one argument is volatile float. Snapshot volatile telemetry first and use
  // explicit comparisons so the blackboard audio event remains compile-safe.
  float micRmsNow = (float)swarmMicRms;
  float micPeakNow = (float)swarmMicPeak;
  float micSignalNow = (float)micSignal;
  float micNoiseNow = (float)micNoiseFloor;
  float micRawNow = (float)micRawRms;
  float micGatedNow = (float)micLastGated;
  float soundEnergy = micRmsNow;
  if (micSignalNow > soundEnergy) soundEnergy = micSignalNow;
  if (micGatedNow > soundEnergy) soundEnergy = micGatedNow;
  // v8.31E: blackboard sound threshold uses guarded/capped floor, not runaway floor.
  float soundFloorForDecision = janusClampF(micNoiseNow, JANUS_TRON_MIC_FLOOR_MIN, JANUS_TRON_MIC_FLOOR_SOFT_MAX);
  float soundHotThreshold = janusTronMicMaxF(220.0f, soundFloorForDecision * 1.65f);
  bool soundHot = soundEnergy > soundHotThreshold;
  if ((soundHot && now - lastSoundMs >= JANUS_BLACKBOARD_EVENT_ALERT_MS) ||
      (now - lastSoundMs >= JANUS_BLACKBOARD_EVENT_SOUND_MS * sensorMul && soundEnergy > 40.0f)) {
    lastSoundMs = now;
    float soundConfDen = janusTronMicMaxF(520.0f, soundFloorForDecision * 3.8f);
    uint8_t conf = janusBbConfidenceFrom01(soundEnergy / soundConfDen, 28, 94);
    uint8_t urg = soundHot ? 74 : 34;
    janusEmitBlackboardEvent(soundHot ? JE_SOUND : JE_PRESENCE, "tron_audio", conf, urg,
                             janusBbHash16("audio"), janusBbHash16("echo-mic"),
                             janusBbX10(micRmsNow / 10.0f, 0.0f, 3200.0f),
                             janusBbX10(micPeakNow / 10.0f, 0.0f, 3200.0f),
                             janusBbX10(micNoiseNow / 10.0f, 0.0f, 3200.0f),
                             janusBbX10(swarmFutureStress, 0.0f, 20.0f),
                             soundHot ? 12000UL : 18000UL);
  }

  bool motionHot = swarmGameSurprise > 0.75f || swarmFutureStress > 1.1f;
  if (motionHot && now - lastAlertMs >= JANUS_BLACKBOARD_EVENT_ALERT_MS) {
    lastAlertMs = now;
    uint8_t conf = janusBbConfidenceFrom01(min(1.0f, swarmFutureStress / 2.0f + swarmGameSurprise * 0.30f), 45, 95);
    janusEmitBlackboardEvent(JE_MOTION, "tron_activity", conf, 72,
                             janusBbHash16("motion"), janusBbHash16("tron-game-audio"),
                             janusBbX10(swarmGameSurprise, 0.0f, 20.0f),
                             janusBbX10(swarmFutureStress, 0.0f, 20.0f),
                             (int16_t)swarmVirtualSector,
                             (int16_t)swarmPredictedSector,
                             14000UL);
  }

  if (now - lastMemoryMs >= JANUS_BLACKBOARD_MEMORY_MS * radioMul) {
    lastMemoryMs = now;
    uint8_t memUsed = 0;
    uint32_t memFresh = 0;
    for (uint8_t i = 0; i < JANUS_SWARM_MEMORY_SLOTS; ++i) {
      if (!swarmRecentMemory[i].used) continue;
      memUsed++;
      if (now - swarmRecentMemory[i].lastSeenMs < 60000UL) memFresh++;
    }
    uint8_t conf = (uint8_t)constrain(40 + memFresh * 5 + (int)(pVigor * 20.0f) - (int)(pFatigue * 10.0f), 10, 95);
    janusEmitBlackboardEvent(JE_AI_MEMORY, "tron_memory", conf, 34,
                             janusBbHash16("memory"), janusBbHash16("tron-mirror"),
                             (int16_t)memUsed, (int16_t)memFresh,
                             janusBbX10(pFatigue, 0.0f, 1.0f),
                             janusBbX10(pEntropy, 0.0f, 1.0f),
                             70000UL);
  }

  if (now - lastTaskMs >= JANUS_BLACKBOARD_TASK_MS * radioMul) {
    bool tired = pFatigue > 0.88f;
    bool rfTrouble = (rssi && rssi < JANUS_BLACKBOARD_WIFI_WEAK_RSSI);
    bool cortexRecoverHard = (janusBlackboardMood == JM_RECOVER && janusBlackboardDangerX100 > 72);
    bool overloaded = tired || rfTrouble || cortexRecoverHard;
    if (overloaded) {
      lastTaskMs = now;
      uint8_t conf = (uint8_t)constrain(55 + (int)(pFatigue * 35.0f) + (rfTrouble ? 12 : 0), 0, 98);
      uint8_t urg = (uint8_t)constrain(35 + (int)(pFatigue * 45.0f) + (rfTrouble ? 18 : 0), 0, 100);
      janusEmitBlackboardEvent(JE_TASK_NEED, "tron_recover_need", conf, urg,
                               janusBbHash16("task"), janusBbHash16("recover"),
                               janusBbX10(pFatigue, 0.0f, 1.0f),
                               janusBbX10(pVigor, 0.0f, 1.0f),
                               (int16_t)rssi,
                               (int16_t)(ESP.getFreeHeap() / 1024),
                               30000UL);
    } else if (pFatigue < 0.46f && janusBlackboardPolicyRx > 0) {
      lastTaskMs = now;
      janusEmitBlackboardEvent(JE_TASK_DONE, "tron_stable", 72, 18,
                               janusBbHash16("task"), janusBbHash16("stable"),
                               janusBbX10(pFatigue, 0.0f, 1.0f),
                               janusBbX10(pVigor, 0.0f, 1.0f),
                               (int16_t)rssi, 0,
                               35000UL);
    }
  }

  if (now - lastHashMs >= JANUS_BLACKBOARD_EVENT_HASH_MS * radioMul) {
    lastHashMs = now;
    janusEmitBlackboardEvent(JE_HASH, "tron_hash", totalH > 0 ? 82 : 35, totalH > 0 ? 38 : 18,
                             janusBbHash16("hash"), janusBbHash16("tron-miner"),
                             (int16_t)constrain((int32_t)totalH / 10, -32768L, 32767L),
                             (int16_t)best,
                             (int16_t)(shares > 32767UL ? 32767 : shares),
                             (int16_t)(rejects > 32767UL ? 32767 : rejects),
                             26000UL);
  }

  if (shares > lastAcceptSeen) {
    lastAcceptSeen = shares;
    janusEmitBlackboardEvent(JE_SOLO_ACCEPT, "tron_share", 96, 82,
                             janusBbHash16("hash-accept"), janusBbHash16("tron-miner"),
                             (int16_t)(shares > 32767UL ? 32767 : shares), (int16_t)best,
                             (int16_t)constrain((int32_t)totalH / 10, -32768L, 32767L), 0,
                             45000UL);
  }
  if (rejects > lastRejectSeen) {
    lastRejectSeen = rejects;
    janusEmitBlackboardEvent(JE_SOLO_REJECT, "tron_reject", 90, 86,
                             janusBbHash16("hash-reject"), janusBbHash16("tron-miner"),
                             (int16_t)(rejects > 32767UL ? 32767 : rejects), (int16_t)best,
                             (int16_t)rssi, 0,
                             45000UL);
  }

  if (rssi && rssi < JANUS_BLACKBOARD_WIFI_WEAK_RSSI && now - lastAlertMs >= 3500UL) {
    lastAlertMs = now;
    janusEmitBlackboardEvent(JE_WIFI_WEAK, "tron_radio", 78, 62,
                             janusBbHash16("radio"), janusBbHash16("wifi"),
                             (int16_t)rssi, (int16_t)WiFi.channel(), (int16_t)policyAge, 0,
                             15000UL);
  }

  uint32_t heap = ESP.getFreeHeap();
  if (heap < JANUS_BLACKBOARD_LOW_HEAP_BYTES && now - lastAlertMs >= 5000UL) {
    lastAlertMs = now;
    janusEmitBlackboardEvent(JE_LOW_HEAP, "tron_heap", 88, 70,
                             janusBbHash16("heap"), janusBbHash16("tron"),
                             (int16_t)(heap / 1024), (int16_t)(ESP.getMinFreeHeap() / 1024), 0, 0,
                             20000UL);
  }

  if (now - lastDiagMs > 15000UL) {
    lastDiagMs = now;
    Serial.printf("[BLACKBOARD/TRON] tx=%lu fail=%lu skip=%lu att=%u dedup=%u pol=%lu mood=%s radio=%u sensor=%u H=%lu T=%.1f P=%.1f mic=%.0f raw=%.0f gate=%.0f floor=%.0f fg=%lu best=%lu ageF=%.2f vig=%.2f order=%s\n",
                  (unsigned long)janusBlackboardEventTx,
                  (unsigned long)janusBlackboardEventFail,
                  (unsigned long)janusBlackboardEventSkip,
                  (unsigned)janusBlackboardLastAttention,
                  (unsigned)JANUS_BLACKBOARD_DEDUP_ENABLE,
                  (unsigned long)janusBlackboardPolicyRx,
                  janusBbMoodName(janusBlackboardMood),
                  (unsigned)janusBlackboardRadioRate,
                  (unsigned)janusBlackboardSensorRate,
                  (unsigned long)totalH,
                  swarmTempC,
                  swarmPressureHpa,
                  swarmMicRms,
                  micRawRms,
                  micLastGated,
                  micNoiseFloor,
                  (unsigned long)micFloorGuardHits,
                  (unsigned long)best,
                  pFatigue,
                  pVigor,
                  janusBlackboardOrder);
  }
#endif
}

#if JANUS_ENABLE_COLONY
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
void onColonyRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onColonyRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (!data || len < 2) return;
  int8_t rxRssi = 0;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
  if (info && info->rx_ctrl) rxRssi = info->rx_ctrl->rssi;
#endif

  if (len == sizeof(JanusPolicyPacket) && data[0] == 'J' && data[1] == 'P') {
    JanusPolicyPacket jp{};
    memcpy(&jp, data, sizeof(jp));
    janusHandleBlackboardPolicyPacket(jp);
    return;
  }

  if (len == sizeof(JanusTachyonProphecyPacket) && data[0] == 'T' && data[1] == 'P') {
    JanusTachyonProphecyPacket tp{};
    memcpy(&tp, data, sizeof(tp));
    onJanusTachyonProphecy(tp, rxRssi);
    return;
  }

  if (len == sizeof(JanusKenshiPacket) && data[0] == 'K' && data[1] == '2') {
    JanusKenshiPacket kp{};
    memcpy(&kp, data, sizeof(kp));
    onJanusKenshiPacket(kp, rxRssi);
    return;
  }

  if (len == sizeof(JanusAudioControlPacket) && data[0] == 'A' && data[1] == 'C') {
    JanusAudioControlPacket ac{};
    memcpy(&ac, data, sizeof(ac));
    janusHandleAudioControlPacket(&ac);
    return;
  }

  if (len == sizeof(JanusColonyPacket)) {
    JanusColonyPacket pkt;
    memcpy(&pkt, data, sizeof(pkt));
    if (memcmp(pkt.magic, "JANUS", 5) != 0) return;
    if (!strncmp(pkt.nodeId, JANUS_NODE_ID, sizeof(pkt.nodeId))) return;

    colonyPeersSeen++;
    colonyLastRxMs = millis();
    strlcpy(colonyLastPeer, pkt.nodeId, sizeof(colonyLastPeer));

    if (!strncmp(pkt.role, "BuzzLighter", sizeof(pkt.role))) {
      colonyMasterPresent = true;
      colonyLastMasterMs = millis();
    }

    if (pkt.bestBits > colonyBestPeerBits) colonyBestPeerBits = pkt.bestBits;
    if (pkt.hashRate > colonyBestPeerHash) colonyBestPeerHash = pkt.hashRate;

    if (pkt.rejects == 0 && pkt.aiBatch >= 80 && pkt.aiBatch <= 700 && pkt.hashRate > 0) {
      uint16_t cur = getColonyMiningBatch();
      setColonyMiningBatch((uint16_t)((cur * 3 + pkt.aiBatch) / 4));
      if (colonyConfidence < 95) colonyConfidence++;
    } else if (pkt.rejects > 0 && getColonyMiningBatch() > 80) {
      adjustColonyMiningBatch(-10);
      if (colonyConfidence > 5) colonyConfidence--;
    }
    return;
  }

  if (len == sizeof(JanusAgentRewardPacket) && data[0] == 'A' && data[1] == 'R') {
    JanusAgentRewardPacket ar{};
    memcpy(&ar, data, sizeof(ar));
    janusApplyAgentRewardPacket(&ar);
    return;
  }

  if (len == sizeof(EntropyReportV2) && data[0] == 'E' && data[1] == '2') {
    EntropyReportV2 er2{};
    memcpy(&er2, data, sizeof(er2));
    colonyEntropyRx++;
    colonyLastRxMs = millis();
    colonyPeerEntropy = colonyPeerEntropy * 0.84f + er2.local_entropy * 0.16f;
    swarmRememberNodeState(er2.nodeId, 1, er2.local_entropy, er2.prediction_error, er2.sync_hint, er2.fit, er2.values[0], er2.values[1], er2.values[2], er2.values[3]);
    if (er2.values[6] > colonyBestPeerBits) colonyBestPeerBits = (uint32_t)er2.values[6];
    if (er2.values[5] > colonyBestPeerHash) colonyBestPeerHash = (uint32_t)er2.values[5];
    return;
  }

  if (len == sizeof(GroundOrderPacket) && data[0] == 'G' && data[1] == 'O') {
    GroundOrderPacket go;
    memcpy(&go, data, sizeof(go));
    janusGroundOrderMode = go.mode ? 1 : 0;
    janusGroundOrderSector = go.sector;
    janusGroundOrderPriority = go.priority;
    janusGroundOrderMission = go.mission_id;
    janusGroundOrderFlags = go.flags;
    strlcpy(janusGroundOrderTarget, go.target[0] ? go.target : "BASE_HOLD", sizeof(janusGroundOrderTarget));
    janusGroundOrderAtMs = millis();
    janusGroundOrderSeq++;
    Serial.printf("[GROUNDOPS] ORDER rx mode=%u sector=%u prio=%u mission=%lu target=%s\n",
                  (unsigned)janusGroundOrderMode, (unsigned)janusGroundOrderSector,
                  (unsigned)janusGroundOrderPriority, (unsigned long)janusGroundOrderMission, go.target);
    return;
  }

  if (len == sizeof(JobPacket) && data[0] == 'J' && data[1] == 'B') {
    JobPacket job;
    memcpy(&job, data, sizeof(job));
    uint32_t nowJob = millis();
    colonyMasterPresent = true;
    colonyLastMasterMs = nowJob;

    // v8.31E4S: Buzz may send range_size=0 as discovery/heartbeat.
    // Never let that zero packet erase a valid active REMOTE job or force LOCAL flicker.
    if (job.range_size == 0) {
      if (!remoteJobFresh()) setMinerStatus("REMOTE_WAIT");
      return;
    }

    memcpy(remoteJobId, job.job_id, 8);
    memcpy(remoteHeader, job.header, 80);
    memcpy(remoteTarget, job.target, 32);
    remoteStartNonce = job.start_nonce;
    remoteRangeSize = job.range_size;
    remoteNonce = remoteStartNonce;
    remoteRangeEnd = remoteStartNonce + remoteRangeSize;
    remoteJobRxMs = nowJob;
    setRemoteJobActive(true);
    setMinerStatus("REMOTE");
    return;
  }

  if (len == sizeof(EntropyReport) && data[0] == 'E' && data[1] == 'R') {
    EntropyReport er;
    memcpy(&er, data, sizeof(er));
    colonyEntropyRx++;
    colonyLastRxMs = millis();
    colonyPeerEntropy = colonyPeerEntropy * 0.86f + er.local_entropy * 0.14f;
    char memId[24]; snprintf(memId, sizeof(memId), "ER1-%u", er.worker_id);
    swarmRememberNodeState(memId, 1, er.local_entropy, er.values[3], 0.0f, 0.0f, er.values[0], er.values[1], er.values[2], er.values[3]);
    return;
  }
}

#endif

uint8_t swarmMemoryFindSlot(const char* nodeId) {
  if (!nodeId || !nodeId[0]) nodeId = "node";
  for (uint8_t i = 0; i < JANUS_SWARM_MEMORY_SLOTS; ++i) {
    if (swarmRecentMemory[i].used && strncmp(swarmRecentMemory[i].nodeId, nodeId, sizeof(swarmRecentMemory[i].nodeId)) == 0) return i;
  }
  uint8_t freeSlot = 255;
  uint32_t oldest = 0xFFFFFFFFUL;
  uint8_t oldestSlot = 0;
  for (uint8_t i = 0; i < JANUS_SWARM_MEMORY_SLOTS; ++i) {
    if (!swarmRecentMemory[i].used && freeSlot == 255) freeSlot = i;
    if (swarmRecentMemory[i].lastSeenMs < oldest) { oldest = swarmRecentMemory[i].lastSeenMs; oldestSlot = i; }
  }
  return freeSlot != 255 ? freeSlot : oldestSlot;
}

void swarmRememberNodeState(const char* nodeId, uint8_t kind, float entropy, float predErr, float sync, float fit, float v0, float v1, float v2, float v3) {
  uint8_t slot = swarmMemoryFindSlot(nodeId);
  SwarmRecentMemorySlot& m = swarmRecentMemory[slot];
  m.used = true;
  strlcpy(m.nodeId, nodeId && nodeId[0] ? nodeId : "node", sizeof(m.nodeId));
  m.lastSeenMs = millis();
  m.kind = kind;
  m.entropy = isfinite(entropy) ? entropy : 0.0f;
  m.predError = isfinite(predErr) ? predErr : 0.0f;
  m.sync = isfinite(sync) ? sync : 0.0f;
  m.fit = isfinite(fit) ? fit : 0.0f;
  m.v0 = isfinite(v0) ? v0 : 0.0f;
  m.v1 = isfinite(v1) ? v1 : 0.0f;
  m.v2 = isfinite(v2) ? v2 : 0.0f;
  m.v3 = isfinite(v3) ? v3 : 0.0f;
}

void saveSwarmMemoryState(bool force) {
  uint32_t now = millis();
  if (!force && now - lastSwarmMemorySaveMs < JANUS_SWARM_MEMORY_SAVE_MS) return;
  lastSwarmMemorySaveMs = now;
  if (!janusSwarmMemoryPrefs.begin("swrm_mem", false)) return;
  janusSwarmMemoryPrefs.putBytes("slots", swarmRecentMemory, sizeof(swarmRecentMemory));
  janusSwarmMemoryPrefs.end();
}

void loadSwarmMemoryState() {
  if (!janusSwarmMemoryPrefs.begin("swrm_mem", false)) return;  // E4S2: create namespace on first boot, avoids NVS NOT_FOUND
  size_t n = janusSwarmMemoryPrefs.getBytesLength("slots");
  if (n == sizeof(swarmRecentMemory)) janusSwarmMemoryPrefs.getBytes("slots", swarmRecentMemory, sizeof(swarmRecentMemory));
  janusSwarmMemoryPrefs.end();
}

uint8_t swarmFindRemoteProphecySlot(const char* nodeId) {
  if (!nodeId || !nodeId[0]) nodeId = "node";
  for (uint8_t i = 0; i < JANUS_SWARM_REMOTE_PROPHECIES; ++i) {
    if (swarmRemoteProphecies[i].active && strncmp(swarmRemoteProphecies[i].nodeId, nodeId, sizeof(swarmRemoteProphecies[i].nodeId)) == 0) return i;
  }
  uint8_t freeSlot = 255;
  uint32_t oldest = 0xFFFFFFFFUL;
  uint8_t oldestSlot = 0;
  for (uint8_t i = 0; i < JANUS_SWARM_REMOTE_PROPHECIES; ++i) {
    if (!swarmRemoteProphecies[i].active && freeSlot == 255) freeSlot = i;
    if (swarmRemoteProphecies[i].lastSeenMs < oldest) { oldest = swarmRemoteProphecies[i].lastSeenMs; oldestSlot = i; }
  }
  return freeSlot != 255 ? freeSlot : oldestSlot;
}

void onJanusTachyonProphecy(const JanusTachyonProphecyPacket& tp, int8_t rxRssi) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  if (tp.magic[0] != 'T' || tp.magic[1] != 'P' || tp.version != 1) return;
  if (strncmp(tp.nodeId, JANUS_NODE_ID, sizeof(tp.nodeId)) == 0) return;
  uint8_t slot = swarmFindRemoteProphecySlot(tp.nodeId);
  JanusRemoteProphecyState& r = swarmRemoteProphecies[slot];
  r.active = true;
  strlcpy(r.nodeId, tp.nodeId[0] ? tp.nodeId : "node", sizeof(r.nodeId));
  r.lastSeenMs = millis();
  r.seq = tp.seq;
  r.sector = tp.sector & 7;
  r.predictedSector = tp.predictedSector & 7;
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
  r.rssi = rxRssi;
  tachyonProphecyRx++;
  swarmRememberNodeState(tp.nodeId, 2, tp.presence_now + tp.motion_now, tp.future_stress, (float)tp.confidence / 100.0f, 0.0f, tp.pred_presence_1, tp.pred_motion_1, tp.pred_presence_2, tp.pred_motion_2);
#endif
}

void onJanusKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi) {
  if (kp.magic[0] != 'K' || kp.magic[1] != '2' || kp.version != 1) return;
  if (strncmp(kp.nodeId, JANUS_NODE_ID, sizeof(kp.nodeId)) == 0) return;
  colonyLastRxMs = millis();
  colonyPeersSeen++;
  strlcpy(colonyLastPeer, kp.nodeId[0] ? kp.nodeId : "Kenshi", sizeof(colonyLastPeer));
  colonyPeerEntropy = colonyPeerEntropy * 0.88f + kp.entropy * 0.12f;
  swarmRemotePressure = swarmRemotePressure * 0.86f + kp.entropy * 0.02f + kp.confidence * 0.03f;
  kenshiBubbleRx++;
  swarmRememberNodeState(kp.nodeId, 3, kp.entropy, kp.values[4], kp.confidence, kp.values[5], kp.values[0], kp.values[1], kp.values[2], kp.values[3]);
  (void)rxRssi;
}

// Forward declarations for EchoBase microphone state.
// Real definitions live lower near EchoBase globals; tachyon/Kenshi code is above them.
extern float micNoiseFloor;
extern float micSignal;

float swarmMemoryEntropyBias() {
  float sum = 0.0f, wsum = 0.0f;
  uint32_t now = millis();
  for (uint8_t i = 0; i < JANUS_SWARM_MEMORY_SLOTS; ++i) {
    SwarmRecentMemorySlot& m = swarmRecentMemory[i];
    if (!m.used) continue;
    uint32_t age = now - m.lastSeenMs;
    if (age > 60000UL) continue;
    float w = constrain(1.0f - (float)age / 60000.0f, 0.05f, 1.0f) * (0.35f + constrain(m.sync, 0.0f, 1.2f));
    sum += m.entropy * w;
    wsum += w;
  }
  if (wsum <= 0.001f) return swarmLocalEntropy;
  return sum / wsum;
}

void updateSwarmTachyonPrediction() {
#if JANUS_TACHYON_PROPHECY_ENABLE
  static float lastMic = 0.0f;
  static float lastMotion = 0.0f;
  float remoteP = 0.0f, remoteM = 0.0f, wsum = 0.0f;
  uint32_t now = millis();
  for (uint8_t i = 0; i < JANUS_SWARM_REMOTE_PROPHECIES; ++i) {
    JanusRemoteProphecyState& r = swarmRemoteProphecies[i];
    if (!r.active) continue;
    uint32_t age = now - r.lastSeenMs;
    if (age > 20000UL) continue;
    float w = constrain((float)r.confidence / 100.0f, 0.05f, 1.0f) * constrain(1.0f - (float)age / 22000.0f, 0.05f, 1.0f);
    remoteP += r.pred_presence_1 * w;
    remoteM += r.pred_motion_1 * w;
    wsum += w;
  }
  if (wsum > 0.001f) { remoteP /= wsum; remoteM /= wsum; }
  float micNow = swarmMicRms;
  float motionNow = swarmGameSurprise + micSignal * 0.02f + (float)getMinerBestBitsSafe() * 0.015f;
  float trendMic = constrain(micNow - lastMic, -1800.0f, 1800.0f);
  float trendMotion = constrain(motionNow - lastMotion, -3.0f, 3.0f);
  lastMic = micNow;
  lastMotion = motionNow;
  float memE = swarmMemoryEntropyBias();
  swarmPredMic1 = max(0.0f, micNow * 0.62f + (micNow + trendMic * 1.2f) * 0.20f + remoteP * 0.18f);
  swarmPredMic2 = max(0.0f, swarmPredMic1 * 0.70f + (micNow + trendMic * 2.0f) * 0.16f + remoteP * 0.14f);
  swarmPredMic3 = max(0.0f, swarmPredMic2 * 0.74f + memE * 38.0f + remoteP * 0.12f);
  swarmPredMotion1 = max(0.0f, motionNow * 0.58f + (motionNow + trendMotion) * 0.24f + remoteM * 0.18f);
  swarmPredMotion2 = max(0.0f, swarmPredMotion1 * 0.72f + (motionNow + trendMotion * 1.6f) * 0.16f + remoteM * 0.12f);
  swarmPredMotion3 = max(0.0f, swarmPredMotion2 * 0.76f + memE * 0.12f + remoteM * 0.10f);
  swarmFutureStress = constrain(swarmPredictionError * 0.55f + janusPersonalEntropy * 0.35f + swarmRemotePressure * 0.25f + (janusAudioTxEnabled ? 0.08f : 0.0f), 0.0f, 3.0f);
  swarmProphecyConfidence = constrain(0.38f + (1.0f / (1.0f + swarmPredictionError)) * 0.34f + janusVigor * 0.16f + (wsum > 0.01f ? 0.12f : 0.0f), 0.0f, 1.0f);
  uint8_t sectorRaw = (uint8_t)((uint32_t)(micNow * 0.01f + swarmGameSurprise * 3.0f + millis() / 9000UL + getMinerBestBitsSafe()) & 7);
  swarmVirtualSector = sectorRaw;
  swarmPredictedSector = (uint8_t)((sectorRaw + (swarmPredMotion1 > motionNow + 0.35f ? 1 : 0) + (swarmFutureStress > 1.1f ? 2 : 0)) & 7);
  swarmJobState = swarmFutureStress > 1.1f ? 3 : (janusAudioTxEnabled ? 5 : (swarmProphecyConfidence > 0.72f ? 4 : 1));
  snprintf(swarmMemoryLine, sizeof(swarmMemoryLine), "MEM %.1f TP%lu/%lu K%lu/%lu", memE, (unsigned long)tachyonProphecyRx, (unsigned long)tachyonProphecyTx, (unsigned long)kenshiBubbleRx, (unsigned long)kenshiBubbleTx);
#endif
}

void sendSwarmTachyonProphecyPacket(bool force=false) {
#if JANUS_TACHYON_PROPHECY_ENABLE
  uint32_t now = millis();
  uint32_t interval = (swarmFutureStress > 1.1f || janusAudioTxEnabled) ? JANUS_TACHYON_TX_ALERT_MS : JANUS_TACHYON_TX_BG_MS;
  if (!force && now - tachyonLastTxMs < interval) return;
  JanusTachyonProphecyPacket tp{};
  tp.magic[0] = 'T'; tp.magic[1] = 'P';
  tp.version = 1;
  tp.flags = 0;
  if (swarmMicRms > max(260.0f, micNoiseFloor * 2.2f)) tp.flags |= 0x01;
  if (swarmGameSurprise > 0.75f) tp.flags |= 0x02;
  if (swarmFutureStress > 1.1f) tp.flags |= 0x04;
  if (tachyonProphecyRx > 0) tp.flags |= 0x08;
  strlcpy(tp.nodeId, JANUS_NODE_ID, sizeof(tp.nodeId));
  tp.seq = ++tachyonProphecySeq;
  tp.worker_id = swarmWorkerId();
  tp.uptime_ms = now;
  tp.horizon_ms = 2200;
  tp.sector = swarmVirtualSector;
  tp.predictedSector = swarmPredictedSector;
  tp.confidence = (uint8_t)constrain((int)(swarmProphecyConfidence * 100.0f), 0, 100);
  tp.jobState = swarmJobState;
  tp.presence_now = swarmMicRms;
  tp.motion_now = swarmGameSurprise;
  tp.pred_presence_1 = swarmPredMic1;
  tp.pred_motion_1 = swarmPredMotion1;
  tp.pred_presence_2 = swarmPredMic2;
  tp.pred_motion_2 = swarmPredMotion2;
  tp.pred_presence_3 = swarmPredMic3;
  tp.pred_motion_3 = swarmPredMotion3;
  tp.event_eta_ms = (swarmFutureStress > 1.1f) ? 350.0f : 9999.0f;
  tp.future_stress = swarmFutureStress;
  tp.swarm_pressure = swarmRemotePressure;
  esp_err_t tpErr = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&tp, sizeof(tp));
  if (tpErr == ESP_OK) {
    tachyonProphecyTx++;
    tachyonLastTxMs = now;
  } else {
    colonyTxFail++;
    janusColonyForcePeerRebuild("tachyon-tx-fail");
  }
#else
  (void)force;
#endif
}

void sendSwarmKenshiPacket(bool force=false) {
  uint32_t now = millis();
  if (!force && now - kenshiLastTxMs < JANUS_KENSHI_BUBBLE_TX_MS) return;
  JanusKenshiPacket kp{};
  kp.magic[0] = 'K'; kp.magic[1] = '2';
  kp.version = 1;
  kp.flags = 0x04; // virtual summary: this node abstracts the game/ground battle outside active Core view
  if (janusAudioTxEnabled || swarmMicRms > max(260.0f, micNoiseFloor * 2.2f)) kp.flags |= 0x01;
  if (swarmFutureStress > 1.1f) kp.flags |= 0x02;
  kp.flags |= 0x08; // future motion base ready / compatible with Blind Eye v2.8
  strlcpy(kp.nodeId, JANUS_NODE_ID, sizeof(kp.nodeId));
  kp.seq = ++kenshiBubbleSeq;
  kp.worker_id = swarmWorkerId();
  kp.uptime_ms = now;
  kp.activeBubbleNodes = (uint8_t)constrain((tachyonProphecyRx > 0 ? 1 : 0) + (janusAudioTxEnabled ? 1 : 0) + (colonyMasterPresent ? 1 : 0), 0, 8);
  kp.virtualNodes = JANUS_SWARM_MEMORY_SLOTS;
  kp.worldFlags = ((uint32_t)kp.flags) | ((uint32_t)getMinerBestBitsSafe() << 8) | ((uint32_t)(janusMoodCode & 0x0F) << 24);
  kp.sector = swarmVirtualSector;
  kp.predictedSector = swarmPredictedSector;
  kp.jobState = swarmJobState;
  kp.priority = (uint8_t)constrain((int)(swarmFutureStress * 55.0f + swarmProphecyConfidence * 80.0f + (janusAudioTxEnabled ? 30 : 0)), 0, 255);
  kp.rssi = (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : 0;
  kp.entropy = swarmLocalEntropy + janusPersonalEntropy * 2.0f;
  kp.activity = swarmGameSurprise + swarmMicRms / 1500.0f;
  kp.confidence = swarmProphecyConfidence;
  kp.values[0] = swarmMicRms;
  kp.values[1] = swarmPressureHpa;
  kp.values[2] = swarmTempC;
  kp.values[3] = swarmGameSurprise;
  kp.values[4] = swarmPredictionError;
  kp.values[5] = janusMood;
  esp_err_t kpErr = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&kp, sizeof(kp));
  if (kpErr == ESP_OK) {
    kenshiBubbleTx++;
    kenshiLastTxMs = now;
  } else {
    colonyTxFail++;
    janusColonyForcePeerRebuild("kenshi-tx-fail");
  }
}

void tachyonProphecyTick() {
  updateSwarmTachyonPrediction();
  sendSwarmTachyonProphecyPacket(false);
  sendSwarmKenshiPacket(false);
  saveSwarmMemoryState(false);
}

uint8_t janusColonyCurrentWifiChannel() {
#if JANUS_ENABLE_COLONY
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  esp_wifi_get_channel(&primary, &second);
  if (primary == 0 && WiFi.status() == WL_CONNECTED) primary = WiFi.channel();
  if (primary == 0) primary = 1;
  return primary;
#else
  return 1;
#endif
}

bool janusColonyAddBroadcastPeer(uint8_t primary, const char* reason) {
#if JANUS_ENABLE_COLONY
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, JANUS_BROADCAST_MAC, 6);
  peerInfo.channel = primary;   // must match Wi-Fi home channel, otherwise ESP-NOW send fails
  peerInfo.encrypt = false;

  esp_err_t r = esp_now_add_peer(&peerInfo);
  if (r == ESP_OK || r == ESP_ERR_ESPNOW_EXIST) {
    colonyPeerChannel = primary;
    colonyPeerLastFixMs = millis();
    colonyPeerRebuilds++;
    if (r == ESP_ERR_ESPNOW_EXIST && primary != colonyPeerChannel) {
      // Defensive fallback; normally unreachable after del/add path.
      colonyPeerChannel = primary;
    }
    Serial.printf("[COLONY] peer ready ch=%u rebuilds=%lu suppressed=%lu reason=%s\n",
                  (unsigned)primary,
                  (unsigned long)colonyPeerRebuilds,
                  (unsigned long)colonyPeerRebuildSuppressed,
                  reason ? reason : "-");
    return true;
  }

  colonyPeerChannel = 0;
  Serial.printf("[COLONY] peer add failed ch=%u err=%d reason=%s\n",
                (unsigned)primary, (int)r, reason ? reason : "-");
  return false;
#else
  (void)primary; (void)reason;
  return false;
#endif
}

void janusColonyForcePeerRebuild(const char* reason) {
#if JANUS_ENABLE_COLONY
  uint8_t primary = janusColonyCurrentWifiChannel();
  if (primary == 0) return;

  uint32_t now = millis();
  if (colonyPeerLastForceMs && now - colonyPeerLastForceMs < JANUS_COLONY_PEER_REBUILD_MIN_MS) {
    colonyPeerRebuildSuppressed++;
    return;
  }
  colonyPeerLastForceMs = now;

  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) {
    esp_now_del_peer(JANUS_BROADCAST_MAC);
  }
  colonyPeerChannel = 0;
  janusColonyAddBroadcastPeer(primary, reason ? reason : "force");
#else
  (void)reason;
#endif
}

void janusColonyEnsurePeer() {
#if JANUS_ENABLE_COLONY
  uint8_t primary = janusColonyCurrentWifiChannel();
  if (primary == 0) return;

  bool exists = esp_now_is_peer_exist(JANUS_BROADCAST_MAC);
  if (exists && colonyPeerChannel == primary) {
    return;
  }

  if (exists) {
    esp_now_del_peer(JANUS_BROADCAST_MAC);
  }
  colonyPeerChannel = 0;
  janusColonyAddBroadcastPeer(primary, "ensure");
#endif
}


bool janusSendEchoMicKeepalive(uint32_t now, bool force, const char* reason) {
#if JANUS_ENABLE_COLONY
  if (!force && colonyLastEchoMicKeepaliveMs && now - colonyLastEchoMicKeepaliveMs < JANUS_ECHOMIC_KEEPALIVE_MS) return false;
  colonyLastEchoMicKeepaliveMs = now;
  if (WiFi.status() != WL_CONNECTED) return false;
  janusColonyEnsurePeer();

  float pEntropy = 0.0f, pMood = 0.0f, pFatigue = 0.0f, pVigor = 0.0f;
  uint8_t pMoodCode = 0;
  janusPersonalSnapshot(pEntropy, pMood, pFatigue, pVigor, pMoodCode);

  // v8.31E4: Core2 has two AUDIO meanings:
  // 1) live radio stream A/F, active only after Core2 sends A/C ON;
  // 2) persistent EchoMic node presence in AUDIO page / HOME slot.
  // The E2 mirror was usually enough, but under RF/NAS/audio pressure Core2 could age it out.
  // Send a tiny JANUS heartbeat as an independent keepalive so AUDIO does not look dead.
  JanusColonyPacket hb = {};
  memcpy(hb.magic, "JANUS", 6);
  strlcpy(hb.nodeId, "EchoMic", sizeof(hb.nodeId));
  strlcpy(hb.role, "AudioMic", sizeof(hb.role));
  hb.seq = ++colonySeq;
  hb.hashRate = 0;
  hb.shares = swarmMicFrames;
  hb.rejects = swarmMicFails;
  hb.bestBits = 0;
  hb.diff = swarmLocalEntropy;
  hb.targetBits = janusAudioTxEnabled ? 1 : 0;
  hb.aiBatch = (uint16_t)constrain((int)(janusAudioTxFrames & 0xFFFF), 0, 65535);
  hb.aiHint = janusAudioTxEnabled ? 3 : 1;
  hb.jobAgeMs = janusAudioTxEnabled && janusAudioTxLastControlMs ? now - janusAudioTxLastControlMs : 0;
  hb.rssi = (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : 0;
  hb.uptime = now / 1000UL;

  esp_err_t hbErr = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&hb, sizeof(hb));
  if (hbErr == ESP_OK) {
    colonyEchoMicHbTxOk++;
    colonyTxOk++;
  } else {
    colonyEchoMicHbTxFail++;
    colonyTxFail++;
    janusColonyForcePeerRebuild("echo-mic-hb-tx-fail");
  }

  EntropyReportV2 mic2 = {};
  mic2.magic[0] = 'E'; mic2.magic[1] = '2';
  mic2.worker_id = swarmWorkerId();
  strlcpy(mic2.nodeId, "EchoMic", sizeof(mic2.nodeId));
  mic2.local_entropy = swarmLocalEntropy;
  mic2.prediction_error = swarmPredictionError;
  mic2.sync_hint = janusClampF(0.48f + pVigor * 0.22f - pFatigue * 0.10f, 0.0f, 1.5f);
  mic2.fit = janusClampF(0.35f + min(0.55f, swarmMicRms / 1800.0f) - min(0.25f, (float)swarmMicFails / 200.0f), 0.0f, 1.0f);
  mic2.sensor_flags = 0x03;  // mic + env/BPS; keeps Core2 AUDIO semantic slot alive
  mic2.values[0] = swarmMicRms;
  mic2.values[1] = swarmPressureHpa;
  mic2.values[2] = swarmTempC;
  mic2.values[3] = swarmMicPeak;
  mic2.values[4] = (float)swarmMicFails;
  mic2.values[5] = (float)swarmMicFrames;
  mic2.values[6] = (float)janusAudioTxFrames;
  mic2.values[7] = pMood * 100.0f;
  mic2.uptime_ms = now;

  esp_err_t mic2Err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&mic2, sizeof(mic2));
  if (mic2Err == ESP_OK) {
    colonyEchoMicTxOk++;
    colonyTxOk++;
  } else {
    colonyEchoMicTxFail++;
    colonyTxFail++;
    janusColonyForcePeerRebuild("echo-mic-e2-tx-fail");
  }

  static uint32_t lastMicDbg = 0;
  if (now - lastMicDbg > 5000UL) {
    lastMicDbg = now;
    Serial.printf("[ECHO-MIC] keepalive reason=%s rms=%.1f entropy=%.2f pressure=%.1f temp=%.1f live=%u af=%lu e2Ok=%lu e2Fail=%lu hbOk=%lu hbFail=%lu peerCh=%u\n",
                  reason ? reason : "tick", swarmMicRms, swarmLocalEntropy, swarmPressureHpa, swarmTempC,
                  janusAudioTxEnabled ? 1U : 0U,
                  (unsigned long)janusAudioTxFrames,
                  (unsigned long)colonyEchoMicTxOk,
                  (unsigned long)colonyEchoMicTxFail,
                  (unsigned long)colonyEchoMicHbTxOk,
                  (unsigned long)colonyEchoMicHbTxFail,
                  (unsigned)colonyPeerChannel);
  }
  return hbErr == ESP_OK || mic2Err == ESP_OK;
#else
  (void)now; (void)force; (void)reason; return false;
#endif
}

void janusColonyBegin() {
#if JANUS_ENABLE_COLONY
  esp_now_deinit();
  if (esp_now_init() != ESP_OK) {
    Serial.println("[COLONY] ESP-NOW init failed");
    return;
  }
  esp_now_register_recv_cb(onColonyRecv);
  janusColonyEnsurePeer();
  Serial.printf("[COLONY] ready node=%s channel=%d\n", JANUS_NODE_ID, WiFi.channel());
#endif
}

void janusColonyTick() {
#if JANUS_ENABLE_COLONY
  uint32_t now = millis();

  // v8.31E4: EchoMic has its own keepalive, independent of full entropy cadence and live A/F stream.
  janusSendEchoMicKeepalive(now, false, "tick");

  if (colonyMasterPresent && now - colonyLastMasterMs > JANUS_MASTER_TIMEOUT_MS) {
    colonyMasterPresent = false;
    setRemoteJobActive(false);
  }

  if (now - colonyLastTxMs >= JANUS_COLONY_PULSE_MS) {
    colonyLastTxMs = now;
    janusColonyEnsurePeer();

    JanusColonyPacket pkt = {};
    memcpy(pkt.magic, "JANUS", 6);
    strlcpy(pkt.nodeId, JANUS_NODE_ID, sizeof(pkt.nodeId));
    strlcpy(pkt.role, "GroundOps", sizeof(pkt.role));
    pkt.seq = ++colonySeq;
    pkt.hashRate = max(minerRemoteHashrate, stratumConnected ? minerRealHashrate : minerLocalHashrate);
    pkt.shares = getMinerSharesSafe() + colonyAgentShareRewardsRx;  // real pool ACCEPTs only: local accepts + Buzz-confirmed remote accepts
    pkt.rejects = getMinerRejectsSafe();
    pkt.bestBits = getMinerBestBitsSafe();
    pkt.diff = (float)minerCurrentDiff;
    pkt.targetBits = minerShareTargetBits;
    pkt.aiBatch = getAdaptiveMiningBatch();
    if (colonyAgentLastRewardMs && now - colonyAgentLastRewardMs < 30000UL && colonyAgentShareRewardsRx > 0) {
      pkt.aiHint = 3;  // recent real pool ACCEPT confirmed by Buzz; Core2 can treat this as Janus progress
    } else {
      pkt.aiHint = getMinerRejectsSafe() > getMinerSharesSafe() + 2 ? 2 : (getRemoteJobActive() ? 3 : 1);
    }
    pkt.jobAgeMs = minerLastJobMs ? (now - minerLastJobMs) : (getRemoteJobActive() ? now - remoteJobRxMs : 0);
    pkt.rssi = (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : 0;
    pkt.uptime = now / 1000;

    esp_err_t r = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
    if (r != ESP_OK) {
      colonyTxFail++;
      Serial.printf("[COLONY] heartbeat err=%d ch=%d peerCh=%u fail=%lu\n",
                    (int)r, WiFi.channel(), (unsigned)colonyPeerChannel, (unsigned long)colonyTxFail);
      janusColonyForcePeerRebuild("heartbeat-tx-fail");
    } else {
      colonyTxOk++;
    }
  }

  if (now - colonyLastEntropyMs >= JANUS_COLONY_ENTROPY_MS) {
    colonyLastEntropyMs = now;
    EntropyReport er = {};
    er.magic[0] = 'E'; er.magic[1] = 'R';
    er.worker_id = swarmWorkerId();
    er.local_entropy = swarmLocalEntropy;
    er.sensor_flags = 0x0F;
    er.values[0] = swarmMicRms;
    er.values[1] = swarmPressureHpa;
    er.values[2] = swarmGameSurprise;
    er.values[3] = swarmPredictionError;
    esp_err_t erErr = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&er, sizeof(er));
    if (erErr != ESP_OK) {
      colonyTxFail++;
      janusColonyForcePeerRebuild("entropy-er-tx-fail");
    }

    float pEntropy, pMood, pFatigue, pVigor;
    uint8_t pMoodCode;
    janusPersonalSnapshot(pEntropy, pMood, pFatigue, pVigor, pMoodCode);

    EntropyReportV2 er2 = {};
    er2.magic[0] = 'E'; er2.magic[1] = '2';
    er2.worker_id = swarmWorkerId();
    // Keep the semantic slot compatible with Core2's SWARM/ATOM detector.
    strlcpy(er2.nodeId, JANUS_NODE_ID, sizeof(er2.nodeId));
    er2.local_entropy = swarmLocalEntropy + pEntropy * 2.0f + pFatigue * 0.55f;
    er2.prediction_error = swarmPredictionError + pEntropy * 0.12f + pFatigue * 0.10f;
    er2.sync_hint = janusClampF((1.0f / (1.0f + swarmPredictionError)) * 0.58f + pMood * 0.24f + pVigor * 0.18f - pFatigue * 0.08f, 0.0f, 1.5f);
    er2.fit = janusClampF(0.40f + pMood * 0.34f + pVigor * 0.24f - pFatigue * 0.18f + min(0.28f, swarmLocalEntropy * 0.015f), 0.0f, 1.5f);
    er2.sensor_flags = 0x3F;  // mic + env + game + prediction + personal-agent state
    er2.values[0] = swarmMicRms;
    er2.values[1] = swarmPressureHpa;
    er2.values[2] = swarmTempC;
    er2.values[3] = swarmGameSurprise + pEntropy;
    er2.values[4] = er2.prediction_error;
    er2.values[5] = stratumConnected ? minerRealHashrate : (minerRemoteHashrate ? minerRemoteHashrate : minerLocalHashrate);
    er2.values[6] = getMinerBestBitsSafe();
    er2.values[7] = pMood * 100.0f;
    er2.uptime_ms = now;
    esp_err_t er2Err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&er2, sizeof(er2));
    if (er2Err != ESP_OK) {
      colonyTxFail++;
      janusColonyForcePeerRebuild("entropy-e2-tx-fail");
    }

    // v8.31E4: EchoMic AUDIO presence is handled by janusSendEchoMicKeepalive().
    // Force one mirror on the full entropy cadence too, so Core2 receives fresh mic/env values.
    janusSendEchoMicKeepalive(now, true, "entropy");
  }
#endif
}

void janusColonyAITick() {
#if JANUS_ENABLE_COLONY
  static uint32_t lastAiMs = 0;
  uint32_t now = millis();
  if (now - lastAiMs < 3000) return;
  lastAiMs = now;

  // Mode 0 observe, 1 stabilize, 2 learn-from-peer, 3 push.
  if (getMinerRejectsSafe() > 0) {
    colonyAIMode = 1;
    if (getColonyMiningBatch() > 65) adjustColonyMiningBatch(-5);
    if (colonyConfidence > 5) colonyConfidence -= 2;
  } else if (colonyLastRxMs && (now - colonyLastRxMs < 10000) && colonyBestPeerBits >= getMinerBestBitsSafe()) {
    colonyAIMode = 2;
    if (getColonyMiningBatch() < 700) adjustColonyMiningBatch(5);
    if (colonyConfidence < 95) colonyConfidence++;
  } else if (stratumConnected && minerLastJobMs > 0) {
    colonyAIMode = 3;
    if ((minerRealHashrate > 0 || minerRemoteHashrate > 0) && getMinerRejectsSafe() == 0 && getColonyMiningBatch() < 520) adjustColonyMiningBatch(5);
  } else {
    colonyAIMode = 0;
  }

  setColonyMiningBatch(getColonyMiningBatch());
#endif
}


String extranonce1 = "";
int extranonce2_size = 0;
uint32_t extranonce2 = 0;

void initSwarmWorkerName() {
  uint64_t mac = ESP.getEfuseMac();
  uint32_t chip = (uint32_t)(mac & 0xFFFFFF);
  snprintf(BTC_WORKER, sizeof(BTC_WORKER), "Swarm_%06lX", (unsigned long)chip);
  snprintf(JANUS_NODE_ID, sizeof(JANUS_NODE_ID), "%s", BTC_WORKER);
  snprintf(MINER_USER, sizeof(MINER_USER), "%s.%s", BTC_WALLET, BTC_WORKER);
}

String minerUserString() {
  if (MINER_USER[0] == '\0') initSwarmWorkerName();
  return String(MINER_USER);
}

void hexStringToBytes(String hex, uint8_t *bytes) {
  for (int i = 0; i < hex.length(); i += 2) {
    String byteString = hex.substring(i, i + 2);
    bytes[i / 2] = (uint8_t)strtol(byteString.c_str(), NULL, 16);
  }
}

void reverse_bytes(uint8_t *data, int len) {
  for(int i=0; i<len/2; i++) {
    uint8_t t = data[i]; 
    data[i] = data[len-1-i]; 
    data[len-1-i] = t;
  }
}

// Exact NerdMiner prevhash transform: byte-swap inside every 32-bit word.
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

// Extranonce2 must be little-endian hex exactly like NerdMiner/Buzz.
void formatExtranonce2LE(uint64_t value, uint8_t sizeBytes, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (sizeBytes == 0) return;
  if (sizeBytes > 8) sizeBytes = 8;
  size_t need = (size_t)sizeBytes * 2 + 1;
  if (outSize < need) return;
  for (uint8_t i = 0; i < sizeBytes; ++i) {
    uint8_t b = (uint8_t)((value >> (8 * i)) & 0xFF);
    snprintf(out + i * 2, outSize - i * 2, "%02x", b);
  }
  out[sizeBytes * 2] = '\0';
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

void writeLE32(uint8_t* p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

void hashToShareOrder(const uint8_t in[32], uint8_t out[32]) {
  // Buzz/NerdMiner-compatible gate:
  // SHA256d output is reversed into display/share order, then compared with BE target.
  for (int i = 0; i < 32; ++i) out[i] = in[31 - i];
}

// Bitcoin difficulty-1 share target, big-endian:
// 00000000FFFF0000000000000000000000000000000000000000000000000000
const uint8_t BTC_DIFF1_TARGET[32] = {
  0x00,0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

void setShareTargetFromDifficulty(double diff) {
  if (diff <= 0.0) diff = 1.0;
  memset(minerShareTarget, 0, sizeof(minerShareTarget));

  // Exact share target used for local prefiltering:
  //   target = BTC_DIFF1_TARGET / difficulty
  // For fractional low-diff pools (0.0001, 0.001, ...), this is a multiplication
  // by round(1/diff). That is enough for public-pool/NerdMiner-style low diff values.
  if (diff < 1.0) {
    uint32_t mul = (uint32_t)(1.0 / diff + 0.5);
    if (mul < 1) mul = 1;
    uint32_t carry = 0;
    for (int i = 31; i >= 0; --i) {
      uint64_t val = (uint64_t)BTC_DIFF1_TARGET[i] * mul + carry;
      minerShareTarget[i] = (uint8_t)(val & 0xFF);
      carry = (uint32_t)(val >> 8);
    }
    if (carry) memset(minerShareTarget, 0xFF, sizeof(minerShareTarget));
  } else {
    uint32_t div = (uint32_t)(diff + 0.5);
    if (div < 1) div = 1;
    uint64_t rem = 0;
    for (int i = 0; i < 32; i++) {
      uint64_t cur = (rem << 8) | BTC_DIFF1_TARGET[i];
      minerShareTarget[i] = (uint8_t)(cur / div);
      rem = cur % div;
    }
  }
  minerShareTargetBits = countLeadingZeroBits(minerShareTarget);
}

bool hashMeetsShareTarget(const uint8_t hash[32]) {
  // Exact 256-bit big-endian comparison: hash <= target.
  // Leading-zero-bit checks are only approximate and were the source of false submits.
  for (int i = 0; i < 32; i++) {
    if (hash[i] < minerShareTarget[i]) return true;
    if (hash[i] > minerShareTarget[i]) return false;
  }
  return true;
}


bool runRemoteColonyMining(mbedtls_sha256_context* ctx,
                           uint8_t hash1[32],
                           uint8_t hash2[32],
                           uint32_t& hashesThisSecond) {
#if JANUS_ENABLE_COLONY
  if (!colonyMasterPresent || !getRemoteJobActive()) return false;
  if (millis() - remoteJobRxMs > JANUS_REMOTE_JOB_TTL_MS) {
    setRemoteJobActive(false);
    return false;
  }

  uint16_t rawBatch = getAdaptiveMiningBatch();
  uint16_t batch = janusAudioTxEnabled ? constrain(rawBatch, (uint16_t)16, (uint16_t)120)
                                       : constrain(rawBatch, (uint16_t)60, (uint16_t)700);
  uint8_t header[80];
  uint8_t jobId[8];
  uint8_t target[32];
  uint8_t shareHash[32];
  uint32_t localEnd;

  memcpy(header, remoteHeader, 80);
  memcpy(jobId, remoteJobId, 8);
  memcpy(target, remoteTarget, 32);
  localEnd = remoteRangeEnd;

  for (uint16_t i = 0; i < batch; i++) {
    if (remoteNonce == localEnd) {
      setRemoteJobActive(false);
      return true;
    }

    uint32_t n = remoteNonce++;
    writeLE32(header + 76, n);

    mbedtls_sha256_starts(ctx, 0); mbedtls_sha256_update(ctx, header, 80); mbedtls_sha256_finish(ctx, hash1);
    mbedtls_sha256_starts(ctx, 0); mbedtls_sha256_update(ctx, hash1, 32); mbedtls_sha256_finish(ctx, hash2);

    hashesThisSecond++;
    minerTotalHashes++;

    // v8.15B: remote Buzz worker mirrors Buzz verification exactly.
    // raw SHA256d -> reversed/display hash -> BE target.
    hashToShareOrder(hash2, shareHash);
    uint16_t bits = countLeadingZeroBits(shareHash);
    updateMinerBestBitsSafe(bits);

    if ((bits >= countLeadingZeroBits(target)) && hashMeetsTargetBytes(shareHash, target)) {
      ShareResponse sr = {};
      sr.magic[0] = 'S'; sr.magic[1] = 'R';
      memcpy(sr.job_id, jobId, 8);
      sr.nonce = n;
      sr.worker_id = swarmWorkerId();
      esp_err_t srErr = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&sr, sizeof(sr));
      if (srErr != ESP_OK) {
        colonyTxFail++;
        janusColonyForcePeerRebuild("share-response-tx-fail");
      }
      remoteSharesSent++;
      minerLastSubmitMs = millis();
      janusMinerGameReward(1, bits);  // praise/ticket only; no buff until Buzz returns A/R with deltaShares>0 after pool ACCEPT
      Serial.printf("[REMOTE] share sent nonce=%08lx bits=%u total=%lu\n",
                    (unsigned long)n, bits, (unsigned long)remoteSharesSent);
    }
  }

  minerLocalFallback = false;
  setMinerStatus("REMOTE");
  return true;
#else
  return false;
#endif
}

// ============================================================
// CORE 0: VAULT (TRUE STRATUM MINER V25)
// ============================================================
void microMinerTask(void *pvParameters) {
  WiFiClient client;
  client.setTimeout(80);

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);

  uint32_t hashesThisSecond = 0;
  uint32_t localHashesThisSecond = 0;
  uint32_t lastHashTick = millis();

  uint8_t blockHeader[80];
  uint8_t localHeader[80];
  uint8_t hash1[32];
  uint8_t hash2[32];
  uint8_t shareHash2[32];

  String currentJobId = "";
  String ntimeHex = "";
  char en2Hex[17] = "";
  uint32_t nonce = 0;
  uint32_t localNonce = esp_random();
  bool jobReady = false;
  bool subscribed = false;
  bool authorized = false;
  uint32_t connectedAtMs = 0;
  setShareTargetFromDifficulty(minerCurrentDiff);

  // Seed local real hash header with wallet/device/time.
  memset(localHeader, 0, sizeof(localHeader));
  writeLE32(localHeader + 0, 0x20000000UL);
  uint8_t seed[160];
  snprintf((char*)seed, sizeof(seed), "JANUS_LOCAL_PREV|%s|%08lx|%lu",
           BTC_WALLET, (unsigned long)(uint32_t)ESP.getEfuseMac(), (unsigned long)millis());
  mbedtls_sha256(seed, strlen((char*)seed), localHeader + 4, 0);
  snprintf((char*)seed, sizeof(seed), "JANUS_LOCAL_MERKLE|%s|%s|%lu",
           BTC_WALLET, BTC_WORKER, (unsigned long)millis());
  mbedtls_sha256(seed, strlen((char*)seed), localHeader + 36, 0);
  writeLE32(localHeader + 72, 0x1e0ffff0UL);

  while(true) {
    // REMOTE colony mode has priority when Buzz master broadcasts work.
    // Local Stratum socket is closed while we work on master's ranges.
    if (colonyMasterPresent && getRemoteJobActive() && remoteJobFresh()) {
      if (client.connected()) client.stop();
      stratumConnected = false;
      jobReady = false;
      subscribed = false;
      authorized = false;
      if (runRemoteColonyMining(&ctx, hash1, hash2, hashesThisSecond)) {
        uint32_t now = millis();
        if (now - lastHashTick >= 1000) {
          minerRemoteHashrate = hashesThisSecond;
          minerLocalHashrate = 0;
          minerRealHashrate = 0;
          hashesThisSecond = 0;
          localHashesThisSecond = 0;
          lastHashTick = now;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        continue;
      }
    }

    // Single-mode policy: no parallel local/pool farming while Buzz is between fresh jobs.
    if (shouldStayRemoteMode() && !remoteJobFresh()) {
      if (client.connected()) client.stop();
      stratumConnected = false;
      jobReady = false;
      subscribed = false;
      authorized = false;
      minerRemoteHashrate = 0;
      minerLocalHashrate = 0;
      minerRealHashrate = 0;
      setMinerStatus("REMOTE_WAIT");
      vTaskDelay(pdMS_TO_TICKS(80));
      continue;
    }

    if(WiFi.status() != WL_CONNECTED) {
      if (client.connected()) client.stop();
      stratumConnected = false;
      jobReady = false;
      subscribed = false;
      authorized = false;
      minerLocalFallback = true;
      setMinerStatus("WIFI");
      uint32_t nowMs = millis();
      if (nowMs - lastWifiKickMs > minerWifiRetryDelayMs) {
        lastWifiKickMs = nowMs;
        minerWifiReconnects++;
        Serial.printf("[MINER] WiFi reconnect #%lu delay=%lu to SSID: %s\n",
                      (unsigned long)minerWifiReconnects,
                      (unsigned long)minerWifiRetryDelayMs,
                      WIFI_SSID);
        // v8.31E4S: do not force disconnect every 5s; that caused 4WAY_HANDSHAKE_TIMEOUT loops.
        WiFi.begin(WIFI_SSID, WIFI_PASS);
        minerWifiRetryDelayMs = min((uint32_t)JANUS_COLONY_WIFI_RETRY_MAX_MS,
                                    (uint32_t)(minerWifiRetryDelayMs + 15000UL));
      }
      vTaskDelay(pdMS_TO_TICKS(500));
    } else if(!client.connected()) {
      minerWifiRetryDelayMs = JANUS_COLONY_WIFI_RETRY_BASE_MS;
      stratumConnected = false;
      jobReady = false;
      subscribed = false;
      authorized = false;
      minerLocalFallback = true;

      if(client.connect(POOL_HOST, POOL_PORT)) {
        stratumConnected = true;
        minerLastPoolConnectMs = millis();
        setMinerStatus("AUTH");
        Serial.printf("[MINER] Pool connected: %s:%u as %s\n", POOL_HOST, POOL_PORT, MINER_USER);
        client.setTimeout(80);
        connectedAtMs = millis();
        // NerdMiner V2 handshake path for public-pool TCP stratum.
        client.print("{\"id\":10,\"method\":\"mining.suggest_difficulty\",\"params\":[0.0001]}\n");
        client.print("{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"NerdMinerV2\"]}\n");
        Serial.println("[MINER] Subscribe sent as NerdMinerV2");
      } else {
        vTaskDelay(pdMS_TO_TICKS(400));
      }
    }

    uint8_t linesPerTick = 0;
    while(client.connected() && client.available() && linesPerTick < 4) {
      linesPerTick++;
      String line = client.readStringUntil('\n');
      line.trim();
      if (!line.length()) continue;

      StaticJsonDocument<3072> doc;
      DeserializationError err = deserializeJson(doc, line);
      if (err) continue;

      if (doc["id"] == 1 && doc["result"].is<JsonArray>()) {
        extranonce1 = doc["result"][1].as<String>();
        extranonce2_size = doc["result"][2].as<int>();
        if (extranonce2_size <= 0 || extranonce2_size > 8) extranonce2_size = 4;
        subscribed = true;
        Serial.printf("[MINER] Subscribed extranonce2_size=%d extranonce1=%s\n", extranonce2_size, extranonce1.c_str());
        String loginStr = String("{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"") + minerUserString() + "\",\"x\"]}\n";
        client.print(loginStr);
        Serial.printf("[MINER] Authorize sent: %s\n", MINER_USER);
      }

      if (doc["id"] == 2) {
        if (doc["result"] == true) {
          authorized = true;
          setMinerStatus("AUTH OK");
          Serial.println("[MINER] AUTH OK");
        } else {
          authorized = false;
          setMinerStatus("AUTH ERR");
          const char* errText = doc["error"][1] | "authorize failed";
          Serial.printf("[MINER] AUTH REJECT err=%s\n", errText);
        }
      }

      const char* method = doc["method"] | "";
      if (!strcmp(method, "mining.set_difficulty")) {
        minerCurrentDiff = doc["params"][0].as<double>();
        if (minerCurrentDiff <= 0.0) minerCurrentDiff = 1.0;
        setShareTargetFromDifficulty(minerCurrentDiff);
        Serial.printf("[MINER] Pool difficulty=%.8f targetBits=%u\n", minerCurrentDiff, minerShareTargetBits);
      }

      if (!strcmp(method, "mining.notify")) {
        JsonArray params = doc["params"];
        if (params.size() < 8) continue;

        currentJobId = params[0].as<String>();
        String prevhashHex = params[1].as<String>();
        String coinb1Hex = params[2].as<String>();
        String coinb2Hex = params[3].as<String>();
        JsonArray merkleBranch = params[4];
        String versionHex = params[5].as<String>();
        String nbitsHex = params[6].as<String>();
        ntimeHex = params[7].as<String>();

        if (!extranonce2_size) extranonce2_size = 4;
        extranonce2++;
        formatExtranonce2LE((uint64_t)extranonce2, (uint8_t)extranonce2_size, en2Hex, sizeof(en2Hex));

        String coinbase = coinb1Hex + extranonce1 + String(en2Hex) + coinb2Hex;
        if ((coinbase.length() & 1) || coinbase.length() > 768) continue;
        if (ntimeHex.length() != 8 || nbitsHex.length() != 8 || versionHex.length() != 8 || prevhashHex.length() != 64) continue;
        int cbLen = coinbase.length() / 2;
        if (cbLen <= 0 || cbLen > 384) continue;

        uint8_t cbBytes[384];
        hexStringToBytes(coinbase, cbBytes);

        uint8_t mRoot[32];
        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, cbBytes, cbLen); mbedtls_sha256_finish(&ctx, mRoot);
        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, mRoot, 32); mbedtls_sha256_finish(&ctx, mRoot);

        for (JsonVariant v : merkleBranch) {
          String branchHex = v.as<String>();
          if (branchHex.length() != 64) continue;
          uint8_t branchBytes[32];
          hexStringToBytes(branchHex, branchBytes);
          uint8_t concat[64];
          memcpy(concat, mRoot, 32);
          memcpy(concat + 32, branchBytes, 32);
          mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, concat, 64); mbedtls_sha256_finish(&ctx, mRoot);
          mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, mRoot, 32); mbedtls_sha256_finish(&ctx, mRoot);
        }

        memset(blockHeader, 0, 80);
        hexStringToBytes(versionHex, blockHeader);
        reverse_bytes(blockHeader, 4);

        // v8.12 direct miner uses the same proven Buzz/NerdMiner layout:
        // prevhash = word byte-swap, merkle = mbedtls SHA256d bytes without final reverse,
        // ntime/nbits = little-endian, nonce = little-endian in header.
        hexStringToBytes(prevhashHex, blockHeader + 4);
        reverse_word_bytes(blockHeader + 4, 32);

        memcpy(blockHeader + 36, mRoot, 32);

        uint8_t ntimeLE[4];
        uint8_t nbitsLE[4];
        hexStringToBytes(ntimeHex, ntimeLE); reverse_bytes(ntimeLE, 4);
        hexStringToBytes(nbitsHex, nbitsLE); reverse_bytes(nbitsLE, 4);
        memcpy(blockHeader + 68, ntimeLE, 4);
        memcpy(blockHeader + 72, nbitsLE, 4);

        nonce = esp_random();
        jobReady = true;
        minerLocalFallback = false;
        minerLastJobMs = millis();
        setMinerStatus("HASH");
        Serial.printf("[MINER] Job %s ready, hashing...\n", currentJobId.c_str());
      }

      if (doc["id"] == 4) {
        if (doc["result"] == true) {
          uint32_t sharesNow = incMinerSharesSafe();
          minerLastAcceptMs = millis();
          janusMinerGameReward(3, (uint16_t)getMinerBestBitsSafe());
          setMinerStatus("ACCEPT");
          Serial.printf("[MINER] ACCEPT share=%lu reject=%lu\n", (unsigned long)sharesNow, (unsigned long)getMinerRejectsSafe());
        } else {
          uint32_t rejectsNow = incMinerRejectsSafe();
          setMinerStatus("REJECT");
          janusPersonalNudge(6, 0);
          const char* errText = doc["error"][1] | "unknown";
          Serial.printf("[MINER] REJECT attempts=%lu rejects=%lu err=%s\n", (unsigned long)minerSubmitAttempts, (unsigned long)rejectsNow, errText);
        }
      }
    }

    if (client.connected() && (!subscribed || !authorized) && connectedAtMs && millis() - connectedAtMs > 12000) {
      Serial.println("[MINER] Handshake timeout, reconnecting");
      client.stop();
      stratumConnected = false;
      jobReady = false;
      subscribed = false;
      authorized = false;
      vTaskDelay(pdMS_TO_TICKS(250));
      continue;
    }

    if (jobReady && client.connected()) {
      uint16_t localBatch = getAdaptiveMiningBatch();
      for(int batch = 0; batch < localBatch; batch++) {
        nonce++;
        writeLE32(blockHeader + 76, nonce);

        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, blockHeader, 80); mbedtls_sha256_finish(&ctx, hash1);
        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, hash1, 32); mbedtls_sha256_finish(&ctx, hash2);
        hashesThisSecond++;
        minerTotalHashes++;

        // v8.15B: same share-order gate as the working Buzz/Core2 worker path.
        hashToShareOrder(hash2, shareHash2);
        uint16_t bits = countLeadingZeroBits(shareHash2);
        updateMinerBestBitsSafe(bits);

        // Submit only when reversed/display hash <= pool share target.
        if ((bits >= minerShareTargetBits) && hashMeetsShareTarget(shareHash2)) {
          if (ntimeHex.length() != 8 || currentJobId.length() == 0) continue;
          char n_hex[9];
          snprintf(n_hex, sizeof(n_hex), "%08lx", (unsigned long)nonce);
          String submitMsg = String("{\"id\":4,\"method\":\"mining.submit\",\"params\":[\"") +
                             minerUserString() + "\",\"" + currentJobId + "\",\"" +
                             String(en2Hex) + "\",\"" + ntimeHex + "\",\"" + String(n_hex) + "\"]}";
          client.println(submitMsg);
          minerSubmitAttempts++;
          minerLastSubmitMs = millis();
          janusMinerGameReward(2, bits);
          Serial.printf("[MINER] Submit #%lu nonce=%s bits=%u target=%u best=%u H=%lu\n",
                        (unsigned long)minerSubmitAttempts, n_hex, bits, minerShareTargetBits, getMinerBestBitsSafe(), (unsigned long)minerRealHashrate);
        }
      }
    } else {
      minerLocalFallback = true;
      writeLE32(localHeader + 68, 1700000000UL + millis() / 1000UL);
      for (int batch = 0; batch < 180; batch++) {
        writeLE32(localHeader + 76, localNonce++);
        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, localHeader, 80); mbedtls_sha256_finish(&ctx, hash1);
        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, hash1, 32); mbedtls_sha256_finish(&ctx, hash2);
        localHashesThisSecond++;
        minerTotalHashes++;

        hashToShareOrder(hash2, shareHash2);
        uint16_t bits = countLeadingZeroBits(shareHash2);
        updateMinerBestBitsSafe(bits);
        static uint32_t lastLocalOmenMs = 0;
        if (bits >= 22 && millis() - lastLocalOmenMs > 7000UL) {
          lastLocalOmenMs = millis();
          janusMinerGameReward(4, bits);
        }
      }
    }

    uint32_t now = millis();
    if (now - lastHashTick >= 1000) {
      minerRealHashrate = hashesThisSecond;
      minerRemoteHashrate = 0;
      minerLocalHashrate = localHashesThisSecond;
      hashesThisSecond = 0;
      localHashesThisSecond = 0;
      lastHashTick = now;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}


QMP6988 qmp;
M5Canvas canvas(&M5.Display);

M5EchoBase echobase;
static uint8_t echoMicBuf[1024];
bool echoBaseReady = false;
uint32_t echoMicReadOk = 0;
uint32_t echoMicReadFail = 0;
uint32_t lastEchoMicPollMs = 0;
uint32_t lastShockwaveMicMs = 0;
float micNoiseFloor = JANUS_TRON_MIC_FLOOR_BOOT;
float micSignal = 0.0f;
float micRawRms = 0.0f;
float micLastGated = 0.0f;
uint32_t micFloorGuardHits = 0;
uint32_t micFloorLastClampMs = 0;

static inline float janusTronMicMaxF(float a, float b) { return (a > b) ? a : b; }

float janusTronUpdateMicFloor(float rmsRaw, float peakToPeak, uint32_t nowMs) {
#if JANUS_TRON_MIC_FLOOR_GUARD_ENABLE
  float floorNow = micNoiseFloor;
  if (!isfinite(floorNow)) floorNow = JANUS_TRON_MIC_FLOOR_BOOT;
  if (!isfinite(rmsRaw) || rmsRaw < 0.0f) rmsRaw = 0.0f;
  if (!isfinite(peakToPeak) || peakToPeak < 0.0f) peakToPeak = 0.0f;

  bool steadyRoom = (rmsRaw < janusTronMicMaxF(90.0f, floorNow * 1.45f)) &&
                    (peakToPeak < janusTronMicMaxF(900.0f, floorNow * 7.0f));
  bool stuckHighQuiet = (floorNow > JANUS_TRON_MIC_STUCK_FLOOR) &&
                        (rmsRaw < JANUS_TRON_MIC_STUCK_RMS || rmsRaw < floorNow * 0.42f);

  if (stuckHighQuiet) {
    float target = janusClampF(janusTronMicMaxF(JANUS_TRON_MIC_FLOOR_MIN, rmsRaw * 2.0f),
                               JANUS_TRON_MIC_FLOOR_MIN, JANUS_TRON_MIC_FLOOR_SOFT_MAX);
    floorNow = floorNow * (1.0f - JANUS_TRON_MIC_FLOOR_FALL_ALPHA) + target * JANUS_TRON_MIC_FLOOR_FALL_ALPHA;
    micFloorGuardHits++;
    micFloorLastClampMs = nowMs;
  } else if (steadyRoom) {
    float target = janusClampF(rmsRaw, JANUS_TRON_MIC_FLOOR_MIN, JANUS_TRON_MIC_FLOOR_SOFT_MAX);
    float alpha = (target > floorNow) ? JANUS_TRON_MIC_FLOOR_RISE_ALPHA : JANUS_TRON_MIC_FLOOR_FALL_ALPHA;
    floorNow = floorNow * (1.0f - alpha) + target * alpha;
  } else {
    if (floorNow > JANUS_TRON_MIC_FLOOR_SOFT_MAX) {
      floorNow = floorNow * (1.0f - JANUS_TRON_MIC_FLOOR_DECAY_ALPHA) +
                 JANUS_TRON_MIC_FLOOR_SOFT_MAX * JANUS_TRON_MIC_FLOOR_DECAY_ALPHA;
      micFloorGuardHits++;
      micFloorLastClampMs = nowMs;
    }
  }

  if (floorNow > JANUS_TRON_MIC_FLOOR_HARD_MAX) {
    floorNow = JANUS_TRON_MIC_FLOOR_HARD_MAX;
    micFloorGuardHits++;
    micFloorLastClampMs = nowMs;
  }
  floorNow = janusClampF(floorNow, JANUS_TRON_MIC_FLOOR_MIN, JANUS_TRON_MIC_FLOOR_HARD_MAX);
  micNoiseFloor = floorNow;
  return floorNow;
#else
  if (rmsRaw < micNoiseFloor * 1.8f) micNoiseFloor = micNoiseFloor * 0.995f + rmsRaw * 0.005f;
  micNoiseFloor = constrain(micNoiseFloor, 80.0f, 650.0f);
  return micNoiseFloor;
#endif
}

float janusTronComputeGatedMic(float rmsRaw, float peakToPeak, float floorNow) {
  float rmsGate = rmsRaw - floorNow * JANUS_TRON_MIC_GATE_MUL;
  float peakGate = peakToPeak * JANUS_TRON_MIC_PEAK_GATE_MUL - floorNow * 0.82f;
  float gated = (rmsGate > peakGate) ? rmsGate : peakGate;
  if (gated < 0.0f) gated = 0.0f;
  return janusClampF(gated, 0.0f, 12000.0f);
}

#if JANUS_AUDIO_LIVE_TX_ENABLE
static int16_t janusAudioTxPcm16[JANUS_AUDIO_TX_PCM_SAMPLES];
static int16_t janusAudioTxDown8k[JANUS_AUDIO_FRAME_SAMPLES];

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

int16_t janusAudioSoftClip32(int32_t v) {
  if (v > 27000) return 27000;
  if (v < -27000) return -27000;
  return (int16_t)v;
}

uint8_t janusPcm16ToULaw(int16_t sample) {
  const int16_t BIAS = 0x84;
  const int16_t CLIP = 32635;
  uint8_t sign = 0;
  int16_t pcm = sample;
  if (pcm < 0) {
    if (pcm == INT16_MIN) pcm = -32767;
    pcm = -pcm;
    sign = 0x80;
  }
  if (pcm > CLIP) pcm = CLIP;
  pcm += BIAS;

  uint8_t exponent = 7;
  for (int16_t expMask = 0x4000; (pcm & expMask) == 0 && exponent > 0; expMask >>= 1) exponent--;
  uint8_t mantissa = (uint8_t)((pcm >> (exponent + 3)) & 0x0F);
  return (uint8_t)(~(sign | (exponent << 4) | mantissa));
}

uint8_t janusAdpcmEncodeNibble(int16_t sample) {
  int step = janusAdpcmStepTable[janusAudioTxAdpcmIndex];
  int diff = (int)sample - (int)janusAudioTxAdpcmPredictor;
  uint8_t code = 0;
  if (diff < 0) { code = 8; diff = -diff; }

  int delta = step >> 3;
  if (diff >= step) { code |= 4; diff -= step; delta += step; }
  if (diff >= (step >> 1)) { code |= 2; diff -= (step >> 1); delta += (step >> 1); }
  if (diff >= (step >> 2)) { code |= 1; delta += (step >> 2); }

  int pred = janusAudioTxAdpcmPredictor;
  if (code & 8) pred -= delta;
  else pred += delta;
  if (pred > 32767) pred = 32767;
  if (pred < -32768) pred = -32768;
  janusAudioTxAdpcmPredictor = (int16_t)pred;

  int idx = (int)janusAudioTxAdpcmIndex + janusAdpcmIndexTable[code & 0x0F];
  if (idx < 0) idx = 0;
  if (idx > 88) idx = 88;
  janusAudioTxAdpcmIndex = (uint8_t)idx;
  return code & 0x0F;
}


#if JANUS_AUDIO_SNAPSHOT_MODE
void janusAudioTxSnapshotReset() {
  janusAudioSnapCaptureIdx = 0;
  janusAudioSnapSendIdx = 0;
  janusAudioSnapReadyFrames = 0;
  janusAudioSnapSending = false;
  janusAudioSnapLastSendMs = 0;
}

void janusAudioTxSnapshotSendTick(uint32_t now) {
  if (!janusAudioSnapSending) return;
  if (janusAudioSnapSendIdx >= janusAudioSnapReadyFrames) {
    janusAudioTxSnapshotReset();
    return;
  }
  if (now - janusAudioSnapLastSendMs < JANUS_AUDIO_SNAPSHOT_SEND_GAP_MS) return;
  janusAudioSnapLastSendMs = now;

  uint8_t idx = janusAudioSnapSendIdx;
  uint16_t payloadBytes = janusAudioSnapPayload[idx];
  if (payloadBytes == 0 || payloadBytes > JANUS_AUDIO_FRAME_MAX_BYTES) {
    janusAudioSnapSendIdx++;
    return;
  }

  JanusAudioFramePacket af{};
  af.magic[0] = 'A';
  af.magic[1] = 'F';
  af.version = 1;
  af.codec = JANUS_AUDIO_CODEC_ULAW;
  af.seq = ++janusAudioTxSeq;
  af.sampleRate = JANUS_AUDIO_SAMPLE_RATE;
  af.samples = janusAudioSnapSamples[idx] ? janusAudioSnapSamples[idx] : JANUS_AUDIO_FRAME_SAMPLES;
  af.predictor = 0;
  af.stepIndex = 0;
  af.flags = janusAudioSnapFlags[idx];
  memcpy(af.data, janusAudioSnapData[idx], payloadBytes);

  const uint16_t headerLen = sizeof(JanusAudioFramePacket) - JANUS_AUDIO_FRAME_MAX_BYTES;
  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&af, headerLen + payloadBytes);
  if (err == ESP_OK) janusAudioTxFrames++;
  else {
    janusAudioTxSendFail++;
    janusColonyForcePeerRebuild("audio-snapshot-tx-fail");
  }

  janusAudioSnapSendIdx++;
  if (janusAudioSnapSendIdx >= janusAudioSnapReadyFrames) {
    janusAudioSnapClips++;
    janusAudioTxSnapshotReset();
  }
}
#endif

void janusAudioTxTick() {
  uint32_t now = millis();

  if (janusAudioTxEnabled && (now - janusAudioTxLastControlMs > JANUS_AUDIO_CONTROL_TIMEOUT_MS)) {
    janusAudioTxEnabled = false;
    janusAudioTxAdpcmReady = false;
#if JANUS_AUDIO_SNAPSHOT_MODE
    janusAudioTxSnapshotReset();
#endif
    Serial.println("[AUDIO-TX] timeout: Core2 AC pulse lost, stream stopped");
  }

  if (!janusAudioTxEnabled) return;
#if JANUS_AUDIO_SNAPSHOT_MODE
  if (janusAudioSnapSending) {
    janusAudioTxSnapshotSendTick(now);
    return;
  }
#endif
  if (!echoBaseReady) {
    janusAudioTxReadFail++;
    if (now - janusAudioTxLastLogMs > 5000UL) {
      janusAudioTxLastLogMs = now;
      Serial.printf("[AUDIO-TX] EchoBase not ready, ctrl=%lu readFail=%lu\n",
                    (unsigned long)janusAudioTxControlsRx, (unsigned long)janusAudioTxReadFail);
    }
    return;
  }
  if (now - janusAudioTxLastFrameMs < JANUS_AUDIO_FRAME_MS) return;
  janusAudioTxLastFrameMs = now;

  // Exact cadence: 20 ms at 16 kHz = 320 PCM samples = 640 bytes.
  // We downsample to 8 kHz and encode 160 samples into 160 u-law bytes.
  bool ok = echobase.record((uint8_t*)janusAudioTxPcm16, sizeof(janusAudioTxPcm16));
  if (!ok) {
    janusAudioTxReadFail++;
    return;
  }

  // First pass: 16 kHz -> 8 kHz speech decimator + voice-formant cleanup.
  // Keyboard clicks were already clear; speech was "utrobnyy" because low-frequency body
  // dominated the u-law stream. This pass cuts mud below voice range and lifts consonants.
  int64_t sumAbsIn = 0;
  int32_t peakIn = 0;
  for (uint16_t i = 0; i < JANUS_AUDIO_FRAME_SAMPLES; ++i) {
    uint16_t p = i * 2;
    int32_t s0 = janusAudioTxPcm16[p];
    int32_t s1 = janusAudioTxPcm16[p + 1];
    int32_t raw = (s0 + s1) >> 1;

    // Very slow DC removal: only offset, not speech bass.
    janusAudioTxDc = janusAudioTxDc * 0.9990f + (float)raw * 0.0010f;
    float centered = (float)raw - janusAudioTxDc;

    // Speech high-pass around the mud/body area. This is intentionally stronger than
    // the old DC-only filter: it removes the "barrel/uterus" tone but keeps voice rhythm.
    janusAudioTxSpeechLp = janusAudioTxSpeechLp * 0.86f + centered * 0.14f;
    float hp = centered - janusAudioTxSpeechLp;

    // Mild pre-emphasis for consonants. Strong values made clicks great but speech harsh;
    // 0.38 keeps syllables readable on Core2's tiny speaker.
    float shaped = hp + 0.38f * (hp - janusAudioTxPreemphPrev);
    janusAudioTxPreemphPrev = hp;

    // Tiny de-zipper: enough to calm single-sample spikes, not enough to muffle speech.
    shaped = shaped * 0.82f + janusAudioTxLastShaped * 0.18f;
    janusAudioTxLastShaped = shaped;

    int16_t shaped16 = janusAudioSoftClip32((int32_t)shaped);
    janusAudioTxDown8k[i] = shaped16;
    int32_t a = shaped16 < 0 ? -(int32_t)shaped16 : (int32_t)shaped16;
    sumAbsIn += a;
    if (a > peakIn) peakIn = a;
  }

  float avgAbsIn = (float)sumAbsIn / (float)JANUS_AUDIO_FRAME_SAMPLES;
  if (avgAbsIn < janusAudioTxNoiseFloor * 1.65f) {
    janusAudioTxNoiseFloor = janusAudioTxNoiseFloor * 0.990f + avgAbsIn * 0.010f;
  }
  janusAudioTxNoiseFloor = janusClampF(janusAudioTxNoiseFloor, 8.0f, 1200.0f);

  // v8.28 VOICEFORMANT:
  // Keep speech loud enough, but use peak-aware AGC so vowels do not become a monster growl.
  float speechFloor = max(180.0f, janusAudioTxNoiseFloor * 1.45f);
  float effective = max(0.0f, avgAbsIn - speechFloor * 0.25f);
  float desiredGain = JANUS_AUDIO_AGC_TARGET_ABS / (effective + 260.0f);
  if (peakIn > 2200) {
    float peakSafeGain = 23000.0f / ((float)peakIn + 160.0f);
    if (peakSafeGain < desiredGain) desiredGain = peakSafeGain;
  }
  desiredGain = janusClampF(desiredGain, JANUS_AUDIO_AGC_MIN_GAIN, JANUS_AUDIO_AGC_MAX_GAIN);
  float agcAlpha = (desiredGain < janusAudioTxAgcGain) ? 0.26f : 0.055f;
  janusAudioTxAgcGain = janusAudioTxAgcGain * (1.0f - agcAlpha) + desiredGain * agcAlpha;
  janusAudioTxAgcGain = janusClampF(janusAudioTxAgcGain, JANUS_AUDIO_AGC_MIN_GAIN, JANUS_AUDIO_AGC_MAX_GAIN);

  JanusAudioFramePacket af{};
  af.magic[0] = 'A';
  af.magic[1] = 'F';
  af.version = 2;
  af.codec = JANUS_AUDIO_CODEC_ACTIVE;
  af.seq = ++janusAudioTxSeq;
  af.sampleRate = JANUS_AUDIO_SAMPLE_RATE;
  af.samples = JANUS_AUDIO_FRAME_SAMPLES;
  af.flags = 0;

  int64_t sumAbsOut = 0;
  int32_t peakOut = 0;
  uint16_t payloadBytes = 0;

  if (JANUS_AUDIO_CODEC_ACTIVE == JANUS_AUDIO_CODEC_ADPCM4) {
    if (!janusAudioTxAdpcmReady) {
      janusAudioTxAdpcmPredictor = janusAudioTxDown8k[0];
      janusAudioTxAdpcmIndex = 0;
      janusAudioTxAdpcmReady = true;
    }
    af.predictor = janusAudioTxAdpcmPredictor;
    af.stepIndex = janusAudioTxAdpcmIndex;

    uint8_t packed = 0;
    for (uint16_t i = 0; i < JANUS_AUDIO_FRAME_SAMPLES; ++i) {
      int32_t centered = janusAudioTxDown8k[i];
      int32_t a = centered < 0 ? -centered : centered;

      // Very gentle gate: reduce room hiss but do not chop syllables.
      float gate = 1.0f;
      float gateEdge = max(JANUS_AUDIO_NOISE_GATE_ABS, janusAudioTxNoiseFloor * 1.20f);
      if ((float)a < gateEdge) gate = 0.35f + 0.65f * ((float)a / gateEdge);

      int32_t boosted = (int32_t)((float)centered * janusAudioTxAgcGain * gate);
      int16_t out = janusAudioSoftClip32(boosted);
      int32_t oa = out < 0 ? -(int32_t)out : (int32_t)out;
      sumAbsOut += oa;
      if (oa > peakOut) peakOut = oa;
      if (oa > (int32_t)(janusAudioTxNoiseFloor * 3.0f)) af.flags |= 0x01;

      uint8_t n = janusAdpcmEncodeNibble(out);
      if ((i & 1) == 0) packed = n;
      else {
        af.data[payloadBytes++] = (uint8_t)(packed | (n << 4));
        if (payloadBytes >= JANUS_AUDIO_FRAME_MAX_BYTES) break;
      }
    }
  } else {
    af.version = 1;
    af.codec = JANUS_AUDIO_CODEC_ULAW;
    af.samples = min((uint16_t)JANUS_AUDIO_FRAME_SAMPLES, (uint16_t)JANUS_AUDIO_FRAME_MAX_BYTES);
    af.predictor = 0;
    af.stepIndex = 0;
    for (uint16_t i = 0; i < af.samples; ++i) {
      int32_t centered = janusAudioTxDown8k[i];
      int32_t a0 = centered < 0 ? -centered : centered;

      // Do not hard-gate speech. Just attenuate tiny room rumble so endings of words survive.
      float gate = 1.0f;
      float gateEdge = max(JANUS_AUDIO_NOISE_GATE_ABS, janusAudioTxNoiseFloor * 0.85f);
      if ((float)a0 < gateEdge) gate = 0.62f + 0.38f * ((float)a0 / gateEdge);

      int32_t boosted = (int32_t)((float)centered * janusAudioTxAgcGain * gate);
      int32_t ab = boosted < 0 ? -boosted : boosted;
      if (ab > 21500) {
        int32_t sign = boosted < 0 ? -1 : 1;
        boosted = sign * (21500 + ((ab - 21500) >> 2));
      }
      int16_t out = janusAudioSoftClip32(boosted);
      int32_t oa = out < 0 ? -(int32_t)out : (int32_t)out;
      sumAbsOut += oa;
      if (oa > peakOut) peakOut = oa;
      if (oa > (int32_t)(janusAudioTxNoiseFloor * 1.50f)) af.flags |= 0x01;
      af.data[payloadBytes++] = janusPcm16ToULaw(out);
    }
  }

  float avgOut = (float)sumAbsOut / (float)max(1U, (unsigned)af.samples);
  swarmMicFrames++;
  echoMicReadOk++;
  swarmMicRms = swarmMicRms * 0.80f + avgOut * 0.20f;
  if ((float)peakOut > swarmMicPeak) swarmMicPeak = (float)peakOut;
  else swarmMicPeak *= 0.975f;
  micSignal = micSignal * 0.82f + avgOut * 0.18f;

#if JANUS_AUDIO_SNAPSHOT_MODE
  // v8.29: do not send every frame live. Store a clean speech clip in RAM,
  // then burst it to Core2 so Core2 can buffer before playback.
  if (payloadBytes > 0 && payloadBytes <= JANUS_AUDIO_FRAME_MAX_BYTES && janusAudioSnapCaptureIdx < JANUS_AUDIO_SNAPSHOT_FRAMES) {
    uint8_t si = janusAudioSnapCaptureIdx;
    memcpy(janusAudioSnapData[si], af.data, payloadBytes);
    janusAudioSnapPayload[si] = payloadBytes;
    janusAudioSnapSamples[si] = af.samples;
    janusAudioSnapFlags[si] = af.flags;
    janusAudioSnapCaptureIdx++;
    if (janusAudioSnapCaptureIdx >= JANUS_AUDIO_SNAPSHOT_FRAMES) {
      janusAudioSnapReadyFrames = janusAudioSnapCaptureIdx;
      janusAudioSnapSendIdx = 0;
      janusAudioSnapSending = true;
      janusAudioSnapLastSendMs = 0;
    }
  }
#else
  const uint16_t headerLen = sizeof(JanusAudioFramePacket) - JANUS_AUDIO_FRAME_MAX_BYTES;
  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&af, headerLen + payloadBytes);
  if (err == ESP_OK) janusAudioTxFrames++;
  else {
    janusAudioTxSendFail++;
    janusColonyForcePeerRebuild("audio-live-tx-fail");
  }
#endif

  if (now - janusAudioTxLastLogMs > 5000UL) {
    janusAudioTxLastLogMs = now;
    Serial.printf("[AUDIO-TX] radio=%d codec=%u tx=%lu fail=%lu readFail=%lu seq=%u samples=%u bytes=%u in=%.0f out=%.0f pin=%ld pout=%ld gain=%.2f nf=%.0f snap=%u/%u send=%u clips=%lu\n",
                  janusAudioTxEnabled ? 1 : 0, (unsigned)af.codec,
                  (unsigned long)janusAudioTxFrames,
                  (unsigned long)janusAudioTxSendFail,
                  (unsigned long)janusAudioTxReadFail,
                  (unsigned)janusAudioTxSeq,
                  (unsigned)af.samples, (unsigned)payloadBytes,
                  avgAbsIn, avgOut, (long)peakIn, (long)peakOut, janusAudioTxAgcGain, janusAudioTxNoiseFloor,
                  (unsigned)janusAudioSnapCaptureIdx, (unsigned)JANUS_AUDIO_SNAPSHOT_FRAMES,
                  janusAudioSnapSending ? 1 : 0, (unsigned long)janusAudioSnapClips);
  }
}
#else
void janusAudioTxTick() {}
#endif

class NeuralSwarm {
private:
  Base bases[2];
  Unit units[Config::MAX_UNITS];
  Resource resources[Config::MAX_RESOURCES];
  Particle particles[Config::MAX_PARTICLES];
  SpitFX spits[Config::MAX_SPITS];
  BgDrop bgDrops[15];
  SwarmMicroAI ai;

  GameState state;
  float gameOverTimer;
  uint32_t lastTick;
  float fixedAccumulator = 0.0f;
  uint32_t nextSoundTime;
  uint32_t runStartMs;
  uint32_t survivalMs;
  uint32_t lastSaveMs;
  uint32_t lastAutoBuyMs;

  uint8_t brightLevels[5] = {255, 150, 70, 20, 0};
  int brightIdx = 0;
  bool soundEnabled = true;

  void loadUiState() {
    Preferences p;
    if (!p.begin("tron_ui", false)) return;  // E4S2: create namespace on first boot, avoids NVS NOT_FOUND
    brightIdx = constrain((int)p.getUChar("bright", 0), 0, 4);
    soundEnabled = p.getBool("sound", true);
    p.end();
  }

  void saveUiState() {
    Preferences p;
    if (!p.begin("tron_ui", false)) return;
    p.putUChar("bright", (uint8_t)constrain(brightIdx, 0, 4));
    p.putBool("sound", soundEnabled);
    p.end();
  }
  float avgFps = 60.0f;
  float shockwaveRadius = 0.0f;
  int gridHead[64];
  int gridNext[Config::MAX_UNITS];
  bool spatialReady = false;

  uint16_t playerColor = 0;
  uint16_t enemyColor = 0;
  uint32_t gold = 0;
  uint32_t xp = 0;
  uint16_t level = 1;
  uint16_t wave = 0;
  uint16_t enemiesLeftToSpawn = 0;
  uint16_t enemiesKilledThisWave = 0;
  float waveSpawnTimer = 0.0f;
  float betweenWaveTimer = 4.0f;
  float throneCooldown = 0.0f;

  // Visual survival layer: fixed day/night cycle + lightweight procedural weather.
  float dayPhase = 0.18f;
  uint8_t weatherMode = 0;      // 0 clear, 1 rain, 2 storm, 3 fog, 4 heat haze
  float weatherStrength = 0.0f;
  uint32_t lastWeatherShiftMs = 0;

  // Per-wave random sensor boons. Reset at every wave start; not saved.
  float waveDamageMul = 1.0f;
  float waveFireMul = 1.0f;
  float waveLootMul = 1.0f;
  float waveShieldMul = 1.0f;
  char waveBoostLine[28] = "B:NONE";

  uint8_t dmgLv = 0;
  uint8_t fireLv = 0;
  uint8_t throneLv = 0;
  uint8_t unitLv = 0;
  uint8_t towerLv = 0;
  uint8_t repairLv = 0;
  uint8_t towerCount = 0;
  uint8_t heroLimit = 1;
  char heroLine[32] = "MECH L1";
  int cachedHeroIdx = -1;
  uint32_t cachedHeroUntilMs = 0;
  uint16_t modelId = 1;

  // v8.16 GroundOps: Core2 commands planetary sectors, Stick delivers/pilots mechs,
  // this ATOM-SWARM-TRON executes the ground layer.
  // 0 = base/throne defense; 1 = hero/mecha raid / sabotage operation.
  uint8_t groundMode = 0;
  uint8_t groundStyle = 0;      // 0=RTS base hold, 1=hero sabotage raid, 2=throne siege
  uint8_t groundSector = 0;
  uint8_t groundPriority = 0;
  uint8_t groundBiome = 0;      // procedural planet skin
  uint8_t groundFaction = 0;
  uint16_t groundFlags = 0;
  uint32_t groundSeed = 0x1138A11FUL;
  uint32_t groundMissionId = 1;
  uint32_t groundBannerUntilMs = 0;
  float raidFocusX = 98.0f;
  float raidFocusY = 30.0f;
  char groundMissionLine[32] = "BASE HOLD";
  char groundPlanetLine[32] = "JANUS ARK";
  char groundObjectiveLine[32] = "DERZHAT TRON";
  uint32_t lastGroundOrderSeq = 0;

  // v8.18: Core2 orders are mission contracts, not instant background swaps.
  // Latest Core2 G/O packet is queued and committed only on a clean wave boundary.
  bool pendingGroundOrder = false;
  uint32_t pendingGroundOrderSeq = 0;
  uint8_t pendingGroundMode = 0;
  uint8_t pendingGroundSector = 0;
  uint8_t pendingGroundPriority = 0;
  uint16_t pendingGroundFlags = 0;
  uint32_t pendingGroundMission = 0;
  char pendingGroundTarget[16] = "BASE_HOLD";
  uint32_t groundToastUntilMs = 0;
  uint32_t groundMissionStartMs = 0;
  uint8_t activeMissionWave = 0;
  char groundToastLine[32] = "CORE2 ORDER READY";

  // v8.22: mining luck and personal agent state form a positive gameplay loop. Shares/accepts/Buzz Agent rewards become
  // station morale, wave boons, particles and small resource rewards.
  uint8_t miningLuckCharges = 0;
  float miningLuckPower = 0.0f;
  uint32_t miningToastUntilMs = 0;
  char miningToastLine[32] = "HASH LUCK";

  uint32_t bestMs[4] = {0,0,0,0};
  uint16_t bestWave[4] = {0,0,0,0};
  uint16_t bestColor[4] = {0,0,0,0};
  uint16_t bestModel[4] = {0,0,0,0};
  bool recordsLoaded = false;
  uint32_t recordsToastUntilMs = 0;

  struct SaveRec {
    uint32_t magic;
    uint16_t version;
    uint16_t crc;
    uint32_t bestMs[4];
    uint16_t bestWave[4];
    uint16_t bestColor[4];
    uint16_t bestModel[4];
    uint16_t nextModel;
  };

  float distSq(float x1, float y1, float x2, float y2) const {
    return (x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1);
  }

  uint16_t crc16(const uint8_t* data, size_t n) {
    uint16_t c = 0xA55A;
    for (size_t i = 0; i < n; i++) c = (uint16_t)((c << 5) ^ (c >> 11) ^ data[i]);
    return c;
  }

  uint16_t dimColor(uint16_t c, float k) {
    k = constrain(k, 0.0f, 1.0f);
    uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((c >> 5) & 0x3F) * 255 / 63;
    uint8_t b = (c & 0x1F) * 255 / 31;
    return canvas.color565((uint8_t)(r*k), (uint8_t)(g*k), (uint8_t)(b*k));
  }

  uint16_t brightenColor(uint16_t c, float add) {
    add = constrain(add, 0.0f, 1.0f);
    int r = ((c >> 11) & 0x1F) * 255 / 31;
    int g = ((c >> 5) & 0x3F) * 255 / 63;
    int b = (c & 0x1F) * 255 / 31;
    r = r + (int)((255 - r) * add);
    g = g + (int)((255 - g) * add);
    b = b + (int)((255 - b) * add);
    return canvas.color565(r, g, b);
  }

  uint16_t getNeonColor() {
    uint8_t r = (esp_random() % 156) + 100;
    uint8_t g = (esp_random() % 156) + 100;
    uint8_t b = (esp_random() % 156) + 100;
    if (r <= g && r <= b) r /= 3;
    else if (g <= r && g <= b) g /= 3;
    else b /= 3;
    return canvas.color565(r, g, b);
  }

  uint16_t tournamentColor(uint16_t id) {
    // Larger deterministic color ladder for the endless survival tournament.
    static const uint8_t pal[][3] = {
      { 80,255,150},{255, 72,120},{ 86,190,255},{255,190, 72},
      {205, 96,255},{ 92,255,230},{255,120, 70},{170,255, 90},
      {255,105,210},{120,145,255},{255,230, 90},{ 70,230,120},
      {240,130,255},{ 95,220,255},{255,150,120},{185,255,180},
      {255, 90, 90},{120,255,190},{180,120,255},{255,210,140},
      {110,210,120},{140,235,255},{255,135,185},{210,235, 95},
      {105,170,255},{255,170, 95},{180,255,235},{230,110,170},
      {150,255,105},{115,255,255},{255,245,130},{200,160,255}
    };
    const uint8_t n = sizeof(pal) / sizeof(pal[0]);
    uint8_t idx = (uint8_t)((id - 1) % n);
    uint8_t cycle = (uint8_t)(((id - 1) / n) % 4);
    int r = pal[idx][0], g = pal[idx][1], b = pal[idx][2];
    if (cycle == 1) { r = (r * 4) / 5; g = min(255, (g * 11) / 10); b = min(255, (b * 11) / 10); }
    else if (cycle == 2) { r = min(255, (r * 11) / 10); g = (g * 4) / 5; b = min(255, (b * 11) / 10); }
    else if (cycle == 3) { r = min(255, (r * 11) / 10); g = min(255, (g * 11) / 10); b = (b * 4) / 5; }
    return canvas.color565((uint8_t)r, (uint8_t)g, (uint8_t)b);
  }

  const char* compactK(uint32_t v) {
    static char buf[4][12];
    static uint8_t idx = 0;
    char* b = buf[idx++ & 3];
    if (v < 1000UL) snprintf(b, 12, "%lu", (unsigned long)v);
    else if (v < 1000000UL) snprintf(b, 12, "%luk", (unsigned long)(v / 1000UL));
    else if (v < 1000000000UL) snprintf(b, 12, "%lukk", (unsigned long)(v / 1000000UL));
    else snprintf(b, 12, "%lukkk", (unsigned long)(v / 1000000000UL));
    return b;
  }

  const char* fmtTime(uint32_t ms) {
    static char b[12];
    uint32_t s = ms / 1000UL;
    uint32_t m = s / 60UL;
    s %= 60UL;
    snprintf(b, sizeof(b), "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
    return b;
  }

  const char* weatherName() const {
    switch (weatherMode) {
      case 1: return "RAIN";
      case 2: return "STORM";
      case 3: return "FOG";
      case 4: return "HEAT";
      default: return "CLEAR";
    }
  }

  const char* dayName() const {
    // Day phase 0.00..0.49 = day, 0.50..0.99 = night.
    return (dayPhase < 0.50f) ? "DAY" : "NIGHT";
  }

  uint32_t groundMix32(uint32_t x) const {
    x ^= x >> 16; x *= 0x7feb352dUL;
    x ^= x >> 15; x *= 0x846ca68bUL;
    x ^= x >> 16;
    return x;
  }

  const char* groundBiomeName() const {
    switch (groundBiome % 8) {
      case 0: return "ARK MEADOW";
      case 1: return "DUNE RUINS";
      case 2: return "TOXIC SWAMP";
      case 3: return "ICE VAULT";
      case 4: return "EMBER HIVE";
      case 5: return "OCEAN MOON";
      case 6: return "CRYSTAL FIELD";
      default: return "VOID DUST";
    }
  }

  const char* groundFactionName() const {
    switch (groundFaction % 6) {
      case 0: return "THG";
      case 1: return "PIR";
      case 2: return "CORP";
      case 3: return "MERC";
      case 4: return "WILD";
      default: return "XENO";
    }
  }

  const char* groundStyleName() const {
    if (groundStyle == 1) return "HERO RAID";
    if (groundStyle == 2) return "TRON DEF";
    return "RTS HOLD";
  }

  const char* groundModeName() const {
    return groundStyleName();
  }

  uint16_t groundBiomeColor(float dl) const {
    uint8_t light = (uint8_t)constrain((int)(dl * 90.0f), 20, 95);
    switch (groundBiome % 8) {
      case 0: return canvas.color565(5 + light/5, 20 + light/3, 10 + light/6);
      case 1: return canvas.color565(32 + light/2, 22 + light/3, 8 + light/8);
      case 2: return canvas.color565(8 + light/8, 24 + light/3, 18 + light/5);
      case 3: return canvas.color565(10 + light/6, 18 + light/3, 36 + light/2);
      case 4: return canvas.color565(42 + light/3, 9 + light/8, 4 + light/12);
      case 5: return canvas.color565(4 + light/10, 12 + light/4, 34 + light/2);
      case 6: return canvas.color565(20 + light/4, 10 + light/5, 32 + light/2);
      default: return canvas.color565(5 + light/12, 5 + light/12, 12 + light/4);
    }
  }

  uint8_t inferGroundStyle(uint8_t rawMode, const char* target, uint32_t mission, uint8_t prio, uint16_t flags) const {
    if (rawMode >= 2) return rawMode % 3;
    if (target) {
      if (strstr(target, "RAID") || strstr(target, "SAB") || strstr(target, "HERO")) return 1;
      if (strstr(target, "TRON") || strstr(target, "THRONE") || strstr(target, "SIEGE")) return 2;
    }
    if (rawMode == 1) return 1;
    if (flags & 0x0002) return 2;
    if (prio >= 230) return ((mission ^ groundSector) & 1) ? 2 : 1;
    if ((mission % 11UL) == 3UL) return 1;
    if ((mission % 7UL) == 2UL) return 2;
    return 0;
  }

  void configureGroundWorld(uint8_t sector, uint8_t prio, uint32_t mission, uint16_t flags, const char* target) {
    groundSector = sector & 31;
    groundPriority = prio;
    groundFlags = flags;
    if (mission) groundMissionId = mission;
    groundSeed = groundMix32(0x4A414E55UL ^ ((uint32_t)groundSector << 24) ^ groundMissionId ^ ((uint32_t)prio << 9) ^ flags);
    groundBiome = (uint8_t)(groundMix32(groundSeed ^ 0xB10B10UL) % 8);
    groundFaction = (uint8_t)(groundMix32(groundSeed ^ 0xFA0700UL) % 6);
    groundStyle = inferGroundStyle(janusGroundOrderMode, target, groundMissionId, prio, flags);
    groundMode = (groundStyle == 1) ? 1 : 0;

    const char* st = groundStyleName();
    snprintf(groundMissionLine, sizeof(groundMissionLine), "%s S%02u", st, (unsigned)groundSector);
    snprintf(groundPlanetLine, sizeof(groundPlanetLine), "%s/%s", groundBiomeName(), groundFactionName());
    if (groundStyle == 1) snprintf(groundObjectiveLine, sizeof(groundObjectiveLine), "DIVERSIYA BAZY");
    else if (groundStyle == 2) snprintf(groundObjectiveLine, sizeof(groundObjectiveLine), "ZASHITA TRONA");
    else snprintf(groundObjectiveLine, sizeof(groundObjectiveLine), "BAZA I RESURS");

    uint32_t r = groundMix32(groundSeed ^ 0xC0FFEEUL);
    raidFocusX = 16.0f + (float)(r % 96);
    raidFocusY = 16.0f + (float)((r >> 8) % 96);
    groundBannerUntilMs = millis() + 8000UL;

    // Planet seed influences the living world immediately.
    weatherMode = (groundBiome == 3) ? 3 : (groundBiome == 4 ? 4 : (groundBiome == 2 ? 1 : (uint8_t)(r % 5)));
    weatherStrength = 0.25f + (float)((r >> 16) & 0x3F) / 90.0f;
    enemyColor = (groundFaction == 0) ? canvas.color565(210,40,220) :
                 (groundFaction == 1) ? canvas.color565(230,70,70) :
                 (groundFaction == 2) ? canvas.color565(220,160,60) :
                 (groundFaction == 3) ? canvas.color565(120,180,240) :
                 (groundFaction == 4) ? canvas.color565(80,220,110) : canvas.color565(210,230,80);
  }

  void updateRaidFocus() {
    if (groundStyle != 1) return;
    uint32_t now = millis();
    float t = (now * 0.00045f) + (float)(groundSeed & 1023) * 0.01f;
    raidFocusX = 64.0f + cosf(t * 1.7f) * 42.0f + sinf(t * 0.31f) * 10.0f;
    raidFocusY = 64.0f + sinf(t * 1.3f) * 42.0f + cosf(t * 0.27f) * 10.0f;
    raidFocusX = constrain(raidFocusX, 10.0f, 118.0f);
    raidFocusY = constrain(raidFocusY, 10.0f, 118.0f);
  }

  void drawGroundTiles(uint32_t now, float dl) {
    uint16_t base = groundBiomeColor(dl);
    uint16_t line = dimColor(base, 0.68f);
    uint8_t step = 16;
    int scrollX = (groundStyle == 1) ? (int)((now / 80 + (groundSeed & 31)) % step) : (int)((groundSeed >> 3) & 15);
    int scrollY = (groundStyle == 1) ? (int)((now / 110 + ((groundSeed >> 8) & 31)) % step) : (int)((groundSeed >> 12) & 15);
    for (int y = -scrollY; y < Config::SCREEN_H; y += step) {
      for (int x = -scrollX; x < Config::SCREEN_W; x += step) {
        uint32_t n = groundMix32(groundSeed ^ (uint32_t)(x + 300) * 73UL ^ (uint32_t)(y + 500) * 197UL);
        uint16_t c = (n & 1) ? dimColor(base, 0.72f) : dimColor(base, 0.58f);
        canvas.fillRect(x, y, step, step, c);
        if ((n & 7) == 0) canvas.drawPixel(x + (n & 15), y + ((n >> 4) & 15), brightenColor(c, 0.40f));
      }
    }
    for (int i = 0; i < 6; ++i) {
      int x = (int)((groundMix32(groundSeed ^ (uint32_t)i * 99UL) + now / (groundStyle == 1 ? 18 : 90)) & 127);
      int y = (int)((groundMix32(groundSeed ^ (uint32_t)i * 177UL) + now / (groundStyle == 1 ? 23 : 130)) & 127);
      canvas.drawLine(x - 3, y, x + 3, y, line);
      canvas.drawLine(x, y - 3, x, y + 3, line);
    }
    if (groundStyle == 1) {
      canvas.drawCircle((int)raidFocusX, (int)raidFocusY, 5, TFT_RED);
      canvas.drawLine((int)raidFocusX - 6, (int)raidFocusY, (int)raidFocusX + 6, (int)raidFocusY, TFT_RED);
      canvas.drawLine((int)raidFocusX, (int)raidFocusY - 6, (int)raidFocusX, (int)raidFocusY + 6, TFT_RED);
    }
  }

  void drawGroundMissionOverlay(uint32_t now) {
    // Temporary compact toast only. Persistent objective text stays in the tiny HUD line,
    // so the fight view remains clean.
    if (!groundBannerUntilMs || now > groundBannerUntilMs) return;
    uint16_t frame = (groundStyle == 1) ? canvas.color565(255, 140, 40) : (groundStyle == 2 ? TFT_RED : playerColor);
    canvas.drawRoundRect(3, 23, 122, 15, 3, dimColor(frame, 0.78f));
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(brightenColor(frame, 0.18f), TFT_TRANSPARENT);
    canvas.drawString(groundMissionLine, 6, 25);
    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(TFT_SILVER, TFT_TRANSPARENT);
    canvas.drawString(groundStyle == 1 ? "SAB" : (groundStyle == 2 ? "DEF" : "HOLD"), 121, 25);
    canvas.setTextDatum(TL_DATUM);
  }

  void drawGroundToast(uint32_t now) {
    if (!groundToastUntilMs || now > groundToastUntilMs) return;
    uint16_t c = pendingGroundOrder ? canvas.color565(255, 190, 80) : dimColor(playerColor, 0.90f);
    canvas.drawRoundRect(5, 39, 118, 12, 3, dimColor(c, 0.65f));
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(c, TFT_TRANSPARENT);
    canvas.drawString(groundToastLine, 8, 41);
  }

  bool canCommitGroundOrder() const {
    return state == GameState::BETWEEN_WAVES && enemiesLeftToSpawn == 0 && aliveEnemies() == 0;
  }

  void commitPendingGroundOrder() {
    if (!pendingGroundOrder) return;
    uint8_t oldMode = janusGroundOrderMode;
    janusGroundOrderMode = pendingGroundMode;
    configureGroundWorld(pendingGroundSector, pendingGroundPriority,
                         pendingGroundMission ? pendingGroundMission : (groundMissionId + 1),
                         pendingGroundFlags, pendingGroundTarget);
    janusGroundOrderMode = oldMode;
    pendingGroundOrder = false;
    groundBannerUntilMs = millis() + 4200UL;
    groundToastUntilMs = millis() + 2200UL;
    snprintf(groundToastLine, sizeof(groundToastLine), "CORE2 START S%02u", (unsigned)groundSector);

    if (groundStyle == 1 && aliveHeroes() < 1) {
      if (gold < 30) gold += 30;
      gold = (gold > 30) ? (gold - 30) : 0;
      spawnUnit(0, Role::HERO, bases[0].x, bases[0].y - 12);
      playBeep(1450, 70);
    } else if (groundStyle == 2) {
      bases[0].hp = min(bases[0].maxHp, bases[0].hp + 30.0f);
      shockwaveRadius = max(shockwaveRadius, 12.0f);
      spawnParticle(bases[0].x, bases[0].y, TFT_RED, 14);
      playBeep(360, 70);
    } else {
      spawnParticle(bases[0].x, bases[0].y, playerColor, 9);
      playBeep(650, 35);
    }
  }

  void applyGroundOrders() {
    uint32_t seq = janusGroundOrderSeq;
    if (seq != 0 && seq != lastGroundOrderSeq) {
      lastGroundOrderSeq = seq;
      pendingGroundOrder = true;
      pendingGroundOrderSeq = seq;
      pendingGroundMode = janusGroundOrderMode ? 1 : 0;
      pendingGroundSector = janusGroundOrderSector;
      pendingGroundPriority = janusGroundOrderPriority;
      pendingGroundMission = janusGroundOrderMission ? janusGroundOrderMission : (groundMissionId + 1);
      pendingGroundFlags = janusGroundOrderFlags;
      strlcpy(pendingGroundTarget, janusGroundOrderTarget[0] ? janusGroundOrderTarget : "BASE_HOLD", sizeof(pendingGroundTarget));
      groundToastUntilMs = millis() + 5200UL;
      snprintf(groundToastLine, sizeof(groundToastLine), "CORE2 QUEUE S%02u", (unsigned)(pendingGroundSector & 31));
      if (state == GameState::BETWEEN_WAVES) betweenWaveTimer = max(betweenWaveTimer, 1.2f);
      playBeep(880, 24);
    }

    if (pendingGroundOrder && canCommitGroundOrder()) commitPendingGroundOrder();
  }

  float dayLight() const {
    // 0.35 at night, 1.0 at noon; one sin call per frame, not per-pixel.
    float v = 0.5f + 0.5f * sinf(dayPhase * 6.2831853f);
    return 0.35f + v * 0.65f;
  }

  float enemyWeatherSpeedMul() const {
    float m = 1.0f;
    if (weatherMode == 1) m -= 0.05f + weatherStrength * 0.05f;      // rain mud
    else if (weatherMode == 2) m -= 0.08f + weatherStrength * 0.07f; // storm pressure
    else if (weatherMode == 3) m -= 0.03f;                           // fog hesitation
    else if (weatherMode == 4) m += 0.02f + weatherStrength * 0.03f; // heat frenzy
    if (dayPhase >= 0.50f) m += 0.035f;                              // night mobs push harder
    return constrain(m, 0.78f, 1.12f);
  }

  void resetWaveBoosts() {
    waveDamageMul = 1.0f;
    waveFireMul = 1.0f;
    waveLootMul = 1.0f;
    waveShieldMul = 1.0f;
    strlcpy(waveBoostLine, "B:NONE", sizeof(waveBoostLine));
  }

  void applySensorWaveBoost() {
    resetWaveBoosts();
    uint32_t r = esp_random() % 100;

    // Real sensor bias comes first; random fallback keeps every wave slightly different.
    if (swarmPressureHpa > 0.1f && swarmPressureHpa < 1007.0f) {
      waveShieldMul = 0.88f;
      waveLootMul = 1.08f;
      bases[0].hp = min(bases[0].maxHp, bases[0].hp + 18.0f + wave * 1.5f);
      snprintf(waveBoostLine, sizeof(waveBoostLine), "B:LOWP");
    } else if (swarmPressureHpa > 1022.0f) {
      gold += 18 + wave * 2;
      waveLootMul = 1.10f;
      snprintf(waveBoostLine, sizeof(waveBoostLine), "B:HIGHP");
    } else if (swarmTempC > 29.5f) {
      waveDamageMul = 1.16f;
      waveFireMul = 1.10f;
      snprintf(waveBoostLine, sizeof(waveBoostLine), "B:HEAT");
    } else if (swarmTempC > 0.1f && swarmTempC < 18.0f) {
      waveFireMul = 1.14f;
      waveLootMul = 1.12f;
      snprintf(waveBoostLine, sizeof(waveBoostLine), "B:COLD");
    } else if (weatherMode == 2) {
      waveDamageMul = 1.08f;
      waveShieldMul = 0.94f;
      snprintf(waveBoostLine, sizeof(waveBoostLine), "B:STORM");
    } else if (weatherMode == 1 || weatherMode == 3) {
      waveLootMul = 1.14f;
      snprintf(waveBoostLine, sizeof(waveBoostLine), weatherMode == 1 ? "B:RAIN" : "B:FOG");
    } else if (r < 22) {
      gold += 12 + wave;
      snprintf(waveBoostLine, sizeof(waveBoostLine), "B:GOLD");
    } else if (r < 44) {
      waveDamageMul = 1.10f;
      snprintf(waveBoostLine, sizeof(waveBoostLine), "B:DMG");
    } else if (r < 66) {
      waveFireMul = 1.10f;
      snprintf(waveBoostLine, sizeof(waveBoostLine), "B:FIRE");
    } else if (r < 84) {
      waveLootMul = 1.12f;
      snprintf(waveBoostLine, sizeof(waveBoostLine), "B:LOOT");
    } else {
      waveShieldMul = 0.92f;
      snprintf(waveBoostLine, sizeof(waveBoostLine), "B:WARD");
    }

    // Personal agent state is not a fake share. It only shapes how well the same wave boon is used.
    float pEntropy, pMood, pFatigue, pVigor;
    uint8_t pMoodCode;
    janusPersonalSnapshot(pEntropy, pMood, pFatigue, pVigor, pMoodCode);
    float personalUse = constrain(0.84f + pMood * 0.18f + pVigor * 0.16f - pFatigue * 0.18f - pEntropy * 0.05f, 0.70f, 1.18f);
    waveDamageMul = constrain(waveDamageMul * personalUse, 0.72f, 1.65f);
    waveFireMul = constrain(waveFireMul * (0.90f + pVigor * 0.16f - pFatigue * 0.06f), 0.75f, 1.55f);
    waveLootMul = constrain(waveLootMul * (0.92f + pMood * 0.18f), 0.80f, 1.90f);
    if (pFatigue > 0.74f) waveShieldMul = min(1.08f, waveShieldMul + 0.04f);

    // Tiny visual/audio confirmation, cheap and readable.
    spawnParticle(bases[0].x, bases[0].y, playerColor, 10);
    playBeep(720, 28);
  }

  void playBeep(int freq, int duration) {
    if (!soundEnabled) return;
    uint32_t now = millis();
    if (now >= nextSoundTime) {
      M5.Speaker.tone(freq, duration);
      nextSoundTime = now + duration;
    }
  }

  void drawSpark(int x, int y, uint16_t col) {
    canvas.drawPixel(x, y, col);
    canvas.drawPixel(x - 1, y, dimColor(col, 0.65f));
    canvas.drawPixel(x + 1, y, dimColor(col, 0.65f));
    canvas.drawPixel(x, y - 1, dimColor(col, 0.65f));
    canvas.drawPixel(x, y + 1, dimColor(col, 0.65f));
  }

  void addBeam(float x1, float y1, float x2, float y2, uint16_t color, float life = 0.18f) {
    for (auto& s : spits) {
      if (!s.active) {
        s.active = true;
        s.startX = x1; s.startY = y1; s.endX = x2; s.endY = y2;
        s.color = color; s.life = life; s.maxLife = life;
        return;
      }
    }
  }

  void spawnParticle(float x, float y, uint16_t color, int count) {
    for (int i = 0; i < count; i++) {
      for (auto& p : particles) {
        if (!p.active) {
          p.active = true;
          p.x = x; p.y = y;
          float angle = (float)(esp_random() % 628) * 0.01f;
          float speed = (float)((esp_random() % 45) + 10);
          p.vx = cosf(angle) * speed;
          p.vy = sinf(angle) * speed;
          p.life = ((esp_random() % 24) + 10) * 0.012f;
          p.color = color;
          break;
        }
      }
    }
  }

  void spawnLoot(float x, float y, uint16_t amount) {
    for (auto& r : resources) {
      if (!r.active) {
        r.active = true;
        r.x = constrain(x + (int)(esp_random() % 13) - 6, 3.0f, (float)Config::SCREEN_W - 3.0f);
        r.y = constrain(y + (int)(esp_random() % 13) - 6, 3.0f, (float)Config::SCREEN_H - 3.0f);
        r.amount = amount;
        return;
      }
    }
  }

  void absorbLootToThrone(float x, float y, uint16_t amount) {
    // v8.15B: mob drops no longer clutter the map.
    // The throne absorbs loot immediately as gold/xp/resources.
    if (amount == 0) amount = 1;
    gold += amount;
    xp += max(1, (int)amount / 2);
    bases[0].resources += amount;

    addBeam(x, y, bases[0].x, bases[0].y, TFT_YELLOW, 0.18f);
    spawnParticle(x, y, TFT_YELLOW, 4);
    spawnParticle(bases[0].x, bases[0].y, TFT_YELLOW, 9);

    if (xp >= level * 70UL) {
      xp -= level * 70UL;
      level++;
      gold += 18 + level * 4;
      spawnParticle(bases[0].x, bases[0].y, playerColor, 12);
      playBeep(1450, 35);
    }
  }

  int aliveEnemies() const {
    int n = 0;
    for (int i = 0; i < Config::MAX_UNITS; i++) if (units[i].active && units[i].team == 1) n++;
    return n;
  }

  int aliveFriendlies() const {
    int n = 0;
    for (int i = 0; i < Config::MAX_UNITS; i++) if (units[i].active && units[i].team == 0 && units[i].role != Role::TOWER) n++;
    return n;
  }

  int aliveTowers() const {
    int n = 0;
    for (int i = 0; i < Config::MAX_UNITS; i++) if (units[i].active && units[i].team == 0 && units[i].role == Role::TOWER) n++;
    return n;
  }

  int aliveHeroes() const {
    int n = 0;
    for (int i = 0; i < Config::MAX_UNITS; i++) if (units[i].active && units[i].team == 0 && units[i].role == Role::HERO) n++;
    return n;
  }

  enum HeroSkillBits : uint16_t {
    SK_HOOK   = 1 << 0,
    SK_FIRE   = 1 << 1,
    SK_SHIELD = 1 << 2,
    SK_LASER  = 1 << 3,
    SK_NOVA   = 1 << 4,
    SK_CHAIN  = 1 << 5,
    SK_BLINK  = 1 << 6,
    SK_FROST  = 1 << 7,
    SK_POISON = 1 << 8,
    SK_TOTEM  = 1 << 9,
    SK_METEOR = 1 << 10
  };

  const char* heroClassName(uint8_t c) const {
    static const char* names[] = {"VANGUARD", "PYRO-MECH", "PSION", "PAL-MECH", "SABOTEUR", "ENGINEER"};
    return names[c % 6];
  }

  const char* heroClassShort(uint8_t c) const {
    static const char* names[] = {"VNG", "PYM", "PSI", "PLM", "SAB", "ENG"};
    return names[c % 6];
  }

  uint8_t heroSkillCount(uint16_t m) const {
    uint8_t n = 0;
    while (m) { n += (m & 1); m >>= 1; }
    return n;
  }

  uint16_t randomSkillBit() const {
    return (uint16_t)(1u << (esp_random() % 11));
  }

  uint16_t pickRandomOwnedSkill(uint16_t mask) const {
    uint16_t owned[12];
    uint8_t n = 0;
    for (uint8_t i = 0; i < 11; ++i) {
      uint16_t b = (uint16_t)(1u << i);
      if (mask & b) owned[n++] = b;
    }
    if (!n) return 0;
    return owned[esp_random() % n];
  }

  uint16_t nextSkillBit(uint16_t cur) const {
    for (uint8_t i = 0; i < 11; ++i) {
      uint16_t b = (uint16_t)(1u << i);
      if (cur == b) return (i == 10) ? 0 : (uint16_t)(1u << (i + 1));
    }
    return 0;
  }

  uint8_t skillIndex(uint16_t bit) const {
    for (uint8_t i = 0; i < 11; ++i) {
      if (bit == (uint16_t)(1u << i)) return i;
    }
    return 255;
  }

  uint8_t heroSkillManaCost(uint16_t bit, uint8_t heroLv) const {
    uint8_t base = 8;
    if (bit == SK_HOOK || bit == SK_BLINK) base = 6;
    else if (bit == SK_FIRE || bit == SK_POISON || bit == SK_CHAIN) base = 10;
    else if (bit == SK_SHIELD || bit == SK_FROST || bit == SK_LASER) base = 12;
    else if (bit == SK_NOVA || bit == SK_TOTEM) base = 16;
    else if (bit == SK_METEOR) base = 18;
    uint8_t discount = min((uint8_t)4, (uint8_t)(heroLv / 3));
    return max((int)4, (int)base - (int)discount);
  }

  uint16_t pickReadyOwnedSkill(const Unit& u) const {
    uint16_t ready[12];
    uint8_t n = 0;
    for (uint8_t i = 0; i < 11; ++i) {
      uint16_t b = (uint16_t)(1u << i);
      if (!(u.skillMask & b)) continue;
      if (u.skillCd[i] > 0.05f) continue;
      if (u.mana + 0.01f < (float)heroSkillManaCost(b, u.heroLv)) continue;
      ready[n++] = b;
    }
    if (!n) return 0;
    return ready[esp_random() % n];
  }

  bool heroHasReadySkill(const Unit& u) const {
    return pickReadyOwnedSkill(u) != 0;
  }

  void tickHeroManaAndSkillCooldowns(Unit& u, float dt) {
    if (u.role != Role::HERO) {
      if (u.skillCooldown > 0) u.skillCooldown -= dt;
      return;
    }

    if (u.maxMana <= 1.0f) {
      u.maxMana = 42.0f + u.heroLv * 5.0f;
      u.mana = u.maxMana;
    }

    float minCd = 999.0f;
    bool anyCd = false;
    for (uint8_t i = 0; i < 11; ++i) {
      if (u.skillCd[i] > 0.0f) {
        u.skillCd[i] = max(0.0f, u.skillCd[i] - dt);
        if (u.skillCd[i] > 0.0f) {
          anyCd = true;
          if (u.skillCd[i] < minCd) minCd = u.skillCd[i];
        }
      }
    }

    float regen = 3.2f + u.heroLv * 0.38f + repairLv * 0.12f;
    if (state == GameState::BETWEEN_WAVES) regen *= 1.6f;
    u.mana = min(u.maxMana, u.mana + regen * dt);

    // Legacy aggregate is only used as a tiny visual pulse. It no longer blocks every skill.
    u.skillCooldown = anyCd ? minCd : 0.0f;
  }

  uint16_t heroClassColor(uint8_t c) {
    switch (c % 6) {
      case 0: return canvas.color565(230, 214, 150); // knight
      case 1: return canvas.color565(255, 118, 54);  // pyro
      case 2: return canvas.color565(160, 92, 255);  // arcane
      case 3: return canvas.color565(112, 242, 190); // paladin
      case 4: return canvas.color565(80, 180, 255);  // rogue
      default: return canvas.color565(230, 230, 255); // tech
    }
  }

  uint16_t randomHeroSkills(uint8_t heroClass) {
    uint16_t m = 0;
    switch (heroClass % 6) {
      case 0: m = SK_HOOK | SK_SHIELD | SK_NOVA; break;
      case 1: m = SK_FIRE | SK_METEOR | SK_NOVA; break;
      case 2: m = SK_CHAIN | SK_LASER | SK_FROST; break;
      case 3: m = SK_SHIELD | SK_TOTEM | SK_CHAIN; break;
      case 4: m = SK_HOOK | SK_BLINK | SK_POISON; break;
      default: m = SK_LASER | SK_FIRE | SK_TOTEM; break;
    }
    uint8_t extra = 1 + (esp_random() % 3);
    while (extra--) m |= randomSkillBit();
    return m;
  }

  Role randomFriendlyRole() {
    uint8_t r = esp_random() % 100;
    if (r < 22) return Role::GUARD;
    if (r < 44) return Role::ARCHER;
    if (r < 60) return Role::MAGE;
    if (r < 72) return Role::BRUTE;
    if (r < 86) return Role::HEALER;
    return Role::REPAIR;
  }

  void announceHero(Unit& u) {
    snprintf(heroLine, sizeof(heroLine), "MECH %s L%u", heroClassName(u.heroClass), u.heroLv);
    cachedHeroUntilMs = 0;
  }

  int focusHeroIndex() {
    uint32_t now = millis();
    if (cachedHeroIdx >= 0 && cachedHeroIdx < Config::MAX_UNITS && now < cachedHeroUntilMs) {
      Unit& h = units[cachedHeroIdx];
      if (h.active && h.team == 0 && h.role == Role::HERO) return cachedHeroIdx;
    }
    int best = -1;
    int score = -1;
    for (int i = 0; i < Config::MAX_UNITS; ++i) {
      if (!units[i].active || units[i].team != 0 || units[i].role != Role::HERO) continue;
      int s = (int)units[i].heroLv * 1000 + (int)units[i].heroXp;
      if (s > score) { score = s; best = i; }
    }
    cachedHeroIdx = best;
    cachedHeroUntilMs = now + 500;
    return best;
  }

  int tdPathCount() const { return 10; }

  void tdPathPoint(int idx, float& x, float& y) const {
    // Hand-made Atom S3R maze: mobs enter from upper-left and snake to edge throne.
    switch (idx) {
      case 0: x = 6;   y = 18; break;
      case 1: x = 32;  y = 18; break;
      case 2: x = 32;  y = 50; break;
      case 3: x = 14;  y = 50; break;
      case 4: x = 14;  y = 82; break;
      case 5: x = 56;  y = 82; break;
      case 6: x = 56;  y = 34; break;
      case 7: x = 86;  y = 34; break;
      case 8: x = 86;  y = 78; break;
      default: x = bases[0].x - 13; y = bases[0].y; break;
    }
  }

  void tdPathStart(float& x, float& y) const {
    tdPathPoint(0, x, y);
  }

  void tdAdvanceEnemyWaypoint(Unit& u, float& tx, float& ty) {
    int wp = u.targetResId;
    if (wp < 0) wp = 0;
    if (wp >= tdPathCount()) {
      tx = bases[0].x;
      ty = bases[0].y;
      u.targetResId = tdPathCount();
      return;
    }

    tdPathPoint(wp, tx, ty);
    if (distSq(u.x, u.y, tx, ty) < 42.0f) {
      wp++;
      u.targetResId = wp;
      if (wp >= tdPathCount()) {
        tx = bases[0].x;
        ty = bases[0].y;
      } else {
        tdPathPoint(wp, tx, ty);
      }
    }
  }

  void drawMazePath() {
    uint16_t wall = canvas.color565(31, 38, 52);
    uint16_t path = canvas.color565(10, 14, 22);
    uint16_t edge = dimColor(playerColor, 0.28f);
    float x1, y1, x2, y2;
    tdPathPoint(0, x1, y1);
    canvas.fillCircle((int)x1, (int)y1, 5, canvas.color565(26, 14, 18));
    canvas.drawCircle((int)x1, (int)y1, 6, dimColor(enemyColor, 0.55f));
    for (int i = 1; i < tdPathCount(); ++i) {
      tdPathPoint(i, x2, y2);
      canvas.drawLine((int)x1, (int)y1 - 4, (int)x2, (int)y2 - 4, wall);
      canvas.drawLine((int)x1, (int)y1 + 4, (int)x2, (int)y2 + 4, wall);
      canvas.drawLine((int)x1, (int)y1, (int)x2, (int)y2, path);
      canvas.drawLine((int)x1, (int)y1 - 1, (int)x2, (int)y2 - 1, dimColor(path, 0.85f));
      canvas.drawLine((int)x1, (int)y1 + 1, (int)x2, (int)y2 + 1, dimColor(path, 0.85f));
      canvas.drawCircle((int)x2, (int)y2, 3, edge);
      x1 = x2;
      y1 = y2;
    }
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(dimColor(enemyColor, 0.75f), TFT_TRANSPARENT);
    canvas.drawString(groundMode ? "DROP" : "INV", 2, 6);
  }

  void spawnUnit(uint8_t team, Role role, float x, float y) {
    // v8.15B: the throne can command only one living hero at a time.
    if (team == 0 && role == Role::HERO && aliveHeroes() >= 1) return;

    for (auto& u : units) {
      if (!u.active) {
        u = Unit{};
        u.active = true;
        u.team = team;
        u.role = role;
        u.x = constrain(x + (int)(esp_random() % 9) - 4, 2.0f, (float)Config::SCREEN_W - 2.0f);
        u.y = constrain(y + (int)(esp_random() % 9) - 4, 2.0f, (float)Config::SCREEN_H - 2.0f);
        u.targetResId = -1;
        u.carrying = false;
        u.cooldown = 0;
        u.skillCooldown = 0;
        u.skillCooldownMax = 0;
        for (uint8_t si = 0; si < 11; ++si) { u.skillCd[si] = 0.0f; u.skillCdMax[si] = 0.0f; }
        u.lastSkillUsed = 0;
        u.idleTime = 0;
        u.heroXp = 0;
        u.heroLv = 1;
        u.heroClass = esp_random() % 6;
        u.maxMana = 0.0f;
        u.mana = 0.0f;
        u.visualSeed = esp_random() & 0xFF;
        u.skillMask = 0;
        if (team == 0 && role == Role::HERO) {
          u.skillMask = randomHeroSkills(u.heroClass);
          announceHero(u);
        } else if (team == 0 && role != Role::SCAVENGER && role != Role::TOWER && role != Role::REPAIR) {
          // Procedural non-hero defenders sometimes get one small cliché ability.
          if ((esp_random() % 100) < 34) u.skillMask = randomSkillBit();
        }

        if (team == 0) {
          switch (role) {
            case Role::SCAVENGER: u.maxHp = 16 + unitLv * 3; u.speed = 24.0f; u.dmg = 0; u.rangeSq = 0; break;
            case Role::REPAIR:    u.maxHp = 20 + repairLv * 4; u.speed = 18.0f; u.dmg = 0; u.rangeSq = 0; break;
            case Role::HEALER:    u.maxHp = 26 + unitLv * 4 + repairLv * 3; u.speed = 17.0f; u.dmg = 0; u.rangeSq = 560.0f; break;
            case Role::HERO:
              u.maxHp = 64 + unitLv * 7 + u.heroLv * 9;
              u.maxMana = 42.0f + u.heroLv * 5.0f;
              u.mana = u.maxMana;
              u.speed = 15.5f + (u.heroClass == 4 ? 2.0f : 0.0f);
              u.dmg = 10 + dmgLv * 2.0f + u.heroLv * 2.2f;
              u.rangeSq = 650.0f + u.heroLv * 34.0f;
              break;
            case Role::ARCHER:    u.maxHp = 24 + unitLv * 4; u.speed = 16.0f; u.dmg = 6 + dmgLv * 1.8f; u.rangeSq = 520.0f; break;
            case Role::MAGE:      u.maxHp = 20 + unitLv * 3; u.speed = 13.0f; u.dmg = 10 + dmgLv * 2.2f; u.rangeSq = 700.0f; break;
            case Role::BRUTE:     u.maxHp = 70 + unitLv * 8; u.speed = 8.0f;  u.dmg = 12 + dmgLv * 2.0f; u.rangeSq = 120.0f; break;
            case Role::TOWER:     u.maxHp = 70 + towerLv * 20; u.speed = 0.0f; u.dmg = 8 + towerLv * 2.5f + dmgLv; u.rangeSq = 780.0f; break;
            default:              u.maxHp = 30 + unitLv * 5; u.speed = 15.0f; u.dmg = 7 + dmgLv * 1.5f; u.rangeSq = 170.0f; break;
          }
        } else {
          float w = max(1, (int)wave);
          switch (role) {
            case Role::ARCHER: u.maxHp = 18 + w * 3; u.speed = 13.5f; u.dmg = 5 + w * 0.75f; u.rangeSq = 460.0f; break;
            case Role::MAGE:   u.maxHp = 16 + w * 2.5f; u.speed = 11.0f; u.dmg = 8 + w * 0.9f;  u.rangeSq = 620.0f; break;
            case Role::BRUTE:  u.maxHp = 70 + w * 8; u.speed = 7.0f; u.dmg = 11 + w * 1.1f; u.rangeSq = 110.0f; break;
            default:           u.maxHp = 24 + w * 4; u.speed = 13.0f + min(6.0f, w * 0.18f); u.dmg = 6 + w * 0.8f; u.rangeSq = 130.0f; break;
          }
        }
        u.hp = u.maxHp;
        return;
      }
    }
  }

  void spawnEnemy() {
    // v8.15B: back to original swarm — enemies enter from all screen edges.
    int side = esp_random() % 4;
    float x = 0, y = 0;
    if (side == 0) { x = esp_random() % Config::SCREEN_W; y = 1; }
    else if (side == 1) { x = Config::SCREEN_W - 2; y = esp_random() % Config::SCREEN_H; }
    else if (side == 2) { x = esp_random() % Config::SCREEN_W; y = Config::SCREEN_H - 2; }
    else { x = 1; y = esp_random() % Config::SCREEN_H; }

    uint32_t r = esp_random() % 100;
    Role role = Role::GUARD;
    uint8_t mageChance = min(26, 6 + (int)wave);
    uint8_t bruteChance = min(22, 5 + (int)wave / 2);
    if (r < bruteChance) role = Role::BRUTE;
    else if (r < bruteChance + mageChance) role = Role::MAGE;
    else if (r < bruteChance + mageChance + 28) role = Role::ARCHER;
    else role = Role::GUARD;
    spawnUnit(1, role, x, y);
  }

  int findNearestEnemy(float x, float y, float maxSq) {
    int best = -1;
    float bd = maxSq;
    if (spatialReady) {
      int cx = constrain((int)x / 16, 0, 7);
      int cy = constrain((int)y / 16, 0, 7);
      int rad = constrain((int)(sqrtf(maxSq) / 16.0f) + 1, 1, 7);
      for (int yy = max(0, cy - rad); yy <= min(7, cy + rad); yy++) {
        for (int xx = max(0, cx - rad); xx <= min(7, cx + rad); xx++) {
          int j = gridHead[xx + yy * 8];
          while (j != -1) {
            if (units[j].active && units[j].team == 1) {
              float d = distSq(x, y, units[j].x, units[j].y);
              if (d < bd) { bd = d; best = j; }
            }
            j = gridNext[j];
          }
        }
      }
      return best;
    }
    for (int i = 0; i < Config::MAX_UNITS; i++) {
      if (!units[i].active || units[i].team != 1) continue;
      float d = distSq(x, y, units[i].x, units[i].y);
      if (d < bd) { bd = d; best = i; }
    }
    return best;
  }

  int findNearestFriendlyTarget(float x, float y) {
    int best = -1;
    float bd = 800.0f;
    if (spatialReady) {
      int cx = constrain((int)x / 16, 0, 7);
      int cy = constrain((int)y / 16, 0, 7);
      for (int yy = max(0, cy - 2); yy <= min(7, cy + 2); yy++) {
        for (int xx = max(0, cx - 2); xx <= min(7, cx + 2); xx++) {
          int j = gridHead[xx + yy * 8];
          while (j != -1) {
            if (units[j].active && units[j].team == 0) {
              float d = distSq(x, y, units[j].x, units[j].y);
              if (!(units[j].role == Role::SCAVENGER && d > 400.0f) && d < bd) { bd = d; best = j; }
            }
            j = gridNext[j];
          }
        }
      }
      return best;
    }
    for (int i = 0; i < Config::MAX_UNITS; i++) {
      if (!units[i].active || units[i].team != 0) continue;
      float d = distSq(x, y, units[i].x, units[i].y);
      if (units[i].role == Role::SCAVENGER && d > 400.0f) continue;
      if (d < bd) { bd = d; best = i; }
    }
    return best;
  }

  void damageThrone(float dmg) {
    bases[0].hp -= dmg;
    spawnParticle(bases[0].x, bases[0].y, enemyColor, 2);
  }

  bool insertRecord(uint32_t ms, uint16_t w, uint16_t col, uint16_t mid) {
    int pos = -1;
    for (int i = 0; i < 4; i++) {
      if (ms > bestMs[i]) { pos = i; break; }
    }
    if (pos < 0) return false;
    for (int i = 3; i > pos; i--) {
      bestMs[i] = bestMs[i-1]; bestWave[i] = bestWave[i-1]; bestColor[i] = bestColor[i-1]; bestModel[i] = bestModel[i-1];
    }
    bestMs[pos] = ms; bestWave[pos] = w; bestColor[pos] = col; bestModel[pos] = mid;
    recordsToastUntilMs = millis() + 10000UL;
    return true;
  }

  void loadRecords() {
    if (recordsLoaded) return;
    recordsLoaded = true;
    Preferences p;
    if (!p.begin("tdsurv", false)) return;   // E4S2: create namespace on first boot, avoids NVS NOT_FOUND
    SaveRec rec = {};
    size_t got = p.getBytes("rec", &rec, sizeof(rec));
    p.end();
    if (got != sizeof(rec) || rec.magic != 0x54445356UL || rec.version != 1) return;
    uint16_t old = rec.crc;
    rec.crc = 0;
    if (crc16((uint8_t*)&rec, sizeof(rec)) != old) return;
    memcpy(bestMs, rec.bestMs, sizeof(bestMs));
    memcpy(bestWave, rec.bestWave, sizeof(bestWave));
    memcpy(bestColor, rec.bestColor, sizeof(bestColor));
    memcpy(bestModel, rec.bestModel, sizeof(bestModel));
    modelId = max((uint16_t)1, rec.nextModel);
  }

  void saveRecords() {
    SaveRec rec = {};
    rec.magic = 0x54445356UL;
    rec.version = 1;
    memcpy(rec.bestMs, bestMs, sizeof(bestMs));
    memcpy(rec.bestWave, bestWave, sizeof(bestWave));
    memcpy(rec.bestColor, bestColor, sizeof(bestColor));
    memcpy(rec.bestModel, bestModel, sizeof(bestModel));
    rec.nextModel = modelId;
    rec.crc = 0;
    rec.crc = crc16((uint8_t*)&rec, sizeof(rec));
    Preferences p;
    if (p.begin("tdsurv", false)) {
      p.putBytes("rec", &rec, sizeof(rec));
      p.end();
    }
  }

  uint32_t upgradeCost(uint8_t lv, uint16_t base) const {
    return (uint32_t)base + (uint32_t)lv * (uint32_t)(base / 2 + 15) + (uint32_t)wave * 3UL;
  }

  uint32_t heroBuyCost() const {
    // v8.15B: only one hero may exist. If he dies, Janus can buy a new body with gold.
    return 105UL + (uint32_t)wave * 4UL + (uint32_t)level * 8UL;
  }

  void buildTower() {
    if (towerCount >= 8) return;
    float a = (float)towerCount * 0.785398f + 0.35f;
    float r = 22.0f + (towerCount & 1) * 10.0f;
    float x = bases[0].x + cosf(a) * r;
    float y = bases[0].y + sinf(a) * r;
    spawnUnit(0, Role::TOWER, x, y);
    towerCount++;
  }

  void janusAutoUpgrade() {
    uint32_t now = millis();
    if (now - lastAutoBuyMs < 900) return;
    lastAutoBuyMs = now;

    if (bases[0].hp < bases[0].maxHp * 0.55f && gold >= 18) {
      uint32_t repairBudget = (uint32_t)(34 + repairLv * 3);
      uint32_t spend = (gold < repairBudget) ? gold : repairBudget;
      gold -= spend;
      bases[0].hp = min(bases[0].maxHp, bases[0].hp + (float)spend * (1.4f + repairLv * 0.18f));
      return;
    }

    if (aliveHeroes() < 1 && gold >= heroBuyCost()) {
      // v8.15B: hero death does not hurt the throne and does not restart the run.
      // Janus buys exactly one new hero body when enough gold is available.
      gold -= heroBuyCost();
      spawnUnit(0, Role::HERO, bases[0].x, bases[0].y - 12);
      playBeep(1200, 80);
      return;
    }

    if (aliveFriendlies() < 3 + unitLv && gold >= 22) {
      gold -= 22;
      spawnUnit(0, randomFriendlyRole(), bases[0].x, bases[0].y);
      return;
    }
    if (repairLv > 0 && aliveFriendlies() < 5 + unitLv && gold >= 28 && (esp_random() % 100) < 34) {
      gold -= 28;
      spawnUnit(0, Role::REPAIR, bases[0].x, bases[0].y);
      return;
    }
    if (gold >= 80 + towerCount * 28 && towerCount < 2 + towerLv) {
      gold -= 80 + towerCount * 28;
      buildTower();
      playBeep(900, 35);
      return;
    }

    uint8_t choice = esp_random() % 7;
    if (choice == 0 && gold >= upgradeCost(dmgLv, 55)) { gold -= upgradeCost(dmgLv, 55); dmgLv++; }
    else if (choice == 1 && gold >= upgradeCost(fireLv, 50)) { gold -= upgradeCost(fireLv, 50); fireLv++; }
    else if (choice == 2 && gold >= upgradeCost(throneLv, 70)) { gold -= upgradeCost(throneLv, 70); throneLv++; bases[0].maxHp += 80; bases[0].hp += 80; }
    else if (choice == 3 && gold >= upgradeCost(unitLv, 58)) { gold -= upgradeCost(unitLv, 58); unitLv++; spawnUnit(0, Role::ARCHER, bases[0].x, bases[0].y); }
    else if (choice == 4 && gold >= upgradeCost(towerLv, 90)) { gold -= upgradeCost(towerLv, 90); towerLv++; }
    else if (choice == 5 && aliveHeroes() < 1 && gold >= heroBuyCost()) { gold -= heroBuyCost(); spawnUnit(0, Role::HERO, bases[0].x, bases[0].y - 12); }
    else if (gold >= upgradeCost(repairLv, 62)) { gold -= upgradeCost(repairLv, 62); repairLv++; spawnUnit(0, Role::HEALER, bases[0].x, bases[0].y); }
  }

  void applyMiningLuckWaveBoost() {
    if (miningLuckCharges == 0 && miningLuckPower <= 0.03f) return;
    float pEntropy, pMood, pFatigue, pVigor;
    uint8_t pMoodCode;
    janusPersonalSnapshot(pEntropy, pMood, pFatigue, pVigor, pMoodCode);
    float personalLuckUse = constrain(0.72f + pMood * 0.24f + pVigor * 0.22f - pFatigue * 0.14f, 0.58f, 1.24f);
    float p = constrain(miningLuckPower * personalLuckUse, 0.10f, 2.45f);
    waveLootMul = min(1.90f, waveLootMul + 0.08f + p * 0.055f);
    waveDamageMul = min(1.62f, waveDamageMul + p * 0.030f);
    waveShieldMul = max(0.68f, waveShieldMul - p * 0.024f);
    gold += 4 + (uint32_t)(p * 7.0f);
    bases[0].hp = min(bases[0].maxHp, bases[0].hp + 4.0f + p * 5.0f);
    if (miningLuckCharges > 0) miningLuckCharges--;
    miningLuckPower *= 0.72f;
    snprintf(waveBoostLine, sizeof(waveBoostLine), "B:HASH+%u", (unsigned)miningLuckCharges);
  }

  void startWave() {
    if (pendingGroundOrder) commitPendingGroundOrder();
    state = GameState::PLAYING;
    wave++;
    groundMissionStartMs = millis();
    activeMissionWave = (uint8_t)min(255, (int)wave);
    snprintf(groundMissionLine, sizeof(groundMissionLine), "%s S%02u", groundStyleName(), (unsigned)groundSector);
    enemiesKilledThisWave = 0;
    if (groundStyle == 1) enemiesLeftToSpawn = 5 + wave * 2 + min(18, (int)wave);
    else if (groundStyle == 2) enemiesLeftToSpawn = 9 + wave * 4 + min(34, (int)wave * 2);
    else enemiesLeftToSpawn = 7 + wave * 3 + min(26, (int)wave * 2);
    waveSpawnTimer = 0.0f;
    applySensorWaveBoost();
    applyMiningLuckWaveBoost();
    groundBannerUntilMs = millis() + 2800UL;
    playBeep(420 + wave * 10, 50);
  }

  void startBetweenWaves() {
    state = GameState::BETWEEN_WAVES;
    betweenWaveTimer = pendingGroundOrder ? 3.0f : 5.0f;
    uint32_t reward = 25 + wave * 8;
    gold += reward;
    xp += reward / 2;
    janusPersonalNudge(7, (uint16_t)min(96, (int)wave + 16));
    if (xp >= level * 70UL) { xp -= level * 70UL; level++; gold += 30 + level * 5; if ((esp_random() % 100) < 45) spawnUnit(0, randomFriendlyRole(), bases[0].x, bases[0].y); }
    groundToastUntilMs = millis() + 3200UL;
    snprintf(groundToastLine, sizeof(groundToastLine), "MISSION DONE W%u", (unsigned)wave);
    // v8.15B: no free/extra hero between waves; a dead hero is bought with gold by janusAutoUpgrade().
    playBeep(1000, 70);
  }

  void startNewRun() {
    loadRecords();
    state = GameState::BETWEEN_WAVES;
    gameOverTimer = 0.0f;
    betweenWaveTimer = 3.0f;
    runStartMs = millis();
    survivalMs = 0;
    wave = 0;
    gold = 45;
    xp = 0;
    level = 1;
    dmgLv = fireLv = throneLv = unitLv = towerLv = repairLv = 0;
    towerCount = 0;
    heroLimit = 1; // v8.15B: hard cap, only one living hero for the throne.
    snprintf(heroLine, sizeof(heroLine), "MECH L1");
    enemiesLeftToSpawn = 0;
    enemiesKilledThisWave = 0;
    throneCooldown = 0;
    shockwaveRadius = 0;
    nextSoundTime = 0;
    resetWaveBoosts();
    janusPersonalReset();
    playerColor = tournamentColor(modelId);
    enemyColor = canvas.color565(230, 72, 98);
    configureGroundWorld(janusGroundOrderSector, janusGroundOrderPriority,
                         janusGroundOrderMission ? janusGroundOrderMission : modelId,
                         janusGroundOrderFlags, janusGroundOrderTarget);
    // v8.15B: throne restored to center; mob loot is absorbed automatically.
    bases[0] = {Config::SCREEN_W * 0.5f, Config::SCREEN_H * 0.5f, 520, 520, 0, 0, playerColor, 0.0f};
    bases[1] = {0, 0, 0, 0, 0, 1, enemyColor, 0.0f};

    for (auto& u : units) u.active = false;
    for (auto& r : resources) r.active = false;
    for (auto& p : particles) p.active = false;
    for (auto& s : spits) s.active = false;
    spatialReady = false;
    cachedHeroUntilMs = 0;

    spawnUnit(0, Role::SCAVENGER, bases[0].x - 6, bases[0].y + 8);
    spawnUnit(0, Role::SCAVENGER, bases[0].x + 6, bases[0].y + 8);
    spawnUnit(0, Role::GUARD, bases[0].x - 9, bases[0].y - 5);
    spawnUnit(0, Role::ARCHER, bases[0].x + 9, bases[0].y - 5);
    spawnUnit(0, Role::HEALER, bases[0].x + 10, bases[0].y + 8);
    spawnUnit(0, Role::HERO, bases[0].x, bases[0].y - 12);
    buildTower();
    lastTick = millis();
    fixedAccumulator = 0.0f;
  }

  void finishRun() {
    survivalMs = millis() - runStartMs;
    bool recChanged = insertRecord(survivalMs, wave, playerColor, modelId);
    modelId++;
    saveRecords();
    if (recChanged) recordsToastUntilMs = millis() + 10000UL;
    spawnParticle(bases[0].x, bases[0].y, TFT_WHITE, 120);
    spawnParticle(bases[0].x, bases[0].y, playerColor, 80);
    state = GameState::GAME_OVER;
    gameOverTimer = 3.0f;
    playBeep(90, 500);
  }

  void handleInput() {
    M5.update();

    // ATOM S3R: short press on the screen button cycles backlight including full OFF.
    // Long hold toggles sound without also stepping brightness on release.
    static bool holdConsumed = false;
    if (M5.BtnA.wasHold()) {
      soundEnabled = !soundEnabled;
      holdConsumed = true;
      saveUiState();
      if (soundEnabled) M5.Speaker.tone(1000, 100);
    } else if (M5.BtnA.wasReleased()) {
      if (!holdConsumed) {
        brightIdx = (brightIdx + 1) % 5;
        M5.Display.setBrightness(brightLevels[brightIdx]);
        saveUiState();
      }
      holdConsumed = false;
    }
  }

  void updateEchoBaseMicRms() {
    uint32_t now = millis();
    if (now - lastEchoMicPollMs < 50) return;
    lastEchoMicPollMs = now;

#if JANUS_AUDIO_LIVE_TX_ENABLE
    // During live audio, janusAudioTxTick() is the single owner of EchoBase.record().
    // Double-reading the I2S mic caused gaps and underruns on Core2.
    if (janusAudioTxEnabled) return;
#endif

    if (!echoBaseReady) {
      swarmMicFails++;
      echoMicReadFail++;
      swarmMicRms *= 0.985f;
      swarmMicPeak *= 0.992f;
      return;
    }

    bool ok = echobase.record(echoMicBuf, sizeof(echoMicBuf));
    if (!ok) {
      swarmMicFails++;
      echoMicReadFail++;
      swarmMicRms *= 0.985f;
      swarmMicPeak *= 0.992f;
      return;
    }

    int samples = sizeof(echoMicBuf) / 2;
    int16_t* pcm = (int16_t*)echoMicBuf;
    int64_t sum = 0;
    int16_t minV = 32767, maxV = -32768;
    for (int i = 0; i < samples; i++) {
      int16_t v = pcm[i];
      sum += v;
      if (v < minV) minV = v;
      if (v > maxV) maxV = v;
    }
    float mean = (float)sum / (float)samples;
    float sumSq = 0.0f;
    for (int i = 0; i < samples; i++) {
      float centered = (float)pcm[i] - mean;
      sumSq += centered * centered;
    }
    float rmsRaw = sqrtf(sumSq / (float)samples);
    float pp = (float)(maxV - minV);
    if (pp < 2.0f) {
      swarmMicFails++;
      echoMicReadFail++;
      swarmMicRms *= 0.985f;
      swarmMicPeak *= 0.992f;
      return;
    }

    swarmMicFrames++;
    echoMicReadOk++;

    // v8.31E MIC FLOOR GUARD: keep raw RMS for diagnostics, update floor with
    // anti-self-mute logic, then gate with both RMS and peak-to-peak evidence.
    micRawRms = micRawRms * 0.78f + rmsRaw * 0.22f;
    float floorNow = janusTronUpdateMicFloor(rmsRaw, pp, now);
    float gated = janusTronComputeGatedMic(rmsRaw, pp, floorNow);
    micLastGated = gated;
    micSignal = micSignal * (1.0f - JANUS_TRON_MIC_SIGNAL_ALPHA) + gated * JANUS_TRON_MIC_SIGNAL_ALPHA;
    swarmMicRms = swarmMicRms * (1.0f - JANUS_TRON_MIC_RMS_ALPHA) + micSignal * JANUS_TRON_MIC_RMS_ALPHA;
    if (gated > swarmMicPeak) swarmMicPeak = gated;
    else swarmMicPeak *= 0.975f;

    if ((swarmMicRms > 720.0f || swarmMicPeak > 2600.0f) && (now - lastShockwaveMicMs > 30000UL)) {
      lastShockwaveMicMs = now;
      triggerShockwave();
    }
  }

  void triggerShockwave() {
    if (shockwaveRadius > 0.0f) return;
    playBeep(150, 150);
    shockwaveRadius = 1.0f;
    float cx = bases[0].x;
    float cy = bases[0].y;
    for (auto& u : units) {
      if (!u.active || u.team != 1) continue;
      float dx = u.x - cx;
      float dy = u.y - cy;
      float d = sqrtf(dx*dx + dy*dy) + 0.1f;
      float force = 1250.0f / d;
      u.vx += (dx / d) * force;
      u.vy += (dy / d) * force;
      u.hp -= 4.0f + wave * 0.15f;
      spawnParticle(u.x, u.y, TFT_CYAN, 3);
    }
  }

  void updateEffects(float dt) {
    for (auto& p : particles) {
      if (!p.active) continue;
      p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt;
      if (p.life <= 0) p.active = false;
    }
    for (auto& s : spits) {
      if (!s.active) continue;
      s.life -= dt;
      if (s.life <= 0) s.active = false;
    }
    if (shockwaveRadius > 0.0f) {
      shockwaveRadius += 120.0f * dt;
      if (shockwaveRadius > 70.0f) shockwaveRadius = 0.0f;
    }
  }

  void updateThrone(float dt) {
    if (throneCooldown > 0) throneCooldown -= dt;
    if (throneCooldown > 0) return;
    int target = findNearestEnemy(bases[0].x, bases[0].y, 1500.0f + fireLv * 120.0f);
    if (target < 0) return;
    float dmg = (11.0f + dmgLv * 2.2f + throneLv * 1.5f) * waveDamageMul;
    units[target].hp -= dmg;
    addBeam(bases[0].x, bases[0].y, units[target].x, units[target].y, brightenColor(playerColor, 0.35f), 0.13f);
    spawnParticle(units[target].x, units[target].y, TFT_WHITE, 3);
    throneCooldown = max(0.08f, (0.42f - fireLv * 0.025f) / waveFireMul);
    if (units[target].hp <= 0) killEnemy(target);
  }

  void killEnemy(int idx) {
    if (idx < 0 || idx >= Config::MAX_UNITS || !units[idx].active) return;
    float x = units[idx].x, y = units[idx].y;
    uint16_t loot = 8 + (esp_random() % (8 + max(1, (int)wave)));
    if (units[idx].role == Role::BRUTE) loot += 16 + wave;
    if (units[idx].role == Role::MAGE) loot += 8;
    loot = (uint16_t)max(1, (int)((float)loot * waveLootMul));
    units[idx].active = false;
    enemiesKilledThisWave++;
    grantHeroXp((uint16_t)(3 + (loot / 5)));
    absorbLootToThrone(x, y, loot);
    spawnParticle(x, y, enemyColor, 10);
    spawnParticle(x, y, TFT_WHITE, 5);
    playBeep(300, 24);
  }

  void updateWave(float dt) {
    if (state == GameState::BETWEEN_WAVES) {
      betweenWaveTimer -= dt;
      if (betweenWaveTimer <= 0.0f) startWave();
      return;
    }
    if (state != GameState::PLAYING) return;

    if (enemiesLeftToSpawn > 0) {
      waveSpawnTimer -= dt;
      float rate = max(0.12f, 0.72f - wave * 0.018f);
      if (waveSpawnTimer <= 0.0f) {
        spawnEnemy();
        enemiesLeftToSpawn--;
        waveSpawnTimer = rate;
      }
    } else if (aliveEnemies() == 0) {
      startBetweenWaves();
    }
  }

  int findHurtFriendly(float x, float y, float maxSq) {
    int best = -1;
    float bd = maxSq;
    for (int i = 0; i < Config::MAX_UNITS; i++) {
      if (!units[i].active || units[i].team != 0 || units[i].role == Role::TOWER) continue;
      if (units[i].hp >= units[i].maxHp * 0.92f) continue;
      float d = distSq(x, y, units[i].x, units[i].y);
      if (d < bd) { bd = d; best = i; }
    }
    return best;
  }

  void grantHeroXp(uint16_t amount) {
    for (int i = 0; i < Config::MAX_UNITS; i++) {
      if (!units[i].active || units[i].team != 0 || units[i].role != Role::HERO) continue;
      Unit& h = units[i];
      h.heroXp += amount;
      uint16_t need = 36 + h.heroLv * 24;
      if (h.heroXp >= need && h.heroLv < 9) {
        h.heroXp -= need;
        h.heroLv++;
        h.maxHp += 11;
        h.hp = min(h.maxHp, h.hp + 22.0f);
        h.maxMana += 7.0f;
        h.mana = min(h.maxMana, h.mana + 18.0f);
        h.dmg += 2.4f;
        h.rangeSq += 35.0f;
        if ((esp_random() % 100) < 55) h.skillMask |= randomSkillBit();
        announceHero(h);
        spawnParticle(h.x, h.y, heroClassColor(h.heroClass), 12);
        playBeep(1400, 45);
      }
    }
  }

  void heroAoE(float x, float y, float radiusSq, float dmg, uint16_t c) {
    if (spatialReady) {
      int cx = constrain((int)x / 16, 0, 7);
      int cy = constrain((int)y / 16, 0, 7);
      int rad = constrain((int)(sqrtf(radiusSq) / 16.0f) + 1, 1, 7);
      for (int yy = max(0, cy - rad); yy <= min(7, cy + rad); yy++) {
        for (int xx = max(0, cx - rad); xx <= min(7, cx + rad); xx++) {
          int j = gridHead[xx + yy * 8];
          while (j != -1) {
            int cur = j;
            j = gridNext[j];
            if (!units[cur].active || units[cur].team != 1) continue;
            if (distSq(x, y, units[cur].x, units[cur].y) > radiusSq) continue;
            units[cur].hp -= dmg;
            addBeam(x, y, units[cur].x, units[cur].y, c, 0.12f);
            spawnParticle(units[cur].x, units[cur].y, c, 2);
            if (units[cur].hp <= 0) killEnemy(cur);
          }
        }
      }
      return;
    }
    for (int i = 0; i < Config::MAX_UNITS; i++) {
      if (!units[i].active || units[i].team != 1) continue;
      if (distSq(x, y, units[i].x, units[i].y) > radiusSq) continue;
      units[i].hp -= dmg;
      addBeam(x, y, units[i].x, units[i].y, c, 0.12f);
      spawnParticle(units[i].x, units[i].y, c, 2);
      if (units[i].hp <= 0) killEnemy(i);
    }
  }


  int heroChainGrid(float x, float y, float radiusSq, float dmg, uint16_t c, float fromX, float fromY, int maxHits) {
    int hits = 0;
    if (spatialReady) {
      int cx = constrain((int)x / 16, 0, 7);
      int cy = constrain((int)y / 16, 0, 7);
      int rad = constrain((int)(sqrtf(radiusSq) / 16.0f) + 1, 1, 7);
      for (int yy = max(0, cy - rad); yy <= min(7, cy + rad) && hits < maxHits; yy++) {
        for (int xx = max(0, cx - rad); xx <= min(7, cx + rad) && hits < maxHits; xx++) {
          int j = gridHead[xx + yy * 8];
          while (j != -1 && hits < maxHits) {
            int cur = j;
            j = gridNext[j];
            if (!units[cur].active || units[cur].team != 1) continue;
            if (distSq(x, y, units[cur].x, units[cur].y) > radiusSq) continue;
            units[cur].hp -= dmg;
            addBeam(hits == 0 ? fromX : x, hits == 0 ? fromY : y, units[cur].x, units[cur].y, c, 0.13f);
            if (units[cur].hp <= 0) killEnemy(cur);
            hits++;
          }
        }
      }
      return hits;
    }
    for (int i = 0; i < Config::MAX_UNITS && hits < maxHits; i++) {
      if (!units[i].active || units[i].team != 1) continue;
      if (distSq(x, y, units[i].x, units[i].y) > radiusSq) continue;
      units[i].hp -= dmg;
      addBeam(hits == 0 ? fromX : x, hits == 0 ? fromY : y, units[i].x, units[i].y, c, 0.13f);
      if (units[i].hp <= 0) killEnemy(i);
      hits++;
    }
    return hits;
  }

  void heroFrostGrid(float x, float y, float radiusSq, float dmg) {
    if (spatialReady) {
      int cx = constrain((int)x / 16, 0, 7);
      int cy = constrain((int)y / 16, 0, 7);
      int rad = constrain((int)(sqrtf(radiusSq) / 16.0f) + 1, 1, 7);
      for (int yy = max(0, cy - rad); yy <= min(7, cy + rad); yy++) {
        for (int xx = max(0, cx - rad); xx <= min(7, cx + rad); xx++) {
          int j = gridHead[xx + yy * 8];
          while (j != -1) {
            int cur = j;
            j = gridNext[j];
            if (!units[cur].active || units[cur].team != 1) continue;
            if (distSq(x, y, units[cur].x, units[cur].y) > radiusSq) continue;
            units[cur].hp -= dmg;
            units[cur].x += (bases[0].x - units[cur].x) * -0.05f;
            units[cur].y += (bases[0].y - units[cur].y) * -0.05f;
            spawnParticle(units[cur].x, units[cur].y, TFT_CYAN, 3);
            if (units[cur].hp <= 0) killEnemy(cur);
          }
        }
      }
      return;
    }
    for (int i = 0; i < Config::MAX_UNITS; ++i) {
      if (!units[i].active || units[i].team != 1) continue;
      if (distSq(x,y,units[i].x,units[i].y) > radiusSq) continue;
      units[i].hp -= dmg;
      units[i].x += (bases[0].x - units[i].x) * -0.05f;
      units[i].y += (bases[0].y - units[i].y) * -0.05f;
      spawnParticle(units[i].x, units[i].y, TFT_CYAN, 3);
      if (units[i].hp <= 0) killEnemy(i);
    }
  }

  void heroTotemGrid(float x, float y, float radiusSq, float heal) {
    if (spatialReady) {
      int cx = constrain((int)x / 16, 0, 7);
      int cy = constrain((int)y / 16, 0, 7);
      int rad = constrain((int)(sqrtf(radiusSq) / 16.0f) + 1, 1, 7);
      for (int yy = max(0, cy - rad); yy <= min(7, cy + rad); yy++) {
        for (int xx = max(0, cx - rad); xx <= min(7, cx + rad); xx++) {
          int j = gridHead[xx + yy * 8];
          while (j != -1) {
            int cur = j;
            j = gridNext[j];
            if (!units[cur].active || units[cur].team != 0) continue;
            if (distSq(x,y,units[cur].x,units[cur].y) > radiusSq) continue;
            units[cur].hp = min(units[cur].maxHp, units[cur].hp + heal);
          }
        }
      }
      return;
    }
    for (int i = 0; i < Config::MAX_UNITS; ++i) {
      if (!units[i].active || units[i].team != 0) continue;
      if (distSq(x,y,units[i].x,units[i].y) > radiusSq) continue;
      units[i].hp = min(units[i].maxHp, units[i].hp + heal);
    }
  }

  void castHeroSkill(Unit& u, int targetIdx) {
    if (u.role != Role::HERO || targetIdx < 0 || !units[targetIdx].active) return;

    uint16_t chosen = pickReadyOwnedSkill(u);
    if (!chosen) return;

    uint8_t idx = skillIndex(chosen);
    if (idx >= 11) return;

    uint8_t manaCost = heroSkillManaCost(chosen, u.heroLv);
    if (u.mana + 0.01f < (float)manaCost) return;
    u.mana = max(0.0f, u.mana - (float)manaCost);

    Unit& e = units[targetIdx];
    uint16_t c = heroClassColor(u.heroClass);
    float cd = 2.0f;

    if (chosen == SK_HOOK) {
      e.x += (u.x - e.x) * 0.45f;
      e.y += (u.y - e.y) * 0.45f;
      e.hp -= 7.0f + u.heroLv * 2.0f;
      addBeam(u.x, u.y, e.x, e.y, TFT_ORANGE, 0.18f);
      cd = 1.8f;
    } else if (chosen == SK_FIRE) {
      heroAoE(e.x, e.y, 170.0f, 10.0f + u.heroLv * 3.0f + dmgLv, TFT_ORANGE);
      spawnParticle(e.x, e.y, TFT_ORANGE, 16);
      cd = 2.4f;
    } else if (chosen == SK_SHIELD) {
      u.hp = min(u.maxHp, u.hp + 22.0f + u.heroLv * 8.0f);
      bases[0].hp = min(bases[0].maxHp, bases[0].hp + 9.0f + u.heroLv * 2.0f);
      spawnParticle(u.x, u.y, TFT_CYAN, 14);
      cd = 3.0f;
    } else if (chosen == SK_LASER) {
      e.hp -= 20.0f + u.heroLv * 5.0f + dmgLv * 2.0f;
      addBeam(u.x, u.y, e.x, e.y, TFT_CYAN, 0.22f);
      addBeam(u.x + 1, u.y, e.x + 1, e.y, TFT_WHITE, 0.12f);
      cd = 2.2f;
    } else if (chosen == SK_NOVA) {
      heroAoE(u.x, u.y, 520.0f, 9.0f + u.heroLv * 2.6f, c);
      spawnParticle(u.x, u.y, c, 20);
      cd = 2.7f;
    } else if (chosen == SK_CHAIN) {
      heroChainGrid(e.x, e.y, 780.0f, 8.0f + u.heroLv * 2.5f, canvas.color565(180, 100, 255), u.x, u.y, 4);
      cd = 2.0f;
    } else if (chosen == SK_BLINK) {
      float dx = e.x - u.x, dy = e.y - u.y;
      float inv = 1.0f / sqrtf(dx*dx + dy*dy + 0.001f);
      u.x = constrain(e.x - dx*inv*8.0f, 4.0f, (float)Config::SCREEN_W - 4.0f);
      u.y = constrain(e.y - dy*inv*8.0f, 4.0f, (float)Config::SCREEN_H - 4.0f);
      heroAoE(u.x, u.y, 200.0f, 8.0f + u.heroLv * 2.3f, brightenColor(c,0.3f));
      spawnParticle(u.x, u.y, brightenColor(c,0.4f), 18);
      cd = 1.6f;
    } else if (chosen == SK_FROST) {
      heroFrostGrid(e.x, e.y, 420.0f, 4.0f + u.heroLv * 1.8f);
      cd = 2.5f;
    } else if (chosen == SK_POISON) {
      e.hp -= 10.0f + u.heroLv * 2.2f;
      heroAoE(e.x, e.y, 210.0f, 4.0f + u.heroLv * 1.4f, canvas.color565(80,255,90));
      cd = 2.1f;
    } else if (chosen == SK_TOTEM) {
      heroTotemGrid(u.x, u.y, 650.0f, 8.0f + u.heroLv * 1.8f);
      bases[0].hp = min(bases[0].maxHp, bases[0].hp + 16.0f + u.heroLv * 3.0f);
      spawnParticle(u.x, u.y, canvas.color565(112,242,190), 20);
      cd = 3.4f;
    } else if (chosen == SK_METEOR) {
      heroAoE(e.x, e.y, 330.0f, 15.0f + u.heroLv * 3.6f, canvas.color565(255,120,40));
      addBeam(e.x, 0, e.x, e.y, canvas.color565(255,190,100), 0.12f);
      spawnParticle(e.x, e.y, canvas.color565(255,140,50), 22);
      cd = 3.2f;
    }

    u.lastSkillUsed = chosen;
    u.skillCd[idx] = cd;
    u.skillCdMax[idx] = cd;
    // Legacy fields remain as visual hints only. Other skills keep their own cooldowns.
    u.skillCooldownMax = cd;
    u.skillCooldown = cd;
  }

  void updateFriendly(Unit& u, float dt) {
    float tx = bases[0].x;
    float ty = bases[0].y;
    float speed = u.speed;

    if (u.role == Role::SCAVENGER) {
      if (!u.carrying) {
        if (u.targetResId < 0 || !resources[u.targetResId].active) {
          float best = 999999.0f;
          u.targetResId = -1;
          for (int r = 0; r < Config::MAX_RESOURCES; r++) {
            if (!resources[r].active) continue;
            float d = distSq(u.x, u.y, resources[r].x, resources[r].y);
            if (d < best) { best = d; u.targetResId = r; }
          }
        }
        if (u.targetResId >= 0) {
          tx = resources[u.targetResId].x;
          ty = resources[u.targetResId].y;
          if (distSq(u.x, u.y, tx, ty) < Config::PICKUP_DIST_SQ) {
            u.carrying = true;
            u.idleTime = resources[u.targetResId].amount;
            resources[u.targetResId].active = false;
            u.targetResId = -1;
            playBeep(1900, 10);
          }
        } else {
          float a = (millis() * 0.001f) + (float)((uintptr_t)&u & 15);
          tx = bases[0].x + cosf(a) * 18.0f;
          ty = bases[0].y + sinf(a) * 18.0f;
        }
      } else {
        tx = bases[0].x;
        ty = bases[0].y;
        if (distSq(u.x, u.y, tx, ty) < Config::BASE_UNLOAD_DIST_SQ) {
          uint32_t val = (uint32_t)u.idleTime;
          gold += val;
          xp += (val / 2 > 0) ? (val / 2) : 1;
          u.carrying = false;
          u.idleTime = 0;
          playBeep(1500, 10);
        }
      }
    } else if (u.role == Role::HEALER) {
      int h = findHurtFriendly(u.x, u.y, u.rangeSq + 260.0f);
      if (h >= 0) {
        tx = units[h].x; ty = units[h].y;
        if (distSq(u.x, u.y, tx, ty) <= u.rangeSq && u.cooldown <= 0) {
          units[h].hp = min(units[h].maxHp, units[h].hp + 9.0f + repairLv * 2.5f + unitLv);
          addBeam(u.x, u.y, tx, ty, TFT_GREEN, 0.16f);
          spawnParticle(tx, ty, TFT_GREEN, 3);
          u.cooldown = max(0.32f, 0.90f - repairLv * 0.05f);
          speed = 0.0f;
        }
      } else {
        float a = (millis() * 0.0007f) + (float)((uintptr_t)&u & 31);
        tx = bases[0].x + cosf(a) * 15.0f;
        ty = bases[0].y + sinf(a) * 15.0f;
      }
    } else if (u.role == Role::REPAIR) {
      Unit* repairTarget = nullptr;
      float best = 999999.0f;
      for (int i = 0; i < Config::MAX_UNITS; i++) {
        if (!units[i].active || units[i].team != 0 || units[i].role != Role::TOWER) continue;
        if (units[i].hp >= units[i].maxHp * 0.98f) continue;
        float d = distSq(u.x, u.y, units[i].x, units[i].y);
        if (d < best) { best = d; repairTarget = &units[i]; }
      }
      if (bases[0].hp < bases[0].maxHp * 0.96f && distSq(u.x,u.y,bases[0].x,bases[0].y) < best) repairTarget = nullptr;
      if (repairTarget) { tx = repairTarget->x; ty = repairTarget->y; }
      if (!repairTarget) { tx = bases[0].x; ty = bases[0].y; }
      if (distSq(u.x, u.y, tx, ty) < 36.0f && u.cooldown <= 0) {
        if (repairTarget) repairTarget->hp = min(repairTarget->maxHp, repairTarget->hp + 8.0f + repairLv * 2.0f);
        else bases[0].hp = min(bases[0].maxHp, bases[0].hp + 7.0f + repairLv * 2.0f);
        spawnParticle(tx, ty, TFT_GREEN, 2);
        u.cooldown = 0.45f;
      }
    } else {
      int e = findNearestEnemy(u.x, u.y, u.rangeSq + 900.0f);
      if (e >= 0) {
        tx = units[e].x; ty = units[e].y;
        float d = distSq(u.x, u.y, tx, ty);
        if (d <= u.rangeSq && u.cooldown <= 0) {
          units[e].hp -= u.dmg * waveDamageMul;
          uint16_t c = (u.role == Role::MAGE) ? TFT_CYAN : (u.role == Role::HERO ? heroClassColor(u.heroClass) : playerColor);
          addBeam(u.x, u.y, tx, ty, brightenColor(c, 0.25f), 0.15f);
          spawnParticle(tx, ty, TFT_WHITE, 2);
          if (u.role == Role::HERO) castHeroSkill(u, e);
          float baseCd = (u.role == Role::TOWER) ? max(0.18f, 0.70f - towerLv * 0.05f) : (u.role == Role::HERO ? max(0.20f, 0.78f - u.heroLv * 0.035f - fireLv * 0.025f) : max(0.22f, 0.95f - fireLv * 0.04f));
          u.cooldown = baseCd / waveFireMul;
          if (e >= 0 && e < Config::MAX_UNITS && units[e].active && units[e].hp <= 0) killEnemy(e);
          if (u.role != Role::BRUTE) speed = 0.0f;
        }
      } else {
        if (u.role == Role::HERO && groundStyle == 1) {
          tx = raidFocusX;
          ty = raidFocusY;
        } else if (u.role == Role::HERO && groundStyle == 2) {
          float a = (millis() * 0.0015f) + (float)(groundSeed & 31);
          tx = bases[0].x + cosf(a) * 10.0f;
          ty = bases[0].y + sinf(a) * 10.0f;
        } else {
          float a = (millis() * 0.0008f) + (float)((uintptr_t)&u & 31);
          tx = bases[0].x + cosf(a) * (u.role == Role::TOWER ? 0.0f : 18.0f);
          ty = bases[0].y + sinf(a) * (u.role == Role::TOWER ? 0.0f : 18.0f);
        }
      }
    }

    if (u.role == Role::TOWER) return;
    float dx = tx - u.x;
    float dy = ty - u.y;
    float ls = dx*dx + dy*dy;
    if (ls > 0.01f) {
      float inv = 1.0f / (sqrtf(ls) + 0.0001f);
      dx *= inv; dy *= inv;
    }
    u.vx += (dx * speed - u.vx) * 4.2f * dt;
    u.vy += (dy * speed - u.vy) * 4.2f * dt;
  }

  void updateEnemy(Unit& u, float dt) {
    int ft = findNearestFriendlyTarget(u.x, u.y);
    float tx = (ft >= 0) ? units[ft].x : bases[0].x;
    float ty = (ft >= 0) ? units[ft].y : bases[0].y;
    float d = distSq(u.x, u.y, tx, ty);
    if (d <= u.rangeSq && u.cooldown <= 0) {
      uint16_t c = (u.role == Role::MAGE) ? canvas.color565(190, 80, 255) : enemyColor;
      addBeam(u.x, u.y, tx, ty, c, 0.15f);
      spawnParticle(tx, ty, c, 2);
      if (ft >= 0) {
        units[ft].hp -= u.dmg * waveShieldMul;
        if (units[ft].hp <= 0) {
          spawnParticle(units[ft].x, units[ft].y, playerColor, 8);
          units[ft].active = false;
        }
      } else {
        damageThrone(u.dmg * waveShieldMul);
      }
      u.cooldown = (u.role == Role::BRUTE) ? 1.25f : ((u.role == Role::MAGE) ? 1.1f : 0.82f);
    }
    if (d > u.rangeSq * 0.72f) {
      float dx = tx - u.x;
      float dy = ty - u.y;
      float ls = dx*dx + dy*dy;
      if (ls > 0.01f) { float inv = 1.0f / (sqrtf(ls) + 0.0001f); dx *= inv; dy *= inv; }
      float wm = enemyWeatherSpeedMul();
      u.vx += (dx * u.speed * wm - u.vx) * 4.6f * dt;
      u.vy += (dy * u.speed * wm - u.vy) * 4.6f * dt;
    } else {
      u.vx *= 0.86f;
      u.vy *= 0.86f;
    }
  }

  void buildSpatialGrid() {
    memset(gridHead, -1, sizeof(gridHead));
    for (int i = 0; i < Config::MAX_UNITS; i++) {
      if (!units[i].active) continue;
      int cx = constrain((int)units[i].x / 16, 0, 7);
      int cy = constrain((int)units[i].y / 16, 0, 7);
      int cell = cx + cy * 8;
      gridNext[i] = gridHead[cell];
      gridHead[cell] = i;
    }
    spatialReady = true;
  }

  void updateUnits(float dt) {
    buildSpatialGrid();
    for (int i = 0; i < Config::MAX_UNITS; i++) {
      if (!units[i].active) continue;
      Unit& u = units[i];
      if (u.cooldown > 0) u.cooldown -= dt;
      tickHeroManaAndSkillCooldowns(u, dt);
      if (u.team == 0) updateFriendly(u, dt);
      else updateEnemy(u, dt);

      int cx = constrain((int)u.x / 16, 0, 7);
      int cy = constrain((int)u.y / 16, 0, 7);
      for (int ny = max(0, cy - 1); ny <= min(7, cy + 1); ny++) {
        for (int nx = max(0, cx - 1); nx <= min(7, cx + 1); nx++) {
          int cell = nx + ny * 8;
          int j = gridHead[cell];
          while (j != -1) {
            if (i != j && units[j].active) {
              float sepDist = distSq(u.x, u.y, units[j].x, units[j].y);
              if (sepDist < Config::SEPARATION_DIST_SQ && sepDist > 0.1f) {
                u.vx += (u.x - units[j].x) * 1.7f * dt;
                u.vy += (u.y - units[j].y) * 1.7f * dt;
              }
            }
            j = gridNext[j];
          }
        }
      }
      u.x = constrain(u.x + u.vx * dt, 2.0f, (float)Config::SCREEN_W - 2.0f);
      u.y = constrain(u.y + u.vy * dt, 2.0f, (float)Config::SCREEN_H - 2.0f);
    }
  }

  void updateTelemetry() {
    static uint32_t lastQmpMs = 0;
    uint32_t now = millis();
    if (now - lastQmpMs > 150) {
      lastQmpMs = now;
      qmp.update();
      swarmTempC = qmp.cTemp;
      swarmPressureHpa = qmp.pressure / 100.0f;
    }
    static float predPres = 1013.0f;
    static float predTemp = 24.0f;
    float errP = fabsf(predPres - swarmPressureHpa) * 0.02f;
    float errT = fabsf(predTemp - swarmTempC) * 0.05f;
    predPres = predPres * 0.985f + swarmPressureHpa * 0.015f;
    predTemp = predTemp * 0.985f + swarmTempC * 0.015f;
    swarmGameSurprise = (float)aliveEnemies() * 0.04f + shockwaveRadius * 0.01f + (float)wave * 0.03f;
    swarmPredictionError = errP + errT + swarmGameSurprise * 0.12f;
    swarmLocalEntropy = constrain(swarmMicRms * 0.0008f + fabsf(swarmPressureHpa - 1013.25f) * 0.006f + fabsf(swarmTempC - 24.0f) * 0.035f + swarmGameSurprise + swarmPredictionError, 0.01f, 9999.0f);
  }

  void updateWeather(float dt) {
    uint32_t now = millis();
    dayPhase += dt / 96.0f; // full day/night loop around 96 seconds.
    if (dayPhase >= 1.0f) dayPhase -= 1.0f;

    if (lastWeatherShiftMs == 0 || now - lastWeatherShiftMs > 28000UL) {
      lastWeatherShiftMs = now;
      uint32_t r = esp_random() % 100;
      // Sensor bias: low pressure nudges toward storm/fog, high temp nudges toward heat.
      if (swarmPressureHpa > 0.1f && swarmPressureHpa < 1007.0f && r < 55) weatherMode = 2;
      else if (swarmTempC > 29.5f && r < 50) weatherMode = 4;
      else if (r < 36) weatherMode = 0;
      else if (r < 58) weatherMode = 1;
      else if (r < 72) weatherMode = 2;
      else if (r < 88) weatherMode = 3;
      else weatherMode = 4;
      weatherStrength = 0.35f + (float)(esp_random() % 65) * 0.01f;
    }
  }

  void drawWeatherLayer(uint32_t now) {
    if (weatherMode == 1 || weatherMode == 2) {
      int drops = (weatherMode == 2) ? 22 : 14;
      uint16_t rc = (weatherMode == 2) ? canvas.color565(70, 110, 150) : canvas.color565(48, 82, 122);
      for (int i = 0; i < drops; i++) {
        int x = (i * 17 + now / 8) & 127;
        int y = (i * 31 + now / 5) & 127;
        canvas.drawLine(x, y, x - 2, y + 4, dimColor(rc, 0.65f));
      }
      if (weatherMode == 2 && ((now / 420) & 15) == 0) {
        canvas.drawFastHLine(0, 18 + ((now / 97) & 31), 128, dimColor(TFT_CYAN, 0.35f));
      }
    } else if (weatherMode == 3) {
      for (int i = 0; i < 7; i++) {
        int x = (i * 25 + now / 23) & 127;
        int y = 22 + ((i * 13 + now / 31) & 63);
        canvas.drawFastHLine(x, y, 18, canvas.color565(18, 22, 28));
      }
    } else if (weatherMode == 4) {
      for (int i = 0; i < 10; i++) {
        int x = (i * 19 + now / 19) & 127;
        int y = 18 + ((i * 29 + now / 37) & 80);
        canvas.drawPixel(x, y, canvas.color565(58, 34, 18));
        canvas.drawPixel(x + 1, y, canvas.color565(46, 28, 14));
      }
    }
  }

public:
  void drawAgentStateMini(uint32_t now) {
    (void)now;
    float pEntropy, pMood, pFatigue, pVigor;
    uint8_t pMoodCode;
    janusPersonalSnapshot(pEntropy, pMood, pFatigue, pVigor, pMoodCode);
    uint16_t c = TFT_SILVER;
    if (pMoodCode == 0) c = canvas.color565(140, 140, 160);
    else if (pMoodCode == 4) c = canvas.color565(255, 110, 80);
    else if (pMoodCode == 5) c = canvas.color565(255, 220, 90);
    else if (pMoodCode == 3) c = canvas.color565(110, 220, 255);
    else if (pMoodCode == 2) c = canvas.color565(120, 230, 150);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(c, TFT_TRANSPARENT);
    char b[28];
    snprintf(b, sizeof(b), "%s F%02d V%02d", janusMoodName(pMoodCode), (int)(pFatigue * 100.0f), (int)(pVigor * 100.0f));
    canvas.drawString(b, 2, 20);
  }

  void init() {
    loadUiState();
    M5.Display.setBrightness(brightLevels[brightIdx]);
    loadRecords();
    for (int i=0; i<15; i++) {
      bgDrops[i].x = esp_random() % Config::SCREEN_W;
      bgDrops[i].y = esp_random() % Config::SCREEN_H;
      bgDrops[i].radius = (esp_random() % 4) + 2;
      bgDrops[i].speed = ((esp_random() % 10) + 5) * 0.35f;
      bgDrops[i].phase = (esp_random() % 100) / 10.0f;
    }
    lastWeatherShiftMs = 0;
    weatherMode = 0;
    weatherStrength = 0.35f;
    startNewRun();
  }

  void drawMiningLuckToast(uint32_t now) {
    if (!miningToastUntilMs || now > miningToastUntilMs) return;
    uint16_t c = canvas.color565(236, 178, 44);
    canvas.drawRoundRect(5, 53, 118, 12, 3, dimColor(c, 0.70f));
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(c, TFT_TRANSPARENT);
    canvas.drawString(miningToastLine, 8, 55);
  }

  void applyMinerRewardEvents() {
    static uint32_t seenSeq = 0;
    uint32_t seq;
    uint8_t kind;
    uint16_t bits;
    portENTER_CRITICAL(&minerGameRewardMux);
    seq = minerGameRewardSeq;
    kind = minerGameRewardKind;
    bits = minerGameRewardBits;
    portEXIT_CRITICAL(&minerGameRewardMux);

    if (seq == seenSeq || seq == 0) return;
    seenSeq = seq;

    uint16_t col = TFT_YELLOW;
    uint32_t bonusGold = 0;
    uint16_t bonusXp = 0;
    uint8_t luckAdd = 1;
    float luckPowerAdd = 0.08f + (float)min(64, (int)bits) * 0.004f;

    if (kind == 5) {        // Buzz-confirmed pool ACCEPT from a remote worker: real buff
      bonusGold = 130 + bits * 5;
      bonusXp = 72 + bits * 2;
      col = canvas.color565(255, 224, 92);
      luckAdd = 4 + min(3, (int)colonyAgentLevel);
      luckPowerAdd += 0.72f + 0.08f * (float)colonyAgentLevel;
      snprintf(heroLine, sizeof(heroLine), "BUZZ POOL ACCEPT B%u", bits);
      snprintf(miningToastLine, sizeof(miningToastLine), "REAL ACCEPT +%luG", (unsigned long)bonusGold);
      if (repairLv < 12) repairLv++;
      if (towerLv < 10) towerLv++;
      if (dmgLv < 14) dmgLv++;
    } else if (kind == 3) {        // direct/local pool accepted: rare, big positive event
      bonusGold = 110 + bits * 4;
      bonusXp = 60 + bits * 2;
      col = TFT_GREEN;
      luckAdd = 4;
      luckPowerAdd += 0.55f;
      snprintf(heroLine, sizeof(heroLine), "POOL ACCEPT B%u", bits);
      snprintf(miningToastLine, sizeof(miningToastLine), "POOL LUCK +%luG", (unsigned long)bonusGold);
      if (towerLv < 10) towerLv++;
      if (dmgLv < 14) dmgLv++;
    } else if (kind == 2) { // local pool candidate submitted: praise only until pool ACCEPT
      bonusGold = 0;
      bonusXp = 0;
      col = TFT_YELLOW;
      luckAdd = 0;
      luckPowerAdd = 0.0f;
      snprintf(heroLine, sizeof(heroLine), "POOL CAND B%u", bits);
      snprintf(miningToastLine, sizeof(miningToastLine), "POOL CAND WAIT");
    } else if (kind == 1) { // Buzz job share sent: real ticket, but no buff until Buzz reports pool ACCEPT
      bonusGold = 0;
      bonusXp = 0;
      col = TFT_ORANGE;
      luckAdd = 0;
      luckPowerAdd = 0.0f;
      snprintf(heroLine, sizeof(heroLine), "BUZZ TICKET B%u", bits);
      snprintf(miningToastLine, sizeof(miningToastLine), "BUZZ TICKET SENT");
    } else if (kind == 8) { // Buzz Agent praise only: visible morale, strictly no gameplay buff
      bonusGold = 0;
      bonusXp = 0;
      col = canvas.color565(110, 220, 255);
      luckAdd = 0;
      luckPowerAdd = 0.0f;
      snprintf(heroLine, sizeof(heroLine), "BUZZ PRAISE L%u", (unsigned)colonyAgentLevel);
      snprintf(miningToastLine, sizeof(miningToastLine), "PRAISE ONLY B%u", bits);
    } else {               // local fallback omen: praise only, not a fake share
      bonusGold = 0;
      bonusXp = 0;
      col = canvas.color565(180, 140, 40);
      luckAdd = 0;
      luckPowerAdd = 0.0f;
      snprintf(heroLine, sizeof(heroLine), "HASH OMEN B%u", bits);
      snprintf(miningToastLine, sizeof(miningToastLine), "HASH PRAISE ONLY");
    }

    bool realBuffEvent = (bonusGold > 0 || bonusXp > 0 || luckAdd > 0 || luckPowerAdd > 0.0f);
    if (realBuffEvent) {
      gold += bonusGold;
      xp += bonusXp;
      bases[0].hp = min(bases[0].maxHp, bases[0].hp + (float)(7 + bits / 2));
      bases[0].resources += (int)(bonusGold / 3);
      miningLuckCharges = (uint8_t)min(9, (int)miningLuckCharges + (int)luckAdd);
      miningLuckPower = constrain(miningLuckPower + luckPowerAdd, 0.0f, 2.60f);

      // Immediate current-wave morale, plus charges for future waves. This turns real accepted
      // mining success into visible good gameplay instead of only numbers on the miner HUD.
      waveLootMul = min(1.90f, waveLootMul + 0.035f * (float)luckAdd);
      waveDamageMul = min(1.60f, waveDamageMul + 0.018f * (float)luckAdd);
      grantHeroXp((uint16_t)(bonusXp / 2 + luckAdd * 3));
      if ((kind == 3 || kind == 5) && aliveFriendlies() < 8 + unitLv) spawnUnit(0, randomFriendlyRole(), bases[0].x, bases[0].y);
      shockwaveRadius = max(shockwaveRadius, 8.0f + (float)min(28, (int)bits));
      spawnParticle(bases[0].x, bases[0].y, col, (kind == 3 || kind == 5) ? 30 : 14);
    }

    miningToastUntilMs = millis() + ((kind == 3 || kind == 5) ? 6200UL : 2600UL);
    addBeam(bases[0].x, bases[0].y, bases[0].x + 1, bases[0].y - 1, col, realBuffEvent ? 0.25f : 0.10f);
    playBeep(realBuffEvent ? 2100 : 1200, realBuffEvent ? 55 : 18);
  }

  void fixedGameStep(float dt, uint32_t now) {
    updateEffects(dt);
    updateWeather(dt);
    applyMinerRewardEvents();
    janusPersonalMetabolism(now, minerRealHashrate + minerRemoteHashrate + minerLocalHashrate, state == GameState::PLAYING, swarmTempC, swarmGameSurprise);

    if (state == GameState::GAME_OVER) {
      gameOverTimer -= dt;
      if (gameOverTimer <= 0.0f) startNewRun();
      return;
    }

    survivalMs = now - runStartMs;
    for (int i=0; i<15; i++) {
      bgDrops[i].y -= bgDrops[i].speed * dt;
      if (bgDrops[i].y < -8) { bgDrops[i].y = Config::SCREEN_H + 8; bgDrops[i].x = esp_random() % Config::SCREEN_W; }
    }

    if (xp >= level * 70UL) { xp -= level * 70UL; level++; gold += 20 + level * 4; }
    updateWave(dt);
    janusAutoUpgrade();

    // Grid is now used for targeting before throne/tower/hero logic.
    buildSpatialGrid();
    updateThrone(dt);
    updateUnits(dt);

    if (bases[0].hp <= 0.0f) finishRun();
  }

  void update() {
    uint32_t now = millis();
    float frameDt = (now - lastTick) / 1000.0f;
    lastTick = now;
    if (frameDt < 0.001f) frameDt = 0.001f;
    if (frameDt > Config::MAX_FRAME_DT) frameDt = Config::MAX_FRAME_DT;
    avgFps = avgFps * 0.90f + (1.0f / frameDt) * 0.10f;

    handleInput();
    applyGroundOrders();
    updateRaidFocus();
    updateEchoBaseMicRms();
    updateTelemetry();

    fixedAccumulator += frameDt;
    uint8_t steps = 0;
    while (fixedAccumulator >= Config::FIXED_DT && steps < Config::MAX_CATCHUP_STEPS) {
      fixedGameStep(Config::FIXED_DT, now);
      fixedAccumulator -= Config::FIXED_DT;
      steps++;
    }
    if (steps == Config::MAX_CATCHUP_STEPS) fixedAccumulator = 0.0f;
  }

  void drawThrone() {
    int cx = (int)bases[0].x;
    int cy = (int)bases[0].y;
    uint16_t hot = brightenColor(playerColor, 0.35f);
    canvas.fillCircle(cx, cy, 8, dimColor(playerColor, 0.55f));
    canvas.drawCircle(cx, cy, 10, dimColor(playerColor, 0.65f));
    canvas.drawRect(cx - 6, cy - 6, 13, 13, hot);
    canvas.drawLine(cx - 10, cy, cx + 10, cy, dimColor(hot, 0.65f));
    canvas.drawLine(cx, cy - 10, cx, cy + 10, dimColor(hot, 0.65f));
    drawSpark(cx, cy, TFT_WHITE);
    int hpw = (int)((bases[0].hp / bases[0].maxHp) * 28.0f);
    canvas.fillRect(cx - 14, cy - 17, 28, 3, dimColor(playerColor, 0.18f));
    canvas.fillRect(cx - 14, cy - 17, hpw, 3, hot);
  }

  void drawUnit(const Unit& u) {
    int x = (int)u.x;
    int y = (int)u.y;
    uint16_t col = (u.team == 0) ? playerColor : enemyColor;
    if (u.team == 1 && u.role == Role::MAGE) col = canvas.color565(180, 80, 255);
    if (u.team == 1 && u.role == Role::BRUTE) col = canvas.color565(255, 110, 50);
    uint16_t hot = brightenColor(col, 0.25f);

    if (u.team == 1) {
      // v8.15B: enemies are never drawn with throne-unit silhouettes.
      // Dark outline + hostile glyph shapes keep them readable in the swarm.
      uint16_t shadow = canvas.color565(18, 0, 4);
      canvas.drawCircle(x, y, 6, shadow);
      canvas.drawFastHLine(x - 6, y, 13, shadow);
      canvas.drawFastVLine(x, y - 6, 13, shadow);

      if (u.role == Role::BRUTE) {
        canvas.fillCircle(x, y, 5, canvas.color565(255, 88, 30));
        canvas.drawCircle(x, y, 6, TFT_BLACK);
        canvas.drawFastHLine(x - 4, y, 9, TFT_WHITE);
      } else if (u.role == Role::MAGE) {
        uint16_t m = canvas.color565(190, 70, 255);
        canvas.fillTriangle(x, y - 6, x - 5, y, x, y + 6, m);
        canvas.fillTriangle(x, y - 6, x + 5, y, x, y + 6, dimColor(m, 0.75f));
        canvas.drawCircle(x, y, 3, TFT_WHITE);
      } else if (u.role == Role::ARCHER) {
        canvas.drawTriangle(x, y - 5, x - 5, y + 4, x + 5, y + 4, canvas.color565(255, 54, 84));
        canvas.drawFastVLine(x, y - 5, 10, TFT_WHITE);
      } else {
        canvas.fillRect(x - 4, y - 4, 9, 9, enemyColor);
        canvas.drawLine(x - 5, y - 5, x + 5, y + 5, TFT_BLACK);
        canvas.drawLine(x + 5, y - 5, x - 5, y + 5, TFT_BLACK);
        canvas.drawPixel(x, y, TFT_WHITE);
      }

      if (u.hp < u.maxHp) {
        int hpw = constrain((int)((u.hp / max(1.0f, u.maxHp)) * 9.0f), 0, 9);
        canvas.fillRect(x - 4, y - 8, 9, 2, canvas.color565(55, 0, 0));
        canvas.fillRect(x - 4, y - 8, hpw, 2, canvas.color565(255, 40, 40));
      }
      return;
    }

    if (u.role == Role::SCAVENGER) {
      canvas.fillCircle(x, y, 2, col);
      canvas.drawPixel(x, y, TFT_WHITE);
      if (u.carrying) drawSpark(x, y - 3, TFT_YELLOW);
    } else if (u.role == Role::REPAIR) {
      canvas.drawRect(x - 2, y - 2, 5, 5, col);
      canvas.drawFastHLine(x - 2, y, 5, TFT_GREEN);
      canvas.drawFastVLine(x, y - 2, 5, TFT_GREEN);
    } else if (u.role == Role::HEALER) {
      canvas.drawCircle(x, y, 3, TFT_GREEN);
      canvas.drawFastHLine(x - 2, y, 5, TFT_WHITE);
      canvas.drawFastVLine(x, y - 2, 5, TFT_WHITE);
    } else if (u.role == Role::HERO) {
      col = heroClassColor(u.heroClass);
      hot = brightenColor(col, 0.35f);
      uint8_t v = u.visualSeed & 3;
      if (v == 0) canvas.fillTriangle(x, y - 5, x - 4, y + 4, x + 4, y + 4, col);
      else if (v == 1) { canvas.fillCircle(x, y, 4, dimColor(col, 0.90f)); canvas.drawFastHLine(x - 5, y, 11, hot); }
      else if (v == 2) { canvas.drawRect(x - 4, y - 4, 9, 9, col); canvas.drawLine(x - 5, y + 4, x + 5, y - 4, hot); }
      else { canvas.fillRect(x - 3, y - 4, 7, 9, dimColor(col, 0.80f)); canvas.drawCircle(x, y, 5, hot); }
      canvas.drawPixel(x, y, TFT_WHITE);
      if (heroHasReadySkill(u)) drawSpark(x, y - 6, hot);
      if (u.heroLv > 1) canvas.drawFastHLine(x - 3, y + 6, min(7, (int)u.heroLv), hot);
    } else if (u.role == Role::TOWER) {
      canvas.fillRect(x - 3, y - 3, 7, 7, dimColor(col, 0.60f));
      canvas.drawRect(x - 4, y - 4, 9, 9, hot);
      canvas.drawPixel(x, y, TFT_WHITE);
    } else if (u.role == Role::BRUTE) {
      canvas.fillCircle(x, y, 5, dimColor(col, 0.88f));
      canvas.drawCircle(x, y, 5, hot);
      canvas.drawPixel(x, y, TFT_WHITE);
    } else if (u.role == Role::ARCHER) {
      canvas.drawRect(x - 2, y - 2, 5, 5, col);
      canvas.drawPixel(x, y, TFT_WHITE);
    } else if (u.role == Role::MAGE) {
      canvas.drawCircle(x, y, 3, col);
      canvas.fillCircle(x, y, 1, TFT_WHITE);
    } else {
      canvas.fillTriangle(x, y - 4, x - 3, y + 3, x + 3, y + 3, col);
      canvas.drawPixel(x, y, TFT_WHITE);
    }
  }

  void drawSkillIcon(int x, int y, uint16_t skillBit, uint16_t col, bool cooling, float cdFrac) {
    canvas.drawRect(x, y, 8, 8, dimColor(col, cooling ? 0.45f : 0.85f));
    uint16_t ic = cooling ? dimColor(col, 0.45f) : brightenColor(col, 0.30f);
    if (skillBit == SK_HOOK) { canvas.drawCircle(x + 3, y + 3, 2, ic); canvas.drawLine(x + 5, y + 5, x + 7, y + 7, ic); }
    else if (skillBit == SK_FIRE) { canvas.drawTriangle(x + 4, y + 1, x + 1, y + 6, x + 6, y + 6, ic); }
    else if (skillBit == SK_SHIELD) { canvas.drawRect(x + 2, y + 1, 4, 5, ic); }
    else if (skillBit == SK_LASER) { canvas.drawFastHLine(x + 1, y + 4, 6, ic); canvas.drawPixel(x + 6, y + 3, ic); }
    else if (skillBit == SK_NOVA) { canvas.drawCircle(x + 4, y + 4, 2, ic); canvas.drawPixel(x + 4, y + 1, ic); canvas.drawPixel(x + 4, y + 7, ic); }
    else if (skillBit == SK_CHAIN) { canvas.drawLine(x + 1, y + 6, x + 3, y + 3, ic); canvas.drawLine(x + 3, y + 3, x + 5, y + 5, ic); canvas.drawLine(x + 5, y + 5, x + 7, y + 2, ic); }
    else if (skillBit == SK_BLINK) { canvas.drawFastVLine(x + 4, y + 1, 5, ic); canvas.drawPixel(x + 2, y + 2, ic); canvas.drawPixel(x + 6, y + 5, ic); }
    else if (skillBit == SK_FROST) { canvas.drawPixel(x + 4, y + 1, ic); canvas.drawFastVLine(x + 4, y + 1, 6, ic); canvas.drawFastHLine(x + 1, y + 4, 6, ic); }
    else if (skillBit == SK_POISON) { canvas.drawCircle(x + 4, y + 4, 2, ic); canvas.drawPixel(x + 3, y + 3, ic); canvas.drawPixel(x + 5, y + 5, ic); }
    else if (skillBit == SK_TOTEM) { canvas.drawFastVLine(x + 4, y + 1, 6, ic); canvas.drawPixel(x + 2, y + 2, ic); canvas.drawPixel(x + 6, y + 2, ic); }
    else if (skillBit == SK_METEOR) { canvas.drawLine(x + 1, y + 6, x + 5, y + 2, ic); canvas.drawPixel(x + 6, y + 1, ic); }
    if (cooling) {
      canvas.drawCircle(x + 6, y + 1, 1, TFT_SILVER);
      canvas.drawPixel(x + 6, y + 1, TFT_SILVER);
      if (cdFrac > 0.66f) canvas.drawLine(x + 6, y + 1, x + 6, y - 1, TFT_SILVER);
      else if (cdFrac > 0.33f) canvas.drawLine(x + 6, y + 1, x + 7, y, TFT_SILVER);
      else canvas.drawLine(x + 6, y + 1, x + 5, y + 2, TFT_SILVER);
    }
  }

  void drawHeroHud() {
    // v8.15B: compact transparent hero HUD.
    // No opaque lower block: only thin outlines, tiny bars and transparent text.
    const int x = 2;
    const int w = 60;      // v8.15B: same width as right miner HUD
    const int hPanel = 21; // v8.15B: same height as right miner HUD
    const int goldY = Config::SCREEN_H - 34;
    const int y = Config::SCREEN_H - 22;
    uint16_t goldLine = canvas.color565(224, 168, 34);

    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(brightenColor(goldLine, 0.25f), TFT_TRANSPARENT);
    char gb[22];
    snprintf(gb, sizeof(gb), "G:%s", compactK(gold));
    canvas.drawString(gb, x + 1, goldY);

    int hi = focusHeroIndex();
    if (hi < 0) {
      canvas.drawRoundRect(x, y, w, hPanel, 3, dimColor(goldLine, 0.58f));
      canvas.setTextColor(TFT_SILVER, TFT_TRANSPARENT);
      canvas.drawString("NO MECH", x + 3, y + 3);
      char cb[24];
      snprintf(cb, sizeof(cb), "DROP %sG", compactK(heroBuyCost()));
      canvas.setTextColor(gold >= heroBuyCost() ? goldLine : TFT_DARKGREY, TFT_TRANSPARENT);
      canvas.drawString(cb, x + 3, y + 12);
      canvas.setTextDatum(TL_DATUM);
      return;
    }

    const Unit& h = units[hi];
    uint16_t hc = heroClassColor(h.heroClass);
    canvas.drawRoundRect(x, y, w, hPanel, 3, dimColor(hc, 0.62f));

    int ix = x + 3;
    int drawn = 0;
    for (uint16_t bit = 1; bit && drawn < 6 && (ix + 8) <= (x + w - 2); bit = nextSkillBit(bit)) {
      if (!(h.skillMask & bit)) continue;
      uint8_t si = skillIndex(bit);
      bool cooling = (si < 11) && h.skillCd[si] > 0.05f;
      bool enoughMana = h.mana + 0.01f >= (float)heroSkillManaCost(bit, h.heroLv);
      float frac = (si < 11 && h.skillCdMax[si] > 0.01f) ? (h.skillCd[si] / h.skillCdMax[si]) : 0.0f;
      drawSkillIcon(ix, y + 1, bit, enoughMana ? hc : dimColor(hc, 0.42f), cooling, frac);
      ix += 9;  // v8.15B: 6 icons fit inside 60 px without touching miner HUD
      drawn++;
    }

    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(hc, TFT_TRANSPARENT);
    canvas.drawString(heroClassShort(h.heroClass), x + 3, y + 10);

    char hb[18];
    snprintf(hb, sizeof(hb), "L%u", h.heroLv);
    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_TRANSPARENT);
    canvas.drawString(hb, x + w - 4, y + 10);

    int hpw = constrain((int)((h.hp / max(1.0f, h.maxHp)) * (float)(w - 8)), 0, w - 8);
    int mpw = constrain((int)((h.mana / max(1.0f, h.maxMana)) * (float)(w - 8)), 0, w - 8);

    // Tiny health/mana strips under the hero line.
    canvas.drawFastHLine(x + 4, y + 17, w - 8, canvas.color565(50, 0, 0));
    canvas.drawFastHLine(x + 4, y + 17, hpw, TFT_RED);
    canvas.drawFastHLine(x + 4, y + 19, w - 8, canvas.color565(0, 9, 40));
    canvas.drawFastHLine(x + 4, y + 19, mpw, canvas.color565(40, 120, 255));

    canvas.setTextDatum(TL_DATUM);
  }

  void drawRecords() {
    // Event-only record toast: shown only for a few seconds after the table changes.
    canvas.fillRect(2, 22, 68, 42, canvas.color565(0,0,0));
    canvas.drawRect(2, 22, 68, 42, dimColor(playerColor, 0.45f));
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextSize(1);
    canvas.setTextColor(TFT_SILVER, TFT_TRANSPARENT);
    canvas.drawString("RECORD", 5, 24);
    for (int i = 0; i < 4; i++) {
      if (!bestMs[i]) continue;
      canvas.setTextColor(bestColor[i] ? bestColor[i] : TFT_WHITE, TFT_TRANSPARENT);
      char b[34];
      snprintf(b, sizeof(b), "#%u %s W%u", bestModel[i], fmtTime(bestMs[i]), bestWave[i]);
      canvas.drawString(b, 5, 32 + i * 7);
    }
  }

  void draw() {
    uint32_t now = millis();
    float dl = dayLight();
    uint8_t bgR = (uint8_t)(3 + dl * 5);
    uint8_t bgG = (uint8_t)(5 + dl * 8);
    uint8_t bgB = (uint8_t)(10 + dl * 16);
    if (weatherMode == 2) { bgR = 3; bgG = 5; bgB = 13; }
    if (weatherMode == 3) { bgR += 4; bgG += 4; bgB += 4; }
    if (weatherMode == 4) { bgR += 8; bgG += 3; }
    canvas.fillRect(0, 0, Config::SCREEN_W, Config::SCREEN_H, canvas.color565(bgR, bgG, bgB));
    drawGroundTiles(now, dl);

    for (int i=0; i<15; i++) {
      uint16_t haze = canvas.color565(6 + (uint8_t)(dl * 6), 10 + (i & 3) * 2 + (uint8_t)(dl * 6), 18 + (i & 1) * 5 + (uint8_t)(dl * 8));
      canvas.fillCircle((int)bgDrops[i].x, (int)bgDrops[i].y, (int)bgDrops[i].radius + 2, dimColor(haze, weatherMode == 3 ? 0.95f : 0.75f));
    }
    for (int i = 0; i < 18; i++) canvas.drawPixel((i * 29 + now / 13) & 127, (i * 47 + now / 21) & 127, canvas.color565(18 + (uint8_t)(dl * 12), 22 + (uint8_t)(dl * 10), 30 + (uint8_t)(dl * 12)));
    drawWeatherLayer(now);
    // v8.15B: TD maze layer removed; swarm comes from all sides again.

    if (shockwaveRadius > 0.0f) {
      canvas.drawCircle((int)bases[0].x, (int)bases[0].y, (int)shockwaveRadius, TFT_CYAN);
      canvas.drawCircle((int)bases[0].x, (int)bases[0].y, (int)shockwaveRadius + 1, dimColor(TFT_CYAN, 0.35f));
    }

    for (auto& r : resources) {
      if (!r.active) continue;
      canvas.fillRect((int)r.x - 1, (int)r.y - 1, 3, 3, TFT_YELLOW);
      canvas.drawPixel((int)r.x, (int)r.y - 2, TFT_WHITE);
    }

    drawThrone();

    for (auto& s : spits) {
      if (!s.active) continue;
      float t = 1.0f - (s.life / s.maxLife);
      float cx = s.startX + (s.endX - s.startX) * t;
      float cy = s.startY + (s.endY - s.startY) * t;
      canvas.drawLine((int)s.startX, (int)s.startY, (int)cx, (int)cy, dimColor(s.color, 0.40f));
      canvas.drawLine((int)(s.startX + (s.endX - s.startX) * max(0.0f, t - 0.16f)), (int)(s.startY + (s.endY - s.startY) * max(0.0f, t - 0.16f)), (int)cx, (int)cy, brightenColor(s.color, 0.42f));
      drawSpark((int)cx, (int)cy, brightenColor(s.color, 0.45f));
    }

    for (int i=0; i<Config::MAX_UNITS; i++) if (units[i].active) drawUnit(units[i]);

    for (auto& p : particles) {
      if (!p.active) continue;
      if (p.life > 0.12f) drawSpark((int)p.x, (int)p.y, p.color);
      else canvas.drawPixel((int)p.x, (int)p.y, dimColor(p.color, p.life * 6.0f));
    }

    // HUD: FPS + weather left, throne survival timer top-center, wave/level + sensors top-right, gold bottom-right.
    canvas.setTextSize(1);
    canvas.setTextDatum(TL_DATUM);
    canvas.setTextColor(TFT_DARKGREY, TFT_TRANSPARENT);
    char fpsBuf[14]; snprintf(fpsBuf, sizeof(fpsBuf), "FPS:%02d", (int)avgFps);
    canvas.drawString(fpsBuf, 2, 2);
    char wBuf[16]; snprintf(wBuf, sizeof(wBuf), "%s", weatherName());
    canvas.drawString(wBuf, 2, 11);
    drawAgentStateMini(now);

    canvas.setTextDatum(TC_DATUM);
    canvas.setTextColor(TFT_SILVER, TFT_TRANSPARENT);
    // This is the total time the throne survived in the current run; finishRun writes this exact value into records.
    canvas.drawString(fmtTime(survivalMs), Config::SCREEN_W / 2, 2);
    canvas.setTextColor(groundMode ? canvas.color565(255, 190, 80) : dimColor(playerColor, 0.75f), TFT_TRANSPARENT);
    canvas.drawString(groundMissionLine, Config::SCREEN_W / 2, 11);
    drawGroundMissionOverlay(now);
    drawGroundToast(now);
    drawMiningLuckToast(now);

    canvas.setTextDatum(TR_DATUM);
    canvas.setTextColor(TFT_LIGHTGREY, TFT_TRANSPARENT);
    char waveBuf[18]; snprintf(waveBuf, sizeof(waveBuf), "W%u L%u", wave, level);
    canvas.drawString(waveBuf, Config::SCREEN_W - 2, 2);
    char tBuf[16]; snprintf(tBuf, sizeof(tBuf), "%.1fC", swarmTempC);
    canvas.drawString(tBuf, Config::SCREEN_W - 2, 11);
    char pBuf[16]; snprintf(pBuf, sizeof(pBuf), "%.0fhP", swarmPressureHpa);
    canvas.drawString(pBuf, Config::SCREEN_W - 2, 20);
    drawHeroHud();
    if (recordsToastUntilMs && now < recordsToastUntilMs) drawRecords();

    // v8.15B compact transparent miner HUD on the right side.
    // No opaque gold capsule and no 36px bottom wall.
    uint32_t poolH = minerRealHashrate;
    uint32_t localH = minerLocalHashrate;
    uint32_t remoteH = minerRemoteHashrate;
    uint32_t totalH = poolH + localH + remoteH;
    uint32_t sharesNow = getMinerSharesSafe();
    uint32_t rejectsNow = getMinerRejectsSafe();
    uint32_t bestNow = getMinerBestBitsSafe();
    const char* mmode = remoteH ? "BUZ" : (poolH ? "POOL" : (localH ? "SELF" : "SEEK"));

    uint16_t goldLine = canvas.color565(224, 168, 34);
    const int panelW = 60; // v8.15B: equal to left hero HUD width
    const int panelH = 21; // v8.15B: equal to left hero HUD height
    const int panelX = Config::SCREEN_W - panelW - 2; // 66 on 128px screen, no overlap with left 2..62
    const int panelY = Config::SCREEN_H - panelH - 2;

    canvas.setTextDatum(TL_DATUM);
    canvas.drawRoundRect(panelX, panelY, panelW, panelH, 3, dimColor(goldLine, 0.72f));

    canvas.setTextColor(goldLine, TFT_TRANSPARENT);
    char mh1[24]; snprintf(mh1, sizeof(mh1), "%s H:%s", mmode, compactK(totalH));
    canvas.drawString(mh1, panelX + 3, panelY + 2);

    char mh2[26]; snprintf(mh2, sizeof(mh2), "S%s R%s B%lu/%u",
                           compactK(sharesNow + remoteSharesSent),
                           compactK(rejectsNow),
                           (unsigned long)bestNow,
                           (unsigned)minerShareTargetBits);
    canvas.setTextColor(brightenColor(goldLine, 0.28f), TFT_TRANSPARENT);
    canvas.drawString(mh2, panelX + 3, panelY + 11);

    canvas.setTextDatum(TC_DATUM);
    canvas.setTextColor(TFT_DARKGREY, TFT_TRANSPARENT);
    canvas.drawString(brightLevels[brightIdx] == 0 ? "LCD OFF" : (groundMode ? "RAID" : (shouldStayRemoteMode() ? "REMOTE" : "LOCAL")),
                      Config::SCREEN_W / 2, 2);

    if (!soundEnabled) {
      canvas.setTextDatum(TC_DATUM);
      canvas.setTextColor(TFT_SILVER, TFT_TRANSPARENT);
      canvas.drawString("MUTE", Config::SCREEN_W / 2, 11);
    }

    canvas.setTextDatum(TL_DATUM);
    canvas.pushSprite(0, 0);
  }
};


NeuralSwarm swarm;

void setup() {
  Serial.begin(115200);
  delay(200);
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setBrightness(70);   // temporary boot brightness; saved UI state is applied in swarm.init()
  canvas.createSprite(Config::SCREEN_W, Config::SCREEN_H);
  

  // EchoBase init first on official Wire SDA=38/SCL=39.
  // After codec/I2S setup, Wire is switched back to BPS SDA=2/SCL=1.
  Wire.end();
  delay(20);
  echoBaseReady = echobase.init(16000, 38, 39, 7, 6, 5, 8, Wire);
  if (echoBaseReady) {
    echobase.setSpeakerVolume(0);
    echobase.setMicGain(ES8311_MIC_GAIN_12DB);  // v8.28 VOICEFORMANT: middle gain, AGC handles loud/quiet speech
    echobase.setMute(true);
    Serial.println("[ECHO-MIC] EchoBase init OK on Wire SDA=38 SCL=39");
  } else {
    Serial.println("[ECHO-MIC] EchoBase init FAIL on Wire SDA=38 SCL=39");
  }
  delay(20);
  Wire.end();
  delay(20);

  Wire.begin(2, 1, 400000); 
  
  // Инициализация BPS с правильной сигнатурой
  if (!qmp.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, 2, 1, 400000U)) {
    if (!qmp.begin(&Wire, 0x56, 2, 1, 400000U)) {
      Serial.println("BPS Sensor not found!");
    }
  }
  
  M5.Speaker.setVolume(0);
initSwarmWorkerName();
  Serial.printf("JANUS GROUNDOPS v8.31E4S2 STABLE SWARM PEERGUARD + UI MEMORY NVSFIX worker: %s\n", MINER_USER);
  Serial.println("[COLONY] v8.31E4S2 ATOM_SWARM_TRON: E4S1 + NVS namespace fix + range0 guard + peer rebuild throttle + WiFi backoff + UI memory.");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial.printf("WiFi connecting to SSID: %s\n", WIFI_SSID);
  Serial.printf("Pool endpoint: stratum+tcp://%s:%u\n", POOL_HOST, POOL_PORT);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  janusColonyBegin();
  janusBlackboardBootEvent();

  xTaskCreatePinnedToCore(
    microMinerTask,
    "SwarmMiner",
    12288,
    NULL,
    1,
    &minerTaskHandle,
    0
  );

  randomSeed(esp_random());
  swarm.init();
}

void loop() {
  swarm.update();
  swarm.draw();
  janusColonyAITick();
  janusColonyTick();
  janusAudioTxTick();
  tachyonProphecyTick();
  janusBlackboardTronTick();
  static uint32_t lastTdLog = 0;
  if (millis() - lastTdLog > 5000) {
    lastTdLog = millis();
    char statusCopy[20];
    copyMinerStatus(statusCopy, sizeof(statusCopy));
    Serial.printf("[TD] mode=%s remoteH=%lu localH=%lu poolH=%lu mic=%.1f/%.1f floor=%.1f raw=%.1f gate=%.1f fg=%lu micFrames=%lu micFail=%lu echoOk=%lu echoFail=%lu echoReady=%d audio=%d audioTx=%lu audioFail=%lu shares=%lu remoteShares=%lu best=%lu status=%s agent=%s\n",
      shouldStayRemoteMode() ? "REMOTE" : "LOCAL",
      (unsigned long)minerRemoteHashrate,
      (unsigned long)minerLocalHashrate,
      (unsigned long)minerRealHashrate,
      swarmMicRms, swarmMicPeak, micNoiseFloor, micRawRms, micLastGated, (unsigned long)micFloorGuardHits,
      (unsigned long)swarmMicFrames,
      (unsigned long)swarmMicFails,
      (unsigned long)echoMicReadOk,
      (unsigned long)echoMicReadFail,
      echoBaseReady ? 1 : 0,
      janusAudioTxEnabled ? 1 : 0,
      (unsigned long)janusAudioTxFrames,
      (unsigned long)janusAudioTxSendFail,
      (unsigned long)getMinerSharesSafe(),
      (unsigned long)remoteSharesSent,
      (unsigned long)getMinerBestBitsSafe(),
      statusCopy, janusPersonalLine, swarmMemoryLine);
  }
  delay(1); 
}