# How Snap works

Snap is about 7,800 lines of C++ (headers and comments included) and 800
lines of GLSL. This document walks through its parts, the threads they run
on, and the less obvious decisions (most of them forced by something learned
the hard way, listed at the end).

## The parts

```
                         ┌──────────────────────────── SnapWindow (window thread) ──┐
 camera node ──buffers──▶│ CameraSource ──▶ FrameExchange ─┐                         │
 (usb_webcam add-on)     │   BMediaRecorder               │                         │
                         │                                 ▼                         │
 sound card ──buffers───▶│ MicrophoneInput ─┐          StageView (BGLView)          │
                         │   BMediaRecorder  ├▶ SoundAnalysis ─▶ render thread:      │
 music file ─decode─────▶│ MusicPlayer ──────┘   (AudioAnalyzer)   StageRenderer     │
                         │   BMediaFile, BSoundPlayer               └─ Effects (GLSL)│
                         │                                               │ photo    │
                         │ PhotoStrip ◀── node monitor ── ~/Pictures/Snap ◀── PhotoLibrary (BLooper)
                         └──────────────────────────────────────────────────────────┘
```

| Class | Thread | Job |
|---|---|---|
| `SnapApp` | application | Checks the GPU (`GPUCheck`), opens the window, passes scripting on |
| `SnapWindow` | window | Menus, controls, settings; owns and connects everything below |
| `CameraSource` | its recorder node's | Connects a `BMediaRecorder` to a camera node, copies each picture into the `FrameExchange` |
| `FrameExchange` | any | Triple buffer: the newest picture, handed over without waiting |
| `MicrophoneInput` | its recorder node's | `BMediaRecorder` on the default sound input, feeding `SoundAnalysis` |
| `MusicPlayer` | the sound player's | Decodes a file, plays it with `BSoundPlayer`, feeds `SoundAnalysis` |
| `SoundAnalysis` / `AudioAnalyzer` | the feeding thread | FFT, levels, bands, beats; the renderer copies the result |
| `SoundEffects` | Game Kit's | Countdown ticks and the shutter, `BSimpleGameSound` |
| `StageView` | window + render | The `BGLView`; its own thread draws 60 frames a second |
| `StageRenderer` | render | All the OpenGL: camera texture, effects, grid, overlays, photo read-back |
| `Effect` and friends | render | One look each; mostly a GLSL fragment shader |
| `PhotoLibrary` | its own (`BLooper`) | Encodes PNGs, writes attributes and thumbnails, loads thumbnails |
| `PhotoStrip` | window | The photos under the stage, kept in sync by the node monitor |

Nothing is shared between threads except through these few, small, locked
hand-over points: `FrameExchange` (pictures), `SoundAnalysis` (sound
features), `StageView`'s `StageState` (what to show) and messages.

## A frame

The render thread (`StageView::_RenderLoop()`) runs this 60 times a second:

1. Copy the `StageState` the window last set (effect, grid, mirror,
   countdown, flash, status line, photo requests).
2. `LockGL()`, then `StageRenderer::Render()`:
   1. **Sound**: copy the latest `AudioFeatures`; note beats; upload the
      spectrum and waveform into a 128×2 float texture.
   2. **Camera**: take the newest picture from the `FrameExchange`, if
      there is one, and upload it into the older of two mipmapped textures,
      so the other one is "the picture before" (for motion). Without a
      camera, the placeholder shader draws a test scene into them instead.
   3. **Layout**: the stage keeps the camera's shape; the grid has four
      columns of cells of the same shape. The cell rectangles are kept for
      `CellAt()`, which the window thread calls for clicks.
   4. **Effects**: every effect on screen draws into its own offscreen
      `RenderTarget`: at the camera's resolution on the stage (that picture
      *is* the photo), at the cell's size in the grid. An effect that was
      not on screen the frame before is `Reset()` first.
   5. **Photo**: if one was asked for, `glReadPixels()` the effect's target
      into a `BBitmap` and post it to the window (`kMsgPhotoTaken`).
   6. **Composite**: the target(s) onto the view, then labels, the
      countdown, the status line and the flash, alpha-blended.
3. `SwapBuffers()`, `UnlockGL()`, and sleep until the next 1/60 s.

## Effects

`src/effects/EffectCatalog.cpp` lists them (id, name, descriptions) for
the interface; `CreateEffect()` in `Effects.cpp` makes the render-thread
object for each:

- **ShaderEffect**: one fragment shader over the output (CRT, Turning
  Metal, Thermal, Pop Art, Normal, Neon, Kaleidoscope, Bass Bulge).
- **FeedbackEffect**: an *update* shader computes a new state from the old
  one into a `PingPongTarget`, a *display* shader draws the picture from
  the state and the camera. Set on Fire keeps heat in a half-resolution
  RGBA16F buffer (8 bits would make the cooling step); Echo keeps its trails.
- **TimeWarpEffect**: reads a `CameraHistory`, a ring of the last 48
  camera pictures in a `GL_TEXTURE_2D_ARRAY`, filled by
  `glBlitFramebuffer()` (which also scales them down) only while Time Warp
  is on screen.
- **TextModeEffect**: reads a ramp of 16 glyphs of `be_fixed_font`,
  chosen out of 80 candidates by how much ink each one has, drawn by
  app_server into a `BBitmap` (`MakeGlyphRamp()`).

