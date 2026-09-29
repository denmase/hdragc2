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


#include "../convert_rgb.h"

// Intrinsics base header + really required extension headers
#if defined(_MSC_VER)
#include <intrin.h> // MSVC
#else 
#include <x86intrin.h> // GCC/MinGW/Clang/LLVM
#endif
#include <tmmintrin.h> // SSSE3

#include <avs/alignment.h>




#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
void convert_rgb48_to_rgb64_ssse3(const BYTE *srcp, BYTE *dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height)
{
  size_t mod16_width = sizeof(uint16_t)*(width & (~size_t(7)));
  __m128i mask0 = _mm_set_epi8((char)0x80, (char)0x80, 11, 10, 9, 8, 7, 6, (char)0x80, (char)0x80, 5, 4, 3, 2, 1, 0);
  __m128i mask1 = _mm_set_epi8((char)0x80, (char)0x80, 15, 14, 13, 12, 11, 10, (char)0x80, (char)0x80, 9, 8, 7, 6, 5, 4);
  __m128i alpha = _mm_set_epi32(0xFFFF0000,0x00000000,0xFFFF0000,0x00000000);

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod16_width; x+= 16) {
      __m128i src0 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*3));
      // 7...       ...0
      // B G|R B G R B G  #0 #1 (#2)       x*3+0
      // G R B G|R B G R  (#2) #3 #4 (#5)  x*3+16
      // R B G R B G|R B  (#5) #6 #7       x*3+32
      __m128i dst = _mm_or_si128(alpha, _mm_shuffle_epi8(src0, mask0));
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+4*x), dst);

      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*3+16));
      __m128i tmp = _mm_alignr_epi8(src1, src0, 12);
      dst = _mm_or_si128(alpha, _mm_shuffle_epi8(tmp, mask0));
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*4+16), dst);

      __m128i src2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*3+32));
      tmp = _mm_alignr_epi8(src2, src1, 8);
      dst = _mm_or_si128(alpha, _mm_shuffle_epi8(tmp, mask0));
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*4+32), dst);

      dst = _mm_or_si128(alpha, _mm_shuffle_epi8(src2, mask1));
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*4+48), dst);
    }

    for (size_t x = mod16_width/sizeof(uint16_t); x < width; ++x) {
      reinterpret_cast<uint16_t *>(dstp)[x*4+0] = reinterpret_cast<const uint16_t *>(srcp)[x*3+0];
      reinterpret_cast<uint16_t *>(dstp)[x*4+1] = reinterpret_cast<const uint16_t *>(srcp)[x*3+1];
      reinterpret_cast<uint16_t *>(dstp)[x*4+2] = reinterpret_cast<const uint16_t *>(srcp)[x*3+2];
      reinterpret_cast<uint16_t *>(dstp)[x*4+3] = 65535;
    }

    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
void convert_rgb24_to_rgb32_ssse3(const BYTE *srcp, BYTE *dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height)
{
  size_t mod16_width = (width + 3) & (~size_t(15)); //when the modulo is more than 13, a problem does not happen
  __m128i mask0 = _mm_set_epi8((char)0x80, 11, 10, 9, (char)0x80, 8, 7, 6, (char)0x80, 5, 4, 3, (char)0x80, 2, 1, 0);
  __m128i mask1 = _mm_set_epi8((char)0x80, 15, 14, 13, (char)0x80, 12, 11, 10, (char)0x80, 9, 8, 7, (char)0x80, 6, 5, 4);
  __m128i alpha = _mm_set1_epi32(0xFF000000);

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod16_width; x+= 16) {
      __m128i src0 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*3));
      __m128i dst = _mm_or_si128(alpha, _mm_shuffle_epi8(src0, mask0));
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+4*x), dst);

      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*3+16));
      __m128i tmp = _mm_alignr_epi8(src1, src0, 12);
      dst = _mm_or_si128(alpha, _mm_shuffle_epi8(tmp, mask0));
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*4+16), dst);

      __m128i src2 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*3+32));
      tmp = _mm_alignr_epi8(src2, src1, 8);
      dst = _mm_or_si128(alpha, _mm_shuffle_epi8(tmp, mask0));
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*4+32), dst);

      dst = _mm_or_si128(alpha, _mm_shuffle_epi8(src2, mask1));
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*4+48), dst);
    }

    for (size_t x = mod16_width; x < width; ++x) {
      dstp[x*4+0] = srcp[x*3+0];
      dstp[x*4+1] = srcp[x*3+1];
      dstp[x*4+2] = srcp[x*3+2];
      dstp[x*4+3] = 255;
    }

    srcp += src_pitch;
    dstp += dst_pitch;
  }
}


