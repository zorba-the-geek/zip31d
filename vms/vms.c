/*
  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
 *  vms.c (zip) by Igor Mandrichenko    Version 2.2-2
 *
 *  Revision history:
 *  ...
 *  2.2-2       18-jan-1993     I.Mandrichenko
 *      vms_stat() added - version of stat() that handles special
 *      case when end-of-file-block == 0
 *
 *  3.0         11-oct-2004     SMS
 *      It would be nice to know why vms_stat() is needed.  If EOF can't
 *      be trusted for a zero-length file, why trust it for any file?
 *      Anyway, I removed the (int) cast on ->st_size, which may now be
 *      bigger than an int, just in case this code ever does get used.
 *      (A true zero-length file should still report zero length, even
 *      after the long fight with RMS.)
 *      Moved the VMS_PK_EXTRA test(s) into VMS_IM.C and VMS_PK.C to
 *      allow more general automatic dependency generation.
 *
 *  3.1         10-jun-2012     SMS
 *      Added (and changed to use) vms_status().
 *              23-apr-2011     SMS
 *      Added entropy_fun() for AES encryption.
 *              17-nov-2011     SMS
 *      Added establish_ctrl_t() for user-triggered progress reports.
 *              05-dec-2011     SMS
 *      Added vms_fopen() to solve %rms-w-rtb (or %rms-e-netbts for
 *      DECnet access) errors when zipping Record format: Stream or
 *      Stream_CR files (without "-V[V]").
 *              08-may-2012     SMS
 *      Changed to write all "-vv" diagnostic messages to "mesg" instead
 *      of "stderr", and added a "\n" prefix to the vms_fopen() message
 *      to avoid overwritten and over-long "-vv" messages.
 *
 *              2015-03-23      SMS
 *      Added vms_getcwd() (because the CRTL getcwd() fails when the
 *      size argument is zero).
 */

#ifdef VMS                      /* For VMS only ! */

#define NO_ZIPUP_H              /* Prevent full inclusion of vms/zipup.h. */

#include "zip.h"
#include "zipup.h"              /* Only partial. */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unixlib.h>            /* getpid().  (<unistd.h> is too new.) */

#include <dcdef.h>
#include <descrip.h>
#include <dvidef.h>
#include <fab.h>                /* Needed only in old environments. */
#include <iodef.h>
#include <jpidef.h>
#include <lib$routines.h>
#include <nam.h>                /* Needed only in old environments. */
#include <starlet.h>
#include <ssdef.h>
#include <stsdef.h>
#include <syidef.h>

#ifdef ENABLE_USER_PROGRESS
# include <prdef.h>
/* Last-ditch attempt to get PR$C_PS_USER defined. */
# ifndef PR$C_PS_USER
#  define PR$C_PS_USER 3
# endif /* ndef PR$C_PS_USER */
#endif /* def ENABLE_USER_PROGRESS */

/* On VAX, define Goofy VAX Type-Cast to obviate /standard = vaxc.
   Otherwise, lame system headers on VAX cause compiler warnings.
   (GNU C may define vax but not __VAX.)
*/
#ifdef vax
# define __VAX 1
#endif /* def vax */

#ifdef __VAX
# define GVTC (unsigned int)
#else /* def __VAX */
# define GVTC
#endif /* def __VAX */

#define DIAG_FLAG (verbose >= 2)


#ifdef UTIL

/* For utilities, include only vms.h, as either of the vms_XX.c files
 * would do.
 */

# include "vms.h"

#else /* def UTIL */

/* Include the `VMS attributes' preserving file-io code. We distinguish
   between two incompatible flavours of storing VMS attributes in the
   Zip archive:
   a) The "PKware" style follows the extra field specification for
      PKware's VMS Zip.
   b) The "IM (Info-ZIP)" flavour was defined from scratch by
      Igor Mandrichenko. This version has be used in official Info-ZIP
      releases for several years and is known to work well.
 */

/* Note that only one of these #include directives will include any
 * active code, depending on VMS_PK_EXTRA.  Both are included here (and
 * tested there) to allow more general automatic dependency generation.
 */

# include "vms_pk.c"
# include "vms_im.c"

#endif /* def UTIL [else] */

#ifndef ERR
#define ERR(x) (((x)&1)==0)
#endif

#ifndef NULL
#define NULL (void*)(0L)
#endif


/* 2011-12-04 SMS.
 *
 *       vms_fopen().
 *
 *    VMS-specific jacket for fopen().
 *
 * Formerly, zopen() was defined in [.vms]zipup.h, so:
 *   #define zopen(n,p)   (vms_native?vms_open(n)    :(ftype)fopen((n), p))
 * so, when vms_native was zero, fopen() was used directly (with the
 * "fhow" value which was also defined in [.vms]zipup.h).  This caused
 * problems ("%rms-w-rtb, !ul byte record too large for user's buffer")
 * for files with Record Format: Stream or Stream_CR (but,
 * interestingly, not Stream_LF).
 *
 * Now, zopen() is defined in [.vms]zipup.h, so:
 *   #define zopen(n,p)   (vms_native?vms_open(n)    :vms_fopen(n))
 * so vms_fopen() (below) is used to specify the exotic arguments for
 * fopen() (and "fhow" is ignored).
 *
 * vms_fopen() uses stat() to get the record format of a file before
 * opening it.  If stat() works, and the record format is one of the
 * Stream types, then the file is opened in stream mode ("ctx = stm").
 * This seems to solve the %RMS-W-RTB problem.
 *
 * Changed old non-DEC-C "mbc=60" to DEC-C-default-like "mbc=127".
 */

#ifdef __DECC
# define FOPEN_ARGS , "acc", acc_cb, &fhow_id
#else /* def __DECC */          /* (So, GNU C, VAX C, ...)*/
# define FOPEN_ARGS , "mbc=127"
#endif /* def __DECC [else] */

