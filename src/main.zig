//! HDRAGC — a 1:1 port of the open-source HDRAGC v0.1.5 (LaTo INV./Paviko, 2005)
//! to an AviSynth+ plugin in Zig, using the `avisynth-zig` bindings by dnjulek.
//!
//! Function: HDRAGC(clip, int "avg_lum"=128, float "max_gain"=3.0,
//!   float "min_gain"=1.0, float "coef_gain"=1.0, float "max_sat"=2.0,
//!   float "min_sat"=1.0, float "coef_sat"=1.0, int "circle"=7,
//!   int "avg_window"=1, int "response"=100, float "sigma"=1.5,
//!   int "debug"=0, int "mode"=1)
//!
//! Input: RGB32 only (exactly like 0.1.5). Intentional deviations are
//! documented in README.md.

const std = @import("std");
const avs = @import("avisynth");
const c = avs.c;

const allocator = std.heap.c_allocator;

var api: *const avs.AvsApi = undefined;

// ---------------------------------------------------------------------------
// Ported helper: the original author's custom round().
// (int)val in C truncates toward zero; @intFromFloat does the same.
// Note: behaves "oddly" for negative input — preserved 1:1 from the original.
// ---------------------------------------------------------------------------
fn roundOrig(val: f32) i32 {
    var r: i32 = @intFromFloat(val);
    if (@as(i32, @intFromFloat(val - 0.5)) == r) r += 1;
    return r;
}

// ---------------------------------------------------------------------------
// Edge-aware weight table, built at comptime (256 KB in .rodata).
// The original computed this at runtime in the constructor:
//   weights[i][j] = expf(-powf(fabsf((logf(i+.1)-logf(j+.1))/logf(5)), 25))
// DEVIATION #1: the C source calls abs() on a float (VC6 prototype: int),
// which is compiler-dependent and almost certainly unintended. The port uses
// the mathematically intended float absolute value.
// ---------------------------------------------------------------------------
const weights = blk: {
    @setEvalBranchQuota(10_000_000);
    var w: [256][256]f32 = undefined;
    const log5 = @log(@as(f32, 5.0));
    for (0..256) |i| {
        const li = @log(@as(f32, @floatFromInt(i)) + 0.1);
        for (0..256) |j| {
            const lj = @log(@as(f32, @floatFromInt(j)) + 0.1);
            const t = @abs(li - lj) / log5;
            w[i][j] = @exp(-std.math.pow(f32, t, 25.0));
        }
    }
    break :blk w;
};

const RLUM: f32 = 0.3086;
const GLUM: f32 = 0.6094;
const BLUM: f32 = 0.0820;

const HdrAgcData = struct {
    // parameters (names and order match the original source)
    avg_lum: i32,
    max_gain: f32,
    min_gain: f32,
    coef_gain: f32,
    max_sat: f32,
    min_sat: f32,
    coef_sat: f32,
    circle: i32,
    avg_window: i32,
    response: i32,
    sigma: f32,
    verbose: i32,
    mode: i32,

    // temporal state (mutated in get_frame -> filter must be serialized)
    prev_gain: []f32,
    last_gain: f32 = 0.0,
    index: usize = 0,

    // per-instance tables
    gauss: [256]f32,

    // dimensions
    width: usize,
    height: usize,
    pixels: usize,

    // full-frame buffers (replacing the original int** / float**, but flat)
    r: []u8,
    g: []u8,
    b: []u8,
    y_lum: []u8,
    y_local: []f32,
    circle_mat: []u8, // (2*circle+1)^2

    // saturation matrix coefficients
    sat_a: []f32,
    sat_b: []f32,
    sat_c: []f32,
    sat_d: []f32,
    sat_e: []f32,
    sat_f: []f32,
};

fn argDefined(args: c.AVS_Value, i: usize) bool {
    return c.avs_defined(c.avs_array_elt(args, @intCast(i))) != 0;
}
fn argInt(args: c.AVS_Value, i: usize) i64 {
    return c.avs_as_int(c.avs_array_elt(args, @intCast(i)));
}
fn argFloat(args: c.AVS_Value, i: usize) f64 {
    return c.avs_as_float(c.avs_array_elt(args, @intCast(i)));
}

