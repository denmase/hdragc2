CombinePlanes
=============

Merges planes of source clip(s) into a target clip.
It is similar to ShufflePlanes in Vapoursynth. Performs the functionality of :doc:`SwapUV <swap>`,
:doc:`YToUV <swap>`, :doc:`MergeChroma <merge>`, :doc:`MergeRGB <mergergb>` and more.

See also :doc:`Extract <extract>`, :doc:`AddAlphaPlane <mask>`, :doc:`RemoveAlphaPlane <mask>`,
and :doc:`ShowU/V <showalpha>` filters.

.. rubric:: Syntax and Parameters

::

    CombinePlanes(clip,
        [string planes, string source_planes, string pixel_type, clip sample_clip ] )

    CombinePlanes(clip, clip,
        [string planes, string source_planes, string pixel_type, clip sample_clip ] )

    CombinePlanes(clip, clip, clip,
        [string planes, string source_planes, string pixel_type, clip sample_clip ] )

    CombinePlanes(clip, clip, clip, clip,
        [string planes, string source_planes, string pixel_type, clip sample_clip ] ) 

.. describe:: clip

    | Source clip(s). At least one is required. Up to four clips are accepted.
    | Each clip defines a color plane in the output, as defined by the ``planes`` and 
      ``source_planes`` arguments. 
    | If the clip count is less than the given ``planes`` defined, then the last available 
      clip is used as a source for all later planes. 

.. describe:: planes = ""

    The target plane order (e.g. "YVU", "YYY", "RGB"); missing target planes will be undefined in the target. 

.. describe:: source_planes = "YUVA" or "RGBA"

    The source plane order, defaulting to "YUVA" or "RGBA" depending on the video format. 

    Source clips can even be mixed from greyscale, YUV, YUVA or planar RGB(A) — the only rule being that the relevant source plane character should match with the clip format, respectively. 

.. describe:: pixel_type

    Set color format of the returned clip. Supports all AVS+ color formats. 

.. describe:: sample_clip

        If supplied, output pixel_type will match that of sample_clip. 


Examples
--------

Combine greyscale clips into YUVA clip::

    U8 = source.UToY8()
    V8 = source.VToY8()
    Y8 = source.ConvertToY()
    A8 = source.AddAlphaPlane(128).AToY8()
    CombinePlanes(Y8, U8, V8, A8, planes="YUVA", source_planes="YYYY", 
    \               sample_clip=source) #pixel_type="YUV444P8"

Copy planes between planar RGB(A) and YUV(A) without any conversion
yuv 4:4:4 <-> planar rgb::

    source = last.ConvertBits(32) # 4:4:4
    cast_to_planarrgb = CombinePlanes(source, planes="RGB", source_planes="YUV", 
    \               pixel_type="RGBPS")
    # get back a clip identical with "source"
    cast_to_yuv = CombinePlanes(cast_to_planarrgb, planes="YUV", source_planes="RGB", 
    \               pixel_type="YUV444PS")

Create a black and white planar RGB clip using Y channel.
Source is a YUV clip.::

    grey = CombinePlanes(source, planes="RGB", source_planes="YYY", 
    \               pixel_type="RGBP8")

Copy luma from one clip, U and V from another::

    #Source is the template
    #SourceY is a Y or YUV clip
    #SourceUV is a YUV clip
    grey = CombinePlanes(sourceY, sourceUV, planes="YUV", 
    \               source_planes="YUV", sample_clip = source)

Notes
-----

One optimization in CombinePlanes is aimed to have one less memory (plane) copy.

Theory behind: when a frame has exactly one 'user' (no other frames are yet referencing it) then it can 
directly be grabbed and made writable without any frame plane content copying.
When this "I'm the only one" condition is fulfilled and the below-written conditions are set 
then it can be a bit quicker than using the ordinary "make a new frame and copy the referenced input
frames into that" logic.

* Source clip has the same format as the target, and the first plane ID is the same.
  Y comes from first clip (no Y plane copy, the input frame containing Y is reused), U and V are copied

* Second clip has the same format as the target, and the 2nd and 3rd plane ID is the same
  U and V comes from 2nd clip (no UV copy, frame containing U and V is reused), 
  while Y (or the given first plane ID) is copied from first clip. When there is a 

Example::

    Colorbars(pixel_type="YV12")
    ConvertBits(16)
    a=last # UV is kept
    Blur(1)
    #luma comes from LAST, a's UV is copied to last
    x=MergeLuma(a,last)
    y=CombinePlanes(last,a,planes="YUV",pixel_type="YUV420P16")
    y  # or x
    Prefetch(4)

Comparison: new CombinePlanes and the usual MergeLuma showed ~4600 fps while old CombinePlanes run at only 3540 fps

Note 2
------

Non-planar formats such as packed RGB or YUY2 inputs will automatically converted to planar RGB or YV16 before CombinePlanes.

Note 3
------

When there is only one input clip, a zero-cost (BitBlt-less, using "subframes") method is used, which is much faster.

Such cases are:

* casting YUV to RGB

* shuffle RGBA to ABGR

* U to Y

* etc..

Target planes that are not specified, preserve their content.

Examples::

    combineplanes(clipRGBP, planes="RGB",source_planes="BGR") # swap R and B
    combineplanes(clipYUV, planes="GBRA",source_planes="YUVA",pixel_type="RGBAP8") # cast YUVA to planar RGBA
    combineplanes(clipYUV, planes="Y",source_planes="U",pixel_type="Y8") # extract U

Changelog
---------

.. table::
    :widths: auto

    +-----------------+----------------------------------------------+
    | Version         | Changes                                      |
    +=================+==============================================+
    | AviSynth 3.7.1  | a bit optimized MergeLuma-like cases         |
    +-----------------+----------------------------------------------+
    | 20161110        | First added                                  |
    +-----------------+----------------------------------------------+

$Date: 2023/11/09 11:23:00 $

.. _chroma subsampling:
    https://en.wikipedia.org/wiki/Chroma_subsampling
