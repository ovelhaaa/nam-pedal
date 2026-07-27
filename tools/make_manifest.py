#!/usr/bin/env python3
"""Build the fixed NAM pedal QSPI manifest and combined programming image."""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path

MAGIC = 0x504D414E
MANIFEST_VERSION = 1
MANIFEST_SIZE = 64
VALID_FLAG = 1
QSPI_APP_ADDRESS = 0x90010000
QSPI_APP_OFFSET = 0x10000
QSPI_CAPACITY = 8 * 1024 * 1024


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True, type=Path)
    parser.add_argument("--manifest", required=True, type=Path)
    parser.add_argument("--combined", required=True, type=Path)
    parser.add_argument("--firmware-version", required=True, type=int)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    image = args.image.read_bytes()
    if len(image) < 8:
        raise SystemExit("XIP image is too small to contain a vector table")
    if len(image) > QSPI_CAPACITY - QSPI_APP_OFFSET:
        raise SystemExit("XIP image exceeds the 0x90010000..0x907fffff region")

    image_crc = zlib.crc32(image) & 0xFFFFFFFF
    header_without_crc = struct.pack(
        "<IHHIIIIII7I",
        MAGIC,
        MANIFEST_VERSION,
        MANIFEST_SIZE,
        QSPI_APP_ADDRESS,
        len(image),
        image_crc,
        args.firmware_version,
        VALID_FLAG,
        QSPI_APP_ADDRESS,
        *([0] * 7),
    )
    assert len(header_without_crc) == MANIFEST_SIZE - 4
    header_crc = zlib.crc32(header_without_crc) & 0xFFFFFFFF
    manifest = header_without_crc + struct.pack("<I", header_crc)

    args.manifest.write_bytes(manifest)
    args.combined.write_bytes(
        manifest + bytes([0xFF]) * (QSPI_APP_OFFSET - len(manifest)) + image
    )
    print(
        f"manifest: image={len(image)} bytes "
        f"image_crc=0x{image_crc:08x} header_crc=0x{header_crc:08x}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
