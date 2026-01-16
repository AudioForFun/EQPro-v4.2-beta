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

## Call Flow (Simplified)
1. UI updates APVTS parameters.
2. `timerCallback` builds `ParamSnapshot` and swaps it.
3. `processBlock` reads stable snapshot and processes via `EqEngine`.
4. `AnalyzerTap`/`MeterTap` provide data to UI via lock‑free reads.

