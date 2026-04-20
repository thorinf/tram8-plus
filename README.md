# tram8+

Custom firmware for the [TRAM8](https://github.com/kay-lpzw/TRAM8) MIDI-to-CV module. Adds multiple operating modes, MIDI learn, and a packed SysEx protocol for direct DAW control. Also includes an optional VST3 plugin for bridging a DAW to the hardware.

## Firmware Modes

| Mode | Description |
|------|-------------|
| Velocity | Gate on/off from note events, DAC outputs note velocity as 0–5V |
| CC | Gate on/off from note events, DAC tracks a configurable MIDI CC as 0–5V |
| SysEx | Direct control of all 8 gates and 12-bit DAC values via packed SysEx messages |

Each gate can be independently configured with a MIDI channel and note filter.

## Building

### Firmware

Requires `avr-gcc` toolchain. See `firmware/Makefile`.

### VST3 Plugin (macOS)

```sh
cd vst/build
XCODE_VERSION=15.0 cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release --target tram8-bridge
```

## VST3 Plugin

Optional companion plugin that receives MIDI in the DAW and sends packed SysEx to the hardware via CoreMIDI. Per-gate configuration of channel, note, and DAC mode (velocity, pitch, CC, off).

<p align="center">
  <img src="assets/vst-ui.png" alt="tram8+ VST UI" width="560">
</p>

## Project Structure

```
firmware/       AVR firmware (C)
protocol/       Shared SysEx message definitions
vst/
  source/       VST3 plugin (C++/ObjC++)
  resource/     WebView UI (HTML/CSS/JS)
  external/     VST3 SDK (submodule)
```
