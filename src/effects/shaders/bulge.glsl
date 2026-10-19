// Bass Bulge: a fish-eye bulge in the middle of the picture that swells with
// the bass, and a ripple that runs out from it on every beat.

void main()
{
	vec2 position = (vUV - 0.5) * aspect();
	float radius = length(position);
	float reach = 0.55;

	if (radius < reach) {
		// Pull the sample point towards the centre: magnified near it.
		float strength = 0.45 + 0.5 * uBass;
		float t = radius / reach;
		position *= mix(pow(t, strength), 1.0, t * t);
	}

	// The beat's ripple: a ring that travels outwards and fades.
	float ring = uBeatAge * 0.8;
	float wave = sin((radius - ring) * 45.0)
		* smoothstep(0.1, 0.0, abs(radius - ring)) * exp(-uBeatAge * 2.0);
	position *= 1.0 + wave * 0.04;

	vec2 uv = position / aspect() + 0.5;
	fragColor = vec4(camera(clamp(uv, 0.0, 1.0)), 1.0);
}
