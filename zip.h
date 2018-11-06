/*
  zip.h - Zip 3.1

/---------------------------------------------------------------------/

Info-ZIP License

This is version 2009-Jan-02 of the Info-ZIP license.
The definitive version of this document should be available at
ftp://ftp.info-zip.org/pub/infozip/license.html indefinitely and
a copy at http://www.info-zip.org/pub/infozip/license.html.


Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

For the purposes of this copyright and license, "Info-ZIP" is defined as
the following set of individuals:

   Mark Adler, John Bush, Karl Davis, Harald Denker, Jean-Michel Dubois,
   Jean-loup Gailly, Hunter Goatley, Ed Gordon, Ian Gorman, Chris Herborth,
   Dirk Haase, Greg Hartwig, Robert Heath, Jonathan Hudson, Paul Kienitz,
   David Kirschbaum, Johnny Lee, Onno van der Linden, Igor Mandrichenko,
   Steve P. Miller, Sergio Monesi, Keith Owens, George Petrov, Greg Roelofs,
   Kai Uwe Rommel, Steve Salisbury, Dave Smith, Steven M. Schweda,
   Christian Spieler, Cosmin Truta, Antoine Verheijen, Paul von Behren,
   Rich Wales, Mike White.

This software is provided "as is," without warranty of any kind, express
or implied.  In no event shall Info-ZIP or its contributors be held liable
for any direct, indirect, incidental, special or consequential damages
arising out of the use of or inability to use this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the above disclaimer and the following restrictions:

    1. Redistributions of source code (in whole or in part) must retain
       the above copyright notice, definition, disclaimer, and this list
       of conditions.

    2. Redistributions in binary form (compiled executables and libraries)
       must reproduce the above copyright notice, definition, disclaimer,
       and this list of conditions in documentation and/or other materials
       provided with the distribution.  Additional documentation is not needed
       for executables where a command line license option provides these and
       a note regarding this option is in the executable's startup banner.  The
       sole exception to this condition is redistribution of a standard
       UnZipSFX binary (including SFXWiz) as part of a self-extracting archive;
       that is permitted without inclusion of this license, as long as the
       normal SFX banner has not been removed from the binary or disabled.

    3. Altered versions--including, but not limited to, ports to new operating
       systems, existing ports with new graphical interfaces, versions with
       modified or added functionality, and dynamic, shared, or static library
       versions not from Info-ZIP--must be plainly marked as such and must not
       be misrepresented as being the original source or, if binaries,
       compiled from the original source.  Such altered versions also must not
       be misrepresented as being Info-ZIP releases--including, but not
       limited to, labeling of the altered versions with the names "Info-ZIP"
       (or any variation thereof, including, but not limited to, different
       capitalizations), "Pocket UnZip," "WiZ" or "MacZip" without the
       explicit permission of Info-ZIP.  Such altered versions are further
       prohibited from misrepresentative use of the Zip-Bugs or Info-ZIP
       e-mail addresses or the Info-ZIP URL(s), such as to imply Info-ZIP
       will provide support for the altered versions.

    4. Info-ZIP retains the right to use the names "Info-ZIP," "Zip," "UnZip,"
       "UnZipSFX," "WiZ," "Pocket UnZip," "Pocket Zip," and "MacZip" for its
       own source and binary releases.

/---------------------------------------------------------------------/

*/

/*
 *  zip.h by Mark Adler
 */
#ifndef __zip_h
#define __zip_h 1

#define ZIP   /* for crypt.c:  include zip password functions, not unzip */

/* General-purpose MAX and MIN macros. */
#  define IZ_MAX( a, b) (((a) > (b)) ? (a) : (b))
#  define IZ_MIN( a, b) (((a) < (b)) ? (a) : (b))

/* Types centralized here for easy modification */
#define local static            /* More meaningful outside functions */
typedef unsigned char uch;      /* unsigned 8-bit value */
typedef unsigned short ush;     /* unsigned 16-bit value */
typedef unsigned long ulg;      /* unsigned 32-bit value */

/* This little macro compiles (with no code) if condition is false, but fails
 * to compile if condition is true (non-zero).  Can be used to abort a compile
 * if some condition is met.  For example:
 *   BUILD_BUG_ON(sizeof(struct mystruct) != 8)
 * will not compile if the size of mystruct is not 8.
 * (from http://scaryreasoner.wordpress.com/2009/02/28/checking-sizeof-at-compile-time/)
 */
#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

/* control.h enables and disables AES encryption and the additional compression
   methods.  Currently only used for Windows.  See the comments in that file
   for more information. */
#include "control.h"

/* Set up portability */
#include "tailor.h"

#ifdef USE_ZLIB
#  include "zlib.h"
#endif

#if defined(UNIX) && defined(__APPLE__)
# define UNIX_APPLE
#endif

/* Enable support for new Streaming Attributes/Comments ef (2014-04-07 EG) */
#ifndef STREAM_EF_SUPPORT
# ifndef NO_STREAM_EF_SUPPORT
#  define STREAM_EF_SUPPORT
# endif
#endif

/* Enable support for the Backup options by default for Win32, Unix, and VMS.
   Other ports should only enable if they can support the long paths generated
   for the date-stamped backups, or changes to support shorter paths are made. */
#ifndef BACKUP_SUPPORT
# ifndef NO_BACKUP_SUPPORT
#  if defined(UNIX) || defined(WIN32) || defined(VMS)
#   define BACKUP_SUPPORT
#  endif
# endif
#endif

/* Enable cd_only compression (2014-09-22). */
#ifndef NO_CD_ONLY_SUPPORT
# ifndef CD_ONLY_SUPPORT
#  define CD_ONLY_SUPPORT
# endif
#endif

/* FIFO support */
#ifndef NO_FIFO_SUPPORT
/* Currently FIFOs (named pipes) only supported on Unix */
# ifdef UNIX
#  ifndef FIFO_SUPPORT
#   define FIFO_SUPPORT
#  endif
# endif
#endif

/* Unix directory scan sort */
#ifndef NO_UNIX_DIR_SCAN_SORT
# ifdef UNIX
#  ifndef UNIX_DIR_SCAN_SORT
#   define UNIX_DIR_SCAN_SORT
#  endif
# endif
#endif

/* MAC OS X (Unix Apple) directory scan sort */
/* This sorts "._" AppleDouble files after matching
   primary files.  On Unix Apple, this enables
   sorting of directory scans.  On other platforms,
   it allows use of --apple-double to enable this
   sorting, generally for creating a ditto compatible
   archive on another platform that can be then
   moved to a Unix Apple platform. */
#ifndef NO_UNIX_APPLE_SORT
# ifndef UNIX_APPLE_SORT
#  define UNIX_APPLE_SORT
#  ifdef UNIX_APPLE
#   ifndef UNIX_DIR_SCAN_SORT
#    define UNIX_DIR_SCAN_SORT
#   endif
#  endif
# endif
#endif

/* Enable symlink support. */
/* (The variable linkput is used to actually enable symlink handling.) */
#if !defined(NO_SYMLINKS) && !defined(SYMLINKS)

  /* Not Windows. */
# if defined(S_IFLNK) && !defined(WIN32)
#  define SYMLINKS
# endif

/* Windows. */
# if defined(NTSD_EAS)
    /* For now do not do symlinks on WIN32 in utilities as code is not there.  Also
       need Unicode enabled, which it should be by default on NT and later. */
#  if defined(UNICODE_SUPPORT)
#   if !defined(UTIL)
     /* Need at least Windows Vista for Symlink support */
#    ifdef AT_LEAST_WINVISTA
#     ifndef SYMLINKS
#      define SYMLINKS
#     endif
#     ifndef WINDOWS_SYMLINKS
#      define WINDOWS_SYMLINKS
#     endif
#    endif
#   endif
#  endif
# endif

#endif /* !defined(NO_SYMLINKS) && !defined(SYMLINKS) */

#ifdef WIN32
/* For Windows we need to define S_ISLNK as Windows does not define it (or

   use it), but we need it. */
# ifdef SYMLINKS
#  ifndef S_ISLNK
#   ifndef S_IFLNK
#    define S_IFLNK  0120000
#   endif
#   define S_ISLNK(m) (((m)& S_IFMT) == S_IFLNK)
#  endif /* ndef S_ISLNK */
# endif
#endif /* WIN32 */


/* ---------- */
/* Encryption */

/* Encryption rules.
 *
 * By default, in normal Zip, enable Traditional, disable AES_WG.
 *
 * User-specified macros:
 * NO_CRYPT, NO_CRYPT_TRAD, CRYPT_AES_WG, NO_CRYPT_AES_WG.
 *
 *   NO_CRYPT disables all.
 *
 *   NO_CRYPT_TRAD disables Traditional (ZipCrypto) encryption.
 *
 *   CRYPT_AES_WG enables AES_WG.  This is now the default if
 *     the iz_aes_wg.zip AES WG source kit has been expanded in
 *     the aes_wg/ directory.
 *
 *   NO_CRYPT_AES_WG disables AES_WG.  This is the default if the
 *     AES WG source kit has not been added to aes_wg/.  If that
 *     kit has been added, this disables AES WG encryption.
 *
 * Macros used in code: IZ_CRYPT_AES_WG, IZ_CRYPT_ANY, IZ_CRYPT_TRAD,
 * ETWODD_SUPPORT.
 *
 * CRYPT_AES_WG_NEW enables AES_WG encryption using newer code,
 * but is no where near completed.  Doesn't work.  Don't enable.
 */
#ifdef NO_CRYPT
   /* Disable all encryption. */
# if 0
/* Disabling these here makes no sense as they could not have been
   set yet. */
#  undef IZ_CRYPT_AES_WG
#  undef IZ_CRYPT_AES_WG_NEW
#  undef IZ_CRYPT_TRAD
# endif

# ifdef IZ_CRYPT_TRAD
#  undef IZ_CRYPT_TRAD
# endif

# ifdef CRYPT_AES_WG
#   undef CRYPT_AES_WG
#   ifndef NO_CRYPT_AES_WG
#    define NO_CRYPT_AES_WG
#   endif
# endif

#else /* not def NO_CRYPT */

/* Allow encryption to be enabled. */
# ifdef NO_CRYPT_TRAD
    /* Disable Traditional encryption. */
    /* There should be no way this was set. */
#  ifdef IZ_CRYPT_TRAD
#   undef IZ_CRYPT_TRAD
#  endif

# else /* def NO_CRYPT_TRAD */
    /* Enable Traditional encryption. */
#  define IZ_CRYPT_TRAD 1
# endif /* def NO_CRYPT_TRAD [else] */

#endif /* def NO_CRYPT [else] */

#if defined(ETWODD_SUPPORT) && !defined(IZ_CRYPT_TRAD)
  /* Disable ETWODD if no Traditional encryption. */
# undef ETWODD_SUPPORT
#endif

#ifdef CRYPT_AES_WG
#  define IZ_CRYPT_AES_WG 1
#  include "aes_wg/aes.h"
#  include "aes_wg/fileenc.h"
#  include "aes_wg/prng.h"
#endif

#ifdef CRYPT_AES_WG_NEW
#  define IZ_CRYPT_AES_WG_NEW 1
#  include "aesnew/ccm.h"
#endif

/* ---------- */