#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
void convert_rgb64_to_rgb48_ssse3(const BYTE *srcp, BYTE *dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height)
{
  size_t mod16_width = sizeof(uint16_t) * ((width) & (~size_t(7))); // perhaps width+2 is still o.k
  __m128i mask0 = _mm_set_epi8(13, 12, 11, 10, 9, 8, 5, 4, 3, 2, 1, 0, 15, 14, 7, 6); // BBGGRRBBGGRRAAAA
  __m128i mask1 = _mm_set_epi8(15, 14, 7, 6, 13, 12, 11, 10, 9, 8, 5, 4, 3, 2, 1, 0); // AAAABBGGRRBBGGRR

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod16_width; x+= 16) {
      __m128i src0 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*4));    //a1b1 g1r1 a0b0 g0r0
      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*4+16)); //a3b3 g3r3 a2b2 g2r2
      src0 = _mm_shuffle_epi8(src0, mask0);         //b1g1 r1b0 g0r0 a1a0
      src1 = _mm_shuffle_epi8(src1, mask1);         //a3a2 b3g3 r3b2 g2r2
      __m128i dst = _mm_alignr_epi8(src1, src0, 4); //g2r2 b1g1 r1b0 g0r0
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*3), dst);

      src0 = _mm_slli_si128(src1, 4);       // b3g3 r3b2 g2r2 XXXX
      src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 4 + 32)); // a5b5 g5r5 a4b4 g4r4
      src1 = _mm_shuffle_epi8(src1, mask1); // a5a4 b5g5 r5b4 g4r4
      dst = _mm_alignr_epi8(src1, src0, 8); // r5b4 g4r4 b3g3 r3b2
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*3+16), dst);

      src0 = _mm_slli_si128(src1, 4);        // b5g5 r5b4 g4r4 XXXX
      src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*4+48)); // a7b7 g7r7 a6b6 g6r6
      src1 = _mm_shuffle_epi8(src1, mask1);  // a7a6 b7g7 r7b6 g6r6
      dst = _mm_alignr_epi8(src1, src0, 12); // b7g7 r7b6 g6r6 b5g5
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*3+32), dst);
    }

    for (size_t x = mod16_width/sizeof(uint16_t); x < width; ++x) {
      reinterpret_cast<uint16_t *>(dstp)[x*3+0] = reinterpret_cast<const uint16_t *>(srcp)[x*4+0];
      reinterpret_cast<uint16_t *>(dstp)[x*3+1] = reinterpret_cast<const uint16_t *>(srcp)[x*4+1];
      reinterpret_cast<uint16_t *>(dstp)[x*3+2] = reinterpret_cast<const uint16_t *>(srcp)[x*4+2];
    }

    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

//todo: think how to port to sse2 without tons of shuffles or (un)packs
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
void convert_rgb32_to_rgb24_ssse3(const BYTE *srcp, BYTE *dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height)
{
  size_t mod16_width = (width + 3) & (~size_t(15)); //when the modulo is more than 13, a problem does not happen
  __m128i mask0 = _mm_set_epi8(14, 13, 12, 10, 9, 8, 6, 5, 4, 2, 1, 0, 15, 11, 7, 3);
  __m128i mask1 = _mm_set_epi8(15, 11, 7, 3, 14, 13, 12, 10, 9, 8, 6, 5, 4, 2, 1, 0);

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod16_width; x+= 16) {
      __m128i src0 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*4));    //a3b3 g3r3 a2b2 g2r2 a1b1 g1r1 a0b0 g0r0
      __m128i src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*4+16)); //a7b7 g7r7 a6b6 g6r6 a5b5 g5r5 a4b4 g4r4
      src0 = _mm_shuffle_epi8(src0, mask0);         //b3g3 r3b2 g2r2 b1g1 r1b0 g0r0 a3a2 a1a0
      src1 = _mm_shuffle_epi8(src1, mask1);         //a7a6 a5a4 b7g7 r7b6 g6r6 b5g5 r5b4 g4r4
      __m128i dst = _mm_alignr_epi8(src1, src0, 4); //r5b4 g4r4 b3g3 r3b2 g2r2 b1g1 r1b0 g0r0
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*3), dst);

      src0 = _mm_slli_si128(src1, 4);       //b7g7 r7b6 g6r6 b5g5 r5b4 g4r4 XXXX XXXX
      src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 4 + 32)); //aBbB gBrB aAbA gArA a9b9 g9r9 a8b8 g8r8
      src1 = _mm_shuffle_epi8(src1, mask1); //aBaA a9a8 bBgB rBbA gArA b9g9 r9b8 g8r8
      dst = _mm_alignr_epi8(src1, src0, 8); //gArA b9g9 r9b8 g8r8 b7g7 r7b6 g6r6 b5g5
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*3+16), dst);

      src0 = _mm_slli_si128(src1, 4);        //bBgB rBbA gArA b9g9 r9b8 g8r8 XXXX XXXX
      src1 = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp+x*4+48)); //aFbF gFrF aEbE gErE aDbD gDrD aCbC gCrC
      src1 = _mm_shuffle_epi8(src1, mask1);  //aFaE aDaC bFgF rFbE gErE bDgD rDbC gCrC
      dst = _mm_alignr_epi8(src1, src0, 12); //bFgF rFbE gErE bDgD rDbC gCrC bBgB rBbA
      _mm_stream_si128(reinterpret_cast<__m128i*>(dstp+x*3+32), dst);
    }

    for (size_t x = mod16_width; x < width; ++x) {
      dstp[x*3+0] = srcp[x*4+0];
      dstp[x*3+1] = srcp[x*4+1];
      dstp[x*3+2] = srcp[x*4+2];
    }

    srcp += src_pitch;
    dstp += dst_pitch;
  }
}

