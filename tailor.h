/*
  tailor.h - Zip 3.1

  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-02 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/

/* Some compiler distributions for Win32/i386 systems try to emulate
 * a Unix (POSIX-compatible) environment.
 */
#if (defined(WIN32) && defined(UNIX))
   /* Zip does not support merging both ports in a single executable. */
#  if (defined(FORCE_WIN32_OVER_UNIX) && defined(FORCE_UNIX_OVER_WIN32))
     /* conflicting choice requests -> we prefer the Win32 environment */
#    undef FORCE_UNIX_OVER_WIN32
#  endif
#  ifdef FORCE_WIN32_OVER_UNIX
     /* native Win32 support was explicitly requested... */
#    undef UNIX
#  else
     /* use the POSIX (Unix) emulation features by default... */
#    undef WIN32
#  endif
#endif

/* UNICODE */
#ifdef NO_UNICODE_SUPPORT
/* NO_UNICODE_SUPPORT override disables Unicode support
   - Useful for environments like MS Visual Studio */
# ifdef UNICODE_SUPPORT
#   undef UNICODE_SUPPORT
# endif
#endif

/* This needs fixing, but currently assume that we don't have full
   Unicode support unless UNICODE_WCHAR is set.

   For Unicode support, both UNICODE_SUPPORT must be defined (to enable the
   Unicode parts of the code) and either UNICODE_WCHAR (Unicode using the
   ANSI wide functions) and/or UNICODE_ICONV (to do local <-> UTF-8 encoding
   translations) needs to be set.  As iconv does not support things like
   uppercase to lowercase conversions, some features may be disabled if
   the port does not address them.

   It's important to realize that zipfiles are now ASCII UTF-8 by default,
   meaning that the native character set paths are stored in a zip archive
   is now UTF-8.  This is per the AppNote zip standard.  Any ports where
   the local character set is not ASCII, such as EBCDIC, needs to take care
   to distinguish between internal names (which tend to be ASCII UTF-8) and
   external names (which may be ASCII or EBCDIC).
   
   These are possible:

   UNICODE_SUPPORT       = Enables cross-platform character set support via
                           storage of UTF-8.  Requires either UNICODE_WCHAR or
                           UNICODE_ICONV to convert between local paths and
                           UTF-8.

   UNICODE_WCHAR         = We have the wide character support we need for
                           Unicode.  This should only be set if wide characters
                           are represented in Unicode (see tests in
                           unix/configure) and the port has working versions of
                           towupper(), towlower(), and iswprint() (and maybe
                           other wide functions).  This is set for Windows in
                           win32/osdep.h.  For Unix, if unix/configure found
                           the port is missing something, the port needs to
                           provide it and can turn UNICODE_WCHAR back on
                           manually.

                           Only this mode supports Unicode directory scans and
                           so only this mode can archive all files on a file
                           system, even those not representable in the local
                           character set.  If full file system scanning using
                           Unicode is supported, UNICODE_FILE_SCAN is set.
                           Note that this is also set if the native charset
                           when Zip is compiled is a UTF-8 one.

                           This is the preferred mode to operate in, as all
                           Unicode operations can be done using the port's wide
                           API functions and so is likely to be more consistent.

   UNICODE_FILE_SCAN     = If set, the port does file system scans using
                           Unicode.  This allows reading and storing files that
                           are not readable using the local character set, so
                           may allow a more complete file system scan.

   HAVE_WCHAR            = This is set when wide characters are not known to be
                           represented in Unicode, but otherwise the conditions
                           of UNICODE_WCHAR are met.  If this is defined, then
                           it is assumed that working versions of wide character
                           functions, such as towlower(), exist and conversions
                           to and from wide characters are supported.  This
                           might be used with UNICODE_ICONV, where character
                           set translations are done by iconv, but command line
                           processing is done using a known character set and
                           wide functions such as towupper and iswprint should
                           work correctly in this case.  Note that HAVE_WCHAR
                           is only a flag of capability and does not enable any
                           functionality on its own.  The port must do that.
                           The path case conversion functions (-Cl and -Cu) are
                           disabled unless USE_WCHAR_CASE_CONV or
                           USE_PORT_CASE_CONV is set.
                           
   USE_WCHAR_CASE_CONV   = Use the wchar_t wide functions towlower and towupper
                           to convert letter case in paths.  This is set if
                           UNICODE_WCHAR is set.  If UNICODE_WCHAR is not set,
                           then it's not clear if converting UTF-8 paths to wide,
                           using towlower or towupper to convert case, then
                           converting back to UTF-8 is reliable.  This needs to
                           be determined for a port before USE_WCHAR_CASE_CONV
                           is set.  If USE_WCHAR_CASE_CONV is not set, then the
                           path case conversion functions -Cl and -Cu are
                           disabled, unless the port sets USE_PORT_CASE_CONV
                           and supplies replacement functions for
                           ustring_upper_lower and astring_upper_lower in
                           zipfile.c.
                         
   USE_PORT_CASE_CONV    = Instead of using the default ustring_upper_lower
                           UTF-8 function (which uses towlower and towupper)
                           and the default astring_upper_lower function (for
                           ASCII 7-bit case conversions), the port will
                           provide replacements for ustring_upper_lower and
                           astring_upper_lower.

   UNICODE_SUPPORT_WIN32 = This is shorthand for:
                             defined(UNICODE_SUPPORT) && defined(WIN32)
                           and should be set automatically.  UNICODE_WCHAR must
                           be set to set this.  Much of the WIN32 Unicode code
                           is unique to Windows, so it makes sense to make this
                           a distinct case.  UNICODE_SUPPORT_WIN32 is a fully
                           compatible variation of UNICODE_WCHAR.

   HAVE_ICONV            = A test indicates we have sufficient iconv support.
                           This is typically set in unix/configure, but a port
                           could set this manually if it is known iconv is
                           available.  However, iconv is not used unless the
                           port sets UNICODE_ICONV.

   UNICODE_ICONV         = Enables using iconv for conversions between the
                           local character set and Unicode (UTF-8, as well as
                           maybe UCS-32).  This is disabled if HAVE_ICONV is
                           not set.  If UNICODE_WCHAR is set, then conversions
                           between local and UTF-8 paths are done using wide
                           functions, but the new options --iconv-from-code
                           and --iconv-to-code are available to convert paths
                           in other known character sets to and from UTF-8.
                           If UNICODE_WCHAR is not set, conversions from and
                           to UTF-8 use iconv.  The port must supply working
                           versions of astring_upper_lower() and
                           ustring_upper_lower() to replace those in zipfile.c
                           if path case conversion is to be supported.
                           (HAVE_WCHAR is not enough to enable this by
                           default.)

   UTF8_MAYBE_NATIVE     = This flag indicates that the local character set
                           may be UTF-8.  UNICODE_WCHAR is required, which
                           should be the case, or this is disabled.  We can
                           use just the local MBCS functions for parsing and
                           storing the local path as UTF-8.  We still need
                           the wide functions for performing operations on
                           characters where MBCS support of these operations
                           is not provided, such as case conversion of wide
                           characters.  In this mode, there essentially is no
                           local version of the path, just a UTF-8 version
                           which is also local.

                           Note that the UTF8_MAYBE_NATIVE macro actually
                           does nothing.  If UNICODE_SUPPORT is defined and
                           setlocale is available, Zip checks the current
                           locale when it starts up and sets using_utf8 if it
                           finds the native character set is UTF-8.  This
                           macro is just for use in #if constructs and scripts.

   Note that the code assumes the port has working toupper and tolower
   functions.  It is up to the port to make sure the locale (or equivalent)
   is set to allow these to work with the character set used by the command
   line.  Zip option processing uses only the characters in 7-bit ASCII for
   options and option values that come from lists (such as encryption method).
   Other values can be outside of this, but such values are not processed,
   just passed as is.  So toupper and tolower only have to work with the
   single byte characters Zip uses for options and option choices.  Other
   values can contain nearly any character in the local character set, but
   are still assumed to be proper MBCS (if _MBCS is enabled).  (If Windows
   wide console support is enabled, this would be an exception.)
   
   The globals localename and charactersetname are set to the output of
   setlocale() and nl_langinfo(), respectively.  (Windows does not have
   nl_langinfo and derives charactersetname from the locale.)  These are
   NULL if a port does not provide them.

   Typically a port would define UNICODE_SUPPORT, HAVE_WCHAR and UNICODE_WCHAR.
   Wide characters are used for full Unicode support.  This approach is
   preferred if supported.

   If a working wide implementation is not available, and iconv is available,
   then the port can set UNICODE_SUPPORT, HAVE_ICONV (assuming the iconv
   support is sufficient) and UNICODE_ICONV.  The port then needs to set either
   USE_WCHAR_CASE_CONV or USE_PORT_CASE_CONV to enable -Cl and -Cu.
   
   If on startup Zip finds the local character set is UTF-8, native UTF-8
   support is enabled.  UNICODE_WCHAR must also be defined, as conversions to
   and from wide characters are used, so this mode is really a special case
   of UNICODE_WCHAR.

   This is still being worked. */