FILE *vms_fopen( char *file_spec)
{
    int sts;
    struct stat stat_buf;

    sts = stat( file_spec, &stat_buf);

    if (DIAG_FLAG)
    {
        fprintf( mesg,
         "\nvms_fopen(): stat() = %d, rfm = %d.\n",
         sts, stat_buf.st_fab_rfm);
    }

    if ((sts == 0) &&
     ((stat_buf.st_fab_rfm == FAB$C_STM) ||
     (stat_buf.st_fab_rfm == FAB$C_STMCR) ||
     (stat_buf.st_fab_rfm == FAB$C_STMLF)))
    {
        /* Use stream-mode access ("ctx = stm") for Stream[_xx] files. */
        return fopen( file_spec, "r", "ctx = stm" FOPEN_ARGS);
    }
    else
    {
        return fopen( file_spec, "r" FOPEN_ARGS);
    }
}


/*
 *       vms_stat().
 */
int vms_stat( char *file, stat_t *s)
{
    int status;
    int staterr;
    struct FAB fab;
    struct NAMX_STRUCT nam;
    struct XABFHC fhc;

    /*
     *  In simplest case when stat() returns "ok" and file size is
     *  nonzero or this is directory, finish with this
     */

    if( (staterr=stat(file,s)) == 0
        && ( s->st_size >= 0                    /* Size - ok */
             || (s->st_mode & S_IFREG) == 0     /* Not a plain file */
           )
    ) return staterr;

    /*
     *  Get here to handle the special case when stat() returns
     *  invalid file size. Use RMS to compute the size.
     *  When EOF block is zero, set file size to its physical size.
     *  One more case to get here is when this is remote file accessed
     *  via DECnet.
     */

    fab = cc$rms_fab;
    nam = CC_RMS_NAMX;
    fhc = cc$rms_xabfhc;
    fab.FAB_NAMX = &nam;
    fab.fab$l_xab = (char*)(&fhc);

    NAMX_DNA_FNA_SET(fab)
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNA = file;
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNS = strlen( file);

    fab.fab$b_fac = FAB$M_GET;

    status = sys$open(&fab);
    fab.fab$l_xab = (char*)0L;
    sys$close(&fab);

    if( !ERR(status) )
    {
        if( fhc.xab$l_ebk > 0 )
            s->st_size = ( fhc.xab$l_ebk-1 ) * 512 + fhc.xab$w_ffb;
        else if( fab.fab$b_org == FAB$C_IDX
                 || fab.fab$b_org == FAB$C_REL
                 || fab.fab$b_org == FAB$C_HSH )
                /* Special case, when ebk=0: save entire allocated space */
                    s->st_size = fhc.xab$l_hbk * 512;
        else
            s->st_size = fhc.xab$w_ffb;
        return 0; /* stat() success code */
    }
    else
        return status;
}


/*
 * 2007-01-29 SMS.
 *
 *  VMS Status Code Summary  (See STSDEF.H for details.)
 *
 *      Bits:   31:28    27:16     15:3     2:0
 *      Field:  Control  Facility  Message  Severity
 *
 *  In the Control field, bits 31:29 are reserved.  Bit 28 inhibits
 *  printing the message.  In the Facility field, bit 27 means
 *  customer-defined (not HP-assigned, like us).  In the Message field,
 *  bit 15 means facility-specific (which our messages are).  The
 *  Severity codes are 0 = Warning, 1 = Success, 2 = Error, 3 = Info,
 *  4 = Severe (fatal).
 *
 *  Zip versions before 3.0 used a generic ("chosen by
 *  experimentation)") Control+Facility code of 0x7FFF, which included
 *  some reserved control bits, the inhibit-printing bit, and the
 *  customer-defined bit.
 *
 *  HP has now assigned official Facility names and corresponding
 *  Facility codes for the Info-ZIP products:
 *
 *      Facility Name    Facility Code
 *      IZ_UNZIP         1954 = 0x7A2
 *      IZ_ZIP           1955 = 0x7A3
 *
 *  Now, unless the CTL_FAC_IZ_ZIP macro is defined at build-time, we
 *  will use the official Facility code.
 */

/* Official HP-assigned Info-ZIP Zip Facility code. */
#define FAC_IZ_ZIP 1955   /* 0x7A3 */

#ifndef CTL_FAC_IZ_ZIP
   /*
    * Default is inhibit-printing with the official Facility code.
    */
#  define CTL_FAC_IZ_ZIP ((0x1 << 12)| FAC_IZ_ZIP)
#  define MSG_FAC_SPEC 0x8000   /* Facility-specific code. */
#else /* ndef CTL_FAC_IZ_ZIP */
   /* Use the user-supplied Control+Facility code for err or warn. */
#  define OLD_STATUS
#  ifndef MSG_FAC_SPEC          /* Old default is not Facility-specific. */
#    define MSG_FAC_SPEC 0x0    /* Facility-specific code.  Or 0x8000. */
#  endif /* ndef MSG_FAC_SPEC */
#endif /* ndef CTL_FAC_IZ_ZIP [else] */


/* Translate a ZE_xxx code to the corresponding VMS status value. */

int vms_status( int err)
{
    int sts;

    /* 2013-06-13 SMS.
     * Normal behavior is to return SS$_NORMAL for success (ZE_OK), or a
     * status value with an official Facility code for any other result.
     * Define OK_USE_FAC to return a success code with a Facility code
     * (instead of SS$_NORMAL).
     *
     * Raw status codes are effectively multiplied by two ("<< 4"
     * instead of "<< 3"), which makes them easier to read in a
     * hexadecimal representation of the VMS status value.  For example,
     * PK_PARAM = 10 (0x0a) -> %x17A280A2.
     *                                ^^
     *
     * Zip versions before 3.0 (before an official Facility code was
     * assigned) used 0x7FFF instead of the official Facility code.
     * Define CTL_FAC_IZ_UZP as 0x7FFF (and/or MSG_FAC_SPEC) to get the
     * old behavior.  (See above.)
     */

#ifdef OK_USE_FAC

    /* Always return a status code comprising Control, Facility,
     * Message, and Severity.
     */
    sts = (CTL_FAC_IZ_ZIP << 16) |              /* Facility                */
          MSG_FAC_SPEC |                        /* Facility-specific (+?)  */
          (err << 4) |                          /* Message code            */
          (ziperrors[ err].severity & 0x07);    /* Severity                */

#else /* def OK_USE_FAC */

    /* Return simple SS$_NORMAL for ZE_OK.  Otherwise, return a status
     * code comprising Control, Facility, Message, and Severity.
     */
    sts = (err == ZE_OK) ? SS$_NORMAL :         /* Success (others below)  */
          ((CTL_FAC_IZ_ZIP << 16) |             /* Facility                */
          MSG_FAC_SPEC |                        /* Facility-specific (+?)  */
          (err << 4) |                          /* Message code            */
          (ziperrors[ err].severity & 0x07));   /* Severity                */

#endif /* ndef OLD_STATUS [else] */

    return sts;
}