void convert_rgb32_to_rgb24_sse2(const BYTE* srcp, BYTE* dstp, size_t src_pitch, size_t dst_pitch, size_t width, size_t height) {
  size_t mod4_width = width & (~size_t(3));

  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < mod4_width; x += 4) {
      // Load 4 pixels (16 bytes)
      __m128i src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(srcp + x * 4));

      // Pixel 0 (bytes 0,1,2) and Pixel 1 (bytes 4,5,6)
      // Goal: [ 00 00 r1 g1 b1 r0 g0 b0 ] in the low 64 bits
      __m128i p0 = _mm_and_si128(src, _mm_set_epi32(0, 0, 0, 0x00FFFFFF));
      __m128i p1 = _mm_and_si128(src, _mm_set_epi32(0, 0, 0x00FFFFFF, 0));
      __m128i dst01 = _mm_or_si128(p0, _mm_srli_si128(p1, 1));

      // Pixel 2 (bytes 8,9,10) and Pixel 3 (bytes 12,13,14)
      __m128i p2 = _mm_and_si128(src, _mm_set_epi32(0, 0x00FFFFFF, 0, 0));
      __m128i p3 = _mm_and_si128(src, _mm_set_epi32(0x00FFFFFF, 0, 0, 0));

      // Shift p2 down to byte 6 and p3 down to byte 9
      __m128i dst23 = _mm_or_si128(_mm_srli_si128(p2, 2), _mm_srli_si128(p3, 3));

      // Combine them
      __m128i final = _mm_or_si128(dst01, dst23);

      // final now contains: [ B3 G3 R3 | B2 G2 R2 | B1 G1 R1 | B0 G0 R0 ]
      // In the first 12 bytes. 
      // We store 8 bytes (64-bit) then 4 bytes (32-bit)
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp + x * 3), final);
      *reinterpret_cast<int*>(dstp + x * 3 + 8) = _mm_cvtsi128_si32(_mm_srli_si128(final, 8));
    }

    // Standard remainder loop
    for (size_t x = mod4_width; x < width; ++x) {
      dstp[x * 3 + 0] = srcp[x * 4 + 0];
      dstp[x * 3 + 1] = srcp[x * 4 + 1];
      dstp[x * 3 + 2] = srcp[x * 4 + 2];
    }

    srcp += src_pitch;
    dstp += dst_pitch;
  }
}


// ============================================================
// SSE2 emulation of _mm_alignr_epi8(hi, lo, N)
// Concatenates [hi:lo] as 32 bytes, returns bytes [N..N+15]
// ============================================================
template<int N>
static AVS_FORCEINLINE __m128i sse2_alignr(__m128i hi, __m128i lo) {
  return _mm_or_si128(_mm_srli_si128(lo, N), _mm_slli_si128(hi, 16 - N));
}

// ============================================================
// 8-bit helper: gather B, G, R from 4 BGR8 pixels
//
// Input 'grp': low 12 bytes hold 4 BGR8 pixels
//   byte:  0   1   2   3   4   5   6   7   8   9  10  11
//   data: B0  G0  R0  B1  G1  R1  B2  G2  R2  B3  G3  R3
//
// Output bOut, gOut, rOut: valid bytes in positions 0..3 only
//   bOut[0..3] = B0 B1 B2 B3
//   gOut[0..3] = G0 G1 G2 G3
//   rOut[0..3] = R0 R1 R2 R3
// ============================================================
static AVS_FORCEINLINE void gather_bgr8_4px(__m128i grp,
  __m128i& bOut, __m128i& gOut, __m128i& rOut)
{
  // B is at bytes 0, 3, 6, 9. We shift them to 0, 1, 2, 3.
  __m128i b = _mm_and_si128(grp, _mm_set_epi32(0, 0, 0, 0x000000FF)); // B0
  b = _mm_or_si128(b, _mm_and_si128(_mm_srli_si128(grp, 2), _mm_set_epi32(0, 0, 0, 0x0000FF00))); // B1
  b = _mm_or_si128(b, _mm_and_si128(_mm_srli_si128(grp, 4), _mm_set_epi32(0, 0, 0, 0x00FF0000))); // B2
  b = _mm_or_si128(b, _mm_and_si128(_mm_srli_si128(grp, 6), _mm_set_epi32(0, 0, 0, 0xFF000000))); // B3
  bOut = b;

  // G is at bytes 1, 4, 7, 10.
  __m128i g = _mm_and_si128(_mm_srli_si128(grp, 1), _mm_set_epi32(0, 0, 0, 0x000000FF)); // G0
  g = _mm_or_si128(g, _mm_and_si128(_mm_srli_si128(grp, 3), _mm_set_epi32(0, 0, 0, 0x0000FF00))); // G1
  g = _mm_or_si128(g, _mm_and_si128(_mm_srli_si128(grp, 5), _mm_set_epi32(0, 0, 0, 0x00FF0000))); // G2
  g = _mm_or_si128(g, _mm_and_si128(_mm_srli_si128(grp, 7), _mm_set_epi32(0, 0, 0, 0xFF000000))); // G3
  gOut = g;

  // R is at bytes 2, 5, 8, 11.
  __m128i r = _mm_and_si128(_mm_srli_si128(grp, 2), _mm_set_epi32(0, 0, 0, 0x000000FF)); // R0
  r = _mm_or_si128(r, _mm_and_si128(_mm_srli_si128(grp, 4), _mm_set_epi32(0, 0, 0, 0x0000FF00))); // R1
  r = _mm_or_si128(r, _mm_and_si128(_mm_srli_si128(grp, 6), _mm_set_epi32(0, 0, 0, 0x00FF0000))); // R2
  r = _mm_or_si128(r, _mm_and_si128(_mm_srli_si128(grp, 8), _mm_set_epi32(0, 0, 0, 0xFF000000))); // R3
  rOut = r;
}

