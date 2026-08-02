void janusEventTick(bool force) {
#if JANUS_EVENT_BUS_ENABLE
  uint32_t now = millis();

  if (force || now - janusEventLastStatusMs >= janusEventIntervalNow()) {
    janusEventLastStatusMs = now;
    uint8_t conf = (uint8_t)constrain((int)((tachyonPresenceConfidence * 0.5f + tachyonMotionConfidence * 0.5f) * 100.0f), 10, 100);
    janusEmitEyeEvent(JE_HEARTBEAT, conf, kenshiPriority,
                      (int16_t)constrain((int)(tmos_presence * 10.0f), -32768, 32767),
                      (int16_t)constrain((int)(tmos_motion * 10.0f), -32768, 32767),
                      (int16_t)constrain((int)(tachyonFutureStress * 100.0f), -32768, 32767),
                      (int16_t)constrain((int)(motionBaseServoAngle * 10), -32768, 32767),
                      janusHash16("home_eye"), janusHash16("blind_eye"), 8000UL);
  }

  if (tmos_motion_now && now - janusEventLastMotionMs >= JANUS_EVENT_MOTION_COOLDOWN_MS) {
    janusEventLastMotionMs = now;
    janusEmitEyeEvent(JE_MOTION, (uint8_t)constrain((int)(tachyonMotionConfidence * 100.0f), 30, 100), 86,
                      (int16_t)constrain((int)(tmos_motion * 10.0f), -32768, 32767),
                      (int16_t)constrain((int)(tachyonPredMotion1 * 10.0f), -32768, 32767),
                      (int16_t)motionBaseTargetAngle,
                      (int16_t)kenshiPredSector,
                      janusHash16("motion"), janusHash16("blind_eye_tmos"), 4500UL);
  }

  if (tmos_presence_now && now - janusEventLastPresenceMs >= JANUS_EVENT_MOTION_COOLDOWN_MS) {
    janusEventLastPresenceMs = now;
    janusEmitEyeEvent(JE_PRESENCE, (uint8_t)constrain((int)(tachyonPresenceConfidence * 100.0f), 30, 100), 78,
                      (int16_t)constrain((int)(tmos_presence * 10.0f), -32768, 32767),
                      (int16_t)constrain((int)(tachyonPredPresence1 * 10.0f), -32768, 32767),
                      (int16_t)motionBaseTargetAngle,
                      (int16_t)kenshiSector,
                      janusHash16("presence"), janusHash16("blind_eye_tmos"), 4500UL);
  }

  if (wifi_rssi != -127 && wifi_rssi < -74) {
    static uint32_t lastWeak = 0;
    if (now - lastWeak > 12000UL) {
      lastWeak = now;
      janusEmitEyeEvent(JE_WIFI_WEAK, 80, 52, (int16_t)(wifi_rssi * 10), 0, 0, 0,
                        janusHash16("radio"), janusHash16("blind_eye_wifi"), 10000UL);
    }
  }

  if (rf_ready && (rf_motion_now || rf_presence_now)) {
    static uint32_t lastRfEvent = 0;
    if (now - lastRfEvent > 1800UL) {
      lastRfEvent = now;
      uint8_t rfConf = (uint8_t)constrain((int)(rf_presence_score * 64.0f + rf_motion_energy * 4.0f), 25, 100);
      uint8_t rfUrg = rf_motion_now ? 70 : 48;
      janusEmitEyeEvent(rf_motion_now ? JE_MOTION : JE_PRESENCE, rfConf, rfUrg,
                        (int16_t)constrain((int)(rf_presence_score * 1000.0f), -32768, 32767),
                        (int16_t)constrain((int)(rf_motion_energy * 100.0f), -32768, 32767),
                        (int16_t)constrain((int)(rf_abs_drift * 100.0f), -32768, 32767),
                        (int16_t)constrain((int)(rf_entropy * 100.0f), -32768, 32767),
                        janusHash16("rf_eye"), janusHash16("blind_eye_ruview_lite"), 5000UL);
    }
  }

  if (ESP.getFreeHeap() < 65000) {
    static uint32_t lastHeap = 0;
    if (now - lastHeap > 15000UL) {
      lastHeap = now;
      janusEmitEyeEvent(JE_LOW_HEAP, 75, 55, (int16_t)(ESP.getFreeHeap() / 1024), 0, 0, 0,
                        janusHash16("heap"), janusHash16("blind_eye_heap"), 10000UL);
    }
  }

  janusEyeSemanticTick(now, force);
#endif
}

