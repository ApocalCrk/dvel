#pragma once

#include <cstdint>
#include <cstdio>
#include <deque>
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <sstream>
#include <string>
#include <vector>

#include "types.hpp"
#include "bus.hpp"

namespace dvelsim
{

    // Hash key wrapper for unordered_map (32-byte key)
    struct HashKey
    {
        uint8_t b[32];

        HashKey()
        {
            for (int i = 0; i < 32; ++i)
                b[i] = 0;
        }

        // pending_by_parent (prev_hash)
        explicit HashKey(const dvel_hash_t &h)
        {
            for (int i = 0; i < 32; ++i)
                b[i] = h.bytes[i];
        }

        // validation ctx (author)
        explicit HashKey(const dvel_pubkey_t &pk)
        {
            for (int i = 0; i < 32; ++i)
                b[i] = pk.bytes[i];
        }

        bool operator==(const HashKey &o) const
        {
            for (int i = 0; i < 32; ++i)
                if (b[i] != o.b[i])
                    return false;
            return true;
        }
    };

    struct HashKeyHasher
    {
        std::size_t operator()(const HashKey &k) const noexcept
        {
            // Deterministic FNV-1a over 32 bytes
            std::size_t h = static_cast<std::size_t>(1469598103934665603ull);
            for (int i = 0; i < 32; ++i)
            {
                h ^= static_cast<std::size_t>(k.b[i]);
                h *= static_cast<std::size_t>(1099511628211ull);
            }
            return h;
        }
    };

    // Process stats (consumed by sim_metrics/scheduler)
    struct ProcessStats
    {
        uint32_t accepted{0};
        uint32_t rejected_perm{0};
        uint32_t pending_added{0};
        uint32_t pending_drained{0};
        uint32_t pending_dropped{0};

        bool any() const
        {
            return accepted || rejected_perm || pending_added || pending_drained || pending_dropped;
        }
    };

    struct NodeRuntime
    {
        uint32_t node_id_;
        dvel_pubkey_t author_;
        dvel_hash_t secret_;
        dvel_ledger_t *ledger_;
        dvel_sybil_overlay_t *overlay_;
        dvel_trace_recorder_t *trace_recorder_;
        // Per-author validation context to avoid out-of-order rejects
        std::unordered_map<HashKey, dvel_validation_ctx_t, HashKeyHasher> vctx_by_author_{};
        // Dedup cache: seen event hashes (bounded)
        std::unordered_set<HashKey, HashKeyHasher> seen_hashes_;
        static constexpr std::size_t MAX_SEEN = 8192;

        dvel_validation_ctx_t &vctx_for_(const dvel_pubkey_t &author)
        {
            HashKey k(author); // reuse HashKey storage (32 bytes)
            auto it = vctx_by_author_.find(k);
            if (it == vctx_by_author_.end())
            {
                dvel_validation_ctx_t ctx{};
                dvel_validation_ctx_init(&ctx);
                auto [it2, _] = vctx_by_author_.emplace(k, ctx);
                return it2->second;
            }
            return it->second;
        }

        bool merkle_root(dvel_hash_t &out) const
        {
            dvel_merkle_root_t res{};
            if (dvel_ledger_merkle_root(ledger_, &res) && res.has_value)
            {
                out = res.root;
                return true;
            }
            return false;
        }

        // inbox_ defined below

        std::deque<Message> inbox_;

        // Pending pool: parent_hash -> queued children
        std::unordered_map<HashKey, std::deque<Message>, HashKeyHasher> pending_by_parent_;
        std::size_t pending_total_{0};

        static constexpr std::size_t MAX_PENDING_TOTAL = 16384;
        static constexpr int MAX_DRAIN_STEPS = 16384; // safety bound per append

