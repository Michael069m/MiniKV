#include "miniredis/kvstore.hpp"
#include "miniredis/wal.hpp"
#include <mutex>

namespace miniredis {

KVStore::KVStore() = default;
KVStore::~KVStore() { stop_cleaner(); }

void KVStore::set_wal(WAL* wal) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    wal_ = wal;
}

bool KVStore::is_expired_unsafe(const std::string& key) const {
    auto it = expiry_.find(key);
    if (it == expiry_.end()) return false;
    return std::chrono::steady_clock::now() >= it->second;
}

bool KVStore::purge_if_expired_unsafe(const std::string& key) {
    if (is_expired_unsafe(key)) {
        store_.erase(key);
        expiry_.erase(key);
        if (wal_) wal_->append_del(key);
        return true;
    }
    return false;
}

void KVStore::set(const std::string& key, const std::string& value, std::optional<uint64_t> ttl_seconds) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (wal_) wal_->append_set(key, value);
    store_[key] = value;
    if (ttl_seconds.has_value()) {
        expiry_[key] = std::chrono::steady_clock::now() + std::chrono::seconds(*ttl_seconds);
    } else {
        expiry_.erase(key);
    }
}

std::optional<std::string> KVStore::get(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (purge_if_expired_unsafe(key)) return std::nullopt;
    auto it = store_.find(key);
    if (it == store_.end()) return std::nullopt;
    return it->second;
}

bool KVStore::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    expiry_.erase(key);
    if (wal_) wal_->append_del(key);
    return store_.erase(key) > 0;
}

bool KVStore::exists(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (purge_if_expired_unsafe(key)) return false;
    return store_.find(key) != store_.end();
}

bool KVStore::expire(const std::string& key, uint64_t seconds) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (purge_if_expired_unsafe(key) || store_.find(key) == store_.end()) return false;
    expiry_[key] = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    return true;
}

int64_t KVStore::ttl(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (purge_if_expired_unsafe(key) || store_.find(key) == store_.end()) return -2;
    auto it = expiry_.find(key);
    if (it == expiry_.end()) return -1;

    auto now = std::chrono::steady_clock::now();
    if (now >= it->second) return -2;

    auto remaining_ms = std::chrono::duration_cast<std::chrono::milliseconds>(it->second - now).count();
    if (remaining_ms <= 0) return -2;
    return (remaining_ms + 999) / 1000;
}

std::vector<std::string> KVStore::keys() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::string> result;
    for (auto it = store_.begin(); it != store_.end(); ) {
        if (is_expired_unsafe(it->first)) {
            expiry_.erase(it->first);
            if (wal_) wal_->append_del(it->first);
            it = store_.erase(it);
        } else {
            result.push_back(it->first);
            ++it;
        }
    }
    return result;
}

std::vector<KVEntry> KVStore::snapshot() {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<KVEntry> entries;
    auto now = std::chrono::steady_clock::now();

    for (const auto& [key, val] : store_) {
        auto exp_it = expiry_.find(key);
        if (exp_it != expiry_.end()) {
            if (now >= exp_it->second) continue; // Skip expired
            auto remaining_sec = std::chrono::duration_cast<std::chrono::seconds>(exp_it->second - now).count();
            entries.push_back({key, val, static_cast<uint64_t>(remaining_sec > 0 ? remaining_sec : 1)});
        } else {
            entries.push_back({key, val, std::nullopt});
        }
    }
    return entries;
}

size_t KVStore::cleanup_expired() {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    size_t count = 0;
    auto now = std::chrono::steady_clock::now();
    for (auto it = expiry_.begin(); it != expiry_.end(); ) {
        if (now >= it->second) {
            store_.erase(it->first);
            if (wal_) wal_->append_del(it->first);
            it = expiry_.erase(it);
            count++;
        } else {
            ++it;
        }
    }
    return count;
}

void KVStore::start_cleaner(std::chrono::milliseconds interval) {
    if (cleaner_running_.exchange(true)) return;
    cleaner_thread_ = std::thread([this, interval]() {
        while (cleaner_running_.load()) {
            std::this_thread::sleep_for(interval);
            if (!cleaner_running_.load()) break;
            cleanup_expired();
        }
    });
}

void KVStore::stop_cleaner() {
    if (cleaner_running_.exchange(false)) {
        if (cleaner_thread_.joinable()) cleaner_thread_.join();
    }
}

} // namespace miniredis
