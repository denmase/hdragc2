
Import
======
::

    Import(filename[, ...] [, bool utf8]))

Evaluates the contents of another script and returns the result of that evaluation.

It works exactly like :ref:`Eval <syntax_internal_functions_control_eval>`, except

*   The expression to be evaluated comes from a file instead of a string;
*   The values of ScriptName, ScriptFile and ScriptDir are set to the current (imported) script;
*   The current working directory (CWD) is set to the current (imported) script.
*   ``utf8`` if true, assumes filename(s) are UTF8, else (default), assume ANSI. 

Functions, variables and loaded plugins declared inside the imported script are made available to the 
parent script. Import's return value can be assigned to a variable of the parent script; this is most
useful when the imported script ends with a clip. 

Typically Import is used to make library functions available to the parent script, and the return
value is not used. However this is simply a convention; it is not enforced by the :doc:`AviSynth Syntax <../syntax/syntax>`.

Possible scenarios (an indicative list) where the return value could be of use is for the library script to:

-   Storing multiple script-functions, variables and global variables for reuse by scripts 
    (creation of script libraries).
-   Retrieving pre-built streams.
-   Retrieving dynamically configured pre-built streams (the core idea is that the importing 
    script declares some global variables which the imported script uses to configure the 
    stream that will return). 
-   indicate whether it succesfully initialised itself (a bool return value),
-   inform for the number of presets found on disk (an int return value);

the value then could be tested by the calling script to decide what action to
take next.

*   Note 1: Since the contents of the imported script are evaluated at the point of invocation, 
    it is possible by enclosing the Import call in a nested scope (for example inside a function) 
    to make available to the importing script the functions and globals of the imported script 
    without its script-level variables. 
*   Note 2: Any script with the AVSI extension in the AviSynth Plugins folder is automatically 
    imported. This is useful for making script functions available to any new script you create 
    without having to copy and paste. 

*Examples:*
::

    # B.avsi
    A.Invert
    
    ColorBars
    A=Subtitle("A", align=5) ## create clip 'A'
    Import("B.avsi")
    return Last ## returns clip 'A' with colors inverted

::

    ## here we do not care about the return value (mylib.avsi contains only functions)
    Import("mylib.avsi")  
    ...
    ## mysources.avsi loads predetermined file names from a folder into globals
    okflag = Import("mysources.avsi")  
    source = okflag ? global1 + global2 + global3 : BlankClip()

$Date: 2024/01/06 19:00:00 $