        NodeRuntime(uint32_t id, dvel_pubkey_t author, dvel_hash_t secret)
            : node_id_(id), author_(author), secret_(secret)
        {
            static bool config_set = false;
            if (!config_set)
            {
                // Large backward skew to suppress timestamp rejects under adversarial delivery order.
                dvel_set_max_backward_skew(1'000'000);
                config_set = true;
            }
            ledger_ = dvel_ledger_new();
            overlay_ = dvel_sybil_overlay_new();
            trace_recorder_ = dvel_trace_recorder_new();
            dvel_sybil_overlay_attach_trace_recorder(overlay_, trace_recorder_);
            dvel_sybil_config_t cfg{};
            cfg.warmup_ticks = 4;
            cfg.quarantine_ticks = 12;
            cfg.fixed_point_scale = 1000;
            cfg.max_link_walk = 4096;
            dvel_sybil_overlay_set_config(overlay_, &cfg);
            // init local author's validation ctx
            (void)vctx_for_(author_);
        }

        ~NodeRuntime()
        {
            dvel_sybil_overlay_attach_trace_recorder(overlay_, nullptr);
            dvel_trace_recorder_free(trace_recorder_);
            dvel_sybil_overlay_free(overlay_);
            dvel_ledger_free(ledger_);
        }

        // identity
        uint32_t id() const { return node_id_; }
        const dvel_pubkey_t &author() const { return author_; }

        // tips
        dvel_hash_t current_tip_or_zero() const
        {
            dvel_hash_t tips[8];
            std::size_t n = dvel_ledger_get_tips(ledger_, tips, 8);
            if (n == 0)
                return dvel_hash_t{{0}};
            return tips[0];
        }

        dvel_preferred_tip_t preferred_tip(uint64_t tick) const
        {
            // Use sybil-aware weighting by default for production realism
            return dvel_select_preferred_tip_sybil(ledger_, overlay_, tick, 128);
        }

        uint64_t author_weight_sybil_fp(uint64_t tick, const dvel_pubkey_t &author) const
        {
            return dvel_sybil_overlay_author_weight_fp(overlay_, tick, author);
        }

        // Dump the Rust trace recorder to a JSON file (deterministic, no pretty-print).
        bool dump_trace_json(const std::string &path) const
        {
            std::ofstream out(path, std::ios::trunc);
            if (!out.is_open())
            {
                return false;
            }
            auto hex = [](const uint8_t *data, std::size_t len) {
                std::ostringstream oss;
                oss << std::hex << std::setfill('0');
                for (std::size_t i = 0; i < len; ++i)
                {
                    oss << std::setw(2) << static_cast<unsigned int>(data[i]);
                }
                return oss.str();
            };

            const std::size_t n = dvel_trace_recorder_len(trace_recorder_);
            out << "[";
            for (std::size_t i = 0; i < n; ++i)
            {
                dvel_trace_row_t row{};
                if (!dvel_trace_recorder_get(trace_recorder_, i, &row))
                    break;
                if (i > 0)
                    out << ",";
                out << "{";
                out << "\"node_id\":" << node_id_ << ",";
                out << "\"row_index\":" << i << ",";
                out << "\"prev_hash\":\"" << hex(row.prev_hash.bytes, 32) << "\",";
                out << "\"author\":\"" << hex(row.author.bytes, 32) << "\",";
                out << "\"timestamp\":" << static_cast<unsigned long long>(row.timestamp) << ",";
                out << "\"payload_hash\":\"" << hex(row.payload_hash.bytes, 32) << "\",";
                out << "\"signature\":\"" << hex(row.signature.bytes, 64) << "\",";
                out << "\"parent_present\":" << (row.parent_present ? "true" : "false") << ",";
                out << "\"ancestor_check\":" << (row.ancestor_check ? "true" : "false") << ",";
                out << "\"quarantined_until_before\":" << static_cast<unsigned long long>(row.quarantined_until_before) << ",";
                out << "\"quarantined_until_after\":" << static_cast<unsigned long long>(row.quarantined_until_after) << ",";
                if (row.merkle_root_has)
                    out << "\"merkle_root\":\"" << hex(row.merkle_root.bytes, 32) << "\",";
                else
                    out << "\"merkle_root\":null,";
                if (row.preferred_tip_has)
                    out << "\"preferred_tip\":\"" << hex(row.preferred_tip.bytes, 32) << "\",";
                else
                    out << "\"preferred_tip\":null,";
                out << "\"author_weight_fp\":" << static_cast<unsigned long long>(row.author_weight_fp);
                out << "}";
            }
            out << "]";
            return true;
        }

