// Author: Hossein Taji

#include "ThreadPool.h"

ThreadPool::ThreadPool(size_t num_threads) : stop(false) {
    // Create and launch the specified number of worker threads.
    for (size_t i = 0; i < num_threads; ++i) {
        workers.emplace_back([this] { this->worker(); });
    }
}

ThreadPool::~ThreadPool() {
    // Lock the queue and set the stop flag.
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }

    // Wake up all threads so they can check the stop flag.
    condition.notify_all();

    // Wait for all threads to complete their work and exit.
    for (std::thread &worker : workers) {
        worker.join();
    }
}

void ThreadPool::enqueue(std::function<void()> task) {
    // Lock the queue to safely add the new task.
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        // std::move is a way to avoid making unnecessary copies of objects.
        tasks.push(std::move(task));
    }
    // Notify one waiting thread that a task is available.
    condition.notify_one();
}

void ThreadPool::worker() {
    while (true) {
        std::function<void()> task;

        // Lock the queue to wait for and retrieve a task.
        {
            std::unique_lock<std::mutex> lock(queue_mutex);

            // Wait until a task is available or the pool is stopped.
            condition.wait(lock, [this] { return this->stop || !this->tasks.empty(); });

            // If the pool is stopped and no tasks are left, exit the thread.
            if (this->stop && this->tasks.empty()) {
                return;
            }

            task = std::move(this->tasks.front());
            this->tasks.pop();
        }

        // Execute the task.
        task();
    }
}