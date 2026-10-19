#!/usr/bin/env bash
# Compiles every shader the way Snap assembles them (#version 140, then the
# prelude common.glsl, then the effect) with glslangValidator, to catch GLSL
# errors on the build host before they reach a GPU.
# Usage: tools/check-shaders.sh   (needs glslang-tools)
set -euo pipefail
ROOT=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
SHADERS=$ROOT/src/effects/shaders
WORK=$(mktemp -d)
trap 'rm -rf -- "$WORK"' EXIT
command -v glslangValidator > /dev/null || { echo "glslangValidator is not installed" >&2; exit 2; }

failed=0
{ echo "#version 140"; cat "$SHADERS/quad.vert"; } > "$WORK/quad.vert"
glslangValidator -S vert "$WORK/quad.vert" > "$WORK/log" || { cat "$WORK/log"; failed=1; }
for shader in "$SHADERS"/*.glsl; do
	name=$(basename "$shader")
	[[ $name == common.glsl ]] && continue
	{ echo "#version 140"; echo "#line 1 1"; cat "$SHADERS/common.glsl";
	  echo "#line 1 2"; cat "$shader"; } > "$WORK/$name.frag"
	if glslangValidator -S frag "$WORK/$name.frag" > "$WORK/log" 2>&1; then
		echo "ok      $name"
	else
		echo "FAILED  $name (file 1 = common.glsl, file 2 = $name)"
		grep -E "ERROR|WARNING" "$WORK/log" | sed 's/^/        /'
		failed=1
	fi
done
exit $failed
