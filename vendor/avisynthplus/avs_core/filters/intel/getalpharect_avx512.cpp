// AviSynth + .Copyright 2026 - AviSynth + Project
// https://avs-plus.net
// http://avisynth.nl
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA, or visit
// http://www.gnu.org/copyleft/gpl.html .
//
// Linking Avisynth statically or dynamically with other modules is making a
// combined work based on Avisynth.  Thus, the terms and conditions of the GNU
// General Public License cover the whole combination.
//
// As a special exception, the copyright holders of Avisynth give you
// permission to link Avisynth with independent modules that communicate with
// Avisynth solely through the interfaces defined in avisynth.h, regardless of the license
// terms of these independent modules, and to copy and distribute the
// resulting combined work under terms of your choice, provided that
// every copy of the combined work is accompanied by a complete copy of
// the source code of Avisynth (the version of Avisynth used to produce the
// combined work), being distributed under the terms of the GNU General
// Public License plus this exception.  An independent module is a module
// which is not derived from or based on Avisynth, such as 3rd-party filters,
// import and export plugins, or graphical user interfaces.

// GetAlphaRect — AVX-512 FAST tier (BITALG + VL required).
// Compiled with AVX512_FAST flags (CPUF_AVX512_FAST dispatch).
//
// Changes over AVX2:
//
//   alpha1 (non-interlaced):
//     _mm_popcnt_epi8 (VPOPCNTB, AVX-512 BITALG+VL) counts set bits in all
//     8 gathered bytes with a single instruction, then PSADBW sums the counts.
//     Replaces the nibble-LUT + PSHUFB approach (SSE4.1) and the
//     scatter-into-uint64 + hardware POPCNT64 approach (AVX2).
//
//   alpha1 (interlaced):
//     All 16 rows (-4..11) are gathered into one 128-bit register; a single
//     _mm_popcnt_epi8 + both PSADBW halves replaces two separate POPCNT64
//     calls on split uint64_t values (AVX2).
//
//   cenmask (non-interlaced):
//     simd_bitexr / simd_bitexl compute the per-byte smear analytically via
//     3 masked shift-and-OR steps each (right-smear: v|(v<<1)|(v<<2)|(v<<4)
//     per byte; left-smear: v|(v>>1)|(v>>2)|(v>>4) per byte), followed by a
//     3-step SIMD horizontal OR to reduce 8 bytes to one.  No 256-entry
//     bitexr[]/bitexl[] table lookups.
//
//   alpha2 (non-interlaced):
//     Fully vectorised with forward and backward inclusive prefix-OR scans:
//       - gather right/center/left neighbors for rows 8..15 (forward) and
//         rows -8..-1 (backward) into XMM registers via PINSRB×8 each;
//       - apply analytical simd_bitexr / simd_bitexl + simd_nonzero per batch;
//       - inclusive prefix-OR (byte 0→7): 3 × _mm_slli_si128 + OR steps;
//       - inclusive suffix-OR (byte 7→0): 3 × _mm_srli_si128 + OR steps;
//       - combined[i] = fwd[i] | bwd[i]; _mm_popcnt_epi8 + PSADBW give alpha2.
//     Replaces 2 scalar loops totalling 48 table lookups (bitexr/bitexl/bitcnt).
//
//   alpha2 (interlaced):
//     Scalar path identical to AVX2 (the interlaced sweep is complex and
//     interlaced video is rarely encountered in practice).
//
//   Quick-exit batch:
//     Identical SSE2 vectorisation as the SSE4.1 and AVX2 tiers: 16-byte OR
//     across all rows, sliding 3-byte OR for per-column empty check.
//

#include <avs/config.h>
#if defined(AVS_WINDOWS) && !defined(NO_WIN_GDI)
#ifdef INTEL_INTRINSICS_AVX512

#include "../getalpharect_impl.h"
#include "check_avx512.h"

#ifdef AVS_WINDOWS
#include <avs/win.h>
#else
#include <avs/posix.h>
#endif

#include <cstring>

