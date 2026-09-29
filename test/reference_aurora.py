#!/usr/bin/env python3
"""Independent Aurora reference implementation, mirroring src/aurora.zig +
src/common.zig float32 arithmetic operation-by-operation for cross-checking.
The exact input YV12 planes are dumped by the C host (host_aurora_*_src.raw),
so no RGB->YUV reimplementation is needed here."""
import numpy as np

f32 = np.float32
WEIGHTS = np.exp(-np.power(np.abs(
    (np.log(np.arange(256, dtype=f32) + f32(0.1))[:, None]
     - np.log(np.arange(256, dtype=f32) + f32(0.1))[None, :])
    / np.log(f32(5.0))), f32(25.0))).astype(f32)


def round_orig(val):  # mirror common.roundOrig
    v = np.int32(val)
    return v + ((val - f32(0.5)).astype(np.int32) == v)


def box_blur(src, w, h, r):
    """Mirror common.boxBlur: separable running-window with clamped edges."""
    src = src.reshape(h, w).astype(f32)
    tmp = np.zeros((h, w), dtype=f32)
    out = np.zeros((h, w), dtype=f32)
    # horizontal
    for y in range(h):
        row = src[y]
        acc = f32(0.0); lo = 0; hi = min(r, w - 1)
        for i in range(lo, hi + 1): acc = f32(acc + row[i])
        tmp[y, 0] = f32(acc / f32(hi - lo + 1))
        for x in range(1, w):
            new_lo = x - r if x > r else 0
            new_hi = x + r if x + r < w else w - 1
            if new_lo > lo:
                acc = f32(acc - row[lo]); lo = new_lo
            if new_hi > hi:
                acc = f32(acc + row[new_hi]); hi = new_hi
            tmp[y, x] = f32(acc / f32(hi - lo + 1))
    # vertical
    for x in range(w):
        acc = f32(0.0); lo = 0; hi = min(r, h - 1)
        for y in range(lo, hi + 1): acc = f32(acc + tmp[y, x])
        out[lo, x] = f32(acc / f32(hi - lo + 1))
        for y in range(1, h):
            new_lo = y - r if y > r else 0
            new_hi = y + r if y + r < h else h - 1
            if new_lo > lo:
                acc = f32(acc - tmp[lo, x]); lo = new_lo
            if new_hi > hi:
                acc = f32(acc + tmp[new_hi, x]); hi = new_hi
            out[y, x] = f32(acc / f32(hi - lo + 1))
    return out.ravel()


def guided_filter(y, w, h, r, eps=26.0):
    yf = y.astype(f32).ravel()
    mean_i = box_blur(yf, w, h, r)
    mean_ii = box_blur(yf * yf, w, h, r)
    var = mean_ii - mean_i * mean_i
    a = var / (var + f32(eps))
    b = mean_i * (f32(1.0) - a)
    q = box_blur(a, w, h, r) * yf + box_blur(b, w, h, r)
    return q


def separable_weighted(src_u8, w, h, r):
    """Mirror aurora.zig separableWeightedMean (two passes, averaged)."""
    y = src_u8.reshape(h, w)
    dst = np.zeros((h, w), dtype=f32)
    for yy in range(h):
        for x in range(w):
            yhw = y[yy, x]
            acc = f32(0.0); wsum = f32(0.0)
            for xx in range(max(0, x - r), min(w - 1, x + r) + 1):
                wgt = WEIGHTS[y[yy, xx], yhw]
                wsum = f32(wsum + wgt); acc = f32(acc + wgt * f32(y[yy, xx]))
            dst[yy, x] = f32(acc / wsum)
    for x in range(w):
        for yy in range(h):
            yhw = y[yy, x]
            acc = f32(0.0); wsum = f32(0.0)
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


def build_ylut(hist, gauss, curr_gain, protect_on):
    ylut = np.zeros(256, dtype=f32)
    acc = 0; last_y = f32(0.0); gi = 0
    limit = f32(204.0) / curr_gain
    for i in range(256):
        acc += int(hist[i])
        while gi < 255 and f32(acc) > gauss[gi]: gi += 1
        new_lum = f32(gi); max_lum = last_y
        if last_y >= f32(235.0): max_lum = f32(last_y + 1.0)
        elif (not protect_on) or f32(i) < limit: max_lum = f32(last_y + curr_gain)
        else:
            t = (f32(i) - limit) / (f32(235.0) - limit) * (last_y - limit) / (f32(235.0) - limit)
            max_lum = f32(last_y + curr_gain - np.sqrt(t, dtype=f32) * (curr_gain - f32(1.0)))
        if new_lum > max_lum: new_lum = max_lum
        if new_lum < f32(i): new_lum = f32(i)
        ylut[i] = new_lum; last_y = new_lum
    return ylut


