
AviSynth Syntax - Trigonometry functions
========================================

Relationships involving lengths and angles of triangles. 


Sin
~~~
::

    Sin(float)

Returns the sine of the argument (assumes it is radians).

*Examples:*
::

    Sin(Pi()/4) = 0.707
    Sin(Pi()/2) = 1.0

Cos
~~~
::

    Cos(float)

Returns the cosine of the argument (assumes it is radians).

*Examples:*
::

    Cos(Pi()/4) = 0.707
    Cos(Pi()/2) = 0.0

Tan
~~~
::

    Tan(float)

Returns the tangent of the argument (assumes it is radians).

*Examples:*
::

    Tan(Pi()/4) = 1.0
    Tan(Pi()/2) = not defined

32 bit IEEE floats do not have sufficient resolution to exactly represent 
pi/2 so AviSynth returns a large positive number for the value slightly less 
than pi/2 and a large negative value for the next possible value which is 
slightly greater than pi/2. 

Asin
~~~~
::

    Asin(float)

Returns the inverse of the sine of the argument (output is radians).

*Examples:*
::

    Asin(0.707) = 0.7852471634 (~ Pi/4)
    Asin(1.0) = 1.570796327 (~ Pi/2)

Acos
~~~~
::

    Acos(float)

Returns the inverse of the cosine of the argument (output is in radians).

*Examples:*
::

    Acos(0.707) = 0.7852471634 (~ Pi/4)
    Acos(0.0) = 1.570796327 (~ Pi/2)

Atan
~~~~
::

    Atan(float)

Returns the inverse of the tangent of the argument (output is in radians).

*Examples:*
::

    Atan(0.707) = 0.6154085176
    Atan(1.0) = 0.7853981634 (~ Pi/4)

Atan2
~~~~~
::

    Atan2(float, float)

Returns the angle between the positive x-axis of a plane and the point given
by the coordinates (x, y) on it (output is in radians). See `the wikipedia article on Atan2`_ for
more information. y is the first argument and x is the second argument.

*Examples:*
::

    Atan2(1.0, 0) = 1.570796327 (~ Pi/2)
    Atan2(1.0, 1.0) = 0.7852471634 (~ Pi/4)
    Atan2(-1.0, -1.0) = -2.356194490 (~ -3Pi/4)


Sinh
~~~~
::

    Sinh(float)

Returns the hyperbolic sine of the argument. See `wikipedia`_ for more
information.

*Examples:*
::

    Sinh(2.0) = 3.626860408

Cosh
~~~~
::

    Cosh(float)

Returns the hyperbolic cosine of the argument.

*Examples:*
::

    Cosh(2.0) = 3.762195691

Tanh
~~~~
::

    Tanh(float)

Returns the hyperbolic tangent of the argument.

*Examples:*
::

    Tanh(2.0) = 0.9640275801

--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2024/01/15 16:16:00 $

.. _the wikipedia article on Atan2: http://en.wikipedia.org/wiki/Atan2
.. _wikipedia: http://en.wikipedia.org/wiki/Hyperbolic_function
