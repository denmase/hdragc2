
AviSynth Syntax - 2.6 Multithreading and memory functions
=========================================================

A historical page showing how memory settings and multithreading for classic
``Avisynth 2.6`` worked.

SetMemoryMax
~~~~~~~~~~~~
::

    SetMemoryMax(amount)

Sets the maximum memory (in MB) that AviSynth uses for its internal Video
Frame cache to the value of *amount*. From v2.5.8, setting to zero just
returns the current Memory Max value. In the 2.5 series the default Memory
Max value is 25% of the free physical memory, with a minimum of 16MB. From
rev 2.5.8 RC4, the default Memory Max is also limited to 512MB.

+-----------------------------+-----+-----+-----+-----+------+------+------+
| Free                        | <64 | 128 | 256 | 512 | 1024 | 2048 | 3072 |
+=============================+=====+=====+=====+=====+======+======+======+
| Default Max v2.57 and older | 16  | 32  | 64  | 128 | 256  | 512  | 768  |
+-----------------------------+-----+-----+-----+-----+------+------+------+
| Default Max since v2.58 RC4 | 16  | 32  | 64  | 128 | 256  | 512  | 512  |
+-----------------------------+-----+-----+-----+-----+------+------+------+

In some versions there is a default setting of 5MB, which is quite low. If
you encounter problems (e.g. low speed) try to set this values to at least
32MB. Too high values can result in crashes because of 2GB address space
limit.  Return value: Actual MemoryMax value set.

*Examples:*
::

    SetMemoryMax(128)

GetMTMode, SetMTMode
~~~~~~~~~~~~~~~~~~~~
::

    GetMTMode(threads)
    SetMTMode(mode, threads)

These functions enable AviSynth to use more than one thread when processing
filters. This is useful if you have more than one cpu/core or hyperthreading.
This feature is still experimental.

::

    GetMTMode(bool threads):

If *threads* is set to true GetMTMode returns the number of threads used else
the current mode is returned (see below). Default value false.

::

    SetMTMode(int mode, int threads):

Place this at the first line in the avs file to enable temporal (that is more
than one frame is processed at the same time) multithreading. Use it later in
the script to change the mode for the filters below it.

*mode*: There are 6 modes 1 to 6. Default value 2.

-   Mode 1 is the fastest but only works with a few filter.
-   Mode 2 should work with most filters but uses more memory.
-   Mode 3 should work with some of the filters that doesn't work with
    mode 2 but is slower.
-   Mode 4 is a combination of mode 2 and 3 and should work with even
    more filter but is both slower and uses more memory
-   Mode 5 is slowest but should work with all filters that doesn't
    require linear frameserving (that is the frames come in order (frame
    0,1,2 ... last).
-   Mode 6 is a modified mode 5 that might be slightly faster.

*threads*: Number of threads to use. Set to 0 to set it to the number of
processors available. It is not possible to change the number of threads
other than in the first SetMTMode. Default value 0.

**Example:**

::

    SetMTMode(2,0)                      # enables multithreading using thread = to
                                        # the number of available processors and mode 2
    LoadPlugin("...\LoadPluginEX.dll")  # needed to load avisynth 2.0 plugins
    LoadPlugin("...\DustV5.dll")        # Loads Pixiedust
    Import("limitedsharpen.avs")
    src = AviSource("test.avi")
    SetMTMode(5)                        # change the mode to 5 for the lines below
    src = src.ConvertToYUY2.PixieDust() # Pixiedust needs mode 5 to function.
    SetMTMode(2)                        # change the mode back to 2
    src.LimitedSharpen()                # because LimitedSharpen works well with mode 2
    # display mode and number of threads in use
    Subtitle("Number of threads used: " + String(GetMTMode(true))
     \ + " Current MT Mode: " + String(GetMTMode()))

--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2024/01/06 20:02:00 $
