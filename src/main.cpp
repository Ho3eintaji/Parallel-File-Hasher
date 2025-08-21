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
#include <unordered_set> // For efficient extension filtering
#include <utility> // For std::pair
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

// --- Shared resource to store results ---
std::vector<std::pair<std::filesystem::path, std::string>> results;
std::mutex results_mutex;

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
                return;
            }
            file_path = file_queue.front();
            file_queue.pop();
        } // The lock is released here

        // --- Do the work (outside the lock) ---
        std::string hash = get_file_hash(file_path);

        {
            std::lock_guard<std::mutex> lock(results_mutex);
            results.emplace_back(file_path, hash);
        }

        int current_count = ++processed_files_count;

        {
            // Use a lock_guard to ensure only one thread writes to std::cout at a time
            std::lock_guard<std::mutex> lock(cout_mutex);
            
            float percentage = static_cast<float>(current_count) / total_files * 100.0f;
            const int bar_width = 50;
            int pos = static_cast<int>(bar_width * percentage / 100.0);

            std::cout << "\r[";
            for (int i = 0; i < bar_width; ++i) {
                if (i < pos) std::cout << "=";
                else if (i == pos) std::cout << ">";
                else std::cout << " ";
            }
            std::cout << "] " << static_cast<int>(percentage) << "% (" << current_count << "/" << total_files << ")  ";
            std::cout << std::flush;
        }
    }
}


// Updated to include the new output option
void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <directory_path> [options]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -j <num_threads>      Specify the number of worker threads. Defaults to hardware cores." << std::endl;
    std::cerr << "  -r, --recursive       Scan directories recursively." << std::endl;
    std::cerr << "  --filter .ext1 .ext2  Only process files with the specified extensions." << std::endl;
    std::cerr << "  -o, --output <file>   Write the final hash report to a file instead of the console." << std::endl;
}

int main(int argc, char* argv[]) {
    // --- 1. Argument Parsing ---
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    std::vector<std::string> args(argv + 1, argv + argc);
    std::filesystem::path directory_path = args[0];

    // Default values for options
    std::string output_file_path;
    unsigned int num_threads = std::thread::hardware_concurrency();
    bool recursive = false;
    std::unordered_set<std::string> filters;

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-j" && i + 1 < args.size()) {
            try { num_threads = std::stoi(args[++i]); } catch (...) {}
        } else if (args[i] == "-r" || args[i] == "--recursive") {
            recursive = true;
        } else if ((args[i] == "-o" || args[i] == "--output") && i + 1 < args.size()) {
            output_file_path = args[++i];
        } else if (args[i] == "--filter" && i + 1 < args.size()) {
            while (++i < args.size() && args[i][0] != '-') { filters.insert(args[i]); }
            --i;
        }
    }

    if (!std::filesystem::is_directory(directory_path)) {
        std::cerr << "Error: Provided path '" << directory_path.string() << "' is not a valid directory." << std::endl;
        return 1;
    }

    // --- 2. File Discovery ---
    std::vector<std::filesystem::path> files_to_process;
    std::cout << "Scanning for files..." << std::endl;
    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory_path)) {
                if (entry.is_regular_file()) {
                    // Apply filter if one is provided
                    if (filters.empty() || filters.count(entry.path().extension().string())) {
                        files_to_process.push_back(entry.path());
                    }
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
                if (entry.is_regular_file()) {
                     if (filters.empty() || filters.count(entry.path().extension().string())) {
                        files_to_process.push_back(entry.path());
                    }
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error during scan: " << e.what() << std::endl;
        return 1;
    }

    total_files = files_to_process.size();
    if (total_files == 0) {
        std::cout << "No matching files found to process." << std::endl;
        return 0;
    }
    std::cout << "Found " << total_files << " files." << std::endl;

    // --- 3. Thread Pool and Processing ---
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

    std::cout << std::endl;

    // --- final report section ---
    if (!output_file_path.empty()) {
        std::cout << "Writing report to " << output_file_path << "..." << std::endl;
        std::ofstream output_file(output_file_path);
        if (!output_file.is_open()) {
            std::cerr << "Error: Could not open output file for writing." << std::endl;
        } else {
            for (const auto& pair : results) {
                output_file << pair.first.string() << ": " << pair.second << std::endl;
            }
        }
    } else {
        std::cout << "--- Hash Report ---" << std::endl;
        for (const auto& pair : results) {
            std::cout << pair.first.string() << ": " << pair.second << std::endl;
        }
        std::cout << "-------------------" << std::endl;
    }
    std::cout << "All files processed." << std::endl;


    return 0;
}