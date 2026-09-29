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

#include "overlayfunctions.h"

#include <stdint.h>
#include <type_traits>

void OL_AddImage::DoBlendImageMask(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  if (rgb) {
    if (of_mode == OF_Add) {
      if (bits_per_pixel == 8)
        BlendImageMask_RGB<uint8_t, true, true>(base, overlay, mask);
      else if (bits_per_pixel <= 16)
        BlendImageMask_RGB<uint16_t, true, true>(base, overlay, mask);
      else if (bits_per_pixel == 32)
        BlendImageMask_RGB_float<true, true>(base, overlay, mask);
    } else {
      // OF_Subtract
      if (bits_per_pixel == 8)
        BlendImageMask_RGB<uint8_t, true, false>(base, overlay, mask);
      else if (bits_per_pixel <= 16)
        BlendImageMask_RGB<uint16_t, true, false>(base, overlay, mask);
      else if (bits_per_pixel == 32)
        BlendImageMask_RGB_float<true, false>(base, overlay, mask);
    }
    return;
  }
  // existing YUV logic
  if(of_mode == OF_Add) {
    if (bits_per_pixel == 8)
      BlendImageMask<uint8_t, true, true>(base, overlay, mask);
    else if(bits_per_pixel <= 16)
      BlendImageMask<uint16_t, true, true>(base, overlay, mask);
    else if(bits_per_pixel == 32)
      BlendImageMask_float<true, true>(base, overlay, mask);
  }
  else {
    // OF_Subtract
    if (bits_per_pixel == 8)
      BlendImageMask<uint8_t, true, false>(base, overlay, mask);
    else if(bits_per_pixel <= 16)
      BlendImageMask<uint16_t, true, false>(base, overlay, mask);
    else if(bits_per_pixel == 32)
      BlendImageMask_float<true, false>(base, overlay, mask);
  }
}

void OL_AddImage::DoBlendImage(ImageOverlayInternal* base, ImageOverlayInternal* overlay) {
  if (rgb) {
    if (of_mode == OF_Add) {
      if (bits_per_pixel == 8)
        BlendImageMask_RGB<uint8_t, false, true>(base, overlay, nullptr);
      else if (bits_per_pixel <= 16)
        BlendImageMask_RGB<uint16_t, false, true>(base, overlay, nullptr);
      else if (bits_per_pixel == 32)
        BlendImageMask_RGB_float<false, true>(base, overlay, nullptr);
    }
    else {
      // OF_Subtract
      if (bits_per_pixel == 8)
        BlendImageMask_RGB<uint8_t, false, false>(base, overlay, nullptr);
      else if (bits_per_pixel <= 16)
        BlendImageMask_RGB<uint16_t, false, false>(base, overlay, nullptr);
      else if (bits_per_pixel == 32)
        BlendImageMask_RGB_float<false, false>(base, overlay, nullptr);
    }
    return;
  }
  // existing YUV logic
  if(of_mode == OF_Add) {
    if (bits_per_pixel == 8)
      BlendImageMask<uint8_t, false, true>(base, overlay, nullptr);
    else if(bits_per_pixel <= 16)
      BlendImageMask<uint16_t, false, true>(base, overlay, nullptr);
    else if(bits_per_pixel == 32)
      BlendImageMask_float<false, true>(base, overlay, nullptr);
  }
  else {
    // OF_Subtract
    if (bits_per_pixel == 8)
      BlendImageMask<uint8_t, false, false>(base, overlay, nullptr);
    else if(bits_per_pixel <= 16)
      BlendImageMask<uint16_t, false, false>(base, overlay, nullptr);
    else if(bits_per_pixel == 32)
      BlendImageMask_float<false, false>(base, overlay, nullptr);
  }
}