#ifdef AMIGA
#include "amiga/osdep.h"
#endif

#ifdef AOSVS
#include "aosvs/osdep.h"
#endif

#ifdef ATARI
#include "atari/osdep.h"
#endif

#ifdef __ATHEOS__
#include "atheos/osdep.h"
#endif

#ifdef __BEOS__
#include "beos/osdep.h"
#endif

#ifdef DOS
#include "msdos/osdep.h"
#endif

#ifdef __human68k__
#include "human68k/osdep.h"
#endif

#if ((defined(__MWERKS__) && defined(macintosh)) || defined(MACOS))
#include "macos/osdep.h"
#endif

#ifdef NLM
#include "novell/osdep.h"
#endif

#ifdef OS2
#include "os2/osdep.h"
#endif

#ifdef __riscos
#include "acorn/osdep.h"
#endif

#ifdef QDOS
#include "qdos/osdep.h"
#endif

#ifdef __TANDEM
#include "tandem.h"
#include "tanzip.h"
#endif

#ifdef UNIX
#include "unix/osdep.h"
#endif

#if defined(__COMPILER_KCC__) || defined(TOPS20)
#include "tops20/osdep.h"
#endif

#if defined(VMS) || defined(__VMS)
#include "osdep.h"
#endif

#if defined(__VM__) || defined(VM_CMS) || defined(MVS)
#include "cmsmvs.h"
#endif

#ifdef WIN32
#include "win32/osdep.h"
#endif

#ifdef THEOS
#include "theos/osdep.h"
#endif


