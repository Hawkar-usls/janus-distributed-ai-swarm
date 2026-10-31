# JANUS Brain Five — GitHub Actions source-bound Face bus

This pilot extends the already-merged 50-repository Genesis Face transport. Five
distinct ephemeral Actions jobs represent core JANUS brain organs in a directed
ring. This architecture does **not** run five live AI models and does not grant
cross-repository write access.

| Organ | Source-bound evidence | Role |
|---|---|---|
| Demi_Head | `docs/ARCHITECTURE.md` | Bicameral guardian / discrepancy comparator |
| AIFC | `conformance/AIFC-ADMISSION-AUTHORITY-ORDER-v1.json` | Evidence-admission authority boundary |
| Janus | `docs/JANUS_MCP_GATEWAY.md` | Integration / MCP tool gateway |
| janus-io-public | `docs/proof-of-observation-origin-record.json` | I0 observation provenance |
| janus-distributed-ai-swarm | `docs/architecture.md` | Distributed worker transport |

## Worker-to-worker transport

In `.github/workflows/validate-brain-five-face-bus.yml`:
1. `plan` validates the five source/role/path allowlist against the 50-Face manifest.
2. Five independent `brain-observe` GitHub Actions jobs query an exact public
   branch HEAD and read a small contract document by **immutable commit SHA**.
   No target repository code is executed.
3. `brain-route` **independently re-reads** the exact SHA-pinned contract,
   verifies file SHA-256, recipient, role, schema and zero authority deltas,
   then routes one message to each next organ via Actions artifacts.
4. Five separate `brain-receive` jobs validate their mailboxes and emit
   deterministic, non-executing ACK receipts.
5. `brain-independent-witness` re-reads the immutable source documents and
   validates all five original messages and exactly five ACKs. Its JSON
   receipt records the source Git commits and contract SHA-256 hashes.

The cohort operates on an explicit five-organ directed ring:
Demi_Head → AIFC → Janus → janus-io-public → janus-distributed-ai-swarm → Demi_Head.
This is an evidence transport contract, not an authority/deployment channel.

## Safety and boundaries

- All five logically dedicated jobs use GitHub-managed ephemeral workers.
  The workflow caps concurrent organ jobs at three to bound resource demand.
- Only public SHA-pinned source text/JSON is read, with a 128 KiB file cap.
- No model invocation, Copilot quota use, Rowan spending, target shell execution,
  target repository write, default schedule or external cloud infrastructure.
- Token permissions: `contents: read` only.
- Duplicate sender messages, spoofed roles, mismatched SHA/file digest,
  missing ACK, unknown sources and impossible permission fields fail closed.
- A source commit and SHA-256 checksum are verifiable **document identity**,
  not a proof that the document is scientifically true or current.
- CI success demonstrates bounded cross-worker message delivery only.
  The next gate for useful actual brain cognition requires independent
  adapters which execute each organ's **approved, frozen, read-only**
  domain tests, and separately qualify substantive evidence quality.
- Actions artifacts expire; durable cross-run memory, lease fencing,
  cross-repo authentication and Meta Registry writeback are **not** enabled.

## Local offline checks

```bash
python -m unittest -v tests/test_brain_five_face_bus.py
python tools/brain_five_face_bus.py matrix
```

The implementation follows these existing Genesis and Meta Registry laws:
`MESSAGE != COMMAND`, `FACE_COUNT != AUTHORITY`, `RECEIPT != PERMISSION`,
`PROPOSAL != WORLD_COMMIT`. No duplicate global controller is introduced.
