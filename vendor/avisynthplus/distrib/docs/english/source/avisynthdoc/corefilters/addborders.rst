==========
AddBorders
==========

Add black or colored borders, increasing frame size. This has several common uses:

* Adjust the `aspect ratio`_ (make a 4:3 clip into 16:9 without stretching)
* :doc:`Splice <splice>` a smaller resolution clip to a larger one without resizing
* Together with :doc:`Crop <crop>`, shift a clip horizontally or vertically – see below.
* Optionally filters the transient areas. (3.7.4-)

See also: :doc:`letterbox`, which adds borders without changing frame size.


Syntax and Parameters
---------------------

::

    AddBorders (clip clip, int left, int top, int right, int bottom, int "color", int "color_yuv",
                string "resample", float "param1", float "param2", float "param3", int "r" )

.. describe:: clip

    Source clip; all color formats supported.

.. describe:: left, top, right, bottom

    Border width in pixels.

    * For YUV411 sources, left and right must be `mod4`_ (divisible by 4).
    * For YUV420 sources, all four border widths must be `mod2`_ (divisible by 2).
    * For YUV422 sources, left and right must be mod2.

.. describe:: color

    | Specifies the border color; black by default.
    | Color is specified as an RGB value in either hexadecimal or decimal notation.
    | Hex numbers must be preceded with a $. See the
      :doc:`colors <../syntax/syntax_colors>` page for more information on
      specifying colors.

    * For YUV clips, colors are converted from full range to limited range
      `Rec.601`_.

    * Use ``color_yuv`` to specify full range YUV values or a color with a
      different matrix.

    Default: $000000

.. describe:: color_yuv

    | Specifies the border color using YUV values. Input clip must be YUV;
      otherwise an error is raised.
    | Similar to ``color_yuv`` in :doc:`BlankClip <blankclip>` 

.. describe:: resample

    string  resample = "gauss"

    When `r` radius is not zero, then determines which resampler is used in the transient filtering. 
    All AviSynth :doc:`resizers <resize>` are allowed:
    ("point", "bilinear", "bicubic", "lanczos", "lanczos4", "blackman", "spline16", "spline36", "spline64", 
    "gauss" and "sinc", "sinpow", "sinclin2" and "userdefined2"). 
    
    Default is "gauss". 

.. describe:: param1, param2, param3

    These 'float' type parameters can be the additional parameters for the
    resampler. Some resizer algorithms would need and can be fine tuned with up to 3 parameters.
    Their default values depend on the selected chromaresample resizer kernel.
    
    default (when "gauss"): 
    
    * param1 (p), param2(b), param3(s): p=10, b=2.71828182, s=0
    
    for other resizer algoritms see  in :doc:`resizers <resize>`. 
    
.. describe:: r
    
    int  r = 0

    The radius of the transient treatment in pixels. The value is meant as +/- around the border line.

    When `r` radius is not zero, transient filtering occurs.
    Even ``r=1`` is giving sufficient protection for some next processing stages.
    
    Why: by filtering the transient areas (boundary of the new borders), we can prevent 
    artifacts e.g. ringing after a subsequent upscale.
    
    This is how it works:
    
    Eight crops are taken from around the transient areas: 
    
    - left and right rectangles, which cover +/- 10 (but at least ``r`` + ceil(filter_support)) pixels 
      horizontally around the new border line.
    - top and bottom rectangles, which cover +/- 10 (but at least ``r`` + ceil(filter_support)) pixels 
      vertically around the new border line.
    - four corners, top left, top right, bottom left, bottom right, dimension rules
      as seen above.
    
    These eight rectangles are "resized", using the given resizer in convolution mode.
    No dimension is changed, just we'll get a blurred area.
    
    Since 3.7.4 resizers can accept a ``force`` parameter, so we use it internally. 
    
    - left and right parts need only horizontal treatment.
    - top and bottom parts need only vertical treatment.
    - corners get both H and V processing.
    
    Then, from this larger area only a central ``r`` (radius)-wide rectangle is copied over the transient area.
    
    The left and right side will be overwritten by a ``r * original_height`` sized part.
    At the top and bottom an ``original_width * r`` sized rectangle will be copied back.
    And a ``r * r`` rectangle is copied over the corners.
    
    The exact dimensions can be a bit different, e.g. if ``r`` is larger than the actually added left border.
    Then the radius is reduced accordingly.
    
    The ``r`` radius is automatically adjusted with the video format subsampling requirements.
    AddBorders won't give error if e.g. for a YV12 ``r=1`` is given: due to the chroma subampling it will be 
    automatically promoted to 2.
    
    Other notes:
    
    - This copy-paste of the up-to eight area is using a new :doc:`MultiOverlay <multioverlay>` filter.
    - The convolution filters (resizers) now take the chroma placement into account (``_ChromaLocation`` 
      frame property)
    