// ============================================================
// 8-bit: deinterleave 16 BGR8 pixels from 3 × 16-byte registers
// into full 16-byte B, G, R output registers
// ============================================================
static AVS_FORCEINLINE void deinterleave_bgr8_16px(
  __m128i r0, __m128i r1, __m128i r2,
  __m128i& B, __m128i& G, __m128i& R)
{
  // Split 48-byte stream into 4 groups of 12 bytes (4 pixels each),
  // each aligned to byte 0 of a 128-bit register:
  //   group0: r0[0..11]
  //   group1: r0[12..15] ++ r1[0..7]   = alignr(r1, r0, 12)
  //   group2: r1[8..15]  ++ r2[0..3]   = alignr(r2, r1,  8)
  //   group3: r2[4..15]                = srli(r2, 4)
  __m128i g0 = r0;
  __m128i g1 = sse2_alignr<12>(r1, r0);
  __m128i g2 = sse2_alignr<8>(r2, r1);
  __m128i g3 = _mm_srli_si128(r2, 4);

  // Extract channels from each group (4 valid bytes each in positions 0..3)
  __m128i B0, G0, R0, B1, G1, R1, B2, G2, R2, B3, G3, R3;
  gather_bgr8_4px(g0, B0, G0, R0);
  gather_bgr8_4px(g1, B1, G1, R1);
  gather_bgr8_4px(g2, B2, G2, R2);
  gather_bgr8_4px(g3, B3, G3, R3);

  // Combine 4 groups × 4 bytes into one 16-byte register per channel.
  // Each Bx/Gx/Rx has its 4 bytes in the low dword (bytes 0..3).
  // unpacklo_epi32 interleaves low dwords of two registers:
  //   unpacklo_epi32(A, B) = [A.dw0, B.dw0, A.dw1, B.dw1]
  // unpacklo_epi64 combines low qwords:
  //   unpacklo_epi64(A, B) = [A.qw0, B.qw0]
  __m128i B01 = _mm_unpacklo_epi32(B0, B1);  // [B_g0(4B), B_g1(4B), ...]
  __m128i B23 = _mm_unpacklo_epi32(B2, B3);
  B = _mm_unpacklo_epi64(B01, B23);           // all 16 B bytes

  __m128i G01 = _mm_unpacklo_epi32(G0, G1);
  __m128i G23 = _mm_unpacklo_epi32(G2, G3);
  G = _mm_unpacklo_epi64(G01, G23);

  __m128i R01 = _mm_unpacklo_epi32(R0, R1);
  __m128i R23 = _mm_unpacklo_epi32(R2, R3);
  R = _mm_unpacklo_epi64(R01, R23);
}

// ============================================================
// 16-bit helper: gather B, G, R from 2 BGR16 pixels
//
// Input 'grp': low 12 bytes hold 2 BGR16 pixels
//   word:  0   1   2   3   4   5
//   data: B0  G0  R0  B1  G1  R1
//
// Output bOut, gOut, rOut: valid words in positions 0..1 only
//   bOut[0..1] = B0 B1
//   gOut[0..1] = G0 G1
//   rOut[0..1] = R0 R1
// ============================================================
static AVS_FORCEINLINE void gather_bgr16_2px(__m128i grp,
  __m128i& bOut, __m128i& gOut, __m128i& rOut)
{
  // Words in grp: B0 G0 R0 B1 G1 R1 x x
  // Blue: w0, w3. Use shuffle to bring them to low dword.
  bOut = _mm_shufflelo_epi16(grp, _MM_SHUFFLE(3, 3, 3, 0));
  // Green: w1, w4.
  gOut = _mm_shufflelo_epi16(_mm_srli_si128(grp, 2), _MM_SHUFFLE(3, 3, 3, 0));
  // Red: w2, w5.
  rOut = _mm_shufflelo_epi16(_mm_srli_si128(grp, 4), _MM_SHUFFLE(3, 3, 3, 0));

  // Mask out the upper 3 dwords if necessary (though unpacklo handles it)
  __m128i mask = _mm_set_epi32(0, 0, 0, 0xFFFFFFFF);
  bOut = _mm_and_si128(bOut, mask);
  gOut = _mm_and_si128(gOut, mask);
  rOut = _mm_and_si128(rOut, mask);
}

// ============================================================
// 16-bit: deinterleave 8 BGR16 pixels from 3 × 16-byte registers
// into full 16-byte B, G, R output registers
// ============================================================
static AVS_FORCEINLINE void deinterleave_bgr16_8px(
  __m128i r0, __m128i r1, __m128i r2,
  __m128i& B, __m128i& G, __m128i& R)
{
  // Same group extraction as 8-bit case (48 bytes, same offsets)
  __m128i g0 = r0;
  __m128i g1 = sse2_alignr<12>(r1, r0);
  __m128i g2 = sse2_alignr<8>(r2, r1);
  __m128i g3 = _mm_srli_si128(r2, 4);

  // Each group: 12 bytes = 6 words = 2 BGR16 pixels
  // Each output xB/xG/xR has 2 valid words (4 bytes) in dword 0
  __m128i B0, G0, R0, B1, G1, R1, B2, G2, R2, B3, G3, R3;
  gather_bgr16_2px(g0, B0, G0, R0);
  gather_bgr16_2px(g1, B1, G1, R1);
  gather_bgr16_2px(g2, B2, G2, R2);
  gather_bgr16_2px(g3, B3, G3, R3);

  // Combine: 4 groups × 2 words = 8 words = 16 bytes per channel
  __m128i B01 = _mm_unpacklo_epi32(B0, B1);
  __m128i B23 = _mm_unpacklo_epi32(B2, B3);
  B = _mm_unpacklo_epi64(B01, B23);

  __m128i G01 = _mm_unpacklo_epi32(G0, G1);
  __m128i G23 = _mm_unpacklo_epi32(G2, G3);
  G = _mm_unpacklo_epi64(G01, G23);

  __m128i R01 = _mm_unpacklo_epi32(R0, R1);
  __m128i R23 = _mm_unpacklo_epi32(R2, R3);
  R = _mm_unpacklo_epi64(R01, R23);
}

