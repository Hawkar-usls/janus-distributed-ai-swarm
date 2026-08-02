                (unsigned long)(queuedJobValid ? janusSafeAgeMs(millis(), queuedJobAtMs, 0UL) : 0UL));
  janusTwinTaskBroadcast(true);
}

void janusJobQueue(const RemoteJobState& incoming, const char* reason) {
#if JANUS_JOB_QUEUE_ENABLE
  queuedJob = incoming;
  queuedJob.active = true;
  queuedJob.hashesDone = 0;
  queuedJob.nonce = queuedJob.startNonce;
  queuedJobAtMs = millis();
  queuedJobFp32 = janusJobFp32(queuedJob);
  queuedJobValid = true;
  jobsQueued++;
  uint32_t now = millis();
  if (janusSafeAgeMs(now, lastJobQueueLogMs, 999999UL) >= JANUS_JOB_QUEUE_LOG_MS || jobsQueued <= 3UL) {
    lastJobQueueLogMs = now;
    Serial.printf("[ANCHOR/JOBQ] queued=%lu reason=%s fp=%08lX start=%08lX activeStart=%08lX activeChecked=%lu age=%lums min=%lums/%luH\n",
                  (unsigned long)jobsQueued, reason ? reason : "?", (unsigned long)queuedJobFp32,
                  (unsigned long)queuedJob.startNonce, (unsigned long)job.startNonce,
                  (unsigned long)job.hashesDone, (unsigned long)(job.active ? janusSafeAgeMs(now, job.receivedAt, 0UL) : 0UL),
                  (unsigned long)JANUS_JOB_MIN_WORK_MS, (unsigned long)JANUS_JOB_MIN_WORK_HASHES);
  }
#else
  (void)incoming; (void)reason;
#endif
}
bool janusJobPromoteQueued(const char* reason) {
#if JANUS_JOB_QUEUE_ENABLE
  if (!queuedJobValid) return false;

  uint32_t now = millis();
  uint32_t qAge = janusSafeAgeMs(now, queuedJobAtMs, 0UL);
  if (qAge >= JANUS_JOB_QUEUE_MAX_AGE_MS) {
    Serial.printf("[ANCHOR/JOBQ] promote_stale_drop fp=%08lX start=%08lX age=%lums reason=%s\n",
                  (unsigned long)queuedJobFp32, (unsigned long)queuedJob.startNonce,
                  (unsigned long)qAge, reason ? reason : "?");
    queuedJobValid = false;
    queuedJobAtMs = 0;
    queuedJobFp32 = 0;
    return false;
  }

  RemoteJobState q = queuedJob;
  queuedJobValid = false;
  queuedJobAtMs = 0;
  queuedJobFp32 = 0;
  jobsYielded++;
  if ((jobsYielded % 6UL) == 1UL || qAge > JANUS_JOB_QUEUE_MAX_AGE_MS / 2UL) {
    Serial.printf("[ANCHOR/JOBQ] promote=%lu reason=%s qAge=%lums start=%08lX fp=%08lX\n",
                  (unsigned long)jobsYielded, reason ? reason : "?", (unsigned long)qAge,
                  (unsigned long)q.startNonce, (unsigned long)janusJobFp32(q));
  }
  janusJobAccept(q, reason ? reason : "queue_promote");
  return true;
#else
  (void)reason;
  return false;
#endif
}

