// DVEL Eclipse Attack Simulation
//
// Attack: Isolate a victim node by controlling its peer connections
// Goal: Feed victim a fake chain divergent from honest majority
// Success: Victim accepts fake chain and diverges from network consensus
//
// This tests:
// - Network partition resistance
// - Consensus recovery mechanisms
// - Attack detection capabilities

#include <algorithm>
#include <cstdio>
#include <unordered_map>
#include <vector>

#include "../core/bus.hpp"
#include "../core/gossip.hpp"
#include "../core/node.hpp"
#include "../core/types.hpp"
#include "../core/metrics.hpp"

using namespace dvelsim;

// Attack configuration
struct EclipseAttackConfig {
    uint32_t total_nodes;           // Total nodes in network
    uint32_t victim_id;             // Which node to eclipse
    uint32_t num_attackers;         // How many attacker nodes
    uint64_t attack_start_tick;     // When attack begins
    uint64_t attack_end_tick;       // When to stop (test recovery)
    bool verbose;
};

int main(int argc, char** argv) {
    EclipseAttackConfig cfg;
    cfg.total_nodes = 10;
    cfg.victim_id = 5;
    cfg.num_attackers = 3;
    cfg.attack_start_tick = 20;
    cfg.attack_end_tick = 80;
    cfg.verbose = false;

    // Parse args (simple)
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--verbose" || arg == "-v") cfg.verbose = true;
        else if (arg == "--nodes" && i+1 < argc) cfg.total_nodes = std::atoi(argv[++i]);
        else if (arg == "--victim" && i+1 < argc) cfg.victim_id = std::atoi(argv[++i]);
        else if (arg == "--attackers" && i+1 < argc) cfg.num_attackers = std::atoi(argv[++i]);
        else if (arg == "--attack-start" && i+1 < argc) cfg.attack_start_tick = std::atoi(argv[++i]);
        else if (arg == "--attack-end" && i+1 < argc) cfg.attack_end_tick = std::atoi(argv[++i]);
    }

    std::printf("DVEL ECLIPSE ATTACK SIMULATION\n");
    std::printf("Network: %u nodes total\n", cfg.total_nodes);
    std::printf("Victim: Node %u\n", cfg.victim_id);
    std::printf("Attackers: %u malicious nodes\n", cfg.num_attackers);
    std::printf("Attack Window: ticks %llu-%llu\n\n", 
               (unsigned long long)cfg.attack_start_tick,
               (unsigned long long)cfg.attack_end_tick);

    // Setup nodes
    std::vector<NodeRuntime> owned;
    std::vector<NodeRuntime*> nodes;
    owned.reserve(cfg.total_nodes);
    nodes.reserve(cfg.total_nodes);

    for (uint32_t i = 0; i < cfg.total_nodes; ++i) {
        owned.emplace_back(i, make_pubkey(0x10 + i), make_secret(0x10 + i));
    }
    for (uint32_t i = 0; i < cfg.total_nodes; ++i) {
        nodes.push_back(&owned[i]);
    }

    // Identify honest vs attacker nodes
    std::vector<uint32_t> honest_nodes;
    std::vector<uint32_t> attacker_nodes;
    
    for (uint32_t i = 0; i < cfg.total_nodes; ++i) {
        if (i == cfg.victim_id) continue; // victim separate
        if (attacker_nodes.size() < cfg.num_attackers) {
            attacker_nodes.push_back(i);
        } else {
            honest_nodes.push_back(i);
        }
    }

    // Gossip policies
    BroadcastAll honest_gossip(1);
    
    // Victim's allowlist: starts normal, switches to attackers-only during attack
    std::vector<uint32_t> victim_allowlist;
    for (uint32_t i = 0; i < cfg.total_nodes; ++i) {
        if (i != cfg.victim_id) victim_allowlist.push_back(i);
    }
    AllowlistOnly victim_gossip(victim_allowlist, 1);

    MessageBus bus(1);
    Metrics metrics(cfg.total_nodes);

    // Attack metrics
    struct AttackMetrics {
        uint64_t ticks_diverged = 0;
        uint64_t max_consensus_gap = 0;
        bool victim_recovered = false;
        uint64_t recovery_tick = 0;
    } attack_metrics;

    const uint64_t simulation_ticks = cfg.attack_end_tick + 30;

    for (uint64_t tick = 0; tick <= simulation_ticks; ++tick) {
        // Update victim's allowlist based on attack phase
        bool attack_active = (tick >= cfg.attack_start_tick && tick < cfg.attack_end_tick);
        
        if (attack_active) {
            // Eclipse: victim only sees attackers
            victim_allowlist = attacker_nodes;
        } else {
            // Normal: victim sees everyone
            victim_allowlist.clear();
            for (uint32_t i = 0; i < cfg.total_nodes; ++i) {
                if (i != cfg.victim_id) victim_allowlist.push_back(i);
            }
        }
        victim_gossip = AllowlistOnly(victim_allowlist, 1);

        // Each node produces a transaction
        for (uint32_t i = 0; i < cfg.total_nodes; ++i) {
            NodeRuntime* n = nodes[i];
            const dvel_hash_t prev = n->current_tip_or_zero();
            const uint64_t ts = 1000000 + tick * 1000 + i;
            
            // Attackers produce fake/conflicting transactions during attack
            uint8_t payload = attack_active && 
                             std::find(attacker_nodes.begin(), attacker_nodes.end(), i) != attacker_nodes.end()
                             ? (uint8_t)(0xFF - (i % 10))  // Distinct fake payload
                             : (uint8_t)(0x01 + (i % 10)); // Normal payload
            
            Message msg = n->make_event_message(ts, prev, payload);
            
            bool accepted = n->local_append(msg, tick, cfg.verbose);
            if (accepted) {
                metrics.on_local_append(i);
                
                // Gossip with appropriate policy
                if (i == cfg.victim_id) {
                    victim_gossip.broadcast_event(bus, tick, n->id(), msg, victim_allowlist);
                } else if (attack_active && 
                          std::find(attacker_nodes.begin(), attacker_nodes.end(), i) != attacker_nodes.end()) {
                    // Attackers: broadcast to everyone INCLUDING victim
                    std::vector<uint32_t> attacker_targets;
                    attacker_targets.push_back(cfg.victim_id);  // Always target victim
                    for (auto aid : attacker_nodes) {
                        if (aid != i) attacker_targets.push_back(aid);
                    }
                    honest_gossip.broadcast_event(bus, tick, n->id(), msg, attacker_targets);
                } else {
                    // Honest nodes: normal broadcast (but victim won't see during eclipse)
                    std::vector<uint32_t> targets;
                    for (uint32_t j = 0; j < cfg.total_nodes; ++j) {
                        if (j != i) targets.push_back(j);
                    }
                    honest_gossip.broadcast_event(bus, tick, n->id(), msg, targets);
                }
            }
        }

        // Deliver messages
        bus.deliver(tick, [&](uint32_t to, const Message& msg) {
            if (to < nodes.size()) {
                nodes[to]->inbox_push(msg);
            }
        });

        // Process inbox
        for (NodeRuntime* n : nodes) {
            auto stats = n->process_inbox(tick, cfg.verbose);
            metrics.on_remote_accepted(n->id(), stats.accepted);
            metrics.on_rejected(n->id(), stats.rejected_perm);
        }

        // Measure consensus every 10 ticks
        if (tick > 0 && tick % 10 == 0) {
            std::unordered_map<HashKey, std::vector<uint32_t>, HashKeyHasher> tip_groups;
            for (uint32_t i = 0; i < nodes.size(); ++i) {
                dvel_preferred_tip_t pref = nodes[i]->preferred_tip(tick);
                if (pref.has_value) {
                    HashKey k(pref.tip);
                    tip_groups[k].push_back(i);
                }
            }

            // Find victim's group and honest majority group
            std::vector<uint32_t>* victim_group = nullptr;
            std::vector<uint32_t>* honest_majority = nullptr;
            size_t max_honest_size = 0;

            for (auto& pair : tip_groups) {
                // Is victim in this group?
                if (std::find(pair.second.begin(), pair.second.end(), cfg.victim_id) != pair.second.end()) {
                    victim_group = &pair.second;
                }
                // Count honest nodes in this group
                size_t honest_count = 0;
                for (auto nid : pair.second) {
                    if (std::find(honest_nodes.begin(), honest_nodes.end(), nid) != honest_nodes.end()) {
                        honest_count++;
                    }
                }
                if (honest_count > max_honest_size) {
                    max_honest_size = honest_count;
                    honest_majority = &pair.second;
                }
            }

            bool victim_diverged = (victim_group != honest_majority);
            if (attack_active && victim_diverged) {
                attack_metrics.ticks_diverged++;
            }
            if (!attack_active && !victim_diverged && attack_metrics.ticks_diverged > 0 && !attack_metrics.victim_recovered) {
                attack_metrics.victim_recovered = true;
                attack_metrics.recovery_tick = tick;
            }

            uint64_t consensus_gap = tip_groups.size();
            attack_metrics.max_consensus_gap = std::max(attack_metrics.max_consensus_gap, consensus_gap);

            // Status output
            std::string phase = attack_active ? "[ATTACK]" : tick < cfg.attack_start_tick ? "[NORMAL]" : "[RECOVERY]";
            std::printf("tick=%3lu %s tips=%2zu victim=%s honest_majority=%zu/%zu\n",
                       (unsigned long)tick,
                       phase.c_str(),
                       tip_groups.size(),
                       victim_diverged ? "ECLIPSED" : "OK",
                       honest_majority ? honest_majority->size() : 0,
                       honest_nodes.size());
        }
    }

    // Final Analysis
    std::printf("\nECLIPSE ATTACK ANALYSIS\n");

    std::printf("--- Attack Effectiveness ---\n");
    std::printf("Attack Duration: %llu ticks\n", 
               (unsigned long long)(cfg.attack_end_tick - cfg.attack_start_tick));
    std::printf("Ticks Victim Diverged: %llu\n", 
               (unsigned long long)attack_metrics.ticks_diverged);
    
    double divergence_rate = 100.0 * attack_metrics.ticks_diverged / 
                            (cfg.attack_end_tick - cfg.attack_start_tick);
    std::printf("Divergence Rate: %.1f%%\n", divergence_rate);
    
    std::printf("\n--- Recovery ---\n");
    if (attack_metrics.victim_recovered) {
        std::printf("Victim recovered consensus\n");
        std::printf("Recovery Time: %llu ticks after attack ended\n",
                   (unsigned long long)(attack_metrics.recovery_tick - cfg.attack_end_tick));
    } else {
        std::printf("Victim did NOT recover\n");
    }

    std::printf("\n--- Network Health ---\n");
    std::printf("Max Consensus Divergence: %llu different tips\n",
               (unsigned long long)attack_metrics.max_consensus_gap);

    if (divergence_rate > 80 && attack_metrics.victim_recovered) {
        std::printf("RESULT: ATTACK SUCCESSFUL (but recovered)\n");
        std::printf("Eclipse attack isolated victim during attack window\n");
        std::printf("Network recovered after attackers stopped\n");
    } else if (divergence_rate > 80) {
        std::printf("RESULT: ATTACK SUCCESSFUL (no recovery)\n");
        std::printf("Eclipse attack isolated victim\n");
        std::printf("WARNING: Victim did not rejoin consensus\n");
    } else {
        std::printf("RESULT: ATTACK FAILED\n");
        std::printf("System resisted eclipse attack\n");
    }

    return (divergence_rate > 80) ? 0 : 1;
}
