cmake_minimum_required(VERSION 3.22)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0" CACHE STRING
        "Minimum macOS version supported by SoftLoaf Trichrome")
endif()

project(SoftLoafTrichrome
    VERSION 0.2.0
    DESCRIPTION "Standalone SoftLoaf trichrome desktop app and core"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SOFTLOAF_TRICHROME_BUILD_TOOLS "Build command line tools" ON)
option(SOFTLOAF_TRICHROME_BUILD_TESTS "Build smoke tests" ON)
option(SOFTLOAF_TRICHROME_BUILD_APP "Build Qt desktop app" ON)

if(WIN32)
    add_compile_definitions(
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00
        NTDDI_VERSION=0x0A000000)
endif()

find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs)
find_package(lcms2 CONFIG QUIET)
if(TARGET lcms2::lcms2)
    set(SOFTLOAF_LCMS_TARGET lcms2::lcms2)
elseif(TARGET lcms2)
    set(SOFTLOAF_LCMS_TARGET lcms2)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LCMS2 REQUIRED IMPORTED_TARGET lcms2)
    set(SOFTLOAF_LCMS_TARGET PkgConfig::LCMS2)
endif()

find_package(libraw CONFIG QUIET)
if(TARGET libraw::raw)
    set(SOFTLOAF_LIBRAW_TARGET libraw::raw)
elseif(TARGET libraw::raw_r)
    set(SOFTLOAF_LIBRAW_TARGET libraw::raw_r)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBRAW REQUIRED IMPORTED_TARGET libraw)
    set(SOFTLOAF_LIBRAW_TARGET PkgConfig::LIBRAW)
endif()

