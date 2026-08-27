/*
  JANUS ADV_ELITE RC1 — PATCHWORK FLASH CANDIDATE
  ------------------------------------------------
  Purpose: first hardware-flash candidate for the single final ADV_Elite.

  Preserved doctrine:
  - one ADV, one body;
  - real sensor truth != prediction != simulation;
  - Blind Eye is ESP-NOW input (it has no microphone);
  - anomaly detector is always on and independent from Love/House/game;
  - 1488 is the manual extended M2R gate;
  - J is manual LoRa gate; LOVE context = HOUSE && LORA;
  - ENTER is absolute audio mute;
  - [ ] share one brightness axis for display + enabled LED;
  - L gates only the LED;
  - foreground O/Z/R/D/A never stops P0/P1/P2 core work;
  - game/pet/visuals are FICTIONAL or VISUALIZATION and never OBSERVED_REAL.

  Donor lineage used in RC1:
  Beacon v4.5A / FIX12, Elite Zero v0.20 hardware organs,
  Atom AutoRPG / Death&Rebirth / Spore / TD_SWARM gameplay ideas,
  Zim autonomous-specialist resource courtesy.
*/

#include <M5Cardputer.h>
#include <FastLED.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_arduino_version.h>
#include <mbedtls/sha256.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <M5UnitENV.h>
#include <TinyGPSPlus.h>
#include <math.h>

#if __has_include("ADV_Elite_local_config.h")
  #include "ADV_Elite_local_config.h"
#endif
#ifndef ADV_WIFI_SSID
  #define ADV_WIFI_SSID ""
#endif
#ifndef ADV_WIFI_PASSWORD
  #define ADV_WIFI_PASSWORD ""
#endif
#ifndef ADV_ESPNOW_CHANNEL
  #define ADV_ESPNOW_CHANNEL 10
#endif

#if __has_include(<RadioLib.h>)
  #include <RadioLib.h>
  #define ADV_HAS_RADIOLIB 1
#else
  #define ADV_HAS_RADIOLIB 0
#endif

#include "../adv_elite/ADV_Elite_mode_contract.h"
#include "../adv_elite/ADV_Elite_illumination_policy.h"
#include "../adv_elite/ADV_Elite_alien_survival_runtime.h"

using namespace janus_adv_elite;

// -------------------------- hardware --------------------------
static constexpr uint8_t GROVE_SDA_PIN = 2;
static constexpr uint8_t GROVE_SCL_PIN = 1;
static constexpr uint8_t LED_PIN = 21;
static constexpr uint8_t SD_SCK_PIN = 40;
static constexpr uint8_t SD_MISO_PIN = 39;
static constexpr uint8_t SD_MOSI_PIN = 14;
static constexpr uint8_t SD_CS_PIN = 12;
static constexpr uint8_t GNSS_RX_PIN = 15;
static constexpr uint8_t GNSS_TX_PIN = 13;
static constexpr uint32_t GNSS_BAUD = 115200UL;

static constexpr uint8_t LORA_NSS = 5;
static constexpr uint8_t LORA_IRQ = 4;
static constexpr uint8_t LORA_RST = 3;
static constexpr uint8_t LORA_BUSY = 6;
static constexpr uint8_t LORA_PWR_EN = 10;

CRGB advLed[1];
SHT3X advSht;
QMP6988 advQmp;
TinyGPSPlus advGps;
#if ADV_HAS_RADIOLIB
SX1262 advRadio = new Module(LORA_NSS, LORA_IRQ, LORA_RST, LORA_BUSY);
#endif

// -------------------------- core state --------------------------
struct CoreTelemetry {
  float tempC = NAN;
  float humidity = NAN;
  float pressureHpa = NAN;
  bool shtValid = false;
  bool qmpValid = false;
  uint32_t envFreshMs = 0;

  float ax = 0, ay = 0, az = 1;
  float gx = 0, gy = 0, gz = 0;
  float imuShock = 1.0f;
  float imuPred = 1.0f;
  float imuLoss = 0.0f;
  bool imuValid = false;

  float entropy = 0.20f;
  float predEntropy = 0.20f;
  float loss = 0.0f;
  float trend = 0.0f;
  float future1 = 0.20f;
  float future2 = 0.20f;
  float future3 = 0.20f;

  int battery = 0;
  int8_t wifiRssi = -127;
  uint32_t freeHeap = 0;
  uint32_t loopUs = 0;
  uint32_t loopMaxUs = 0;
} core;

struct BlindEyeState {
  bool online = false;
  uint32_t lastOkMs = 0;
  float presence = NAN;
  float motion = NAN;
  float magNorm = NAN;
  float shock = NAN;
  float activity = NAN;
  float predActivity = NAN;
  float loss = NAN;
  float sync = NAN;
  int8_t rssi = -127;
  char nodeId[24] = "none";
} eye;

struct AudioNodeState {
  bool online = false;
  uint32_t lastOkMs = 0;
  float micRms = NAN;
  float entropy = NAN;
  float loss = NAN;
  char nodeId[24] = "none";
} audioNode;

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

ModeContract mode;
IlluminationPolicy illumination;
AlienSurvivalRuntime alien;
AlienPerformanceGovernor alienGovernor;
Preferences prefs;

bool houseActive = false;
bool m2rActive = false;
bool loraActive = false;
bool loraReady = false;
bool hardMute = false;
bool brainWaveEnabled = true;
bool anomalyLatched = false;
uint8_t masterVolume = 96;
float visualGain = 1.0f;
uint8_t petAction = 0;
uint32_t anomalyCount = 0;
uint32_t witnessCount = 0;
uint32_t espRxCount = 0;
uint32_t espTxCount = 0;
uint32_t espTxFail = 0;
uint32_t heartbeatSeq = 0;
String statusLine = "boot";
String codeBuffer;