/* Assume have setlocale unless port says otherwise. */
#ifndef NO_SETLOCALE
# ifndef HAVE_SETLOCALE
#  define HAVE_SETLOCALE
# endif
#endif



/* ---------------------------------------------------------------------- */
/* Currently UNICODE_SUPPORT requires UNICODE_WCHAR and/or UNICODE_ICONV. */


/* --------------------------------------
   Right now UNICODE_ICONV is not used.
   HAVE_ICONV enables support for iconv,
   which it should exist before setting
   this.
   -------------------------------------- */

/* Ports that do not use unix/configure need to set UNICODE_WCHAR,
   HAVE_ICONV, UNICODE_ICONV, and UNICODE_SUPPORT in their osdep.h
   file.  See above for guidance. */


/* Allow disabling UNICODE_WCHAR */
#ifdef NO_UNICODE_WCHAR
# ifdef UNICODE_WCHAR
#  undef UNICODE_WCHAR
# endif
#endif

/* UNICODE_ICONV requires HAVE_ICONV */
#ifdef UNICODE_ICONV
# ifndef HAVE_ICONV
#  undef UNICODE_ICONV
# endif
#endif

/* UTF8_MAYBE_NATIVE requires UNICODE_WCHAR */
#ifdef UTF8_MAYBE_NATIVE
# ifndef UNICODE_WCHAR
#  undef UTF8_MAYBE_NATIVE
# endif
#endif

/* If UNICODE_WCHAR is not defined, try UNICODE_ICONV if HAVE_ICONV */
#ifndef UNICODE_WCHAR
# ifdef HAVE_ICONV
#  ifndef UNICODE_ICONV
#   define UNICODE_ICONV
#  endif
# endif
#endif

/* Win32 Unicode support requires UNICODE_WCHAR */
#if defined(UNICODE_SUPPORT) && defined(WIN32)
# ifndef UNICODE_WCHAR
#  undef UNICODE_SUPPORT
# endif
#endif

/* UNICODE_SUPPORT requires either UNICODE_WCHAR and/or UNICODE_ICONV */
#ifdef UNICODE_SUPPORT
# if !(defined(UNICODE_WCHAR) || defined(UNICODE_ICONV))
#  undef UNICODE_SUPPORT
# endif
#endif

/* if UNICODE_WCHAR, assume UNICODE_FILE_SCAN unless port says otherwise */
#ifdef UNICODE_WCHAR
# ifndef NO_UNICODE_FILE_SCAN
#  ifndef UNICODE_FILE_SCAN
#   define UNICODE_FILE_SCAN
#  endif
# endif
#endif

/* if UNICODE_WCHAR, use WCHAR functions for case conversion */
#ifdef UNICODE_WCHAR
# ifndef USE_WCHAR_CASE_CONV
#  define USE_WCHAR_CASE_CONV
# endif
#endif

/* Windows unique Unicode code is in UNICODE_SUPPORT_WIN32 blocks */
#if defined(UNICODE_SUPPORT) && defined(WIN32)
# ifndef UNICODE_SUPPORT_WIN32
#  define UNICODE_SUPPORT_WIN32
# endif
#endif

/* Disable -Cl and -Cu path case conversion if no way to do it */
#if defined(USE_WCHAR_CASE_CONV) || defined(USE_PORT_CASE_CONV)
/* enable it */
# ifndef ENABLE_PATH_CASE_CONV
#  define ENABLE_PATH_CASE_CONV
# endif
#else
/* disable it */
# ifdef ENABLE_PATH_CASE_CONV
#  undef ENABLE_PATH_CASE_CONV
# endif
#endif



/* ---------------------------------------------------------------------- */


#ifdef NEED_STRERROR
   char *strerror();
#endif /* def NEED_STRERROR */


/* generic LARGE_FILE_SUPPORT defines
   These get used if not defined above.
   7/21/2004 EG
*/
/* If a port hasn't defined ZOFF_T_FORMAT_SIZE_PREFIX
   then probably need to define all of these. */
#ifndef ZOFF_T_FORMAT_SIZE_PREFIX

# ifdef LARGE_FILE_SUPPORT
    /* Probably passed in from command line instead of in above
       includes if get here.  Assume large file support and hope. 8/14/04 EG */

    /* Set the Large File Summit (LFS) defines to turn on large file support
       in case it helps. */

#   define _LARGEFILE_SOURCE    /* some OSes need this for fseeko */
#   define _LARGEFILE64_SOURCE
#   define _FILE_OFFSET_BITS 64 /* select default interface as 64 bit */
#   ifndef _LARGE_FILES
#     define _LARGE_FILES       /* some OSes need this for 64-bit off_t */
#   endif

    typedef off_t zoff_t;
    typedef unsigned long long uzoff_t;  /* unsigned zoff_t (12/29/04 EG) */

    /* go with common prefix for use with printf */
#   define ZOFF_T_FORMAT_SIZE_PREFIX "ll"

# else
    /* Default type for offsets and file sizes was ulg but reports
       of using ulg to create files from 2 GB to 4 GB suggest
       it doesn't work well.  Now just switch to Zip64 or not
       support over 2 GB.  7/24/04 EG */
    /* Now use uzoff_t for unsigned things.  12/29/04 EG */
    typedef long zoff_t;
    typedef unsigned long uzoff_t;

#   define ZOFF_T_FORMAT_SIZE_PREFIX "l"

# endif

  typedef struct stat z_stat;

  /* flag that we are defaulting */