// integer 8-16 bit add/subtract with YUV overshoot handling
template<typename pixel_t, bool maskMode, bool of_add>
void OL_AddImage::BlendImageMask(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {

  pixel_t* baseY = reinterpret_cast<pixel_t *>(base->GetPtr(PLANAR_Y));
  pixel_t* baseU = reinterpret_cast<pixel_t *>(base->GetPtr(PLANAR_U));
  pixel_t* baseV = reinterpret_cast<pixel_t *>(base->GetPtr(PLANAR_V));

  pixel_t* ovY = reinterpret_cast<pixel_t *>(overlay->GetPtr(PLANAR_Y));
  pixel_t* ovU = reinterpret_cast<pixel_t *>(overlay->GetPtr(PLANAR_U));
  pixel_t* ovV = reinterpret_cast<pixel_t *>(overlay->GetPtr(PLANAR_V));

  pixel_t* maskY = maskMode ? reinterpret_cast<pixel_t *>(mask->GetPtr(PLANAR_Y)) : nullptr;
  pixel_t* maskU = maskMode ? reinterpret_cast<pixel_t *>(mask->GetPtr(PLANAR_U)) : nullptr;
  pixel_t* maskV = maskMode ? reinterpret_cast<pixel_t *>(mask->GetPtr(PLANAR_V)) : nullptr;

  const int half_pixel_value = (sizeof(pixel_t) == 1) ? 128 : (1 << (bits_per_pixel - 1));
  const int max_pixel_value = (sizeof(pixel_t) == 1) ? 255 : (1 << bits_per_pixel) - 1;
  const int pixel_range = max_pixel_value + 1;
  const int SHIFT  = (sizeof(pixel_t) == 1) ? 5 : 5 + (bits_per_pixel - 8);
  const int MASK_CORR_SHIFT = (sizeof(pixel_t) == 1) ? 8 : bits_per_pixel;
  const int OPACITY_SHIFT  = 8; // opacity always max 0..256
  const int over32 = (1 << SHIFT); // 32
  const int basepitch = (base->pitch) / sizeof(pixel_t);
  const int overlaypitch = (overlay->pitch) / sizeof(pixel_t);
  const int maskpitch = maskMode ? (mask->pitch) / sizeof(pixel_t) : 0;

  // avoid "uint16*uint16 can't get into int32" overflows
  typedef typename std::conditional < sizeof(pixel_t) == 1, int, typename std::conditional < sizeof(pixel_t) == 2, int64_t, float>::type >::type result_t;

/*
  In YUV, "add" and "subtract" are not just per-channel math. The luma (Y) is added/subtracted, but if the result
  overflows (Y > max) or underflows (Y < 0), the chroma (U/V) is "pulled" toward neutral (gray/white) to mimic
  how RGB overbright/underbright behaves visually.

  In RGB, adding two bright colors can result in "white" (all channels maxed). In YUV, if you just add Y, U, and V,
  we can get weird color shifts. The code compensates by blending U/V toward neutral when Y is out of range,
  making the result look more like RGB addition.

  For RGB, a simple per-channel add/subtract (with clamping for 8/16-bit, or no clamping for float) is done.
  The "magic" is only needed for YUV to avoid odd color artifacts. In RGB, overbright naturally becomes white,
  so no special handling is needed.
*/

  int w = base->w();
  int h = base->h();
  if (opacity == 256) {
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        int Y, U, V;
        if (of_add) {
          Y = baseY[x] + (maskMode ? (((result_t)ovY[x] * maskY[x]) >> MASK_CORR_SHIFT) : ovY[x]);
          U = baseU[x] + (int)(maskMode ? ((((result_t)half_pixel_value*(pixel_range - maskU[x])) + ((result_t)maskU[x] * ovU[x])) >> MASK_CORR_SHIFT) : ovU[x]) - half_pixel_value;
          V = baseV[x] + (int)(maskMode ? ((((result_t)half_pixel_value*(pixel_range - maskV[x])) + ((result_t)maskV[x] * ovV[x])) >> MASK_CORR_SHIFT) : ovV[x]) - half_pixel_value;
          // When Y is too high, U and V are blended toward half_pixel_value (neutral chroma), making the color "whiter".
          if (Y>max_pixel_value) {  // Apply overbrightness to UV
            int multiplier = max(0,pixel_range + over32 -Y);  // 0 to 32
            U = ((U*(         multiplier)) + (half_pixel_value*(over32-multiplier)))>>SHIFT;
            V = ((V*(         multiplier)) + (half_pixel_value*(over32-multiplier)))>>SHIFT;
            Y = max_pixel_value;
          }
        }
        else {
          // of_subtract
          Y = baseY[x] - (maskMode ? (((result_t)ovY[x] * maskY[x]) >> MASK_CORR_SHIFT) : ovY[x]);
          U = baseU[x] - (int)(maskMode ? ((((result_t)half_pixel_value*(pixel_range - maskU[x])) + ((result_t)maskU[x] * ovU[x])) >> MASK_CORR_SHIFT) : ovU[x]) + half_pixel_value;
          V = baseV[x] - (int)(maskMode ? ((((result_t)half_pixel_value*(pixel_range - maskV[x])) + ((result_t)maskV[x] * ovV[x])) >> MASK_CORR_SHIFT) : ovV[x]) + half_pixel_value;
          if (Y<0) {  // Apply superdark to UV
            int multiplier = min(-Y,over32);  // 0 to 32
            U = ((U*(over32 - multiplier)) + (half_pixel_value*(       multiplier)))>>SHIFT;
            V = ((V*(over32 - multiplier)) + (half_pixel_value*(       multiplier)))>>SHIFT;
            Y = 0;
          }
        }
        baseU[x] = (pixel_t)clamp(U, 0, max_pixel_value);
        baseV[x] = (pixel_t)clamp(V, 0, max_pixel_value);
        baseY[x] = (pixel_t)Y;
      }
      baseY += basepitch;
      baseU += basepitch;
      baseV += basepitch;

      ovY += overlaypitch;
      ovU += overlaypitch;
      ovV += overlaypitch;

      if(maskMode) {
        maskY += maskpitch;
        maskU += maskpitch;
        maskV += maskpitch;
      }
    }
  } else {
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; x++) {
        int Y, U, V;
        if(of_add)
          Y = baseY[x] + (maskMode ? (((result_t)maskY[x] * opacity*ovY[x]) >> (OPACITY_SHIFT + MASK_CORR_SHIFT)) : ((opacity*ovY[x]) >> OPACITY_SHIFT));
        else
          Y = baseY[x] - (maskMode ? (((result_t)maskY[x] * opacity*ovY[x]) >> (OPACITY_SHIFT + MASK_CORR_SHIFT)) : ((opacity*ovY[x]) >> OPACITY_SHIFT));
        if (maskMode) {
          result_t mU = (maskU[x] * opacity) >> OPACITY_SHIFT;
          result_t mV = (maskV[x] * opacity) >> OPACITY_SHIFT;
          if(of_add) {
            U = baseU[x] + (int)(((half_pixel_value*(pixel_range - mU)) + (mU*ovU[x])) >> MASK_CORR_SHIFT) - half_pixel_value;
            V = baseV[x] + (int)(((half_pixel_value*(pixel_range - mV)) + (mV*ovV[x])) >> MASK_CORR_SHIFT) - half_pixel_value;
          }
          else {
            U = baseU[x] - (int)(((half_pixel_value*(pixel_range - mU)) + (mU*ovU[x])) >> MASK_CORR_SHIFT) + half_pixel_value;
            V = baseV[x] - (int)(((half_pixel_value*(pixel_range - mV)) + (mV*ovV[x])) >> MASK_CORR_SHIFT) + half_pixel_value;
          }
        }
        else {
          if(of_add) {
            U = baseU[x] + (((half_pixel_value*inv_opacity)+(opacity*(ovU[x])))>>OPACITY_SHIFT) - half_pixel_value;
            V = baseV[x] + (((half_pixel_value*inv_opacity)+(opacity*(ovV[x])))>>OPACITY_SHIFT) - half_pixel_value;
          }
          else {
            U = baseU[x] - (((half_pixel_value*inv_opacity)+(opacity*(ovU[x])))>>OPACITY_SHIFT) + half_pixel_value;
            V = baseV[x] - (((half_pixel_value*inv_opacity)+(opacity*(ovV[x])))>>OPACITY_SHIFT) + half_pixel_value;
          }
        }
        if(of_add) {
          if (Y>max_pixel_value) {  // Apply overbrightness to UV
            int multiplier = max(0,(max_pixel_value + 1) + over32 - Y);  // 288-Y : 0 to 32
            U = ((U*multiplier) + (half_pixel_value*(over32 - multiplier))) >> SHIFT;
            V = ((V*multiplier) + (half_pixel_value*(over32 - multiplier))) >> SHIFT;
            Y = max_pixel_value;
          }
        }
        else {
          // of_subtract
          if (Y<0) {  // Apply overbrightness to UV
            int multiplier = min(-Y,over32);  // 0 to 32
            U = ((U*(over32 - multiplier)) + (half_pixel_value*(       multiplier)))>>SHIFT;
            V = ((V*(over32 - multiplier)) + (half_pixel_value*(       multiplier)))>>SHIFT;
            Y = 0;
          }
        }
        baseU[x] = (pixel_t)clamp(U, 0, max_pixel_value);
        baseV[x] = (pixel_t)clamp(V, 0, max_pixel_value);
        baseY[x] = (pixel_t)Y;
      }
      baseY += basepitch;
      baseU += basepitch;
      baseV += basepitch;

      ovY += overlaypitch;
      ovU += overlaypitch;
      ovV += overlaypitch;

      if(maskMode) {
        maskY += maskpitch;
        maskU += maskpitch;
        maskV += maskpitch;
      }
    }
  }
}

