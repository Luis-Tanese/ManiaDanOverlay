#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build/LinuxX11"
OUTPUT_BINARY="${ROOT}/bin/ManiaDanOverlay-LinuxX11"

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "This release script is for Linux/X11 only." >&2
    exit 1
fi

if [[ "${1:-}" == "--clean" ]]; then
    rm -rf "${BUILD_DIR}"
    rm -f "${OUTPUT_BINARY}"
fi

if command -v nproc >/dev/null 2>&1; then
    DEFAULT_JOBS="$(nproc)"
else
    DEFAULT_JOBS="4"
fi

JOBS="${JOBS:-${DEFAULT_JOBS}}"

echo "==> Configuring ManiaDanOverlay Release (Linux X11 only)"
cmake \
    -S "${ROOT}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DMANIADANOVERLAY_LINUX_X11_ONLY=ON \
    -DMANIADANOVERLAY_ENABLE_LTO=ON \
    -DMANIADANOVERLAY_STATIC_GNU_RUNTIME=ON

echo "==> Building and staging Linux X11 binary"
cmake \
    --build "${BUILD_DIR}" \
    --parallel "${JOBS}" \
    --target linux-x11-bin

if [[ ! -x "${OUTPUT_BINARY}" ]]; then
    echo "Release verification finished but ${OUTPUT_BINARY} is missing or not executable." >&2
    exit 1
fi

echo
echo "============================================================"
echo " ManiaDanOverlay Linux X11 release is ready"
echo " ${OUTPUT_BINARY}"
echo "============================================================"
echo
file "${OUTPUT_BINARY}" || true
