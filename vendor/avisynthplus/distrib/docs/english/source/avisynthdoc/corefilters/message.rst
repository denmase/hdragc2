
MessageClip
===========

**MessageClip** produces a clip containing a text message. Used internally for 
error reporting.

The font face is "Arial". The font size is between 24 points and 9 points - 
chosen to fit, if possible, in the width by height clip. The pixeltype is RGB32
and 240 frames in length.
On non-Windows GDI systems fixed Terminus font is used instead.


Syntax and Parameters
----------------------

::

    MessageClip (string message, int "width", int "height", bool "shrink",
                 int "text_color", int "halo_color", int "bg_color", bool "utf8")

.. describe:: message

    The message to be displayed. Required.

.. describe:: width, height

    Width and height of the resulting clip. By default, the width and height are 
    chosen such that it can display the message with size 24 points.
    
    Default: -1, -1

.. describe:: shrink

    | If true, and ``width`` and/or ``height`` are specified, the clip resolution 
      is reduced to a smaller size, if possible, to fit the text.
    | If false, the text will appear at the top-center of the video frame.
    
    Default: false

.. describe:: text_color, halo_color, bg_color

    Colors for font fill, outline and background respectively. See the
    :doc:`colors <../syntax/syntax_colors>` page for more information on 
    specifying colors. Default text color is yellow and halo and background 
    color are black.
    
    Default: $FFFFFF, $000000, $000000

.. describe:: utf8

    | If true, message string is interpreted as an utf8 string.
    | Available since 3.7.6.
    
    Default: false on windows GDI systems, true otherwise

Examples
--------

Displaying AviSynth+ version information with ``MessageClip``::

    MessageClip(VersionString)
    
.. image:: pictures/messageclip-versionstring.png


+------------+--------------------------------+
| Changelog: |                                |
+============+================================+
| 3.7.6      | Added utf8 parameter           |
+------------+--------------------------------+

$Date: 2025/08/31 17:52:00 $