// float add/subtract with YUV overshoot handling
template<bool maskMode, bool of_add>
void OL_AddImage::BlendImageMask_float(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  // specialized pixel_t float images
  // No clamping needed.
  // float range here is supposed to be [0.0f .. 1.0f] for Y, [-0.5f .. 0.5f] for U/V
  // mask is [0.0f .. 1.0f]
  float* baseY = reinterpret_cast<float*>(base->GetPtr(PLANAR_Y));
  float* baseU = reinterpret_cast<float*>(base->GetPtr(PLANAR_U));
  float* baseV = reinterpret_cast<float*>(base->GetPtr(PLANAR_V));

  float* ovY = reinterpret_cast<float*>(overlay->GetPtr(PLANAR_Y));
  float* ovU = reinterpret_cast<float*>(overlay->GetPtr(PLANAR_U));
  float* ovV = reinterpret_cast<float*>(overlay->GetPtr(PLANAR_V));

  float* maskY = maskMode ? reinterpret_cast<float*>(mask->GetPtr(PLANAR_Y)) : nullptr;
  float* maskU = maskMode ? reinterpret_cast<float*>(mask->GetPtr(PLANAR_U)) : nullptr;
  float* maskV = maskMode ? reinterpret_cast<float*>(mask->GetPtr(PLANAR_V)) : nullptr;

  // For float, half_pixel_value is 0.0f, max_pixel_value is 1.0f for Y
  constexpr float half_pixel_value = 0.0f; // intentionally keep it 0.0f for U/V calculation, compiler will optimize away
  constexpr float max_pixel_value = 1.0f; // for Y overshoot check
  constexpr float pixel_range = 1.0f; // mask must be in [0.0f, 1.0f]

  // have no opacity (0..256), but special opacity_f (0..1.0) for float
  // Unlike integer case which has OPACITY_SHIFT of 8 bit for integer arithmetic 
  const float inv_opacity_f = 1.0f - opacity_f; 

  const int basepitch = (base->pitch) / sizeof(float);
  const int overlaypitch = (overlay->pitch) / sizeof(float);
  const int maskpitch = maskMode ? (mask->pitch) / sizeof(float) : 0;

  int w = base->w();
  int h = base->h();

  if (opacity_f == 1.0f) {
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; ++x) {
        float Y, U, V;
        if (of_add) {
          Y = baseY[x] + (maskMode ? ovY[x] * maskY[x] : ovY[x]);
          U = baseU[x] + (maskMode ? (half_pixel_value * (pixel_range - maskU[x]) + maskU[x] * ovU[x]) : ovU[x]) - half_pixel_value;
          V = baseV[x] + (maskMode ? (half_pixel_value * (pixel_range - maskV[x]) + maskV[x] * ovV[x]) : ovV[x]) - half_pixel_value;
        } else {
          Y = baseY[x] - (maskMode ? ovY[x] * maskY[x] : ovY[x]);
          U = baseU[x] - (maskMode ? (half_pixel_value * (pixel_range - maskU[x]) + maskU[x] * ovU[x]) : ovU[x]) + half_pixel_value;
          V = baseV[x] - (maskMode ? (half_pixel_value * (pixel_range - maskV[x]) + maskV[x] * ovV[x]) : ovV[x]) + half_pixel_value;
        }

        constexpr float over32 = 32.0f / 255.0f; // ~0.12549f

        if (of_add) {
          if (Y > max_pixel_value) { // Y > 1.0f
            float multiplier = max(0.0f, 1.0f + over32 - Y); // 1.12549 - Y, clamp to >=0
            // Blend U/V toward neutral (0.0f)
            U = U * multiplier / over32;
            V = V * multiplier / over32;
            Y = max_pixel_value; // 1.0f
          }
        }
        else {
          if (Y < 0.0f) {
            float multiplier = min(-Y, over32); // 0 to over32
            U = U * (over32 - multiplier) / over32;
            V = V * (over32 - multiplier) / over32;
            Y = 0.0f;
          }
        }

        // No other clamping for float
        baseU[x] = U;
        baseV[x] = V;
        baseY[x] = Y;
      }
      baseY += basepitch;
      baseU += basepitch;
      baseV += basepitch;

      ovY += overlaypitch;
      ovU += overlaypitch;
      ovV += overlaypitch;

      if (maskMode) {
        maskY += maskpitch;
        maskU += maskpitch;
        maskV += maskpitch;
      }
    }
  } else {
    for (int y = 0; y < h; y++) {
      for (int x = 0; x < w; ++x) {
        float Y, U, V;
        if (of_add)
          Y = baseY[x] + (maskMode ? maskY[x] * opacity_f * ovY[x] : opacity_f * ovY[x]);
        else
          Y = baseY[x] - (maskMode ? maskY[x] * opacity_f * ovY[x] : opacity_f * ovY[x]);
        if (maskMode) {
          float mU = maskU[x] * opacity_f;
          float mV = maskV[x] * opacity_f;
          if (of_add) {
            U = baseU[x] + (half_pixel_value * (pixel_range - mU) + mU * ovU[x]) - half_pixel_value;
            V = baseV[x] + (half_pixel_value * (pixel_range - mV) + mV * ovV[x]) - half_pixel_value;
          } else {
            U = baseU[x] - (half_pixel_value * (pixel_range - mU) + mU * ovU[x]) + half_pixel_value;
            V = baseV[x] - (half_pixel_value * (pixel_range - mV) + mV * ovV[x]) + half_pixel_value;
          }
        } else {
          if (of_add) {
            U = baseU[x] + (half_pixel_value * inv_opacity_f + opacity_f * ovU[x]) - half_pixel_value;
            V = baseV[x] + (half_pixel_value * inv_opacity_f + opacity_f * ovV[x]) - half_pixel_value;
          } else {
            U = baseU[x] - (half_pixel_value * inv_opacity_f + opacity_f * ovU[x]) + half_pixel_value;
            V = baseV[x] - (half_pixel_value * inv_opacity_f + opacity_f * ovV[x]) + half_pixel_value;
          }
        }

        constexpr float over32 = 32.0f / 255.0f; // ~0.12549f

        if (of_add) {
          if (Y > max_pixel_value) { // Y > 1.0f
            float multiplier = max(0.0f, 1.0f + over32 - Y); // 1.12549 - Y, clamp to >=0
            // Blend U/V toward neutral (0.0f)
            U = U * multiplier / over32;
            V = V * multiplier / over32;
            Y = max_pixel_value; // 1.0f
          }
        }
        else {
          if (Y < 0.0f) {
            float multiplier = min(-Y, over32); // 0 to over32
            U = U * (over32 - multiplier) / over32;
            V = V * (over32 - multiplier) / over32;
            Y = 0.0f;
          }
        }

        // No other clamping for float
        baseU[x] = U;
        baseV[x] = V;
        baseY[x] = Y;
      }
      baseY += basepitch;
      baseU += basepitch;
      baseV += basepitch;

      ovY += overlaypitch;
      ovU += overlaypitch;
      ovV += overlaypitch;

      if (maskMode) {
        maskY += maskpitch;
        maskU += maskpitch;
        maskV += maskpitch;
      }
    }
  }
}


