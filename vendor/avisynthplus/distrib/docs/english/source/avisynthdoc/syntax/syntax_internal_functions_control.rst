
AviSynth Syntax - Script control functions
==========================================

They facilitate flow of control at language level (loading of scripts, arguments checks, etc.).

Apply
~~~~~
::

    Apply(string func_string [, arg1 [, arg2 [, ... [,argn]]]] )

Apply calls the function or filter func_string with arguments arg1, arg2,
..., argn (as many as supplied).

It provides a way to call a function or filter **by name** providing 
arguments in the usual way as in a typical function call.

Consequently,

* ``Apply("f", x)`` is equivalent to ``f(x)``
* which in turn is equivalent to ``Eval("f(" + String(x) + ")")``.

Note that the clip argument must be supplied - ``last`` is not implicitly assumed.

*Examples:*
::

    # here the same call to BicubicResize as in the Eval() example is shown
    Apply("BicubicResize", last, 352, 288)
    # Note that the clip argument must be supplied - 'Last' is not implicitly assumed
    
    ## the same action, using Eval
    Eval( "BicubicResize(" + String(new_width) + "," + String(new_height) + ")" )

::

    ## using a filter only if it exists (AVS+ only)
    ColorBars  
    return FunctionExists("MyFilter") 
    \ ? Apply("MyFilter", Last, "TEST") 
    \ : Last 

::

    ## using a filter only if it exists (AviSynth 2.6)
    function MyFilter(clip C, string s) { 
        return C.Subtitle("MyFilter: "+s, align=5)
    }
    ColorBars 
    try {
        Apply("MyFilter", Last, "TEST")
    } catch (err_msg) {
        # (ignore)
    }
    return Last


.. _syntax_internal_functions_control_eval:

Eval
~~~~
::

    Eval(expression [, string "name"])

Eval evaluates an arbitrary *expression* as if it was placed inside the
script at the point of the call to Eval and returns the result of evaluation
(either to the :doc:`variable <syntax_script_variables>` that is explicitly assigned to or to the last
special variable. 

It works exactly like Import below, except

*   The expression to be evaluated comes from a string instead of a file;
*   The values of ScriptName, ScriptFile and ScriptDir are not changed;
*   The current working directory (CWD) is not changed. 

Argument name will be shown in the error message beside the script name. Both will be followed 
with the line number in expression where the error occurred. 

Variables in your calling script are available within expression; global variables are not required. 

Note: Eval can return the result of any valid expression; usually a clip, but also a string, boolean etc. 

You can use Eval to construct and evaluate expressions dynamically inside your scripts, based on variable input data. 

Eval is useful as a control structure for creating multi-line block statements without requiring AviSynth+ features. 

Eval can be used to put aside the need to install external plugins if they are not actually used. 

*Examples:*
::

    ## Building an expression dynamically 
    ## calls BicubicResize(last, 352, 288)
    settings = "352, 288"
    Eval( "BicubicResize(" + settings + ")" )

::

    ## if...else control structure simulation
    option = true
    option  
    \ ? Eval("""
        Levels(0, 1.2, 255, 20, 235)
        Spline36Resize(720, 400)
        Sharpen(0.2)
    """) 
    \ : Eval("""
        BicubicResize(720, 400)
        Sharpen(0.3)
    """)

::

    ## using a filter only if it is needed; 
    ## in this example, SMDegrain only needs to be installed if the option is true.
    option = false
    option 
    \ ? Eval("""
        SMDegrain(tr=2, thSAD=250, contrasharp=true, refinemotion=true, lsb=true)
    """)
    \ : Last 

::

    ## accessing script variables
    ColorBars
    A=Subtitle("A", align=5)
    Eval("A")
    return Last ## returns clip 'A'

::


    ## setting script variables
    ColorBars
    A=Subtitle("A", align=5)
    Eval("B = A.Invert")
    return B ## returns clip 'A' with colors inverted

::

    ## Increment a global variable, based on a local variable
    Eval("global my_counter = my_counter + " + String(increment)) 

::

    ## multi-line example with comment and line continuation
    Eval("""
    ColorBars
    BicubicResize(352, 288)
    #FlipVertical
    Subtitle(
    \   "Width  = "  + String(Width) + "\n"
    \ + "Height = " + String(Height)
    \ , align=7, lsp=0)
    """)

::

    ## Empty expression 
    ## results in error: 'Defined(u) == false'
    u = Eval("#")   

::

    ## Error reporting
    ColorBars
    A=Subtitle("A", size=Height, align=2)
    Eval("""
    A
    foo("bar") ## ERROR!
    """, "eval_test_1") ## name for error reporting purposes
    return Last
    
    results in the error message:
    Script error: there is no function named "foo"
    (eval_test_1, line 3)
    (E:\_test.avs, line 6)


Import
~~~~~~
::

    Import(filename[, ...] [, bool utf8]))

