// Interface: text drawn with a BFont into a BBitmap, white on black, used as
// coverage: the red channel says how much of each pixel the glyphs cover.

uniform sampler2D uImage;
uniform vec4 uColor;

void main()
{
	float coverage = texture(uImage, vUV).r;
	fragColor = vec4(uColor.rgb, uColor.a * coverage);
}
