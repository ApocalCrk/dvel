// sim_sybil.cpp (patched: true equivocation + split-view + merge + shadow quarantine)
//
// IMPORTANT:
// - Does NOT modify NodeRuntime / Rust core.
// - Still uses a shadow overlay (because runner doesn't have rust ledger hashes).
// - But now the "equivocation" is REAL: same author, same prev_hash, two distinct events.
// - Adds a minimal quarantine detector in shadow overlay so you can see weight go to 0.

#include <cstdio>
#include <cstdint>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <cstring>
#include <optional>

#include "../core/types.hpp"
#include "../core/bus.hpp"
#include "../core/node.hpp"
#include "../core/gossip.hpp"
#include "../core/scenario.hpp"
#include "../core/metrics.hpp"
#include "../core/scoring.hpp"

using namespace dvelsim;

// ------------------------------------------------------------
// Hash helpers (dvel_hash_t in types.hpp)
// ------------------------------------------------------------
struct HashEq
{
    bool operator()(const dvel_hash_t &a, const dvel_hash_t &b) const
    {
        return std::memcmp(a.bytes, b.bytes, 32) == 0;
    }
};

struct HashHasher
{
    std::size_t operator()(const dvel_hash_t &h) const
    {
        std::size_t out = 0;
        std::memcpy(&out, h.bytes, sizeof(out));
        return out;
    }
};

// Synthetic, deterministic ID for shadow bookkeeping (NON-CRYPTO)
static dvel_hash_t shadow_id(const dvel_event_t &e)
{
    dvel_hash_t h = zero_hash();

    // very simple stable mixing into first 8 bytes
    uint64_t acc = 0;
    acc ^= e.timestamp;
    acc ^= (uint64_t)e.author.bytes[0] << 8;
    acc ^= (uint64_t)e.payload_hash.bytes[0] << 16;
    acc ^= (uint64_t)e.signature.bytes[0] << 24;

    // also mix prev hash a little (helps distinguish fork siblings)
    acc ^= (uint64_t)e.prev_hash.bytes[0] << 32;
    acc ^= (uint64_t)e.prev_hash.bytes[1] << 40;

    std::memcpy(h.bytes, &acc, sizeof(acc));
    return h;
}

// ------------------------------------------------------------
// ShadowNode: latest-per-author tips + per-event weight at accept time
// + QUARANTINE: if same author produces two distinct children of same prev_hash.
// ------------------------------------------------------------
struct ShadowNode
{
    // shadow_id -> weight
    std::unordered_map<dvel_hash_t, double, HashHasher, HashEq> weight_by_id;

    // shadow_id -> author_tag (for quarantine lookup)
    std::unordered_map<dvel_hash_t, uint8_t, HashHasher, HashEq> author_by_id;

    // author_tag -> last timestamp observed (rate dampening)
    std::unordered_map<uint8_t, uint64_t> last_ts_by_author;

    // author_tag -> current tip id (latest event from that author)
    std::unordered_map<uint8_t, dvel_hash_t> tip_by_author;

    // All current tips (latest-per-author)
    std::unordered_set<dvel_hash_t, HashHasher, HashEq> tips;

    // fork index:
    // prev_hash -> number of accepted children already seen for this parent
    std::unordered_map<dvel_hash_t, uint64_t, HashHasher, HashEq> child_count_by_parent;

    // Quarantine:
    // author_tag -> last prev_hash (to detect two siblings)
    std::unordered_map<uint8_t, dvel_hash_t> last_prev_by_author;
    // author_tag -> last event id (so "same event" doesn't trigger)
    std::unordered_map<uint8_t, dvel_hash_t> last_id_by_author;
    // author_tag -> quarantined until tick (exclusive)
    std::unordered_map<uint8_t, uint64_t> quarantined_until;

    static constexpr uint64_t QUARANTINE_TICKS = 6;

    static double weight_event(const dvel_event_t &e,
                               uint64_t now_tick,
                               uint64_t prev_ts_for_author,
                               uint64_t fork_depth)
    {
        const double base = 1.0;

        // H1 rate dampening
        double rate_factor = 1.0;
        if (prev_ts_for_author != 0)
        {
            uint64_t dt = (e.timestamp > prev_ts_for_author) ? (e.timestamp - prev_ts_for_author) : 0;
            rate_factor = (dt >= RATE_WINDOW) ? 1.0 : (double(dt) / double(RATE_WINDOW));
        }

        // H2 fork depth penalty
        double fork_factor = 1.0 / (1.0 + double(fork_depth));

        // H3 temporal decay (in this sim timestamp == tick)
        uint64_t age = (now_tick > e.timestamp) ? (now_tick - e.timestamp) : 0;
        double decay = 1.0 / (1.0 + double(age) / double(DECAY_WINDOW));

        return base * rate_factor * fork_factor * decay;
    }

