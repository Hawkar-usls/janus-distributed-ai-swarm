#!/usr/bin/env python3
from pathlib import Path

root=Path(__file__).resolve().parents[1]/'firmware'/'adv_elite'
ino=root/'ADV_Elite.ino'
alien=root/'ADV_Elite_alien_survival_runtime.h'
wifi=root/'ADV_Elite_wifi_manager.h'

s=ino.read_text(encoding='utf-8')
a=alien.read_text(encoding='utf-8')
w=wifi.read_text(encoding='utf-8')

if '// RC3_ZIM_PASS2' in s:
    print('pass2 already applied');raise SystemExit(0)

def rep(text,old,new,label):
    if old not in text: raise RuntimeError(f'{label}: marker not found')
    return text.replace(old,new,1)

# --- game canvas: no idle 16-bit 65 KB allocation ---
a=rep(a,
'''  void begin() {
    if (!frameReady_) {
      frame_.setColorDepth(16);
      frame_.createSprite(kScreenW, kScreenH);
      frameReady_ = frame_.width() == kScreenW && frame_.height() == kScreenH;
    }
    resetRun();
    lastWallMs_ = millis();
    lastDrawMs_ = lastWallMs_;
  }

  void enter() {
    active_ = true;
''',
'''  void begin() {
    // ZIM-style: do not reserve a game framebuffer while HOME/Radio owns foreground.
    resetRun();
    lastWallMs_ = millis();
    lastDrawMs_ = lastWallMs_;
  }

  void enter() {
    ensureFrame();
    active_ = true;
''','alien lazy begin')
a=rep(a,
'''  void leave() {
    active_ = false;
    paused_ = false;
  }
''',
'''  void leave() {
    active_ = false;
    paused_ = false;
    // Return sprite RAM to the rest of JANUS when game is not foreground.
    if (frameReady_) { frame_.deleteSprite(); frameReady_ = false; }
  }
''','alien leave release')
a=rep(a,
'''  static float dist2(float x, float y) { return x*x + y*y; }
''',
'''  void ensureFrame() {
    if (frameReady_) return;
    frame_.setColorDepth(8);
    frame_.createSprite(kScreenW, kScreenH);
    frameReady_ = frame_.width() == kScreenW && frame_.height() == kScreenH;
  }

  static float dist2(float x, float y) { return x*x + y*y; }
''','alien ensure frame')

# --- Wi-Fi: no transition sleeps; persist fallback only in residual P5 ---
w=rep(w,
'''        if (state_ == State::CONNECT_PICKED) savePicked();
''',
'''        if (state_ == State::CONNECT_PICKED) stagePickedSave();
''','wifi stage save')
w=rep(w,
'''  bool consumeConnectedEvent() {
''',
'''  void servicePersistence() {
    if (!savePending_) return;
    savePending_ = false;
    if (!store_.begin("adv_wifi", false)) return;
    store_.putString("ssid", savedSsid_);
    store_.putString("pwd", savedPassword_);
    store_.end();
  }

  bool consumeConnectedEvent() {
''','wifi service persistence')
w=rep(w,
'''  bool connectedEvent_ = false;
  char error_[32] = {};
''',
'''  bool connectedEvent_ = false;
  bool savePending_ = false;
  char error_[32] = {};
''','wifi dirty flag')
start=w.find('  void savePicked() {')
end=w.find('  void startPrimary() {',start)
if start<0 or end<0: raise RuntimeError('wifi savePicked block')
w=w[:start]+'''  void stagePickedSave() {
    if (pickedSsid_.length() == 0) return;
    savedSsid_ = pickedSsid_;
    savedPassword_ = password_;
    savePending_ = true;
  }

'''+w[end:]
w=w.replace('    delay(20);\n    WiFi.mode(WIFI_STA);','    WiFi.mode(WIFI_STA);')

# --- persistence: user operations mark dirty; P5 commits later ---
s=rep(s,'// RC3_ZIM_OPTIMIZED\n','// RC3_ZIM_OPTIMIZED\n// RC3_ZIM_PASS2\n','pass2 marker')
s=rep(s,
'''uint32_t lastLedPushMs = 0;
uint8_t brainStep = 0;
''',
'''uint32_t lastLedPushMs = 0;
uint32_t lastBatteryPollMs = 0;
uint32_t lastWifiTelemetryMs = 0;
bool uiPrefsDirty = false;
bool petPrefsDirty = false;
bool radioScoreDirty = false;
uint8_t radioScoreDirtyIndex = 0;
uint8_t brainStep = 0;
''','pass2 state')

