// AviSynth+.  Copyright 2026- AviSynth+ Project
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

#include "../turn.h"
#include "turn_avx2.h"

#if defined(_MSC_VER)
#include <intrin.h> // MSVC
#else 
#include <x86intrin.h> // GCC/MinGW/Clang/LLVM
#endif
#include <immintrin.h>

#include <cstdint>

//-------------------------------------------------------------------------------------------------
// 8-bit Transpose Kernels
//-------------------------------------------------------------------------------------------------

// The final winner over 8x8, 16x8 and 8x32.
// Transpose 32x8 (input) -> 8x32 (output)
// Input: 32 rows of 8 pixels (uint8_t)
// Output: 8 rows of 32 pixels
static AVS_FORCEINLINE void transpose_32x8x8_avx2(const uint8_t* AVS_RESTRICT srcp, uint8_t* AVS_RESTRICT dstp, const int src_pitch, const int dst_pitch)
{
  // Load 32 rows, 8 bytes each
  // One 256 bit register contains 4x8 bytes, which are 4 rows apart, so A,E,I,M in reg0, B,F,J,N in reg1, etc.
  // after reaching the 16th row, it continues with Q,U,Y,c in reg4, R,V,Z,d in reg5, etc.
  auto load_quad_row = [](const uint8_t* base, int pitch, int offset) -> __m256i {
    __m128i r0 = _mm_loadu_si64((const __m128i*)(base + (offset + 0 * 4) * pitch));
    __m128i r1 = _mm_loadu_si64((const __m128i*)(base + (offset + 1 * 4) * pitch));
    __m128i r2 = _mm_loadu_si64((const __m128i*)(base + (offset + 2 * 4) * pitch));
    __m128i r3 = _mm_loadu_si64((const __m128i*)(base + (offset + 3 * 4) * pitch));

    __m128i r01 = _mm_unpacklo_epi64(r0, r1);
    __m128i r23 = _mm_unpacklo_epi64(r2, r3);
    return _mm256_inserti128_si256(_mm256_castsi128_si256(r01), r23, 1);
  };

  // Load 32 rows into 8 registers
  __m256i r0 = load_quad_row(srcp, src_pitch, 0);  // Rows 0,4,8,12
  __m256i r1 = load_quad_row(srcp, src_pitch, 1);  // Rows 1,5,9,13
  __m256i r2 = load_quad_row(srcp, src_pitch, 2);  // Rows 2,6,10,14
  __m256i r3 = load_quad_row(srcp, src_pitch, 3);  // Rows 3,7,11,15
  __m256i r4 = load_quad_row(srcp, src_pitch, 16); // Rows 16,20,24,28
  __m256i r5 = load_quad_row(srcp, src_pitch, 17); // Rows 17,21,25,29
  __m256i r6 = load_quad_row(srcp, src_pitch, 18); // Rows 18,22,26,30
  __m256i r7 = load_quad_row(srcp, src_pitch, 19); // Rows 19,23,27,31

  __m256i t0 = _mm256_unpacklo_epi8(r0, r1);
  __m256i t1 = _mm256_unpackhi_epi8(r0, r1);
  __m256i t2 = _mm256_unpacklo_epi8(r2, r3);
  __m256i t3 = _mm256_unpackhi_epi8(r2, r3);
  __m256i t4 = _mm256_unpacklo_epi8(r4, r5);
  __m256i t5 = _mm256_unpackhi_epi8(r4, r5);
  __m256i t6 = _mm256_unpacklo_epi8(r6, r7);
  __m256i t7 = _mm256_unpackhi_epi8(r6, r7);

  __m256i u0 = _mm256_unpacklo_epi16(t0, t2);
  __m256i u1 = _mm256_unpacklo_epi16(t1, t3);
  __m256i u2 = _mm256_unpackhi_epi16(t0, t2);
  __m256i u3 = _mm256_unpackhi_epi16(t1, t3);
  __m256i u4 = _mm256_unpacklo_epi16(t4, t6);
  __m256i u5 = _mm256_unpacklo_epi16(t5, t7);
  __m256i u6 = _mm256_unpackhi_epi16(t4, t6);
  __m256i u7 = _mm256_unpackhi_epi16(t5, t7);

  __m256i v0 = _mm256_unpacklo_epi32(u0, u1);
  __m256i v1 = _mm256_unpackhi_epi32(u0, u1);
  __m256i v2 = _mm256_unpacklo_epi32(u2, u3);
  __m256i v3 = _mm256_unpackhi_epi32(u2, u3);
  __m256i v4 = _mm256_unpacklo_epi32(u4, u5);
  __m256i v5 = _mm256_unpackhi_epi32(u4, u5);
  __m256i v6 = _mm256_unpacklo_epi32(u6, u7);
  __m256i v7 = _mm256_unpackhi_epi32(u6, u7);

  __m256i w0 = _mm256_unpacklo_epi64(v0, v4); // AH0, QX0, IP0, Yf0
  __m256i w1 = _mm256_unpackhi_epi64(v0, v4); // AH1, QX1, IP1, Yf1
  __m256i w2 = _mm256_unpacklo_epi64(v1, v5); // AH2, QX2, IP2, Yf2
  __m256i w3 = _mm256_unpackhi_epi64(v1, v5); // AH3, QX3, IP3, Yf3
  __m256i w4 = _mm256_unpacklo_epi64(v2, v6); // AH4, QX4, IP4, Yf4
  __m256i w5 = _mm256_unpackhi_epi64(v2, v6); // AH5, QX5, IP5, Yf5
  __m256i w6 = _mm256_unpacklo_epi64(v3, v7); // AH6, QX6, IP6, Yf6
  __m256i w7 = _mm256_unpackhi_epi64(v3, v7); // AH7, QX7, IP7, Yf7

  // Cross-lane permute to fix AVX2 dual-lane unpack.
  // Arrange 64-bit blocks into the correct linear order
  const int PERM_CTRL = 0b11'01'10'00; // Standard 0,2,1,3 permute
  __m256i out0 = _mm256_permute4x64_epi64(w0, PERM_CTRL); // AH0 | IP0 | QX0 | Yf0: ABCEFGHIJKLMNOPQRTSUVWXYZabcdef_0 ready
  __m256i out1 = _mm256_permute4x64_epi64(w1, PERM_CTRL); // AH1 | IP1 | QX1 | Yf1
  __m256i out2 = _mm256_permute4x64_epi64(w2, PERM_CTRL); // AH2 | IP2 | QX2 | Yf2
  __m256i out3 = _mm256_permute4x64_epi64(w3, PERM_CTRL); // AH3 | IP3 | QX3 | Yf3
  __m256i out4 = _mm256_permute4x64_epi64(w4, PERM_CTRL); // AH4 | IP4 | QX4 | Yf4
  __m256i out5 = _mm256_permute4x64_epi64(w5, PERM_CTRL); // AH5 | IP5 | QX5 | Yf5
  __m256i out6 = _mm256_permute4x64_epi64(w6, PERM_CTRL); // AH6 | IP6 | QX6 | Yf6
  __m256i out7 = _mm256_permute4x64_epi64(w7, PERM_CTRL); // AH7 | IP7 | QX7 | Yf7

  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + 0 * dst_pitch), out0);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + 1 * dst_pitch), out1);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + 2 * dst_pitch), out2);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + 3 * dst_pitch), out3);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + 4 * dst_pitch), out4);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + 5 * dst_pitch), out5);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + 6 * dst_pitch), out6);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + 7 * dst_pitch), out7);
}

