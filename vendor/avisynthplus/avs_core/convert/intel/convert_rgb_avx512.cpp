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

#include <avs/config.h>
#include <avs/types.h>
#include <cstdint>

#include "../../filters/intel/check_avx512.h" // compiler avx512 directives check
#include "convert_rgb_avx512.h"

#include <immintrin.h> // Includes AVX-512 intrinsics

template<typename pixel_t, bool targetHasAlpha>
void convert_rgba_to_rgbp_avx512vbmi(const BYTE *srcp, BYTE * (&dstp)[4],
    int src_pitch, int (&dst_pitch)[4], int width, int height)
{
  // 8-bit:  16 BGRA8  pixels = 64 bytes fits in one ZMM
  // 16-bit:  8 BGRA16 pixels = 64 bytes fits in one ZMM
  const int pixels_at_a_time = (sizeof(pixel_t) == 1) ? 16 : 8;

  // Build the vpermb index vector, constant, computed once
  __m512i perm;
  if constexpr (sizeof(pixel_t) == 1) {
    // B channel: bytes 0,4,8,...,60  (stride 4, offset 0)
    // G channel: bytes 1,5,9,...,61  (stride 4, offset 1)
    // R channel: bytes 2,6,10,...,62 (stride 4, offset 2)
    // A channel: bytes 3,7,11,...,63 (stride 4, offset 3)
    alignas(64) uint8_t idx[64];
    for (int i = 0; i < 16; i++) idx[i]    = i * 4 + 0; // B
    for (int i = 0; i < 16; i++) idx[16+i] = i * 4 + 1; // G
    for (int i = 0; i < 16; i++) idx[32+i] = i * 4 + 2; // R
    for (int i = 0; i < 16; i++) idx[48+i] = i * 4 + 3; // A
    perm = _mm512_load_si512(reinterpret_cast<const __m512i *>(idx));
  }
  else {
    alignas(64) uint8_t idx[64];
    for (int i = 0; i < 8; i++) {
      idx[i * 2 + 0] = (uint8_t)(i * 8 + 0); // B low byte
      idx[i * 2 + 1] = (uint8_t)(i * 8 + 1); // B high byte
      idx[16 + i * 2 + 0] = (uint8_t)(i * 8 + 2); // G low byte
      idx[16 + i * 2 + 1] = (uint8_t)(i * 8 + 3); // G high byte
      idx[32 + i * 2 + 0] = (uint8_t)(i * 8 + 4); // R low byte
      idx[32 + i * 2 + 1] = (uint8_t)(i * 8 + 5); // R high byte
      idx[48 + i * 2 + 0] = (uint8_t)(i * 8 + 6); // A low byte
      idx[48 + i * 2 + 1] = (uint8_t)(i * 8 + 7); // A high byte
    }
    perm = _mm512_load_si512(reinterpret_cast<const __m512i*>(idx));
  }

  // No remainder: BGRA source is 4 bytes/pixel, 64-byte aligned scanlines guarantee width is a multiple of 16 (8-bit) or 8 (16-bit)
  for (int y = height; y > 0; --y) {
    for (int x = 0; x < width; x += pixels_at_a_time) {
      __m512i src = _mm512_load_si512(
          reinterpret_cast<const __m512i *>(srcp + x * 4 * sizeof(pixel_t)));

      // 64-byte cross-lane permutation
      // Result layout:
      //   bytes  0-15: B0..B15  (8-bit) or B0..B7 as pairs (16-bit)
      //   bytes 16-31: G channel
      //   bytes 32-47: R channel
      //   bytes 48-63: A channel
      __m512i result = _mm512_permutexvar_epi8(perm, src);

      // Extract each 128-bit quarter and store
      // _mm512_extracti32x4_epi32: extract 128-bit lane 0,1,2,3
      __m128i B = _mm512_extracti32x4_epi32(result, 0);
      __m128i G = _mm512_extracti32x4_epi32(result, 1);
      __m128i R = _mm512_extracti32x4_epi32(result, 2);
      __m128i A = _mm512_extracti32x4_epi32(result, 3);

      _mm_store_si128(reinterpret_cast<__m128i *>(dstp[1] + x * sizeof(pixel_t)), B);
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp[0] + x * sizeof(pixel_t)), G);
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp[2] + x * sizeof(pixel_t)), R);
      if constexpr (targetHasAlpha)
        _mm_store_si128(reinterpret_cast<__m128i *>(dstp[3] + x * sizeof(pixel_t)), A);
    }


    srcp -= src_pitch;
    dstp[0] += dst_pitch[0];
    dstp[1] += dst_pitch[1];
    dstp[2] += dst_pitch[2];
    if constexpr (targetHasAlpha)
      dstp[3] += dst_pitch[3];
  }
}

// Instantiations
template void convert_rgba_to_rgbp_avx512vbmi<uint8_t,  false>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgba_to_rgbp_avx512vbmi<uint8_t,  true>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgba_to_rgbp_avx512vbmi<uint16_t, false>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgba_to_rgbp_avx512vbmi<uint16_t, true>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
