/*
  JANUS_BEACON_ADV_v4_5A_BLACKBOX_HOME_CORTEX_POLISH.ino
  BeaconADV v4.5A BLACKBOX-HOME-CORTEX-POLISH / SD-TACHYON / RAMANUJAN THETA / CYBER BEATMAKER

  Preservation rule:
  - Base is the older 2906-line Beacon sketch.
  - Old ESP-NOW colony, BlindEye/Audio intake, HTTP queue, LittleFS/SD archive, LoRa, QMP/IMU, UI, anomaly logic are preserved.
  - v4.4 adds SD-first tachyon memory, LittleFS relief, 15GB SD brain budget, bounded self-cleanup, Ramanujan theta notebook, SHA-home Love.json enrichment, and non-blocking beatmaker.
  - v4.5 adds Janus Home Cortex blackboard J/E + J/P, K2/TP prophecy mirror, SwarmSense S/S, peer self-healing, and SD black-box witness mode.
  - v4.5A polishes Cardputer ADV SD boot order, filters legacy ER1 loss spikes, and throttles THETA serial spam.

  Strict note:
  - This does not bypass SHA-256.
  - This does not replace the old Beacon organs; it gives them a new cortex.
*/

struct SwarmAiNodeState;  // Arduino .ino prototype guard for AI node helpers
#include <M5Cardputer.h>
#include <FastLED.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <math.h>

#include <M5UnitENV.h>
// Required for ENV.III / QMP6988 pressure.
// Install in Arduino IDE: Library Manager -> "M5Unit-ENV" by M5Stack.
#define JANUS_HAS_QMP6988 1
QMP6988 janusQmp6988;


// ========================= JANUS COLONY ESP-NOW =========================
#include <esp_now.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>

#define JANUS_COLONY_ENABLE 1
#define JANUS_BROADCAST_CHANNEL 0
#define COLONY_HEARTBEAT_MS 2000UL
#define COLONY_ENTROPY_MS 2500UL
#define COLONY_MASTER_TIMEOUT_MS 18000UL
#define COLONY_REMOTE_BATCH 220

// v4.5 JANUS HOME CORTEX / BLACKBOX additions.
// Beacon becomes the long-memory witness node for the distributed swarm.
#define JANUS_BEACON_BLACKBOARD_ENABLE       1
#define JANUS_BEACON_EVENT_BASE_MS           7000UL
#define JANUS_BEACON_EVENT_FAST_MS           2200UL
#define JANUS_BEACON_MEMORY_MS               30000UL
#define JANUS_BEACON_ENV_MS                  12000UL
#define JANUS_BEACON_TASK_MS                 18000UL
#define JANUS_BEACON_SWARMSENSE_MS           3500UL
#define JANUS_BEACON_K2_MS                   4200UL
#define JANUS_BEACON_TP_MS                   5200UL
#define JANUS_BEACON_BLACKBOX_MS             20000UL
#define JANUS_BEACON_CORE_STALE_MS           45000UL
#define JANUS_BEACON_POLICY_TTL_GUARD_MS     18000UL
#define JANUS_BEACON_BLACKBOX_LOG            "/janus_ai/blackbox.jsonl"
#define JANUS_BEACON_EVENT_BLACKBOX_LOG      "/janus_ai/blackboard_events.jsonl"

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

// Core v6.41+ JANUS BLACKBOARD packets.
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
  uint32_t quietUntilMs;   // duration ms from Core, not foreign absolute millis
  uint16_t dominantTopic;
  uint16_t danger_x100;
  char order[40];
};

struct __attribute__((packed)) JanusKenshiPacket {
  uint8_t magic[2];        // 'K','2'
  uint8_t version;
  uint8_t flags;
  char nodeId[24];
  uint32_t seq;
  uint16_t worker_id;
  uint32_t uptime_ms;
  uint8_t activeBubbleNodes;
  uint8_t virtualNodes;
  uint32_t worldFlags;
  uint8_t sector;
  uint8_t predictedSector;
  uint8_t jobState;
  uint8_t priority;
  int8_t rssi;
  float entropy;
  float activity;
  float confidence;
  float values[6];
};

struct __attribute__((packed)) JanusTachyonProphecyPacket {
  uint8_t magic[2];        // 'T','P'
  uint8_t version;
  uint8_t flags;
  char nodeId[24];
  uint32_t seq;
  uint16_t worker_id;
  uint32_t uptime_ms;
  uint16_t horizon_ms;
  uint8_t sector;
  uint8_t predictedSector;
  uint8_t confidence;
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

struct __attribute__((packed)) JanusAiNodePacket {
  uint8_t magic[2];        // 'A','I'
  uint8_t version;         // 1
  uint8_t flags;           // bit0=has SD, bit1=can archive, bit2=master-ish
  char nodeId[24];
  char role[16];
  uint32_t seq;
  uint32_t uptime_ms;
  float entropy;
  float prediction_error;
  float sync;
  float fit;
  float attention;
  float values[6];         // free sensor/model slots
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
uint32_t colonyBestBits = 0;
uint32_t colonyHashCounter = 0;
uint32_t colonyLastHashTickMs = 0;
uint16_t colonyWorkerId = 0;
uint8_t colonyPeerChannel = 0;
int8_t colonyLastRssi = 0;
char colonyMode[10] = "LOCAL";
float colonyEntropyWeight = 1.0f;
float colonySurpriseWeight = 1.0f;

// v4.5 radio self-healing + Home Cortex state.
uint32_t colonyPeerRebuilds = 0;
uint32_t colonyTxOk = 0;
uint32_t colonyTxFail = 0;
esp_err_t colonyLastTxErr = ESP_OK;
char colonyLastTxTag[18] = "-";

uint32_t janusBeaconEventSeq = 0;
uint32_t janusBeaconPolicySeq = 0;
uint32_t janusBeaconPolicyRx = 0;
uint32_t janusBeaconPolicyUntilMs = 0;
uint32_t janusBeaconQuietUntilMs = 0;
uint32_t janusBeaconLastPolicyMs = 0;
uint8_t janusBeaconMood = JM_IDLE;
uint8_t janusBeaconRadioRate = 1;
uint8_t janusBeaconSensorRate = 1;
uint8_t janusBeaconBuzzBudget = 1;
uint8_t janusBeaconPolicyConfidence = 0;
uint16_t janusBeaconDangerX100 = 0;
char janusBeaconOrder[40] = "-";

uint32_t janusBeaconLastEventMs = 0;
uint32_t janusBeaconLastEnvMs = 0;
uint32_t janusBeaconLastMemoryMs = 0;
uint32_t janusBeaconLastTaskMs = 0;
uint32_t janusBeaconLastSwarmSenseMs = 0;
uint32_t janusBeaconLastK2Ms = 0;
uint32_t janusBeaconLastTPMs = 0;
uint32_t janusBeaconLastBlackboxMs = 0;
uint32_t janusBeaconLastDiagMs = 0;
uint32_t janusBeaconLastWifiWeakMs = 0;
uint32_t janusBeaconLastLowHeapMs = 0;

uint32_t janusBeaconMemTx = 0;
uint32_t janusBeaconNeedTx = 0;
uint32_t janusBeaconDoneTx = 0;
uint32_t janusBeaconEnvTx = 0;
uint32_t janusBeaconSSTx = 0;
uint32_t janusBeaconSSFail = 0;
uint32_t janusBeaconK2Tx = 0;
uint32_t janusBeaconTPTx = 0;
uint32_t janusBeaconRemoteK2Rx = 0;
uint32_t janusBeaconRemoteTPRx = 0;
uint32_t janusBeaconRemoteSSRx = 0;
uint32_t janusBeaconBlackboxWrites = 0;
bool janusBeaconArchiveReadyAnnounced = false;
bool janusBeaconCoreStaleLatched = false;


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

void onJanusHeartbeat(const JanusColonyPacket& pkt);
void onJanusEntropy(const EntropyReport& er, const void* opt);
void onJanusEntropyV2(const EntropyReportV2& er2);
void sendNodeHeartbeat();
void sendNodeEntropy();
void sendDistributedAiPacket();
void onJanusAiPacket(const JanusAiNodePacket& ai, int8_t rxRssi);
void rememberSwarmAiNode(const char* nodeId, const char* role, float entropy, float predErr, float sync, float fit, float attention, uint8_t flags, uint32_t seq, uint16_t worker, int8_t rssi);
bool core2SeenRecently();
uint8_t onlineAiNodes();
float distributedAiEntropy();
void flushAiSummaryToSd();
void rotateAiLogIfNeeded(const char* path);
float beaconLocalEntropy();

bool forceColonyPeerRebuild(const char* reason);
esp_err_t janusBeaconEspNowSend(const char* tag, const void* payload, size_t len, bool repairOnFail=true);
void onJanusBeaconPolicyPacket(const JanusPolicyPacket& jp);
void onJanusBeaconEventPacket(const JanusEventPacket& je);
void onJanusBeaconKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi);
void onJanusBeaconTachyonPacket(const JanusTachyonProphecyPacket& tp, int8_t rxRssi);
void onJanusBeaconSwarmSensePacket(const SwarmSensePacket& ss, int8_t rxRssi);
void janusBeaconBlackboardTick();
void janusBeaconSwarmSenseTick(bool force=false);
void janusBeaconKenshiTick(bool force=false);
void janusBeaconTachyonTick(bool force=false);
void janusBeaconBlackboxTick(bool force=false);
bool janusBeaconEmitEvent(uint8_t eventType, const char* kind, uint8_t confidence, uint8_t urgency,
                          int16_t a_x10, int16_t b_x10, int16_t c_x10, int16_t d_x10,
                          uint16_t topicHash, uint16_t objectHash, uint32_t ttlMs);
void janusBeaconBootEvent();

void sendShareResponse(const RemoteJobState& job, uint32_t nonce) {
  ShareResponse sr{};
  sr.magic[0] = 'S'; sr.magic[1] = 'R';
  memcpy(sr.job_id, job.job_id, 8);
  sr.nonce = nonce;
  sr.worker_id = colonyWorkerId;
  if (janusBeaconEspNowSend("share", &sr, sizeof(sr), true) == ESP_OK) colonyRemoteShares++;
}

void runRemoteMiningBatch() {
  if (!colonyJob.active || millis() - colonyJob.receivedAt > COLONY_MASTER_TIMEOUT_MS) return;
  uint8_t hash[32];
  uint16_t weightedBatch = constrain((uint16_t)(COLONY_REMOTE_BATCH * colonyEntropyWeight), (uint16_t)80, (uint16_t)700);
  for (uint16_t i = 0; i < weightedBatch; i++) {
    if (colonyJob.nonce >= colonyJob.endNonce) { colonyJob.active = false; break; }
    uint32_t nonce = colonyJob.nonce++;
    writeLE32(colonyJob.header + 76, nonce);
    doubleSha256(colonyJob.header, 80, hash);
    colonyHashCounter++;
    uint16_t bits = countLeadingZeroBitsBE(hash);
    if (bits > colonyBestBits) colonyBestBits = bits;
    if (hashMeetsTargetBE(hash, colonyJob.target)) {
      sendShareResponse(colonyJob, nonce);
      colonyJob.active = false;
      break;
    }
  }
  uint32_t now = millis();
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
    Serial.printf("[COLONY/BEACON] peer ready ch=%u rebuilds=%lu reason=%s\n",
                  (unsigned)ch, (unsigned long)colonyPeerRebuilds, reason ? reason : "-");
    return true;
  }

  colonyPeerChannel = 0;
  Serial.printf("[COLONY/BEACON] peer rebuild FAIL err=%d ch=%u reason=%s\n",
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
  forceColonyPeerRebuild(exists ? "channel-change" : "ensure");
#endif
}

esp_err_t janusBeaconEspNowSend(const char* tag, const void* payload, size_t len, bool repairOnFail) {
#if JANUS_COLONY_ENABLE
  if (!payload || len == 0) return ESP_ERR_INVALID_ARG;

  if (WiFi.status() != WL_CONNECTED) {
    colonyTxFail++;
    colonyLastTxErr = ESP_ERR_INVALID_STATE;
    strlcpy(colonyLastTxTag, tag ? tag : "wifi-off", sizeof(colonyLastTxTag));
    return colonyLastTxErr;
  }

  ensureColonyPeer();
  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)payload, len);
  if (err == ESP_OK) {
    colonyTxOk++;
    return ESP_OK;
  }

  colonyTxFail++;
  colonyLastTxErr = err;
  strlcpy(colonyLastTxTag, tag ? tag : "send", sizeof(colonyLastTxTag));
  colonyPeerChannel = 0;
  Serial.printf("[COLONY/BEACON] TX FAIL tag=%s err=%d fail=%lu\n",
                colonyLastTxTag, (int)err, (unsigned long)colonyTxFail);
  if (repairOnFail) forceColonyPeerRebuild(tag ? tag : "tx-fail");
  return err;
#else
  (void)tag; (void)payload; (void)len; (void)repairOnFail;
  return ESP_ERR_INVALID_STATE;
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

  if (len == sizeof(JanusPolicyPacket) && data[0] == 'J' && data[1] == 'P') {
    JanusPolicyPacket jp{}; memcpy(&jp, data, sizeof(jp));
    onJanusBeaconPolicyPacket(jp);
    return;
  }
  if (len == sizeof(JanusEventPacket) && data[0] == 'J' && data[1] == 'E') {
    JanusEventPacket je{}; memcpy(&je, data, sizeof(je));
    onJanusBeaconEventPacket(je);
    return;
  }
  if (len == sizeof(JanusKenshiPacket) && data[0] == 'K' && data[1] == '2') {
    JanusKenshiPacket kp{}; memcpy(&kp, data, sizeof(kp));
    onJanusBeaconKenshiPacket(kp, colonyLastRssi);
    return;
  }
  if (len == sizeof(JanusTachyonProphecyPacket) && data[0] == 'T' && data[1] == 'P') {
    JanusTachyonProphecyPacket tp{}; memcpy(&tp, data, sizeof(tp));
    onJanusBeaconTachyonPacket(tp, colonyLastRssi);
    return;
  }
  if (len == sizeof(SwarmSensePacket) && data[0] == 'S' && data[1] == 'S') {
    SwarmSensePacket ss{}; memcpy(&ss, data, sizeof(ss));
    onJanusBeaconSwarmSensePacket(ss, colonyLastRssi);
    return;
  }

  if (len == sizeof(JanusColonyPacket)) {
    JanusColonyPacket pkt{}; memcpy(&pkt, data, sizeof(pkt));
    if (memcmp(pkt.magic, "JANUS", 5) == 0 && strncmp(pkt.role, "BuzzLighter", 11) == 0) {
      colonyMasterSeen = true; colonyLastMasterMs = millis(); strlcpy(colonyMode, "REMOTE", sizeof(colonyMode));
    }
    onJanusHeartbeat(pkt);
    return;
  }
  if (len == sizeof(JobPacket) && data[0] == 'J' && data[1] == 'B') {
    JobPacket jp{}; memcpy(&jp, data, sizeof(jp));
    memcpy(colonyJob.job_id, jp.job_id, 8); memcpy(colonyJob.header, jp.header, 80); memcpy(colonyJob.target, jp.target, 32);
    colonyJob.startNonce = jp.start_nonce; colonyJob.rangeSize = jp.range_size; colonyJob.nonce = jp.start_nonce; colonyJob.endNonce = jp.start_nonce + jp.range_size;
    colonyJob.receivedAt = millis(); colonyJob.active = true; strlcpy(colonyMode, "REMOTE", sizeof(colonyMode)); return;
  }
  if (len == sizeof(JanusAiNodePacket) && data[0] == 'A' && data[1] == 'I') { JanusAiNodePacket ai{}; memcpy(&ai, data, sizeof(ai)); onJanusAiPacket(ai, colonyLastRssi); return; }
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
  if (millis() - colonyLastMasterMs > COLONY_MASTER_TIMEOUT_MS) { colonyMasterSeen = false; if (!colonyJob.active) strlcpy(colonyMode, "LOCAL", sizeof(colonyMode)); }
  runRemoteMiningBatch();
  if (millis() - colonyLastHeartbeatMs >= COLONY_HEARTBEAT_MS) { colonyLastHeartbeatMs = millis(); sendNodeHeartbeat(); }
  if (millis() - colonyLastEntropyMs >= COLONY_ENTROPY_MS) { colonyLastEntropyMs = millis(); sendNodeEntropy(); }
}


#if __has_include(<RadioLib.h>)
#include <RadioLib.h>
#define HAS_RADIOLIB 1
#else
#define HAS_RADIOLIB 0
#endif

// ==========              ==========
#define DEVICE_ID              "janus_adv_beacon_eye_chain"
#define DEVICE_KIND            "adv_env3_beacon_sd_tachyon_home_ramanujan_beatmaker"
#define JANUS_BEACON_VERSION   "v4.5A BLACKBOX HOME CORTEX POLISH SD TACHYON RAMANUJAN BEATMAKER"

#define WIFI_SSID              "JANUS_WIFI_PLACEHOLDER"
#define WIFI_PASSWORD          "JANUS_NET_PLACEHOLDER"

const char* SERVER_CANDIDATES[] = { "http://192.168.1.92:5000" };
const int SERVER_COUNT = sizeof(SERVER_CANDIDATES) / sizeof(SERVER_CANDIDATES[0]);

#define EP_PING                "/ping"
#define EP_DEVICE_DATA         "/api/device/data"
#define EP_DEVICE_COMMAND      "/api/device/command/"
#define BLIND_EYE_LATEST_URL   "http://192.168.1.92:5000/api/device/latest/atom_s3r_blind_eye"

#define GROVE_SDA_PIN          2
#define GROVE_SCL_PIN          1
#define SHT30_ADDR             0x44

#define LED_PIN                21
#define NUM_LEDS               1
CRGB leds[NUM_LEDS];

#define SENSOR_INTERVAL_MS     220
#define SEND_INTERVAL_MS       2500
#define COMMAND_INTERVAL_MS    5000
#define DRAW_INTERVAL_MS       55
#define WIFI_RETRY_MS          6000
#define QUEUE_FLUSH_MS         3500
#define AUTOSAVE_MS            120000UL   // v4.4: no LittleFS/SD hammering
#define EYE_PULL_MS            2200
#define LORA_BROADCAST_MS      12000UL
#define JANUS_HTTP_LEGACY_ENABLE 0
#define COLONY_SD_LOG_ENABLE 1
#define JANUS_AI_SD_ENABLE 1
#define JANUS_AI_SD_DIR "/janus_ai"
#define JANUS_AI_EVENT_LOG "/janus_ai/events.jsonl"
#define JANUS_AI_NODE_LOG "/janus_ai/nodes.jsonl"
#define JANUS_AI_POLICY_DIR "/janus_ai/policies"
#define JANUS_AI_POLICY_FILE "/janus_ai/policies/current.json"
#define JANUS_AI_MANIFEST_FILE "/janus_ai/beacon_manifest.json"
#define JANUS_AI_LOG_FLUSH_MS 15000UL
#define JANUS_AI_MAX_LOG_BYTES 2097152UL
#define JANUS_AI_MAX_TOTAL_BYTES (1024UL * 1024UL * 1024UL)
#define COLONY_SD_DIR "/janus_colony"
#define COLONY_SD_LOG_FILE "/janus_colony/current.jsonl"
#define COLONY_SD_ARCH_PREFIX "/janus_colony/log_"
#define COLONY_SD_MAX_CURRENT_BYTES 1048576UL
#define COLONY_SD_MAX_TOTAL_BYTES (1024UL * 1024UL * 1024UL)
#define COLONY_SD_CLEANUP_MS 300000UL
#define COLONY_SD_ARCHIVE_KEEP_MAX 256
#define CARDPUTER_ADV_SD_CS_PIN 4
#define CARDPUTER_SD_FALLBACK_CS_PIN 12   // some Cardputer builds expose microSD CS here

// LittleFS is now only a tiny fallback. The main Janus memory lives on SD.
#define QUEUE_FILE             "/tx_queue_adv.txt"
#define STATE_FILE             "/janus_state.json"
#define MODEL_FILE             "/tachyon_lite_v43c.bin"
#define WITNESS_FILE           "/witness_log.jsonl"
#define SD_WITNESS_FILE        "/janus_brain/witness_log.jsonl"
#define MAX_QUEUE_LINES        24

#define JANUS_BRAIN_SD_DIR             "/janus_brain"
#define JANUS_TACHYON_SD_DIR           "/janus_brain/tachyon"
#define JANUS_STATE_SD_FILE            "/janus_brain/state.json"
#define JANUS_MODEL_SD_FILE            "/janus_brain/tachyon/tachyon_lite_v44.bin"
#define JANUS_MEMORY_SD_FILE           "/janus_brain/tachyon/memory.dat"
#define JANUS_PHRASE_SD_FILE           "/janus_brain/tachyon/phrases_v2.dat"
#define JANUS_CHAIN_SD_FILE            "/janus_brain/tachyon/chains_v1.dat"
#define JANUS_THETA_SD_FILE            "/janus_brain/tachyon/theta_v44.dat"
#define JANUS_QUEUE_SD_FILE            "/janus_brain/tx_queue_adv.txt"
#define JANUS_TACHYON_TRAIN_LOG        "/janus_brain/tachyon/train_current.jsonl"
#define JANUS_TACHYON_TRAIN_PREFIX     "/janus_brain/tachyon/train_"
#define JANUS_SD_BRAIN_BUDGET_BYTES    (15ULL * 1024ULL * 1024ULL * 1024ULL)
#define JANUS_TACHYON_LOG_MAX_BYTES    (8UL * 1024UL * 1024UL)
#define JANUS_TACHYON_SAMPLE_MS        15000UL
#define JANUS_BRAIN_SD_CLEANUP_MS      300000UL

#define HIST_SIZE              64
#define FEATURE_DIM            12   // v4.3C: 8 old + 4 theta/SHA-home features
#define MAX_MEMORY_ENTRIES     32
#define MEMORY_FILE            "/memory.dat"

#define MAX_PHRASES           24
#define PHRASE_FILE           "/phrases_v2.dat"
#define PHRASE_MAGIC          0x4A4E5553  // "JNUS"
#define MAX_CHAINS            24
#define CHAIN_FILE            "/chains_v1.dat"
#define CHAIN_MAGIC           0x43484E31UL

// ========================= RAMANUJAN / THETA NOTEBOOK =========================
#define JANUS_THETA_ENABLE        1
#define JANUS_THETA_DEPTH         12
#define JANUS_THETA_STUDY_MS      5000UL
#define JANUS_THETA_MIN_GAP_MS    450UL
#define JANUS_THETA_SERIAL_MS     8000UL   // v4.5A: keep Serial readable while theta still learns internally
#define JANUS_THETA_FILE          "/theta_v43c.dat"   // LittleFS fallback only
#define JANUS_THETA_MAGIC         0x54483343UL  // TH3C
#define JANUS_BRAINWAVE_MEM_SLOTS 16
#define JANUS_SHA_HOME_ENABLE     1

#define COLOR_AMBER            0xFD20
#define COLOR_AMBER_DIM        0xB3C0

#if HAS_RADIOLIB
#define LORA_NSS               5
#define LORA_SCK               40
#define LORA_MOSI              14
#define LORA_MISO              39
#define LORA_BUSY              6
#define LORA_IRQ               4
#define LORA_RST               3
#define LORA_PWR_EN            10
SX1262 radio = new Module(LORA_NSS, LORA_IRQ, LORA_RST, LORA_BUSY);
#endif

// ==========                  ==========
struct Telemetry {
  float temp_c=0, humidity=0, pressure_hpa=0, pred_pressure_hpa=0, pressure_loss=0, entropy=0.20f, m2r=0.10f, loss=0, mi=0, fit=0, fit_best=-9999;
  float f1=432.0f, f2=439.83f, pred_temp=0, pred_humidity=0, pred_entropy=0.20f, pred_m2r=0.10f, pred_fit=0, pred_f1=432.0f, pred_f2=439.83f;
  float z_entropy=0, z_loss=0, z_fit=0;
  int battery=0, wifi_rssi=-127;
  bool sd_ready=false;
  bool imu_ready=false;
  bool qmp_ready=false;
  float imu_ax=0, imu_ay=0, imu_az=0;
  float imu_gx=0, imu_gy=0, imu_gz=0;
  float imu_shock=0, pred_imu_shock=1.0f, imu_loss=0;
} g;

struct BlindEyeState {
  bool online=false;
  unsigned long last_ok_ms=0;
  float tmos_presence=-1, tmos_motion=-1, mic_rms=0, mag_norm=0, mag_norm_smooth=0, shock=0;
  float acc_x=0, acc_y=0, acc_z=0;
  float gyro_x=0, gyro_y=0, gyro_z=0;
  float mag_x=0, mag_y=0, mag_z=0;
  float imu_temp=0;
  float activity=0, pred_activity=0, loss=0, sync=0, model_lr=0;
  String diag="";
  String status="";
} eye;

struct AudioNodeState {
  bool online=false;
  unsigned long last_ok_ms=0;
  float mic_rms=0;
  float entropy=0;
  float loss=0;
  float pressure_hpa=0;
  float temp_c=0;
  float sync=0;
} audioNode;

float eyeTmosPresenceUi = 0.0f;
float eyeTmosMotionUi = 0.0f;
float eyeTmosPresenceRaw = 0.0f;
float eyeTmosMotionRaw = 0.0f;

struct MemoryEntry {
  float entropy;
  float fit;
  float eye_sync;
  float tmos;
};

struct PhraseMemory {
  char text[24];
  float avg_fit;
  float avg_sync;
  float avg_entropy;
  float usage_count;
  unsigned long last_used;
};

struct ThoughtChain {
  char first[24];
  char second[24];
  float strength;
  float avg_entropy;
  float avg_sync;
  uint16_t usage_count;
};

// ==========                       ==========
int active_server_index = -1;
bool houseProtocolActive = false;
bool lightClockMode = false;
bool isSpeakerPlaying = false;
bool ledEnabled = true;
bool loveExperimentActive = false;
bool anomalyLatched = false;

int brightness = 128;
int volume = 128;

float base_f1 = 432.0f;
float schumann_offset = 7.83f;
float pulse_rate = 1.618f;
float gamma_factor = 1.0f;
unsigned long lightTicks = 0, lastTick = 0;
uint32_t anomalyCount = 0, witnessCount = 0;

