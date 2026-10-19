// Echo, pass 2 of 2: the camera with the trails of pass 1 laid over it.

uniform sampler2D uState;	// the echo buffer

void main()
{
	vec3 now = camera(vUV);
	vec3 echo = texture(uState, vUV).rgb;
	vec3 color = max(now * 0.85, echo) + echo * 0.25;
	fragColor = vec4(color, 1.0);
}
