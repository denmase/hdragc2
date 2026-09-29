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

// Overlay (c) 2003, 2004 by Klaus Post

#include "444convert_sse.h"
#include "../../../core/internal.h"

// Intrinsics base header + required extension headers
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#include <smmintrin.h> // SSE4.1

// fast in-place conversions from and to 4:4:4

/***** YV12 -> YUV 4:4:4   ******/

template<typename pixel_t>
static void convert_yv12_chroma_to_yv24_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int src_width, int src_height) {
  src_width *= sizeof(pixel_t);
  int mod8_width = src_width / 8 * 8;
  for (int y = 0; y < src_height; ++y) {
    for (int x = 0; x < mod8_width; x+=8) {
      // 0 0 0 0 0 0 0 0 U7 U6 U5 U4 U3 U2 U1 U0 for 8 bits
      // 0 0 0 0 U3 U2 U1 U0 for 16 bits
      __m128i src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp+x));
      if constexpr(sizeof(pixel_t) == 1)
        src = _mm_unpacklo_epi8(src, src); //U7 U7 U6 U6 U5 U5 U4 U4 U3 U3 U2 U2 U1 U1 U0 U0
      else
        src = _mm_unpacklo_epi16(src, src); //U3 U3 U2 U2 U1 U1 U0 U0

      _mm_store_si128(reinterpret_cast<__m128i*>(dstp+x*2), src);
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp+x*2 + dst_pitch), src);
    }

    if (mod8_width != src_width) {
      __m128i src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp+src_width - 8));
      if constexpr(sizeof(pixel_t) == 1)
        src = _mm_unpacklo_epi8(src, src);
      else
        src = _mm_unpacklo_epi16(src, src);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + (src_width * 2) - 16), src);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + (src_width * 2) - 16 + dst_pitch), src);
    }

    dstp += dst_pitch*2;
    srcp += src_pitch;
  }
}



/***** YV16 -> YUV 4:4:4   ******/

template<typename pixel_t>
static void convert_yv16_chroma_to_yv24_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int src_width, int src_height) {
  src_width *= sizeof(pixel_t);
  int mod8_width = src_width / 8 * 8;
  for (int y = 0; y < src_height; ++y) {
    for (int x = 0; x < mod8_width; x+=8) {
      __m128i src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp+x));
      if constexpr(sizeof(pixel_t) == 1)
        src = _mm_unpacklo_epi8(src, src);
      else
        src = _mm_unpacklo_epi16(src, src);

      _mm_store_si128(reinterpret_cast<__m128i*>(dstp+x*2), src);
    }

    if (mod8_width != src_width) {
      __m128i src = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp+src_width - 8));
      if constexpr(sizeof(pixel_t)==1)
        src = _mm_unpacklo_epi8(src, src);
      else
        src = _mm_unpacklo_epi16(src, src);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + (src_width * 2) - 16), src);
    }

    dstp += dst_pitch;
    srcp += src_pitch;
  }
}


/***** YV24 -> YV12 chroma downsamplers ******/

// YV24->YV12 float types: simple 2x2 averaging
static AVS_FORCEINLINE __m128 convert_yv24_chroma_block_to_yv12_float_sse2(const __m128 &src_line0_p0, const __m128 &src_line1_p0, const __m128 &src_line0_p1, const __m128 &src_line1_p1, const __m128 &onefourth) {
  __m128 avg1f = _mm_add_ps(src_line0_p0, src_line1_p0);
  __m128 avg2f = _mm_add_ps(src_line0_p1, src_line1_p1);
  avg1f = _mm_add_ps(avg1f, _mm_castsi128_ps(_mm_srli_epi64(_mm_castps_si128(avg1f), 32)));
  avg2f = _mm_add_ps(avg2f, _mm_castsi128_ps(_mm_srli_epi64(_mm_castps_si128(avg2f), 32)));
  return _mm_mul_ps(_mm_shuffle_ps(avg1f, avg2f, _MM_SHUFFLE(2, 0, 2, 0)), onefourth);
}

