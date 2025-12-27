/*
 * DVEL Reference Prototype Benchmark
 */

#include <iostream>
#include <vector>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <cstdio>

#include "dvel_ffi.h"

using namespace std::chrono;

// RAII wrappers for FFI types
struct Ledger {
    dvel_ledger_t* ptr;
    Ledger() { ptr = dvel_ledger_new(); }
    ~Ledger() { dvel_ledger_free(ptr); }
    operator dvel_ledger_t*() { return ptr; }
};

struct Overlay {
    dvel_sybil_overlay_t* ptr;
    Overlay() { ptr = dvel_sybil_overlay_new(); }
    ~Overlay() { dvel_sybil_overlay_free(ptr); }
    operator dvel_sybil_overlay_t*() { return ptr; }
};

int main() {
    // Benchmark Configuration
    const size_t NUM_EVENTS = 50000;
    const size_t NUM_AUTHORS = 10;
    const size_t TIP_SELECT_INTERVAL = 50; 

    std::cout << "========================================" << std::endl;
    std::cout << "   DVEL Reference Prototype Benchmark   " << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Events:  " << NUM_EVENTS << std::endl;
    std::cout << "Authors: " << NUM_AUTHORS << std::endl;
    std::cout << "----------------------------------------" << std::endl;
    
    std::cout << "Pre-generating events (signing)..." << std::endl;
    std::vector<dvel_event_t> events(NUM_EVENTS);

    Ledger ledger;
    Overlay overlay;
    
    dvel_validation_ctx_t val_ctx;
    dvel_validation_ctx_init(&val_ctx);

    // Setup Authors (Keys and Tips)
    std::vector<dvel_hash_t> secret_keys(NUM_AUTHORS);
    std::vector<dvel_pubkey_t> public_keys(NUM_AUTHORS);
    std::vector<dvel_hash_t> last_tips(NUM_AUTHORS);

    for (size_t i = 0; i < NUM_AUTHORS; ++i) {
        // Deterministic dummy keys
        memset(secret_keys[i].bytes, 0, 32);
        secret_keys[i].bytes[0] = (uint8_t)(i + 1); 
        
        if (!dvel_derive_pubkey_from_secret(&secret_keys[i], &public_keys[i])) {
            std::cerr << "Fatal: Failed to derive key for author " << i << std::endl;
            return 1;
        }
        // Initialize tips to zero (genesis parent)
        memset(last_tips[i].bytes, 0, 32); 
    }

    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        size_t author_idx = i % NUM_AUTHORS;
        uint64_t ts = 10000 + i; // Monotonic timestamp

        memset(&events[i], 0, sizeof(dvel_event_t));
        events[i].version = 1;
        events[i].prev_hash = last_tips[author_idx];
        events[i].author = public_keys[author_idx];
        events[i].timestamp = ts;
        
        // Dummy payload (hash of index)
        memset(events[i].payload_hash.bytes, 0xAA, 32);
        memcpy(events[i].payload_hash.bytes, &i, sizeof(i));

        // 1. Sign Event
        dvel_sign_event(&events[i], &secret_keys[author_idx], &events[i].signature);
        
        // Update local tip for author (simulate hash for next event)
        last_tips[author_idx] = dvel_hash_event_struct(&events[i]);
    }

    std::cout << "Starting benchmark loop (Validate -> Link -> Observe)..." << std::endl;
    auto start_time = high_resolution_clock::now();

    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        uint64_t ts = events[i].timestamp;

        // 2. Validate Event (Sim-side checks)
        dvel_validation_result_t vr = dvel_validate_event(&events[i], &val_ctx);
        if (vr != DVEL_OK) {
            std::cerr << "Validation failed at index " << i << " error=" << vr << std::endl;
            return 1;
        }

        // 3. Link to Ledger
        dvel_hash_t event_hash;
        dvel_link_result_t lr = dvel_ledger_link_event(ledger, &events[i], &event_hash);
        if (lr != DVEL_LINK_OK) {
            std::cerr << "Link failed at index " << i << " error=" << lr << std::endl;
            return 1;
        }

        // 4. Observe in Overlay (Sybil tracking)
        dvel_sybil_overlay_observe_event(overlay, ledger, ts, 0, &event_hash);

        // 5. Select Preferred Tip (Periodic)
        if (i % TIP_SELECT_INTERVAL == 0) {
            dvel_select_preferred_tip_sybil(ledger, overlay, ts, 100);
        }
    }

    auto end_time = high_resolution_clock::now();
    auto duration_us = duration_cast<microseconds>(end_time - start_time).count();
    double seconds = duration_us / 1000000.0;
    double ops_per_sec = NUM_EVENTS / seconds;

    std::cout << "----------------------------------------" << std::endl;
    std::cout << "Total Time: " << std::fixed << std::setprecision(3) << seconds << " s" << std::endl;
    std::cout << "Throughput: " << std::fixed << std::setprecision(2) << ops_per_sec << " events/sec" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    return 0;
}
