README_LIB.txt

When building the application, it's important that the appropriate version
of the library be used.  Specifically, if the application is being compiled
as DEBUG, then the DEBUG version of the library must be used.  If the user
application is compiled as RELEASE, the RELEASE version of the library must
be used.  Not following this may result in unexpected crashes.

The default names for the static Zip libraries (under Windows) are:

  zip32_libd.lib      - Debug static library

  zip32_lib.lib       - Release static library

See README_VC10_EXAMPLES.txt for more on using the DLL and LIB.

23 August 2015