void janusEyeRecordEpisode(uint8_t eventType, uint8_t confidence, uint8_t urgency, uint8_t flags) {
#if JANUS_EYE_EPISODE_ENABLE
  uint32_t now = millis();
  JanusEyeEpisode& ep = janusEyeEpisodes[janusEyeEpisodeHead];
  ep.atMs = now;
  ep.eventType = eventType;
  ep.confidence = constrain((int)confidence, 0, 100);
  ep.urgency = constrain((int)urgency, 0, 100);
  ep.sector = kenshiSector;
  ep.predictedSector = kenshiPredSector;
  ep.flags = flags;
  ep.presence_x10 = (int16_t)constrain((int)(tmos_presence * 10.0f), -32768, 32767);
  ep.motion_x10 = (int16_t)constrain((int)(tmos_motion * 10.0f), -32768, 32767);
  ep.futureStress_x100 = (int16_t)constrain((int)(tachyonFutureStress * 100.0f), -32768, 32767);
  ep.servoAngle_x10 = (int16_t)constrain((int)(motionBaseServoAngle * 10), -32768, 32767);
  janusEyeEpisodeHead = (janusEyeEpisodeHead + 1) % JANUS_EYE_EPISODE_COUNT;
  if (janusEyeEpisodeCount < JANUS_EYE_EPISODE_COUNT) janusEyeEpisodeCount++;
  janusEyeLastEpisodeMs = now;
#else
  (void)eventType; (void)confidence; (void)urgency; (void)flags;
#endif
}

const JanusEyeEpisode* janusEyeLatestEpisode() {
#if JANUS_EYE_EPISODE_ENABLE
  if (janusEyeEpisodeCount == 0) return nullptr;
  uint8_t idx = (janusEyeEpisodeHead == 0) ? (JANUS_EYE_EPISODE_COUNT - 1) : (janusEyeEpisodeHead - 1);
  return &janusEyeEpisodes[idx];
#else
  return nullptr;
#endif
}

void janusEyeEmitAiMemory(uint32_t now, bool force) {
#if JANUS_EVENT_BUS_ENABLE && JANUS_EYE_EPISODE_ENABLE
  if (!force && now - janusEyeLastAiMemoryMs < JANUS_EYE_AI_MEMORY_TX_MS) return;
  const JanusEyeEpisode* ep = janusEyeLatestEpisode();
  if (!ep) return;
  janusEyeLastAiMemoryMs = now;
  bool ok = janusEmitEyeEvent(JE_AI_MEMORY, ep->confidence, ep->urgency,
                              ep->presence_x10, ep->motion_x10, ep->futureStress_x100,
                              (int16_t)(((uint16_t)ep->sector << 8) | ep->predictedSector),
                              janusHash16("eye_memory"), janusHash16("last_episode"), 60000UL);
  if (ok) janusEyeAiMemoryTx++;
#else
  (void)now; (void)force;
#endif
}

void janusEyeEmitTaskNeed(uint8_t urgency, const char* object, int16_t a, int16_t b, int16_t c, int16_t d) {
#if JANUS_EVENT_BUS_ENABLE
  bool ok = janusEmitEyeEvent(JE_TASK_NEED, 82, urgency, a, b, c, d,
                              janusHash16("eye_task_need"), janusHash16(object ? object : "eye_need"), 30000UL);
  if (ok) janusEyeTaskNeedTx++;
#else
  (void)urgency; (void)object; (void)a; (void)b; (void)c; (void)d;
#endif
}

void janusEyeEmitTaskDone(uint8_t confidence, const char* object, int16_t a, int16_t b, int16_t c, int16_t d) {
#if JANUS_EVENT_BUS_ENABLE
  bool ok = janusEmitEyeEvent(JE_TASK_DONE, confidence, 22, a, b, c, d,
                              janusHash16("eye_task_done"), janusHash16(object ? object : "eye_done"), 20000UL);
  if (ok) janusEyeTaskDoneTx++;
#else
  (void)confidence; (void)object; (void)a; (void)b; (void)c; (void)d;
#endif
}

