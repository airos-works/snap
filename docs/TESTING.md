# Testing Snap

## On the build host (Linux)

```sh
make check-host
```

builds and runs `tests/AudioAnalyzerTests.cpp` (the FFT, the band levels,
the automatic gain, beat detection on synthetic claps, tones and silence)
and compiles every shader the way Snap assembles them with
`glslangValidator` (`tools/check-shaders.sh`; Debian/Ubuntu package
`glslang-tools`).

`tools/build-cross.sh` builds `build-cross/Snap` with the air/OS cross
compiler and sysroot (paths at the top of the script).

## In a virtual machine

`tools/vm/run-vm.sh` starts a throwaway Haiku R1/beta6 VM (VNC
127.0.0.1:5912, SSH port 2232 through `tools/vm/haiku.sh`, screenshots and
input with `tools/vm/vm.py`). A VM has no GPU, so:

```sh
tools/vm/deploy.sh                      # copy build-cross/Snap in, run it with
                                        # SNAP_ALLOW_SOFTWARE_RENDERING=1
tools/vm/deploy.sh SNAP_STATS=1         # ... printing frame rate and frame time
tools/vm/sync-shaders.sh                # copy the shaders in, then:
tools/vm/deploy.sh SNAP_SHADER_DIR=/boot/home/snap-test/shaders
```

Snap's output goes to `/boot/home/snap-test/snap.log` in the VM. Drive it
with scripting through `tools/vm/haiku.sh 'hey Snap ...'` (see the README).

### Without a camera

`make test-camera` (or `tools/build-cross.sh test-camera`) builds
`SnapTestCamera`, a live producer node that calls itself a physical input,
like a real camera's node, and sends an animated B_RGB32 picture at 30
frames a second in the size asked for. Start it while Snap runs and Snap
picks it up (hot-plug); `SnapTestCamera 20` ends after 20 seconds, which
tests a camera going away while in use.

### Music

`tools/make-test-music.py beat.wav 20` writes a 20 second dance loop at
120 beats a minute. Play it with `hey Snap set Music to /path/beat.wav`;
`hey Snap get Level` and `hey Snap get Beats` show what the analysis hears.

## On real hardware

What a VM cannot show: the GPU gate and the speed on a real GPU, and a real
camera. Run Snap from a Terminal with `SNAP_STATS=1` to see frames per
second and the time a frame takes to draw. `hey Snap get Renderer` names
the OpenGL renderer. Photos taken with `hey Snap do Photo` are read back
from the GPU, so they show what the effects look like even when nobody can
look at the screen.

A check list:

- [ ] Starts on a computer with a GPU; refuses (with the renderer's name)
      on one that draws OpenGL in software
- [ ] The camera picture is live; the camera's light goes out when Snap quits
- [ ] A second program that wants the camera is refused while Snap has it,
      and Snap says so when another program has it first
- [ ] Unplugging and plugging the camera while Snap runs
- [ ] All twelve effects in the grid and big; clicking a cell picks it
- [ ] Countdown, flash, shutter sound; the photo appears in the strip
- [ ] A photo opens in ShowImage, drags to the Desktop, shows its thumbnail
      in Tracker, goes to the Trash and comes back with Tracker's Restore
- [ ] The microphone moves the level lights; claps light the beat dot
- [ ] Music plays and the effects follow it
- [ ] Settings (effect, mirror, countdown, sound, window frame) survive a
      restart
