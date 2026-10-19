#!/usr/bin/env bash
# Copies a cross build (build-cross/Snap) to /boot/home/snap-test on the test
# machine of tools/ws.sh and gives it its attributes there. Not packaged:
# run it from a Terminal with /boot/home/snap-test/Snap.
set -euo pipefail
SNAP_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
WS="$SNAP_ROOT/tools/ws.sh"
"$WS" 'mkdir -p /boot/home/snap-test && rm -f /boot/home/snap-test/Snap.new'
"$WS" 'cat > /boot/home/snap-test/Snap.new' < "$SNAP_ROOT/build-cross/Snap"
# Replaced with a rename: a running copy keeps its file.
"$WS" 'cd /boot/home/snap-test && chmod 755 Snap.new && mv -f Snap.new Snap && mimeset -f Snap && echo deployed $(stat -c %s Snap) bytes'
