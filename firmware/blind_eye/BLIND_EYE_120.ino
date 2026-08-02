void initMotionBase() {
#if JANUS_MOTION_BASE_ENABLE
  // Official M5Atomic-Motion wiring for AtomS3 / AtomS3R:
  // Motion Base MCU = I2C 0x38 on SDA=38 / SCL=39.
  // Servo registers: 0..3. DC motor registers: 32..33.
  // v2.14C: the base is OPTIONAL. If it is not present, BlindEye continues as
  // a normal TMOS/RF/ESP-NOW swarm sensor. No endless fault mode.
  motionBaseScanBuses();

  motionBaseUsesMainWire = true;
  motionWireStarted = janusSelectMotionBus(true);
  motionBasePresent = motionWireStarted && motionI2cProbeOnSelectedBus(JANUS_MOTION_BASE_I2C_ADDR);
  motionBaseEverDetected = motionBaseEverDetected || motionBasePresent;
  motionBaseOptionalAbsent = !motionBasePresent;
  if (motionBaseOptionalAbsent && !motionBaseAbsentSinceMs) motionBaseAbsentSinceMs = millis();

  motionBasePowerPresent = false;
  motionBasePowerAddr = 0;
  motionBaseServoAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
  motionBaseTargetAngle = JANUS_MOTION_BASE_TRACK_CENTER_DEG;
  motionBaseLastSentAngle = -1;
  motionBaseMotorSpeed[0] = 0;
  motionBaseMotorSpeed[1] = 0;

  if (!motionBasePresent) {
    // Do not count absence as an I2C fault. It is a supported hardware profile.
    motionBaseI2cErrors = 0;
    motionBaseBusMv = 0;
    motionBaseCurrentRaw = 0;
    motionBasePowerRaw = 0;
    motionBasePowerFlags = 0;
    motionBasePowerSource = 5;   // local name: NOBASE
    motionBaseBatteryPct = 0;
    motionBaseArmed = false;
    roboZombieCrawlerManualEnable = false;
    roboZombieLastLeftSpeed = 0;
    roboZombieLastRightSpeed = 0;
    roboZombieLastLeftValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    roboZombieLastRightValue = JANUS_ROBOZOMBIE_SERVO_STOP;
    janusSelectGroveBus(true);

    Serial.printf("[MOTIONBASE] OPTIONAL ABSENT -> штатный SENSOR MODE. expected=0x%02X on SDA%u/SCL%u; TMOS/RF/ESP-NOW/miner continue.\n",
                  (unsigned)JANUS_MOTION_BASE_I2C_ADDR,
                  (unsigned)JANUS_MOTION_BASE_SDA_PIN,
                  (unsigned)JANUS_MOTION_BASE_SCL_PIN);
    Serial.printf("[ROBOZOMBIE] mode=%s base=0 headConfigured=%u leftConfigured=%u rightConfigured=%u. Actuators disabled, sensor brain alive.\n",
                  roboZombieBodyModeName(),
                  roboZombieHeadPresent ? 1 : 0,
                  roboZombieLeftLegPresent ? 1 : 0,
                  roboZombieRightLegPresent ? 1 : 0);
    motionBaseSendPowerPacket(true);
    return;
  }

  if (motionI2cProbeOnSelectedBus(JANUS_MOTION_BASE_INA226_ADDR_A)) {
    motionBasePowerPresent = true;
    motionBasePowerAddr = JANUS_MOTION_BASE_INA226_ADDR_A;
  } else if (motionI2cProbeOnSelectedBus(JANUS_MOTION_BASE_INA226_ADDR_B)) {
    motionBasePowerPresent = true;
    motionBasePowerAddr = JANUS_MOTION_BASE_INA226_ADDR_B;
  }
  janusSelectGroveBus(true);

  motionBaseSafeStop();
  motionBaseReadPower();
  motionBaseUpdateBatteryState();

  Serial.printf("[MOTIONBASE] Atomic Motion Base v1.2 OFFICIAL present=%u power=%u addr=0x%02X bus=WireMux-38/39 batt=%u%% mv=%d flags=0x%02X writes=%u trackServo=S%u span=%d..%d i2cErr=%lu\n",
                motionBasePresent ? 1 : 0, motionBasePowerPresent ? 1 : 0,
                motionBasePowerAddr,
                (unsigned)motionBaseBatteryPct, (int)motionBaseBusMv, (unsigned)motionBasePowerFlags,
                (unsigned)JANUS_MOTION_BASE_WRITE_ENABLE,
                (unsigned)(JANUS_MOTION_BASE_TRACK_SERVO_CH + 1),
                JANUS_MOTION_BASE_TRACK_MIN_DEG, JANUS_MOTION_BASE_TRACK_MAX_DEG,
                (unsigned long)motionBaseI2cErrors);

  Serial.printf("[ROBOZOMBIE] modular mode=%s headP=%u leftP=%u rightP=%u auto=%u pull=%u keys: S passive, s stop, a/g wake, h/j/k toggle installed parts, g manual crawl\n",
                roboZombieBodyModeName(), roboZombieHeadPresent ? 1 : 0,
                roboZombieLeftLegPresent ? 1 : 0, roboZombieRightLegPresent ? 1 : 0,
                (unsigned)JANUS_ROBOZOMBIE_AUTO_CRAWL_ENABLE, (unsigned)roboZombieBasePull);

#endif
}


