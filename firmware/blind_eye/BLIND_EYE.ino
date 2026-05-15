#include <M5Unified.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <M5_STHS34PF80.h>
#include <LittleFS.h>
#include <math.h>

// ========================= JANUS Blind Eye v3.0 EAGLE FOCUS LOCK + TMOS/PIR SMOOTH + MEMORY =========================
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
#define JANUS_EYE_VISION_IDLE_MS           2600UL

// v2.9 Eagle Focus: software aperture/AGC for the single-zone STHS34PF80.
// It does not fake distance; it makes weak far-field deltas visible without
// letting the room baseline drift into the target.
#define JANUS_EYE_EAGLE_FOCUS_ENABLE       1
#define JANUS_EYE_FOCUS_MIN_GAIN           2.20f
#define JANUS_EYE_FOCUS_MAX_GAIN           11.80f
#define JANUS_EYE_BASELINE_ALPHA_QUIET     0.0018f
#define JANUS_EYE_BASELINE_ALPHA_HOT       0.000018f
#define JANUS_EYE_NOISE_ALPHA              0.0035f
#define JANUS_EYE_PRESENCE_FLAG_LEVEL      18.0f
#define JANUS_EYE_MOTION_FLAG_LEVEL        14.0f

// v3.0 Eagle Focus Lock: single-zone STHS34PF80 cannot know real distance,
// but it can behave like a biological eye: calibrate, squint/focus, smooth,
// remember a coherent target, and then decay ghosts when evidence disappears.
#define JANUS_EYE_LOCK_ENABLE              1
#define JANUS_EYE_RECALIBRATE_ON_BOOT      1
#define JANUS_EYE_CALIB_SAMPLES            96
#define JANUS_EYE_RAW_EMA_ALPHA            0.18f
#define JANUS_EYE_RAW_SPIKE_ALPHA          0.42f
#define JANUS_EYE_SIGNAL_RISE_ALPHA        0.34f
#define JANUS_EYE_SIGNAL_FALL_ALPHA        0.055f
#define JANUS_EYE_OCC_RISE_ALPHA           0.16f
#define JANUS_EYE_OCC_FALL_ALPHA           0.018f
#define JANUS_EYE_MEMORY_DECAY_ALPHA       0.0065f
#define JANUS_EYE_GHOST_DECAY_ALPHA        0.070f
#define JANUS_EYE_FAR_LOCK_ALPHA           0.055f
#define JANUS_EYE_FAR_UNLOCK_ALPHA         0.014f
#define JANUS_EYE_ABSENT_REBASE_MS         14000UL
#define JANUS_EYE_QUIET_REBASE_ALPHA       0.0060f
#define JANUS_EYE_NEGATIVE_REBASE_ALPHA    0.0180f
#define JANUS_EYE_PRESENT_ON_LEVEL         0.78f
#define JANUS_EYE_PRESENT_OFF_LEVEL        0.32f
#define JANUS_EYE_MIN_PRESENT_MS           650UL


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
};

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
uint32_t colonyLastHashTickMs = 0;
uint16_t colonyWorkerId = 0;
uint8_t colonyPeerChannel = 0;
int8_t colonyLastRssi = 0;
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

void sendShareResponse(const RemoteJobState& job, uint32_t nonce) {
  ShareResponse sr{};
  sr.magic[0] = 'S'; sr.magic[1] = 'R';
  memcpy(sr.job_id, job.job_id, 8);
  sr.nonce = nonce;
  sr.worker_id = colonyWorkerId;
  esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)&sr, sizeof(sr));
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

    uint32_t nonce = colonyJob.nonce++;
    colonyJob.hashesDone++;

    memcpy(header, colonyJob.header, 80);
    writeLE32(header + 76, nonce);

    doubleSha256(header, 80, rawHash);
    hashToShareOrder(rawHash, shareHash);

    colonyHashCounter++;
    uint16_t bits = countLeadingZeroBitsBE(shareHash);
    if (bits > colonyBestBits) colonyBestBits = bits;

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

