#include <gtest/gtest.h>
#include "miniredis/kvstore.hpp"
#include "miniredis/command_handler.hpp"
#include <thread>

using namespace miniredis;

TEST(ExpiryTest, SetWithEXOption) {
    KVStore store;
    store.set("temp_key", "temp_val", 1); // 1 sec TTL

    EXPECT_EQ(store.get("temp_key").value(), "temp_val");
    EXPECT_GT(store.ttl("temp_key"), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    EXPECT_FALSE(store.get("temp_key").has_value());
    EXPECT_EQ(store.ttl("temp_key"), -2);
}

TEST(ExpiryTest, ExpireCommandAndTTL) {
    KVStore store;
    store.set("session", "active");

    EXPECT_EQ(store.ttl("session"), -1); // Persist

    EXPECT_TRUE(store.expire("session", 1));
    EXPECT_GT(store.ttl("session"), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    EXPECT_FALSE(store.exists("session"));
    EXPECT_EQ(store.ttl("session"), -2);
}

TEST(ExpiryTest, OverwriteClearsExpiry) {
    KVStore store;
    store.set("key", "val1", 1);
    store.set("key", "val2"); // Overwrite without TTL

    EXPECT_EQ(store.ttl("key"), -1); // Now persistent

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    auto val = store.get("key");
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(val.value(), "val2");
}

TEST(ExpiryTest, BackgroundCleanerThread) {
    KVStore store;
    store.start_cleaner(std::chrono::milliseconds(20));

    store.set("unaccessed_1", "v1", 1);
    store.set("unaccessed_2", "v2", 1);

    EXPECT_EQ(store.keys().size(), 2);

    std::this_thread::sleep_for(std::chrono::milliseconds(1200));

    // Background cleaner should have automatically purged expired keys
    EXPECT_EQ(store.keys().size(), 0);

    store.stop_cleaner();
}
