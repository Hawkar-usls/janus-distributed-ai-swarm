void janusEyeSwarmSenseTick(uint32_t now, bool force) {
#if JANUS_EYE_SWARMSENSE_ENABLE
  uint32_t interval = (tmos_motion_now || tmos_presence_now || rf_motion_now || rf_presence_now || tachyonFutureStress > 0.90f || kenshiPriority >= 120)
                      ? JANUS_EYE_SWARMSENSE_ALERT_MS
                      : JANUS_EYE_SWARMSENSE_TX_MS;
  if (!force && now - janusEyeLastSwarmSenseMs < interval) return;
  janusEyeLastSwarmSenseMs = now;

  SwarmSensePacket ss{};
  ss.magic[0] = 'S'; ss.magic[1] = 'S';
  ss.version = 1;
  ss.worker_id = colonyWorkerId;
  strlcpy(ss.nodeId, "BlindEye", sizeof(ss.nodeId));
  strlcpy(ss.kind, "eye_tmos_rf", sizeof(ss.kind));
  ss.seq = ++janusEyeSwarmSenseSeq;
  ss.uptime_ms = now;
  ss.micros_tail = micros();
  ss.free_heap = ESP.getFreeHeap();
  ss.loop_jitter_us = (uint16_t)constrain((int32_t)(now - lastSensorAt), 0L, 65535L);
  ss.loop_max_us = (uint16_t)constrain((int32_t)janusPolicySensorIntervalMs, 0L, 65535L);
  ss.rssi = (WiFi.status() == WL_CONNECTED) ? (int8_t)WiFi.RSSI() : -127;
  ss.radio_mode = janusPolicyRadioRate;
  ss.bt_flags = 0;
  if (janusPolicyRx && now < janusPolicyUntilMs) ss.bt_flags |= 0x01;
  if (tmos_presence_now) ss.bt_flags |= 0x02;
  if (tmos_motion_now) ss.bt_flags |= 0x04;
  if (motionBasePresent) ss.bt_flags |= 0x08;
  if (motionBasePowerPresent) ss.bt_flags |= 0x10;
  if (motionBasePowerPresent && motionBaseBatteryPct <= 20) ss.bt_flags |= 0x80;
  if (motionBaseArmed) ss.bt_flags |= 0x20;
  if (tmos_ready || eyeVisionEnabled || rf_ready) ss.bt_flags |= 0x40;
  if (tmosWarmupActive(now)) ss.bt_flags |= 0x04;
  if ((!tmosWarmupActive(now) && tmos_ghost_score > 0.70f) || rf_entropy > JANUS_RF_LITE_ANOMALY_LEVEL) ss.bt_flags |= 0x80;
  ss.palette = eyeVisionEnabled ? 1 : (tmos_ready ? 3 : (rf_ready ? 2 : 0));
  ss.knn_label = kenshiSector;
  ss.knn_confidence = (uint8_t)constrain((int)((kenshiConfidence * 0.72f + rf_presence_score * 0.28f) * 100.0f), 0, 100);
  ss.ai_hint = (tachyonFutureStress > 0.90f || kenshiPriority > 140 || rf_entropy > JANUS_RF_LITE_ANOMALY_LEVEL) ? 3 : ((loss > 0.25f) ? 2 : 1);
  ss.thermal_load = (uint8_t)constrain((int)roundf(imu_temp), 0, 100);
  ss.effective_batch = effectiveColonyRemoteBatch();
  ss.dynamic_batch = effectiveColonyRemoteBatch();
  ss.hash_rate = colonyRemoteHashrate;
  ss.total_hashes = colonyJob.active ? colonyJob.hashesDone : colonyJobsDone;
  ss.best_bits = colonyBestBits;
  ss.hash_eff_x1000 = (uint16_t)constrain((int32_t)(sync_hint * 1000.0f), 0L, 65535L);
  ss.prediction_error_x1000 = (int16_t)constrain((int32_t)((loss + rf_abs_drift * 0.015f) * 1000.0f), -32768L, 32767L);
  ss.entropy_x1000 = (uint16_t)constrain((int32_t)(eyeLocalEntropy() * 100.0f), 0L, 65535L);
  ss.touch_delta = (uint16_t)constrain((int32_t)max(max(tmos_presence, tmos_motion), rf_presence_score * 100.0f + rf_motion_energy * 12.0f), 0L, 65535L);
  ss.job_age_s = colonyJob.active ? (uint16_t)min(65535UL, (now - colonyJob.receivedAt) / 1000UL) : 65535U;
  ss.nonce_remaining_l16 = (colonyJob.active && colonyJob.rangeSize > colonyJob.hashesDone) ? (uint16_t)((colonyJob.rangeSize - colonyJob.hashesDone) & 0xFFFF) : 0;
  ss.flags = ((uint16_t)(motionBasePresent ? 1 : 0) << 15) |
             ((uint16_t)(motionBasePowerPresent ? 1 : 0) << 14) |
             ((uint16_t)(eyeVisionEnabled ? 1 : 0) << 13) |
             ((uint16_t)(tmos_ready ? 1 : 0) << 12) |
             ((uint16_t)kenshiJobState << 8) |
             (uint16_t)(kenshiPredSector & 0xFF);

  if (janusEyeEspNowSend("S/S", &ss, sizeof(ss), true)) janusEyeSwarmSenseTx++;
  else janusEyeSwarmSenseFail++;
#else
  (void)now; (void)force;
#endif
}

