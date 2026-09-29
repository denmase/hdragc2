# HDRAGC2

A 1:1 port of **HDRAGC v0.1.5** (the AviSynth filter by LaTo INV./Paviko, 2005,
open-source release) to an **AviSynth+ 64-bit plugin written in Zig**, using the
[`dnjulek/avisynth-zig`](https://github.com/dnjulek/avisynth-zig) module
(AviSynth+ C API V8+, dynamic loading).

A separate `HDRAGC187()` function replicating the documented v1.8.7 parameter
set is planned — see `hdragc2-desain.md` for the design notes.

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
