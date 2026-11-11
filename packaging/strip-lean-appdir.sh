#!/usr/bin/env bash
# Remove forbidden remote/bloat stacks from an AppDir (ARCHITECTURE.md §2.1 / §8.0.5).
# Keeps Qt DBus / libdbus / systemd / local-IPC deps — those are allowed offline.
set -euo pipefail

APPDIR="${1:-}"
if [[ -z "${APPDIR}" || ! -d "${APPDIR}" ]]; then
  echo "Usage: $0 /path/to/AppDir" >&2
  exit 1
fi

echo "Lean strip of AppDir: ${APPDIR}"

is_forbidden_lib() {
  local base="$1"
  case "${base}" in
    libQt5Network.so*|libQt5PrintSupport.so*|libQt5Qml.so*|libQt5QmlModels.so*|\
    libQt5QmlWorkerScript.so*|libQt5Quick.so*|libQt5QuickWidgets.so*|\
    libQt5Multimedia.so*|libQt5MultimediaWidgets.so*|libQt5Sql.so*|\
    libQt5Designer.so*|libQt5DesignerComponents.so*|libQt5WebEngine*.so*|\
    libQt5WebKit*.so*|libQt5WebSockets.so*|libQt5HttpServer.so*|\
    libcups.so*|libcupsimage.so*|libssl.so*|libcrypto.so*|\
    libnss3.so*|libnssutil3.so*|libsmime3.so*|libssl3.so*|libgnutls.so*|\
    libhogweed.so*|libnettle.so*)
      return 0
      ;;
  esac
  return 1
}

plugin_dirs=(
  bearer
  printsupport
  designer
  designercomponents
  sqldrivers
  playlistformats
  mediaservice
  audio
  video
  qmltooling
  scenegraph
  sensorgestures
  position
  geolocation
  sensors
  canbus
  gamepads
  texttospeech
)

for base in \
  "${APPDIR}/usr/plugins" \
  "${APPDIR}/usr/lib/plugins" \
  "${APPDIR}/usr/lib/qt5/plugins" \
  "${APPDIR}/usr/lib/x86_64-linux-gnu/qt5/plugins"
do
  [[ -d "${base}" ]] || continue
  for d in "${plugin_dirs[@]}"; do
    if [[ -e "${base}/${d}" ]]; then
      echo "  remove plugin dir ${base}/${d}"
      rm -rf "${base}/${d}"
    fi
  done
done

while IFS= read -r -d '' f; do
  base="$(basename "${f}")"
  if is_forbidden_lib "${base}"; then
    echo "  remove ${f}"
    rm -f "${f}"
  fi
done < <(find "${APPDIR}" \( -type f -o -type l \) -name '*.so*' -print0 2>/dev/null)

find "${APPDIR}" -xtype l -print -delete 2>/dev/null || true

echo "Lean strip done."