void ensureColonyPeer() {
  uint8_t ch = getWifiChannelSafe();
  if (ch == 0) ch = JANUS_BROADCAST_CHANNEL;
  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC) && colonyPeerChannel == ch) return;
  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) esp_now_del_peer(JANUS_BROADCAST_MAC);
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6);
  peer.channel = ch;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) == ESP_OK) colonyPeerChannel = ch;
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
#define DEVICE_KIND            "blind_eye_full_v2_9_kenshi_tachyon_prophecy_eagle_focus"
#define WIFI_SSID "YOUR_WIFI"
#define WIFI_PASSWORD "YOUR_PASSWORD"
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
#define GROVE_SDA_PIN          2
#define GROVE_SCL_PIN          1

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

// v3.0 Eagle Focus Lock runtime state.
// Raw -> EMA -> baseline/noise -> signal -> occupancy/memory.
// "presence_now" is real evidence; "memory" is not allowed to set a hard presence flag.
float tmos_raw_presence_ema = 0.0f;
float tmos_raw_motion_ema = 0.0f;
float tmos_presence_signal = 0.0f;
float tmos_motion_signal = 0.0f;
float tmos_presence_integrator = 0.0f;
float tmos_motion_integrator = 0.0f;
float tmos_focus_lock = 0.0f;
float tmos_occupancy = 0.0f;
float tmos_memory = 0.0f;
float tmos_ghost_pressure = 0.0f;
float tmos_calibration_quality = 0.0f;
bool tmos_raw_filter_ready = false;
bool tmos_presence_now = false;
bool tmos_motion_now = false;
uint32_t tmos_present_since_ms = 0;
uint32_t tmos_last_real_ms = 0;
uint32_t tmos_last_absent_ms = 0;
uint16_t tmos_absent_frames = 0;
uint16_t tmos_present_frames = 0;
float mic_rms = 0;
int wifi_rssi = -127;

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
float janusEmaF(float prev, float cur, float alpha) {
  if (!isfinite(prev)) return cur;
  if (!isfinite(cur)) return prev;
  return prev * (1.0f - alpha) + cur * alpha;
}

float janusApproachF(float prev, float target, float riseAlpha, float fallAlpha) {
  float a = (target > prev) ? riseAlpha : fallAlpha;
  return janusEmaF(prev, target, a);
}

void tmosResetFocusRuntime(float baseP, float baseM, float noiseP, float noiseM) {
  tmos_presence_baseline = baseP;
  tmos_motion_baseline = baseM;
  tmos_presence_noise = constrain(noiseP, 2.5f, 95.0f);
  tmos_motion_noise = constrain(noiseM, 2.0f, 90.0f);
  tmos_focus_gain = 3.2f;
  tmos_focus_confidence = 0.0f;
  tmos_focus_lock = 0.0f;
  tmos_presence_signal = 0.0f;
  tmos_motion_signal = 0.0f;
  tmos_presence_integrator = 0.0f;
  tmos_motion_integrator = 0.0f;
  tmos_occupancy = 0.0f;
  tmos_memory = 0.0f;
  tmos_ghost_pressure = 0.0f;
  tmos_presence_now = false;
  tmos_motion_now = false;
  tmos_absent_frames = 0;
  tmos_present_frames = 0;
  tmos_raw_presence_ema = baseP;
  tmos_raw_motion_ema = baseM;
  tmos_raw_filter_ready = true;
  tmos_focus_ready = true;
  tmos_last_absent_ms = millis();
  tmos_last_real_ms = 0;
}

bool tmosStrongPresenceEvidence() {
  return tmos_presence_now || tmos_motion_now ||
         tmos_presence > JANUS_EYE_PRESENCE_FLAG_LEVEL ||
         tmos_motion > JANUS_EYE_MOTION_FLAG_LEVEL ||
         tmos_occupancy > 0.48f;
}


