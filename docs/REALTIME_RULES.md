# Real-Time Rules

- No heap allocations in `processBlock`.
- No mutex locks or blocking calls in the audio thread.
- Avoid I/O and logging in real-time processing.
- Preallocate all DSP state in `prepareToPlay`.
- Use lock-free communication for UI ↔ DSP.
