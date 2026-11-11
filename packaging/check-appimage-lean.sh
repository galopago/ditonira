#!/usr/bin/env bash
# Gate: AppImage must not embed Network/CUPS/Print/TLS/QML/Web stacks (§8.0.5).
# Local D-Bus / systemd / Pulse clients remain allowed.
set -euo pipefail

APPIMAGE="${1:-}"
if [[ -z "${APPIMAGE}" || ! -f "${APPIMAGE}" ]]; then
  echo "Usage: $0 path/to/Ditonira-x86_64.AppImage" >&2
  exit 1
fi

TMP="$(mktemp -d)"
cleanup() { rm -rf "${TMP}"; }
trap cleanup EXIT

echo "Extracting $(basename "${APPIMAGE}") for lean gate…"
(
  cd "${TMP}"
  if ! "${APPIMAGE}" --appimage-extract >/dev/null 2>&1; then
    APPIMAGE_EXTRACT_AND_RUN=1 "${APPIMAGE}" --appimage-extract >/dev/null
  fi
)

ROOT="${TMP}/squashfs-root"
BIN="${ROOT}/usr/bin/ditonira"
if [[ ! -f "${BIN}" ]]; then
  echo "ditonira binary not found inside AppImage" >&2
  exit 1
fi

fail=0

echo "DT_NEEDED on usr/bin/ditonira:"
needed="$(readelf -d "${BIN}" | awk '/NEEDED/ {print $NF}' | tr -d '[]')"
printf '  %s\n' ${needed}

# Binary must not directly need forbidden families.
while read -r lib; do
  case "${lib}" in
    libQt5Network.so*|libQt5PrintSupport.so*|libcups.so*|libssl.so*|libcrypto.so*|\
    libnss3.so*|libgnutls.so*|libQt5Qml.so*|libQt5Multimedia.so*|libQt5Sql.so*)
      echo "FAIL: ditonira DT_NEEDED ${lib}" >&2
      fail=1
      ;;
  esac
done <<< "${needed}"

# AppDir inventory must not contain forbidden plugin dirs / libs.
hits="$(find "${ROOT}" \( -type f -o -type l -o -type d \) 2>/dev/null \
  | grep -iE '/(bearer|printsupport|sqldrivers|mediaservice|qmltooling)(/|$)|libQt5Network\.so|libQt5PrintSupport\.so|libcups\.so|libssl\.so|libcrypto\.so|libnss3\.so|libgnutls\.so|libQt5Qml|libQt5WebEngine|libQt5Multimedia' \
  || true)"

if [[ -n "${hits}" ]]; then
  echo "FAIL: forbidden paths still present in AppImage:" >&2
  echo "${hits}" >&2
  fail=1
else
  echo "AppDir inventory: no Network/CUPS/Print/TLS/QML/WebEngine/Multimedia hits"
fi

# Confirm expected minimal Qt modules are present.
for need in libQt5Core.so.5 libQt5Gui.so.5 libQt5Widgets.so.5 libQt5SerialPort.so.5 libonnxruntime.so; do
  if ! find "${ROOT}/usr/lib" -name "${need}*" | grep -q .; then
    echo "FAIL: required ${need}* missing from AppDir" >&2
    fail=1
  fi
done

if [[ "${fail}" -ne 0 ]]; then
  echo "Lean gate FAILED" >&2
  exit 1
fi

echo "Lean gate PASS (offline allowlist; local D-Bus/audio IPC still permitted)"
