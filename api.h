/*
  api.h - Zip 3

  Copyright (c) 1990-2014 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/

/* api.h and api.c are a callable interface to Zip.  The following entry points
   are defined:

   Exported DLL entry points:

     Current entry points:

       ZpVersion            - Returns version of DLL or LIB.  User should call
                              this first to make sure the found DLL or the LIB
                              used is compatible with your application.

       ZpZip                - Single call main entry point for calling Zip with
                              command line to execute.  This is now the only
                              recommended way to call Zip as a DLL or LIB.

       ZpStringCopy         - A simple string copy function used only by the
                              Visual Studio 2010 Visual Basic example in the
                              vb10 directory to pass back string arguments.
                              See that example for how it is used.

     Deprecated (do not use):

       ZpInit               - Initializes function callbacks.  This is now done
                              by providing the callback pointer structure to
                              ZpZip.

       ZpArchive            - Calls DLL or LIB using old options structure.
                              The options structure is now deprecated as much
                              more functionality is provided by using the
                              command line interface of ZpZip.  The options
                              structure has not been updated and ZpArchive has
                              not been recently tested.

     Test entry points:

       ZpZipTest            - Single call main entry point for testing Zip DLL
                              interfaces.  See the vb10 example for how this
                              is used.

       ZpTestComParse       - Test command line parsing.  See the vb10 example
                              for how this is used.

       ZpTestCallback       - Test calling a user callback.  See the vb10
                              example for how this is used.

       ZpTestCallbackStruct - Test providing callback addresses to DLL using
                              structure.  See the vb10 example for how this is
                              used.

   Calling zipmain directly is now deprecated.  The preferred invocation when
   using the LIB or DLL is via ZpZip.  When the Zip source is included
   directly, USE_ZIPMAIN_NO_API may be useful, but the API callbacks would not
   be available without code modification.

   Currently the LIB has the following versions:

      zip32_lib.lib       - The Zip static library on Windows.
      zip32_libd.lib      - Debug version of zip32_lib.lib.
      libizzip.a          - The Zip static library on Unix.

   Currently the DLL has the following components:

      zip32_dll.dll       - The Zip dynamic library on Windows.
      zip32_dll.lib       - Library to link to with linkages to zip32_dll.dll.

   In brief, a user program using the DLL calls ZpVersion() to get the
   version of the DLL.  If the version is compatible, ZpZip() is then called
   to both set callbacks and to pass command line to execute.  Because of a
   need to maintain DLL calling context, best to pass all information in
   a single call to the DLL, which is what ZpZip() does.  The old approach of
   setting callbacks using ZpInit() and then calling ZpArchive() to perform
   the actual archiving operations should no longer be used for the DLL as
   it could result in corrupted data or the app crashing on some platforms.
   (One should not assume the context of the DLL has not changed between
   the ZpInit() and the ZpArchive() calls.  It probably hasn't, but it's still
   an assumption.  Older Windows platforms has known issues with this.)  All
   interactions with the DLL once ZpZip() is called is through callbacks and
   the final return code.

   When the LIB is linked in, many of the DLL context issues do not apply.
   In this case ZpInit() could be used to set function callbacks once and
   the local function zipmain() used to actually call Zip.  And, unlike the
   DLL, the LIB inherits the caller's stdin, stdout, and stderr.  The LIB
   also inherits any directory context.  However, using ZpZip() is still
   recommended.

   See WhatsNew_DLL_LIB.txt for related information on the LIB and DLL.

   The following callback functions can be defined by the caller.  Any
   callback not used should be set to NULL.  Zip will skip any NULL
   address callbacks:

     DLLPRNT             - Called to print strings.  Nearly all standard
     (print)               output of Zip is routed through this callback.
                           If set to NULL, standard output is discarded,
                           except for specific output also routed through
                           other callbacks below.

     DLLPASSWORD         - If a password is needed by Zip, this function is
     (password)            called.  If the user will be encrypting entries,
                           this callback must be defined to set the password
                           or an error will occur.

     DLLSERVICE          - If defined, called as each entry is processed.
     (service)             This callback provides stats at the entry level.
                           It also allows the Zip operation to be aborted by
                           setting the callback return code to 1.  This
                           version uses long long (64-bit) parameters for
                           file sizes (except on VMS).

     DLLSERVICE_NO_INT64 - A version of DLLSERVICE that does not use 64-bit
     (service_no_int64)    args for file size, but instead uses high and low
                           32-bit parts.  This was needed for Visual Basic 6,
                           for instance, which doesn't know how to handle
                           64-bit arguments.  However, the latest example code
                           in the vb6 directory shows that Visual Studio 6
                           Visual Basic can work with 64-bit parameters.  As
                           such, this callback is now only available on WIN32
                           and is deprecated.

     DLLSPLIT            - Called to request split destination if splitting
     (split)               (not yet implemented).  DO NOT USE the split
                           destination feature or split pause feature (-sp)
                           with the DLL until this is implemented.  (-sp is,
                           in fact, locked out when Zip is compiled as a LIB
                           or DLL.)  Note that -s does not interact with the
                           user and can be used by the LIB/DLL to create
                           split archives.

     DLLECOMMENT         - Called to request a comment for an entry.  If the
     (ecomment)            caller will be working with entry comments this
                           callback must be defined or an error will result.

     DLLACOMMENT         - Called to request the archive zipfile comment.  If
     (acomment)            the caller will be working with archive comments
                           this callback must be defined or an error will
                           result.

     DLLPROGRESS         - If defined, this callback is called to report
     (progress)            progress during the Zip operation.  It returns:
                           - entry name (path)
                           - UTF-8 entry name (upath)
                           - % of bytes of entry processed
                           - % of all bytes processed
                           - uncompressed file size in bytes
                           - uncompressed file size as string (such as 7.7m)
                           - action being performed for this entry, such as Add
                           - method being used for this entry, such as Deflate
                           - additional information if any

                           This callback is called at start and end of each
                           entry processing as well as after so many bytes
                           processed for that entry.  The processing chunk
                           size is set as an argument to ZpZip(), which sets
                           the global progress_chunk_size.  (If ZpArchive is
                           used, or zipmain called directly, and this callback
                           is used, the user must ensure that this global is
                           properly set.)  This is the number of bytes to
                           process before putting out another progress
                           report.  The chunk size should be at least 1K
                           bytes.  The smaller the chunk size, the more fine
                           the control and reporting, but the slower the
                           execution (as callbacks take time).  100m (100 MiB)
                           may be a good compromise.  Setting the return code
                           to 1 aborts the zip operation.  Since the progress
                           callback may be called many times while an entry is
                           processed, while the service callback is called
                           just once, using this callback generally allows
                           much quicker response when aborting a large file
                           entry.

     DLLERROR            - This callback is called when a Zip warning or error
     (error)               is generated.  The user should define either the
                           DLLPRNT (print) or DLLERROR (error) callback to see
                           any warnings or errors generated by Zip.  This
                           callback is provided so that DLLPRNT can be NULL
                           (ignore any normal Zip output) while still getting
                           any generated warnings and errors.

     DLLFINISH           - If defined, this callback is called at end of zip
     (finish)              operation to provide stats.

   Unused callbacks should be set to NULL.  Zip will skip any callbacks where
   the address is set to NULL, though using some features (such as -c or -z)
   without setting the appropriate callback will generate an error.

   As a minimum, the DLLPRNT call back should be defined to see what's going
   on.  Alternatively, the DLLSERVICE and DLLERROR callbacks could be used to
   track progress and handle any warnings or errors.  If passwords or comments
   are being worked with, those callbacks must be defined to provide the
   passwords or comments.

   The ZPOPT structure was used to pass settings, such as root directory, to
   Zip via the ZpArchive() call.  As ZpArchive() is now obsolete, this
   structure should no longer be used and is no longer being updated.  Both
   DLL and LIB users should only use ZpZip().

   The LIB is now supported on Unix, VMS, and Windows.  The DLL is currently
   only supported on Windows platforms. */

