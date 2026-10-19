#!/usr/bin/env bash
# Starts a throwaway Haiku test VM for Snap (VNC 127.0.0.1:5912, SSH 2232).
#
# It boots TasAmp's Haiku R1/beta6 disk (apps/tasamp/.vm/work.qcow2, override
# with SNAP_VM_BASE) read-only through a qcow2 overlay in SNAP_VM_DIR (default
# /mnt/HaikuWork/tmp/snap-vm), and uses TasAmp's SSH key. QEMU locks the
# base: TasAmp's, WordCraft's and GridCraft's VMs can't run at the same time.
#
# A VM has no GPU (OpenGL is llvmpipe), so run Snap in it with
# SNAP_ALLOW_SOFTWARE_RENDERING=1, and no camera, so the effects run on the
# test scene. The sound card (intel-hda) is there for the Media Kit.
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
TASAMP=$(cd "$ROOT/.." && pwd)/tasamp/.vm
BASE=${SNAP_VM_BASE:-$TASAMP/work.qcow2}
DIR=${SNAP_VM_DIR:-/mnt/HaikuWork/tmp/snap-vm}
OVERLAY=$DIR/overlay.qcow2
STAMP=$DIR/base.stamp
mkdir -p "$DIR"
if [[ -f $DIR/qemu.pid ]] && kill -0 "$(cat "$DIR/qemu.pid")" 2>/dev/null; then
	echo 'The Snap test VM is already running.'
	exit 0
fi
stamp=$(stat -c '%s %Y' "$BASE")
if [[ -f $OVERLAY && "$(cat "$STAMP" 2>/dev/null)" != "$stamp" ]]; then
	echo 'Base disk changed; starting from a fresh overlay.'
	rm -f "$OVERLAY"
fi
if [[ ! -f $OVERLAY ]]; then
	qemu-img create -q -f qcow2 -b "$BASE" -F qcow2 "$OVERLAY"
	echo "$stamp" > "$STAMP"
fi
[[ -f $DIR/id_ed25519 ]] || cp "$TASAMP/id_ed25519" "$DIR/id_ed25519"
chmod 600 "$DIR/id_ed25519"
rm -f "$DIR/qmp.sock"
exec qemu-system-x86_64 -enable-kvm -cpu host -m "${SNAP_VM_MEMORY:-6144}" \
	-smp "${SNAP_VM_CPUS:-8}" \
	-drive file="$OVERLAY",format=qcow2,if=ide,index=0 \
	-nic user,model=e1000,hostfwd=tcp:127.0.0.1:2232-:22 \
	-device qemu-xhci -device usb-tablet -vga std -display none \
	-audiodev none,id=snd -device intel-hda -device hda-duplex,audiodev=snd \
	-vnc 127.0.0.1:12 -qmp unix:"$DIR/qmp.sock",server=on,wait=off \
	-pidfile "$DIR/qemu.pid" -serial file:"$DIR/serial.log" -daemonize
