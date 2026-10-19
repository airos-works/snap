// The stand-in for a camera: a cheerful animated scene the effects run on
// while there is no camera (none connected, or another program has it). It
// gives them shapes, edges, colour and motion to work with.
//
// It is drawn into a texture that is then read like a camera picture, whose
// first row is the top: so it is drawn upside down here.

float circle(vec2 p, vec2 center, float radius)
{
	return smoothstep(radius + 0.004, radius - 0.004, length(p - center));
}

void main()
{
	vec2 uv = vec2(vUV.x, 1.0 - vUV.y);
	vec2 p = (uv - 0.5) * aspect();

	// A slow sunset gradient with drifting blobs of colour.
	vec3 color = mix(vec3(0.16, 0.1, 0.35), vec3(0.95, 0.5, 0.35), uv.y);
	for (int i = 0; i < 5; i++) {
		float fi = float(i);
		vec2 center = vec2(sin(uTime * 0.2 + fi * 1.7) * 0.7,
			cos(uTime * 0.17 + fi * 2.3) * 0.35);
		color = mix(color, hsv2rgb(vec3(fract(fi * 0.21 + 0.55), 0.6, 0.9)),
			circle(p, center, 0.12 + 0.03 * fi) * 0.35);
	}

	// A friendly face swaying from side to side.
	vec2 head = vec2(sin(uTime * 0.8) * 0.25, 0.02);
	float face = circle(p, head, 0.26);
	color = mix(color, vec3(1.0, 0.82, 0.62), face);
	float blink = step(0.06, fract(uTime * 0.23));
	color = mix(color, vec3(0.1), circle(p, head + vec2(-0.09, 0.07), 0.035 * blink + 0.004));
	color = mix(color, vec3(0.1), circle(p, head + vec2(0.09, 0.07), 0.035 * blink + 0.004));
	vec2 mouth = p - head - vec2(0.0, -0.02);
	float smile = smoothstep(0.012, 0.0, abs(length(mouth) - 0.13))
		* step(mouth.y, -0.06);
	color = mix(color, vec3(0.55, 0.12, 0.15), smile);

	// Colour bars along the bottom, as on a test card.
	if (uv.y < 0.1) {
		float bar = floor(uv.x * 8.0);
		color = hsv2rgb(vec3(bar / 8.0, 0.7, 0.9));
	}

	fragColor = vec4(color, 1.0);
}