# define USING_DEFAULT_LARGE_FILE_SUPPORT
#endif


/* --------------------------------------- */


#if (defined(USE_ZLIB) && defined(ASM_CRC))
#  undef ASM_CRC
#endif

#if (defined(USE_ZLIB) && defined(ASMV))
#  undef ASMV
#endif

/* When "void" is an alias for "int", prototypes cannot be used. */
#if (defined(NO_VOID) && !defined(NO_PROTO))
#  define NO_PROTO
#endif

/* Used to remove arguments in function prototypes for non-ANSI C */
#ifndef NO_PROTO
#  define OF(a) a
#  define OFT(a) a
#else /* NO_PROTO */
#  define OF(a) ()
#  define OFT(a)
#endif /* ?NO_PROTO */

/* If the compiler can't handle const define ZCONST in osdep.h */
/* Define const itself in case the system include files are bonkers */
#ifndef ZCONST
#  ifdef NO_CONST
#    define ZCONST
#    define const
#  else
#    define ZCONST const
#  endif
#endif

/*
 * Some compiler environments may require additional attributes attached
 * to declarations of runtime libary functions (e.g. to prepare for
 * linking against a "shared dll" version of the RTL).  Here, we provide
 * the "empty" default for these attributes.
 */
#ifndef IZ_IMP
#  define IZ_IMP
#endif


/* Windows does not have strcasecmp() */
#ifndef STRCASECMP
# ifdef WIN32
#  define STRCASECMP(X,Y)  stricmp(X,Y)
# else
#  define STRCASECMP(X,Y)  strcasecmp(X,Y)
# endif
#endif



/*
 * case mapping functions. case_map is used to ignore case in comparisons,
 * to_up is used to force upper case even on Unix (for dosify option).
 */
#ifdef USE_CASE_MAP
#  define case_map(c) upper[(c) & 0xff]
#  define to_up(c)    upper[(c) & 0xff]
#else
#  define case_map(c) (c)
#  define to_up(c)    ((c) >= 'a' && (c) <= 'z' ? (c)-'a'+'A' : (c))
#endif /* USE_CASE_MAP */

/* Define void, zvoid, and extent (size_t) */
#include <stdio.h>

#ifndef NO_STDDEF_H
#  include <stddef.h>
#endif /* !NO_STDDEF_H */

#ifndef NO_STDLIB_H
#  include <stdlib.h>
#endif /* !NO_STDLIB_H */

#ifndef NO_UNISTD_H
#  include <unistd.h> /* usually defines _POSIX_VERSION */
#endif /* !NO_UNISTD_H */

#ifndef NO_FCNTL_H
#  include <fcntl.h>
#endif /* !NO_FNCTL_H */

#ifndef NO_STRING_H
#  include <string.h>
#else
#  include <strings.h>
#endif /* NO_STRING_H */

#ifdef NO_VOID
#  define void int
   typedef char zvoid;
#else /* !NO_VOID */
# ifdef NO_TYPEDEF_VOID
#  define zvoid void
# else
   typedef void zvoid;
# endif
#endif /* ?NO_VOID */

#ifdef NO_STRRCHR
#  define strrchr rindex
#endif

#ifdef NO_STRCHR
#  define strchr index
#endif

/*
 * A couple of forward declarations that are needed on systems that do
 * not supply C runtime library prototypes.
 */
#ifdef NO_PROTO
IZ_IMP char *strcpy();
IZ_IMP char *strcat();
IZ_IMP char *strrchr();
/* XXX use !defined(ZMEM) && !defined(__hpux__) ? */
#if !defined(ZMEM) && defined(NO_STRING_H)
IZ_IMP char *memset();
IZ_IMP char *memcpy();
#endif /* !ZMEM && NO_STRING_H */

/* XXX use !defined(__hpux__) ? */
#ifdef NO_STDLIB_H
IZ_IMP char *calloc();
IZ_IMP char *malloc();
IZ_IMP char *getenv();
IZ_IMP long atol();
#endif /* NO_STDLIB_H */

#ifndef NO_MKTEMP
IZ_IMP char *mktemp();
#endif /* !NO_MKTEMP */

#endif /* NO_PROTO */

/*
 * SEEK_* macros, should be defined in stdio.h
 */
/* Define fseek() commands */
#ifndef SEEK_SET
#  define SEEK_SET 0
#endif /* !SEEK_SET */

#ifndef SEEK_CUR
#  define SEEK_CUR 1
#endif /* !SEEK_CUR */

#ifndef FALSE
#  define FALSE 0
#endif

#ifndef TRUE
#  define TRUE 1
#endif

#ifdef NO_SIZE_T
   typedef unsigned int extent;
   /* define size_t 3/17/05 EG */
   typedef unsigned int size_t;
#else
   typedef size_t extent;
#endif

#ifdef NO_TIME_T
   typedef long time_t;
#endif

/* DBCS support for Info-ZIP's zip  (mainly for japanese (-: )
 * by Yoshioka Tsuneo (QWF00133@nifty.ne.jp,tsuneo-y@is.aist-nara.ac.jp)
 * This code is public domain!   Date: 1998/12/20
 */

/* 2007-07-29 SMS.
 * Include <locale.h> here if it will be needed later for Unicode.
 * Otherwise, SETLOCALE may be defined here, and then defined again
 * (differently) when <locale.h> is read later.
 */
