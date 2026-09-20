#!/usr/bin/env bash
set -euo pipefail

build_dir="${BUILD_DIR:-build}"
pico_sdk_path="${PICO_SDK_PATH:-}"
picotool_cmake_dir="${PICOTOOL_DIR:-${picotool_DIR:-}}"
cmake_bin="${CMAKE_BIN:-}"

if [[ -z "$cmake_bin" ]]; then
    for candidate in /usr/bin/cmake /usr/local/bin/cmake; do
        if [[ -x "$candidate" ]] && "$candidate" --version >/dev/null 2>&1; then
            cmake_bin="$candidate"
            break
        fi
    done
fi

if [[ -z "$cmake_bin" ]]; then
    cmake_bin="$(command -v cmake 2>/dev/null || true)"
fi

if [[ -z "$cmake_bin" ]] || ! "$cmake_bin" --version >/dev/null 2>&1; then
    printf 'CMake is required to build this firmware. Install it first, for example:\n' >&2
    printf '  sudo apt install cmake\n' >&2
    exit 2
fi

if [[ -z "$pico_sdk_path" ]]; then
    printf 'PICO_SDK_PATH is not set. Set it to your Raspberry Pi Pico SDK directory.\n' >&2
    exit 2
fi

cmake_args=(-DPICO_SDK_PATH="$pico_sdk_path")
if [[ -n "$picotool_cmake_dir" ]]; then
    cmake_args+=(-Dpicotool_DIR="$picotool_cmake_dir")
fi

"$cmake_bin" -S . -B "$build_dir" "${cmake_args[@]}"
"$cmake_bin" --build "$build_dir" --parallel

printf 'Firmware: %s/rp2040_zero_hid_joystick.uf2\n' "$build_dir"