void turn_right_plane_8_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  // Start at the BOTTOM left of the source
  const uint8_t* s0 = srcp + src_pitch * (src_height - 1);

  constexpr int PIXELS_W = 8; // width block size
  constexpr int PIXELS_H = 32; // height block size

  const int w = src_rowsize & ~(PIXELS_W * sizeof(uint8_t) - 1);
  const int h = src_height & ~(PIXELS_H - 1);
  const int simd_width_in_pixels = w / sizeof(uint8_t);

  for (int y = 0; y < h; y += PIXELS_H)
  {
    // Destination starts at y offset
    uint8_t* d0 = dstp + y;

    for (int x = 0; x < w; x += PIXELS_W * sizeof(uint8_t))
    {
      // Use negative pitch to load rows bottom-to-top to avoid row reversing after transpose.
      transpose_32x8x8_avx2(s0 + x, d0, -src_pitch, dst_pitch);

      d0 += PIXELS_W * dst_pitch;
    }
    s0 -= PIXELS_H * src_pitch;
  }

  // Boundary handling fallback
  if (src_rowsize != w)
  {
    // consider calling the sse2 version when difference is at least 8, since sse2 does 8x8 blocks
    turn_right_plane_8_c(srcp + w, dstp + dst_pitch * simd_width_in_pixels, src_rowsize - w, src_height, src_pitch, dst_pitch);
  }

  if (src_height != h)
  {
    turn_right_plane_8_c(srcp, dstp + h * sizeof(uint8_t), src_rowsize, src_height - h, src_pitch, dst_pitch);
  }
}

