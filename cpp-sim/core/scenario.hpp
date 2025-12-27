// Deterministic scenario injection: baseline, sybil swarm, eclipse.

#pragma once

#include <cstdint>
#include <vector>

namespace dvelsim {

struct PlannedEvent {
    uint64_t tick;
    uint32_t node_id;
    uint8_t payload_tag;
};

struct Scenario {
    const char* name;

    // Nodes exist with IDs 0..N-1
    uint32_t node_count;

    // Deterministic event plan
    std::vector<PlannedEvent> plan;

    // Optional: override gossip for a specific node (eclipse victim)
    bool has_eclipse_victim = false;
    uint32_t victim_id = 0;
    std::vector<uint32_t> victim_allowlist;

    // Optional: sybil set (used by higher-level metrics)
    std::vector<uint32_t> sybil_nodes;
};

inline Scenario scenario_honest_3nodes() {
    Scenario s;
    s.name = "honest_3nodes";
    s.node_count = 3;
    s.plan = {
        {1, 0, 0x10},
        {3, 1, 0x11},
        {5, 2, 0x12},
        {7, 0, 0x13},
        {9, 1, 0x14},
    };
    return s;
}

// Sybil swarm (deterministic) round-robin:
// - Node 0 (honest) emits outside the swarm window.
// - Exactly one sybil emits per tick in [start_tick..end_tick], rotating over 1..N-1.

inline Scenario scenario_sybil_swarm(uint32_t total_nodes, uint64_t start_tick, uint64_t end_tick) {
    Scenario s;
    s.name = "sybil_swarm";
    s.node_count = total_nodes;

    // Node 0 is honest, nodes 1.. are sybils
    for (uint32_t i = 1; i < total_nodes; i++) s.sybil_nodes.push_back(i);

    // Honest produces outside the sybil window (avoid same-tick conflicts)
    // so other nodes can accept remote timestamps without being equal.
    if (start_tick >= 2) {
        s.plan.push_back({start_tick - 1, 0, 0x40});
    } else {
        // fallback safe tick
        s.plan.push_back({0, 0, 0x40});
    }
    s.plan.push_back({end_tick + 2, 0, 0x41});

    // Sybils produce: exactly one per tick, rotating
    uint8_t tag = 0x50;
    const uint32_t sybil_count = (total_nodes > 1) ? (total_nodes - 1) : 0;

    for (uint64_t t = start_tick; t <= end_tick; t++) {
        if (sybil_count == 0) break;

        // round-robin among 1..(N-1)
        const uint32_t idx = uint32_t((t - start_tick) % sybil_count); // 0..sybil_count-1
        const uint32_t producer = 1 + idx;

        s.plan.push_back({t, producer, tag});
        tag = (uint8_t)(tag + 1);
    }

    return s;
}

// Eclipse: victim only gossips with allowlist peers.
inline Scenario scenario_eclipse_victim(const Scenario& base, uint32_t victim, std::vector<uint32_t> allowlist) {
    Scenario s = base;
    s.name = "eclipse_victim";
    s.has_eclipse_victim = true;
    s.victim_id = victim;
    s.victim_allowlist = std::move(allowlist);
    return s;
}

} // namespace dvelsim
