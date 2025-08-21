// ----------------------------------------------------------------------------
// Parallel File Hasher
// Author: Hossein Taji
//
// A high-performance C++ utility to compute SHA256 hashes for files
// in a directory tree. It leverages a thread pool for parallel processing.
// ----------------------------------------------------------------------------

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <atomic>
#include <algorithm>
#include <unordered_set>
#include <utility>

#include "picosha2.h"
#include "ThreadPool.h"

// Shared resources for progress, output, and results.
std::atomic<int> processed_files_count = 0;
int total_files = 0;
std::mutex cout_mutex;
std::vector<std::pair<std::filesystem::path, std::string>> results;
std::mutex results_mutex;

// The main task for processing a single file.
void process_file(const std::filesystem::path& file_path) {
    // 1. Hash the file
    std::ifstream file_stream(file_path, std::ios::binary);
    if (!file_stream.is_open()) return;
    std::string hash = picosha2::hash256_hex_string(
        std::istreambuf_iterator<char>(file_stream),
        std::istreambuf_iterator<char>()
    );

    // 2. Store the result
    {
        std::lock_guard<std::mutex> lock(results_mutex);
        results.emplace_back(file_path, hash);
    }

    // 3. Update and display progress
    int current_count = ++processed_files_count;
    {
        std::lock_guard<std::mutex> lock(cout_mutex);
        float percentage = static_cast<float>(current_count) / total_files * 100.0f;
        const int bar_width = 50;
        int pos = static_cast<int>(bar_width * percentage / 100.0);
        std::cout << "\r[";
        for (int i = 0; i < bar_width; ++i) {
            if (i < pos) std::cout << "="; else if (i == pos) std::cout << ">"; else std::cout << " ";
        }
        std::cout << "] " << static_cast<int>(percentage) << "% (" << current_count << "/" << total_files << ")  ";
        std::cout << std::flush;
    }
}

void print_usage(const char* prog_name) {
    std::cerr << "Usage: " << prog_name << " <directory_path> [options]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -j <num_threads>      Specify the number of worker threads. Defaults to hardware cores." << std::endl;
    std::cerr << "  -r, --recursive       Scan directories recursively." << std::endl;
    std::cerr << "  --filter .ext1 .ext2  Only process files with the specified extensions." << std::endl;
    std::cerr << "  -o, --output <file>   Write the final hash report to a file instead of the console." << std::endl;
}

int main(int argc, char* argv[]) {
    // Argument parsing
    if (argc < 2) { print_usage(argv[0]); return 1; }
    std::vector<std::string> args(argv + 1, argv + argc);
    std::filesystem::path directory_path = args[0];
    std::string output_file_path;
    unsigned int num_threads = std::thread::hardware_concurrency();
    bool recursive = false;
    std::unordered_set<std::string> filters;
    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "-j" && i + 1 < args.size()) { try { num_threads = std::stoi(args[++i]); } catch (...) {} } 
        else if (args[i] == "-r" || args[i] == "--recursive") { recursive = true; } 
        else if ((args[i] == "-o" || args[i] == "--output") && i + 1 < args.size()) { output_file_path = args[++i]; } 
        else if (args[i] == "--filter" && i + 1 < args.size()) { while (++i < args.size() && args[i][0] != '-') { filters.insert(args[i]); } --i; }
    }
    if (!std::filesystem::is_directory(directory_path)) { std::cerr << "Error: Not a valid directory." << std::endl; return 1; }

    // File discovery
    std::vector<std::filesystem::path> files_to_process;
    std::cout << "Scanning for files..." << std::endl;
    try {
        if (recursive) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(directory_path)) {
                if (entry.is_regular_file() && (filters.empty() || filters.count(entry.path().extension().string()))) {
                    files_to_process.push_back(entry.path());
                }
            }
        } else {
            for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
                if (entry.is_regular_file() && (filters.empty() || filters.count(entry.path().extension().string()))) {
                    files_to_process.push_back(entry.path());
                }
            }
        }
    } catch (const std::filesystem::filesystem_error& e) { std::cerr << "Filesystem error: " << e.what() << std::endl; return 1; }
    total_files = files_to_process.size();
    if (total_files == 0) { std::cout << "No matching files found." << std::endl; return 0; }
    std::cout << "Found " << total_files << " files. Starting processing..." << std::endl;

    // Create a scope for the ThreadPool to ensure its destructor is called
    // before we try to print the results.
    {
        ThreadPool pool(num_threads);
        for (const auto& path : files_to_process) {
            pool.enqueue([path] {
                process_file(path);
            });
        }
    } 

    // Final report logic
    std::cout << std::endl;
    if (!output_file_path.empty()) {
        std::cout << "Writing report to " << output_file_path << "..." << std::endl;
        std::ofstream output_file(output_file_path);
        if (!output_file.is_open()) { std::cerr << "Error: Could not open output file." << std::endl; } 
        else { for (const auto& pair : results) { output_file << pair.first.string() << ": " << pair.second << std::endl; } }
    } else {
        std::cout << "--- Hash Report ---" << std::endl;
        for (const auto& pair : results) { std::cout << pair.first.string() << ": " << pair.second << std::endl; }
        std::cout << "-------------------" << std::endl;
    }
    std::cout << "All files processed." << std::endl;
    return 0;
}
