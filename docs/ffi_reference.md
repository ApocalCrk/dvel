# DVEL FFI Reference (v0.1.0)

Header: `include/dvel_ffi.h` (C ABI). Types are POD with fixed-size arrays; caller owns any returned handles and must free them via the matching `*_free` function.

## Core types
- `dvel_hash_t`, `dvel_pubkey_t`, `dvel_sig_t`: byte arrays (32/64 bytes).
- `dvel_event_t`: `{ version, prev_hash, author, timestamp, payload_hash, signature }`; hash identity is `SHA256(canonical_bytes || signature)`.
- `dvel_ledger_t*`: opaque ledger handle; free with `dvel_ledger_free`.

## Ledger
- `dvel_ledger_new/free`: create/destroy an in-memory ledger.
- `dvel_ledger_add_event`: unchecked add (no linkage validation), returns computed hash.
- `dvel_ledger_link_event`: linkage-aware add; rejects duplicate or missing parent (unless genesis); writes hash to `out_hash`.
- `dvel_ledger_get_event`: fetch by hash into `out_event`.
- `dvel_ledger_get_tips`: enumerate current tips (write up to `out_capacity`).
- `dvel_ledger_merkle_root`: deterministic Merkle root over all event hashes.
- `dvel_hash_event_struct`: compute canonical event hash from struct fields.

## Validation
- `dvel_validation_ctx_t`: tracks last timestamp per author (caller stores/owns).
- `dvel_validation_ctx_init`: initialize context.
- `dvel_set_max_backward_skew`: set max allowed backward timestamp skew (ticks, min 1).
- `dvel_set_signing_key`: set deterministic signing key for simulator use.
- `dvel_sign_event`: ed25519 sign event with a 32-byte secret key.
- `dvel_validate_event`: checks protocol version, ed25519 signature, and bounded timestamp monotonicity.
- `dvel_derive_pubkey_from_secret`: derive ed25519 public key from 32-byte secret.

## Tip selection / scoring
- `dvel_weight_policy_t`: `DVEL_WEIGHT_UNIT` or `DVEL_WEIGHT_LATEST_PER_AUTHOR_UNIT`.
- `dvel_select_preferred_tip`: pick preferred tip under a local policy (no sybil overlay).
- `dvel_select_preferred_tip_sybil`: pick preferred tip with sybil-aware overlay.

## Sybil overlay & trace
- `dvel_sybil_overlay_new/free`: create/destroy overlay.
- `dvel_sybil_overlay_set_config`: override `dvel_sybil_config_t` fields.
- `dvel_sybil_overlay_observe_event`: feed an accepted event (must exist in ledger).
- `dvel_sybil_overlay_author_weight_fp`: fixed-point author weight at a tick.
- `dvel_trace_recorder_*`: create/free/clear/len/get rows; attach to overlay with `dvel_sybil_overlay_attach_trace_recorder`.

## Storage (chunk/sign/verify)
- `dvel_storage_chunk_file(input_path, out_dir, chunk_size_bytes, secret_key32, sign)`: chunk file, write chunks + `<file>.manifest` to `out_dir`; optional signing if `sign=true`.
- `dvel_storage_download(manifest_path, chunk_dir, output_path, expect_signer32)`: verify manifest/chunks (and optional signer) then reassemble to `output_path`.
- `dvel_storage_manifest_hash` / `dvel_storage_chunk_merkle_root`: compute manifest hash and chunk Merkle root for anchoring/audit.
- `dvel_storage_last_error(buf, buf_len)`: copy last error message (NUL-terminated if space permits); returns full message length.

## Notes
- All functions are deterministic and avoid heap crossing the FFI boundary.
- Callers must manage concurrency externally; the ledger and overlay are not thread-safe.
- Genesis is indicated by `prev_hash` = all-zero.
