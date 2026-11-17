#!/usr/bin/env bash
set -euo pipefail

readonly APP_NAME="deezer-desktop"
readonly RELEASE_VERSION="v0.1.3"
readonly ARCH="x86_64"
readonly ARCHIVE_NAME="${APP_NAME}-${RELEASE_VERSION}-linux-${ARCH}.tar.gz"

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_ROOT}/build}"
DIST_DIR="${DIST_DIR:-${PROJECT_ROOT}/dist}"

fail() {
    printf 'package-linux: error: %s\n' "$*" >&2
    exit 1
}

require_file() {
    [[ -f "$1" ]] || fail "required file is missing: $1"
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "required command is missing: $1"
}

[[ "$(uname -m)" == "$ARCH" ]] || fail "this package target is ${ARCH}; host architecture is $(uname -m)"
[[ -d "$BUILD_DIR" ]] || fail "CMake build directory not found: $BUILD_DIR"
[[ -x "$BUILD_DIR/$APP_NAME" ]] || fail "built application is missing or not executable: $BUILD_DIR/$APP_NAME"
require_file "$BUILD_DIR/CMakeCache.txt"
require_command tar
require_command gzip
require_command sha256sum
require_command patchelf

CEF_ROOT="$(sed -n 's/^CEF_ROOT:PATH=//p' "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
[[ -n "$CEF_ROOT" && -d "$CEF_ROOT" ]] || fail "CEF_ROOT was not found in $BUILD_DIR/CMakeCache.txt"
CEF_BUILD_TYPE="$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$BUILD_DIR/CMakeCache.txt" | head -n 1)"
[[ -n "$CEF_BUILD_TYPE" ]] || CEF_BUILD_TYPE=Release
CEF_BINARY_DIR="${CEF_ROOT}/${CEF_BUILD_TYPE}"

for required in \
    "$PROJECT_ROOT/install.sh" \
    "$PROJECT_ROOT/uninstall.sh" \
    "$PROJECT_ROOT/README.md" \
    "$PROJECT_ROOT/LICENSE" \
    "$CEF_ROOT/LICENSE.txt" \
    "$CEF_ROOT/CREDITS.html" \
    "$CEF_BINARY_DIR/libcef.so" \
    "$CEF_BINARY_DIR/libGLESv2.so"; do
    require_file "$required"
done

[[ -d "$PROJECT_ROOT/assets/icons" ]] || fail "application icon directory is missing"
[[ -n "$(find "$PROJECT_ROOT/assets/icons" -type f -print -quit)" ]] || fail "application icon directory is empty"

STAGING_DIR="$(mktemp -d "${TMPDIR:-/tmp}/deezer-desktop-package.XXXXXXXX")"
cleanup() {
    rm -rf -- "$STAGING_DIR"
}
trap cleanup EXIT INT TERM

PACKAGE_ROOT="${STAGING_DIR}/package"
mkdir -p "$PACKAGE_ROOT/assets"

# Start from the built executable and CEF runtime output, never from the build
# directory as a whole (which also contains generated developer artifacts).
install -m 0755 "$BUILD_DIR/$APP_NAME" "$PACKAGE_ROOT/$APP_NAME"
install -m 0755 "$PROJECT_ROOT/install.sh" "$PACKAGE_ROOT/install.sh"
install -m 0755 "$PROJECT_ROOT/uninstall.sh" "$PACKAGE_ROOT/uninstall.sh"
install -m 0644 "$PROJECT_ROOT/README.md" "$PACKAGE_ROOT/README.md"
install -m 0644 "$PROJECT_ROOT/LICENSE" "$PACKAGE_ROOT/LICENSE"
install -m 0644 "$CEF_ROOT/LICENSE.txt" "$PACKAGE_ROOT/LICENSE.txt"
install -m 0644 "$CEF_ROOT/CREDITS.html" "$PACKAGE_ROOT/CREDITS.html"
cp -a "$PROJECT_ROOT/assets/icons" "$PACKAGE_ROOT/assets/icons"

# These files are the Linux CEF binary/resource runtime used by this project.
# CMake copies these entries to BUILD_DIR, except libcef.so (and chrome-sandbox,
# which this project disables). libGLESv2.so is shipped by the same CEF bundle
# and was part of the existing runtime staging layout.
CEF_BUILD_RUNTIME_FILES=(
    libEGL.so
    libvk_swiftshader.so
    libvulkan.so.1
    v8_context_snapshot.bin
    vk_swiftshader_icd.json
)
CEF_RESOURCE_FILES=(
    chrome_100_percent.pak
    chrome_200_percent.pak
    resources.pak
    icudtl.dat
)

for runtime_file in "${CEF_BUILD_RUNTIME_FILES[@]}" "${CEF_RESOURCE_FILES[@]}"; do
    require_file "$BUILD_DIR/$runtime_file"
    cp -a "$BUILD_DIR/$runtime_file" "$PACKAGE_ROOT/$runtime_file"
done

[[ -d "$BUILD_DIR/locales" ]] || fail "CEF locales runtime is not a directory"
cp -a "$BUILD_DIR/locales" "$PACKAGE_ROOT/locales"

install -m 0644 "$CEF_BINARY_DIR/libcef.so" "$PACKAGE_ROOT/libcef.so"
install -m 0755 "$CEF_BINARY_DIR/libGLESv2.so" "$PACKAGE_ROOT/libGLESv2.so"

# The CEF development build embeds an absolute SDK RUNPATH. Make the staged
# executable resolve its bundled libcef.so beside itself when extracted.
patchelf --set-rpath '$ORIGIN' "$PACKAGE_ROOT/$APP_NAME"

[[ -x "$PACKAGE_ROOT/$APP_NAME" ]] || fail "staged application is not executable"
[[ -x "$PACKAGE_ROOT/install.sh" ]] || fail "staged install.sh is not executable"
[[ -x "$PACKAGE_ROOT/uninstall.sh" ]] || fail "staged uninstall.sh is not executable"
[[ -s "$PACKAGE_ROOT/libcef.so" ]] || fail "staged libcef.so is empty"
[[ -s "$PACKAGE_ROOT/libGLESv2.so" ]] || fail "staged libGLESv2.so is empty"
[[ -n "$(find "$PACKAGE_ROOT/assets/icons" -type f -print -quit)" ]] || fail "staged application icons are missing"

mkdir -p "$DIST_DIR"
ARCHIVE_PATH="${DIST_DIR}/${ARCHIVE_NAME}"
CHECKSUM_PATH="${ARCHIVE_PATH}.sha256"
TEMP_ARCHIVE="${STAGING_DIR}/${ARCHIVE_NAME}"

# Normalize ownership, ordering, timestamps and gzip headers for repeatable
# archives from the same build inputs.
tar --sort=name --mtime='@0' --owner=0 --group=0 --numeric-owner \
    -C "$PACKAGE_ROOT" -cf - . | gzip -n -9 > "$TEMP_ARCHIVE"
tar -tzf "$TEMP_ARCHIVE" >/dev/null

mv -f -- "$TEMP_ARCHIVE" "$ARCHIVE_PATH"
(
    cd -- "$DIST_DIR"
    sha256sum "$ARCHIVE_NAME" > "${ARCHIVE_NAME}.sha256"
)

printf 'Package created: %s\n' "$ARCHIVE_PATH"
printf 'Checksum: %s\n' "$CHECKSUM_PATH"
