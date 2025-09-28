# TRAM8+

Enhanced firmware for the TRAM8 eurorack synthesizer module with improved functionality.

## Overview

TRAM8+ is an enhanced version of the original [TRAM8](https://github.com/kay-lpzw/TRAM8) firmware.

## Quick Start

### Prerequisites

- AVR toolchain (`avr-gcc`, `avr-objcopy`, `avr-size`)
- Python 3.10+ with `intelhex` package

### Building Firmware

```bash
# Build firmware
cd firmware
make

# Package for MIDI firmware update
cd ..
python tools/firmware-packager.py firmware/build/main.hex -v
```

This generates:
- `main.hex` - Intel HEX firmware file
- `main_firmware.syx` - MIDI SysEx file for hardware updates

### Development Setup

```bash
# Install Python dependencies
pip install intelhex

# Install pre-commit hooks (optional)
pip install pre-commit
pre-commit install
```
