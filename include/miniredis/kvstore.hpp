#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <shared_mutex>

namespace miniredis {

class WAL; // Forward declaration

class KVStore {
public:
    KVStore() = default;
    ~KVStore() = default;

    // Prevent copying and moving
    KVStore(const KVStore&) = delete;
    KVStore& operator=(const KVStore&) = delete;
    KVStore(KVStore&&) = delete;
    KVStore& operator=(KVStore&&) = delete;

    void set_wal(WAL* wal);

    void set(const std::string& key, const std::string& value);
    std::optional<std::string> get(const std::string& key) const;
    bool del(const std::string& key);
    bool exists(const std::string& key) const;

private:
    std::unordered_map<std::string, std::string> store_;
    mutable std::shared_mutex mutex_;
    WAL* wal_{nullptr};
};

} // namespace miniredis
