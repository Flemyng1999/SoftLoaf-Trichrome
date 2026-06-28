cmake_minimum_required(VERSION 3.22)

project(SoftLoafTrichrome
    VERSION 0.1.0
    DESCRIPTION "Standalone SoftLoaf trichrome import and artifact core"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SOFTLOAF_TRICHROME_BUILD_TOOLS "Build command line tools" ON)
option(SOFTLOAF_TRICHROME_BUILD_TESTS "Build smoke tests" ON)

find_package(OpenCV REQUIRED COMPONENTS core imgproc)

add_library(softloaf_trichrome INTERFACE)
target_include_directories(softloaf_trichrome INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(softloaf_trichrome INTERFACE ${OpenCV_LIBS})

if(SOFTLOAF_TRICHROME_BUILD_TOOLS)
    add_executable(softloaf_trichrome_check tools/softloaf_trichrome_check.cpp)
    target_link_libraries(softloaf_trichrome_check PRIVATE softloaf_trichrome)
endif()

if(SOFTLOAF_TRICHROME_BUILD_TESTS)
    enable_testing()
    add_executable(trichrome_smoke tests/trichrome_smoke.cpp)
    target_link_libraries(trichrome_smoke PRIVATE softloaf_trichrome)
    add_test(NAME trichrome_smoke COMMAND trichrome_smoke)
endif()
