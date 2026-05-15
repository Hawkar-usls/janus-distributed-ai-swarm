# File Inventory

Focused inventory for the public LastSwarm firmware branch.

| path | project | role | size | risk | reason |
| --- | --- | --- | ---: | --- | --- |
| `firmware/core2/CORE2.ino` | CORE2 | BOOTABLE | 362777 | MEDIUM | M5Stack Core2 display/control surface |
| `firmware/buzz/Buzz.ino` | BUZZ | BOOTABLE | 263330 | MEDIUM | Audio, worker, ESP-NOW, Stratum-aware node |
| `firmware/blind_eye/BLIND_EYE.ino` | BLIND_EYE | BOOTABLE | 93558 | MEDIUM | Presence and sensor calibration node |
| `firmware/beacon/Beacon_A1.ino` | BEACON | BOOTABLE | 133608 | MEDIUM | Beacon preserve candidate |
| `firmware/esp32_swarm/Stick.ino` | STICK | BOOTABLE | 255836 | MEDIUM | Mobile ESP32 swarm node |
| `firmware/esp32_swarm/ATOM_SWARM_TRON.ino` | ATOM_SWARM_TRON | BOOTABLE | 197825 | MEDIUM | Atom swarm/TRON node |
| `firmware/pyramid/ATOM_MATRIX_Pyramid.ino` | ATOM_MATRIX_PYRAMID | BOOTABLE | 155657 | MEDIUM | Atom Matrix visual/swarm node |
| `firmware/zim_geek/Zim.ino` | ZIM_GEEK | BOOTABLE | 154064 | MEDIUM | ESP32-S3 Geek swarm reporting node |
| `README.md` | SUPPORT | DOC | 3726 | LOW | Public repository overview |
| `M5STACK_REVIEW_GUIDE.md` | SUPPORT | DOC | 2339 | LOW | External hardware review entrypoint |
| `PROJECT_MAP.md` | SUPPORT | DOC | 1204 | LOW | Focused repository map |
| `SECURITY.md` | SUPPORT | DOC | 683 | LOW | Security and secret policy |
| `LICENSE` | SUPPORT | DOC | 833 | LOW | MIT license |
| `configs/examples/secrets.example.h` | SUPPORT | CONFIG | 223 | LOW | Placeholder secret header example |
| `configs/examples/config.example.json` | SUPPORT | CONFIG | 143 | LOW | Placeholder JSON config example |
| `tests/static_checks/check_sensitive_markers.py` | SUPPORT | TEST | 681 | LOW | Lightweight secret marker check |
