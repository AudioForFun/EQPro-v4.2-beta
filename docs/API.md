# EQ Pro API Reference (DSP/UI Boundary)

## Purpose
This document explains the DSP/UI boundary and how other developers should interact with the engine safely.

## High‑Level Design
- `EQProAudioProcessor` owns **APVTS**, the **DSP engine**, snapshots, and analyzer/meter taps.
- `EqEngine` contains **all DSP state and processing** and must remain UI‑free.
- `PluginEditor` (and UI components) **never** call `EqEngine` directly.
- UI communicates via **APVTS attachments** and **read‑only accessors** for analyzer/meter data.

## Core Types

### `eqdsp::ParamSnapshot`
Location: `src/dsp/ParamSnapshot.h`  
Role: Fixed‑size, RT‑safe struct containing all parameters required by DSP.

Usage:
- Built on the message thread (`timerCallback`).
- Swapped atomically; audio thread reads a stable snapshot per block.
- Never allocated or resized in audio thread.

Key fields (non‑exhaustive):
- `globalBypass`, `globalMix`, `phaseMode`, `linearQuality`, `linearWindow`
- `outputTrimDb`, `characterMode`, `smartSolo`
- `autoGainEnabled`, `gainScale`, `phaseInvert`
- `bands[ch][band]` with freq/gain/Q/type/bypass/slope/mix/dynamic parameters
- `bands[ch][band].dynExternal` for per‑band external sidechain detection
- `msTargets[]` and `bandChannelMasks[]` for channel routing

### `eqdsp::EqEngine`
Location: `src/dsp/EqEngine.h/.cpp`  
Role: DSP‑only engine for prepare/reset/process.

Lifecycle:
1. `prepare(sampleRate, maxBlockSize, numChannels)`
2. `reset()`
3. `process(buffer, snapshot, detectorBuffer, preTap, postTap, meterTap)`
4. `updateLinearPhase(snapshot, sampleRate)` (called from timer)

Notes:
- No UI includes, no GUI dependencies.
- No heap allocations in `process`.
- Uses `ParamSnapshot` exclusively for parameters.
- Global dry/wet mix uses an internal delay line to align dry with linear-phase latency.
- Startup diagnostics write a log to `%TEMP%\\EQPro_startup_*.log`.
- Standalone state restore is disabled by default; set `EQPRO_LOAD_STATE=1` to enable.
- Standalone audio device restore is disabled by default; set `EQPRO_LOAD_AUDIO_STATE=1` to enable.
- Standalone window position restore is disabled by default; set `EQPRO_LOAD_WINDOW_POS=1` to enable.

### `eqdsp::AnalyzerTap`
Location: `src/dsp/AnalyzerTap.h/.cpp`  
Role: Lock‑free FIFO bridge for analyzer data.

Usage:
- DSP calls `push()` from audio thread.
- UI reads via `AudioFifo& getFifo()` on timer.

### `eqdsp::MeterTap`
Location: `src/dsp/MeterTap.h/.cpp`  
Role: DSP‑side metering bridge.

Usage:
- DSP calls `process()` on audio buffer each block.
- UI reads `getState()` and `getCorrelation()`.

## Audio Thread Rules
- No allocations, locks, or blocking in `processBlock`.
- No APVTS reads in audio thread; use snapshot only.
- Taps must be lock‑free.
- Prefer block ramps (`applyGainRamp`) over per‑sample smoothing loops.
- Decimate analyzer/meter updates at high sample rates to reduce CPU load.

## Processor ↔ UI Contract

### Processor Responsibilities
- Own APVTS, `EqEngine`, snapshots, and taps.
- Build snapshots in `timerCallback`.
- Expose read‑only accessors:
  - `getAnalyzerPreFifo()`, `getAnalyzerPostFifo()`, `getAnalyzerExternalFifo()`
  - `getMeterState()`, `getCorrelation()`

### UI Responsibilities
- Use APVTS attachments for parameters.
- Read analyzer/meter data via processor accessors only.
- Never include or reference `EqEngine`.

## v3.0 Beta Updates (DSP/UI Boundary)
- `BandControlsPanel` now maintains a **per-channel, per-band UI cache** for all band parameters.
  - Cache updates on UI edits and on timer-driven parameter reads.
  - Cache is re-applied to APVTS on band switches so **all bands stay active** and the analyzer reflects every band.
  - Cache access remains on the message thread; no audio-thread reads/writes.
- Analyzer point hit-testing now requires a **tighter center hit** to avoid overlapping band selection.

