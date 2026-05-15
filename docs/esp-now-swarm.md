# ESP-NOW Swarm

The LastSwarm sketches use ESP-NOW-style packet exchange for local swarm coordination. Packet formats must be treated as compatibility contracts.

## Review Checklist

- Verify packet type bytes.
- Verify struct packing and `sizeof(...)` values.
- Verify version fields where present.
- Verify channel behavior when Wi-Fi and ESP-NOW are both active.
- Verify callback code does not do blocking work.
- Verify RX queues are drained from the main loop or a safe task context.

Do not mix firmware generations until packet compatibility is checked.