fn hdragcCreate(env: ?*c.AVS_ScriptEnvironment, args: c.AVS_Value, user_data: ?*anyopaque) callconv(.c) c.AVS_Value {
    _ = user_data;

    var fi: [*c]c.AVS_FilterInfo = undefined;
    const clip = api.avs_new_c_filter.?(env, &fi, c.avs_array_elt(args, 0), 1);
    defer api.avs_release_clip.?(clip);

    if (c.avs_has_video(&fi.*.vi) == 0)
        return c.avs_new_value_error("HDRAGC: input clip must have video.");
    // DEVIATION #2: the original silently returned an uninitialized frame
    // for non-RGB32 input. The port rejects it with an explicit message
    // (the user adds ConvertToRGB32() in the script, as intended).
    if (c.avs_is_rgb32(&fi.*.vi) == 0)
        return c.avs_new_value_error("HDRAGC (0.1.5 port): RGB32 input only — add ConvertToRGB32().");

    const vi: c.AVS_VideoInfo = fi.*.vi;
    const width: usize = @intCast(vi.width);
    const height: usize = @intCast(vi.height);
    const pixels = width * height;

    // ---- argument parsing (defaults from Create_HDRAGC in the original) ----
    const avg_lum: i32 = if (argDefined(args, 1)) @intCast(argInt(args, 1)) else 128;
    const max_gain: f32 = if (argDefined(args, 2)) @floatCast(argFloat(args, 2)) else 3.0;
    const min_gain: f32 = if (argDefined(args, 3)) @floatCast(argFloat(args, 3)) else 1.0;
    const coef_gain: f32 = if (argDefined(args, 4)) @floatCast(argFloat(args, 4)) else 1.0;
    const max_sat: f32 = if (argDefined(args, 5)) @floatCast(argFloat(args, 5)) else 2.0;
    const min_sat: f32 = if (argDefined(args, 6)) @floatCast(argFloat(args, 6)) else 1.0;
    var coef_sat: f32 = if (argDefined(args, 7)) @floatCast(argFloat(args, 7)) else 1.0;
    var circle: i32 = if (argDefined(args, 8)) @intCast(argInt(args, 8)) else 7;
    var avg_window: i32 = if (argDefined(args, 9)) @intCast(argInt(args, 9)) else 1;
    var response: i32 = if (argDefined(args, 10)) @intCast(argInt(args, 10)) else 100;
    var sigma: f32 = if (argDefined(args, 11)) @floatCast(argFloat(args, 11)) else 1.5;
    const verbose: i32 = if (argDefined(args, 12)) @intCast(argInt(args, 12)) else 0;
    const mode: i32 = if (argDefined(args, 13)) @intCast(argInt(args, 13)) else 1;

    // ---- original constructor logic ----
    if (coef_sat == 0.0)
        coef_sat = (max_sat - min_sat) / (max_gain - 1.0);

    if (avg_window == -1) {
        const fps: f32 = @as(f32, @floatFromInt(vi.fps_numerator)) / @as(f32, @floatFromInt(vi.fps_denominator));
        avg_window = @divTrunc(@as(i32, @intFromFloat(@ceil(fps))), 2);
    }
    // DEVIATION #9: the original crashed (modulo by zero) with avg_window=0
    // and produced 0/0 NaN with circle=0. The port clamps both to a minimum
    // of 1.
    if (avg_window < 1) avg_window = 1;
    if (response < 1) response = 1;
    if (circle < 1) circle = 1;

    // DEVIATION #3: the original code fixed the local variable (not the
    // member) when sigma<0, so a negative sigma stayed in use (producing a
    // nonsensical gauss table). The port deliberately fixes the value.
    if (sigma < 0.0) sigma = 1.5;

    const d = allocator.create(HdrAgcData) catch
        return c.avs_new_value_error("HDRAGC: out of memory.");
    d.* = .{
        .avg_lum = avg_lum,
        .max_gain = max_gain,
        .min_gain = min_gain,
        .coef_gain = coef_gain,
        .max_sat = max_sat,
        .min_sat = min_sat,
        .coef_sat = coef_sat,
        .circle = circle,
        .avg_window = avg_window,
        .response = response,
        .sigma = sigma,
        .verbose = verbose,
        .mode = mode,
        .prev_gain = allocator.alloc(f32, @intCast(avg_window)) catch
            return c.avs_new_value_error("HDRAGC: out of memory."),
        .gauss = undefined,
        .width = width,
        .height = height,
        .pixels = pixels,
        .r = allocator.alloc(u8, pixels) catch return c.avs_new_value_error("HDRAGC: out of memory."),
        .g = allocator.alloc(u8, pixels) catch return c.avs_new_value_error("HDRAGC: out of memory."),
        .b = allocator.alloc(u8, pixels) catch return c.avs_new_value_error("HDRAGC: out of memory."),
        .y_lum = allocator.alloc(u8, pixels) catch return c.avs_new_value_error("HDRAGC: out of memory."),
        .y_local = allocator.alloc(f32, pixels) catch return c.avs_new_value_error("HDRAGC: out of memory."),
        .circle_mat = allocator.alloc(u8, @intCast((2 * circle + 1) * (2 * circle + 1))) catch
            return c.avs_new_value_error("HDRAGC: out of memory."),
        .sat_a = undefined,
        .sat_b = undefined,
        .sat_c = undefined,
        .sat_d = undefined,
        .sat_e = undefined,
        .sat_f = undefined,
    };
    @memset(d.prev_gain, 0.0);

    // ---- gauss table (identical to the original constructor) ----
    {
        const length: f32 = 4.0;
        const pi: f32 = 3.141593;
        const alpha: f32 = 1.0 / (d.sigma * @sqrt(2.0 * pi));
        const mu: f32 = (-@as(f32, @floatFromInt(avg_lum)) + 128.0) * (2.0 * length) / 256.0;
        var suma: f32 = 0.0;
        for (0..256) |i| {
            const x: f32 = length - 2.0 * length * @as(f32, @floatFromInt(i)) / 255.0;
            const dx = x - mu;
            const prob = alpha * @exp(-(dx * dx) / (2.0 * d.sigma * d.sigma));
            suma += prob;
            d.gauss[i] = suma * @as(f32, @floatFromInt(pixels));
        }
        for (0..256) |i| d.gauss[i] /= suma;
    }

    // ---- circle matrix. ORIGINAL BUG (preserved): the original loop
    // `for (x = -circle; x < circle; x++)` is asymmetric (skips +circle).
    {
        const csz: usize = @intCast(2 * circle + 1);
        var x: i32 = -circle;
        while (x < circle) : (x += 1) {
            var y: i32 = -circle;
            while (y < circle) : (y += 1) {
                const inside = (x * x + y * y) <= circle * circle;
                d.circle_mat[@intCast(x + circle + @as(i32, @intCast(csz)) * (y + circle))] = if (inside) 1 else 0;
            }
        }
    }

    // ---- saturation matrix coefficient tables ----
    {
        const sat_size: usize = @as(usize, @intFromFloat((max_sat - 1.0) * 100.0)) + 1;
        d.sat_a = allocator.alloc(f32, sat_size) catch return c.avs_new_value_error("HDRAGC: out of memory.");
        d.sat_b = allocator.alloc(f32, sat_size) catch return c.avs_new_value_error("HDRAGC: out of memory.");
        d.sat_c = allocator.alloc(f32, sat_size) catch return c.avs_new_value_error("HDRAGC: out of memory.");
        d.sat_d = allocator.alloc(f32, sat_size) catch return c.avs_new_value_error("HDRAGC: out of memory.");
        d.sat_e = allocator.alloc(f32, sat_size) catch return c.avs_new_value_error("HDRAGC: out of memory.");
        d.sat_f = allocator.alloc(f32, sat_size) catch return c.avs_new_value_error("HDRAGC: out of memory.");
        for (0..sat_size) |i| {
            const fs: f32 = @floatFromInt(i);
            d.sat_a[i] = (1.0 - (1.0 + fs / 100.0)) * RLUM;
            d.sat_b[i] = d.sat_a[i] + (1.0 + fs / 100.0);
            d.sat_c[i] = (1.0 - (1.0 + fs / 100.0)) * GLUM;
            d.sat_d[i] = d.sat_c[i] + (1.0 + fs / 100.0);
            d.sat_e[i] = (1.0 - (1.0 + fs / 100.0)) * BLUM;
            d.sat_f[i] = d.sat_e[i] + (1.0 + fs / 100.0);
        }
    }

    fi.*.user_data = d;
    fi.*.get_frame = &hdragcGetFrame;
    fi.*.set_cache_hints = &hdragcSetCacheHints;
    fi.*.free_filter = &hdragcFree;

    var v: c.AVS_Value = undefined;
    api.avs_set_to_clip.?(&v, clip);
    return v;
}

