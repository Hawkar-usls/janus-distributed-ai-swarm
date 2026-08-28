#!/usr/bin/env python3
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'firmware'/'adv_elite'/'ADV_Elite.ino'
s=p.read_text(encoding='utf-8')
if '// RC3_ASYNC_RADIO_START' in s:
    print('async radio already applied');raise SystemExit(0)

old='''AudioFileSourceICYStream* radioFile = nullptr;
AudioFileSourceBuffer* radioBuffer = nullptr;
AudioGeneratorMP3* radioMp3 = nullptr;
AdvAudioOutputM5Speaker* radioOut = nullptr;
#endif
'''
new='''AudioFileSourceICYStream* radioFile = nullptr;
AudioFileSourceBuffer* radioBuffer = nullptr;
AudioGeneratorMP3* radioMp3 = nullptr;
AdvAudioOutputM5Speaker* radioOut = nullptr;

// RC3_ASYNC_RADIO_START: URL open/HTTP GET belongs to Core0, never foreground loop.
struct RadioStartRequest { uint32_t generation=0; char url[180] = {}; };
portMUX_TYPE radioStartMux = portMUX_INITIALIZER_UNLOCKED;
RadioStartRequest radioStartRequest;
TaskHandle_t radioStartTaskHandle = nullptr;
bool radioStartPending = false;
bool radioStartReady = false;
bool radioStartStageOk = false;
uint32_t radioStartGeneration = 1;
uint32_t radioStartStageGeneration = 0;
uint32_t radioNextStartMs = 0;
AudioFileSourceICYStream* radioStageFile = nullptr;
AudioFileSourceBuffer* radioStageBuffer = nullptr;
AudioGeneratorMP3* radioStageMp3 = nullptr;
AdvAudioOutputM5Speaker* radioStageOut = nullptr;
#endif
'''
if old not in s: raise RuntimeError('radio globals marker not found')
s=s.replace(old,new,1)

