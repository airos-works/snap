#!/usr/bin/env python3
"""Writes a short synthetic dance loop (WAV) for testing Snap's music input:
120 beats a minute, a kick on every beat, a bass line and off-beat hi-hats.

Usage: make-test-music.py output.wav [seconds]
"""
import math
import random
import struct
import sys
import wave

RATE = 44100


def render(seconds):
    random.seed(1)
    frames = int(RATE * seconds)
    beat = RATE // 2                       # 120 beats a minute
    bass_notes = [55.0, 55.0, 65.41, 73.42]  # A1 A1 C2 D2, one per bar beat
    out = []
    for i in range(frames):
        t_beat = (i % beat) / RATE
        beat_index = i // beat
        # Kick: a falling sine with a fast decay.
        kick = math.sin(2 * math.pi * (50 + 120 * math.exp(-t_beat * 30)) * t_beat) \
            * math.exp(-t_beat * 9)
        # Bass: a soft square-ish tone.
        f = bass_notes[beat_index % 4]
        bass = 0.35 * math.tanh(3 * math.sin(2 * math.pi * f * i / RATE)) \
            * (0.6 + 0.4 * math.exp(-t_beat * 4))
        # Hi-hat on the off-beat: short bright noise.
        t_off = ((i + beat // 2) % beat) / RATE
        hat = (random.random() * 2 - 1) * 0.25 * math.exp(-t_off * 60)
        out.append(0.7 * kick + bass + hat)
    peak = max(abs(s) for s in out)
    return [s / peak * 0.8 for s in out]


def main():
    path = sys.argv[1]
    seconds = float(sys.argv[2]) if len(sys.argv) > 2 else 20
    samples = render(seconds)
    with wave.open(path, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(b''.join(struct.pack('<hh', int(s * 32767), int(s * 32767))
                               for s in samples))


if __name__ == '__main__':
    main()
