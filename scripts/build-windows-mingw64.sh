#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/Windows"
OUTPUT_BINARY="${ROOT}/bin/ManiaDanOverlay-Windows.exe"
TOOLCHAIN="${ROOT}/cmake/toolchains/mingw64.cmake"

if [[ "${1:-}" == "--clean" ]]; then
    rm -rf "${BUILD_DIR}"
    rm -f "${OUTPUT_BINARY}"
fi

required=(
    x86_64-w64-mingw32-gcc
    x86_64-w64-mingw32-g++
    x86_64-w64-mingw32-windres
    x86_64-w64-mingw32-strip
    x86_64-w64-mingw32-objdump
    cmake
)

missing=()
for tool in "${required[@]}"; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        missing+=("${tool}")
    fi
done

if (( ${#missing[@]} > 0 )); then
    printf 'Missing Windows cross-build tools:\n' >&2
    printf '  %s\n' "${missing[@]}" >&2
    printf '\nOn Linux Mint install them with:\n  sudo apt install mingw-w64\n' >&2
    exit 2
fi

cmake_args=(
    -S "${ROOT}"
    -B "${BUILD_DIR}"
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}"
    -DCMAKE_BUILD_TYPE=Release
    -DMANIADANOVERLAY_WINDOWS_GUI_RELEASE=ON
    -DMANIADANOVERLAY_WINDOWS_STATIC_GNU_RUNTIME=ON
    -DMANIADANOVERLAY_ENABLE_LTO=ON
)

if command -v ninja >/dev/null 2>&1; then
    cmake_args+=( -G Ninja )
fi

cmake "${cmake_args[@]}"
cmake --build "${BUILD_DIR}" --target windows-bin --parallel

printf '\nWindows release:\n  %s\n' "${OUTPUT_BINARY}"
