// JANUS BLACKBOARD runtime state.
uint32_t janusEventSeq = 0;
uint32_t janusEventLastTxMs = 0;
uint32_t janusEventLastMotionMs = 0;
uint32_t janusEventLastPresenceMs = 0;
uint32_t janusEventLastStatusMs = 0;
uint32_t janusPolicyRx = 0;
uint32_t janusPolicySeq = 0;
uint32_t janusPolicyUntilMs = 0;
uint32_t janusPolicyQuietUntilMs = 0;
uint8_t janusPolicyMood = JM_IDLE;
uint8_t janusPolicyRadioRate = 1;
uint8_t janusPolicySensorRate = 1;
uint8_t janusPolicyBuzzBudget = 1;
uint8_t janusPolicyConfidence = 0;
char janusPolicyOrder[40] = "-";
uint16_t janusPolicySensorIntervalMs = SENSOR_INTERVAL_MS;

// v2.10G Eye episodic memory / semantic task bridge / S/S telemetry.
JanusEyeEpisode janusEyeEpisodes[JANUS_EYE_EPISODE_COUNT];
uint8_t janusEyeEpisodeHead = 0;
uint8_t janusEyeEpisodeCount = 0;
uint32_t janusEyeLastEpisodeMs = 0;
uint32_t janusEyeLastAiMemoryMs = 0;
uint32_t janusEyeLastTaskNeedMs = 0;
uint32_t janusEyeLastTaskDoneMs = 0;
uint32_t janusEyeTaskNeedTx = 0;
uint32_t janusEyeTaskDoneTx = 0;
uint32_t janusEyeAiMemoryTx = 0;
uint32_t janusEyeSwarmSenseSeq = 0;
uint32_t janusEyeSwarmSenseTx = 0;
uint32_t janusEyeSwarmSenseFail = 0;
uint32_t janusEyeLastSwarmSenseMs = 0;
bool janusEyePrevPresenceNow = false;
bool janusEyePrevMotionNow = false;

// Atomic Motion Base v1.2 scaffold/runtime state.
bool motionBasePresent = false;
bool motionBasePowerPresent = false;
bool motionBaseArmed = false;       // Core policy may arm; writes still require compile flag.
bool motionBaseTrackEnabled = true; // planner is enabled, physical writes are safe-gated.
uint8_t motionBasePowerAddr = 0;
uint32_t motionBaseLastTickMs = 0;
uint32_t motionBaseLastPowerMs = 0;
uint32_t motionBaseLastStatusMs = 0;
uint32_t motionBaseServoWrites = 0;
uint32_t motionBaseI2cErrors = 0;
int16_t motionBaseBusMv = 0;
int16_t motionBaseCurrentRaw = 0;
int16_t motionBasePowerRaw = 0;
int16_t motionBaseServoAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
int16_t motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
int16_t motionBaseLastSentAngle = -1;
int8_t motionBaseMotorSpeed[2] = {0, 0};
uint8_t motionBaseBatteryPct = 0;
uint8_t motionBasePowerFlags = 0;
uint8_t motionBasePowerSource = 0;
uint16_t motionBaseLastCellMv = 0;       // last real 1S battery voltage seen before USB/boost rail
uint8_t motionBaseLastCellPct = 0;       // kept while USB-C is connected, so we do not fake 100%
uint32_t motionBaseExternalSinceMs = 0;
uint32_t motionBaseBatterySeq = 0;
uint32_t motionBaseLastBatteryTxMs = 0;
// v2.14D: Motion Base is optional. If absent, BlindEye becomes a normal sensor/swarm node. Serial S enters PASSIVE_EYE mode.
bool motionBaseOptionalAbsent = false;
bool motionBaseEverDetected = false;
uint32_t motionBaseAbsentSinceMs = 0;
// v2.13 RoboZombie crawler runtime.
bool roboZombieLocalArm = JANUS_ROBOZOMBIE_LOCAL_TEST_ARM != 0;
bool roboZombieCrawlerManualEnable = false;
bool roboZombiePassiveMode = false;   // Serial capital S: passive eye, no actuator writes until a/g/manual target re-enables
bool roboZombieLeftReverse = JANUS_ROBOZOMBIE_LEFT_REVERSE != 0;
bool roboZombieRightReverse = JANUS_ROBOZOMBIE_RIGHT_REVERSE != 0;
bool roboZombieHeadPresent = JANUS_ROBOZOMBIE_HEAD_PRESENT != 0;
bool roboZombieLeftLegPresent = JANUS_ROBOZOMBIE_LEFT_LEG_PRESENT != 0;
bool roboZombieRightLegPresent = JANUS_ROBOZOMBIE_RIGHT_LEG_PRESENT != 0;
uint8_t roboZombieBasePull = 30;
uint32_t roboZombieCrawlerPulses = 0;
uint32_t roboZombieCrawlerLastStopMs = 0;
uint32_t roboZombieCrawlerLastPulseMs = 0;
int8_t roboZombieLastLeftSpeed = 0;
int8_t roboZombieLastRightSpeed = 0;
uint8_t roboZombieLastLeftValue = JANUS_ROBOZOMBIE_SERVO_STOP;
uint8_t roboZombieLastRightValue = JANUS_ROBOZOMBIE_SERVO_STOP;
float roboZombieGaitConfidence = 0.0f;
uint32_t roboZombieLastConfidentMs = 0;
uint32_t roboZombieAutoBlockedLowPowerMs = 0;



float hist_activity[HIST_SIZE] = {0};
float hist_loss[HIST_SIZE] = {0};
int hist_count = 0;
int hist_pos = 0;

