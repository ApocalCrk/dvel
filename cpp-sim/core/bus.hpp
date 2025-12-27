// Deterministic message bus: tick-indexed delivery with stable ordering (deliver_tick, seq)

#pragma once

#include <cstdint>
#include <queue>
#include <vector>

#include "types.hpp"

namespace dvelsim
{
    struct ScheduledMessage
    {
        uint64_t deliver_tick;
        uint64_t seq;
        Message msg;
    };

    struct ScheduledMessageGreater
    {
        bool operator()(const ScheduledMessage &a, const ScheduledMessage &b) const
        {
            if (a.deliver_tick != b.deliver_tick)
                return a.deliver_tick > b.deliver_tick; // min-heap
            return a.seq > b.seq;
        }
    };

    class MessageBus
    {
    public:
        explicit MessageBus(uint64_t default_delay_ticks = 1)
            : default_delay(default_delay_ticks), seq_counter(0) {}

        void send(uint32_t from, uint32_t to, const Message &msg, uint64_t now_tick, uint64_t delay_ticks = 0)
        {
            ScheduledMessage sm;
            sm.deliver_tick = now_tick + (delay_ticks == 0 ? default_delay : delay_ticks);
            sm.seq = seq_counter++;
            sm.msg = msg;
            sm.msg.from = from;
            sm.msg.to = to;
            q.push(sm);
        }

        template <typename InboxPushFn>
        void deliver(uint64_t now_tick, InboxPushFn push_into_inbox)
        {
            while (!q.empty())
            {
                const auto &top = q.top();
                if (top.deliver_tick > now_tick)
                    break;
                ScheduledMessage sm = top;
                q.pop();
                push_into_inbox(sm.msg.to, sm.msg);
            }
        }

        // Deliver with an adversarial scheduling policy
        template <typename SchedulePolicy, typename InboxPushFn>
        void deliver_with_policy(
            uint64_t now_tick,
            const SchedulePolicy& policy,
            InboxPushFn push_into_inbox
        ) {
            std::vector<ScheduledMessage> deferred;

            while (!q.empty()) {
                ScheduledMessage sm = q.top();
                if (sm.deliver_tick > now_tick) break;
                q.pop();

                if (policy.allow_delivery(sm, now_tick)) {
                    push_into_inbox(sm.msg.to, sm.msg);
                } else {
                    // keep message pending (delay / starvation)
                    deferred.push_back(sm);
                }
            }

            // Reinsert deferred messages deterministically
            for (const auto& sm : deferred) {
                q.push(sm);
            }
        }

        size_t pending() const { return q.size(); }

    private:
        uint64_t default_delay;
        uint64_t seq_counter;
        std::priority_queue<ScheduledMessage, std::vector<ScheduledMessage>, ScheduledMessageGreater> q;
    };
} // namespace dvelsim
