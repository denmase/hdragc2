//! Aurora — the "next-generation" HDRAGC.
//!
//! A modern reconstruction inspired by the documented HDRAGC v1.8.7
//! parameter set (LaTo INV., closed-source, documented at the archived
//! strony.aster.pl/paviko/hdragc.htm), with planned improvements:
//!
//!   * YUV processing (YV12 and YUV444P) — v1.8.7 moved off RGB32; luma and
//!     chroma are handled independently, chroma "saturation" being a clean
//!     scale around neutral 128.
//!   * Selectable local estimators (`engine`): "guided" (self-guided filter,
//!     default), "legacy" (0.1.5 separable kernel), "clahe".
//!   * `domain`: run the statistics/gain stages in "gamma" (legacy),
//!     "linear" (sRGB EOTF), or "log" space. Linear/log make the multiplicative
//!     gain behave like a pure exposure shift and preserve midtone contrast.
//!   * Temporal stability: global-gain ring buffer (1.8.7 avg_window/response)
//!     PLUS an IIR-smoothed per-pixel gain map (`pg_smooth`), with automatic
//!     scene-cut detection (`scene_cut`) that resets all temporal state so a
//!     cut never inherits the previous scene's lift.
//!   * black_clip auto black-point, freezer, corrector/reducer/protect gain
//!     shaping — reconstructed from the 1.8.7 documentation.
//!
//! Where the 1.8.7 docs were ambiguous, the interpretation is marked [interp].

const std = @import("std");
const avs = @import("avisynth");
const c = avs.c;
const common = @import("common.zig");

const allocator = std.heap.c_allocator;

/// The AvsApi function table, injected by main.zig at registration time.
var api: *const avs.AvsApi = undefined;

const Engine = enum { legacy, guided, clahe };

const AuroraData = struct {
    // ---- parameters (documented 1.8.7 set + Aurora additions) ----
    avg_lum: i32,
    max_gain: f32,
    min_gain: f32,
    coef_gain: f32,
    max_sat: f32,
    min_sat: f32,
    coef_sat: f32,
    avg_window: i32, // temporal window for the GLOBAL gain, frames
    response: i32, // per-frame global-gain change limiter, %
    debug: bool,
    mode: i32,
    engine: Engine,
    protect: i32, // 0=off, 1=on, 2=auto
    passes: i32, // legacy-engine iterations
    shift: i32, // fixed luma pre-shift
    shadows: bool,
    shift_u: i32,
    shift_v: i32,
    corrector: f32, // 1 = no shaping; lower = less gain on bright pixels
    reducer: f32, // gain-map spatial smoothing, 0..2 [interp]
    black_clip: f32, // fraction of darkest pixels pinned to black [interp]
    freezer: i32, // >=0: freeze statistics from the first evaluated frame
    radius: i32,
    clip_limit: f32,
    tiles: i32,
    // Aurora temporal/domain additions
    pg_smooth: f32, // IIR blend of the per-pixel gain map across frames, 0..1
    scene_cut: f32, // histogram-difference threshold for a scene cut, 0=off
    domain: common.Domain, // working domain for stats/gain

    // ---- precomputed domain constants ----
    avg_work: i32, // avg_lum transformed into the working domain (rounded)
    bin_lo: i32, // histogram mean range, transformed ("9")
    bin_hi: i32, // ("193")
    lum_white: f32, // fwd(250): near-white threshold for auto protect
    lum_hi: f32, // fwd(235): taper top
    lum_204: f32, // fwd(204): taper threshold base
    is_yv12: bool, // chroma layout: 2x2 average vs full-res

    // ---- state ----
    last_n: i64 = -1, // last evaluated frame; -1 = none yet
    gauss: [256]f32,
    prev_gain: []f32, // global-gain ring buffer
    last_gain: f32 = 0.0,
    index: usize = 0,
    frozen: bool = false,
    frozen_ylut: [256]f32,
    frozen_gain: f32,
    hist_prev: [256]u32, // previous frame histogram (scene-cut detection)
    hist_prev_valid: bool = false,
    pg_prev: []f32, // previous frame gain map (temporal IIR)
    pg_prev_valid: bool = false,

    // ---- buffers ----
    width: usize,
    height: usize,
    cw: usize,
    ch: usize,
    pixels: usize,
    ybuf: []u8, // luma after black_clip/shift (always gamma-encoded)
    est_in: []u8, // estimator input: ybuf (gamma) or quantized work domain
    wbuf: []f32, // working-domain luma (identity in gamma mode)
    ytmp: []u8, // scratch for legacy multi-pass
    lbuf: []f32, // local-luminance map
    pg: []f32, // per-pixel gain map
    tmp1: []f32,
    tmp2: []f32,
};

