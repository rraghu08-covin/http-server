#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

// ── ThreadPool ────────────────────────────────────────────────────────────────
// Fixed-size worker pool.  Tasks are queued (FIFO) and dispatched to idle
// threads.  Thread-safe enqueue; graceful shutdown drains the queue and
// joins all worker threads.
class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    // Enqueue a callable.  No-op if shutdown() has been called.
    void enqueue(std::function<void()> task);

    // Number of tasks waiting to be picked up (not counting running ones).
    size_t queueSize() const;

    // Signal all workers to exit after draining the queue, then join them.
    // Safe to call multiple times.
    void shutdown();

private:
    void workerLoop();

    std::vector<std::thread>           workers_;
    std::queue<std::function<void()>>  tasks_;
    mutable std::mutex                 mutex_;
    std::condition_variable            cv_;
    std::atomic<bool>                  stop_{false};
};
