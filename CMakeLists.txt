cmake_minimum_required(VERSION 3.22)

project(SoftLoafTrichrome
    VERSION 0.1.0
    DESCRIPTION "Standalone SoftLoaf trichrome desktop app and core"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SOFTLOAF_TRICHROME_BUILD_TOOLS "Build command line tools" ON)
option(SOFTLOAF_TRICHROME_BUILD_TESTS "Build smoke tests" ON)
option(SOFTLOAF_TRICHROME_BUILD_APP "Build Qt desktop app" ON)

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBRAW REQUIRED IMPORTED_TARGET libraw)

add_library(softloaf_trichrome INTERFACE)
target_include_directories(softloaf_trichrome INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(softloaf_trichrome INTERFACE ${OpenCV_LIBS})

if(SOFTLOAF_TRICHROME_BUILD_APP)
    find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2 Widgets)
    if(APPLE)
        enable_language(OBJCXX)
    endif()
    qt_standard_project_setup(REQUIRES 6.6)
    qt_policy(SET QTP0004 NEW)

    if(APPLE)
        set(SOFTLOAF_TRICHROME_NATIVE_OPEN_PANEL
            src/native_open_panel.mm
            src/native_open_panel.hpp)
    else()
        set(SOFTLOAF_TRICHROME_NATIVE_OPEN_PANEL
            src/native_open_panel_stub.cpp
            src/native_open_panel.hpp)
    endif()

    qt_add_executable(softloaf_trichrome_app
        src/main.cpp
        src/desktop_decoder.cpp
        src/desktop_decoder.hpp
        ${SOFTLOAF_TRICHROME_NATIVE_OPEN_PANEL}
        src/obs_log.cpp
        src/obs_log.hpp
        src/trichrome_cache.cpp
        src/trichrome_cache.hpp
        src/trichrome_controller.cpp
        src/trichrome_controller.hpp
        src/trichrome_image_provider.cpp
        src/trichrome_image_provider.hpp)

    set_target_properties(softloaf_trichrome_app PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "SoftLoaf Trichrome"
        MACOSX_BUNDLE_GUI_IDENTIFIER "art.revela.softloaf-trichrome")

    if(APPLE)
        set_source_files_properties(src/native_open_panel.mm
            PROPERTIES LANGUAGE OBJCXX COMPILE_FLAGS "-fobjc-arc")
    endif()

    qt_add_qml_module(softloaf_trichrome_app
        URI SoftLoafTrichrome
        VERSION 1.0
        QML_FILES
            qml/ExportWindow.qml
            qml/Main.qml)

    target_link_libraries(softloaf_trichrome_app PRIVATE
        softloaf_trichrome
        PkgConfig::LIBRAW
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::Widgets)
endif()

if(SOFTLOAF_TRICHROME_BUILD_TOOLS)
    add_executable(softloaf_trichrome_check tools/softloaf_trichrome_check.cpp)
    target_link_libraries(softloaf_trichrome_check PRIVATE softloaf_trichrome)
endif()

if(SOFTLOAF_TRICHROME_BUILD_TESTS)
    enable_testing()
    add_executable(trichrome_smoke tests/trichrome_smoke.cpp)
    target_link_libraries(trichrome_smoke PRIVATE softloaf_trichrome)
    add_test(NAME trichrome_smoke COMMAND trichrome_smoke)

    if(SOFTLOAF_TRICHROME_BUILD_APP)
        add_executable(trichrome_desktop_core_test
            tests/trichrome_desktop_core_test.cpp
            src/obs_log.cpp
            src/trichrome_cache.cpp)
        target_include_directories(trichrome_desktop_core_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(trichrome_desktop_core_test PRIVATE
            softloaf_trichrome
            Qt6::Core
            Qt6::Gui)
        add_test(NAME trichrome_desktop_core COMMAND trichrome_desktop_core_test)
    endif()
endif()