/* There are basically three ways to include Zip functionality:

     DLL     - Compile Zip as a Dynamic Load Library (DLL) and call it from
               the application using DLL conventions.

     LIB     - Compile Zip as a Static Load Library (LIB), link it in, and
               call it from the application.

     in line - Include the Zip source in the application directly and call Zip
               via zipmain (the user will need to handle any reentry issues,
               as well as functions normally handled by callbacks).

   At this point we are mainly only supporting the DLL and LIB approaches.  If
   the user is integrating Zip source directly, they need to work out any
   issues between their code and ours.

   The following macros determine how this interface is compiled:

     WINDLL               - Compile a WIN32 DLL, mainly for Microsoft Visual
                            Studio.  Implies WIN32 and ZIPDLL.  Set by the
                            build environment.  This is also set when building
                            a Windows LIB (along with ZIPLIB).

     WIN32                - Running on WIN32.  The compiler usually sets this.

     ZIPDLL               - Compile the Zip DLL (WINDLL implies this).  Set by
                            the build environment.  (We tend to set WINDLL in
                            our Visual Studio projects for historical reasons.)
                            We currently do not support building dynamic
                            libraries on platforms other than Windows, and as
                            we tend to use WINDLL on Windows, ZIPDLL probably
                            should not be used in any build environment.  It
                            gets set automatically when WINDLL is set.

     WIN32 and ZIPDLL     - This would be used to build a Windows DLL using
                            other than Visual Studio.  However, currently no
                            other Windows platforms are suppported for building
                            the DLL.  (OS2 may again be supported some day.)

     USE_ZIPMAIN          - Use zipmain() instead of main().  Set automatically
                            when ZIPDLL or ZIPLIB are set, but could also be
                            used to compile Zip source into another application
                            directly.

     ZIPLIB               - Compile as static library (LIB).  For Microsoft
                            Visual Studio, WINDLL and ZIPLIB are both defined
                            to create the LIB.

     ZIP_DLL_LIB          - Set by zip.h when either ZIPDLL or ZIPLIB are
                            defined.

     NO_ZPARCHIVE         - Deprecated.  Set if not using ZpArchive.  It was
                            used with ZIPLIB on some ports.  (The DLL has to
                            call an established entry point so can't call
                            zipmain() directly.)  ZpZip is now preferred over
                            calling zipmain() directly.  NO_ZPARCHIVE is NOT
                            set when using ZpZip, just call ZpZip.

     UNIXLIB              - Set when UNIX and ZIPLIB are set.

   On Windows, the build environment should also define _LIB for the static
   library and _USRDLL for the dynamic library.  If you choose the right
   project settings in Visual Studio, these should be set for you.

   When building applications using the LIB, set ZIPLIB in the build environment
   and include api.h.  When building DLL applications, set WINDLL (on Windows)
   or ZIPDLL in the build environment.

   DLL_ZIPAPI is deprecated.  Use of USE_ZIPMAIN && DLL_ZIPAPI for UNIX and VMS
   is replaced by ZIPLIB && NO_ZPARCHIVE.  Creates LIB that calls zipmain()
   directly.  But actually this is now deprecated as well.  The preference now
   is to use ZpZip().

   File size in the calls below is of type API_FILESIZE_T, which defaults in
   zip.h to uzoff_t.  On Windows (the only port providing a DLL currently) this
   MUST be 64-bit, i.e. the DLL will not run (due to a check in zip.c) if
   compiled without LARGE_FILE_SUPPORT.  The DLL interface sizes are not
   checked, so it is important that only one size be used for each structure
   member, including file size.  LIB users should use API_FILESIZE_T for file
   sizes, which should be set appropriately for the port.  A port can override
   the setting of API_FILESIZE_T in tailor.h or osdep.h.
*/


