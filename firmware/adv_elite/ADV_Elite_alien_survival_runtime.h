#pragma once

#include <M5Cardputer.h>
#include <math.h>
#include "ADV_Elite_alien_survival_policy.h"

namespace janus_adv_elite {

class AlienSurvivalRuntime {
 public:
  static constexpr int kScreenW = 240;
  static constexpr int kScreenH = 135;
  static constexpr int kMaxEnemies = 28;
  static constexpr int kMaxParticles = 64;
  static constexpr int kMaxPickups = 14;

  enum EnemyKind : uint8_t { GRUNT = 0, FAST = 1, TANK = 2, BOSS = 3 };
  enum PickupKind : uint8_t { XP = 0, MED = 1, RAPID = 2 };

  struct Enemy {
    bool active = false;
    EnemyKind kind = GRUNT;
    float x = 0, y = 0;
    float hp = 0, maxHp = 0;
    float speed = 0;
    float contactDps = 0;
  };

  struct Particle {
    bool active = false;
    float x = 0, y = 0, vx = 0, vy = 0, life = 0;
    uint16_t color = TFT_WHITE;
  };

  struct Pickup {
    bool active = false;
    PickupKind kind = XP;
    float x = 0, y = 0;
    float life = 0;
  };

  struct Stats {
    uint32_t kills = 0;
    uint32_t shots = 0;
    uint32_t hits = 0;
    uint32_t wave = 1;
    uint32_t level = 1;
    uint32_t xp = 0;
    uint32_t xpNeed = 12;
    uint32_t bestWave = 1;
  };

  void begin() {
    resetRun();
    lastWallMs_ = millis();
    lastDrawMs_ = lastWallMs_;
  }

  void enter() {
    active_ = true;
    lastWallMs_ = millis();
    accumulator_ = 0.0f;
    if (dead_) resetRun();
  }

  void leave() {
    active_ = false;
    paused_ = false;
  }

  bool active() const { return active_; }
  bool paused() const { return paused_; }
  bool autoFire() const { return autoFire_; }
  bool gyroMode() const { return gyroMode_; }
  bool dead() const { return dead_; }
  float fpsEma() const { return fpsEma_; }
  float health01() const { return maxHp_ > 0.01f ? constrain(hp_ / maxHp_, 0.0f, 1.0f) : 0.0f; }
  const Stats& stats() const { return stats_; }
  const char* perkLabel() const { return perkLabel_; }

  void togglePause() { if (!dead_) paused_ = !paused_; }
  void toggleAutoFire() { autoFire_ = !autoFire_; }

  void toggleGyro(float ax, float ay) {
    gyroMode_ = !gyroMode_;
    if (gyroMode_) {
      neutralAx_ = ax;
      neutralAy_ = ay;
      gyroSide_ = gyroFwd_ = 0.0f;
      gyroSettleUntilMs_ = millis() + 900UL;
    }
  }

  void restartIfDead() {
    if (dead_) resetRun();
  }

  void update(uint32_t nowMs,
              bool left, bool right, bool up, bool down,
              bool fireHeld,
              float ax, float ay, float gyroMag) {
    if (!active_) return;

    float wallDt = (nowMs - lastWallMs_) * 0.001f;
    lastWallMs_ = nowMs;
    if (wallDt < 0.0f) wallDt = 0.0f;
    if (wallDt > 0.25f) wallDt = 0.25f;

    if (dead_ || paused_) return;

    updateGyro(nowMs, ax, ay, gyroMag, left || right || up || down);

    moveX_ = 0.0f;
    moveY_ = 0.0f;
    if (gyroMode_) {
      moveX_ = gyroSide_;
      moveY_ = gyroFwd_;
    } else {
      if (left)  moveX_ -= 1.0f;
      if (right) moveX_ += 1.0f;
      if (up)    moveY_ -= 1.0f;
      if (down)  moveY_ += 1.0f;
      normalize(moveX_, moveY_);
    }

    fireRequested_ = fireHeld || autoFire_;
    accumulator_ += wallDt;
    uint8_t steps = 0;
    while (accumulator_ >= AlienSimulationPolicy::kFixedDtSeconds &&
           steps < AlienSimulationPolicy::kMaxCatchupSteps) {
      simulateStep(AlienSimulationPolicy::kFixedDtSeconds);
      accumulator_ -= AlienSimulationPolicy::kFixedDtSeconds;
      ++steps;
    }
    if (steps == AlienSimulationPolicy::kMaxCatchupSteps && accumulator_ > 0.2f) {
      accumulator_ = 0.0f;
    }
  }

