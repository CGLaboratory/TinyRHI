cmake_minimum_required(VERSION 3.20)

project(TinyRHI
    VERSION 0.1.0
    LANGUAGES C CXX
)

option(TINYRHI_BUILD_EXAMPLES "Build TinyRHI examples" ON)
option(TINYRHI_BUILD_TESTS "Build TinyRHI tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(third_party/glad)
add_subdirectory(TinyRHI)

if(TINYRHI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(TINYRHI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