/* CRC32() is now used by zipbare(). */
#if 0
/* In the utilities, the crc32() function is only used for UNICODE_SUPPORT. */
#if defined(UTIL) && !defined(UNICODE_SUPPORT)
#  define CRC_TABLE_ONLY
#endif
#endif

#define MIN_MATCH  3
#define MAX_MATCH  258
/* The minimum and maximum match lengths */

#ifndef WSIZE
#  define WSIZE  (0x8000)
#endif
/* Maximum window size = 32K. If you are really short of memory, compile
 * with a smaller WSIZE but this reduces the compression ratio for files
 * of size > WSIZE. WSIZE must be a power of two in the current implementation.
 *
 * If assembly is used, this WSIZE must match the WSIZE there.
 */

#define MIN_LOOKAHEAD (MAX_MATCH+MIN_MATCH+1)
/* Minimum amount of lookahead, except at the end of the input file.
 * See deflate.c for comments about the MIN_MATCH+1.
 */

#define MAX_DIST  (WSIZE-MIN_LOOKAHEAD)
/* In order to simplify the code, particularly on 16 bit machines, match
 * distances are limited to MAX_DIST instead of WSIZE.
 */

/* Forget FILENAME_MAX (incorrectly = 14 on some System V) */
#ifdef DOS
#  define FNMAX 256
#else
#  define FNMAX 1024
#endif

#ifndef MATCH
#  define MATCH shmatch         /* Default for pattern matching: UNIX style */
#endif


/* Structure carrying extended timestamp information */
typedef struct iztimes {
   time_t atime;                /* new access time */
   time_t mtime;                /* new modification time */
   time_t ctime;                /* new creation time (!= Unix st.ctime) */
} iztimes;

/* Lengths of headers after signatures in bytes */
#define LOCHEAD 26
#define CENHEAD 42
#define ENDHEAD 18
#define EC64LOC 16
#define EC64REC 52


/* --------------------- */
/* zflags - used by flist, zlist and newname() */

/* zflags bit masks. */
#define ZFLAG_DIR    0x00000001
#define ZFLAG_APLDBL 0x00000002
#define ZFLAG_FIFO   0x00000004

/* zflags bit tests. */
#define IS_ZFLAG_DIR(zflags)    ((zflags) & ZFLAG_DIR)
#define IS_ZFLAG_APLDBL(zflags) ((zflags) & ZFLAG_APLDBL)
#define IS_ZFLAG_FIFO(zflags)   ((zflags) & ZFLAG_FIFO)


/* ---------------------------------- */
/* General Purpose Bit Flag (2 bytes) */

#define GPBF_00_ENCRYPTED       0x0001

/* Bits 1 and 2:
      Method 6 - Imploding:
        Bit 1: 8K sliding dictionary.  If clear, 4K.
        Bit 2: 3 Shannon-Fano trees used.  If clear, then 2.
      Methods 8 and 9 - Deflating:
        Bit 2  Bit 1
          0      0    Normal (-en) compression option was used.
          0      1    Maximum (-exx/-ex) compression option was used.
          1      0    Fast (-ef) compression option was used.
          1      1    Super Fast (-es) compression option was used.
      Method 14 - LZMA:
        Bit 1: An end-of-stream (EOS) marker used to
               mark the end of the compressed data stream.
               If clear, then an EOS marker is not present
               and the compressed data size must be known
               to extract.
      Note:  Bits 1 and 2 are undefined if the compression
             method is any other.
 */
#define GPBF_01_COMPRESSION_1      0x0002
#define GPBF_02_COMPRESSION_2      0x0004

#define GPBF_03_DATA_DESCRIPTOR    0x0008
#define GPBF_04_ENH_DEFLATING      0x0010
#define GPBF_05_PATCHED_DATA       0x0020
#define GPBF_06_STRONG_ENCRYPTION  0x0040
#define GPBF_07_UNUSED             0x0080
#define GPBF_08_UNUSED             0x0100
#define GPBF_09_UNUSED             0x0200
#define GPBF_10_UNUSED             0x0400

/* Bit 11:
     Language encoding flag (EFS).  If this bit is set,
     the filename and comment fields for this file
     MUST be encoded using UTF-8.
 */
#define GPBF_11_UTF8               0x0800

/* Bit 12 reserved by PKWARE. */
#define GPBF_12_ENH_COMPRESSION    0x1000

/* Bit 13:
     Set when encrypting the Central Directory to indicate 
     selected data values in the Local Header are masked to
     hide their actual values.
 */
#define GPBF_13_LH_MASKED          0x2000

#define GPBF_14_RESERVED           0x4000
#define GPBF_15_RESERVED           0x8000

/* ---------------------------------- */

/* Internal File Attributes (2 bytes) */

/* Bit 0:
     Lowest bit of this field indicates, if set, that
     file is apparently an ASCII or text file.  If not
     set, that the file apparently contains binary data.

     The remaining bits are unused in version 1.0.
 */
#define IATTR_00_TEXT_FILE           0x0001

/* Bit 1:
     4 byte variable record length control field precedes each 
     logical record indicating the length of the record. The 
     record length control field is stored in little-endian byte
     order.  This flag is independent of text control characters, 
     and if used in conjunction with text data, includes any 
     control characters in the total length of the record. This 
     value is provided for mainframe data transfer support.
 */
#define IATTR_01_REC_CONTROL_FIELDS  0x0002

/* Bit 2:
     PKWare verification bit, bit 2 (0x0004) of internal attributes.
     If this is set, then a verification checksum is in the first 3
     bytes of the external attributes.  In this case all we can use
     for setting file attributes is the last external attributes byte.

     The above from a comment in UnZip.  This seems an undocumented
     feature and apparently only applies to MSDOS/Windows.
 */
#define IATTR_02_PKWARE_VERIF        0x0004



/* ------------- */
/* Things to set */
/* ------------- */

/* ---------------------------------- */

/* Binary Detection */

#if 1
# define ALLOW_TEXT_BIN_RESTART
#endif
/* When a file is labeled as text and later determined to include
   binary, if this is defined, scrap the entry and redo it as
   binary.  This happens in zipup.c.  The output must be seekable
   and other conditions must be met (such as output is not split).
   Seems to work so this is now enabled by default. */

/* ---------------------------------- */

/* PIPES and STDIN */

# define STDIN_PIPE_PERMS 0600
/* The default permission given files that are read from stdin.  This is
   currently only used by unix.c.  Undefine this to allow files created
   from stdin to inherit permissions from the OS (typically 0600 on Linux). */

/* ---------------------------------- */

/* GENERAL LIMITS */

#define MAX_COM_LEN (32766)
/* The entire record must be less than 65535, so the comment length needs to
   be kept well below that.  Changed 65534L to 32766L. */

#define MAX_PASSWORD_LEN (1024)
/* This is for sizing storage for the returned password.  The max size for
   TRADITIONAL is about 256 and for AES about 128. */

#define MAX_ZIP_ARG_SIZE (8192)
/* Max size of command line arguments to Zip.  Used by API. */

#define MAX_PATH_SIZE (32768)
/* Max size (in bytes) of a path.  This is used for creating storage.  Usually
   FNMAX or MAX_PATH (or similar) is the limiting factor.  Also used by
   getnam(). */

#define MAX_EXTRA_FIELD_BLOCK_SIZE (65535)
/* Max size of extra field block. */

#define ERRBUF_SIZE (FNMAX + 65536)
/* Size of errbuf[] error message buffer.  Also used for some similar
   buffers.  Should be big enough to hold the message and a couple paths.
   Was FNMAX+4081.  Long paths on Windows can be up to 32k. */


/* INCLUDE FILES and ARGUMENT FILES */

#define MAX_INCLUDE_FILE_SIZE (100 * 1<<20) /* 100 MiB */
/* Maximum size of (UTF-8) include file that is allowed.  Even 100 MiB is
   starting to show significant delay with the current UTF-8 detection
   algorithm.  Include files are the files read by -@@, -x@ and -i@ as well
   as argfiles. */

/* for insert_args_from_file() */
#define ADD_ARGS_DELTA  100
/* How far to extend newargs when needed. */

#define MAX_ARGFILE_LINE  MAX_ZIP_ARG_SIZE
/* Max length of line in arg file. */

#define MAX_ARGFILE_DEPTH  4
/* Max levels of recursion supported.  4 allows @a to call @b to call @c to
   call @d.  If argfile d tries to open an argfile, an error is issued.  A
   depth limit is needed to prevent recursion loops (similar to how a limit
   is used to prevent symlink loops). */


/* PATH PREFIXES */

#define ALLOWED_PREFIX_CHARS "!@#$%^&()-_=+/[]{}|."
/* These are the characters allowed in -pa and -pp prefixes, in addition to
   alphanumeric.  This list should be kept small as these characters end up
   in paths stored in the archive.  Only include characters that tend to be
   allowed in file system paths on most ports.  '/' is included so that a
   prefix can put paths in a directory. */


/* --------------------------------------- */

/* Zip64 */

/* Define this to use the Zip64 Placeholder extra field.  This reserves space
 * for the Zip64 extra field when Zip64 is not needed but might be, based
 * on exceeding the below Zip64 thresholds.  If later Zip64 is actually
 * needed, we can switch to Zip64.  If this is not defined, Zip will exit
 * with an error if Zip64 was not planned for but later is needed.
 */

#ifndef NO_USE_ZIP64_PLACEHOLDER
# define USE_ZIP64_PLACEHOLDER
#endif

/* Zip64 thresholds - The below margins are subtracted from 4 GiB
 * (2 GiB if -l used) to get the threshold.  If an input file is at
 * least this large (but less than 4 GiB) and output is seekable,
 * use of the Zip64 ef placeholder is considered.  These values are
 * used in putlocal().
 *
 * These margins were empirically determined and may need adjustment.
 *
 * For methods that can revert to Store, the margin for that method
 * should be no smaller than the margin for Store.
 *
 * (Store has a margin because the addition of meta data grows the
 * file slightly.  Compression methods can grow files that refuse to
 * compress by quite a bit due to internal bookkeeping taking more
 * space than is recovered by compression.)
 */

#define ZIP64_MARGIN_MB_STORE              1  /* 1 MiB */
#define ZIP64_MARGIN_MB_DEFLATE            1  /* 1 MiB */
#define ZIP64_MARGIN_MB_BZIP2             50  /* 50 MiB */
#define ZIP64_MARGIN_MB_LZMA             100  /* 100 MiB */
#define ZIP64_MARGIN_MB_PPMD             150  /* 150 MiB */


/* Some shorthands */
#define KiB   (0x400)
#define MiB   (0x100000)
#define GiB   (0x40000000)


/* --------------------------------------- */

/* Testing */

#if 0
# define UNICODE_EXTRACT_TEST
#endif
/* If set, enables -sC where file paths (with no contents)
   can be extracted.  This is used to test certain features
   in Zip before UnZip has them implemented, and provides a
   check of those features when implemented in UnZip.  For
   development only! */

#if 0
# define UNICODE_TESTS_OPTION
#endif
/* If set, enables -UT that runs a set of tests to determine
   level of Unicode support available.  This feature is still
   being developed and is not ready for inclusion in the main
   executable. */


/* --------------------------------------- */

/* List of UnZip features needed to test the archive. */

