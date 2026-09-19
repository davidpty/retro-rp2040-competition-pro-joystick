#!/usr/bin/env bash
set -euo pipefail

build_dir="${BUILD_DIR:-build}"
pico_sdk_path="${PICO_SDK_PATH:-}"
picotool_cmake_dir="${PICOTOOL_DIR:-${picotool_DIR:-}}"

if [[ -z "$pico_sdk_path" ]]; then
    printf 'PICO_SDK_PATH is not set. Set it to your Raspberry Pi Pico SDK directory.\n' >&2
    exit 2
fi

cmake_args=(-DPICO_SDK_PATH="$pico_sdk_path")
if [[ -n "$picotool_cmake_dir" ]]; then
    cmake_args+=(-Dpicotool_DIR="$picotool_cmake_dir")
fi

cmake -S . -B "$build_dir" "${cmake_args[@]}"
cmake --build "$build_dir" --parallel

printf 'Firmware: %s/rp2040_zero_hid_joystick.uf2\n' "$build_dir"
