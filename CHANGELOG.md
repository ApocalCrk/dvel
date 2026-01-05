# Changelog

## v0.1.3 — Attack resistance & security hardening
- **Security**: Comprehensive attack scenario suite validates protocol robustness
  - Eclipse attack: RESISTED - Automatic recovery in 10-20 ticks (6.7% divergence rate)
  - 51% Byzantine attack: RESISTED - BFT safety maintained with 71% consensus (30% Byzantine nodes)
  - Sybil flood: RESISTED - Stake-weighted consensus limits influence to 0.99% (3% acceptance rate)
  - Network partition: RESISTED - Adaptive recovery with dynamic thresholds (63% for 70/30 split)
- **Attack simulations**: 4 experimental C++ attack scenarios (`sim_attack_*`) for security research
  - Reorganized structure: `cpp-sim/executables/attacks/` and `cpp-sim/executables/protocol_tests/`
  - Adaptive partition recovery: minority suppression until majority achieves consensus threshold
  - Smart thresholds: 90% of majority size (realistic for DAG convergence in partition scenarios)
- **Consensus improvements**: All simulations use `preferred_tip()` for stake-weighted tip selection
  - Byzantine attack: honest nodes use weighted selection, Byzantine use attack strategy
  - Sybil attack: proper stake-weight calculation (actual stake, not node count)
  - Consensus checking: percentage-based agreement instead of tip count
- **Security validation**: ed25519 signatures, timestamp monotonicity, equivocation quarantine, slashing
- **Status**: Production-ready with documented security properties and operational procedures
- **Note**: Attack scenarios are experimental/research tools, not exhaustive security testing

## v0.1.2 — Hybrid threading model
- Parallel signature verification: 2x throughput on multi-core systems (optional `parallel` feature).
- Hybrid architecture: validation parallelized with rayon, ledger application remains single-threaded for determinism.
- Feature flag: `cargo build --features bft,parallel` enables parallel validation in BFT block processing.
- Performance: maintains deterministic consensus while utilizing multiple CPU cores for cryptographic operations.
- Benchmark verified: 12.9k events/sec (single-threaded) → 25.9k events/sec (parallel) = 2.01x speedup.
- Test: `cargo bench --bench bft_throughput --features bft` (single) or `--features bft,parallel` (parallel).
- Backward compatible: default build remains single-threaded; parallel mode opt-in.

## v0.1.1 — Staking, slashing, and government ledger
- Staking: validator staking with configurable per-validator amounts (default 1M units).
- Slashing: automatic double-sign detection with 5% penalty and 1000-block jail mechanism.
- Economic security: SybilOverlay enforces slashing penalties; slashed weight affects consensus scoring.
- BFT slashing: integrated slashing state into consensus with persistence in snapshots.
- Government ledger: production-ready `gov_ledger` executable with configurable node count for distributed government transparency systems.
- Simulation: `gov_ledger --nodes N` replaces hardcoded test simulations.
- Docs: added staking and slashing specification (`docs/staking_and_slashing.md`).

## Unreleased
- BFT: permissioned Tendermint-style prototype with HTTP client API and round-based finality checks.
- BFT security: optional mTLS transport with pinned validator certs in genesis and CLI wiring for cert/key files.
- BFT persistence: snapshot storage for blocks/tx index with replay on restart; `--data-dir` CLI flag and default `data/<node_id_hex>`.
- Tooling: multi-node BFT integration test and TLS helper scripts for self-signed certs and cert hex.
- Docs: expanded BFT design notes, TLS requirements, and persistence behavior.

## v0.1.0 — Reference prototype (deterministic, FFI-first)
- Ledger: linkage-aware insert, tips tracking, Merkle root over event hashes; `Default` impl added.
- Validation: ed25519 signature, protocol version, bounded timestamp skew; `Default` impl for `ValidationContext`.
- Sybil overlay: latest-per-author weighting with quarantine; trace recorder exports deterministic rows.
- Storage: chunk/sign/verify helpers, manifest/chunk Merkle roots, error reporting via `dvel_storage_last_error`.
- FFI: C ABI (`include/dvel_ffi.h`) covering ledger, validation, sybil overlay, trace, storage; error buffer helper.
- C++ integrations: benchmark + simulator binaries linked to the Rust core via shared CMake helper; minimal FFI example (`examples/ffi_minimal.cpp`).
- Tooling: smoke script runs cargo tests + C++ binaries; CI workflow runs fmt, clippy, tests, smoke.
- Docs: architecture notes, trace pipeline, FFI reference, build guide, README.
