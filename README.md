# MiniRedis: Multithreaded Redis-Compatible Key-Value Server

MiniRedis is a high-performance, multithreaded, Redis-compatible key-value server built in C++17 for Linux and POSIX systems without external networking or database libraries. It implements the Redis Serialization Protocol (RESP), allowing standard client drivers (`redis-cli`, Python `redis`, Node `ioredis`) to connect seamlessly. 

Data durability is maintained via a Write-Ahead Log (WAL) with length-prefixed binary framing, startup crash recovery, atomic log compaction, and a hybrid key expiration engine (passive lazy eviction + active background cleaner thread).

---

## Architecture Diagram

```text
               +-------------------------------------------------+
               |  Clients (redis-cli, python-redis, custom TCP)   |
               +-------------------------------------------------+
                                        | (RESP Protocol over TCP)
                                        v
               +-------------------------------------------------+
               |              POSIX TCP Server Socket            |
               |             (bind/listen/accept loop)           |
               +-------------------------------------------------+
                                        | (Offload Connection)
                                        v
               +-------------------------------------------------+
               |           Worker ThreadPool (N Threads)          |
               |        (std::mutex & std::condition_variable)   |
               +-------------------------------------------------+
                                        |
                   +--------------------+--------------------+
                   |                                         |
                   v                                         v
        +-----------------------+                 +---------------------+
        |  RESP Streaming Parser|                 |   Command Handler   |
        |  (std::string_view)   |                 | (PING, SET, GET...) |
        +-----------------------+                 +---------------------+
                                                             |
                                                             v
+---------------------------------------------------------------------------------------------------+
|                                      miniredis::KVStore                                           |
|  +-----------------------------------+     +---------------------------------------------------+  |
|  | std::unordered_map<string,string> |     | std::shared_mutex (Single-Writer, Multi-Reader)   |  |
|  +-----------------------------------+     +---------------------------------------------------+  |
|  | Expiry Map & Active Background    |     | Length-Prefixed Write-Ahead Log (WAL)             |  |
|  | Cleaner Thread (steady_clock)     |     | (fdatasync/fsync & Atomic Rename Compaction)      |  |
|  +-----------------------------------+     +---------------------------------------------------+  |
+---------------------------------------------------------------------------------------------------+
```

---

## Features

- **POSIX Sockets & Networking:** Pure C++17 socket implementation without Boost.Asio or external dependencies.
- **Redis Protocol (RESP):** Full support for Simple Strings, Errors, Integers, Bulk Strings, Nulls, and Arrays.
- **Thread-Safe Data Engine:** Single-Writer Multiple-Reader locking model using `std::shared_mutex`.
- **Thread Pool:** Fixed worker thread pool using C++17 synchronization primitives (`std::condition_variable`, `std::mutex`).
- **Persistence & Durability (WAL):** Write-Ahead Logging prior to memory mutations with length-prefixed binary framing.
- **Crash Recovery:** Sequential startup replay restoring complete memory state after process crashes (`kill -9`).
- **Log Compaction (AOF Rewrite):** Active state snapshotting and atomic POSIX file replacement (`std::filesystem::rename`).
- **Hybrid Expiry Engine:** Passive lazy eviction on access + periodic active background cleaner thread (`std::thread`).

---

## Supported Commands

| Command | Usage | Description |
| :--- | :--- | :--- |
| `PING` | `PING [message]` | Returns `+PONG` or bulk string echo |
| `SET` | `SET key value [EX seconds]` | Sets key to value with optional expiration seconds |
| `GET` | `GET key` | Retrieves key value, returns `nil` (`$-1\r\n`) if missing/expired |
| `DEL` | `DEL key [key ...]` | Removes specified keys, returns integer count of deleted keys |
| `EXISTS` | `EXISTS key [key ...]` | Checks existence of keys, returns count |
| `INCR` | `INCR key` | Increments integer value by 1 |
| `EXPIRE` | `EXPIRE key seconds` | Sets expiration timeout on key in seconds |
| `TTL` | `TTL key` | Returns remaining TTL in seconds (`-2` if missing, `-1` if persistent) |
| `KEYS` | `KEYS pattern` | Returns all matching active keys (`KEYS *`) |

---

## Build & Run

### Prerequisites
- C++17 compatible compiler (`clang++` or `g++`)
- `cmake` (VERSION $\ge$ 3.14)
- `make`

### Build Steps
```bash
# 1. Clone repository
git clone https://github.com/Michael069m/MiniKV.git
cd MiniKV

# 2. Configure build
mkdir -p build && cd build
cmake ..

# 3. Compile binaries (-j4)
make -j4

# 4. Run GoogleTest Suite (25 unit tests)
./unit_tests

# 5. Run Live MiniRedis Server
./miniredis_server
```

---

## Benchmark Results

Benchmark measured over 10 concurrent client connection threads issuing 20,000 requests over TCP loopback socket:

| Command | Throughput (Requests / Sec) | Average Latency |
| :--- | :--- | :--- |
| **GET** | **93,977.05 req/sec** | **0.011 ms** |
| **PING** | **79,682.13 req/sec** | **0.013 ms** |
| **INCR** | **74,155.67 req/sec** | **0.013 ms** |
| **SET** | **63,881.03 req/sec** | **0.016 ms** |

---

## Design Decisions & Trade-offs

1. **`std::shared_mutex` for Read-Write Locking:**
   - *Decision:* Used `std::shared_mutex` with `std::shared_lock` for read operations (`get`, `exists`) and `std::unique_lock` for write operations (`set`, `del`).
   - *Trade-off:* Maximizes concurrent read throughput. Writes require global exclusive locks, brief contention under heavy write workloads.

2. **Length-Prefixed Binary Framing for WAL:**
   - *Decision:* Formatted WAL entries with explicit payload lengths (`S <key_len> <val_len> <key><val>\n`).
   - *Trade-off:* Adds minor byte length metadata overhead per entry but guarantees binary safety when keys/values contain embedded newlines or spaces.

3. **Atomic File Swap for Log Compaction:**
   - *Decision:* Snapshotting active keys to `.wal.tmp` and swapping via `std::filesystem::rename()`.
   - *Trade-off:* Guarantees crash consistency during log compaction (>99% size reduction). Requires extra disk space for the temporary file during rewrite execution.

4. **Hybrid Expiry (Lazy Eviction + Background Thread):**
   - *Decision:* Combined passive check on key read with a background worker thread (`std::thread`) running every 100ms.
   - *Trade-off:* Guarantees $O(1)$ read checks while preventing memory leaks for unaccessed expired keys.

---

## Honest System Limitations

1. **Thread-per-Connection Scaling:** Worker thread pool allocates threads for concurrent client sockets. While efficient up to thousands of clients, single-threaded event-loop architectures (`epoll`/`kqueue`) scale better to $100,000+$ idle connections.
2. **Global Store Lock:** A single `std::shared_mutex` guards the global hash map. Partitioned hash tables or sharded bucket locks would increase write parallelism under high concurrency.
3. **Single Database Index:** MiniRedis operates on a single database namespace without multi-database indexing (`SELECT 0..15`).
