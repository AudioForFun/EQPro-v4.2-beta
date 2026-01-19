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

