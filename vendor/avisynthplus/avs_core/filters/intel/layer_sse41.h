// AviSynth+  Copyright 2026- AviSynth+ Project
// SPDX-License-Identifier: GPL-2.0-or-later

// SSE4.1 Layer add dispatcher mirrors layer_avx2.h.
// Only integer hasAlpha=true use_chroma=true paths are accelerated;
// float/hasAlpha=false/use_chroma=false fall through to C templates.
// subtract is handled by pre-inverting the overlay in Layer::Create.

#ifndef __Layer_SSE41_H__
#define __Layer_SSE41_H__

#include <avisynth.h>
#include <stdint.h>
#include "../layer.h"   // layer_yuv_add_c_t, layer_yuv_add_f_c_t

void get_layer_yuv_masked_add_functions_sse41(
  bool is_chroma,
  int placement, VideoInfo& vi, int bits_per_pixel,
  layer_yuv_add_c_t** layer_fn,
  layer_yuv_add_f_c_t** layer_f_fn);

void get_layer_planarrgb_add_functions_sse41(
  bool chroma, bool hasAlpha, bool blendAlpha, int bits_per_pixel,
  layer_planarrgb_add_c_t** layer_fn,
  layer_planarrgb_add_f_c_t** layer_f_fn);

void get_layer_packedrgb_blend_functions_sse41(
  bool has_separate_mask, int bits_per_pixel,
  layer_packedrgb_blend_c_t** fn);

#endif  // __Layer_SSE41_H__
