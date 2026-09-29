
AviSynth Syntax - Function objects
==================================

This feature introduces function objects into scripts.

Functions (along their parameter definition) can also appear as standard Avisynth 
variables

Function objects can even be inlined, like lambdas.

They can work even with variable capture [] (like in GRuntT args).

In Avisynth+ runtime filters (e.g. ScriptClip) can accept function objects as well, 
where in the classic syntax a stringified script would appear.

    The input clip
    The function object. See Function_objects
    Capture variables (part of function object syntax). 

In this case Script is not an assembled string but a prewritten function, either inline or separately written. 

The concept was first introduced in nekopanda's Avisynth+ fork.

Function object basics
----------------------

Concept of function objects was introduced in Avisynth Neo and was backported to Avisynth+ 
(avaliable from v3.6). Function object is a new variable type. Once declared, they can be assigned 
to a variable, and use as function arguments.

:doc:`Internal functions can be casted <syntax_internal_functions_function_object>` to function objects, this helpes a cleaner syntax for some 
existing internal function (e.g. ScriptClip).

A user defined function or an external plugin can even take such type as an argument in the parameter list.
(in FilterSDK: ``AVSValue type='n'``)

The syntax allows capturing the variable at that point with '[]' before the formal argument.

*Examples*

Basic example
::

    a = function(int x, int y) {
        return x + y
    }
    MessageClip(String(a)) #Function
    b = a
    MessageClip(String(b)) #Function
    MessageClip(String(a == b)) # true

Take as an argument
::

    function MyFunc(func f) {
        return f(2, 3)
    }
    a = MyFunc(function(x, y) {
        return x + y
    })
    MessageClip(String(a)) # 5

return as a return value
::

    function MyFunc() {
        return function(x, y) {
            return x + y
        }
    }
    a = MyFunc()(2, 3)
    MessageClip(String(a)) # 5

Capture example
::

    function MyFunc() {
        x = 2
        y = 3
        return function[x, y]() {
            return x + y
        }
    }
    a = MyFunc()()
    MessageClip(String(a)) # 5

Specification details
~~~~~~~~~~~~~~~~~~~~~

Function objects are functions defined with the new syntax.
::

    function [] () {...}

It looks like an unnamed function compared to the usual function definitions so far.
``'[]'`` Does not have to be optional.

Functions defined in the normal function format are not function objects (for compatibility).
::

    function MyFunc() {return 123}
    a = MyFunc
    MessageClip(String(a)) # 123 (Not Function)

Similarly, built-in functions and plug-in functions are not function objects.
::

    a = Invert # Error: I don't know what 'Invert' means.

Functions that are not function objects can be made into function objects by using the 
:doc:`Func <syntax_internal_functions_function_object>` function.
::

    a = func(Invert)
    Version().a() # Invert a clip

A new 'func' has been added to the value type.
::

    function MyFunc(func x, func y, int z) {
      return x () + y () + z
    }
    a = MyFunc(function(){1}, function(){2}, 3)
    MessageClip(String(a)) # 6 (= 1 + 2 + 3)

The :doc:`IsFunction <syntax_internal_functions_boolean>` function that determines the function object has been added.
::

    a = function() {}
    MessageClip(String(IsFunction(a))) # true

Compared with GRunT
~~~~~~~~~~~~~~~~~~~

Let's compare function objects with ``GRunT``, a plugin from the Avisynth 2.6 era that makes 
ScriptClip easier to write.

