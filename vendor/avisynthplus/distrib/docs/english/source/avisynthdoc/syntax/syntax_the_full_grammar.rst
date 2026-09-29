
The full AviSynth grammar
=========================

A new, revised version of syntax, syntax_ref, backported from 
`Avisynth.nl: The full Avisynth grammar <http://avisynth.nl/index.php/The_full_AviSynth_grammar>`_

Introduction
~~~~~~~~~~~~

From the perspective of the AviSynth interpreter each script is a series of tokens. The general 
term token corresponds to the basic building element of a script (if we imagine a script as a wall, 
then the tokens are the bricks). The AviSynth grammar is the set of rules (the recipe) for 
identifying and grouping tokens into higher-level structures.

We present those rules in the following sections, in a bottom-up fashion (from low-level to 
higher-level constructs). However, for a reader with a basic understanding of programming that 
wants a quick tour of the language another road is possible: start directly with the 
Expressions and Statements section and visit previous sections if a clarification is needed.

Case
~~~~

The very first and maybe most important one rule of the AviSynth grammar is case. AviSynth ignores case:
::

    aViSouRCe 

is just as good as
::

    AVISource

For the AviSynth Grammar both entries correspond to the same token. Thus, you should always have 
in mind that capitalisation does not matter when defining your variables and functions; you must 
always ensure that they are unique in a case-insensitive manner.

Whitespace, Line Continuation and Comments
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first layer of grammar rules concerns the identification of tokens out of the overall script 
text. Text that does not belong to a token is commonly referred to as whitespace in most 
programming and scripting languages.

Whitespace
----------

Whitespace in AviSynth language consists of:

-   Space, tab and newline characters (except inside string literals).
-   The backslash (\) character when it is the first or last non-whitespace character in a line (and not inside a string literal).
-   Comments.
-   Anything from the appearance of the __END__ special keyword up to the end of the script file. 

Backslash
---------

The backslash character serves the role of line continuation. It is used to split a large line 
of code in multiple ones for better readability of the script when editing, yet serve it to the 
AviSynth interpreter as a single logical line of code. Line splitting examples (both valid and equal):
::

    Subtitle("Hello, World!", 100, 200, 0, \
      999999, "Arial", 24, $00FF00)

-or-
::

    Subtitle("Hello, World!", 100, 200, 0,
      \ 999999, "Arial", 24, $00FF00)

Comments
--------

Comments serve the purpose of code documentation. They come in the following flavors:

-   Standard comments: They start with a pound (#) character and extend to the end of the line. 
-   Block comments (AviSynth v2.58 and later): They start either with /* or [* and extend until 
    a (closing) */ or *], respectively, is found downstream the script text. They can span 
    multiple lines and the [* form also supports nested block comments. 

Examples of comments:
::

    AviSource("myclip.avi")    # this is a standard comment

::

    /* this is a block comment 
    we can write a lot here
    SubTitle("Hello, World!")
    and also comment out multiple lines of code
    */

::

    [* this is a nested block comment
    [* 
    a meaningful example will follow later :)
    *]
    for the time being just experiment *]

The comments mechanism has higher precedence than the backslash. If you comment out a line that 
ends with \, line continuation will not happen. A quick example from real life (someone did 
submitted a bug report for this):
::

    ColorBars
    ShowFrameNumber
    Trim(0,9) # select some frames  \
      + Trim(20,29)

The above example does not return frames [0..9,20..29] as the user intended because the "\" is masked 
by the comment start "#" character before it; thus the line continuation never happens. 
The comment should go at the last line.

The __END__ special keyword
---------------------------

The ``__END__`` special keyword can be used to quickly disable some last commands of the script.

Example:
::

    Version()
    __END__
    ReduceBy2()
    Result is not reduced and we can write any text here

Keywords, Identifiers, Literals and Punctuation
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The second layer of grammar rules - once whitespace has been handled and tokens have been 
identified - concerns the categorisation of tokens (that is, finding the type of the tokens). 
Tokens generally belong to one of the following categories:

