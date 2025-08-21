#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <algorithm> // For std::find
#include "picosha2.h"

// --- Shared resources for the thread pool ---
std::queue<std::filesystem::path> file_queue;
std::mutex queue_mutex;
std::condition_variable queue_cv;
bool done_adding_files = false;

// --- Shared resources for progress and output ---
std::atomic<int> processed_files_count = 0;
int total_files = 0;
std::mutex cout_mutex;

// A helper function to compute the SHA256 hash of a file.
// It returns the hash as a hexadecimal string.
std::string get_file_hash(const std::filesystem::path& file_path) {
    // Open the file in binary mode
    std::ifstream file_stream(file_path, std::ios::binary);
    if (!file_stream.is_open()) {
        // Return an error message if the file can't be opened
        return "Error: Could not open file.";
    }

    // Use stream iterators to read the file content directly into the hasher
    // This is memory-efficient as it doesn't load the whole file at once.
    return picosha2::hash256_hex_string(
        std::istreambuf_iterator<char>(file_stream),
        std::istreambuf_iterator<char>()
    );
}

// The function that each worker thread will execute
void worker() {
    while (true) {
        std::filesystem::path file_path;

        // --- Critical Section: Accessing the Queue ---
        {
            // Acquire a unique_lock. This is more flexible than lock_guard and is required for condition variables.
            // The lock is automatically released when 'lock' goes out of scope.
            std::unique_lock<std::mutex> lock(queue_mutex);

            // Wait on the condition variable. The thread will sleep until:
            // 1. The queue is NOT empty, OR
            // 2. The producer is done adding files.
            // The lambda prevents "spurious wakeups" by re-checking the condition.
            queue_cv.wait(lock, []{ return !file_queue.empty() || done_adding_files; });

            // If the queue is empty AND the producer is done, there's no more work.
            if (file_queue.empty() && done_adding_files) {
                return; // Exit the thread
            }

            file_path = file_queue.front();
            file_queue.pop();
        } // The lock is released here

        // --- Do the work (outside the lock) ---
        std::string hash = get_file_hash(file_path);
        
        // Atomically increment the counter. This is thread-safe.
        int current_count = ++processed_files_count;

        // --- Critical Section for printing to the console ---
        {
            // Use a lock_guard to ensure only one thread writes to std::cout at a time
            std::lock_guard<std::mutex> lock(cout_mutex);
            std::cout << "[" << current_count << "/" << total_files << "] "
                      << file_path.filename().string() << ": " << hash << std::endl;
        }
    }
}


void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <directory_path> [-j <num_threads>]" << std::endl;
    std::cerr << "  -j <num_threads>: Optional. Specify the number of worker threads." << std::endl;
    std::cerr << "                    Defaults to the number of hardware cores." << std::endl;
}

int main(int argc, char* argv[]) {
    // --- 1. Argument Parsing ---
    if (argc < 2 || argc > 4) {
        print_usage(argv[0]);
        return 1;
    }

    std::filesystem::path directory_path;
    unsigned int num_threads = std::thread::hardware_concurrency(); // Default value

    std::vector<std::string> args(argv + 1, argv + argc);
    directory_path = args[0];

    auto it = std::find(args.begin(), args.end(), "-j");
    if (it != args.end() && std::next(it) != args.end()) {
        try {
            int n = std::stoi(*std::next(it));
            if (n > 0) {
                num_threads = n;
            } else {
                std::cerr << "Error: Number of threads must be positive." << std::endl;
                return 1;
            }
        } catch (const std::invalid_argument& e) {
            std::cerr << "Error: Invalid number provided for -j flag." << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }

    if (!std::filesystem::is_directory(directory_path)) {
        std::cerr << "Error: Provided path '" << directory_path.string() << "' is not a valid directory." << std::endl;
        return 1;
    }

    // --- Pre-scan to count files ---
    std::vector<std::filesystem::path> files_to_process;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
            if (entry.is_regular_file()) {
                files_to_process.push_back(entry.path());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error during pre-scan: " << e.what() << std::endl;
        return 1;
    }

    total_files = files_to_process.size();
    if (total_files == 0) {
        std::cout << "No files found in the specified directory." << std::endl;
        return 0;
    }
    std::cout << "Found " << total_files << " files to process." << std::endl;

    // --- Thread Pool Setup ---
    std::cout << "Using " << num_threads << " worker threads." << std::endl;
    std::vector<std::thread> threads;
    for (unsigned int i = 0; i < num_threads; ++i) {
        threads.emplace_back(worker);
    }

    // --- Producer: Add files from our vector to the queue ---
    for (const auto& path : files_to_process) {
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            file_queue.push(path);
        }
        queue_cv.notify_one();
    }

    // --- Signal that we are done adding files ---
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        done_adding_files = true;
    }
    // Notify ALL waiting threads so they can wake up and check the 'done_adding_files' flag.
    queue_cv.notify_all();

    // --- Join all threads ---
    for (auto& t : threads) {
        t.join();
    }

    std::cout << "All files processed." << std::endl;
    return 0;
}