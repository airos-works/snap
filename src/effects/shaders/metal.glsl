// Turning Metal: the picture turns into polished chrome, spreading down from
// the top of the frame. The surface's shape comes from the picture's shading,
// so faces and folds stay recognisable as they become reflective metal. A
// beat sends a ripple across the liquid surface.

// A photo studio to reflect: sky-lit ceiling, dark floor, two soft boxes.
vec3 studio(vec3 direction)
{
	float y = direction.y;
	vec3 ceiling = mix(vec3(0.48, 0.52, 0.6), vec3(0.95, 0.97, 1.0),
		smoothstep(0.0, 0.7, y));
	vec3 ground = mix(vec3(0.05, 0.045, 0.04), vec3(0.32, 0.3, 0.28),
		smoothstep(-0.9, -0.05, y));
	vec3 color = mix(ground, ceiling, smoothstep(-0.04, 0.04, y));
	color += vec3(2.4, 2.3, 2.1)
		* smoothstep(0.22, 0.16, length(direction.xy - vec2(-0.5, 0.45)));
	color += vec3(1.3, 1.4, 1.6) * smoothstep(0.1, 0.06, abs(direction.x - 0.62))
		* smoothstep(-0.1, 0.25, direction.y);
	return color;
}

// The height of the metal surface: the picture's brightness, blurred. A
// single small mipmap level is not enough, because a smooth gradient on the
// wall behind someone is a staircase of 8-bit steps, and the slopes of a
// staircase would show as ridges in the chrome. A tent of 3 x 3 samples
// from mipmap level 2 smooths the stairs away.
float height(vec2 uv)
{
	vec2 spacing = 5.0 / uResolution;
	float sum = 0.0;
	for (int y = -1; y <= 1; y++) {
		for (int x = -1; x <= 1; x++) {
			float weight = (2.0 - abs(float(x))) * (2.0 - abs(float(y)));
			sum += weight * luma(cameraBlur(uv + vec2(x, y) * spacing, 2.0));
		}
	}
	return sum / 16.0;
}

void main()
{
	// The surface normal from the slope of the height.
	vec2 e = 3.0 / uResolution;
	float dx = height(vUV + vec2(e.x, 0.0)) - height(vUV - vec2(e.x, 0.0));
	float dy = height(vUV + vec2(0.0, e.y)) - height(vUV - vec2(0.0, e.y));

	// A ripple ring running out from the centre after each beat.
	vec2 fromCenter = (vUV - 0.5) * aspect();
	float radius = length(fromCenter);
	float ring = uBeatAge * 0.9;
	float ripple = sin((radius - ring) * 60.0)
		* smoothstep(0.08, 0.0, abs(radius - ring)) * exp(-uBeatAge * 2.5);
	vec2 rippleSlope = normalize(fromCenter + 1e-5) * ripple * 0.6;

	vec3 normal = normalize(vec3(-(dx * 9.0 + rippleSlope.x),
		-(dy * 9.0 + rippleSlope.y), 1.0));

	// Reflect the studio; brighter at grazing angles (Fresnel).
	vec3 view = vec3(0.0, 0.0, -1.0);
	vec3 reflected = reflect(view, normal);
	float fresnel = pow(1.0 - max(normal.z, 0.0), 2.0);
	vec3 chrome = studio(reflected) * (0.6 + 0.55 * height(vUV));
	chrome = mix(chrome, studio(reflected) * 1.3, fresnel);

	// A sharp highlight from the key light at the upper left.
	vec3 light = normalize(vec3(-0.5, 0.6, 0.65));
	float specular = pow(max(dot(reflect(-light, normal), -view), 0.0), 60.0);
	chrome += vec3(1.0) * specular * 1.5;
	chrome *= vec3(0.93, 0.96, 1.02);

	// The metal spreads down from the top with a ragged, molten edge.
	float front = uEffectTime * 0.4 - 0.1;
	float position = (1.0 - vUV.y) + (fbm(vUV * 5.0 + 3.0) - 0.5) * 0.3;
	float metal = smoothstep(front, front - 0.06, position);
	float rim = smoothstep(0.06, 0.0, abs(position - front + 0.03))
		* step(position, front + 0.05);

	vec3 color = mix(camera(vUV), chrome, metal);
	color += vec3(1.0, 0.85, 0.6) * rim * 0.8;
	fragColor = vec4(color, 1.0);
}
