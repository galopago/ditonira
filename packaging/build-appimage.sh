#!/usr/bin/env bash
# Build a self-contained Ditonira-x86_64.AppImage (bundles Qt5).
# Prefer building on Ubuntu 20.04 (or older glibc) for wider host compatibility.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT}/build-appimage"
APPDIR="${ROOT}/AppDir"
DIST="${ROOT}/dist"
TOOLS="${ROOT}/packaging/tools"
ARCH="$(uname -m)"

if [[ "${ARCH}" != "x86_64" ]]; then
  echo "This script targets x86_64 (got ${ARCH})." >&2
  exit 1
fi

mkdir -p "${TOOLS}" "${DIST}"

download() {
  local url="$1"
  local out="$2"
  if [[ -x "${out}" ]]; then
    return 0
  fi
  echo "Downloading $(basename "${out}")…"
  curl -fsSL -o "${out}" "${url}"
  chmod +x "${out}"
}

LINUXDEPLOY="${TOOLS}/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="${TOOLS}/linuxdeploy-plugin-qt-x86_64.AppImage"

download \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
  "${LINUXDEPLOY}"
download \
  "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
  "${LINUXDEPLOY_QT}"

echo "Configuring Release build…"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${BUILD_DIR}" -j"$(nproc)"

rm -rf "${APPDIR}"
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}"

# AppImage expects icon name matching desktop Icon= without extension under usr/share/icons
mkdir -p "${APPDIR}/usr/share/icons/hicolor/256x256/apps"
cp -f "${ROOT}/resources/icons/ditonira.png" \
  "${APPDIR}/usr/share/icons/hicolor/256x256/apps/ditonira.png"
mkdir -p "${APPDIR}/usr/share/applications"
cp -f "${ROOT}/resources/ditonira.desktop" "${APPDIR}/usr/share/applications/ditonira.desktop"

export QMAKE="$(command -v qmake || command -v qmake-qt5 || true)"
if [[ -z "${QMAKE}" ]]; then
  echo "qmake not found; Qt plugin needs it to locate Qt libraries." >&2
  exit 1
fi

export LDAI_OUTPUT="${DIST}/Ditonira-x86_64.AppImage"
export LINUXDEPLOY_OUTPUT_VERSION="${LINUXDEPLOY_OUTPUT_VERSION:-0.1.0}"

# Extraneous AppImage FUSE often fails in CI/containers; extract and run if needed.
run_tool() {
  local tool="$1"
  shift
  if "${tool}" "$@" 2>/dev/null; then
    return 0
  fi
  local extract_dir
  extract_dir="$(mktemp -d)"
  pushd "${extract_dir}" >/dev/null
  "${tool}" --appimage-extract >/dev/null
  popd >/dev/null
  "${extract_dir}/squashfs-root/AppRun" "$@"
  rm -rf "${extract_dir}"
}

echo "Bundling Qt and creating AppImage…"
run_tool "${LINUXDEPLOY}" \
  --appdir "${APPDIR}" \
  --executable "${APPDIR}/usr/bin/ditonira" \
  --desktop-file "${APPDIR}/usr/share/applications/ditonira.desktop" \
  --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/ditonira.png" \
  --plugin qt \
  --output appimage

# linuxdeploy may write the AppImage in cwd; move if needed
if [[ ! -f "${LDAI_OUTPUT}" ]]; then
  found="$(find "${ROOT}" -maxdepth 2 -name 'Ditonira*.AppImage' -o -name 'ditonira*.AppImage' | head -n1 || true)"
  if [[ -n "${found}" ]]; then
    mv -f "${found}" "${LDAI_OUTPUT}"
  fi
fi

# Ensure output lands in dist/
shopt -s nullglob
for f in "${ROOT}"/Ditonira*.AppImage "${ROOT}"/ditonira*.AppImage "${BUILD_DIR}"/Ditonira*.AppImage; do
  mv -f "${f}" "${DIST}/Ditonira-x86_64.AppImage"
done
shopt -u nullglob

if [[ -f "${DIST}/Ditonira-x86_64.AppImage" ]]; then
  chmod +x "${DIST}/Ditonira-x86_64.AppImage"
  echo "OK: ${DIST}/Ditonira-x86_64.AppImage"
else
  echo "AppImage was not produced." >&2
  exit 1
fi