void convert_yv24_chroma_to_yv12_float_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, const int dst_height) {
  const __m128 onefourth = _mm_set1_ps(0.25f);

  for (int y = 0; y < dst_height; ++y) {
    int x = 0;
    for (; x + 16 <= dst_width; x += 16) {
      __m128 p0 = _mm_loadu_ps(reinterpret_cast<const float*>(srcp + x * 2));
      __m128 p1 = _mm_loadu_ps(reinterpret_cast<const float*>(srcp + x * 2 + 16));
      __m128 q0 = _mm_loadu_ps(reinterpret_cast<const float*>(srcp + x * 2 + src_pitch));
      __m128 q1 = _mm_loadu_ps(reinterpret_cast<const float*>(srcp + x * 2 + src_pitch + 16));
      _mm_storeu_ps(reinterpret_cast<float*>(dstp + x),
        convert_yv24_chroma_block_to_yv12_float_sse2(p0, q0, p1, q1, onefourth));
    }
    // scalar tail (0-3 remaining float pixels)
    const float *s0 = reinterpret_cast<const float*>(srcp);
    const float *s1 = reinterpret_cast<const float*>(srcp + src_pitch);
    float *d = reinterpret_cast<float*>(dstp);
    for (int px = x / 4; px < dst_width / 4; ++px)
      d[px] = (s0[px * 2] + s0[px * 2 + 1] + s1[px * 2] + s1[px * 2 + 1]) * 0.25f;

    dstp += dst_pitch;
    srcp += src_pitch * 2;
  }
}

// YV24->YV12 uint8_t: exact (a+b+c+d+2)>>2 using SSSE3 pmaddubsw.
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
void convert_yv24_chroma_to_yv12_u8_ssse3(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, const int dst_height)
{
  const __m128i ones = _mm_set1_epi8(1);
  const __m128i two  = _mm_set1_epi16(2);

  for (int y = 0; y < dst_height; ++y) {
    int x = 0;
    for (; x + 16 <= dst_width; x += 16) {
      __m128i row0_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2));
      __m128i row0_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + 16));
      __m128i row1_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch));
      __m128i row1_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch + 16));

      __m128i hsum0_lo = _mm_maddubs_epi16(row0_lo, ones);
      __m128i hsum0_hi = _mm_maddubs_epi16(row0_hi, ones);
      __m128i hsum1_lo = _mm_maddubs_epi16(row1_lo, ones);
      __m128i hsum1_hi = _mm_maddubs_epi16(row1_hi, ones);

      __m128i sum_lo = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(hsum0_lo, hsum1_lo), two), 2);
      __m128i sum_hi = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(hsum0_hi, hsum1_hi), two), 2);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + x), _mm_packus_epi16(sum_lo, sum_hi));
    }
    // scalar tail (0-15 remaining u8 pixels)
    for (; x < dst_width; ++x)
      dstp[x] = (srcp[x*2] + srcp[x*2+1] + srcp[x*2+src_pitch] + srcp[x*2+src_pitch+1] + 2) >> 2;

    dstp += dst_pitch;
    srcp += src_pitch * 2;
  }
}

