# DVEL v0.1 - Spec Hygiene Pass (Normative Language)

> Status: Draft
> Scope: Pre-implementation
> Purpose: Normalize normative language across the specification

---

## H.1 Purpose

This document performs a **spec hygiene pass** on DVEL v0.1.

Its goals are to:

* Eliminate ambiguous language
* Standardize normative terms
* Prevent divergent interpretations

No protocol behavior is changed.

---

## H.2 Normative Keywords

The following keywords are used as defined in RFC 2119:

* **MUST** / **MUST NOT** - absolute requirement
* **SHOULD** / **SHOULD NOT** - strong recommendation
* **MAY** - optional behavior

All normative meaning in DVEL is expressed exclusively using these terms.

---

## H.3 Core Spec Hygiene Notes

### H.3.1 Node Identity (Section 4)

* Each node **MUST** generate a cryptographic keypair.
* NodeID **MUST** be derived as `SHA256(public_key)`.
* Nodes **MUST NOT** share private keys.

---

### H.3.2 Event Structure (Section 5)

* Events **MUST** include all defined fields.
* Event hashes **MUST** be computed exactly as specified (canonical bytes excluding signature, then hash = SHA256(canonical || signature)).
* Event hashes **MUST NOT** be recomputed off-ledger with alternative material.
* Event signatures **MUST** verify against the `author` public key.

---

### H.3.3 Event Validation (Section 5.3)

An event is valid if and only if:

* `signature` **MUST** verify correctly
* `timestamp` **MUST NOT** be extremely non-monotonic

Linkage (`prev_hash` exists unless genesis) **MUST** be enforced at ledger insertion time. Nodes **MUST NOT** propagate invalid or unlinkable events as accepted.

---

### H.3.4 Ledger Model (Section 6)

* Nodes **MUST** store valid events immutably.
* Nodes **MAY** maintain multiple forks simultaneously.
* Nodes **MUST NOT** discard valid history.
* Nodes **MUST** expose a deterministic Merkle root over accepted event hashes.

---

### H.3.5 Consensus Rule (Section 7)

* Nodes **MUST** be able to apply the baseline LOVC rule.
* Nodes **MAY** apply alternative preference heuristics.
* Preference heuristics **MUST NOT** affect event validity.

---

### H.3.6 Finality (Section 8)

* Finality **MUST** be treated as probabilistic.
* Nodes **SHOULD** expose finality depth as configuration.

---

### H.3.7 Network Layer (Section 9)

* Nodes **MUST** validate all received events.
* Nodes **SHOULD** support gossip-based propagation.
* Nodes **MUST NOT** assume reliable or ordered delivery.

### H.3.8 Proof Trace (Optional, Non-Consensus)

* Implementations **MAY** emit a deterministic execution trace for external proofs (e.g., Binius).
* If emitted, the trace **MUST** reflect ledger-accepted hashes only and be deterministic under fixed inputs.
* Proof hooks **MUST NOT** alter validity or preference semantics.

---

## H.4 Appendix A Hygiene Notes

* All Sybil mitigation mechanisms **MUST** be treated as non-consensus.
* Implementations **SHOULD** allow all heuristics to be disabled.
* Heuristics **MUST NOT** cause valid events to become invalid.

---

## H.5 Threat Model and Adversary Model

* Threat tables **MUST** be treated as descriptive, not prescriptive.
* Adversary assumptions **MUST NOT** be weakened silently by implementations.

---

## H.6 Language Cleanup Summary

The following terms are explicitly avoided:

* "secure"
* "safe"
* "trustless"
* "Byzantine fault tolerant"

These terms are replaced with explicit properties or limitations.

---

## H.7 Compliance Requirement

An implementation claiming DVEL v0.1 compliance:

* **MUST** follow all MUST / MUST NOT statements
* **SHOULD** justify deviations from SHOULD statements
* **MAY** implement optional behavior explicitly

---ison the Protocol v0.1 spec.

---

## H.8 Closing Statement

This hygiene pass exists to ensure:

* One protocol
* Many honest implementations

Ambiguity is treated as a defect.

---

> A protocol fails first in language,
> long before it fails in code.
