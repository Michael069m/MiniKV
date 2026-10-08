#include <iostream>
#include "miniredis/kvstore.hpp"
#include "miniredis/wal.hpp"

int main() {
    std::cout << "MiniRedis Server starting (Phase 3 WAL persistence)...\n";

    const std::string wal_filepath = "miniredis.wal";
    miniredis::KVStore store;
    miniredis::WAL wal(wal_filepath, miniredis::FsyncPolicy::ALWAYS);

    std::cout << "Replaying WAL log from " << wal_filepath << "...\n";
    size_t replayed = wal.replay(store);
    std::cout << "Replayed " << replayed << " state operations.\n";

    store.set_wal(&wal);

    store.set("server:status", "running");
    store.set("last_boot", "2026-10-09");

    if (auto val = store.get("server:status")) {
        std::cout << "GET server:status -> " << *val << "\n";
    }

    return 0;
}