// YV24->YV12 uint16_t: exact (a+b+c+d+2)>>2.
// lessthan16bit=true:  values <= 16383; madd_epi16 signed interpretation is correct.
// lessthan16bit=false: XOR 0x8000 pivot before madd maps u16->i16 [-32768,32767];
//   sum becomes (v0+v1+v2+v3 - 4*32768 + 2); srai gives result-32768; XOR 0x8000 restores u16.
template<bool lessthan16bit>
static void convert_yv24_chroma_to_yv12_u16_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, const int dst_height)
{
  const __m128i ones     = _mm_set1_epi16(1);
  const __m128i two      = _mm_set1_epi32(2);
  const __m128i xor_sign = _mm_set1_epi16((short)0x8000);

  for (int y = 0; y < dst_height; ++y) {
    int x = 0;
    for (; x + 16 <= dst_width; x += 16) {
      __m128i row0_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2));
      __m128i row0_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + 16));
      __m128i row1_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch));
      __m128i row1_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch + 16));

      if constexpr (!lessthan16bit) {
        row0_lo = _mm_xor_si128(row0_lo, xor_sign);
        row0_hi = _mm_xor_si128(row0_hi, xor_sign);
        row1_lo = _mm_xor_si128(row1_lo, xor_sign);
        row1_hi = _mm_xor_si128(row1_hi, xor_sign);
      }

      __m128i hsum0_lo = _mm_madd_epi16(row0_lo, ones);
      __m128i hsum0_hi = _mm_madd_epi16(row0_hi, ones);
      __m128i hsum1_lo = _mm_madd_epi16(row1_lo, ones);
      __m128i hsum1_hi = _mm_madd_epi16(row1_hi, ones);

      __m128i sum_lo = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(hsum0_lo, hsum1_lo), two), 2);
      __m128i sum_hi = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(hsum0_hi, hsum1_hi), two), 2);

      __m128i result = _mm_packs_epi32(sum_lo, sum_hi);
      if constexpr (!lessthan16bit)
        result = _mm_xor_si128(result, xor_sign);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + x), result);
    }
    // scalar tail (0-7 remaining u16 pixels)
    const uint16_t *s0 = reinterpret_cast<const uint16_t*>(srcp);
    const uint16_t *s1 = reinterpret_cast<const uint16_t*>(srcp + src_pitch);
    uint16_t *d = reinterpret_cast<uint16_t*>(dstp);
    for (int px = x / 2; px < dst_width / 2; ++px)
      d[px] = (uint16_t)((s0[px*2] + s0[px*2+1] + s1[px*2] + s1[px*2+1] + 2) >> 2);

    dstp += dst_pitch;
    srcp += src_pitch * 2;
  }
}

#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif
void convert_yv24_chroma_to_yv12_u16_sse41(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, const int dst_height)
{
  // XOR 0x8000 pivot: u16→i16 before madd_epi16 (signed); srai+packs+XOR restores u16
  const __m128i xor_sign = _mm_set1_epi16((short)0x8000);
  const __m128i ones     = _mm_set1_epi16(1);
  const __m128i two      = _mm_set1_epi32(2);

  for (int y = 0; y < dst_height; ++y) {
    int x = 0;
    for (; x + 16 <= dst_width; x += 16) {
      __m128i row0_lo = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2)),            xor_sign);
      __m128i row0_hi = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + 16)),       xor_sign);
      __m128i row1_lo = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch)),      xor_sign);
      __m128i row1_hi = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch + 16)), xor_sign);

      __m128i hsum0_lo = _mm_madd_epi16(row0_lo, ones);
      __m128i hsum0_hi = _mm_madd_epi16(row0_hi, ones);
      __m128i hsum1_lo = _mm_madd_epi16(row1_lo, ones);
      __m128i hsum1_hi = _mm_madd_epi16(row1_hi, ones);

      __m128i sum_lo = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(hsum0_lo, hsum1_lo), two), 2);
      __m128i sum_hi = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(hsum0_hi, hsum1_hi), two), 2);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + x),
        _mm_xor_si128(_mm_packs_epi32(sum_lo, sum_hi), xor_sign));
    }
    // scalar tail (0-7 remaining u16 pixels)
    const uint16_t *s0 = reinterpret_cast<const uint16_t*>(srcp);
    const uint16_t *s1 = reinterpret_cast<const uint16_t*>(srcp + src_pitch);
    uint16_t *d = reinterpret_cast<uint16_t*>(dstp);
    for (int px = x / 2; px < dst_width / 2; ++px)
      d[px] = (uint16_t)((s0[px*2] + s0[px*2+1] + s1[px*2] + s1[px*2+1] + 2) >> 2);

    dstp += dst_pitch;
    srcp += src_pitch * 2;
  }
}

