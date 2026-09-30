#!/usr/bin/env python3
"""Independent Aurora reference — mirrors src/aurora.zig + src/common.zig
float32 arithmetic operation-by-operation. Input YUV planes are dumped by
the C host (host_aurora_*_src.raw) so no colorspace reimplementation is
needed. Supports YV12 and YUV444P, domain gamma/linear/log, temporal gain-map
IIR (pg_smooth) and scene-cut detection (scene_cut)."""
import numpy as np

f32 = np.float32
WEIGHTS = np.exp(-np.power(np.abs(
    (np.log(np.arange(256, dtype=f32) + f32(0.1))[:, None]
     - np.log(np.arange(256, dtype=f32) + f32(0.1))[None, :])
    / np.log(f32(5.0))), f32(25.0))).astype(f32)


def round_orig(val):
    v = np.int32(val)
    return v + ((val - f32(0.5)).astype(np.int32) == v)


# --- domain transforms: LUTs shared verbatim with the plugin ---
# (src/tables/*.txt is the single source of truth — bit-identical on both
# sides, avoiding libm pow()/log() ULP differences between implementations)
import os
_TBL_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'tables')
def _load_tbl(name):
    with open(os.path.join(_TBL_DIR, name + '.txt')) as f:
        return np.array([np.float32(line.strip()) for line in f if line.strip()], dtype=f32)
_TBL = {n: _load_tbl(n) for n in ('linear_fwd', 'linear_inv', 'log_fwd', 'log_inv')}


def _idx(v):
    i = int(np.floor(f32(v) + f32(0.5)))  # round half away from zero (v >= 0)
    return min(255, max(0, i))


def fwd(domain, v):
    if domain == 'gamma':
        return v
    return _TBL['linear_fwd' if domain == 'linear' else 'log_fwd'][_idx(v)]


def inv(domain, v):
    if domain == 'gamma':
        return v
    return _TBL['linear_inv' if domain == 'linear' else 'log_inv'][_idx(v)]


def box_blur(src, w, h, r):
    src = src.reshape(h, w).astype(f32)
    tmp = np.zeros((h, w), dtype=f32)
    out = np.zeros((h, w), dtype=f32)
    for y in range(h):
        row = src[y]
        acc = f32(0.0); lo = 0; hi = min(r, w - 1)
        for i in range(lo, hi + 1): acc = f32(acc + row[i])
        tmp[y, 0] = f32(acc / f32(hi - lo + 1))
        for x in range(1, w):
            new_lo = x - r if x > r else 0
            new_hi = x + r if x + r < w else w - 1
            if new_lo > lo: acc = f32(acc - row[lo]); lo = new_lo
            if new_hi > hi: acc = f32(acc + row[new_hi]); hi = new_hi
            tmp[y, x] = f32(acc / f32(hi - lo + 1))
    for x in range(w):
        acc = f32(0.0); lo = 0; hi = min(r, h - 1)
        for y in range(lo, hi + 1): acc = f32(acc + tmp[y, x])
        out[lo, x] = f32(acc / f32(hi - lo + 1))
        for y in range(1, h):
            new_lo = y - r if y > r else 0
            new_hi = y + r if y + r < h else h - 1
            if new_lo > lo: acc = f32(acc - tmp[lo, x]); lo = new_lo
            if new_hi > hi: acc = f32(acc + tmp[new_hi, x]); hi = new_hi
            out[y, x] = f32(acc / f32(hi - lo + 1))
    return out.ravel()


def guided_filter(y, w, h, r, eps=26.0):
    yf = y.astype(f32).ravel()
    mean_i = box_blur(yf, w, h, r)
    mean_ii = box_blur(yf * yf, w, h, r)
    var = mean_ii - mean_i * mean_i
    a = var / (var + f32(eps))
    b = mean_i * (f32(1.0) - a)
    return box_blur(a, w, h, r) * yf + box_blur(b, w, h, r)