void janusEyeSemanticTick(uint32_t now, bool force) {
#if JANUS_EVENT_BUS_ENABLE
  bool warmup = tmosWarmupActive(now);
  bool currentHot = !warmup && (tmos_presence_now || tmos_motion_now);
  bool wasHot = !warmup && (janusEyePrevPresenceNow || janusEyePrevMotionNow);

  if (!warmup && (tmos_motion_now || tmos_presence_now) &&
      (force || now - janusEyeLastEpisodeMs >= JANUS_EYE_EPISODE_RECORD_MS)) {
    uint8_t eventType = tmos_motion_now ? JE_MOTION : JE_PRESENCE;
    uint8_t conf = (uint8_t)constrain((int)((tachyonPresenceConfidence * 0.5f + tachyonMotionConfidence * 0.5f) * 100.0f), 30, 100);
    uint8_t urg = tmos_motion_now ? 86 : 78;
    uint8_t flags = 0;
    if (tmos_presence_now) flags |= 0x01;
    if (tmos_motion_now) flags |= 0x02;
    if (motionBasePresent) flags |= 0x04;
    if (motionBasePowerPresent) flags |= 0x08;
    if (rf_presence_now || rf_motion_now) flags |= 0x10;
    janusEyeRecordEpisode(eventType, conf, urg, flags);
  }

  if (wasHot && !currentHot && now - janusEyeLastTaskDoneMs >= JANUS_EYE_TASK_DONE_MS) {
    janusEyeLastTaskDoneMs = now;
    janusEyeEmitTaskDone(76, "eye_area_clear",
                         (int16_t)constrain((int)(tmos_occupancy * 100.0f), -32768, 32767),
                         (int16_t)constrain((int)(tmos_clear_score * 100.0f), -32768, 32767),
                         (int16_t)kenshiSector, (int16_t)kenshiPredSector);
  }

  bool ghostHigh = tmos_ghost_score >= JANUS_GHOST_TASKNEED_LEVEL; // legacy name; artifact/health score
  if (ghostHigh && !tmosGhostHighSinceMs) tmosGhostHighSinceMs = now;
  if (!ghostHigh) tmosGhostHighSinceMs = 0;

  if (!warmup && (force || now - janusEyeLastTaskNeedMs >= JANUS_EYE_TASK_NEED_MS)) {
    bool emittedNeed = false;
    bool ghostNeedAllowed = (tmos_bad_frame_streak >= JANUS_EYE_RECALIBRATE_BAD_FRAMES) ||
      (ghostHigh && tmosGhostHighSinceMs &&
       now - tmosGhostHighSinceMs >= JANUS_GHOST_TASKNEED_HOLD_MS &&
       now - tmosLastGhostTaskNeedMs >= JANUS_GHOST_TASKNEED_COOLDOWN_MS);
    if (ghostNeedAllowed) {
      janusEyeLastTaskNeedMs = now;
      tmosLastGhostTaskNeedMs = now;
      emittedNeed = true;
      janusEyeEmitTaskNeed(82, "eye_needs_recalibration",
                           (int16_t)tmos_bad_frame_streak,
                           (int16_t)constrain((int)(tmos_ghost_score * 100.0f), 0, 32767),
                           (int16_t)constrain((int)(tmos_focus_confidence * 100.0f), 0, 32767),
                           (int16_t)constrain((int)(tmos_occupancy * 100.0f), 0, 32767));
    } else if (tachyonFutureStress >= JANUS_EYE_QUIET_STRESS_LEVEL && !currentHot) {
      janusEyeLastTaskNeedMs = now;
      emittedNeed = true;
      janusEyeEmitTaskNeed(66, "eye_needs_quiet",
                           (int16_t)constrain((int)(tachyonFutureStress * 100.0f), 0, 32767),
                           (int16_t)constrain((int)(loss * 1000.0f), 0, 32767),
                           (int16_t)kenshiSector, (int16_t)kenshiPredSector);
    } else if (motionBasePresent && motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV && !(motionBasePowerFlags & 0x04)) {
      janusEyeLastTaskNeedMs = now;
      emittedNeed = true;
      janusEyeEmitTaskNeed(78, "motionbase_power_low", motionBaseBusMv, motionBaseCurrentRaw, motionBasePowerRaw, motionBaseServoAngle);
    } else if (motionBasePresent && !motionBasePowerPresent) {
      janusEyeLastTaskNeedMs = now;
      emittedNeed = true;
      janusEyeEmitTaskNeed(42, "motionbase_needs_power_monitor",
                           (int16_t)motionBasePresent, (int16_t)motionBasePowerPresent, (int16_t)motionBaseI2cErrors, 0);
    } else if ((motionBasePresent || motionBaseEverDetected) && motionBaseI2cErrors > 20) {
      janusEyeLastTaskNeedMs = now;
      emittedNeed = true;
      janusEyeEmitTaskNeed(62, "motionbase_i2c_errors",
                           (int16_t)constrain((int)motionBaseI2cErrors, 0, 32767), motionBaseBusMv, motionBaseCurrentRaw, 0);
    }
    (void)emittedNeed;
  }

  janusEyeEmitAiMemory(now, force);
  janusEyePrevPresenceNow = tmos_presence_now;
  janusEyePrevMotionNow = tmos_motion_now;
#else
  (void)now; (void)force;
#endif
}