bool motionI2cWrite8(uint8_t addr, uint8_t reg, uint8_t value) {
  if (!janusSelectMotionBus()) { motionBaseI2cErrors++; return false; }
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(value);
  uint8_t err = Wire.endTransmission();
  if (err) motionBaseI2cErrors++;
  janusSelectGroveBus();
  return err == 0;
}

bool motionI2cRead16(uint8_t addr, uint8_t reg, uint16_t& value) {
  if (!janusSelectMotionBus()) { motionBaseI2cErrors++; return false; }
  Wire.beginTransmission(addr);
  Wire.write(reg);
  uint8_t err = Wire.endTransmission(false);
  if (err) { motionBaseI2cErrors++; janusSelectGroveBus(); return false; }
  uint8_t got = Wire.requestFrom((int)addr, 2);
  if (got != 2) { motionBaseI2cErrors++; janusSelectGroveBus(); return false; }
  value = ((uint16_t)Wire.read() << 8) | Wire.read();
  janusSelectGroveBus();
  return true;
}

bool motionI2cProbeOnSelectedBus(uint8_t addr) {
  Wire.beginTransmission(addr);
  uint8_t err = Wire.endTransmission();
  return err == 0;
}

bool motionI2cProbe(uint8_t addr) {
  if (!janusSelectMotionBus()) { motionBaseI2cErrors++; return false; }
  bool ok = motionI2cProbeOnSelectedBus(addr);
  janusSelectGroveBus();
  return ok;
}

void motionBaseScanCurrentBus(const char* name) {
  Serial.printf("[MOTION/I2C] scan %s:", name ? name : "bus");
  bool any = false;
  for (uint8_t a = 0x08; a <= 0x77; ++a) {
    if (motionI2cProbeOnSelectedBus(a)) {
      Serial.printf(" 0x%02X", (unsigned)a);
      any = true;
    }
    delay(1);
  }
  if (!any) Serial.print(" none");
  Serial.println();
}

void motionBaseScanBuses() {
  if (janusSelectMotionBus(true)) motionBaseScanCurrentBus("MotionBase SDA38/SCL39");
  if (janusSelectGroveBus(true)) motionBaseScanCurrentBus("Grove/TMOS SDA2/SCL1");
}