/* bit 0 reserved (set when valid feature set, 0 otherwise) */
#define UNZIP_UNSHRINK_SUPPORT       0x000002  /*  1 */
#define UNZIP_DEFLATE64_SUPPORT      0x000004  /*  2 */
#define UNZIP_UNICODE_SUPPORT        0x000008  /*  3 */
#define UNZIP_WIN32_WIDE             0x000010  /*  4 */
#define UNZIP_LARGE_FILE_SUPPORT     0x000020  /*  5 */
#define UNZIP_ZIP64_SUPPORT          0x000040  /*  6 */
#define UNZIP_BZIP2_SUPPORT          0x000080  /*  7 */
#define UNZIP_LZMA_SUPPORT           0x000100  /*  8 */
#define UNZIP_PPMD_SUPPORT           0x000200  /*  9 */
#define UNZIP_IZ_CRYPT_TRAD          0x000400  /* 10 */
#define UNZIP_IZ_CRYPT_AES_WG        0x000800  /* 11 */
#define UNZIP_SPLITS_BASIC           0x001000  /* 12 */
#define UNZIP_SPLITS_MEDIA           0x002000  /* 13 */
#define UNZIP_WIN_LONG_PATH          0x004000  /* 14 */
#define UNZIP_KEYFILE                0x008000  /* 15 */

/* These are bit masks.  All unused bits must be set to 0. */

/* Set below to highest bit number used. */
#define MAX_UNZIP_FEATURE_BIT  15


/* --------------------------------------- */

/* Structures for in-memory file information */
struct zlist {
  /* See central header in zipfile.c for what vem..off are */
  /* Do not rearrange these as less than smart coding in zipfile.c
     in scanzipf_reg() depends on u being set to ver and then stepping
     through as a byte array.  Ack.  Should be fixed.  5/25/2005 EG */
  /* All the new read code does not rely on this order.  */
  /* This list has been modified significantly.  Do not use the old
     scanzipf_reg() without changes. */
  ush vem, ver, flg, how;
  ulg tim, crc;
  uzoff_t siz, len;             /* zip64 support 08/29/2003 R.Nausedat */
  /* changed from extent to ush 3/10/2005 EG */
  ush nam, ext, cext, com;      /* offset of ext must be >= LOCHEAD */
  ulg dsk;                      /* disk number was ush but now ulg */
  ush att, lflg;                /* offset of lflg must be >= LOCHEAD */
  uzoff_t off;
  ulg atx;
  char *name;                   /* File name in zip file */
  char *extra;                  /* Extra field (set only if ext != 0) */
  char *cextra;                 /* Extra in central (set only if cext != 0) */
  char *comment;                /* Comment (set only if com != 0) */
  char *iname;                  /* Internal file name after cleanup (stored in archive) */
  char *zname;                  /* External version of internal name */
  char *oname;                  /* Display version of name used in messages */
#ifdef UNICODE_SUPPORT
  /* Unicode support */
  char *uname;                  /* UTF-8 version of iname */
  /* if uname has chars not in local char set, zuname can be different than zname */
  int utf8_path;                /* Set if path derived from UTF-8 */
  char *zuname;                 /* Escaped Unicode zname from uname */
  char *ouname;                 /* Display version of zuname */
# ifdef WIN32
  char *wuname;                 /* Converted back ouname for Win32 */
  wchar_t *namew;               /* Windows wide character version of name */
  wchar_t *inamew;              /* Windows wide character version of iname */
  wchar_t *znamew;              /* Windows wide character version of zname */
# endif
#endif
  int mark;                     /* Marker for files to operate on */
  int trash;                    /* Marker for files to delete */
  int current;                  /* Marker for files current to what is on OS (filesync) */
  int dosflag;                  /* Set to force MSDOS file attributes */
  int zflags;                   /* Special flags for Zip use */
  int encrypt_method;
  ush thresh_mthd;              /* Compression method used to determine Zip64 threshold */
  struct zlist far *nxt;        /* Pointer to next header in list */
};
struct flist {
  char *name;                   /* Raw internal file name */
  char *iname;                  /* Internal file name after cleanup */
  char *zname;                  /* External version of internal name */
  char *oname;                  /* Display version of internal name */
#ifdef UNICODE_SUPPORT
  char *uname;                  /* UTF-8 name */
  int utf8_path;                /* Set if path derived from UTF-8 */
# ifdef WIN32
  wchar_t *namew;               /* Windows wide character version of name */
  wchar_t *inamew;              /* Windows wide character version of iname */
  wchar_t *znamew;              /* Windows wide character version of zname */
# endif
#endif
  int dosflag;                  /* Set to force MSDOS file attributes */
  uzoff_t usize;                /* usize from initial scan */
  struct flist far *far *lst;   /* Pointer to link pointing here */
  struct flist far *nxt;        /* Link to next name */
  int zflags;                   /* Special flags for Zip use */
#ifdef UNIX_APPLE_SORT
  char *saved_iname;
#endif
};
struct plist {
  char *zname;                  /* External version of internal name */
  char *uzname;                 /* UTF-8 version of zname */
  char *duzname;                /* De-escaped uzname (Unicode escapes converted back) */
  int select;                   /* Selection flag ('i' or 'x') */
};


/* --------------------------------------- */

/* internal file type attribute */
#define FT_UNKNOWN     (-1)
#define FT_BINARY      (0)
#define FT_ASCII_TXT   (1)
#define FT_EBCDIC_TXT  (2)


/* Extra field block ID codes (tags): */
#define EF_ACL          0x4C41   /* ACL, access control list ("AL") */
#define EF_AES_WG       0x9901   /* AES (WinZip/Gladman) encryption ("c^!") */
#define EF_AOSVS        0x5356   /* AOS/VS ("VS") */
#define EF_ATHEOS       0x7441   /* AtheOS ("At") */
#define EF_BEOS         0x6542   /* BeOS ("Be") */
#define EF_IZUNIX       0x5855   /* UNIX ("UX") */
#define EF_IZUNIX2      0x7855   /* Info-ZIP's new Unix ("Ux") */
#define EF_MVS          0x470f   /* MVS ("G")   */
#define EF_NTSD         0x4453   /* NT Security Descriptor ("SD") */
#define EF_OS2EA        0x0009   /* OS/2 (extended attributes) */
#define EF_QDOS         0xfb4a   /* SMS/QDOS ("J\373") */
#define EF_SPARK        0x4341   /* David Pilling's Acorn/SparkFS ("AC") */
#define EF_TANDEM       0x4154   /* Tandem NSK ("TA") */
#define EF_THEOS        0x6854   /* THEOS ("Th") */
#define EF_TIME         0x5455   /* Universal timestamp ("UT") */
#define EF_UTFPTH       0x7075   /* Unicode UTF-8 path ("up") */
#define EF_VMCMS        0x4704   /* VM/CMS Extra Field ID ("G")*/
#define EF_ZIP64        0x0001   /* ID for zip64 extra field */

#define EF_STREAM       0x6C78   /* Extended Local Header (Stream) ("xl") */

#define EF_PLACEHOLDER  0x4850   /* NO-OP Placeholder tag ("PH") */


/* Definitions for extra field handling: */
#define EF_SIZE_MAX  ((unsigned)0xFFFF) /* hard limit of total e.f. length */
#define EB_HEADSIZE       4     /* length of a extra field block header */
#define EB_ID             0     /* offset of block ID in header */
#define EB_LEN            2     /* offset of data length field in header */
#define EB_MEMCMPR_HSIZ   6     /* header length for memcompressed data */
#define EB_DEFLAT_EXTRA  10     /* overhead for 64kByte "undeflatable" data */

#define EB_AES_VERS       0     /* AES encryption version. */
#define EB_AES_VEND       2     /* AES encryption vendor. */
#define EB_AES_MODE       4     /* AES encryption mode. */
#define EB_AES_MTHD       5     /* AES encryption real compression method. */
#define EB_AES_HLEN       7     /* AES encryption extra block size. */

#define EB_UX_MINLEN      8     /* minimal "UX" field contains atime, mtime */
#define EB_UX_ATIME       0     /* offset of atime in "UX" extra field data */
#define EB_UX_MTIME       4     /* offset of mtime in "UX" extra field data */

#define EB_UX_FULLSIZE    12    /* full "UX" field (atime, mtime, uid, gid) */
#define EB_UX_UID         8     /* byte offset of UID in "UX" field data */
#define EB_UX_GID         10    /* byte offset of GID in "UX" field data */

#define EB_UT_MINLEN      1     /* minimal UT field contains Flags byte */
#define EB_UT_FLAGS       0     /* byte offset of Flags field */
#define EB_UT_TIME1       1     /* byte offset of 1st time value */
#define EB_UT_FL_MTIME    (1 << 0)      /* mtime present */
#define EB_UT_FL_ATIME    (1 << 1)      /* atime present */
#define EB_UT_FL_CTIME    (1 << 2)      /* ctime present */
#define EB_UT_LEN(n)      (EB_UT_MINLEN + 4 * (n))

#define EB_UX2_MINLEN     4     /* minimal Ux field contains UID/GID */
#define EB_UX2_UID        0     /* byte offset of UID in "Ux" field data */
#define EB_UX2_GID        2     /* byte offset of GID in "Ux" field data */
#define EB_UX2_VALID      (1 << 8)      /* UID/GID present */


/* ASCII definitions for line terminators in text files: */
#define LF     10        /* '\n' on ASCII machines; must be 10 due to EBCDIC */
#define CR     13        /* '\r' on ASCII machines; must be 13 due to EBCDIC */
#define CTRLZ  26        /* DOS & OS/2 EOF marker (used in fileio.c, vms.c) */


/* return codes of password fetches (negative: user abort; positive: error) */
#define IZ_PW_ENTERED   0       /* got some PWD string, use/try it */
#define IZ_PW_CANCEL    -1      /* no password available (for this entry) */
#define IZ_PW_CANCELALL -2      /* no password, skip any further PWD request */
#define IZ_PW_ERROR     5       /* = PK_MEM2 : failure (no mem, no tty, ...) */
#define IZ_PW_SKIPVERIFY IZ_PW_CANCEL   /* skip encrypt. passwd verification */

/* mode flag values of password prompting function */
#define ZP_PW_ENTER     0       /* request for encryption password */
#define ZP_PW_VERIFY    1       /* request for reentering password */


/* Error return codes and PERR macro */
#include "ziperr.h"

#if 0            /* Optimization: use the (const) result of crc32(0L,NULL,0) */
#  define CRCVAL_INITIAL  crc32(0L, (uch *)NULL, 0)
# if 00 /* not used, should be removed !! */
#  define ADLERVAL_INITIAL adler16(0U, (uch *)NULL, 0)
# endif /* 00 */
#else
#  define CRCVAL_INITIAL  0L
# if 00 /* not used, should be removed !! */
#  define ADLERVAL_INITIAL 1
# endif /* 00 */
#endif

#define DOSTIME_MINIMUM         ((ulg)0x00210000L)
#define DOSTIME_2038_01_18      ((ulg)0x74320000L)


#define MAXCOMLINE 256       /* Maximum one-line comment size */

extern int comadd;           /* 1=add comments for new files */
extern extent comment_size;  /* comment size */
extern FILE *comment_stream; /* set to stderr if anything is read from stdin */


/* Public globals */
extern uch upper[256];          /* Country dependent case map table */
extern uch lower[256];
#ifdef EBCDIC
extern ZCONST uch ascii[256];   /* EBCDIC <--> ASCII translation tables */
extern ZCONST uch ebcdic[256];
#endif /* EBCDIC */