void turn_left_plane_8_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  turn_right_plane_8_avx2(srcp + src_pitch * (src_height - 1), dstp + dst_pitch * (src_rowsize / sizeof(uint8_t) - 1), src_rowsize, src_height, -src_pitch, -dst_pitch);
}

//-------------------------------------------------------------------------------------------------
// 16-bit Transpose Kernels
//-------------------------------------------------------------------------------------------------

// Transpose 8x16 (input) -> 16x8 (output)
// Input: 16 rows of 8 pixels (uint16_t)
// Output: 8 rows of 16 pixels
static AVS_FORCEINLINE void transpose_16x8x16_avx2(const BYTE* AVS_RESTRICT srcp, BYTE* AVS_RESTRICT dstp, const int src_pitch, const int dst_pitch)
{
  // Helper to load 16 bytes from row i (low lane) and row i+8 (high lane)
  auto load_dual_row_avx2 = [](const BYTE* base, int pitch, int row_idx) -> __m256i
  {
    __m128i lo = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base + pitch * row_idx));
    __m128i hi = _mm_loadu_si128(reinterpret_cast<const __m128i*>(base + pitch * (row_idx + 8)));
    return _mm256_inserti128_si256(_mm256_castsi128_si256(lo), hi, 1);
  };

  // Load 16 rows into 8 registers.
  // Row x in low lane and row x+8 in high lane.
  __m256i v0 = load_dual_row_avx2(srcp, src_pitch, 0); // r0 | r8 // A0..A7 | I0..I7
  __m256i v1 = load_dual_row_avx2(srcp, src_pitch, 1); // r1 | r9 // B0..B7 | J0..J7
  __m256i v2 = load_dual_row_avx2(srcp, src_pitch, 2); // r2 | r10 // C0..C7 | K0..K7
  __m256i v3 = load_dual_row_avx2(srcp, src_pitch, 3); // r3 | r11 // D0..D7 | L0..L7
  __m256i v4 = load_dual_row_avx2(srcp, src_pitch, 4); // r4 | r12 // E0..E7 | M0..M7
  __m256i v5 = load_dual_row_avx2(srcp, src_pitch, 5); // r5 | r13 // F0..F7 | N0..N7
  __m256i v6 = load_dual_row_avx2(srcp, src_pitch, 6); // r6 | r14 // G0..G7 | O0..O7
  __m256i v7 = load_dual_row_avx2(srcp, src_pitch, 7); // r7 | r15 // H0..H7 | P0..P7

  // Standard 8x8 Transpose Logic (applied to both lanes simultaneously)
  __m256i t0 = _mm256_unpacklo_epi16(v0, v1); // A0 B0 A1 B1 A2 B2 A3 B3 | I0 J0 I1 J1 I2 J2 I3 J3
  __m256i t1 = _mm256_unpackhi_epi16(v0, v1); // A4 B4 A5 B5 A6 B6 A7 B7 | I4 J4 I5 J5 I6 J6 I7 J7
  __m256i t2 = _mm256_unpacklo_epi16(v2, v3); // C0 D0 C1 D1 C2 D2 C3 D3 | K0 L0 K1 L1 K2 L2 K3 L3
  __m256i t3 = _mm256_unpackhi_epi16(v2, v3); // C4 D4 C5 D5 C6 D6 C7 D7 | K4 L4 K5 L5 K6 L6 K7 L7
  __m256i t4 = _mm256_unpacklo_epi16(v4, v5); // E0 F0 E1 F1 E2 F2 E3 F3 | M0 N0 M1 N1 M2 N2 M3 N3
  __m256i t5 = _mm256_unpackhi_epi16(v4, v5); // E4 F4 E5 F5 E6 F6 E7 F7 | M4 N4 M5 N5 M6 N6 M7 N7
  __m256i t6 = _mm256_unpacklo_epi16(v6, v7); // G0 H0 G1 H1 G2 H2 G3 H3 | O0 P0 O1 P1 O2 P2 O3 P3
  __m256i t7 = _mm256_unpackhi_epi16(v6, v7); // G4 H4 G5 H5 G6 H6 G7 H7 | O4 P4 O5 P5 O6 P6 O7 P7

  __m256i m0 = _mm256_unpacklo_epi32(t0, t2); // AD0,AD1 | IL0,IL1
  __m256i m1 = _mm256_unpackhi_epi32(t0, t2); // AD2,AD3 | IL2,IL3
  __m256i m2 = _mm256_unpacklo_epi32(t1, t3); // AD4,AD5 | IL4,IL5
  __m256i m3 = _mm256_unpackhi_epi32(t1, t3); // AD6,AD7 | IL6,IL7
  __m256i m4 = _mm256_unpacklo_epi32(t4, t6); // EH0,EH1 | MP0,MP1
  __m256i m5 = _mm256_unpackhi_epi32(t4, t6); // EH2,EH3 | MP2,MP3
  __m256i m6 = _mm256_unpacklo_epi32(t5, t7); // EH4,EH5 | MP4,MP5
  __m256i m7 = _mm256_unpackhi_epi32(t5, t7); // EH6,EH7 | MP6,MP7

  __m256i out0 = _mm256_unpacklo_epi64(m0, m4); // AD0,EH0 | IL0,MP0
  __m256i out1 = _mm256_unpackhi_epi64(m0, m4); // AD1,EH1 | IL1,MP1
  __m256i out2 = _mm256_unpacklo_epi64(m1, m5); // AD2,EH2 | IL2,MP2
  __m256i out3 = _mm256_unpackhi_epi64(m1, m5); // AD3,EH3 | IL3,MP3
  __m256i out4 = _mm256_unpacklo_epi64(m2, m6); // AD4,EH4 | IL4,MP4
  __m256i out5 = _mm256_unpackhi_epi64(m2, m6); // AD5,EH5 | IL5,MP5
  __m256i out6 = _mm256_unpacklo_epi64(m3, m7); // AD6,EH6 | IL6,MP6
  __m256i out7 = _mm256_unpackhi_epi64(m3, m7); // AD7,EH7 | IL7,MP7

  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 0), out0);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 1), out1);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 2), out2);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 3), out3);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 4), out4);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 5), out5);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 6), out6);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 7), out7);
}

