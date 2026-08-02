String buildPayload() {
  StaticJsonDocument<1536> doc;
  doc["device_id"] = DEVICE_ID;
  JsonObject d = doc["data"].to<JsonObject>();

  d["kind"] = DEVICE_KIND;
  d["acc_x"] = acc_x;
  d["acc_y"] = acc_y;
  d["acc_z"] = acc_z;
  d["gyro_x"] = gyro_x;
  d["gyro_y"] = gyro_y;
  d["gyro_z"] = gyro_z;
  d["mag_x"] = mag_x;
  d["mag_y"] = mag_y;
  d["mag_z"] = mag_z;
  d["mag_norm"] = mag_norm;
  d["temp"] = imu_temp;
  d["shock"] = imu_shock;
  d["tmos_presence"] = tmos_presence;
  d["tmos_motion"] = tmos_motion;
  d["tmos_ready"] = tmos_ready;
  d["tmos_hw_presence"] = tmos_hw_presence_flag;
  d["tmos_hw_motion"] = tmos_hw_motion_flag;
  d["tmos_clear"] = tmos_clear_score;
  d["tmos_artifact"] = tmos_ghost_score;
  d["camera_present"] = (bool)JANUS_EYE_CAMERA_PRESENT;
  d["primary_eye"] = "TMOS_PIR";
  d["mic_rms"] = mic_rms;
  d["wifi_rssi"] = wifi_rssi;
  d["rf_ready"] = rf_ready;
  d["rf_presence_now"] = rf_presence_now;
  d["rf_motion_now"] = rf_motion_now;
  d["rf_presence_score"] = rf_presence_score;
  d["rf_motion_energy"] = rf_motion_energy;
  d["rf_abs_drift"] = rf_abs_drift;
  d["rf_noise"] = rf_rssi_noise;
  d["rf_entropy"] = rf_entropy;
  d["rf_packets"] = rf_rx_packets;
  d["calibrated"] = calibrated;
  d["uptime_ms"] = millis();
  d["activity"] = activity;
  d["pred_activity"] = pred_activity;
  d["loss"] = loss;
  d["fit"] = fit;
  d["fit_best"] = fit_best;
  d["model_lr"] = model_lr;
  d["z_activity"] = z_activity;
  d["z_loss"] = z_loss;
  d["sync_hint"] = sync_hint;
  d["tachyon_pred_presence_1"] = tachyonPredPresence1;
  d["tachyon_pred_motion_1"] = tachyonPredMotion1;
  d["tachyon_pred_presence_3"] = tachyonPredPresence3;
  d["tachyon_pred_motion_3"] = tachyonPredMotion3;
  d["tachyon_eta_ms"] = tachyonEventEtaMs;
  d["tachyon_conf_p"] = tachyonPresenceConfidence;
  d["tachyon_conf_m"] = tachyonMotionConfidence;
  d["tachyon_stress"] = tachyonFutureStress;
  d["tachyon_swarm_pressure"] = tachyonSwarmPressure;
  d["aperture_stream"] = eyeVisionEnabled;
  d["vision_enabled"] = eyeVisionEnabled; // legacy key: means TMOS aperture stream, not camera
  d["vision_frames"] = eyeVisionFramesTx;
  d["vision_event_frames"] = eyeVisionEventFramesTx;
  d["motion_base_present"] = motionBasePresent;
  d["motion_base_power"] = motionBasePowerPresent;
  d["motion_base_armed"] = motionBaseArmed;
  d["motion_base_servo_angle"] = motionBaseServoAngle;
  d["motion_base_target_angle"] = motionBaseTargetAngle;
  d["motion_base_bus_mv"] = motionBaseBusMv;
  d["motion_base_battery_pct"] = motionBaseBatteryPct;
  d["motion_base_power_flags"] = motionBasePowerFlags;
  d["motion_base_i2c_errors"] = motionBaseI2cErrors;
  d["janus_policy_mood"] = janusPolicyMood;
  d["janus_policy_order"] = janusPolicyOrder;
  d["status"] = statusLine;
  d["diag"] = diagLine;
  d["colony_mode"] = colonyMode;
  d["colony_hashrate"] = colonyRemoteHashrate;
  d["colony_tickets"] = colonyRemoteShares;
  d["agent_rewards"] = colonyAgentRewardsRx;
  d["agent_aok"] = colonyAgentShareRewardsRx;
  d["agent_points"] = colonyAgentRewardPoints;
  d["agent_level"] = colonyAgentLevel;
  d["agent_hint"] = colonyAgentHint;
  d["agent_score"] = colonyAgentScore;
  d["agent_pred_hash"] = colonyAgentPredictedHash;
  d["agent_pred_err"] = colonyAgentPredictionError;
  d["agent_batch"] = effectiveColonyRemoteBatch();
  d["kenshi_state"] = kenshiBubbleState;
  d["kenshi_job"] = kenshiJobState;
  d["kenshi_sector"] = kenshiSector;
  d["kenshi_next"] = kenshiPredSector;
  d["kenshi_priority"] = kenshiPriority;
  d["kenshi_conf"] = kenshiConfidence;
  d["kenshi_active"] = kenshiActiveNodes;
  d["kenshi_virtual"] = kenshiVirtualNodes;
  d["kenshi_rx"] = kenshiRxPackets;
  d["kenshi_tx"] = kenshiTxPackets;

  String out;
  serializeJson(doc, out);
  return out;
}

