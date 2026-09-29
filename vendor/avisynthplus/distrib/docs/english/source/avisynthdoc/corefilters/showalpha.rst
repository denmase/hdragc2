
ShowAlpha, ShowRed, ShowGreen, ShowBlue, ShowY, ShowU, ShowV
============================================================

Returns the selected channel as a greyscale clip.

::

    ShowAlpha (clip, string pixel_type)
    ShowBlue (clip, string pixel_type)
    ShowGreen (clip, string pixel_type)
    ShowRed (clip, string pixel_type)
    ShowY (clip, string pixel_type)
    ShowU (clip, string pixel_type)
    ShowV (clip, string pixel_type)

``ShowAlpha`` Returns the alpha channel of a RGB32/RGB64/RGBAP/YUVA clip in greyscale.

``ShowBlue``, ``ShowGreen``, ``ShowRed`` returns the selected channel of a RGB clip

``ShowY``, ``ShowU``, ``ShowV`` returns the selected channel of a YUV clip

``pixel_type`` Sets the color format of the output.

Default pixel_type is adaptive.

If pixel_type is empty and source is RGB, or pixel_type="rgb", then output type is

- RGB32 or RGB64 when source if packed RGB(A) (match the bit depth)
- RGBP with the matching bit depth if source is planar RGB(A)

If pixel_type is empty and source is YUV, or pixel_type="yuv", then output type is

- YUV444 (match the bit depth)

If pixel_type is "y" or "rgbp" or "rgbap" then output type is

- Y, RGBP or RGBAP respectively with the matching bit depth.

At all other cases, the pixel_type should be explicitely given.

Conversion rules:

For RGB output the selected channel is copied to all R, G and B channels, 
but not the Alpha channel which is left untouched (if target format has alpha).

For Y or YUV output the selected channel is copied to the Luma channel.
For YUV output the chroma (U and Y) channels are set to grey (0x80 when 8 bits).

If output is set to a Y/YUV format, they are are full range (8 bits: 0-255), and so 
can be used as the mask argument to Overlay. 

**Examples:**
::

    # shows alpha channels of clip
    AviSource("clip.avi")
    ShowAlpha()

    # swaps red and blue channels:
    AviSource("clip.avi")
    MergeRGB(ShowBlue("YV12"), Last, ShowRed("YV12"))

See also :doc:`plane Extract functions <extract>` (AviSynth+)

+-----------+---------------------------------------+
| Changelog |                                       |
+===========+=======================================+
| v3.7.4    | Fix swapped ShowGreen/ShowBlue for    |
|           | planar RGB input                      |
+-----------+---------------------------------------+
| AviSynth+ | | added ShowY, ShowU, ShowV           |
|           | | allow any planar/packed RGB(A)      |
|           | | allow YUV input                     |
|           | | allow any valid pixel_type          |
+-----------+---------------------------------------+
| v2.6      | pixel_type "Y8"                       |
+-----------+---------------------------------------+
| v2.56     | added ShowBlue, ShowGreen and ShowRed |
+-----------+---------------------------------------+
| v2.53     | added ShowAlpha                       |
+-----------+---------------------------------------+

$Date: 2005/07/08 22:53:16 $