void turn_right_plane_16_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  // source order: bottom to top
  const BYTE* s0 = srcp + src_pitch * (src_height - 1);

  constexpr int PIXELS_W = 8; // width block size
  constexpr int PIXELS_H = 16; // height block size

  const int w = src_rowsize & ~(PIXELS_W * sizeof(uint16_t) - 1);
  const int h = src_height & ~(PIXELS_H - 1);
  const int simd_width_in_pixels = w / sizeof(uint16_t);

  for (int y = 0; y < h; y += PIXELS_H)
  {
    BYTE* d0 = dstp + y * sizeof(uint16_t);

    for (int x = 0; x < w; x += PIXELS_W * sizeof(uint16_t))
    {
      // Use negative pitch to load rows bottom-to-top to avoid row reversing after transpose.
      transpose_16x8x16_avx2(s0 + x, d0, -src_pitch, dst_pitch);
      d0 += PIXELS_W * dst_pitch;
    }
    s0 -= PIXELS_H * src_pitch;
  }

  // Boundary handling
  if (src_rowsize != w)
  {
    turn_right_plane_16_c(srcp + w, dstp + dst_pitch * simd_width_in_pixels, src_rowsize - w, src_height, src_pitch, dst_pitch);
  }

  if (src_height != h)
  {
    turn_right_plane_16_c(srcp, dstp + h * sizeof(uint16_t), src_rowsize, src_height - h, src_pitch, dst_pitch);
  }
}

void turn_left_plane_16_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  turn_right_plane_16_avx2(srcp + src_pitch * (src_height - 1), dstp + dst_pitch * (src_rowsize / sizeof(uint16_t) - 1), src_rowsize, src_height, -src_pitch, -dst_pitch);
}

//-------------------------------------------------------------------------------------------------
// 32-bit Transpose Kernels (Float/RGB32)
//-------------------------------------------------------------------------------------------------

