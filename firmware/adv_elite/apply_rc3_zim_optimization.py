#!/usr/bin/env python3
from pathlib import Path

p = Path(__file__).with_name('ADV_Elite.ino')
s = p.read_text(encoding='utf-8')

if '// RC3_ZIM_OPTIMIZED' in s:
    print('RC3 ZIM optimization already applied')
    raise SystemExit(0)

def must_replace(old, new, label):
    global s
    if old not in s:
        raise RuntimeError(f'{label}: expected text not found')
    s = s.replace(old, new, 1)

def replace_between(start, end, body, label):
    global s
    a = s.find(start)
    if a < 0:
        raise RuntimeError(f'{label}: start marker not found')
    b = s.find(end, a)
    if b < 0:
        raise RuntimeError(f'{label}: end marker not found')
    s = s[:a] + body.rstrip() + '\n\n' + s[b:]

must_replace(
    '#include "ADV_Elite_sht3x_async.h"\n',
    '#include "ADV_Elite_sht3x_async.h"\n#include "ADV_Elite_runtime_governor.h"\n\n// RC3_ZIM_OPTIMIZED\n',
    'governor include')

must_replace(
    'AdvSht3xAsync shtAsync;\nPreferences prefs;',
    'AdvSht3xAsync shtAsync;\nAdvRuntimeGovernor runtimeGovernor;\nPreferences prefs;',
    'governor instance')

must_replace(
    'uint32_t lastUserInputMs = 0;\nuint8_t brainStep = 0;',
    'uint32_t lastUserInputMs = 0;\nuint32_t lastRobustStatsMs = 0;\nuint32_t lastLedPushMs = 0;\nuint8_t brainStep = 0;',
    'runtime timestamps')

# Witness: build/hash synchronously, defer filesystem/SD I/O to residual budget.
replace_between(
    'void witness(const char* type,const char* detail,const char* truthClass="CONTROL_STATE") {',
    'void saveUiSettings() {',
    r'''static constexpr uint8_t WITNESS_QUEUE_N = 8;
static constexpr size_t WITNESS_LINE_MAX = 896;
struct WitnessPending { char line[WITNESS_LINE_MAX] = {}; };
WitnessPending witnessQueue[WITNESS_QUEUE_N];
uint8_t witnessQHead=0,witnessQTail=0,witnessQCount=0;

void writeWitnessLineNow(const char* line){
  File f=LittleFS.open(WITNESS_LFS,FILE_APPEND);if(f){f.println(line);f.close();}
  if(SD.cardType()!=CARD_NONE){ensureSdWitnessDir();File sf=SD.open(WITNESS_SD,FILE_APPEND);if(sf){sf.println(line);sf.close();}}
}

void serviceWitnessQueue(uint8_t quota){
  while(quota--&&witnessQCount){
    writeWitnessLineNow(witnessQueue[witnessQTail].line);
    witnessQTail=(witnessQTail+1)%WITNESS_QUEUE_N;
    --witnessQCount;
  }
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
  char newHash[65];sha256Hex(String(witnessPrevHash)+payload,newHash);
  String line=payload.substring(0,payload.length()-1)+",\"hash\":\""+String(newHash)+"\"}";

  // Never break the hash chain. Queue saturation is exceptional; flush one oldest
  // synchronously rather than dropping an event whose hash is already referenced.
  if(witnessQCount>=WITNESS_QUEUE_N){
    writeWitnessLineNow(witnessQueue[witnessQTail].line);
    witnessQTail=(witnessQTail+1)%WITNESS_QUEUE_N;
    --witnessQCount;
  }
  if(line.length()>=WITNESS_LINE_MAX){
    // Current event payloads are much smaller; keep valid JSON + chain if a future
    // caller supplies pathological detail.
    line=String("{\"schema\":\"JANUS_EVENT_V1\",\"source\":\"ADV_Elite\",\"ts_ms\":")+String(millis())+
         ",\"type\":\"WITNESS_OVERSIZE\",\"truth_class\":\"CONTROL_STATE\",\"detail\":\"payload_compacted\",\"prev_hash\":\""+
         String(witnessPrevHash)+"\",\"hash\":\""+String(newHash)+"\"}";
  }
  line.toCharArray(witnessQueue[witnessQHead].line,WITNESS_LINE_MAX);
  witnessQHead=(witnessQHead+1)%WITNESS_QUEUE_N;
  ++witnessQCount;
  strlcpy(witnessPrevHash,newHash,sizeof(witnessPrevHash));
  ++witnessCount;
}
''',
    'witness queue')

