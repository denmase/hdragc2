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

#include "444convert_avx2.h"

#include <stdint.h>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#endif
#include <immintrin.h>

// YV24->YV12 uint8_t AVX2.
// permute4x64(0xD8) fixes packus_epi16 lane interleaving: [lo0,hi0,lo1,hi1] -> [lo0,lo1,hi0,hi1].
// Tail: XMM step (maddubs, SSSE3 implied by AVX2) then scalar.
void convert_yv24_chroma_to_yv12_u8_avx2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height)
{
  const __m256i ones256 = _mm256_set1_epi8(1);
  const __m256i two256  = _mm256_set1_epi16(2);
  const __m128i ones128 = _mm_set1_epi8(1);
  const __m128i two128  = _mm_set1_epi16(2);

  for (int y = 0; y < dst_height; ++y) {
    int x = 0;

    for (; x + 32 <= dst_width; x += 32) {
      __m256i r0lo = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2));
      __m256i r0hi = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2 + 32));
      __m256i r1lo = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2 + src_pitch));
      __m256i r1hi = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2 + src_pitch + 32));

      __m256i sum_lo = _mm256_srli_epi16(_mm256_add_epi16(_mm256_add_epi16(_mm256_maddubs_epi16(r0lo, ones256), _mm256_maddubs_epi16(r1lo, ones256)), two256), 2);
      __m256i sum_hi = _mm256_srli_epi16(_mm256_add_epi16(_mm256_add_epi16(_mm256_maddubs_epi16(r0hi, ones256), _mm256_maddubs_epi16(r1hi, ones256)), two256), 2);

      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + x),
        _mm256_permute4x64_epi64(_mm256_packus_epi16(sum_lo, sum_hi), 0xD8));
    }

    // XMM tail: one 16-byte chunk if enough room
    if (x + 16 <= dst_width) {
      __m128i r0lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2));
      __m128i r0hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + 16));
      __m128i r1lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch));
      __m128i r1hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch + 16));

      __m128i sum_lo = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(_mm_maddubs_epi16(r0lo, ones128), _mm_maddubs_epi16(r1lo, ones128)), two128), 2);
      __m128i sum_hi = _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(_mm_maddubs_epi16(r0hi, ones128), _mm_maddubs_epi16(r1hi, ones128)), two128), 2);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + x), _mm_packus_epi16(sum_lo, sum_hi));
      x += 16;
    }

    // scalar tail (0-15 remaining u8 pixels)
    for (; x < dst_width; ++x)
      dstp[x] = (srcp[x*2] + srcp[x*2+1] + srcp[x*2+src_pitch] + srcp[x*2+src_pitch+1] + 2) >> 2;

    dstp += dst_pitch;
    srcp += src_pitch * 2;
  }
}

// YV24->YV12 uint16_t lessthan16bit AVX2.
// hadd_epi16 lane interleaving fixed by permute4x64(0xD8).
// Tail: XMM hadd step (SSSE3 implied by AVX2) then scalar.
void convert_yv24_chroma_to_yv12_u16_lessthan16bit_avx2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height)
{
  const __m256i two256 = _mm256_set1_epi16(2);
  const __m128i two128 = _mm_set1_epi16(2);

  for (int y = 0; y < dst_height; ++y) {
    int x = 0;

    for (; x + 32 <= dst_width; x += 32) {
      __m256i r0lo = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2));
      __m256i r0hi = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2 + 32));
      __m256i r1lo = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2 + src_pitch));
      __m256i r1hi = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2 + src_pitch + 32));

      __m256i pair_sums = _mm256_permute4x64_epi64(
        _mm256_hadd_epi16(_mm256_add_epi16(r0lo, r1lo), _mm256_add_epi16(r0hi, r1hi)), 0xD8);

      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + x),
        _mm256_srli_epi16(_mm256_add_epi16(pair_sums, two256), 2));
    }

    // XMM tail: one 16-byte chunk (8 u16 pixels) if enough room
    if (x + 16 <= dst_width) {
      __m128i r0lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2));
      __m128i r0hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + 16));
      __m128i r1lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch));
      __m128i r1hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch + 16));

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + x),
        _mm_srli_epi16(_mm_add_epi16(_mm_hadd_epi16(_mm_add_epi16(r0lo, r1lo), _mm_add_epi16(r0hi, r1hi)), two128), 2));
      x += 16;
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

// YV24->YV12 uint16_t true 16-bit AVX2.
// XOR 0x8000 pivot before madd_epi16 maps u16→i16 [-32768,32767]; srai+packs+XOR restores u16.
// packs_epi32 lane interleaving fixed by permute4x64(0xD8).
// Tail: XMM step (SSE2; SSE4.1 implied by AVX2) then scalar.
void convert_yv24_chroma_to_yv12_u16_avx2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height)
{
  const __m256i xor256  = _mm256_set1_epi16((short)0x8000);
  const __m256i ones256 = _mm256_set1_epi16(1);
  const __m256i two256  = _mm256_set1_epi32(2);
  const __m128i xor128  = _mm_set1_epi16((short)0x8000);
  const __m128i ones128 = _mm_set1_epi16(1);
  const __m128i two128  = _mm_set1_epi32(2);

  for (int y = 0; y < dst_height; ++y) {
    int x = 0;

    for (; x + 32 <= dst_width; x += 32) {
      __m256i r0lo = _mm256_xor_si256(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2)),            xor256);
      __m256i r0hi = _mm256_xor_si256(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2 + 32)),       xor256);
      __m256i r1lo = _mm256_xor_si256(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2 + src_pitch)),      xor256);
      __m256i r1hi = _mm256_xor_si256(_mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + x * 2 + src_pitch + 32)), xor256);

      __m256i sum_lo = _mm256_srai_epi32(_mm256_add_epi32(_mm256_add_epi32(_mm256_madd_epi16(r0lo, ones256), _mm256_madd_epi16(r1lo, ones256)), two256), 2);
      __m256i sum_hi = _mm256_srai_epi32(_mm256_add_epi32(_mm256_add_epi32(_mm256_madd_epi16(r0hi, ones256), _mm256_madd_epi16(r1hi, ones256)), two256), 2);

      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + x),
        _mm256_xor_si256(_mm256_permute4x64_epi64(_mm256_packs_epi32(sum_lo, sum_hi), 0xD8), xor256));
    }

    // XMM tail: one 16-byte chunk (8 u16 pixels) if enough room
    if (x + 16 <= dst_width) {
      __m128i r0lo = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2)),            xor128);
      __m128i r0hi = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + 16)),       xor128);
      __m128i r1lo = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch)),      xor128);
      __m128i r1hi = _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 2 + src_pitch + 16)), xor128);

      __m128i sum_lo = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(r0lo, ones128), _mm_madd_epi16(r1lo, ones128)), two128), 2);
      __m128i sum_hi = _mm_srai_epi32(_mm_add_epi32(_mm_add_epi32(_mm_madd_epi16(r0hi, ones128), _mm_madd_epi16(r1hi, ones128)), two128), 2);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp + x),
        _mm_xor_si128(_mm_packs_epi32(sum_lo, sum_hi), xor128));
      x += 16;
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