#ifdef UNICODE_SUPPORT
# ifdef UNICODE_WCHAR
#  include <stdlib.h>
/* wchar support may be in any of these three headers */
#  ifdef HAVE_CTYPE_H
#   include <ctype.h>
#  endif
#  ifdef HAVE_WCHAR_H
#   include <wchar.h>
#  endif
#  ifdef HAVE_WCTYPE_H
#   include <wctype.h>
#  endif
# endif /* UNICODE_WCHAR */

/* Desperate attempt to deal with a missing iswprint() (SunOS 4.1.4). */
/* Should not get here if unix/configure doesn't find iswprint() */
# ifdef NO_ISWPRINT
#  define iswprint(x) isprint(x)
# endif
#endif /* UNICODE_SUPPORT */

#ifdef HAVE_LANGINFO_H
# include <langinfo.h>
#endif

#ifndef _MBCS  /* no need to include <locale.h> twice, see below */
# ifdef HAVE_LOCALE_H
#  include <locale.h>
# endif
#endif /* ndef _MBCS */

#ifdef _MBCS
   /* Multi Byte Character Set support
      - This section supports a number of mbcs-related functions which
        will use the OS-provided version (if found by a unix/configure
        test, or equivalent) or will use a generic minimally-functional
        version provided as a utilio.c function.
      - All references to this function Zip code are via the FUNCTION
        name.
      - If unix/configure finds the OS-provided function, it will define
        a macro in the form FUNCTION=function.
      - If not defined:
        - A replacement is defined below pointing to the generic version.
        - The prototype for the generic function will be defined (below).
        - A NEED_FUNCTION macro will also be defined, to enable compile
          of the util.c code to implement the generic function.
    */
#  ifdef HAVE_LOCALE_H
#   include <locale.h>
#  endif
#  ifdef HAVE_MBSTR_H
#   include <mbstr.h>
#  endif

    extern char *___tmp_ptr;

#  ifndef CLEN
#    define CLEN(ptr) mblen((ZCONST char *)ptr, MB_CUR_MAX)
     /* UnZip defines as
#    define NEED_UZMBCLEN
#    define CLEN(ptr) (int)uzmbclen((ZCONST unsigned char *)(ptr))
      */
#  endif

#  ifndef PREINCSTR
#    define PREINCSTR(ptr) (ptr += CLEN(ptr))
#  endif

#   define POSTINCSTR(ptr) (___tmp_ptr=(char *)ptr,ptr += CLEN(ptr),___tmp_ptr)
    int lastchar OF((ZCONST char *ptr));
    /* UnZip defines as
#  define POSTINCSTR(ptr) (___TMP_PTR=(char *)(ptr), PREINCSTR(ptr),___TMP_PTR)
   char *plastchar OF((ZCONST char *ptr, extent len));
#  define lastchar(ptr, len) ((int)(unsigned)*plastchar(ptr, len))
      */

#  ifndef MBSCHR
#    define NEED_ZMBSCHR
#    define MBSCHR(str,c) (char *)zmbschr((ZCONST unsigned char *)(str), c)
#  endif

#  ifndef MBSRCHR
#    define NEED_ZMBSRCHR
#    define MBSRCHR(str,c) (char *)zmbsrchr((ZCONST unsigned char *)(str), c)
#  endif

#  ifndef SETLOCALE
#    define SETLOCALE(category, locale) setlocale(category, locale)
#  endif

#else /* !_MBCS */
   /* Multi Byte Character Set support is not available */
#  define CLEN(ptr) 1
#  define PREINCSTR(ptr) (++(ptr))
#  define POSTINCSTR(ptr) ((ptr)++)
#  define lastchar(ptr) ((*(ptr)=='\0') ? '\0' : ptr[strlen(ptr)-1])
#  define MBSCHR(str, c) strchr(str, c)
#  define MBSRCHR(str, c) strrchr(str, c)
#  ifndef SETLOCALE
#     define SETLOCALE(category, locale)
#  endif /* ndef SETLOCALE */
#endif /* ?_MBCS */

#define INCSTR(ptr) PREINCSTR(ptr)


/*---------------------------------------------------------------------------
    Functions in util.c:
  ---------------------------------------------------------------------------*/
#ifdef NEED_ZMBSCHR
   unsigned char *zmbschr  OF((ZCONST unsigned char *str, unsigned int c));
#endif
#ifdef NEED_ZMBSRCHR
   unsigned char *zmbsrchr OF((ZCONST unsigned char *str, unsigned int c));
#endif


/* System independent replacement for "struct utimbuf", which is missing
 * in many older OS environments.
 */
typedef struct ztimbuf {
    time_t actime;              /* new access time */
    time_t modtime;             /* new modification time */
} ztimbuf;

/* This macro round a time_t value to the OS specific resolution */
#ifndef ROUNDED_TIME
#  define ROUNDED_TIME(time)   (time)
#endif

/* Some systems define S_IFLNK but do not support symbolic links */
#if defined (SYMLINKS) && defined(NO_SYMLINKS)
#  undef SYMLINKS
/*#  undef S_IFLNK */
#endif

#if 0
/* This will likely go away soon.  The API no long uses this, now
   using API_FILESIZE_T instead, defined in zip.h to uzoff_t.  But
   there may be another need for 64-bit quantities when the uses of
   uzoff_t and zoff_t are clarified later.  So it stays for now. */

/* long long */
/* These must be 64-bit.  Any platform using api.h must define these. */
/* A platform without long long should define it as something
   else in its osdep.h. */
