// Text Mode: the picture as characters on a terminal, brighter characters
// where the picture is brighter, in the colours of the picture. The glyphs
// were drawn with the system's fixed font into a BBitmap. A beat flashes the
// screen phosphor green.

uniform sampler2D uGlyphs;		// the glyphs side by side, white on black
uniform float uGlyphCount;
uniform vec2 uCellSize;		// one character cell in output pixels

void main()
{
	vec2 cellCount = floor(uResolution / uCellSize);
	vec2 cell = floor(vUV * cellCount);
	vec2 inCell = fract(vUV * cellCount);

	// The picture averaged over the cell, from a mipmap level that size.
	vec2 cellUV = (cell + 0.5) / cellCount;
	float lod = log2(max(uCellSize.x * uCameraSize.x / uResolution.x, 1.0));
	vec3 picture = cameraBlur(cellUV, lod);
	float brightness = clamp(pow(luma(picture), 0.8) * 1.15, 0.0, 1.0);

	float glyph = min(floor(brightness * uGlyphCount), uGlyphCount - 1.0);
	vec2 glyphUV = vec2((glyph + inCell.x) / uGlyphCount, 1.0 - inCell.y);
	float ink = texture(uGlyphs, glyphUV).r;

	// The character takes the picture's colour, at full saturation, with
	// a touch of terminal green (all green for a moment on a beat).
	float strongest = max(max(picture.r, picture.g), picture.b);
	vec3 tint = mix(picture / max(strongest, 0.05), vec3(0.3, 1.0, 0.5),
		0.2 + 0.7 * uBeat);
	vec3 color = tint * ink * (0.45 + 1.1 * brightness);
	color += vec3(0.01, 0.03, 0.02);
	fragColor = vec4(color, 1.0);
}
