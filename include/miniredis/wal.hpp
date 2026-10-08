#pragma once

#include <string>
#include <fstream>
#include <mutex>

namespace miniredis {

class KVStore;

enum class FsyncPolicy {
    ALWAYS,
    EVERY_SEC,
    NO
};

class WAL {
public:
    explicit WAL(const std::string& filepath, FsyncPolicy policy = FsyncPolicy::ALWAYS);
    ~WAL();

    // Non-copyable, non-movable
    WAL(const WAL&) = delete;
    WAL& operator=(const WAL&) = delete;
    WAL(WAL&&) = delete;
    WAL& operator=(WAL&&) = delete;

    void append_set(const std::string& key, const std::string& value);
    void append_del(const std::string& key);

    // Replay log entries into KVStore instance to rebuild memory state
    size_t replay(KVStore& store);

    // Compact WAL log by rewriting active state to temporary file and atomically swapping
    void compact(KVStore& store);
    
    void sync();

private:
    std::string filepath_;
    FsyncPolicy policy_;
    std::ofstream log_file_;
    mutable std::mutex mutex_;

    void flush_and_sync();
};

} // namespace miniredis