void motionBaseSafeStop() {
#if JANUS_MOTION_BASE_ENABLE
  motionBaseMotorSpeed[0] = 0;
  motionBaseMotorSpeed[1] = 0;
#if JANUS_MOTION_BASE_WRITE_ENABLE
  if (motionBasePresent) {
    motionI2cWrite8(JANUS_MOTION_BASE_I2C_ADDR, 0x20, 0);
    motionI2cWrite8(JANUS_MOTION_BASE_I2C_ADDR, 0x21, 0);
  }
#endif
  motionBaseStopCrawler("safe-stop");
#endif
}

uint8_t motionBaseEstimateBatteryPct(uint16_t mv) {
  if (mv == 0) return 0;
  // Atomic Motion Base usually sees a 1S Li-ion/LiPo pack through INA226.
  // If the bus is above Li-ion range, treat it as external/boost/USB-like power.
  if (mv >= 4400) return 100;
  struct P { uint16_t mv; uint8_t pct; } curve[] = {
    {4200,100}, {4120,92}, {4060,84}, {4000,76}, {3940,68}, {3880,60},
    {3820,52}, {3760,44}, {3710,36}, {3660,28}, {3600,20}, {3520,12},
    {3440,6}, {3350,2}, {3200,0}
  };
  if (mv >= curve[0].mv) return curve[0].pct;
  for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i) {
    if (mv >= curve[i].mv) {
      uint16_t hiMv = curve[i - 1].mv, loMv = curve[i].mv;
      uint8_t hiPct = curve[i - 1].pct, loPct = curve[i].pct;
      float t = (float)(mv - loMv) / (float)max((int)(hiMv - loMv), 1);
      return (uint8_t)constrain((int)roundf((float)loPct + t * (float)(hiPct - loPct)), 0, 100);
    }
  }
  return 0;
}

void motionBaseUpdateBatteryState() {
#if JANUS_MOTION_BASE_ENABLE
  motionBasePowerFlags = 0;
  if (!motionBasePresent) {
    // v2.14C: base is optional and absent is not a battery fault.
    motionBasePowerSource = 5; // NOBASE
    motionBaseBatteryPct = 0;
    motionBaseBusMv = 0;
    motionBaseCurrentRaw = 0;
    motionBasePowerRaw = 0;
    return;
  }
  if (motionBasePresent) motionBasePowerFlags |= 0x01;
  if (motionBasePowerPresent) motionBasePowerFlags |= 0x02;

  if (motionBasePowerPresent && motionBaseBusMv > 0) {
    const bool external = motionBaseBusMv >= JANUS_MOTION_BASE_EXT_MV;
    const bool rawCurrentSeen = abs((int)motionBaseCurrentRaw) >= JANUS_MOTION_BASE_CHG_CURRENT_MIN;

    if (!external) {
      // Real 1S cell range. This is the only moment where INA bus voltage can be used
      // as a believable battery percentage.
      motionBasePowerSource = 1;
      motionBaseLastCellMv = (uint16_t)motionBaseBusMv;
      motionBaseBatteryPct = motionBaseEstimateBatteryPct(motionBaseLastCellMv);
      motionBaseLastCellPct = motionBaseBatteryPct;
      motionBaseExternalSinceMs = 0;
      if (motionBaseBusMv < JANUS_MOTION_BASE_LOW_MV) motionBasePowerFlags |= 0x08;
      if (motionBaseBusMv < JANUS_MOTION_BASE_SLEEP_MV) motionBasePowerFlags |= 0x10;
    } else {
      // USB-C / boost / charger rail. Do NOT blindly report 100%: the INA226 now sees
      // the powered rail, not necessarily the bare cell voltage. Keep last known cell
      // estimate and mark the packet as charge-aware.
      motionBasePowerFlags |= 0x04; // external/USB present
      if (!motionBaseExternalSinceMs) motionBaseExternalSinceMs = millis();

      uint8_t heldPct = motionBaseLastCellMv ? motionBaseLastCellPct : motionBaseEstimateBatteryPct(JANUS_MOTION_BASE_FULL_MV);
      bool estimateOnly = motionBaseLastCellMv == 0;
      if (estimateOnly) motionBasePowerFlags |= 0x80;

      bool likelyFull = (!estimateOnly && heldPct >= 96) || (motionBaseBusMv >= 5000 && !rawCurrentSeen && heldPct >= 92);
      bool likelyCharging = !likelyFull;

      if (likelyCharging) {
        motionBasePowerSource = 3; // charging / USB-C attached
        motionBasePowerFlags |= 0x20;
      } else {
        motionBasePowerSource = 4; // full/float/external hold
        motionBasePowerFlags |= 0x40;
      }
      motionBaseBatteryPct = constrain((int)heldPct, 0, 100);
    }
  } else {
    motionBasePowerSource = 0;
    motionBaseBatteryPct = 0;
  }
#else
  motionBasePowerFlags = 0; motionBasePowerSource = 0; motionBaseBatteryPct = 0;
#endif
}

