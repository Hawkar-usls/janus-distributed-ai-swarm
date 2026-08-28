#pragma once

#include <M5Cardputer.h>
#include <math.h>
#include "ADV_Elite_alien_survival_policy.h"

namespace janus_adv_elite {

// RC3 Pixel-Biome shooter.
// Visual donor ideas: primordial/deep-space palette, parallax spores, organic
// silhouettes and short-lived FX from the supplied Neural Swarm sketches.
// Gameplay remains the ADV top-down survival shooter and stays SIMULATED.
class AlienSurvivalRuntime {
 public:
  static constexpr int kScreenW = 240;
  static constexpr int kScreenH = 135;
  static constexpr int kMaxEnemies = 30;
  static constexpr int kMaxParticles = 72;
  static constexpr int kMaxPickups = 14;
  static constexpr int kMaxBullets = 26;
  static constexpr int kBgSpores = 24;

  enum EnemyKind : uint8_t { GRUNT = 0, FAST = 1, TANK = 2, BOSS = 3 };
  enum PickupKind : uint8_t { XP = 0, MED = 1, RAPID = 2 };

  struct Enemy {
    bool active = false;
    EnemyKind kind = GRUNT;
    float x = 0, y = 0;
    float hp = 0, maxHp = 0;
    float speed = 0;
    float contactDps = 0;
    float phase = 0;
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

  struct Bullet {
    bool active = false;
    float x = 0, y = 0, vx = 0, vy = 0;
    float damage = 0;
    float life = 0;
  };

  struct BgSpore {
    float x = 0, y = 0;
    float speed = 0;
    float phase = 0;
    uint8_t radius = 1;
    uint8_t layer = 0;
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

  AlienSurvivalRuntime() : frame_(&M5Cardputer.Display) {}

  void begin() {
    // ZIM-style: do not reserve a game framebuffer while HOME/Radio owns foreground.
    resetRun();
    lastWallMs_ = millis();
    lastDrawMs_ = lastWallMs_;
  }

  void enter() {
    ensureFrame();
    active_ = true;
    lastWallMs_ = millis();
    accumulator_ = 0.0f;
    if (dead_) resetRun();
  }