#ifndef _ZIPAPI_H
# define _ZIPAPI_H


# include "zip.h"

/* =================
 * This needs to be looked at
 */
#  ifdef WIN32
#   ifndef PATH_MAX
#    define PATH_MAX 260
#   endif
# endif


# if 0
# ifdef __cplusplus
   extern "C"
   {
      void  EXPENTRY ZpVersion(ZpVer far *);
      int   EXPENTRY ZpInit(LPZIPUSERFUNCTIONS lpZipUserFunc);
      int   EXPENTRY ZpArchive(ZCL C, LPZPOPT Opts);
   }
# endif
# endif /* 0 */

# ifdef WIN32
#  include <windows.h>

/* Porting definations between Win 3.1x and Win32 */
#  define far
#  define _far
#  define __far
#  define near
#  define _near
#  define __near
# endif

# if 0
#  define USE_STATIC_LIB
# endif /* 0 */

# ifdef VMS
    /* Get a realistic value for PATH_MAX. */
    /* 2012-12-31 SMS.
     * Use <nam.h> instead of <namdef.h> to avoid conflicts on some old
     * systems (like, say, VMS V5.5-2, DEC C V4.0-000), where <nam.h>
     * and <namdef.h> are incompatible, and [.vms]vms.h gets <rms.h>,
     * which gets <nam.h>.  (On modern systems, <nam.h> is a wrapper for
     * <namdef.h>.)
     */