The following code on the GRunT introduction page
::

    function bracket_luma(clip c, float th1, float th2) {
        Assert (0 <= th1 && th1 <th2 && th2 <= 255, "Invalid thresholds!")
        ScriptClip (c, "" "
            avl = AverageLuma ()
            avl <= th1? Last.BlankClip (): avl> = th2? last.BlankClip (color = color_white): last
        "" ", args =" th1, th2 ", local = true)
    }

It is a sample to appeal the goodness of GRunT, but with Avisynth+ (first in Neo fork) it can 
be written as follows.
::

    function bracket_luma(clip c, float th1, float th2) {
        Assert (0 <= th1 && th1 <th2 && th2 <= 255, "Invalid thresholds!")
        ScriptClip(c, function [th1, th2] () {
            avl = AverageLuma()
            avl <= th1? Last.BlankClip() : avl> = th2? last.BlankClip(color = color_white) : last
        })
    }

There are the following differences compared to GRunT.

- There is no need to pass the processing content as a character string 
- Variables to be used can be written in a special syntax, so the amount of description is reduced.
- Supports 'function' type of input of built-in functions

Examples on runtime filters with extended syntax
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A version that can pass the function object has been added to some filters.
Formerly these only allowed and processed content as a script string.

ScriptClip
^^^^^^^^^^
::

    ScriptClip(clip clip, func filter [, bool show, bool after_frame])

*Examples:*
::

    Version()
    ScriptClip (function [] (clip c) {
        c.Subtitle(String(current_frame))
    })

Comparison with string input:
::

    # ScriptClip with text script input
    SSS2="""
    p=(current_frame)*pow(framecount(last)-1,-1)*100
    q=ceil(0.0625*((framecount(last))-(current_frame+1)))
      Subtitle(String(q,"%.0f")+String(" - ")+String(p,"%.2f")+String("% - ")+String(current_frame,"%.0f")) 
    """
    clip1 = Input.Scriptclip(SSS2,After_Frame=True)
    
    # With function object (declared inline)
    clip1 = Input.ScriptClip(function[](clip c) { p=(current_frame)*pow(framecount(last)-1,-1)*100 \
      q=ceil(0.0625*((framecount(last))-(current_frame+1))) \
      Subtitle(String(q,"%.0f")+String(" - ")+String(p,"%.2f")+String("% - ")+String(current_frame,"%.0f")) \
    }, After_Frame=True)

ConditionalFilter
^^^^^^^^^^^^^^^^^
::

    ConditionalFilter(clip testclip, clip source1, clip source2, func condition [, bool show])

*Example:*
::

    a = Version()
    b = a.Invert()
    ConditionalFilter(a, a, b, function [] (clip c) {
        current_frame<30 # if true return a else b
    })

ConditionalSelect
^^^^^^^^^^^^^^^^^
::

    ConditionalSelect(clip testclip, func get_index, clip source0 [, clip source1 ...] [, bool show])

*Example:*
::

    Version ()
    ConditionalSelect(function [] (clip c) {
        current_frame / 100
    }, subtitle("0"), subtitle("1"), subtitle("2"))

WriteFile system
^^^^^^^^^^^^^^^^
::

    WriteFile(clip clip, string filename, func expression1 [, func expression2 [, ...]] [, bool append, bool flush]) 
    WriteFileIf(clip clip, string filename, func expression1 [, func expression2 [, ...]] [, bool append, bool flush]) 
    WriteFileStart (clip clip, string filename, func expression1 [, func expression2 [, ...]] [, bool append]) 
    WriteFileEnd (clip clip, string filename, func expression1 [, func expression2 [, ...]] [, bool append]) 

*Example:*
::

    Version().ConvertToY()
    WriteFile("out.txt", function() {
        string(current_frame) + ":" + string(YPlaneMedian())
    })

Type cast
~~~~~~~~~
::

    Func 

See at conversion function :doc:`Func <syntax_internal_functions_function_object>`

::

    # write frame properties with function object
    ScriptClip("""propSetInt("frameprop_from_str",func(YPlaneMax))""")
    # write frame properties with traditional script string
    ScriptClip(function[](clip c) { propSetInt("frameluma_sc_func",func(AverageLuma)) })

Changelog
~~~~~~~~~
+----------------+------------------------------------------------------------+
| Version        | Changes                                                    |
+================+============================================================+
| Avisynth 3.6.0 | Added function objects                                     |
+----------------+------------------------------------------------------------+

--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

This page was backported from `Avisynth.nl <http://avisynth.nl/index.php/Function_objects>`_

$Date: 2024/01/0914:13:14 $

