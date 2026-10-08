#include <gtest/gtest.h>
#include "miniredis/kvstore.hpp"
#include <thread>
#include <vector>
#include <atomic>

class KVStoreTest : public ::testing::Test {
protected:
    miniredis::KVStore store;
};

TEST_F(KVStoreTest, SetAndGet) {
    store.set("user:100", "Alice");
    auto val = store.get("user:100");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "Alice");
}

TEST_F(KVStoreTest, GetNonExistentKeyReturnsNullopt) {
    auto val = store.get("nonexistent");
    EXPECT_FALSE(val.has_value());
}

TEST_F(KVStoreTest, OverwriteKey) {
    store.set("key1", "val1");
    store.set("key1", "val2");
    auto val = store.get("key1");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "val2");
}

TEST_F(KVStoreTest, DeleteExistingKey) {
    store.set("key1", "val1");
    EXPECT_TRUE(store.del("key1"));
    EXPECT_FALSE(store.get("key1").has_value());
}

TEST_F(KVStoreTest, DeleteNonExistentKeyReturnsFalse) {
    EXPECT_FALSE(store.del("missing_key"));
}

TEST_F(KVStoreTest, ExistsReturnsCorrectStatus) {
    EXPECT_FALSE(store.exists("mykey"));
    store.set("mykey", "myval");
    EXPECT_TRUE(store.exists("mykey"));
    store.del("mykey");
    EXPECT_FALSE(store.exists("mykey"));
}

TEST_F(KVStoreTest, ConcurrentReadWriteStressTest) {
    constexpr int num_writers = 4;
    constexpr int num_readers = 8;
    constexpr int ops_per_thread = 2000;

    std::vector<std::thread> threads;
    std::atomic<bool> start_flag{false};

    // Writers
    for (int i = 0; i < num_writers; ++i) {
        threads.emplace_back([this, i, &start_flag]() {
            while (!start_flag.load()) {}
            for (int j = 0; j < ops_per_thread; ++j) {
                std::string key = "key_" + std::to_string(i) + "_" + std::to_string(j % 50);
                std::string val = "val_" + std::to_string(j);
                store.set(key, val);
                if (j % 5 == 0) {
                    store.del(key);
                }
            }
        });
    }

    // Readers
    for (int i = 0; i < num_readers; ++i) {
        threads.emplace_back([this, &start_flag]() {
            while (!start_flag.load()) {}
            for (int j = 0; j < ops_per_thread; ++j) {
                std::string key = "key_" + std::to_string(j % num_writers) + "_" + std::to_string(j % 50);
                store.get(key);
                store.exists(key);
            }
        });
    }

    start_flag.store(true);

    for (auto& t : threads) {
        t.join();
    }
}