#  include <nam.h>
#  undef PATH_MAX
   /* Some compilers may complain about the "$" in NAML$C_MAXRSS. */
#  ifdef NAML$C_MAXRSS
#   define PATH_MAX (NAML$C_MAXRSS+1)
#  else
#   define PATH_MAX (NAM$C_MAXRSS+1)
#  endif
# endif /* def VMS */

# ifndef PATH_MAX
#  ifdef MAXPATHLEN
#   define PATH_MAX      MAXPATHLEN    /* in <sys/param.h> on some systems */
#  else /* def MAXPATHLEN */
#   ifdef _MAX_PATH
#    define PATH_MAX    _MAX_PATH
#   else /* def _MAX_PATH */
#    if FILENAME_MAX > 255
#     define PATH_MAX  FILENAME_MAX    /* used like PATH_MAX on some systems */
#    else /* FILENAME_MAX > 255 */
#     define PATH_MAX  1024
#    endif /* FILENAME_MAX > 255 [else] */
#   endif /* def _MAX_PATH [else] */
#  endif /* def MAXPATHLEN [else] */
# endif /* ndef PATH_MAX */

# ifndef WIN32
   /* Adapt Windows-specific code to normal C RTL. */
#  define far
#  define _far
#  define __far
#  define _msize sizeof
#  define _strnicmp strncasecmp
#  define lstrcat strcat
#  define lstrcpy strcpy
#  define lstrlen strlen
#if 0
#  define GlobalAlloc( a, b) malloc( b)
#  define GlobalFree( a) free( a)
#  define GlobalLock( a) (a)
#  define GlobalUnlock( a)
#endif
#  define BOOL int
#  define DWORD size_t
#  define EXPENTRY
#  define HANDLE void *
#  define LPSTR char *
#  define LPCSTR const char *
#  define WINAPI
# endif /* not WIN32 */


# ifdef ZIP_DLL_LIB

/* The below are used to interface with the DLL and LIB */

#  ifdef WIN32

#   ifdef _LIB
#    ifndef ZIPLIB
#     define ZIPLIB
#    endif
#   endif

#   if defined(_USRDLL) || defined(ZIPLIB) || defined(ZIPDLL)
#    ifndef WINDLL
#     define WINDLL
#    endif
#   endif

#  endif

/*---------------------------------------------------------------------------
    Prototypes for public Zip API (DLL and LIB) functions.
  ---------------------------------------------------------------------------*/

#  define ZPVER_LEN    sizeof(ZpVer)
/* These defines are set to zero for now, until OS/2 comes out
   with a dll.
 */
#  define D2_MAJORVER 0
#  define D2_MINORVER 0
#  define D2_PATCHLEVEL 0

   /* intended to be a private struct: */
   typedef struct _zip_ver {
    uch major;              /* e.g., integer 3 */
    uch minor;              /* e.g., 1 */
    uch patchlevel;         /* e.g., 0 */
    uch not_used;
   } _zip_version_type;

   /* this is what is returned for version information */
   /* NOTE:  This structure has been modified in Zip 3.1 */
   /* "max bytes" is size of string allocated in VB6 to receive value */
   typedef struct _ZpVer {
    ulg structlen;             /* length of the struct being passed */
    ulg flag;                  /* bit 0: is_beta   bit 1: uses_zlib */
    char *BetaLevel;           /* e.g., "g BETA" or "" (max 10) */
    char *Version;             /* e.g., "3.1d28" (max 20) */
    char *RevDate;             /* e.g., "4 Sep 95" (beta)/"4 September 1995" (max 20) */
    char *RevYMD;              /* e.g., "1995/09/04" (same as RevDate) (max 20) */
    char *zlib_Version;        /* e.g., "0.95" or NULL (max 10) */
    BOOL fEncryption;          /* TRUE if encryption enabled, FALSE if not */
                               /* the actual encryption methods available are
                                  given in the Features list */
    _zip_version_type zip;     /* the Zip version the DLL is compiled from */
    _zip_version_type os2dll;
    _zip_version_type libdll_interface;  /* updated when lib/dll interface
                                            changes */
    /* new */
    ulg opt_struct_size;       /* Expected size of the option structure */
    char *szFeatures;          /* Comma separated list of enabled features */
   } ZpVer;

   /* An application using the LIB or DLL should check libdll_interface.  If
      the version is less than expected, then some capabilities may be missing
      or the LIB or DLL may be incompatible.  We expect this to stay at 3.1 for
      the foreseeable future, meaning that features may be added, but in a
      backward compatible to 3.1 way.
    */

