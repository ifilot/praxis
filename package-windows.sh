#!/usr/bin/env bash
#
# Build Praxis and create the Windows installer (run in an MSYS2 MinGW64 shell)
#
set -euo pipefail

# ============================
# Configuration
# ============================
APP_NAME="Praxis"
APP_EXE="praxis.exe"
BUILD_DIR="build"
DIST_ROOT="dist"
DIST_DIR="${DIST_ROOT}/${APP_NAME}"
BUILD_TYPE="Release"

# ============================
# Sanity checks
# ============================
command -v cmake >/dev/null || { echo "cmake not found"; exit 1; }
command -v makensis >/dev/null || { echo "makensis not found"; exit 1; }
command -v unzip >/dev/null || { echo "unzip not found"; exit 1; }

if [[ -z "${MINGW_PREFIX:-}" ]]; then
  echo "MINGW_PREFIX not set (are you in a MinGW shell?)"
  exit 1
fi

echo "[INFO] Using MINGW_PREFIX=${MINGW_PREFIX}"

# ============================
# Clean
# ============================
echo "[INFO] Cleaning previous output"
mkdir -p "${DIST_DIR}"
find "${DIST_DIR}" -mindepth 1 -delete

# ============================
# Configure, build and test
# ============================
echo "[INFO] Configuring (${BUILD_TYPE})"
cmake -S . -B "${BUILD_DIR}" \
  -G Ninja \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"

echo "[INFO] Building"
cmake --build "${BUILD_DIR}"

echo "[INFO] Testing"
QT_QPA_PLATFORM=offscreen ctest --test-dir "${BUILD_DIR}" --output-on-failure

APP_VERSION="$(sed -n 's/#define PROGRAM_VERSION "\(.*\)"/\1/p' "${BUILD_DIR}/config.h")"
echo "[INFO] Version ${APP_VERSION}"

# ============================
# Stage files
# ============================
echo "[INFO] Staging files"
cp "${BUILD_DIR}/${APP_EXE}" "${DIST_DIR}/"

# uv is used by the program to install the private Python environment
bash scripts/fetch-uv.sh x86_64-pc-windows-msvc "${DIST_DIR}"

# ============================
# Locate windeployqt
# ============================
if command -v windeployqt >/dev/null 2>&1; then
  WINDEPLOYQT="$(command -v windeployqt)"
elif [[ -x "$MINGW_PREFIX/bin/windeployqt-qt6.exe" ]]; then
  WINDEPLOYQT="$MINGW_PREFIX/bin/windeployqt-qt6.exe"
else
  echo "ERROR: Qt 6 windeployqt not found"
  exit 1
fi

"$WINDEPLOYQT" "${DIST_DIR}/${APP_EXE}"

echo "[INFO] Bundling MinGW runtime DLLs"

# Follow dependencies recursively: windeployqt handles Qt plugins, while this
# loop also captures transitive MinGW libraries (for example ICU and Brotli).
while true; do
  dependency_list="$(mktemp)"
  find "${DIST_DIR}" -type f \( -iname '*.exe' -o -iname '*.dll' \) ! -iname 'uv.exe' -print0 |
    while IFS= read -r -d '' binary; do
      ldd "$binary" 2>/dev/null || true
    done |
    awk -v prefix="${MINGW_PREFIX}/bin/" '$3 ~ "^" prefix && tolower($3) ~ /\.dll$/ { print $3 }' |
    sort -u > "${dependency_list}"

  copied=0
  while IFS= read -r path; do
    [[ -n "$path" ]] || continue
    dll="$(basename "$path")"
    if [[ ! -f "${DIST_DIR}/${dll}" ]]; then
      echo "  + $dll"
      cp "$path" "${DIST_DIR}/"
      copied=1
    fi
  done < "${dependency_list}"
  rm -f "${dependency_list}"

  [[ "$copied" -eq 1 ]] || break
done

echo "[INFO] Verifying deployed dependencies"
missing_dependencies="$(
  find "${DIST_DIR}" -type f \( -iname '*.exe' -o -iname '*.dll' \) -print0 |
    while IFS= read -r -d '' binary; do
      ldd "$binary" 2>/dev/null | awk -v binary="$binary" '/=> not found/ { print binary ": " $1 }'
    done
)"
if [[ -n "$missing_dependencies" ]]; then
  echo "ERROR: Missing runtime dependencies:"
  echo "$missing_dependencies"
  exit 1
fi

# ============================
# Build installer
# ============================
echo "[INFO] Building NSIS installer"
makensis -DAPP_VERSION="${APP_VERSION}" installer.nsi

echo
echo "[SUCCESS] Windows package staged in ${DIST_DIR}"
echo "[SUCCESS] Windows installer created: ${APP_NAME}-Windows-Setup.exe"
