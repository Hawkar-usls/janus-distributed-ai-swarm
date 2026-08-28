from pathlib import Path
import re

p = Path(__file__).with_name("ADV_Elite.ino")
s = p.read_text(encoding="utf-8")
original = s


def require_replace(old: str, new: str, label: str):
    global s
    if new in s:
        return
    if old not in s:
        raise SystemExit(f"RC3 PATCH MISS: {label}")
    s = s.replace(old, new, 1)


def regex_replace(pattern: str, replacement: str, label: str):
    global s
    if re.search(pattern, s, flags=re.S) is None:
        raise SystemExit(f"RC3 PATCH MISS: {label}")
    s = re.sub(pattern, replacement, s, count=1, flags=re.S)


# ---- compile blocker ----
s = s.replace("comstrain(", "constrain(")

# ---- modules ----
require_replace(
    '#include "ADV_Elite_primary_credential.h"',
    '#include "ADV_Elite_primary_credential.h"\n#include "ADV_Elite_beacon_home.h"\n#include "ADV_Elite_sht3x_async.h"',
    "headers",
)
require_replace(
    'AdvWifiManager advWifi;\nPreferences prefs;',
    'AdvWifiManager advWifi;\nBeaconHomeRenderer beaconHome;\nAdvSht3xAsync shtAsync;\nPreferences prefs;',
    "globals",
)

# ---- smooth frame pacing ----
s = s.replace(
    'static constexpr uint32_t DRAW_INTERVAL_MS = 45UL;',
    'static constexpr uint32_t DRAW_INTERVAL_MS = 16UL;  // ~60 Hz presentation cadence',
)
require_replace(
    'uint32_t lastEnvMs = 0;',
    'uint32_t lastEnvMs = 0;\nuint32_t lastShtFreshMs = 0;\nuint32_t lastQmpFreshMs = 0;',
    "env-freshness",
)

# ---- nonblocking ENV III ----
new_env = r'''void serviceEnv(){
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

void calibrateImuZero'''
regex_replace(
    r'void readEnv\(\)\{.*?\n\}\n\nvoid calibrateImuZero',
    new_env,
    "service-env",
)

# ---- historical Beacon HOME: frame, 3 columns, ticker ----
new_home = r'''void drawTicker(){}
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
float histValue'''
regex_replace(
    r'void drawTicker\(\)\{.*?\n\}\nfloat histValue',
    new_home,
    "beacon-home",
)

# ---- official ENV III address ----
old_setup = (
    'Wire.begin(GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);'
    'bool shtOk=advSht.begin(&Wire,0x44,GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);'
    'bool qmpOk=advQmp.begin(&Wire,0x56,GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);'
    'Serial.printf("[ADV] ENV SHT=%d QMP=%d\\n",shtOk?1:0,qmpOk?1:0);'
)
new_setup = (
    'Wire.begin(GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);'
    'bool shtOk=shtAsync.begin(&Wire,0x44);'
    'bool qmpOk=advQmp.begin(&Wire,QMP6988_SLAVE_ADDRESS_L,GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);'
    'if(!qmpOk)qmpOk=advQmp.begin(&Wire,QMP6988_SLAVE_ADDRESS_H,GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);'
    'Serial.printf("[ADV] ENV SHT=%d QMP=%d\\n",shtOk?1:0,qmpOk?1:0);'
)
require_replace(old_setup,new_setup,"env3-setup")

# Allocate the sprite once at boot, never once-per-frame.
require_replace(
    'loadPet();alien.begin();String factorySsid,factoryPass;',
    'loadPet();alien.begin();beaconHome.begin();String factorySsid,factoryPass;',
    "beacon-begin",
)

# ENV service must be called on every loop iteration so the delayed SHT read is
# collected when ready, rather than sleeping inside a one-second callback.
require_replace(
    'if(now-lastEnvMs>=ENV_INTERVAL_MS){lastEnvMs=now;readEnv();}',
    'serviceEnv();',
    "env-loop",
)

p.write_text(s, encoding="utf-8")
print("RC3 patch:", "changed" if s != original else "already current")
