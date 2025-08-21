#include "ThreadPool.h"

// Constructor implementation
ThreadPool::ThreadPool(size_t num_threads) : stop(false) {
    // Create and launch the specified number of worker threads.
    for (size_t i = 0; i < num_threads; ++i) {
        // Each thread will run the 'worker' member function.
        workers.emplace_back([this] { this->worker(); });
    }
}

// Destructor implementation
ThreadPool::~ThreadPool() {
    // Lock the queue to safely modify the 'stop' flag.
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }

    // Notify all waiting threads to wake them up.
    condition.notify_all();

    // Join all worker threads to wait for them to finish.
    for (std::thread &worker : workers) {
        worker.join();
    }
}

// Enqueue method implementation
void ThreadPool::enqueue(std::function<void()> task) {
    // Lock the queue to safely add a new task.
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        // std::move is a way to avoid making unnecessary copies of objects.
        tasks.push(std::move(task));
    }
    // Notify one waiting thread that a new task is available.
    condition.notify_one();
}

// Worker function implementation
void ThreadPool::worker() {
    // Loop indefinitely until the pool is stopped.
    while (true) {
        std::function<void()> task;

        // Lock the queue to wait for a task.
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // Wait until there's a task in the queue or the pool is stopped.
            condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

            // If the pool is stopped and the queue is empty, exit the thread.
            if (this->stop && this->tasks.empty()) {
                return;
            }

            // Get the next task from the queue.
            task = std::move(this->tasks.front());
            this->tasks.pop();
        }

        // Execute the task outside the lock.
        task();
    }
}