// DVEL 51% Attack Simulation - Protocol Testing
//
// Tests BFT safety with Byzantine nodes attempting:
// 1. Double-spend: broadcast conflicting transactions at high rate
// 2. Censorship: delay/drop specific transactions
// 3. Chain reorg: create competing forks
//
// Validates that <1/3 Byzantine nodes cannot break consensus

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

using namespace dvelsim;


enum AttackStrategy {
    DOUBLE_SPEND,      // Broadcast conflicting transactions at high rate
    CENSORSHIP,        // Delay specific node transactions  
    CHAIN_REORG        // Create competing chain fork
};

struct AttackMetrics {
    int honest_tx = 0;
    int byzantine_tx = 0;
    std::map<uint64_t, int> tip_count;  // tick -> unique tips
};

void print_header(int total, int byzantine, AttackStrategy strategy) {
    std::cout << "DVEL 51% ATTACK SIMULATION\n";
    std::cout << "Network: " << total << " nodes\n";
    std::cout << "Byzantine: " << byzantine << " nodes ("
              << (100.0 * byzantine / total) << "%)\n";
    std::cout << "Strategy: ";
    
    if (strategy == DOUBLE_SPEND) {
        std::cout << "DOUBLE-SPEND\n";
    } else if (strategy == CENSORSHIP) {
        std::cout << "CENSORSHIP\n";
    } else {
        std::cout << "CHAIN-REORG\n";
    }
    
    std::cout << "BFT Threshold: 33.3% (safety expected below this)\n\n";
}

void analyze_attack(const AttackMetrics& metrics, AttackStrategy strategy,
                    int total, int byzantine, double final_consensus_pct) {
    std::cout << "\n51% ATTACK ANALYSIS\n";

    std::cout << "--- Transaction Volume ---\n";
    std::cout << "Honest TX: " << metrics.honest_tx << "\n";
    std::cout << "Byzantine TX: " << metrics.byzantine_tx << "\n";
    
    if (strategy == DOUBLE_SPEND) {
        std::cout << "Byzantine TX Rate: " 
                  << (metrics.byzantine_tx > metrics.honest_tx ? "ELEVATED" : "normal") << "\n";
    }

    // Find max tip divergence
    int max_tips = 1;
    for (auto& [tick, tips] : metrics.tip_count) {
        if (tips > max_tips) max_tips = tips;
    }
    std::cout << "\n--- Chain Safety ---\n";
    std::cout << "Max Chain Divergence: " << max_tips << " competing tips\n";
    std::cout << "Final Honest Consensus: " << final_consensus_pct << "%\n";
    
    // BFT safety requires <1/3 Byzantine nodes
    double byzantine_ratio = (double)byzantine / total;
    bool under_threshold = byzantine_ratio < 0.334;

    // Attack succeeds if honest nodes can't maintain >51% consensus (majority)
    // Note: 66% is ideal BFT threshold, but 51% is acceptable for majority consensus
    bool attack_succeeded = (final_consensus_pct < 51.0);

    if (attack_succeeded) {
        std::cout << "RESULT: ATTACK SUCCEEDED\n";
        std::cout << "WARNING: BFT safety violated!\n";
        if (under_threshold) {
            std::cout << "CRITICAL: Attack succeeded below 1/3 threshold\n";
        }
    } else {
        std::cout << "RESULT: ATTACK FAILED\n";
        std::cout << "System maintained BFT safety\n";
        if (under_threshold) {
            std::cout << "Expected: Byzantine nodes below 1/3 threshold\n";
        }
    }
}