    void accept_message(const Message &msg, uint64_t now_tick)
    {
        if (msg.type != MsgType::Event)
            return;

        const dvel_event_t &e = msg.event;
        const uint8_t a = e.author.bytes[0];
        const dvel_hash_t id = shadow_id(e);

        // Quarantine detection: same author, same prev_hash, different event id
        // (this is the simplest single-parent equivocation test)
        {
            auto it_prev = last_prev_by_author.find(a);
            auto it_id = last_id_by_author.find(a);
            if (it_prev != last_prev_by_author.end() && it_id != last_id_by_author.end())
            {
                const bool same_prev = HashEq{}(it_prev->second, e.prev_hash);
                const bool same_id = HashEq{}(it_id->second, id);
                if (same_prev && !same_id)
                {
                    uint64_t until = now_tick + QUARANTINE_TICKS;
                    auto it_q = quarantined_until.find(a);
                    if (it_q == quarantined_until.end() || it_q->second < until)
                    {
                        quarantined_until[a] = until;
                    }
                }
            }
            last_prev_by_author[a] = e.prev_hash;
            last_id_by_author[a] = id;
        }

        // previous timestamp for this author (if any)
        uint64_t prev_ts = 0;
        auto it_ts = last_ts_by_author.find(a);
        if (it_ts != last_ts_by_author.end())
            prev_ts = it_ts->second;

        // fork depth = how many children we've already accepted for this prev_hash
        uint64_t fork_depth = 0;
        {
            auto it = child_count_by_parent.find(e.prev_hash);
            if (it != child_count_by_parent.end())
                fork_depth = it->second;
            child_count_by_parent[e.prev_hash] = fork_depth + 1;
        }

        double w = weight_event(e, now_tick, prev_ts, fork_depth);

        weight_by_id[id] = w;
        author_by_id[id] = a;
        last_ts_by_author[a] = e.timestamp;

        // maintain latest-per-author tip set
        auto it_tip = tip_by_author.find(a);
        if (it_tip != tip_by_author.end())
        {
            tips.erase(it_tip->second);
        }
        tip_by_author[a] = id;
        tips.insert(id);
    }

    WeightedTip weighted_preferred(uint64_t now_tick) const
    {
        WeightedTip best;

        for (const auto &id : tips)
        {
            auto it_w = weight_by_id.find(id);
            auto it_a = author_by_id.find(id);
            if (it_w == weight_by_id.end() || it_a == author_by_id.end())
                continue;

            const uint8_t a = it_a->second;

            auto it_q = quarantined_until.find(a);
            if (it_q != quarantined_until.end() && now_tick < it_q->second)
            {
                // quarantined => behaves like weight 0
                continue;
            }

            const double w = it_w->second;
            if (!best.has_value || w > best.weight)
            {
                best.has_value = true;
                best.tip = id;
                best.weight = w;
            }
        }

        return best;
    }
};

