windll/Readme_DLL_LIB.txt

On Windows open this file in WordPad, gEdit, or similar.

This windll directory contains projects for building and using the
Zip 3.1 static and dynamic libraries (LIB and DLL).

See the Readme and other files in the various subdirectories for more
information on each library port and on the examples provided showing
use of the LIB and DLL.

See WhatsNew_DLL_LIB.txt and the comments in the api.h and api.c source files
for details on the updated LIB/DLL interfaces.

The Zip 3.1 libraries are not backward compatible with either the Zip 2.32 or
the Zip 3.0 libraries.  They are also not compatible with the libraries or
examples provided with Zip 3.1 betas.  This was necessary to finally clean up
the interface to make it more consistent and intuitive (hopefully) and to add
in hooks for later expansion.  These libraries also have an updated version
reporting feature that can be used to verify compatibility and confirm
supported features at run time.  See the example code in the windll/examples
directories for how to use these new features.

The four libraries built using the zip32_lib and zip32_dll projects in
the windll/vc10 directory are:

Static linking:

  zip32_lib.lib    - This is the static library.

  zip32_libd.lib   - This is zip32_lib.lib with debugging included.

    Be sure to use the DEBUG version with a DEBUG app and the RELEASE
    version with a RELEASE app.

Dynamic linking:

  zip32_dll.lib    - This is a static library that calls the DLL.  Use
                     this one to create the DLL references in your
                     DLL project.

  zip32_dll.dll    - This is the dynamic library.  When you link to
                     zip32_dll.lib at link time, zip32_dll.dll needs to
                     be in the project directory or in a directory
                     listed in PATH.  You can also load this dynamically
                     without using zip32_dll.lib to reference it.  The
                     C examples link to zip32_dll.lib while the Visual
                     Basic examples do not use it.

Currently the bzip2 library is compiled separately.  This library (libbz2.lib)
is also needed to build the LIB examples if bzip2 support is included (which
is the default).

It should be simple to convert a program using the old libraries to the
Zip 3.1 libraries.  For a zip32.dll compatible replacement (the old Zip 2.3x
DLL) use the dll and lib compiled from Zip 2.32 (released separately).

Note that the files may be saved in Unix format with carriage returns
stripped.  These must be restored before the project can be successfully
used with older versions of Visual Studio.  Visual Studio 2010 seems OK
with these line endings.

Enjoy!

13 September 2015

