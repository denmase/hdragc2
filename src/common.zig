//! Shared helpers for the HDRAGC2 plugin family (HDRAGC 0.1.5 port + Aurora).

const std = @import("std");

/// The original author's custom round(): `(int)val; if ((int)(val-0.5)==round) round++`.
/// Truncation toward zero matches the C cast.
pub fn roundOrig(val: f32) i32 {
    var r: i32 = @intFromFloat(val);
    if (@as(i32, @intFromFloat(val - 0.5)) == r) r += 1;
    return r;
}

/// Edge-aware weight table, built at comptime (256 KB in .rodata).
/// weights[i][j] = exp(-(|log(i+.1)-log(j+.1)|/log 5)^25) with float fabs
/// (DEVIATION: the C source used abs() whose float behavior is
/// compiler-dependent; the intended float absolute value is used).
pub const weights = blk: {
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

/// Gaussian target-distribution table for histogram matching, identical to
/// the HDRAGC 0.1.5 constructor (cumulative, normalized to `pixels`).
pub fn buildGauss(gauss: *[256]f32, avg_lum: i32, sigma: f32, pixels: usize) void {
    const length: f32 = 4.0;
    const pi: f32 = 3.141593;
    const alpha: f32 = 1.0 / (sigma * @sqrt(2.0 * pi));
    const mu: f32 = (-@as(f32, @floatFromInt(avg_lum)) + 128.0) * (2.0 * length) / 256.0;
    var suma: f32 = 0.0;
    for (0..256) |i| {
        const x: f32 = length - 2.0 * length * @as(f32, @floatFromInt(i)) / 255.0;
        const dx = x - mu;
        const prob = alpha * @exp(-(dx * dx) / (2.0 * sigma * sigma));
        suma += prob;
        gauss[i] = suma * @as(f32, @floatFromInt(pixels));
    }
    for (0..256) |i| gauss[i] /= suma;
}

/// Separable box blur on a float plane (running-sum implementation, O(w*h)).
pub fn boxBlur(src: []const f32, dst: []f32, w: usize, h: usize, r: usize) void {
    if (r == 0) {
        @memcpy(dst, src);
        return;
    }
    const tmp = std.heap.c_allocator.alloc(f32, w * h) catch return;
    defer std.heap.c_allocator.free(tmp);
    // Horizontal pass. Running window [lo..hi] slides one pixel right per
    // step; at frame edges the window is clamped (shrinks or stays pinned),
    // which the lo/hi update handles exactly — no over/under-counting.
    for (0..h) |y| {
        const row = src[y * w .. (y + 1) * w];
        const out = tmp[y * w .. (y + 1) * w];
        var acc: f32 = 0.0;
        var lo: usize = 0;
        var hi: usize = if (r < w) r else w - 1;
        for (lo..hi + 1) |i| acc += row[i];
        out[0] = acc / @as(f32, @floatFromInt(hi - lo + 1));
        for (1..w) |x| {
            const new_lo = if (x > r) x - r else 0;
            const new_hi = if (x + r < w) x + r else w - 1;
            if (new_lo > lo) {
                acc -= row[lo];
                lo = new_lo;
            }
            if (new_hi > hi) {
                acc += row[new_hi];
                hi = new_hi;
            }
            out[x] = acc / @as(f32, @floatFromInt(hi - lo + 1));
        }
    }
    // Vertical pass, identical logic per column.
    for (0..w) |x| {
        var acc: f32 = 0.0;
        var lo: usize = 0;
        var hi: usize = if (r < h) r else h - 1;
        for (lo..hi + 1) |y| acc += tmp[y * w + x];
        dst[lo * w + x] = acc / @as(f32, @floatFromInt(hi - lo + 1));
        for (1..h) |y| {
            const new_lo = if (y > r) y - r else 0;
            const new_hi = if (y + r < h) y + r else h - 1;
            if (new_lo > lo) {
                acc -= tmp[lo * w + x];
                lo = new_lo;
            }
            if (new_hi > hi) {
                acc += tmp[new_hi * w + x];
                hi = new_hi;
            }
            dst[y * w + x] = acc / @as(f32, @floatFromInt(hi - lo + 1));
        }
    }
}

/// Tone-mapping LUT (histogram matching against the gauss target, with
/// optional highlight tapering) — the YLUT stage shared by Aurora's engines.
/// Identical math to the verified HDRAGC 0.1.5 get_frame loop; `protect_on`
/// enables the "204/gain" taper that eases the slope to gain 1.0 at luma 235.
pub fn buildYlut(ylut: *[256]f32, hist: *const [256]u32, gauss: *const [256]f32, curr_gain: f32, protect_on: bool, lum_hi: f32, limit: f32) void {
    var acc: u32 = 0;
    var last_y: f32 = 0.0;
    var gauss_i: usize = 0;
    for (0..256) |i| {
        acc += hist[i];
        // Walk the gauss CDF to the bin whose cumulative count matches the
        // source histogram's cumulative count -> classic histogram matching.
        // '>=' (not '>'): the f32 cumulative sum plateaus once the remaining
        // probabilities fall below ULP, making gauss[i] == pixel count
        // exactly for a range of i; '>' then halts the walk early and leaks
        // an un-clamped new_lum. '>=' lets the clamp handle the plateau.
        while (gauss_i < 255 and @as(f32, @floatFromInt(acc)) >= gauss[gauss_i])
            gauss_i += 1;
        var new_lum: f32 = @floatFromInt(gauss_i);
        var max_lum: f32 = last_y;
        if (last_y >= lum_hi) {
            max_lum += 1.0; // at white: only allow a single gray step
        } else if (!protect_on or @as(f32, @floatFromInt(i)) < limit) {
            max_lum += curr_gain; // full gain below the protect threshold
        } else {
            // Above the threshold, taper the added slope with a sqrt curve
            // so highlights approach gain 1.0 at lum_hi instead of clipping.
            const t = (@as(f32, @floatFromInt(i)) - limit) / (lum_hi - limit)
                * (last_y - limit) / (lum_hi - limit);
            max_lum += curr_gain - @sqrt(t) * (curr_gain - 1.0);
        }
        if (new_lum > max_lum) new_lum = max_lum;
        if (new_lum < @as(f32, @floatFromInt(i))) new_lum = @floatFromInt(i); // never darken
        ylut[i] = new_lum;
        last_y = new_lum;
    }
}

/// Self-guided filter (He et al. 2010) on an 8-bit luma plane, output as
/// float in 0..255. `eps` is the regularization variance.
pub fn guidedFilter(src: []const u8, dst: []f32, tmp1: []f32, tmp2: []f32, w: usize, h: usize, r: usize, eps: f32) void {
    const n = w * h;
    const mean_i = tmp1; // float luma + its box blur reused below
    const ii = dst; // I*I (as float)
    // mean_I
    for (0..n) |i| mean_i[i] = @floatFromInt(src[i]);
    const mean_blur = tmp2;
    boxBlur(mean_i, mean_blur, w, h, r);
    // var = mean(I^2) - mean(I)^2 ; a = var/(var+eps); b = meanI*(1-a)
    // reuse mean_i to hold `a`, mean_blur holds meanI
    const a_coef = mean_i;
    const meanI = mean_blur;
    // compute I^2 into ii (dst temporarily)
    for (0..n) |i| {
        const v: f32 = @floatFromInt(src[i]);
        ii[i] = v * v;
    }
    // NOTE: buffer juggling is handled by the caller providing distinct
    // scratch; here we allocate once more for clarity.
    const mean_ii = std.heap.c_allocator.alloc(f32, n) catch return;
    defer std.heap.c_allocator.free(mean_ii);
    boxBlur(ii, mean_ii, w, h, r);
    const b_coef = ii; // reuse ii for b
    for (0..n) |i| {
        const v = mean_ii[i] - meanI[i] * meanI[i];
        a_coef[i] = v / (v + eps);
        b_coef[i] = meanI[i] * (1.0 - a_coef[i]);
    }
    // q = box(a)*I + box(b)
    const box_a = mean_ii; // reuse
    boxBlur(a_coef, box_a, w, h, r);
    const box_b = meanI; // reuse (meanI no longer needed)
    boxBlur(b_coef, box_b, w, h, r);
    for (0..n) |i| {
        const v: f32 = @floatFromInt(src[i]);
        dst[i] = box_a[i] * v + box_b[i];
    }
}

/// CLAHE (Contrast-Limited Adaptive Histogram Equalization) on an 8-bit luma
/// plane. `clip_limit` is expressed like OpenCV (relative to the uniform
/// distribution: clip = clip_limit * tile_pixels / 256). Output float 0..255.
pub fn clahe(src: []const u8, dst: []f32, w: usize, h: usize, tiles: usize, clip_limit: f32) void {
    const n = w * h;
    const tw = (w + tiles - 1) / tiles;
    const th = (h + tiles - 1) / tiles;
    // per-tile LUTs
    const luts = std.heap.c_allocator.alloc(u8, tiles * tiles * 256) catch return;
    defer std.heap.c_allocator.free(luts);
    for (0..tiles) |ty| {
        for (0..tiles) |tx| {
            const x0 = tx * tw;
            const y0 = ty * th;
            const x1 = @min(x0 + tw, w);
            const y1 = @min(y0 + th, h);
            const tile_px = (x1 - x0) * (y1 - y0);
            const lut = luts[(ty * tiles + tx) * 256 ..][0..256];
            var hist: [256]u32 = [_]u32{0} ** 256;
            for (y0..y1) |y| {
                for (x0..x1) |x| hist[src[y * w + x]] += 1;
            }
            // clip and redistribute
            const clip = @as(u32, @intFromFloat(clip_limit * @as(f32, @floatFromInt(tile_px)) / 256.0));
            if (clip > 0) {
                var excess: u32 = 0;
                for (0..256) |i| {
                    if (hist[i] > clip) {
                        excess += hist[i] - clip;
                        hist[i] = clip;
                    }
                }
                const redist: u32 = excess / 256;
                var rem: u32 = excess % 256;
                for (0..256) |i| {
                    hist[i] += redist;
                    if (rem > 0) {
                        hist[i] += 1;
                        rem -= 1;
                    }
                }
            }
            // CDF -> LUT (scaled to 255)
            var acc: u32 = 0;
            for (0..256) |i| {
                acc += hist[i];
                lut[i] = @intCast(@min(255, (acc * 255 + tile_px / 2) / tile_px));
            }
        }
    }
    // Degenerate case: a single tile has no neighbors to interpolate with.
    if (tiles == 1) {
        const lut = luts[0..256];
        for (0..n) |i| dst[i] = @floatFromInt(lut[src[i]]);
        return;
    }
    // Bilinear interpolation between neighboring tile LUTs (OpenCV-style:
    // sample position is measured relative to tile centers).
    for (0..h) |y| {
        // tile row coordinates: center-based interpolation like OpenCV
        const fy = (@as(f32, @floatFromInt(y)) + 0.5) / @as(f32, @floatFromInt(th)) - 0.5;
        var y0i: i32 = @intFromFloat(@floor(fy));
        var wy: f32 = fy - @floor(fy);
        if (y0i < 0) {
            y0i = 0;
            wy = 0;
        }
        if (y0i > @as(i32, @intCast(tiles)) - 2) {
            y0i = @as(i32, @intCast(tiles)) - 2;
            wy = 1;
        }
        for (0..w) |x| {
            const fx = (@as(f32, @floatFromInt(x)) + 0.5) / @as(f32, @floatFromInt(tw)) - 0.5;
            var x0i: i32 = @intFromFloat(@floor(fx));
            var wx: f32 = fx - @floor(fx);
            if (x0i < 0) {
                x0i = 0;
                wx = 0;
            }
            if (x0i > @as(i32, @intCast(tiles)) - 2) {
                x0i = @as(i32, @intCast(tiles)) - 2;
                wx = 1;
            }
            const v = src[y * w + x];
            const l00 = luts[(@as(usize, @intCast(y0i)) * tiles + @as(usize, @intCast(x0i))) * 256 + v];
            const l01 = luts[(@as(usize, @intCast(y0i)) * tiles + @as(usize, @intCast(x0i)) + 1) * 256 + v];
            const l10 = luts[(@as(usize, @intCast(y0i + 1)) * tiles + @as(usize, @intCast(x0i))) * 256 + v];
            const l11 = luts[(@as(usize, @intCast(y0i + 1)) * tiles + @as(usize, @intCast(x0i)) + 1) * 256 + v];
            const top = @as(f32, @floatFromInt(l00)) * (1.0 - wx) + @as(f32, @floatFromInt(l01)) * wx;
            const bot = @as(f32, @floatFromInt(l10)) * (1.0 - wx) + @as(f32, @floatFromInt(l11)) * wx;
            dst[y * w + x] = top * (1.0 - wy) + bot * wy;
        }
    }
}


// ---------------------------------------------------------------------------
// Working-domain transforms for Aurora's `domain` parameter, implemented as
// 256-entry LUTs shared verbatim with test/reference_aurora.py (the tables
// under src/tables/ are the single source of truth). Why LUTs instead of
// powf/logf at runtime:
//   * deterministic and BIT-IDENTICAL across Zig and the Python reference
//     (libm pow() differs by ULPs between implementations, which shifted
//     histogram bins and produced a 10-level output diff in domain=linear);
//   * faster: one rounding + one lookup per call.
// Indexing: round-to-nearest (half away from zero, but inputs are >= 0),
// clamped to 0..255 — mirrored exactly by the reference.
//   gamma  : identity (no table)
//   linear : sRGB EOTF, fwd = decode, inv = encode
//   log    : 255*ln(1+v)/ln(256)
// ---------------------------------------------------------------------------
pub const Domain = enum { gamma, linear, log };

fn parseDomainTable(comptime text: []const u8) [256]f32 {
    @setEvalBranchQuota(100_000);
    var tbl: [256]f32 = undefined;
    var it = std.mem.splitScalar(u8, text, '\n');
    var i: usize = 0;
    while (it.next()) |line| {
        const t = std.mem.trim(u8, line, " \t\r");
        if (t.len == 0) continue;
        tbl[i] = std.fmt.parseFloat(f32, t) catch @panic("bad domain table");
        i += 1;
    }
    if (i != 256) @panic("domain table must have 256 entries");
    return tbl;
}

const linear_fwd_tbl = parseDomainTable(@embedFile("tables/linear_fwd.txt"));
const linear_inv_tbl = parseDomainTable(@embedFile("tables/linear_inv.txt"));
const log_fwd_tbl = parseDomainTable(@embedFile("tables/log_fwd.txt"));
const log_inv_tbl = parseDomainTable(@embedFile("tables/log_inv.txt"));

fn domainIdx(v: f32) usize {
    const r = @round(v);
    const cl = std.math.clamp(r, 0.0, 255.0);
    return @intFromFloat(cl);
}

pub fn fwd(d: Domain, v: f32) f32 {
    return switch (d) {
        .gamma => v,
        .linear => linear_fwd_tbl[domainIdx(v)],
        .log => log_fwd_tbl[domainIdx(v)],
    };
}

pub fn inv(d: Domain, v: f32) f32 {
    return switch (d) {
        .gamma => v,
        .linear => linear_inv_tbl[domainIdx(v)],
        .log => log_inv_tbl[domainIdx(v)],
    };
}