-   *Keywords*: Tokens with specific, standard meaning for the AviSynth language (ie reserved words).
-   *Identifiers*: Tokens that identify an entity (a variable, a function, etc.).
-   *Literals*: Tokens that represent a value (ie a constant quantity).
-   *Punctuation*: This generic term comprises all tokens with specific, standard meaning 
    for the AviSynth language that are too short to be considered keywords. They include: 

    -   :doc:`Operators <syntax_operators>`.
    -   Grouping and ordering tokens. 

Keywords
--------

The following are AviSynth language's keywords. We use here an all lowercase notation,
 but bear in mind that since AviSynth ignores case, any equivalent combination of 
 uppercase / lowercase letters counts as a keyword (for example: try, Try, tRy, trY, TRy, TrY, tRY, TRY):

-   function : Begins the declaration of a :doc:`user-defined script function <syntax_userdefined_scriptfunctions>` .
-   global : Modifies a variable, such that it has global scope.
-   return : Returns (the result of the expression on the right) from the enclosing
    'script block' - usually a function or the main script, but may also be a try or 
    catch block, an Eval string or an Import file.
-   try : Starts the try part of a try..catch block. See :doc:`Control structures <syntax_control_structures>` for details.
-   catch : Starts the catch part of a try..catch block. See :doc:`Control structures <syntax_control_structures>` for details. 

The following keyword is a special identifier (ie variable):

-   *last* : The special last variable available on any scope for implicit 
     assignment (see below in Expressions and Statements for details). 

The following keywords are special literals (ie constants):

-   *true* : Boolean constant denoting a positive truth value (a true statement).
-   *false* : Boolean constant denoting a negative truth value (a false statement).
-   *yes* : Same as true.
-   *no* : Same as false. 

The following keywords are used only inside arguments lists of function declarations 
to declare the type of arguments:

-   *clip* : The function argument following the keyword is a video clip.
-   *int* : The function argument following the keyword is an integer.
-   *float* : The function argument following the keyword is a floating point number.
-   *string* : The function argument following the keyword is a character string.
-   *bool* : The function argument following the keyword is a boolean (true/false) variable.
-   *func* : The function argument following the keyword is a function object variable. (Avisynth+)
-   *array* : The function argument following the keyword is an :doc:`array <../script_ref/script_ref_arrays>` variable. (Avisynth+)
-   *val* : The function argument following the keyword can be of any type (ie any of the above types). 

Identifiers
-----------

Identifiers, as the term suggests, are specific and unique names that you use in your 
script to refer to distinct entities. In AviSynth language identifiers are used to 
name the following types of entities:

-   :doc:`Variables <syntax_script_variables>` : A variable is a symbolic placeholder for a value
    that can be read and changed (as a result of an assignment) many times during script execution.
-   Functions : A function is a piece of code that performs a specific computation and 
    returns its result to the caller. 

Thus, whenever you need in your script to refer to a variable or function, either built-in
 or user-defined you have to use an identifier. Bear in mind that since AviSynth ignores case, 
 your identifiers should be unique in a case-insensitive manner.

For example, the following is probably an error:
::

    MyClip = AviSource("clip1.avi")
    myclip = AviSource("clip2.avi")    # oops! these two lines assign to the *same* variable

while this is correct:
::

    MyClip = AviSource("clip1.avi")
    YourClip = AviSource("clip2.avi")

Literals
--------

Literals are all the constant (ie specific) values that you use in your scripts. For instance, 
all the tokens that appear at the right side of the assignment operator (the "=" character) 
in the examples below are literals:

::

    a_num = 123
    another_num = 2.456
    
    a_string = "this is a string literal"
    
    another_string = """this is a multiline
           string literal. Note that the 2nd line has leading spaces (which are included)
    while this line has not. Also newlines are included in this type
           of strings"""
    
    a_boolean = true

since Avisynth+ 3.6: 
::

    escaped_string = e"Hello \n"
      with e prefix right before the quotation mark will store actual control character into the string
      Converted literals:
        \n to LF-Chr(10)
        \r to CR-Chr(13)
        \t to TAB-Chr(9)
        \0 to NUL-Chr(0) (NUL is string terminator, use at your own risk)
        \a to Chr(7)-audible beep
        \f to FF-Chr(12) - Form feed
        \\ (double \) to Backslash
        \" to " (double-quotation mark)
        \' to ' (single-quotation mark) (since 3.7.1)
        \b to BS-CHR(8) - backspace (since 3.7.1)
        \v to VT-CHR(11) - vertical tab (since 3.7.1)

