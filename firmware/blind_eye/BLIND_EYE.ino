#include <M5Unified.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <M5_STHS34PF80.h>
#include <LittleFS.h>
#include <math.h>

// ========================= JANUS BLIND EYE v2.15A TMOS-PRIMARY / CAMERA-ABSENT =========================
// v2.15A keeps RF/policy/miner/blackboard compatibility, makes TMOS/PIR the primary eye,
// treats the missing camera as normal hardware, and fixes quiet-room artifact semantics.
#include <esp_now.h>
#include <esp_wifi.h>
#include <mbedtls/sha256.h>

#define JANUS_COLONY_ENABLE 1
#define JANUS_BROADCAST_CHANNEL 0
#define COLONY_HEARTBEAT_MS 2000UL
#define COLONY_ENTROPY_MS 2500UL
#define COLONY_MASTER_TIMEOUT_MS 18000UL
#define COLONY_REMOTE_BATCH 220
#define COLONY_JOB_RANGE_DEFAULT 262144UL
#define COLONY_NO_SELF_MINING 1

// v2.5: Buzz v10.11+ Agent rewards.
// Old Buzz builds ignore this; new Buzz sends 'A','R' packets with score,
// entropySeed and targetBatch. This worker never treats rewards as ACCEPT shares.
#define COLONY_AGENT_ENABLE 1
#define COLONY_AGENT_BATCH_MIN 80
#define COLONY_AGENT_BATCH_MAX 900
#define COLONY_AGENT_REWARD_VISIBLE_MS 6000UL

// v2.7: JANUS Kenshi Bubble Bus.
// This is a compatibility layer above ESP-NOW, not a replacement for old JANUS/E2 packets.
// Idea: only hot/near/important nodes are "materialized"; quiet nodes stay virtual
// as tiny timers, flags and Markov transition hints.
#define JANUS_KENSHI_BUS_ENABLE       1
#define JANUS_KENSHI_MAX_NODES        18
#define JANUS_KENSHI_SECTORS          8
#define JANUS_KENSHI_ACTIVE_TTL_MS    9000UL
#define JANUS_KENSHI_VIRTUAL_TTL_MS   45000UL
#define JANUS_KENSHI_ALERT_TX_MS      700UL
#define JANUS_KENSHI_ACTIVE_TX_MS     1500UL
#define JANUS_KENSHI_BG_TX_MS         5000UL
#define JANUS_KENSHI_SAVE_FILE        "/eye_kenshi.bin"

// v2.8: JANUS Tachyon Prophecy + TMOS aperture vision.
// The Python Physarious Movement Tachyon model cannot run on the Atom,
// so this is its embedded micro-brother: tiny temporal memory, viscosity,
// energy/stress gates, sector prediction and ESP-NOW prophecy exchange.
#define JANUS_TACHYON_PROPHECY_ENABLE      1
#define JANUS_TACHYON_SEQ_N                16
#define JANUS_TACHYON_REMOTE_N             10
#define JANUS_TACHYON_TX_BG_MS             2200UL
#define JANUS_TACHYON_TX_ALERT_MS          650UL
#define JANUS_TACHYON_REMOTE_TTL_MS        12000UL
#define JANUS_TACHYON_SAVE_FILE            "/eye_tachyon.bin"

// v2.15A: this particular ATOMS3R has no camera. That is a normal hardware profile.
// The STHS34PF80 TMOS/PIR sensor is the primary eye. Optional E/F frames are
// synthetic 8x8 thermal-aperture maps derived only from real TMOS/RF telemetry.
#define JANUS_EYE_CAMERA_PRESENT           0
#define JANUS_EYE_TMOS_PRIMARY             1
#define JANUS_EYE_TMOS_APERTURE_ENABLE     1
#define JANUS_EYE_VISION_ENABLE            JANUS_EYE_TMOS_APERTURE_ENABLE
#define JANUS_EYE_VISION_W                 8
#define JANUS_EYE_VISION_H                 8
#define JANUS_EYE_FRAME_PIXELS             (JANUS_EYE_VISION_W * JANUS_EYE_VISION_H)
#define JANUS_EYE_VISION_DEFAULT_FRAME_MS  160
#define JANUS_EYE_VISION_IDLE_MS           12000UL
#define JANUS_EYE_EVENT_FRAME_MS           900UL

