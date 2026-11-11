#!/usr/bin/env bash
# Implementer/CI factory: build a portable AppImage inside Ubuntu 20.04.
# End users never run this — they only run dist/Ditonira-x86_64.AppImage.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_NAME="${DITONIRA_FACTORY_IMAGE:-ditonira-appimage-factory:20.04}"
DOCKER_BIN="${DOCKER_BIN:-}"

if [[ -z "${DOCKER_BIN}" ]]; then
  if command -v docker >/dev/null 2>&1; then
    DOCKER_BIN=docker
  elif command -v podman >/dev/null 2>&1; then
    DOCKER_BIN=podman
  else
    echo "Neither docker nor podman found (needed for the Implementer factory only)." >&2
    exit 1
  fi
fi

echo "Factory engine: ${DOCKER_BIN}"
echo "Building image ${IMAGE_NAME} from packaging/Dockerfile…"
"${DOCKER_BIN}" build -t "${IMAGE_NAME}" -f "${ROOT}/packaging/Dockerfile" "${ROOT}/packaging"

# Record image id for HANDOFF traceability.
IMAGE_ID="$("${DOCKER_BIN}" image inspect --format '{{.Id}}' "${IMAGE_NAME}")"
echo "Factory image id: ${IMAGE_ID}"

echo "Running factory build (mounted repo → /src)…"
mkdir -p "${ROOT}/dist"
"${DOCKER_BIN}" run --rm \
  --user "$(id -u):$(id -g)" \
  -e HOME=/tmp \
  -e APPIMAGE_EXTRACT_AND_RUN=1 \
  -e LINUXDEPLOY_OUTPUT_VERSION="${LINUXDEPLOY_OUTPUT_VERSION:-0.1.0}" \
  -v "${ROOT}:/src:rw" \
  -w /src \
  "${IMAGE_NAME}" \
  bash /src/packaging/factory-build.sh

# Persist factory image note next to the artifact.
{
  echo "image=${IMAGE_NAME}"
  echo "id=${IMAGE_ID}"
  echo "baseline=ubuntu:20.04"
  echo "built_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
} > "${ROOT}/dist/Ditonira-x86_64.AppImage.factory.txt"

echo
echo "Portable AppImage ready for the end user:"
echo "  ${ROOT}/dist/Ditonira-x86_64.AppImage"
echo "Factory record: ${ROOT}/dist/Ditonira-x86_64.AppImage.factory.txt"