#if (!defined(USE_ZLIB) || defined(USE_OWN_CRCTAB))
  extern ZCONST ulg near *crc_32_tab;
#else
  /* 2012-05-31 SMS.  Z_U4 is defined in ZLIB and used here to choose the
     right data type based on ZLIB version.  See note in zip.c. */
# ifdef Z_U4
  extern ZCONST z_crc_t Far *crc_32_tab;
# else /* def Z_U4 */
  extern ZCONST ulg Far *crc_32_tab;
# endif /* def Z_U4 [else] */
#endif

/* Are these ever used?  6/12/05 EG */
#ifdef IZ_ISO2OEM_ARRAY         /* ISO 8859-1 (Win CP 1252) --> OEM CP 850 */
extern ZCONST uch Far iso2oem[128];
#endif
#ifdef IZ_OEM2ISO_ARRAY         /* OEM CP 850 --> ISO 8859-1 (Win CP 1252) */
extern ZCONST uch Far oem2iso[128];
#endif

extern char errbuf[];           /* Handy place to build error messages */
extern int recurse;             /* Recurse into directories encountered */
extern int dispose;             /* Remove files after put in zip file */
extern int pathput;             /* Store path with name */

#ifdef UNIX_APPLE
extern int data_fork_only;
extern int sequester;
#endif /* UNIX_APPLE */

#ifdef RISCOS
extern int scanimage;           /* Scan through image files */
#endif

#define BEST -1                 /* Use best method (deflation or store) */
#define STORE 0                 /* Store (compression) method */
#define DEFLATE 8               /* Deflation comppression method*/
#define BZIP2 12                /* BZIP2 compression method */
#define LZMA 14                 /* LZMA compression method */
#define PPMD 98                 /* PPMd compression method */
#define AESWG 99                /* AES WinZip/Gladman encryption */
#define COPYING 9998            /* Used by z->threshold_mthd to flag using zipcopy() */
#ifdef CD_ONLY_SUPPORT
# define CD_ONLY 9999           /* Only store cd (not a real compression) */
#endif

#define LAST_KNOWN_COMPMETHOD   DEFLATE
#ifdef BZIP2_SUPPORT
# undef LAST_KNOWN_COMPMETHOD
# define LAST_KNOWN_COMPMETHOD  BZIP2
#endif
#ifdef LZMA_SUPPORT
# undef LAST_KNOWN_COMPMETHOD
# define LAST_KNOWN_COMPMETHOD  LZMA
#endif
#ifdef PPMD_SUPPORT
# undef LAST_KNOWN_COMPMETHOD
# define LAST_KNOWN_COMPMETHOD  PPMD
#endif


/* --------------------------------------------------------------
 * Store method default file name suffix list.
 * Files with these suffixes are stored (not compressed) by default.
 * The -n/--suffixes option can override this list.
 *
 * .7z      7-Zip archive
 * .arc     ARC archive
 * .arj     ARJ archive
 * .bz2     bzip2 file
 * .cab     Windows Cabinet archive
 * .gz      GNU zip (gzip) file
 * .lha     LHA archive (mainly Amiga)
 * .lzh     LHA archive (mainly Amiga)
 * .lzma    LZMA file
 * .pea     PeaZip archive
 * .rar     RAR archive
 * .rz      RZIP file (good on very large files with distance redundancy)
 * .tbz2    Tar archive with bzip2 compression
 * .tgz     Tar archive with gzip compression
 * .tlz     Tar archive with LZMA compression
 * .txz     Tar archive with XZ compression
 * .xz      XZ file (uses LZMA2 to get very high compression ratios)
 * .Z       Compress file
 * .zip     ZIP archive
 * .zipx    ZIP archive (extended format)
 * .zoo     ZOO archive
 * .zz      Zzip archive
 */

/* If you change the macros below, update the list above. */
#ifndef RISCOS
# ifndef QDOS
#  ifndef TANDEM
    /* Normal */
#   define MTHD_SUFX_0 \
 ".7z:.arc:.arj:.bz2:.cab:.gz:.lha:.lzh:.lzma:.pea:\
.rar:.rz:.tbz2:.tgz:.tlz:.txz:.xz:.Z:.zip:.zipx:.zoo:.zz"
#  else /* ndef TANDEM */
    /* Tandem */
#   define MTHD_SUFX_0 \
 " 7z: arc: arj: bz2: cab: gz: lha: lzh: lzma: pea:\
 rar: rz: tbz2: tgz: tlz: txz: xz: Z: zip: zipx: zoo: zz"
#  endif /* ndef TANDEM [else] */
# else /* ndef QDOS */
    /* QDOS */
#  define MTHD_SUFX_0 \
 "_7z:_arc:_arj:_bz2:_cab:_gz:_lha:_lzh:_lzma:_pea:\
_rar:_rz:_tbz2:_tgz:_tlz:_txz:_xz:_Z:_zip:_zipx:_zoo:_zz"
# endif /* ndef QDOS [else] */
#else /* ndef RISCOS */
    /* RISCOS */
# define MTHD_SUFX_0 "68E:D96:DDC";
#endif /* ndef RISCOS [else] */


/* -------------------------------------------------------------- */

#define AESENCRED 99            /* AES (WG) encrypted */

extern int method;              /* Restriction on compression method */

extern char *localename;        /* What setlocale() returns */
extern char *charsetname;       /* Character set name (may be what nl_langinfo() returns) */

extern ulg skip_this_disk;
extern int des_good;            /* Good data descriptor found */
extern ulg des_crc;             /* Data descriptor CRC */
extern uzoff_t des_csize;       /* Data descriptor csize */
extern uzoff_t des_usize;       /* Data descriptor usize */
extern int dosify;              /* Make new entries look like MSDOS */
extern int verbose;             /* Report oddities in zip file structure */
extern int fix;                 /* Fix the zip file */
extern int filesync;            /* 1=file sync, delete entries not on file system */
extern int adjust;              /* Adjust the unzipsfx'd zip file */
extern int translate_eol;       /* Translate end-of-line LF -> CR LF */
extern int level;               /* Compression level, global (-0, ..., -9) */
extern int levell;              /* Compression level, adjusted by mthd, sufx. */

#define MAX_ACTION_STRING 100
#define MAX_METHOD_STRING 100
#define MAX_INFO_STRING 100

extern char action_string[];    /* action string, such as "Freshen" */           
extern char method_string[];    /* method string, such as "Deflate" */           
extern char info_string[];      /* additional info, such as "AES256" */           

/* Normally Traditional Zip encryption processes the entry in one pass,
   resulting in the CRC.  However at this point the entry is encrypted, so
   the CRC is stored in the following data descriptor.  ETWODD avoids
   this by first calculating the CRC using a first pass, updating the CRC field,
   then encrypting the file on a second pass.  Doing two passes takes longer,
   but the ETWODD entries are more compatible with some utilities.  (Other
   utilities can read the one pass entries with no problem.  Test any other
   utilities you will be using with Traditional encryption before relying
   on it.)

   Given the weakness of Traditional encryption, and the more standard
   structure of AES WG encrypted entries, it is suggested that Traditional
   encryption just not be used for any sensitive information, which avoids
   the issue. */
#ifdef ETWODD_SUPPORT
extern int etwodd;              /* Encrypt Trad without data descriptor. */
#endif /* def ETWODD_SUPPORT */

/* Compression method and level (with file name suffixes). */
typedef struct
{
  int  method;
  int  level;
  int  level_sufx;
  char *method_str;
  char *suffixes;
} mthd_lvl_t;

extern mthd_lvl_t mthd_lvl[];   /* Compr. method, level, file name suffixes. */

#ifdef VMS
   extern int prsrv_vms;        /* Preserve idiosyncratic VMS file names. */
   extern int vmsver;           /* Append VMS version number to file names */
   extern int vms_native;       /* Store in VMS format */
   extern int vms_case_2;       /* ODS2 file name case in VMS. -1: down. */
   extern int vms_case_5;       /* ODS5 file name case in VMS. +1: preserve. */
   extern char **argv_cli;      /* New argv[] storage to free, if non-NULL. */
#endif /* VMS */
#if defined(OS2) || defined(WIN32)
   extern int use_longname_ea;   /* use the .LONGNAME EA as the file's name */
#endif
#if defined (QDOS) || defined(QLZIP)
extern short qlflag;
#endif
/* 9/26/04 EG */
extern int no_wild;             /* wildcards are disabled */
extern int allow_regex;         /* 1 = allow [list] matching (regex) */
extern int wild_stop_at_dir;    /* wildcards do not include / in matches */

#if defined(VMS) || defined(DOS) || defined(WIN32)
# define FILE_SYSTEM_CASE_INS
#endif
extern int file_system_case_sensitive;

#if defined(UNICODE_SUPPORT) && defined(WIN32)
# define WINDOWS_LONG_PATHS
#endif

#ifdef WINDOWS_LONG_PATHS
extern int include_windows_long_paths;  /* include paths longer than MAX_PATH */
extern int archive_has_long_path;
#endif

extern int using_utf8;        /* 1 if current character set is UTF-8 */
#ifdef UNICODE_SUPPORT
# ifdef WIN32
   extern int no_win32_wide;    /* 1 = no wide functions, like GetFileAttributesW() */
# endif
#endif

/* New General Purpose Bit Flag bit 11 flags when entry path and
   comment are in UTF-8 */
#define UTF8_BIT (1 << 11)


/* 10/20/04 */
extern zoff_t dot_size;         /* if not 0 then display dots every size buffers */
extern zoff_t dot_count;        /* if dot_size not 0 counts buffers */
/* status 10/30/04 */
extern int display_counts;      /* display running file count */
extern int display_bytes;       /* display running bytes remaining */
extern int display_globaldots;  /* display dots for archive instead of for each file */
extern int display_volume;      /* display current input and output volume (disk) numbers */
extern int display_usize;       /* display uncompressed bytes */
extern int display_est_to_go;   /* display estimated time to go */
extern int display_time;        /* display time start each entry */
extern int display_zip_rate;    /* display bytes per second rate */
extern ulg files_so_far;        /* files processed so far */
extern ulg bad_files_so_far;    /* files skipped so far */
extern ulg files_total;         /* files total to process */
extern uzoff_t bytes_so_far;    /* bytes processed so far (from initial scan) */
extern uzoff_t good_bytes_so_far;/* good bytes read so far */
extern uzoff_t bad_bytes_so_far;/* bad bytes skipped so far */
extern uzoff_t bytes_total;     /* total bytes to process (from initial scan) */

#ifdef ENABLE_ENTRY_TIMING
extern int performance_time;   /* 1=output total execution time */
extern uzoff_t start_time;     /* start time */
extern uzoff_t start_zip_time; /* when start zipping files (after scan) in usec */
extern uzoff_t current_time;   /* current time in usec */
#endif

extern time_t clocktime;        /* current time */

/* logfile 6/5/05 */
extern int logall;              /* 0 = warnings/errors, 1 = all */
extern FILE *logfile;           /* pointer to open logfile or NULL */
extern int logfile_append;      /* append to existing logfile */
extern char *logfile_path;      /* pointer to path of logfile */
extern int use_outpath_for_log; /* 1 = use output archive path for log */
extern int log_utf8;            /* log names as UTF-8 */
#ifdef WIN32
extern int nonlocal_name;       /* Name has non-local characters */
extern int nonlocal_path;       /* Path has non-local characters */
#endif
#ifdef UNICODE_SUPPORT
/* Unicode 10/12/05 */
extern int use_wide_to_mb_default;/* use the default MB char instead of escape */
#endif

