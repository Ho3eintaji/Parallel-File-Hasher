#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>

class ThreadPool {
public:
    // Constructor: creates the thread pool with a specified number of threads.
    ThreadPool(size_t num_threads);

    // Destructor: joins all threads, ensuring a clean shutdown.
    ~ThreadPool();

    // Enqueues a task (a function with no arguments and no return value) to be executed by a worker thread.
    void enqueue(std::function<void()> task);

private:
    // The worker function that each thread will execute.
    void worker();

    // A vector to hold the worker threads.
    std::vector<std::thread> workers;

    // The queue of tasks to be executed.
    std::queue<std::function<void()>> tasks;

    // Mutex to protect access to the tasks queue.
    std::mutex queue_mutex;

    // Condition variable to signal threads about new tasks or shutdown.
    std::condition_variable condition;

    // A flag to indicate whether the thread pool should stop.
    bool stop;
};

#endif // THREAD_POOL_H