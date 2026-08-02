void motionBasePlanTarget() {
#if JANUS_MOTION_BASE_ENABLE
  if (!motionBaseTrackEnabled) {
    motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
    return;
  }

  bool hot = tmos_motion_now || tmos_presence_now || tachyonPredMotion1 > 38.0f || tachyonFutureStress > 0.85f || kenshiBubbleState >= 2;
  if (hot) {
    uint8_t sector = (tachyonFutureStress > 0.75f) ? kenshiPredSector : kenshiSector;
    int16_t sectorAngle = motionBaseSectorToAngle(sector);
    float memory = constrain(max(tmos_motion_memory, tmos_presence_memory), 0.0f, 1.0f);
    float alpha = constrain(0.22f + memory * 0.38f + tachyonFutureStress * 0.14f, 0.20f, 0.74f);
    motionBaseTargetAngle = (int16_t)constrain((int)roundf(motionBaseTargetAngle * (1.0f - alpha) + sectorAngle * alpha),
                                               JANUS_MOTION_BASE_TRACK_MIN_DEG, JANUS_MOTION_BASE_TRACK_MAX_DEG);
  } else {
    // No target: relax toward center slowly.
    motionBaseTargetAngle = (int16_t)roundf(motionBaseTargetAngle * 0.96f + JANUS_MOTION_BASE_TRACK_CENTER_DEG * 0.04f);
  }

  if (motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV && !(motionBasePowerFlags & 0x04)) {
    // Low battery: do not chase aggressively.
    motionBaseTargetAngle = (int16_t)roundf(motionBaseTargetAngle * 0.90f + JANUS_MOTION_BASE_TRACK_CENTER_DEG * 0.10f);
  }
#endif
}

void motionBaseTick() {
#if JANUS_MOTION_BASE_ENABLE
  uint32_t now = millis();

  if (!motionBasePresent) {
    // v2.14C: no ATOMIC Motion Base attached. Stay in штатный sensor-only mode.
    // Keep telemetry alive for Core2, but do not touch the missing actuator bus.
    motionBaseArmed = false;
    roboZombieCrawlerManualEnable = false;
    motionBaseServoAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
    motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
    motionBaseSendPowerPacket(false);
    motionBaseSendStatusEvent(false);
    return;
  }

  if (now - motionBaseLastPowerMs >= JANUS_MOTION_BASE_POWER_MS) {
    motionBaseLastPowerMs = now;
    motionBaseReadPower();
  }

  if (now - motionBaseLastTickMs < JANUS_MOTION_BASE_TICK_MS) return;
  motionBaseLastTickMs = now;

  if (roboZombiePassiveMode) {
    motionBaseTargetAngle = motionBaseServoAngle;
    // Keep passive mode quiet: do not spam stop writes/logs every 80 ms.
    if (roboZombieCrawlerManualEnable ||
        roboZombieLastLeftValue != JANUS_ROBOZOMBIE_SERVO_STOP ||
        roboZombieLastRightValue != JANUS_ROBOZOMBIE_SERVO_STOP) {
      roboZombieCrawlerManualEnable = false;
      motionBaseStopCrawler("passive-s");
    }
    motionBaseSendStatusEvent(false);
    return;
  }

  motionBasePlanTarget();

  int16_t diff = motionBaseTargetAngle - motionBaseServoAngle;
  if (diff > JANUS_MOTION_BASE_MAX_STEP_DEG) diff = JANUS_MOTION_BASE_MAX_STEP_DEG;
  if (diff < -JANUS_MOTION_BASE_MAX_STEP_DEG) diff = -JANUS_MOTION_BASE_MAX_STEP_DEG;
  motionBaseServoAngle = constrain((int)(motionBaseServoAngle + diff), JANUS_MOTION_BASE_TRACK_MIN_DEG, JANUS_MOTION_BASE_TRACK_MAX_DEG);

#if JANUS_MOTION_BASE_WRITE_ENABLE
  bool powerOk = motionBasePowerOkForActuators();
  bool headAllowed = motionBasePresent && !roboZombiePassiveMode && roboZombieHeadPresent && (motionBaseArmed || roboZombieLocalArm) && powerOk;
  if (headAllowed && abs(motionBaseServoAngle - motionBaseLastSentAngle) >= 1) {
    if (motionBaseWriteServoAngle(JANUS_MOTION_BASE_TRACK_SERVO_CH, (uint8_t)constrain((int)motionBaseServoAngle, 0, 180), "head-S1")) {
      motionBaseLastSentAngle = motionBaseServoAngle;
    }
  } else if (!roboZombieHeadPresent) {
    // Modular build: no head servo installed. Planner still runs for Core2 telemetry and leg steering.
    motionBaseLastSentAngle = motionBaseServoAngle;
  }
#else
  // Dry-run: planner runs, no physical write.
  motionBaseLastSentAngle = motionBaseServoAngle;
#endif

  motionBaseCrawlerTick(now);
  motionBaseSendStatusEvent(false);
#endif
}