extern char *startup_dir;       /* dir that Zip starts in (current dir ".") */
extern char *working_dir;       /* dir user asked to change to for zipping */
#ifdef UNICODE_SUPPORT_WIN32
extern wchar_t *startup_dirw;  /* dir that Zip starts in (current dir ".") */
extern wchar_t *working_dirw;  /* dir user asked to change to for zipping */
#endif

extern int hidden_files;        /* process hidden and system files */
extern int volume_label;        /* add volume label */
extern int dirnames;            /* include directory names */
extern int filter_match_case;   /* 1=match case when filter() */
extern int diff_mode;           /* 1=diff mode - only store changed and add */
extern char *label;             /* Volume label. */

#ifdef BACKUP_SUPPORT
# define BACKUP_CONTROL_FILE_NAME "control.zbc"
/* backup types */
# define BACKUP_NONE 0
# define BACKUP_FULL 1
# define BACKUP_DIFF 2
# define BACKUP_INCR 3
extern int   backup_type;           /* 0=no,1=full backup,2=diff,3=incr */
extern char *backup_dir;            /* dir for backup archive (and control) */
extern char *backup_name;           /* name of backup archive and control */
extern char *backup_control_dir;    /* dir to put control file */
extern char *backup_log_dir;        /* dir for backup log file */
extern char *backup_start_datetime; /* date/time stamp of start of backup */
extern char *backup_control_path;   /* control file used to store backup set */
extern char *backup_full_path;      /* full archive of backup set */
extern char *backup_output_path;    /* path output archive before finalizing */
#endif

extern int binary_full_check;       /* 1=check entire file for binary before
                                       calling it text */

extern uzoff_t cd_total_entries;  /* num of entries as read from Zip64 EOCDR */
extern uzoff_t total_cd_total_entries; /* num entries across all archives */

extern int sort_apple_double;   /* 1=sort Zip added "._" files after primary files */
extern int sort_apple_double_all;/* 1=ignore AppleDouble zflag and sort all "._" files */

#if defined(WIN32)
extern int only_archive_set;    /* only include if DOS archive bit set */
extern int clear_archive_bits;  /* clear DOS archive bit of included files */
#endif
extern int linkput;             /* Store symbolic links as such (-y) */
#define FOLLOW_ALL 2
#define FOLLOW_NORMAL 1
#define FOLLOW_NONE 0
extern int follow_mount_points; /* 0=skip mount points (-yy), 1=follow normal, 2=follow all (-yy-) */
extern int noisy;               /* False for quiet operation */
extern int extra_fields;        /* 0=create minimum, 1=don't copy old, 2=keep old */
#ifdef NTSD_EAS
 extern int use_privileges;     /* use security privilege overrides */
 extern int no_security;        /* 1=exclude security information (ACLs) */
#endif
extern int no_universal_time;   /* 1=do not store UT extra field */
extern int use_descriptors;     /* use data descriptors (extended headings) */
extern int allow_empty_archive; /* if no files, create empty archive anyway */
extern int copy_only;           /* 1 = copy archive with no changes */
extern int zip_to_stdout;       /* output to stdout */
extern int output_seekable;     /* 1 = output seekable 3/13/05 EG */
#ifdef ZIP64_SUPPORT            /* zip64 globals 10/4/03 E. Gordon */
 extern int force_zip64;        /* force use of zip64 when streaming from stdin */
 extern int zip64_entry;        /* current entry needs Zip64 */
 extern int zip64_archive;      /* at least 1 entry needs zip64 */
#endif
extern int allow_fifo;          /* Allow reading Unix FIFOs, waiting if pipe open */
extern int show_files;          /* show files to operate on and exit (=2 log only) */
extern int include_stream_ef;   /* 1=include stream ef that allows full stream extraction */
extern int cd_only;             /* 1=create cd_only compression archive (central dir only) */

extern int sf_usize;            /* include usize in -sf listing */
extern int sf_comment;          /* include entry comments in -sf listing */

extern char *tempzip;           /* temp file name */
extern FILE *y;                 /* output file now global for splits */

#ifdef UNICODE_SUPPORT
  extern int utf8_native;       /* 1=store UTF-8 as standard per AppNote bit 11 */
#endif
#ifdef UNICODE_SUPPORT_WIN32
 extern int win32_utf8_argv;    /* 1=got UTF-8 from win32 wide command line */
#endif
extern int unicode_escape_all;  /* 1=escape all non-ASCII characters in paths */
extern int unicode_mismatch;    /* unicode mismatch is 0=error, 1=warn, 2=ignore, 3=no */
extern int unicode_show;        /* show unicode on the console (requires console support) */

extern int mvs_mode;            /* 0=lastdot (default), 1=dots, 2=slashes */

extern time_t scan_delay;       /* seconds before display Scanning files message */
extern time_t scan_dot_time;    /* time in seconds between Scanning files dots */
extern time_t scan_start;       /* start of file scan */
extern time_t scan_last;        /* time of last message */
extern int scan_started;        /* scan has started */
extern uzoff_t scan_count;      /* Used for "Scanning files..." message */

extern ulg before;              /* 0=ignore, else exclude files before this time */
extern ulg after;               /* 0=ignore, else exclude files newer than this time */

/* in split globals */

extern ulg total_disks;

extern ulg current_in_disk;
extern uzoff_t current_in_offset;
extern ulg skip_current_disk;


/* out split globals */

extern ulg    current_local_disk; /* disk with current local header */

extern ulg     current_disk;     /* current disk number */
extern ulg     cd_start_disk;    /* central directory start disk */
extern uzoff_t cd_start_offset;  /* offset of start of cd on cd start disk */
extern uzoff_t cd_entries_this_disk; /* cd entries this disk */
extern uzoff_t total_cd_entries; /* total cd entries in new/updated archive */
extern ulg     zip64_eocd_disk;  /* disk with Zip64 EOCD Record */
extern uzoff_t zip64_eocd_offset; /* offset of Zip64 EOCD Record */
/* for split method 1 (keep split with local header open and update) */
extern char *current_local_tempname; /* name of temp file */
extern FILE  *current_local_file; /* file pointer for current local header */
extern uzoff_t current_local_offset; /* offset to start of current local header */
/* global */
extern uzoff_t bytes_this_split; /* bytes written to current split */
extern int read_split_archive;   /* 1=scanzipf_reg detected spanning signature */
extern int split_method;         /* 0=no splits, 1=seekable, 2=data descs, -1=no */
extern uzoff_t split_size;       /* how big each split should be */
extern int split_bell;           /* when pause for next split ring bell */
extern uzoff_t bytes_prev_splits; /* total bytes written to all splits before this */
extern uzoff_t bytes_this_entry; /* bytes written for this entry across all splits */
extern int noisy_splits;         /* note when splits are being created */
extern int mesg_line_started;    /* 1=started writing a line to mesg */
extern int logfile_line_started; /* 1=started writing a line to logfile */
extern char *tempath;            /* Path for temporary files */
extern FILE *mesg;               /* Where informational output goes */
/* dll progress */
extern uzoff_t bytes_read_this_entry; /* bytes read from current input file */
extern uzoff_t bytes_expected_this_entry; /* uncompressed size from scan */
extern char usize_string[];      /* string version of bytes_expected_this_entry */
extern char *entry_name;         /* used by DLL to pass z->zname to iz_file_read() */
extern char *unicode_entry_name; /* Unicode version of entry name, if any, or NULL */

extern uzoff_t progress_chunk_size;  /* how many bytes before next progress report */
extern uzoff_t last_progress_chunk;  /* used to determine when to send next report */


/* User-triggered (Ctrl/T, SIGUSR1) progress.  (Expects localtime_r().) */

#if defined( NO_LOCALTIME_R) && !defined( NO_USER_PROGRESS)
# define NO_USER_PROGRESS
#endif

#ifndef NO_USER_PROGRESS
# ifndef VMS
#  include <signal.h>
# endif /* ndef VMS */
# if defined(VMS) || defined(SIGUSR1)
#  if !defined(MACOS) && !defined(WINDLL)
#   define ENABLE_USER_PROGRESS
#  endif /* !defined(MACOS) && !defined(WINDLL) */
# endif /* defined(VMS) || defined(SIGUSR1) */
#endif /* ndef NO_USER_PROGRESS */

#ifdef ENABLE_USER_PROGRESS
 extern char *u_p_name;
 extern char *u_p_task;
 extern int u_p_phase;
#endif /* def ENABLE_USER_PROGRESS */

/* Encryption. */

/* Values for encryption_method. */
#define NO_ENCRYPTION            0      /* None. */
#define TRADITIONAL_ENCRYPTION   1      /* Traditional (weak). */
#define AES_128_ENCRYPTION       2      /* AES (WG) mode 1. */
#define AES_192_ENCRYPTION       3      /* AES (WG) mode 2. */
#define AES_256_ENCRYPTION       4      /* AES (WG) mode 3. */

#define AES_MAX_ENCRYPTION      AES_256_ENCRYPTION      /* AES upper bound. */
#define AES_MIN_ENCRYPTION      AES_128_ENCRYPTION      /* AES lower bound. */
/* Note that code which tests "method >= AES_MIN_ENCRYPTION", must be
 * changed to test "AES_MAX_ENCRYPTION >= method >= AES_MIN_ENCRYPTION"
 * if a method beyond AES_MAX_ENCRYPTION is ever added.
 */

#define AES_128_MIN_PASS        16      /* Min password length for AES 128 */
#define AES_192_MIN_PASS        20      /* Min password length for AES 192 */
#define AES_256_MIN_PASS        24      /* Min password length for AES 256 */


extern char *key;               /* Encryption password.  (NULL, if none.) */
extern char *passwd;            /* Password before keyfile content added */
extern int pass_pswd_to_unzip;  /* 1=put password on cmd line to unzip to test */
extern int force_ansi_key;      /* Only ANSI characters for password (char codes 32 - 126) */
extern int allow_short_key;     /* Allow password to be shorter than minimum */
extern int encryption_method;   /* See above defines */
extern char *keyfile;           /* File to read (end part of) password from */
extern char *keyfile_pass;      /* (piece of) password from keyfile */

#ifdef IZ_CRYPT_AES_WG
  /* Values for aes_vendor_version. */
# define AES_WG_VEND_VERS_AE1 0x0001    /* AE-1. */
# define AES_WG_VEND_VERS_AE2 0x0002    /* AE-2. */

 extern int aes_strength;
 extern int key_size;                 /* Size of strong encryption key */
 extern fcrypt_ctx zctx;
 extern unsigned char *zpwd;
 extern int zpwd_len;
 extern unsigned char *zsalt;
 extern unsigned char zpwd_verifier[PWD_VER_LENGTH];
 extern prng_ctx aes_rnp;
 extern unsigned char auth_code[20]; /* returned authentication code */
#endif /* IZ_CRYPT_AES_WG */

#ifdef IZ_CRYPT_AES_WG_NEW
# define PWD_VER_LENGTH 2
 extern int key_size;                 /* Size of strong encryption key */
 extern ccm_ctx aesnew_ctx;
 extern unsigned char *zpwd;
 extern int zpwd_len;
 extern unsigned char *zsalt;
 extern unsigned char zpwd_verifier[PWD_VER_LENGTH];
 extern unsigned char auth_code[20]; /* returned authentication code */
