A C++ command-line tool that takes a directory path and calculates the SHA256 hash for every file inside it, using a pool of worker threads.

```bash
# Create a separate directory for build files
mkdir build
cd build

# Configure the project with CMake
cmake ..

# Compile the project (on Linux/macOS)
make

./file_hasher DIRECTORY_PATH
```