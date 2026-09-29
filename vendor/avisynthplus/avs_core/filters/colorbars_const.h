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


#ifndef __COLORBARS_CONST_H
#define __COLORBARS_CONST_H

// ===== -I AND +Q SIGNAL CONSTANTS =====
// Extracted for reuse in vectorscope graticule rendering
// RGB-native values (lifted to studio black, code 16)
constexpr double MINUS_I_R = 0.0;
constexpr double MINUS_I_G = 0.33856;
constexpr double MINUS_I_B = 0.51916;

constexpr double PLUS_Q_R = 0.34736;
constexpr double PLUS_Q_G = 0.0;
constexpr double PLUS_Q_B = 0.58156;

// YUV-targeted values (zero-luma, produces Y=16 after BT.601 conversion)
constexpr double MINUS_I_R_YUV = -0.20654;
constexpr double MINUS_I_G_YUV = 0.05911;
constexpr double MINUS_I_B_YUV = 0.23732;

constexpr double PLUS_Q_R_YUV = 0.13144;
constexpr double PLUS_Q_G_YUV = -0.13762;
constexpr double PLUS_Q_B_YUV = 0.36390;

// Legacy 8-bit YUV codes for direct comparison
constexpr int MINUS_I_Y8_SD601 = 16;
constexpr int MINUS_I_CB8_SD601 = 158;
constexpr int MINUS_I_CR8_SD601 = 95;

// Legacy 8-bit YUV codes (BT.601)
constexpr int PLUS_Q_Y8_SD601 = 16;
constexpr int PLUS_Q_CB8_SD601 = 174;
constexpr int PLUS_Q_CR8_SD601 = 149;

// Legacy 8-bit YUV codes (BT.709)
constexpr int PLUS_Q_Y8_HD709 = 53;
constexpr int PLUS_Q_CB8_HD709 = 178;
constexpr int PLUS_Q_CR8_HD709 = 154;

// ===== ColorBarsHD +I SIGNAL CONSTANT (BT.709) =====
// SMPTE RP 219 / EG 1: +I Signal Reference (Rec. 709 / HD)
// The +I (In-phase) signal is defined by its analog IRE levels:
//   R = 41.2545 IRE, G = 16.6946 IRE, B = 0 IRE
// Normalized Linear RGB (full range [0,1]): R: 0.412545, G: 0.166946, B: 0.000000
// After BT.709 conversion: Y=61, Cb=103, Cr=157 at 8-bit
constexpr double PLUS_I_R_YUV = 0.412545;
constexpr double PLUS_I_G_YUV = 0.166946;
constexpr double PLUS_I_B_YUV = 0.0;

// Legacy 8-bit YUV codes for +I (BT.709)
constexpr int PLUS_I_Y8_HD709 = 61;
constexpr int PLUS_I_CB8_HD709 = 103;
constexpr int PLUS_I_CR8_HD709 = 157;

#endif // __COLORBARS_CONST_H