#endif

extern char **args;             /* Copy of argv that can be updated and freed */

extern int allow_arg_files;     /* 1=process arg files (@argfile), 0=@ not significant */
extern int case_upper_lower;    /* Upper or lower case added/updated output paths */
extern char *path_prefix;       /* Prefix to add to all new archive entries */
extern int path_prefix_mode;    /* 0=Prefix all paths, 1=Prefix only added/updated paths */
extern char *stdin_name;        /* Name to change default stdin "-" to */
extern int all_ascii;           /* Skip binary check and handle all files as text */
extern char *zipfile;           /* New or existing zip archive (zip file) */
extern FILE *in_file;           /* Current input file for spits */
extern char *in_path;           /* Name of input archive, used to track reading splits */
extern char *in_split_path;     /* in split path */
extern char *out_path;          /* Name of output file, usually same as zipfile */
extern int zip_attributes;
extern char *old_in_path;       /* used to save in_path when doing incr archives */

/* zip64 support 08/31/2003 R.Nausedat */
extern uzoff_t zipbeg;          /* Starting offset of zip structures */
extern uzoff_t cenbeg;          /* Starting offset of central directory */
extern uzoff_t tempzn;          /* Count of bytes written to output zip file */

/* NOTE: zcount and fcount cannot exceed "size_t" (resp. "extent") range.
   This is an internal limitation built into Zip's action handling:
   Zip keeps "{z|f}count * struct {z|f}list" arrays in (flat) memory,
   for sorting, file matching, and building the central-dir structures.
   Typically size_t is a long, allowing up to about 2^31 entries.
 */

extern struct zlist far *zfiles;/* Pointer to list of files in zip file */
extern struct zlist far * far *zfilesnext;/* Pointer to end of zfiles */
extern extent zcount;           /* Number of files in zip file */
extern int zipfile_exists;      /* 1 if zipfile exists */
extern ush zcomlen;             /* Length of zip file comment */
extern char *zcomment;          /* Zip file comment (not zero-terminated) (may be now) */
extern struct flist far **fsort;/* List of files sorted by name */
extern struct zlist far **zsort;/* List of files sorted by name */
#ifdef UNICODE_SUPPORT
extern struct zlist far **zusort;/* List of files sorted by zuname */
#endif
extern struct flist far *found; /* List of names found */
extern struct flist far *far *fnxt;     /* Where to put next in found list */
extern extent fcount;           /* Count of names in found list */

extern struct plist *patterns;  /* List of patterns to be matched */
extern unsigned pcount;         /* number of patterns */
extern unsigned icount;         /* number of include only patterns */
extern unsigned Rcount;         /* number of -R include patterns */

#ifdef IZ_CHECK_TZ
extern int zp_tz_is_valid;      /* signals "timezone info is available" */
#endif
#if (defined(MACOS) || defined(WINDLL))
extern int zipstate;            /* flag "zipfile has been stat()'ed */
#endif

extern int show_what_doing;     /* Diagnostic message flag. */


/* zprintf(), zfprintf(), zperror() - replacements for printf, fprintf,
 * and perror.
 * Defined in fileio.c.
 */
#ifndef NO_PROTO
int zprintf(const char *format, ...);
int zfprintf(FILE *file, const char *format, ...);
void zperror(const char *);
#else
int zprintf OF((const char *, ...));
int zfprintf OF((FILE *, const char *, ...));
void zperror OF((const char *));
#endif


/* Diagnostic functions */
#ifdef DEBUG
# ifdef MSDOS
#  undef  stderr
#  define stderr stdout
# endif
#  define diag(where) zfprintf(stderr, "zip diagnostic: %s\n", where)
#  define Assert(cond,msg) {if(!(cond)) error(msg);}
# ifdef THEOS
#  define Trace(x) _fprintf x
#  define Tracev(x) {if (verbose) _fprintf x ;}
#  define Tracevv(x) {if (verbose>1) _fprintf x ;}
#  define Tracec(c,x) {if (verbose && (c)) _fprintf x ;}
#  define Tracecv(c,x) {if (verbose>1 && (c)) _fprintf x ;}
# else
#  define Trace(x) zfprintf x
#  define Tracev(x) {if (verbose) zfprintf x ;}
#  define Tracevv(x) {if (verbose>1) zfprintf x ;}
#  define Tracec(c,x) {if (verbose && (c)) zfprintf x ;}
#  define Tracecv(c,x) {if (verbose>1 && (c)) zfprintf x ;}
# endif
#else
#  define diag(where)
#  define Assert(cond,msg)
#  define Trace(x)
#  define Tracev(x)
#  define Tracevv(x)
#  define Tracec(c,x)
#  define Tracecv(c,x)
#endif

#ifdef DEBUGNAMES
#  define free(x) { int *v;Free(x); v=x;*v=0xdeadbeef;x=(void *)0xdeadbeef; }
#endif

/* The MEMDIAG functions are currently not defined in Zip.  izz are
   Info-ZIP Zip definitions and izc are Info-ZIP Common definitions
   (common to Zip and UnZip). The below are mainly for compatibility
   with modules common to both Zip and UnZip, such as encryption. */
#ifdef MEMDIAG
  void izz_free( void *ptr);
  void *izz_malloc( size_t siz);
  void *izz_realloc( void *ptr, size_t siz);
  void izz_md_check( void);
#else /* def MEMDIAG */
# define izz_free free
# define izz_malloc malloc
# define izz_realloc realloc
#endif /* def MEMDIAG [else] */
#define izc_free izz_free
#define izc_malloc izz_malloc
#define izc_realloc izz_realloc


/* ============================================================================ */

/* DLL, LIB, and ZIPMAIN

   The following macros determine how this interface is compiled:
     WINDLL               - Compile for WIN32 DLL, mainly for MS Visual Studio
                            WINDLL implies WIN32 and ZIPDLL
                            Set by the build environment
     WIN32                - Running on WIN32
                            The compiler usually sets this
     ZIPDLL               - Compile the Zip DLL (WINDLL implies this)
                            Set by the build environment
     WIN32 && ZIPDLL      - A DLL for WIN32 (except MSVS - use WINDLL instead)
                             - this is used for creating DLLs for other ports
                            (such as OS2), but only the MSVS version is currently
                            supported
     USE_ZIPMAIN          - Use zipmain() instead of main() - for DLL and LIB, but
                             could also be used to compile the source into another
                             application directly
                            Deprecated.
     ZIPLIB               - Compile as static library (LIB)
                            For MSVS, WINDLL and ZIPLIB are both defined to create
                             the LIB
     ZIP_DLL_LIB          - Automatically set when ZIPDLL or ZIPLIB is defined and
                             is used to include the API interface code
     NO_ZPARCHIVE         - Set if not using ZpArchive - used with ZIPLIB (DLL has
                             to call an established entry point)
                            Deprecated.

   DLL_ZIPAPI is deprecated.  USE_ZIPMAIN && DLL_ZIPAPI for UNIX and VMS is
   replaced by ZIPLIB && NO_ZPARCHIVE (as they call zipmain() directly instead).
   These are now also deprecated.
*/

/* WINDLL implies WIN32 && ZIPDLL */
# ifdef WINDLL
#  ifndef ZIPDLL
#   define ZIPDLL
#  endif
#  ifndef ENABLE_DLL_PROGRESS
#    define ENABLE_DLL_PROGRESS
#  endif
# endif


# if defined(ZIPDLL) || defined(ZIPLIB)
#  ifndef ZIP_DLL_LIB
#   define ZIP_DLL_LIB
#  endif
# endif

#if defined(UNIX) && defined(ZIPLIB)
# define UNIXLIB
#endif


/* Win32 wide command line */
#if defined(UNICODE_SUPPORT_WIN32) && defined(AT_LEAST_WINXP)

/* Do not get wide command line if getting command line from DLL/LIB. */
# ifndef ZIP_DLL_LIB
#  ifndef WIN32_WIDE_CMD_LINE
#   ifndef NO_WIN32_WIDE_CMD_LINE
#    define WIN32_WIDE_CMD_LINE
#   endif
#  endif
# endif
#endif


/* --------------------------------------- */

/* The filesize type used for API (LIB/DLL) calls defaults to
   unsigned long long.  A port can override this by setting it
   here (or in osdep.h). */

#ifdef ZIP_DLL_LIB
# ifndef API_FILESIZE_T
#  define API_FILESIZE_T unsigned long long
# endif
#endif


#ifdef ZIP_DLL_LIB
/*---------------------------------------------------------------------------
    Prototypes for public Zip API (DLL and LIB) functions.
  ---------------------------------------------------------------------------*/
# include "api.h"

#ifndef USE_ZIPMAIN
# define USE_ZIPMAIN
#endif

#endif /* ZIP_DLL_LIB */

/* ============================================================================ */


/* Public function prototypes */

#ifndef UTIL
#ifdef USE_ZIPMAIN
int zipmain OF((int, char **));
#else
int main OF((int, char **));
#endif /* USE_ZIPMAIN */
#endif


#ifdef EBCDIC
extern int aflag;
#endif /* EBCDIC */
#ifdef CMS_MVS
extern int bflag;
#endif /* CMS_MVS */

#ifndef NO_PROTO
 void zipmessage_nl(ZCONST char *, int);
 void zipmessage(ZCONST char *, ZCONST char *);
 void zipwarn(ZCONST char *, ZCONST char *);
 void zipwarn_i(char *, int, ZCONST char *, ZCONST char *);
 void ziperr(int, ZCONST char *);
 void print_utf8(ZCONST char *);
#else
 void zipmessage_nl OF((ZCONST char *, int));
 void zipmessage OF((ZCONST char *, ZCONST char *));
 void zipwarn OF((ZCONST char *, ZCONST char *));
 void zipwarn_i OF((char *, int, ZCONST char *, ZCONST char *));
 void ziperr OF((int, ZCONST char *));
 void print_utf8 OF((ZCONST char *));
#endif

#ifdef UTIL
#  define error(msg)    ziperr(ZE_LOGIC, msg)
#else
   void error OF((ZCONST char *));
#  ifdef VMSCLI
     void help OF((void));
#  endif
# if 0
   int encr_passwd OF((int, char *, int, ZCONST char *));
# endif
# ifndef NO_PROTO
   int simple_encr_passwd(int modeflag, char *pwbuf, size_t bufsize);
# else
   int simple_encr_passwd OF((int, char *, size_t));
# endif
#endif

        /* in zipup.c */
#ifndef UTIL
  /* zip64 support 08/31/2003 R.Nausedat */
   int percent OF((uzoff_t, uzoff_t));

   int zipup OF((struct zlist far *));
#  ifdef USE_ZLIB
     void zl_deflate_free OF((void));
#  else
     void flush_outbuf OF((char *, unsigned *));
     extern unsigned (*read_buf) OF((char *, unsigned int));
#  endif /* !USE_ZLIB */
   int seekable OF((FILE *));

#  ifdef ZP_NEED_MEMCOMPR
     ulg memcompress OF((char *, ulg, char *, ulg));
#  endif
#  ifdef BZIP2_SUPPORT
   void bz_compress_free OF((void));
#  endif

# ifndef RISCOS
   int suffixes OF((char *, char *));
# else
   int filetypes OF((char *, char *));
