#!/usr/bin/env bash
# Runs INSIDE the Ubuntu 20.04 factory image.
# Builds PortAudio for this glibc, then produces dist/Ditonira-x86_64.AppImage.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

ORT_ROOT="${ROOT}/.deps/onnxruntime-linux-x64-1.19.2"
PA_SRC="${ROOT}/.deps/portaudio-src"
PA_PREFIX="${ROOT}/.deps/portaudio-install-u2004"

if [[ ! -f "${ORT_ROOT}/include/onnxruntime_cxx_api.h" ]]; then
  echo "ONNX Runtime missing at ${ORT_ROOT}" >&2
  exit 1
fi
if [[ ! -x "${PA_SRC}/configure" ]]; then
  echo "PortAudio sources missing at ${PA_SRC}" >&2
  exit 1
fi

echo "Building PortAudio (static) for Ubuntu 20.04 factory…"
rm -rf "${PA_PREFIX}"
mkdir -p "${PA_PREFIX}"
PA_BUILD="${ROOT}/.deps/portaudio-build-u2004"
rm -rf "${PA_BUILD}"
# Use CMake out-of-tree (autoconf out-of-tree is unreliable for this tree).
/usr/bin/cmake -S "${PA_SRC}" -B "${PA_BUILD}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="${PA_PREFIX}" \
  -DPA_BUILD_STATIC=ON \
  -DPA_BUILD_SHARED=OFF \
  -DPA_USE_ALSA=ON \
  -DPA_USE_JACK=OFF
/usr/bin/cmake --build "${PA_BUILD}" -j"$(nproc)"
/usr/bin/cmake --install "${PA_BUILD}"
# Ditonira expects include/portaudio.h and lib/libportaudio.a
if [[ ! -f "${PA_PREFIX}/include/portaudio.h" || ! -f "${PA_PREFIX}/lib/libportaudio.a" ]]; then
  echo "PortAudio install incomplete under ${PA_PREFIX}" >&2
  ls -laR "${PA_PREFIX}" >&2 || true
  exit 1
fi

echo "Cleaning previous AppImage artifacts…"
rm -rf "${ROOT}/build-appimage" "${ROOT}/AppDir" "${ROOT}/dist"
mkdir -p "${ROOT}/dist"

# Inject PortAudio/ALSA paths only for cmake *configure* (not --build/--install).
WRAPPER_BIN="$(mktemp -d)"
cat > "${WRAPPER_BIN}/cmake" <<EOF
#!/usr/bin/env bash
set -euo pipefail
real=/usr/bin/cmake
# Configure invocations set a build/source tree; build/install pass --build/--install.
for arg in "\$@"; do
  case "\$arg" in
    --build|--install|--find-package) exec "\$real" "\$@" ;;
  esac
done
exec "\$real" -DPORTAUDIO_ROOT="${PA_PREFIX}" -DALSA_LIB_DIR=/nonexistent "\$@"
EOF
chmod +x "${WRAPPER_BIN}/cmake"
export PATH="${WRAPPER_BIN}:${PATH}"
export APPIMAGE_EXTRACT_AND_RUN=1

./packaging/build-appimage.sh

rm -rf "${WRAPPER_BIN}"

./packaging/check-appimage-glibc.sh "${ROOT}/dist/Ditonira-x86_64.AppImage"
./packaging/check-appimage-lean.sh "${ROOT}/dist/Ditonira-x86_64.AppImage"

echo "Factory build OK: ${ROOT}/dist/Ditonira-x86_64.AppImage"
