# DVEL v0.1 - Minimal Adversary Model

> Status: Draft
> Scope: Pre-implementation
> Applies to: Core Spec v0.1 + Appendix A

---

## M.1 Purpose

This document defines the **minimal adversary model** assumed by DVEL.

It exists to:

* Make security assumptions explicit
* Prevent accidental over-claims
* Guide reference implementation behavior

This is **not** a formal proof model.
It is an *engineering-grade* threat assumption set.
External proofs (e.g., Binius-style execution proofs) do not change this adversary model; they only attest to correct execution under the same assumptions.

---

## M.2 Design Philosophy

DVEL assumes:

* Adversaries exist
* Adversaries are rational but not necessarily economic
* Adversaries may control infrastructure
* Adversaries are not bounded by honesty

Security is treated as **relative degradation**, not absolute safety.

---

## M.3 Adversary Capabilities (Assumed)

An adversary MAY:

1. Generate arbitrary numbers of keypairs
2. Run many nodes on commodity hardware
3. Control network timing and message ordering
4. Withhold, delay, or replay messages
5. Maintain long-lived identities
6. Pre-compute alternative chains offline
7. Collude across identities

These capabilities are assumed by default.

---

## M.4 Adversary Constraints (Assumed)

An adversary is constrained by:

* Physical time (cannot skip real time)
* Finite bandwidth per node
* Finite concurrent connections per node
* Observability by honest peers over time

There is **no assumption** of:

* Economic cost
* Identity scarcity
* Trusted clocks
* Trusted peers

---

## M.5 Adversary Classes

### M.5.1 Opportunistic Attacker

Characteristics:

* Short-lived
* Low preparation
* Reactive

Examples:

* Spam flooding
* Instant Sybil swarm

Appendix A is primarily designed against this class.

---

### M.5.2 Prepared Attacker

Characteristics:

* Identity pre-aging
* Coordinated release
* Moderate bandwidth

Examples:

* Gradual Sybil accumulation
* Strategic chain extension

Appendix A partially degrades this attacker.

---

### M.5.3 Determined Attacker

Characteristics:

* Long-term preparation
* Network-level manipulation
* Targeted eclipse

Examples:

* Long-range reorganization
* Sustained isolation of nodes

DVEL does NOT claim resistance against this class.

---

## M.6 Honest Node Assumptions

Honest nodes are assumed to:

* Validate all received events
* Apply local heuristics consistently
* Maintain multiple peer connections
* Prefer liveness over strict convergence

Honest nodes are NOT assumed to:

* Agree globally
* Share trust scores
* Detect adversaries reliably

---

## M.7 Time Assumptions

* No global clock is trusted
* Local clocks may drift
* Time is only used for *relative heuristics*

Time-based mechanisms MUST tolerate bounded skew.

---

## M.8 Network Assumptions

The network is assumed to be:

* Asynchronous
* Partitionable
* Adversarially schedulable

Message delivery is:

* Unordered
* Delayed
* Possibly duplicated

---

## M.9 Security Claims (Explicitly Limited)

Under this adversary model, DVEL claims:

* Events are locally verifiable
* History cannot be silently rewritten
* Instantaneous domination is degraded

DVEL does NOT claim:

* Finality against prepared attackers
* Global agreement
* Resistance to long-range attacks

---

## M.10 Implications for Implementation

Reference implementations SHOULD:

* Avoid assumptions stronger than this model
* Expose heuristic parameters
* Log deviations and reorgs

Any stronger security guarantees MUST be documented as extensions.

---

## M.11 Closing Statement

This adversary model is intentionally pessimistic.

If DVEL survives under these assumptions,
it survives honestly.

---

> DVEL does not assume good actors.
> It assumes *reality*.