        // event creation (compatible with sim_baseline)
        Message make_event_message(uint64_t ts, const dvel_hash_t &prev, uint64_t payload_tag) const
        {
            Message m{};
            m.type = MsgType::Event;
            m.from = node_id_;
            m.to = node_id_;

            dvel_event_t ev{};
            ev.version = 1;
            ev.prev_hash = prev;
            ev.author = author_;
            ev.timestamp = ts;
            ev.payload_hash = make_payload_hash(payload_tag);
            // Sign after assembling the event
            dvel_sign_event(&ev, &secret_, &ev.signature);

            m.event = ev;
            return m;
        }

        // local append (used by some executables)
        bool local_append(const Message &m, uint64_t now_tick, bool verbose = false)
        {
            ProcessStats s;
            (void)accept_or_queue_(m, now_tick, verbose, s);
            return true;
        }

        void inbox_push(const Message &m) { inbox_.push_back(m); }

        // Returns detailed stats (needed by sim_metrics)
        ProcessStats process_inbox(uint64_t now_tick, bool verbose = false)
        {
            ProcessStats stats{};
            while (!inbox_.empty())
            {
                Message m = inbox_.front();
                inbox_.pop_front();
                (void)accept_or_queue_(m, now_tick, verbose, stats);
            }
            return stats;
        }

    private:
        enum class AcceptResult
        {
            Accepted,
            Pending,
            RejectedPerm
        };

        // Core accept path
        AcceptResult accept_or_queue_(const Message &m, uint64_t now_tick, bool verbose, ProcessStats &stats)
        {
            // Compute hash once for dedup
            dvel_hash_t ev_hash = dvel_hash_event_struct(&m.event);
            if (seen_hashes_.find(HashKey(ev_hash)) != seen_hashes_.end())
            {
                if (verbose)
                {
                    std::printf("node[%u] drop duplicate before validate (from=%u)\n", node_id_, m.from);
                }
                return AcceptResult::RejectedPerm;
            }

            // Validation
            auto &ctx = vctx_for_(m.event.author);
            auto vr = dvel_validate_event(&m.event, &ctx);
            if (vr != DVEL_OK)
            {
                stats.rejected_perm++;
                if (verbose)
                {
                    std::printf("node[%u] validation reject: %d (from=%u)\n", node_id_, (int)vr, m.from);
                }
                return AcceptResult::RejectedPerm;
            }

            // Linkage
            dvel_hash_t out{};
            auto lr = dvel_ledger_link_event(ledger_, &m.event, &out);
            if (lr == DVEL_LINK_OK)
            {
                stats.accepted++;
                dvel_sybil_overlay_observe_event(overlay_, ledger_, now_tick, node_id_, &out);
                seen_hashes_.insert(HashKey(out));
                if (seen_hashes_.size() > MAX_SEEN)
                {
                    // drop oldest arbitrarily by clearing (deterministic enough for sim)
                    seen_hashes_.clear();
                }
                // Drain children waiting for this newly linked hash
                drain_pending_for_parent_(out, now_tick, verbose, stats);
                return AcceptResult::Accepted;
            }

            if (lr == DVEL_LINK_ERR_DUPLICATE)
            {
                if (verbose)
                {
                    std::printf("node[%u] duplicate hash (from=%u)\n", node_id_, m.from);
                }
                // Treat duplicate as no-op; do not count as rejected.
                return AcceptResult::RejectedPerm;
            }

            if (lr == DVEL_LINK_ERR_MISSING_PARENT)
            {
                // Store in pending pool keyed by prev_hash
                queue_pending_(m, verbose, stats);
                return AcceptResult::Pending;
            }

            // Duplicate or other permanent failure
            stats.rejected_perm++;
            if (verbose)
            {
                std::printf("node[%u] linkage reject: %s (from=%u)\n", node_id_, link_to_str(lr), m.from);
            }
            return AcceptResult::RejectedPerm;
        }