## v3.1 Beta Updates (DSP/UI Boundary)
- Band `freq` parameter is now capped at **20 kHz** across all channels/bands.
- The dark theme background and analyzer panel are now **true black** (no dark grey gradient or noise overlay).
- Band colour palette for 1–12 is **fully unique** (no repeated blue band LEDs).
- Analyzer curves now include a **band-coloured translucent fill** under each active band.
- Dynamic EQ now exposes **per-band dynamic gain delta (dB)** to the UI, and the analyzer curves
  reflect real-time gain reduction/expansion.

## v3.2 Beta Updates (DSP/UI Boundary)
- Natural/Linear modes now use **adaptive FIR tap lengths** based on band complexity
  (active band count, Q, gain, slope) to improve clarity without unnecessary latency.
- Linear-phase window selection now **auto-optimizes** (Hann/Blackman/Kaiser) for better stop-band attenuation.
- Linear modes add a **mixed-phase blend** (minimum-phase correction path) for improved transient response.
- Dynamic EQ detector now uses **peak/RMS blending** and **frequency weighting** for smoother, more musical action.
- Per-band **mix/threshold smoothing** reduces zippering during automation or UI tweaks.

## v3.5 Beta Updates (DSP/UI Boundary)
- Linear/Natural FIR build now uses **per-band mix-aware magnitude** and excludes MS-only bands
  from the full linear path to avoid double-processing.
- Linear/Natural output is **RMS-calibrated to realtime** for consistent meter levels across modes/quality.
- **Dynamic EQ UI and processing are disabled** (controls hidden; processing bypassed) until reintroduced.
- Goniometer replaces the dynamic panel area and uses **soft-clipped scaling** for stable visualization.

## v3.6 Beta Updates (DSP/UI Boundary)
- Linear/Natural convolution is now **thread-safe during impulse rebuilds**, preventing silent blocks
  when FIR partitions are updated at lower quality settings.
- Linear/Natural partition head size now **clamps to the prepared block size** to avoid silent
  output when higher-quality FIR taps exceed the convolution partition limits.
- Linear/Natural now **passes dry audio until impulses are ready**, avoiding dropouts during
  rebuilds for lower quality settings.
- Linear/Natural convolution is **forced to uniform partitioning** to prevent silence in lower
  quality modes (temporary stability measure).
- Linear/Natural impulse build now **falls back to a delta response** if the FIR magnitude
  collapses, preventing silent output even when coefficients become near-zero.
- Linear/Natural convolution **disables IR trimming** to prevent low-quality impulses from
  being trimmed to silence.
- Natural mode now **ignores the linear quality selector** (quality applies only to Linear).
- Added **Undo/Redo** and **Save/Load preset** actions to the top bar for faster workflow.
- Added **Reset Band** (renamed from Reset) and **Reset All** actions in the band section.

## v3.7 Beta Updates (DSP/UI Boundary)
- Auto gain now uses **pre/post RMS** comparison for consistent level matching.
- Band mix now **scales curve response only** while keeping EQ points fixed.
- Linear/Natural convolution uses a **double-buffered IR swap** to avoid dropouts.

## v3.8 Beta Updates (GUI/UX + Metering)
- Added **preset navigation bar** (prev/next + browser) to the top row.
- Added **meter focus toggle** (RMS vs Peak fill) and **peak hold tick**.
- Added **value readouts** for active knobs and dragged FFT points.
- Added **band header strip** with state icons (Bypass/Solo/Link).
- Improved **hit zones**, **hover/selection fades**, and **focus outlines**.
- Analyzer dB labels now **auto-hide** when the grid is dense to reduce overlap.

## v3.9 Beta Updates (Navigation + Metering)
- Added **RMS/Peak toggles** in the top bar for meter fill mode.
- Added **correlation meter bar** under the goniometer.
- Added **band navigation arrows** next to Reset All.
- Analyzer dB grid aligns with **exact 0 dB** reference line.

## v3.9 Beta Updates (Channel Routing)
- Channel routing dropdown now **adapts to the active host layout**, showing only
  the relevant targets for each format.
- Added **format-specific targets** for stereo pairs (Front/Rear/Lateral/Wide/Top),
  per-channel selections, and Mid/Side variants for each pair.
- Processor routing now maps **pair selections to the correct channel masks**
  and mirrors band parameters to all channels in the selected target.

## Channel Routing Formats
The `ms` parameter choices are dynamically filtered by layout, but the canonical
format lists are:

### Mono
- M

### Stereo
- ALL (STEREO)
- L
- R
- MID
- SIDE

