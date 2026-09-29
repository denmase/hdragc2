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

#ifndef __Planeswap_AVX2_H__
#define __Planeswap_AVX2_H__

#include <avs/types.h>

// Packed RGB32/RGB64 single-channel extraction — AVX2.
// srcp: last (bottom) row of source.  Pitch is positive; each row
// steps srcp -= src_pitch upward.  channel_index: B=0 G=1 R=2 A=3.
// Width must be a multiple of 16 (RGB32) / 8 (RGB64) — guaranteed by 64-byte alignment.
template<int channel_index>
void extract_packed_rgb32_channel_avx2(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height);

template<int channel_index>
void extract_packed_rgb64_channel_avx2(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height);

// Packed RGB24/RGB48 single-channel extraction — AVX2.
// pixel_t = uint8_t (RGB24) or uint16_t (RGB48).  channel_index: B=0 G=1 R=2.
// Processes 32 pixels (RGB24) or 16 pixels (RGB48) per main iteration, both consuming
// 96 source bytes.  Caller must ensure width >= 32 (RGB24) or >= 16 (RGB48).
// Remainder (width not a multiple of pixels_per_iter) handled by overlapping re-run.
#include <cstdint>
template<typename pixel_t, int channel_index>
void extract_packed_rgb_noalpha_channel_avx2(const BYTE* srcp, BYTE* dstp, int src_pitch, int dst_pitch, int width, int height);

#endif  // __Planeswap_AVX2_H__
