# avisynth

Zig module for writing AviSynth+ plugins, using [avisynthplus-c-api-dynamic-loader](https://github.com/Asd-g/avisynthplus-c-api-dynamic-loader) to load the C API at runtime (no static link against `AviSynth.lib`) and the [AviSynthPlus](https://github.com/AviSynth/AviSynthPlus) headers for the C API types. Both are pinned git deps (see `build.zig.zon`).

## Consumer build

Add the dependency to your package:

```sh
zig fetch --save git+https://github.com/dnjulek/avisynth-zig.git
```

This pins the current commit in your `build.zig.zon` as `.avisynth`. Then wire it up:

```zig
// build.zig
const avs_dep = b.dependency("avisynth", .{
    .target = target,
    .optimize = optimize,
});
mod.addImport("avisynth", avs_dep.module("avisynth"));
```

The module carries the C++20 loader + shim sources and `link_libcpp`, so `addImport` is all a consumer needs — no extra linking.

## Usage

```zig
const avs = @import("avisynth");

// in avisynth_c_plugin_init:
const api = avs.getApi(env, avs.INTERFACE_VERSION, avs.INTERFACE_BUGFIX_VERSION, &.{
    "avs_add_function",
    "avs_get_frame",
}) catch {
    // avs.getLastError() has the reason
    return "failed to load AviSynth C API";
};
_ = api.avs_add_function.?(env, ...);
```

Call `getApi` once, from `avisynth_c_plugin_init`. The underlying loader initializes exactly once and does not retry after a failed first load.

- `avs.c` — raw types/constants from `avisynth_c.h` (`AVS_Value`, `AVS_VideoInfo`, ...).
- `avs.AvsApi` — the loaded function-pointer table. Hand-written field list (so ZLS can resolve/complete the fields), verified at comptime against the loader's `avs_c_api_functions.inc` (the same file its C++ side compiles with) — any drift is a compile error. Functions absent from the loaded avisynth library are `null` unless listed as required.
- `avs.api_function_names` — the parsed table entries, in order.

## Example

`example/` is a standalone package that consumes this module exactly as described above (a `zig fetch`-pinned git dep, like any external consumer) and builds a real plugin DLL:

```sh
cd example
zig build -Doptimize=ReleaseFast   # → zig-out/bin/invert_example.dll
```

It registers `ZInvert(clip, bool "enabled")`, a planar invert filter ported from the VapourSynth SDK invert example — see `example/src/invert_example.zig` for the full plugin-init/create/get_frame/free flow, and [`example/README.md`](example/README.md) for details.

To test it against an installed AviSynth+, run `example/test.bat` (needs `ffmpeg` in PATH): it loads `example/test.avs`, whose `Assert()`s check the filter per frame on every supported format (pixel math vs the built-in `Invert()`, pass-through, double-invert identity, alpha), then writes a `test_result.png` grid of the results. A failed assert aborts with a nonzero exit and the assert message.

## Build

```sh
zig build        # installs a static lib, but that's only a compile smoke-test — the product is the module
zig build test   # comptime derivation asserts + module tests
```

Note: `zig build test` verifies compile-time layout and wrappers only; actual DLL loading is exercised the first time a real plugin runs under an AviSynth host.

## License

MIT. Dependencies have their own licenses that apply to what a plugin ships: the dynamic loader is MPL-2.0, and the AviSynth+ headers are GPL with the C-interface exception.
