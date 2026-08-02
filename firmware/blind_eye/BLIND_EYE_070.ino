float readMicRms() {
  if (!rx_handle) return 0.0f;

  int32_t samples[MIC_FRAME_SAMPLES];
  size_t bytesRead = 0;
  esp_err_t err = i2s_channel_read(rx_handle, samples, sizeof(samples), &bytesRead, 10);
  if (err != ESP_OK || bytesRead == 0) return 0.0f;

  int count = bytesRead / sizeof(int32_t);
  double sumSq = 0.0;
  for (int i = 0; i < count; ++i) {
    float v = (float)samples[i] / 2147483648.0f;
    sumSq += (double)v * (double)v;
  }
  return sqrt(sumSq / (double)count);
}

// ========================= IMU =========================

void initIMU() {
  M5.Imu.init();
  delay(100);
}

void readIMUClassic() {
  if (!M5.Imu.isEnabled()) {
    acc_x = acc_y = acc_z = 0;
    gyro_x = gyro_y = gyro_z = 0;
    mag_x = mag_y = mag_z = 0;
    imu_temp = 0;
    mag_norm = 0;
    imu_shock = 0;
    return;
  }

  M5.Imu.getAccel(&acc_x, &acc_y, &acc_z);
  M5.Imu.getGyro(&gyro_x, &gyro_y, &gyro_z);

  if (!M5.Imu.getMag(&mag_x, &mag_y, &mag_z)) {
    mag_x = mag_y = mag_z = 0;
  }

  if (!M5.Imu.getTemp(&imu_temp)) {
    imu_temp = 0;
  }

  imu_shock = fabsf(acc_x) + fabsf(acc_y) + fabsf(acc_z);
  mag_norm = sqrtf(mag_x * mag_x + mag_y * mag_y + mag_z * mag_z);
}

// ========================= MODEL =========================

void buildFeatures(float x[FEATURE_DIM]) {
  x[0] = constrain(tmos_presence / 1000.0f, 0.0f, 1.0f);
  x[1] = constrain(tmos_motion / 500.0f, 0.0f, 1.0f);
  x[2] = constrain(mic_rms * 10.0f, 0.0f, 4.0f);
  x[3] = constrain(mag_norm / 200.0f, 0.0f, 4.0f);
  x[4] = constrain(imu_shock / 5.0f, 0.0f, 4.0f);
  x[5] = constrain(((float)wifi_rssi / -100.0f) + rf_presence_score * 0.35f + rf_motion_energy * 0.020f, 0.0f, 2.5f);

  // Tachyon/Physarious micro-features: future pressure, movement viscosity,
  // remote prophecies and Markov sector confidence. These are bounded so old
  // bad frames cannot destroy the tiny linear model.
  x[6] = constrain(tachyonPredPresence1 / 1000.0f, 0.0f, 2.0f);
  x[7] = constrain(tachyonPredMotion1 / 500.0f, 0.0f, 2.0f);
  x[8] = constrain(tachyonSwarmPressure + kenshiConfidence * 0.25f + tmos_focus_confidence * 0.12f + rfLiteFusionScore() * 0.25f, 0.0f, 2.4f);
  x[9] = constrain(tachyonFutureStress, 0.0f, 2.0f);
}

float predict(const float x[FEATURE_DIM]) {
  float y = model_b;
  for (int i = 0; i < FEATURE_DIM; ++i) y += model_w[i] * x[i];
  return y;
}

float computeActivity() {
  return
    tmos_presence * 0.002f +
    tmos_motion * 0.004f +
    mic_rms * 20.0f +
    mag_norm * 0.010f +
    imu_shock * 0.20f +
    tmos_focus_confidence * 0.10f +
    rf_motion_energy * 0.10f +
    rf_presence_score * 0.35f;
}

void train(const float target, const float x[FEATURE_DIM]) {
  float pred = predict(x);
  float err = constrain(pred - target, -4.0f, 4.0f);

  // Physarious-style "blackhole guard": high future stress damps the step.
  float stable = 1.0f / (1.0f + tachyonFutureStress * 0.55f + tachyonLangerDrag * 0.35f);
  float lr = constrain(model_lr * stable, 0.00018f, 0.0060f);

  for (int i = 0; i < FEATURE_DIM; ++i) {
    float xi = constrain(x[i], -3.0f, 4.0f);
    model_w[i] -= lr * err * xi;
    model_w[i] = constrain(model_w[i], -3.0f, 3.0f);
  }
  model_b -= lr * err;
  model_b = constrain(model_b, -4.0f, 4.0f);

  loss = loss * 0.82f + fabsf(pred - target) * 0.18f;
}

