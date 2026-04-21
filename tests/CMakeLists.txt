cmake_minimum_required(VERSION 3.16)
project(ungula_event_brokers_tests)

set(CMAKE_CXX_STANDARD 17)

set(FETCHCONTENT_BASE_DIR ${CMAKE_CURRENT_LIST_DIR}/vendor)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

set(LIB_SRC ${CMAKE_CURRENT_LIST_DIR}/../src)

include_directories(
    ${LIB_SRC}
)

add_executable(test_event_brokers
    test_notifier.cpp
)
target_link_libraries(test_event_brokers gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(test_event_brokers)
