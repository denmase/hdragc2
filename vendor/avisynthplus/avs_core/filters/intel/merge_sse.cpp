// Avisynth v2.5.  Copyright 2002 Ben Rudiak-Gould et al.
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


// Avisynth filter: YUV merge / Swap planes
// by Klaus Post (kp@interact.dk)
// adapted by Richard Berg (avisynth-dev@richardberg.net)
// iSSE code by Ian Brabham


#include "../merge.h"
#include "merge_sse.h"
#include "merge_avx2.h"
#include "../core/internal.h"

// Intrinsics base header + really required extension headers
#if defined(_MSC_VER)
#include <intrin.h> // MSVC
#else 
#include <x86intrin.h> // GCC/MinGW/Clang/LLVM
#endif

#include "avs/alignment.h"
#include <stdint.h>


/* -----------------------------------
 *     weighted_merge_chroma_yuy2
 * -----------------------------------
 */
void weighted_merge_chroma_yuy2_sse2(BYTE *src, const BYTE *chroma, int pitch, int chroma_pitch,int width, int height, int weight, int invweight )
{
  __m128i round_mask = _mm_set1_epi32(0x4000);
  __m128i mask = _mm_set_epi16(weight, invweight, weight, invweight, weight, invweight, weight, invweight);
  __m128i luma_mask = _mm_set1_epi16(0x00FF);

  int wMod16 = (width/16) * 16;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod16; x += 16) {
      __m128i px1 = _mm_load_si128(reinterpret_cast<const __m128i*>(src+x));
      __m128i px2 = _mm_load_si128(reinterpret_cast<const __m128i*>(chroma+x));

      __m128i src_lo = _mm_unpacklo_epi16(px1, px2);
      __m128i src_hi = _mm_unpackhi_epi16(px1, px2);

      src_lo = _mm_srli_epi16(src_lo, 8);
      src_hi = _mm_srli_epi16(src_hi, 8);

      src_lo = _mm_madd_epi16(src_lo, mask);
      src_hi = _mm_madd_epi16(src_hi, mask);

      src_lo = _mm_add_epi32(src_lo, round_mask);
      src_hi = _mm_add_epi32(src_hi, round_mask);

      src_lo = _mm_srli_epi32(src_lo, 15);
      src_hi = _mm_srli_epi32(src_hi, 15);

      __m128i result_chroma = _mm_packs_epi32(src_lo, src_hi);
      result_chroma = _mm_slli_epi16(result_chroma, 8);

      __m128i result_luma = _mm_and_si128(px1, luma_mask);
      __m128i result = _mm_or_si128(result_chroma, result_luma);

      _mm_store_si128(reinterpret_cast<__m128i*>(src+x), result);
    }

    for (int x = wMod16; x < width; x+=2) {
      src[x+1] = (chroma[x+1] * weight + src[x+1] * invweight + 16384) >> 15;
    }

    src += pitch;
    chroma += chroma_pitch;
  }
}

#ifdef X86_32
void weighted_merge_chroma_yuy2_mmx(BYTE *src, const BYTE *chroma, int pitch, int chroma_pitch,int width, int height, int weight, int invweight )
{
  __m64 round_mask = _mm_set1_pi32(0x4000);
  __m64 mask = _mm_set_pi16(weight, invweight, weight, invweight);
  __m64 luma_mask = _mm_set1_pi16(0x00FF);

  int wMod8 = (width/8) * 8;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod8; x += 8) {
      __m64 px1 = *reinterpret_cast<const __m64*>(src+x); //V1 Y3 U1 Y2 V0 Y1 U0 Y0
      __m64 px2 = *reinterpret_cast<const __m64*>(chroma+x); //v1 y3 u1 y2 v0 y1 u0 y0

      __m64 src_lo = _mm_unpacklo_pi16(px1, px2); //v0 y1 V0 Y1 u0 y0 U0 Y0
      __m64 src_hi = _mm_unpackhi_pi16(px1, px2);

      src_lo = _mm_srli_pi16(src_lo, 8); //00 v0 00 V0 00 u0 00 U0
      src_hi = _mm_srli_pi16(src_hi, 8);

      src_lo = _mm_madd_pi16(src_lo, mask);
      src_hi = _mm_madd_pi16(src_hi, mask);

      src_lo = _mm_add_pi32(src_lo, round_mask);
      src_hi = _mm_add_pi32(src_hi, round_mask);

      src_lo = _mm_srli_pi32(src_lo, 15);
      src_hi = _mm_srli_pi32(src_hi, 15);

      __m64 result_chroma = _mm_packs_pi32(src_lo, src_hi);
      result_chroma = _mm_slli_pi16(result_chroma, 8);

      __m64 result_luma = _mm_and_si64(px1, luma_mask);
      __m64 result = _mm_or_si64(result_chroma, result_luma);

      *reinterpret_cast<__m64*>(src+x) = result;
    }

    for (int x = wMod8; x < width; x+=2) {
      src[x+1] = (chroma[x+1] * weight + src[x+1] * invweight + 16384) >> 15;
    }

    src += pitch;
    chroma += chroma_pitch;
  }
  _mm_empty();
}
#endif