// YV24->YV12 uint16_t lessthan16bit fast path: pure u16 arithmetic via hadd_epi16 (SSSE3).
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
void convert_yv24_chroma_to_yv12_u16_lessthan16bit_ssse3(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, const int dst_height)
{
  const __m128i two = _mm_set1_epi16(2);

  for (int y = 0; y < dst_height; ++y) {
    int x = 0;
    for (; x + 16 <= dst_width; x += 16) {
      __m128i row0_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2));
      __m128i row0_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + 16));
      __m128i row1_lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch));
      __m128i row1_hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch + 16));

      __m128i vert_lo = _mm_add_epi16(row0_lo, row1_lo);
      __m128i vert_hi = _mm_add_epi16(row0_hi, row1_hi);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + x),
        _mm_srli_epi16(_mm_add_epi16(_mm_hadd_epi16(vert_lo, vert_hi), two), 2));
    }
    // scalar tail (0-7 remaining u16 pixels)
    const uint16_t *s0 = reinterpret_cast<const uint16_t*>(srcp);
    const uint16_t *s1 = reinterpret_cast<const uint16_t*>(srcp + src_pitch);
    uint16_t *d = reinterpret_cast<uint16_t*>(dstp);
    for (int px = x / 2; px < dst_width / 2; ++px)
      d[px] = (uint16_t)((s0[px*2] + s0[px*2+1] + s1[px*2] + s1[px*2+1] + 2) >> 2);

    dstp += dst_pitch;
    srcp += src_pitch * 2;
  }
}


/***** YV24 -> YV16 chroma downsamplers ******/

static AVS_FORCEINLINE __m128 convert_yv24_chroma_block_to_yv16_float_sse2(const __m128 &src_line0_p0, const __m128 &src_line0_p1, const __m128 &half) {
  __m128 avg1 = src_line0_p0;
  __m128 avg2 = src_line0_p1;
  avg1 = _mm_add_ps(avg1, _mm_castsi128_ps(_mm_srli_epi64(_mm_castps_si128(avg1), 32)));
  avg2 = _mm_add_ps(avg2, _mm_castsi128_ps(_mm_srli_epi64(_mm_castps_si128(avg2), 32)));
  return _mm_mul_ps(_mm_shuffle_ps(avg1, avg2, _MM_SHUFFLE(2, 0, 2, 0)), half);
}

// Requires dst_width >= 16 and 16-byte aligned src/dst (checked by caller)
void convert_yv24_chroma_to_yv16_float_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, const int dst_height) {
  int mod16_width = dst_width / 16 * 16;
  const __m128 half = _mm_set1_ps(0.5f);

  for (int y = 0; y < dst_height; ++y) {
    for (int x = 0; x < mod16_width; x += 16) {
      __m128 src_line0_p0 = _mm_load_ps(reinterpret_cast<const float*>(srcp + x * 2));
      __m128 src_line0_p1 = _mm_load_ps(reinterpret_cast<const float*>(srcp + x * 2 + 16));

      __m128 avg = convert_yv24_chroma_block_to_yv16_float_sse2(src_line0_p0, src_line0_p1, half);

      _mm_store_ps(reinterpret_cast<float*>(dstp + x), avg);
    }

    if (mod16_width != dst_width) {
      __m128 src_line0_p0 = _mm_loadu_ps(reinterpret_cast<const float*>(srcp + dst_width * 2 - 32));
      __m128 src_line0_p1 = _mm_loadu_ps(reinterpret_cast<const float*>(srcp + dst_width * 2 - 16));

      __m128 avg = convert_yv24_chroma_block_to_yv16_float_sse2(src_line0_p0, src_line0_p1, half);

      _mm_storeu_ps(reinterpret_cast<float*>(dstp + dst_width - 16), avg);
    }

    dstp += dst_pitch;
    srcp += src_pitch;
  }
}

