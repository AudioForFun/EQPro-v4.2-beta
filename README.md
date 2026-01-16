# EQ Pro (JUCE) - 2.2 beta

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

