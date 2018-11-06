/*
  Copyright (c) 1990-2014 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/* Include file for z/VM CMS and z/OS MVS non-POSIX files           */

/* This is normally named osdep.h on most systems.                    
   - These non-POSIX filesystems do not support directories, only
     PDS/PDSE-stype libraries, so this file has been given a unique
     name to avoid confusion.                                       */


#ifndef __cmsmvs_h   /* prevent multiple inclusions */
#define __cmsmvs_h

#ifdef MVS
#  define _POSIX_SOURCE       /* Tell z/OS we want full definitions */
#  include <features.h>
#endif /* MVS */

#include <time.h>             /* the usual non-BSD time functions   */
   
#ifdef MVS                    /* z/OS                               */
#  include <sys/stat.h>       /* - POSIX stat() et al               */
#  include <sys/modes.h>
#else /* !MVS */              /* z/VM                               */
#  include "cstat.h"          /* - CMS stat() simulation            */
#endif

#if defined(__VM__) && !defined(VM_CMS)
#  define VM_CMS              /* Building zip for z/VM              */
#endif

#define CMS_MVS               /* z/OS & z/VM non-POSIX file support */
#ifndef EBCDIC
#  define EBCDIC
#endif

#ifndef MVS                   /* z/VM does not provde these         */ 
#  define NO_UNISTD_H
#  define NO_FCNTL_H
#endif /*MVS */

/* If generating a stand-alone z/VM CMS module, patch in a new      */
/* main() function before the real main() to access program args.   */
#ifdef CMS_STAND_ALONE
#  define USE_ZIPMAIN
#endif

#ifndef NULL
#  define NULL 0
#endif

#define PASSWD_FROM_STDIN
                  /* Kludge until we know how to open a non-echo tty channel */

/* definition for ZIP */
#define getch() getc(stdin)
#define MAXPATHLEN 128
#define NO_RMDIR
#define NO_MKTEMP
#define USE_CASE_MAP

#ifndef MVS                   /* z/VM I/O routines                  */ 
#  define fileno(x) (char *)(x)
#  define fdopen fopen
#  define unlink remove
#  define link rename
#  define utime(f,t)
#endif /*MVS */

#ifdef MVS                    /* z/OS I/O routines                  */ 
#  include <unistd.h>         /* - usually defines _POSIX_VERSION
                                 - z/OS isatty() does not work in
                                   batch. #include isatty defn,
                                   then override like z/VM          */
#endif /*MVS */
#define isatty(t) 1           /* Indicate support not available     */

#ifdef ZCRYPT_INTERNAL
#  define ZCR_SEED2     (unsigned)3141592654L   /* use PI as seed pattern */
#endif

#ifdef MVS                    /* z/OS CSECTs for separate compiles  */
#  if defined(__CRC32_C)
#    pragma csect(STATIC,"crc32_s")
#  elif defined(__DEFLATE_C)
#    pragma csect(STATIC,"deflat_s")
#  elif defined(__ZIPFILE_C)
#    pragma csect(STATIC,"zipfil_s")
#  elif defined(__ZIPUP_C)
#    pragma csect(STATIC,"zipup_s")
#  endif
#endif /* MVS */

/* end defines for ZIP */


/* fopen() options                     
   - The "byteseek" option enables byte seeks for a binary file  
   - All input files are processed in binary to allow explicit   
     determination of binary vs text, and processing of line
     terminations                                                   */

/* FOPR: Read/Only access to existing input files                   */
#define FOPR "rb,byteseek"

/* FOPR: Read/Write access to existing non-POSIX ZIP files (for -g) */
#define FOPM "rb+,byteseek"

/* FOPW: Write access for new non-POSIX ZIP files
   - z/OS MVS: RECFM=V, LRECL=4KB
     - Was RECFM=U, LRECL=32KB, but that is reserved for executble  
       programs, not user data files
   - z/VM CMS: RECFM=V, LRECL=32KB                                  */
#ifdef MVS                    /* z/OS MVS                           */
  #define FOPW "wb,recfm=v,lrecl=4096,byteseek"
#else /* !MVS */              /* z/VM CMS                           */
  #define FOPW "wb,recfm=v,lrecl=32760,byteseek"
#endif /* MVS */

/* FOPW_TMP: Read/Write access to zip-created temporary               
   - Currently uses hiperspace memory file (auxilary address space)
   - Q: Should this be "wb"?                                        */
#if 0
#define FOPW_TMP "w,byteseek"
#else
#define FOPW_TMP "w,type=memory(hiperspace)"
#endif

#define CBSZ 0x40000
#define ZBSZ 0x40000

#endif /* !__cmsmvs_h */
