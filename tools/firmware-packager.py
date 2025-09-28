#!/usr/bin/env python3
"""Firmware packager for TRAM8+ hardware."""

import argparse
from pathlib import Path
from intelhex import IntelHex


def package_firmware(hex_path: Path, output_dir: Path = None) -> Path:
    """Package Intel HEX firmware into TRAM8 SysEx format for MIDI updates."""

    if output_dir is None:
        output_dir = hex_path.parent

    ih = IntelHex()
    ih.loadhex(str(hex_path))

    bootloader_start = 0xE000
    lines_per_page = 4

    last_addr = max(i for i in range(bootloader_start) if ih[i] != 255)
    num_pages = int(round(last_addr / (16 * lines_per_page) + 0.5))

    # Validate firmware doesn't exceed bootloader boundary
    max_pages = bootloader_start // (16 * lines_per_page)
    if num_pages > max_pages:
        raise ValueError(
            f"Firmware too large: {num_pages} pages exceeds maximum {max_pages} pages "
            f"(bootloader starts at 0x{bootloader_start:X})"
        )

    # Add free page if space available
    if last_addr < (bootloader_start - 1):
        num_pages += 1
        for i in range(16 * lines_per_page):
            last_addr += 1
            ih[last_addr] = 0

    total_length = num_pages * 16 * lines_per_page

    linear_path = output_dir / f"{hex_path.stem}_linear.hex"
    with open(linear_path, "w") as f:
        ih[0:total_length].write_hex_file(f)

    sysex_path = output_dir / f"{hex_path.stem}_firmware.syx"

    sysex_start = bytes.fromhex("F000297F")  # SysEx start + LPZW manufacturer ID
    firmware_id = b"T8FW"
    version_byte = b"\x00"
    sysex_end = bytes.fromhex("3A303030303030303146460D0AF7")
    timing_padding = b"\x00" * 451  # Inter-page delay for flash timing

    with open(linear_path) as hex_file, open(sysex_path, "wb") as sysex_file:
        sysex_file.write(sysex_start + firmware_id + version_byte)

        for page in range(num_pages):
            sysex_file.write(timing_padding)
            for _ in range(lines_per_page):
                hex_line = hex_file.readline()
                sysex_file.write(hex_line.encode("utf-8"))

        sysex_file.write(timing_padding + sysex_end)

    return sysex_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("hex_file", type=Path, help="Input Intel HEX firmware file")
    parser.add_argument("-o", "--output-dir", type=Path, help="Output directory (default: same as input)")
    parser.add_argument("-v", "--verbose", action="store_true", help="Enable verbose output")

    args = parser.parse_args()

    if not args.hex_file.exists():
        parser.error(f"Firmware file not found: {args.hex_file}")

    if not args.hex_file.suffix.lower() == ".hex":
        parser.error(f"Expected .hex file, got: {args.hex_file.suffix}")

    try:
        sysex_path = package_firmware(args.hex_file, args.output_dir)

        if args.verbose:
            print(f"Packaged firmware: {args.hex_file}")
            print(f"Generated SysEx: {sysex_path}")
        else:
            print(f"Created: {sysex_path.name}")

    except Exception as e:
        parser.error(f"Packaging failed: {e}")


if __name__ == "__main__":
    main()
