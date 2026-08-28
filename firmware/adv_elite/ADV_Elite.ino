/*
  JANUS ADV_ELITE RC2 — AUDITED FIRST-FLASH SOURCE
  ------------------------------------------------
  Single patchwork target for M5Stack Cardputer ADV.

  Canonical invariants:
  - OBSERVED_REAL != DERIVED_FROM_REAL != PREDICTED != SIMULATED != CONTROL_STATE.
  - Blind Eye is a trusted ESP-NOW sensor peer; it has no microphone.
  - EchoMic/Audio nodes are separate audio-sensor peers.
  - anomaly attention is ALWAYS ON; House/Love/game/radio cannot gate it.
  - 1488 manually starts/stops a bounded M2R forecasting session.
  - J manually gates LoRa; LOVE context is derived: HOUSE && LORA_J.
  - ENTER is absolute persisted audio mute.
  - [ ] drive one shared display+LED brightness axis.
  - L only gates LED output; it never owns an independent brightness value.
  - ESC always returns to HOME.
  - -/+ are global volume.
  - O/Z/R/D/A are foreground organs; P0/P1/P2 JANUS core keeps running.
  - game/pet/kaleidoscope are FICTION/SIMULATION/PRESENTATION and never become sensor truth.
  - real events may decorate games; game state never feeds OBSERVED_REAL/anomaly.

  Runtime organs:
  HOME Beacon HUD + ticker
  O Oscilloscope/Kaleidoscope + optional desk screensaver
  Z Zim-style resource/self view
  R Radio Browser catalogue + MP3 HTTP web-radio (ESP8266Audio when installed)
  D persistent ENV-reactive Tamagotchi
  A Alien Survival (gyro G, pause P, autofire I, fixed-step)
  CAP GNSS + SX1262 manual LoRa
  trusted ESP-NOW Blind Eye / EchoMic input
  bounded exact Buzz worker for assigned SHA256d jobs
  hash-chained Witness JSONL
  robust anomaly (classic Z + MAD + prediction error + disagreement)
  manual M2R with bounded theta ablation and later scoring

  Local configuration (never commit real secrets):
    create ADV_Elite_local_config.h beside this file with, e.g.
      #define ADV_WIFI_SSID "..."
      #define ADV_WIFI_PASSWORD "..."
*/

#include <M5Cardputer.h>
#include <FastLED.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
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
#include <ArduinoJson.h>
#include <math.h>
#include <time.h>

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

#if __has_include(<AudioOutput.h>) && __has_include(<AudioFileSourceICYStream.h>) && \
    __has_include(<AudioFileSourceBuffer.h>) && __has_include(<AudioGeneratorMP3.h>)
  #include <AudioOutput.h>
  #include <AudioFileSourceICYStream.h>
  #include <AudioFileSourceBuffer.h>
  #include <AudioGeneratorMP3.h>
  #define ADV_HAS_WEBRADIO 1
#else
  #define ADV_HAS_WEBRADIO 0
#endif

#include "ADV_Elite_mode_contract.h"
#include "ADV_Elite_illumination_policy.h"
#include "ADV_Elite_alien_survival_runtime.h"
#include "ADV_Elite_wifi_manager.h"
#include "ADV_Elite_primary_credential.h"
#include "ADV_Elite_beacon_home.h"
#include "ADV_Elite_sht3x_async.h"

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

// -------------------------- truth-bearing core --------------------------
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

  float robustZE = 0.0f;
  float robustZL = 0.0f;
  float classicZE = 0.0f;
  float classicZL = 0.0f;
  float disagreement = 0.0f;

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

struct RemoteJobState {
  bool active = false;
  uint8_t jobId[8] = {};
  uint8_t header[80] = {};
  uint8_t target[32] = {};
  uint32_t nonce = 0;
  uint32_t endNonce = 0;
  uint32_t receivedAt = 0;
} buzzJob;

ModeContract mode;
IlluminationPolicy illumination;
AlienSurvivalRuntime alien;
AlienPerformanceGovernor alienGovernor;
AdvWifiManager advWifi;
BeaconHomeRenderer beaconHome;
AdvSht3xAsync shtAsync;
Preferences prefs;

bool houseActive = false;
bool m2rActive = false;
bool loraActive = false;
bool loraReady = false;
bool hardMute = false;
bool brainWaveEnabled = true;
bool anomalyLatched = false;
bool espNowReady = false;
bool deskVisualizerEnabled = true;
bool autoVisualizerEntered = false;
bool zimExtended = false;
uint8_t masterVolume = 96;
float visualGain = 1.0f;
uint8_t petAction = 0;
uint8_t petPage = 0;
uint32_t anomalyCount = 0;
uint32_t witnessCount = 0;
uint32_t espRxCount = 0;
uint32_t espTxCount = 0;
uint32_t espTxFail = 0;
uint32_t espTxFailStreak = 0;
uint32_t heartbeatSeq = 0;
uint32_t buzzHashRate = 0;
uint32_t buzzHashCounter = 0;
uint32_t buzzShares = 0;
uint32_t buzzRejects = 0;
uint16_t buzzBestBits = 0;
uint16_t buzzWorkerId = 0;
String statusLine = "boot";
String codeBuffer;

static constexpr uint32_t ENV_INTERVAL_MS = 1000UL;
static constexpr uint32_t ENV_TTL_MS = 15000UL;
static constexpr uint32_t CORE_INTERVAL_MS = 120UL;
static constexpr uint32_t HEARTBEAT_MS = 2000UL;
static constexpr uint32_t DRAW_INTERVAL_MS = 16UL;  // ~60 Hz presentation cadence
static constexpr uint32_t BRAINWAVE_MIN_NOTE_MS = 110UL;
static constexpr uint32_t DESK_VISUALIZER_IDLE_MS = 120000UL;
static constexpr uint32_t ESP_RESCUE_COOLDOWN_MS = 12000UL;
static constexpr uint32_t BUZZ_JOB_TTL_MS = 18000UL;
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
uint32_t lastShtFreshMs = 0;
uint32_t lastQmpFreshMs = 0;
uint32_t lastCoreMs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastDrawMs = 0;
uint32_t lastBrainNoteMs = 0;
uint32_t lastGpsSealMs = 0;
uint32_t lastBuzzRateMs = 0;
uint32_t lastEspRescueMs = 0;
uint32_t lastUserInputMs = 0;
uint8_t brainStep = 0;
float gyroBiasX = 0, gyroBiasY = 0, gyroBiasZ = 0;

char witnessPrevHash[65] = "GENESIS";
static const char* WITNESS_LFS = "/adv_witness.jsonl";
static const char* WITNESS_SD = "/janus/adv_witness.jsonl";

// -------------------------- manual M2R --------------------------
struct M2RSession {
  bool active = false;
  bool complete = false;
  uint32_t sessionId = 0;
  uint32_t startedMs = 0;
  float baseEntropy = 0.0f;
  float slopePerSec = 0.0f;
  float uncertainty = 0.0f;
  float thetaFeature = 0.0f;
  float thetaWeight = 0.10f;
  static constexpr uint8_t N = 4;
  uint32_t horizonMs[N] = {5000, 15000, 30000, 60000};
  float predThetaOn[N] = {};
  float predThetaOff[N] = {};
  float errThetaOn[N] = {};
  float errThetaOff[N] = {};
  bool scored[N] = {};
} m2r;

// -------------------------- Tamagotchi (SIMULATED) --------------------------
struct PetState {
  float hunger = 18;
  float thirst = 15;
  float dirt = 8;
  float energy = 82;
  float mood = 75;
  float health = 100;
  float comfort = 75;
  uint32_t ageMinutes = 0;
  uint32_t interactions = 0;
  uint32_t meals = 0;
  uint32_t drinks = 0;
  uint32_t cleanups = 0;
  uint32_t lastTickMs = 0;
  bool sleeping = false;
} pet;
const char* petActions[] = {"FEED","DRINK","CLEAN","PLAY","SLEEP","MEDIC","CHECK"};
static constexpr uint8_t PET_ACTION_COUNT = sizeof(petActions)/sizeof(petActions[0]);

// -------------------------- radio --------------------------
static constexpr uint8_t RADIO_MAX = 24;
struct RadioStation {
  char name[42] = {};
  char url[180] = {};
  char uuid[40] = {};
  uint16_t bitrate = 0;
  int16_t localScore = 0;
  uint32_t votes = 0;
};
struct RadioState {
  RadioStation stations[RADIO_MAX];
  uint8_t count = 0;
  uint8_t index = 0;
  bool desiredPlaying = false;
  bool engineRunning = false;
  bool catalogBusy = false;
  uint32_t failures = 0;
  uint32_t lastCatalogMs = 0;
  char nowTitle[72] = "";
} radioState;

