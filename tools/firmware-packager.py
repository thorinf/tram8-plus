#!/usr/bin/env python3
"""Firmware packager for TRAM8+ hardware."""

import argparse
from pathlib import Path
from intelhex import IntelHex


def package_firmware(hex_path: Path, output_dir: Path = None, verbose: bool = False) -> Path:
    """Package Intel HEX firmware into TRAM8 SysEx format for MIDI updates.

    Args:
        hex_path: Path to input Intel HEX file
        output_dir: Output directory (defaults to same as input)
        verbose: Enable verbose output

    Returns:
        Path to generated SysEx file
    """

    if output_dir is None:
        output_dir = hex_path.parent

    output_dir.mkdir(parents=True, exist_ok=True)

    BOOTLOADER_START = 0x1C00  # ATmega8A boot section start (1 KB bootloader)
    LINES_PER_PAGE = 4
    PAGE_SIZE = 16 * LINES_PER_PAGE

    ih = IntelHex()
    ih.padding = 0xFF
    ih.loadhex(str(hex_path))

    defined_addresses = ih.addresses()
    if not defined_addresses:
        raise ValueError("No firmware data found in HEX file")

    first_overrun = next((addr for addr in defined_addresses if addr >= BOOTLOADER_START), None)
    if first_overrun is not None:
        raise ValueError(f"Firmware exceeds bootloader boundary: 0x{first_overrun:X} >= 0x{BOOTLOADER_START:X}")

    last_addr = max(defined_addresses)

    num_pages = (last_addr + 1 + PAGE_SIZE - 1) // PAGE_SIZE
    total_length = num_pages * PAGE_SIZE

    if verbose:
        print(f"Bootloader start: 0x{BOOTLOADER_START:X}")
        print(f"Last used address: 0x{last_addr:X}")
        print(f"Firmware size: {last_addr + 1} bytes")
        print(f"Pages required: {num_pages}")
        print(f"Total length: {total_length} bytes")

    linear_ih = IntelHex()
    linear_ih.padding = 0xFF
    for addr in range(total_length):
        linear_ih[addr] = ih[addr]

    linear_path = output_dir / f"{hex_path.stem}_linear.hex"
    with open(linear_path, "w", newline="\r\n", encoding="ascii") as f:
        linear_ih.write_hex_file(f, byte_count=16)

    sysex_path = output_dir / f"{hex_path.stem}_firmware.syx"

    sysex_start = bytes.fromhex("F000297F")
    firmware_id = b"T8FW"
    version_byte = b"\x00"
    sysex_end = bytes.fromhex("3A303030303030303146460D0AF7")
    timing_padding = b"\x00" * 451

    # newline="" keeps CRLF pairs intact when feeding the SysEx payload
    with open(linear_path, encoding="ascii", newline="") as hex_file, open(sysex_path, "wb") as sysex_file:
        sysex_file.write(sysex_start + firmware_id + version_byte)

        for page in range(num_pages):
            sysex_file.write(timing_padding)
            for _ in range(LINES_PER_PAGE):
                hex_line = hex_file.readline()
                if not hex_line:
                    raise ValueError(f"Linear HEX file ended unexpectedly at page {page}")
                # Intel HEX is ASCII, so UTF-8 encoding is safe for the SysEx stream
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
        sysex_path = package_firmware(args.hex_file, args.output_dir, args.verbose)

        if args.verbose:
            print(f"Generated SysEx: {sysex_path}")
        else:
            print(f"Created: {sysex_path.name}")

    except Exception as e:
        parser.error(f"Packaging failed: {e}")


if __name__ == "__main__":
    main()