        void queue_pending_(const Message &m, bool verbose, ProcessStats &stats)
        {
            if (pending_total_ >= MAX_PENDING_TOTAL)
            {
                // Drop newest pending when at cap (deterministic)
                stats.pending_dropped++;
                if (verbose)
                {
                    std::printf("node[%u] pending drop (cap=%zu) from=%u prev_tip=%02x%02x%02x%02x...\n",
                                node_id_, (std::size_t)MAX_PENDING_TOTAL, m.from,
                                m.event.prev_hash.bytes[0], m.event.prev_hash.bytes[1],
                                m.event.prev_hash.bytes[2], m.event.prev_hash.bytes[3]);
                }
                return;
            }

            HashKey parent(m.event.prev_hash);
            pending_by_parent_[parent].push_back(m);
            pending_total_++;
            stats.pending_added++;

            if (verbose)
            {
                // optional lightweight trace
                // std::printf("node[%u] pending added (total=%zu)\n", node_id_, pending_total_);
            }
        }

        void drain_pending_for_parent_(const dvel_hash_t &parent_hash, uint64_t now_tick, bool verbose, ProcessStats &stats)
        {
        HashKey key(parent_hash);
        auto it = pending_by_parent_.find(key);
            if (it == pending_by_parent_.end()) {
                return;
            }

            // Move bucket out to avoid iterator invalidation and allow re-queueing
            std::deque<Message> bucket = std::move(it->second);
            pending_by_parent_.erase(it);

            // Re-process these children; decrement total now and re-add if still pending.
            std::size_t bucket_sz = bucket.size();
            if (bucket_sz > pending_total_)
                pending_total_ = 0;
            else
                pending_total_ -= bucket_sz;

            int steps = 0;
            while (!bucket.empty() && steps < MAX_DRAIN_STEPS)
            {
                Message child = bucket.front();
                bucket.pop_front();
                steps++;

                // Re-validate for determinism and link
                dvel_hash_t out{};
                auto &ctx2 = vctx_for_(child.event.author);
                auto vr = dvel_validate_event(&child.event, &ctx2);
                if (vr != DVEL_OK)
                {
                    stats.rejected_perm++;
                    if (verbose)
                    {
                        std::printf("node[%u] pending child reject: %d (from=%u)\n", node_id_, (int)vr, child.from);
                    }
                    continue;
                }

                auto lr = dvel_ledger_link_event(ledger_, &child.event, &out);
                if (lr == DVEL_LINK_OK)
                {
                    if (seen_hashes_.find(HashKey(out)) != seen_hashes_.end())
                    {
                        if (verbose)
                        {
                            std::printf("node[%u] duplicate pending child (already accepted) from=%u\n", node_id_, child.from);
                        }
                        continue;
                    }
                    stats.accepted++;
                    stats.pending_drained++;
                    dvel_sybil_overlay_observe_event(overlay_, ledger_, now_tick, node_id_, &out);
                    // Recursively drain grandchildren
                    drain_pending_for_parent_(out, now_tick, verbose, stats);
                    continue;
                }

                if (lr == DVEL_LINK_ERR_DUPLICATE)
                {
                    if (verbose)
                    {
                        std::printf("node[%u] duplicate pending child (from=%u)\n", node_id_, child.from);
                    }
                    continue;
                }

                if (lr == DVEL_LINK_ERR_MISSING_PARENT)
                {
                    // Still missing something (grandparent). Put back to pending, but do not count as reject.
                    queue_pending_(child, verbose, stats);
                    continue;
                }

                // Duplicate or other permanent failure
                stats.rejected_perm++;
            }

            // If bucket still has items (hit drain bound), put them back deterministically.
            while (!bucket.empty())
            {
                Message child = bucket.front();
                bucket.pop_front();
                queue_pending_(child, verbose, stats);
            }
        }
    };

} // namespace dvelsim
