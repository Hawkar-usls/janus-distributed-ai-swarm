# JANUS Portfolio Genesis Faces — read-only pilot

This pilot gives each currently listed Hawkar-usls public repository a stable **logical
Face identity** and an exact default Git ref. **It does not run 50 simultaneous
VMs.** The PR workflow exercises three Face identities on temporary GitHub
Actions workers; the remaining 47 are configured but dormant.

## Canonical ancestry

- Genesis \`protocol/JANUS_PROJECT_MULTI_FACE_COORDINATION-v1.0.json\`: routing,
  SHA-pin, HOLD, and zero authority delta.
- Genesis \`protocol/JANUS_GENESIS_THIRD_WISH_MEMORY_IS_NOT_OWNERSHIP_MESSAGE_IS_NOT_COMMAND-v1.0.json\`:
  MESSAGE != COMMAND; RECEIVE != CONSENT.
- Meta Registry \`data/JANUS-FACE-COUNCIL-GIT-COORDINATION-v1.0.json\`:
  durable Git-visible coordination is not runtime authority.
- Meta Registry \`data/JANUS-GENESIS-GUARDIAN-MESH-FACE-ORCHESTRATION-v1.0.json\`:
  GENESIS orchestrates N Faces; number of Faces does not increase authority.

See exact URLs in \`configs/portfolio_genesis_faces_v1.json\`. This is a
small compatible transport adapter, not an alternative authority model.

## How the GitHub canary communicates

1. **Plan:** validate the 50-repository roster and construct a stable
   3-Face ring (Genesis → HRAiN → iNaiHR → Genesis).
2. **Observe:** separate Actions jobs retrieve the public Git HEAD by
   \`git ls-remote\`. No target checkout, model inference or target write.
3. **Route:** a separate job validates the exact packet shape, sender,
   recipient, source SHA and zero-authority fields, then writes mailboxes.
4. **Receive:** each recipient runs in a separate ephemeral Actions job,
   consumes the mailbox without executing its content, and returns an ACK.
5. **Witness:** a separate job checks that every expected original packet and
   recipient ACK is present and matches its deterministic digest. A result is
   a *delivery receipt*, not scientific truth, authentication of a person,
   model synthesis, or authorization to deploy code.

The transport is GitHub Actions artifacts, which expire; this pilot is **not
durable cross-run memory**. Production-grade coordination must independently
verify remote provenance and append verified lineage to Meta Registry.

## Safety

- Automatic selection is always the three-Face canary.
- Maximum active worker parallelism in this workflow: three.
- Actions token has only \`contents: read\` permissions.
- No cloud LLM/Copilot invocation; no paid Rowan experiment.
- No workflow writes to source repositories, Meta Registry or main branches.
- A message cannot grant authority, truth, effect budget or permission.
- Missing/duplicate/altered packets or ACKs fail closed.
- The full 50-Face matrix is inspectable offline using:

\`\`\`shell
python tools/portfolio_genesis_face_bus.py matrix --cohort all
python -m unittest -v tests/test_portfolio_genesis_face_bus.py
\`\`\`

The \`all\` cohort has no live dispatch option in this pilot; first pass the
three-worker real Actions canary and independent receipt. Independent
cross-repository authorization, crash recovery across workflow runs, exact
state CAS and economic scheduling remain separate future gates.