As you can see, literals can be of any type (except clips; currently AviSynth does not have clip-type literals).
 The thing that differentiates them from identifiers is that they are not names that hold a value but bare values.

Punctuation
-----------

As said before, this generic term comprises all tokens with specific, standard meaning for the AviSynth 
language that are too short to be considered keywords. The tokens that are bundled under this catch-all category are:

-   operators: Operators apply an operation to one or more entities (and allow to retrieve the result of 
    the operation); this is the reason that they are named that way. 

    In essence operators are mini-functions that are defined in the script grammar with a more 
    user-friendly syntax (for instance, instead of calling ``Add(a, b)`` it is easier to write ``a + b``). 
    Due to their significance in the AviSynth language operators are documented in a :doc:`separate page <syntax_operators>` . 
    They are just listed here for completeness:

    -   Assignment: =
    -   Sign and common math operations: + , - , * , / , % , ++ (the last is for clips only)
    -   Comparisons: ==, != , <> , < , > , <= , >=
    -   Boolean operations: ! , && , ||
    -   Ternary operation (if...else): ?: 

-   Grouping / ordering tokens. These include: 

    -   The comma character [,]: For separating arguments in function argument lists only.
    -   The dot character [.]: When successive calls to functions are chained together with 
        the use of the OOP notation.
    -   The parenthesis, opening and closing [()]: For grouping expressions into a single unit. 
        Also for grouping arguments of a function declaration or call.
    -   The (curly) brackets [{}]: For grouping multiple statements in a single block of code
        (currently: function bodies and ``try...catch`` blocks only). 

Expressions and Statements
~~~~~~~~~~~~~~~~~~~~~~~~~~

The third layer of grammar rules - after whitespace has been handled and tokens have been identified 
and distributed to the available categories (keywords, identifiers, etc.) - concerns the grouping of 
tokens in higher-level structures of the grammar: expressions and statements. A little terminology 
is necessary at this point to clarify the difference between them.

-   Expressions are groupings of tokens that perform a computation and return a value. They form a 
    distinct part of either a larger enclosing expression or a statement.
-   Statements are the smallest standalone element of an AviSynth script; in other words a statement 
    is a single unit of script code (in the case of AviSynth language, this is typically a line of script code). 

Having made this distinction, lets see each one in more detail at the sections that follow.

Expressions
-----------

Expressions are the first step in the creation of the higher-level grammar constructs. 
They combine tokens in order to compute a new value from old ones and deliver this new value 
to either a surrounding expression or directly to an even higher-level construct, ie a statement.

A few examples will help to fully understand the concepts presented above:
::

    # 10 is a literal; 
    # it is also an expression; a grouping can have just 1 element
    a = 10
    
    # a + 7 is an expression; so is (a + 7) / 5
    b = (a + 7) / 5
    
    # b > 0, 12, 25 are expressions (see 1st line); 
    # [b > 0 ? 12 : 25] is also an expression
    c = b > 0 ? 12 : 25
    
    # BlankClip(...) below is an expression; 
    # so is Trim(...)
    Trim(BlankClip(width=b, height=c, pixel_type="RGB32"), 0, a)
    
    # all the above lines of code are statements
    
Most of the time the result of an expression will be a video clip; however an expression's result 
can be of any type supported by the scripting language (clip, int, float, bool, string) and this 
is how utility functions such as internal script functions operate.

