#!/usr/bin/env python3
"""Convert an RGBA PNG into a compact C alpha-mask asset."""
import struct
import sys
import zlib


def paeth(a, b, c):
    estimate = a + b - c
    pa = abs(estimate - a)
    pb = abs(estimate - b)
    pc = abs(estimate - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def decode(path):
    data = open(path, "rb").read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG")
    pos = 8
    idat = bytearray()
    width = height = depth = color_type = None
    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, color_type, compression, filtering, interlace = struct.unpack(">IIBBBBB", chunk)
            if depth != 8 or color_type != 6 or interlace != 0:
                raise ValueError("expected non-interlaced 8-bit RGBA PNG")
        elif kind == b"IDAT":
            idat.extend(chunk)
        elif kind == b"IEND":
            break
    if width is None:
        raise ValueError("missing PNG header")

    raw = zlib.decompress(idat)
    stride = width * 4
    rows = []
    offset = 0
    previous = bytearray(stride)
    for _ in range(height):
        filter_type = raw[offset]
        offset += 1
        encoded = raw[offset:offset + stride]
        offset += stride
        row = bytearray(stride)
        for index, value in enumerate(encoded):
            left = row[index - 4] if index >= 4 else 0
            above = previous[index]
            upper_left = previous[index - 4] if index >= 4 else 0
            if filter_type == 0:
                result = value
            elif filter_type == 1:
                result = value + left
            elif filter_type == 2:
                result = value + above
            elif filter_type == 3:
                result = value + ((left + above) // 2)
            elif filter_type == 4:
                result = value + paeth(left, above, upper_left)
            else:
                raise ValueError("unsupported PNG filter")
            row[index] = result & 0xFF
        rows.append(row)
        previous = row
    alpha = bytes(row[index] for row in rows for index in range(3, stride, 4))
    return width, height, alpha


def main():
    if len(sys.argv) != 3:
        raise SystemExit("usage: png_to_alpha_c.py input.png output.c")
    width, height, alpha = decode(sys.argv[1])
    with open(sys.argv[2], "w", newline="\n") as output:
        output.write("#include <stdint.h>\n\n")
        output.write(f"const uint32_t uos_logo_width = {width};\n")
        output.write(f"const uint32_t uos_logo_height = {height};\n")
        output.write("const uint8_t uos_logo_alpha[] = {\n")
        for start in range(0, len(alpha), 16):
            output.write("    " + ", ".join(f"0x{value:02X}" for value in alpha[start:start + 16]) + ",\n")
        output.write("};\n")


if __name__ == "__main__":
    main()
