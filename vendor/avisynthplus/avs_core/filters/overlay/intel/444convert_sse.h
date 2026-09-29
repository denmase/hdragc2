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

#ifndef __444Convert_sse_h
#define __444Convert_sse_h

#include <avs/types.h>

// YV12 -> YV24 chroma upsamplers (typed wrappers around SSE2 uint8_t/uint16_t templates)
void conv_yv12_to_yv24_chroma_u8_sse2 (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int src_width, int src_height);
void conv_yv12_to_yv24_chroma_u16_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int src_width, int src_height);

// YV16 -> YV24 chroma upsamplers (typed wrappers around SSE2 uint8_t/uint16_t templates)
void conv_yv16_to_yv24_chroma_u8_sse2 (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int src_width, int src_height);
void conv_yv16_to_yv24_chroma_u16_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int src_width, int src_height);

// YV24 -> YV12 chroma downsamplers
void convert_yv24_chroma_to_yv12_u8_ssse3               (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);
void convert_yv24_chroma_to_yv12_u16_lessthan16bit_ssse3 (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);
void convert_yv24_chroma_to_yv12_u16_sse41              (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);
void convert_yv24_chroma_to_yv12_float_sse2             (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);
// typed wrappers around the bool template (lessthan16bits=true/false)
void conv_yv24_to_yv12_chroma_u16_lessthan16bit_sse2    (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);
void conv_yv24_to_yv12_chroma_u16_true16bit_sse2        (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);

// YV24 -> YV16 chroma downsamplers (typed wrappers around SSE2/SSE4.1 uint8_t/uint16_t templates)
void conv_yv24_to_yv16_chroma_u8_sse2  (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);
void conv_yv24_to_yv16_chroma_u16_sse2 (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);
void conv_yv24_to_yv16_chroma_u8_sse41 (BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);
void conv_yv24_to_yv16_chroma_u16_sse41(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);
void convert_yv24_chroma_to_yv16_float_sse2(BYTE *dstp, const BYTE *srcp, int dst_pitch, int src_pitch, int dst_width, int dst_height);

#endif // __444Convert_sse_h
