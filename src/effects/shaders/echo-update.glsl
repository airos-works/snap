// Echo, pass 1 of 2: keeps a fading, colour-shifting copy of everything that
// passed in front of the camera, in a buffer kept from frame to frame. The
// trails drift outwards and last longer the louder it is; a beat jumps their
// colour.

uniform sampler2D uState;	// last frame's echo

void main()
{
	// Last frame's echo, a touch larger so the trails float outwards.
	vec2 fromCenter = vUV - 0.5;
	vec3 echo = texture(uState, 0.5 + fromCenter * 0.992).rgb;

	// Fade, and turn the colour wheel a little (a lot on a beat).
	float fade = 0.86 + 0.11 * uLevel;
	vec3 hsv = rgb2hsv(echo);
	hsv.x = fract(hsv.x + 0.012 + uBeat * 0.08);
	hsv.y = min(1.0, hsv.y + 0.04);
	echo = hsv2rgb(hsv) * pow(fade, uDelta * 60.0);

	// What moves now is painted on top of the echo.
	vec3 now = camera(vUV);
	float moving = smoothstep(0.08, 0.3, motion(vUV, 1.0));
	fragColor = vec4(max(echo, now * moving), 1.0);
}
