/*
  PEA4 Companion Bridge v0.1

  Runs on an ESP32-S3/C6-class radio companion, not on the ESP32-P4 itself.
  It reads optional PEA4 Serial text frames and rebroadcasts PEA4 presence as
  Buzz SwarmSense 'S','S' packets over ESP-NOW channel 10.

  Hard rule: observer-only. No pool submit, no target mutation, no scheduler
  pressure. This is only a radio voice for PEA4.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_idf_version.h>

#define PEA4_BRIDGE_VERSION "v0.1"
#define JANUS_SWARM_CHANNEL 10
#define SERIAL_BAUD 115200
#define PEA4_UART_RX 18
#define PEA4_UART_TX 17
#define PEA4_UART_BAUD 115200
#define PRESENCE_MS 2000UL

static const uint8_t JANUS_BROADCAST_MAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

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

static char line0[256];
static size_t line0Len = 0;
static char line1[256];
static size_t line1Len = 0;
static uint32_t seqNo = 0;
static uint32_t lastPresenceMs = 0;
static uint32_t lastP4Ms = 0;
static uint32_t p4HashRate = 0;
static uint32_t p4TotalLow = 0;
static uint16_t p4BestBits = 0;
static uint16_t p4TargetBits = 22;
static uint32_t p4Jp4Frames = 0;
static uint32_t p4ParseOk = 0;
static uint32_t p4ParseBad = 0;
static uint32_t txOk = 0;
static uint32_t txFail = 0;
static char p4Mode[10] = "WAIT";
static char p4Role[8] = "P4";
static char p4Digest16[17] = "0000000000000000";

static uint32_t fnv1a32(const char *s) {
  uint32_t h = 2166136261UL;
  while (*s) {
    h ^= (uint8_t)*s++;
    h *= 16777619UL;
  }
  return h;
}

static bool parseU32(const char *s, uint32_t *out) {
  if (!s || !*s) return false;
  char *end = nullptr;
  unsigned long v = strtoul(s, &end, 10);
  if (!end || *end) return false;
  *out = (uint32_t)v;
  return true;
}

static void copyField(char *dst, size_t dstLen, const char *src) {
  if (!dst || !dstLen) return;
  if (!src) src = "";
  strncpy(dst, src, dstLen - 1);
  dst[dstLen - 1] = '\0';
}

static void parsePea4Presence(char *line) {
  if (!strstr(line, "[PEA4/PN]")) return;
  char *h = strstr(line, " H=");
  char *best = strstr(line, " best=");
  char *mode = strstr(line, " mode=");
  char *role = strstr(line, " role=");
  char *jp4 = strstr(line, " jp4=");
  char *digest = strstr(line, " digest=");
  if (h) p4HashRate = strtoul(h + 3, nullptr, 10);
  if (best) {
    p4BestBits = (uint16_t)strtoul(best + 6, nullptr, 10);
    char *slash = strchr(best + 6, '/');
    if (slash) p4TargetBits = (uint16_t)strtoul(slash + 1, nullptr, 10);
  }
  if (mode) sscanf(mode + 6, "%9s", p4Mode);
  if (role) sscanf(role + 6, "%7s", p4Role);
  if (jp4) p4Jp4Frames = strtoul(jp4 + 5, nullptr, 10);
  if (digest) {
    char tmp[24] = {0};
    sscanf(digest + 8, "%16s", tmp);
    copyField(p4Digest16, sizeof(p4Digest16), tmp);
  }
  lastP4Ms = millis();
  p4ParseOk++;
}

static void parseJp4(char *line) {
  if (strncmp(line, "JP4,", 4)) return;
  char *star = strchr(line, '*');
  if (star) *star = '\0';
  char *fields[12] = {0};
  uint8_t count = 0;
  char *tok = strtok(line, ",");
  while (tok && count < 12) {
    fields[count++] = tok;
    tok = strtok(nullptr, ",");
  }
  if (count != 12) {
    p4ParseBad++;
    return;
  }
  copyField(p4Role, sizeof(p4Role), fields[3]);
  copyField(p4Mode, sizeof(p4Mode), fields[4]);
  uint32_t z = 0, hps = 0;
  parseU32(fields[9], &z);
  p4BestBits = (uint16_t)z;
  p4HashRate = (uint32_t)atof(fields[11]);
  (void)hps;
  copyField(p4Digest16, sizeof(p4Digest16), fields[10]);
  p4Digest16[16] = '\0';
  p4Jp4Frames++;
  lastP4Ms = millis();
  p4ParseOk++;
}

static void handleLine(char *line) {
  parsePea4Presence(line);
  parseJp4(line);
}

static void pumpStream(Stream &s, char *buf, size_t &len) {
  while (s.available()) {
    char c = (char)s.read();
    if (c == '\n') {
      buf[len] = '\0';
      handleLine(buf);
      len = 0;
    } else if (c != '\r' && len < 255) {
      buf[len++] = c;
    }
  }
}

static void onSent(
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 5, 0)
  const wifi_tx_info_t *,
#else
  const uint8_t *,
#endif
  esp_now_send_status_t status) {
  if (status == ESP_NOW_SEND_SUCCESS) txOk++;
  else txFail++;
}

static void ensureEspNow() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(JANUS_SWARM_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);
  if (esp_now_init() != ESP_OK) {
    Serial.println("[PEA4B] ESP-NOW init failed");
    delay(500);
    ESP.restart();
  }
  esp_now_register_send_cb(onSent);
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6);
  peer.channel = JANUS_SWARM_CHANNEL;
  peer.encrypt = false;
  esp_now_add_peer(&peer);
}

static void sendPresence() {
  SwarmSensePacket ss{};
  ss.magic[0] = 'S';
  ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = (uint16_t)(fnv1a32("PEA4") & 0xFFFF);
  copyField(ss.nodeId, sizeof(ss.nodeId), "PEA4");
  copyField(ss.kind, sizeof(ss.kind), "p4_titan_bridge");
  ss.seq = ++seqNo;
  ss.uptime_ms = millis();
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = 0;
  ss.loop_max_us = 0;
  ss.rssi = 0;
  ss.radio_mode = 10;
  ss.bt_flags = 0;
  ss.palette = 4;
  ss.knn_label = p4BestBits >= p4TargetBits ? 2 : 1;
  ss.knn_confidence = lastP4Ms && millis() - lastP4Ms < 6000UL ? 92 : 38;
  ss.ai_hint = 44;
  ss.thermal_load = 30;
  ss.effective_batch = 0;
  ss.dynamic_batch = 0;
  ss.hash_rate = p4HashRate;
  ss.total_hashes = p4Jp4Frames;
  ss.best_bits = p4BestBits;
  ss.hash_eff_x1000 = p4TargetBits ? (uint16_t)min(3000UL, (uint32_t)p4BestBits * 1000UL / p4TargetBits) : 0;
  ss.prediction_error_x1000 = 0;
  ss.entropy_x1000 = (uint16_t)(fnv1a32(p4Digest16) % 1000U);
  ss.touch_delta = 0;
  ss.job_age_s = lastP4Ms ? (uint16_t)min(65535UL, (millis() - lastP4Ms) / 1000UL) : 65535;
  ss.nonce_remaining_l16 = 0;
  ss.flags = 0x0100; // external bridge
  if (lastP4Ms && millis() - lastP4Ms < 6000UL) ss.flags |= 0x0001;
  if (p4BestBits >= p4TargetBits) ss.flags |= 0x0002;

  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t *)&ss, sizeof(ss));
  if (err != ESP_OK) txFail++;
  Serial.printf("[PEA4B] tx err=%d node=PEA4 role=%s mode=%s H=%lu best=%u/%u p4Age=%lums ok/bad=%lu/%lu tx=%lu/%lu digest=%s\n",
                (int)err, p4Role, p4Mode, (unsigned long)p4HashRate,
                p4BestBits, p4TargetBits,
                lastP4Ms ? (unsigned long)(millis() - lastP4Ms) : 0UL,
                (unsigned long)p4ParseOk, (unsigned long)p4ParseBad,
                (unsigned long)txOk, (unsigned long)txFail, p4Digest16);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(300);
  Serial1.begin(PEA4_UART_BAUD, SERIAL_8N1, PEA4_UART_RX, PEA4_UART_TX);
  ensureEspNow();
  Serial.printf("[PEA4B] ready %s ch=%u uart1 rx=%u tx=%u observer_only=1\n",
                PEA4_BRIDGE_VERSION, JANUS_SWARM_CHANNEL, PEA4_UART_RX, PEA4_UART_TX);
}

void loop() {
  pumpStream(Serial, line0, line0Len);
  pumpStream(Serial1, line1, line1Len);
  uint32_t now = millis();
  if (now - lastPresenceMs >= PRESENCE_MS) {
    lastPresenceMs = now;
    sendPresence();
  }
}
