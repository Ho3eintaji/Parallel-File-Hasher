#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem> 
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

    // --- 2. Directory Iteration and Hashing ---
    try {
        // Use a directory_iterator to loop through all entries in the given path
        for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
            // Check if the entry is a regular file (i.e., not a directory, symlink, etc.)
            if (entry.is_regular_file()) {
                std::filesystem::path file_path = entry.path();
                std::string hash = get_file_hash(file_path);

                // Print the result in a clean format
                std::cout << file_path.filename().string() << ": " << hash << std::endl;
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        std::cerr << "Filesystem error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}