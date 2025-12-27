# DVEL v0.1 - Reference Implementation Contract

> Status: Draft
> Scope: Pre-implementation
> Purpose: Define mandatory vs optional behaviors for the first reference implementation

---

## R.1 Purpose

This document defines the **implementation contract** for the first DVEL reference node.

Its goal is to:

* Prevent ambiguous interpretations of the spec
* Avoid silent divergence between implementations
* Establish a common behavioral baseline

This document specifies **behavioral requirements**, not code structure.

---

## R.2 Implementation Philosophy

The reference implementation is:

* A correctness oracle, not a performance benchmark
* A pedagogical artifact, not a production client
* Conservative in assumptions

Simplicity and auditability are prioritized over optimization.

---

## R.3 Core Protocol Requirements

### R.3.1 Cryptography

The implementation MUST:

* Generate a public/private keypair per node
* Derive NodeID = SHA256(public_key)
* Verify event signatures strictly over canonical bytes
* Compute event_hash as SHA256(canonical_bytes || signature) exactly once in the ledger

The implementation MUST NOT:

* Accept unsigned events
* Accept events with invalid linkage hashes

---

### R.3.2 Event Validation

The implementation MUST validate events according to Section 5:

* Signature correctness
* Bounded timestamp monotonicity

Linkage (`prev_hash` exists unless genesis) MUST be enforced when inserting into the ledger. Invalid or unlinkable events MUST be rejected locally and MUST NOT be propagated as accepted.

---

### R.3.3 Ledger Handling

The implementation MUST:

* Maintain an append-only local event store
* Allow multiple forks to coexist
* Track chain tips independently
* Maintain a Merkle root over accepted event hashes (lexicographically ordered leaves)

The implementation MUST NOT:

* Enforce global chain agreement
* Delete valid historical events

---

## R.4 Consensus and Chain Selection

### R.4.1 Baseline Behavior

The implementation MUST support the baseline LOVC rule:

```
Score(C) = |C|
```

This baseline MUST be available as a configuration option.

---

### R.4.2 Heuristic Extensions

The implementation SHOULD:

* Support weighted chain preference as described in Appendix A
* Allow heuristic parameters to be configured

The implementation MUST:

* Treat all heuristics as non-consensus
* Allow heuristics to be disabled entirely

### R.4.3 Proof Readiness (Binius-style, optional)

The implementation SHOULD expose a deterministic execution trace for external proof systems:

* Per-accepted-event record: `(prev_hash, author, timestamp, payload_hash, signature, parent_present, ancestor_check_result, quarantined_until_before/after, merkle_root_before/after, preferred_tip, author_weight_fp)`.
* All weights MUST be fixed-point integers (no floating point).
* If provided, the trace MUST be deterministic under fixed config and input.
* Identity hash MUST remain single-sourced from the ledger (`SHA256(canonical || sig)`).

---

## R.5 Networking Requirements

### R.5.1 Peer Connections

The implementation MUST:

* Support multiple concurrent peer connections
* Use gossip-based message propagation

The implementation SHOULD:

* Randomize peer selection
* Maintain peer diversity

---

### R.5.2 Message Handling

The implementation MUST:

* Validate all received events before processing
* Deduplicate events by hash

The implementation MUST NOT:

* Trust peer-provided metadata blindly

---

## R.6 Sybil Friction Heuristics (Appendix A)

### R.6.1 Identity Observation

The implementation SHOULD:

* Track first-seen time per author
* Track observed event counts per author

This data MUST remain local.

---

### R.6.2 Event Rate Handling

The implementation SHOULD:

* Implement soft rate limiting per author
* Degrade preference weight rather than reject events

---

### R.6.3 Peer Trust (Optional)

The implementation MAY:

* Maintain peer trust scores
* Use them for gossip prioritization

Peer trust MUST NOT affect event validity.

---

## R.7 Configuration and Observability

The implementation SHOULD:

* Expose configuration for all heuristic parameters
* Log reorgs, forks, and preference changes

The implementation MUST:

* Allow deterministic behavior under fixed configuration

---

## R.8 Non-Goals

The reference implementation MUST NOT:

* Claim production readiness
* Implement economic incentives
* Attempt to solve long-range attacks

---

## R.9 Compliance Statement

An implementation is considered **DVEL v0.1 compliant** if it:

* Fully implements Core Spec v0.1
* Honors the MUST/MUST NOT requirements in this document
* Clearly documents any deviations

---

## R.10 Closing Note

This contract exists to ensure that the first implementation reflects
**the spirit and limits of the specification**.

Any stronger guarantees must be explicit extensions.

---

> The reference implementation is not the protocol.
> It is the protocol made inspectable.