#  ifndef EXPENTRY
#   define EXPENTRY WINAPI
#  endif

#  ifdef ZIPLIB
#   define ZIPEXPENTRY
#  else
#   define ZIPEXPENTRY EXPENTRY
#  endif

#  ifndef DEFINED_ONCE
#   define DEFINED_ONCE
  typedef long (ZIPEXPENTRY DLLPRNT) (char *to_print,
                                      unsigned long print_len);

  typedef long (ZIPEXPENTRY DLLPASSWORD) (long bufsize,
                                          char *prompt,
                                          char *password);

    /* DLLPROGRESS:
       current file path, % all input bytes processed * 100 (100% = 10000),
       % this entry processed * 100, uncompressed size bytes, uncompressed
       size string, action */

  typedef long (ZIPEXPENTRY DLLPROGRESS) (char *file_name,
                                          char *unicode_file_name,
                                          long percent_all_done_x_100,
                                          long percent_entry_done_x_100,
                                          API_FILESIZE_T uncompressed_size,
                                          char *uncompressed_size_string,
                                          char *action,
                                          char *method,
                                          char *info);

    /* DLLERROR:
       called when ZipWarn or ZIPERR are called to return the warning or
       error message */
  typedef long (ZIPEXPENTRY DLLERROR) (char *errstring);

    /* DLLFINISH:
	   called at end of operation to return stats */
  typedef long (ZIPEXPENTRY DLLFINISH) (char *final_uncompressed_size_string,
                                        char *final_compressed_size_string,
                                        API_FILESIZE_T final_uncompressed_size,
                                        API_FILESIZE_T final_compressed_size,
                                        long final_percent);

  typedef long (ZIPEXPENTRY DLLSERVICE) (char *file_name,
                                         char *unicode_file_name,
                                         char *uncompressed_size_string,
                                         char *compressed_size_string,
                                         API_FILESIZE_T uncompressed_size,
                                         API_FILESIZE_T compressed_size,
                                         char *action,
                                         char *method,
                                         char *info,
                                         long percent);

  /* DLLSERVICE_NO_INT64 seems no longer needed (see examples) and may be
     deprecated soon. */
  typedef long (ZIPEXPENTRY DLLSERVICE_NO_INT64) (char *file_name,
                                                  char *unicode_file_name,
                                                  unsigned long uncompressed_size,
                                                  unsigned long compressed_size);

  typedef long (ZIPEXPENTRY DLLECOMMENT)(char *old_comment,
                                         char *file_name,
                                         char *unicode_file_name,
                                         long maxcomlen,
                                         long *newcomlen,
                                         char *new_comment);

  typedef long (ZIPEXPENTRY DLLACOMMENT)(char *old_comment,
                                         long maxcomlen,
                                         long *newcomlen,
                                         char *new_comment);
  /* do not use DLLSPLIT - not completed */
  typedef long (ZIPEXPENTRY DLLSPLIT) (char *, long *, char *);

# endif /* DEFINED_ONCE */

