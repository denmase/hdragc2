
AviSynth+ Syntax - Multithreading functions
===========================================

By default, AviSynth is single-threaded.

This page documents how to enable multi-threading: use more than 
one thread when processing filters. This is useful if you have more than 
one cpu/core or hyperthreading, so nearly always.

In this document the abbreviation MT is used for multithreading.

On startup default MT mode is "MT_MULTI_INSTANCE". Plugins or SetFilterMTMode can override this.

AviSynth+ does the multithreading differently than classic Avisynth 2.6.

In AviSynth+, you specify the MT-mode for only specific filters, and those filters will then automatically 
use their own mode, even if there were other MT-modes in between. This means that for old filters you can 
specify all the MT modes at the beginning without polluting your script. 

You can even make a SetMTMode.avsi if you wish and let it autoload for all of your scripts, 
or import() it from their top. This is much cleaner, and it allows you to maintain all your 
MT-modes centrally at a single place. To make this distinction clear from AviSynth+, Avisynth 2.6's 
SetMTMode() is called SetFilterMTMode() in AviSynth+. 

Notes

1.  Source filters are auto detected by Avisynth (such filters have no input clip parameters) and are 
    always set to MT_SERIALIZED (no threaded calls)
2.  Mode 3 (MT_SERIALIZED) is evil. It is necessary for some filters, and it is usually no problem 
    for source filters, but it can literally completely negate all advantages of MT, if such a filter 
    is placed near the end of your script. In general, avoid such calls if you want performance.

    Programmers can check ``class MtModeEvaluator`` in ``avisynth.cpp``.

    <todo check: what's the effect of using multiple Prefetch sections?>
3.  Though Avisynth video frame caches will save you a lot of memory in single-threaded scripts, 
    but due to the way they work, they will also use more memory than before with MT enabled. 

    The memory usage will scale much closer with the number of threads you have. Just something to keep in mind. 

Notes for filter writers:

-   Always specify your filters' required MT mode by answering with an ``MT_xxxx`` constant on 
    ``CACHE_GET_MTMODE`` in your ``SetCacheHints`` implementation.
    But don't go too safe with specifying MT_SERIALIZED, do it only when it's really necessary: See Note 2.


MT Modes explained
==================

*   MT_NICE_FILTER:

    Some filters (like nnedi3) use some buffers to do their dirty work and with mode 1 you get 
    multiple threads writing data from different frames to the same buffer. 
    This causes corruption when later someone tries to read from this buffer and gets not what was expected. 
    Most of the "more complicated" filters use some kind of temporary storage thus won't work 
    well with this mode. Simple filters might. 

*   MT_MULTI_INSTANCE:

    Mode 2 doesn't have this issue because multiple threads will get their own buffers and no data will be shared.
    Hence mode 2 is the "default" mode which should work with most filters, 
    but it wastes memory like crazy (take SangNom2 for example - for 1080p YV12 frame, 
    size of temporary buffers is about 10MB, so with 4 threads you get 40MBs on single filter invocation.
    Now add some usual supersampling to this and multiple invocations in most aa scripts and... you get the idea).

*   MT_SERIALIZED: 

    If the filter requires sequential access or uses some global storage, then mode 3 is the only way to go. 
    Source filter (filters without clip parameter) are autodetected, they do not need an 
    explicit MT mode setting, they will automatically use MT_SERIALIZED. 

*   MT_SPECIAL_MT: 

    Experimental, no longer is needed. It was used only for MP_Pipeline, the filter is like a source filter 
    (no input clip parameter), internally multithreaded, and suffer heavy performance degradation from any 
    of the three regular mt modes. Really, this is a workaround. Available from AviSynth+ version r2440. 
    Avisynth+ 3.6 has gained serious MT fixes from Neo fork, this mode is probably not needed anymore. 

Prefetch
========
::

    Prefetch (clip, int "threads", int "frames") 

.. describe:: clip

    Input clip. 

.. describe:: int threads

    Number of threads to use. If it is 0, it passes without doing anything. 
    
    default: (number of logical cores in the system) +1 

.. describe:: int frames

    Expert parameter.

    Number of frames to prefetch. Again, if it is 0, it passes without doing anything. 

    default: threads * 2 

In the original Avisynth+ (before v3.6), only one ``Prefetch`` per script was supported, 
typically placed at the very end of the script.

Starting from Neo fork you can use as many as you like. Also, a "frames" argument has been added 
to specify the number of frames to prefetch. Neo's multithreading enhancements were backported 
to the mainstream Avisynth+ and are available since v3.6

*Examples:*
::

    # This line causes all filters that don't have an MT mode explicitly use mode 2 by default.
    # Mode 2 is a relatively safe choice until you don't know most of your calls to be either mode 1 or 3.
    # Compared with mode 1, mode 2 trades memory for MT-safety, but only a select few filters will work with mode 1.
    SetFilterMTMode("DEFAULT_MT_MODE", 2)
    or
    SetFilterMTMode("DEFAULT_MT_MODE", MT_MULTI_INSTANCE)

