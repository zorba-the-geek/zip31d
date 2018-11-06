README_VC10_EXAMPLES.txt

VC10 LIB and DLL Examples Readme

This directory has VS 2010 C example projects for showing how to use the
Zip 3.1 LIB and DLL libraries.

c_dll_ex contains the DLL example.  zip32_dll.lib must be in the project
directory (look for the file "zip32_dll.lib here.txt" for where it goes) to
compile the example.  This sets up the references in the example executable.

Once the example is compiled, zip32_dll.dll must be placed in the project
directory or in the command PATH.

c_lib_ex contains the LIB example.  To compile the example, you must place
the compiled library (zip32_lib.lib or zip32_libd.lib) as well as the bzip2
library (libbz2.lib) in that directory.  (Look for "zip32_lib.lib here.txt",
"zip32_libd.lib here.txt", and "libbz2.lib here.txt" for where these go.)
Also see the file README_LIB.txt in that directory for additional important
information regarding the LIB.

The source files for both examples are in this directory.  Be sure to read
the comments in each example.

The libraries are built using the projects in windll/vc10.  See the file
README_VC10.txt in that directory.

31 August 2015