void sendTelemetry() {
  // JANUS v2.2: HTTP removed. ESP-NOW only.
}



void applyCommand(const String& cmd) {
  if (cmd == "CALIBRATE") calibrateTMOS();
  else if (cmd == "PING") sendTelemetry();
  else if (cmd == "REBOOT") ESP.restart();
}

void fetchCommand() {
  // JANUS v2.2: HTTP removed. ESP-NOW only.
}






// ========================= JANUS BLACKBOARD EVENT BUS + MOTION BASE =========================

uint16_t janusHash16(const char* s) {
  uint16_t h = 21661U;
  if (!s) return h;
  while (*s) {
    h ^= (uint8_t)*s++;
    h = (uint16_t)(h * 16719U);
  }
  return h ? h : 1;
}

uint16_t janusEyeCapabilities() {
  uint16_t caps = JC_IMU | JC_MIC | JC_TMOS | JC_VISION | JC_AI | JC_RF;
  if (motionBasePresent) caps |= JC_RELAY;     // actuator base attached
  if (motionBasePowerPresent) caps |= JC_BATTERY;
  return caps;
}

const char* janusEyeEventKind(uint8_t eventType) {
  switch (eventType) {
    case JE_BOOT: return "eye_boot";
    case JE_HEARTBEAT: return "eye_status";
    case JE_ENV: return "eye_env";
    case JE_MOTION: return "eye_motion";
    case JE_PRESENCE: return "eye_presence";
    case JE_WIFI_WEAK: return "eye_wifi_weak";
    case JE_LOW_HEAP: return "eye_low_heap";
    case JE_TASK_NEED: return "eye_task_need";
    case JE_TASK_DONE: return "eye_task_done";
    case JE_DANGER: return "eye_danger";
    case JE_SAFE: return "eye_safe";
    case JE_AI_MEMORY: return "eye_memory";
    default: return "eye_tmos_motion";
  }
}

bool janusEmitEyeEvent(uint8_t eventType, uint8_t confidence, uint8_t urgency,
                       int16_t a_x10, int16_t b_x10, int16_t c_x10, int16_t d_x10,
                       uint16_t topicHash, uint16_t objectHash, uint32_t ttlMs) {
#if JANUS_EVENT_BUS_ENABLE
  JanusEventPacket ev{};
  ev.magic[0] = 'J'; ev.magic[1] = 'E';
  ev.version = 1;
  ev.eventType = eventType;
  ev.nodeRole = JR_BLIND;
  ev.confidence = constrain((int)confidence, 0, 100);
  ev.urgency = constrain((int)urgency, 0, 100);
  strlcpy(ev.nodeId, "BlindEye", sizeof(ev.nodeId));
  strlcpy(ev.kind, janusEyeEventKind(eventType), sizeof(ev.kind));
  ev.seq = ++janusEventSeq;
  ev.uptimeMs = millis();
  ev.topicHash = topicHash ? topicHash : janusHash16("blind_eye");
  ev.objectHash = objectHash;
  ev.capabilities = janusEyeCapabilities();
  ev.valueA_x10 = a_x10;
  ev.valueB_x10 = b_x10;
  ev.valueC_x10 = c_x10;
  ev.valueD_x10 = d_x10;
  ev.eventHash = ((uint32_t)eventType << 24) ^ ((uint32_t)ev.topicHash << 8) ^ ev.seq ^ (uint32_t)colonyWorkerId;
  ev.ttlMs = ttlMs ? ttlMs : 7000UL;
  bool ok = janusEyeEspNowSend("J/E", &ev, sizeof(ev), true);
  if (ok) janusEventLastTxMs = millis();
  return ok;
#else
  (void)eventType; (void)confidence; (void)urgency; (void)a_x10; (void)b_x10; (void)c_x10; (void)d_x10; (void)topicHash; (void)objectHash; (void)ttlMs;
  return false;
#endif
}

const char* janusMoodName(uint8_t mood) {
  switch (mood) {
    case JM_IDLE: return "IDLE";
    case JM_QUIET: return "QUIET";
    case JM_ALERT: return "ALERT";
    case JM_EXPLORE: return "EXPLORE";
    case JM_GUARD: return "GUARD";
    case JM_RECOVER: return "RECOVER";
    default: return "?";
  }
}

