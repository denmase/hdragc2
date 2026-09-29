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

// GetAlphaRect — SSE4.1 tier.
//
// Improvements over scalar:
//   quick-exit: 8-column batch via _mm_loadu_si128 + byte-shift ORs,
//               amortising 32 (or 24) stride-scattered reads across 8 cols.
//   alpha1 (non-interlaced): PINSRB×8 + PSHUFB nibble-LUT + PSADBW,
//               no hardware POPCNT needed.

#include <avs/config.h>  // must precede AVS_WINDOWS guard
#if defined(AVS_WINDOWS) && !defined(NO_WIN_GDI)
#ifdef INTEL_INTRINSICS

#include "../getalpharect_impl.h"

#ifdef AVS_WINDOWS
#include <avs/win.h>
#else
#include <avs/posix.h>
#endif

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#include <smmintrin.h>  // SSE4.1 for _mm_insert_epi8
#include <cstring>      // memcpy

// Nibble-LUT popcount: count set bits in each byte of an XMM register.
static inline __m128i xmm_popcount_epi8(__m128i v) {
  const __m128i low4 = _mm_set1_epi8(0x0F);
  const __m128i lut  = _mm_set_epi8(4,3,3,2,3,2,2,1,3,2,2,1,2,1,1,0);
  __m128i lo = _mm_and_si128(v, low4);
  __m128i hi = _mm_and_si128(_mm_srli_epi16(v, 4), low4);
  return _mm_add_epi8(_mm_shuffle_epi8(lut, lo), _mm_shuffle_epi8(lut, hi));
}

// Horizontal byte-sum of an XMM register via PSADBW.
static inline int xmm_hsum_epi8(__m128i v) {
  __m128i s = _mm_sad_epu8(v, _mm_setzero_si128());
  return _mm_cvtsi128_si32(s) + _mm_extract_epi16(s, 4);
}

// Per-column alpha + output computation, shared between batch and scalar tail.
template<bool noaa, bool interlaced>
static inline void gar_sse41_col(
    const BYTE* src, int srcpitch,
    const uint8_t* bitcnt, const uint8_t* bitexl, const uint8_t* bitexr,
    const uint16_t* gamma,
    int Atext, int Ahalo,
    int RYtext, int GUtext, int BVtext,
    int RYhalo, int GUhalo, int BVhalo,
    uint16_t* dest_ba, uint16_t* dest_ry, uint16_t* dest_gu, uint16_t* dest_bv)
{
  int i;
  int alpha1 = 0, alpha2 = 0;

  if constexpr (interlaced) {
    BYTE topmask = 0, cenmask = 0, botmask = 0;
    BYTE hmasks[16], mask;

    for (i = -4; i < 12; i++) {
      mask = src[srcpitch*i];
      alpha1 += bitcnt[mask];
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
    // non-interlaced
    // alpha1: PINSRB × 8 (lane index must be a compile-time constant) →
    // nibble-LUT PSHUFB → PSADBW horizontal sum.
    __m128i vb = _mm_setzero_si128();
    vb = _mm_insert_epi8(vb, src[srcpitch * 0], 0);
    vb = _mm_insert_epi8(vb, src[srcpitch * 1], 1);
    vb = _mm_insert_epi8(vb, src[srcpitch * 2], 2);
    vb = _mm_insert_epi8(vb, src[srcpitch * 3], 3);
    vb = _mm_insert_epi8(vb, src[srcpitch * 4], 4);
    vb = _mm_insert_epi8(vb, src[srcpitch * 5], 5);
    vb = _mm_insert_epi8(vb, src[srcpitch * 6], 6);
    vb = _mm_insert_epi8(vb, src[srcpitch * 7], 7);
    alpha1 = xmm_hsum_epi8(xmm_popcount_epi8(vb)) * 2;

    if (alpha1) {
      alpha2 = 128;
    } else {
      BYTE cenmask = 0;
      for (i = 0; i < 8; i++) {
        cenmask |= bitexr[src[srcpitch*i-1]];
        cenmask |= bitexl[src[srcpitch*i+1]];
      }
      if (cenmask == 0xFF) {
        alpha2 = 128;
      } else {
        BYTE hmasks[8];
        BYTE mask = cenmask;
        for (i = 0; i < 8; i++) {
          mask |= bitexr[src[srcpitch*(i+8)-1]];
          mask |=   -!!src[srcpitch*(i+8)  ];
          mask |= bitexl[src[srcpitch*(i+8)+1]];
          hmasks[i] = mask;
        }
        mask = cenmask;
        for (i = 7; i >= 0; i--) {
          mask |= bitexr[src[srcpitch*(i-8)-1]];
          mask |=   -!!src[srcpitch*(i-8)  ];
          mask |= bitexl[src[srcpitch*(i-8)+1]];
          alpha2 += bitcnt[hmasks[i] | mask];
        }
        alpha2 *= 2;
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
static void GetAlphaRect_sse41_impl(
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

    // 8-column batch quick-exit.
    // Guard wt >= 16 ensures the 16-byte SSE load at src-1 stays within the
    // DIB row: srcpitch >= w+4, load covers src-1..src+14 = col-1..col+14;
    // for the last column in the batch (col = w-wt+7) the rightmost byte is
    // at col+8, i.e., offset w-wt+15 from row base+2; need w-wt+15 < srcpitch,
    // i.e., wt > w - srcpitch + 15 >= w - (w+4) + 15 = 11, so wt >= 12 suffices;
    // wt >= 16 gives extra margin.
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
        const int cwt = wt - c;  // effective wt for bounding box
        if ((uint8_t)(pmask >> (c * 8)) == 0) {
          *dest_ba++ = 256; *dest_ry++ = 0; *dest_gu++ = 0; *dest_bv++ = 0;
        } else {
          if (cwt >= xl) xl = cwt;
          if (cwt <= xr) xr = cwt;
          if (y   >= yt) yt = y;
          if (y   <= yb) yb = y;
          gar_sse41_col<noaa, interlaced>(
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
        gar_sse41_col<noaa, interlaced>(
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

getalpharect_fn_t GetAlphaRect_select_sse41(bool noaa, bool interlaced) {
  if (noaa) {
    return interlaced ? GetAlphaRect_sse41_impl<true,true>
                      : GetAlphaRect_sse41_impl<true,false>;
  } else {
    return interlaced ? GetAlphaRect_sse41_impl<false,true>
                      : GetAlphaRect_sse41_impl<false,false>;
  }
}

#endif // INTEL_INTRINSICS
#endif // AVS_WINDOWS && !NO_WIN_GDI