void calibrateTMOS() {
  if (!tmos_ready) return;

  int32_t sumP = 0;
  int32_t sumM = 0;
  int32_t minP = 32767, maxP = -32768;
  int32_t minM = 32767, maxM = -32768;
  const int samples = JANUS_EYE_CALIB_SAMPLES;

  // v3.0: longer boot calibration. Keep the eye aimed at a neutral/empty part
  // of the room for ~3 seconds. If this was not possible, negative-rebase logic
  // below can recover from a too-hot baseline instead of staying blind.
  int16_t prevP = 0;
  int16_t prevM = 0;
  int32_t jumpAccum = 0;

  for (int i = 0; i < samples; ++i) {
    tmos.getPresenceValue(&raw_presence);
    tmos.getMotionValue(&raw_motion);
    sumP += raw_presence;
    sumM += raw_motion;
    minP = min<int32_t>(minP, raw_presence);
    maxP = max<int32_t>(maxP, raw_presence);
    minM = min<int32_t>(minM, raw_motion);
    maxM = max<int32_t>(maxM, raw_motion);
    if (i > 0) jumpAccum += abs((int)raw_presence - (int)prevP) + abs((int)raw_motion - (int)prevM);
    prevP = raw_presence;
    prevM = raw_motion;
    delay(26);
  }

  calib_presence = sumP / samples;
  calib_motion = sumM / samples;

  float spanP = (float)(maxP - minP);
  float spanM = (float)(maxM - minM);
  tmos_calibration_quality = constrain(1.0f - ((float)jumpAccum / max(1, samples - 1)) / 180.0f, 0.0f, 1.0f);

  float noiseP = spanP * 0.38f + 5.0f;
  float noiseM = spanM * 0.38f + 3.5f;

  tmosResetFocusRuntime((float)calib_presence, (float)calib_motion, noiseP, noiseM);
  calibrated = true;

  Serial.printf("[EYE/CAL] TMOS base=%d/%d span=%.1f/%.1f noise=%.1f/%.1f q=%.2f\n",
                calib_presence, calib_motion, spanP, spanM,
                tmos_presence_noise, tmos_motion_noise, tmos_calibration_quality);
}
void readTMOS() {
  if (!tmos_ready) {
    tmos_presence = 0.0f;
    tmos_motion = 0.0f;
    tmos_focus_confidence = 0.0f;
    tmos_presence_now = false;
    tmos_motion_now = false;
    return;
  }

  tmos.getPresenceValue(&raw_presence);
  tmos.getMotionValue(&raw_motion);

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
    tmosResetFocusRuntime((float)calib_presence, (float)calib_motion, 12.0f, 8.0f);
  }

  float rawP = (float)raw_presence;
  float rawM = (float)raw_motion;

  if (!tmos_raw_filter_ready) {
    tmos_raw_presence_ema = rawP;
    tmos_raw_motion_ema = rawM;
    tmos_raw_filter_ready = true;
  }

  // Smooth the raw STHS34PF80 presence/motion readings. Big jumps are allowed
  // to enter faster, but still not as single-frame chaos.
  float rawJump = max(fabsf(rawP - tmos_raw_presence_ema), fabsf(rawM - tmos_raw_motion_ema));
  float rawAlpha = (rawJump > 160.0f) ? JANUS_EYE_RAW_SPIKE_ALPHA : JANUS_EYE_RAW_EMA_ALPHA;
  tmos_raw_presence_ema = janusEmaF(tmos_raw_presence_ema, rawP, rawAlpha);
  tmos_raw_motion_ema = janusEmaF(tmos_raw_motion_ema, rawM, rawAlpha);

  float filtP = tmos_raw_presence_ema;
  float filtM = tmos_raw_motion_ema;
  float dP = filtP - tmos_presence_baseline;
  float dM = filtM - tmos_motion_baseline;
  float aP = fabsf(dP);
  float aM = fabsf(dM);

  // Negative rebase recovers from boot calibration made while a person/hand was in the beam.
  float hotGate = max(11.0f, tmos_presence_noise * 1.65f + tmos_motion_noise * 0.85f);
  bool negativeRebase = (dP < -(tmos_presence_noise * 2.6f + 8.0f)) || (dM < -(tmos_motion_noise * 2.4f + 6.0f));

  // Far targets create small slow deltas. A "hot" field freezes the baseline;
  // quiet room slowly follows. This prevents a sitting human at ~2m from being
  // absorbed into the zero point.
  bool hot = (max(aP, aM * 1.25f) > hotGate) || tmos_occupancy > 0.42f;
  float baseAlpha = hot ? JANUS_EYE_BASELINE_ALPHA_HOT : JANUS_EYE_BASELINE_ALPHA_QUIET;
  if (negativeRebase) baseAlpha = JANUS_EYE_NEGATIVE_REBASE_ALPHA;

  tmos_presence_baseline = tmos_presence_baseline * (1.0f - baseAlpha) + filtP * baseAlpha;
  tmos_motion_baseline   = tmos_motion_baseline   * (1.0f - baseAlpha) + filtM * baseAlpha;

  // Noise learns only from quiet frames. Strong movement should not raise the threshold.
  if (!hot && !negativeRebase) {
    tmos_presence_noise = janusEmaF(tmos_presence_noise, aP, JANUS_EYE_NOISE_ALPHA);
    tmos_motion_noise = janusEmaF(tmos_motion_noise, aM, JANUS_EYE_NOISE_ALPHA);
  } else {
    tmos_presence_noise = tmos_presence_noise * 0.9995f + min(aP, tmos_presence_noise) * 0.0005f;
    tmos_motion_noise = tmos_motion_noise * 0.9995f + min(aM, tmos_motion_noise) * 0.0005f;
  }
  tmos_presence_noise = constrain(tmos_presence_noise, 2.5f, 120.0f);
  tmos_motion_noise = constrain(tmos_motion_noise, 2.0f, 110.0f);

  // Software "squint": weak but coherent positive presence increases focus lock.
  float pWeak = max(0.0f, dP - (tmos_presence_noise * 0.28f + 0.8f));
  float mWeak = max(0.0f, aM - (tmos_motion_noise * 0.25f + 0.7f));
  bool coherentFar = (pWeak > 0.55f && dP > 0.0f && aP < 180.0f && aM < 260.0f);
  float lockTarget = coherentFar ? constrain(pWeak / max(9.0f, tmos_presence_noise * 2.0f), 0.0f, 1.0f) : 0.0f;
  tmos_focus_lock = janusApproachF(tmos_focus_lock, lockTarget, JANUS_EYE_FAR_LOCK_ALPHA, JANUS_EYE_FAR_UNLOCK_ALPHA);

  float targetGain = constrain(10.8f - (tmos_presence_noise + tmos_motion_noise) * 0.028f + tmos_focus_lock * 3.8f,
                               JANUS_EYE_FOCUS_MIN_GAIN, JANUS_EYE_FOCUS_MAX_GAIN);
  if (hot) targetGain = min(JANUS_EYE_FOCUS_MAX_GAIN, targetGain + 0.65f);
  tmos_focus_gain = janusEmaF(tmos_focus_gain, targetGain, 0.12f);

  // Detection math: lower threshold for far-field, but integrate slowly so
  // random TMOS/PIR jumps do not become a permanent ghost.
  float pSignal = max(0.0f, dP - (tmos_presence_noise * 0.34f + 1.15f));
  float mSignal = max(0.0f, aM - (tmos_motion_noise * 0.32f + 0.95f));
  tmos_presence_delta = dP;
  tmos_motion_delta = dM;

  tmos_presence_integrator = constrain(tmos_presence_integrator * 0.955f + pSignal * 0.180f, 0.0f, 260.0f);
  tmos_motion_integrator = constrain(tmos_motion_integrator * 0.900f + mSignal * 0.210f, 0.0f, 220.0f);

  float pOut = (pSignal * 0.58f + tmos_presence_integrator * 0.92f) * tmos_focus_gain;
  float mOut = (mSignal * 0.66f + tmos_motion_integrator * 0.54f) * tmos_focus_gain;

  tmos_presence_signal = constrain(pOut, 0.0f, 1400.0f);
  tmos_motion_signal = constrain(mOut, 0.0f, 800.0f);

  tmos_presence = janusApproachF(tmos_presence, tmos_presence_signal, JANUS_EYE_SIGNAL_RISE_ALPHA, JANUS_EYE_SIGNAL_FALL_ALPHA);
  tmos_motion = janusApproachF(tmos_motion, tmos_motion_signal, JANUS_EYE_SIGNAL_RISE_ALPHA, JANUS_EYE_SIGNAL_FALL_ALPHA);

  float pNorm = tmos_presence / max(JANUS_EYE_PRESENCE_FLAG_LEVEL, 1.0f);
  float mNorm = tmos_motion / max(JANUS_EYE_MOTION_FLAG_LEVEL, 1.0f);
  float evidence = constrain(max(pNorm, mNorm * 0.88f) + tmos_focus_lock * 0.55f, 0.0f, 3.0f);
  bool evidenceNow = evidence > JANUS_EYE_PRESENT_ON_LEVEL;

  if (evidenceNow) {
    tmos_present_frames++;
    tmos_absent_frames = 0;
    if (tmos_present_since_ms == 0) tmos_present_since_ms = millis();
    if (millis() - tmos_present_since_ms >= JANUS_EYE_MIN_PRESENT_MS) {
      tmos_last_real_ms = millis();
    }
  } else {
    tmos_absent_frames++;
    if (tmos_absent_frames > 8) {
      tmos_present_frames = 0;
      tmos_present_since_ms = 0;
    }
  }

  float occTarget = evidenceNow ? constrain(evidence / 1.35f, 0.0f, 1.0f) : 0.0f;
  tmos_occupancy = janusApproachF(tmos_occupancy, occTarget, JANUS_EYE_OCC_RISE_ALPHA, JANUS_EYE_OCC_FALL_ALPHA);
  tmos_memory = max(tmos_memory * (1.0f - JANUS_EYE_MEMORY_DECAY_ALPHA), tmos_occupancy);

  bool hardOn = (tmos_occupancy > 0.36f && evidence > JANUS_EYE_PRESENT_ON_LEVEL);
  bool hardOff = (tmos_occupancy < JANUS_EYE_PRESENT_OFF_LEVEL && evidence < 0.55f && tmos_absent_frames > 12);

  if (hardOn) {
    tmos_presence_now = true;
    tmos_motion_now = mNorm > 0.80f;
    tmos_last_absent_ms = 0;
  } else if (hardOff) {
    tmos_presence_now = false;
    tmos_motion_now = false;
    if (tmos_last_absent_ms == 0) tmos_last_absent_ms = millis();
  }

  // Anti-ghost: if the room is quiet for long enough, decay outputs and allow
  // the baseline to settle. Predictions may remember, but the sensor no longer
  // claims "present now".
  bool quietFrame = !evidenceNow && aP < (tmos_presence_noise * 0.85f + 3.0f) && aM < (tmos_motion_noise * 0.85f + 2.5f);
  if (quietFrame && tmos_absent_frames > 18) {
    tmos_ghost_pressure = constrain(tmos_ghost_pressure + 0.025f, 0.0f, 1.0f);
    tmos_presence *= (1.0f - JANUS_EYE_GHOST_DECAY_ALPHA);
    tmos_motion *= (1.0f - JANUS_EYE_GHOST_DECAY_ALPHA * 1.4f);
    tmos_presence_integrator *= 0.88f;
    tmos_motion_integrator *= 0.82f;
  } else {
    tmos_ghost_pressure = max(0.0f, tmos_ghost_pressure - 0.050f);
  }

  if (quietFrame && tmos_last_absent_ms != 0 && millis() - tmos_last_absent_ms > JANUS_EYE_ABSENT_REBASE_MS) {
    tmos_presence_baseline = janusEmaF(tmos_presence_baseline, filtP, JANUS_EYE_QUIET_REBASE_ALPHA);
    tmos_motion_baseline = janusEmaF(tmos_motion_baseline, filtM, JANUS_EYE_QUIET_REBASE_ALPHA);
  }

  tmos_focus_confidence = constrain(tmos_focus_confidence * 0.82f + (tmos_occupancy + tmos_focus_lock * 0.65f) * 0.18f, 0.0f, 2.0f);
  if (tmos_presence_now || tmos_motion_now) tmos_last_focus_ms = millis();

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
  x[5] = constrain((float)wifi_rssi / -100.0f, 0.0f, 2.0f);

  // Tachyon/Physarious micro-features: future pressure, movement viscosity,
  // remote prophecies and Markov sector confidence. v3.0 gates predictions so
  // "memory" can help attention but cannot masquerade as real present evidence.
  x[6] = constrain((tmos_presence_now ? tachyonPredPresence1 : tachyonPredPresence1 * 0.35f) / 1000.0f, 0.0f, 2.0f);
  x[7] = constrain((tmos_motion_now ? tachyonPredMotion1 : tachyonPredMotion1 * 0.35f) / 500.0f, 0.0f, 2.0f);
  x[8] = constrain(tachyonSwarmPressure + kenshiConfidence * 0.25f + tmos_focus_confidence * 0.08f + tmos_occupancy * 0.42f, 0.0f, 2.0f);
  x[9] = constrain(tachyonFutureStress * (0.55f + tmos_occupancy * 0.45f), 0.0f, 2.0f);
}


