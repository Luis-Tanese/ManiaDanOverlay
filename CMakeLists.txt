cmake_minimum_required(VERSION 3.20)

project(ManiaDanOverlay
    VERSION 0.2.0
    LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(
    MANIADANOVERLAY_LINUX_X11_ONLY
    "Build the Linux desktop target with X11 enabled and Wayland disabled"
    ON
)

option(
    MANIADANOVERLAY_ENABLE_LTO
    "Use link-time optimization for Release builds when the toolchain supports it"
    ON
)

option(
    MANIADANOVERLAY_STATIC_GNU_RUNTIME
    "Statically link libgcc/libstdc++ into Linux Release builds when using GCC"
    ON
)

option(
    MANIADANOVERLAY_WINDOWS_STATIC_GNU_RUNTIME
    "Statically link MinGW GCC/C++ support runtimes into the Windows Release executable"
    ON
)

option(
    MANIADANOVERLAY_WINDOWS_GUI_RELEASE
    "Build Windows Release without a console window"
    ON
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build third-party libraries statically" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Do not build raylib examples" FORCE)
set(YYJSON_BUILD_TESTS OFF CACHE BOOL "Do not build yyjson tests" FORCE)
set(YYJSON_BUILD_MISC OFF CACHE BOOL "Do not build yyjson misc tools" FORCE)
set(YYJSON_INSTALL OFF CACHE BOOL "Do not generate yyjson install targets" FORCE)

set(USE_EXTERNAL_GLFW OFF CACHE STRING "Use bundled GLFW" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Do not build GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Do not build GLFW tests" FORCE)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Do not build GLFW docs" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Do not install GLFW" FORCE)

if (UNIX AND NOT APPLE AND MANIADANOVERLAY_LINUX_X11_ONLY)
    set(PLATFORM Desktop CACHE STRING "raylib platform" FORCE)
    set(OPENGL_VERSION "3.3" CACHE STRING "raylib OpenGL version" FORCE)
    set(GLFW_BUILD_X11 ON CACHE BOOL "Build bundled GLFW with X11" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "Do not build Wayland support" FORCE)
    message(STATUS "ManiaDanOverlay Linux backend: X11 only (Wayland disabled)")
endif()

add_subdirectory(vendor/raylib)
add_subdirectory(vendor/yyjson)

set(MINACALC_ROOT
    ${CMAKE_SOURCE_DIR}/vendor/minacalc
)

if (NOT EXISTS "${MINACALC_ROOT}/API.cpp")
    message(FATAL_ERROR
        "MinaCalc is missing. Run: git submodule update --init --recursive"
    )
endif()

add_library(
    minacalc
    STATIC

    ${MINACALC_ROOT}/MinaCalc/MinaCalc.cpp
    ${MINACALC_ROOT}/API.cpp
)

target_include_directories(
    minacalc
    PUBLIC

    ${MINACALC_ROOT}
    ${MINACALC_ROOT}/MinaCalc
)

target_compile_features(
    minacalc
    PUBLIC
    cxx_std_17
)

if (MSVC)
    target_compile_options(
        minacalc
        PRIVATE
        $<$<CONFIG:Debug>:/O2>
    )
else()
    target_compile_options(
        minacalc
        PRIVATE
        $<$<CONFIG:Debug>:-O2>
    )
endif()

set(MANIADANOVERLAY_FONT_REGULAR
    ${CMAKE_SOURCE_DIR}/assets/fonts/Torus-Regular.otf
)

set(MANIADANOVERLAY_FONT_BOLD
    ${CMAKE_SOURCE_DIR}/assets/fonts/Torus-Bold.ttf
)

if (NOT EXISTS "${MANIADANOVERLAY_FONT_REGULAR}")
    message(FATAL_ERROR
        "Missing Torus-Regular.otf at assets/fonts/Torus-Regular.otf"
    )
endif()

if (NOT EXISTS "${MANIADANOVERLAY_FONT_BOLD}")
    message(FATAL_ERROR
        "Missing Torus-Bold.ttf at assets/fonts/Torus-Bold.ttf"
    )
endif()

set(MANIADANOVERLAY_GENERATED_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/generated
)

set(MANIADANOVERLAY_EMBEDDED_FONTS_C
    ${MANIADANOVERLAY_GENERATED_DIR}/embedded_fonts.c
)

set(MANIADANOVERLAY_EMBEDDED_FONTS_H
    ${MANIADANOVERLAY_GENERATED_DIR}/embedded_fonts.h
)

add_custom_command(
    OUTPUT
        ${MANIADANOVERLAY_EMBEDDED_FONTS_C}
        ${MANIADANOVERLAY_EMBEDDED_FONTS_H}

    COMMAND
        ${CMAKE_COMMAND}
        -DREGULAR_FONT=${MANIADANOVERLAY_FONT_REGULAR}
        -DBOLD_FONT=${MANIADANOVERLAY_FONT_BOLD}
        -DOUTPUT_C=${MANIADANOVERLAY_EMBEDDED_FONTS_C}
        -DOUTPUT_H=${MANIADANOVERLAY_EMBEDDED_FONTS_H}
        -P ${CMAKE_SOURCE_DIR}/cmake/EmbedFonts.cmake

    DEPENDS
        ${MANIADANOVERLAY_FONT_REGULAR}
        ${MANIADANOVERLAY_FONT_BOLD}
        ${CMAKE_SOURCE_DIR}/cmake/EmbedFonts.cmake

    COMMENT "Embedding Torus fonts into ManiaDanOverlay"
    VERBATIM
)

set(MANIADANOVERLAY_PLATFORM_SOURCES "")

if (WIN32)
    file(MAKE_DIRECTORY "${MANIADANOVERLAY_GENERATED_DIR}")

    set(MANIADANOVERLAY_WINDOWS_MANIFEST
        ${CMAKE_SOURCE_DIR}/src/platform/windows/ManiaDanOverlay.manifest
    )

    if (NOT EXISTS "${MANIADANOVERLAY_WINDOWS_MANIFEST}")
        message(FATAL_ERROR "Missing Windows manifest: ${MANIADANOVERLAY_WINDOWS_MANIFEST}")
    endif()

    set(MANIADANOVERLAY_WINDOWS_RC
        ${MANIADANOVERLAY_GENERATED_DIR}/ManiaDanOverlay.rc
    )

    set(MANIADANOVERLAY_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
    set(MANIADANOVERLAY_VERSION_MINOR ${PROJECT_VERSION_MINOR})
    set(MANIADANOVERLAY_VERSION_PATCH ${PROJECT_VERSION_PATCH})

    configure_file(
        ${CMAKE_SOURCE_DIR}/src/platform/windows/ManiaDanOverlay.rc.in
        ${MANIADANOVERLAY_WINDOWS_RC}
        @ONLY
    )

    list(APPEND MANIADANOVERLAY_PLATFORM_SOURCES
        src/platform/windows/windows_entry.c
        ${MANIADANOVERLAY_WINDOWS_RC}
    )
endif()

add_executable(
    ManiaDanOverlay

    ${MANIADANOVERLAY_EMBEDDED_FONTS_C}
    ${MANIADANOVERLAY_PLATFORM_SOURCES}

    src/main.c

    src/platform/platform_paths.c
    src/settings/app_settings.c
    src/net/local_http.c

    src/ui/app_view.c
    src/ui/settings_panel.c
    src/ui/font.c
    src/ui/theme.c

    src/tosu/tosu.c

    src/beatmap/beatmap.c
    src/beatmap/beatmap_fetch.c
    src/beatmap/osu_parser.c

    src/calibration/mania4k_calibration.c

    src/engine/analysis_map.c
    src/engine/chart_features.c
    src/engine/family_classifier.c
    src/engine/ln_course.c
    src/engine/ln_profile.c
    src/engine/minacalc_bridge.cpp
    src/engine/reform_rank.c
    src/engine/rhythm_profile.c
    src/engine/ruler_sanity.c
    src/engine/sunny_sr.c
)

target_include_directories(
    ManiaDanOverlay
    PRIVATE
    src
    ${MANIADANOVERLAY_GENERATED_DIR}
)

target_compile_definitions(
    ManiaDanOverlay
    PRIVATE
    MANIADANOVERLAY_VERSION="${PROJECT_VERSION}"
)

if (WIN32 AND MANIADANOVERLAY_WINDOWS_GUI_RELEASE)
    set_property(
        TARGET ManiaDanOverlay
        PROPERTY WIN32_EXECUTABLE "$<CONFIG:Release>"
    )
endif()

target_link_libraries(
    ManiaDanOverlay
    PRIVATE
    raylib
    yyjson
    minacalc
)

if (WIN32)
    target_link_libraries(
        ManiaDanOverlay
        PRIVATE
        ws2_32
    )
endif()

if (UNIX AND NOT APPLE)
    target_link_libraries(
        ManiaDanOverlay
        PRIVATE
        m
        pthread
    )
endif()

if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    foreach(_target ManiaDanOverlay minacalc raylib yyjson)
        if (TARGET ${_target})
            target_compile_options(
                ${_target}
                PRIVATE
                $<$<CONFIG:Release>:-ffunction-sections>
                $<$<CONFIG:Release>:-fdata-sections>
            )
        endif()
    endforeach()

    target_link_options(
        ManiaDanOverlay
        PRIVATE
        $<$<CONFIG:Release>:-Wl,--gc-sections>
        $<$<CONFIG:Release>:-Wl,--as-needed>
    )
endif()

if (
    UNIX AND NOT APPLE
    AND MANIADANOVERLAY_STATIC_GNU_RUNTIME
    AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
)
    target_link_options(
        ManiaDanOverlay
        PRIVATE
        $<$<CONFIG:Release>:-static-libgcc>
        $<$<CONFIG:Release>:-static-libstdc++>
    )
endif()

if (
    WIN32
    AND MINGW
    AND MANIADANOVERLAY_WINDOWS_STATIC_GNU_RUNTIME
)
    target_link_options(
        ManiaDanOverlay
        PRIVATE
        $<$<CONFIG:Release>:-static>
        $<$<CONFIG:Release>:-static-libgcc>
        $<$<CONFIG:Release>:-static-libstdc++>
    )
endif()

if (MANIADANOVERLAY_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(
        RESULT MANIADANOVERLAY_IPO_SUPPORTED
        OUTPUT MANIADANOVERLAY_IPO_ERROR
    )

    if (MANIADANOVERLAY_IPO_SUPPORTED)
        foreach(_target ManiaDanOverlay minacalc raylib yyjson)
            if (TARGET ${_target})
                set_property(
                    TARGET ${_target}
                    PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE
                )
            endif()
        endforeach()
        message(STATUS "ManiaDanOverlay Release LTO: enabled")
    else()
        message(STATUS
            "ManiaDanOverlay Release LTO: unavailable (${MANIADANOVERLAY_IPO_ERROR})"
        )
    endif()
endif()

if (UNIX AND NOT APPLE AND MANIADANOVERLAY_LINUX_X11_ONLY)
    add_custom_target(
        linux-x11-bin

        COMMAND
            ${CMAKE_COMMAND}
            "-DINPUT_BINARY=$<TARGET_FILE:ManiaDanOverlay>"
            "-DOUTPUT_BINARY=${CMAKE_SOURCE_DIR}/bin/ManiaDanOverlay-LinuxX11"
            "-DBUILD_CONFIG=$<CONFIG>"
            "-DSTRIP_TOOL=${CMAKE_STRIP}"
            "-DREQUIRE_STATIC_CPP_RUNTIME=${MANIADANOVERLAY_STATIC_GNU_RUNTIME}"
            "-DCXX_COMPILER_ID=${CMAKE_CXX_COMPILER_ID}"
            -P ${CMAKE_SOURCE_DIR}/cmake/StageLinuxX11Release.cmake

        DEPENDS
            ManiaDanOverlay

        COMMENT
            "Staging and verifying Linux X11 release binary"

        VERBATIM
    )
endif()

if (WIN32)
    add_custom_target(
        windows-bin

        COMMAND
            ${CMAKE_COMMAND}
            "-DINPUT_BINARY=$<TARGET_FILE:ManiaDanOverlay>"
            "-DOUTPUT_BINARY=${CMAKE_SOURCE_DIR}/bin/ManiaDanOverlay-Windows.exe"
            "-DBUILD_CONFIG=$<CONFIG>"
            "-DSTRIP_TOOL=${CMAKE_STRIP}"
            "-DOBJDUMP_TOOL=${CMAKE_OBJDUMP}"
            "-DREQUIRE_STATIC_GNU_RUNTIME=${MANIADANOVERLAY_WINDOWS_STATIC_GNU_RUNTIME}"
            -P ${CMAKE_SOURCE_DIR}/cmake/StageWindowsRelease.cmake

        DEPENDS
            ManiaDanOverlay

        COMMENT
            "Staging and verifying Windows release binary"

        VERBATIM
    )
endif()
