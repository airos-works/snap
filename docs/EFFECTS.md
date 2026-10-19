# Writing an effect

An effect is, in most cases, one GLSL fragment shader. This is how to add
one, using a made-up "Duotone" as the example.

## 1. The shader

Create `src/effects/shaders/duotone.glsl`. Snap puts `#version 140` and
[`common.glsl`](../src/effects/shaders/common.glsl) in front of it, so the
shader only needs a `main()` that writes `fragColor` for the output pixel at
`vUV`:

```glsl
// Duotone: the picture in two colours, which swap on every beat.

void main()
{
	float tone = smoothstep(0.2, 0.8, luma(camera(vUV)));
	vec3 dark = vec3(0.1, 0.0, 0.3);
	vec3 light = vec3(1.0, 0.5, 0.2);
	if (mod(uBeatCount, 2.0) == 1.0) {
		vec3 swap = dark;
		dark = light;
		light = swap;
	}
	fragColor = vec4(mix(dark, light, tone), 1.0);
}
```

What `common.glsl` gives every shader:

| Name | What |
|---|---|
| `vUV` | 0..1 across the output, (0, 0) at the bottom left. The output has the camera's shape. |
| `camera(uv)`, `cameraBlur(uv, lod)` | The camera picture (mirrored if the user wants); blurred from its mipmaps, each `lod` step halves the resolution |
| `previousCamera(uv)`, `motion(uv, lod)` | The picture before, and how much changed since (0..1) |
| `uResolution`, `uCameraSize` | Output and camera size in pixels |
| `aspect()` | Multiply a `vUV` difference by it to measure in screen heights (for circles) |
| `uTime`, `uEffectTime`, `uDelta` | Seconds since Snap started, since this effect came on screen, since the last frame |
| `uLevel`, `uBass`, `uMid`, `uTreble` | The sound, 0..1 |
| `uBeat`, `uBeatAge`, `uBeatCount` | 1.0 on a beat falling to 0; seconds since the last beat; beats so far |
| `uAudioTime` | A clock that runs faster when it is loud: spin things with it |
| `spectrum(x)`, `waveform(x)` | The spectrum (x from low to high frequencies) and the sound wave, from a texture |
| `luma()`, `hash12()`, `noise()`, `fbm()`, `hsv2rgb()`, `rgb2hsv()` | Helpers |

## 2. The catalog

Add a line to `kEffects` in
[`EffectCatalog.cpp`](../src/effects/EffectCatalog.cpp): the id (the shader
is found as `<id>.glsl`, and photos and settings store it), the name in the
interface, a description, and what sound does to it (or `NULL`). The order
of the list is the order of the grid and the Effects menu. With more than
twelve effects the grid simply gets more rows.

That is all for a one-shader effect: the Makefile puts every `.glsl` file
into the application's resources, and `CreateEffect()` makes a
`ShaderEffect` for any id it has no special class for.

## 3. Trying it

`tools/check-shaders.sh` (or `make check-host`) compiles every shader with
`glslangValidator` on Linux. On Haiku, run Snap with the shaders read from
the source tree:

```sh
SNAP_SHADER_DIR=$PWD/src/effects/shaders build-haiku/Snap
```

Edit the shader and switch to another effect and back: Snap builds it again
from the file. Compile errors go to the Terminal, with the line numbers of
your file (file 2; common.glsl is file 1).

## Effects with more than a shader

`Effects.cpp` has the other kinds; use them as models.

- **Memory from frame to frame** (Set on Fire, Echo): a `FeedbackEffect`
  takes two shaders. The *update* shader reads last frame's state as
  `uState` (size `uStateSize`) and writes the new state; the *display*
  shader reads the new state and draws the picture. The state can be kept
  at a fraction of the output size and in a float format.
- **The past** (Time Warp): override `NeedsHistory()` to have the renderer
  keep the last 48 camera pictures in a texture array, and bind it in
  `SetUniforms()`.
- **Textures of your own** (Text Mode): bind them in `SetUniforms()` to
  texture units from `kEffectTextureUnit` on.

An effect draws at whatever size its output has: the camera's resolution
on the big stage (that picture is what a photo saves), a few hundred pixels
in the grid. Use `uResolution` rather than pixel counts in shaders, so it
looks the same in both.