bool janusJobReadyToYield(uint32_t now) {
#if JANUS_JOB_QUEUE_ENABLE
  if (!job.active || !queuedJobValid) return false;
  if (janusJobFp32(job) != queuedJobFp32) return true;
  uint32_t age = janusSafeAgeMs(now, job.receivedAt, 0UL);
  return (age >= JANUS_JOB_MIN_WORK_MS) || (job.hashesDone >= JANUS_JOB_MIN_WORK_HASHES);
#else
  (void)now;
  return false;
#endif
}
void janusJobHandlePacket(const JobPacket& jp) {
  uint32_t now = millis();
  rxJobs++;
  lastMasterMs = now;

  if (jp.magic[0] != 'J' || jp.magic[1] != 'B') {
    jobsInvalid++;
    return;
  }
  if (jp.range_size == 0) {
    jobsDiscoveryPings++;
    if ((jobsDiscoveryPings % 12UL) == 1UL) {
      Serial.printf("[ANCHOR/JOB] discovery_ping=%lu active=%u q=%u masterAge=0 protocol=S2\n",
                    (unsigned long)jobsDiscoveryPings, job.active ? 1 : 0,
                    queuedJobValid ? 1 : 0);
    }
    if (!lastDiscoveryReplyMs || janusSafeAgeMs(now, lastDiscoveryReplyMs, 0UL) >= ANCHOR_DISCOVERY_REPLY_MS) {
      lastDiscoveryReplyMs = now;
      sendHeartbeat();
      sendSwarmSense(true);
      sendAnchorPnCortex(true);
      janusTwinTaskBroadcast(true);
    }
    return;
  }
  if (janusTargetAllZero(jp.target)) {
    jobsInvalid++;
    if ((jobsInvalid % 8UL) == 1UL) {
      Serial.printf("[ANCHOR/JOB] invalid=%lu reason=zero_target start=%08lX range=%lu\n",
                    (unsigned long)jobsInvalid, (unsigned long)jp.start_nonce,
                    (unsigned long)jp.range_size);
    }
    return;
  }
  jobsSeen++;
  RemoteJobState incoming = janusJobBuildFromPacket(jp);
  uint32_t incomingFp = janusJobFp32(incoming);
  if (!job.active) {
    queuedJobValid = false;
    queuedJobAtMs = 0;
    queuedJobFp32 = 0;
    janusJobAccept(incoming, "idle_accept");
    return;
  }
  uint32_t currentFp = janusJobFp32(job);
  bool newPoolWork = !janusJobSameIdentity(incoming, job);
#if JANUS_JOB_DUP_START_DROP
  if (!newPoolWork && janusJobSameStart(job, incoming)) {
    jobsDroppedDuplicate++;
    if ((jobsDroppedDuplicate % 12UL) == 1UL) {
      Serial.printf("[ANCHOR/JOBQ] dupDrop=%lu fp=%08lX start=%08lX activeChecked=%lu\n",
                    (unsigned long)jobsDroppedDuplicate, (unsigned long)incomingFp,
                    (unsigned long)incoming.startNonce, (unsigned long)job.hashesDone);
    }
    return;
  }
  if (queuedJobValid && janusJobSameStart(queuedJob, incoming)) {
    jobsDroppedDuplicate++;
    return;
  }
#endif
#if JANUS_JOB_NEW_FP_REPLACE
  if (newPoolWork) {
    jobsReplacedNewWork++;
    queuedJobValid = false;
    queuedJobAtMs = 0;
    queuedJobFp32 = 0;
    Serial.printf("[ANCHOR/JOB] newfp_replace=%lu oldFp=%08lX newFp=%08lX oldChecked=%lu oldAge=%lums\n",
                  (unsigned long)jobsReplacedNewWork, (unsigned long)currentFp, (unsigned long)incomingFp,
                  (unsigned long)job.hashesDone,
                  (unsigned long)janusSafeAgeMs(now, job.receivedAt, 0UL));
    janusJobAccept(incoming, "new_pool_work");
    return;
  }
#endif
#if JANUS_JOB_QUEUE_ENABLE
  uint32_t age = janusSafeAgeMs(now, job.receivedAt, 0UL);
  bool usefulSliceDone = (age >= JANUS_JOB_MIN_WORK_MS) || (job.hashesDone >= JANUS_JOB_MIN_WORK_HASHES);
  if (!usefulSliceDone) {
    janusJobQueue(incoming, "same_fp_hold_current");
    return;
  }
  jobsYielded++;
  if ((jobsYielded % 6UL) == 1UL || job.hashesDone >= 20000UL || bestBits + 1 >= targetBits) {
    Serial.printf("[ANCHOR/JOBQ] yield=%lu reason=same_fp_slice_done oldStart=%08lX oldChecked=%lu oldAge=%lums newStart=%08lX fp=%08lX\n",
                  (unsigned long)jobsYielded, (unsigned long)job.startNonce,
                  (unsigned long)job.hashesDone, (unsigned long)age,
                  (unsigned long)incoming.startNonce, (unsigned long)incomingFp);
  }
  queuedJobValid = false;
  queuedJobAtMs = 0;
  queuedJobFp32 = 0;
  janusJobAccept(incoming, "same_fp_yield");
#else
  janusJobAccept(incoming, "queue_disabled_replace");
#endif
}
void janusJobHousekeeping(uint32_t now) {
#if JANUS_JOB_QUEUE_ENABLE
  if (queuedJobValid && janusSafeElapsed(now, queuedJobAtMs, JANUS_JOB_QUEUE_MAX_AGE_MS)) {
    Serial.printf("[ANCHOR/JOBQ] stale_drop fp=%08lX start=%08lX age=%lums\n",
                  (unsigned long)queuedJobFp32, (unsigned long)queuedJob.startNonce,
                  (unsigned long)(janusSafeAgeMs(now, queuedJobAtMs, 0UL)));
    queuedJobValid = false;
    queuedJobAtMs = 0;
    queuedJobFp32 = 0;
  }
  if (!job.active) janusJobPromoteQueued("idle_promote");
  else if (janusJobReadyToYield(now)) janusJobPromoteQueued("scheduled_yield");
#else
  (void)now;
#endif
}
void anchorLedFlashShare(uint16_t bits) {
  lastShareMs = millis();
  lastShareBits = bits;
  lastLedMs = 0;
  janusFaceBroadcast(true, bits);
  janusTwinTaskBroadcast(true);
}
void sendShare(uint32_t nonce, uint16_t bits, const uint8_t shareHashBE[32]) {
  BuzzShareResponseV2 sr{};
  sr.magic[0] = 'S'; sr.magic[1] = '2';
  memcpy(sr.job_id, job.job_id, 8);
  sr.nonce = nonce;
  sr.worker_id = workerId;
  sr.bits = bits;
  sr.total_hashes_l32 = totalHashesLifetime;
  if (shareHashBE) memcpy(sr.hash_tail, shareHashBE + 28, sizeof(sr.hash_tail));
  bool ok = sendEspNow("S/2", &sr, sizeof(sr));
  esp_err_t directErr = sendEspNowToBuzzMaster("S2-direct", &sr, sizeof(sr));