replace_between(
    'void updateLed(){',
    'float meanArr(',
    r'''void updateLed(){
  const uint32_t now=millis();
  const uint16_t cadence=runtimeGovernor.ledIntervalMs();
  if(now-lastLedPushMs<cadence&&!anomalyLatched)return;

  CRGB target;
  if(!illumination.led_enabled||illumination.ledBrightness()==0)target=CRGB::Black;
  else if(anomalyLatched)target=CRGB::White;
  else if(mode.mode==ForegroundMode::ALIEN_SURVIVAL_A){AlienRgb c=AlienLedPolicy::healthColor(alien.health01());target=CRGB(c.r,c.g,c.b);}
  else if(houseActive)target=CRGB(255,140,0);
  else {uint8_t hue=(uint8_t)constrain(160-(int)(core.entropy*14.0f),0,160);target=CHSV(hue,255,255);}

  static uint8_t lastR=255,lastG=255,lastB=255,lastBright=255;
  const uint8_t bright=illumination.ledBrightness();
  if(target.r==lastR&&target.g==lastG&&target.b==lastB&&bright==lastBright&&now-lastLedPushMs<1000UL)return;
  advLed[0]=target;
  FastLED.setBrightness(bright);
  FastLED.show();
  lastR=target.r;lastG=target.g;lastB=target.b;lastBright=bright;lastLedPushMs=now;
}

float meanArr(''',
    'LED cadence')

# The replacement above intentionally consumes the function name marker; repair its signature.
s=s.replace('float meanArr(const float* a,int n)', 'float meanArr(const float* a,int n)', 1)

replace_between(
    'void updatePredictorAndAnomaly(){',
    '// -------------------------- M2R manual future session --------------------------',
    r'''void updatePredictorAndAnomaly(){
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
  core.classicZE=core.classicZL=0;
  core.disagreement=(eye.online&&isfinite(eye.activity)&&isfinite(eye.predActivity))?fabsf(eye.activity-eye.predActivity):0.0f;
  if(histCount>=24){
    // Cheap classic Z remains live every P1 tick: anomaly attention is never gated.
    float mE=meanArr(histEntropy,histCount),sE=stdArr(histEntropy,histCount,mE);
    float mL=meanArr(histLoss,histCount),sL=stdArr(histLoss,histCount,mL);
    core.classicZE=sE>1e-5f?fabsf((core.entropy-mE)/sE):0;
    core.classicZL=sL>1e-5f?fabsf((core.loss-mL)/sL):0;

    // Median/MAD is O(n^2) in the tiny insertion-sort implementation. Cache it and
    // refresh on a governor cadence; classic Z + disagreement still react immediately.
    const uint32_t now=millis();
    if(lastRobustStatsMs==0||now-lastRobustStatsMs>=runtimeGovernor.robustStatsIntervalMs()){
      lastRobustStatsMs=now;
      float medE=medianCopy(histEntropy,histCount),madE=madArr(histEntropy,histCount,medE);
      float medL=medianCopy(histLoss,histCount),madL=madArr(histLoss,histCount,medL);
      core.robustZE=madE>1e-5f?0.6745f*fabsf(core.entropy-medE)/madE:0;
      core.robustZL=madL>1e-5f?0.6745f*fabsf(core.loss-medL)/madL:0;
    }
    anomaly=core.classicZE>4.2f||core.classicZL>4.0f||core.robustZE>5.0f||core.robustZL>5.0f||core.disagreement>2.25f;
  }
  if(anomaly&&!anomalyLatched){anomalyLatched=true;++anomalyCount;statusLine="ANOMALY / WITNESS";witness("ANOMALY","z+mad+prediction+peer_disagreement","DERIVED_FROM_REAL");playUiTone(820,80);}
  else if(!anomaly)anomalyLatched=false;
}

// -------------------------- M2R manual future session --------------------------''',
    'predictor split')