float meanOf(float* arr, int n) {
  if (n <= 0) return 0.0f;
  float s = 0.0f;
  for (int i = 0; i < n; ++i) s += arr[i];
  return s / n;
}

float stdOf(float* arr, int n, float mean) {
  if (n <= 1) return 0.0f;
  float s = 0.0f;
  for (int i = 0; i < n; ++i) {
    float d = arr[i] - mean;
    s += d * d;
  }
  return sqrtf(s / (n - 1));
}

void pushHistory() {
  hist_activity[hist_pos] = activity;
  hist_loss[hist_pos] = loss;
  hist_pos = (hist_pos + 1) % HIST_SIZE;
  if (hist_count < HIST_SIZE) hist_count++;
}

void updateMiniGPT() {
  float x[FEATURE_DIM];
  buildFeatures(x);

  pred_activity = predict(x);
  activity = computeActivity();
  train(activity, x);

  fit = (1.0f / (1.0f + loss)) + min(2.0f, activity * 0.15f);
  if (fit > fit_best) fit_best = fit;

  if (loss > 0.12f) model_lr = min(0.006f, model_lr * 1.0008f);
  else model_lr = max(0.0003f, model_lr * 0.9992f);

  pushHistory();

  float meanA = meanOf(hist_activity, hist_count);
  float stdA = stdOf(hist_activity, hist_count, meanA);
  float meanL = meanOf(hist_loss, hist_count);
  float stdL = stdOf(hist_loss, hist_count, meanL);

  z_activity = (stdA > 1e-6f) ? (activity - meanA) / stdA : 0.0f;
  z_loss = (stdL > 1e-6f) ? (loss - meanL) / stdL : 0.0f;
  sync_hint = 1.0f / (1.0f + fabsf(pred_activity - activity) + loss);

  // v2.6: apply Buzz Agent hint here, where model globals are already declared.
  if (millis() - colonyAgentLastRewardMs < COLONY_AGENT_REWARD_VISIBLE_MS) {
    if (colonyAgentHint == 3) {
      model_lr = min(0.006f, model_lr * 1.0015f);
      sync_hint = max(sync_hint, 0.88f);
    } else if (colonyAgentHint == 2) {
      model_lr = max(0.0003f, model_lr * 0.9985f);
    }
  }

  bool imu_ok = M5.Imu.isEnabled() && (fabsf(acc_x) + fabsf(acc_y) + fabsf(acc_z) + fabsf(gyro_x) + fabsf(gyro_y) + fabsf(gyro_z)) > 0.01f;
  bool mag_ok = fabsf(mag_norm) > 0.01f;
  bool tmos_ok = tmos_ready;
  bool mic_ok = rx_handle != nullptr;

  if (!imu_ok) statusLine = "imu offline";
  else if (!tmos_ok) statusLine = "tmos missing";
  else if (millis() - colonyAgentLastRewardMs < COLONY_AGENT_REWARD_VISIBLE_MS) {
    if (colonyAgentLevel >= 3) statusLine = "agent golden";
    else if (colonyAgentLevel == 2) statusLine = "agent boost";
    else if (colonyAgentLevel == 1) statusLine = "agent praise";
    else statusLine = "agent observe";
  }
  else if (loss < 0.04f) statusLine = "eye guessed";
  else if (loss > 0.40f) statusLine = "eye training";
  else statusLine = "eye stable";

  diagLine =
    String(imu_ok ? "IMU" : "--") + " " +
    String(mag_ok ? "MAG" : "--") + " " +
    String(tmos_ok ? "TMOS" : "--") + " " +
    String(mic_ok ? "MIC" : "--");
}

// ========================= STORAGE =========================

struct EyeModelBlob {
  uint32_t magic;
  uint16_t version;
  uint16_t dim;
  float w[FEATURE_DIM];
  float b;
  float lr;
};

void resetModelDefaults() {
  const float defaults[FEATURE_DIM] = {0.10f, -0.02f, 0.08f, 0.12f, 0.05f, 0.03f, 0.06f, 0.05f, 0.04f, -0.02f};
  memcpy(model_w, defaults, sizeof(model_w));
  model_b = 0.0f;
  model_lr = 0.0020f;
}