uint32_t janusEyePowerCrc32(const void* data, size_t len) {
  const uint8_t* p = (const uint8_t*)data;
  uint32_t h = 2166136261UL;
  for (size_t i = 0; i < len; ++i) { h ^= p[i]; h *= 16777619UL; }
  return h;
}


const char* motionBasePowerSourceName(uint8_t src, uint8_t flags) {
  if (src == 5 || !motionBasePresent) return "NOBASE";
  if (flags & 0x20) return "CHG";
  if (flags & 0x40) return "FULL";
  if (src == 3) return "CHG";
  if (src == 4) return "FULL";
  if (src == 2 || (flags & 0x04)) return "EXT";
  if (src == 1) return "BAT";
  return "UNK";
}

void motionBaseSendPowerPacket(bool force) {
#if JANUS_MOTION_BASE_ENABLE
  uint32_t now = millis();
  uint32_t txInterval = motionBasePresent ? JANUS_EYE_POWER_TX_MS : JANUS_MOTION_BASE_ABSENT_STATUS_MS;
  if (!force && now - motionBaseLastBatteryTxMs < txInterval) return;
  motionBaseLastBatteryTxMs = now;
  motionBaseUpdateBatteryState();

  JanusEyePowerPacket eb{};
  eb.magic[0] = 'E'; eb.magic[1] = 'B';
  eb.version = 1;
  eb.flags = motionBasePowerFlags;
  strlcpy(eb.nodeId, "BlindEye", sizeof(eb.nodeId));
  eb.seq = ++motionBaseBatterySeq;
  eb.uptime_ms = now;
  eb.bus_mv = (uint16_t)constrain((int)motionBaseBusMv, 0, 65535);
  eb.current_raw = motionBaseCurrentRaw;
  eb.power_raw = motionBasePowerRaw;
  eb.battery_pct = motionBaseBatteryPct;
  eb.source = motionBasePowerSource;
  eb.servo_angle = (uint16_t)constrain((int)motionBaseServoAngle, 0, 65535);
  eb.target_angle = (uint16_t)constrain((int)motionBaseTargetAngle, 0, 65535);
  eb.crc = 0;
  eb.crc = janusEyePowerCrc32(&eb, sizeof(eb));
  bool ok = janusEyeEspNowSend("E/B", &eb, sizeof(eb), true);
  if (force || eb.seq <= 3 || (eb.seq % 20UL) == 0) {
    Serial.printf("[EYE/BATT] tx=%s seq=%lu pct=%u mv=%u flags=0x%02X src=%u/%s cell=%umV cur=%d pwr=%d\n",
                  ok ? "OK" : "FAIL", (unsigned long)eb.seq, (unsigned)eb.battery_pct,
                  (unsigned)eb.bus_mv, (unsigned)eb.flags, (unsigned)eb.source,
                  motionBasePowerSourceName(eb.source, eb.flags), (unsigned)motionBaseLastCellMv,
                  (int)eb.current_raw, (int)eb.power_raw);
  }
#else
  (void)force;
#endif
}

void motionBaseReadPower() {
#if JANUS_MOTION_BASE_ENABLE
  if (!motionBasePowerPresent || !motionBasePowerAddr) return;
  uint16_t bus = 0, current = 0, power = 0;
  if (motionI2cRead16(motionBasePowerAddr, 0x02, bus)) {
    // INA226 bus voltage LSB is 1.25mV.
    motionBaseBusMv = (int16_t)constrain((int)((uint32_t)bus * 125UL / 100UL), 0, 32767);
  }
  if (motionI2cRead16(motionBasePowerAddr, 0x04, current)) motionBaseCurrentRaw = (int16_t)current;
  if (motionI2cRead16(motionBasePowerAddr, 0x03, power)) motionBasePowerRaw = (int16_t)power;
  motionBaseUpdateBatteryState();
#endif
}

int16_t motionBaseSectorToAngle(uint8_t sector) {
  sector %= JANUS_KENSHI_SECTORS;
  float t = (JANUS_KENSHI_SECTORS <= 1) ? 0.5f : ((float)sector / (float)(JANUS_KENSHI_SECTORS - 1));
  return (int16_t)constrain((int)roundf(JANUS_MOTION_BASE_TRACK_MIN_DEG + t * (JANUS_MOTION_BASE_TRACK_MAX_DEG - JANUS_MOTION_BASE_TRACK_MIN_DEG)),
                            JANUS_MOTION_BASE_TRACK_MIN_DEG, JANUS_MOTION_BASE_TRACK_MAX_DEG);
}