static AVS_FORCEINLINE void transpose_8x8_32bit_avx2(const BYTE* AVS_RESTRICT srcp, BYTE* AVS_RESTRICT dstp, int src_pitch, int dst_pitch)
{
  // load 8 rows of 8 pixels (32-bit each)
  __m256i r0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 0));
  __m256i r1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 1));
  __m256i r2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 2));
  __m256i r3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 3));
  __m256i r4 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 4));
  __m256i r5 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 5));
  __m256i r6 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 6));
  __m256i r7 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 7));

  __m256i t0 = _mm256_unpacklo_epi32(r0, r1);
  __m256i t1 = _mm256_unpackhi_epi32(r0, r1);
  __m256i t2 = _mm256_unpacklo_epi32(r2, r3);
  __m256i t3 = _mm256_unpackhi_epi32(r2, r3);
  __m256i t4 = _mm256_unpacklo_epi32(r4, r5);
  __m256i t5 = _mm256_unpackhi_epi32(r4, r5);
  __m256i t6 = _mm256_unpacklo_epi32(r6, r7);
  __m256i t7 = _mm256_unpackhi_epi32(r6, r7);

  __m256i q0 = _mm256_unpacklo_epi64(t0, t2);
  __m256i q1 = _mm256_unpackhi_epi64(t0, t2);
  __m256i q2 = _mm256_unpacklo_epi64(t1, t3);
  __m256i q3 = _mm256_unpackhi_epi64(t1, t3);
  __m256i q4 = _mm256_unpacklo_epi64(t4, t6);
  __m256i q5 = _mm256_unpackhi_epi64(t4, t6);
  __m256i q6 = _mm256_unpacklo_epi64(t5, t7);
  __m256i q7 = _mm256_unpackhi_epi64(t5, t7);

  // cross-lane permute
  // out0..3 gets low lane of (rows 0-3) and low lane of (rows 4-7)
  // out4..7 gets high lane of (rows 0-3) and high lane of (rows 4-7)
  __m256i out0 = _mm256_permute2x128_si256(q0, q4, 0x20);
  __m256i out1 = _mm256_permute2x128_si256(q1, q5, 0x20);
  __m256i out2 = _mm256_permute2x128_si256(q2, q6, 0x20);
  __m256i out3 = _mm256_permute2x128_si256(q3, q7, 0x20);
  __m256i out4 = _mm256_permute2x128_si256(q0, q4, 0x31);
  __m256i out5 = _mm256_permute2x128_si256(q1, q5, 0x31);
  __m256i out6 = _mm256_permute2x128_si256(q2, q6, 0x31);
  __m256i out7 = _mm256_permute2x128_si256(q3, q7, 0x31);

  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 0), out0);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 1), out1);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 2), out2);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 3), out3);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 4), out4);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 5), out5);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 6), out6);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 7), out7);
}

// float, rgb32
void turn_right_plane_32_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  // source order: bottom to top
  const BYTE* s0 = srcp + src_pitch * (src_height - 1);

  constexpr int PIXELS_W = 8; // width block size
  constexpr int PIXELS_H = 8; // height block size

  const int w = src_rowsize & ~(PIXELS_W * sizeof(uint32_t) - 1);
  const int h = src_height & ~(PIXELS_H - 1);
  const int simd_width_in_pixels = w / sizeof(uint32_t);

  for (int y = 0; y < h; y += PIXELS_H)
  {
    // Destination grows forward: y index becomes the x-offset in the rotated image
    BYTE* d0 = dstp + y * sizeof(uint32_t);

    for (int x = 0; x < w; x += PIXELS_W * sizeof(uint32_t))
    {
      // Use negative pitch to load rows bottom-to-top to avoid row reversing after transpose.
      transpose_8x8_32bit_avx2(s0 + x, d0, -src_pitch, dst_pitch);
      d0 += PIXELS_W * dst_pitch;
    }
    s0 -= PIXELS_H * src_pitch;
  }

  // Boundary handling fallbacks
  if (src_rowsize != w)
  {
    turn_right_plane_32_c(srcp + w, dstp + dst_pitch * simd_width_in_pixels, src_rowsize - w, src_height, src_pitch, dst_pitch);
  }
  if (src_height != h)
  {
    turn_right_plane_32_c(srcp, dstp + h * sizeof(uint32_t), src_rowsize, src_height - h, src_pitch, dst_pitch);
  }
}

