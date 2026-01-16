# Real-Time Rules

- No heap allocations in `processBlock`.
- No mutex locks or blocking calls in the audio thread.
- Avoid I/O and logging in real-time processing.
- Preallocate all DSP state in `prepareToPlay`.
- Use lock-free communication for UI ↔ DSP (`AnalyzerTap`/`MeterTap`).
- Read a stable `ParamSnapshot` once per block; no APVTS reads in the audio thread.
- Prefer block ramps (e.g., `applyGainRamp`) over per-sample smoothing loops.
- Decimate analyzer/meter updates at high sample rates to reduce CPU load.
- Heavy FIR rebuilds should run off the audio thread (background job or message thread).