static constexpr uint32_t ENV_INTERVAL_MS = 1000UL;
static constexpr uint32_t ENV_TTL_MS = 15000UL;
static constexpr uint32_t CORE_INTERVAL_MS = 120UL;
static constexpr uint32_t HEARTBEAT_MS = 2000UL;
static constexpr uint32_t DRAW_INTERVAL_MS = 45UL;
static constexpr uint32_t WITNESS_FLUSH_MS = 15000UL;
static constexpr uint32_t BRAINWAVE_MIN_NOTE_MS = 110UL;
static constexpr int HIST_N = 120;

float histEntropy[HIST_N] = {};
float histLoss[HIST_N] = {};
float histTemp[HIST_N] = {};
float histHum[HIST_N] = {};
float histMotion[HIST_N] = {};
float histSelf[HIST_N] = {};
uint16_t histPos = 0;
uint16_t histCount = 0;

uint32_t lastEnvMs = 0;
uint32_t lastCoreMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastDrawMs = 0;
uint32_t lastWitnessFlushMs = 0;
uint32_t lastBrainNoteMs = 0;
uint32_t lastLoopStartUs = 0;
uint32_t lastGpsSealMs = 0;
uint8_t brainStep = 0;

char witnessPrevHash[65] = "GENESIS";
static const char* WITNESS_LFS = "/adv_witness.jsonl";
static const char* WITNESS_SD = "/janus/adv_witness.jsonl";

// -------------------------- pet: simulated only --------------------------
struct PetState {
  float hunger = 18;
  float thirst = 15;
  float dirt = 8;
  float energy = 82;
  float mood = 75;
  float health = 100;
  uint32_t ageMinutes = 0;
  uint32_t lastTickMs = 0;
} pet;

const char* petActions[] = {"FEED", "DRINK", "CLEAN", "PLAY", "SLEEP", "CHECK"};
static constexpr uint8_t PET_ACTION_COUNT = sizeof(petActions) / sizeof(petActions[0]);

// -------------------------- helpers --------------------------
float clamp01(float x) { return constrain(x, 0.0f, 1.0f); }

bool loveContextActive() {
  return houseActive && loraActive;
}

uint16_t uiPrimary() {
  return houseActive ? M5Cardputer.Display.color565(255, 145, 0) : TFT_CYAN;
}
uint16_t uiSecondary() {
  return houseActive ? M5Cardputer.Display.color565(155, 88, 0) : TFT_DARKGREY;
}

void applyVolume() {
  M5Cardputer.Speaker.setVolume(hardMute ? 0 : masterVolume);
  if (hardMute) M5Cardputer.Speaker.stop();
}

void playUiTone(uint16_t freq, uint16_t ms) {
  if (hardMute || masterVolume == 0) return;
  // Radio will later own the renderer while actually playing; RC1 radio has no decoder yet.
  if (mode.mode == ForegroundMode::RADIO_R) return;
  M5Cardputer.Speaker.tone(freq, ms);
}

void applyIllumination() {
  M5Cardputer.Display.setBrightness(illumination.displayBrightness());
  FastLED.setBrightness(illumination.ledBrightness());
}

void updateLed() {
  if (!illumination.led_enabled || illumination.ledBrightness() == 0) {
    advLed[0] = CRGB::Black;
    FastLED.show();
    return;
  }

  if (anomalyLatched) {
    advLed[0] = CRGB::White;
  } else if (mode.mode == ForegroundMode::ALIEN_SURVIVAL_A) {
    AlienRgb c = AlienLedPolicy::healthColor(alien.health01());
    advLed[0] = CRGB(c.r, c.g, c.b);
  } else if (houseActive) {
    advLed[0] = CRGB(255, 140, 0);
  } else {
    uint8_t hue = (uint8_t)constrain(160 - (int)(core.entropy * 14.0f), 0, 160);
    advLed[0] = CHSV(hue, 255, 255);
  }
  FastLED.show();
}

void sha256Hex(const String& text, char out[65]) {
  uint8_t hash[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, (const unsigned char*)text.c_str(), text.length());
  mbedtls_sha256_finish(&ctx, hash);
  mbedtls_sha256_free(&ctx);
  static const char hx[] = "0123456789abcdef";
  for (int i = 0; i < 32; ++i) {
    out[i * 2] = hx[(hash[i] >> 4) & 0xF];
    out[i * 2 + 1] = hx[hash[i] & 0xF];
  }
  out[64] = 0;
}

void ensureSdWitnessDir() {
  if (!SD.exists("/janus")) SD.mkdir("/janus");
}

void witness(const char* type, const char* detail, const char* truthClass = "CONTROL_STATE") {
  String payload = "{";
  payload += "\"ts_ms\":" + String(millis()) + ",";
  payload += "\"type\":\"" + String(type) + "\",";
  payload += "\"truth_class\":\"" + String(truthClass) + "\",";
  payload += "\"detail\":\"" + String(detail) + "\",";
  payload += "\"entropy\":" + String(core.entropy, 5) + ",";
  payload += "\"pred\":" + String(core.predEntropy, 5) + ",";
  payload += "\"loss\":" + String(core.loss, 5) + ",";
  payload += "\"eye_online\":" + String(eye.online ? "true" : "false") + ",";
  payload += "\"prev_hash\":\"" + String(witnessPrevHash) + "\"";
  payload += "}";

  char newHash[65];
  sha256Hex(String(witnessPrevHash) + payload, newHash);
  String line = payload.substring(0, payload.length() - 1) + ",\"hash\":\"" + String(newHash) + "\"}";

  File f = LittleFS.open(WITNESS_LFS, FILE_APPEND);
  if (f) { f.println(line); f.close(); }
  if (SD.cardType() != CARD_NONE) {
    ensureSdWitnessDir();
    File sf = SD.open(WITNESS_SD, FILE_APPEND);
    if (sf) { sf.println(line); sf.close(); }
  }
  strlcpy(witnessPrevHash, newHash, sizeof(witnessPrevHash));
  ++witnessCount;
}

float meanArr(const float* a, int n) {
  if (n <= 0) return 0;
  float s = 0;
  for (int i = 0; i < n; ++i) s += a[i];
  return s / n;
}

float stdArr(const float* a, int n, float m) {
  if (n <= 1) return 0;
  float s = 0;
  for (int i = 0; i < n; ++i) { float d = a[i] - m; s += d * d; }
  return sqrtf(s / (n - 1));
}

