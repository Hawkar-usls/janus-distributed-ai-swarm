#!/usr/bin/env python3
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'firmware'/'adv_elite'/'ADV_Elite.ino'
s=p.read_text(encoding='utf-8')
if '// RC3_PHYSICAL_BOOTSAFE' in s:
    print('bootsafe already applied'); raise SystemExit(0)

def rep(old,new,label):
    global s
    if old not in s: raise RuntimeError(f'{label}: marker not found')
    s=s.replace(old,new,1)

rep('// RC3_ZIM_PASS2\n','// RC3_ZIM_PASS2\n// RC3_PHYSICAL_BOOTSAFE\n','marker')
rep('static constexpr uint8_t LORA_PWR_EN = 10;\n','// Cardputer ADV GPIO10 is BATTERY_ADC. Never drive it as LoRa power.\n','remove fake lora power')
rep('bool loraReady = false;\n','bool loraReady = false;\nbool loraInitAttempted = false;\nbool envReady = false;\nbool ledReady = false;\nbool speakerReady = false;\n','boot readiness')
rep('void playUiTone(uint16_t freq,uint16_t ms){\n  if(hardMute||masterVolume==0)return;',
    'void playUiTone(uint16_t freq,uint16_t ms){\n  if(!speakerReady||hardMute||masterVolume==0)return;','tone gate')
rep('void updateLed(){\n  const uint32_t now=millis();',
    'void updateLed(){\n  if(!ledReady)return;\n  const uint32_t now=millis();','led gate')
rep('void serviceEnv(){\n  const uint32_t now=millis();',
    'void serviceEnv(){\n  if(!envReady)return;\n  const uint32_t now=millis();','env gate')

old='''void initGnssAndLoRa(){
  Serial2.begin(GNSS_BAUD,SERIAL_8N1,GNSS_RX_PIN,GNSS_TX_PIN);
#if ADV_HAS_RADIOLIB
  pinMode(LORA_PWR_EN,OUTPUT);digitalWrite(LORA_PWR_EN,HIGH);delay(80);int st=advRadio.begin(868.0,125.0,9,7,0x34,10,8,1.6,false);loraReady=(st==RADIOLIB_ERR_NONE);if(loraReady){advRadio.setOutputPower(10);advRadio.setCRC(true);advRadio.setDio1Action(onLoraTxDone);}
#else
  loraReady=false;
#endif
}
'''
new='''void initGnssOnly(){
  Serial2.begin(GNSS_BAUD,SERIAL_8N1,GNSS_RX_PIN,GNSS_TX_PIN);
}
bool ensureLoRaReady(){
  if(loraReady)return true;
  if(loraInitAttempted)return false;
  loraInitAttempted=true;
#if ADV_HAS_RADIOLIB
  // ADV EXT module shares SCK40/MOSI14/MISO39 and uses CS5/IRQ4/RST3/BUSY6.
  // There is NO GPIO power-enable: GPIO10 is the battery ADC and must stay untouched.
  int st=advRadio.begin(868.0,125.0,9,7,0x34,10,8,1.6,false);
  loraReady=(st==RADIOLIB_ERR_NONE);
  if(loraReady){advRadio.setOutputPower(10);advRadio.setCRC(true);advRadio.setDio1Action(onLoraTxDone);}
#else
  loraReady=false;
#endif
  return loraReady;
}
'''
rep(old,new,'lazy lora')

old='''  if(rising(nJ,prevKey.j)){loraActive=!loraActive;statusLine=loraActive?(loraReady?"LORA ON":"LORA REQUEST/HW FAIL"):"LORA OFF";witness("LORA_GATE",loraActive?"on":"off");}
'''
new='''  if(rising(nJ,prevKey.j)){
    if(!loraActive){loraActive=ensureLoRaReady();statusLine=loraActive?"LORA ON":"LORA HW FAIL";}
    else {loraActive=false;statusLine="LORA OFF";}
    witness("LORA_GATE",loraActive?"on":"off");
  }
'''
rep(old,new,'J lazy lora')

