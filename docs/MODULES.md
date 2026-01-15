# EQ Pro Modules

## DSP
- `EQDSP`: per-channel EQ engine (24 bands). Handles IIR minimum-phase, tilt/flat tilt, slopes, per-band M/S or L/R routing, smart solo audition, and per-band dynamic EQ (attack/release, threshold, mix). Supports optional external detector buffer for sidechain.
- `Biquad`: biquad filter core used by EQ bands. Computes RBJ-style coefficients and runs sample-by-sample.
- `OnePole`: simple one-pole used for fractional HP/LP slope contributions.
- `LinearPhaseEQ`: FIR convolution engine for linear and natural phase modes with background impulse updates and latency reporting.
- `SpectralDynamicsDSP`: FFT-based multiband dynamics with overlap-add, threshold/ratio/attack/release/mix.
- `EllipticDSP`: low-end mono-maker (elliptic/side low-pass) with bypass, cutoff, amount.
- `MeteringDSP`: RMS/peak metering and correlation for selected channel pairs.

## UI
- `PluginEditor`: main layout and global controls. Hosts meters (left), analyzer (center), and controls/correlation (right). Handles theme, presets, snapshots, MIDI learn, and resizing.
- `AnalyzerComponent`: spectrum analyzer (pre/post/external), EQ curve, phase overlay, band points with multi-selection and spectrum grab (Alt drag). Supports snap-to-peak and band labels.
- `BandControlsPanel`: per-band controls (freq/gain/Q/type/slope/MS/LR, bypass/solo, dynamic settings, sidechain source/filter, mix, link pairs, copy/paste).
- `MetersComponent`: RMS/peak meters with peak readout.
- `CorrelationComponent`: correlation meter/graph for selectable channel pairs.
- `EllipticPanel`: elliptic mono-maker controls and meter.
- `SpectralDynamicsPanel`: spectral dynamics controls (threshold/ratio/attack/release/mix).
- `LookAndFeel`: custom rotary knob styling and UI colors.
- `Theme`: dark/light theme palettes and shared colors.

## Utilities
- `ParamIDs`: parameter IDs and name helpers.
- `RingBuffer`: lock-free audio transfer for analyzer data.
- `FFTUtils`: log-frequency mapping helpers.
- `Smoothing`: lightweight smoothing function.
- `ColorUtils`: band colors and UI accents.
- `ChannelLayoutUtils`: layout-to-label mapping and linkable speaker pairs.
