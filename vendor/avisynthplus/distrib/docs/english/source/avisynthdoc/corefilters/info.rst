
Info
====

Gives :doc:`clip property information <../syntax/syntax_clip_properties>` as a 
text overlay in the upper-left corner.

The displayed information consists of:

* current frame and total frame count,
* current time and total duration,
* colorspace and bit depth,
* width and height,
* frame rate (as floating-point and fraction),
* whether it is field or frame based,
* parity: whether AviSynth thinks it is bottom or top field first,
* video pitch (length of a video line in bytes),
* whether there is audio present,
* the number of audio channels,
* audio sample type,
* audio sample rate,
* total audio samples and total audio duration (hh:mm:ss:ddd),
* channel layout
* CPU capabilities


Syntax and Parameters
----------------------

::

    Info (clip clip, string "font", float "size", int "text_color", int "halo_color",
          bool "bold", bool "italic", bool "noaa",
          bool "cpu",
          float "x", float "y", int "align", bool "gdi")

.. describe:: clip

    Source clip. If only an audio clip is supplied, **Info** creates a blank 
    video clip to overlay the information on.

.. describe:: font

    | Font name; can be the name of any installed Windows font when ``gdi=true``.
    | When ``gdi=false``, only internal bitmap fonts are available (``"Terminus"``
      or ``"info_h"``).

    Default: "Courier New" when ``gdi=true``, "Terminus" when ``gdi=false``

.. describe:: size

    | Height of the text in pixels, and is rounded to the nearest 0.125 pixel. 
    | Default depends on the dimensions of the clip:
    
    * If either the width or height is less than 384x224, the font is auto-scaled.
    * If the both the width and height are greater than 384x224, ``size`` defaults
      to 15.
    * If ``size`` is set to < 0, the font is automatically enlarged for clips 
      over 640x480.

    Default: auto

    On non-Windows-GDI systems the available font sizes are limited.

.. describe:: text_color, halo_color

    | Colors for font fill and outline respectively.  See the
      :doc:`colors <../syntax/syntax_colors>` page for more information on 
      specifying colors.
    | Default text color is yellow and halo color is black.

    Default: $FFFF00, $000000

.. describe:: bold

    | Using bold letters or not.

    Default: true when ``gdi=true``, false when ``gdi=false``

.. describe:: italic

    | Using italic letters or not

    Default: false

.. describe:: noaa

    | Disables antialiasing when drawing the text

    Default: false

.. describe:: cpu

    | Enables or disables showing CPU capabilities flags

    Default: true

.. describe:: x, y

    | Reference point of the Info text box area, alignments are
      calculated relative to that.

    Default: 4, 0 (top left), screen centers or right/bottom when alignment is specified

.. describe:: align

    | an integer number describing at what screen area (or given x,y coordinates)
      will be the info text box aligned.
      Values 1-9 are allowed. See your numeric keypad layout, e.g. 7 is top-left,
      9 is top right, 3: bottom-right, etc..

    Default: 7 (top-left)

    Note: when ``gdi=true`` (Windows GDI), the whole text box is aligned and its
    content is left-aligned within the box. When ``gdi=false`` (e.g. Linux or
    ``gdi=false`` on Windows), each line is aligned individually.

.. describe:: gdi

    | When *true*, text is rendered using the Windows GDI Antialiaser, which
      produces antialiased output but is slower.
    | When *false*, the built-in bitmap font (Terminus) is used. This is faster
      and is the only option on non-Windows platforms.
    | Ignored (forced to *false*) on non-Windows builds.

    Default: true on Windows (with GDI support), false otherwise


Examples
--------

::

    Blankclip(pixel_type="YUV444P16")
    SetChannelMask("mono")
    Info()

Results in a video with the following information overlay:
    
::

     Frame: 0 of 240
     Time: 00:00:00:000 of 00:00:10:000
     ColorSpace: YUV444P16, BitsPerComponent: 16
     Width: 640 pixels, Height: 480 pixels.
     Frames per second: 24.0000 (24/1)
     FieldBased (Separated) Video: NO
     Parity: Bottom Field First
     Video Pitch: 1280 bytes.
     Has Audio: YES
     Audio Channels: 1
     Sample Type: Integer 16 bit
     Samples Per Second: 44100
     Audio length: 441000 samples. 00:00:10:000
     Channel mask: mono
     CPU: SSE2 SSE3 SSSE3 SSE4.1 SSE4.2 AVX F16C

The following script aligns Info box to the top right, it won't interfere with propShow
in top left.
    
::

    ColorBars()
    propShow()
    Info(cpu=false, align=9)


Changelog
---------

+-----------------+-----------------------------------------------------------------------+
| Version         | Changes                                                               |
+=================+=======================================================================+
| AviSynth+ 3.7.6 || Add ``gdi`` parameter to select between Windows GDI antialiased      |
|                 |  rendering (slow, high quality) and the built-in bitmap font (fast,   |
|                 |  cross-platform). Defaults to ``true`` on Windows with GDI.           |
|                 || ``font``, ``bold`` defaults are now ``gdi``-dependent.               |
+-----------------+-----------------------------------------------------------------------+
| AviSynth+ 3.7.4 || Add ``cpu`` parameter                                                |
|                 || Add ``x, y, align`` parameters                                       |
|                 || Draw partially visible lines as well                                 |
+-----------------+-----------------------------------------------------------------------+
| AviSynth+ 3.7.3 | Add ``bold``, ``italic`` and ``noaa``                                 |
+-----------------+-----------------------------------------------------------------------+
| AviSynth+ 3.5.1 | When parameter ``size`` < 0, font is automatically enlarged when the  |
|                 | dimensions of the clip are greater than 640x480.                      | 
+-----------------+-----------------------------------------------------------------------+
| AviSynth+ r2487 | Added parameters ``font, size, text_color, halo_color`` and fix       |
|                 | hardcoded dimensions.                                                 |
+-----------------+-----------------------------------------------------------------------+
| AviSynth 2.6.0  | Added support audio only clips.                                       |
+-----------------+-----------------------------------------------------------------------+
| AviSynth 2.5.7  | Added time of current frame, total time, numerator and denominator of |
|                 | the framerate and audio length.                                       |
+-----------------+-----------------------------------------------------------------------+
| AviSynth 2.5.5  | Added supported CPU optimizations                                     |
+-----------------+-----------------------------------------------------------------------+
| AviSynth 2.5.0  | Initial Release                                                       |
+-----------------+-----------------------------------------------------------------------+

$Date: 2023/11/03 10:42:00 $
