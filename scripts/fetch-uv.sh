#!/usr/bin/env bash
#
# Download the pinned uv release for a platform, verify its checksum and
# place the executable in a destination folder.
#
# uv (https://github.com/astral-sh/uv) is shipped with PyQInt-GUI and is used
# on first launch to install a private Python environment with PyQInt.
#
# usage: scripts/fetch-uv.sh <target> <destination-dir>
#
#   target: x86_64-pc-windows-msvc | aarch64-apple-darwin |
#           x86_64-apple-darwin | x86_64-unknown-linux-gnu |
#           aarch64-unknown-linux-gnu
#
# To update uv: change UV_VERSION and the checksums below (published next to
# each release asset as <asset>.sha256).
#
set -euo pipefail

UV_VERSION="0.12.20"

sha256_for() {
    case "$1" in
        x86_64-pc-windows-msvc)   echo "95f9bc30fbb3574d276e28ac4a6de932d25153645853d13da8c21eec3bc88d06" ;;
        aarch64-apple-darwin)     echo "848fdeb602ff1a1baacd4f6c8b7bdc6cf1ad026a6d9cf59475fda17c179743ca" ;;
        x86_64-apple-darwin)      echo "ac54283d211fd77cdc152b67606dbaf6406ff4ab03f3af4ae99468fa8e887141" ;;
        x86_64-unknown-linux-gnu) echo "6590717592ace991ff83a63fef799e3ad9d33ecc8f96c5d6bdd732496e79337f" ;;
        aarch64-unknown-linux-gnu) echo "8a7aad7bc76a2fae5151566ff3e43eacce0b2a113d5e4de3e4afe3e58fa2441e" ;;
        *) echo "" ;;
    esac
}

TARGET="${1:?target triple required}"
DEST="${2:?destination directory required}"

EXPECTED="$(sha256_for "${TARGET}")"
if [[ -z "${EXPECTED}" ]]; then
    echo "ERROR: no checksum known for target ${TARGET}" >&2
    exit 1
fi

if [[ "${TARGET}" == *windows* ]]; then
    ASSET="uv-${TARGET}.zip"
else
    ASSET="uv-${TARGET}.tar.gz"
fi
URL="https://github.com/astral-sh/uv/releases/download/${UV_VERSION}/${ASSET}"

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

echo "[INFO] Downloading uv ${UV_VERSION} (${TARGET})"
curl --fail --location --silent --show-error --output "${WORK}/${ASSET}" "${URL}"

if command -v sha256sum >/dev/null; then
    ACTUAL="$(sha256sum "${WORK}/${ASSET}" | awk '{print $1}')"
else
    ACTUAL="$(shasum -a 256 "${WORK}/${ASSET}" | awk '{print $1}')"
fi
if [[ "${ACTUAL}" != "${EXPECTED}" ]]; then
    echo "ERROR: checksum mismatch for ${ASSET}" >&2
    echo "  expected ${EXPECTED}" >&2
    echo "  got      ${ACTUAL}" >&2
    exit 1
fi

mkdir -p "${DEST}"
if [[ "${ASSET}" == *.zip ]]; then
    unzip -o -q "${WORK}/${ASSET}" -d "${WORK}/extract"
    cp "${WORK}/extract/uv.exe" "${DEST}/"
else
    tar -xzf "${WORK}/${ASSET}" -C "${WORK}"
    cp "${WORK}/uv-${TARGET}/uv" "${DEST}/"
    chmod +x "${DEST}/uv"
fi

echo "[INFO] uv ${UV_VERSION} installed in ${DEST}"
