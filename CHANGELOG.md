# Changelog

## 0.2.2 (2026-10-02)

### Fixed (bug review)
- HDRAGC mode=0: `circle_mat` zero-initialized — the preserved asymmetric
  original loop never writes the +circle row/column and the 2D pass read
  malloc garbage (UB in ReleaseFast). Matches the Python reference, which
  already assumed zeros.
- HDRAGC: `max_sat < 1` is rejected with an explicit error (negative
  saturation-table size was UB); `max_gain == 1` with `coef_sat == 0` no
  longer divides by zero (propagated inf/NaN reached `@intFromFloat`).
- CLAHE: tiles starting at/past the frame edge (tiles larger than the frame,
  or exact multiples like w=20/tiles=6) underflowed usize or divided by
  zero. Tiles are clamped to min(w,h), empty tiles get an identity LUT, the
  clip conversion is clamped, and Aurora rejects `clip_limit < 0`.
- Aurora `freezer=N` now actually freezes frame N — frames before N take the
  normal temporal path — and the frozen state is cleared on non-sequential
  access so seeking stays deterministic. (Previously any freezer >= 0 froze
  the first frame the host happened to evaluate.)
- OOM robustness: `boxBlur`/`guidedFilter`/`clahe` propagate errors instead
  of returning with an uninitialized output buffer; `hdragcCreate` and
  `auroraCreate` free all earlier buffers via errdefer when a later
  allocation fails.
- CI: the vendored AviSynth+ build cache key now includes
  `hashFiles('vendor/avisynthplus/**')`.

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

## 0.2.1 (2026-09-29)

### Fixed (from real-footage testing feedback)
- Protect taper threshold unit bug in domain modes: limit is now
  fwd(204/gain) instead of fwd(204)/gain (in log domain the old formula
  tapered gain from gamma-luma ~5 upward, effectively disabling lifts).
  Regression test: cb_prot (bright ColorBars, domain=log, protect=1).
- Temporal pumping on seek/resume/scrub: non-sequential frame access now
  resets all temporal state (ring buffer, pg IIR, scene-cut history) so
  output is deterministic regardless of evaluation history.
- Temporal pumping during playback: degenerate frames (e.g. black decoder
  warm-up frames with no pixels in the analysis bins) no longer write
  min_gain into the temporal ring buffer; the previous gain is kept.

### Known issue
- The Linux C test host prints a glibc double-free message at PROCESS
  EXIT (after all output, after every filter instance is freed). The
  fault is in the C++ teardown path of this specific host setup, not in
  frame processing (16/16 scenarios pixel-exact) and not in the plugin's
  own allocations (verified with a guard-page allocator). Does not
  affect usage in AviSynth+ hosts.
