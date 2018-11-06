                        README_SZIP.txt
                        ---------------

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

      Description
      -----------

   The Info-ZIP programs UnZip (version 6.1 and later) and Zip (version
3.1 and later) offer optional support for LZMA compression (method 14)
and PPMd compression (method 98).  The PKWARE, Inc. APPNOTE describes
the LZMA method as an Early Feature Specification, hence subject to
change.

      http://www.pkware.com/documents/casestudies/APPNOTE.TXT

   Our LZMA implementation uses public-domain code from the LZMA
Software Development Kit (SDK) provided by Igor Pavlov:

      http://www.7-zip.org/sdk.html

Only a small subset of the LZMA SDK is used by (and provided with) the
Info-ZIP programs, in an "szip/" subdirectory (szip for SevenZip).

   Our PPMd implementation uses additional public-domain code from the
p7zip project:

      http://p7zip.sourceforge.net/
      http://sourceforge.net/projects/p7zip/

The PPMd-related files (also in the "szip/" subdirectory) are:

   From the LZMA SDK:
      szip/Ppmd.h

   From the p7zip (version 9.20.1) kit:
      szip/Ppmd8.c
      szip/Ppmd8.h
      szip/Ppmd8Dec.c
      szip/Ppmd8Enc.c

   Minor changes were made to various files to improve portability. 
Indentation was changed to keep "#" in column one (required by some
compilers).  Some global names longer than 31 characters were shortened
for VMS.  The use of "signed char" was made conditional.  "7zVersion.h"
was renamed to "SzVersion.h" to accommodate some IBM operating systems.
Originals of the changed files may be found in the "szip/orig/" directory.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

      Building UnZip and Zip with LZMA and/or PPMd Compression Support
      ----------------------------------------------------------------

   The build instructions (found either in the file INSTALL and/or an
OS-specific INSTALL supplement, depending on OS, in both the UnZip and
Zip source kits) describe how to build UnZip and Zip with support for
LZMA and/or PPMd compression.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

      Acknowledgement
      ---------------

   We're grateful to Igor Pavlov for providing the LZMA and PPMd
compression code, and to Dmitry Shkarin for the p7zip material.  Any
problems involving LZMA or PPMd compression in Info-ZIP programs should
be reported to the Info-ZIP team, not to Mr. Pavlov or Mr. Shkarin. 
However, any questions on LZMA or PPMd compression algorithms, or
regarding the original LZMA SDK or PPMd code (except as we modified and
use it) should be addressed to the original authors.

- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

      LZMA/PPMd Implementation History (see CHANGES for details)
      ----------------------------------------------------------

      2011-08-14  New.  LZMA SDK version 9.20.  (SMS)

      2011-08-20  Add description of new Zip LZMA support and make other
                  updates to this file.  (EG)

      2011-12-24  Add PPMd from p7zip version 9.20.1.  (SMS)

      2011-12-24  Remove references to 7zFile.[ch], SzFile.[ch].  (SMS)

      2012-04-21  Rename "lzma/" directory to "szip/".  (SMS)

      2014-01-15  Minor updates and bug fixes to szip/ files.  (SMS)

      2014-01-15  Update README_SZIP.txt (this file).  (EG)