/* Exit with an intelligent status/severity code. */

/* Declare __posix_exit() if <stdlib.h> won't, and we use it. */

#if __CRTL_VER >= 70000000 && !defined(_POSIX_EXIT)
#  if !defined( NO_POSIX_EXIT)
void     __posix_exit     (int __status);
#  endif /* !defined( NO_POSIX_EXIT) */
#endif /* __CRTL_VER >= 70000000 && !defined(_POSIX_EXIT) */


void vms_exit( int err)
{
#if !defined( NO_POSIX_EXIT) && (__CRTL_VER >= 70000000)

    /* If the environment variable "SHELL" is defined, and not defined
     * as "DCL" (by GNV "bash", for example), then use __posix_exit() to
     * exit with the raw, UNIX-like status code.
     */
    char *sh_ptr;

    sh_ptr = getenv( "SHELL");
    if ((sh_ptr != NULL) && strcasecmp( sh_ptr, "DCL"))
    {
        __posix_exit( err);
    }
    else

#endif /* #if !defined( NO_POSIX_EXIT) && (__CRTL_VER >= 70000000) */

    {
        exit( vms_status( err));
    }
}


/***************************/
/*  Function vms_getcwd()  */
/***************************/

/* 2015-03-23 SMS.
 * As of __VMS_VER = 80400022 and __CRTL_VER = 80400000, getcwd() fails
 * if the size argument is 0.  Duh.
 * styl: 0 for UNIX-style result; 1 for VMS-style result.
 */

char *vms_getcwd( char *bf, size_t sz, int styl)
{
    if ((bf == NULL) || (sz == 0))
    {                                   /* We're expected to malloc storage. */
        sz = NAMX_MAXRSS;               /* Size of the max-len file spec. */
        bf = izz_malloc( sz+ 1);        /* Allocate the (adequate) storage. */
    }
    if (bf != NULL)
    {
        bf = getcwd( bf, sz, styl);     /* Use the lame getcwd(). */
    }

    return bf;
}


/******************************/
/*  Function version_local()  */
/******************************/

void version_local()
{
    static ZCONST char CompiledWith[] = "Compiled with %s%s for %s%s%s%s.\n\n";
#ifdef VMS_VERSION
    char *chrp1;
    char *chrp2;
    char buf[40];
    char vms_vers[ 16];
    int ver_maj;
#endif
#ifdef __DECC_VER
    char buf2[40];
    int  vtyp;
#endif

#ifdef VMS_VERSION
    /* Truncate the version string at the first (trailing) space. */
    strncpy( vms_vers, VMS_VERSION, sizeof( vms_vers));
    chrp1 = strchr( vms_vers, ' ');
    if (chrp1 != NULL)
        *chrp1 = '\0';

    /* Determine the major version number. */
    ver_maj = 0;
    chrp1 = strchr( &vms_vers[ 1], '.');
    for (chrp2 = &vms_vers[ 1];
     chrp2 < chrp1;
     ver_maj = ver_maj* 10+ *(chrp2++)- '0');

#endif /* def VMS_VERSION */

/*  DEC C in ANSI mode does not like "#ifdef MACRO" inside another
    macro when MACRO is equated to a value (by "#define MACRO 1").   */

    printf(CompiledWith,

#ifdef __GNUC__
      "gcc ", __VERSION__,
#else
#  if defined(DECC) || defined(__DECC) || defined (__DECC__)
      "DEC C",
#    ifdef __DECC_VER
      (sprintf(buf2, " %c%d.%d-%03d",
               ((vtyp = (__DECC_VER / 10000) % 10) == 6 ? 'T' :
                (vtyp == 8 ? 'S' : 'V')),
               __DECC_VER / 10000000,
               (__DECC_VER % 10000000) / 100000, __DECC_VER % 1000), buf2),
#    else
      "",
#    endif
#  else
#  ifdef VAXC
      "VAX C", "",
#  else
      "unknown compiler", "",
#  endif
#  endif
#endif

#ifdef VMS_VERSION
#  if defined( __alpha)
      "OpenVMS",
      (sprintf( buf, " (%s Alpha)", vms_vers), buf),
#  elif defined( __ia64) /* defined( __alpha) */
      "OpenVMS",
      (sprintf( buf, " (%s IA64)", vms_vers), buf),
#  else /* defined( __alpha) */
      (ver_maj >= 6) ? "OpenVMS" : "VMS",
      (sprintf( buf, " (%s VAX)", vms_vers), buf),
#  endif /* defined( __alpha) */
#else
      "VMS",
      "",
#endif /* def VMS_VERSION */

#if defined( __DATE__) && !defined( NO_BUILD_DATE)
      " on ", __DATE__
#else
      "", ""
#endif
      );

} /* end function version_local() */

/* 2004-10-08 SMS.
 *
 *       tempname() for VMS.
 *
 *    Generate a temporary Zip archive file name, near the actual
 *    destination Zip archive file, or at "tempath", if specified.
 *
 *    Using sys$parse() is probably more work than it's worth, but it
 *    should also be ODS5-safe.
 *
 *    Note that the generic method using tmpnam() (in FILEIO.C)
 *    produces "ziXXXXXX", where "XXXXXX" is the low six digits of the
 *    decimal representation of the process ID.  This method produces
 *    "ZIxxxxxxxx", where "xxxxxxxx" is the (whole) eight-digit
 *    hexadecimal representation of the process ID.  More important, it
 *    actually uses the directory part of the argument or "tempath".
 *
 * 2010-09-29 SMS.
 *    Well, duh.  Split archives need more than one temporary file name,
 *    and a PID-only method can't achieve that.  The new method retains
 *    the PID base, but adds a (hexadecimal) serial number as the
 *    ".type", producing "ZIxxxxxxxx.yyyyyyyy;", where "xxxxxxxx" is the
 *    hexadecimal PID, and "yyyyyyyy" is the hexadecimal serial number.
 *    This should still be safe on ODS2 or ODS5.
 */