void appendHistory() {
  histEntropy[histPos] = core.entropy;
  histLoss[histPos] = core.loss;
  histTemp[histPos] = core.shtValid ? core.tempC : NAN;
  histHum[histPos] = core.shtValid ? core.humidity : NAN;
  histMotion[histPos] = eye.online && isfinite(eye.motion) ? eye.motion : core.imuLoss;
  histSelf[histPos] = (float)core.loopUs;
  histPos = (histPos + 1) % HIST_N;
  if (histCount < HIST_N) ++histCount;
}

// -------------------------- sensors + predictor --------------------------
void readEnv() {
  bool any = false;
  if (advSht.update()) {
    core.tempC = advSht.cTemp;
    core.humidity = advSht.humidity;
    core.shtValid = isfinite(core.tempC) && isfinite(core.humidity) &&
                    core.tempC > -40.0f && core.tempC < 90.0f &&
                    core.humidity >= 0.0f && core.humidity <= 100.0f;
    any |= core.shtValid;
  }
  if (advQmp.update()) {
    core.pressureHpa = advQmp.pressure / 100.0f;
    core.qmpValid = isfinite(core.pressureHpa) && core.pressureHpa > 300.0f && core.pressureHpa < 1200.0f;
    any |= core.qmpValid;
  }
  if (any) core.envFreshMs = millis();
  if (millis() - core.envFreshMs > ENV_TTL_MS) {
    core.shtValid = false;
    core.qmpValid = false;
  }
}

void readImu() {
  float ax = 0, ay = 0, az = 1, gx = 0, gy = 0, gz = 0;
  M5.Imu.getAccelData(&ax, &ay, &az);
  M5.Imu.getGyroData(&gx, &gy, &gz);
  if (!isfinite(ax) || !isfinite(ay) || !isfinite(az)) return;
  core.ax = ax; core.ay = ay; core.az = az;
  core.gx = gx; core.gy = gy; core.gz = gz;
  float amag = sqrtf(ax*ax + ay*ay + az*az);
  float gmag = sqrtf(gx*gx + gy*gy + gz*gz) * 0.010f;
  core.imuShock = amag + gmag;
  core.imuLoss = fabsf(core.imuShock - core.imuPred);
  core.imuPred = core.imuPred * 0.965f + core.imuShock * 0.035f;
  core.imuValid = true;
}

void updatePredictorAndAnomaly() {
  float prevEntropy = core.entropy;
  float envTerm = 0.0f;
  if (core.shtValid) {
    envTerm += fabsf(core.tempC - 23.0f) * 0.035f;
    envTerm += fabsf(core.humidity - 50.0f) * 0.006f;
  }
  if (core.qmpValid) envTerm += fabsf(core.pressureHpa - 1013.25f) * 0.002f;

  float eyeTerm = 0.0f;
  if (eye.online) {
    if (isfinite(eye.loss)) eyeTerm += constrain(eye.loss, 0.0f, 5.0f) * 0.50f;
    if (isfinite(eye.motion)) eyeTerm += constrain(fabsf(eye.motion), 0.0f, 1000.0f) * 0.0010f;
  }
  float imuTerm = core.imuValid ? core.imuLoss * 0.80f : 0.0f;

  core.entropy = constrain(0.05f + envTerm + eyeTerm + imuTerm, 0.01f, 10.0f);
  core.loss = fabsf(core.predEntropy - core.entropy);
  core.trend = core.trend * 0.86f + (core.entropy - prevEntropy) * 0.14f;
  core.predEntropy = core.predEntropy * 0.88f + core.entropy * 0.12f;
  core.future1 = constrain(core.predEntropy + core.trend * 1.0f, 0.01f, 10.0f);
  core.future2 = constrain(core.predEntropy + core.trend * 2.0f, 0.01f, 10.0f);
  core.future3 = constrain(core.predEntropy + core.trend * 3.0f, 0.01f, 10.0f);

  appendHistory();

  bool anomaly = false;
  if (histCount >= 20) {
    float mE = meanArr(histEntropy, histCount);
    float sE = stdArr(histEntropy, histCount, mE);
    float mL = meanArr(histLoss, histCount);
    float sL = stdArr(histLoss, histCount, mL);
    float zE = sE > 1e-5f ? fabsf((core.entropy - mE) / sE) : 0.0f;
    float zL = sL > 1e-5f ? fabsf((core.loss - mL) / sL) : 0.0f;
    float disagreement = 0.0f;
    if (eye.online && isfinite(eye.activity) && isfinite(eye.predActivity)) {
      disagreement = fabsf(eye.activity - eye.predActivity);
    }
    anomaly = zE > 4.2f || zL > 4.0f || disagreement > 2.25f;
  }

  if (anomaly && !anomalyLatched) {
    anomalyLatched = true;
    ++anomalyCount;
    statusLine = "ANOMALY / WITNESS";
    witness("ANOMALY", "multiview_detector", "DERIVED_FROM_REAL");
    playUiTone(820, 80);
  } else if (!anomaly) {
    anomalyLatched = false;
  }
}

// -------------------------- ESP-NOW --------------------------
uint8_t broadcastMac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
int8_t lastEspRssi = -127;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (info && info->rx_ctrl) lastEspRssi = info->rx_ctrl->rssi;
#else
void onEspRecv(const uint8_t* mac, const uint8_t* data, int len) {
#endif
  if (!data || len <= 0) return;
  ++espRxCount;
  if (len == (int)sizeof(EntropyReportV2) && data[0] == 'E' && data[1] == '2') {
    EntropyReportV2 er{};
    memcpy(&er, data, sizeof(er));
    er.nodeId[sizeof(er.nodeId)-1] = 0;
    String node(er.nodeId);
    if (node.indexOf("BlindEye") >= 0 || node.indexOf("Blind") >= 0) {
      eye.online = true;
      eye.lastOkMs = millis();
      eye.presence = er.values[1];
      eye.motion = er.values[2];
      eye.magNorm = er.values[3];
      eye.shock = er.values[4];
      eye.activity = er.values[5];
      eye.predActivity = er.values[6];
      eye.loss = er.prediction_error;
      eye.sync = er.sync_hint;
      eye.rssi = lastEspRssi;
      strlcpy(eye.nodeId, er.nodeId, sizeof(eye.nodeId));
    }
    if (node.indexOf("EchoMic") >= 0 || node.indexOf("Audio") >= 0) {
      audioNode.online = true;
      audioNode.lastOkMs = millis();
      audioNode.micRms = er.values[0];
      audioNode.entropy = er.local_entropy;
      audioNode.loss = er.prediction_error;
      strlcpy(audioNode.nodeId, er.nodeId, sizeof(audioNode.nodeId));
    }
  }
}

