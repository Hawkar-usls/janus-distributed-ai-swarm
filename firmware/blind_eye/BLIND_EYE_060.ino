void readTMOS() {
  uint32_t now = millis();
  if (!tmos_ready) {
    tmos_presence = 0.0f;
    tmos_motion = 0.0f;
    tmos_focus_confidence = 0.0f;
    tmos_presence_instant = false;
    tmos_motion_instant = false;
    tmos_presence_now = false;
    tmos_motion_now = false;
    tmos_hw_status_valid = false;
    tmos_occupancy *= JANUS_EYE_MEMORY_DECAY;
    tmos_presence_memory *= JANUS_EYE_MEMORY_DECAY;
    tmos_motion_memory *= JANUS_EYE_MEMORY_DECAY;
    tmos_clear_score *= JANUS_EYE_CLEAR_DECAY;
    tmos_ghost_score = constrain(tmos_ghost_score * (1.0f - JANUS_EYE_ARTIFACT_ATTACK) + JANUS_EYE_ARTIFACT_ATTACK,
                                 0.0f, 1.0f);
    if (tmos_bad_frame_streak < 0xFFFF) tmos_bad_frame_streak++;
    return;
  }

  int32_t pErr = tmos.getPresenceValue(&raw_presence);
  int32_t mErr = tmos.getMotionValue(&raw_motion);
  sths34pf80_tmos_func_status_t hwStatus{};
  int32_t sErr = tmos.getStatus(&hwStatus);
  tmos_last_read_error = pErr ? pErr : (mErr ? mErr : sErr);
  tmos_hw_status_valid = (sErr == 0);
  tmos_hw_presence_flag = tmos_hw_status_valid && (hwStatus.pres_flag != 0);
  tmos_hw_motion_flag = tmos_hw_status_valid && (hwStatus.mot_flag != 0);

  bool readInvalid = (pErr != 0) || (mErr != 0) ||
                     abs((int)raw_presence) >= JANUS_EYE_STUCK_RAW_ABS ||
                     abs((int)raw_motion) >= JANUS_EYE_STUCK_RAW_ABS;
  if (readInvalid) {
    if (tmos_bad_frames < 0xFFFF) tmos_bad_frames++;
    if (tmos_bad_frame_streak < 0xFFFF) tmos_bad_frame_streak++;
    tmos_presence = 0.0f;
    tmos_motion = 0.0f;
    tmos_focus_confidence *= 0.80f;
    tmos_presence_instant = false;
    tmos_motion_instant = false;
    tmos_presence_now = ((int32_t)(tmos_presence_hold_until_ms - now) > 0);
    tmos_motion_now = ((int32_t)(tmos_motion_hold_until_ms - now) > 0);
    tmos_occupancy *= JANUS_EYE_GHOST_DECAY;
    tmos_clear_score *= JANUS_EYE_CLEAR_DECAY;
    float badPressure = constrain((float)tmos_bad_frame_streak / max(1.0f, (float)JANUS_EYE_RECALIBRATE_BAD_FRAMES), 0.0f, 1.0f);
    tmos_ghost_score = constrain(tmos_ghost_score * (1.0f - JANUS_EYE_ARTIFACT_ATTACK) + badPressure * JANUS_EYE_ARTIFACT_ATTACK,
                                 0.0f, 1.0f);
    return;
  }

  tmos_bad_frame_streak = 0;
  tmos_last_valid_ms = now;

  if (!calibrated) {
    tmos_presence = 0.0f;
    tmos_motion = 0.0f;
    tmos_focus_confidence = 0.0f;
    tmos_presence_instant = false;
    tmos_motion_instant = false;
    tmos_presence_now = false;
    tmos_motion_now = false;
    return;
  }

#if JANUS_EYE_EAGLE_FOCUS_ENABLE
  if (!tmos_focus_ready) {
    tmos_presence_baseline = (float)calib_presence;
    tmos_motion_baseline = (float)calib_motion;
    tmos_presence_noise = 12.0f;
    tmos_motion_noise = 8.0f;
    tmos_focus_gain = 3.2f;
    tmos_focus_ready = true;
  }

  float rawP = (float)raw_presence;
  float rawM = (float)raw_motion;
  float dP = rawP - tmos_presence_baseline;
  float dM = rawM - tmos_motion_baseline;
  float aP = fabsf(dP);
  float aM = fabsf(dM);

  // Soft calibration: warmup follows the real room; after warmup a hot target
  // mostly freezes the baseline, while extreme jumps still get a slow release.
  bool warmup = tmosWarmupActive(now);
  float hotGate = max(16.0f, tmos_presence_noise * 2.3f + tmos_motion_noise * 1.4f);
  bool hugeJump = (max(aP, aM) > JANUS_TMOS_BASELINE_JUMP_LEVEL);
  bool hot = (max(aP, aM * 1.35f) > hotGate);
  float baseAlpha = 0.0f;
  if (warmup) {
    baseAlpha = hugeJump ? JANUS_TMOS_WARMUP_SETTLE_ALPHA : JANUS_TMOS_WARMUP_SOFT_ALPHA;
  } else {
    baseAlpha = hot ? (hugeJump ? JANUS_TMOS_POSTWARM_JUMP_ALPHA : JANUS_EYE_BASELINE_ALPHA_HOT)
                    : JANUS_EYE_BASELINE_ALPHA_QUIET;
  }
  tmos_presence_baseline = tmos_presence_baseline * (1.0f - baseAlpha) + rawP * baseAlpha;
  tmos_motion_baseline   = tmos_motion_baseline   * (1.0f - baseAlpha) + rawM * baseAlpha;

  dP = rawP - tmos_presence_baseline;
  dM = rawM - tmos_motion_baseline;
  aP = fabsf(dP);
  aM = fabsf(dM);

  float noiseAlpha = warmup ? JANUS_TMOS_WARMUP_NOISE_ALPHA : JANUS_EYE_NOISE_ALPHA;
  if (!hot || warmup) {
    tmos_presence_noise = tmos_presence_noise * (1.0f - noiseAlpha) + aP * noiseAlpha;
    tmos_motion_noise   = tmos_motion_noise   * (1.0f - noiseAlpha) + aM * noiseAlpha;
  } else {
    tmos_presence_noise = tmos_presence_noise * 0.999f + min(aP, tmos_presence_noise) * 0.001f;
    tmos_motion_noise   = tmos_motion_noise   * 0.999f + min(aM, tmos_motion_noise) * 0.001f;
  }
  tmos_presence_noise = constrain(tmos_presence_noise, 4.0f, 220.0f);
  tmos_motion_noise = constrain(tmos_motion_noise, 3.0f, 200.0f);

  float targetGain = constrain(9.0f - (tmos_presence_noise + tmos_motion_noise) * 0.035f,
                               JANUS_EYE_FOCUS_MIN_GAIN, warmup ? JANUS_TMOS_WARMUP_GAIN_MAX : JANUS_EYE_FOCUS_MAX_GAIN);
  if (hot && !warmup) targetGain = min(JANUS_EYE_FOCUS_MAX_GAIN, targetGain + 0.8f);
  float gainAlpha = warmup ? 0.045f : 0.14f;
  tmos_focus_gain = tmos_focus_gain * (1.0f - gainAlpha) + targetGain * gainAlpha;

  float warmSignal = max(0.0f, dP - (tmos_presence_noise * 0.85f + 5.0f));
  float coolSignal = max(0.0f, -dP - (tmos_presence_noise * 2.60f + 12.0f));
  float mSignal = max(0.0f, aM - (tmos_motion_noise * 0.85f + 5.0f));
  float coolGate = constrain(mSignal / max(JANUS_EYE_MOTION_FLAG_LEVEL * 2.0f, 1.0f), 0.0f, 1.0f);
  float pSignal = warmSignal + coolSignal * coolGate * JANUS_EYE_COOL_PRESENCE_WEIGHT;
  tmos_presence_delta = dP;
  tmos_motion_delta = dM;

  if (warmup) {
    pSignal *= JANUS_TMOS_WARMUP_OUTPUT_SCALE;
    mSignal *= JANUS_TMOS_WARMUP_OUTPUT_SCALE;
  }
  tmos_presence = constrain(pSignal * 0.22f * tmos_focus_gain, 0.0f, 1400.0f);
  tmos_motion   = constrain(mSignal * 0.22f * tmos_focus_gain, 0.0f, 800.0f);

  float pNorm = tmos_presence / max(JANUS_EYE_PRESENCE_FLAG_LEVEL, 1.0f);
  float mNorm = tmos_motion / max(JANUS_EYE_MOTION_FLAG_LEVEL, 1.0f);
  float evidence = max(pNorm, mNorm);
  tmos_focus_confidence = constrain(tmos_focus_confidence * 0.82f + evidence * 0.18f, 0.0f, 2.0f);

  bool softwarePresence = (pNorm > 1.10f) || (pNorm > 0.72f && mNorm > 0.65f);
  bool softwareMotion = (mNorm > 1.18f);
  bool hwPresenceAssist = tmos_hw_status_valid && tmos_hw_presence_flag && pNorm > JANUS_EYE_HW_FLAG_ASSIST_LEVEL;
  bool hwMotionAssist = tmos_hw_status_valid && tmos_hw_motion_flag && mNorm > JANUS_EYE_HW_FLAG_ASSIST_LEVEL;
  bool currentPresence = softwarePresence || hwPresenceAssist;
  bool currentMotion = softwareMotion || hwMotionAssist;
  if (warmup) {
    // During warmup the hardware flag is only accepted together with strong raw evidence.
    currentPresence = (pNorm > 2.80f) || (pNorm > 1.80f && mNorm > 1.40f) ||
                      (tmos_hw_presence_flag && pNorm > 1.60f);
    currentMotion = (mNorm > 2.60f) || (tmos_hw_motion_flag && mNorm > 1.60f);
  }

  tmos_presence_instant = currentPresence;
  tmos_motion_instant = currentMotion;
  if (currentPresence) {
    tmos_last_focus_ms = now;
    tmos_presence_hold_until_ms = now + JANUS_EYE_NOW_HOLD_MS;
  }
  if (currentMotion) {
    tmos_last_focus_ms = now;
    tmos_motion_hold_until_ms = now + JANUS_EYE_NOW_HOLD_MS;
  }
  tmos_presence_now = currentPresence || ((int32_t)(tmos_presence_hold_until_ms - now) > 0);
  tmos_motion_now = currentMotion || ((int32_t)(tmos_motion_hold_until_ms - now) > 0);

  // Memories learn only from instantaneous evidence. The 900 ms NOW hold prevents
  // flicker but cannot keep occupancy alive forever by feeding itself.
  if (currentPresence) tmos_presence_memory = tmos_presence_memory * (1.0f - JANUS_EYE_MEMORY_ATTACK) + min(2.0f, pNorm) * JANUS_EYE_MEMORY_ATTACK;
  else tmos_presence_memory *= JANUS_EYE_MEMORY_DECAY;

  if (currentMotion) tmos_motion_memory = tmos_motion_memory * (1.0f - JANUS_EYE_MEMORY_ATTACK) + min(2.0f, mNorm) * JANUS_EYE_MEMORY_ATTACK;
  else tmos_motion_memory *= JANUS_EYE_MEMORY_DECAY;

  tmos_occupancy = constrain(max(tmos_presence_memory, tmos_motion_memory), 0.0f, 2.0f);

  if (!currentPresence && !currentMotion && now - tmos_last_focus_ms > JANUS_EYE_STALE_RELEASE_MS) {
    tmos_focus_confidence *= 0.72f;
    tmos_occupancy *= JANUS_EYE_GHOST_DECAY;
    tmos_presence_memory *= JANUS_EYE_GHOST_DECAY;
    tmos_motion_memory *= JANUS_EYE_GHOST_DECAY;
  }

  // v2.15A truth semantics:
  // - a valid quiet room is CLEAR, never a "ghost";
  // - artifact rises only for stale/invalid sensor data or contradictory residual memory.
  bool sensorFresh = (now - tmos_last_valid_ms) <= JANUS_EYE_SENSOR_STALE_MS;
  bool quietInstant = !currentPresence && !currentMotion;
  bool contradictoryResidual = quietInstant &&
                               tmos_occupancy > JANUS_EYE_RESIDUAL_MEMORY_LEVEL &&
                               max(pNorm, mNorm) < 0.18f &&
                               now - tmos_last_focus_ms > JANUS_EYE_STALE_RELEASE_MS * 2UL;
  float artifactTarget = 0.0f;
  if (!sensorFresh) artifactTarget = 1.0f;
  if (contradictoryResidual) artifactTarget = max(artifactTarget, 0.72f);
  if (tmos_bad_frame_streak > 0) {
    artifactTarget = max(artifactTarget,
                         constrain((float)tmos_bad_frame_streak / max(1.0f, (float)JANUS_EYE_RECALIBRATE_BAD_FRAMES), 0.0f, 1.0f));
  }

  if (artifactTarget > 0.0f) {
    tmos_ghost_score = constrain(tmos_ghost_score * (1.0f - JANUS_EYE_ARTIFACT_ATTACK) + artifactTarget * JANUS_EYE_ARTIFACT_ATTACK,
                                 0.0f, 1.0f);
  } else {
    tmos_ghost_score *= JANUS_EYE_ARTIFACT_DECAY;
  }

  bool clearRoom = sensorFresh && quietInstant && tmos_occupancy < 0.20f && max(pNorm, mNorm) < 0.35f;
  if (clearRoom) {
    tmos_clear_score = constrain(tmos_clear_score * (1.0f - JANUS_EYE_CLEAR_ATTACK) + JANUS_EYE_CLEAR_ATTACK,
                                 0.0f, 1.0f);
  } else {
    tmos_clear_score *= JANUS_EYE_CLEAR_DECAY;
  }
#else
  tmos_presence = constrain((raw_presence - calib_presence) / 10.0f, 0.0f, 1000.0f);
  tmos_motion   = constrain((raw_motion - calib_motion) / 10.0f, 0.0f, 500.0f);
  tmos_presence_instant = tmos_presence > JANUS_EYE_PRESENCE_FLAG_LEVEL;
  tmos_motion_instant = tmos_motion > JANUS_EYE_MOTION_FLAG_LEVEL;
  if (tmos_presence_instant) tmos_presence_hold_until_ms = now + JANUS_EYE_NOW_HOLD_MS;
  if (tmos_motion_instant) tmos_motion_hold_until_ms = now + JANUS_EYE_NOW_HOLD_MS;
  tmos_presence_now = tmos_presence_instant || ((int32_t)(tmos_presence_hold_until_ms - now) > 0);
  tmos_motion_now = tmos_motion_instant || ((int32_t)(tmos_motion_hold_until_ms - now) > 0);
#endif
}

// ========================= MIC =========================

bool initMicI2S() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  if (i2s_new_channel(&chan_cfg, NULL, &rx_handle) != ESP_OK) return false;

  i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(MIC_SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)MIC_I2S_BCLK_PIN,
      .ws = (gpio_num_t)MIC_I2S_WS_PIN,
      .dout = I2S_GPIO_UNUSED,
      .din = (gpio_num_t)MIC_I2S_DATA_PIN,
      .invert_flags = {
        .mclk_inv = false,
        .bclk_inv = false,
        .ws_inv = false,
      },
    },
  };

  if (i2s_channel_init_std_mode(rx_handle, &std_cfg) != ESP_OK) return false;
  return i2s_channel_enable(rx_handle) == ESP_OK;
}

