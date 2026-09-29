======
Invert
======

Inverts one or several color channels of a clip.


Syntax and Parameters
----------------------

::

    Invert (clip, string "channels")

.. describe:: clip

    Source clip; all color formats supported.

.. describe:: channels

    | Defines which channels should be inverted by their initial letters, e.g.
      "R" (=red).
    | Any letters that don't correspond to a channel in the current colorspace
      are ignored.
    | Valid channel letters are:

    * R, G, B, A for RGB(A) clips.
    * Y, U, V, A for YUV(A) clips.

    | Letters are not case sensitive and may be given in any order.
    | By default, all channels of the current colorspace are inverted.

    Default: "RGBA" if input clip is RGB, "YUVA" if input clip is YUV.


Notes
-----

**Integer formats (8–16 bit)**

*Luma and RGB channels* are inverted by ``max - value`` (equivalent to XOR with the maximum
sample value, e.g. ``255 - value`` for 8-bit).

*Chroma channels (U/V) in planar YUV* are inverted by reflecting around the neutral value:
``result = 2 × neutral - value``, clamped to the valid range.
For 8-bit this means ``256 - value``:

- sample value 1 → 255, value 255 → 1
- neutral value 128 → 128 (unchanged)
- value 0 has no proper inverse (it would require 256, which is out of range) and is mapped to 255

**32-bit float**

*Luma and RGB channels* are inverted as ``1.0 - value``, reflecting around the midpoint of
the ``[0.0, 1.0]`` luma range.  No clamping is applied.

*Chroma channels (U/V)* are negated: ``result = -value``.
In AviSynth+ 32-bit float YUV the chroma planes are signed, ranging from ``-0.5`` to ``+0.5``
with ``0.0`` as the neutral.  Negation is an exact mirror around that neutral, with no clamping
needed (``0.5 → -0.5``, ``-0.5 → 0.5``, ``0.0 → 0.0``).

**YUY2** still inverts chroma by XOR with 255 (``255 - value``), which does *not* preserve
the neutral value.  Convert to a planar format first if correct chroma inversion is needed.


Examples
---------

Invert the blue and green channels::

    AviSource("clip.avi")
    ConvertToRGB32()
    Invert(channels="BG") # can also be written as channels="g, b"

Examples were Invert has no effect::

    AviSource("clip.avi")
    ConvertToRGB24()
    Invert(channels="A")     # no effect (no current A channel)
    Invert(channels="VUY")   # no effect (no current Y, U or V channels)


Changelog
---------

+-----------------+--------------------------------------------------------+
| Version         | Changes                                                |
+=================+========================================================+
| AviSynth+ 3.7.6 | Planar YUV: U/V chroma inversion changed from XOR with |
|                 | maximum to pivot around neutral (``256 - value`` for   |
|                 | 8-bit). Value 0 maps to 255 (no valid inverse).        |
|                 | YUY2 retains the old XOR behaviour.                    |
|                 | Added proper AVX2 and SSE2 SIMD paths for planar       |
|                 | luma and chroma.                                       |
+-----------------+--------------------------------------------------------+
| AviSynth+ r2487 | Added support for YUV(A)/PlanarRGB(A) 8,10-16,32 bit,  |
|                 | RGB48/64 color formats, with SSE2.                     |
+-----------------+--------------------------------------------------------+
| AviSynth 2.6.0  | Added support for YV24, YV16, YV411, Y8 color formats. |
+-----------------+--------------------------------------------------------+
| Avisynth 2.5.5  | Added support for RGB24, YUY2 and YV12 color formats.  |
+-----------------+--------------------------------------------------------+
| AviSynth 2.5.3  | Initial Release.                                       |
+-----------------+--------------------------------------------------------+

$Date: 2026/05/08 11:03:00 $
