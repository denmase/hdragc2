
MultiOverlay
============

The ``MultiOverlay`` filter allows you to copy and paste one or more source clips onto a 
base clip. The source clips can be of different sizes, but they must have the same color 
format as the base clip.

This filter performs a straightforward BitBlt (copy-paste) operation of the original clips or their sub-areas.
It does not support transparency, mask clips, or blending modes—just a simple copy.

.. rubric:: Syntax and Parameters

::

    MultiOverlay (clip base_clip, clip overlay_clips[], int overlay_params[] )

.. describe:: base_clip

    the underlying clip which determines the size and all other video
    and audio properties of the result.

.. describe:: overlay_clips

    One or more source clips. Color formats must match base_clip.

.. describe:: overlay_params

    List of integer values.
    
    You must provide either two or six parameters for each source clip.
    The parameters needn't be separated between clips.
    
    The two parameter version needs 
    
    - ``target_x`` and ``target_y``.
    
    The six parameter version reads
    
    - ``source_x`` and ``source_y``.
    - ``width_to_copy`` and ``height_to_copy``.

    ``target_x`` and ``target_y`` and the offset positions of the actual ``overlay_clip``
    
    When additional parameters are specified, it is possible to read from an arbitrary 
    ``source_x`` and ``source_y`` position of the input clip, with a specified width and height:
    ``width_to_copy`` and ``height_to_copy``.
    
    Rules:
    
    - ``target_x`` and ``target_y`` must be >= 0
    - ``source_x``+``width_to_copy`` cannot exceed the full width of the actual ``overlay_clip`` 
    - ``source_y``+``height_to_copy`` cannot exceed the full height of the actual ``overlay_clip`` 
    - All positions and widths must fulfill the subsampling rules of the video format.
      E.g. all positions and dimensions must be mod2 for a YV12 clip.
    
    But:
    
    - ``source_x`` and ``source_y`` are allowed to be negative values. Obviously, the off-clip contents
      will shift the visual experience.

Other notes
-----------

Audio, FrameRate and FrameCount are taken from the first clip.

This filter was originally developed for :doc:`AddBorders <addborders>`, as a helper filter,
in which the eight smaller blurred areas are copied back into the transient areas of the 
original clip atop the boundary of the new borders as a ringing prevention measure.

Examples
~~~~~~~~

::

    ColorbarsHD()
    Info() # let us have some text to see the effect
    b=last.Crop(0, 0, 80,80)
    c=last.Crop(80, 0, 80,80)
    # two clips, 
    # copy "b" from sub-positions (40,40) a 30x30 area to (-10,300) onto original clip 
    # copy "c" from sub-positions (20,20) a 60x60 area to (80,300) onto original clip
    #      (part or the 2nd clip is off-screen, not drawn)
    MultiOverlay(last, b,c, \
      -10, 300, 40, 40, 30, 30, \
      80, 300, 20, 20, 60, 60 \
      )

    # copy the whole 80x80 "b" clip to (0,400) onto original clip
    # copy the whole 80x80 "c" clip to (80,400) onto original clip
    MultiOverlay(last, b,c, \
      0, 400, \
      80, 400)


Changelog
----------

+-----------------+---------------------------------------------------------------+
| Version         | Changes                                                       |
+=================+===============================================================+
| 3.7.4           | Initial release                                               |
+-----------------+---------------------------------------------------------------+


$Date: 2025/03/14 12:58:00 $

