# JANUS Distributed AI Swarm — Third-Party Notices

This file is a transaction-facing register of third-party software and platform dependencies visible in the public repository. It is not a substitute for an automated SBOM and license scan on a specific release.

## External libraries / platform components referenced by firmware

The Buzz firmware currently references external components including, among others:

- Arduino / ESP32 board-support APIs;
- Espressif ESP-NOW, Wi-Fi, camera and related ESP32 platform components;
- mbedTLS SHA-256 facilities provided through the ESP32 platform stack;
- `Adafruit_NeoPixel`;
- `ArduinoJson`;
- an external `Audio.h` library used by the target board/audio path;
- filesystem / SD / LittleFS components supplied by the relevant board-support environment.

These components are **not relicensed by the JANUS repository-level license**. Each retains its own upstream copyright and license terms.

The repository-level Source-Available Evaluation License applies only to material for which the project has the right to apply that license.

## Hardware and vendor references

Names such as ESP32, M5Stack, Waveshare, Arduino, Espressif, Adafruit, and other vendor/library names are used descriptively for compatibility and engineering context. Their names and marks remain the property of their respective owners. No sponsorship or endorsement is implied.

## Historical MIT project code

Repository-authored code from historical revisions through commit:

`b644af87de104b405427a8c0ae3c35c8d192507c`

was published under MIT as described in `LICENSE_HISTORY.md`.

That historical project-level MIT grant is separate from third-party licenses and remains relevant when substantial material from those snapshots is redistributed.

## Closing / commercial review rule

Before any OEM, acquisition, commercial deployment, or code transfer:

1. generate an SBOM for the exact closing commit;
2. inventory source files, vendored files, libraries, board packages and generated artifacts;
3. map every external component to its exact version and license;
4. preserve required attribution and license texts;
5. identify reciprocal/copyleft obligations, if any;
6. confirm that proprietary/vendor SDK terms permit the intended deployment;
7. block closing on any unresolved incompatible or unknown license.

## No false clean-room claim

The existence of this register does not mean that an automated scan has already proved the repository free of third-party obligations. It is an explicit starting point for that verification.
