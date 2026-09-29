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
#include "../core/internal.h"

#include <avs/alignment.h>
#include <avs/minmax.h>

#include "check_avx512.h" // compiler avx512 directives check
#include "resample_avx512.h"
#include <type_traits>

#include <immintrin.h> // Includes AVX-512 intrinsics

#include "resample_avx512.hpp"

// uint8_t h pretransposed_coeffs _vnni wrappers

void resize_h_planar_uint8_avx512_permutex_vstripe_mpz_ks4_pretransposed_coeffs_vnni(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  // template parameter true: UseVNNI
  resize_h_planar_uint8_avx512_permutex_vstripe_mpz_ks4_pretransposed_coeffs_internal<true>(dst8, src8, dst_pitch, src_pitch, program, width, height, bits_per_pixel);
}
void resize_h_planar_uint8_avx512_permutex_vstripe_mpz_ks8_pretransposed_coeffs_vnni(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  resize_h_planar_uint8_avx512_permutex_vstripe_mpz_ks8_pretransposed_coeffs_internal<true>(dst8, src8, dst_pitch, src_pitch, program, width, height, bits_per_pixel);
}
void resize_h_planar_uint8_avx512_permutex_vstripe_2s32_ks8_pretransposed_coeffs_vnni(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  resize_h_planar_uint8_avx512_permutex_vstripe_2s32_ks8_pretransposed_coeffs_internal<true>(dst8, src8, dst_pitch, src_pitch, program, width, height, bits_per_pixel);
}
void resize_h_planar_uint8_avx512_permutex_vstripe_mpz_ks16_pretransposed_coeffs_vnni(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  resize_h_planar_uint8_avx512_permutex_vstripe_mpz_ks16_pretransposed_coeffs_internal<true>(dst8, src8, dst_pitch, src_pitch, program, width, height, bits_per_pixel);
}
void resize_h_planar_uint8_avx512_permutex_vstripe_mpz_2s32_ks64_pretransposed_coeffs_vnni(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  resize_h_planar_uint8_avx512_permutex_vstripe_mpz_2s32_ks64_pretransposed_coeffs_internal<true>(dst8, src8, dst_pitch, src_pitch, program, width, height, bits_per_pixel);
}

// uint16_t h pretransposed_coeffs _vnni wrappers

template<bool lessthan16bit>
void resize_h_planar_uint16_avx512_permutex_vstripe_mp_2s32_ks4_pretransposed_coeffs_vnni(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  resize_h_planar_uint16_avx512_permutex_vstripe_mp_2s32_ks4_pretransposed_coeffs_internal<lessthan16bit, true>(dst8, src8, dst_pitch, src_pitch, program, width, height, bits_per_pixel);
}
template<bool lessthan16bit>
void resize_h_planar_uint16_avx512_permutex_vstripe_mp_2s32_ks8_pretransposed_coeffs_vnni(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  resize_h_planar_uint16_avx512_permutex_vstripe_mp_2s32_ks8_pretransposed_coeffs_internal<lessthan16bit, true>(dst8, src8, dst_pitch, src_pitch, program, width, height, bits_per_pixel);
}
template<bool lessthan16bit>
void resize_h_planar_uint16_avx512_permutex_vstripe_mp_4s16_ks8_pretransposed_coeffs_vnni(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  resize_h_planar_uint16_avx512_permutex_vstripe_mp_4s16_ks8_pretransposed_coeffs_internal<lessthan16bit, true>(dst8, src8, dst_pitch, src_pitch, program, width, height, bits_per_pixel);
}
template<bool lessthan16bit>
void resize_h_planar_uint16_avx512_permutex_vstripe_mp_ks16_pretransposed_coeffs_vnni(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  resize_h_planar_uint16_avx512_permutex_vstripe_mp_ks16_pretransposed_coeffs_internal<lessthan16bit, true>(dst8, src8, dst_pitch, src_pitch, program, width, height, bits_per_pixel);
}
template<bool lessthan16bit>
void resize_h_planar_uint16_avx512_permutex_vstripe_mp_4s16_ks48_pretransposed_coeffs_vnni(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel) {
  resize_h_planar_uint16_avx512_permutex_vstripe_mp_4s16_ks48_pretransposed_coeffs_internal<lessthan16bit, true>(dst8, src8, dst_pitch, src_pitch, program, width, height, bits_per_pixel);
}

// Explicit template instantiations for pretransposed uint16 _vnni variants
template void resize_h_planar_uint16_avx512_permutex_vstripe_mp_2s32_ks4_pretransposed_coeffs_vnni<false>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resize_h_planar_uint16_avx512_permutex_vstripe_mp_2s32_ks4_pretransposed_coeffs_vnni<true>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resize_h_planar_uint16_avx512_permutex_vstripe_mp_2s32_ks8_pretransposed_coeffs_vnni<false>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resize_h_planar_uint16_avx512_permutex_vstripe_mp_2s32_ks8_pretransposed_coeffs_vnni<true>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resize_h_planar_uint16_avx512_permutex_vstripe_mp_4s16_ks8_pretransposed_coeffs_vnni<false>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resize_h_planar_uint16_avx512_permutex_vstripe_mp_4s16_ks8_pretransposed_coeffs_vnni<true>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resize_h_planar_uint16_avx512_permutex_vstripe_mp_ks16_pretransposed_coeffs_vnni<false>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resize_h_planar_uint16_avx512_permutex_vstripe_mp_ks16_pretransposed_coeffs_vnni<true>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resize_h_planar_uint16_avx512_permutex_vstripe_mp_4s16_ks48_pretransposed_coeffs_vnni<false>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
template void resize_h_planar_uint16_avx512_permutex_vstripe_mp_4s16_ks48_pretransposed_coeffs_vnni<true>(BYTE* dst8, const BYTE* src8, int dst_pitch, int src_pitch, ResamplingProgram* program, int width, int height, int bits_per_pixel);
