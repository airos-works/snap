#!/usr/bin/env bash
# Cross-compiles Snap on Linux for x86_64 air/OS with the Haiku cross
# compiler and a sysroot copied from an air/OS machine; the resources are
# compiled with host builds of rc, xres and mimeset.
# Produces build-cross/Snap. Usage: tools/build-cross.sh [make target]
set -euo pipefail
SNAP_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
HAIKU_SYSROOT=${HAIKU_SYSROOT:-/mnt/HaikuWork/toolchains/haiku-sysroot-x86_64}
HAIKU_BUILD=${HAIKU_BUILD:-/mnt/HaikuWork/x399/build/x86_64}
CROSS=${HAIKU_CROSS:-$HAIKU_BUILD/cross-tools-x86_64/bin/x86_64-unknown-haiku}
HOST_TOOLS=$HAIKU_BUILD/objects/linux/x86_64/release/tools
MIMEDB=$HAIKU_BUILD/objects/haiku/x86_64/release/kits/libbe.so_mimedb

cd "$SNAP_ROOT"
exec make BUILD=build-cross -j"$(nproc)" \
	CXX="$CROSS-g++ --sysroot=$HAIKU_SYSROOT" \
	RC="$HOST_TOOLS/rc/rc" XRES="$HOST_TOOLS/xres" \
	MIMESET="$HOST_TOOLS/mimeset --mimedb $MIMEDB" \
	"${1:-all}"
