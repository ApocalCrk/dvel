// Minimal FFI client: creates a single event, validates, links, and fetches it back.

#include <cstdio>
#include <cstring>

#include "../include/dvel_ffi.h"

int main() {
    // Deterministic secret/public key
    dvel_hash_t secret{};
    secret.bytes[0] = 0x42;
    dvel_pubkey_t pub{};
    if (!dvel_derive_pubkey_from_secret(&secret, &pub)) {
        std::printf("derive_pubkey failed\n");
        return 1;
    }

    // Ledger + validation context
    dvel_ledger_t *ledger = dvel_ledger_new();
    dvel_validation_ctx_t ctx{};
    dvel_validation_ctx_init(&ctx);

    // Build event
    dvel_event_t ev{};
    ev.version = 1;
    std::memset(&ev.prev_hash, 0, sizeof(ev.prev_hash)); // genesis
    ev.author = pub;
    ev.timestamp = 1;
    std::memset(&ev.payload_hash, 0xAB, sizeof(ev.payload_hash));

    // Sign
    dvel_sign_event(&ev, &secret, &ev.signature);

    // Validate + link
    dvel_validation_result_t vr = dvel_validate_event(&ev, &ctx);
    if (vr != DVEL_OK) {
        std::printf("validation failed: %d\n", (int)vr);
        return 1;
    }

    dvel_hash_t ev_hash{};
    dvel_link_result_t lr = dvel_ledger_link_event(ledger, &ev, &ev_hash);
    if (lr != DVEL_LINK_OK) {
        std::printf("link failed: %d\n", (int)lr);
        return 1;
    }

    // Fetch
    dvel_event_t fetched{};
    bool found = dvel_ledger_get_event(ledger, &ev_hash, &fetched);
    if (!found) {
        std::printf("get_event failed\n");
        return 1;
    }

    std::printf("OK: linked hash %02x%02x%02x%02x... ts=%llu\n",
                ev_hash.bytes[0], ev_hash.bytes[1], ev_hash.bytes[2], ev_hash.bytes[3],
                (unsigned long long)fetched.timestamp);

    dvel_ledger_free(ledger);
    return 0;
}
