#include "miniredis/wal.hpp"
#include "miniredis/kvstore.hpp"
#include <iostream>
#include <stdexcept>
#include <filesystem>

namespace fs = std::filesystem;

namespace miniredis {

WAL::WAL(const std::string& filepath, FsyncPolicy policy)
    : filepath_(filepath), policy_(policy) {
    log_file_.open(filepath_, std::ios::out | std::ios::app | std::ios::binary);
    if (!log_file_.is_open()) {
        throw std::runtime_error("Failed to open WAL file: " + filepath_);
    }
}

WAL::~WAL() {
    if (log_file_.is_open()) {
        flush_and_sync();
        log_file_.close();
    }
}

void WAL::append_set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_file_ << "S " << key.size() << " " << value.size() << " ";
    log_file_.write(key.data(), key.size());
    log_file_.write(value.data(), value.size());
    log_file_ << "\n";
    flush_and_sync();
}

void WAL::append_del(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    log_file_ << "D " << key.size() << " ";
    log_file_.write(key.data(), key.size());
    log_file_ << "\n";
    flush_and_sync();
}

void WAL::flush_and_sync() {
    log_file_.flush();
    if (policy_ == FsyncPolicy::ALWAYS) {
        log_file_.seekp(0, std::ios::cur);
    }
}

void WAL::sync() {
    std::lock_guard<std::mutex> lock(mutex_);
    log_file_.flush();
}

size_t WAL::replay(KVStore& store) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (log_file_.is_open()) {
        log_file_.close();
    }

    std::ifstream in(filepath_, std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        log_file_.open(filepath_, std::ios::out | std::ios::app | std::ios::binary);
        return 0;
    }

    size_t count = 0;
    char op;
    while (in >> op) {
        if (op == 'S') {
            size_t key_len = 0, val_len = 0;
            if (!(in >> key_len >> val_len)) break;

            char space;
            in.get(space);

            std::string key(key_len, '\0');
            in.read(&key[0], key_len);

            std::string val(val_len, '\0');
            in.read(&val[0], val_len);

            char newline;
            in.get(newline);

            store.set(key, val);
            count++;
        } else if (op == 'D') {
            size_t key_len = 0;
            if (!(in >> key_len)) break;

            char space;
            in.get(space);

            std::string key(key_len, '\0');
            in.read(&key[0], key_len);

            char newline;
            in.get(newline);

            store.del(key);
            count++;
        }
    }

    in.close();
    log_file_.open(filepath_, std::ios::out | std::ios::app | std::ios::binary);
    return count;
}

void WAL::compact(KVStore& store) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string temp_filepath = filepath_ + ".tmp";
    std::ofstream temp_file(temp_filepath, std::ios::out | std::ios::binary);
    if (!temp_file.is_open()) {
        throw std::runtime_error("Failed to open temporary WAL file for compaction: " + temp_filepath);
    }

    // Get live snapshot of non-expired keys/values
    auto entries = store.snapshot();
    for (const auto& entry : entries) {
        temp_file << "S " << entry.key.size() << " " << entry.value.size() << " ";
        temp_file.write(entry.key.data(), entry.key.size());
        temp_file.write(entry.value.data(), entry.value.size());
        temp_file << "\n";
    }

    temp_file.flush();
    temp_file.close();

    // Close active file handle before atomic swap
    if (log_file_.is_open()) {
        log_file_.close();
    }

    // Atomic file replacement
    fs::rename(temp_filepath, filepath_);

    // Re-open active log file in append mode
    log_file_.open(filepath_, std::ios::out | std::ios::app | std::ios::binary);
}

} // namespace miniredis
