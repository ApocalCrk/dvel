# Decentralized Verifiable Event Ledger (DVEL)
## Protocol Specification v0.1 (Draft)

> Status: Experimental
> Author: Community-driven
> Philosophy: Simplicity > Completeness

---

## 1. Background & Motivation

Most modern distributed systems are:
- overly complex
- dependent on economic incentives
- hard to audit
- difficult to reimplement

DVEL aims to be a **fundamental primitive**, similar to:
- Git (for code)
- TCP (for networking)
- Bitcoin (for minimal consensus)

DVEL is **NOT**:
- a currency
- a financial blockchain
- a smart contract platform

DVEL is a **machine for ordering and committing facts**.

---

## 2. System Goals

The system should provide:

1. An append-only ledger
2. Immutable, signed events
3. No trusted authority
4. Minimal consensus
5. Local verifiability
6. Easy reimplementation

---

## 3. System Model

### 3.1 Entities

- **Node**: an independent process
- **Event**: an atomic data unit
- **Ledger**: a chain of events
- **Peer**: another node

---

## 4. Identity & Cryptography

### 4.1 Node Identity

Each node holds a keypair: (private_key, public_key)

Node identity: NodeID = SHA256(public_key)

There is no:
- registry
- permissioning
- recovery mechanism

---

## 5. Event Structure

### 5.1 Event Definition

Event {
    version: u8
    prev_hash: Hash
    author: PublicKey
    timestamp: u64
    payload_hash: Hash
    signature: Signature
}

### 5.2 Event Hash (Canonical Identity)

Canonical bytes C(E):
```
C(E) = version || prev_hash || author || timestamp (LE) || payload_hash
```

Identity:
```
event_hash = SHA256( C(E) || signature )
```

Only the ledger computes and emits `event_hash`. Off-ledger re-hashing is not authoritative.

### 5.3 Event Validation

Validity is split in the reference system:
1. **Signature/format/time**: `version` matches, ed25519 signature verifies over `C(E)`, and timestamp is within a bounded monotonic window (checked by the validator).
2. **Linkage**: `prev_hash` is either all-zero (genesis) or already in the ledger (checked by the linkage-aware insert).

Both must hold before an event is accepted into the ledger.

---

## 6. Ledger Model

The ledger is a **single-parent DAG**: E0 -> E1 -> E2 -> ...; forks are permitted.

- Multiple tips may exist.
- A node selects a preferred tip (chain) locally.

Merkle commitment: the ledger exposes a Merkle root over all accepted `event_hash` values (lexicographically ordered leaves).

---

## 7. Consensus (LOVC)

### 7.1 Longest-Observed-Valid-Chain (baseline)

A node selects the chain `C` that maximizes: `Score(C) = |C|`

Subject to:
- all events are valid
- linkage hashes are correct

There is no:
- Proof-of-Work
- Proof-of-Stake
- voting

### 7.2 Sybil-aware Preference (Appendix A)

Nodes MAY apply latest-per-author weighting with equivocation quarantine (Appendix A) as a non-consensus overlay. This affects preference only, never validity.

---

## 8. Finality

Finality is probabilistic.

An event `E` is considered final when: depth(E) >= N

Default: N = 10

---

## 9. Network Layer

### 9.1 Transport

- TCP / QUIC
- Gossip-based

### 9.2 Message Types

HELLO(node_id)
EVENT(event)
CHAIN_REQUEST(from_hash)
CHAIN_RESPONSE(events[])
PING

---

## 10. Threat Model (HONEST ABOUT RISKS)

### 10.1 Minimal Assumptions

- Nodes may be malicious
- The network is unreliable
- There is no trusted clock
- There are no trusted peers

---

## 11. CRITICAL WEAKNESSES v0.1

### 11.1 Sybil Attack (FATAL)

Because there is no identity cost:
- A single attacker can create many nodes
- Dominate gossip
- Influence the longest chain

> v0.1 is **NOT** Sybil-resistant

Appendix A provides non-consensus friction (latest-per-author, quarantine) but does not change validity.

---

### 11.2 No Economic Finality

There is no:
- slashing
- punishment
- cost to reorganization

Consequences:
- reorgs are cheap
- long-range attacks are trivial

---

### 11.3 Timestamp Manipulation

Timestamps are not trusted:
- an attacker can forward/backward time
- only local tolerance bounds apply

---

## 12. Proof Readiness (Binius-style, non-normative)

To support a binary-field proof system (e.g., Binius):

- Execution trace: per accepted event record `(prev_hash, author, timestamp, payload_hash, signature, parent_present, ancestor_check_result, quarantined_until_before/after, merkle_root_before/after, preferred_tip)` in deterministic order.
- Hash: identity is `SHA256(C(E)||sig)` (as above). A proof circuit must either model SHA as a lookup/gadget or replace it with a binary-field-friendly hash in an extension spec.
- Constraints:
  - Signature verifies over `C(E)`.
  - `parent_present` is true unless `prev_hash == ZERO`.
  - Equivocation quarantine fires iff two tips of the same author are non-ancestors within `max_link_walk`.
  - Author weight is fixed-point, warmup ramped, zeroed during quarantine.
  - Merkle root recomputes deterministically over accepted hashes.
- Commitments: publish ledger Merkle root and (optionally) a commitment to the execution trace for external proof systems.

---

### 11.4 Spam Events

Since events are cheap: Cost(event) ~ 0

Without weighting -> easy to manipulate

---

### 11.6 Eclipse Attack

Nodes can be:
- isolated by malicious peers
- fed a false reality

---

## 12. Why Build This Anyway?

Because:

1. All weaknesses are explicit
2. All are easy to analyze
3. The system can evolve
4. Suitable for:
   - research
   - educational infrastructure
   - experimental base layer

---

## 13. Design Principles

1. Do not hide weaknesses
2. Do not over-engineer
3. Everyone can read and run their own node
4. No marketing roadmap

---

## 14. License & Open Source

- MIT / Apache 2.0
- Reference implementation only
- The specification is more important than the code

---

## 15. Closing Statement

> This system does not promise absolute security.
> It only promises **structural honesty**.

If the system fails,
it fails openly.

---
