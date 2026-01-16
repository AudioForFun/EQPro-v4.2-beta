# EQ Pro Signal Flow and Call Graphs

## High-Level DSP Signal Flow

Input
  -> Global Bypass?
     -> EQDSP (per-channel, 12 bands)
        -> per-band channel targets (All/Mid/Side/L/R + immersive pairs)
        -> per-band mix
        -> per-band dynamic gain modulation (Up/Down)
     -> Spectral Dynamics (optional)
     -> Character Mode (Gentle/Warm saturation)
     -> Output Trim (smoothed gain)
  -> Metering + Correlation
  -> Analyzer taps (pre/post/external)
Output

## EQDSP (per channel)

Input channel
  -> (if smart solo: band-pass audition)
  -> For each band:
       - slope model (continuous dB/oct)
       - tilt / flat tilt handled as dual shelves
       - per-band mix (dry/wet)
       - dynamic detector (threshold/attack/release + auto scale)
  -> Output channel

## Dynamic EQ (per band)

Detector input:
  -> Band-pass detector
  -> Envelope follower (attack/release)
  -> Up/Down gain modulation against threshold

## Spectral Dynamics

Input
  -> Frame buffer (overlap-add)
  -> Windowed FFT
  -> Per-bin compression
  -> IFFT + overlap-add
  -> Dry/Wet mix
Output

## Analyzer

Pre FIFO -> FFT -> pre magnitude
Post FIFO -> FFT -> post magnitude
External FIFO -> FFT -> external magnitude
EQ curve + phase curve from band parameters

## Per-Class Call Graphs

### EQProAudioProcessor
prepareToPlay()
  -> EQDSP.prepare
  -> LinearPhaseEQ.prepare
  -> SpectralDynamicsDSP.prepare
  -> MeteringDSP.prepare

processBlock()
  -> EQDSP.process (min phase)
  -> LinearPhaseEQ.process (linear/natural)
  -> SpectralDynamicsDSP.process
  -> Character Mode saturation
  -> Output Trim (smoothed gain)
  -> MeteringDSP.process
  -> Analyzer FIFO push

timerCallback()
  -> rebuildLinearPhase()
  -> updateOversampling()

### EQDSP
process()
  -> solo audition (if enabled)
  -> per-band filter update + sample processing

### AnalyzerComponent
timerCallback()
  -> updateFft()
  -> updateCurves()

mouseDown/Drag/Up
  -> select band(s)
  -> update parameters (freq/gain)

### BandControlsPanel
setSelectedBand()
  -> updateAttachments()
  -> updateTypeUi()

copy/paste
  -> copyBandState()
  -> pasteBandState()

### SpectralDynamicsDSP
process()
  -> hop buffer
  -> FFT -> per-bin compression -> IFFT
  -> overlap-add
