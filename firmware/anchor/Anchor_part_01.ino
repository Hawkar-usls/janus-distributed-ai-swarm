static_assert(offsetof(BuzzShareResponseV2, nonce) == 10, "S/2 nonce offset changed");
static_assert(offsetof(BuzzShareResponseV2, bits) == 16, "S/2 bits offset changed");
static_assert(offsetof(BuzzShareResponseV2, hash_tail) == 22, "S/2 tail offset changed");

struct __attribute__((packed)) JanusFaceSyncPacket {
  uint8_t magic[2];      // 'J','F'
  uint8_t version;       // 1
  uint8_t roleFace;      // static/default face of this device
  uint16_t nodeId;
  uint32_t seq;
  uint32_t uptimeMs;
  uint8_t face;          // currently displayed face: 0 cyan, 1 magenta
  uint8_t brightness;
  uint16_t eventBits;    // share/proof bits if active
  uint8_t flags;         // bit0 own share/proof glow, bit1 peer known, bit2 gladius, bit3 anchor
  uint8_t reserved;
  uint32_t colorSeed;
  uint32_t crc;
};

struct __attribute__((packed)) JanusTwinTaskPacket {
  uint8_t magic[2];      // 'J','T' = Janus Twin task/race packet
  uint8_t version;       // 1
  uint8_t role;          // 1 Anchor, 2 Gladius
  uint16_t nodeId;
  uint32_t seq;
  uint32_t uptimeMs;
  uint8_t jobId8[8];
  uint32_t jobFp32;
  uint32_t jobStart;
  uint32_t jobRange;
  uint32_t checked;
  uint32_t nonce;
  uint32_t hashRate;
  uint32_t bestBits;
  uint32_t shares;
  uint32_t jobsSeen;
  uint8_t lane;
  uint8_t sector;
  uint16_t targetBits;
  uint32_t stride;
  uint8_t face;
  uint8_t brightness;
  uint16_t eventBits;
  uint16_t flags;
  int8_t rssi;
  uint8_t reserved;
  uint32_t crc;
};

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