  void leave() {
    active_ = false;
    paused_ = false;
    // Return sprite RAM to the rest of JANUS when game is not foreground.
    if (frameReady_) { frame_.deleteSprite(); frameReady_ = false; }
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

    // Background is presentation-only and continues while paused/dead.
    updateBackground(wallDt, nowMs);
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
    if (fabsf(moveX_) + fabsf(moveY_) > 0.05f) {
      facingX_ = moveX_;
      facingY_ = moveY_;
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

  void draw(M5GFX& display) {
    if (!active_) return;
    const uint32_t now = millis();
    const uint32_t frameMs = now - lastDrawMs_;
    lastDrawMs_ = now;
    if (frameMs > 0) {
      float fps = 1000.0f / (float)frameMs;
      fpsEma_ = fpsEma_ * 0.90f + fps * 0.10f;
    }

    if (frameReady_) {
      drawScene(frame_);
      frame_.pushSprite(0, 0);
    } else {
      drawScene(display);
    }
  }

 private:
  Enemy enemies_[kMaxEnemies];
  Particle particles_[kMaxParticles];
  Pickup pickups_[kMaxPickups];
  Bullet bullets_[kMaxBullets];
  BgSpore bgSpores_[kBgSpores];
  Stats stats_;
  M5Canvas frame_;
  bool frameReady_ = false;

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
  float facingX_ = 0.0f, facingY_ = -1.0f;
  float muzzleFlash_ = 0.0f;
  float hurtFlash_ = 0.0f;
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

  void ensureFrame() {
    if (frameReady_) return;
    frame_.setColorDepth(8);
    frame_.createSprite(kScreenW, kScreenH);
    frameReady_ = frame_.width() == kScreenW && frame_.height() == kScreenH;
  }

  static float dist2(float x, float y) { return x*x + y*y; }

  static void normalize(float& x, float& y) {
    float l2 = x*x + y*y;
    if (l2 < 0.0001f) { x = y = 0.0f; return; }
    float inv = 1.0f / sqrtf(l2);
    x *= inv; y *= inv;
  }

  void initBackground() {
    for (int i = 0; i < kBgSpores; ++i) {
      bgSpores_[i].x = (float)random(0, kScreenW);
      bgSpores_[i].y = (float)random(12, kScreenH);
      bgSpores_[i].layer = (uint8_t)(i % 3);
      bgSpores_[i].radius = (uint8_t)(1 + (i % 3));
      bgSpores_[i].speed = 1.6f + bgSpores_[i].layer * 2.2f + random(0, 20) * 0.05f;
      bgSpores_[i].phase = random(0, 6283) * 0.001f;
    }
  }

  void updateBackground(float dt, uint32_t nowMs) {
    float t = nowMs * 0.001f;
    for (int i = 0; i < kBgSpores; ++i) {
      BgSpore& s = bgSpores_[i];
      s.y -= s.speed * dt;
      s.x += sinf(t * (0.28f + 0.08f * s.layer) + s.phase) * (0.12f + 0.05f * s.layer);
      if (s.y < 12.0f - s.radius) {
        s.y = kScreenH + (float)random(1, 15);
        s.x = (float)random(0, kScreenW);
      }
      if (s.x < -4.0f) s.x = kScreenW + 3.0f;
      if (s.x > kScreenW + 4.0f) s.x = -3.0f;
    }
  }

  void resetRun() {
    for (int i = 0; i < kMaxEnemies; ++i) enemies_[i].active = false;
    for (int i = 0; i < kMaxParticles; ++i) particles_[i].active = false;
    for (int i = 0; i < kMaxPickups; ++i) pickups_[i].active = false;
    for (int i = 0; i < kMaxBullets; ++i) bullets_[i].active = false;
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
    muzzleFlash_ = hurtFlash_ = 0.0f;
    spawnCooldown_ = 0.15f;
    waveRest_ = 0.0f;
    spawnedThisWave_ = 0;
    requiredThisWave_ = 10;
    dead_ = false;
    paused_ = false;
    facingX_ = 0.0f;
    facingY_ = -1.0f;
    strncpy(perkLabel_, "BASELINE", sizeof(perkLabel_) - 1);
    perkLabel_[sizeof(perkLabel_) - 1] = 0;
    initBackground();
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
    if (muzzleFlash_ > 0.0f) muzzleFlash_ -= dt;
    if (hurtFlash_ > 0.0f) hurtFlash_ -= dt;

    px_ += moveX_ * speed_ * dt;
    py_ += moveY_ * speed_ * dt;
    px_ = constrain(px_, 8.0f, (float)kScreenW - 8.0f);
    py_ = constrain(py_, 16.0f, (float)kScreenH - 12.0f);

    if (waveRest_ <= 0.0f && spawnedThisWave_ < requiredThisWave_ && spawnCooldown_ <= 0.0f) {
      spawnEnemy();
      ++spawnedThisWave_;
      float rate = 0.42f - 0.015f * (float)stats_.wave;
      spawnCooldown_ = constrain(rate, 0.11f, 0.42f);
    }

    if (fireRequested_ && fireCooldown_ <= 0.0f) fireNearest();
    updateBullets(dt);

    for (int i = 0; i < kMaxEnemies; ++i) {
      Enemy& e = enemies_[i];
      if (!e.active) continue;
      float dx = px_ - e.x;
      float dy = py_ - e.y;
      float d2 = dx*dx + dy*dy;
      float d = sqrtf(d2) + 0.001f;
      // FAST enemies weave slightly; other classes keep deterministic pursuit.
      float wx = 0.0f, wy = 0.0f;
      if (e.kind == FAST) {
        float wiggle = sinf(e.phase + millis() * 0.008f) * 0.28f;
        wx = (-dy / d) * wiggle;
        wy = ( dx / d) * wiggle;
      }
      e.x += ((dx / d) + wx) * e.speed * dt;
      e.y += ((dy / d) + wy) * e.speed * dt;
      float contactR = e.kind == BOSS ? 12.0f : (e.kind == TANK ? 9.0f : 7.0f);
      if (d < contactR) {
        hp_ -= e.contactDps * dt;
        hurtFlash_ = 0.10f;
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
      if (dist2(px_ - p.x, py_ - p.y) < 70.0f) {
        if (p.kind == XP) addXp(3);
        else if (p.kind == MED) hp_ = constrain(hp_ + maxHp_ * 0.28f, 0.0f, maxHp_);
        else rapidTimer_ = 7.0f;
        spawnBurst(p.x, p.y, p.kind == MED ? TFT_GREEN : TFT_CYAN, 5);
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
    e.phase = random(0, 6283) * 0.001f;
    e.active = true;
  }

  void fireNearest() {
    int target = -1;
    float best = 1e9f;
    for (int i = 0; i < kMaxEnemies; ++i) {
      if (!enemies_[i].active) continue;
      float d2 = dist2(px_ - enemies_[i].x, py_ - enemies_[i].y);
      if (d2 < best) { best = d2; target = i; }
    }
    if (target < 0) return;

    float dx = enemies_[target].x - px_;
    float dy = enemies_[target].y - py_;
    normalize(dx, dy);
    if (fabsf(dx) + fabsf(dy) < 0.01f) { dx = facingX_; dy = facingY_; }
    facingX_ = dx; facingY_ = dy;

    for (int i = 0; i < kMaxBullets; ++i) {
      Bullet& b = bullets_[i];
      if (b.active) continue;
      b.active = true;
      b.x = px_ + dx * 6.0f;
      b.y = py_ + dy * 6.0f;
      b.vx = dx * 185.0f;
      b.vy = dy * 185.0f;
      b.damage = damage_;
      b.life = 0.90f;
      ++stats_.shots;
      muzzleFlash_ = 0.07f;
      fireCooldown_ = rapidTimer_ > 0.0f ? firePeriod_ * 0.47f : firePeriod_;
      return;
    }
  }

  void updateBullets(float dt) {
    for (int bi = 0; bi < kMaxBullets; ++bi) {
      Bullet& b = bullets_[bi];
      if (!b.active) continue;
      b.x += b.vx * dt;
      b.y += b.vy * dt;
      b.life -= dt;
      if (b.life <= 0.0f || b.x < 0 || b.x >= kScreenW || b.y < 11 || b.y >= kScreenH) {
        b.active = false;
        continue;
      }
      for (int ei = 0; ei < kMaxEnemies; ++ei) {
        Enemy& e = enemies_[ei];
        if (!e.active) continue;
        float hitR = e.kind == BOSS ? 8.0f : (e.kind == TANK ? 6.0f : 4.5f);
        if (dist2(b.x - e.x, b.y - e.y) <= hitR * hitR) {
          ++stats_.hits;
          e.hp -= b.damage;
          spawnImpact(b.x, b.y, e.kind == BOSS ? TFT_MAGENTA : TFT_WHITE);
          b.active = false;
          if (e.hp <= 0.0f) killEnemy(ei);
          break;
        }
      }
    }
  }

  void killEnemy(int idx) {
    Enemy& e = enemies_[idx];
    bool boss = e.kind == BOSS;
    e.active = false;
    ++stats_.kills;
    addXp(boss ? 8 : 2);
    spawnBurst(e.x, e.y, boss ? TFT_MAGENTA : TFT_ORANGE, boss ? 18 : 8);
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

  void spawnImpact(float x, float y, uint16_t color) {
    spawnBurst(x, y, color, 3);
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

  template <typename GFX>
  void drawScene(GFX& d) {
    drawBackground(d);

    for (int i = 0; i < kMaxPickups; ++i) {
      if (!pickups_[i].active) continue;
      drawPickup(d, pickups_[i]);
    }

    for (int i = 0; i < kMaxBullets; ++i) {
      if (!bullets_[i].active) continue;
      int x = (int)bullets_[i].x, y = (int)bullets_[i].y;
      d.drawPixel(x, y, TFT_WHITE);
      d.drawPixel(x - (int)(bullets_[i].vx * 0.010f), y - (int)(bullets_[i].vy * 0.010f), TFT_CYAN);
    }

    for (int i = 0; i < kMaxEnemies; ++i) {
      if (!enemies_[i].active) continue;
      drawEnemy(d, enemies_[i]);
    }

    for (int i = 0; i < kMaxParticles; ++i) {
      if (!particles_[i].active) continue;
      d.drawPixel((int)particles_[i].x, (int)particles_[i].y, particles_[i].color);
    }

    drawPlayer(d);
    drawHud(d);

    if (hurtFlash_ > 0.0f) {
      d.drawRect(0, 11, kScreenW, kScreenH - 11, TFT_RED);
    }

    if (paused_) {
      d.fillRect(58, 44, 124, 39, d.color565(4, 6, 12));
      d.drawRect(58, 44, 124, 39, TFT_YELLOW);
      d.setTextColor(TFT_YELLOW, d.color565(4, 6, 12));
      d.setCursor(101, 54); d.print("PAUSED");
      d.setTextColor(TFT_LIGHTGREY, d.color565(4, 6, 12));
      d.setCursor(72, 69); d.print("P resume | ESC home");
    }

    if (dead_) {
      d.fillRect(41, 37, 158, 59, d.color565(4, 6, 12));
      d.drawRect(41, 37, 158, 59, TFT_RED);
      d.setTextColor(TFT_RED, d.color565(4, 6, 12));
      d.setCursor(84, 48); d.print("BIOME LOST");
      d.setTextColor(TFT_WHITE, d.color565(4, 6, 12));
      d.setCursor(56, 63); d.printf("wave %lu  kills %lu", (unsigned long)stats_.wave, (unsigned long)stats_.kills);
      d.setCursor(70, 78); d.print("SPACE restart");
    }
  }

  template <typename GFX>
  void drawBackground(GFX& d) {
    const uint16_t abyss = d.color565(3, 6, 14);
    const uint16_t deep = d.color565(5, 11, 22);
    const uint16_t vein = d.color565(12, 25, 36);
    const uint16_t vein2 = d.color565(20, 35, 44);
    d.fillScreen(abyss);

    // Pixel-depth bands make the tiny 240x135 panel read as a deep cavern.
    d.fillRect(0, 34, kScreenW, 34, d.color565(4, 9, 19));
    d.fillRect(0, 68, kScreenW, kScreenH - 68, deep);

    // Deterministic ruins/veins per wave: stable scene, no random flicker.
    uint32_t seed = stats_.wave * 2654435761UL + 0xA17E31UL;
    for (int i = 0; i < 12; ++i) {
      seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
      int x = 4 + (seed % (kScreenW - 12));
      int y = 18 + ((seed >> 8) % (kScreenH - 28));
      int len = 5 + ((seed >> 16) % 18);
      if (seed & 1) d.drawFastHLine(x, y, min(len, kScreenW - x - 1), (seed & 4) ? vein : vein2);
      else d.drawFastVLine(x, y, min(len, kScreenH - y - 2), (seed & 4) ? vein : vein2);
      if ((seed & 7) == 0) d.drawCircle(x, y, 2, d.color565(18, 42, 48));
    }

    // Primordial parallax spores from the donor ecosystem visual language.
    for (int i = 0; i < kBgSpores; ++i) {
      const BgSpore& s = bgSpores_[i];
      uint16_t c = s.layer == 0 ? d.color565(8, 18, 29)
                   : (s.layer == 1 ? d.color565(12, 28, 37) : d.color565(18, 40, 45));
      if (s.radius <= 1) d.drawPixel((int)s.x, (int)s.y, c);
      else d.fillCircle((int)s.x, (int)s.y, s.radius, c);
    }
  }

  template <typename GFX>
  void drawPlayer(GFX& d) {
    int x = (int)px_, y = (int)py_;
    int fx = (int)roundf(facingX_ * 4.0f);
    int fy = (int)roundf(facingY_ * 4.0f);
    // Small pixel craft / cell-cannon hybrid, readable at arm's length.
    d.fillRect(x - 2, y - 2, 5, 5, TFT_WHITE);
    d.drawRect(x - 4, y - 3, 9, 7, TFT_CYAN);
    d.drawPixel(x - 4, y, TFT_MAGENTA);
    d.drawPixel(x + 4, y, TFT_MAGENTA);
    d.drawLine(x, y, x + fx, y + fy, TFT_CYAN);
    if (muzzleFlash_ > 0.0f) d.fillCircle(x + fx, y + fy, 2, TFT_YELLOW);
  }

  template <typename GFX>
  void drawEnemy(GFX& d, const Enemy& e) {
    int x = (int)e.x, y = (int)e.y;
    uint16_t c = TFT_CYAN;
    int r = 3;
    if (e.kind == FAST) { c = TFT_PINK; r = 3; }
    else if (e.kind == TANK) { c = TFT_YELLOW; r = 5; }
    else if (e.kind == BOSS) { c = TFT_MAGENTA; r = 7; }

    float dx = px_ - e.x, dy = py_ - e.y;
    normalize(dx, dy);
    float wiggle = sinf(millis() * 0.012f + e.phase);

    if (e.kind == GRUNT) {
      // Flagellated spore.
      d.fillCircle(x, y, 3, c);
      d.drawPixel(x, y, TFT_WHITE);
      d.drawLine(x - 2, y + 2, x - 5, y + 3 + (int)wiggle, c);
      d.drawLine(x + 2, y + 2, x + 5, y + 3 - (int)wiggle, c);
    } else if (e.kind == FAST) {
      // Dart/predator morphology.
      int hx = x + (int)(dx * 5), hy = y + (int)(dy * 5);
      int sx = (int)(-dy * 3), sy = (int)(dx * 3);
      d.fillTriangle(hx, hy, x - (int)(dx * 3) + sx, y - (int)(dy * 3) + sy,
                     x - (int)(dx * 3) - sx, y - (int)(dy * 3) - sy, c);
      d.drawPixel(x, y, TFT_WHITE);
    } else if (e.kind == TANK) {
      // Armoured cyst.
      d.fillCircle(x, y, 5, c);
      d.drawCircle(x, y, 5, TFT_WHITE);
      d.fillRect(x - 2, y - 2, 5, 5, d.color565(14, 17, 25));
      d.drawPixel(x, y, TFT_RED);
    } else {
      // Boss: pulsating layered primordial core.
      int pulse = (int)(1.0f + sinf(millis() * 0.008f + e.phase));
      d.fillCircle(x, y, 6 + pulse, c);
      d.drawCircle(x, y, 9 + pulse, TFT_WHITE);
      d.drawCircle(x, y, 11 + pulse, d.color565(90, 20, 100));
      d.fillCircle(x, y, 2, TFT_BLACK);
      d.drawPixel(x, y, TFT_WHITE);
    }

    int w = r * 2 + 4;
    int fill = (int)((float)w * constrain(e.hp / e.maxHp, 0.0f, 1.0f));
    d.drawFastHLine(x - w/2, y - r - 4, w, d.color565(38, 38, 42));
    if (fill > 0) d.drawFastHLine(x - w/2, y - r - 4, fill, TFT_RED);
  }

  template <typename GFX>
  void drawPickup(GFX& d, const Pickup& p) {
    int x = (int)p.x, y = (int)p.y;
    if (p.kind == MED) {
      d.fillRect(x - 3, y - 1, 7, 3, TFT_GREEN);
      d.fillRect(x - 1, y - 3, 3, 7, TFT_GREEN);
    } else if (p.kind == RAPID) {
      d.drawFastVLine(x - 2, y - 3, 5, TFT_YELLOW);
      d.drawLine(x - 2, y + 2, x + 2, y - 2, TFT_YELLOW);
      d.drawFastVLine(x + 2, y - 2, 5, TFT_YELLOW);
    } else {
      d.drawCircle(x, y, 3, TFT_CYAN);
      d.drawPixel(x, y, TFT_WHITE);
    }
  }

  template <typename GFX>
  void drawHud(GFX& d) {
    const uint16_t barBg = d.color565(2, 4, 9);
    d.fillRect(0, 0, kScreenW, 11, barBg);
    d.setTextSize(1);
    d.setTextColor(TFT_WHITE, barBg);
    d.setCursor(2, 2);
    d.printf("A W%lu L%lu K%lu", (unsigned long)stats_.wave, (unsigned long)stats_.level, (unsigned long)stats_.kills);
    d.setCursor(142, 2);
    d.printf("%s %s %dFPS", gyroMode_ ? "GY" : "FN", autoFire_ ? "AUTO" : "MAN", (int)fpsEma_);

    int hpW = 78;
    d.drawRect(2, kScreenH - 8, hpW, 5, d.color565(50, 50, 55));
    int hpFill = (int)((hpW - 2) * health01());
    if (hpFill > 0) d.fillRect(3, kScreenH - 7, hpFill, 3, healthColor565(d));

    float xp01 = stats_.xpNeed ? constrain((float)stats_.xp / (float)stats_.xpNeed, 0.0f, 1.0f) : 0.0f;
    d.drawRect(84, kScreenH - 8, 70, 5, d.color565(50, 50, 55));
    int xpFill = (int)(68 * xp01);
    if (xpFill > 0) d.fillRect(85, kScreenH - 7, xpFill, 3, TFT_MAGENTA);

    d.setTextColor(TFT_LIGHTGREY, d.color565(5, 11, 22));
    d.setCursor(160, kScreenH - 10);
    d.printf("%s", perkLabel_);
  }

  template <typename GFX>
  uint16_t healthColor565(GFX& d) const {
    AlienRgb c = AlienLedPolicy::healthColor(health01());
    return d.color565(c.r, c.g, c.b);
  }
};

}  // namespace janus_adv_elite
