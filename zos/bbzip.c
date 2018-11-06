/*
  zos/bbzip.c - Zip 3

  Copyright (c) 1990-2014 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*--------------------------------------------------------------------------

  zos/bbzip.c

  This source file supports the compilation of the Infozip ZIP utility.
                                                              
  This enables the InfoZip source code to be used unmodified to build the
  utility.  A single compilation is performed, with each of the required C
  header and source files #included.                          

  This greatly simplifies the compilation of the z/OS MVS ZIP executable,
  and results in optimum performance by providing a cheap form of global
  program optimization.

  ---------------------------------------------------------------------------*/
#pragma title ("BBZIP -- InfoZip ZIP Utility") 

#pragma runopts(ENV(MVS),PLIST(MVS), TRAP(OFF), NOEXECOPS,             \
                STACK(64K),                                            \
                HEAP(512K,1M,ANYWHERE,FREE),                           \
                NOTEST(NONE,INSPICD,PROMPT) )

//#pragma strings( readonly )
#pragma csect(code,   "IZ_MAIN")
#pragma csect(static, "IZ_WS")
#pragma csect(test,   "IZ_TEST")

#pragma comment( copyright , "1990-2014 Info-ZIP.  All rights reserved" ) 

#pragma subtitle( "Load module eye-catchers" )
#pragma page()

/*------------------------------------------------------
| Program build time eye-catchers for executable.      |
------------------------------------------------------*/
static const char * EYECATCHER_EyeCatcher  = "EYE CATCHER" ;
static const char * EYECATCHER_Version     = "03.01.00" ;
static const char * EYECATCHER_Datestamp   = __DATE__ ;
static const char * EYECATCHER_Timestamp   = __TIME__ ;
static const char * EYECATCHER_Filename    = __FILE__ ;

#pragma subtitle ("Build Controls")
#pragma page ()

/* #define DEBUG */

#define MVS
#define _POSIX_SOURCE
#define IZ_BIGBUILD             /* Single compilation build */
#define GLOBALS

#define PROGRAM_ID   "ZIP"

#pragma subtitle ("Include Files")
#pragma page ()

   /*------------------------------------------------------+
   | System include files.                                 |
   +------------------------------------------------------*/

   /*-------------------------------------------------------
   | ZIP utility: mainline                                 |
   -------------------------------------------------------*/
#include "zip.c"

   /*-------------------------------------------------------
   | ZIP utility: encryption routines                      |
   -------------------------------------------------------*/
#include "crypt.c"

   /*-------------------------------------------------------
   | ZIP utility: tty I/O routines                         |
   -------------------------------------------------------*/
#include "ttyio.c"

   /*-------------------------------------------------------
   | ZIP utility: tree routines                            |
   -------------------------------------------------------*/
#include "trees.c"

   /*-------------------------------------------------------
   | ZIP utility: deflate routines                         |
   -------------------------------------------------------*/
#include "deflate.c"

   /*-------------------------------------------------------
   | ZIP utility: file I/O routines                        |
   -------------------------------------------------------*/
#include "fileio.c"

   /*-------------------------------------------------------
   | ZIP utility: global variable definitions              |
   -------------------------------------------------------*/
#include "globals.c"
#undef UTIL

   /*-------------------------------------------------------
   | ZIP utility: utility routines                         |
   -------------------------------------------------------*/
#include "util.c"

   /*-------------------------------------------------------
   | ZIP utility: 32-bit CRC routines                      |
   -------------------------------------------------------*/
#include "crc32.c"

   /*-------------------------------------------------------
   | ZIP utility: ZIP file routines                        |
   -------------------------------------------------------*/
#include "zipfile.c"

   /*-------------------------------------------------------
   | ZIP utility: Compression routines                     |
   -------------------------------------------------------*/
#include "zipup.c"

   /*-------------------------------------------------------
   | ZIP utility: Routines common to VM/CMS and MVS        |
   -------------------------------------------------------*/
#include "cmsmvs.c"

   /*-------------------------------------------------------
   | ZIP utility: MVS-specific routines                    |
   -------------------------------------------------------*/
#include "mvs.c"

#pragma title ("BBZIP -- InfoZip ZIP Utility") 
#pragma subtitle (" ")
