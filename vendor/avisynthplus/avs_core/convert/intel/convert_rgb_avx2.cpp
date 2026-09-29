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

#include <avs/alignment.h>
#ifdef _MSC_VER
    #include <intrin.h>
#else
    #include <x86intrin.h>
#endif
#include <immintrin.h>

#ifndef _mm256_set_m128i
#define _mm256_set_m128i(v0, v1) _mm256_insertf128_si256(_mm256_castsi128_si256(v1), (v0), 1)
#endif

#ifndef _mm256_set_m128
#define _mm256_set_m128(v0, v1) _mm256_insertf128_ps(_mm256_castps128_ps256(v1), (v0), 1)
#endif

#include "convert_rgb_avx2.h"

// minimum width: 48*2 bytes
template<typename pixel_t, bool targetHasAlpha>
void convert_rgb_to_rgbp_avx2(const BYTE *srcp, BYTE * (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height)
{
  // RGB24: 2x3x16 bytes cycle, 2x16*(RGB) 8bit pixels
  // RGB48: 2x3x16 bytes cycle, 2x8*(RGB) 16bit pixels
  // 0123456789ABCDEF 0123456789ABCDEF 0123456789ABCDEF
  // BGRBGRBGRBGRBGRB GRBGRBGRBGRBGRBG RBGRBGRBGRBGRBGR // 8 bit
  // B G R B G R B G  R B G R B G R B  G R B G R B G R  // 16 bit
  // 1111111111112222 2222222233333333 3333444444444444

  constexpr int pixels_at_a_time = (sizeof(pixel_t) == 1) ? 32 : 16;
  const int wmod = (width / pixels_at_a_time) * pixels_at_a_time; // 8 pixels for 8 bit, 4 pixels for 16 bit
  __m256i mask;
  if constexpr(sizeof(pixel_t) == 1)
    mask = _mm256_set_epi8(15, 14, 13, 12, 11, 8, 5, 2, 10, 7, 4, 1, 9, 6, 3, 0,
      15, 14, 13, 12, 11, 8, 5, 2, 10, 7, 4, 1, 9, 6, 3, 0); // same for both lanes
  else
    mask = _mm256_set_epi8(15, 14, 13, 12, 11, 10, 5, 4, 9, 8, 3, 2, 7, 6, 1, 0,
      15, 14, 13, 12, 11, 10, 5, 4, 9, 8, 3, 2, 7, 6, 1, 0); // same for both lanes

  __m256i max_pixel_value;
  if constexpr(sizeof(pixel_t) == 1)
    max_pixel_value = _mm256_set1_epi8((char)0xFF);
  else
    max_pixel_value = _mm256_set1_epi16((short)0xFFFF); // bits_per_pixel is 16

// read-optimized
#define SRC_ADDRESS_ADVANCES
#ifdef SRC_ADDRESS_ADVANCES
  srcp -= src_pitch * (height - 1); // source packed RGB is upside down
  dstp[0] += dst_pitch[0] * (height - 1);
  dstp[1] += dst_pitch[1] * (height - 1);
  dstp[2] += dst_pitch[2] * (height - 1);
  if (targetHasAlpha)
    dstp[3] += dst_pitch[3] * (height - 1);
#endif

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < wmod; x += pixels_at_a_time) {
      auto BGRA_1_Lo48 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time));
      auto BGRA_2_Lo48 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time + 16));
      auto BGRA_3_Lo48 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time + 32));

      auto BGRA_1_Hi48 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time + 0 + 48));
      auto BGRA_2_Hi48 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time + 16 + 48));
      auto BGRA_3_Hi48 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time + 32 + 48));

      auto BGRA_1 = _mm256_set_m128i(BGRA_1_Hi48, BGRA_1_Lo48);
      auto BGRA_2 = _mm256_set_m128i(BGRA_2_Hi48, BGRA_2_Lo48);
      auto BGRA_3 = _mm256_set_m128i(BGRA_3_Hi48, BGRA_3_Lo48);

      auto pack_lo = _mm256_shuffle_epi8(BGRA_1, mask); // 111111111111: BBBBGGGGRRRR and rest: BGRB | BBGGRR and rest: BBGG
      BGRA_1 = _mm256_alignr_epi8(BGRA_2, BGRA_1, 12);
      auto pack_hi = _mm256_shuffle_epi8(BGRA_1, mask); // 222222222222: BBBBGGGGRRRR | BBGGRR
      BGRA_2 = _mm256_alignr_epi8(BGRA_3, BGRA_2, 8);
      auto pack_lo2 = _mm256_shuffle_epi8(BGRA_2, mask); // 333333333333: BBBBGGGGRRRR | BBGGRR
      BGRA_3 = _mm256_srli_si256(BGRA_3, 4); // to use the same mask
      auto pack_hi2 = _mm256_shuffle_epi8(BGRA_3, mask); // 444444444444: BBBBGGGGRRRR | BBGGRR

      auto BG1 = _mm256_unpacklo_epi32(pack_lo, pack_hi);  // BBBB_lo BBBB_hi GGGG_lo GGGG_hi
      auto BG2 = _mm256_unpacklo_epi32(pack_lo2, pack_hi2);  // BBBB_lo BBBB_hi GGGG_lo GGGG_hi
      auto RA1 = _mm256_unpackhi_epi32(pack_lo, pack_hi);   // RRRR_lo RRRR_hi AAAA_lo AAAA_hi
      auto RA2 = _mm256_unpackhi_epi32(pack_lo2, pack_hi2);  // RRRR_lo RRRR_hi AAAA_lo AAAA_hi
      auto B = _mm256_unpacklo_epi64(BG1, BG2);
      _mm256_stream_si256(reinterpret_cast<__m256i *>(dstp[1] + x * sizeof(pixel_t)), B); // B
      auto G = _mm256_unpackhi_epi64(BG1, BG2);
      _mm256_stream_si256(reinterpret_cast<__m256i *>(dstp[0] + x * sizeof(pixel_t)), G); // G
      auto R = _mm256_unpacklo_epi64(RA1, RA2);
      _mm256_stream_si256(reinterpret_cast<__m256i *>(dstp[2] + x * sizeof(pixel_t)), R); // R
      if (targetHasAlpha)
        _mm256_stream_si256(reinterpret_cast<__m256i *>(dstp[3] + x * sizeof(pixel_t)), max_pixel_value); // A
    }
    // rest, unaligned but simd
    if (wmod != width) {
      size_t x = (width - pixels_at_a_time);
      auto BGRA_1_Lo48 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time));
      auto BGRA_2_Lo48 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time + 16));
      auto BGRA_3_Lo48 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time + 32));

      auto BGRA_1_Hi48 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time + 0 + 48));
      auto BGRA_2_Hi48 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time + 16 + 48));
      auto BGRA_3_Hi48 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 * 48 / pixels_at_a_time + 32 + 48));

      auto BGRA_1 = _mm256_set_m128i(BGRA_1_Hi48, BGRA_1_Lo48);
      auto BGRA_2 = _mm256_set_m128i(BGRA_2_Hi48, BGRA_2_Lo48);
      auto BGRA_3 = _mm256_set_m128i(BGRA_3_Hi48, BGRA_3_Lo48);

      auto pack_lo = _mm256_shuffle_epi8(BGRA_1, mask); // 111111111111: BBBBGGGGRRRR and rest: BGRB | BBGGRR and rest: BBGG
      BGRA_1 = _mm256_alignr_epi8(BGRA_2, BGRA_1, 12);
      auto pack_hi = _mm256_shuffle_epi8(BGRA_1, mask); // 222222222222: BBBBGGGGRRRR | BBGGRR
      BGRA_2 = _mm256_alignr_epi8(BGRA_3, BGRA_2, 8);
      auto pack_lo2 = _mm256_shuffle_epi8(BGRA_2, mask); // 333333333333: BBBBGGGGRRRR | BBGGRR
      BGRA_3 = _mm256_srli_si256(BGRA_3, 4); // to use the same mask
      auto pack_hi2 = _mm256_shuffle_epi8(BGRA_3, mask); // 444444444444: BBBBGGGGRRRR | BBGGRR

      auto BG1 = _mm256_unpacklo_epi32(pack_lo, pack_hi);  // BBBB_lo BBBB_hi GGGG_lo GGGG_hi
      auto BG2 = _mm256_unpacklo_epi32(pack_lo2, pack_hi2);  // BBBB_lo BBBB_hi GGGG_lo GGGG_hi
      auto RA1 = _mm256_unpackhi_epi32(pack_lo, pack_hi);   // RRRR_lo RRRR_hi AAAA_lo AAAA_hi
      auto RA2 = _mm256_unpackhi_epi32(pack_lo2, pack_hi2);  // RRRR_lo RRRR_hi AAAA_lo AAAA_hi
      auto B = _mm256_unpacklo_epi64(BG1, BG2);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp[1] + x * sizeof(pixel_t)), B); // B
      auto G = _mm256_unpackhi_epi64(BG1, BG2);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp[0] + x * sizeof(pixel_t)), G); // G
      auto R = _mm256_unpacklo_epi64(RA1, RA2);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp[2] + x * sizeof(pixel_t)), R); // R
      if (targetHasAlpha)
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp[3] + x * sizeof(pixel_t)), max_pixel_value); // A
    }