// Sum all 16 bytes of an XMM register via two PSADBW half-sums.
static inline int xmm_hsum16_epi8(__m128i v) {
  __m128i s = _mm_sad_epu8(v, _mm_setzero_si128());
  return _mm_cvtsi128_si32(s) + _mm_extract_epi16(s, 4);
}

// Per-byte left-smear: bit k set in v → bits 0..k set in result.
// Equivalent to bitexl[] table but computed analytically via 3 masked shifts.
static inline __m128i simd_bitexl(__m128i v) {
  const __m128i m7f = _mm_set1_epi8(0x7F);
  const __m128i m3f = _mm_set1_epi8(0x3F);
  const __m128i m0f = _mm_set1_epi8(0x0F);
  __m128i t = _mm_or_si128(v, _mm_and_si128(_mm_srli_epi16(v, 1), m7f));
  t = _mm_or_si128(t, _mm_and_si128(_mm_srli_epi16(t, 2), m3f));
  t = _mm_or_si128(t, _mm_and_si128(_mm_srli_epi16(t, 4), m0f));
  return t;
}

// Per-byte right-smear: bit k set in v → bits k..7 set in result.
// Equivalent to bitexr[] table but computed analytically via 3 masked shifts.
static inline __m128i simd_bitexr(__m128i v) {
  const __m128i mfe = _mm_set1_epi8((int8_t)0xFE);
  const __m128i mfc = _mm_set1_epi8((int8_t)0xFC);
  const __m128i mf0 = _mm_set1_epi8((int8_t)0xF0);
  __m128i t = _mm_or_si128(v, _mm_and_si128(_mm_slli_epi16(v, 1), mfe));
  t = _mm_or_si128(t, _mm_and_si128(_mm_slli_epi16(t, 2), mfc));
  t = _mm_or_si128(t, _mm_and_si128(_mm_slli_epi16(t, 4), mf0));
  return t;
}

// 0xFF per byte where nonzero, 0x00 where zero (equivalent to scalar -!!x).
static inline __m128i simd_nonzero(__m128i v) {
  return _mm_andnot_si128(_mm_cmpeq_epi8(v, _mm_setzero_si128()),
                          _mm_set1_epi8((int8_t)0xFF));
}

