bool janusEyeEspNowSend(const char* tag, const void* payload, size_t len, bool repairOnFail) {
#if JANUS_COLONY_ENABLE
  if (!payload || len == 0) return false;
  if (WiFi.status() != WL_CONNECTED) {
    colonyTxFail++;
    colonyLastTxErr = ESP_ERR_ESPNOW_IF;
    strlcpy(colonyLastTxTag, tag ? tag : "wifi-off", sizeof(colonyLastTxTag));
    return false;
  }

  ensureColonyPeer();
  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (const uint8_t*)payload, len);
  if (err == ESP_OK) {
    colonyTxOk++;
    return true;
  }

  colonyTxFail++;
  colonyLastTxErr = err;
  strlcpy(colonyLastTxTag, tag ? tag : "send", sizeof(colonyLastTxTag));
  colonyPeerChannel = 0;
  Serial.printf("[COLONY/EYE] TX FAIL tag=%s err=%d fail=%lu ch=%u\n",
                colonyLastTxTag, (int)err, (unsigned long)colonyTxFail, (unsigned)colonyPeerChannel);
  if (repairOnFail) forceColonyPeerRebuild(tag ? tag : "tx-fail");
  return false;
#else
  (void)tag; (void)payload; (void)len; (void)repairOnFail;
  return false;
#endif
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onColonyRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len)
#else
void onColonyRecv(const uint8_t *mac, const uint8_t *data, int len)
#endif
{
  if (!data || len < 2) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (info && info->rx_ctrl) colonyLastRssi = info->rx_ctrl->rssi;
#endif
  rfLiteOnPacketRssi(colonyLastRssi);

  if (len == sizeof(JanusPolicyPacket) && data[0] == 'J' && data[1] == 'P') {
    JanusPolicyPacket jp{};
    memcpy(&jp, data, sizeof(jp));
    onJanusPolicyPacket(jp);
    return;
  }
  if (len == sizeof(JanusEventPacket) && data[0] == 'J' && data[1] == 'E') {
    // Core consumes J/E. BlindEye only ignores echoes/other semantic events for now.
    return;
  }

  if (len == sizeof(JanusColonyPacket)) {
    JanusColonyPacket pkt{}; memcpy(&pkt, data, sizeof(pkt));
    if (memcmp(pkt.magic, "JANUS", 5) == 0 && looksLikeBuzzMaster(pkt)) {
      colonyMasterSeen = true;
      colonyLastMasterMs = millis();
      if (!colonyJob.active) strlcpy(colonyMode, "READY", sizeof(colonyMode));
    }
    onJanusHeartbeat(pkt);
    return;
  }
  if (len == sizeof(JobPacket) && data[0] == 'J' && data[1] == 'B') {
    JobPacket jp{}; memcpy(&jp, data, sizeof(jp));
    memcpy(colonyJob.job_id, jp.job_id, 8);
    memcpy(colonyJob.header, jp.header, 80);
    memcpy(colonyJob.target, jp.target, 32);
    colonyJob.startNonce = jp.start_nonce;
    colonyJob.rangeSize = jp.range_size ? jp.range_size : COLONY_JOB_RANGE_DEFAULT;
    colonyJob.nonce = jp.start_nonce;
    colonyJob.endNonce = jp.start_nonce + colonyJob.rangeSize;
    colonyJob.hashesDone = 0;
    colonyJob.receivedAt = millis();
    colonyMinerConfigureForJob(colonyJob);
    colonyJob.active = true;
    colonyTargetBits = countLeadingZeroBitsBE(colonyJob.target);
    colonyJobsSeen++;
    colonyMasterSeen = true;
    colonyLastMasterMs = millis();
    strlcpy(colonyMode, "REMOTE", sizeof(colonyMode));
    return;
  }
  if (len == sizeof(JanusAgentRewardPacket) && data[0] == 'A' && data[1] == 'R') {
    JanusAgentRewardPacket ar{};
    memcpy(&ar, data, sizeof(ar));
    onJanusAgentReward(ar);
    return;
  }
  if (len == sizeof(JanusKenshiPacket) && data[0] == 'K' && data[1] == '2') {
    JanusKenshiPacket kp{};
    memcpy(&kp, data, sizeof(kp));
    onJanusKenshiPacket(kp, colonyLastRssi);
    return;
  }
  if (len == sizeof(JanusTachyonProphecyPacket) && data[0] == 'T' && data[1] == 'P') {
    JanusTachyonProphecyPacket tp{};
    memcpy(&tp, data, sizeof(tp));
    onJanusTachyonProphecy(tp, colonyLastRssi);
    return;
  }
  if (len == sizeof(JanusEyeVisionControlPacket) && data[0] == 'E' && data[1] == 'C') {
    JanusEyeVisionControlPacket ec{};
    memcpy(&ec, data, sizeof(ec));
    onJanusEyeVisionControl(ec);
    return;
  }
  if (len == sizeof(EntropyReport) && data[0] == 'E' && data[1] == 'R') { EntropyReport er{}; memcpy(&er, data, sizeof(er)); onJanusEntropy(er, nullptr); return; }
  if (len == sizeof(EntropyReportV2) && data[0] == 'E' && data[1] == '2') { EntropyReportV2 er2{}; memcpy(&er2, data, sizeof(er2)); onJanusEntropyV2(er2); return; }
}

