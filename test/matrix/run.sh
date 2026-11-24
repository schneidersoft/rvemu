#!/usr/bin/env bash
# Host-side driver: build the rvemu-matrix image via podman (skip if present)
# and run the cross-build + qemu-user matrix for every architecture.
#
# Usage:
#   test/matrix/run.sh             # full matrix
#   test/matrix/run.sh --rebuild   # force image rebuild
#   test/matrix/run.sh --run-only  # skip any image build
set -euo pipefail

IMAGE="rvemu-matrix:local"
HERE="$( cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd )"
ROOT="$( cd -- "$HERE/../.." && pwd )"   # repo root (HERE = <root>/test/matrix)

REBUILD=0
RUN_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --rebuild) REBUILD=1 ;;
        --run-only) RUN_ONLY=1 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

image_exists() {
    podman image exists "$IMAGE" 2>/dev/null
}

build_image() {
    echo "==> building image $IMAGE (cross gccs + qemu-user)..."
    podman build -t "$IMAGE" -f "$HERE/Dockerfile" "$ROOT"
}

if [ $RUN_ONLY -eq 0 ]; then
    if [ $REBUILD -eq 1 ]; then
        echo "==> removing existing image (forced rebuild)"
        podman rmi "$IMAGE" >/dev/null 2>&1 || true
        build_image
    elif image_exists; then
        echo "==> image $IMAGE already present (use --rebuild to force)"
    else
        build_image
    fi
fi

if ! image_exists; then
    echo "image $IMAGE not available; build it first (drop --run-only)" >&2
    exit 1
fi

echo
echo "==> running matrix inside container ($IMAGE)"
# The repo root is bind-mounted at /workspace and used as workdir, so matrix.py can
# see the sources, the prebuilt guest bins, and write its logs under out/.
mkdir -p "$ROOT/out"
podman run --rm \
    -v "$ROOT:/workspace" \
    --workdir /workspace \
    --network=host \
    "$IMAGE" \
    python3 test/matrix/matrix.py