// ============================================================
// Main conversion function
// ============================================================
// RGB24/48 -> RGBP(A)8/16 SSE2 implementation. Ugly and not very quick.
template<typename pixel_t, bool targetHasAlpha>
void convert_rgb_to_rgbp_sse2(const BYTE* srcp, BYTE* (&dstp)[4],
  int src_pitch, int(&dst_pitch)[4],
  int width, int height)
{
  const int pixels_at_a_time = (sizeof(pixel_t) == 1) ? 16 : 8;
  const int wmod = (width / pixels_at_a_time) * pixels_at_a_time;

  __m128i max_pixel_value;
  if constexpr (sizeof(pixel_t) == 1)
    max_pixel_value = _mm_set1_epi8((char)0xFF);
  else
    max_pixel_value = _mm_set1_epi16((short)0xFFFF);

  for (int y = height; y > 0; --y) {

    for (int x = 0; x < wmod; x += pixels_at_a_time) {
      const BYTE* src = srcp + x * 3 * sizeof(pixel_t);
      __m128i r0 = _mm_load_si128(reinterpret_cast<const __m128i*>(src));
      __m128i r1 = _mm_load_si128(reinterpret_cast<const __m128i*>(src + 16));
      __m128i r2 = _mm_load_si128(reinterpret_cast<const __m128i*>(src + 32));

      __m128i B, G, R;
      if constexpr (sizeof(pixel_t) == 1)
        deinterleave_bgr8_16px(r0, r1, r2, B, G, R);
      else
        deinterleave_bgr16_8px(r0, r1, r2, B, G, R);

      _mm_store_si128(reinterpret_cast<__m128i*>(dstp[1] + x * sizeof(pixel_t)), B);
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp[0] + x * sizeof(pixel_t)), G);
      _mm_store_si128(reinterpret_cast<__m128i*>(dstp[2] + x * sizeof(pixel_t)), R);
      if constexpr (targetHasAlpha)
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp[3] + x * sizeof(pixel_t)), max_pixel_value);
    }

    if (wmod != width) {
      size_t x = width - pixels_at_a_time;
      const BYTE* src = srcp + x * 3 * sizeof(pixel_t);
      __m128i r0 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src));
      __m128i r1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 16));
      __m128i r2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 32));

      __m128i B, G, R;
      if constexpr (sizeof(pixel_t) == 1)
        deinterleave_bgr8_16px(r0, r1, r2, B, G, R);
      else
        deinterleave_bgr16_8px(r0, r1, r2, B, G, R);

      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp[1] + x * sizeof(pixel_t)), B);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp[0] + x * sizeof(pixel_t)), G);
      _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp[2] + x * sizeof(pixel_t)), R);
      if constexpr (targetHasAlpha)
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dstp[3] + x * sizeof(pixel_t)), max_pixel_value);
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
template void convert_rgb_to_rgbp_sse2<uint8_t, false>(const BYTE*, BYTE* (&)[4], int, int(&)[4], int, int);
template void convert_rgb_to_rgbp_sse2<uint8_t, true>(const BYTE*, BYTE* (&)[4], int, int(&)[4], int, int);
template void convert_rgb_to_rgbp_sse2<uint16_t, false>(const BYTE*, BYTE* (&)[4], int, int(&)[4], int, int);
template void convert_rgb_to_rgbp_sse2<uint16_t, true>(const BYTE*, BYTE* (&)[4], int, int(&)[4], int, int);

