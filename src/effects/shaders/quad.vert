// Snap draws everything as rectangles, without any vertex buffer: the four
// corners of a triangle strip come from gl_VertexID (0..3).

uniform vec4 uRect;		// x0, y0, x1, y1 in normalized device coordinates
uniform vec4 uTexRect;	// the vUV values at those corners

out vec2 vUV;

void main()
{
	vec2 corner = vec2(float(gl_VertexID & 1), float(gl_VertexID >> 1));
	vUV = mix(uTexRect.xy, uTexRect.zw, corner);
	gl_Position = vec4(mix(uRect.xy, uRect.zw, corner), 0.0, 1.0);
}