void motionBaseSendStatusEvent(bool force) {
#if JANUS_MOTION_BASE_ENABLE && JANUS_EVENT_BUS_ENABLE
  uint32_t now = millis();
  if (!force && now - motionBaseLastStatusMs < JANUS_MOTION_BASE_STATUS_MS) return;
  motionBaseLastStatusMs = now;

  uint8_t conf = motionBasePresent ? 82 : 78;
  uint8_t urg = motionBasePresent ? 18 : 4;
  uint8_t eventType = motionBasePresent ? JE_ENV : JE_SAFE;
  uint16_t topic = motionBasePresent ? janusHash16("motion_base") : janusHash16("motion_base_absent");
  uint16_t object = motionBasePresent ? janusHash16("blind_eye_pan") : janusHash16("sensor_only");
  if (motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV && !(motionBasePowerFlags & 0x04)) urg = 72;
  if (motionBasePowerPresent && motionBaseBusMv > 0 && motionBaseBusMv < JANUS_MOTION_BASE_SLEEP_MV && !(motionBasePowerFlags & 0x04)) urg = 90;
  if (urg >= 72) eventType = JE_DANGER;

  janusEmitEyeEvent(eventType, conf, urg,
                    (int16_t)(motionBaseServoAngle * 10),
                    (int16_t)(motionBaseTargetAngle * 10),
                    motionBaseBusMv,
                    motionBaseCurrentRaw,
                    topic, object, motionBasePresent ? 9000UL : 20000UL);
#endif
}

// ========================= JANUS KENSHI BUBBLE BUS =========================
// Inspired by the "visible bubble / virtual world" idea: ESP-NOW remains simple,
// but the Eye stops thinking every peer must be fully simulated every tick.
// Quiet peers are compressed into virtual timers and a few world-state flags.
// Hot peers/events materialize into active bubble packets.

enum JanusKenshiWorldFlags : uint32_t {
  K_WORLD_PRESENCE = 1UL << 0,
  K_WORLD_MOTION   = 1UL << 1,
  K_WORLD_MIC      = 1UL << 2,
  K_WORLD_SHOCK    = 1UL << 3,
  K_WORLD_MASTER   = 1UL << 4,
  K_WORLD_JOB      = 1UL << 5,
  K_WORLD_AGENT    = 1UL << 6,
  K_WORLD_UNSTABLE = 1UL << 7,
  K_WORLD_TRAINING = 1UL << 8,
  K_WORLD_LOW_SYNC = 1UL << 9,
  K_WORLD_RF       = 1UL << 10
};

uint8_t kenshiFindNodeSlot(const char* nodeId) {
  if (!nodeId || !nodeId[0]) nodeId = "node";
  for (uint8_t i = 0; i < JANUS_KENSHI_MAX_NODES; ++i) {
    if (kenshiNodes[i].active && strncmp(kenshiNodes[i].nodeId, nodeId, sizeof(kenshiNodes[i].nodeId)) == 0) return i;
  }

  uint8_t freeSlot = 255;
  uint8_t oldestSlot = 0;
  uint32_t oldestAge = 0;
  uint32_t now = millis();

  for (uint8_t i = 0; i < JANUS_KENSHI_MAX_NODES; ++i) {
    if (!kenshiNodes[i].active && freeSlot == 255) freeSlot = i;
    uint32_t age = now - kenshiNodes[i].lastSeenMs;
    if (age > oldestAge) {
      oldestAge = age;
      oldestSlot = i;
    }
  }
  return freeSlot != 255 ? freeSlot : oldestSlot;
}