class AuroraRef:
    def __init__(self, w, h, engine='guided', avg_lum=128, max_gain=3.0, min_gain=1.0,
                 coef_gain=1.0, max_sat=9.0, min_sat=0.0, coef_sat=1.0,
                 avg_window=-1, response=100, protect=2, passes=4, shift=0,
                 shadows=True, shift_u=0, shift_v=0, corrector=0.0, reducer=0.5,
                 black_clip=0.0, freezer=-1, radius=7, clip_limit=2.0, tiles=8, fps=25):
        self.w, self.h = w, h
        self.engine = engine
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
        if avg_window == -1: avg_window = int(np.ceil(f32(fps)))
        self.avg_window = max(1, avg_window)
        self.prev_gain = np.zeros(self.avg_window, dtype=f32)
        self.index = 0; self.last_gain = f32(0.0)
        self.frozen = False
        self.gauss = build_gauss(avg_lum, w * h)

    def process(self, Y, U, V):
        w, h, N = self.w, self.h, self.w * self.h
        # ---- 1. black_clip + shift ----
        hist_raw = np.bincount(Y.ravel(), minlength=256)
        black_offset = self.shift
        if self.black_clip > 0.0:
            target = int(self.black_clip * f32(N))
            acc = 0
            for i in range(256):
                acc += hist_raw[i]
                if acc >= target:
                    black_offset += i; break
        ybuf = np.clip(Y.astype(np.int32) - black_offset, 0, 255).astype(np.uint8).ravel()
        hist = np.bincount(ybuf.ravel(), minlength=256).astype(np.uint32)
        max_luma = int(ybuf.max())

        # ---- 2. global gain ----
        sb = int(hist[9:193].sum()); vb = int((np.arange(9, 193) * hist[9:193]).sum())
        if sb == 0: curr_gain = self.min_gain
        else:
            mean = f32(vb) / f32(sb)
            curr_gain = f32(f32(self.avg_lum) * self.coef_gain / mean)
            curr_gain = max(curr_gain, self.min_gain); curr_gain = min(curr_gain, self.max_gain)

        protect_on = (self.protect == 1) or (self.protect == 2 and max_luma >= 250)

        if self.freezer >= 0:
            if not self.frozen:
                self.frozen_ylut = build_ylut(hist, self.gauss, curr_gain, protect_on)
                self.frozen_gain = curr_gain
                self.frozen = True
            ylut = self.frozen_ylut; curr_gain = self.frozen_gain
        else:
            if self.last_gain == 0.0:
                self.index = 0; self.last_gain = curr_gain
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
            ylut = build_ylut(hist, self.gauss, curr_gain, protect_on)

        # ---- 5. local estimator ----
        if self.engine == 'guided':
            lmap = guided_filter(ybuf, w, h, self.radius)
        elif self.engine == 'clahe':
            lmap = clahe(ybuf, w, h, self.tiles, self.clip_limit)
        else:
            lmap = separable_weighted(ybuf, w, h, self.radius)
            for _ in range(self.passes - 1):
                ytmp = np.clip(round_orig(lmap), 0, 255).astype(np.uint8)
                lmap = separable_weighted(ytmp, w, h, self.radius)

        # ---- 6. gain map + corrector ----
        pg = np.zeros(N, dtype=f32)
        for p in range(N):
            lum = lmap[p]
            li = min(255, max(0, int(round_orig(np.float32(lum)))))
            g = curr_gain if lum <= 0.0 else ylut[li] / lum
            if not np.isfinite(g): g = curr_gain
            g = max(f32(1.0), min(g, curr_gain))
            y0 = f32(ybuf[p])
            factor = f32(self.corrector + (f32(1.0) - self.corrector) * (f32(1.0) - y0 / f32(255.0)))
            pg[p] = f32(f32(1.0) + (g - f32(1.0)) * factor)

        # ---- 7. reducer ----
        if self.reducer > 0.0:
            rblur = int(np.ceil(self.reducer * f32(3.0)))
            blurred = box_blur(pg, w, h, rblur)
            a = min(self.reducer, 1.0) * 0.5
            pg = f32(pg * (f32(1.0) - a) + blurred * a)

        # ---- 8a. luma apply ----
        Yout = np.zeros(N, dtype=np.uint8)
        for yy in range(h):
            for x in range(w):
                idx = yy * w + x
                g = pg[idx]
                out = f32(ybuf[idx]) * g
                if self.shadows and g > 1.0:
                    t = f32(1.0) - out / f32(255.0)
                    out = f32(out + min((g - f32(1.0)) * f32(4.0), f32(12.0)) * t * t)
                Yout[idx] = min(255, max(0, int(round_orig(np.float32(out)))))

        # ---- 8b. chroma ----
        cw, ch = (w + 1) // 2, (h + 1) // 2
        Uout = np.zeros((ch, cw), dtype=np.uint8); Vout = np.zeros((ch, cw), dtype=np.uint8)
        pgm = pg.reshape(h, w)
        for cy in range(ch):
            for cx in range(cw):
                x0, y0 = cx * 2, cy * 2
                x1, y1 = min(x0 + 1, w - 1), min(y0 + 1, h - 1)
                g_avg = f32((pgm[y0, x0] + pgm[y0, x1] + pgm[y1, x0] + pgm[y1, x1]) / f32(4.0))
                sat = f32(1.0) + (g_avg - f32(1.0)) * self.coef_sat
                sat = min(max(sat, self.min_sat), self.max_sat)
                un = np.int32(U[cy, cx]) - 128  # signed, like Zig i32 math
                vn = np.int32(V[cy, cx]) - 128
                uo = int(128 + int(np.floor(f32(un) * sat + f32(0.5))) + self.shift_u)
                vo = int(128 + int(np.floor(f32(vn) * sat + f32(0.5))) + self.shift_v)
                Uout[cy, cx] = min(255, max(0, uo)); Vout[cy, cx] = min(255, max(0, vo))
        return Yout.reshape(h, w), Uout, Vout


