cmake_minimum_required(VERSION 3.20)
project(vectordb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(vectordb
    src/vector_store.cpp
)
target_include_directories(vectordb
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

enable_testing()

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(vector_store_test
    tests/vector_store_test.cpp
)
target_link_libraries(vector_store_test
    PRIVATE
        vectordb
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(vector_store_test)

add_executable(vectordb_cli
    tools/vectordb_cli.cpp
)
target_link_libraries(vectordb_cli PRIVATE vectordb)