Setting a source filter to MT mode 3 (MT_SERIALIZED) is no longer needed.
::

    # FFVideoSource(), like most of all source filters, needs MT mode 3. 
    # Note: starting  with AviSynth+ r2069, it will now automatically recognize source filters.
    # If it sees a source filter which has no MT-mode specified at all, it will automatically use 
    # mode 3 instead of the default MT mode.
    SetFilterMTMode("FFVideoSource", 3)
    or 
    SetFilterMTMode("FFVideoSource", MT_SERIALIZED)
    
    # Now comes your script as usual
    FFVideoSource(...)
    Trim(...)
    QTGMC(...)
    ...
    
    # Enable MT!
    Prefetch(4)

Pipeline parallelization

::

    Filtering A
    Prefetch(1,4)
    Filtering B
    Prefetch(1,4)
    Filtering C
    Prefetch(1,4)

Prefetch (1,4) makes one thread stand and read four frames ahead.

In the above example, the filtering processes A, B, and C are executed in parallel in a pipeline.
Since the number of threads of each Prefetch is arbitrary, for example, filter processing B is heavy, 
so if you want to increase the number of parallels by that amount, you can increase the number of threads as follows:

::

    Filtering A
    Prefetch(1,4)
    Filtering B
    Prefetch(4)
    Filtering C
    Prefetch(1,4)

SetFilterMTMode
===============
::

    SetFilterMTMode (string filtername, int mode, bool "force")

Depending on the filter type, different multithreading rules can be set.
The filter developer can specify which one is used, this can be done programatically,
(self-registering) so no user intervention is necessary.

When ``force`` = false then the function has effect only if the filter doesn't 
set (self-register) own MT mode. Usually the recent versions of the plugins have set MT mode.

For old filters, this method can set the proper behaviour, how Avisynth core will treat it MT-wise.
Meaningful only if this mode is different that the default MT mode.

.. describe:: string filtername  =

    name of the filter you want to set an MT Mode for. You cannot set the MT mode on script function calls, 
    only on binary (external plugin) filters.
    
    ``"DEFAULT_MT_MODE"``, sets the default MT mode for all filters that do not have an MT mode 
    explicitly set. Does not affect for source filters and filters that self-register their own MT mode. 

.. describe:: int mode

    Sets MT Mode, there are three basic MT modes (1,2,3) and an experimental workaround mode (4). Instead of the numbers, you can also use symbolic names for MT modes:

    *   1 : MT_NICE_FILTER
    *   2 : MT_MULTI_INSTANCE
    *   3 : MT_SERIALIZED
    *   (4 : MT_SPECIAL_MT do not use, early workaround setting for a problem, which has been solved since then)

.. describe:: bool force = false 

    Force MT mode. Default is false.
    Override the setting even if filter was registering its MT mode programatically.
    When false, MT mode of self-registering filters won't be changed.

As a side effect, a filter with MT_SERIALIZED (either set by ``SetFilterMTMode`` or self-registering it) 
can downgrade the MT modes of the filters in the chain before the filter, despite of they have their 
own ``SetFilterMTMode``. It has performance effects as mentioned in Notes 2.

There was a discussion about this topic:
https://forum.doom9.org/showthread.php?p=1823754#post1823754
answering on
https://forum.doom9.org/showthread.php?p=1823726#post1823726


*Examples:*

::

    SetFilterMTMode(X, MT_SERIALIZED, force=true)
    sourceFilter
    
    # filterY has own MT mode MT_MULTI_INSTANCE but now is changed to MT_SERIALIZED
    filterY
    
    # filterZ has own MT mode MT_NICE_FILTER but now is changed to MT_SERIALIZED
    filterZ
    
    filterX

::

    SetFilterMTMode(X, MT_MULTI_INSTANCE, force=true)
    sourceFilter
    
    # filterY has own MT mode MT_SERIALIZED, not changed
    filterY
    
    # filterZ has own MT mode MT_NICE_FILTER but now is changed to MT_MULTI_INSTANCE
    filterZ
    
    filterX

::

    SetFilterMTMode(X, MT_NICE_FILTER, force=true)
    sourceFilter
    
    # filterY has own MT mode MT_SERIALIZED, not changed
    filterY
    
    # filterZ has own MT mode MT_MULTI_INSTANCE, not changed
    filterZ
    
    filterX

::

    SetFilterMTMode(X, MT_SERIALIZED, force=true)
    SetFilterMTMode(Y, MT_NICE_FILTER, force=true)
    sourceFilter
    
    # filterY has own MT mode MT_MULTI_INSTANCE but now is changed to MT_SERIALIZED
    # SetFilterMTMode(Y, MT_NICE_FILTER, force=true) doesn't have effect
    filterY
    
    # filterZ has own MT mode MT_NICE_FILTER but now is changed to MT_SERIALIZED
    filterZ
    
    filterX

::

    SetFilterMTMode(X, MT_MULTI_INSTANCE, force=true)
    SetFilterMTMode(Y, MT_SERIALIZED, force=true)
    sourceFilter
    
    # filterY has own MT mode MT_MULTI_INSTANCE but now is changed to MT_SERIALIZED
    filterY
    
    # filterZ has own MT mode MT_NICE_FILTER but now is changed to MT_MULTI_INSTANCE
    filterZ
    
    filterX

--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2024/01/07 09:37:00 $
