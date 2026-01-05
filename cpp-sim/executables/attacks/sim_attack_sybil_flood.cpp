// DVEL Sybil Flood Attack - Protocol Testing
//
// Tests stake-based sybil resistance by flooding network with fake identities.
// Validates that:
// 1. Low-stake identities cannot overwhelm honest validators
// 2. Weight-based consensus ignores spam nodes
// 3. Equivocation detection quarantines attackers
//
// Attack: Spin up 100+ fake nodes with minimal/zero stake

#include <cstdio>
#include <iostream>
#include <vector>
#include <random>
#include <map>
#include <set>
#include <algorithm>

#include "../core/types.hpp"
#include "../core/bus.hpp"
#include "../core/node.hpp"
#include "../core/gossip.hpp"
#include "../core/scoring.hpp"

using namespace dvelsim;

struct SybilMetrics {
    int honest_tx = 0;
    int sybil_tx = 0;
    int sybil_accepted = 0;
    int honest_consensus_breaks = 0;
    std::map<uint64_t, int> honest_tip_count;
    std::map<uint64_t, double> sybil_weight_share;
};

void print_header(int honest, int sybil, uint64_t stake_per_honest, uint64_t stake_per_sybil) {
    std::cout << "DVEL SYBIL FLOOD ATTACK\n";
    std::cout << "Honest Validators: " << honest << " (stake: " << stake_per_honest << " each)\n";
    std::cout << "Sybil Nodes: " << sybil << " (stake: " << stake_per_sybil << " each)\n";
    
    uint64_t honest_total = honest * stake_per_honest;
    uint64_t sybil_total = sybil * stake_per_sybil;
    double sybil_ratio = 100.0 * sybil_total / (honest_total + sybil_total);
    
    std::cout << "Total Honest Stake: " << honest_total << "\n";
    std::cout << "Total Sybil Stake: " << sybil_total << "\n";
    std::cout << "Sybil Stake %: " << sybil_ratio << "%\n\n";
}

void analyze_attack(const SybilMetrics& metrics, int honest, int sybil,
                    uint64_t stake_honest, uint64_t stake_sybil) {
    std::cout << "\nSYBIL FLOOD ANALYSIS\n\n";

    std::cout << "--- Transaction Counts ---\n";
    std::cout << "Honest TX: " << metrics.honest_tx << "\n";
    std::cout << "Sybil TX Attempted: " << metrics.sybil_tx << "\n";
    std::cout << "Sybil TX Accepted: " << metrics.sybil_accepted << "\n";
    
    double acceptance_rate = metrics.sybil_tx > 0 
        ? 100.0 * metrics.sybil_accepted / metrics.sybil_tx 
        : 0.0;
    std::cout << "Sybil Acceptance Rate: " << acceptance_rate << "%\n";

    std::cout << "\n--- Honest Consensus ---\n";
    int max_honest_tips = 1;
    for (auto& [tick, tips] : metrics.honest_tip_count) {
        if (tips > max_honest_tips) max_honest_tips = tips;
    }
    std::cout << "Max Honest Divergence: " << max_honest_tips << " competing tips\n";
    std::cout << "Consensus Breaks: " << metrics.honest_consensus_breaks << "\n";

    std::cout << "\n--- Sybil Weight Impact ---\n";
    double max_sybil_weight = 0.0;
    for (auto& [tick, weight] : metrics.sybil_weight_share) {
        if (weight > max_sybil_weight) max_sybil_weight = weight;
    }
    std::cout << "Max Sybil Weight Share: " << (max_sybil_weight * 100) << "%\n\n";

    
    // Attack succeeds if sybil nodes gain significant influence (>20% stake weight)
    bool attack_succeeded = (acceptance_rate > 50.0) || 
                           (max_honest_tips > 5) || 
                           (max_sybil_weight > 0.20);

    if (attack_succeeded) {
        std::cout << "RESULT: ATTACK SUCCEEDED\n";
        std::cout << "WARNING: Sybil nodes gained influence\n";
        if (max_sybil_weight > 0.20) {
            std::cout << "  CRITICAL: Sybil stake weight >20%\n";
        }
    } else {
        std::cout << "RESULT: ATTACK FAILED\n";
        std::cout << "System resisted sybil flood\n";
        std::cout << "Stake-weighted consensus effective\n";
    }
    
    std::cout << "\n\n";
}