# endif
#endif /* !UTIL */

        /* in zipfile.c */
#ifndef UTIL
   struct zlist far *zsearch OF((ZCONST char *));
#  ifdef USE_EF_UT_TIME
     int get_ef_ut_ztime OF((struct zlist far *, iztimes *));
#  endif /* USE_EF_UT_TIME */
   int trash OF((void));
#endif /* !UTIL */
char *ziptyp OF((char *));
int readzipfile OF((void));
#ifndef NO_PROTO
int putlocal(struct zlist far *z, int rewrite);
#else
int putlocal OF((struct zlist far *, int));
#endif
int putextended OF((struct zlist far *));
int putcentral OF((struct zlist far *));
/* zip64 support 09/05/2003 R.Nausedat */
int putend OF((uzoff_t, uzoff_t, uzoff_t, ush, char *));
/* moved seekable to separate function 3/14/05 EG */
int is_seekable OF((FILE *));
int zipcopy OF((struct zlist far *));
int readlocal OF((struct zlist far **, struct zlist far *));
/* made global for handling extra fields */
char *get_extra_field OF((ush, char *, unsigned));
char *copy_nondup_extra_fields OF((char *, unsigned, char *, unsigned, unsigned *));

int read_inc_file OF((char *));

#ifdef IZ_CRYPT_AES_WG
# ifndef NO_PROTO
 int read_crypt_aes_cen_extra_field(struct zlist far *pZEntry,
                                    ush *aes_vendor_version,
                                    int *aes_strength,
                                    ush *comp_method);
# else
 int read_crypt_aes_cen_extra_field();
# endif
#endif

#ifndef NO_PROTO
int exceeds_zip64_threshold(struct zlist far *z);
#else
 int exceeds_zip64_threshold();
#endif


        /* in fileio.c */
void display_dot OF((int, int));
#ifndef UTIL
   char *getnam OF((FILE *));
   struct flist far *fexpel OF((struct flist far *));
   char *last OF((char *, int));
# ifdef UNICODE_SUPPORT
   wchar_t *lastw OF((wchar_t *, wchar_t));
# endif
   char *msname OF((char *));
# ifdef UNICODE_SUPPORT_WIN32
   wchar_t *msnamew OF((wchar_t *));
# endif
   int check_dup_sort OF((int sort_found));
   int filter OF((char *, int));
#ifndef NO_PROTO
   int newname(char *name, int zflags, int casesensitive);
#else
   int newname OF((char *, int, int));
#endif
# ifdef UNICODE_SUPPORT_WIN32
   int newnamew OF((wchar_t *, int, int));
# endif
   /* used by copy mode */
   int proc_archive_name OF((char *, int));
#endif /* !UTIL */
#if (!defined(UTIL) || defined(W32_STATROOT_FIX))
   time_t dos2unixtime OF((ulg));
#endif
#ifndef UTIL
   ulg dostime OF((int, int, int, int, int, int));
   ulg unix2dostime OF((time_t *));
   int issymlnk OF((ulg a));
#  ifdef SYMLINKS
#    define rdsymlnk(p,b,n) readlink(p,b,n)
#  else /* !SYMLINKS */
#    define rdsymlnk(p,b,n) (0)
#  endif /* SYMLINKS */
#endif /* !UTIL */

int set_locale();

int get_entry_comment(struct zlist far *);

int destroy OF((char *));
int replace OF((char *, char *));
int getfileattr OF((char *));
int setfileattr OF((char *, int));
char *tempname OF((char *));

/* for splits */
int close_split OF((ulg, FILE *, char *));
int ask_for_split_read_path OF((ulg));
int ask_for_split_write_path OF((ulg));
char *get_in_split_path OF((char *, ulg));
char *find_in_split_path OF((char *, ulg));
char *get_out_split_path OF((char *, ulg));
int rename_split OF((char *, char *));
int set_filetype OF((char *));

int bfcopy OF((uzoff_t));

int fcopy OF((FILE *, FILE *, uzoff_t));

#ifdef ZMEM
   char *memset OF((char *, int, unsigned int));
   char *memcpy OF((char *, char *, unsigned int));
   int memcmp OF((char *, char *, unsigned int));
#endif /* ZMEM */

        /* in system dependent fileio code (<system>.c) */
#ifndef UTIL
# ifdef PROCNAME
   int wild OF((char *));
# endif
   char *in2ex OF((char *));
   char *ex2in OF((char *, int, int *));
#ifdef UNICODE_SUPPORT_WIN32
   int has_win32_wide OF((void));
   wchar_t *in2exw OF((wchar_t *));
   wchar_t *ex2inw OF((wchar_t *, int, int *));
   int procnamew OF((wchar_t *, int));
#endif
   int procname OF((char *, int));
   void stamp OF((char *, ulg));

   ulg filetime OF((char *, ulg *, zoff_t *, iztimes *));


   /* Windows Unicode */
# ifdef UNICODE_SUPPORT_WIN32
   ulg filetimew OF((wchar_t *, ulg *, zoff_t *, iztimes *));
   char *get_win32_utf8path OF((char *));
# endif /* def UNICODE_SUPPORT_WIN32 */

# ifdef UNIX_APPLE
   int make_apl_dbl_header( char *name, int *hdr_size);
# endif /* UNIX_APPLE */

# if !(defined(VMS) && defined(VMS_PK_EXTRA))
   int set_extra_field OF((struct zlist far *, iztimes *));
# endif /* ?(VMS && VMS_PK_EXTRA) */
   int deletedir OF((char *));
# ifdef MY_ZCALLOC
     zvoid far *zcalloc OF((unsigned int, unsigned int));
     zvoid zcfree       OF((zvoid far *));
# endif /* MY_ZCALLOC */
#endif /* !UTIL */

#ifndef NO_PROTO
  int is_utf8_string(ZCONST char *instring, int *has_bom, int *count,
                     int *ascii_count, int *utf8_count);
#else
  int is_utf8_string OF((ZCONST char *, int *, int *, int *, int *));
#endif

#ifdef UNICODE_SUPPORT
int is_utf8_file OF((FILE *infile, int *has_bom, int *count, int *ascii_count, int *utf8_count));
int read_utf8_bom OF((FILE *infile));
int is_utf16LE_file OF((FILE *infile));
#endif
# ifdef UNICODE_SUPPORT_WIN32
wchar_t *local_to_wchar_string OF ((ZCONST char *));
# endif

void version_local OF((void));

        /* in util.c */
#ifndef UTIL
int   fseekable    OF((FILE *));
char *isshexp      OF((ZCONST char *));
#ifdef UNICODE_SUPPORT
# ifdef WIN32
   wchar_t *isshexpw     OF((ZCONST wchar_t *));
   int dosmatchw   OF((ZCONST wchar_t *, ZCONST wchar_t *, int));
# endif
#endif
int   shmatch      OF((ZCONST char *, ZCONST char *, int));
# if defined(DOS) || defined(WIN32)
   int dosmatch    OF((ZCONST char *, ZCONST char *, int));
# endif /* DOS || WIN32 */
#endif /* !UTIL */

/* functions to convert zoff_t to a string */
char *zip_fuzofft      OF((uzoff_t, char *, char*));
char *zip_fzofft       OF((zoff_t, char *, char*));

/* read and write number strings like 10M */
int DisplayNumString OF ((FILE *file, uzoff_t i));
int WriteNumString OF((uzoff_t num, char *outstring));
uzoff_t ReadNumString OF((char *numstring));


/* string functions */

/* for abbrevmatch and strmatch */
#define CASE_INS 0
#define CASE_SEN 1

/* for strmatch */
#define ENTIRE_STRING 0

/* for string_replace */
#define REPLACE_FIRST 1
#define REPLACE_ALL 0

/* for string_find */
#define LAST_MATCH -1
#define NO_MATCH -1

/* for check_zipfile */
#define TESTING_TEMP 1
#define TESTING_FINAL_NAME 0

        /* in fileio.c */

/* Make copy of string */
char *string_dup OF((ZCONST char *in_string, char *error_message, int fluff));

/* Replace substring with string */
#ifndef NO_PROTO
char *string_replace(char *in_string, char *find, char *replace,
                     int replace_times, int case_sens);
#else
char *string_replace OF((char *, char *, char *, int, int));
#endif

char *trim_string OF((char *instring));

/* find string find within instring */
#ifndef NO_PROTO
int string_find (char *instring, char *find, int case_sens, int occurrence);
#else
int string_find OF((char *, char *, int, int));
#endif

        /* in util.c */

/* returns true if abbrev is abbreviation for matchstring */
#ifndef NO_PROTO
int abbrevmatch (char *matchstring, char *abbrev, int cs, int minmatch);
#else
int abbrevmatch OF((char *, char *, int, int));
#endif

/* returns true if strings match up to maxmatch (0 = entire string) */
#ifndef NO_PROTO
int strmatch (char *s1, char *s2, int case_sensitive, int maxmatch);
#else
int strmatch OF((char *, char *, int, int));
#endif



void init_upper    OF((void));
int  namecmp       OF((ZCONST char *string1, ZCONST char *string2));

#ifdef EBCDIC
  char *strtoasc     OF((char *str1, ZCONST char *str2));
  char *strtoebc     OF((char *str1, ZCONST char *str2));
  char *memtoasc     OF((char *mem1, ZCONST char *mem2, unsigned len));
  char *memtoebc     OF((char *mem1, ZCONST char *mem2, unsigned len));
#endif /* EBCDIC */
#ifdef IZ_ISO2OEM_ARRAY
  char *str_iso_to_oem    OF((char *dst, ZCONST char *src));
#endif
#ifdef IZ_OEM2ISO_ARRAY
  char *str_oem_to_iso    OF((char *dst, ZCONST char *src));
#endif

zvoid far **search OF((ZCONST zvoid *, ZCONST zvoid far **, extent,
                       int (*)(ZCONST zvoid *, ZCONST zvoid far *)));
void envargs       OF((int *, char ***, char *, char *));
void expand_args   OF((int *, char ***));

#ifndef NO_PROTO
  int  is_text_buf(ZCONST char *buf_ptr, unsigned buf_size);
#else
  int  is_text_buf OF((ZCONST char *buf_ptr, unsigned buf_size));
#endif

/* this is no longer used ...
unsigned int adler16 OF((unsigned int, ZCONST uch *, extent));
*/
        /*  crc functions are now declared in crc32.h */

#ifndef UTIL
#ifndef USE_ZLIB
        /* in deflate.c */
void lm_init OF((int, ush *));
void lm_free OF((void));

uzoff_t deflate OF((void));

        /* in trees.c */
void     ct_init      OF((ush *, int *));
int      ct_tally     OF((int, int));
uzoff_t  flush_block  OF((char far *, ulg, int));
void     bi_init      OF((char *, unsigned int, int));
#endif /* !USE_ZLIB */
#endif /* !UTIL */

        /* in system specific assembler code, replacing C code in trees.c */
#if defined(ASMV) && defined(RISCOS)
  void     send_bits    OF((int, int));
  unsigned bi_reverse   OF((unsigned int, int));
#endif /* ASMV && RISCOS */

/*---------------------------------------------------------------------------
    VMS-only functions:
  ---------------------------------------------------------------------------*/
#ifdef VMS
  int    vms_stat        OF((char *, stat_t *));                /* vms.c */
  int    vms_status      OF((int));                             /* vms.c */
  void   vms_exit        OF((int));                             /* vms.c */
  char  *vms_getcwd      OF((char *bf, size_t sz, int styl));   /* vms.c */