bool motionBasePowerOkForActuators() {
#if JANUS_MOTION_BASE_ENABLE
  if (!motionBasePowerPresent) return true;
  if (motionBaseBusMv <= 0) return true;
  if (motionBasePowerFlags & 0x04) return true; // USB/external rail present
  return motionBaseBusMv >= JANUS_MOTION_BASE_SLEEP_MV;
#else
  return false;
#endif
}

bool motionBaseWriteServoAngle(uint8_t ch, uint8_t angle, const char* tag) {
#if JANUS_MOTION_BASE_ENABLE && JANUS_MOTION_BASE_WRITE_ENABLE
  if (!motionBasePresent) return false;
  if (!motionBasePowerOkForActuators()) return false;
  uint8_t reg = 0x00 + (ch & 0x03);
  bool ok = motionI2cWrite8(JANUS_MOTION_BASE_I2C_ADDR, reg, (uint8_t)constrain((int)angle, 0, 180));
  if (ok) motionBaseServoWrites++;
  else Serial.printf("[ROBOZOMBIE] servo write FAIL tag=%s ch=S%u val=%u i2cErr=%lu\n",
                     tag ? tag : "servo", (unsigned)((ch & 0x03) + 1), (unsigned)angle,
                     (unsigned long)motionBaseI2cErrors);
  return ok;
#else
  (void)ch; (void)angle; (void)tag;
  return false;
#endif
}

bool roboZombieAnyLegPresent() {
#if JANUS_ROBOZOMBIE_ENABLE
  return roboZombieLeftLegPresent || roboZombieRightLegPresent;
#else
  return false;
#endif
}

const char* roboZombieBodyModeName() {
#if JANUS_ROBOZOMBIE_ENABLE
  // v2.14C: configured servos are not enough. If the ATOMIC Motion Base is absent,
  // the eye is intentionally a sensor-only node and must not pretend to be FULL.
  if (!motionBasePresent) return "BASELESS_SENSOR";
  if (roboZombieHeadPresent && roboZombieAnyLegPresent()) return "FULL";
  if (roboZombieHeadPresent && !roboZombieAnyLegPresent()) return "HEAD_ONLY";
  if (!roboZombieHeadPresent && roboZombieAnyLegPresent()) return "CRAWLER_ONLY";
  return "SENSOR_ONLY";
#else
  return "OFF";
#endif
}

uint8_t roboZombieSpeedToServoValue(int8_t speed, bool reverse) {
  speed = (int8_t)constrain((int)speed, -100, 100);
  if (reverse) speed = (int8_t)-speed;
  int delta = (int)roundf((float)speed * (float)JANUS_ROBOZOMBIE_MAX_PULL_DELTA / 100.0f);
  return (uint8_t)constrain(JANUS_ROBOZOMBIE_SERVO_STOP + delta, 0, 180);
}

float roboZombieComputeConfidence() {
#if JANUS_ROBOZOMBIE_ENABLE
  float tmos = constrain(max(tmos_presence_memory, tmos_motion_memory), 0.0f, 2.0f);
  float rf = rf_ready ? constrain(rfLiteFusionScore() / 1.8f, 0.0f, 1.8f) : 0.0f;
  float tach = constrain(tachyonFutureStress * 0.65f + tachyonPredMotion1 / 95.0f, 0.0f, 1.6f);
  float bubble = constrain(kenshiConfidence + (kenshiBubbleState >= 2 ? 0.35f : 0.0f), 0.0f, 1.4f);
  float nowHit = (tmos_motion_now || tmos_presence_now || rf_motion_now || rf_presence_now) ? 0.45f : 0.0f;
  float raw = tmos * 0.35f + rf * 0.28f + tach * 0.22f + bubble * 0.15f + nowHit;
  roboZombieGaitConfidence = constrain(roboZombieGaitConfidence * 0.78f + raw * 0.22f, 0.0f, 2.4f);
  if (roboZombieGaitConfidence > 0.55f) roboZombieLastConfidentMs = millis();
  return roboZombieGaitConfidence;
#else
  return 0.0f;
#endif
}