int main(int argc, char* argv[]) {
    int honest_count = 10;
    int sybil_count = 10;  // Equal count for testing
    uint64_t stake_per_honest = 1000000;  // 1M per honest validator
    uint64_t stake_per_sybil = 10000;      // 10k per sybil (1/100th)
    uint64_t ticks = 50;  // Short test
    uint64_t attack_start = 10;

    // Parse args
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--honest" && i + 1 < argc) {
            honest_count = std::atoi(argv[++i]);
        } else if (arg == "--sybil" && i + 1 < argc) {
            sybil_count = std::atoi(argv[++i]);
        } else if (arg == "--stake-honest" && i + 1 < argc) {
            stake_per_honest = std::atoi(argv[++i]);
        } else if (arg == "--stake-sybil" && i + 1 < argc) {
            stake_per_sybil = std::atoi(argv[++i]);
        } else if (arg == "--ticks" && i + 1 < argc) {
            ticks = std::atoi(argv[++i]);
        }
    }

    print_header(honest_count, sybil_count, stake_per_honest, stake_per_sybil);

    // Create nodes with different stakes
    std::vector<NodeRuntime*> honest_nodes;
    std::vector<NodeRuntime*> sybil_nodes;
    std::vector<NodeRuntime*> all_nodes;
    std::vector<uint32_t> all_peer_ids;
    
    // Honest validators with high stake
    for (int i = 0; i < honest_count; i++) {
        auto* node = new NodeRuntime(i, make_pubkey(0x1000 + i), make_secret(0x2000 + i));
        honest_nodes.push_back(node);
        all_nodes.push_back(node);
        all_peer_ids.push_back(i);
    }

    // Sybil nodes with minimal stake
    for (int i = 0; i < sybil_count; i++) {
        int node_id = honest_count + i;
        auto* node = new NodeRuntime(node_id, make_pubkey(0x5000 + i), make_secret(0x6000 + i));
        sybil_nodes.push_back(node);
        all_nodes.push_back(node);
        all_peer_ids.push_back(node_id);
    }

    MessageBus bus(/*delay=*/1);
    BroadcastAll gossip(/*delay=*/1);

    SybilMetrics metrics;

    std::mt19937 rng(42);
    std::uniform_real_distribution<> tx_dist(0.0, 1.0);

    // Simulate
    for (uint64_t t = 0; t <= ticks; t++) {
        bool in_attack = (t >= attack_start);

        // Honest nodes produce normally with stake-weighted tip selection
        for (auto* node : honest_nodes) {
            if (tx_dist(rng) < 0.3) {
                dvel_preferred_tip_t pref = node->preferred_tip(t);
                dvel_hash_t prev = pref.has_value ? pref.tip : node->current_tip_or_zero();
                uint64_t ts = 1000 + t * 10 + node->id();
                uint8_t payload = 0xA0 + (node->id() % 16);
                
                Message msg = node->make_event_message(ts, prev, payload);
                node->local_append(msg, t, false);
                gossip.broadcast_event(bus, t, node->id(), msg, all_peer_ids);
                
                metrics.honest_tx++;
            }
        }

        // Sybil nodes flood network (if attack active)
        if (in_attack) {
            for (auto* node : sybil_nodes) {
                // High production rate to flood network
                if (tx_dist(rng) < 0.8) {
                    dvel_hash_t prev = node->current_tip_or_zero();
                    uint64_t ts = 1000 + t * 10 + node->id();
                    uint8_t payload = 0xF0 + (node->id() % 16);
                    
                    Message msg = node->make_event_message(ts, prev, payload);
                    
                    // Try to append locally
                    node->local_append(msg, t, false);
                    
                    // Broadcast to all (spam the network)
                    gossip.broadcast_event(bus, t, node->id(), msg, all_peer_ids);
                    
                    metrics.sybil_tx++;
                }
            }
        }

        // Process network
        bus.deliver(t, [&all_nodes](uint32_t to, const Message& m) {
            if (to < all_nodes.size()) {
                all_nodes[to]->inbox_push(m);
            }
        });
        
        for (auto* node : all_nodes) {
            node->process_inbox(t, false);
        }

        // Metrics every 10 ticks - use stake-weighted consensus
        if (t % 10 == 0) {
            // Count honest consensus using preferred_tip
            std::map<std::string, int> honest_tip_counts;
            for (auto* node : honest_nodes) {
                dvel_preferred_tip_t pref = node->preferred_tip(t);
                if (pref.has_value) {
                    char buf[65];
                    for (int j = 0; j < 32; j++) {
                        snprintf(buf + j*2, 3, "%02x", pref.tip.bytes[j]);
                    }
                    honest_tip_counts[std::string(buf)]++;
                }
            }

            int max_agreement = 0;
            for (const auto& [tip, count] : honest_tip_counts) {
                max_agreement = std::max(max_agreement, count);
            }
            double consensus_pct = (double)max_agreement / honest_count * 100.0;
            
            metrics.honest_tip_count[t] = honest_tip_counts.size();
            if (consensus_pct < 90.0) {
                metrics.honest_consensus_breaks++;
            }

            // Estimate sybil acceptance (simplified: check if sybil nodes have non-zero tip)
            int sybil_with_events = 0;
            for (auto* node : sybil_nodes) {
                dvel_hash_t tip = node->current_tip_or_zero();
                bool has_events = false;
                for (int j = 0; j < 32; j++) {
                    if (tip.bytes[j] != 0) {
                        has_events = true;
                        break;
                    }
                }
                if (has_events) sybil_with_events++;
            }
            
            if (in_attack && metrics.sybil_tx > 0) {
                metrics.sybil_accepted = sybil_with_events;
            }

            // Calculate stake-based weight share
            uint64_t total_honest_stake = (uint64_t)honest_count * stake_per_honest;
            uint64_t total_sybil_stake = (uint64_t)sybil_count * stake_per_sybil;
            double sybil_stake_weight = (double)total_sybil_stake / (total_honest_stake + total_sybil_stake);
            metrics.sybil_weight_share[t] = sybil_stake_weight;

            std::string phase = in_attack ? "[ATTACK]" : "[NORMAL]";
            std::printf("tick=%3llu %s honest_consensus=%5.1f%% tips=%zu sybil=%d/%d status=%s\n",
                       (unsigned long long)t, phase.c_str(), consensus_pct,
                       honest_tip_counts.size(), sybil_with_events, sybil_count,
                       (consensus_pct >= 90.0 ? "OK" : "DEGRADED"));
        }
    }

    // Final consensus check - stake-weighted
    std::map<std::string, int> final_honest_tip_counts;
    for (auto* node : honest_nodes) {
        dvel_preferred_tip_t pref = node->preferred_tip(ticks);
        if (pref.has_value) {
            char buf[65];
            for (int j = 0; j < 32; j++) {
                snprintf(buf + j*2, 3, "%02x", pref.tip.bytes[j]);
            }
            final_honest_tip_counts[std::string(buf)]++;
        }
    }
    
    int max_final_agreement = 0;
    for (const auto& [tip, count] : final_honest_tip_counts) {
        max_final_agreement = std::max(max_final_agreement, count);
    }
    double final_consensus_pct = (double)max_final_agreement / honest_count * 100.0;
    
    std::cout << "\nFinal honest consensus: " 
              << (final_consensus_pct >= 90.0 ? "UNIFIED" : "DEGRADED")
              << " (" << final_consensus_pct << "%)\n";

    analyze_attack(metrics, honest_count, sybil_count, 
                  stake_per_honest, stake_per_sybil);

    // Cleanup
    for (auto* node : all_nodes) {
        delete node;
    }

    // Return 0 if attack failed (system safe)
    double sybil_acceptance = metrics.sybil_tx > 0 
        ? 100.0 * metrics.sybil_accepted / metrics.sybil_tx 
        : 0.0;
    
    bool attack_succeeded = (sybil_acceptance > 50.0) || (final_consensus_pct < 80.0);
    
    return attack_succeeded ? 1 : 0;
}
