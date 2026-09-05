if (NOT DEFINED INPUT_BINARY OR INPUT_BINARY STREQUAL "")
    message(FATAL_ERROR "INPUT_BINARY is required")
endif()

if (NOT EXISTS "${INPUT_BINARY}")
    message(FATAL_ERROR "Windows executable does not exist: ${INPUT_BINARY}")
endif()

if (NOT DEFINED OUTPUT_BINARY OR OUTPUT_BINARY STREQUAL "")
    message(FATAL_ERROR "OUTPUT_BINARY is required")
endif()

if (DEFINED BUILD_CONFIG AND NOT BUILD_CONFIG STREQUAL "" AND NOT BUILD_CONFIG STREQUAL "Release")
    message(FATAL_ERROR "windows-bin must be staged from Release, got '${BUILD_CONFIG}'")
endif()

get_filename_component(OUTPUT_DIR "${OUTPUT_BINARY}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
file(REMOVE "${OUTPUT_BINARY}")
file(COPY_FILE "${INPUT_BINARY}" "${OUTPUT_BINARY}")

if (DEFINED STRIP_TOOL AND NOT STRIP_TOOL STREQUAL "")
    execute_process(
        COMMAND "${STRIP_TOOL}" --strip-unneeded "${OUTPUT_BINARY}"
        RESULT_VARIABLE STRIP_RESULT
        ERROR_VARIABLE STRIP_ERROR
    )

    if (NOT STRIP_RESULT EQUAL 0)
        message(FATAL_ERROR "Could not strip Windows release: ${STRIP_ERROR}")
    endif()
endif()

if (NOT DEFINED OBJDUMP_TOOL OR OBJDUMP_TOOL STREQUAL "")
    message(FATAL_ERROR "CMAKE_OBJDUMP/OBJDUMP_TOOL is required to verify the Windows release")
endif()

execute_process(
    COMMAND "${OBJDUMP_TOOL}" -p "${OUTPUT_BINARY}"
    RESULT_VARIABLE OBJDUMP_RESULT
    OUTPUT_VARIABLE PE_INFO
    ERROR_VARIABLE OBJDUMP_ERROR
)

if (NOT OBJDUMP_RESULT EQUAL 0)
    message(FATAL_ERROR "Could not inspect Windows executable: ${OBJDUMP_ERROR}")
endif()

if (NOT PE_INFO MATCHES "Subsystem[ \t]+00000002")
    message(FATAL_ERROR "Windows release is not marked as IMAGE_SUBSYSTEM_WINDOWS_GUI")
endif()

string(TOLOWER "${PE_INFO}" PE_INFO_LOWER)

set(FORBIDDEN_DLLS
    "libcurl"
    "libgcc_s"
    "libstdc++"
    "libwinpthread"
    "glfw3.dll"
    "raylib.dll"
    "yyjson.dll"
    "minacalc.dll"
)

foreach(FORBIDDEN IN LISTS FORBIDDEN_DLLS)
    string(FIND "${PE_INFO_LOWER}" "${FORBIDDEN}" FORBIDDEN_AT)
    if (NOT FORBIDDEN_AT EQUAL -1)
        message(FATAL_ERROR "Forbidden runtime dependency found in Windows release: ${FORBIDDEN}")
    endif()
endforeach()

file(SIZE "${OUTPUT_BINARY}" OUTPUT_SIZE)
message(STATUS "Windows one-file release ready: ${OUTPUT_BINARY} (${OUTPUT_SIZE} bytes)")

string(REGEX MATCHALL "DLL Name: [^\r\n]+" IMPORT_LINES "${PE_INFO}")
if (IMPORT_LINES)
    message(STATUS "Windows system/runtime imports:")
    foreach(LINE IN LISTS IMPORT_LINES)
        string(STRIP "${LINE}" LINE)
        message(STATUS "  ${LINE}")
    endforeach()
endif()
