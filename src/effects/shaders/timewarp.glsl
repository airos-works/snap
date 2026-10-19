// Time Warp: every row of the picture comes from a different moment. The top
// is now, further down is further back in time (up to about 1.5 s), so
// anything that moves gets stretched and bent. Sound bends the boundary.

uniform sampler2DArray uHistory;	// the last camera pictures, a ring
uniform float uHistoryLayers;		// size of the ring
uniform float uHistoryNewest;		// layer of the newest picture
uniform float uHistoryCount;		// layers filled so far

vec3 pictureAt(vec2 uv, float age)
{
	// Blend the two pictures either side of the age for smooth motion.
	age = clamp(age, 0.0, uHistoryCount - 1.0);
	float older = floor(age);
	float layerA = mod(uHistoryNewest - older + uHistoryLayers, uHistoryLayers);
	float layerB = mod(uHistoryNewest - min(older + 1.0, uHistoryCount - 1.0)
		+ uHistoryLayers, uHistoryLayers);
	vec2 tex = cameraUV(uv);
	vec3 a = texture(uHistory, vec3(tex, layerA)).rgb;
	vec3 b = texture(uHistory, vec3(tex, layerB)).rgb;
	return mix(a, b, fract(age));
}

void main()
{
	float depth = 1.0 - vUV.y;
	depth += sin(vUV.x * 9.0 + uTime * 2.0) * 0.08 * (0.3 + uLevel);
	float age = clamp(depth, 0.0, 1.0) * (uHistoryLayers - 2.0);
	fragColor = vec4(pictureAt(vUV, age), 1.0);
}
