# JANUS Resilient Edge Swarm — Reviewer FAQ

This FAQ is written for a skeptical technical evaluator. Short answers come first; supporting detail follows.

## Is JANUS already a military system?

**No.** It is an active-engineering heterogeneous ESP32/M5Stack swarm and physical prototype environment. The xTech proposition is to evaluate transferable edge-system behavior under controlled disruption, not to claim an already fielded Army C2 product.

## Is the system physically real or only simulated?

The developer reports a **ten-node physical JANUS swarm** and the repository contains the firmware lineages behind it. However, the exact ten-device xTech manifest has not yet been frozen with dated photos, board identities, flashed commit hashes, and test receipts. Until that capture is complete, the strongest submission wording is `developer-reported ten-node physical swarm` rather than `independently verified ten-node system`.

## Why show only four or five devices in the quick demo if there are ten physical nodes?

Because the quick demo is a communication tool, not the system boundary. Core2 + Blind Eye + Anchor + a heterogeneous peer, optionally Buzz, is enough to show sensing, peer visibility, loss/stale behavior, and rejoin in a short video. The full ten-node fleet is the harder system-level evaluation target.

## What is actually novel here?

The claim is not that ESP-NOW or ESP32 hardware is new. The candidate value is the **combination and discipline** around heterogeneous specialist roles, protected local missions, bounded shared work, explicit freshness/staleness, sensor-truth separation, and observable recovery across constrained low-cost devices.

Whether that combination is sufficiently differentiated for Army interest is an evaluation question, not something this branch declares by fiat.

## Why not just call this an IoT mesh?

That description is partly true but incomplete. JANUS is less about routing packets and more about how unlike nodes preserve local roles and expose state when peers or services fail. The xTech evaluation focuses on application-level freshness, role continuity, and recovery semantics above the current radio transport.

## Does JANUS form a true multi-hop mesh network?

This package should **not imply multi-hop routing unless the exact selected firmware/test proves it**. The current public implementation primarily uses ESP-NOW peer/coordinator communication patterns. `Mesh` in the xTech title is used in the broad distributed-edge sense; if reviewer ambiguity is a concern, `network` or `swarm` is the safer technical term in detailed descriptions.

## Is it anti-jam?

**No such claim is established.** The current low-cost transport is 2.4 GHz ESP-NOW/Wi-Fi class hardware. The evaluation can measure impairment and recovery, but that is not the same as jam resistance, protected RF, LPI/LPD, or contested-spectrum certification.

## Is it secure enough for Army operational use?

**Not established.** The repository does not claim a DoD-accredited security architecture, complete fleet key management, penetration-test results, an ATO, or military networking certification. Security hardening would be a separate integration requirement.

## Why use hobby/commercial hardware for an Army competition?

Because inexpensive COTS hardware is useful for fast architecture experiments and repeatable failure testing. If the architecture proves valuable, the role/state/recovery interfaces can then be evaluated for migration to hardened hardware. The present boards themselves are not claimed to be rugged military equipment.

## What happens when the coordinator disappears?

The architecture requires protected primary missions and bounded side-work, and several firmware paths include recovery/fallback behavior. But this package does not convert that design rule into a guarantee. Coordinator-loss continuity is one of the explicit controlled tests.

## How fast does a lost node get detected?

**Not yet measured under a frozen xTech configuration.** Source timeouts are implementation constants, not benchmark results. The submission should insert a number only after repeated tests produce timestamped evidence.

## How fast does a node rejoin?

**Not yet established quantitatively.** The code contains reconnect/rejoin paths. The xTech test protocol measures restoration-to-first-packet and restoration-to-healthy-state separately.

## Can stale data be mistaken for current data?

The architecture explicitly tries to prevent this and documents stale/current boundaries. The correct engineering question is whether the implementation succeeds under failure. Therefore the test target is `stale_state_events = 0`, but zero is not claimed until verified.