start=s.index('#if ADV_HAS_WEBRADIO\nvoid radioStopEngine(){',s.index('void serviceRadioCache(){'))
end=s.index('void radioVoteLocal(int delta)',start)
block=r'''#if ADV_HAS_WEBRADIO
void radioDestroyObjects(AudioFileSourceICYStream*& f,AudioFileSourceBuffer*& b,AudioGeneratorMP3*& m,AdvAudioOutputM5Speaker*& o){
  if(m){m->stop();delete m;m=nullptr;}
  if(b){b->close();delete b;b=nullptr;}
  if(f){f->close();delete f;f=nullptr;}
  if(o){o->stop();delete o;o=nullptr;}
}

void radioStopEngine(){
  radioDestroyObjects(radioFile,radioBuffer,radioMp3,radioOut);
  radioState.engineRunning=false;
}

void radioCancelStart(){
  portENTER_CRITICAL(&radioStartMux);
  ++radioStartGeneration; // worker may finish later, but can no longer be adopted
  portEXIT_CRITICAL(&radioStartMux);
}

void radioStartWorker(void*){
  RadioStartRequest req;
  portENTER_CRITICAL(&radioStartMux);req=radioStartRequest;portEXIT_CRITICAL(&radioStartMux);

  AudioFileSourceICYStream* f=nullptr;
  AudioFileSourceBuffer* b=nullptr;
  AudioGeneratorMP3* m=nullptr;
  AdvAudioOutputM5Speaker* o=nullptr;
  bool ok=false;

  f=new AudioFileSourceICYStream(req.url); // HTTP GET may block: intentionally Core0
  if(f&&f->isOpen()){
    f->SetReconnect(3,250);
    b=new AudioFileSourceBuffer(f,8192);
    o=new AdvAudioOutputM5Speaker(&M5Cardputer.Speaker,1);
    m=new AudioGeneratorMP3();
    if(b&&o&&m)ok=m->begin(b,o);
  }

  bool adopt=false;
  portENTER_CRITICAL(&radioStartMux);
  if(ok&&req.generation==radioStartGeneration){
    radioStageFile=f;radioStageBuffer=b;radioStageMp3=m;radioStageOut=o;
    radioStartStageGeneration=req.generation;radioStartStageOk=true;radioStartReady=true;adopt=true;
  }
  radioStartPending=false;radioStartTaskHandle=nullptr;
  portEXIT_CRITICAL(&radioStartMux);

  if(!adopt)radioDestroyObjects(f,b,m,o);
  vTaskDelete(nullptr);
}

bool radioRequestStart(){
  if(hardMute||WiFi.status()!=WL_CONNECTED||radioState.count==0)return false;
  if((int32_t)(millis()-radioNextStartMs)<0)return false;
  if(ESP.getFreeHeap()<90000UL){statusLine="RADIO LOW HEAP";radioNextStartMs=millis()+1500UL;return false;}

  portENTER_CRITICAL(&radioStartMux);
  if(radioStartPending||radioStartReady){portEXIT_CRITICAL(&radioStartMux);return false;}
  const uint32_t gen=++radioStartGeneration;
  radioStartRequest.generation=gen;
  strlcpy(radioStartRequest.url,radioState.stations[radioState.index].url,sizeof(radioStartRequest.url));
  radioStartPending=true;
  portEXIT_CRITICAL(&radioStartMux);

  BaseType_t rc=xTaskCreatePinnedToCore(radioStartWorker,"adv_radio_open",8192,nullptr,1,&radioStartTaskHandle,0);
  if(rc!=pdPASS){
    portENTER_CRITICAL(&radioStartMux);radioStartPending=false;++radioStartGeneration;portEXIT_CRITICAL(&radioStartMux);
    statusLine="RADIO TASK FAIL";radioNextStartMs=millis()+1500UL;return false;
  }
  statusLine="RADIO CONNECT";
  return true;
}

void radioStartService(){
  AudioFileSourceICYStream* f=nullptr;AudioFileSourceBuffer* b=nullptr;AudioGeneratorMP3* m=nullptr;AdvAudioOutputM5Speaker* o=nullptr;
  uint32_t gen=0;bool ready=false,ok=false;
  portENTER_CRITICAL(&radioStartMux);
  if(radioStartReady){
    ready=true;ok=radioStartStageOk;gen=radioStartStageGeneration;
    f=radioStageFile;b=radioStageBuffer;m=radioStageMp3;o=radioStageOut;
    radioStageFile=nullptr;radioStageBuffer=nullptr;radioStageMp3=nullptr;radioStageOut=nullptr;
    radioStartReady=false;radioStartStageOk=false;
  }
  portEXIT_CRITICAL(&radioStartMux);
  if(!ready)return;

  if(!ok||gen!=radioStartGeneration||!radioState.desiredPlaying||hardMute||WiFi.status()!=WL_CONNECTED){
    radioDestroyObjects(f,b,m,o);return;
  }
  radioStopEngine();
  radioFile=f;radioBuffer=b;radioMp3=m;radioOut=o;radioState.engineRunning=true;radioNextStartMs=0;statusLine="RADIO PLAY";
}

void radioTick(){
  radioStartService();
  if(!radioState.desiredPlaying||hardMute){if(radioState.engineRunning)radioStopEngine();return;}
  if(!radioState.engineRunning){radioRequestStart();return;}
  if(!radioMp3->loop()){++radioState.failures;radioStopEngine();radioNextStartMs=millis()+1500UL;}
}
#else
void radioStopEngine(){radioState.engineRunning=false;}
void radioCancelStart(){}
bool radioRequestStart(){return false;}
void radioTick(){}
#endif
void radioSetPlaying(bool on){
  radioState.desiredPlaying=on;
  if(!on){radioCancelStart();radioStopEngine();}
  else if(!hardMute)radioRequestStart();
}
void radioStep(int dir){
  if(!radioState.count)return;
  radioState.index=(radioState.index+radioState.count+(dir<0?-1:1))%radioState.count;
  if(radioState.desiredPlaying){radioCancelStart();radioStopEngine();radioRequestStart();}
}
'''
s=s[:start]+block+s[end:]

# Do not start a catalogue while a radio-open worker is active/ready.
s=s.replace('if(radioState.catalogBusy||radioCatalogTaskHandle)return false;',
            'if(radioState.catalogBusy||radioCatalogTaskHandle)return false;\n#if ADV_HAS_WEBRADIO\n  if(radioStartPending||radioStartReady)return false;\n#endif',1)

p.write_text(s,encoding='utf-8')
print('RC3 async radio patch applied')