unsigned long lastSensorAt = 0;
unsigned long lastSendAt = 0;
unsigned long lastCmdAt = 0;
unsigned long lastDebugAt = 0;
unsigned long lastSaveAt = 0;
unsigned long lastWifiTry = 0;

String statusLine = "boot";
String diagLine = "init";

i2s_chan_handle_t rx_handle = nullptr;

// ========================= HELPERS =========================

String joinUrl(const String& base, const String& path) {
  if (base.endsWith("/") && path.startsWith("/")) return base.substring(0, base.length() - 1) + path;
  if (!base.endsWith("/") && !path.startsWith("/")) return base + "/" + path;
  return base + path;
}

bool initWiFi(bool force = false) {
  if (!force && WiFi.status() == WL_CONNECTED) return true;
  if (!force && millis() - lastWifiTry < WIFI_RETRY_MS) return false;

  lastWifiTry = millis();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long until = millis() + 5000;
  while (WiFi.status() != WL_CONNECTED && millis() < until) {
    delay(80);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool ensureWiFi() {
  return WiFi.status() == WL_CONNECTED ? true : initWiFi(false);
}

bool httpGet(const String& url, String& response, int timeoutMs = 450) {
  // JANUS v2.3: HTTP disabled. ESP-NOW colony only.
  return false;
}


bool httpPostJson(const String& url, const String& payload, String& response, int timeoutMs = 800) {
  // JANUS v2.2: HTTP removed. ESP-NOW only.
  return false;
}


// ========================= TMOS =========================

void initTMOS() {
  janusSelectGroveBus(true);
  if (tmos.begin(&Wire, STHS34PF80_I2C_ADDRESS, GROVE_SDA_PIN, GROVE_SCL_PIN)) tmos_ready = true;
  else if (tmos.begin(&Wire, 0x5A, GROVE_SDA_PIN, GROVE_SCL_PIN)) tmos_ready = true;
  else if (tmos.begin(&Wire, 0x5B, GROVE_SDA_PIN, GROVE_SCL_PIN)) tmos_ready = true;
  else tmos_ready = false;

  if (tmos_ready) {
    // Match the 100 ms sensor loop with fresh samples and use the sensor's own
    // presence/motion flags as a second vote, never as fake camera pixels.
    tmos.setTmosODR(STHS34PF80_TMOS_ODR_AT_15Hz);
    tmos.setPresenceThreshold(0xC8);
    tmos.setMotionThreshold(0xC8);
    tmos.setPresenceHysteresis(0x32);
    tmos.setMotionHysteresis(0x32);
    Serial.println("[EYE/TMOS] primary eye READY odr=15Hz camera=ABSENT aperture=TMOS/RF");
  } else {
    Serial.println("[EYE/TMOS] primary eye MISSING camera=ABSENT sensor-only fallback=RF/IMU/MIC");
  }
}

void calibrateTMOS() {
  if (!tmos_ready) return;

  int32_t sumP = 0;
  int32_t sumM = 0;
  int32_t minP = 32767, maxP = -32768;
  int32_t minM = 32767, maxM = -32768;
  const int samples = 48;

  // Keep the sensor still and preferably aim it at an empty/neutral part of the room
  // during this boot window. If the operator is inside the beam, Eagle Focus will
  // still recover slowly, but the first minute will be less sensitive.
  for (int i = 0; i < samples; ++i) {
    tmos.getPresenceValue(&raw_presence);
    tmos.getMotionValue(&raw_motion);
    sumP += raw_presence;
    sumM += raw_motion;
    minP = min<int32_t>(minP, raw_presence);
    maxP = max<int32_t>(maxP, raw_presence);
    minM = min<int32_t>(minM, raw_motion);
    maxM = max<int32_t>(maxM, raw_motion);
    delay(28);
  }

  calib_presence = sumP / samples;
  calib_motion = sumM / samples;
  tmos_presence_baseline = (float)calib_presence;
  tmos_motion_baseline = (float)calib_motion;
  tmos_presence_noise = constrain((float)(maxP - minP) * 0.55f + 8.0f, 6.0f, 80.0f);
  tmos_motion_noise = constrain((float)(maxM - minM) * 0.55f + 5.0f, 4.0f, 70.0f);
  tmos_focus_gain = 3.2f;
  tmos_focus_confidence = 0.0f;
  tmos_presence_memory = 0.0f;
  tmos_motion_memory = 0.0f;
  tmos_occupancy = 0.0f;
  tmos_ghost_score = 0.0f;
  tmos_clear_score = 0.0f;
  tmos_bad_frame_streak = 0;
  tmos_last_read_error = 0;
  tmos_presence_instant = false;
  tmos_motion_instant = false;
  tmos_presence_now = false;
  tmos_motion_now = false;
  tmos_hw_status_valid = false;
  tmos_hw_presence_flag = false;
  tmos_hw_motion_flag = false;
  tmos_presence_hold_until_ms = 0;
  tmos_motion_hold_until_ms = 0;
  tmos_last_focus_ms = 0;
  tmos_last_valid_ms = millis();
  tmos_focus_ready = true;
  calibrated = true;
  Serial.printf("[EYE/CAL] v2.9I truth base=%.1f/%.1f noise=%.1f/%.1f room-empty=%u\n",
                tmos_presence_baseline, tmos_motion_baseline,
                tmos_presence_noise, tmos_motion_noise,
                (unsigned)JANUS_EYE_RECALIBRATE_ON_BOOT);
}