Every fragment shader is compiled as `#version 140`, then
`common.glsl` (the uniforms Snap provides and helper functions), then the
effect. [EFFECTS.md](EFFECTS.md) describes the uniforms and how to add an
effect.

The shaders are part of the application file: the Makefile writes
`Shaders.rdef` with one `'GLSL'` resource per file in
`src/effects/shaders`, and `ShaderLibrary` reads them back with
`BResources`. `listres Snap` shows them. With `SNAP_SHADER_DIR` set, Snap
reads them from that folder instead and builds an effect again whenever it
comes back on screen, so shaders can be changed without building Snap.

## Sound

`AudioAnalyzer` is plain C++ (and unit-tested on Linux). Every 512 samples
it runs a Hann-windowed 1024-point FFT and updates:

- `level` (RMS), `bass` (20–250 Hz), `mid` (250–2000 Hz), `treble`
  (2–12 kHz), each divided by its own slowly falling peak (automatic
  gain, with a floor so that a quiet room stays quiet);
- 32 log-spaced spectrum bands in decibels below the loudest recent band;
- beats: the summed rise of log-compressed magnitudes (spectral flux)
  above 1.6 times its average over the last half second, at most one per
  170 ms.

Values rise fast and fall slowly, so the effects jump with the sound and
calm down gently. Two more values are made by the renderer: `uBeatAge`
(seconds since the last beat, for ripples that travel) and `uAudioTime`
(a clock that runs faster the louder it is, for spinning).

The microphone and the music both feed one `SoundAnalysis`. Only one is
on at a time; switching stops one and resets the analysis.

## Photos

The photo is the effect's offscreen target, read back on the render thread
(so the overlays are never in it), sent as a `BBitmap*` in a message to the
window, which passes it to the `PhotoLibrary` looper. That thread encodes
it with the Translation Kit into `~/Pictures/Snap/Snap <date> <time>.png`,
sets `BEOS:TYPE`, writes `Media:Width`, `Media:Height`, `Snap:Effect` and
Tracker's own thumbnail attributes (`Media:Thumbnail`, a 128-pixel WebP,
and `Media:Thumbnail:CreationTime`), and answers with the file's
`entry_ref`. Tracker shows the thumbnail without decoding the PNG, and so
does the strip, whose thumbnails come from the same attribute.

## Things learned the hard way

- **Only hardware GPUs.** The live effects cost a few milliseconds a frame
  on a GPU (1.4 ms for one, 2 ms for the twelve of the grid on a Radeon
  760M) and many times that in llvmpipe. `GPUCheck` makes a hidden window
  with a `BGLView`, reads `GL_RENDERER` and `GL_VERSION`, and refuses
  software renderers (llvmpipe, softpipe, lavapipe) and anything below
  OpenGL 3.1. `SNAP_ALLOW_SOFTWARE_RENDERING=1` lets Snap run anyway, for
  working on it in a virtual machine.
- **`SwapBuffers()` does not wait for the display** on the EGL-based
  `BGLView` (AMD, ARM boards), so the render thread paces itself.
- **The viewport is the application's job after a resize**: `FrameResized()`
  only records the size; the render thread sets `glViewport()` every frame.
- **Draw from another thread, but stop it carefully.** `BGLView` shows the
  finished frame on the window's thread, so `DetachedFromWindow()` unlocks
  the window while it waits for the render thread to end (as GLTeapot does),
  and the window stops it in its destructor before anything it reads goes.
- **One consumer per camera.** A camera node refuses a second connection;
  Snap says the camera is in use and runs on the test scene. A program that
  crashed while using a camera leaves its output connected to a dead port:
  `FindFreeOutput()` disconnects such outputs (the media services don't).
- **A USB microphone must be started as a node.** `BMediaRecorder::Start()`
  starts only the producer's time source, which is enough for a sound card
  that also plays; `MicrophoneInput` starts the input node too.
- **The system mixer has one output** (to the speakers), so another
  program's sound cannot be tapped. Snap plays music itself and analyses
  exactly the buffers it gives the mixer.
- **Many media nodes come and go** (every program that plays a sound makes
  some, and so does Snap). Snap reacts to `B_MEDIA_NODE_CREATED` /
  `DELETED` only when the list of cameras changed; otherwise its own
  connection attempts would trigger new attempts.
- **A file appears before it is finished.** The node monitor reports a new
  photo before the library has written its type and attributes, so the
  window tells the strip about each saved photo, and the strip accepts
  image files by name while they have no type yet.
- **8-bit gradients are staircases.** Turning Metal takes surface slopes
  from the picture's brightness; taken from one mipmap level, the steps of
  a smooth wall became ridges. A tent filter over several samples of a small
  mipmap level gives a smooth height.
- **Advect with jitter.** Set on Fire moves its heat up by a few pixels a
  frame; moved by the same amount everywhere, an outline that is fuel every
  frame leaves visible copies of itself. A random rise per pixel turns them
  into flames, at any frame rate.
- **Offscreen `B_RGBA32` bitmaps have no reliable alpha** in app_server, so
  text for OpenGL is drawn white on black into `B_RGB32` and the red channel
  is used as coverage.
- **A scripting target in another looper** cannot be returned from
  `ResolveSpecifier()` (`BLooper` drops it); `SnapApp` sends the message on
  to the window, as `BApplication` does for "Window" specifiers.
