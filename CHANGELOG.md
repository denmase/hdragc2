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
- Aurora(): next-generation function reconstructing the documented HDRAGC
  v1.8.7 parameter set in YV12 — engines "guided"/"legacy"/"clahe",
  protect, passes, shift, shadows, shift_u/v, corrector, reducer,
  black_clip, freezer, plus radius/clip_limit/tiles tuning. Cross-checked
  against an independent Python reference (CLAHE and freezer scenarios
  pixel-exact; guided within 2 levels).
- Refactor: shared code (weights, gauss, box blur, guided filter, CLAHE,
  YLUT) moved to src/common.zig; HDRAGC 0.1.5 output verified unchanged
  (pixel-exact on all four scenarios).
- Known caveat: the plugin must be tested against an AviSynth+ library built
  from the same source commit as the vendored headers (avisynth-zig pins
  AviSynthPlus@cfdaf8e); a header/library version mismatch crashes
  avs_invoke. The CI test job builds the library from vendor/avisynthplus
  for exactly this reason.
