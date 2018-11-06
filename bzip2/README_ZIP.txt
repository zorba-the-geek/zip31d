BZIP2 SUPPORT IN ZIP

bzip2 is now included in the Zip source kit.  The builders should
now include bzip2 compression (as well as LZMA and PPMd compression)
by default, though it is possible to leave a method out.  For example,
to prevent inclusion of bzip2 in Zip, either set macro NO_BZIP2_SUPPORT
or just delete the contents of the bzip2 directory in the source kit
(or rename that directory to something other than bzip2).

Zip has been tested with bzip2 library 1.0.6 and earlier.

This kit has been modified to work with VMS, as well as Unix, Windows,
and some other ports.  See http://antinode.info/dec/sw/bzip2.html for
more on VMS compatibility.

See INSTALL for details regarding installing Zip.  The -Z option can
be used to select bzip2 compression.


Notes

Since Zip 3.0, the license for bzip2 has changed to one more compatible
with the Info-ZIP license, so as of Zip 3.1 bzip2 is included in the Zip
source kit and this compression method is included by default on ports
(such as Unix) that support it.

Compression method bzip2 requires a modern unzip.  Before using bzip2
compression in Zip, verify that a modern UnZip program with bzip2 support
will be used to read the resulting zip archive so that entries compressed
with bzip2 (compression method 12) can be read.  Older unzips probably
won't recognize the compression method and will skip those entries.

Zip using bzip2 compression is not compatible with the bzip2 application,
but instead provides an additional way to compress files before adding
them to a Zip archive.  It does not replace the bzip2 program itself,
which creates bzip2 archives in a different format that are not
compatible with zip or unzip.
 
The bzip2 code and algorithms are provided under the bzip2 license
(provided in the bzip2 source kit, now included in the bzip2 directory)
and what is not covered by that license is covered under the Info-ZIP
license.  Info-ZIP will look at issues involving the use of bzip2
compression in Zip, but any questions about the bzip2 code and
algorithms or bzip2 licensing, for example, should be directed to the
bzip2 maintainer.

-----------------------------------------------------------------------
The rest of this README discusses installation details and can be
skipped in most cases.

The standard Zip build uses the bzip2 source provided in the kit.  Use
of external bzip2 source or linking to an external bzip2 library is now
considered non-standard, as the external code or library may not be
compatible with Zip.  Use of external bzip2 code or library is done at
user's risk.
-----------------------------------------------------------------------

Installation

  NOTE:  bzip2 is now included by default.  The below is still provided
  for informational purposes only (for instance, in case you want to
  update the library used).  See INSTALL for how to install Zip,
  including with bzip2.  No external files are needed for the standard
  build.

To build Zip with bzip2 support, Zip generally needs one bzip2 header
file, "bzlib.h", and the object library, typically "libbz2.a", except
in cases where the source files are compiled in directly.  If you
are either compiling the bzip2 library or compiling in the bzip2
source files, we recommend defining the C macro BZ_NO_STDIO, which
excludes a lot of standalone error code (not used when bzip2 is
used as a library and makes the library smaller) and provides hooks
that Zip can use to provide better error handling.  However, a
standard bzip2 object library will work, though any errors that bzip2
generates may be more cryptic.

Building the bzip2 library from the bzip2 source files (recommended):

  Download the latest bzip2 package (from "http://www.bzip.org/", for
  example).  (The bzip2 directory in the source kit has the latest
  bzip2 kit tested with Zip.)
     
  Unpack the bzip2 source kit (bzip2-1.0.6.tar.gz was current as of
  this writing, but the latest should work).

  Read the README file in the bzip2 source kit.

  Compile the bzip2 library for your OS, preferably defining
  BZ_NO_STDIO.  Note: On UNIX systems, this may be done automatically
  when building Zip, as explained below.


Installation on UNIX (see below for installation on other systems):

  Note:  Zip on UNIX uses the "bzlib.h" include file and the compiled
  "libbz2.a" library to static link to bzip2.  Currently we do not
  support using the shared library (patches welcome).

  The easiest approach may be to drop the two above files in the
  bzip2 directory of the Zip source tree and build Zip using the
  "generic" target, that is, using a command like
    make -f unix/Makefile generic
  If all goes well, make should confirm that it found the files and
  will be compiling in bzip2 by setting the BZIP2_SUPPORT flag and
  then including the libraries while compiling and linking Zip.

  To use bzlib.h and libbz2.a from somewhere else on your system,
  define the "make" macro IZ_BZIP2 to point to that directory.  For
  example:
    make -f unix/Makefile generic IZ_BZIP2=/mybz2
  where /mybz2 might be "/usr/local/src/bzip2/bzip2-1.0.5" on some
  systems.  Only a compiled bzip2 library can be pointed to using
  IZ_BZIP2 and Zip will not compile bzip2 source in other than the
  bzip2 directory.

  If IZ_BZIP2 is not defined, Zip will look for the bzip2 files in
  the "bzip2" directory in the Zip source directory.  (This is the
  default, and will include the bzip2 source provided with the Zip
  source kit.)  To use other than the standard bzip2 source, either
  remove the bzip2 files from this directory or rename the directory
  and create a new bzip2 directory.  Then either drop bzlib.h and
  libbz2.a in it to use a compiled library as noted above or drop
  the contents of the bzip2 source kit in this directory so that
  bzlib.h is directly in the bzip2 directory and Zip will try to
  compile it if no compiled library is already there.


  Unpacking bzip2 so Zip compiles it:

  To make this work, the bzip2 source kit must be unpacked directly
  into the Zip "bzip2" directory.  For example:

      # Unpack the Zip source kit.
      gzip -cd zip30.tar-gz | tar xfo -
      # Move down to the Zip kit's "bzip2" directory, ...
      cd zip30/bzip2
      # ... and unpack the bzip2 source kit there.
      gzip -cd ../../bzip2-1.0.5.tar.gz | tar xfo -
      # Move the bzip2 source files up to the Zip kit's bzip2 directory.
      cd bzip2-1.0.5
      mv * ..
      # Return to the Zip source kit directory, ready to build.
      cd ../..
      # Build Zip.
      make -f unix/Makefile generic


  Using a system bzip2 library:

  If IZ_BZIP2 is not defined and both a compiled library and the bzip2
  source files are missing from the Zip bzip2 directory, Zip will test
  to see if bzip2 is globally defined on the system in the default
  include and library paths and, if found, link in the system bzip2
  library.  This is automatic.

  However, given the actual library there may not be completely
  compatible with Zip, using a system bzip2 library is not preferred.


  Preventing inclusion of bzip2:

  To build Zip with _no_ bzip2 support on a system where the automatic
  bzip2 detection scheme will find bzip2, you can specify a bad
  IZ_BZIP2 directory.  For example:

      make -f unix/Makefile generic IZ_BZIP2=no_such_directory

  You can also define NO_BZIP2_SUPPORT to exclude bzip2.


  Verifying bzip2 support in Zip:

  When the Zip build is complete, verify that bzip2 support has been
  enabled by checking the feature list:

      ./zip -v

  If all went well, bzip2 (and its library version) should be listed.