// minimum width: 48 bytes
template<typename pixel_t, bool targetHasAlpha>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
void convert_rgb_to_rgbp_ssse3(const BYTE *srcp, BYTE * (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height)
{
  // RGB24: 3x16 bytes cycle, 16*(RGB) 8bit pixels
  // RGB48: 3x16 bytes cycle, 8*(RGB) 16bit pixels
  // 0123456789ABCDEF 0123456789ABCDEF 0123456789ABCDEF
  // BGRBGRBGRBGRBGRB GRBGRBGRBGRBGRBG RBGRBGRBGRBGRBGR // 8 bit
  // B G R B G R B G  R B G R B G R B  G R B G R B G R  // 16 bit
  // 1111111111112222 2222222233333333 3333444444444444

  const int pixels_at_a_time = (sizeof(pixel_t) == 1) ? 16 : 8;
  const int wmod = (width / pixels_at_a_time) * pixels_at_a_time; // 8 pixels for 8 bit, 4 pixels for 16 bit
  __m128i mask;
  if constexpr(sizeof(pixel_t) == 1)
    mask = _mm_set_epi8(15, 14, 13, 12, 11, 8, 5, 2, 10, 7, 4, 1, 9, 6, 3, 0);
  else
    mask = _mm_set_epi8(15, 14, 13, 12, 11, 10, 5, 4, 9, 8, 3, 2, 7, 6, 1, 0);

  __m128i max_pixel_value;
  if constexpr(sizeof(pixel_t) == 1)
    max_pixel_value = _mm_set1_epi8((char)0xFF);
  else
    max_pixel_value = _mm_set1_epi16((short)0xFFFF); // bits_per_pixel is 16

  for (int y = height; y > 0; --y) {
    __m128i BGRA_1, BGRA_2, BGRA_3;
    for (int x = 0; x < wmod; x += pixels_at_a_time) {
      BGRA_1 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x * 48 / pixels_at_a_time));
      BGRA_2 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x * 48 / pixels_at_a_time + 16));
      BGRA_3 = _mm_load_si128(reinterpret_cast<const __m128i *>(srcp + x * 48 / pixels_at_a_time + 32));

      auto pack_lo = _mm_shuffle_epi8(BGRA_1, mask); // 111111111111: BBBBGGGGRRRR and rest: BGRB | BBGGRR and rest: BBGG
      BGRA_1 = _mm_alignr_epi8(BGRA_2, BGRA_1, 12);
      auto pack_hi = _mm_shuffle_epi8(BGRA_1, mask); // 222222222222: BBBBGGGGRRRR | BBGGRR
      BGRA_2 = _mm_alignr_epi8(BGRA_3, BGRA_2, 8);
      auto pack_lo2 = _mm_shuffle_epi8(BGRA_2, mask); // 333333333333: BBBBGGGGRRRR | BBGGRR
      BGRA_3 = _mm_srli_si128(BGRA_3, 4); // to use the same mask
      auto pack_hi2 = _mm_shuffle_epi8(BGRA_3, mask); // 444444444444: BBBBGGGGRRRR | BBGGRR

      __m128i BG1 = _mm_unpacklo_epi32(pack_lo, pack_hi);  // BBBB_lo BBBB_hi GGGG_lo GGGG_hi
      __m128i BG2 = _mm_unpacklo_epi32(pack_lo2, pack_hi2);  // BBBB_lo BBBB_hi GGGG_lo GGGG_hi
      __m128i RA1 = _mm_unpackhi_epi32(pack_lo, pack_hi);   // RRRR_lo RRRR_hi AAAA_lo AAAA_hi
      __m128i RA2 = _mm_unpackhi_epi32(pack_lo2, pack_hi2);  // RRRR_lo RRRR_hi AAAA_lo AAAA_hi
      __m128i B = _mm_unpacklo_epi64(BG1, BG2);
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp[1] + x * sizeof(pixel_t)), B); // B
      __m128i G = _mm_unpackhi_epi64(BG1, BG2);
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp[0] + x * sizeof(pixel_t)), G); // G
      __m128i R = _mm_unpacklo_epi64(RA1, RA2);
      _mm_store_si128(reinterpret_cast<__m128i *>(dstp[2] + x * sizeof(pixel_t)), R); // R
      if (targetHasAlpha)
        _mm_store_si128(reinterpret_cast<__m128i *>(dstp[3] + x * sizeof(pixel_t)), max_pixel_value); // A
    }
    // rest, unaligned but simd
    // RGB24/48 source is 3 bytes/pixel: no power-of-2 stride, 64-byte alignment does not guarantee width is a multiple of 16/8
    // So we cannot spare the last chunk
    if (wmod != width) {
      // width = 17: 0..7 8..15, 16
      // last_start = 1 (9..16 8 pixels)  width - pixels_at_a_time
      size_t x = (width - pixels_at_a_time);
      BGRA_1 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcp + x * 48 / pixels_at_a_time));
      BGRA_2 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcp + x * 48 / pixels_at_a_time + 16));
      BGRA_3 = _mm_loadu_si128(reinterpret_cast<const __m128i *>(srcp + x * 48 / pixels_at_a_time + 32));

      auto pack_lo = _mm_shuffle_epi8(BGRA_1, mask);  // 111111111111: BBBBGGGGRRRR and rest: BGRB | BBGGRR and rest: BBGG
      BGRA_1 = _mm_alignr_epi8(BGRA_2, BGRA_1, 12);
      auto pack_hi = _mm_shuffle_epi8(BGRA_1, mask); // 222222222222: BBBBGGGGRRRR | BBGGRR
      BGRA_2 = _mm_alignr_epi8(BGRA_3, BGRA_2, 8);
      auto pack_lo2 = _mm_shuffle_epi8(BGRA_2, mask); // 333333333333: BBBBGGGGRRRR | BBGGRR
      BGRA_3 = _mm_srli_si128(BGRA_3, 4); // to use the same mask
      auto pack_hi2 = _mm_shuffle_epi8(BGRA_3, mask); // 444444444444: BBBBGGGGRRRR | BBGGRR

      __m128i BG1 = _mm_unpacklo_epi32(pack_lo, pack_hi);  // BBBB_lo BBBB_hi GGGG_lo GGGG_hi
      __m128i BG2 = _mm_unpacklo_epi32(pack_lo2, pack_hi2);  // BBBB_lo BBBB_hi GGGG_lo GGGG_hi
      __m128i RA1 = _mm_unpackhi_epi32(pack_lo, pack_hi);   // RRRR_lo RRRR_hi AAAA_lo AAAA_hi
      __m128i RA2 = _mm_unpackhi_epi32(pack_lo2, pack_hi2);  // RRRR_lo RRRR_hi AAAA_lo AAAA_hi
      __m128i B = _mm_unpacklo_epi64(BG1, BG2);
      _mm_storeu_si128(reinterpret_cast<__m128i *>(dstp[1] + x * sizeof(pixel_t)), B); // B
      __m128i G = _mm_unpackhi_epi64(BG1, BG2);
      _mm_storeu_si128(reinterpret_cast<__m128i *>(dstp[0] + x * sizeof(pixel_t)), G); // G
      __m128i R = _mm_unpacklo_epi64(RA1, RA2);
      _mm_storeu_si128(reinterpret_cast<__m128i *>(dstp[2] + x * sizeof(pixel_t)), R); // R
      if (targetHasAlpha)
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dstp[3] + x * sizeof(pixel_t)), max_pixel_value); // A
    }
    srcp -= src_pitch; // source packed RGB is upside down
    dstp[0] += dst_pitch[0];
    dstp[1] += dst_pitch[1];
    dstp[2] += dst_pitch[2];
    if (targetHasAlpha)
      dstp[3] += dst_pitch[3];
  }
}