void initEspNow() {
  WiFi.mode(WIFI_STA);
  bool hasWifiConfig = strlen(ADV_WIFI_SSID) > 0;
  if (hasWifiConfig) {
    WiFi.begin(ADV_WIFI_SSID, ADV_WIFI_PASSWORD);
    uint32_t until = millis() + 3500UL;
    while (WiFi.status() != WL_CONNECTED && millis() < until) delay(25);
  }
  if (WiFi.status() != WL_CONNECTED) {
    esp_wifi_set_channel(ADV_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE);
  }
  if (esp_now_init() != ESP_OK) {
    statusLine = "ESP-NOW FAIL";
    return;
  }
  esp_now_register_recv_cb(onEspRecv);
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, broadcastMac, 6);
  peer.channel = 0;
  peer.encrypt = false;
  if (!esp_now_is_peer_exist(broadcastMac)) esp_now_add_peer(&peer);
}

void sendHeartbeat() {
  JanusColonyPacket p{};
  memcpy(p.magic, "JANUS", 6);
  strlcpy(p.nodeId, "ADV_Elite", sizeof(p.nodeId));
  strlcpy(p.role, "ADV_ELITE", sizeof(p.role));
  p.seq = ++heartbeatSeq;
  p.hashRate = 0;
  p.shares = 0;
  p.rejects = 0;
  p.bestBits = 0;
  p.diff = 0;
  p.targetBits = 0;
  p.aiBatch = 0;
  p.aiHint = anomalyLatched ? 2 : 1;
  p.jobAgeMs = 0;
  p.rssi = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : lastEspRssi;
  p.uptime = millis() / 1000UL;
  esp_err_t e = esp_now_send(broadcastMac, (uint8_t*)&p, sizeof(p));
  if (e == ESP_OK) ++espTxCount; else ++espTxFail;
}

// -------------------------- CAP / LoRa / GNSS --------------------------
void initGnssAndLoRa() {
  Serial2.begin(GNSS_BAUD, SERIAL_8N1, GNSS_RX_PIN, GNSS_TX_PIN);
#if ADV_HAS_RADIOLIB
  pinMode(LORA_PWR_EN, OUTPUT);
  digitalWrite(LORA_PWR_EN, HIGH);
  delay(80);
  int st = advRadio.begin(868.0, 125.0, 9, 7, 0x34, 10, 8, 1.6, false);
  loraReady = (st == RADIOLIB_ERR_NONE);
  if (loraReady) { advRadio.setOutputPower(10); advRadio.setCRC(true); }
#else
  loraReady = false;
#endif
}

void gnssTick() {
  while (Serial2.available()) advGps.encode((char)Serial2.read());
}

void maybeSendLoRaSeal() {
#if ADV_HAS_RADIOLIB
  if (!loraActive || !loraReady) return;
  if (millis() - lastGpsSealMs < 30000UL) return;
  lastGpsSealMs = millis();
  String s = "JSA3|ADV|";
  s += String(millis()/1000UL);
  s += "|E=" + String(core.entropy, 2);
  s += "|A=" + String(anomalyLatched ? 1 : 0);
  if (advGps.location.isValid()) {
    // Privacy boundary: raw GNSS is not broadcast. Only fix quality is emitted.
    s += "|FIX=1|SAT=" + String(advGps.satellites.value());
  } else s += "|FIX=0";
  advRadio.transmit(s.c_str());
#endif
}

// -------------------------- BrainWave synthesis core --------------------------
const uint16_t bwPhraseA[8] = {220, 277, 330, 440, 392, 330, 277, 220};
const uint16_t bwPhraseB[8] = {196, 247, 294, 392, 349, 294, 247, 196};

void brainWaveTick() {
  if (!brainWaveEnabled || hardMute || masterVolume == 0) return;
  if (mode.mode == ForegroundMode::RADIO_R || mode.mode == ForegroundMode::ALIEN_SURVIVAL_A) return;
  uint32_t now = millis();
  uint32_t interval = (uint32_t)constrain(420.0f - core.entropy * 28.0f, 110.0f, 420.0f);
  if (now - lastBrainNoteMs < interval) return;
  lastBrainNoteMs = now;
  const uint16_t* phrase = houseActive ? bwPhraseB : bwPhraseA;
  float swarmShift = eye.online && isfinite(eye.sync) ? 1.0f + constrain(eye.sync,0.0f,1.0f)*0.08f : 1.0f;
  int note = (int)(phrase[brainStep] * (1.0f + core.entropy * 0.012f) * swarmShift);
  M5Cardputer.Speaker.tone(note, (uint32_t)(interval * 0.65f));
  brainStep = (brainStep + 1) & 7;
}

// -------------------------- pet --------------------------
void loadPet() {
  prefs.begin("adv_pet", true);
  pet.hunger = prefs.getFloat("hung", 18);
  pet.thirst = prefs.getFloat("thir", 15);
  pet.dirt = prefs.getFloat("dirt", 8);
  pet.energy = prefs.getFloat("ener", 82);
  pet.mood = prefs.getFloat("mood", 75);
  pet.health = prefs.getFloat("heal", 100);
  pet.ageMinutes = prefs.getUInt("age", 0);
  prefs.end();
  pet.lastTickMs = millis();
}

