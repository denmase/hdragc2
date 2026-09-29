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


// Avisynth filter: YUV merge
// by Klaus Post
// adapted by Richard Berg (avisynth-dev@richardberg.net)


#ifndef __Merge_H__
#define __Merge_H__

#include <avisynth.h>
#include "overlay/blend_common.h"
#ifdef INTEL_INTRINSICS
#include "overlay/intel/blend_common_avx2.h"
#include "overlay/intel/blend_common_sse.h"
#endif
#ifdef NEON_INTRINSICS
#include "overlay/aarch64/blend_common_neon.h"
#endif


/****************************************************
****************************************************/

class MergeChroma : public GenericVideoFilter
/**
  * Merge the chroma planes of one clip into another, preserving luma
 **/
{
public:
  MergeChroma(PClip _child, PClip _clip, float _weight, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    AVS_UNUSED(frame_range);
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl Create(AVSValue args, void* user_data, IScriptEnvironment* env);

private:
  PClip clip;
  float weight;
  int pixelsize;
  int bits_per_pixel;
};


class MergeLuma : public GenericVideoFilter
/**
  * Merge the luma plane of one clip into another, preserving chroma
 **/
{
public:
  MergeLuma(PClip _child, PClip _clip, float _weight, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    AVS_UNUSED(frame_range);
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl Create(AVSValue args, void* user_data, IScriptEnvironment* env);

private:
  PClip clip;
  float weight;
  int pixelsize;
  int bits_per_pixel;
};


class MergeAll : public GenericVideoFilter
/**
  * Merge the planes of one clip into another
 **/
{
public:
  MergeAll(PClip _child, PClip _clip, float _weight, IScriptEnvironment* env);
  PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env) override;

  int __stdcall SetCacheHints(int cachehints, int frame_range) override {
    AVS_UNUSED(frame_range);
    return cachehints == CACHE_GET_MTMODE ? MT_NICE_FILTER : 0;
  }

  static AVSValue __cdecl Create(AVSValue args, void* user_data, IScriptEnvironment* env);

private:
  PClip clip;
  float weight;
  int pixelsize;
  int bits_per_pixel;
};

void merge_plane(BYTE* srcp, const BYTE* otherp, int src_pitch, int other_pitch, int src_rowsize, int src_height, float weight, int bits_per_pixel, bool use_padded_width, IScriptEnvironment* env);

// Dispatch helpers for weighted (non-average) merges using the unified API.
// Callers compute:
//   pixelsize = bits_per_pixel <= 8 ? 1 : (bits_per_pixel == 32 ? 4 : 2)
//   width     = rowsize / pixelsize
//   weight_i  = (int)(weight_f * 32768.0f + 0.5f)
//   invweight_i = 32768 - weight_i
// Float clips use get_weighted_merge_float_fn; integer clips use get_weighted_merge_fn.
inline weighted_merge_fn_t* get_weighted_merge_fn(int cpuFlags, int weight_i) {
  if (weight_i == 0 || weight_i == 32768)
    return &weighted_merge_return_a_or_b;

#ifdef INTEL_INTRINSICS
  if (cpuFlags & CPUF_AVX2) return &weighted_merge_avx2;
  if (cpuFlags & CPUF_SSE2) return &weighted_merge_sse2;
#endif
#ifdef NEON_INTRINSICS
  if (cpuFlags & CPUF_ARM_NEON) return &weighted_merge_neon;
#endif
  return &weighted_merge_c;
}
inline weighted_merge_float_fn_t* get_weighted_merge_float_fn(int cpuFlags) {
#ifdef INTEL_INTRINSICS
  if (cpuFlags & CPUF_AVX2) return &weighted_merge_float_avx2;
  if (cpuFlags & CPUF_SSE2) return &weighted_merge_float_sse2;
#endif
#ifdef NEON_INTRINSICS
  if (cpuFlags & CPUF_ARM_NEON) return &weighted_merge_float_neon;
#endif
  return &weighted_merge_float_c;
}

#endif  // __Merge_H__
