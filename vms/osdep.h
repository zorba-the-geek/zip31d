/*
  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
#ifndef VMS
#  define VMS 1
#endif

#if (defined(__VMS_VER) && !defined(__CRTL_VER))
#  define __CRTL_VER __VMS_VER
#endif

#if (defined(__VMS_VERSION) && !defined(VMS_VERSION))
#  define VMS_VERSION __VMS_VERSION
#endif

#if !(defined(__DECC) || defined(__DECCXX) || defined(__GNUC__))
     /* VAX C does not properly support the void keyword. (Only functions
        are allowed to have the type "void".)  */
#  ifndef NO_TYPEDEF_VOID
#    define NO_TYPEDEF_VOID
#  endif
#  define NO_FCNTL_H        /* VAXC does not supply fcntl.h. */
#endif /* VAX C */

#define USE_CASE_MAP
#define PROCNAME(n) \
 (((action == ADD) || (action == UPDATE) || (action == FRESHEN)) ? \
 wild(n) : procname(n, filter_match_case))


/* Progress dot control.  See fileio.c for details. */
#define PROGRESS_DOTS_PER_FLUSH 64


#if 0
/* 2015-07-31 SMS.
 * "long long" type.
 */
#ifdef __VAX
# define Z_LONGLONG long
# define UZ_LONGLONG unsigned long
#else
# define HAVE_LONG_LONG
# define Z_LONGLONG long long
# define UZ_LONGLONG unsigned long long
#endif

#ifdef __VAX
# define API_FILESIZE_T unsigned long
#else
# define API_FILESIZE_T unsigned long long
#endif
#endif


/* 2004-11-09 SMS.
   Large file support.
*/
#ifdef LARGE_FILE_SUPPORT

#  define _LARGEFILE                   /* Define the pertinent macro. */

/* LARGE_FILE_SUPPORT implies ZIP64_SUPPORT,
   unless explicitly disabled by NO_ZIP64_SUPPORT.
*/
#  ifdef NO_ZIP64_SUPPORT
#    ifdef ZIP64_SUPPORT
#      undef ZIP64_SUPPORT
#    endif /* def ZIP64_SUPPORT */
#  else /* def NO_ZIP64_SUPPORT */
#    ifndef ZIP64_SUPPORT
#      define ZIP64_SUPPORT
#    endif /* ndef ZIP64_SUPPORT */
#  endif /* def NO_ZIP64_SUPPORT */

#  define ZOFF_T_FORMAT_SIZE_PREFIX "ll"

#else /* def LARGE_FILE_SUPPORT */

#  define ZOFF_T_FORMAT_SIZE_PREFIX "l"

#endif /* def LARGE_FILE_SUPPORT */

/* Need _LARGEFILE for types.h. */

#include <types.h>

#ifdef __GNUC__
#include <sys/types.h>
#endif /* def __GNUC__ */

/* Need types.h for off_t. */

#ifdef LARGE_FILE_SUPPORT
   typedef off_t zoff_t;
   typedef unsigned long long uzoff_t;
#else /* def LARGE_FILE_SUPPORT */
   typedef long zoff_t;
   typedef unsigned long uzoff_t;
#endif /* def LARGE_FILE_SUPPORT */

#include <stat.h>

typedef struct stat z_stat;

#ifndef S_ISDIR                         /* VAX C V3.1-051 needs help. */
# define S_ISDIR(m)  (((m)& S_IFMT) == S_IFDIR)
#endif /* ndef S_ISDIR */

#ifndef S_ISREG                         /* VAX C V3.1-051 needs help. */
# define S_ISREG(m)  (((m)& S_IFMT) == S_IFREG)
#endif /* ndef S_ISREG */

#include <unixio.h>

/* Need <unixlib.h> (on old VMS versions) for:
 *    ctermid() declaration in ttyio.c,
 *    getcwd() declaration in api.c,
 *    getpid() declaration for srand seed.
 */
#if defined( __GNUC__) || defined( ZIPLIB) || defined( ZCRYPT_INTERNAL)
#  define NEED_UNIXLIB_H
#endif

#ifdef NEED_UNIXLIB_H
#  include <unixlib.h>
#endif

#if defined(_MBCS)
#  undef _MBCS                 /* Zip on VMS does not support MBCS */
#endif

/* VMS is run on little-endian processors with 4-byte ints:
 * enable the optimized CRC-32 code */
#ifdef IZ_CRC_BE_OPTIMIZ
#  undef IZ_CRC_BE_OPTIMIZ
#endif
#if !defined(IZ_CRC_LE_OPTIMIZ) && !defined(NO_CRC_OPTIMIZ)
#  define IZ_CRC_LE_OPTIMIZ
#endif
#if !defined(IZ_CRCOPTIM_UNFOLDTBL) && !defined(NO_CRC_OPTIMIZ)
#  define IZ_CRCOPTIM_UNFOLDTBL
#endif

#if !defined(NO_EF_UT_TIME) && !defined(USE_EF_UT_TIME)
#  if (defined(__CRTL_VER) && (__CRTL_VER >= 70000000))
#    define USE_EF_UT_TIME
#  endif
#endif

#if defined(VMS_PK_EXTRA) && defined(VMS_IM_EXTRA)
#  undef VMS_IM_EXTRA                 /* PK style takes precedence */
#endif
#if !defined(VMS_PK_EXTRA) && !defined(VMS_IM_EXTRA)
#  define VMS_PK_EXTRA 1              /* PK style VMS support is default */
#endif

