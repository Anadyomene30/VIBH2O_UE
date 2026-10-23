"""Draws the iPad remote's home-screen icons.

    python Tools/make_remote_icons.py

Writes icon-180.png (the size iOS asks for) and icon-512.png into
Plugins/VibH2O/Resources/Remote/. Standard library only: a bubble of
concentric rings on black, the motif of the piece, legible as a small icon in
a dark room.
"""

import math
import struct
import zlib
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / 'Plugins' / 'VibH2O' / 'Resources' / 'Remote'


def smooth(edge0, edge1, x):
    t = min(max((x - edge0) / (edge1 - edge0), 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


def pixel(x, y, size):
    """Colour of one pixel, from its distance to the centre."""
    c = (size - 1) / 2.0
    r = math.hypot(x - c, y - c) / (size * 0.5)  # 0 at the centre, 1 at the edge
    aa = 1.5 / size

    # The bubble: a soft disc, brighter toward its rim like a lit membrane.
    disc = 1.0 - smooth(0.78 - aa, 0.78 + aa, r)
    rim = math.exp(-((r - 0.74) / 0.05) ** 2)

    # Three rings spreading out from the heart - the beat.
    rings = 0.0
    for k, radius in enumerate((0.22, 0.40, 0.58)):
        rings += (0.9 - 0.2 * k) * math.exp(-((r - radius) / 0.022) ** 2)

    body = disc * (0.18 + 0.35 * (1.0 - r))
    intensity = body + 0.85 * rim * disc + 0.8 * rings * disc
    red = 0.10 * intensity
    green = 0.55 * intensity
    blue = 1.00 * intensity
    return tuple(int(255 * min(max(v, 0.0), 1.0)) for v in (red, green, blue))


def write_png(path, size):
    rows = bytearray()
    for y in range(size):
        rows.append(0)  # filter: none
        for x in range(size):
            rows.extend(pixel(x, y, size))

    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xFFFFFFFF)

    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', size, size, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(bytes(rows), 9))
    png += chunk(b'IEND', b'')
    path.write_bytes(png)
    print(f'{path.name}: {size} x {size}, {len(png)} octets')


if __name__ == '__main__':
    OUT.mkdir(parents=True, exist_ok=True)
    write_png(OUT / 'icon-180.png', 180)
    write_png(OUT / 'icon-512.png', 512)