void motionBaseStopCrawler(const char* reason) {
#if JANUS_ROBOZOMBIE_ENABLE
  roboZombieLastLeftSpeed = 0;
  roboZombieLastRightSpeed = 0;
  roboZombieLastLeftValue = JANUS_ROBOZOMBIE_SERVO_STOP;
  roboZombieLastRightValue = JANUS_ROBOZOMBIE_SERVO_STOP;
#if JANUS_MOTION_BASE_WRITE_ENABLE
  if (motionBasePresent && motionBasePowerOkForActuators()) {
    if (roboZombieLeftLegPresent) motionBaseWriteServoAngle(JANUS_ROBOZOMBIE_LEFT_SERVO_CH, JANUS_ROBOZOMBIE_SERVO_STOP, "crawl-stop-L");
    if (roboZombieRightLegPresent) motionBaseWriteServoAngle(JANUS_ROBOZOMBIE_RIGHT_SERVO_CH, JANUS_ROBOZOMBIE_SERVO_STOP, "crawl-stop-R");
  }
#endif
  roboZombieCrawlerLastStopMs = millis();
  if (reason && reason[0] && (motionBasePresent || !motionBaseOptionalAbsent)) Serial.printf("[ROBOZOMBIE] crawler stop reason=%s mode=%s Lp=%u Rp=%u\n",
                                         reason, roboZombieBodyModeName(),
                                         roboZombieLeftLegPresent ? 1 : 0,
                                         roboZombieRightLegPresent ? 1 : 0);
#else
  (void)reason;
#endif
}

void motionBaseCrawlerWriteSpeeds(int8_t leftSpeed, int8_t rightSpeed, const char* tag) {
#if JANUS_ROBOZOMBIE_ENABLE
  // If one leg is missing, the other one may still pulse gently; if both are missing,
  // this becomes a harmless no-op and the rest of BlindEye keeps running.
  if (!roboZombieLeftLegPresent) leftSpeed = 0;
  if (!roboZombieRightLegPresent) rightSpeed = 0;

  roboZombieLastLeftSpeed = leftSpeed;
  roboZombieLastRightSpeed = rightSpeed;
  roboZombieLastLeftValue = roboZombieSpeedToServoValue(leftSpeed, roboZombieLeftReverse);
  roboZombieLastRightValue = roboZombieSpeedToServoValue(rightSpeed, roboZombieRightReverse);
#if JANUS_MOTION_BASE_WRITE_ENABLE
  if (motionBasePresent && motionBasePowerOkForActuators()) {
    if (roboZombieLeftLegPresent) motionBaseWriteServoAngle(JANUS_ROBOZOMBIE_LEFT_SERVO_CH, roboZombieLastLeftValue, "crawl-L");
    if (roboZombieRightLegPresent) motionBaseWriteServoAngle(JANUS_ROBOZOMBIE_RIGHT_SERVO_CH, roboZombieLastRightValue, "crawl-R");
  }
#endif
  Serial.printf("[ROBOZOMBIE] pulse tag=%s mode=%s Lp=%u L=%d val=%u rev=%u Rp=%u R=%d val=%u rev=%u target=%d angle=%d pull=%u conf=%.2f\n",
                tag ? tag : "crawl", roboZombieBodyModeName(),
                roboZombieLeftLegPresent ? 1 : 0,
                (int)leftSpeed, (unsigned)roboZombieLastLeftValue, roboZombieLeftReverse ? 1 : 0,
                roboZombieRightLegPresent ? 1 : 0,
                (int)rightSpeed, (unsigned)roboZombieLastRightValue, roboZombieRightReverse ? 1 : 0,
                (int)motionBaseTargetAngle, (int)motionBaseServoAngle,
                (unsigned)roboZombieBasePull, roboZombieGaitConfidence);
#else
  (void)leftSpeed; (void)rightSpeed; (void)tag;
#endif
}

