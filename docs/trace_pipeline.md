# Trace Pipeline: sim -> merge -> check -> prove

## Purpose
Deterministically produce a proof-ready execution trace from the simulator and sanity-check it before sending to a prover.

## Steps
1. Build `rust-core` and `sim_sybil` (as done in the repo).
2. Run the pipeline:
   ```
   cpp-sim/tools/run_trace_pipeline.sh
   ```
   This:
   - runs `sim_sybil` (emits `trace_sybil_node*.json` in `cpp-sim/build/`)
   - merges per-node traces into `merged_trace.json`
   - runs invariant checks on the merged trace
   - computes a deterministic SHA256 commitment over header + rows (`trace_commitment.txt`)

3. Consume `cpp-sim/build/merged_trace.json` in your prover.
   - The helper `cpp-sim/tools/prover_stub.py` emits `trace_commitment_sha256=...` (also saved to `trace_commitment.txt`).

## Artifact format
- `header`:
  - `protocol_version`, `max_backward_skew`, `max_pending_total`, `max_drain_steps`
  - `sybil_config`: `warmup_ticks`, `quarantine_ticks`, `fixed_point_scale`, `max_link_walk`
  - `final_merkle_root`: last non-null Merkle root observed
  - `sources`: per-node trace filenames
- `rows`: sorted by `(timestamp, node_id, row_index)`, each with:
  - `node_id`, `row_index`
  - `prev_hash`, `author`, `timestamp`, `payload_hash`, `signature`
  - `parent_present`, `ancestor_check`
  - `quarantined_until_before`, `quarantined_until_after`
  - `merkle_root` (or null), `preferred_tip` (or null)
  - `author_weight_fp`

## Checks performed
- parent flag matches prev_hash zero/non-zero
- quarantine window increments when `ancestor_check` is false
- author weight within [0, fixed_point_scale] and zero during quarantine
- final Merkle root matches the last non-null row root

## Prover hints
- Public input: `header.final_merkle_root` (and optional commitment to the full trace).
- Constraints: signature over canonical bytes, parent_present unless genesis, ancestor_check gates quarantine, fixed-point weights, deterministic Merkle recompute, sybil config params fixed.
- SHA256 is still the identity hash; model it in-circuit or replace with a binary-friendly hash in an extension spec.
