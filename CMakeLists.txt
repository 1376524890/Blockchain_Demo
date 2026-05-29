cmake_minimum_required(VERSION 3.16)
project(rbft_chain_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED libsodium)
pkg_check_modules(SQLITE3 REQUIRED sqlite3)

FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(httplib
  URL https://github.com/yhirose/cpp-httplib/archive/refs/tags/v0.15.3.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(json httplib googletest)

file(GLOB_RECURSE RBFT_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER RBFT_SOURCES EXCLUDE REGEX ".*/main\\.cpp$")
list(FILTER RBFT_SOURCES EXCLUDE REGEX ".*/main_cli\\.cpp$")

add_library(rbft_core ${RBFT_SOURCES})
target_include_directories(rbft_core PUBLIC include ${SODIUM_INCLUDE_DIRS} ${SQLITE3_INCLUDE_DIRS})
target_link_directories(rbft_core PUBLIC ${SODIUM_LIBRARY_DIRS} ${SQLITE3_LIBRARY_DIRS})
target_link_libraries(rbft_core PUBLIC nlohmann_json::nlohmann_json httplib::httplib ${SODIUM_LIBRARIES} ${SQLITE3_LIBRARIES})
target_compile_options(rbft_core PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic>)

add_executable(rbft_node src/main.cpp)
target_link_libraries(rbft_node PRIVATE rbft_core)

add_executable(rbft_cli src/cli/main_cli.cpp)
target_link_libraries(rbft_cli PRIVATE rbft_core)

enable_testing()
file(GLOB UNIT_TESTS CONFIGURE_DEPENDS tests/unit/*.cpp)
add_executable(rbft_unit_tests ${UNIT_TESTS})
target_link_libraries(rbft_unit_tests PRIVATE rbft_core GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(rbft_unit_tests)
