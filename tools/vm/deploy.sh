#!/usr/bin/env bash
# Copies the cross build into the Snap test VM and (re)starts it there with
# software rendering allowed; Snap's output goes to /boot/home/snap-test/snap.log.
# Extra arguments are environment settings for Snap, e.g. SNAP_SHADER_DIR=...
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
VM="$ROOT/tools/vm/haiku.sh"
"$VM" 'hey Snap quit > /dev/null 2>&1; for i in 1 2 3 4 5 6 7 8 9 10; do ps | grep -q "[.]/Snap" || break; sleep 0.5; done; mkdir -p /boot/home/snap-test'
"$VM" 'cat > /boot/home/snap-test/Snap' < "$ROOT/build-cross/Snap"
"$VM" "cd /boot/home/snap-test && chmod 755 Snap && mimeset -f Snap && (env SNAP_ALLOW_SOFTWARE_RENDERING=1 $* ./Snap > snap.log 2>&1 &) ; sleep 4; cat snap.log"