Evaluates the contents of another script and returns the result of that evaluation.

It works exactly like Eval above, except

*   The expression to be evaluated comes from a file instead of a string;
*   The values of ScriptName, ScriptFile and ScriptDir are set to the current (imported) script;
*   The current working directory (CWD) is set to the current (imported) script.
*   ``utf8`` if true, assumes filename(s) are UTF8, else (default), assume ANSI. 

Functions, variables and loaded plugins declared inside the imported script are made available to the 
parent script. Import's return value can be assigned to a variable of the parent script; this is most
useful when the imported script ends with a clip. 

Typically Import is used to make library functions available to the parent script, and the return
value is not used. However this is simply a convention; it is not enforced by the :doc:`AviSynth Syntax <syntax>`.

See also the dedicated :doc:`Import <../corefilters/import>` page in 
:doc:`Internal filters <../corefilters>` for other possible uses.

Select
~~~~~~
::

    Select(index, item0 [, item1 [, ... [, itemn]]])

Returns the item selected by the index argument, which must be of int type (0
returns ``item0``, 1 returns ``item1``, ..., etc). Items can be any :doc:`script variable <syntax_script_variables>`
or expression of any type and can even be mixed.

If ``index`` is out of range, an error is raised. 

*Examples:*
::

    # select a clip-brush from a set of presets
    idx = 2
    brush = Select(idx, AviSource("round.avi"), 
    \        rectangle, diagonal, diagonal.FlipHorizontal)

Note - all branches are evaluated:
::

    index=1
    Select(index, "zero", "one", "two", 
    \        Assert(false, "Select evaluates all branches")) 
    ## NOTE this code does not run - it throws Assert error
    ## because Select evaluates all branches

If this is not desired, use the conditional execution operator:
::

    index=1
    x = (index==0) ? "zero"
    \ : (index==1) ? "one"
    \ : (index==2) ? "two"
    \ : Assert(false, "index out of range")
    


Default
~~~~~~~
::

    Default(x, d)

Returns *x* if ``Defined(x)`` is true, *d* otherwise. *x* must either be a
function's argument or an already declared script variable (ie a variable
which has been assigned a value) else an error will occur.

*Examples:*
::

    function myfunc(clip c, ..., int "strength") {
        ...
        strength = Default(strength, 4) # if not supplied make it 4
        ...
    }


Assert
~~~~~~
::

    Assert(condition [, err_msg])

Does nothing if ``condition`` is true; throws an error, immediately terminating
script execution, if ``condition`` is false. In the later case ``err_msg``, if
supplied, is presented to the user; else the standard message "Assert:
assertion failed". shows up.

*Examples:*
::

    function myfunc(clip c, ..., int "strength") {
        ...
        strength = Default(strength, 4) # if not supplied make it 4
        Assert(strength > 0, "'strength' must be positive")
        ...
    }


NOP
~~~
::

    NOP()

This is a no-operation function provided mainly for conditional execution
with non-return value items such as :doc:`Import <../corefilters/import>`, when no "else" condition is
desired. That is, use it whenever the :doc:`AviSynth Syntax <syntax>` requires an
operation (such as with the ?: operator) but your script does not need one.

Return value: 0 (int type).

*Examples:*
::

    preset = want_presets ? AviSource("c:\presets\any.avi") : NOP
    ...
    loadlib ? Import("my_useful_functions.avs") : NOP


UnDefined
~~~~~~~~~
::

    UnDefined()

Returns the undefined state.

It's the state for which Defined() returns false.

*Examples:*
::

    x = Undefined()
        Defined(x) # = true

--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2025-02-25 15:25:59-05:00 $

.. _planar: http://avisynth.nl/index.php/Planar
.. _memory alignment used in the AVIFile output emulation (not yet written):
    http://avisynth.nl/index.php/AVIFile_output_emulation
