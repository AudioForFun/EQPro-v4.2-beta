# EQ Pro Architecture

## DSP/UI Boundary (Refactor)
- `EQProAudioProcessor` owns APVTS, `EqEngine`, snapshots, and analyzer/meter taps.
- `EqEngine` in `src/dsp/` contains all audio processing state and logic (prepare/reset/process).
- `ParamSnapshot` is a fixed-size struct updated on the message thread and read atomically at block start.
- `AnalyzerTap` and `MeterTap` are DSP-side, lock-free bridges for UI visualization.
- `PluginEditor` talks to DSP only via APVTS attachments and read-only tap accessors.

## Performance Notes
- Snapshot swaps are hashed and applied only when parameters change.
- Linear-phase rebuilds are debounced to avoid repeated FIR regeneration during automation.
- Analyzer taps decimate at high sample rates / large buffers to reduce FIFO pressure.
- Metering updates are decimated at very high sample rates to lower CPU.
- Global mix and output trim use block ramps instead of per-sample smoothing loops.

## DSP Pipeline (Milestone 1)
- Per-channel processing pipeline with 12 fixed bands each.
- Each band is a minimum-phase IIR biquad (RBJ formulas).
- Parameters are stored in APVTS and read into a fixed-size cache.
- `EQDSP` owns the filters and processes channels in-place (invoked by `EqEngine`).
- Global bypass short-circuits processing.
- Band Solo audition routes audio through a band-pass version of the selected band(s).
- Per-band mix blends dry/wet for each band.
- Per-band channel targets can address all channels, M/S targets, L/R, and immersive pairs.
- Dynamic EQ modulates per-band gain using a detector envelope (Up/Down trigger modes).
- Output trim applies a post-processing gain stage.
- Smart Solo tightens the audition bandwidth and applies a small gain lift.

## Analyzer (Milestone 2)
- Pre/post analyzer taps into channel 1 via lock-free FIFO (`AnalyzerTap`).
- FFT runs on a UI timer (30 Hz) and uses windowed FFT.
- EQ curve is computed from current band parameters for display.
- External analyzer input can be enabled for overlay visualization.

## Channel Selection (Milestone 3)
- UI channel selector adapts to current bus layout.
- Labels derived from JUCE channel types (L, R, C, LFE, Ls, Rs, Ltf, Rtf, Ltr, Rtr, etc.).
- Selected channel controls which band parameters are edited.
- Per-band channel target selector supports L/R, M/S, and immersive pairs.

## Metering & Correlation (Milestone 4)
- Per-channel RMS/peak meters updated on a UI timer.
- Correlation meter uses the main L/R pair (channels 0/1).

## Spectral Dynamics
- Optional spectral dynamics processor uses short-time FFT with overlap-add.
- Per-bin compression with threshold/ratio/attack/release and dry/wet mix.

## Mid/Side (Milestone 5)
- Per-band Mid/Side target (All/Mid/Side) for stereo processing.

## Phase Modes (Milestone 6)
- Real-time: minimum-phase IIR (current biquad pipeline).
- Natural: short linear-phase FIR (lower latency).
- Linear: long linear-phase FIR with selectable quality and host latency reporting.
- Linear-phase IRs are windowed (Hann) and rebuilt only when parameters change.
- Optional oversampling parameter is present but currently disabled in the DSP path.
- Character modes (Gentle/Warm) apply a soft saturator (oversampled when enabled).

## Channel Mapping
- Processing uses JUCE bus layout channel order.
- Supported channel counts: 1..16 (Mono up to 9.1.6).

## Parameter List
See `docs/PARAMS.md`.

## Signal Flow Diagrams
See `docs/SIGNAL_FLOW.md`.

## Architecture Diagrams
See `docs/DIAGRAMS.md`.

## GUI
See `docs/GUI.md`.