void turn_left_plane_32_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  turn_right_plane_32_avx2(srcp + src_pitch * (src_height - 1), dstp + dst_pitch * (src_rowsize / sizeof(float) - 1), src_rowsize, src_height, -src_pitch, -dst_pitch);
}

void turn_left_rgb32_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  // packed rgb is upside down
  turn_right_plane_32_avx2(srcp, dstp, src_rowsize, src_height, src_pitch, dst_pitch);
}

void turn_right_rgb32_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  // packed rgb is upside down
  turn_left_plane_32_avx2(srcp, dstp, src_rowsize, src_height, src_pitch, dst_pitch);
}

//-------------------------------------------------------------------------------------------------
// 64-bit Transpose Kernels (RGB64)
//-------------------------------------------------------------------------------------------------

static AVS_FORCEINLINE void transpose_4x4_64bit_avx2(const BYTE* AVS_RESTRICT srcp, BYTE* AVS_RESTRICT dstp, int src_pitch, int dst_pitch)
{
  // Load 4 rows (4 pixels each reg = 32 bytes = 1 YMM register)
  __m256i r0 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 0));
  __m256i r1 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 1));
  __m256i r2 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 2));
  __m256i r3 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(srcp + src_pitch * 3));

  __m256i t0 = _mm256_unpacklo_epi64(r0, r1); // [R0_P0, R1_P0 | R0_P2, R1_P2]
  __m256i t1 = _mm256_unpackhi_epi64(r0, r1); // [R0_P1, R1_P1 | R0_P3, R1_P3]
  __m256i t2 = _mm256_unpacklo_epi64(r2, r3); // [R2_P0, R3_P0 | R2_P2, R3_P2]
  __m256i t3 = _mm256_unpackhi_epi64(r2, r3); // [R2_P1, R3_P1 | R2_P3, R3_P3]

  // Cross lane permute
  // Combine low lanes of t0/t2, t1/t3 and high lanes of t0/t2, t1/t3
  __m256i d0 = _mm256_permute2x128_si256(t0, t2, 0x20); // Column 0: [R0-R3]_P0
  __m256i d1 = _mm256_permute2x128_si256(t1, t3, 0x20); // Column 1: [R0-R3]_P1
  __m256i d2 = _mm256_permute2x128_si256(t0, t2, 0x31); // Column 2: [R0-R3]_P2
  __m256i d3 = _mm256_permute2x128_si256(t1, t3, 0x31); // Column 3: [R0-R3]_P3

  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 0), d0);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 1), d1);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 2), d2);
  _mm256_storeu_si256(reinterpret_cast<__m256i*>(dstp + dst_pitch * 3), d3);
}

void turn_right_plane_64_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  // Start at bottom row
  const BYTE* s0 = srcp + src_pitch * (src_height - 1);

  constexpr int PIXELS_W = 4;
  constexpr int PIXELS_H = 4;

  const int w = src_rowsize & ~(PIXELS_W * sizeof(uint64_t) - 1);
  const int h = src_height & ~(PIXELS_H - 1);
  const int simd_width_in_pixels = w / sizeof(uint64_t);

  for (int y = 0; y < h; y += PIXELS_H)
  {
    // Destination starts at y offset (which is the x-coordinate in turn-right)
    BYTE* d0 = dstp + y * sizeof(uint64_t);

    for (int x = 0; x < w; x += PIXELS_W * sizeof(uint64_t))
    {
      // Standard transpose + Negative Pitch = 90 CW Turn
      transpose_4x4_64bit_avx2(s0 + x, d0, -src_pitch, dst_pitch);
      d0 += PIXELS_W * dst_pitch;
    }
    s0 -= PIXELS_H * src_pitch;
  }

  if (src_rowsize != w)
  {
    turn_right_plane_c<uint64_t>(srcp + w, dstp + dst_pitch * simd_width_in_pixels, src_rowsize - w, src_height, src_pitch, dst_pitch);
  }
  if (src_height != h)
  {
    turn_right_plane_c<uint64_t>(srcp, dstp + h * sizeof(uint64_t), src_rowsize, src_height - h, src_pitch, dst_pitch);
  }

}

void turn_left_rgb64_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  // packed rgb is upside down
  turn_right_plane_64_avx2(srcp, dstp, src_rowsize, src_height, src_pitch, dst_pitch);
}

void turn_right_rgb64_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  // packed rgb is upside down
  turn_right_plane_64_avx2(srcp + src_pitch * (src_height - 1), dstp + dst_pitch * (src_rowsize / sizeof(uint64_t) - 1), src_rowsize, src_height, -src_pitch, -dst_pitch);
}