char *tempname( char *zip)
/* char *zip; */                /* Path name of Zip archive. */
{
    char *temp_name;            /* Return value. */
    int sts;                    /* System service status. */

    static int pid;             /* Process ID. */
    static int pid_len;         /* Returned size of process ID. */
    static int serial = 0;      /* Serial number, for certain uniqueness. */

    struct                      /* Item list for GETJPIW. */
    {
        short buf_len;          /* Buffer length. */
        short itm_cod;          /* Item code. */
        int *buf;               /* Buffer address. */
        int *ret_len;           /* Returned length. */
        int term;               /* Item list terminator. */
    } jpi_itm_lst = { sizeof( pid), JPI$_PID, &pid, &pid_len };

    /* ZI<UNIQUE> name storage. */
    static char zip_tmp_nam[ 24] = "ZI<pid|time>.<serial>;";

    struct FAB fab;             /* FAB structure. */
    struct NAMX_STRUCT nam;     /* NAM[L] structure. */

    char exp_str[ NAMX_MAXRSS+ 1];      /* Expanded name storage. */

#ifdef VMS_UNIQUE_TEMP_BY_TIME

    /* Use alternate time-based scheme to generate a unique temporary name. */
    sprintf( &zip_tmp_nam[ 2], "%08X.%08X", time( NULL), serial);

#else /* def VMS_UNIQUE_TEMP_BY_TIME */

    /* Use the process ID to generate a unique temporary name. */
    sts = sys$getjpiw( 0, 0, 0, &jpi_itm_lst, 0, 0, 0);
    sprintf( &zip_tmp_nam[ 2], "%08X.%08X", pid, serial);

#endif /* def VMS_UNIQUE_TEMP_BY_TIME */

    /* Increment the serial number. */
    serial++;

    /* Smoosh the unique temporary name against the actual Zip archive
       name (or "tempath") to create the full temporary path name.
    */
    if (tempath != NULL)        /* Use "tempath", if it's been specified. */
        zip = tempath;

    /* Initialize the FAB and NAM[L], and link the NAM[L] to the FAB. */
    fab = cc$rms_fab;
    nam = CC_RMS_NAMX;
    fab.FAB_NAMX = &nam;

    /* Point the FAB/NAM[L] fields to the actual name and default name. */
    NAMX_DNA_FNA_SET(fab)

    /* Default name = Zip archive name. */
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_DNA = zip;
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_DNS = strlen( zip);

    /* File name = "ZI<unique>.<serial>;". */
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNA = zip_tmp_nam;
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNS = strlen( zip_tmp_nam);

    nam.NAMX_ESA = exp_str;     /* Expanded name (result) storage. */
    nam.NAMX_ESS = NAMX_MAXRSS; /* Size of expanded name storage. */

    nam.NAMX_NOP = NAMX_M_SYNCHK; /* Syntax-only analysis. */

    temp_name = NULL;           /* Prepare for failure (unlikely). */
    sts = sys$parse( &fab, 0, 0);       /* Parse the name(s). */

    if ((sts& STS$M_SEVERITY) == STS$M_SUCCESS)
    {
        /* Truncate/NUL-terminate the resulting file spec. */
        /* 2010-09-29 SMS.
         * Truncate at version, with ".<serial>"; at type, without.
         * Was:
         * strcpy( nam.NAMX_L_TYPE, ".;");
         * Now:
         */
        *(nam.NAMX_L_VER+ 1) = '\0';

        /* Allocate temp name storage (as caller expects), and copy the
           (truncated/terminated) temp name into the new location.
        */
        temp_name = izz_malloc( strlen( nam.NAMX_ESA)+ 1);

        if (temp_name != NULL)
        {
            strcpy( temp_name, nam.NAMX_ESA);
        }
    }
    return temp_name;
} /* tempname() for VMS. */


/* 2005-02-17 SMS.
 *
 *       ziptyp() for VMS.
 *
 *    Generate a real Zip archive file name (exact, if it exists), using
 *    a default file name.
 *
 *    2005-02-17 SMS.  Moved to here from [-]ZIPFILE.C, to segregate
 *    better the RMS stuff.
 *
 *    Before 2005-02-17, if sys$parse() failed, ziptyp() returned a null
 *    string ("&zero", where "static char zero = '\0';").  This
 *    typically caused Zip to proceed, but then the final rename() of
 *    the temporary archive would (silently) fail (null file name, after
 *    all), leaving only the temporary archive file, and providing no
 *    warning message to the victim.  Now, when sys$parse() fails,
 *    ziptyp() returns the original string, so a later open() fails, and
 *    a relatively informative message is provided.  (A VMS-specific
 *    message could also be provided here, if desired.)
 *
 *    2005-09-16 SMS.
 *    Changed name parsing in ziptyp() to solve a problem with a
 *    search-list logical name device-directory spec for the zipfile.
 *    Previously, when the zipfile did not exist (so sys$search()
 *    failed), the expanded name was used, but as it was
 *    post-sys$search(), it was based on the _last_ member of the search
 *    list instead of the first.  Now, the expanded name from the
 *    original sys$parse() (pre-sys$search()) is retained, and it is
 *    used if sys$search() fails.  This name is based on the first
 *    member of the search list, as a user might expect.
 */

/* Default Zip archive file spec. */
#define DEF_DEVDIRNAM "SYS$DISK:[].zip"