#ifdef SRC_ADDRESS_ADVANCES
    srcp += src_pitch; // source packed RGB is upside down
    dstp[0] -= dst_pitch[0];
    dstp[1] -= dst_pitch[1];
    dstp[2] -= dst_pitch[2];
    if (targetHasAlpha)
      dstp[3] -= dst_pitch[3];
#else
    srcp -= src_pitch; // source packed RGB is upside down
    dstp[0] += dst_pitch[0];
    dstp[1] += dst_pitch[1];
    dstp[2] += dst_pitch[2];
    if (targetHasAlpha)
      dstp[3] += dst_pitch[3];
#endif
  }
#undef SRC_ADDRESS_ADVANCES
}

// Instantiate them
template void convert_rgb_to_rgbp_avx2<uint8_t, false>(const BYTE *srcp, BYTE * (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgb_to_rgbp_avx2<uint8_t, true>(const BYTE *srcp, BYTE * (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgb_to_rgbp_avx2<uint16_t, false>(const BYTE *srcp, BYTE * (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgb_to_rgbp_avx2<uint16_t, true>(const BYTE *srcp, BYTE * (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);

template<typename pixel_t, bool targetHasAlpha>
void convert_rgba_to_rgbp_avx2(const BYTE* srcp, BYTE* (&dstp)[4],
  int src_pitch, int(&dst_pitch)[4], int width, int height)
{
  const int pixels_per_iter = (sizeof(pixel_t) == 1) ? 16 : 8;
  // 8-bit: process 16 pixels per loop (64 bytes in -> four 16-byte stores out)
  // 16-bit: process 8 pixels per loop (64 bytes in -> four 16-byte stores out)
  __m128i mask128;
  if constexpr (sizeof(pixel_t) == 1)
    mask128 = _mm_set_epi8(15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
  else
    mask128 = _mm_set_epi8(15, 14, 7, 6, 13, 12, 5, 4, 11, 10, 3, 2, 9, 8, 1, 0);
  __m256i vmask = _mm256_broadcastsi128_si256(mask128);
  // Avisynth's scanline alignment is 64 bytes, so no remainder hanfling is needed for 16 RGB32 or 8 RGB64 pixels
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; x += pixels_per_iter) {
      __m256i srcA = _mm256_load_si256(reinterpret_cast<const __m256i*>(srcp + x * 4 * sizeof(pixel_t)));
      __m256i srcB = _mm256_load_si256(reinterpret_cast<const __m256i*>(srcp + (x + pixels_per_iter / 2) * 4 * sizeof(pixel_t)));

      __m256i shufA = _mm256_shuffle_epi8(srcA, vmask);
      __m256i shufB = _mm256_shuffle_epi8(srcB, vmask);

      __m128i a_lo = _mm256_castsi256_si128(shufA);
      __m128i a_hi = _mm256_extracti128_si256(shufA, 1);
      __m128i b_lo = _mm256_castsi256_si128(shufB);
      __m128i b_hi = _mm256_extracti128_si256(shufB, 1);

      __m128i bg_0 = _mm_unpacklo_epi32(a_lo, a_hi);
      __m128i ra_0 = _mm_unpackhi_epi32(a_lo, a_hi);
      __m128i bg_1 = _mm_unpacklo_epi32(b_lo, b_hi);
      __m128i ra_1 = _mm_unpackhi_epi32(b_lo, b_hi);

      __m128i chB = _mm_unpacklo_epi64(bg_0, bg_1);
      __m128i chG = _mm_unpackhi_epi64(bg_0, bg_1);
      __m128i chR = _mm_unpacklo_epi64(ra_0, ra_1);
      __m128i chA = _mm_unpackhi_epi64(ra_0, ra_1);

      _mm_store_si128(reinterpret_cast<__m128i*>(dstp[1] + x * sizeof(pixel_t)), chB);
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp[0] + x * sizeof(pixel_t)), chG);
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp[2] + x * sizeof(pixel_t)), chR);
      if constexpr (targetHasAlpha)
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp[3] + x * sizeof(pixel_t)), chA);
    }

    srcp -= src_pitch; // source packed RGB is upside down
    dstp[0] += dst_pitch[0];
    dstp[1] += dst_pitch[1];
    dstp[2] += dst_pitch[2];
    if constexpr (targetHasAlpha)
      dstp[3] += dst_pitch[3];
  }
}