/* Would have used LONGLONG and ULONGLONG, but these are already
   defined on Windows (in winnt.h), which is awkward for other ports
   and creates a redefining error on Windows because of include order. */
#ifndef Z_LONGLONG
# define Z_LONGLONG long long
#endif
#ifndef UZ_LONGLONG
# define UZ_LONGLONG unsigned long long
#endif
#endif

/*---------------------------------------------------------------------*/

/* uint4 for crypt.c */
#ifndef Z_UINT4_DEFINED
#  if !defined(NO_LIMITS_H)
#    if (defined(UINT_MAX) && (UINT_MAX == 0xffffffff))
       typedef unsigned int     z_uint4;
#      define Z_UINT4_DEFINED
#    else
#      if (defined(ULONG_MAX) && (ULONG_MAX == 0xffffffff))
         typedef unsigned long    z_uint4;
#        define Z_UINT4_DEFINED
#      else
#        if (defined(USHRT_MAX) && (USHRT_MAX == 0xffffffff))
           typedef unsigned short   z_uint4;
#          define Z_UINT4_DEFINED
#        endif
#      endif
#    endif
#  endif /* !defined(NO_LIMITS_H) */
#endif /* ndef Z_UINT4_DEFINED */
#ifndef Z_UINT4_DEFINED
  typedef ulg                z_uint4;
# define Z_UINT4_DEFINED
#endif

#ifndef FOPR    /* fallback default definitions for FOPR, FOPM, FOPW: */
#  define FOPR "r"
#  define FOPM "r+"
#  define FOPW "w"
#endif /* fallback definition */

#ifndef FOPW_TMP    /* fallback default for opening writable temp files */
#  define FOPW_TMP FOPW
#endif

/* Open the old zip file in exclusive mode if possible (to avoid adding
 * zip file to itself).
 */
#ifdef OS2
#  define FOPR_EX FOPM
#else
#  define FOPR_EX FOPR
#endif


/* MSDOS file or directory attributes */
#define MSDOS_HIDDEN_ATTR 0x02
#define MSDOS_DIR_ATTR 0x10


/* Define this symbol if your target allows access to unaligned data.
 * This is not mandatory, just a speed optimization. The compressed
 * output is strictly identical.
 */
#if (defined(MSDOS) && !defined(WIN32)) || defined(i386)
#    define UNALIGNED_OK
#endif
#if defined(mc68020) || defined(vax)
#    define UNALIGNED_OK
#endif

#if (defined(SMALL_MEM) && !defined(CBSZ))
#   define CBSZ 2048 /* buffer size for copying files */
#   define ZBSZ 2048 /* buffer size for temporary zip file */
#endif

#if (defined(MEDIUM_MEM) && !defined(CBSZ))
#  define CBSZ 8192
#  define ZBSZ 8192
#endif

#ifndef CBSZ
#  define CBSZ 16384
#  define ZBSZ 16384
#endif

#ifndef SBSZ
#  define SBSZ CBSZ     /* copy buf size for STORED entries, see zipup() */
#endif

#ifndef MEMORY16
#  ifdef __WATCOMC__
#    undef huge
#    undef far
#    undef near
#  endif
#  ifdef THEOS
#    undef far
#    undef near
#  endif
#  if (!defined(__IBMC__) || !defined(OS2))
#    ifndef huge
#      define huge
#    endif
#    ifndef far
#      define far
#    endif
#    ifndef near
#      define near
#    endif
#  endif
#  define nearmalloc malloc
#  define nearfree free
#  define farmalloc malloc
#  define farfree free
#endif /* !MEMORY16 */

#ifndef Far
#  define Far far
#endif

/* MMAP and BIG_MEM cannot be used together -> let MMAP take precedence */
#if (defined(MMAP) && defined(BIG_MEM))
#  undef BIG_MEM
#endif

#if (defined(BIG_MEM) || defined(MMAP)) && !defined(DYN_ALLOC)
#   define DYN_ALLOC
#endif


/* LARGE_FILE_SUPPORT
 *
 * Types are in osdep.h for each port
 *
 * LARGE_FILE_SUPPORT and ZIP64_SUPPORT are automatically
 * set in osdep.h (for some ports) based on the port and compiler.
 *
 * Function prototypes are below as OF is defined earlier in this file
 * but after osdep.h is included.  In the future ANSI prototype
 * support may be required and the OF define may then go away allowing
 * the function defines to be in the port osdep.h.
 *
 * E. Gordon 9/21/2003
 * Updated 7/24/04 EG
 */
#ifdef LARGE_FILE_SUPPORT
  /* 64-bit Large File Support */

  /* Arguments for all functions are assumed to match the actual
     arguments of the various port calls.  As such only the
     function names are mapped below. */

/* ---------------------------- */
/*
    RBW  --  2009/06/12  --  z/OS needs these 64-bit functions
    defined for either the Unix or the MVS build.
*/
# if defined(UNIX) || defined(ZOS)

  /* Assume 64-bit file environment is defined.  The below should all
     be set to their 64-bit versions automatically.  Neat.  7/20/2004 EG */

    /* 64-bit stat functions */
#   define zstat stat
#   define zfstat fstat
#   define zlstat lstat

# if defined(__alpha) && defined(__osf__)  /* support for osf4.0f */
    /* 64-bit fseek */
#   define zfseeko fseek

    /* 64-bit ftell */
#   define zftello ftell