//instantiate
//template<typename pixel_t, bool targetHasAlpha>
template void convert_rgb_to_rgbp_ssse3<uint8_t, false>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgb_to_rgbp_ssse3<uint8_t, true>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgb_to_rgbp_ssse3<uint16_t, false>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgb_to_rgbp_ssse3<uint16_t, true>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);


// minimum width: 32 bytes (8 RGBA pixels for 8 bits, 4 RGBA pixels for 16 bits)
// RGB32/64 -> RGBP(A)8/16 SSE2 implementation. Ugly and not very quick.
template<typename pixel_t, bool targetHasAlpha>
void convert_rgba_to_rgbp_sse2(const BYTE* srcp, BYTE* (&dstp)[4],
  int src_pitch, int(&dst_pitch)[4], int width, int height)
{
  const int pixels_at_a_time = (sizeof(pixel_t) == 1) ? 8 : 4;
  __m128i zero = _mm_setzero_si128();

  for (int y = height; y > 0; --y) {
    // Avisynth's scanline alignment is 64 bytes, so no remainder handling is needed for 8-bit (16 pixels) and 16-bit (8 pixels) formats.
    for (int x = 0; x < width; x += pixels_at_a_time) {
      const BYTE* src = srcp + x * 4 * sizeof(pixel_t);
      __m128i lo = _mm_load_si128(reinterpret_cast<const __m128i*>(src));
      __m128i hi = _mm_load_si128(reinterpret_cast<const __m128i*>(src + 16));
      __m128i ch0, ch1;

      if constexpr (sizeof(pixel_t) == 1) {
        __m128i w0 = _mm_unpacklo_epi8(lo, zero);
        __m128i w1 = _mm_unpackhi_epi8(lo, zero);
        __m128i c0 = _mm_unpacklo_epi16(w0, w1);
        __m128i c1 = _mm_unpackhi_epi16(w0, w1);
        __m128i lo_bg = _mm_unpacklo_epi16(c0, c1);
        __m128i lo_ra = _mm_unpackhi_epi16(c0, c1);
        __m128i w2 = _mm_unpacklo_epi8(hi, zero);
        __m128i w3 = _mm_unpackhi_epi8(hi, zero);
        __m128i c2 = _mm_unpacklo_epi16(w2, w3);
        __m128i c3 = _mm_unpackhi_epi16(w2, w3);
        __m128i hi_bg = _mm_unpacklo_epi16(c2, c3);
        __m128i hi_ra = _mm_unpackhi_epi16(c2, c3);
        ch0 = _mm_shuffle_epi32(_mm_packus_epi16(lo_bg, hi_bg), _MM_SHUFFLE(3, 1, 2, 0));
        ch1 = _mm_shuffle_epi32(_mm_packus_epi16(lo_ra, hi_ra), _MM_SHUFFLE(3, 1, 2, 0));
      }
      else {
        __m128i t0 = _mm_unpacklo_epi16(lo, hi);
        __m128i t1 = _mm_unpackhi_epi16(lo, hi);
        ch0 = _mm_unpacklo_epi16(t0, t1);
        ch1 = _mm_unpackhi_epi16(t0, t1);
      }
      // Stores 8 byte per channel, so we can use storel_epi64. The high 64 bits of ch0/ch1 are ignored.
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp[1] + x * sizeof(pixel_t)), ch0); // B
      _mm_storeh_pd(reinterpret_cast<double*>(dstp[0] + x * sizeof(pixel_t)),_mm_castsi128_pd(ch0)); // G
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp[2] + x * sizeof(pixel_t)), ch1); // R
      if constexpr (targetHasAlpha)
        _mm_storeh_pd(reinterpret_cast<double*>(dstp[3] + x * sizeof(pixel_t)), _mm_castsi128_pd(ch1)); // A
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
template void convert_rgba_to_rgbp_sse2<uint8_t, false>(const BYTE*, BYTE* (&)[4], int, int(&)[4], int, int);
template void convert_rgba_to_rgbp_sse2<uint8_t, true>(const BYTE*, BYTE* (&)[4], int, int(&)[4], int, int);
template void convert_rgba_to_rgbp_sse2<uint16_t, false>(const BYTE*, BYTE* (&)[4], int, int(&)[4], int, int);
template void convert_rgba_to_rgbp_sse2<uint16_t, true>(const BYTE*, BYTE* (&)[4], int, int(&)[4], int, int);


