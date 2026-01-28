# EQ Pro Modules

## DSP
- `EqEngine`: top-level DSP router; switches phase modes, builds FIRs, applies quality/oversampling, aligns dry/wet, and feeds analyzer/meter taps. Includes adaptive linear quality, thread-safe FIR swaps, and crossfades to avoid artifacts.
- `EQDSP`: per-channel minimum-phase IIR engine (12 bands). Handles tilt/flat tilt, slopes, per-band channel targets (all/MS/L/R + immersive pairs), smart solo audition, per-band mix, dynamics, and harmonic generation.
- `Biquad`: RBJ-style biquad core for IIR bands, sample-accurate processing.
- `OnePole`: single-pole filter for fractional HP/LP slope contributions.
- `LinearPhaseEQ`: FIR convolution engine for Natural/Linear modes with background impulse updates and latency reporting.
- `SpectralDynamicsDSP`: FFT-based multiband dynamics (overlap-add, threshold/ratio/attack/release/mix).
- `MeteringDSP`: RMS/peak metering and correlation for selected channel pairs.

## UI
- `PluginEditor`: main layout and global controls. Hosts analyzer (top), controls (mid), meters/correlation (right), and processing row (bottom). Handles resizing.
- `AnalyzerComponent`: spectrum analyzer (pre/post/external), EQ curve, per-band curve overlay, band points, and spectrum grab. Throttles FFT updates in linear/natural modes to reduce CPU spikes.
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
