// Gossip policy abstractions: separate local append from broadcast topology.

#pragma once

#include <cstdint>
#include <vector>
#include <algorithm>

#include "types.hpp"
#include "bus.hpp"

namespace dvelsim {

class GossipPolicy {
public:
    virtual ~GossipPolicy() = default;

    // Deterministic broadcast hook invoked by the simulator.
    virtual void broadcast_event(
        MessageBus& bus,
        uint64_t now_tick,
        uint32_t from,
        const Message& msg,
        const std::vector<uint32_t>& peers
    ) = 0;
};

// Broadcast to all peers (except self) with fixed delay.
class BroadcastAll final : public GossipPolicy {
public:
    explicit BroadcastAll(uint64_t delay_ticks = 1) : delay(delay_ticks) {}

    void broadcast_event(
        MessageBus& bus,
        uint64_t now_tick,
        uint32_t from,
        const Message& msg,
        const std::vector<uint32_t>& peers
    ) override {
        for (uint32_t to : peers) {
            if (to == from) continue;
            bus.send(from, to, msg, now_tick, delay);
        }
    }

private:
    uint64_t delay;
};

// Eclipse policy: restrict delivery to a fixed allowlist (victim isolation).
class AllowlistOnly final : public GossipPolicy {
public:
    AllowlistOnly(std::vector<uint32_t> allow, uint64_t delay_ticks = 1)
        : allowlist(std::move(allow)), delay(delay_ticks) {
        std::sort(allowlist.begin(), allowlist.end());
    }

    void broadcast_event(
        MessageBus& bus,
        uint64_t now_tick,
        uint32_t from,
        const Message& msg,
        const std::vector<uint32_t>& peers
    ) override {
        (void)peers;
        for (uint32_t to : allowlist) {
            if (to == from) continue;
            bus.send(from, to, msg, now_tick, delay);
        }
    }

private:
    std::vector<uint32_t> allowlist;
    uint64_t delay;
};

} // namespace dvelsim
