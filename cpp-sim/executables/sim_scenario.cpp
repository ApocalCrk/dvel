// DVEL Scenario Simulation - Protocol Testing
//
// Purpose: Test different attack scenarios (sybil, eclipse)
// Type: Protocol validation (NOT production system)
// Features:
//   - NodeRuntime split: local_append vs remote_receive
//   - GossipPolicy abstraction
//   - Scenario injection: honest / sybil swarm / eclipse
// Note: For production, use gov_ledger.cpp
//       This file is for protocol attack resistance testing.

#include <cstdio>
#include <vector>

#include "types.hpp"
#include "bus.hpp"
#include "node.hpp"
#include "gossip.hpp"
#include "scenario.hpp"

using namespace dvelsim;

int main() {
    // Choose a scenario deterministically (compile-time constant for now)
    // Replace by scenario list iteration later.
    Scenario sc = scenario_honest_3nodes();
    // Scenario sc = scenario_sybil_swarm(/*total_nodes=*/6, /*start_tick=*/1, /*end_tick=*/6);
    // Scenario sc = scenario_eclipse_victim(scenario_honest_3nodes(), /*victim=*/2, /*allowlist=*/{0});

    std::printf("DVEL Scenario: scenario=%s nodes=%u\n", sc.name, sc.node_count);

    // Create nodes with deterministic author tags
    std::vector<NodeRuntime*> nodes;
    nodes.reserve(sc.node_count);

    std::vector<NodeRuntime> owned;
    owned.reserve(sc.node_count);

    for (uint32_t i = 0; i < sc.node_count; i++) {
        uint8_t tag = static_cast<uint8_t>(0xA0 + i);
        owned.emplace_back(i, make_pubkey(tag), make_secret(tag));
    }
    for (uint32_t i = 0; i < sc.node_count; i++) nodes.push_back(&owned[i]);

    // Peer list (fixed order)
    std::vector<uint32_t> peer_ids;
    for (uint32_t i = 0; i < sc.node_count; i++) peer_ids.push_back(i);

    // Gossip policies
    BroadcastAll gossip_all(/*delay_ticks=*/1);
    AllowlistOnly gossip_victim(sc.victim_allowlist, /*delay_ticks=*/1);

    // Bus
    MessageBus bus(/*default_delay_ticks=*/1);

    // Simulation horizon: compute from plan
    uint64_t max_tick = 0;
    for (const auto& pe : sc.plan) if (pe.tick > max_tick) max_tick = pe.tick;
    max_tick += 3; // drain window

    for (uint64_t tick = 0; tick <= max_tick; tick++) {
        // --- produce events scheduled at this tick ---
        for (const auto& pe : sc.plan) {
            if (pe.tick != tick) continue;

            NodeRuntime* n = nodes[pe.node_id];

            const dvel_hash_t prev = n->current_tip_or_zero();
            const uint64_t ts = 10'000 + tick; // deterministic timestamp injection

            Message msg = n->make_event_message(ts, prev, pe.payload_tag);

            // Local append (self-accept)
            n->local_append(msg, tick, /*verbose=*/true);

            // Gossip out
            // If the scenario marks an eclipse victim, apply allowlist policy for that node only.
            GossipPolicy* gp = &gossip_all;
            if (sc.has_eclipse_victim && pe.node_id == sc.victim_id) gp = &gossip_victim;

            gp->broadcast_event(bus, tick, n->id(), msg, peer_ids);
        }

        // --- deliver due messages ---
        bus.deliver(tick, [&](uint32_t to, const Message& msg) {
            if (to < nodes.size()) nodes[to]->inbox_push(msg);
        });

        // --- process inbox for each node in fixed order ---
        for (NodeRuntime* n : nodes) {
            (void)n->process_inbox(tick, /*verbose=*/true);
        }

        // --- observe preferred tips ---
        std::printf("tick=%llu pending_bus=%zu\n", (unsigned long long)tick, bus.pending());
        for (NodeRuntime* n : nodes) {
            dvel_preferred_tip_t pref = n->preferred_tip(tick);
            if (pref.has_value) {
                std::printf("  node[%u] preferred score=%llu ", n->id(), (unsigned long long)pref.score);
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
        std::string path = "trace_scenario_node" + std::to_string(i) + ".json";
        nodes[i]->dump_trace_json(path);
    }
    return 0;
}
