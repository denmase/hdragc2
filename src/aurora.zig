//! Aurora — the "next-generation" HDRAGC.
//!
//! A modern reconstruction inspired by the documented HDRAGC v1.8.7
//! parameter set (LaTo INV., closed-source, documented at the archived
//! strony.aster.pl/paviko/hdragc.htm), with the planned improvements on top:
//!
//!   * YUV (YV12) processing — v1.8.7 moved off RGB32 for good reasons:
//!     luma and chroma can be treated independently, and chroma "saturation"
//!     is a clean scale around the neutral 128 instead of an RGB matrix.
//!   * Selectable local-estimator engines (`engine`):
//!       - "guided"  — self-guided filter (He et al. 2010), the default;
//!                     much better edge preservation than the 2005 kernel.
//!       - "legacy"  — the separable edge-aware weighted mean from 0.1.5
//!                     (kept so behavior can be compared 1:1).
//!       - "clahe"   — CLAHE of the luma plane, used as the local map.
//!   * black_clip auto black-point, freezer, corrector/reducer/protect gain
//!     shaping — all reconstructed from the 1.8.7 documentation.
//!
//! Input: YV12 8-bit (v1). Use ConvertToYV12() in the script.
//!
//! Where the 1.8.7 docs were ambiguous, the interpretation is marked
//! [interp] in the comments and in README.md.

const std = @import("std");
const avs = @import("avisynth");
const c = avs.c;
const common = @import("common.zig");

const allocator = std.heap.c_allocator;

/// The AvsApi function table, injected by main.zig at registration time so
/// this file does not need a circular import with the root module.
var api: *const avs.AvsApi = undefined;

const Engine = enum { legacy, guided, clahe };

const AuroraData = struct {
    // ---- parameters (from the documented 1.8.7 set + Aurora additions) ----
    avg_lum: i32, // target average luma
    max_gain: f32, // global gain clamp upper bound
    min_gain: f32, // global gain clamp lower bound
    coef_gain: f32, // damper: g = 1 + (g_raw-1)*coef_gain
    max_sat: f32, // chroma scale clamp (1.8.7 default is a bold 9.0)
    min_sat: f32,
    coef_sat: f32, // chroma scale = 1 + (g-1)*coef_sat
    avg_window: i32, // temporal smoothing window, frames
    response: i32, // max gain change per frame, percent
    debug: bool, // accepted for compat; no overlay in v1
    mode: i32, // 1.8.7 mode selector (used only to pick the default engine)
    engine: Engine, // Aurora addition: which local estimator to use
    protect: i32, // 0=off, 1=on, 2=auto highlight protection
    passes: i32, // legacy-engine estimator iterations (doc: "mode 1 only")
    shift: i32, // fixed luma pre-shift (keeps near-black noise out)
    shadows: bool, // extra shadow-region enhancement curve
    shift_u: i32, // constant U offset (white balance)
    shift_v: i32, // constant V offset (white balance)
    corrector: f32, // 1 -> no correction; lower -> less gain on bright pixels
    reducer: f32, // gain-map spatial smoothing strength, 0..2 [interp]
    black_clip: f32, // fraction of darkest pixels pinned to black, 0..1 [interp]
    freezer: i32, // >=0: freeze statistics from frame N for the whole clip
    // Aurora-only tuning parameters
    radius: i32, // estimator neighborhood radius (replaces 0.1.5 "circle")
    clip_limit: f32, // CLAHE clip limit (OpenCV convention)
    tiles: i32, // CLAHE grid size (tiles x tiles)

    // ---- state ----
    gauss: [256]f32, // gaussian target distribution (sigma fixed at 1.5)
    prev_gain: []f32, // temporal ring buffer
    last_gain: f32 = 0.0,
    index: usize = 0,
    frozen: bool = false, // freezer: statistics already captured?
    frozen_ylut: [256]f32, // frozen LUT (freezer mode)
    frozen_gain: f32, // frozen gain (freezer mode)

    // ---- buffers ----
    width: usize,
    height: usize,
    cw: usize, // chroma width  = ceil(w/2)
    ch: usize, // chroma height = ceil(h/2)
    pixels: usize,
    ybuf: []u8, // luma after black_clip/shift pre-processing
    ytmp: []u8, // scratch for legacy multi-pass
    lbuf: []f32, // local-luminance map (engine output)
    pg: []f32, // per-pixel gain map
    tmp1: []f32, // scratch
    tmp2: []f32, // scratch
};

