#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <thread> 
#include "picosha2.h"

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

// A function that is the entry point for each thread.
// It combines getting the hash and printing it.
void process_and_print_hash(const std::filesystem::path& file_path) {
    std::string hash = get_file_hash(file_path);
    
    // Note: Multiple threads writing to std::cout at the same time can jumble the output.
    // We will fix this properly with a mutex in a later step. For now, we accept it.
    std::cout << file_path.filename().string() << ": " << hash << std::endl;
}

int main(int argc, char* argv[]) {
    // --- 1. Argument Validation ---
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <directory_path>" << std::endl;
        return 1; // Indicate error
    }

    std::filesystem::path directory_path = argv[1];

    if (!std::filesystem::is_directory(directory_path)) {
        std::cerr << "Error: Provided path '" << directory_path << "' is not a valid directory." << std::endl;
        return 1;
    }

    std::cout << "Scanning directory: " << directory_path << std::endl;
    
    // --- 2. Create a vector to hold our thread objects ---
    std::vector<std::thread> threads;

    try {
        // --- 3. Launch one thread per file ---
        for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
            if (entry.is_regular_file()) {
                // Create a thread that runs 'process_and_print_hash' with the file path as an argument.
                // emplace_back is slightly more efficient than push_back as it constructs the thread in place.
                threads.emplace_back(process_and_print_hash, entry.path());
            }
        }
        
        std::cout << "Launched " << threads.size() << " threads to process files." << std::endl;

        // --- 4. Wait for all threads to complete ---
        // We must join() every thread we create.
        for (auto& t : threads) {
            t.join();
        }

    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "All files processed." << std::endl;
    return 0;
}