void savePet() {
  prefs.begin("adv_pet", false);
  prefs.putFloat("hung", pet.hunger);
  prefs.putFloat("thir", pet.thirst);
  prefs.putFloat("dirt", pet.dirt);
  prefs.putFloat("ener", pet.energy);
  prefs.putFloat("mood", pet.mood);
  prefs.putFloat("heal", pet.health);
  prefs.putUInt("age", pet.ageMinutes);
  prefs.end();
}

void petTick() {
  uint32_t now = millis();
  if (pet.lastTickMs == 0) pet.lastTickMs = now;
  uint32_t elapsed = now - pet.lastTickMs;
  if (elapsed < 60000UL) return;
  uint32_t mins = elapsed / 60000UL;
  pet.lastTickMs += mins * 60000UL;
  pet.ageMinutes += mins;
  float m = (float)mins;
  pet.hunger = constrain(pet.hunger + 0.22f*m, 0.0f, 100.0f);
  pet.thirst = constrain(pet.thirst + 0.30f*m, 0.0f, 100.0f);
  pet.dirt = constrain(pet.dirt + 0.13f*m, 0.0f, 100.0f);
  pet.energy = constrain(pet.energy - 0.16f*m, 0.0f, 100.0f);
  float comfort = 1.0f;
  if (core.shtValid) {
    comfort -= constrain(fabsf(core.tempC - 23.0f) / 30.0f, 0.0f, 0.6f);
    comfort -= constrain(fabsf(core.humidity - 50.0f) / 100.0f, 0.0f, 0.3f);
  }
  float neglect = (pet.hunger + pet.thirst + pet.dirt + (100.0f-pet.energy)) * 0.0025f;
  pet.mood = constrain(pet.mood + (comfort - neglect - 0.45f)*m*0.12f, 0.0f, 100.0f);
  if (pet.hunger > 90 || pet.thirst > 90 || pet.dirt > 95 || pet.energy < 5) pet.health = constrain(pet.health - 0.35f*m, 1.0f, 100.0f);
  else pet.health = constrain(pet.health + 0.05f*m, 1.0f, 100.0f);
  if ((pet.ageMinutes % 15) < mins) savePet();
}

void petDoAction() {
  switch (petAction) {
    case 0: pet.hunger = constrain(pet.hunger - 26.0f, 0.0f, 100.0f); break;
    case 1: pet.thirst = constrain(pet.thirst - 32.0f, 0.0f, 100.0f); break;
    case 2: pet.dirt = constrain(pet.dirt - 38.0f, 0.0f, 100.0f); break;
    case 3: pet.mood = constrain(pet.mood + 18.0f, 0.0f, 100.0f); pet.energy = constrain(pet.energy - 8.0f,0.0f,100.0f); break;
    case 4: pet.energy = constrain(pet.energy + 34.0f, 0.0f, 100.0f); break;
    default: break;
  }
  savePet();
  playUiTone(980 + petAction*70, 35);
}

// -------------------------- rendering --------------------------
void drawTicker() {
  String ticker = "JANUS // ";
  ticker += statusLine;
  ticker += " // E " + String(core.entropy,2);
  ticker += " P " + String(core.predEntropy,2);
  ticker += " // EYE "; ticker += eye.online ? "ON" : "STALE";
  ticker += " // W " + String(witnessCount);
  int width = ticker.length() * 6;
  int x = 240 - (int)((millis()/70UL) % (uint32_t)(width + 240));
  M5Cardputer.Display.fillRect(0, 122, 240, 13, TFT_BLACK);
  M5Cardputer.Display.setTextColor(uiSecondary(), TFT_BLACK);
  M5Cardputer.Display.setCursor(x, 124);
  M5Cardputer.Display.print(ticker);
}

void drawHome() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextSize(1);
  d.setTextColor(uiPrimary(), TFT_BLACK);
  d.setCursor(2,2); d.print("JANUS ADV_ELITE RC1");
  d.setCursor(184,2); d.printf("B%02d%%", core.battery);

  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.setCursor(2,16); d.printf("ENV  T:%s  H:%s  P:%s",
    core.shtValid ? String(core.tempC,1).c_str() : "?",
    core.shtValid ? String(core.humidity,0).c_str() : "?",
    core.qmpValid ? String(core.pressureHpa,0).c_str() : "?");
  d.setCursor(2,28); d.printf("IMU  sh %.2f  err %.3f", core.imuShock, core.imuLoss);

  d.setTextColor(TFT_CYAN, TFT_BLACK);
  d.setCursor(2,42); d.printf("E %.3f  pred %.3f  loss %.3f", core.entropy, core.predEntropy, core.loss);
  d.setCursor(2,54); d.printf("FUT %.2f > %.2f > %.2f", core.future1, core.future2, core.future3);

  d.setTextColor(eye.online ? TFT_GREEN : TFT_DARKGREY, TFT_BLACK);
  d.setCursor(2,68); d.printf("EYE %s  TM:%s/%s  SY:%s", eye.online?"ON":"STALE",
    isfinite(eye.presence)?String(eye.presence,0).c_str():"?",
    isfinite(eye.motion)?String(eye.motion,0).c_str():"?",
    isfinite(eye.sync)?String(eye.sync,2).c_str():"?");

  d.setTextColor(anomalyLatched ? TFT_WHITE : uiSecondary(), TFT_BLACK);
  d.setCursor(2,82); d.printf("ANOM:%s #%lu  WIT:%lu  RX:%lu", anomalyLatched?"YES":"no", (unsigned long)anomalyCount, (unsigned long)witnessCount, (unsigned long)espRxCount);
  d.setCursor(2,94); d.printf("M2R:%s  H:%s  J:%s  LOVE:%s", m2rActive?"ON":"off", houseActive?"ON":"off", loraActive?"ON":"off", loveContextActive()?"ON":"off");
  d.setCursor(2,106); d.printf("O scope | Z self | R radio | D pet | A alien");
  drawTicker();
}