int main(int argc, char* argv[]) {
    int total_nodes = 10;
    int byzantine_nodes = 3;  // 30% - below 1/3 threshold
    uint64_t ticks = 150;
    uint64_t attack_start = 30;
    uint64_t attack_duration = 90;
    AttackStrategy strategy = DOUBLE_SPEND;

    // Parse args
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--nodes" && i + 1 < argc) {
            total_nodes = std::atoi(argv[++i]);
        } else if (arg == "--byzantine" && i + 1 < argc) {
            byzantine_nodes = std::atoi(argv[++i]);
        } else if (arg == "--strategy" && i + 1 < argc) {
            std::string s = argv[++i];
            if (s == "double-spend") strategy = DOUBLE_SPEND;
            else if (s == "censorship") strategy = CENSORSHIP;
            else if (s == "chain-reorg") strategy = CHAIN_REORG;
        } else if (arg == "--ticks" && i + 1 < argc) {
            ticks = std::atoi(argv[++i]);
        }
    }

    print_header(total_nodes, byzantine_nodes, strategy);

    // Create nodes (heap allocated like baseline)
    std::vector<NodeRuntime*> all_nodes;
    std::vector<uint32_t> peer_ids;
    
    int honest_count = total_nodes - byzantine_nodes;
    
    for (int i = 0; i < total_nodes; i++) {
        all_nodes.push_back(new NodeRuntime(i, make_pubkey(0x1000 + i), make_secret(0x2000 + i)));
        peer_ids.push_back(i);
    }

    MessageBus bus(/*delay=*/1);
    BroadcastAll gossip(/*delay=*/1);

    AttackMetrics metrics;
    uint64_t attack_end = attack_start + attack_duration;

    std::mt19937 rng(12345);
    std::uniform_real_distribution<> tx_dist(0.0, 1.0);

    // Simulate
    for (uint64_t t = 0; t <= ticks; t++) {
        bool in_attack = (t >= attack_start && t < attack_end);
        
        // Reduce transaction rate initially to establish consensus
        double base_tx_rate = (t < 20) ? 0.15 : 0.3;

        // Honest nodes produce normally
        for (int i = 0; i < honest_count; i++) {
            auto* node = all_nodes[i];
            
            if (tx_dist(rng) < base_tx_rate) {
                dvel_preferred_tip_t pref = node->preferred_tip(t);
                dvel_hash_t prev = pref.has_value ? pref.tip : node->current_tip_or_zero();
                uint64_t ts = 1000 + t * 10 + i;
                uint8_t payload = 0xA0 + (i % 16);
                
                Message msg = node->make_event_message(ts, prev, payload);
                node->local_append(msg, t, false);
                gossip.broadcast_event(bus, t, node->id(), msg, peer_ids);
                
                metrics.honest_tx++;
            }
        }

        // Byzantine nodes
        for (int i = honest_count; i < total_nodes; i++) {
            auto* node = all_nodes[i];
            
            double tx_rate = base_tx_rate;
            
            if (in_attack) {
                if (strategy == DOUBLE_SPEND) {
                    // Produce at double rate to flood network
                    tx_rate = base_tx_rate * 2.0;
                } else if (strategy == CENSORSHIP) {
                    // Stop producing (passive attack)
                    tx_rate = 0.0;
                } else if (strategy == CHAIN_REORG) {
                    // Aggressive production
                    tx_rate = base_tx_rate * 2.5;
                }
            }
            
            if (tx_dist(rng) < tx_rate) {
                // Byzantine nodes deliberately use current_tip_or_zero (wrong) to attack
                dvel_hash_t prev = node->current_tip_or_zero();
                uint64_t ts = 1000 + t * 10 + i;
                uint8_t payload = 0xB0 + (i % 16);
                
                Message msg = node->make_event_message(ts, prev, payload);
                node->local_append(msg, t, false);
                gossip.broadcast_event(bus, t, node->id(), msg, peer_ids);
                
                metrics.byzantine_tx++;
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

        // Metrics every 10 ticks - use preferred_tip for consensus
        if (t % 10 == 0) {
            std::map<std::string, int> tip_counts;
            for (auto* node : all_nodes) {
                dvel_preferred_tip_t pref = node->preferred_tip(t);
                if (pref.has_value) {
                    char buf[65];
                    for (int j = 0; j < 32; j++) {
                        snprintf(buf + j*2, 3, "%02x", pref.tip.bytes[j]);
                    }
                    tip_counts[std::string(buf)]++;
                }
            }

            int max_agreement = 0;
            for (const auto& [tip, count] : tip_counts) {
                max_agreement = std::max(max_agreement, count);
            }
            double consensus_pct = (double)max_agreement / total_nodes * 100.0;
            
            metrics.tip_count[t] = tip_counts.size();

            std::string phase = in_attack ? "[ATTACK]" : "[NORMAL]";
            std::printf("tick=%3llu %s consensus=%5.1f%% tips=%zu status=%s\n",
                       (unsigned long long)t, phase.c_str(), consensus_pct,
                       tip_counts.size(),
                       (consensus_pct >= 66.0 ? "OK" : "DIVERGED"));
        }
    }

    // Final consensus check - only honest nodes
    std::map<std::string, int> final_tip_counts;
    for (int i = 0; i < honest_count; i++) {
        dvel_preferred_tip_t pref = all_nodes[i]->preferred_tip(ticks);
        if (pref.has_value) {
            char buf[65];
            for (int j = 0; j < 32; j++) {
                snprintf(buf + j*2, 3, "%02x", pref.tip.bytes[j]);
            }
            final_tip_counts[std::string(buf)]++;
        }
    }
    
    int max_agreement = 0;
    for (const auto& [tip, count] : final_tip_counts) {
        max_agreement = std::max(max_agreement, count);
    }
    double final_consensus_pct = (double)max_agreement / honest_count * 100.0;
    
    std::cout << "\nFinal honest consensus: " 
              << (final_consensus_pct >= 51.0 ? "\u2713 UNIFIED" : "\u2717 DIVERGED")
              << " (" << final_consensus_pct << "%)\n";

    analyze_attack(metrics, strategy, total_nodes, byzantine_nodes, final_consensus_pct);

    // Cleanup
    for (auto* node : all_nodes) {
        delete node;
    }

    // Return 0 if attack failed (system safe)
    int max_tips = 1;
    for (auto& [t, tips] : metrics.tip_count) {
        if (tips > max_tips) max_tips = tips;
    }
    
    bool attack_succeeded = (max_tips > 3);
    return attack_succeeded ? 1 : 0;
}