fn argDefined(args: c.AVS_Value, i: usize) bool {
    // Bounds-check against the ACTUAL argument count first: reading an
    // AVS_Value array beyond its size is undefined behavior (it crashed
    // non-deterministically when optional trailing args were omitted).
    if (@as(usize, @intCast(c.avs_array_size(args))) <= i) return false;
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
    // NOTE: avs_array_size() returns the DECLARED parameter count of the
    // signature (padded with placeholders), not the passed-arg count, so a
    // bounds check alone is not enough — always verify the value type.
    if (!argDefined(args, i)) return null;
    const v = c.avs_array_elt(args, @intCast(i));
    if (c.avs_is_string(v) == 0) return null;
    const p = c.avs_as_string(v);
    if (p == null) return null;
    return std.mem.span(p);
}

fn parseEngine(args: c.AVS_Value, mode: i32) Engine {
    if (argStr(args, 12)) |s| {
        if (std.ascii.eqlIgnoreCase(s, "legacy")) return .legacy;
        if (std.ascii.eqlIgnoreCase(s, "guided")) return .guided;
        if (std.ascii.eqlIgnoreCase(s, "clahe")) return .clahe;
    }
    return if (mode == 1) .legacy else .guided;
}

fn parseDomain(args: c.AVS_Value) common.Domain {
    if (argStr(args, 28)) |s| {
        if (std.ascii.eqlIgnoreCase(s, "linear")) return .linear;
        if (std.ascii.eqlIgnoreCase(s, "log")) return .log;
    }
    return .gamma;
}