float predict(const float x[FEATURE_DIM]) {
  float y = model_b;
  for (int i = 0; i < FEATURE_DIM; ++i) y += model_w[i] * x[i];
  return y;
}
float computeActivity() {
  // v3.0: activity follows smoothed real evidence and occupancy, not raw TMOS jumps.
  return
    tmos_presence * 0.00165f +
    tmos_motion * 0.00325f +
    tmos_occupancy * 1.10f +
    mic_rms * 20.0f +
    mag_norm * 0.010f +
    imu_shock * 0.20f +
    tmos_focus_confidence * 0.06f -
    tmos_ghost_pressure * 0.55f;
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
  else if (tmos_presence_now && tmos_motion_now) statusLine = "eye locked moving";
  else if (tmos_presence_now) statusLine = "eye locked";
  else if (tmos_ghost_pressure > 0.55f) statusLine = "eye clearing ghost";
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

  StaticJsonDocument<768> doc;
  doc["calibrated"] = calibrated;
  doc["fit_best"] = fit_best;
  doc["model_lr"] = model_lr;
  doc["calib_presence"] = calib_presence;
  doc["calib_motion"] = calib_motion;
  doc["focus_base_p"] = tmos_presence_baseline;
  doc["focus_base_m"] = tmos_motion_baseline;
  doc["focus_noise_p"] = tmos_presence_noise;
  doc["focus_noise_m"] = tmos_motion_noise;
  doc["focus_gain"] = tmos_focus_gain;
  doc["focus_lock"] = tmos_focus_lock;
  doc["calib_q"] = tmos_calibration_quality;
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

  StaticJsonDocument<768> doc;
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
  tmos_presence_baseline = doc["focus_base_p"] | (float)calib_presence;
  tmos_motion_baseline = doc["focus_base_m"] | (float)calib_motion;
  tmos_presence_noise = constrain(doc["focus_noise_p"] | tmos_presence_noise, 2.5f, 120.0f);
  tmos_motion_noise = constrain(doc["focus_noise_m"] | tmos_motion_noise, 2.0f, 110.0f);
  tmos_focus_gain = constrain(doc["focus_gain"] | tmos_focus_gain, JANUS_EYE_FOCUS_MIN_GAIN, JANUS_EYE_FOCUS_MAX_GAIN);
  tmos_focus_lock = constrain(doc["focus_lock"] | tmos_focus_lock, 0.0f, 1.0f);
  tmos_calibration_quality = constrain(doc["calib_q"] | tmos_calibration_quality, 0.0f, 1.0f);
  tmos_focus_ready = calibrated;
  tachyonPresenceConfidence = doc["tachyon_pg"] | tachyonPresenceConfidence;
  tachyonMotionConfidence = doc["tachyon_mg"] | tachyonMotionConfidence;
  tachyonTrendGain = constrain(doc["tachyon_tg"] | tachyonTrendGain, 0.10f, 1.50f);
  tachyonMemoryGain = constrain(doc["tachyon_mg2"] | tachyonMemoryGain, 0.05f, 1.00f);
  tachyonRemoteGain = constrain(doc["tachyon_rg"] | tachyonRemoteGain, 0.00f, 0.80f);
}

