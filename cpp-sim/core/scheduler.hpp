// Adversarial Scheduler: deterministic time/delivery policy (delay, reorder, starvation).

#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>

#include "bus.hpp"

namespace dvelsim {

// Scheduler policy interface
class SchedulePolicy {
public:
    virtual ~SchedulePolicy() = default;

    // Decide delivery vs delay; true => deliver now, false => keep pending.
    virtual bool allow_delivery(
        const ScheduledMessage& msg,
        uint64_t now_tick
    ) const = 0;
};

// Honest policy: deliver everything on time
class HonestSchedule final : public SchedulePolicy {
public:
    bool allow_delivery(const ScheduledMessage&, uint64_t) const override {
        return true;
    }
};

// Delay policy: delay messages to specific nodes by fixed ticks
class FixedDelaySchedule final : public SchedulePolicy {
public:
    FixedDelaySchedule(uint32_t target_node, uint64_t extra_delay)
        : victim(target_node), delay(extra_delay) {}

    bool allow_delivery(const ScheduledMessage& msg, uint64_t now_tick) const override {
        // Delay messages to victim until deliver_tick + delay
        if (msg.msg.to == victim) {
            return now_tick >= (msg.deliver_tick + delay);
        }
        return true;
    }

private:
    uint32_t victim;
    uint64_t delay;
};

// Starvation policy: never deliver messages to a target node
class StarvationSchedule final : public SchedulePolicy {
public:
    explicit StarvationSchedule(uint32_t target_node)
        : victim(target_node) {}

    bool allow_delivery(const ScheduledMessage& msg, uint64_t) const override {
        return msg.msg.to != victim;
    }

private:
    uint32_t victim;
};

// Reorder policy: reverse ordering for a specific receiver
class ReorderSchedule final : public SchedulePolicy {
public:
    explicit ReorderSchedule(uint32_t target_node)
        : victim(target_node) {}

    bool allow_delivery(const ScheduledMessage& msg, uint64_t) const override {
        // Always allow; reordering handled at bus pop level
        (void)msg;
        return true;
    }

    bool should_reverse(uint32_t to) const {
        return to == victim;
    }

private:
    uint32_t victim;
};

} // namespace dvelsim
