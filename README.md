# Parallel File Hasher 📂⚙️
A command-line utility written in modern C++ to calculate the SHA256 hash of all files within a specified directory, using multithreading to accelerate the process.

## Features
- Calculates the SHA256 hash for any given file.
- Scans a directory and processes all regular files within it.
- Uses C++17 <filesystem> for modern and cross-platform directory traversal.
- Header-only SHA256 implementation via picosha2.h.

## Requirements
To build and run this project, you will need:
- A C++ compiler that supports the C++17 standard (e.g., GCC 7+, Clang 5+, MSVC 2017+).
- CMake (version 3.16 or higher).

## How to Build
1. Clone the repository:
```bash
git clone <your-repository-url>
cd ParallelFileHasher
```
2. Create a build directory:
```bash
mkdir build
cd build
```
3. Configure the project with CMake:
```bash
cmake ..
```
4. Compile the project:
```bash
make
```
This will create an executable named file_hasher inside the build directory.

## How to Run
Run the executable from within the build directory, providing the path to the directory you want to scan as a command-line argument.
```bash
# Example usage:
./file_hasher ../path/to/your/files
```