### 2.1
- ALL (2.1)
- STEREO
- L
- R
- LFE
- MID
- SIDE

### 3.0
- ALL (3.0)
- STEREO
- L
- R
- C
- MID
- SIDE

### 3.1
- ALL (3.1)
- STEREO
- L
- R
- C
- LFE
- MID
- SIDE

### 4.0
- ALL (4.0)
- STEREO FRONT
- L
- R
- STEREO REAR
- Ls
- Rs
- MID FRONT
- MID REAR
- SIDE FRONT
- SIDE REAR

### 4.1
- ALL (4.1)
- STEREO FRONT
- L
- R
- LFE
- STEREO REAR
- Ls
- Rs
- MID FRONT
- MID REAR
- SIDE FRONT
- SIDE REAR

### 5.0 – Film (ITU)
- ALL (5.0)
- STEREO FRONT
- L
- R
- C
- STEREO REAR
- Ls
- Rs
- MID FRONT
- MID REAR

### 5.0 – Music (SMPTE)
- ALL (5.0)
- STEREO FRONT
- L
- R
- STEREO REAR
- Ls
- Rs
- C
- MID FRONT
- MID REAR
- SIDE FRONT
- SIDE REAR

### 5.1 – Film
- ALL (5.1)
- STEREO FRONT
- L
- R
- C
- LFE
- STEREO REAR
- Ls
- Rs
- MID FRONT
- MID REAR
- SIDE FRONT
- SIDE REAR

### 5.1 – Music
- ALL (5.1)
- STEREO FRONT
- L
- R
- STEREO REAR
- Ls
- Rs
- C
- LFE
- MID FRONT
- MID REAR
- SIDE FRONT
- SIDE REAR

### 6.0 – Film
- ALL (6.0)
- STEREO FRONT
- L
- R
- C
- STEREO REAR
- Ls
- Rs
- Cs
- MID FRONT
- MID REAR
- SIDE FRONT
- SIDE REAR

### 6.1 – Film
- ALL (6.1)
- STEREO FRONT
- L
- R
- C
- LFE
- STEREO REAR
- Ls
- Rs
- Cs
- MID FRONT
- MID REAR
- SIDE FRONT
- SIDE REAR

### 7.0 – Film
- ALL (7.0)
- STEREO FRONT
- L
- R
- C
- STEREO REAR
- Ls
- Rs
- STEREO LATERAL
- Lrs
- Rrs
- MID FRONT
- MID REAR
- MID LATERAL
- SIDE FRONT
- SIDE REAR
- SIDE LATERAL

### 7.1 – Film
- ALL (7.1)
- STEREO FRONT
- L
- R
- C
- LFE
- STEREO REAR
- Ls
- Rs
- STEREO LATERAL
- Lrs
- Rrs
- MID FRONT
- MID REAR
- MID LATERAL

### 7.1 – Music
- ALL (7.1)
- STEREO FRONT
- L
- R
- STEREO REAR
- Ls
- Rs
- C
- LFE
- STEREO LATERAL
- Lrs
- Rrs
- MID FRONT
- MID REAR
- MID LATERAL
- SIDE FRONT
- SIDE REAR
- SIDE LATERAL

### Dolby Atmos 7.1.2
- ALL (7.1.2)
- STEREO FRONT
- L
- R
- C
- LFE
- STEREO REAR
- Ls
- Rs
- STEREO LATERAL
- Lrs
- Rrs
- STEREO TOP FRONT
- Top Front Left (TFL)
- Top Front Right (TFR)
- MID FRONT
- MID REAR
- MID LATERAL
- MID TOP FRONT
- SIDE FRONT
- SIDE REAR
- SIDE LATERAL
- SIDE TOP FRONT

### Dolby Atmos 7.1.4
- ALL (7.1.4)
- STEREO FRONT
- L
- R
- C
- LFE
- STEREO REAR
- Ls
- Rs
- STEREO LATERAL
- Lrs
- Rrs
- STEREO TOP FRONT
- Top Front Left (TFL)
- Top Front Right (TFR)
- STEREO TOP REAR
- Top Rear Left (TRL)
- Top Rear Right (TRR)
- MID FRONT
- MID REAR
- MID LATERAL
- MID TOP FRONT
- SIDE FRONT
- SIDE REAR
- SIDE LATERAL
- SIDE TOP FRONT

