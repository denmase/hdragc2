// masked_rowprep_avx2_impl.h
// Internal AVX2 helper — only include from TUs compiled with AVX2 flags.
// Provides simd_magic_div_32_avx2 inline + the rowprep declarations.

#pragma once

#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <immintrin.h>
#endif

#include "masked_rowprep_avx2.h"

// ---------------------------------------------------------------------------
// simd_magic_div_32_avx2
// Inline here: used by masked_rowprep_avx2.cpp and masked_merge_avx2_impl.hpp
// in inner loops where it must remain inlineable.
// ---------------------------------------------------------------------------
#if defined(GCC) || defined(CLANG)
__attribute__((__target__("avx2")))
#endif
static AVS_FORCEINLINE __m256i simd_magic_div_32_avx2(__m256i val, uint32_t magic, int shift) {
  __m256i v_magic  = _mm256_set1_epi64x(magic);
  __m256i res_even = _mm256_mul_epu32(val, v_magic);
  __m256i res_odd  = _mm256_mul_epu32(_mm256_srli_si256(val, 4), v_magic);
  res_even = _mm256_srli_epi64(res_even, 32 + shift);
  res_odd  = _mm256_srli_epi64(res_odd,  32 + shift);
  __m256i res_odd_shifted = _mm256_slli_epi64(res_odd, 32);
  return _mm256_blend_epi32(res_even, res_odd_shifted, 0xAA);
}