// ========================= IO =========================

String buildPayload() {
  StaticJsonDocument<1536> doc;
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
  d["tmos_presence_now"] = tmos_presence_now ? 1 : 0;
  d["tmos_motion_now"] = tmos_motion_now ? 1 : 0;
  d["tmos_occupancy"] = tmos_occupancy;
  d["tmos_memory"] = tmos_memory;
  d["tmos_focus_lock"] = tmos_focus_lock;
  d["tmos_ghost"] = tmos_ghost_pressure;
  d["tmos_base_p"] = tmos_presence_baseline;
  d["tmos_base_m"] = tmos_motion_baseline;
  d["tmos_noise_p"] = tmos_presence_noise;
  d["tmos_noise_m"] = tmos_motion_noise;
  d["raw_presence"] = raw_presence;
  d["raw_motion"] = raw_motion;
  d["mic_rms"] = mic_rms;
  d["wifi_rssi"] = wifi_rssi;
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
  K_WORLD_LOW_SYNC = 1UL << 9
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

  if (kenshiEventPower > 2.05f || fabsf(z_activity) > 3.2f || fabsf(z_loss) > 3.0f) {
    kenshiBubbleState = 3;
    kenshiJobState = 3; // alert
  } else if (tmos_motion > 9.0f || tmos_presence > 25.0f || mic_rms > 0.02f) {
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
  kp.flags |= 0x08; // future Motion Base/servo planner compatible
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
  kp.values[5] = fit;

  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&kp, sizeof(kp));
  if (err == ESP_OK) {
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

  // Anti-ghost: if no real evidence is present for a while, prophecy decays.
  // The eye may remember a path, but it must not keep broadcasting "human now".
  if (!tmos_presence_now && !tmos_motion_now && tmos_absent_frames > 24) {
    float decay = constrain(0.985f - tmos_ghost_pressure * 0.075f, 0.86f, 0.985f);
    tachyonPredPresence1 *= decay;
    tachyonPredPresence2 *= decay;
    tachyonPredPresence3 *= decay;
    tachyonPredMotion1 *= (decay * 0.96f);
    tachyonPredMotion2 *= (decay * 0.96f);
    tachyonPredMotion3 *= (decay * 0.96f);
    tachyonFutureStress *= (decay * 0.94f);
  }

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
  // v3.0: flags mean real sensor evidence now. Prediction/memory is reported in values,
  // but it must not make the room look occupied when it is empty.
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

  if (esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&tp, sizeof(tp)) == ESP_OK) {
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

  // v3.0: presence/motion flags are real-current only. Prediction/memory affects
  // dim aperture shape but cannot lie that somebody is there.
  if (tmos_motion_now) ef.flags |= 0x01;
  if (tmos_presence_now) ef.flags |= 0x02;

  float pPower = constrain(tmos_presence / 220.0f, 0.0f, 1.0f);
  float mPower = constrain(tmos_motion / 150.0f, 0.0f, 1.0f);
  float memoryPower = constrain(tmos_memory * 0.28f, 0.0f, 0.28f);
  float predPower = constrain(max(tachyonPredPresence1 / 1200.0f, tachyonPredMotion1 / 700.0f) * 0.18f, 0.0f, 0.18f);
  float base = constrain(0.025f + pPower * 0.58f + mPower * 0.34f + memoryPower + predPower +
                         tachyonFutureStress * 0.025f + tmos_focus_lock * 0.10f - tmos_ghost_pressure * 0.22f,
                         0.0f, 1.0f);
  uint8_t curSector = kenshiSector & 7;
  uint8_t nextSector = kenshiPredSector & 7;

  for (uint8_t y = 0; y < JANUS_EYE_VISION_H; ++y) {
    for (uint8_t x = 0; x < JANUS_EYE_VISION_W; ++x) {
      float cx = (float)x - 3.5f;
      float cy = (float)y - 3.5f;
      float r2 = cx * cx + cy * cy;
      // Squint aperture: higher focus lock tightens the beam instead of lighting
      // the whole grid. This gives "eagle focus" without fake camera pixels.
      float apertureShape = 1.0f + r2 * (0.18f + tmos_focus_lock * 0.18f);
      int aperture = (int)(base * 138.0f / apertureShape);
      int rayNow = tmos_presence_now ? (eyeVisionSectorIntensity(x, y, curSector, mPower) / 2) : 0;
      int rayNext = (int)(eyeVisionSectorIntensity(x, y, nextSector, constrain(tachyonFutureStress, 0.0f, 1.0f)) * 0.18f);
      int pulse = tmos_presence_now ? (int)(6.0f + 4.0f * sinf((float)millis() * 0.003f + (float)(x + y))) : 0;
      ef.pixels[y * JANUS_EYE_VISION_W + x] = (uint8_t)constrain(aperture + rayNow + rayNext + pulse, 0, 255);
    }
  }

  if (esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&ef, sizeof(ef)) == ESP_OK) {
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


// ========================= JANUS COLONY EYE HOOKS =========================
float eyeLocalEntropy() {
  float agentEntropy = (float)(colonyAgentEntropySeed & 0xFFFF) / 65535.0f;
  return constrain(tmos_presence * 0.0018f + tmos_motion * 0.0032f + tmos_occupancy * 0.65f +
                   mic_rms * 20.0f + mag_norm * 0.010f + imu_shock * 0.20f + loss * 1.5f +
                   agentEntropy * 0.75f + (float)colonyAgentLevel * 0.10f +
                   tachyonFutureStress * 0.25f - tmos_ghost_pressure * 0.50f,
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
  esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
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
  er.sensor_flags = 0x27; // mic + TMOS + IMU/mag + Kenshi bubble metadata available
  er.values[0] = mic_rms;
  er.values[1] = tmos_presence;
  er.values[2] = mag_norm;
  er.values[3] = loss;
  esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&er, sizeof(er));

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
  er2.values[7] = colonyAgentScore;   // v2.5: Buzz Agent training feedback loop
  er2.uptime_ms = millis();
  esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&er2, sizeof(er2));
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

  Serial.printf("[EYE] focus rawP=%d rawM=%d ema=%.1f/%.1f dP=%.1f dM=%.1f base=%.1f/%.1f noise=%.1f/%.1f gain=%.2f lock=%.2f occ=%.2f mem=%.2f ghost=%.2f now=%d/%d conf=%.2f\n",
                raw_presence, raw_motion,
                tmos_raw_presence_ema, tmos_raw_motion_ema,
                tmos_presence_delta, tmos_motion_delta,
                tmos_presence_baseline, tmos_motion_baseline,
                tmos_presence_noise, tmos_motion_noise,
                tmos_focus_gain, tmos_focus_lock, tmos_occupancy, tmos_memory, tmos_ghost_pressure,
                tmos_presence_now ? 1 : 0, tmos_motion_now ? 1 : 0, tmos_focus_confidence);

  Serial.printf("[EYE] miner H=%lu best=%lu target=%u tickets=%lu jobs=%lu done=%lu exp=%lu\n",
                (unsigned long)colonyRemoteHashrate,
                (unsigned long)colonyBestBits,
                (unsigned)colonyTargetBits,
                (unsigned long)colonyRemoteShares,
                (unsigned long)colonyJobsSeen,
                (unsigned long)colonyJobsDone,
                (unsigned long)colonyJobsExpired);

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
}

// ========================= MAIN =========================

void readSensors() {
  readIMUClassic();
  readTMOS();
  mic_rms = readMicRms();
  wifi_rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
  updateMiniGPT();
  updateTachyonProphecy();
  updateKenshiVirtualWorld();
}

void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);  // Used for IMU/buttons/power only. Blind EYE is headless: no display output.
  Serial.begin(115200);
  Serial.println("JANUS Blind Eye v3.0 EAGLE FOCUS LOCK + TMOS/PIR SMOOTH + MEMORY / HEADLESS / no SELF mining");

  LittleFS.begin(true);
  loadState();
  loadModel();
  loadKenshiState();
  loadTachyonState();

  Wire.begin(GROVE_SDA_PIN, GROVE_SCL_PIN, 100000);
  initIMU();
  initTMOS();
  initMicI2S();
  initWiFi(true);
  initColonyNow();

#if JANUS_EYE_RECALIBRATE_ON_BOOT
  if (tmos_ready) {
    calibrateTMOS();
  }
#else
  if (!calibrated && tmos_ready) {
    calibrateTMOS();
  }
#endif

  statusLine = M5.Imu.isEnabled() ? "imu ready + kenshi tachyon" : "imu disabled";
}

void loop() {
  M5.update();
  unsigned long now = millis();

  if (now - lastSensorAt >= SENSOR_INTERVAL_MS) {
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

  if (WiFi.status() != WL_CONNECTED) {
    initWiFi(false);
  }

  delay(1);
}