# Replace monolithic setup with visible staged SAFEBOOT derived from the known-good Beacon sequence.
start=s.index('void setup(){')
end=s.index('\n\nvoid loop(){',start)
setup=r'''void bootStage(const char* text,uint16_t color=TFT_DARKGREY){
  Serial.printf("[ADV-BOOT] %s\n",text);
  M5Cardputer.Display.setTextColor(color,TFT_BLACK);
  M5Cardputer.Display.println(text);
}

void setup(){
  // SAFEBOOT rule: serial and a visible LCD marker exist before any optional organ.
  Serial.begin(115200);
  delay(120);
  Serial.println();
  Serial.println("[ADV-BOOT] RC3 PHYSICAL SAFEBOOT");

  auto cfg=M5.config();
  cfg.internal_mic=false; // microphone is not an ADV_Elite truth source; shorten early audio init.
  M5Cardputer.begin(cfg,false); // M5/display only; keyboard comes after the first visible marker.
  M5Cardputer.Display.setRotation(1);
  M5Cardputer.Display.setTextSize(1);
  M5Cardputer.Display.setBrightness(90);
  M5Cardputer.Display.fillScreen(TFT_BLACK);
  M5Cardputer.Display.setCursor(4,4);
  M5Cardputer.Display.setTextColor(TFT_CYAN,TFT_BLACK);
  M5Cardputer.Display.println("JANUS ADV RC3 SAFEBOOT");
  M5Cardputer.Display.setTextColor(TFT_DARKGREY,TFT_BLACK);
  M5Cardputer.Display.printf("BOARD %d\n",(int)M5.getBoard());
  bootStage("S0 DISPLAY OK",TFT_GREEN);

  // Second begin is intentional: M5.begin() is already complete, while M5Cardputer
  // enables its keyboard reader now that a boot marker exists. ADV selects TCA8418.
  M5Cardputer.begin(cfg,true);
  bootStage("S1 KEYBOARD OK",TFT_GREEN);

  bool fsOk=LittleFS.begin(true);
  bootStage(fsOk?"S2 LITTLEFS OK":"S2 LITTLEFS FAIL",fsOk?TFT_GREEN:TFT_YELLOW);

  loadUiSettings();
  loadPet();
  alien.begin();
  bool homeBuf=beaconHome.begin();
  runtimeGovernor.begin(millis());
  bootStage(homeBuf?"S3 UI BUFFER OK":"S3 UI BUFFER FAIL",homeBuf?TFT_GREEN:TFT_YELLOW);

  // External Grove I2C only (G2/G1). ADV internal keyboard/audio/IMU I2C is G8/G9.
  Wire.begin(GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);
  Wire.setClock(400000U);
  bool shtOk=shtAsync.begin(&Wire,0x44);
  bool qmpOk=advQmp.begin(&Wire,QMP6988_SLAVE_ADDRESS_L,GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);
  if(!qmpOk)qmpOk=advQmp.begin(&Wire,QMP6988_SLAVE_ADDRESS_H,GROVE_SDA_PIN,GROVE_SCL_PIN,400000U);
  envReady=shtOk||qmpOk;
  Serial.printf("[ADV-BOOT] ENV SHT=%d QMP=%d\n",shtOk?1:0,qmpOk?1:0);
  bootStage(envReady?"S4 ENV READY":"S4 ENV OPTIONAL",envReady?TFT_GREEN:TFT_YELLOW);

  // M5Unified already initialized the ADV BMI270 during M5.begin(). Do not init it twice.
  core.imuValid=M5.Imu.isEnabled();
  if(core.imuValid)calibrateImuZero();
  bootStage(core.imuValid?"S5 IMU OK":"S5 IMU OPTIONAL",core.imuValid?TFT_GREEN:TFT_YELLOW);

  // SD is optional and deliberately slow-clocked like the historical ADV SAFEBOOT.
  SPI.begin(SD_SCK_PIN,SD_MISO_PIN,SD_MOSI_PIN,SD_CS_PIN);
  bool sdOk=SD.begin(SD_CS_PIN,SPI,10000000UL);
  Serial.printf("[ADV-BOOT] SD=%d\n",sdOk?1:0);
  bootStage(sdOk?"S6 SD OK":"S6 SD OPTIONAL",sdOk?TFT_GREEN:TFT_YELLOW);

  // ADV RGB rail is GPIO38; LED data is GPIO21. Display is already alive before touching it.
  pinMode(38,OUTPUT);digitalWrite(38,HIGH);delay(2);
  FastLED.addLeds<WS2812,LED_PIN,GRB>(advLed,1);
  ledReady=true;
  applyIllumination();

  M5Cardputer.Speaker.begin();
  speakerReady=true;
  applyVolume();
  bootStage("S7 AUDIO/LED OK",TFT_GREEN);

  String factorySsid,factoryPass;
  bool factoryCred=loadFactoryPrimaryCredential(factorySsid,factoryPass);
  advWifi.begin(factoryCred?factorySsid.c_str():ADV_WIFI_SSID,factoryCred?factoryPass.c_str():ADV_WIFI_PASSWORD);
  buzzWorkerId=(uint16_t)(ESP.getEfuseMac()&0xFFFF);
  bool enow=initEspNow();
  Serial.printf("[ADV-BOOT] ESPNOW=%d\n",enow?1:0);
  bootStage(enow?"S8 SWARM OK":"S8 SWARM OPTIONAL",enow?TFT_GREEN:TFT_YELLOW);

  // GNSS UART is harmless at boot; LoRa SPI probing is lazy and happens only on J.
  initGnssOnly();
  if(sdOk)radioLoadCache();

  core.predEntropy=core.entropy;
  core.battery=M5.Power.getBatteryLevel();
  core.freeHeap=ESP.getFreeHeap();
  lastUserInputMs=millis();
  statusLine="RC3 SAFE READY";
  if(fsOk)witness("BOOT","ADV_Elite_RC3_SAFEBOOT","CONTROL_STATE");

  delay(120);
  drawHome(); // first canonical Beacon frame replaces the diagnostic boot page.
}
'''
s=s[:start]+setup+s[end:]

p.write_text(s,encoding='utf-8')
print('RC3 physical SAFEBOOT repair applied')
