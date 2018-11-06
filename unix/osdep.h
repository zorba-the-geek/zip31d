/*
  unix/osdep.h - Zip 3

  Copyright (c) 1990-2014 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/

#ifdef NO_LARGE_FILE_SUPPORT
# ifdef LARGE_FILE_SUPPORT
#  undef LARGE_FILE_SUPPORT
# endif
#endif

#ifdef LARGE_FILE_SUPPORT
  /* 64-bit Large File Support */

  /* The following Large File Summit (LFS) defines turn on large file support on
     Linux (probably 2.4 or later kernel) and many other unixen */

# define _LARGEFILE_SOURCE      /* some OSes need this for fseeko */
# define _LARGEFILE64_SOURCE
# define _FILE_OFFSET_BITS 64   /* select default interface as 64 bit */
# ifndef _LARGE_FILES
#   define _LARGE_FILES         /* some OSes need this for 64-bit off_t */
# endif
#endif

#include <sys/types.h>
#include <sys/stat.h>

/* printf format size prefix for zoff_t values */
#ifdef LARGE_FILE_SUPPORT
# define ZOFF_T_FORMAT_SIZE_PREFIX "ll"
#else
# define ZOFF_T_FORMAT_SIZE_PREFIX "l"
#endif

#ifdef NO_OFF_T
  typedef long zoff_t;
  typedef unsigned long uzoff_t;
#else
  typedef off_t zoff_t;
# if defined(LARGE_FILE_SUPPORT) && !(defined(__alpha) && defined(__osf__))
  typedef unsigned long long uzoff_t;
# else
  typedef unsigned long uzoff_t;
# endif
#endif
  typedef struct stat z_stat;


/* Automatically set ZIP64_SUPPORT if LFS */

#ifdef LARGE_FILE_SUPPORT
# ifndef NO_ZIP64_SUPPORT
#   ifndef ZIP64_SUPPORT
#     define ZIP64_SUPPORT
#   endif
# else
#   ifdef ZIP64_SUPPORT
#     undef ZIP64_SUPPORT
#   endif
# endif
#endif


/* symlinks */
#ifdef S_ISLNK
# ifndef SYMLINKS
#  define SYMLINKS
# endif
#endif


/* Added 2014-09-05 */
#define PROCNAME(n) (action == ADD || action == UPDATE ? wild(n) : \
                     procname(n, 1))


/* Process files in binary mode */
#if defined(__DJGPP__) || defined(__CYGWIN__)
#  define FOPR "rb"
#  define FOPM "r+b"
#  define FOPW "wb"
#endif


/* Enable the "UT" extra field (time info) */
#if !defined(NO_EF_UT_TIME) && !defined(USE_EF_UT_TIME)
#  define USE_EF_UT_TIME
#endif


/* 2013-04-11 SMS.  Have zrewind() in zipup.h. */
#ifndef NO_ETWODD_SUPPORT
# define ETWODD_SUPPORT
#endif /* ndef NO_ETWODD_SUPPORT */

/* None of this may be needed now. */
/* These should go away next beta. */
#if 0

/* 2013-11-18 SMS.
 * Define subsidiary object library macros based on ZIPLIB.
 * NO_ZPARCHIVE enables non-Windows api.c:comment().
 * USE_ZIPMAIN enables zip.c:zipmain() instead of main().
 */
#ifdef ZIPLIB
# ifndef NO_ZPARCHIVE
#  define NO_ZPARCHIVE
# endif
# ifndef USE_ZIPMAIN
#  define USE_ZIPMAIN
# endif
#endif /* def ZIPLIB */

#endif

