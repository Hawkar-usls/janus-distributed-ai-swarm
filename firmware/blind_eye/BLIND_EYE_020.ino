struct __attribute__((packed)) JanusEventPacket {
  uint8_t magic[2];        // 'J','E'
  uint8_t version;         // 1
  uint8_t eventType;
  uint8_t nodeRole;
  uint8_t confidence;      // 0..100
  uint8_t urgency;         // 0..100
  char nodeId[24];
  char kind[16];
  uint32_t seq;
  uint32_t uptimeMs;
  uint16_t topicHash;
  uint16_t objectHash;
  uint16_t capabilities;
  int16_t valueA_x10;
  int16_t valueB_x10;
  int16_t valueC_x10;
  int16_t valueD_x10;
  uint32_t eventHash;
  uint32_t ttlMs;
};

struct __attribute__((packed)) JanusPolicyPacket {
  uint8_t magic[2];        // 'J','P'
  uint8_t version;         // 1
  uint8_t swarmMood;
  uint8_t radioRate;       // 0 low, 1 normal, 2 high
  uint8_t buzzBudget;      // 0 hold, 1 lazy, 2 normal, 3 boost
  uint8_t sensorRate;      // 0 low, 1 normal, 2 high
  uint8_t confidence;      // 0..100
  uint16_t flags;
  uint32_t seq;
  uint32_t ttlMs;
  uint32_t quietUntilMs;
  uint16_t dominantTopic;
  uint16_t danger_x100;
  char order[40];
};

struct JanusRemoteProphecyState {
  bool active = false;
  char nodeId[24] = "";
  uint32_t lastSeenMs = 0;
  uint32_t seq = 0;
  uint8_t sector = 0;
  uint8_t predictedSector = 0;
  uint8_t confidence = 0;
  uint8_t flags = 0;
  float presence_now = 0.0f;
  float motion_now = 0.0f;
  float pred_presence_1 = 0.0f;
  float pred_motion_1 = 0.0f;
  float pred_presence_2 = 0.0f;
  float pred_motion_2 = 0.0f;
  float pred_presence_3 = 0.0f;
  float pred_motion_3 = 0.0f;
  float future_stress = 0.0f;
  float swarm_pressure = 0.0f;
};

struct JanusKenshiNode {
  bool active = false;
  char nodeId[24] = "";
  uint32_t lastSeenMs = 0;
  uint32_t seq = 0;
  uint32_t worldFlags = 0;
  float entropy = 0.0f;
  float activity = 0.0f;
  float confidence = 0.0f;
  uint8_t sector = 0;
  uint8_t predictedSector = 0;
  uint8_t jobState = 0;
  uint8_t priority = 0;
  int8_t rssi = 0;
  uint8_t flags = 0;
};

struct JanusEyeEpisode {
  uint32_t atMs = 0;
  uint8_t eventType = JE_NONE;
  uint8_t confidence = 0;
  uint8_t urgency = 0;
  uint8_t sector = 0;
  uint8_t predictedSector = 0;
  uint8_t flags = 0;
  int16_t presence_x10 = 0;
  int16_t motion_x10 = 0;
  int16_t futureStress_x100 = 0;
  int16_t servoAngle_x10 = 0;
};

struct RemoteJobState {
  bool active = false;
  uint8_t job_id[8] = {};
  uint8_t header[80] = {};
  uint8_t target[32] = {};
  uint32_t startNonce = 0;
  uint32_t rangeSize = 0;
  uint32_t nonce = 0;
  uint32_t endNonce = 0;      // kept for debug; range wrap is handled by hashesDone
  uint32_t hashesDone = 0;
  uint32_t receivedAt = 0;
  // v2.12 scheduler-only lane metadata. Header/target bytes are still provided by Buzz.
  uint8_t minerLane = 0;
  uint8_t minerSector = 0;
  uint8_t minerStrideArm = 0;
  uint32_t minerSeed = 0;
  uint32_t minerStride = 1;
  uint32_t minerStartOffset = 0;
};

// Explicit prototypes for functions with custom Janus types.
// This prevents Arduino IDE auto-prototype generation from placing these
// signatures before the struct definitions and breaking compilation.
void colonyMinerConfigureForJob(RemoteJobState& job);
uint32_t colonyNextNonceV31(const RemoteJobState& job, uint32_t i);
bool looksLikeBuzzMaster(const JanusColonyPacket& pkt);
bool agentRewardTargetsThisEye(const JanusAgentRewardPacket& ar);
void onJanusAgentReward(const JanusAgentRewardPacket& ar);
void sendShareResponse(const RemoteJobState& job, uint32_t nonce);
void onJanusPolicyPacket(const JanusPolicyPacket& jp);
const JanusEyeEpisode* janusEyeLatestEpisode();
void onJanusKenshiPacket(const JanusKenshiPacket& kp, int8_t rxRssi);
void onJanusTachyonProphecy(const JanusTachyonProphecyPacket& tp, int8_t rxRssi);
bool eyeVisionTargetsThisEye(const JanusEyeVisionControlPacket& ec);
void onJanusEyeVisionControl(const JanusEyeVisionControlPacket& ec);