// uint8_t, uint16_t
template<typename pixel_t>
static AVS_FORCEINLINE __m128i convert_yv24_chroma_block_to_yv16_sse2(const __m128i &src_line0_p0, const __m128i &src_line0_p1, const __m128i &mask) {
  __m128i avg1, avg2;

  if constexpr(sizeof(pixel_t) == 1) {
    __m128i avg1_sh = _mm_srli_epi16(src_line0_p0, 8);
    __m128i avg2_sh = _mm_srli_epi16(src_line0_p1, 8);

    avg1 = _mm_avg_epu8(src_line0_p0, avg1_sh);
    avg2 = _mm_avg_epu8(src_line0_p1, avg2_sh);
  }
  else {
    __m128i avg1_sh = _mm_srli_epi32(src_line0_p0, 16);
    __m128i avg2_sh = _mm_srli_epi32(src_line0_p1, 16);

    avg1 = _mm_avg_epu16(src_line0_p0, avg1_sh);
    avg2 = _mm_avg_epu16(src_line0_p1, avg2_sh);
  }

  avg1 = _mm_and_si128(avg1, mask);
  avg2 = _mm_and_si128(avg2, mask);

  __m128i packed;
  if constexpr(sizeof(pixel_t) == 1)
    packed = _mm_packus_epi16(avg1, avg2);
  else
    packed = _MM_PACKUS_EPI32(avg1, avg2); // SSE4.1 simul for SSE2
  return packed;
}

template<typename pixel_t>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif
static AVS_FORCEINLINE __m128i convert_yv24_chroma_block_to_yv16_sse41(const __m128i &src_line0_p0, const __m128i &src_line0_p1, const __m128i &mask)
{
  __m128i avg1, avg2;

  if constexpr (sizeof(pixel_t) == 1) {
    __m128i avg1_sh = _mm_srli_epi16(src_line0_p0, 8);
    __m128i avg2_sh = _mm_srli_epi16(src_line0_p1, 8);

    avg1 = _mm_avg_epu8(src_line0_p0, avg1_sh);
    avg2 = _mm_avg_epu8(src_line0_p1, avg2_sh);
  }
  else {
    __m128i avg1_sh = _mm_srli_epi32(src_line0_p0, 16);
    __m128i avg2_sh = _mm_srli_epi32(src_line0_p1, 16);

    avg1 = _mm_avg_epu16(src_line0_p0, avg1_sh);
    avg2 = _mm_avg_epu16(src_line0_p1, avg2_sh);
  }

  avg1 = _mm_and_si128(avg1, mask);
  avg2 = _mm_and_si128(avg2, mask);

  __m128i packed;
  if constexpr (sizeof(pixel_t) == 1)
    packed = _mm_packus_epi16(avg1, avg2);
  else
    packed = _mm_packus_epi32(avg1, avg2); // SSE4.1
  return packed;
}

// Requires 16-byte aligned src/dst (checked by caller); dst_width in bytes
template<typename pixel_t>
static void convert_yv24_chroma_to_yv16_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, const int dst_height) {
  int mod16_width = dst_width / 16 * 16;

  __m128i mask;
  if constexpr(sizeof(pixel_t) == 1)
    mask = _mm_set1_epi16(0x00FF);
  else
    mask = _mm_set1_epi32(0x0000FFFF);

  for (int y = 0; y < dst_height; ++y) {
    for (int x = 0; x < mod16_width; x+=16) {
      __m128i src_line0_p0 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*2));
      __m128i src_line0_p1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*2+16));

      __m128i avg = convert_yv24_chroma_block_to_yv16_sse2<pixel_t>(src_line0_p0, src_line0_p1, mask);

      _mm_store_si128(reinterpret_cast<__m128i*>(dstp+x), avg);
    }

    if (mod16_width != dst_width) {
      __m128i src_line0_p0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp+dst_width*2-32));
      __m128i src_line0_p1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp+dst_width*2-16));

      __m128i avg = convert_yv24_chroma_block_to_yv16_sse2<pixel_t>(src_line0_p0, src_line0_p1, mask);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp+dst_width-16), avg);
    }

    dstp += dst_pitch;
    srcp += src_pitch;
  }
}