Installation on other systems

  MSDOS:

  Thanks to Robert Riebisch, the DJGPP 2.x Zip port now supports bzip2.
  To include bzip2, first install bzip2.  The new msdos/makebz2.dj2
  makefile then looks in the standard bzip2 installation directories
  for the needed files.  As he says:
    It doesn't try to be clever about finding libbz2.a. It just
    expects bzip2 stuff installed to the default include and library
    folders, e.g., "C:\DJGPP\include" and "C:\DJGPP\lib" on DOS.

  Given a standard DJGPP 2.x installation, this should create a
  version of Zip 3.1 with bzip2 support.

  The bzip2 library for DJGPP can be found on any DJGPP mirror in
  "current/v2apps" (or "beta/v2apps/" for the latest beta). This
  library has been ported to MSDOS/DJGPP by Juan Manuel Guerrero.


  WIN32 using Visual Studio 2010 (Windows 7/Vista):

  The projects for later Windows builds now build the bzip2 library
  and then link to it.  Use of bzip2 should be automatic.  Add the
  preprocessor macro NO_BZIP2_SUPPORT to exclude bzip2.


  WIN32 using VS 6 (Windows NT/2K/XP/2K3/... and Windows 95/98/ME):

  For Windows XP and earier there seems to be two approaches, either
  use bzip2 as a dynamic link library or compile the bzip2 source in
  directly.  I have not gotten the static library libbz2.lib to work,
  but that may be me.  As the below methods work, no additional effort
  has been made to link the static library in on these older Windows
  ports.  The default is to compile in the bzip2 files directly.

  Using bzip2 as a dynamic link library:

    Building bzip2:

    If you have the needed bzlib.h, libbz2.lib, and libbz2.dll files
    you can skip building bzip2.  If not, open the libbz2.dsp project
    and build libbz2.dll

    This creates
      debug/libbz2.lib
    and
      libbz2.dll


    Building Zip:

    Copy libbz2.lib to the bzip2 directory in the Zip source tree.  This
    is needed to compile Zip with bzip2 support.  Also copy the matching
    bzlib.h from the bzip2 source to the same directory.

    Add libbz2.lib to the link list for whatever you are building.  Also
    define the compiler define BZIP2_SUPPORT.

    Build Zip.


    Using Zip with bzip2 as dll:

    Put libbz2.dll in your command path.  This is needed to run Zip with
    bzip2 support.

    Verify that bzip2 is enabled with the command

      zip -v

    You should see bzip2 listed.

  Compiling in bzip2 from the bzip2 source:

    This approach compiles in the bzip2 code directly.  No external
    library is needed.

    Get a copy of the bzip2 source and copy the contents to the bzip2
    directory in the Zip source tree so that bzlib.h is directly in
    the bzip2 directory.  (The current Zip source kit includes all the
    needed files in the bzip2 directory already where they need to go.)

    Use one of the standard VS 6 project builds.  These include bzip2.
    (One project also includes AES encryption if the AES kit has been
    added, the other excludes AES.)

    Verify that bzip2 is enabled with the command

      zip -v


  Windows DLL (WIN32):

    The VS 6 (Windows XP) and VS 2010 (Windows 7) DLL projects now include
    bzip2 by default.  These compile in the bzip2 files directly as noted
    above.  Set the proprocessor macro NO_BZIP2_SUPPORT to exclude bzip2.

    The updated examples show use of the updated DLLs.  The new command
    line DLL interface allows specifying use of bzip2 by just setting the
    compression method, as with -Z bzip2.


  Mac OS X:

  Follow the standard UNIX build procedure.  Mac OS X includes bzip2
  and the UNIX builders should find the bzip2 files in the standard
  places.  Note that the version of bzip2 on your OS may not be
  current and you can instead specify a different library or compile
  your own bzip2 library as noted in the Unix procedures above.  The
  preference is to build the bzip2 library using the standard bzip2
  files in bzip2/ and then link to that library.


  OS/2:

  Nothing yet.


  VMS (OpenVMS):

  See [.vms]INSTALL_VMS.txt for how to enable bzip2 support on VMS.

-----------------------------------------------------------------------

Updated 26 March 2007, 15 July 2007, 9 April 2008, 27 June 2008,
12 May 2010, 10 May 2014, 1 October 2014
S. Schweda, E. Gordon

