# EQ Pro (JUCE) - 3.2 beta

Work-in-progress JUCE multi-channel EQ plugin.

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

## Processing Modes (v3.2 beta)
- Real-time: minimum-phase IIR (0 latency).
- Natural: short linear-phase FIR with adaptive tap length and mixed-phase blend.
- Linear: long linear-phase FIR with adaptive taps and mixed-phase blend.


