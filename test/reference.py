#!/usr/bin/env python3
"""Independent HDRAGC 0.1.5 reference implementation (from the C source)
for cross-checking the Zig plugin output. Float32 everywhere to mirror
the C arithmetic."""
import numpy as np
import os
import sys

f32 = np.float32

WEIGHTS = np.exp(-np.power(np.abs(
    (np.log(np.arange(256, dtype=f32) + f32(0.1))[:, None]
     - np.log(np.arange(256, dtype=f32) + f32(0.1))[None, :])
    / np.log(f32(5.0))), f32(25.0))).astype(f32)

RLUM, GLUM, BLUM = f32(0.3086), f32(0.6094), f32(0.0820)

class HdrAgcRef:
    def __init__(self, w, h, avg_lum=128, max_gain=3.0, min_gain=1.0,
                 coef_gain=1.0, max_sat=2.0, min_sat=1.0, coef_sat=1.0,
                 circle=7, avg_window=1, response=100, sigma=1.5, mode=1):
        self.w, self.h = w, h
        self.avg_lum, self.max_gain, self.min_gain, self.coef_gain = avg_lum, f32(max_gain), f32(min_gain), f32(coef_gain)
        self.max_sat, self.min_sat = f32(max_sat), f32(min_sat)
        self.coef_sat = f32(coef_sat if coef_sat != 0.0 else (max_sat - min_sat) / (max_gain - 1.0))
        self.circle, self.response, self.mode = circle, response, mode
        if avg_window == -1: avg_window = int(np.ceil(f32(25))) // 2
        self.avg_window = max(1, avg_window)
        self.response = max(1, response)
        self.sigma = f32(sigma if sigma >= 0 else 1.5)  # deviasi #3
        self.prev_gain = np.zeros(self.avg_window, dtype=f32)
        self.index = 0
        self.last_gain = f32(0.0)
        # gauss table
        length, pi = f32(4.0), f32(3.141593)
        alpha = f32(1.0) / (self.sigma * np.sqrt(f32(2.0) * pi, dtype=f32))
        mu = (f32(-avg_lum) + f32(128.0)) * (f32(2.0) * length) / f32(256.0)
        pixels = w * h
        suma = f32(0.0)
        self.gauss = np.zeros(256, dtype=f32)
        for i in range(256):
            x = length - f32(2.0) * length * f32(i) / f32(255.0)
            prob = alpha * np.exp(-((x - mu) ** 2) / (f32(2.0) * self.sigma * self.sigma), dtype=f32)
            suma = f32(suma + prob)
            self.gauss[i] = f32(suma * f32(pixels))
        self.gauss /= suma
        # sat tables
        sat_size = int((self.max_sat - 1.0) * 100.0) + 1
        i100 = np.arange(sat_size, dtype=f32) / f32(100.0)
        a = (f32(1.0) - (f32(1.0) + i100)) * RLUM
        cc = (f32(1.0) - (f32(1.0) + i100)) * GLUM
        e = (f32(1.0) - (f32(1.0) + i100)) * BLUM
        self.A, self.B, self.C = a, a + (f32(1.0) + i100), cc
        self.D, self.E, self.F = cc + (f32(1.0) + i100), e, e + (f32(1.0) + i100)

    def _round_orig(self, val):
        # round() buatan author: (int)val; if ((int)(val-0.5)==round) round++
        v = val.astype(np.int32)
        return v + ((val - f32(0.5)).astype(np.int32) == v)

    def process(self, frame_u8):
        """frame_u8: (h, w, 4) BGRX -> returns (h, w, 4)"""
        H, W = self.h, self.w
        b = frame_u8[:, :, 0].astype(np.uint16)
        g = frame_u8[:, :, 1].astype(np.uint16)
        r = frame_u8[:, :, 2].astype(np.uint16)
        y = ((r + g + b) // 3).astype(np.uint8)
        hist = np.bincount(y.ravel(), minlength=256).astype(np.uint32)

        # local luminance
        c = self.circle
        yf = y.astype(f32)
        yloc = np.zeros((H, W), dtype=f32)
        if self.mode == 0:
            cmat = np.zeros((2*c+1, 2*c+1), dtype=bool)  # BUG asli: x in [-c, c)
            for x in range(-c, c):
                for yy in range(-c, c):
                    if x*x + yy*yy <= c*c: cmat[x+c, yy+c] = True
            for iy in range(H):
                for ix in range(W):
                    acc = f32(0.0); wsum = f32(0.0); yhw = y[iy, ix]
                    for yy in range(max(0, iy-c), min(H-1, iy+c)+1):
                        for xx in range(max(0, ix-c), min(W-1, ix+c)+1):
                            if cmat[ix-xx+c, iy-yy+c]:
                                yxy = y[yy, xx]
                                wt = WEIGHTS[yxy, yhw]
                                wsum = f32(wsum + wt); acc = f32(acc + wt * f32(yxy))
                    yloc[iy, ix] = f32(acc / wsum)
        else:
            # separable H+V averaged
            for iy in range(H):
                for ix in range(W):
                    yhw = y[iy, ix]
                    acc = f32(0.0); wsum = f32(0.0)
                    for xx in range(max(0, ix-c), min(W-1, ix+c)+1):
                        yxy = y[iy, xx]; wt = WEIGHTS[yxy, yhw]
                        wsum = f32(wsum + wt); acc = f32(acc + wt * f32(yxy))
                    loc = f32(acc / wsum)
                    acc = f32(0.0); wsum = f32(0.0)
                    for yy in range(max(0, iy-c), min(H-1, iy+c)+1):
                        yxy = y[yy, ix]; wt = WEIGHTS[yxy, yhw]
                        wsum = f32(wsum + wt); acc = f32(acc + wt * f32(yxy))
                    yloc[iy, ix] = f32((loc + acc / wsum) / f32(2.0))

        # global gain
        sum_bins = int(hist[9:193].sum()); val_bins = int((np.arange(9, 193) * hist[9:193]).sum())
        if sum_bins == 0:
            curr_max = self.min_gain
        else:
            mean = f32(val_bins) / f32(sum_bins)
            curr_max = f32(f32(128.0) * self.coef_gain / mean)
            curr_max = max(curr_max, self.min_gain)
            curr_max = min(curr_max, self.max_gain)
        if self.last_gain == 0.0:
            self.index = 0; self.last_gain = curr_max
        self.prev_gain[self.index] = curr_max
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
        curr_max = self.last_gain

        # YLUT
        ylut = np.zeros(256, dtype=f32)
        acc = 0; last_y = f32(0.0); gi = 0
        limit = f32(204.0) / curr_max
        for i in range(256):
            acc += int(hist[i])
            while gi < 255 and f32(acc) > self.gauss[gi]: gi += 1
            new_lum = f32(gi)
            max_lum = last_y
            if last_y >= f32(235.0): max_lum = f32(last_y + 1.0)
            elif f32(i) < limit: max_lum = f32(last_y + curr_max)
            else:
                t = (f32(i) - limit) / (f32(235.0) - limit) * (last_y - limit) / (f32(235.0) - limit)
                max_lum = f32(last_y + curr_max - np.sqrt(t, dtype=f32) * (curr_max - f32(1.0)))
            if new_lum > max_lum: new_lum = max_lum
            if new_lum < f32(i): new_lum = f32(i)
            ylut[i] = new_lum; last_y = new_lum

        # apply
        out = np.zeros((H, W, 4), dtype=np.uint8)
        for iy in range(H):
            for ix in range(W):
                yl = yloc[iy, ix]
                if yl <= 0.0: gain = curr_max
                else: gain = ylut[min(255, max(0, int(self._round_orig(np.float32(yl)))))] / yl
                if not np.isfinite(gain): gain = curr_max
                gain = max(f32(1.0), min(gain, curr_max))
                f_sat = (gain - f32(1.0)) * self.coef_sat + self.min_sat - f32(1.0)
                f_sat = min(max(f_sat, self.min_sat - f32(1.0)), self.max_sat - f32(1.0))
                if f_sat < 0.0: f_sat = f32(0.0)
                sat = min(int(f_sat * 100.0), len(self.A) - 1)
                R, G, B = f32(r[iy, ix]), f32(g[iy, ix]), f32(b[iy, ix])
                rn = int((self.B[sat]*R + self.C[sat]*G + self.E[sat]*B) * gain)
                gn = int((self.A[sat]*R + self.D[sat]*G + self.E[sat]*B) * gain)
                bn = int((self.A[sat]*R + self.C[sat]*G + self.F[sat]*B) * gain)
                out[iy, ix, 0] = min(255, max(0, bn))
                out[iy, ix, 1] = min(255, max(0, gn))
                out[iy, ix, 2] = min(255, max(0, rn))
                out[iy, ix, 3] = 0
        return out


def blank(w, h, length, color):
    b = color & 0xFF; g = (color >> 8) & 0xFF; r = (color >> 16) & 0xFF
    fr = np.zeros((h, w, 4), dtype=np.uint8); fr[:, :, 0] = b; fr[:, :, 1] = g; fr[:, :, 2] = r
    return [fr.copy() for _ in range(length)]

def compare(name, w, h, frames, ref_frames):
    out = os.environ.get('HDLT_OUT', '/tmp/hdlt')
    got = np.fromfile(f'{out}/host_{name}.raw', dtype=np.uint8).reshape(-1, h, w, 4)
    assert got.shape[0] == len(ref_frames), (got.shape, len(ref_frames))
    worst = 0; total = 0; n = 0
    for i, rf in enumerate(ref_frames):
        d = np.abs(got[i].astype(int)[:, :, :3] - rf[:, :, :3].astype(int))
        worst = max(worst, d.max()); total += d.sum(); n += d.size
    print(f'{name:10s}: shape OK, max_abs_diff={worst}, mean={total/n:.5f}')
    return worst

if __name__ == '__main__':
    # skenario identik dengan host.c
    src = blank(64, 48, 3, 0x35507A)
    r = HdrAgcRef(64, 48, max_gain=1.0)
    compare('identity', 64, 48, src, [r.process(f) for f in src])

    src = blank(64, 48, 8, 0x202020)
    r = HdrAgcRef(64, 48)
    compare('dark', 64, 48, src, [r.process(f) for f in src])

    src = blank(48, 32, 6, 0x202020)
    r = HdrAgcRef(48, 32, mode=0, circle=5, avg_window=4, response=50)
    compare('dark_m0', 48, 32, src, [r.process(f) for f in src])

    src = blank(64, 48, 3, 0xD0D0D0)
    r = HdrAgcRef(64, 48)
    compare('bright', 64, 48, src, [r.process(f) for f in src])
