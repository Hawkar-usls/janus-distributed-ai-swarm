void sendNodeEntropy() {
  static uint32_t lastEntropyDbg = 0;
  if (millis() - lastEntropyDbg > 5000) {
    lastEntropyDbg = millis();
    Serial.printf("[COLONY] TX entropy worker=%u\n", colonyWorkerId);
  }
  EntropyReport er{};
  er.magic[0] = 'E'; er.magic[1] = 'R';
  er.worker_id = colonyWorkerId;
  er.local_entropy = eyeLocalEntropy();
  er.sensor_flags = 0xE7; // mic + TMOS + IMU/mag + Kenshi + JANUS event/motion-base metadata
  er.values[0] = mic_rms;
  er.values[1] = tmos_presence;
  er.values[2] = mag_norm;
  er.values[3] = rf_ready ? rf_presence_score : (motionBasePresent ? (float)motionBaseServoAngle : loss);
  janusEyeEspNowSend("E/R", &er, sizeof(er), true);

  EntropyReportV2 er2{};
  er2.magic[0] = 'E'; er2.magic[1] = '2';
  er2.worker_id = colonyWorkerId;
  strlcpy(er2.nodeId, "BlindEye", sizeof(er2.nodeId));
  er2.local_entropy = er.local_entropy;
  er2.prediction_error = loss;
  er2.sync_hint = sync_hint;
  er2.fit = fit;
  er2.sensor_flags = er.sensor_flags;
  er2.values[0] = mic_rms;
  er2.values[1] = tmos_presence;
  er2.values[2] = tmos_motion;
  er2.values[3] = mag_norm;
  er2.values[4] = imu_shock;
  er2.values[5] = activity;
  er2.values[6] = pred_activity;
  er2.values[7] = rf_entropy;       // v2.12: RF fusion entropy / radio anomaly score
  er2.uptime_ms = millis();
  janusEyeEspNowSend("E2", &er2, sizeof(er2), true);
}

// ========================= HEADLESS STATUS =========================
// Blind EYE has no physical screen. There is no display output.
// Status is exposed through ESP-NOW heartbeat/entropy packets and optional Serial debug.