template<bool noaa, bool interlaced>
static inline void gar_avx512_col(
    const BYTE* src, int srcpitch,
    const uint8_t* bitcnt, const uint8_t* bitexl, const uint8_t* bitexr,
    const uint16_t* gamma,
    int Atext, int Ahalo,
    int RYtext, int GUtext, int BVtext,
    int RYhalo, int GUhalo, int BVhalo,
    uint16_t* dest_ba, uint16_t* dest_ry, uint16_t* dest_gu, uint16_t* dest_bv)
{
  int alpha1 = 0, alpha2 = 0;

  if constexpr (interlaced) {
    // alpha1: rows -4..11 (16 rows) gathered into one XMM, single _mm_popcnt_epi8.
    // Lane indices are compile-time constants for PINSRB.
    __m128i v = _mm_setzero_si128();
    v = _mm_insert_epi8(v, src[srcpitch * -4],  0);
    v = _mm_insert_epi8(v, src[srcpitch * -3],  1);
    v = _mm_insert_epi8(v, src[srcpitch * -2],  2);
    v = _mm_insert_epi8(v, src[srcpitch * -1],  3);
    v = _mm_insert_epi8(v, src[srcpitch *  0],  4);
    v = _mm_insert_epi8(v, src[srcpitch *  1],  5);
    v = _mm_insert_epi8(v, src[srcpitch *  2],  6);
    v = _mm_insert_epi8(v, src[srcpitch *  3],  7);
    v = _mm_insert_epi8(v, src[srcpitch *  4],  8);
    v = _mm_insert_epi8(v, src[srcpitch *  5],  9);
    v = _mm_insert_epi8(v, src[srcpitch *  6], 10);
    v = _mm_insert_epi8(v, src[srcpitch *  7], 11);
    v = _mm_insert_epi8(v, src[srcpitch *  8], 12);
    v = _mm_insert_epi8(v, src[srcpitch *  9], 13);
    v = _mm_insert_epi8(v, src[srcpitch * 10], 14);
    v = _mm_insert_epi8(v, src[srcpitch * 11], 15);
    alpha1 = xmm_hsum16_epi8(_mm_popcnt_epi8(v));

    // alpha2: scalar (same as AVX2 tier — complex interlaced sweep).
    int i;
    BYTE topmask = 0, cenmask = 0, botmask = 0;
    BYTE hmasks[16], mask;

    for (i = -4; i < 12; i++) {
      mask = src[srcpitch*i];
      mask = -!!mask;
      mask |= bitexr[src[srcpitch*i-1]];
      mask |= bitexl[src[srcpitch*i+1]];
      hmasks[i+4] = mask;
    }
    for (i = -4; i < 4;  i++) topmask |= hmasks[i+4];
    for (i =  0; i < 8;  i++) cenmask  |= hmasks[i+4];
    for (i =  4; i < 12; i++) botmask  |= hmasks[i+4];

    for (mask = topmask, i = -4; i < 4; i++) {
      mask |= bitexr[src[srcpitch*(i+8)-1]]; mask |= -!!src[srcpitch*(i+8)]; mask |= bitexl[src[srcpitch*(i+8)+1]]; hmasks[i+4] |= mask;
    }
    for (mask = cenmask, i = 0; i < 8; i++) {
      mask |= bitexr[src[srcpitch*(i+8)-1]]; mask |= -!!src[srcpitch*(i+8)]; mask |= bitexl[src[srcpitch*(i+8)+1]]; hmasks[i+4] |= mask;
    }
    for (mask = botmask, i = 4; i < 12; i++) {
      mask |= bitexr[src[srcpitch*(i+8)-1]]; mask |= -!!src[srcpitch*(i+8)]; mask |= bitexl[src[srcpitch*(i+8)+1]]; hmasks[i+4] |= mask;
    }
    for (mask = botmask, i = 11; i >= 4; i--) {
      mask |= bitexr[src[srcpitch*(i-8)-1]]; mask |= -!!src[srcpitch*(i-8)]; mask |= bitexl[src[srcpitch*(i-8)+1]]; hmasks[i+4] |= mask;
    }
    for (mask = cenmask, i = 7; i >= 0; i--) {
      mask |= bitexr[src[srcpitch*(i-8)-1]]; mask |= -!!src[srcpitch*(i-8)]; mask |= bitexl[src[srcpitch*(i-8)+1]]; hmasks[i+4] |= mask;
    }
    for (mask = topmask, i = 3; i >= -4; i--) {
      mask |= bitexr[src[srcpitch*(i-8)-1]]; mask |= -!!src[srcpitch*(i-8)]; mask |= bitexl[src[srcpitch*(i-8)+1]]; hmasks[i+4] |= mask;
    }
    for (i = 0; i < 16; i++) alpha2 += bitcnt[hmasks[i]];

  } else {
    // Non-interlaced: no bitcnt/bitexl/bitexr tables needed.
    (void)bitcnt; (void)bitexl; (void)bitexr;

    // alpha1: gather rows 0..7, VPOPCNTB, PSADBW lower half × 2.
    __m128i vb = _mm_setzero_si128();
    vb = _mm_insert_epi8(vb, src[srcpitch * 0], 0);
    vb = _mm_insert_epi8(vb, src[srcpitch * 1], 1);
    vb = _mm_insert_epi8(vb, src[srcpitch * 2], 2);
    vb = _mm_insert_epi8(vb, src[srcpitch * 3], 3);
    vb = _mm_insert_epi8(vb, src[srcpitch * 4], 4);
    vb = _mm_insert_epi8(vb, src[srcpitch * 5], 5);
    vb = _mm_insert_epi8(vb, src[srcpitch * 6], 6);
    vb = _mm_insert_epi8(vb, src[srcpitch * 7], 7);
    // _mm_cvtsi128_si32 extracts lower 32 bits = SAD lower half = sum of bytes 0..7.
    alpha1 = _mm_cvtsi128_si32(
                 _mm_sad_epu8(_mm_popcnt_epi8(vb), _mm_setzero_si128())) * 2;

    if (alpha1) {
      alpha2 = 128;
    } else {
      // cenmask: analytical bitexr(right-neighbor) | bitexl(left-neighbor)
      // for rows 0..7, then horizontal OR to one byte.
      // Unrolled: PINSRB requires compile-time-constant lane index.
      __m128i vr = _mm_setzero_si128(), vl = _mm_setzero_si128();
      vr = _mm_insert_epi8(vr, src[srcpitch*0 - 1], 0);
      vr = _mm_insert_epi8(vr, src[srcpitch*1 - 1], 1);
      vr = _mm_insert_epi8(vr, src[srcpitch*2 - 1], 2);
      vr = _mm_insert_epi8(vr, src[srcpitch*3 - 1], 3);
      vr = _mm_insert_epi8(vr, src[srcpitch*4 - 1], 4);
      vr = _mm_insert_epi8(vr, src[srcpitch*5 - 1], 5);
      vr = _mm_insert_epi8(vr, src[srcpitch*6 - 1], 6);
      vr = _mm_insert_epi8(vr, src[srcpitch*7 - 1], 7);
      vl = _mm_insert_epi8(vl, src[srcpitch*0 + 1], 0);
      vl = _mm_insert_epi8(vl, src[srcpitch*1 + 1], 1);
      vl = _mm_insert_epi8(vl, src[srcpitch*2 + 1], 2);
      vl = _mm_insert_epi8(vl, src[srcpitch*3 + 1], 3);
      vl = _mm_insert_epi8(vl, src[srcpitch*4 + 1], 4);
      vl = _mm_insert_epi8(vl, src[srcpitch*5 + 1], 5);
      vl = _mm_insert_epi8(vl, src[srcpitch*6 + 1], 6);
      vl = _mm_insert_epi8(vl, src[srcpitch*7 + 1], 7);
      __m128i cen = _mm_or_si128(simd_bitexr(vr), simd_bitexl(vl));
      // Horizontal OR of bytes 0..7: three shift-right-and-OR steps.
      cen = _mm_or_si128(cen, _mm_srli_si128(cen, 4));
      cen = _mm_or_si128(cen, _mm_srli_si128(cen, 2));
      cen = _mm_or_si128(cen, _mm_srli_si128(cen, 1));
      BYTE cenmask = (BYTE)_mm_cvtsi128_si32(cen);

      if (cenmask == 0xFF) {
        alpha2 = 128;
      } else {
        // Vectorised forward/backward prefix-OR scans + VPOPCNTB.
        //
        // Forward: fwd[i] = contribution from row i+8 (rows 8..15).
        // After inclusive prefix-OR scan: fwd[i] = fwd[0]|..|fwd[i].
        // Then OR with cenmask gives hmasks[i] equivalent.
        __m128i fr = _mm_setzero_si128(), fc = _mm_setzero_si128(), fl = _mm_setzero_si128();
        fr = _mm_insert_epi8(fr, src[srcpitch* 8 - 1], 0);
        fr = _mm_insert_epi8(fr, src[srcpitch* 9 - 1], 1);
        fr = _mm_insert_epi8(fr, src[srcpitch*10 - 1], 2);
        fr = _mm_insert_epi8(fr, src[srcpitch*11 - 1], 3);
        fr = _mm_insert_epi8(fr, src[srcpitch*12 - 1], 4);
        fr = _mm_insert_epi8(fr, src[srcpitch*13 - 1], 5);
        fr = _mm_insert_epi8(fr, src[srcpitch*14 - 1], 6);
        fr = _mm_insert_epi8(fr, src[srcpitch*15 - 1], 7);
        fc = _mm_insert_epi8(fc, src[srcpitch* 8    ], 0);
        fc = _mm_insert_epi8(fc, src[srcpitch* 9    ], 1);
        fc = _mm_insert_epi8(fc, src[srcpitch*10    ], 2);
        fc = _mm_insert_epi8(fc, src[srcpitch*11    ], 3);
        fc = _mm_insert_epi8(fc, src[srcpitch*12    ], 4);
        fc = _mm_insert_epi8(fc, src[srcpitch*13    ], 5);
        fc = _mm_insert_epi8(fc, src[srcpitch*14    ], 6);
        fc = _mm_insert_epi8(fc, src[srcpitch*15    ], 7);
        fl = _mm_insert_epi8(fl, src[srcpitch* 8 + 1], 0);
        fl = _mm_insert_epi8(fl, src[srcpitch* 9 + 1], 1);
        fl = _mm_insert_epi8(fl, src[srcpitch*10 + 1], 2);
        fl = _mm_insert_epi8(fl, src[srcpitch*11 + 1], 3);
        fl = _mm_insert_epi8(fl, src[srcpitch*12 + 1], 4);
        fl = _mm_insert_epi8(fl, src[srcpitch*13 + 1], 5);
        fl = _mm_insert_epi8(fl, src[srcpitch*14 + 1], 6);
        fl = _mm_insert_epi8(fl, src[srcpitch*15 + 1], 7);
        __m128i fwd = _mm_or_si128(_mm_or_si128(simd_bitexr(fr), simd_bitexl(fl)),
                                   simd_nonzero(fc));
        // Inclusive prefix-OR, byte 0 → byte 7 (3 slli steps cover 8 positions).
        fwd = _mm_or_si128(fwd, _mm_slli_si128(fwd, 1));
        fwd = _mm_or_si128(fwd, _mm_slli_si128(fwd, 2));
        fwd = _mm_or_si128(fwd, _mm_slli_si128(fwd, 4));
        fwd = _mm_or_si128(fwd, _mm_set1_epi8((int8_t)cenmask));

        // Backward: bwd[i] = contribution from row i-8.
        // bwd[7]=row-1, bwd[6]=row-2, ..., bwd[0]=row-8.
        // After inclusive suffix-OR scan: bwd[i] = bwd[i]|..|bwd[7].
        // Then OR with cenmask gives accumulated backward mask at step i.
        __m128i br = _mm_setzero_si128(), bc = _mm_setzero_si128(), bl = _mm_setzero_si128();
        br = _mm_insert_epi8(br, src[srcpitch*(-8) - 1], 0);
        br = _mm_insert_epi8(br, src[srcpitch*(-7) - 1], 1);
        br = _mm_insert_epi8(br, src[srcpitch*(-6) - 1], 2);
        br = _mm_insert_epi8(br, src[srcpitch*(-5) - 1], 3);
        br = _mm_insert_epi8(br, src[srcpitch*(-4) - 1], 4);
        br = _mm_insert_epi8(br, src[srcpitch*(-3) - 1], 5);
        br = _mm_insert_epi8(br, src[srcpitch*(-2) - 1], 6);
        br = _mm_insert_epi8(br, src[srcpitch*(-1) - 1], 7);
        bc = _mm_insert_epi8(bc, src[srcpitch*(-8)    ], 0);
        bc = _mm_insert_epi8(bc, src[srcpitch*(-7)    ], 1);
        bc = _mm_insert_epi8(bc, src[srcpitch*(-6)    ], 2);
        bc = _mm_insert_epi8(bc, src[srcpitch*(-5)    ], 3);
        bc = _mm_insert_epi8(bc, src[srcpitch*(-4)    ], 4);
        bc = _mm_insert_epi8(bc, src[srcpitch*(-3)    ], 5);
        bc = _mm_insert_epi8(bc, src[srcpitch*(-2)    ], 6);
        bc = _mm_insert_epi8(bc, src[srcpitch*(-1)    ], 7);
        bl = _mm_insert_epi8(bl, src[srcpitch*(-8) + 1], 0);
        bl = _mm_insert_epi8(bl, src[srcpitch*(-7) + 1], 1);
        bl = _mm_insert_epi8(bl, src[srcpitch*(-6) + 1], 2);
        bl = _mm_insert_epi8(bl, src[srcpitch*(-5) + 1], 3);
        bl = _mm_insert_epi8(bl, src[srcpitch*(-4) + 1], 4);
        bl = _mm_insert_epi8(bl, src[srcpitch*(-3) + 1], 5);
        bl = _mm_insert_epi8(bl, src[srcpitch*(-2) + 1], 6);
        bl = _mm_insert_epi8(bl, src[srcpitch*(-1) + 1], 7);
        __m128i bwd = _mm_or_si128(_mm_or_si128(simd_bitexr(br), simd_bitexl(bl)),
                                   simd_nonzero(bc));
        // Inclusive suffix-OR, byte 7 → byte 0 (3 srli steps cover 8 positions).
        bwd = _mm_or_si128(bwd, _mm_srli_si128(bwd, 1));
        bwd = _mm_or_si128(bwd, _mm_srli_si128(bwd, 2));
        bwd = _mm_or_si128(bwd, _mm_srli_si128(bwd, 4));
        bwd = _mm_or_si128(bwd, _mm_set1_epi8((int8_t)cenmask));

        // combined[i] = fwd[i] | bwd[i] = hmasks[i] | back_mask_at_i.
        // SAD lower half sums bytes 0..7 only (_mm_cvtsi128_si32 ignores upper).
        __m128i combined = _mm_or_si128(fwd, bwd);
        __m128i pc = _mm_popcnt_epi8(combined);
        alpha2 = _mm_cvtsi128_si32(_mm_sad_epu8(pc, _mm_setzero_si128())) * 2;
      }
    }
  }

  if constexpr (noaa) {
    if (alpha1 >= 1)                         { alpha1 = 64*516*Atext; alpha2 = 0; }
    else if (alpha2 > alpha1 && alpha2 >= 1) { alpha1 = 0; alpha2 = 64*516*Ahalo; }
    else                                     { alpha1 = 0; alpha2 = 0; }
  } else {
    alpha2 = gamma[alpha2]; alpha1 = gamma[alpha1];
    alpha2 -= alpha1; alpha2 *= Ahalo; alpha1 *= Atext;
  }

  *dest_ba = (uint16_t)((64*516*255 - alpha1 -          alpha2) >> 15);
  *dest_bv = (uint16_t)((    BVtext * alpha1 + BVhalo * alpha2) >> 15);
  *dest_gu = (uint16_t)((    GUtext * alpha1 + GUhalo * alpha2) >> 15);
  *dest_ry = (uint16_t)((    RYtext * alpha1 + RYhalo * alpha2) >> 15);
}