//-------------------------------------------------------------------------------------------------
// Turn 180 (Flip)
//-------------------------------------------------------------------------------------------------

template <typename T>
void turn_180_plane_avx2(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch)
{
  const BYTE* AVS_RESTRICT s0 = srcp;
  const int row_bytes = src_rowsize; // src_rowsize is the byte width of the row.

  // d0 points to the byte after the last pixel (byte) of the last row of dst.
  // dst_pitch * (src_height - 1) is the start of the last row.
  // + row_bytes is the address after the last byte of that row.
  BYTE* AVS_RESTRICT d0 = dstp + dst_pitch * (src_height - 1) + row_bytes;

  constexpr int vector_bytes = 32;
  const int aligned_row_bytes = row_bytes & ~(vector_bytes - 1);

  // Prepare Shuffle Masks (These masks are correct for byte reversal within lanes)
  __m256i shuf_mask; // only for 8/16-bit types
  if constexpr (sizeof(T) == 1) {
    // Reverse bytes in 16-byte chunks (31..16 and 15..0)
    shuf_mask = _mm256_set_epi8(
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
    );
  }
  else if constexpr (sizeof(T) == 2) {
    // Reverse words (16-bit elements) in 16-byte chunks
    shuf_mask = _mm256_set_epi8(
      1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14,
      1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14
    );
  }
  // For 32/64 we use permute instructions, no shuffle mask needed.

  for (int y = 0; y < src_height; ++y)
  {
    // Main AVX2 Loop
    for (int x = 0; x < aligned_row_bytes; x += vector_bytes)
    {
      __m256i src = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(s0 + x));

      if constexpr (sizeof(T) == 8) { // uint64_t (4 pixels)
        // Swap 64-bit elements: 3, 2, 1, 0
        src = _mm256_permute4x64_epi64(src, _MM_SHUFFLE(0, 1, 2, 3));
      }
      else if constexpr (sizeof(T) == 4) { // uint32_t (8 pixels)
        // Reverse 32-bit elements: 7, 6, 5, 4, 3, 2, 1, 0
        const __m256i idx = _mm256_set_epi32(0, 1, 2, 3, 4, 5, 6, 7);
        src = _mm256_permutevar8x32_epi32(src, idx);
      }
      else { // uint8_t or uint16_t
        // reverse within 128-bit lanes (using precomputed mask)
        src = _mm256_shuffle_epi8(src, shuf_mask);
        // Swap the 128-bit lanes
        src = _mm256_permute2x128_si256(src, src, 0x01);
      }

      // Store backwards: d0 points to end, x is byte offset from start.
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(d0 - x - vector_bytes), src);
    }

    // fallback for leftovers 
    int x = aligned_row_bytes;
    int rem_bytes = row_bytes - x;

    if (rem_bytes > 0) {
      const BYTE* src_tail = s0 + x;

      // Destination pointer: The first pixel of the remainder (src_tail)
      // must go to the address *just before* where the AVX loop finished writing.
      // The AVX loop finished writing at: d0 - aligned_row_bytes.
      // The remaining bytes (rem_bytes) are written to fill the gap.
      // Start of destination remainder region: d0 - row_bytes
      BYTE* dst_rem_start = d0 - row_bytes;

      const int n_pixels = rem_bytes / sizeof(T);

      // Pointers for pixel-wise copy and reverse.
      const T* s_ptr = reinterpret_cast<const T*>(src_tail);

      // d_ptr starts at the last pixel position of the remainder region, 
      // and moves backward to reverse the order.
      T* d_ptr = reinterpret_cast<T*>(dst_rem_start + rem_bytes) - 1;

      for (int k = 0; k < n_pixels; ++k) {
        *d_ptr = s_ptr[k];
        d_ptr--; // Move backward
      }
    }
    s0 += src_pitch;
    d0 -= dst_pitch;
  }
}

// Instantiate templates
template void turn_180_plane_avx2<uint8_t>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch);
template void turn_180_plane_avx2<uint16_t>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch);
template void turn_180_plane_avx2<uint32_t>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch);
template void turn_180_plane_avx2<uint64_t>(const BYTE* srcp, BYTE* dstp, int src_rowsize, int src_height, int src_pitch, int dst_pitch);
