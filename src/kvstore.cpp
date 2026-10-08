#include "miniredis/kvstore.hpp"
#include "miniredis/wal.hpp"
#include <mutex>

namespace miniredis {

void KVStore::set_wal(WAL* wal) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    wal_ = wal;
}

void KVStore::set(const std::string& key, const std::string& value) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (wal_) {
        wal_->append_set(key, value);
    }
    store_[key] = value;
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    auto it = store_.find(key);
    if (it == store_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool KVStore::del(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mutex_);
    if (wal_) {
        wal_->append_del(key);
    }
    return store_.erase(key) > 0;
}

bool KVStore::exists(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    return store_.find(key) != store_.end();
}

std::vector<std::string> KVStore::keys() const {
    std::shared_lock<std::shared_mutex> lock(mutex_);
    std::vector<std::string> result;
    result.reserve(store_.size());
    for (const auto& [k, v] : store_) {
        result.push_back(k);
    }
    return result;
}

} // namespace miniredis