struct __attribute__((packed)) SwarmSensePacket {
  uint8_t magic[2];
  uint8_t version;
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

// Common Janus P/N Cortex packet. Observer-only: RF/silicon/body trace for Core2,
// never a scheduler override and never a pool/share wire mutation.
struct __attribute__((packed)) JanusPnCortexPacket {
  uint8_t magic[2];        // 'P','N'
  uint8_t version;         // 1
  uint8_t role;
  uint16_t worker_id;
  char nodeId[24];
  char kind[16];
  uint32_t seq;
  uint32_t uptime_ms;
  uint32_t job_sig;
  uint32_t prev_hash;
  uint32_t packet_hash;
  uint32_t hash_rate;
  uint32_t total_hashes;
  uint16_t target_bits;
  uint16_t best_bits;
  uint8_t lane;
  uint8_t sector;
  uint8_t flags;           // bit0 job, bit1 Buzz/pool, bit2 RF dome, bit3 transition, bit4 policy/memory
  int8_t rssi;
  uint16_t thermal_x1000;
  uint16_t load_x1000;
  uint16_t jitter_us;
  uint16_t entropy_x1000;
  uint16_t tail_x1000;
  uint16_t voltage_mv;
  uint16_t ir_phase;
  uint16_t reserved;
};
// v1.13/v6.42C4 Core2 <-> Anchor RF dome packets.
// R/P is a small pulse from Core2. Anchor measures RSSI on reception.
// R/S is Anchor's interpreted RF-sleeve snapshot. It is not exact CSI;
// it is a low-cost ESP-NOW/RSSI human-sonar estimate for JANUS.
struct __attribute__((packed)) RfDomePingPacket {
  uint8_t magic[2];       // 'R','P'
  uint8_t version;        // 1
  uint8_t pingMode;       // 0 normal, 1 page-open active scan
  char source[16];        // Core2Home
  uint32_t seq;
  uint32_t uptimeMs;
  uint16_t pulse;
  uint8_t channel;
  uint8_t reserved;
};

struct __attribute__((packed)) RfDomeSonarPacket {
  uint8_t magic[2];       // 'R','S'
  uint8_t version;        // 1
  uint8_t flags;          // bit0 coreFresh, bit1 presence, bit2 motion, bit3 human?, bit4 pet?, bit5 learning
  char anchorId[24];
  uint32_t seq;
  uint32_t uptimeMs;
  int8_t coreRssi;
  int8_t ambientRssi;
  int16_t coreEma_x10;
  int16_t coreBase_x10;
  int16_t coreDelta_x10;
  uint16_t coreVar_x10;
  uint16_t motion_x100;
  uint16_t presence_x100;
  uint16_t human_x100;
  uint16_t pet_x100;
  uint8_t zonePct;
  uint16_t distanceCm;
  uint8_t confidence;
  uint16_t domeLengthCm;
  uint32_t packetsSeen;
  uint32_t crc;
};

// Explicit prototypes prevent Arduino .ino autoprototype from seeing custom RF Dome types too early.
uint32_t rfDomeCrc32(const void* data, size_t len);
void rfDomeOnCorePing(const RfDomePingPacket& ping, int8_t rssi);
void sendRfDome(bool force);
void sendAnchorPnCortex(bool force=false);
void sendHeartbeat();
void sendEntropy();
void sendSwarmSense(bool force);
void janusTwinTaskBroadcast(bool force);
void anchorPresenceBurst(const char* reason);
void ensurePeer();
uint8_t currentChannel();
bool ensureBuzzMasterPeer(const char* reason);
esp_err_t sendEspNowToBuzzMaster(const char* tag, const void* payload, size_t len);
void rememberBuzzMasterMac(const uint8_t* mac, const char* reason);


struct __attribute__((packed)) JanusAgentRewardPacket {
  uint8_t magic[2];
  uint8_t version;
  char source[16];
  char targetNode[24];
  uint32_t seq;
  uint8_t rewardLevel;
  uint8_t aiHint;
  uint16_t rewardPoints;
  uint16_t targetBatch;
  uint32_t entropySeed;
  float score;
  float predictedHashRate;
  float predictionError;
  uint32_t deltaShares;
  uint32_t uptime_ms;
};

// Compile-time wire guards shared with Gladius/Buzz. A field-order drift here
// would silently break ESP-NOW even though both sketches still compile.
static_assert(sizeof(JanusColonyPacket) == 80, "JANUS colony wire layout changed");
static_assert(sizeof(SwarmSensePacket) == 101, "SwarmSense wire layout changed");
static_assert(sizeof(JanusFaceSyncPacket) == 28, "Janus face wire layout changed");
static_assert(sizeof(JanusTwinTaskPacket) == 78, "Janus twin wire layout changed");
static_assert(sizeof(EntropyReportV2) == 81, "Entropy wire layout changed");
static_assert(sizeof(JanusPnCortexPacket) == 98, "P/N cortex wire layout changed");
static_assert(sizeof(RfDomePingPacket) == 32, "RF dome ping layout changed");
static_assert(sizeof(RfDomeSonarPacket) == 68, "RF dome sonar layout changed");
static_assert(sizeof(JanusAgentRewardPacket) == 77, "Agent reward wire layout changed");
static_assert(sizeof(SwarmSensePacket) <= ANCHOR_RX_MAX_LEN, "RX buffer too small");


struct RemoteJobState {
  bool active = false;
  uint8_t job_id[8] = {};
  uint8_t header[80] = {};
  uint8_t target[32] = {};
  uint32_t startNonce = 0;
  uint32_t rangeSize = 0;
  uint32_t nonce = 0;
  uint32_t hashesDone = 0;
  uint32_t receivedAt = 0;
  uint8_t minerLane = 0;
  uint8_t minerSector = 0;
  uint8_t minerStrideArm = 0;
  uint32_t minerSeed = 0;
  uint32_t minerStride = 1;
  uint32_t minerStartOffset = 0;
};

RemoteJobState job;
uint16_t workerId = 0;
uint32_t seqNo = 0;
uint32_t ssSeq = 0;
uint32_t anchorPnSeq = 0;
uint32_t anchorPnLastMs = 0;
uint32_t anchorPnTx = 0;
uint32_t anchorPnFail = 0;
uint32_t anchorPnPrevHash = 0;
uint32_t anchorLoopLastMs = 0;
uint32_t anchorLoopLastUs = 0;
uint16_t anchorLoopJitterUs = 0;
uint16_t anchorLoopMaxUs = 0;
uint32_t lastHeartbeatMs = 0;
uint32_t lastEntropyMs = 0;
uint32_t lastSwarmSenseMs = 0;
uint32_t lastHashTickMs = 0;
uint32_t lastMasterMs = 0;
uint32_t lastRfSampleMs = 0;
uint32_t lastRfDebugMs = 0;
uint32_t hashCounter = 0;
uint32_t hashRate = 0;
uint32_t shares = 0;
uint32_t rejects = 0;
uint32_t jobsSeen = 0;
uint32_t jobsDone = 0;
uint32_t jobsExpired = 0;
uint32_t jobsQueued = 0;
uint32_t jobsYielded = 0;
uint32_t jobsReplacedNewWork = 0;
uint32_t jobsDroppedDuplicate = 0;
uint32_t jobsAccepted = 0;
uint32_t jobsDiscoveryPings = 0;
uint32_t lastDiscoveryReplyMs = 0;
uint32_t jobsInvalid = 0;
uint32_t lastJobQueueLogMs = 0;
RemoteJobState queuedJob;
bool queuedJobValid = false;
uint32_t queuedJobAtMs = 0;
uint32_t queuedJobFp32 = 0;
uint16_t targetBits = 0;
uint32_t bestBits = 0;          // current accepted Buzz range
uint32_t bestNonce = 0;
uint32_t bestBitsLifetime = 0;
uint32_t bestNonceLifetime = 0;
uint32_t tailHits = 0;
uint32_t laneSwitches = 0;
uint32_t txOk = 0;
uint32_t txFail = 0;
int lastTxErr = 0;
int8_t lastRssi = -127;
uint8_t peerChannel = 0;
uint8_t buzzMasterMac[6] = {0};
bool buzzMasterMacKnown = false;
uint8_t buzzMasterPeerChannel = 0;
uint32_t buzzMasterMacSeenMs = 0;
uint32_t buzzMasterDirectOk = 0;
uint32_t buzzMasterDirectFail = 0;
uint32_t buzzMasterMacMissing = 0;
uint32_t buzzMasterLastLogMs = 0;
uint32_t anchorRadioLastRescueMs = 0;
uint32_t anchorRadioLastWatchMs = 0;
uint32_t anchorRadioLastTxFailSeen = 0;
uint32_t anchorRadioLastTxOkSeen = 0;
uint32_t anchorRadioRescues = 0;
uint32_t anchorPresenceBursts = 0;
uint32_t anchorLastPresenceBurstMs = 0;
uint32_t anchorLastPresenceRefreshMs = 0;
uint32_t anchorSwarmRejoinLastGuardMs = 0;
uint32_t anchorSwarmRejoinFirstMissingMs = 0;
uint32_t anchorSwarmRejoinLastStateLogMs = 0;
uint32_t anchorSwarmRejoinAttempts = 0;
uint32_t anchorSwarmRejoinSoftRescues = 0;
uint32_t anchorSwarmRejoinHardRestarts = 0;
uint8_t anchorSwarmRejoinEpisodeRescues = 0;
uint32_t lastStatusMs = 0;
uint32_t lastShareMs = 0;
uint16_t lastShareBits = 0;
uint32_t lastLedMs = 0;
uint8_t lastLedR = 0, lastLedG = 0, lastLedB = 0;
uint32_t janusFaceSeq = 0;
uint32_t janusFaceLastTxMs = 0;
uint32_t janusFacePeerLastMs = 0;
uint16_t janusFacePeerNode = 0;
uint8_t janusFacePeerRoleFace = 255;
uint8_t janusFacePeerFace = 255;
uint8_t janusFacePeerFlags = 0;
uint16_t janusFacePeerEventBits = 0;
int8_t janusFacePeerRssi = -127;

uint32_t janusTwinSeq = 0;
uint32_t janusTwinLastTxMs = 0;
uint32_t janusTwinPeerLastMs = 0;
uint16_t janusTwinPeerNode = 0;
uint8_t janusTwinPeerRole = 0;
uint32_t janusTwinPeerJobFp32 = 0;
uint32_t janusTwinPeerJobStart = 0;
uint32_t janusTwinPeerJobRange = 0;
uint32_t janusTwinPeerChecked = 0;
uint32_t janusTwinPeerHashRate = 0;
uint32_t janusTwinPeerBestBits = 0;
uint32_t janusTwinPeerShares = 0;
uint8_t janusTwinPeerLane = 0;
uint8_t janusTwinPeerSector = 0;
uint16_t janusTwinPeerTargetBits = 0;
uint32_t janusTwinPeerStride = 0;
uint16_t janusTwinPeerFlags = 0;
int8_t janusTwinPeerRssi = -127;
uint32_t janusTwinRx = 0;
uint32_t janusTwinTx = 0;
uint32_t janusTwinSplitApplied = 0;
uint32_t janusTwinRaceWins = 0;
uint32_t janusTwinRaceLosses = 0;
float anchorOxytocin = 50.0f;
float anchorTorricelliVacuum = 0.50f;
uint32_t anchorTorricelliLastMs = 0;
float anchorTranceptionLiteScore = 0.0f;
uint8_t anchorTranceptionHint = 1;
uint8_t anchorTranceptionLane = 0;
uint32_t anchorTranceptionLastMs = 0;
uint32_t anchorTranceptionReports = 0;
uint32_t lastMaxBrightnessFlashMs = 0;
uint8_t ledBrightness = ANCHOR_LED_BRIGHTNESS;
// Unified Janus twin control: long BOOT hold or serial U toggles UART0 full logs
// and the small separate blue/activity LED together, just like Gladius.
bool anchorSmallLedEnabled = false;
bool anchorSmallLedLastState = false;
uint32_t lastButtonSampleMs = 0;
uint32_t buttonPressStartMs = 0;
uint32_t buttonLastRepeatMs = 0;
bool buttonStablePressed = false;
bool buttonLastRawPressed = false;
bool buttonLongMode = false;
bool buttonLogToggleFired = false;
bool buttonBrightnessDirUp = true;  // v1.12: tap-cycle direction, flips at min/max.
uint32_t brightnessChangedMs = 0;
bool brightnessDirty = false;
#if ANCHOR_BRIGHTNESS_PERSIST
Preferences anchorPrefs;
bool anchorPrefsReady = false;
#endif
uint32_t heartbeatTx = 0;
uint32_t entropyTx = 0;
uint32_t swarmSenseTx = 0;
uint32_t rxSeen = 0;
uint32_t lastAnyRxMs = 0;
uint32_t rxJanus = 0;
uint32_t rxJobs = 0;
uint32_t rxAgent = 0;
volatile uint32_t sentCbOk = 0;
volatile uint32_t sentCbFail = 0;
volatile bool sentCbSawFailure = false;
volatile uint16_t sentCbConsecutiveFail = 0;
uint32_t sentCbLastReportedOk = 0;
uint32_t sentCbLastReportedFail = 0;
volatile uint32_t rxQueueDrops = 0;
uint32_t rxQueueProcessed = 0;
QueueHandle_t anchorNowQueue = nullptr;
uint32_t lastWiFiReconnectMs = 0;
wl_status_t lastWiFiStatus = WL_IDLE_STATUS;
uint8_t lastWiFiChannel = 0;
uint32_t lastWaitLogMs = 0;
uint32_t lastMinerLogMs = 0;
uint32_t lastUart0StatusMs = 0;
uint32_t totalHashesLifetime = 0;
float hashRateEma = 0.0f;

uint8_t agentHint = 1;
uint8_t agentLevel = 0;
uint16_t agentBatch = REMOTE_BATCH_BASE;
uint32_t agentEntropySeed = 0xC4111903UL;
uint32_t agentRewards = 0;
float agentScore = 0.0f;
float agentPredH = 0.0f;
float agentErr = 0.0f;

float rfEma = -127.0f;
float rfBase = -127.0f;
float rfNoise = 3.0f;
float rfDrift = 0.0f;
float rfMotion = 0.0f;
float rfPresence = 0.0f;
float rfEntropy = 0.0f;
float rfPacketPressure = 0.0f;
float rfLastPacketDrift = 0.0f;
uint32_t rfRxPackets = 0;
uint32_t rfLastPacketMs = 0;
bool rfReady = false;
bool rfPresenceNow = false;
bool rfMotionNow = false;

uint32_t rfDomeSeq = 0;
uint32_t rfDomeRxPing = 0;
uint32_t rfDomeTx = 0;
uint32_t rfDomeLastPingMs = 0;
uint32_t rfDomeLastTxMs = 0;
uint32_t rfDomeLastLogMs = 0;
int8_t rfDomeCoreRssi = -127;
float rfDomeCoreEma = -127.0f;
float rfDomeCoreBase = -127.0f;
float rfDomeVar = 4.0f;
float rfDomeDelta = 0.0f;
float rfDomeMotion = 0.0f;
float rfDomePresence = 0.0f;
float rfDomeHuman = 0.0f;
float rfDomePet = 0.0f;
uint8_t rfDomeZonePct = 50;
uint16_t rfDomeDistanceCm = RF_DOME_DEFAULT_LENGTH_CM / 2;
uint8_t rfDomeConfidence = 0;
bool rfDomeReady = false;


uint32_t bitReverse32(uint32_t x);

// v1.19B SAFE AGE GUARD:
// ESP-NOW callbacks can update timestamps a few milliseconds AFTER loop() captured `now`.
// Raw `now - then` then prints 4294967xxx ms and can falsely stale-drop a queued job.
// Unsigned subtraction is still preserved for real millis() rollover; only impossible
// half-range deltas are clamped to zero.
uint32_t janusSafeAgeMs(uint32_t now, uint32_t then, uint32_t missing = 999999UL) {
  if (!then) return missing;
  uint32_t d = now - then;
  if (d > 0x7FFFFFFFUL) return 0UL;
  return d;
}

bool janusSafeElapsed(uint32_t now, uint32_t then, uint32_t intervalMs) {
  if (!then) return false;
  return janusSafeAgeMs(now, then, 0UL) >= intervalMs;
}

size_t janusWireFieldLength(const char* field, size_t fieldCap) {
  if (!field) return 0;
  size_t n = 0;
  while (n < fieldCap && field[n] != '\0') n++;
