// DVEL Scheduler Simulation - Protocol Testing
//
// Purpose: Test adversarial message scheduling and delivery timing
// Type: Protocol validation (NOT production system)
//
// Tests:
// - Message delivery under adversarial scheduling
// - Impact on divergence and consensus metrics
// - Timing attack resistance
//
// Note: For production, use gov_ledger.cpp
//       This file is for protocol timing/scheduling testing.

#include <cstdio>
#include <vector>

#include "../core/types.hpp"
#include "../core/bus.hpp"
#include "../core/node.hpp"
#include "../core/gossip.hpp"
#include "../core/scenario.hpp"
#include "../core/metrics.hpp"
#include "../core/scheduler.hpp"

using namespace dvelsim;

int main() {
    // Scenario
    Scenario sc = scenario_honest_3nodes();

    std::printf("DVEL Scheduler Test: scenario=%s nodes=%u\n", sc.name, sc.node_count);

    // Nodes
    std::vector<NodeRuntime*> nodes;
    std::vector<NodeRuntime> owned;
    owned.reserve(sc.node_count);
    nodes.reserve(sc.node_count);

    for (uint32_t i = 0; i < sc.node_count; i++) {
        uint8_t tag = static_cast<uint8_t>(0xA0 + i);
        owned.emplace_back(i, make_pubkey(tag), make_secret(tag));
    }
    for (uint32_t i = 0; i < sc.node_count; i++) nodes.push_back(&owned[i]);

    // Peers
    std::vector<uint32_t> peer_ids;
    for (uint32_t i = 0; i < sc.node_count; i++) peer_ids.push_back(i);

    // Gossip
    BroadcastAll gossip_all(/*delay_ticks=*/1);

    // Bus
    MessageBus bus(/*default_delay_ticks=*/1);

    // Scheduler policy (choose ONE)
    // HonestSchedule policy;
    FixedDelaySchedule policy(/*victim=*/1, /*extra_delay=*/3);
    // StarvationSchedule policy(/*victim=*/2);

    // Metrics
    Metrics metrics(sc.node_count);

    // Horizon
    uint64_t max_tick = 0;
    for (const auto& pe : sc.plan) if (pe.tick > max_tick) max_tick = pe.tick;
    max_tick += 5;

    for (uint64_t tick = 0; tick <= max_tick; tick++) {
        // Produce
        for (const auto& pe : sc.plan) {
            if (pe.tick != tick) continue;

            NodeRuntime* n = nodes[pe.node_id];
            Message msg = n->make_event_message(10'000 + tick, n->current_tip_or_zero(), pe.payload_tag);

            if (n->local_append(msg, tick, false)) metrics.on_local_append(n->id());
            gossip_all.broadcast_event(bus, tick, n->id(), msg, peer_ids);
        }

        // Adversarial delivery
        bus.deliver_with_policy(tick, policy,
            [&](uint32_t to, const Message& msg) {
                if (to < nodes.size()) {
                    nodes[to]->inbox_push(msg);
                }
            }
        );

        // Process inbox
        for (NodeRuntime* n : nodes) {
            auto stats = n->process_inbox(tick, false);
            metrics.on_remote_accepted(n->id(), stats.accepted);
            metrics.on_rejected(n->id(), stats.rejected_perm);
        }

        // Observe
        auto snap = metrics.observe_tick(tick, nodes);
        metrics.print_tick(snap, nodes, bus.pending());
        std::puts("---");
    }

    metrics.print_summary();
    // Dump traces for external prover tooling
    for (uint32_t i = 0; i < nodes.size(); i++) {
        std::string path = "trace_scheduler_node" + std::to_string(i) + ".json";
        nodes[i]->dump_trace_json(path);
    }
    return 0;
}
