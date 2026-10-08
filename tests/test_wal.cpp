#include <gtest/gtest.h>
#include "miniredis/kvstore.hpp"
#include "miniredis/wal.hpp"
#include <filesystem>

namespace fs = std::filesystem;

class WALTest : public ::testing::Test {
protected:
    std::string test_wal_path = "test_miniredis.wal";

    void SetUp() override {
        if (fs::exists(test_wal_path)) fs::remove(test_wal_path);
    }
    void TearDown() override {
        if (fs::exists(test_wal_path)) fs::remove(test_wal_path);
    }
};

TEST_F(WALTest, LogAndReplayBasicOperations) {
    {
        miniredis::KVStore store;
        miniredis::WAL wal(test_wal_path);
        store.set_wal(&wal);

        store.set("user:1", "Alice");
        store.set("user:2", "Bob");
        store.del("user:2");
        store.set("user:1", "Alice Updated");
    }

    miniredis::KVStore restored_store;
    miniredis::WAL wal_replay(test_wal_path);
    size_t replayed = wal_replay.replay(restored_store);

    EXPECT_EQ(replayed, 4);
    EXPECT_EQ(restored_store.get("user:1").value(), "Alice Updated");
    EXPECT_FALSE(restored_store.get("user:2").has_value());
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

TEST_F(WALTest, LogCompactionReducesFileSize) {
    miniredis::KVStore store;
    miniredis::WAL wal(test_wal_path);
    store.set_wal(&wal);

    // Perform 500 redundant updates on a single key
    for (int i = 0; i < 500; ++i) {
        store.set("counter", std::to_string(i));
    }

    // Perform 200 insertions and deletions
    for (int i = 0; i < 200; ++i) {
        std::string temp_key = "temp_" + std::to_string(i);
        store.set(temp_key, "data");
        store.del(temp_key);
    }

    uint64_t uncompacted_size = fs::file_size(test_wal_path);
    EXPECT_GT(uncompacted_size, 10000); // > 10KB of uncompacted logs

    // Compact WAL
    wal.compact(store);

    uint64_t compacted_size = fs::file_size(test_wal_path);
    EXPECT_LT(compacted_size, 200); // Shrunk down to single active key entry (~30 bytes)

    // Replay compacted log into fresh store
    miniredis::KVStore restored_store;
    miniredis::WAL wal_replay(test_wal_path);
    size_t replayed = wal_replay.replay(restored_store);

    EXPECT_EQ(replayed, 1);
    EXPECT_EQ(restored_store.get("counter").value(), "499");
}
