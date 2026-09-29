// AviSynth + .Copyright 2026 - AviSynth + Project
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

#pragma once
// GetAlphaRect scalar baseline template — included only by text-overlay.cpp.
// noaa and interlaced are baked in as template parameters so if constexpr
// eliminates the dead branches at compile time (~2% speedup on tight clips).

#include <avs/config.h>  // must precede AVS_WINDOWS guard

#if defined(AVS_WINDOWS) && !defined(NO_WIN_GDI)

#include "getalpharect_impl.h"

#include <avs/win.h>

template<bool noaa, bool interlaced>
static void GetAlphaRect_scalar(
    const void* lpAntialiasBits_,
    uint16_t*   soa_buf,
    int w, int h, int w_stride,
    int srcpitch,
    int textcolor, int halocolor,
    int& xl, int& yt, int& xr, int& yb)
{
  const GARTables& tbl = gar_tables();
  const uint8_t*  bitcnt = tbl.bitcnt;
  const uint8_t*  bitexl = tbl.bitexl;
  const uint8_t*  bitexr = tbl.bitexr;
  const uint16_t* gamma  = tbl.gamma;

  const int RYtext = ((textcolor>>16)&255), GUtext = ((textcolor>>8)&255), BVtext = (textcolor&255);
  const int RYhalo = ((halocolor>>16)&255), GUhalo = ((halocolor>>8)&255), BVhalo = (halocolor&255);
  const int Atext  = 255 - ((textcolor >> 24) & 0xFF);
  const int Ahalo  = 255 - ((halocolor >> 24) & 0xFF);

  xl = 0; xr = w + 1; yt = -1; yb = h;

  for (int y = 0; y < h; ++y) {
    uint16_t* row     = soa_buf + y * 4 * w_stride;
    uint16_t* dest_ba = row;
    uint16_t* dest_ry = row + w_stride;
    uint16_t* dest_gu = row + 2 * w_stride;
    uint16_t* dest_bv = row + 3 * w_stride;
    BYTE* src = (BYTE*)lpAntialiasBits_ + ((h-y-1)*8 + 20) * srcpitch + 2;
    int wt = w;
    do {
      int i;

#pragma warning(push)
#pragma warning(disable: 4068)
      DWORD tmp = 0;
      if constexpr (interlaced) {
#pragma unroll
        for (int ii = -8; ii < 16; ++ii)
          tmp |= *reinterpret_cast<const int*>(src + srcpitch*ii - 1);
      } else {
#pragma unroll
        for (int ii = -12; ii < 20; ++ii)
          tmp |= *reinterpret_cast<const int*>(src + srcpitch*ii - 1);
      }
      tmp &= 0x00FFFFFF;
#pragma warning(pop)

      if (tmp != 0) {
        if (wt >= xl) xl = wt;
        if (wt <= xr) xr = wt;
        if (y  >= yt) yt = y;
        if (y  <= yb) yb = y;

        int alpha1 = 0, alpha2 = 0;

        if constexpr (interlaced) {
          BYTE topmask = 0, cenmask = 0, botmask = 0;
          BYTE hmasks[16], mask;

          for (i = -4; i < 12; i++) {
            mask = src[srcpitch*i];
            alpha1 += bitcnt[mask];
            mask = -!!mask;
            mask |= bitexr[src[srcpitch*i-1]];
            mask |= bitexl[src[srcpitch*i+1]];
            hmasks[i+4] = mask;
          }

          for (i = -4; i < 4;  i++) topmask |= hmasks[i+4];
          for (i =  0; i < 8;  i++) cenmask  |= hmasks[i+4];
          for (i =  4; i < 12; i++) botmask  |= hmasks[i+4];

          for (mask = topmask, i = -4; i < 4; i++) {
            mask |= bitexr[src[srcpitch*(i+8)-1]];
            mask |=   -!!src[srcpitch*(i+8)  ];
            mask |= bitexl[src[srcpitch*(i+8)+1]];
            hmasks[i+4] |= mask;
          }
          for (mask = cenmask, i = 0; i < 8; i++) {
            mask |= bitexr[src[srcpitch*(i+8)-1]];
            mask |=   -!!src[srcpitch*(i+8)  ];
            mask |= bitexl[src[srcpitch*(i+8)+1]];
            hmasks[i+4] |= mask;
          }
          for (mask = botmask, i = 4; i < 12; i++) {
            mask |= bitexr[src[srcpitch*(i+8)-1]];
            mask |=   -!!src[srcpitch*(i+8)  ];
            mask |= bitexl[src[srcpitch*(i+8)+1]];
            hmasks[i+4] |= mask;
          }
          for (mask = botmask, i = 11; i >= 4; i--) {
            mask |= bitexr[src[srcpitch*(i-8)-1]];
            mask |=   -!!src[srcpitch*(i-8)  ];
            mask |= bitexl[src[srcpitch*(i-8)+1]];
            hmasks[i+4] |= mask;
          }
          for (mask = cenmask, i = 7; i >= 0; i--) {
            mask |= bitexr[src[srcpitch*(i-8)-1]];
            mask |=   -!!src[srcpitch*(i-8)  ];
            mask |= bitexl[src[srcpitch*(i-8)+1]];
            hmasks[i+4] |= mask;
          }
          for (mask = topmask, i = 3; i >= -4; i--) {
            mask |= bitexr[src[srcpitch*(i-8)-1]];
            mask |=   -!!src[srcpitch*(i-8)  ];
            mask |= bitexl[src[srcpitch*(i-8)+1]];
            hmasks[i+4] |= mask;
          }
          for (i = 0; i < 16; i++)
            alpha2 += bitcnt[hmasks[i]];

        } else {
          // Pack 8 stride-scattered bytes into a uint64_t for one SWAR popcount.
          uint64_t packed = 0;
          for (i = 0; i < 8; i++)
            packed |= (uint64_t)(uint8_t)src[srcpitch * i] << (i * 8);
          alpha1 = swar_popcount64(packed) * 2;

          if (alpha1) {
            alpha2 = 128;
          } else {
            BYTE cenmask = 0;
            for (i = 0; i < 8; i++) {
              cenmask |= bitexr[src[srcpitch*i-1]];
              cenmask |= bitexl[src[srcpitch*i+1]];
            }
            if (cenmask == 0xFF) {
              alpha2 = 128;
            } else {
              BYTE hmasks[8];
              BYTE mask = cenmask;
              for (i = 0; i < 8; i++) {
                mask |= bitexr[src[srcpitch*(i+8)-1]];
                mask |=   -!!src[srcpitch*(i+8)  ];
                mask |= bitexl[src[srcpitch*(i+8)+1]];
                hmasks[i] = mask;
              }
              mask = cenmask;
              for (i = 7; i >= 0; i--) {
                mask |= bitexr[src[srcpitch*(i-8)-1]];
                mask |=   -!!src[srcpitch*(i-8)  ];
                mask |= bitexl[src[srcpitch*(i-8)+1]];
                alpha2 += bitcnt[hmasks[i] | mask];
              }
              alpha2 *= 2;
            }
          }
        }

        if constexpr (noaa) {
          if (alpha1 >= 1) {
            alpha1 = 64 * 516 * Atext;
            alpha2 = 0;
          } else if (alpha2 > alpha1 && alpha2 >= 1) {
            alpha1 = 0;
            alpha2 = 64 * 516 * Ahalo;
          } else {
            alpha1 = 0;
            alpha2 = 0;
          }
        } else {
          alpha2 = gamma[alpha2];
          alpha1 = gamma[alpha1];
          alpha2 -= alpha1;
          alpha2 *= Ahalo;
          alpha1 *= Atext;
        }

        *dest_ba++ = (uint16_t)((64*516*255 - alpha1 -          alpha2) >> 15);
        *dest_bv++ = (uint16_t)((    BVtext * alpha1 + BVhalo * alpha2) >> 15);
        *dest_gu++ = (uint16_t)((    GUtext * alpha1 + GUhalo * alpha2) >> 15);
        *dest_ry++ = (uint16_t)((    RYtext * alpha1 + RYhalo * alpha2) >> 15);
      } else {
        *dest_ba++ = 256;
        *dest_bv++ = 0;
        *dest_gu++ = 0;
        *dest_ry++ = 0;
      }

      ++src;
    } while (--wt);
  }

  xl = w - xl;
  xr = w - xr;
}

// Dispatch table for the 4 noaa×interlaced combinations.
static getalpharect_fn_t GetAlphaRect_select_scalar(bool noaa, bool interlaced) {
  if (noaa) {
    return interlaced ? GetAlphaRect_scalar<true,true>
                      : GetAlphaRect_scalar<true,false>;
  } else {
    return interlaced ? GetAlphaRect_scalar<false,true>
                      : GetAlphaRect_scalar<false,false>;
  }
}

#endif // AVS_WINDOWS && !NO_WIN_GDI