  void draw(M5GFX& d) {
    if (!active_) return;
    const uint32_t now = millis();
    const uint32_t frameMs = now - lastDrawMs_;
    lastDrawMs_ = now;
    if (frameMs > 0) {
      float fps = 1000.0f / (float)frameMs;
      fpsEma_ = fpsEma_ * 0.90f + fps * 0.10f;
    }

    d.fillScreen(TFT_BLACK);
    drawBackground(d);

    for (int i = 0; i < kMaxPickups; ++i) {
      if (!pickups_[i].active) continue;
      uint16_t c = pickups_[i].kind == MED ? TFT_GREEN : (pickups_[i].kind == RAPID ? TFT_YELLOW : TFT_CYAN);
      d.fillCircle((int)pickups_[i].x, (int)pickups_[i].y, 2, c);
    }

    for (int i = 0; i < kMaxEnemies; ++i) {
      if (!enemies_[i].active) continue;
      drawEnemy(d, enemies_[i]);
    }

    for (int i = 0; i < kMaxParticles; ++i) {
      if (!particles_[i].active) continue;
      d.drawPixel((int)particles_[i].x, (int)particles_[i].y, particles_[i].color);
    }

    // player
    d.fillCircle((int)px_, (int)py_, 4, TFT_WHITE);
    d.drawCircle((int)px_, (int)py_, 6, TFT_CYAN);

    // HUD
    d.fillRect(0, 0, kScreenW, 11, TFT_BLACK);
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(2, 2);
    d.printf("A W%lu L%lu K%lu", (unsigned long)stats_.wave, (unsigned long)stats_.level, (unsigned long)stats_.kills);
    d.setCursor(142, 2);
    d.printf("%s %s %dFPS", gyroMode_ ? "GY" : "KB", autoFire_ ? "AUTO" : "MAN", (int)fpsEma_);

    int hpW = 78;
    d.drawRect(2, kScreenH - 8, hpW, 5, TFT_DARKGREY);
    int hpFill = (int)((hpW - 2) * health01());
    if (hpFill > 0) d.fillRect(3, kScreenH - 7, hpFill, 3, healthColor565(d));

    float xp01 = stats_.xpNeed ? constrain((float)stats_.xp / (float)stats_.xpNeed, 0.0f, 1.0f) : 0.0f;
    d.drawRect(84, kScreenH - 8, 70, 5, TFT_DARKGREY);
    int xpFill = (int)(68 * xp01);
    if (xpFill > 0) d.fillRect(85, kScreenH - 7, xpFill, 3, TFT_MAGENTA);

    d.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    d.setCursor(160, kScreenH - 10);
    d.printf("%s", perkLabel_);

    if (paused_) {
      d.fillRect(65, 47, 110, 35, TFT_BLACK);
      d.drawRect(65, 47, 110, 35, TFT_YELLOW);
      d.setTextColor(TFT_YELLOW, TFT_BLACK);
      d.setCursor(100, 58); d.print("PAUSED");
      d.setCursor(77, 70); d.print("P resume  ESC home");
    }

    if (dead_) {
      d.fillRect(43, 38, 154, 56, TFT_BLACK);
      d.drawRect(43, 38, 154, 56, TFT_RED);
      d.setTextColor(TFT_RED, TFT_BLACK);
      d.setCursor(82, 49); d.print("YOU DIED");
      d.setTextColor(TFT_WHITE, TFT_BLACK);
      d.setCursor(59, 62); d.printf("wave %lu  kills %lu", (unsigned long)stats_.wave, (unsigned long)stats_.kills);
      d.setCursor(66, 76); d.print("SPACE restart");
    }
  }

