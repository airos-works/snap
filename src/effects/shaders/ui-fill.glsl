// Interface: a flat colour in a rounded rectangle (label plates, the flash,
// dimming behind the countdown).

uniform vec4 uColor;
uniform vec2 uSize;
uniform float uRadius;

float roundedBox(vec2 position, vec2 halfSize, float radius)
{
	vec2 q = abs(position) - halfSize + radius;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
	vec2 position = (vUV - 0.5) * uSize;
	float inside = clamp(0.5 - roundedBox(position, uSize * 0.5, uRadius), 0.0, 1.0);
	fragColor = vec4(uColor.rgb, uColor.a * inside);
}
