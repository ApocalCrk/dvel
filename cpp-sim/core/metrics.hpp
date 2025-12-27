// Deterministic metrics/invariants: read-only, deterministic, human-readable.

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <cstdio>

#include "types.hpp"

namespace dvelsim {

struct NodeMetrics {
    uint64_t local_appended = 0;
    uint64_t remote_accepted = 0;
    uint64_t rejected = 0;

    uint64_t last_preferred_score = 0;
    dvel_hash_t last_preferred_tip = zero_hash();
    bool has_preferred = false;
};

struct TickSnapshot {
    uint64_t tick = 0;

    // Preferred tips observed at this tick
    std::vector<dvel_hash_t> preferred_tips;
    std::vector<uint64_t> preferred_scores;
    std::vector<bool> preferred_has;

    // Non-consensus indicator: count distinct preferred tips
    uint64_t unique_preferred_tips = 0;
};

class Metrics {
public:
    explicit Metrics(uint32_t node_count)
        : per_node(node_count) {}

    void on_local_append(uint32_t node_id) {
        per_node[node_id].local_appended++;
    }

    void on_remote_accepted(uint32_t node_id, uint64_t n) {
        per_node[node_id].remote_accepted += n;
    }

    void on_rejected(uint32_t node_id, uint64_t n) {
        per_node[node_id].rejected += n;
    }


    // Observe preferred tips after processing a tick.
    template <typename NodePtrVec>
    TickSnapshot observe_tick(uint64_t tick, const NodePtrVec& nodes) {
        TickSnapshot snap;
        snap.tick = tick;
        snap.preferred_tips.resize(nodes.size());
        snap.preferred_scores.resize(nodes.size());
        snap.preferred_has.resize(nodes.size());

        // Collect
        std::vector<dvel_hash_t> tips_present;
        tips_present.reserve(nodes.size());

        for (size_t i = 0; i < nodes.size(); i++) {
            auto* n = nodes[i];
            dvel_preferred_tip_t pref = n->preferred_tip(tick);

            snap.preferred_has[i] = pref.has_value;
            snap.preferred_scores[i] = pref.score;
            snap.preferred_tips[i] = pref.tip;

            per_node[i].has_preferred = pref.has_value;
            per_node[i].last_preferred_score = pref.score;
            per_node[i].last_preferred_tip = pref.tip;

            if (pref.has_value) {
                tips_present.push_back(pref.tip);
            }
        }

        // Compute unique tip count deterministically (O(n^2) fine for small N)
        uint64_t unique = 0;
        for (size_t i = 0; i < tips_present.size(); i++) {
            bool seen = false;
            for (size_t j = 0; j < i; j++) {
                if (std::memcmp(tips_present[i].bytes, tips_present[j].bytes, 32) == 0) {
                    seen = true;
                    break;
                }
            }
            if (!seen) unique++;
        }
        snap.unique_preferred_tips = unique;

        return snap;
    }

    // per-tick print
    template <typename NodePtrVec>
    void print_tick(const TickSnapshot& snap, const NodePtrVec& nodes, size_t pending_bus) const {
        std::printf("tick=%llu pending_bus=%zu unique_preferred_tips=%llu\n",
                    (unsigned long long)snap.tick,
                    pending_bus,
                    (unsigned long long)snap.unique_preferred_tips);

        for (size_t i = 0; i < nodes.size(); i++) {
            if (snap.preferred_has[i]) {
                std::printf("  node[%zu] pref_score=%llu ", i, (unsigned long long)snap.preferred_scores[i]);
                print_hash_prefix("pref_tip:", snap.preferred_tips[i]);
            } else {
                std::printf("  node[%zu] pref: <none>\n", i);
            }
        }
    }

    // End-of-run summary
    void print_summary() const {
        std::puts("=== SUMMARY ===");
        for (size_t i = 0; i < per_node.size(); i++) {
            const auto& m = per_node[i];
            std::printf("node[%zu] local=%llu remote_ok=%llu rejected=%llu\n",
                        i,
                        (unsigned long long)m.local_appended,
                        (unsigned long long)m.remote_accepted,
                        (unsigned long long)m.rejected);
        }
    }

    // Invariant checks (soft assertions via stdout; report-only)
    void check_invariants_basic() const {
        // Basic invariant: counters are monotonic (always true by construction).
        // Placeholder for future invariants.
        std::puts("[inv] basic: OK (monotonic counters)\n");
    }

private:
    std::vector<NodeMetrics> per_node;
};

} // namespace dvelsim
