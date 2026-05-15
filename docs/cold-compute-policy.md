# Cold Compute Policy

LastSwarm firmware treats heat, low heap, weak signal, and timing jitter as control inputs. A node may reduce display rate, packet rate, work batch, or optional behavior when runtime conditions degrade.

## Review Points

- Avoid blocking delays in critical UI, sensor, audio, and ESP-NOW loops.
- Keep optional heavy logic behind timing gates.
- Prefer measured state over fixed assumptions.
- Treat throttling as a signal to reduce work, not as a cosmetic issue.