/* -----------------------------------
 *      weighted_merge_luma_yuy2
 * -----------------------------------
 */
void weighted_merge_luma_yuy2_sse2(BYTE *src, const BYTE *luma, int pitch, int luma_pitch,int width, int height, int weight, int invweight)
{
  __m128i round_mask = _mm_set1_epi32(0x4000);
  __m128i mask = _mm_set_epi16(weight, invweight, weight, invweight, weight, invweight, weight, invweight);
  __m128i luma_mask = _mm_set1_epi16(0x00FF);
  __m128i chroma_mask = _mm_set1_epi16((short)0xFF00);

  int wMod16 = (width/16) * 16;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod16; x += 16) {
      __m128i px1 = _mm_load_si128(reinterpret_cast<const __m128i*>(src+x)); //V1 Y3 U1 Y2 V0 Y1 U0 Y0
      __m128i px2 = _mm_load_si128(reinterpret_cast<const __m128i*>(luma+x)); //v1 y3 u1 y2 v0 y1 u0 y0

      __m128i src_lo = _mm_unpacklo_epi16(px1, px2); //v0 y1 V0 Y1 u0 y0 U0 Y0
      __m128i src_hi = _mm_unpackhi_epi16(px1, px2);

      src_lo = _mm_and_si128(src_lo, luma_mask); //00 v0 00 V0 00 u0 00 U0
      src_hi = _mm_and_si128(src_hi, luma_mask);

      src_lo = _mm_madd_epi16(src_lo, mask);
      src_hi = _mm_madd_epi16(src_hi, mask);

      src_lo = _mm_add_epi32(src_lo, round_mask);
      src_hi = _mm_add_epi32(src_hi, round_mask);

      src_lo = _mm_srli_epi32(src_lo, 15);
      src_hi = _mm_srli_epi32(src_hi, 15);

      __m128i result_luma = _mm_packs_epi32(src_lo, src_hi);

      __m128i result_chroma = _mm_and_si128(px1, chroma_mask);
      __m128i result = _mm_or_si128(result_chroma, result_luma);

      _mm_store_si128(reinterpret_cast<__m128i*>(src+x), result);
    }

    for (int x = wMod16; x < width; x+=2) {
      src[x] = (luma[x] * weight + src[x] * invweight + 16384) >> 15;
    }

    src += pitch;
    luma += luma_pitch;
  }
}

#ifdef X86_32
void weighted_merge_luma_yuy2_mmx(BYTE *src, const BYTE *luma, int pitch, int luma_pitch,int width, int height, int weight, int invweight)
{
  __m64 round_mask = _mm_set1_pi32(0x4000);
  __m64 mask = _mm_set_pi16(weight, invweight, weight, invweight);
  __m64 luma_mask = _mm_set1_pi16(0x00FF);
  __m64 chroma_mask = _mm_set1_pi16((short)0xFF00);

  int wMod8 = (width/8) * 8;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod8; x += 8) {
      __m64 px1 = *reinterpret_cast<const __m64*>(src+x); //V1 Y3 U1 Y2 V0 Y1 U0 Y0
      __m64 px2 = *reinterpret_cast<const __m64*>(luma+x); //v1 y3 u1 y2 v0 y1 u0 y0

      __m64 src_lo = _mm_unpacklo_pi16(px1, px2); //v0 y1 V0 Y1 u0 y0 U0 Y0
      __m64 src_hi = _mm_unpackhi_pi16(px1, px2);

      src_lo = _mm_and_si64(src_lo, luma_mask); //00 v0 00 V0 00 u0 00 U0
      src_hi = _mm_and_si64(src_hi, luma_mask);

      src_lo = _mm_madd_pi16(src_lo, mask);
      src_hi = _mm_madd_pi16(src_hi, mask);

      src_lo = _mm_add_pi32(src_lo, round_mask);
      src_hi = _mm_add_pi32(src_hi, round_mask);

      src_lo = _mm_srli_pi32(src_lo, 15);
      src_hi = _mm_srli_pi32(src_hi, 15);

      __m64 result_luma = _mm_packs_pi32(src_lo, src_hi);

      __m64 result_chroma = _mm_and_si64(px1, chroma_mask);
      __m64 result = _mm_or_si64(result_chroma, result_luma);

      *reinterpret_cast<__m64*>(src+x) = result;
    }

    for (int x = wMod8; x < width; x+=2) {
      src[x] = (luma[x] * weight + src[x] * invweight + 16384) >> 15;
    }

    src += pitch;
    luma += luma_pitch;
  }
  _mm_empty();
}
#endif


/* -----------------------------------
 *          replace_luma_yuy2
 * -----------------------------------
 */
