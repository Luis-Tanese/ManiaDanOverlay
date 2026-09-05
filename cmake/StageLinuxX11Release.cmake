cmake_minimum_required(VERSION 3.20)

if (NOT DEFINED INPUT_BINARY OR INPUT_BINARY STREQUAL "")
    message(FATAL_ERROR "StageLinuxX11Release: INPUT_BINARY was not provided")
endif()

if (NOT EXISTS "${INPUT_BINARY}")
    message(FATAL_ERROR
        "StageLinuxX11Release: built executable does not exist: ${INPUT_BINARY}"
    )
endif()

if (NOT DEFINED OUTPUT_BINARY OR OUTPUT_BINARY STREQUAL "")
    message(FATAL_ERROR "StageLinuxX11Release: OUTPUT_BINARY was not provided")
endif()

if (NOT BUILD_CONFIG STREQUAL "Release")
    message(FATAL_ERROR
        "linux-x11-bin must be built with CMAKE_BUILD_TYPE=Release (got '${BUILD_CONFIG}')"
    )
endif()

get_filename_component(OUTPUT_DIR "${OUTPUT_BINARY}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(REMOVE "${OUTPUT_BINARY}")

file(
    COPY_FILE
    "${INPUT_BINARY}"
    "${OUTPUT_BINARY}"
)

file(
    CHMOD "${OUTPUT_BINARY}"
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

if (DEFINED STRIP_TOOL AND NOT STRIP_TOOL STREQUAL "" AND EXISTS "${STRIP_TOOL}")
    execute_process(
        COMMAND "${STRIP_TOOL}" --strip-unneeded "${OUTPUT_BINARY}"
        RESULT_VARIABLE STRIP_RESULT
        OUTPUT_VARIABLE STRIP_STDOUT
        ERROR_VARIABLE STRIP_STDERR
    )

    if (NOT STRIP_RESULT EQUAL 0)
        message(FATAL_ERROR
            "Failed to strip release executable with '${STRIP_TOOL}':\n${STRIP_STDERR}"
        )
    endif()
else()
    message(WARNING "No strip tool found; release executable will remain unstripped")
endif()

if (IS_DIRECTORY "${OUTPUT_BINARY}")
    message(FATAL_ERROR "Release output is unexpectedly a directory: ${OUTPUT_BINARY}")
endif()

find_program(LDD_TOOL NAMES ldd)

if (NOT LDD_TOOL)
    message(FATAL_ERROR "Could not find ldd; cannot verify Linux release dependencies")
endif()

execute_process(
    COMMAND "${LDD_TOOL}" "${OUTPUT_BINARY}"
    RESULT_VARIABLE LDD_RESULT
    OUTPUT_VARIABLE LDD_STDOUT
    ERROR_VARIABLE LDD_STDERR
)

if (NOT LDD_RESULT EQUAL 0)
    message(FATAL_ERROR
        "ldd failed for ${OUTPUT_BINARY}:\n${LDD_STDOUT}\n${LDD_STDERR}"
    )
endif()

string(TOLOWER "${LDD_STDOUT}\n${LDD_STDERR}" LDD_LOWER)

foreach(FORBIDDEN_DEP
    "libcurl"
    "libwayland"
    "libglfw"
    "libraylib"
    "libyyjson"
    "libminacalc"
)
    string(FIND "${LDD_LOWER}" "${FORBIDDEN_DEP}" FORBIDDEN_POS)
    if (NOT FORBIDDEN_POS EQUAL -1)
        message(FATAL_ERROR
            "Forbidden runtime dependency '${FORBIDDEN_DEP}' found:\n${LDD_STDOUT}"
        )
    endif()
endforeach()

string(FIND "${LDD_LOWER}" "libx11" X11_POS)
if (X11_POS EQUAL -1)
    message(FATAL_ERROR
        "libX11 was not found in the release dependencies; X11-only verification failed:\n${LDD_STDOUT}"
    )
endif()

if (
    REQUIRE_STATIC_CPP_RUNTIME
    AND CXX_COMPILER_ID STREQUAL "GNU"
)
    string(FIND "${LDD_LOWER}" "libstdc++" LIBSTDCXX_POS)
    if (NOT LIBSTDCXX_POS EQUAL -1)
        message(FATAL_ERROR
            "libstdc++ is still dynamic even though static GNU runtime was requested:\n${LDD_STDOUT}"
        )
    endif()

    string(FIND "${LDD_LOWER}" "libgcc_s" LIBGCC_POS)
    if (NOT LIBGCC_POS EQUAL -1)
        message(FATAL_ERROR
            "libgcc_s is still dynamic even though static GNU runtime was requested:\n${LDD_STDOUT}"
        )
    endif()
endif()

file(SIZE "${OUTPUT_BINARY}" OUTPUT_SIZE_BYTES)
math(EXPR OUTPUT_SIZE_KIB "${OUTPUT_SIZE_BYTES} / 1024")

message(STATUS "")
message(STATUS "ManiaDanOverlay Linux X11 release ready")
message(STATUS "  file: ${OUTPUT_BINARY}")
message(STATUS "  size: ${OUTPUT_SIZE_KIB} KiB")
message(STATUS "  backend: X11 only")
message(STATUS "  libcurl: absent")
message(STATUS "  Wayland: absent")
message(STATUS "  external GLFW: absent")
message(STATUS "")
message(STATUS "Dynamic Linux system libraries:")
message(STATUS "${LDD_STDOUT}")
