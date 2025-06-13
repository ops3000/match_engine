cmake_minimum_required(VERSION 3.14)
project(exchange_engine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# --- Dependencies ---
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

# ✅ 正确声明 rdkafka++
pkg_check_modules(RDKAFKA REQUIRED rdkafka++)

include(FetchContent)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.11.0
)
FetchContent_MakeAvailable(spdlog)

FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/release-1.12.1.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# --- INTERFACE: rdkafka wrapper ---
add_library(rdkafka_interface INTERFACE)
target_include_directories(rdkafka_interface INTERFACE ${RDKAFKA_INCLUDE_DIRS})
target_link_libraries(rdkafka_interface INTERFACE ${RDKAFKA_LIBRARIES})

# --- trader_lib ---
file(GLOB_RECURSE LIB_SOURCE_FILES
    "src/event_handler/event.cpp"
    "src/matching/market/concurrent_market.cpp"
    "src/matching/market/market.cpp"
    "src/matching/orderbook/level.cpp"
    "src/matching/orderbook/map_orderbook.cpp"
    "src/matching/orderbook/order.cpp"
    "src/matching/orderbook/symbol.cpp"
    "src/utils/log.cpp"
)
add_library(trader_lib ${LIB_SOURCE_FILES})
target_include_directories(trader_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(trader_lib
    PUBLIC
        spdlog::spdlog
    PRIVATE
        Threads::Threads
)

# --- kafka_handler_lib ---
add_library(kafka_handler_lib src/kafka_event_handler.cpp)
target_include_directories(kafka_handler_lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${json_SOURCE_DIR}/include)
target_link_libraries(kafka_handler_lib
    PRIVATE
        trader_lib
        rdkafka_interface
)

# --- exchange_main ---
add_executable(exchange_main src/main.cpp)
target_include_directories(exchange_main PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include ${json_SOURCE_DIR}/include)
target_link_libraries(exchange_main
    PRIVATE
        kafka_handler_lib
        spdlog::spdlog
        rdkafka_interface
)

# --- trader_tests ---
add_executable(trader_tests test/main_test.cpp)
target_include_directories(trader_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(trader_tests
    PRIVATE
        trader_lib
        gtest_main
)

enable_testing()
include(GoogleTest)
gtest_discover_tests(trader_tests)
