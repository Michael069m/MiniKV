#include <iostream>
#include "miniredis/kvstore.hpp"
#include "miniredis/wal.hpp"
#include "miniredis/server.hpp"

int main() {
    std::cout << "Starting MiniRedis Server (Phase 5 TCP + Thread Pool)...\n";

    const std::string wal_filepath = "miniredis.wal";
    miniredis::KVStore store;
    miniredis::WAL wal(wal_filepath, miniredis::FsyncPolicy::ALWAYS);

    std::cout << "Replaying WAL log from " << wal_filepath << "...\n";
    size_t replayed = wal.replay(store);
    std::cout << "Replayed " << replayed << " state operations.\n";

    store.set_wal(&wal);

    miniredis::Server server(store, 6379, 4);
    server.start();

    return 0;
}