#if ADV_HAS_WEBRADIO
class AdvAudioOutputM5Speaker : public AudioOutput {
 public:
  explicit AdvAudioOutputM5Speaker(m5::Speaker_Class* s, uint8_t ch=1) : speaker_(s), channel_(ch) {}
  bool begin() override { return true; }
  bool ConsumeSample(int16_t sample[2]) override {
    if (index_ + 1 < BUF) {
      data_[bank_][index_++] = sample[0];
      data_[bank_][index_++] = sample[1];
      return true;
    }
    flush();
    return false;
  }
  void flush() override {
    if (!index_) return;
    speaker_->playRaw(data_[bank_], index_, hertz, true, 1, channel_);
    bank_ = bank_ < 2 ? bank_ + 1 : 0;
    index_ = 0;
  }
  bool stop() override {
    flush();
    speaker_->stop(channel_);
    return true;
  }
 private:
  m5::Speaker_Class* speaker_;
  uint8_t channel_;
  static constexpr size_t BUF = 1536;
  int16_t data_[3][BUF] = {};
  size_t index_ = 0;
  size_t bank_ = 0;
};
AudioFileSourceICYStream* radioFile = nullptr;
AudioFileSourceBuffer* radioBuffer = nullptr;
AudioGeneratorMP3* radioMp3 = nullptr;
AdvAudioOutputM5Speaker* radioOut = nullptr;
#endif

// -------------------------- helpers --------------------------
float clamp01(float x) { return constrain(x,0.0f,1.0f); }
bool loveContextActive() { return houseActive && loraActive; }

uint16_t uiPrimary() { return houseActive ? M5Cardputer.Display.color565(255,145,0) : TFT_CYAN; }
uint16_t uiSecondary() { return houseActive ? M5Cardputer.Display.color565(155,88,0) : TFT_DARKGREY; }

void ensureSdWitnessDir() { if (!SD.exists("/janus")) SD.mkdir("/janus"); }

void sha256Hex(const String& text,char out[65]) {
  uint8_t hash[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx,0);
  mbedtls_sha256_update(&ctx,(const unsigned char*)text.c_str(),text.length());
  mbedtls_sha256_finish(&ctx,hash);
  mbedtls_sha256_free(&ctx);
  static const char hx[]="0123456789abcdef";
  for(int i=0;i<32;i++){out[i*2]=hx[(hash[i]>>4)&15];out[i*2+1]=hx[hash[i]&15];}
  out[64]=0;
}

void witness(const char* type,const char* detail,const char* truthClass="CONTROL_STATE") {
  String payload="{";
  payload += "\"schema\":\"JANUS_EVENT_V1\",";
  payload += "\"source\":\"ADV_Elite\",";
  payload += "\"ts_ms\":"+String(millis())+",";
  payload += "\"type\":\""+String(type)+"\",";
  payload += "\"truth_class\":\""+String(truthClass)+"\",";
  payload += "\"detail\":\""+String(detail)+"\",";
  payload += "\"entropy\":"+String(core.entropy,5)+",";
  payload += "\"pred\":"+String(core.predEntropy,5)+",";
  payload += "\"loss\":"+String(core.loss,5)+",";
  payload += "\"eye_online\":"+String(eye.online?"true":"false")+",";
  payload += "\"prev_hash\":\""+String(witnessPrevHash)+"\"}";
  char newHash[65]; sha256Hex(String(witnessPrevHash)+payload,newHash);
  String line=payload.substring(0,payload.length()-1)+",\"hash\":\""+String(newHash)+"\"}";
  File f=LittleFS.open(WITNESS_LFS,FILE_APPEND); if(f){f.println(line);f.close();}
  if(SD.cardType()!=CARD_NONE){ensureSdWitnessDir();File sf=SD.open(WITNESS_SD,FILE_APPEND);if(sf){sf.println(line);sf.close();}}
  strlcpy(witnessPrevHash,newHash,sizeof(witnessPrevHash));
  ++witnessCount;
}

void saveUiSettings() {
  prefs.begin("adv_ui",false);
  prefs.putUChar("vol",masterVolume);
  prefs.putBool("mute",hardMute);
  prefs.putUChar("bright",illumination.level_index);
  prefs.putBool("led",illumination.led_enabled);
  prefs.putBool("desk",deskVisualizerEnabled);
  prefs.putFloat("theta",m2r.thetaWeight);
  prefs.end();
}
void loadUiSettings() {
  prefs.begin("adv_ui",true);
  masterVolume=prefs.getUChar("vol",96);
  hardMute=prefs.getBool("mute",false);
  illumination.level_index=min<uint8_t>(prefs.getUChar("bright",6),IlluminationPolicy::kLevelCount-1);
  illumination.led_enabled=prefs.getBool("led",true);
  deskVisualizerEnabled=prefs.getBool("desk",true);
  m2r.thetaWeight=constrain(prefs.getFloat("theta",0.10f),0.0f,0.35f);
  prefs.end();
}

void applyVolume(){M5Cardputer.Speaker.setVolume(hardMute?0:masterVolume);if(hardMute)M5Cardputer.Speaker.stop();}
void applyIllumination(){M5Cardputer.Display.setBrightness(illumination.displayBrightness());FastLED.setBrightness(illumination.ledBrightness());}

void playUiTone(uint16_t freq,uint16_t ms){
  if(hardMute||masterVolume==0)return;
  if(mode.mode==ForegroundMode::RADIO_R)return;
  M5Cardputer.Speaker.tone(freq,ms);
}

void updateLed(){
  if(!illumination.led_enabled||illumination.ledBrightness()==0){advLed[0]=CRGB::Black;FastLED.show();return;}
  if(anomalyLatched) advLed[0]=CRGB::White;
  else if(mode.mode==ForegroundMode::ALIEN_SURVIVAL_A){AlienRgb c=AlienLedPolicy::healthColor(alien.health01());advLed[0]=CRGB(c.r,c.g,c.b);}
  else if(houseActive) advLed[0]=CRGB(255,140,0);
  else {uint8_t hue=(uint8_t)constrain(160-(int)(core.entropy*14.0f),0,160);advLed[0]=CHSV(hue,255,255);}
  FastLED.show();
}

float meanArr(const float* a,int n){if(n<=0)return 0;float s=0;for(int i=0;i<n;i++)s+=a[i];return s/n;}
float stdArr(const float* a,int n,float m){if(n<=1)return 0;float s=0;for(int i=0;i<n;i++){float d=a[i]-m;s+=d*d;}return sqrtf(s/(n-1));}

float medianCopy(const float* a,int n){
  if(n<=0)return 0;
  float tmp[HIST_N]; n=min(n,HIST_N);
  for(int i=0;i<n;i++)tmp[i]=a[i];
  for(int i=1;i<n;i++){float v=tmp[i];int j=i-1;while(j>=0&&tmp[j]>v){tmp[j+1]=tmp[j];--j;}tmp[j+1]=v;}
  return (n&1)?tmp[n/2]:0.5f*(tmp[n/2-1]+tmp[n/2]);
}
float madArr(const float* a,int n,float med){
  if(n<=0)return 0;float dev[HIST_N];n=min(n,HIST_N);for(int i=0;i<n;i++)dev[i]=fabsf(a[i]-med);return medianCopy(dev,n);
}

void appendHistory(){
  histEntropy[histPos]=core.entropy;histLoss[histPos]=core.loss;
  histTemp[histPos]=core.shtValid?core.tempC:NAN;histHum[histPos]=core.shtValid?core.humidity:NAN;
  histMotion[histPos]=eye.online&&isfinite(eye.motion)?eye.motion:core.imuLoss;histSelf[histPos]=(float)core.loopUs;
  histPos=(histPos+1)%HIST_N;if(histCount<HIST_N)++histCount;
}

// -------------------------- sensors --------------------------
void serviceEnv(){
  const uint32_t now=millis();

  // Start a new SHT3X conversion once per ENV interval, but never sleep here.
  if(!shtAsync.pending() && now-lastEnvMs>=ENV_INTERVAL_MS){
    lastEnvMs=now;
    (void)shtAsync.start(now);

    // QMP6988 normal-mode read is short and contains no recurring library delay.
    if(advQmp.update()){
      const float p=advQmp.pressure/100.0f;
      if(isfinite(p)&&p>300.0f&&p<1200.0f){
        core.pressureHpa=p;
        core.qmpValid=true;
        lastQmpFreshMs=now;
      }
    }
  }

  float t=NAN,h=NAN;
  if(shtAsync.poll(now,t,h)){
    if(isfinite(t)&&isfinite(h)&&t>-40.0f&&t<90.0f&&h>=0.0f&&h<=100.0f){
      core.tempC=t;
      core.humidity=h;
      core.shtValid=true;
      lastShtFreshMs=now;
    }
  }

  if(lastShtFreshMs==0 || now-lastShtFreshMs>ENV_TTL_MS) core.shtValid=false;
  if(lastQmpFreshMs==0 || now-lastQmpFreshMs>ENV_TTL_MS) core.qmpValid=false;
  core.envFreshMs=max(lastShtFreshMs,lastQmpFreshMs);
}

