# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.16)

# the project name and language
project(FileHasher CXX)

# C++17 for std::filesystem
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Creating our executable from the main.cpp source file
add_executable(file_hasher src/main.cpp)

# Telling CMake where to find our header files
target_include_directories(file_hasher PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)