def load_yuv(name, w, h, n):
    Y = np.fromfile(f'/tmp/host_{name}.raw', dtype=np.uint8).reshape(n, h, w)
    cw, ch = (w + 1) // 2, (h + 1) // 2
    U = np.fromfile(f'/tmp/host_{name}.raw.u', dtype=np.uint8).reshape(n, ch, cw)
    V = np.fromfile(f'/tmp/host_{name}.raw.v', dtype=np.uint8).reshape(n, ch, cw)
    return Y, U, V


def compare(name, w, h, n, ref, tol):
    Yg, Ug, Vg = load_yuv(name, w, h, n)
    worst = 0
    for fr in range(n):
        Yo, Uo, Vo = ref.process(Yg[fr].copy(), Ug[fr].copy(), Vg[fr].copy())
        dy = np.abs(Yg[fr].astype(int) - Yo.astype(int)).max()
        du = np.abs(Ug[fr].astype(int) - Uo.astype(int)).max()
        dv = np.abs(Vg[fr].astype(int) - Vo.astype(int)).max()
        worst = max(worst, dy, du, dv)
    status = 'OK' if worst <= tol else 'FAIL'
    print(f'{name:22s}: worst_abs_diff={worst} (tol {tol}) {status}')
    return worst <= tol


if __name__ == '__main__':
    ok = True
    # scenario 5: default (guided engine), dark, 6 frames
    r = AuroraRef(64, 48)
    ok &= compare('aurora_default', 64, 48, 6, r, tol=2)
    # scenario 6: CLAHE engine
    r = AuroraRef(64, 48, engine='clahe')
    ok &= compare('aurora_clahe', 64, 48, 3, r, tol=2)
    # scenario 7: freezer+corrector+reducer+black_clip+shift (guided)
    r = AuroraRef(64, 48, freezer=0, corrector=0.9, reducer=1.0, black_clip=0.01, shift=4)
    ok &= compare('aurora_freeze', 64, 48, 4, r, tol=2)
    print('AURORA CROSS-CHECK', 'PASSED' if ok else 'FAILED')