void calibrateImuZero(){
  float sx=0,sy=0,sz=0;int good=0;
  for(int i=0;i<36;i++){float gx=0,gy=0,gz=0;M5.Imu.getGyroData(&gx,&gy,&gz);if(isfinite(gx)&&isfinite(gy)&&isfinite(gz)){sx+=gx;sy+=gy;sz+=gz;++good;}delay(8);}
  if(good){gyroBiasX=sx/good;gyroBiasY=sy/good;gyroBiasZ=sz/good;}
}
void readImu(){
  float ax=0,ay=0,az=1,gx=0,gy=0,gz=0;
  M5.Imu.getAccelData(&ax,&ay,&az);M5.Imu.getGyroData(&gx,&gy,&gz);
  if(!isfinite(ax)||!isfinite(ay)||!isfinite(az))return;
  gx-=gyroBiasX;gy-=gyroBiasY;gz-=gyroBiasZ;
  core.ax=ax;core.ay=ay;core.az=az;core.gx=gx;core.gy=gy;core.gz=gz;
  float amag=sqrtf(ax*ax+ay*ay+az*az);float gmag=sqrtf(gx*gx+gy*gy+gz*gz)*0.010f;
  core.imuShock=amag+gmag;core.imuLoss=fabsf(core.imuShock-core.imuPred);core.imuPred=core.imuPred*0.965f+core.imuShock*0.035f;core.imuValid=true;
}

void updatePredictorAndAnomaly(){
  float prevEntropy=core.entropy;
  float envTerm=0;
  if(core.shtValid){envTerm+=fabsf(core.tempC-23.0f)*0.035f;envTerm+=fabsf(core.humidity-50.0f)*0.006f;}
  if(core.qmpValid)envTerm+=fabsf(core.pressureHpa-1013.25f)*0.002f;
  float eyeTerm=0;
  if(eye.online){if(isfinite(eye.loss))eyeTerm+=constrain(eye.loss,0.0f,5.0f)*0.50f;if(isfinite(eye.motion))eyeTerm+=constrain(fabsf(eye.motion),0.0f,1000.0f)*0.001f;}
  float imuTerm=core.imuValid?core.imuLoss*0.80f:0;
  core.entropy=constrain(0.05f+envTerm+eyeTerm+imuTerm,0.01f,10.0f);
  core.loss=fabsf(core.predEntropy-core.entropy);
  core.trend=core.trend*0.86f+(core.entropy-prevEntropy)*0.14f;
  core.predEntropy=core.predEntropy*0.88f+core.entropy*0.12f;
  core.future1=constrain(core.predEntropy+core.trend,0.01f,10.0f);
  core.future2=constrain(core.predEntropy+core.trend*2.0f,0.01f,10.0f);
  core.future3=constrain(core.predEntropy+core.trend*3.0f,0.01f,10.0f);
  appendHistory();

  bool anomaly=false;
  core.classicZE=core.classicZL=core.robustZE=core.robustZL=0;
  core.disagreement=0;
  if(histCount>=24){
    float mE=meanArr(histEntropy,histCount),sE=stdArr(histEntropy,histCount,mE);
    float mL=meanArr(histLoss,histCount),sL=stdArr(histLoss,histCount,mL);
    core.classicZE=sE>1e-5f?fabsf((core.entropy-mE)/sE):0;
    core.classicZL=sL>1e-5f?fabsf((core.loss-mL)/sL):0;
    float medE=medianCopy(histEntropy,histCount),madE=madArr(histEntropy,histCount,medE);
    float medL=medianCopy(histLoss,histCount),madL=madArr(histLoss,histCount,medL);
    core.robustZE=madE>1e-5f?0.6745f*fabsf(core.entropy-medE)/madE:0;
    core.robustZL=madL>1e-5f?0.6745f*fabsf(core.loss-medL)/madL:0;
    if(eye.online&&isfinite(eye.activity)&&isfinite(eye.predActivity))core.disagreement=fabsf(eye.activity-eye.predActivity);
    anomaly=core.classicZE>4.2f||core.classicZL>4.0f||core.robustZE>5.0f||core.robustZL>5.0f||core.disagreement>2.25f;
  }
  if(anomaly&&!anomalyLatched){anomalyLatched=true;++anomalyCount;statusLine="ANOMALY / WITNESS";witness("ANOMALY","z+mad+prediction+peer_disagreement","DERIVED_FROM_REAL");playUiTone(820,80);}
  else if(!anomaly)anomalyLatched=false;
}

// -------------------------- M2R manual future session --------------------------
float thetaFeature(float entropy){
  float q=constrain(0.03f+entropy*0.025f,0.03f,0.30f);
  float t3=1.0f,t4=1.0f;
  for(int n=1;n<=6;n++){float term=powf(q,(float)(n*n));t3+=2.0f*term;t4+=2.0f*((n&1)?-term:term);}
  return constrain((t3-t4)*0.5f,0.0f,1.5f);
}
void startM2R(){
  m2r.active=true;m2r.complete=false;m2r.sessionId++;m2r.startedMs=millis();m2r.baseEntropy=core.entropy;
  m2r.slopePerSec=constrain(core.trend/(CORE_INTERVAL_MS*0.001f),-0.18f,0.18f);
  m2r.uncertainty=constrain(0.10f+core.loss*1.4f+core.imuLoss*0.35f+core.disagreement*0.08f,0.10f,3.0f);
  m2r.thetaFeature=thetaFeature(core.entropy);
  for(uint8_t i=0;i<M2RSession::N;i++){
    float sec=m2r.horizonMs[i]*0.001f;
    float off=constrain(m2r.baseEntropy+m2r.slopePerSec*sec,0.01f,10.0f);
    float adj=m2r.thetaWeight*m2r.thetaFeature*constrain(m2r.slopePerSec*sec,-0.8f,0.8f);
    m2r.predThetaOff[i]=off;m2r.predThetaOn[i]=constrain(off+adj,0.01f,10.0f);m2r.scored[i]=false;m2r.errThetaOn[i]=m2r.errThetaOff[i]=0;
  }
  m2rActive=true;statusLine="M2R 1488 RUN";witness("M2R_START","manual_forward_ensemble_theta_ablation","CONTROL_STATE");
}
void stopM2R(const char* why){m2r.active=false;m2rActive=false;statusLine="M2R STOP";witness("M2R_STOP",why,"CONTROL_STATE");}
void m2rTick(){
  if(!m2r.active)return;
  uint32_t elapsed=millis()-m2r.startedMs;bool all=true;
  for(uint8_t i=0;i<M2RSession::N;i++){
    if(!m2r.scored[i]&&elapsed>=m2r.horizonMs[i]){
      m2r.scored[i]=true;m2r.errThetaOn[i]=fabsf(core.entropy-m2r.predThetaOn[i]);m2r.errThetaOff[i]=fabsf(core.entropy-m2r.predThetaOff[i]);
      String d="h="+String(m2r.horizonMs[i])+" on="+String(m2r.errThetaOn[i],4)+" off="+String(m2r.errThetaOff[i],4);
      witness("M2R_SCORE",d.c_str(),"DERIVED_FROM_REAL");
    }
    if(!m2r.scored[i])all=false;
  }
  if(all){
    float on=0,off=0;for(uint8_t i=0;i<M2RSession::N;i++){on+=m2r.errThetaOn[i];off+=m2r.errThetaOff[i];}on/=M2RSession::N;off/=M2RSession::N;
    if(on+0.01f<off)m2r.thetaWeight=constrain(m2r.thetaWeight+0.01f,0.0f,0.35f);else m2r.thetaWeight*=0.92f;
    saveUiSettings();m2r.complete=true;m2r.active=false;m2rActive=false;
    String d="mean_on="+String(on,4)+" mean_off="+String(off,4)+" theta_w="+String(m2r.thetaWeight,3);
    witness("M2R_COMPLETE",d.c_str(),"DERIVED_FROM_REAL");statusLine="M2R SCORED";
  }
}

