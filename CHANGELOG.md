# Changelog

## 1.0.0 - 2026-05-17

Initial public release of tram8+.

### Firmware

- Adds custom firmware for the TRAM8 MIDI-to-CV module.
- Supports 8 gate outputs with independent MIDI note mappings.
- Provides velocity mode, where each DAC follows the velocity of the active note for its gate.
- Provides CC mode, where gates follow mapped notes while DACs track a fixed MIDI CC bank: CC 69-76 map to DAC 0-7.
- Provides SysEx mode for direct DAW control of all gates and DAC outputs.
- Supports MIDI learn for assigning notes to gates from the hardware.
- Tracks held notes per gate so overlapping mapped notes release back to the previous held note.

### VST3 Bridge

- Adds a macOS VST3 plugin for sending MIDI-driven control data from a DAW to tram8+ over CoreMIDI.
- Provides per-gate channel and note filtering.
- Provides per-gate DAC modes: velocity, pitch, CC, and off.
- Supports DAC channel filtering for pitch mode and configurable CC numbers for CC mode.
- Preserves plugin state for gate mappings, DAC modes, DAC channels, and CC assignments.
- Shows MIDI input and output activity in the plugin UI.

### SysEx Protocol

- Adds a shared packed SysEx protocol for firmware and VST communication.
- Supports gate-only, coarse DAC, and full 12-bit DAC state messages.
- Supports targeted single-gate and single-DAC messages for lower-latency updates.
- Batches DAC and gate writes in firmware for faster application of full state updates.

### Releases

- Publishes firmware as `tram8-firmware.syx`.
- Publishes the macOS VST3 plugin as `tram8-bridge-macos.zip`.
- Uses a repo-level `VERSION` file as the release version source of truth.
- Validates release tags against `VERSION` before building release artifacts.