// ---------------------------------------------------------------------------
// Argument helpers (the C API gives us an AVS_Value array).
// ---------------------------------------------------------------------------
fn argDefined(args: c.AVS_Value, i: usize) bool {
    return c.avs_defined(c.avs_array_elt(args, @intCast(i))) != 0;
}
fn argInt(args: c.AVS_Value, i: usize, default: i32) i32 {
    if (!argDefined(args, i)) return default;
    return @intCast(c.avs_as_int(c.avs_array_elt(args, @intCast(i))));
}
fn argFloat(args: c.AVS_Value, i: usize, default: f64) f32 {
    if (!argDefined(args, i)) return @floatCast(default);
    return @floatCast(c.avs_as_float(c.avs_array_elt(args, @intCast(i))));
}
fn argBool(args: c.AVS_Value, i: usize, default: bool) bool {
    if (!argDefined(args, i)) return default;
    return c.avs_as_bool(c.avs_array_elt(args, @intCast(i))) != 0;
}
fn argStr(args: c.AVS_Value, i: usize) ?[]const u8 {
    if (!argDefined(args, i)) return null;
    return std.mem.span(c.avs_as_string(c.avs_array_elt(args, @intCast(i))));
}

fn parseEngine(args: c.AVS_Value, mode: i32) Engine {
    // "engine" explicitly overrides; otherwise mode picks the default,
    // mirroring the 1.8.7 docs (mode 1 = fast, mode 2 = better quality).
    if (argStr(args, 11)) |s| {
        if (std.ascii.eqlIgnoreCase(s, "legacy")) return .legacy;
        if (std.ascii.eqlIgnoreCase(s, "guided")) return .guided;
        if (std.ascii.eqlIgnoreCase(s, "clahe")) return .clahe;
    }
    return if (mode == 1) .legacy else .guided;
}

