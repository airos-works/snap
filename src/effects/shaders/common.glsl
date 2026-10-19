// Snap's shader prelude, put in front of every fragment shader (after the
// #version line). It declares what Snap hands every effect and a few helpers.
//
// Coordinates: vUV runs 0..1 across the output with (0, 0) at the bottom left,
// as OpenGL has it. The output always has the camera's aspect ratio, so vUV
// and the camera picture line up; use aspect() for round shapes.

in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uCamera;		// the newest camera picture, with mipmaps
uniform sampler2D uPrevious;	// the picture before it, for motion
uniform sampler2D uAudio;		// row 0: spectrum (low..high), row 1: waveform
uniform vec2 uResolution;		// output size in pixels
uniform vec2 uCameraSize;		// camera picture size in pixels
uniform float uTime;			// seconds since Snap started
uniform float uEffectTime;		// seconds since this effect came on screen
uniform float uDelta;			// seconds since the previous frame
uniform float uMirror;			// 1.0 shows the camera like a mirror

// The sound, every value 0..1 (see AudioAnalyzer).
uniform float uLevel;			// loudness
uniform float uBass;			// 20 - 250 Hz
uniform float uMid;				// 250 - 2000 Hz
uniform float uTreble;			// 2 - 12 kHz
uniform float uBeat;			// 1.0 on a beat, falling off quickly
uniform float uBeatAge;			// seconds since the last beat
uniform float uBeatCount;		// beats so far
uniform float uAudioTime;		// a clock that runs faster the louder it is

const float PI = 3.14159265;

// Multiply a vUV difference by this to measure it in screen-height units.
vec2 aspect()
{
	return vec2(uResolution.x / uResolution.y, 1.0);
}

// Output coordinates to camera texture coordinates: the texture's first row
// is the top of the picture, and the mirror swaps left and right.
vec2 cameraUV(vec2 uv)
{
	return vec2(mix(uv.x, 1.0 - uv.x, uMirror), 1.0 - uv.y);
}

vec3 camera(vec2 uv)
{
	return texture(uCamera, cameraUV(uv)).rgb;
}

// A blurred camera picture from the mipmaps: lod 1 halves the resolution,
// each further step halves it again.
vec3 cameraBlur(vec2 uv, float lod)
{
	return textureLod(uCamera, cameraUV(uv), lod).rgb;
}

vec3 previousCamera(vec2 uv)
{
	return texture(uPrevious, cameraUV(uv)).rgb;
}

float luma(vec3 color)
{
	return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

// How much changed here since the previous camera picture, 0..1.
float motion(vec2 uv, float lod)
{
	vec3 now = textureLod(uCamera, cameraUV(uv), lod).rgb;
	vec3 before = textureLod(uPrevious, cameraUV(uv), lod).rgb;
	return clamp(length(now - before) * 2.5, 0.0, 1.0);
}

float spectrum(float x)
{
	return texture(uAudio, vec2(x, 0.25)).r;
}

float waveform(float x)
{
	return texture(uAudio, vec2(x, 0.75)).r;
}

// Hashes and value noise for fire, static and the like.
float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

float noise(vec2 p)
{
	vec2 i = floor(p);
	vec2 f = fract(p);
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(hash12(i), hash12(i + vec2(1.0, 0.0)), u.x),
		mix(hash12(i + vec2(0.0, 1.0)), hash12(i + vec2(1.0, 1.0)), u.x), u.y);
}

float fbm(vec2 p)
{
	float sum = 0.0;
	float amplitude = 0.5;
	for (int octave = 0; octave < 5; octave++) {
		sum += amplitude * noise(p);
		p = p * 2.03 + vec2(17.0, 9.0);
		amplitude *= 0.5;
	}
	return sum;
}

vec3 hsv2rgb(vec3 c)
{
	vec3 p = abs(fract(c.xxx + vec3(1.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
	return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}

vec3 rgb2hsv(vec3 c)
{
	vec4 k = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
	vec4 p = mix(vec4(c.bg, k.wz), vec4(c.gb, k.xy), step(c.b, c.g));
	vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
	float d = q.x - min(q.w, q.y);
	return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + 1e-10)),
		d / (q.x + 1e-10), q.x);
}
