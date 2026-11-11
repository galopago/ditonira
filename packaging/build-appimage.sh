#!/usr/bin/env bash
# Build a self-contained Ditonira-x86_64.AppImage (bundles Qt5).
# Prefer the Ubuntu 20.04 factory (build-appimage-docker.sh) for shipping artifacts.
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
APPIMAGETOOL="${TOOLS}/appimagetool-x86_64.AppImage"

download \
  "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" \
  "${LINUXDEPLOY}"
download \
  "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" \
  "${LINUXDEPLOY_QT}"
download \
  "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" \
  "${APPIMAGETOOL}"

echo "Configuring Release build…"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build "${BUILD_DIR}" -j"$(nproc)"

rm -rf "${APPDIR}"
DESTDIR="${APPDIR}" cmake --install "${BUILD_DIR}"

# Ensure ONNX Runtime + DeepCW model are present for RX (ARCHITECTURE_RX_DEEPCW.md §8).
ORT_ROOT="${ROOT}/.deps/onnxruntime-linux-x64-1.19.2"
mkdir -p "${APPDIR}/usr/lib" "${APPDIR}/usr/share/ditonira/models"
if [[ -f "${ORT_ROOT}/lib/libonnxruntime.so.1.19.2" ]]; then
  cp -f "${ORT_ROOT}/lib/libonnxruntime.so.1.19.2" "${APPDIR}/usr/lib/"
  ln -sfn libonnxruntime.so.1.19.2 "${APPDIR}/usr/lib/libonnxruntime.so.1"
  ln -sfn libonnxruntime.so.1 "${APPDIR}/usr/lib/libonnxruntime.so"
fi
cp -f "${ROOT}/resources/models/deepcw_en.onnx" "${APPDIR}/usr/share/ditonira/models/" 2>/dev/null || true
cp -f "${ROOT}/resources/models/deepcw_en_narrow.onnx" "${APPDIR}/usr/share/ditonira/models/" 2>/dev/null || true
cp -f "${ROOT}/resources/models/NOTICE.txt" "${APPDIR}/usr/share/ditonira/models/" 2>/dev/null || true

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
export VERSION="${LINUXDEPLOY_OUTPUT_VERSION}"
export APPIMAGE_EXTRACT_AND_RUN="${APPIMAGE_EXTRACT_AND_RUN:-1}"

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

echo "Bundling Qt into AppDir (no AppImage yet)…"
run_tool "${LINUXDEPLOY}" \
  --appdir "${APPDIR}" \
  --executable "${APPDIR}/usr/bin/ditonira" \
  --desktop-file "${APPDIR}/usr/share/applications/ditonira.desktop" \
  --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/ditonira.png" \
  --plugin qt \
  --exclude-library 'libQt5Network.so*' \
  --exclude-library 'libQt5PrintSupport.so*' \
  --exclude-library 'libcups.so*' \
  --exclude-library 'libssl.so*' \
  --exclude-library 'libcrypto.so*' \
  --exclude-library 'libnss3.so*' \
  --exclude-library 'libgnutls.so*'

# §8.0.5: strip leftovers linuxdeploy/qt plugin may still copy.
"${ROOT}/packaging/strip-lean-appdir.sh" "${APPDIR}"

echo "Packing AppImage with appimagetool…"
rm -f "${LDAI_OUTPUT}"
run_tool "${APPIMAGETOOL}" "${APPDIR}" "${LDAI_OUTPUT}"

# appimagetool may write next to AppDir or cwd; normalize into dist/
shopt -s nullglob
for f in "${ROOT}"/Ditonira*.AppImage "${ROOT}"/ditonira*.AppImage \
         "${BUILD_DIR}"/Ditonira*.AppImage "${APPDIR}"/../Ditonira*.AppImage; do
  [[ -f "${f}" ]] || continue
  [[ "${f}" -ef "${LDAI_OUTPUT}" ]] && continue
  mv -f "${f}" "${LDAI_OUTPUT}"
done
shopt -u nullglob

if [[ -f "${LDAI_OUTPUT}" ]]; then
  chmod +x "${LDAI_OUTPUT}"
  echo "OK: ${LDAI_OUTPUT}"
else
  echo "AppImage was not produced." >&2
  exit 1
fi