/* Structures */
/* This options structure should no longer be used. */

   typedef struct {        /* zip options */
    LPSTR ExcludeBeforeDate;/* Exclude if file date before this, or NULL */
    LPSTR IncludeBeforeDate;/* Include if file date before this, or NULL */
    LPSTR szRootDir;        /* Directory to use as base for zipping, or NULL */
    LPSTR szTempDir;        /* Temporary directory used during zipping, or NULL */
   /* BOOL fTemp;             Use temporary directory '-b' during zipping */
   /* BOOL fSuffix;           include suffixes (not implemented) */

    int  fUnicode;          /* Unicode flags (was "include suffixes", fMisc) */
    /*  Add values to set flags (currently 2 and 4 are exclusive)
          1 = (was include suffixes (not implemented), now not used)
          2 = no UTF8          Ignore UTF-8 information (except native)
          4 = native UTF8      Store UTF-8 as native character set
    */

    int  fEncrypt;          /* encrypt method (was "encrypt files") */
                            /* See zip.h for xxx_ENCRYPTION macros. */

    BOOL fSystem;           /* include system and hidden files */
    BOOL fVolume;           /* Include volume label */
    BOOL fExtra;            /* Exclude extra attributes */
    BOOL fNoDirEntries;     /* Do not add directory entries */
    BOOL fVerbose;          /* Mention oddities in zip file structure */
    BOOL fQuiet;            /* Quiet operation */
    BOOL fCRLF_LF;          /* Translate CR/LF to LF */
    BOOL fLF_CRLF;          /* Translate LF to CR/LF */
    BOOL fJunkDir;          /* Junk directory names */
    BOOL fGrow;             /* Allow appending to a zip file */
    BOOL fForce;            /* Make entries using DOS names (k for Katz) */
    BOOL fMove;             /* Delete files added or updated in zip file */
    BOOL fDeleteEntries;    /* Delete files from zip file */
    BOOL fUpdate;           /* Update zip file--overwrite only if newer */
    BOOL fFreshen;          /* Freshen zip file--overwrite only */
    BOOL fJunkSFX;          /* Junk SFX prefix */
    BOOL fLatestTime;       /* Set zip file time to time of latest file in it */
    BOOL fComment;          /* Put comment in zip file */
    BOOL fOffsets;          /* Update archive offsets for SFX files */
    BOOL fPrivilege;        /* Use privileges (WIN32 only) */
    BOOL fEncryption;       /* TRUE if encryption supported (compiled in), else FALSE.
                               this is a read only flag */
    LPSTR szSplitSize;      /* This string contains the size that you want to
                               split the archive into. i.e. 100 for 100 bytes,
                               2K for 2 k bytes, where K is 1024, m for meg
                               and g for gig. If this string is not NULL it
                               will automatically be assumed that you wish to
                               split an archive. */
    LPSTR szIncludeList;    /* Pointer to include file list string (for VB) */
    long IncludeListCount;  /* Count of file names in the include list array */
    char **IncludeList;     /* Pointer to include file list array. Note that the last
                               entry in the array must be NULL */
    LPSTR szExcludeList;    /* Pointer to exclude file list (for VB) */
    long ExcludeListCount;  /* Count of file names in the include list array */
    char **ExcludeList;     /* Pointer to exclude file list array. Note that the last
                               entry in the array must be NULL */
    int  fRecurse;          /* Recurse into subdirectories. 1 => -r, 2 => -R */
    int  fRepair;           /* Repair archive. 1 => -F, 2 => -FF */
    char fLevel;            /* Compression level (0 - 9) */
    LPSTR szCompMethod;     /* Compression method string (e.g. "bzip2"), or NULL */
   /* ProgressSize should be added as an option to support the zipmain() interface */

    LPSTR szProgressSize;  /* Bytes read in between progress reports (-ds format)
                               Set to NULL for no reports.  If used, must define
                               DLLPROGRESS. */

    long int fluff[8];      /* not used, for later expansion */
   } ZPOPT, _far *LPZPOPT;

   typedef struct {
     int  argc;            /* Count of files to zip */
     LPSTR lpszZipFN;      /* name of archive to create/update */
     char **FNV;           /* array of file names to zip up */
     LPSTR lpszAltFNL;     /* pointer to a string containing a list of file
                              names to zip up, separated by whitespace. Intended
                              for use only by VB users, all others should set this
                              to NULL. */
   } ZCL, _far *LPZCL;

   /* The size and layout of the function callback structure now fixed.  Note
      that some platforms depend on this order, so don't change it. */
   typedef struct {
     /* pass any STDOUT and STDERR output to caller */
     DLLPRNT *print;
     /* query for an entry comment */
     DLLECOMMENT *ecomment;
     /* query for the archive comment */
     DLLACOMMENT *acomment;
     /* query for the password */
     DLLPASSWORD *password;

     /* get split destination - NOT IMPLEMENTED */
     DLLSPLIT *split;      /* This MUST be set to NULL unless you want to be
                              queried for a destination for each split
                              of the archive. */

     /* The service callback is called once for each entry to "service" the
        entry.  For "service", file sizes MUST be 64-bit for the DLL, but can
        be port specific for the LIB (e.g. they are 32-bit on VAX).  The
        "service_no_int64" breaks sizes into high and low parts for ports
        that have large files but no 64-bit variables (such as old VB 6
        apps; however, the VB 6 example shows that this is not needed, so
        "service" should always be used and "service_no_int64" is now
        deprecated. */
     DLLSERVICE *service;                      /* "64-bit" version */
     DLLSERVICE_NO_INT64 *service_no_int64;    /* 32-bit version */

     /* called at start of entry, every so many bytes during zipping, and at
        end of entry */
     DLLPROGRESS *progress;
     /* called when zipwarn() or ZIPERR() are called to report warning or
        error */
     DLLERROR *error;
     /* called at end of zip operation to pass stats to caller */
     DLLFINISH *finish;
   } ZIPUSERFUNCTIONS, far * LPZIPUSERFUNCTIONS;

  extern LPZIPUSERFUNCTIONS lpZipUserFunctions;


  /* structure used to test DLL interface */
  typedef struct {
     DLLPRNT *print;
   } ZpTESTFUNCTION, far * LPZpTESTFUNCTION;