## What does Blind Eye actually sense?

The current camera-absent Blind Eye profile uses an STHS34PF80 TMOS/PIR-style physical sensing path as its primary eye, with additional RF/IMU/mic fusion concepts in the firmware lineage. It should not be described as a thermal camera, biometric identifier, or precision ranging device unless a separate sensor path actually supports that claim.

## Is RF-based human detection proven?

Not as an xTech performance claim. The repository contains RF-lite/RF-dome experimental paths, but the xTech proposal should distinguish experimental inference from independently calibrated physical sensing. The safe core claim is that the system has inspectable sensing and telemetry paths with explicit truth-state boundaries.

## What role does mining play?

Some firmware evolved while performing NerdMiner/Stratum or SHA workloads. That history is useful as evidence that constrained nodes have hosted networking, UI, audio, persistence, and shared work concurrently. **Mining performance is not the Army capability claim** and should not be used as the value proposition.

## Why are there game, galaxy, prophecy, or other unusual names in the firmware?

They are project vocabulary and preserved experimental lineage. This xTech directory exists precisely so an evaluator does not have to interpret the lore to understand the engineering. The original code is not being rewritten or sanitized to pretend that history did not exist.

## Why is preserving that unusual history preferable to cleaning it out?

Because provenance matters. Removing old vocabulary solely for a competition could make it harder to understand how the system actually evolved. A clean reviewer layer plus transparent original source is more honest than presenting a competition-specific rewrite as if it were the historical system.

## Is every repository firmware role one of the ten physical nodes?

**No assumption should be made.** The repository includes active firmware, compatibility images, preserved lineages, and staged P4 tracks. The exact physical ten must be mapped from the devices themselves.

## Can it scale to hundreds or thousands of nodes?

**Not proven.** Ten physical nodes is a useful non-trivial prototype scale, not evidence of arbitrary scalability. Larger-scale claims require emulation, hardware-in-the-loop, or expanded physical tests.

## What is the weakest part of the current xTech proposition?

The evidence package is not yet complete. Specifically, the exact physical fleet is not frozen, quantitative recovery/packet/power data is not yet collected under the xTech protocol, military cybersecurity and ruggedization are outside the current demonstrated scope, and commercial traction is not documented.

## Then why should xTech care now?

Because the project is at a useful transition point: **more mature than a concept, less mature than a product**. There is enough working firmware and physical hardware to generate meaningful test data quickly, while the remaining technical questions are concrete enough for a Phase I effort to reduce uncertainty.

## What would Phase I actually buy?

Not a new mythology and not a rewrite. A disciplined Phase I would produce:

- an exact frozen ten-node reference configuration;
- repeated disruption/recovery measurements;
- packet, stale-state, resource, thermal, and power data;
- a transparent failure register;
- an interface description separating transport-specific code from general state/recovery behavior;
- a reproducible evaluation kit and evidence bundle suitable for independent review.

## What would count as a bad Phase I outcome?

A bad outcome would be learning that the architecture cannot preserve useful local roles under realistic disruption, that stale state is difficult to bound, that recovery creates cascading failures, or that power/resource cost is impractical. Those outcomes should be reported, not hidden.

## Does the project have customers or revenue?

No verified customer/revenue evidence is currently frozen in this branch. The submission must not invent commercial traction. Commercial potential should be framed as a path from an existing prototype to measured evaluation kits, integration work, licensing, or OEM/partner deployment if the technical evidence supports it.

## Is the developer eligible to submit directly?

Written clarification is pending from the Army FUZE xTech Program because the detailed RFI and a summary field on the official website are not aligned for an applicant based in Ukraine. The branch does not assume eligibility and will follow the official response.

## What is the single strongest sentence to remember?

> **JANUS is an existing heterogeneous low-cost edge swarm whose value proposition is not that failures never happen, but that local roles, data freshness, peer loss, and recovery are explicit enough to observe, measure, and improve.**