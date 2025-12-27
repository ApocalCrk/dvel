// Minimal Sybil Mitigation Scoring
// Strict-causal, deterministic

#pragma once

#include <cstdint>
#include <unordered_map>

#include "types.hpp"

namespace dvelsim {

// Tunable constants (v0.1 fixed)
static constexpr uint64_t RATE_WINDOW = 5;
static constexpr uint64_t DECAY_WINDOW = 10;

struct AuthorState {
    uint64_t last_timestamp = 0;
};

class ScoringContext {
public:
    void observe_event(const dvel_event_t& e) {
        auto& st = authors[e.author.bytes[0]]; // deterministic small-key index
        st.last_timestamp = e.timestamp;
    }

    double event_weight(const dvel_event_t& e,
                        uint64_t now_tick,
                        uint64_t fork_depth) const {
        const double base = 1.0;

        // H1 - rate dampening
        double rate_factor = 1.0;
        auto it = authors.find(e.author.bytes[0]);
        if (it != authors.end()) {
            uint64_t dt = e.timestamp > it->second.last_timestamp
                            ? e.timestamp - it->second.last_timestamp
                            : 0;
            rate_factor = dt >= RATE_WINDOW ? 1.0
                                            : double(dt) / double(RATE_WINDOW);
        }

        // H2 - fork depth penalty
        double fork_factor = 1.0 / (1.0 + double(fork_depth));

        // H3 - temporal decay
        uint64_t age = now_tick > e.timestamp ? now_tick - e.timestamp : 0;
        double decay = 1.0 / (1.0 + double(age) / double(DECAY_WINDOW));

        return base * rate_factor * fork_factor * decay;
    }

private:
    std::unordered_map<uint8_t, AuthorState> authors;
};

} // namespace dvelsim
