// CRT: an old cathode-ray television. Curved glass, scanlines, an RGB shadow
// mask, phosphor glow and a slow hum bar. Sound makes the picture jitter and
// fills it with static; a beat makes the vertical hold slip.

vec2 bendGlass(vec2 uv)
{
	uv = uv * 2.0 - 1.0;
	vec2 offset = abs(uv.yx) / vec2(5.0, 4.0);
	uv += uv * offset * offset;
	return uv * 0.5 + 0.5;
}

void main()
{
	vec2 uv = bendGlass(vUV);
	if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {
		// The plastic bezel around the tube.
		fragColor = vec4(vec3(0.03), 1.0);
		return;
	}

	// The vertical hold slips on a beat: the picture rolls and a black bar
	// (the blanking interval) runs through it.
	float slip = uBeat * uBeat * 0.12;
	uv.y = fract(uv.y + slip);

	// Loud sound shakes each line sideways, like a bad antenna.
	float line = floor(uv.y * uResolution.y * 0.5);
	float shake = (noise(vec2(uTime * 50.0, line * 0.37)) - 0.5)
		* (0.002 + 0.03 * uLevel * uLevel);
	uv.x += shake;

	// The three electron beams do not quite converge towards the edges.
	vec2 fromCenter = uv - 0.5;
	vec2 spread = fromCenter * (0.006 + 0.02 * dot(fromCenter, fromCenter));
	vec3 color;
	color.r = camera(uv + spread).r;
	color.g = camera(uv).g;
	color.b = camera(uv - spread).b;

	// Phosphor glow from the blurred picture.
	vec3 glow = cameraBlur(uv, 4.0);
	color += glow * glow * 0.45;

	// Scanlines: about 300 visible lines, whatever the output size.
	float lines = min(uResolution.y * 0.5, 300.0);
	float scan = 0.5 + 0.5 * sin(uv.y * lines * 2.0 * PI);
	color *= 0.62 + 0.38 * scan;

	// The shadow mask: red, green and blue phosphor stripes.
	float stripe = mod(gl_FragCoord.x, 3.0);
	vec3 mask = vec3(stripe < 1.0 ? 1.15 : 0.75,
		stripe >= 1.0 && stripe < 2.0 ? 1.15 : 0.75,
		stripe >= 2.0 ? 1.15 : 0.75);
	color *= mask;

	// A hum bar crawling up the screen, and the blanking bar of a slip.
	float hum = 1.0 - 0.12 * smoothstep(0.35, 0.0,
		abs(fract(uv.y * 0.5 - uTime * 0.07) - 0.5));
	color *= hum;
	color *= smoothstep(0.0, 0.025, uv.y) * smoothstep(1.0, 0.975, uv.y)
		* (1.0 - step(0.001, slip) * smoothstep(0.03, 0.0, abs(uv.y - slip)));

	// Static: always a little, a lot when it is loud.
	float snow = hash12(gl_FragCoord.xy + vec2(fract(uTime) * 1931.0, 0.0));
	color = mix(color, vec3(snow), 0.04 + 0.4 * uLevel * uLevel * uLevel);

	// Darker corners, and a faint warm tint of old phosphor.
	float vignette = 16.0 * uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y);
	color *= pow(vignette, 0.3) * vec3(1.02, 1.0, 0.94);

	fragColor = vec4(color, 1.0);
}
