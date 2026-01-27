# EQ Pro (JUCE) - 4.5 beta

Professional multi-channel EQ plugin with harmonic processing and advanced visual feedback.

## Product Description

**EQ Pro** is a precision multi-channel equalizer designed for modern mixing, mastering, and immersive audio production. Combining surgical precision with musical workflow, EQ Pro delivers transparent or character-rich processing across stereo, surround, and Dolby Atmos formats.

Built on JUCE with zero-compromise audio quality, EQ Pro features phase-accurate processing modes, real-time spectral analysis, and an innovative **Harmonic Processing Layer** that adds warmth, character, and musicality to your mixes.

---

## Key Features

### 🎛️ **Multi-Channel EQ Engine**
- **12 fully parametric bands per channel** with independent frequency, gain, Q, and filter type control
- **Per-band mix control** (0-100%) for parallel processing workflows
- **Per-band routing** with channel masks and Mid/Side targeting
- **Format-aware processing** supporting stereo, surround (5.1, 7.1), and Dolby Atmos layouts
- **Band-colored visual feedback** with dynamic LED rings and EQ frame highlighting

### 🎨 **Harmonic Processing Layer (v4.5 beta)**
- **Independent odd and even harmonic generation** per band (-24 to +24 dB)
- **Per-band harmonic mix controls** (0-100%) for precise blend of harmonic content
- **Per-band harmonic bypass** (independent for each of 12 bands)
- **Global harmonic layer oversampling** (NONE, 2X, 4X, 8X, 16X) - applies uniformly to all bands
- **Oversampling available in Natural Phase and Linear Phase modes** (disabled in Real-time for zero latency)
- **Sample-accurate dry/wet mixing** with automatic latency compensation
- **Non-linear waveshaping**: Cubic distortion for odd harmonics, quadratic for even harmonics
- **Layer-based UI navigation** with dedicated EQ (Layer 1) and Harmonic (Layer 2) views

### 📊 **Real-Time Spectral Analyzer**
- **Three analyzer curves** for comprehensive visual feedback:
  - **Pre-EQ curve** (light grey): Input signal before processing
  - **Post-EQ curve** (darker grey): Output signal after EQ processing
  - **Harmonic curve** (red, v4.5 beta): Program signal + harmonics - only visible when harmonics are active
- **Interactive band overlays** with color-coded frequency responses
- **Hover readouts** showing frequency, gain, Q, and filter type
- **Adjustable analyzer settings**: Range, speed, freeze, and view modes
- **External analyzer input** for sidechain visualization
- **Logarithmic frequency axis** with smooth, beautiful curve rendering

### ⚡ **Phase Processing Modes**
- **Real-time Mode**: Minimum-phase IIR filters with **zero latency** - perfect for live performance and tracking
- **Natural Phase Mode**: Short linear-phase FIR with adaptive tap length and mixed-phase blend - ideal for mixing
- **Linear Phase Mode**: Long linear-phase FIR with adaptive taps and mixed-phase blend - perfect for mastering
- **Adaptive quality settings** that automatically adjust FIR length based on frequency content
- **Mixed-phase blending** for transient preservation in linear modes

### 🎚️ **Dynamic Processing**
- **Per-band dynamic EQ** with threshold, ratio, attack, and release controls
- **External sidechain detection** per band for frequency-dependent ducking and enhancement
- **Auto-gain compensation** for consistent level matching across processing modes
- **Smart solo mode** for intelligent band isolation

### 📈 **Advanced Metering**
- **Goniometer** with stereo width visualization
- **Correlation meter** for phase relationship monitoring
- **Output level meters** with peak and RMS detection
- **Auto-gain stability** ensuring consistent metering across all processing modes
- **Professional color scheme**: Green (safe), yellow (moderate), red (loud)

### 🎯 **Workflow Features**
- **Undo/Redo** with full state management
- **Preset browser** with favorites, search, and prev/next navigation
- **A/B/C/D snapshot comparison** for instant recall
- **Copy/Paste** for quick parameter transfer
- **Value pills** with precise numeric readouts
- **Focus rings** and hover indicators for enhanced usability
- **Modern, polished UI** with flat-color styling and smooth animations

### 🔧 **Technical Excellence**
- **Sample-accurate processing** with automatic latency compensation
- **Lock-free audio thread** with zero allocations during playback
- **Optimized performance** with deferred initialization and buffered rendering
- **Professional DSP architecture** with clean separation between audio and UI threads
- **Comprehensive parameter automation** support for all controls