Examples
--------

* Add letterbox (top and bottom) borders:

  .. code-block:: c++

    # add dark blue borders, using hex color notation
    AddBorders(0, 86, 0, 86, color=$00008B)

    # same as above, using named preset color
    AddBorders(0, 86, 0, 86, color=color_darkblue)

    # full scale black border using color_yuv hex color notation
    AddBorders(0, 86, 0, 86, color_yuv=$008080)

* Be aware that many older lossy compression algorithms don't deal well with
  solid-color borders, unless the border happens to fall on a `macroblock`_
  boundary (16 pixels for MPEG).

* Use **AddBorders** in combination with **Crop** to *shift* an image without
  changing the frame size:

  .. code-block:: c++

    # Shift an image 2 pixels to the right
    Crop(0, 0, Width-2, Height)
    AddBorders(2, 0, 0, 0)

  * Note, shifting this way must be done in 1- or 2-pixel increments, depending
    on color format.
  * You can shift in sub-pixel increments with :doc:`Resize <resize>`.

* AddBorders with filtering

  ::

    # Add 20 black pixels around the clip, filters (blurs) 1 pixel with the default "gauss" method
    AddBorders(20, 20, 20, 20, r=1)

* AddBorders filtering test script
  ::
  
    Function AddBordersHF(clip c, int left, int right, int flt_rad)
    {
      unflt=AddBorders(c, left, 0, right, 0)
      flt=GaussResize(unflt, unflt.width, unflt.height, p=10, b=2.71828, s=0, force=1)
      uf_internal=Crop(c, flt_rad, 0, c.width-flt_rad*2, c.height)
      return Overlay(flt, uf_internal, x=left+flt_rad, y=0)
    }

    Function AddBordersVF(clip c, int top, int bottom, int flt_rad)
    {
      unflt=AddBorders(c, 0, top, 0, bottom)
      flt=GaussResize(unflt, unflt.width, unflt.height, p=10, b=2.71828, s=0, force=2)
      uf_internal=Crop(c, 0, flt_rad, 0, c.height - flt_rad*2)
      return Overlay(flt, uf_internal, x=0, y=top+flt_rad)
    }

    Function Diff(clip src1, clip src2)
    {
      return Subtract(src1.ConvertBits(8),src2.ConvertBits(8)).Levels(120, 1, 255-120, 0, 255, coring=false)
    }

    ColorBarsHD(2000,2000)
    UserDefined2Resize(width/10, height/10)

    # filtering area, means +/- around the border boundaries
    r1=2
    r2=2

    left=20
    top=20
    right=20
    bottom=20

    std=AddBorders(left, top, right, bottom)
    a=last
    a=AddBordersHF(a, left, right, r1)
    a=AddBordersVF(a, top, bottom, r1).SubTitle("Scriptbased", align=5)

    b=last
    b=AddBorders(b, left, top, right, bottom, param1=10, param2=2.71828, param3=0, r=r2).SubTitle("AVS 3.7.4", align=5)

    d1 = Diff(a,b)
    d2 = Diff(std,a)
    d3 = Diff(std,b)

    StackHorizontal(StackVertical(std, a, b), Stackvertical(d1, d2, d3))

    LanczosResize(width*4, height*2, taps=16)



Changelog
----------

+-----------------+------------------------------------------------------------------+
| Version         | Changes                                                          |
+=================+==================================================================+
| 3.7.4           | Add filtering. resample, param1, param2, param3, r parameters    |
|                 | Make filtering with chroma placement aware resizers.             |
+-----------------+------------------------------------------------------------------+
| AviSynth+ 3.6.2 | Fix: AddBorders did not pass frame properties                    |
+-----------------+------------------------------------------------------------------+
| AviSynth+ 3.5.0 | New ``color_yuv`` parameter like in BlankClip                    |
+-----------------+------------------------------------------------------------------+
| AviSynth+ r2397 | AddBorders missing l/r/top/bottom vs. subsampling check for YUVA |
+-----------------+------------------------------------------------------------------+
| AviSynth 2.6.0  | Bugfix: Fixed RGB24 AddBorders with ``right=0``                  |
+-----------------+------------------------------------------------------------------+
| AviSynth 2.0.7  | New ``color`` parameter                                          |
+-----------------+------------------------------------------------------------------+

$Date: 2025/03/23 11:37:04 $

.. _aspect ratio:
    http://avisynth.nl/index.php/Aspect_ratios
.. _mod2:
    http://avisynth.nl/index.php/Modulo
.. _mod4:
    http://avisynth.nl/index.php/Modulo
.. _Rec.601:
    https://en.wikipedia.org/wiki/Rec._601
.. _macroblock:
    https://en.wikipedia.org/wiki/Macroblock
