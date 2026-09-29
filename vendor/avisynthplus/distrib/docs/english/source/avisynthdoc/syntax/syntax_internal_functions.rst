
AviSynth Syntax - Internal functions
====================================

In addition to :doc:`internal filters <../corefilters>` AviSynth has a fairly large number of
other (non-clip) internal functions. The input or/and output of these
functions are not clips, but some other variables which can be used in a
script. They are roughly classified as follows:

-   :doc:`Boolean functions <syntax_internal_functions_boolean>`

They return true or false, if the condition that they test holds or not,
respectively.

-   :doc:`Control functions <syntax_internal_functions_control>`

They facilitate flow of control (loading of scripts, arguments checks, global
settings adjustment, etc.).

-   :doc:`Type conversion functions <syntax_internal_functions_conversion>`

They convert between different types.

-   :doc:`Numeric functions <syntax_internal_functions_numeric>`

They provide common mathematical operations on numeric variables.

-   :doc:`Trigonometry functions <syntax_internal_functions_trig>`

They provide trigonometry functions.

-   :doc:`Bit functions <syntax_internal_functions_bit>`

They provide bitwise functions.

-   :doc:`Casting to function object <syntax_internal_functions_function_object>`

Casting an internal function to a function object.

-   :doc:`Runtime functions <syntax_internal_functions_runtime>`

These are internal functions which are evaluated at every frame. They can be
used inside the scripts passed to runtime filters (:doc:`ConditionalFilter <../corefilters/conditionalfilter>`,
:doc:`ScriptClip <../corefilters/conditionalfilter>`, :doc:`FrameEvaluate <../corefilters/conditionalfilter>`) to return information for a frame.

-   :doc:`Script functions <syntax_internal_functions_script>`

They provide AviSynth script information.

-   :doc:`String functions <syntax_internal_functions_string>`

They provide common operations on string variables.

-   :doc:`Version functions <syntax_internal_functions_version>`

They provide AviSynth version and Avisynth/Operating System bitness information.

-   :doc:`Frame property functions <syntax_internal_functions_frame_properties>`

They provide manipulation (read, write, delete) of frame properties.

This section contains an overview on the concept and lists the quasi-standard
frame properties as well.

-   :doc:`Multithreading <syntax_internal_functions_multithreading_new>` (Avisynth+)

Controlling the threads mechanism.

-   :doc:`Global options and resource control (memory, CPU, cache) <syntax_internal_functions_global_options>`

Methods for fine-tune resources: memory, cache strategy and CPU environment settings.

Global variables which affect specific audio or video (VfW export) features.

-   :doc:`Debugging helper function <syntax_internal_functions_debug>`

Debugging and troubleshooting helper functions. Filter graphs, logging.

-   :doc:`History: Avisynth 2.6 Multithreading and memory limit functions <syntax_internal_functions_multithreading>`

(Historical: Avisynth 2.6) Controlling the threads and the maximum used memory.

Back to :doc:`Avisynth Syntax <syntax>`.
Back to :doc:`Avisynth Syntax ref <syntax_ref>`.
Back to :doc:`The full Avisynth grammar <syntax_the_full_grammar>`.

$Date: 2024/01/10 10:38:00 $