int main()
{
    Scenario sc = scenario_sybil_swarm(/*nodes=*/8, /*start=*/1, /*end=*/6);

    std::printf("DVEL Sybil overlay (latest-per-author tips): scenario=%s nodes=%u\n",
                sc.name, sc.node_count);

    // Real nodes
    std::vector<NodeRuntime> owned;
    std::vector<NodeRuntime *> nodes;
    owned.reserve(sc.node_count);
    nodes.reserve(sc.node_count);

    for (uint32_t i = 0; i < sc.node_count; i++)
    {
        uint8_t tag = static_cast<uint8_t>(0xA0 + i);
        owned.emplace_back(i, make_pubkey(tag), make_secret(tag));
    }
    for (uint32_t i = 0; i < sc.node_count; i++)
    {
        nodes.push_back(&owned[i]);
    }

    // Shadow nodes
    std::vector<ShadowNode> shadow(sc.node_count);

    // Peers
    std::vector<uint32_t> peers;
    for (uint32_t i = 0; i < sc.node_count; i++)
        peers.push_back(i);

    // Gossip & bus
    BroadcastAll gossip(1);
    MessageBus bus(1);

    // Metrics
    Metrics metrics(sc.node_count);

    // Store split messages so we can force a merge later.
    std::optional<Message> split_a;
    std::optional<Message> split_b;

    // Horizon
    uint64_t max_tick = 0;
    for (const auto &pe : sc.plan)
        if (pe.tick > max_tick)
            max_tick = pe.tick;
    max_tick += 4;

    bool quarantine_ok = true;

    const bool verbose = true;

    for (uint64_t tick = 0; tick <= max_tick; tick++)
    {

        // Produce scheduled events, but SKIP node[3] at tick 3 (we inject equivocation there).
        for (const auto &pe : sc.plan)
        {
            if (pe.tick != tick)
                continue;
            if (tick == 3 && pe.node_id == 3)
                continue;

            NodeRuntime *n = nodes[pe.node_id];
            Message msg = n->make_event_message(
                tick,
                n->current_tip_or_zero(),
                pe.payload_tag);

            if (n->local_append(msg, tick, verbose))
            {
                metrics.on_local_append(n->id());
            }
            else
            {
                metrics.on_rejected(n->id(), 1);
            }

            shadow[n->id()].accept_message(msg, tick);
            gossip.broadcast_event(bus, tick, n->id(), msg, peers);
        }

        // Split-view equivocation injection at tick 3:
        // - Same author (node[3]) produces two DIFFERENT events
        // - Both share the SAME parent hash (captured before any append)
        // - A goes to {0,1,2,3}; B goes to {4,5,6,7}
        if (tick == 3)
        {
            NodeRuntime *n = nodes[3];

            // Capture SAME parent for BOTH events (this is the key fix).
            dvel_hash_t parent = zero_hash();

            Message msg_a = n->make_event_message(tick, parent, 0xAA);
            Message msg_b = n->make_event_message(tick, parent, 0xBB);

            // Force them to be different in fields used by Ledger::hash_event
            msg_b.event.timestamp = tick + 12345;

            msg_a.event.payload_hash.bytes[0] ^= 0xAA;
            msg_b.event.payload_hash.bytes[0] ^= 0xBB;
            msg_a.event.payload_hash.bytes[1] ^= 0x11;
            msg_b.event.payload_hash.bytes[1] ^= 0x22;

            // Append both locally (fork-legal)
            if (n->local_append(msg_a, tick, verbose))
                metrics.on_local_append(n->id());
            else
                metrics.on_rejected(n->id(), 1);

            if (n->local_append(msg_b, tick, verbose))
                metrics.on_local_append(n->id());
            else
                metrics.on_rejected(n->id(), 1);

            // Shadow observes local too
            shadow[n->id()].accept_message(msg_a, tick);
            shadow[n->id()].accept_message(msg_b, tick);

            // Broadcast split
            std::vector<uint32_t> peers_a = {0, 1, 2, 3};
            std::vector<uint32_t> peers_b = {4, 5, 6, 7};
            gossip.broadcast_event(bus, tick, n->id(), msg_a, peers_a);
            gossip.broadcast_event(bus, tick, n->id(), msg_b, peers_b);

            // Save for merge phase
            split_a = msg_a;
            split_b = msg_b;
        }

        // Merge phase at tick 4: rebroadcast BOTH to ALL nodes
        if (tick == 4 && split_a.has_value() && split_b.has_value())
        {
            gossip.broadcast_event(bus, tick, /*from=*/3, *split_a, peers);
            gossip.broadcast_event(bus, tick, /*from=*/3, *split_b, peers);
        }

        // Deliver messages (shadow observes on delivery)
        bus.deliver(tick, [&](uint32_t to, const Message &msg)
                    {
            if (to < nodes.size()) {
                nodes[to]->inbox_push(msg);
                shadow[to].accept_message(msg, tick);
            } });

        // Process inbox (real)
        for (NodeRuntime *n : nodes)
        {
            auto stats = n->process_inbox(tick, verbose);
            metrics.on_remote_accepted(n->id(), stats.accepted);
            metrics.on_rejected(n->id(), stats.rejected_perm);
        }

        // Observe weighted tips
        std::unordered_set<dvel_hash_t, HashHasher, HashEq> uniq;
        std::vector<WeightedTip> wts(sc.node_count);
        std::vector<dvel_preferred_tip_t> prefs(sc.node_count);

        for (uint32_t i = 0; i < sc.node_count; i++)
        {
            wts[i] = shadow[i].weighted_preferred(tick);
            if (wts[i].has_value)
                uniq.insert(wts[i].tip);
            prefs[i] = nodes[i]->preferred_tip(tick);
        }

        std::printf("tick=%llu pending_bus=%zu unique_weighted_tips=%zu\n",
                    (unsigned long long)tick,
                    bus.pending(),
                    uniq.size());

        dvel_hash_t root{};
        if (nodes[0]->merkle_root(root))
        {
            print_hash_prefix("  merkle_root:", root);
        }

        for (uint32_t i = 0; i < sc.node_count; i++)
        {
            if (!wts[i].has_value)
            {
                std::printf("  node[%u] wpref: <none>\n", i);
            }
            else
            {
                double sybil_w = nodes[i]->author_weight_sybil_fp(tick, nodes[3]->author()) / 1000.0;
                if (tick > 4 && sybil_w > 0.0)
                {
                    quarantine_ok = false;
                }
                std::printf("  node[%u] wpref_weight=%.3f sybil_w(author3)=%.3f ", i, wts[i].weight, sybil_w);
                print_hash_prefix("tip:", wts[i].tip);
            }
            if (prefs[i].has_value)
            {
                std::printf("     sybil_pref score=%llu ", (unsigned long long)prefs[i].score);
                print_hash_prefix("tip:", prefs[i].tip);
            }
            else
            {
                std::printf("     sybil_pref: <none>\n");
            }
        }

        std::puts("---");
    }

    if (!quarantine_ok)
    {
        std::fprintf(stderr, "ERROR: sybil quarantine failed to zero weight after equivocation\n");
        return 1;
    }

    metrics.print_summary();

    // Dump per-node trace rows to JSON for external proof tooling
    for (uint32_t i = 0; i < sc.node_count; i++)
    {
        std::string path = "trace_sybil_node" + std::to_string(i) + ".json";
        nodes[i]->dump_trace_json(path);
    }
    return 0;
}
