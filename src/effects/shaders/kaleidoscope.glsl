// Kaleidoscope: eight mirrored slices of the picture, turning with the music
// (faster when it is louder) and zooming on the bass. The point the slices
// look at drifts slowly around the middle of the picture, so the pattern
// keeps changing even when nobody moves.

void main()
{
	vec2 position = (vUV - 0.5) * aspect();
	float radius = length(position);
	float angle = atan(position.y, position.x) + uAudioTime * 0.35;

	// Fold the angle into one slice, mirroring every other one.
	float slice = 2.0 * PI / 8.0;
	angle = mod(angle, slice);
	angle = abs(angle - slice * 0.5);

	radius *= 0.85 - 0.25 * uBass;
	vec2 drift = vec2(cos(uTime * 0.13), sin(uTime * 0.17)) * 0.18;
	vec2 uv = (vec2(cos(angle), sin(angle)) * radius + drift) / aspect() + 0.5;
	// Keep inside the picture by bouncing off its edges.
	uv = abs(mod(uv + 1.0, 2.0) - 1.0);

	vec3 color = camera(uv);
	color *= 1.0 + 0.4 * uBeat * smoothstep(0.5, 0.0, length(position));
	fragColor = vec4(color, 1.0);
}