template<typename pixel_t, bool targetHasAlpha>
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("ssse3")))
#endif
void convert_rgba_to_rgbp_ssse3(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height)
{
  const int pixels_at_a_time = (sizeof(pixel_t) == 1) ? 8 : 4;

  __m128i mask;
  if constexpr (sizeof(pixel_t) == 1)
    mask = _mm_set_epi8(15, 11, 7, 3, 14, 10, 6, 2, 13, 9, 5, 1, 12, 8, 4, 0);
  else
    mask = _mm_set_epi8(15, 14, 7, 6, 13, 12, 5, 4, 11, 10, 3, 2, 9, 8, 1, 0);

  for (int y = height; y > 0; --y) {
    // Avisynth's scanline alignment is 64 bytes, so no remainder handling is needed for 8 RGB32 or 4 RGB64 pixels
    for (int x = 0; x < width; x += pixels_at_a_time) {
      __m128i BGRA_lo = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 4 * sizeof(pixel_t)));
      __m128i BGRA_hi = _mm_load_si128(reinterpret_cast<const __m128i*>(srcp + x * 4 * sizeof(pixel_t) + 16));
      __m128i pack_lo = _mm_shuffle_epi8(BGRA_lo, mask);
      __m128i pack_hi = _mm_shuffle_epi8(BGRA_hi, mask);
      __m128i eightbytes_of_pixels = _mm_unpacklo_epi32(pack_lo, pack_hi);
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp[1] + x * sizeof(pixel_t)), eightbytes_of_pixels); // B
      _mm_storeh_pd(reinterpret_cast<double*>(dstp[0] + x * sizeof(pixel_t)), _mm_castsi128_pd(eightbytes_of_pixels)); // G
      eightbytes_of_pixels = _mm_unpackhi_epi32(pack_lo, pack_hi);
      _mm_storel_epi64(reinterpret_cast<__m128i*>(dstp[2] + x * sizeof(pixel_t)), eightbytes_of_pixels); // R
      if constexpr (targetHasAlpha)
        _mm_storeh_pd(reinterpret_cast<double*>(dstp[3] + x * sizeof(pixel_t)), _mm_castsi128_pd(eightbytes_of_pixels)); // A
    }
    srcp -= src_pitch;
    dstp[0] += dst_pitch[0];
    dstp[1] += dst_pitch[1];
    dstp[2] += dst_pitch[2];
    if constexpr (targetHasAlpha)
      dstp[3] += dst_pitch[3];
  }
}

//instantiate
//template<typename pixel_t, bool targetHasAlpha>
template void convert_rgba_to_rgbp_ssse3<uint8_t, false>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgba_to_rgbp_ssse3<uint8_t, true>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgba_to_rgbp_ssse3<uint16_t, false>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);
template void convert_rgba_to_rgbp_ssse3<uint16_t, true>(const BYTE* srcp, BYTE* (&dstp)[4], int src_pitch, int(&dst_pitch)[4], int width, int height);

template<typename pixel_t, bool hasSrcAlpha>
void convert_rgbp_to_rgba_sse2(const BYTE* (&srcp)[4], BYTE* dstp, int(&src_pitch)[4], int dst_pitch, int width, int height)
{
  const int pixels_at_a_time = 8 / sizeof(pixel_t); // 8x uint8_t, 4x uint16_t
  const __m128i transparent = _mm_set1_epi8((char)0xFF);
  // Avisynth's scanline alignment is 64 bytes, so no remainder handling is needed for 8 RGB32 or 4 RGB64 pixels
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x += pixels_at_a_time) {
      __m128i R, G, B, A;
      G = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp[0] + x * sizeof(pixel_t)));
      B = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp[1] + x * sizeof(pixel_t)));
      R = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp[2] + x * sizeof(pixel_t)));
      if constexpr (hasSrcAlpha)
        A = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(srcp[3] + x * sizeof(pixel_t)));
      else
        A = transparent;

      if constexpr (sizeof(pixel_t) == 1) {
        __m128i BG = _mm_unpacklo_epi8(B, G);
        __m128i RA = _mm_unpacklo_epi8(R, A);
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x * 4), _mm_unpacklo_epi16(BG, RA)); // pixels 0..3
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x * 4 + 16), _mm_unpackhi_epi16(BG, RA)); // pixels 4..7
      }
      else {
        __m128i BG = _mm_unpacklo_epi16(B, G);
        __m128i RA = _mm_unpacklo_epi16(R, A);
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x * 8), _mm_unpacklo_epi32(BG, RA)); // pixels 0..1
        _mm_store_si128(reinterpret_cast<__m128i*>(dstp + x * 8 + 16), _mm_unpackhi_epi32(BG, RA)); // pixels 2..3
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

//instantiate
//template<typename pixel_t, bool hasSrcAlpha>
template void convert_rgbp_to_rgba_sse2<uint8_t, false>(const BYTE* (&srcp)[4], BYTE* dstp, int(&src_pitch)[4], int dst_pitch, int width, int height);
template void convert_rgbp_to_rgba_sse2<uint8_t, true>(const BYTE* (&srcp)[4], BYTE* dstp, int(&src_pitch)[4], int dst_pitch, int width, int height);
template void convert_rgbp_to_rgba_sse2<uint16_t, false>(const BYTE* (&srcp)[4], BYTE* dstp, int(&src_pitch)[4], int dst_pitch, int width, int height);
template void convert_rgbp_to_rgba_sse2<uint16_t, true>(const BYTE* (&srcp)[4], BYTE* dstp, int(&src_pitch)[4], int dst_pitch, int width, int height);


