#!/usr/bin/env bash
set -euo pipefail

build_dir="${HOST_TEST_BUILD_DIR:-build-host-tests}"
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
    printf 'CMake is required to build the host tests. Install it first, for example:\n' >&2
    printf '  sudo apt install cmake\n' >&2
    exit 2
fi

"$cmake_bin" -S tests -B "$build_dir"
"$cmake_bin" --build "$build_dir" --parallel
"$build_dir/joystick_host_tests"