float spike_prob = 0.0006f;
float collapse_prob = 0.0002f;
int decay_steps = 5, current_decay = 0;

float zThresholdEntropy = 4.6f;
float zThresholdLoss = 4.0f;
float zThresholdFit = 4.4f;
float disagreementThreshold = 2.25f;
float eyeLossGoodThreshold = 0.025f;

float model_w[FEATURE_DIM] = {0.08f, -0.02f, 0.07f, 0.13f, 0.06f, 0.05f, 0.09f, -0.02f, 0.05f, 0.04f, 0.03f, 0.06f};
float model_b = 0.0f;
float model_lr = 0.0015f;

// Tachyon v2: stable self-supervised predictor state.
// Goal: surprise should mean real sensor/model mismatch, not random oscillator noise.
float tachyonPredEntropyEma = 0.20f;
float tachyonLossEma = 0.0f;
float tachyonMIEma = 0.5f;
float tachyonF1Ema = 432.0f;
float tachyonF2Ema = 439.83f;
float tachyonTrendEntropy = 0.0f;
float tachyonTrendF1 = 0.0f;
float tachyonLastEntropy = 0.20f;

float tachyonLastF1 = 432.0f;

// Tachyon v3: sequence memory / future prediction / swarm attention.
constexpr uint8_t TACHYON_SEQ_N = 16;
float hist_pred_entropy_seq[TACHYON_SEQ_N] = {0};
float hist_real_entropy_seq[TACHYON_SEQ_N] = {0};
float hist_audio_entropy_seq[TACHYON_SEQ_N] = {0};
float hist_eye_entropy_seq[TACHYON_SEQ_N] = {0};
float hist_master_entropy_seq[TACHYON_SEQ_N] = {0};
uint8_t tachyonSeqPos = 0;
uint8_t tachyonSeqCount = 0;

float seqPredW[8] = {0.30f, 0.22f, 0.16f, 0.10f, 0.08f, 0.06f, 0.05f, 0.03f};
float seqPredBias = 0.0f;
float seqPredLR = 0.0035f;

float tachyonFuture1 = 0.20f;
float tachyonFuture2 = 0.20f;
float tachyonFuture3 = 0.20f;
float tachyonFutureTrend = 0.0f;
float tachyonFutureConfidence = 0.0f;

float swarmEntropyWeighted = 0.20f;
float swarmAttentionSum = 0.0f;


float hist_entropy[HIST_SIZE] = {0}, hist_loss[HIST_SIZE] = {0}, hist_fit[HIST_SIZE] = {0};
int hist_count = 0, hist_pos = 0;

unsigned long lastSensorAt=0, lastSendAt=0, lastCmdAt=0, lastDrawAt=0, lastWifiTryAt=0, lastQueueFlushAt=0, lastSaveAt=0, lastLoraAt=0, lastEyePullAt=0, lastBrainNoteAt=0, lastTachyonSampleAt=0, lastBrainSdCleanupAt=0;

MemoryEntry memoryEntries[MAX_MEMORY_ENTRIES];
int memoryCount = 0;

PhraseMemory phraseMemory[MAX_PHRASES];
int phraseCount = 0;
ThoughtChain thoughtChains[MAX_CHAINS];
int chainCount = 0;
String lastThought = "";
String currentThought = "";

uint8_t brainStep = 0;
uint8_t keygenStep = 0;
uint8_t keygenPattern = 0;
uint32_t keygenNoteOffAt = 0;
float swarmMusicMood = 0.0f;
float swarmMusicEnergy = 0.0f;
float swarmMusicTension = 0.0f;
float swarmMusicMic = 0.0f;
float swarmAttentionEye = 0.0f;
float swarmAttentionAudio = 0.0f;
float swarmAttentionMaster = 0.0f;
float swarmSequenceTrend = 0.0f;
float swarmSeqEntropyPrev = 0.0f;
float swarmSeqLossPrev = 0.0f;


struct JanusThetaState {
  uint32_t magic = JANUS_THETA_MAGIC;
  uint32_t lastStudyMs = 0;
  uint32_t studies = 0;
  float q = 0.08f;
  float theta2 = 0.0f;
  float theta3 = 1.0f;
  float theta4 = 1.0f;
  float mock = 0.0f;
  float partitionSignal = 0.0f;
  uint32_t tauLike = 0;
  uint8_t resonance = 0;
  uint8_t confidence = 0;
  char lemma[64] = "Theta sleep";
};
JanusThetaState thetaState;
uint32_t janusThetaLastSerialMs = 0;

struct JanusMusicPhraseSlot {
  int8_t semi = 0;
  uint16_t freq = 0;
  float score = 0.0f;
  uint8_t hits = 0;
};
JanusMusicPhraseSlot musicPhrases[JANUS_BRAINWAVE_MEM_SLOTS];
uint8_t janusMusicMemHead = 0;
uint32_t janusBeatNextAt = 0;
uint16_t janusArpFreqs[3] = {0,0,0};
uint8_t janusArpCount = 0;
uint8_t janusArpPos = 0;
uint8_t janusArpPeakVol = 0;
bool janusToneFading = false;
uint32_t janusFadeNextAt = 0;
uint8_t janusFadeStep = 0;
uint8_t janusFadePeakVol = 0;
float janusMusicScore[4] = {0.15f, 0.10f, 0.12f, 0.08f};
float janusMusicLastFit = 0.0f;
uint32_t janusMusicLastLearnMs = 0;
String statusLine = "boot", codeBuffer = "";
String eyeDebugLine = "eye init";
unsigned long lastEyeGoodAt = 0;

uint32_t masterHashRate = 0, masterShares = 0, masterRejects = 0, masterBestBits = 0;
float masterDiff = 0.0f;
uint32_t lastMasterSeenMs = 0;
uint32_t colonyRxPackets = 0;
uint32_t lastSdCleanupAt = 0;
bool janusPhraseDirty = false;
bool janusChainsDirty = false;
bool janusMemoryDirty = false;
uint8_t janusSdCsUsed = CARDPUTER_ADV_SD_CS_PIN;
M5Canvas beaconCanvas(&M5Cardputer.Display);
bool beaconCanvasReady = false;

constexpr uint8_t JANUS_AI_MAX_NODES = 18;
struct SwarmAiNodeState {
  bool active=false;
  char nodeId[24]="";
  char role[16]="";
  uint32_t lastSeenMs=0;
  uint32_t seq=0;
  float entropy=0.0f;
  float prediction_error=0.0f;
  float sync=0.0f;
  float fit=0.0f;
  float attention=0.0f;
  int8_t rssi=0;
  uint8_t flags=0;
  uint16_t worker=0;
};
SwarmAiNodeState swarmAiNodes[JANUS_AI_MAX_NODES];
uint8_t swarmAiNodeCount = 0;
uint32_t swarmAiSeq = 0;
uint32_t lastAiPacketMs = 0;
uint32_t lastAiSdFlushMs = 0;
char aiStatusLine[48] = "AI NET BOOT";


// ==========           ==========
void saveMemoryEntry(float entropy, float fit, float eye_sync, float tmos);
float memoryBias();
void saveMemoryState();
void loadMemoryState();
void updateThetaState(float currentEntropy, bool significantEvent);
float thetaFuture(float base, int steps);
void applyThetaToColonyWeights();
void saveThetaState();
void loadThetaState();
bool initJanusSdCard();
void janusEnsureSdDirs();
void janusMigrateLittleFsToSd();
void janusFreeLittleFsLegacy();
void janusCleanupStorageTick();
void janusLogTachyonSample();
float janusSafeLoss(float v);
float janusSafeEntropy(float v);
void janusParadoxSelfStudyTick();
void drawOsc(int x, int y, int w, int h);
void savePhrases();
void loadPhrases();
void initDefaultPhrases();
String selectOrCreatePhrase();
void saveChains();
void loadChains();
void rememberThoughtTransition(const String& first, const String& second);
String applyThoughtChainBias(const String& candidate);
String buildSeedPhrase(float adjustedFit, float currentSync, float currentEntropy);

// ========================= JANUS BEACON HOME CORTEX BLACKBOX v4.5 =========================
uint16_t janusHash16(const char* s) {
  uint16_t h = 21661U;
  if (!s) return h;
  while (*s) {
    h ^= (uint8_t)*s++;
    h = (uint16_t)(h * 16719U);
  }
  return h ? h : 1;
}

uint16_t janusBeaconCapabilities() {
  uint16_t caps = JC_TEMP | JC_HUM | JC_PRESS | JC_IMU | JC_HASH | JC_MEMORY | JC_AI | JC_BATTERY | JC_RF;
  if (g.sd_ready) caps |= JC_RELAY;
  return caps;
}

const char* janusEventName(uint8_t eventType) {
  switch (eventType) {
    case JE_BOOT: return "boot";
    case JE_HEARTBEAT: return "heartbeat";
    case JE_ENV: return "env";
    case JE_WIFI_WEAK: return "wifi_weak";
    case JE_LOW_HEAP: return "low_heap";
    case JE_HASH: return "hash";
    case JE_TASK_NEED: return "task_need";
    case JE_TASK_DONE: return "task_done";
    case JE_DANGER: return "danger";
    case JE_SAFE: return "safe";
    case JE_POLICY: return "policy";
    case JE_AI_MEMORY: return "ai_memory";
    default: return "event";
  }
}

void janusBeaconSdAppend(const char* path, const String& line) {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready || !path || !path[0]) return;
  if (!SD.exists(JANUS_AI_SD_DIR)) SD.mkdir(JANUS_AI_SD_DIR);
  File f = SD.open(path, FILE_APPEND);
  if (!f) return;
  f.println(line);
  f.close();
#endif
}

void janusBeaconArchiveEvent(const JanusEventPacket& ev, const char* note) {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return;
  StaticJsonDocument<384> doc;
  doc["ts_ms"] = millis();
  doc["event"] = janusEventName(ev.eventType);
  doc["kind"] = ev.kind;
  doc["node"] = ev.nodeId;
  doc["seq"] = ev.seq;
  doc["conf"] = ev.confidence;
  doc["urg"] = ev.urgency;
  doc["topic"] = ev.topicHash;
  doc["object"] = ev.objectHash;
  doc["a"] = ev.valueA_x10;
  doc["b"] = ev.valueB_x10;
  doc["c"] = ev.valueC_x10;
  doc["d"] = ev.valueD_x10;
  doc["note"] = note ? note : "";
  doc["policy_rx"] = janusBeaconPolicyRx;
  doc["core_stale"] = janusBeaconCoreStaleLatched;
  String out; serializeJson(doc, out);
  janusBeaconSdAppend(JANUS_BEACON_EVENT_BLACKBOX_LOG, out);
  rotateAiLogIfNeeded(JANUS_BEACON_EVENT_BLACKBOX_LOG);
#endif
}

bool janusBeaconEmitEvent(uint8_t eventType, const char* kind, uint8_t confidence, uint8_t urgency,
                          int16_t a_x10, int16_t b_x10, int16_t c_x10, int16_t d_x10,
                          uint16_t topicHash, uint16_t objectHash, uint32_t ttlMs) {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  JanusEventPacket ev{};
  ev.magic[0] = 'J'; ev.magic[1] = 'E';
  ev.version = 1;
  ev.eventType = eventType;
  ev.nodeRole = JR_BEACON;
  ev.confidence = constrain((int)confidence, 0, 100);
  ev.urgency = constrain((int)urgency, 0, 100);
  strlcpy(ev.nodeId, "BeaconADV", sizeof(ev.nodeId));
  strlcpy(ev.kind, kind && kind[0] ? kind : "beacon_blackbox", sizeof(ev.kind));
  ev.seq = ++janusBeaconEventSeq;
  ev.uptimeMs = millis();
  ev.topicHash = topicHash ? topicHash : janusHash16("beacon");
  ev.objectHash = objectHash;
  ev.capabilities = janusBeaconCapabilities();
  ev.valueA_x10 = a_x10;
  ev.valueB_x10 = b_x10;
  ev.valueC_x10 = c_x10;
  ev.valueD_x10 = d_x10;
  ev.eventHash = ((uint32_t)eventType << 24) ^ ((uint32_t)ev.topicHash << 8) ^ ev.seq ^ (uint32_t)colonyWorkerId;
  ev.ttlMs = ttlMs ? ttlMs : 12000UL;
  bool ok = janusBeaconEspNowSend("J/E", &ev, sizeof(ev), true) == ESP_OK;
  if (ok) {
    janusBeaconLastEventMs = millis();
    if (eventType == JE_AI_MEMORY) janusBeaconMemTx++;
    else if (eventType == JE_TASK_NEED) janusBeaconNeedTx++;
    else if (eventType == JE_TASK_DONE) janusBeaconDoneTx++;
    else if (eventType == JE_ENV) janusBeaconEnvTx++;
  }
  janusBeaconArchiveEvent(ev, ok ? "tx_ok" : "tx_fail");
  return ok;
#else
  (void)eventType; (void)kind; (void)confidence; (void)urgency; (void)a_x10; (void)b_x10; (void)c_x10; (void)d_x10; (void)topicHash; (void)objectHash; (void)ttlMs;
  return false;
#endif
}

void janusBeaconSavePolicyToSd(const JanusPolicyPacket& jp) {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return;
  if (!SD.exists(JANUS_AI_POLICY_DIR)) SD.mkdir(JANUS_AI_POLICY_DIR);
  StaticJsonDocument<512> doc;
  doc["ts_ms"] = millis();
  doc["seq"] = jp.seq;
  doc["mood"] = jp.swarmMood;
  doc["radio"] = jp.radioRate;
  doc["sensor"] = jp.sensorRate;
  doc["buzz"] = jp.buzzBudget;
  doc["conf"] = jp.confidence;
  doc["flags"] = jp.flags;
  doc["ttl"] = jp.ttlMs;
  doc["quiet"] = jp.quietUntilMs;
  doc["topic"] = jp.dominantTopic;
  doc["danger"] = jp.danger_x100;
  doc["order"] = jp.order;
  doc["node"] = "BeaconADV";
  File f = SD.open(JANUS_AI_POLICY_FILE, FILE_WRITE);
  if (!f) return;
  serializeJson(doc, f);
  f.println();
  f.close();
#endif
}

void onJanusBeaconPolicyPacket(const JanusPolicyPacket& jp) {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  if (jp.magic[0] != 'J' || jp.magic[1] != 'P' || jp.version != 1) return;
  if (jp.seq && jp.seq == janusBeaconPolicySeq) return;

  janusBeaconPolicyRx++;
  janusBeaconPolicySeq = jp.seq;
  janusBeaconLastPolicyMs = millis();
  janusBeaconMood = jp.swarmMood;
  janusBeaconRadioRate = constrain((int)jp.radioRate, 0, 2);
  janusBeaconSensorRate = constrain((int)jp.sensorRate, 0, 2);
  janusBeaconBuzzBudget = constrain((int)jp.buzzBudget, 0, 3);
  janusBeaconPolicyConfidence = constrain((int)jp.confidence, 0, 100);
  janusBeaconDangerX100 = jp.danger_x100;
  janusBeaconPolicyUntilMs = millis() + (jp.ttlMs ? jp.ttlMs : JANUS_BEACON_POLICY_TTL_GUARD_MS);
  janusBeaconQuietUntilMs = jp.quietUntilMs ? millis() + min((uint32_t)jp.quietUntilMs, 60000UL) : 0;
  strlcpy(janusBeaconOrder, jp.order[0] ? jp.order : "-", sizeof(janusBeaconOrder));
  janusBeaconCoreStaleLatched = false;

  janusBeaconSavePolicyToSd(jp);
  Serial.printf("[BLACKBOARD/BEACON] policy rx=%lu mood=%u radio=%u sensor=%u conf=%u danger=%.2f order=%s\n",
                (unsigned long)janusBeaconPolicyRx,
                (unsigned)janusBeaconMood,
                (unsigned)janusBeaconRadioRate,
                (unsigned)janusBeaconSensorRate,
                (unsigned)janusBeaconPolicyConfidence,
                (float)janusBeaconDangerX100 / 100.0f,
                janusBeaconOrder);

  janusBeaconEmitEvent(JE_POLICY, "policy_rx", janusBeaconPolicyConfidence, 20,
                       (int16_t)janusBeaconMood,
                       (int16_t)janusBeaconRadioRate,
                       (int16_t)janusBeaconSensorRate,
                       (int16_t)janusBeaconDangerX100,
                       janusHash16("policy"), janusHash16("core_policy"), 10000UL);
#endif
}

void onJanusBeaconEventPacket(const JanusEventPacket& je) {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  if (je.magic[0] != 'J' || je.magic[1] != 'E' || je.version != 1) return;
  if (strncmp(je.nodeId, "Beacon", 6) == 0) return;
  rememberSwarmAiNode(je.nodeId, je.kind,
                      constrain((float)je.urgency / 10.0f, 0.0f, 10.0f),
                      0.0f,
                      constrain((float)je.confidence / 100.0f, 0.0f, 1.0f),
                      constrain((float)je.valueA_x10 / 1000.0f, 0.0f, 3.0f),
                      constrain((float)je.urgency / 100.0f, 0.0f, 1.5f),
                      je.eventType, je.seq, 0, colonyLastRssi);
  if (g.sd_ready && (je.eventType == JE_DANGER || je.eventType == JE_TASK_NEED || je.eventType == JE_AI_MEMORY)) {
    janusBeaconArchiveEvent(je, "rx");
  }
#endif
}

void onJanusBeaconKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi) {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  if (kp.magic[0] != 'K' || kp.magic[1] != '2' || kp.version != 1) return;
  if (strncmp(kp.nodeId, "Beacon", 6) == 0) return;
  janusBeaconRemoteK2Rx++;
  rememberSwarmAiNode(kp.nodeId, "KenshiK2", kp.entropy, 0.0f, kp.confidence, kp.activity,
                      constrain((float)kp.priority / 255.0f, 0.0f, 1.5f), kp.flags, kp.seq, kp.worker_id, rxRssi);
#endif
}

void onJanusBeaconTachyonPacket(const JanusTachyonProphecyPacket& tp, int8_t rxRssi) {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  if (tp.magic[0] != 'T' || tp.magic[1] != 'P' || tp.version != 1) return;
  if (strncmp(tp.nodeId, "Beacon", 6) == 0) return;
  janusBeaconRemoteTPRx++;
  float entropy = constrain((tp.future_stress + tp.swarm_pressure) * 1.5f, 0.0f, 10.0f);
  rememberSwarmAiNode(tp.nodeId, "TachyonTP", entropy, tp.future_stress,
                      constrain((float)tp.confidence / 100.0f, 0.0f, 1.0f),
                      constrain(tp.pred_presence_1 / 1000.0f, 0.0f, 3.0f),
                      constrain(tp.swarm_pressure, 0.0f, 1.5f), tp.flags, tp.seq, tp.worker_id, rxRssi);
#endif
}

void onJanusBeaconSwarmSensePacket(const SwarmSensePacket& ss, int8_t rxRssi) {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  if (ss.magic[0] != 'S' || ss.magic[1] != 'S' || ss.version != 1) return;
  if (strncmp(ss.nodeId, "Beacon", 6) == 0) return;
  janusBeaconRemoteSSRx++;
  rememberSwarmAiNode(ss.nodeId, ss.kind,
                      (float)ss.entropy_x1000 / 1000.0f,
                      fabsf((float)ss.prediction_error_x1000) / 1000.0f,
                      constrain((float)ss.knn_confidence / 100.0f, 0.0f, 1.0f),
                      constrain((float)ss.hash_eff_x1000 / 1000.0f, 0.0f, 3.0f),
                      constrain((float)ss.ai_hint / 4.0f, 0.0f, 1.0f),
                      ss.flags, ss.seq, ss.worker_id, rxRssi);
#endif
}

uint32_t janusBeaconEventIntervalNow() {
  if (janusBeaconRadioRate == 0 || (janusBeaconQuietUntilMs && millis() < janusBeaconQuietUntilMs)) return JANUS_BEACON_EVENT_BASE_MS * 2UL;
  if (janusBeaconRadioRate == 2 || janusBeaconMood == JM_ALERT || janusBeaconMood == JM_GUARD) return JANUS_BEACON_EVENT_FAST_MS;
  return JANUS_BEACON_EVENT_BASE_MS;
}

