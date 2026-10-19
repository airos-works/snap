// Thermal Camera: brightness as heat on an "iron" palette, with whatever
// moves glowing hotter. The scale on the right warms up with the noise in
// the room.

vec3 ironPalette(float t)
{
	t = clamp(t, 0.0, 1.0);
	vec3 color = mix(vec3(0.0, 0.0, 0.08), vec3(0.3, 0.0, 0.55), smoothstep(0.0, 0.2, t));
	color = mix(color, vec3(0.85, 0.05, 0.45), smoothstep(0.15, 0.4, t));
	color = mix(color, vec3(1.0, 0.45, 0.0), smoothstep(0.35, 0.65, t));
	color = mix(color, vec3(1.0, 0.9, 0.15), smoothstep(0.6, 0.85, t));
	return mix(color, vec3(1.0), smoothstep(0.85, 1.0, t));
}

void main()
{
	float heat = luma(cameraBlur(vUV, 1.5));
	heat = pow(heat, 1.15) * 0.9 + motion(vUV, 2.5) * 0.6 + uLevel * 0.12;
	vec3 color = ironPalette(heat);

	// A crosshair in the middle.
	vec2 pixel = (vUV - 0.5) * uResolution;
	float size = uResolution.y * 0.04;
	float crosshair = step(abs(pixel.x), 1.0) * step(abs(pixel.y), size)
		* step(size * 0.3, abs(pixel.y))
		+ step(abs(pixel.y), 1.0) * step(abs(pixel.x), size)
		* step(size * 0.3, abs(pixel.x));
	color = mix(color, vec3(1.0), clamp(crosshair, 0.0, 1.0) * 0.8);

	// The colour scale at the right edge.
	float barLeft = 1.0 - 36.0 / uResolution.x;
	float barRight = 1.0 - 24.0 / uResolution.x;
	if (vUV.x > barLeft && vUV.x < barRight && vUV.y > 0.15 && vUV.y < 0.85) {
		float t = (vUV.y - 0.15) / 0.7;
		color = ironPalette(t + uLevel * 0.1);
	}

	fragColor = vec4(color, 1.0);
}