replace_between(
    'void runBuzzBatch(){',
    '// -------------------------- ESP-NOW --------------------------',
    r'''void runBuzzBatch(uint16_t budgetUs){
  if(!buzzJob.active||millis()-buzzJob.receivedAt>BUZZ_JOB_TTL_MS){buzzJob.active=false;return;}
  if(budgetUs==0){if(millis()-lastBuzzRateMs>=1000UL){buzzHashRate=buzzHashCounter;buzzHashCounter=0;lastBuzzRateMs=millis();}return;}
  const uint32_t started=micros();
  const uint16_t hardCap=alienGovernor.throttle_p3?40:180;
  uint16_t done=0;uint8_t hash[32];
  while(done<hardCap&&buzzJob.active){
    if((uint32_t)(micros()-started)>=budgetUs)break;
    if(buzzJob.nonce>=buzzJob.endNonce){buzzJob.active=false;break;}
    uint32_t nonce=buzzJob.nonce++;writeLE32(buzzJob.header+76,nonce);doubleSha256(buzzJob.header,80,hash);++buzzHashCounter;++done;
    uint16_t bits=leadingZeroBitsBE(hash);if(bits>buzzBestBits)buzzBestBits=bits;
    if(hashMeetsTargetBE(hash,buzzJob.target)){sendShare(buzzJob,nonce);buzzJob.active=false;break;}
  }
  if(millis()-lastBuzzRateMs>=1000UL){buzzHashRate=buzzHashCounter;buzzHashCounter=0;lastBuzzRateMs=millis();}
}

// -------------------------- ESP-NOW --------------------------''',
    'Buzz budget')

must_replace(
    'void rescueEspNow(const char* why){if(millis()-lastEspRescueMs<ESP_RESCUE_COOLDOWN_MS)return;lastEspRescueMs=millis();esp_now_deinit();delay(5);initEspNow();witness("ESPNOW_RESCUE",why,"CONTROL_STATE");}',
    'bool espRescuePending=false;char espRescueWhy[32]="";\nvoid rescueEspNow(const char* why){if(millis()-lastEspRescueMs<ESP_RESCUE_COOLDOWN_MS)return;espRescuePending=true;strlcpy(espRescueWhy,why,sizeof(espRescueWhy));}\nvoid serviceEspRescue(){if(!espRescuePending||!runtimeGovernor.allowMaintenance())return;lastEspRescueMs=millis();espRescuePending=false;esp_now_deinit();delay(5);initEspNow();witness("ESPNOW_RESCUE",espRescueWhy,"CONTROL_STATE");}',
    'ESP-NOW rescue deferral')

replace_between(
    'void initGnssAndLoRa(){',
    '// -------------------------- exclusive foreground audio lease --------------------------',
    r'''volatile bool loraTxDoneFlag=false;
bool loraTxPending=false;
void onLoraTxDone(){loraTxDoneFlag=true;}

void initGnssAndLoRa(){
  Serial2.begin(GNSS_BAUD,SERIAL_8N1,GNSS_RX_PIN,GNSS_TX_PIN);
#if ADV_HAS_RADIOLIB
  pinMode(LORA_PWR_EN,OUTPUT);digitalWrite(LORA_PWR_EN,HIGH);delay(80);int st=advRadio.begin(868.0,125.0,9,7,0x34,10,8,1.6,false);loraReady=(st==RADIOLIB_ERR_NONE);if(loraReady){advRadio.setOutputPower(10);advRadio.setCRC(true);advRadio.setDio1Action(onLoraTxDone);}
#else
  loraReady=false;
#endif
}
void gnssTick(){while(Serial2.available())advGps.encode((char)Serial2.read());}
void maybeSendLoRaSeal(bool allowStart){
#if ADV_HAS_RADIOLIB
  if(loraTxPending&&loraTxDoneFlag){loraTxDoneFlag=false;advRadio.finishTransmit();loraTxPending=false;}
  if(!allowStart||loraTxPending||!loraActive||!loraReady||millis()-lastGpsSealMs<30000UL)return;
  lastGpsSealMs=millis();
  String s="JSA3|ADV|"+String(millis()/1000UL)+"|E="+String(core.entropy,2)+"|A="+String(anomalyLatched?1:0);
  if(advGps.location.isValid())s+="|FIX=1|SAT="+String(advGps.satellites.value());else s+="|FIX=0";
  loraTxDoneFlag=false;int st=advRadio.startTransmit(s.c_str());
  if(st==RADIOLIB_ERR_NONE)loraTxPending=true;else statusLine="LORA TX FAIL";
#endif
}

// -------------------------- exclusive foreground audio lease --------------------------''',
    'async LoRa')