 private:
  Enemy enemies_[kMaxEnemies];
  Particle particles_[kMaxParticles];
  Pickup pickups_[kMaxPickups];
  Stats stats_;

  bool active_ = false;
  bool paused_ = false;
  bool autoFire_ = true;
  bool gyroMode_ = false;
  bool dead_ = false;
  bool fireRequested_ = false;

  float px_ = kScreenW * 0.5f;
  float py_ = kScreenH * 0.55f;
  float hp_ = 100.0f;
  float maxHp_ = 100.0f;
  float speed_ = 74.0f;
  float damage_ = 17.0f;
  float fireCooldown_ = 0.0f;
  float firePeriod_ = 0.30f;
  float rapidTimer_ = 0.0f;
  float moveX_ = 0, moveY_ = 0;
  float accumulator_ = 0.0f;
  float spawnCooldown_ = 0.0f;
  float waveRest_ = 0.0f;
  uint32_t spawnedThisWave_ = 0;
  uint32_t requiredThisWave_ = 10;

  float neutralAx_ = 0, neutralAy_ = 0;
  float gyroSide_ = 0, gyroFwd_ = 0;
  uint32_t gyroSettleUntilMs_ = 0;

  uint32_t lastWallMs_ = 0;
  uint32_t lastDrawMs_ = 0;
  float fpsEma_ = 45.0f;
  char perkLabel_[18] = "BASELINE";

  static float sq(float x) { return x * x; }

  static void normalize(float& x, float& y) {
    float l2 = x*x + y*y;
    if (l2 < 0.0001f) { x = y = 0.0f; return; }
    float inv = 1.0f / sqrtf(l2);
    x *= inv; y *= inv;
  }

  void resetRun() {
    for (int i = 0; i < kMaxEnemies; ++i) enemies_[i].active = false;
    for (int i = 0; i < kMaxParticles; ++i) particles_[i].active = false;
    for (int i = 0; i < kMaxPickups; ++i) pickups_[i].active = false;
    uint32_t best = stats_.bestWave;
    stats_ = Stats{};
    stats_.bestWave = best > 0 ? best : 1;
    px_ = kScreenW * 0.5f;
    py_ = kScreenH * 0.58f;
    maxHp_ = 100.0f;
    hp_ = maxHp_;
    speed_ = 74.0f;
    damage_ = 17.0f;
    firePeriod_ = 0.30f;
    rapidTimer_ = 0.0f;
    fireCooldown_ = 0.0f;
    spawnCooldown_ = 0.15f;
    waveRest_ = 0.0f;
    spawnedThisWave_ = 0;
    requiredThisWave_ = 10;
    dead_ = false;
    paused_ = false;
    strncpy(perkLabel_, "BASELINE", sizeof(perkLabel_) - 1);
    perkLabel_[sizeof(perkLabel_) - 1] = 0;
  }

  int activeEnemies() const {
    int n = 0;
    for (int i = 0; i < kMaxEnemies; ++i) if (enemies_[i].active) ++n;
    return n;
  }

