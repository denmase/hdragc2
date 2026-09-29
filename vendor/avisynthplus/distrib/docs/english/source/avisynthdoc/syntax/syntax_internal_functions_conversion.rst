
AviSynth Syntax - Type conversion functions
===========================================

Conversion functions convert between different types. There are also some
:doc:`numeric functions <syntax_internal_functions_numeric>` that can be classified in this 
category, namely: ``Ceil``, ``Floor``, ``Float``, ``Int``, ``IntI``, ``Long``, ``FloatF``,
``Double`` and ``Round``.

See :doc:`AviSynth Syntax - Numeric functions <syntax_internal_functions_numeric>`.

Value
~~~~~
::

    Value(string)

Converts a decimal string to its associated numeric value.

*Examples:*
::

    Value ("-2.7") = -2.7


HexValue
~~~~~~~~
HexValue64
~~~~~~~~~~
::

    HexValue(string[, int "pos"])
    HexValue64(string[, int "pos"])

Converts a hexadecimal string to its associated numeric value.

HexValue returns 32 bit integer.

HexValue64 returns 64 bit long. (since v3.7.4)

Conversion will cease at the first non legal number base digit, without 
producing an error.

Function returns 0 if ``pos`` is erroenus: ie less than 1 or greater than string length.

.. describe:: int pos (default 1)

    Sets the starting point of the numeric parser. All characters to the 
    left of pos are ignored. Default is 1 (start of string).

*Examples:*
::

    HexValue ("FF00") # 65280
    HexValue ("FFA0", 3) # 160
    HexValue ("FFFFFFFF") # -1
    HexValue64 ("FF00") # 65280
    HexValue64 ("FFFFFFFF") # 4294967295


Hex
~~~
::

    Hex(int [, int "width"])

Converts a numerical value to its hexadecimal value string. See `Colors`_ for
more information on specifying colors.

Function returns hex string in uppercase, instead of lowercase.

.. describe:: int width (default 0)

    Width is 0 to 16 the *minimum* width of the returned string. (8 hex digit is the
    max of Avisynth 32 bit integer, 16 digits for 64 bit numbers)

    Resulting string will be left-padded with zeroes as needed.

    When width is 0 (default) or not supplied then string length is a minimum needed.

*Examples:*
::

    Hex(10824234) # "A52A2A"
    Hex(255,4) # returns "00FF".

String
~~~~~~
::

    String(float / int [, string format_string])

Converts a variable to a string. 

- String arguments are passed along unchanged; 
- bools are converted to "true" or "false"; 
- numbers are formatted as described below; 
- other variable types (clip, val) are converted to the empty string. 

If the variable is float or integer and ``format_string`` exists, it
first converts it to a float and then uses format_string to convert the float
to a string. The syntax of ``format_string`` is as follows:

- ``%[flags][width][.precision]f``

  the leading ``'%'`` and trailing ``'f'`` are required!

  - *width*: the minimum width (the string is never truncated if it is wider than width)
  - *precision*: the number of digits printed
  - *flags*:

    - ``-`` left align (instead right align)
    - ``+`` always print the +/- sign (show only '+' by default)
    - ``0`` pad (see width) with leading zeroes (pad with spaces by default)  
    - ``' '`` print a blank instead of a "+"
    - ``#`` always print the decimal point (dropped by default if there are no decimal digits)

You can also put arbitrary text around the format_string as defined above, similar to the C-language *printf* function.

*Examples:*
::

    Subtitle( String(1.23) )                    # '1.230000' (six decimals by default for floats)
    Subtitle( String(123) )                     # '123'      (no decimals by default for ints)
    
    Subtitle( String(1.23, "%0.2f" ))           # '1.23'
    Subtitle( String(1.23, "%0.1f" ))           # '1.2'
    Subtitle( String(1.23, "%5.1f") )           # '  1.2'    (padded to 5 characters wide)
    Subtitle( String(1.23, "%1.3f") )           # '1.230'    (3 decimals; add trailing zeroes)
    
    Subtitle( String(123, "%0.0f") )            # '123'      (no decimals for precision=0)
    Subtitle( String(123, "%#0.0f") )           # '123.'     ('#' flag: always show decimal point)
    Subtitle( String(123, "%0.2f") )            # '123.00'   (2 decimals: add trailing zeroes)
    Subtitle( String(123, "%5.0f") )            # '  123'    (padded to 5 characters wide using ' ')
    Subtitle( String(123, "%05.0f") )           # '00123'    (padded to 5 characters wide using '0')
    
    Subtitle( String(PI, "PI=%0.0f") )          # 'PI=3'     (text around format_string)
    Subtitle( String(PI, "PI=%#0.0f") )         # 'PI=3.'    ('#' flag: always show decimal point)
    Subtitle( String(PI, "PI=%2.0f") )          # 'PI= 3'
    Subtitle( String(PI, "PI=%3.2f") )          # 'PI=3.14'
    Subtitle( String(PI, "PI=%0.5f") )          # 'PI=3.14159'
    Subtitle( String(PI, "PI=%6.3f") )          # 'PI= 3.142'
    
    Subtitle( String(32, "%0.0f") )             # '32'
    Subtitle( String(32, "%3.0f") )             # ' 32'
    Subtitle( String(32, "%8.0f") )             # '      32'

::

    ## arbitrary text around format_string:
    Subtitle( String(Last.Height, "Clip height is %0.0f") ) # 'Clip height is 480'
    ## same output as above but using string concatenation:
    Subtitle( "Clip height is " + String(Last.Height) )
    
    Subtitle( String(x, "Value of x is %.3f after AR calc") )
    Subtitle( "Value of x is " + String(x, "%.3f") + " after AR calc") )
    # same as above


Changelog
---------
+-----------------+----------------------------------+
| Version         | Changes                          |
+=================+==================================+
| 3.7.4           | | Add HexValue64                 |
|                 | | Add IntI, Long, FloatF, Double |
+-----------------+----------------------------------+
| Avisynth+ r2632 | | Hex: added "width"             |
|                 | | Hexvalue: added "pos"          |
+-----------------+----------------------------------+


--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2025/03/06 16:00:00 $

.. _Colors: http://avisynth.nl/index.php/Colors