template<typename pixel_t>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("sse4.1")))
#endif
static void convert_yv24_chroma_to_yv16_sse41(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, const int dst_height)
{
  int mod16_width = dst_width / 16 * 16;

  __m128i mask;
  if constexpr (sizeof(pixel_t) == 1)
    mask = _mm_set1_epi16(0x00FF);
  else
    mask = _mm_set1_epi32(0x0000FFFF);

  for (int y = 0; y < dst_height; ++y) {
    for (int x = 0; x < mod16_width; x += 16) {
      __m128i src_line0_p0 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 2));
      __m128i src_line0_p1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + 16));

      __m128i avg = convert_yv24_chroma_block_to_yv16_sse41<pixel_t>(src_line0_p0, src_line0_p1, mask);

      _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x), avg);
    }

    if (mod16_width != dst_width) {
      __m128i src_line0_p0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + dst_width * 2 - 32));
      __m128i src_line0_p1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + dst_width * 2 - 16));

      __m128i avg = convert_yv24_chroma_block_to_yv16_sse41<pixel_t>(src_line0_p0, src_line0_p1, mask);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + dst_width - 16), avg);
    }

    dstp += dst_pitch;
    srcp += src_pitch;
  }
}


/***** Typed non-template wrapper functions (called from 444convert.cpp dispatch) *****/

void conv_yv12_to_yv24_chroma_u8_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int src_width, int src_height) {
  convert_yv12_chroma_to_yv24_sse2<uint8_t>(dstp, srcp, dst_pitch, src_pitch, src_width, src_height);
}
void conv_yv12_to_yv24_chroma_u16_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int src_width, int src_height) {
  convert_yv12_chroma_to_yv24_sse2<uint16_t>(dstp, srcp, dst_pitch, src_pitch, src_width, src_height);
}

void conv_yv16_to_yv24_chroma_u8_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int src_width, int src_height) {
  convert_yv16_chroma_to_yv24_sse2<uint8_t>(dstp, srcp, dst_pitch, src_pitch, src_width, src_height);
}
void conv_yv16_to_yv24_chroma_u16_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int src_width, int src_height) {
  convert_yv16_chroma_to_yv24_sse2<uint16_t>(dstp, srcp, dst_pitch, src_pitch, src_width, src_height);
}

void conv_yv24_to_yv12_chroma_u16_lessthan16bit_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height) {
  convert_yv24_chroma_to_yv12_u16_sse2<true>(dstp, srcp, dst_pitch, src_pitch, dst_width, dst_height);
}
void conv_yv24_to_yv12_chroma_u16_true16bit_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height) {
  convert_yv24_chroma_to_yv12_u16_sse2<false>(dstp, srcp, dst_pitch, src_pitch, dst_width, dst_height);
}

void conv_yv24_to_yv16_chroma_u8_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height) {
  convert_yv24_chroma_to_yv16_sse2<uint8_t>(dstp, srcp, dst_pitch, src_pitch, dst_width, dst_height);
}
void conv_yv24_to_yv16_chroma_u16_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height) {
  convert_yv24_chroma_to_yv16_sse2<uint16_t>(dstp, srcp, dst_pitch, src_pitch, dst_width, dst_height);
}
void conv_yv24_to_yv16_chroma_u8_sse41(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height) {
  convert_yv24_chroma_to_yv16_sse41<uint8_t>(dstp, srcp, dst_pitch, src_pitch, dst_width, dst_height);
}
void conv_yv24_to_yv16_chroma_u16_sse41(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height) {
  convert_yv24_chroma_to_yv16_sse41<uint16_t>(dstp, srcp, dst_pitch, src_pitch, dst_width, dst_height);
}
