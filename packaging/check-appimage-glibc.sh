#!/usr/bin/env bash
# Gate: AppImage ditonira must not require GLIBC newer than 2.31 (Ubuntu 20.04).
set -euo pipefail

APPIMAGE="${1:-}"
MAX_GLIBC="${MAX_GLIBC:-2.31}"

if [[ -z "${APPIMAGE}" || ! -f "${APPIMAGE}" ]]; then
  echo "Usage: $0 path/to/Ditonira-x86_64.AppImage" >&2
  exit 1
fi

TMP="$(mktemp -d)"
cleanup() { rm -rf "${TMP}"; }
trap cleanup EXIT

echo "Extracting $(basename "${APPIMAGE}") for GLIBC gate…"
(
  cd "${TMP}"
  # Prefer extract without FUSE (works in containers).
  if ! "${APPIMAGE}" --appimage-extract >/dev/null 2>&1; then
    APPIMAGE_EXTRACT_AND_RUN=1 "${APPIMAGE}" --appimage-extract >/dev/null
  fi
)

BIN="${TMP}/squashfs-root/usr/bin/ditonira"
if [[ ! -f "${BIN}" ]]; then
  echo "ditonira binary not found inside AppImage" >&2
  exit 1
fi

mapfile -t VERS < <(objdump -T "${BIN}" | grep -oE 'GLIBC_[0-9.]+' | sed 's/^GLIBC_//' | sort -uV)
if [[ ${#VERS[@]} -eq 0 ]]; then
  echo "No GLIBC_ version symbols found (unexpected)" >&2
  exit 1
fi

echo "GLIBC symbols on usr/bin/ditonira:"
printf '  GLIBC_%s\n' "${VERS[@]}"

fail=0
for ver in "${VERS[@]}"; do
  newest="$(printf '%s\n%s\n' "${ver}" "${MAX_GLIBC}" | sort -V | tail -n1)"
  if [[ "${newest}" != "${MAX_GLIBC}" ]]; then
    echo "FAIL: GLIBC_${ver} is newer than allowed GLIBC_${MAX_GLIBC}" >&2
    fail=1
  fi
done

# Spot-check that ORT/Qt resolve from AppDir when LD path is restricted.
APPDIR="${TMP}/squashfs-root"
export LD_LIBRARY_PATH="${APPDIR}/usr/lib:${APPDIR}/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
if command -v ldd >/dev/null; then
  if ldd "${BIN}" 2>/dev/null | grep -E 'not found'; then
    echo "FAIL: unresolved shared libraries under AppDir library path" >&2
    fail=1
  else
    echo "ldd (AppDir lib path): no 'not found' lines"
  fi
fi

# Offscreen smoke (may fail without display plugins in some extract layouts — non-fatal warn).
if QT_QPA_PLATFORM=offscreen "${APPDIR}/AppRun" --help >/dev/null 2>&1 \
   || QT_QPA_PLATFORM=offscreen "${BIN}" --help >/dev/null 2>&1; then
  echo "Smoke: binary starts under QT_QPA_PLATFORM=offscreen"
else
  # Qt apps often have no --help; try a short timeout start.
  if command -v timeout >/dev/null; then
    if timeout 3 env QT_QPA_PLATFORM=offscreen "${BIN}" >/dev/null 2>&1; then
      echo "Smoke: binary exits cleanly under offscreen"
    else
      # timeout/exit nonzero is OK if it at least linked and started
      echo "Smoke: offscreen start attempted (no --help); continuing if GLIBC gate passed"
    fi
  fi
fi

if [[ "${fail}" -ne 0 ]]; then
  echo "GLIBC gate FAILED" >&2
  exit 1
fi

echo "GLIBC gate PASS (max allowed ${MAX_GLIBC}; newest on binary: GLIBC_${VERS[-1]})"
