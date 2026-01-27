# EQ Pro (JUCE) - 4.5 beta

Work-in-progress JUCE multi-channel EQ plugin.

## Product Description
EQ Pro is a precision multi-channel EQ designed for modern mixing, mastering, and immersive
formats. It combines detailed visual feedback with fast, musical controls and phase-accurate
processing modes for transparent or surgical work across stereo, surround, and Atmos layouts.

**v4.5 beta** introduces a new Harmonic Processing Layer (Layer 2) with independent odd and even harmonic generation per band, 
global harmonic layer oversampling controls, and enhanced UI organization with layer-based navigation.

## Key Features
- 12 fully parametric bands per channel with per-band mix and routing.
- **Harmonic Processing Layer (v4.5 beta)**: Independent odd and even harmonic generation per band with global oversampling controls.
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

## Processing Modes
- Real-time: minimum-phase IIR (0 latency).
- Natural: short linear-phase FIR with adaptive tap length and mixed-phase blend.
- Linear: long linear-phase FIR with adaptive taps and mixed-phase blend.

## Recent Updates (v4.5 beta)
- **Harmonic Processing Layer**: New Layer 2 with independent odd and even harmonic generation
  - Per-band odd harmonic control (-24 to +24 dB) with mix (0-100%)
  - Per-band even harmonic control (-24 to +24 dB) with mix (0-100%)
  - Per-band harmonic bypass (independent for each of 12 bands)
  - Global harmonic layer oversampling (NONE, 2X, 4X, 8X, 16X) - applies uniformly to all bands
  - Oversampling only available in Natural Phase and Linear Phase modes (disabled in Real-time)
  - Layer-based UI navigation with EQ (Layer 1) and Harmonic (Layer 2) toggles
  - Sample-accurate dry/wet mixing with latency compensation for oversampled processing
- **UI Organization**: Improved header layout with layer toggles on left, band number on right
- **Performance Optimizations** (v4.4 beta): Critical fixes for instant GUI loading
  - Removed expensive 3D gradients, replaced with fast flat colors for instant rendering
  - Deferred timer initialization until components are properly laid out
  - Added buffered rendering (`setBufferedToImage`) for reduced repaint overhead
  - All controls now appear immediately on plugin load
- **FFT Analyzer Optimization** (v4.4 beta): Compact, plain text labels (no background boxes) for maximum display space
- **UI Harmonization** (v4.4 beta): Flat-color styling across all buttons, toggles, and knobs for consistent visual language