// integer 8-16-bit RGB add/subtract
template<typename pixel_t, bool maskMode, bool of_add>
void OL_AddImage::BlendImageMask_RGB(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  int w = base->w();
  int h = base->h();
  const int pixelsize = sizeof(pixel_t);
  const int max_pixel_value = (sizeof(pixel_t) == 1) ? 255 : (1 << bits_per_pixel) - 1;
  auto factor = maskMode ? opacity_f / max_pixel_value : opacity_f;

  for (int p = 0; p < 3; ++p) {
    pixel_t* baseP = reinterpret_cast<pixel_t*>(base->GetPtrByIndex(p));
    pixel_t* ovP = reinterpret_cast<pixel_t*>(overlay->GetPtrByIndex(p));
    pixel_t* maskP = maskMode ? reinterpret_cast<pixel_t*>(mask->GetPtrByIndex(p)) : nullptr;
    int basePitch = base->GetPitchByIndex(p) / pixelsize;
    int overlayPitch = overlay->GetPitchByIndex(p) / pixelsize;
    int maskPitch = maskMode ? (mask->GetPitchByIndex(p) / pixelsize) : 0;

    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        int baseVal = baseP[x];
        int ovVal = ovP[x];

        const float new_mask = maskMode ? (float)reinterpret_cast<const pixel_t*>(maskP)[x] * factor : factor;
        float result;

        if constexpr (of_add)
          result = baseVal + ovVal * new_mask;
        else
          result = baseVal - ovVal * new_mask;

        baseP[x] = (pixel_t)(min(max((int)(result + 0.5f), 0), max_pixel_value));
      }
      baseP += basePitch;
      ovP += overlayPitch;
      if constexpr (maskMode) maskP += maskPitch;
    }
  }
}


