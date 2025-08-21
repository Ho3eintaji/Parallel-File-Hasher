# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.16)

# the project name and language
project(FileHasher CXX)

# C++17 for std::filesystem
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create our executable from main.cpp and ThreadPool.cpp
add_executable(file_hasher 
    src/main.cpp 
    src/ThreadPool.cpp
)

# Telling CMake where to find our header files
target_include_directories(file_hasher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)