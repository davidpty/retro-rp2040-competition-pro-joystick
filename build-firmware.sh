#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Usage: ./build-firmware.sh [OPTIONS]

Build the RP2040 joystick firmware.

Options:
  --overwrite-settings  Build firmware that restores the embedded defaults once
                        on its first boot, then preserves later changes.
  -h, --help            Show this help and exit.

Environment:
  BUILD_DIR             CMake build directory (default: build)
  PICO_SDK_PATH         Raspberry Pi Pico SDK directory (required)
  PICOTOOL_DIR          picotool CMake package directory (optional)
  CMAKE_BIN             CMake executable (optional)
EOF
}

overwrite_settings=0
while (($# > 0)); do
    case "$1" in
        --overwrite-settings) overwrite_settings=1 ;;
        -h|--help) usage; exit 0 ;;
        *)
            printf 'Unknown option: %s\n\n' "$1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

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
cmake_args+=(-DJOY_SETTINGS_OVERWRITE="$overwrite_settings")
if ((overwrite_settings)); then
    reset_token="$(date +%s%N | cksum | cut -d ' ' -f1)"
    reset_token=$((reset_token % 65535 + 1))
else
    reset_token=0
fi
cmake_args+=(-DJOY_SETTINGS_OVERWRITE_TOKEN="$reset_token")

"$cmake_bin" -S . -B "$build_dir" "${cmake_args[@]}"
"$cmake_bin" --build "$build_dir" --parallel

printf 'Firmware: %s/rp2040_zero_hid_joystick.uf2\n' "$build_dir"
