# Appendix A: Non-Consensus Sybil Friction (Draft)

> Status: Draft
> Scope: Pre-implementation
> Applies to: Protocol Specification v0.1

---

## A.1 Purpose

This appendix documents **non-consensus mechanisms** intended to reduce the *immediate impact* of Sybil attacks in DVEL.

These mechanisms:

* DO NOT change event validity rules
* DO NOT introduce global consensus requirements
* DO NOT guarantee Sybil resistance
* ONLY affect **local node behavior**

The goal is **friction, not prevention**.

---

## A.2 Design Constraints

The following constraints are intentionally enforced:

* No Proof-of-Work
* No Proof-of-Stake
* No token economics
* No global identity registry
* No human verification
* No mandatory reputation system

All mechanisms must be:

* Locally computable
* Optional
* Parameterizable per node

---

## A.3 Terminology

* **Validity**: Whether an event satisfies protocol rules (Section 5)
* **Preference**: Local heuristics used to select a chain among valid chains
* **Sybil Identity**: A node identity generated at negligible cost

Validity is global.
Preference is local.

---

## A.4 Identity Observation Window

Each node maintains a local observation record for authors it encounters.

```
ObservedIdentity {
    author: PublicKey
    first_seen_time: u64
    observed_event_count: u64
}
```

This record is **local-only** and MUST NOT be gossiped or agreed upon.

---

## A.5 Identity Maturity Heuristic

### A.5.1 Definition

Nodes MAY assign a maturity factor to event authors based on local observation time.

```
maturity(author) = min(1, (now - first_seen_time) / T_mature)
```

Where:

* `T_mature` is a locally chosen constant
* `now` is the node's local time

### A.5.2 Usage

Maturity MUST NOT affect event validity.

Nodes MAY use maturity to:

* Weight events when selecting a preferred chain
* Deprioritize gossip from newly observed identities

### A.5.3 Rationale

This introduces a **temporal cost** to identity creation without requiring cryptographic work or economic stake.

---

## A.6 Event Rate Friction

### A.6.1 Observation

Nodes MAY track the rate of events authored by a given identity over a sliding window.

```
event_rate(author, delta_t)
```

### A.6.2 Soft Limiting

If an author's event rate exceeds a local threshold `K`, nodes MAY:

* Reduce the local preference weight of excess events
* Delay propagation of excess events
* Ignore excess events for chain preference while still storing them

Hard rejection is NOT RECOMMENDED.

### A.6.3 Rationale

This prevents linear amplification of influence via event spamming while preserving append-only semantics.

---

## A.7 Local Chain Preference Weighting

Nodes MAY compute chain preference using a weighted score rather than raw length.

```
Score(C) = sum(local_weight(event_i))
```

Where `local_weight(event)` MAY incorporate:

* Author maturity
* Event rate considerations
* Peer trust signals

Nodes are NOT REQUIRED to converge on identical scoring functions.

---

## A.8 Peer Trust Heuristics (Optional)

Nodes MAY maintain local peer scores based on:

* Connection uptime
* Ratio of valid to invalid events received
* Consistency with previously observed chains

Peer trust scores MAY influence:

* Gossip priority
* Chain synchronization decisions

Peer trust MUST NOT affect event validity.

---

## A.9 Security Properties and Limitations

These mechanisms:

* DO NOT prevent Sybil identities
* DO NOT stop long-range attacks
* DO NOT guarantee convergence

They DO:

* Increase the time cost of Sybil dominance
* Reduce the effectiveness of spam flooding
* Mitigate trivial eclipse attacks

Attackers remain unconstrained by design.

---

## A.10 Implementation Guidance (Non-Normative)

Reference implementations are encouraged to:

* Enable these heuristics by default
* Expose all parameters for configuration
* Log heuristic effects for auditability
* Keep weighting fixed-point and hash identity single-sourced (ledger) to remain proof-friendly

Disabling all heuristics MUST result in behavior equivalent to core v0.1.

---

## A.11 Design Philosophy Note

DVEL intentionally separates:

* What is **valid**
* From what is **preferred**

This appendix exists to document common preference strategies without elevating them to protocol law.

Failure modes remain explicit.

---

> This appendix does not make DVEL safe.
> It makes DVEL **honest under pressure**.
