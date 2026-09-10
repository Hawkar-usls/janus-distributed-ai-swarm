# JUXTAPOSE for xTech — Existing Algorithmic Capability and Its Relation to JANUS Swarm

## Why this document exists

This file records a second, already-developed JANUS capability that is relevant to a Pentagon/xTech reviewer without pretending that it has already been integrated into the current physical swarm.

The xTech candidate remains the **existing JANUS Resilient Edge Swarm as it exists today**. Its firmware is not rewritten for this application. JUXTAPOSE is presented alongside it for two reasons:

1. it shows an existing, separately developed communications-resilience algorithmic capability; and
2. it defines a natural future experiment that could be run on the ten-node JANUS hardware without retroactively claiming that the swarm already uses it.

The intended reviewer takeaway is simple:

```text
WHAT EXISTS NOW
=
10-NODE JANUS PHYSICAL / FIRMWARE SWARM
+
SEPARATE JUXTAPOSE ADAPTIVE-SEARCH RESEARCH ASSET

WHAT IS NOT CLAIMED
=
JUXTAPOSE IS ALREADY EMBEDDED IN THE CURRENT SWARM
```

This distinction is deliberate. The goal is to show what the developer can already build, test, formalize, and falsify—not to make the existing hardware look more mature by silently merging two separate bodies of work.

---

## JUXTAPOSE in one sentence

**JUXTAPOSE is an experimental transport-agnostic, exact-backed adaptive search architecture for resilient communications under degraded, disrupted, intermittent, and limited connectivity.**

Its central design rule is:

> **Learn where to look; never learn what is true.**

The adaptive layer may rank which authorized path should be checked first and may choose how broadly to search, but it does not have authority to declare current connectivity. A current `CONNECTED` state requires a **fresh end-to-end measurement**. If the environment becomes unfamiliar or predictions fail, search widens again. If evidence is insufficient before the resource budget is exhausted, the correct terminal result is an explicit `UNKNOWN_RESOURCE_LIMIT`, not a fabricated outage conclusion.

Conceptually:

```text
AUTHORIZED CANDIDATE PATHS
        |
        v
HISTORICAL EXACT RECEIPTS + CURRENT STRUCTURAL CONTEXT
        |
        v
ADAPTIVE ORDER / SEARCH WIDTH
        |
        v
FRESH END-TO-END CHECK
        |
   +----+----+
   |         |
 PASS       FAIL
   |         |
CURRENT      TRY NEXT / WIDEN
CONNECTED    SEARCH
             |
             v
      UNKNOWN_RESOURCE_LIMIT
      if budget is exhausted
```

Prediction can therefore reduce search effort, but it cannot replace measurement authority.

---

## Existing JUXTAPOSE evidence

JUXTAPOSE was developed and packaged separately from the xTech swarm branch. A DARPA-facing technical package was previously prepared and shared for independent evaluation. The package explicitly described the work as **local synthetic-twin evidence only**, not field validation.

The frozen v5.1 synthetic holdout used **20,000 episodes** and passed **15/15 preregistered gates**. In that specific synthetic environment:

| Metric | JUXTAPOSE v5.1 | Comparison / note |
| --- | ---: | --- |
| Validated connectivity | 98.215% | 97.785% for uniform-random comparison |
| Mean mission/search-resource cost | 1.389 | 1.914 for uniform-random comparison |
| Mean exact checks | 1.340 | 1.502 for uniform-random comparison |
| False `CONNECTED` in complete-outage controls | 0 | Truth firewall held in that synthetic test |
| OOD episodes retained as COLD/new | 82.138% | Against frozen >=60% novelty-safety gate |

These numbers are **not hardware-swarm performance claims**. They belong only to the frozen synthetic test described by the JUXTAPOSE package.

The strongest evidence is not simply that one metric improved. The more important property is that the adaptive layer remained subordinate to exact current-state measurement in the test, while novelty/surprise could force broader search again.

---

## Negative results are part of the asset

JUXTAPOSE is included here with its failures, not only the repaired result.

Earlier development exposed material weaknesses:

- **v1:** probe overhead and route thrashing;
- **v5:** novelty handling became confident too quickly in combined out-of-distribution regimes;
- **v5.1:** repaired that specific premature-confidence failure with asymmetric trust and independent-support requirements;
- the independent end-to-end contribution of every submodule, including Spider by itself, is **not established in every setting**;
- there is **no independent external replication or field validation yet**.

Those statements are not caveats to hide in fine print. They are part of the development record and are exactly why controlled physical validation is the next meaningful step.

---

## Why it fits the JANUS Swarm

The Swarm and JUXTAPOSE were developed separately, but they share the same architectural discipline:

```text
IMMUTABLE_TRUTH_CORE
+
LEARNABLE_POLICY_AROUND_IT
```

In the current Swarm, sensor truth, packet/protocol truth, identity, stale state, memory, inference, and UI state are deliberately separated. JUXTAPOSE applies the same principle to connectivity:

```text
PREDICTED GOOD PATH
!=
CURRENT CONNECTIVITY
```

A future controlled test could expose real JANUS peer/interface/path candidates to JUXTAPOSE:

```text
JANUS NODE / INTERFACE / RELAY / AUTHORIZED PATH
        -> candidate

FRESH REAL HEALTH CHECK
        -> exact receipt

SURVIVING END-TO-END PATH
        -> validated current path

HISTORICAL EXACT RECEIPTS
        -> learning context

JUXTAPOSE
        -> search-order / search-width proposal only

FRESH MEASUREMENT
        -> current-state authority
```

The ten-node swarm is therefore a plausible **physical falsification substrate** for JUXTAPOSE. It is not evidence that such validation has already happened.

---

## What JUXTAPOSE would not do to the Swarm

JUXTAPOSE is not proposed as a magic radio layer and does not justify rewriting the current swarm before evaluation.

It does **not**:

- strengthen RF power or create physical reachability where none exists;
- turn ESP-NOW into a protected tactical waveform;
- create anti-jam capability;
- automatically provide multi-hop routing if the underlying networking layer does not provide candidate paths;
- override packet ABI, node identity, authorization, sensor truth, or user-control gates;
- convert a stale historical route into a current viable route without a fresh check;
- prove battlefield resilience from synthetic results.

If all physical routes are unavailable, JUXTAPOSE cannot manufacture connectivity.

---

## Why include it in an xTech package if it is not integrated?

Because it answers a different reviewer question from the physical swarm.

The Swarm shows **what the developer already has in hardware and firmware**:

- a heterogeneous low-cost physical distributed system;
- real sensing and operator surfaces;
- peer/telemetry/state exchange;
- stale-state and recovery-oriented firmware paths;
- multiple specialized devices and constrained-resource workloads.

JUXTAPOSE shows **what the developer can already do at the algorithm / experimental-method level**:

- formalize a communications decision problem;
- separate prediction from current truth authority;
- preregister measurable gates;
- preserve negative lineage;
- run large frozen synthetic holdouts;
- build OOD and complete-outage controls;
- use explicit `UNKNOWN` states rather than forcing a conclusion;
- carry a candidate from abstract reasoning into a falsifiable hardware-validation question.

Together they demonstrate breadth without pretending that breadth is integration.

---

## xTech presentation rule

For xTech|Search 10, the preferred framing is:

> **JANUS Resilient Edge Swarm is the existing physical technology being presented. JUXTAPOSE is a separate, already-developed exact-backed adaptive communications-search architecture that demonstrates additional developer capability and offers a natural future validation experiment on the same heterogeneous hardware. It is not represented as already integrated into the current swarm.**

The current physical swarm should be shown **as-is**. No firmware rewrite is required to make the submission more impressive.

If xTech wants only the existing technology, the reviewer can evaluate the swarm exactly as it stands. If the reviewer is interested in follow-on adaptive communications research, JUXTAPOSE is available as an adjacent evidence-backed research asset whose next scientific gate is controlled real-network validation.

---

## Scientific and engineering claim ceiling

```text
JUXTAPOSE SYNTHETIC RESULT
!=
JANUS HARDWARE RESULT

JUXTAPOSE SEARCH POLICY
!=
CONNECTIVITY TRUTH

SEPARATE CAPABILITIES
!=
CURRENT INTEGRATION

ALGORITHM + HARDWARE TESTBED
!=
FIELD-PROVEN MILITARY SYSTEM
```

Current claim ceiling:

**JUXTAPOSE has reproducible local synthetic evidence sufficient to motivate independent evaluation and controlled hardware/network validation. JANUS provides an existing heterogeneous physical system that could serve as such a validation substrate. No field superiority, anti-jam, military-network certification, or already-integrated JUXTAPOSE-on-swarm claim is made.**

---

## What this says about the developer

This package intentionally lets the artifacts speak for themselves.

The claim is not "I can imagine a resilient swarm." The evidence is that a physical heterogeneous swarm already exists.

The claim is not "I could someday design an adaptive resilience algorithm." The evidence is that JUXTAPOSE already has a frozen, falsification-first synthetic lineage with positive and negative results.

The remaining question is empirical:

> **Do the useful properties survive contact with the real ten-node hardware?**

That is a testable question, and it can be answered without rewriting the existing swarm to fit the application.