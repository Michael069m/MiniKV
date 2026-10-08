#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <shared_mutex>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>

namespace miniredis {

class WAL;

struct KVEntry {
    std::string key;
    std::string value;
    std::optional<uint64_t> ttl_seconds;
};

class KVStore {
public:
    KVStore();
    ~KVStore();

    // Non-copyable, non-movable
    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;
    KVStore(KVStore&&) = delete;
    KVStore& operator=(KVStore&&) = delete;

    void set_wal(WAL* wal);

    void set(const std::string& key, const std::string& value, std::optional<uint64_t> ttl_seconds = std::nullopt);
    std::optional<std::string> get(const std::string& key);
    bool del(const std::string& key);
    bool exists(const std::string& key);
    bool expire(const std::string& key, uint64_t seconds);
    int64_t ttl(const std::string& key);
    std::vector<std::string> keys();

    // Snapshot current non-expired state for compaction
    std::vector<KVEntry> snapshot();

    // Background cleaner
    void start_cleaner(std::chrono::milliseconds interval = std::chrono::milliseconds(100));
    void stop_cleaner();
    size_t cleanup_expired();

private:
    std::unordered_map<std::string, std::string> store_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> expiry_;
    mutable std::shared_mutex mutex_;
    WAL* wal_{nullptr};

    std::thread cleaner_thread_;
    std::atomic<bool> cleaner_running_{false};

    bool is_expired_unsafe(const std::string& key) const;
    bool purge_if_expired_unsafe(const std::string& key);
};

} // namespace miniredis