void kenshiDecayAndCountNodes() {
  uint32_t now = millis();
  kenshiActiveNodes = 0;
  kenshiVirtualNodes = 0;
  float eSum = 0.0f;
  float wSum = 0.0f;

  for (uint8_t i = 0; i < JANUS_KENSHI_MAX_NODES; ++i) {
    JanusKenshiNode& n = kenshiNodes[i];
    if (!n.active) continue;
    uint32_t age = now - n.lastSeenMs;
    if (age > JANUS_KENSHI_VIRTUAL_TTL_MS) {
      n.active = false;
      continue;
    }

    bool hot = (age <= JANUS_KENSHI_ACTIVE_TTL_MS) && (n.priority >= 80 || (n.flags & 0x03));
    if (hot) kenshiActiveNodes++;
    else kenshiVirtualNodes++;

    float w = constrain(1.0f - (float)age / (float)JANUS_KENSHI_VIRTUAL_TTL_MS, 0.05f, 1.0f);
    w *= constrain(0.35f + n.confidence * 0.65f, 0.10f, 1.25f);
    eSum += n.entropy * w;
    wSum += w;
  }

  kenshiVirtualEntropy = (wSum > 0.001f) ? (eSum / wSum) : eyeLocalEntropy();
}

uint8_t kenshiInferSector() {
  static float lastPresence = 0.0f;
  static float lastMotion = 0.0f;
  static float drift = 0.0f;

  float dp = tmos_presence - lastPresence;
  float dm = tmos_motion - lastMotion;
  lastPresence = tmos_presence;
  lastMotion = tmos_motion;

  // Blind Eye has a single TMOS channel today, so direction is inferred from temporal
  // motion shape + IMU/mag phase. When Motion Base arrives, this same sector index
  // can drive servo angle directly.
  drift = drift * 0.82f + (dm * 0.035f + dp * 0.012f + gyro_z * 0.04f + mag_norm * 0.001f) * 0.18f;

  float phase = drift + (float)(colonyAgentEntropySeed & 0xFF) * 0.003f + (float)(millis() & 0x3FF) * 0.0004f;
  int sector = (int)floorf(fmodf(fabsf(phase) * 1.37f + kenshiLastSector * 0.63f, (float)JANUS_KENSHI_SECTORS));
  sector = constrain(sector, 0, JANUS_KENSHI_SECTORS - 1);

  if (tmos_motion < 2.0f && tmos_presence < 4.0f) sector = kenshiLastSector; // no fake turning when nothing moves
  return (uint8_t)sector;
}

uint8_t kenshiPredictNextSector(uint8_t sector) {
  sector %= JANUS_KENSHI_SECTORS;
  float best = -1.0f;
  uint8_t bestIdx = sector;

  for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) {
    float v = kenshiMarkov[sector][j];
    if (j == sector) v += 0.04f; // inertia
    if (j == (uint8_t)((sector + 1) % JANUS_KENSHI_SECTORS)) v += constrain(tmos_motion / 900.0f, 0.0f, 0.10f);
    if (v > best) {
      best = v;
      bestIdx = j;
    }
  }

  return bestIdx;
}

void kenshiTrainMarkov(uint8_t from, uint8_t to, float strength) {
  from %= JANUS_KENSHI_SECTORS;
  to %= JANUS_KENSHI_SECTORS;
  strength = constrain(strength, 0.02f, 1.0f);

  for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) {
    kenshiMarkov[from][j] *= 0.992f;
  }
  kenshiMarkov[from][to] += 0.020f + strength * 0.050f;

  // Normalize row softly so it remains a probability-like memory.
  float s = 0.0f;
  for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) s += kenshiMarkov[from][j];
  if (s > 2.5f) {
    for (uint8_t j = 0; j < JANUS_KENSHI_SECTORS; ++j) kenshiMarkov[from][j] /= s;
  }
  kenshiStateDirty = true;
}

