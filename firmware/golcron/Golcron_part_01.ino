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

struct __attribute__((packed)) HiveMetricPacket {
  uint8_t magic[2]; uint8_t version; uint16_t worker_id; char nodeId[24]; char kind[16];
  uint32_t seq; uint32_t uptime_ms; uint32_t free_heap; uint32_t min_free_heap; uint16_t cpu_mhz;
  uint16_t loop_jitter_us; uint16_t loop_max_us; int8_t rssi; uint8_t bt_flags; uint8_t volume; uint8_t palette;
  uint16_t touch_count; uint16_t effective_batch; uint32_t hash_rate; uint32_t total_hashes; uint32_t shares;
  uint32_t rejects; uint16_t best_bits; uint32_t job_age_ms; uint32_t nonce_remaining; uint8_t reward_level;
  uint8_t ai_hint; uint16_t target_batch; int16_t prediction_error_x1000; uint16_t entropy_x1000; uint16_t random_tail; uint16_t reserved;
};
struct __attribute__((packed)) JanusAgentRewardPacket {
  uint8_t magic[2]; uint8_t version; char source[16]; char targetNode[24]; uint32_t seq;
  uint8_t rewardLevel; uint8_t aiHint; uint16_t rewardPoints; uint16_t targetBatch; uint32_t entropySeed;
  float score; float predictedHashRate; float predictionError; uint32_t deltaShares; uint32_t uptime_ms;
};
enum RxKind : uint8_t { RX_NONE = 0, RX_JOB, RX_REWARD };
struct RxItem { RxKind kind = RX_NONE; int8_t rssi = -127; union { JobPacket job; JanusAgentRewardPacket reward; } body; };
struct StarRef { const char *name; uint16_t ra; int16_t dec; uint8_t mag10; uint8_t house; };
static const StarRef STAR_MAP[] = {
  {"SIRIUS",1012,-167,14,1},{"CANOPUS",957,-526,7,2},{"ARCTURUS",2139,191,0,3},{"VEGA",2792,388,0,4},
  {"CAPELLA",792,459,1,5},{"RIGEL",789,-82,1,6},{"PROCYON",1148,52,4,7},{"BETELGEUSE",887,74,5,8},
  {"ALTAIR",2977,89,8,9},{"ALDEBARAN",690,163,9,10},{"ANTARES",2473,-263,10,11},{"SPICA",2013,-111,10,12},
  {"POLLUX",1161,280,11,13},{"FOMALHAUT",3444,-296,12,14},{"DENEB",3104,450,13,15},{"REGULUS",1527,120,14,16},
};
static const uint8_t STAR_COUNT = sizeof(STAR_MAP) / sizeof(STAR_MAP[0]);
struct AstrolabeJob {
  bool active=false; uint8_t jobId[8]={0}; uint8_t header[80]={0}; uint8_t target[32]={0};
  uint32_t startNonce=0, rangeSize=0, cursor=0, offset=0, stride=1, rxMs=0, seq=0; uint8_t starIndex=0;
  uint32_t sliceSpan=0, sliceCount=0, sliceOrdinal=0, sliceIndex=0, sliceStart=0, sliceSize=0, sliceCursor=0;
  uint32_t sliceOrderOffset=0, sliceOrderStride=1, replanSalt=0; bool forceReplan=false;
};
struct StarForgeLane {
  uint32_t seed=0, hashes=0, bitSum=0, strongHits=0, shares=0;
  uint16_t emaBitsX256=256, score=0, bestBits=0, selections=0; uint8_t starIndex=0;
};
struct StarForgeState {
  bool online=true; uint8_t lane=0, previousLane=0; uint16_t energy=0, heat=0, lastScore=0, lastBest=0;
  uint32_t seed=0, reforges=0, autoPaths=0, manualPlans=0, fires=0, lastMs=0;
};
static AstrolabeJob job;
static StarForgeState forge;
static StarForgeLane forgeLanes[STAR_FORGE_LANES];
static uint16_t workerIdCache=0; static uint32_t seqNo=0, jobSeq=0, lastStatusMs=0, lastDisplayMs=0, lastJediLedMs=0, lastRateMs=0;
static uint32_t totalHashes=0, windowHashes=0, hashRate=0, sharesSent=0, rejectsLocal=0, jobsRx=0, discoveryRx=0, bestBits=0, bestNonce=0;
static uint16_t targetBitsNow=22, batchSize=180; static uint8_t agentHint=1; static int8_t lastRssi=-127;
static bool buzzSeen=false; static uint32_t lastBuzzMs=0, txOk=0, txFail=0; static uint8_t bestTail[4]={0};
static char statusLine[48]="WAIT BUZZ"; static volatile uint8_t rxHead=0, rxTail=0; static volatile uint32_t rxDrops=0;
static RxItem rxQueue[RX_QUEUE_DEPTH]; static uint8_t activeChannel=JANUS_SWARM_CHANNEL, scanIndex=0;
static uint32_t lastScanMs=0,lastHiveMs=0,jobDoneCount=0,jobStaleCount=0,shareRejectSelf=0,duplicateJobRx=0,badJobRx=0;
static uint32_t jediShareFlashUntilMs=0,jediJobPulseUntilMs=0,jediProbeUntilMs=0; static uint8_t jediProbeIndex=0;
static portMUX_TYPE rxMux = portMUX_INITIALIZER_UNLOCKED;