char *ziptyp( char *s)
{
    int status;
    int exp_len;
    struct FAB fab;
    struct NAMX_STRUCT nam;
    char result[ NAMX_MAXRSS+ 1];
    char exp[ NAMX_MAXRSS+ 1];
    char *p;

    fab = cc$rms_fab;                   /* Initialize FAB. */
    nam = CC_RMS_NAMX;                  /* Initialize NAM[L]. */
    fab.FAB_NAMX = &nam;                /* FAB -> NAM[L] */

    /* Point the FAB/NAM[L] fields to the actual name and default name. */
    NAMX_DNA_FNA_SET(fab)

    /* Argument file name and length. */
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNA = s;
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNS = strlen( s);

    /* Default file spec and length. */
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_DNA = DEF_DEVDIRNAM;
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_DNS = sizeof( DEF_DEVDIRNAM)- 1;

    nam.NAMX_ESA = exp;                 /* Expanded name, */
    nam.NAMX_ESS = NAMX_MAXRSS;         /* storage size. */
    nam.NAMX_RSA = result;              /* Resultant name, */
    nam.NAMX_RSS = NAMX_MAXRSS;         /* storage size. */

    status = sys$parse(&fab);
    if ((status & 1) == 0)
    {
        /* Invalid file name.  Return (re-allocated) original, and hope
           for a later error message.
        */
        if ((p = izz_malloc( strlen( s)+ 1)) != NULL )
        {
            strcpy( p, s);
        }
        return p;
    }

    /* Save expanded name length from sys$parse(). */
    exp_len = nam.NAMX_ESL;

    /* Leave expanded name as-is, in case of search failure. */
    nam.NAMX_ESA = NULL;                /* Expanded name, */
    nam.NAMX_ESS = 0;                   /* storage size. */

    status = sys$search(&fab);
    if (status & 1)
    {   /* Zip file exists.  Use resultant (complete, exact) name. */
        if ((p = izz_malloc( nam.NAMX_RSL+ 1)) != NULL )
        {
            result[ nam.NAMX_RSL] = '\0';
            strcpy( p, result);
        }
    }
    else
    {   /* New Zip file.  Use pre-search expanded name. */
        if ((p = izz_malloc( exp_len+ 1)) != NULL )
        {
            exp[ exp_len] = '\0';
            strcpy( p, exp);
        }
    }
    return p;
} /* ziptyp() for VMS. */


/* 2005-12-30 SMS.
 *
 *       vms_file_version().
 *
 *    Return the ";version" part of a VMS file specification.
 */

char *vms_file_version( char *s)
{
    int status;
    struct FAB fab;
    struct NAMX_STRUCT nam;
    char *p;

    static char exp[ NAMX_MAXRSS+ 1];   /* Expanded name storage. */


    fab = cc$rms_fab;                   /* Initialize FAB. */
    nam = CC_RMS_NAMX;                  /* Initialize NAM[L]. */
    fab.FAB_NAMX = &nam;                /* FAB -> NAM[L] */

    /* Point the FAB/NAM[L] fields to the actual name and default name. */
    NAMX_DNA_FNA_SET(fab)

    /* Argument file name and length. */
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNA = s;
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNS = strlen( s);

    nam.NAMX_ESA = exp;                 /* Expanded name, */
    nam.NAMX_ESS = NAMX_MAXRSS;         /* storage size. */

    nam.NAMX_NOP = NAMX_M_SYNCHK;       /* Syntax-only analysis. */

    status = sys$parse(&fab);

    if ((status & 1) == 0)
    {
        /* Invalid file name.  Return "". */
        exp[ 0] = '\0';
        p = exp;
    }
    else
    {
        /* Success.  NUL-terminate, and return a pointer to the ";" in
           the expanded name storage buffer.
        */
        p = nam.NAMX_L_VER;
        p[ nam.NAMX_B_VER] = '\0';
    }
    return p;
} /* vms_file_version(). */


# ifdef IZ_CRYPT_AES_WG

/* Entropy gathering for VMS.  We use the system time, some memory usage
 * and process I/O statistics, the process CPU time, the process ID, and
 * then we smoosh together bits from operation counts of every device on
 * the system.
 */

/* $DEVICE_SCAN parameters. */
static unsigned short dev_name_ret_len;         /* Returned name length. */
static unsigned int dev_scan_ctx[ 2] =          /* Context. */
     { 0, 0 };
static char dev_name_ret[ 65];                  /* Returned name storage. */
static struct dsc$descriptor_s                  /* Returned name descriptor. */
 dev_name_ret_descr =
 { ((sizeof dev_name_ret)- 1), DSC$K_DTYPE_T, DSC$K_CLASS_S, dev_name_ret };

/* Devices to consider.  (That is, all of them.) */
static $DESCRIPTOR( dev_name_descr, "*");

/* 2011-05-01 SMS.
 *
 *       device_opcnt_bits().
 *
 *    Fill the user's (32-bit) buffer with a non-tiny operation-count
 *    value from each device on the system, in turn.
 *    Return the number of bits.  If negative, then we've run out of
 *    devices to check, and the next call will begin a new scan (of the
 *    same old devices, most likely).
 */

int device_opcnt_bits( unsigned int *ui)
{
    unsigned int os_info;
    unsigned int os_infos;
    int bits;
    int shft;
    int sts;

    sts = SS$_NORMAL;
    bits = -1;
    while (((sts& STS$M_SEVERITY) == STS$K_SUCCESS) && (bits < 0))
    {
        /* Get the next device name. */
        sts = sys$device_scan( &dev_name_ret_descr, /* Returned name. */
                               &dev_name_ret_len,   /* Returned name length. */
                               &dev_name_descr,     /* Search name. */
                               0,                   /* Selection item list. */
                               dev_scan_ctx);       /* Search context. */

        if ((sts& STS$M_SEVERITY) == STS$K_SUCCESS)
        {
            /* Get the device operation count. */
            sts = lib$getdvi( &((int) DVI$_OPCNT),  /* Item code. */
                              0,                    /* Channel. */
                              &dev_name_ret_descr,  /* Device name. */
                              &os_info,             /* Result buffer (int). */
                              0,                    /* Result string. */
                              0);                   /* Result string len. */

            if ((sts& STS$M_SEVERITY) == STS$K_SUCCESS)
            {
                /* Ignore tiny values. */
                if (os_info > 15)
                {
                    /* Count the useful bits. */
                    os_infos = os_info;
                    for (bits = 2; (os_infos >>= 2) != 0; bits += 2);
                    shft = 32- bits;
                }
            }
        }
        else
        {
            /* DEBICE_SCAN failed.  Reset the context for the next round. */
            dev_scan_ctx[ 0] = 0;
            dev_scan_ctx[ 1] = 0;
        }
    }

    if (ui != NULL)
    {
        /* Store the result in the user's buffer. */
        *ui = os_info;
    }

    /* Return the number of good bits. */
    return bits;
}


