# Golcron v1.5 — BH Pixel Cosmos / Adaptive Unseen Paths

Open `Golcron.ino` from this folder as one Arduino sketch. Arduino IDE compiles
`Golcron.ino` and all numbered `Golcron_part_*.ino` tabs together in filename order.

## Correct board

Use the classic **ESP32 Dev Module** profile — not `ESP32P4 Dev Module`, ESP32-C3,
ESP32-C6, ESP32-S2 or ESP32-S3.

Recommended settings:

- Board: **ESP32 Dev Module**
- CPU frequency: **240 MHz (WiFi/BT)**
- Flash size: **4 MB**
- Flash mode: **QIO**
- Flash frequency: **80 MHz**
- PSRAM: **Disabled**
- Upload speed: **921600** (reduce if the USB-UART link is unstable)

A correct build for this classic ESP32 uses the Xtensa toolchain, not the P4
RISC-V toolchain.

## Automatic mining

No button is required to start mining. Golcron starts ESP-NOW at boot, scans the
configured swarm channels, accepts real Buzz `J/B` ranges, mines them, and sends
valid `S/2` shares automatically.

The range is divided into disjoint untouched slices. Inside each slice Golcron
uses a coprime permutation, learns from lane history, and selects the next
unvisited path. Planning does not reset the current cursor or intentionally
revisit already checked nonce positions.

## Controls

- **A tap:** screen fully off/on (`ST7789 DISPOFF/DISPON` plus backlight GPIO4)
- **A hold:** telemetry frame off/on
- **B tap:** cycle `COSMOS / MINER / DIAG`
- **B hold:** bias planning of the next untouched path; current progress is not reset

Screen-off mode stops rendering but leaves mining, Buzz and ESP-NOW active.

## Visual identity

The display uses a BH-family pixel cosmos: broad galaxy arms, accretion ribbon,
reverse-black-hole/Holocron core, comets and birth waves. Thin astrolabe rails
remain to preserve Golcron's own identity. The visual layer does not alter hash,
target or share truth.
