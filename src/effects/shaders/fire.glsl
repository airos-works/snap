// Set on Fire, pass 2 of 2: the camera picture lit by the flames of the heat
// buffer, with embers floating up.

uniform sampler2D uState;	// the heat buffer from pass 1
uniform vec2 uStateSize;

vec3 flameColor(float heat)
{
	// A black body: dark red, orange, yellow, then white-hot.
	return vec3(1.0, 0.22, 0.03) * smoothstep(0.05, 0.4, heat)
		+ vec3(0.0, 0.55, 0.12) * smoothstep(0.3, 0.75, heat)
		+ vec3(0.1, 0.25, 0.55) * smoothstep(0.75, 1.3, heat);
}

void main()
{
	float heat = texture(uState, vUV).r;

	// The glow of the fire on everything around it: the heat, blurred.
	vec2 texel = 6.0 / uStateSize;
	float glow = 0.0;
	for (int i = 0; i < 8; i++) {
		float angle = float(i) * PI * 0.25;
		glow += texture(uState, vUV + vec2(cos(angle), sin(angle)) * texel).r;
		glow += texture(uState, vUV + vec2(cos(angle), sin(angle)) * texel * 2.5).r;
	}
	glow /= 16.0;

	// The scene goes dim and warm, and the fire lights it up.
	vec3 scene = camera(vUV) * vec3(0.62, 0.5, 0.42);
	scene += scene * vec3(1.6, 0.8, 0.3) * glow * 1.6;

	vec3 color = scene + flameColor(heat) * 1.25;

	// Embers: a sparse grid of sparks drifting upwards, shown only above the
	// fire.
	vec2 cell = vec2(vUV.x * 48.0, vUV.y * 27.0 - uTime * 3.0);
	vec2 id = floor(cell);
	float spark = hash12(id);
	if (spark > 0.93) {
		vec2 at = fract(cell) - vec2(0.5 + 0.3 * sin(uTime * 2.0 + spark * 40.0), 0.5);
		float ember = smoothstep(0.18, 0.0, length(at));
		color += vec3(1.0, 0.6, 0.2) * ember * smoothstep(0.1, 0.4, glow) * 1.5;
	}

	fragColor = vec4(color, 1.0);
}