/* 2011-05-01 SMS.
 *
 *       device_opcnt_32().
 *
 *    Fill the user's (32-bit) buffer with device-operation-count
 *    values, smooshed together.
 *    Return the number of bits.  If less than 32, then we've run out of
 *    devices to check, and the next call will begin a new scan (of the
 *    same old devices, most likely).
 */

static unsigned int junk_acc = 0;       /* 32-bit junk accumulator. */
static int junk_acc_bit_cnt = 0;        /* Good (high) bits in the j_acc. */

int device_opcnt_32( unsigned int *ui)
{
    int junk_bit_cnt;
    unsigned int junk;
    unsigned int out_buf;
    int ret_bits;
    int shft;
    int sts;

    junk_bit_cnt = 0;
    ret_bits = -1;

    while ((junk_acc_bit_cnt < 32) && (junk_bit_cnt >= 0))
    {
        junk_bit_cnt = device_opcnt_bits( &junk);
        if (junk_bit_cnt > 0)
        {
            shft = 32- junk_acc_bit_cnt- junk_bit_cnt;
            if (shft > 0)
            {   /* Too few bits.  Shift and OR in what's available. */
                junk_acc |= (junk << shft);
            }
            else
            {
                /* Enough bits.  Or in what's needed.  Keep residue. */
                out_buf = junk_acc| (junk >> (-shft));
                junk_acc = junk << (32+ shft);
            }
            junk_acc_bit_cnt += junk_bit_cnt;
        }
    }

    /* Reduce junk_acc_bit_cnt after filling 32 bits. */
    if (junk_acc_bit_cnt >= 32)
    {
        ret_bits = 32;
        junk_acc_bit_cnt -= 32;
    }
    else
    {
        /* Ran out of bits. */
        ret_bits = junk_acc_bit_cnt;
    }

    if (ui != NULL)
    {
        /* Store the result in the user's buffer. */
        *ui = out_buf;
    }

    /* Return the number of good bits. */
    return ret_bits;
}


/* 2011-04-21 SMS.
 *
 *       entropy_fun().
 *
 *    Fill the user's buffer with up to 8 bytes of stuff.
 */

int entropy_fun( unsigned char *buf, unsigned int len)
{
    union
    {
        unsigned char b[ 8];
        unsigned int i[ 2];
    } my_buf;                   /* Main buffer.  sys$gettim() results. */

    union
    {
        unsigned char b[ 4];
        unsigned int i;
    } os_info;                  /* sys$getXXX() results. */

    int i;
    int sts;                    /* System service/RTL status, */
    unsigned char bt;           /* Temporary byte. */
    static int num = 0;         /* Use count (method choice). */

    /* Get the 64-bit VMS system time (100ns units).
     * Note: A 64-bit system time should have rapidly changing low bits,
     * but old (VAX) systems may increment by a big number (100000?)
     * seldom, instead of by one every 100ns.
     */
    sts = sys$gettim( &my_buf);

    /* Mix in additional, invocation-dependent junk. */
    switch (num)
    {
      case 0:   /* First invocation. */
        /* Mix in process login time (high), and process ID (low). */
        sts = lib$getjpi( &((int) JPI$_LOGINTIM), 0, 0, &os_info, 0, 0);
        my_buf.i[ 1] ^= os_info.i;
        sts = lib$getjpi( &((int) JPI$_PID), 0, 0, &os_info, 0, 0);
        my_buf.i[ 0] ^= os_info.i;
        num++;
        break;
      case 1:   /* Second invocation. */
        /* Mix in process page fault count (high),
         * system free pagefile count (low).
         */
        sts = lib$getjpi( &((int) JPI$_PAGEFLTS), 0, 0, &os_info, 0, 0);
        my_buf.i[ 1] ^= os_info.i;
        sts = lib$getsyi( &((int) SYI$_PAGEFILE_FREE), &os_info, 0, 0, 0, 0);
        my_buf.i[ 0] ^= os_info.i;
        num++;
        break;
      case 2:   /* Third invocation. */
        /* Mix in process CPU time (high),
         * and process buffered and direct IO counts (low).
         */
        sts = lib$getjpi( &((int) JPI$_CPUTIM), 0, 0, &os_info, 0, 0);
        my_buf.i[ 1] ^= os_info.i;
        sts = lib$getjpi( &((int) JPI$_BUFIO), 0, 0, &os_info, 0, 0);
        my_buf.i[ 0] ^= (os_info.i << 16);
        sts = lib$getjpi( &((int) JPI$_DIRIO), 0, 0, &os_info, 0, 0);
        my_buf.i[ 0] ^= os_info.i;
        num++;
        break;
      default:  /* After the third invocation. */
        /* Mix in device operation count bits. */
        device_opcnt_32( &os_info.i);
        my_buf.i[ 1] ^= os_info.i;
        device_opcnt_32( &os_info.i);
        my_buf.i[ 0] ^= os_info.i;
    }

    /* Move the results into the user's buffer. */
    i = IZ_MIN( 8, len);
    memcpy( buf, &my_buf, i);

    /* Return the byte count. */
    return i;

} /* entropy_fun(). */

# endif /* def IZ_CRYPT_AES_WG */


#ifdef ENABLE_USER_PROGRESS

/* 2011-12-05 SMS.
 *
 *       establish_ctrl_t().
 *
 *    Establish Ctrl/T handler, if SYS$COMMAND is a terminal.
 */

