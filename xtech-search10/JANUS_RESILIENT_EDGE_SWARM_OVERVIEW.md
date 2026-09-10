# JANUS Resilient Edge Swarm — Technical Overview

## Candidate technology

**JANUS Resilient Edge Swarm** is the xTech-facing presentation of the existing `JANUS Distributed AI Swarm` engineering project. It is not a new rewrite and does not replace the original firmware.

The developer reports a **10-node physical JANUS swarm** built around heterogeneous ESP32/M5Stack-class devices. Different nodes preserve different local roles—operator/UI, sensing, telemetry, radio anchor, local compute, multimedia or bounded workload participation—while selected state is shared through explicit packet interfaces, primarily ESP-NOW in the current public implementation.

The xTech submission concept isolates one practical engineering question from the larger JANUS project:

> Can a low-cost heterogeneous edge mesh preserve useful local sensing and telemetry behavior when one peer, coordinator or network path becomes unavailable, while making loss/recovery visible and testable?

## Existing architecture

The current repository already implements and documents the relevant architectural pieces:

- heterogeneous ESP32/M5Stack node roles rather than identical workers;
- ESP-NOW packet/ABI handling;
- local sensor and state processing;
- periodic heartbeat and peer visibility;
- stale-node handling and reconnect/recovery paths;
- protected `PRIMARY_MISSION` with bounded side-work;
- local state persistence in selected nodes;
- operator/UI surfaces that distinguish current sensor state from memory, prediction and presentation state;
- explicit technical boundaries around adaptive control and claims.

The technology is the **full ten-node physical swarm**. For a short, reproducible reviewer demonstration, the preferred first subset is **Core2 + Blind Eye + Anchor + ATOM SWARM TRON**, with Buzz available when coordinator/workload behavior is useful. This smaller subset is a demonstration configuration only; it must not be interpreted as the total size of JANUS.

## Why the architecture is relevant

Low-cost edge systems are attractive only if they remain interpretable when communications become imperfect. A small distributed system that silently loses a node, continues displaying stale data as current, or depends completely on one coordinator is difficult to trust.

JANUS addresses this at the firmware-architecture level rather than by claiming a completed military network. Current design rules require that:

1. each upgraded specialist declares a protected primary mission;
2. optional coordinator work is bounded and may not silently starve that mission;
3. nodes remain visible through heartbeat/health/provenance paths;
4. stale peer state is handled explicitly rather than treated as indefinitely current;
5. sensor truth remains distinguishable from prediction, memory and UI state;
6. recovery behavior remains inspectable.

These are useful properties for civilian and government edge networks that need to tolerate intermittent peers and heterogeneous hardware.

## Existing node roles selected for first xTech review

### Core2 — operator and telemetry surface

`firmware/core2/CORE2.ino`

Core2 is the dense display/control surface for swarm state. The current firmware receives ESP-NOW swarm data, maintains node/state views, integrates local sensing, and provides the most direct human-facing surface for a failure/recovery demonstration.

### Blind Eye — sensor node

`firmware/blind_eye/BLIND_EYE.ino`

Blind Eye uses an AtomS3R-class device with an STHS34PF80 TMOS/PIR sensor as its primary physical sensing path. The current build treats the absence of a camera as a normal hardware profile and separates current sensing from memory/prediction semantics. It also contains RF-lite telemetry and swarm packet compatibility.

### Anchor — resilience/reference node

`firmware/anchor/Anchor.ino`

Anchor is a current ESP32-S3 RF anchor/beacon-style node with deferred receive handling, heartbeat/telemetry behavior, Wi-Fi/ESP-NOW reconnect logic and explicit radio-rejoin safeguards. It is the strongest existing firmware target for showing observable connectivity loss and recovery.

### ATOM SWARM TRON — heterogeneous edge node

`firmware/esp32_swarm/ATOM_SWARM_TRON.ino`

ATOM SWARM TRON represents a separate Atom-class firmware lineage with telemetry and local/remote swarm behavior. It is useful in the demonstration because the system is intended to coordinate unlike devices rather than a fleet of identical nodes.

### Buzz — optional coordinator/workload node

`firmware/buzz/Buzz.ino`

Buzz contains mature coordinator/worker, ESP-NOW and recovery lineage alongside multimedia and protocol work. For an xTech demonstration, its mining-related functionality is not the proposed Army capability; it is background engineering showing that communication, local multimedia workload and bounded distributed work have already coexisted on constrained hardware.

## Army relevance — scoped carefully

The official xTech|Search 10 RFI places strong consideration on **Command and Control (C2) and Counter-C2 Networks**, including resilient communications and deep sensing. JANUS is not presented as an operational C2 system. The relevant transferable engineering is the low-cost edge pattern:

```text
LOCAL SENSOR / LOCAL MISSION
          |
          v
SELECTED STATE + HEARTBEAT
          |
       ESP-NOW
          |
          v
PEER / OPERATOR VISIBILITY
          |
    LOSS -> STALE -> RECOVERY
```

A Phase I effort, if selected and eligible, would therefore focus on measuring and hardening the existing edge-network behavior under controlled disruption rather than inventing a new combat function.

## Dual-use paths

Potential non-defense applications include industrial/environmental telemetry, remote infrastructure monitoring, disaster-response sensor deployment, building/field instrumentation, maker/education edge networks, and resilient local automation. These are **application pathways**, not claims that the project already has commercial customers in those markets.

## What is implemented vs. what remains to prove

### Implemented in the current project

Existing firmware, heterogeneous node roles, ESP-NOW communication paths, sensor integration, heartbeat/state visibility, reconnect/recovery code paths, bounded-specialist architecture and explicit truth/protocol boundaries. The developer reports ten physical nodes in the current swarm; the final submission inventory will freeze the exact hardware/firmware identity of each physical device.

### Needs controlled evidence before being claimed quantitatively

Packet-loss tolerance, recovery-time distribution, useful range, energy budget by role, stale-state frequency, multi-node mission continuity, RF coexistence behavior, environmental robustness and repeatability across a frozen hardware/software configuration.

### Not claimed

Battlefield readiness, Army integration, secure military communications certification, anti-jam performance, cyber/EW superiority, autonomous weapon control, production safety certification or independently validated commercial traction.

## xTech framing

The proposed xTech value is therefore deliberately narrow and testable:

**Take a physically existing ten-node, low-cost heterogeneous edge swarm and turn its current resilience/recovery mechanisms into a reproducible measurement package that Army evaluators can independently stress, observe and compare.**
