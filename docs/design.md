# MiniRedis Architecture & Design Decisions

## Phase 1: In-Memory Key-Value Store (Single-Threaded Core)
- **Decision**: Built an in-memory key-value storage engine using `std::unordered_map<std::string, std::string>` wrapped in a clean C++17 RAII class (`miniredis::KVStore`).
- **Trade-off**: `std::unordered_map` provides $O(1)$ average time complexity for basic operations (`set`, `get`, `del`, `exists`). However, it is single-threaded and not thread-safe; concurrent access without external synchronization will lead to data races and undefined behavior. Memory overhead per entry includes bucket pointers and node pointers.

## Phase 2: Thread Safety with std::shared_mutex
- **Decision**: Used `std::shared_mutex` (C++17) to implement a Reader-Writer lock (Single-Writer, Multiple-Readers pattern). Write operations (`set`, `del`) acquire an exclusive lock via `std::unique_lock`, while read operations (`get`, `exists`) acquire a shared lock via `std::shared_lock`. `mutex_` is marked `mutable` to permit locking inside `const` member functions.
- **Trade-off**: Maximizes read concurrency by allowing simultaneous reader threads without blocking. However, write operations require global exclusive access, stalling readers during writes. Fine-grained bucket locking or lock-free data structures could provide higher write throughput at the expense of higher code complexity.

## Phase 3: Write-Ahead Log (WAL) & Crash Recovery
- **Decision**: Implemented an append-only Write-Ahead Log (`WAL`) using length-prefixed binary framing (`S <key_len> <val_len> <key><val>\n`). All write operations (`set`, `del`) log to disk prior to mutating the in-memory `unordered_map`. Startup replay restores system state sequentially.
- **Trade-off**: `FsyncPolicy::ALWAYS` guarantees zero data loss on power crashes by forcing synchronous disk flushes, but reduces write throughput to disk I/O latency limits. Length-prefixed binary framing adds minor header overhead per entry but guarantees safe handling of arbitrary binary payloads containing spaces or newlines.

## Phase 4: RESP Protocol Parser & Serializer
- **Decision**: Built a non-destructive zero-copy streaming parser (`RespParser`) operating over `std::string_view` buffers, alongside a typed serializer (`RespValue`). Supports all standard Redis data types (Simple Strings, Errors, Integers, Bulk Strings, Nulls, Arrays, and inline fallback).
- **Trade-off**: Returning `std::nullopt` on incomplete buffer reads enables seamless non-blocking TCP streaming without corrupting connection buffers. Constructing owned `std::string` objects during AST materialization simplifies memory management at the cost of heap allocation per frame token.

## Phase 5: POSIX TCP Server & ThreadPool Concurrency
- **Decision**: Built a multithreaded network server using POSIX sockets (`socket`, `bind`, `listen`, `accept`) paired with a producer-consumer task queue `ThreadPool` guarded by `std::condition_variable`. Incoming client connections are offloaded to worker threads, executing RESP commands against `KVStore`.
- **Trade-off**: Thread-per-connection / thread-pool model provides straightforward concurrent client handling and low implementation complexity. However, under tens of thousands of idle connections, thread stack memory overhead scales linearly compared to single-threaded event-loop architectures (e.g. `epoll`/`kqueue`).

## Phase 6: Hybrid Key Expiry (Lazy Eviction + Active Cleaner Thread)
- **Decision**: Combined passive/lazy eviction upon key access with a active background cleaner thread (`std::thread`) running periodically. `std::chrono::steady_clock` tracks expiration timestamps safely across system clock adjustments.
- **Trade-off**: Hybrid approach guarantees $O(1)$ check time on access while preventing memory leaks for unaccessed expired keys. Periodic lock acquisition by the cleaner thread briefly contention-competes with worker threads under heavy write volume.