int establish_ctrl_t( void ctrl_t_ast())
{
    int i;
    int status;
    short iosb[ 4];

    int access_mode = PR$C_PS_USER;
    int dev_class;

/*
 * Mask bits for Ctrl/x characters.
 *  Z  Y  X  W  V  U  T  S  R  Q  P O N M L K J I H G F E D C B A sp
 * 1A 19 18 17 16 15 14 13 12 11 10 F E D C B A 9 8 7 6 5 4 3 2 1  0
 * -------- ----------- ----------- ------- ------- ------- --------
 *        0           1           0       0       0       0        0
 */
    struct
    {
        int mask_size;
        unsigned int mask;
    } char_mask = { 0, 0x00100000 };    /* Ctrl/T. */

    $DESCRIPTOR( term_name_descr, "SYS$COMMAND");
    short term_chan;

    status = sys$assign( &term_name_descr, &term_chan, 0, 0);
    if ((status& STS$M_SEVERITY) != STS$K_SUCCESS)
    {
        fprintf( mesg, " establish_ctrl_t(): $ASSIGN sts =  %%x%08x .\n",
         status);
        return status;
    }

    status = lib$getdvi( &((int)DVI$_DEVCLASS), /* Item code. */
                         &term_chan,            /* Channel. */
                         0,                     /* Device name. */
                         &dev_class,            /* Result buffer (int). */
                         0,                     /* Result string. */
                         0);                    /* Result string len. */

    if ((status& STS$M_SEVERITY) != STS$K_SUCCESS)
    {
        fprintf( mesg, " establish_ctrl_t(): $GETDVI sts =  %%x%08x .\n",
         status);
        sys$dassgn( term_chan);
        return status;
    }

    if (dev_class != DC$_TERM)
    {
        /* SYS$COMMAND is not a terminal.  (Batch job, for example.)
         * Harmless, but we can't establish a Ctrl/T handler for it.
         */
        sys$dassgn( term_chan);
        vaxc$errno = ENOTTY;
        return EVMSERR;
    }

#define FUN_AST_ENA (IO$_SETMODE| IO$M_OUTBAND)

    status = sys$qiow( 0,               /* Event flag. */
                       term_chan,       /* Channel. */
                       FUN_AST_ENA,     /* Function code. */
                       &iosb,           /* IOSB. */
                       0,               /* AST address. */
                       0,               /* AST parameter. */
                       ctrl_t_ast,      /* P1 = OOB AST addr. */
                       &char_mask,      /* P2 = Char mask. */
                       &access_mode,    /* P3 = Access mode. */
                       0,               /* P4. */
                       0,               /* P5. */
                       0);              /* P6. */

    if ((status& STS$M_SEVERITY) != STS$K_SUCCESS)
    {
        fprintf( mesg, " establish_ctrl_t(): $QIOW sts =  %%x%08x .\n",
         status);
        sys$dassgn( term_chan);
    }

    /* If successful, then don't deassign the channel. */
    return status;
}

#endif /* def ENABLE_USER_PROGRESS */


/* 2004-11-23 SMS.
 *
 *       get_rms_defaults().
 *
 *    Get user-specified values from (DCL) SET RMS_DEFAULT.  FAB/RAB
 *    items of particular interest are:
 *
 *       fab$w_deq         default extension quantity (blocks) (write).
 *       rab$b_mbc         multi-block count.
 *       rab$b_mbf         multi-buffer count (used with rah and wbh).
 */

/* Default RMS parameter values. */

#define RMS_DEQ_DEFAULT 16384   /* About 1/4 the max (65535 blocks). */
#define RMS_MBC_DEFAULT 127     /* The max, */
#define RMS_MBF_DEFAULT 2       /* Enough to enable rah and wbh. */

/* GETJPI item descriptor structure. */
typedef struct
    {
    short buf_len;
    short itm_cod;
    void *buf;
    int *ret_len;
    } jpi_item_t;

/* Durable storage */

static int rms_defaults_known = 0;

/* JPI item buffers. */
static unsigned short rms_ext;
static char rms_mbc;
static unsigned char rms_mbf;

/* Active RMS item values. */
unsigned short rms_ext_active;
char rms_mbc_active;
unsigned char rms_mbf_active;

/* GETJPI item lengths. */
static int rms_ext_len;         /* Should come back 2. */
static int rms_mbc_len;         /* Should come back 1. */
static int rms_mbf_len;         /* Should come back 1. */

/* Desperation attempts to define unknown macros.  Probably doomed.
 * If these get used, expect sys$getjpiw() to return %x00000014 =
 * %SYSTEM-F-BADPARAM, bad parameter value.
 * They keep compilers with old header files quiet, though.
 */
#ifndef JPI$_RMS_EXTEND_SIZE
#  define JPI$_RMS_EXTEND_SIZE 542
#endif /* ndef JPI$_RMS_EXTEND_SIZE */

#ifndef JPI$_RMS_DFMBC
#  define JPI$_RMS_DFMBC 535
#endif /* ndef JPI$_RMS_DFMBC */

#ifndef JPI$_RMS_DFMBFSDK
#  define JPI$_RMS_DFMBFSDK 536
#endif /* ndef JPI$_RMS_DFMBFSDK */

/* GETJPI item descriptor set. */

struct
    {
    jpi_item_t rms_ext_itm;
    jpi_item_t rms_mbc_itm;
    jpi_item_t rms_mbf_itm;
    int term;
    } jpi_itm_lst =
     { { 2, JPI$_RMS_EXTEND_SIZE, &rms_ext, &rms_ext_len },
       { 1, JPI$_RMS_DFMBC, &rms_mbc, &rms_mbc_len },
       { 1, JPI$_RMS_DFMBFSDK, &rms_mbf, &rms_mbf_len },
       0
     };

int get_rms_defaults()
{
int sts;

/* Get process RMS_DEFAULT values. */

sts = sys$getjpiw( 0, 0, 0, &jpi_itm_lst, 0, 0, 0);
if ((sts& STS$M_SEVERITY) != STS$M_SUCCESS)
    {
    /* Failed.  Don't try again. */
    rms_defaults_known = -1;
    }
else
    {
    /* Fine, but don't come back. */
    rms_defaults_known = 1;
    }

/* Limit the active values according to the RMS_DEFAULT values. */

if (rms_defaults_known > 0)
    {
    /* Set the default values. */

    rms_ext_active = RMS_DEQ_DEFAULT;
    rms_mbc_active = RMS_MBC_DEFAULT;
    rms_mbf_active = RMS_MBF_DEFAULT;

    /* Default extend quantity.  Use the user value, if set. */
    if (rms_ext > 0)
        {
        rms_ext_active = rms_ext;
        }

    /* Default multi-block count.  Use the user value, if set. */
    if (rms_mbc > 0)
        {
        rms_mbc_active = rms_mbc;
        }

    /* Default multi-buffer count.  Use the user value, if set. */
    if (rms_mbf > 0)
        {
        rms_mbf_active = rms_mbf;
        }
    }

if (DIAG_FLAG)
    {
    fprintf( mesg,
     "Get RMS defaults.  getjpi sts = %%x%08x.\n",
     sts);

    if (rms_defaults_known > 0)
        {
        fprintf( mesg,
         "               Default: deq = %6d, mbc = %3d, mbf = %3d.\n",
         rms_ext, rms_mbc, rms_mbf);
        }
    }
return sts;
}