// v2.9 Eagle Focus: software aperture/AGC for the single-zone STHS34PF80.
// It does not fake distance; it makes weak far-field deltas visible without
// letting the room baseline drift into the target.
#define JANUS_EYE_EAGLE_FOCUS_ENABLE       1
#define JANUS_EYE_FOCUS_MIN_GAIN           2.20f
#define JANUS_EYE_FOCUS_MAX_GAIN           7.50f
#define JANUS_EYE_BASELINE_ALPHA_QUIET     0.0030f
#define JANUS_EYE_BASELINE_ALPHA_HOT       0.00008f
#define JANUS_EYE_NOISE_ALPHA              0.0060f
#define JANUS_EYE_PRESENCE_FLAG_LEVEL      18.0f
#define JANUS_EYE_MOTION_FLAG_LEVEL        14.0f

// v2.9I TRUTH RELEASE: only signal/core fixes. I2C/TMOS init stays exactly v2.9.
// Purpose: do not see ghosts when the room is empty after stale saved baseline.
#define JANUS_EYE_RECALIBRATE_ON_BOOT      1
#define JANUS_EYE_STUCK_RAW_ABS            16000
#define JANUS_EYE_MEMORY_DECAY             0.925f
#define JANUS_EYE_MEMORY_ATTACK            0.110f
#define JANUS_EYE_GHOST_DECAY              0.820f  // retained name for packet/state compatibility; now means sensor artifact
#define JANUS_EYE_STALE_RELEASE_MS         1800UL
#define JANUS_EYE_NOW_HOLD_MS              900UL
#define JANUS_EYE_COOL_PRESENCE_WEIGHT     0.32f
#define JANUS_EYE_FLAG_PRESENCE_NOW        0x40
#define JANUS_EYE_FLAG_MOTION_NOW          0x80
#define JANUS_EYE_SENSOR_STALE_MS          1600UL
#define JANUS_EYE_ARTIFACT_ATTACK          0.120f
#define JANUS_EYE_ARTIFACT_DECAY           0.860f
#define JANUS_EYE_CLEAR_ATTACK             0.100f
#define JANUS_EYE_CLEAR_DECAY              0.920f
#define JANUS_EYE_RESIDUAL_MEMORY_LEVEL    0.55f
#define JANUS_EYE_HW_FLAG_ASSIST_LEVEL     0.45f


// v2.10G JANUS BLACKBOARD + Episodic Eye Memory + SwarmSense + Atomic Motion Base scaffold.
// Atomic Motion Base v1.2 uses I2C @0x38 for 4 servos + 2 DC motors; INA226 power
// monitor is probed separately. All actuator writes are OFF by default.
// To physically move the future TMOS/PIR pan head, set JANUS_MOTION_BASE_WRITE_ENABLE to 1
// and explicitly arm through Core policy or local flag.
#define JANUS_EVENT_BUS_ENABLE              1
#define JANUS_EVENT_TX_BASE_MS              3500UL
#define JANUS_EVENT_TX_ALERT_MS             750UL
#define JANUS_EVENT_MOTION_COOLDOWN_MS      650UL
#define JANUS_EVENT_POLICY_TTL_GUARD_MS     15000UL

// v2.10G: Eye semantic memory / task need / SwarmSense mirror.
#define JANUS_EYE_EPISODE_ENABLE            1
#define JANUS_EYE_EPISODE_COUNT             16
#define JANUS_EYE_EPISODE_RECORD_MS         3000UL
#define JANUS_EYE_AI_MEMORY_TX_MS           30000UL
#define JANUS_EYE_TASK_NEED_MS              20000UL
#define JANUS_EYE_TASK_DONE_MS              12000UL
#define JANUS_EYE_SWARMSENSE_ENABLE         1
#define JANUS_EYE_SWARMSENSE_TX_MS          5000UL
#define JANUS_EYE_SWARMSENSE_ALERT_MS       1600UL
#define JANUS_EYE_RECALIBRATE_GHOST_LEVEL   0.72f
#define JANUS_EYE_RECALIBRATE_BAD_FRAMES    8
#define JANUS_EYE_QUIET_STRESS_LEVEL        1.15f

// v2.12 RuView-lite RF Fusion.
// Stable first stage: no CSI yet, only WiFi RSSI drift + ESP-NOW RX RSSI pressure.
// It gives BlindEye a camera-free "radio skin" that can be fused with TMOS/mic/IMU.
#define JANUS_RF_LITE_ENABLE                 1
#define JANUS_RF_LITE_SAMPLE_MS              120UL
#define JANUS_RF_LITE_BASELINE_ALPHA_QUIET   0.0060f
#define JANUS_RF_LITE_BASELINE_ALPHA_HOT     0.0007f
#define JANUS_RF_LITE_NOISE_ALPHA            0.0250f
#define JANUS_RF_LITE_MOTION_LEVEL_DB        3.2f
#define JANUS_RF_LITE_PRESENCE_LEVEL         0.42f
#define JANUS_RF_LITE_ANOMALY_LEVEL          1.18f
#define JANUS_RF_LITE_PACKET_TTL_MS          4500UL

