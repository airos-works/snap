#!/usr/bin/env bash
# Runs a command on an air/OS test machine over SSH (default: the X399
# workstation). Usage: tools/ws.sh 'command'   (stdin is forwarded)
set -euo pipefail
SNAP_WS_HOST=${SNAP_WS_HOST:-192.168.1.247}
SNAP_WS_USER=${SNAP_WS_USER:-user}
SNAP_WS_KEY=${SNAP_WS_KEY:-/mnt/HaikuWork/x399/ssh/workstation_ed25519}
exec ssh -i "$SNAP_WS_KEY" -o IdentitiesOnly=yes -o BatchMode=yes \
	-o UserKnownHostsFile=/dev/null -o StrictHostKeyChecking=no -o LogLevel=ERROR \
	-o ConnectTimeout=15 -o ServerAliveInterval=30 -o ServerAliveCountMax=8 \
	"$SNAP_WS_USER@$SNAP_WS_HOST" "$@"