#ifdef __DECC

/* 2004-11-23 SMS.
 *
 *       acc_cb(), access callback function for DEC C zfopen().
 *
 *    Set some RMS FAB/RAB items, with consideration of user-specified
 * values from (DCL) SET RMS_DEFAULT.  Items of particular interest are:
 *
 *       fab$w_deq         default extension quantity (blocks).
 *       rab$b_mbc         multi-block count.
 *       rab$b_mbf         multi-buffer count (used with rah and wbh).
 *
 *    See also the FOP* macros in OSDEP.H.  Currently, no notice is
 * taken of the caller-ID value, but options could be set differently
 * for read versus write access.  (I assume that specifying fab$w_deq,
 * for example, for a read-only file has no ill effects.)
 */

/* Global storage. */

int fopm_id = FOPM_ID;          /* Callback id storage, modify. */
int fopr_id = FOPR_ID;          /* Callback id storage, read. */
int fopw_id = FOPW_ID;          /* Callback id storage, write. */

int fhow_id = FHOW_ID;          /* Callback id storage, in read. */

/* acc_cb() */

int acc_cb( int *id_arg, struct FAB *fab, struct RAB *rab)
{
int sts;

/* Get process RMS_DEFAULT values, if not already done. */
if (rms_defaults_known == 0)
    {
    get_rms_defaults();
    }

/* If RMS_DEFAULT (and adjusted active) values are available, then set
 * the FAB/RAB parameters.  If RMS_DEFAULT values are not available,
 * suffer with the default parameters.
 */
if (rms_defaults_known > 0)
    {
    /* Set the FAB/RAB parameters accordingly. */
    fab-> fab$w_deq = rms_ext_active;
    rab-> rab$b_mbc = rms_mbc_active;
    rab-> rab$b_mbf = rms_mbf_active;

    /* Truncate at EOF on close, as we'll probably over-extend. */
    fab-> fab$v_tef = 1;

    /* If using multiple buffers, enable read-ahead and write-behind. */
    if (rms_mbf_active > 1)
        {
        rab-> rab$v_rah = 1;
        rab-> rab$v_wbh = 1;
        }

    if (DIAG_FLAG)
        {
        fprintf( mesg,
         "Open callback.  ID = %d, deq = %6d, mbc = %3d, mbf = %3d.\n",
         *id_arg, fab-> fab$w_deq, rab-> rab$b_mbc, rab-> rab$b_mbf);
        }
    }

/* Declare success. */
return 0;
}

#endif /* def __DECC */

/*
 * 2004-09-19 SMS.
 *
 *----------------------------------------------------------------------
 *
 *       decc_init()
 *
 *    On non-VAX systems, uses LIB$INITIALIZE to set a collection of C
 *    RTL features without using the DECC$* logical name method.
 *
 *----------------------------------------------------------------------
 */

#ifdef __DECC

#ifdef __CRTL_VER

#if !defined( __VAX) && (__CRTL_VER >= 70301000)

/*--------------------------------------------------------------------*/

/* Global storage. */

/*    Flag to sense if decc_init() was called. */

int decc_init_done = -1;

/*--------------------------------------------------------------------*/

/* decc_init()

      Uses LIB$INITIALIZE to set a collection of C RTL features without
      requiring the user to define the corresponding logical names.
*/

/* Structure to hold a DECC$* feature name and its desired value. */

typedef struct
   {
   char *name;
   int value;
   } decc_feat_t;

/* Array of DECC$* feature names and their desired values. */

decc_feat_t decc_feat_array[] = {

   /* Preserve command-line case with SET PROCESS/PARSE_STYLE=EXTENDED */
 { "DECC$ARGV_PARSE_STYLE", 1 },

   /* Preserve case for file names on ODS5 disks. */
 { "DECC$EFS_CASE_PRESERVE", 1 },

   /* Enable multiple dots (and most characters) in ODS5 file names,
      while preserving VMS-ness of ";version". */
 { "DECC$EFS_CHARSET", 1 },

   /* List terminator. */
 { (char *)NULL, 0 } };

/* LIB$INITIALIZE initialization function. */

void decc_init( void)
{
int feat_index;
int feat_value;
int feat_value_max;
int feat_value_min;
int i;
int sts;

/* Set the global flag to indicate that LIB$INITIALIZE worked. */

decc_init_done = 1;

/* Loop through all items in the decc_feat_array[]. */

for (i = 0; decc_feat_array[ i].name != NULL; i++)
   {
   /* Get the feature index. */
   feat_index = decc$feature_get_index( decc_feat_array[ i].name);
   if (feat_index >= 0)
      {
      /* Valid item.  Collect its properties. */
      feat_value = decc$feature_get_value( feat_index, 1);
      feat_value_min = decc$feature_get_value( feat_index, 2);
      feat_value_max = decc$feature_get_value( feat_index, 3);

      if ((decc_feat_array[ i].value >= feat_value_min) &&
       (decc_feat_array[ i].value <= feat_value_max))
         {
         /* Valid value.  Set it if necessary. */
         if (feat_value != decc_feat_array[ i].value)
            {
            sts = decc$feature_set_value( feat_index,
             1,
             decc_feat_array[ i].value);
            }
         }
      else
         {
         /* Invalid DECC feature value. */
         printf( " INVALID DECC FEATURE VALUE, %d: %d <= %s <= %d.\n",
          feat_value,
          feat_value_min, decc_feat_array[ i].name, feat_value_max);
         }
      }
   else
      {
      /* Invalid DECC feature name. */
      printf( " UNKNOWN DECC FEATURE: %s.\n", decc_feat_array[ i].name);
      }
   }
}

#endif /* !defined( __VAX) && (__CRTL_VER >= 70301000) */

#endif /* def __CRTL_VER */

#endif /* def __DECC */

#endif /* VMS */
