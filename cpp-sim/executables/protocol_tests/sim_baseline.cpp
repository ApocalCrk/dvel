// DVEL Baseline Simulation - Protocol Testing
//
// Purpose: Test basic multi-peer consensus convergence
// Type: Protocol validation (NOT production system)
// Architecture:
//   - cpp-sim/core: Simulation framework (calls Rust FFI)
//   - rust-core: Actual consensus/validation logic
// Note: For production, use gov_ledger.cpp (configurable)
//       This file is for protocol testing only.

#include <cstdio>
#include <vector>

#include "../core/types.hpp"
#include "../core/bus.hpp"
#include "../core/node.hpp"
#include "../core/gossip.hpp"

using namespace dvelsim;

int main() {
    std::puts("DVEL Baseline: multi-peer deterministic simulation");

    // --- Create nodes ---
    NodeRuntime n0(0, make_pubkey(0xA1), make_secret(0xA1));
    NodeRuntime n1(1, make_pubkey(0xB2), make_secret(0xB2));
    NodeRuntime n2(2, make_pubkey(0xC3), make_secret(0xC3));

    std::vector<NodeRuntime*> nodes = {&n0, &n1, &n2};
    std::vector<uint32_t> peer_ids = {0, 1, 2};

    // --- Message bus ---
    MessageBus bus(/*default_delay_ticks=*/1);

    // Deterministic gossip policy (baseline = broadcast all)
    BroadcastAll gossip(/*delay_ticks=*/1);

    // Deterministic event plan:
    // tick 1: n0 creates event
    // tick 3: n1 creates event
    // tick 5: n2 creates event
    // tick 7: n0 creates event
    // tick 9: n1 creates event

    const uint64_t END_TICK = 12;

    for (uint64_t t = 0; t <= END_TICK; t++) {

        auto produce_and_gossip = [&](NodeRuntime& n, uint8_t payload_tag) {
            const dvel_hash_t prev = n.current_tip_or_zero();
            const uint64_t ts = 1000 + t; // deterministic timestamp injection

            Message msg = n.make_event_message(ts, prev, payload_tag);

            // Local append (self-accept)
            n.local_append(msg, t, /*verbose=*/true);

            // Gossip to peers
            gossip.broadcast_event(bus, t, n.id(), msg, peer_ids);
        };

        if (t == 1) produce_and_gossip(n0, 0x10);
        if (t == 3) produce_and_gossip(n1, 0x11);
        if (t == 5) produce_and_gossip(n2, 0x12);
        if (t == 7) produce_and_gossip(n0, 0x13);
        if (t == 9) produce_and_gossip(n1, 0x14);

        // Deliver messages scheduled for this tick
        bus.deliver(t, [&](uint32_t to, const Message& msg) {
            if (to < nodes.size()) {
                nodes[to]->inbox_push(msg);
            }
        });

        // Each node processes inbox in fixed order (deterministic)
        for (NodeRuntime* n : nodes) {
            (void)n->process_inbox(t, /*verbose=*/true);
        }

        // Observe preferred tips
        std::printf("tick=%llu pending_bus=%zu\n",
                    (unsigned long long)t, bus.pending());

        for (NodeRuntime* n : nodes) {
            dvel_preferred_tip_t pref = n->preferred_tip(t);
            if (pref.has_value) {
                std::printf("  node[%u] preferred score=%llu ",
                            n->id(), (unsigned long long)pref.score);
                print_hash_prefix("tip:", pref.tip);
            } else {
                std::printf("  node[%u] preferred: <none>\n", n->id());
            }
        }

        std::puts("---");
    }

    std::puts("done");
    // Dump traces for external prover tooling
    for (uint32_t i = 0; i < nodes.size(); i++) {
        std::string path = "trace_baseline_node" + std::to_string(i) + ".json";
        nodes[i]->dump_trace_json(path);
    }
    return 0;
}
