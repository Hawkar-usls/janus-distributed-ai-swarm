# M5Stack Showcase

JANUS LastSwarm uses M5Stack-class devices as coordinated firmware nodes rather than isolated demos. The sketches combine display, sensing, audio, telemetry, packet exchange, and local adaptive state.

## Hardware Story

- Core2 provides the main rich display/control surface.
- Atom and Atom Matrix nodes provide compact visual/swarm endpoints.
- Stick/Cardputer-style nodes fit mobile swarm roles.
- Blind Eye-style nodes focus on presence sensing and calibration.
- Beacon, Buzz, and Zim preserve distinct firmware personalities while sharing swarm concepts.

## Engineering Profile

Most embedded examples prove one feature at a time. This suite keeps several subsystems active together:

- UI rendering;
- ESP-NOW packet exchange;
- sensor interpretation;
- local memory and prediction-error tracking;
- audio and visual state;
- protocol telemetry where supported;
- cold/safety-oriented control hints.

## Review Entrypoints

| area | file |
| --- | --- |
| Core2 swarm surface | `firmware/core2/CORE2.ino` |
| Buzz worker/audio lineage | `firmware/buzz/Buzz.ino` |
| Blind Eye sensing | `firmware/blind_eye/BLIND_EYE.ino` |
| Beacon preserve candidate | `firmware/beacon/Beacon_A1.ino` |
| Stick swarm node | `firmware/esp32_swarm/Stick.ino` |
| Atom swarm/TRON node | `firmware/esp32_swarm/ATOM_SWARM_TRON.ino` |
| Atom Matrix pyramid | `firmware/pyramid/ATOM_MATRIX_Pyramid.ino` |
| Zim Geek node | `firmware/zim_geek/Zim.ino` |

## Validation Status

The repository contains static review artifacts only. Firmware was not compiled or flashed during repository cleanup. Hardware validation should check board definitions, optional libraries, pin mappings, ESP-NOW packet sizes, sensor calibration, and non-blocking behavior on the actual devices.