fn hdragcSetCacheHints(fi: [*c]c.AVS_FilterInfo, cachehints: c_int, frame_range: c_int) callconv(.c) c_int {
    _ = fi;
    _ = frame_range;
    // The temporal state (prev_gain/last_gain) is mutated in get_frame — a
    // serialized MT mode is required; parallel/out-of-order evaluation would
    // corrupt the gain ring buffer.
    return if (cachehints == c.AVS_CACHE_GET_MTMODE) c.AVS_MT_SERIALIZED else 0;
}

fn hdragcFree(fi: [*c]c.AVS_FilterInfo) callconv(.c) void {
    const d: *HdrAgcData = @ptrCast(@alignCast(fi.*.user_data));
    allocator.free(d.prev_gain);
    allocator.free(d.r);
    allocator.free(d.g);
    allocator.free(d.b);
    allocator.free(d.y_lum);
    allocator.free(d.y_local);
    allocator.free(d.circle_mat);
    allocator.free(d.sat_a);
    allocator.free(d.sat_b);
    allocator.free(d.sat_c);
    allocator.free(d.sat_d);
    allocator.free(d.sat_e);
    allocator.free(d.sat_f);
    allocator.destroy(d);
    fi.*.user_data = null;
}

fn hdragcGetFrame(fi: [*c]c.AVS_FilterInfo, n: c_int) callconv(.c) [*c]c.AVS_VideoFrame {
    const d: *HdrAgcData = @ptrCast(@alignCast(fi.*.user_data));

    const src = api.avs_get_frame.?(fi.*.child, n);
    if (src == null) return null;
    defer api.avs_release_video_frame.?(src);

    // prop_src=src carries over frame properties (V8+) — a small improvement
    // over the original, which lost properties.
    const dst = api.avs_new_video_frame_p_a.?(fi.*.env, &fi.*.vi, src, c.AVS_FRAME_ALIGN);
    if (dst == null) {
        fi.*.@"error" = "HDRAGC: could not allocate the destination frame.";
        return null;
    }

    const W = d.width;
    const H = d.height;
    const circle: i32 = d.circle;
    const csz: i32 = 2 * circle + 1;

    var src_row: [*]const u8 = @ptrCast(api.avs_get_read_ptr_p.?(src, c.AVS_PLANAR_Y));
    var dst_row: [*]u8 = @ptrCast(api.avs_get_write_ptr_p.?(dst, c.AVS_PLANAR_Y));
    const src_pitch: usize = @intCast(api.avs_get_pitch_p.?(src, c.AVS_PLANAR_Y));
    const dst_pitch: usize = @intCast(api.avs_get_pitch_p.?(dst, c.AVS_PLANAR_Y));

    // ------------------------------------------------------------------
    // Stage A: pixel decomposition + luma histogram. Luma is the plain
    // average (R+G+B)/3, deliberately not weighted luminance — the original
    // author's comment: "No true lumination (because of better results)".
    // ------------------------------------------------------------------
    var hist: [256]u32 = [_]u32{0} ** 256;
    for (0..H) |h| {
        for (0..W) |w| {
            const px = std.mem.readInt(u32, src_row[w * 4 ..][0..4], .little);
            const r: u8 = @intCast((px >> 16) & 0xFF);
            const g: u8 = @intCast((px >> 8) & 0xFF);
            const b: u8 = @intCast(px & 0xFF);
            const idx = h * W + w;
            d.r[idx] = r;
            d.g[idx] = g;
            d.b[idx] = b;
            // explicit u16 promotion: in C, u8+u8+u8 wraps silently; Zig
            // safety mode would panic on the overflow.
            const y: u8 = @intCast((@as(u16, r) + @as(u16, g) + @as(u16, b)) / 3);
            d.y_lum[idx] = y;
            hist[y] += 1;
        }
        src_row += src_pitch;
    }

    // ------------------------------------------------------------------
    // Stage B: edge-aware local luminance.
    // mode==0 : full 2D pass within the `circle` radius (masked by circleMat)
    // mode!=0 : two 1D passes (horizontal + vertical) averaged — the faster
    //           separable approximation.
    // ------------------------------------------------------------------
    if (d.mode == 0) {
        for (0..H) |h| {
            const hmin_i: i32 = @max(0, @as(i32, @intCast(h)) - circle);
            const hmax_i: i32 = @min(@as(i32, @intCast(H)) - 1, @as(i32, @intCast(h)) + circle);
            for (0..W) |w| {
                const wmin_i: i32 = @max(0, @as(i32, @intCast(w)) - circle);
                const wmax_i: i32 = @min(@as(i32, @intCast(W)) - 1, @as(i32, @intCast(w)) + circle);
                var acc: f32 = 0.0;
                var wsum: f32 = 0.0;
                const y_hw: u8 = d.y_lum[h * W + w];
                var y: usize = @intCast(hmin_i);
                while (y <= @as(usize, @intCast(hmax_i))) : (y += 1) {
                    var x: usize = @intCast(wmin_i);
                    while (x <= @as(usize, @intCast(wmax_i))) : (x += 1) {
                        const mat_x: i32 = @as(i32, @intCast(w)) - @as(i32, @intCast(x)) + circle;
                        const mat_y: i32 = @as(i32, @intCast(h)) - @as(i32, @intCast(y)) + circle;
                        if (d.circle_mat[@intCast(mat_x + csz * mat_y)] != 0) {
                            const y_xy: u8 = d.y_lum[y * W + x];
                            const weight = weights[y_xy][y_hw];
                            wsum += weight;
                            acc += weight * @as(f32, @floatFromInt(y_xy));
                        }
                    }
                }
                d.y_local[h * W + w] = acc / wsum;
            }
        }
    } else {
        for (0..H) |h| {
            const hmin_i: i32 = @max(0, @as(i32, @intCast(h)) - circle);
            const hmax_i: i32 = @min(@as(i32, @intCast(H)) - 1, @as(i32, @intCast(h)) + circle);
            for (0..W) |w| {
                const wmin_i: i32 = @max(0, @as(i32, @intCast(w)) - circle);
                const wmax_i: i32 = @min(@as(i32, @intCast(W)) - 1, @as(i32, @intCast(w)) + circle);
                const y_hw: u8 = d.y_lum[h * W + w];

                var acc: f32 = 0.0;
                var wsum: f32 = 0.0;
                var x: usize = @intCast(wmin_i);
                while (x <= @as(usize, @intCast(wmax_i))) : (x += 1) {
                    const y_xy: u8 = d.y_lum[h * W + x];
                    const weight = weights[y_xy][y_hw];
                    wsum += weight;
                    acc += weight * @as(f32, @floatFromInt(y_xy));
                }
                const local = acc / wsum;

                acc = 0.0;
                wsum = 0.0;
                var y: usize = @intCast(hmin_i);
                while (y <= @as(usize, @intCast(hmax_i))) : (y += 1) {
                    const y_xy: u8 = d.y_lum[y * W + w];
                    const weight = weights[y_xy][y_hw];
                    wsum += weight;
                    acc += weight * @as(f32, @floatFromInt(y_xy));
                }
                d.y_local[h * W + w] = (local + acc / wsum) / 2.0;
            }
        }
    }

    // ------------------------------------------------------------------
    // Stage C: global gain from the histogram, averaging bins 9..=192 only.
    // DEVIATION #4: an all-black frame gives sum==0 -> division by zero in C
    // (inf/NaN corrupts the whole frame). The port falls back to min_gain.
    // ------------------------------------------------------------------
    var sum_bins: u32 = 0;
    var val_bins: u32 = 0;
    for (9..193) |i| {
        sum_bins += hist[i];
        val_bins += @as(u32, @intCast(i)) * hist[i];
    }
    var curr_max_gain: f32 = undefined;
    if (sum_bins == 0) {
        curr_max_gain = d.min_gain;
    } else {
        const mean = @as(f32, @floatFromInt(val_bins)) / @as(f32, @floatFromInt(sum_bins));
        curr_max_gain = 128.0 * d.coef_gain / mean;
        if (curr_max_gain < d.min_gain) curr_max_gain = d.min_gain;
        if (curr_max_gain > d.max_gain) curr_max_gain = d.max_gain;
    }

    // ------------------------------------------------------------------
    // Stage D: temporal smoothing (ring buffer) + response limiter.
    // DEVIATION #5: the float abs() in C (VC6 int prototype, truncated) is
    // not replicated — the port uses the intended float absolute value.
    // ------------------------------------------------------------------
    if (d.last_gain == 0.0) {
        d.index = 0;
        d.last_gain = curr_max_gain;
    }
    d.prev_gain[d.index] = curr_max_gain;
    d.index = (d.index + 1) % @as(usize, @intCast(d.avg_window));

    var avg: f32 = 0.0;
    var avail: u32 = 0;
    for (d.prev_gain) |g| {
        if (g != 0.0) {
            avail += 1;
            avg += g;
        }
    }
    if (avail > 0) avg /= @as(f32, @floatFromInt(avail));

    const diff_pct = @abs(avg - d.last_gain) / d.last_gain * 100.0;
    if (diff_pct > @as(f32, @floatFromInt(d.response))) {
        if (avg < d.last_gain) {
            d.last_gain = d.last_gain * @as(f32, @floatFromInt(100 - d.response)) / 100.0;
        } else {
            d.last_gain = d.last_gain * @as(f32, @floatFromInt(100 + d.response)) / 100.0;
        }
    } else {
        d.last_gain = avg;
    }
    curr_max_gain = d.last_gain;

    // ------------------------------------------------------------------
    // Stage E: YLUT — histogram matching against the gauss table, with
    // highlight tapering above `limit` (204/gain), easing to gain 1.0 at 235.
    // DEVIATION #6: guard against gauss[256] out-of-bounds (in C the table
    // could be read past its end when float accumulation exceeds the total
    // pixel count).
    // ------------------------------------------------------------------
    var ylut: [256]f32 = undefined;
    {
        var acc: u32 = 0;
        var last_y: f32 = 0.0;
        var gauss_i: usize = 0;
        const limit: f32 = 204.0 / curr_max_gain;
        for (0..256) |i| {
            acc += hist[i];
            while (gauss_i < 255 and @as(f32, @floatFromInt(acc)) > d.gauss[gauss_i])
                gauss_i += 1;
            var new_lum: f32 = @floatFromInt(gauss_i);
            var max_lum: f32 = last_y;
            if (last_y >= 235.0) {
                max_lum += 1.0;
            } else if (@as(f32, @floatFromInt(i)) < limit) {
                max_lum += curr_max_gain;
            } else {
                const t = (@as(f32, @floatFromInt(i)) - limit) / (235.0 - limit)
                    * (last_y - limit) / (235.0 - limit);
                max_lum += curr_max_gain - @sqrt(t) * (curr_max_gain - 1.0);
            }
            if (new_lum > max_lum) new_lum = max_lum;
            if (new_lum < @as(f32, @floatFromInt(i))) new_lum = @floatFromInt(i);
            ylut[i] = new_lum;
            last_y = new_lum;
        }
    }

    // ------------------------------------------------------------------
    // Stage F: per-pixel application — gain from YLUT[local], proportional
    // saturation via the a..f matrix, clamp to 0..255.
    // Note: y_local==0 (all-black neighborhood) yields gain inf in C, which
    // the clamp to curr_max_gain then catches. The port handles it
    // explicitly without going through inf (identical result).
    // ------------------------------------------------------------------
    for (0..H) |h| {
        for (0..W) |w| {
            const idx = h * W + w;
            const yl = d.y_local[idx];
            const lut_idx: usize = @intCast(std.math.clamp(roundOrig(yl), 0, 255));

            var gain: f32 = undefined;
            if (yl <= 0.0) {
                gain = curr_max_gain;
            } else {
                gain = ylut[lut_idx] / yl;
            }
            if (!std.math.isFinite(gain)) gain = curr_max_gain;
            if (gain < 1.0) gain = 1.0;
            if (gain > curr_max_gain) gain = curr_max_gain;

            var f_sat = (gain - 1.0) * d.coef_sat + d.min_sat - 1.0;
            if (f_sat > d.max_sat - 1.0) f_sat = d.max_sat - 1.0;
            if (f_sat < d.min_sat - 1.0) f_sat = d.min_sat - 1.0;
            // defensive clamp (the original reads out of bounds if min_sat<1)
            if (f_sat < 0.0) f_sat = 0.0;
            const sat: usize = @min(@as(usize, @intFromFloat(f_sat * 100.0)), d.sat_a.len - 1);

            const red: f32 = @floatFromInt(d.r[idx]);
            const green: f32 = @floatFromInt(d.g[idx]);
            const blue: f32 = @floatFromInt(d.b[idx]);

            var red_n: i32 = @intFromFloat((d.sat_b[sat] * red + d.sat_c[sat] * green + d.sat_e[sat] * blue) * gain);
            var green_n: i32 = @intFromFloat((d.sat_a[sat] * red + d.sat_d[sat] * green + d.sat_e[sat] * blue) * gain);
            var blue_n: i32 = @intFromFloat((d.sat_a[sat] * red + d.sat_c[sat] * green + d.sat_f[sat] * blue) * gain);
            red_n = std.math.clamp(red_n, 0, 255);
            green_n = std.math.clamp(green_n, 0, 255);
            blue_n = std.math.clamp(blue_n, 0, 255);

            // DEVIATION #7 (preserved 1:1): output alpha byte = 0, exactly
            // like the original, which wrote a u32 without alpha bits.
            const out: u32 = (@as(u32, @intCast(red_n)) << 16)
                | (@as(u32, @intCast(green_n)) << 8)
                | @as(u32, @intCast(blue_n));
            std.mem.writeInt(u32, dst_row[w * 4 ..][0..4], out, .little);
        }
        dst_row += dst_pitch;
    }

    // DEVIATION #8: debug!=0 in the original draws the gain value as text
    // via GDI (Antialiaser from the AviSynth core). The port does not
    // implement a text overlay; the parameter is accepted but ignored.
    _ = d.verbose;

    return dst;
}