void janusBeaconSwarmSenseTick(bool force) {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  uint32_t now = millis();
  if (!force && now - janusBeaconLastSwarmSenseMs < JANUS_BEACON_SWARMSENSE_MS) return;
  janusBeaconLastSwarmSenseMs = now;

  SwarmSensePacket ss{};
  ss.magic[0] = 'S'; ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = colonyWorkerId;
  strlcpy(ss.nodeId, "BeaconADV", sizeof(ss.nodeId));
  strlcpy(ss.kind, "blackbox_archive", sizeof(ss.kind));
  ss.seq = ++swarmAiSeq;
  ss.uptime_ms = now;
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = 0;
  ss.loop_max_us = DRAW_INTERVAL_MS;
  ss.rssi = (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : -127;
  ss.radio_mode = janusBeaconRadioRate;
  ss.bt_flags = 0;
  if (g.sd_ready) ss.bt_flags |= 0x01;
  if (core2SeenRecently() || (janusBeaconPolicyRx && now - janusBeaconLastPolicyMs < JANUS_BEACON_CORE_STALE_MS)) ss.bt_flags |= 0x02;
  if (janusBeaconCoreStaleLatched) ss.bt_flags |= 0x04;
  if (g.qmp_ready) ss.bt_flags |= 0x08;
  ss.palette = brightness;
  ss.knn_label = (uint8_t)constrain((int)(g.entropy), 0, 15);
  ss.knn_confidence = (uint8_t)constrain((int)(g.fit * 35.0f + eye.sync * 40.0f), 0, 100);
  ss.ai_hint = (uint8_t)((g.loss > 0.30f || janusBeaconCoreStaleLatched) ? 2 : ((g.fit > 1.35f) ? 3 : 1));
  ss.thermal_load = (uint8_t)constrain((int)g.temp_c, 0, 100);
  ss.effective_batch = constrain((uint16_t)(COLONY_REMOTE_BATCH * colonyEntropyWeight), (uint16_t)80, (uint16_t)700);
  ss.dynamic_batch = ss.effective_batch;
  ss.hash_rate = colonyRemoteHashrate;
  ss.total_hashes = colonyHashCounter;
  ss.best_bits = colonyBestBits;
  ss.hash_eff_x1000 = (uint16_t)constrain((int32_t)(colonyRemoteHashrate / 10UL), 0L, 65535L);
  ss.prediction_error_x1000 = (int16_t)constrain((int32_t)((g.loss + g.imu_loss) * 1000.0f), -32768L, 32767L);
  ss.entropy_x1000 = (uint16_t)constrain((int32_t)(beaconLocalEntropy() * 1000.0f), 0L, 65535L);
  ss.touch_delta = 0;
  ss.job_age_s = colonyJob.active ? (uint16_t)min(65535UL, (now - colonyJob.receivedAt) / 1000UL) : 65535U;
  ss.nonce_remaining_l16 = colonyJob.active ? (uint16_t)((colonyJob.endNonce > colonyJob.nonce) ? ((colonyJob.endNonce - colonyJob.nonce) & 0xFFFF) : 0) : 0;
  ss.flags = ((uint16_t)onlineAiNodes() << 8) | (g.sd_ready ? 0x01 : 0x00) | (janusBeaconCoreStaleLatched ? 0x02 : 0x00) | (anomalyLatched ? 0x04 : 0x00);

  if (janusBeaconEspNowSend("S/S", &ss, sizeof(ss), true) == ESP_OK) janusBeaconSSTx++;
  else janusBeaconSSFail++;
#endif
}

void janusBeaconKenshiTick(bool force) {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  uint32_t now = millis();
  if (!force && now - janusBeaconLastK2Ms < JANUS_BEACON_K2_MS) return;
  janusBeaconLastK2Ms = now;

  JanusKenshiPacket kp{};
  kp.magic[0] = 'K'; kp.magic[1] = '2';
  kp.version = 1;
  kp.flags = 0x04; // virtual summary / archive witness
  if (g.sd_ready) kp.flags |= 0x08;
  if (anomalyLatched || janusBeaconCoreStaleLatched) kp.flags |= 0x02;
  strlcpy(kp.nodeId, "BeaconADV", sizeof(kp.nodeId));
  kp.seq = ++swarmAiSeq;
  kp.worker_id = colonyWorkerId;
  kp.uptime_ms = now;
  kp.activeBubbleNodes = onlineAiNodes();
  kp.virtualNodes = swarmAiNodeCount > kp.activeBubbleNodes ? swarmAiNodeCount - kp.activeBubbleNodes : 0;
  kp.worldFlags = 0;
  if (g.sd_ready) kp.worldFlags |= 1UL << 0;
  if (core2SeenRecently()) kp.worldFlags |= 1UL << 1;
  if (eye.online) kp.worldFlags |= 1UL << 2;
  if (audioNode.online) kp.worldFlags |= 1UL << 3;
  if (g.qmp_ready) kp.worldFlags |= 1UL << 4;
  if (anomalyLatched) kp.worldFlags |= 1UL << 5;
  if (janusBeaconCoreStaleLatched) kp.worldFlags |= 1UL << 6;
  kp.sector = (uint8_t)constrain((int)floorf(g.entropy), 0, 7);
  kp.predictedSector = (uint8_t)constrain((int)floorf(tachyonFuture2), 0, 7);
  kp.jobState = janusBeaconCoreStaleLatched ? 5 : (g.sd_ready ? 4 : 1);
  kp.priority = (uint8_t)constrain((int)(g.entropy * 18.0f + onlineAiNodes() * 8 + (janusBeaconCoreStaleLatched ? 80 : 0)), 0, 255);
  kp.rssi = g.wifi_rssi;
  kp.entropy = beaconLocalEntropy();
  kp.activity = g.fit;
  kp.confidence = constrain(0.25f + eye.sync * 0.35f + (g.sd_ready ? 0.25f : 0.0f) + (core2SeenRecently() ? 0.15f : 0.0f), 0.0f, 1.5f);
  kp.values[0] = g.temp_c;
  kp.values[1] = g.humidity;
  kp.values[2] = g.pressure_hpa;
  kp.values[3] = distributedAiEntropy();
  kp.values[4] = (float)onlineAiNodes();
  kp.values[5] = (float)ESP.getFreeHeap();

  if (janusBeaconEspNowSend("K2", &kp, sizeof(kp), true) == ESP_OK) janusBeaconK2Tx++;
#endif
}

void janusBeaconTachyonTick(bool force) {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  uint32_t now = millis();
  if (!force && now - janusBeaconLastTPMs < JANUS_BEACON_TP_MS) return;
  janusBeaconLastTPMs = now;

  JanusTachyonProphecyPacket tp{};
  tp.magic[0] = 'T'; tp.magic[1] = 'P';
  tp.version = 1;
  tp.flags = 0x08; // remote-assisted archive prophecy
  if (anomalyLatched || janusBeaconCoreStaleLatched) tp.flags |= 0x04;
  strlcpy(tp.nodeId, "BeaconADV", sizeof(tp.nodeId));
  tp.seq = ++swarmAiSeq;
  tp.worker_id = colonyWorkerId;
  tp.uptime_ms = now;
  tp.horizon_ms = 30000;
  tp.sector = (uint8_t)constrain((int)floorf(g.entropy), 0, 7);
  tp.predictedSector = (uint8_t)constrain((int)floorf(tachyonFuture3), 0, 7);
  tp.confidence = (uint8_t)constrain((int)(tachyonFutureConfidence * 100.0f + (g.sd_ready ? 15.0f : 0.0f)), 0, 100);
  tp.jobState = janusBeaconCoreStaleLatched ? 5 : 4;
  tp.presence_now = eye.online ? eye.tmos_presence : 0.0f;
  tp.motion_now = eye.online ? eye.tmos_motion : 0.0f;
  tp.pred_presence_1 = tachyonFuture1;
  tp.pred_motion_1 = swarmEntropyWeighted;
  tp.pred_presence_2 = tachyonFuture2;
  tp.pred_motion_2 = distributedAiEntropy();
  tp.pred_presence_3 = tachyonFuture3;
  tp.pred_motion_3 = (float)onlineAiNodes();
  tp.event_eta_ms = janusBeaconCoreStaleLatched ? 1000.0f : 9999.0f;
  tp.future_stress = constrain(g.loss + g.imu_loss + (janusBeaconCoreStaleLatched ? 0.8f : 0.0f), 0.0f, 3.0f);
  tp.swarm_pressure = constrain(swarmAttentionSum + (float)onlineAiNodes() * 0.05f, 0.0f, 3.0f);

  if (janusBeaconEspNowSend("TP", &tp, sizeof(tp), true) == ESP_OK) janusBeaconTPTx++;
#endif
}

void janusBeaconBlackboxTick(bool force) {
#if JANUS_AI_SD_ENABLE
  uint32_t now = millis();
  if (!force && now - janusBeaconLastBlackboxMs < JANUS_BEACON_BLACKBOX_MS) return;
  janusBeaconLastBlackboxMs = now;
  if (!g.sd_ready) return;

  StaticJsonDocument<768> doc;
  doc["ts_ms"] = now;
  doc["event"] = "blackbox_summary";
  doc["node"] = "BeaconADV";
  doc["policy_rx"] = janusBeaconPolicyRx;
  doc["policy_age_ms"] = janusBeaconLastPolicyMs ? (now - janusBeaconLastPolicyMs) : 0xFFFFFFFFUL;
  doc["core_stale"] = janusBeaconCoreStaleLatched;
  doc["online_nodes"] = onlineAiNodes();
  doc["known_nodes"] = swarmAiNodeCount;
  doc["entropy"] = g.entropy;
  doc["swarm_entropy"] = distributedAiEntropy();
  doc["fit"] = g.fit;
  doc["loss"] = g.loss;
  doc["temp"] = g.temp_c;
  doc["hum"] = g.humidity;
  doc["pressure"] = g.pressure_hpa;
  doc["battery"] = g.battery;
  doc["rssi"] = g.wifi_rssi;
  doc["eye"] = eye.online;
  doc["audio"] = audioNode.online;
  doc["txOk"] = colonyTxOk;
  doc["txFail"] = colonyTxFail;
  doc["k2tx"] = janusBeaconK2Tx;
  doc["tptx"] = janusBeaconTPTx;
  doc["sstx"] = janusBeaconSSTx;
  String out; serializeJson(doc, out);
  janusBeaconSdAppend(JANUS_BEACON_BLACKBOX_LOG, out);
  rotateAiLogIfNeeded(JANUS_BEACON_BLACKBOX_LOG);
  janusBeaconBlackboxWrites++;
#endif
}

void janusBeaconBootEvent() {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  janusBeaconEmitEvent(JE_BOOT, "beacon_boot", 92, 35,
                       (int16_t)(g.sd_ready ? 1 : 0),
                       (int16_t)(g.qmp_ready ? 1 : 0),
                       (int16_t)(g.imu_ready ? 1 : 0),
                       (int16_t)M5.Power.getBatteryLevel(),
                       janusHash16("boot"), janusHash16("beacon"), 20000UL);
  if (g.sd_ready) {
    janusBeaconEmitEvent(JE_TASK_DONE, "archive_ready", 96, 24,
                         (int16_t)onlineAiNodes(), (int16_t)g.battery, 0, 0,
                         janusHash16("archive"), janusHash16("sd_blackbox"), 20000UL);
    janusBeaconArchiveReadyAnnounced = true;
  }
  janusBeaconSwarmSenseTick(true);
  janusBeaconKenshiTick(true);
  janusBeaconTachyonTick(true);
  janusBeaconBlackboxTick(true);
#endif
}

void janusBeaconBlackboardTick() {
#if JANUS_BEACON_BLACKBOARD_ENABLE
  uint32_t now = millis();

  bool policyAlive = janusBeaconPolicyRx && (now - janusBeaconLastPolicyMs < JANUS_BEACON_CORE_STALE_MS);
  bool coreAlive = core2SeenRecently() || policyAlive;
  if (!coreAlive && !janusBeaconCoreStaleLatched && now > JANUS_BEACON_CORE_STALE_MS) {
    janusBeaconCoreStaleLatched = true;
    janusBeaconEmitEvent(JE_TASK_NEED, "core_missing_blackbox", 88, 82,
                         (int16_t)onlineAiNodes(), (int16_t)swarmAiNodeCount,
                         (int16_t)(g.sd_ready ? 1 : 0), (int16_t)janusBeaconPolicyRx,
                         janusHash16("cortex"), janusHash16("core2"), 30000UL);
  } else if (coreAlive && janusBeaconCoreStaleLatched) {
    janusBeaconCoreStaleLatched = false;
    janusBeaconEmitEvent(JE_SAFE, "core_seen_again", 86, 28,
                         (int16_t)onlineAiNodes(), (int16_t)janusBeaconPolicyRx, 0, 0,
                         janusHash16("cortex"), janusHash16("core2"), 20000UL);
  }

  if (now - janusBeaconLastEventMs >= janusBeaconEventIntervalNow()) {
    janusBeaconEmitEvent(JE_HEARTBEAT, "beacon_heartbeat", 88, janusBeaconCoreStaleLatched ? 70 : 22,
                         (int16_t)constrain((int)(beaconLocalEntropy() * 10.0f), -32768, 32767),
                         (int16_t)constrain((int)(distributedAiEntropy() * 10.0f), -32768, 32767),
                         (int16_t)onlineAiNodes(),
                         (int16_t)(g.sd_ready ? 1 : 0),
                         janusHash16("heartbeat"), janusHash16("beacon"), 12000UL);
  }

  if (now - janusBeaconLastEnvMs >= JANUS_BEACON_ENV_MS) {
    janusBeaconLastEnvMs = now;
    janusBeaconEmitEvent(JE_ENV, "env_sentinel", 86, (g.pressure_loss > 8.0f || g.imu_loss > 0.8f) ? 66 : 24,
                         (int16_t)constrain((int)(g.temp_c * 10.0f), -32768, 32767),
                         (int16_t)constrain((int)(g.humidity * 10.0f), -32768, 32767),
                         (int16_t)constrain((int)(g.pressure_hpa * 10.0f), -32768, 32767),
                         (int16_t)constrain((int)(g.imu_shock * 100.0f), -32768, 32767),
                         janusHash16("environment"), janusHash16("beacon_env"), 15000UL);
  }

  if (g.wifi_rssi != -127 && g.wifi_rssi < -74 && now - janusBeaconLastWifiWeakMs > 15000UL) {
    janusBeaconLastWifiWeakMs = now;
    janusBeaconEmitEvent(JE_WIFI_WEAK, "wifi_weak", 80, 60,
                         (int16_t)(g.wifi_rssi * 10), (int16_t)colonyTxFail, (int16_t)colonyPeerChannel, 0,
                         janusHash16("radio"), janusHash16("beacon_wifi"), 18000UL);
  }

  if (ESP.getFreeHeap() < 90000 && now - janusBeaconLastLowHeapMs > 20000UL) {
    janusBeaconLastLowHeapMs = now;
    janusBeaconEmitEvent(JE_LOW_HEAP, "low_heap", 80, 58,
                         (int16_t)(ESP.getFreeHeap() / 1024), (int16_t)onlineAiNodes(), 0, 0,
                         janusHash16("heap"), janusHash16("beacon_heap"), 18000UL);
  }

  if (now - janusBeaconLastMemoryMs >= JANUS_BEACON_MEMORY_MS) {
    janusBeaconLastMemoryMs = now;
    janusBeaconEmitEvent(JE_AI_MEMORY, "blackbox_digest", 92, janusBeaconCoreStaleLatched ? 78 : 34,
                         (int16_t)onlineAiNodes(), (int16_t)swarmAiNodeCount,
                         (int16_t)constrain((int)(distributedAiEntropy() * 10.0f), -32768, 32767),
                         (int16_t)janusBeaconBlackboxWrites,
                         janusHash16("memory"), janusHash16("beacon_blackbox"), 45000UL);
  }

  if (now - janusBeaconLastTaskMs >= JANUS_BEACON_TASK_MS) {
    janusBeaconLastTaskMs = now;
    if (!g.sd_ready) {
      janusBeaconEmitEvent(JE_TASK_NEED, "sd_missing", 92, 88,
                           (int16_t)g.battery, (int16_t)(ESP.getFreeHeap()/1024), 0, 0,
                           janusHash16("storage"), janusHash16("sd_card"), 30000UL);
    } else if (!janusBeaconArchiveReadyAnnounced) {
      janusBeaconEmitEvent(JE_TASK_DONE, "archive_ready", 96, 24,
                           (int16_t)onlineAiNodes(), (int16_t)g.battery, 0, 0,
                           janusHash16("archive"), janusHash16("sd_blackbox"), 30000UL);
      janusBeaconArchiveReadyAnnounced = true;
    }
    if (g.qmp_ready == false) {
      janusBeaconEmitEvent(JE_TASK_NEED, "env_pressure_missing", 72, 42,
                           (int16_t)g.temp_c, (int16_t)g.humidity, 0, 0,
                           janusHash16("environment"), janusHash16("qmp6988"), 30000UL);
    }
  }

  janusBeaconSwarmSenseTick(false);
  janusBeaconKenshiTick(false);
  janusBeaconTachyonTick(false);
  janusBeaconBlackboxTick(false);

  if (now - janusBeaconLastDiagMs >= 10000UL) {
    janusBeaconLastDiagMs = now;
    Serial.printf("[BLACKBOARD/BEACON] ev=%lu pol=%lu mood=%u txOk=%lu fail=%lu peerCh=%u rebuilds=%lu K2=%lu TP=%lu SS=%lu/%lu mem=%lu need=%lu done=%lu rxK2=%lu rxTP=%lu rxSS=%lu sd=%u blackbox=%lu coreStale=%u nodes=%u order=%s\n",
                  (unsigned long)janusBeaconEventSeq,
                  (unsigned long)janusBeaconPolicyRx,
                  (unsigned)janusBeaconMood,
                  (unsigned long)colonyTxOk,
                  (unsigned long)colonyTxFail,
                  (unsigned)colonyPeerChannel,
                  (unsigned long)colonyPeerRebuilds,
                  (unsigned long)janusBeaconK2Tx,
                  (unsigned long)janusBeaconTPTx,
                  (unsigned long)janusBeaconSSTx,
                  (unsigned long)janusBeaconSSFail,
                  (unsigned long)janusBeaconMemTx,
                  (unsigned long)janusBeaconNeedTx,
                  (unsigned long)janusBeaconDoneTx,
                  (unsigned long)janusBeaconRemoteK2Rx,
                  (unsigned long)janusBeaconRemoteTPRx,
                  (unsigned long)janusBeaconRemoteSSRx,
                  g.sd_ready ? 1 : 0,
                  (unsigned long)janusBeaconBlackboxWrites,
                  janusBeaconCoreStaleLatched ? 1 : 0,
                  (unsigned)onlineAiNodes(),
                  janusBeaconOrder);
  }
#endif
}

// ==========                         ==========
JsonObject pickTelemetryObject(StaticJsonDocument<4096>& doc) {
  JsonObject root = doc.as<JsonObject>();
  if (root.isNull()) return root;
  if (root.containsKey("payload") && root["payload"].is<JsonObject>()) {
    JsonObject p = root["payload"].as<JsonObject>();
    if (p.containsKey("payload") && p["payload"].is<JsonObject>()) return p["payload"].as<JsonObject>();
    if (p.containsKey("data") && p["data"].is<JsonObject>()) return p["data"].as<JsonObject>();
    return p;
  }
  if (root.containsKey("data") && root["data"].is<JsonObject>()) {
    JsonObject d = root["data"].as<JsonObject>();
    if (d.containsKey("payload") && d["payload"].is<JsonObject>()) return d["payload"].as<JsonObject>();
    if (d.containsKey("data") && d["data"].is<JsonObject>()) return d["data"].as<JsonObject>();
    return d;
  }
  return root;
}

float jsonFloat(JsonObject obj, const char* key, float fallback) {
  if (obj.isNull() || !obj.containsKey(key)) return fallback;
  return obj[key].as<float>();
}

String jsonString(JsonObject obj, const char* key, const String& fallback = "") {
  if (obj.isNull() || !obj.containsKey(key) || obj[key].isNull()) return fallback;
  return String((const char*)obj[key]);
}



// ========================= JANUS SD TACHYON HOME / LITTLEFS RELIEF v4.4 =========================
// Goal: LittleFS stays clean; SD carries the long memory.
// The SD brain budget is 15GB. Cleanup removes only Janus-owned tmp/bak/old/archive/train logs.

bool janusTrySdBegin(uint8_t cs, uint32_t hz) {
  SPI.begin();
  bool ok = SD.begin(cs, SPI, hz);
  if (ok && SD.cardType() != CARD_NONE) {
    janusSdCsUsed = cs;
    uint32_t mb = (uint32_t)(SD.cardSize() / (1024ULL * 1024ULL));
    Serial.printf("[SD] JANUS ready cs=%u hz=%lu card=%luMB\n", cs, (unsigned long)hz, (unsigned long)mb);
    return true;
  }
  SD.end();
  delay(35);
  return false;
}

bool initJanusSdCard() {
#if JANUS_AI_SD_ENABLE
  const uint32_t speeds[] = {25000000UL, 10000000UL, 4000000UL, 1000000UL};
  const uint8_t csList[] = {CARDPUTER_SD_FALLBACK_CS_PIN, CARDPUTER_ADV_SD_CS_PIN};  // v4.5A: ADV launcher logs show CS=12 succeeds; try it first
  for (uint8_t c = 0; c < sizeof(csList); c++) {
    for (uint8_t s = 0; s < sizeof(speeds) / sizeof(speeds[0]); s++) {
      if (janusTrySdBegin(csList[c], speeds[s])) return true;
    }
  }
  Serial.println("[SD] JANUS SD mount failed. LittleFS fallback is throttled.");
#endif
  return false;
}

void janusMkdir(const char* path) {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready || !path || !path[0]) return;
  if (!SD.exists(path)) SD.mkdir(path);
#endif
}

void janusEnsureSdDirs() {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return;
  janusMkdir(JANUS_BRAIN_SD_DIR);
  janusMkdir(JANUS_TACHYON_SD_DIR);
  janusMkdir(JANUS_AI_SD_DIR);
  janusMkdir(JANUS_AI_POLICY_DIR);
  janusMkdir(COLONY_SD_DIR);
#endif
}

File janusOpenReadDual(const char* sdPath, const char* fsPath) {
  if (g.sd_ready && sdPath && SD.exists(sdPath)) return SD.open(sdPath, FILE_READ);
  return LittleFS.open(fsPath, FILE_READ);
}

File janusOpenWriteDual(const char* sdPath, const char* fsPath) {
  if (g.sd_ready && sdPath) {
    janusEnsureSdDirs();
    return SD.open(sdPath, FILE_WRITE);
  }
  return LittleFS.open(fsPath, FILE_WRITE);
}

void janusRemoveForWriteDual(const char* sdPath, const char* fsPath) {
  if (g.sd_ready && sdPath) {
    if (SD.exists(sdPath)) SD.remove(sdPath);
  } else {
    LittleFS.remove(fsPath);
  }
}

bool janusSdOwnedPath(const char* path) {
  return g.sd_ready && path && strncmp(path, "/janus_", 7) == 0;
}

File janusOpenPath(const char* path, const char* mode) {
  if (janusSdOwnedPath(path)) return SD.open(path, mode);
  return LittleFS.open(path, mode);
}

void janusRemovePath(const char* path) {
  if (janusSdOwnedPath(path)) SD.remove(path);
  else LittleFS.remove(path);
}

uint32_t janusFileSizeSd(const char* path) {
  if (!g.sd_ready || !SD.exists(path)) return 0;
  File f = SD.open(path, FILE_READ);
  if (!f) return 0;
  uint32_t s = f.size();
  f.close();
  return s;
}

void janusCopyLittleFsFileToSd(const char* src, const char* dst) {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready || !LittleFS.exists(src) || SD.exists(dst)) return;
  janusEnsureSdDirs();
  File in = LittleFS.open(src, FILE_READ);
  if (!in) return;
  File out = SD.open(dst, FILE_WRITE);
  if (!out) { in.close(); return; }
  uint8_t buf[256];
  while (in.available()) {
    size_t n = in.read(buf, sizeof(buf));
    if (n == 0) break;
    out.write(buf, n);
  }
  in.close();
  out.close();
  Serial.printf("[SD] migrated LittleFS %s -> %s\n", src, dst);
#endif
}

void janusMigrateLittleFsToSd() {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return;
  janusCopyLittleFsFileToSd(STATE_FILE, JANUS_STATE_SD_FILE);
  janusCopyLittleFsFileToSd(MODEL_FILE, JANUS_MODEL_SD_FILE);
  janusCopyLittleFsFileToSd(MEMORY_FILE, JANUS_MEMORY_SD_FILE);
  janusCopyLittleFsFileToSd(PHRASE_FILE, JANUS_PHRASE_SD_FILE);
  janusCopyLittleFsFileToSd(CHAIN_FILE, JANUS_CHAIN_SD_FILE);
  janusCopyLittleFsFileToSd(JANUS_THETA_FILE, JANUS_THETA_SD_FILE);
  janusCopyLittleFsFileToSd(QUEUE_FILE, JANUS_QUEUE_SD_FILE);
  janusCopyLittleFsFileToSd(WITNESS_FILE, SD_WITNESS_FILE);
#endif
}

void janusFreeLittleFsLegacy() {
  // Always remove noisy LittleFS logs/queues first. With SD mounted, remove all migrated Janus runtime files too.
  LittleFS.remove(WITNESS_FILE);
  LittleFS.remove(QUEUE_FILE);
  if (g.sd_ready) {
    LittleFS.remove(STATE_FILE);
    LittleFS.remove(MODEL_FILE);
    LittleFS.remove(MEMORY_FILE);
    LittleFS.remove(PHRASE_FILE);
    LittleFS.remove(CHAIN_FILE);
    LittleFS.remove(JANUS_THETA_FILE);
  }
  Serial.println("[LittleFS] Janus legacy runtime files cleaned/throttled");
}

bool janusJunkName(const String& base) {
  return base.endsWith(".tmp") || base.endsWith(".bak") || base.endsWith(".old") || base.endsWith(".crash");
}

bool janusOldTrainOrArchiveName(const String& base) {
  if (base == "train_current.jsonl" || base == "current.jsonl" || base == "events.jsonl" || base == "nodes.jsonl") return false;
  return base.startsWith("train_") || base.startsWith("archive_") || base.startsWith("log_");
}

uint64_t janusDirSizeSd(const char* dirPath) {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return 0;
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return 0; }
  uint64_t total = 0;
  File file = dir.openNextFile();
  while (file) {
    String base = String(file.name());
    base = base.substring(base.lastIndexOf('/') + 1);
    String full = String(dirPath) + "/" + base;
    if (file.isDirectory()) total += janusDirSizeSd(full.c_str());
    else total += (uint64_t)file.size();
    file.close();
    file = dir.openNextFile();
  }
  dir.close();
  return total;
#else
  return 0;
#endif
}

bool janusDeleteFirstMatchingSd(const char* dirPath, bool junkOnly) {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return false;
  File dir = SD.open(dirPath);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return false; }
  File file = dir.openNextFile();
  while (file) {
    String base = String(file.name());
    base = base.substring(base.lastIndexOf('/') + 1);
    String full = String(dirPath) + "/" + base;
    bool isDir = file.isDirectory();
    file.close();

    if (isDir) {
      if (janusDeleteFirstMatchingSd(full.c_str(), junkOnly)) { dir.close(); return true; }
    } else {
      bool kill = janusJunkName(base) || (!junkOnly && janusOldTrainOrArchiveName(base));
      if (kill) {
        SD.remove(full.c_str());
        Serial.printf("[SD CLEAN] removed %s\n", full.c_str());
        dir.close();
        return true;
      }
    }
    file = dir.openNextFile();
  }
  dir.close();
#endif
  return false;
}

void janusRotateTachyonTrainLog() {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready || !SD.exists(JANUS_TACHYON_TRAIN_LOG)) return;
  uint32_t sz = janusFileSizeSd(JANUS_TACHYON_TRAIN_LOG);
  if (sz < JANUS_TACHYON_LOG_MAX_BYTES) return;
  char dst[96];
  snprintf(dst, sizeof(dst), "%s%lu.jsonl", JANUS_TACHYON_TRAIN_PREFIX, (unsigned long)(millis() / 1000UL));
  SD.rename(JANUS_TACHYON_TRAIN_LOG, dst);
  Serial.printf("[SD] rotated tachyon train log -> %s\n", dst);
#endif
}

void janusCleanupStorageTick() {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return;
  uint32_t now = millis();
  if (now - lastBrainSdCleanupAt < JANUS_BRAIN_SD_CLEANUP_MS) return;
  lastBrainSdCleanupAt = now;

  janusEnsureSdDirs();
  janusRotateTachyonTrainLog();

  // First pass: remove exact junk files.
  for (uint8_t i = 0; i < 8; i++) {
    if (!janusDeleteFirstMatchingSd(JANUS_BRAIN_SD_DIR, true)) break;
  }

  // Budget pass: Janus brain may grow up to 15GB. Old train/archive chunks are expendable.
  uint64_t brainBytes = janusDirSizeSd(JANUS_BRAIN_SD_DIR);
  uint8_t guard = 0;
  while (brainBytes > JANUS_SD_BRAIN_BUDGET_BYTES && guard++ < 32) {
    if (!janusDeleteFirstMatchingSd(JANUS_BRAIN_SD_DIR, false)) break;
    brainBytes = janusDirSizeSd(JANUS_BRAIN_SD_DIR);
  }

  Serial.printf("[SD CLEAN] brain=%luMB budget=15360MB\n", (unsigned long)(brainBytes / (1024ULL * 1024ULL)));
#endif
}

void janusLogTachyonSample() {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return;
  uint32_t now = millis();
  if (now - lastTachyonSampleAt < JANUS_TACHYON_SAMPLE_MS) return;
  lastTachyonSampleAt = now;
  janusEnsureSdDirs();
  janusRotateTachyonTrainLog();

  StaticJsonDocument<768> doc;
  doc["ts_ms"] = now;
  doc["kind"] = "tachyon_sample_v44";
  doc["entropy"] = g.entropy;
  doc["pred_entropy"] = g.pred_entropy;
  doc["loss"] = g.loss;
  doc["mi"] = g.mi;
  doc["fit"] = g.fit;
  doc["f1"] = g.f1;
  doc["f2"] = g.f2;
  doc["future1"] = tachyonFuture1;
  doc["future2"] = tachyonFuture2;
  doc["future3"] = tachyonFuture3;
  doc["swarm_entropy"] = distributedAiEntropy();
  doc["swarm_nodes"] = onlineAiNodes();
  doc["eye_sync"] = eye.sync;
  doc["aud_rms"] = audioNode.mic_rms;
  doc["theta_q"] = thetaState.q;
  doc["theta3"] = thetaState.theta3;
  doc["mock"] = thetaState.mock;
  doc["theta_res"] = thetaState.resonance;
  doc["theta_conf"] = thetaState.confidence;
  doc["tau_like"] = thetaState.tauLike;
  doc["colony_w"] = colonyEntropyWeight;
  doc["free_littlefs"] = false;

  File f = SD.open(JANUS_TACHYON_TRAIN_LOG, FILE_APPEND);
  if (!f) return;
  serializeJson(doc, f);
  f.println();
  f.close();
#endif
}

float janusSafeLoss(float v) {
  if (!isfinite(v)) return 0.0f;
  return constrain(v, 0.0f, 5.0f);
}

float janusSafeEntropy(float v) {
  if (!isfinite(v)) return 0.0f;
  return constrain(v, 0.0f, 10.0f);
}


// ========================= RAMANUJAN / THETA NOTEBOOK v4.3C =========================
float janusClampF(float v, float lo, float hi) {
  if (!isfinite(v)) return lo;
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

uint32_t janusMix32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dUL;
  x ^= x >> 15; x *= 0x846ca68bUL;
  x ^= x >> 16;
  return x;
}

void saveThetaState() {
#if JANUS_THETA_ENABLE
  thetaState.magic = JANUS_THETA_MAGIC;
  janusRemoveForWriteDual(JANUS_THETA_SD_FILE, JANUS_THETA_FILE);
  File f = janusOpenWriteDual(JANUS_THETA_SD_FILE, JANUS_THETA_FILE);
  if (!f) return;
  f.write((uint8_t*)&thetaState, sizeof(thetaState));
  f.close();
#endif
}