# ifdef __DECC
#  ifdef __CRTL_VER
#   if !defined(__VAX) && (__CRTL_VER >= 70301000)              /* vms.c */
  void decc_init( void);
#   endif /* !defined(__VAX) && (__CRTL_VER >= 70301000) */
#  endif /* def __CRTL_VER */
# endif /* def __DECC */
# ifndef UTIL
#  ifdef VMSCLI
  unsigned int vms_zip_cmdline OF((int *, char ***));           /* cmdline.c */
  void   VMSCLI_help     OF((void));                            /* cmdline.c */
#  endif /* VMSCLI */
# endif /* !UTIL */
#endif /* VMS */

#if 0
#ifdef ZIP64_SUPPORT
   update_local_Zip64_extra_field OF((struct zlist far *, FILE *));
#endif
#endif /* 0 */

/*---------------------------------------------------------------------------
    WIN32-only functions:
  ---------------------------------------------------------------------------*/
#ifdef WIN32
   int ZipIsWinNT         OF((void));                         /* win32.c */
   int ClearArchiveBit    OF((char *));                       /* win32.c */
   int GetWinVersion OF((unsigned long *, unsigned long *, unsigned long *));
# ifdef UNICODE_SUPPORT
   int ClearArchiveBitW   OF((wchar_t *));                    /* win32.c */
# endif
#endif /* WIN32 */

   FILE *fopen_utf8 OF((char *filename, char *mode));

#if defined(NTSD_EAS) && !defined(UTIL)
   /* These are defined in nt.c.
      2014/06/11 */

# define MAX_SYMLINK_WTARGET_NAME_LENGTH 8192

   /* True if a real symlink. */
   int isWinSymlink(char *path);
   int isWinSymlinkw(wchar_t *wpath);
   
   /* Returns information about a Windows directory object, such as a file,
      directory or reparse point such as a symlink. */
   int WinDirObjectInfo(wchar_t *wpath,
                        unsigned long *attr,
                        unsigned long *reparse_tag,
                        wchar_t *wtarget_name,
                        size_t max_wtarget_name_length);

   /* Windows replacement for Unix readlink(). */
   int readlink(char *, char *, size_t);
   int readlinkw(wchar_t *, char *, size_t);

#endif


/* WIN32_OEM */

#ifdef WIN32
  /* convert oem to ansi string */
  char *oem_to_local_string OF((char *, char *));

  /* convert local string to oem string */
  char *local_to_oem_string OF((char *, char *));
#endif


/* Upper/Lower case */

  /* Convert local string to uppercase/lowercase */
#ifndef NO_PROTO
  int astring_upper_lower(char *ascii_string, int upper);
#else
  int astring_upper_lower OF((char *, int));
#endif

  /* Convert UTF-8 string to uppercase/lowercase */
#ifndef NO_PROTO
  char *ustring_upper_lower(char *utf8_string, int upper);
#else
  char *ustring_upper_lower OF((char *, int));
#endif


#ifdef ENABLE_ENTRY_TIMING
 uzoff_t get_time_in_usec OF(());
#endif

#ifdef IZ_CRYPT_AES_WG
 void aes_crypthead OF((ZCONST uch *, int, ZCONST uch *));
 int entropy_fun OF((unsigned char buf[], unsigned int len));
#endif


/*---------------------------------------------------------------------
  LZMA
  27 August 2011
  ---------------------------------------------------------------------*/

/* This needs to be global to support VC project configuration */

 /* disable multiple threads, which most ports can't do.  May
    enable on Win32 at some point. */
 /* for now do things generically */

#ifndef _7ZIP_ST
# define _7ZIP_ST
#endif

#ifdef LZMA_SUPPORT
# define NO_USE_WINDOWS_FILE
#endif

/* this needs to be global for LZMA */
#ifndef NO_PROTO
  local unsigned iz_file_read_bt(char *buf, unsigned size);
#else
  local unsigned iz_file_read_bt OF((char *, unsigned));
#endif


/* Case conversion options - used with -Cl and -Cu */
#define CASE_PRESERVE 0
#define CASE_UPPER 1
#define CASE_LOWER 2




/*---------------------------------------------------------------------
    Unicode Support
    28 August 2005
  ---------------------------------------------------------------------*/
#ifdef UNICODE_SUPPORT

  /* Default character when a zwchar too big for wchar_t */
# define zwchar_to_wchar_t_default_char '_'

  /* Default character string when wchar_t does not convert to mb */
# define wide_to_mb_default_string "_"

  /* wide character type */
  typedef unsigned long zwchar;


  /* check if string is all ASCII */
  int is_ascii_string OF((char *mbstring));
  int is_ascii_stringw OF((wchar_t *wstring));

  zwchar *wchar_to_wide_string OF((wchar_t *wchar_string));
  wchar_t *wide_to_wchar_string OF((zwchar *wide_string));

  char *utf8_to_local_string OF((char *utf8_string));
  char *local_to_utf8_string OF((char *local_string));

  zwchar *utf8_to_wide_string OF((ZCONST char *utf8_string));
  char *wide_to_utf8_string OF((zwchar *wide_string));

  char *wide_to_local_string OF((zwchar *wide_string));
  zwchar *local_to_wide_string OF((char *local_string));

  char *wide_to_escape_string OF((zwchar *wide_string));
  char *local_to_escape_string OF((char *local_string));
  char *utf8_to_escape_string OF((char *utf8_string));

  int zwchar_string_len OF((zwchar *wide_string));
  zwchar *char_to_wide_string OF((char *char_string));

  wchar_t *utf8_to_wchar_string OF((ZCONST char *utf8_string));
  char *wchar_to_utf8_string OF((wchar_t *wchar_string));

#ifdef WIN32
  char **get_win32_utf8_argv OF(());
  wchar_t *utf8_to_wchar_string_windows OF((ZCONST char *utf8_string));
  char *wchar_to_utf8_string_windows OF((wchar_t *wchar_string));

  char *wchar_to_local_string OF((wchar_t *));

#endif

#ifdef WIN32
  unsigned long write_console(FILE *outfile, ZCONST char *instring);
  unsigned long write_consolew(FILE *outfile, wchar_t *instringw);
# if 0
  long write_consolew2(wchar_t *instringw);
# endif
#endif


  /* convert local string to multi-byte display string */
  char *local_to_display_string OF((char *));

  /* convert wide character to escape string */
  char *wide_char_to_escape_string OF((unsigned long));

  zwchar *escapes_to_wide_string OF((zwchar *));
  char *escapes_to_utf8_string OF((char *escaped_string));
#ifdef UNICODE_SUPPORT_WIN32
  /* convert escape string to wide character */
  zwchar escape_string_to_wide_char OF((char *));
  wchar_t *escapes_to_wchar_string OF((wchar_t *));
#endif


#if defined(__STDC_UTF_16__)
# define UTFSIZE 16
#else
# if defined(__STDC_UTF_32__)
# define UTFSIZE 32
# else
# define UTFSIZE 0
# endif
#endif

#endif /* UNICODE_SUPPORT */


/*---------------------------------------------------
 * Split archives
 *
 * 10/20/05 EG
 */

#define BFWRITE_DATA 0
#define BFWRITE_LOCALHEADER 1
#define BFWRITE_CENTRALHEADER 2
#define BFWRITE_HEADER 3 /* data descriptor or end records */

size_t bfwrite OF((ZCONST void *buffer, size_t size, size_t count,
                   int));

/* for putlocal() */
#define PUTLOCAL_WRITE 0
#define PUTLOCAL_REWRITE 1


#ifndef NO_SHOW_PARSED_COMMAND
# define SHOW_PARSED_COMMAND
#endif


/*--------------------------------------------------------------------
    Long option support
    23 August 2003
    See fileio.c
  --------------------------------------------------------------------*/

/* Option value types.  See get_option() in fileio.c for details. */

/* value_type - value is always returned as a string. */
#define o_NO_VALUE        0   /* this option does not take a value */
#define o_REQUIRED_VALUE  1   /* this option requires a value */
#define o_OPTIONAL_VALUE  2   /* value is optional (may be replaced by o_OPT_EQ_VALUE) */
#define o_VALUE_LIST      3   /* this option takes a list of values */
#define o_ONE_CHAR_VALUE  4   /* next char is value (does not end short opt string) */
#define o_NUMBER_VALUE    5   /* value is integer (does not end short opt string) */
#define o_OPT_EQ_VALUE    6   /* value optional, but "=" required for one. */


/* negatable - a dash following the option (but before any value) sets negated. */
#define o_NOT_NEGATABLE   0   /* trailing '-' to negate either starts value or generates error */
#define o_NEGATABLE       1   /* trailing '-' sets negated to TRUE */


/* option_num can be this when option not in options table */
#define o_NO_OPTION_MATCH     -1

/* special values returned by get_option - do not use these as option IDs */
#define o_NON_OPTION_ARG      ((unsigned long) 0xFFFF)    /* returned for non-option
                                                             args */
#define o_ARG_FILE_ERR        ((unsigned long) 0xFFFE)    /* internal recursion
                                                             return (user never sees) */

/* Maximum size (in bytes) of a returned value (0 = no limit).  Should be
   less than errbuf - any message sizes. */

#define MAX_OPTION_VALUE_SIZE (ERRBUF_SIZE - 1024)


/* Default extension for argument files.  Must be 3 letters to work with code in fileio.c. */
#define ARGFILE_EXTENSION ".zag"


/* options array is set in zip.c */
struct option_struct {
  char *shortopt;           /* char * to sequence of char that is short option */
  char Far *longopt;        /* char * to long option string */
  int  value_type;          /* from above */
  int  negatable;           /* from above */
  unsigned long option_ID;  /* value returned by get_option when this option is found */
  char Far *name;           /* optional string for option returned on some errors */
};
extern struct option_struct far options[];


/* moved here from fileio.c to make global - 10/6/05 EG */

/* If will support wide for Unicode then need to add */
  /* multi-byte */
#ifdef _MBCS
# ifndef MULTIBYTE_GETOPTNS
#   define MULTIBYTE_GETOPTNS
# endif
#endif
#ifdef MULTIBYTE_GETOPTNS
  int mb_clen OF((ZCONST char *));
# define MB_CLEN(ptr) mb_clen(ptr)
# define MB_NEXTCHAR(ptr) ((ptr) += MB_CLEN(ptr))
#else
  /* no multi-byte */
# define MB_CLEN(ptr) (1)
# define MB_NEXTCHAR(ptr) ((ptr)++)
#endif


/* function prototypes */

/* get the next option from args */
unsigned long get_option OF((char ***pargs, int *argc, int *argnum, int *optchar,
                             char **value, int *negated, int *first_nonopt_arg,
                             int *option_num, int recursion_depth));

/* copy args - copy an args array, allocating space as needed */
char **copy_args OF((char **args, int max_args));

/* arg count - return argc of args */
int arg_count OF((char **args));

/* free args - free args created with one of these functions */
int free_args OF ((char **args));

/* insert arg - copy an arg into args */
int insert_arg OF ((char ***args, ZCONST char *arg, int insert_at,
                    int free_args));


/*--------------------------------------------------------------------
    End of Long option support
  --------------------------------------------------------------------*/


#endif /* !__zip_h */
/* end of zip.h */
