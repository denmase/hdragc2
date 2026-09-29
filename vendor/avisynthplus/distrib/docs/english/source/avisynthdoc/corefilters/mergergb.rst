
MergeARGB / MergeRGB
====================

::

    MergeARGB (clipA, clipR, clipG, clipB [, string "pixel_type"])
    MergeRGB (clipR, clipG, clipB [, string "pixel_type"])

Merge the alpha (transparency) and color channels from the source video clips 
into the output video clip. 

*ClipA* is the clip that provided the alpha data to merge into the output clip.
For a YUV format input clip the data is taken from the Luma channel. 
For a planar RGBA or RGB32/64 input clip the data is taken from the Alpha channel. 
It may not be RGB24/48 or alpha-less planar RGB format.

*ClipR*, *ClipG* and *ClipB* are the clips that provided the R, G and B data
respectively to merge into the output clip. For YUV format input clips the
data is taken from the Luma channel. For RGB format input clips the data is
taken from the respective source channel. i.e. R to R, G to G, B to B. The
unused chroma or color channels of the input clips are ignored.

All YUV luma pixel data is assumed to be pc-range, [0..255] (8 bit example, for
higher bit depths scaled from ``0`` to ``2^N-1``; or 0.0 to 1.0 when 32 bit float.
There is no tv-range, [16..235], scaling. Chroma data from YUV clips is ignored. 
Input clips may be a mixture of all formats, even single greyscale will do.

*pixel_type* default RGB32 or planar RGB (see below the rules), specifies the output format.
Accept any RGB, or planar RGB(A) pixel_type, plus a special one: "rgb".

The output format is planar rgb(a) (MergeRGB/MergeARGB) when

- pixel_type = "rgb" or
- pixel_type is empty and

  - either input is planar RGB
  - either input is different from 8 or 16 bits (no packed RGB formats there)
- pixel_type is explicitely set to a valid planar rgb constant e.g. "RGBP10"

Other notes:

- Alpha channel is filled with zero when MergeRGB output pixel_type format is specified to have an alpha plane.
- Frame property source is the R clip; ``_Matrix`` and ``_ChromaLocation`` are removed if R is not an RGB clip

The unused channels of the input clips are ignored.

Audio, FrameRate and FrameCount are taken from the first clip. 

Also see :ref:`here <multiclip>` for the resulting clip properties.

**Examples:**
::

    # This will only blur the Green channel.
    mpeg2source("c:\apps\avisynth\main.d2v")
    ConvertToRGB24()
    MergeRGB(Last, Blur(0.5), Last)


    # This will swap the red and blue channels and
    # load the alpha from a second video sources.
    vid1 = avisource("c:\apps\avisynth\main.avi")
    vid2 = avisource("c:\apps\avisynth\alpha.avi")
    MergeARGB(vid2, vid1.ShowBlue("YV12"), vid1, vid1.ShowRed("YV12"))
    AudioDub(vid1)


+-----------+-----------------------------------------------------------------------------------+
| Changelog |                                                                                   |
+===========+===================================================================================+
| 3.7.2     || add MergeARGB parameter "pixel_type", similar to MergeRGB                        |
|           || accept pixel_type other than 8-bit packed RGB formats, plus a special "rgb"      |
|           || output format can be planar rgb(a)                                               |
|           || Accept planar RGB clip in place of input clips and the appropriate color plane   |
|           |  is copied from them                                                              |
|           || Fill alpha channel with zero when MergeRGB output pixel_type format is specified |
|           |  to have an alpha plane                                                           |
|           || frame property source is the R clip; _Matrix and _ChromaLocation are removed if  |
|           |  R is not an RGB clip                                                             |
+-----------+-----------------------------------------------------------------------------------+
| v2.56     | added MergeARGB and MergeRGB                                                      |
+-----------+-----------------------------------------------------------------------------------+

$Date: 2025/03/07 14:15:00 $
