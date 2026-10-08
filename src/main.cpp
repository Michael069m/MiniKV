#include <iostream>
#include "miniredis/kvstore.hpp"
#include "miniredis/wal.hpp"
#include "miniredis/server.hpp"

int main() {
    std::cout << "Starting MiniRedis Server (Phase 6 Expiry & Active Cleaner)...\n";

    const std::string wal_filepath = "miniredis.wal";
    miniredis::KVStore store;
    miniredis::WAL wal(wal_filepath, miniredis::FsyncPolicy::ALWAYS);

    std::cout << "Replaying WAL log from " << wal_filepath << "...\n";
    size_t replayed = wal.replay(store);
    std::cout << "Replayed " << replayed << " state operations.\n";

    store.set_wal(&wal);

    // Start background key cleaner thread (purges expired keys every 100ms)
    store.start_cleaner(std::chrono::milliseconds(100));

    miniredis::Server server(store, 6379, 4);
    server.start();

    return 0;
}