RemoteJobState colonyJob;
volatile bool colonyMasterSeen = false;
uint32_t colonyLastMasterMs = 0;
uint32_t colonySeq = 0;
uint32_t colonyLastHeartbeatMs = 0;
uint32_t colonyLastEntropyMs = 0;
uint32_t colonyRemoteHashrate = 0;
uint32_t colonyRemoteShares = 0;
uint32_t colonyRemoteRejects = 0;
uint16_t colonyTargetBits = 0;
uint32_t colonyJobsSeen = 0;
uint32_t colonyJobsDone = 0;
uint32_t colonyJobsExpired = 0;
uint32_t colonyBestBits = 0;
uint32_t colonyHashCounter = 0;
uint32_t colonyMinerLaneSwitches = 0;
uint32_t colonyMinerTailHits = 0;
uint32_t colonyMinerBestNonce = 0;
uint32_t colonyLastHashTickMs = 0;
uint16_t colonyWorkerId = 0;
uint8_t colonyPeerChannel = 0;
int8_t colonyLastRssi = 0;
uint32_t colonyPeerRebuilds = 0;
uint32_t colonyTxOk = 0;
uint32_t colonyTxFail = 0;
esp_err_t colonyLastTxErr = ESP_OK;
char colonyLastTxTag[16] = "-";
char colonyMode[12] = "SEEK";

// v2.5 Buzz Agent state.
uint32_t colonyAgentRewardsRx = 0;
uint32_t colonyAgentShareRewardsRx = 0;  // reward events caused by worker ticket/share delta
uint32_t colonyAgentSeq = 0;
uint8_t colonyAgentLevel = 0;
uint8_t colonyAgentHint = 1;
uint16_t colonyAgentRewardPoints = 0;
uint16_t colonyAgentTargetBatch = COLONY_REMOTE_BATCH;
uint32_t colonyAgentEntropySeed = 0xB11D3E5EUL;
uint32_t colonyAgentLastRewardMs = 0;
uint32_t colonyAgentLastDeltaShares = 0;
float colonyAgentScore = 0.0f;
float colonyAgentPredictedHash = 0.0f;
float colonyAgentPredictionError = 0.0f;
char colonyAgentSource[16] = "-";

// v2.7 Kenshi Bubble Bus state.
// "Active bubble" = hot sensor/model event. "Virtual" = quiet nodes remembered by timers.
JanusKenshiNode kenshiNodes[JANUS_KENSHI_MAX_NODES];
uint32_t kenshiSeq = 0;
uint32_t kenshiLastTxMs = 0;
uint32_t kenshiLastRxMs = 0;
uint32_t kenshiRxPackets = 0;
uint32_t kenshiTxPackets = 0;
uint32_t kenshiWorldFlags = 0;
uint8_t kenshiBubbleState = 0;    // 0 sleep, 1 virtual, 2 active, 3 alert
uint8_t kenshiJobState = 0;       // 0 idle, 1 watch, 2 track, 3 alert, 4 learn, 5 relay
uint8_t kenshiSector = 0;
uint8_t kenshiPredSector = 0;
uint8_t kenshiPriority = 0;
uint8_t kenshiActiveNodes = 0;
uint8_t kenshiVirtualNodes = 0;
float kenshiConfidence = 0.0f;
float kenshiVirtualEntropy = 0.0f;
float kenshiEventPower = 0.0f;
float kenshiMarkov[JANUS_KENSHI_SECTORS][JANUS_KENSHI_SECTORS] = {0};
uint8_t kenshiLastSector = 0;
bool kenshiStateDirty = false;


uint16_t countLeadingZeroBitsBE(const uint8_t h[32]) {
  uint16_t bits = 0;
  for (int i = 0; i < 32; i++) {
    uint8_t b = h[i];
    if (b == 0) { bits += 8; continue; }
    for (int k = 7; k >= 0; k--) {
      if ((b & (1 << k)) == 0) bits++;
      else return bits;
    }
  }
  return bits;
}