  void updateGyro(uint32_t nowMs, float ax, float ay, float gyroMag, bool manualIntent) {
    if (!gyroMode_) return;
    float rawSide = -(ay - neutralAy_);
    float rawFwd  =  (ax - neutralAx_);

    bool settling = nowMs < gyroSettleUntilMs_;
    bool quiet = gyroMag < 0.075f;
    bool smallDrift = fabsf(rawSide) < 0.86f && fabsf(rawFwd) < 0.86f;
    if (settling || (!manualIntent && quiet && smallDrift)) {
      float beta = settling ? 0.20f : 0.018f;
      neutralAx_ = neutralAx_ * (1.0f - beta) + ax * beta;
      neutralAy_ = neutralAy_ * (1.0f - beta) + ay * beta;
      rawSide = -(ay - neutralAy_);
      rawFwd = ax - neutralAx_;
    }

    float targetSide = constrain(rawSide * 1.20f, -1.0f, 1.0f);
    float targetFwd  = constrain(rawFwd * 1.10f, -1.0f, 1.0f);
    if (fabsf(targetSide) < 0.12f) targetSide = 0.0f;
    if (fabsf(targetFwd) < 0.12f) targetFwd = 0.0f;
    if (settling) targetSide = targetFwd = 0.0f;
    gyroSide_ = gyroSide_ * 0.90f + targetSide * 0.10f;
    gyroFwd_  = gyroFwd_ * 0.90f + targetFwd * 0.10f;
  }

  void simulateStep(float dt) {
    if (fireCooldown_ > 0.0f) fireCooldown_ -= dt;
    if (rapidTimer_ > 0.0f) rapidTimer_ -= dt;
    if (spawnCooldown_ > 0.0f) spawnCooldown_ -= dt;
    if (waveRest_ > 0.0f) waveRest_ -= dt;

    px_ += moveX_ * speed_ * dt;
    py_ += moveY_ * speed_ * dt;
    px_ = constrain(px_, 7.0f, (float)kScreenW - 7.0f);
    py_ = constrain(py_, 15.0f, (float)kScreenH - 11.0f);

    if (waveRest_ <= 0.0f && spawnedThisWave_ < requiredThisWave_ && spawnCooldown_ <= 0.0f) {
      spawnEnemy();
      ++spawnedThisWave_;
      float rate = 0.42f - 0.015f * (float)stats_.wave;
      spawnCooldown_ = constrain(rate, 0.11f, 0.42f);
    }

    if (fireRequested_ && fireCooldown_ <= 0.0f) fireNearest();

    for (int i = 0; i < kMaxEnemies; ++i) {
      Enemy& e = enemies_[i];
      if (!e.active) continue;
      float dx = px_ - e.x;
      float dy = py_ - e.y;
      float d2 = dx*dx + dy*dy;
      float d = sqrtf(d2) + 0.001f;
      e.x += (dx / d) * e.speed * dt;
      e.y += (dy / d) * e.speed * dt;
      if (d < 8.0f) {
        hp_ -= e.contactDps * dt;
        if (hp_ <= 0.0f) {
          hp_ = 0.0f;
          dead_ = true;
          if (stats_.wave > stats_.bestWave) stats_.bestWave = stats_.wave;
        }
      }
    }

    for (int i = 0; i < kMaxPickups; ++i) {
      Pickup& p = pickups_[i];
      if (!p.active) continue;
      p.life -= dt;
      if (p.life <= 0.0f) { p.active = false; continue; }
      float d2 = sq(px_ - p.x) + sq(py_ - p.y);
      if (d2 < 70.0f) {
        if (p.kind == XP) addXp(3);
        else if (p.kind == MED) hp_ = constrain(hp_ + maxHp_ * 0.28f, 0.0f, maxHp_);
        else rapidTimer_ = 7.0f;
        p.active = false;
      }
    }

    for (int i = 0; i < kMaxParticles; ++i) {
      Particle& p = particles_[i];
      if (!p.active) continue;
      p.x += p.vx * dt;
      p.y += p.vy * dt;
      p.life -= dt;
      if (p.life <= 0.0f) p.active = false;
    }

    if (spawnedThisWave_ >= requiredThisWave_ && activeEnemies() == 0 && waveRest_ <= 0.0f) {
      ++stats_.wave;
      if (stats_.wave > stats_.bestWave) stats_.bestWave = stats_.wave;
      spawnedThisWave_ = 0;
      requiredThisWave_ = 9 + stats_.wave * 3;
      waveRest_ = 1.25f;
      hp_ = constrain(hp_ + maxHp_ * 0.18f, 0.0f, maxHp_);
      strncpy(perkLabel_, "WAVE CLEAR", sizeof(perkLabel_) - 1);
    }
  }

