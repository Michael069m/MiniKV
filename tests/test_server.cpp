#include <gtest/gtest.h>
#include "miniredis/kvstore.hpp"
#include "miniredis/command_handler.hpp"
#include "miniredis/thread_pool.hpp"
#include <atomic>

using namespace miniredis;

TEST(CommandHandlerTest, HandlePingSetGetDelExistsIncrKeys) {
    KVStore store;
    CommandHandler handler(store);

    // PING
    auto ping_cmd = RespValue::make_array({RespValue::make_bulk_string("PING")});
    auto ping_res = handler.handle_command(ping_cmd);
    EXPECT_EQ(ping_res.type, RespType::SimpleString);
    EXPECT_EQ(ping_res.string_val, "PONG");

    // SET
    auto set_cmd = RespValue::make_array({
        RespValue::make_bulk_string("SET"),
        RespValue::make_bulk_string("num"),
        RespValue::make_bulk_string("10")
    });
    auto set_res = handler.handle_command(set_cmd);
    EXPECT_EQ(set_res.string_val, "OK");

    // GET
    auto get_cmd = RespValue::make_array({
        RespValue::make_bulk_string("GET"),
        RespValue::make_bulk_string("num")
    });
    auto get_res = handler.handle_command(get_cmd);
    EXPECT_EQ(get_res.string_val, "10");

    // INCR
    auto incr_cmd = RespValue::make_array({
        RespValue::make_bulk_string("INCR"),
        RespValue::make_bulk_string("num")
    });
    auto incr_res = handler.handle_command(incr_cmd);
    EXPECT_EQ(incr_res.int_val, 11);

    // EXISTS
    auto exists_cmd = RespValue::make_array({
        RespValue::make_bulk_string("EXISTS"),
        RespValue::make_bulk_string("num")
    });
    auto exists_res = handler.handle_command(exists_cmd);
    EXPECT_EQ(exists_res.int_val, 1);

    // KEYS *
    auto keys_cmd = RespValue::make_array({
        RespValue::make_bulk_string("KEYS"),
        RespValue::make_bulk_string("*")
    });
    auto keys_res = handler.handle_command(keys_cmd);
    ASSERT_EQ(keys_res.array_val.size(), 1);
    EXPECT_EQ(keys_res.array_val[0].string_val, "num");

    // DEL
    auto del_cmd = RespValue::make_array({
        RespValue::make_bulk_string("DEL"),
        RespValue::make_bulk_string("num")
    });
    auto del_res = handler.handle_command(del_cmd);
    EXPECT_EQ(del_res.int_val, 1);
}

TEST(ThreadPoolTest, ConcurrentTaskExecution) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    constexpr int total_tasks = 100;

    for (int i = 0; i < total_tasks; ++i) {
        pool.enqueue([&counter]() {
            counter++;
        });
    }

    // Allow tasks to run
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_EQ(counter.load(), total_tasks);
}