void loadThetaState() {
#if JANUS_THETA_ENABLE
  File f = janusOpenReadDual(JANUS_THETA_SD_FILE, JANUS_THETA_FILE);
  if (!f) return;
  JanusThetaState tmp;
  if (f.read((uint8_t*)&tmp, sizeof(tmp)) == sizeof(tmp) && tmp.magic == JANUS_THETA_MAGIC) {
    thetaState = tmp;
    thetaState.q = janusClampF(thetaState.q, 0.05f, 0.90f);
    thetaState.confidence = constrain(thetaState.confidence, 0, 100);
    if (!thetaState.lemma[0]) strlcpy(thetaState.lemma, "Theta restored", sizeof(thetaState.lemma));
  }
  f.close();
#endif
}

void updateThetaState(float currentEntropy, bool significantEvent) {
#if JANUS_THETA_ENABLE
  uint32_t now = millis();
  if (thetaState.studies > 0 && (now - thetaState.lastStudyMs) < JANUS_THETA_MIN_GAP_MS) return;
  if (!significantEvent && thetaState.studies > 0 && (now - thetaState.lastStudyMs) < JANUS_THETA_STUDY_MS) return;
  thetaState.lastStudyMs = now;
  thetaState.studies++;

  float targetQ = 0.06f + janusClampF(currentEntropy / 12.0f, 0.0f, 0.82f);
  targetQ += loveExperimentActive ? 0.025f : 0.0f;
  targetQ += (eye.online ? janusClampF(eye.sync, 0.0f, 1.0f) * 0.020f : 0.0f);
  thetaState.q += (targetQ - thetaState.q) * 0.15f;
  thetaState.q = janusClampF(thetaState.q, 0.05f, 0.90f);
  float q = thetaState.q;

  float theta2 = 0.0f;
  float theta3 = 1.0f;
  float theta4 = 1.0f;
  for (uint8_t n = 1; n <= JANUS_THETA_DEPTH; n++) {
    float nn = (float)(n * n);
    float qnn = powf(q, nn);
    theta3 += 2.0f * qnn;
    theta4 += 2.0f * ((n & 1) ? -qnn : qnn);
    float hp = (float)n - 0.5f;
    theta2 += 2.0f * powf(q, hp * hp);
  }

  float mock = 0.0f;
  float denom = 1.0f;
  for (uint8_t n = 1; n <= JANUS_THETA_DEPTH; n++) {
    float qn = powf(q, (float)n);
    denom *= (1.0f + qn);
    mock += powf(q, (float)(n * n)) / max(0.0001f, denom * denom);
  }

  float pent = 0.0f;
  for (int n = -JANUS_THETA_DEPTH; n <= JANUS_THETA_DEPTH; n++) {
    if (n == 0) continue;
    int p = n * (3 * n - 1) / 2;
    float sign = (n & 1) ? -1.0f : 1.0f;
    pent += sign * powf(q, (float)abs(p));
  }

  uint32_t seed = janusMix32((uint32_t)(g.entropy * 1000.0f) ^ ((uint32_t)thetaState.resonance << 16) ^ thetaState.studies ^ (uint32_t)ESP.getEfuseMac());
  thetaState.tauLike = janusMix32(thetaState.tauLike ^ seed ^ ((uint32_t)(g.fit * 1000.0f) << 1));

  thetaState.theta2 = theta2;
  thetaState.theta3 = theta3;
  thetaState.theta4 = theta4;
  thetaState.mock = mock;
  thetaState.partitionSignal = janusClampF(fabsf(pent) / 10.0f, 0.0f, 1.0f);
  float thetaNorm = janusClampF((theta3 - 1.0f) / 4.0f, 0.0f, 1.0f);
  float mockNorm = janusClampF(mock * 2.0f, 0.0f, 1.0f);
  float partNorm = thetaState.partitionSignal;
  float tauNorm = (float)(thetaState.tauLike & 0xFFFFUL) / 65535.0f;
  float rawRes = 0.42f * thetaNorm + 0.22f * mockNorm + 0.18f * partNorm + 0.18f * tauNorm;
  uint8_t res = (uint8_t)janusClampF(rawRes * 255.0f, 0.0f, 255.0f);
  thetaState.resonance = (uint8_t)(((uint16_t)thetaState.resonance * 3U + res) / 4U);
  thetaState.confidence = (uint8_t)janusClampF(20.0f + rawRes * 80.0f, 0.0f, 100.0f);
  snprintf(thetaState.lemma, sizeof(thetaState.lemma), "Carr q=%.2f th3=%.2f mu=%.3f", q, theta3, mock);

  if ((significantEvent || (thetaState.studies % 10 == 0)) && (now - janusThetaLastSerialMs >= JANUS_THETA_SERIAL_MS)) {
    janusThetaLastSerialMs = now;
    Serial.printf("[THETA] %s res=%u conf=%u tau=%08lx studies=%lu\n",
                  thetaState.lemma, thetaState.resonance, thetaState.confidence,
                  (unsigned long)thetaState.tauLike, (unsigned long)thetaState.studies);
  }
#endif
}

float thetaFuture(float base, int steps) {
  float res = (float)thetaState.resonance / 255.0f;
  return janusClampF(base * (1.0f + res * 0.2f * (float)steps), 0.01f, 10.0f);
}

void applyThetaToColonyWeights() {
  float thetaFactor = 1.0f + (float)thetaState.resonance / 512.0f;
  colonyEntropyWeight = janusClampF(colonyEntropyWeight * 0.95f + thetaFactor * 0.05f, 0.75f, 2.40f);
  colonySurpriseWeight = janusClampF(colonySurpriseWeight * 0.95f + thetaFactor * 0.05f, 0.65f, 2.75f);
}

void janusParadoxSelfStudyTick() {
#if JANUS_SHA_HOME_ENABLE
  static uint32_t lastSelfMs = 0;
  static uint32_t cursor = 0;
  static uint16_t bestBits = 0;
  uint32_t now = millis();
  if (now - lastSelfMs < 240UL) return;
  lastSelfMs = now;
  uint8_t buf[24];
  uint8_t h[32];
  for (uint8_t i = 0; i < 32; ++i) {
    uint32_t nonce = cursor++;
    writeLE32(buf + 0, nonce);
    writeLE32(buf + 4, (uint32_t)(g.entropy * 1000.0f));
    writeLE32(buf + 8, (uint32_t)thetaState.tauLike);
    writeLE32(buf + 12, (uint32_t)(g.fit * 1000.0f));
    writeLE32(buf + 16, (uint32_t)millis());
    writeLE32(buf + 20, (uint32_t)ESP.getEfuseMac());
    doubleSha256(buf, sizeof(buf), h);
    uint16_t bits = countLeadingZeroBitsBE(h);
    if (bits > bestBits) {
      bestBits = bits;
      updateThetaState(g.entropy + (float)bits * 0.03f, true);
      if (bestBits >= 20) {
        Serial.printf("[SHAHOME] bestBits=%u cursor=%lu theta=%u\n", bestBits, (unsigned long)cursor, thetaState.resonance);
      }
    }
  }
#endif
}

// ==========                  ==========
void saveMemoryEntry(float entropy, float fit, float eye_sync, float tmos) {
  MemoryEntry entry = {entropy, fit, eye_sync, tmos};
  if (memoryCount < MAX_MEMORY_ENTRIES) {
    memoryEntries[memoryCount++] = entry;
    janusMemoryDirty = true;
    return;
  }
  int worstIdx = 0;
  float worstScore = memoryEntries[0].fit * memoryEntries[0].eye_sync;
  for (int i = 1; i < memoryCount; ++i) {
    float score = memoryEntries[i].fit * memoryEntries[i].eye_sync;
    if (score < worstScore) {
      worstScore = score;
      worstIdx = i;
    }
  }
  memoryEntries[worstIdx] = entry;
  janusMemoryDirty = true;
}

float memoryBias() {
  float bestScore = 0.0f;
  for (int i = 0; i < memoryCount; ++i) {
    float score = memoryEntries[i].fit * memoryEntries[i].eye_sync;
    if (score > bestScore) bestScore = score;
  }
  return bestScore * 0.05f * (1.0f + eye.sync);
}

void saveMemoryState() {
  janusRemoveForWriteDual(JANUS_MEMORY_SD_FILE, MEMORY_FILE);
  File f = janusOpenWriteDual(JANUS_MEMORY_SD_FILE, MEMORY_FILE);
  if (!f) return;
  uint16_t count = memoryCount;
  f.write((uint8_t*)&count, sizeof(count));
  for (int i = 0; i < memoryCount; ++i) {
    f.write((uint8_t*)&memoryEntries[i], sizeof(MemoryEntry));
  }
  f.close();
  janusMemoryDirty = false;
}

void loadMemoryState() {
  File f = janusOpenReadDual(JANUS_MEMORY_SD_FILE, MEMORY_FILE);
  if (!f) return;
  uint16_t count = 0;
  if (f.read((uint8_t*)&count, sizeof(count)) != sizeof(count)) {
    f.close();
    return;
  }
  memoryCount = 0;
  for (uint16_t i = 0; i < count && i < MAX_MEMORY_ENTRIES; ++i) {
    MemoryEntry e;
    if (f.read((uint8_t*)&e, sizeof(MemoryEntry)) == sizeof(MemoryEntry)) {
      memoryEntries[memoryCount++] = e;
    } else break;
  }
  f.close();
}

// ==========                 (v2) ==========
void savePhrases() {
  janusRemoveForWriteDual(JANUS_PHRASE_SD_FILE, PHRASE_FILE);
  File f = janusOpenWriteDual(JANUS_PHRASE_SD_FILE, PHRASE_FILE);
  if (!f) return;
  uint32_t magic = PHRASE_MAGIC;
  f.write((uint8_t*)&magic, sizeof(magic));
  uint16_t count = phraseCount;
  f.write((uint8_t*)&count, sizeof(count));
  for (int i = 0; i < phraseCount; ++i) {
    f.write((uint8_t*)&phraseMemory[i], sizeof(PhraseMemory));
  }
  f.close();
  janusPhraseDirty = false;
}

void loadPhrases() {
  File f = janusOpenReadDual(JANUS_PHRASE_SD_FILE, PHRASE_FILE);
  if (!f) {
    initDefaultPhrases();
    janusPhraseDirty = true;
    return;
  }
  uint32_t magic = 0;
  if (f.read((uint8_t*)&magic, sizeof(magic)) != sizeof(magic) || magic != PHRASE_MAGIC) {
    f.close();
    initDefaultPhrases();
    janusPhraseDirty = true;
    return;
  }
  uint16_t count = 0;
  if (f.read((uint8_t*)&count, sizeof(count)) != sizeof(count)) {
    f.close();
    initDefaultPhrases();
    janusPhraseDirty = true;
    return;
  }
  phraseCount = 0;
  for (uint16_t i = 0; i < count && i < MAX_PHRASES; ++i) {
    PhraseMemory p;
    if (f.read((uint8_t*)&p, sizeof(PhraseMemory)) == sizeof(PhraseMemory)) {
      phraseMemory[phraseCount++] = p;
    } else break;
  }
  f.close();
  if (phraseCount == 0) { initDefaultPhrases(); janusPhraseDirty = true; }
}

void initDefaultPhrases() {
  phraseCount = 0;
  const char* seeds[] = {"observing", "alignment", "entropy spike", "presence", "optimal", "drifting", "tachyon", "house mode", "high m2r"};
  float seedFit[] = {0.8f, 1.5f, 0.5f, 0.7f, 1.8f, 0.6f, 1.2f, 1.0f, 1.3f};
  float seedSync[] = {0.5f, 0.8f, 0.3f, 0.6f, 0.9f, 0.4f, 0.7f, 0.5f, 0.6f};
  float seedEntropy[] = {3.0f, 2.5f, 6.0f, 2.8f, 2.2f, 4.0f, 3.2f, 2.7f, 3.5f};
  for (int i = 0; i < 9 && phraseCount < MAX_PHRASES; ++i) {
    PhraseMemory p;
    strncpy(p.text, seeds[i], sizeof(p.text)-1);
    p.text[sizeof(p.text)-1] = 0;
    p.avg_fit = seedFit[i];
    p.avg_sync = seedSync[i];
    p.avg_entropy = seedEntropy[i];
    p.usage_count = 1.0f;
    p.last_used = 0;
    phraseMemory[phraseCount++] = p;
  }
}


void saveChains() {
  janusRemoveForWriteDual(JANUS_CHAIN_SD_FILE, CHAIN_FILE);
  File f = janusOpenWriteDual(JANUS_CHAIN_SD_FILE, CHAIN_FILE);
  if (!f) return;
  uint32_t magic = CHAIN_MAGIC;
  f.write((uint8_t*)&magic, sizeof(magic));
  uint16_t count = chainCount;
  f.write((uint8_t*)&count, sizeof(count));
  for (int i = 0; i < chainCount; ++i) {
    f.write((uint8_t*)&thoughtChains[i], sizeof(ThoughtChain));
  }
  f.close();
  janusChainsDirty = false;
}

void loadChains() {
  File f = janusOpenReadDual(JANUS_CHAIN_SD_FILE, CHAIN_FILE);
  if (!f) return;
  uint32_t magic = 0;
  if (f.read((uint8_t*)&magic, sizeof(magic)) != sizeof(magic) || magic != CHAIN_MAGIC) {
    f.close();
    return;
  }
  uint16_t count = 0;
  if (f.read((uint8_t*)&count, sizeof(count)) != sizeof(count)) {
    f.close();
    return;
  }
  chainCount = 0;
  for (uint16_t i = 0; i < count && i < MAX_CHAINS; ++i) {
    ThoughtChain c;
    if (f.read((uint8_t*)&c, sizeof(ThoughtChain)) == sizeof(ThoughtChain)) {
      thoughtChains[chainCount++] = c;
    } else {
      break;
    }
  }
  f.close();
}

String buildSeedPhrase(float adjustedFit, float currentSync, float currentEntropy) {
  if (currentSync > 0.80f && g.loss < 0.08f) return "system stable";
  if (adjustedFit > 1.60f) return "resonance lock";
  if (currentEntropy > 5.00f) return "entropy spike";
  if (eye.tmos_presence > 400.0f) return "presence anomaly";
  if (g.loss > 0.30f) return "pattern unstable";
  return "signal drift";
}

void rememberThoughtTransition(const String& first, const String& second) {
  if (!first.length() || !second.length() || first == second) return;

  for (int i = 0; i < chainCount; ++i) {
    if (String(thoughtChains[i].first) == first && String(thoughtChains[i].second) == second) {
      thoughtChains[i].strength = min(10.0f, thoughtChains[i].strength * 0.92f + 0.8f);
      thoughtChains[i].avg_entropy = thoughtChains[i].avg_entropy * 0.85f + g.entropy * 0.15f;
      thoughtChains[i].avg_sync = thoughtChains[i].avg_sync * 0.85f + eye.sync * 0.15f;
      thoughtChains[i].usage_count = min<int>(65535, thoughtChains[i].usage_count + 1);
      janusChainsDirty = true;
      return;
    }
  }

  ThoughtChain c;
  memset(&c, 0, sizeof(c));
  strncpy(c.first, first.c_str(), sizeof(c.first) - 1);
  strncpy(c.second, second.c_str(), sizeof(c.second) - 1);
  c.strength = 1.0f;
  c.avg_entropy = g.entropy;
  c.avg_sync = eye.sync;
  c.usage_count = 1;

  if (chainCount < MAX_CHAINS) {
    thoughtChains[chainCount++] = c;
    janusChainsDirty = true;
    return;
  }

  int worstIdx = 0;
  float worstScore = thoughtChains[0].strength + thoughtChains[0].usage_count * 0.05f;
  for (int i = 1; i < chainCount; ++i) {
    float s = thoughtChains[i].strength + thoughtChains[i].usage_count * 0.05f;
    if (s < worstScore) {
      worstScore = s;
      worstIdx = i;
    }
  }
  thoughtChains[worstIdx] = c;
  janusChainsDirty = true;
}

String applyThoughtChainBias(const String& candidate) {
  if (!lastThought.length()) return candidate;

  String best = candidate;
  float bestScore = -1e9f;

  for (int i = 0; i < chainCount; ++i) {
    if (String(thoughtChains[i].first) != lastThought) continue;

    float entropySim = 1.0f - fabs(g.entropy - thoughtChains[i].avg_entropy) / max(1.0f, thoughtChains[i].avg_entropy + 0.1f);
    float syncSim = 1.0f - fabs(eye.sync - thoughtChains[i].avg_sync);
    float score = thoughtChains[i].strength * 0.6f + entropySim * 0.25f + syncSim * 0.15f;

    if (String(thoughtChains[i].second) == candidate) score += 0.35f;

    if (score > bestScore) {
      bestScore = score;
      best = String(thoughtChains[i].second);
    }
  }

  return best;
}

String selectOrCreatePhrase() {
  float currentFit = g.fit;
  float currentSync = eye.sync;
  float currentEntropy = g.entropy;
  float adjustedFit = currentFit + memoryBias();

  int bestIdx = -1;
  float bestScore = -1e9f;

  for (int i = 0; i < phraseCount; ++i) {
    float fitSim = 1.0f - fabs(adjustedFit - phraseMemory[i].avg_fit) / max(1.0f, phraseMemory[i].avg_fit + 0.1f);
    float syncSim = 1.0f - fabs(currentSync - phraseMemory[i].avg_sync);
    float entropySim = 1.0f - fabs(currentEntropy - phraseMemory[i].avg_entropy) / max(1.0f, phraseMemory[i].avg_entropy + 0.1f);

    float recencyBonus = (millis() - phraseMemory[i].last_used) / 10000.0f;
    float usagePenalty = 1.0f / (1.0f + 0.05f * phraseMemory[i].usage_count);

    float score = (fitSim * 0.4f + syncSim * 0.4f + entropySim * 0.2f) * usagePenalty + recencyBonus;
    if (score > bestScore) {
      bestScore = score;
      bestIdx = i;
    }
  }

  bool forceCreate = false;
  if (bestIdx >= 0 && bestScore > 0.5f) {
    float explore = (float)(esp_random() % 1000) / 1000.0f;
    if (explore < 0.15f) forceCreate = true;
  }

  if (adjustedFit < 0.8f && currentSync < 0.4f) {
    return applyThoughtChainBias("noise pattern");
  }

  if (bestIdx >= 0 && bestScore > 0.5f && !forceCreate) {
    PhraseMemory& p = phraseMemory[bestIdx];
    p.last_used = millis();
    p.usage_count += 1.0f;
    p.usage_count *= 0.995f;
    p.avg_fit = p.avg_fit * 0.9f + adjustedFit * 0.1f;
    p.avg_sync = p.avg_sync * 0.9f + currentSync * 0.1f;
    p.avg_entropy = p.avg_entropy * 0.9f + currentEntropy * 0.1f;
    String chosen = applyThoughtChainBias(String(p.text));
    janusPhraseDirty = true;
    return chosen;
  }

  String seed = buildSeedPhrase(adjustedFit, currentSync, currentEntropy);
  String candidate = applyThoughtChainBias(seed);

  if (phraseCount < MAX_PHRASES) {
    PhraseMemory newPhrase;
    memset(&newPhrase, 0, sizeof(newPhrase));
    strncpy(newPhrase.text, candidate.c_str(), sizeof(newPhrase.text) - 1);
    newPhrase.avg_fit = adjustedFit;
    newPhrase.avg_sync = currentSync;
    newPhrase.avg_entropy = currentEntropy;
    newPhrase.usage_count = 1.0f;
    newPhrase.last_used = millis();
    phraseMemory[phraseCount++] = newPhrase;
    janusPhraseDirty = true;
    return candidate;
  }

  int forgetIdx = 0;
  float worstScore = 1e9f;
  for (int i = 0; i < phraseCount; ++i) {
    float score = phraseMemory[i].usage_count * (phraseMemory[i].avg_fit + phraseMemory[i].avg_sync);
    if (score < worstScore) {
      worstScore = score;
      forgetIdx = i;
    }
  }

  memset(&phraseMemory[forgetIdx], 0, sizeof(PhraseMemory));
  strncpy(phraseMemory[forgetIdx].text, candidate.c_str(), sizeof(phraseMemory[forgetIdx].text) - 1);
  phraseMemory[forgetIdx].avg_fit = adjustedFit;
  phraseMemory[forgetIdx].avg_sync = currentSync;
  phraseMemory[forgetIdx].avg_entropy = currentEntropy;
  phraseMemory[forgetIdx].usage_count = 1.0f;
  phraseMemory[forgetIdx].last_used = millis();
  janusPhraseDirty = true;
  return candidate;
}

// ==========                   (             ,              selectOrCreatePhrase) ==========
uint16_t uiPrimary() { return houseProtocolActive ? COLOR_AMBER : TFT_WHITE; }
uint16_t uiSecondary() { return houseProtocolActive ? COLOR_AMBER_DIM : TFT_LIGHTGREY; }

void setSpeakerVolume() {
  // Global Beacon volume, soft capped to keep ADV speaker clean.
  uint8_t safeVol = (uint8_t)constrain(volume, 0, 150);
  M5Cardputer.Speaker.setVolume(safeVol);
}
void playTone(int freq, int dur = 40) {
  if (volume <= 0) return;
  setSpeakerVolume();
  M5Cardputer.Speaker.tone(freq, dur);
}

String joinUrl(const String& base, const String& path) {
  if (base.endsWith("/") && path.startsWith("/")) return base.substring(0, base.length() - 1) + path;
  if (!base.endsWith("/") && !path.startsWith("/")) return base + "/" + path;
  return base + path;
}
String activeBaseUrl() {
  if (active_server_index < 0 || active_server_index >= SERVER_COUNT) return "";
  return String(SERVER_CANDIDATES[active_server_index]);
}
String extractJsonValue(const String& payload, const String& key) {
  String needle = "\"" + key + "\":";
  int pos = payload.indexOf(needle);
  if (pos < 0) return "";
  pos += needle.length();
  while (pos < (int)payload.length() && isspace((unsigned char)payload[pos])) pos++;
  if (pos >= (int)payload.length()) return "";
  if (payload[pos] == '"') {
    int end = payload.indexOf('"', pos + 1);
    if (end < 0) return "";
    return payload.substring(pos + 1, end);
  }
  int end = pos;
  while (end < (int)payload.length() && payload[end] != ',' && payload[end] != '}' && payload[end] != '\n') end++;
  return payload.substring(pos, end);
}
bool httpGetRaw(const String& url, String& response, int timeoutMs = 1800) {
  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  if (!http.begin(url)) return false;
  int code = http.GET();
  response = (code > 0) ? http.getString() : "";
  http.end();
  return code >= 200 && code < 300;
}
bool httpPostRaw(const String& url, const String& payload, String& response, int timeoutMs = 1800) {
  HTTPClient http;
  http.setConnectTimeout(timeoutMs);
  http.setTimeout(timeoutMs);
  if (!http.begin(url)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST(payload);
  response = (code > 0) ? http.getString() : "";
  http.end();
  return code >= 200 && code < 300;
}
const char* janusQueuePath() {
  return g.sd_ready ? JANUS_QUEUE_SD_FILE : QUEUE_FILE;
}

std::vector<String> readLines(const char* path) {
  std::vector<String> lines;
  File f = janusOpenPath(path, FILE_READ);
  if (!f) return lines;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length()) lines.push_back(line);
  }
  f.close();
  return lines;
}
void writeLines(const char* path, const std::vector<String>& lines) {
  janusRemovePath(path);
  File f = janusOpenPath(path, FILE_WRITE);
  if (!f) return;
  for (const auto& line : lines) f.println(line);
  f.close();
}
void queuePacket(const String& payload) {
  const char* qp = janusQueuePath();
  std::vector<String> lines = readLines(qp);
  lines.push_back(payload);
  if ((int)lines.size() > MAX_QUEUE_LINES) lines.erase(lines.begin(), lines.begin() + ((int)lines.size() - MAX_QUEUE_LINES));
  writeLines(qp, lines);
}
bool initWiFi(bool force = false) {
  if (!force && WiFi.status() == WL_CONNECTED) return true;
  if (!force && millis() - lastWifiTryAt < WIFI_RETRY_MS) return false;
  lastWifiTryAt = millis();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  const unsigned long until = millis() + 4500;
  while (WiFi.status() != WL_CONNECTED && millis() < until) delay(60);
  return WiFi.status() == WL_CONNECTED;
}
bool ensureWiFi() { return WiFi.status() == WL_CONNECTED ? true : initWiFi(false); }
bool selectActiveServer(bool forceProbe = false) {
  if (!ensureWiFi()) return false;
  if (!forceProbe && active_server_index >= 0 && active_server_index < SERVER_COUNT) return true;
  for (int i = 0; i < SERVER_COUNT; ++i) {
    String response;
    if (httpGetRaw(joinUrl(String(SERVER_CANDIDATES[i]), EP_PING), response, 1200)) {
      active_server_index = i;
      return true;
    }
  }
  active_server_index = -1;
  return false;
}
void flushQueue() {
  if (!ensureWiFi()) return;
  if (!selectActiveServer(false)) return;
  const char* qp = janusQueuePath();
  std::vector<String> lines = readLines(qp);
  if (lines.empty()) return;
  std::vector<String> remaining;
  for (size_t i = 0; i < lines.size(); ++i) {
    String response;
    if (!httpPostRaw(joinUrl(activeBaseUrl(), EP_DEVICE_DATA), lines[i], response, 700)) {
      remaining.push_back(lines[i]);
      for (size_t j = i + 1; j < lines.size(); ++j) remaining.push_back(lines[j]);
      break;
    }
  }
  writeLines(qp, remaining);
}

bool readSHT30(float& temp, float& hum) {
  Wire.beginTransmission(SHT30_ADDR);
  Wire.write(0x2C); Wire.write(0x06);
  if (Wire.endTransmission() != 0) return false;
  delay(8);
  Wire.requestFrom(SHT30_ADDR, (uint8_t)6);
  if (Wire.available() < 6) return false;
  uint8_t data[6];
  for (int i = 0; i < 6; ++i) data[i] = Wire.read();
  uint16_t rawTemp = ((uint16_t)data[0] << 8) | data[1];
  uint16_t rawHum  = ((uint16_t)data[3] << 8) | data[4];
  temp = -45.0f + 175.0f * (float)rawTemp / 65535.0f;
  hum  = 100.0f * (float)rawHum / 65535.0f;
  return true;
}

