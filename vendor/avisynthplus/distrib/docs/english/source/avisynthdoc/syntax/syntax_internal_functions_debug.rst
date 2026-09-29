
AviSynth Syntax - Debug helper function
=======================================

Filter graph handling and logging.

.. _syntax_debug_filtergraph:

Filter graph
------------

Switch it on by putting ``SetGraphAnalysis(true)`` at the beginning of the script.
Dump to text file with ``DumpFilterGraph``. E.g. ``DumpFilterGraph("graph.txt", mode=2)``
Output is in "dot" format, it can be converted to an image with Graphviz as follows:
::

    dot -Tsvg graph.txt -o graph.svg

SetGraphAnalysis
~~~~~~~~~~~~~~~~
::

    SetGraphAnalysis (bool)

Enables (true) or disables (false) graph node insertion into the instantiated filter.
To output a filter graph, a graph node must be inserted in the filter.
When a graph node is inserted, performance may decrease slightly due to the increase of internal function calls.
(In most cases, there is no observable performance degradation.)

DumpFilterGraph
~~~~~~~~~~~~~~~
::

    DumpFilterGraph (clip, string "outfile", int "mode", int "nframes", bool "repeat")

Outputs a filter graph.

.. describe:: clip

    Clip to output filter graph

.. describe:: string outfile = ""

    Output file path

.. describe:: int mode = 0

.. describe:: int nframes = -1

     Outputs the filter graph when processing the specified frame. The cache size and memory usage of 
     each filter at that time are output together. This is effective when you want to know the memory 
     usage of each filter. If -1, output when DumpFilterGraph is called (before the frame is processed).

.. describe:: bool repeat = false

    Valid only when nframes> 0. Outputs a filter graph repeatedly at nframes intervals.

Logging
-------

SetLogParams
~~~~~~~~~~~~
::

    SetLogParams([string target, int level])

Sets a file path for a log file, used by LogMsg and internal error reporting.

.. describe:: string target

    Names a file which will be created when the script loads. If attempting to create or write to 
    target fails, the script will raise an error immediately. If the file exists, new log entries
    will be appended to the end. If omitted, target defaults to ``stderr``.

.. describe:: int level

    Sets the log verbosity; it can be one of the following: 

    - LOG_ERROR (1) creates the fewest log entries
    - LOG_WARNING (2)
    - LOG_INFO (3) creates the most log entries 

    If omitted, level defaults to LOG_INFO. 

    For examples see ``LogMsg`` below. 

LogMsg
~~~~~~
::

    LogMsg(string, int)

Creates a new log entry.

.. describe:: string (required)

    specifies the log message.

.. describe:: int (required)

    specifies the log entry level: see ``SetLogParams`` above.

*Examples:*
::

    ## creating file and set path for future log entries:
    SetLogParams("<path>\_test1.log", LOG_INFO)

log content at this point:

::

    (empty)

::

    ## logging an INFO message:
    SetLogParams("<path>\_test2.log", LOG_INFO)
    LogMsg("this is a test", LOG_INFO)

log contents at this point:
::

    ---------------------------------------------------------------------
    INFO: this is a test

::

    ## logging a script error:
    SetLogParams("<path>\_test3.log", LOG_INFO)
    foo("bar") ## ERROR!

log contents (redundant entries are common):
::

    ERROR: Script error: There is no function named 'foo'.
    ---------------------------------------------------------------------
    ERROR: Script error: There is no function named 'foo'.
    (<path>\_test.avs, line 35)

::

    ## logging INFO context for script error:
    SetLogParams("<path>\_test4.log", LOG_INFO)
    function MyFunction(clip C)
    {
        C
        try {
            foo("bar") ## ERROR!
        } catch (err_msg) {
            msg2 = "Error in MyFunction: "
            LogMsg(Time("%Y-%m-%d %I:%M:%S %p, %z") + ": " + msg2, LOG_INFO)
            #Assert(false, msg2 + err_msg) ## optional: stop script, else continue
        }
        return Last
    }

log contents (redundant entries omitted):

::

    ---------------------------------------------------------------------
    ERROR: Script error: There is no function named 'foo'.
    (<path>\_test.avs, line 54)
    ---------------------------------------------------------------------
    INFO: 2017-11-12 11:03:41 AM, -0500: Error in MyFunction:
    (<path>\_test.avs, line 54)

Changelog
---------
+----------------+----------------------------------+
| Version        | Changes                          |
+================+==================================+
| AviSynth 3.7.4 | Fix SetLogParams defaults        |
+----------------+----------------------------------+
| AviSynth+      | all of them                      |
+----------------+----------------------------------+

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2024/04/10 9:43:00 $