// 32-bit float RGB add/subtract
template<bool maskMode, bool of_add>
void OL_AddImage::BlendImageMask_RGB_float(ImageOverlayInternal* base, ImageOverlayInternal* overlay, ImageOverlayInternal* mask) {
  int w = base->w();
  int h = base->h();

  auto factor = maskMode ? opacity_f / 1.0f : opacity_f; // for float, max_pixel_value is 1.0f for masks

  for (int p = 0; p < 3; ++p) {
    float* baseP = reinterpret_cast<float*>(base->GetPtrByIndex(p));
    float* ovP = reinterpret_cast<float*>(overlay->GetPtrByIndex(p));
    float* maskP = maskMode ? reinterpret_cast<float*>(mask->GetPtrByIndex(p)) : nullptr;
    int basePitch = base->GetPitchByIndex(p) / sizeof(float);
    int overlayPitch = overlay->GetPitchByIndex(p) / sizeof(float);
    int maskPitch = maskMode ? (mask->GetPitchByIndex(p) / sizeof(float)) : 0;

    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        float baseVal = baseP[x];
        float ovVal = ovP[x];

        const float new_mask = maskMode ? (float)reinterpret_cast<const float*>(maskP)[x] * factor : factor;
        float result;

        if constexpr (of_add)
          result = baseVal + ovVal * new_mask;
        else
          result = baseVal - ovVal * new_mask;
        baseP[x] = result; // no clamping for float
      }
      baseP += basePitch;
      ovP += overlayPitch;
      if constexpr (maskMode) maskP += maskPitch;
    }
  }
}