# else
     /* 64-bit fseeko */
#   define zfseeko fseeko

     /* 64-bit ftello */
#   define zftello ftello
# endif                                    /* __alpha && __osf__ */

    /* 64-bit fopen */
#   define zfopen fopen
#   define zfdopen fdopen

# endif /* UNIX */


/* ---------------------------- */
# ifdef VMS

    /* 64-bit stat functions */
#   define zstat stat
#   define zfstat fstat
#   define zlstat lstat

    /* 64-bit fseeko */
#   define zfseeko fseeko

    /* 64-bit ftello */
#   define zftello ftello

    /* 64-bit fopen */
#   define zfopen fopen
#   define zfdopen fdopen

# endif /* def VMS */


/* ---------------------------- */
# ifdef WIN32

#   if defined(__MINGW32__)
    /* GNU C, linked against "msvcrt.dll" */

      /* 64-bit stat functions */
#     define zstat _stati64
# ifdef UNICODE_SUPPORT_WIN32
#     define zwfstat _fstati64
#     define zwstat _wstati64
#     define zw_stat struct _stati64
# endif
#     define zfstat _fstati64
#     define zlstat lstat

      /* 64-bit fseeko */
      /* function in win32.c */
      int zfseeko OF((FILE *, zoff_t, int));

      /* 64-bit ftello */
      /* function in win32.c */
      zoff_t zftello OF((FILE *));

      /* 64-bit fopen */
#     define zfopen fopen
#     define zfdopen fdopen

#   endif

#   if defined(__CYGWIN__)
    /* GNU C, CygWin with its own POSIX compatible runtime library */

      /* 64-bit stat functions */
#     define zstat stat
#     define zfstat fstat
#     define zlstat lstat

      /* 64-bit fseeko */
#     define zfseeko fseeko

      /* 64-bit ftello */
#     define zftello ftello

      /* 64-bit fopen */
#     define zfopen fopen
#     define zfdopen fdopen

#   endif

#   ifdef __WATCOMC__
    /* WATCOM C */

      /* 64-bit stat functions */
#     define zstat _stati64
# ifdef UNICODE_SUPPORT_WIN32
#     define zwfstat _fstati64
#     define zwstat _wstati64
#     define zw_stat struct _stati64
# endif
#     define zfstat _fstati64
#     define zlstat lstat

      /* 64-bit fseeko */
      /* function in win32.c */
      int zfseeko OF((FILE *, zoff_t, int));

      /* 64-bit ftello */
      /* function in win32.c */
      zoff_t zftello OF((FILE *));

      /* 64-bit fopen */
#     define zfopen fopen
#     define zfdopen fdopen

#   endif

#   ifdef _MSC_VER
    /* MS C and VC */

      /* 64-bit stat functions */
#     define zstat _stati64
# ifdef UNICODE_SUPPORT_WIN32
#     define zwfstat _fstati64
#     define zwstat _wstati64
#     define zw_stat struct _stati64
# endif
#     define zfstat _fstati64
#     define zlstat stat /* there is no Windows lstat */

#     if (_MSC_VER >= 1400)
        /* Beginning with VS 8.0 (Visual Studio 2005, MSC 14), the Microsoft
           C rtl publishes its (previously internal) implmentations of
           "fseeko" and "ftello" for 64-bit file offsets. */
        /* 64-bit fseeko */
#       define zfseeko _fseeki64

        /* 64-bit ftello */
#       define zftello _ftelli64

#     else /* MSC rtl library version < 1400 */

        /* The newest MinGW port contains built-in extensions to the MSC rtl
           that provide fseeko and ftello, but our implementations will do
           for now. */
        /* 64-bit fseeko */
        /* function in win32.c */
        int zfseeko OF((FILE *, zoff_t, int));

        /* 64-bit ftello */
        /* function in win32.c */
        zoff_t zftello OF((FILE *));

#     endif /* ? (_MSC_VER >= 1400) */

      /* 64-bit, UTF-8-capable fopen */
#     ifdef UNICODE_SUPPORT_WIN32
#      define zfopen fopen_utf8
#     else
#      define zfopen fopen
#     endif

      /* changed from fdopen() to _fdopen() - 2014-06-25 EG */
#     define zfdopen _fdopen

#   endif

#   ifdef __IBMC__
      /* IBM C */

      /* 64-bit stat functions */

      /* 64-bit fseeko */
      /* function in win32.c */
      int zfseeko OF((FILE *, zoff_t, int));

      /* 64-bit ftello */
      /* function in win32.c */
      zoff_t zftello OF((FILE *));

      /* 64-bit fopen */

#   endif

# endif /* WIN32 */


#else
  /* No Large File Support or default for 64-bit environment */

# define zstat stat
# define zfstat fstat
# define zlstat lstat
# define zfseeko fseek
# define zftello ftell
# define zfopen fopen
# define zfdopen fdopen
# ifdef UNICODE_SUPPORT
#   define zwfstat _fstat
#   define zwstat _wstat
#   define zw_stat struct _stat
# endif

#endif

#ifdef LARGE_FILE_SUPPORT         /* E. Gordon 9/12/2003 */

# ifndef SSTAT
#  define SSTAT      zstat
#  ifdef UNICODE_SUPPORT
#    define SSTATW   zwstat
#  endif
# endif
# ifdef SYMLINKS
#  define LSTAT      zlstat
#  define LSSTAT(n, s)  (linkput ? zlstat((n), (s)) : SSTAT((n), (s)))
# else
#  define LSTAT      SSTAT
#  define LSSTAT     SSTAT
#  ifdef UNICODE_SUPPORT
#    define LSSTATW  SSTATW
#  endif
# endif