void printHeadlessStatus() {
  Serial.printf("[EYE] mode=%s wifi=%s rssi=%d act=%.2f pred=%.2f loss=%.3f fit=%.2f diag=%s status=%s\n",
                colonyMode,
                WiFi.status() == WL_CONNECTED ? "OK" : "OFF",
                wifi_rssi,
                activity,
                pred_activity,
                loss,
                fit,
                diagLine.c_str(),
                statusLine.c_str());

  Serial.printf("[EYE] focus rawP=%d rawM=%d dP=%.1f dM=%.1f base=%.1f/%.1f noise=%.1f/%.1f gain=%.2f conf=%.2f\n",
                raw_presence, raw_motion,
                tmos_presence_delta, tmos_motion_delta,
                tmos_presence_baseline, tmos_motion_baseline,
                tmos_presence_noise, tmos_motion_noise,
                tmos_focus_gain, tmos_focus_confidence);

  Serial.printf("[EYE] v2.15A TMOS P/M=%u/%u inst=%u/%u hw=%u/%u occ=%.2f mem=%.2f/%.2f clear=%.2f artifact=%.2f bad=%u/%u err=%ld validAgo=%lums warmup=%lus flags=0x%02X\n",
                tmos_presence_now ? 1 : 0,
                tmos_motion_now ? 1 : 0,
                tmos_presence_instant ? 1 : 0,
                tmos_motion_instant ? 1 : 0,
                tmos_hw_presence_flag ? 1 : 0,
                tmos_hw_motion_flag ? 1 : 0,
                tmos_occupancy,
                tmos_presence_memory,
                tmos_motion_memory,
                tmos_clear_score,
                tmos_ghost_score,
                (unsigned)tmos_bad_frames,
                (unsigned)tmos_bad_frame_streak,
                (long)tmos_last_read_error,
                (unsigned long)(millis() - tmos_last_valid_ms),
                (unsigned long)(tmosWarmupActive(millis()) ? (tmosWarmupUntilMs - millis()) / 1000UL : 0UL),
                (unsigned)((tmos_presence_now ? JANUS_EYE_FLAG_PRESENCE_NOW : 0) |
                           (tmos_motion_now ? JANUS_EYE_FLAG_MOTION_NOW : 0)));

  rfLiteDebugTick(millis(), true);

  Serial.printf("[EYE] miner H=%lu best=%lu target=%u tickets=%lu jobs=%lu done=%lu exp=%lu lane=%s/s%u stride=%lu arm=%u switches=%lu tail=%lu bestN=%08lX\n",
                (unsigned long)colonyRemoteHashrate,
                (unsigned long)colonyBestBits,
                (unsigned)colonyTargetBits,
                (unsigned long)colonyRemoteShares,
                (unsigned long)colonyJobsSeen,
                (unsigned long)colonyJobsDone,
                (unsigned long)colonyJobsExpired,
                colonyMinerLaneName(colonyJob.minerLane),
                (unsigned)colonyJob.minerSector,
                (unsigned long)colonyJob.minerStride,
                (unsigned)colonyJob.minerStrideArm,
                (unsigned long)colonyMinerLaneSwitches,
                (unsigned long)colonyMinerTailHits,
                (unsigned long)colonyMinerBestNonce);

  Serial.printf("[EYE] agent rewards=%lu aok=%lu lvl=%u hint=%u pts=%u batch=%u score=%.1f predH=%.1f err=%.3f entropy=%04lX\n",
                (unsigned long)colonyAgentRewardsRx,
                (unsigned long)colonyAgentShareRewardsRx,
                (unsigned)colonyAgentLevel,
                (unsigned)colonyAgentHint,
                (unsigned)colonyAgentRewardPoints,
                (unsigned)effectiveColonyRemoteBatch(),
                colonyAgentScore,
                colonyAgentPredictedHash,
                colonyAgentPredictionError,
                (unsigned long)(colonyAgentEntropySeed & 0xFFFF));

  Serial.printf("[EYE] kenshi bubble=%u job=%u sector=%u->%u prio=%u conf=%.2f active=%u virtual=%u rx=%lu tx=%lu flags=0x%08lX\n",
                (unsigned)kenshiBubbleState,
                (unsigned)kenshiJobState,
                (unsigned)kenshiSector,
                (unsigned)kenshiPredSector,
                (unsigned)kenshiPriority,
                kenshiConfidence,
                (unsigned)kenshiActiveNodes,
                (unsigned)kenshiVirtualNodes,
                (unsigned long)kenshiRxPackets,
                (unsigned long)kenshiTxPackets,
                (unsigned long)kenshiWorldFlags);

  Serial.printf("[EYE] tachyon P %.0f/%.0f/%.0f M %.0f/%.0f/%.0f eta=%.0f stress=%.2f conf=%.2f/%.2f rem=%u TP rx/tx=%lu/%lu camera=%u primary=TMOS aperture=%u frames=%lu event=%lu ctrl=%lu\n",
                tachyonPredPresence1, tachyonPredPresence2, tachyonPredPresence3,
                tachyonPredMotion1, tachyonPredMotion2, tachyonPredMotion3,
                tachyonEventEtaMs, tachyonFutureStress, tachyonPresenceConfidence, tachyonMotionConfidence,
                (unsigned)tachyonRemoteCount,
                (unsigned long)tachyonRxPackets, (unsigned long)tachyonTxPackets,
                (unsigned)JANUS_EYE_CAMERA_PRESENT,
                eyeVisionEnabled ? 1 : 0,
                (unsigned long)eyeVisionFramesTx,
                (unsigned long)eyeVisionEventFramesTx,
                (unsigned long)eyeVisionControlsRx);

  Serial.printf("[EYE] blackboard ev=%lu pol=%lu mood=%s raw=%s radio=%u sensor=%u smoothDrop=%lu order=%s | motionBase present=%u power=%u armed=%u write=%u angle=%d target=%d mv=%d curRaw=%d i2cErr=%lu servoWr=%lu\n",
                (unsigned long)janusEventSeq,
                (unsigned long)janusPolicyRx,
                janusMoodName(janusPolicyMood),
                janusMoodName(janusPolicyRawLastMood),
                (unsigned)janusPolicyRadioRate,
                (unsigned)janusPolicySensorRate,
                (unsigned long)janusPolicySmoothedDrops,
                janusPolicyOrder,
                motionBasePresent ? 1 : 0,
                motionBasePowerPresent ? 1 : 0,
                motionBaseArmed ? 1 : 0,
                (unsigned)JANUS_MOTION_BASE_WRITE_ENABLE,
                (int)motionBaseServoAngle,
                (int)motionBaseTargetAngle,
                (int)motionBaseBusMv,
                (int)motionBaseCurrentRaw,
                (unsigned long)motionBaseI2cErrors,
                (unsigned long)motionBaseServoWrites);

  Serial.printf("[EYE] semantic episodes=%u memTx=%lu needTx=%lu doneTx=%lu ss=%lu/%lu lastEpAgo=%lums clear=%.2f artifact=%.2f artifactSince=%lums warmup=%lus motionBase=%u/%u bus=%dmV write=%u\n",
                (unsigned)janusEyeEpisodeCount,
                (unsigned long)janusEyeAiMemoryTx,
                (unsigned long)janusEyeTaskNeedTx,
                (unsigned long)janusEyeTaskDoneTx,
                (unsigned long)janusEyeSwarmSenseTx,
                (unsigned long)janusEyeSwarmSenseFail,
                (unsigned long)(janusEyeLastEpisodeMs ? millis() - janusEyeLastEpisodeMs : 0),
                tmos_clear_score,
                tmos_ghost_score,
                (unsigned long)(tmosGhostHighSinceMs ? millis() - tmosGhostHighSinceMs : 0UL),
                (unsigned long)(tmosWarmupActive(millis()) ? (tmosWarmupUntilMs - millis()) / 1000UL : 0UL),
                motionBasePresent ? 1 : 0,
                motionBasePowerPresent ? 1 : 0,
                (int)motionBaseBusMv,
                (unsigned)JANUS_MOTION_BASE_WRITE_ENABLE);

  Serial.printf("[EYE] radio txOk=%lu txFail=%lu lastErr=%d lastTag=%s peerCh=%u rebuilds=%lu K2tx=%lu TPtx=%lu EFtx=%lu pol=%lu\n",
                (unsigned long)colonyTxOk,
                (unsigned long)colonyTxFail,
                (int)colonyLastTxErr,
                colonyLastTxTag,
                (unsigned)colonyPeerChannel,
                (unsigned long)colonyPeerRebuilds,
                (unsigned long)kenshiTxPackets,
                (unsigned long)tachyonTxPackets,
                (unsigned long)eyeVisionFramesTx,
                (unsigned long)janusPolicyRx);
}

// ========================= MAIN =========================

void readSensors() {
  readIMUClassic();
  readTMOS();
  mic_rms = readMicRms();
  wifi_rssi = (WiFi.status() == WL_CONNECTED) ? WiFi.RSSI() : -127;
  rfLiteTick(millis());
  updateMiniGPT();
  updateTachyonProphecy();
  updateKenshiVirtualWorld();
}

