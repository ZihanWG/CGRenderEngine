#!/bin/bash
#
# Shared verification entrypoint. Run from anywhere inside the repository.
#
#   Tools/dev/verify_renderer.sh shaders   validate every GLSL shader with glslangValidator
#   Tools/dev/verify_renderer.sh tests     configure if needed, build, and run the CPU-only tests
#   Tools/dev/verify_renderer.sh fast      shaders, build every target, run the tests
#   Tools/dev/verify_renderer.sh full      fast, then a Release build and test run in its own tree
#
# `fast` is the normal completion gate. None of these runs the renderer, so none
# of them is evidence about GPU output or frame timing.
#
# Environment:
#   CGENGINE_BUILD_DIR          Debug build tree (default out/build)
#   CGENGINE_RELEASE_BUILD_DIR  Release build tree for `full` (default out/build-release)
#   CGENGINE_TOOLCHAIN_PATH     extra PATH entries searched first for cmake/ninja/g++
set -euo pipefail

ROOT="$(git -C "$(dirname "${BASH_SOURCE[0]}")" rev-parse --show-toplevel)"
cd "$ROOT"

readonly BUILD_DIR="${CGENGINE_BUILD_DIR:-out/build}"
readonly RELEASE_DIR="${CGENGINE_RELEASE_BUILD_DIR:-out/build-release}"
readonly CONFIGURE_ARGS=(-DCGENGINE_BUILD_TESTS=ON -DCGENGINE_BUILD_EDITOR=ON)

step() {
    printf '\n== %s\n' "$*"
}

# CLion bundles cmake, ninja and MinGW without putting them on PATH, and an
# existing Ninja build tree needs all three. Prefer whatever PATH already has.
find_toolchain() {
    if [ -n "${CGENGINE_TOOLCHAIN_PATH:-}" ]; then
        PATH="$CGENGINE_TOOLCHAIN_PATH:$PATH"
    fi
    if command -v cmake >/dev/null 2>&1; then
        return 0
    fi

    local clion
    shopt -s nullglob
    for clion in /d/CLion/*/ "/c/Program Files/JetBrains"/CLion*/ "${LOCALAPPDATA:-/nonexistent}"/Programs/CLion*/; do
        if [ -d "$clion/bin/cmake/win/x64/bin" ]; then
            PATH="$clion/bin/cmake/win/x64/bin:$clion/bin/ninja/win/x64:$clion/bin/mingw/bin:$PATH"
            echo "toolchain: using CMake bundled with ${clion%/}"
            break
        fi
    done
    shopt -u nullglob

    if ! command -v cmake >/dev/null 2>&1; then
        echo "error: cmake not found on PATH or in a CLion install." >&2
        echo "       Set CGENGINE_TOOLCHAIN_PATH to the directory holding cmake." >&2
        exit 1
    fi
}

run_shaders() {
    step "Validate GLSL shaders"
    if ! command -v glslangValidator >/dev/null 2>&1; then
        echo "error: glslangValidator not found on PATH (it ships with the Vulkan SDK)." >&2
        exit 1
    fi

    local shader failed=0 count=0
    for shader in Shaders/*.vert Shaders/*.frag; do
        count=$((count + 1))
        if ! glslangValidator "$shader" >/dev/null; then
            glslangValidator "$shader" || true
            failed=$((failed + 1))
        fi
    done
    if [ "$failed" -ne 0 ]; then
        echo "shaders: $failed of $count failed validation" >&2
        exit 1
    fi
    echo "shaders: $count validated"
}

build_and_test() { # build_and_test <build-dir> <config>
    local dir="$1" config="$2"
    step "Configure $dir ($config)"
    cmake -S . -B "$dir" "${CONFIGURE_ARGS[@]}" -DCMAKE_BUILD_TYPE="$config" >/dev/null
    step "Build $dir ($config)"
    cmake --build "$dir" --config "$config" --parallel
    step "Test $dir ($config)"
    ctest --test-dir "$dir" -C "$config" --output-on-failure
}

mode="${1:-}"
case "$mode" in
shaders)
    run_shaders
    ;;
tests)
    find_toolchain
    build_and_test "$BUILD_DIR" Debug
    ;;
fast)
    run_shaders
    find_toolchain
    build_and_test "$BUILD_DIR" Debug
    ;;
full)
    run_shaders
    find_toolchain
    build_and_test "$BUILD_DIR" Debug
    build_and_test "$RELEASE_DIR" Release
    ;;
*)
    sed -n '3,17p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//' >&2
    exit 2
    ;;
esac

step "verify_renderer.sh $mode: passed"
