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

Input: **YV12 or YUV444P 8-bit** (`Aurora(ConvertToYV12(src))`).

### Aurora usage examples

Complete, ready-to-run script — pick one source filter (the commented
lines show the common choices; each needs its own plugin DLL):

```avisynth
LoadCPlugin("C:\App\avs64\plugins\hdragc2.dll")

# ---------- 1. open the source ----------
# Uncomment whichever matches your input and installed plugins:
# src = FFmpegSource2("input.mp4")        # needs FFmpegSource2.dll
# src = LSMASHVideoSource("input.mp4")    # needs LSMASHSource.dll
# src = DGSource("index.dgi")             # needs DGDecodeNV (NVENC)
src = AVISource("input.avi")              # built-in (uncompressed AVI)

# Aurora works in YUV: convert once, reuse for every variant.
# (Use ConvertToYUV444() instead for the 4:4:4 path.)
src_yv12 = ConvertToYV12(src)

# ---------- 2. process ----------
# Basic: automatic shadow lift with defaults
a = Aurora(src_yv12)

# Cinematic: protect highlights, log domain for natural midtones,
# tamed saturation (1.8.7's default max_sat=9.0 is very aggressive)
b = Aurora(src_yv12, radius=9, corrector=0.85, protect=1, \
           reducer=0.8, domain="log", max_sat=3.0)

# Noisy footage with scene cuts: temporal gain-map smoothing, with
# scene-change detection resetting temporal state
c = Aurora(src_yv12, pg_smooth=0.5, scene_cut=0.3, avg_window=-1, \
           response=30)

# White balance + CLAHE engine instead of the guided filter
d = Aurora(src_yv12, engine="clahe", clip_limit=1.5, tiles=8, \
           shift_u=2, shift_v=-1)

# ---------- 3. compare on a split screen ----------
StackHorizontal(a, b, c, d)

# Or write one variant out:
# return b
```

Notes:
* Multi-line calls use AviSynth+ line continuation (`\` at the start of
  the continuation line).
* All four variants share `src_yv12`, so they see identical input —
  differences on screen come only from Aurora's parameters.
* HDRAGC (the 0.1.5 port) is RGB32-only and stateless per frame:
  `HDRAGC(ConvertToRGB32(src))` — kept for behavior compatibility.

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

**NUMERICALLY VERIFIED — 15/15 scenarios, pixel-exact (max_abs_diff = 0) in
both Debug and ReleaseFast builds** (2026-09-29, Linux sandbox, AviSynth+
built from the vendored pinned source, C host test rendering synthetic clips
+ SMPTE ColorBars, independent Python reference implementations in
`test/reference.py` and `test/reference_aurora.py`).

| Group | Scenarios | Result |
|---|---|---|
| HDRAGC 0.1.5 | identity, dark, dark mode=0+temporal, bright | exact 0 |
| Aurora engines | guided default, legacy, clahe (flat + ColorBars) | exact 0 |
| Aurora domain | gamma, linear, log (LUT-based transforms) | exact 0 |
| Aurora temporal | avg_window/response ring buffer, freezer, pg_smooth IIR, scene_cut | exact 0 |
| Aurora formats | YV12, YUV444P | exact 0 |

Notes learned during verification (baked into the test design):
* The reference must run on the plugin's *input* (dumped src), never on its
  output — re-processing already-lifted frames is near-idempotent and hides
  bugs (circular-verification trap).
* Flat-color clips cannot distinguish engines/configurations; SMPTE
  ColorBars is the minimum non-uniform test pattern.

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
