#ifndef THREAD_POOL_H
#define THREAD_POOL_H

// Author: Hossein Taji

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    // Constructor to create and launch worker threads.
    ThreadPool(size_t num_threads);

    // Destructor to join all threads.
    ~ThreadPool();

    // Add a new task to the execution queue.
    void enqueue(std::function<void()> task);

private:
    // The main function for each worker thread.
    void worker();

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;

    // Synchronization primitives
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

#endif // THREAD_POOL_H