static void ensureBroadcastPeer(); static void setRadioChannel(uint8_t ch,const char *reason); static void starForgeReforgeCurrent(const char *reason);
static bool enqueueRx(const RxItem &item); static bool dequeueRx(RxItem *item); static void applyAgentReward(const JanusAgentRewardPacket &ar);
static bool sameJobAssignment(const JobPacket &jp);
static const StarRef &currentStar(); static uint32_t jobSeed(const JobPacket &jp);
static uint16_t starForgeAdaptiveScore(uint8_t laneIndex,uint32_t contextSeed,bool forceDifferent);
static uint8_t starForgeChooseLane(uint32_t contextSeed,bool forceDifferent);
static bool starForgePrepareNextSlice(const char *reason); static bool starForgeAdvanceSlice(const char *reason);
static void starForgeLearn(uint16_t bits,bool accepted); static void configureAstrolabePath(const JobPacket &jp); static void onJobPacket(const JobPacket &jp);
static void saveUiState(); static void loadUiState(); static void uiOverlay(const char *line,uint32_t ttl=1600UL); static void updateVisualState(uint32_t now);

static uint32_t mix32(uint32_t x){x^=x>>16;x*=0x7FEB352DUL;x^=x>>15;x*=0x846CA68BUL;x^=x>>16;return x;}
static void writeLE32(uint8_t *p,uint32_t v){p[0]=v&0xFF;p[1]=(v>>8)&0xFF;p[2]=(v>>16)&0xFF;p[3]=(v>>24)&0xFF;}
static void hashToShareOrder(const uint8_t raw[32],uint8_t out[32]){for(int i=0;i<32;++i)out[i]=raw[31-i];}
static uint16_t countLeadingZeroBits(const uint8_t h[32]){uint16_t bits=0;for(int i=0;i<32;++i){uint8_t b=h[i];if(b==0){bits+=8;continue;}for(int k=7;k>=0;--k){if((b&(1<<k))==0)bits++;else return bits;}}return bits;}
static bool hashMeetsTargetBytes(const uint8_t hash[32],const uint8_t target[32]){for(int i=0;i<32;++i){if(hash[i]<target[i])return true;if(hash[i]>target[i])return false;}return true;}
static uint16_t workerId(){if(workerIdCache)return workerIdCache;uint8_t mac[6]={0};WiFi.macAddress(mac);uint16_t id=0x4700;for(uint8_t b:mac)id=(uint16_t)((id*33U)^b);if(!id)id=0x4711;workerIdCache=id;return id;}
static void uiOverlay(const char *line,uint32_t ttl){if(!line||!line[0])return;strlcpy(overlayLine,line,sizeof(overlayLine));overlayUntilMs=millis()+ttl;}
static void saveUiState(){Preferences prefs;if(!prefs.begin("gol_ui_v13",false))return;prefs.putUChar("view",charmView);prefs.putBool("frame",frameVisible);prefs.putUChar("tft",tftProfileIndex);prefs.end();}
static void loadUiState(){Preferences prefs;if(!prefs.begin("gol_ui_v13",true))return;charmView=(uint8_t)constrain((int)prefs.getUChar("view",0),0,2);frameVisible=prefs.getBool("frame",true);tftProfileIndex=(uint8_t)constrain((int)prefs.getUChar("tft",0),0,(int)(TFT_PROFILE_COUNT?TFT_PROFILE_COUNT-1:0));prefs.end();}
static void updateVisualState(uint32_t now){
  if(!visualTickMs){visualTickMs=now;smoothForgeEnergy=forge.energy;smoothForgeHeat=forge.heat;smoothHashRateK=(float)hashRate/1000.0f;visualLastJobSeq=job.seq;visualLastBestBits=bestBits;return;}
  uint32_t dtMs=now-visualTickMs;if(!dtMs)return;visualTickMs=now;float dt=(float)dtMs/1000.0f;dt=constrain(dt,0.001f,0.080f);float a=1.0f-expf(-dt*5.2f);
  if(job.seq!=visualLastJobSeq){visualLastJobSeq=job.seq;if(cosmosPulse<0.82f)cosmosPulse=0.82f;}
  if(bestBits>visualLastBestBits){visualLastBestBits=bestBits;if(cosmosPulse<1.10f)cosmosPulse=1.10f;}
  if(now<jediShareFlashUntilMs&&cosmosPulse<1.32f)cosmosPulse=1.32f;
  visualPhase+=dt*(0.16f+(job.active?0.24f:0.06f));orbitPhase+=dt*(0.48f+forge.energy/1000.0f*0.92f+(job.active?0.20f:0));dustPhase+=dt*(0.10f+forge.heat/1000.0f*0.52f+(job.active?0.05f:0));
  if(visualPhase>1024)visualPhase-=1024;if(orbitPhase>1024)orbitPhase-=1024;if(dustPhase>1024)dustPhase-=1024;
  smoothForgeEnergy+=((float)forge.energy-smoothForgeEnergy)*a;smoothForgeHeat+=((float)forge.heat-smoothForgeHeat)*a;
  float hashA=min(1.0f,dt*2.5f);smoothHashRateK+=((float)hashRate/1000.0f-smoothHashRateK)*hashA;cosmosPulse*=expf(-dt*1.25f);if(cosmosPulse<0.002f)cosmosPulse=0;
}
static uint32_t gcd32(uint32_t a,uint32_t b){while(b){uint32_t t=a%b;a=b;b=t;}return a?a:1;}
static bool enqueueRx(const RxItem &item){bool ok=false;portENTER_CRITICAL_ISR(&rxMux);uint8_t next=(rxHead+1)%RX_QUEUE_DEPTH;if(next!=rxTail){rxQueue[rxHead]=item;rxHead=next;ok=true;}else rxDrops++;portEXIT_CRITICAL_ISR(&rxMux);return ok;}
static bool dequeueRx(RxItem *item){bool ok=false;portENTER_CRITICAL(&rxMux);if(rxTail!=rxHead){*item=rxQueue[rxTail];rxTail=(rxTail+1)%RX_QUEUE_DEPTH;ok=true;}portEXIT_CRITICAL(&rxMux);return ok;}
static bool targetMatchesMe(const char *targetNode){if(!targetNode||!targetNode[0])return false;return !strncmp(targetNode,GOLCRON_NODE_ID,24)||!strncmp(targetNode,"Golcron",24)||!strncmp(targetNode,"Holocron",24)||!strncmp(targetNode,"Astrolabe",24)||!strncmp(targetNode,"all",24)||!strncmp(targetNode,"*",24);}
static void applyAgentReward(const JanusAgentRewardPacket &ar){if(!targetMatchesMe(ar.targetNode))return;if(ar.targetBatch>=GOLCRON_MIN_BATCH&&ar.targetBatch<=GOLCRON_MAX_BATCH)batchSize=ar.targetBatch;if(ar.aiHint)agentHint=ar.aiHint;if(ar.entropySeed&&job.rangeSize){job.replanSalt^=mix32(ar.entropySeed^millis()^job.cursor);job.forceReplan=true;snprintf(statusLine,sizeof(statusLine),"NEXT PATH SEEDED");}}
static const StarRef &currentStar(){return STAR_MAP[job.starIndex%STAR_COUNT];}
static uint32_t jobPathNumber(){if(!job.sliceCount)return 0;uint32_t n=job.sliceOrdinal;if(job.cursor<job.rangeSize)n++;return min<uint32_t>(n,job.sliceCount);}
static uint32_t jobSeed(const JobPacket &jp){uint32_t s=jp.start_nonce^mix32(jp.range_size);for(int i=0;i<8;++i)s=mix32(s^((uint32_t)jp.job_id[i]<<((i&3)*8)));return s;}
static uint32_t makeCoprimeStride(uint32_t raw,uint32_t rangeSize){if(rangeSize<=1)return 1;raw%=rangeSize;if(raw==0)raw=1;while(gcd32(raw,rangeSize)!=1){raw++;if(raw>=rangeSize)raw=1;}return raw;}
static bool targetHasWork(const uint8_t target[32]){for(uint8_t i=0;i<32;++i)if(target[i]!=0)return true;return false;}
static bool sameJobAssignment(const JobPacket &jp){return job.rangeSize==jp.range_size&&job.startNonce==jp.start_nonce&&memcmp(job.jobId,jp.job_id,sizeof(job.jobId))==0&&memcmp(job.header,jp.header,sizeof(job.header))==0&&memcmp(job.target,jp.target,sizeof(job.target))==0;}
static uint32_t starForgeSliceSpan(uint32_t rangeSize){if(rangeSize<=STAR_FORGE_SLICE_SMALL)return max<uint32_t>(1,rangeSize);if(rangeSize<=32768)return STAR_FORGE_SLICE_SMALL;if(rangeSize<=262144)return STAR_FORGE_SLICE_NORMAL;if(rangeSize<=1048576)return STAR_FORGE_SLICE_LARGE;return STAR_FORGE_SLICE_HUGE;}
static uint16_t starForgeAdaptiveScore(uint8_t laneIndex,uint32_t contextSeed,bool forceDifferent){
  StarForgeLane &lane=forgeLanes[laneIndex%STAR_FORGE_LANES];uint32_t jitter=(mix32(contextSeed^lane.seed^((uint32_t)laneIndex*0x9E3779B9UL))>>24)&0x7F;
  if(lane.selections==0){uint32_t virgin=60000UL+jitter-(uint32_t)laneIndex*17UL;if(forceDifferent&&laneIndex==forge.lane&&STAR_FORGE_LANES>1)virgin/=8;return min<uint32_t>(65535,virgin);}
  uint32_t meanX256=lane.hashes?(uint32_t)(((uint64_t)lane.bitSum*256ULL)/lane.hashes):256;
  uint32_t strongRateX4096=lane.hashes?(uint32_t)(((uint64_t)lane.strongHits*4096ULL)/lane.hashes):0;
  uint32_t exploration=2200UL/(lane.selections+1U);
  uint32_t score=(uint32_t)lane.emaBitsX256*3UL+meanX256*2UL+(uint32_t)lane.bestBits*176UL+