float histValue(VisualizerSource src, int idx) {
  switch (src) {
    case VisualizerSource::ENV: return isfinite(histTemp[idx]) ? histTemp[idx] : 0;
    case VisualizerSource::MOTION: return histMotion[idx];
    case VisualizerSource::SWARM: return eye.online && isfinite(eye.activity) ? eye.activity : histEntropy[idx];
    case VisualizerSource::PREDICTOR: return histLoss[idx];
    case VisualizerSource::SELF: return histSelf[idx] / 1000.0f;
    default: return histEntropy[idx];
  }
}

const char* visualName(VisualizerSource s) {
  switch (s) {
    case VisualizerSource::ENV: return "ENV";
    case VisualizerSource::MOTION: return "MOTION";
    case VisualizerSource::SWARM: return "SWARM";
    case VisualizerSource::PREDICTOR: return "PREDICTOR";
    case VisualizerSource::SELF: return "SELF";
    case VisualizerSource::KALEIDOSCOPE: return "KALEIDOSCOPE";
    default: return "?";
  }
}

void drawVisualizer() {
  auto& d = M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextColor(uiPrimary(), TFT_BLACK);
  d.setCursor(2,2); d.printf("O:%s  gain %.1f", visualName(mode.visualizer_source), visualGain);
  d.setTextColor(TFT_DARKGREY, TFT_BLACK);
  d.setCursor(150,2); d.print("<- -> source  ESC");

  if (mode.visualizer_source == VisualizerSource::KALEIDOSCOPE) {
    // Presentation-only deterministic transform of real WORLD+SWARM+SELF state.
    float e = core.entropy;
    float m = core.imuLoss + (eye.online && isfinite(eye.motion) ? fabsf(eye.motion)*0.001f : 0.0f);
    int cx = 120, cy = 72;
    uint16_t c = d.color565((uint8_t)constrain(40+e*35,0.0f,255.0f), (uint8_t)constrain(80+m*80,0.0f,255.0f), 210);
    for (int ring = 1; ring <= 7; ++ring) {
      float r = 8 + ring * 7 + sinf(millis()*0.001f*ring + e) * 4;
      for (int k = 0; k < 8; ++k) {
        float a = k * PI / 4.0f + millis()*0.00025f*(ring&1?1:-1);
        int x = cx + cosf(a)*r;
        int y = cy + sinf(a)*r*0.65f;
        d.drawCircle(x,y,1+(ring&1),c);
      }
    }
    d.setTextColor(TFT_DARKGREY,TFT_BLACK); d.setCursor(2,124); d.print("VISUALIZATION - NOT EVIDENCE");
    return;
  }

  d.drawRect(3,17,234,106,TFT_DARKGREY);
  if (histCount < 2) return;
  float minV = 1e9f, maxV = -1e9f;
  for (int i=0;i<histCount;i++) {
    float v = histValue(mode.visualizer_source, i);
    if (!isfinite(v)) continue;
    if (v<minV) minV=v; if (v>maxV) maxV=v;
  }
  if (!(maxV>minV)) { maxV=minV+1.0f; }
  float mid = (minV+maxV)*0.5f;
  float span = (maxV-minV)/visualGain;
  minV=mid-span*0.5f; maxV=mid+span*0.5f;
  for (int i=0;i<histCount-1;i++) {
    int idx=(histPos + HIST_N - histCount + i)%HIST_N;
    int idx2=(idx+1)%HIST_N;
    float v1=histValue(mode.visualizer_source,idx);
    float v2=histValue(mode.visualizer_source,idx2);
    int x1=4 + i*232/max(1,(int)histCount-1);
    int x2=4 + (i+1)*232/max(1,(int)histCount-1);
    int y1=121-(int)(constrain((v1-minV)/(maxV-minV),0.0f,1.0f)*101);
    int y2=121-(int)(constrain((v2-minV)/(maxV-minV),0.0f,1.0f)*101);
    d.drawLine(x1,y1,x2,y2,uiPrimary());
  }
  d.setTextColor(TFT_LIGHTGREY,TFT_BLACK); d.setCursor(6,20); d.printf("%.2f .. %.2f",minV,maxV);
}

void drawZimView() {
  auto& d=M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextColor(TFT_YELLOW,TFT_BLACK); d.setCursor(2,2); d.print("Z / ADV RESOURCE VIEW");
  d.setTextColor(TFT_WHITE,TFT_BLACK);
  d.setCursor(2,18); d.print("PRIMARY: observe + predict + witness");
  d.setCursor(2,30); d.printf("P0 core: RUN  anomaly:%s", anomalyLatched?"LATCH":"watch");
  d.setCursor(2,42); d.printf("P1 predictor: RUN  M2R:%s",m2rActive?"FULL":"light");
  d.setCursor(2,54); d.printf("P2 swarm: RUN RX:%lu TX:%lu/%lu",(unsigned long)espRxCount,(unsigned long)espTxCount,(unsigned long)espTxFail);
  d.setCursor(2,66); d.printf("heap:%lu  loop:%luus max:%lu",(unsigned long)core.freeHeap,(unsigned long)core.loopUs,(unsigned long)core.loopMaxUs);
  d.setCursor(2,78); d.printf("P4 fg:%d  gameFPS:%d",(int)mode.mode,(int)alien.fpsEma());
  d.setCursor(2,90); d.printf("game discretionary:%u%%",alienGovernor.discretionary_budget_pct);
  d.setCursor(2,102); d.printf("P3 throttle:%s P5 defer:%s",alienGovernor.throttle_p3?"YES":"no",alienGovernor.defer_p5?"YES":"no");
  d.setTextColor(TFT_DARKGREY,TFT_BLACK); d.setCursor(2,122); d.print("read-only | ESC HOME");
}

void drawRadioStub() {
  auto& d=M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextColor(TFT_CYAN,TFT_BLACK); d.setCursor(2,2); d.print("R / INTERNET RADIO");
  d.setTextColor(TFT_WHITE,TFT_BLACK);
  d.setCursor(2,25); d.print("RC1: catalogue/audio decoder not linked yet.");
  d.setCursor(2,39); d.print("Core remains alive. No fake PLAY state.");
  d.setCursor(2,53); d.printf("WiFi: %s RSSI:%d",WiFi.status()==WL_CONNECTED?"ON":"OFF",core.wifiRssi);
  d.setCursor(2,67); d.print("Next hardware gate: MP3 stream -> speaker");
  d.setTextColor(TFT_DARKGREY,TFT_BLACK); d.setCursor(2,122); d.print("ESC HOME");
}