#  if 0
#  ifndef __cplusplus
    void  EXPENTRY ZpVersion(ZpVer far *);
    int   EXPENTRY ZpInit(LPZIPUSERFUNCTIONS lpZipUserFunc);
    int   EXPENTRY ZpArchive(ZCL C, LPZPOPT Opts);
#  endif
#  endif /* 0 */

#  if defined(ZIPLIB) || defined(COM_OBJECT)
#   define ydays zp_ydays
#  endif


/* Functions not yet supported */
#  if 0
    int      EXPENTRY ZpMain            (int argc, char **argv);
    int      EXPENTRY ZpAltMain         (int argc, char **argv, ZpInit *init);
#  endif

#  ifdef ZIPDLL
    /* These are replaced by zprintf(), zfprintf(), and zperror(). */
#  if 0
#   define printf  ZPprintf
#   define fprintf ZPfprintf
#   define perror  ZPperror

    extern int __far __cdecl printf(const char *format, ...);
    extern int __far __cdecl fprintf(FILE *file, const char *format, ...);
    extern void __far __cdecl perror(const char *);
#  endif
#  endif /* ZIPDLL */


#if 0
    typedef struct {
      long a;
      char *s;
      unsigned char b[10];
      char s10[10];
    } teststruc;
#endif

/* Interface function prototypes. */
#  ifdef __cplusplus
    extern "C"
    {
#  endif /* def __cplusplus */

  int   ZIPEXPENTRY ZpTestComParse(char *commandline, char *parsedline);
  void  ZIPEXPENTRY ZpTestCallback(long cbAddress);
  void  ZIPEXPENTRY ZpTestCallbackStruct(LPZpTESTFUNCTION lpTestFuncStruct);
  
  int   ZIPEXPENTRY ZpZipTest(char *commandline, char *CurrentDir,
                              LPZIPUSERFUNCTIONS lpZipUserFunc);
  int   ZIPEXPENTRY ZpZip(char *commandline, char *CurrentDir,
                          LPZIPUSERFUNCTIONS lpZipUserFunc,
                          char *ProgressChunkSize);
  
  int   ZIPEXPENTRY ZpStringCopy(char *deststring, char *sourcestring,
                                 int maxlength);
  
  void  ZIPEXPENTRY ZpVersion(ZpVer far *);
  int   ZIPEXPENTRY ZpInit(LPZIPUSERFUNCTIONS lpZipUserFunc);
  int   ZIPEXPENTRY ZpArchive(ZCL C, LPZPOPT Opts);

#  ifdef __cplusplus
    }
#  endif


#if defined(ZIPLIB) || defined(ZIPDLL)
  extern LPSTR szCommentBuf;
  extern HANDLE hStr;

  void acomment(long);
  void ecomment(struct zlist far *z);
#endif

#if 0
#  ifndef WINDLL

/* windll.[ch] stuff for non-Windows systems. */

extern LPSTR szCommentBuf;
extern HANDLE hStr;

void comment(unsigned short);

#  endif /* ndef WINDLL */
#endif

# endif /* ZIPDLL || ZIPLIB */


#endif /* _ZIPAPI_H */