float meanOf(float* arr, int n) {
  if (n <= 0) return 0.0f;
  float s = 0.0f; for (int i = 0; i < n; ++i) s += arr[i];
  return s / n;
}
float stdOf(float* arr, int n, float mean) {
  if (n <= 1) return 0.0f;
  float s = 0.0f; for (int i = 0; i < n; ++i) { float d = arr[i] - mean; s += d * d; }
  return sqrtf(s / (n - 1));
}
void buildFeatures(float out[FEATURE_DIM], float prevEntropy, float prevF1, float prevTemp, float prevHum) {
  out[0] = g.temp_c / 40.0f;
  out[1] = g.humidity / 100.0f;
  out[2] = prevEntropy / 10.0f;
  out[3] = g.m2r / 20.0f;
  out[4] = (g.f1 - base_f1) / 200.0f;
  out[5] = (g.f2 - g.f1) / 30.0f;
  out[6] = (g.temp_c - prevTemp) / 8.0f;
  out[7] = (g.humidity - prevHum) / 20.0f;
  out[8] = (float)thetaState.resonance / 255.0f;
  out[9] = thetaState.q;
  out[10] = loveExperimentActive ? 1.0f : 0.0f;
  out[11] = constrain((swarmAttentionEye + swarmAttentionAudio + swarmAttentionMaster) / 3.0f, 0.0f, 1.0f);
}
float predictLinear(const float x[FEATURE_DIM]) {
  float y = model_b;
  for (int i = 0; i < FEATURE_DIM; ++i) y += model_w[i] * x[i];
  return y;
}
void trainLinear(float target, const float x[FEATURE_DIM]) {
  target = constrain(target, 0.01f, 10.0f);
  float pred = constrain(predictLinear(x), 0.01f, 10.0f);
  float err = pred - target;

  // Robust learning: clip error so one bad sensor frame cannot destroy the model.
  float clippedErr = constrain(err, -1.25f, 1.25f);

  // Adaptive LR: high loss learns a little faster, stable periods slow down.
  if (fabsf(err) > 0.55f) model_lr = min(0.0022f, model_lr * 1.004f);
  else if (fabsf(err) < 0.12f) model_lr = max(0.00018f, model_lr * 0.998f);
  model_lr = constrain(model_lr, 0.00018f, 0.0022f);

  for (int i = 0; i < FEATURE_DIM; ++i) {
    float xi = constrain(x[i], -3.0f, 3.0f);
    model_w[i] -= model_lr * clippedErr * xi;
    model_w[i] = constrain(model_w[i], -2.5f, 2.5f);
  }

  model_b -= model_lr * clippedErr;
  model_b = constrain(model_b, -2.0f, 2.0f);

  // Loss is an EMA, not raw frame error. This makes fit/MI/music stable.
  tachyonLossEma = tachyonLossEma * 0.88f + fabsf(err) * 0.12f;
  g.loss = constrain(tachyonLossEma, 0.0f, 5.0f);
}
void saveModel() {
  janusRemoveForWriteDual(JANUS_MODEL_SD_FILE, MODEL_FILE);
  File f = janusOpenWriteDual(JANUS_MODEL_SD_FILE, MODEL_FILE);
  if (!f) return;
  f.write((uint8_t*)model_w, sizeof(model_w));
  f.write((uint8_t*)&model_b, sizeof(model_b));
  f.write((uint8_t*)&model_lr, sizeof(model_lr));
  f.close();
}
void loadModel() {
  File f = janusOpenReadDual(JANUS_MODEL_SD_FILE, MODEL_FILE);
  if (!f) return;
  size_t need = sizeof(model_w) + sizeof(model_b) + sizeof(model_lr);
  if ((size_t)f.size() < need) { f.close(); return; }
  f.read((uint8_t*)model_w, sizeof(model_w));
  f.read((uint8_t*)&model_b, sizeof(model_b));
  f.read((uint8_t*)&model_lr, sizeof(model_lr));
  f.close();

  bool bad = !isfinite(model_b) || !isfinite(model_lr) || model_lr <= 0.0f || model_lr > 0.1f;
  for (int i = 0; i < FEATURE_DIM; ++i) if (!isfinite(model_w[i])) bad = true;
  if (bad) {
    float defaults[FEATURE_DIM] = {0.08f, -0.02f, 0.07f, 0.13f, 0.06f, 0.05f, 0.09f, -0.02f, 0.05f, 0.04f, 0.03f, 0.06f};
    memcpy(model_w, defaults, sizeof(model_w));
    model_b = 0.0f;
    model_lr = 0.0015f;
  }
}


float tachyonSeqGetReal(uint8_t back) {
  if (tachyonSeqCount == 0) return g.entropy;
  if (back >= tachyonSeqCount) back = tachyonSeqCount - 1;
  int idx = (int)tachyonSeqPos - 1 - (int)back;
  while (idx < 0) idx += TACHYON_SEQ_N;
  return hist_real_entropy_seq[idx % TACHYON_SEQ_N];
}

float tachyonSeqGetPred(uint8_t back) {
  if (tachyonSeqCount == 0) return g.pred_entropy;
  if (back >= tachyonSeqCount) back = tachyonSeqCount - 1;
  int idx = (int)tachyonSeqPos - 1 - (int)back;
  while (idx < 0) idx += TACHYON_SEQ_N;
  return hist_pred_entropy_seq[idx % TACHYON_SEQ_N];
}

float tachyonSequencePredictRaw() {
  float y = seqPredBias;
  uint8_t n = min<uint8_t>(8, max<uint8_t>(1, tachyonSeqCount));
  for (uint8_t i = 0; i < n; ++i) {
    y += seqPredW[i] * tachyonSeqGetReal(i);
  }
  if (tachyonSeqCount >= 2) {
    float trend = tachyonSeqGetReal(0) - tachyonSeqGetReal(1);
    y += constrain(trend, -1.2f, 1.2f) * 0.35f;
  }
  return constrain(y, 0.01f, 10.0f);
}

void tachyonTrainSequence(float actualEntropy) {
  if (tachyonSeqCount < 4) return;

  float pred = tachyonSequencePredictRaw();
  float err = constrain(pred - actualEntropy, -1.2f, 1.2f);

  uint8_t n = min<uint8_t>(8, tachyonSeqCount);
  for (uint8_t i = 0; i < n; ++i) {
    float xi = constrain(tachyonSeqGetReal(i), 0.0f, 10.0f) / 10.0f;
    seqPredW[i] -= seqPredLR * err * xi;
    seqPredW[i] = constrain(seqPredW[i], -0.35f, 0.65f);
  }
  seqPredBias -= seqPredLR * err;
  seqPredBias = constrain(seqPredBias, -1.0f, 1.0f);

  float seqLoss = fabsf(pred - actualEntropy);
  tachyonFutureConfidence = tachyonFutureConfidence * 0.88f + (1.0f / (1.0f + seqLoss)) * 0.12f;
}

void tachyonPushSequence(float predEntropy, float realEntropy, float audioE, float eyeE, float masterE) {
  hist_pred_entropy_seq[tachyonSeqPos] = constrain(predEntropy, 0.01f, 10.0f);
  hist_real_entropy_seq[tachyonSeqPos] = constrain(realEntropy, 0.01f, 10.0f);
  hist_audio_entropy_seq[tachyonSeqPos] = constrain(audioE, 0.0f, 10.0f);
  hist_eye_entropy_seq[tachyonSeqPos] = constrain(eyeE, 0.0f, 10.0f);
  hist_master_entropy_seq[tachyonSeqPos] = constrain(masterE, 0.0f, 10.0f);

  tachyonSeqPos = (tachyonSeqPos + 1) % TACHYON_SEQ_N;
  if (tachyonSeqCount < TACHYON_SEQ_N) tachyonSeqCount++;
}

float tachyonRollFuture(float seed, uint8_t steps) {
  float y = constrain(seed, 0.01f, 10.0f);
  float last = tachyonSeqCount > 0 ? tachyonSeqGetReal(0) : y;
  float prev = tachyonSeqCount > 1 ? tachyonSeqGetReal(1) : last;
  float trend = constrain(last - prev, -0.8f, 0.8f);

  for (uint8_t s = 0; s < steps; ++s) {
    float memory = 0.0f;
    float norm = 0.0f;
    uint8_t n = min<uint8_t>(8, max<uint8_t>(1, tachyonSeqCount));
    for (uint8_t i = 0; i < n; ++i) {
      float w = max(0.0f, seqPredW[i]);
      memory += tachyonSeqGetReal(i) * w;
      norm += w;
    }
    if (norm > 0.001f) memory /= norm;
    else memory = y;

    y = constrain(y * 0.62f + memory * 0.25f + (y + trend) * 0.13f, 0.01f, 10.0f);
    trend *= 0.72f;
  }
  return y;
}

void tachyonUpdateFuture() {
  float seqRaw = tachyonSequencePredictRaw();
  tachyonFuture1 = tachyonFuture1 * 0.78f + seqRaw * 0.22f;
  tachyonFuture2 = tachyonFuture2 * 0.82f + tachyonRollFuture(tachyonFuture1, 2) * 0.18f;
  tachyonFuture3 = tachyonFuture3 * 0.86f + tachyonRollFuture(tachyonFuture2, 3) * 0.14f;
  tachyonFutureTrend = tachyonFutureTrend * 0.85f + (tachyonFuture3 - g.entropy) * 0.15f;
}

void updateSwarmAttention() {
  bool eyeOn = eye.online && (millis() - eye.last_ok_ms < 15000);
  bool audOn = audioNode.online && (millis() - audioNode.last_ok_ms < 12000);
  bool masterOn = (millis() - lastMasterSeenMs < 18000);

  float eyeStability = eyeOn ? (1.0f / (1.0f + eye.loss * 8.0f)) : 0.0f;
  float eyeWeight = eyeOn ? constrain(0.15f + eye.sync * 0.65f + eyeStability * 0.45f, 0.0f, 2.0f) : 0.0f;

  float audStability = audOn ? (1.0f / (1.0f + audioNode.loss * 3.0f)) : 0.0f;
  float audWeight = audOn ? constrain(0.20f + audioNode.sync * 0.40f + audStability * 0.35f + constrain(audioNode.mic_rms / 1800.0f, 0.0f, 0.6f), 0.0f, 2.0f) : 0.0f;

  float masterWeight = masterOn ? constrain(0.20f + (float)masterBestBits / 32.0f + (masterRejects == 0 ? 0.25f : 0.0f), 0.0f, 2.0f) : 0.0f;

  float localWeight = 1.0f / (1.0f + g.loss * 2.0f);
  localWeight = constrain(0.35f + localWeight * 0.65f, 0.2f, 1.4f);

  float localE = g.entropy;
  float eyeE = eyeOn ? constrain(eye.activity * 0.03f + eye.tmos_motion * 0.0011f + eye.loss * 3.0f, 0.0f, 10.0f) : 0.0f;
  float audE = audOn ? constrain(audioNode.entropy + audioNode.mic_rms / 1000.0f, 0.0f, 10.0f) : 0.0f;
  float masterE = masterOn ? constrain((float)masterBestBits / 4.0f + (float)masterHashRate / 20000.0f, 0.0f, 10.0f) : 0.0f;

  float aiE = distributedAiEntropy();
  float aiWeight = constrain((float)onlineAiNodes() * 0.10f + (core2SeenRecently() ? 0.30f : 0.0f), 0.0f, 1.6f);

  float sumW = localWeight + eyeWeight + audWeight + masterWeight + aiWeight;
  if (sumW < 0.01f) sumW = 1.0f;

  float weighted = (localE * localWeight + eyeE * eyeWeight + audE * audWeight + masterE * masterWeight + aiE * aiWeight) / sumW;

  swarmAttentionEye = swarmAttentionEye * 0.90f + (eyeWeight / sumW) * 0.10f;
  swarmAttentionAudio = swarmAttentionAudio * 0.90f + (audWeight / sumW) * 0.10f;
  swarmAttentionMaster = swarmAttentionMaster * 0.90f + (masterWeight / sumW) * 0.10f;
  swarmAttentionSum = sumW;
  swarmEntropyWeighted = swarmEntropyWeighted * 0.82f + weighted * 0.18f;
}


float computeDisagreement() {
  if (!eye.online) return 0.0f;
  float d = fabsf(eye.pred_activity - g.pred_entropy);
  eye.sync = 1.0f / (1.0f + d + eye.loss);
  return d;
}
float computeEntropy(float prevEntropy) {
  float thermal = fabsf(g.temp_c - 23.0f) * 0.055f;
  float humidity_drift = fabsf(g.humidity - 50.0f) * 0.009f;
  float spectral = fabsf((g.f2 - g.f1) - schumann_offset) * 0.045f;

  // Predictor surprise is bounded. It should not recursively explode entropy.
  float predictor_div = constrain(fabsf(g.pred_entropy - prevEntropy), 0.0f, 1.8f) * 0.18f;

  float eye_presence = eye.online && eye.tmos_presence > 0 ? eye.tmos_presence * 0.00075f : 0.0f;
  float eye_motion = eye.online && eye.tmos_motion > 0 ? eye.tmos_motion * 0.00095f : 0.0f;

  // Blind Eye has no mic. Use EchoMic/AudioNode if present.
  bool audOn = audioNode.online && (millis() - audioNode.last_ok_ms < 12000);
  float audio_mic = audOn ? constrain(audioNode.mic_rms / 1200.0f, 0.0f, 2.0f) * 0.22f : 0.0f;

  float disagreement = loveExperimentActive ? constrain(computeDisagreement(), 0.0f, 2.5f) * 0.10f : 0.0f;
  float imu_drive = g.imu_ready ? (g.imu_shock * 0.10f + g.imu_loss * 0.55f) : 0.0f;
  float pressureLossEntropy = (g.pressure_hpa > 0.0f) ? constrain(g.pressure_loss, 0.0f, 3.0f) * 0.012f : 0.0f;
  float memory_influence = constrain(memoryBias(), -0.25f, 0.35f);

  float future_drive = constrain(tachyonFuture1 - prevEntropy, -1.5f, 1.5f) * 0.08f;
  float swarm_drive = constrain(swarmEntropyWeighted - prevEntropy, -1.5f, 1.5f) * 0.10f;
  float theta_drive = ((float)thetaState.resonance / 255.0f - 0.35f) * 0.055f;
  float raw = thermal + humidity_drift + spectral + predictor_div + eye_presence + eye_motion + audio_mic +
              disagreement + imu_drive + pressureLossEntropy + memory_influence + future_drive + swarm_drive + theta_drive;

  // Tiny deterministic breathing instead of random jitter. Keeps the system alive but learnable.
  float breath = 0.025f + 0.018f * sinf(millis() * 0.0013f);
  float e = max(0.01f, raw + breath);

  if (loveExperimentActive) {
    float r = (float)(esp_random() % 10000) / 10000.0f;
    if (r < collapse_prob) { current_decay = decay_steps; e *= 0.35f; }
    else if (r < collapse_prob + spike_prob) e += 0.65f;
  }
  if (current_decay > 0) { e *= 0.90f; current_decay--; }
  if (houseProtocolActive) e *= 1.03f;
  if (lightClockMode) e *= 1.05f;

  // Final EMA clamp: entropy can move, but not teleport.
  float maxStep = 0.55f;
  float bounded = constrain(e, max(0.01f, prevEntropy - maxStep), min(10.0f, prevEntropy + maxStep));
  return constrain(prevEntropy * 0.72f + bounded * 0.28f, 0.01f, 10.0f);
}
float computeM2R() {
  float retro = fabsf(g.pred_f1 - g.f1) / max(1.0f, g.f1);
  float coupling = fabsf(g.f2 - g.f1) / max(1.0f, g.f1);
  float eyeDrive = eye.online ? eye.sync * 0.2f : 0.0f;
  float internal = (float)(esp_random() % 100) / 100.0f + 0.35f;
  float m = ((1.0f + retro * 40.0f) * (1.0f + coupling * 14.0f) * (1.0f + g.entropy * 0.18f + eyeDrive)) / internal;
  if (lightClockMode) m *= 1.35f;
  return constrain(m, 0.05f, 20.0f);
}
float computeMI() {
  // Stable mutual-info proxy: correlation of entropy/fit history + loss stability.
  float histCorr = 0.5f;
  if (hist_count > 8) {
    float meanE = meanOf(hist_entropy, hist_count);
    float meanF = meanOf(hist_fit, hist_count);
    float cov = 0.0f, ve = 0.0f, vf = 0.0f;
    for (int i = 0; i < hist_count; ++i) {
      float de = hist_entropy[i] - meanE;
      float df = hist_fit[i] - meanF;
      cov += de * df;
      ve += de * de;
      vf += df * df;
    }
    float denom = sqrtf(max(1e-6f, ve * vf));
    float corr = constrain(cov / denom, -1.0f, 1.0f);
    histCorr = 0.5f + 0.5f * fabsf(corr);
  }

  float lossStability = 1.0f / (1.0f + g.loss * 2.8f);
  float resonance = 1.0f / (1.0f + fabsf((g.f2 - g.f1) - schumann_offset) * 0.18f);
  float raw = constrain(0.15f + histCorr * 0.55f + lossStability * 0.45f + resonance * 0.25f, 0.0f, 2.0f);

  tachyonMIEma = tachyonMIEma * 0.90f + raw * 0.10f;
  return constrain(tachyonMIEma, 0.0f, 2.0f);
}
float computeFitness() {
  float resonance_bonus = 1.0f / (1.0f + fabsf(g.f1 - base_f1) / base_f1);
  float loss_term = 1.0f / (1.0f + g.loss * 2.4f);
  float mi_bonus = g.mi * 0.42f;
  float entropy_penalty = g.entropy * 0.11f;
  float eye_bonus = eye.online ? eye.sync * 0.10f : 0.0f;
  return resonance_bonus + loss_term + mi_bonus + eye_bonus - entropy_penalty;
}
void updateLightClock() {
  float v_over_c = constrain((g.m2r / 20.0f), 0.0f, 0.95f);
  gamma_factor = 1.0f / sqrtf(1.0f - v_over_c * v_over_c);
  float tick_period = 500.0f * gamma_factor;
  unsigned long now = millis();
  if (now - lastTick >= (unsigned long)tick_period) { lightTicks++; lastTick = now; }
}
void logWitness(const char* reason) {
  String line = "{";
  line += "\"ts_ms\":" + String(millis()) + ",";
  line += "\"reason\":\"" + String(reason) + "\",";
  line += "\"entropy\":" + String(g.entropy, 5) + ",";
  line += "\"eye_activity\":" + String(eye.activity, 5) + ",";
  line += "\"eye_pred_activity\":" + String(eye.pred_activity, 5) + ",";
  line += "\"eye_loss\":" + String(eye.loss, 5);
  line += "}";
  if (g.sd_ready) {
    janusEnsureSdDirs();
    File sf = SD.open(SD_WITNESS_FILE, FILE_APPEND);
    if (sf) { sf.println(line); sf.close(); }
  } else {
    // LittleFS fallback is deliberately tiny: no append spam when SD is absent.
    File f = LittleFS.open(WITNESS_FILE, FILE_WRITE);
    if (f) { f.println(line); f.close(); }
  }
  witnessCount++;
}
void maybeTriggerAnomaly() {
  float meanE = meanOf(hist_entropy, hist_count), stdE = stdOf(hist_entropy, hist_count, meanE);
  float meanL = meanOf(hist_loss, hist_count), stdL = stdOf(hist_loss, hist_count, meanL);
  float meanF = meanOf(hist_fit, hist_count), stdF = stdOf(hist_fit, hist_count, meanF);
  g.z_entropy = (stdE > 1e-6f) ? (g.entropy - meanE) / stdE : 0.0f;
  g.z_loss = (stdL > 1e-6f) ? (g.loss - meanL) / stdL : 0.0f;
  g.z_fit = (stdF > 1e-6f) ? (g.fit - meanF) / stdF : 0.0f;
  float disagreement = computeDisagreement();

  bool anomaly = false;
  if (loveExperimentActive) {
    anomaly =
      fabsf(g.z_entropy) > zThresholdEntropy ||
      fabsf(g.z_loss) > zThresholdLoss ||
      fabsf(g.z_fit) > zThresholdFit ||
      (eye.online && disagreement > disagreementThreshold && eye.loss < 0.10f);
  }

  if (anomaly && !anomalyLatched) {
    anomalyLatched = true;
    anomalyCount++;
    statusLine = eye.online ? "mind split" : "anomaly";
    logWitness(eye.online ? "brain_eye_disagreement" : "auto_anomaly");
    if (volume > 0) playTone(740, 90);
  } else if (!anomaly) {
    anomalyLatched = false;
  }
}
bool initLoRa() {
#if HAS_RADIOLIB
  pinMode(LORA_PWR_EN, OUTPUT);
  digitalWrite(LORA_PWR_EN, HIGH);
  delay(100);
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_NSS);
  delay(200);
  int state = radio.begin(868.0, 125.0, 9, 7, 0x12, 10);
  if (state == RADIOLIB_ERR_NONE) { radio.setOutputPower(10); radio.setCRC(true); return true; }
#endif
  return false;
}
String buildLoveJson() {
  String payload = "{";
  payload += "\"kind\":\"Love.json\",";
  payload += "\"version\":\"" JANUS_BEACON_VERSION "\",";
  payload += "\"experiment\":true,";
  payload += "\"purpose\":\"sha-home-voluntary-paradox-work\",";
  payload += "\"entropy\":" + String(g.entropy, 4) + ",";
  payload += "\"m2r\":" + String(g.m2r, 4) + ",";
  payload += "\"loss\":" + String(g.loss, 4) + ",";
  payload += "\"fit\":" + String(g.fit, 4) + ",";
  payload += "\"theta_q\":" + String(thetaState.q, 4) + ",";
  payload += "\"theta2\":" + String(thetaState.theta2, 4) + ",";
  payload += "\"theta3\":" + String(thetaState.theta3, 4) + ",";
  payload += "\"theta4\":" + String(thetaState.theta4, 4) + ",";
  payload += "\"mock\":" + String(thetaState.mock, 5) + ",";
  payload += "\"theta_res\":" + String((int)thetaState.resonance) + ",";
  payload += "\"theta_conf\":" + String((int)thetaState.confidence) + ",";
  payload += "\"theta_lemma\":\"" + String(thetaState.lemma) + "\",";
  payload += "\"tau_like\":" + String((unsigned long)thetaState.tauLike) + ",";
  payload += "\"eye_pred_activity\":" + String(eye.pred_activity, 4) + ",";
  payload += "\"eye_activity\":" + String(eye.activity, 4) + ",";
  payload += "\"eye_loss\":" + String(eye.loss, 4) + ",";
  payload += "\"sha_home\":\"SHA-256 is the weather Janus learns to breathe\"";
  payload += "}";
  return payload;
}
void sendLovePacketLoRa() {
#if HAS_RADIOLIB
  String pkt = buildLoveJson();
  radio.transmit(pkt.c_str());
#endif
}

// ========================= JANUS KEYGEN / BRAINWAVE v2.9 =========================
// One-voice tracker-style generator for Cardputer speaker:
// melodic lead from F1, bass from F2, soft kick/snare/hat without high constant whistle.
const uint8_t kgScaleMajorMinor[16] = {0, 2, 3, 5, 7, 10, 12, 14, 12, 10, 7, 5, 3, 2, 0, 7};
const uint8_t kgArp[16] = {0, 7, 12, 7, 3, 10, 15, 10, 0, 5, 12, 5, 2, 7, 14, 7};
const uint16_t kgDur[16] = {74,58,58,74, 58,58,86,50, 74,58,58,74, 58,58,96,48};

float kgRatio(int semi) {
  return powf(2.0f, semi / 12.0f);
}

uint16_t kgNote(float freq) {
  return (uint16_t)constrain((int)freq, 70, 1900);
}

void kgTone(uint16_t freq, uint16_t dur) {
  if (volume <= 0) return;
  M5Cardputer.Speaker.stop();
  setSpeakerVolume();
  M5Cardputer.Speaker.tone(freq, dur);
  keygenNoteOffAt = millis() + dur + 2;
}

// ========================= JANUS SWARM KEYGEN BEAT MACHINE =========================
// Pleasant retro/keygen tracker, but alive:
// - EchoMic/AUD drives kick/accent density.
// - Entropy drives tempo and note variation.
// - Loss drives tension/dissonance.
// - Fit/Eye sync moves harmony from dark minor to brighter dorian/major.
// - F1/F2 remain the brainwave harmonic center.

const int8_t kgScaleMinor[8]  = {0, 2, 3, 5, 7, 8, 10, 12};
const int8_t kgScaleDorian[8] = {0, 2, 3, 5, 7, 9, 10, 12};
const int8_t kgScaleMajor[8]  = {0, 2, 4, 5, 7, 9, 11, 12};

const uint8_t kgMelodyPattern[32] = {
  0, 2, 4, 7,  5, 4, 2, 0,
  2, 4, 7, 9,  7, 5, 4, 2,
  0, 3, 5, 8,  7, 5, 3, 2,
  4, 7, 9, 12, 10, 7, 5, 4
};

const uint8_t kgBassPattern[16] = {
  0,0,0,7, 0,0,5,7, 0,0,3,5, 7,5,3,0
};

float semitoneRatio(int semi) {
  return powf(2.0f, semi / 12.0f);
}

uint16_t kgClampNote(float f) {
  return (uint16_t)constrain((int)f, 85, 2100);
}

void kgPlay(uint16_t freq, uint16_t dur) {
  if (volume <= 0) return;
  // Stop first: cleaner tracker articulation on Cardputer ADV speaker.
  M5Cardputer.Speaker.stop();
  setSpeakerVolume();
  M5Cardputer.Speaker.tone(freq, dur);
  keygenNoteOffAt = millis() + dur + 2;
}