void replace_luma_yuy2_sse2(BYTE *src, const BYTE *luma, int pitch, int luma_pitch,int width, int height)
{
  int mod16_width = width / 16 * 16;
  __m128i luma_mask = _mm_set1_epi16(0x00FF);
  __m128i chroma_mask = _mm_set1_epi16((short)0xFF00);

  for(int y = 0; y < height; y++) {
    for(int x = 0; x < mod16_width; x+=16) {
      __m128i s = _mm_load_si128(reinterpret_cast<const __m128i*>(src+x));
      __m128i l = _mm_load_si128(reinterpret_cast<const __m128i*>(luma+x));

      __m128i s_chroma = _mm_and_si128(s, chroma_mask);
      __m128i l_luma = _mm_and_si128(l, luma_mask);

      __m128i result = _mm_or_si128(s_chroma, l_luma);

      _mm_store_si128(reinterpret_cast<__m128i*>(src+x), result);
    }

    for (int x = mod16_width; x < width; x+=2) {
      src[x] = luma[x];
    }
    src += pitch;
    luma += luma_pitch;
  }
}

#ifdef X86_32
void replace_luma_yuy2_mmx(BYTE *src, const BYTE *luma, int pitch, int luma_pitch,int width, int height)
{
  int mod8_width = width / 8 * 8;
  __m64 luma_mask = _mm_set1_pi16(0x00FF);
  __m64 chroma_mask = _mm_set1_pi16((short)0xFF00);

  for(int y = 0; y < height; y++) {
    for(int x = 0; x < mod8_width; x+=8) {
      __m64 s = *reinterpret_cast<const __m64*>(src+x);
      __m64 l = *reinterpret_cast<const __m64*>(luma+x);

      __m64 s_chroma = _mm_and_si64(s, chroma_mask);
      __m64 l_luma = _mm_and_si64(l, luma_mask);

      __m64 result = _mm_or_si64(s_chroma, l_luma);

      *reinterpret_cast<__m64*>(src+x) = result;
    }

    for (int x = mod8_width; x < width; x+=2) {
      src[x] = luma[x];
    }
    src += pitch;
    luma += luma_pitch;
  }
  _mm_empty();
}
#endif



/* -----------------------------------
 *            average_plane
 * -----------------------------------
 */
 // Can be called from Layer or Overlay, so no aligned loads/stores here and prepare for exact width
template<typename pixel_t>
void average_plane_sse2(BYTE *p1, const BYTE *p2, int p1_pitch, int p2_pitch, int rowsize, int height) {
  // width is RowSize here
  int mod16_width = rowsize / 16 * 16;

  for(int y = 0; y < height; y++) {
    for(int x = 0; x < mod16_width; x+=16) {
      __m128i src1  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p1+x));
      __m128i src2  = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p2+x));
      __m128i dst;
      if constexpr(sizeof(pixel_t) == 1)
        dst = _mm_avg_epu8(src1, src2); // 8 pixels
      else // pixel_size == 2
        dst = _mm_avg_epu16(src1, src2); // 4 pixels

      _mm_storeu_si128(reinterpret_cast<__m128i*>(p1+x), dst);
    }

    if (mod16_width != rowsize) {
      for (size_t x = mod16_width / sizeof(pixel_t); x < rowsize/sizeof(pixel_t); ++x) {
        reinterpret_cast<pixel_t *>(p1)[x] = (int(reinterpret_cast<pixel_t *>(p1)[x]) + reinterpret_cast<const pixel_t *>(p2)[x] + 1) >> 1;
      }
    }
    p1 += p1_pitch;
    p2 += p2_pitch;
  }
}

// instantiate
template void average_plane_sse2<uint8_t>(BYTE* p1, const BYTE* p2, int p1_pitch, int p2_pitch, int rowsize, int height);
template void average_plane_sse2<uint16_t>(BYTE* p1, const BYTE* p2, int p1_pitch, int p2_pitch, int rowsize, int height);

// weighted_merge_planar SSE2 implementations moved to overlay/intel/blend_common_sse.cpp
// (see weighted_merge_sse2 / weighted_merge_float_sse2)

void average_plane_sse2_float(BYTE *p1, const BYTE *p2, int p1_pitch, int p2_pitch, int rowsize, int height) {
  auto OneHalf = _mm_set1_ps(0.5f);

  int wMod16 = (rowsize / 16) * 16;

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wMod16; x += 16) {
      auto px1 = _mm_loadu_ps(reinterpret_cast<const float*>(p1 + x));
      auto px2 = _mm_loadu_ps(reinterpret_cast<const float*>(p2 + x));

      auto result = _mm_mul_ps(_mm_add_ps(px1, px2), OneHalf);

      _mm_storeu_ps(reinterpret_cast<float *>(p1 + x), result);
    }

    for (size_t x = wMod16 / sizeof(float); x < rowsize / sizeof(float); x++) {
      reinterpret_cast<float *>(p1)[x] = (reinterpret_cast<float *>(p1)[x] + reinterpret_cast<const float *>(p2)[x]) * 0.5f;
    }

    p1 += p1_pitch;
    p2 += p2_pitch;
  }
}

