// DVEL metrics runner
//
// Purpose:
// - Run a deterministic scenario
// - Collect read-only metrics
// - Print human-readable stdout

#include <cstdio>
#include <vector>

#include "../core/types.hpp"
#include "../core/bus.hpp"
#include "../core/node.hpp"
#include "../core/gossip.hpp"
#include "../core/scenario.hpp"
#include "../core/metrics.hpp"

using namespace dvelsim;

int main() {
    // Choose scenario deterministically (compile-time edit, no CLI)
    Scenario sc = scenario_honest_3nodes();
    // Scenario sc = scenario_sybil_swarm(/*total_nodes=*/8, /*start_tick=*/1, /*end_tick=*/6);
    // Scenario sc = scenario_eclipse_victim(scenario_honest_3nodes(), /*victim=*/2, /*allowlist=*/{0});

    std::printf("DVEL Metrics: scenario=%s nodes=%u\n", sc.name, sc.node_count);

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

    // Peer list
    std::vector<uint32_t> peer_ids;
    for (uint32_t i = 0; i < sc.node_count; i++) peer_ids.push_back(i);

    // Gossip policies
    BroadcastAll gossip_all(/*delay_ticks=*/1);
    AllowlistOnly gossip_victim(sc.victim_allowlist, /*delay_ticks=*/1);

    // Bus
    MessageBus bus(/*default_delay_ticks=*/1);

    // Metrics
    Metrics metrics(sc.node_count);

    // Horizon
    uint64_t max_tick = 0;
    for (const auto& pe : sc.plan) if (pe.tick > max_tick) max_tick = pe.tick;
    max_tick += 3; // drain window

    for (uint64_t tick = 0; tick <= max_tick; tick++) {
        // Produce scheduled events
        for (const auto& pe : sc.plan) {
            if (pe.tick != tick) continue;

            NodeRuntime* n = nodes[pe.node_id];

            const dvel_hash_t prev = n->current_tip_or_zero();
            const uint64_t ts = 10'000 + tick;

            Message msg = n->make_event_message(ts, prev, pe.payload_tag);

            // Local append
            if (n->local_append(msg, tick, /*verbose=*/false)) {
                metrics.on_local_append(n->id());
            }
            // else: local failure is ignored for now (not remote rejection)


            // Gossip out
            GossipPolicy* gp = &gossip_all;
            if (sc.has_eclipse_victim && pe.node_id == sc.victim_id) gp = &gossip_victim;
            gp->broadcast_event(bus, tick, n->id(), msg, peer_ids);
        }

        // Deliver due messages
        bus.deliver(tick, [&](uint32_t to, const Message& msg) {
            if (to < nodes.size()) nodes[to]->inbox_push(msg);
        });

        // Process inbox
        for (NodeRuntime* n : nodes) {
            // We do not get granular accept/reject counts from NodeRuntime yet.
            // We treat "processed" as read-only and only track local appends reliably.
            // Future: expose accept/reject counts via callbacks.
            auto stats = n->process_inbox(tick, /*verbose=*/false);
            metrics.on_remote_accepted(n->id(), stats.accepted);
            metrics.on_rejected(n->id(), stats.rejected_perm);

        }

        // Observe + print per tick
        TickSnapshot snap = metrics.observe_tick(tick, nodes);
        metrics.print_tick(snap, nodes, bus.pending());
        std::puts("---");
    }

    metrics.check_invariants_basic();
    metrics.print_summary();

    // Dump traces for external prover tooling
    for (uint32_t i = 0; i < nodes.size(); i++) {
        std::string path = "trace_metrics_node" + std::to_string(i) + ".json";
        nodes[i]->dump_trace_json(path);
    }

    return 0;
}