void initColonyNow() {
#if JANUS_COLONY_ENABLE
  colonyWorkerId = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) { Serial.println("[COLONY] ESP-NOW init failed"); return; }
  esp_now_register_recv_cb(onColonyRecv);
  ensureColonyPeer();
  Serial.printf("[COLONY] ESP-NOW ready id=%u channel=%u\n", colonyWorkerId, colonyPeerChannel);
#endif
}

void colonyTick() {
  if (WiFi.status() == WL_CONNECTED) ensureColonyPeer();
  if (millis() - colonyLastMasterMs > COLONY_MASTER_TIMEOUT_MS) {
    colonyMasterSeen = false;
    if (!colonyJob.active) strlcpy(colonyMode, "SEEK", sizeof(colonyMode));
  }
  runRemoteMiningBatch();
  if (millis() - colonyLastHeartbeatMs >= COLONY_HEARTBEAT_MS) { colonyLastHeartbeatMs = millis(); sendNodeHeartbeat(); }
  if (millis() - colonyLastEntropyMs >= COLONY_ENTROPY_MS) { colonyLastEntropyMs = millis(); sendNodeEntropy(); }
  kenshiBubbleTick();
  tachyonProphecyTick();
  eyeVisionTick();
}


extern "C" {
  #include "driver/i2s_std.h"
}

// ========================= JANUS BLIND EYE =========================

#define DEVICE_ID              "atom_s3r_blind_eye"
#define DEVICE_KIND            "blind_eye_rf_fusion_v2_11_ruview_lite_buzz_miner"

#define WIFI_SSID              "YOUR_WIFI"
#define WIFI_PASSWORD          "YOUR_PASS"
#define SERVER_BASE            "http://192.168.1.92:5000"

#define EP_DEVICE_DATA         "/api/device/data"
#define EP_DEVICE_COMMAND      "/api/device/command/"

#define SERVER_URL             SERVER_BASE EP_DEVICE_DATA
#define COMMAND_URL_BASE       SERVER_BASE EP_DEVICE_COMMAND

#define SENSOR_INTERVAL_MS     100
#define SEND_INTERVAL_MS       2500
#define COMMAND_INTERVAL_MS    3000
#define JANUS_HTTP_LEGACY_ENABLE 0
#define HEADLESS_DEBUG_INTERVAL_MS 5000UL
#define SAVE_INTERVAL_MS       60000UL
#define WIFI_RETRY_MS          7000UL

// Grove TMOS
#ifndef GROVE_SDA_PIN
#define GROVE_SDA_PIN          2
#endif
#ifndef GROVE_SCL_PIN
#define GROVE_SCL_PIN          1
#endif

// Bottom mic base
#define MIC_I2S_BCLK_PIN       6
#define MIC_I2S_WS_PIN         5
#define MIC_I2S_DATA_PIN       7
#define MIC_SAMPLE_RATE        16000
#define MIC_FRAME_SAMPLES      192

#define FEATURE_DIM            10
#define HIST_SIZE              48

#define MODEL_FILE             "/eye_model.bin"
#define STATE_FILE             "/eye_state.json"

// ========================= GLOBALS =========================

M5_STHS34PF80 tmos;
bool tmos_ready = false;
bool calibrated = false;

int16_t raw_presence = 0;
int16_t raw_motion = 0;
int16_t calib_presence = 0;
int16_t calib_motion = 0;

float acc_x = 0, acc_y = 0, acc_z = 0;
float gyro_x = 0, gyro_y = 0, gyro_z = 0;
float mag_x = 0, mag_y = 0, mag_z = 0;
float imu_temp = 0;
float imu_shock = 0;
float mag_norm = 0;

float tmos_presence = 0;
float tmos_motion = 0;

// v2.9 Eagle Focus runtime state.
float tmos_presence_delta = 0.0f;
float tmos_motion_delta = 0.0f;
float tmos_presence_baseline = 0.0f;
float tmos_motion_baseline = 0.0f;
float tmos_presence_noise = 12.0f;
float tmos_motion_noise = 8.0f;
float tmos_focus_gain = 3.2f;
float tmos_focus_confidence = 0.0f;
bool tmos_focus_ready = false;
uint32_t tmos_last_focus_ms = 0;
float tmos_presence_memory = 0.0f;
float tmos_motion_memory = 0.0f;
float tmos_occupancy = 0.0f;
// Kept as tmos_ghost_score for packet/state compatibility. In v2.15A it is an
// artifact/health score: an empty, valid room drives it DOWN instead of UP.
float tmos_ghost_score = 0.0f;
float tmos_clear_score = 0.0f;
uint32_t tmos_last_valid_ms = 0;
uint16_t tmos_bad_frames = 0;
uint16_t tmos_bad_frame_streak = 0;
int32_t tmos_last_read_error = 0;
bool tmos_presence_instant = false;
bool tmos_motion_instant = false;
bool tmos_presence_now = false;
bool tmos_motion_now = false;
bool tmos_hw_status_valid = false;
bool tmos_hw_presence_flag = false;
bool tmos_hw_motion_flag = false;
uint32_t tmos_presence_hold_until_ms = 0;
uint32_t tmos_motion_hold_until_ms = 0;
float mic_rms = 0;
int wifi_rssi = -127;

