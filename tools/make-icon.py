#!/usr/bin/env python3
"""Generate Snap's vector icon (HVIF) straight from code.

Usage: make-icon.py output.hvif [preview.png]

A graphite camera whose big lens glows with the colours of the effects (a
radial yellow-orange-magenta-violet burst), a chrome lens ring, and the white
four-pointed spark of the flash at the upper right. Light comes from the upper
left, as in Haiku's icon style; detail shapes are left out at small sizes.
"""
from pathlib import Path
import sys

import hvif

C = hvif.hex_color
styles, paths, shapes = [], [], []
K = 0.55228475  # cubic Bézier circle constant


def solid(color, alpha=255):
    styles.append({'color': C(color, alpha)})
    return len(styles) - 1


def linear(start, end, colors):
    styles.append(hvif.linear_gradient(start, end,
        [(i / (len(colors) - 1), C(color)) for i, color in enumerate(colors)]))
    return len(styles) - 1


def radial(center, radius, stops):
    styles.append(hvif.radial_gradient(center, radius,
        [(offset, C(color)) for offset, color in stops]))
    return len(styles) - 1


def shape(style, points, detail=False):
    paths.append({'closed': True, 'points': points})
    item = {'style': style, 'paths': [len(paths) - 1]}
    if detail:
        item['lod'] = (0.5, 4.0)
    shapes.append(item)


def ellipse(style, cx, cy, rx, ry, detail=False):
    shape(style, [
        ((cx + rx, cy), (cx + rx, cy - K * ry), (cx + rx, cy + K * ry)),
        ((cx, cy + ry), (cx + K * rx, cy + ry), (cx - K * rx, cy + ry)),
        ((cx - rx, cy), (cx - rx, cy + K * ry), (cx - rx, cy - K * ry)),
        ((cx, cy - ry), (cx - K * rx, cy - ry), (cx + K * rx, cy - ry)),
    ], detail)


def rounded(style, left, top, right, bottom, r, detail=False):
    k = r * (1 - K)
    shape(style, [
        ((left + r, top), (left + k, top), (left + r, top)),
        ((right - r, top), (right - r, top), (right - k, top)),
        ((right, top + r), (right, top + k), (right, top + r)),
        ((right, bottom - r), (right, bottom - r), (right, bottom - k)),
        ((right - r, bottom), (right - k, bottom), (right - r, bottom)),
        ((left + r, bottom), (left + r, bottom), (left + k, bottom)),
        ((left, bottom - r), (left, bottom - k), (left, bottom - r)),
        ((left, top + r), (left, top + r), (left, top + k)),
    ], detail)


def spark(style, cx, cy, size, waist, detail=False):
    """A four-pointed star with curved sides."""
    s, w = size, waist
    shape(style, [
        ((cx, cy - s), (cx, cy - s), (cx + w * 0.3, cy - w)),
        ((cx + s, cy), (cx + w, cy - w * 0.3), (cx + w, cy + w * 0.3)),
        ((cx, cy + s), (cx + w * 0.3, cy + w), (cx - w * 0.3, cy + w)),
        ((cx - s, cy), (cx - w, cy + w * 0.3), (cx - w, cy - w * 0.3)),
    ], detail)


outline = solid('1c2129')
body = linear((10, 16), (44, 58), ['5c6676', '3a414d', '262b33'])
body_top = linear((10, 18), (10, 30), ['7d8798', '4b5361'])
hump = linear((18, 8), (26, 22), ['6b7585', '3c434f'])
grip = linear((6, 28), (14, 50), ['30363f', '1f242b'])
ring = linear((14, 18), (46, 52), ['f4f7fb', '9aa4b2', '59626f', 'c9d0da'])
ring_inner = solid('171a20')
lens = radial((30, 34), 15, [(0.0, 'fff7b0'), (0.28, 'ffc24a'), (0.5, 'ff6a3d'),
                             (0.72, 'e0307f'), (1.0, '5b2fc4')])
gleam = solid('ffffff', 200)
viewfinder = linear((40, 21), (50, 26), ['c8f2ff', '5fb4d9'])
red_dot = linear((12, 21), (16, 25), ['ff8a7a', 'd8283a'])
flash_glow = radial((51, 12), 11, [(0.0, 'fffbe0'), (0.55, 'ffe78a'), (1.0, 'ffd04a')])
white = solid('ffffff')

# The camera body: a rounded block with a viewfinder hump on top.
shape(outline, [(15, 14), (18, 7), (31, 7), (34, 14)])
rounded(outline, 3, 13, 61, 57, 8)
shape(hump, [(17, 15), (19.5, 9), (29.5, 9), (32, 15)])
rounded(body, 5, 15, 59, 55, 6.5)
rounded(body_top, 5, 15, 59, 27, 6.5, detail=True)
shape(grip, [(5, 27), (12, 27), (12, 52), (8, 55), (5, 50)], detail=True)
rounded(outline, 40, 19, 51, 27, 2)
rounded(viewfinder, 41, 20, 50, 26, 1.5)
ellipse(red_dot, 14, 22.5, 2.2, 2.2, detail=True)

# The lens: chrome ring, dark gap, then the glowing effect glass.
ellipse(outline, 31, 37, 16.5, 16.5)
ellipse(ring, 31, 37, 15, 15)
ellipse(ring_inner, 31, 37, 11.5, 11.5)
ellipse(lens, 31, 37, 10.2, 10.2)
shape(gleam, [
    ((24, 33), (24, 33), (25, 30)),
    ((29, 29), (27, 29.2), (29, 29)),
    ((27.5, 31), (27.5, 31), (25.5, 32)),
], detail=True)
ellipse(gleam, 36, 42, 1.4, 1.4, detail=True)

# The flash: a soft glow and a crisp white spark breaking out of the corner.
spark(outline, 52, 11.5, 11.5, 3.2)
spark(flash_glow, 52, 11.5, 10, 2.6)
spark(white, 52, 11.5, 5.5, 1.4, detail=True)

icon = {'styles': styles, 'paths': paths, 'shapes': shapes}

if __name__ == '__main__':
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path('snap-icon.hvif')
    data = hvif.encode(icon)
    hvif.decode(data)
    out.write_bytes(data)
    if len(sys.argv) > 2:
        hvif.preview(icon, 256).save(sys.argv[2])
    print(f'{out}: {len(data)} bytes')
