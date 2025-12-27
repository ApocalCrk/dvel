# DVEL Reference Prototype Notes (v0.1)

## End-to-end flow
```
Event E = (prev, author, ts, payload, sig)
      |   (canonical_bytes(E) excludes sig)
      v
H := SHA256( canonical_bytes(E) || sig )
      |   (computed once by ledger)
      v
FFI exposes {E, H, MerkleRoot}
      |
      v
Sybil overlay consumes {ledger, H} and updates author weights
      |
      v
Tip selection (latest-per-author + quarantine) -> next parent choice
```

## Canonical identity
- Hash material: `H = SHA256(C(E) || sig(E))` with `C(E)` omitting the signature slot.
- Hash is single-source: only the ledger emits `H`; sims never re-derive identity.
- Signatures: ed25519 over `C(E)`; verification keyed by `author`, with bounded skew on `ts`.

## Equivocation handling
- Definition: same author, two children that are not ancestors of each other within `max_link_walk`.
- Detection: if `!ancestor(prev_tip, new_tip)` and `!ancestor(new_tip, prev_tip)` then set `quarantined_until = now + Q`.
- Effect: `weight(author, t) = 0` for `t < quarantined_until`; else linear warmup to 1 over `warmup_ticks`.

## Merkle commitment
- Ledger folds all accepted event hashes into a Merkle root; FFI exports it.
- Sims log the root each tick for deterministic audit (root changes iff accepted set changes).

## Simulator knobs (C++)
- `MAX_BACKWARD_SKEW`: large to suppress false timestamp rejects under adversarial delivery.
- Pending pool caps (`MAX_PENDING_TOTAL`, `MAX_DRAIN_STEPS`) widened for liveness under burst gossip.
- Sybil overlay config (`warmup_ticks`, `quarantine_ticks`, `fixed_point_scale`, `max_link_walk`) set once at bootstrap.

## Proof readiness (Binius-style, sketch)
- Emit a deterministic per-accepted-event trace: `(prev_hash, author, ts, payload_hash, sig, parent_present, ancestor_check, quarantined_until_before/after, merkle_root_before/after, preferred_tip, author_weight_fp)`.
- Identity hash is `SHA256(C(E) || sig)` and is single-sourced from the ledger; a proof circuit must model SHA or replace it in an extension spec.
- All weights remain fixed-point integers; no floating point in the ledger path.
- Constraints: signature over `C(E)`, parent exists unless genesis, quarantine triggers on non-ancestor sibling tips, Merkle root recomputes deterministically.
- Commitments: publish ledger Merkle root; optionally commit to the execution trace for an external prover.

## Minimal equivocation test (constructive)
1. Create `e1`, `e2` with identical `(author, prev)` and distinct payload/timestamp; sign both.
2. Link `e1`, then `e2` (order arbitrary) into the ledger.
3. Overlay must drive `weight(author)=0` for >= `quarantine_ticks` and keep latest-per-author tip updated.

## Checklist (equivocation must be detectable if all hold)
- `H` includes `sig` and is computed once by the ledger.
- Validation enforces protocol version, ed25519 signature, and bounded timestamp skew.
- Overlay observes ledger-accepted `H` and tests ancestor relation for consecutive tips per author.
- Quarantine window `>0` and applied on divergent siblings.
- Tip selection uses sybil-aware weighting (latest-per-author + quarantine).
- Sims sign before send; no post-sign mutation or off-ledger rehashing.

## Storage FFI (C++ control path)
- Rust lib now exposes chunk/manifest/sign/verify over C ABI (`include/dvel_ffi.h`):
  - `dvel_storage_chunk_file(input_path, out_dir, chunk_size, secret32, sign)` -> writes chunks + `<file>.manifest` in `out_dir`.
  - `dvel_storage_download(manifest_path, chunk_dir, output_path, expect_signer32)` -> verify manifest/chunks (+signature if present) then reassemble.
  - `dvel_storage_manifest_hash(manifest_path)` -> SHA256 of canonical manifest (use as payload/anchor).
  - `dvel_storage_chunk_merkle_root(manifest_path)` -> Merkle root over chunk hashes (sorted, pairwise fold).
  - `dvel_storage_last_error(buf, len)` -> copy last error string for diagnostics.
- C++ keeps control: call the storage FFI from sim/daemon, manage sync/transport of chunks + manifest between nodes (e.g., rsync/Syncthing/gossip), and optionally anchor `manifest` hash in ledger events or trace logs.
