#ifndef __Merge_AVX2_H__
#define __Merge_AVX2_H__

#include <avs/types.h>

template<typename pixel_t>
void average_plane_avx2(BYTE *p1, const BYTE *p2, int p1_pitch, int p2_pitch, int rowsize, int height);
void average_plane_avx2_float(BYTE* p1, const BYTE* p2, int p1_pitch, int p2_pitch, int rowsize, int height);

#endif  // __MergeAVX2_H__