template<bool noaa, bool interlaced>
static void GetAlphaRect_avx512_impl(
    const void* lpAntialiasBits_,
    uint16_t*   soa_buf,
    int w, int h, int w_stride,
    int srcpitch,
    int textcolor, int halocolor,
    int& xl, int& yt, int& xr, int& yb)
{
  const GARTables& tbl = gar_tables();
  const uint8_t*  bitcnt = tbl.bitcnt;
  const uint8_t*  bitexl = tbl.bitexl;
  const uint8_t*  bitexr = tbl.bitexr;
  const uint16_t* gamma  = tbl.gamma;

  const int RYtext = ((textcolor>>16)&255), GUtext = ((textcolor>>8)&255), BVtext = (textcolor&255);
  const int RYhalo = ((halocolor>>16)&255), GUhalo = ((halocolor>>8)&255), BVhalo = (halocolor&255);
  const int Atext  = 255 - ((textcolor >> 24) & 0xFF);
  const int Ahalo  = 255 - ((halocolor >> 24) & 0xFF);

  xl = 0; xr = w + 1; yt = -1; yb = h;

  for (int y = 0; y < h; ++y) {
    uint16_t* row     = soa_buf + y * 4 * w_stride;
    uint16_t* dest_ba = row;
    uint16_t* dest_ry = row + w_stride;
    uint16_t* dest_gu = row + 2 * w_stride;
    uint16_t* dest_bv = row + 3 * w_stride;
    const BYTE* src = (const BYTE*)lpAntialiasBits_ + ((h-y-1)*8 + 20) * srcpitch + 2;
    int wt = w;

    // 8-column batch quick-exit (SSE2, always available in AVX-512 TU).
    while (wt >= 16) {
      __m128i acc = _mm_setzero_si128();
      if constexpr (interlaced) {
        for (int ii = -8; ii < 16; ++ii)
          acc = _mm_or_si128(acc, _mm_loadu_si128(
              reinterpret_cast<const __m128i*>(src + srcpitch*ii - 1)));
      } else {
        for (int ii = -12; ii < 20; ++ii)
          acc = _mm_or_si128(acc, _mm_loadu_si128(
              reinterpret_cast<const __m128i*>(src + srcpitch*ii - 1)));
      }
      // Sliding 3-byte OR: pres[c] = acc[c]|acc[c+1]|acc[c+2] = left|center|right for col c.
      __m128i sl1 = _mm_srli_si128(acc, 1);
      __m128i sl2 = _mm_srli_si128(acc, 2);
      __m128i pres = _mm_or_si128(_mm_or_si128(acc, sl1), sl2);
      uint64_t pmask;
      memcpy(&pmask, reinterpret_cast<const char*>(&pres), 8);

      for (int c = 0; c < 8; ++c) {
        const int cwt = wt - c;
        if ((uint8_t)(pmask >> (c * 8)) == 0) {
          *dest_ba++ = 256; *dest_ry++ = 0; *dest_gu++ = 0; *dest_bv++ = 0;
        } else {
          if (cwt >= xl) xl = cwt;
          if (cwt <= xr) xr = cwt;
          if (y   >= yt) yt = y;
          if (y   <= yb) yb = y;
          gar_avx512_col<noaa, interlaced>(
              src + c, srcpitch,
              bitcnt, bitexl, bitexr, gamma,
              Atext, Ahalo,
              RYtext, GUtext, BVtext, RYhalo, GUhalo, BVhalo,
              dest_ba++, dest_ry++, dest_gu++, dest_bv++);
        }
      }
      src += 8;
      wt  -= 8;
    }

    // Scalar tail for remaining wt < 16.
    do {
#pragma warning(push)
#pragma warning(disable: 4068)
      DWORD tmp = 0;
      if constexpr (interlaced) {
#pragma unroll
        for (int ii = -8; ii < 16; ++ii)
          tmp |= *reinterpret_cast<const int*>(src + srcpitch*ii - 1);
      } else {
#pragma unroll
        for (int ii = -12; ii < 20; ++ii)
          tmp |= *reinterpret_cast<const int*>(src + srcpitch*ii - 1);
      }
      tmp &= 0x00FFFFFF;
#pragma warning(pop)

      if (tmp != 0) {
        if (wt >= xl) xl = wt;
        if (wt <= xr) xr = wt;
        if (y  >= yt) yt = y;
        if (y  <= yb) yb = y;
        gar_avx512_col<noaa, interlaced>(
            src, srcpitch,
            bitcnt, bitexl, bitexr, gamma,
            Atext, Ahalo,
            RYtext, GUtext, BVtext, RYhalo, GUhalo, BVhalo,
            dest_ba, dest_ry, dest_gu, dest_bv);
      } else {
        *dest_ba = 256; *dest_ry = 0; *dest_gu = 0; *dest_bv = 0;
      }
      ++dest_ba; ++dest_ry; ++dest_gu; ++dest_bv;
      ++src;
    } while (--wt);
  }

  xl = w - xl;
  xr = w - xr;
}

getalpharect_fn_t GetAlphaRect_select_avx512(bool noaa, bool interlaced) {
  if (noaa) {
    return interlaced ? GetAlphaRect_avx512_impl<true,true>
                      : GetAlphaRect_avx512_impl<true,false>;
  } else {
    return interlaced ? GetAlphaRect_avx512_impl<false,true>
                      : GetAlphaRect_avx512_impl<false,false>;
  }
}

#endif // INTEL_INTRINSICS_AVX512
#endif // AVS_WINDOWS && !NO_WIN_GDI
