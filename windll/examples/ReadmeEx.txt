WinDLL Examples Readme

On Windows open in WordPad, GEdit, or similar to adjust line ends.

This directory has projects for example programs showing how to use
Win32 versions of Zip LIB and DLL.

vc10 - Visual Studio 2010 C examples for using the LIB and the DLL.
       These are simple command line examples that don't exercise
       all callbacks.  Tested on Windows 7.

vc6  - Visual Studio 6 C examples for using the LIB and DLL.  These
       are simple command line examples.  Tested on Windows XP.

vb10 - Visual Studio 2010 Visual Basic example showing use of DLL
       in a graphical application.  Includes demonstration of all
       the various callbacks available in the new Zip interface.
       Tested on Windows 7.

vb6  - Visual Studio 6 Visual Basic example for the DLL.  This is
       an updated version of the Zip 3.0 example and shows how to
       use many of the new callbacks in a graphical application.
       Tested on Windows XP.

Updated C# examples are planned.

The C# examples in work and the "old" examples have been moved to
the embedded archive "notready.zip".  This should all be cleaned
up by release.

There were significant changes in the Microsoft library interface
between Windows XP and Windows 7, with new capabilities added and
some key capabilities removed.  The latter makes some of the Visual
Studio 6 examples (in particular the vb6 example) unworkable on
Windows 7.  Hence the need for different interfacing approaches for
Windows XP (Visual Studio 6) and Windows 7 (Visual Studio 2010).

Note that there is also a Unix LIB example, izzip_example.c, which
is in the top directory of the source tree.  See the comments in
that file for how to build and use it.

These are more or less minimum examples showing the basics.  They
probably need some work before use in any real situation.

See ReadUse_DLL_LIB.txt for more on using the libraries.

See WhatsNew_DLL_LIB.txt for a description of the updated LIB/DLL interface.

See the Readme files in each project directory for details of each
example.

23 April 2015