void saveModel() {
  LittleFS.remove(MODEL_FILE);
  File f = LittleFS.open(MODEL_FILE, "w");
  if (!f) return;
  EyeModelBlob b{};
  b.magic = 0x45594532UL; // EYE2
  b.version = 2;
  b.dim = FEATURE_DIM;
  memcpy(b.w, model_w, sizeof(model_w));
  b.b = model_b;
  b.lr = model_lr;
  f.write((uint8_t*)&b, sizeof(b));
  f.close();
}

void loadModel() {
  File f = LittleFS.open(MODEL_FILE, FILE_READ);
  if (!f) {
    resetModelDefaults();
    return;
  }

  size_t sz = f.size();
  if (sz == sizeof(EyeModelBlob)) {
    EyeModelBlob b{};
    size_t got = f.read((uint8_t*)&b, sizeof(b));
    f.close();
    if (got == sizeof(b) && b.magic == 0x45594532UL && b.version == 2 && b.dim == FEATURE_DIM) {
      memcpy(model_w, b.w, sizeof(model_w));
      model_b = b.b;
      model_lr = b.lr;
    } else {
      resetModelDefaults();
    }
  } else {
    // Legacy v2.6/v2.7 model: 6 weights + bias + lr. Preserve learned old core,
    // initialize the new tachyon features safely.
    float legacyW[6] = {0};
    float legacyB = 0.0f;
    float legacyLr = 0.0020f;
    f.read((uint8_t*)legacyW, sizeof(legacyW));
    f.read((uint8_t*)&legacyB, sizeof(legacyB));
    f.read((uint8_t*)&legacyLr, sizeof(legacyLr));
    f.close();
    resetModelDefaults();
    for (uint8_t i = 0; i < 6; ++i) model_w[i] = legacyW[i];
    model_b = legacyB;
    model_lr = legacyLr;
  }

  bool bad = !isfinite(model_b) || !isfinite(model_lr) || model_lr <= 0.0f || model_lr > 0.05f;
  for (uint8_t i = 0; i < FEATURE_DIM; ++i) if (!isfinite(model_w[i])) bad = true;
  if (bad) resetModelDefaults();
  model_lr = constrain(model_lr, 0.00018f, 0.0060f);
}

void saveState() {
  LittleFS.remove(STATE_FILE);
  File f = LittleFS.open(STATE_FILE, "w");
  if (!f) return;

  StaticJsonDocument<512> doc;
  doc["calibrated"] = calibrated;
  doc["fit_best"] = fit_best;
  doc["model_lr"] = model_lr;
  doc["calib_presence"] = calib_presence;
  doc["calib_motion"] = calib_motion;
  doc["tachyon_pg"] = tachyonPresenceConfidence;
  doc["tachyon_mg"] = tachyonMotionConfidence;
  doc["tachyon_tg"] = tachyonTrendGain;
  doc["tachyon_mg2"] = tachyonMemoryGain;
  doc["tachyon_rg"] = tachyonRemoteGain;
  serializeJson(doc, f);
  f.close();
}

void loadState() {
  File f = LittleFS.open(STATE_FILE, FILE_READ);
  if (!f) return;

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, f) != DeserializationError::Ok) {
    f.close();
    return;
  }
  f.close();

  calibrated = doc["calibrated"] | calibrated;
  fit_best = doc["fit_best"] | fit_best;
  model_lr = doc["model_lr"] | model_lr;
  calib_presence = doc["calib_presence"] | calib_presence;
  calib_motion = doc["calib_motion"] | calib_motion;
  tachyonPresenceConfidence = doc["tachyon_pg"] | tachyonPresenceConfidence;
  tachyonMotionConfidence = doc["tachyon_mg"] | tachyonMotionConfidence;
  tachyonTrendGain = constrain(doc["tachyon_tg"] | tachyonTrendGain, 0.10f, 1.50f);
  tachyonMemoryGain = constrain(doc["tachyon_mg2"] | tachyonMemoryGain, 0.05f, 1.00f);
  tachyonRemoteGain = constrain(doc["tachyon_rg"] | tachyonRemoteGain, 0.00f, 0.80f);
}

// ========================= IO =========================

