# SWARM_GENOME_LEDGER

The distributed swarm now carries a shared genealogy above its per-node `SpiralLedger` histories.

Each spiral turn contributes two paired strands:

- identity/state — current role, capabilities and active state;
- evidence/provenance — candidate state, outcome, lessons, constraints and recovery history.

The shared ledger supports traversal in both directions: ancestor to descendants, and current node back to every origin path. Explicit specialization/synthesis links may join different entity lineages without replacing their parents.

This layer changes memory and lineage semantics only. It does not alter sensor truth, SHA-256 work, target math, Stratum, pool submission, firmware flashing or external-effect authority.
