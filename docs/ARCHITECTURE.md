# EQ Pro Architecture

## DSP Pipeline (Milestone 1)
- Per-channel processing pipeline with 12 fixed bands each.
- Each band is a minimum-phase IIR biquad (RBJ formulas).
- Parameters are stored in APVTS and read into a fixed-size cache.
- `EQDSP` owns the filters and processes channels in-place.
- Global bypass short-circuits processing.
- Band Solo audition routes audio through a band-pass version of the selected band(s).
- Per-band mix blends dry/wet for each band.
- Per-band channel targets can address all channels, M/S targets, L/R, and immersive pairs.
- Dynamic EQ modulates per-band gain using a detector envelope (Up/Down trigger modes).
- Output trim applies a post-processing gain stage.
- Smart Solo tightens the audition bandwidth and applies a small gain lift.

## Analyzer (Milestone 2)
- Pre/post analyzer taps into channel 1 via lock-free FIFO.
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

## GUI
See `docs/GUI.md`.