uint8_t janusPolicySmoothMood(uint8_t rawMood, uint8_t confidence, uint32_t now) {
  if (rawMood > JM_RECOVER) rawMood = JM_IDLE;
  if (janusPolicyRx <= 1) {
    janusPolicyCandidateMood = rawMood;
    janusPolicyCandidateCount = 1;
    janusPolicyRawLastMood = rawMood;
    janusPolicyLastMoodChangeMs = now;
    return rawMood;
  }

  if (rawMood == janusPolicyMood) {
    janusPolicyCandidateMood = rawMood;
    janusPolicyCandidateCount = 0;
    return janusPolicyMood;
  }

  if (rawMood != janusPolicyCandidateMood) {
    janusPolicyCandidateMood = rawMood;
    janusPolicyCandidateCount = 1;
  } else if (janusPolicyCandidateCount < 255) {
    janusPolicyCandidateCount++;
  }
  janusPolicyRawLastMood = rawMood;

  uint8_t needed = 1;
  if (rawMood == JM_ALERT || rawMood == JM_GUARD) needed = JANUS_POLICY_ALERT_CONFIRM;
  else if (rawMood == JM_RECOVER || rawMood == JM_QUIET) needed = JANUS_POLICY_RECOVER_CONFIRM;

  bool dwellOk = (now - janusPolicyLastMoodChangeMs) >= JANUS_POLICY_SMOOTH_MIN_DWELL_MS;
  bool strongOverride = (confidence >= 70 && (rawMood == JM_ALERT || rawMood == JM_GUARD));
  if ((janusPolicyCandidateCount >= needed && dwellOk) || strongOverride) {
    janusPolicyLastMoodChangeMs = now;
    janusPolicyAcceptedChanges++;
    janusPolicyCandidateCount = 0;
    return rawMood;
  }

  janusPolicySmoothedDrops++;
  return janusPolicyMood;
}

void onJanusPolicyPacket(const JanusPolicyPacket& jp) {
#if JANUS_EVENT_BUS_ENABLE
  if (jp.magic[0] != 'J' || jp.magic[1] != 'P' || jp.version != 1) return;
  if (jp.seq && jp.seq == janusPolicySeq) return;
  janusPolicyRx++;
  janusPolicySeq = jp.seq;
  uint32_t now = millis();
  uint8_t rawMood = constrain((int)jp.swarmMood, 0, (int)JM_RECOVER);
  uint8_t smoothedMood = janusPolicySmoothMood(rawMood, jp.confidence, now);
  bool moodAccepted = (smoothedMood == rawMood);
  janusPolicyMood = smoothedMood;
  if (moodAccepted || jp.confidence >= 65 || janusPolicyRx < 3) {
    janusPolicyRadioRate = constrain((int)jp.radioRate, 0, 2);
    janusPolicySensorRate = constrain((int)jp.sensorRate, 0, 2);
  }
  janusPolicyBuzzBudget = constrain((int)jp.buzzBudget, 0, 3);
  janusPolicyConfidence = constrain((int)jp.confidence, 0, 100);
  janusPolicyUntilMs = now + (jp.ttlMs ? jp.ttlMs : JANUS_EVENT_POLICY_TTL_GUARD_MS);
  janusPolicyQuietUntilMs = jp.quietUntilMs
    ? now + min((uint32_t)jp.quietUntilMs, 60000UL)
    : 0;
  strlcpy(janusPolicyOrder, jp.order[0] ? jp.order : "-", sizeof(janusPolicyOrder));

  if (janusPolicySensorRate == 0) janusPolicySensorIntervalMs = 220;
  else if (janusPolicySensorRate == 2) janusPolicySensorIntervalMs = 70;
  else janusPolicySensorIntervalMs = SENSOR_INTERVAL_MS;

  // Core can arm future tracker only when explicitly confident. Compile-time write gate still wins.
  motionBaseArmed = motionBasePresent && !roboZombiePassiveMode && (janusPolicyMood == JM_GUARD || janusPolicyMood == JM_ALERT) && jp.confidence >= 55 && !(jp.flags & 0x0001);
  Serial.printf("[EYE/POLICY] rx=%lu raw=%s mood=%s radio=%u sensor=%u conf=%u armed=%u smoothDrop=%lu accept=%lu order=%s\n",
                (unsigned long)janusPolicyRx, janusMoodName(rawMood), janusMoodName(janusPolicyMood),
                (unsigned)janusPolicyRadioRate, (unsigned)janusPolicySensorRate,
                (unsigned)janusPolicyConfidence, motionBaseArmed ? 1 : 0,
                (unsigned long)janusPolicySmoothedDrops, (unsigned long)janusPolicyAcceptedChanges, janusPolicyOrder);
#endif
}

uint32_t janusEventIntervalNow() {
  if (tmos_motion_now || tmos_presence_now || tachyonFutureStress > 0.90f || kenshiBubbleState >= 2) return JANUS_EVENT_TX_ALERT_MS;
  if (janusPolicyRadioRate == 0 || (janusPolicyQuietUntilMs && millis() < janusPolicyQuietUntilMs)) return JANUS_EVENT_TX_BASE_MS * 2UL;
  if (janusPolicyRadioRate == 2) {
    uint32_t fast = JANUS_EVENT_TX_BASE_MS / 2UL;
    return fast < 900UL ? 900UL : fast;
  }
  return JANUS_EVENT_TX_BASE_MS;
}

