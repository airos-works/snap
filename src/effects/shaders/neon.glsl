// Neon: the outlines of the picture as glowing neon tubes over a dark room,
// coloured along the spectrum, with an equalizer of the sound below. The
// treble makes the tubes flare.

float edgeStrength(vec2 uv, float lod)
{
	vec2 e = exp2(lod) * 1.5 / uCameraSize;
	float tl = luma(cameraBlur(uv + vec2(-e.x, e.y), lod));
	float t = luma(cameraBlur(uv + vec2(0.0, e.y), lod));
	float tr = luma(cameraBlur(uv + vec2(e.x, e.y), lod));
	float l = luma(cameraBlur(uv + vec2(-e.x, 0.0), lod));
	float r = luma(cameraBlur(uv + vec2(e.x, 0.0), lod));
	float bl = luma(cameraBlur(uv + vec2(-e.x, -e.y), lod));
	float b = luma(cameraBlur(uv + vec2(0.0, -e.y), lod));
	float br = luma(cameraBlur(uv + vec2(e.x, -e.y), lod));
	// Sobel.
	float gx = (tr + 2.0 * r + br) - (tl + 2.0 * l + bl);
	float gy = (tl + 2.0 * t + tr) - (bl + 2.0 * b + br);
	return length(vec2(gx, gy));
}

void main()
{
	float fine = smoothstep(0.08, 0.45, edgeStrength(vUV, 1.0));
	float halo = smoothstep(0.05, 0.4, edgeStrength(vUV, 3.0));

	float hue = fract(vUV.y * 0.6 + vUV.x * 0.2 + uTime * 0.05);
	vec3 tube = hsv2rgb(vec3(hue, 0.75, 1.0));
	vec3 glow = hsv2rgb(vec3(fract(hue + 0.08), 0.9, 1.0));

	vec3 color = camera(vUV) * 0.12;
	color += glow * halo * (0.5 + 0.8 * uTreble);
	color += mix(tube, vec3(1.0), 0.35) * fine * (1.1 + 1.2 * uTreble);

	// The equalizer: 32 bars along the bottom, with their reflection.
	float bars = 32.0;
	float bar = floor(vUV.x * bars);
	float inBar = step(0.15, fract(vUV.x * bars)) * step(fract(vUV.x * bars), 0.85);
	float level = spectrum((bar + 0.5) / bars) * 0.3;
	float lit = step(vUV.y, level) * inBar;
	vec3 barColor = hsv2rgb(vec3(bar / bars * 0.8, 0.8, 1.0));
	color = mix(color, barColor * (0.6 + 0.6 * vUV.y / max(level, 0.01)), lit * 0.85);

	fragColor = vec4(color, 1.0);
}
