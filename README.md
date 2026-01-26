# EQ Pro (JUCE) - 3.8 beta

Work-in-progress JUCE multi-channel EQ plugin.

## Product Description
EQ Pro is a precision multi-channel EQ designed for modern mixing and mastering.
It combines detailed visual feedback with fast, musical controls and phase-accurate
processing modes for transparent or surgical work.

## Key Features
- 12 fully parametric bands per channel with per-band mix and routing.
- Real-time analyzer with pre/post curves, band overlays, and hover readouts.
- Natural and Linear phase modes with adaptive FIR lengths.
- Goniometer + correlation scope with auto-gain stability.
- RMS-based auto gain for consistent level matching.
- Polished workflow: Undo/Redo, Save/Load, preset browser with prev/next.
- Professional UI polish: value pills, focus rings, modern meters.

## Requirements
- JUCE (download from https://juce.com)
- CMake 3.15+
- A C++17 compiler

## Build (Windows example)
```
cmake -S . -B build -DJUCE_DIR="C:\path\to\JUCE"
cmake --build build --config Release
```

## Notes
- VST3/AU/Standalone targets are enabled in `CMakeLists.txt`.
- Change `JUCE_DIR` in `CMakeLists.txt` or pass `-DJUCE_DIR=...` when configuring.
- Standalone startup log: `%TEMP%\\EQPro_startup_*.log`.
- Standalone state restore is disabled by default; set `EQPRO_LOAD_STATE=1` to enable.
- Standalone audio device restore is disabled by default; set `EQPRO_LOAD_AUDIO_STATE=1` to enable.
- Standalone window position restore is disabled by default; set `EQPRO_LOAD_WINDOW_POS=1` to enable.

## Processing Modes (v3.8 beta)
- Real-time: minimum-phase IIR (0 latency).
- Natural: short linear-phase FIR with adaptive tap length and mixed-phase blend.
- Linear: long linear-phase FIR with adaptive taps and mixed-phase blend.