// -------------------------- exact Buzz worker --------------------------
void writeLE32(uint8_t* p,uint32_t v){p[0]=v&255;p[1]=(v>>8)&255;p[2]=(v>>16)&255;p[3]=(v>>24)&255;}
void doubleSha256(const uint8_t* data,size_t len,uint8_t out[32]){
  uint8_t first[32];mbedtls_sha256_context ctx;mbedtls_sha256_init(&ctx);mbedtls_sha256_starts(&ctx,0);mbedtls_sha256_update(&ctx,data,len);mbedtls_sha256_finish(&ctx,first);mbedtls_sha256_starts(&ctx,0);mbedtls_sha256_update(&ctx,first,32);mbedtls_sha256_finish(&ctx,out);mbedtls_sha256_free(&ctx);
}
bool hashMeetsTargetBE(const uint8_t hash[32],const uint8_t target[32]){for(int i=0;i<32;i++){if(hash[i]<target[i])return true;if(hash[i]>target[i])return false;}return true;}
uint16_t leadingZeroBitsBE(const uint8_t h[32]){uint16_t bits=0;for(int i=0;i<32;i++){uint8_t b=h[i];if(!b){bits+=8;continue;}for(int k=7;k>=0;k--){if((b&(1<<k))==0)bits++;else return bits;}}return bits;}
void sendShare(const RemoteJobState& j,uint32_t nonce){ShareResponse s{};s.magic[0]='S';s.magic[1]='R';memcpy(s.job_id,j.jobId,8);s.nonce=nonce;s.worker_id=buzzWorkerId;if(esp_now_send((uint8_t*)"\xFF\xFF\xFF\xFF\xFF\xFF",(uint8_t*)&s,sizeof(s))==ESP_OK)buzzShares++;else buzzRejects++;}
void runBuzzBatch(){
  if(!buzzJob.active||millis()-buzzJob.receivedAt>BUZZ_JOB_TTL_MS){buzzJob.active=false;return;}
  uint16_t batch=alienGovernor.throttle_p3?40:180;uint8_t hash[32];
  for(uint16_t i=0;i<batch&&buzzJob.active;i++){
    if(buzzJob.nonce>=buzzJob.endNonce){buzzJob.active=false;break;}
    uint32_t nonce=buzzJob.nonce++;writeLE32(buzzJob.header+76,nonce);doubleSha256(buzzJob.header,80,hash);++buzzHashCounter;
    uint16_t bits=leadingZeroBitsBE(hash);if(bits>buzzBestBits)buzzBestBits=bits;
    if(hashMeetsTargetBE(hash,buzzJob.target)){sendShare(buzzJob,nonce);buzzJob.active=false;break;}
  }
  if(millis()-lastBuzzRateMs>=1000UL){buzzHashRate=buzzHashCounter;buzzHashCounter=0;lastBuzzRateMs=millis();}
}