// v2.12 safety/debug layer.
#define JANUS_EYE_VERSION_LABEL              "v2.15A_TMOS_PRIMARY_NO_CAMERA"
#define JANUS_RF_LITE_DEBUG_MS               2500UL
#define JANUS_TMOS_WARMUP_MS                 90000UL
#define JANUS_POLICY_SMOOTH_MIN_DWELL_MS     2500UL
#define JANUS_POLICY_ALERT_CONFIRM           2
#define JANUS_POLICY_RECOVER_CONFIRM         3
#define JANUS_GHOST_TASKNEED_LEVEL           0.96f
#define JANUS_GHOST_TASKNEED_HOLD_MS         45000UL
#define JANUS_GHOST_TASKNEED_COOLDOWN_MS     60000UL

// v2.12B: soft TMOS settling. During the first warmup window the baseline is
// allowed to glide toward the real room temperature field, while output is damped.
// This removes the wild several-thousand-count deltas after a bad boot angle.
#define JANUS_TMOS_WARMUP_SETTLE_ALPHA       0.0450f
#define JANUS_TMOS_WARMUP_SOFT_ALPHA         0.0180f
#define JANUS_TMOS_WARMUP_NOISE_ALPHA        0.0220f
#define JANUS_TMOS_WARMUP_OUTPUT_SCALE       0.18f
#define JANUS_TMOS_WARMUP_GAIN_MAX           4.20f
#define JANUS_TMOS_BASELINE_JUMP_LEVEL       850.0f
#define JANUS_TMOS_POSTWARM_JUMP_ALPHA       0.0012f


// Grove TMOS pins must be visible before any I2C helper function.
// Arduino IDE auto-prototypes functions, so keep these near the top.
#ifndef GROVE_SDA_PIN
#define GROVE_SDA_PIN          2
#endif
#ifndef GROVE_SCL_PIN
#define GROVE_SCL_PIN          1
#endif

// v2.12 Buzz lottery miner scheduler-only imports from RBLGANUL V31.
// This does NOT change block/header wire bytes. It only changes nonce walk order.
#define JANUS_MINER_V31_SCHEDULER_ENABLE     1
#define JANUS_MINER_V31_SECTORS              8


#define JANUS_MOTION_BASE_ENABLE            1
#define JANUS_MOTION_BASE_WRITE_ENABLE      1   // ROBOZOMBIE TEST: physical servo writes ON
#define JANUS_MOTION_BASE_I2C_ADDR          0x38
#define JANUS_MOTION_BASE_INA226_ADDR_A     0x40
#define JANUS_MOTION_BASE_INA226_ADDR_B     0x41
// Atomic Motion Base is NOT on the external TMOS Grove bus on most ATOM builds.
// Official M5Atomic-Motion library defaults to SDA=25, SCL=21 for the base MCU.
#define JANUS_MOTION_BASE_SDA_PIN           38     // ATOM S3 / S3R Atomic Motion Base I2C SDA
#define JANUS_MOTION_BASE_SCL_PIN           39     // ATOM S3 / S3R Atomic Motion Base I2C SCL
#define JANUS_MOTION_BASE_TICK_MS           80UL
#define JANUS_MOTION_BASE_POWER_MS          1000UL
#define JANUS_MOTION_BASE_STATUS_MS         2500UL
#define JANUS_MOTION_BASE_ABSENT_STATUS_MS  10000UL  // v2.14C: when Motion Base is absent, report calmly and stay sensor-only
#define JANUS_MOTION_BASE_OPTIONAL          1        // v2.14C: BlindEye must work normally without the base
#define JANUS_EYE_POWER_TX_MS                2500UL
#define JANUS_MOTION_BASE_TRACK_SERVO_CH    0   // register ch0 = physical Servo1
#define JANUS_MOTION_BASE_TRACK_MIN_DEG     20
#define JANUS_MOTION_BASE_TRACK_MAX_DEG     160
#define JANUS_MOTION_BASE_TRACK_CENTER_DEG  90
#define JANUS_MOTION_BASE_MAX_STEP_DEG      3
#define JANUS_MOTION_BASE_LOW_MV            3400
#define JANUS_MOTION_BASE_SLEEP_MV          3200
#define JANUS_MOTION_BASE_EXT_MV            4350   // USB/boost/charger rail, not raw cell voltage
#define JANUS_MOTION_BASE_FULL_MV           4170
#define JANUS_MOTION_BASE_CHG_CURRENT_MIN   2      // raw INA226 current threshold, advisory only

