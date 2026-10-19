#!/usr/bin/env bash
# Copies src/effects/shaders into the Snap test VM (/boot/home/snap-test/shaders).
# A Snap started with SNAP_SHADER_DIR=/boot/home/snap-test/shaders reads them
# from there and builds an effect's shaders again whenever it is shown anew.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
tar -C "$ROOT/src/effects" -cf - shaders \
	| "$ROOT/tools/vm/haiku.sh" 'mkdir -p /boot/home/snap-test && tar -C /boot/home/snap-test -xf -'