old='''void saveUiSettings() {
  prefs.begin("adv_ui",false);
  prefs.putUChar("vol",masterVolume);
  prefs.putBool("mute",hardMute);
  prefs.putUChar("bright",illumination.level_index);
  prefs.putBool("led",illumination.led_enabled);
  prefs.putBool("desk",deskVisualizerEnabled);
  prefs.putFloat("theta",m2r.thetaWeight);
  prefs.end();
}
'''
new='''void saveUiSettingsNow() {
  prefs.begin("adv_ui",false);
  prefs.putUChar("vol",masterVolume);prefs.putBool("mute",hardMute);prefs.putUChar("bright",illumination.level_index);
  prefs.putBool("led",illumination.led_enabled);prefs.putBool("desk",deskVisualizerEnabled);prefs.putFloat("theta",m2r.thetaWeight);prefs.end();
}
void saveUiSettings(){uiPrefsDirty=true;}
'''
s=rep(s,old,new,'ui deferred persist')

old='''void savePet(){prefs.begin("adv_pet",false);pet.hunger=prefs.getFloat("hung",pet.hunger);prefs.end();}'''
# source has a long one-line savePet; locate robustly
ps=s.find('void savePet(){')
pe=s.find('\nvoid petTick(){',ps)
if ps<0 or pe<0: raise RuntimeError('pet save block')
orig=s[ps:pe]
actual=orig.replace('void savePet(){','void savePetNow(){',1)
s=s[:ps]+actual+'\nvoid savePet(){petPrefsDirty=true;}'+s[pe:]

# radio score deferred
rs=s.find('void radioSaveScore(uint8_t i){')
re=s.find('\nint32_t stationRank',rs)
if rs<0 or re<0: raise RuntimeError('radio score block')
orig=s[rs:re]
actual=orig.replace('void radioSaveScore(uint8_t i){','void radioSaveScoreNow(uint8_t i){',1)
s=s[:rs]+actual+'\nvoid radioSaveScore(uint8_t i){if(i>=radioState.count)return;radioScoreDirtyIndex=i;radioScoreDirty=true;}'+s[re:]

# ESP-NOW does not own Wi-Fi association and never spins for seconds during rescue.
old='''bool initEspNow(){
  WiFi.mode(WIFI_STA);
  if(strlen(ADV_WIFI_SSID)>0&&WiFi.status()!=WL_CONNECTED){WiFi.begin(ADV_WIFI_SSID,ADV_WIFI_PASSWORD);uint32_t until=millis()+3500UL;while(WiFi.status()!=WL_CONNECTED&&millis()<until)delay(25);}
'''
new='''bool initEspNow(){
  WiFi.mode(WIFI_STA);
'''
s=rep(s,old,new,'espnow no connect spin')

# Bounded GNSS drain: no serial backlog may monopolize a frame.
s=rep(s,
'void gnssTick(){while(Serial2.available())advGps.encode((char)Serial2.read());}',
'''void gnssTick(){
  const uint32_t started=micros();uint8_t n=0;
  while(Serial2.available()&&n<96&&(uint32_t)(micros()-started)<350UL){advGps.encode((char)Serial2.read());++n;}
}''','gnss budget')

# P5 deferred persistence service.
insert='''void serviceDeferredPersistence(){
  if(!runtimeGovernor.allowMaintenance())return;
  const bool heavyFg=mode.mode==ForegroundMode::ALIEN_SURVIVAL_A||(mode.mode==ForegroundMode::RADIO_R&&radioState.engineRunning);
  if(heavyFg)return;
  if(uiPrefsDirty){uiPrefsDirty=false;saveUiSettingsNow();return;}
  if(petPrefsDirty){petPrefsDirty=false;savePetNow();return;}
  if(radioScoreDirty){uint8_t i=radioScoreDirtyIndex;radioScoreDirty=false;radioSaveScoreNow(i);return;}
  advWifi.servicePersistence();
}

'''
marker='// -------------------------- rendering --------------------------\n'
if marker not in s: raise RuntimeError('persistence insertion marker')
s=s.replace(marker,insert+marker,1)

# Slow battery/RSSI telemetry while preserving P1 IMU/anomaly cadence.
s=rep(s,
'''    core.battery=M5.Power.getBatteryLevel();
    core.wifiRssi=WiFi.status()==WL_CONNECTED?WiFi.RSSI():-127;
    core.freeHeap=ESP.getFreeHeap();
''',
'''    if(now-lastBatteryPollMs>=2000UL){lastBatteryPollMs=now;core.battery=M5.Power.getBatteryLevel();}
    if(now-lastWifiTelemetryMs>=500UL){lastWifiTelemetryMs=now;core.wifiRssi=WiFi.status()==WL_CONNECTED?WiFi.RSSI():-127;}
    core.freeHeap=ESP.getFreeHeap();
''','telemetry cadence')

s=rep(s,
'''  serviceWitnessQueue(runtimeGovernor.witnessFlushQuota());
  serviceRadioCache();
''',
'''  serviceWitnessQueue(runtimeGovernor.witnessFlushQuota());
  serviceRadioCache();
  serviceDeferredPersistence();
''','P5 persistence')

ino.write_text(s,encoding='utf-8')
alien.write_text(a,encoding='utf-8')
wifi.write_text(w,encoding='utf-8')
print('RC3 ZIM pass2 applied')