# Radio Browser HTTP/JSON moves to Core 0. Main loop only atomically publishes staged catalogue.
replace_between(
    'bool radioRefreshCatalog(){',
    '#if ADV_HAS_WEBRADIO',
    r'''RadioStation radioCatalogStage[RADIO_MAX];
volatile uint8_t radioCatalogStageCount=0;
volatile bool radioCatalogStageReady=false;
volatile bool radioCatalogStageOk=false;
TaskHandle_t radioCatalogTaskHandle=nullptr;
bool radioCacheDirty=false;

void radioCatalogWorker(void*){
  uint8_t count=0;bool ok=false;
  if(WiFi.status()==WL_CONNECTED){
    WiFiClientSecure client;client.setInsecure();HTTPClient http;
    String url="https://de1.api.radio-browser.info/json/stations/search?codec=MP3&is_https=false&hidebroken=true&order=votes&reverse=true&limit=24";
    if(http.begin(client,url)){
      http.addHeader("User-Agent","JANUS-ADV-Elite/3.0");
      int code=http.GET();
      if(code==200){
        DynamicJsonDocument doc(32768);DeserializationError de=deserializeJson(doc,http.getStream());
        if(!de){
          for(JsonObject o:doc.as<JsonArray>()){
            if(count>=RADIO_MAX)break;const char* u=o["url_resolved"]|"";const char* c=o["codec"]|"";
            if(strncmp(u,"http://",7)!=0||strcasecmp(c,"MP3")!=0)continue;
            RadioStation& st=radioCatalogStage[count++];
            strlcpy(st.name,o["name"]|"station",sizeof(st.name));strlcpy(st.url,u,sizeof(st.url));strlcpy(st.uuid,o["stationuuid"]|"none",sizeof(st.uuid));st.bitrate=o["bitrate"]|0;st.votes=o["votes"]|0;st.localScore=0;
          }
          ok=count>0;
        }
      }
      http.end();
    }
  }
  radioCatalogStageCount=count;radioCatalogStageOk=ok;radioCatalogStageReady=true;radioCatalogTaskHandle=nullptr;vTaskDelete(nullptr);
}

bool radioRefreshCatalog(){
  if(radioState.catalogBusy||radioCatalogTaskHandle)return false;
  if(WiFi.status()!=WL_CONNECTED)return radioState.count>0||radioLoadCache();
  if(radioState.engineRunning||!runtimeGovernor.allowRadioCatalogStart()){statusLine="CATALOG DEFERRED";return false;}
  radioState.catalogBusy=true;radioCatalogStageReady=false;radioCatalogStageOk=false;radioCatalogStageCount=0;
  BaseType_t rc=xTaskCreatePinnedToCore(radioCatalogWorker,"adv_radio_cat",9216,nullptr,1,&radioCatalogTaskHandle,0);
  if(rc!=pdPASS){radioCatalogTaskHandle=nullptr;radioState.catalogBusy=false;statusLine="CATALOG TASK FAIL";return false;}
  return true;
}

void radioCatalogService(){
  if(!radioCatalogStageReady)return;
  radioCatalogStageReady=false;radioState.catalogBusy=false;
  if(!radioCatalogStageOk){statusLine="CATALOG NET FAIL";return;}
  uint8_t count=min<uint8_t>(radioCatalogStageCount,RADIO_MAX);radioState.count=count;
  for(uint8_t i=0;i<count;i++)radioState.stations[i]=radioCatalogStage[i];
  radioLoadScores();radioSort();radioState.lastCatalogMs=millis();radioCacheDirty=true;statusLine="CATALOG READY";
}

void serviceRadioCache(){
  if(!radioCacheDirty||!runtimeGovernor.allowMaintenance())return;
  if(mode.mode==ForegroundMode::ALIEN_SURVIVAL_A||mode.mode==ForegroundMode::RADIO_R)return;
  radioSaveCache();radioCacheDirty=false;
}

#if ADV_HAS_WEBRADIO''',
    'radio catalog worker')