#else /* no LARGE_FILE_SUPPORT */

# ifndef SSTAT
#  define SSTAT      stat
# endif
# ifdef SYMLINKS
#  define LSTAT      lstat
#  define LSSTAT(n, s)  (linkput ? lstat((n), (s)) : SSTAT((n), (s)))
# else
#  define LSTAT      SSTAT
#  define LSSTAT     SSTAT
#  ifdef UNICODE_SUPPORT
#    define LSSTATW  SSTATW
#  endif
# endif

#endif


/* change directory (-cd) */
/* Only ports that support saving and changing directories should be
   defined here.  This is implemented in zip.c.

   To add a port, define CHANGE_DIRECTORY and map GETCWD and CHDIR to
   the appropriate functions. 2014-06-25 EG */
#ifndef NO_CHANGE_DIRECTORY
# ifndef CHANGE_DIRECTORY

#  ifdef WIN32
#   define CHANGE_DIRECTORY                             /* Enables -cd */
#   define CHDIR              _chdir
#   define GETCWD             _getcwd
#  endif

#  ifdef UNIX
#   define CHANGE_DIRECTORY
#   define CHDIR              chdir
#   define GETCWD             getcwd
#  endif

#  ifdef VMS
#   define CHANGE_DIRECTORY
#   define CHDIR              chdir                     /* Non-permanent. */
#   define GETCWD( buf, siz)  vms_getcwd( buf, siz, 1)  /* 1: VMS format. */
#  endif

# endif
#endif


/*---------------------------------------------------------------------*/


/* 2004-12-01 SMS.
 * Added fancy zofft() macros, et c.
 */

/* Default fzofft() format selection.
 * Modified 2004-12-27 EG
 */

#ifndef FZOFFT_FMT
# define FZOFFT_FMT      ZOFF_T_FORMAT_SIZE_PREFIX /* printf for zoff_t values */

# ifdef LARGE_FILE_SUPPORT
#   define FZOFFT_HEX_WID_VALUE     "16"  /* width of 64-bit hex values */
# else
#   define FZOFFT_HEX_WID_VALUE     "8"   /* digits in 32-bit hex values */
# endif

#endif /* ndef FZOFFT_FMT */

#define FZOFFT_HEX_WID ((char *) -1)
#define FZOFFT_HEX_DOT_WID ((char *) -2)




/* The following default definition of the second input for the crypthead()
 * random seed computation can be used on most systems (all those that
 * supply a UNIX compatible getpid() function).
 */
#ifdef ZCRYPT_INTERNAL
#  ifndef ZCR_SEED2
#    define ZCR_SEED2     (unsigned) getpid()   /* use PID as seed pattern */
#  endif
#endif /* ZCRYPT_INTERNAL */

/* The following OS codes are defined in pkzip appnote.txt */
#ifdef AMIGA
#  define OS_CODE  0x100
#endif
#ifdef VMS
#  define OS_CODE  0x200
#endif
/* unix    3 */
#ifdef VM_CMS
#  define OS_CODE  0x400
#endif
#ifdef ATARI
#  define OS_CODE  0x500
#endif
#ifdef OS2
#  define OS_CODE  0x600
#endif
#ifdef MACOS
#  define OS_CODE  0x700
#endif
/* z system 8 */
/* cp/m     9 */
#ifdef TOPS20
#  define OS_CODE  0xa00
#endif
#ifdef WIN32
#  define OS_CODE  0xb00
#endif
#ifdef QDOS
#  define OS_CODE  0xc00
#endif
#ifdef RISCOS
#  define OS_CODE  0xd00
#endif
#ifdef VFAT
#  define OS_CODE  0xe00
#endif
#ifdef MVS
#  define OS_CODE  0xf00
#endif
#ifdef __BEOS__
#  define OS_CODE  0x1000
#endif
#ifdef TANDEM
#  define OS_CODE  0x1100
#endif
#ifdef THEOS
#  define OS_CODE  0x1200
#endif
/* Yes, there is a gap here. */
#ifdef __ATHEOS__
#  define OS_CODE  0x1E00
#endif

#define NUM_HOSTS 31
/* Number of operating systems. Should be updated when new ports are made */

#if defined(DOS) && !defined(OS_CODE)
#  define OS_CODE  0x000
#endif

#ifndef OS_CODE
#  define OS_CODE  0x300  /* assume Unix */
#endif

/* can't use "return 0" from main() on VMS */
#ifndef EXIT
#  define EXIT  exit
#endif
#ifndef RETURN
#  define RETURN return
#endif

#ifndef ZIPERR
#  define ZIPERR ziperr
#endif

#if (defined(USE_ZLIB) && defined(MY_ZCALLOC))
   /* special zcalloc function is not needed when linked against zlib */
#  undef MY_ZCALLOC
#endif

#if (!defined(USE_ZLIB) && !defined(MY_ZCALLOC))
   /* Any system without a special calloc function */
#  define zcalloc(items,size) \
          (zvoid far *)calloc((unsigned)(items), (unsigned)(size))
#  define zcfree    free
#endif /* !USE_ZLIB && !MY_ZCALLOC */

/* end of tailor.h */
