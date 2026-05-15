/*
  JANUS_BUZZ_v10_11N3D_SAFE_CHARGE_WIFI_GUARD_FINAL.ino


  v10.11N3D SAFE-CHARGE WIFI-GUARD:
    - Adds thermal safe-charge guard for always-on charger operation.
    - Stops miner/audio/LED load on chip/optional port overheat and resumes after cooldown.
    - Adds Wi-Fi reconnect storm backoff/cooldown so hash does not remain stuck at zero after AP handshakes fail.
    - Adds SwarmSense NAS HTTP circuit breaker and ESP-NOW peer-channel throttle.

  v10.11N3C AUDIO-SAFE FARM MEMORY + SD SELF-CLEAN:
    - Adds persistent farm memory on SD: total hashes, accepts, submits, rejects, best bits survive reboot.
    - Saves /janus/state/farm_state.csv periodically and after share/reject/submit activity.
    - Loads farm memory during boot before miner task starts.
    - Expands telemetry retention for the 16GB SD card while preserving self-clean behavior.
    - Keeps v10.11N3B audio-first SwarmSense safeguards.

  v10.11N3B AUDIO-SAFE SWARMSENSE:
    - Keeps N3A SwarmSense observe bridge, but makes Buzz audio-first again.
    - SwarmSense NAS POST is throttled hard and skipped while Audio.h stream is playing/starting.
    - HTTP timeout for observe POST is reduced so a slow NAS cannot freeze UI/audio.
    - ESP-NOW RX drain is reduced during audio critical windows.
    - Web/SD maintenance is skipped during stream startup/playback to protect FPS and I2S.

  v10.11N3B SWARMSENSE COMPILE FIX:
    - Adds SwarmSensePacket v1 intake from ESP-NOW workers (magic S/S).
    - Keeps observe-first policy: no automatic batch/device commands from SwarmSense.
    - Maintains a local Buzz SwarmSense table for node sensing.
    - Forwards one queued SwarmSense JSON sample at a time to NAS /api/swarm/sense.

  v10.11N3B COMPILE FIX:
    - Avoids Arduino .ino auto-prototype failures by removing SwarmSense custom types from function signatures.
    - Does not touch Audio.h/I2S pinout guard, first-play random, Stratum, music or SD retention.

  ARCHITECTURE (JANUS 113.8):
    - CORE 1 (Face & LED): 25 FPS UI, Dark Red EQ Bonfire, Audio I2S.
    - CORE 0 (Vault): TRUE Stratum Miner (Merkle Root generation, exact Byte Order, Double SHA256).
  
  V40.7/v10.11I TACHYON FINAL COLONY + SHARE SNAPSHOT FIXES:
    - Adds Core2 ESP-NOW remote control: play/pause, prev/next, volume.
    - Allows pool low-difficulty target instead of forcing diff-1 tickets.
    - Correctly handles fractional difficulty (<1) by multiplying diff1 target.
    - Keeps a small safety floor (16 leading zero bits) to avoid ultra-weak spam.
    - Verifies every ESP-NOW remote share on Buzz before relaying to Stratum.
    - Keeps proven Audio.h MP3 path after PCM silence on this board.
    - Full old ES8311 init and per-start PA recovery to survive battery brownout/sleep.
    - Keeps v36.8 beach/horizon UI, track bar, camera-safe pin map.
    - Miner keeps fixed share byte order and ESP-NOW colony master broadcast/relay.

  v10.11M STACKSAFE SD RETENTION PATCH:
    - Fixes loopTask stack canary reboots by increasing Arduino loopTask stack.
    - Moves large colony node/job snapshots off the loop stack into static storage.
    - Keeps SD 5GB retention policy, ESP-NOW protocol, workers and miner logic unchanged.

  v10.11K LOCKED FINAL COLONY PATCH:
    - ESP-NOW receive callback is now lightweight: control/peer/agent/SD packets are copied to a bounded RX queue and processed from colonyTick().
    - Remote share packets keep an immediate fast job snapshot in the callback, so valid shares are not lost across job rotations.
    - Remote share queue reduced to 6 full-snapshot items; RX queue is bounded separately.
    - colonyClearMasterJob has stronger retry logic and a pending-clear fallback so stale jobs do not survive transient mutex contention.
    - Legacy broadcast JobPacket discovery remains disabled by default to protect unique unicast nonce ranges.

  v10.11J STABLE COLONY FINAL PATCH:
    - Remote share queue reduced to 8 items to lower heap pressure with full job snapshots.
    - colonyClearMasterJob now retries and waits longer before leaving stale master work alive.
    - Periodic legacy JobPacket discovery is available via JANUS_COLONY_PERIODIC_JOB_DISCOVERY but disabled by default to avoid duplicate broadcast ranges.
    - Worker discovery still uses safe heartbeat/status/echo broadcasts without overwriting active unicast ranges.

  v10.11I FINAL COLONY COMPILEFIX PATCH:
    - Discovery broadcast no longer overwrites registered workers' unicast nonce ranges.
    - Remote share job-id check and submit snapshot are taken under one colony job mutex.
    - Remote share verification uses the target snapshot from the exact job workers hashed.
    - Stratum job id buffers widened for longer pool job ids.
    - Agent top node/prediction error are read through a critical-section snapshot.
    - Compilefix: share snapshot helper avoids RemoteShareItem in its public signature for Arduino .ino prototype generation.

  V36.6 BEACH UI / TRACK BAR / CAMERA-READY CLEAN:
    - HUD numbers enlarged again while keeping K/KK format.
    - TRACK bar replaces unclear MUSYKA bar; it counts down visually.
    - POOL bar now shows current stratum job freshness.
    - Full-width/taller beach-horizon scene with rare dawn mode.
    - Optional camera pin map for OV2640/JQ-V360-style replacement modules.

  V36.7 HUD READABILITY + SCENE FIX:
    - Pause uses Audio.h pauseResume and does not consume stream while paused.
    - EOF/natural silence reliably advances to shuffled next track.
    - UI top mystery bars removed; signed network/music bars moved into header area.
    - HUD counters use compact K/KK format so P cannot overflow.
    - Miner share test compares reversed/display-order hash against big-endian target.

  V34 COLONY MASTER LAYER:
    - Buzz remains audio-first master speaker.
    - Adds ESP-NOW JobPacket / ShareResponse / EntropyReport protocol.
    - Broadcasts current pool work to voluntary workers.
    - Accepts remote shares into a queue and relays them to public-pool.
    - Collects worker entropy/RSSI and ACK bits for future slime/chaos tuning.
    - LittleFS still not used.

  V33 AUDIO AUTOPILOT + WORKER FIXES:
    - No LittleFS. Audio-first recovery after brownout/amp sleep.
    - Manual pause never triggers recovery.
    - EOF/stall auto-advances tracks; recovery restarts same stream first.
    - Auto-next can random-skip 1..7 tracks to keep playlist order fresh.

  V25 TRUE HASH FIXES:
    - SPIDER STATE FIX: Resolved the "stuck in a corner drawing a square" bug by properly 
      overwriting spMode and spOldState during Tarzan/Zip-line exits.
    - TRUE STRATUM PROTOCOL: Replaced esp_random() headers with cryptographically exact 
      Merkle Root calculations and proper little/big endian byte swapping for valid BTC shares.
    - SESSION PERSISTENCE: Valid shares will now be accepted by public-pool.io.

  V20 POSTAPOC/MOON + TOBI SUIT FIXES:
    - Tobi suit red/black corrected: removed white belt stripe, added web chest/limb accents.
    - Scene backdrop is clipped inside the UI frame, right-side spill removed.
    - Background separated from fire with darker skyline, visible smoke column and large moon behind stalker.
    - Difficulty and palette values moved down near the marquee/ground line.

  V21 WEATHER + TOBI SUIT + CAMP FPS FIXES:
    - Dynamic background weather: clouds/rain/snow/clear/stars/red moon, music-reactive switching.
    - Tobi is Spider-Man-like again: red/black suit, web pattern, no white belt.
    - Tobi swings more often between screen sides and shoots web to different screen points.
    - Camp scene runs smoother; deterministic background removes flicker.
    - PAL/0 moved above SLOZHN, closer to the camp marquee.

  V19 POSTAPOC CAMPFIRE HUD FIXES:
    - Full-width centered post-apoc camp scene: knight left, fire/sword center, stalker right.
    - Difficulty/BEST label lifted above EQ so bottom equalizer no longer overlaps it.
    - Pixel skyline/ruins background, smoke, firelight, guitar strings and glowing stalker mask.

  V18 STALKER CAMPFIRE HUD FIXES:
    - Added stalker guitarist to the right of the bonfire.
    - Centered knight/fire/stalker scene and lifted it to make room for taller EQ.
    - Stalker nods and strums while music is playing.
    - P counter now follows the active palette color.

  V17 EQDOWN HUD FIXES:
    - Equalizer moved to the bottom strip, so it no longer covers the bonfire/knight.
    - Miner difficulty, shares, rejects and ESP-NOW P counter are shown in a small marquee above EQ.
    - P counter added like TD_SWARM/Swarm HUD.

  V14 TOBI CLEAN WEB FIXES:
    - Full clean redraw before Tobi overlay to eliminate permanent web trails/artifacts.
    - Web is drawn only on the current frame, after UI, with no destructive black erasing.
    - Swing anchors rotate across the screen and attach to alternating hands, not two fixed ropes.
    - Camera web shot and zip-line now originate from Tobi hands.
*/


// ===================== JANUS v10.6 CANONICAL MINER PATCH =====================
// Core2-compatible mining header handling.
// - reverse version / prevhash / merkle
// - DO NOT reverse ntime / nbits
// - nonce stays LE
// ============================================================================
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <FS.h>
#include <SD_MMC.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include "Audio.h"  // Proven MP3 Audio.h path for this Waveshare/ESP32-S3 audio board
#include <mbedtls/sha256.h>
#include <ArduinoJson.h>
#if JANUS_SAFE_LITTLEFS_ENABLE
#include <LittleFS.h>
#endif
#include <esp_now.h>
#include <esp_wifi.h>
#define JANUS_CAMERA_ENABLE 0
#if JANUS_CAMERA_ENABLE
#include <esp_camera.h>
#endif

// ---------- CORE 1 LOOP STACK GUARD ----------
// v10.11M: Buzz loopTask runs UI + web + audio service + colony RX drain + SD retention.
// With 5 workers and SD telemetry, the default Arduino loop stack can trip the
// stack canary on ESP32-S3. Override it before setup() is launched by the core.
#ifndef JANUS_LOOP_TASK_STACK_BYTES
#define JANUS_LOOP_TASK_STACK_BYTES (24 * 1024)
#endif
#if defined(ARDUINO_ARCH_ESP32)
size_t getArduinoLoopTaskStackSize(void) {
  return JANUS_LOOP_TASK_STACK_BYTES;
}
#endif

// ---------- НАСТРОЙКИ СЕТИ И АУДИО ----------
const char* WIFI_SSID = "YOUR_WIFI";
const char* WIFI_PASS = "YOUR_PASSWORD";

const char* STREAM_URL  = "http://192.168.1.92:8095/api/music/stream";
// Raw PCM endpoint kept on NAS, but v37 uses proven MP3 Audio.h stream.
// const char* STREAM_PCM_URL = "http://192.168.1.92:8095/api/music/stream.pcm";
const char* CURRENT_URL = "http://192.168.1.92:8095/api/music/current";
const char* NEXT_URL    = "http://192.168.1.92:8095/api/music/next";
const char* PREV_URL    = "http://192.168.1.92:8095/api/music/prev";

// ---------- НАСТРОЙКИ MINER (VAULT) ----------
const char* POOL_HOST = "pool.nerdminers.org";
const uint16_t POOL_PORT = 3333;   // Public Pool current TCP Stratum port; TLS 4333 needs WiFiClientSecure and is not used here

// Public-Pool / NerdMiner ticket gate.
// v40.2: do NOT force diff-1 tickets. Buzz must obey the pool low-difficulty target,
// otherwise an ESP32-S3 can run for many hours/days without a single accepted share.
// Keep only a light local floor to avoid absurdly weak spam if the pool sends a broken diff.
#define JANUS_NERDMINER_MERKLE_MIN_SHARE_BITS 16
#define JANUS_FORCE_DIFF1_TICKETS 0
const char* BTC_WALLET = "1F1Y6CdkApZboDF6g1DYrQ8Dke2E5gWiP1";
char BTC_WORKER[32] = "BuzzLighter";
char MINER_USER[96] = "";

#define LED_COLOR_ORDER NEO_GRB
#define LED_CHANNEL_FIX 1 
#define LED_PALETTE_COUNT 9
#define LED_PROC_PALETTE_INDEX 8
#define JANUS_SAFE_LITTLEFS_ENABLE 0 

// ---------- ПИНЫ ----------
#define SDA_PIN      11
#define SCL_PIN      10
#define TCA_ADDR     0x20
#define ES8311_ADDR  0x18

#define EXIO_LCD_RST    0
#define EXIO_AUDIO_PA   8
#define EXIO_KEY1       9
#define EXIO_KEY2       10
#define EXIO_KEY3       11

#define LCD_CS      3
#define LCD_SCK     4
#define LCD_BL      5
#define LCD_DC      7
#define LCD_MISO    8
#define LCD_MOSI    9

#define I2S_MCLK    12
#define I2S_BCLK    13
#define I2S_LRCK    14
#define I2S_DOUT    16  // default from old working Audio.h Buzz; Serial o can toggle/store 15

#define RGB_PIN     38
#define LED_NUM     7

#define BATTERY_ADC_PIN -1  // v10.11N3D: disabled; GPIO1 is not a valid analog battery channel on this build

// ---------- SD / TF VAULT ----------
// Waveshare ESP32-S3-AUDIO-Board TF slot: CLK=40, CMD=42, D0=41.
// Use SD_MMC 1-bit mode, so EXIO3 / CS is not required for normal operation.
#define JANUS_SD_ENABLE       1
#define JANUS_SD_CLK          40
#define JANUS_SD_CMD          42
#define JANUS_SD_D0           41
#define JANUS_SD_MOUNT        "/sdcard"
#define JANUS_SD_ROOT         "/janus"

// SD retention policy: keep the AI/training telemetry bounded.
// Current CSV logs are rotated into /janus/archive; oldest archives are deleted
// when /janus grows beyond JANUS_SD_RETENTION_LIMIT_BYTES.
#define JANUS_SD_RETENTION_ENABLE          1
#define JANUS_SD_RETENTION_LIMIT_BYTES     (12ULL * 1024ULL * 1024ULL * 1024ULL)
#define JANUS_SD_RETENTION_CHECK_MS        300000UL
#define JANUS_SD_LOG_FILE_MAX_BYTES        (8UL * 1024UL * 1024UL)
#define JANUS_SD_RETENTION_MAX_DELETE_PASS 96

// ---------- FARM MEMORY / AI SD CACHE ----------
// Persistent mining memory. Keeps the farm story alive across Buzz reboots.
// The file is tiny and rewritten atomically-ish; heavy time-series remains in /janus/logs/*.csv.
#define JANUS_FARM_STATE_ENABLE          1
#define JANUS_FARM_STATE_SAVE_MS         15000UL
#define JANUS_FARM_STATE_FILE            JANUS_SD_ROOT "/state/farm_state.csv"

// ---------- OPTIONAL CAMERA (Waveshare ESP32-S3 Audio Board) ----------
// JQ-V360-M12 V3.0 is usually a lens/module name; enable only if the sensor is OV2640-compatible
// and connected to the board CAMERA FPC. Disabled by default so audio/miner stay stable.
#define CAM_PIN_D0      2
#define CAM_PIN_D1      17
#define CAM_PIN_D2      18
#define CAM_PIN_D3      39
#define CAM_PIN_D4      45
#define CAM_PIN_D5      46
#define CAM_PIN_D6      47
#define CAM_PIN_D7      48
#define CAM_PIN_PCLK    44
#define CAM_PIN_VSYNC   21
#define CAM_PIN_HREF    1
#define CAM_PIN_XCLK    43
#define CAM_PIN_SIOD    11
#define CAM_PIN_SIOC    10
#define CAM_PIN_RESET   -1
#define CAM_PIN_PWDN    -1


// ---------- COLORS RGB565 ----------
#define C_BLACK      0x0000
#define C_AMBER      0xFD20
#define C_AMBER2     0xFBE0
#define C_DIM        0x7A80
#define C_DARK       0x20C0
#define C_RED        0xD000
#define C_GREEN      0x05A0
#define C_WHITE      0xFFFF
#define C_BLUE_DIM   0x0190
#define C_SPIDEY_BLUE 0x025F
#define C_ORANGE     0xFB20
#define C_DEEP_RED   0x8000
#define C_CRIMSON    0xA800
#define C_GREY       0x8410
#define C_LOW        0x3186
#define C_PURPLE     0x781F
#define C_CYAN       0x07FF

// ---------- ГЛОБАЛЬНЫЕ ОБЪЕКТЫ ----------
SPIClass lcdSpi(FSPI);
Adafruit_NeoPixel strip(LED_NUM, RGB_PIN, LED_COLOR_ORDER + NEO_KHZ800);
Preferences prefs;

uint16_t tcaConfig = 0xFFFF;
uint16_t tcaOutput = 0xFFFF;

bool playing = false;
bool wanted = false;
bool softPaused = false;
uint32_t streamStartedAt = 0;

// V33 AUDIO AUTOPILOT
// Manual pause must be silent: no recovery, no auto-next.
// When playing, EOF -> next track. Early stall -> same-stream recovery first.
volatile uint32_t audioCriticalUntilMs = 0;
uint32_t audioLastRunningMs = 0;
uint32_t audioLastRecoveryMs = 0;
uint32_t audioLastAutoNextMs = 0;
uint8_t audioRecoveryAttempts = 0;
bool audioEofFlag = false;
bool audioBusySwitching = false;
bool audioUserPaused = false;
bool audioHadStableRun = false;
uint32_t audioStableSinceMs = 0;
uint32_t audioSoftPauseAtMs = 0;
uint8_t audioShuffleHopsMax = 9;
// v10.11N2: NAS /stream can start from its default pointer if boot-time /next happens too early.
// Keep a one-shot random jump pending and apply it immediately before the first real stream connect.
bool janusFirstPlayRandomPending = true;
bool janusFirstPlayRandomDone = false;
uint32_t janusBootEntropySalt = 0;

// Audio player. Restored proven Audio.h MP3 path; raw PCM path is intentionally disabled.
Audio audio;
// v10.11N1: Audio.h on Arduino ESP32 core 3.x allocates the I2S channel inside setPinout().
// Calling setPinout() again while the Audio task/channel is still alive causes
// "i2s_new_channel: no available channel found" and the stream goes silent.
// Keep pinout setup one-shot; fresh starts/recovery only reconnect the HTTP stream.
bool janusAudioPinsConfigured = false;
uint8_t janusAudioPinsDoutConfigured = 0;

uint8_t ledPalette = 0;
uint8_t volumeVal = 14;
uint8_t audioDoutPin = I2S_DOUT;  // default from old working Buzz; Serial 'o' toggles 16/15
uint8_t ledBright = 55;

uint32_t lastLedMs = 0, lastFastUiMs = 0, lastTrackMs = 0;
uint32_t lastTobiMs = 0;
uint32_t ledFrame = 0;

// v10.6 share flare: set by miner task, rendered safely by LED task.
// kind: 1=candidate/share ticket, 2=accepted share.
volatile uint32_t ledShareFlareUntilMs = 0;
volatile uint8_t ledShareFlareKind = 0;
volatile uint32_t ledShareFlareSeq = 0;

void janusShareLedFlare(uint8_t kind) {
  uint32_t now = millis();
  ledShareFlareKind = kind;
  ledShareFlareSeq++;
  ledShareFlareUntilMs = now + ((kind >= 2) ? 1800UL : 1150UL);
}

// V16: split heavy UI from Tobi overlay.
// Heavy raw-LCD widgets stay ~20 FPS; Tobi alone runs 60 FPS.
// For experimental 120 FPS set TOBI_TURBO_MS to 8, but 60 FPS is safer with Wi-Fi+Audio+Miner.
constexpr uint16_t TOBI_TURBO_MS = 33;  // v37.1 smooth UI: 30 FPS overlay, no 100 FPS flicker/tearing
constexpr uint16_t HEAVY_UI_MS = 50;  // v37.1 smooth UI: 20 FPS heavy scene, less LCD blink

// ---------- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ HUD/POPUPS ----------
uint32_t popupTimer = 0;
char popupText[16] = "";
uint16_t popupColor = C_WHITE;
bool popupIsVol = true;
bool popupShowBar = true; 

// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ПАУКА
int spOldX = 0, spOldY = 0;
int spOldFrame = -1;
uint8_t spOldState = 0; 
// v23: restored classic flexible Spider-Man/Tobi from stable Buzz builds.
uint8_t spOldMode = 0;
bool spOldShot = false;
uint8_t spiderMode = 0;
uint32_t spiderModeSince = 0;
uint32_t spiderShotUntil = 0;
float spT = 0;
uint32_t tarzanEndTime = 0;
uint32_t zipStartTime = 0;
int zipDir = 1;
int spOldAnchorX = 0;
uint32_t tobiNextActionMs = 0;
uint32_t tobiWebBurstUntil = 0;
// Tobi dirty-web tracker: erase exact previous web lines, not a permanent black trail.
constexpr uint8_t TOBI_MAX_WEB_LINES = 16;
int spWebX0[TOBI_MAX_WEB_LINES] = {}, spWebY0[TOBI_MAX_WEB_LINES] = {}, spWebX1[TOBI_MAX_WEB_LINES] = {}, spWebY1[TOBI_MAX_WEB_LINES] = {};
uint8_t spWebCount = 0;
int spNextWebX0[TOBI_MAX_WEB_LINES] = {}, spNextWebY0[TOBI_MAX_WEB_LINES] = {}, spNextWebX1[TOBI_MAX_WEB_LINES] = {}, spNextWebY1[TOBI_MAX_WEB_LINES] = {};
uint8_t spNextWebCount = 0;

// V27: persistent web hit-points. Lines disappear, but small white attachment dots stay for a while.
constexpr uint8_t TOBI_MAX_WEB_DOTS = 28;
int spDotX[TOBI_MAX_WEB_DOTS] = {}, spDotY[TOBI_MAX_WEB_DOTS] = {};
uint32_t spDotBorn[TOBI_MAX_WEB_DOTS] = {};
uint8_t spDotHead = 0;

uint32_t uiOldHash = 0;
uint32_t uiOldShare = 0;
uint32_t uiOldDiff = 1;
uint8_t uiOldPal = 0;
uint32_t uiOldColony = 0;
uint32_t uiOldRejects = 0;

// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ РЫЦАРЯ
uint8_t knightFrame = 0;
uint8_t knightState = 0; 
uint32_t knightAnimStart = 0;
uint32_t knightNextAction = 5000;

float spectrum[16] = {0};
int specOldH[16] = {0};

// V21 dynamic camp weather. Kept tiny and deterministic: no big buffers, no image assets.
enum WeatherMode : uint8_t { WX_CLEAR=0, WX_CLOUDS, WX_RAIN, WX_SNOW, WX_STARS, WX_RED_MOON, WX_DAWN };
WeatherMode campWeather = WX_STARS;
uint32_t weatherNextChangeMs = 0;
uint32_t weatherSeed = 0x1138C0DE;
uint32_t weatherMinHoldUntilMs = 0;
uint8_t weatherChainStep = 0;
uint32_t lastSceneFrameMs = 0;

float bonfireHeat[LED_NUM] = {0.1f, 0.2f, 0.3f, 0.5f, 0.3f, 0.2f, 0.1f};

uint32_t procPaletteSeed = 0;
float procHueA = 0.0f;
float procHueB = 0.0f;
float procSpeed = 0.025f;
float procContrast = 0.75f;

char trackName[96] = "unknown";
uint32_t trackVisualStartedAtMs = 0;
uint32_t trackVisualPausedAtMs = 0;
uint32_t trackVisualPausedTotalMs = 0;
uint32_t trackDurationMs = 180000UL;   // fallback if /current does not expose duration
uint32_t lastTrackBarDrawMs = 0;
int bitrateKbps = 128;

struct Btn { bool now=false, prev=false, pressed=false, released=false, longFired=false; uint32_t downAt=0; };
Btn key1, key2, key3;

volatile uint32_t minerRealHashrate = 0;
volatile uint32_t minerShares = 0;
volatile uint32_t minerCurrentDiff = 1;
volatile bool stratumConnected = false;
TaskHandle_t minerTaskHandle = NULL;

// v10.11H: no SELF/local fake mining fallback.
// Buzz only counts real pool/worker hashes tied to live Stratum jobs.
volatile uint32_t minerLocalHashrate = 0;
volatile uint32_t minerBestBits = 0;
volatile uint64_t minerTotalHashes = 0;
volatile bool minerLocalFallback = false;
volatile uint32_t minerSubmitAttempts = 0;
volatile uint32_t minerSubmitRejects = 0;
volatile uint32_t minerShareCandidates = 0;
volatile uint32_t minerLastCandidateMs = 0;
volatile uint16_t minerLastCandidateBits = 0;
volatile uint32_t minerRemoteSubmitAttempts = 0; // subset of minerSubmitAttempts, not extra
volatile uint32_t minerStatsRows = 0;
uint32_t minerLastStatsCsvMs = 0;
volatile uint32_t minerStaleJobRejects = 0;
volatile uint32_t minerLowDiffRejects = 0;
volatile uint32_t minerOtherRejects = 0;
char minerLastRejectReason[64] = "-";
char minerLastSubmitNonce[9] = "--------";
volatile uint32_t minerLastJobMs = 0;
volatile uint32_t minerLastSubmitMs = 0;
volatile uint32_t minerLastAcceptMs = 0;
volatile uint32_t minerWifiReconnects = 0;
volatile float minerCurrentDiffF = 1.0f;
volatile float minerPoolSuggestedDiffF = 1.0f;
volatile uint16_t minerShareTargetBits = 32;
char minerStatus[20] = "BOOT";
uint8_t minerShareTarget[32];
uint32_t connectedAtMs = 0;
uint32_t lastWifiKickMs = 0;
uint32_t poolReconnectHoldUntilMs = 0;
uint8_t poolReconnectFails = 0;

// v10.11N3D safe-charge / Wi-Fi storm state.
volatile bool janusThermalStop = false;
volatile float janusChipTempC = 0.0f;
volatile int janusPortTempRaw = -1;
uint32_t janusThermalLastMs = 0;
uint32_t janusThermalLastLogMs = 0;
uint8_t janusThermalSavedBright = 55;
bool janusThermalDimmed = false;
uint32_t janusWifiNextKickMs = 0;
uint32_t janusWifiStormSinceMs = 0;
uint8_t janusWifiStormCount = 0;
uint8_t janusWifiFailLevel = 0;
bool janusColonyEspNowActive = false;
uint32_t colonySwarmSenseHoldUntilMs = 0;
uint8_t colonySwarmSenseFailStreak = 0;

// Persistent farm memory state. Loaded before MinerTask starts; saved from loop/accept/reject paths.
bool janusFarmLoaded = false;
bool janusFarmDirty = false;
uint32_t janusFarmLastSaveMs = 0;
uint32_t janusFarmSaveRows = 0;
uint32_t janusFarmBootShares = 0;
uint64_t janusFarmBootHashes = 0;
uint32_t janusFarmLastSavedShares = 0;
uint64_t janusFarmLastSavedHashes = 0;

String extranonce1 = "";
int extranonce2_size = 0;
uint32_t extranonce2 = 0;

void initJanusWorkerName() {
  uint64_t mac = ESP.getEfuseMac();
  uint32_t chip = (uint32_t)(mac & 0xFFFFFF);
  snprintf(BTC_WORKER, sizeof(BTC_WORKER), "BuzzLighter_%06lX", (unsigned long)chip);
  snprintf(MINER_USER, sizeof(MINER_USER), "%s.%s", BTC_WALLET, BTC_WORKER);
}

String minerUserString() {
  if (MINER_USER[0] == '\0') initJanusWorkerName();
  return String(MINER_USER);
}

// ============================================================
// SD VAULT / CACHE OFFLOAD
// ============================================================
bool janusSdReady = false;
uint64_t janusSdTotalBytes = 0;
uint64_t janusSdUsedBytes = 0;
SemaphoreHandle_t janusSdMutex = nullptr;
uint32_t janusSdLastStatusMs = 0;
WebServer janusWeb(8088);
bool janusWebReady = false;
uint32_t janusSdLastRetentionMs = 0;
uint32_t janusSdRetentionRotations = 0;
uint32_t janusSdRetentionDeletes = 0;
uint64_t janusSdJanusBytes = 0;

bool janusSdLock(uint32_t waitMs = 15) {
  if (!janusSdMutex) return true;
  return xSemaphoreTake(janusSdMutex, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}
void janusSdUnlock() {
  if (janusSdMutex) xSemaphoreGive(janusSdMutex);
}

void janusSdMkdir(const char* path) {
#if JANUS_SD_ENABLE
  if (!janusSdReady || !path) return;
  if (!SD_MMC.exists(path)) SD_MMC.mkdir(path);
#endif
}

bool janusSdEndsWith(const char* s, const char* suffix) {
  if (!s || !suffix) return false;
  size_t ls = strlen(s), lf = strlen(suffix);
  if (lf > ls) return false;
  return strcmp(s + ls - lf, suffix) == 0;
}

const char* janusSdHeaderForLogPath(const char* path) {
#if JANUS_SD_ENABLE
  if (!path) return nullptr;
  if (strcmp(path, JANUS_SD_ROOT "/logs/miner_stats.csv") == 0) {
    return "ms,mode,pool,hash_total,hash_pool,hash_local,total_hashes,best_bits,target_bits,accepted,submit_total,submit_local,submit_remote,ok_pct,rejects,low,stale,other,workers_online,workers_known,agent_rewards,agent_share_rewards,agent_top,pred_err,rx_packets,queue_now,remote_queued,remote_drop,weak_drop,dup_drop,unicast_jobs,discovery_jobs,peer_best,diff,batch,status,error";
  }
  if (strcmp(path, JANUS_SD_ROOT "/logs/colony_nodes.csv") == 0) {
    return "ms,node,role,mac,hashrate,shares,rejects,bestBits,targetBits,rssi,uptime,aiHint,agentScore,predHash,predErr,rewardPoints,rewards,rewardLevel";
  }
  if (strcmp(path, JANUS_SD_ROOT "/logs/agent_rewards.csv") == 0) {
    return "ms,node,mac,rewardLevel,aiHint,rewardPoints,targetBatch,entropySeed,score,predHash,predErr,deltaShares";
  }
  if (strcmp(path, JANUS_SD_ROOT "/logs/agent_predictions.csv") == 0) {
    return "ms,node,hashrate,shares,bestBits,emaHash,predHash,predBest,predErr,rewardPoints,rewards,aiHint";
  }
  if (strcmp(path, JANUS_SD_ROOT "/logs/buzz.csv") == 0) {
    return "ms,tag,message";
  }
#endif
  return nullptr;
}

void janusSdEnsureLogHeaderUnlocked(const char* path) {
#if JANUS_SD_ENABLE
  if (!janusSdReady || !path) return;
  if (SD_MMC.exists(path)) return;
  const char* header = janusSdHeaderForLogPath(path);
  File f = SD_MMC.open(path, FILE_WRITE);
  if (f) {
    if (header && header[0]) f.println(header);
    f.close();
  }
#endif
}

uint64_t janusSdDirBytesUnlocked(const char* path, uint8_t depth) {
#if JANUS_SD_ENABLE
  if (!janusSdReady || !path || depth > 8) return 0;
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) return 0;
  if (!f.isDirectory()) {
    uint64_t sz = (uint64_t)f.size();
    f.close();
    return sz;
  }

  uint64_t total = 0;
  File e = f.openNextFile();
  while (e) {
    if (e.isDirectory()) {
      String child = String(e.name());
      if (!child.startsWith("/")) child = String(path) + "/" + child;
      e.close();
      total += janusSdDirBytesUnlocked(child.c_str(), depth + 1);
    } else {
      total += (uint64_t)e.size();
      e.close();
    }
    e = f.openNextFile();
  }
  f.close();
  return total;
#else
  return 0;
#endif
}

uint64_t janusSdRetentionBytesUnlocked() {
#if JANUS_SD_ENABLE
  // Retention applies only to AI/training telemetry, not to /janus/audio or persistent settings.
  return janusSdDirBytesUnlocked(JANUS_SD_ROOT "/logs", 0) +
         janusSdDirBytesUnlocked(JANUS_SD_ROOT "/archive", 0) +
         janusSdDirBytesUnlocked(JANUS_SD_ROOT "/cache", 0);
#else
  return 0;
#endif
}

uint32_t janusSdArchiveSeqFromName(const char* name) {
  if (!name) return 0;
  const char* slash = strrchr(name, '/');
  const char* base = slash ? slash + 1 : name;
  const char* dot = strrchr(base, '.');
  const char* us = strrchr(base, '_');
  if (!dot || !us || us >= dot) return 0;
  char tmp[12];
  size_t n = (size_t)(dot - us - 1);
  if (n == 0 || n >= sizeof(tmp)) return 0;
  memcpy(tmp, us + 1, n);
  tmp[n] = 0;
  for (size_t i = 0; i < n; ++i) if (tmp[i] < '0' || tmp[i] > '9') return 0;
  return (uint32_t)strtoul(tmp, nullptr, 10);
}

uint32_t janusSdNextArchiveSeqUnlocked() {
#if JANUS_SD_ENABLE
  uint32_t maxSeq = 0;
  File dir = SD_MMC.open(JANUS_SD_ROOT "/archive", FILE_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return 1;
  }
  File e = dir.openNextFile();
  while (e) {
    if (!e.isDirectory()) {
      String name = String(e.name());
      uint32_t seq = janusSdArchiveSeqFromName(name.c_str());
      if (seq > maxSeq) maxSeq = seq;
    }
    e.close();
    e = dir.openNextFile();
  }
  dir.close();
  return maxSeq + 1;
#else
  return 1;
#endif
}

bool janusSdFindOldestArchiveUnlocked(char* outPath, size_t outPathSize, uint64_t* outSize) {
#if JANUS_SD_ENABLE
  if (!outPath || outPathSize == 0) return false;
  outPath[0] = 0;
  uint32_t bestSeq = 0xFFFFFFFFUL;
  uint64_t bestSize = 0;
  bool found = false;

  File dir = SD_MMC.open(JANUS_SD_ROOT "/archive", FILE_READ);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return false;
  }

  File e = dir.openNextFile();
  while (e) {
    if (!e.isDirectory()) {
      String name = String(e.name());
      if (name.endsWith(".csv")) {
        uint32_t seq = janusSdArchiveSeqFromName(name.c_str());
        if (seq > 0 && seq < bestSeq) {
          String full = name.startsWith("/") ? name : String(JANUS_SD_ROOT "/archive/") + name;
          strlcpy(outPath, full.c_str(), outPathSize);
          bestSeq = seq;
          bestSize = (uint64_t)e.size();
          found = true;
        }
      }
    }
    e.close();
    e = dir.openNextFile();
  }
  dir.close();
  if (outSize) *outSize = bestSize;
  return found;
#else
  return false;
#endif
}

void janusSdRotateLogIfNeededUnlocked(const char* path) {
#if JANUS_SD_ENABLE
#if JANUS_SD_RETENTION_ENABLE
  if (!janusSdReady || !path || !janusSdEndsWith(path, ".csv")) return;
  if (strncmp(path, JANUS_SD_ROOT "/logs/", strlen(JANUS_SD_ROOT "/logs/")) != 0) return;
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    janusSdEnsureLogHeaderUnlocked(path);
    return;
  }
  uint64_t sz = (uint64_t)f.size();
  f.close();
  if (sz < JANUS_SD_LOG_FILE_MAX_BYTES) return;

  janusSdMkdir(JANUS_SD_ROOT "/archive");
  const char* slash = strrchr(path, '/');
  const char* base = slash ? slash + 1 : path;
  char stem[40];
  strlcpy(stem, base, sizeof(stem));
  char* dot = strrchr(stem, '.');
  if (dot) *dot = 0;

  uint32_t seq = janusSdNextArchiveSeqUnlocked();
  char dst[128];
  snprintf(dst, sizeof(dst), JANUS_SD_ROOT "/archive/%s_%08lu.csv", stem, (unsigned long)seq);
  if (SD_MMC.rename(path, dst)) {
    janusSdRetentionRotations++;
    Serial.printf("[SD] rotated %s -> %s size=%lluKB\n", path, dst, (unsigned long long)(sz / 1024ULL));
  } else {
    Serial.printf("[SD] rotate failed for %s size=%lluKB; keeping current log\n", path, (unsigned long long)(sz / 1024ULL));
    return;
  }
  janusSdEnsureLogHeaderUnlocked(path);
#endif
#endif
}

void janusSdRotateCurrentLogsUnlocked() {
#if JANUS_SD_ENABLE
#if JANUS_SD_RETENTION_ENABLE
  janusSdRotateLogIfNeededUnlocked(JANUS_SD_ROOT "/logs/buzz.csv");
  janusSdRotateLogIfNeededUnlocked(JANUS_SD_ROOT "/logs/miner_stats.csv");
  janusSdRotateLogIfNeededUnlocked(JANUS_SD_ROOT "/logs/colony_nodes.csv");
  janusSdRotateLogIfNeededUnlocked(JANUS_SD_ROOT "/logs/agent_rewards.csv");
  janusSdRotateLogIfNeededUnlocked(JANUS_SD_ROOT "/logs/agent_predictions.csv");
#endif
#endif
}

void janusSdPruneRetentionUnlocked(bool forcePrint) {
#if JANUS_SD_ENABLE
#if JANUS_SD_RETENTION_ENABLE
  if (!janusSdReady) return;
  janusSdRotateCurrentLogsUnlocked();
  janusSdJanusBytes = janusSdRetentionBytesUnlocked();
  if (janusSdJanusBytes <= JANUS_SD_RETENTION_LIMIT_BYTES) {
    if (forcePrint) {
      Serial.printf("[SD] retention ok telemetry=%lluMB limit=%lluMB archiveDeletes=%lu rotations=%lu\n",
                    (unsigned long long)(janusSdJanusBytes / 1048576ULL),
                    (unsigned long long)(JANUS_SD_RETENTION_LIMIT_BYTES / 1048576ULL),
                    (unsigned long)janusSdRetentionDeletes,
                    (unsigned long)janusSdRetentionRotations);
    }
    return;
  }

  uint8_t pass = 0;
  while (janusSdJanusBytes > JANUS_SD_RETENTION_LIMIT_BYTES && pass < JANUS_SD_RETENTION_MAX_DELETE_PASS) {
    char victim[128];
    uint64_t victimSize = 0;
    if (!janusSdFindOldestArchiveUnlocked(victim, sizeof(victim), &victimSize)) break;
    if (SD_MMC.remove(victim)) {
      janusSdRetentionDeletes++;
      if (janusSdJanusBytes > victimSize) janusSdJanusBytes -= victimSize;
      else janusSdJanusBytes = janusSdRetentionBytesUnlocked();
      Serial.printf("[SD] retention deleted old archive %s size=%lluKB telemetry=%lluMB/%lluMB\n",
                    victim,
                    (unsigned long long)(victimSize / 1024ULL),
                    (unsigned long long)(janusSdJanusBytes / 1048576ULL),
                    (unsigned long long)(JANUS_SD_RETENTION_LIMIT_BYTES / 1048576ULL));
    } else {
      Serial.printf("[SD] retention delete failed: %s\n", victim);
      break;
    }
    pass++;
  }

  if (janusSdJanusBytes > JANUS_SD_RETENTION_LIMIT_BYTES) {
    Serial.printf("[SD] retention warning: archive exhausted, telemetry=%lluMB limit=%lluMB. Current live telemetry files are preserved.\n",
                  (unsigned long long)(janusSdJanusBytes / 1048576ULL),
                  (unsigned long long)(JANUS_SD_RETENTION_LIMIT_BYTES / 1048576ULL));
  }
#endif
#endif
}

void janusSdRetentionTick(bool force = false) {
#if JANUS_SD_ENABLE
#if JANUS_SD_RETENTION_ENABLE
  if (!janusSdReady) return;
  uint32_t now = millis();
  if (!force && now - janusSdLastRetentionMs < JANUS_SD_RETENTION_CHECK_MS) return;
  janusSdLastRetentionMs = now;
  if (!janusSdLock(force ? 250 : 30)) return;
  janusSdPruneRetentionUnlocked(force);
  janusSdUnlock();
#endif
#endif
}

void janusSdAppendLine(const char* path, const char* line) {
#if JANUS_SD_ENABLE
  if (!janusSdReady || !path || !line) return;
  if (!janusSdLock(20)) return;
  janusSdRotateLogIfNeededUnlocked(path);
  janusSdEnsureLogHeaderUnlocked(path);
  File f = SD_MMC.open(path, FILE_APPEND);
  if (f) {
    f.println(line);
    f.close();
  }
  janusSdUnlock();
#endif
}

void janusSdListSerial(const char* path, uint8_t depth = 0) {
#if JANUS_SD_ENABLE
  if (!janusSdReady) { Serial.println("[SD] not mounted"); return; }
  if (!path || !path[0]) path = JANUS_SD_ROOT;
  if (!janusSdLock(60)) { Serial.println("[SD] busy"); return; }

  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) {
    Serial.printf("[SD] not a directory: %s\n", path);
    if (dir) dir.close();
    janusSdUnlock();
    return;
  }

  Serial.printf("[SD] LIST %s\n", path);
  File f = dir.openNextFile();
  while (f) {
    for (uint8_t i = 0; i < depth; i++) Serial.print("  ");
    Serial.printf("%s %s %llu\n", f.isDirectory() ? "[D]" : "[F]", f.name(), (unsigned long long)f.size());
    if (f.isDirectory() && depth < 2) {
      // Keep serial output bounded: show one nested layer.
    }
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  janusSdUnlock();
#endif
}

void janusSdPrintFileSerial(const char* path, size_t maxBytes = 8192) {
#if JANUS_SD_ENABLE
  if (!janusSdReady) { Serial.println("[SD] not mounted"); return; }
  if (!path || !path[0]) return;
  if (!janusSdLock(80)) { Serial.println("[SD] busy"); return; }

  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    Serial.printf("[SD] cannot open: %s\n", path);
    janusSdUnlock();
    return;
  }
  Serial.printf("[SD] CAT %s size=%llu max=%u\n", path, (unsigned long long)f.size(), (unsigned)maxBytes);
  size_t n = 0;
  while (f.available() && n < maxBytes) {
    Serial.write((uint8_t)f.read());
    n++;
  }
  Serial.println();
  f.close();
  janusSdUnlock();
#endif
}

void janusSdPrintTailSerial(const char* path, size_t tailBytes = 8192) {
#if JANUS_SD_ENABLE
  if (!janusSdReady) { Serial.println("[SD] not mounted"); return; }
  if (!janusSdLock(80)) { Serial.println("[SD] busy"); return; }

  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    Serial.printf("[SD] cannot open: %s\n", path);
    janusSdUnlock();
    return;
  }

  size_t sz = f.size();
  size_t start = (sz > tailBytes) ? (sz - tailBytes) : 0;
  f.seek(start);
  Serial.printf("[SD] TAIL %s size=%u from=%u\n", path, (unsigned)sz, (unsigned)start);
  while (f.available()) Serial.write((uint8_t)f.read());
  Serial.println();
  f.close();
  janusSdUnlock();
#endif
}

String janusHtmlEscape(const String& s) {
  String o;
  o.reserve(s.length() + 16);
  for (size_t i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '<') o += F("&lt;");
    else if (c == '>') o += F("&gt;");
    else if (c == '&') o += F("&amp;");
    else if (c == '"') o += F("&quot;");
    else o += c;
  }
  return o;
}

String janusPathArg() {
  String p = janusWeb.hasArg("path") ? janusWeb.arg("path") : String(JANUS_SD_ROOT);
  if (!p.startsWith("/")) p = "/" + p;
  if (p.indexOf("..") >= 0) p = JANUS_SD_ROOT;
  return p;
}

void janusWebRoot() {
  String ip = WiFi.localIP().toString();
  String h;
  h.reserve(1800);
  h += F("<html><head><meta charset='utf-8'><title>JANUS BUZZ SD</title>");
  h += F("<style>body{font-family:monospace;background:#08070a;color:#f0c070}a{color:#8ff}pre{white-space:pre-wrap}</style></head><body>");
  h += F("<h2>JANUS BUZZ SD VAULT</h2>");
  h += F("<p>IP: "); h += ip; h += F(" port 8088</p>");
  h += F("<ul>");
  h += F("<li><a href='/sd/list?path=/janus'>List /janus</a></li>");
  h += F("<li><a href='/sd/tail?path=/janus/logs/buzz.csv'>Tail buzz.csv</a></li>");
  h += F("<li><a href='/sd/cat?path=/janus/state/buzz.cfg'>Read buzz.cfg</a></li>");
  h += F("</ul>");
  h += F("</body></html>");
  janusWeb.send(200, "text/html; charset=utf-8", h);
}

void janusWebSdList() {
#if JANUS_SD_ENABLE
  if (!janusSdReady) { janusWeb.send(503, "text/plain", "SD not mounted"); return; }
  String path = janusPathArg();
  if (!janusSdLock(80)) { janusWeb.send(503, "text/plain", "SD busy"); return; }

  File dir = SD_MMC.open(path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    janusSdUnlock();
    janusWeb.send(404, "text/plain", "not a directory");
    return;
  }

  String h;
  h.reserve(4096);
  h += F("<html><head><meta charset='utf-8'><title>SD LIST</title>");
  h += F("<style>body{font-family:monospace;background:#08070a;color:#f0c070}a{color:#8ff}</style></head><body><pre>");
  h += "LIST " + janusHtmlEscape(path) + "\n\n";
  if (path != "/") h += "<a href='/sd/list?path=/janus'>/janus</a>\n\n";

  File f = dir.openNextFile();
  while (f) {
    String name = String(f.name());
    String full = name.startsWith("/") ? name : (path + "/" + name);
    h += f.isDirectory() ? "[D] " : "[F] ";
    h += "<a href='";
    h += f.isDirectory() ? "/sd/list?path=" : "/sd/cat?path=";
    h += janusHtmlEscape(full);
    h += "'>";
    h += janusHtmlEscape(name);
    h += "</a>";
    h += " ";
    h += String((unsigned long)f.size());
    h += "\n";
    f.close();
    f = dir.openNextFile();
  }
  dir.close();
  janusSdUnlock();
  h += F("</pre></body></html>");
  janusWeb.send(200, "text/html; charset=utf-8", h);
#endif
}

void janusWebSdCat(bool tailMode) {
#if JANUS_SD_ENABLE
  if (!janusSdReady) { janusWeb.send(503, "text/plain", "SD not mounted"); return; }
  String path = janusPathArg();
  if (!janusSdLock(120)) { janusWeb.send(503, "text/plain", "SD busy"); return; }

  File f = SD_MMC.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    janusSdUnlock();
    janusWeb.send(404, "text/plain", "file not found");
    return;
  }

  size_t maxBytes = tailMode ? 16384 : 65536;
  size_t sz = f.size();
  if (tailMode && sz > maxBytes) f.seek(sz - maxBytes);

  String body;
  body.reserve(min((size_t)70000, maxBytes + 512));
  body += F("<html><head><meta charset='utf-8'><title>SD FILE</title>");
  body += F("<style>body{font-family:monospace;background:#08070a;color:#f0c070}a{color:#8ff}pre{white-space:pre-wrap}</style></head><body>");
  body += F("<a href='/sd/list?path=/janus'>back</a><pre>");
  body += janusHtmlEscape(path);
  body += F("\nsize=");
  body += String((unsigned long)sz);
  body += tailMode ? F(" tail\n\n") : F("\n\n");

  size_t n = 0;
  while (f.available() && n < maxBytes) {
    char c = (char)f.read();
    if (c == '<') body += F("&lt;");
    else if (c == '>') body += F("&gt;");
    else if (c == '&') body += F("&amp;");
    else body += c;
    n++;
  }
  f.close();
  janusSdUnlock();
  body += F("</pre></body></html>");
  janusWeb.send(200, "text/html; charset=utf-8", body);
#endif
}

void janusWebBegin() {
  if (janusWebReady) return;
  janusWeb.on("/", janusWebRoot);
  janusWeb.on("/sd/list", janusWebSdList);
  janusWeb.on("/sd/cat", [](){ janusWebSdCat(false); });
  janusWeb.on("/sd/tail", [](){ janusWebSdCat(true); });
  janusWeb.begin();
  janusWebReady = true;
  Serial.printf("[WEB] SD browser: http://%s:8088/\n", WiFi.localIP().toString().c_str());
  janusSdLogf("WEB", "started ip=%s port=8088", WiFi.localIP().toString().c_str());
}

void janusWebTick() {
  if (janusWebReady) janusWeb.handleClient();
}

void janusSdLogf(const char* tag, const char* fmt, ...) {
#if JANUS_SD_ENABLE
  if (!janusSdReady) return;
  char msg[192];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(msg, sizeof(msg), fmt, ap);
  va_end(ap);

  char line[256];
  snprintf(line, sizeof(line), "%lu,%s,%s", (unsigned long)millis(), tag ? tag : "-", msg);
  janusSdAppendLine(JANUS_SD_ROOT "/logs/buzz.csv", line);
#endif
}

void janusSdWriteConfig() {
#if JANUS_SD_ENABLE
  if (!janusSdReady) return;
  if (!janusSdLock(40)) return;
  File f = SD_MMC.open(JANUS_SD_ROOT "/state/buzz.cfg", FILE_WRITE);
  if (f) {
    f.printf("palette=%u\n", ledPalette);
    f.printf("volume=%u\n", volumeVal);
    f.printf("brightness=%u\n", ledBright);
    f.printf("dout=%u\n", audioDoutPin);
    f.printf("worker=%s\n", MINER_USER);
    f.close();
  }
  janusSdUnlock();
#endif
}

bool janusParseCfgLine(const char* line, const char* key, int* out) {
  if (!line || !key || !out) return false;
  size_t n = strlen(key);
  if (strncmp(line, key, n) != 0 || line[n] != '=') return false;
  *out = atoi(line + n + 1);
  return true;
}

void janusSdLoadConfig() {
#if JANUS_SD_ENABLE
  if (!janusSdReady) return;
  if (!janusSdLock(40)) return;
  File f = SD_MMC.open(JANUS_SD_ROOT "/state/buzz.cfg", FILE_READ);
  if (f) {
    char line[96];
    while (f.available()) {
      size_t len = f.readBytesUntil('\n', line, sizeof(line) - 1);
      line[len] = 0;
      int v = 0;
      if (janusParseCfgLine(line, "palette", &v)) ledPalette = constrain(v, 0, LED_PALETTE_COUNT - 1);
      else if (janusParseCfgLine(line, "volume", &v)) volumeVal = constrain(v, 0, 21);
      else if (janusParseCfgLine(line, "brightness", &v)) ledBright = constrain(v, 0, 100);
      else if (janusParseCfgLine(line, "dout", &v)) audioDoutPin = (v == 15 || v == 16) ? v : I2S_DOUT;
    }
    f.close();
    Serial.println("[SD] loaded /janus/state/buzz.cfg");
  }
  janusSdUnlock();
#endif
}



void janusFarmMarkDirty() {
#if JANUS_FARM_STATE_ENABLE
  janusFarmDirty = true;
#endif
}

uint64_t janusParseU64Token(char** ctx) {
  char* t = strtok_r(nullptr, ",", ctx);
  if (!t) return 0;
  return strtoull(t, nullptr, 10);
}

bool janusFarmLoadMinerStatsFallbackUnlocked() {
#if JANUS_SD_ENABLE
  File mf = SD_MMC.open(JANUS_SD_ROOT "/logs/miner_stats.csv", FILE_READ);
  if (!mf) return false;
  char line[448] = {0};
  char last[448] = {0};
  while (mf.available()) {
    size_t len = mf.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = 0;
    if (len > 10 && strncmp(line, "ms,", 3) != 0) strlcpy(last, line, sizeof(last));
  }
  mf.close();
  if (!last[0]) return false;

  char* ctx = nullptr;
  char* tok = strtok_r(last, ",", &ctx);
  uint8_t col = 0;
  uint64_t total = 0;
  uint32_t best = 0, acc = 0, sub = 0, remote = 0, rej = 0, low = 0, stale = 0, other = 0;
  while (tok) {
    switch (col) {
      case 6:  total = strtoull(tok, nullptr, 10); break;
      case 7:  best = strtoul(tok, nullptr, 10); break;
      case 9:  acc = strtoul(tok, nullptr, 10); break;
      case 10: sub = strtoul(tok, nullptr, 10); break;
      case 12: remote = strtoul(tok, nullptr, 10); break;
      case 14: rej = strtoul(tok, nullptr, 10); break;
      case 15: low = strtoul(tok, nullptr, 10); break;
      case 16: stale = strtoul(tok, nullptr, 10); break;
      case 17: other = strtoul(tok, nullptr, 10); break;
    }
    tok = strtok_r(nullptr, ",", &ctx);
    col++;
  }
  if (total == 0 && acc == 0 && sub == 0 && best == 0) return false;

  minerTotalHashes = total;
  minerShares = acc;
  minerSubmitAttempts = sub;
  minerRemoteSubmitAttempts = remote;
  minerSubmitRejects = rej;
  minerLowDiffRejects = low;
  minerStaleJobRejects = stale;
  minerOtherRejects = other;
  if (best > minerBestBits) minerBestBits = best;

  janusFarmBootHashes = total;
  janusFarmBootShares = acc;
  janusFarmLastSavedHashes = total;
  janusFarmLastSavedShares = acc;
  janusFarmLoaded = true;
  janusFarmDirty = true; // create compact farm_state.csv on the next safe save.
  Serial.printf("[FARM] restored from miner_stats.csv total=%llu acc=%lu sub=%lu remote=%lu rej=%lu best=%lu\n",
                (unsigned long long)minerTotalHashes,
                (unsigned long)minerShares,
                (unsigned long)minerSubmitAttempts,
                (unsigned long)minerRemoteSubmitAttempts,
                (unsigned long)minerSubmitRejects,
                (unsigned long)minerBestBits);
  return true;
#else
  return false;
#endif
}

void janusFarmPrintState() {
#if JANUS_FARM_STATE_ENABLE
  Serial.printf("[FARM] loaded=%d saves=%lu total=%llu bootTotal=%llu acc=%lu bootAcc=%lu sub=%lu remoteSub=%lu cand=%lu rej=%lu low=%lu stale=%lu other=%lu best=%lu file=%s\n",
                janusFarmLoaded ? 1 : 0,
                (unsigned long)janusFarmSaveRows,
                (unsigned long long)minerTotalHashes,
                (unsigned long long)janusFarmBootHashes,
                (unsigned long)minerShares,
                (unsigned long)janusFarmBootShares,
                (unsigned long)minerSubmitAttempts,
                (unsigned long)minerRemoteSubmitAttempts,
                (unsigned long)minerShareCandidates,
                (unsigned long)minerSubmitRejects,
                (unsigned long)minerLowDiffRejects,
                (unsigned long)minerStaleJobRejects,
                (unsigned long)minerOtherRejects,
                (unsigned long)minerBestBits,
                JANUS_FARM_STATE_FILE);
#endif
}

bool janusFarmLoadState() {
#if JANUS_FARM_STATE_ENABLE
#if JANUS_SD_ENABLE
  if (!janusSdReady) return false;
  if (!janusSdLock(80)) return false;
  File f = SD_MMC.open(JANUS_FARM_STATE_FILE, FILE_READ);
  if (!f) {
    bool recovered = janusFarmLoadMinerStatsFallbackUnlocked();
    janusSdUnlock();
    if (!recovered) Serial.println("[FARM] no previous farm_state.csv; starting fresh memory");
    return recovered;
  }

  char line[256] = {0};
  char last[256] = {0};
  while (f.available()) {
    size_t len = f.readBytesUntil('\n', line, sizeof(line) - 1);
    line[len] = 0;
    if (len > 5 && strncmp(line, "version", 7) != 0) strlcpy(last, line, sizeof(last));
  }
  f.close();
  janusSdUnlock();

  if (!last[0]) return false;
  char* ctx = nullptr;
  char* first = strtok_r(last, ",", &ctx);
  if (!first) return false;
  uint32_t version = strtoul(first, nullptr, 10);
  if (version < 1) return false;

  uint64_t savedTotal = janusParseU64Token(&ctx);
  uint32_t savedShares = (uint32_t)janusParseU64Token(&ctx);
  uint32_t savedSubmits = (uint32_t)janusParseU64Token(&ctx);
  uint32_t savedRemote = (uint32_t)janusParseU64Token(&ctx);
  uint32_t savedCand = (uint32_t)janusParseU64Token(&ctx);
  uint32_t savedRejects = (uint32_t)janusParseU64Token(&ctx);
  uint32_t savedLow = (uint32_t)janusParseU64Token(&ctx);
  uint32_t savedStale = (uint32_t)janusParseU64Token(&ctx);
  uint32_t savedOther = (uint32_t)janusParseU64Token(&ctx);
  uint32_t savedBest = (uint32_t)janusParseU64Token(&ctx);

  minerTotalHashes = savedTotal;
  minerShares = savedShares;
  minerSubmitAttempts = savedSubmits;
  minerRemoteSubmitAttempts = savedRemote;
  minerShareCandidates = savedCand;
  minerSubmitRejects = savedRejects;
  minerLowDiffRejects = savedLow;
  minerStaleJobRejects = savedStale;
  minerOtherRejects = savedOther;
  if (savedBest > minerBestBits) minerBestBits = savedBest;

  janusFarmBootHashes = savedTotal;
  janusFarmBootShares = savedShares;
  janusFarmLastSavedHashes = savedTotal;
  janusFarmLastSavedShares = savedShares;
  janusFarmLoaded = true;
  janusFarmDirty = false;
  Serial.printf("[FARM] restored SD memory total=%llu acc=%lu sub=%lu remote=%lu rej=%lu best=%lu\n",
                (unsigned long long)minerTotalHashes,
                (unsigned long)minerShares,
                (unsigned long)minerSubmitAttempts,
                (unsigned long)minerRemoteSubmitAttempts,
                (unsigned long)minerSubmitRejects,
                (unsigned long)minerBestBits);
  return true;
#endif
#endif
  return false;
}

void janusFarmSaveState(bool force) {
#if JANUS_FARM_STATE_ENABLE
#if JANUS_SD_ENABLE
  if (!janusSdReady) return;
  uint32_t now = millis();
  if (!force) {
    if (now - janusFarmLastSaveMs < JANUS_FARM_STATE_SAVE_MS) return;
    if (!janusFarmDirty && minerTotalHashes == janusFarmLastSavedHashes && minerShares == janusFarmLastSavedShares) return;
  }
  janusFarmLastSaveMs = now;

  uint64_t totalSnap = minerTotalHashes;
  uint32_t sharesSnap = minerShares;
  uint32_t submitSnap = minerSubmitAttempts;
  uint32_t remoteSnap = minerRemoteSubmitAttempts;
  uint32_t candSnap = minerShareCandidates;
  uint32_t rejectSnap = minerSubmitRejects;
  uint32_t lowSnap = minerLowDiffRejects;
  uint32_t staleSnap = minerStaleJobRejects;
  uint32_t otherSnap = minerOtherRejects;
  uint32_t bestSnap = minerBestBits;

  if (!janusSdLock(force ? 160 : 25)) return;
  File f = SD_MMC.open(JANUS_FARM_STATE_FILE, FILE_WRITE);
  if (f) {
    f.println("version,total_hashes,accepted,submits,remote_submits,candidates,rejects,low,stale,other,best_bits,updated_ms,last_nonce");
    f.printf("1,%llu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%s\n",
             (unsigned long long)totalSnap,
             (unsigned long)sharesSnap,
             (unsigned long)submitSnap,
             (unsigned long)remoteSnap,
             (unsigned long)candSnap,
             (unsigned long)rejectSnap,
             (unsigned long)lowSnap,
             (unsigned long)staleSnap,
             (unsigned long)otherSnap,
             (unsigned long)bestSnap,
             (unsigned long)now,
             minerLastSubmitNonce);
    f.close();
    janusFarmSaveRows++;
    janusFarmDirty = false;
    janusFarmLastSavedHashes = totalSnap;
    janusFarmLastSavedShares = sharesSnap;
  }
  janusSdUnlock();
#endif
#endif
}
void janusSdWriteBootReadme() {
#if JANUS_SD_ENABLE
  if (!janusSdReady) return;
  if (!janusSdLock(40)) return;
  if (!SD_MMC.exists(JANUS_SD_ROOT "/README.txt")) {
    File f = SD_MMC.open(JANUS_SD_ROOT "/README.txt", FILE_WRITE);
    if (f) {
      f.println("JANUS BUZZ SD VAULT");
      f.println("/janus/logs/buzz.csv          runtime events, submits, accepts/rejects");
      f.println("/janus/logs/miner_stats.csv       structured real pool/colony miner time-series");
      f.println("/janus/logs/colony_nodes.csv      universal worker telemetry + Agent scores");
      f.println("/janus/logs/agent_rewards.csv     reward events sent to workers; AOK is share/ticket rewards");
      f.println("/janus/logs/agent_predictions.csv prediction/training stream from ESP-NOW metrics");
      f.println("/janus/archive/              rotated telemetry CSV; oldest archives auto-delete at 5GB telemetry cap");
      f.println("/janus/state/buzz.cfg             user settings mirror/cache");
      f.println("/janus/state/farm_state.csv       persistent farm memory: hashes/shares/submits/best");
      f.println("/janus/cache/                 future heavy cache area");
      f.println("/janus/audio/                 optional local audio files, never removed by telemetry retention");
      f.println("Buzz v10.11L keeps real pool/colony telemetry bounded by SD retention; SELF fallback is disabled.");
      f.close();
    }
  }
  if (!SD_MMC.exists(JANUS_SD_ROOT "/logs/miner_stats.csv")) {
    File mf = SD_MMC.open(JANUS_SD_ROOT "/logs/miner_stats.csv", FILE_WRITE);
    if (mf) {
      mf.println("ms,mode,pool,hash_total,hash_pool,hash_local,total_hashes,best_bits,target_bits,accepted,submit_total,submit_local,submit_remote,ok_pct,rejects,low,stale,other,workers_online,workers_known,agent_rewards,agent_share_rewards,agent_top,pred_err,rx_packets,queue_now,remote_queued,remote_drop,weak_drop,dup_drop,unicast_jobs,discovery_jobs,peer_best,diff,batch,status,error");
      mf.close();
    }
  }
  if (!SD_MMC.exists(JANUS_SD_ROOT "/logs/colony_nodes.csv")) {
    File cf = SD_MMC.open(JANUS_SD_ROOT "/logs/colony_nodes.csv", FILE_WRITE);
    if (cf) {
      cf.println("ms,node,role,mac,hashrate,shares,rejects,bestBits,targetBits,rssi,uptime,aiHint,agentScore,predHash,predErr,rewardPoints,rewards,rewardLevel");
      cf.close();
    }
  }
  if (!SD_MMC.exists(JANUS_SD_ROOT "/logs/agent_rewards.csv")) {
    File rf = SD_MMC.open(JANUS_SD_ROOT "/logs/agent_rewards.csv", FILE_WRITE);
    if (rf) {
      rf.println("ms,node,mac,rewardLevel,aiHint,rewardPoints,targetBatch,entropySeed,score,predHash,predErr,deltaShares");
      rf.close();
    }
  }
  if (!SD_MMC.exists(JANUS_SD_ROOT "/logs/agent_predictions.csv")) {
    File pf = SD_MMC.open(JANUS_SD_ROOT "/logs/agent_predictions.csv", FILE_WRITE);
    if (pf) {
      pf.println("ms,node,hashrate,shares,bestBits,emaHash,predHash,predBest,predErr,rewardPoints,rewards,aiHint");
      pf.close();
    }
  }
  janusSdUnlock();
#endif
}

void janusSdPrintStatus() {
#if JANUS_SD_ENABLE
  if (!janusSdReady) {
    Serial.println("[SD] not mounted");
    return;
  }
  janusSdTotalBytes = SD_MMC.totalBytes();
  janusSdUsedBytes = SD_MMC.usedBytes();
  uint64_t telemetryBytes = 0;
  if (janusSdLock(80)) {
    telemetryBytes = janusSdRetentionBytesUnlocked();
    janusSdUnlock();
  } else {
    telemetryBytes = janusSdJanusBytes;
  }
  janusSdJanusBytes = telemetryBytes;
  Serial.printf("[SD] mounted total=%lluMB used=%lluMB free=%lluMB telemetry=%lluMB cap=%lluMB rotations=%lu deletes=%lu root=%s\n",
                (unsigned long long)(janusSdTotalBytes / 1048576ULL),
                (unsigned long long)(janusSdUsedBytes / 1048576ULL),
                (unsigned long long)((janusSdTotalBytes - janusSdUsedBytes) / 1048576ULL),
                (unsigned long long)(telemetryBytes / 1048576ULL),
                (unsigned long long)(JANUS_SD_RETENTION_LIMIT_BYTES / 1048576ULL),
                (unsigned long)janusSdRetentionRotations,
                (unsigned long)janusSdRetentionDeletes,
                JANUS_SD_ROOT);
#endif
}

void janusSdBegin() {
#if JANUS_SD_ENABLE
  if (!janusSdMutex) janusSdMutex = xSemaphoreCreateMutex();

  // Reset card driver in case bootloader/previous firmware touched it.
  SD_MMC.end();
  delay(30);

  SD_MMC.setPins(JANUS_SD_CLK, JANUS_SD_CMD, JANUS_SD_D0);
  janusSdReady = SD_MMC.begin(JANUS_SD_MOUNT, true, false, SDMMC_FREQ_DEFAULT, 5);
  if (!janusSdReady) {
    Serial.println("[SD] mount failed. Format FAT32/exFAT? For best reliability use FAT32 and reinsert.");
    return;
  }

  uint8_t cardType = SD_MMC.cardType();
  if (cardType == CARD_NONE) {
    janusSdReady = false;
    Serial.println("[SD] no card detected");
    return;
  }

  janusSdMkdir(JANUS_SD_ROOT);
  janusSdMkdir(JANUS_SD_ROOT "/logs");
  janusSdMkdir(JANUS_SD_ROOT "/archive");
  janusSdMkdir(JANUS_SD_ROOT "/state");
  janusSdMkdir(JANUS_SD_ROOT "/cache");
  janusSdMkdir(JANUS_SD_ROOT "/audio");

  janusSdTotalBytes = SD_MMC.totalBytes();
  janusSdUsedBytes = SD_MMC.usedBytes();
  Serial.printf("[SD] mounted type=%u total=%lluMB used=%lluMB\n",
                (unsigned)cardType,
                (unsigned long long)(janusSdTotalBytes / 1048576ULL),
                (unsigned long long)(janusSdUsedBytes / 1048576ULL));

  janusSdWriteBootReadme();
  janusSdRetentionTick(true);
  janusSdLogf("BOOT", "Buzz v10.11L SD vault mounted totalMB=%llu usedMB=%llu janusCapMB=%llu logRotateMB=%lu",
              (unsigned long long)(janusSdTotalBytes / 1048576ULL),
              (unsigned long long)(janusSdUsedBytes / 1048576ULL),
              (unsigned long long)(JANUS_SD_RETENTION_LIMIT_BYTES / 1048576ULL),
              (unsigned long)(JANUS_SD_LOG_FILE_MAX_BYTES / 1048576UL));
#endif
}

void janusPersistSettings() {
  if (janusSdReady) {
    janusSdWriteConfig();
  } else {
    prefs.putUChar("palette", ledPalette);
    prefs.putUChar("vol", volumeVal);
    prefs.putUChar("bright", ledBright);
    prefs.putUChar("dout", audioDoutPin);
  }
}


void hexStringToBytes(String hex, uint8_t *bytes) {
  for (int i = 0; i < hex.length(); i += 2) {
    String byteString = hex.substring(i, i + 2);
    bytes[i / 2] = (uint8_t)strtol(byteString.c_str(), NULL, 16);
  }
}

void reverse_bytes(uint8_t *data, int len) {
  for(int i=0; i<len/2; i++) {
    uint8_t t = data[i]; 
    data[i] = data[len-1-i]; 
    data[len-1-i] = t;
  }
}

// Exact NerdMiner prevhash transform: byte-swap inside each 32-bit word,
// not a full 32-byte reverse.
void reverse_word_bytes(uint8_t *data, int len) {
  for (int off = 0; off + 3 < len; off += 4) {
    uint8_t t0 = data[off + 0];
    uint8_t t1 = data[off + 1];
    data[off + 0] = data[off + 3];
    data[off + 1] = data[off + 2];
    data[off + 2] = t1;
    data[off + 3] = t0;
  }
}

// v10.11H: Stratum submit sends extranonce2 as hex string, but coinbase bytes
// must match NerdMiner's little-endian counter layout: 1 -> "01000000...".
// Supports extranonce2_size up to 8 bytes.
void formatExtranonce2LE(uint64_t value, uint8_t sizeBytes, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (sizeBytes == 0) return;
  if (sizeBytes > 8) sizeBytes = 8;
  size_t need = (size_t)sizeBytes * 2 + 1;
  if (outSize < need) return;

  for (uint8_t i = 0; i < sizeBytes; ++i) {
    uint8_t b = (uint8_t)((value >> (8 * i)) & 0xFF);
    snprintf(out + i * 2, outSize - i * 2, "%02x", b);
  }
  out[sizeBytes * 2] = '\0';
}

uint16_t countLeadingZeroBits(const uint8_t h[32]) {
  uint16_t bits = 0;
  for (int i = 0; i < 32; i++) {
    uint8_t b = h[i];
    if (b == 0) { bits += 8; continue; }
    for (int k = 7; k >= 0; k--) {
      if ((b & (1 << k)) == 0) bits++;
      else return bits;
    }
  }
  return bits;
}

// Same math model as NerdMiner utils.cpp: hash is interpreted as little-endian
// 256-bit integer and converted into difficulty = diff1 / hash.
static const double JANUS_TRUE_DIFF_ONE =
  26959535291011309493156476344723991336010898738574164086137773096960.0;

double janusLe256ToDouble(const uint8_t le[32]) {
  uint64_t w0, w1, w2, w3;
  memcpy(&w0, le + 0, 8);
  memcpy(&w1, le + 8, 8);
  memcpy(&w2, le + 16, 8);
  memcpy(&w3, le + 24, 8);
  double d = 0.0;
  d += (double)w3 * 6277101735386680763835789423207666416102355444464034512896.0;
  d += (double)w2 * 340282366920938463463374607431768211456.0;
  d += (double)w1 * 18446744073709551616.0;
  d += (double)w0;
  return d;
}

double janusDiffFromHashLE(const uint8_t hashLE[32]) {
  double v = janusLe256ToDouble(hashLE);
  if (v <= 0.0 || !isfinite(v)) v = 1.0;
  return JANUS_TRUE_DIFF_ONE / v;
}

void writeLE32(uint8_t* p, uint32_t v) {
  p[0] = v & 0xFF;
  p[1] = (v >> 8) & 0xFF;
  p[2] = (v >> 16) & 0xFF;
  p[3] = (v >> 24) & 0xFF;
}

// Bitcoin diff-1 target in big-endian form.
const uint8_t BTC_DIFF1_TARGET[32] = {
  0x00,0x00,0x00,0x00,0xFF,0xFF,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};

void makeTargetWithMinLeadingZeroBits(uint8_t out[32], uint16_t minBits) {
  memset(out, 0xFF, 32);
  if (minBits >= 256) { memset(out, 0, 32); return; }
  uint16_t fullZeroBytes = minBits / 8;
  uint8_t remBits = minBits % 8;
  for (uint16_t i = 0; i < fullZeroBytes && i < 32; ++i) out[i] = 0x00;
  if (fullZeroBytes < 32 && remBits) {
    // Example remBits=4 => byte must be 0000 1111 max.
    out[fullZeroBytes] = (uint8_t)(0xFF >> remBits);
  }
}

void setShareTargetFromDifficulty(float diff) {
  if (diff <= 0.0f || !isfinite(diff)) diff = 1.0f;
  minerPoolSuggestedDiffF = diff;

  float effectiveDiff = diff;
#if JANUS_FORCE_DIFF1_TICKETS
  if (effectiveDiff < 1.0f) effectiveDiff = 1.0f;
#endif
  minerCurrentDiffF = effectiveDiff;
  memset(minerShareTarget, 0, sizeof(minerShareTarget));

  // target = diff1_target / effectiveDiff.
  // IMPORTANT: for fractional difficulty (<1) this is a MULTIPLICATION of the target,
  // not integer division by 1. The old v40.1 code rounded diff=0.00001 to div=1,
  // silently turning every low-diff pool job back into rare diff-1 tickets.
  if (effectiveDiff < 1.0f) {
    double mulD = 1.0 / (double)effectiveDiff;
    if (mulD < 1.0) mulD = 1.0;
    if (mulD > 4294967295.0) mulD = 4294967295.0;
    uint32_t mul = (uint32_t)(mulD + 0.5);
    if (mul < 1) mul = 1;

    uint32_t carry = 0;
    for (int i = 31; i >= 0; --i) {
      uint64_t v = (uint64_t)BTC_DIFF1_TARGET[i] * (uint64_t)mul + carry;
      minerShareTarget[i] = (uint8_t)(v & 0xFF);
      carry = (uint32_t)(v >> 8);
    }
    // Saturate to easiest possible target if multiplication overflows 256 bits.
    if (carry) memset(minerShareTarget, 0xFF, sizeof(minerShareTarget));
  } else {
    uint32_t div = (uint32_t)(effectiveDiff + 0.5f);
    if (div < 1) div = 1;
    uint64_t rem = 0;
    for (int i = 0; i < 32; i++) {
      uint64_t cur = (rem << 8) | BTC_DIFF1_TARGET[i];
      minerShareTarget[i] = (uint8_t)(cur / div);
      rem = cur % div;
    }
  }

  minerShareTargetBits = countLeadingZeroBits(minerShareTarget);

  // Light safety clamp: never broadcast/submit a target easier than the configured bit floor.
  // This is NOT diff-1. With minBits=16, Buzz should find candidates in minutes, not days.
  if (minerShareTargetBits < JANUS_NERDMINER_MERKLE_MIN_SHARE_BITS) {
    makeTargetWithMinLeadingZeroBits(minerShareTarget, JANUS_NERDMINER_MERKLE_MIN_SHARE_BITS);
    minerShareTargetBits = countLeadingZeroBits(minerShareTarget);
    // Approx effective diff for display only: diff1 is 32 bits; lower bit floor means lower diff.
    minerCurrentDiffF = powf(2.0f, (float)JANUS_NERDMINER_MERKLE_MIN_SHARE_BITS - 32.0f);
  }
}

bool hashMeetsTarget(const uint8_t hashBE[32], const uint8_t targetBE[32]) {
  if (!hashBE || !targetBE) return false;
  // Big-endian numeric compare: hashBE <= targetBE.
  for (int i = 0; i < 32; i++) {
    if (hashBE[i] < targetBE[i]) return true;
    if (hashBE[i] > targetBE[i]) return false;
  }
  return true;
}

bool hashMeetsShareTarget(const uint8_t hashBE[32]) {
  return hashMeetsTarget(hashBE, (const uint8_t*)minerShareTarget);
}


// ============================================================
// JANUS COLONY ESP-NOW HEARTBEAT
// ============================================================
#define JANUS_COLONY_ENABLE 1
#define JANUS_COLONY_CHANNEL 1
#define JANUS_COLONY_JOB_MS 1500
#define JANUS_COLONY_ECHO_MS 3500
#define JANUS_COLONY_REMOTE_SHARE_QUEUE 6
#define JANUS_COLONY_RX_QUEUE 8
#define JANUS_COLONY_RX_PACKET_MAX 128
// Keep legacy broadcast JobPacket discovery OFF by default.
// Existing workers accept any broadcast JobPacket and would overwrite their unique unicast nonce range.
// Set to 1 only if you intentionally use passive listen-only workers and accept duplicate-range risk.
#define JANUS_COLONY_PERIODIC_JOB_DISCOVERY 0
#define JANUS_COLONY_DISCOVERY_EVERY_SEQ 4
// v10.11N2: audio-safe pinout guard + first-play random + lightweight live-discovery ping for workers added after Buzz is already running.
// It sends a zero-range JobPacket when unicast workers already exist, so old workers do not
// receive duplicate nonce work, while new workers can learn Buzz MAC/channel and announce themselves.
#define JANUS_COLONY_LIVE_DISCOVERY_PING_MS 7000UL
#define JANUS_COLONY_LIVE_DISCOVERY_ZERO_RANGE 1

// v10.11N3: SwarmSense v1 observe bridge.
// Buzz only observes/forwards. It does NOT apply batch/device commands from SwarmSense.
#define JANUS_SWARMSENSE_ENABLE 1
#define JANUS_SWARMSENSE_NAS_URL "http://192.168.1.92:5000/api/swarm/sense"
#define JANUS_SWARMSENSE_QUEUE 8
#define JANUS_SWARMSENSE_MAX_NODES 24
#define JANUS_SWARMSENSE_POST_MIN_MS 5000UL
#define JANUS_SWARMSENSE_HTTP_TIMEOUT_MS 220
#define JANUS_SWARMSENSE_AUDIO_SAFE 1
#define JANUS_SWARMSENSE_SKIP_WHILE_AUDIO 1
#define JANUS_SWARMSENSE_RESET_QUEUE_ON_AUDIO_START 1

// v10.11N3D: Buzz can stay on charger 24/7. Protect the board from heat,
// Wi-Fi reconnect storms and dead NAS HTTP loops. The built-in ESP32-S3
// temperature sensor measures chip temperature, not the USB-C connector itself.
#define JANUS_SAFE_CHARGE_MODE              1
#define JANUS_THERMAL_WARN_C                66.0f
#define JANUS_THERMAL_CUTOFF_C              74.0f
#define JANUS_THERMAL_RESUME_C              58.0f
#define JANUS_THERMAL_SLEEP_C               82.0f
#define JANUS_THERMAL_CHECK_MS              2000UL
#define JANUS_PORT_NTC_PIN                  -1     // set to an ADC GPIO if a real NTC is glued near USB/charger port
#define JANUS_PORT_NTC_HOT_RAW              3600   // raw ADC threshold for optional NTC input
#define JANUS_CHARGE_CUT_PIN                -1     // set to charger CE/EN or MOSFET GPIO if hardware exists
#define JANUS_CHARGE_CUT_ACTIVE_LEVEL       LOW
#define JANUS_WIFI_RECONNECT_BASE_MS        8000UL
#define JANUS_WIFI_RECONNECT_MAX_MS         120000UL
#define JANUS_WIFI_STORM_WINDOW_MS          180000UL
#define JANUS_WIFI_STORM_LIMIT              8
#define JANUS_WIFI_RADIO_COOLDOWN_MS        60000UL
#define JANUS_SWARMSENSE_FAIL_STREAK_LIMIT  3
#define JANUS_SWARMSENSE_FAIL_COOLDOWN_MS   120000UL
#define JANUS_COLONY_PEER_REFIX_MS          15000UL

// v10.11H: Buzz is a universal dispatcher.
// Registered workers receive unique unicast nonce ranges.
// Broadcast job remains only as discovery/fallback for future unknown nodes.
#define JANUS_COLONY_MAX_NODES 24
// v10.11H: ESP-NOW peer table is smaller than logical node registry.
// Keep broadcast + about 15 unicast peers; older inactive peers are evicted automatically.
#define JANUS_COLONY_MAX_ESPNOW_PEERS 16
#define JANUS_COLONY_NODE_TTL_MS 30000UL
#define JANUS_COLONY_NODE_LOG_MS 10000UL
#define JANUS_COLONY_WORKER_RANGE 262144UL
#define JANUS_COLONY_DISCOVERY_RANGE 4194304UL

// v10.11 Agent layer: reward workers from real ESP-NOW metrics only.
// It never changes pool target validity; it only adjusts future worker batch hints,
// sends entropy/reward packets, and logs prediction data to SD.
#define JANUS_AGENT_ENABLE 1
#define JANUS_AGENT_REWARD_MIN_MS 4500UL   // immediate for worker share/ticket deltas
#define JANUS_AGENT_PRAISE_MIN_MS 60000UL   // slow non-share praise, so AGNT does not run away
#define JANUS_AGENT_LOG_MS 10000UL
#define JANUS_AGENT_SCORE_DECAY 0.72f
#define JANUS_AGENT_EMA_ALPHA 0.18f

struct __attribute__((packed)) JanusColonyPacket {
  char magic[6];
  char nodeId[24];
  char role[12];
  uint32_t seq;
  uint32_t hashRate;
  uint32_t shares;
  uint32_t rejects;
  uint32_t bestBits;
  float diff;
  uint16_t targetBits;
  uint16_t aiBatch;
  uint8_t aiHint;     // 0 observe, 1 stable, 2 slow-down, 3 boost
  uint32_t jobAgeMs;
  int8_t rssi;
  uint32_t uptime;
};

struct __attribute__((packed)) JobPacket {
  uint8_t magic[2];       // 'J','B'
  uint8_t job_id[8];      // compact fingerprint of current stratum job id
  uint8_t header[80];     // block header template, nonce field overwritten per worker
  uint32_t start_nonce;
  uint32_t range_size;
  uint8_t target[32];     // share target, big-endian compare, same as minerShareTarget
  uint32_t extranonce2;
};

struct __attribute__((packed)) ShareResponse {
  uint8_t magic[2];       // 'S','R' legacy: nonce only
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
};

struct __attribute__((packed)) ShareResponseV2 {
  uint8_t magic[2];       // 'S','2' JANUS NerdMiner-v2 ESP-NOW: nonce + worker proof telemetry
  uint8_t job_id[8];
  uint32_t nonce;
  uint16_t worker_id;
  uint16_t bits;
  uint32_t total_hashes_l32;
  uint8_t hash_tail[4];   // last 4 bytes of display-order share hash for debugging duplicate/stale shares
};

struct __attribute__((packed)) EntropyReport {
  uint8_t magic[2];       // 'E','R'
  uint16_t worker_id;
  float local_entropy;
  uint8_t sensor_flags;   // bit0=mic, bit1=tmos, bit2=mag/bps, bit3=game/ai
  float values[4];
};

// v10.11N: optional extended telemetry emitted by Atom Matrix / Pyramid workers.
// Buzz accepts it as a universal swarm status packet and converts it into the
// normal colony node table so new devices are visible without a reboot.
struct __attribute__((packed)) HiveMetricPacket {
  uint8_t magic[2];        // 'H','M'
  uint8_t version;         // 2
  uint16_t worker_id;
  char nodeId[24];
  char kind[16];
  uint32_t seq;
  uint32_t uptime_ms;
  uint32_t free_heap;
  uint32_t min_free_heap;
  uint16_t cpu_mhz;
  uint16_t loop_jitter_us;
  uint16_t loop_max_us;
  int8_t rssi;
  uint8_t bt_flags;
  uint8_t volume;
  uint8_t palette;
  uint16_t touch_count;
  uint16_t effective_batch;
  uint32_t hash_rate;
  uint32_t total_hashes;
  uint32_t shares;
  uint32_t rejects;
  uint16_t best_bits;
  uint32_t job_age_ms;
  uint32_t nonce_remaining;
  uint8_t reward_level;
  uint8_t ai_hint;
  uint16_t target_batch;
  int16_t prediction_error_x1000;
  uint16_t entropy_x1000;
  uint16_t random_tail;
  uint16_t reserved;
};

// v10.11N3: observe-first sensory packet.
// Mirrors the NAS SwarmSensePacket v1 layout; old workers ignore it safely.
struct __attribute__((packed)) SwarmSensePacket {
  uint8_t magic[2];        // 'S','S'
  uint8_t version;         // 1
  uint16_t worker_id;
  char nodeId[24];
  char kind[16];
  uint32_t seq;
  uint32_t uptime_ms;
  uint32_t micros_tail;
  uint32_t free_heap;
  uint16_t loop_jitter_us;
  uint16_t loop_max_us;
  int8_t rssi;
  uint8_t radio_mode;
  uint8_t bt_flags;
  uint8_t palette;
  uint8_t knn_label;
  uint8_t knn_confidence;
  uint8_t ai_hint;
  uint8_t thermal_load;
  uint16_t effective_batch;
  uint16_t dynamic_batch;
  uint32_t hash_rate;
  uint32_t total_hashes;
  uint16_t best_bits;
  uint16_t hash_eff_x1000;
  int16_t prediction_error_x1000;
  uint16_t entropy_x1000;
  uint16_t touch_delta;
  uint16_t job_age_s;
  uint16_t nonce_remaining_l16;
  uint16_t flags;
};

struct SwarmSenseQueueItem {
  SwarmSensePacket pkt;
  uint8_t srcMac[6];
  int8_t rxRssi;
  uint32_t receivedAt;
};

struct SwarmSenseNodeSlot {
  bool used;
  uint8_t mac[6];
  char nodeId[24];
  char kind[16];
  uint32_t firstSeenMs;
  uint32_t lastSeenMs;
  uint32_t samples;
  uint32_t seq;
  int8_t rssi;
  uint8_t radioMode;
  uint8_t knnLabel;
  uint8_t knnConfidence;
  uint8_t aiHint;
  uint8_t thermalLoad;
  uint16_t effectiveBatch;
  uint16_t dynamicBatch;
  uint32_t hashRate;
  uint32_t totalHashes;
  uint16_t bestBits;
  uint16_t hashEffX1000;
  int16_t predictionErrorX1000;
};

struct __attribute__((packed)) EchoProbe {
  uint8_t magic[2];       // 'E','P'
  uint32_t seq;
  uint32_t entropyBits;
};

struct __attribute__((packed)) JanusControlPacket {
  uint8_t magic[2];        // 'J','C'
  uint8_t version;         // 1
  char source[16];         // Core2Home
  char target[16];         // Buzz / BuzzLighter / all
  char command[16];        // play_pause / play / pause / next / prev / vol_up / vol_down / volume_set / status
  int32_t value;
  uint32_t seq;
  uint32_t uptime_ms;
};

struct __attribute__((packed)) JanusBuzzStatusPacket {
  uint8_t magic[2];        // 'B','S'
  uint8_t version;         // 1
  char nodeId[24];
  char track[96];
  uint8_t playing;
  uint8_t paused;
  uint8_t volume;
  uint8_t brightness;
  uint32_t hashRate;
  uint32_t shares;
  uint32_t rejects;
  uint32_t bestBits;
  float diff;
  uint32_t uptime_ms;
};

// v10.11H: optional packet for future workers.
// Old workers safely ignore it because magic is 'A','R'.
struct __attribute__((packed)) JanusAgentRewardPacket {
  uint8_t magic[2];        // 'A','R'
  uint8_t version;         // 1
  char source[16];         // BuzzAgent
  char targetNode[24];
  uint32_t seq;
  uint8_t rewardLevel;     // 0 observe, 1 praise, 2 boost, 3 golden/share
  uint8_t aiHint;          // 1 stable, 2 slow, 3 boost
  uint16_t rewardPoints;
  uint16_t targetBatch;
  uint32_t entropySeed;    // worker can mix this into nonce/range pacing/visual rewards
  float score;
  float predictedHashRate;
  float predictionError;
  uint32_t deltaShares;
  uint32_t uptime_ms;
};

void applyJanusControl(const JanusControlPacket& cp);

struct RemoteShareItem {
  ShareResponse share;
  uint16_t bits;
  uint32_t total_hashes_l32;
  uint8_t hash_tail[4];
  uint32_t receivedAt;
  int8_t rssi;
  // v10.11H: exact job snapshot. Remote shares must be submitted with
  // the same job/en2/ntime/header that workers hashed, not the current live job.
  char jobIdText[96];
  char en2Hex[17];
  char ntimeHex[9];
  uint8_t header[80];
  uint8_t target[32];
  uint16_t targetBits;
};

struct ColonyRxItem {
  uint8_t srcMac[6];
  int8_t rssi;
  uint16_t len;
  uint32_t receivedAt;
  uint8_t data[JANUS_COLONY_RX_PACKET_MAX];
};

uint8_t JANUS_BROADCAST_MAC[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
uint32_t colonySeq = 0;
uint32_t colonyLastTxMs = 0;
uint32_t colonyLastBuzzStatusMs = 0;
uint32_t colonyLastJobTxMs = 0;
uint32_t colonyLastEchoMs = 0;
uint32_t colonyLastEchoSentUs = 0;
volatile uint32_t colonyRxCount = 0;
volatile uint32_t colonyRemoteAccepts = 0;
volatile uint32_t colonyRemoteSharesQueued = 0;
volatile uint32_t colonyRemoteSharesDropped = 0;
volatile uint32_t colonyRemoteWeakDrops = 0;
volatile uint32_t colonyRemoteDuplicateDrops = 0;
volatile uint32_t colonyRxQueueDropped = 0;
volatile uint32_t colonyRemoteLegacySeen = 0;
volatile uint32_t colonyEntropyReports = 0;
volatile uint32_t colonyHiveMetricReports = 0;
uint32_t colonyLastLiveDiscoveryPingMs = 0;
volatile uint32_t colonyEchoAckBits = 0;
volatile uint8_t colonyEchoBitCount = 0;
volatile float colonyEntropyAvg = 0.0f;
volatile int8_t colonyLastRssi = 0;
volatile uint32_t colonyBestPeerBits = 0;
volatile uint16_t colonyAiBatch = 900;
uint8_t colonyPeerChannel = 0;
uint32_t colonyLastChannelFixMs = 0;
JanusColonyPacket lastPeerPacket;

struct ColonyNodeSlot {
  bool used;
  uint8_t mac[6];
  char nodeId[24];
  char role[12];
  uint32_t firstSeenMs;
  uint32_t lastSeenMs;
  uint32_t rxPackets;
  uint32_t hashRate;
  uint32_t shares;
  uint32_t rejects;
  uint32_t bestBits;
  uint16_t targetBits;
  uint8_t aiHint;
  int8_t rssi;
  uint32_t uptime;

  // v10.11 Agent memory / prediction / reward state.
  uint32_t lastSharesForAgent;
  uint32_t lastRejectsForAgent;
  uint32_t lastHashForAgent;
  uint32_t lastBestForAgent;
  float emaHashRate;
  float emaBestBits;
  float predictedHashRate;
  float predictedBestBits;
  float predictionError;
  float score;
  uint32_t rewardPoints;
  uint32_t rewards;
  uint32_t lastRewardMs;
  uint32_t lastDeltaShares;
  uint32_t lastDeltaRejects;
  uint8_t rewardLevel;
  uint8_t entropyBoost;
};

ColonyNodeSlot colonyNodes[JANUS_COLONY_MAX_NODES];
// v10.11M: these snapshots used to be local arrays inside loopTask paths.
// Keeping them static prevents loopTask stack canary reboots under 5+ workers.
static ColonyNodeSlot colonyNodeLogSnap[JANUS_COLONY_MAX_NODES];
static uint8_t colonyJobMacSnap[JANUS_COLONY_MAX_NODES][6];
static char minerStatsCsvLine[384];
portMUX_TYPE colonyNodeMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint8_t colonyOnlineNodes = 0;
volatile uint8_t colonyKnownNodes = 0;
volatile uint32_t colonyUnicastJobsSent = 0;
volatile uint32_t colonyDiscoveryJobsSent = 0;
uint32_t colonyLastNodeLogMs = 0;

// v10.11 Agent scoreboard.
volatile uint32_t colonyAgentRewardsSent = 0;       // total Agent packets sent
volatile uint32_t colonyAgentShareRewardsSent = 0;  // Agent rewards caused by worker share/ticket delta
volatile uint32_t colonyAgentEntropySeed = 0xA1135EEDUL;
volatile uint32_t colonyAgentTopScoreX10 = 0;
volatile uint32_t colonyAgentRewardPointsTotal = 0;
volatile float colonyAgentPredictionErrorAvg = 0.0f;
char colonyAgentTopNode[24] = "-";

QueueHandle_t colonyRemoteShareQueue = nullptr;
QueueHandle_t colonyRxQueue = nullptr;
#if JANUS_SWARMSENSE_ENABLE
QueueHandle_t colonySwarmSenseQueue = nullptr;
SwarmSenseNodeSlot swarmSenseNodes[JANUS_SWARMSENSE_MAX_NODES];
portMUX_TYPE swarmSenseMux = portMUX_INITIALIZER_UNLOCKED;
volatile uint32_t colonySwarmSenseReports = 0;
volatile uint32_t colonySwarmSenseQueued = 0;
volatile uint32_t colonySwarmSenseDropped = 0;
volatile uint32_t colonySwarmSensePostOk = 0;
volatile uint32_t colonySwarmSensePostFail = 0;
uint32_t colonySwarmSenseLastPostMs = 0;
#endif

// Current pool job mirrored for voluntary workers.
volatile bool colonyMasterJobReady = false;
volatile bool colonyMasterClearPending = false;
uint8_t colonyMasterJobId[8] = {};
uint8_t colonyMasterHeader[80] = {};
uint8_t colonyMasterTarget[32] = {};
uint16_t colonyMasterTargetBits = 0;
uint32_t colonyMasterNonceCursor = 0;
uint32_t colonyMasterExtranonce2 = 0;
char colonyMasterJobText[18] = "-";
// v10.11I: widened textual Stratum params snapshot for queued remote shares.
char colonyMasterSubmitJobId[96] = "";
char colonyMasterSubmitEn2Hex[17] = "";
char colonyMasterSubmitNtimeHex[9] = "";

// v10.11H: Core0 miner mirrors jobs while Core1/UI broadcasts them.
// Guard 80-byte header/target/job text snapshots against torn reads.
SemaphoreHandle_t xColonyJobLock = nullptr;

bool colonyJobLock(uint32_t waitMs = 4) {
  if (!xColonyJobLock) return true;
  return xSemaphoreTake(xColonyJobLock, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}
void colonyJobUnlock() {
  if (xColonyJobLock) xSemaphoreGive(xColonyJobLock);
}

bool colonyClearMasterJob(const char* reason = nullptr) {
#if JANUS_COLONY_ENABLE
  bool locked = false;
  const uint32_t waitsMs[5] = {20, 50, 100, 100, 150};
  // Pool/Wi-Fi loss is safety-critical: avoid leaving stale work alive just because
  // a short broadcast/share snapshot briefly held the mutex. If all retries fail,
  // colonyTick() will try again through colonyMasterClearPending.
  for (uint8_t attempt = 0; attempt < 5 && !locked; ++attempt) {
    locked = colonyJobLock(waitsMs[attempt]);
    if (!locked) delay(1);
  }
  if (!locked) {
    colonyMasterClearPending = true;
    if (reason && reason[0]) Serial.printf("[COLONY] WARN: master job clear deferred, lock busy: %s\n", reason);
    return false;
  }

  bool wasReady = colonyMasterJobReady;
  colonyMasterJobReady = false;
  colonyMasterClearPending = false;
  memset((void*)colonyMasterJobId, 0, sizeof(colonyMasterJobId));
  memset((void*)colonyMasterHeader, 0, sizeof(colonyMasterHeader));
  memset((void*)colonyMasterTarget, 0, sizeof(colonyMasterTarget));
  colonyMasterTargetBits = 0;
  colonyMasterNonceCursor = 0;
  colonyMasterExtranonce2 = 0;
  strlcpy(colonyMasterJobText, "-", sizeof(colonyMasterJobText));
  strlcpy(colonyMasterSubmitJobId, "", sizeof(colonyMasterSubmitJobId));
  strlcpy(colonyMasterSubmitEn2Hex, "", sizeof(colonyMasterSubmitEn2Hex));
  strlcpy(colonyMasterSubmitNtimeHex, "", sizeof(colonyMasterSubmitNtimeHex));
  colonyJobUnlock();

  if (wasReady && reason && reason[0]) Serial.printf("[COLONY] master job cleared: %s\n", reason);
  return true;
#else
  (void)reason;
  return true;
#endif
}

bool colonyMacLooksValid(const uint8_t mac[6]) {
  if (!mac) return false;
  bool any = false;
  bool allFF = true;
  for (int i = 0; i < 6; ++i) {
    if (mac[i] != 0) any = true;
    if (mac[i] != 0xFF) allFF = false;
  }
  return any && !allFF;
}

void colonyMacToText(const uint8_t mac[6], char* out, size_t n) {
  if (!out || n < 18) return;
  snprintf(out, n, "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool colonyEspNowPeerTableFull() {
#if JANUS_COLONY_ENABLE
  esp_now_peer_num_t pn{};
  if (esp_now_get_peer_num(&pn) != ESP_OK) return false;
  return pn.total_num >= JANUS_COLONY_MAX_ESPNOW_PEERS;
#else
  return false;
#endif
}

bool colonyDropOldestInactivePeer(const uint8_t keepMac[6]) {
#if JANUS_COLONY_ENABLE
  uint8_t dropMac[6] = {0,0,0,0,0,0};
  bool have = false;
  uint32_t now = millis();
  uint32_t bestAge = 0;

  portENTER_CRITICAL(&colonyNodeMux);
  for (int pass = 0; pass < 2; ++pass) {
    bestAge = 0;
    have = false;
    for (int i = 0; i < JANUS_COLONY_MAX_NODES; ++i) {
      if (!colonyNodes[i].used) continue;
      if (!colonyMacLooksValid(colonyNodes[i].mac)) continue;
      if (keepMac && !memcmp(colonyNodes[i].mac, keepMac, 6)) continue;

      uint32_t age = now - colonyNodes[i].lastSeenMs;
      bool stale = age > JANUS_COLONY_NODE_TTL_MS;

      // pass 0: stale/offline only. pass 1: oldest of all.
      if (pass == 0 && !stale) continue;

      if (!have || age > bestAge) {
        memcpy(dropMac, colonyNodes[i].mac, 6);
        bestAge = age;
        have = true;
      }
    }
    if (have) break;
  }
  portEXIT_CRITICAL(&colonyNodeMux);

  if (have && colonyMacLooksValid(dropMac) && esp_now_is_peer_exist(dropMac)) {
    char macTxt[18] = "";
    colonyMacToText(dropMac, macTxt, sizeof(macTxt));
    esp_now_del_peer(dropMac);
    Serial.printf("[COLONY] ESP-NOW peer table full; evicted old peer %s age=%lu\n",
                  macTxt, (unsigned long)bestAge);
    return true;
  }
#endif
  return false;
}

void ensureColonyUnicastPeer(const uint8_t mac[6]) {
#if JANUS_COLONY_ENABLE
  if (!colonyMacLooksValid(mac)) return;
  if (esp_now_is_peer_exist(mac)) return;

  if (colonyEspNowPeerTableFull()) {
    colonyDropOldestInactivePeer(mac);
  }

  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, mac, 6);
  peer.channel = 0;       // current home Wi-Fi channel
  peer.encrypt = false;
  esp_err_t err = esp_now_add_peer(&peer);

  // Some SDK builds do not expose the "peer table full" error symbol,
  // so detect a full table by peer count and retry once after eviction.
  if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST && colonyEspNowPeerTableFull()) {
    if (colonyDropOldestInactivePeer(mac)) {
      err = esp_now_add_peer(&peer);
    }
  }

  if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
    char macTxt[18] = "";
    colonyMacToText(mac, macTxt, sizeof(macTxt));
    Serial.printf("[COLONY] add peer failed %s err=%d\n", macTxt, (int)err);
  }
#endif
}

int colonyFindNodeByMacUnsafe(const uint8_t mac[6]) {
  for (int i = 0; i < JANUS_COLONY_MAX_NODES; ++i) {
    if (colonyNodes[i].used && !memcmp(colonyNodes[i].mac, mac, 6)) return i;
  }
  return -1;
}

int colonyFindNodeByNameUnsafe(const char* nodeId) {
  if (!nodeId || !nodeId[0]) return -1;
  for (int i = 0; i < JANUS_COLONY_MAX_NODES; ++i) {
    if (colonyNodes[i].used && !strncmp(colonyNodes[i].nodeId, nodeId, sizeof(colonyNodes[i].nodeId))) return i;
  }
  return -1;
}

void colonyRecountNodesUnsafe(uint32_t now) {
  uint8_t known = 0;
  uint8_t online = 0;
  float bestScore = -1.0f;
  const char* bestNode = "-";

  for (int i = 0; i < JANUS_COLONY_MAX_NODES; ++i) {
    if (!colonyNodes[i].used) continue;
    known++;
    bool isOnline = (now - colonyNodes[i].lastSeenMs <= JANUS_COLONY_NODE_TTL_MS);
    if (isOnline) {
      online++;
      if (colonyNodes[i].score > bestScore) {
        bestScore = colonyNodes[i].score;
        bestNode = colonyNodes[i].nodeId;
      }
    }
  }

  colonyKnownNodes = known;
  colonyOnlineNodes = online;
  if (online > 0 && bestNode && bestNode[0]) {
    colonyAgentTopScoreX10 = (uint32_t)(bestScore * 10.0f);
    strlcpy(colonyAgentTopNode, bestNode, sizeof(colonyAgentTopNode));
  } else {
    colonyAgentTopScoreX10 = 0;
    strlcpy(colonyAgentTopNode, "-", sizeof(colonyAgentTopNode));
  }
}

float janusAgentClamp(float v, float lo, float hi) {
  if (!isfinite(v)) return lo;
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

uint8_t janusAgentHintFromScore(float score, uint32_t deltaShares, uint32_t deltaRejects) {
  if (deltaShares > 0 || score > 260.0f) return 3;  // reward/boost
  if (deltaRejects > 2 || score < 35.0f) return 2;  // calm down / smaller batches
  return 1;                                         // stable
}

uint16_t janusAgentBatchFromScore(float score, uint8_t hint) {
  if (hint == 2) return 520;
  if (hint == 3) return (score > 420.0f) ? 1600 : 1200;
  if (score > 180.0f) return 1100;
  return 900;
}

void colonySendAgentReward(const uint8_t mac[6],
                           const char* nodeId,
                           uint8_t rewardLevel,
                           uint8_t aiHint,
                           uint16_t rewardPoints,
                           uint16_t targetBatch,
                           uint32_t entropySeed,
                           float score,
                           float predictedHashRate,
                           float predictionError,
                           uint32_t deltaShares) {
#if JANUS_COLONY_ENABLE
#if JANUS_AGENT_ENABLE
  if (!colonyMacLooksValid(mac) || !nodeId || !nodeId[0]) return;
  ensureColonyUnicastPeer(mac);

  JanusAgentRewardPacket ar{};
  ar.magic[0] = 'A';
  ar.magic[1] = 'R';
  ar.version = 1;
  strlcpy(ar.source, "BuzzAgent", sizeof(ar.source));
  strlcpy(ar.targetNode, nodeId, sizeof(ar.targetNode));
  ar.seq = ++colonyAgentRewardsSent;
  if (deltaShares > 0) colonyAgentShareRewardsSent++;
  ar.rewardLevel = rewardLevel;
  ar.aiHint = aiHint;
  ar.rewardPoints = rewardPoints;
  ar.targetBatch = targetBatch;
  ar.entropySeed = entropySeed;
  ar.score = score;
  ar.predictedHashRate = predictedHashRate;
  ar.predictionError = predictionError;
  ar.deltaShares = deltaShares;
  ar.uptime_ms = millis();

  esp_now_send(mac, (uint8_t*)&ar, sizeof(ar));

  char macText[18];
  colonyMacToText(mac, macText, sizeof(macText));
  Serial.printf("[AGENT] reward node=%s mac=%s lvl=%u pts=%u hint=%u batch=%u score=%.1f predH=%.1f err=%.3f dShare=%lu\n",
                nodeId, macText, (unsigned)rewardLevel, (unsigned)rewardPoints,
                (unsigned)aiHint, (unsigned)targetBatch, score, predictedHashRate,
                predictionError, (unsigned long)deltaShares);

  if (janusSdReady) {
    char line[320];
    snprintf(line, sizeof(line),
             "%lu,%s,%s,%u,%u,%u,%u,%lu,%.2f,%.2f,%.4f,%lu",
             (unsigned long)millis(), nodeId, macText,
             (unsigned)rewardLevel, (unsigned)aiHint, (unsigned)rewardPoints,
             (unsigned)targetBatch, (unsigned long)entropySeed,
             score, predictedHashRate, predictionError,
             (unsigned long)deltaShares);
    janusSdAppendLine(JANUS_SD_ROOT "/logs/agent_rewards.csv", line);
  }
#endif
#endif
}

void colonyRememberNode(const uint8_t mac[6], const void* pktPtr, int8_t rssi) {
#if JANUS_COLONY_ENABLE
  if (!pktPtr) return;
  const JanusColonyPacket& pkt = *(const JanusColonyPacket*)pktPtr;

  if (!colonyMacLooksValid(mac)) return;
  if (!strncmp(pkt.nodeId, BTC_WORKER, sizeof(pkt.nodeId))) return;

  uint32_t now = millis();
  uint32_t deltaShares = 0;
  uint32_t deltaRejects = 0;
  bool isNew = false;
  bool rewardNow = false;
  uint8_t rewardLevel = 0;
  uint8_t rewardHint = 1;
  uint16_t rewardPoints = 0;
  uint16_t targetBatch = 900;
  uint32_t entropySeed = 0;
  float score = 0.0f;
  float predictedHash = 0.0f;
  float predErr = 0.0f;
  char macText[18];

  portENTER_CRITICAL(&colonyNodeMux);

  int idx = colonyFindNodeByMacUnsafe(mac);
  if (idx < 0) idx = colonyFindNodeByNameUnsafe(pkt.nodeId);

  if (idx < 0) {
    idx = 0;
    uint32_t oldest = 0xFFFFFFFFUL;
    for (int i = 0; i < JANUS_COLONY_MAX_NODES; ++i) {
      if (!colonyNodes[i].used) { idx = i; break; }
      if (colonyNodes[i].lastSeenMs < oldest) {
        oldest = colonyNodes[i].lastSeenMs;
        idx = i;
      }
    }
    memset(&colonyNodes[idx], 0, sizeof(colonyNodes[idx]));
    colonyNodes[idx].used = true;
    colonyNodes[idx].firstSeenMs = now;
    memcpy(colonyNodes[idx].mac, mac, 6);
    isNew = true;
  }

  uint32_t oldShares = colonyNodes[idx].shares;
  uint32_t oldRejects = colonyNodes[idx].rejects;
  uint32_t oldHash = colonyNodes[idx].hashRate;
  uint32_t oldBest = colonyNodes[idx].bestBits;
  if (pkt.shares > oldShares) deltaShares = pkt.shares - oldShares;
  if (pkt.rejects > oldRejects) deltaRejects = pkt.rejects - oldRejects;

  float oldPred = colonyNodes[idx].predictedHashRate;
  if (colonyNodes[idx].rxPackets == 0 || colonyNodes[idx].emaHashRate <= 0.01f) {
    colonyNodes[idx].emaHashRate = (float)pkt.hashRate;
    colonyNodes[idx].emaBestBits = (float)pkt.bestBits;
    oldPred = (float)pkt.hashRate;
  }

  float hNow = (float)pkt.hashRate;
  float bNow = (float)pkt.bestBits;
  float hTrend = hNow - (float)oldHash;
  float bTrend = bNow - (float)oldBest;

  predErr = fabsf(hNow - oldPred) / max(1.0f, hNow + 1.0f);
  predErr = janusAgentClamp(predErr, 0.0f, 9.99f);
  colonyAgentPredictionErrorAvg = colonyAgentPredictionErrorAvg * 0.94f + predErr * 0.06f;

  colonyNodes[idx].emaHashRate = colonyNodes[idx].emaHashRate * (1.0f - JANUS_AGENT_EMA_ALPHA) + hNow * JANUS_AGENT_EMA_ALPHA;
  colonyNodes[idx].emaBestBits = colonyNodes[idx].emaBestBits * (1.0f - JANUS_AGENT_EMA_ALPHA) + bNow * JANUS_AGENT_EMA_ALPHA;
  colonyNodes[idx].predictedHashRate = janusAgentClamp(colonyNodes[idx].emaHashRate + hTrend * 0.28f, 0.0f, 2500000.0f);
  colonyNodes[idx].predictedBestBits = janusAgentClamp(colonyNodes[idx].emaBestBits + bTrend * 0.18f, 0.0f, 256.0f);
  colonyNodes[idx].predictionError = predErr;

  int16_t overTarget = (int16_t)pkt.bestBits - (int16_t)pkt.targetBits;
  if (overTarget < 0) overTarget = 0;

  float instantScore =
      hNow * 0.018f +
      (float)pkt.bestBits * 3.0f +
      (float)overTarget * 12.0f +
      (float)deltaShares * 420.0f -
      (float)deltaRejects * 38.0f -
      predErr * 65.0f +
      ((pkt.aiHint == 3) ? 18.0f : 0.0f);

  instantScore = janusAgentClamp(instantScore, 0.0f, 9999.0f);
  colonyNodes[idx].score = colonyNodes[idx].score * JANUS_AGENT_SCORE_DECAY + instantScore * (1.0f - JANUS_AGENT_SCORE_DECAY);
  score = colonyNodes[idx].score;
  predictedHash = colonyNodes[idx].predictedHashRate;

  rewardHint = janusAgentHintFromScore(score, deltaShares, deltaRejects);
  targetBatch = janusAgentBatchFromScore(score, rewardHint);

  uint32_t pointsAdd = (uint32_t)(score / 20.0f) + deltaShares * 25UL;
  if (pointsAdd > 250) pointsAdd = 250;
  colonyNodes[idx].rewardPoints += pointsAdd;
  if (colonyNodes[idx].rewardPoints > 65000UL) colonyNodes[idx].rewardPoints = 65000UL;

  rewardLevel = 0;
  if (deltaShares > 0) rewardLevel = 3;
  else if (score > 260.0f) rewardLevel = 2;
  else if (score > 85.0f) rewardLevel = 1;

  entropySeed = (uint32_t)esp_random() ^ micros() ^ ((uint32_t)pkt.hashRate << 1) ^
                ((uint32_t)pkt.bestBits << 24) ^ ((uint32_t)deltaShares << 12) ^
                ((uint32_t)idx * 0x9E3779B9UL);
  colonyAgentEntropySeed ^= entropySeed + 0x9E3779B9UL + (colonyAgentEntropySeed << 6) + (colonyAgentEntropySeed >> 2);

  bool shareReward = (deltaShares > 0);
  bool praiseReward = (rewardLevel >= 2 && now - colonyNodes[idx].lastRewardMs >= JANUS_AGENT_PRAISE_MIN_MS);
  if (rewardLevel > 0 && (shareReward || praiseReward)) {
    colonyNodes[idx].lastRewardMs = now;
    colonyNodes[idx].rewards++;
    colonyNodes[idx].rewardLevel = rewardLevel;
    colonyNodes[idx].entropyBoost = (uint8_t)(entropySeed & 0xFF);
    rewardNow = true;
  }

  colonyNodes[idx].lastDeltaShares = deltaShares;
  colonyNodes[idx].lastDeltaRejects = deltaRejects;
  colonyNodes[idx].lastSharesForAgent = pkt.shares;
  colonyNodes[idx].lastRejectsForAgent = pkt.rejects;
  colonyNodes[idx].lastHashForAgent = pkt.hashRate;
  colonyNodes[idx].lastBestForAgent = pkt.bestBits;

  memcpy(colonyNodes[idx].mac, mac, 6);
  strlcpy(colonyNodes[idx].nodeId, pkt.nodeId, sizeof(colonyNodes[idx].nodeId));
  strlcpy(colonyNodes[idx].role, pkt.role, sizeof(colonyNodes[idx].role));
  colonyNodes[idx].lastSeenMs = now;
  colonyNodes[idx].rxPackets++;
  colonyNodes[idx].hashRate = pkt.hashRate;
  colonyNodes[idx].shares = pkt.shares;
  colonyNodes[idx].rejects = pkt.rejects;
  colonyNodes[idx].bestBits = pkt.bestBits;
  colonyNodes[idx].targetBits = pkt.targetBits;
  colonyNodes[idx].aiHint = pkt.aiHint;
  colonyNodes[idx].rssi = rssi ? rssi : pkt.rssi;
  colonyNodes[idx].uptime = pkt.uptime;

  if ((uint32_t)(score * 10.0f) > colonyAgentTopScoreX10) {
    colonyAgentTopScoreX10 = (uint32_t)(score * 10.0f);
    strlcpy(colonyAgentTopNode, pkt.nodeId, sizeof(colonyAgentTopNode));
  }
  colonyAgentRewardPointsTotal += pointsAdd;
  colonyRecountNodesUnsafe(now);

  rewardPoints = (uint16_t)min(65000UL, colonyNodes[idx].rewardPoints);

  portEXIT_CRITICAL(&colonyNodeMux);

  if (deltaShares > 0) colonyRemoteAccepts += deltaShares;
  if (pkt.bestBits > colonyBestPeerBits) colonyBestPeerBits = pkt.bestBits;

  ensureColonyUnicastPeer(mac);

  if (rewardNow) {
    colonySendAgentReward(mac, pkt.nodeId, rewardLevel, rewardHint, rewardPoints, targetBatch,
                          entropySeed, score, predictedHash, predErr, deltaShares);
  }

  if (isNew) {
    colonyMacToText(mac, macText, sizeof(macText));
    Serial.printf("[COLONY] node online id=%s role=%s mac=%s online=%u known=%u\n",
                  pkt.nodeId, pkt.role, macText, (unsigned)colonyOnlineNodes, (unsigned)colonyKnownNodes);
    janusSdLogf("NODE", "new id=%s role=%s mac=%s rssi=%d online=%u known=%u",
                pkt.nodeId, pkt.role, macText, (int)(rssi ? rssi : pkt.rssi),
                (unsigned)colonyOnlineNodes, (unsigned)colonyKnownNodes);
    // v10.11N: do not wait for the next periodic job tick; a hot-plugged worker
    // should receive a unicast nonce range on the next colonyTick().
    colonyLastJobTxMs = 0;
  }
#endif
}

uint8_t colonyOnlineNodeCount() {
  uint32_t now = millis();
  portENTER_CRITICAL(&colonyNodeMux);
  colonyRecountNodesUnsafe(now);
  uint8_t n = colonyOnlineNodes;
  portEXIT_CRITICAL(&colonyNodeMux);
  return n;
}

void colonyAgentSnapshot(char* topNode, size_t topNodeSize, float* predictionErrorAvg) {
#if JANUS_COLONY_ENABLE
  uint32_t now = millis();
  portENTER_CRITICAL(&colonyNodeMux);
  colonyRecountNodesUnsafe(now);
  if (topNode && topNodeSize) strlcpy(topNode, colonyAgentTopNode, topNodeSize);
  if (predictionErrorAvg) *predictionErrorAvg = colonyAgentPredictionErrorAvg;
  portEXIT_CRITICAL(&colonyNodeMux);
#else
  if (topNode && topNodeSize) strlcpy(topNode, "-", topNodeSize);
  if (predictionErrorAvg) *predictionErrorAvg = 0.0f;
#endif
}

#if JANUS_SWARMSENSE_ENABLE
String janusJsonEscapeTiny(const char* s, size_t maxLen) {
  String out;
  if (!s) return out;
  out.reserve(maxLen + 8);
  for (size_t i = 0; i < maxLen; ++i) {
    char c = s[i];
    if (!c) break;
    if (c == '"') out += F("\\\"");
    else if (c == '\\') out += F("\\\\");
    else if ((uint8_t)c < 32) out += ' ';
    else out += c;
  }
  return out;
}

int swarmSenseFindByMacUnsafe(const uint8_t mac[6]) {
  if (!mac) return -1;
  for (int i = 0; i < JANUS_SWARMSENSE_MAX_NODES; ++i) {
    if (swarmSenseNodes[i].used && !memcmp(swarmSenseNodes[i].mac, mac, 6)) return i;
  }
  return -1;
}

int swarmSenseFindByNameUnsafe(const char* nodeId) {
  if (!nodeId || !nodeId[0]) return -1;
  for (int i = 0; i < JANUS_SWARMSENSE_MAX_NODES; ++i) {
    if (swarmSenseNodes[i].used && !strncmp(swarmSenseNodes[i].nodeId, nodeId, sizeof(swarmSenseNodes[i].nodeId))) return i;
  }
  return -1;
}

void swarmSenseRememberNode(const uint8_t mac[6], const void* ssPtr, int8_t rxRssi) {
  if (!ssPtr) return;
  const SwarmSensePacket& ss = *(const SwarmSensePacket*)ssPtr;
  uint32_t now = millis();
  bool isNew = false;
  char nodeCopy[24];
  char kindCopy[16];
  strlcpy(nodeCopy, ss.nodeId[0] ? ss.nodeId : "unknown", sizeof(nodeCopy));
  strlcpy(kindCopy, ss.kind[0] ? ss.kind : "sense", sizeof(kindCopy));

  portENTER_CRITICAL(&swarmSenseMux);
  int idx = swarmSenseFindByMacUnsafe(mac);
  if (idx < 0) idx = swarmSenseFindByNameUnsafe(nodeCopy);
  if (idx < 0) {
    idx = 0;
    uint32_t oldest = 0xFFFFFFFFUL;
    for (int i = 0; i < JANUS_SWARMSENSE_MAX_NODES; ++i) {
      if (!swarmSenseNodes[i].used) { idx = i; break; }
      if (swarmSenseNodes[i].lastSeenMs < oldest) { oldest = swarmSenseNodes[i].lastSeenMs; idx = i; }
    }
    memset(&swarmSenseNodes[idx], 0, sizeof(swarmSenseNodes[idx]));
    swarmSenseNodes[idx].used = true;
    swarmSenseNodes[idx].firstSeenMs = now;
    if (mac) memcpy(swarmSenseNodes[idx].mac, mac, 6);
    isNew = true;
  }

  if (mac) memcpy(swarmSenseNodes[idx].mac, mac, 6);
  strlcpy(swarmSenseNodes[idx].nodeId, nodeCopy, sizeof(swarmSenseNodes[idx].nodeId));
  strlcpy(swarmSenseNodes[idx].kind, kindCopy, sizeof(swarmSenseNodes[idx].kind));
  swarmSenseNodes[idx].lastSeenMs = now;
  swarmSenseNodes[idx].samples++;
  swarmSenseNodes[idx].seq = ss.seq;
  swarmSenseNodes[idx].rssi = rxRssi ? rxRssi : ss.rssi;
  swarmSenseNodes[idx].radioMode = ss.radio_mode;
  swarmSenseNodes[idx].knnLabel = ss.knn_label;
  swarmSenseNodes[idx].knnConfidence = ss.knn_confidence;
  swarmSenseNodes[idx].aiHint = ss.ai_hint;
  swarmSenseNodes[idx].thermalLoad = ss.thermal_load;
  swarmSenseNodes[idx].effectiveBatch = ss.effective_batch;
  swarmSenseNodes[idx].dynamicBatch = ss.dynamic_batch;
  swarmSenseNodes[idx].hashRate = ss.hash_rate;
  swarmSenseNodes[idx].totalHashes = ss.total_hashes;
  swarmSenseNodes[idx].bestBits = ss.best_bits;
  swarmSenseNodes[idx].hashEffX1000 = ss.hash_eff_x1000;
  swarmSenseNodes[idx].predictionErrorX1000 = ss.prediction_error_x1000;
  portEXIT_CRITICAL(&swarmSenseMux);

  if (isNew) {
    char macText[18];
    colonyMacToText(mac, macText, sizeof(macText));
    Serial.printf("[SWARMSENSE] node online id=%s kind=%s mac=%s rssi=%d observe=1\n",
                  nodeCopy, kindCopy, macText, (int)(rxRssi ? rxRssi : ss.rssi));
    janusSdLogf("SWARMSENSE", "new id=%s kind=%s mac=%s rssi=%d observe=1",
                nodeCopy, kindCopy, macText, (int)(rxRssi ? rxRssi : ss.rssi));
  }
}

bool swarmSenseBuildJson(const void* itemPtr, String& body) {
  if (!itemPtr) return false;
  const SwarmSenseQueueItem& item = *(const SwarmSenseQueueItem*)itemPtr;
  const SwarmSensePacket& ss = item.pkt;
  if (ss.magic[0] != 'S' || ss.magic[1] != 'S' || ss.version != 1 || !ss.nodeId[0]) return false;
  String node = janusJsonEscapeTiny(ss.nodeId, sizeof(ss.nodeId));
  String kind = janusJsonEscapeTiny(ss.kind, sizeof(ss.kind));
  int8_t useRssi = item.rxRssi ? item.rxRssi : ss.rssi;
  body = "";
  body.reserve(920);
  body += F("{\"type\":\"swarm_sense\",\"version\":1,\"source\":\"buzz\"");
  body += F(",\"node_id\":\""); body += node; body += F("\"");
  body += F(",\"kind\":\""); body += kind; body += F("\"");
  body += F(",\"worker_id\":"); body += String((unsigned)ss.worker_id);
  body += F(",\"seq\":"); body += String((unsigned long)ss.seq);
  body += F(",\"uptime_ms\":"); body += String((unsigned long)ss.uptime_ms);
  body += F(",\"micros_tail\":"); body += String((unsigned long)ss.micros_tail);
  body += F(",\"free_heap\":"); body += String((unsigned long)ss.free_heap);
  body += F(",\"loop_jitter_us\":"); body += String((unsigned)ss.loop_jitter_us);
  body += F(",\"loop_max_us\":"); body += String((unsigned)ss.loop_max_us);
  body += F(",\"rssi\":"); body += String((int)useRssi);
  body += F(",\"rx_rssi\":"); body += String((int)item.rxRssi);
  body += F(",\"worker_rssi\":"); body += String((int)ss.rssi);
  body += F(",\"radio_mode\":"); body += String((unsigned)ss.radio_mode);
  body += F(",\"bt_flags\":"); body += String((unsigned)ss.bt_flags);
  body += F(",\"palette\":"); body += String((unsigned)ss.palette);
  body += F(",\"knn_label\":"); body += String((unsigned)ss.knn_label);
  body += F(",\"knn_confidence\":"); body += String((unsigned)ss.knn_confidence);
  body += F(",\"ai_hint\":"); body += String((unsigned)ss.ai_hint);
  body += F(",\"thermal_load\":"); body += String((unsigned)ss.thermal_load);
  body += F(",\"effective_batch\":"); body += String((unsigned)ss.effective_batch);
  body += F(",\"dynamic_batch\":"); body += String((unsigned)ss.dynamic_batch);
  body += F(",\"hash_rate\":"); body += String((unsigned long)ss.hash_rate);
  body += F(",\"total_hashes\":"); body += String((unsigned long)ss.total_hashes);
  body += F(",\"best_bits\":"); body += String((unsigned)ss.best_bits);
  body += F(",\"hash_eff_x1000\":"); body += String((unsigned)ss.hash_eff_x1000);
  body += F(",\"prediction_error_x1000\":"); body += String((int)ss.prediction_error_x1000);
  body += F(",\"entropy_x1000\":"); body += String((unsigned)ss.entropy_x1000);
  body += F(",\"touch_delta\":"); body += String((unsigned)ss.touch_delta);
  body += F(",\"job_age_s\":"); body += String((unsigned)ss.job_age_s);
  body += F(",\"nonce_remaining_l16\":"); body += String((unsigned)ss.nonce_remaining_l16);
  body += F(",\"flags\":"); body += String((unsigned)ss.flags);
  body += F(",\"buzz_received_ms\":"); body += String((unsigned long)item.receivedAt);
  body += F("}");
  return true;
}

bool janusAudioNeedsRealtimeNow() {
  uint32_t now = millis();
  if (audioBusySwitching) return true;
  if (now < audioCriticalUntilMs) return true;
  if (wanted && playing && !softPaused && !audioUserPaused) return true;
  return false;
}

void janusDropSwarmSenseBacklog(const char* reason) {
#if JANUS_SWARMSENSE_ENABLE
  if (!colonySwarmSenseQueue) return;
  UBaseType_t before = uxQueueMessagesWaiting(colonySwarmSenseQueue);
  if (before > 0) xQueueReset(colonySwarmSenseQueue);
  if (before > 0) {
    colonySwarmSenseDropped += before;
    Serial.printf("[SWARMSENSE] audio-safe drop backlog=%u reason=%s\n", (unsigned)before, reason ? reason : "-");
  }
#endif
}

bool swarmSensePostToNas(const void* itemPtr) {
  if (WiFi.status() != WL_CONNECTED) return false;
  if (!itemPtr) return false;
  const SwarmSenseQueueItem& item = *(const SwarmSenseQueueItem*)itemPtr;
  String body;
  if (!swarmSenseBuildJson(itemPtr, body)) return false;
  HTTPClient http;
  http.setConnectTimeout(JANUS_SWARMSENSE_HTTP_TIMEOUT_MS);
  http.setTimeout(JANUS_SWARMSENSE_HTTP_TIMEOUT_MS);
  if (!http.begin(JANUS_SWARMSENSE_NAS_URL)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST((uint8_t*)body.c_str(), body.length());
  String response = http.getString();
  http.end();
  bool ok = (code >= 200 && code < 300);
  if (!ok) {
    Serial.printf("[SWARMSENSE] NAS POST FAIL code=%d node=%s body=%s\n", code, item.pkt.nodeId, response.c_str());
  }
  return ok;
}

void swarmSenseTick() {
  if (!colonySwarmSenseQueue) return;
#if JANUS_SWARMSENSE_AUDIO_SAFE
  if (janusAudioNeedsRealtimeNow()) {
    // Observe stream is intentionally lossy. Never let NAS HTTP POST starve Audio.h/I2S/UI.
    if (uxQueueMessagesWaiting(colonySwarmSenseQueue) >= (JANUS_SWARMSENSE_QUEUE - 1)) {
      janusDropSwarmSenseBacklog("audio-realtime");
    }
    return;
  }
#endif
  if (WiFi.status() != WL_CONNECTED) return;
  uint32_t now = millis();
  if (colonySwarmSenseHoldUntilMs && now < colonySwarmSenseHoldUntilMs) {
    if (uxQueueMessagesWaiting(colonySwarmSenseQueue) >= (JANUS_SWARMSENSE_QUEUE - 1)) {
      janusDropSwarmSenseBacklog("nas-cooldown");
    }
    return;
  }
  if (now - colonySwarmSenseLastPostMs < JANUS_SWARMSENSE_POST_MIN_MS) return;
  SwarmSenseQueueItem item{};
  if (xQueueReceive(colonySwarmSenseQueue, &item, 0) != pdTRUE) return;
  colonySwarmSenseLastPostMs = now;
  bool ok = swarmSensePostToNas(&item);
  if (ok) {
    colonySwarmSenseFailStreak = 0;
    colonySwarmSenseHoldUntilMs = 0;
    colonySwarmSensePostOk++;
    if ((colonySwarmSensePostOk % 10) == 1) {
      Serial.printf("[SWARMSENSE] NAS OK node=%s seq=%lu ok=%lu fail=%lu\n",
                    item.pkt.nodeId, (unsigned long)item.pkt.seq,
                    (unsigned long)colonySwarmSensePostOk, (unsigned long)colonySwarmSensePostFail);
    }
  } else {
    colonySwarmSensePostFail++;
    colonySwarmSenseFailStreak++;
    // Do not requeue aggressively; observe stream is lossy by design and must not hurt audio/mining.
    if (colonySwarmSenseFailStreak >= JANUS_SWARMSENSE_FAIL_STREAK_LIMIT) {
      colonySwarmSenseHoldUntilMs = millis() + JANUS_SWARMSENSE_FAIL_COOLDOWN_MS;
      janusDropSwarmSenseBacklog("nas-fail-circuit");
      Serial.printf("[SWARMSENSE] NAS circuit open cooldown=%lums failStreak=%u\n",
                    (unsigned long)JANUS_SWARMSENSE_FAIL_COOLDOWN_MS,
                    (unsigned)colonySwarmSenseFailStreak);
    }
  }
}
#endif

void colonyLogNodesToSd() {
#if JANUS_COLONY_ENABLE
  uint32_t now = millis();
  if (now - colonyLastNodeLogMs < JANUS_COLONY_NODE_LOG_MS) return;
  colonyLastNodeLogMs = now;

  ColonyNodeSlot* snap = colonyNodeLogSnap;
  uint8_t count = 0;
  uint8_t online = 0;

  portENTER_CRITICAL(&colonyNodeMux);
  for (int i = 0; i < JANUS_COLONY_MAX_NODES; ++i) {
    if (!colonyNodes[i].used) continue;
    if (count >= JANUS_COLONY_MAX_NODES) continue;
    snap[count++] = colonyNodes[i];
    if (now - colonyNodes[i].lastSeenMs <= JANUS_COLONY_NODE_TTL_MS) online++;
  }
  colonyKnownNodes = count;
  colonyOnlineNodes = online;
  portEXIT_CRITICAL(&colonyNodeMux);

  janusSdLogf("COLONY", "online=%u known=%u unicastJobs=%lu discoveryJobs=%lu rx=%lu relayQ=%lu drop=%lu bestPeer=%lu hive=%lu ss=%lu ssOk=%lu ssFail=%lu",
              (unsigned)online,
              (unsigned)count,
              (unsigned long)colonyUnicastJobsSent,
              (unsigned long)colonyDiscoveryJobsSent,
              (unsigned long)colonyRxCount,
              (unsigned long)colonyRemoteSharesQueued,
              (unsigned long)colonyRemoteSharesDropped,
              (unsigned long)colonyBestPeerBits,
              (unsigned long)colonyHiveMetricReports,
              (unsigned long)colonySwarmSenseReports,
              (unsigned long)colonySwarmSensePostOk,
              (unsigned long)colonySwarmSensePostFail);

#if JANUS_SD_ENABLE
  if (janusSdReady) {
    for (uint8_t i = 0; i < count; ++i) {
      if (now - snap[i].lastSeenMs > JANUS_COLONY_NODE_TTL_MS) continue;
      char macText[18];
      colonyMacToText(snap[i].mac, macText, sizeof(macText));
      char line[384];
      snprintf(line, sizeof(line),
               "%lu,%s,%s,%s,%lu,%lu,%lu,%lu,%u,%d,%lu,%u,%.2f,%.2f,%.4f,%lu,%lu,%u",
               (unsigned long)now,
               snap[i].nodeId,
               snap[i].role,
               macText,
               (unsigned long)snap[i].hashRate,
               (unsigned long)snap[i].shares,
               (unsigned long)snap[i].rejects,
               (unsigned long)snap[i].bestBits,
               (unsigned)snap[i].targetBits,
               (int)snap[i].rssi,
               (unsigned long)snap[i].uptime,
               (unsigned)snap[i].aiHint,
               snap[i].score,
               snap[i].predictedHashRate,
               snap[i].predictionError,
               (unsigned long)snap[i].rewardPoints,
               (unsigned long)snap[i].rewards,
               (unsigned)snap[i].rewardLevel);
      janusSdAppendLine(JANUS_SD_ROOT "/logs/colony_nodes.csv", line);

      snprintf(line, sizeof(line),
               "%lu,%s,%lu,%lu,%lu,%.2f,%.2f,%.2f,%.4f,%lu,%lu,%u",
               (unsigned long)now,
               snap[i].nodeId,
               (unsigned long)snap[i].hashRate,
               (unsigned long)snap[i].shares,
               (unsigned long)snap[i].bestBits,
               snap[i].emaHashRate,
               snap[i].predictedHashRate,
               snap[i].predictedBestBits,
               snap[i].predictionError,
               (unsigned long)snap[i].rewardPoints,
               (unsigned long)snap[i].rewards,
               (unsigned)snap[i].aiHint);
      janusSdAppendLine(JANUS_SD_ROOT "/logs/agent_predictions.csv", line);
    }
  }
#endif
#endif
}

uint16_t effectiveMiningBatch() {
  uint16_t b = colonyAiBatch;
  if (b < 260) b = 260;
  if (b > 1800) b = 1800;

  // Audio first: keep the speaker alive during stream startup and normal playback.
  if (millis() < audioCriticalUntilMs) {
    if (b > 140) b = 140;
  } else if (playing && !softPaused) {
    if (b > 1050) b = 1050;
  }

  // Weak Wi-Fi means Stratum needs more breathing room.
  if (WiFi.status() == WL_CONNECTED && WiFi.RSSI() < -75 && b > 700) b = 700;
  return b;
}

void colonyMakeJobId(const String& jobId, uint8_t out[8]) {
  uint32_t h1 = 2166136261UL;
  uint32_t h2 = 0x9E3779B9UL;
  for (size_t i = 0; i < jobId.length(); i++) {
    uint8_t c = (uint8_t)jobId[i];
    h1 ^= c; h1 *= 16777619UL;
    h2 ^= (uint32_t)c + (h2 << 6) + (h2 >> 2);
  }
  out[0] = h1 & 0xFF; out[1] = (h1 >> 8) & 0xFF; out[2] = (h1 >> 16) & 0xFF; out[3] = (h1 >> 24) & 0xFF;
  out[4] = h2 & 0xFF; out[5] = (h2 >> 8) & 0xFF; out[6] = (h2 >> 16) & 0xFF; out[7] = (h2 >> 24) & 0xFF;
}

bool colonyJobIdMatches(const uint8_t in[8]) {
  if (!in) return false;
  uint8_t jobId[8] = {};
  bool ready = false;

  if (colonyJobLock(1)) {
    ready = colonyMasterJobReady;
    memcpy(jobId, (const void*)colonyMasterJobId, 8);
    colonyJobUnlock();
  } else {
    return false;
  }

  if (!ready) return false;
  for (int i = 0; i < 8; i++) if (in[i] != jobId[i]) return false;
  return true;
}

bool colonySnapshotJobForShare(const uint8_t in[8],
                               char* jobIdText, size_t jobIdTextSize,
                               char* en2Hex, size_t en2HexSize,
                               char* ntimeHex, size_t ntimeHexSize,
                               uint8_t* headerOut, size_t headerOutSize,
                               uint8_t* targetOut, size_t targetOutSize,
                               uint16_t* targetBitsOut) {
  // Keep this helper free of RemoteShareItem in the signature.
  // Arduino's .ino preprocessor may auto-generate prototypes before struct declarations.
  if (!in || !jobIdText || !en2Hex || !ntimeHex || !headerOut || !targetOut || !targetBitsOut) return false;
  if (jobIdTextSize < 2 || en2HexSize < 2 || ntimeHexSize < 2 || headerOutSize < 80 || targetOutSize < 32) return false;
  if (!colonyJobLock(2)) return false;

  bool ok = colonyMasterJobReady && (memcmp(in, (const void*)colonyMasterJobId, 8) == 0);
  if (ok) {
    strlcpy(jobIdText, colonyMasterSubmitJobId, jobIdTextSize);
    strlcpy(en2Hex, colonyMasterSubmitEn2Hex, en2HexSize);
    strlcpy(ntimeHex, colonyMasterSubmitNtimeHex, ntimeHexSize);
    memcpy(headerOut, (const void*)colonyMasterHeader, 80);
    memcpy(targetOut, (const void*)colonyMasterTarget, 32);
    *targetBitsOut = colonyMasterTargetBits;
    ok = (jobIdText[0] != '\0' && en2Hex[0] != '\0' && ntimeHex[0] != '\0' && *targetBitsOut > 0);
  }

  colonyJobUnlock();
  return ok;
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onColonySent(const wifi_tx_info_t *tx_info, esp_now_send_status_t status) {
#else
void onColonySent(const uint8_t *mac, esp_now_send_status_t status) {
#endif
  uint8_t bit = (status == ESP_NOW_SEND_SUCCESS) ? 1 : 0;
  colonyEchoAckBits = (colonyEchoAckBits << 1) | bit;
  if (++colonyEchoBitCount >= 32) {
    // Tiny entropy mix: let colony hints nudge batch size without touching pool correctness.
    uint32_t mix = colonyEchoAckBits ^ micros() ^ minerTotalHashes;
    if ((mix & 0x7) == 0 && colonyAiBatch < 1800) colonyAiBatch += 20;
    if ((mix & 0xF) == 1 && colonyAiBatch > 420) colonyAiBatch -= 20;
    colonyEchoBitCount = 0;
  }
}

void colonyProcessRxPacket(const uint8_t* srcMac, int8_t rssi, const uint8_t* data, int len, uint32_t receivedAt) {
  if (!data || len < 2) return;

  uint8_t safeMac[6] = {0,0,0,0,0,0};
  if (srcMac) memcpy(safeMac, srcMac, 6);
  colonyLastRssi = rssi;

  if (len == sizeof(JanusControlPacket) && data[0] == 'J' && data[1] == 'C') {
    JanusControlPacket cp{};
    memcpy(&cp, data, sizeof(cp));
    applyJanusControl(cp);
    colonyRxCount++;
    return;
  }

  if (len == sizeof(JanusColonyPacket)) {
    JanusColonyPacket pkt{};
    memcpy(&pkt, data, sizeof(pkt));
    if (memcmp(pkt.magic, "JANUS", 5) != 0) return;
    if (!strncmp(pkt.nodeId, BTC_WORKER, sizeof(pkt.nodeId))) return;

    memcpy((void*)&lastPeerPacket, &pkt, sizeof(JanusColonyPacket));
    colonyRxCount++;
    colonyRememberNode(safeMac, &pkt, rssi);

    // MicroAI colony tuning: peers cannot magically change SHA256 odds, but they can
    // share stability signals so all devices avoid reject storms and choose safer work batches.
    if (pkt.aiHint == 2 || pkt.rejects > (pkt.shares + 5)) {
      colonyAiBatch = 520;
    } else if (pkt.aiHint == 3 && pkt.hashRate > 0 && pkt.rejects <= pkt.shares + 1) {
      colonyAiBatch = 1200;
    } else if (colonyAiBatch < 900) {
      colonyAiBatch += 20;
    }
    return;
  }

  if (len == sizeof(ShareResponseV2) && data[0] == 'S' && data[1] == '2') {
    ShareResponseV2 shr2{};
    memcpy(&shr2, data, sizeof(shr2));
    RemoteShareItem item{};
    item.share.magic[0] = 'S';
    item.share.magic[1] = 'R';
    memcpy(item.share.job_id, shr2.job_id, 8);
    item.share.nonce = shr2.nonce;
    item.share.worker_id = shr2.worker_id;
    item.bits = shr2.bits;
    item.total_hashes_l32 = shr2.total_hashes_l32;
    memcpy(item.hash_tail, shr2.hash_tail, 4);
    item.receivedAt = receivedAt ? receivedAt : millis();
    item.rssi = rssi;

    // v10.11I/K: job-id match + submit/header/target snapshot are one mutex operation.
    if (!colonySnapshotJobForShare(shr2.job_id,
                                   item.jobIdText, sizeof(item.jobIdText),
                                   item.en2Hex, sizeof(item.en2Hex),
                                   item.ntimeHex, sizeof(item.ntimeHex),
                                   item.header, sizeof(item.header),
                                   item.target, sizeof(item.target),
                                   &item.targetBits)) {
      colonyRemoteSharesDropped++;
      return;
    }

    if (shr2.bits > colonyBestPeerBits) colonyBestPeerBits = shr2.bits;
    if (colonyRemoteShareQueue && xQueueSend(colonyRemoteShareQueue, &item, 0) == pdTRUE) {
      colonyRemoteSharesQueued++;
      colonyRxCount++;
    } else {
      colonyRemoteSharesDropped++;
    }
    return;
  }

  if (len == sizeof(ShareResponse) && data[0] == 'S' && data[1] == 'R') {
    ShareResponse shr{};
    memcpy(&shr, data, sizeof(shr));
    // Legacy S/R has no proof bits. Do not submit blindly anymore;
    // queue it, then the miner task recomputes the hash against the exact snapshot.
    RemoteShareItem item{};
    item.share = shr;
    item.bits = 0;
    item.total_hashes_l32 = 0;
    memset(item.hash_tail, 0, sizeof(item.hash_tail));
    item.receivedAt = receivedAt ? receivedAt : millis();
    item.rssi = rssi;

    // v10.11I/K: job-id match + submit/header/target snapshot are one mutex operation.
    if (!colonySnapshotJobForShare(shr.job_id,
                                   item.jobIdText, sizeof(item.jobIdText),
                                   item.en2Hex, sizeof(item.en2Hex),
                                   item.ntimeHex, sizeof(item.ntimeHex),
                                   item.header, sizeof(item.header),
                                   item.target, sizeof(item.target),
                                   &item.targetBits)) {
      colonyRemoteSharesDropped++;
      return;
    }
    colonyRemoteLegacySeen++;

    if (colonyRemoteShareQueue && xQueueSend(colonyRemoteShareQueue, &item, 0) == pdTRUE) {
      colonyRemoteSharesQueued++;
      colonyRxCount++;
    } else {
      colonyRemoteSharesDropped++;
    }
    return;
  }

#if JANUS_SWARMSENSE_ENABLE
  if (len == sizeof(SwarmSensePacket) && data[0] == 'S' && data[1] == 'S') {
    SwarmSensePacket ss{};
    memcpy(&ss, data, sizeof(ss));
    if (ss.version != 1 || !ss.nodeId[0] || !strncmp(ss.nodeId, BTC_WORKER, sizeof(ss.nodeId))) return;

    colonySwarmSenseReports++;
    colonyRxCount++;
    swarmSenseRememberNode(safeMac, &ss, rssi ? rssi : ss.rssi);

    float e = (float)ss.entropy_x1000 / 1000.0f;
    if (isfinite(e)) colonyEntropyAvg = colonyEntropyAvg * 0.94f + e * 0.06f;

    SwarmSenseQueueItem item{};
    item.pkt = ss;
    memcpy(item.srcMac, safeMac, 6);
    item.rxRssi = rssi ? rssi : ss.rssi;
    item.receivedAt = receivedAt ? receivedAt : millis();
    if (colonySwarmSenseQueue && xQueueSend(colonySwarmSenseQueue, &item, 0) == pdTRUE) {
      colonySwarmSenseQueued++;
    } else {
      colonySwarmSenseDropped++;
    }
    return;
  }
#endif

  if (len == sizeof(HiveMetricPacket) && data[0] == 'H' && data[1] == 'M') {
    HiveMetricPacket hm{};
    memcpy(&hm, data, sizeof(hm));
    if (!hm.nodeId[0] || !strncmp(hm.nodeId, BTC_WORKER, sizeof(hm.nodeId))) return;

    JanusColonyPacket pkt{};
    memcpy(pkt.magic, "JANUS", 6);
    strlcpy(pkt.nodeId, hm.nodeId, sizeof(pkt.nodeId));
    strlcpy(pkt.role, hm.kind[0] ? hm.kind : "hive", sizeof(pkt.role));
    pkt.seq = hm.seq;
    pkt.hashRate = hm.hash_rate;
    pkt.shares = hm.shares;
    pkt.rejects = hm.rejects;
    pkt.bestBits = hm.best_bits;
    pkt.diff = 0.0f;
    pkt.targetBits = hm.best_bits;
    pkt.aiBatch = hm.effective_batch;
    pkt.aiHint = hm.ai_hint;
    pkt.jobAgeMs = hm.job_age_ms;
    pkt.rssi = hm.rssi;
    pkt.uptime = hm.uptime_ms / 1000UL;

    colonyHiveMetricReports++;
    colonyRxCount++;
    colonyRememberNode(safeMac, &pkt, rssi ? rssi : hm.rssi);

    float e = (float)hm.entropy_x1000 / 1000.0f;
    if (isfinite(e)) colonyEntropyAvg = colonyEntropyAvg * 0.90f + e * 0.10f;
    if (hm.ai_hint == 2 && colonyAiBatch > 420) colonyAiBatch -= 20;
    else if (hm.ai_hint == 3 && colonyAiBatch < 1800) colonyAiBatch += 20;

    // Make the next JobPacket tick immediate for a node discovered from HM telemetry.
    colonyLastJobTxMs = 0;
    return;
  }

  if (len == sizeof(EntropyReport) && data[0] == 'E' && data[1] == 'R') {
    EntropyReport er{};
    memcpy(&er, data, sizeof(er));
    colonyEntropyReports++;
    colonyRxCount++;

    float e = er.local_entropy;
    if (!isfinite(e)) e = 0.0f;
    if (e < 0.0f) e = 0.0f;
    if (e > 9999.0f) e = 9999.0f;
    colonyEntropyAvg = colonyEntropyAvg * 0.86f + e * 0.14f;

    // Entropy influences worker batch gently; it never changes target validity.
    uint32_t salt = ((uint32_t)(e * 100.0f) ^ ((uint32_t)er.worker_id << 16) ^ micros());
    if ((salt & 0x0F) == 0 && colonyAiBatch < 1800) colonyAiBatch += 20;
    if ((salt & 0x1F) == 1 && colonyAiBatch > 420) colonyAiBatch -= 20;
    return;
  }

  if (len == sizeof(EchoProbe) && data[0] == 'E' && data[1] == 'P') {
    colonyRxCount++;
    return;
  }
}

void colonyDrainRxQueue(uint8_t maxItems = 8) {
#if JANUS_COLONY_ENABLE
  if (!colonyRxQueue) return;
  ColonyRxItem rx{};
  uint8_t drained = 0;
  while (drained < maxItems && xQueueReceive(colonyRxQueue, &rx, 0) == pdTRUE) {
    colonyProcessRxPacket(rx.srcMac, rx.rssi, rx.data, rx.len, rx.receivedAt);
    drained++;
  }
#else
  (void)maxItems;
#endif
}

#if ESP_ARDUINO_VERSION_MAJOR >= 3
void onColonyRecv(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
#else
void onColonyRecv(const uint8_t *mac, const uint8_t *data, int len) {
#endif
  if (!data || len < 2) return;
  if (len > JANUS_COLONY_RX_PACKET_MAX) {
    colonyRxQueueDropped++;
    return;
  }

  ColonyRxItem rx{};
  rx.len = (uint16_t)len;
  rx.receivedAt = millis();
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  if (info && info->rx_ctrl) rx.rssi = info->rx_ctrl->rssi;
  if (info && info->src_addr) memcpy(rx.srcMac, info->src_addr, 6);
#else
  rx.rssi = 0;
  if (mac) memcpy(rx.srcMac, mac, 6);
#endif
  colonyLastRssi = rx.rssi;

  // Remote shares are rare and correctness-critical: snapshot the current job immediately
  // so a valid share is not lost if a new pool job arrives before colonyTick drains RX.
  // Everything heavier (control/audio/peer/agent/SD/status/entropy) stays deferred.
  if ((len == sizeof(ShareResponseV2) && data[0] == 'S' && data[1] == '2') ||
      (len == sizeof(ShareResponse) && data[0] == 'S' && data[1] == 'R')) {
    colonyProcessRxPacket(rx.srcMac, rx.rssi, data, len, rx.receivedAt);
    return;
  }

  memcpy(rx.data, data, len);
  if (colonyRxQueue && xQueueSend(colonyRxQueue, &rx, 0) == pdTRUE) {
    return;
  }
  colonyRxQueueDropped++;
}

uint8_t currentWifiChannel() {
  uint8_t primary = 0;
  wifi_second_chan_t second = WIFI_SECOND_CHAN_NONE;
  if (esp_wifi_get_channel(&primary, &second) != ESP_OK) return 0;
  return primary;
}

void ensureColonyPeerChannel() {
#if JANUS_COLONY_ENABLE
  if (WiFi.status() != WL_CONNECTED) return;
  uint8_t ch = currentWifiChannel();
  if (ch == 0 || ch == colonyPeerChannel) return;
  if (millis() - colonyLastChannelFixMs < JANUS_COLONY_PEER_REFIX_MS) return;
  colonyLastChannelFixMs = millis();

  if (esp_now_is_peer_exist(JANUS_BROADCAST_MAC)) esp_now_del_peer(JANUS_BROADCAST_MAC);
  esp_now_peer_info_t peer{};
  memcpy(peer.peer_addr, JANUS_BROADCAST_MAC, 6);
  peer.channel = ch;
  peer.encrypt = false;
  if (esp_now_add_peer(&peer) == ESP_OK) {
    colonyPeerChannel = ch;
    Serial.printf("[COLONY] ESP-NOW peer channel=%u\n", ch);
  }
#endif
}

void initColonyNow() {
#if JANUS_COLONY_ENABLE
  if (!xColonyJobLock) xColonyJobLock = xSemaphoreCreateMutex();

  // Use the current Wi-Fi STA channel. Do NOT force channel here,
  // otherwise ESP-NOW can kick the pool connection off-channel.
  WiFi.mode(WIFI_STA);
  if (!colonyRemoteShareQueue) colonyRemoteShareQueue = xQueueCreate(JANUS_COLONY_REMOTE_SHARE_QUEUE, sizeof(RemoteShareItem));
  if (!colonyRxQueue) colonyRxQueue = xQueueCreate(JANUS_COLONY_RX_QUEUE, sizeof(ColonyRxItem));
#if JANUS_SWARMSENSE_ENABLE
  if (!colonySwarmSenseQueue) colonySwarmSenseQueue = xQueueCreate(JANUS_SWARMSENSE_QUEUE, sizeof(SwarmSenseQueueItem));
#endif
  if (janusColonyEspNowActive) return;
  if (esp_now_init() != ESP_OK) {
    janusColonyEspNowActive = false;
    Serial.println("[COLONY] ESP-NOW init failed");
    return;
  }
  janusColonyEspNowActive = true;
  esp_now_register_recv_cb(onColonyRecv);
  esp_now_register_send_cb(onColonySent);
  colonyPeerChannel = 0;
  ensureColonyPeerChannel();
  Serial.printf("[COLONY] ESP-NOW master ready: remoteQ=%u rxQ=%u packetMax=%u\n",
                (unsigned)JANUS_COLONY_REMOTE_SHARE_QUEUE,
                (unsigned)JANUS_COLONY_RX_QUEUE,
                (unsigned)JANUS_COLONY_RX_PACKET_MAX);
#if JANUS_SWARMSENSE_ENABLE
  Serial.printf("[SWARMSENSE] observe bridge ready: queue=%u nas=%s apply=0\n",
                (unsigned)JANUS_SWARMSENSE_QUEUE, JANUS_SWARMSENSE_NAS_URL);
#endif
#endif
}

void sendColonyHeartbeat() {
#if JANUS_COLONY_ENABLE
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - colonyLastTxMs < 2000) return;
  colonyLastTxMs = millis();
  ensureColonyPeerChannel();

  JanusColonyPacket pkt{};
  memcpy(pkt.magic, "JANUS", 6);
  strlcpy(pkt.nodeId, BTC_WORKER, sizeof(pkt.nodeId));
  strlcpy(pkt.role, "BuzzLighter", sizeof(pkt.role));
  pkt.seq = ++colonySeq;
  pkt.hashRate = minerRealHashrate;
  pkt.shares = minerShares;
  pkt.rejects = minerSubmitRejects;
  pkt.bestBits = minerBestBits;
  pkt.diff = minerCurrentDiffF;
  pkt.targetBits = minerShareTargetBits;
  pkt.aiBatch = effectiveMiningBatch();
  pkt.jobAgeMs = minerLastJobMs ? (millis() - minerLastJobMs) : 0;
  if (minerSubmitRejects > minerShares + 3) pkt.aiHint = 2;
  else if (minerShares > 0 || minerBestBits >= minerShareTargetBits) pkt.aiHint = 3;
  else pkt.aiHint = 1;
  pkt.rssi = WiFi.RSSI();
  pkt.uptime = millis() / 1000;

  esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&pkt, sizeof(pkt));
  if (err != ESP_OK && millis() - colonyLastChannelFixMs > JANUS_COLONY_PEER_REFIX_MS) {
    colonyPeerChannel = 0;
    ensureColonyPeerChannel();
  }
#endif
}

void sendBuzzStatusPacket() {
#if JANUS_COLONY_ENABLE
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - colonyLastBuzzStatusMs < 1000UL) return;
  colonyLastBuzzStatusMs = millis();
  ensureColonyPeerChannel();

  JanusBuzzStatusPacket bs{};
  bs.magic[0] = 'B';
  bs.magic[1] = 'S';
  bs.version = 1;
  strlcpy(bs.nodeId, BTC_WORKER, sizeof(bs.nodeId));

  const char* tn = trackName;
  if (!tn || !tn[0] || strcmp(tn, "unknown") == 0) {
    tn = wanted ? "Buzz stream active" : "Buzz idle";
  }
  strlcpy(bs.track, tn, sizeof(bs.track));

  bs.playing = (wanted && playing && !softPaused && !audioUserPaused) ? 1 : 0;
  bs.paused = (softPaused || audioUserPaused) ? 1 : 0;
  bs.volume = volumeVal;
  bs.brightness = ledBright;
  bs.hashRate = minerRealHashrate ? minerRealHashrate : minerLocalHashrate;
  bs.shares = minerShares;
  bs.rejects = minerSubmitRejects;
  bs.bestBits = minerBestBits;
  bs.diff = minerCurrentDiffF;
  bs.uptime_ms = millis();

  esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&bs, sizeof(bs));
#endif
}

void sendColonyEchoProbe() {
#if JANUS_COLONY_ENABLE
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - colonyLastEchoMs < JANUS_COLONY_ECHO_MS) return;
  colonyLastEchoMs = millis();
  ensureColonyPeerChannel();

  EchoProbe ep{};
  ep.magic[0] = 'E';
  ep.magic[1] = 'P';
  ep.seq = colonySeq;
  ep.entropyBits = colonyEchoAckBits ^ colonyAgentEntropySeed ^ esp_random() ^ micros();
  colonyLastEchoSentUs = micros();
  esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&ep, sizeof(ep));
#endif
}

void mirrorColonyJobForWorkers(const String& currentJobId, const uint8_t header[80], uint32_t extranonce2Value, const char* submitEn2Hex, const String& submitNtimeHex) {
#if JANUS_COLONY_ENABLE
  if (!header) return;

  uint8_t nextJobId[8] = {};
  colonyMakeJobId(currentJobId, nextJobId);

  if (!colonyJobLock(25)) {
    Serial.println("[COLONY] job mirror skipped: lock busy");
    return;
  }

  memcpy((void*)colonyMasterJobId, nextJobId, 8);
  memcpy((void*)colonyMasterHeader, header, 80);
  memcpy((void*)colonyMasterTarget, (const void*)minerShareTarget, 32);
  colonyMasterTargetBits = minerShareTargetBits;
  colonyMasterNonceCursor = esp_random();
  colonyMasterExtranonce2 = extranonce2Value;
  strlcpy(colonyMasterSubmitJobId, currentJobId.c_str(), sizeof(colonyMasterSubmitJobId));
  strlcpy(colonyMasterSubmitEn2Hex, submitEn2Hex ? submitEn2Hex : "", sizeof(colonyMasterSubmitEn2Hex));
  strlcpy(colonyMasterSubmitNtimeHex, submitNtimeHex.c_str(), sizeof(colonyMasterSubmitNtimeHex));
  colonyMasterJobReady = true;

  uint32_t jidA = 0, jidB = 0;
  memcpy(&jidA, &colonyMasterJobId[0], 4);
  memcpy(&jidB, &colonyMasterJobId[4], 4);
  snprintf(colonyMasterJobText, sizeof(colonyMasterJobText), "%08lx%08lx",
           (unsigned long)jidA, (unsigned long)jidB);
  colonyJobUnlock();
#endif
}

void broadcastColonyJob() {
#if JANUS_COLONY_ENABLE
  if (WiFi.status() != WL_CONNECTED) return;
  if (millis() - colonyLastJobTxMs < JANUS_COLONY_JOB_MS) return;

  JobPacket job{};
  job.magic[0] = 'J';
  job.magic[1] = 'B';
  char jobTextSnap[18] = "-";

  // Snapshot header/target/job id atomically, then release lock before ESP-NOW sends.
  if (!colonyJobLock(4)) return;
  bool ready = colonyMasterJobReady;
  if (ready) {
    memcpy(job.job_id, (const void*)colonyMasterJobId, 8);
    memcpy(job.header, (const void*)colonyMasterHeader, 80);
    memcpy(job.target, (const void*)colonyMasterTarget, 32);
    job.extranonce2 = colonyMasterExtranonce2;
    strlcpy(jobTextSnap, colonyMasterJobText, sizeof(jobTextSnap));
  }
  colonyJobUnlock();
  if (!ready) return;

  colonyLastJobTxMs = millis();
  ensureColonyPeerChannel();

  job.range_size = JANUS_COLONY_WORKER_RANGE;

  uint8_t count = 0;
  uint32_t now = millis();

  portENTER_CRITICAL(&colonyNodeMux);
  colonyRecountNodesUnsafe(now);
  for (int i = 0; i < JANUS_COLONY_MAX_NODES && count < JANUS_COLONY_MAX_NODES; ++i) {
    if (!colonyNodes[i].used) continue;
    if (now - colonyNodes[i].lastSeenMs > JANUS_COLONY_NODE_TTL_MS) continue;
    memcpy(colonyJobMacSnap[count++], colonyNodes[i].mac, 6);
  }
  portEXIT_CRITICAL(&colonyNodeMux);

  uint8_t sent = 0;
  for (uint8_t i = 0; i < count; ++i) {
    if (!colonyMacLooksValid(colonyJobMacSnap[i])) continue;
    ensureColonyUnicastPeer(colonyJobMacSnap[i]);

    if (!colonyJobLock(2)) continue;
    job.start_nonce = colonyMasterNonceCursor;
    job.range_size = JANUS_COLONY_WORKER_RANGE;
    colonyMasterNonceCursor += JANUS_COLONY_WORKER_RANGE;
    colonyJobUnlock();

    esp_err_t err = esp_now_send(colonyJobMacSnap[i], (uint8_t*)&job, sizeof(job));
    if (err == ESP_OK) {
      sent++;
      colonyUnicastJobsSent++;
    }
  }

  // Discovery fallback: any future device only needs compatible firmware.
  // v10.11N: when known workers already received unicast ranges, broadcast only a
  // zero-range discovery ping. This wakes/identifies new workers without duplicating
  // nonce work on already-running legacy workers. If no unicast worker exists, keep
  // the old real discovery range so a lone passive worker can start.
  bool sendDiscovery = (sent == 0);
  bool liveDiscoveryPing = false;
#if JANUS_COLONY_PERIODIC_JOB_DISCOVERY
  if (!sendDiscovery && ((colonySeq % JANUS_COLONY_DISCOVERY_EVERY_SEQ) == 0)) sendDiscovery = true;
#endif
  if (!sendDiscovery && (millis() - colonyLastLiveDiscoveryPingMs >= JANUS_COLONY_LIVE_DISCOVERY_PING_MS)) {
    sendDiscovery = true;
    liveDiscoveryPing = true;
  }
  if (sendDiscovery) {
    if (!colonyJobLock(2)) return;
    job.start_nonce = colonyMasterNonceCursor;
#if JANUS_COLONY_LIVE_DISCOVERY_ZERO_RANGE
    if (liveDiscoveryPing && sent > 0) {
      job.range_size = 0;
    } else
#endif
    {
      job.range_size = JANUS_COLONY_DISCOVERY_RANGE;
      colonyMasterNonceCursor += JANUS_COLONY_DISCOVERY_RANGE;
    }
    colonyJobUnlock();

    esp_err_t err = esp_now_send(JANUS_BROADCAST_MAC, (uint8_t*)&job, sizeof(job));
    if (err == ESP_OK) {
      colonyDiscoveryJobsSent++;
      colonyLastLiveDiscoveryPingMs = millis();
    }
  }

  if ((colonySeq % 10) == 0) {
    uint32_t nextNonceForLog = 0;
    if (colonyJobLock(1)) {
      nextNonceForLog = colonyMasterNonceCursor;
      colonyJobUnlock();
    }
    Serial.printf("[COLONY] jobs id=%s workers=%u unicast=%u discovery=%u next=%lu range=%lu target=%u q=%lu drop=%lu bestPeer=%lu hive=%lu\n",
                  jobTextSnap,
                  (unsigned)count,
                  (unsigned)sent,
                  sendDiscovery ? 1U : 0U,
                  (unsigned long)nextNonceForLog,
                  (unsigned long)JANUS_COLONY_WORKER_RANGE,
                  (unsigned)minerShareTargetBits,
                  (unsigned long)colonyRemoteSharesQueued,
                  (unsigned long)colonyRemoteSharesDropped,
                  (unsigned long)colonyBestPeerBits,
              (unsigned long)colonyHiveMetricReports);
  }
#endif
}

void colonyTick() {
#if JANUS_COLONY_ENABLE
  // Drain inbound ESP-NOW work outside the Wi-Fi callback. This is where peer add,
  // agent reward, SD logging, control commands, and share snapshot work are allowed.
  colonyDrainRxQueue(janusAudioNeedsRealtimeNow() ? 2 : 8);
  swarmSenseTick();

  if (colonyMasterClearPending) {
    colonyClearMasterJob("pending-clear");
  }

  sendColonyHeartbeat();
  sendBuzzStatusPacket();
  sendColonyEchoProbe();
  colonyLogNodesToSd();
  if (colonyOnlineNodeCount() == 0) {
    if (colonyAiBatch < 900) colonyAiBatch += 5;
    else if (colonyAiBatch > 900) colonyAiBatch -= 5;
  }
  broadcastColonyJob();
#endif
}

void hashToShareOrder(const uint8_t in[32], uint8_t out[32]);
// ============================================================
// CORE 0: VAULT (TRUE STRATUM MINER V25)
// ============================================================

void minerClassifyReject(const char* errText) {
  if (!errText) errText = "unknown";
  strlcpy(minerLastRejectReason, errText, sizeof(minerLastRejectReason));
  String e = String(errText);
  e.toLowerCase();
  if (e.indexOf("low") >= 0 || e.indexOf("difficulty") >= 0 || e.indexOf("target") >= 0) {
    minerLowDiffRejects++;
  } else if (e.indexOf("stale") >= 0 || e.indexOf("job") >= 0 || e.indexOf("duplicate") >= 0) {
    minerStaleJobRejects++;
  } else {
    minerOtherRejects++;
  }
}


void microMinerTask(void *pvParameters) {
  WiFiClient client;
  client.setTimeout(120);

  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);

  uint32_t hashesThisSecond = 0;
  uint32_t localHashesThisSecond = 0;
  uint32_t lastHashTick = millis();

  uint8_t blockHeader[80];
  uint8_t hash1[32];
  uint8_t hash2[32];

  String currentJobId = "";
  String ntimeHex = "";
  char en2Hex[17] = "";
  uint32_t nonce = 0;
  bool jobReady = false;
  bool authorized = false;
  uint8_t stratumDebugLines = 0;

  setShareTargetFromDifficulty(1.0f);

  while(true) {
    // Give Audio.h exclusive breathing room during codec/stream startup.
    if (millis() < audioCriticalUntilMs) {
      vTaskDelay(pdMS_TO_TICKS(25));
      continue;
    }

    if (janusThermalStop) {
      if (client.connected()) client.stop();
      stratumConnected = false;
      jobReady = false;
      authorized = false;
      minerRealHashrate = 0;
      minerLocalHashrate = 0;
      minerLocalFallback = false;
      strlcpy(minerStatus, "THERMAL", sizeof(minerStatus));
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    if(WiFi.status() != WL_CONNECTED) {
      if (client.connected()) client.stop();
      stratumConnected = false;
      jobReady = false;
      colonyClearMasterJob("wifi");
      authorized = false;
      minerLocalFallback = false;
      minerRealHashrate = 0;
      minerLocalHashrate = 0;
      strlcpy(minerStatus, "WIFI", sizeof(minerStatus));
      lastWifiKickMs = millis();
      janusWifiKickGuarded("miner-wifi-lost");
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    } else {
      janusWifiRecovered();
    }

    if(!client.connected()) {
      stratumConnected = false;
      jobReady = false;
      colonyClearMasterJob("pool-disconnect");
      authorized = false;
      minerLocalFallback = false;
      strlcpy(minerStatus, "POOL", sizeof(minerStatus));

      uint32_t nowPool = millis();
      if (poolReconnectHoldUntilMs && nowPool < poolReconnectHoldUntilMs) {
        vTaskDelay(pdMS_TO_TICKS(250));
        continue;
      }

      if(client.connect(POOL_HOST, POOL_PORT)) {
        connectedAtMs = millis();
        poolReconnectHoldUntilMs = 0;
        stratumConnected = true;
        client.setTimeout(1000);
        client.setNoDelay(true);
        stratumDebugLines = 0;
        Serial.printf("[MINER] Pool connected: %s:%u as %s\n", POOL_HOST, POOL_PORT, MINER_USER);
        janusSdLogf("POOL", "connected host=%s port=%u worker=%s", POOL_HOST, POOL_PORT, MINER_USER);
        // v10.11H: official Nerdminer low-diff pool endpoint.
        // No suggest_difficulty here; pool.nerdminers.org handles NerdMiner low-diff policy.
        client.println("{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"NerdMinerV2/JANUS-v10.11-Agent\"]}");
        String loginStr0 = String("{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"") + minerUserString() + "\",\"x\"]}";
        client.println(loginStr0);
        Serial.println("[MINER] TX subscribe+authorize NerdMinerV2 via println()");
        strlcpy(minerStatus, "SUBAUTH", sizeof(minerStatus));
      } else {
        poolReconnectFails++;
        uint32_t backoffMs = 3000UL + (uint32_t)min((int)poolReconnectFails, 8) * 2000UL;
        poolReconnectHoldUntilMs = millis() + backoffMs;
        Serial.printf("[MINER] pool connect failed, backoff=%lums fail=%u\n", (unsigned long)backoffMs, poolReconnectFails);
        janusSdLogf("POOLFAIL", "backoffMs=%lu fail=%u", (unsigned long)backoffMs, poolReconnectFails);
        if (poolReconnectFails >= 12) {
          Serial.println("[MINER] reconnect storm -> deep sleep 30s");
          janusSdLogf("SLEEP", "reconnect storm 30s");
          esp_sleep_enable_timer_wakeup(30ULL * 1000ULL * 1000ULL);
          esp_deep_sleep_start();
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    uint8_t linesPerTick = 0;
    while(client.connected() && client.available() && linesPerTick < 6) {
      linesPerTick++;
      String line = client.readStringUntil('\n');
      line.trim();
      if (!line.length()) continue;
      if (stratumDebugLines < 16) {
        Serial.print("[STRATUM RX] ");
        Serial.println(line);
        stratumDebugLines++;
      }

      StaticJsonDocument<4096> doc;
      DeserializationError err = deserializeJson(doc, line);
      if (err) { Serial.printf("[MINER] JSON error: %s\n", err.c_str()); continue; }

      if (doc["id"] == 1 && doc["result"].is<JsonArray>()) {
        extranonce1 = doc["result"][1].as<String>();
        extranonce2_size = doc["result"][2].as<int>();
        if (extranonce2_size <= 0 || extranonce2_size > 8) extranonce2_size = 4;
        Serial.printf("[MINER] Subscribed extranonce2_size=%d\n", extranonce2_size);
        janusSdLogf("SUB", "extranonce2_size=%d", extranonce2_size);
        String loginStr = String("{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"") + minerUserString() + "\",\"x\"]}\r\n";
        client.print(loginStr);
        strlcpy(minerStatus, "AUTH", sizeof(minerStatus));
      }

      if (doc["id"] == 2) {
        authorized = (doc["result"] == true);
        if (authorized) poolReconnectFails = 0;
        Serial.println(authorized ? "[MINER] AUTH OK" : "[MINER] AUTH REJECT");
        janusSdLogf("AUTH", "authorized=%d", authorized ? 1 : 0);
        strlcpy(minerStatus, authorized ? "AUTHOK" : "AUTHBAD", sizeof(minerStatus));
      }

      const char* method = doc["method"] | "";
      if (!strcmp(method, "client.reconnect")) {
        poolReconnectFails++;
        poolReconnectHoldUntilMs = millis() + 15000UL;
        Serial.println("[MINER] pool requested client.reconnect -> 15s cooldown");
        janusSdLogf("RECONN", "pool requested reconnect cooldown=15s fail=%u", poolReconnectFails);
        strlcpy(minerStatus, "RECONN", sizeof(minerStatus));
        client.stop();
        stratumConnected = false;
        jobReady = false;
        colonyClearMasterJob("client-reconnect");
        authorized = false;
        extranonce1 = "";
        if (poolReconnectFails >= 12) {
          Serial.println("[MINER] reconnect storm after client.reconnect -> deep sleep 30s");
          janusSdLogf("SLEEP", "client.reconnect storm 30s");
          esp_sleep_enable_timer_wakeup(30ULL * 1000ULL * 1000ULL);
          esp_deep_sleep_start();
        }
        continue;
      }
      if (!strcmp(method, "mining.set_extranonce")) {
        extranonce1 = doc["params"][0].as<String>();
        extranonce2_size = doc["params"][1].as<int>();
        if (extranonce2_size <= 0 || extranonce2_size > 8) extranonce2_size = 4;
        extranonce2 = 0;
        jobReady = false;
        colonyClearMasterJob("set-extranonce");
        strlcpy(minerStatus, "EXNONCE", sizeof(minerStatus));
        Serial.printf("[MINER] set_extranonce ex1=%s ex2_size=%d -> waiting new job\n", extranonce1.c_str(), extranonce2_size);
      }


      if (!strcmp(method, "mining.set_difficulty")) {
        float diff = doc["params"][0].as<float>();
        if (diff <= 0.0f) diff = 0.0001f;
        minerPoolSuggestedDiffF = diff;
        setShareTargetFromDifficulty(diff);
        float diffForUi = (float)minerCurrentDiffF;
        if (diffForUi < 1.0f) diffForUi = 1.0f;
        minerCurrentDiff = (uint32_t)diffForUi;
        janusSdLogf("DIFF", "pool=%.8f effective=%.8f targetBits=%u minBits=%u", minerPoolSuggestedDiffF, minerCurrentDiffF, minerShareTargetBits, (unsigned)JANUS_NERDMINER_MERKLE_MIN_SHARE_BITS);
        Serial.printf("[MINER] Pool difficulty=%.8f effective=%.8f targetBits=%u minBits=%u\n", diff, minerCurrentDiffF, minerShareTargetBits, JANUS_NERDMINER_MERKLE_MIN_SHARE_BITS);
      }

      if (!strcmp(method, "mining.notify")) {
        JsonArray params = doc["params"];
        if (params.size() < 8) continue;

        currentJobId = params[0].as<String>();
        String prevhashHex = params[1].as<String>();
        String coinb1Hex = params[2].as<String>();
        String coinb2Hex = params[3].as<String>();
        JsonArray merkleBranch = params[4];
        String versionHex = params[5].as<String>();
        String nbitsHex = params[6].as<String>();
        ntimeHex = params[7].as<String>();

        if (!extranonce2_size) extranonce2_size = 4;
        extranonce2++;
        formatExtranonce2LE((uint64_t)extranonce2, (uint8_t)extranonce2_size, en2Hex, sizeof(en2Hex));

        String coinbase = coinb1Hex + extranonce1 + String(en2Hex) + coinb2Hex;
        int cbLen = coinbase.length() / 2;
        if (cbLen <= 0 || cbLen > 512) continue;

        uint8_t cbBytes[512];
        hexStringToBytes(coinbase, cbBytes);

        // v10.6 exact NerdMiner Merkle pipeline:
        // - coinbase hash is SHA256d bytes as produced by mbedtls
        // - merkle branches are used directly from pool hex
        // - intermediate mRoot is NOT reversed between branches
        // - final mRoot is copied into block header without reverse
        uint8_t mRoot[32];
        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, cbBytes, cbLen); mbedtls_sha256_finish(&ctx, mRoot);
        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, mRoot, 32); mbedtls_sha256_finish(&ctx, mRoot);

        for (JsonVariant v : merkleBranch) {
          String branchHex = v.as<String>();
          if (branchHex.length() != 64) continue;
          uint8_t branchBytes[32];
          hexStringToBytes(branchHex, branchBytes);

          uint8_t concat[64];
          memcpy(concat, mRoot, 32);
          memcpy(concat + 32, branchBytes, 32);

          mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, concat, 64); mbedtls_sha256_finish(&ctx, mRoot);
          mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, mRoot, 32); mbedtls_sha256_finish(&ctx, mRoot);
        }

        memset(blockHeader, 0, 80);
        hexStringToBytes(versionHex, blockHeader); reverse_bytes(blockHeader, 4);

        // v10.11H: exact NerdMiner header layout from calculateMiningData().
        // Build: version + prevhash + merkle_root + ntime + nbits + nonce.
        // Then reverse version, word-swap prevhash, DO NOT reverse merkle,
        // reverse ntime and nbits; nonce is written as uint32 little-endian.
        hexStringToBytes(prevhashHex, blockHeader + 4);
        reverse_word_bytes(blockHeader + 4, 32);

        memcpy(blockHeader + 36, mRoot, 32);

        uint8_t ntimeLE[4];
        uint8_t nbitsLE[4];
        hexStringToBytes(ntimeHex, ntimeLE); reverse_bytes(ntimeLE, 4);
        hexStringToBytes(nbitsHex, nbitsLE); reverse_bytes(nbitsLE, 4);
        memcpy(blockHeader + 68, ntimeLE, 4);
        memcpy(blockHeader + 72, nbitsLE, 4);

        nonce = esp_random();
        jobReady = true;
        minerLocalFallback = false;
        minerLastJobMs = millis();
        strlcpy(minerStatus, "HASH", sizeof(minerStatus));

        // Mirror exact current pool work for JANUS colony workers.
        mirrorColonyJobForWorkers(currentJobId, blockHeader, extranonce2, en2Hex, ntimeHex);

        Serial.printf("[MINER] Job %s ready, colony job mirrored en2=%s size=%u\n",
                      currentJobId.c_str(), en2Hex, (unsigned)extranonce2_size);
        janusSdLogf("JOB", "id=%s targetBits=%u en2=%s en2size=%u", currentJobId.c_str(), minerShareTargetBits, en2Hex, (unsigned)extranonce2_size);
      }

      if (doc["id"].is<int>() && doc["id"].as<int>() == 4) {
        if (doc["result"] == true) {
          minerShares++;
          janusFarmMarkDirty();
          minerLastAcceptMs = millis();
          janusShareLedFlare(2);  // accepted share: larger bonfire burst
          strlcpy(minerStatus, "ACCEPT", sizeof(minerStatus));
          strlcpy(minerLastRejectReason, "-", sizeof(minerLastRejectReason));
          janusSdLogf("ACCEPT", "shares=%lu rejects=%lu nonce=%s", (unsigned long)minerShares, (unsigned long)minerSubmitRejects, minerLastSubmitNonce);
          Serial.printf("[MINER] ACCEPT shares=%lu rejects=%lu nonce=%s\n",
                        (unsigned long)minerShares, (unsigned long)minerSubmitRejects, minerLastSubmitNonce);
        } else {
          minerSubmitRejects++;
          janusFarmMarkDirty();
          strlcpy(minerStatus, "REJECT", sizeof(minerStatus));
          const char* errText = "unknown";
          if (doc["error"].is<const char*>()) {
            errText = doc["error"].as<const char*>();
          } else if (doc["error"].is<JsonArray>() && !doc["error"][1].isNull()) {
            errText = doc["error"][1] | "unknown";
          }
          minerClassifyReject(errText);
          janusSdLogf("REJECT", "attempts=%lu rejects=%lu nonce=%s err=%s low=%lu stale=%lu other=%lu", (unsigned long)minerSubmitAttempts, (unsigned long)minerSubmitRejects, minerLastSubmitNonce, errText, (unsigned long)minerLowDiffRejects, (unsigned long)minerStaleJobRejects, (unsigned long)minerOtherRejects);
          Serial.printf("[MINER] REJECT attempts=%lu rejects=%lu nonce=%s err=%s low=%lu stale=%lu other=%lu\n",
                        (unsigned long)minerSubmitAttempts,
                        (unsigned long)minerSubmitRejects,
                        minerLastSubmitNonce,
                        errText,
                        (unsigned long)minerLowDiffRejects,
                        (unsigned long)minerStaleJobRejects,
                        (unsigned long)minerOtherRejects);
        }
      }
    }



    // v10.11H: after reconnect, wait longer for pool handshake.
    if (client.connected() && stratumConnected && (!authorized || extranonce1.length() == 0) &&
        millis() - connectedAtMs > 25000UL) {
      poolReconnectFails++;
      poolReconnectHoldUntilMs = millis() + 15000UL;
      Serial.println("[MINER] no stratum subscribe/auth response for 25s -> 15s cooldown reconnect");
      janusSdLogf("RESUB", "handshake timeout 25s fail=%u", poolReconnectFails);
      strlcpy(minerStatus, "RESUB", sizeof(minerStatus));
      client.stop();
      stratumConnected = false;
      jobReady = false;
      colonyClearMasterJob("handshake-timeout");
      authorized = false;
      extranonce1 = "";
      if (poolReconnectFails >= 12) {
        Serial.println("[MINER] handshake reconnect storm -> deep sleep 30s");
        janusSdLogf("SLEEP", "handshake storm 30s");
        esp_sleep_enable_timer_wakeup(30ULL * 1000ULL * 1000ULL);
        esp_deep_sleep_start();
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    // v36.8 miner sanity: if we submit/reject for a long time without a single accept,
    // reconnect Stratum and request fresh extranonce/difficulty.
    if (client.connected() && authorized && minerSubmitAttempts >= 3 && minerLastAcceptMs == 0 &&
        minerLastSubmitMs > 0 && millis() - minerLastSubmitMs > 25UL * 60UL * 1000UL) {
      Serial.println("[MINER] no accepts after submits for 25m -> reconnect stratum");
      strlcpy(minerStatus, "RECONN", sizeof(minerStatus));
      client.stop();
      jobReady = false;
      colonyClearMasterJob("no-accept-reconnect");
      authorized = false;
      minerSubmitAttempts = 0;
      minerSubmitRejects = 0;
      vTaskDelay(pdMS_TO_TICKS(800));
      continue;
    }

    // Relay shares found by voluntary ESP-NOW workers.
    // v40: verify every remote nonce locally before Stratum submit. This stops the old
    // "bits=0 / tail=00000000" legacy replies from becoming Difficulty-too-low rejects.
    if (client.connected() && authorized && jobReady && colonyRemoteShareQueue) {
      static uint32_t recentRemoteNonces[24] = {0};
      static uint8_t recentRemotePos = 0;
      static String recentRemoteJob = "";
      if (recentRemoteJob != currentJobId) {
        memset(recentRemoteNonces, 0, sizeof(recentRemoteNonces));
        recentRemotePos = 0;
        recentRemoteJob = currentJobId;
      }

      RemoteShareItem remote{};
      uint8_t relayed = 0;
      while (relayed < 5 && xQueueReceive(colonyRemoteShareQueue, &remote, 0) == pdTRUE) {
        relayed++;

        bool duplicate = false;
        for (uint8_t i = 0; i < 24; i++) {
          if (recentRemoteNonces[i] == remote.share.nonce) { duplicate = true; break; }
        }
        if (duplicate) {
          colonyRemoteSharesDropped++;
          colonyRemoteDuplicateDrops++;
          continue;
        }
        recentRemoteNonces[recentRemotePos] = remote.share.nonce;
        recentRemotePos = (recentRemotePos + 1) % 24;

        uint8_t remoteHeader[80];
        uint8_t remoteHash1[32];
        uint8_t remoteHash2[32];
        memcpy(remoteHeader, remote.header, 80);
        writeLE32(remoteHeader + 76, remote.share.nonce);
        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, remoteHeader, 80); mbedtls_sha256_finish(&ctx, remoteHash1);
        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, remoteHash1, 32); mbedtls_sha256_finish(&ctx, remoteHash2);
        uint8_t remoteShareHash[32];
        hashToShareOrder(remoteHash2, remoteShareHash);
        uint16_t verifiedBits = countLeadingZeroBits(remoteShareHash);
        uint16_t remoteTargetBits = remote.targetBits ? remote.targetBits : countLeadingZeroBits(remote.target);
        bool remoteTargetOk = (remoteTargetBits > 0) &&
                              (verifiedBits >= remoteTargetBits) &&
                              hashMeetsTarget(remoteShareHash, remote.target);
        if (verifiedBits > colonyBestPeerBits) colonyBestPeerBits = verifiedBits;
        if (verifiedBits > minerBestBits) minerBestBits = verifiedBits;

        remote.bits = verifiedBits;
        memcpy(remote.hash_tail, remoteShareHash + 28, 4);

        if (!remoteTargetOk || verifiedBits < JANUS_NERDMINER_MERKLE_MIN_SHARE_BITS) {
          colonyRemoteSharesDropped++;
          colonyRemoteWeakDrops++;
          if ((colonyRemoteWeakDrops % 16UL) == 1UL) {
            Serial.printf("[COLONY] drop weak remote worker=%u nonce=%08lx bits=%u need=%u rssi=%d weakDrops=%lu legacy=%lu\n",
                          remote.share.worker_id,
                          (unsigned long)remote.share.nonce,
                          (unsigned)verifiedBits,
                          (unsigned)remoteTargetBits,
                          remote.rssi,
                          (unsigned long)colonyRemoteWeakDrops,
                          (unsigned long)colonyRemoteLegacySeen);
          }
          continue;
        }

        if (remote.jobIdText[0] == '\0' || remote.en2Hex[0] == '\0' || remote.ntimeHex[0] == '\0') {
          colonyRemoteSharesDropped++;
          continue;
        }

        char n_hex[9];
        snprintf(n_hex, sizeof(n_hex), "%08lx", (unsigned long)remote.share.nonce);
        String submitMsg = String("{\"id\":4,\"method\":\"mining.submit\",\"params\":[\"") +
                           minerUserString() + "\",\"" + String(remote.jobIdText) + "\",\"" +
                           String(remote.en2Hex) + "\",\"" + String(remote.ntimeHex) + "\",\"" + String(n_hex) + "\"]}";
        client.println(submitMsg);
        strlcpy(minerLastSubmitNonce, n_hex, sizeof(minerLastSubmitNonce));
        minerSubmitAttempts++;
        minerRemoteSubmitAttempts++;
        janusFarmMarkDirty();
        minerLastSubmitMs = millis();
        janusShareLedFlare(1);  // remote verified share ticket
        Serial.printf("[COLONY] relay VERIFIED worker=%u nonce=%s bits=%u target=%u rssi=%d job=%s tail=%02x%02x%02x%02x\n",
                      remote.share.worker_id, n_hex, (unsigned)verifiedBits, (unsigned)remoteTargetBits,
                      remote.rssi, remote.jobIdText,
                      remote.hash_tail[0], remote.hash_tail[1], remote.hash_tail[2], remote.hash_tail[3]);
      }
    }

    if (jobReady && client.connected() && authorized) {
      uint16_t activeBatch = effectiveMiningBatch();
      for(int batch = 0; batch < (int)activeBatch; batch++) {
        nonce++;
        writeLE32(blockHeader + 76, nonce);

        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, blockHeader, 80); mbedtls_sha256_finish(&ctx, hash1);
        mbedtls_sha256_starts(&ctx, 0); mbedtls_sha256_update(&ctx, hash1, 32); mbedtls_sha256_finish(&ctx, hash2);
        hashesThisSecond++;
        minerTotalHashes++;

        // v10.11H: pool-compatible local share check.
        // SHA256d digest is converted to Bitcoin display/share order first,
        // then compared against the big-endian Stratum target.
        uint8_t shareHash[32];
        hashToShareOrder(hash2, shareHash);
        uint16_t bits = countLeadingZeroBits(shareHash);
        bool targetOk = (bits >= minerShareTargetBits) && hashMeetsShareTarget(shareHash);

        if (bits > minerBestBits) {
          minerBestBits = bits;
          if (bits >= 20) {
            Serial.printf("[MINER] BEST bits=%u targetBits=%u targetOk=%d nonce=%08lx\n",
                          bits, minerShareTargetBits, targetOk ? 1 : 0, (unsigned long)nonce);
            janusSdLogf("BEST", "bits=%u targetBits=%u targetOk=%d nonce=%08lx",
                        (unsigned)bits, (unsigned)minerShareTargetBits, targetOk ? 1 : 0, (unsigned long)nonce);
          }
        }

        // v10.11H: strict byte target gate only.
        // Do NOT use janusDiffFromHashLE() for submitting: v10.5 proved it can
        // produce false positives (bits=0 but shareDiff>poolDiff), causing Above target.
        if (targetOk) {
          minerShareCandidates++;
          minerLastCandidateMs = millis();
          minerLastCandidateBits = bits;
          janusShareLedFlare(1);  // share candidate found: throw fuel into the LED fire
          strlcpy(minerStatus, "SUBMIT", sizeof(minerStatus));
          char n_hex[9];
          snprintf(n_hex, sizeof(n_hex), "%08lx", (unsigned long)nonce);
          String submitMsg = String("{\"id\":4,\"method\":\"mining.submit\",\"params\":[\"") +
                             minerUserString() + "\",\"" + currentJobId + "\",\"" +
                             String(en2Hex) + "\",\"" + ntimeHex + "\",\"" + String(n_hex) + "\"]}";
          client.println(submitMsg);
          strlcpy(minerLastSubmitNonce, n_hex, sizeof(minerLastSubmitNonce));
          minerSubmitAttempts++;
          janusFarmMarkDirty();
          minerLastSubmitMs = millis();
          janusSdLogf("LOCAL_SUBMIT", "strictTarget localTickets=%lu submit=%lu nonce=%s bits=%u targetBits=%u H=%lu job=%s", (unsigned long)minerShareCandidates, (unsigned long)minerSubmitAttempts, minerLastSubmitNonce, (unsigned)bits, (unsigned)minerShareTargetBits, (unsigned long)minerRealHashrate, currentJobId.c_str());
          Serial.printf("[MINER] LOCAL_SUBMIT ticket=%lu submit=%lu nonce=%s bits=%u targetBits=%u H=%lu\n",
                        (unsigned long)minerShareCandidates, (unsigned long)minerSubmitAttempts, n_hex, bits, minerShareTargetBits, (unsigned long)minerRealHashrate);
          break;
        }
      }
    } else {
      // v10.11H: no SELF/local fallback lottery.
      // If there is no authorized pool job, Buzz waits and keeps all share counters honest.
      minerLocalFallback = false;
      minerLocalHashrate = 0;
      localHashesThisSecond = 0;
      if (WiFi.status() == WL_CONNECTED && client.connected()) {
        strlcpy(minerStatus, authorized ? "WAIT_JOB" : "AUTH", sizeof(minerStatus));
      }
      vTaskDelay(pdMS_TO_TICKS(25));
    }

    uint32_t now = millis();
    if (now - lastHashTick >= 1000) {
      minerRealHashrate = hashesThisSecond;
      minerLocalHashrate = 0;  // v10.11H: SELF/local fallback disabled
      hashesThisSecond = 0;
      localHashesThisSecond = 0;
      lastHashTick = now;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

// ============================================================
// UTILS & LCD SETUP
// ============================================================
uint32_t janusRand() { return esp_random(); }

uint32_t janusMixedRandom32() {
  uint32_t x = esp_random();
  x ^= micros();
  x ^= millis() * 0x9E3779B9UL;
  x ^= (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFULL);
  x ^= ((uint32_t)WiFi.RSSI() << 24);
#if BATTERY_ADC_PIN >= 0
  x ^= ((uint32_t)analogRead(BATTERY_ADC_PIN) << 11);
#endif
  x ^= janusBootEntropySalt;
  x ^= x >> 16; x *= 0x7feb352dU;
  x ^= x >> 15; x *= 0x846ca68bU;
  x ^= x >> 16;
  janusBootEntropySalt ^= x + 0x9E3779B9UL + (janusBootEntropySalt << 6) + (janusBootEntropySalt >> 2);
  return x;
}

// Small deterministic helper for scene FX.
// Core2-style anti-flicker idea: animation must be time-driven and stable inside a short frame bucket,
// not fully random on every redraw.
uint32_t stableMix32(uint32_t x) {
  x ^= x >> 16; x *= 0x7feb352dU;
  x ^= x >> 15; x *= 0x846ca68bU;
  x ^= x >> 16;
  return x;
}

uint8_t stableNoise8(uint32_t seed) {
  return (uint8_t)(stableMix32(seed) & 0xFF);
}

bool stableBlink(uint32_t seed, uint32_t bucketMs, uint8_t threshold) {
  uint32_t bucket = millis() / (bucketMs ? bucketMs : 1U);
  return stableNoise8(seed ^ bucket) < threshold;
}

int stableRange(int minV, int maxV, uint32_t seed, uint32_t bucketMs) {
  if (maxV <= minV) return minV;
  uint32_t bucket = millis() / (bucketMs ? bucketMs : 1U);
  return minV + (stableNoise8(seed ^ bucket) % (uint32_t)(maxV - minV + 1));
}

bool i2cWrite(uint8_t a, uint8_t r, uint8_t v) { Wire.beginTransmission(a); Wire.write(r); Wire.write(v); return Wire.endTransmission() == 0; }
uint16_t tcaRead(uint8_t reg) { Wire.beginTransmission(TCA_ADDR); Wire.write(reg); if (Wire.endTransmission(false) != 0) return 0xFFFF; if (Wire.requestFrom(TCA_ADDR, (uint8_t)2) != 2) return 0xFFFF; return Wire.read() | (Wire.read() << 8); }
bool tcaWrite(uint8_t reg, uint16_t val) { Wire.beginTransmission(TCA_ADDR); Wire.write(reg); Wire.write(val & 0xFF); Wire.write((val >> 8) & 0xFF); return Wire.endTransmission() == 0; }
void tcaApply() { tcaWrite(0x02, tcaOutput); tcaWrite(0x06, tcaConfig); }
void tcaPinMode(uint8_t p, bool input) { if (input) tcaConfig |= (1 << p); else tcaConfig &= ~(1 << p); tcaApply(); }
void tcaWritePin(uint8_t p, bool high) { if (high) tcaOutput |= (1 << p); else tcaOutput &= ~(1 << p); tcaApply(); }
bool tcaReadPin(uint8_t p) { return tcaRead(0x00) & (1 << p); }

void setupTCA() {
  tcaConfig = 0xFFFF; tcaOutput = 0xFFFF; tcaApply(); delay(20);
  tcaPinMode(EXIO_LCD_RST, false); tcaWritePin(EXIO_LCD_RST, true);
  tcaPinMode(EXIO_AUDIO_PA, false); tcaWritePin(EXIO_AUDIO_PA, true);
  tcaPinMode(EXIO_KEY1, true); tcaPinMode(EXIO_KEY2, true); tcaPinMode(EXIO_KEY3, true);
}

void es8311Write(uint8_t reg, uint8_t val) {
  i2cWrite(ES8311_ADDR, reg, val);
  delay(2);
}

void initES8311() {
  Wire.beginTransmission(ES8311_ADDR);
  if (Wire.endTransmission() != 0) {
    Serial.println("[ES8311] not found at 0x18");
    return;
  }

  // Full old-working ES8311 sequence from Buuzz_0.1.
  // The later PCM experiments used a shortened init and could write I2S bytes
  // while the analog DAC/output path stayed silent on some boots.
  Serial.println("[ES8311] full old init");
  es8311Write(0x00, 0x1F); delay(20);
  es8311Write(0x00, 0x80); delay(20);
  es8311Write(0x01, 0x3F);
  es8311Write(0x02, 0x00);
  es8311Write(0x03, 0x10);
  es8311Write(0x04, 0x10);
  es8311Write(0x05, 0x00);
  es8311Write(0x06, 0x00);
  es8311Write(0x07, 0x00);
  es8311Write(0x08, 0xFF);
  es8311Write(0x09, 0x0C);
  es8311Write(0x0A, 0x0C);
  es8311Write(0x0B, 0x00);
  es8311Write(0x0C, 0x00);
  es8311Write(0x10, 0x00);
  es8311Write(0x11, 0x7F);
  es8311Write(0x12, 0x00);
  es8311Write(0x13, 0x10);
  es8311Write(0x14, 0x1A);
  es8311Write(0x31, 0x00);
  es8311Write(0x32, 0xBF);
  tcaWritePin(EXIO_AUDIO_PA, true);
  Serial.println("[ES8311] ready full old init");
}


void audioPowerWake() {
  Serial.println("[Audio] audioPowerWake full old init");
  setupTCA();
  tcaWritePin(EXIO_AUDIO_PA, false);
  delay(90);
  tcaWritePin(EXIO_AUDIO_PA, true);
  delay(140);
  initES8311();
  tcaWritePin(EXIO_AUDIO_PA, true);
}



int readBatteryRawAdc() {
  #if BATTERY_ADC_PIN >= 0
    return analogRead(BATTERY_ADC_PIN);
  #else
    return 4095;
  #endif
}

int readBatteryPinMv() {
  #if BATTERY_ADC_PIN >= 0
    return analogReadMilliVolts(BATTERY_ADC_PIN);
  #else
    return 4200;
  #endif
}

int readBatteryClamped() {
  #if BATTERY_ADC_PIN >= 0
    // Board-specific divider varies, so keep the old proven ADC-range model,
    // but smooth it and show exact raw/mV via serial 'B' for calibration.
    static float smoothed = -1;
    int raw = readBatteryRawAdc();
    if (smoothed < 0) smoothed = raw;
    smoothed = smoothed * 0.94f + raw * 0.06f;

    // USB/charger tends to pin the ADC high on this board.
    if (smoothed > 3300) return 100;

    int pct = map((int)smoothed, 2050, 3180, 0, 100);
    return constrain(pct, 0, 100);
  #else
    return 100;
  #endif
}

void updateLcdBrightness() {
  if (ledBright == 0) {
    ledcWrite(LCD_BL, 0);
    return;
  }
  int pwm = map(ledBright, 1, 100, 1, 255);
  ledcWrite(LCD_BL, constrain(pwm, 0, 255)); 
}


float janusReadChipTempC() {
#if defined(ARDUINO_ARCH_ESP32)
  float t = temperatureRead();
  if (isnan(t) || t < -40.0f || t > 140.0f) return NAN;
  return t;
#else
  return NAN;
#endif
}

int janusReadPortNtcRaw() {
#if JANUS_PORT_NTC_PIN >= 0
  return analogRead(JANUS_PORT_NTC_PIN);
#else
  return -1;
#endif
}

void janusCutChargeOutput(bool cut) {
#if JANUS_CHARGE_CUT_PIN >= 0
  pinMode(JANUS_CHARGE_CUT_PIN, OUTPUT);
  digitalWrite(JANUS_CHARGE_CUT_PIN,
               cut ? JANUS_CHARGE_CUT_ACTIVE_LEVEL : !JANUS_CHARGE_CUT_ACTIVE_LEVEL);
#else
  (void)cut;
#endif
}

void janusEnterThermalStop(const char* reason) {
  if (!janusThermalStop) {
    janusThermalStop = true;
    janusThermalSavedBright = ledBright;
    janusThermalDimmed = true;
    Serial.printf("[THERMAL] STOP reason=%s chip=%.1fC portRaw=%d\n",
                  reason ? reason : "-", janusChipTempC, janusPortTempRaw);
    janusSdLogf("THERMAL", "STOP reason=%s chip=%.1f portRaw=%d",
                reason ? reason : "-", janusChipTempC, janusPortTempRaw);
  }

  strlcpy(minerStatus, "THERMAL", sizeof(minerStatus));
  minerRealHashrate = 0;
  minerLocalHashrate = 0;
  colonyClearMasterJob("thermal-stop");

  if (ledBright > 8) ledBright = 8;
  updateLcdBrightness();
  strip.clear();
  strip.show();

  tcaWritePin(EXIO_AUDIO_PA, false);
  if (wanted || playing || softPaused) {
    audio.stopSong();
    wanted = false;
    playing = false;
    softPaused = false;
    audioUserPaused = false;
  }
  janusCutChargeOutput(true);
}

void janusExitThermalStop() {
  if (!janusThermalStop) return;
  janusThermalStop = false;
  janusCutChargeOutput(false);

  if (janusThermalDimmed) {
    ledBright = janusThermalSavedBright;
    updateLcdBrightness();
    janusThermalDimmed = false;
  }

  strlcpy(minerStatus, "COOL", sizeof(minerStatus));
  Serial.printf("[THERMAL] RESUME chip=%.1fC portRaw=%d\n", janusChipTempC, janusPortTempRaw);
  janusSdLogf("THERMAL", "RESUME chip=%.1f portRaw=%d", janusChipTempC, janusPortTempRaw);
}

void janusThermalTick(uint32_t now) {
#if JANUS_SAFE_CHARGE_MODE
  if (now - janusThermalLastMs < JANUS_THERMAL_CHECK_MS) return;
  janusThermalLastMs = now;

  janusChipTempC = janusReadChipTempC();
  janusPortTempRaw = janusReadPortNtcRaw();

  bool chipValid = !isnan(janusChipTempC);
  bool chipHot = chipValid && janusChipTempC >= JANUS_THERMAL_CUTOFF_C;
  bool chipWarn = chipValid && janusChipTempC >= JANUS_THERMAL_WARN_C;
  bool chipSleep = chipValid && janusChipTempC >= JANUS_THERMAL_SLEEP_C;
  bool portHot = (janusPortTempRaw >= JANUS_PORT_NTC_HOT_RAW && janusPortTempRaw >= 0);

  if (chipWarn && now - janusThermalLastLogMs > 10000UL) {
    janusThermalLastLogMs = now;
    Serial.printf("[THERMAL] warm chip=%.1fC cutoff=%.1fC portRaw=%d\n",
                  janusChipTempC, JANUS_THERMAL_CUTOFF_C, janusPortTempRaw);
  }

  if (chipHot || portHot) {
    janusEnterThermalStop(chipHot ? "chip-hot" : "port-ntc-hot");
    if (chipSleep) {
      Serial.printf("[THERMAL] CRITICAL %.1fC -> deep sleep 120s\n", janusChipTempC);
      janusSdLogf("THERMAL", "CRITICAL chip=%.1f sleep=120s", janusChipTempC);
      delay(100);
      esp_sleep_enable_timer_wakeup(120ULL * 1000ULL * 1000ULL);
      esp_deep_sleep_start();
    }
    return;
  }

  if (janusThermalStop && chipValid && janusChipTempC <= JANUS_THERMAL_RESUME_C) {
    janusExitThermalStop();
  }
#else
  (void)now;
#endif
}

uint32_t janusWifiBackoffMs() {
  uint32_t backoff = JANUS_WIFI_RECONNECT_BASE_MS;
  uint8_t loops = (janusWifiFailLevel > 5) ? 5 : janusWifiFailLevel;
  for (uint8_t i = 0; i < loops; i++) backoff *= 2;
  if (backoff > JANUS_WIFI_RECONNECT_MAX_MS) backoff = JANUS_WIFI_RECONNECT_MAX_MS;
  return backoff;
}

void janusWifiKickGuarded(const char* reason) {
  uint32_t now = millis();
  if (now < janusWifiNextKickMs) return;

  if (janusWifiStormSinceMs == 0 || now - janusWifiStormSinceMs > JANUS_WIFI_STORM_WINDOW_MS) {
    janusWifiStormSinceMs = now;
    janusWifiStormCount = 0;
  }
  janusWifiStormCount++;

  if (janusWifiStormCount >= JANUS_WIFI_STORM_LIMIT) {
    Serial.printf("[WIFI] reconnect storm -> radio cooldown %lums reason=%s\n",
                  (unsigned long)JANUS_WIFI_RADIO_COOLDOWN_MS, reason ? reason : "-");
    janusSdLogf("WIFI", "storm cooldown=%lu reason=%s",
                (unsigned long)JANUS_WIFI_RADIO_COOLDOWN_MS, reason ? reason : "-");
#if JANUS_COLONY_ENABLE
    esp_now_deinit();
    janusColonyEspNowActive = false;
    colonyPeerChannel = 0;
#endif
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    janusWifiStormCount = 0;
    if (janusWifiFailLevel < 7) janusWifiFailLevel++;
    janusWifiNextKickMs = now + JANUS_WIFI_RADIO_COOLDOWN_MS;
    strlcpy(minerStatus, "WIFI_COOL", sizeof(minerStatus));
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(false, false);
  delay(50);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  minerWifiReconnects++;
  if (janusWifiFailLevel < 7) janusWifiFailLevel++;
  uint32_t backoff = janusWifiBackoffMs();
  janusWifiNextKickMs = now + backoff;
  Serial.printf("[WIFI] guarded reconnect #%lu backoff=%lums storm=%u reason=%s\n",
                (unsigned long)minerWifiReconnects,
                (unsigned long)backoff,
                (unsigned)janusWifiStormCount,
                reason ? reason : "-");
}

void janusWifiRecovered() {
  if (janusWifiFailLevel || janusWifiStormCount) {
    Serial.printf("[WIFI] recovered rssi=%d channel=%u\n", WiFi.RSSI(), currentWifiChannel());
  }
  janusWifiFailLevel = 0;
  janusWifiStormCount = 0;
  janusWifiStormSinceMs = 0;
  janusWifiNextKickMs = 0;
#if JANUS_COLONY_ENABLE
  if (!janusColonyEspNowActive) initColonyNow();
#endif
}

uint16_t getThemeColor() {
  switch (ledPalette % LED_PALETTE_COUNT) {
    case 0: return C_AMBER2;   
    case 1: return C_GREEN;    
    case 2: return C_PURPLE;   
    case 3: return C_CYAN;     
    case 4: return C_ORANGE;   
    case 5: return C_BLUE_DIM; 
    case 6: return C_RED;      
    case 7: return 0x07E0;     
    default: return C_AMBER;
  }
}

uint16_t blend565(uint16_t a, uint16_t b, uint8_t amount) {
  // amount: 0 = a, 255 = b
  uint8_t ar = ((a >> 11) & 0x1F) << 3;
  uint8_t ag = ((a >> 5) & 0x3F) << 2;
  uint8_t ab = (a & 0x1F) << 3;

  uint8_t br = ((b >> 11) & 0x1F) << 3;
  uint8_t bg = ((b >> 5) & 0x3F) << 2;
  uint8_t bb = (b & 0x1F) << 3;

  uint8_t r = (uint8_t)(((uint16_t)ar * (255 - amount) + (uint16_t)br * amount) / 255);
  uint8_t g = (uint8_t)(((uint16_t)ag * (255 - amount) + (uint16_t)bg * amount) / 255);
  uint8_t bl = (uint8_t)(((uint16_t)ab * (255 - amount) + (uint16_t)bb * amount) / 255);

  return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (bl >> 3));
}

uint8_t janusShareUiGlow(uint32_t now) {
  // v10.11H:
  // UI glow is tied to the same ACCEPT flare that drives the LED bonfire.
  // Candidate/ticket flares do not color SHAR, because SHAR is real pool ACCEPT only.
  uint32_t until = ledShareFlareUntilMs;
  uint8_t kind = ledShareFlareKind;
  if (kind < 2 || until <= now) return 0;

  uint32_t left = until - now;       // 0..1800 for accepted share
  if (left > 1560UL) return 255;     // strong initial flare, like wood thrown into fire
  return (uint8_t)constrain((int)((left * 255UL) / 1560UL), 0, 255);
}

uint16_t janusShareUiColor(uint16_t baseColor, uint16_t paletteColor, uint32_t now) {
  uint8_t glow = janusShareUiGlow(now);
  if (!glow) return baseColor;
  return blend565(baseColor, paletteColor, glow);
}

void lcdCmd(uint8_t c) { digitalWrite(LCD_DC, LOW); digitalWrite(LCD_CS, LOW); lcdSpi.transfer(c); digitalWrite(LCD_CS, HIGH); }
void lcdData(uint8_t d) { digitalWrite(LCD_DC, HIGH); digitalWrite(LCD_CS, LOW); lcdSpi.transfer(d); digitalWrite(LCD_CS, HIGH); }
void lcdSetWindow(int x, int y, int w, int h) {
  lcdCmd(0x2A); lcdData(x>>8); lcdData(x&0xFF); lcdData((x+w-1)>>8); lcdData((x+w-1)&0xFF);
  lcdCmd(0x2B); lcdData(y>>8); lcdData(y&0xFF); lcdData((y+h-1)>>8); lcdData((y+h-1)&0xFF);
  lcdCmd(0x2C);
}

void lcdFillRect(int x, int y, int w, int h, uint16_t c) {
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (w <= 0 || h <= 0) return;
  if (x >= 240 || y >= 320) return;
  if (x + w > 240) w = 240 - x; 
  if (y + h > 320) h = 320 - y;

  lcdSetWindow(x,y,w,h);
  static uint8_t buf[480];
  for(int i=0;i<w;i++){ buf[i*2]=c>>8; buf[i*2+1]=c&0xFF; }
  digitalWrite(LCD_DC,HIGH); digitalWrite(LCD_CS,LOW);
  for(int yy=0;yy<h;yy++) lcdSpi.writeBytes(buf,w*2);
  digitalWrite(LCD_CS,HIGH);
}

void fillRectClip(int x, int y, int w, int h, uint16_t c, int cx, int cy, int cw, int ch) {
  int x1 = max(x, cx); int y1 = max(y, cy);
  int x2 = min(x + w, cx + cw); int y2 = min(y + h, cy + ch);
  if (x1 < x2 && y1 < y2) lcdFillRect(x1, y1, x2 - x1, y2 - y1, c);
}

void lcdInit() {
  pinMode(LCD_CS,OUTPUT); pinMode(LCD_DC,OUTPUT); pinMode(LCD_BL,OUTPUT);
  digitalWrite(LCD_CS,HIGH); digitalWrite(LCD_DC,HIGH);
  ledcAttach(LCD_BL,5000,8); updateLcdBrightness();
  tcaWritePin(EXIO_LCD_RST, false); delay(150); tcaWritePin(EXIO_LCD_RST, true); delay(300);
  lcdSpi.begin(LCD_SCK, LCD_MISO, LCD_MOSI, LCD_CS);
  lcdSpi.setFrequency(40000000); lcdSpi.setDataMode(SPI_MODE0);
  lcdCmd(0x01); delay(150); lcdCmd(0x11); delay(150);
  lcdCmd(0x3A); lcdData(0x55); lcdCmd(0x36); lcdData(0x00);
  lcdCmd(0x21); delay(10); lcdCmd(0x13); delay(10); lcdCmd(0x29); delay(100);
}

void lcdDigit(int x, int y, uint8_t d, uint16_t color) {
  const uint8_t segs[10] = { 0b1111110,0b0110000,0b1101101,0b1111001,0b0110011,0b1011011,0b1011111,0b1110000,0b1111111,0b1111011 };
  uint8_t s = segs[d%10];
  if(s&0b1000000) lcdFillRect(x+2,y,10,2,color); if(s&0b0100000) lcdFillRect(x+12,y+2,2,10,color);
  if(s&0b0010000) lcdFillRect(x+12,y+14,2,10,color); if(s&0b0001000) lcdFillRect(x+2,y+24,10,2,color);
  if(s&0b0000100) lcdFillRect(x,y+14,2,10,color); if(s&0b0000010) lcdFillRect(x,y+2,2,10,color);
  if(s&0b0000001) lcdFillRect(x+2,y+12,10,2,color);
}
void lcdNumber(int x, int y, uint32_t value, uint16_t color) {
  char buf[16]; snprintf(buf, sizeof(buf), "%lu", (unsigned long)value);
  for (int i = 0; buf[i]; i++) { if (buf[i] >= '0' && buf[i] <= '9') lcdDigit(x + i*16, y, buf[i]-'0', color); }
}


void formatCompact(uint64_t value, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  if (value < 1000ULL) {
    snprintf(out, outSize, "%lu", (unsigned long)value);
  } else if (value < 1000000ULL) {
    uint32_t whole = value / 1000ULL;
    uint32_t frac = (value % 1000ULL) / 100ULL;
    if (whole < 10 && frac > 0) snprintf(out, outSize, "%lu.%luK", (unsigned long)whole, (unsigned long)frac);
    else snprintf(out, outSize, "%luK", (unsigned long)whole);
  } else {
    uint32_t whole = value / 1000000ULL;
    uint32_t frac = (value % 1000000ULL) / 100000ULL;
    if (whole < 10 && frac > 0) snprintf(out, outSize, "%lu.%luKK", (unsigned long)whole, (unsigned long)frac);
    else snprintf(out, outSize, "%luKK", (unsigned long)whole);
  }
}

void tinyText(int x, int y, const char* s, uint16_t color);
void tinyTextClipped(int x, int y, const char* s, uint16_t color, int cx, int cy, int cw, int ch);
void tinyCompact(int x, int y, uint64_t value, uint16_t color=C_LOW) {
  char b[12];
  formatCompact(value, b, sizeof(b));
  tinyText(x, y, b, color);
}

void tinyCompactClipped(int x, int y, uint64_t value, uint16_t color, int cx, int cy, int cw, int ch) {
  char b[12];
  formatCompact(value, b, sizeof(b));
  tinyTextClipped(x, y, b, color, cx, cy, cw, ch);
}

void hashToShareOrder(const uint8_t in[32], uint8_t out[32]) {
  for (int i = 0; i < 32; ++i) out[i] = in[31 - i];
}

uint16_t tinyPattern(char c) {
  switch (c) {
    case 'A': return 0b111101111101101; case 'B': return 0b110101110101110; case 'C': return 0b111100100100111;
    case 'D': return 0b110101101101110; case 'E': return 0b111100110100111; case 'F': return 0b111100110100100;
    case 'G': return 0b111100101101111; case 'H': return 0b101101111101101; case 'I': return 0b111010010010111;
    case 'L': return 0b100100100100111; case 'M': return 0b101111111101101; case 'N': return 0b101111111111101;
    case 'O': return 0b111101101101111; case 'P': return 0b111101111100100; case 'R': return 0b111101111110101;
    case 'S': return 0b111100111001111; case 'T': return 0b111010010010010; case 'U': return 0b101101101101111;
    case 'V': return 0b101101101101010; case 'X': return 0b101101010101101; case 'Z': return 0b111001010100111;
    case 'K': return 0b101101110101101;
    case '0': return 0b111101101101111; case '1': return 0b010110010010111; case '2': return 0b111001111100111;
    case '3': return 0b111001111001111; case '4': return 0b101101111001001; case '5': return 0b111100111001111;
    case '6': return 0b111100111101111; case '7': return 0b111001010010010; case '8': return 0b111101111101111;
    case '9': return 0b111101111001111; case '.': return 0b000000000000010; case ':': return 0b000010000010000;
    case '-': return 0b000000111000000; case '/': return 0b001010100010100; default: return 0;
  }
}

void tinyText(int x, int y, const char* s, uint16_t color=C_LOW) {
  while (*s) {
    char c = *s++; if (c >= 'a' && c <= 'z') c -= 32;
    if (c == ' ') { x += 4; continue; }
    uint16_t p = tinyPattern(c);
    for (int row=0; row<5; row++) {
      for (int col=0; col<3; col++) { if (p & (1 << (14 - (row*3+col)))) lcdFillRect(x+col, y+row, 1, 1, color); }
    }
    x += 4;
  }
}

void tinyTextClipped(int x, int y, const char* s, uint16_t color, int cx, int cy, int cw, int ch) {
  while (*s) {
    char c = *s++; if (c >= 'a' && c <= 'z') c -= 32;
    if (c == ' ') { x += 4; continue; }
    uint16_t p = tinyPattern(c);
    for (int row=0; row<5; row++) {
      for (int col=0; col<3; col++) {
        if (p & (1 << (14 - (row*3+col)))) {
          int px = x+col, py = y+row;
          if (px >= cx && px < cx+cw && py >= cy && py < cy+ch) lcdFillRect(px, py, 1, 1, color);
        }
      }
    }
    x += 4;
  }
}
void mediumText(int x, int y, const char* s, uint16_t color, uint8_t scale) {
  if (scale < 1) scale = 1;
  while (*s) {
    char c = *s++;
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c == ' ') { x += 4 * scale; continue; }
    uint16_t p = tinyPattern(c);
    for (int row=0; row<5; row++) {
      for (int col=0; col<3; col++) {
        if (p & (1 << (14 - (row*3+col)))) {
          lcdFillRect(x + col*scale, y + row*scale, scale, scale, color);
        }
      }
    }
    x += 4 * scale;
  }
}

void mediumCompact(int x, int y, uint64_t value, uint16_t color, uint8_t scale) {
  char b[12];
  formatCompact(value, b, sizeof(b));
  mediumText(x, y, b, color, scale);
}

uint8_t trackRemainBarWidth(uint8_t maxWidth) {
  if (!wanted || !playing || softPaused) return maxWidth;
  uint32_t now = millis();
  uint32_t paused = trackVisualPausedTotalMs;
  if (trackVisualPausedAtMs) paused += now - trackVisualPausedAtMs;
  uint32_t elapsed = (trackVisualStartedAtMs > 0 && now > trackVisualStartedAtMs + paused) ? (now - trackVisualStartedAtMs - paused) : 0;
  uint32_t dur = trackDurationMs;
  if (dur < 30000UL) dur = 180000UL;
  if (elapsed >= dur) return 1;
  return (uint8_t)constrain((int)((uint64_t)(dur - elapsed) * maxWidth / dur), 1, maxWidth);
}

uint8_t poolJobBarWidth(uint8_t maxWidth) {
  if (!stratumConnected) return 18;
  if (!minerLastJobMs) return 44;
  uint32_t age = millis() - minerLastJobMs;
  if (age > 45000UL) return 24;
  return (uint8_t)constrain((int)(maxWidth - ((uint64_t)age * maxWidth / 45000UL)), 24, maxWidth);
}


void healZone(int cx, int cy, int cw, int ch);
void drawWebLine(int x0, int y0, int x1, int y1, uint16_t color) {
  // Thin line: old 2x2 web pixels were the main cause of dirty square trails.
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    lcdFillRect(x0, y0, 1, 1, color);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void healLineZone(int x0, int y0, int x1, int y1, int pad=5) {
  int x = min(x0, x1) - pad;
  int y = min(y0, y1) - pad;
  int w = abs(x1 - x0) + pad * 2 + 4;
  int h = abs(y1 - y0) + pad * 2 + 4;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 240) w = 240 - x;
  if (y + h > 320) h = 320 - y;
  if (w > 0 && h > 0) healZone(x, y, w, h);
}

void beginTobiWebFrame() { spNextWebCount = 0; }

void addTobiWebDot(int x, int y) {
  if (x < 6 || x > 233 || y < 6 || y > 312) return;
  // Avoid stacking duplicates on the same pixel cluster.
  for (uint8_t i = 0; i < TOBI_MAX_WEB_DOTS; i++) {
    if (spDotBorn[i] && abs(spDotX[i] - x) <= 3 && abs(spDotY[i] - y) <= 3) {
      spDotBorn[i] = millis();
      return;
    }
  }
  spDotX[spDotHead] = x;
  spDotY[spDotHead] = y;
  spDotBorn[spDotHead] = millis();
  spDotHead = (spDotHead + 1) % TOBI_MAX_WEB_DOTS;
}

void drawTobiWebDots() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < TOBI_MAX_WEB_DOTS; i++) {
    if (!spDotBorn[i]) continue;
    uint32_t age = now - spDotBorn[i];
    if (age > 22000UL) { spDotBorn[i] = 0; continue; }
    uint16_t c = (age < 12000UL) ? C_WHITE : C_LOW;
    lcdFillRect(spDotX[i] - 1, spDotY[i] - 1, 3, 3, C_BLACK);
    lcdFillRect(spDotX[i], spDotY[i], 1, 1, c);
    if (age < 7000UL) {
      lcdFillRect(spDotX[i] - 1, spDotY[i], 1, 1, c);
      lcdFillRect(spDotX[i] + 1, spDotY[i], 1, 1, c);
      lcdFillRect(spDotX[i], spDotY[i] - 1, 1, 1, c);
      lcdFillRect(spDotX[i], spDotY[i] + 1, 1, 1, c);
    }
  }
}

void drawTobiWebLine(int x0, int y0, int x1, int y1, uint16_t color) {
  // v26: true white web, not brown/amber. Store every drawn line so endpoints do not stay on the ceiling.
  uint16_t webC = (color == C_WHITE || color == C_LOW) ? color : C_WHITE;
  drawWebLine(x0, y0, x1, y1, webC);
  int ddx = x1 - x0;
  int ddy = y1 - y0;
  if ((ddx * ddx + ddy * ddy) > 900) {
    // Store the actual attachment point: edge/top anchors for swings, target point for hand shots.
    bool p0Surface = (x0 < 14 || x0 > 226 || y0 < 42 || y0 > 286);
    bool p1Surface = (x1 < 14 || x1 > 226 || y1 < 42 || y1 > 286);
    if (p0Surface) addTobiWebDot(x0, y0);
    else if (p1Surface) addTobiWebDot(x1, y1);
    else addTobiWebDot(x1, y1);
  }
  if (spNextWebCount < TOBI_MAX_WEB_LINES) {
    uint8_t i = spNextWebCount++;
    spNextWebX0[i] = x0; spNextWebY0[i] = y0; spNextWebX1[i] = x1; spNextWebY1[i] = y1;
  }
}

void eraseOldTobiWeb() {
  for (uint8_t i = 0; i < spWebCount && i < TOBI_MAX_WEB_LINES; i++) {
    drawWebLine(spWebX0[i], spWebY0[i], spWebX1[i], spWebY1[i], C_BLACK);
    drawWebLine(spWebX0[i]+1, spWebY0[i], spWebX1[i]+1, spWebY1[i], C_BLACK);
    healLineZone(spWebX0[i], spWebY0[i], spWebX1[i], spWebY1[i], 2);
  }
  spWebCount = 0;
}

void commitTobiWebFrame() {
  spWebCount = spNextWebCount;
  for (uint8_t i = 0; i < spWebCount && i < TOBI_MAX_WEB_LINES; i++) {
    spWebX0[i] = spNextWebX0[i]; spWebY0[i] = spNextWebY0[i];
    spWebX1[i] = spNextWebX1[i]; spWebY1[i] = spNextWebY1[i];
  }
}

void tobiHand(uint8_t mode, int x, int y, bool rightHand, int &hx, int &hy) {
  // Approximate visible hand positions per orientation.
  if (mode == 2) { hx = x + (rightHand ? 26 : 21); hy = y + (rightHand ? 28 : 6); return; }
  if (mode == 3) { hx = x + (rightHand ? 10 : 15); hy = y + (rightHand ? 28 : 6); return; }
  if (mode == 0) { hx = x + (rightHand ? 30 : 6);  hy = y + (rightHand ? 13 : 13); return; }
  hx = x + (rightHand ? 30 : 6); hy = y + (rightHand ? 22 : 22);
}

void drawTobiCameraShot(int hx, int hy, uint16_t color) {
  // Short-lived web blast toward the viewer/camera.
  int cx = 120 + (int)(sinf(millis() * 0.008f) * 18.0f);
  int cy = 155 + (int)(cosf(millis() * 0.010f) * 10.0f);
  drawTobiWebLine(hx, hy, cx, cy, color);
  drawTobiWebLine(cx - 6, cy, cx + 6, cy, color);
  drawTobiWebLine(cx, cy - 6, cx, cy + 6, color);
}

void drawBatIcon(int x, int y, int pct, uint16_t th) {
  // v10.11H: real battery widget: divisions only, no numeric percent overlay.
  uint16_t c = (pct <= 15) ? C_RED : ((pct <= 30) ? C_AMBER : th);
  lcdFillRect(x, y, 42, 18, C_BLACK);

  // Shell + nose.
  lcdFillRect(x, y, 38, 16, C_DARK);
  lcdFillRect(x+1, y+1, 34, 14, C_GREY);
  lcdFillRect(x+2, y+2, 32, 12, C_BLACK);
  lcdFillRect(x+35, y+4, 2, 8, C_GREY);

  // Five clear inner divisions. Empty divisions stay dark grey; filled divisions fade by battery level.
  const int segW = 5;
  const int gap = 1;
  int bars = (pct > 0) ? ((pct - 1) / 20) + 1 : 0;
  if (bars > 5) bars = 5;

  for (int i = 0; i < 5; i++) {
    int sx = x + 4 + i * (segW + gap);
    lcdFillRect(sx, y + 4, segW, 8, C_DARK);
    if (i < bars) {
      uint16_t fill = c;
      if (pct <= 8 && ((millis() / 400) & 1)) fill = C_BLACK; // low-battery blink
      lcdFillRect(sx + 1, y + 5, segW - 2, 6, fill);
    }
  }
}


void janusBestWorkerAbbrev(const char* in, char* out, size_t outSize) {
  if (!out || outSize == 0) return;
  out[0] = '\0';
  if (!in || !in[0] || !strcmp(in, "-")) {
    strlcpy(out, "NONE", outSize);
    return;
  }

  if (strstr(in, "Blind") || strstr(in, "EYE") || strstr(in, "Eye")) { strlcpy(out, "EYE", outSize); return; }
  if (strstr(in, "Core2") || strstr(in, "CORE")) { strlcpy(out, "CORE", outSize); return; }
  if (strstr(in, "Stick") || strstr(in, "STICK")) { strlcpy(out, "STK3", outSize); return; }
  if (strstr(in, "Beacon") || strstr(in, "BEACON")) { strlcpy(out, "BCN", outSize); return; }
  if (strstr(in, "Swarm") || strstr(in, "SWARM")) { strlcpy(out, "SWRM", outSize); return; }
  if (strstr(in, "Atom") || strstr(in, "ATOM")) { strlcpy(out, "ATOM", outSize); return; }

  size_t n = 0;
  for (size_t i = 0; in[i] && n + 1 < outSize && n < 5; ++i) {
    char c = in[i];
    bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!ok) continue;
    if (c >= 'a' && c <= 'z') c -= 32;
    out[n++] = c;
  }
  out[n] = '\0';
  if (!out[0]) strlcpy(out, "NODE", outSize);
}


// ============================================================
// UI RESTORATION ENGINE
// ============================================================

void mediumText(int x, int y, const char* s, uint16_t color, uint8_t scale=2);
void mediumCompact(int x, int y, uint64_t value, uint16_t color, uint8_t scale=2);
uint8_t trackRemainBarWidth(uint8_t maxWidth);
uint8_t poolJobBarWidth(uint8_t maxWidth);

void drawStaticUI() {
  lcdFillRect(0, 0, 240, 320, C_BLACK); 
  uint16_t th = getThemeColor();
  lcdFillRect(4,4,232,2,th); lcdFillRect(4,314,232,2,th);
  lcdFillRect(4,4,2,312,th); lcdFillRect(234,4,2,312,th);

  // v36.8: removed the two unlabeled mystery header bars.
  // The signed MEREZHA/MUSYKA bars are drawn by drawAllText() in this freed space.
  lcdFillRect(12, 12, 160, 38, C_BLACK);
  lcdFillRect(180, 12, 50, 30, C_BLACK);
}

void healZone(int cx, int cy, int cw, int ch) {
  uint16_t th = getThemeColor();
  fillRectClip(4,4,232,2, th, cx,cy,cw,ch);
  fillRectClip(4,314,232,2, th, cx,cy,cw,ch);
  fillRectClip(4,4,2,312, th, cx,cy,cw,ch);
  fillRectClip(234,4,2,312, th, cx,cy,cw,ch);

  fillRectClip(12, 12, 160, 38, C_BLACK, cx,cy,cw,ch);
  fillRectClip(180, 12, 50, 30, C_BLACK, cx,cy,cw,ch);

  fillRectClip(12, 26, WiFi.status()==WL_CONNECTED?120:34, 5, th, cx,cy,cw,ch);
  fillRectClip(12, 41, playing?120:44, 5, softPaused?C_DIM:th, cx,cy,cw,ch);
  fillRectClip(12, 72, stratumConnected?180:40, 2, stratumConnected?th:C_RED, cx,cy,cw,ch);
  fillRectClip(10, 82, 220, 36, C_BLACK, cx,cy,cw,ch);
  fillRectClip(10, 119, 72, 32, C_BLACK, cx,cy,cw,ch);
}

void drawAllText() {
  // v37.1 anti-flicker:
  // Do not clear/repaint the full HUD every scene frame.
  // The old version black-filled header/value blocks at 20-30 FPS, which reads as blinking.
  static uint32_t lastHudPaintMs = 0;
  static uint32_t lastTrackPaintMs = 0;
  static uint8_t oldWifi = 255;
  static uint8_t oldPlay = 255;
  static uint8_t oldPause = 255;
  static uint8_t oldPalLocal = 255;
  static uint32_t oldHashLocal = 0xFFFFFFFF;
  static uint32_t oldLocalHashLocal = 0xFFFFFFFF;
  static uint32_t oldSharesLocal = 0xFFFFFFFF;
  static uint32_t oldColonyLocal = 0xFFFFFFFF;
  static uint32_t oldBestLocal = 0xFFFFFFFF;
  static uint32_t oldDiffLocal = 0xFFFFFFFF;
  static uint32_t oldRejectLocal = 0xFFFFFFFF;
  static uint32_t oldOkPctLocal = 0xFFFFFFFF;
  static uint8_t oldWorkersOnlineLocal = 255;
  static uint8_t oldWorkersKnownLocal = 255;
  static uint32_t oldAgentRewardsLocal = 0xFFFFFFFF;
  static uint32_t oldAgentShareRewardsLocal = 0xFFFFFFFF;
  static uint32_t oldAgentTopScoreLocal = 0xFFFFFFFF;
  static uint8_t oldShareGlowBucket = 255;
  static uint32_t lastShareGlowPaintMs = 0;

  uint32_t now = millis();
  uint16_t th = getThemeColor();
  uint8_t wifiNow = WiFi.status()==WL_CONNECTED ? 1 : 0;
  uint8_t playNow = playing ? 1 : 0;
  uint8_t pauseNow = softPaused ? 1 : 0;
  uint32_t hNow = minerRealHashrate ? minerRealHashrate : minerLocalHashrate;
  uint32_t submitTotalNow = minerSubmitAttempts;
  uint32_t okPctNow = submitTotalNow ? (minerShares * 100UL / max(1UL, submitTotalNow)) : 0;
  if (okPctNow > 999) okPctNow = 999;
  uint8_t workersOnlineNow = colonyOnlineNodeCount();
  uint8_t workersKnownNow = colonyKnownNodes;
  uint8_t shareGlow = janusShareUiGlow(now);
  uint8_t shareGlowBucket = shareGlow / 24;  // redraw fade in visible steps, not every tick
  bool shareGlowNeedsPaint = (shareGlow > 0) &&
                             (shareGlowBucket != oldShareGlowBucket || now - lastShareGlowPaintMs > 85UL);

  bool forceHeader =
      oldWifi != wifiNow ||
      oldPlay != playNow ||
      oldPause != pauseNow ||
      oldPalLocal != ledPalette ||
      (now - lastHudPaintMs > 900);

  bool forceTrack =
      forceHeader ||
      (now - lastTrackPaintMs > 700);

  bool forceValues =
      forceHeader ||
      oldHashLocal != hNow ||
      oldLocalHashLocal != minerLocalHashrate ||
      oldSharesLocal != minerShares ||
      oldColonyLocal != colonyRxCount ||
      oldBestLocal != minerBestBits ||
      oldDiffLocal != minerCurrentDiff ||
      oldRejectLocal != minerSubmitRejects ||
      oldOkPctLocal != okPctNow ||
      oldWorkersOnlineLocal != workersOnlineNow ||
      oldWorkersKnownLocal != workersKnownNow ||
      oldAgentRewardsLocal != colonyAgentRewardsSent ||
      oldAgentShareRewardsLocal != colonyAgentShareRewardsSent ||
      oldAgentTopScoreLocal != colonyAgentTopScoreX10 ||
      shareGlowNeedsPaint;

  if (!forceHeader && !forceTrack && !forceValues) return;

  if (forceHeader) {
    lastHudPaintMs = now;
    oldWifi = wifiNow;
    oldPlay = playNow;
    oldPause = pauseNow;
    oldPalLocal = ledPalette;

    // Header area only, not the whole screen.
    lcdFillRect(12, 12, 142, 38, C_BLACK);
    drawBatIcon(186, 16, readBatteryClamped(), th);

    tinyText(12, 17, "MEREZHA", C_LOW);
    lcdFillRect(12, 26, 132, 5, C_DARK);
    lcdFillRect(12, 26, wifiNow ? 132 : 34, 5, th);

    tinyText(12, 33, "TRACK", C_LOW);
  }

  if (forceTrack) {
    lastTrackPaintMs = now;
    // Redraw only the changing progress bar, not labels.
    lcdFillRect(12, 41, 132, 5, C_DARK);
    lcdFillRect(12, 41, trackRemainBarWidth(132), 5, softPaused ? C_DIM : th);

    tinyText(12, 64, "POOL JOB", C_LOW);
    lcdFillRect(12, 72, 180, 3, C_DARK);
    lcdFillRect(12, 72, poolJobBarWidth(180), 3, stratumConnected ? th : C_RED);
  }

  if (forceValues) {
    oldHashLocal = hNow;
    oldLocalHashLocal = minerLocalHashrate;
    oldSharesLocal = minerShares;
    oldColonyLocal = colonyRxCount;
    oldBestLocal = minerBestBits;
    oldDiffLocal = minerCurrentDiff;
    oldRejectLocal = minerSubmitRejects;
    oldOkPctLocal = okPctNow;
    oldWorkersOnlineLocal = workersOnlineNow;
    oldWorkersKnownLocal = workersKnownNow;
    oldAgentRewardsLocal = colonyAgentRewardsSent;
    oldAgentShareRewardsLocal = colonyAgentShareRewardsSent;
    oldAgentTopScoreLocal = colonyAgentTopScoreX10;
    oldShareGlowBucket = shareGlowBucket;
    lastShareGlowPaintMs = now;

    // Value blocks update only when values change / once per header refresh.
    lcdFillRect(10, 82, 220, 36, C_BLACK);

    // v10.11H: SHAR label + accepted-share number burn in sync with LED bonfire.
    // Idle: grey. ACCEPT: palette-color flare. Fade: back to grey.
    uint16_t shareLabelColor = janusShareUiColor(C_LOW, th, now);
    uint16_t shareValueColor = janusShareUiColor(C_DIM, th, now);

    // v10.11G aligned grid:
    // columns: left mining speed/difficulty, center SHAR/WORKERS, right BEST/OK%.
    // WORKERS is a single real online count, not online/known like 4/4.
    lcdFillRect(10, 76, 220, 72, C_BLACK);

    mediumText(12, 76, "POOL H/S", C_LOW, 2);
    mediumText(94, 76, "SHAR", shareLabelColor, 2);
    mediumText(176, 76, "BEST", C_LOW, 2);

    mediumCompact(12, 90, hNow, th, 3);
    mediumCompact(94, 90, minerShares, shareValueColor, 3);

    char agentTopSnap[24];
    float agentPredErrSnap = 0.0f;
    colonyAgentSnapshot(agentTopSnap, sizeof(agentTopSnap), &agentPredErrSnap);
    char bestAbbr[8];
    janusBestWorkerAbbrev(agentTopSnap, bestAbbr, sizeof(bestAbbr));
    mediumText(176, 90, bestAbbr, th, 2);

    mediumText(12, 112, minerLocalFallback ? "BEST" : "SLOZHN", C_LOW, 2);
    tinyText(94, 112, "WORKERS", C_LOW);
    mediumText(176, 112, "OK%", C_LOW, 2);

    mediumCompact(12, 126, minerBestBits ? minerBestBits : minerCurrentDiff, C_DIM, 3);
    mediumCompact(94, 126, workersOnlineNow, C_DIM, 3);
    mediumCompact(176, 126, okPctNow, C_DIM, 3);
  }
}

// ============================================================
// CAMPFIRE SCENE v18: centered knight + stalker guitarist
// ============================================================
void drawPixelSword(int x, int y, uint16_t c) {
  lcdFillRect(x, y, 2, 34, c);
  lcdFillRect(x - 5, y + 9, 12, 2, c);
  lcdFillRect(x - 1, y - 3, 4, 4, C_WHITE);
}

void drawKnightCamp(int kx, int ky, uint16_t th, int bob, int actionState, float kProg) {
  int glow = (int)(14.0f * sinf(millis() * 0.015f));
  if (glow < 0) glow = 0;

  int headY = ky + 7 + bob - glow / 10;
  int armLY = ky + 27;
  int armLX = kx + 6;
  int swordY = ky + 8;
  int swordX = kx + 34;
  bool swordSlash = false;

  if (actionState == 1) {
    if (kProg > 0.2f && kProg < 0.8f) {
      armLY = ky + 15;
      armLX = kx + 10;
      headY -= 3;
      lcdFillRect(kx + 8, ky + 9, 8, 10, C_AMBER2);
      lcdFillRect(kx + 10, ky + 5, 4, 4, C_WHITE);
      if (stableBlink(0x4B4E4947U + (uint32_t)actionState, 120, 92)) {
        int sx = kx + 8 + stableRange(0, 7, 0xA111u + (uint32_t)actionState, 120);
        int sy = ky + 7 - stableRange(0, 9, 0xA211u + (uint32_t)actionState, 120);
        lcdFillRect(sx, sy, 2, 2, C_AMBER);
      }
    } else {
      float sub = (kProg <= 0.2f) ? (kProg / 0.2f) : ((1.0f - kProg) / 0.2f);
      armLY = ky + 27 - (int)(10 * sub);
      armLX = kx + 6 + (int)(4 * sub);
    }
  } else if (actionState == 2) {
    if (kProg < 0.3f) { swordY -= 8; swordX += 2; }
    else if (kProg < 0.6f) swordSlash = true;
  } else if (actionState == 3) {
    float lift = (kProg < 0.2f) ? (kProg / 0.2f) : ((kProg < 0.8f) ? 1.0f : ((1.0f - kProg) / 0.2f));
    headY -= (int)(12 * lift);
    armLY -= (int)(12 * lift);
    armLX += (int)(5 * lift);
  }

  lcdFillRect(kx + 14, ky + 22 + bob, 18, 23, C_GREY);
  lcdFillRect(kx + 10, ky + 33, 28, 8, C_DARK);
  lcdFillRect(kx + 8,  ky + 45, 18, 5, C_GREY);
  lcdFillRect(kx + 24, ky + 45, 18, 5, C_GREY);
  lcdFillRect(kx + 16, headY, 16, 13 + glow / 10, th);
  lcdFillRect(kx + 14, headY + 5, 20, 3, C_WHITE);
  lcdFillRect(kx + 20, headY + 5, 10, 3, C_BLACK);
  lcdFillRect(armLX, armLY, 16, 4, C_GREY);

  if (swordSlash) {
    lcdFillRect(kx + 16, ky + 25, 31, 4, C_AMBER);
    lcdFillRect(kx + 20, ky + 21, 4, 12, C_AMBER);
    lcdFillRect(kx + 39, ky + 27, 12, 2, C_WHITE);
  } else {
    drawPixelSword(swordX, swordY, C_AMBER);
  }
}

void drawCampfireCore(int fx, int fy, uint16_t th) {
  uint32_t fireTick = millis() / 70U;
  int f = (int)(4.0f * sinf(millis() * 0.018f)) + (int)(stableNoise8(0xF1A30000U ^ fireTick) & 1);
  lcdFillRect(fx + 8,  fy + 32, 30, 4, C_DIM);
  lcdFillRect(fx + 4,  fy + 36, 38, 4, C_DARK);
  lcdFillRect(fx + 14, fy + 28, 18, 5, C_DEEP_RED);
  lcdFillRect(fx + 20, fy + 12 - f, 7, 22 + f, C_RED);
  lcdFillRect(fx + 16, fy + 18 - f / 2, 5, 16, C_ORANGE);
  lcdFillRect(fx + 27, fy + 19 - f / 2, 5, 14, C_ORANGE);
  lcdFillRect(fx + 22, fy + 19, 4, 10, th);
  lcdFillRect(fx + 23, fy + 13 - f, 2, 7, C_WHITE);
  if (stableBlink(0xF1A3B00FU, 90, 88)) {
    int sx = fx + 18 + stableRange(0, 13, 0xF1A3C111U, 90);
    int sy = fy + 8 - stableRange(0, 9,  0xF1A3D222U, 90);
    lcdFillRect(sx, sy, 1, 2, C_AMBER2);
  }
  if (stableBlink(0xF1A3E00FU, 140, 56)) {
    int sx2 = fx + 20 + stableRange(0, 10, 0xF1A3E111U, 140);
    int sy2 = fy + 6  - stableRange(0, 12, 0xF1A3E222U, 140);
    lcdFillRect(sx2, sy2, 1, 1, C_WHITE);
  }
}

void updateCampWeather(bool musicActive) {
  uint32_t now = millis();
  float energy = 0.0f;
  for (int i = 0; i < 16; i++) energy += spectrum[i];
  energy /= 16.0f;

  // v22: no more chaotic rain->blood-moon jumps every few seconds.
  // Weather now follows a long chain: clear/stars -> clouds -> rain/snow -> clouds -> moon/stars.
  // Dynamic tracks can shorten the HOLD, but cannot skip the logical sequence.
  bool dynamicTrack = musicActive && (energy > 0.78f || spectrum[0] > 0.88f || spectrum[5] > 0.86f);
  if (weatherNextChangeMs == 0) {
    campWeather = WX_STARS;
    weatherChainStep = 0;
    weatherSeed = (janusRand() ^ 0x1138C0DE) | 1;
    weatherMinHoldUntilMs = now + 45000;
    weatherNextChangeMs = now + 75000;
    return;
  }

  if (dynamicTrack && now > weatherMinHoldUntilMs && weatherNextChangeMs > now + 25000) {
    weatherNextChangeMs = now + 25000 + (janusRand() % 12000);
  }

  if (now <= weatherNextChangeMs) return;

  weatherChainStep = (weatherChainStep + 1) % 12;
  uint32_t r = janusRand() ^ (uint32_t)(energy * 1000.0f) ^ now ^ weatherSeed;
  switch (weatherChainStep) {
    case 0: campWeather = WX_STARS; break;
    case 1: campWeather = ((r & 0x1F) == 0) ? WX_DAWN : WX_CLEAR; break;
    case 2: campWeather = WX_CLOUDS; break;
    case 3: campWeather = (r & 1) ? WX_RAIN : WX_SNOW; break;
    case 4: campWeather = WX_CLOUDS; break;
    case 5: campWeather = (r & 2) ? WX_RED_MOON : WX_STARS; break;
    case 6: campWeather = WX_CLOUDS; break;
    case 9: campWeather = ((r & 0x3F) == 0) ? WX_DAWN : WX_STARS; break;
    default: campWeather = WX_CLEAR; break;
  }
  weatherSeed = r | 1;

  uint32_t hold = 55000 + (r % 65000);        // 55..120 sec normally
  if (dynamicTrack) hold = 32000 + (r % 36000); // 32..68 sec on energetic tracks
  weatherMinHoldUntilMs = now + ((hold < 45000UL) ? hold : 45000UL);
  weatherNextChangeMs = now + hold;
}
void drawWeatherSky(int x, int y, int w, int h, uint16_t th) {
  uint32_t t = millis();
  bool redMoon = campWeather == WX_RED_MOON;
  bool dawn = campWeather == WX_DAWN;
  uint16_t sky1 = dawn ? 0x5B7F : (redMoon ? 0x2800 : 0x0841);
  uint16_t sky2 = dawn ? 0x7C9F : (redMoon ? 0x4800 : 0x1082);
  uint16_t sky3 = dawn ? 0xFBA0 : (redMoon ? 0x2000 : 0x1883);

  lcdFillRect(x, y, w, 10, sky1);
  lcdFillRect(x, y + 10, w, 12, sky2);
  lcdFillRect(x, y + 22, w, 16, sky3);
  lcdFillRect(x, y + 38, w, h - 38, C_BLACK);

  // big moon / red moon in the upper-right horizon
  int mx = x + w - 46;
  int my = y + 13;
  uint16_t moonA = redMoon ? C_RED : 0x4208;
  uint16_t moonB = redMoon ? C_CRIMSON : 0x630C;
  uint16_t moonC = redMoon ? C_DEEP_RED : 0x8410;
  if (!dawn && campWeather != WX_CLOUDS && campWeather != WX_RAIN) {
    lcdFillRect(mx + 8,  my + 0, 20, 2, moonA);
    lcdFillRect(mx + 4,  my + 2, 30, 3, moonB);
    lcdFillRect(mx + 2,  my + 5, 34, 12, moonC);
    lcdFillRect(mx + 4,  my + 17, 30, 4, moonB);
    lcdFillRect(mx + 8,  my + 21, 20, 2, moonA);
    lcdFillRect(mx + 12, my + 7, 5, 2, moonA);
    lcdFillRect(mx + 24, my + 12, 6, 2, moonA);
    lcdFillRect(mx + 17, my + 17, 4, 1, 0x3186);
  }

  // stars / clear sky
  if (campWeather == WX_STARS || campWeather == WX_CLEAR) {
    for (int i = 0; i < 34; i++) {
      int sx = x + 2 + ((weatherSeed + i * 37) % max(1, w - 4));
      int sy = y + 3 + ((weatherSeed >> 3) + i * 19) % 50;
      if (sx > mx - 2 && sx < mx + 40 && sy > my - 2 && sy < my + 28) continue;
      uint16_t c = (campWeather == WX_STARS) ? C_LOW : C_DARK;
      lcdFillRect(sx, sy, 1, 1, c);
    }
  }

  // clouds, deterministic moving bands
  if (campWeather == WX_CLOUDS || campWeather == WX_RAIN || campWeather == WX_SNOW) {
    for (int i = 0; i < 7; i++) {
      int cx = x + ((i * 43 + (int)(t / 170) + (weatherSeed & 31)) % (w + 34)) - 18;
      int cy = y + 10 + ((i * 9 + (weatherSeed >> 5)) % 28);
      uint16_t cc = (campWeather == WX_RAIN) ? 0x2104 : 0x3186;
      fillRectClip(cx, cy, 22, 3, cc, x, y, w, h);
      fillRectClip(cx + 5, cy - 3, 18, 3, cc, x, y, w, h);
      fillRectClip(cx + 15, cy + 2, 20, 2, cc, x, y, w, h);
    }
  }

  // rare dawn / beach horizon: sea from edge to edge, Dendy-like but JANUS dark.
  if (dawn) {
    int seaY = y + max(35, h/3);
    lcdFillRect(x, seaY, w, max(1, h - (seaY-y) - 18), 0x039F);
    lcdFillRect(x, seaY + 8, w, 3, 0x7D9F);
    lcdFillRect(x, seaY + 18, w, 2, 0x1C7F);
    for (int i=0;i<9;i++) {
      int wx = x + 8 + ((i*31 + (int)(t/90)) % max(1,w-18));
      int wy = seaY + 10 + (i%4)*8;
      fillRectClip(wx, wy, 18 + (i%3)*5, 1, 0xBDF7, x, y, w, h);
    }
    lcdFillRect(x, y + h - 18, w, 18, 0x6AA0);
    lcdFillRect(x, y + h - 16, w, 2, 0xC540);
  }

  // rain / snow
  if (campWeather == WX_RAIN) {
    for (int i = 0; i < 36; i++) {
      int rx = x + ((i * 17 + (int)(t / 24) + (weatherSeed & 63)) % max(1, w));
      int ry = y + 2 + ((i * 23 + (int)(t / 10)) % max(1, h - 18));
      drawWebLine(rx, ry, min(x + w - 1, rx + 3), min(y + h - 1, ry + 8), C_BLUE_DIM);
    }
  } else if (campWeather == WX_SNOW) {
    for (int i = 0; i < 28; i++) {
      int sx = x + ((i * 29 + (int)(t / 55) + (weatherSeed & 31)) % max(1, w));
      int sy = y + 2 + ((i * 17 + (int)(t / 35)) % max(1, h - 20));
      lcdFillRect(sx, sy, 1 + (i % 2 == 0), 1, C_LOW);
    }
  }
}

void drawPostApocBackdrop(int x, int y, int w, int h, uint16_t th) {
  // V21: clipped cinematic post-apoc backdrop + dynamic weather.
  lcdFillRect(x, y, w, h, C_BLACK);
  drawWeatherSky(x, y, w, h, th);

  if (campWeather == WX_DAWN) {
    // minimal beach props; actors/fire are drawn on top later.
    for (int i=0;i<10;i++) {
      int px = x + 4 + (i*23) % max(1,w-8);
      lcdFillRect(px, y+h-13+(i%3), 3+(i%4), 1, 0x3186);
    }
    int fx = x + w / 2;
    int fy = y + h - 38;
    lcdFillRect(fx - 38, fy - 25, 76, 47, C_BLACK);
    lcdFillRect(fx - 24, fy - 12, 48, 24, C_DEEP_RED);
    lcdFillRect(fx - 15, fy - 7, 30, 14, 0xA280);
    return;
  }

  // broken skyline, no bright blocks near the central fire
  for (int i = 0; i < 12; i++) {
    int bx = x + 3 + i * 14 + ((i * 5) % 4);
    int bw = 8 + (i % 3) * 3;
    int bh = 14 + ((i * 11) % 24);
    int by = y + h - 20 - bh;
    if (bx + bw > x + w - 2) bw = (x + w - 2) - bx;
    if (bw <= 0) continue;
    uint16_t bc = (i & 1) ? 0x0861 : 0x1082;
    if (campWeather == WX_RED_MOON) bc = (i & 1) ? 0x1000 : 0x1800;
    lcdFillRect(bx, by, bw, bh, bc);
    if (i % 2 == 0 && by > y + 4) lcdFillRect(bx + 2, by - 3, max(2, bw - 5), 3, bc);
    if ((i % 3 == 0) && (bx < x + w/2 - 20 || bx > x + w/2 + 28)) lcdFillRect(bx + 3, by + 8, 2, 2, campWeather == WX_RAIN ? C_BLUE_DIM : C_DIM);
  }

  // dead branches on right, clipped by coordinates so they do not spill past frame
  int tx = x + w - 30;
  int ty = y + h - 54;
  lcdFillRect(tx, ty, 4, 43, C_BLACK);
  drawWebLine(tx + 2, ty + 8,  min(x + w - 3, tx + 24), max(y + 2, ty - 6), C_BLACK);
  drawWebLine(tx + 2, ty + 14, min(x + w - 5, tx + 17), ty + 1, C_BLACK);
  drawWebLine(tx + 1, ty + 20, max(x + 3, tx - 11), ty + 8, C_BLACK);
  drawWebLine(tx + 2, ty + 26, min(x + w - 4, tx + 20), ty + 15, C_BLACK);

  // ground / rubble
  lcdFillRect(x, y + h - 16, w, 16, C_BLACK);
  lcdFillRect(x + 3, y + h - 18, w - 6, 2, C_DARK);
  for (int i = 0; i < 24; i++) {
    int rx = x + 2 + (i * 31) % max(1, w - 6);
    int ry = y + h - 13 + ((i * 7) % 9);
    lcdFillRect(rx, ry, 2 + (i % 3), 1, (i & 1) ? C_LOW : C_DARK);
  }

  // central dark cutout behind flames: separates fire from background
  int fx = x + w / 2;
  int fy = y + h - 38;
  lcdFillRect(fx - 38, fy - 25, 76, 47, C_BLACK);
  lcdFillRect(fx - 24, fy - 12, 48, 24, C_DEEP_RED);
  lcdFillRect(fx - 15, fy - 7, 30, 14, 0xA280);
}

void drawSmokeColumn(int fx, int fy) {
  uint32_t t = millis();
  // Smoke rises clearly above the sword/fire, separated from the moon/sky.
  for (int i = 0; i < 9; i++) {
    int sx = fx + 23 + (int)(sinf(t * 0.0022f + i * 1.35f) * (3 + i));
    int sy = fy + 11 - i * 7 - ((t / 240 + i * 2) % 6);
    uint16_t c = (i < 2) ? C_LOW : ((i < 6) ? 0x3186 : 0x2104);
    lcdFillRect(sx, sy, 3 + (i % 2), 1 + (i % 3 == 0), c);
    if (i > 2) lcdFillRect(sx + 5, sy + 1, 2, 1, c);
    if (i > 5) lcdFillRect(sx - 4, sy + 2, 1, 1, c);
  }
}

void drawStalkerGuitar(int sx, int sy, uint16_t th, bool musicActive) {
  uint32_t now = millis();
  int beat = musicActive ? ((now / 135) & 1) : ((now / 520) & 1);
  int nod = musicActive ? (int)(sinf(now * 0.026f) * 3.0f) : (int)(sinf(now * 0.006f) * 1.0f);
  int strum = musicActive ? (int)(sinf(now * 0.046f) * 5.0f) : 0;
  uint16_t coat = musicActive ? C_GREY : C_DARK;

  // warm firelight from the left side
  lcdFillRect(sx + 4, sy + 18, 28, 24, C_DEEP_RED);

  // legs / boots
  lcdFillRect(sx + 3,  sy + 41, 12, 5, C_LOW);
  lcdFillRect(sx + 26, sy + 41, 12, 5, C_LOW);
  lcdFillRect(sx + 8,  sy + 34, 6, 8, C_DARK);
  lcdFillRect(sx + 27, sy + 34, 6, 8, C_DARK);

  // body / hood
  lcdFillRect(sx + 11, sy + 19, 19, 20, coat);
  lcdFillRect(sx + 13, sy + 10 + nod, 15, 12, C_BLACK);
  lcdFillRect(sx + 15, sy + 12 + nod, 11, 7, C_GREY);

  // glowing mask lenses and respirator line
  uint16_t eye = musicActive ? th : C_RED;
  lcdFillRect(sx + 16, sy + 14 + nod, 3, 3, eye);
  lcdFillRect(sx + 22, sy + 14 + nod, 3, 3, eye);
  if (musicActive && beat) {
    lcdFillRect(sx + 15, sy + 13 + nod, 5, 5, th);
    lcdFillRect(sx + 21, sy + 13 + nod, 5, 5, th);
  }
  lcdFillRect(sx + 16, sy + 19 + nod, 9, 2, th);
  lcdFillRect(sx + 18, sy + 21 + nod, 5, 2, C_BLACK);

  // guitar body and neck
  lcdFillRect(sx + 2,  sy + 28, 15, 12, C_AMBER);
  lcdFillRect(sx + 5,  sy + 30, 10, 8, C_ORANGE);
  lcdFillRect(sx + 16, sy + 31, 30, 3, C_AMBER2);
  lcdFillRect(sx + 43, sy + 29, 4, 8, C_AMBER2);
  lcdFillRect(sx + 8,  sy + 32, 4, 4, C_BLACK);

  // animated strings, thin and bright
  for (int i = 0; i < 3; i++) {
    int off = i - 1;
    drawWebLine(sx + 7, sy + 31 + i * 2, sx + 45, sy + 30 + i + off, (musicActive && beat) ? C_WHITE : C_AMBER2);
  }

  // arms, right hand strums while music is active
  lcdFillRect(sx + 0,  sy + 24, 11, 4, C_GREY);
  lcdFillRect(sx + 18, sy + 25, 18, 4, C_GREY);
  lcdFillRect(sx + 13 + strum, sy + 31 + beat, 4, 5, th);
  if (musicActive) {
    lcdFillRect(sx + 49, sy + 30, 2, 2, th);
    if (beat) lcdFillRect(sx + 52, sy + 28, 1, 1, th);
  }
}

// ============================================================
// 25 FPS SURGICAL ENGINE (V25)
// ============================================================
void drawFastUI() {
  uint16_t th = getThemeColor();
  bool act = playing && !softPaused;
  updateCampWeather(act);

  // V16: heavy UI pass only. Tobi is handled by drawTobiTurboOverlay() at 60 FPS.

  // V17: EQ moved to the very bottom. Max height is small so the bonfire/knight area stays clear.
  constexpr int EQ_BASE_Y = 312;
  constexpr int EQ_MAX_H  = 34;
  for (int i=0; i<16; i++) {
    int h = 2 + (int)(spectrum[i] * (float)EQ_MAX_H);
    int xx = 8 + i * 14;
    if (h != specOldH[i]) {
      lcdFillRect(xx, EQ_BASE_Y - EQ_MAX_H, 9, EQ_MAX_H, C_BLACK);
      lcdFillRect(xx, EQ_BASE_Y - h, 9, h, (i%3==0)?th:((i%3==1)?C_AMBER:C_DIM));
      lcdFillRect(xx, EQ_BASE_Y + 1, 9, 2, C_DARK);
      specOldH[i] = h;
    }
  }

  // V27: full-width cinematic camp scene without flicker.
  // The static/weather backdrop is not cleared every actor frame anymore.
  constexpr int SCENE_X = 6;
  constexpr int SCENE_Y = 148;
  constexpr int SCENE_W = 228;
  constexpr int SCENE_H = 108;

  // v37.1: full backdrop is the expensive/blinky part.
  // Refresh it at a steady lower rate; actors/fire remain animated every heavy pass.
  static uint32_t lastBackdropMs = 0;
  static uint8_t lastBackdropPal = 255;
  static WeatherMode lastBackdropWeather = (WeatherMode)255;
  bool needBackdrop = (millis() - lastBackdropMs > 320) || lastBackdropPal != ledPalette || lastBackdropWeather != campWeather;
  if (needBackdrop) {
    lastBackdropMs = millis();
    lastBackdropPal = ledPalette;
    lastBackdropWeather = campWeather;
    drawPostApocBackdrop(SCENE_X, SCENE_Y, SCENE_W, SCENE_H, th);
  }

  knightFrame++;
  int b = (knightFrame / 8) & 1;

  if (knightState == 0 && millis() > knightNextAction) {
      knightState = 1 + (janusRand() % 3);
      knightAnimStart = millis();
      knightNextAction = millis() + 6000 + (janusRand() % 10000);
  }

  float kProg = 0;
  if (knightState > 0) {
      float dur = (knightState == 1) ? 2500.0f : ((knightState == 2) ? 800.0f : 3000.0f);
      kProg = (millis() - knightAnimStart) / dur;
      if (kProg >= 1.0f) { knightState = 0; kProg = 0.0f; }
  }

  // Composition like the reference: left knight, center sword/fire, right stalker guitarist.
  const int knightX  = 54;   // left of campfire
  const int knightY  = 176;
  const int fireX    = 102;  // visual center
  const int fireY    = 186;
  const int stalkerX = 154;  // right, kept inside frame
  const int stalkerY = 176;

  // When backdrop is not fully refreshed this frame, softly heal only actor/fire zones.
  if (!needBackdrop) {
    healZone(knightX - 8, knightY - 12, 68, 76);
    healZone(fireX - 10, fireY - 26, 60, 76);
    healZone(stalkerX - 12, stalkerY - 10, 72, 76);
  }

  // dynamic firelight hitting both characters
  lcdFillRect(knightX + 28, knightY + 26, 16, 18, C_DEEP_RED);
  lcdFillRect(stalkerX - 4, stalkerY + 22, 12, 18, C_DEEP_RED);

  drawKnightCamp(knightX, knightY, th, b, knightState, kProg);
  // center sword silhouette behind flames
  drawPixelSword(fireX + 23, fireY - 12, C_BLACK);
  drawCampfireCore(fireX, fireY, th);
  drawSmokeColumn(fireX, fireY);
  drawStalkerGuitar(stalkerX, stalkerY, th, act);

  // V19: slim marquee raised above the taller bottom EQ. Contains track + miner stats.
  int cx = 12, cy = 260, cw = 216, ch = 10;
  lcdFillRect(cx, cy, cw, ch, act ? th : C_DARK);
  static float textScroll = 0;
  char marquee[176];
  char hBuf[12], sBuf[12], pBuf[12], rBuf[12], bBuf[12], qBuf[12], subBuf[12], lBuf[12], rrBuf[12];
  uint32_t allH = minerRealHashrate + minerLocalHashrate;
  uint32_t subTotal = minerSubmitAttempts;                 // total pool submit attempts
  uint32_t subRemote = minerRemoteSubmitAttempts;           // subset of total
  uint32_t subLocal = (subTotal >= subRemote) ? (subTotal - subRemote) : 0;
  uint32_t okPct = subTotal ? (minerShares * 100UL / max(1UL, subTotal)) : 0;
  uint32_t qNow = colonyRemoteShareQueue ? (uint32_t)uxQueueMessagesWaiting(colonyRemoteShareQueue) : 0;
  formatCompact(allH, hBuf, sizeof(hBuf));
  formatCompact(minerShares, sBuf, sizeof(sBuf));
  formatCompact(colonyRxCount, pBuf, sizeof(pBuf));
  formatCompact(minerSubmitRejects, rBuf, sizeof(rBuf));
  formatCompact(minerBestBits, bBuf, sizeof(bBuf));
  formatCompact(qNow, qBuf, sizeof(qBuf));
  formatCompact(subTotal, subBuf, sizeof(subBuf));
  formatCompact(subLocal, lBuf, sizeof(lBuf));
  formatCompact(subRemote, rrBuf, sizeof(rrBuf));
  char agentTopSnap[24];
  float agentPredErrSnap = 0.0f;
  colonyAgentSnapshot(agentTopSnap, sizeof(agentTopSnap), &agentPredErrSnap);
  snprintf(marquee, sizeof(marquee), "%s  H %s  T%u  B%s  ACC %s  SUB %s L%s/R%s  OK%lu%%  REJ %s  W %u/%u  AOK %lu AG %lu TOP %s PE%.2f  Q %s  RX %s",
           act ? trackName : "IDLE",
           hBuf,
           (unsigned)minerShareTargetBits,
           bBuf,
           sBuf,
           subBuf,
           lBuf,
           rrBuf,
           (unsigned long)okPct,
           rBuf,
           (unsigned)colonyOnlineNodeCount(),
           (unsigned)colonyKnownNodes,
           (unsigned long)colonyAgentShareRewardsSent,
           (unsigned long)colonyAgentRewardsSent,
           agentTopSnap,
           agentPredErrSnap,
           qBuf,
           pBuf);
  int textW = strlen(marquee) * 4;
  if (textW > cw - 4) { textScroll += 1.2f; if (textScroll > textW + 12) textScroll = -cw; }
  else textScroll = -(cw - textW) / 2.0f;
  tinyTextClipped(cx - (int)textScroll, cy + 3, marquee, C_BLACK, cx + 2, cy, cw - 4, ch);

  static uint32_t lastPopupState = 0;
  if (millis() < popupTimer) {
    lcdFillRect(144, 140, 86, 32, C_BLACK); 
    lcdFillRect(144, 140, 2, 32, popupColor); 
    tinyText(152, 146, popupText, popupColor); 
    if (popupShowBar) {
      int barW = 70; lcdFillRect(152, 160, barW, 6, C_DARK); 
      int fillW = popupIsVol ? map(volumeVal, 0, 21, 0, barW) : map(ledBright, 0, 100, 0, barW);
      lcdFillRect(152, 160, constrain(fillW, 0, barW), 6, popupColor); 
    }
    lastPopupState = 1;
  } else if (lastPopupState == 1) {
     lcdFillRect(144, 140, 86, 32, C_BLACK);
     lastPopupState = 0;
  }

  drawAllText();
  // Tobi overlay is rendered in drawTobiTurboOverlay().
}




// ============================================================
// V27 NOFLICKER CAMP + WEB DOTS TOBI ENGINE
// Restored from the older stable Buzz spider behavior:
// flexible 36x36 acrobatic Tobi, fast movement, web from hands.
// The old white belt is removed. No rectangular wipe is used over the camp scene.
// ============================================================
void spiderShape(int x, int y, uint8_t mode, int frame, uint16_t sR, uint16_t sB, uint16_t sE) {
  // v26 slim Spider-Man: roughly half-width vs old 36px body, still human-like.
  // sR = red suit panels, sB = blue suit panels, sE = white eyes/web accents.
  if (mode == 0) { // upright crawl
    lcdFillRect(x+7,  y+0,  9, 10, sR);                         // mask
    lcdFillRect(x+8,  y+3,  2, 2, sE); lcdFillRect(x+13, y+3, 2, 2, sE);
    lcdFillRect(x+5,  y+10, 13, 12, sB);                        // blue torso sides
    lcdFillRect(x+8,  y+10,  7, 10, sR);                        // red chest
    lcdFillRect(x+11, y+13,  2, 4, C_BLACK);                    // small spider emblem
    if (frame == 0) {
      lcdFillRect(x+0,  y+9,  7, 4, sR); lcdFillRect(x+17, y+9, 7, 4, sR);
      lcdFillRect(x+3,  y+22, 4, 9, sB); lcdFillRect(x+16, y+22, 4, 9, sB);
    } else {
      lcdFillRect(x+2,  y+7,  5, 7, sR); lcdFillRect(x+17, y+7, 5, 7, sR);
      lcdFillRect(x+0,  y+20, 7, 4, sB); lcdFillRect(x+17, y+20, 7, 4, sB);
    }
  } else if (mode == 1 || mode == 5) { // ceiling / upside
    lcdFillRect(x+7,  y+22, 9, 10, sR);
    lcdFillRect(x+8,  y+27, 2, 2, sE); lcdFillRect(x+13, y+27, 2, 2, sE);
    lcdFillRect(x+5,  y+10, 13, 12, sB);
    lcdFillRect(x+8,  y+13, 7, 8, sR);
    lcdFillRect(x+11, y+16, 2, 4, C_BLACK);
    if (frame == 0) {
      lcdFillRect(x+0,  y+20, 7, 4, sR); lcdFillRect(x+17, y+20, 7, 4, sR);
      lcdFillRect(x+3,  y+1,  4, 9, sB); lcdFillRect(x+16, y+1, 4, 9, sB);
    } else {
      lcdFillRect(x+2,  y+18, 5, 7, sR); lcdFillRect(x+17, y+18, 5, 7, sR);
      lcdFillRect(x+0,  y+7,  7, 4, sB); lcdFillRect(x+17, y+7, 7, 4, sB);
    }
  } else if (mode == 2) { // right wall / flying right
    lcdFillRect(x+15, y+10, 9, 9, sR);
    lcdFillRect(x+20, y+11, 2, 2, sE); lcdFillRect(x+20, y+16, 2, 2, sE);
    lcdFillRect(x+6,  y+7,  10, 16, sB);
    lcdFillRect(x+9,  y+10, 7, 10, sR);
    lcdFillRect(x+11, y+14, 3, 2, C_BLACK);
    if (frame == 0) {
      lcdFillRect(x+14, y+0, 4, 8, sR); lcdFillRect(x+14, y+23, 4, 8, sR);
      lcdFillRect(x+0,  y+3, 7, 4, sB); lcdFillRect(x+0,  y+25, 7, 4, sB);
    } else {
      lcdFillRect(x+12, y+2, 7, 4, sR); lcdFillRect(x+12, y+25, 7, 4, sR);
      lcdFillRect(x+4,  y+0, 4, 8, sB); lcdFillRect(x+4,  y+23, 4, 8, sB);
    }
  } else if (mode == 3) { // left wall / flying left
    lcdFillRect(x+0,  y+10, 9, 9, sR);
    lcdFillRect(x+2,  y+11, 2, 2, sE); lcdFillRect(x+2,  y+16, 2, 2, sE);
    lcdFillRect(x+8,  y+7,  10, 16, sB);
    lcdFillRect(x+8,  y+10, 7, 10, sR);
    lcdFillRect(x+10, y+14, 3, 2, C_BLACK);
    if (frame == 0) {
      lcdFillRect(x+6,  y+0, 4, 8, sR); lcdFillRect(x+6,  y+23, 4, 8, sR);
      lcdFillRect(x+17, y+3, 7, 4, sB); lcdFillRect(x+17, y+25, 7, 4, sB);
    } else {
      lcdFillRect(x+5,  y+2, 7, 4, sR); lcdFillRect(x+5,  y+25, 7, 4, sR);
      lcdFillRect(x+16, y+0, 4, 8, sB); lcdFillRect(x+16, y+23, 4, 8, sB);
    }
  } else if (mode == 4) { // hanging/swinging front
    lcdFillRect(x+7,  y+11, 9, 10, sR);
    lcdFillRect(x+8,  y+16, 2, 2, sE); lcdFillRect(x+13, y+16, 2, 2, sE);
    lcdFillRect(x+5,  y+0,  13, 11, sB);
    lcdFillRect(x+8,  y+3,  7, 8, sR);
    lcdFillRect(x+11, y+5,  2, 4, C_BLACK);
    if (frame == 0) {
      lcdFillRect(x+0,  y+3,  7, 4, sR); lcdFillRect(x+17, y+3, 7, 4, sR);
      lcdFillRect(x+3,  y+19, 4, 9, sB); lcdFillRect(x+16, y+19, 4, 9, sB);
    } else {
      lcdFillRect(x+2,  y+2,  5, 7, sR); lcdFillRect(x+17, y+2, 5, 7, sR);
      lcdFillRect(x+0,  y+17, 7, 4, sB); lcdFillRect(x+17, y+17, 7, 4, sB);
    }
  }
}

void renderSpider(int x, int y, uint8_t mode, int frame) {
  // v26: no bulky black drop-shadow square; only a 1px silhouette plus red/blue suit.
  spiderShape(x+1, y+1, mode, frame, C_BLACK, C_BLACK, C_BLACK);
  spiderShape(x,   y,   mode, frame, C_RED, C_SPIDEY_BLUE, C_WHITE);
}

void eraseSpider(int x, int y, uint8_t mode, int frame) {
  // Exact-mask erase only. No rectangular wipe over knight/stalker/fire scene.
  spiderShape(x, y, mode, frame, C_BLACK, C_BLACK, C_BLACK);
  spiderShape(x+1, y+1, mode, frame, C_BLACK, C_BLACK, C_BLACK);
}

void stableTobiHand(uint8_t mode, int x, int y, bool rightHand, int &hx, int &hy) {
  // Hand points for the slim 24x32 body.
  if (mode == 2) { hx = x + (rightHand ? 18 : 13); hy = y + (rightHand ? 24 : 5); return; }
  if (mode == 3) { hx = x + (rightHand ? 6  : 11); hy = y + (rightHand ? 24 : 5); return; }
  if (mode == 0) { hx = x + (rightHand ? 20 : 4);  hy = y + (rightHand ? 11 : 11); return; }
  if (mode == 4) { hx = x + (rightHand ? 20 : 4);  hy = y + (rightHand ? 20 : 20); return; }
  hx = x + (rightHand ? 20 : 4); hy = y + (rightHand ? 20 : 20);
}

void eraseStableTobi() {
  eraseOldTobiWeb();
  if (spOldFrame != -1) {
    eraseSpider(spOldX, spOldY, spOldState, spOldFrame);
    // Keep restoration tight. Avoid a moving black square over the post-apoc camp scene.
    healZone(spOldX - 2, spOldY - 2, 30, 38);
  }
}

// ============================================================
// V24 STABLE TOBI TURBO OVERLAY
// Fast old movement: crawl walls/ceiling, Tarzan swing, zipline, camera-web.
// ============================================================
void drawTobiTurboOverlay() {
  bool act = playing && !softPaused;
  float bass = spectrum[0];
  uint32_t now = millis();

  eraseStableTobi();
  beginTobiWebFrame();

  // v26: faster motion, but with fewer web shots. Movement should read like a swinging hero, not a turret.
  spT += act ? (0.105f + bass * 0.20f) : 0.060f;

  if (tobiNextActionMs == 0) tobiNextActionMs = now + 3500;
  if (spOldState != 4 && spOldState != 5 && now > tobiNextActionMs) {
    // Alternate real swing and side-to-side zipline so he visibly rides the web like a swing.
    if ((now / 1000) & 1) {
      spOldState = 4;
      tarzanEndTime = now + 4300 + (janusRand() % 1200);
    } else {
      spOldState = 5;
      zipStartTime = now;
      tarzanEndTime = now + 2600;
      zipDir = (janusRand() & 1) ? 1 : -1;
    }
    spT = 0.0f;
    tobiNextActionMs = now + 9500 + (janusRand() % 6500);
  }

  int x = 0, y = 0;
  uint8_t spMode = spOldState;
  bool useRightHand = ((now / 420) & 1);  // slower hand switching = less unnatural arm twitch

  if (spMode == 4) {
    // Big pendulum swing from different screen anchors.
    const int ax[6] = {18, 58, 118, 182, 222, 96};
    const int ay[6] = {10, 22,  8,  24,  14, 34};
    int phase = (now / 900) % 6;
    int anchorX = ax[phase];
    int anchorY = ay[phase];
    float t = (now % 4300) / 4300.0f;
    float angle = sinf(t * PI * 2.0f) * (1.15f + bass * 0.28f);
    float radius = 70.0f + bass * 34.0f;
    x = anchorX + (int)(sinf(angle) * radius) - 12;
    y = anchorY + (int)(cosf(angle) * radius * 0.78f) + 12;
    x = constrain(x, 0, 216);
    y = constrain(y, 10, 266);
    spMode = (x < anchorX) ? 2 : 3;
    int hx, hy; stableTobiHand(spMode, x, y, useRightHand, hx, hy);
    drawTobiWebLine(anchorX, anchorY, hx, hy, C_WHITE);
    if (now > tarzanEndTime) { spOldState = 0; spT = fmodf(spT, 10.0f); }
  } else if (spMode == 5) {
    // Side-to-side zipline: clearly travels on a web between screen sides.
    float p = constrain((now - zipStartTime) / 2600.0f, 0.0f, 1.0f);
    float ease = 0.5f - 0.5f * cosf(p * PI);
    int anchorX = (zipDir > 0) ? 232 : 8;
    int anchorY = 22 + ((now / 900) & 1) * 24;
    x = (zipDir > 0) ? (int)(-22 + ease * 260.0f) : (int)(218 - ease * 260.0f);
    y = 42 + (int)(sinf(p * PI) * 34.0f) + (int)(bass * 12.0f);
    x = constrain(x, 0, 216);
    y = constrain(y, 8, 268);
    spMode = (zipDir > 0) ? 2 : 3;
    int hx, hy; stableTobiHand(spMode, x, y, useRightHand, hx, hy);
    drawTobiWebLine(anchorX, anchorY, hx, hy, C_WHITE);
    if (now > tarzanEndTime) { spOldState = 0; spT = fmodf(spT, 10.0f); }
  } else {
    // Wall/ceiling crawl loop, slim hero moving around the frame.
    float p = fmodf(spT * 0.72f, 4.0f);
    if (p < 1.0f) { spMode = 1; x = 8 + (int)(p * 202.0f); y = 7; }
    else if (p < 2.0f) { spMode = 3; x = 212; y = 8 + (int)((p - 1.0f) * 272.0f); }
    else if (p < 3.0f) { spMode = 0; x = 210 - (int)((p - 2.0f) * 202.0f); y = 282; }
    else { spMode = 2; x = 8; y = 280 - (int)((p - 3.0f) * 272.0f); }

    // Fewer web shots, from hands to varied screen points. No permanent ceiling dots spam.
    static uint32_t lastSmallWebMs = 0;
    bool webMoment = (now - lastSmallWebMs > (act ? 1700UL : 3200UL)) && (bass > 0.48f || (janusRand() % 5 == 0));
    if (webMoment) {
      lastSmallWebMs = now;
      int hx, hy; stableTobiHand(spMode, x, y, useRightHand, hx, hy);
      const int tx[12] = {18, 44, 82, 122, 166, 218, 225, 188, 136, 74, 22, 116};
      const int ty[12] = {18, 56, 16,  42,  28,  72, 178, 246, 300, 304, 238, 150};
      uint8_t k = (now / 700 + spMode * 2 + (useRightHand ? 5 : 0)) % 12;
      drawTobiWebLine(hx, hy, tx[k], ty[k], C_WHITE);
      if (act && bass > 0.86f && (janusRand() % 3 == 0)) drawTobiCameraShot(hx, hy, C_WHITE);
    }
  }

  x = constrain(x, 0, 216);
  y = constrain(y, 4, 288);
  int frame = ((int)(spT * 13.0f)) & 1;

  drawTobiWebDots();
  renderSpider(x, y, spMode, frame);
  commitTobiWebFrame();
  spOldX = x;
  spOldY = y;
  spOldState = spMode;
  spOldMode = spMode;
  spOldFrame = frame;
  spOldShot = false;
}

// ============================================================
// UNIFIED LED ENGINE 
// ============================================================
uint8_t gamma8(float v) { return constrain((int)v, 0, 255); }
uint32_t fixedColor(uint8_t r, uint8_t g, uint8_t b) {
  switch (LED_CHANNEL_FIX) {
    case 0: return strip.Color(r, g, b); case 1: return strip.Color(g, r, b); 
    case 2: return strip.Color(b, g, r); default: return strip.Color(b, r, g);
  }
}
uint32_t fireColor(float r, float g, float b) {
  float br = ledBright / 100.0f; return fixedColor(gamma8(r * br), gamma8(g * br), gamma8(b * br));
}


uint32_t hsvToFireRgb(float h, float s, float v) {
  h = h - floorf(h);
  float r=0,g=0,b=0;
  float i = floorf(h * 6.0f);
  float f = h * 6.0f - i;
  float p = v * (1.0f - s);
  float q = v * (1.0f - f * s);
  float t = v * (1.0f - (1.0f - f) * s);
  switch (((int)i) % 6) {
    case 0: r=v; g=t; b=p; break;
    case 1: r=q; g=v; b=p; break;
    case 2: r=p; g=v; b=t; break;
    case 3: r=p; g=q; b=v; break;
    case 4: r=t; g=p; b=v; break;
    default: r=v; g=p; b=q; break;
  }
  return fireColor(r*255.0f, g*255.0f, b*255.0f);
}

void generateProceduralPalette() {
  procPaletteSeed = esp_random() ^ micros() ^ (millis() << 7);
  procHueA = ((procPaletteSeed & 0xFFFF) / 65535.0f);
  procHueB = (((procPaletteSeed >> 8) & 0xFFFF) / 65535.0f);
  procSpeed = 0.008f + (((procPaletteSeed >> 16) & 0xFF) / 255.0f) * 0.055f;
  procContrast = 0.45f + (((procPaletteSeed >> 24) & 0x7F) / 127.0f) * 0.55f;
  Serial.printf("[LED] procedural palette seed=0x%08lx hA=%.3f hB=%.3f speed=%.4f contrast=%.2f\n",
                (unsigned long)procPaletteSeed, procHueA, procHueB, procSpeed, procContrast);
}

uint32_t proceduralPaletteColor(uint8_t idx, float phase, float heat, bool spark) {
  if (procPaletteSeed == 0) generateProceduralPalette();
  float mix = 0.5f + 0.5f * sinf(phase * (0.65f + procContrast) + idx * 0.73f);
  float hue = procHueA * (1.0f - mix) + procHueB * mix;
  hue += 0.08f * sinf(phase * 0.31f + idx);
  float sat = 0.62f + 0.36f * procContrast;
  float val = 0.10f + 0.90f * constrain(heat, 0.0f, 1.0f);
  if (spark) { sat = 0.20f; val = 1.0f; }
  return hsvToFireRgb(hue, sat, val);
}

uint32_t getPaletteColor(uint8_t pal, float phase, float heat, bool spark) {
  float r=0, g=0, b=0; float wave = 0.5f + 0.5f * sinf(phase);
  switch (pal % LED_PALETTE_COUNT) {
    case 0: 
      r = 5.0f + 200.0f * heat; g = 0.0f + 90.0f * powf(heat, 2.5f); b = 0;
      if(spark) { r=255; g=110; b=0; } break;
    case 1: 
      r = 10 * heat; g = 50 + 200 * heat; b = 5 * wave;
      if(spark) { r=100; g=255; b=50; } break;
    case 2: 
      r = 30 + 150 * heat; g = 10 * wave; b = 80 + 170 * heat;
      if(spark) { r=200; g=50; b=255; } break;
    case 3: 
      r = 20 + 180 * wave; g = 50 * heat; b = 100 + 150 * heat;
      if(spark) { r=255; g=100; b=255; } break;
    case 4: 
      r = 80 + 175 * heat; g = 30 + 100 * heat; b = 5 * wave;
      if(spark) { r=255; g=255; b=100; } break;
    case 5: 
      r = 0; g = 50 + 100 * heat; b = 100 + 155 * heat;
      if(spark) { r=100; g=200; b=255; } break;
    case 6: 
      r = 100 + 155 * heat; g = 0; b = 10 * wave;
      if(spark) { r=255; g=20; b=20; } break;
    case 7: 
      r = 0; g = 80 + 175 * heat; b = 20 * wave;
      if(spark) { r=50; g=255; b=50; } break;
    case 8:
      return proceduralPaletteColor(8, phase * (procSpeed / 0.025f), heat, spark);
  }
  return fireColor(r, g, b);
}

// v10.11H: ACCEPT LED reaction restored from the uploaded v10.6C old sketch 1:1.
void updateUnifiedLED() {
  uint32_t rnd = janusRand();
  uint32_t now = millis();
  bool act = playing && !softPaused;

  float shareFlare = 0.0f;
  uint32_t until = ledShareFlareUntilMs;
  uint8_t flareKind = ledShareFlareKind;
  if (until > now) {
    float dur = (flareKind >= 2) ? 1800.0f : 1150.0f;
    shareFlare = constrain((float)(until - now) / dur, 0.0f, 1.0f);
  }

  for(int i=0; i<LED_NUM; i++) {
    float geo = (i==3) ? 1.3f : ((i==2||i==4)? 0.85f : ((i==1||i==5)?0.65f:0.35f));
    float targetHeat;
    float musicBand = 0.0f; 

    if (act) {
        musicBand = spectrum[(i*2 + (ledFrame/4)) % 16]; 
        targetHeat = (0.02f + musicBand * 1.6f) * geo; 
    } else {
        float slow = 0.5f + 0.5f * sinf(ledFrame * 0.015f + i * 1.1f);
        float fast = ((rnd >> ((i%4)*8)) & 0xFF) / 255.0f;
        targetHeat = (0.1f + 0.4f * slow + 0.15f * fast) * geo;
    }

    // Share flare: like throwing dry wood into the fire.
    if (shareFlare > 0.0f) {
      float kick = (0.45f + shareFlare * 2.10f) * geo;
      if (flareKind >= 2) kick *= 1.35f;
      targetHeat += kick;
    }

    if (targetHeat > bonfireHeat[i]) bonfireHeat[i] = bonfireHeat[i] * 0.25f + targetHeat * 0.75f; 
    else bonfireHeat[i] = bonfireHeat[i] * 0.92f + targetHeat * 0.08f; 

    bool spark = act ? ((musicBand > 0.7f) && ((rnd & 0xFF) == (uint32_t)(i * 73 + 11))) : ((rnd & 0x1FFF) == (uint32_t)(i * 73 + 11));
    if (shareFlare > 0.0f) {
      spark = spark || (i == 3) || (((rnd >> (i * 3)) & 0x07) == 0);
    }

    uint32_t col = getPaletteColor(ledPalette, ledFrame * 0.038f + i * 0.82f, bonfireHeat[i], spark);
    if (shareFlare > 0.0f) {
      float center = (i == 3) ? 1.0f : ((i == 2 || i == 4) ? 0.75f : 0.45f);
      float hot = constrain(shareFlare * center, 0.0f, 1.0f);
      if (hot > 0.18f) {
        // v10.11H: keep old v10.6C flare motion/heat, but make ACCEPT color amber-gold.
        // The old greenish component was too high on this LED channel map.
        float r = 225.0f + 30.0f * hot;
        float g = 48.0f + 92.0f * hot;
        float b = (flareKind >= 2) ? 10.0f * hot : 2.0f * hot;
        if (flareKind >= 2 && hot > 0.78f) {
          r = 255.0f;
          g = 150.0f;
          b = 14.0f;
        }
        col = fireColor(r, g, b);
      }
    }
    strip.setPixelColor(i, col);
  }
  strip.show();
}

void updateLED() { ledFrame++; updateUnifiedLED(); }

void showPalettePopup() {
  snprintf(popupText, sizeof(popupText), "PAL %u", ledPalette);
  popupColor = getThemeColor();
  popupTimer = millis() + 1600;
  popupIsVol = false;
  popupShowBar = false;
}
void cycleLedPalette() {
  uint8_t old = ledPalette;
  ledPalette = (ledPalette + 1) % LED_PALETTE_COUNT;
  if (ledPalette == LED_PROC_PALETTE_INDEX && old != ledPalette) generateProceduralPalette();
  janusPersistSettings();
  uiOldPal = ledPalette;
  showPalettePopup();
}
void prevLedPalette() {
  uint8_t old = ledPalette;
  ledPalette = (ledPalette + LED_PALETTE_COUNT - 1) % LED_PALETTE_COUNT;
  if (ledPalette == LED_PROC_PALETTE_INDEX && old != ledPalette) generateProceduralPalette();
  janusPersistSettings();
  uiOldPal = ledPalette;
  showPalettePopup();
}

// ============================================================
// AUDIO & SPECTRUM
// ============================================================
void updateSpectrumModel() {
  uint32_t t = millis(); float br = constrain((float)bitrateKbps / 128.0f, 0.5f, 1.5f);
  for (int i=0; i<16; i++) {
    float pulse = powf(sinf(t * (0.003f + i*0.0002f)), 6.0f); 
    float noise = (float)((janusRand() >> (i%4)) & 0xFF) / 255.0f;
    float target = (pulse * 0.7f + noise * 0.3f) * br;
    if (!(playing && !softPaused)) target *= 0.15f; 
    if (target > spectrum[i]) spectrum[i] = spectrum[i] * 0.3f + target * 0.7f; 
    else spectrum[i] = spectrum[i] * 0.88f + target * 0.12f;
    if (spectrum[i] > 1.0f) spectrum[i] = 1.0f;
  }
}

bool httpGet(const char* url, String* out=nullptr) {
  if(WiFi.status()!=WL_CONNECTED) return false;
  HTTPClient http; http.setTimeout(2000);
  if(!http.begin(url)) return false;
  int code=http.GET(); String body=http.getString(); http.end();
  if(out) *out=body; return code>=200 && code<300;
}

long jsonNumberAfter(const String& b, const char* key, long fallback) {
  int p = b.indexOf(key);
  if (p < 0) return fallback;
  int c = b.indexOf(':', p);
  if (c < 0) return fallback;
  int s = c + 1;
  while (s < (int)b.length() && (b[s] == ' ' || b[s] == '"' || b[s] == '\t')) s++;
  int e = s;
  while (e < (int)b.length() && ((b[e] >= '0' && b[e] <= '9') || b[e] == '.')) e++;
  if (e <= s) return fallback;
  float v = b.substring(s, e).toFloat();
  if (v <= 0) return fallback;
  // API may return seconds or milliseconds.
  if (v < 20000.0f) v *= 1000.0f;
  return (long)v;
}

void updateTrackInfo() {
  String b; if(!httpGet(CURRENT_URL,&b)) return;

  bool changed = false;
  const char* keys[] = {"\"filename\"", "\"title\"", "\"name\"", "\"track\"", "\"current\"", "\"song\"", "\"path\"", "\"url\""};
  for (uint8_t ki = 0; ki < sizeof(keys) / sizeof(keys[0]); ++ki) {
    int p = b.indexOf(keys[ki]);
    if (p < 0) continue;
    int c = b.indexOf(':', p);
    int q1 = b.indexOf('"', c + 1);
    int q2 = b.indexOf('"', q1 + 1);
    if (q1 > 0 && q2 > q1) {
      String s = b.substring(q1 + 1, q2);
      s.replace("\\/", "/");
      int slash = s.lastIndexOf('/');
      if (slash >= 0 && slash < (int)s.length() - 1) s = s.substring(slash + 1);
      if (s.length() > 0 && s != "null") {
        char oldName[96];
        strlcpy(oldName, trackName, sizeof(oldName));
        s.toCharArray(trackName, 96);
        changed = strcmp(oldName, trackName) != 0;
      }
      break;
    }
  }

  long dur = jsonNumberAfter(b, "\"duration\"", -1);
  if (dur < 0) dur = jsonNumberAfter(b, "\"duration_ms\"", -1);
  if (dur < 0) dur = jsonNumberAfter(b, "\"length\"", -1);
  if (dur >= 30000L && dur <= 30L*60L*1000L) trackDurationMs = (uint32_t)dur;

  if (changed) {
    trackVisualStartedAtMs = millis();
    trackVisualPausedAtMs = 0;
    trackVisualPausedTotalMs = 0;
  }
}

void audio_info(const char* info){
  Serial.printf("[Audio] %s\n", info);
}

void audio_bitrate(const char* info){
  int v = atoi(info);
  if (v > 0) bitrateKbps = v / 1000;
}

void audio_eof_mp3(const char* info){
  Serial.printf("[Audio] EOF callback: %s\n", info ? info : "-");
  audioEofFlag = true;
}

void audioSetupPins() {
  // v10.11N1: setPinout must be one-shot per boot unless we intentionally changed DOUT.
  // Repeating it during play/next/recovery leaks/duplicates the I2S channel on core 3.x.
  if (!janusAudioPinsConfigured || janusAudioPinsDoutConfigured != audioDoutPin) {
    audio.setPinout(I2S_BCLK, I2S_LRCK, audioDoutPin, I2S_MCLK);
    janusAudioPinsConfigured = true;
    janusAudioPinsDoutConfigured = audioDoutPin;
    Serial.printf("[Audio] pinout INIT BCLK=%d LRCK=%d DOUT=%u MCLK=%d vol=%u\n", I2S_BCLK, I2S_LRCK, audioDoutPin, I2S_MCLK, volumeVal);
  } else {
    Serial.printf("[Audio] pinout reuse DOUT=%u vol=%u run=%d\n", audioDoutPin, volumeVal, audio.isRunning() ? 1 : 0);
  }
  audio.setVolume(softPaused ? 0 : volumeVal);
}

void printAudioDiag(const char* tag) {
  uint16_t tcaIn = tcaRead(0x00);
  Wire.beginTransmission(ES8311_ADDR);
  bool codec = (Wire.endTransmission() == 0);
  Serial.printf("[AudioDiag] %s wifi=%d wanted=%d playing=%d pause=%d run=%d vol=%u heap=%u psram=%u psramFound=%d TCAin=0x%04X out=0x%04X cfg=0x%04X ES8311=%d\n",
                tag ? tag : "-",
                WiFi.status() == WL_CONNECTED,
                wanted, playing, softPaused, audio.isRunning(), volumeVal,
                ESP.getFreeHeap(), ESP.getFreePsram(), psramFound() ? 1 : 0,
                tcaIn, tcaOutput, tcaConfig, codec ? 1 : 0);
}

void audioAmpKick(const char* reason) {
  Serial.printf("[Audio] PA kick: %s\n", reason ? reason : "-");
  setupTCA();
  tcaWritePin(EXIO_AUDIO_PA, false);
  delay(80);
  tcaWritePin(EXIO_AUDIO_PA, true);
  delay(140);
  printAudioDiag("after-pa-kick");
}

void janusMusicAdvanceHops(uint8_t hops, const char* reason, uint16_t delayMs = 95);
uint8_t janusRandomPlaylistHops();
void janusApplyFirstPlayRandom(const char* reason);

void connectAudioStreamFresh(const char* reason, bool hardAmpKick) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[Audio] connect skipped: WiFi offline");
    return;
  }

  audioBusySwitching = true;
  audioCriticalUntilMs = millis() + 9000UL;
#if JANUS_SWARMSENSE_AUDIO_SAFE && JANUS_SWARMSENSE_RESET_QUEUE_ON_AUDIO_START
  janusDropSwarmSenseBacklog("audio-start");
#endif
  audioEofFlag = false;

  Serial.printf("[Audio] FRESH START reason=%s hardKick=%d url=%s\n", reason ? reason : "-", hardAmpKick ? 1 : 0, STREAM_URL);

  // Apply random track selection at the only moment that matters: right before /stream opens.
  // This keeps boot sound stable and prevents NAS from falling back to track #1.
  janusApplyFirstPlayRandom(reason);

  audio.stopSong();
  // Let Audio.h close HTTP buffers, but do not reallocate I2S pinout below.
  delay(220);

  if (hardAmpKick) audioAmpKick(reason);
  else {
    setupTCA();
    tcaWritePin(EXIO_AUDIO_PA, true);
    delay(140);
  }

  initES8311();
  delay(100);
  audioSetupPins();
  audio.setVolume(volumeVal);

  audio.connecttohost(STREAM_URL);

  streamStartedAt = millis();
  audioLastRunningMs = streamStartedAt;
  audioLastRecoveryMs = streamStartedAt;
  audioRecoveryAttempts = 0;
  audioHadStableRun = false;
  audioStableSinceMs = 0;
  audioSoftPauseAtMs = 0;
  wanted = true;
  playing = true;
  softPaused = false;
  audioUserPaused = false;

  updateTrackInfo();
  printAudioDiag("after-connect");
  audioBusySwitching = false;
}

void janusMusicAdvanceHops(uint8_t hops, const char* reason, uint16_t delayMs) {
  if (WiFi.status() != WL_CONNECTED) return;
  if (hops < 1) hops = 1;
  Serial.printf("[Audio] PLAYLIST advance hops=%u reason=%s url=%s\n", hops, reason ? reason : "-", NEXT_URL);
  for (uint8_t i = 0; i < hops; ++i) {
    String body;
    bool ok = httpGet(NEXT_URL, &body);
    Serial.printf("[Audio] /next %u/%u ok=%d body=%s\n", (unsigned)(i + 1), (unsigned)hops, ok ? 1 : 0, body.substring(0, 48).c_str());
    delay(delayMs);
    yield();
  }
  delay(180);
  updateTrackInfo();
}

uint8_t janusRandomPlaylistHops() {
  uint8_t maxHops = audioShuffleHopsMax;
  if (maxHops < 3) maxHops = 9;
  // 2..maxHops+1: never 0 and never just "stay on first".
  return (uint8_t)(2 + (janusMixedRandom32() % maxHops));
}

void janusApplyFirstPlayRandom(const char* reason) {
  if (!janusFirstPlayRandomPending || janusFirstPlayRandomDone) return;
  if (WiFi.status() != WL_CONNECTED) return;
  janusFirstPlayRandomPending = false;
  janusFirstPlayRandomDone = true;
  uint8_t hops = janusRandomPlaylistHops();
  Serial.printf("[Audio] FIRST-PLAY RANDOM armed reason=%s hops=%u\n", reason ? reason : "-", (unsigned)hops);
  janusMusicAdvanceHops(hops, "first-play-random", 110);
}

void janusBootShufflePlaylist() {
  static bool done = false;
  if (done) return;
  done = true;
  if (WiFi.status() != WL_CONNECTED) return;

  // v10.11N2: keep this as a light pre-warm only. The real random jump happens
  // immediately before the first stream connection, because some NAS stream sessions
  // can reset their cursor after boot-time-only /next calls.
  janusBootEntropySalt ^= esp_random() ^ micros() ^ (uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFFULL);
  Serial.println("[Audio] BOOT playlist random deferred until first PLAY");
  updateTrackInfo();
}

void callNextEndpointRandomized(bool randomize, const char* reason) {
  if (WiFi.status() != WL_CONNECTED) return;

  uint8_t hops = 1;
  if (randomize) {
    // Infinite shuffled cycle using NAS /next endpoint only.
    hops = 1 + (janusMixedRandom32() % max((uint8_t)2, audioShuffleHopsMax));
  }

  janusMusicAdvanceHops(hops, reason ? reason : "next", 70);
}

void nextTrackAuto(const char* reason) {
  if (!wanted || softPaused || audioUserPaused) return;
  if (millis() - audioLastAutoNextMs < 2500) return;
  audioLastAutoNextMs = millis();

  callNextEndpointRandomized(true, reason);
  connectAudioStreamFresh(reason ? reason : "auto-next", true);
}

void nextTrackManual() {
  String b; httpGet(NEXT_URL,&b);
  if(wanted||playing||softPaused) {
    audioUserPaused = false;
    connectAudioStreamFresh("manual-next", true);
  }
}

void prevTrackManual() {
  String b; httpGet(PREV_URL,&b);
  if(wanted||playing||softPaused) {
    audioUserPaused = false;
    connectAudioStreamFresh("manual-prev", true);
  }
}

void nextTrack() { nextTrackManual(); }
void prevTrack() { prevTrackManual(); }

void startStream() {
  if(WiFi.status()!=WL_CONNECTED) return;

  // True resume: do not reconnect, do not restart track, just unmute and reopen PA/codec.
  if (softPaused && wanted) {
    setupTCA();
    tcaWritePin(EXIO_AUDIO_PA, true);
    delay(60);
    initES8311();
    audioSetupPins();
    audio.setVolume(volumeVal);

    softPaused = false;
    playing = true;
    audioUserPaused = false;
    audioBusySwitching = false;
    audioEofFlag = false;
    audioRecoveryAttempts = 0;
    streamStartedAt = millis();
    audioLastRunningMs = millis();
    audioSoftPauseAtMs = 0;

    Serial.println("[Audio] RESUME same track");
    printAudioDiag("resume-same-track");
    return;
  }

  connectAudioStreamFresh("play", true);
}

void stopStream() {
  if (!wanted) return;

  // Real pause: keep Audio.h stream alive and muted. Autopilot must stay silent.
  audioUserPaused = true;
  softPaused = true;
  playing = false;
  audioSoftPauseAtMs = millis();
  audio.setVolume(0);

  // Do NOT audio.stopSong() here. That would lose the current HTTP stream/track.
  Serial.println("[Audio] SOFT PAUSE same track: recovery/autonext disabled");
  printAudioDiag("soft-pause");
}

void hardAudioRestartSameTrack(const char* reason) {
  if (!wanted || softPaused || audioUserPaused) return;
  if (millis() - audioLastRecoveryMs < 4500) return;
  audioLastRecoveryMs = millis();
  audioRecoveryAttempts++;
  Serial.printf("[Audio] recovery same-stream attempt=%u reason=%s\n", audioRecoveryAttempts, reason ? reason : "-");
  connectAudioStreamFresh(reason ? reason : "recovery", true);
}

void audioAutopilotTick(uint32_t now) {
  if (!wanted) return;

  // Keep the decoder/service loop alive even during soft pause, so resume can continue.
  audio.loop();

  // Manual pause means silence is intentional. No recovery, no next-track.
  if (softPaused || audioUserPaused || !playing || audioBusySwitching) return;

  bool running = audio.isRunning();
  if (running) {
    audioLastRunningMs = now;
    if (!audioHadStableRun && now - streamStartedAt > 8000) {
      audioHadStableRun = true;
      audioStableSinceMs = now;
    }
    if (now - streamStartedAt > 15000) audioRecoveryAttempts = 0;
    return;
  }

  // EOF callback means track ended: always advance, never restart same song.
  if (audioEofFlag && now - streamStartedAt > 5000) {
    audioEofFlag = false;
    Serial.println("[Audio] EOF -> shuffled next");
    nextTrackAuto("eof");
    return;
  }

  if (now - streamStartedAt < 12000) return;

  uint32_t playedFor = now - streamStartedAt;
  uint32_t silentFor = now - audioLastRunningMs;

  // Natural end detection. In your NAS tracks this often appears as !isRunning()
  // without a reliable EOF callback, so treat a stable run followed by silence as end.
  if ((audioHadStableRun && playedFor > 25000 && silentFor > 1800) ||
      (playedFor > 55000 && silentFor > 1200)) {
    Serial.println("[Audio] natural end -> shuffled next");
    nextTrackAuto("natural-end");
    return;
  }

  // Early header/network stall: two same-stream repairs, then shuffled next.
  if (silentFor > 7000) {
    if (audioRecoveryAttempts < 2) {
      hardAudioRestartSameTrack("early-stall");
    } else {
      Serial.println("[Audio] repeated stall -> shuffled next");
      audioRecoveryAttempts = 0;
      nextTrackAuto("repeated-stall");
    }
  }
}

// ============================================================
// SETUP & LOOP
// ============================================================
void initOptionalCamera() {
#if JANUS_CAMERA_ENABLE
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_1;
  config.ledc_timer = LEDC_TIMER_1;
  config.pin_d0 = CAM_PIN_D0;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d7 = CAM_PIN_D7;
  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_pclk = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;
  config.xclk_freq_hz = 10000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;
  config.jpeg_quality = 14;
  config.fb_count = psramFound() ? 2 : 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;
  esp_err_t err = esp_camera_init(&config);
  Serial.printf("[CAM] init %s err=0x%x\n", err == ESP_OK ? "OK" : "FAIL", (unsigned)err);
#else
  Serial.println("[CAM] disabled. Set JANUS_CAMERA_ENABLE 1 for OV2640-compatible replacement module.");
#endif
}



#if JANUS_CAMERA_ENABLE
// JQ-V360-M12 v3.0 on Waveshare ESP32-S3-Audio-Board camera FPC.
// Enable only after audio/miner are stable: camera consumes PSRAM/CPU.
#define CAM_PIN_D0     2
#define CAM_PIN_D1     17
#define CAM_PIN_D2     18
#define CAM_PIN_D3     39
#define CAM_PIN_D4     45
#define CAM_PIN_D5     46
#define CAM_PIN_D6     47
#define CAM_PIN_D7     48
#define CAM_PIN_PCLK   44
#define CAM_PIN_VSYNC  21
#define CAM_PIN_HREF   1
#define CAM_PIN_XCLK   43
#define CAM_PIN_SIOD   11
#define CAM_PIN_SIOC   10
#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1

bool initJanusCamera() {
  camera_config_t config;
  memset(&config, 0, sizeof(config));
  config.ledc_channel = LEDC_CHANNEL_1;
  config.ledc_timer = LEDC_TIMER_1;
  config.pin_d0 = CAM_PIN_D0;
  config.pin_d1 = CAM_PIN_D1;
  config.pin_d2 = CAM_PIN_D2;
  config.pin_d3 = CAM_PIN_D3;
  config.pin_d4 = CAM_PIN_D4;
  config.pin_d5 = CAM_PIN_D5;
  config.pin_d6 = CAM_PIN_D6;
  config.pin_d7 = CAM_PIN_D7;
  config.pin_xclk = CAM_PIN_XCLK;
  config.pin_pclk = CAM_PIN_PCLK;
  config.pin_vsync = CAM_PIN_VSYNC;
  config.pin_href = CAM_PIN_HREF;
  config.pin_sccb_sda = CAM_PIN_SIOD;
  config.pin_sccb_scl = CAM_PIN_SIOC;
  config.pin_pwdn = CAM_PIN_PWDN;
  config.pin_reset = CAM_PIN_RESET;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size = FRAMESIZE_QQVGA;
  config.jpeg_quality = 18;
  config.fb_count = 1;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] JQ-V360 init failed 0x%x\n", err);
    return false;
  }
  Serial.println("[CAM] JQ-V360-M12 v3.0 ready");
  return true;
}
#endif


// ============================================================
// SETUP & LOOP
// ============================================================

// v10.11H: macro avoids Arduino auto-prototype bug with custom Btn& type.
#define JANUS_UPDATE_BTN(_b, _down) do { \
  (_b).prev = (_b).now; \
  (_b).now = (_down); \
  (_b).pressed = ((_b).now && !(_b).prev); \
  (_b).released = (!(_b).now && (_b).prev); \
  if ((_b).pressed) { (_b).downAt = millis(); (_b).longFired = false; } \
} while (0)

void janusShowPopup(const char* text, uint16_t color, bool isVol, bool showBar) {
  strlcpy(popupText, text ? text : "-", sizeof(popupText));
  popupColor = color;
  popupTimer = millis() + 1600;
  popupIsVol = isVol;
  popupShowBar = showBar;
}

void commitAudioVolume(const char* reason) {
  volumeVal = constrain(volumeVal, (uint8_t)0, (uint8_t)21);
  if (!softPaused) audio.setVolume(volumeVal);
  janusPersistSettings();

  char b[16];
  snprintf(b, sizeof(b), "VOL %u", volumeVal);
  janusShowPopup(b, C_CYAN, true, true);

  Serial.printf("[Audio] volume=%u reason=%s paused=%d\n",
                volumeVal, reason ? reason : "-", softPaused ? 1 : 0);
}

void janusVolumeStep(int delta, const char* reason) {
  int v = (int)volumeVal + delta;
  if (v < 0) v = 0;
  if (v > 21) v = 21;
  volumeVal = (uint8_t)v;
  commitAudioVolume(reason);
}

void applyJanusControl(const JanusControlPacket& cp) {
  String target = String(cp.target);
  String cmd = String(cp.command);
  target.toLowerCase();
  cmd.toLowerCase();

  if (!(target == "buzz" || target == "buzzlighter" || target == "all" || target == "janus")) return;

  Serial.printf("[CORE2CTRL] src=%s target=%s cmd=%s value=%ld seq=%lu\n",
                cp.source, cp.target, cp.command, (long)cp.value, (unsigned long)cp.seq);

  if (cmd == "play_pause" || cmd == "toggle") {
    if ((playing || wanted) && !softPaused) stopStream();
    else startStream();
    janusShowPopup(softPaused ? "PAUSE" : "PLAY", C_CYAN, false, false);
  } else if (cmd == "play" || cmd == "resume") {
    startStream();
    janusShowPopup("PLAY", C_CYAN, false, false);
  } else if (cmd == "pause") {
    stopStream();
    janusShowPopup("PAUSE", C_AMBER, false, false);
  } else if (cmd == "next") {
    nextTrackManual();
    janusShowPopup("NEXT", C_GREEN, false, false);
  } else if (cmd == "prev" || cmd == "previous") {
    prevTrackManual();
    janusShowPopup("PREV", C_GREEN, false, false);
  } else if (cmd == "vol_up" || cmd == "volume_up") {
    janusVolumeStep(+1, "core2");
  } else if (cmd == "vol_down" || cmd == "volume_down") {
    janusVolumeStep(-1, "core2");
  } else if (cmd == "volume_set") {
    int v = constrain((int)cp.value, 0, 21);
    volumeVal = (uint8_t)v;
    commitAudioVolume("core2-set");
  } else if (cmd == "status") {
    printAudioDiag("core2-status");
    colonyLastBuzzStatusMs = 0;
    sendBuzzStatusPacket();
  }
}

void commitLcdBrightness(const char* reason) {
  ledBright = constrain(ledBright, (uint8_t)0, (uint8_t)100);
  updateLcdBrightness();
  janusPersistSettings();

  char b[16];
  if (ledBright == 0) snprintf(b, sizeof(b), "LUM OFF");
  else snprintf(b, sizeof(b), "LUM %u", ledBright);
  janusShowPopup(b, (ledBright == 0) ? C_RED : C_AMBER, false, true);

  Serial.printf("[UI] brightness=%u reason=%s\n", ledBright, reason ? reason : "-");
}

void janusBrightnessStep(int delta, const char* reason) {
  int v = (int)ledBright + delta;
  if (v < 0) v = 0;
  if (v > 100) v = 100;
  ledBright = (uint8_t)v;
  commitLcdBrightness(reason);
}


void pollButtons() {
  JANUS_UPDATE_BTN(key1, !tcaReadPin(EXIO_KEY1));
  JANUS_UPDATE_BTN(key2, !tcaReadPin(EXIO_KEY2));
  JANUS_UPDATE_BTN(key3, !tcaReadPin(EXIO_KEY3));

  if (key1.pressed) {
    if ((playing || wanted) && !softPaused) stopStream();
    else startStream();
  }

  bool pausedOrIdle = softPaused || !wanted || !playing;

  if (pausedOrIdle) {
    if (key2.now && !key2.longFired && millis() - key2.downAt > 800) {
      key2.longFired = true;
      prevLedPalette();
    }
    if (key3.now && !key3.longFired && millis() - key3.downAt > 800) {
      key3.longFired = true;
      cycleLedPalette();
    }
    if (key2.released && !key2.longFired) janusBrightnessStep(-5, "key2-idle");
    if (key3.released && !key3.longFired) janusBrightnessStep(+5, "key3-idle");
    return;
  }

  // Playing mode:
  // short KEY2/KEY3 = real volume control
  // long KEY2/KEY3 = previous/next track
  if (key2.now && !key2.longFired && millis() - key2.downAt > 800) {
    key2.longFired = true;
    prevTrack();
  }
  if (key3.now && !key3.longFired && millis() - key3.downAt > 800) {
    key3.longFired = true;
    nextTrack();
  }

  if (key2.released && !key2.longFired) janusVolumeStep(-1, "key2");
  if (key3.released && !key3.longFired) janusVolumeStep(+1, "key3");
}

void handleSerialCommands() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == 'p') { if((playing || wanted) && !softPaused) stopStream(); else startStream(); }
    else if (c == 'S') { audio.stopSong(); wanted=false; playing=false; softPaused=false; audioUserPaused=false; Serial.println("[Audio] HARD STOP"); }
    else if (c == 'A') connectAudioStreamFresh("serial-A-hard", true);
    else if (c == 'a') audioAmpKick("serial-a");
    else if (c == 'n') nextTrackManual();
    else if (c == 'b') prevTrackManual();
    else if (c == 'd') printAudioDiag("serial-d");
    else if (c == 'o') {
      audioDoutPin = (audioDoutPin == 16) ? 15 : 16;
      janusPersistSettings();
      Serial.printf("[Audio] toggled DOUT=%u; reconnecting Audio.h MP3 path if wanted\n", audioDoutPin);
      // DOUT change is the only time pinout is allowed to re-init. Stop first and let
      // the old channel settle, then force audioSetupPins() to do one new setPinout().
      audio.stopSong();
      delay(260);
      janusAudioPinsConfigured = false;
      janusAudioPinsDoutConfigured = 0;
      if (wanted || playing || softPaused) connectAudioStreamFresh("serial-dout-toggle", true);
      else audioSetupPins();
    }
    else if (c == 'R') { generateProceduralPalette(); if (ledPalette != LED_PROC_PALETTE_INDEX) ledPalette = LED_PROC_PALETTE_INDEX; janusPersistSettings(); drawStaticUI(); showPalettePopup(); }
    else if (c == 'C') {
      bool jobReadySnap = false;
      char jobTextSnap[18] = "-";
      if (colonyJobLock(1)) {
        jobReadySnap = colonyMasterJobReady;
        strlcpy(jobTextSnap, colonyMasterJobText, sizeof(jobTextSnap));
        colonyJobUnlock();
      }
      Serial.printf("[COLONY] jobReady=%d job=%s rx=%lu queued=%lu drop=%lu weak=%lu dup=%lu legacy=%lu entropyReports=%lu entropy=%.2f ackBits=0x%08lx aiBatch=%u activeBatch=%u channel=%u\n",
      jobReadySnap ? 1 : 0, jobTextSnap,
      (unsigned long)colonyRxCount,
      (unsigned long)colonyRemoteSharesQueued,
      (unsigned long)colonyRemoteSharesDropped,
      (unsigned long)colonyRemoteWeakDrops,
      (unsigned long)colonyRemoteDuplicateDrops,
      (unsigned long)colonyRemoteLegacySeen,
      (unsigned long)colonyEntropyReports,
      colonyEntropyAvg,
      (unsigned long)colonyEchoAckBits,
      (unsigned)colonyAiBatch,
      (unsigned)effectiveMiningBatch(),
      (unsigned)colonyPeerChannel);
    }
    else if (c == '+') { janusVolumeStep(+1, "serial+"); }
    else if (c == '-') { janusVolumeStep(-1, "serial-"); }
    else if (c == '0') { ledBright = 0; commitLcdBrightness("serial-0"); }
    else if (c == 'F') { janusSdPrintStatus(); }
    else if (c == 'X') { janusSdRetentionTick(true); janusSdPrintStatus(); }
    else if (c == 'L') { janusSdListSerial(JANUS_SD_ROOT); }
    else if (c == 'T') { janusSdPrintTailSerial(JANUS_SD_ROOT "/logs/buzz.csv", 8192); }
    else if (c == 'G') { janusSdPrintFileSerial(JANUS_SD_ROOT "/state/buzz.cfg", 4096); }
    else if (c == 'P') { janusFarmPrintState(); janusSdPrintFileSerial(JANUS_FARM_STATE_FILE, 4096); }
    else if (c == 'Y') { janusFarmSaveState(true); janusFarmPrintState(); }
    else if (c == 'B') {
      Serial.printf("[BAT] pct=%d raw=%d pinMv=%d bright=%u\n",
                    readBatteryClamped(), readBatteryRawAdc(), readBatteryPinMv(), ledBright);
    }
    else if (c == 'M') {
      Serial.printf("[MINERDBG] H=%lu local=%lu total=%llu cand=%lu lastBits=%u shares=%lu submit=%lu remote=%lu reject=%lu low=%lu stale=%lu other=%lu diff=%.8f targetBits=%u status=%s lastNonce=%s err=%s\n",
        (unsigned long)minerRealHashrate,
        (unsigned long)minerLocalHashrate,
        (unsigned long long)minerTotalHashes,
        (unsigned long)minerShareCandidates,
        (unsigned)minerLastCandidateBits,
        (unsigned long)minerShares,
        (unsigned long)minerSubmitAttempts,
        (unsigned long)minerRemoteSubmitAttempts,
        (unsigned long)minerSubmitRejects,
        (unsigned long)minerLowDiffRejects,
        (unsigned long)minerStaleJobRejects,
        (unsigned long)minerOtherRejects,
        minerCurrentDiffF,
        (unsigned)minerShareTargetBits,
        minerStatus,
        minerLastSubmitNonce,
        minerLastRejectReason);
    }
  }
}


void safeLittleFSBeginOptional() {
#if JANUS_SAFE_LITTLEFS_ENABLE
  bool ok = LittleFS.begin(false);
  if (!ok) {
    Serial.println("[FS] LittleFS mount failed; formatting once");
    LittleFS.end();
    delay(50);
    LittleFS.format();
    ok = LittleFS.begin(false);
  }
  if (ok) {
    Serial.printf("[FS] LittleFS ok total=%lu used=%lu\n",
                  (unsigned long)LittleFS.totalBytes(),
                  (unsigned long)LittleFS.usedBytes());
  } else {
    Serial.println("[FS] LittleFS disabled this boot");
  }
#else
  // Disabled by default: Buzz does not require FS for audio/mining/colony.
#endif
}


// Raw PCM task removed in v37; Audio.h runs from audioAutopilotTick().



void setup() {
  Serial.begin(115200); delay(1000);
  Serial.println("[MINER] v10.11N3D SAFE-CHARGE WIFI-GUARD + N3C FARM MEMORY");
  Serial.println("[MINER] v10.11M Colony: stack-safe loop + locked ESP-NOW + 5GB SD retention");
  Serial.println("[MINER] v10.6 EXPECTED: no CAND/SUBMIT with bits lower than targetBits");
  Serial.println("\n[JANUS] BUZZ COLONY MASTER AUDIO v40.9 TACHYON STABLE COLONY");
  initJanusWorkerName();
  Serial.printf("[JANUS] Worker: %s\n", MINER_USER);
  Serial.println("[Audio] PROVEN ROUTE: /stream MP3 -> Audio.h -> ES8311/PA");
  Serial.println("[Audio] keys: KEY1 play/pause, KEY2 vol-/prev long, KEY3 vol+/next long; Serial +/- vol, M miner dbg");
  Serial.println("[MINER] v40: Public-Pool tickets >=32 bits + verified ESP-NOW remote shares");
  Serial.println("[COLONY] Buzz is master: ESP-NOW job beacon + verified share relay + Core2 remote control");
  safeLittleFSBeginOptional();

  Wire.begin(SDA_PIN, SCL_PIN); Wire.setClock(100000);
  setupTCA();
  janusSdBegin();
  minerLastStatsCsvMs = millis();  // v10.11H: no immediate CSV burst at boot.
  strip.begin(); strip.setBrightness(255); strip.clear(); strip.show();
#if BATTERY_ADC_PIN >= 0
  analogReadResolution(12);
  analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
#endif
#if JANUS_PORT_NTC_PIN >= 0
  analogReadResolution(12);
  analogSetPinAttenuation(JANUS_PORT_NTC_PIN, ADC_11db);
#endif
#if JANUS_CHARGE_CUT_PIN >= 0
  pinMode(JANUS_CHARGE_CUT_PIN, OUTPUT);
  janusCutChargeOutput(false);
#endif

  prefs.begin("janus69", false);
  ledPalette = prefs.getUChar("palette", 0) % LED_PALETTE_COUNT;
  volumeVal = prefs.getUChar("vol", volumeVal);
  if (volumeVal > 21) volumeVal = 14;
  ledBright = prefs.getUChar("bright", ledBright);
  if (ledBright > 100) ledBright = 55;  // v10.11H: 0 is valid LCD-off eco mode
  audioDoutPin = prefs.getUChar("dout", I2S_DOUT);
  if (audioDoutPin != 15 && audioDoutPin != 16) audioDoutPin = I2S_DOUT;
  janusSdLoadConfig();
  janusFarmLoadState();
  if (ledPalette == LED_PROC_PALETTE_INDEX) generateProceduralPalette();
  uiOldPal = ledPalette;

  lcdInit();
  drawStaticUI();

  // Brownout-safe audio bootstrap. Do a full old ES8311 init at boot,
  // then again on every actual stream start/recovery.
  audioPowerWake();
  audioSetupPins();
  audio.setVolume(volumeVal);

  initOptionalCamera();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t = millis();
  while(WiFi.status() != WL_CONNECTED && millis() - t < 15000) { delay(500); Serial.print('.'); }
  if (WiFi.status() == WL_CONNECTED) {
    janusWebBegin();
    // v10.11N2: prepare random seed now; real random jump is applied immediately before first PLAY.
    janusBootShufflePlaylist();
  }

  initColonyNow();
#if JANUS_CAMERA_ENABLE
  initJanusCamera();
#endif

  // Keep miner away from Audio.h service loop/UI core.
  xTaskCreatePinnedToCore(microMinerTask, "MinerTask", 10240, NULL, 1, &minerTaskHandle, 0);
}


void loop() {
  uint32_t now = millis();
  janusThermalTick(now);
  if (janusThermalStop) {
    handleSerialCommands();
    pollButtons();
    if (now - lastLedMs >= 500) {
      lastLedMs = now;
      strip.clear();
      strip.show();
    }
    vTaskDelay(pdMS_TO_TICKS(50));
    return;
  }

  bool audioRealtime = janusAudioNeedsRealtimeNow();

  // Buzz is the speaker-master: HTTP web/SD maintenance is nice-to-have,
  // but Audio.h/I2S service must never wait behind it.
  if (!audioRealtime) {
    janusWebTick();
    janusSdRetentionTick(false);
    janusFarmSaveState(false);
  }
  handleSerialCommands();
  pollButtons();

  audioAutopilotTick(now);

  if(now - lastLedMs >= 30) { lastLedMs = now; updateLED(); }
  if(now - lastFastUiMs >= (audioRealtime ? (HEAVY_UI_MS + 40) : HEAVY_UI_MS)) { lastFastUiMs = now; updateSpectrumModel(); drawFastUI(); }
  if(!audioRealtime && now - lastTobiMs >= TOBI_TURBO_MS) { lastTobiMs = now; drawTobiTurboOverlay(); }
  if(!audioRealtime && now - lastTrackMs >= 12000) { lastTrackMs = now; if(wanted) updateTrackInfo(); }
  colonyTick();

  static uint32_t lastMinerLog = 0;
  if (now - lastMinerLog >= 3000) {
    lastMinerLog = now;
    uint32_t submitTotal = minerSubmitAttempts;       // all submits, local + remote
    uint32_t submitRemote = minerRemoteSubmitAttempts; // subset of submitTotal
    uint32_t submitLocal = (submitTotal >= submitRemote) ? (submitTotal - submitRemote) : 0;
    uint32_t totalHsNow = minerRealHashrate + minerLocalHashrate;
    uint8_t onlineNow = colonyOnlineNodeCount();
    uint32_t qNow = colonyRemoteShareQueue ? (uint32_t)uxQueueMessagesWaiting(colonyRemoteShareQueue) : 0;
    uint32_t acceptPct = submitTotal ? (minerShares * 100UL / max(1UL, submitTotal)) : 0;
    uint16_t activeBatch = effectiveMiningBatch();
    char agentTopSnap[24];
    float agentPredErrSnap = 0.0f;
    colonyAgentSnapshot(agentTopSnap, sizeof(agentTopSnap), &agentPredErrSnap);

    Serial.printf("[MINER] mode=%s pool=%d H=%lu poolH=%lu localH=%lu total=%llu best=%lu target=%u acc=%lu sub=%lu localSub=%lu remoteSub=%lu ok=%lu%% rej=%lu low=%lu stale=%lu other=%lu workers=%u/%u agentRewards=%lu aok=%lu top=%s predErr=%.3f rx=%lu qNow=%lu qTotal=%lu drop=%lu weak=%lu dup=%lu uniJob=%lu discJob=%lu peerBest=%lu diff=%.8f batch=%u sdRows=%lu status=%s err=%s\n",
      stratumConnected ? "POOL" : "WAIT",
      stratumConnected ? 1 : 0,
      (unsigned long)totalHsNow,
      (unsigned long)minerRealHashrate,
      (unsigned long)minerLocalHashrate,
      (unsigned long long)minerTotalHashes,
      (unsigned long)minerBestBits,
      (unsigned)minerShareTargetBits,
      (unsigned long)minerShares,
      (unsigned long)submitTotal,
      (unsigned long)submitLocal,
      (unsigned long)submitRemote,
      (unsigned long)acceptPct,
      (unsigned long)minerSubmitRejects,
      (unsigned long)minerLowDiffRejects,
      (unsigned long)minerStaleJobRejects,
      (unsigned long)minerOtherRejects,
      (unsigned)onlineNow,
      (unsigned)colonyKnownNodes,
      (unsigned long)colonyAgentRewardsSent,
      (unsigned long)colonyAgentShareRewardsSent,
      agentTopSnap,
      agentPredErrSnap,
      (unsigned long)colonyRxCount,
      (unsigned long)qNow,
      (unsigned long)colonyRemoteSharesQueued,
      (unsigned long)colonyRemoteSharesDropped,
      (unsigned long)colonyRemoteWeakDrops,
      (unsigned long)colonyRemoteDuplicateDrops,
      (unsigned long)colonyUnicastJobsSent,
      (unsigned long)colonyDiscoveryJobsSent,
      (unsigned long)colonyBestPeerBits,
      minerCurrentDiffF,
      (unsigned)activeBatch,
      (unsigned long)minerStatsRows,
      minerStatus,
      minerLastRejectReason);

    janusSdLogf("MINER", "mode=%s H=%lu best=%lu target=%u acc=%lu sub=%lu localSub=%lu remoteSub=%lu ok=%lu reject=%lu low=%lu stale=%lu workers=%u/%u agentRewards=%lu aok=%lu top=%s predErr=%.3f rx=%lu qNow=%lu qTotal=%lu drop=%lu uniJob=%lu discJob=%lu status=%s err=%s",
      stratumConnected ? "POOL" : "WAIT",
      (unsigned long)totalHsNow,
      (unsigned long)minerBestBits,
      (unsigned)minerShareTargetBits,
      (unsigned long)minerShares,
      (unsigned long)submitTotal,
      (unsigned long)submitLocal,
      (unsigned long)submitRemote,
      (unsigned long)acceptPct,
      (unsigned long)minerSubmitRejects,
      (unsigned long)minerLowDiffRejects,
      (unsigned long)minerStaleJobRejects,
      (unsigned)onlineNow,
      (unsigned)colonyKnownNodes,
      (unsigned long)colonyAgentRewardsSent,
      (unsigned long)colonyAgentShareRewardsSent,
      agentTopSnap,
      agentPredErrSnap,
      (unsigned long)colonyRxCount,
      (unsigned long)qNow,
      (unsigned long)colonyRemoteSharesQueued,
      (unsigned long)colonyRemoteSharesDropped,
      (unsigned long)colonyUnicastJobsSent,
      (unsigned long)colonyDiscoveryJobsSent,
      minerStatus,
      minerLastRejectReason);

#if JANUS_SD_ENABLE
    if (janusSdReady && now - minerLastStatsCsvMs >= 10000UL) {
      minerLastStatsCsvMs = now;
      snprintf(minerStatsCsvLine, sizeof(minerStatsCsvLine),
               "%lu,%s,%u,%lu,%lu,%lu,%llu,%lu,%u,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%u,%u,%lu,%lu,%s,%.4f,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%lu,%.8f,%u,%s,%s",
               (unsigned long)now,
               stratumConnected ? "POOL" : "WAIT",
               stratumConnected ? 1U : 0U,
               (unsigned long)totalHsNow,
               (unsigned long)minerRealHashrate,
               (unsigned long)minerLocalHashrate,
               (unsigned long long)minerTotalHashes,
               (unsigned long)minerBestBits,
               (unsigned)minerShareTargetBits,
               (unsigned long)minerShares,
               (unsigned long)submitTotal,
               (unsigned long)submitLocal,
               (unsigned long)submitRemote,
               (unsigned long)acceptPct,
               (unsigned long)minerSubmitRejects,
               (unsigned long)minerLowDiffRejects,
               (unsigned long)minerStaleJobRejects,
               (unsigned long)minerOtherRejects,
               (unsigned)onlineNow,
               (unsigned)colonyKnownNodes,
               (unsigned long)colonyAgentRewardsSent,
               (unsigned long)colonyAgentShareRewardsSent,
               agentTopSnap,
               agentPredErrSnap,
               (unsigned long)colonyRxCount,
               (unsigned long)qNow,
               (unsigned long)colonyRemoteSharesQueued,
               (unsigned long)colonyRemoteSharesDropped,
               (unsigned long)colonyRemoteWeakDrops,
               (unsigned long)colonyRemoteDuplicateDrops,
               (unsigned long)colonyUnicastJobsSent,
               (unsigned long)colonyDiscoveryJobsSent,
               (unsigned long)colonyBestPeerBits,
               minerCurrentDiffF,
               (unsigned)activeBatch,
               minerStatus,
               minerLastRejectReason);
      janusSdAppendLine(JANUS_SD_ROOT "/logs/miner_stats.csv", minerStatsCsvLine);
      minerStatsRows++;
    }
#endif
  }
  delay(1);
}