const REQUIRED_INTERFACE_VERSION: c_int = avs.INTERFACE_VERSION;
const REQUIRED_BUGFIX_VERSION: c_int = avs.INTERFACE_BUGFIX_VERSION;

const required_functions = [_][*:0]const u8{
    "avs_add_function",
    "avs_get_frame",
    "avs_get_height_p",
    "avs_get_pitch_p",
    "avs_get_read_ptr_p",
    "avs_get_row_size_p",
    "avs_get_write_ptr_p",
    "avs_new_c_filter",
    "avs_new_video_frame_p_a",
    "avs_release_clip",
    "avs_release_video_frame",
    "avs_set_to_clip",
};

export fn avisynth_c_plugin_init(env: ?*c.AVS_ScriptEnvironment) callconv(.c) [*:0]const u8 {
    const e = env orelse return "HDRAGC: plugin init called without a script environment.";

    api = avs.getApi(e, REQUIRED_INTERFACE_VERSION, REQUIRED_BUGFIX_VERSION, &required_functions) catch
        return avs.getLastError().ptr;

    _ = api.avs_add_function.?(
        e,
        "HDRAGC",
        "c[avg_lum]i[max_gain]f[min_gain]f[coef_gain]f[max_sat]f[min_sat]f[coef_sat]f[circle]i[avg_window]i[response]i[sigma]f[debug]i[mode]i",
        &hdragcCreate,
        null,
    );

    return "HDRAGC 0.1.5 plugin (Zig 1:1 port)";
}
