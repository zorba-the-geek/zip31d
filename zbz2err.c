/*
  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*---------------------------------------------------------------------------

  zbz2err.c

  This file contains the "fatal error" callback routine required by the
  "minimal" (silent, non-stdio) setup of the bzip2 compression library.

  The fatal bzip2 error bail-out routine is provided in a separate code
  module, so that it can be easily overridden when the Zip package is
  used as a static link library. One example is the WinDLL static library
  usage for building a monolithic binary of the Windows application "WiZ"
  that supports bzip2 both in compression and decompression operations.

  Contains:  bz_internal_error()      (BZIP2_SUPPORT only)

  Adapted from UnZip ubz2err.c, with all the DLL fine print stripped
  out.

  ---------------------------------------------------------------------------*/


#define __ZBZ2ERR_C     /* identifies this source module */

#include "zip.h"

#ifdef BZIP2_SUPPORT

/* Ask the MS VS linker to search for the bzip2 library. */
/* The below only works for Visual Studio 2010 build (Windows 7) and later
   (maybe also Vista).  It does not work with how bzip2 is included in the
   VS 6 build (Windows XP), which does not use a bzip2 static library.  As
   the library is not used, looking for the library generates an error, so
   we just don't look for it before Vista.
 */
/* If BZIP2_USEBZIP2DIR is defined, then Zip on WIN32 is not using the libbz2
   library (is compiling in the code instead), so this is not used in that case.
 */
# if defined(WIN32) && (WINVER >= 0x0600) && !defined(BZIP2_USEBZIP2DIR)
#  pragma comment( lib, "libbz2")
# endif

  /* It is useless to include the normal bzlib.h header from the bzip2 library,
     because it does not provide a prototype for the bz_internal_error()
     callback.
     However, on VMS, a special wrapper header is supplied by the Info-ZIP
     sources to support mixing of object modules compiled with case-sensitive
     and case-insensitive external names.  This wrapper provides a prototype
     for the callback using the correct case-mapping mode, which MUST be
     read before defining the function.
   */
# ifdef VMS
#   ifdef BZIP2_USEBZIP2DIR
#     include "bzip2/bzlib.h"
#   else
      /* If IZ_BZIP2 is defined as the location of the bzip2 files then
         assume the location has been added to include path.  For Unix
         this is done by the configure script. */
      /* Also do not need path for bzip2 include if OS includes support
         for bzip2 library. */
#     include "bzlib.h"
#   endif
# endif

/* Provide a prototype locally, to shut up potential compiler warnings. */
extern void bz_internal_error OF((int bzerrcode));

/**********************************/
/*  Function bz_internal_error()  */
/**********************************/

/* Call-back function for the bzip2 decompression code (compiled with
 * BZ_NO_STDIO), required to handle fatal internal bug-type errors of
 * the bzip2 library.
 */
void bz_internal_error(bzerrcode)
    int bzerrcode;
{
    sprintf(errbuf, "fatal error (code %d) in bzip2 library", bzerrcode);
    ziperr(ZE_LOGIC, errbuf);
} /* end function bz_internal_error() */

#endif /* def BZIP2_SUPPORT */