// Instantiate them
template void convert_rgba_to_rgbp_avx2<uint8_t, false>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgba_to_rgbp_avx2<uint8_t, true>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgba_to_rgbp_avx2<uint16_t, false>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgba_to_rgbp_avx2<uint16_t, true>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);

// Planar RGB(A) → packed RGBA32/RGBA64  (reverse of convert_rgba_to_rgbp_avx2)
//
// Main loop: 128 bytes written per iteration
//   uint8_t:  32 pixels → 4 × _mm256_store_si256
//   uint16_t: 16 pixels → 4 × _mm256_store_si256
// Tail: 64 bytes (one half-iteration, uses SSE2 128-bit stores)
//   uint8_t:  16 pixels → 4 × _mm_store_si128
//   uint16_t:  8 pixels → 4 × _mm_store_si128
//
// Scanline alignment is 64 bytes (Avisynth guarantee), so aligned SIMD
// loads/stores are safe. The tail is handled fully in SIMD, potentially
// overreading/overwriting into padded bytes (same strategy as SSE2 path).

template<typename pixel_t, bool hasSrcAlpha>
void convert_rgbp_to_rgba_avx2(const BYTE* (&srcp)[4], BYTE* dstp, int(&src_pitch)[4], int dst_pitch, int width, int height)
{
  // big: pixels processed per 128-byte iteration; small: 64-byte tail size
  constexpr int big_pixels   = (sizeof(pixel_t) == 1) ? 32 : 16;
  constexpr int small_pixels = big_pixels / 2;  // 16 (u8) or 8 (u16)
  const int wmod = (width / big_pixels) * big_pixels;

  const __m256i transparent256 = _mm256_set1_epi8((char)0xFF);
  const __m128i transparent128 = _mm_set1_epi8((char)0xFF);

  for (int y = 0; y < height; y++) {
    // main loop: 128 bytes output per iteration
    for (int x = 0; x < wmod; x += big_pixels) {
      __m256i G = _mm256_load_si256(reinterpret_cast<const __m256i*>(srcp[0] + x * sizeof(pixel_t)));
      __m256i B = _mm256_load_si256(reinterpret_cast<const __m256i*>(srcp[1] + x * sizeof(pixel_t)));
      __m256i R = _mm256_load_si256(reinterpret_cast<const __m256i*>(srcp[2] + x * sizeof(pixel_t)));
      __m256i A;
      if constexpr (hasSrcAlpha)
        A = _mm256_load_si256(reinterpret_cast<const __m256i*>(srcp[3] + x * sizeof(pixel_t)));
      else
        A = transparent256;

      __m256i u0, u1, u2, u3;
      if constexpr (sizeof(pixel_t) == 1) {
        __m256i BG_lo = _mm256_unpacklo_epi8(B, G);
        __m256i BG_hi = _mm256_unpackhi_epi8(B, G);
        __m256i RA_lo = _mm256_unpacklo_epi8(R, A);
        __m256i RA_hi = _mm256_unpackhi_epi8(R, A);
        u0 = _mm256_unpacklo_epi16(BG_lo, RA_lo);  // [px 0-3  | px16-19]
        u1 = _mm256_unpackhi_epi16(BG_lo, RA_lo);  // [px 4-7  | px20-23]
        u2 = _mm256_unpacklo_epi16(BG_hi, RA_hi);  // [px 8-11 | px24-27]
        u3 = _mm256_unpackhi_epi16(BG_hi, RA_hi);  // [px12-15 | px28-31]
      } else {
        __m256i BG_lo = _mm256_unpacklo_epi16(B, G);
        __m256i BG_hi = _mm256_unpackhi_epi16(B, G);
        __m256i RA_lo = _mm256_unpacklo_epi16(R, A);
        __m256i RA_hi = _mm256_unpackhi_epi16(R, A);
        u0 = _mm256_unpacklo_epi32(BG_lo, RA_lo);  // [px 0-1  | px 8-9 ]
        u1 = _mm256_unpackhi_epi32(BG_lo, RA_lo);  // [px 2-3  | px10-11]
        u2 = _mm256_unpacklo_epi32(BG_hi, RA_hi);  // [px 4-5  | px12-13]
        u3 = _mm256_unpackhi_epi32(BG_hi, RA_hi);  // [px 6-7  | px14-15]
      }
      BYTE* d = dstp + x * 4 * sizeof(pixel_t);
      _mm256_store_si256(reinterpret_cast<__m256i*>(d +  0), _mm256_permute2x128_si256(u0, u1, 0x20));
      _mm256_store_si256(reinterpret_cast<__m256i*>(d + 32), _mm256_permute2x128_si256(u2, u3, 0x20));
      _mm256_store_si256(reinterpret_cast<__m256i*>(d + 64), _mm256_permute2x128_si256(u0, u1, 0x31));
      _mm256_store_si256(reinterpret_cast<__m256i*>(d + 96), _mm256_permute2x128_si256(u2, u3, 0x31));
    }

    // tail: process remaining pixels in small SIMD chunks
    // (may touch padded bytes beyond logical width)
    for (int tx = wmod; tx < width; tx += small_pixels) {
      __m128i tG = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp[0] + tx * sizeof(pixel_t)));
      __m128i tB = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp[1] + tx * sizeof(pixel_t)));
      __m128i tR = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp[2] + tx * sizeof(pixel_t)));
      __m128i tA;
      if constexpr (hasSrcAlpha)
        tA = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp[3] + tx * sizeof(pixel_t)));
      else
        tA = transparent128;

      BYTE* d = dstp + tx * 4 * sizeof(pixel_t);
      if constexpr (sizeof(pixel_t) == 1) {
        __m128i BG_lo = _mm_unpacklo_epi8(tB, tG);
        __m128i BG_hi = _mm_unpackhi_epi8(tB, tG);
        __m128i RA_lo = _mm_unpacklo_epi8(tR, tA);
        __m128i RA_hi = _mm_unpackhi_epi8(tR, tA);
        _mm_store_si128(reinterpret_cast<__m128i*>(d +  0), _mm_unpacklo_epi16(BG_lo, RA_lo));  // px tx+0..3
        _mm_store_si128(reinterpret_cast<__m128i*>(d + 16), _mm_unpackhi_epi16(BG_lo, RA_lo));  // px tx+4..7
        _mm_store_si128(reinterpret_cast<__m128i*>(d + 32), _mm_unpacklo_epi16(BG_hi, RA_hi));  // px tx+8..11
        _mm_store_si128(reinterpret_cast<__m128i*>(d + 48), _mm_unpackhi_epi16(BG_hi, RA_hi));  // px tx+12..15
      } else {
        __m128i BG_lo = _mm_unpacklo_epi16(tB, tG);
        __m128i BG_hi = _mm_unpackhi_epi16(tB, tG);
        __m128i RA_lo = _mm_unpacklo_epi16(tR, tA);
        __m128i RA_hi = _mm_unpackhi_epi16(tR, tA);
        _mm_store_si128(reinterpret_cast<__m128i*>(d +  0), _mm_unpacklo_epi32(BG_lo, RA_lo));  // px tx+0..1
        _mm_store_si128(reinterpret_cast<__m128i*>(d + 16), _mm_unpackhi_epi32(BG_lo, RA_lo));  // px tx+2..3
        _mm_store_si128(reinterpret_cast<__m128i*>(d + 32), _mm_unpacklo_epi32(BG_hi, RA_hi));  // px tx+4..5
        _mm_store_si128(reinterpret_cast<__m128i*>(d + 48), _mm_unpackhi_epi32(BG_hi, RA_hi));  // px tx+6..7
      }
    }

    dstp -= dst_pitch;
    srcp[0] += src_pitch[0];
    srcp[1] += src_pitch[1];
    srcp[2] += src_pitch[2];
    if constexpr (hasSrcAlpha)
      srcp[3] += src_pitch[3];
  }
}

// Instantiate them
template void convert_rgbp_to_rgba_avx2<uint8_t, false>(const BYTE* (&srcp)[4], BYTE* dstp, int(&src_pitch)[4], int dst_pitch, int width, int height);
template void convert_rgbp_to_rgba_avx2<uint8_t, true>(const BYTE* (&srcp)[4], BYTE* dstp, int(&src_pitch)[4], int dst_pitch, int width, int height);
template void convert_rgbp_to_rgba_avx2<uint16_t, false>(const BYTE* (&srcp)[4], BYTE* dstp, int(&src_pitch)[4], int dst_pitch, int width, int height);
template void convert_rgbp_to_rgba_avx2<uint16_t, true>(const BYTE* (&srcp)[4], BYTE* dstp, int(&src_pitch)[4], int dst_pitch, int width, int height);
