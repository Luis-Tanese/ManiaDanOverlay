if (NOT DEFINED REGULAR_FONT)
    message(FATAL_ERROR "REGULAR_FONT was not provided")
endif()

if (NOT DEFINED BOLD_FONT)
    message(FATAL_ERROR "BOLD_FONT was not provided")
endif()

if (NOT DEFINED OUTPUT_C)
    message(FATAL_ERROR "OUTPUT_C was not provided")
endif()

if (NOT DEFINED OUTPUT_H)
    message(FATAL_ERROR "OUTPUT_H was not provided")
endif()

if (NOT EXISTS "${REGULAR_FONT}")
    message(FATAL_ERROR "Regular Torus font not found: ${REGULAR_FONT}")
endif()

if (NOT EXISTS "${BOLD_FONT}")
    message(FATAL_ERROR "Bold Torus font not found: ${BOLD_FONT}")
endif()

get_filename_component(OUTPUT_C_DIR "${OUTPUT_C}" DIRECTORY)
get_filename_component(OUTPUT_H_DIR "${OUTPUT_H}" DIRECTORY)
file(MAKE_DIRECTORY "${OUTPUT_C_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_H_DIR}")

function(embed_binary INPUT_FILE SYMBOL OUTPUT_FILE)
    file(READ "${INPUT_FILE}" FILE_HEX HEX)
    string(LENGTH "${FILE_HEX}" HEX_LENGTH)

    if (HEX_LENGTH EQUAL 0)
        message(FATAL_ERROR "Cannot embed empty file: ${INPUT_FILE}")
    endif()

    math(EXPR BYTE_COUNT "${HEX_LENGTH} / 2")
    math(EXPR LAST_OFFSET "${HEX_LENGTH} - 1")

    file(APPEND "${OUTPUT_FILE}" "const unsigned char ${SYMBOL}_data[] = {\n")

    foreach(OFFSET RANGE 0 ${LAST_OFFSET} 32)
        math(EXPR REMAINING "${HEX_LENGTH} - ${OFFSET}")

        if (REMAINING GREATER 32)
            set(CHUNK_LENGTH 32)
        else()
            set(CHUNK_LENGTH ${REMAINING})
        endif()

        string(
            SUBSTRING
            "${FILE_HEX}"
            ${OFFSET}
            ${CHUNK_LENGTH}
            CHUNK
        )

        string(
            REGEX REPLACE
            "([0-9A-Fa-f][0-9A-Fa-f])"
            "0x\\1, "
            CHUNK_BYTES
            "${CHUNK}"
        )

        file(APPEND "${OUTPUT_FILE}" "    ${CHUNK_BYTES}\n")
    endforeach()

    file(APPEND "${OUTPUT_FILE}" "};\n")
    file(APPEND "${OUTPUT_FILE}" "const size_t ${SYMBOL}_size = ${BYTE_COUNT}u;\n\n")
endfunction()

file(
    WRITE
    "${OUTPUT_H}"
    "#ifndef MANIADANOVERLAY_EMBEDDED_FONTS_H\n"
    "#define MANIADANOVERLAY_EMBEDDED_FONTS_H\n\n"
    "#include <stddef.h>\n\n"
    "extern const unsigned char g_torus_regular_data[];\n"
    "extern const size_t g_torus_regular_size;\n\n"
    "extern const unsigned char g_torus_bold_data[];\n"
    "extern const size_t g_torus_bold_size;\n\n"
    "#endif\n"
)

file(
    WRITE
    "${OUTPUT_C}"
    "#include <stddef.h>\n"
    "#include \"embedded_fonts.h\"\n\n"
)

embed_binary(
    "${REGULAR_FONT}"
    "g_torus_regular"
    "${OUTPUT_C}"
)

embed_binary(
    "${BOLD_FONT}"
    "g_torus_bold"
    "${OUTPUT_C}"
)