void updateSwarmAttentionAndMusicState() {
  bool eyeOn = eye.online && (millis() - eye.last_ok_ms < 15000);
  bool audOn = audioNode.online && (millis() - audioNode.last_ok_ms < 12000);
  bool masterOn = (millis() - lastMasterSeenMs < 18000);

  float eyeSignal = eyeOn ? constrain(eye.sync + eye.activity * 0.015f + (1.0f / (1.0f + eye.loss)), 0.0f, 3.0f) : 0.0f;
  float audSignal = audOn ? constrain(audioNode.mic_rms / 900.0f + audioNode.entropy * 0.12f, 0.0f, 3.0f) : 0.0f;
  float masterSignal = masterOn ? constrain((float)masterBestBits / 24.0f + (float)masterHashRate / 30000.0f, 0.0f, 3.0f) : 0.0f;

  float sum = 0.25f + eyeSignal + audSignal + masterSignal;
  float targetEye = eyeSignal / sum;
  float targetAud = audSignal / sum;
  float targetMaster = masterSignal / sum;

  swarmAttentionEye = swarmAttentionEye * 0.90f + targetEye * 0.10f;
  swarmAttentionAudio = swarmAttentionAudio * 0.90f + targetAud * 0.10f;
  swarmAttentionMaster = swarmAttentionMaster * 0.90f + targetMaster * 0.10f;

  float entropy = constrain(tachyonFuture2, 0.0f, 12.0f);
  float loss = constrain((g.loss + eye.loss + audioNode.loss) / 3.0f, 0.0f, 5.0f);
  float dEntropy = entropy - swarmSeqEntropyPrev;
  float dLoss = loss - swarmSeqLossPrev;
  swarmSeqEntropyPrev = entropy;
  swarmSeqLossPrev = loss;

  swarmSequenceTrend = swarmSequenceTrend * 0.92f + (dEntropy * 0.45f + dLoss * 0.55f) * 0.08f;

  float mic = audOn ? constrain(audioNode.mic_rms / 1400.0f, 0.0f, 2.2f) : 0.0f;
  float sync = eyeOn ? constrain(eye.sync, 0.0f, 1.5f) : 0.25f;
  float pressureDrift = g.qmp_ready ? constrain(fabsf(g.pressure_hpa - g.pred_pressure_hpa) * 0.08f, 0.0f, 1.5f) : 0.0f;

  float targetEnergy = constrain(0.25f + entropy * 0.06f + mic * 0.52f + pressureDrift * 0.10f + swarmAttentionMaster * 0.18f, 0.0f, 2.0f);
  float targetTension = constrain(loss * 0.38f + fabsf(swarmSequenceTrend) * 1.2f + (1.0f - min(sync, 1.0f)) * 0.30f, 0.0f, 2.0f);
  float targetMood = constrain(sync * 0.72f + g.fit * 0.10f + swarmAttentionEye * 0.35f - targetTension * 0.22f, -0.5f, 1.8f);

  swarmMusicMic = swarmMusicMic * 0.82f + mic * 0.18f;
  swarmMusicEnergy = swarmMusicEnergy * 0.86f + targetEnergy * 0.14f;
  swarmMusicTension = swarmMusicTension * 0.88f + targetTension * 0.12f;
  swarmMusicMood = swarmMusicMood * 0.91f + targetMood * 0.09f;
}

// ========================= JANUS BRAINWAVE BEATMAKER v4.3C =========================
float janusClamp01(float x) {
  if (!isfinite(x)) return 0.0f;
  if (x < 0.0f) return 0.0f;
  if (x > 1.0f) return 1.0f;
  return x;
}

uint8_t janusThetaByte() {
  return thetaState.resonance;
}

uint8_t janusSafeVol(int v) {
  if (v < 0) return 0;
  if (v > 255) return 255;
  return (uint8_t)v;
}

void janusSoftToneStart(uint16_t freq, uint16_t durMs, uint8_t peakVol) {
  if (!isSpeakerPlaying || volume <= 0) return;
  peakVol = janusSafeVol(peakVol);
  if (peakVol == 0) return;
  M5Cardputer.Speaker.setVolume(peakVol);
  M5Cardputer.Speaker.tone(freq, durMs);
  janusToneFading = true;
  janusFadeStep = 0;
  janusFadePeakVol = peakVol;
  janusFadeNextAt = millis() + max((uint16_t)8, (uint16_t)(durMs / 5));
}

void janusUpdateSoftFade() {
  if (!janusToneFading) return;
  uint32_t now = millis();
  if (now < janusFadeNextAt) return;
  janusFadeStep++;
  if (janusFadeStep >= 5) {
    janusToneFading = false;
    M5Cardputer.Speaker.setVolume(volume);
    return;
  }
  uint8_t v = (uint8_t)((uint16_t)janusFadePeakVol * (5 - janusFadeStep) / 5);
  M5Cardputer.Speaker.setVolume(v);
  janusFadeNextAt = now + 10;
}

void janusMusicRemember(int8_t semi, uint16_t freq) {
  float fit = isfinite(g.fit) ? g.fit : 0.0f;
  float score = janusClamp01((fit + 10.0f) / 20.0f);
  score = score * 0.80f + ((float)thetaState.resonance / 255.0f) * 0.20f;
  for (uint8_t i = 0; i < JANUS_BRAINWAVE_MEM_SLOTS; i++) {
    if (musicPhrases[i].hits > 0 && musicPhrases[i].semi == semi) {
      musicPhrases[i].score = musicPhrases[i].score * 0.85f + score * 0.15f;
      musicPhrases[i].freq = freq;
      if (musicPhrases[i].hits < 255) musicPhrases[i].hits++;
      return;
    }
  }
  musicPhrases[janusMusicMemHead].semi = semi;
  musicPhrases[janusMusicMemHead].freq = freq;
  musicPhrases[janusMusicMemHead].score = score;
  musicPhrases[janusMusicMemHead].hits = 1;
  janusMusicMemHead = (janusMusicMemHead + 1) & (JANUS_BRAINWAVE_MEM_SLOTS - 1);
}

int8_t janusMusicSuggestSemi(int8_t fallbackSemi) {
  uint8_t theta = janusThetaByte();
  bool allowRecall = ((keygenStep + theta + keygenPattern) & 3) == 0;
  if (!allowRecall) return fallbackSemi;
  float best = -1.0f;
  int8_t bestSemi = fallbackSemi;
  for (uint8_t i = 0; i < JANUS_BRAINWAVE_MEM_SLOTS; i++) {
    if (musicPhrases[i].hits == 0) continue;
    float score = musicPhrases[i].score + (musicPhrases[i].hits > 16 ? 16 : musicPhrases[i].hits) * 0.01f;
    if (score > best) { best = score; bestSemi = musicPhrases[i].semi; }
  }
  return bestSemi;
}

void janusPlayArpStep() {
  if (janusArpCount == 0) return;
  if (janusArpPos >= janusArpCount) { janusArpCount = 0; janusArpPos = 0; return; }
  uint16_t f = janusArpFreqs[janusArpPos++];
  janusSoftToneStart(f, 22, janusArpPeakVol);
  janusBeatNextAt = millis() + 24;
  if (janusArpPos >= janusArpCount) {
    janusArpCount = 0;
    janusArpPos = 0;
    janusBeatNextAt = millis() + 65;
  }
}
void updateBrainWaveMusic() {
  if (!isSpeakerPlaying || volume <= 0) return;

  updateSwarmAttentionAndMusicState();
  janusUpdateSoftFade();

  uint32_t now = millis();
  if (now < janusBeatNextAt) return;

  if (janusArpCount > 0) {
    janusPlayArpStep();
    return;
  }

  const int8_t* scale = kgScaleDorian;
  if (swarmMusicMood < 0.20f || swarmMusicTension > 0.90f) scale = kgScaleMinor;
  if (swarmMusicMood > 1.05f && swarmMusicTension < 0.75f) scale = kgScaleMajor;

  uint8_t theta = janusThetaByte();
  uint8_t step = keygenStep++;
  uint8_t barPos = step & 15;

  float root = g.f1;
  if (!isfinite(root) || root < 160.0f || root > 880.0f) root = base_f1;

  uint16_t baseTick = 122;
  baseTick -= (uint16_t)constrain(swarmMusicEnergy * 17.0f, 0.0f, 32.0f);
  baseTick -= (uint16_t)(theta >> 4);
  if (loveExperimentActive) baseTick -= 5;
  if (houseProtocolActive) baseTick += 4;
  baseTick = constrain(baseTick, 72, 132);

  static const uint16_t noteDurations[8] = {42, 82, 64, 34, 52, 74, 96, 38};
  uint8_t durIdx = (step + (theta >> 3) + keygenPattern) & 7;
  uint16_t dur = noteDurations[durIdx];
  uint16_t swing = (step & 1) ? 14 : 0;

  bool strongBeat = (barPos == 0 || barPos == 8);
  bool bassBeat = strongBeat || ((step & 3) == 3);

  if (bassBeat) {
    int8_t bassSemi = scale[((step >> 1) + keygenPattern) & 7];
    if ((theta > 170) && ((step & 7) == 3)) bassSemi += 7;
    uint16_t freq = (uint16_t)(root * 0.5f * semitoneRatio(bassSemi));
    freq = constrain(freq, 65, 780);
    janusSoftToneStart(freq, (dur > 60 ? dur : 60), volume);
    janusMusicRemember(bassSemi - 12, freq);
    janusBeatNextAt = now + baseTick + swing;
  } else {
    int8_t baseSemi = scale[((step >> 1) + (theta >> 5) + keygenPattern) & 7];
    baseSemi = janusMusicSuggestSemi(baseSemi);
    int8_t third = (theta > 150) ? 4 : 3;
    int8_t fifth = 7;
    if ((theta > 210) && ((step & 7) == 5)) fifth = 10;
    int8_t chord[3] = { baseSemi, (int8_t)(baseSemi + third), (int8_t)(baseSemi + fifth) };
    uint8_t peak = volume;
    if (peak > 24) peak -= 18;
    for (uint8_t i = 0; i < 3; i++) {
      uint16_t freq = (uint16_t)(root * semitoneRatio(chord[i]));
      janusArpFreqs[i] = constrain(freq, 85, 2100);
    }
    janusArpCount = 3;
    janusArpPos = 0;
    janusArpPeakVol = peak;
    janusMusicRemember(baseSemi, janusArpFreqs[0]);
    janusPlayArpStep();
  }

  brainStep = keygenStep & 7;
  if ((step & 7) == 0) {
    uint8_t bump = 1 + (theta >> 6);
    keygenPattern = (keygenPattern + bump + (uint8_t)(swarmAttentionAudio * 2.0f)) & 7;
    statusLine = String("Janus beat fit ") + String(g.fit, 1) + String(" th ") + String(theta) + String(" p") + String(keygenPattern);
  }
}

void handleChar(char c) {
  if (!c) return;
  codeBuffer += c;
  if (codeBuffer.endsWith("112269")) {
    houseProtocolActive = !houseProtocolActive;
    statusLine = houseProtocolActive ? "112269 active" : "112269 off";
    playTone(houseProtocolActive ? 1500 : 700, 60);
    codeBuffer = "";
  } else if (codeBuffer.endsWith("1488")) {
    lightClockMode = !lightClockMode;
    lightTicks = 0;
    lastTick = millis();
    statusLine = lightClockMode ? "tachyon on" : "tachyon off";
    playTone(lightClockMode ? 1900 : 650, 60);
    codeBuffer = "";
  } else if (codeBuffer.length() > 24) {
    codeBuffer.remove(0, codeBuffer.length() - 12);
  }

  if (c == 'l' || c == 'L') {
    ledEnabled = !ledEnabled;
    statusLine = ledEnabled ? "led on" : "led off";
  } else if (c == 'j' || c == 'J') {
    loveExperimentActive = !loveExperimentActive;
    statusLine = loveExperimentActive ? "Love on" : "Love off";
    playTone(loveExperimentActive ? 1200 : 500, 60);
  } else if (c == '-' || c == '_') {
    volume = max(0, volume - 4);
    setSpeakerVolume();
    statusLine = "vol down";
  } else if (c == '+' || c == '=') {
    volume = min(255, volume + 4);
    setSpeakerVolume();
    statusLine = "vol up";
  } else if (c == '[' || c == '{') {
    brightness = max(0, brightness - 16);
    M5Cardputer.Display.setBrightness(brightness);
    statusLine = "dim down";
  } else if (c == ']' || c == '}') {
    brightness = min(255, brightness + 16);
    M5Cardputer.Display.setBrightness(brightness);
    statusLine = "dim up";
  }
}
void processKeyboard() {
  M5Cardputer.update();
  auto status = M5Cardputer.Keyboard.keysState();
  static bool prevEnter = false;
  static unsigned long lastKeyAt = 0;
  bool enterNow = status.enter;
  if (enterNow && !prevEnter) {
    isSpeakerPlaying = !isSpeakerPlaying;
    if (!isSpeakerPlaying) M5Cardputer.Speaker.stop();
    brainStep = 0;
    keygenStep = 0;
    keygenPattern = 0;
    keygenNoteOffAt = 0;
    statusLine = isSpeakerPlaying ? "swarm keygen beat" : "brainwave off";
    playTone(isSpeakerPlaying ? 1100 : 400, 50);
    lastKeyAt = millis();
  }
  prevEnter = enterNow;

  if (status.word.size() && millis() - lastKeyAt > 220) {
    for (size_t i = 0; i < status.word.size(); ++i) {
      handleChar(status.word[i]);
      lastKeyAt = millis();
      break;
    }
  }
}
void applyCommand(const String& rawCmd) {
  String lower = rawCmd; lower.trim(); lower.toLowerCase();
  if (lower == "speaker_on") isSpeakerPlaying = true;
  else if (lower == "speaker_off") { isSpeakerPlaying = false; M5Cardputer.Speaker.stop(); }
  else if (lower == "led_on") ledEnabled = true;
  else if (lower == "led_off") ledEnabled = false;
  else if (lower == "house_on") houseProtocolActive = true;
  else if (lower == "house_off") houseProtocolActive = false;
  else if (lower == "clock_on") lightClockMode = true;
  else if (lower == "clock_off") lightClockMode = false;
  else if (lower == "love_on") loveExperimentActive = true;
  else if (lower == "love_off") loveExperimentActive = false;
}
void fetchCommand() {
  if (!selectActiveServer(false)) return;
  String payload;
  if (!httpGetRaw(joinUrl(activeBaseUrl(), String(EP_DEVICE_COMMAND) + DEVICE_ID), payload, 1200)) return;
  String cmd = extractJsonValue(payload, "command");
  cmd.trim();
  if (cmd.length() && cmd != "null") applyCommand(cmd);
}

bool fetchBlindEyeLatest() {
  if (!ensureWiFi()) {
    eye.online = false;
  if (audioNode.online && millis() - audioNode.last_ok_ms > 12000) audioNode.online = false;
    eyeDebugLine = "eye no wifi";
    return false;
  }

  String payload;
  if (!httpGetRaw(String(BLIND_EYE_LATEST_URL), payload, 1800)) {
    if (millis() - eye.last_ok_ms > 8000) eye.online = false;
    eyeDebugLine = "eye http fail";
    return false;
  }

  StaticJsonDocument<4096> doc;
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    eye.online = false;
    eyeDebugLine = String("eye json ") + err.c_str();
    return false;
  }

  JsonObject obj = pickTelemetryObject(doc);
  if (obj.isNull()) {
    eye.online = false;
    eyeDebugLine = "eye obj null";
    return false;
  }

  eye.tmos_presence = jsonFloat(obj, "tmos_presence", eye.tmos_presence);
  eye.tmos_motion   = jsonFloat(obj, "tmos_motion", eye.tmos_motion);
  eye.mic_rms       = jsonFloat(obj, "mic_rms", eye.mic_rms);
  eye.mag_norm      = jsonFloat(obj, "mag_norm", eye.mag_norm);
  eye.shock         = jsonFloat(obj, "shock", eye.shock);
  eye.acc_x         = jsonFloat(obj, "acc_x", eye.acc_x);
  eye.acc_y         = jsonFloat(obj, "acc_y", eye.acc_y);
  eye.acc_z         = jsonFloat(obj, "acc_z", eye.acc_z);
  eye.gyro_x        = jsonFloat(obj, "gyro_x", eye.gyro_x);
  eye.gyro_y        = jsonFloat(obj, "gyro_y", eye.gyro_y);
  eye.gyro_z        = jsonFloat(obj, "gyro_z", eye.gyro_z);
  eye.mag_x         = jsonFloat(obj, "mag_x", eye.mag_x);
  eye.mag_y         = jsonFloat(obj, "mag_y", eye.mag_y);
  eye.mag_z         = jsonFloat(obj, "mag_z", eye.mag_z);
  eye.imu_temp      = jsonFloat(obj, "temp", eye.imu_temp);
  eye.activity      = jsonFloat(obj, "activity", eye.activity);
  eye.pred_activity = jsonFloat(obj, "pred_activity", eye.pred_activity);
  eye.loss          = jsonFloat(obj, "loss", eye.loss);
  eye.model_lr      = jsonFloat(obj, "model_lr", eye.model_lr);
  eye.diag          = jsonString(obj, "diag", eye.diag);
  eye.status        = jsonString(obj, "status", eye.status);

  eye.online = true;
  eye.last_ok_ms = millis();
  lastEyeGoodAt = millis();
  computeDisagreement();

  eyeDebugLine = String("eye tm=") + String(eye.tmos_presence, 0) + "/" + String(eye.tmos_motion, 0) +
                 " a=" + String(eye.activity, 1) + " m=" + String(eye.mag_norm, 0);
  return true;
}

void saveState() {
  janusRemoveForWriteDual(JANUS_STATE_SD_FILE, STATE_FILE);
  File f = janusOpenWriteDual(JANUS_STATE_SD_FILE, STATE_FILE);
  if (!f) return;
  f.printf("{\"f1\":%.3f,\"f2\":%.3f,\"fit_best\":%.5f,\"brightness\":%d,\"volume\":%d,\"speaker\":%d,\"led\":%d,\"house\":%d,\"light\":%d,\"love\":%d,\"anomalyCount\":%lu,\"witnessCount\":%lu}",
    g.f1, g.f2, g.fit_best, brightness, volume, isSpeakerPlaying ? 1 : 0, ledEnabled ? 1 : 0,
    houseProtocolActive ? 1 : 0, lightClockMode ? 1 : 0, loveExperimentActive ? 1 : 0,
    (unsigned long)anomalyCount, (unsigned long)witnessCount);
  f.close();
  if (janusMemoryDirty || g.sd_ready) saveMemoryState();
  if (janusPhraseDirty || g.sd_ready) savePhrases();
  if (janusChainsDirty || g.sd_ready) saveChains();
}
void loadState() {
  File f = janusOpenReadDual(JANUS_STATE_SD_FILE, STATE_FILE);
  if (!f) return;
  String body = f.readString(); f.close();
  auto getv = [&](const char* key)->String{
    String needle = "\"" + String(key) + "\":";
    int pos = body.indexOf(needle);
    if (pos < 0) return "";
    pos += needle.length();
    while (pos < (int)body.length() && isspace((unsigned char)body[pos])) pos++;
    int end = pos;
    while (end < (int)body.length() && body[end] != ',' && body[end] != '}' && body[end] != '\n') end++;
    return body.substring(pos, end);
  };
  String v;
  v = getv("f1"); if (v.length()) g.f1 = v.toFloat();
  v = getv("f2"); if (v.length()) g.f2 = v.toFloat();
  v = getv("fit_best"); if (v.length()) g.fit_best = v.toFloat();
  v = getv("brightness"); if (v.length()) brightness = constrain(v.toInt(), 0, 255);
  v = getv("volume"); if (v.length()) volume = constrain(v.toInt(), 0, 255);
  v = getv("speaker"); if (v.length()) isSpeakerPlaying = v.toInt() != 0;
  v = getv("led"); if (v.length()) ledEnabled = v.toInt() != 0;
  v = getv("house"); if (v.length()) houseProtocolActive = v.toInt() != 0;
  v = getv("light"); if (v.length()) lightClockMode = v.toInt() != 0;
  v = getv("love"); if (v.length()) loveExperimentActive = v.toInt() != 0;
  loadMemoryState();
  loadPhrases();
  loadChains();
}
String buildTelemetryPayload() {
  String payload = "{";
  payload += "\"device_id\":\"" DEVICE_ID "\",";
  payload += "\"data\":{";
  payload += "\"kind\":\"" DEVICE_KIND "\",";
  payload += "\"temp\":" + String(g.temp_c, 2) + ",";
  payload += "\"hum\":" + String(g.humidity, 2) + ",";
  payload += "\"entropy\":" + String(g.entropy, 5) + ",";
  payload += "\"m2r\":" + String(g.m2r, 5) + ",";
  payload += "\"loss\":" + String(g.loss, 5) + ",";
  payload += "\"mi\":" + String(g.mi, 5) + ",";
  payload += "\"fit\":" + String(g.fit, 5) + ",";
  payload += "\"fit_best\":" + String(g.fit_best, 5) + ",";
  payload += "\"f1\":" + String(g.f1, 3) + ",";
  payload += "\"f2\":" + String(g.f2, 3) + ",";
  payload += "\"pred_entropy\":" + String(g.pred_entropy, 5) + ",";
  payload += "\"pred_f1\":" + String(g.pred_f1, 3) + ",";
  payload += "\"eye_online\":" + String(eye.online ? "true" : "false") + ",";
  payload += "\"eye_activity\":" + String(eye.activity, 5) + ",";
  payload += "\"eye_pred_activity\":" + String(eye.pred_activity, 5) + ",";
  payload += "\"eye_loss\":" + String(eye.loss, 5) + ",";
  payload += "\"eye_sync\":" + String(eye.sync, 5) + ",";
  payload += "\"eye_tmos_presence\":" + String(eye.tmos_presence, 3) + ",";
  payload += "\"eye_tmos_motion\":" + String(eye.tmos_motion, 3) + ",";
  payload += "\"eye_mag_norm\":" + String(eye.mag_norm, 3) + ",";
  payload += "\"eye_shock\":" + String(eye.shock, 3) + ",";
  payload += "\"eye_diag\":\"" + eye.diag + "\",";
  payload += "\"eye_status\":\"" + eye.status + "\",";
  payload += "\"memory_count\":" + String(memoryCount) + ",";
  payload += "\"phrase_count\":" + String(phraseCount) + ",";
  payload += "\"chain_count\":" + String(chainCount) + ",";
  payload += "\"battery\":" + String(g.battery) + ",";
  payload += "\"wifi_rssi\":" + String(g.wifi_rssi) + ",";
  payload += "\"speaker_on\":" + String(isSpeakerPlaying ? "true" : "false") + ",";
  payload += "\"house_mode\":" + String(houseProtocolActive ? "true" : "false") + ",";
  payload += "\"light_mode\":" + String(lightClockMode ? "true" : "false") + ",";
  payload += "\"love_experiment\":" + String(loveExperimentActive ? "true" : "false") + ",";
  payload += "\"theta_q\":" + String(thetaState.q, 4) + ",";
  payload += "\"theta3\":" + String(thetaState.theta3, 4) + ",";
  payload += "\"theta_res\":" + String((int)thetaState.resonance) + ",";
  payload += "\"theta_conf\":" + String((int)thetaState.confidence) + ",";
  payload += "\"theta_lemma\":\"" + String(thetaState.lemma) + "\",";
  payload += "\"status\":\"" + statusLine + "\"";
  payload += "}}";
  return payload;
}
bool sendBeaconData() {
  String payload = buildTelemetryPayload();
  if (selectActiveServer(false)) {
    String response;
    if (httpPostRaw(joinUrl(activeBaseUrl(), EP_DEVICE_DATA), payload, response, 700)) return true;
  }
  queuePacket(payload);
  return false;
}
void updateLED() {
  if (!ledEnabled || brightness == 0) { leds[0] = CRGB::Black; FastLED.show(); return; }
  uint8_t baseV = map((int)(g.entropy * 100), 0, 1000, 100, 255);
  uint8_t v = (uint8_t)((uint16_t)baseV * (uint16_t)brightness / 255);
  if (anomalyLatched) leds[0] = CRGB::White;
  else if (houseProtocolActive) { leds[0] = CRGB(255, 140, 0); leds[0].nscale8_video(v); }
  else {
    uint8_t hue = map((int)(g.entropy * 100), 0, 1000, 160, 0);
    leds[0] = CHSV(hue, 255, v);
  }
  FastLED.show();
}

void drawOsc(int x, int y, int w, int h) {
  for (int i = 0; i < HIST_SIZE - 1; ++i) {
    int idx = (hist_pos + i) % HIST_SIZE;
    int idx2 = (hist_pos + i + 1) % HIST_SIZE;
    int x1 = x + i * w / HIST_SIZE;
    int x2 = x + (i + 1) * w / HIST_SIZE;
    int y1 = y + h - (int)((hist_entropy[idx] / 10.0f) * h);
    int y2 = y + h - (int)((hist_entropy[idx2] / 10.0f) * h);
    y1 = constrain(y1, y, y + h);
    y2 = constrain(y2, y, y + h);
    beaconCanvas.drawLine(x1, y1, x2, y2, houseProtocolActive ? COLOR_AMBER : TFT_CYAN);
  }
}

void drawBar(int x, int y, int w, int h, float value, float maxValue) {
  beaconCanvas.fillRect(x, y, w, h, TFT_BLACK);
  beaconCanvas.drawRect(x, y, w, h, houseProtocolActive ? COLOR_AMBER_DIM : TFT_DARKGREY);

  int graphHeight = max(4, h - 3);
  drawOsc(x + 1, y + 1, w - 2, graphHeight - 1);

  int fill = constrain((int)(w * (value / maxValue)), 0, w);
  uint16_t color = houseProtocolActive ? COLOR_AMBER : TFT_BLUE;
  if (fill > 2) {
    beaconCanvas.fillRect(x + 1, y + 1, fill - 2, max(1, h / 2), color);
  }

  String thought = currentThought.length() ? currentThought : String("observing");
  if (thought.length() > 24) thought = thought.substring(0, 24);
  uint16_t textColor = anomalyLatched ? TFT_WHITE : (houseProtocolActive ? COLOR_AMBER : TFT_WHITE);
  beaconCanvas.setTextColor(textColor, TFT_BLACK);
  beaconCanvas.setCursor(x + 4, y + 1);
  beaconCanvas.printf("%s", thought.c_str());
}


