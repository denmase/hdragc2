
AviSynth Syntax - Version functions
===================================

Version functions provide AviSynth and OS version or bitness information.

VersionNumber
~~~~~~~~~~~~~
::

    VersionNumber()

Returns AviSynth version number as a float.

Note: use VersionString below to determine if running in AviSynth+. 

*Examples:*
::

    ver = VersionNumber() # ver == 2.57
    VersionNumber ## returns 2.60 (release version as of 11/2017)
    VersionNumber ## returns 2.61 (beta)
    VersionNumber ## returns 2.60 (AVS+ releases)

VersionString
~~~~~~~~~~~~~
::

    VersionString()

Returns AviSynth version info as a string (first line used in :doc:`Version <../corefilters/version>`
command).

*Examples:*
::

    VersionString ## returns "AviSynth 2.60, build:Mar 31 2015 [16:38:54]"
    VersionString ## returns "AviSynth 2.61, build:May 17 2016 [16:06:18] VC2008Exp"
    VersionString ## returns "AviSynth+ 0.1, (r2508, MT, x86_64)"
    VersionString ## returns "AviSynth+ 3.7.3 (r4041, master, x86_64)"

Sometimes we have distinct between Avisynth+ or Avisynth classic 2.6
::

    function IsAvsPlus()
    {
        sVer = LCase(VersionString) 
        return (FindStr(sVer, "avisynth+")    > 0)
        \   || (FindStr(sVer, "avisynthplus") > 0)
    }

IsVersionOrGreater
~~~~~~~~~~~~~~~~~~
::

    bool IsVersionOrGreater(int majorVersion, int minorVersion [,int bugfixVersion])
    
    Returns true if Avisynth+ version is is equal or greater than the required one in the parameters. 
    Since Avisynth+ 3.5 

*Examples:*

Check if script is run under at least...
::

    isAtLeast3_10 = IsVersionOrGreater(3,10)
    isAtLeast3_5_1 = IsVersionOrGreater(3,5,1)

    if(IsVersionOrGreater(3,7))
    {  SubTitle("at least 3.7") 
    }
    if(IsVersionOrGreater(3,7,3))
    {  SubTitle("at least 3.7.3") 
    }


GetProcessInfo
~~~~~~~~~~~~~~
::

    GetProcessInfo(int)

Returns information about the process the script is running in. 

.. describe:: int (required)

    valid values are 0 (default) and 1.

    - 0 (default):

      - returns 64 for a 64-bit process 
      - returns 32 for a 32-bit process.

    - 1

      - returns 0 for 32-bit process on 32-bit OS;
      - returns 1 for 32-bit process on 64-bit OS;
      - returns 2 for 64-bit process on 64-bit OS;
      - else returns -1 (unknown) 

*Examples:*

Always loads the appropriate 32/64 bit plugin version:
::

    IF (GetProcessInfo() == 32) {
    LoadVirtualdubPlugin("c:\virtualdub\plugins32\Deshaker.vdf","deshaker")
    } else {
    LoadVirtualdubPlugin("c:\virtualdub\plugins64\Deshaker_64.vdf","deshaker")
    }
    .. source filter ..
    ConvertBits(8)
    ConvertToRGB32()
    Deshaker("19|1|30|4|1|0|1|0|640|480|1|2|1000|1000|1000|1000|4|1|0|2|8|30|300|4|C:\\temp\\Deshaker.log|0|0|0|0|0|0|0|0|0|0|0|0|0|1|15|15|5|15|0|0|30|30|0|0|0|0|1|1|0|10|1000|1|88|1|1|20|5000|100|20|1|0|ff00ff")

With parameters:
::

    SubTitle("Process bits=" + string(GetProcessInfo) + \
      " detailed=" + string(GetProcessInfo(1)) + \
      " type0=" + string(GetProcessInfo(0)))
    # returns process bits=64 detailed=2 type0=64
    # running a 64 bit Avisynth on 64 bit OS


+----------------+----------------------------------+
| Version        | Changes                          |
+================+==================================+
| AviSynth+      | | IsVersionOrGreater             |
|                | | GetProcessInfo                 |
+----------------+----------------------------------+


--------

Back to :doc:`Internal functions <syntax_internal_functions>`.

$Date: 2008/04/20 19:07:34 $
