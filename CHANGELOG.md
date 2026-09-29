# Changelog

## 0.1.5-zig (2026-09-29)

- Initial release: 1:1 port of HDRAGC v0.1.5 (LaTo INV./Paviko, 2005) to Zig.
- AviSynth+ C API via the avisynth-zig module (dynamic loader, no static
  link against AviSynth).
- RGB32 input, all 14 original parameters.
- Numerically verified: pixel-exact against an independent Python reference
  on 4 scenarios (identity, dark, mode=0 + temporal, bright), Debug and
  ReleaseFast.
- Documented intentional deviations from the original (see README).
- Planned: HDRAGC187() replicating the documented v1.8.7 parameter set in
  YUV space (protect, corrector, reducer, black_clip, freezer, shift_u/v).
