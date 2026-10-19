// Interface: a picture in a rounded rectangle with an optional border, for the
// effect previews in the grid and the stage itself.

uniform sampler2D uImage;
uniform vec2 uSize;			// rectangle size in pixels
uniform float uRadius;		// corner radius in pixels
uniform float uBorderWidth;	// in pixels, 0 for none
uniform vec4 uBorderColor;
uniform float uOpacity;

float roundedBox(vec2 position, vec2 halfSize, float radius)
{
	vec2 q = abs(position) - halfSize + radius;
	return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - radius;
}

void main()
{
	vec2 position = (vUV - 0.5) * uSize;
	float edge = roundedBox(position, uSize * 0.5, uRadius);
	float inside = clamp(0.5 - edge, 0.0, 1.0);
	vec4 color = vec4(texture(uImage, vUV).rgb, 1.0);
	float border = clamp(0.5 - abs(edge + uBorderWidth * 0.5)
		+ uBorderWidth * 0.5, 0.0, 1.0) * step(0.01, uBorderWidth);
	color = mix(color, uBorderColor, border * uBorderColor.a);
	fragColor = vec4(color.rgb, inside * uOpacity);
}