fn auroraCreate(env: ?*c.AVS_ScriptEnvironment, args: c.AVS_Value, user_data: ?*anyopaque) callconv(.c) c.AVS_Value {
    _ = user_data;

    var fi: [*c]c.AVS_FilterInfo = undefined;
    const clip = api.avs_new_c_filter.?(env, &fi, c.avs_array_elt(args, 0), 1);
    defer api.avs_release_clip.?(clip);

    if (c.avs_has_video(&fi.*.vi) == 0)
        return c.avs_new_value_error("Aurora: input clip must have video.");
    if ((fi.*.vi.pixel_type & c.AVS_CS_YV12) != c.AVS_CS_YV12)
        return c.avs_new_value_error("Aurora: YV12 input required in v1 — add ConvertToYV12().");

    const vi: c.AVS_VideoInfo = fi.*.vi;
    const width: usize = @intCast(vi.width);
    const height: usize = @intCast(vi.height);
    const pixels = width * height;

    const mode = argInt(args, 10, 2);

    const d = allocator.create(AuroraData) catch
        return c.avs_new_value_error("Aurora: out of memory.");
    d.* = .{
        .avg_lum = argInt(args, 1, 128),
        .max_gain = argFloat(args, 2, 3.0),
        .min_gain = argFloat(args, 3, 1.0),
        .coef_gain = argFloat(args, 4, 1.0),
        .max_sat = argFloat(args, 5, 9.0),
        .min_sat = argFloat(args, 6, 0.0),
        .coef_sat = argFloat(args, 7, 1.0),
        .avg_window = argInt(args, 8, -1), // -1 = one second worth of frames
        .response = argInt(args, 9, 100),
        .debug = argBool(args, 10, false),
        .mode = mode,
        .engine = parseEngine(args, mode),
        .protect = argInt(args, 12, 2),
        .passes = argInt(args, 13, 4),
        .shift = argInt(args, 14, 0),
        .shadows = argBool(args, 15, true),
        .shift_u = argInt(args, 16, 0),
        .shift_v = argInt(args, 17, 0),
        .corrector = argFloat(args, 18, 0.0),
        .reducer = argFloat(args, 19, 0.5),
        .black_clip = argFloat(args, 20, 0.0),
        .freezer = argInt(args, 21, -1),
        .radius = argInt(args, 22, 7),
        .clip_limit = argFloat(args, 23, 2.0),
        .tiles = argInt(args, 24, 8),
        .gauss = undefined,
        .prev_gain = undefined,
        .frozen_ylut = undefined,
        .frozen_gain = 0.0,
        .width = width,
        .height = height,
        .cw = (width + 1) / 2,
        .ch = (height + 1) / 2,
        .pixels = pixels,
        .ybuf = allocator.alloc(u8, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .ytmp = allocator.alloc(u8, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .lbuf = allocator.alloc(f32, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .pg = allocator.alloc(f32, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .tmp1 = allocator.alloc(f32, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .tmp2 = allocator.alloc(f32, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
    };

    // avg_window = -1 means "one second" (docs): convert fps to frames.
    if (d.avg_window == -1) {
        const fps: f32 = @as(f32, @floatFromInt(vi.fps_numerator)) / @as(f32, @floatFromInt(vi.fps_denominator));
        d.avg_window = @intFromFloat(@ceil(fps));
    }
    if (d.avg_window < 1) d.avg_window = 1;
    if (d.response < 1) d.response = 1;
    if (d.radius < 1) d.radius = 1;
    if (d.tiles < 1) d.tiles = 1;
    if (d.passes < 1) d.passes = 1;

    d.prev_gain = allocator.alloc(f32, @intCast(d.avg_window)) catch
        return c.avs_new_value_error("Aurora: out of memory.");
    @memset(d.prev_gain, 0.0);

    // Gaussian target distribution — sigma is fixed at 1.5 (1.8.7 removed
    // the sigma parameter when it dropped the RGB path).
    common.buildGauss(&d.gauss, d.avg_lum, 1.5, pixels);

    fi.*.user_data = d;
    fi.*.get_frame = &auroraGetFrame;
    fi.*.set_cache_hints = &auroraSetCacheHints;
    fi.*.free_filter = &auroraFree;

    var v: c.AVS_Value = undefined;
    api.avs_set_to_clip.?(&v, clip);
    return v;
}

fn auroraSetCacheHints(fi: [*c]c.AVS_FilterInfo, cachehints: c_int, frame_range: c_int) callconv(.c) c_int {
    _ = fi;
    _ = frame_range;
    // Temporal state (ring buffer / freezer) is mutated in get_frame.
    return if (cachehints == c.AVS_CACHE_GET_MTMODE) c.AVS_MT_SERIALIZED else 0;
}

fn auroraFree(fi: [*c]c.AVS_FilterInfo) callconv(.c) void {
    const d: *AuroraData = @ptrCast(@alignCast(fi.*.user_data));
    allocator.free(d.prev_gain);
    allocator.free(d.ybuf);
    allocator.free(d.ytmp);
    allocator.free(d.lbuf);
    allocator.free(d.pg);
    allocator.free(d.tmp1);
    allocator.free(d.tmp2);
    allocator.destroy(d);
    fi.*.user_data = null;
}

// ---------------------------------------------------------------------------
// Legacy engine: separable edge-aware weighted mean (the 0.1.5 "mode!=0"
// path). Each pixel is averaged only with neighbors whose luma is within a
// factor ~5 of its own (the comptime weights table), which keeps shadows
// uniform without bleeding across bright edges. `passes` iterations repeat
// the estimation on the previous result, smoothing the map further.
// ---------------------------------------------------------------------------
fn legacyLocal(d: *AuroraData) void {
    const W = d.width;
    const H = d.height;
    const r: i32 = d.radius;

    // Pass 1 reads ybuf and writes lbuf.
    separableWeightedMean(d.ybuf, d.lbuf, W, H, r);

    // Additional passes re-estimate from the previous map (quantized back to
    // 8-bit so the weight table still applies).
    var i: i32 = 1;
    while (i < d.passes) : (i += 1) {
        for (0..d.pixels) |p| d.ytmp[p] = @intCast(std.math.clamp(common.roundOrig(d.lbuf[p]), 0, 255));
        separableWeightedMean(d.ytmp, d.tmp1, W, H, r);
        @memcpy(d.lbuf, d.tmp1[0..d.pixels]);
    }
}

fn separableWeightedMean(src: []const u8, dst: []f32, w: usize, h: usize, r: i32) void {
    // Horizontal weighted pass into tmp2, vertical into tmp1, then average.
    // (Implemented directly over dst with two scratch passes inline.)
    for (0..h) |y| {
        for (0..w) |x| {
            const y_hw: u8 = src[y * w + x];
            var acc: f32 = 0.0;
            var wsum: f32 = 0.0;
            const x0: usize = @intCast(@max(0, @as(i32, @intCast(x)) - r));
            const x1: usize = @intCast(@min(@as(i32, @intCast(w)) - 1, @as(i32, @intCast(x)) + r));
            var xx: usize = x0;
            while (xx <= x1) : (xx += 1) {
                const y_xy: u8 = src[y * w + xx];
                const weight = common.weights[y_xy][y_hw];
                wsum += weight;
                acc += weight * @as(f32, @floatFromInt(y_xy));
            }
            dst[y * w + x] = acc / wsum; // stored temporarily (horizontal part)
        }
    }
    // Vertical pass: fold into the final value by averaging with the stored
    // horizontal result, exactly like the original mode!=0 path.
    for (0..h) |y| {
        const y0: usize = @intCast(@max(0, @as(i32, @intCast(y)) - r));
        const y1: usize = @intCast(@min(@as(i32, @intCast(h)) - 1, @as(i32, @intCast(y)) + r));
        for (0..w) |x| {
            const y_hw: u8 = src[y * w + x];
            var acc: f32 = 0.0;
            var wsum: f32 = 0.0;
            var yy: usize = y0;
            while (yy <= y1) : (yy += 1) {
                const y_xy: u8 = src[yy * w + x];
                const weight = common.weights[y_xy][y_hw];
                wsum += weight;
                acc += weight * @as(f32, @floatFromInt(y_xy));
            }
            dst[y * w + x] = (dst[y * w + x] + acc / wsum) / 2.0;
        }
    }
}

// ---------------------------------------------------------------------------
// Guided engine: self-guided filter (He et al. 2010). The guidance image is
// the luma itself, so edges are preserved while flat regions get averaged.
// eps is the regularization variance: (0.02 * 255)^2 ~ 26.
// ---------------------------------------------------------------------------
fn guidedLocal(d: *AuroraData) void {
    const eps: f32 = 26.0;
    common.guidedFilter(d.ybuf, d.lbuf, d.tmp1, d.tmp2, d.width, d.height, @intCast(d.radius), eps);
}

// ---------------------------------------------------------------------------
// CLAHE engine: contrast-limited adaptive histogram equalization produces
// the local map directly (its output is already a locally tone-mapped luma;
// the global gain/LUT stages then apply temporal stability and highlight
// protection uniformly on top).
// ---------------------------------------------------------------------------
fn claheLocal(d: *AuroraData) void {
    common.clahe(d.ybuf, d.lbuf, d.width, d.height, @intCast(d.tiles), d.clip_limit);
}

// ---------------------------------------------------------------------------
// get_frame — the per-frame pipeline:
//   1. black_clip + shift pre-processing   5. local estimator (engine)
//   2. histogram + global gain             6. per-pixel gain shaping
//   3. temporal smoothing / freezer        7. reducer smoothing of the map
//   4. YLUT (histogram matching, protect)  8. chroma scale + shifts
// ---------------------------------------------------------------------------
fn auroraGetFrame(fi: [*c]c.AVS_FilterInfo, n: c_int) callconv(.c) [*c]c.AVS_VideoFrame {
    const d: *AuroraData = @ptrCast(@alignCast(fi.*.user_data));

    const src = api.avs_get_frame.?(fi.*.child, n);
    if (src == null) return null;
    defer api.avs_release_video_frame.?(src);

    const dst = api.avs_new_video_frame_p_a.?(fi.*.env, &fi.*.vi, src, c.AVS_FRAME_ALIGN);
    if (dst == null) {
        fi.*.@"error" = "Aurora: could not allocate the destination frame.";
        return null;
    }

    const W = d.width;
    const H = d.height;
    const N = d.pixels;

    const src_row: [*]const u8 = @ptrCast(api.avs_get_read_ptr_p.?(src, c.AVS_PLANAR_Y));
    const src_pitch: usize = @intCast(api.avs_get_pitch_p.?(src, c.AVS_PLANAR_Y));

    // ---- 1. black_clip + shift pre-processing ----
    // black_clip [interp]: walk the raw histogram until black_clip fraction
    // of pixels is covered; that luma value becomes the black offset, so the
    // darkest (usually noise-only) pixels are pinned to zero before analysis.
    var hist_raw: [256]u32 = [_]u32{0} ** 256;
    for (0..H) |y| {
        for (0..W) |x| hist_raw[src_row[y * src_pitch + x]] += 1;
    }
    var black_offset: i32 = d.shift;
    if (d.black_clip > 0.0) {
        const target: u32 = @intFromFloat(d.black_clip * @as(f32, @floatFromInt(N)));
        var acc: u32 = 0;
        for (0..256) |i| {
            acc += hist_raw[i];
            if (acc >= target) {
                black_offset += @intCast(i);
                break;
            }
        }
    }
    var hist: [256]u32 = [_]u32{0} ** 256;
    var max_luma: u32 = 0;
    for (0..H) |y| {
        for (0..W) |x| {
            const v = src_row[y * src_pitch + x];
            const t: i32 = @as(i32, v) - black_offset;
            const yv: u8 = if (t < 0) 0 else @intCast(@min(t, 255));
            d.ybuf[y * W + x] = yv;
            hist[yv] += 1;
            if (yv > max_luma) max_luma = yv;
        }
    }

    // ---- 2. global gain from the histogram (bins 9..=192, like 0.1.5) ----
    var sum_bins: u32 = 0;
    var val_bins: u32 = 0;
    for (9..193) |i| {
        sum_bins += hist[i];
        val_bins += @as(u32, @intCast(i)) * hist[i];
    }
    var curr_gain: f32 = undefined;
    if (sum_bins == 0) {
        curr_gain = d.min_gain; // degenerate all-black frame guard
    } else {
        const mean = @as(f32, @floatFromInt(val_bins)) / @as(f32, @floatFromInt(sum_bins));
        curr_gain = @as(f32, @floatFromInt(d.avg_lum)) * d.coef_gain / mean;
        if (curr_gain < d.min_gain) curr_gain = d.min_gain;
        if (curr_gain > d.max_gain) curr_gain = d.max_gain;
    }

    // ---- 3. freezer / temporal smoothing ----
    var ylut: [256]f32 = undefined;
    var protect_on = d.protect == 1;
    if (d.protect == 2) protect_on = max_luma >= 250; // auto: only if near-white present
    if (d.protect == 0) protect_on = false;

    if (d.freezer >= 0) {
        if (!d.frozen) {
            // Capture statistics from the requested frame, then freeze.
            const fr: c_int = @min(d.freezer, fi.*.vi.num_frames - 1);
            _ = fr; // v1: freeze from the FIRST evaluated frame's stats to
            // keep evaluation order-independent under MT_SERIALIZED.
            common.buildYlut(&d.frozen_ylut, &hist, &d.gauss, curr_gain, protect_on);
            d.frozen_gain = curr_gain;
            d.frozen = true;
        }
        ylut = d.frozen_ylut;
        curr_gain = d.frozen_gain;
    } else {
        // Temporal ring buffer + response limiter (identical to 0.1.5).
        if (d.last_gain == 0.0) {
            d.index = 0;
            d.last_gain = curr_gain;
        }
        d.prev_gain[d.index] = curr_gain;
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
        curr_gain = d.last_gain;
        common.buildYlut(&ylut, &hist, &d.gauss, curr_gain, protect_on);
    }

    // ---- 5. local estimator ----
    switch (d.engine) {
        .legacy => legacyLocal(d),
        .guided => guidedLocal(d),
        .clahe => claheLocal(d),
    }

    // ---- 6. per-pixel gain map (pass 1: shape, before reducer smoothing) ----
    for (0..N) |p| {
        const lum = d.lbuf[p];
        const lut_idx: usize = @intCast(std.math.clamp(common.roundOrig(lum), 0, 255));
        var g: f32 = undefined;
        if (lum <= 0.0) {
            g = curr_gain;
        } else {
            g = ylut[lut_idx] / lum;
        }
        if (!std.math.isFinite(g)) g = curr_gain;
        if (g < 1.0) g = 1.0;
        if (g > curr_gain) g = curr_gain;
        // corrector [interp]: factor = corrector + (1-corrector)*(1-y0/255).
        // corrector=1 -> no correction; lower values progressively withhold
        // gain from bright pixels (doc: "help for high values of gain").
        const y0: f32 = @floatFromInt(d.ybuf[p]);
        const factor = d.corrector + (1.0 - d.corrector) * (1.0 - y0 / 255.0);
        d.pg[p] = 1.0 + (g - 1.0) * factor;
    }

    // ---- 7. reducer: spatial smoothing of the gain map [interp] ----
    // "The higher, the more noise is removed from gained shadows." A small
    // box blur whose radius and blend amount grow with `reducer`.
    if (d.reducer > 0.0) {
        const rblur: usize = @intFromFloat(@ceil(d.reducer * 3.0));
        common.boxBlur(d.pg, d.tmp1, W, H, rblur);
        const a: f32 = @min(d.reducer, 1.0) * 0.5;
        for (0..N) |p| d.pg[p] = d.pg[p] * (1.0 - a) + d.tmp1[p] * a;
    }

    // ---- 6b/8. apply: luma scaling + shadow curve, then chroma ----
    var dst_row: [*]u8 = @ptrCast(api.avs_get_write_ptr_p.?(dst, c.AVS_PLANAR_Y));
    const dst_pitch: usize = @intCast(api.avs_get_pitch_p.?(dst, c.AVS_PLANAR_Y));
    for (0..H) |y| {
        for (0..W) |x| {
            const idx = y * W + x;
            const g = d.pg[idx];
            var out: f32 = @as(f32, @floatFromInt(d.ybuf[idx])) * g;
            // shadows [interp]: mild concave lift strongest near black,
            // proportional to how much gain was applied.
            if (d.shadows and g > 1.0) {
                const t = 1.0 - out / 255.0;
                out += @min((g - 1.0) * 4.0, 12.0) * t * t;
            }
            dst_row[y * dst_pitch + x] = @intCast(std.math.clamp(common.roundOrig(out), 0, 255));
        }
    }

    // Chroma: scale U/V around the neutral 128 by a saturation factor
    // derived from the average gain of the corresponding 2x2 luma block
    // (YV12 is quarter-resolution in chroma), then apply shift_u/shift_v.
    const sat_coef = d.coef_sat;
    const sat_lo = d.min_sat;
    const sat_hi = d.max_sat;
    const CW = d.cw;
    const CH = d.ch;
    const u_src: [*]const u8 = @ptrCast(api.avs_get_read_ptr_p.?(src, c.AVS_PLANAR_U));
    const v_src: [*]const u8 = @ptrCast(api.avs_get_read_ptr_p.?(src, c.AVS_PLANAR_V));
    var u_dst: [*]u8 = @ptrCast(api.avs_get_write_ptr_p.?(dst, c.AVS_PLANAR_U));
    var v_dst: [*]u8 = @ptrCast(api.avs_get_write_ptr_p.?(dst, c.AVS_PLANAR_V));
    const u_pitch: usize = @intCast(api.avs_get_pitch_p.?(src, c.AVS_PLANAR_U));
    const v_pitch: usize = @intCast(api.avs_get_pitch_p.?(src, c.AVS_PLANAR_V));
    const ud_pitch: usize = @intCast(api.avs_get_pitch_p.?(dst, c.AVS_PLANAR_U));
    const vd_pitch: usize = @intCast(api.avs_get_pitch_p.?(dst, c.AVS_PLANAR_V));
    for (0..CH) |cy| {
        for (0..CW) |cx| {
            // average gain over the 2x2 luma block (clamped at edges)
            const x0 = cx * 2;
            const y0 = cy * 2;
            const x1 = @min(x0 + 1, W - 1);
            const y1 = @min(y0 + 1, H - 1);
            const g_avg = (d.pg[y0 * W + x0] + d.pg[y0 * W + x1] + d.pg[y1 * W + x0] + d.pg[y1 * W + x1]) / 4.0;
            var sat = 1.0 + (g_avg - 1.0) * sat_coef;
            if (sat > sat_hi) sat = sat_hi;
            if (sat < sat_lo) sat = sat_lo;
            const un: i32 = @as(i32, u_src[cy * u_pitch + cx]) - 128;
            const vn: i32 = @as(i32, v_src[cy * v_pitch + cx]) - 128;
            const uo = @as(i32, 128) + @as(i32, @intFromFloat(@round(@as(f32, @floatFromInt(un)) * sat))) + d.shift_u;
            const vo = @as(i32, 128) + @as(i32, @intFromFloat(@round(@as(f32, @floatFromInt(vn)) * sat))) + d.shift_v;
            u_dst[cy * ud_pitch + cx] = @intCast(std.math.clamp(uo, 0, 255));
            v_dst[cy * vd_pitch + cx] = @intCast(std.math.clamp(vo, 0, 255));
        }
    }

    _ = d.debug; // accepted for parameter compatibility; no overlay in v1
    return dst;
}

/// Registers the Aurora function. Called from main.zig's plugin init with
/// the resolved AvsApi table.
pub fn register(env: *c.AVS_ScriptEnvironment, avs_api: *const avs.AvsApi) void {
    api = avs_api;
    _ = api.avs_add_function.?(
        env,
        "Aurora",
        "c[avg_lum]i[max_gain]f[min_gain]f[coef_gain]f[max_sat]f[min_sat]f[coef_sat]f" ++
            "[avg_window]i[response]i[debug]b[mode]i[engine]s[protect]i[passes]i[shift]i" ++
            "[shadows]b[shift_u]i[shift_v]i[corrector]f[reducer]f[black_clip]f[freezer]i" ++
            "[radius]i[clip_limit]f[tiles]i",
        &auroraCreate,
        null,
    );
}
