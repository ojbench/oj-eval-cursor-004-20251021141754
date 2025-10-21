cmake_minimum_required(VERSION 3.16)
project(bookstore_004 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(code
  src/main.cpp
)

# Conservative compile flags suitable for OJ
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(code PRIVATE -O2 -pipe -s)
endif()

# Avoid extra runtime deps; do not link non-standard libs