---

## Processing Modes

### Real-time Mode
- **Zero latency** minimum-phase IIR processing
- Perfect for live performance, tracking, and real-time monitoring
- Harmonic processing available with optional oversampling (adds latency)

### Natural Phase Mode
- Short linear-phase FIR with adaptive tap length
- Mixed-phase blend for transient preservation
- Harmonic oversampling available (2X, 4X, 8X, 16X)

### Linear Phase Mode
- Long linear-phase FIR with adaptive taps
- Maximum phase accuracy for mastering applications
- Harmonic oversampling available (2X, 4X, 8X, 16X)

---

## Recent Updates (v4.5 beta)

### Harmonic Processing Layer
- **New Layer 2** with independent odd and even harmonic generation per band
- Per-band controls: Odd harmonic amount (-24 to +24 dB), Mix Odd (0-100%), Even harmonic amount (-24 to +24 dB), Mix Even (0-100%)
- Per-band harmonic bypass (independent for each of 12 bands)
- Global harmonic layer oversampling (NONE, 2X, 4X, 8X, 16X) - applies uniformly to all bands
- Oversampling only available in Natural Phase and Linear Phase modes (disabled in Real-time)
- Layer-based UI navigation with EQ (Layer 1) and Harmonic (Layer 2) toggles
- Sample-accurate dry/wet mixing with latency compensation for oversampled processing
- Non-linear waveshaping: Cubic distortion for odd harmonics, quadratic for even harmonics

### Red Analyzer Curve
- **Third analyzer curve** displaying program signal + harmonics in bright red
- Only visible when harmonics are active on at least one band
- Respects all analyzer settings: range, speed, freeze
- Provides real-time visual feedback for harmonic processing impact

### UI Organization
- Improved header layout with layer toggles on left, band number on right
- Optimized toggle sizes based on text length
- Simplified band number display (e.g., "1" instead of "1 / 12")

### Performance Optimizations (v4.4 beta)
- Critical fixes for instant GUI loading
- Removed expensive 3D gradients, replaced with fast flat colors
- Deferred timer initialization until components are properly laid out
- Added buffered rendering for reduced repaint overhead
- All controls now appear immediately on plugin load

### UI Harmonization (v4.4 beta)
- Flat-color styling across all buttons, toggles, and knobs
- Consistent visual language throughout the interface
- Compact, plain text labels for maximum display space

---

## Requirements

- **JUCE** (download from https://juce.com)
- **CMake** 3.15+
- **C++17 compiler**

---

## Build (Windows example)

```bash
cmake -S . -B build -DJUCE_DIR="C:\path\to\JUCE"
cmake --build build --config Release
```

---

## Notes

- VST3/AU/Standalone targets are enabled in `CMakeLists.txt`.
- Change `JUCE_DIR` in `CMakeLists.txt` or pass `-DJUCE_DIR=...` when configuring.
- Standalone startup log: `%TEMP%\\EQPro_startup_*.log`.
- Standalone state restore is disabled by default; set `EQPRO_LOAD_STATE=1` to enable.
- Standalone audio device restore is disabled by default; set `EQPRO_LOAD_AUDIO_STATE=1` to enable.
- Standalone window position restore is disabled by default; set `EQPRO_LOAD_WINDOW_POS=1` to enable.

---

## Plugin Sections

### EQ Section (Layer 1)
- 12 parametric bands per channel
- Frequency, gain, Q, filter type, slope controls
- Per-band mix, bypass, solo
- Channel routing and Mid/Side targeting
- Dynamic processing per band

### Harmonic Section (Layer 2)
- Odd and even harmonic generation per band
- Independent mix controls for each harmonic type
- Per-band harmonic bypass
- Global oversampling controls
- Non-linear waveshaping for musical character

### Analyzer Section
- Pre-EQ, Post-EQ, and Harmonic curves
- Interactive band overlays
- Hover readouts and frequency response visualization
- Adjustable range, speed, and freeze controls

### Metering Section
- Goniometer with stereo width
- Correlation meter
- Output level meters (peak and RMS)
- Professional color-coded metering

### Control Section
- Phase mode selection (Real-time, Natural, Linear)
- Quality settings for linear modes
- Global mix, output trim, auto-gain
- Preset management and snapshots

---

**EQ Pro** - Professional EQ with Harmonic Processing for Modern Audio Production