// -------------------------- ESP-NOW --------------------------
uint8_t broadcastMac[6]={0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
int8_t lastEspRssi=-127;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onEspRecv(const esp_now_recv_info_t* info,const uint8_t* data,int len){if(info&&info->rx_ctrl)lastEspRssi=info->rx_ctrl->rssi;
#else
void onEspRecv(const uint8_t* mac,const uint8_t* data,int len){
#endif
  if(!data||len<=0)return;++espRxCount;
  if(len==(int)sizeof(EntropyReportV2)&&data[0]=='E'&&data[1]=='2'){
    EntropyReportV2 er{};memcpy(&er,data,sizeof(er));er.nodeId[sizeof(er.nodeId)-1]=0;String node(er.nodeId);
    if(node.indexOf("BlindEye")>=0||node.indexOf("Blind")>=0){eye.online=true;eye.lastOkMs=millis();eye.presence=er.values[1];eye.motion=er.values[2];eye.magNorm=er.values[3];eye.shock=er.values[4];eye.activity=er.values[5];eye.predActivity=er.values[6];eye.loss=er.prediction_error;eye.sync=er.sync_hint;eye.rssi=lastEspRssi;strlcpy(eye.nodeId,er.nodeId,sizeof(eye.nodeId));}
    if(node.indexOf("EchoMic")>=0||node.indexOf("Audio")>=0){audioNode.online=true;audioNode.lastOkMs=millis();audioNode.micRms=er.values[0];audioNode.entropy=er.local_entropy;audioNode.loss=er.prediction_error;strlcpy(audioNode.nodeId,er.nodeId,sizeof(audioNode.nodeId));}
    return;
  }
  if(len==(int)sizeof(JobPacket)&&data[0]=='J'&&data[1]=='B'){
    JobPacket j{};memcpy(&j,data,sizeof(j));memcpy(buzzJob.jobId,j.job_id,8);memcpy(buzzJob.header,j.header,80);memcpy(buzzJob.target,j.target,32);buzzJob.nonce=j.start_nonce;buzzJob.endNonce=j.start_nonce+j.range_size;buzzJob.receivedAt=millis();buzzJob.active=true;return;
  }
}

bool initEspNow(){
  WiFi.mode(WIFI_STA);
  if(strlen(ADV_WIFI_SSID)>0&&WiFi.status()!=WL_CONNECTED){WiFi.begin(ADV_WIFI_SSID,ADV_WIFI_PASSWORD);uint32_t until=millis()+3500UL;while(WiFi.status()!=WL_CONNECTED&&millis()<until)delay(25);}
  if(WiFi.status()!=WL_CONNECTED)esp_wifi_set_channel(ADV_ESPNOW_CHANNEL,WIFI_SECOND_CHAN_NONE);
  if(esp_now_init()!=ESP_OK){statusLine="ESP-NOW FAIL";espNowReady=false;return false;}
  esp_now_register_recv_cb(onEspRecv);
  esp_now_peer_info_t peer{};memcpy(peer.peer_addr,broadcastMac,6);peer.channel=0;peer.encrypt=false;if(!esp_now_is_peer_exist(broadcastMac))esp_now_add_peer(&peer);
  espNowReady=true;espTxFailStreak=0;return true;
}
void rescueEspNow(const char* why){if(millis()-lastEspRescueMs<ESP_RESCUE_COOLDOWN_MS)return;lastEspRescueMs=millis();esp_now_deinit();delay(5);initEspNow();witness("ESPNOW_RESCUE",why,"CONTROL_STATE");}
void sendHeartbeat(){
  if(!espNowReady)return;JanusColonyPacket p{};memcpy(p.magic,"JANUS",6);strlcpy(p.nodeId,"ADV_Elite",sizeof(p.nodeId));strlcpy(p.role,"ADV_ELITE",sizeof(p.role));p.seq=++heartbeatSeq;p.hashRate=buzzHashRate;p.shares=buzzShares;p.rejects=buzzRejects;p.bestBits=buzzBestBits;p.aiBatch=alienGovernor.throttle_p3?40:180;p.aiHint=anomalyLatched?2:1;p.jobAgeMs=buzzJob.active?millis()-buzzJob.receivedAt:0;p.rssi=WiFi.status()==WL_CONNECTED?WiFi.RSSI():lastEspRssi;p.uptime=millis()/1000UL;
  esp_err_t e=esp_now_send(broadcastMac,(uint8_t*)&p,sizeof(p));if(e==ESP_OK){++espTxCount;espTxFailStreak=0;}else{++espTxFail;++espTxFailStreak;if(espTxFailStreak>=18)rescueEspNow("tx_fail_streak");}
}

// -------------------------- CAP GNSS / manual LoRa --------------------------
void initGnssAndLoRa(){
  Serial2.begin(GNSS_BAUD,SERIAL_8N1,GNSS_RX_PIN,GNSS_TX_PIN);
#if ADV_HAS_RADIOLIB
  pinMode(LORA_PWR_EN,OUTPUT);digitalWrite(LORA_PWR_EN,HIGH);delay(80);int st=advRadio.begin(868.0,125.0,9,7,0x34,10,8,1.6,false);loraReady=(st==RADIOLIB_ERR_NONE);if(loraReady){advRadio.setOutputPower(10);advRadio.setCRC(true);}
#else
  loraReady=false;
#endif
}
void gnssTick(){while(Serial2.available())advGps.encode((char)Serial2.read());}
void maybeSendLoRaSeal(){
#if ADV_HAS_RADIOLIB
  if(!loraActive||!loraReady||millis()-lastGpsSealMs<30000UL)return;lastGpsSealMs=millis();String s="JSA3|ADV|"+String(millis()/1000UL)+"|E="+String(core.entropy,2)+"|A="+String(anomalyLatched?1:0);if(advGps.location.isValid())s+="|FIX=1|SAT="+String(advGps.satellites.value());else s+="|FIX=0";advRadio.transmit(s.c_str());
#endif
}

// -------------------------- exclusive foreground audio lease --------------------------
bool radioAudioLease=false;
bool brainWaveBeforeRadio=true;
void acquireRadioAudioLease(){if(radioAudioLease)return;brainWaveBeforeRadio=brainWaveEnabled;brainWaveEnabled=false;radioStopEngine();M5Cardputer.Speaker.stop();radioAudioLease=true;lastBrainNoteMs=millis();}
void releaseRadioAudioLease(){if(!radioAudioLease)return;radioSetPlaying(false);M5Cardputer.Speaker.stop();radioAudioLease=false;brainWaveEnabled=brainWaveBeforeRadio;lastBrainNoteMs=millis();applyVolume();}

// -------------------------- BrainWave synthesis core --------------------------
const uint16_t bwA[8]={220,277,330,440,392,330,277,220};
const uint16_t bwB[8]={196,247,294,392,349,294,247,196};
void brainWaveTick(){
  if(!brainWaveEnabled||hardMute||masterVolume==0||mode.mode==ForegroundMode::RADIO_R||mode.mode==ForegroundMode::ALIEN_SURVIVAL_A)return;
  uint32_t now=millis();uint32_t interval=(uint32_t)constrain(420.0f-core.entropy*28.0f,110.0f,420.0f);if(now-lastBrainNoteMs<interval)return;lastBrainNoteMs=now;
  const uint16_t* phrase=houseActive?bwB:bwA;float swarmShift=eye.online&&isfinite(eye.sync)?1.0f+constrain(eye.sync,0.0f,1.0f)*0.08f:1.0f;int note=(int)(phrase[brainStep]*(1.0f+core.entropy*0.012f)*swarmShift);M5Cardputer.Speaker.tone(note,(uint32_t)(interval*0.65f));brainStep=(brainStep+1)&7;
}

// -------------------------- pet persistence + ENV comfort --------------------------
void loadPet(){
  prefs.begin("adv_pet",true);pet.hunger=prefs.getFloat("hung",18);pet.thirst=prefs.getFloat("thir",15);pet.dirt=prefs.getFloat("dirt",8);pet.energy=prefs.getFloat("ener",82);pet.mood=prefs.getFloat("mood",75);pet.health=prefs.getFloat("heal",100);pet.ageMinutes=prefs.getUInt("age",0);pet.interactions=prefs.getUInt("ints",0);pet.meals=prefs.getUInt("meal",0);pet.drinks=prefs.getUInt("drink",0);pet.cleanups=prefs.getUInt("clean",0);pet.sleeping=prefs.getBool("sleep",false);prefs.end();pet.lastTickMs=millis();
}
void savePet(){prefs.begin("adv_pet",false);prefs.putFloat("hung",pet.hunger);prefs.putFloat("thir",pet.thirst);prefs.putFloat("dirt",pet.dirt);prefs.putFloat("ener",pet.energy);prefs.putFloat("mood",pet.mood);prefs.putFloat("heal",pet.health);prefs.putUInt("age",pet.ageMinutes);prefs.putUInt("ints",pet.interactions);prefs.putUInt("meal",pet.meals);prefs.putUInt("drink",pet.drinks);prefs.putUInt("clean",pet.cleanups);prefs.putBool("sleep",pet.sleeping);prefs.end();}
void petTick(){
  uint32_t now=millis();if(!pet.lastTickMs)pet.lastTickMs=now;uint32_t elapsed=now-pet.lastTickMs;if(elapsed<60000UL)return;uint32_t mins=elapsed/60000UL;pet.lastTickMs+=mins*60000UL;pet.ageMinutes+=mins;float m=(float)mins;
  pet.hunger=constrain(pet.hunger+0.22f*m,0.0f,100.0f);pet.thirst=constrain(pet.thirst+0.30f*m,0.0f,100.0f);pet.dirt=constrain(pet.dirt+0.13f*m,0.0f,100.0f);
  pet.energy=constrain(pet.energy+(pet.sleeping?0.55f:-0.16f)*m,0.0f,100.0f);if(pet.sleeping&&pet.energy>92)pet.sleeping=false;
  float comfort=0.75f;if(core.shtValid){comfort=1.0f-constrain(fabsf(core.tempC-23.0f)/30.0f,0.0f,0.6f)-constrain(fabsf(core.humidity-50.0f)/100.0f,0.0f,0.3f);} // corrected below by explicit clamp
  pet.comfort=constrain(comfort*100.0f,0.0f,100.0f);
  float neglect=(pet.hunger+pet.thirst+pet.dirt+(100.0f-pet.energy))*0.0025f;pet.mood=constrain(pet.mood+(comfort-neglect-0.45f)*m*0.12f,0.0f,100.0f);
  if(pet.hunger>90||pet.thirst>90||pet.dirt>95||pet.energy<5)pet.health=constrain(pet.health-0.35f*m,1.0f,100.0f);else pet.health=constrain(pet.health+0.05f*m,1.0f,100.0f);
  if((pet.ageMinutes%15)<mins)savePet();
}
void petDoAction(){
  ++pet.interactions;
  switch(petAction){case 0:pet.hunger=constrain(pet.hunger-26.0f,0.0f,100.0f);++pet.meals;break;case 1:pet.thirst=constrain(pet.thirst-32.0f,0.0f,100.0f);++pet.drinks;break;case 2:pet.dirt=constrain(pet.dirt-38.0f,0.0f,100.0f);++pet.cleanups;break;case 3:pet.mood=constrain(pet.mood+18.0f,0.0f,100.0f);pet.energy=constrain(pet.energy-8.0f,0.0f,100.0f);break;case 4:pet.sleeping=!pet.sleeping;break;case 5:if(pet.health<80)pet.health=constrain(pet.health+12.0f,0.0f,100.0f);break;default:break;}
  savePet();playUiTone(900+petAction*60,35);
}

// -------------------------- Radio Browser + web radio --------------------------
uint32_t fnv1a(const char* s){uint32_t h=2166136261UL;while(*s){h^=(uint8_t)*s++;h*=16777619UL;}return h;}
String stationScoreKey(const char* uuid){char k[12];snprintf(k,sizeof(k),"s%08lx",(unsigned long)fnv1a(uuid));return String(k);}
void radioLoadScores(){prefs.begin("adv_radio",true);for(uint8_t i=0;i<radioState.count;i++)radioState.stations[i].localScore=(int16_t)prefs.getShort(stationScoreKey(radioState.stations[i].uuid).c_str(),0);prefs.end();}
void radioSaveScore(uint8_t i){if(i>=radioState.count)return;prefs.begin("adv_radio",false);prefs.putShort(stationScoreKey(radioState.stations[i].uuid).c_str(),radioState.stations[i].localScore);prefs.end();}
int32_t stationRank(const RadioStation& s){return (int32_t)s.localScore*100000+(int32_t)min<uint32_t>(s.votes,99999);}
void radioSort(){for(uint8_t i=1;i<radioState.count;i++){RadioStation v=radioState.stations[i];int j=i-1;while(j>=0&&stationRank(radioState.stations[j])<stationRank(v)){radioState.stations[j+1]=radioState.stations[j];--j;}radioState.stations[j+1]=v;}if(radioState.index>=radioState.count)radioState.index=0;}
void radioSaveCache(){if(SD.cardType()==CARD_NONE)return;ensureSdWitnessDir();File f=SD.open("/janus/radio_catalog.json",FILE_WRITE);if(!f)return;DynamicJsonDocument doc(24576);JsonArray a=doc.to<JsonArray>();for(uint8_t i=0;i<radioState.count;i++){JsonObject o=a.createNestedObject();o["name"]=radioState.stations[i].name;o["url"]=radioState.stations[i].url;o["uuid"]=radioState.stations[i].uuid;o["bitrate"]=radioState.stations[i].bitrate;o["votes"]=radioState.stations[i].votes;}serializeJson(doc,f);f.close();}
bool radioLoadCache(){if(SD.cardType()==CARD_NONE||!SD.exists("/janus/radio_catalog.json"))return false;File f=SD.open("/janus/radio_catalog.json",FILE_READ);if(!f)return false;DynamicJsonDocument doc(24576);DeserializationError e=deserializeJson(doc,f);f.close();if(e)return false;radioState.count=0;for(JsonObject o:doc.as<JsonArray>()){if(radioState.count>=RADIO_MAX)break;RadioStation& s=radioState.stations[radioState.count++];strlcpy(s.name,o["name"]|"station",sizeof(s.name));strlcpy(s.url,o["url"]|"",sizeof(s.url));strlcpy(s.uuid,o["uuid"]|"none",sizeof(s.uuid));s.bitrate=o["bitrate"]|0;s.votes=o["votes"]|0;}radioLoadScores();radioSort();return radioState.count>0;}
bool radioRefreshCatalog(){
  if(WiFi.status()!=WL_CONNECTED)return radioLoadCache();radioState.catalogBusy=true;
  WiFiClientSecure client;client.setInsecure();HTTPClient http;String url="https://de1.api.radio-browser.info/json/stations/search?codec=MP3&is_https=false&hidebroken=true&order=votes&reverse=true&limit=24";
  if(!http.begin(client,url)){radioState.catalogBusy=false;return radioLoadCache();}http.addHeader("User-Agent","JANUS-ADV-Elite/2.0");int code=http.GET();if(code!=200){http.end();radioState.catalogBusy=false;return radioLoadCache();}
  DynamicJsonDocument doc(32768);DeserializationError de=deserializeJson(doc,http.getStream());http.end();if(de){radioState.catalogBusy=false;return radioLoadCache();}
  radioState.count=0;for(JsonObject o:doc.as<JsonArray>()){if(radioState.count>=RADIO_MAX)break;const char* u=o["url_resolved"]|"";const char* c=o["codec"]|"";if(strncmp(u,"http://",7)!=0||strcasecmp(c,"MP3")!=0)continue;RadioStation& s=radioState.stations[radioState.count++];strlcpy(s.name,o["name"]|"station",sizeof(s.name));strlcpy(s.url,u,sizeof(s.url));strlcpy(s.uuid,o["stationuuid"]|"none",sizeof(s.uuid));s.bitrate=o["bitrate"]|0;s.votes=o["votes"]|0;}
  radioLoadScores();radioSort();radioState.lastCatalogMs=millis();radioState.catalogBusy=false;if(radioState.count)radioSaveCache();return radioState.count>0;
}
#if ADV_HAS_WEBRADIO
void radioStopEngine(){if(radioMp3){radioMp3->stop();delete radioMp3;radioMp3=nullptr;}if(radioBuffer){delete radioBuffer;radioBuffer=nullptr;}if(radioFile){radioFile->close();delete radioFile;radioFile=nullptr;}if(radioOut){radioOut->stop();delete radioOut;radioOut=nullptr;}radioState.engineRunning=false;}
bool radioStartEngine(){
  if(hardMute||WiFi.status()!=WL_CONNECTED||radioState.count==0)return false;radioStopEngine();RadioStation& s=radioState.stations[radioState.index];radioFile=new AudioFileSourceICYStream(s.url);radioFile->SetReconnect(3,250);radioBuffer=new AudioFileSourceBuffer(radioFile,8192);radioOut=new AdvAudioOutputM5Speaker(&M5Cardputer.Speaker,1);radioMp3=new AudioGeneratorMP3();radioState.engineRunning=radioMp3->begin(radioBuffer,radioOut);if(!radioState.engineRunning){radioStopEngine();++radioState.failures;}return radioState.engineRunning;
}
void radioTick(){if(!radioState.desiredPlaying||hardMute){if(radioState.engineRunning)radioStopEngine();return;}if(!radioState.engineRunning){radioStartEngine();return;}if(!radioMp3->loop()){++radioState.failures;radioStopEngine();}}
#else
void radioStopEngine(){radioState.engineRunning=false;}
bool radioStartEngine(){return false;}
void radioTick(){}
#endif
void radioSetPlaying(bool on){radioState.desiredPlaying=on;if(!on)radioStopEngine();else if(!hardMute)radioStartEngine();}
void radioStep(int dir){if(!radioState.count)return;radioState.index=(radioState.index+radioState.count+(dir<0?-1:1))%radioState.count;if(radioState.desiredPlaying){radioStopEngine();if(!hardMute)radioStartEngine();}}
void radioVoteLocal(int delta){if(!radioState.count)return;RadioStation& s=radioState.stations[radioState.index];s.localScore=constrain((int)s.localScore+delta,-50,50);radioSaveScore(radioState.index);char uuid[40];strlcpy(uuid,s.uuid,sizeof(uuid));radioSort();for(uint8_t i=0;i<radioState.count;i++)if(strcmp(radioState.stations[i].uuid,uuid)==0){radioState.index=i;break;}}

// -------------------------- rendering --------------------------
void drawTicker(){}
void drawHome(){
  BeaconHomeView v;
  v.tempC=core.tempC;
  v.humidity=core.humidity;
  v.pressureHpa=core.pressureHpa;
  v.entropy=core.entropy;
  v.predicted=core.predEntropy;
  v.loss=core.loss;
  v.future1=core.future1;
  v.future2=core.future2;
  v.future3=core.future3;
  v.sync=eye.online?eye.sync:NAN;
  v.shock=core.imuShock;
  v.battery=core.battery;
  v.wifiRssi=core.wifiRssi;
  v.buzzHashRate=buzzHashRate;
  v.witnessCount=witnessCount;
  v.anomalyCount=anomalyCount;
  v.loopUs=core.loopUs;
  v.shtValid=core.shtValid;
  v.qmpValid=core.qmpValid;
  v.eyeOnline=eye.online;
  v.wifiOnline=WiFi.status()==WL_CONNECTED;
  v.m2r=m2rActive;
  v.house=houseActive;
  v.lora=loraActive;
  v.love=loveContextActive();
  v.anomaly=anomalyLatched;
  v.status=statusLine.c_str();
  beaconHome.draw(v,millis());
}
float histValue(VisualizerSource src,int idx){switch(src){case VisualizerSource::ENV:return isfinite(histTemp[idx])?histTemp[idx]:0;case VisualizerSource::MOTION:return histMotion[idx];case VisualizerSource::SWARM:return eye.online&&isfinite(eye.activity)?eye.activity:histEntropy[idx];case VisualizerSource::PREDICTOR:return histLoss[idx];case VisualizerSource::SELF:return histSelf[idx]/1000.0f;default:return histEntropy[idx];}}
const char* visualName(VisualizerSource s){switch(s){case VisualizerSource::ENV:return"ENV";case VisualizerSource::MOTION:return"MOTION";case VisualizerSource::SWARM:return"SWARM";case VisualizerSource::PREDICTOR:return"PREDICTOR";case VisualizerSource::SELF:return"SELF";case VisualizerSource::KALEIDOSCOPE:return"KALEIDOSCOPE";default:return"?";}}
void drawVisualizer(){
  auto& d=M5Cardputer.Display;d.fillScreen(TFT_BLACK);d.setTextColor(uiPrimary(),TFT_BLACK);d.setCursor(2,2);d.printf("O:%s gain %.1f",visualName(mode.visualizer_source),visualGain);d.setTextColor(TFT_DARKGREY,TFT_BLACK);d.setCursor(145,2);d.print("arrows  ESC");
  if(mode.visualizer_source==VisualizerSource::KALEIDOSCOPE){float e=core.entropy;float m=core.imuLoss+(eye.online&&isfinite(eye.motion)?fabsf(eye.motion)*0.001f:0);int cx=120,cy=72;uint16_t c=d.color565((uint8_t)constrain(40+e*35,0.0f,255.0f),(uint8_t)constrain(80+m*80,0.0f,255.0f),210);for(int ring=1;ring<=7;ring++){float r=8+ring*7+sinf(millis()*0.001f*ring+e)*4;for(int k=0;k<8;k++){float a=k*PI/4.0f+millis()*0.00025f*(ring&1?1:-1);d.drawCircle(cx+cosf(a)*r,cy+sinf(a)*r*0.65f,1+(ring&1),c);}}d.setTextColor(TFT_DARKGREY,TFT_BLACK);d.setCursor(2,124);d.print("VISUALIZATION - NOT EVIDENCE");return;}
  d.drawRect(3,17,234,106,TFT_DARKGREY);if(histCount<2)return;float minV=1e9f,maxV=-1e9f;for(int i=0;i<histCount;i++){float v=histValue(mode.visualizer_source,i);if(!isfinite(v))continue;if(v<minV)minV=v;if(v>maxV)maxV=v;}if(!(maxV>minV))maxV=minV+1;float mid=(minV+maxV)*0.5f,span=(maxV-minV)/visualGain;minV=mid-span*0.5f;maxV=mid+span*0.5f;for(int i=0;i<histCount-1;i++){int idx=(histPos+HIST_N-histCount+i)%HIST_N,idx2=(idx+1)%HIST_N;float v1=histValue(mode.visualizer_source,idx),v2=histValue(mode.visualizer_source,idx2);int x1=4+i*232/max(1,(int)histCount-1),x2=4+(i+1)*232/max(1,(int)histCount-1);int y1=121-(int)(constrain((v1-minV)/(maxV-minV),0.0f,1.0f)*101),y2=121-(int)(constrain((v2-minV)/(maxV-minV),0.0f,1.0f)*101);d.drawLine(x1,y1,x2,y2,uiPrimary());}d.setTextColor(TFT_LIGHTGREY,TFT_BLACK);d.setCursor(6,20);d.printf("%.2f .. %.2f",minV,maxV);
}
void drawZimView(){auto& d=M5Cardputer.Display;d.fillScreen(TFT_BLACK);d.setTextColor(TFT_YELLOW,TFT_BLACK);d.setCursor(2,2);d.print("Z / ADV RESOURCE VIEW");d.setTextColor(TFT_WHITE,TFT_BLACK);d.setCursor(2,18);d.print("PRIMARY: observe+predict+witness");d.setCursor(2,30);d.printf("P0 RUN anomaly:%s",anomalyLatched?"LATCH":"watch");d.setCursor(2,42);d.printf("P1 RUN M2R:%s theta:%.2f",m2rActive?"FULL":"light",m2r.thetaWeight);d.setCursor(2,54);d.printf("P2 swarm RX:%lu TX:%lu/%lu",(unsigned long)espRxCount,(unsigned long)espTxCount,(unsigned long)espTxFail);d.setCursor(2,66);d.printf("heap:%lu loop:%luus max:%lu",(unsigned long)core.freeHeap,(unsigned long)core.loopUs,(unsigned long)core.loopMaxUs);d.setCursor(2,78);d.printf("P3 Buzz %luH/s sh:%lu b:%u",(unsigned long)buzzHashRate,(unsigned long)buzzShares,buzzBestBits);d.setCursor(2,90);d.printf("P4 fg:%d game:%dFPS budget:%u%%",(int)mode.mode,(int)alien.fpsEma(),alienGovernor.discretionary_budget_pct);d.setCursor(2,102);d.printf("P3 throttle:%s P5 defer:%s",alienGovernor.throttle_p3?"YES":"no",alienGovernor.defer_p5?"YES":"no");if(zimExtended){d.setTextColor(TFT_CYAN,TFT_BLACK);d.setCursor(2,114);d.printf("GNSS:%s LoRa:%s WiFi:%d",advGps.location.isValid()?"FIX":"?",loraReady?"HW":"?",core.wifiRssi);}d.setTextColor(TFT_DARKGREY,TFT_BLACK);d.setCursor(2,124);d.print("hold Z detail | ESC HOME");}
void drawRadio(){auto& d=M5Cardputer.Display;if(advWifi.overlayActive()){advWifi.draw(d);return;}d.fillScreen(TFT_BLACK);d.setTextColor(TFT_CYAN,TFT_BLACK);d.setCursor(2,2);d.print("R / INTERNET RADIO");d.setTextColor(TFT_WHITE,TFT_BLACK);if(!ADV_HAS_WEBRADIO){d.setCursor(2,22);d.print("Install ESP8266Audio library.");d.setCursor(2,34);d.print("Core remains alive; no fake PLAY.");}else if(radioState.count==0){d.setCursor(2,22);d.print(radioState.catalogBusy?"Loading Radio Browser...":"No catalogue. hold R refresh");}else{RadioStation& s=radioState.stations[radioState.index];d.setCursor(2,20);d.printf("%02u/%02u %s",radioState.index+1,radioState.count,s.name);d.setCursor(2,34);d.printf("MP3 %uk  local %+d",s.bitrate,s.localScore);d.setCursor(2,48);d.printf("%s engine:%s fail:%lu",radioState.desiredPlaying?"PLAY":"PAUSE",radioState.engineRunning?"RUN":"idle",(unsigned long)radioState.failures);d.setCursor(2,64);d.print("<- -> station   up/down rank");d.setCursor(2,78);d.print("SPACE play/pause  N WiFi  hold R refresh");}d.setTextColor(TFT_DARKGREY,TFT_BLACK);d.setCursor(2,110);d.printf("WiFi:%s RSSI:%d",WiFi.status()==WL_CONNECTED?"ON":"OFF",core.wifiRssi);d.setCursor(2,124);d.print("HTTP MP3 stream | ESC HOME");}
void drawPet(){auto& d=M5Cardputer.Display;d.fillScreen(TFT_BLACK);d.setTextColor(TFT_MAGENTA,TFT_BLACK);d.setCursor(2,2);d.print("D / JANUS PET [SIMULATED]");d.setTextColor(TFT_WHITE,TFT_BLACK);if(petPage==0){d.setCursor(2,18);d.printf("Hunger %3d Thirst %3d",(int)pet.hunger,(int)pet.thirst);d.setCursor(2,30);d.printf("Dirt %3d Energy %3d",(int)pet.dirt,(int)pet.energy);d.setCursor(2,42);d.printf("Mood %3d Health %3d",(int)pet.mood,(int)pet.health);d.setCursor(2,54);d.printf("Comfort %3d %s",(int)pet.comfort,pet.sleeping?"SLEEP":"AWAKE");}else if(petPage==1){d.setCursor(2,18);d.print("REAL ENV -> SIMULATED COMFORT");d.setCursor(2,32);d.printf("T:%s H:%s P:%s",core.shtValid?String(core.tempC,1).c_str():"?",core.shtValid?String(core.humidity,0).c_str():"?",core.qmpValid?String(core.pressureHpa,0).c_str():"?");d.setCursor(2,46);d.printf("comfort:%d/100",(int)pet.comfort);d.setCursor(2,60);d.print("Pet state is NOT sensor evidence.");}else{d.setCursor(2,18);d.printf("Age %lumin interactions %lu",(unsigned long)pet.ageMinutes,(unsigned long)pet.interactions);d.setCursor(2,32);d.printf("Meals %lu Drinks %lu Clean %lu",(unsigned long)pet.meals,(unsigned long)pet.drinks,(unsigned long)pet.cleanups);d.setCursor(2,46);d.print("Persistent via NVS wear-guarded saves");}d.setTextColor(TFT_YELLOW,TFT_BLACK);d.setCursor(2,84);d.printf("ACTION < %s >",petActions[petAction]);d.setTextColor(TFT_LIGHTGREY,TFT_BLACK);d.setCursor(2,100);d.print("<- -> action  up/down page  SPACE");d.setTextColor(TFT_DARKGREY,TFT_BLACK);d.setCursor(2,124);d.print("FICTIONAL STATE | ESC HOME");}
void drawCurrentMode(){switch(mode.mode){case ForegroundMode::HOME:drawHome();break;case ForegroundMode::VISUALIZER_O:drawVisualizer();break;case ForegroundMode::ZIM_VIEW_Z:drawZimView();break;case ForegroundMode::RADIO_R:drawRadio();break;case ForegroundMode::TAMAGOTCHI_D:drawPet();break;case ForegroundMode::ALIEN_SURVIVAL_A:alien.draw(M5Cardputer.Display);break;}}

// -------------------------- input --------------------------
bool keyNow(char a,char b=0){return M5Cardputer.Keyboard.isKeyPressed(a)||(b&&M5Cardputer.Keyboard.isKeyPressed(b));}
struct EdgeState{bool enter=false,esc=false,space=false,l=false,j=false,o=false,z=false,r=false,d=false,a=false,g=false,p=false,i=false,n=false,minus=false,plus=false,lb=false,rb=false,left=false,right=false,up=false,down=false;}prevKey;
bool rising(bool now,bool& prev){bool r=now&&!prev;prev=now;return r;}
struct HoldState{bool was=false,fired=false;uint32_t since=0;};HoldState holdO,holdR,holdZ,holdD;
bool longHold(bool now,HoldState& h,uint32_t ms=900){if(now&&!h.was){h.since=millis();h.fired=false;}bool fire=now&&!h.fired&&millis()-h.since>=ms;if(fire)h.fired=true;if(!now){h.since=0;h.fired=false;}h.was=now;return fire;}
void userActivity(){lastUserInputMs=millis();if(autoVisualizerEntered){mode.escapeToHome();autoVisualizerEntered=false;statusLine="HOME";}}

void handleCodeBuffer(const Keyboard_Class::KeysState& ks){
  if(!M5Cardputer.Keyboard.isChange()||!M5Cardputer.Keyboard.isPressed())return;
  for(char c:ks.word)if(c>='0'&&c<='9'){codeBuffer+=c;if(codeBuffer.length()>12)codeBuffer.remove(0,codeBuffer.length()-12);if(codeBuffer.endsWith("1488")){codeBuffer="";if(m2r.active)stopM2R("manual_cancel");else startM2R();playUiTone(m2rActive?1500:600,50);}else if(codeBuffer.endsWith("112269")){houseActive=!houseActive;codeBuffer="";statusLine=houseActive?"HOUSE 112269 ON":"HOUSE OFF";witness("HOUSE_GATE",houseActive?"on":"off");playUiTone(houseActive?1300:550,50);}}
}

void processInput(){
  M5Cardputer.update();auto ks=M5Cardputer.Keyboard.keysState();if(!advWifi.passwordEntry())handleCodeBuffer(ks);bool topHome=M5Cardputer.BtnA.wasPressed();
  bool any=M5Cardputer.Keyboard.isPressed();if(any)userActivity();
  bool nEnter=ks.enter,nEsc=ks.fn&&keyNow('`','~'),nSpace=ks.space,nL=keyNow('l','L'),nJ=keyNow('j','J'),nO=keyNow('o','O'),nZ=keyNow('z','Z'),nR=keyNow('r','R'),nD=keyNow('d','D'),nA=keyNow('a','A'),nG=keyNow('g','G'),nP=keyNow('p','P'),nI=keyNow('i','I'),nN=keyNow('n','N'),nMinus=keyNow('-','_'),nPlus=keyNow('+','='),nLb=keyNow('[','{'),nRb=keyNow(']','}');
  if(topHome||rising(nEsc,prevKey.esc)){if(mode.mode==ForegroundMode::ALIEN_SURVIVAL_A)alien.leave();if(mode.mode==ForegroundMode::RADIO_R){releaseRadioAudioLease();advWifi.leaveRadio();}mode.escapeToHome();statusLine="HOME";}
  if(!advWifi.overlayActive()&&rising(nEnter,prevKey.enter)){hardMute=!hardMute;if(hardMute)radioStopEngine();applyVolume();if(!hardMute&&radioState.desiredPlaying)radioStartEngine();statusLine=hardMute?"MUTE":"AUDIO ON";witness("AUDIO_MUTE",hardMute?"on":"off");saveUiSettings();}
  if(rising(nMinus,prevKey.minus)){masterVolume=masterVolume>8?masterVolume-8:0;applyVolume();saveUiSettings();}
  if(rising(nPlus,prevKey.plus)){masterVolume=masterVolume<247?masterVolume+8:255;applyVolume();saveUiSettings();}
  if(rising(nLb,prevKey.lb)){illumination.stepDown();applyIllumination();saveUiSettings();}
  if(rising(nRb,prevKey.rb)){illumination.stepUp();applyIllumination();saveUiSettings();}
  if(rising(nL,prevKey.l)){illumination.toggleLed();applyIllumination();statusLine=illumination.led_enabled?"LED ON":"LED OFF";saveUiSettings();}
  if(rising(nJ,prevKey.j)){loraActive=!loraActive;statusLine=loraActive?(loraReady?"LORA ON":"LORA REQUEST/HW FAIL"):"LORA OFF";witness("LORA_GATE",loraActive?"on":"off");}

  if(mode.mode==ForegroundMode::HOME){if(rising(nO,prevKey.o)){mode.enter(ForegroundMode::VISUALIZER_O);statusLine="OSCILLOSCOPE";}if(rising(nZ,prevKey.z)){mode.enter(ForegroundMode::ZIM_VIEW_Z);statusLine="RESOURCE VIEW";}if(rising(nR,prevKey.r)){mode.enter(ForegroundMode::RADIO_R);acquireRadioAudioLease();advWifi.enterRadio();statusLine="RADIO";if(advWifi.connected()&&!radioState.count)radioRefreshCatalog();}if(rising(nD,prevKey.d)){mode.enter(ForegroundMode::TAMAGOTCHI_D);statusLine="PET";}if(rising(nA,prevKey.a)){mode.enter(ForegroundMode::ALIEN_SURVIVAL_A);alien.enter();statusLine="ALIEN SURVIVAL";witness("MODE","alien_survival","CONTROL_STATE");}}
  else{prevKey.o=nO;prevKey.z=nZ;prevKey.r=nR;prevKey.d=nD;prevKey.a=nA;}

  bool left=ks.fn&&keyNow(',','<'),right=ks.fn&&keyNow('/','?'),up=ks.fn&&keyNow(';',':'),down=ks.fn&&keyNow('.','>');
  if(mode.mode==ForegroundMode::VISUALIZER_O){if(rising(left,prevKey.left))mode.visualizerPrev();if(rising(right,prevKey.right))mode.visualizerNext();if(rising(up,prevKey.up))visualGain=constrain(visualGain*1.25f,0.5f,4.0f);if(rising(down,prevKey.down))visualGain=constrain(visualGain/1.25f,0.5f,4.0f);if(longHold(nO,holdO)){deskVisualizerEnabled=!deskVisualizerEnabled;statusLine=deskVisualizerEnabled?"DESK VIS ON":"DESK VIS OFF";saveUiSettings();}}
  else if(mode.mode==ForegroundMode::ZIM_VIEW_Z){if(longHold(nZ,holdZ))zimExtended=!zimExtended;prevKey.left=left;prevKey.right=right;prevKey.up=up;prevKey.down=down;prevKey.space=nSpace;}
  else if(mode.mode==ForegroundMode::RADIO_R){if(advWifi.overlayActive()){if(advWifi.passwordEntry()&&M5Cardputer.Keyboard.isChange()&&M5Cardputer.Keyboard.isPressed()&&!ks.fn){for(char c:ks.word)advWifi.onChar(c);if(ks.del)advWifi.onBackspace();}if(rising(up,prevKey.up))advWifi.onUp();if(rising(down,prevKey.down))advWifi.onDown();if(rising(nEnter,prevKey.enter))advWifi.onEnter();prevKey.left=left;prevKey.right=right;prevKey.space=nSpace;}else{if(rising(left,prevKey.left))radioStep(-1);if(rising(right,prevKey.right))radioStep(1);if(rising(up,prevKey.up))radioVoteLocal(1);if(rising(down,prevKey.down))radioVoteLocal(-1);if(rising(nSpace,prevKey.space))radioSetPlaying(!radioState.desiredPlaying);if(rising(nN,prevKey.n)){radioSetPlaying(false);advWifi.forceScan();}if(longHold(nR,holdR)){radioSetPlaying(false);radioRefreshCatalog();}}}
  else if(mode.mode==ForegroundMode::TAMAGOTCHI_D){if(rising(left,prevKey.left))petAction=(petAction+PET_ACTION_COUNT-1)%PET_ACTION_COUNT;if(rising(right,prevKey.right))petAction=(petAction+1)%PET_ACTION_COUNT;if(rising(up,prevKey.up))petPage=(petPage+2)%3;if(rising(down,prevKey.down))petPage=(petPage+1)%3;if(rising(nSpace,prevKey.space))petDoAction();if(longHold(nD,holdD))petPage=2;}
  else if(mode.mode==ForegroundMode::ALIEN_SURVIVAL_A){if(rising(nG,prevKey.g))alien.toggleGyro(core.ax,core.ay);if(rising(nP,prevKey.p))alien.togglePause();if(rising(nI,prevKey.i))alien.toggleAutoFire();if(alien.dead()&&rising(nSpace,prevKey.space))alien.restartIfDead();alien.update(millis(),left,right,up,down,nSpace,core.ax,core.ay,sqrtf(core.gx*core.gx+core.gy*core.gy+core.gz*core.gz)*0.010f);}
  else{prevKey.left=left;prevKey.right=right;prevKey.up=up;prevKey.down=down;prevKey.space=nSpace;}
  if(mode.mode!=ForegroundMode::ALIEN_SURVIVAL_A){prevKey.g=nG;prevKey.p=nP;prevKey.i=nI;}if(mode.mode!=ForegroundMode::RADIO_R)prevKey.n=nN;
}

// -------------------------- setup / loop --------------------------
void setup(){
  auto cfg=M5.config();M5Cardputer.begin(cfg,true);Serial.begin(115200);M5Cardputer.Display.setRotation(1);M5Cardputer.Display.setTextSize(1);
  LittleFS.begin(true);SPI.begin(SD_SCK_PIN,SD_MISO_PIN,SD_MOSI_PIN,SD_CS_PIN);bool sdOk=SD.begin(SD_CS_PIN,SPI,25000000);Serial.printf("[ADV] SD=%d\n",sdOk?1:0);
  Wire.begin(GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);bool shtOk=shtAsync.begin(&Wire,0x44);bool qmpOk=advQmp.begin(&Wire,QMP6988_SLAVE_ADDRESS_L,GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);if(!qmpOk)qmpOk=advQmp.begin(&Wire,QMP6988_SLAVE_ADDRESS_H,GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);Serial.printf("[ADV] ENV SHT=%d QMP=%d\n",shtOk?1:0,qmpOk?1:0);
  M5.Imu.init();core.imuValid=true;calibrateImuZero();
  FastLED.addLeds<WS2812,LED_PIN,GRB>(advLed,1);loadUiSettings();applyIllumination();M5Cardputer.Speaker.begin();applyVolume();
  loadPet();alien.begin();beaconHome.begin();String factorySsid,factoryPass;bool factoryCred=loadFactoryPrimaryCredential(factorySsid,factoryPass);advWifi.begin(factoryCred?factorySsid.c_str():ADV_WIFI_SSID,factoryCred?factoryPass.c_str():ADV_WIFI_PASSWORD);buzzWorkerId=(uint16_t)(ESP.getEfuseMac()&0xFFFF);initEspNow();advWifi.enterRadio();initGnssAndLoRa();if(SD.cardType()!=CARD_NONE)radioLoadCache();
  core.predEntropy=core.entropy;core.battery=M5.Power.getBatteryLevel();core.freeHeap=ESP.getFreeHeap();lastUserInputMs=millis();witness("BOOT","ADV_Elite_RC2","CONTROL_STATE");statusLine="RC2 READY";
}

void loop(){
  uint32_t loopStart=micros();processInput();gnssTick();uint32_t now=millis();
  if(deskVisualizerEnabled&&mode.mode==ForegroundMode::HOME&&now-lastUserInputMs>=DESK_VISUALIZER_IDLE_MS){mode.enter(ForegroundMode::VISUALIZER_O);mode.visualizer_source=VisualizerSource::KALEIDOSCOPE;autoVisualizerEntered=true;statusLine="DESK VISUALIZER";}
  serviceEnv();
  if(now-lastCoreMs>=CORE_INTERVAL_MS){lastCoreMs=now;readImu();if(eye.online&&now-eye.lastOkMs>18000UL)eye.online=false;if(audioNode.online&&now-audioNode.lastOkMs>18000UL)audioNode.online=false;core.battery=M5.Power.getBatteryLevel();core.wifiRssi=WiFi.status()==WL_CONNECTED?WiFi.RSSI():-127;core.freeHeap=ESP.getFreeHeap();updatePredictorAndAnomaly();petTick();m2rTick();}
  if(now-lastHeartbeatMs>=HEARTBEAT_MS){lastHeartbeatMs=now;sendHeartbeat();}
  advWifi.tick();if(mode.mode==ForegroundMode::RADIO_R&&advWifi.consumeConnectedEvent()&&!radioState.count)radioRefreshCatalog();alienGovernor.update(alien.fpsEma(),mode.mode==ForegroundMode::ALIEN_SURVIVAL_A&&!alien.paused());runBuzzBatch();maybeSendLoRaSeal();radioTick();brainWaveTick();updateLed();
  if(now-lastDrawMs>=DRAW_INTERVAL_MS){lastDrawMs=now;drawCurrentMode();}
  core.loopUs=micros()-loopStart;if(core.loopUs>core.loopMaxUs)core.loopMaxUs=core.loopUs;delay(1);
}