  void spawnEnemy() {
    int slot = -1;
    for (int i = 0; i < kMaxEnemies; ++i) if (!enemies_[i].active) { slot = i; break; }
    if (slot < 0) return;
    Enemy& e = enemies_[slot];
    int r = random(0, 100);
    if (stats_.wave % 5 == 0 && spawnedThisWave_ == requiredThisWave_ - 1) e.kind = BOSS;
    else if (r < 18) e.kind = FAST;
    else if (r < 30) e.kind = TANK;
    else e.kind = GRUNT;

    int edge = random(0, 4);
    if (edge == 0) { e.x = 2; e.y = random(16, kScreenH - 8); }
    else if (edge == 1) { e.x = kScreenW - 2; e.y = random(16, kScreenH - 8); }
    else if (edge == 2) { e.x = random(2, kScreenW - 2); e.y = 14; }
    else { e.x = random(2, kScreenW - 2); e.y = kScreenH - 3; }

    float scale = 1.0f + 0.10f * (float)(stats_.wave - 1);
    if (e.kind == FAST) { e.maxHp = 16.0f * scale; e.speed = 39.0f + stats_.wave; e.contactDps = 12.0f * scale; }
    else if (e.kind == TANK) { e.maxHp = 62.0f * scale; e.speed = 17.0f; e.contactDps = 20.0f * scale; }
    else if (e.kind == BOSS) { e.maxHp = 185.0f * scale; e.speed = 14.0f; e.contactDps = 30.0f * scale; }
    else { e.maxHp = 27.0f * scale; e.speed = 25.0f + stats_.wave * 0.4f; e.contactDps = 15.0f * scale; }
    e.hp = e.maxHp;
    e.active = true;
  }

  void fireNearest() {
    int target = -1;
    float best = 1e9f;
    for (int i = 0; i < kMaxEnemies; ++i) {
      if (!enemies_[i].active) continue;
      float d2 = sq(px_ - enemies_[i].x) + sq(py_ - enemies_[i].y);
      if (d2 < best) { best = d2; target = i; }
    }
    if (target < 0) return;
    ++stats_.shots;
    Enemy& e = enemies_[target];
    ++stats_.hits;
    e.hp -= damage_;
    spawnShotFx(e.x, e.y);
    fireCooldown_ = rapidTimer_ > 0.0f ? firePeriod_ * 0.47f : firePeriod_;
    if (e.hp <= 0.0f) killEnemy(target);
  }

  void killEnemy(int idx) {
    Enemy& e = enemies_[idx];
    bool boss = e.kind == BOSS;
    e.active = false;
    ++stats_.kills;
    addXp(boss ? 8 : 2);
    spawnBurst(e.x, e.y, boss ? TFT_MAGENTA : TFT_ORANGE, boss ? 16 : 7);
    int roll = random(0, 100);
    if (boss || roll < 20) spawnPickup(e.x, e.y, boss ? RAPID : (roll < 8 ? MED : XP));
  }

  void addXp(uint32_t n) {
    stats_.xp += n;
    while (stats_.xp >= stats_.xpNeed) {
      stats_.xp -= stats_.xpNeed;
      ++stats_.level;
      stats_.xpNeed = 10 + stats_.level * 6;
      applyRandomPerk();
    }
  }

  void applyRandomPerk() {
    int p = random(0, 4);
    if (p == 0) {
      damage_ *= 1.14f;
      strncpy(perkLabel_, "DMG +14%", sizeof(perkLabel_) - 1);
    } else if (p == 1) {
      speed_ *= 1.08f;
      strncpy(perkLabel_, "SPEED +8%", sizeof(perkLabel_) - 1);
    } else if (p == 2) {
      maxHp_ += 15.0f;
      hp_ = constrain(hp_ + 15.0f, 0.0f, maxHp_);
      strncpy(perkLabel_, "MAX HP +15", sizeof(perkLabel_) - 1);
    } else {
      firePeriod_ *= 0.90f;
      if (firePeriod_ < 0.10f) firePeriod_ = 0.10f;
      strncpy(perkLabel_, "ROF +10%", sizeof(perkLabel_) - 1);
    }
    perkLabel_[sizeof(perkLabel_) - 1] = 0;
  }

