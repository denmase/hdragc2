#ifndef __FIXEDFONTS_H__
#define __FIXEDFONTS_H__

#include <cstdint>

typedef struct {
  const char* const fontname;
  const char* const fontname_internal;
  int charcount;
  uint8_t width; // like in global FONTBOUNDINGBOX
  uint8_t height;
  int8_t offset_x;
  int8_t offset_y;
  bool bold;
} FixedFont_info_t;

constexpr int PREDEFINED_FONT_COUNT = 18 + 2; // 2x9 Terminus + info_h

#endif  // __FIXEDFONTS_H__
