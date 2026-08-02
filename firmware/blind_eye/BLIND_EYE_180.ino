void setup() {
  auto cfg = M5.config();
  cfg.serial_baudrate = 115200;
  M5.begin(cfg);  // Used for IMU/buttons/power only. Blind EYE is headless: no display output.
  Serial.begin(115200);
  Serial.println("JANUS Blind Eye v2.15A TMOS PRIMARY / CAMERA ABSENT / RF FUSION / HEADLESS");

  LittleFS.begin(true);
  loadState();
  loadModel();
  loadKenshiState();
  loadTachyonState();

  janusSelectGroveBus(true);
  Serial.printf("[I2C] WireMux ready: Grove/TMOS SDA=%u SCL=%u, MotionBase SDA=%u SCL=%u addr=0x%02X\n",
                (unsigned)GROVE_SDA_PIN, (unsigned)GROVE_SCL_PIN,
                (unsigned)JANUS_MOTION_BASE_SDA_PIN, (unsigned)JANUS_MOTION_BASE_SCL_PIN,
                (unsigned)JANUS_MOTION_BASE_I2C_ADDR);
  initIMU();
  initMotionBase();
  janusSelectGroveBus(true);
  initTMOS();
  initMicI2S();
  initWiFi(true);
  initColonyNow();

  janusEyeBootMs = millis();
  tmosWarmupUntilMs = janusEyeBootMs + JANUS_TMOS_WARMUP_MS;
  janusPolicyLastMoodChangeMs = janusEyeBootMs;
  Serial.printf("[EYE/TMOS] camera=ABSENT primary=TMOS warmup=%lus softSettle=%.3f/%.3f outScale=%.2f jump=%.0f artifact_task_gate=level%.2f hold%lus cooldown%lus\n",
                (unsigned long)(JANUS_TMOS_WARMUP_MS / 1000UL),
                JANUS_TMOS_WARMUP_SETTLE_ALPHA,
                JANUS_TMOS_WARMUP_SOFT_ALPHA,
                JANUS_TMOS_WARMUP_OUTPUT_SCALE,
                JANUS_TMOS_BASELINE_JUMP_LEVEL,
                JANUS_GHOST_TASKNEED_LEVEL,
                (unsigned long)(JANUS_GHOST_TASKNEED_HOLD_MS / 1000UL),
                (unsigned long)(JANUS_GHOST_TASKNEED_COOLDOWN_MS / 1000UL));

#if JANUS_EYE_RECALIBRATE_ON_BOOT
  // v2.9I: do not trust old saved TMOS baseline. Start empty-room truth each boot.
  calibrated = false;
  tmos_focus_ready = false;
  tmos_presence_memory = 0.0f;
  tmos_motion_memory = 0.0f;
  tmos_occupancy = 0.0f;
  tmos_ghost_score = 0.0f;
  tmos_clear_score = 0.0f;
  tmos_bad_frame_streak = 0;
  tmos_presence_instant = false;
  tmos_motion_instant = false;
  tmos_presence_now = false;
  tmos_motion_now = false;
  tmos_presence_hold_until_ms = 0;
  tmos_motion_hold_until_ms = 0;
  if (tmos_ready) {
    calibrateTMOS();
  }
#else
  if (!calibrated && tmos_ready) {
    calibrateTMOS();
  }
#endif

  statusLine = M5.Imu.isEnabled() ? "TMOS primary + RF fusion + kenshi tachyon" : "TMOS primary / IMU disabled";
  diagLine = tmos_ready ? "IMU MAG TMOS PIR MIC / NO CAMERA" : "RF IMU MIC / TMOS MISSING / NO CAMERA";
  janusEmitEyeEvent(JE_BOOT, 92, 35,
                    (int16_t)(tmos_ready ? 1 : 0),
                    (int16_t)(motionBasePresent ? 1 : 0),
                    motionBaseBusMv,
                    (int16_t)(JANUS_MOTION_BASE_WRITE_ENABLE ? 1 : 0),
                    janusHash16("boot"), janusHash16("blind_eye"), 15000UL);
  motionBaseSendStatusEvent(true);
  motionBaseSendPowerPacket(true);
  janusEyeEmitTaskDone(90, "eye_boot_ready",
                       (int16_t)(tmos_ready ? 1 : 0),
                       (int16_t)(motionBasePresent ? 1 : 0),
                       motionBaseBusMv,
                       (int16_t)(motionBasePowerPresent ? 1 : 0));
  janusEyeSwarmSenseTick(millis(), true);
}

void loop() {
  M5.update();
  handleRoboZombieSerial();
  unsigned long now = millis();

  if (now - lastSensorAt >= janusPolicySensorIntervalMs) {
    lastSensorAt = now;
    readSensors();
  }

  // HTTP legacy fully disabled: no sendTelemetry(), no fetchCommand().
  // Blind Eye communicates with Beacon/Buzz only through ESP-NOW colony packets.

  if (now - lastDebugAt >= HEADLESS_DEBUG_INTERVAL_MS) {
    lastDebugAt = now;
    printHeadlessStatus();
  }

  if (now - lastSaveAt >= SAVE_INTERVAL_MS) {
    lastSaveAt = now;
    saveModel();
    saveState();
    saveKenshiState();
    saveTachyonState();
  }

  colonyTick();
  motionBaseTick();
  motionBaseSendPowerPacket(false);
  janusEventTick(false);
  janusEyeSwarmSenseTick(now, false);
  rfLiteDebugTick(now, false);

  if (WiFi.status() != WL_CONNECTED) {
    initWiFi(false);
  }

  delay(1);
}