### Dolby Atmos 9.1.6
- ALL (9.1.6)
- STEREO FRONT
- L
- R
- C
- LFE
- STEREO REAR
- Ls
- Rs
- STEREO LATERAL
- Lrs
- Rrs
- STEREO FRONT WIDE
- Front Wide Left (Lw)
- Front Wide Right (Rw)
- STEREO TOP FRONT
- Top Front Left
- Top Front Right
- STEREO TOP MIDDLE
- Top Middle Left
- Top Middle Right
- STEREO TOP REAR
- Top Rear Left
- Top Rear Right
- MID FRONT
- MID REAR
- MID LATERAL
- MID FRONT WIDE
- MID TOP FRONT
- MID TOP REAR
- MID TOP MIDDLE
- SIDE FRONT
- SIDE REAR
- SIDE LATERAL
- SIDE FRONT WIDE
- SIDE TOP FRONT
- SIDE TOP REAR
- SIDE TOP MIDDLE

## v4.1 Beta Updates (UI + Routing)
- RMS/Peak toggles moved to the **right panel above meters**.
- Correlation meter now shows **-1 / 0 / +1 graduations** with a clearer bar frame.
- EQ band section frame and divider lines now **follow the active band color**.
- Rotary knobs render **band-colored inner LED ring** to match the main LED row.

## v4.2 Beta Updates (UI Improvements)
- **FFT Analyzer Modernization**:
  - Analyzer background uses subtle gradient for depth
  - Spectrum curves use theme accent colors (cyan for pre, purple for post) with gradient fills
  - Grid lines use theme colors with improved visual hierarchy (major: 0.2 alpha, minor: 0.08 alpha)
  - 0 dB reference line enhanced with accent color and subtle glow
  - Legend modernized with gradient background and enhanced styling
  - Layered borders for modern appearance
- **Solo Toggle Styling**:
  - Dual-layer border system (outer 1.2px, inner 0.8px) for clear definition
  - Borders use full opacity `panelOutline` or `accent` color when on/hover
  - Ensures visibility even with shading/gradients behind
- **Band Number Visibility**:
  - Text color algorithm improved for better contrast
  - Selected bands use white text, non-selected use brighter band color (brighter 0.4, alpha 0.95)
  - Bypassed bands use dimmed color (alpha 0.6) but remain visible
- **Channel Selection Dropdown**:
  - Width calculation includes all immersive format channel names
  - Tests against longest possible labels: "STEREO TOP MIDDLE" (17 chars), "TML (TML/TMR)" (15 chars)
  - Automatically accommodates 9.1.6, 7.1.4, and other immersive formats
- **UI Cleanup**:
  - Removed residual divider lines under band toggles, solo toggles, and dropdown menus

## v4.4 Beta Updates (UI Harmonization, Space Optimization & Performance)
- **Critical Performance Optimizations**:
  - **Removed Expensive 3D Gradients**: Replaced all `ColourGradient` objects with fast flat colors
    - Text buttons: Flat `theme.panel` color with simple brightness adjustments
    - Toggle buttons: Flat colors with state-based brightness (no gradients or highlight overlays)
    - Rotary knobs: Flat `theme.panel` color (gradient and shadow removed)
    - Result: Instant rendering, no blocking operations, all controls appear immediately
  - **Deferred Timer Initialization**:
    - `AnalyzerComponent`: Timer starts in `resized()` instead of constructor
    - `BandControlsPanel`: Timer starts only after first resize
    - Prevents expensive repaints before components are properly laid out
  - **Buffered Rendering**:
    - `PluginEditor`, `BandControlsPanel`, and `AnalyzerComponent` use `setBufferedToImage(true)`
    - Reduces repaint overhead and improves rendering performance
  - **Result**: Plugin loads instantly with all controls visible from the start, no missing or partially rendered components
- **FFT Analyzer Label Optimization**:
  - Amplitude scale labels made more compact (leftGutter: 70px → 52px, labelWidth: 48px → 36px)
  - Removed background boxes from both amplitude and frequency labels for minimal, clean appearance
  - Plain text labels only - no rounded rectangle backgrounds or borders
  - More space available for FFT display while maintaining readability
  - Labels use left-aligned text with smaller font (9.5px) for compactness
- **Flat-Color Text Button Styling**:
  - Added `drawButtonBackground` override in `EQProLookAndFeel` for fast flat-color text buttons
  - Flat colors with simple state-based brightness adjustments (no gradients)
  - Consistent corner radius (4.0f), border style, and hover/pressed states
  - Applied to all text buttons:
    * Preset section: Copy, Paste, Reset, Reset All, Save, Load, Prev, Next, Refresh
    * EQ control section: Copy, Paste, Reset Band, Reset All, Band navigation (< >)
    * Snapshot buttons: A, B, C, D, Store, Recall
    * Undo/Redo buttons
  - Harmonized visual language across entire GUI (knobs, toggles, and buttons) with flat colors