void drawPet() {
  auto& d=M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setTextColor(TFT_MAGENTA,TFT_BLACK); d.setCursor(2,2); d.print("D / JANUS PET [SIMULATED]");
  d.setTextColor(TFT_WHITE,TFT_BLACK);
  d.setCursor(2,18); d.printf("Hunger %3d  Thirst %3d",(int)pet.hunger,(int)pet.thirst);
  d.setCursor(2,30); d.printf("Dirt   %3d  Energy %3d",(int)pet.dirt,(int)pet.energy);
  d.setCursor(2,42); d.printf("Mood   %3d  Health %3d",(int)pet.mood,(int)pet.health);
  d.setCursor(2,54); d.printf("Age %lumin",(unsigned long)pet.ageMinutes);
  d.setCursor(2,68); d.printf("ENV comfort: %s",core.shtValid?"REAL CONTEXT":"UNKNOWN_ENV");
  d.setTextColor(TFT_YELLOW,TFT_BLACK); d.setCursor(2,88); d.printf("ACTION < %s >",petActions[petAction]);
  d.setTextColor(TFT_LIGHTGREY,TFT_BLACK); d.setCursor(2,104); d.print("<- -> choose   SPACE act");
  d.setTextColor(TFT_DARKGREY,TFT_BLACK); d.setCursor(2,122); d.print("FICTIONAL STATE | ESC HOME");
}

void drawCurrentMode() {
  switch (mode.mode) {
    case ForegroundMode::HOME: drawHome(); break;
    case ForegroundMode::VISUALIZER_O: drawVisualizer(); break;
    case ForegroundMode::ZIM_VIEW_Z: drawZimView(); break;
    case ForegroundMode::RADIO_R: drawRadioStub(); break;
    case ForegroundMode::TAMAGOTCHI_D: drawPet(); break;
    case ForegroundMode::ALIEN_SURVIVAL_A: alien.draw(M5Cardputer.Display); break;
  }
}

// -------------------------- input --------------------------
bool keyNow(char a, char b = 0) {
  return M5Cardputer.Keyboard.isKeyPressed(a) || (b && M5Cardputer.Keyboard.isKeyPressed(b));
}

struct EdgeState {
  bool enter=false, esc=false, space=false;
  bool l=false,j=false,o=false,z=false,r=false,d=false,a=false,g=false,p=false,i=false;
  bool minus=false,plus=false,lb=false,rb=false;
  bool left=false,right=false,up=false,down=false;
} prevKey;

bool rising(bool now, bool &prev) { bool r=now&&!prev; prev=now; return r; }

void handleCodeBufferOnKeyboardChange(const Keyboard_Class::KeysState& ks) {
  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return;
  for (char c : ks.word) {
    if (c >= '0' && c <= '9') {
      codeBuffer += c;
      if (codeBuffer.length() > 12) codeBuffer.remove(0, codeBuffer.length()-12);
      if (codeBuffer.endsWith("1488")) {
        m2rActive=!m2rActive; codeBuffer=""; statusLine=m2rActive?"M2R 1488 ON":"M2R OFF";
        witness("M2R_GATE",m2rActive?"on":"off"); playUiTone(m2rActive?1500:600,50);
      } else if (codeBuffer.endsWith("112269")) {
        houseActive=!houseActive; codeBuffer=""; statusLine=houseActive?"HOUSE 112269 ON":"HOUSE OFF";
        witness("HOUSE_GATE",houseActive?"on":"off"); playUiTone(houseActive?1300:550,50);
      }
    }
  }
}

