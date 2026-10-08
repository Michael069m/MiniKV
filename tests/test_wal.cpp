#include <gtest/gtest.h>
#include "miniredis/kvstore.hpp"
#include "miniredis/wal.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

class WALTest : public ::testing::Test {
protected:
    std::string test_wal_path = "test_miniredis.wal";

    void SetUp() override {
        if (fs::exists(test_wal_path)) {
            fs::remove(test_wal_path);
        }
    }

    void TearDown() override {
        if (fs::exists(test_wal_path)) {
            fs::remove(test_wal_path);
        }
    }
};

TEST_F(WALTest, LogAndReplayBasicOperations) {
    {
        miniredis::KVStore store;
        miniredis::WAL wal(test_wal_path);
        store.set_wal(&wal);

        store.set("user:1", "Alice");
        store.set("user:2", "Bob");
        store.set("user:3", "Charlie");
        store.del("user:2");
        store.set("user:1", "Alice Updated");
    } // WAL and store flush & close here

    // Simulate crash and restart: create a new empty KVStore and replay log
    miniredis::KVStore restored_store;
    miniredis::WAL wal_replay(test_wal_path);
    size_t replayed_count = wal_replay.replay(restored_store);

    EXPECT_EQ(replayed_count, 5);

    // Verify state after recovery
    auto user1 = restored_store.get("user:1");
    ASSERT_TRUE(user1.has_value());
    EXPECT_EQ(user1.value(), "Alice Updated");

    EXPECT_FALSE(restored_store.get("user:2").has_value());

    auto user3 = restored_store.get("user:3");
    ASSERT_TRUE(user3.has_value());
    EXPECT_EQ(user3.value(), "Charlie");
}

TEST_F(WALTest, ReplayEmptyFileReturnsZero) {
    miniredis::KVStore store;
    miniredis::WAL wal(test_wal_path);
    size_t count = wal.replay(store);
    EXPECT_EQ(count, 0);
}

TEST_F(WALTest, BinaryValuesHandling) {
    {
        miniredis::KVStore store;
        miniredis::WAL wal(test_wal_path);
        store.set_wal(&wal);

        store.set("key with spaces", "value with spaces\nand newlines");
    }

    miniredis::KVStore restored_store;
    miniredis::WAL wal_replay(test_wal_path);
    wal_replay.replay(restored_store);

    auto val = restored_store.get("key with spaces");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "value with spaces\nand newlines");
}
