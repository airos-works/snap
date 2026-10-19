#!/usr/bin/env bash
# Builds the Haiku package (.hpkg) on Haiku or air/OS. Run from the
# repository root (or `make package`).
#
# Snap is a demo: it goes into /boot/system/demos, with a link in Deskbar's
# Demos menu, next to GLTeapot. The air/OS build server stages the same
# layout (airos-ci apps/build-app-packages.sh, build_snap).
set -euo pipefail
SNAP_ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
cd "$SNAP_ROOT"
INFO=resources/Snap.PackageInfo
NAME=$(awk '$1 == "name" { print $2; exit }' "$INFO")
VERSION=$(awk '$1 == "version" { print $2; exit }' "$INFO")
ARCH=$(getarch 2>/dev/null || awk '$1 == "architecture" { print $2; exit }' "$INFO")
FILE="$SNAP_ROOT/artifacts/$NAME-$VERSION-$ARCH.hpkg"

make -j"$(nproc 2>/dev/null || echo 4)"
STAGE=$(mktemp -d /tmp/snap-package-XXXXXX)
trap 'rm -rf -- "$STAGE"' EXIT
DOCS="$STAGE/documentation/packages/snap"
mkdir -p "$STAGE/demos" "$STAGE/data/deskbar/menu/Demos" "$DOCS" \
	"$SNAP_ROOT/artifacts"

cp build-haiku/Snap "$STAGE/demos/Snap"
strip --strip-debug "$STAGE/demos/Snap"
# GNU strip removes the appended Haiku resources (and the shaders with
# them); put them back.
xres -o "$STAGE/demos/Snap" build-haiku/Snap.rsrc
ln -s ../../../../demos/Snap "$STAGE/data/deskbar/menu/Demos/Snap"
sed "s/^architecture[ \t].*/architecture $ARCH/" "$INFO" > "$STAGE/.PackageInfo"
cp README.md LICENSE "$DOCS/"
cp -R docs "$DOCS/"

( cd "$STAGE" && mimeset --all -f --mimedb data/mime_db --mimedb /boot/system/data/mime_db demos/Snap )
package create -C "$STAGE" "$FILE"
printf '%s\n' "$FILE"
