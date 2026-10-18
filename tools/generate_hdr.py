import math
import struct
from pathlib import Path


WIDTH = 512
HEIGHT = 256
OUTPUT = Path("assets/studio_test.hdr")


def make_pixel(x, y):
    # Equirectangular environment coordinates.
    u = x / WIDTH
    v = y / HEIGHT

    # Dim neutral studio environment.
    base = [0.22, 0.24, 0.28]

    # Warm floor / lower environment.
    if v > 0.68:
        base = [0.32, 0.23, 0.16]

    # Cool ceiling.
    if v < 0.28:
        base = [0.18, 0.23, 0.32]

    # Large bright rectangular studio window.
    if 0.12 < u < 0.34 and 0.20 < v < 0.62:
        base = [5.0, 5.8, 6.5]

    # Smaller warm light panel.
    if 0.62 < u < 0.74 and 0.30 < v < 0.43:
        base = [10.0, 5.0, 1.6]

    # A soft bright strip to create reflected highlights.
    if 0.40 < u < 0.48 and 0.12 < v < 0.16:
        base = [3.0, 3.2, 3.5]

    # Subtle horizontal illumination gradient.
    gradient = 0.8 + 0.35 * math.sin(u * 2.0 * math.pi)
    base = [c * gradient for c in base]

    return base


def to_rgbe(rgb):
    maximum = max(rgb)

    if maximum < 1e-32:
        return bytes((0, 0, 0, 0))

    mantissa, exponent = math.frexp(maximum)
    scale = mantissa * 256.0 / maximum

    r = max(0, min(255, int(rgb[0] * scale)))
    g = max(0, min(255, int(rgb[1] * scale)))
    b = max(0, min(255, int(rgb[2] * scale)))
    e = exponent + 128

    if e <= 0 or e > 255:
        return bytes((0, 0, 0, 0))

    return bytes((r, g, b, e))


def encode_scanline(pixels):
    # Use the old flat RGBE scanline encoding.
    # The C++ loader supports this for any scanline width.
    return b"".join(to_rgbe(pixel) for pixel in pixels)


def main():
    OUTPUT.parent.mkdir(parents=True, exist_ok=True)

    with OUTPUT.open("wb") as f:
        f.write(b"#?RADIANCE\n")
        f.write(b"FORMAT=32-bit_rle_rgbe\n")
        f.write(b"\n")
        f.write(f"-Y {HEIGHT} +X {WIDTH}\n".encode("ascii"))

        for y in range(HEIGHT):
            row = [make_pixel(x, y) for x in range(WIDTH)]
            f.write(encode_scanline(row))

    print(f"Generated: {OUTPUT.resolve()}")
    print(f"Resolution: {WIDTH} x {HEIGHT}")


if __name__ == "__main__":
    main()
