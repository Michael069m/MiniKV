# MiniRedis Architecture & Design Decisions

## Phase 1: In-Memory Key-Value Store (Single-Threaded Core)
- **Decision**: Built an in-memory key-value storage engine using `std::unordered_map<std::string, std::string>` wrapped in a clean C++17 RAII class (`miniredis::KVStore`).
- **Trade-off**: `std::unordered_map` provides $O(1)$ average time complexity for basic operations (`set`, `get`, `del`, `exists`). However, it is single-threaded and not thread-safe; concurrent access without external synchronization will lead to data races and undefined behavior. Memory overhead per entry includes bucket pointers and node pointers.

## Phase 2: Thread Safety with std::shared_mutex
- **Decision**: Used `std::shared_mutex` (C++17) to implement a Reader-Writer lock (Single-Writer, Multiple-Readers pattern). Write operations (`set`, `del`) acquire an exclusive lock via `std::unique_lock`, while read operations (`get`, `exists`) acquire a shared lock via `std::shared_lock`. `mutex_` is marked `mutable` to permit locking inside `const` member functions.
- **Trade-off**: Maximizes read concurrency by allowing simultaneous reader threads without blocking. However, write operations require global exclusive access, stalling readers during writes. Fine-grained bucket locking or lock-free data structures could provide higher write throughput at the expense of higher code complexity.