Combining all information presented above, we can now see that an AviSynth expression typically has 
one of these forms (with square brackets, ([]), we enclose optional elements, with the vertical bar 
character, (|), we separate alternatives, with the pound character, (#), we enclose comments):

-    Literal, ie: 

    ::

        numeric_constant
        | string_constant
        | bool_constant

The value of the expression is the value of the constant.

-   Identifier, ie: 

    ::

        variable_name 
        | clip_property
        | function_name                                  # without (args) #

The value of the expression is the value returned by clip properties or contained inside script 
variables (which must have been previously initialized).

-   Expression, ie: 

    ::

        [ + | - | ! ] expression                         # unary operator expression #
        | ( expression )                                 # expression inside parentheses #
        | expression-1 operator expression-2             # binary operator expression #
        | bool_expression ? expression-1 : expression-2  # the ternary operator #
        | function_name[ ( args ) ]                      # function call #
        | expression.function_name[ ( args ) ]           # OOP notation #

The value of the expression is either the result of the computation of the sub-expressions or 
the return value of the function_name call.

Looking a bit closer at the possible expression alternatives, the following notes can be made:

-   The first three cases show that one can manipulate expressions using all of the usual 
    arithmetic and logical operators (from C) as you'd expect on ints, floats, vals, and bools. 

    -   Strings can be concatenated with '+'.
    -   The following operators are also defined on video clips: 

    ::

        a + b   
        # is equivalent to:
        UnalignedSplice(a, b) 

    and: 
    ::

        a ++ b
        # is equivalent to:
        AlignedSplice(a, b)

-   The fourth case shows that one can execute code conditionally with the ternary operator.
-   The fifth case shows that a function call is, from the grammar's perspective, a special type of expression.
-   The sixth case shows OOP notation, an alternate syntax for chaining function calls, which is equivalent to: 
    ::

        function_name(expression, args)

Statements
----------

Statements are the smallest standalone element of an AviSynth script/ Statements do not compute a value; 
they are evaluated for their side effects (which are most of the time the assignment of a value 
computed by an expression to a :doc:`variable <syntax_script_variables>`).

Statements are grouped together to form a script. An AviSynth script is simply the aggregate of a number of statements.

All statements in AviSynth scripting language have one of these forms (with square brackets, ([]), 
we enclose optional elements, with the vertical bar character, (|), we separate alternatives, 
with the pound character, (#), we enclose comments):

::

    [ global ] variable_identifier = expression
    | [ return ] expression
    | try_catch_block
    | function_declaration

For each specific type of statement, the following notes can be made:

-   In the first case, *expression* is evaluated and the result is assigned to an identifier.
    The identifier can only identify a variable, either local or global (if the optional global 
    keyword is present). That is you can only assign to variables. Hence the name *variable_identifier*. 

-   In the second case, *expression* is evaluated and the result is used as follows: 

    -   If the return keyword is present or the statement is the last in its script block, it is used 
        as the "return value" of the active script block - that is, either a function or the entire 
        script. In the latter case, the return value is typically the video clip that will be seen by 
        the application which opens the AVS file.
    -   Otherwise, if the result is a clip, it is assigned to the special variable ``last``. If the result 
        is not a clip, it is simply discarded. 

The last two cases are the only compound statements supported by AviSynth script language.
They are presented in detail in the section that follows.

Compound Statements
-------------------

A compound statement is a block of statements that is considered a single unit of code (ie statement). 
Thus a compound statement is a multiline statement. As we saw, AviSynth supports two types of compound 
statements: the *try_catch_block* and *function_declaration*.

-   The *try_catch_block* statement has the following form: 
    ::

        try {                         # the try part is always executed #
          [ statement                 # you can put as many statements as you want #
            ...
            statement ]               # an empty block is allowed (but not very useful!) #
        }
        catch (variable_identifier) { # catch part is executed only if an error occurs in try part #
          [ statement                 # you can put as many statements as you want #
            ... 
            statement ]               # an empty block is allowed and causes the error to be ignored #
        }

    It implements the try..catch :doc:`control structure <syntax_control_structures>`.
    See there for details. 

-   The *function_declaration* statement has the following form: 
    ::

        function identifier( [ argument_list ] )
        /* from v2.60 you can also put comments here */
        {
          [ statement                 # you can put as many statements as you want #
            ...
            statement ]               # an empty function is allowed (but not very useful!) #
        }

    It declares a user-defined function and makes it available for calling to the rest of script code, 
    by using the identifier as the name of the function to be called. 

    The optional argument_list (yes, you can have functions without arguments) declares the type 
    and name of function's arguments, as well as whether they are required or are optional. 
    Optional arguments are also called named arguments, because you can supply them by name 
    in a function call. It has the following form: 

    ::

        argument-1 , ... , argument-K , optional_argument-K+1 , ... , optional_argument-N

        ``argument-i`` (i = 1 to K) and ``optional_argument-j`` (j = K + 1 to N) have the following 
        forms (again, with square brackets, ([]), we enclose optional elements, with the vertical bar 
        character, (|), we separate alternatives, with the pound character, (#), 
        we enclose comments), respectively: 

    ::

        [ type_keyword ] identifier     # (normal) argument
        [ type_keyword ] "identifier"   # optional argument    

    As you can see, *optional arguments* distinguish from (normal) arguments in that they *are enclosed 
    in double quotation marks*. In a function call you can refer to an optional argument as: 
    ``identifier = value``. You can also refer to in the normal way as if it was a normal, positional argument.

Three more things to note are the following:

-   Once you declare an optional argument, all subsequent arguments must also be declared optional.
-   If you don't supply the type of the argument in the declaration (ie one of the type keywords 
    presented above), the argument is of the val type. That is it can be of any type. Consequently 
    in the body of the function you have to query for its type, if you want your code to be robust.
-   Function declarations can be written in any order and at any point in the script where a statement 
    is allowed, independently of where the functions themselves are called. The presence of the 
    declaration itself does not interfere with the order of script execution or its result. 
    However, the usual convention is to group functions together at the start of the script. 

A few examples will help to clarify things:
::

    function MyFunc1() {                 # a function with no arguments
        ...
    }

::

    function MyFunc2(clip c, int n) {    # a function with two (normal) arguments
        ...
    }

::

    function MyFunc3(clip c, string "text", bool "invert") {
        ...                              # a function with one argument and two optional arguments
    }                                    # if they are not supplied, it uses some default values

::

    function MyFunc4(clip "c", bool "invert, int "n") {
        ...                              # you can declare a function with all arguments optional
    }

::

    function MyFunc5(clip clp, effect, "text") {
        ...                              # a function with two normal and one optional argument
    }                                    # the last two arguments are of val (ie any) type
    ...
    f = MyFunc1()
    g = MyFunc2(ColorBars(), 6)      # all normal arguments *must* be supplied
    ...
    h = MyFunc3(g, "some text", false)   # you can supply optional arguments as if they were normal
    i = MyFunc3(g)                       # but you can omit them also entirely
    j = MyFunc3(g, invert=true)          # or you can pass some of them by name
    ...
    k = MyFunc4()                        # MyFunc4 will use defaults for all its arguments
    l = MyFunc4(g, n=12)                 # you can supply some optional arguments as positional
    ...                                  # and some by name
    ...
    m = MyFunc5(g, 25, "test")           # you can pass any type in the last two arguments of MyFunc5
    n = MyFunc5(g, "dissolve", text=m)   # this can be both flexible *AND* dangerous if you don't check
    o = MyFunc5(g, g)                    # the type of the arguments; you can of course omit optional ones


Closing Remarks
~~~~~~~~~~~~~~~

The set of rules for identifying and grouping tokens into higher-level structures (ie the AviSynth Grammar) 
ends with statements. An AviSynth script is simply the aggregate of a number of statements. 
In it you place as many statements as required to do the job. 
The grammar does not care how you do so. However, there are a couple of things that are worth noting here 
to make developing scripts easier:

-   The return value of the entire script is either (cf. the second case of Statements section above): 

    -   The result of a return expression statement anywhere in the main script block (ie not in a 
        function body or inside a try...catch block); all statements below that one will be ignored. 
        As a shorthand, a bare expression as the final statement is treated as if the keyword return 
        was present.
    -   If there is no (explicit or implicit) return, a void value (ie a value of the 'undefined' type) 
        is returned. For example, this will happen if the last statement is an assignment. 

-    AviSynth provides a mechanism to include other scripts inside the current script block: the 
     Import function. The result of calling Import is the same as if you have typed the entire imported 
     script text at the point of the function call. 

-    Making self-contained scripts and using Import to include them in you scripts is a way to organise 
     and reuse your code (for example, your favorite used-defined functions). 

The Full Avisynth Grammar - For Language Lawyers
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For those readers that prefer a formal definition of the AviSynth script language's grammar, 
there is one available (though not "officially-endorsed" at the moment) in 
:doc:`Extended Backus-Naur form <syntax_formal_avisynth_grammar>` (or EBNF for short). 

Back to :doc:`Avisynth Syntax sections<syntax_sections>`.

$Date: 2024/01/09 10:00:00 $
