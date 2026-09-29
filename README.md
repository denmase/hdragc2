# HDRAGC2

A 1:1 port of **HDRAGC v0.1.5** (the AviSynth filter by LaTo INV./Paviko, 2005,
open-source release) to an **AviSynth+ 64-bit plugin written in Zig**, using the
[`dnjulek/avisynth-zig`](https://github.com/dnjulek/avisynth-zig) module
(AviSynth+ C API V8+, dynamic loading).

## Aurora — the next-generation function

`Aurora(clip, ...)` is a modern reconstruction inspired by the documented
HDRAGC v1.8.7 parameter set (the original is closed-source; semantics were
reconstructed from the archived documentation). Improvements over the 0.1.5
port, as designed in `hdragc2-desain.md`:

* **YUV (YV12) processing** — independent luma/chroma handling; chroma
  saturation is a clean scale around neutral 128 (v1.8.7 removed RGB32).
* **Pluggable local estimators** (`engine`): `"guided"` (self-guided filter,
  default), `"legacy"` (0.1.5 separable kernel), `"clahe"`.
* 1.8.7 parameters: `protect`, `passes`, `shift`, `shadows`, `shift_u/v`,
  `corrector`, `reducer`, `black_clip`, `freezer` — see the table below.

| parameter | default | source / meaning |
|---|---|---|
| avg_lum / max_gain / min_gain / coef_gain | 128 / 3.0 / 1.0 / 1.0 | as in 0.1.5 |
| max_sat / min_sat / coef_sat | **9.0 / 0.0 / 1.0** | 1.8.7 defaults (YUV chroma scale) |
| avg_window | -1 (= one second) | temporal window, frames |
| response | 100 | per-frame gain change limiter, % |
| mode | 2 | picks the default engine when `engine` is not given (1=legacy, 2=guided) |
| engine | from mode | "guided" / "legacy" / "clahe" |
| protect | 2 | 0=off, 1=on, 2=auto (on when a near-white pixel exists) |
| passes | 4 | legacy-estimator iterations (doc: "mode 1 only") |
| shift | 0 | fixed luma pre-shift |
| shadows | true | extra shadow-region enhancement curve |
| shift_u / shift_v | 0 | constant chroma offsets (white balance) |
| corrector | 0.0 | 1=no shaping; lower withholds gain from bright pixels |
| reducer | 0.5 | gain-map spatial smoothing, 0..2 |
| black_clip | 0.0 | fraction of darkest pixels pinned to black |
| freezer | -1 | >=0: freeze statistics from the first evaluated frame |
| radius / clip_limit / tiles | 7 / 2.0 / 8 | Aurora tuning (estimator radius, CLAHE clip & grid) |

Where the 1.8.7 documentation was ambiguous, the chosen interpretation is
marked [interp] in `src/aurora.zig`. `corrector_mode` is undocumented in the
original docs and not implemented.

Input: **YV12 8-bit** in v1 (`Aurora(ConvertToYV12(src))`).

## Build

Dependencies are vendored under `vendor/`, so the build works fully offline:

```bat
zig build -Doptimize=ReleaseFast
REM result: zig-out\bin\hdragc2.dll   (Windows)
REM    or : zig-out/lib/libhdragc2.so   (Linux)
```

Requires **Zig 0.16.0 or newer** (the minimum required by the vendored
`avisynth-zig` module).

## Usage

```avisynth
LoadCPlugin("path\to\hdragc2.dll")
HDRAGC(ConvertToRGB32(src))   ; input must be RGB32, exactly like 0.1.5
```

Parameters (defaults per the 0.1.5 source):

| parameter | default | description (from the original source) |
|---|---|---|
| avg_lum | 128 | target global average luma |
| max_gain / min_gain | 3.0 / 1.0 | global gain clamp |
| coef_gain | 1.0 | g = 1 + (g_raw-1)*coef_gain |
| max_sat / min_sat / coef_sat | 2.0 / 1.0 / 1.0 | saturation boost proportional to gain; coef_sat=0 -> auto |
| circle | 7 | radius of the local-luminance neighborhood |
| avg_window | 1 (`avg_window=-1` = half the fps) | temporal smoothing ring buffer |
| response | 100 | per-frame gain change limiter, in percent |
| sigma | 1.5 | width of the gaussian target distribution for histogram matching |
| debug | 0 | accepted, but the text overlay is **not** implemented (see deviations) |
| mode | 1 | 0 = full 2D pass, otherwise separable H+V approximation |

## Deviations from 0.1.5 (intentional, documented)

1. `abs(float)` in the C source depends on the compiler (VC6: `int`
   prototype, silently truncated). The port uses the intended float
   absolute value.
2. Non-RGB32 input: the original silently returned an uninitialized frame;
   the port rejects it with an explicit error. Add `ConvertToRGB32()`.
3. `sigma < 0`: original bug (fixed the local variable, not the member).
   The port fixes the value.
4. All-black frame (histogram bins 9..192 empty): the original divided by
   zero (NaN corrupted the frame). The port falls back to `min_gain`.
5. Out-of-bounds guard on `gauss[256]` in the YLUT loop.
6. Robustness guards: `avg_window=0` (original: crash), `circle=0`
   (original: 0/0), saturation index with `min_sat<1` (original: OOB read),
   `y_local<=0` (original: passes through inf before the clamp — identical
   result, handled explicitly).
7. Output alpha byte = 0, **preserved 1:1** from the original (flip one line
   in `hdragcGetFrame` if you want alpha preserved).
8. The `debug` text overlay (GDI/Antialiaser from the AviSynth core) is not
   ported.
9. `GetGain()` (the ConditionalFilter helper) is not ported — it needs
   access to the `current_frame` variable; may follow later.
10. MT: `set_cache_hints` returns `AVS_MT_SERIALIZED` (temporal state must
    be evaluated in order). AVS 2.5 was single-threaded, so this preserves
    original behavior.
11. Frame properties are carried to the output (`prop_src`), a small V8+
    improvement.

## Verification status

**NUMERICALLY VERIFIED** (2026-09-29, Linux sandbox, AviSynth+ 3.7.5 built
from source, C host test + independent Python reference implementation):

| Scenario | max_abs_diff | mean |
|---|---|---|
| identity (max_gain=1) | 0 | 0.00000 |
| dark (defaults, 8 frames, temporal) | 0 | 0.00000 |
| dark_m0 (mode=0, circle=5, avg_window=4, response=50) | 0 | 0.00000 |
| bright (gain approx 1) | 0 | 0.00000 |
| Aurora default (guided engine, temporal) | 2 | within tolerance |
| Aurora engine=clahe | 0 | 0.00000 (exact) |
| Aurora freezer+corrector+reducer+black_clip+shift | 0 | 0.00000 (exact) |

Plugin output is **pixel-per-pixel identical** to the Python reference on all
four scenarios, in both Debug and ReleaseFast builds. Comparison logic in
`test/reference.py`.

Note on running the host test: the Asd-g dynamic loader performs
`dlopen("libavisynth.so")` itself, so the host must be run with
`LD_LIBRARY_PATH=<directory containing libavisynth.so>`.

## Testing

```bat
REM Linux (with libavisynth.so built, see test/host.c header comment):
gcc -O2 -I vendor/avisynthplus/avs_core/include test/host.c \
    -o /tmp/host -L/path/to/avs_core -lavisynth -Wl,-rpath,/path/to/avs_core
LD_LIBRARY_PATH=/path/to/avs_core /tmp/host
python3 test/reference.py
python3 test/reference_aurora.py

REM Windows smoke test (AviSynth+ installed):
test\test.bat
```

## CI

GitHub Actions workflow in `.github/workflows/ci.yml`:
- **build** job: builds the plugin on Windows, Linux, and macOS and uploads
  the artifact.
- **test** job (Linux): builds AviSynth+ from source, runs the C host test,
  and cross-checks the output against the Python reference (must be an exact
  match).

## License

GPL-2.0-or-later, following the original HDRAGC source. The AviSynth+ headers
are GPL with a C-interface exception, so plugin code linked only through the
C API may use any license; this project stays GPL out of respect for the
original authors.
