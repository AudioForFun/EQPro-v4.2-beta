# EQ Pro (JUCE) - 4.4 beta

Work-in-progress JUCE multi-channel EQ plugin.

## Product Description
EQ Pro is a precision multi-channel EQ designed for modern mixing, mastering, and immersive
formats. It combines detailed visual feedback with fast, musical controls and phase-accurate
processing modes for transparent or surgical work across stereo, surround, and Atmos layouts.

**v4.4 beta** introduces harmonized 3D beveled styling across all UI elements (knobs, toggles, and buttons),
optimized FFT analyzer labels for maximum display space, and a consistent modern visual language throughout the interface.

## Key Features
- 12 fully parametric bands per channel with per-band mix and routing.
- Real-time analyzer with pre/post curves, band overlays, and hover readouts.
- Natural and Linear phase modes with adaptive FIR lengths.
- Goniometer + correlation scope with auto-gain stability.
- Format-aware channel routing with per-pair Mid/Side targets.
- Band-colored EQ frame and updated LED rings per band.
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

## Processing Modes (v4.4 beta)
- Real-time: minimum-phase IIR (0 latency).
- Natural: short linear-phase FIR with adaptive tap length and mixed-phase blend.
- Linear: long linear-phase FIR with adaptive taps and mixed-phase blend.

## Recent Updates (v4.4 beta)
- **UI Harmonization**: All text buttons now feature modern 3D beveled styling matching knobs and toggles
- **FFT Analyzer Optimization**: Compact, plain text labels (no background boxes) for maximum display space
- **Visual Consistency**: Unified 3D gradient style across all interactive elements