def separable_weighted(src_u8, w, h, r):
    y = src_u8.reshape(h, w)
    dst = np.zeros((h, w), dtype=f32)
    for yy in range(h):
        for x in range(w):
            yhw = y[yy, x]; acc = f32(0.0); wsum = f32(0.0)
            for xx in range(max(0, x - r), min(w - 1, x + r) + 1):
                wgt = WEIGHTS[y[yy, xx], yhw]
                wsum = f32(wsum + wgt); acc = f32(acc + wgt * f32(y[yy, xx]))
            dst[yy, x] = f32(acc / wsum)
    for x in range(w):
        for yy in range(h):
            yhw = y[yy, x]; acc = f32(0.0); wsum = f32(0.0)
            for y2 in range(max(0, yy - r), min(h - 1, yy + r) + 1):
                wgt = WEIGHTS[y[y2, x], yhw]
                wsum = f32(wsum + wgt); acc = f32(acc + wgt * f32(y[y2, x]))
            dst[yy, x] = f32((dst[yy, x] + acc / wsum) / f32(2.0))
    return dst.ravel()


def clahe(y, w, h, tiles, clip_limit):
    y = y.reshape(h, w)
    tw = (w + tiles - 1) // tiles
    th = (h + tiles - 1) // tiles
    luts = np.zeros((tiles, tiles, 256), dtype=np.uint8)
    for ty in range(tiles):
        for tx in range(tiles):
            x0, y0 = tx * tw, ty * th
            x1, y1 = min(x0 + tw, w), min(y0 + th, h)
            tile = y[y0:y1, x0:x1].ravel()
            hist = np.bincount(tile, minlength=256).astype(np.uint32)
            clip = int(clip_limit * len(tile) / 256.0)
            if clip > 0:
                excess = 0
                for i in range(256):
                    if hist[i] > clip:
                        excess += hist[i] - clip; hist[i] = clip
                redist = excess // 256; rem = excess % 256
                for i in range(256):
                    hist[i] += redist
                    if rem > 0: hist[i] += 1; rem -= 1
            acc = 0
            for i in range(256):
                acc += hist[i]
                luts[ty, tx, i] = min(255, (acc * 255 + len(tile) // 2) // len(tile))
    dst = np.zeros((h, w), dtype=f32)
    if tiles == 1:
        lut = luts[0, 0]
        for yy in range(h):
            for x in range(w):
                dst[yy, x] = f32(lut[y[yy, x]])
        return dst.ravel()
    for yy in range(h):
        fy = (f32(yy) + f32(0.5)) / f32(th) - f32(0.5)
        y0i = int(np.floor(fy)); wy = f32(fy - np.floor(fy))
        if y0i < 0: y0i, wy = 0, f32(0.0)
        if y0i > tiles - 2: y0i, wy = tiles - 2, f32(1.0)
        for x in range(w):
            fx = (f32(x) + f32(0.5)) / f32(tw) - f32(0.5)
            x0i = int(np.floor(fx)); wx = f32(fx - np.floor(fx))
            if x0i < 0: x0i, wx = 0, f32(0.0)
            if x0i > tiles - 2: x0i, wx = tiles - 2, f32(1.0)
            v = y[yy, x]
            l00 = f32(luts[y0i, x0i, v]);     l01 = f32(luts[y0i, x0i + 1, v])
            l10 = f32(luts[y0i + 1, x0i, v]); l11 = f32(luts[y0i + 1, x0i + 1, v])
            top = l00 * (f32(1.0) - wx) + l01 * wx
            bot = l10 * (f32(1.0) - wx) + l11 * wx
            dst[yy, x] = top * (f32(1.0) - wy) + bot * wy
    return dst.ravel()


def build_gauss(avg_lum, pixels):
    sigma = f32(1.5); length = f32(4.0); pi = f32(3.141593)
    alpha = f32(1.0) / (sigma * np.sqrt(f32(2.0) * pi))
    mu = (f32(-avg_lum) + f32(128.0)) * f32(8.0) / f32(256.0)
    g = np.zeros(256, dtype=f32); suma = f32(0.0)
    for i in range(256):
        x = length - f32(2.0) * length * f32(i) / f32(255.0)
        prob = alpha * np.exp(-((x - mu) ** 2) / (f32(2.0) * sigma * sigma))
        suma = f32(suma + prob)
        g[i] = f32(suma * f32(pixels))
    return g / suma


def build_ylut(hist, gauss, curr_gain, protect_on, lum_hi, limit):
    ylut = np.zeros(256, dtype=f32)
    acc = 0; last_y = f32(0.0); gi = 0
    for i in range(256):
        acc += int(hist[i])
        while gi < 255 and f32(acc) >= gauss[gi]: gi += 1
        new_lum = f32(gi); max_lum = last_y
        if last_y >= lum_hi: max_lum = f32(last_y + 1.0)
        elif (not protect_on) or f32(i) < limit: max_lum = f32(last_y + curr_gain)
        else:
            t = (f32(i) - limit) / (lum_hi - limit) * (last_y - limit) / (lum_hi - limit)
            max_lum = f32(last_y + curr_gain - np.sqrt(t, dtype=f32) * (curr_gain - f32(1.0)))
        if new_lum > max_lum: new_lum = max_lum
        if new_lum < f32(i): new_lum = f32(i)
        ylut[i] = new_lum; last_y = new_lum
    return ylut


class AuroraRef:
    def __init__(self, w, h, engine='guided', fmt='yv12', domain='gamma',
                 avg_lum=128, max_gain=3.0, min_gain=1.0, coef_gain=1.0,
                 max_sat=9.0, min_sat=0.0, coef_sat=1.0,
                 avg_window=-1, response=100, protect=2, passes=4, shift=0,
                 shadows=True, shift_u=0, shift_v=0, corrector=0.0, reducer=0.5,
                 black_clip=0.0, freezer=-1, radius=7, clip_limit=2.0, tiles=8,
                 pg_smooth=0.0, scene_cut=0.0, protect_above=204.0,
                 chroma_mode='sat', contrast=0.0, fps=25):
        self.w, self.h = w, h
        self.engine, self.fmt, self.domain = engine, fmt, domain
        self.avg_lum = avg_lum
        self.max_gain, self.min_gain, self.coef_gain = f32(max_gain), f32(min_gain), f32(coef_gain)
        self.max_sat, self.min_sat, self.coef_sat = f32(max_sat), f32(min_sat), f32(coef_sat)
        self.response = max(1, response); self.protect = protect
        self.passes = max(1, passes); self.shift = shift; self.shadows = shadows
        self.shift_u, self.shift_v = shift_u, shift_v
        self.corrector = f32(corrector); self.reducer = f32(reducer)
        self.black_clip = f32(black_clip); self.freezer = freezer
        self.radius = max(1, radius); self.clip_limit = f32(clip_limit)
        self.tiles = max(1, tiles)
        self.pg_smooth = min(max(f32(pg_smooth), f32(0.0)), f32(0.95))
        self.protect_above = f32(protect_above)
        self.chroma_mode = chroma_mode
        self.contrast = f32(min(max(contrast, 0.0), 1.0))
        self.scene_cut = f32(scene_cut)
        if avg_window == -1: avg_window = int(np.ceil(f32(fps)))
        self.avg_window = max(1, avg_window)
        self.prev_gain = np.zeros(self.avg_window, dtype=f32)
        self.index = 0; self.last_gain = f32(0.0)
        self.frozen = False
        self.hist_prev = None
        self.pg_prev = None
        # domain precomputations
        self.avg_work = int(round_orig(np.float32(fwd(domain, f32(avg_lum)))))
        self.bin_lo = int(round_orig(np.float32(fwd(domain, f32(9.0)))))
        self.bin_hi = min(255, int(round_orig(np.float32(fwd(domain, f32(193.0))))))
        self.lum_white = fwd(domain, f32(250.0))
        self.lum_hi = fwd(domain, f32(235.0))
        self.lum_204 = fwd(domain, f32(204.0))
        self.gauss = build_gauss(self.avg_work, w * h)

    def process(self, Y, U, V, fr=None):
        # seek-reset mirror: fr=None means "sequential, no reset expected"
        if fr is not None:
            if getattr(self, '_last_fr', -1) >= 0 and fr != self._last_fr + 1:
                self.prev_gain[:] = 0.0
                self.last_gain = f32(0.0)
                self.index = 0
                self.pg_prev = None
                self.hist_prev = None
            self._last_fr = fr
        w, h, N = self.w, self.h, self.w * self.h
        dom = self.domain
        # 1. black_clip + shift
        hist_raw = np.bincount(Y.ravel(), minlength=256)
        black_offset = self.shift
        if self.black_clip > 0.0:
            target = int(self.black_clip * f32(N))
            acc = 0
            for i in range(256):
                acc += hist_raw[i]
                if acc >= target: black_offset += i; break
        ybuf = np.clip(Y.astype(np.int32) - black_offset, 0, 255).astype(np.uint8).ravel()
        # 2. domain transform
        if dom == 'gamma':
            est_in = ybuf
            wbuf = ybuf.astype(f32)
        else:
            wv = np.array([fwd(dom, f32(v)) for v in ybuf], dtype=f32)
            wbuf = wv
            est_in = np.clip(round_orig(wv), 0, 255).astype(np.uint8)
        hist = np.bincount(est_in, minlength=256).astype(np.uint32)
        max_work = 0
        for i in range(255, -1, -1):
            if hist[i] > 0: max_work = i; break
        # 3. global gain
        sb = int(hist[self.bin_lo:self.bin_hi + 1].sum())
        vb = int((np.arange(self.bin_lo, self.bin_hi + 1) * hist[self.bin_lo:self.bin_hi + 1]).sum())
        degenerate = False
        if sb == 0:
            degenerate = True
            curr_gain = self.last_gain if self.last_gain > 0.0 else self.min_gain
        else:
            mean = f32(vb) / f32(sb)
            curr_gain = f32(f32(self.avg_work) * self.coef_gain / mean)
            curr_gain = max(curr_gain, self.min_gain); curr_gain = min(curr_gain, self.max_gain)
        # 4. scene cut
        if self.scene_cut > 0.0 and self.hist_prev is not None:
            diff = 0
            for i in range(256):
                diff += abs(int(hist[i]) - int(self.hist_prev[i]))
            dist = f32(diff) / f32(2 * N)
            if dist > self.scene_cut:
                self.prev_gain[:] = 0.0
                self.last_gain = f32(0.0)
                self.index = 0
                self.pg_prev = None
        self.hist_prev = hist.copy()
        # 5. freezer / temporal
        protect_on = (self.protect == 1) or (self.protect == 2 and f32(max_work) >= self.lum_white)
        if self.freezer >= 0:
            if not self.frozen:
                self.frozen_ylut = build_ylut(hist, self.gauss, curr_gain, protect_on,
                                              self.lum_hi, fwd(self.domain, f32(self.protect_above) / curr_gain))
                self.frozen_gain = curr_gain
                self.frozen = True
            ylut = self.frozen_ylut; curr_gain = self.frozen_gain
        else:
            if self.last_gain == 0.0:
                self.index = 0; self.last_gain = curr_gain
            if not degenerate:
                self.prev_gain[self.index] = curr_gain
                self.index = (self.index + 1) % self.avg_window
            avail = self.prev_gain != 0.0
            avg = f32(self.prev_gain[avail].sum() / f32(avail.sum()))
            if abs(avg - self.last_gain) / self.last_gain * 100.0 > f32(self.response):
                if avg < self.last_gain:
                    self.last_gain = f32(self.last_gain * f32(100 - self.response) / f32(100))
                else:
                    self.last_gain = f32(self.last_gain * f32(100 + self.response) / f32(100))
            else:
                self.last_gain = avg
            curr_gain = self.last_gain
            ylut = build_ylut(hist, self.gauss, curr_gain, protect_on,
                              self.lum_hi, fwd(self.domain, f32(self.protect_above) / curr_gain))
        # 6. estimator
        if self.engine == 'guided':
            lmap = guided_filter(est_in, w, h, self.radius)
        elif self.engine == 'clahe':
            lmap = clahe(est_in, w, h, self.tiles, self.clip_limit)
        else:
            lmap = separable_weighted(est_in, w, h, self.radius)
            for _ in range(self.passes - 1):
                ytmp = np.clip(round_orig(lmap), 0, 255).astype(np.uint8)
                lmap = separable_weighted(ytmp, w, h, self.radius)
        # 7. gain map + corrector
        pg = np.zeros(N, dtype=f32)
        for p in range(N):
            lum = lmap[p]
            li = min(255, max(0, int(round_orig(np.float32(lum)))))
            g = curr_gain if lum <= 0.0 else ylut[li] / lum
            if not np.isfinite(g): g = curr_gain
            g = max(f32(1.0), min(g, curr_gain))
            y0 = wbuf[p]
            factor = f32(self.corrector + (f32(1.0) - self.corrector) * (f32(1.0) - y0 / f32(255.0)))
            pg[p] = f32(f32(1.0) + (g - f32(1.0)) * factor)
        # reducer
        if self.reducer > 0.0:
            rblur = int(np.ceil(self.reducer * f32(3.0)))
            blurred = box_blur(pg, w, h, rblur)
            a = min(self.reducer, 1.0) * 0.5
            pg = f32(pg * (f32(1.0) - a) + blurred * a)
        # 8. temporal IIR
        if self.pg_smooth > 0.0:
            if self.pg_prev is not None:
                pg = f32(pg * (f32(1.0) - self.pg_smooth) + self.pg_prev * self.pg_smooth)
            self.pg_prev = pg.copy()
        # 9. apply
        Yout = np.zeros(N, dtype=np.uint8)
        for yy in range(h):
            for x in range(w):
                idx = yy * w + x
                g = pg[idx]
                out = f32(wbuf[idx]) * g
                if self.shadows and g > 1.0:
                    t = f32(1.0) - out / f32(255.0)
                    out = f32(out + min((g - f32(1.0)) * f32(4.0), f32(12.0)) * t * t)
                if dom != 'gamma':
                    out = inv(dom, out)
                if self.contrast > 0.0:
                    slope = f32(1.0) + self.contrast * f32(0.2)
                    pivot = f32(self.avg_lum)
                    curved = pivot + (out - pivot) * slope
                    wblend = min(1.0, max(0.0, out / (pivot * f32(0.4))))
                    out = out * (f32(1.0) - wblend) + curved * wblend
                Yout[idx] = min(255, max(0, int(round_orig(np.float32(out)))))
        # 10. chroma
        yv12 = (self.fmt == 'yv12')
        cw = (w + 1) // 2 if yv12 else w
        ch = (h + 1) // 2 if yv12 else h
        Uout = np.zeros((ch, cw), dtype=np.uint8); Vout = np.zeros((ch, cw), dtype=np.uint8)
        pgm = pg.reshape(h, w)
        for cy in range(ch):
            for cx in range(cw):
                if yv12:
                    x0, y0 = cx * 2, cy * 2
                    x1, y1 = min(x0 + 1, w - 1), min(y0 + 1, h - 1)
                    g_avg = f32((pgm[y0, x0] + pgm[y0, x1] + pgm[y1, x0] + pgm[y1, x1]) / f32(4.0))
                else:
                    g_avg = pgm[cy, cx]
                sat = f32(1.0) + (g_avg - f32(1.0)) * self.coef_sat
                sat = min(max(sat, self.min_sat), self.max_sat)
                if self.chroma_mode == 'vibrance':
                    mag = max(abs(int(U[cy, cx]) - 128), abs(int(V[cy, cx]) - 128))
                    vw = min(1.0, max(0.0, 1.0 - mag / 64.0))  # NOT 'w' - shadows frame width
                    sat = f32(1.0) + (sat - f32(1.0)) * f32(vw)
                un = np.int32(U[cy, cx]) - 128
                vn = np.int32(V[cy, cx]) - 128
                uo = int(128 + int(np.floor(f32(un) * sat + f32(0.5))) + self.shift_u)
                vo = int(128 + int(np.floor(f32(vn) * sat + f32(0.5))) + self.shift_v)
                Uout[cy, cx] = min(255, max(0, uo)); Vout[cy, cx] = min(255, max(0, vo))
        return Yout.reshape(h, w), Uout, Vout


def load_yuv(name, w, h, n, fmt='yv12'):
    Y = np.fromfile(f'/mnt/agents/output/hdlt/host_{name}.raw', dtype=np.uint8).reshape(n, h, w)
    cw = (w + 1) // 2 if fmt == 'yv12' else w
    ch = (h + 1) // 2 if fmt == 'yv12' else h
    U = np.fromfile(f'/mnt/agents/output/hdlt/host_{name}.raw.u', dtype=np.uint8).reshape(n, ch, cw)
    V = np.fromfile(f'/mnt/agents/output/hdlt/host_{name}.raw.v', dtype=np.uint8).reshape(n, ch, cw)
    return Y, U, V


def load_src(name, w, h, n, fmt='yv12'):
    """The plugin's INPUT planes, dumped by the host (host_<name>_src.raw)."""
    Y = np.fromfile(f'/mnt/agents/output/hdlt/host_{name}_src.raw', dtype=np.uint8).reshape(n, h, w)
    cw = (w + 1) // 2 if fmt == 'yv12' else w
    ch = (h + 1) // 2 if fmt == 'yv12' else h
    U = np.fromfile(f'/mnt/agents/output/hdlt/host_{name}_src.raw.u', dtype=np.uint8).reshape(n, ch, cw)
    V = np.fromfile(f'/mnt/agents/output/hdlt/host_{name}_src.raw.v', dtype=np.uint8).reshape(n, ch, cw)
    return Y, U, V


def compare(name, w, h, n, ref, tol, fmt='yv12'):
    # got = plugin output; ref runs on the plugin's actual INPUT (src dump)
    Yg, Ug, Vg = load_yuv(name, w, h, n, fmt)
    Ys, Us, Vs = load_src(name, w, h, n, fmt)
    worst = 0
    for fr in range(n):
        Yo, Uo, Vo = ref.process(Ys[fr].copy(), Us[fr].copy(), Vs[fr].copy(), fr=fr)
        dy = np.abs(Yg[fr].astype(int) - Yo.astype(int)).max()
        du = np.abs(Ug[fr].astype(int) - Uo.astype(int)).max()
        dv = np.abs(Vg[fr].astype(int) - Vo.astype(int)).max()
        worst = max(worst, dy, du, dv)
    status = 'OK' if worst <= tol else 'FAIL'
    print(f'{name:22s}: worst_abs_diff={worst} (tol {tol}) {status}')
    return worst <= tol


if __name__ == '__main__':
    ok = True
    r = AuroraRef(64, 48)
    ok &= compare('aurora_default', 64, 48, 6, r, tol=2)
    r = AuroraRef(64, 48, engine='clahe')
    ok &= compare('aurora_clahe', 64, 48, 3, r, tol=2)
    r = AuroraRef(64, 48, freezer=0, corrector=0.9, reducer=1.0, black_clip=0.01, shift=4)
    ok &= compare('aurora_freeze', 64, 48, 4, r, tol=2)
    r = AuroraRef(64, 48, domain='linear')
    ok &= compare('aurora_lin', 64, 48, 3, r, tol=3)
    r = AuroraRef(64, 48, domain='log')
    ok &= compare('aurora_log', 64, 48, 3, r, tol=3)
    r = AuroraRef(64, 48, fmt='yuv444p')
    ok &= compare('aurora_444', 64, 48, 3, r, tol=2, fmt='yuv444p')
    r = AuroraRef(64, 48, pg_smooth=0.7, scene_cut=0.2)
    ok &= compare('aurora_tpg', 64, 48, 6, r, tol=2)
    # --- ColorBars (non-uniform) ---
    ok &= compare('cb_dark', 64, 48, 3, AuroraRef(64, 48), tol=2)
    ok &= compare('cb_clahe', 64, 48, 3, AuroraRef(64, 48, engine='clahe'), tol=2)
    ok &= compare('cb_lin', 64, 48, 3, AuroraRef(64, 48, domain='linear'), tol=3)
    ok &= compare('cb_tpg', 64, 48, 6, AuroraRef(64, 48, pg_smooth=0.5, scene_cut=0.3), tol=2)
    # protect taper in the LOG domain on bright content (regression: limit
    # must be computed as fwd(204/gain), not fwd(204)/gain)
    ok &= compare('cb_prot', 64, 48, 3, AuroraRef(64, 48, domain='log', protect=1, protect_above=160.0), tol=2)
    ok &= compare('cb_vib', 64, 48, 3, AuroraRef(64, 48, chroma_mode='vibrance', coef_sat=2.6), tol=2)
    # contrast restore (gamma-domain, protected dark floor)
    ok &= compare('cb_ctr', 64, 48, 3, AuroraRef(64, 48, contrast=0.8), tol=2)
    print('AURORA CROSS-CHECK', 'PASSED' if ok else 'FAILED')