# Add governor visibility to the Z resource view without changing the screen contract.
must_replace(
    'd.setCursor(2,102);d.printf("P3 throttle:%s P5 defer:%s",alienGovernor.throttle_p3?"YES":"no",alienGovernor.defer_p5?"YES":"no");',
    'd.setCursor(2,102);d.printf("GOV:%s ema:%lu peak:%lu",runtimeGovernor.label(),(unsigned long)runtimeGovernor.loopEmaUs(),(unsigned long)runtimeGovernor.loopPeakUs());',
    'ZIM governor HUD')

# setup: initialize governor before first deferred witness.
must_replace(
    'loadPet();alien.begin();beaconHome.begin();String factorySsid,factoryPass;',
    'loadPet();alien.begin();beaconHome.begin();runtimeGovernor.begin(millis());String factorySsid,factoryPass;',
    'governor setup')

# Replace the terminal loop: foreground first, residual work after draw.
a=s.find('void loop(){')
if a<0:
    raise RuntimeError('loop start not found')
new_loop=r'''void loop(){
  const uint32_t loopStart=micros();
  processInput();
  gnssTick();
  const uint32_t now=millis();
  const bool gameFg=mode.mode==ForegroundMode::ALIEN_SURVIVAL_A;
  const bool radioFg=mode.mode==ForegroundMode::RADIO_R;

  if(deskVisualizerEnabled&&mode.mode==ForegroundMode::HOME&&now-lastUserInputMs>=DESK_VISUALIZER_IDLE_MS){mode.enter(ForegroundMode::VISUALIZER_O);mode.visualizer_source=VisualizerSource::KALEIDOSCOPE;autoVisualizerEntered=true;statusLine="DESK VISUALIZER";}

  // P0/P1: never gated.
  serviceEnv();
  if(now-lastCoreMs>=CORE_INTERVAL_MS){
    lastCoreMs=now;readImu();
    if(eye.online&&now-eye.lastOkMs>18000UL)eye.online=false;
    if(audioNode.online&&now-audioNode.lastOkMs>18000UL)audioNode.online=false;
    core.battery=M5.Power.getBatteryLevel();
    core.wifiRssi=WiFi.status()==WL_CONNECTED?WiFi.RSSI():-127;
    core.freeHeap=ESP.getFreeHeap();
    updatePredictorAndAnomaly();petTick();m2rTick();
  }

  runtimeGovernor.update(now,core.freeHeap,core.wifiRssi,WiFi.status()==WL_CONNECTED,core.loopUs,alien.fpsEma(),gameFg,radioFg,radioState.engineRunning);
  alienGovernor.update(alien.fpsEma(),gameFg&&!alien.paused());

  // P4 audio must remain continuous in radio foreground; Wi-Fi state machine is short/non-blocking.
  advWifi.tick();
  if(radioFg&&advWifi.consumeConnectedEvent()&&!radioState.count)radioRefreshCatalog();
  radioCatalogService();
  if(radioFg||radioState.engineRunning)radioTick();

  // Presentation runs before discretionary compute. Missed cadence can therefore only
  // throttle background organs, not make the foreground wait behind them.
  const uint16_t drawMs=runtimeGovernor.drawIntervalMs(gameFg);
  if(now-lastDrawMs>=drawMs){lastDrawMs=now;drawCurrentMode();}

  // P2 essential swarm heartbeat.
  if(now-lastHeartbeatMs>=HEARTBEAT_MS){lastHeartbeatMs=now;sendHeartbeat();}
  brainWaveTick();
  updateLed();

  // P3: exact Buzz work is deadline-bounded like ZIM miner batches.
  runBuzzBatch(runtimeGovernor.buzzBudgetUs(gameFg,radioFg));

  // P5 residual maintenance. No foreground path waits for SD/LoRa/catalogue work.
  serviceEspRescue();
  maybeSendLoRaSeal(runtimeGovernor.allowLoRaStart(gameFg,radioFg));
  serviceWitnessQueue(runtimeGovernor.witnessFlushQuota());
  serviceRadioCache();

  core.loopUs=micros()-loopStart;
  if(core.loopUs>core.loopMaxUs)core.loopMaxUs=core.loopUs;
  delay(1);
}
'''
s=s[:a]+new_loop

p.write_text(s,encoding='utf-8')
print('RC3 ZIM optimization applied')