void writeLE32(uint8_t *p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

void doubleSha256(const uint8_t *data, size_t len, uint8_t out[32]) {
  uint8_t first[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, data, len);
  mbedtls_sha256_finish(&ctx, first);
  mbedtls_sha256_starts(&ctx, 0);
  mbedtls_sha256_update(&ctx, first, 32);
  mbedtls_sha256_finish(&ctx, out);
  mbedtls_sha256_free(&ctx);
}

bool hashMeetsTargetBE(const uint8_t hash[32], const uint8_t target[32]) {
  for (int i = 0; i < 32; i++) {
    if (hash[i] < target[i]) return true;
    if (hash[i] > target[i]) return false;
  }
  return true;
}

void hashToShareOrder(const uint8_t in[32], uint8_t out[32]) {
  // Buzz/NerdMiner-compatible worker gate:
  // raw SHA256d bytes -> reversed/display share order -> compare with big-endian target.
  for (int i = 0; i < 32; ++i) out[i] = in[31 - i];
}

uint32_t janusBitReverse32(uint32_t x) {
  x = ((x & 0x55555555UL) << 1) | ((x >> 1) & 0x55555555UL);
  x = ((x & 0x33333333UL) << 2) | ((x >> 2) & 0x33333333UL);
  x = ((x & 0x0F0F0F0FUL) << 4) | ((x >> 4) & 0x0F0F0F0FUL);
  x = ((x & 0x00FF00FFUL) << 8) | ((x >> 8) & 0x00FF00FFUL);
  x = (x << 16) | (x >> 16);
  return x;
}

uint32_t janusXorShift32(uint32_t x) {
  if (!x) x = 0xA5A5A5A5UL;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  return x;
}

const char* colonyMinerLaneName(uint8_t lane) {
  switch (lane) {
    case 0: return "linear";
    case 1: return "zim_reverse";
    case 2: return "bitrev";
    case 3: return "janus_center";
    case 4: return "knight";
    case 5: return "random";
    default: return "linear";
  }
}

uint32_t colonyZimStrideArmValue(uint8_t arm) {
  static const uint32_t arms[] = {
    1UL, 3UL, 5UL, 7UL, 11UL, 17UL, 29UL, 31UL, 53UL, 97UL, 257UL, 521UL,
    4099UL, 65537UL, 0x9E3779B9UL, 0xC4111903UL, 0x4F1BBCDDUL
  };
  return arms[arm % (sizeof(arms) / sizeof(arms[0]))] | 1UL;
}

void colonyMinerConfigureForJob(RemoteJobState& job) {
#if JANUS_MINER_V31_SCHEDULER_ENABLE
  uint32_t seed = micros() ^ ESP.getCycleCount() ^ colonyAgentEntropySeed ^ ((uint32_t)colonyJobsSeen << 16) ^ (uint32_t)colonyWorkerId;
  for (uint8_t i = 0; i < 8; ++i) seed = janusXorShift32(seed ^ job.job_id[i]);
  job.minerSeed = seed;
  job.minerStrideArm = (uint8_t)((seed ^ (seed >> 8) ^ colonyAgentLevel) % 17);
  job.minerStride = colonyZimStrideArmValue(job.minerStrideArm);

  // V31-inspired scheduler-only lane mix: DualLock-ish sector bias + Zim reverse/linear/knight/bitrev/random.
  uint8_t selector = (uint8_t)((seed ^ (seed >> 11) ^ (uint32_t)colonyAgentHint ^ (uint32_t)colonyJobsSeen) % 100);
  if (colonyAgentHint >= 3 || colonyAgentLevel >= 2) {
    if (selector < 42) job.minerLane = 1;       // zim_reverse
    else if (selector < 64) job.minerLane = 4;  // knight
    else if (selector < 80) job.minerLane = 2;  // bitrev
    else if (selector < 92) job.minerLane = 3;  // janus_center
    else job.minerLane = 5;                     // random baseline
  } else {
    if (selector < 38) job.minerLane = 0;       // linear proof lane
    else if (selector < 67) job.minerLane = 1;  // zim_reverse
    else if (selector < 80) job.minerLane = 2;  // bitrev
    else if (selector < 92) job.minerLane = 3;  // janus_center
    else job.minerLane = 5;
  }

  // DualLock flavor in 8 local sectors: prefer sector 6 for linear/zim, sector 7 for knight-tail probing.
  if (job.minerLane == 0 || job.minerLane == 1) job.minerSector = 6 % JANUS_MINER_V31_SECTORS;
  else if (job.minerLane == 4) job.minerSector = 7 % JANUS_MINER_V31_SECTORS;
  else job.minerSector = (uint8_t)((seed >> 24) % JANUS_MINER_V31_SECTORS);
  job.minerStartOffset = janusBitReverse32(seed ^ 0xC4111903UL);
  colonyMinerLaneSwitches++;
#else
  job.minerLane = 0;
  job.minerSector = 0;
  job.minerStrideArm = 0;
  job.minerStride = 1;
  job.minerSeed = micros();
  job.minerStartOffset = 0;
#endif
}