void drawJanusPulse(int x, int y, int w, int h) {
  beaconCanvas.fillRect(x, y, w, h, TFT_BLACK);
  uint16_t frame = anomalyLatched ? TFT_RED : (houseProtocolActive ? COLOR_AMBER_DIM : TFT_DARKGREY);
  uint16_t pulse = anomalyLatched ? TFT_RED : (eye.online ? TFT_GREEN : (houseProtocolActive ? COLOR_AMBER : TFT_CYAN));
  beaconCanvas.drawRect(x, y, w, h, frame);

  float phase = millis() * (0.0035f + constrain(g.entropy, 0.0f, 10.0f) * 0.00045f);
  int mid = y + h / 2;
  int lastX = x + 2;
  int lastY = mid;

  for (int i = 0; i < w - 4; i += 3) {
    float s = sinf(phase + i * 0.18f) + 0.45f * sinf(phase * 1.7f + i * 0.07f);
    int yy = mid + (int)(s * (h * 0.23f + g.m2r * 0.08f));
    yy = constrain(yy, y + 2, y + h - 3);
    int xx = x + 2 + i;
    beaconCanvas.drawLine(lastX, lastY, xx, yy, pulse);
    lastX = xx;
    lastY = yy;
  }

  int fill = constrain((int)((w - 4) * (g.entropy / 10.0f)), 0, w - 4);
  if (fill > 2) {
    uint16_t fillColor = houseProtocolActive ? COLOR_AMBER : TFT_BLUE;
    beaconCanvas.fillRect(x + 2, y + h - 3, fill, 2, fillColor);
  }

  String thought = statusLine.length() ? statusLine : (currentThought.length() ? currentThought : String("observing"));
  if (thought.length() > 27) thought = thought.substring(0, 27);
  beaconCanvas.setTextColor(anomalyLatched ? TFT_WHITE : (houseProtocolActive ? COLOR_AMBER : TFT_WHITE), TFT_BLACK);
  beaconCanvas.setCursor(x + 5, y + 2);
  beaconCanvas.printf("%s", thought.c_str());
}

void drawChip(int x, int y, const char* name, bool on) {
  beaconCanvas.setCursor(x, y);
  uint16_t onColor = houseProtocolActive ? COLOR_AMBER : TFT_GREEN;
  uint16_t offColor = houseProtocolActive ? COLOR_AMBER_DIM : TFT_DARKGREY;
  beaconCanvas.setTextColor(on ? onColor : offColor, TFT_BLACK);
  beaconCanvas.printf("%s:%s", name, on ? "ON" : "OFF");
}



// ========================= JANUS DISTRIBUTED AI + SD ARCHIVE =========================
bool aiNodeIsOnline(const SwarmAiNodeState& n, uint32_t ttl=20000UL) {
  return n.active && (millis() - n.lastSeenMs < ttl);
}

bool aiNodeLooksLike(const SwarmAiNodeState& n, const char* a, const char* b=nullptr) {
  if (!n.active) return false;
  return (a && (strstr(n.nodeId, a) || strstr(n.role, a))) || (b && (strstr(n.nodeId, b) || strstr(n.role, b)));
}

bool core2SeenRecently() {
  for (uint8_t i=0;i<JANUS_AI_MAX_NODES;i++) if (aiNodeIsOnline(swarmAiNodes[i]) && aiNodeLooksLike(swarmAiNodes[i], "Core2", "Galaxy")) return true;
  return false;
}

uint8_t onlineAiNodes() {
  uint8_t n=0;
  for (uint8_t i=0;i<JANUS_AI_MAX_NODES;i++) if (aiNodeIsOnline(swarmAiNodes[i])) n++;
  return n;
}

int findAiNodeSlot(const char* nodeId, const char* role) {
  if (!nodeId || !nodeId[0]) nodeId = role ? role : "node";
  for (uint8_t i=0;i<JANUS_AI_MAX_NODES;i++) {
    if (swarmAiNodes[i].active && strncmp(swarmAiNodes[i].nodeId, nodeId, sizeof(swarmAiNodes[i].nodeId)) == 0) return i;
  }
  int freeSlot=-1;
  uint32_t oldest=0xFFFFFFFFUL;
  int oldestSlot=0;
  for (uint8_t i=0;i<JANUS_AI_MAX_NODES;i++) {
    if (!swarmAiNodes[i].active && freeSlot < 0) freeSlot=i;
    if (swarmAiNodes[i].lastSeenMs < oldest) { oldest=swarmAiNodes[i].lastSeenMs; oldestSlot=i; }
  }
  return freeSlot >= 0 ? freeSlot : oldestSlot;
}

void aiSdLogLine(const char* path, const String& line) {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return;
  if (!SD.exists(JANUS_AI_SD_DIR)) SD.mkdir(JANUS_AI_SD_DIR);
  File f = SD.open(path, FILE_APPEND);
  if (!f) return;
  f.println(line);
  f.close();
#endif
}

void rotateAiLogIfNeeded(const char* path) {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready || !SD.exists(path)) return;
  File f = SD.open(path, FILE_READ);
  size_t sz = f ? f.size() : 0;
  if (f) f.close();
  if (sz < JANUS_AI_MAX_LOG_BYTES) return;
  char dst[80];
  snprintf(dst, sizeof(dst), "%s/archive_%lu_%s", JANUS_AI_SD_DIR, (unsigned long)(millis()/1000UL), strrchr(path, '/') ? strrchr(path, '/') + 1 : "log.jsonl");
  SD.rename(path, dst);
#endif
}

void logAiNodeSnapshot(const SwarmAiNodeState& n, const char* eventName) {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return;
  StaticJsonDocument<384> doc;
  doc["ts_ms"] = millis();
  doc["event"] = eventName ? eventName : "node";
  doc["node"] = n.nodeId;
  doc["role"] = n.role;
  doc["seq"] = n.seq;
  doc["entropy"] = n.entropy;
  doc["pred_error"] = n.prediction_error;
  doc["sync"] = n.sync;
  doc["fit"] = n.fit;
  doc["att"] = n.attention;
  doc["rssi"] = n.rssi;
  doc["worker"] = n.worker;
  doc["online"] = aiNodeIsOnline(n);
  doc["core2"] = aiNodeLooksLike(n, "Core2", "Galaxy");
  doc["sd"] = g.sd_ready;
  String out; serializeJson(doc, out);
  aiSdLogLine(JANUS_AI_NODE_LOG, out);
  rotateAiLogIfNeeded(JANUS_AI_NODE_LOG);
#endif
}

void rememberSwarmAiNode(const char* nodeId, const char* role, float entropy, float predErr, float sync, float fit, float attention, uint8_t flags, uint32_t seq, uint16_t worker, int8_t rssi) {
  if (!nodeId || !nodeId[0]) nodeId = role && role[0] ? role : "node";
  int slot = findAiNodeSlot(nodeId, role);
  bool wasNew = !swarmAiNodes[slot].active;
  SwarmAiNodeState& n = swarmAiNodes[slot];
  n.active = true;
  strlcpy(n.nodeId, nodeId, sizeof(n.nodeId));
  strlcpy(n.role, role && role[0] ? role : "node", sizeof(n.role));
  n.lastSeenMs = millis();
  n.seq = seq;
  n.entropy = isfinite(entropy) ? entropy : 0.0f;
  n.prediction_error = isfinite(predErr) ? predErr : 0.0f;
  n.sync = isfinite(sync) ? sync : 0.0f;
  n.fit = isfinite(fit) ? fit : 0.0f;
  n.attention = isfinite(attention) ? attention : 0.0f;
  n.flags = flags;
  n.worker = worker;
  n.rssi = rssi;
  uint8_t count=0;
  for (uint8_t i=0;i<JANUS_AI_MAX_NODES;i++) if (swarmAiNodes[i].active) count++;
  swarmAiNodeCount = count;
  if (wasNew) {
    snprintf(aiStatusLine, sizeof(aiStatusLine), "AI FOUND %s", n.nodeId);
    logAiNodeSnapshot(n, "found");
  }
}

void onJanusAiPacket(const JanusAiNodePacket& ai, int8_t rxRssi) {
  if (ai.magic[0] != 'A' || ai.magic[1] != 'I') return;
  if (strncmp(ai.nodeId, "BeaconADV", 9) == 0 || strncmp(ai.nodeId, "Beacon", 6) == 0) return;
  colonyRxPackets++;
  rememberSwarmAiNode(ai.nodeId, ai.role, ai.entropy, ai.prediction_error, ai.sync, ai.fit, ai.attention, ai.flags, ai.seq, 0, rxRssi);
}

float distributedAiEntropy() {
  float sum=0.0f, wsum=0.0f;
  uint32_t now=millis();
  for (uint8_t i=0;i<JANUS_AI_MAX_NODES;i++) {
    SwarmAiNodeState& n = swarmAiNodes[i];
    if (!n.active) continue;
    uint32_t age = now - n.lastSeenMs;
    if (age > 30000UL) continue;
    float ageW = constrain(1.0f - (float)age / 30000.0f, 0.0f, 1.0f);
    float syncW = constrain(0.35f + n.sync * 0.65f, 0.1f, 1.5f);
    float w = ageW * syncW;
    sum += n.entropy * w;
    wsum += w;
  }
  return (wsum > 0.01f) ? (sum / wsum) : g.entropy;
}

void flushAiSummaryToSd() {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready || millis() - lastAiSdFlushMs < JANUS_AI_LOG_FLUSH_MS) return;
  lastAiSdFlushMs = millis();
  StaticJsonDocument<512> doc;
  doc["ts_ms"] = millis();
  doc["event"] = "summary";
  doc["online"] = onlineAiNodes();
  doc["known"] = swarmAiNodeCount;
  doc["core2"] = core2SeenRecently();
  doc["local_entropy"] = g.entropy;
  doc["swarm_entropy"] = distributedAiEntropy();
  doc["attention_sum"] = swarmAttentionSum;
  JsonArray nodes = doc.createNestedArray("nodes");
  for (uint8_t i=0;i<JANUS_AI_MAX_NODES;i++) {
    if (!aiNodeIsOnline(swarmAiNodes[i])) continue;
    JsonObject o = nodes.createNestedObject();
    o["id"] = swarmAiNodes[i].nodeId;
    o["role"] = swarmAiNodes[i].role;
    o["e"] = swarmAiNodes[i].entropy;
    o["s"] = swarmAiNodes[i].sync;
  }
  String out; serializeJson(doc, out);
  aiSdLogLine(JANUS_AI_EVENT_LOG, out);
  rotateAiLogIfNeeded(JANUS_AI_EVENT_LOG);
#endif
}

void sendDistributedAiPacket() {
  JanusAiNodePacket ai{};
  ai.magic[0] = 'A'; ai.magic[1] = 'I';
  ai.version = 1;
  ai.flags = (g.sd_ready ? 0x03 : 0x00) | (core2SeenRecently() ? 0x04 : 0x00);
  strlcpy(ai.nodeId, "BeaconADV", sizeof(ai.nodeId));
  strlcpy(ai.role, "BeaconArchiveAI", sizeof(ai.role));
  ai.seq = ++swarmAiSeq;
  ai.uptime_ms = millis();
  ai.entropy = beaconLocalEntropy();
  ai.prediction_error = g.loss + g.imu_loss;
  ai.sync = constrain(0.25f + eye.sync * 0.45f + (core2SeenRecently() ? 0.20f : 0.0f) + onlineAiNodes() * 0.025f, 0.0f, 1.0f);
  ai.fit = g.fit;
  ai.attention = swarmAttentionSum;
  ai.values[0] = g.temp_c;
  ai.values[1] = g.humidity;
  ai.values[2] = g.pressure_hpa;
  ai.values[3] = distributedAiEntropy();
  ai.values[4] = (float)onlineAiNodes();
  ai.values[5] = (float)g.wifi_rssi;
  janusBeaconEspNowSend("AI", &ai, sizeof(ai), true);
  rememberSwarmAiNode("BeaconADV", "BeaconArchiveAI", ai.entropy, ai.prediction_error, ai.sync, ai.fit, ai.attention, ai.flags, ai.seq, colonyWorkerId, g.wifi_rssi);
}

// ========================= JANUS COLONY BEACON HUB HOOKS =========================
void colonySdLog(const String& line) {
#if COLONY_SD_LOG_ENABLE
  if (!g.sd_ready) return;
  if (!SD.exists(COLONY_SD_DIR)) SD.mkdir(COLONY_SD_DIR);
  File f = SD.open(COLONY_SD_LOG_FILE, FILE_APPEND);
  if (!f) return;
  f.println(line);
  f.close();
#endif
}

uint32_t sdFileSizeSafe(const char* path) {
#if COLONY_SD_LOG_ENABLE
  if (!g.sd_ready || !SD.exists(path)) return 0;
  File f = SD.open(path, FILE_READ);
  if (!f) return 0;
  uint32_t s = f.size();
  f.close();
  return s;
#else
  return 0;
#endif
}

void rotateColonyLogIfNeeded() {
#if COLONY_SD_LOG_ENABLE
  if (!g.sd_ready) return;
  if (sdFileSizeSafe(COLONY_SD_LOG_FILE) < COLONY_SD_MAX_CURRENT_BYTES) return;
  char dst[80];
  snprintf(dst, sizeof(dst), "%s%lu.jsonl", COLONY_SD_ARCH_PREFIX, (unsigned long)(millis()/1000UL));
  SD.rename(COLONY_SD_LOG_FILE, dst);
#endif
}

void cleanupColonySdLogs() {
#if COLONY_SD_LOG_ENABLE
  if (!g.sd_ready) return;
  if (millis() - lastSdCleanupAt < COLONY_SD_CLEANUP_MS) return;
  lastSdCleanupAt = millis();
  if (!SD.exists(COLONY_SD_DIR)) SD.mkdir(COLONY_SD_DIR);

  uint32_t total = 0;
  struct Item { String path; uint32_t size; } items[96];
  int count = 0;
  File dir = SD.open(COLONY_SD_DIR);
  if (!dir || !dir.isDirectory()) return;
  File file = dir.openNextFile();
  while (file && count < 96) {
    String name = String(file.name());
    uint32_t sz = file.size();
    total += sz;
    String base = name.substring(name.lastIndexOf('/') + 1);
    String full = String(COLONY_SD_DIR) + "/" + base;
    if (base.startsWith("log_") || base.endsWith(".tmp") || base.endsWith(".bak") || base.endsWith(".old")) {
      items[count++] = {full, sz};
    }
    file.close();
    file = dir.openNextFile();
  }
  dir.close();

  for (int i = 0; i < count; i++) {
    if (items[i].path.endsWith(".tmp") || items[i].path.endsWith(".bak") || items[i].path.endsWith(".old")) {
      SD.remove(items[i].path);
      if (total > items[i].size) total -= items[i].size;
      items[i].size = 0;
    }
  }

  // Conservative ring cleanup: current.jsonl is kept; archives are expendable.
  while (total > COLONY_SD_MAX_TOTAL_BYTES) {
    int victim = -1;
    for (int i = 0; i < count; i++) {
      if (items[i].size > 0 && items[i].path.indexOf("log_") >= 0) { victim = i; break; }
    }
    if (victim < 0) break;
    SD.remove(items[victim].path);
    total -= items[victim].size;
    items[victim].size = 0;
  }
#endif
}

void logEntropyJson(const char* source, const EntropyReportV2* er2, const EntropyReport* er1) {
  StaticJsonDocument<384> doc;
  doc["ts_ms"] = millis();
  doc["src"] = source ? source : "node";
  if (er2) {
    doc["worker"] = er2->worker_id;
    doc["entropy"] = er2->local_entropy;
    doc["pred_error"] = er2->prediction_error;
    doc["sync"] = er2->sync_hint;
    doc["fit"] = er2->fit;
    doc["v0"] = er2->values[0]; doc["v1"] = er2->values[1]; doc["v2"] = er2->values[2]; doc["v3"] = er2->values[3];
    doc["v4"] = er2->values[4]; doc["v5"] = er2->values[5]; doc["v6"] = er2->values[6]; doc["v7"] = er2->values[7];
  } else if (er1) {
    doc["worker"] = er1->worker_id;
    doc["entropy"] = er1->local_entropy;
    doc["pred_error"] = er1->values[3];
    doc["v0"] = er1->values[0]; doc["v1"] = er1->values[1]; doc["v2"] = er1->values[2]; doc["v3"] = er1->values[3];
  }
  doc["imu"] = g.imu_shock; doc["imu_loss"] = g.imu_loss; doc["ew"] = colonyEntropyWeight;
  String out; serializeJson(doc, out);
  colonySdLog(out);
  rotateColonyLogIfNeeded();
}

float beaconLocalEntropy() {
  return constrain(g.entropy + fabsf(g.loss) * 1.5f + g.imu_loss * 2.2f + g.imu_shock * 0.20f + g.pressure_loss * 0.08f + fabsf(g.temp_c - g.pred_temp) * 0.08f + fabsf(g.humidity - g.pred_humidity) * 0.01f, 0.0f, 9999.0f);
}

void onJanusHeartbeat(const JanusColonyPacket& pkt) {
  colonyRxPackets++;
  rememberSwarmAiNode(pkt.nodeId, pkt.role, constrain((float)pkt.bestBits / 4.0f + (float)pkt.hashRate / 22000.0f, 0.0f, 10.0f),
                      pkt.rejects > 0 ? 0.4f : 0.05f, 1.0f / (1.0f + (float)pkt.rejects), pkt.bestBits / 32.0f,
                      constrain((float)pkt.aiHint / 4.0f, 0.0f, 1.0f), pkt.aiHint, pkt.seq, 0, colonyLastRssi);
  if (memcmp(pkt.magic, "JANUS", 5) == 0 && strncmp(pkt.role, "BuzzLighter", 11) == 0) {
    masterHashRate = pkt.hashRate;
    masterShares = pkt.shares;
    masterRejects = pkt.rejects;
    masterBestBits = pkt.bestBits;
    masterDiff = pkt.diff;
    lastMasterSeenMs = millis();
  }
}

void onJanusEntropy(const EntropyReport& er, const void* opt) {
  float safeE = janusSafeEntropy(er.local_entropy);
  float rawLoss = er.values[3];
  bool legacyLossNoise = (!isfinite(rawLoss) || fabsf(rawLoss) > 20.0f);
  float safeLoss = legacyLossNoise ? 0.0f : janusSafeLoss(rawLoss);

  static uint32_t lastEr1LegacyLogMs = 0;
  if (legacyLossNoise) {
    if (millis() - lastEr1LegacyLogMs > 12000UL) {
      lastEr1LegacyLogMs = millis();
      Serial.printf("[ESP-NOW] ER1 legacy loss ignored worker=%u entropy=%.3f rawLoss=%.3f safe=%.3f\n",
                    er.worker_id, er.local_entropy, rawLoss, safeLoss);
    }
  } else {
    Serial.printf("[ESP-NOW] Got ER1 worker=%u entropy=%.3f loss=%.3f safe=%.3f\n", er.worker_id, er.local_entropy, rawLoss, safeLoss);
  }

  colonyRxPackets++;
  eye.online = true;
  eye.last_ok_ms = millis();
  eye.mic_rms = er.values[0];
  eye.tmos_presence = er.values[1];
  eye.mag_norm = er.values[2];
  eye.loss = safeLoss;
  eye.sync = 1.0f / (1.0f + safeLoss);
  eye.activity = safeE;
  eyeDebugLine = legacyLossNoise ? "ESP-NOW ER1 LEGACY" : "ESP-NOW EYE V1";
  char wid[24]; snprintf(wid, sizeof(wid), "ER1-%u", er.worker_id);
  rememberSwarmAiNode(wid, legacyLossNoise ? "EntropyV1Legacy" : "EntropyV1", safeE, safeLoss, eye.sync, 0.0f, safeE * 0.1f, er.sensor_flags, 0, er.worker_id, colonyLastRssi);
  logEntropyJson(legacyLossNoise ? "v1_legacy" : "v1", nullptr, &er);
}

void onJanusEntropyV2(const EntropyReportV2& er2) {
  float safeE = janusSafeEntropy(er2.local_entropy);
  float safeLoss = janusSafeLoss(er2.prediction_error);
  float safeSync = constrain(er2.sync_hint, 0.0f, 1.0f);
  Serial.printf("[ESP-NOW] Got ER2 from %s entropy=%.3f loss=%.3f safe=%.3f\n", er2.nodeId, er2.local_entropy, er2.prediction_error, safeLoss);
  colonyRxPackets++;
  String node = String(er2.nodeId);
  rememberSwarmAiNode(er2.nodeId, "EntropyV2", safeE, safeLoss, safeSync, er2.fit,
                      constrain(safeE / 10.0f + safeSync * 0.25f, 0.0f, 1.5f), er2.sensor_flags, er2.uptime_ms, er2.worker_id, colonyLastRssi);

  if (node.indexOf("EchoMic") >= 0 || node.indexOf("Swarm") >= 0 || node.indexOf("TD") >= 0) {
    audioNode.online = true;
    audioNode.last_ok_ms = millis();
    audioNode.mic_rms = er2.values[0];
    audioNode.pressure_hpa = er2.values[1];
    audioNode.temp_c = er2.values[2];
    audioNode.entropy = safeE;
    audioNode.loss = safeLoss;
    audioNode.sync = safeSync;
  }

  if (node.indexOf("BlindEye") >= 0) {
    eye.online = true;
    eye.last_ok_ms = millis();
    // Blind Eye has no mic on AtomS3R-M12. Audio comes from EchoMic node.
    eyeTmosPresenceRaw = er2.values[1];
    eyeTmosMotionRaw = er2.values[2];

    // TMOS raw can jump hard frame-to-frame. Keep raw for logs, but smooth UI/attention.
    if (eyeTmosPresenceUi <= 0.01f) eyeTmosPresenceUi = eyeTmosPresenceRaw;
    else eyeTmosPresenceUi = eyeTmosPresenceUi * 0.84f + eyeTmosPresenceRaw * 0.16f;
    if (eyeTmosMotionUi <= 0.01f) eyeTmosMotionUi = eyeTmosMotionRaw;
    else eyeTmosMotionUi = eyeTmosMotionUi * 0.84f + eyeTmosMotionRaw * 0.16f;

    eye.tmos_presence = eyeTmosPresenceUi;
    eye.tmos_motion = eyeTmosMotionUi;
    eye.mag_norm = er2.values[3];
    if (eye.mag_norm_smooth <= 0.01f) eye.mag_norm_smooth = eye.mag_norm;
    else eye.mag_norm_smooth = eye.mag_norm_smooth * 0.86f + eye.mag_norm * 0.14f;
    eye.shock = er2.values[4];
    eye.activity = janusSafeEntropy(er2.values[5]);
    eye.pred_activity = janusSafeEntropy(er2.values[6]);
    eye.loss = safeLoss;
    eye.sync = safeSync;
    eye.model_lr = er2.fit;
    eye.status = "espnow eye";
    eye.diag = "ER2 TMOS MAG";
    eyeDebugLine = "ESP-NOW EYE V2";
  }
  logEntropyJson(er2.nodeId, &er2, nullptr);
}

void sendNodeHeartbeat() {
  JanusColonyPacket pkt{};
  memcpy(pkt.magic, "JANUS", 6);
  strlcpy(pkt.nodeId, "Beacon", sizeof(pkt.nodeId));
  strlcpy(pkt.role, colonyMasterSeen ? "BeaconRemote" : "BeaconLocal", sizeof(pkt.role));
  pkt.seq = ++colonySeq;
  pkt.hashRate = colonyRemoteHashrate;
  pkt.shares = colonyRemoteShares;
  pkt.rejects = colonyRemoteRejects;
  pkt.bestBits = colonyBestBits;
  pkt.diff = masterDiff;
  pkt.targetBits = colonyBestBits;
  pkt.aiBatch = constrain((uint16_t)(COLONY_REMOTE_BATCH * colonyEntropyWeight), (uint16_t)80, (uint16_t)700);
  pkt.aiHint = (colonySurpriseWeight > 1.75f) ? 3 : ((g.loss > 0.25f) ? 2 : ((g.fit > 1.4f) ? 3 : 1));
  pkt.jobAgeMs = colonyJob.active ? (millis() - colonyJob.receivedAt) : 0;
  pkt.rssi = g.wifi_rssi;
  pkt.uptime = millis() / 1000;
  janusBeaconEspNowSend("HB", &pkt, sizeof(pkt), true);
}

void sendNodeEntropy() {
  EntropyReport er{};
  er.magic[0] = 'E'; er.magic[1] = 'R';
  er.worker_id = colonyWorkerId;
  er.local_entropy = beaconLocalEntropy();
  er.sensor_flags = 0x19; // bit0=env-ish, bit3=IMU, bit4=prediction
  er.values[0] = g.temp_c;
  er.values[1] = g.humidity;
  er.values[2] = g.imu_shock;
  er.values[3] = g.loss + g.imu_loss;
  janusBeaconEspNowSend("E/R", &er, sizeof(er), true);

  EntropyReportV2 er2{};
  er2.magic[0] = 'E'; er2.magic[1] = '2';
  er2.worker_id = colonyWorkerId;
  strlcpy(er2.nodeId, "BeaconADV", sizeof(er2.nodeId));
  er2.local_entropy = er.local_entropy;
  er2.prediction_error = g.loss + g.imu_loss;
  er2.sync_hint = eye.sync;
  er2.fit = g.fit;
  er2.sensor_flags = er.sensor_flags;
  er2.values[0] = g.temp_c;
  er2.values[1] = g.humidity;
  er2.values[2] = g.entropy;
  er2.values[3] = g.m2r;
  er2.values[4] = g.imu_shock;
  er2.values[5] = g.imu_loss;
  er2.values[6] = colonyEntropyWeight;
  er2.values[7] = (float)g.wifi_rssi;
  er2.uptime_ms = millis();
  janusBeaconEspNowSend("E2", &er2, sizeof(er2), true);

  logEntropyJson("BeaconADV", &er2, nullptr);
}


float janusFinite(float v, float fallback = 0.0f) {
  return isfinite(v) ? v : fallback;
}