// v2.12 RuView-lite RF Fusion runtime state.
float rf_rssi_ema = -127.0f;
float rf_rssi_baseline = -127.0f;
float rf_rssi_noise = 2.8f;
float rf_abs_drift = 0.0f;
float rf_motion_energy = 0.0f;
float rf_presence_score = 0.0f;
float rf_entropy = 0.0f;
float rf_packet_pressure = 0.0f;
float rf_last_packet_drift = 0.0f;
int8_t rf_last_packet_rssi = -127;
bool rf_ready = false;
bool rf_presence_now = false;
bool rf_motion_now = false;
uint32_t rf_samples = 0;
uint32_t rf_rx_packets = 0;
uint32_t rf_last_sample_ms = 0;
uint32_t rf_last_packet_ms = 0;
uint32_t rf_anomaly_count = 0;
uint32_t rf_last_debug_ms = 0;

// v2.12 TMOS warmup / ghost damping / policy smoothing runtime state.
uint32_t janusEyeBootMs = 0;
uint32_t tmosWarmupUntilMs = 0;
uint32_t tmosGhostHighSinceMs = 0;
uint32_t tmosLastGhostTaskNeedMs = 0;
uint8_t janusPolicyCandidateMood = JM_IDLE;
uint8_t janusPolicyCandidateCount = 0;
uint8_t janusPolicyRawLastMood = JM_IDLE;
uint32_t janusPolicyLastMoodChangeMs = 0;
uint32_t janusPolicySmoothedDrops = 0;
uint32_t janusPolicyAcceptedChanges = 0;


float model_w[FEATURE_DIM] = {0.10f, -0.02f, 0.08f, 0.12f, 0.05f, 0.03f, 0.06f, 0.05f, 0.04f, -0.02f};
float model_b = 0.0f;
float model_lr = 0.0020f;

float pred_activity = 0;
float activity = 0;
float loss = 0;
float fit = 0;
float fit_best = -9999.0f;
float z_activity = 0;
float z_loss = 0;
float sync_hint = 0;

// v2.8 Tachyon Prophecy / Physarious micro movement state.
float tachyonPredPresence1 = 0.0f;
float tachyonPredMotion1 = 0.0f;
float tachyonPredPresence2 = 0.0f;
float tachyonPredMotion2 = 0.0f;
float tachyonPredPresence3 = 0.0f;
float tachyonPredMotion3 = 0.0f;
float tachyonLastPredPresence1 = 0.0f;
float tachyonLastPredMotion1 = 0.0f;
float tachyonPresenceConfidence = 0.15f;
float tachyonMotionConfidence = 0.15f;
float tachyonFutureStress = 0.0f;
float tachyonEventEtaMs = 9999.0f;
float tachyonLossEma = 0.0f;
float tachyonPhysarumTrace = 0.0f;
float tachyonLangerDrag = 0.0f;
float tachyonEnergy = 0.0f;
float tachyonSwarmPressure = 0.0f;
float tachyonRemotePresence = 0.0f;
float tachyonRemoteMotion = 0.0f;
float tachyonTrendGain = 0.72f;
float tachyonMemoryGain = 0.24f;
float tachyonRemoteGain = 0.18f;
float tachyonSeqPresence[JANUS_TACHYON_SEQ_N] = {0};
float tachyonSeqMotion[JANUS_TACHYON_SEQ_N] = {0};
float tachyonSeqActivity[JANUS_TACHYON_SEQ_N] = {0};
uint8_t tachyonSeqPos = 0;
uint8_t tachyonSeqCount = 0;
uint32_t tachyonSeq = 0;
uint32_t tachyonLastTxMs = 0;
uint32_t tachyonLastRxMs = 0;
uint32_t tachyonTxPackets = 0;
uint32_t tachyonRxPackets = 0;
uint8_t tachyonRemoteCount = 0;
JanusRemoteProphecyState tachyonRemotes[JANUS_TACHYON_REMOTE_N];

// Core2 "eye of eyes" E/C -> E/F TMOS-aperture stream state.
// No camera bytes are read anywhere in this build.
bool eyeVisionEnabled = false;
uint16_t eyeVisionFrameMs = JANUS_EYE_VISION_DEFAULT_FRAME_MS;
uint16_t eyeVisionSeq = 0;
uint32_t eyeVisionLastControlMs = 0;
uint32_t eyeVisionLastFrameMs = 0;
uint32_t eyeVisionLastEventFrameMs = 0;
uint32_t eyeVisionFramesTx = 0;
uint32_t eyeVisionEventFramesTx = 0;
uint32_t eyeVisionControlsRx = 0;
uint32_t eyeVisionControlsIgnored = 0;
char eyeVisionSource[16] = "-";


