# Project Rules

- Keep audio-thread code real-time safe.
- Avoid allocations and locks in `processBlock`.
- Use fixed-size containers for DSP state.
- Keep DSP and UI in separate modules.
- Update docs when adding parameters or changing DSP.