/* 2014-04-18 SMS.
 * IM-style vms/vms_im.c:vms_read() is incompatible with the any-size
 * requests used by the LZMA compression code.
 */
#if defined( LZMA_SUPPORT) && defined( VMS_IM_EXTRA)
    Bad code: error: LZMA_SUPPORT incompatible with VMS_IM_EXTRA.
#endif

/* 2007-02-22 SMS.
 * <unistd.h> is needed for symbolic link functions, so use it when the
 * symbolic link criteria are met.
 */
#if defined(__VAX) || __CRTL_VER < 70301000
#  define NO_UNISTD_H
#  define NO_SYMLINKS
#endif /* defined(__VAX) || __CRTL_VER < 70301000 */

/* 2007-02-22 SMS.  Use delete() when unlink() is not available. */
#if defined(NO_UNISTD_H) || (__CRTL_VER < 70000000)
#  define unlink delete
#endif /* defined(NO_UNISTD_H) || __CRTL_VER < 70000000) */

#define SSTAT vms_stat

/* 2013-04-11 SMS.  Have zrewind() in zipup.h. */
#ifndef NO_ETWODD_SUPPORT
# define ETWODD_SUPPORT
#endif /* ndef NO_ETWODD_SUPPORT */

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

#ifdef USE_ZIPMAIN
# define EXIT( exit_code) return( exit_code)
# define RETURN( exit_code) return( exit_code)
#else /* def USE_ZIPMAIN */
# define EXIT( exit_code) vms_exit( exit_code)
# define RETURN( exit_code) return (vms_exit( exit_code), 1)
#endif /* def USE_ZIPMAIN [else] */

/* 2011-04-21 SMS.
 * Moved strcasecmp() stuff from vmszip.c to here.
 * (This must follow the large-file stuff so that _LARGEFILE is defined
 * before <decc$types.h> gets read, and <decc$types.h> does get read
 * before this.)
 */

/* Judge availability of str[n]casecmp() in C RTL.
 * (Note: This must follow a "#include <decc$types.h>" in something to
 * ensure that __CRTL_VER is as defined as it will ever be.  DEC C on
 * VAX may not define it itself.)
 */
#if __CRTL_VER >= 70000000
# define HAVE_STRCASECMP
#endif /* __CRTL_VER >= 70000000 */

#ifdef HAVE_STRCASECMP
# include <strings.h>    /* str[n]casecmp() */
#else /* def HAVE_STRCASECMP */
# include <limits.h>
# ifndef UINT_MAX
#  define UINT_MAX 4294967295U
# endif
# define strcasecmp( s1, s2) strncasecmp( s1, s2, UINT_MAX)
extern int strncasecmp( char *, char *, size_t);
#endif /* def HAVE_STRCASECMP [else] */


/* UNICODE.  (Still in the dream stage.) */
#if __CRTL_VER >= 60200000
# define HAVE_WCHAR_H
#endif


/* Accommodation for /NAMES = AS_IS with old header files. */

# define cma$tis_errno_get_addr CMA$TIS_ERRNO_GET_ADDR
# define cma$tis_vmserrno_get_addr CMA$TIS_VMSERRNO_GET_ADDR
# define lib$establish LIB$ESTABLISH
# define lib$getdvi LIB$GETDVI
# define lib$getjpi LIB$GETJPI
# define lib$getsyi LIB$GETSYI
# define lib$get_foreign LIB$GET_FOREIGN
# define lib$get_input LIB$GET_INPUT
# define lib$sig_to_ret LIB$SIG_TO_RET
# define ots$cvt_tu_l OTS$CVT_TU_L
# define str$concat STR$CONCAT
# define str$find_first_substring STR$FIND_FIRST_SUBSTRING
# define str$free1_dx STR$FREE1_DX
# define sys$asctim SYS$ASCTIM
# define sys$assign SYS$ASSIGN
# define sys$bintim SYS$BINTIM
# define sys$close SYS$CLOSE
# define sys$connect SYS$CONNECT
# define sys$dassgn SYS$DASSGN
# define sys$device_scan SYS$DEVICE_SCAN
# define sys$display SYS$DISPLAY
# define sys$getjpiw SYS$GETJPIW
# define sys$gettim SYS$GETTIM
# define sys$open SYS$OPEN
# define sys$parse SYS$PARSE
# define sys$qiow SYS$QIOW
# define sys$read SYS$READ
# define sys$search SYS$SEARCH


#ifdef __DECC

/* File open callback ID values. */

#  define FOPM_ID 1
#  define FOPR_ID 2
#  define FOPW_ID 3

/* File open callback ID storage. */

extern int fopm_id;
extern int fopr_id;
extern int fopw_id;

/* File open callback ID function. */

extern int acc_cb();

/* Option macros for zfopen().
 * General: Stream access
 * Output: fixed-length, 512-byte records.
 *
 * Callback function (DEC C only) sets deq, mbc, mbf, rah, wbh, ...
 */

#  define FOPM "r+b", "ctx=stm", "rfm=fix", "mrs=512", "acc", acc_cb, &fopm_id
#  define FOPR "rb",  "ctx=stm", "acc", acc_cb, &fopr_id
#  define FOPW "wb",  "ctx=stm", "rfm=fix", "mrs=512", "acc", acc_cb, &fopw_id

#else /* def __DECC */ /* (So, GNU C, VAX C, ...)*/

#  define FOPM "r+b", "ctx=stm", "rfm=fix", "mrs=512"
#  define FOPR "rb",  "ctx=stm"
#  define FOPW "wb",  "ctx=stm", "rfm=fix", "mrs=512"

#endif /* def __DECC */