fn auroraCreate(env: ?*c.AVS_ScriptEnvironment, args: c.AVS_Value, user_data: ?*anyopaque) callconv(.c) c.AVS_Value {
    _ = user_data;

    var fi: [*c]c.AVS_FilterInfo = undefined;
    const clip = api.avs_new_c_filter.?(env, &fi, c.avs_array_elt(args, 0), 1);
    defer api.avs_release_clip.?(clip);

    if (c.avs_has_video(&fi.*.vi) == 0)
        return c.avs_new_value_error("Aurora: input clip must have video.");
    const pt = fi.*.vi.pixel_type;
    const is_yv12 = (pt & c.AVS_CS_YV12) == c.AVS_CS_YV12;
    // The C API uses the classic AviSynth name: YV24 == 8-bit planar YUV 4:4:4
    // (what the script side calls YUV444P).
    const is_444 = (pt & c.AVS_CS_YV24) == c.AVS_CS_YV24;
    if (!is_yv12 and !is_444)
        return c.avs_new_value_error("Aurora: YV12 or YUV444P input required — use ConvertToYV12()/ConvertToYUV444().");

    const vi: c.AVS_VideoInfo = fi.*.vi;
    const width: usize = @intCast(vi.width);
    const height: usize = @intCast(vi.height);
    const pixels = width * height;
    const mode = argInt(args, 11, 2);
    const domain = parseDomain(args);

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
        .avg_window = argInt(args, 8, -1),
        .response = argInt(args, 9, 100),
        .debug = argBool(args, 10, false),
        .mode = mode,
        .engine = parseEngine(args, mode),
        .protect = argInt(args, 13, 2),
        .passes = argInt(args, 14, 4),
        .shift = argInt(args, 15, 0),
        .shadows = argBool(args, 16, true),
        .shift_u = argInt(args, 17, 0),
        .shift_v = argInt(args, 18, 0),
        .corrector = argFloat(args, 19, 0.0),
        .reducer = argFloat(args, 20, 0.5),
        .black_clip = argFloat(args, 21, 0.0),
        .freezer = argInt(args, 22, -1),
        .radius = argInt(args, 23, 7),
        .clip_limit = argFloat(args, 24, 2.0),
        .tiles = argInt(args, 25, 8),
        .pg_smooth = argFloat(args, 26, 0.0),
        .scene_cut = argFloat(args, 27, 0.0),
        .domain = domain,
        .avg_work = undefined,
        .bin_lo = undefined,
        .bin_hi = undefined,
        .lum_white = common.fwd(domain, 250.0),
        .lum_hi = common.fwd(domain, 235.0),
        .lum_204 = common.fwd(domain, 204.0),
        .is_yv12 = is_yv12,
        .gauss = undefined,
        .prev_gain = undefined,
        .frozen_ylut = undefined,
        .frozen_gain = 0.0,
        .hist_prev = undefined,
        .hist_prev_valid = false,
        .pg_prev = undefined,
        .pg_prev_valid = false,
        .width = width,
        .height = height,
        .cw = if (is_yv12) (width + 1) / 2 else width,
        .ch = if (is_yv12) (height + 1) / 2 else height,
        .pixels = pixels,
        .ybuf = allocator.alloc(u8, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .est_in = allocator.alloc(u8, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .wbuf = allocator.alloc(f32, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .ytmp = allocator.alloc(u8, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .lbuf = allocator.alloc(f32, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .pg = allocator.alloc(f32, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .tmp1 = allocator.alloc(f32, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
        .tmp2 = allocator.alloc(f32, pixels) catch return c.avs_new_value_error("Aurora: out of memory."),
    };

    if (d.avg_window == -1) {
        const fps: f32 = @as(f32, @floatFromInt(vi.fps_numerator)) / @as(f32, @floatFromInt(vi.fps_denominator));
        d.avg_window = @intFromFloat(@ceil(fps));
    }
    if (d.avg_window < 1) d.avg_window = 1;
    if (d.response < 1) d.response = 1;
    if (d.radius < 1) d.radius = 1;
    if (d.tiles < 1) d.tiles = 1;
    if (d.passes < 1) d.passes = 1;
    if (d.pg_smooth < 0.0) d.pg_smooth = 0.0;
    if (d.pg_smooth > 0.95) d.pg_smooth = 0.95;

    // Domain precomputations: transform the gamma-domain reference points so
    // parameter MEANING stays stable across domains (avg_lum=128 in gamma
    // equals fwd(128) as the target in the working domain).
    d.avg_work = common.roundOrig(common.fwd(domain, @floatFromInt(d.avg_lum)));
    d.bin_lo = common.roundOrig(common.fwd(domain, 9.0));
    d.bin_hi = common.roundOrig(common.fwd(domain, 193.0));
    if (d.bin_hi > 255) d.bin_hi = 255;

    d.prev_gain = allocator.alloc(f32, @intCast(d.avg_window)) catch
        return c.avs_new_value_error("Aurora: out of memory.");
    @memset(d.prev_gain, 0.0);
    d.pg_prev = allocator.alloc(f32, pixels) catch
        return c.avs_new_value_error("Aurora: out of memory.");

    // Gaussian target in the working domain (sigma fixed at 1.5, as 1.8.7).
    common.buildGauss(&d.gauss, d.avg_work, 1.5, pixels);

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
    // Temporal state (global ring buffer, pg IIR, scene-cut history) is
    // mutated in get_frame, so frames must be evaluated in order.
    return if (cachehints == c.AVS_CACHE_GET_MTMODE) c.AVS_MT_SERIALIZED else 0;
}

fn auroraFree(fi: [*c]c.AVS_FilterInfo) callconv(.c) void {
    const d: *AuroraData = @ptrCast(@alignCast(fi.*.user_data));
    std.debug.print("[afree] w={d} h={d}\n", .{d.width, d.height});
    allocator.free(d.prev_gain);
    allocator.free(d.pg_prev);
    allocator.free(d.ybuf);
    allocator.free(d.est_in);
    allocator.free(d.wbuf);
    allocator.free(d.ytmp);
    allocator.free(d.lbuf);
    allocator.free(d.pg);
    allocator.free(d.tmp1);
    allocator.free(d.tmp2);
    allocator.destroy(d);
    fi.*.user_data = null;
}

// ---------------------------------------------------------------------------
// Legacy engine (0.1.5 separable edge-aware weighted mean), with passes.
// ---------------------------------------------------------------------------
fn legacyLocal(d: *AuroraData) void {
    const W = d.width;
    const H = d.height;
    const r: i32 = d.radius;
    separableWeightedMean(d.est_in, d.lbuf, W, H, r);
    var i: i32 = 1;
    while (i < d.passes) : (i += 1) {
        for (0..d.pixels) |p| d.ytmp[p] = @intCast(std.math.clamp(common.roundOrig(d.lbuf[p]), 0, 255));
        separableWeightedMean(d.ytmp, d.tmp1, W, H, r);
        @memcpy(d.lbuf, d.tmp1[0..d.pixels]);
    }
}

fn separableWeightedMean(src: []const u8, dst: []f32, w: usize, h: usize, r: i32) void {
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
            dst[y * w + x] = acc / wsum;
        }
    }
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

fn guidedLocal(d: *AuroraData) void {
    const eps: f32 = 26.0; // (0.02 * 255)^2 — gentle edge preservation
    common.guidedFilter(d.est_in, d.lbuf, d.tmp1, d.tmp2, d.width, d.height, @intCast(d.radius), eps);
}

fn claheLocal(d: *AuroraData) void {
    common.clahe(d.est_in, d.lbuf, d.width, d.height, @intCast(d.tiles), d.clip_limit);
}

// ---------------------------------------------------------------------------
// get_frame pipeline:
//   1. black_clip + shift pre-processing      6. gain map + corrector
//   2. domain transform (gamma/linear/log)    7. reducer (spatial smooth)
//   3. histogram + global gain                8. pg temporal IIR + scene cut
//   4. freezer / global temporal smoothing    9. apply (inverse domain)
//   5. local estimator (engine)              10. chroma scale + shifts
// ---------------------------------------------------------------------------
fn auroraGetFrame(fi: [*c]c.AVS_FilterInfo, n: c_int) callconv(.c) [*c]c.AVS_VideoFrame {
    const d: *AuroraData = @ptrCast(@alignCast(fi.*.user_data));

    // Seek/preview determinism: temporal state (ring buffer, pg IIR,
    // scene-cut history) makes output depend on evaluation history.
    // On NON-sequential access (seek, resume, scrub) reset it so the
    // result is identical no matter how the host reached this frame.
    const nn: i64 = n;
    if (d.last_n >= 0 and nn != d.last_n + 1) {
        @memset(d.prev_gain, 0.0);
        d.last_gain = 0.0;
        d.index = 0;
        d.pg_prev_valid = false;
        d.hist_prev_valid = false;
    }
    d.last_n = nn;

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
    const domain = d.domain;

    const src_row: [*]const u8 = @ptrCast(api.avs_get_read_ptr_p.?(src, c.AVS_PLANAR_Y));
    const src_pitch: usize = @intCast(api.avs_get_pitch_p.?(src, c.AVS_PLANAR_Y));

    // ---- 1. black_clip (percentile) + shift ----
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
    for (0..H) |y| {
        for (0..W) |x| {
            const v = src_row[y * src_pitch + x];
            const t: i32 = @as(i32, v) - black_offset;
            d.ybuf[y * W + x] = if (t < 0) 0 else @intCast(@min(t, 255));
        }
    }

    // ---- 2. domain transform ----
    // est_in (u8) feeds histogram + estimators; wbuf (f32) holds the exact
    // working-domain values used for the final scaling. In gamma mode both
    // are just the input luma (identity).
    if (domain == .gamma) {
        @memcpy(d.est_in, d.ybuf);
        for (0..N) |p| d.wbuf[p] = @floatFromInt(d.ybuf[p]);
    } else {
        for (0..N) |p| {
            const wv = common.fwd(domain, @floatFromInt(d.ybuf[p]));
            d.wbuf[p] = wv;
            d.est_in[p] = @intCast(std.math.clamp(common.roundOrig(wv), 0, 255));
        }
    }

    var hist: [256]u32 = [_]u32{0} ** 256;
    var max_work: u32 = 0;
    for (0..N) |p| {
        hist[d.est_in[p]] += 1;
    }
    for (0..256) |i| {
        if (hist[i] > 0) max_work = @intCast(i);
    }

    // ---- 3. global gain (mean over the transformed 9..193 bin range) ----
    var sum_bins: u32 = 0;
    var val_bins: u32 = 0;
    for (@intCast(d.bin_lo)..@intCast(d.bin_hi + 1)) |i| {
        sum_bins += hist[i];
        val_bins += @as(u32, @intCast(i)) * hist[i];
    }
    // A degenerate frame (e.g. a black decoder warm-up frame with no pixels
    // in the analysis bins) must NOT pollute the temporal ring buffer with
    // min_gain: it would drag the running average down and produce the
    // dark->bright->settle pumping. Reuse the previous gain instead, and
    // skip the ring update entirely.
    var degenerate = false;
    var curr_gain: f32 = undefined;
    if (sum_bins == 0) {
        degenerate = true;
        curr_gain = if (d.last_gain > 0.0) d.last_gain else d.min_gain;
    } else {
        const mean = @as(f32, @floatFromInt(val_bins)) / @as(f32, @floatFromInt(sum_bins));
        curr_gain = @as(f32, @floatFromInt(d.avg_work)) * d.coef_gain / mean;
        if (curr_gain < d.min_gain) curr_gain = d.min_gain;
        if (curr_gain > d.max_gain) curr_gain = d.max_gain;
    }
    // ---- 4. scene-cut detection (before any temporal update) ----
    // Metric: normalized histogram L1 distance, dist in 0..1. On a cut, ALL
    // temporal state is reset so the new scene starts fresh.
    if (d.scene_cut > 0.0 and d.hist_prev_valid) {
        var diff: u64 = 0;
        for (0..256) |i| {
            diff += if (hist[i] > d.hist_prev[i]) hist[i] - d.hist_prev[i] else d.hist_prev[i] - hist[i];
        }
        const dist = @as(f32, @floatFromInt(diff)) / (2.0 * @as(f32, @floatFromInt(N)));
        if (dist > d.scene_cut) {
            @memset(d.prev_gain, 0.0);
            d.last_gain = 0.0;
            d.index = 0;
            d.pg_prev_valid = false; // gain map adopts the new scene immediately
        }
    }
    d.hist_prev = hist;
    d.hist_prev_valid = true;

    // ---- 5. freezer / global temporal smoothing ----
    var ylut: [256]f32 = undefined;
    const protect_on = switch (d.protect) {
        1 => true,
        2 => @as(f32, @floatFromInt(max_work)) >= d.lum_white,
        else => false,
    };
    if (d.freezer >= 0) {
        if (!d.frozen) {
            common.buildYlut(&d.frozen_ylut, &hist, &d.gauss, curr_gain, protect_on, d.lum_hi, common.fwd(d.domain, 204.0 / curr_gain));
            d.frozen_gain = curr_gain;
            d.frozen = true;
        }
        ylut = d.frozen_ylut;
        curr_gain = d.frozen_gain;
    } else {
        if (d.last_gain == 0.0) {
            d.index = 0;
            d.last_gain = curr_gain;
        }
        if (degenerate) {
            // keep last_gain; do not touch the ring buffer
        } else {
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
        }
        curr_gain = d.last_gain;
        common.buildYlut(&ylut, &hist, &d.gauss, curr_gain, protect_on, d.lum_hi, common.fwd(d.domain, 204.0 / curr_gain));
    }

    // ---- 6-7. local estimator + gain map + corrector + reducer ----
    switch (d.engine) {
        .legacy => legacyLocal(d),
        .guided => guidedLocal(d),
        .clahe => claheLocal(d),
    }

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
        const y0 = d.wbuf[p];
        const factor = d.corrector + (1.0 - d.corrector) * (1.0 - y0 / 255.0);
        d.pg[p] = 1.0 + (g - 1.0) * factor;
    }

    if (d.reducer > 0.0) {
        const rblur: usize = @intFromFloat(@ceil(d.reducer * 3.0));
        common.boxBlur(d.pg, d.tmp1, W, H, rblur);
        const a: f32 = @min(d.reducer, 1.0) * 0.5;
        for (0..N) |p| d.pg[p] = d.pg[p] * (1.0 - a) + d.tmp1[p] * a;
    }

    // ---- 8. temporal IIR on the gain map ----
    // pg_smooth=0 -> off. Higher values increasingly reuse the previous
    // frame's map, which removes spatial pumping but slightly lags motion;
    // scene-cut detection above prevents cross-scene smearing.
    if (d.pg_smooth > 0.0) {
        if (d.pg_prev_valid) {
            for (0..N) |p| d.pg[p] = d.pg[p] * (1.0 - d.pg_smooth) + d.pg_prev[p] * d.pg_smooth;
        }
        @memcpy(d.pg_prev, d.pg);
        d.pg_prev_valid = true;
    }

    // ---- 9. apply: scale in the working domain, then inverse transform ----
    const dst_row: [*]u8 = @ptrCast(api.avs_get_write_ptr_p.?(dst, c.AVS_PLANAR_Y));
    const dst_pitch: usize = @intCast(api.avs_get_pitch_p.?(dst, c.AVS_PLANAR_Y));
    for (0..H) |y| {
        for (0..W) |x| {
            const idx = y * W + x;
            const g = d.pg[idx];
            var out: f32 = d.wbuf[idx] * g;
            if (d.shadows and g > 1.0) {
                const t = 1.0 - out / 255.0;
                out += @min((g - 1.0) * 4.0, 12.0) * t * t;
            }
            if (domain != .gamma) out = common.inv(domain, out);
            dst_row[y * dst_pitch + x] = @intCast(std.math.clamp(common.roundOrig(out), 0, 255));
        }
    }

    // ---- 10. chroma ----
    // YV12: average gain over each 2x2 luma block. YUV444P: 1:1 mapping.
    const CW = d.cw;
    const CH = d.ch;
    const u_src: [*]const u8 = @ptrCast(api.avs_get_read_ptr_p.?(src, c.AVS_PLANAR_U));
    const v_src: [*]const u8 = @ptrCast(api.avs_get_read_ptr_p.?(src, c.AVS_PLANAR_V));
    const u_dst: [*]u8 = @ptrCast(api.avs_get_write_ptr_p.?(dst, c.AVS_PLANAR_U));
    const v_dst: [*]u8 = @ptrCast(api.avs_get_write_ptr_p.?(dst, c.AVS_PLANAR_V));
    const u_pitch: usize = @intCast(api.avs_get_pitch_p.?(src, c.AVS_PLANAR_U));
    const v_pitch: usize = @intCast(api.avs_get_pitch_p.?(src, c.AVS_PLANAR_V));
    const ud_pitch: usize = @intCast(api.avs_get_pitch_p.?(dst, c.AVS_PLANAR_U));
    const vd_pitch: usize = @intCast(api.avs_get_pitch_p.?(dst, c.AVS_PLANAR_V));
    for (0..CH) |cy| {
        for (0..CW) |cx| {
            var g_avg: f32 = undefined;
            if (d.is_yv12) {
                const x0 = cx * 2;
                const y0 = cy * 2;
                const x1 = @min(x0 + 1, W - 1);
                const y1 = @min(y0 + 1, H - 1);
                g_avg = (d.pg[y0 * W + x0] + d.pg[y0 * W + x1] + d.pg[y1 * W + x0] + d.pg[y1 * W + x1]) / 4.0;
            } else {
                g_avg = d.pg[cy * W + cx];
            }
            var sat = 1.0 + (g_avg - 1.0) * d.coef_sat;
            if (sat > d.max_sat) sat = d.max_sat;
            if (sat < d.min_sat) sat = d.min_sat;
            const un: i32 = @as(i32, u_src[cy * u_pitch + cx]) - 128;
            const vn: i32 = @as(i32, v_src[cy * v_pitch + cx]) - 128;
            const uo = @as(i32, 128) + @as(i32, @intFromFloat(@round(@as(f32, @floatFromInt(un)) * sat))) + d.shift_u;
            const vo = @as(i32, 128) + @as(i32, @intFromFloat(@round(@as(f32, @floatFromInt(vn)) * sat))) + d.shift_v;
            u_dst[cy * ud_pitch + cx] = @intCast(std.math.clamp(uo, 0, 255));
            v_dst[cy * vd_pitch + cx] = @intCast(std.math.clamp(vo, 0, 255));
        }
    }

    _ = d.debug;
    return dst;
}

/// Registers the Aurora function. Called from main.zig's plugin init.
pub fn register(env: *c.AVS_ScriptEnvironment, avs_api: *const avs.AvsApi) void {
    api = avs_api;
    _ = api.avs_add_function.?(
        env,
        "Aurora",
        "c[avg_lum]i[max_gain]f[min_gain]f[coef_gain]f[max_sat]f[min_sat]f[coef_sat]f" ++
            "[avg_window]i[response]i[debug]b[mode]i[engine]s[protect]i[passes]i[shift]i" ++
            "[shadows]b[shift_u]i[shift_v]i[corrector]f[reducer]f[black_clip]f[freezer]i" ++
            "[radius]i[clip_limit]f[tiles]i[pg_smooth]f[scene_cut]f[domain]s",
        &auroraCreate,
        null,
    );
}
