# Arduino Static Audit

Static shape check only. No firmware was compiled or flashed. Delay calls are counted for hardware review but are not treated as compile failures by this heuristic.

| file | setup | loop | delay calls | duplicate named structs | status | notes |
| --- | ---: | ---: | ---: | --- | --- | --- |
| `firmware/core2/CORE2.ino` | 1 | 1 | 3 | - | PASS | 3 delay() calls require loop-impact review on hardware |
| `firmware/buzz/Buzz.ino` | 1 | 1 | 30 | - | PASS | 30 delay() calls require loop-impact review on hardware |
| `firmware/blind_eye/BLIND_EYE.ino` | 1 | 1 | 4 | - | PASS | 4 delay() calls require loop-impact review on hardware |
| `firmware/beacon/Beacon_A1.ino` | 1 | 1 | 6 | SwarmAiNodeState | WARN | duplicate named structs: SwarmAiNodeState; 6 delay() calls require loop-impact review on hardware |
| `firmware/esp32_swarm/Stick.ino` | 1 | 1 | 6 | - | PASS | 6 delay() calls require loop-impact review on hardware |
| `firmware/esp32_swarm/ATOM_SWARM_TRON.ino` | 1 | 1 | 5 | - | PASS | 5 delay() calls require loop-impact review on hardware |
| `firmware/pyramid/ATOM_MATRIX_Pyramid.ino` | 1 | 1 | 21 | - | PASS | 21 delay() calls require loop-impact review on hardware |
| `firmware/zim_geek/Zim.ino` | 1 | 1 | 7 | - | PASS | 7 delay() calls require loop-impact review on hardware |
