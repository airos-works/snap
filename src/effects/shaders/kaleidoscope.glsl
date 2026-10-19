// Kaleidoscope: eight mirrored slices of the middle of the picture, turning
// with the music (faster when it is louder) and zooming on the bass.

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
	vec2 uv = vec2(cos(angle), sin(angle)) * radius;
	uv = uv / aspect() + 0.5;
	// Keep inside the picture by bouncing off its edges.
	uv = abs(mod(uv + 1.0, 2.0) - 1.0);

	vec3 color = camera(uv);
	color *= 1.0 + 0.4 * uBeat * smoothstep(0.5, 0.0, radius);
	fragColor = vec4(color, 1.0);
}
