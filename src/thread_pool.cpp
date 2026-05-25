#include "thread_pool.h"

// ── Constructor ───────────────────────────────────────────────────────────────
ThreadPool::ThreadPool(size_t numThreads) {
    workers_.reserve(numThreads);
    for (size_t i = 0; i < numThreads; ++i)
        workers_.emplace_back(&ThreadPool::workerLoop, this);
}

// ── Destructor ────────────────────────────────────────────────────────────────
ThreadPool::~ThreadPool() {
    shutdown();
}

// ── Worker loop ───────────────────────────────────────────────────────────────
void ThreadPool::workerLoop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return stop_.load() || !tasks_.empty(); });
            if (stop_.load() && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

// ── enqueue ───────────────────────────────────────────────────────────────────
void ThreadPool::enqueue(std::function<void()> task) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_.load()) return;   // reject after shutdown
        tasks_.push(std::move(task));
    }
    cv_.notify_one();
}

// ── queueSize ─────────────────────────────────────────────────────────────────
size_t ThreadPool::queueSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return tasks_.size();
}

// ── shutdown ──────────────────────────────────────────────────────────────────
void ThreadPool::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    cv_.notify_all();
    for (auto& w : workers_)
        if (w.joinable()) w.join();
    workers_.clear();
}
