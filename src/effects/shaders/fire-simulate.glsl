// Set on Fire, pass 1 of 2: the heat simulation, in a buffer of its own that
// keeps its contents from frame to frame.
//
// Every frame the old heat rises and drifts with turbulence, spreads a little
// and cools. Fresh heat comes from what moves in front of the camera and from
// the outline of the person, so the flames lick upwards off their shoulders
// and hands. Bass (or a loud room) feeds the fire.

uniform sampler2D uState;	// last frame's heat in .r
uniform vec2 uStateSize;	// heat buffer size in pixels

float heatAt(vec2 uv)
{
	return texture(uState, uv).r;
}

void main()
{
	vec2 texel = 1.0 / uStateSize;
	float dt = clamp(uDelta, 0.0, 0.1);

	// Turbulence: a slowly churning noise field pushes the heat sideways.
	float swirl = fbm(vec2(vUV.x * 5.0, vUV.y * 3.0 - uTime * 1.3)) - 0.5;

	// Advect: take the heat from where it was a moment ago (below). Each
	// pixel rises by a slightly different random amount, so that heat
	// rising several pixels a frame (at a low frame rate) smears into
	// tongues instead of leaving copies of the outline every few pixels.
	float jitter = hash12(vUV * uStateSize + fract(uTime * 7.31) * 911.0);
	vec2 drift = vec2(swirl * 0.9, 0.5 + jitter) * (0.42 * dt);
	vec2 from = vUV - drift;

	// ... and smudge it across its neighbours.
	vec2 spread = texel * 1.5;
	float heat = heatAt(from) * 0.5
		+ (heatAt(from + vec2(spread.x, 0.0)) + heatAt(from - vec2(spread.x, 0.0))
			+ heatAt(from + vec2(0.0, spread.y)) + heatAt(from - vec2(0.0, spread.y)))
			* 0.125;

	// Cooling, uneven so the flames break into tongues.
	float flicker = noise(vec2(vUV.x * 18.0, vUV.y * 9.0 - uTime * 4.0));
	heat -= (0.35 + 1.4 * flicker) * dt;

	// Fuel: motion, and the edges of what is in front of the camera.
	float fuel = motion(vUV, 2.0) * 1.4;
	vec2 e = 2.0 / uCameraSize;
	float gx = luma(cameraBlur(vUV + vec2(e.x, 0.0), 2.0))
		- luma(cameraBlur(vUV - vec2(e.x, 0.0), 2.0));
	float gy = luma(cameraBlur(vUV + vec2(0.0, e.y), 2.0))
		- luma(cameraBlur(vUV - vec2(0.0, e.y), 2.0));
	float edge = smoothstep(0.04, 0.2, length(vec2(gx, gy)));
	fuel += edge * (0.35 + 0.25 * noise(vUV * 40.0 + uTime * 3.0));
	// The fire builds up over the first two seconds.
	fuel *= smoothstep(0.0, 2.0, uEffectTime) * (0.75 + 0.8 * max(uBass, uLevel));

	fragColor = vec4(clamp(max(heat, fuel), 0.0, 1.5), 0.0, 0.0, 1.0);
}
