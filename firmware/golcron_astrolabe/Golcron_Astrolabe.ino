/*
  Golcron Astrolabe v0.1

  11th Janus swarm participant: a tiny medallion / astrolabe worker.

  Role:
  - receives Buzz J/B jobs over ESP-NOW
  - scans nonce space with a deterministic star-map stride/offset
  - submits only valid S/2 share candidates back to Buzz
  - emits JANUS + S/S presence so Buzz can see it as a real swarm member
  - shows a compact astrolabe HUD on the small SSD1306 OLED

  Math rule:
  The star map only changes nonce traversal order. It does not change block
  header bytes except nonce, target bytes, hash function, or pool semantics.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>
#include <mbedtls/sha256.h>
#include <Wire.h>
#include <U8g2lib.h>

#define GOLCRON_VERSION "v0.1"
#define GOLCRON_NODE_ID "Golcron"
#define GOLCRON_ROLE "ASTROLABE"
#define JANUS_SWARM_CHANNEL 10
#define SERIAL_BAUD 115200
#define OLED_SCL 22
#define OLED_SDA 21
#define OLED_RESET U8X8_PIN_NONE
#define STATUS_MS 2000UL
#define OLED_MS 160UL
#define JOB_TTL_MS 6500UL

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, OLED_RESET, OLED_SCL, OLED_SDA);
static bool oledOk = false;

static const uint8_t JANUS_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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
  uint8_t magic[2];       // 'J','B'
  uint8_t job_id[8];
  uint8_t header[80];
  uint32_t start_nonce;
  uint32_t range_size;
  uint8_t target[32];
  uint32_t extranonce2;
};

struct __attribute__((packed)) ShareResponseV2 {
  uint8_t magic[2];       // 'S','2'
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
  uint16_t bits;
  uint32_t total_hashes_l32;
  uint8_t hash_tail[4];
};

struct __attribute__((packed)) SwarmSensePacket {
  uint8_t magic[2];        // 'S','S'
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

struct __attribute__((packed)) JanusAgentRewardPacket {
  uint8_t magic[2];        // 'A','R'
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

struct StarRef {
  const char *name;
  uint16_t ra;
  int16_t dec;
  uint8_t mag10;
  uint8_t house;
};

static const StarRef STAR_MAP[] = {
  {"SIRIUS", 1012, -167, 14, 1},
  {"CANOPUS", 957, -526, 7, 2},
  {"ARCTURUS", 2139, 191, 0, 3},
  {"VEGA", 2792, 388, 0, 4},
  {"CAPELLA", 792, 459, 1, 5},
  {"RIGEL", 789, -82, 1, 6},
  {"PROCYON", 1148, 52, 4, 7},
  {"BETELGEUSE", 887, 74, 5, 8},
  {"ALTAIR", 2977, 89, 8, 9},
  {"ALDEBARAN", 690, 163, 9, 10},
  {"ANTARES", 2473, -263, 10, 11},
  {"SPICA", 2013, -111, 10, 12},
  {"POLLUX", 1161, 280, 11, 13},
  {"FOMALHAUT", 3444, -296, 12, 14},
  {"DENEB", 3104, 450, 13, 15},
  {"REGULUS", 1527, 120, 14, 16},
};
static const uint8_t STAR_COUNT = sizeof(STAR_MAP) / sizeof(STAR_MAP[0]);

struct AstrolabeJob {
  bool active = false;
  uint8_t jobId[8] = {0};
  uint8_t header[80] = {0};
  uint8_t target[32] = {0};
  uint32_t startNonce = 0;
  uint32_t rangeSize = 0;
  uint32_t cursor = 0;
  uint32_t offset = 0;
  uint32_t stride = 1;
  uint32_t rxMs = 0;
  uint32_t seq = 0;
  uint8_t starIndex = 0;
};

static AstrolabeJob job;
static uint16_t workerIdCache = 0;
static uint32_t seqNo = 0;
static uint32_t jobSeq = 0;
static uint32_t lastStatusMs = 0;
static uint32_t lastOledMs = 0;
static uint32_t lastRateMs = 0;
static uint32_t totalHashes = 0;
static uint32_t windowHashes = 0;
static uint32_t hashRate = 0;
static uint32_t sharesSent = 0;
static uint32_t rejectsLocal = 0;
static uint32_t jobsRx = 0;
static uint32_t discoveryRx = 0;
static uint32_t bestBits = 0;
static uint32_t bestNonce = 0;
static uint16_t targetBitsNow = 22;
static uint16_t batchSize = 180;
static uint8_t agentHint = 1;
static int8_t lastRssi = 0;
static bool buzzSeen = false;
static uint32_t lastBuzzMs = 0;
static uint32_t txOk = 0;
static uint32_t txFail = 0;
static uint8_t bestTail[4] = {0};
static char statusLine[48] = "WAIT BUZZ";

static uint32_t mix32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x7FEB352DUL;
  x ^= x >> 15;
  x *= 0x846CA68BUL;
  x ^= x >> 16;
  return x;
}

static void writeLE32(uint8_t *p, uint32_t v) {
  p[0] = (uint8_t)(v & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)((v >> 16) & 0xFF);
  p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void hashToShareOrder(const uint8_t raw[32], uint8_t out[32]) {
  for (int i = 0; i < 32; ++i) out[i] = raw[31 - i];
}

static uint16_t countLeadingZeroBits(const uint8_t h[32]) {
  uint16_t bits = 0;
  for (int i = 0; i < 32; ++i) {
    uint8_t b = h[i];
    if (b == 0) {
      bits += 8;
      continue;
    }
    for (int k = 7; k >= 0; --k) {
      if ((b & (1 << k)) == 0) bits++;
      else return bits;
    }
  }
  return bits;
}

static bool hashMeetsTargetBytes(const uint8_t hash[32], const uint8_t target[32]) {
  for (int i = 0; i < 32; ++i) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;
}

static uint16_t workerId() {
  if (workerIdCache) return workerIdCache;
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  uint16_t id = 0x4700;
  for (uint8_t b : mac) id = (uint16_t)((id * 33U) ^ b);
  if (!id) id = 0x4711;
  workerIdCache = id;
  return id;
}

static bool targetMatchesMe(const char *targetNode) {
  if (!targetNode || !targetNode[0]) return false;
  return !strncmp(targetNode, GOLCRON_NODE_ID, 24) ||
         !strncmp(targetNode, "Golcron", 24) ||
         !strncmp(targetNode, "all", 24) ||
         !strncmp(targetNode, "*", 24);
}

static void applyAgentReward(const JanusAgentRewardPacket &ar) {
  if (!targetMatchesMe(ar.targetNode)) return;
  if (ar.targetBatch >= 40 && ar.targetBatch <= 900) batchSize = ar.targetBatch;
  if (ar.aiHint) agentHint = ar.aiHint;
  if (ar.entropySeed) {
    job.offset ^= mix32(ar.entropySeed ^ millis());
    if (job.rangeSize) job.offset %= job.rangeSize;
  }
}

static const StarRef &currentStar() {
  return STAR_MAP[job.starIndex % STAR_COUNT];
}

static uint32_t jobSeed(const JobPacket &jp) {
  uint32_t s = jp.start_nonce ^ mix32(jp.range_size);
  for (int i = 0; i < 8; ++i) s = mix32(s ^ ((uint32_t)jp.job_id[i] << ((i & 3) * 8)));
  return s;
}

static void configureAstrolabePath(const JobPacket &jp) {
  uint32_t seed = jobSeed(jp);
  job.starIndex = seed % STAR_COUNT;
  const StarRef &s = currentStar();
  uint32_t base = mix32(seed ^ ((uint32_t)s.ra << 16) ^ ((uint32_t)(uint16_t)s.dec << 1) ^ s.house);
  job.offset = jp.range_size ? (base % jp.range_size) : 0;
  uint32_t rawStride = mix32(base ^ 0xA5710ABEU ^ ((uint32_t)s.mag10 << 24));
  rawStride ^= (uint32_t)s.ra * 2654435761UL;
  rawStride |= 1UL;
  if (jp.range_size) rawStride %= jp.range_size;
  if (rawStride < 3) rawStride = 3;
  if ((rawStride & 1) == 0) rawStride++;
  job.stride = rawStride;
  snprintf(statusLine, sizeof(statusLine), "%s S%02u z%u", s.name, (unsigned)s.house, (unsigned)targetBitsNow);
}

static void onJobPacket(const JobPacket &jp) {
  buzzSeen = true;
  lastBuzzMs = millis();
  if (jp.range_size == 0) {
    discoveryRx++;
    return;
  }

  memset(&job, 0, sizeof(job));
  job.active = true;
  memcpy(job.jobId, jp.job_id, 8);
  memcpy(job.header, jp.header, 80);
  memcpy(job.target, jp.target, 32);
  job.startNonce = jp.start_nonce;
  job.rangeSize = jp.range_size;
  job.cursor = 0;
  job.rxMs = millis();
  job.seq = ++jobSeq;
  targetBitsNow = countLeadingZeroBits(job.target);
  jobsRx++;
  configureAstrolabePath(jp);
}

static void sendBytes(const char *tag, const uint8_t *data, size_t len) {
  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, data, len);
  if (err != ESP_OK) txFail++;
  (void)tag;
}

static void sendShare(uint32_t nonce, uint16_t bits, const uint8_t shareHash[32]) {
  ShareResponseV2 sr{};
  sr.magic[0] = 'S';
  sr.magic[1] = '2';
  memcpy(sr.job_id, job.jobId, 8);
  sr.nonce = nonce;
  sr.worker_id = workerId();
  sr.bits = bits;
  sr.total_hashes_l32 = totalHashes;
  memcpy(sr.hash_tail, shareHash + 28, 4);
  sendBytes("share", (const uint8_t *)&sr, sizeof(sr));
  sharesSent++;
}

static void sendColonyStatus() {
  JanusColonyPacket pkt{};
  memcpy(pkt.magic, "JANUS", 6);
  strlcpy(pkt.nodeId, GOLCRON_NODE_ID, sizeof(pkt.nodeId));
  strlcpy(pkt.role, GOLCRON_ROLE, sizeof(pkt.role));
  pkt.seq = ++seqNo;
  pkt.hashRate = hashRate;
  pkt.shares = sharesSent;
  pkt.rejects = rejectsLocal;
  pkt.bestBits = bestBits;
  pkt.diff = 0.0f;
  pkt.targetBits = targetBitsNow;
  pkt.aiBatch = batchSize;
  pkt.aiHint = agentHint;
  pkt.jobAgeMs = job.active ? (millis() - job.rxMs) : 0;
  pkt.rssi = lastRssi;
  pkt.uptime = millis() / 1000UL;
  sendBytes("janus", (const uint8_t *)&pkt, sizeof(pkt));
}

static void sendSwarmSense() {
  SwarmSensePacket ss{};
  ss.magic[0] = 'S';
  ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = workerId();
  strlcpy(ss.nodeId, GOLCRON_NODE_ID, sizeof(ss.nodeId));
  strlcpy(ss.kind, "astrolabe_worker", sizeof(ss.kind));
  ss.seq = seqNo;
  ss.uptime_ms = millis();
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = 0;
  ss.loop_max_us = 0;
  ss.rssi = lastRssi;
  ss.radio_mode = JANUS_SWARM_CHANNEL;
  ss.palette = 6;
  ss.knn_label = job.active ? 2 : 1;
  ss.knn_confidence = buzzSeen ? 86 : 24;
  ss.ai_hint = agentHint;
  ss.thermal_load = 20;
  ss.effective_batch = batchSize;
  ss.dynamic_batch = batchSize;
  ss.hash_rate = hashRate;
  ss.total_hashes = totalHashes;
  ss.best_bits = bestBits > 65535UL ? 65535U : (uint16_t)bestBits;
  ss.hash_eff_x1000 = targetBitsNow ? (uint16_t)min(3000UL, bestBits * 1000UL / targetBitsNow) : 0;
  ss.entropy_x1000 = (uint16_t)((job.offset ^ job.stride ^ micros()) % 1000U);
  ss.job_age_s = job.active ? (uint16_t)min(65535UL, (millis() - job.rxMs) / 1000UL) : 65535;
  ss.nonce_remaining_l16 = job.active && job.rangeSize > job.cursor ? (uint16_t)((job.rangeSize - job.cursor) & 0xFFFF) : 0;
  ss.flags = 0x0041; // worker + astrolabe
  sendBytes("sense", (const uint8_t *)&ss, sizeof(ss));
}

static void drawOled() {
  if (!oledOk) return;
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x7_tf);
  oled.drawStr(0, 7, "GOLCRON ASTROLABE");
  oled.drawHLine(0, 10, 128);
  char line[32];
  snprintf(line, sizeof(line), "BUZZ:%c JOB:%c CH:%u", buzzSeen ? 'Y' : 'N', job.active ? 'Y' : 'N', JANUS_SWARM_CHANNEL);
  oled.drawStr(0, 21, line);
  snprintf(line, sizeof(line), "H:%lu B:%lu/%u", (unsigned long)hashRate, (unsigned long)bestBits, (unsigned)targetBitsNow);
  oled.drawStr(0, 31, line);
  snprintf(line, sizeof(line), "STAR:%s", currentStar().name);
  oled.drawStr(0, 41, line);
  snprintf(line, sizeof(line), "A%lu S%lu", (unsigned long)(job.offset & 0xFFFF), (unsigned long)(job.stride & 0xFFFF));
  oled.drawStr(0, 51, line);
  int w = job.rangeSize ? (int)((uint64_t)min(job.cursor, job.rangeSize) * 126ULL / job.rangeSize) : 0;
  oled.drawFrame(0, 56, 128, 8);
  if (w > 0) oled.drawBox(1, 57, w, 6);
  oled.sendBuffer();
}

static void runMinerSlice(uint16_t batch) {
  if (!job.active) return;
  if (millis() - job.rxMs > JOB_TTL_MS) {
    job.active = false;
    snprintf(statusLine, sizeof(statusLine), "JOB STALE");
    return;
  }
  if (job.cursor >= job.rangeSize) {
    job.active = false;
    snprintf(statusLine, sizeof(statusLine), "RANGE DONE");
    return;
  }

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  uint8_t header[80];
  uint8_t hash1[32];
  uint8_t hash2[32];
  uint8_t shareHash[32];

  for (uint16_t i = 0; i < batch && job.cursor < job.rangeSize; ++i) {
    uint32_t mapped = (uint32_t)(((uint64_t)job.cursor * (uint64_t)job.stride + job.offset) % job.rangeSize);
    uint32_t nonce = job.startNonce + mapped;
    job.cursor++;

    memcpy(header, job.header, 80);
    writeLE32(header + 76, nonce);

    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, header, 80);
    mbedtls_sha256_finish(&ctx, hash1);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, hash1, 32);
    mbedtls_sha256_finish(&ctx, hash2);
    hashToShareOrder(hash2, shareHash);

    totalHashes++;
    windowHashes++;
    uint16_t bits = countLeadingZeroBits(shareHash);
    if (bits > bestBits) {
      bestBits = bits;
      bestNonce = nonce;
      memcpy(bestTail, shareHash + 28, 4);
    }
    if (bits >= targetBitsNow && hashMeetsTargetBytes(shareHash, job.target)) {
      sendShare(nonce, bits, shareHash);
      snprintf(statusLine, sizeof(statusLine), "SHARE z%u", (unsigned)bits);
    }
  }
  mbedtls_sha256_free(&ctx);
}

static void updateRate() {
  uint32_t now = millis();
  if (now - lastRateMs < 1000UL) return;
  uint32_t dt = now - lastRateMs;
  hashRate = dt ? (uint32_t)((uint64_t)windowHashes * 1000ULL / dt) : 0;
  windowHashes = 0;
  lastRateMs = now;
}

static void ensureBroadcastPeer() {
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6);
  peer.channel = JANUS_SWARM_CHANNEL;
  peer.encrypt = false;
  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) esp_now_del_peer(JANUS_BROADCAST_MAC);
  esp_now_add_peer(&peer);
}

static void radioBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(100);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(JANUS_SWARM_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  esp_now_deinit();
  if (esp_now_init() != ESP_OK) {
    Serial.println("[GOLCRON] ESP-NOW init failed, reboot");
    delay(500);
    ESP.restart();
  }
  ensureBroadcastPeer();
  esp_now_register_recv_cb([](const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (!data || len < 2) return;
    if (info && info->rx_ctrl) lastRssi = info->rx_ctrl->rssi;
    if (len == (int)sizeof(JobPacket) && data[0] == 'J' && data[1] == 'B') {
      JobPacket jp{};
      memcpy(&jp, data, sizeof(jp));
      onJobPacket(jp);
      return;
    }
    if (len == (int)sizeof(JanusAgentRewardPacket) && data[0] == 'A' && data[1] == 'R') {
      JanusAgentRewardPacket ar{};
      memcpy(&ar, data, sizeof(ar));
      if (ar.version == 1) applyAgentReward(ar);
      return;
    }
  });
  esp_now_register_send_cb([](
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
    const wifi_tx_info_t *,
#else
    const uint8_t *,
#endif
    esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) txOk++;
    else txFail++;
  });
}

static void oledBegin() {
  Wire.begin(OLED_SDA, OLED_SCL);
  oled.begin();
  oled.setBusClock(400000);
  oledOk = true;
  oled.clearBuffer();
  oled.setFont(u8g2_font_5x7_tf);
  oled.drawStr(0, 10, "GOLCRON ONLINE");
  oled.drawStr(0, 24, "ASTROLABE WORKER");
  oled.drawStr(0, 38, "WAIT BUZZ J/B");
  oled.sendBuffer();
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(250);
  oledBegin();
  radioBegin();
  lastRateMs = millis();
  lastStatusMs = millis();
  Serial.printf("[GOLCRON] %s ready node=%s ch=%u worker=%u oled=%d sda=%u scl=%u observer_math=star_nonce_order\n",
                GOLCRON_VERSION, GOLCRON_NODE_ID, JANUS_SWARM_CHANNEL,
                workerId(), oledOk ? 1 : 0, OLED_SDA, OLED_SCL);
}

void loop() {
  uint16_t b = batchSize;
  if (!buzzSeen || millis() - lastBuzzMs > 12000UL) b = 40;
  if (agentHint == 3) b = min<uint16_t>(900, b + 120);
  if (agentHint == 2) b = max<uint16_t>(40, b / 2);
  runMinerSlice(b);
  updateRate();

  uint32_t now = millis();
  if (now - lastStatusMs >= STATUS_MS) {
    sendColonyStatus();
    sendSwarmSense();
    lastStatusMs = now;
    Serial.printf("[GOLCRON] buzz=%u job=%u age=%lu H=%lu best=%lu/%u shares=%lu jobs=%lu/%lu star=%s house=%u off=%lu stride=%lu cursor=%lu/%lu tx=%lu/%lu rssi=%d status=\"%s\"\n",
                  buzzSeen ? 1 : 0, job.active ? 1 : 0,
                  job.active ? (unsigned long)(now - job.rxMs) : 0UL,
                  (unsigned long)hashRate, (unsigned long)bestBits, (unsigned)targetBitsNow,
                  (unsigned long)sharesSent, (unsigned long)jobsRx, (unsigned long)discoveryRx,
                  currentStar().name, (unsigned)currentStar().house,
                  (unsigned long)job.offset, (unsigned long)job.stride,
                  (unsigned long)job.cursor, (unsigned long)job.rangeSize,
                  (unsigned long)txOk, (unsigned long)txFail, (int)lastRssi, statusLine);
  }
  if (now - lastOledMs >= OLED_MS) {
    drawOled();
    lastOledMs = now;
  }
}