add_library(softloaf_trichrome INTERFACE)
target_include_directories(softloaf_trichrome INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(softloaf_trichrome INTERFACE ${OpenCV_LIBS} ${SOFTLOAF_LCMS_TARGET})

add_library(softloaf_trichrome_color STATIC
    src/color_management.cpp)
target_include_directories(softloaf_trichrome_color PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)
target_link_libraries(softloaf_trichrome_color PUBLIC
    ${SOFTLOAF_LCMS_TARGET})

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
        set(SOFTLOAF_TRICHROME_PLATFORM_RESOURCES
            resources/AppIcon.icns)
    else()
        set(SOFTLOAF_TRICHROME_NATIVE_OPEN_PANEL
            src/native_open_panel_stub.cpp
            src/native_open_panel.hpp)
        set(SOFTLOAF_TRICHROME_PLATFORM_RESOURCES)
        if(WIN32)
            set(SOFTLOAF_TRICHROME_PLATFORM_RESOURCES
                resources/windows_app_icon.rc)
        endif()
    endif()

    qt_add_executable(softloaf_trichrome_app
        src/main.cpp
        src/desktop_decoder.cpp
        src/desktop_decoder.hpp
        src/export_encode_backend.cpp
        src/export_encode_backend.hpp
        ${SOFTLOAF_TRICHROME_NATIVE_OPEN_PANEL}
        src/obs_log.cpp
        src/obs_log.hpp
        src/trichrome_cache.cpp
        src/trichrome_cache.hpp
        src/trichrome_controller.cpp
        src/trichrome_controller.hpp
        src/trichrome_image_provider.cpp
        src/trichrome_image_provider.hpp
        ${SOFTLOAF_TRICHROME_PLATFORM_RESOURCES})

    if(APPLE)
        set_source_files_properties(resources/AppIcon.icns
            PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    endif()

    set_target_properties(softloaf_trichrome_app PROPERTIES
        MACOSX_BUNDLE TRUE
        WIN32_EXECUTABLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "SoftLoaf Trichrome"
        MACOSX_BUNDLE_GUI_IDENTIFIER "art.revela.softloaf-trichrome"
        MACOSX_BUNDLE_ICON_FILE "AppIcon.icns"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")

    if(APPLE)
        set_source_files_properties(src/native_open_panel.mm
            PROPERTIES LANGUAGE OBJCXX COMPILE_FLAGS "-fobjc-arc")
        target_sources(softloaf_trichrome_app PRIVATE
            src/export_encode_backend_metal.mm)
        set_source_files_properties(src/export_encode_backend_metal.mm
            PROPERTIES LANGUAGE OBJCXX COMPILE_FLAGS "-fobjc-arc")
        target_link_libraries(softloaf_trichrome_app PRIVATE
            "-framework Metal"
            "-framework Foundation")
    elseif(WIN32)
        target_sources(softloaf_trichrome_app PRIVATE
            src/export_encode_backend_d3d12.cpp)
        target_link_libraries(softloaf_trichrome_app PRIVATE
            d3d12 dxgi dxguid d3dcompiler)
    endif()

    qt_add_qml_module(softloaf_trichrome_app
        URI SoftLoafTrichrome
        VERSION 1.0
        QML_FILES
            qml/ExportWindow.qml
            qml/Main.qml
            qml/ThemedButton.qml
            qml/ThemedComboBox.qml)

    target_link_libraries(softloaf_trichrome_app PRIVATE
        softloaf_trichrome
        softloaf_trichrome_color
        ${SOFTLOAF_LIBRAW_TARGET}
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

    if(SOFTLOAF_TRICHROME_BUILD_APP)
        add_executable(softloaf_raw_probe
            tools/softloaf_raw_probe.cpp
            src/desktop_decoder.cpp)
        target_include_directories(softloaf_raw_probe PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(softloaf_raw_probe PRIVATE
            softloaf_trichrome
            ${SOFTLOAF_LIBRAW_TARGET}
            Qt6::Core)

        add_executable(softloaf_raw_provenance_matrix
            tools/softloaf_raw_provenance_matrix.cpp
            src/desktop_decoder.cpp)
        target_include_directories(softloaf_raw_provenance_matrix PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(softloaf_raw_provenance_matrix PRIVATE
            softloaf_trichrome
            ${SOFTLOAF_LIBRAW_TARGET}
            Qt6::Core)
    endif()

    add_executable(softloaf_rt32_compare tools/softloaf_rt32_compare.cpp)
    target_link_libraries(softloaf_rt32_compare PRIVATE softloaf_trichrome)

    add_executable(softloaf_image_stats tools/softloaf_image_stats.cpp)
    target_link_libraries(softloaf_image_stats PRIVATE softloaf_trichrome)
endif()

if(SOFTLOAF_TRICHROME_BUILD_TESTS)
    enable_testing()
    add_executable(trichrome_smoke tests/trichrome_smoke.cpp)
    target_link_libraries(trichrome_smoke PRIVATE softloaf_trichrome)
    add_test(NAME trichrome_smoke COMMAND trichrome_smoke)

    if(SOFTLOAF_TRICHROME_BUILD_APP)
        add_executable(trichrome_desktop_core_test
            tests/trichrome_desktop_core_test.cpp
            src/desktop_decoder.cpp
            src/obs_log.cpp
            src/trichrome_cache.cpp)
        target_include_directories(trichrome_desktop_core_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_link_libraries(trichrome_desktop_core_test PRIVATE
            softloaf_trichrome
            softloaf_trichrome_color
            ${SOFTLOAF_LIBRAW_TARGET}
            Qt6::Core
            Qt6::Gui)
        add_test(NAME trichrome_desktop_core COMMAND trichrome_desktop_core_test)

        add_executable(raw_fixture_regression_test
            tests/raw_fixture_regression_test.cpp
            src/desktop_decoder.cpp)
        target_include_directories(raw_fixture_regression_test PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src)
        target_compile_definitions(raw_fixture_regression_test PRIVATE
            SOFTLOAF_TRICHROME_RAW_FIXTURE_MANIFEST="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/raw_compat_manifest.tsv")
        target_link_libraries(raw_fixture_regression_test PRIVATE
            softloaf_trichrome
            ${SOFTLOAF_LIBRAW_TARGET}
            Qt6::Core)
        add_test(NAME raw_fixture_regression COMMAND raw_fixture_regression_test)
    endif()
endif()