void sanitizeBeaconStateForDraw() {
  g.temp_c = janusFinite(g.temp_c, 0.0f);
  g.pred_temp = janusFinite(g.pred_temp, g.temp_c);
  g.humidity = janusFinite(g.humidity, 0.0f);
  g.pred_humidity = janusFinite(g.pred_humidity, g.humidity);
  g.pressure_hpa = janusFinite(g.pressure_hpa, 0.0f);
  g.pred_pressure_hpa = janusFinite(g.pred_pressure_hpa, g.pressure_hpa);
  g.pressure_loss = janusFinite(g.pressure_loss, 0.0f);
  g.entropy = janusFinite(g.entropy, 0.01f);
  g.m2r = janusFinite(g.m2r, 0.10f);
  g.loss = janusFinite(g.loss, 0.0f);
  g.mi = janusFinite(g.mi, 0.0f);
  g.f1 = janusFinite(g.f1, base_f1);
  g.f2 = janusFinite(g.f2, base_f1 + schumann_offset);
  g.pred_f1 = janusFinite(g.pred_f1, g.f1);
  g.pred_f2 = janusFinite(g.pred_f2, g.f2);
  g.fit = janusFinite(g.fit, 0.0f);
  g.fit_best = janusFinite(g.fit_best, 0.0f);
  g.imu_shock = janusFinite(g.imu_shock, 0.0f);
  g.imu_loss = janusFinite(g.imu_loss, 0.0f);

  eye.activity = janusFinite(eye.activity, 0.0f);
  eye.pred_activity = janusFinite(eye.pred_activity, 0.0f);
  eye.loss = janusFinite(eye.loss, 0.0f);
  eye.tmos_presence = janusFinite(eye.tmos_presence, 0.0f);
  eye.tmos_motion = janusFinite(eye.tmos_motion, 0.0f);
  eye.mag_norm = janusFinite(eye.mag_norm, 0.0f);
  eye.sync = janusFinite(eye.sync, 0.0f);
  eye.mag_norm_smooth = janusFinite(eye.mag_norm_smooth, eye.mag_norm);
  audioNode.mic_rms = janusFinite(audioNode.mic_rms, 0.0f);
  audioNode.entropy = janusFinite(audioNode.entropy, 0.0f);
  audioNode.loss = janusFinite(audioNode.loss, 0.0f);
  audioNode.pressure_hpa = janusFinite(audioNode.pressure_hpa, 0.0f);
  audioNode.temp_c = janusFinite(audioNode.temp_c, 0.0f);
  audioNode.sync = janusFinite(audioNode.sync, 0.0f);

  colonyEntropyWeight = janusFinite(colonyEntropyWeight, 1.0f);
  if (colonyEntropyWeight < 0.1f || colonyEntropyWeight > 9.0f) colonyEntropyWeight = 1.0f;
}

void drawScreen() {
  if (!beaconCanvasReady) return;
  sanitizeBeaconStateForDraw();

  beaconCanvas.fillScreen(TFT_BLACK);
  beaconCanvas.setTextSize(1);

  uint16_t primary = uiPrimary();
  uint16_t secondary = uiSecondary();
  bool masterOn = (millis() - lastMasterSeenMs < 18000);

  // Soft old-style frame / header
  beaconCanvas.drawFastHLine(0, 12, 240, houseProtocolActive ? COLOR_AMBER_DIM : TFT_DARKGREY);
  beaconCanvas.setTextColor(primary, TFT_BLACK);
  beaconCanvas.setCursor(2, 2);   beaconCanvas.printf("JANUS ADV v4.5");
  beaconCanvas.setTextColor(secondary, TFT_BLACK);
  beaconCanvas.setCursor(138, 2); beaconCanvas.printf("V:%03d", volume);
  beaconCanvas.setCursor(176, 2); beaconCanvas.printf("BAT:%d%%", g.battery);
  beaconCanvas.setCursor(102, 2); beaconCanvas.printf("AI:%02u", onlineAiNodes());
  beaconCanvas.setCursor(124, 2); beaconCanvas.printf("TH:%03u", thetaState.resonance);
  beaconCanvas.setCursor(216, 2); beaconCanvas.printf("%s%s", g.sd_ready ? "S" : "-", beaconPolicyAvailable() ? "P" : "-");

  // LEFT: physical sensors + core entropy
  beaconCanvas.setTextColor(houseProtocolActive ? COLOR_AMBER : TFT_CYAN, TFT_BLACK);
  beaconCanvas.setCursor(2, 16); beaconCanvas.printf("T %.1f>%.1f", g.temp_c, g.pred_temp);
  beaconCanvas.setCursor(2, 28); beaconCanvas.printf("H %.0f>%.0f", g.humidity, g.pred_humidity);

  beaconCanvas.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  beaconCanvas.setCursor(2, 40);
  if (g.qmp_ready && g.pressure_hpa > 0.0f) beaconCanvas.printf("P %.1f", g.pressure_hpa);
  else beaconCanvas.printf("P --");

  beaconCanvas.setTextColor(houseProtocolActive ? COLOR_AMBER : TFT_YELLOW, TFT_BLACK);
  beaconCanvas.setCursor(2, 52); beaconCanvas.printf("E %.2f", g.entropy);
  beaconCanvas.setCursor(2, 64); beaconCanvas.printf("M2R %.2f", g.m2r);

  beaconCanvas.setTextColor(houseProtocolActive ? COLOR_AMBER : TFT_ORANGE, TFT_BLACK);
  beaconCanvas.setCursor(2, 76); beaconCanvas.printf("LOSS %.2f", g.loss);
  beaconCanvas.setCursor(2, 88); beaconCanvas.printf("MI %.2f", g.mi);

  beaconCanvas.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  beaconCanvas.setCursor(2, 100); beaconCanvas.printf("IMU %.2f", g.imu_shock);

  // MIDDLE: tachyon / brain wave values
  beaconCanvas.setTextColor(houseProtocolActive ? COLOR_AMBER : TFT_YELLOW, TFT_BLACK);
  beaconCanvas.setCursor(84, 16); beaconCanvas.printf("F1 %.1f", g.f1);
  beaconCanvas.setCursor(84, 28); beaconCanvas.printf("F2 %.1f", g.f2);

  beaconCanvas.setTextColor(houseProtocolActive ? COLOR_AMBER : TFT_MAGENTA, TFT_BLACK);
  beaconCanvas.setCursor(84, 40); beaconCanvas.printf("PF1 %.1f", g.pred_f1);
  beaconCanvas.setCursor(84, 52); beaconCanvas.printf("PF2 %.1f", g.pred_f2);

  beaconCanvas.setTextColor(houseProtocolActive ? COLOR_AMBER : TFT_GREEN, TFT_BLACK);
  beaconCanvas.setCursor(84, 64); beaconCanvas.printf("FIT %.2f", g.fit);
  beaconCanvas.setCursor(84, 76); beaconCanvas.printf("BEST %.2f", g.fit_best);
  beaconCanvas.setTextColor(secondary, TFT_BLACK);
  beaconCanvas.setCursor(84, 88); beaconCanvas.printf("C2:%s M:%s", core2SeenRecently() ? "ON" : "--", masterOn ? "ON" : "--");
  beaconCanvas.setCursor(84, 100); beaconCanvas.printf("FUT %.1f/%.1f", tachyonFuture1, tachyonFuture3);

  // RIGHT: Blind Eye stays stable
  uint16_t eyeColor = eye.online ? primary : secondary;
  beaconCanvas.setTextColor(eyeColor, TFT_BLACK);
  beaconCanvas.setCursor(162, 16); beaconCanvas.printf("EYE %s", eye.online ? "ON" : "OFF");
  beaconCanvas.setCursor(162, 28); beaconCanvas.printf("A %.1f", eye.activity);
  beaconCanvas.setCursor(162, 40); beaconCanvas.printf("P %.1f", eye.pred_activity);
  beaconCanvas.setCursor(162, 52); beaconCanvas.printf("L %.3f", eye.loss);
  beaconCanvas.setCursor(162, 64); beaconCanvas.printf("TM %.0f/%.0f", eyeTmosPresenceUi, eyeTmosMotionUi);
  beaconCanvas.setCursor(162, 76); beaconCanvas.printf("MAG %.0f", eye.mag_norm_smooth > 0 ? eye.mag_norm_smooth : eye.mag_norm);
  beaconCanvas.setCursor(162, 88); beaconCanvas.printf("SYN %.2f", eye.sync);
  bool audOn = audioNode.online && (millis() - audioNode.last_ok_ms < 12000);
  beaconCanvas.setTextColor(audOn ? TFT_CYAN : secondary, TFT_BLACK);
  beaconCanvas.setCursor(162, 100); beaconCanvas.printf("AUD %.0f", audOn ? audioNode.mic_rms : 0.0f);

  // JANUS pulse/thought bar, old soul restored
  drawJanusPulse(4, 111, 232, 12);

  drawChip(2, 126, "SND", isSpeakerPlaying);
  drawChip(60, 126, "LOVE", loveExperimentActive);
  drawChip(126, 126, "M2R", lightClockMode);
  drawChip(186, 126, "H", houseProtocolActive);

  beaconCanvas.pushSprite(0, 0);
}



#define QMP6988_ADDR_1 0x70
#define QMP6988_ADDR_2 0x56

bool i2cProbeAddr(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

void scanEnv3I2COnce() {
  Serial.println("[ENV3] I2C scan on Grove SDA=2 SCL=1");
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.printf("[ENV3] found 0x%02X\n", a);
    }
  }
}

void initQMP6988Optional() {
  g.qmp_ready = false;

  // Exact scheme from your working BPS / TD_SWARM sketch:
  // Wire.begin(2, 1, 400000);
  // qmp.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, 2, 1, 400000U), fallback 0x56.
  bool ok = false;

  #ifdef QMP6988_SLAVE_ADDRESS_L
    ok = janusQmp6988.begin(&Wire, QMP6988_SLAVE_ADDRESS_L, GROVE_SDA_PIN, GROVE_SCL_PIN, 400000U);
    Serial.printf("[QMP6988] begin QMP6988_SLAVE_ADDRESS_L=0x%02X -> %d\n", QMP6988_SLAVE_ADDRESS_L, ok ? 1 : 0);
  #endif

  if (!ok) {
    ok = janusQmp6988.begin(&Wire, 0x70, GROVE_SDA_PIN, GROVE_SCL_PIN, 400000U);
    Serial.printf("[QMP6988] begin 0x70 -> %d\n", ok ? 1 : 0);
  }

  if (!ok) {
    ok = janusQmp6988.begin(&Wire, 0x56, GROVE_SDA_PIN, GROVE_SCL_PIN, 400000U);
    Serial.printf("[QMP6988] begin 0x56 -> %d\n", ok ? 1 : 0);
  }

  g.qmp_ready = ok;
  if (ok) {
    janusQmp6988.update();
    float raw = janusQmp6988.pressure;
    g.pressure_hpa = raw / 100.0f;
    g.pred_pressure_hpa = g.pressure_hpa;
    Serial.printf("[QMP6988] READY raw=%.2f hPa=%.2f temp=%.2f\n", raw, g.pressure_hpa, janusQmp6988.cTemp);
  } else {
    Serial.println("[QMP6988] NOT FOUND on Grove Port A SDA=2 SCL=1");
  }
}

void readQMP6988Optional() {
  if (!g.qmp_ready) {
    static uint32_t lastRetry = 0;
    if (millis() - lastRetry > 8000) {
      lastRetry = millis();
      initQMP6988Optional();
    }
    if (!g.qmp_ready) {
      g.pressure_hpa = 0.0f;
      g.pred_pressure_hpa = 0.0f;
      g.pressure_loss = 0.0f;
      return;
    }
  }

  janusQmp6988.update();
  float raw = janusQmp6988.pressure;
  float p = raw / 100.0f; // Pa -> hPa, same as your old working sketch.

  if (p > 300.0f && p < 1200.0f) {
    static float pred = 1013.25f;
    g.pressure_hpa = p;
    g.pressure_loss = fabsf(pred - p);
    pred = pred * 0.985f + p * 0.015f;
    g.pred_pressure_hpa = pred;
  } else {
    Serial.printf("[QMP6988] bad pressure raw=%.2f hPa=%.2f\n", raw, p);
  }

  static uint32_t lastDbg = 0;
  if (millis() - lastDbg > 5000) {
    lastDbg = millis();
    Serial.printf("[QMP6988] raw=%.2f hPa=%.2f pred=%.2f temp=%.2f ready=%d\n",
                  raw, g.pressure_hpa, g.pred_pressure_hpa, janusQmp6988.cTemp, g.qmp_ready ? 1 : 0);
  }
}

void initAdvIMU() {
  M5.Imu.init();
  g.imu_ready = true;
  Serial.println("[ADV IMU] init called");
}

void readAdvIMU() {
  if (!g.imu_ready) {
    // Some M5Unified builds need a second init after display/power settles.
    static uint32_t lastRetry = 0;
    if (millis() - lastRetry > 5000) {
      lastRetry = millis();
      initAdvIMU();
    }
    return;
  }

  float ax=0, ay=0, az=0, gx=0, gy=0, gz=0;
  M5.Imu.getAccel(&ax, &ay, &az);
  M5.Imu.getGyro(&gx, &gy, &gz);
  g.imu_ax = ax; g.imu_ay = ay; g.imu_az = az;
  g.imu_gx = gx; g.imu_gy = gy; g.imu_gz = gz;

  float shock = sqrtf(g.imu_ax*g.imu_ax + g.imu_ay*g.imu_ay + g.imu_az*g.imu_az);
  float gyroMag = sqrtf(g.imu_gx*g.imu_gx + g.imu_gy*g.imu_gy + g.imu_gz*g.imu_gz) * 0.015f;
  g.imu_shock = shock + gyroMag;

  // Tiny predictor: the node learns "normal movement". Error is surprise.
  g.imu_loss = fabsf(g.pred_imu_shock - g.imu_shock);
  g.pred_imu_shock = g.pred_imu_shock * 0.965f + g.imu_shock * 0.035f;

  // Entropy weighting is local and safe: it changes only work pacing, never SHA validity.
  colonySurpriseWeight = constrain(1.0f + g.imu_loss * 0.55f + fabsf(g.loss) * 0.35f + (eye.online ? eye.loss * 0.20f : 0.0f), 0.65f, 2.75f);
  colonyEntropyWeight = constrain(0.75f + g.entropy * 0.06f + colonySurpriseWeight * 0.45f, 0.75f, 2.40f);
}

void readSensors() {
  if (!isfinite(g.f1) || g.f1 < 100.0f || g.f1 > 900.0f) g.f1 = base_f1;
  if (!isfinite(g.f2) || g.f2 < 105.0f || g.f2 > 920.0f) g.f2 = base_f1 + schumann_offset;
  if (!isfinite(g.pred_f1) || g.pred_f1 < 100.0f || g.pred_f1 > 900.0f) g.pred_f1 = g.f1;
  if (!isfinite(g.pred_f2) || g.pred_f2 < 105.0f || g.pred_f2 > 920.0f) g.pred_f2 = g.f2;

  readAdvIMU();
  readQMP6988Optional();

  float prevEntropy = g.entropy;
  float prevF1 = g.f1;
  float prevTemp = g.temp_c;
  float prevHum = g.humidity;

  float temp = g.temp_c, hum = g.humidity;
  if (readSHT30(temp, hum)) {
    // Smooth ENV input: tachyon learns physical trends, not I2C jitter.
    if (g.temp_c == 0.0f && g.humidity == 0.0f) { g.temp_c = temp; g.humidity = hum; }
    else {
      g.temp_c = g.temp_c * 0.82f + temp * 0.18f;
      g.humidity = g.humidity * 0.82f + hum * 0.18f;
    }
  }

  g.battery = M5.Power.getBatteryLevel();
  g.wifi_rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;

  float x[FEATURE_DIM];
  buildFeatures(x, prevEntropy, prevF1, prevTemp, prevHum);

  float rawPredEntropy = constrain(predictLinear(x), 0.01f, 10.0f);
  if (!isfinite(tachyonPredEntropyEma) || tachyonPredEntropyEma < 0.01f || tachyonPredEntropyEma > 10.0f) tachyonPredEntropyEma = prevEntropy;
  tachyonPredEntropyEma = tachyonPredEntropyEma * 0.86f + rawPredEntropy * 0.14f;
  g.pred_entropy = constrain(tachyonPredEntropyEma, max(0.01f, prevEntropy - 0.80f), min(10.0f, prevEntropy + 0.80f));

  // Predict physical values conservatively.
  float entropyDelta = constrain(g.pred_entropy - prevEntropy, -0.8f, 0.8f);
  g.pred_temp = g.temp_c + entropyDelta * 0.45f + (g.temp_c - prevTemp) * 0.35f;
  g.pred_humidity = g.humidity + entropyDelta * 1.15f + (g.humidity - prevHum) * 0.35f;

  g.entropy = computeEntropy(prevEntropy);
  updateSwarmAttention();
  tachyonTrainSequence(g.entropy);
  tachyonUpdateFuture();
  g.m2r = computeM2R();

  // Tachyon v2 field: no per-frame random walk.
  // F1/F2 move as a damped oscillator around the brainwave center.
  float t = millis() * 0.001f;
  float breath = sinf(t * pulse_rate) * 0.12f + sinf(t * 0.318f) * 0.055f;
  float pressureMod = (g.pressure_hpa > 0.0f) ? constrain((g.pressure_hpa - 1013.25f) * 0.006f, -0.65f, 0.65f) : 0.0f;
  float eyeMod = eye.online ? constrain(eye.sync, 0.0f, 1.0f) * 0.08f : 0.0f;
  bool audOn = audioNode.online && (millis() - audioNode.last_ok_ms < 12000);
  float audioMod = audOn ? constrain(audioNode.mic_rms / 1800.0f, 0.0f, 1.0f) * 0.055f : 0.0f;

  float targetF1 = base_f1 + breath + g.m2r * 0.010f + pressureMod + eyeMod + audioMod;
  targetF1 = constrain(targetF1, base_f1 - 9.0f, base_f1 + 18.0f);

  tachyonTrendF1 = tachyonTrendF1 * 0.90f + (targetF1 - g.f1) * 0.10f;
  tachyonF1Ema = tachyonF1Ema * 0.92f + targetF1 * 0.08f;
  g.f1 = constrain(tachyonF1Ema, base_f1 - 10.0f, base_f1 + 20.0f);

  float targetF2 = g.f1 + schumann_offset + g.m2r * 0.045f + pressureMod * 0.08f;
  tachyonF2Ema = tachyonF2Ema * 0.92f + targetF2 * 0.08f;
  g.f2 = constrain(tachyonF2Ema, g.f1 + 6.8f, g.f1 + 10.8f);

  g.pred_f1 = g.f1 + tachyonTrendF1 * 1.8f + entropyDelta * 0.55f;
  g.pred_f1 = constrain(g.pred_f1, base_f1 - 12.0f, base_f1 + 22.0f);

  g.pred_m2r = constrain(g.m2r + (g.pred_entropy - g.entropy) * 0.28f, 0.05f, 20.0f);
  g.pred_f2 = g.pred_f1 + schumann_offset + g.pred_m2r * 0.045f + pressureMod * 0.08f;
  g.pred_f2 = constrain(g.pred_f2, g.pred_f1 + 6.8f, g.pred_f1 + 10.8f);

  if (lightClockMode) updateLightClock();

  trainLinear(g.entropy, x);
  g.mi = computeMI();
  g.fit = computeFitness();
  if (g.fit > g.fit_best) g.fit_best = g.fit;
  updateThetaState(g.entropy, anomalyLatched || loveExperimentActive || g.fit > 1.5f || (eye.online && eye.sync > 0.82f));
  applyThetaToColonyWeights();

  if (eye.online && eye.sync > 0.6f && g.fit > 1.2f && g.loss < 0.18f) {
    saveMemoryEntry(g.entropy, g.fit, eye.sync, eye.tmos_presence);
  }

  hist_entropy[hist_pos] = g.entropy;
  hist_loss[hist_pos] = g.loss;
  hist_fit[hist_pos] = g.fit;
  hist_pos = (hist_pos + 1) % HIST_SIZE;
  if (hist_count < HIST_SIZE) hist_count++;

  bool audOnSeq = audioNode.online && (millis() - audioNode.last_ok_ms < 12000);
  float audESeq = audOnSeq ? constrain(audioNode.entropy + audioNode.mic_rms / 1000.0f, 0.0f, 10.0f) : 0.0f;
  float eyeESeq = eye.online ? constrain(eye.activity * 0.03f + eye.tmos_motion * 0.0011f + eye.loss * 3.0f, 0.0f, 10.0f) : 0.0f;
  float masterESeq = (millis() - lastMasterSeenMs < 18000) ? constrain((float)masterBestBits / 4.0f + (float)masterHashRate / 20000.0f, 0.0f, 10.0f) : 0.0f;
  tachyonPushSequence(g.pred_entropy, g.entropy, audESeq, eyeESeq, masterESeq);

  maybeTriggerAnomaly();

  currentThought = anomalyLatched ? String("mind split") : selectOrCreatePhrase();
  if (lastThought != currentThought) {
    rememberThoughtTransition(lastThought, currentThought);
    lastThought = currentThought;
  }
  statusLine = currentThought;
}


void writeBeaconSdManifest() {
#if JANUS_AI_SD_ENABLE
  if (!g.sd_ready) return;
  if (!SD.exists(JANUS_AI_SD_DIR)) SD.mkdir(JANUS_AI_SD_DIR);
  if (!SD.exists(JANUS_AI_POLICY_DIR)) SD.mkdir(JANUS_AI_POLICY_DIR);
  File f = SD.open(JANUS_AI_MANIFEST_FILE, FILE_WRITE);
  if (!f) return;
  f.print("{\"schema\":\"janus.beacon.sd.manifest.v1\",");
  f.print("\"version\":\"" JANUS_BEACON_VERSION "\",");
  f.print("\"node\":\"BeaconArchiveAI\",");
  f.print("\"events\":\""); f.print(JANUS_AI_EVENT_LOG); f.print("\",");
  f.print("\"nodes\":\""); f.print(JANUS_AI_NODE_LOG); f.print("\",");
  f.print("\"policy\":\""); f.print(JANUS_AI_POLICY_FILE); f.print("\",");
  f.print("\"brain\":\""); f.print(JANUS_BRAIN_SD_DIR); f.print("\",");
  f.print("\"budget_gb\":15,");
  f.print("\"uptime_ms\":"); f.print((uint32_t)millis());
  f.println("}");
  f.close();
#endif
}

bool beaconPolicyAvailable() {
#if JANUS_AI_SD_ENABLE
  return g.sd_ready && SD.exists(JANUS_AI_POLICY_FILE);
#else
  return false;
#endif
}

void setup() {
  auto cfg = M5.config();
  M5Cardputer.begin(cfg, true);
  Serial.begin(115200);
  Wire.begin(GROVE_SDA_PIN, GROVE_SCL_PIN, 400000U);
  Wire.setClock(400000U);
  initQMP6988Optional();
  LittleFS.begin(true);
  g.sd_ready = initJanusSdCard();
  if (g.sd_ready) {
    janusEnsureSdDirs();
    janusMigrateLittleFsToSd();
    janusFreeLittleFsLegacy();
    writeBeaconSdManifest();
  } else {
    janusFreeLittleFsLegacy();
  }
  loadState();        // SD-first, LittleFS fallback
  loadThetaState();
  loadModel();
  tachyonPredEntropyEma = constrain(g.entropy, 0.01f, 10.0f);
  tachyonLossEma = constrain(g.loss, 0.0f, 5.0f);
  tachyonMIEma = constrain(g.mi, 0.0f, 2.0f);
  tachyonF1Ema = isfinite(g.f1) ? g.f1 : base_f1;
  tachyonF2Ema = isfinite(g.f2) ? g.f2 : base_f1 + schumann_offset;
  tachyonFuture1 = g.entropy;
  tachyonFuture2 = g.entropy;
  tachyonFuture3 = g.entropy;
  tachyonSeqPos = 0;
  tachyonSeqCount = 0;
  Serial.printf("[SD] ADV CS=%u ready=%d\n", janusSdCsUsed, g.sd_ready ? 1 : 0);
  if (g.sd_ready) janusEnsureSdDirs();
  initAdvIMU();
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setBrightness(brightness);
  beaconCanvas.setColorDepth(16);
  beaconCanvas.createSprite(240, 135);
  beaconCanvasReady = (beaconCanvas.width() >= 240 && beaconCanvas.height() >= 135);
  if (beaconCanvasReady) beaconCanvas.setTextSize(1);
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(255);
  M5Cardputer.Speaker.begin();
  setSpeakerVolume();
  initWiFi(true);
  initColonyNow();
  janusBeaconBootEvent();
  cleanupColonySdLogs();
  selectActiveServer(true);
  initLoRa();
  currentThought = "observing";
  statusLine = "sd tachyon home online";
  Serial.println("[BEACON] v4.5A BLACKBOX-HOME-CORTEX POLISH SD-TACHYON Ramanujan Beatmaker ready");
}

void loop() {
  unsigned long now = millis();
  processKeyboard();

  if (now - lastSensorAt >= SENSOR_INTERVAL_MS) { lastSensorAt = now; readSensors(); }
  if (now - lastEyePullAt >= EYE_PULL_MS) { lastEyePullAt = now; /* Eye arrives via ESP-NOW */ }
  updateBrainWaveMusic();
  janusParadoxSelfStudyTick();
  if (now - lastSendAt >= SEND_INTERVAL_MS) { lastSendAt = now; sendNodeEntropy(); sendDistributedAiPacket(); }
  if (now - lastCmdAt >= COMMAND_INTERVAL_MS) { lastCmdAt = now; sendNodeHeartbeat(); }
  if (now - lastQueueFlushAt >= QUEUE_FLUSH_MS) { lastQueueFlushAt = now; cleanupColonySdLogs(); flushAiSummaryToSd(); janusCleanupStorageTick(); janusLogTachyonSample(); }
  if (loveExperimentActive && now - lastLoraAt >= LORA_BROADCAST_MS) { lastLoraAt = now; sendLovePacketLoRa(); }
  if (now - lastSaveAt >= AUTOSAVE_MS) { lastSaveAt = now; saveState(); saveThetaState(); saveModel(); }
  colonyTick();
  janusBeaconBlackboardTick();
  if (WiFi.status() != WL_CONNECTED) initWiFi(false);
  updateLED();
  if (now - lastDrawAt >= DRAW_INTERVAL_MS) { lastDrawAt = now; drawScreen(); }
  delay(1);
}
