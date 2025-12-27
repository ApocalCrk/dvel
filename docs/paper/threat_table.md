# DVEL v0.1 - Threat Table (Post-Appendix A)

> Status: Draft
> Scope: Pre-implementation
> Purpose: Explicit threat-to-mechanism mapping

---

## T.1 Purpose of This Table

This table documents **known threats**, their **impact on core DVEL properties**, and how (or whether) they are mitigated by:

* Core Protocol v0.1
* Appendix A: Non-Consensus Sybil Friction

This is **not** a security guarantee.
It is an explicit statement of failure modes.

---

## T.2 Threat Classification

Threats are classified by:

* **Vector**: Ledger / Identity / Network / Time
* **Severity**: Low / Medium / High / Fatal
* **Mitigation Type**:

  * None
  * Partial (Friction)
  * Structural (Out of Scope)

---

## T.3 Threat Table

| ID  | Threat                          | Vector   | Severity (v0.1 Core) | Mitigation (Appendix A)     | Residual Risk |
| --- | ------------------------------- | -------- | -------------------- | --------------------------- | ------------- |
| T1  | Mass Sybil Identity Creation    | Identity | Fatal                | Identity Maturity (A.5)     | High          |
| T2  | Instant Chain Domination        | Ledger   | Fatal                | Weighted Preference (A.7)   | Medium        |
| T3  | Event Spam Flooding             | Ledger   | Fatal                | Rate Friction (A.6)         | Medium        |
| T4  | Long-Range Reorganization       | Ledger   | Fatal                | None                        | Fatal         |
| T5  | Timestamp Manipulation          | Time     | High                 | Local Bounds Only           | High          |
| T6  | Eclipse Attack (Full Isolation) | Network  | Fatal                | Peer Trust Heuristics (A.8) | Medium-High   |
| T7  | Partial Eclipse / Gossip Bias   | Network  | High                 | Gossip Deprioritization     | Medium        |
| T8  | Identity Grinding Over Time     | Identity | High                 | Temporal Cost               | Medium        |
| T9  | Chain Withholding               | Ledger   | Medium               | None                        | Medium        |
| T10 | Honest Minority Suppression     | Network  | High                 | None                        | High          |

---

## T.4 Detailed Threat Notes

### T1 - Mass Sybil Identity Creation

**Description:**
An attacker generates a large number of identities at negligible cost.

**Effect:**

* Dominates gossip
* Controls apparent majority

**Mitigation:**

* Identity Maturity introduces time-based friction

**Limitation:**

* Attacker can still prepare identities in advance

---

### T2 - Instant Chain Domination

**Description:**
Sybil identities rapidly append events to create the longest chain.

**Mitigation:**

* Chain preference weighting reduces immediate influence

**Note:**

* Validity remains unaffected

---

### T3 - Event Spam Flooding

**Description:**
Large volumes of cheap events overwhelm honest participants.

**Mitigation:**

* Soft rate limiting
* Weight degradation

**Rationale:**

* Prevents linear influence amplification

---

### T4 - Long-Range Reorganization

**Description:**
An attacker rebuilds an alternative history offline and releases it.

**Mitigation:**

* None

**Status:**

* Explicitly unmitigated by design

---

### T5 - Timestamp Manipulation

**Description:**
Events use manipulated timestamps to bypass ordering heuristics.

**Mitigation:**

* Local monotonic tolerance only

**Risk:**

* Time is not a trusted input

---

### T6 - Eclipse Attack (Full Isolation)

**Description:**
A node is surrounded exclusively by malicious peers.

**Mitigation:**

* Peer trust heuristics
* Gossip diversity strategies

**Residual Risk:**

* Still possible under sustained attack

---

### T7 - Partial Eclipse / Gossip Bias

**Description:**
Selective message delivery biases perceived chain state.

**Mitigation:**

* Deprioritization of inconsistent peers

---

### T8 - Identity Grinding Over Time

**Description:**
Attacker pre-ages identities to bypass maturity heuristics.

**Mitigation:**

* Temporal cost increases preparation time

**Note:**

* Does not prevent determined attackers

---

### T9 - Chain Withholding

**Description:**
An attacker withholds a valid chain segment to release strategically.

**Mitigation:**

* None

---

### T10 - Honest Minority Suppression

**Description:**
Honest nodes are outnumbered and overshadowed in gossip.

**Mitigation:**

* None

**Observation:**

* This is a fundamental property of non-economic systems

---

## T.5 Summary

* Appendix A reduces **instantaneous failure modes**
* Structural attacks remain possible
* No threat is hidden or redefined as impossible

DVEL remains:

* Unsafe against determined adversaries
* Honest about its limits
* Suitable as a research and educational primitive

---

> DVEL does not claim security.
> It claims *explicit failure boundaries*.