## Call Flow (Simplified)
1. UI updates APVTS parameters.
2. `timerCallback` builds `ParamSnapshot` and swaps it.
3. `processBlock` reads stable snapshot and processes via `EqEngine`.
4. `AnalyzerTap`/`MeterTap` provide data to UI via lock‑free reads.

## Call Flow Diagram (Mermaid)
```mermaid
flowchart LR
  UI[PluginEditor + UI] -->|APVTS attachments| APVTS[AudioProcessorValueTreeState]
  APVTS -->|timerCallback| SNAP[ParamSnapshot (double buffer)]
  SNAP -->|atomic swap| PROC[EQProAudioProcessor::processBlock]
  PROC -->|process()| ENG[EqEngine]
  ENG -->|push()| PRE[AnalyzerTap (pre)]
  ENG -->|push()| POST[AnalyzerTap (post)]
  ENG -->|process()| METER[MeterTap]
  PRE -->|read FIFO| UI
  POST -->|read FIFO| UI
  METER -->|read state| UI
```

For larger architecture and threading diagrams, see `docs/DIAGRAMS.md`.

## Method Summary (Core API)

### `EQProAudioProcessor`
| Method | Thread | Purpose |
| --- | --- | --- |
| `prepareToPlay()` | audio | Initializes DSP engine and taps. |
| `processBlock()` | audio | Processes audio using a stable `ParamSnapshot`. |
| `timerCallback()` | message | Builds next snapshot and updates linear phase. |
| `getAnalyzerPreFifo()` | UI | Read-only access to pre analyzer FIFO. |
| `getAnalyzerPostFifo()` | UI | Read-only access to post analyzer FIFO. |
| `getMeterState()` | UI | Read-only access to meter state. |
| `getCorrelation()` | UI | Read-only access to correlation. |

### `EqEngine`
| Method | Thread | Purpose |
| --- | --- | --- |
| `prepare()` | audio | Preallocates DSP state. |
| `reset()` | audio | Clears DSP state. |
| `process()` | audio | Processes buffer using snapshot. |
| `updateLinearPhase()` | message | Rebuilds FIR when params change. |

### `AnalyzerTap`
| Method | Thread | Purpose |
| --- | --- | --- |
| `prepare()` | message | Preallocates FIFO. |
| `push()` | audio | Writes analyzer samples (lock-free). |
| `getFifo()` | UI | Read-only FIFO access. |

### `MeterTap`
| Method | Thread | Purpose |
| --- | --- | --- |
| `prepare()` | message | Preallocates meter state. |
| `process()` | audio | Updates meter values. |
| `getState()` | UI | Read-only meter values. |
| `getCorrelation()` | UI | Read-only correlation value. |

## Parameter Summary
### Global Parameters
- `globalBypass`: Master bypass switch for the entire EQ.
- `globalMix`: Global dry/wet mix (percentage).
- `phaseMode`: Processing mode (Real-time / Natural / Linear).
- `linearQuality`: FIR quality selector (Linear mode only).
- `linearWindow`: FIR window selection.
- `oversampling`: Oversampling selector (non-realtime modes).
- `outputTrim`: Output trim gain (dB).
- `autoGainEnable`: RMS-based auto-gain enable.
- `gainScale`: Auto-gain intensity scale (percentage).
- `phaseInvert`: Output phase inversion.
- `analyzerRange`: Analyzer dB range.
- `analyzerSpeed`: Analyzer speed.
- `analyzerView`: Analyzer view (Pre/Post/Both).
- `analyzerFreeze`: Freeze analyzer.
- `analyzerExternal`: External overlay toggle.
- `qMode` / `qModeAmount`: Q behavior and weighting.
- `characterMode`: Gentle/Warm character mode.
- `spectralEnable` + spectral params: Spectral dynamics controls (currently disabled).
- `midiLearn` / `midiTarget`: MIDI learn state and target.
- `smartSolo`: Smart solo toggle.

### Per‑Band Parameters (per channel/band)
- `freq`: Band center frequency (Hz).
- `gain`: Band gain (dB).
- `q`: Band Q value.
- `type`: Filter type.
- `bypass`: Band bypass.
- `ms`: Channel target (All/Mid/Side/etc.).
- `slope`: Filter slope (dB/oct).
- `solo`: Band solo.
- `mix`: Band dry/wet mix (percentage).

