# EQ Pro GUI Guide

## Layout
- Top: global bypass + global mix + Undo/Redo + Save/Load + preset browser.
- Center/top: analyzer with pre/post/external spectra and EQ curves.
- Middle: band controls panel.
- Right: RMS/Peak toggles above the meters + multi-channel meters + goniometer + correlation meter.
- Bottom: processing row (phase mode + quality) and output trim/auto gain.

## Analyzer
- Drag band points to change frequency/gain.
- Shift-click to multi-select bands; drag to move all selected.
- Alt-drag to snap to nearby spectrum peaks (Spectrum Grab).
- Pre/Post/External spectra can be shown; view selection controls render.
- Per-band curves are overlaid with a highlighted selected band.
- The plot shows a small legend for Pre/Post/Ext.
- Dragging a band point shows a value pill (freq + gain).

## Band Controls
- Per-band: Frequency, Gain, Q, Mix, Filter Type (dropdown), Slope (dropdown).
- Band Mix is parallel dry/wet per band (dry + wet delta).
- FFT display ignores Band Mix so the curve represents the band filter only.
- Routing: format-specific targets for per-channel, stereo pairs, and Mid/Side pairs.
- Channel options are filtered to the current host layout and refresh on layout change.
- Channel selector sits under the rotary row (not inside Dynamic section).
- Bypass is toggled by double-clicking band buttons; active = bypass off.
- Solo is a per-band toggle under the 1–12 band row (exclusive per channel; double-click clears).
- Any band edit automatically un-bypasses that band.
- Dynamic EQ controls are currently hidden (reserved for a future release).
- Copy/Paste band state and Reset actions.
- Band header strip shows active band color and navigation arrows.
- EQ frame and row dividers follow the active band color.

## Processing / Global
- Phase mode: Real-time / Natural / Linear.
- Linear quality selection.
- Character modes: Gentle / Warm.
- Auto Gain + Gain Scale.
- Output Trim (smoothed).

## Presets / Snapshots
- Snapshot buttons and menu (Recall/Store).
- Preset browser with prev/next navigation in the top bar.

## Analyzer Options
- Range: 3/6/12/30 dB.
- Speed: Slow/Normal/Fast (adaptive).
- View: Pre/Post/Both.
- Freeze and External overlay toggles.

## MIDI
- MIDI Learn toggle and target selector.

## Resizing
- Fixed size (1125x735).

## Channel Routing Formats
The routing dropdown exposes only the targets that match the active plugin format.

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

## Visual Updates (v4.1 beta)
- RMS/Peak toggles sit above the meters on the right panel.
- Correlation meter includes -1/0/+1 ticks with a framed bar.
- Rotary knobs include a smaller inner LED ring that matches band color.

## Visual Updates (v4.2 beta)
- **FFT Analyzer Modernization**: 
  - Modern gradient background with subtle depth
  - Theme-based color scheme using accent colors (cyan/purple) for spectrum curves
  - Gradient fills under spectrum curves for modern appearance
  - Enhanced grid using theme colors with better visual hierarchy
  - Improved 0 dB reference line with accent color and subtle glow
  - Modernized legend with gradient background and enhanced swatches
  - Layered borders for depth and polish
- **Solo Toggle Borders**: 
  - Clear, well-defined dual-layer borders (outer + inner) for better visibility
  - Borders remain visible even with shading/gradients behind
  - State-aware styling (normal/on/hover/disabled) for visual feedback
- **Band Number Visibility**: 
  - Selected bands: white text for maximum contrast
  - Non-selected bands: brighter band color (brighter 0.4) with high alpha (0.95) for clear visibility
  - Bypassed bands: slightly dimmed but still visible (0.6 alpha)
- **Channel Selection Dropdown**: 
  - Width automatically adapts to longest possible channel names
  - Supports immersive formats (9.1.6, 7.1.4, etc.) with longest labels like "STEREO TOP MIDDLE"
  - Accommodates all channel types including top channels (TFL, TFR, TML, TMR, TRL, TRR), bottom channels (Bfl, Bfr), wide channels (Lw, Rw), and LFE2
- **UI Cleanup**: 
  - Removed residual lines under band toggles, solo toggles, and dropdown menus for cleaner appearance



