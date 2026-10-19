// Pop Art: four prints of the picture side by side in loud flat colours with
// halftone dots, like a silkscreen. The colour sets move along one print on
// every beat.

vec3 swatch(int set, int tone)
{
	// Eight sets of shadow, middle and highlight colours.
	vec3 table[24] = vec3[24](
		vec3(0.10, 0.05, 0.35), vec3(0.95, 0.20, 0.50), vec3(1.00, 0.90, 0.20),
		vec3(0.05, 0.25, 0.20), vec3(1.00, 0.55, 0.05), vec3(0.55, 0.95, 1.00),
		vec3(0.35, 0.00, 0.10), vec3(0.20, 0.70, 0.95), vec3(1.00, 0.95, 0.85),
		vec3(0.00, 0.10, 0.30), vec3(0.95, 0.85, 0.10), vec3(1.00, 0.45, 0.70),
		vec3(0.20, 0.00, 0.30), vec3(0.20, 0.85, 0.40), vec3(1.00, 0.70, 0.10),
		vec3(0.30, 0.05, 0.05), vec3(1.00, 0.30, 0.15), vec3(0.95, 0.95, 0.55),
		vec3(0.05, 0.05, 0.10), vec3(0.55, 0.30, 0.95), vec3(0.40, 1.00, 0.85),
		vec3(0.15, 0.20, 0.45), vec3(1.00, 0.60, 0.65), vec3(0.95, 1.00, 0.70));
	return table[set * 3 + tone];
}

void main()
{
	// Which of the four prints, and where in it.
	vec2 tile = floor(vUV * 2.0);
	vec2 uv = fract(vUV * 2.0);
	int index = int(tile.x + (1.0 - tile.y) * 2.0);
	int set = int(mod(float(index) * 2.0 + uBeatCount, 8.0));

	float tone = luma(cameraBlur(uv, 1.0));
	tone = smoothstep(0.1, 0.9, tone);

	// Halftone dots on the middle tone: bigger dots where it is darker.
	vec2 grid = vUV * uResolution / max(uResolution.y / 90.0, 3.0);
	float dots = length(fract(grid) - 0.5);
	float dotRadius = mix(0.55, 0.12, tone);

	vec3 color;
	if (tone < 0.33)
		color = swatch(set, 0);
	else if (tone < 0.66)
		color = mix(swatch(set, 1), swatch(set, 0), step(dots, dotRadius) * 0.45);
	else
		color = swatch(set, 2);

	// A thin white gutter between the prints.
	vec2 gutter = abs(vUV - 0.5) * uResolution;
	if (min(gutter.x, gutter.y) < max(uResolution.y / 270.0, 1.0))
		color = vec3(1.0);

	fragColor = vec4(color, 1.0);
}
