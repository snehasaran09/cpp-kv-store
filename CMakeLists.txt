cmake_minimum_required(VERSION 3.16)
project(cpp-kv-store VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type")
set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")

set(STORAGE_SOURCES
    src/storage/kv_store.cpp
    src/storage/hash_map.cpp
    src/storage/lru_cache.cpp
    src/memory/memory_pool.cpp
    src/persistence/aof_log.cpp
    src/ttl/ttl_manager.cpp
)

set(SERVER_SOURCES
    src/main.cpp
    src/server/tcp_server.cpp
    src/server/epoll_loop.cpp
    ${STORAGE_SOURCES}
)

add_executable(kvstore ${SERVER_SOURCES})
target_include_directories(kvstore PRIVATE src)
target_link_libraries(kvstore pthread)

add_executable(unit_tests tests/unit_tests.cpp ${STORAGE_SOURCES})
target_include_directories(unit_tests PRIVATE src)
target_link_libraries(unit_tests pthread)

add_executable(load_test benchmarks/load_test.cpp ${STORAGE_SOURCES})
target_include_directories(load_test PRIVATE src)
target_link_libraries(load_test pthread)