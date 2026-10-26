#!/usr/bin/env python3
"""Print the offset and size of the ota_1 app partition in an ESP32 partitions.bin."""
import struct
import sys

data = open(sys.argv[1], "rb").read()
for i in range(0, len(data), 32):
    entry = data[i:i + 32]
    if entry[:2] != b"\xaa\x50":
        break
    ptype, subtype, offset, size = struct.unpack_from("<BBII", entry, 2)
    if ptype == 0x00 and subtype == 0x11:  # app, ota_1
        print(hex(offset), hex(size))
        sys.exit(0)
sys.exit("ota_1 not found")
