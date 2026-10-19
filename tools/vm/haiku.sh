#!/usr/bin/env bash
# Runs a command in the Snap test VM over SSH (port 2232). See run-vm.sh.
set -euo pipefail
DIR=${SNAP_VM_DIR:-/mnt/HaikuWork/tmp/snap-vm}
exec ssh -i "$DIR/id_ed25519" -p 2232 -o IdentitiesOnly=yes -o BatchMode=yes \
	-o UserKnownHostsFile="$DIR/known_hosts" -o StrictHostKeyChecking=accept-new \
	-o ConnectTimeout=10 -o ServerAliveInterval=30 user@127.0.0.1 "$@"
