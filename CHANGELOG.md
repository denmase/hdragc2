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

## 0.2.0 (2026-09-29)

### Added — Aurora()
- `Aurora()`: full reconstruction of the documented HDRAGC v1.8.7 parameter
  set in YUV: protect, passes, shift, shadows, shift_u/v, corrector, reducer,
  black_clip, freezer — plus next-generation improvements:
  - engines: "guided" (self-guided filter, default), "legacy" (0.1.5
    separable kernel), "clahe" (contrast-limited adaptive histogram
    equalization);
  - `domain`: "gamma" | "linear" | "log" via shared 256-entry LUTs
    (src/tables/, bit-identical with the reference implementation);
  - temporal stability: per-pixel gain-map IIR (`pg_smooth`) with automatic
    scene-cut detection (`scene_cut`) resetting all temporal state;
  - YUV444P input support alongside YV12.
### Fixed
- Out-of-bounds AVS_Value reads for omitted optional args (avs_array_size
  returns the DECLARED parameter count with padded placeholders — always
  type-check values before converting).
- Off-by-one argument indices from `mode` onward (signature contains
  `[debug]b` at slot 10).
- f32 plateau in the gauss CDF walk halting histogram matching early
  (comparison is now `>=`, letting the clamp handle exact-equality plateaus).
- Debug-build-only diagnostics removed; no functional change.
### Verification
- 15/15 scenarios pixel-exact (max_abs_diff = 0) vs independent Python
  references, Debug AND ReleaseFast, including SMPTE ColorBars content.