  void spawnPickup(float x, float y, PickupKind kind) {
    for (int i = 0; i < kMaxPickups; ++i) {
      if (pickups_[i].active) continue;
      pickups_[i].active = true;
      pickups_[i].kind = kind;
      pickups_[i].x = x;
      pickups_[i].y = y;
      pickups_[i].life = 10.0f;
      return;
    }
  }

  void spawnShotFx(float x, float y) {
    for (int i = 0; i < kMaxParticles; ++i) {
      if (particles_[i].active) continue;
      particles_[i].active = true;
      particles_[i].x = x;
      particles_[i].y = y;
      particles_[i].vx = random(-40, 41);
      particles_[i].vy = random(-40, 41);
      particles_[i].life = 0.12f;
      particles_[i].color = TFT_WHITE;
      return;
    }
  }

  void spawnBurst(float x, float y, uint16_t color, int count) {
    for (int n = 0; n < count; ++n) {
      for (int i = 0; i < kMaxParticles; ++i) {
        if (particles_[i].active) continue;
        float a = random(0, 6283) * 0.001f;
        float s = random(18, 70);
        particles_[i].active = true;
        particles_[i].x = x;
        particles_[i].y = y;
        particles_[i].vx = cosf(a) * s;
        particles_[i].vy = sinf(a) * s;
        particles_[i].life = random(15, 45) * 0.01f;
        particles_[i].color = color;
        break;
      }
    }
  }

  void drawBackground(M5GFX& d) {
    uint16_t c1 = d.color565(6, 9, 16);
    uint16_t c2 = d.color565(11, 18, 25);
    for (int y = 12; y < kScreenH; y += 16) {
      for (int x = 0; x < kScreenW; x += 16) {
        bool alt = (((x >> 4) + (y >> 4) + (stats_.wave & 1)) & 1) != 0;
        d.fillRect(x, y, 16, 16, alt ? c1 : c2);
      }
    }
    // sparse procedural ruins/noita-like pixels
    uint32_t seed = stats_.wave * 2654435761UL;
    for (int i = 0; i < 18; ++i) {
      seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
      int x = 4 + (seed % (kScreenW - 8));
      int y = 17 + ((seed >> 8) % (kScreenH - 25));
      d.drawPixel(x, y, d.color565(28, 42, 48));
    }
  }

  void drawEnemy(M5GFX& d, const Enemy& e) {
    uint16_t c = TFT_CYAN;
    int r = 3;
    if (e.kind == FAST) { c = TFT_PINK; r = 2; }
    else if (e.kind == TANK) { c = TFT_YELLOW; r = 4; }
    else if (e.kind == BOSS) { c = TFT_MAGENTA; r = 6; }
    d.fillCircle((int)e.x, (int)e.y, r, c);
    if (e.kind == BOSS) d.drawCircle((int)e.x, (int)e.y, r + 2, TFT_WHITE);
    int w = r * 2 + 2;
    int fill = (int)((float)w * constrain(e.hp / e.maxHp, 0.0f, 1.0f));
    d.drawFastHLine((int)e.x - w/2, (int)e.y - r - 3, w, TFT_DARKGREY);
    if (fill > 0) d.drawFastHLine((int)e.x - w/2, (int)e.y - r - 3, fill, TFT_RED);
  }

  uint16_t healthColor565(M5GFX& d) const {
    AlienRgb c = AlienLedPolicy::healthColor(health01());
    return d.color565(c.r, c.g, c.b);
  }
};

}  // namespace janus_adv_elite
