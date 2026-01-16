# EQ Pro Modules

## DSP
- `EQDSP`: per-channel EQ engine (12 bands). Handles IIR minimum-phase, tilt/flat tilt, slopes, per-band channel targets (all/MS/L/R + immersive pairs), smart solo audition, per-band mix, and dynamic band gain modulation.
- `Biquad`: biquad filter core used by EQ bands. Computes RBJ-style coefficients and runs sample-by-sample.
- `OnePole`: simple one-pole used for fractional HP/LP slope contributions.
- `LinearPhaseEQ`: FIR convolution engine for linear and natural phase modes with background impulse updates and latency reporting.
- `SpectralDynamicsDSP`: FFT-based multiband dynamics with overlap-add, threshold/ratio/attack/release/mix.
- `MeteringDSP`: RMS/peak metering and correlation for selected channel pairs.

## UI
- `PluginEditor`: main layout and global controls. Hosts analyzer (top), controls (mid), meters/correlation (right), and processing row (bottom). Handles resizing.
- `AnalyzerComponent`: spectrum analyzer (pre/post/external), EQ curve, per-band curve overlay, band points, and spectrum grab.
- `BandControlsPanel`: per-band controls (freq/gain/Q/type/slope/channel target/mix, bypass/solo, copy/paste, reset/delete).
- `MetersComponent`: multi-channel RMS/peak meters with peak readout and phase bar.
- `CorrelationComponent`: correlation meter/graph.
- `SpectralDynamicsPanel`: spectral dynamics controls (threshold/ratio/attack/release/mix).
- `LookAndFeel`: custom rotary knob styling, filmstrip knob rendering, and UI colors.
- `Theme`: dark theme palette and shared colors.

## Utilities
- `ParamIDs`: parameter IDs and name helpers.
- `RingBuffer`: lock-free audio transfer for analyzer data.
- `FFTUtils`: log-frequency mapping helpers.
- `Smoothing`: lightweight smoothing function.
- `ColorUtils`: band colors and UI accents.
- `ChannelLayoutUtils`: layout-to-label mapping for speaker channels and immersive labels.