void processInput() {
  M5Cardputer.update();
  auto ks=M5Cardputer.Keyboard.keysState();
  handleCodeBufferOnKeyboardChange(ks);

  bool nEnter=ks.enter;
  bool nEsc=ks.esc;
  bool nSpace=ks.space;
  bool nL=keyNow('l','L');
  bool nJ=keyNow('j','J');
  bool nO=keyNow('o','O');
  bool nZ=keyNow('z','Z');
  bool nR=keyNow('r','R');
  bool nD=keyNow('d','D');
  bool nA=keyNow('a','A');
  bool nG=keyNow('g','G');
  bool nP=keyNow('p','P');
  bool nI=keyNow('i','I');
  bool nMinus=keyNow('-','_');
  bool nPlus=keyNow('+','=');
  bool nLb=keyNow('[','{');
  bool nRb=keyNow(']','}');

  if (rising(nEsc,prevKey.esc)) {
    if (mode.mode == ForegroundMode::ALIEN_SURVIVAL_A) alien.leave();
    mode.escapeToHome(); statusLine="HOME";
  }
  if (rising(nEnter,prevKey.enter)) {
    hardMute=!hardMute; applyVolume(); statusLine=hardMute?"MUTE":"AUDIO ON";
    witness("AUDIO_MUTE",hardMute?"on":"off");
  }
  if (rising(nMinus,prevKey.minus)) {
    masterVolume = masterVolume>8 ? masterVolume-8 : 0; applyVolume();
  }
  if (rising(nPlus,prevKey.plus)) {
    masterVolume = masterVolume<247 ? masterVolume+8 : 255; applyVolume();
  }
  if (rising(nLb,prevKey.lb)) {
    illumination.stepDown(); applyIllumination();
  }
  if (rising(nRb,prevKey.rb)) {
    illumination.stepUp(); applyIllumination();
  }
  if (rising(nL,prevKey.l)) {
    illumination.toggleLed(); applyIllumination(); statusLine=illumination.led_enabled?"LED ON":"LED OFF";
  }
  if (rising(nJ,prevKey.j)) {
    loraActive=!loraActive; statusLine=loraActive?(loraReady?"LORA ON":"LORA REQUEST / HW FAIL"):"LORA OFF";
    witness("LORA_GATE",loraActive?"on":"off");
  }

  // HOME app launcher only. Inside A, WASD remains gameplay input.
  if (mode.mode == ForegroundMode::HOME) {
    if (rising(nO,prevKey.o)) { mode.enter(ForegroundMode::VISUALIZER_O); statusLine="OSCILLOSCOPE"; }
    if (rising(nZ,prevKey.z)) { mode.enter(ForegroundMode::ZIM_VIEW_Z); statusLine="RESOURCE VIEW"; }
    if (rising(nR,prevKey.r)) { mode.enter(ForegroundMode::RADIO_R); statusLine="RADIO RC1"; }
    if (rising(nD,prevKey.d)) { mode.enter(ForegroundMode::TAMAGOTCHI_D); statusLine="PET"; }
    if (rising(nA,prevKey.a)) { mode.enter(ForegroundMode::ALIEN_SURVIVAL_A); alien.enter(); statusLine="ALIEN SURVIVAL"; witness("MODE","alien_survival"); }
  } else {
    prevKey.o=nO; prevKey.z=nZ; prevKey.r=nR; prevKey.d=nD; prevKey.a=nA;
  }

  bool left=ks.left || keyNow('a','A');
  bool right=ks.right || keyNow('d','D');
  bool up=ks.up || keyNow('w','W');
  bool down=ks.down || keyNow('s','S');

  if (mode.mode == ForegroundMode::VISUALIZER_O) {
    if (rising(left,prevKey.left)) mode.visualizerPrev();
    if (rising(right,prevKey.right)) mode.visualizerNext();
    if (rising(up,prevKey.up)) visualGain=constrain(visualGain*1.25f,0.5f,4.0f);
    if (rising(down,prevKey.down)) visualGain=constrain(visualGain/1.25f,0.5f,4.0f);
  } else if (mode.mode == ForegroundMode::TAMAGOTCHI_D) {
    if (rising(left,prevKey.left)) petAction=(petAction+PET_ACTION_COUNT-1)%PET_ACTION_COUNT;
    if (rising(right,prevKey.right)) petAction=(petAction+1)%PET_ACTION_COUNT;
    if (rising(nSpace,prevKey.space)) petDoAction();
  } else if (mode.mode == ForegroundMode::ALIEN_SURVIVAL_A) {
    if (rising(nG,prevKey.g)) alien.toggleGyro(core.ax,core.ay);
    if (rising(nP,prevKey.p)) alien.togglePause();
    if (rising(nI,prevKey.i)) alien.toggleAutoFire();
    if (alien.dead() && rising(nSpace,prevKey.space)) alien.restartIfDead();
    alien.update(millis(),left,right,up,down,nSpace,core.ax,core.ay,sqrtf(core.gx*core.gx+core.gy*core.gy+core.gz*core.gz)*0.010f);
  } else {
    prevKey.left=left; prevKey.right=right; prevKey.up=up; prevKey.down=down;
    prevKey.space=nSpace;
  }

  if (mode.mode != ForegroundMode::ALIEN_SURVIVAL_A) {
    prevKey.g=nG; prevKey.p=nP; prevKey.i=nI;
  }
}

// -------------------------- setup / loop --------------------------
void setup() {
  auto cfg=M5.config();
  M5Cardputer.begin(cfg,true);
  Serial.begin(115200);
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(1);

  LittleFS.begin(true);
  SPI.begin(SD_SCK_PIN,SD_MISO_PIN,SD_MOSI_PIN,SD_CS_PIN);
  bool sdOk=SD.begin(SD_CS_PIN,SPI,25000000);
  Serial.printf("[ADV] SD=%d\n",sdOk?1:0);

  Wire.begin(GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);
  bool shtOk=advSht.begin(&Wire,0x44,GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);
  bool qmpOk=advQmp.begin(&Wire,0x56,GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);
  Serial.printf("[ADV] ENV SHT=%d QMP=%d\n",shtOk?1:0,qmpOk?1:0);

  M5.Imu.init();
  core.imuValid=true;

  FastLED.addLeds<WS2812,LED_PIN,GRB>(advLed,1);
  applyIllumination();
  M5Cardputer.Speaker.begin();
  applyVolume();

  loadPet();
  alien.begin();
  initEspNow();
  initGnssAndLoRa();

  core.predEntropy=core.entropy;
  core.battery=M5.Power.getBatteryLevel();
  core.freeHeap=ESP.getFreeHeap();
  witness("BOOT","ADV_Elite_RC1","OBSERVED_REAL");
  statusLine="RC1 READY";
}

void loop() {
  uint32_t loopStart=micros();
  processInput();
  gnssTick();

  uint32_t now=millis();
  if (now-lastEnvMs>=ENV_INTERVAL_MS) { lastEnvMs=now; readEnv(); }

  if (now-lastCoreMs>=CORE_INTERVAL_MS) {
    lastCoreMs=now;
    readImu();
    if (eye.online && now-eye.lastOkMs>18000UL) eye.online=false;
    if (audioNode.online && now-audioNode.lastOkMs>18000UL) audioNode.online=false;
    core.battery=M5.Power.getBatteryLevel();
    core.wifiRssi=WiFi.status()==WL_CONNECTED?WiFi.RSSI():-127;
    core.freeHeap=ESP.getFreeHeap();
    updatePredictorAndAnomaly();
    petTick();
  }

  if (now-lastHeartbeatMs>=HEARTBEAT_MS) { lastHeartbeatMs=now; sendHeartbeat(); }
  maybeSendLoRaSeal();
  brainWaveTick();

  // USER_ACTIVE_GAME_RESOURCE_COURTESY: optional work yields before P0/P1/P2.
  alienGovernor.update(alien.fpsEma(),mode.mode==ForegroundMode::ALIEN_SURVIVAL_A && !alien.paused());

  updateLed();
  if (now-lastDrawMs>=DRAW_INTERVAL_MS) { lastDrawMs=now; drawCurrentMode(); }

  core.loopUs=micros()-loopStart;
  if (core.loopUs>core.loopMaxUs) core.loopMaxUs=core.loopUs;
  delay(1);
}
