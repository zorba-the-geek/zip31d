/*
  zip.c - Zip 3.1

  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
 *  zip.c by Mark Adler.
 */
#define __ZIP_C

#include "zip.h"
#include <time.h>       /* for tzset() declaration */
#if defined(WIN32) || defined(WINDLL)
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif
#ifdef ZIP_DLL_LIB
# include <setjmp.h>
#endif /* def ZIP_DLL_LIB */
#ifdef WINDLL
# include "windll/windll.h"
#endif
#define DEFCPYRT        /* main module: enable copyright string defines! */
#include "revision.h"
#include "crc32.h"
#include "crypt.h"
#include "ttyio.h"
#include <ctype.h>
#include <errno.h>

#ifdef WIN32
/* for locale, codepage support */
# include <locale.h>
# include <mbctype.h>
#endif

/* for getcwd, chdir */
#ifdef CHANGE_DIRECTORY
# ifdef WIN32
#  include <direct.h>
# endif
# ifdef UNIX
#  include <unistd.h>
# endif
#endif

#ifdef MACOS
# include "macglob.h"
  extern MacZipGlobals MacZip;
  extern int error_level;
#endif

#if (defined(MSDOS) && !defined(__GO32__)) || defined(__human68k__)
# include <process.h>
# if (!defined(P_WAIT) && defined(_P_WAIT))
#  define P_WAIT _P_WAIT
# endif
#endif

#ifdef UNIX_APPLE
# include "unix/macosx.h"
#endif

#ifdef VMS
# include <stsdef.h>
# include "vms/vmsmunch.h"
# include "vms/vms.h"
extern void globals_dummy( void);
#endif /* def VMS */

#include <signal.h>
#include <stdio.h>

#include <string.h>

#ifdef UNICODE_EXTRACT_TEST
# ifdef WIN32
#  include <direct.h>
# endif
#endif

#ifdef BZIP2_SUPPORT
# ifdef BZIP2_USEBZIP2DIR
#  include "bzip2/bzlib.h"
# else /* def BZIP2_USEBZIP2DIR */

   /* If IZ_BZIP2 is defined as the location of the bzip2 files, then
    * we assume that this location has been added to include path.  For
    * Unix, this is done by the unix/configure script.
    * If the OS includes support for a bzip2 library, then we assume
    * that the bzip2 header file is also found naturally.
    */

#  include "bzlib.h"
# endif /* def BZIP2_USEBZIP2DIR [else] */
#endif /* def BZIP2_SUPPORT [else] */


/* Local option flags */
#ifndef DELETE
# define DELETE 0
#endif
#define ADD     1
#define UPDATE  2
#define FRESHEN 3
#define ARCHIVE 4
local int action = ADD; /* one of ADD, UPDATE, FRESHEN, DELETE, or ARCHIVE */
                        /* comadd (edit entry comments) now global to support -st */
local int zipedit = 0;  /* 1=edit zip comment (and not "all file comments") */
local int latest = 0;   /* 1=set zip file time to time of latest file */
local int test = 0;     /* 1=test zip file with unzip -t */
local char *unzip_string = NULL; /* command string to use to test archives */
local char *unzip_path = NULL; /* where to find "unzip" for archive testing */
local int unzip_verbose = 0;  /* 1=use "unzip -t" instead of "unzip -tqq" */
local int tempdir = 0;  /* 1=use temp directory (-b) */
local int junk_sfx = 0; /* 1=junk the sfx prefix */


/* list of unzip features needed to test archive */
/* this is 0 when uninitialized */
local ulg needed_unzip_features = 0;

/* list of features supported by unzip to be used to test archive */
/* this is 0 when uninitialized */
local ulg unzip_supported_features = 0;

/* version of unzip returned by get_unzip_features() */
local float unzip_version = 0.0;


#if defined(AMIGA) || defined(MACOS)
local int filenotes = 0;    /* 1=take comments from AmigaDOS/MACOS filenotes */
#endif

#ifdef EBCDIC
int aflag = FT_EBCDIC_TXT;  /* Convert EBCDIC to ASCII or stay EBCDIC ? */
#endif
#ifdef CMS_MVS
int bflag = 0;              /* Use text mode as default */
#endif

#ifdef QDOS
char _version[] = VERSION;
#endif

#ifdef ZIP_DLL_LIB
jmp_buf zipdll_error_return;
# ifdef WIN32
  /* This kluge is for VB 6 only */
  unsigned long low, high; /* returning 64 bit values for systems without an _int64 */
  uzoff_t filesize64;
# endif /* def ZIP64_SUPPORT */
#endif /* def ZIP_DLL_LIB */

#ifdef IZ_CRYPT_ANY
/* Pointer to crc_table, needed in crypt.c */
# if (!defined(USE_ZLIB) || defined(USE_OWN_CRCTAB))
ZCONST ulg near *crc_32_tab;
# else
/* 2012-05-31 SMS.
 * Zlib 1.2.7 changed the type of *get_crc_table() from uLongf to
 * z_crc_t (to get a 32-bit type on systems with a 64-bit long).  To
 * avoid complaints about mismatched (int-long) pointers (such as
 * %CC-W-PTRMISMATCH on VMS, for example), we need to match the type
 * zlib uses.  At zlib version 1.2.7, the only indicator available to
 * CPP seems to be the Z_U4 macro.
 */
#  ifdef Z_U4
ZCONST z_crc_t *crc_32_tab;
#  else /* def Z_U4 */
ZCONST uLongf *crc_32_tab;
#  endif /* def Z_U4 [else] */
# endif
#endif /* def IZ_CRYPT_ANY */

#ifdef IZ_CRYPT_AES_WG
# include "aes_wg/aes.h"
# include "aes_wg/aesopt.h"
# include "aes_wg/iz_aes_wg.h"
#endif

#ifdef IZ_CRYPT_AES_WG_NEW
# include "aesnew/ccm.h"
#endif

#if defined( LZMA_SUPPORT) || defined( PPMD_SUPPORT)
/* Some ports can't handle file names with leading numbers,
 * hence 7zVersion.h is now SzVersion.h.
 */
# include "szip/SzVersion.h"
#endif /* defined( LZMA_SUPPORT) || defined( PPMD_SUPPORT) */

/* Local functions */

local void freeup  OF((void));
local int  finish  OF((int));
#ifndef MACOS
# ifndef ZIP_DLL_LIB
local void handler OF((int));
# endif /* ndef ZIP_DLL_LIB */
local void license OF((void));
# ifndef VMSCLI
local void help    OF((void));
local void help_extended OF((void));
# endif /* ndef VMSCLI */
#endif /* ndef MACOS */

#ifdef ENABLE_USER_PROGRESS
# ifdef VMS
#  define USER_PROGRESS_CLASS extern
# else /* def VMS */
#  define USER_PROGRESS_CLASS local
int show_pid;
# endif /* def VMS [else] */
USER_PROGRESS_CLASS void user_progress OF((int));
#endif /* def ENABLE_USER_PROGRESS */

/* prereading of arguments is not supported in new command
   line interpreter get_option() so read filters as arguments
   are processed and convert to expected array later */
local int add_filter OF((int flag, char *pattern));
local int filterlist_to_patterns OF((void));
/* not used
 local int get_filters OF((int argc, char **argv));
*/

/* list to store file arguments */
local long add_name OF((char *filearg, int verbatim));

local int DisplayRunningStats OF((void));
local int BlankRunningStats OF((void));

local void version_info OF((void));

# if !defined(WINDLL) && !defined(MACOS)
local void zipstdout OF((void));
# endif /* !WINDLL && !MACOS */

#ifndef ZIP_DLL_LIB
local int check_unzip_version OF((char *unzippath, ulg needed_unzip_features));
local void check_zipfile OF((char *zipname, char *zippath, int is_temp));
#endif /* ndef ZIP_DLL_LIB */

/* structure used by add_filter to store filters */
struct filterlist_struct {
  char flag;
  char *pattern;
  struct filterlist_struct *next;
};
struct filterlist_struct *filterlist = NULL;  /* start of list */
struct filterlist_struct *lastfilter = NULL;  /* last filter in list */

/* structure used by add_filearg to store file arguments */
struct filelist_struct {
  char *name;
  int verbatim;
  struct filelist_struct *next;
};
long filearg_count = 0;
struct filelist_struct *filelist = NULL;  /* start of list */
struct filelist_struct *lastfile = NULL;  /* last file in list */

/* used by incremental archive */
long apath_count = 0;
struct filelist_struct *apath_list = NULL;  /* start of list */
struct filelist_struct *last_apath = NULL;  /* last apath in list */


local void freeup()
/* Free all allocations in the 'found' list, the 'zfiles' list and
   the 'patterns' list.  Also free up any globals that were allocated
   and close any open files. */
{
  struct flist far *f;  /* steps through found list */
  struct zlist far *z;  /* pointer to next entry in zfiles list */
  int j;

  for (f = found; f != NULL; f = fexpel(f))
    ;
  while (zfiles != NULL)
  {
    z = zfiles->nxt;
    if (zfiles->zname && zfiles->zname != zfiles->name)
      free((zvoid *)(zfiles->zname));
    if (zfiles->name)
      free((zvoid *)(zfiles->name));
    if (zfiles->iname)
      free((zvoid *)(zfiles->iname));
    if (zfiles->cext && zfiles->cextra && zfiles->cextra != zfiles->extra)
      free((zvoid *)(zfiles->cextra));
    if (zfiles->ext && zfiles->extra)
      free((zvoid *)(zfiles->extra));
    if (zfiles->com && zfiles->comment)
      free((zvoid *)(zfiles->comment));
    if (zfiles->oname)
      free((zvoid *)(zfiles->oname));
#ifdef UNICODE_SUPPORT
    if (zfiles->uname)
      free((zvoid *)(zfiles->uname));
    if (zfiles->zuname)
      free((zvoid *)(zfiles->zuname));
    if (zfiles->ouname)
      free((zvoid *)(zfiles->ouname));
# ifdef WIN32
    if (zfiles->namew)
      free((zvoid *)(zfiles->namew));
    if (zfiles->inamew)
      free((zvoid *)(zfiles->inamew));
    if (zfiles->znamew)
      free((zvoid *)(zfiles->znamew));
# endif
#endif
    farfree((zvoid far *)zfiles);
    zfiles = z;
    zcount--;
  }

  if (patterns != NULL) {
    while (pcount-- > 0) {
      if (patterns[pcount].zname != NULL)
        free((zvoid *)(patterns[pcount].zname));
      if (patterns[pcount].uzname != NULL)
        free((zvoid *)(patterns[pcount].uzname));
      if (patterns[pcount].duzname != NULL)
        free((zvoid *)(patterns[pcount].duzname));
    }
    free((zvoid *)patterns);
    patterns = NULL;
  }

  /* free up any globals */
  if (path_prefix) {
    free(path_prefix);
    path_prefix = NULL;
  }
  if (stdin_name) {
    free(stdin_name);
    stdin_name = NULL;
  }
  if (tempath != NULL)
  {
    free((zvoid *)tempath);
    tempath = NULL;
  }
  if (zipfile != NULL)
  {
    free((zvoid *)zipfile);
    zipfile = NULL;
  }
  if (in_path != NULL)
  {
    free((zvoid *)in_path);
    in_path = NULL;
  }
  if (out_path != NULL)
  {
    free((zvoid *)out_path);
    out_path = NULL;
  }
  if (zcomment != NULL)
  {
    free((zvoid *)zcomment);
    zcomment = NULL;
  }
  if (key != NULL) {
    free((zvoid *)key);
    key = NULL;
  }
  if (passwd) {
    free(passwd);
    passwd = NULL;
  }
  if (keyfile) {
    free(keyfile);
    keyfile = NULL;
  }
  if (keyfile_pass) {
    free(keyfile_pass);
    keyfile_pass = NULL;
  }
  if (unzip_string) {
    free(unzip_string);
    unzip_string = NULL;
  }
  if (unzip_path) {
    free(unzip_path);
    unzip_path = NULL;
  }
  
  /* free any suffix lists */
  for (j = 0; mthd_lvl[j].method >= 0; j++)
  {
    if (mthd_lvl[j].suffixes)
      free(mthd_lvl[j].suffixes);
  }

  /* close any open files */
  if (in_file != NULL)
  {
    fclose(in_file);
    in_file = NULL;
  }

#ifdef CHANGE_DIRECTORY
  /* change dir */
  if (working_dir) {
# if 0
    /* return to startup directory
       This is not needed on Windows and Unix as the current directory
       of the zip process does not impact the caller. */
    if (startup_dir) {
      if (CHDIR(startup_dir)) {
        zprintf("changing to dir: %s\n  %s", startup_dir, strerror(errno));
      }
    }
# endif
    free(working_dir);
    working_dir = NULL;
  }
  if (startup_dir) {
    free(startup_dir);
    startup_dir = NULL;
  }
#endif

  /* If args still has args, free them */
  if (args) {
    free_args(args);
  }

#ifdef VMSCLI
  if (argv_cli != NULL)
  {
    /* Free VMS CLI argv[]. */
    if (argv_cli[0] != NULL)
      free(argv_cli[0]);
    free(argv_cli);
  }
#endif /* def VMSCLI */

  /* close logfile */
  if (logfile) {
    fclose(logfile);
  }
}

local int finish(e)
int e;                  /* exit code */
/* Process -o and -m options (if specified), free up malloc'ed stuff, and
   exit with the code e. */
{
  int r;                /* return value from trash() */
  ulg t;                /* latest time in zip file */
  struct zlist far *z;  /* pointer into zfile list */

  /* If latest, set time to zip file to latest file in zip file */
  if (latest && zipfile && strcmp(zipfile, "-"))
  {
    diag("changing time of zip file to time of latest file in it");
    /* find latest time in zip file */
    if (zfiles == NULL)
       zipwarn("zip file is empty, can't make it as old as latest entry", "");
    else {
      t = 0;
      for (z = zfiles; z != NULL; z = z->nxt)
        /* Ignore directories in time comparisons */
#ifdef USE_EF_UT_TIME
        if (z->iname[z->nam-1] != (char)0x2f)   /* ascii '/' */
        {
          iztimes z_utim;
          ulg z_tim;

          z_tim = ((get_ef_ut_ztime(z, &z_utim) & EB_UT_FL_MTIME) ?
                   unix2dostime(&z_utim.mtime) : z->tim);
          if (t < z_tim)
            t = z_tim;
        }
#else /* !USE_EF_UT_TIME */
        if (z->iname[z->nam-1] != (char)0x2f    /* ascii '/' */
            && t < z->tim)
          t = z->tim;
#endif /* ?USE_EF_UT_TIME */
      /* set modified time of zip file to that time */
      if (t != 0)
        stamp(zipfile, t);
      else
        zipwarn(
         "zip file has only directories, can't make it as old as latest entry",
         "");
    }
  }

  /* If dispose, delete all files in the zfiles list that are marked */
  if (dispose)
  {
    diag("deleting files that were added to zip file");
    if ((r = trash()) != ZE_OK)
      ZIPERR(r, "was deleting moved files and directories");
  }

  /* display execution time if -pt */
#ifdef ENABLE_ENTRY_TIMING
  if (performance_time) {
    zoff_t delta;
    double secs;

    current_time = get_time_in_usec();
    delta = current_time - start_time;
    secs = (double)delta / 1000000;
    sprintf(errbuf, "(Zip elapsed time:  %5.3f secs)", secs);
    zipmessage(errbuf, "");
  }
#endif

  /* Done!  (Almost.) */
  freeup();
  return e;
}


/* show_env() - Display Zip-related environment variables.
 *
 * This is used by ziperr() and version_info().
 */
/* List of variables to check - port dependent */
  static ZCONST char *zipenv_names[] = {
# ifndef VMS
#  ifndef RISCOS
    "ZIP"
#  else /* RISCOS */
    "Zip$Options"
#  endif /* ?RISCOS */
# else /* VMS */
    "ZIP_OPTS"
# endif /* ?VMS */
    ,"ZIPOPT"
# ifdef AZTEC_C
    ,     /* extremely lame compiler bug workaround */
# endif
# ifndef __RSXNT__
#  ifdef __EMX__
    ,"EMX"
    ,"EMXOPT"
#  endif
#  if (defined(__GO32__) && (!defined(__DJGPP__) || __DJGPP__ < 2))
    ,"GO32"
    ,"GO32TMP"
#  endif
#  if (defined(__DJGPP__) && __DJGPP__ >= 2)
    ,"TMPDIR"
#  endif
# endif /* !__RSXNT__ */
# ifdef RISCOS
    ,"Zip$Exts"
# endif
  };

void show_env(non_null_only)
 int non_null_only;
{
  int heading = 0;
  int i;
  char *envptr;

  for (i = 0; i < sizeof(zipenv_names) / sizeof(char *); i++)
  {
    envptr = getenv(zipenv_names[i]);
    if ((non_null_only == 0) || (envptr != (char *)NULL))
    {
      if (heading == 0) {
        zipmessage_nl("Zip environment options:", 1);
        heading = 1;
      }
      sprintf(errbuf, "%16s:  %s", zipenv_names[i],
       ((envptr == (char *)NULL || *envptr == 0) ? "[none]" : envptr));
      zipmessage_nl(errbuf, 1);
    }
  }
  if ((non_null_only == 0) && (i == 0))
  {
      zipmessage_nl("        [none]", 1);
      zipmessage_nl("", 1);
  }
}


void ziperr(c, h)
int c;                  /* error code from the ZE_ class */
ZCONST char *h;         /* message about how it happened */
/* Issue a message for the error, clean up files and memory, and exit. */
{
  /* Message h may be using errbuf, so overwriting errbuf overwrites
     message h in that case.  To avoid that, we set up a separate
     buffer for use by ziperr().
  */
  char ebuf[ERRBUF_SIZE + 1];

#ifndef ZIP_DLL_LIB
# ifndef MACOS
  static int error_level = 0;
# endif

  if (error_level++ > 0)
     /* avoid recursive ziperr() printouts (his should never happen) */
     EXIT(ZE_LOGIC);  /* ziperr recursion is an internal logic error! */
#endif /* !ZIP_DLL_LIB */

  if (mesg_line_started) {
    zfprintf(mesg, "\n");
    mesg_line_started = 0;
  }
  if (logfile && logfile_line_started) {
    zfprintf(logfile, "\n");
    logfile_line_started = 0;
  }
  if (h != NULL) {
    if (PERR(c)) {
      sprintf(ebuf, "zip I/O error: %s\n", strerror(errno));
      /* perror("zip I/O error"); */
#ifdef UNICODE_SUPPORT_WIN32
      print_utf8(ebuf);
#else
      zfprintf(mesg, "%s", ebuf);
#endif
    }
    sprintf(ebuf, "zip error: %s (%s)\n", ZIPERRORS(c), h);
#ifdef UNICODE_SUPPORT_WIN32
    print_utf8(ebuf);
#else
    zfprintf(mesg, "%s", ebuf);
#endif
    fflush(mesg);

#ifdef ZIP_DLL_LIB
  /* LIB and DLL error callback */
  if (*lpZipUserFunctions->error != NULL) {
    if (PERR(c)) {
      sprintf(ebuf, "zip I/O error: %s", strerror(errno));
      (*lpZipUserFunctions->error)(ebuf);
    }
    sprintf(ebuf, "zip error: %s (%s)", ZIPERRORS(c), h);
    (*lpZipUserFunctions->error)(ebuf);
  }
#endif

    /* Show non-null option environment variables after a syntax error. */
    if (c == ZE_PARMS) {
      show_env(1);
    }

#ifdef DOS
    check_for_windows("Zip");
#endif
    if (logfile) {
      if (PERR(c))
        zfprintf(logfile, "zip I/O error: %s\n", strerror(errno));
      zfprintf(logfile, "\nzip error: %s (%s)\n", ZIPERRORS(c), h);
      logfile_line_started = 0;
    }
  }
  if (tempzip != NULL)
  {
    if (tempzip != zipfile) {
      if (current_local_file)
        fclose(current_local_file);
      if (y != current_local_file && y != NULL)
        fclose(y);
#ifndef DEBUG
      destroy(tempzip);
#endif
      free((zvoid *)tempzip);
      tempzip = NULL;
    } else {
      /* -g option, attempt to restore the old file */

      /* zip64 support 09/05/2003 R.Nausedat */
      uzoff_t k = 0;                        /* keep count for end header */
      uzoff_t cb = cenbeg;                  /* get start of central */

      struct zlist far *z;  /* steps through zfiles linked list */

      zfprintf(mesg, "attempting to restore %s to its previous state\n",
         zipfile);
      if (logfile)
        zfprintf(logfile, "attempting to restore %s to its previous state\n",
           zipfile);

      zfseeko(y, cenbeg, SEEK_SET);

      tempzn = cenbeg;
      for (z = zfiles; z != NULL; z = z->nxt)
      {
        putcentral(z);
        tempzn += 4 + CENHEAD + z->nam + z->cext + z->com;
        k++;
      }
      putend(k, tempzn - cb, cb, zcomlen, zcomment);
      fclose(y);
      y = NULL;
    }
  }

  freeup();
#ifdef ZIP_DLL_LIB
  longjmp(zipdll_error_return, c);
#else
  EXIT(c);
#endif
}


void error(h)
  ZCONST char *h;
/* Internal error, should never happen */
{
  ziperr(ZE_LOGIC, h);
}

#if (!defined(MACOS) && !defined(ZIP_DLL_LIB) && !defined(NO_EXCEPT_SIGNALS))
local void handler(s)
int s;                  /* signal number (ignored) */
/* Upon getting a user interrupt, turn echo back on for tty and abort
   cleanly using ziperr(). */
{
# if defined(AMIGA) && defined(__SASC)
   _abort();
# else /* defined(AMIGA) && defined(__SASC) [else] */
#  if !defined(MSDOS) && !defined(__human68k__) && !defined(RISCOS)
  echon();
  putc('\n', mesg);
#  endif /* !MSDOS */
# endif /* defined(AMIGA) && defined(__SASC) [else] */
  ziperr(ZE_ABORT, "aborting");
  s++;                                  /* keep some compilers happy */
}
#endif /* !defined(MACOS) && !defined(ZIP_DLL_LIB) && !defined(NO_EXCEPT_SIGNALS) */


/* void zipmessage_nl(a, nl) - moved to fileio.c */

/* void zipmessage(a, b) - moved to fileio.c */

/* void zipwarn_i(indent, a, b) - moved to fileio.c */

void zipwarn(a, b)
ZCONST char *a, *b;     /* message strings juxtaposed in output */
{
  zipwarn_i("zip warning:", 0, a, b);
}

/* zipwarn_indent(): zipwarn(), with message indented. */

void zipwarn_indent(a, b)
ZCONST char *a, *b;
{
    zipwarn_i("zip warning:", 1, a, b);
}

local void license()
/* Print license information to stdout. */
{
  extent i;             /* counter for copyright array */

  for (i = 0; i < sizeof(swlicense)/sizeof(char *); i++)
    zprintf("%s\n", swlicense[i]);
  zprintf("\n");
  zprintf("See \"zip -v\" for additional copyright information and encryption notices.\n");
  zprintf("\n");
}

# ifdef VMSCLI
void help()
# else /* def VMSCLI */
local void help()
# endif /* def VMSCLI [else] */
/* Print help (along with license info) to stdout. */
{
  extent i;             /* counter for help array */

  /* help array */
  static ZCONST char *text[] = {
# ifdef VMS
"Zip %s (%s). Usage: zip == \"$ disk:[dir]zip.exe\"",
# else /* def VMS */
"Zip %s (%s). Usage:",
# endif /* def VMS [else] */
# ifdef MACOS
"zip [-options] [-b fm] [-t mmddyyyy] [-n suffixes] [zipfile list] [-xi list]",
"  The default action is to add or replace zipfile entries from list.",
" ",
"  -f   freshen: only changed files  -u   update: only changed or new files",
"  -d   delete entries in zipfile    -m   move into zipfile (delete OS files)",
"  -r   recurse into directories     -j   junk (don't record) directory names",
"  -0   store only                   -l   convert LF to CR LF (-ll CR LF to LF)",
"  -1   compress faster              -9   compress better",
"  -q   quiet operation              -v   verbose operation/print version info",
"  -c   add one-line comments        -z   add zipfile comment",
"                                    -o   make zipfile as old as latest entry",
"  -F   fix zipfile (-FF try harder) -D   do not add directory entries",
"  -T   test zipfile integrity       -X   eXclude eXtra file attributes",
#  ifdef IZ_CRYPT_ANY
"  -e   encrypt                      -n   don't compress these suffixes"
#  else /* def IZ_CRYPT_ANY [else] */
"  -h   show this help               -n   don't compress these suffixes"
#  endif /* def IZ_CRYPT_ANY [else] */
," -hh  show more help",
"  Macintosh specific:",
"  -jj  record Fullpath (+ Volname)  -N store finder-comments as comments",
"  -df  zip only datafork of a file  -S include finder invisible/system files"
# else /* def MACOS [else] */
#  ifdef VM_CMS
"zip [-options] [-b fm] [-t mmddyyyy] [-n suffixes] [zipfile list] [-xi list]",
#  else /* def VM_CMS [else] */
"zip [-options] [-b path] [-t mmddyyyy] [-n suffixes] [zipfile list] [-xi list]",
#  endif /* def VM_CMS [else] */
"  The default action is to add or replace zipfile entries from list, which",
"  can include the special name - to compress standard input.",
"  If zipfile and list are omitted, zip compresses stdin to stdout.",
"  -f   freshen: only changed files  -u   update: only changed or new files",
"  -d   delete entries in zipfile    -m   move into zipfile (delete OS files)",
"  -r   recurse into directories     -j   junk (don't record) directory names",
#  ifdef THEOS
"  -0   store only                   -l   convert CR to CR LF (-ll CR LF to CR)",
#  else /* def THEOS [else] */
"  -0   store only                   -l   convert LF to CR LF (-ll CR LF to LF)",
#  endif /* def THEOS [else] */
"  -1   compress faster              -9   compress better",
"  -q   quiet operation              -v   verbose operation/print version info",
"  -c   add one-line comments        -z   add zipfile comment",
"  -@   read names from stdin        -o   make zipfile as old as latest entry",
"  -x   exclude the following paths  -i   include only the following paths",
#  ifdef EBCDIC
#   ifdef CMS_MVS
"  -a   translate to ASCII           -B   force binary read (text is default)",
#   else  /* !CMS_MVS [else] */
"  -a   translate to ASCII",
"  -aa  Handle all files as ASCII text files, EBCDIC/ASCII conversions.",
#   endif /* ?CMS_MVS [else] */
#  endif /* EBCDIC */
#  ifdef TANDEM
"                                    -Bn  set Enscribe formatting options",
#  endif /* def TANDEM */
"  -F   fix zipfile (-FF try harder) -D   do not add directory entries",
"  -A   adjust self-extracting exe   -J   junk zipfile prefix (unzipsfx)",
"  -T   test zipfile integrity       -X   eXclude eXtra file attributes",
#  ifdef VMS
"  -C   preserve case of file names  -C-  down-case all file names",
"  -C2  preserve case of ODS2 names  -C2- down-case ODS2 file names* (*=default)",
"  -C5  preserve case of ODS5 names* -C5- down-case ODS5 file names",
"  -V   save VMS file attributes (-VV also save allocated blocks past EOF)",
"  -w   store file version numbers\
   -ww  store file version numbers as \".nnn\"",
#  endif /* def VMS */
#  ifdef NTSD_EAS
"  -!   use privileges (if granted) to obtain all aspects of WinNT security",
#  endif /* NTSD_EAS */
#  ifdef OS2
"  -E   use the .LONGNAME Extended attribute (if found) as filename",
#  endif /* OS2 */
#  ifdef VMS
#   ifdef SYMLINKS
"  -vn  preserve all VMS file names  -y   store (don't follow) symlinks",
#   else /* def SYMLINKS [else] */
"  -vn  preserve all VMS file names",
#   endif /* def SYMLINKS [else] */
#  else /* def VMS [else] */
#   ifdef SYMLINKS
"  -y   store symbolic links as the link instead of the referenced file",
#   endif /* def SYMLINKS */
#  endif /* def VMS [else] */
/*
"  -R   PKZIP recursion (see manual)",
*/
#  if defined(MSDOS) || defined(OS2)
"  -$   include volume label         -S   include system and hidden files",
#  endif /* defined(MSDOS) || defined(OS2) */
#  ifdef AMIGA
#   ifdef IZ_CRYPT_ANY
"  -N   store filenotes as comments  -e   encrypt",
"  -h   show this help               -n   don't compress these suffixes"
#   else /* def IZ_CRYPT_ANY [else] */
"  -N   store filenotes as comments  -n   don't compress these suffixes"
#   endif /* def IZ_CRYPT_ANY [else] */
#  else /* def AMIGA [else] */
#   ifdef IZ_CRYPT_ANY
"  -e   encrypt                      -n   don't compress these suffixes"
#   else /* def IZ_CRYPT_ANY [else] */
"  -h   show this help               -n   don't compress these suffixes"
#   endif /* def IZ_CRYPT_ANY [else] */
#  endif /* def AMIGA [else] */
#  ifdef RISCOS
,"  -hh  show more help               -I   don't scan thru Image files"
#  else /* def RISCOS [else] */
#   ifdef UNIX_APPLE
,"  -as  sequester AppleDouble files  -df  save Mac data fork only"
#   endif
,"  -hh  show more help"
#  endif /* def RISCOS [else] */
# endif /* def MACOS [else] */
# ifdef VMS
,"  (Must quote upper-case options, like \"-V\", unless SET PROC/PARSE=EXTEND)"
# endif /* def VMS */
,"  "
  };

  for (i = 0; i < sizeof(copyright)/sizeof(char *); i++)
  {
    zprintf(copyright[i], "zip");
    zprintf("\n");
  }
  for (i = 0; i < sizeof(text)/sizeof(char *); i++)
  {
    zprintf(text[i], VERSION, REVDATE);
    zprintf("\n");
  }
}

# ifdef VMSCLI
void help_extended()
# else /* def VMSCLI [else] */
local void help_extended()
# endif /* def VMSCLI [else] */
/* Print extended help to stdout. */
{
  extent i;             /* counter for help array */

  /* help array */
  static ZCONST char *text[] = {
"",
"Extended Help for Zip 3.1",
"",
"This is a quick summary of most features of Zip.  See the Zip Manual for",
"more detailed help.",
"",
"",
"Zip stores files in zip archives.  The default action is to add or replace",
"zipfile entries.",
"",
"Basic command line:",
"  zip options archive_name file file ...",
"",
"Some examples:",
"  Add file.txt to z.zip (create z if needed):      zip z file.txt",
"  Zip all files in current dir:                    zip z *",
"  Zip files in current dir and subdirs also:       zip -r z .",
"  Zip up directory foo and contents/subdirs:       zip -r z foo",
"",
"Basic modes:",
" External modes (selects files from file system):",
"        add      - add new files/update existing files in archive (default)",
"  -u    update   - add new files/update existing if OS file has later date",
"  -f    freshen  - update files if OS file has later date (no files added)",
"  -FS   filesync - update if date or size changed, delete if no OS match",
" Internal modes (selects entries in archive):",
"  -d    delete   - delete files from archive (see below)",
"  -U    copy     - select files in archive to copy (use with --out)",
"",
"Basic options:",
"  -r      recurse into directories (see Recursion below)",
"  -m      after archive created, delete original files (move into archive)",
"  -j      junk directory names (store just file names, not relative paths)",
"  -p      include relative dir path (deprecated) - use -j- instead (default)",
"  -q      quiet operation",
"  -v      verbose operation (just \"zip -v\" shows version information)",
"  -c      prompt for one-line comment for each entry (see Comments below)",
"  -z      prompt for comment for archive (end with just \".\" line or EOF)",
"  -o      make zipfile as old as latest entry",
"",
"",
"Syntax:",
"  The full command line syntax is:",
"",
"    zip [-shortopts ...] [--longopt ...] [zipfile [path path ...]] [-xi list]",
"",
"  Any number of short option and long option arguments allowed (within",
"  limits) as well as any number of path arguments for files to zip up.",
"  If zipfile exists, the archive is read in.  If zipfile is \"-\", stream",
"  to stdout.  If any input path is \"-\", zip stdin.",
"",
"  As of Zip 3.0, options and non-option arguments (such as file names) can",
"  appear nearly anywhere on the command line.  One exception are -i and -x",
"  lists, which must appear at the end unless special handling is done.",
"  (See -i and -x below for more on this.)  If specified, the zipfile name",
"  must be the first non-option argument.",
"",
"  Currently Zip limits arguments to a max of 4k bytes and paths (such as",
"  read from the file system) to a max of 32k bytes.  These limits may be",
"  smaller on specific OS.",
"",
"Options and Values:",
"  For short options that take values, use -ovalue or -o value or -o=value.",
"  For long option values, use either --longoption=value or --longoption value.",
"  For example:",
"    zip -ds 10 --temp-dir=tpath zipfil path1 path2 --exclude pattrn1 pattrn2",
"  Avoid -ovalue (no space between) to avoid confusion.",
"  With this release, optional values are supported for some options.  These",
"  optional values must be preceeded by \"=\" to avoid ambiguities.  E.g.:",
"      -9=deflate",
"",
"Two-character options:",
"  Be aware of 2-character options.  For example:",
"    -d -s is (delete, split size) while -ds is (dot size)",
"  Usually better to break short options across multiple arguments by function:",
"    zip -r -dbdcds 10m -lilalf logfile archive input_directory -ll",
"  If combined options are not doing what you expect:",
"   - break up options to one per argument",
"   - use -sc to see what parsed command line looks like, incl shell expansion",
"   - use -so to check for 2-char options you might have unknowingly specified",
"   - use -sf to see files Zip will operate on (includes -@, -@@, @argfile)",
"",
"Verbatim args:",
"  All args after just \"--\" arg are read verbatim as paths and not options.",
"    zip zipfile path path ... path -- verbatimpath verbatimpath ...",
"  For example:",
"    zip z -- \"-LeadDashPath\" \"a[path].c\" \"@justfile\" \"path*withwild\"",
"  You may still have to escape or quote arguments to avoid shell expansion.",
"  Zip 3.1 now supports argfiles.  To prevent @afile from being read as",
"  argfile afile, use -AF- (or put after --).",
"",
"Wildcards:",
"  Internally zip supports the following wildcards:",
"    ?       (or %% or #, depending on OS) matches any single character",
"    *       matches any number of characters, including zero",
"    [list]  matches char in list (regex), can do range [ac-f], all but [!bf]",
"  Set -RE (regex) to use [list] matching.  If use, must escape [ as [[].",

"  For shells that expand wildcards (such as Unix), escape (\\* or \"*\") so",
"  zip can recurse:",
"    zip zipfile -r . -i \"*.h\"",
"    zip files_ending_with_number -RE \"foo[0-9].c\"",
"  On Unix, can use shell to process wildcards:",
"    zip files_ending_with_number foo[0-9].c",
"    zip zipfile * -i \"*.h\"",
"  but filters, such as -i, -x, and -R patterns, should always be escaped so",
"  Zip sees them.",
"",
"  Normally * crosses dir bounds in path, e.g. 'a*b' can match 'ac/db'.  If",
"  -ws used, * does not cross dir bounds but ** does:",
"    zip -ws zipfile a*b  foo*/d",
"  will match a1234b and foobar/d, but not a/b or foo/bar/d.",
"    zip -ws zipfile a**b  foo*/d",
"  will match a1234b and a/b, but not foo/bar/d.",
"",
"  Use -nw to disable wildcards.  You may need to escape or quote to avoid",
"  shell expansion.",
"",
"Verbose/Version:",
"    -v        either enable verbose mode or show version",
"  The short option -v, when the only option, shows version info. This use",
"  can be forced using --version instead.  Otherwise -v tells Zip to be",
"  verbose, providing additional info about what is going on.  (On VMS,",
"  additional levels of verboseness are enabled using -vv and -vvv.)  This",
"  info tends towards the debugging level and is probably not useful to",
"  the average user.  Long option is --verbose.",
"",
"Large files and Zip64:",
"  Zip handles files larger than 4 GiB and archives with more than 64k entries",
"  by using the Zip64 extensions.  For backward compatibility, Zip only uses",
"  these when needed, and the use is automatic.  However, if a file is very",
"  close to 4 GiB it's possible bad compression or operations like -l line",
"  end conversions may push the resulting file over the limit.  If Zip does",
"  not automatically select Zip64 and a \"need --force-zip64\" error results,",
"  use -fz (--force-zip64) to force use of Zip64.",
"",
"  Zip 3.1 better handles files near 4 GiB.  -fz should rarely be needed now.",
"",
"Input file lists and include files:",
"    -@          read names (paths) to zip from stdin (one name per line)",
"    -@@ fpath   open file at fpath and read names (one per line)",
"",
"  As of Zip 3.1, any leading or trailing white space on a line is trimmed.",
"  To prevent this, include the entire path in double quotes.",
"",
"  On Windows with Unicode support, Zip 3.1 now checks each arg or path for",
"  proper UTF-8 and processes that arg or path as UTF-8 if it has.  This means",
"  an include file can contain a mix of UTF-8 and local MBCS and, as long as",
"  the MBCS does not look like UTF-8, the UTF-8 and the local MBCS should",
"  each be handled correctly.  (A particular arg or path should NOT contain",
"  a mix of local MBCS and UTF-8, as local charset MBCS in the string is not",
"  properly handled if proper UTF-8 is detected.)  So labeling an include",
"  file's overall encoding as UTF-8 or not (as many text editors do) is now",
"  not that relevant to Zip.  Nonetheless, it's probably good practice not to",
"  mix local MBCS and UTF-8 in the same include file, and most text editors",
"  probably won't allow it anyway.  (Note that Windows Unicode (UTF-16 or",
"  double byte) is not UTF-8.  Zip will not process UTF-16 paths.)",
"",
"  Currently non-Windows ports do not explicitly support UTF-8.  If the",
"  local character set (locale) is UTF-8, then UTF-8 is naturally handled as",
"  the local character set.  Unlike Windows, on Unix and similar ports",
"  implicit conversion between UTF-8 and another (local) character set is",
"  not yet supported.  On these ports an include file should be in the",
"  same character set as the locale.  For full automatic character set",
"  support, we recommend using a UTF-8 locale.",
"",
"Argument files:",
"  An argument file is a text file (default extension .zag) that contains",
"  white space separated arguments for Zip.  When Zip encounters @argfile",
"  (no space between @ and filename), it tries to open file \"argfile\" and",
"  insert any args found into the command line at that location.  For",
"  example, if file myfiles.zag contains",
"    file1.txt  file2.txt  -ll",
"  then",
"    zip  myzipfile  @myfiles",
"  would result in the command line",
"    zip  -ll  myzipfile  file1.txt  file2.txt",
"  (after permutation, i.e. options migrated before non-option args).",
"  Argfiles can contain most any valid options or arguments separated by",
"  white space.  Inserted contents are not evaluated until complete command",
"  line is built.  Enclose args containing white space in \"double quotes\".",
"  Use -sc to see final command line before executing it.  Comments in",
"  argfiles start with the \"#\" arg and must not have any non-space",
"  characters around it:",
"    # this is a comment",
"    file1.txt  file2.txt  -ll  #  another comment",
"  A Directive is a command to the argfile processor.  Directives must start",
"  at left edge (excluding white space).  Currently just one, #echo (no space",
"  between # and echo), that outputs message when that line is processed:",
"    #echo   Starting arg file 1",
"",
"  Note that @file in -i (include) or -x (exclude) lists is taken as part of",
"  the -i or -x option and not read as argfiles.  These instead are read as",
"  include files with one path or pattern a line:",
"    zip foo -r . -i @include.lst @include2.lst",
"",
"  Argfiles support a max recursion depth of 4, so @1 can call @2 can call @3",
"  can call @4.  If 4 tries to open an argfile, Zip exits with an error.",
"",
"  -AF- will turn off processing arg files, so @file is just \"@file\", but",
"  -AF- must appear before any arg files on command line.",
"",
"  On Windows, Zip performs UTF-8 detection on each token from an argile as",
"  it is processed, so flagging a file as UTF-8 or not is no longer relevant.",
"  On other ports, only local charset (associated with locale) is understood.",
"  See the comments in \"Input file lists and include files\" section above.",
"",
"Include and Exclude:",
"    -i pattern1 pattern2 ...   include files that match a pattern",
"    -x pattern1 pattern2 ...   exclude files that match a pattern",
"  Patterns are paths with optional wildcards and match entire paths",
"  relative to the current directory (which might have been changed",
"  using the -cd option).  For example, on Unix aa/bb/* will match",
"  aa/bb/file.c, aa/bb/cc/file.txt, and so on.  (This syntax also works",
"  on Windows, where \\ and / are both handled as directory separators.",
"  Patterns should be in the local file system syntax.  For example, on",
"  VMS, VMS syntax for paths should be used.)  Also, a*b.c will match",
"  ab.c, a/b.c, and ab/cd/efb.c.  (But see -ws to not match across slashes",
"  (directory separators).)  Exclude and include lists end at next option,",
"  @, or end of line.",
"    zip -x pattern pattern @ zipfile path path ...",
"",
"  A list of include or exlude patterns can be read from a file using",
"  -i@incfile and -x@excfile, respectively.  (No space between -i or -x and",
"  @.)  For example, to include only files that match the patterns in",
"  myinclude.txt, one per line:",
"    zip archive -r . -i@myinclude.txt",
"  These are patterns/paths relative to the current directory.  As of",
"  Zip 3.1, leading and trailing white space is removed from each line",
"  before processing.  Use double quotes to prevent this trimming.",
"",
"  Note that there are two forms of \"list\" arguments, single value and",
"  multiple value.  The single value form has no space between the option",
"  and the first (and only) value:",
"    zip  zipfile  -i@patterns_to_include.txt  file1  file2",
"  The list has only one member, @patterns_to_include.txt.  The @ tells Zip",
"  to open this file and read a list of patterns from it.",
"    zip  zipfile  -i=*.txt  file1  file2",
"  Zip reads *.txt as the only pattern.  The \"=\" is optional, but",
"  recommended to avoid matching 2-character options, like -ic.",
"",
"  The multiple value form must include a space between the option and the list",
"  members, which is what tells Zip this is a multi-value list.  For instance:",
"    zip  zipfile  -r  somedir  -i *.txt *.log",
"  tells Zip to recurse down somedir, including files that match one of the",
"  two patterns *.txt and *.log.  A multi-value list ends at the end of the",
"  line, at the next option, or at @ (an argument that is just @).  In",
"  this case the end of the command line terminates the list.  Another option",
"  can also:",
"    zip  zipfile  -r  somedir  -i *.txt *.log  -x *.o",
"  The @ argument can also terminate a list:",
"    zip  -i *.txt *.log @  zipfile -r somedir",
"",
"  Note that @file in a value list is read as an include file (one path or",
"  pattern a line), not as an argfile:",
"    zip  -i @include.lst @include2.list -r zipfile somedir",
"",
"  Include (-i), exclude (-x) and recurse current (-R) patterns are filters.",
"  -i and -x patterns are applied after paths/names, either listed on the",
"  command line or gathered during -r or -R recursion, are collected.  For",
"  -R, the directory tree starting at the current directory is traversed, then",
"  -i, -x, and -R filters are applied to remove entries from the list.  To save",
"  execution time, list specific directories to search rather than rely on -i",
"  and -x to remove entries.  For large current directory trees, consider using",
"  -r with targeted subdirectories instead of -R.",
"",
"Case matching:",
"  On most OS the case of patterns must match the case in the archive, unless",
"  -ic used.",
"    -ic       ignore case of archive entries",
"  This option not available on case-sensitive file systems.  On others, case",
"  ignored when matching files on file system, but matching against archive",
"  entries remains case sensitive typically for modes -f (freshen), -U",
"  (archive copy), and -d (delete) (this varies on some OS) because archive",
"  paths are always case sensitive.  With -ic, all matching ignores case, but",
"  then multiple archive entries that differ only in case will match.",
"",
"End Of Line Translation (text files only):",
"    -l        change CR or LF (depending on OS) line end to CR LF (Unix->Win)",
"    -ll       change CR LF to CR or LF (depending on OS) line end (Win->Unix)",
"  If first buffer read from file contains binary the translation is skipped",
"  (this check generally reliable, but when there's no binary in first couple",
"  K bytes of file, this check can fail and binary file might get corrupted).",
"",
"  WARNING:  The binary detection algorithm is simplistic and, though it seems",
"  totally effective in our tests, it may not catch all files with binary and",
"  so it's possible binary files could be corrupted.  For example, a text file",
"  with binary appended to the end will likely get labeled \"text\".  If",
"  corruption does occur, reversing the conversion (if -l was used, use -ll,",
"  and vice versa) may fix the file.  (There is no reverse equivalent for -a",
"  (EBCDIC -> ASCII).)  If file contains a mix of Unix and Windows line ends,",
"  the conversion can't currently be reversed.  But see below for Zip 3.1.",
"",
"  As of Zip 3.1, -BF can be used to force checking entire file for binary.",
"  -l, -ll and -a still rely on the initial buffer check, but a warning is",
"  issued if the initial assumption was wrong.  (The test exists as soon as",
"  any binary is found.)  If -l, -ll or -a are used, -BF is implied.  Also,",
"  if a file being converted using -l, -ll or -a is found to contain binary,",
"  Zip will restart the processing of that file as binary, replacing the",
"  corrupted entry.  The output file must be seekable and rewritable for a",
"  restart to happen.",
"",
"  Currently a file must be detected as text (or Zip told all files should be",
"  considered text using the EBCDIC -aa option) for the line end and",
"  character set translations to happen.  The current binary detection",
"  algorithm is designed for ASCII, UTF-8 and EBCDIC text files and these",
"  should be automatically detected as text and converted.  However, Windows",
"  \"Unicode\" (double byte) format is currently considered binary and not",
"  converted.  If a Unicode text file is intended for use on multiple systems",
"  (Windows, MAC, and/or Unix, for instance), then save the file in UTF-8",
"  format, not Windows Unicode format.",
"",
"  Zip now automatically handles selection of Zip64 when needed, even for",
"  input files close to 4 GiB and when using -l to change Unix line ends to",
"  Windows line ends.  The exception is when streaming to stdout, in which",
"  case Zip will make an assumption based on thresholds for each compression",
"  method.  If you get a \"need --force-zip64\" error, use -fz (--force-zip64)",
"  to force Zip to use Zip64 large file extensions.",
"",
"Recursion:",
"    -r        recurse paths, include files in subdirs: zip -r a path path ...",
"    -R        recurse current dir and match patterns:  zip -R a ptn ptn ...",
"  Path root in archive starts at current dir, so if /a/b/c/file and",
"  current dir is /a/b, \"zip -r archive .\" puts c/file in archive.",
"",
"  -r recurses down each directory path specified and matches all files,",
"  unless -i or -x used to filter out specific files.",
"",
"  -R patterns match ends of paths.  Given n is the number of directory",
"  components in the pattern, where n is one more than the number of",
"  slashes in the pattern, -R will compare the last n components of",
"  each path to the pattern.  (-R matches against internal paths, so",
"  the slash is the directory separator for all ports.)  For -R, * does",
"  not cross slashes (directory boundaries).  So if pattern contains 2",
"  slashes, last 3 components of path are compared.  For instance, if pattern",
"  is *.txt, then one component will be matched against (last one, the file",
"  name) and foo.txt, a/bar.txt and b/a/foobar.txt all match.  Pattern foo/*",
"  will look at the last two components of each path and compare to pattern.",
"  Paths like .../foo/bar.txt will match.  Note that subdirectories below",
"  foo/ will not be recursed.  Pattern *a/*.txt will match a/bar.txt and",
"  b/a/foobar.txt, but not foo.txt (as looking for two components, not one).",
"  Pattern *a* will match only filenames with 'a', such as 'bar' in a/bar.txt,",
"  however deep.  -R patterns do not match directories themselves, as these",
"  always end in / (unless pattern includes end /).",
"",
"  Use -i and -x with either -r or -R to include only or exclude files.",
"",
"Dates and date filtering:",
"    -t date   exclude before (include files modified on this date and later)",
"    -tt date  include before (include files modified before date)",
"  Can use both at same time to set inclusive date range (include if both -t",
"  and -tt are true).  For example, to include all files from May 2015, could",
"  use:",
"    zip  MayFiles  -t 2015-05-01  -tt 2015-06-01  -r  .",
"",
"  Dates are mmddyyyy or yyyy-mm-dd.",
"  As of Zip 3.1, dates can include times.  The new optional format is:",
"    [date][:time]",
"  where a time value always starts with colon (:).  If no date, today",
"  assumed.  (Be careful when working near date boundaries where current date",
"  changes - probably best to always include date.)",
"  Time format:",
"    :HH:MM[:SS]",
"  where seconds optional.  24 hour format, so 15:10 is 3:10 PM.",
"  Examples:",
"    -t 2014-01-17            Include files modified on/after Jan 17, 2014",
"    -tt 01172014:07:08       Include files before 7:08 AM on Jan 17, 2014",
"    -tt 2014-01-17:07:08:09  Include files before 7:08:09 on Jan 17, 2014",
"    -t :07:08                Include files after 7:08 AM today",
"    -t :07:00  -tt :15:00    Include files from 7 AM to 3 PM today",
"",
"  Be aware of time and date differences when Zip archives are moved between",
"  time zones.  Also changes in daylight saving time status.  Use of",
"  Universal Time extra field, when available, mitigates effects to some",
"  extent.  (See Zip Manual for more on this.)",
"",
"  -tn prevents storage of univeral time.  -X prevents storage of most extra",
"  fields, including universal time.",
"",
"Show files:",
"    -sf       show files to operate on, and exit (-sf- logfile only)",
"  -sf lists files that will be operated on.  If included in a command line",
"  where the archive is being updated, -sf will show the files that operation",
"  will operate on.  If no input files are given, -sf lists the contents of",
"  the archive.  For instance:",
"    zip  -sf  foo",
"  will list the paths in archive foo (similar to unzip -l).  Negating -sf",
"  will prevent output to the console, only outputting the list to an open",
"  logfile."
"",
"    -sF       add info to -sf listing",
"  -sF can be used to include additional information in the -sf listing.",
"  Currently the following values are supported:",
"    usize     include uncompressed size",
"    comment   include the entry (file) comment (only for existing entries)",
"  For example:",
"     zip -sf  -sF usize  foo.zip",
"  will list each path in archive foo.zip followed by uncompressed size",
"  (rounded to 2 sig figs) in parentheses.",
"  If both are needed, they need to be specified separately:",
"     zip -sf  -sF=usize  -sF=comment  foo.zip",
"  This will include the uncompressed size after the file name and display",
"  the file comment (if any) indented on the following line.",
"",
"  -sf supports some filtering such as use of -t and -tt for date filtering",
"  and \"-i *.txt\", for instance, to list only .txt paths in the archive.",
"",
"    -su       as -sf but show escaped UTF-8 Unicode names also if exist",
"    -sU       as -sf but show escaped UTF-8 Unicode names instead",
"  These options are similar to -sf, but any character not in current locale",
"  is escaped as #Uxxxx, where x is hex digit, if 16-bit code sufficient, or",
"  #Lxxxxxx if 24-bits needed.  If add -UN=e, Zip escapes all non-ASCII",
"  characters.",
"",
"  As of Zip 3.1, Windows defaults to UTF-8 and other ports are generally",
"  restricted to the local character set defined by locale.  -su and -sU",
"  are of lesser use now, unless -UN=local is used.",
"",
"Deletion, File Sync:",
"    -d        delete files",
"  Delete archive entries matching internal archive paths in list",
"    zip archive -d pattern pattern ...",
"  Can use -t and -tt to select files in archive, but NOT -x or -i, so",
"    zip archive -d \"*\" -t 2005-12-27",
"  deletes all files from archive.zip with date of 27 Dec 2005 and later.",
"  Note the * (escape as \"*\" on Unix) to select all files in archive.",
"  Multiple names or patterns can be provided to delete.  -@ and -@@ can be",
"  used to provide a list of files to delete.  For example:",
"    zip foo -d -@@ filestodelete",
"  deletes all files listed in filestodelete, one file per line, from foo.",
"  As of Zip 3.1, leading and trailing white space is removed from each",
"  line before processing.  Use double quotes to prevent this trimming.",
"    zip foo -d @argfile",
"  also works, but paths can't include white space or must be enclosed in",
"  double quotes.  In this case, multiple names or patterns can appear on",
"  each line.  (Argfiles use white space to separate args, not line ends.)",
"",
"    -FS       file sync",
"  Similar to update, but a file is updated if date or size of entry does not",
"  match the file on OS.  Also, if no matching file on OS file system, file",
"  is deleted from archive.  Result is the archive should be brought",
"  \"in sync\" with file system.",
"    zip archive_to_update -FS -r dir_used_before",
"  Result generally same as creating new archive, but unchanged entries",
"  are copied instead of being read and compressed so can be faster.",
"",
"  WARNING:  -FS deletes entries so make backup copy of archive first.",
"",
"  When used with -sf, -FS can be used to determine the differences between",
"  the archive and the file system.",
"    zip archive_to_check -FS -r dir_used_before -sf",
"  This will show the list of Freshen (update), Delete and Add operations",
"  needed to bring archive_to_check in sync with dir_used_before.",
"",
"  It's important to use the same command line paths, patterns, and",
"  relevant options with -FS.  For example, if -r was used to make the archive",
"  but is left off the -FS command line, all the subdirectory contents will be",
"  deleted.  This is proper behavior, so make sure the command line matches",
"  all the same files as before.  -sf can be used to get an idea of what the",
"  -FS operation will do before doing it.",
"",
"Copy Mode (copying from archive to archive):",
"    -U        (also --copy) select entries in archive to copy (reverse delete)",
"  Copy Mode copies entries from old to new archive with --out and is used by",
"  zip when --out and either no input files or -U (--copy) used (see --out",
"  below).",
"    zip inarchive --copy pattern pattern ... --out outarchive",
"  To copy only files matching *.c into new archive, excluding foo.c:",
"    zip old_archive --copy \"*.c\" --out new_archive -x foo.c",
"  Wildcards must be escaped if shell would process them.",
"  If no input files and --out, copy all entries in old archive:",
"    zip old_archive --out new_archive",
"  If --out used, -U (copy) is opposite of -d (delete):",
"    zip ia -d file1 file2 --out oa   copy ia to oa, except file1, file2",
"    zip ia -U file1 file2 --out oa   copy only file1, file2 to oa",
"",
"Using --out (output to new archive):",
"    --out oa  output to new archive oa (- for stdout)",
"  Instead of updating input archive, create new output archive oa.  Result",
"  same as without --out but in new archive.  Input archive unchanged.",
"  -O (big o) can also be used to specify --out.",
"",
"  WARNING:  --out ALWAYS overwrites any existing output file(s)",
"",
"  For example, to create new_archive like old_archive but add newfile1",
"  and newfile2:",
"    zip old_archive newfile1 newfile2 --out new_archive",
"",
"Splits (archives created as set of split files):",
"    -s ssize  create split archive with splits of size ssize, where ssize nm",
"                n number and m multiplier (kmgt, default m), 100k -> 100 kB",
"    -sp       pause after each split closed to allow changing disks",
"",
"  WARNING:  Archives created with -sp use data descriptors and should",
"            work with most unzips but may not work with some",
"",
"    -sb       ring bell when pause",
"    -sv       be verbose about creating splits",
"  Cannot update split archive, so use --out to out new archive:",
"    zip in_split_archive newfile1 newfile2 --out out_split_archive",
"  If input is split, output will default to same split size",
"  Use -s=0 or -s- to turn off splitting to convert split to single file:",
"    zip in_split_archive -s 0 --out out_single_file_archive",
"",
"  WARNING:  If overwriting old split archive but need less splits,",
"            old splits not overwritten are not needed but remain",
"",
"  UnZip 6.00 and earlier can't read split archives.  One workaround is to",
"  use \"zip -s-\" to convert a split archive into a single file archive.",
"  This requires enough space for the resulting single file archive.",
"  Another approach is to use ZipSplit to read the split archive and convert",
"  it to a set of single file archives.  This set can then be given on the",
"  UnZip command line (using a wildcard), which UnZip can then read and",
"  process as a set.",
"",
"  The latest UnZip 6.1 beta does support split (multi-part) archives.",
"",
"  Split support varies among the other utilities out there.  Be sure to",
"  verify the destination utility supports splits before relying on them.",
"",
"Compression:",
"  Compression method:",
"      -Z cm   set compression method to cm",
"    Valid compression methods include:",
"      store   - store without compression, same as option -0",
"      deflate - original zip deflate, same as -1 to -9 (default)",
"      bzip2   - use bzip2 compression (need modern unzip)",
"      lzma    - use LZMA compression (need modern unzip)",
"      ppmd    - use PPMd compression (need modern unzip)",
"",
"    -Z defaults to setting a global compression method used when no other",
"    method is otherwise specified and the file suffix is not in the -n list.",
"",
"    bzip2, LZMA, and PPMd are optional and may not be enabled.",
"",
"    A special method is available when copying archives:",
"      cd_only - Only save central directory",
"    Only the central directory (file list) is copied to out archive.",
"      zip  large_archive  --out large_archive_cd_only  -Z cd_only",
"    This small archive can be used as the base archive for --diff and -BT",
"    to create difference archives without keeping the full archive on the",
"    system.  Note that CD_ONLY archives have no actual file contents.",
"",
"  Compression level:",
"      -0        store files (no compression)",
"      -1 to -9  compress fastest to compress best (default is 6)",
"",
"  Usually -Z and -0 .. -9 are sufficient for most needs.",
"",
"  Suffixes to not compress:",
"      -n suffix1:suffix2:...",
"    For example:",
"      -n .Z:.zip:.zoo:.arc:.lzh:.arj",
"    stores without compression files that end with these extensions.",
"  If no -n is specified, the default suffix (don't compress) list is:",
"    .7z:.arc:.arj:.bz2:.cab:.gz:.lha:.lzh:.lzma:.pea:.rar:.rz:.tbz2:.tgz:",
"    .tlz:.txz:.xz:.Z:.zip:.zipx:.zoo:.zz",
"  Files with these suffixes are assumed already compressed and so are stored",
"  without further compression in the archive.  If this list works for you,",
"  you may not need -n.  More on -n below.",
"",
"  Now can control level to use with a particular method, and level/method to",
"  use with particular suffix (type of file).",
"",
"  Compression level to use for particular compression methods:",
"      -L=methodlist",
"    where L is compression level (1, 2, ..., 9), and methodlist is list of",
"    compression methods in form:",
"      cm1:cm2:...",
"    Examples: -4=deflate  -8=bzip2:lzma:ppmd",
"      This tells Zip to use level 4 compression when deflate is used and",
"      level 8 compression when bzip2, LZMA or PPMd are used.",
"      So:",
"        zip archive file.txt  -4=deflate  -8=bzip2:lzma:ppmd",
"          would use deflate at level 4 on file.txt.",
"        zip archive file.txt  -Z bzip2  -4=deflate  -8=bzip2:lzma:ppmd",
"          would use level 8 bzip2 compression on file.txt.",
"    The \"=\" is required to associate method with level.  No spaces.",
"",
"    Note that \"-4=deflate\" does not actually set a compression method or",
"    level.  It just tells Zip to use level 4 IF deflate is used.  -Z and -n",
"    set the compression method, with deflate being the default if neither",
"    is given.",
"",
"  Compression method/level to use with particular file name suffixes:",
"      -n suffixlist",
"    where suffixlist is in the form:",
"      .ext1:.ext2:.ext3:...",
"    Files whose names end with a suffix in the list will be stored instead of",
"    compressed.  Note that list of just \":\" disables default list and",
"    forces compression of all extensions, including .zip files.",
"      zip -9 -n : compressall.zip *",
"    A more advanced form is now supported:",
"      -n cm=suffixlist",
"    where cm is a compression method (such as bzip2), which specifies use",
"    of compression method cm when one of those suffixes found.",
"      zip  -n lzma=.txt:.log  archive  *",
"    uses default deflate on most files, but uses LZMA on .txt/.log files",
"",
"    Specifying a store (-n list) or compression (-n method=list) list",
"    replaces old list with new list.  But suffix * includes current list.",
"    For example:",
"      zip foo.zip test1.doc test2.zip  -n \"*:.doc\"  -n lzma=.zip",
"    adds .doc to the current STORE list and changes .zip from STORE to",
"    LZMA compression, deleting any previous LZMA list.  * must be escaped",
"    on systems like Unix where the shell would expand it.  Note that",
"    assignments proceed in the order given on the command line, each -n",
"    potentially modifying any previous -n.",
"",
"    A yet more advanced form:",
"      -n cm-L=suffixlist",
"    where L is one of (1 through 9 or -), which specifies use of method cm",
"    at level L when one of those suffixes found.  (\"-\" specifies default",
"    level.)  Multiple -n can be used and are processed in order:",
"      -n deflate=.txt  -n lzma-9=.c:.h  -n ppmd=.txt",
"    which first sets the DEFLATE list to .txt, then sets the LZMA list to",
"    .c and .h and the default compression level to 9 when LZMA is used,",
"    then sets the PPMD list to .txt which pulls .txt from the DEFLATE list.",
"      zip foo test.txt  -n lzma-9=.c:.h  -n lzma-2=.txt  archive *",
"    sets LZMA list to .c:.h (compressing these using level 9 LZMA), then",
"    sets the LZMA list to .txt at level 2, wiping out the effects of the",
"    first LZMA -n.  Each compression method can accept only one set of",
"    suffixes and one level to use (if specified).  * can be used to add",
"    lists, but the last level specified is the one used.",
"      zip  foo  test1.zip test2.txt test3.Z  -n lzma=.zip  -n lzma=*:.Z",
"    compresses test2.txt using DEFLATE, and test1.zip and test3.Z with LZMA.",
"",
"    If no level is specified, the default level of 6 is used.",
"",
"      -ss     list current compression mappings (suffix lists), and exit.",
"",
"Encryption:",
"    -e        use encryption, prompt for password (default if -Y used)",
"    -P pswd   use encryption, password is pswd (NOT SECURE!  Many OS allow",
"                seeing what others type on command line.  See manual.)",
"    -kf kfil  read (last part of) password from (beginning of) file kfil,",
"                up to 128 non-NULL bytes (NULL bytes skipped)",
"",
"  If both password (-e or -P) and keyfile (-kf) used, then contents of",
"  keyfile are appended to password up to max of 128 bytes total.",
"",
"  Note that on many OS the command line is visible to other users, and",
"  so the keyfile used will be visible.  This may not be a problem if the",
"  keyfile is physically secured or is on a removable media.  If this is a",
"  concern, the -kf option can be specified in an argfile, preventing the",
"  name of the keyfile from appearing on the command line.  It's also",
"  possible to hide -P in an argfile, but if others have read access to",
"  that file, they can just open the file and see the password.",
"",
"  Default is original traditional (weak) PKZip 2.0 encryption (Traditional",
"  or ZipCrypto encryption), unless -Y is used to set another encryption",
"  method.",
"",
"    -Y em     set encryption method",
"  Valid encryption methods are:",
"    TRADITIONAL (also known as ZipCrypto)",
"    AES128",
"    AES192",
"    AES256",
"  (Zip uses WinZip AES encryption.  Other strong encryption methods may be",
"  added in the future.)",
"",
"  For example:",
"    zip myarchive filetosecure.txt -Y AES2",
"  compresses using deflate and encrypts file filetosecure.txt using AES256",
"  and adds it to myarchive.  Zip will prompt for password.",
"",
"  Min password lengths in chars:  16 (AES128), 20 (AES192), 24 (AES256).",
"  (No minimum for TRADITIONAL zip encryption.)  The longer and more varied",
"  the password the better.  We suggest length of passwords should be at least",
"  22 chars (AES128), 33 chars (AES192), and 44 chars (AES256) and include",
"  mix of lowercase, uppercase, numeric, and other characters.  Strength of",
"  encryption directly depends on length and variedness of password.  To",
"  fully take advantage of AES encryption strength selected, passwords should",
"  be much longer and approach length of encryption key used (64, 96, and 128",
"  ANSI 7-bit characters, for instance).  Even using AES256, entries using",
"  simple short passwords are probably easy to crack.",
"",
"  Using a keyfile (-kf) does not impact these minimum password lengths:  Zip",
"  will still require the entered password to meet the minimum lengths.  If",
"  only a keyfile is used, the derived password must also meet these minimums.",
"",
"    -pn       allow non-ANSI characters in password.  Default is to restrict",
"                passwords to printable 7-bit ANSI characters for portability",
"    -ps       allow shorter passwords than normally permitted.",
"",
"Comments:",
"  A zip archive can include comments for each entry as well as a comment for",
"  the entire archive (the archive or zip file comment):",
"    -c        prompt for a one-line entry comment for each entry",
"    -z        prompt for the archive comment (end with just \".\" line or EOF)",
"",
"  Entry comments (-c).  Zip defaults to asking for a one-line comment for",
"  each entry (file).  As of Zip 3.1, existing comments are displayed and can",
"  be kept by just hitting ENTER.  Zip now accepts multi-line entry comments.",
"  Entering SPACE followed by ENTER changes Zip to multi-line mode.  Entering",
"  a period \".\" followed by ENTER ends the comment.  On some platforms, EOF",
"  can also end the comment.",
"",
"  Archive comments (-z).  An archive can have one zip file comment, which",
"  some utilities (such as UnZip) displays when working with the archive.  An",
"  archive comment can span multiple lines and ends when a line with just a",
"  period \".\" is entered or when EOF is detected.  As of Zip 3.1, any",
"  existing comment is displayed and can be kept by just hitting ENTER, or",
"  a new comment can be typed in.  If -z is given and a text file is streamed",
"  to stdin, Zip will set the archive comment to the text:",
"    zip -z archive < comment.txt",
"",
"  Support for Unicode (UTF-8) comments is currently limited.  If a UTF-8",
"  locale is used (such as on Linux), Zip should accept and display UTF-8",
"  comments without problems.  Unicode comment support on Windows is limited",
"  and still in development.",
"",
"  For more complex comment operations consider using ZipNote.",
"",
"Streaming and FIFOs:",
"    prog1 | zip -ll z -      zip output of prog1 to zipfile z, converting CR LF",
"    zip - -R \"*.c\" | prog2   zip *.c files in current dir and stream to prog2",
"    prog1 | zip | prog2      zip in pipe with no in or out acts like zip - -",
"",
"  If Zip is Zip64 enabled, streaming stdin creates Zip64 archives by default",
"  that need PKZip 4.5 unzipper like UnZip 6.0.",
"",
"  WARNING:  Some archives created with streaming use data descriptors and",
"            should work with most unzips but may not work with some",
"",
"  Can use --force-zip64- (-fz-) to turn off Zip64 if input known not to",
"  be large (< 4 GiB):",
"    prog_with_small_output | zip archive --force-zip64-",
"",
"  As of Zip 3.1, file attributes and comments can be included in local",
"  headers so entries in resulting zip file can be fully extracted using",
"  streaming unzip.  As new information adds slight amount to each entry,",
"  in this beta this feature needs to be enabled using -st (--stream)",
"  option.  In release may be enabled by default.",
"",
#if STREAM_COMMENTS
"  Normally when -c used to add comments for each entry added or updated,",
"  Zip asks for new comments after zip operation done.  When -c and -st used",
"  together, Zip asks for each comment as each entry is processed so comments",
"  can be included in local headers, pausing while waiting for user input.",
"  For large archives, it may be easier to first create the archive with -c",
"  and without -st, adding all the comments once the zip operation is",
"  finished:",
"    zip -c NoStreamArchive files",
"  then update the archive without -c and with -st to add streaming:",
"    zip -st NoStreamArchive --out StreamArchive",
"",
#endif
"  If something like:",
"    zip - test.txt > outfile1.zip",
"  is used, Zip usually can seek in output file and a standard archive is",
"  created.  However, if something like:",
"    zip - test.txt >> outfile2.zip",
"  is used (append to output file), Zip will try to append the created",
"  zipfile contents to any existing contents in outfile2.zip.  On Unix",
"  where >> does not allow seeking before end of file, a stream archive",
"  is created.  On Windows where seeking before end of file is allowed,",
"  a standard archive is created.",
"",
"  Zip can read Unix FIFO (named pipes).  Off by default to prevent Zip",
"  from stopping unexpectedly on unfed pipe, use -FI to enable:",
"    zip -FI archive fifo",
"  As of Zip 3.1, all Named Pipes are stored, but contents are only read",
"  if -FI given.  (Previously named pipes were skipped without -FI.)",
"",
"Dots, counts:",
"  Any console output may slow performance of Zip.  That said, there are",
"  times when additional output information is worth performance impact or",
"  when actual completion time is not critical.  Below options can provide",
"  useful information in those cases.  If speed important, check out the -dg",
"  option (which can be used with log file to save more detailed information).",
"",
"    -db       display running count of bytes processed and bytes to go",
"                (uncompressed size, except delete and copy show stored size)",
"    -dc       display running count of entries done and entries to go",
"    -dd       display dots every 10 MiB (or dot size) while processing files",
"    -de       display estimated time to go",
"    -dg       display dots globally for archive instead of for each file",
"      zip -qdgds 1g   will turn off most output except dots every 1 GiB",
"    -dr       display estimated zipping rate in bytes/sec",
"    -ds siz   each dot is siz processed where siz is nm as splits (0=no dots)",
"    -dt       display time Zip started zipping entry in day/hr:min:sec format",
"    -du       display original uncompressed size for each entry as added",
"    -dv       display volume (disk) number in format in_disk>out_disk",
"  Dot size is approximate, especially for dot sizes less than 1 MiB.",
"  Dot options don't apply to Scanning files dots (dot/2sec) (-q turns off).",
"  Options -de and -dr do not display for first few entries.",
"",
"Logging:",
"    -lf path  open file at path as logfile (overwrite existing file)",
"              Without -li, only end summary and any errors reported.",
"",
"              If path is \"-\" send log output to stdout, replacing normal",
"              output (implies -q).  Cannot use with -la or -v.",
"",
"    zip -li -lF -q -dg -ds 1g -r archive.zip foo",
"              will zip up directory foo, displaying just dots every 1 GiB",
"              and an end summary, minimizing console output.  A log file",
"              with name archive.log will be created and will include the",
"              detailed information for each entry normally output to the",
"              console.  (Typically log file writing has much less impact",
"              on speed.)",
"",
"    -la       append to existing logfile (default is to overwrite)",
"    -lF       open log file named ARCHIVE.log, where ARCHIVE is the name of",
"              the output archive.  Shortcut for -lf ARCHIVE.log.",
"    -li       include info messages (default just warnings and errors)",
"    -lu       log paths using UTF-8.  This option generally for Windows only.",
"              Instead of writing paths to the log in the local character set,",
"              write UTF-8 paths.  Need to open the resulting log in an",
"              application that can understand UTF-8, such as Notepad on",
"              Windows.",
"",
"Testing archives:",
"    -T        test completed temp archive with unzip (in spawned process)",
"              before committing updates.  See -pu below for handling of",
"              password.  Uses default \"unzip\" on system.",
"",
"    -TT cmd   use command cmd instead of 'unzip -tqq' to test archive.  (If",
"              cmd includes spaces, put in quotes.)  -TT allows use of unzip",
"              or another utility to test an archive, as long as the utility",
"              returns error code 0 on success (except on VMS).  If -TU can",
"              be used, that is preferred to -TT.  On Unix, to use unzip in",
"              current directory, could use:",
"                zip archive file1 file2 -T -TT \"./unzip -tqq\"",
"              In cmd, {} replaced by temp archive path, else temp path",
"              appended, and {p} replaced by password if one provided to zip,",
"              but see -pu below for handling of password.  Zip 3.1 now",
"              also supports {p: -P {p}} to include \"-P password\" in the",
"              command line if there is a password.  Also keyfiles are now",
"              supported using {k} and {k: -kf {k}}.  (Currently UnZip does",
"              not support keyfiles, but ZipCloak does.)  Return code",
"              checked for success (0 on Unix).  Note that Zip spawns new",
"              process for UnZip, passing passwords on that command line",
"              which might be viewable by others.",
"",
"              When possible, use -TU instead.",
"",
"    -TU unz   use the path unz for unzip.  The path unz replaces \"unzip\",",
"              but otherwise Zip assumes unzip is being used for testing.",
"              Comparing to the -TT example above, on Unix to use unzip in",
"              current directory, could use:",
"                zip archive file1 file2 -T -TU ./unzip",
"              Any password or keyfile is passed to unzip as normal.",
"",
"    -TV       Normally only the test results from unzip are displayed.",
"              -TV tells unzip to be verbose, showing the results of each",
"              file tested.  If testing shows a vague error, using -TV may",
"              provide more useful information.",
"",
"    -pu       pass Zip password to unzip to test archive with.  Without -pu,",
"              Zip does not pass any password and the unzip being used to",
"              test the archive will likely prompt for it.  (This is the",
"              default.)  With -pu, Zip either uses -P to pass the password",
"              it used to UnZip, or if -TT was used, {p} in command string",
"              is replaced with password.",
"",
"Fixing archives:",
"    -F        attempt to fix a mostly intact archive (try this first)",
"    -FF       try to salvage what can (may get more but less reliable)",
"  Fix options copy entries from potentially bad archive to new archive.",
"  -F tries to read archive normally (using central directory) and copy only",
"  intact entries, while",
"  -FF tries to salvage what can (reading headers directly) and may result",
"  in incomplete entries.",
"  Must use --out option to specify output archive:",
"    zip -F bad.zip --out fixed.zip",
"  As -FF only does one pass, may need to run -F on result to fix offsets.",
"  Use -v (verbose) with -FF to see details:",
"    zip reallybad.zip -FF -v --out fixed.zip",
"  Currently neither option fixes bad entries, as from text mode ftp get.",
"",
"Difference mode:",
"    -DF       (also --dif) only include files that have changed or are",
"               new as compared to the input archive",
"",
"  Difference mode can be used to create differential backups.  For example:",
"    zip --dif full_backup.zip -r somedir --out diff.zip",
"  will store all new files, as well as any files in full_backup.zip where",
"  either file time or size have changed from that in full_backup.zip,",
"  in new diff.zip.  Output archive not excluded automatically if exists,",
"  so either use -x to exclude it or put outside of what is being zipped.",
"",
"  A new compression mode, CD_ONLY, allows creating archives that contain",
"  only the file list (central directory).  Though these archives contain",
"  no file data, they can be used as the full backup --dif compares to.",
"  And as CD_ONLY archives tend to be very small (maybe 100 bytes to a few",
"  KB an entry) a --dif can be done without leaving a large full backup",
"  archive on the target system.  See the \"Compression\" section for more",
"  on CD_ONLY.",
"",
"Backup modes:",
"  A recurring backup cycle can be set up using these options:",
"    -BT type   backup type (one of FULL, DIFF, or INCR)",
"    -BD bdir   backup dir (see below)",
"    -BN bname  backup name (see below)",
"    -BC cdir   backup control file dir (see below)",
"    -BL ldir   backup log dir (see below)",
"  This mode creates control file that is written to bdir/bname (unless cdir",
"  set, in which case written to cdir/bname).  This control file tracks",
"  archives currently in backup set.  bdir/bname also used as destination of",
"  created archives, so no output archive is specified, rather this mode",
"  creates output name by adding mode (full, diff, or incr) and date/time",
"  stamp to bdir/bname.  This allows recurring backups to be generated without",
"  altering command line to specify new archive names to avoid overwriting old",
"  backups.  The OS needs to support long enough paths to use backup mode.",
"",
"  The backup types are:",
"    FULL - create a normal zip archive, but also create a control file.",
"    DIFF - create a --diff archive against the FULL archive listed in",
"             control file, and update control file to list both FULL and DIFF",
"             archives.  A DIFF backup set consists of just FULL and DIFF",
"             archives, as these two archives capture all files.",
"    INCR - create a --diff archive against FULL and any DIFF or INCR",
"             archives in control file, and update control file.  An INCR",
"             backup set consists of FULL archive and any INCR (and DIFF)",
"             archives created to that point, capturing just new and changed",
"             files each time an INCR backup is run.",
"  A FULL backup clears any DIFF and INCR entries from control file, and so",
"  starts a new backup set.  A DIFF backup clears out any INCR archives listed",
"  in control file.  INCR just adds a new incremental archive to list of",
"  archives in control file.",
"",
"  Backup options:",
"    -BT type sets the backup type.",
"",
"    -BD bdir is dir the backup archive goes in.  Must be given.",
"",
"    -BN bname is name of backup.  Gets prepended to type and date/time stamp.",
"    Also name of control file.  Must be given.",
"",
"    If -BC cdir given, control file written and maintained there.  Otherwise",
"    control file written to backup dir given by -BD.  Control file has",
"    file extension .zbc (Zip backup control).",
"",
"    If -BL given without value, a log file is written with same path as",
"    output archive, except .zip replaced by .log.  If -BT=ldir used,",
"    logfile written to log dir specified, but name of log will be same as",
"    archive, except .zip replaced by .log.  If you want list of files in",
"    log, include -li option.",
"",
"  The following command lines can be used to perform weekly full and daily",
"  incremental backups:",
"    zip -BT=full -BD=mybackups -BN=mybackupset -BL -li -r path_to_back_up",
"    zip -BT=incr -BD=mybackups -BN=mybackupset -BL -li -r path_to_back_up",
"  Full backups could be scheduled to run weekly and incr daily.",
"  Full backup will have a name such as:",
"    mybackups/mybackupset_full_FULLDATETIMESTAMP.zip",
"  and incrementals will have names such as:",
"    mybackups/mybackupset_full_FULLDATETIMESTAMP_incr_INCRDATETIMESTAMP.zip",
"  Backup mode not supported on systems that don't allow long enough names.",
"",
"  It is recommended that bdir be outside of what is being backed up, either",
"  a separate drive, path, or other destination, or use -x to exclude backup",
"  directory and everything in it.  Otherwise each backup may get included",
"  in later backups, creating exponential growth.",
"",
"  Avoid putting multiple backups or control files in same directory to",
"  prevent name collisions, unless -BN is unique for each backup set.",
"",
"  See Manual for more information.",
"",
"DOS Archive bit (Windows only):",
"    -AS       include only files with DOS Archive bit set",
"    -AC       after archive created, clear archive bit of included files",
"",
"  WARNING: Once archive bits are cleared they are cleared.",
"           Use -T to test the archive before bits are cleared.",
"           Can also use -sf to save file list before zipping files.",
"",
"Unicode and charsets:",
"  If compiled with Unicode support, Zip stores UTF-8 paths and comments of",
"  entries.  Unicode allows better conversion of entry names between different",
"  character sets.  Unicode support is enabled by default on many Unix ports",
"  and on Windows NT and later.",
"",
"  Zip 3.0 stored UTF-8 paths and comments in a backward compatible way using",
"  extra fields.  As of Zip 3.1, the default is now to store UTF-8 paths and",
"  comments in the main fields and set the UTF-8 flag.  More on this below.",
"",
"  The Unicode extra fields include checksums to verify the Unicode path",
"  goes with the main field path for that entry (as utilities like ZipNote",
"  can rename entries but do not yet update the extra fields).  If these do",
"  not match, use below options to set what Zip does:",
"    -UN=Quit     - if mismatch, exit with error",
"    -UN=Warn     - if mismatch, warn, ignore UTF-8 (default)",
"    -UN=Ignore   - if mismatch, quietly ignore UTF-8",
"    -UN=No       - ignore any UTF-8 paths, use standard paths for all",
"  An exception to -UN=N are entries with the new UTF-8 bit set (instead",
"  of using extra fields).  These are always handled as Unicode.  As Zip 3.1",
"  now defaults to using this bit, the above should not be needed with zip",
"  archives created with Zip 3.1 and later (unless -UN=LOCAL used).",
"",
"  Normally Zip escapes all chars outside the current character set, but",
"  leaves as is any supported chars, which may not be OK in path names.",
"  -UN=Escape escapes any char that is not printable 7-bit ANSI.  For",
"  example:",
"    zip -sU -UN=e archiv",
"  lists the contents of archiv, using Unicode escapes for any character",
"  that is not 7-bit ANSI.",
"",
"  On Unix and Windows ports with Unicode enabled, can use either UTF-8 or",
"  Unicode escapes as well as the local character set (code page in Windows)",
"  in a path or pattern on command line to match files in archive.",
"",
"  Zip 3.1 no longer transliterates characters.  If there is no exact",
"  match for a char in the destination charset, a Unicode escape is used.",
"  This allows consistent matching of paths with Unicode escapes to real",
"  paths.",
"",
"    -UN=UTF8     - store UTF-8 in main path and comment fields",
"    -UN=LOCAL    - store UTF-8 in extra fields (backward compatible)",
"  Zip now stores UTF-8 in entry path and comment fields rather than",
"  use the extra fields.  This is the default for most other major zip",
"  utilities, and so ZIP is now doing this.  Zip used to store UTF-8 in",
"  the extra fields (with escaped versions of entry path and comment in",
"  the main fields for backward compatibility).  Option -UN=LOCAL will",
"  revert Zip back to storing UTF-8 in extra fields for new entries.",
"  If using an old utility that only knows the local charset, you may",
"  want to use -UN=local to store local charset paths in the main fields.",
"",
"    -UN=ShowUTF8 - output file names to console as UTF-8",
"  This option tells Zip to display UTF-8 directly on the console.  For",
"  UTF-8 native ports, this is the default.  For others, the port must be",
"  able to display UTF-8 characters to see them.  On Windows, setting a",
"  command prompt to Lucida Console and code page 65001 allows displaying",
"  some additional character sets, but (in Windows 7) not Asian like",
"  Japanese (displays boxes instead).",
"",
"  For Windows XP and later, Zip 3.1 now reads Unicode args using the",
"  wide command line and processes these as UTF-8.  -UN=show should not",
"  be needed in this case.",
"",
"  The Windows UTF-8 code page remains buggy, at least through Windows 7,",
"  and while Greek may display correctly, without proper fonts loaded",
"  (there seems registry hacks to do this) or maybe the relevant locale set,",
"  some characters, such as Japanese, will display as rectangles.  Copying",
"  and pasting these rectangles to a Unicode aware editor (such as Notepad)",
"  should show that these are, in fact, the actual characters, just that",
"  the console window can't display them.  Pasting or otherwise adding",
"  Unicode to the command line should now pass that Unicode to Zip.",
"",
"  A modern UnZip (such as UnZip 6.00 or later) is now needed to properly",
"  process archives created by Zip 3.1 that contain anything but 7-bit ANSI",
"  characters.  If the zip archive is destined for an old unzip, use",
"  -UN=LOCAL to ensure readability of non-ANSI paths.  Note that even UnZip 6.0",
"  does not display paths in Unicode in Windows consoles or accept Unicode on",
"  the command line.  This should be addressed in UnZip 6.1."
"",
"  Now that Zip 3.1 defaults to storing UTF-8 in the main fields, UTF-8",
"  comments are now supported by default.  UTF-8 comments when -UN=local",
"  is used are still not supported.  They may make the next release.",
"  On Windows, Zip does not yet accept Unicode comments as input.",
"",
"Unix Apple (Mac OS X):",
"  On a Unix Apple system, Zip reads resource information and stores it in",
"  AppleDouble format.  This format uses \"._\" files to store meta data",
"  such as the resource fork.  This format should be compatible with ditto",
"  and similar utilities.",
"    -as       Sequester AppleDouble files in __MACOSX directory",
"  This option will store AppleDouble meta data files Zip creates (using",
"  data from the OS) in a single __MACOSX directory rather than with each",
"  file.  The default is to store each Zip generated AppleDouble \"._\" file",
"  with the main data file it goes with.  For ditto compatibility, these",
"  \"__MACOSX/\" entries still sort after the file they go with in the",
"  archive.",
"    -ad       Sort file system \"._\" files after matching primary file",
"  This option treats file system \"._\" files as AppleDouble files, sorting",
"  them after the file they apparently go with.  This can be useful when a",
"  Unix Apple archive is unpacked on another platform, edited, and then zipped",
"  up for use again on a Unix Apple system.  Using -ad should recreate a valid",
"  ditto compatible archive.  Note that Zip only generates AppleDouble files",
"  on Unix Apple systems.  On these OS, the sorting of AppleDouble files after",
"  the files they go with is automatic.  Using -ad sorts both Zip generated",
"  and existing file system \"._\" files as AppleDouble files.  Negating",
"  (-ad-) disables AppleDouble sorting on MacOS X.  (-ad- is the default on",
"  other OS.)",
"    -df       Save data fork only",
"  Only save the main (data) file.  Resource fork information is not saved",
"  (no AppleDouble \"._\" files are stored in the archive).",
"",
"Symlinks and Mount Points",
"    -y        Store symlinks",
"  If -y, instead of quietly following symlinks, don't follow but store link",
"  and target.  Symlinks have been supported on Unix and other ports for",
"  awhile.  Zip 3.1 now includes support for Windows symlinks, both file and",
"  directory.  UnZip 6.1 or later is needed to list and extract Windows",
"  symlinks.",
"    -yy       Store/skip mount points",
"  Windows now includes various reparse point objects.  By default Zip just",
"  follows most quietly.  With -yy, these are skipped, except for mount",
"  points, which are saved as a directory in archive.  With -yy- (negated -yy)",
"  all reparse point objects (including those to offline storage) are",
"  followed.  Support for storing and restoring mount points is coming soon.",
"",
"Windows long paths:",
"  Zip now includes the ability to read and store long paths on Windows.",
"  This requires UNICODE_SUPPORT be enabled (now the default on Windows).  A",
"  long path is one over MAX_PATH length (260 bytes).  This feature is",
"  enabled by default when UNICODE_SUPPORT is enabled.  The option",
"    -wl (--windows-long-paths)",
"  controls this.  Negating it (-wl-) will tell Zip to ignore long paths",
"  as previous versions of Zip did, leaving them out of the resulting",
"  archive.",
"",
"  Be warned that up to at least Windows 7, Windows Explorer can't read zip",
"  archives that include paths greater than 260 bytes, and other utilities",
"  may have problems with long paths as well.  (For instance, a recent version",
"  of WinZip truncates these paths.)  DO NOT USE THIS FEATURE unless you have",
"  a way to read and process the resulting archive.  It looks like 7-Zip",
"  may support long paths, for instance.  When released, UnZip 6.1 should",
"  support long paths.",
"",
"Security/ACLs/Extended Attributes:",
"    -!  - use any privileges to gain additional access (Windows NT)",
"    -!! - do not include security information (Windows NT)",
"    -X  - will leave out most extra fields, including security info",
"    -EA - sets how to handle extended attributes (NOT YET IMPLEMENTED)",
"",
"Self extractor:",
"    -A        Adjust offsets - a self extractor is created by prepending",
"              extractor executable to archive, but internal offsets are",
"              then off.  Use -A to fix offsets.",
"",
"    -J        Junk sfx - removes prepended extractor executable from",
"              self extractor, leaving a plain zip archive.",
"",
"EBCDIC/MVS (MVS, z/OS, CMS, z/VM):",
"    -a        Translate \"text\" files from EBCDIC to ASCII",
"    -aa       Handle all files as text files, do EBCDIC/ASCII conversions",
"    -MV=m     Set MVS path translation mode.  m is one of:",
"                dots     - store paths as they are (typically aa.bb.cc.dd)",
"                slashes  - change aa.bb.cc.dd to aa/bb/cc/dd",
"                lastdot  - change aa.bb.cc.dd to aa/bb/cc.dd (default)",
"",
"Modifying paths:",
"  Currently there are four options that allow modifying paths as they are",
"  being stored in archive.  At some point a more general path modifying",
"  feature similar to the ZipNote capability is planned.",
"    -pp pfx   prefix string pfx to all paths in archive",
"    -pa pfx   prefix pfx to just added/updated entries in archive",
"    -Cl       store added/updated paths as lowercase",
"    -Cu       store added/updated paths as uppercase",
"",
"  If existing archive myzip.zip contains foo.txt:",
"    zip myzip bar.txt -pa abc_",
"  would result in foo.txt and abc_bar.txt in myzip, while:",
"    zip myzip bar.txt -pp abc_",
"  would result in abc_foo.txt and abc_bar.txt in myzip.",
"",
"  These can be used to put content into a directory:",
"    zip myzip bar.txt -pa abc/",
"  would result in foo.txt and abc/bar.txt in myzip, which has put",
"  bar.txt in the abc directory in the archive.",
"",
"  If existing archive myzip.zip contains foo.txt:",
"    zip myzip bar.txt -Cu",
"  would result in foo.txt and BAR.TXT in myzip.",
"",
"  Be aware that OS path limits still apply to modified paths on",
"  extraction, and some utilities may have problems with long paths.",
"",
"Current directory, temp files:",
"    -cd dir   change current directory",
"              Equivalent to changing current dir before calling zip, except",
"              current dir of caller not impacted.  On Windows, if dir",
"              includes different drive letter, zip changes to that drive.",
"",
"    -b dir    put temp archive file in directory dir",
"              Specifies where to put the temp file that will become",
"              the zip archive.  Allows using seekable temp file when",
"              writing to a \"write once\" CD.  (Such archives may be",
"              compatible with more unzips.)  Using this option could",
"              require an additional file copy if on another device.",
"",
"Zip error codes:",
"  This section to be expanded soon.  Zip error codes are detailed in the",
"  Zip Manual (man page).",
"",
"More option highlights (see manual for additional options and details):",
"    -MM       input patterns must match at least one file and matched files",
"              must be readable or exit with OPEN error and abort archive",
"              (without -MM, both are warnings only, and if unreadable files",
"              are skipped OPEN error (18) returned after archive created)",
"    -nw       no wildcards (wildcards are like any other character)",
"    -sc       show command line arguments as processed, and exit",
"    -sd       show debugging as Zip does each step",
"    -so       show all available options on this system",
"    -X        default=strip old extra fields, -X- keep old, -X strip most",
"    -ws       wildcards don't span directory boundaries in paths",
""
  };

  for (i = 0; i < sizeof(text)/sizeof(char *); i++)
  {
    zprintf("%s\n", text[i]);
  }
# ifdef DOS
  check_for_windows("Zip");
# endif
}


void quick_version()
{
  char cdate[5];

  strncpy(cdate, REVYMD, 4);
  cdate[4] = '\0';
  zfprintf(mesg, "Zip %s (%s)  (c)%s Info-ZIP  http://info-zip.org\n", VERSION, REVYMD, cdate);
}


/*
 * XXX version_info() in a separate file
 */
local void version_info()
/* Print verbose info about program version and compile time options
   to stdout. */
{
  extent i;             /* counter in text arrays */

  /* AES_WG option string storage (with version). */

# ifdef IZ_CRYPT_AES_WG
  static char aes_wg_opt_ver[81];
  static char aes_wg_opt_ver2[81];
  static char aes_wg_opt_ver3[81];
# endif /* def IZ_CRYPT_AES_WG */

  /* Bzip2 option string storage (with version). */

# ifdef BZIP2_SUPPORT
  static char bz_opt_ver[81];
  static char bz_opt_ver2[81];
  static char bz_opt_ver3[81];
# endif

# ifdef LZMA_SUPPORT
  static char lzma_opt_ver[81];
  static char lzma_opt_ver2[81];
# endif

# ifdef PPMD_SUPPORT
  static char ppmd_opt_ver[81];
  static char ppmd_opt_ver2[81];
# endif

# ifdef IZ_CRYPT_TRAD
  static char crypt_opt_ver[81];
  static char crypt_opt_ver2[81];
# endif

  /* Non-default AppleDouble resource fork suffix. */
# ifdef UNIX_APPLE
#  ifndef APPLE_NFRSRC
#   error APPLE_NFRSRC not defined.
   Bad code: error APPLE_NFRSRC not defined.
#  endif
#  if defined(__ppc__) || defined(__ppc64__)
#   if APPLE_NFRSRC
#    define APPLE_NFRSRC_MSG \
     "APPLE_NFRSRC         (\"/..namedfork/rsrc\" suffix for resource fork)"
#   endif /* APPLE_NFRSRC */
#  else /* defined(__ppc__) || defined(__ppc64__) [else] */
#   if ! APPLE_NFRSRC
#    define APPLE_NFRSRC_MSG \
     "APPLE_NFRSRC         (NOT!  \"/rsrc\" suffix for resource fork)"
#   endif /* ! APPLE_NFRSRC */
#  endif /* defined(__ppc__) || defined(__ppc64__) [else] */
# endif /* UNIX_APPLE */

  /* Compile options info array */
  static ZCONST char *comp_opts[] = {
# ifdef APPLE_NFRSRC_MSG
    APPLE_NFRSRC_MSG,
# endif
# ifdef APPLE_XATTR
    "APPLE_XATTR          (Apple extended attributes supported)",
# endif
# ifdef ASM_CRC
    "ASM_CRC              (assembly code used for CRC calculation)",
# endif
# ifdef ASMV
    "ASMV                 (assembly code used for pattern matching)",
# endif
# ifdef BACKUP_SUPPORT
    "BACKUP_SUPPORT       (enable backup options: -BT and related)",
# endif
# ifdef DYN_ALLOC
    "DYN_ALLOC",
# endif
# ifdef MMAP
    "MMAP",
# endif
# ifdef BIG_MEM
    "BIG_MEM",
# endif
# ifdef MEDIUM_MEM
    "MEDIUM_MEM",
# endif
# ifdef SMALL_MEM
    "SMALL_MEM",
# endif
# if defined(DEBUG)
    "DEBUG                (debug/trace mode)",
# endif
# if defined(_DEBUG)
    "_DEBUG               (compiled with debugging)",
# endif
# ifdef USE_EF_UT_TIME
    "USE_EF_UT_TIME       (store Universal Time)",
# endif
# ifdef NTSD_EAS
    "NTSD_EAS             (store NT Security Descriptor)",
# endif
# if defined(WIN32) && defined(NO_W32TIMES_IZFIX)
    "NO_W32TIMES_IZFIX",
# endif
# ifdef VMS
#  ifdef VMSCLI
    "VMSCLI               (VMS-style command-line interface)",
#  endif
#  ifdef VMS_IM_EXTRA
    "VMS_IM_EXTRA         (IM-style (obsolete) VMS file attribute encoding)",
#  endif
#  ifdef VMS_PK_EXTRA
    "VMS_PK_EXTRA         (PK-style (default) VMS file attribute encoding)",
#  endif
# endif /* VMS */
# ifdef WILD_STOP_AT_DIR
    "WILD_STOP_AT_DIR     (wildcards do not cross directory boundaries)",
# endif
# ifdef WIN32_OEM
    "WIN32_OEM            (store file paths on Windows as OEM, except UTF-8)",
# endif
# ifdef BZIP2_SUPPORT
    bz_opt_ver,
    bz_opt_ver2,
    bz_opt_ver3,
# endif
# ifdef LZMA_SUPPORT
    lzma_opt_ver,
    lzma_opt_ver2,
# endif
# ifdef PPMD_SUPPORT
    ppmd_opt_ver,
    ppmd_opt_ver2,
# endif
# ifdef SYMLINKS
#  ifdef VMS
    "SYMLINKS             (symbolic links supported, if C RTL permits)",
#  else
#   ifdef WIN32
    "SYMLINKS             (Windows symbolic links supported)",
    "REPARSE_POINTS       (Windows reparse/mount points supported)",
#   else
    "SYMLINKS             (symbolic links supported)",
#   endif
#  endif
# endif
# ifdef LARGE_FILE_SUPPORT
#  ifdef USING_DEFAULT_LARGE_FILE_SUPPORT
    "LARGE_FILE_SUPPORT   (default settings)",
#  else
    "LARGE_FILE_SUPPORT   (can read and write large files on file system)",
#  endif
# endif
# ifdef ZIP64_SUPPORT
    "ZIP64_SUPPORT        (use Zip64 to store large files in archives)",
# endif
# ifdef UNICODE_SUPPORT
    "UNICODE_SUPPORT      (store and read Unicode paths)",
# endif
# ifdef UNICODE_WCHAR
    "UNICODE_WCHAR        (Unicode support via wchar_t wide functions)",
# endif
# ifdef UNICODE_ICONV
    "UNICODE_ICONV        (Unicode support via iconv)",
# endif
# ifdef UNICODE_FILE_SCAN
    "UNICODE_FILE_SCAN    (file system scans use Unicode)",
# endif

# ifdef WIN32_WIDE_CMD_LINE
    "WIN32_WIDE_CMD_LINE  (Windows wide (Unicode) command line)",
# endif

# ifdef UNIX
    "STORE_UNIX_UIDs_GIDs (store UID, GID (any size) using \"ux\" extra field)",
#  ifdef UIDGID_NOT_16BIT
    "UIDGID_NOT_16BIT     (old Unix 16-bit UID/GID extra field not used)",
#  else
    "UIDGID_16BIT         (old Unix 16-bit UID/GID extra field also used)",
#  endif
# endif

    "SPLIT_SUPPORT        (can read and write split (multi-part) archives)",

# ifdef IZ_CRYPT_TRAD
    crypt_opt_ver,
    crypt_opt_ver2,
#  ifdef ETWODD_SUPPORT
    "ETWODD_SUPPORT       (encrypt Traditional w/o data descriptor if -et)",
#  endif
# endif

# ifdef IZ_CRYPT_AES_WG
    aes_wg_opt_ver,
    aes_wg_opt_ver2,
    aes_wg_opt_ver3,
# endif

# ifdef IZ_CRYPT_AES_WG_NEW
    "IZ_CRYPT_AES_WG_NEW  (AES strong encryption (WinZip/Gladman new))",
# endif

# if defined(IZ_CRYPT_ANY) && defined(PASSWD_FROM_STDIN)
    "PASSWD_FROM_STDIN",
# endif /* defined(IZ_CRYPT_ANY) && defined(PASSWD_FROM_STDIN) */

# ifdef WINDLL
    "WINDLL               (Windows DLL/LIB)",
#  ifdef ZIPLIB
    "ZIPLIB               (compiled as Zip static library)",
#  else
    "ZIPDLL               (compiled as Zip dynamic library)",
#  endif
# else
#  ifdef ZIPLIB
    "ZIPLIB               (compiled as Zip static library)",
#  endif
#  ifdef ZIPDLL
    "ZIPDLL               (compiled as Zip dynamic library)",
#  endif
# endif

# ifdef WINDOWS_LONG_PATHS
    "WINDOWS_LONG_PATHS   (can store paths longer than 260 chars on Windows)",
# endif

    NULL
  };


  for (i = 0; i < sizeof(copyright)/sizeof(char *); i++)
  {
    zprintf(copyright[i], "zip");
    zprintf("\n");
  }

  for (i = 0; i < sizeof(versinfolines)/sizeof(char *); i++)
  {
    zprintf(versinfolines[i], "Zip", VERSION, REVDATE);
    zprintf("\n");
  }

  version_local();

  /* was puts() */
  zprintf("%s", "Zip special compilation options/features:\n");
# if WSIZE != 0x8000
  zprintf("        WSIZE=%u\n", WSIZE);
# endif


/*
  RBW  --  2009/06/23  --  TEMP TEST for devel...drop when done.
  Show what some critical sizes are. For z/OS, long long and off_t
  must be 8 bytes (off_t is a typedefed long long), and fseeko must
  take zoff_t as its 2nd arg.
*/
# if 0
  zprintf("* size of int:         %d\n", sizeof(int));        /* May be 4 */
  zprintf("* size of long:        %d\n", sizeof(long));       /* May be 4 */
  zprintf("* size of long long:   %d\n", sizeof(long long));  /* Must be 8 */
  zprintf("* size of off_t:       %d\n", sizeof(off_t));      /* Must be 8 */
#  ifdef __LF
  zprintf("__LF is defined.\n");
  zprintf("  off_t must be defined as a long long\n");
#  else /* def __LF [else] */           /* not all compilers know elif */
#   ifdef _LP64
  zprintf("_LP64 is defined.\n");
  zprintf("  off_t must be defined as a long\n");
#   else /* def _LP64 [else] */
  zprintf("Neither __LF nor _LP64 is defined.\n");
  zprintf("  off_t must be defined as an int\n");
#   endif /* def _LP64 [else] */
#  endif /* def __LF [else] */
# endif /* 0 */


  /* Fill in bzip2 version.  (32-char limit valid as of bzip 1.0.3.) */
# ifdef BZIP2_SUPPORT
  sprintf( bz_opt_ver,
    "BZIP2_SUPPORT        (bzip2 library version %.32s)", BZ2_bzlibVersion());
  sprintf( bz_opt_ver2,
    "    bzip2 code and library copyright (c) Julian R Seward");
  sprintf( bz_opt_ver3,
    "    (See the bzip2 license for terms of use)");
  i++;
# endif

  /* Fill in LZMA version. */
# ifdef LZMA_SUPPORT
  sprintf(lzma_opt_ver,
    "LZMA_SUPPORT         (LZMA compression, ver %s)",
    MY_VERSION);
  sprintf(lzma_opt_ver2,
    "    LZMA code placed in public domain by Igor Pavlov");
  i++;
# endif

  /* Fill in PPMd version. */
# ifdef PPMD_SUPPORT
  sprintf(ppmd_opt_ver,
    "PPMD_SUPPORT         (PPMd compression, ver %s)",
    MY_VERSION);
  sprintf(ppmd_opt_ver2,
    "    PPMd code placed in public domain by Igor Pavlov");
  i++;
# endif

# ifdef IZ_CRYPT_TRAD
  sprintf(crypt_opt_ver,
    "IZ_CRYPT_TRAD        (Traditional (weak) encryption, ZCRYPT ver %d.%d%s)",
    CR_MAJORVER, CR_MINORVER, CR_BETA_VER);
  sprintf(crypt_opt_ver2,
    "    Copyright (c) Info-ZIP; parts placed in public domain (see below)");
# endif /* def IZ_CRYPT_TRAD */

  /* Fill in IZ_AES_WG version. */
# ifdef IZ_CRYPT_AES_WG
  sprintf(aes_wg_opt_ver,
    "IZ_CRYPT_AES_WG      (AES encryption (IZ WinZip/Gladman), ver %d.%d%s)",
    IZ_AES_WG_MAJORVER, IZ_AES_WG_MINORVER, IZ_AES_WG_BETA_VER);
  sprintf(aes_wg_opt_ver2,
    "    AES code copyright (c) Dr Brian Gladman");
  sprintf(aes_wg_opt_ver3,
    "    (See the AES license for terms of use)");
# endif


  for (i = 0; (int)i < (int)(sizeof(comp_opts)/sizeof(char *) - 1); i++)
  {
    zprintf("        %s\n",comp_opts[i]);
  }

# ifdef USE_ZLIB
  if (strcmp(ZLIB_VERSION, zlibVersion()) == 0)
    zprintf("        USE_ZLIB             (zlib version %s)\n", ZLIB_VERSION);
  else
    zprintf("        USE_ZLIB             (compiled with version %s, using %s)\n",
      ZLIB_VERSION, zlibVersion());
  i++;  /* zlib use means there IS at least one compilation option */
# endif

  if (i != 0)
    zprintf("\n");

/* Any CRYPT option sets "i", indicating at least one compilation option. */

# ifdef IZ_CRYPT_TRAD
  for (i = 0; i < sizeof(cryptnote)/sizeof(char *); i++)
  {
    zprintf("%s\n", cryptnote[i]);
  }
# endif /* def IZ_CRYPT_TRAD */

# ifdef IZ_CRYPT_AES_WG
#  ifdef IZ_CRYPT_TRAD
  zprintf("\n");
#  endif /* def IZ_CRYPT_TRAD */
  for (i = 0; i < sizeof(cryptAESnote)/sizeof(char *); i++)
  {
    zprintf("%s\n", cryptAESnote[i]);
  }
# endif /* def IZ_CRYPT_AES_WG */

# ifdef IZ_CRYPT_AES_WG_NEW
#  if defined( IZ_CRYPT_TRAD) || defined( IZ_CRYPT_AES_WG)
  zprintf("\n");
#  endif /* defined( IZ_CRYPT_TRAD) || defined( IZ_CRYPT_AES_WG) */
  for (i = 0; i < sizeof(cryptAESnote)/sizeof(char *); i++)
  {
    zprintf("%s\n", cryptAESnote[i]);
  }
# endif /* def IZ_CRYPT_AES_WG_NEW */

# if defined( IZ_CRYPT_ANY) || defined( IZ_CRYPT_AES_WG_NEW)
  zprintf("\n");
# endif
  
  if (charsetname)
  {
    zprintf("Current charset/code page:  %s\n", charsetname);
    zprintf("\n");
  }

  /* Show option environment variables (unconditionally). */
  show_env(0);

# ifdef DOS
  check_for_windows("Zip");
# endif
}


#ifndef PROCNAME
/* Default to case-sensitive matching of archive entries for the modes
   that specifically operate on archive entries, as this archive may
   have come from a system that allows paths in the archive to differ
   only by case.  Except for adding ARCHIVE (copy mode), this is how it
   was done before.  Note that some case-insensitive ports (WIN32, VMS)
   define their own PROCNAME() in their respective osdep.h that use the
   filter_match_case flag set to FALSE by the -ic option to enable
   case-insensitive archive entry mathing. */
#  define PROCNAME(n) procname(n, (action == ARCHIVE || action == DELETE \
                                   || action == FRESHEN) \
                                  && filter_match_case)
#endif /* PROCNAME */


#if !defined(WINDLL) && !defined(MACOS)
local void zipstdout()
/* setup for writing zip file on stdout */
{
  mesg = stderr;
  if (isatty(1))
    ziperr(ZE_PARMS, "cannot write zip file to terminal");
  if ((zipfile = malloc(4)) == NULL)
    ziperr(ZE_MEM, "was processing arguments (1)");
  strcpy(zipfile, "-");
  /*
  if ((r = readzipfile()) != ZE_OK)
    ziperr(r, zipfile);
  */
}
#endif /* !WINDLL && !MACOS */


#ifndef ZIP_DLL_LIB
/* ------------- test the archive ------------- */

local char *unzip_feature(bit_mask)
  unsigned int bit_mask;
{
  char *feature;
  char *feature_string;

  if (bit_mask == UNZIP_UNSHRINK_SUPPORT) {
    feature = "Unshrink Support";
  }
  else if (bit_mask == UNZIP_DEFLATE64_SUPPORT) {
    feature = "Deflate64 Support";
  }
  else if (bit_mask == UNZIP_UNICODE_SUPPORT) {
    feature = "Unicode Support";
  }
  else if (bit_mask == UNZIP_WIN32_WIDE) {
    feature = "Win32 Wide";
  }
  else if (bit_mask == UNZIP_LARGE_FILE_SUPPORT) {
    feature = "Large File Support";
  }
  else if (bit_mask == UNZIP_ZIP64_SUPPORT) {
    feature = "Zip64 Support";
  }
  else if (bit_mask == UNZIP_BZIP2_SUPPORT) {
    feature = "bzip2 Compression";
  }
  else if (bit_mask == UNZIP_LZMA_SUPPORT) {
    feature = "LZMA Compression";
  }
  else if (bit_mask == UNZIP_PPMD_SUPPORT) {
    feature = "PPMd Compression";
  }
  else if (bit_mask == UNZIP_IZ_CRYPT_TRAD) {
    feature = "Traditional (ZipCrypto) Encryption";
  }
  else if (bit_mask == UNZIP_IZ_CRYPT_AES_WG) {
    feature = "AES WG Encryption";
  }
  else if (bit_mask == UNZIP_KEYFILE) {
    feature = "Keyfiles";
  }
  else if (bit_mask == UNZIP_SPLITS_BASIC) {
    feature = "Split Archives (Basic)";
  }
  else if (bit_mask == UNZIP_SPLITS_MEDIA) {
    feature = "Split Archives (Media)";
  }
  else if (bit_mask == UNZIP_WIN_LONG_PATH) {
    feature = "Windows Long Paths";
  }
  else {
    feature = "Undefined";
  }

  feature_string = string_dup(feature, "unzip_feature", 0);
  return feature_string;
}


local ulg get_needed_unzip_features()
{
  ulg needed_unzip_features;
  int zip64_may_be_needed = 0;
  struct zlist *z;
#ifdef ZIP64_SUPPORT
  uzoff_t entry_count = 0;
#endif
  ush comp_method = 0;
#ifdef WINDOWS_LONG_PATHS
  int has_windows_long_path = 0;
#endif

  /* bit 0 = 1 indicates that feature set is initialized (valid) */
  needed_unzip_features = 1;

  for (z = zfiles; z != NULL; z = z->nxt) {
    /* compression */
    comp_method = z->how;
    if (z->how == BZIP2) {
      needed_unzip_features |= UNZIP_BZIP2_SUPPORT;
    }
    else if (z->how == LZMA) {
      needed_unzip_features |= UNZIP_LZMA_SUPPORT;
    }
    else if (z->how == PPMD) {
      needed_unzip_features |= UNZIP_PPMD_SUPPORT;
    }
#ifdef IZ_CRYPT_AES_WG
    /* AES encryption */
    if (z->how == 99) {
      ush aes_vendor_version = 0;
      int aes_strength = 0;
      int result;

      needed_unzip_features |= UNZIP_IZ_CRYPT_AES_WG;
      /* read the AES_WG extra field */
      result = read_crypt_aes_cen_extra_field(z,
                                              &aes_vendor_version,
                                              &aes_strength,
                                              &comp_method);
      if (result == 0) {
        /* no AES_WG extra field */
        zipwarn("file AES WG encrypted, but missing extra field: ", z->oname);
      }
      else {
        /* note actual compression method */
        if (comp_method == BZIP2) {
          needed_unzip_features |= UNZIP_BZIP2_SUPPORT;
        }
        else if (comp_method == LZMA) {
          needed_unzip_features |= UNZIP_LZMA_SUPPORT;
        }
        else if (comp_method == PPMD) {
          needed_unzip_features |= UNZIP_PPMD_SUPPORT;
        }
      }
    } /* 99 */
#endif /* IZ_CRYPT_AES_WG */
#ifdef IZ_CRYPT_TRAD
    /* Trad encryption */
    if (z->flg & GPBF_00_ENCRYPTED) {
      needed_unzip_features |= UNZIP_IZ_CRYPT_TRAD;
    }
#endif

#ifdef ZIP64_SUPPORT
    /* Need Zip64? */
    /* Once we need Zip64, can stop checking */
    if (!zip64_may_be_needed) {
      char *pExtra;

      if (force_zip64 == 1) {
        zip64_may_be_needed = 1;
      }

      /* Count the entries */
      entry_count++;
      if (entry_count >= 0x10000) {
        /* Too many entries, need Zip64 */
        zip64_may_be_needed = 1;
      }

      /* Is there a Zip64 local extra field? */
      pExtra = get_extra_field(EF_ZIP64, z->extra, z->ext);
      if (pExtra) {
        /* yes */
        zip64_may_be_needed = 1;
      }
      else if (exceeds_zip64_threshold(z)) {
        zip64_may_be_needed = 1;
      }
    }
#endif /* ZIP64_SUPPORT */

#ifdef WINDOWS_LONG_PATHS
    if (!has_windows_long_path && z->namew) {
      int len = (int)wcslen(z->namew);
      if (len > MAX_PATH && include_windows_long_paths) {
        if (has_windows_long_path == 0)
          zipwarn("Archive will contain Windows long path", "");
        has_windows_long_path = 1;
      }
    }
#endif /* WINDOWS_LONG_PATHS */

  } /* for zfiles */

  { /* found list */
  /* For new items in found list, we just check the command line for
     what was used and assume all are needed. */
    int i;
    
    /* check the global compression method */
    if (method == BZIP2) {
      needed_unzip_features |= UNZIP_BZIP2_SUPPORT;
    }
    else if (method == LZMA) {
      needed_unzip_features |= UNZIP_LZMA_SUPPORT;
    }
    else if (method == PPMD) {
      needed_unzip_features |= UNZIP_PPMD_SUPPORT;
    }

#if 0
    /* Check for any specified suffix methods.  We assume that
       any method with a suffix associated with it may be used. */
    for (i = 0; mthd_lvl[i].method >= 0; i++)
    {
      if (mthd_lvl[i].suffixes) {
        if (mthd_lvl[i].method == BZIP2) {
          needed_unzip_features |= UNZIP_BZIP2_SUPPORT;
        }
        else if (mthd_lvl[i].method == LZMA) {
          needed_unzip_features |= UNZIP_LZMA_SUPPORT;
        }
        else if (mthd_lvl[i].method == PPMD) {
          needed_unzip_features |= UNZIP_PPMD_SUPPORT;
        }
      }
    } /* for */
#endif

#ifdef IZ_CRYPT_TRAD
    /* encryption */
    if (encryption_method == TRADITIONAL_ENCRYPTION) {
      needed_unzip_features |= UNZIP_IZ_CRYPT_TRAD;
    }
#endif
#ifdef IZ_CRYPT_AES_WG
    if (encryption_method == AES_128_ENCRYPTION ||
        encryption_method == AES_192_ENCRYPTION ||
        encryption_method == AES_256_ENCRYPTION) {
      needed_unzip_features |= UNZIP_IZ_CRYPT_AES_WG;
    }
#endif

    {
      struct flist *f;
      int how;

#ifdef ZIP64_SUPPORT
      if (force_zip64 == 1) {
        zip64_may_be_needed = 1;
      }
#endif

      /* go through found list */
      for (f = found; f != NULL; ) {
#ifdef ZIP64_SUPPORT
        /* Continue counting the entries */
        entry_count++;
        if (entry_count >= 0x10000) {
          /* Too many entries, need Zip64 */
          zip64_may_be_needed = 1;
        }
#endif

        /* For size check start with global compression method. */
        how = method;
        if (how == -1) {
          how = DEFLATE;
        }

        /* There should probably be a RISCOS version of this. */
#ifndef RISCOS
        /* Check if suffix in list and so gets another method. */
        for (i = 0; mthd_lvl[i].method >= 0; i++)
        {
          if (suffixes(f->name, mthd_lvl[i].suffixes)) {
            if (mthd_lvl[i].method == STORE) {
              how = STORE;
            }
            else if (mthd_lvl[i].method == DEFLATE) {
              how = DEFLATE;
            }
            else if (mthd_lvl[i].method == BZIP2) {
              how = BZIP2;
              needed_unzip_features |= UNZIP_BZIP2_SUPPORT;
            }
            else if (mthd_lvl[i].method == LZMA) {
              how = LZMA;
              needed_unzip_features |= UNZIP_LZMA_SUPPORT;
            }
            else if (mthd_lvl[i].method == PPMD) {
              how = PPMD;
              needed_unzip_features |= UNZIP_PPMD_SUPPORT;
            }
            break;
          }
        } /* for method */
#endif

#ifdef ZIP64_SUPPORT
        if (!zip64_may_be_needed) {
          /* Now check if this file with this size and method exceeds
             the Zip64 threshold */
          struct zlist tempz;

          tempz.name = f->name;
          tempz.len = f->usize;
          tempz.thresh_mthd = how;

          if (exceeds_zip64_threshold(&tempz)) {
            zip64_may_be_needed = 1;
          }
        } /* !zip64_may_be_needed */
#endif /* ZIP64_SUPPORT */

#ifdef WINDOWS_LONG_PATHS
        if (!has_windows_long_path && f->namew) {
          int len = (int)wcslen(f->namew);
          if (len > MAX_PATH && include_windows_long_paths) {
            if (has_windows_long_path == 0)
              zipwarn("Archive will contain Windows long path", "");
            has_windows_long_path = 1;
          }
        }
#endif /* WINDOWS_LONG_PATHS */

        f = f->nxt;
      } /* for found */

    }
  } /* found list */

  if (zip64_may_be_needed) {
    needed_unzip_features |= UNZIP_ZIP64_SUPPORT;
  }

  if (split_method == 1) {
    needed_unzip_features |= UNZIP_SPLITS_BASIC;
  }
  if (split_method == 2) {
    needed_unzip_features |= UNZIP_SPLITS_MEDIA;
  }

#ifdef WINDOWS_LONG_PATHS
  if (has_windows_long_path) {
    needed_unzip_features |= UNZIP_WIN_LONG_PATH;
  }
#endif

  if (keyfile) {
    needed_unzip_features |= UNZIP_KEYFILE;
  }

  return needed_unzip_features;
}


#define MAX_UNZIP_COMMAND_LEN    MAX_ZIP_ARG_SIZE /* was 4000 */
#define MAX_UNZIP_VER_LINE_LEN   1000
#define MAX_UNZIP_VERSION_LINES  100

local void get_unzip_features(unzippath, version, features)
  char *unzippath;   /* path to unzip */
  float *version;    /* returned version of unzip */
  ulg *features;     /* returned feature set */
{
  /* Spawn "unzip -v" for unzip path given and get the version of unzip
   * as well as a list of supported features returned as an unsigned int
   * feature set.  If unzippath == NULL, use "unzip".
   *
   * The returned feature set is used to verify that the unzip has the
   * features needed to check the archive Zip is creating.  Bit 0 is
   * always set to show the feature set has been initialized.
   *
   * At this time there seems no need to actually check the version of
   * UnZip, as the compiled in features are really what we're interested
   * in.
   *
   * The MSDOS/WINDOWS feature where, if unzip is not found, the zip path
   * is checked for unzip, has been removed.  If restored, zippath would
   * need to be made available, probably as a parameter.
   */

  char cmd[MAX_UNZIP_COMMAND_LEN + 4];
  char buf[MAX_UNZIP_VER_LINE_LEN + 1];
  FILE *unzip_out = NULL;
  float UnZip_Version = 0.0;
  unsigned long unzip_supported_features = 1;
  int line = 0;
  int success = 1;
  char *path_unzip;
  int good_popen = 0;
  char unzip_version_string[MAX_UNZIP_VER_LINE_LEN + 1];
  int at_least_61c = 0;
  char c;
  int i;
  char *number_string;
  char *beta_string;

  if (show_what_doing) {
    zipmessage("sd:  get unzip version and features", "");
  }

  cmd[0] = '\0';

  if (unzippath) {
    path_unzip = unzippath;
  }
  else {
    /* assume unzip */
    path_unzip = "unzip";
  }

  if (strlen(path_unzip) > MAX_UNZIP_COMMAND_LEN) {
    sprintf(errbuf, "unzip path length greater than max (%d): %s",
            MAX_UNZIP_COMMAND_LEN, path_unzip);
    ZIPERR(ZE_PARMS, errbuf);
  }

  strcat(cmd, path_unzip);
  strcat(cmd, " -v");

  errno = 0;

#  if defined(ZOS)
  /*  RBW  --  More z/OS - MVS nonsense.  Cast shouldn't be needed. */
  /*  Real fix is probably to find where popen is defined...        */
  /*  The compiler seems to think it's returning an int.            */

  /* Assuming the cast is needed for z/OS, probably can put it in
      the main code version and drop this #if.  Other ports shouldn't
      have trouble with the cast, but left it as is for now.  - EG   */
  if ((unzip_out = (FILE *) popen(cmd, "r")) == NULL)
#  else
  if ((unzip_out = popen(cmd, "r")) == NULL)
#  endif
  {
    zperror("unzip pipe error");
    ZIPERR(ZE_UNZIP, "unable to get information from unzip");
  }
  else
  { /* pipe open */
    if (errno == 0) {
      good_popen = 1;
    }
    else {
      /* Now all ports just error if unzip is not found. */
      zipwarn("could not find unzip", "");
      perror("popen unzip");
      pclose(unzip_out);
      ZIPERR(ZE_UNZIP, "could not find unzip");

#if 0
      /* The "try zip path" feature, where, when the unzip path
         does not lead to unzip, the path to zip is used to look
         for unzip there, is now dropped.  This feature was only
         supported on MSDOS and WINDOWS.  If it was to be restored,
         here is probably where it would be. */

#  if (defined(MSDOS) && !defined(__GO32__)) || defined(__human68k__)
      /* For MSDOS and WINDOWS, if unzip not found, try directory
         where zip is. */

      if (errno != ENOENT) {
        zipwarn("error running unzip, trying where zip is", "");
        perror("popen");
        pclose(unzip_out);
        ZIPERR(ZE_UNZIP, "could not find unzip");
      }
      else {  /* errno == ENOENT */
        /* no such file or entry */
        char *p;

        zipwarn("could not find unzip, trying where zip is", "");

        /* look for last \ in zip path */
        p = MBSRCHR(zippath, '\\');
        if (!p) {
          /* look for last / in zip path */
          p = MBSRCHR(zippath, '/');
        }
        if (p) {
          /* chop path after last \ */
          *(p + 1) = '\0';
          if ((path_unzip = (char *)malloc(strlen(zippath) + 6)) == NULL) {
            ZIPERR(ZE_MEM, "path_unzip");
          }
          strcat(path_unzip, "unzip");
        }

        /* Length checking needed. */
        strcpy(cmd, path_unzip);
        strcat(cmd, " -v");
        free(path_unzip);
        /* repeat popen here */
        if ((unzip_out = popen(cmd, "r")) == NULL)
        {
          zperror("unzip pipe error");
          ZIPERR(ZE_UNZIP, "unable to get information from unzip");
        }
        good_popen = 1;

      } /* errno == ENOENT */
#  else
      /* Other ports just fail if unzip not where it should be. */

      zipwarn("could not find unzip", "");
      perror("popen");
      pclose(unzip_out);
      ZIPERR(ZE_UNZIP, "could not find unzip");

#  endif /* (defined(MSDOS) && !defined(__GO32__)) || defined(__human68k__) */
#endif /* 0 */

    } /* errno */
  } /* pipe open */

  if (good_popen) {
    /* read the unzip version information */

    /* get first line */
    if (fgets(buf, MAX_UNZIP_VER_LINE_LEN, unzip_out) == NULL) {
      zipwarn("failed to get information from UnZip", "");
      success = 0;
    } else {
      /* the first line should start with the version */
      line++;
      if (sscanf(buf, "UnZip %s ", unzip_version_string) < 1) {
        zipwarn("unexpected output of UnZip -v: ", buf);
        success = 0;
      }
      if (success) {
        for (i = 0; (c = unzip_version_string[i]); i++) {
          if (!(isdigit(c) || c == '.'))
            break;
        }
        number_string = string_dup(unzip_version_string, "number_string", 0);
        number_string[i] = '\0';
        if (unzip_version_string[i]) {
          beta_string = string_dup(unzip_version_string + i, "beta_string", 0);
          if (sscanf(number_string, "%f ", &UnZip_Version) < 1) {
            zipwarn("unexpected UnZip version: ", unzip_version_string);
            success = 0;
          }
        }
      }

      if (success) {
        if (show_what_doing) {
          sprintf(errbuf, "sd:  UnZip %4.2f (%s) number %s beta %s",
                  UnZip_Version, unzip_version_string, number_string,
                  beta_string);
          zipmessage(errbuf, "");
        }

        /* Currently only UnZip 6.1c and later support basic split archives.
           Basic splits requires all the splits to be in the same place as
           the .zip file.  As of this writing, UnZip does not support splits
           spread across multiple media, and so probably can't test an archive
           made with -sp. */
        if ((UnZip_Version == 6.1 &&             /* UnZip 6.1 and either */
             (strcmp(beta_string, "c") == 0 ||   /*  final public beta "c" */
              strcmp(beta_string, "d") >= 0)) || /*  or "d" and later betas */
            (UnZip_Version > 6.1)) {             /* or UnZip 6.11 or later */
          at_least_61c = 1;
          if (show_what_doing) {
            zipmessage("sd:  at least UnZip 6.1c with basic split support", "");
          }
        }
        if (at_least_61c) {
          unzip_supported_features |= UNZIP_SPLITS_BASIC;
        }

        /* read rest of version information */
        while (fgets(buf, MAX_UNZIP_VER_LINE_LEN, unzip_out)) {
          line++;
          if (line > MAX_UNZIP_VERSION_LINES) {
            sprintf(errbuf, "too many unzip version lines (%d)", line);
            zipwarn(errbuf, "");
            success = 0;
            break;
          }
          /* look for compilation options */
          if (strstr(buf, "    UNSHRINK_SUPPORT"))
            unzip_supported_features |= UNZIP_UNSHRINK_SUPPORT;
          else if (strstr(buf, "    DEFLATE64_SUPPORT"))
            unzip_supported_features |= UNZIP_DEFLATE64_SUPPORT;
          else if (strstr(buf, "    UNICODE_SUPPORT"))
            unzip_supported_features |= UNZIP_UNICODE_SUPPORT;
          else if (strstr(buf, "    WIN32_WIDE"))
            unzip_supported_features |= UNZIP_WIN32_WIDE;
          else if (strstr(buf, "    LARGE_FILE_SUPPORT"))
            unzip_supported_features |= UNZIP_LARGE_FILE_SUPPORT;
          else if (strstr(buf, "    ZIP64_SUPPORT"))
            unzip_supported_features |= UNZIP_ZIP64_SUPPORT;
          else if (strstr(buf, "    BZIP2_SUPPORT"))
            unzip_supported_features |= UNZIP_BZIP2_SUPPORT;
          else if (strstr(buf, "    LZMA_SUPPORT"))
            unzip_supported_features |= UNZIP_LZMA_SUPPORT;
          else if (strstr(buf, "    PPMD_SUPPORT"))
            unzip_supported_features |= UNZIP_PPMD_SUPPORT;
          else if (strstr(buf, "    IZ_CRYPT_TRAD"))
            unzip_supported_features |= UNZIP_IZ_CRYPT_TRAD;
          else if (strstr(buf, "    IZ_CRYPT_AES_WG"))
            unzip_supported_features |= UNZIP_IZ_CRYPT_AES_WG;
          /* This is to support UnZip 5.52. */
          else if (strstr(buf, "[decryption, version 2.9"))
            unzip_supported_features |= UNZIP_IZ_CRYPT_TRAD;
        } /* while */
      } /* read rest of popen output */
    } /* first line */
    pclose(unzip_out);
  } /* good_popen */

  if (success == 0) {
    /* something went wrong */
    ZIPERR(ZE_UNZIP, "could not read unzip version information");
  }

  if (show_what_doing) {
    sprintf(errbuf, "sd:  got unzip supported feature set:  %04lx",
            unzip_supported_features);
    zipmessage(errbuf, "");
  }

  *version = UnZip_Version;
  *features = unzip_supported_features;
}


local int check_unzip_version(unzippath, needed_unzip_features)
  char *unzippath;
  ulg needed_unzip_features;
{
  /* Here is where we need to check for the version of unzip the user
   * has.  If creating a Zip64 archive, we need UnZip 6 or later or
   * testing may fail.
   */
  /* This has been updated to now check for the various compilation options,
   * allowing verification that a specific feature exists in this instance
   * of UnZip.
   *
   * To pass this check, each bit (feature) set in needed_unzip_features
   * must also be set in unzip_supported_features derived from "unzip -v".
   * At this time there seems no need to actually check the version of
   * UnZip, as the compiled in features are really what we're interested
   * in.
   */

  unsigned long needed;
  unsigned long supported;
  unsigned long mask;
  int bit;
  int unzip_supports_needed_features = 1;

  if (show_what_doing) {
    zipmessage("sd:  check unzip version", "");
  }

  if (unzip_supported_features == 0) {
    /* == 0 means haven't checked unzip yet */
    /* unzip_version and unzip_supported_features are global to zip.c */
    get_unzip_features(unzippath, &unzip_version, &unzip_supported_features);
  }
  
  if (show_what_doing) {
    sprintf(errbuf, "sd:  unzip version:  %4.2f", unzip_version);
    zipmessage(errbuf, "");
    sprintf(errbuf, "sd:  unzip supported features:  %04lx",
             unzip_supported_features);
    zipmessage(errbuf, "");
    sprintf(errbuf, "sd:  needed unzip features:     %04lx",
             needed_unzip_features);
    zipmessage(errbuf, "");
  }

  for (bit = 0; bit <= MAX_UNZIP_FEATURE_BIT; bit++) {
    mask = 1 << bit;
    needed = needed_unzip_features & mask;
    supported = unzip_supported_features & mask;
    if (needed && !supported) {
      char *feature = unzip_feature(mask);
      sprintf(errbuf, "unzip does not support \"%s\" used by archive", feature);
      zipwarn(errbuf, "");
      free(feature);
      unzip_supports_needed_features = 0;
    }
  }

  /* We no longer look at version for Zip64, as above is more specific. */
#  if 0
  if (unzip_version < 6.0 && zip64_archive) {
    sprintf(buf, "Found UnZip version %4.2f", UnZip_Version);
    zipwarn(buf, "");
    zipwarn("Need UnZip 6.00 or later to test this Zip64 archive", "");
    return 0;
  }
#  endif

  return unzip_supports_needed_features;
}


# ifdef UNIX

/* strcpy_qu()
 * Copy a string (src), adding apostrophe quotation, to dst.
 * Return actual length.
 *
 * This is used to build the string used to call unzip for testing
 * an archive.
 *
 * The destination string must be allocated at least 2 times + 2 the
 * space needed by the source string. It probably would be cleaner
 * to allocate the destination here and return a pointer to it.
 */
local int strcpy_qu( dst, src)
 char *dst;
 char *src;
{
  char *cp1;
  char *cp2;
  int len;

  /* Surround the archive name with apostrophes.
   * Convert an embedded apostrophe to the required mess.
   */

  *dst = '\'';                             /* Append an initial apostrophe. */
  len = 1;                                 /* One char so far. */

  /* Copy src to dst, converting >'< to >'"'"'<. */
  cp1 = src;
  while ((cp2 = strchr( cp1, '\'')))       /* Find next apostrophe. */
  {
    strncpy((dst + len), cp1, (cp2 - cp1));  /* Append chars up to next apos. */
    len += cp2 - cp1;                      /* Increment the dst length. */
    strncpy((dst + len), "'\"'\"'", 5);    /* Replace src >'< with >'"'"'<. */
    len += 5;                              /* Increment the dst length. */
    cp1 = cp2 + 1;                         /* Advance beyond src apostrophe. */
  }
  strcpy((dst + len), cp1);                /* Append the remainder of src. */
  len += strlen(cp1);                      /* Increment the dst length. */
  strcpy((dst + len), "'");                /* Append a final apostrophe. */
  len += 1;                                /* Increment the dst length. */

  return len;                              /* Help the caller count chars. */
}
# endif /* def UNIX */


# if 0
/* Quote double quotes in string (" -> \\\").  This reduces to \" in
 * the string, which inserts just " into the string.  This may not
 * work for all shells, as some may expect quotes to be quotes as
 * repeated quotes ("") to insert a single quote.
 *
 * This version does not handle character sets where \0 is used for
 * other than just a NULL terminator.  Passwords should be just ASCII,
 * but should also work for UTF-8 and most other character sets.)
 */
local char* quote_quotes(instring)
  char *instring;
{
  char *temp;
  char *outstring;
  int i;
  int j;
  char c;

  /* Get a temp string that is long enough to handle a large number
     of embedded quotes. */
  if ((temp = malloc(strlen(instring) * 4 + 100)) == NULL) {
    ziperr(ZE_MEM, "quote_quotes (1)");
  }
  temp[0] = '\0';
  for (i = 0, j = 0; (c = instring[i]); i++) {
    if (c == '"') {
      temp[j++] = '\\';
      temp[j++] = '"';
    } else {
      temp[j++] = c;
    }
  }
  temp[j] = '\0';

  /* Get a string that is the size of the resulting command string. */
  if ((outstring = malloc(strlen(temp) + 1)) == NULL) {
    ziperr(ZE_MEM, "quote_quotes (2)");
  }
  /* Copy over quoted string. */
  strcpy(outstring, temp);
  free(temp);

  return outstring;
}
# endif /* 0 */


void warn_unzip_return(status)
  int status;
{
  /* Output warning appropriate for UnZip return code. */
  
  if (status == 0) {
    zipwarn("unzip returned 0 (success)", "");
  } else if (status == 1) {
    zipwarn("unzip error 1 (warning)", "");
  } else if (status == 2) {
    zipwarn("unzip error 2 (error in zipfile)", "");
  } else if (status == 3) {
    zipwarn("unzip error 3 (severe error in zipfile)", "");
  } else if (status == 4) {
    zipwarn("unzip error 4 (ran out of memory - init)", "");
  } else if (status == 5) {
    zipwarn("unzip error 5 (ran out of memory - password)", "");
  } else if (status == 6) {
    zipwarn("unzip error 6 (ran out of memory - file decom)", "");
  } else if (status == 7) {
    zipwarn("unzip error 7 (ran out of memory - memory decom)", "");
  } else if (status == 8) {
    zipwarn("unzip error 8 (ran out of memory - unknown)", "");
  } else if (status == 9) {
    zipwarn("unzip error 9 (zipfile not found)", "");
  } else if (status == 10) {
    zipwarn("unzip error 10 (bad or illegal parameters)", "");
  } else if (status == 11) {
    zipwarn("unzip error 11 (no files found)", "");
  } else if (status == 19) {
    zipwarn("unzip error 19 (error in compile options)", "");
  } else if (status == 50) {
    zipwarn("unzip error 50 (disk full)", "");
  } else if (status == 51) {
    zipwarn("unzip error 51 (unexpected eof)", "");
  } else if (status == 80) {
    zipwarn("unzip error 80 (user hit ^C)", "");
  } else if (status == 81) {
    zipwarn("unzip error 81 (unsupported compression/encryption)", "");
  } else if (status == 82) {
    zipwarn("unzip error 82 (bad password)", "");
  } else if (status == 83) {
    zipwarn("unzip error 83 (unzip can't handle large file)", "");
  } else if (status == 84) {
    zipwarn("unzip error 84 (bad destination directory)", "");
  }
}


local char *parse_TT_string(unzip_string, temp_zip_path,
                            passwd, keyfile, key, unzip_being_used)
  char *unzip_string;    /* -TT string */
  char *temp_zip_path;   /* path to temp zip file */
  char *passwd;          /* password string (not key) */
  char *keyfile;         /* keyfile path (can be NULL) */
  char *key;             /* key = passwd + keyfile password content (text only) */
  int *unzip_being_used; /* returns 1 if {u} (UnZip being used), else 0 */
{
  /* Given the -TT unzip command string provided by user, find {...}
   * placeholders and replace with values as appropriate.
   *
   * The caller needs to surround input parameters with quotes as needed as
   * well as deal with embedded quotes.
   *
   * Returns malloc string of unzip command, or
   * NULL on failure.
   */
  char *cmd;
  int content_start = 0;
  int content_end = 0;
  int i;
  int j;
  int tempname_added = 0;
  int unzip_string_len;
  char unzip_cmd[ERRBUF_SIZE + 1];


  /* We now support additional formats.  For backward compatibility
      these are still supported:
        {}    = where path of temp archive goes in command to UnZip
        {p}   = where password goes in command to UnZip
      However these cause trouble when tests may or may not include
      passwords.  They also can't handle keyfiles well.
      The following are now supported to address these issues:
        {p: argument}  = if password used, insert this argument
        {k: argument}  = if keyfile used, insert this argument
      In each case, "argument" is replaced with what should go in the
      UnZip command line.  So, for instance, if an UnZip that supports
      both passwords and keyfiles is being used, the following command
      could be specified with -TT:
        unzip  -tqq  {p: -P {p}}  {k: -kf {k}}  {}
      which gets converted to:
        unzip  -tqq  -P password  -kf keyfile  temp_archive_path
      If no keyfile is specified to Zip, Zip passes this to UnZip:
        unzip  -tqq  -P password  temp_archive_path
      and if no password or no keyfile, UnZip gets:
        unzip  -tqq  temp_archive_path
      Zip now also supports:
        {k}            = path of keyfile as given to Zip
        {y}            = the entire key (password + keyfile contents)
      However, if the keyfile contains binary, {y} should not be used.
      If the user prefers to have UnZip prompt for the password, then
      just {k: ...} can be specified.
        {u}            = assume error return is from unzip
      When {u} is added, it gets parsed away but it sets a flag that
      tells Zip to output unzip descriptions for any errors.  Without
      {u}, just the return error number is displayed.  So a complete
      -TT command string for using unzip might be:
        unzip  -tqq  {u}  {p: -P {p}}  {k: -kf {k}}  {}
      But if UnZip is being used, the new -TU is probably the better
      choice.
      */

  *unzip_being_used = 0;

  j = 0;
  unzip_cmd[0] = '\0';

  /* Look for {.  Should be followed by either }, p, k, or y.  If
     p (password) or k (keyfile), look for : next.  If so, only include
     the following content if password or keyfile, respectively, were
     provided to zip.  Replace {}, {p}, {k}, or {y} with the respective
     content.  Exit with error if format flawed.  (Seems safer to just
     give up than to press on with flawed command string.) */
  unzip_string_len = (int)strlen(unzip_string);
  for (i = 0; i < unzip_string_len; ) {
    if (unzip_string[i] == '{') {
      int ii = i + 1;
      if (unzip_string[ii] == '}') {
        /* {} */
        /* temp file path goes here */
        unzip_cmd[j] = '\0';
        strcat(unzip_cmd, temp_zip_path);
        j += (int)strlen(temp_zip_path);
        tempname_added = 1;
        i += 2;
      }
      else if (unzip_string[ii] == 'p') {
        /* password */
        ii++;
        if (unzip_string[ii] == '}') {
          /* {p} */
          unzip_cmd[j] = '\0';
          strcat(unzip_cmd, passwd);
          j += (int)strlen(passwd);
          i += 3;
        }
        else if (unzip_string[ii] == ':') {
          /* {p: ...} */
          int n = ii + 1;
          int bcount = 0;
          if (unzip_string[n] == ' ') {
            /* skip leading space */
            n++;
          }
          content_start = n;
          /* a properly formed expression should look something
             like {p: -P={p}}, with only two levels of braces
             that all close */
          for (; unzip_string[n]; n++) {
            if (unzip_string[n] == '{')
              bcount++;
            if (unzip_string[n] == '}')
              bcount--;
            if (bcount > 1 || bcount < 0)
              break;
          } /* for */
          content_end = n;
          if (bcount < 0 && unzip_string[n] != '}') {
            sprintf(errbuf, "at %d in -TT string - no end }", n);
            zipwarn(errbuf, "");
            return NULL;
          }
          else if (bcount >= 0) {
            sprintf(errbuf, "at %d in -TT string - unclosed }", n);
            zipwarn(errbuf, "");
            return NULL;
          }
          else {
            /* looks good, so process it */
            if (passwd) {
              if (pass_pswd_to_unzip) {
                for (n = content_start; n < content_end; n++) {
                  /* this is the content of "{p: ...}", which is
                      "..." and likely something like "-P {p}" */
                  if (unzip_string[n] == '{') {
                    if (unzip_string[n + 1] == 'p' && unzip_string[n + 2] == '}') {
                      /* {p} */
                      unzip_cmd[j] = '\0';
                      strcat(unzip_cmd, passwd);
                      j += (int)strlen(passwd);
                      n += 3;
                    }
                    else if (unzip_string[n + 1] == 'y' && unzip_string[n + 2] == '}') {
                      /* {y} */
                      /* this should be checked for text only */
                      unzip_cmd[j] = '\0';
                      strcat(unzip_cmd, key);
                      j += (int)strlen(key);
                      n += 3;
                    }
                    else {
                      sprintf(errbuf, "at %d in -TT string - { that is not {p}", n);
                      zipwarn(errbuf, "");
                      return NULL;
                    }
                  }
                  else {
                    /* just copy content */
                    unzip_cmd[j++] = unzip_string[n];
                  }
                } /* for */
              } /* pass_pswd_to_unzip */
              else {
                zipmessage(
              "  note:  add -pu to pass password to unzip (could be unsecure)",
                  "");
              }
              i = content_end + 1;
            } /* passwd */
          } /* looks good */
        }
        else {
          sprintf(errbuf, "at %d in -TT string - after {p not } or :", ii);
          zipwarn(errbuf, "");
          return NULL;
        }
      } /* {p */
      else if (unzip_string[ii] == 'k') {
        /* keyfile */
        ii++;
        if (unzip_string[ii] == '}') {
          /* {k} */
          unzip_cmd[j] = '\0';
          strcat(unzip_cmd, keyfile);
          j += (int)strlen(keyfile);
          i += 3;
        }
        else if (unzip_string[ii] == ':') {
          /* {k: ...} */
          int n = ii + 1;
          int bcount = 0;
          if (unzip_string[n] == ' ') {
            /* skip leading space */
            n++;
          }
          content_start = n;
          /* a properly formed expression should look something
             like {k: -kf={k}}, with only two levels of braces
             that all close */
          for (; unzip_string[n]; n++) {
            if (unzip_string[n] == '{')
              bcount++;
            if (unzip_string[n] == '}')
              bcount--;
            if (bcount > 1 || bcount < 0)
              break;
          } /* for */
          content_end = n;
          if (bcount < 0 && unzip_string[n] != '}') {
            /* bad format - must end on a brace */
            sprintf(errbuf, "at %d in -TT string - no end }", n);
            zipwarn(errbuf, "");
            return NULL;
          }
          else if (bcount >= 0) {
            /* bad format - unclosed brace */
            /* skip { and return to loop */
            sprintf(errbuf, "at %d in -TT string - unclosed }", n);
            zipwarn(errbuf, "");
            return NULL;
          }
          else {
            /* looks good, so process it */
            if (keyfile) {
              for (n = content_start; n < content_end; n++) {
                /* this is the content of "{k: ...}", which is
                    "..." and likely something like "-kf {k}" */
                if (unzip_string[n] == '{') {
                  if (unzip_string[n + 1] == 'k' && unzip_string[n + 2] == '}') {
                    /* {k} */
                    unzip_cmd[j] = '\0';
                    strcat(unzip_cmd, keyfile);
                    j += (int)strlen(keyfile);
                    n += 3;
                  }
                  else {
                    sprintf(errbuf, "at %d in -TT string - { that is not {k}", n);
                    zipwarn(errbuf, "");
                    return NULL;
                  }
                }
                else {
                  /* just copy content */
                  unzip_cmd[j++] = unzip_string[n];
                }
              } /* for */
            }
            i = content_end + 1;
          } /* looks good */
        }
        else {
          sprintf(errbuf, "at %d in -TT string - after {k not } or :", ii);
          zipwarn(errbuf, "");
          return NULL;
        }
      } /* {k */
      else if (unzip_string[ii] == 'y') {
        /* {y} entire key */
        /* this should be checked for binary */
        ii++;
        if (unzip_string[ii] == '}') {
          /* {y} */
          if (key && pass_pswd_to_unzip) {
            unzip_cmd[j] = '\0';
            strcat(unzip_cmd, key);
            j += (int)strlen(key);
            i += 3;
          }
        }
        else {
          sprintf(errbuf, "at %d in -TT string - must be {y}", ii);
          zipwarn(errbuf, "");
          return NULL;
        }
      } /* {y */
      else if (unzip_string[ii] == 'u') {
        /* {u} testing with UnZip */
        ii++;
        if (unzip_string[ii] == '}') {
          /* {u} */
          *unzip_being_used = 1;
          i += 3;
        }
        else {
          sprintf(errbuf, "at %d in -TT string - must be {u}", ii);
          zipwarn(errbuf, "");
          return NULL;
        }
      } /* {u */
      else {
        sprintf(errbuf, "at %d in -TT string - unknown {...} format", i);
        zipwarn(errbuf, "");
        return NULL;
      }
    } /* found { */
    else {
      unzip_cmd[j++] = unzip_string[i++];
    }
  } /* for */

  /* make sure command string is terminated */
  unzip_cmd[j] = '\0';

  if (!tempname_added) {
    /* append tempname to end */
    strcat(unzip_cmd, " ");
    strcat(unzip_cmd, temp_zip_path);
  }

  cmd = string_dup(unzip_cmd, "parse_TT_string", 0);

  return cmd;
}


local char *build_unzip_command(unzip_path, temp_zip_path, passwd, keyfile)
  char *unzip_path;      /* path to unzip */
  char *temp_zip_path;   /* path to temp zip file */
  char *passwd;          /* password string (not key) */
  char *keyfile;         /* keyfile path (can be NULL) */
{
  /* Build the command string from the given components, accounting for
   * existence of password or keyfile.  This is used whenever -TT is not
   * used.
   *
   * The caller needs to surround input parameters with quotes as needed as
   * well as deal with embedded quotes.
   *
   * Returns malloc string of unzip command, or
   * NULL on failure.
   */
  char *cmd;
  char unzip_cmd[MAX_UNZIP_COMMAND_LEN + 1];
  int len = 0;

  unzip_cmd[0] = '\0';

  if (unzip_path) {
    len += (int)strlen(unzip_path);
    if (len > MAX_UNZIP_COMMAND_LEN) {
      ZIPERR(ZE_UNZIP, "unzip command too long");
    }
    strcat(unzip_cmd, unzip_path);
  }
  else {
    len += (int)strlen("unzip");
    if (len > MAX_UNZIP_COMMAND_LEN) {
      ZIPERR(ZE_UNZIP, "unzip command too long");
    }
    strcat(unzip_cmd, "unzip");
  }

  if (unzip_verbose) {
    len += (int)strlen(" -t");
    if (len > MAX_UNZIP_COMMAND_LEN) {
      ZIPERR(ZE_UNZIP, "unzip command too long");
    }
    strcat(unzip_cmd, " -t");
  }
  else {
    len += (int)strlen(" -tqq");
    if (len > MAX_UNZIP_COMMAND_LEN) {
      ZIPERR(ZE_UNZIP, "unzip command too long");
    }
    strcat(unzip_cmd, " -tqq");
  }

#ifdef QDOS
  len += strlen(" -Q4");
  if (len > MAX_UNZIP_COMMAND_LEN) {
    ZIPERR(ZE_UNZIP, "unzip command too long");
  }
  strcat(unzip_cmd, " -Q4");
#endif

  if (passwd) {
    if (pass_pswd_to_unzip) {
      len += (int)strlen(" -P ") + (int)strlen(passwd);
      if (len > MAX_UNZIP_COMMAND_LEN) {
        ZIPERR(ZE_UNZIP, "unzip command too long");
      }
      strcat(unzip_cmd, " -P ");
      strcat(unzip_cmd, passwd);
    }
    else {
      zipmessage(
        "  note:  add -pu to pass password to unzip (could be unsecure)", "");
    }
  }

  if (keyfile) {
    len += (int)strlen(" -kf ") + (int)strlen(keyfile);
    if (len > MAX_UNZIP_COMMAND_LEN) {
      ZIPERR(ZE_UNZIP, "unzip command too long");
    }
    strcat(unzip_cmd, " -kf ");
    strcat(unzip_cmd, keyfile);
  }

  /* append tempname to end */
  len += (int)strlen(" ") + (int)strlen(temp_zip_path);
  if (len > MAX_UNZIP_COMMAND_LEN) {
    ZIPERR(ZE_UNZIP, "unzip command too long");
  }
  strcat(unzip_cmd, " ");
  strcat(unzip_cmd, temp_zip_path);

  cmd = string_dup(unzip_cmd, "build_unzip_command", 0);

  return cmd;
}


/* quote_arg()
 *
 * Add quotation and/or escapes to a shell (VMS: DCL) argument string
 * appropriate to the local operating system or shell  (Unix, Windows,
 * etc.).  This is mainly used to build the command line to pass to
 * UnZip (or other application when -TT used) to test an archive.
 * Return malloc()'d result.
 *
 *    All:     Add " at beginning and end.
 *    MSDOS:   % -> "^%"
 *             " -> \""
 *    Unix:    ! -> "'!'"
 *             $ -> \$
 *             \ -> \\
 *             ` -> \`
 *    Non-VMS: " -> \"
 *    VMS:     " -> """
 *
 * On VMS, quoted double apostrophes are also special.  Currently not
 * handled.  (How?  Quotation marks are needed for (upper-)case
 * preservation.  Double apostrophes in quotation marks are interpreted
 * (symbol evaluation).  SMS sees no way to handle "fr''ed".  "fr'""'ed"
 * becomes >fr'"'ed<, for example.)  Not a problem for file specs, but
 * imposes a restriction on passwords.
 */
local char *quote_arg(instring)
  char *instring;
{
  int i;
  int j;
  char *tempstring;
  char *outstring;
  char c;

  if (instring == NULL)
    return NULL;

#ifdef MSDOS
# define QA_FACTOR 4            /* Worst case (MSDOS): % -> "^%"  */

#else /* not MSDOS */
# ifdef VMS
#  define QA_FACTOR 3           /* Worst case (VMS): " -> """  */

# else /* not MSDOS or VMS */
#  define QA_FACTOR 5           /* Worst case (Unix): ! -> "'!'"  */
# endif /* VMS [else] */
#endif /* MSDOS [else] */

#define QA_INCR 2               /* Surrounding quotation marks. */

  i = QA_FACTOR * (int)strlen(instring) + QA_INCR + 1;
  if ((tempstring = (char *)malloc(i)) == NULL) {
    ZIPERR(ZE_MEM, "quote_arg");
  }

  j = 0;

  tempstring[j++] = '\"';       /* Surrounding quotation mark (start). */

  for (i = 0; instring[i]; i++) {
    c = instring[i];

#ifdef MSDOS /* or Windows */
    if (c == '%')               /* Percent. */
    {
      tempstring[j++] = '"';    /* Add (closing) quotation mark. */
      tempstring[j++] = '^';    /* Add caret escape. */
      tempstring[j++] = '%';    /* Original character (%). */
      c = '"';                  /* Prepare (re-opening) quotation mark. */
    }
    else if (c == '"')          /* Quotation mark. */
    {
      tempstring[j++] = '\\';   /* Add backslash (escape). */
      tempstring[j++] = '"';    /* Add quote (acts as closing and literal). */
    }
#else /* not def MSDOS */

# ifdef VMS
    if (c == '"')               /* Quotation mark. */
    {
      tempstring[j++] = '"';    /* Add two quotation marks. */
      tempstring[j++] = '"';
    }
# else /* not def VMS */

    /* UNIX is default for others */

    if (c == '"')               /* Quotation mark. */
    {
      tempstring[j++] = '\\';   /* Add backslash (escape). */
    }
    else if (c == '!')          /* Exclamation.  (Inefficient.) */
    {
      tempstring[j++] = '"';    /* Add (closing) quotation mark. */
      tempstring[j++] = '\'';   /* Add (opening) apostrophe. */
      tempstring[j++] = '!';    /* Original character (!). */
      tempstring[j++] = '\'';   /* Add (closing) apostrophe. */
      c = '"';                  /* Prepare (re-opening) quotation mark. */
    }
    else if ((c == '$') ||      /* Dollar sign. */
             (c == '`') ||      /* Grave accent (backtick). */
             (c == '\\'))       /* Backslash. */
    {
      tempstring[j++] = '\\';   /* Add backslash (escape). */
    }

# endif /* def VMS [else] */
#endif /* def MSDOS [else] */

    tempstring[j++] = c;        /* Original (or other last) character. */
  }

  tempstring[j++] = '\"';       /* Surrounding quotation mark (end). */

  tempstring[j] = '\0';
  outstring = string_dup(tempstring, "quote_arg", 0);
  free(tempstring);

  return outstring;
}


local void check_zipfile(zipname, zippath, is_temp)
  char *zipname;   /* name of archive to test */
  char *zippath;   /* our path */
  int is_temp;     /* true if testing temp file */
  /* Invoke unzip -t on the given zip file */
{
  int result;      /* result of unzip invocation */
  char *cmd;
  char *qzipname;
  char *qpasswd;
  char *qkeyfile;
  char *qkey;
  int unzip_being_used;

# if (defined(MSDOS) && !defined(__GO32__)) || defined(__human68k__)

#  ifdef MSDOS
  /* MSDOS and Windows */

  /* Add quotes for MSDOS.  8/11/04 */

  /* accept spaces in name and path */
  /* double quotes are illegal in MSDOS/Windows paths */

  qzipname = quote_arg(zipname);
#  else
  qzipname = string_dup(zipname, "qzipname");
#  endif

  if (show_what_doing) {
    if (unzip_string) {
      sprintf(errbuf, "sd:  unzip -TT string:  %s", unzip_string);
      zipmessage(errbuf, "");
    }
    if (unzip_path) {
      sprintf(errbuf, "sd:  unzip -TU path:    %s", unzip_path);
      zipmessage(errbuf, "");
    }
  }

  /* Put double quotes around and escape special characters in each string. */
  qpasswd = quote_arg(passwd);
  qkeyfile = quote_arg(keyfile);
  qkey = quote_arg(key);

  if (show_what_doing) {
    sprintf(errbuf, "sd:  qzipname:  %s", qzipname);
    zipmessage(errbuf, "");
    sprintf(errbuf, "sd:  qpasswd:   %s", qpasswd);
    zipmessage(errbuf, "");
    sprintf(errbuf, "sd:  qkeyfile:  %s", qkeyfile);
    zipmessage(errbuf, "");
    sprintf(errbuf, "sd:  qkey:      %s", qkey);
    zipmessage(errbuf, "");
  }

  if (unzip_string) {
    /* Build command string to execute.  Test app may not be unzip. */
    cmd = parse_TT_string(unzip_string, qzipname, qpasswd, qkeyfile, qkey,
                          &unzip_being_used);
  }
  else {
    /* Build command string to execute.  Test app assumed to be unzip. */
    /* build_unzip_command() handles case where unzip_path is NULL. */
    cmd = build_unzip_command(unzip_path, qzipname, qpasswd, qkeyfile);
    unzip_being_used = 1;
  }

  if (qzipname)
    free(qzipname);
  if (qpasswd)
    free(qpasswd);
  if (qkeyfile)
    free(qkeyfile);
  if (qkey)
    free(qkey);

  if (cmd == NULL) {
    if (unzip_string) {
      ZIPERR(ZE_PARMS, "badly formatted -TT string");
    }
    else {
      ZIPERR(ZE_PARMS, "bad unzip path (-TU)");
    }
  }

  if (show_what_doing) {
    sprintf(errbuf, "sd:  unzip command:     %s", cmd);
    zipmessage(errbuf, "");
  }

  /* Skip unzip version check for -TT as don't know what app being used. */
  if (!unzip_string) {
    if (check_unzip_version(unzip_path, needed_unzip_features) == 0) {
      ZIPERR(ZE_UNZIP, zipfile);
    }
  }

  result = system(cmd);

  if (result == -1) {
    /* system() call failed */
    zperror("test zip archive");
  }
  else if (result == 0) {
    /* worked */
  }
  else { /* result > 0 */
    if (unzip_being_used) {
#if 0
    /* Now we only interpret unzip error if we "know" unzip was used. */

    /* guess if unzip was command used */
    if (string_find(cmd, "unzip ", CASE_INS, 1) == 0 ||
        string_find(cmd, "/unzip ", CASE_INS, 1) != NO_MATCH) {
      /* "unzip " at beginning or "/unzip " somewhere in command line */
#endif
      warn_unzip_return(result);
    }
    else {
    /* Since we don't know what app was used to test the archive,
      we don't know what the return status means and so can't
      return more than just the error number. */
      sprintf(errbuf, "test returned error %d", result);
      zipwarn(errbuf, "");
    }
  }  /* result > 0 */
    
  if (unzip_string) {
    free(unzip_string);
    unzip_string = NULL;
  }
  if (unzip_path) {
    free(unzip_path);
    unzip_path = NULL;
  }
  free(cmd);


  if (result != 0)


# else /* (MSDOS && !__GO32__) || __human68k__ [else] */

  /* Non-MSDOS/Windows case */

  int len;
  char *t;    /* temp string */

  if (show_what_doing) {
    if (unzip_string) {
      sprintf(errbuf, "sd:  unzip -TT string:  %s", unzip_string);
      zipmessage(errbuf, "");
    }
    if (unzip_path) {
      sprintf(errbuf, "sd:  unzip -TU path:    %s", unzip_path);
      zipmessage(errbuf, "");
    }
    if (!unzip_string && !unzip_path) {
      sprintf(errbuf, "sd:  using default unzip");
      zipmessage(errbuf, "");
    }
  }

  /* Quote each arg (and add appropriate escapes). */
  qzipname = quote_arg(zipname);
  qpasswd = quote_arg(passwd);
  qkeyfile = quote_arg(keyfile);
  qkey = quote_arg(key);

  if (show_what_doing) {
    sprintf(errbuf, "sd:  qzipname:  %s", qzipname);
    zipmessage(errbuf, "");
    sprintf(errbuf, "sd:  qpasswd:   %s", qpasswd);
    zipmessage(errbuf, "");
    sprintf(errbuf, "sd:  qkeyfile:  %s", qkeyfile);
    zipmessage(errbuf, "");
    sprintf(errbuf, "sd:  qkey:      %s", qkey);
    zipmessage(errbuf, "");
  }

  if (unzip_string) {
    /* -TT
     * Build command string to execute.  Test app may not be unzip.
     */
    cmd = parse_TT_string(unzip_string, qzipname, qpasswd, qkeyfile, qkey,
                          &unzip_being_used);
  }
  else {
    /* -TU or default
     * Build command string to execute.  Test app assumed to be unzip.
     * build_unzip_command() handles case where unzip_path is NULL.
     */
    cmd = build_unzip_command(unzip_path, qzipname, qpasswd, qkeyfile);
    unzip_being_used = 1;
  }

  if (qzipname)
    free(qzipname);
  if (qpasswd)
    free(qpasswd);
  if (qkeyfile)
    free(qkeyfile);
  if (qkey)
    free(qkey);

  if (cmd == NULL) {
    if (unzip_string) {
      ZIPERR(ZE_PARMS, "badly formatted -TT string");
    }
    else {
      ZIPERR(ZE_PARMS, "bad unzip path (-TU)");
    }
  }

  if (show_what_doing) {
    sprintf(errbuf, "sd:  unzip command:     %s", cmd);
    zipmessage(errbuf, "");
  }

  /* Only check unzip version if not using -TT. */
  if (!unzip_string) {
    if (check_unzip_version(unzip_path, needed_unzip_features) == 0) {
      ZIPERR(ZE_UNZIP, zipfile);
    }
  }

  /* Test the archive. */
  result = system(cmd);

#  ifdef VMS
  /* Convert success severity to 0, others to non-zero. */
  result = ((result & STS$M_SUCCESS) == 0);
#  endif /* def VMS */
  free(cmd);
  cmd = NULL;
  if (result)
# endif /* (MSDOS && !__GO32__) || __human68k__ [else] */
  {
    sprintf(errbuf, "test of %s FAILED\n", zipfile);
    zfprintf(mesg, "%s", errbuf);
    if (logfile) {
      zfprintf(logfile, "%s", errbuf);
    }
    if (!is_temp) {
      sprintf(errbuf,
        "as multi-disk archive tested or --out used, failed archive remains\n");
      zfprintf(mesg, "%", errbuf);
      if (logfile) {
        zfprintf(logfile, "%s", errbuf);
      }
    }
    ziperr(ZE_TEST, "original files unmodified");
  }
  if (noisy) {
    zfprintf(mesg, "test of %s OK\n", zipfile);
    fflush(mesg);
  }
  if (logfile) {
    zfprintf(logfile, "test of %s OK\n", zipfile);
    fflush(logfile);
  }
}


/* ------------- end test archive ------------- */
#endif /* ndef ZIP_DLL_LIB */


/* get_filters() is replaced by add_filter() and filterlist_to_patterns()
local int get_filters(argc, argv)
*/

/* The filter patterns for options -x, -i, and -R are
   returned by get_option() one at a time, so use a linked
   list to store until all args are processed.  Then convert
   to array for processing.

   flag = 'i', 'x', or 'R'
   pattern = the pattern to match against, or @file to read patterns from file
 */

/* add a filter to the linked list */
local int add_filter(flag, pattern)
  int flag;
  char *pattern;
{
  char *iname;
  int pathput_save;
  FILE *fp;
  char *p = NULL;
  struct filterlist_struct *filter = NULL;

  /* should never happen */
  if (flag != 'R' && flag != 'x' && flag != 'i') {
    ZIPERR(ZE_LOGIC, "bad flag to add_filter");
  }
  if (pattern == NULL) {
    ZIPERR(ZE_LOGIC, "null pattern to add_filter");
  }

  if (pattern[0] == '@') {
    /* read file with 1 pattern per line */
    if (pattern[1] == '\0') {
      ZIPERR(ZE_PARMS, "missing file after @");
    }
    fp = zfopen(pattern + 1, "r");
    if (fp == NULL) {
      sprintf(errbuf, "%c pattern file '%s'", flag, pattern);
      ZIPERR(ZE_OPEN, errbuf);
    }

#ifdef UNICODE_SUPPORT
# if 0
    /* As we now check each arg and path separately, checking the
       file as a whole is no longer that useful. */
    if (is_utf8_file(fp, NULL, NULL, NULL, NULL)) {
      sprintf(errbuf, "-%c include file is UTF-8", flag);
      zipmessage(errbuf, "");
    }
# endif
#endif

    while ((p = getnam(fp)) != NULL) {
      int len;

      len = (int)strlen(p);
      if (len > 0) {
        if ((filter = (struct filterlist_struct *) malloc(sizeof(struct filterlist_struct))) == NULL) {
          ZIPERR(ZE_MEM, "adding filter (1)");
        }
        if (filterlist == NULL) {
          /* first filter */
          filterlist = filter;         /* start of list */
          lastfilter = filter;
        } else {
          lastfilter->next = filter;   /* link to last filter in list */
          lastfilter = filter;
        }

        /* always store full path for pattern matching */
        pathput_save = pathput;
        pathput = 1;
        iname = ex2in(p, 0, (int *)NULL);
        pathput = pathput_save;
        free(p);

        if (iname != NULL) {
          lastfilter->pattern = in2ex(iname);
          free(iname);
        } else {
          lastfilter->pattern = NULL;
        }
        lastfilter->flag = flag;
        pcount++;
        lastfilter->next = NULL;
      } /* len > 0 */
    } /* while */
    fclose(fp);
  } else {
    /* single pattern */
    if ((filter = (struct filterlist_struct *) malloc(sizeof(struct filterlist_struct))) == NULL) {
      ZIPERR(ZE_MEM, "adding filter (2)");
    }
    if (filterlist == NULL) {
      /* first pattern */
      filterlist = filter;         /* start of list */
      lastfilter = filter;
    } else {
      lastfilter->next = filter;   /* link to last filter in list */
      lastfilter = filter;
    }

    /* always store full path for pattern matching */
    pathput_save = pathput;
    pathput = 1;
    iname = ex2in(pattern, 0, (int *)NULL);
    pathput = pathput_save;

    if (iname != NULL) {
       lastfilter->pattern = in2ex(iname);
       free(iname);
    } else {
      lastfilter->pattern = NULL;
    }
    lastfilter->flag = flag;
    pcount++;
    lastfilter->next = NULL;
  }

  return pcount;
}

/* convert list to patterns array */
local int filterlist_to_patterns()
{
  unsigned i;
  struct filterlist_struct *next = NULL;

  if (pcount == 0) {
    patterns = NULL;
    return 0;
  }
  if ((patterns = (struct plist *) malloc((pcount + 1) * sizeof(struct plist)))
      == NULL) {
    ZIPERR(ZE_MEM, "was creating pattern list");
  }

  for (i = 0; i < pcount && filterlist != NULL; i++) {
    switch (filterlist->flag) {
      case 'i':
        icount++;
        break;
      case 'R':
        Rcount++;
        break;
    }
    patterns[i].select = filterlist->flag;
#ifdef UNICODE_SUPPORT
    if (is_utf8_string(filterlist->pattern, NULL, NULL, NULL, NULL)) {
      patterns[i].uzname = filterlist->pattern;
      patterns[i].zname = utf8_to_local_string(filterlist->pattern);
    } else {
      patterns[i].uzname = local_to_utf8_string(filterlist->pattern);
      patterns[i].zname = filterlist->pattern;
    }
    patterns[i].duzname = escapes_to_utf8_string(filterlist->pattern);
#else
    patterns[i].uzname = NULL;
    patterns[i].zname = filterlist->pattern;
#endif
    next = filterlist->next;
    free(filterlist);
    filterlist = next;
  }

  return pcount;
}


/* add a file argument to linked list */
local long add_name(filearg, verbatim)
  char *filearg;
  int verbatim;
{
  char *name = NULL;
  struct filelist_struct *fileentry = NULL;

  if ((fileentry = (struct filelist_struct *) malloc(sizeof(struct filelist_struct))) == NULL) {
    ZIPERR(ZE_MEM, "adding file (1)");
  }
  if ((name = malloc(strlen(filearg) + 1)) == NULL) {
    ZIPERR(ZE_MEM, "adding file (2)");
  }
  strcpy(name, filearg);
  fileentry->verbatim = verbatim;
  fileentry->next = NULL;
  fileentry->name = name;
  if (filelist == NULL) {
    /* first file argument */
    filelist = fileentry;         /* start of list */
    lastfile = fileentry;
  } else {
    lastfile->next = fileentry;   /* link to last name in list */
    lastfile = fileentry;
  }
  filearg_count++;

  return filearg_count;
}


/* add incremental archive path to linked list */
local long add_apath(path)
  char *path;
{
  char *name = NULL;
  struct filelist_struct *apath_entry = NULL;

  if ((apath_entry = (struct filelist_struct *) malloc(sizeof(struct filelist_struct))) == NULL) {
    ZIPERR(ZE_MEM, "adding incremental archive path entry");
  }

  if ((name = malloc(strlen(path) + 1)) == NULL) {
    ZIPERR(ZE_MEM, "adding incremental archive path");
  }
  strcpy(name, path);
  apath_entry->next = NULL;
  apath_entry->name = name;
  if (apath_list == NULL) {
    /* first apath */

    apath_list = apath_entry;         /* start of list */
    last_apath = apath_entry;
  } else {
    last_apath->next = apath_entry;   /* link to last apath in list */
    last_apath = apath_entry;
  }
  apath_count++;

  return apath_count;
}


/* Running Stats
   10/30/04 */

local int DisplayRunningStats()
{
  char tempstrg[100];

  if (mesg_line_started && !display_globaldots) {
    zfprintf(mesg, "\n");
    mesg_line_started = 0;
  }
  if (logfile_line_started) {
    zfprintf(logfile, "\n");
    logfile_line_started = 0;
  }
  if (display_volume) {
    if (noisy) {
      zfprintf(mesg, "%lu>%lu: ", current_in_disk + 1, current_disk + 1);
      mesg_line_started = 1;
    }
    if (logall) {
      zfprintf(logfile, "%lu>%lu: ", current_in_disk + 1, current_disk + 1);
      logfile_line_started = 1;
    }
  }
  if (display_counts) {
    if (noisy) {
      zfprintf(mesg, "%3ld/%3ld ", files_so_far, files_total - files_so_far);
      mesg_line_started = 1;
    }
    if (logall) {
      zfprintf(logfile, "%3ld/%3ld ", files_so_far, files_total - files_so_far);
      logfile_line_started = 1;
    }
  }
  if (display_bytes) {
    /* since file sizes can change as we go, use bytes_so_far from
       initial scan so all adds up */
    WriteNumString(bytes_so_far, tempstrg);
    if (noisy) {
      zfprintf(mesg, "[%4s", tempstrg);
      mesg_line_started = 1;
    }
    if (logall) {
      zfprintf(logfile, "[%4s", tempstrg);
      logfile_line_started = 1;
    }
    if (bytes_total >= bytes_so_far) {
      WriteNumString(bytes_total - bytes_so_far, tempstrg);
      if (noisy) {
        zfprintf(mesg, "/%4s] ", tempstrg);
      }
      if (logall) {
        zfprintf(logfile, "/%4s] ", tempstrg);
      }
    } else {
      WriteNumString(bytes_so_far - bytes_total, tempstrg);
      if (noisy) {
        zfprintf(mesg, "-%4s] ", tempstrg);
      }
      if (logall) {
        zfprintf(logfile, "-%4s] ", tempstrg);
      }
    }
  }
  if (display_time || display_est_to_go || display_zip_rate) {
    /* get current time */
    time(&clocktime);
  }
  if (display_time) {
    struct tm *now;

    now = localtime(&clocktime);

    /* avoid strftime() to keep old systems (like old VMS) happy */
    sprintf(errbuf, "%02d/%02d:%02d:%02d", now->tm_mday, now->tm_hour,
                                           now->tm_min, now->tm_sec);
    /* strftime(errbuf, 50, "%d/%X", now); */
    /* strcpy(errbuf, asctime(now)); */

    if (noisy) {
      zfprintf(mesg, errbuf);
      mesg_line_started = 1;
    }
    if (logall) {
      zfprintf(logfile, errbuf);
      logfile_line_started = 1;
    }
  }
#ifdef ENABLE_ENTRY_TIMING
  if (display_est_to_go || display_zip_rate) {
    /* get estimated time to go */
    uzoff_t bytes_to_go = bytes_total - bytes_so_far;
    zoff_t elapsed_time_in_usec;
    uzoff_t elapsed_time_in_10msec;
    uzoff_t bytes_per_second = 0;
    uzoff_t time_to_go = 0;
    uzoff_t secs;
    uzoff_t mins;
    uzoff_t hours;
    static uzoff_t lasttime = 0;
    uzoff_t files_to_go = 0;


    if (files_total > files_so_far)
      files_to_go = files_total - files_so_far;

    current_time = get_time_in_usec();
    lasttime = current_time;
    secs = current_time / 1000000;
    mins = secs / 60;
    secs = secs % 60;
    hours = mins / 60;
    mins = mins % 60;


    /* First time through we just finished the file scan and
       are starting to actually zip entries.  Use that time
       to calculate zipping speed. */
    if (start_zip_time == 0) {
      start_zip_time = current_time;
    }
    elapsed_time_in_usec = current_time - start_zip_time;
    elapsed_time_in_10msec = elapsed_time_in_usec / 10000;

    if (bytes_to_go < 1)
      bytes_to_go = 1;

    /* Seems best to wait about 90 msec for counts to stablize
       before displaying estimates of time to go. */
    if (display_est_to_go && elapsed_time_in_10msec > 8) {
      /* calculate zipping rate */
      bytes_per_second = (bytes_so_far * 100) / elapsed_time_in_10msec;
      /* if going REALLY slow, assume at least 1 byte per second */
      if (bytes_per_second < 1) {
        bytes_per_second = 1;
      }

      /* calculate estimated time to go based on rate */
      time_to_go = bytes_to_go / bytes_per_second;

      /* add estimate for console listing of entries */
      time_to_go += files_to_go / 40;

      secs = (time_to_go % 60);
      time_to_go /= 60;
      mins = (time_to_go % 60);
      time_to_go /= 60;
      hours = time_to_go;

      if (hours > 10) {
        /* show hours */
        sprintf(errbuf, "<%3dh to go>", (int)hours);
      } else if (hours > 0) {
        /* show hours */
        float h = (float)((int)hours + (int)mins / 60.0);
        sprintf(errbuf, "<%3.1fh to go>", h);
      } else if (mins > 10) {
        /* show minutes */
        sprintf(errbuf, "<%3dm to go>", (int)mins);
      } else if (mins > 0) {
        /* show minutes */
        float m = (float)((int)mins + (int)secs / 60.0);
        sprintf(errbuf, "<%3.1fm to go>", m);
      } else {
        /* show seconds */
        int s = (int)mins * 60 + (int)secs;
        sprintf(errbuf, "<%3ds to go>", s);
      }
      /* sprintf(errbuf, "<%02d:%02d:%02d to go> ", hours, mins, secs); */
      if (noisy) {
        zfprintf(mesg, errbuf);
        mesg_line_started = 1;
      }
      if (logall) {
        zfprintf(logfile, errbuf);
        logfile_line_started = 1;
      }
    }
# if 0
    else {
      /* first time have no data */
      sprintf(errbuf, "<>");
      if (noisy) {
        fprintf(mesg, "%s ", errbuf);
        mesg_line_started = 1;
      }
      if (logall) {
        fprintf(logfile, "%s ", errbuf);
        logfile_line_started = 1;
      }
    }
# endif

    if (display_zip_rate && elapsed_time_in_usec > 0) {
      /* calculate zipping rate */
      bytes_per_second = (bytes_so_far * 100) / elapsed_time_in_10msec;
      /* if going REALLY slow, assume at least 1 byte per second */
      if (bytes_per_second < 1) {
        bytes_per_second = 1;
      }

      WriteNumString(bytes_per_second, tempstrg);
      sprintf(errbuf, "{%4sB/s}", tempstrg);
      if (noisy) {
        zfprintf(mesg, errbuf);
        mesg_line_started = 1;
      }
      if (logall) {
        zfprintf(logfile, errbuf);
        logfile_line_started = 1;
      }
    }
# if 0
    else {
      /* first time have no data */
      sprintf(errbuf, "{}");
      if (noisy) {
        fprintf(mesg, "%s ", errbuf);
        mesg_line_started = 1;
      }
      if (logall) {
        fprintf(logfile, "%s ", errbuf);
        logfile_line_started = 1;
      }
    }
# endif
  }
#endif /* ENABLE_ENTRY_TIMING */
  if (noisy)
      fflush(mesg);
  if (logall)
      fflush(logfile);

  return 0;
}

local int BlankRunningStats()
{
  if (display_volume) {
    if (noisy) {
      zfprintf(mesg, "%lu>%lu: ", current_in_disk + 1, current_disk + 1);
      mesg_line_started = 1;
    }
    if (logall) {
      zfprintf(logfile, "%lu>%lu: ", current_in_disk + 1, current_disk + 1);
      logfile_line_started = 1;
    }
  }
  if (display_counts) {
    if (noisy) {
      zfprintf(mesg, "   /    ");
      mesg_line_started = 1;
    }
    if (logall) {
      zfprintf(logfile, "   /    ");
      logfile_line_started = 1;
    }
  }
  if (display_bytes) {
    if (noisy) {
      zfprintf(mesg, "     /      ");
      mesg_line_started = 1;
    }
    if (logall) {
      zfprintf(logfile, "     /      ");
      logfile_line_started = 1;
    }
  }
  if (noisy)
      fflush(mesg);
  if (logall)
      fflush(logfile);

  return 0;
}


#ifdef IZ_CRYPT_ANY
# ifndef ZIP_DLL_LIB
int encr_passwd(modeflag, pwbuf, size, zfn)
int modeflag;
char *pwbuf;
int size;
ZCONST char *zfn;
{
    char *prompt;

    /* Tell picky compilers to shut up about unused variables */
    zfn = zfn;

    prompt = (modeflag == ZP_PW_VERIFY) ?
              "Verify password: " : "Enter password: ";

    if (getp(prompt, pwbuf, size) == NULL) {
      ziperr(ZE_PARMS, "stderr is not a tty");
    }
    return IZ_PW_ENTERED;
}

/* This version should be sufficient for Zip.  Zip does not track the
   Zip file name so that parameter is not needed and, in fact, is
   misleading. */
int simple_encr_passwd(modeflag, pwbuf, bufsize)
  int modeflag;
  char *pwbuf;
  size_t bufsize; /* max password length  + 1 (includes NULL) */
{
    char *prompt;

    prompt = (modeflag == ZP_PW_VERIFY) ?
              "Verify password: " : "Enter password: ";

    if (getp(prompt, pwbuf, (int)bufsize) == NULL) {
      ziperr(ZE_PARMS, "stderr is not a tty");
    }
    if (strlen(pwbuf) >= (bufsize - 1)) {
      return -1;
    }
    return 0;
}

# endif /* !ZIP_DLL_LIB */
#else /* def IZ_CRYPT_ANY [else] */
int encr_passwd(modeflag, pwbuf, size, zfn)
int modeflag;
char *pwbuf;
int size;
ZCONST char *zfn;
{
    /* Tell picky compilers to shut up about unused variables */
    modeflag = modeflag; pwbuf = pwbuf; size = size; zfn = zfn;

    return ZE_LOGIC;    /* This function should never be called! */
}

/* This version should be sufficient for Zip. */
/* Version when no encryption is included. */
int simple_encr_passwd(modeflag, pwbuf, bufsize)
  int modeflag;
  char *pwbuf;
  size_t bufsize;
{
    /* Tell picky compilers to shut up about unused variables */
    modeflag = modeflag; pwbuf = pwbuf; bufsize = bufsize;

    return ZE_LOGIC;    /* This function should never be called! */
}

#endif /* def IZ_CRYPT_ANY [else] */


/* int rename_split(temp_name, out_path) - moved to fileio.c */


int set_filetype(out_path)
  char *out_path;
{
#ifdef __BEOS__
  /* Set the filetype of the zipfile to "application/zip" */
  setfiletype( out_path, "application/zip" );
#endif

#ifdef __ATHEOS__
  /* Set the filetype of the zipfile to "application/x-zip" */
  setfiletype(out_path, "application/x-zip");
#endif

#ifdef MACOS
  /* Set the Creator/Type of the zipfile to 'IZip' and 'ZIP ' */
  setfiletype(out_path, 'IZip', 'ZIP ');
#endif

#ifdef RISCOS
  /* Set the filetype of the zipfile to &DDC */
  setfiletype(out_path, 0xDDC);
#endif
  return ZE_OK;
}


/* datetime() - Convert "-t[t]" value string to DOS date/time.
 * Supply current date-time for date to use with time-only values.
 * 2013-12-17 SMS.
 */

/* Modified to acccept the following formats:
 *   ddmmyyyy
 *   ddmmyyyy:HH:MM
 *   ddmmyyyy:HH:MM:SS
 *   yyyy-mm-dd
 *   yyyy-mm-dd:HH:MM
 *   yyyy-mm-dd:HH:MM:SS
 *   :HH:MM                (times alone need leading :)
 *   :HH:MM:SS
 */

#define DT_BAD ((ulg)-1)        /* Bad return value. */

static ulg datetime(arg, curtime)
  ZCONST char *arg;
  ZCONST time_t curtime;
{
  int yr;                               /* Year. */
  int mo;                               /* Month. */
  int dy;                               /* Day (of month). */
  int hr;                               /* Hour. */
  int mn;                               /* Minute. */
  int sc;                               /* Second. */

  ulg dt;                               /* Return value. */
  int itm;                              /* sscan() item count. */
  char *lhp;                            /* Last hyphen. */
  char *fcp;                            /* First colon. */
  char xs[4];                           /* Excess characters. */
  char myarg[20];                       /* Local copy of arg. */

  dt = 0;
  yr = 0;
  mo = 0;
  dy = 0;
  hr = 0;
  mn = 0;
  sc = 0;

  if (strlen(arg) > 19)
  {
    dt = DT_BAD;                /* Longer than "yyyy-mm-dd:HH:MM:SS". */
  }
  else
  {
    strcpy(myarg, arg);                 /* Local copy of argument. */
    fcp = strchr(myarg, ':');           /* First colon. */
    lhp = strrchr(myarg, '-');          /* Last hyphen. */

    if ((fcp != NULL) && (lhp != NULL) && (lhp > fcp))
    {
      dt = DT_BAD;              /* Last hyphen must precede first colon. */
    }
    else if (lhp == NULL)
    {
      /* no hyphens */
      if (fcp == NULL)
      {
        /* No hyphen, no colon.  Look for "mmddyyyy". */
        itm = sscanf(myarg, "%2d%2d%4d%2s", &mo, &dy, &yr, xs);
        if (itm != 3)
        {
          dt = DT_BAD;          /* Excess characters, or not "mmddyyyy". */
        }
      }
      else
      {
        /* colon found, but no hyphens */
        if (fcp > myarg) {
          /* stuff before first colon, assume date in mmddyyyy format */
          *fcp = '\0';  /* NULL terminate date part */
          fcp++;        /* time part is one after that */
          itm = sscanf(myarg, "%2d%2d%4d%2s", &mo, &dy, &yr, xs);
          if (itm != 3)
          {
            dt = DT_BAD;        /* Excess characters, or not "mmddyyyy". */
          }
        }
        else
        {
          /* fcp == myarg, so colon starts myarg, assume time only */
          fcp++;                /* skip leading colon and point to time */
        }
      }
    }
    else
    {
      /* Found a hyphen.  Look for "yyyy-mm-dd[:HH[:MM[:SS]]]". */
      if (fcp != NULL)
      {
        /* Date ends at first colon.  Time begins after. */
        *(fcp++) = '\0';
      }
      itm = sscanf(myarg, "%4d-%2d-%2d%2s", &yr, &mo, &dy, xs);
      if (itm != 3)
      {
        dt = DT_BAD;            /* Excess characters, or not "yyyy-mm-dd". */
      }
    }
  }

  if (dt == 0)
  {
    if (fcp != NULL)
    {
      /* Look for "HH:MM[:SS]". */
      itm = sscanf(fcp, "%2d:%2d:%2d%2s", &hr, &mn, &sc, xs);
      if (itm == 2)
      { /* Not "HH:MM:SS".  Try "HH:MM"? */
        hr = 0;
        mn = 0;
        sc = 0;
        itm = sscanf(fcp, "%2d:%2d%2s", &hr, &mn, xs);
        if (itm != 2)
        {
          dt = DT_BAD;          /* Excess characters, or not "HH:MM". */
        }
      }
      else if (itm != 3)
      {
        dt = DT_BAD;            /* Excess characters, or not "HH:MM:SS". */
      }
    }
  }

  if (dt == 0)
  {
    if ((yr <= 0) || (mo <= 0) || (dy <= 0))
    {
      time_t timet;
      struct tm *ltm;

      timet = curtime;
      ltm = localtime(&timet);
      yr = ltm->tm_year + 1900;
      mo = ltm->tm_mon + 1;
      dy = ltm->tm_mday;
    }
    else if ((yr < 1980) || (yr > 2107) ||      /* Year. */
     (mo < 1) || (mo > 12) ||                   /* Month. */
     (dy < 1) || (dy > 31))                     /* Day (of month). */
    {
      dt = DT_BAD;                              /* Invalid date. */
    }
  }

  if ((dt == 0) &&
   ((hr < 0) || (hr > 23) ||            /* Hour. */
   (mn < 0) || (mn > 59) ||             /* Minute. */
   (sc < 0) || (sc > 59)))              /* Second. */
  {
    dt = DT_BAD;                        /* Invalid time. */
  }

  if (dt == 0)
  {
    dt = dostime(yr, mo, dy, hr, mn, sc);
  }
  return dt;
}



/* --------------------------------------------------------------------- */








#if defined(UNICODE_SUPPORT) && defined(UNICODE_TESTS_OPTION)
/* Unicode_Tests
 *
 * Perform select tests of Unicode conversions and display results.
 */
int Unicode_Tests()
{
  {
    char *instring = "Test - \xcf\x82\xce\xb5\xcf\x81\xcf\x84\xcf\x85\xce\xb8\xce\xb9\xce\xbf\xcf\x80";
    wchar_t *outstring;
    char *restored_string;
    int unicode_wchar = 0;
    int have_iconv = 0;
    int no_nl_langinfo = 0;
    int wchar_size = sizeof(wchar_t);
    int unicode_file_scan = 0;

# ifdef UNICODE_WCHAR
    unicode_wchar = 1;
# endif
# ifdef HAVE_ICONV
    have_iconv = 1;
# endif
# ifdef NO_NL_LANGINFO
    no_nl_langinfo = 1;
# endif
# ifdef UNICODE_FILE_SCAN
    unicode_file_scan = 1;
# endif

    printf("\n");
    printf("localename:   %s\n", localename);
    printf("charsetname:  %s\n", charsetname);
    printf("\n");

    if (unicode_wchar)
      printf("UNICODE_WCHAR defined\n");
    if (have_iconv)
      printf("HAVE_ICONV defined\n");
    if (no_nl_langinfo)
      printf("NO_NL_LANGINFO defined\n");
    if (unicode_file_scan)
      printf("UNICODE_FILE_SCAN defined\n");
    if (using_utf8)
      printf("UTF-8 Native\n");
    printf("sizeof(wchar_t) = %d bytes\n", wchar_size);
    printf("\n");

#ifdef WIN32
    printf("Using Windows API for character set conversions\n");
#else
# ifdef UNIX
    if (unicode_wchar)
      printf("Using wide (wchar_t) calls for character set translations\n");
    else if (have_iconv)
      printf("Using iconv for character set translations\n");
    if (wchar_size == 4)
      printf("Using Zip's internal functions for UTF-8 <--> wchar_t conversions\n");
    else if (have_iconv)
      printf("Using iconv for UTF-8 <--> wchar_t conversions\n");
# endif
#endif
    printf("\n");

    printf("Test utf8_to_wchar_string():\n");

#if defined(WIN32) && !defined(ZIP_DLL_LIB)
    printf("  instring (UTF-8) via print_utf8: '");
    print_utf8(instring);
    printf("'");
#else
    printf("  instring (UTF-8) = '%s'", instring);
#endif
    printf("\n");
    {
      int i;
      printf("  instring bytes:   (");
      for (i = 0; instring[i]; i++) {
        printf(" %02x", (unsigned char)instring[i]);
      }
      printf(")\n");
    }
    printf("\n");
#if 0
    printf(
      "  (Windows requires Lucida Console font and codepage 65001 to see (some) UTF-8.)\n");
    printf("\n");
#endif

    outstring = utf8_to_wchar_string(instring);

    if (outstring == NULL) {
      printf("outstring null\n");

      printf("  did not try to restore string\n");
    }
    else
    {
      int i;

#if defined(WIN32) && !defined(ZIP_DLL_LIB)
      printf("  outstring (wchar_t) via write_consolew:  ");
      write_consolew(stdout, outstring);
#else
      printf("  outstring (wchar_t) = '%S'", outstring);
#endif
      printf("\n");
      printf("\n");
      printf("  outstring codes:  (");
      for (i = 0; outstring[i]; i++) {
        printf(" %04x", outstring[i]);
      }
      printf(")\n");
      printf("\n");

      printf("Test wchar_to_utf8_string():\n");

      restored_string = wchar_to_utf8_string(outstring);

      if (restored_string == NULL)
        printf("  restored string NULL\n");
      else
      {
#if defined(WIN32) && !defined(ZIP_DLL_LIB)
        printf("  restored string (UTF-8) via print_utf8:  ");
        print_utf8(restored_string);
#else
        printf("  restored string (UTF-8) = '%s'", restored_string);
#endif
        printf("\n");
        {
          int i;
          printf("  restored bytes:   (");
          for (i = 0; instring[i]; i++) {
            printf(" %02x", (unsigned char)instring[i]);
          }
          printf(")\n");
        }
      }
    }

    if (restored_string)
      free(restored_string);
    if (outstring)
      free(outstring);

    printf("\n");
  }

  {
    char *utf8_string =
      "Test - \xcf\x82\xce\xb5\xcf\x81\xcf\x84\xcf\x85\xce\xb8\xce\xb9\xce\xbf\xcf\x80";
    char *local_string;
    char *restored_string;

    printf("Test utf8_to_local_string:\n");

#if defined(WIN32) && !defined(ZIP_DLL_LIB)
    printf("  UTF-8 string via print_utf8:  ");
    print_utf8(utf8_string);
    printf("\n");
#else
    printf("  UTF-8 string:  '%s'", utf8_string);
    printf("\n");
#endif
    {
      int i;
      printf("  UTF-8 string bytes:   (");
      for (i = 0; utf8_string[i]; i++) {
        printf(" %02x", (unsigned char)utf8_string[i]);
      }
      printf(")\n");
    }
    printf("\n");

    local_string = utf8_to_local_string(utf8_string);
#if defined(WIN32) && !defined(ZIP_DLL_LIB)
    printf("  local string via write_console:  ");
    write_console(stdout, local_string);
    printf("\n");
#else
    printf("  local string:  '%s'", local_string);
    printf("\n");
#endif
    {
      int i;
      printf("  local string bytes:     (");
      for (i = 0; local_string[i]; i++) {
        printf(" %02x", (unsigned char)local_string[i]);
      }
      printf(")\n");
    }

    printf("\n");
    printf("Test local_to_utf8_string:\n");

    restored_string = local_to_utf8_string(local_string);
#if defined(WIN32) && !defined(ZIP_DLL_LIB)
    printf("  restored string via write_console:  ");
    write_console(stdout, restored_string);
    printf("\n");
#else
    printf("  restored UTF-8 string:  '%s'", restored_string);
    printf("\n");
#endif
    {
      int i;
      printf("  restored UTF-8 bytes:   (");
      for (i = 0; restored_string[i]; i++) {
        printf(" %02x", (unsigned char)restored_string[i]);
      }
      printf(")\n");
    }

    if (local_string)
      free(local_string);
    if (restored_string)
      free(restored_string);

    printf("\n");
  }

  return 0;
}
#endif /* UNICODE_SUPPORT && UNICODE_TESTS_OPTION */

/* --------------------------------------------------------------------- */


/* This function processes the -n (suffixes) option.  The code was getting too
   lengthy for the options switch. */
#ifndef NO_PROTO
void suffixes_option(char *value)
#else
void suffixes_option(value)
  char *value;
#endif
{
  /* Value format: "method[-lvl]=sfx1:sfx2:sfx3...". */

#define MAX_SUF 4000

  int i;
  int j;  /* Place in suffix array of selected compression method. */
  int lvl;
  char *sfx_list;
  int k1;
  int k2;
  int k;
  int jj;
  int kk1;
  int kk2;
  int kk;
  int delta;
  int slen;
  char c;
  char *s;
  char suf[MAX_SUF + 1];
  char suf2[MAX_SUF + 1];
  char *new_list;
  int new_list_size;
  int merged = 0;

  lvl = -1;                   /* Assume the default level. */
  sfx_list = value;

  /* Now "*" in suffix list merges in the existing list.  There is no
      way to remove suffixes from an existing list, except to start
      the list from scratch.  It may make sense to implement the
      chmod approach, using '+' to add a suffix, "-n deflate=+.txt",
      and '-' to delete it. */

  /* Find the first special character in value. */
  for (i = 0; value[i] != '\0'; i++)
  {
    if ((value[i] == '=') || (value[i] == ':') || (value[i] == ';'))
      break;
  }

  j = 0;                      /* Default = STORE. */
  if (value[i] == '=')
  {
    /* Found "method[-lvl]=".  Replace "=" with NUL. */
    value[i] = '\0';

    /* Look for "-n" level specifier.
      *  Must be "--" or "-0" - "-9".
      */
    if ((value[i - 2] == '-') &&
        ((value[i - 1] == '-') ||
          ((value[i - 1] >= '0') && (value[i - 1] <= '9'))))
    {
      if (value[i - 1] != '-')
      {
        /* Some explicit level, 0-9. */
        lvl = value[i - 1] - '0';
      }
      value[i - 2] = '\0';
    }

    /* Check for a match in the method-by-suffix array. */
    for (j = 0; mthd_lvl[j].method >= 0; j++)
    {
      /* Matching only first char - if 2 methods start with same
          char, this needs to be updated. */
      if (abbrevmatch(mthd_lvl[j].method_str, value, CASE_INS, 1))
      {
        /* Matched method name. */
        sfx_list = value + (i + 1);     /* Stuff after "=". */
        break;
      }
    }
    if (mthd_lvl[j].method < 0)
    {
      sprintf(errbuf, "Invalid compression method: %s", value);
      free(value);
      ZIPERR(ZE_PARMS, errbuf);
    }
  } /* = */

  /* Check for a valid "-n" level value, and store it. */
  if (lvl < 0)
  {
    /* Restore the default level setting. */
    mthd_lvl[ j].level_sufx = -1;
  }
  else
  {
    if ((j == 0) && (lvl != 0))
    {
      /* Setting STORE to other than 0 */
      sprintf( errbuf, "Can't set default level for compression method \"%s\" to other than 0: %d",
        mthd_lvl[ j].method_str, lvl);
      free(value);
      ZIPERR(ZE_PARMS, errbuf);
    }
    else if ((j != 0) && (lvl == 0))
    {
      /* Setting something else to 0 */

      sprintf( errbuf, "Can't set default level for compression method \"%s\" to 0",
        mthd_lvl[ j].method_str);
      free(value);
      ZIPERR(ZE_PARMS, errbuf);
    }
    else
    {
      mthd_lvl[j].level_sufx = lvl;
    }
  }

  /* Make freeable copy of suffix list */
  new_list_size = (int)strlen(sfx_list);
  if ((new_list = malloc(new_list_size + 1)) == NULL) {
    ZIPERR(ZE_MEM, "copying suffix list");
  }
  strcpy(new_list, sfx_list);
  free(value);
  sfx_list = new_list;

  /* Parse suffix list to see if * is in there. */
  k1 = 0;
  while ((c = sfx_list[k1])) {
    if (c == ':' || c == ';') {
      k1++;
      continue;
    }
    /* k1 at first char of suffix */
    k2 = k1 + 1;
    while ((c = sfx_list[k2])) {
      if (c == ':' || c == ';') {
        break;
      }
      k2++;
    }
    /* k2 - 1 at end of suffix */
    if (k2 - k1 != 1 || sfx_list[k1] != '*') {
      /* not suffix "*" */
      k1 = k2;
      continue;
    }
    /* found suffix "*" */
    if (merged) {
      /* more than one '*' found */
      ZIPERR(ZE_PARMS, "multiple '*' not allowed in suffix list");
    }
    if (mthd_lvl[j].suffixes) {
      /* replace "*" with current list */
      new_list_size = (int)strlen(sfx_list);
      new_list_size += (int)strlen(mthd_lvl[j].suffixes);
      if ((new_list = malloc(new_list_size + 1)) == NULL) {
        ZIPERR(ZE_MEM, "merging suffix lists");
      }
      for (k = 0; k < k1; k++) {
        new_list[k] = sfx_list[k];
      }
      new_list[k] = '\0';
      strcat(new_list, mthd_lvl[j].suffixes);
      strcat(new_list, sfx_list + k2);
      k2 += (int)strlen(mthd_lvl[j].suffixes);
      k1 = k2;
      merged = 1;
      free(sfx_list);
      sfx_list = new_list;
    } else {
      /* no existing list to merge, just remove "*:" */
      for (k = k1; sfx_list[k + 1] && sfx_list[k + 2]; k++) {
        sfx_list[k] = sfx_list[k + 2];
      }
      sfx_list[k] = '\0';
    }
  }

  /* Set suffix list value. */
  if (*sfx_list == '\0') {   /* Store NULL for an empty list. */
    free(sfx_list);
    sfx_list = NULL;         /* (Worry about white space?) */
  }

  if (mthd_lvl[j].suffixes)
    free(mthd_lvl[j].suffixes);
  mthd_lvl[j].suffixes = sfx_list;

  /* ------------------ */

  /* Remove these suffixes from other methods */

  /* Run through the new suffix list */
  k1 = 0;
  while ((c = sfx_list[k1]))
  {
    if (c == ':' || c == ';') {
      k1++;
      continue;
    }
    /* k1 at first char of suffix */
    k2 = k1 + 1;
    while ((c = sfx_list[k2])) {
      if (c == ':' || c == ';') {
        break;
      }
      k2++;
    }
    /* k2 - 1 at end of suffix */
    delta = k2 - k1 + 1;
    if (delta > MAX_SUF) {
      sprintf(errbuf, "suffix too big (max %d)", MAX_SUF);
      ZIPERR(ZE_PARMS, errbuf);
    }
    for (k = k1; k < k2; k++) {
      suf[k - k1] = sfx_list[k];
    }
    suf[k - k1] = '\0';

    /* See if this suffix is listed for other methods */
    for (jj = 0; mthd_lvl[jj].method >= 0; jj++)
    {
#ifdef SUFDEB
      printf("jj = %d\n", jj);
#endif
      /* Only other global methods */
      if (j != jj && mthd_lvl[jj].suffixes) {
        kk1 = 0;
        s = mthd_lvl[jj].suffixes;

        while ((c = s[kk1])) {
          if (c == ':' || c == ';') {
            kk1++;
            continue;
          }
          /* kk1 at first char of suffix */
          kk2 = kk1 + 1;
          while ((c = s[kk2])) {
            if (c == ':' || c == ';') {
              break;
            }
            kk2++;
          }
          /* kk2 - 1 at end of suffix */
          delta = kk2 - kk1 + 1;
          if (delta > MAX_SUF) {
            sprintf(errbuf, "suffix too big (max %d)", MAX_SUF);
            ZIPERR(ZE_PARMS, errbuf);
          }
          for (kk = kk1; kk < kk2; kk++) {
            suf2[kk - kk1] = s[kk];
          }
          suf2[kk - kk1] = '\0';

#ifdef SUFDEB
          printf("\n  suf: '%s'  suf2: '%s'\n", suf, suf2);
#endif
          if (namecmp(suf, suf2) == 0) {
            /* found it, remove it */
#ifdef SUFDEB
            printf("    before mthd_lvl[%d].suffixes = '%s'\n", jj, mthd_lvl[jj].suffixes);
#endif
            slen = (int)strlen(s);
            for (kk = kk1; kk + delta < slen; kk++) {
              s[kk] = s[kk + delta];
            }
            s[kk] = '\0';
#ifdef SUFDEB
            printf("    after  mthd_lvl[%d].suffixes = '%s'\n", jj, mthd_lvl[jj].suffixes);
#endif
          } else {
            /* not the one, skip it */
            kk1 = kk2;
          }
        } /* while */

        /* Set suffix list value. */
        if (*s == '\0') {   /* Store NULL for an empty list. */
          free(s);
          s = NULL;
        }
      }
    } /* for each method */
    k1 = k2;
  } /* while sfx_list */

  /* ------------------ */

}


/*
  -------------------------------------------------------
  Command Line Options
  -------------------------------------------------------

  Valid command line options.

  The function get_option() uses this table to check if an
  option is valid and if it takes a value (also called an
  option argument).  To add an option to zip just add it
  to this table and add a case in the main switch to handle
  it.  If either shortopt or longopt not used set to "".

   The fields:
       shortopt     - short option name (1 or 2 chars)
       longopt      - long option name
       value_type   - see zip.h for constants
       negatable    - option is negatable with trailing -
       ID           - unsigned long int returned for option
       name         - short description of option which is
                        returned on some errors and when options
                        are listed with -so option, can be NULL
*/

/* Single-letter option IDs are set to the shortopt char (like 'a').  For
   multichar short options set to arbitrary unused constant (like o_aa). */
#define o_aa            0x101
#define o_ad            0x102
#define o_as            0x103
#define o_AC            0x104
#define o_AF            0x105
#define o_AS            0x106
#define o_BC            0x107
#define o_BD            0x108
#define o_BF            0x109
#define o_BL            0x110
#define o_BN            0x111
#define o_BT            0x112
#define o_cd            0x113
#define o_C2            0x114
#define o_C5            0x115
#define o_Cl            0x116
#define o_Cu            0x117
#define o_db            0x118
#define o_dc            0x119
#define o_dd            0x120
#define o_de            0x121
#define o_des           0x122
#define o_df            0x123
#define o_DF            0x124
#define o_DI            0x125
#define o_dg            0x126
#define o_dr            0x127
#define o_ds            0x128
#define o_dt            0x129
#define o_du            0x130
#define o_dv            0x131
#define o_EA            0x132
#define o_FF            0x133
#define o_FI            0x134
#define o_FS            0x135
#define o_h2            0x136
#define o_ic            0x137
#define o_jj            0x138
#define o_kf            0x139
#define o_la            0x140
#define o_lf            0x141
#define o_lF            0x142
#define o_li            0x143
#define o_ll            0x144
#define o_lu            0x145
#define o_mm            0x146
#define o_MM            0x147
#define o_MV            0x148
#define o_nw            0x149
#define o_pa            0x150
#define o_pn            0x151
#define o_pp            0x152
#define o_ps            0x153
#define o_pt            0x154
#define o_pu            0x155
#define o_RE            0x156
#define o_sb            0x157
#define o_sc            0x158
#define o_sC            0x159
#define o_sd            0x160
#define o_sf            0x161
#define o_sF            0x162
#define o_si            0x163
#define o_SI            0x164
#define o_so            0x165
#define o_sp            0x166
#define o_sP            0x167
#define o_ss            0x168
#define o_st            0x169
#define o_su            0x170
#define o_sU            0x171
#define o_sv            0x172
#define o_tn            0x173
#define o_tt            0x174
#define o_TT            0x175
#define o_TU            0x176
#define o_TV            0x177
#define o_UN            0x178
#define o_UT            0x179
#define o_ve            0x180
#define o_vq            0x181
#define o_VV            0x182
#define o_wl            0x183
#define o_ws            0x184
#define o_ww            0x185
#define o_yy            0x186
#define o_z64           0x187
#define o_atat          0x188
#define o_vn            0x189
#define o_et            0x190
#define o_exex          0x191


/* the below is mainly from the old main command line
   switch with a growing number of changes */
/* The preference now is for most non-port-specific options that are not
   enabled to remain visible, but generate an error if the user tries to
   use them.  This should help prevent command lines from being used that
   depend on some option being disabled, and let's the user know that an
   option does exist, but that their version of Zip doesn't support it.
   This needs to be reconciled with -so. */
/* Limit long option names to 18 characters or less to avoid problems showing
   them in -so. */
struct option_struct far options[] = {
  /* short longopt        value_type        negatable        ID    name */
    {"0",  "store",       o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, '0',  "store"},
    {"1",  "compress-1",  o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, '1',  "compress 1 (fastest)"},
    {"2",  "compress-2",  o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, '2',  "compress 2"},
    {"3",  "compress-3",  o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, '3',  "compress 3"},
    {"4",  "compress-4",  o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, '4',  "compress 4"},
    {"5",  "compress-5",  o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, '5',  "compress 5"},
    {"6",  "compress-6",  o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, '6',  "compress 6"},
    {"7",  "compress-7",  o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, '7',  "compress 7"},
    {"8",  "compress-8",  o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, '8',  "compress 8"},
    {"9",  "compress-9",  o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, '9',  "compress 9 (most compression)"},
    {"A",  "adjust-sfx",  o_NO_VALUE,       o_NOT_NEGATABLE, 'A',  "adjust self extractor offsets"},
#ifdef WIN32
    {"AC", "archive-clear", o_NO_VALUE,     o_NOT_NEGATABLE, o_AC, "clear DOS archive bit of included files"},
    {"AS", "archive-set", o_NO_VALUE,       o_NOT_NEGATABLE, o_AS, "include only files with archive bit set"},
#endif
    {"AF", "argfiles",   o_NO_VALUE,       o_NEGATABLE,     o_AF, "enable (default)/disable @argfile"},
#ifdef EBCDIC
    {"a",  "ascii",       o_NO_VALUE,       o_NOT_NEGATABLE, 'a',  "to ASCII"},
    {"aa", "all-ascii",   o_NO_VALUE,       o_NOT_NEGATABLE, o_aa, "all files ASCII text (skip bin check)"},
#endif /* EBCDIC */
#ifdef UNIX_APPLE_SORT
    {"ad", "ad-sort",     o_NO_VALUE,       o_NEGATABLE,     o_ad, "sort AppleDouble ._ after matching file"},
#endif
#ifdef UNIX_APPLE
    {"as", "sequester",   o_NO_VALUE,       o_NEGATABLE,     o_as, "sequester AppleDouble files in __MACOSX"},
#endif
#ifdef CMS_MVS
    {"B",  "binary",      o_NO_VALUE,       o_NOT_NEGATABLE, 'B',  "binary"},
#endif /* CMS_MVS */
#ifdef TANDEM
    {"B",  "",            o_NUMBER_VALUE,   o_NOT_NEGATABLE, 'B',  "nsk"},
#endif
    {"BF", "binary-full-check",o_NO_VALUE,  o_NEGATABLE,      o_BF,"check entire file for binary"},
    /* backup options ---- */
    {"BC", "backup-control",o_REQUIRED_VALUE,o_NOT_NEGATABLE, o_BC,"dir for backup control file"},
    {"BD", "backup-dir",    o_REQUIRED_VALUE,o_NOT_NEGATABLE, o_BD,"dir for backup archive"},
    {"BL", "backup-log",    o_OPT_EQ_VALUE,  o_NOT_NEGATABLE, o_BL,"dir for backup log file"},
    {"BN", "backup-name",   o_REQUIRED_VALUE,o_NOT_NEGATABLE, o_BN,"name of backup"},
    {"BT", "backup-type",   o_REQUIRED_VALUE,o_NOT_NEGATABLE, o_BT,"backup type (FULL, DIFF, or INCR)"},
    /* ------------------- */
    {"b",  "temp-path",   o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'b',  "dir to use for temp archive"},
    {"c",  "entry-comments", o_NO_VALUE,    o_NOT_NEGATABLE, 'c',  "add comments for each entry"},
    {"cd", "current-directory", o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_cd, "set current dir for paths"},
#ifdef VMS
    {"C",  "preserve-case", o_NO_VALUE,     o_NEGATABLE,     'C',  "Preserve (C-: down-) case all on VMS"},
    {"C2", "preserve-case-2", o_NO_VALUE,   o_NEGATABLE,     o_C2, "Preserve (C2-: down-) case ODS2 on VMS"},
    {"C5", "preserve-case-5", o_NO_VALUE,   o_NEGATABLE,     o_C5, "Preserve (C5-: down-) case ODS5 on VMS"},
#endif
#if 0
    {"CP", "code-page",   o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_CP, "set Windows code page"},
#endif
#ifdef ENABLE_PATH_CASE_CONV
    {"Cl", "case-lower",  o_NO_VALUE,      o_NOT_NEGATABLE, o_Cl, "convert added/updated names to lowercase"},
    {"Cu", "case-upper",  o_NO_VALUE,      o_NOT_NEGATABLE, o_Cu, "convert added/udated names to uppercase"},
#endif
    {"d",  "delete",      o_NO_VALUE,       o_NOT_NEGATABLE, 'd',  "delete entries from archive"},
    {"db", "display-bytes", o_NO_VALUE,     o_NEGATABLE,     o_db, "display running bytes"},
    {"dc", "display-counts", o_NO_VALUE,    o_NEGATABLE,     o_dc, "display running file count"},
    {"dd", "display-dots", o_NO_VALUE,      o_NEGATABLE,     o_dd, "display dots as process each file"},
    {"de", "display-est-to-go", o_NO_VALUE, o_NEGATABLE,     o_de, "display estimated time to go"},
    {"dg", "display-globaldots",o_NO_VALUE, o_NEGATABLE,     o_dg, "display dots for archive instead of files"},
    {"dr", "display-rate", o_NO_VALUE,      o_NEGATABLE,     o_dr, "display estimated zip rate in bytes/sec"},
    {"ds", "dot-size",     o_REQUIRED_VALUE,o_NOT_NEGATABLE, o_ds, "set progress dot size - default 10M bytes"},
    {"dt", "display-time", o_NO_VALUE,      o_NOT_NEGATABLE, o_dt, "display time start each entry"},
    {"du", "display-usize", o_NO_VALUE,     o_NEGATABLE,     o_du, "display uncompressed size in bytes"},
    {"dv", "display-volume", o_NO_VALUE,    o_NEGATABLE,     o_dv, "display volume (disk) number"},
#if defined(MACOS) || defined(UNIX_APPLE)
    {"df", "datafork",    o_NO_VALUE,       o_NEGATABLE,     o_df, "save data fork only"},
#endif
    {"D",  "no-dir-entries", o_NO_VALUE,    o_NOT_NEGATABLE, 'D',  "no entries for dirs themselves (-x */)"},
    {"DF", "difference-archive",o_NO_VALUE, o_NOT_NEGATABLE, o_DF, "create diff archive with changed/new files"},
    {"DI", "incremental-list",o_VALUE_LIST, o_NOT_NEGATABLE, o_DI, "archive list to exclude from -DF archive"},
#ifdef IZ_CRYPT_ANY
    {"e",  "encrypt",     o_NO_VALUE,       o_NOT_NEGATABLE, 'e',  "encrypt entries, ask for password"},
#endif
#ifdef ETWODD_SUPPORT
    {"et", "etwodd",      o_NO_VALUE,       o_NOT_NEGATABLE, o_et, "encrypt Traditional without data descriptor"},
#endif
#ifdef OS2
    {"E",  "longnames",   o_NO_VALUE,       o_NOT_NEGATABLE, 'E',  "use OS2 longnames"},
#endif
    {"EA", "extended-attributes",o_REQUIRED_VALUE,o_NOT_NEGATABLE,o_EA,"control storage of extended attributes"},
    {"F",  "fix",         o_NO_VALUE,       o_NOT_NEGATABLE, 'F',  "fix mostly intact archive (try first)"},
    {"FF", "fixfix",      o_NO_VALUE,       o_NOT_NEGATABLE, o_FF, "try harder to fix archive (not as reliable)"},
    {"FI", "fifo",        o_NO_VALUE,       o_NEGATABLE,     o_FI, "read Unix FIFO (zip will wait on open pipe)"},
    {"FS", "filesync",    o_NO_VALUE,       o_NOT_NEGATABLE, o_FS, "add/delete entries to make archive match OS"},
    {"f",  "freshen",     o_NO_VALUE,       o_NOT_NEGATABLE, 'f',  "freshen existing archive entries"},
    {"fd", "force-descriptors", o_NO_VALUE, o_NOT_NEGATABLE, o_des,"force data descriptors as if streaming"},
#ifdef ZIP64_SUPPORT
    {"fz", "force-zip64", o_NO_VALUE,       o_NEGATABLE,     o_z64,"force use of Zip64 format, negate prevents"},
#endif
    {"g",  "grow",        o_NO_VALUE,       o_NOT_NEGATABLE, 'g',  "grow existing archive instead of replace"},
    {"h",  "help",        o_NO_VALUE,       o_NOT_NEGATABLE, 'h',  "help"},
    {"H",  "",            o_NO_VALUE,       o_NOT_NEGATABLE, 'h',  "help"},
    {"?",  "",            o_NO_VALUE,       o_NOT_NEGATABLE, 'h',  "help"},
    {"h2", "more-help",   o_NO_VALUE,       o_NOT_NEGATABLE, o_h2, "extended help"},
    {"hh", "",            o_NO_VALUE,       o_NOT_NEGATABLE, o_h2, "extended help"},
    {"HH", "",            o_NO_VALUE,       o_NOT_NEGATABLE, o_h2, "extended help"},
    {"i",  "include",     o_VALUE_LIST,     o_NOT_NEGATABLE, 'i',  "include only files matching patterns"},
#if defined(VMS) || defined(WIN32)
    {"ic", "ignore-case", o_NO_VALUE,       o_NEGATABLE,     o_ic, "ignore case when matching archive entries"},
#endif
#ifdef RISCOS
    {"I",  "no-image",    o_NO_VALUE,       o_NOT_NEGATABLE, 'I',  "no image"},
#endif
    {"j",  "junk-paths",  o_NO_VALUE,       o_NEGATABLE,     'j',  "strip paths and just store file names"},
#ifdef MACOS
    {"jj", "absolute-path", o_NO_VALUE,     o_NOT_NEGATABLE, o_jj, "MAC absolute path"},
#endif /* ?MACOS */
    {"J",  "junk-sfx",    o_NO_VALUE,       o_NOT_NEGATABLE, 'J',  "strip self extractor from archive"},
    {"k",  "DOS-names",   o_NO_VALUE,       o_NOT_NEGATABLE, 'k',  "force use of 8.3 DOS names"},
    {"kf", "keyfile",     o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_kf, "read (end part of) password from keyfile"},
    {"l",  "to-crlf",     o_NO_VALUE,       o_NOT_NEGATABLE, 'l',  "convert text file line ends - LF->CRLF"},
    {"ll", "from-crlf",   o_NO_VALUE,       o_NOT_NEGATABLE, o_ll, "convert text file line ends - CRLF->LF"},
    {"lf", "logfile-path",o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_lf, "log to log file at path (default overwrite)"},
    {"lF", "log-output",  o_NO_VALUE,       o_NEGATABLE,     o_lF, "log to OUTNAME.log (default overwrite)"},
    {"la", "log-append",  o_NO_VALUE,       o_NEGATABLE,     o_la, "append to existing log file"},
    {"li", "log-info",    o_NO_VALUE,       o_NEGATABLE,     o_li, "include informational messages in log"},
    {"lu", "log-utf8",    o_NO_VALUE,       o_NEGATABLE,     o_lu, "log names as UTF-8"},
    {"L",  "license",     o_NO_VALUE,       o_NOT_NEGATABLE, 'L',  "display license"},
    {"m",  "move",        o_NO_VALUE,       o_NOT_NEGATABLE, 'm',  "add files to archive then delete files"},
    {"mm", "",            o_NO_VALUE,       o_NOT_NEGATABLE, o_mm, "not used"},
    {"MM", "must-match",  o_NO_VALUE,       o_NOT_NEGATABLE, o_MM, "error if infile not matched/not readable"},
#ifdef CMS_MVS
    {"MV", "MVS",         o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_MV, "MVS path translate (dots, slashes, lastdot)"},
#endif /* CMS_MVS */
    {"n",  "suffixes",    o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'n',  "suffixes to not compress: .gz:.zip"},
    {"nw", "no-wild",     o_NO_VALUE,       o_NOT_NEGATABLE, o_nw, "no wildcards during add or update"},
#if defined(AMIGA) || defined(MACOS)
    {"N",  "notes",       o_NO_VALUE,       o_NOT_NEGATABLE, 'N',  "add notes as entry comments"},
#endif
    {"o",  "latest-time", o_NO_VALUE,       o_NOT_NEGATABLE, 'o',  "use latest entry time as archive time"},
    {"O",  "output-file", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'O',  "set out zipfile different than in zipfile"},
    {"p",  "paths",       o_NO_VALUE,       o_NEGATABLE,     'p',  "store paths"},
    {"pa", "prefix-add-path",o_REQUIRED_VALUE,o_NOT_NEGATABLE,o_pa,"add prefix to added/updated paths"},
    {"pn", "non-ansi-password", o_NO_VALUE, o_NEGATABLE,     o_pn, "allow non-ANSI password"},
    {"ps", "allow-short-pass", o_NO_VALUE, o_NEGATABLE,  o_ps, "allow short password"},
    {"pp", "prefix-path", o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_pp, "add prefix to all paths in archive"},
    {"pt", "performance-time", o_NO_VALUE,  o_NEGATABLE,     o_pt, "time execution of zip"},
    {"pu", "pswd-to-unzip", o_NO_VALUE,     o_NEGATABLE,     o_pu, "pass password to unzip for test"},
#ifdef IZ_CRYPT_ANY
    {"P",  "password",    o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'P',  "encrypt entries, option value is password"},
#endif /* def IZ_CRYPT_ANY */
#if defined(QDOS) || defined(QLZIP)
    {"Q",  "Q-flag",      o_NUMBER_VALUE,   o_NOT_NEGATABLE, 'Q',  "Q flag"},
#endif
    {"q",  "quiet",       o_NO_VALUE,       o_NOT_NEGATABLE, 'q',  "quiet"},
    {"r",  "recurse-paths", o_NO_VALUE,     o_NOT_NEGATABLE, 'r',  "recurse down listed paths"},
    {"R",  "recurse-patterns", o_NO_VALUE,  o_NOT_NEGATABLE, 'R',  "recurse current dir and match patterns"},
    {"RE", "regex",       o_NO_VALUE,       o_NOT_NEGATABLE, o_RE, "allow [list] matching (regex)"},
    {"s",  "split-size",  o_REQUIRED_VALUE, o_NOT_NEGATABLE, 's',  "do splits, set split size (-s=0 no splits)"},
    {"sp", "split-pause", o_NO_VALUE,       o_NOT_NEGATABLE, o_sp, "pause while splitting to select destination"},
    {"sv", "split-verbose", o_NO_VALUE,     o_NOT_NEGATABLE, o_sv, "be verbose about creating splits"},
    {"sb", "split-bell",  o_NO_VALUE,       o_NOT_NEGATABLE, o_sb, "when pause for next split ring bell"},
    {"sc", "show-command",o_OPT_EQ_VALUE,   o_NOT_NEGATABLE, o_sc, "show command line"},
#if 0
    {"sP", "show-parsed-command",o_NO_VALUE,o_NOT_NEGATABLE, o_sP, "show command line as parsed"},
#endif
#ifdef UNICODE_EXTRACT_TEST
    {"sC", "create-files",o_NO_VALUE,       o_NOT_NEGATABLE, o_sC, "create empty files using archive names"},
#endif
    {"sd", "show-debug",  o_NO_VALUE,       o_NOT_NEGATABLE, o_sd, "show debug"},
    {"sf", "show-files",  o_NO_VALUE,       o_NEGATABLE,     o_sf, "show files to operate on and exit"},
    {"sF", "sf-params",   o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_sF, "add info to -sf listing"},
#if !defined( VMS) && defined( ENABLE_USER_PROGRESS)
    {"si", "show-pid",    o_NO_VALUE,       o_NEGATABLE,     o_si, "show process ID"},
#endif /* !defined( VMS) && defined( ENABLE_USER_PROGRESS) */
    {"so", "show-options",o_NO_VALUE,       o_NOT_NEGATABLE, o_so, "show options"},
    {"ss", "show-suffixes",o_NO_VALUE,      o_NOT_NEGATABLE, o_ss, "show method-level suffix lists"},
    {"st", "stream",      o_NO_VALUE,       o_NOT_NEGATABLE, o_st, "include local attr/comment for stream extract"},
    {"su", "show-unicode", o_NO_VALUE,      o_NEGATABLE,     o_su, "as -sf but also show escaped Unicode"},
    {"sU", "show-just-unicode", o_NO_VALUE, o_NEGATABLE,     o_sU, "as -sf but only show escaped Unicode"},
#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(ATARI)
    {"S",  "",            o_NO_VALUE,       o_NOT_NEGATABLE, 'S',  "include system and hidden"},
#endif /* MSDOS || OS2 || WIN32 || ATARI */
    {"SI", "rename-stdin",o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_SI, "rename stdin from \"-\" to this"},
    {"t",  "from-date",   o_REQUIRED_VALUE, o_NOT_NEGATABLE, 't',  "exclude before date"},
    {"tt", "before-date", o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_tt, "include before date"},
#ifdef USE_EF_UT_TIME
    {"tn", "no-universal-time",o_NO_VALUE,  o_NOT_NEGATABLE, o_tn, "do not store universal time for file/entry"},
#endif
    {"T",  "test",        o_NO_VALUE,       o_NOT_NEGATABLE, 'T',  "test archive before committing updates"},
    {"TT", "unzip-command", o_REQUIRED_VALUE,o_NOT_NEGATABLE,o_TT, "unzip command string to use (see help)"},
    {"",   "test-command", o_REQUIRED_VALUE,o_NOT_NEGATABLE, o_TT, "unzip command string to use (see help)"},
    {"TU", "unzip-path",  o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_TU, "path to unzip to use for testing archive"},
    {"TV", "unzip-verbose",o_NO_VALUE,      o_NEGATABLE,     o_TV, "when test with -T, tell unzip to be verbose"},
    {"u",  "update",      o_NO_VALUE,       o_NOT_NEGATABLE, 'u',  "update existing entries and add new"},
    {"U",  "copy-entries", o_NO_VALUE,      o_NOT_NEGATABLE, 'U',  "select from archive instead of file system"},
    {"UN", "unicode",     o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_UN, "UN=quit/warn/ignore/no, esc, loc/utf8, show"},
#ifdef UNICODE_TESTS_OPTION
    {"UT", "utest",       o_NO_VALUE,       o_NOT_NEGATABLE, o_UT, "Do some Unicode tests"},
#endif
    {"v",  "verbose",     o_NO_VALUE,       o_NOT_NEGATABLE, 'v',  "display additional information"},
    {"vq", "quick-version", o_NO_VALUE,     o_NOT_NEGATABLE, o_vq, "display quick version"},
    {"",   "version",     o_NO_VALUE,       o_NOT_NEGATABLE, o_ve, "(if no other args) show version information"},
#ifdef VMS
    {"V",  "VMS-portable", o_NO_VALUE,      o_NOT_NEGATABLE, 'V',  "store VMS attributes, portable file format"},
    {"VV", "VMS-specific", o_NO_VALUE,      o_NOT_NEGATABLE, o_VV, "store VMS attributes, VMS specific format"},
    {"vn", "vms-names",    o_NO_VALUE,      o_NEGATABLE,     o_vn, "preserve idiosyncratic VMS file names"},
    {"w",  "VMS-versions", o_NO_VALUE,      o_NOT_NEGATABLE, 'w',  "store VMS versions"},
    {"ww", "VMS-dot-versions", o_NO_VALUE,  o_NOT_NEGATABLE, o_ww, "store VMS versions as \".nnn\""},
#endif /* VMS */
#ifdef WINDOWS_LONG_PATHS
    {"wl", "windows-long-paths", o_NO_VALUE, o_NEGATABLE,    o_wl,  "include windows long paths (see help -hh)"},
#endif
    {"ws", "wild-stop-dirs", o_NO_VALUE,    o_NOT_NEGATABLE, o_ws,  "* stops at /, ** includes any /"},
    {"x",  "exclude",     o_VALUE_LIST,     o_NOT_NEGATABLE, 'x',  "exclude files matching patterns"},
/*    {"X",  "no-extra",    o_NO_VALUE,       o_NOT_NEGATABLE, 'X',  "no extra"},
*/
    {"X",  "strip-extra", o_NO_VALUE,       o_NEGATABLE,     'X',  "-X- keep all ef, -X strip but critical ef"},
#ifdef SYMLINKS
    {"y",  "symlinks",    o_NO_VALUE,       o_NOT_NEGATABLE, 'y',  "store symbolic links"},
# ifdef WIN32
    {"yy", "no-mount-points", o_NO_VALUE,   o_NEGATABLE,     o_yy, "-yy=don't follow mount points, -yy-=follow all"},
# endif
#endif /* SYMLINKS */
#ifdef IZ_CRYPT_ANY
    {"Y", "encryption-method", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'Y', "set encryption method"},
#endif /* def IZ_CRYPT_ANY */
    {"z",  "archive-comment", o_NO_VALUE,   o_NOT_NEGATABLE, 'z',  "ask for archive comment"},
    {"Z",  "compression-method", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'Z', "compression method"},
#if defined(MSDOS) || defined(OS2) || defined( VMS)
    {"$",  "volume-label", o_NO_VALUE,      o_NOT_NEGATABLE, '$',  "store volume label"},
#endif
#if !defined(MACOS) && !defined(WINDLL)
    {"@",  "names-stdin", o_NO_VALUE,       o_NOT_NEGATABLE, '@',  "get file names from stdin, one per line"},
#endif /* !MACOS */
    {"@@", "names-file",  o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_atat,"get file names from file, one per line"},
#ifdef NTSD_EAS
    {"!",  "use-privileges", o_NO_VALUE,    o_NOT_NEGATABLE, '!',  "use privileges"},
    {"!!", "no-security",    o_NO_VALUE,    o_NOT_NEGATABLE, o_exex,"leave out security info (ACLs)"},
#endif
#ifdef RISCOS
    {"/",  "exts-to-swap", o_REQUIRED_VALUE, o_NOT_NEGATABLE,'/',  "override Zip$Exts"},
#endif
    /* the end of the list */
    {NULL, NULL,          o_NO_VALUE,       o_NOT_NEGATABLE, 0,    NULL} /* end has option_ID = 0 */
  };



#ifndef USE_ZIPMAIN
int main(argc, argv)
#else
int zipmain(argc, argv)
#endif
int argc;               /* number of tokens in command line */
char **argv;            /* command line tokens */
/* Add, update, freshen, or delete zip entries in a zip file.  See the
   command help in help() above. */
{
  int d;                /* true if just adding to a zip file */
  char *e;              /* malloc'd comment buffer */
  struct flist far *f;  /* steps through found linked list */
  int i;                /* arg counter, root directory flag */
  int kk;               /* next arg type (formerly another re-use of "k") */

  /* zip64 support 09/05/2003 R.Nausedat */
  uzoff_t c;            /* start of central directory */
  uzoff_t t;            /* length of central directory */
  zoff_t k;             /* marked counter, entry count */
  uzoff_t n;            /* total of entry len's */

  int o;                /* true if there were any ZE_OPEN errors */
#if 0
#if !defined(ZIPLIB) && !defined(ZIPDLL)
  char *p;              /* steps through option arguments */
#endif
#endif
  char *pp;             /* temporary pointer */
  int r;                /* temporary variable */
  int s;                /* flag to read names from stdin */
  uzoff_t csize;        /* compressed file size for stats */
  uzoff_t usize;        /* uncompressed file size for stats */
  ulg tf;               /* file time */
  int first_listarg = 0;/* index of first arg of "process these files" list */
  struct zlist far *v;  /* temporary variable */
  struct zlist far * far *w;    /* pointer to last link in zfiles list */
  FILE *x /*, *y */;    /* input and output zip files (y global) */
  struct zlist far *z;  /* steps through zfiles linked list */
  int bad_open_is_error = 0; /* if read fails, 0=warning, 1=error */
#ifdef ZIP_DLL_LIB
  int retcode;          /* return code for dll */
#endif /* ZIP_DLL_LIB */
#if (!defined(VMS) && !defined(CMS_MVS))
  char *zipbuf;         /* stdio buffer for the zip file */
#endif /* !VMS && !CMS_MVS */
  int all_current;      /* used by File Sync to determine if all entries are current */

  struct filelist_struct *filearg;

/* used by get_option */
  unsigned long option; /* option ID returned by get_option */
  int argcnt = 0;       /* current argcnt in args */
  int argnum = 0;       /* arg number */
  int optchar = 0;      /* option state */
  char *value = NULL;   /* non-option arg, option value or NULL */
  int negated = 0;      /* 1 = option negated */
  int fna = 0;          /* current first non-opt arg */
  int optnum = 0;       /* index in table */
  time_t cur_time_opt;  /* Current date-time to get date when -t[t] have time only */

  int show_options = 0; /* show options */
  int show_suffixes = 0;/* Show method-level suffix lists. */
  int show_args = 0;    /* show command line */
  int show_parsed_args = 0; /* show parsed command line */
  int seen_doubledash = 0; /* seen -- argument */
  int key_needed = 0;   /* prompt for encryption key */
  int have_out = 0;     /* if set, in_path and out_path different archives */

  int sort_found_list = 0; /* sort the found list (set below) */

#ifdef UNICODE_EXTRACT_TEST
  int create_files = 0;
#endif
  int names_from_file = 0; /* names being read from file */

#ifdef SHOW_PARSED_COMMAND
  int parsed_arg_count;
  char **parsed_args;
  int max_parsed_args;
#endif /* def SHOW_PARSED_COMMAND */



#ifdef THEOS
  /* the argument expansion from the standard library is full of bugs */
  /* use mine instead */
  _setargv(&argc, &argv);
#endif

#ifdef ENABLE_ENTRY_TIMING
  start_time = get_time_in_usec();
#endif




/* ===================================================================== */

  /* Initializing and checking locale now done in set_locale() in fileio.c.
     All comments also moved there.  This allows set_locale() to also be
     used by the utilities. */
  set_locale();

  /* Keeping copy of this comment here for historical reasons. */
  
  /* For Unix, we either need to be working in some UTF-8 environment or we
     need help elsewise to see all file system paths available to us,
     otherwise paths not supported in the current character set won't be seen
     and can't be archived.  If UTF-8 is native, the full Unicode paths
     should be viewable in a terminal window.

     As of Zip 3.1, we no longer set the locale to UTF-8.  If the native
     locale is UTF-8, we proceed with UTF-8 as native.  Otherwise we
     use the local character set as is.  (An exception is Windows, where
     we always use the wide functions if available, and so native UTF-8
     support is not needed.  UTF-8 is derived as needed from the wide
     versions of the paths.)

     If we detect we are already in some UTF-8 environment, then we can
     proceed.  If not, we can attempt (in some cases) to change to UTF-8 if
     supported.  (For most ports, the actual UTF-8 encoding probably does not
     matter, and setting the locale to en_US.UTF-8 may be sufficient.  For
     others, it does matter and some special handling is needed.)  If neither
     work and we can't establish some UTF-8 environment, we should give up on
     Unicode, as the level of support is not clear and may not be sufficient.
     One of the main reasons for supporting Unicode is so we can find and
     archive the various non-current-char-set paths out there as they come
     up in a directory scan.  We now distinguish this case (UNICODE_FILE_SCAN).
     We also use UTF-8 for writing paths in the archive and for display of
     the real paths on the console (if supported).  To some degree, the
     Unicode escapes (#Uxxxx and #Lxxxxxx) can be used to bypass that use
     of UTF-8.

     For Windows, the Unicode tasks are handled using the OS wide character
     calls, as there is no direct UTF-8 support.  So directory scans are done
     using wide character strings and then converted to UTF-8 for storage in
     the archive.  This falls into the "need help elsewise" category and adds
     considerable Windows-unique code, but it seems the only way if full
     native Unicode support is to be had.  (iconv won't work, as there are
     many abnormalities in how Unicode is handled in Windows that are handled
     by the native OS calls, but would need significant kluging if we tried to
     do all that.  For instance, the handling of surrogates.  Best to leave
     converting Windows wide strings to UTF-8 to Windows.)  has_win32_wide()
     is used to determine if the Windows port supports wide characters.
     
     Note that paths displayed in a Windows command prompt window will likely
     be escaped.  If a Unicode supporting font is loaded (like Lucida Console)
     and the code page is set to UTF-8 (chcp 65001), then most Western
     characters should be visible, but languages like Japanese probably will
     not display correctly (showing small boxes instead of the right characters).

     As of Zip 3.1, Windows console output uses print_utf8() (via zprintf()),
     which calls write_consolew(), which does not need a UTF-8 console to output
     Unicode.  This still displays boxes for Japanese and similar fonts, though,
     as that is a font support issue of Windows consoles.

     For the IBM ports (z/OS and related), this gets more complex than the Unix
     case as those ports are EBCDIC based.  While one can work with UTF-8 via
     iconv, one can not actually run in an ASCII-based locale via setlocale()
     in the z/OS Unix environment.  (Some?) IBM ports do provide a wide DBCS
     (double byte character set) environment that seems to mirror somewhat
     Windows wide functionality, but this is reported to be insufficient.  IBM
     support is still rough and untested.
     
     AIX will support the UTF-8 locale, but it is an optional feature, so one
     must do a test to see if it is present.  Some specific testing is needed
     and is being worked on.

     In some cases, character set conversions can be done using iconv, but
     iconv only does character set conversions.  The wide functions provide
     many additional capabilities (like case conversion of wide characters)
     that iconv does not.  When iconv is used, alternatives need to be
     found for the missing capabilities, or features using those capabilities
     disabled for the port using iconv.

     See the large note in tailor.h for more on Unicode support.

     A new option -UT (--utest) performs select Unicode tests and displays
     the results, but it currently needs updating.

     We plan to update the locale checks in set_locale() shortly.
  */

/* ===================================================================== */




#if defined(__IBMC__) && defined(__DEBUG_ALLOC__)
  {
    extern void DebugMalloc(void);
    atexit(DebugMalloc);
  }
#endif

#ifdef QDOS
  {
    extern void QDOSexit(void);
    atexit(QDOSexit);
  }
#endif

#ifdef NLM
  {
    extern void NLMexit(void);
    atexit(NLMexit);
  }
#endif

#ifdef RISCOS
  set_prefix();
#endif

#ifdef __human68k__
  fflush(stderr);
  setbuf(stderr, NULL);
#endif

#ifdef VMS
  /* This pointless reference to a do-nothing function ensures that the
   * globals get linked in, even on old systems, or when compiled using
   * /NAMES = AS_IS.  (See also globals.c.)
   */
  {
    void (*local_dummy)( void);
    local_dummy = globals_dummy;
  }
#endif /* def VMS */


/* Re-initialize global variables to make the zip dll re-entrant. It is
 * possible that we could get away with not re-initializing all of these
 * but better safe than sorry.
 */
#if defined(MACOS) || defined(WINDLL) || defined(USE_ZIPMAIN)
  action = ADD; /* one of ADD, UPDATE, FRESHEN, DELETE, or ARCHIVE */
  comadd = 0;   /* 1=add comments for new files */
  zipedit = 0;  /* 1=edit zip comment and all file comments */
  latest = 0;   /* 1=set zip file time to time of latest file */
  before = 0;   /* 0=ignore, else exclude files before this time */
  after = 0;    /* 0=ignore, else exclude files newer than this time */
  test = 0;     /* 1=test zip file with unzip -t */
  unzip_string = NULL; /* command string to test archive with */
  unzip_path = NULL; /* where to look for unzip */
  unzip_verbose = 0;  /* use "unzip -t" instead of "unzip -tqq" */
  tempdir = 0;  /* 1=use temp directory (-b) */
  junk_sfx = 0; /* 1=junk the sfx prefix */
# if defined(AMIGA) || defined(MACOS)
  filenotes = 0;/* 1=take comments from AmigaDOS/MACOS filenotes */
# endif
# ifndef USE_ZIPMAIN
  zipstate = -1;
# endif
  tempzip = NULL;
  fcount = 0;
  recurse = 0;         /* 1=recurse into directories; 2=match filenames */
  dispose = 0;         /* 1=remove files after put in zip file */
  pathput = 1;         /* 1=store path with name */

# ifdef UNIX_APPLE
  data_fork_only = 0;
  sequester = 0;
# endif
# ifdef RISCOS
  int scanimage = 0;   /* Scan through image files */
# endif

  method = BEST;        /* one of BEST, DEFLATE (only), or STORE (only) */
  dosify = 0;           /* 1=make new entries look like MSDOS */
  verbose = 0;          /* 1=report oddities in zip file structure */
  fix = 0;              /* 1=fix the zip file */
  filesync = 0;         /* 1=file sync, delete entries not on file system */
  adjust = 0;           /* 1=adjust offsets for sfx'd file (keep preamble) */
  level = 6;            /* 0=fastest compression, 9=best compression */
# ifdef ETWODD_SUPPORT
  etwodd = 0;           /* Encrypt Trad without data descriptor. */
# endif
  translate_eol = 0;    /* Translate end-of-line LF -> CR LF */
# ifdef VMS
  prsrv_vms = 0;        /* Preserve idiosyncratic VMS file names. */
  vmsver = 0;           /* Append VMS version number to file names. */
  vms_native = 0;       /* Store in VMS format */
  vms_case_2 = 0;       /* ODS2 file name case in VMS. -1: down. */
  vms_case_5 = 0;       /* ODS5 file name case in VMS. +1: preserve. */
  argv_cli = NULL;      /* New argv[] storage to free, if non-NULL. */
# endif /* VMS */
# if defined(OS2) || defined(WIN32)
  use_longname_ea = 0;  /* 1=use the .LONGNAME EA as the file's name */
# endif
# if defined (QDOS) || defined(QLZIP)
  qlflag = 0;
# endif
# ifdef NTSD_EAS
  use_privileges = 0;     /* 1=use security privileges overrides */
# endif
  no_wild = 0;            /* 1 = wildcards are disabled */
# ifdef WILD_STOP_AT_DIR
   wild_stop_at_dir = 1;  /* default wildcards do not include / in matches */
# else
   wild_stop_at_dir = 0;  /* default wildcards do include / in matches */
# endif

  for (i = 0; mthd_lvl[i].method >= 0; i++)
  { /* Restore initial compression level-method states. */
    mthd_lvl[i].level = -1;       /* By-method level. */
    mthd_lvl[i].level_sufx = -1;  /* By-suffix level. */
    mthd_lvl[i].suffixes = NULL;  /* Suffix list. */
  }
  mthd_lvl[0].suffixes = MTHD_SUFX_0;  /* STORE default suffix list. */

  skip_this_disk = 0;
  des_good = 0;           /* Good data descriptor found */
  des_crc = 0;            /* Data descriptor CRC */
  des_csize = 0;          /* Data descriptor csize */
  des_usize = 0;          /* Data descriptor usize */

  dot_size = 0;           /* buffers processed in deflate per dot, 0 = no dots */
  dot_count = 0;          /* buffers seen, recyles at dot_size */

  display_counts = 0;     /* display running file count */
  display_bytes = 0;      /* display running bytes remaining */
  display_globaldots = 0; /* display dots for archive instead of each file */
  display_volume = 0;     /* display current input and output volume (disk) numbers */
  display_usize = 0;      /* display uncompressed bytes */
  display_est_to_go = 0;  /* display estimated time to go */
  display_time = 0;       /* display time start each entry */
  display_zip_rate = 0;   /* display bytes per second rate */

  files_so_far = 0;       /* files processed so far */
  bad_files_so_far = 0;   /* bad files skipped so far */
  files_total = 0;        /* files total to process */
  bytes_so_far = 0;       /* bytes processed so far (from initial scan) */
  good_bytes_so_far = 0;  /* good bytes read so far */
  bad_bytes_so_far = 0;   /* bad bytes skipped so far */
  bytes_total = 0;        /* total bytes to process (from initial scan) */

  logall = 0;             /* 0 = warnings/errors, 1 = all */
  logfile = NULL;         /* pointer to open logfile or NULL */
  logfile_append = 0;     /* append to existing logfile */
  logfile_path = NULL;    /* pointer to path of logfile */

  use_outpath_for_log = 0; /* 1 = use output archive path for log */
  log_utf8 = 0;           /* log names as UTF-8 */
#ifdef WIN32
  nonlocal_name = 0;      /* Name has non-local characters */
  nonlocal_path = 0;      /* Path has non-local characters */
#endif

  startup_dir = NULL;     /* dir that Zip starts in (current dir ".") */
  working_dir = NULL;     /* dir user asked to change to for zipping */

  hidden_files = 0;       /* process hidden and system files */
  volume_label = 0;       /* add volume label */
  label = NULL;           /* volume label */
  dirnames = 1;           /* include directory entries by default */
# if defined(WIN32)
  only_archive_set = 0;   /* only include if DOS archive bit set */
  clear_archive_bits = 0; /* clear DOS archive bit of included files */
# endif
  linkput = 0;            /* 1=store symbolic links as such */
  noisy = 1;              /* 0=quiet operation */
  extra_fields = 1;       /* 0=create minimum, 1=don't copy old, 2=keep old */

  use_descriptors = 0;    /* 1=use data descriptors 12/29/04 */
  zip_to_stdout = 0;      /* output zipfile to stdout 12/30/04 */
  allow_empty_archive = 0;/* if no files, create empty archive anyway 12/28/05 */
  copy_only = 0;          /* 1=copying archive entries only */

  include_stream_ef = 0;  /* 1=include stream ef that allows full stream extraction */

  output_seekable = 1;    /* 1 = output seekable 3/13/05 EG */

# ifdef ZIP64_SUPPORT     /* zip64 support 10/4/03 */
  force_zip64 = -1;       /* if 1 force entries to be zip64 */
                          /* mainly for streaming from stdin */
  zip64_entry = 0;        /* current entry needs Zip64 */
  zip64_archive = 0;      /* if 1 then at least 1 entry needs zip64 */
# endif

# ifdef UNICODE_SUPPORT
  utf8_native = 1;        /* 1=force storing UTF-8 as standard per AppNote bit 11 */
# endif

  unicode_escape_all = 0; /* 1=escape all non-ASCII characters in paths */
  unicode_mismatch = 1;   /* unicode mismatch is 0=error, 1=warn, 2=ignore, 3=no */

  scan_delay = 5;         /* seconds before display Scanning files message */
  scan_dot_time = 2;      /* time in seconds between Scanning files dots */
  scan_start = 0;         /* start of scan */
  scan_last = 0;          /* time of last message */
  scan_started = 0;       /* scan has started */
  scan_count = 0;         /* Used for Scanning files ... message */

  before = 0;             /* 0=ignore, else exclude files before this time */
  after = 0;              /* 0=ignore, else exclude files newer than this time */

  key = NULL;             /* Scramble password if scrambling */
  passwd = NULL;          /* Password before keyfile content added */
  key_needed = 0;         /* Need scramble password */
#ifdef IZ_CRYPT_AES_WG
  key_size = 0;
#endif
  force_ansi_key = 1;     /* Only ANSI characters for password (32 - 126) */

  path_prefix = NULL;     /* Prefix to add to all new archive entries */
  path_prefix_mode = 0;   /* 0=Prefix all paths, 1=Prefix only added/updated paths */
  stdin_name = NULL;      /* Name to change default stdin "-" to */
  tempath = NULL;         /* Path for temporary files */
  patterns = NULL;        /* List of patterns to be matched */
  pcount = 0;             /* number of patterns */
  icount = 0;             /* number of include only patterns */
  Rcount = 0;             /* number of -R include patterns */

#ifdef ENABLE_USER_PROGRESS
  u_p_phase = 0;
  u_p_task = NULL;
  u_p_name = NULL;
#endif

  found = NULL;           /* List of names found, or new found entry */
  fnxt = &found;

  /* used by get_option */
  argcnt = 0;             /* size of args */
  argnum = 0;             /* current arg number */
  optchar = 0;            /* option state */
  value = NULL;           /* non-option arg, option value or NULL */
  negated = 0;            /* 1 = option negated */
  fna = 0;                /* current first nonopt arg */
  optnum = 0;             /* option index */

  show_options = 0;       /* 1 = show options */
  show_what_doing = 0;    /* 1 = show what zip doing */
  show_args = 0;          /* 1 = show command line */
  show_parsed_args = 0;   /* 1 = show parsed command line */
  seen_doubledash = 0;    /* seen -- argument */

  args = NULL;            /* copy of argv that can be freed by free_args() */

  all_ascii = 0;          /* skip binary check and handle all files as text */
  binary_full_check = 0;

  zipfile = NULL;         /* path of usual in and out zipfile */
  tempzip = NULL;         /* name of temp file */
  y = NULL;               /* output file now global so can change in splits */
  in_file = NULL;         /* current input file for splits */
  in_split_path = NULL;   /* current in split path */
  in_path = NULL;         /* used by splits to track changing split locations */
  out_path = NULL;        /* if set, use -O out_path as output */
  have_out = 0;           /* if set, in_path and out_path not the same archive */
  zip_attributes = 0;

  total_disks = 0;        /* total disks in archive */
  current_in_disk = 0;    /* current read split disk */
  current_in_offset = 0;  /* current offset in current read disk */
  skip_current_disk = 0;  /* if != 0 and fix then skip entries on this disk */

  zip64_eocd_disk = 0;    /* disk with Zip64 End Of Central Directory Record */
  zip64_eocd_offset = 0;  /* offset for Zip64 EOCD Record */

  current_local_disk = 0; /* disk with current local header */

  current_disk = 0;           /* current disk number */
  cd_start_disk = (ulg)-1;    /* central directory start disk */
  cd_start_offset = 0;        /* offset of start of cd on cd start disk */
  cd_entries_this_disk = 0;   /* cd entries this disk */
  total_cd_entries = 0;       /* total cd entries in new/updated archive */

  /* for split method 1 (keep split with local header open and update) */
  current_local_tempname = NULL; /* name of temp file */
  current_local_file = NULL;  /* file pointer for current local header */
  current_local_offset = 0;   /* offset to start of current local header */

  /* global */
  bytes_this_split = 0;       /* bytes written to the current split */
  read_split_archive = 0;     /* 1=scanzipf_reg detected spanning signature */
  split_method = 0;           /* 0=no splits, 1=update LHs, 2=data descriptors */
  split_size = 0;             /* how big each split should be */
  split_bell = 0;             /* when pause for next split ring bell */
  bytes_prev_splits = 0;      /* total bytes written to all splits before this */
  bytes_this_entry = 0;       /* bytes written for this entry across all splits */
  noisy_splits = 0;           /* be verbose about creating splits */
  mesg_line_started = 0;      /* 1=started writing a line to mesg */
  logfile_line_started = 0;   /* 1=started writing a line to logfile */

#ifdef WINDOWS_LONG_PATHS
  include_windows_long_paths = 0;
  archive_has_long_path = 0;
#endif

  filelist = NULL;            /* list of input files */
  filearg_count = 0;

  apath_list = NULL;          /* list of incremental archive paths */
  apath_count = 0;

  allow_empty_archive = 0;    /* if no files, allow creation of empty archive anyway */
  bad_open_is_error = 0;      /* if read fails, 0=warning, 1=error */
  unicode_mismatch = 0;       /* unicode mismatch is 0=error, 1=warn, 2=ignore, 3=no */
  show_files = 0;             /* show files to operate on and exit */

  sf_usize = 0;               /* include usize in -sf listing */

  mvs_mode = 0;               /* 0=lastdot (default), 1=dots, 2=slashes */

  scan_delay = 5;             /* seconds before display Scanning files message */
  scan_dot_time = 2;          /* time in seconds between Scanning files dots */
  scan_started = 0;           /* space at start of scan has been displayed */
  scan_last = 0;              /* Time last dot displayed for Scanning files message */
  scan_start = 0;             /* Time scanning started for Scanning files message */
# ifdef UNICODE_SUPPORT
  use_wide_to_mb_default = 0;
# endif
  filter_match_case = 1;      /* default is to match case when matching archive entries */
  diff_mode = 0;              /* 1=diff mode - only store changed and add */

  allow_fifo = 0;             /* 1=allow reading Unix FIFOs, waiting if pipe open */
# ifdef ENABLE_ENTRY_TIMING
  start_zip_time = 0;         /* after scan, when start zipping files */
# endif
  names_from_file = 0;

  case_upper_lower = CASE_PRESERVE;

#ifdef ENABLE_ENTRY_TIMING
  performance_time = 0;
#endif

#ifdef UNICODE_SUPPORT
  unicode_show = 0;
#endif

  encryption_method = NO_ENCRYPTION;

  allow_arg_files = 1;

# ifdef ZIP_DLL_LIB
  /* set up error return jump */
  retcode = setjmp(zipdll_error_return);
  if (retcode) {
    return retcode;
  }
  /* verify NULL termination of (user-supplied) argv[] */
  if (argv[ argc] != NULL)
  {
    ZIPERR( ZE_PARMS, "argv[argc] != NULL");
  }
# endif /* def ZIP_DLL_LIB */
# ifdef BACKUP_SUPPORT
  backup_dir = NULL;            /* dir to save backup archives (and control) */
  backup_name = NULL;           /* name to use for archive, log, and control */
  backup_control_dir = NULL;    /* control file dir (overrides backup_dir) */
  backup_log_dir = NULL;        /* backup log dir (defaults to backup_dir) */
  backup_type = 0;              /* default to not using backup mode */
  backup_start_datetime = NULL; /* date/time stamp of start of backup */
  backup_control_dir = NULL;    /* dir to put control file */
  backup_control_path = NULL;   /* control file used to store backup set */
  backup_full_path = NULL;      /* full archive of backup set */
  backup_output_path = NULL;    /* path of output archive before finalizing */
# endif
#endif /* MACOS || WINDLL || USE_ZIPMAIN */

/* Standardize use of -RE to enable special [] use (was just MSDOS and WIN32) */
/*#if !defined(ALLOW_REGEX) && (defined(MSDOS) || defined(WIN32)) */
#if !defined(ALLOW_REGEX)
  allow_regex = 0;        /* 1 = allow [list] matching (regex) */
#else
  allow_regex = 1;
#endif

  mesg = (FILE *) stdout; /* cannot be made at link time for VMS */
  comment_stream = (FILE *)stdin;

  init_upper();           /* build case map table */

#ifdef LARGE_FILE_SUPPORT
  /* test if we can support large files - 9/29/04 */
  if (sizeof(zoff_t) < 8) {
    ZIPERR(ZE_COMPILE, "LARGE_FILE_SUPPORT enabled but OS not supporting it");
  }
#endif
  /* test if sizes are the same - 12/30/04 */
  if (sizeof(uzoff_t) != sizeof(zoff_t)){
    sprintf(errbuf, "uzoff_t size (%d bytes) not same as zoff_t (%d bytes)",
             (int)sizeof(uzoff_t), (int)sizeof(zoff_t));
    ZIPERR(ZE_COMPILE, errbuf);
  }


#if defined( IZ_CRYPT_AES_WG) || defined( IZ_CRYPT_AES_WG_NEW)
  /* Verify the AES_WG compile-time endian decision. */
  {
    union {
      unsigned int i;
      unsigned char b[ 4];
    } bi;

# ifndef PLATFORM_BYTE_ORDER
#  define ENDI_BYTE 0x00
#  define ENDI_PROB "(Undefined)"
# else
#  if PLATFORM_BYTE_ORDER == AES_LITTLE_ENDIAN
#   define ENDI_BYTE 0x78
#   define ENDI_PROB "Little"
#  else
#   if PLATFORM_BYTE_ORDER == AES_BIG_ENDIAN
#    define ENDI_BYTE 0x12
#    define ENDI_PROB "Big"
#   else
#    define ENDI_BYTE 0xff
#    define ENDI_PROB "(Unknown)"
#   endif
#  endif
# endif

    bi.i = 0x12345678;
    if (bi.b[ 0] != ENDI_BYTE) {
      sprintf( errbuf, "Bad AES_WG compile-time endian: %s", ENDI_PROB);
      ZIPERR( ZE_COMPILE, errbuf);
    }
  }
#endif /* defined( IZ_CRYPT_AES_WG) || defined( IZ_CRYPT_AES_WG_NEW) */


#if (defined(WIN32) && defined(USE_EF_UT_TIME))
  /* For the Win32 environment, we may have to "prepare" the environment
     prior to the tzset() call, to work around tzset() implementation bugs.
   */
  iz_w32_prepareTZenv();
#endif

#if (defined(IZ_CHECK_TZ) && defined(USE_EF_UT_TIME))
#  ifndef VALID_TIMEZONE
#     define VALID_TIMEZONE(tmp) \
             (((tmp = getenv("TZ")) != NULL) && (*tmp != '\0'))
#  endif
  zp_tz_is_valid = VALID_TIMEZONE(p);
#if (defined(AMIGA) || defined(DOS))
  if (!zp_tz_is_valid)
    extra_fields = 0;     /* disable storing "UT" time stamps */
#endif /* AMIGA || DOS */
#endif /* IZ_CHECK_TZ && USE_EF_UT_TIME */

/* For systems that do not have tzset() but supply this function using another
   name (_tzset() or something similar), an appropriate "#define tzset ..."
   should be added to the system specifc configuration section.  */
#if (!defined(TOPS20) && !defined(VMS))
#if (!defined(RISCOS) && !defined(MACOS) && !defined(QDOS))
#if (!defined(BSD) && !defined(MTS) && !defined(CMS_MVS) && !defined(TANDEM))
  tzset();
#endif
#endif
#endif

#ifdef VMSCLI
  {
    unsigned int status;
    char **argv_old;

    argv_old = argv;    /* Save the original argv for later comparison. */
    status = vms_zip_cmdline( &argc, &argv);

    /* Record whether vms_zip_cmdline() created a new argv[]. */
    if (argv_old == argv)
      argv_cli = NULL;
    else
      argv_cli = argv;

    if (!(status & 1))
      return status;
  }
#endif /* VMSCLI */

  /*    Substitutes the extended command line argument list produced by
   *    the MKS Korn Shell in place of the command line info from DOS.
   */

  /* extract extended argument list from environment */
  expand_args(&argc, &argv);

  /* Process arguments */
  diag("processing arguments");
  /* First, check if just the help or version screen should be displayed */
  /* Now support help listing when called from DLL */
  if (argc == 1
# ifndef WINDLL
      && isatty(1)
# endif
      )   /* no arguments, and output screen available */
  {                             /* show help screen */
# ifdef VMSCLI
    VMSCLI_help();
# else
    help();
# endif
#ifdef WINDLL
    return ZE_OK;
#else
    EXIT(ZE_OK);
#endif
  }
  /* Check -v here as env arg can change argc.  Handle --version in main switch. */
  else if (argc == 2 && strcmp(argv[1], "-v") == 0 &&
           /* only "-v" as argument, and */
           (isatty(1) || isatty(0)))
           /* stdout or stdin is connected to console device */
  {                             /* show diagnostic version info */
    version_info();
#ifdef WINDLL
    return ZE_OK;
#else
    EXIT(ZE_OK);
#endif
  }
#ifndef ZIP_DLL_LIB
# ifndef VMS
#   ifndef RISCOS
  envargs(&argc, &argv, "ZIPOPT", "ZIP");  /* get options from environment */
#   else /* RISCOS */
  envargs(&argc, &argv, "ZIPOPT", "Zip$Options");  /* get options from environment */
  getRISCOSexts("Zip$Exts");        /* get the extensions to swap from environment */
#   endif /* ? RISCOS */
# else /* VMS */
  envargs(&argc, &argv, "ZIPOPT", "ZIP_OPTS");  /* 4th arg for unzip compat. */
# endif /* ?VMS */
#endif /* !ZIP_DLL_LIB */

  zipfile = tempzip = NULL;
  y = NULL;
  d = 0;                        /* disallow adding to a zip file */

#ifndef NO_EXCEPT_SIGNALS
# if (!defined(MACOS) && !defined(ZIP_DLL_LIB) && !defined(NLM))
  signal(SIGINT, handler);
#  ifdef SIGTERM                  /* AMIGADOS and others have no SIGTERM */
  signal(SIGTERM, handler);
#  endif
#  if defined(SIGABRT) && !(defined(AMIGA) && defined(__SASC))
   signal(SIGABRT, handler);
#  endif
#  ifdef SIGBREAK
   signal(SIGBREAK, handler);
#  endif
#  ifdef SIGBUS
   signal(SIGBUS, handler);
#  endif
#  ifdef SIGILL
   signal(SIGILL, handler);
#  endif
#  ifdef SIGSEGV
   signal(SIGSEGV, handler);
#  endif
# endif /* !MACOS && !ZIP_DLL_LIB && !NLM */
# ifdef NLM
  NLMsignals();
# endif
#endif /* ndef NO_EXCEPT_SIGNALS */


#ifdef ENABLE_USER_PROGRESS
# ifdef VMS
  establish_ctrl_t( user_progress);
# else /* def VMS */
#  ifdef SIGUSR1
  signal( SIGUSR1, user_progress);
#  endif /* def SIGUSR1 */
# endif /* def VMS [else] */
#endif /* def ENABLE_USER_PROGRESS */


#ifdef UNICODE_SUPPORT_WIN32
  /* check if this Win32 OS has support for wide character calls */
  has_win32_wide();
#endif

  /* Set default STORE list. */
  {
    int new_list_size;
    char *new_list;

    new_list_size = (int)strlen(MTHD_SUFX_0);
    if ((new_list = malloc(new_list_size + 1)) == NULL) {
      ZIPERR(ZE_MEM, "setting STORE suffix list");
    }
    strcpy(new_list, MTHD_SUFX_0);
    mthd_lvl[0].suffixes = new_list;
  }

  /* Get current (local) date for possible use with time-only "-t[t]"
   * option values.
   */
  cur_time_opt = time(NULL);

#ifdef SHOW_PARSED_COMMAND
  /* make copy of command line as it is parsed */
  parsed_arg_count = 0;
  parsed_args = NULL;
  max_parsed_args = 0;    /* space allocated on first arg assignment */
#endif

#if 0
  {
    int winver = WINVER;
    long major, minor, build;

    printf("winver %4x\n", winver);

    GetWinVersion(&major, &minor, &build);
    printf("Windows version:  %d.%d.%d\n", major, minor, build);
  }
#endif

#ifdef WIN32_WIDE_CMD_LINE

  /* Read the Windows Unicode wide command line */
  {

    /* set up args for envargs() */
    char **env_argv;
    int env_argc;
    int i;
    int j;
    char **utf8_argv = NULL;
    int utf8_argc;

#if 0
    printf("{1}\n");
#endif
    /* Create argv with just our name. */
    if ((env_argv = (char **)malloc(2 * sizeof(char *))) == NULL) {
      ZIPERR(ZE_MEM, "env_argv");
    }

    env_argv[0] = argv[0];
    env_argv[1] = NULL;
    env_argc = 1;
    
    /* Get options from environment. */
    envargs(&env_argc, &env_argv, "ZIPOPT", "ZIP");

    /* Get Win32 wide command line converted to UTF-8. */
    utf8_argv = get_win32_utf8_argv();
    utf8_argc = arg_count(utf8_argv);
    
#if 0
    printf("{2}\n");
#endif
    /* Get combined arg count (argcnt + any env args). */
    argcnt = (env_argc - 1) + utf8_argc;

    /* Allocate space for new args[] and args_utf8[] flags. */
    if ((args = malloc((argcnt + 1) * sizeof(char *))) == NULL) {
      ZIPERR(ZE_MEM, "wide args");
    }

#if 0
    printf("{3}\n");
#endif
    /* Get our name from wide argv and flag as UTF-8. */
    args[0] = utf8_argv[0];

    /* Copy env args.
     * envargs() may have just passed through the original argv or
     * may have parsed a single string into args.  As we don't know,
     * need to just copy the contents into free'able args.
     *
     * envargs() needs to be cleaned up to allow reliable free'ing.
     */
    for (i = 1; env_argv[i]; i++) {
      args[i] = string_dup(env_argv[i], "environment args", 0);
    }

    /* Pass through rest of wide command line UTF-8 args */
    for (j = 1; utf8_argv[j]; j++) {
      args[i + j - 1] = utf8_argv[j];
    }

    /* NULL terminate */
    args[i + j - 1] = NULL;

    /* Flag that command line is from UTF-8. */
    win32_utf8_argv = 1;
#if 0
    // _setmode(_fileno(stdout), _O_U16TEXT);
#endif

#if 0
    printf("----------------\n");
    for (i = 0; args[i]; i++) {
      wchar_t *ws = utf8_to_wchar_string(args[i]);
      wprintf(L"%2d: '%s'    '", i, ws);
      //printf("  wcw2a: '");
      write_consolew2a(ws);
      printf("'\n");
    }
    printf("----------------\n");
#endif
  }

#else

  /* Make copy of args that can use with insert_arg() used by get_option(). */
  args = copy_args(argv, 0);
  argcnt = arg_count(args);

#endif


#if defined(UNICODE_SUPPORT_WIN32) && defined(WIN32_WIDE_CMD_LINE)
  /* If on Windows getting input as wide, show Unicode directly
     (as well as a Windows console window can display it). */
  unicode_show = 1;
#endif


#ifdef ZIPDLL
  /* The DLL requires all structure elements to be specific sizes, in
     this case file size MUST be 64-bit to ensure the caller knows what
     size it will be.  There is no size checking of
     structure elements when the DLL is called.  For the LIB there is
     size checking at compile time so we can be more accomodating of
     ports without 64-bit variables (like VMS).  As long as a LIB user
     uses API_FILESIZE_T as set for that port, it should all work, in
     theory.  Currently Unix and Windows uses unsigned long long for
     API_FILESIZE_T.  VAX VMS uses unsigned long (32-bit), while
     other VMS uses unsigned long long (64-bit). */
  if (sizeof(API_FILESIZE_T) != 8) {

    /* Could use this here. */
    /* BUILD_BUG_ON(sizeof(API_FILESIZE_T) != 8); */

    ZIPERR(ZE_COMPILE, "DLL compiled without 64-bit file support");
  }
#endif


  kk = 0;                       /* Next non-option argument type */
  s = 0;                        /* set by -@ */

  /*
  -------------------------------------------
  Process command line using get_option
  -------------------------------------------

  Each call to get_option() returns either a command
  line option and possible value or a non-option argument.
  Arguments are permuted so that all options (-r, -b temp)
  are returned before non-option arguments (zipfile).
  Returns 0 when nothing left to read.
  */

  /* set argnum = 0 on first call to init get_option */
  argnum = 0;

  /* get_option returns the option ID and updates parameters:
         args    - usually copy of argv unless argument file adds arguments
         argcnt  - current argc for args
         value   - char* to value (free() when done with it) or NULL if no value
         negated - option was negated with trailing -

     Any argfile get_option() may read may add args.
  */

  while ((option = get_option(&args, &argcnt, &argnum,
                              &optchar, &value, &negated,
                              &fna, &optnum, 0)))
  {
    /* Limit returned value or non-option to MAX_OPTION_VALUE_SIZE. */
    if (value && (MAX_OPTION_VALUE_SIZE) &&
        strlen(value) > (MAX_OPTION_VALUE_SIZE)) {
      sprintf(errbuf, "command line argument larger than %d - truncated: ",
              MAX_OPTION_VALUE_SIZE); 
      zipwarn(errbuf, value);
      value[MAX_OPTION_VALUE_SIZE] = '\0';
    }

#ifdef SHOW_PARSED_COMMAND
    {
      /* save this parsed argument */

      int use_short = 1;
      char *opt;

      /* this arg may parse to option and value (creating 2 parsed args),
         or to non-opt arg (creating 1 parsed arg) */
      if (parsed_arg_count > max_parsed_args - 2) {
        /* increase space used to store parsed arguments */
        max_parsed_args += 256;
        if ((parsed_args = (char **)realloc(parsed_args,
                (max_parsed_args + 1) * sizeof(char *))) == NULL) {
          ZIPERR(ZE_MEM, "realloc parsed command line");
        }
      }

      if (option != o_NON_OPTION_ARG) {
        /* assume option and possible value */

        /* default to using short option string */
        opt = options[optnum].shortopt;
        if (strlen(opt) == 0) {
          /* if no short, use long option string */
          use_short = 0;
          opt = options[optnum].longopt;
        }
        if ((parsed_args[parsed_arg_count] =
                      (char *)malloc(strlen(opt) + 3)) == NULL) {
          ZIPERR(ZE_MEM, "parse command line (1)");
        }
        if (use_short) {
          strcpy(parsed_args[parsed_arg_count], "-");
        } else {
          strcpy(parsed_args[parsed_arg_count], "--");
        }
        strcat(parsed_args[parsed_arg_count], opt);

        if (value) {
          parsed_arg_count++;

          if ((parsed_args[parsed_arg_count] =
                     (char *)malloc(strlen(value) + 1)) == NULL) {
            ZIPERR(ZE_MEM, "parse command line (2)");
          }
          strcpy(parsed_args[parsed_arg_count], value);
        }
      } else {
        /* non-option arg */

        if ((parsed_args[parsed_arg_count] =
                   (char *)malloc(strlen(value) + 1)) == NULL) {
          ZIPERR(ZE_MEM, "parse command line (3)");
        }
        strcpy(parsed_args[parsed_arg_count], value);
      }
      parsed_arg_count++;


      parsed_args[parsed_arg_count] = NULL;
    }
#endif


    switch (option)
    {
        case '0':
          method = STORE; level = 0;
          break;
        case '1':  case '2':  case '3':  case '4':
        case '5':  case '6':  case '7':  case '8':  case '9':
          {
            /* Could be a simple number (-3) or include a value
               (-3=def:bz) */
            char *dp1;
            char *dp2;
            char t;
            int j;
            int lvl;

            /* Calculate the integer compression level value. */
            lvl = (int)option - '0';

            /* Analyze any option value (method names). */
            if (value == NULL)
            {
              /* No value.  Set the global compression level. */
              level = lvl;
            }
            else
            {
              /* Set the by-method compression level for the specified
               * method names in "value" ("mthd1:mthd2:...:mthdn").
               */
              dp1 = value;
              while (*dp1 != '\0')
              {
                /* Advance dp2 to the next delimiter.  t = delimiter. */
                for (dp2 = dp1;
                 (((t = *dp2) != ':') && (*dp2 != ';') && (*dp2 != '\0'));
                 dp2++);

                /* NUL-terminate the latest list segment. */
                *dp2 = '\0';

                /* Check for a match in the method-by-suffix array. */
                for (j = 0; mthd_lvl[ j].method >= 0; j++)
                {
                  if (abbrevmatch( mthd_lvl[ j].method_str, dp1, CASE_INS, 1))
                  {
                    /* Matched method name.  Set the by-method level. */
                    mthd_lvl[ j].level = lvl;
                    break;

                  }
                }

                if (mthd_lvl[ j].method < 0)
                {
                  sprintf( errbuf, "Invalid compression method: \"%s\"", dp1);
                  free( value);
                  ZIPERR( ZE_PARMS, errbuf);
                }

                /* Quit at end-of-string. */
                if (t == '\0') break;

                /* Otherwise, advance dp1 to the next list segment. */
                dp1 = dp2+ 1;
              }
            }
          }
          break;

#ifdef EBCDIC
        case 'a':
          aflag = FT_ASCII_TXT;
          zipmessage("Translating text files to ASCII...", "");
          binary_full_check = 1;
          break;
        case o_aa:
          aflag = FT_ASCII_TXT;
          all_ascii = 1;
          zipmessage("Skipping binary check, translating all files as text to ASCII...", "");
          break;
#endif /* EBCDIC */
#ifdef CMS_MVS
        case 'B':
          bflag = 1;
          zipmessage("Using binary mode...", "");
          break;
#endif /* CMS_MVS */
#ifdef TANDEM
        case 'B':
          nskformatopt(value);
          free(value);
          break;
#endif
        case 'A':   /* Adjust unzipsfx'd zipfile:  adjust offsets only */
          adjust = 1; break;
#if defined(WIN32)
        case o_AC:
          clear_archive_bits = 1; break;
        case o_AS:
          /* Since some directories could be empty if no archive bits are
             set for files in a directory, don't add directory entries (-D).
             Just files with the archive bit set are added, including paths
             (unless paths are excluded).  All major unzips should create
             directories from the paths as needed. */
          dirnames = 0;
          only_archive_set = 1; break;
#endif /* defined(WIN32) */
        case o_AF:
          /* Turn off and on processing of @argfiles.  Default is on. */
          if (negated)
            allow_arg_files = 0;
          else
            allow_arg_files = 1;
#ifdef UNIX_APPLE_SORT
        case o_ad:
          /* Enable sorting "._" AppleDouble files after the
             file they go with.  On UNIX_APPLE this is on by
             default for AppleDouble files Zip creates.  This
             option enables sorting of existing file system
             "._" files.  These should not exist on Unix Apple
             systems (Zip creates them after reading the meta
             data).  This option is more for other platforms
             where an archive from a Unix Apple system is
             extracted on another platform, including "._"
             files, and Zip is now being used to again
             archive those files, including "._" files,
             for use on a Unix Apple system. */
          if (negated) {
            sort_apple_double = 0;
            sort_apple_double_all = 0;
          }
          else {
            sort_apple_double = 1;
            sort_apple_double_all = 1;
          }
          break;
#endif
#ifdef UNIX_APPLE
        case o_as:
          /* sequester Apple Double resource files */
          if (negated)
            sequester = 0;
          else
            sequester = 1;
          break;
#endif /* UNIX_APPLE */

        case 'b':   /* Specify path for temporary file */
          tempdir = 1;
          tempath = value;
          break;

        case o_BF:  /* Perform full binary check */
          if (negated)
            binary_full_check = 0;
          else
            binary_full_check = 1;
          break;

        case o_BC:  /* Dir for BACKUP control file */
#ifdef BACKUP_SUPPORT
          if (backup_control_dir)
            free(backup_control_dir);
          backup_control_dir = value;
          break;
#else
          ZIPERR(ZE_PARMS, "backup support disabled: no -BC");
#endif
        case o_BD:  /* Dir for BACKUP archive (and control file) */
#ifdef BACKUP_SUPPORT
          if (backup_dir)
            free(backup_dir);
          backup_dir = value;
          break;
#else
          ZIPERR(ZE_PARMS, "backup support disabled: no -BD");
#endif
        case o_BL:  /* dir for BACKUP log */
#ifdef BACKUP_SUPPORT
          if (backup_log_dir)
            free(backup_log_dir);
          if (value == NULL)
          {
            if ((backup_log_dir = malloc(1)) == NULL) {
              ZIPERR(ZE_MEM, "backup_log_dir");
            }
            strcpy(backup_log_dir, "");
          } else {
            backup_log_dir = value;
          }
          break;
#else
          ZIPERR(ZE_PARMS, "backup support disabled: no -BL");
#endif
        case o_BN:  /* Name of BACKUP archive and control file */
#ifdef BACKUP_SUPPORT
          if (backup_name)
            free(backup_name);
          backup_name = value;
          break;
#else
          ZIPERR(ZE_PARMS, "backup support disabled: no -BN");
#endif
        case o_BT:  /* Type of BACKUP archive */
#ifdef BACKUP_SUPPORT
          if (abbrevmatch("none", value, CASE_INS, 1)) {
            /* Revert to normal operation, no backup control file */
            backup_type = BACKUP_NONE;
          } else if (abbrevmatch("full", value, CASE_INS, 1)) {
            /* Perform full backup (normal archive), init control file */
            backup_type = BACKUP_FULL;
          } else if (abbrevmatch("differential", value, CASE_INS, 1)) {
            /* Perform differential (all files different from base archive) */
            backup_type = BACKUP_DIFF;
            diff_mode = 1;
            allow_empty_archive = 1;
          } else if (abbrevmatch("incremental", value, CASE_INS, 1)) {
            /* Perform incremental (all different from base and other incr) */
            backup_type = BACKUP_INCR;
            diff_mode = 1;
            allow_empty_archive = 1;
          } else {
            zipwarn("-BT:  Unknown backup type: ", value);
            free(value);
            ZIPERR(ZE_PARMS, "-BT:  backup type must be FULL, DIFF, INCR, or NONE");
          }
          free(value);
          break;
#else
          ZIPERR(ZE_PARMS, "backup support disabled: no -BT");
#endif
        case 'c':   /* Add comments for new files in zip file */
          comadd = 1;  break;

        case o_cd:  /* Change default directory */
#ifdef CHANGE_DIRECTORY
          if (working_dir)
            free(working_dir);
          working_dir = value;
          break;
#else
          ZIPERR(ZE_PARMS, "-cd not enabled");
#endif

        /* -C, -C2, and -C5 are with -V */

#ifdef ENABLE_PATH_CASE_CONV
        case o_Cl:   /* Convert added/updated paths to lowercase */
          case_upper_lower = CASE_LOWER;
          break;
        case o_Cu:   /* Convert added/updated paths to uppercase */
          case_upper_lower = CASE_UPPER;
          break;
#endif

        case 'd':   /* Delete files from zip file */
          if (action != ADD) {
            ZIPERR(ZE_PARMS, "specify just one action");
          }
          action = DELETE;
          break;
#ifdef MACOS
        case o_df:
          MacZip.DataForkOnly = true;
          break;
#endif /* def MACOS */
#ifdef UNIX_APPLE
        case o_df:
          if (negated)
            data_fork_only = 0;
          else
            data_fork_only = 1;
          break;
#endif /* UNIX_APPLE */
        case o_db:
          if (negated)
            display_bytes = 0;
          else
            display_bytes = 1;
          break;
        case o_dc:
          if (negated)
            display_counts = 0;
          else
            display_counts = 1;
          break;
        case o_dd:
          /* display dots */
          display_globaldots = 0;
          if (negated) {
            dot_count = 0;
          } else {
            /* set default dot size if dot_size not set (dot_count = 0) */
            if (dot_count == 0)
              /* default to 10 MiB */
              dot_size = 10 * 0x100000;
            dot_count = -1;
          }
          break;
        case o_de:
#ifdef ENABLE_ENTRY_TIMING
          if (negated)
            display_est_to_go = 0;
          else
            display_est_to_go = 1;
#else
          ZIPERR(ZE_PARMS, "Entry timing (-de) not enabled/supported");
#endif
          break;
        case o_dg:
          /* display dots globally for archive instead of for each file */
          if (negated) {
            display_globaldots = 0;
          } else {
            display_globaldots = 1;
            /* set default dot size if dot_size not set (dot_count = 0) */
            if (dot_count == 0)
              dot_size = 10 * 0x100000;
            dot_count = -1;
          }
          break;
        case o_dr:
#ifdef ENABLE_ENTRY_TIMING
          if (negated)
            display_zip_rate = 0;
          else
            display_zip_rate = 1;
#else
          ZIPERR(ZE_PARMS, "Entry timing (-dr) not enabled/supported");
#endif
          break;
        case o_ds:
          /* input dot_size is now actual dot size to account for
             different buffer sizes */
          if (value == NULL)
            dot_size = 10 * 0x100000;
          else if (value[0] == '\0') {
            /* default to 10 MiB */
            dot_size = 10 * 0x100000;
            free(value);
          } else {
            dot_size = ReadNumString(value);
            if (dot_size == (zoff_t)-1) {
              sprintf(errbuf, "option -ds (--dot-size) has bad size:  '%s'",
                      value);
              free(value);
              ZIPERR(ZE_PARMS, errbuf);
            }
            if (dot_size < 0x400) {
              /* < 1 KB so there is no multiplier, assume MiB */
              dot_size *= 0x100000;

            } else if (dot_size < 0x400L * 32) {
              /* 1K <= dot_size < 32K */
              sprintf(errbuf, "dot size must be at least 32 KB:  '%s'", value);
              free(value);
              ZIPERR(ZE_PARMS, errbuf);

            } else {
              /* 32K <= dot_size */
            }
            free(value);
          }
          dot_count = -1;
          break;
        case o_dt:
          if (negated)
            display_time = 0;
          else
            display_time = 1;
          break;
        case o_du:
          if (negated)
            display_usize = 0;
          else
            display_usize = 1;
          break;
        case o_dv:
          if (negated)
            display_volume = 0;
          else
            display_volume = 1;
          break;
        case 'D':   /* Do not add directory entries */
          dirnames = 0; break;
        case o_DF:  /* Create a difference archive */
          diff_mode = 1;
          allow_empty_archive = 1;
          break;
        case o_DI:  /* Create an incremental archive */
          ZIPERR(ZE_PARMS, "-DI not yet implemented");

          /* implies diff mode */
          diff_mode = 2;

          /* add archive path to list */
          add_apath(value);

          break;
        case 'e':   /* Encrypt */
#ifdef IZ_CRYPT_ANY
          if (key == NULL)
            key_needed = 1;
          if (encryption_method == NO_ENCRYPTION) {
# ifdef IZ_CRYPT_TRAD
            encryption_method = TRADITIONAL_ENCRYPTION;
# else
            encryption_method = AES_128_ENCRYPTION;
# endif
          }
#else /* def IZ_CRYPT_ANY */
          ZIPERR(ZE_PARMS, "encryption (-e) not supported");
#endif /* def IZ_CRYPT_ANY [else] */
          break;
        case o_EA:
          ZIPERR(ZE_PARMS, "-EA (extended attributes) not yet implemented");
#ifdef ETWODD_SUPPORT
        case o_et:      /* Encrypt Traditional without data descriptor. */
          etwodd = 1;
          break;
#endif
        case 'F':   /* fix the zip file */
#if defined(ZIPLIB) || defined(ZIPDLL)
          ZIPERR(ZE_PARMS, "-F not yet supported for LIB or DLL");
#else
          fix = 1;
#endif
          break;
        case o_FF:  /* try harder to fix file */
#if defined(ZIPLIB) || defined(ZIPDLL)
          ZIPERR(ZE_PARMS, "-FF not yet supported for LIB or DLL");
#else
          fix = 2;
#endif
          break;
        case o_FI:
#ifdef FIFO_SUPPORT
          if (negated)
            allow_fifo = 0;
          else
            allow_fifo = 1;
#else
          ZIPERR(ZE_PARMS, "-FI (FIFO support) not enabled on this platform");
#endif
          break;
        case o_FS:  /* delete exiting entries in archive where there is
                       no matching file on file system */
          filesync = 1; break;
        case 'f':   /* Freshen zip file--overwrite only */
          if (action != ADD) {
            ZIPERR(ZE_PARMS, "specify just one action");
          }
          action = FRESHEN;
          break;
        case 'g':   /* Allow appending to a zip file */
          d = 1;  break;

        case 'h': case 'H': case '?':  /* Help */
#ifdef VMSCLI
          VMSCLI_help();
#else
          help();
#endif
          RETURN(finish(ZE_OK));

        case o_h2:  /* Extended Help */
          help_extended();
          RETURN(finish(ZE_OK));

        /* -i is with -x */
#if defined(VMS) || defined(WIN32)
        case o_ic:  /* Ignore case (case-insensitive matching of archive entries) */
          if (negated)
            filter_match_case = 1;
          else
            filter_match_case = 0;
          break;
#endif
#ifdef RISCOS
        case 'I':   /* Don't scan through Image files */
          scanimage = 0;
          break;
#endif
#ifdef MACOS
        case o_jj:   /* store absolute path including volname */
            MacZip.StoreFullPath = true;
            break;
#endif /* ?MACOS */
        case 'j':   /* Junk directory names */
          if (negated)
            pathput = 1;
          else
            pathput = 0;
          break;
        case 'J':   /* Junk sfx prefix */
          junk_sfx = 1;  break;
        case 'k':   /* Make entries using DOS names (k for Katz) */
          dosify = 1;  break;
        case o_kf:  /* Read (end part of) password from keyfile */
#ifdef IZ_CRYPT_ANY
          keyfile = value;
          if (encryption_method == NO_ENCRYPTION) {
# ifdef IZ_CRYPT_TRAD
            encryption_method = TRADITIONAL_ENCRYPTION;
# else
            encryption_method = AES_128_ENCRYPTION;
# endif
          }
#else /* def IZ_CRYPT_ANY */
          ZIPERR(ZE_PARMS, "encryption (-kf) not supported");
#endif /* def IZ_CRYPT_ANY [else] */
          break;
        case 'l':   /* Translate end-of-line */
          translate_eol = 1;
          binary_full_check = 1;
          break;
        case o_ll:
          translate_eol = 2;
          binary_full_check = 1;
          break;
        case o_lf:
          /* open a logfile */
          /* allow multiple use of option but only last used */
          if (logfile_path) {
            free(logfile_path);
          }
          logfile_path = value;
          break;
        case o_lF:
          /* open a logfile */
          /* allow multiple use of option but only last used */
          if (logfile_path) {
            free(logfile_path);
          }
          logfile_path = NULL;
          if (negated) {
            use_outpath_for_log = 0;
          } else {
            use_outpath_for_log = 1;
          }
          break;
        case o_la:
          /* append to existing logfile */
          if (negated)
            logfile_append = 0;
          else
            logfile_append = 1;
          break;
        case o_li:
          /* log all including informational messages */
          if (negated)
            logall = 0;
          else
            logall = 1;
          break;
        case o_lu:
          /* log messages in UTF-8 (requires UTF-8 enabled reader to read log) */
#ifdef UNICODE_SUPPORT
          if (negated)
            log_utf8 = 0;
          else
            log_utf8 = 1;
#else
          ZIPERR(ZE_PARMS, "Unicode (-lu) not enabled/supported");
#endif
          break;
        case 'L':   /* Show license */
          license();
          RETURN(finish(ZE_OK));
        case 'm':   /* Delete files added or updated in zip file */
          dispose = 1;  break;
        case o_mm:  /* To prevent use of -mm for -MM */
          ZIPERR(ZE_PARMS, "-mm not supported, Must_Match is -MM");
          dispose = 1;  break;
        case o_MM:  /* Exit with error if input file can't be read */
          bad_open_is_error = 1; break;
#ifdef CMS_MVS
        case o_MV:   /* MVS path translation mode */
          if (abbrevmatch("dots", value, CASE_INS, 1)) {
            /* aaa.bbb.ccc.ddd stored as is */
            mvs_mode = 1;
          } else if (abbrevmatch("slashes", value, CASE_INS, 1)) {
            /* aaa.bbb.ccc.ddd -> aaa/bbb/ccc/ddd */
            mvs_mode = 2;
          } else if (abbrevmatch("lastdot", value, CASE_INS, 1)) {
            /* aaa.bbb.ccc.ddd -> aaa/bbb/ccc.ddd */
            mvs_mode = 0;
          } else {
            zipwarn("-MV must be dots, slashes, or lastdot: ", value);
            free(value);
            ZIPERR(ZE_PARMS, "-MV (MVS path translate mode) bad value");
          }
          free(value);
          break;
#endif /* CMS_MVS */
        case 'n':   /* Specify compression-method-by-name-suffix list. */
          {         /* Value format: "method[-lvl]=sfx1:sfx2:sfx3...". */
            suffixes_option(value);
          }
          break;
        case o_nw:  /* no wildcards - wildcards are handled like other
                       characters */
          no_wild = 1;
          break;
#if defined(AMIGA) || defined(MACOS)
        case 'N':   /* Get zipfile comments from AmigaDOS/MACOS filenotes */
          filenotes = 1; break;
#endif
        case 'o':   /* Set zip file time to time of latest file in it */
          latest = 1;  break;
        case 'O':   /* Set output file different than input archive */
          if (strcmp( value, "-") == 0)
          {
            mesg = stderr;
            if (isatty(1))
              ziperr(ZE_PARMS, "cannot write zip file to terminal");
            if ((out_path = malloc(4)) == NULL)
              ziperr(ZE_MEM, "was processing arguments (2)");
            strcpy( out_path, "-");
          }
          else
          {
            out_path = ziptyp(value);
            free(value);
          }
          have_out = 1;
          break;
        case 'p':   /* Store path with name */
          zipwarn("-p (include path) is deprecated.  Use -j- instead", "");
          if (negated)
            pathput = 0;
          else
            pathput = 1;
          break;            /* (do nothing as annoyance avoidance) */
        case o_pa:  /* Set prefix for paths of new entries in archive */
          if (path_prefix)
            free(path_prefix);
          path_prefix = value;
          path_prefix_mode = 1;
          break;
        case o_pp:  /* Set prefix for paths of new entries in archive */
          if (path_prefix)
            free(path_prefix);
          path_prefix = value;
          path_prefix_mode = 0;
          break;
        case o_pn:  /* Allow non-ANSI password */
          if (negated) {
            force_ansi_key = 1;
          } else {
            force_ansi_key = 0;
          }
          break;
        case o_ps:  /* Allow short password */
          if (negated) {
            allow_short_key = 0;
          } else {
            allow_short_key = 1;
          }
          break;
        case o_pt:  /* Time performance */
#ifdef ENABLE_ENTRY_TIMING
          if (negated) {
            performance_time = 0;
          } else {
            performance_time = 1;
          }
#else
          ZIPERR(ZE_PARMS, "Performance timing (-pt) not enabled/supported");
#endif
          break;
        case o_pu:  /* Pass password to unzip for test */
          if (negated) {
            pass_pswd_to_unzip = 0;
          } else {
            pass_pswd_to_unzip = 1;
          }
          break;
        case 'P':   /* password for encryption */
          if (key != NULL) {
            free(key);
          }
#ifdef IZ_CRYPT_ANY
          key = value;
          key_needed = 0;
          if (encryption_method == NO_ENCRYPTION) {
# ifdef IZ_CRYPT_TRAD
            encryption_method = TRADITIONAL_ENCRYPTION;
# else
            encryption_method = AES_128_ENCRYPTION;
# endif
          }
#else /* def IZ_CRYPT_ANY */
          ZIPERR(ZE_PARMS, "encryption (-P) not supported");
#endif /* def IZ_CRYPT_ANY [else] */
          break;
#if defined(QDOS) || defined(QLZIP)
        case 'Q':
          qlflag  = strtol(value, NULL, 10);
       /* qlflag  = strtol((p+1), &p, 10); */
       /* p--; */
          if (qlflag == 0) qlflag = 4;
          free(value);
          break;
#endif
        case 'q':   /* Quiet operation */
          noisy = 0;
#ifdef MACOS
          MacZip.MacZip_Noisy = false;
#endif  /* MACOS */
          if (verbose) verbose--;
          break;
        case 'r':   /* Recurse into subdirectories, match full path */
          if (recurse == 2) {
            ZIPERR(ZE_PARMS, "do not specify both -r and -R");
          }
          recurse = 1;  break;
        case 'R':   /* Recurse into subdirectories, match filename */
          if (recurse == 1) {
            ZIPERR(ZE_PARMS, "do not specify both -r and -R");
          }
          recurse = 2;  break;

        case o_RE:   /* Allow [list] matching (regex) */
          allow_regex = 1; break;

        case o_sc:  /* show command line args */
          if (value) {
            if (abbrevmatch("read", value, CASE_INS, 1)) {
              show_args = 1;
            }
            else if (abbrevmatch("parsed", value, CASE_INS, 1)) {
              show_parsed_args = 1;
            }
            else if (abbrevmatch("all", value, CASE_INS, 1)) {
              show_args = 1;
              show_parsed_args = 1;
            }
          }
          else { /* no value */
            show_args = 1;
          }
          break;

#if 0
        case o_sP:  /* show parsed command line args */
          show_parsed_args = 1; break;
#endif

#ifdef UNICODE_EXTRACT_TEST
        case o_sC:  /* create empty files from archive names */
          create_files = 1;
          show_files = 1; break;
#endif
        case o_sd:  /* show debugging */
          show_what_doing = 1; break;
        case o_sf:  /* show files to operate on */
          if (!negated)
            show_files = 1;
          else
            show_files = 2;
          break;
        case o_sF:  /* list of params for -sf */
          /* if beginning of 2 or more match, should check for
             ambiguity and issue ambiguity message */
          if (abbrevmatch("usize", value, CASE_INS, 1)) {
            sf_usize = 1;
          }
          else if (abbrevmatch("comment", value, CASE_INS, 1)) {
            sf_comment = 1;
          }
          else {
            zipwarn("bad -sF parameter: ", value);
            free(value);
            zipwarn("valid values are: usize, comment", "");
            ZIPERR(ZE_PARMS, "-sF (-sf (show files) parameter) bad value");
          }
          free(value);
          break;

#if !defined( VMS) && defined( ENABLE_USER_PROGRESS)
        case o_si:  /* Show process id. */
          show_pid = 1; break;
#endif /* !defined( VMS) && defined( ENABLE_USER_PROGRESS) */

        case o_so:  /* show all options */
          show_options = 1; break;
        case o_ss:  /* show all options */
          show_suffixes = 1; break;
        case o_su:  /* -sf but also show Unicode if exists */
#ifdef UNICODE_SUPPORT
          if (!negated)
            show_files = 3;
          else
            show_files = 4;
          break;
#else
          ZIPERR(ZE_PARMS, "no Unicode support: -su not available");
#endif
        case o_sU:  /* -sf but only show Unicode if exists or normal if not */
#ifdef UNICODE_SUPPORT
          if (!negated)
            show_files = 5;
          else
            show_files = 6;
          break;
#else
          ZIPERR(ZE_PARMS, "no Unicode support: -sU not available");
#endif

        case 's':   /* enable split archives */
          /* get the split size from value */
          if (strcmp(value, "-") == 0) {
            /* -s- do not allow splits */
            split_method = -1;
          } else {
            split_size = ReadNumString(value);
            if (split_size == (uzoff_t)-1) {
              sprintf(errbuf, "bad split size:  '%s'", value);
              ZIPERR(ZE_PARMS, errbuf);
            }
            if (split_size == 0) {
              /* do not allow splits */
              split_method = -1;
            } else {
              if (split_method == 0) {
                split_method = 1;
              }
              if (split_size < 0x400) {
                /* < 1 KB there is no multiplier, assume MiB */
                split_size *= 0x100000;
              }
              /* By setting the minimum split size to 64 KB we avoid
                 not having enough room to write a header unsplit
                 which is required */
              if (split_size < 0x400L * 64) {
                /* split_size < 64K */
                sprintf(errbuf, "minimum split size is 64 KB:  '%s'", value);
                free(value);
                ZIPERR(ZE_PARMS, errbuf);
              }
            }
          }
          free(value);
          break;
        case o_sb:  /* when pause for next split ring bell */
          split_bell = 1;
          break;
        case o_sp:  /* enable split select - pause splitting between splits */
#if defined(ZIPLIB) || defined(ZIPDLL)
          free(value);
          ZIPERR(ZE_PARMS, "-sp not yet supported for LIB or DLL");
#else
          use_descriptors = 1;
          split_method = 2;
#endif
          break;
        case o_sv:  /* be verbose about creating splits */
          noisy_splits = 1;
          break;

#ifdef STREAM_EF_SUPPORT
        case o_st:  /* enable stream mode (storing of attributes and comments in local headers) */
          include_stream_ef = 1;
          break;
#endif


#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(ATARI)
        case 'S':
          hidden_files = 1; break;
#endif /* MSDOS || OS2 || WIN32 || ATARI */
#ifdef MACOS
        case 'S':
          MacZip.IncludeInvisible = true; break;
#endif /* MACOS */
        case o_SI:  /* Change default stdin name "-" to this */
          if (stdin_name)
            free(stdin_name);
          stdin_name = value;
          break;
        case 't':   /* Exclude files earlier than specified date */
          {
            ulg dt;

            /* Support ISO 8601 & American dates */
            dt = datetime( value, cur_time_opt);
            if (dt == DT_BAD)
            {
              ZIPERR(ZE_PARMS,
         "invalid date/time for -t:  use mmddyyyy or yyyy-mm-dd[:HH:MM[:SS]]");
            }
            before = dt;
          }
          free(value);
          break;
        case o_tt:  /* Exclude files at or after specified date */
          {
            ulg dt;

            /* Support ISO 8601 & American dates */
            dt = datetime( value, cur_time_opt);
            if (dt == DT_BAD)
            {
              ZIPERR(ZE_PARMS,
         "invalid date/time for -tt:  use mmddyyyy or yyyy-mm-dd[:HH:MM[:SS]]");
            }
            after = dt;
          }
          free(value);
          break;
#ifdef USE_EF_UT_TIME
        case o_tn:
          no_universal_time = 1;
          break;
#endif
        case 'T':   /* test zip file */
#ifdef ZIP_DLL_LIB
          ZIPERR(ZE_PARMS, "-T not supported by DLL/LIB");
#endif
          test = 1; break;
        case o_TT:  /* command string to use instead of 'unzip -t ' */
#ifdef ZIP_DLL_LIB
          ZIPERR(ZE_PARMS, "-TT not supported by DLL/LIB");
#endif
          if (unzip_string)
            free(unzip_string);
          unzip_string = value;
          break;
        case o_TU:  /* unzip path */
#ifdef ZIP_DLL_LIB
          ZIPERR(ZE_PARMS, "-TU not supported by DLL/LIB");
#endif
          if (unzip_path)
            free(unzip_path);
          unzip_path = value;
          break;
        case o_TV:  /* unzip verbose */
#ifdef ZIP_DLL_LIB
          ZIPERR(ZE_PARMS, "-TV not supported by DLL/LIB");
#endif
          if (negated)
            unzip_verbose = 0;
          else
            unzip_verbose = 1;
          break;
        case 'U':   /* Select archive entries to keep or operate on */
          if (action != ADD) {
            ZIPERR(ZE_PARMS, "specify just one action");
          }
          action = ARCHIVE;
          break;
        case o_UN:   /* Unicode */
#ifdef UNICODE_SUPPORT
          {
            int value_len = (int)strlen(value);
            negated = 0;
            if (value_len && value[value_len - 1] == '-') {
              negated = 1;
              value[value_len - 1] = '\0';
            }
          }
          /* quit, warn, ignore, and no are mutually exclusive */
          if (abbrevmatch("quit", value, CASE_INS, 1)) {
            /* Unicode path mismatch is error */
            if (negated)
              ZIPERR(ZE_PARMS, "-UN=quit not negatable");
            unicode_mismatch = 0;
          } else if (abbrevmatch("warn", value, CASE_INS, 1)) {
            /* warn of mismatches and continue */
            if (negated)
              ZIPERR(ZE_PARMS, "-UN=warn not negatable");
            unicode_mismatch = 1;
          } else if (abbrevmatch("ignore", value, CASE_INS, 1)) {
            /* ignore mismatches and continue */
            if (negated)
              ZIPERR(ZE_PARMS, "-UN=ignore not negatable");
            unicode_mismatch = 2;
          } else if (abbrevmatch("no", value, CASE_INS, 1)) {
            /* no use Unicode path */
            if (negated)
              ZIPERR(ZE_PARMS, "-UN=no not negatable");
            unicode_mismatch = 3;

          } else if (abbrevmatch("escape", value, CASE_INS, 1)) {
            /* escape all non-ASCII characters */
            if (!negated)
              unicode_escape_all = 1;
            else
              unicode_escape_all = 0;

          } else if (abbrevmatch("LOCAL", value, CASE_INS, 1)) {
            /* use extra fields for backward compatibility (reverse of UTF8) */
            if (!negated)
              utf8_native = 0;
            else
              utf8_native = 1;
          } else if (abbrevmatch("UTF8", value, CASE_INS, 1)) {
            /* store UTF-8 as standard per AppNote bit 11 (reverse of LOCAL) */
            if (!negated)
              utf8_native = 1;
            else
              utf8_native = 0;

          } else if (abbrevmatch("ShowUTF8", value, CASE_INS, 1)) {
            /* on ports that support it (mainly later WIN32), show UTF-8 directly on console */
            /* Note that when WIN32 wide command line is read (WIN32_WIDE_CMD_LINE)
               unicode_show is set. */
            if (!negated)
              unicode_show = 1;
            else
              unicode_show = 0;

          } else {
            zipwarn("-UN must be Quit, Warn, Ignore, No, Escape, UTF8, LOCAL, or ShowUTF8: ", value);

            sprintf(errbuf, "-UN (unicode) bad value:  %s", value);
            free(value);
            ZIPERR(ZE_PARMS, errbuf);
          }
          free(value);
          break;
#else
          ZIPERR(ZE_PARMS, "no Unicode support: -UN not available");
#endif
#ifdef UNICODE_TESTS_OPTION
        case o_UT:   /* Perform select Unicode tests and exit */
#ifdef UNICODE_SUPPORT
          Unicode_Tests();
          RETURN(finish(ZE_OK));
          break;
#else
          ZIPERR(ZE_PARMS, "no Unicode support: -UT not available");
#endif
#endif
        case 'u':   /* Update zip file--overwrite only if newer */
          if (action != ADD) {
            ZIPERR(ZE_PARMS, "specify just one action");
          }
          action = UPDATE;
          break;
        case 'v':        /* Either display version information or */
        case o_ve:       /* Mention oddities in zip file structure */
          if (option == o_ve ||      /* --version */
              (argcnt == 2 && strlen(args[1]) == 2)) { /* -v only */
            /* display version */
            version_info();
            RETURN(finish(ZE_OK));
          } else {
            noisy = 1;
            verbose++;
          }
          break;
        case o_vq:       /* Display Quick Version */
          quick_version();
          RETURN(finish(ZE_OK));
          break;
#ifdef VMS
        case 'C':  /* Preserve case (- = down-case) all. */
          if (negated)
          { /* Down-case all. */
            if ((vms_case_2 > 0) || (vms_case_5 > 0))
            {
              ZIPERR( ZE_PARMS, "Conflicting case directives (-C-)");
            }
            vms_case_2 = -1;
            vms_case_5 = -1;
          }
          else
          { /* Not negated.  Preserve all. */
            if ((vms_case_2 < 0) || (vms_case_5 < 0))
            {
              ZIPERR( ZE_PARMS, "Conflicting case directives (-C)");
            }
            vms_case_2 = 1;
            vms_case_5 = 1;
          }
          break;
        case o_C2:  /* Preserve case (- = down-case) ODS2. */
          if (negated)
          { /* Down-case ODS2. */
            if (vms_case_2 > 0)
            {
              ZIPERR( ZE_PARMS, "Conflicting case directives (-C2-)");
            }
            vms_case_2 = -1;
          }
          else
          { /* Not negated.  Preserve ODS2. */
            if (vms_case_2 < 0)
            {
              ZIPERR( ZE_PARMS, "Conflicting case directives (-C2)");
            }
            vms_case_2 = 1;
          }
          break;
        case o_C5:  /* Preserve case (- = down-case) ODS5. */
          if (negated)
          { /* Down-case ODS5. */
            if (vms_case_5 > 0)
            {
              ZIPERR( ZE_PARMS, "Conflicting case directives (-C5-)");
            }
            vms_case_5 = -1;
          }
          else
          { /* Not negated.  Preserve ODS5. */
            if (vms_case_5 < 0)
            {
              ZIPERR( ZE_PARMS, "Conflicting case directives (-C5)");
            }
            vms_case_5 = 1;
          }
          break;
        case 'V':   /* Store in VMS format.  (Record multiples.) */
          vms_native = 1; break;
          /* below does work with new parser but doesn't allow tracking
             -VV separately, like adding a separate description */
          /* vms_native++; break; */
        case o_VV:  /* Store in VMS specific format */
          vms_native = 2; break;
        case o_vn:  /* Preserve idiosyncratic VMS file names. */
          if (negated) {
            prsrv_vms = 0;
          } else {
            prsrv_vms = 1;
          }
          break;
        case 'w':   /* Append the VMS version number */
          vmsver |= 1;  break;

        case o_ww:   /* Append the VMS version number as ".nnn". */
          vmsver |= 3;  break;
#endif /* VMS */

#ifdef WINDOWS_LONG_PATHS
        case o_wl:  /* Include windows paths > MAX_PATH (260 characters) */
          if (negated)
            include_windows_long_paths = 0;
          else
            include_windows_long_paths = 1;
          break;
#endif

        case o_ws:  /* Wildcards do not include directory boundaries in matches */
          wild_stop_at_dir = 1;
          break;

        case 'i':   /* Include only the following files */
          /* if nothing matches include list then still create an empty archive */
          allow_empty_archive = 1;
        case 'x':   /* Exclude following files */
          add_filter((int) option, value);
          free(value);
          break;
#ifdef SYMLINKS
        case 'y':   /* Store symbolic links as such */
          linkput = 1;
          break;
# ifdef WIN32
        case o_yy:  /* Mount points */
          /* Normal action is to follow normal mount points */
          if (negated) /* -yy- */
            follow_mount_points = FOLLOW_ALL;  /* Include special */
          else         /* -yy */
            follow_mount_points = FOLLOW_NONE; /* Skip mount points */
          break;

# endif
#endif /* SYMLINKS */

#ifdef IZ_CRYPT_ANY
        case 'Y':   /* Encryption method */
          encryption_method = NO_ENCRYPTION;

          if (abbrevmatch("traditional", value, CASE_INS, 1) ||
              abbrevmatch("zipcrypto", value, CASE_INS, 1)) {
#  ifdef IZ_CRYPT_TRAD
            encryption_method = TRADITIONAL_ENCRYPTION;
#  else /* def IZ_CRYPT_TRAD */
            free(value);
            ZIPERR(ZE_PARMS,
                   "Traditional zip encryption not supported in this build");
#  endif /* def IZ_CRYPT_TRAD [else] */
          }
          if (strmatch("AES", value, CASE_INS, 3)) {
# if defined(IZ_CRYPT_AES_WG) || defined(IZ_CRYPT_AES_WG_NEW)
            if (abbrevmatch("AES128", value, CASE_INS, 5)) {
              encryption_method = AES_128_ENCRYPTION;
            }
            else if (abbrevmatch("AES192", value, CASE_INS, 5)) {
              encryption_method = AES_192_ENCRYPTION;
            }
            else if (abbrevmatch("AES256", value, CASE_INS, 4)) {
              encryption_method = AES_256_ENCRYPTION;
            }
            else if (strmatch("AES", value, CASE_INS, ENTIRE_STRING) ||
                     strmatch("AES1", value, CASE_INS, ENTIRE_STRING)) {
              zipwarn("encryption method ambiguous: ", value);
            }
# else
            free(value);
            ZIPERR(ZE_PARMS,
                   "AES encryption not supported in this build");
# endif
          }
          if (encryption_method == NO_ENCRYPTION) {
            /* Method specified not valid one. */
            strcpy(errbuf, "");
# ifdef IZ_CRYPT_TRAD
            strcat(errbuf, "Traditional (ZipCrypto)");
# endif
# if defined( IZ_CRYPT_AES_WG) || defined( IZ_CRYPT_AES_WG_NEW)
            if (strlen(errbuf) > 0)
              strcat(errbuf, ", ");
            strcat(errbuf, "AES128, AES192, and AES256");
# endif
            zipwarn("valid encryption methods are:  ", errbuf);
            sprintf(errbuf,
                    "Option -Y:  unknown encryption method \"%s\"", value);
            free(value);
            ZIPERR(ZE_PARMS, errbuf);
          }
          free(value);
          break;
#else /* def IZ_CRYPT_ANY */
          ZIPERR(ZE_PARMS, "encryption (-Y) not supported");
#endif /* def IZ_CRYPT_ANY [else] */

        case 'z':   /* Edit zip file comment */
          zipedit = 1;  break;

        case 'Z':   /* Compression method */
          if (abbrevmatch("deflate", value, CASE_INS, 1)) {
            /* deflate */
            method = DEFLATE;
          } else if (abbrevmatch("store", value, CASE_INS, 1)) {
            /* store */
            method = STORE;
          } else if (abbrevmatch("bzip2", value, CASE_INS, 1)) {
            /* bzip2 */
#ifdef BZIP2_SUPPORT
            method = BZIP2;
#else
            ZIPERR(ZE_COMPILE, "Compression method bzip2 not enabled");
#endif
          } else if (abbrevmatch("lzma", value, CASE_INS, 1)) {
            /* LZMA */
#ifdef LZMA_SUPPORT
            method = LZMA;
#else
            ZIPERR(ZE_COMPILE, "Compression method LZMA not enabled");
#endif
          } else if (abbrevmatch("ppmd", value, CASE_INS, 1)) {
            /* PPMD */
#ifdef PPMD_SUPPORT
            method = PPMD;
#else
            ZIPERR(ZE_COMPILE, "Compression method PPMd not enabled");
#endif
          } else if (abbrevmatch("cd_only", value, CASE_INS, 3)) {
            /* cd_only */
#ifdef CD_ONLY_SUPPORT
            method = CD_ONLY;
#else
            ZIPERR(ZE_COMPILE, "Compression method cd_only not enabled");
#endif
          } else {
            strcpy(errbuf, "store, deflate");
#ifdef BZIP2_SUPPORT
            strcat(errbuf, ", bzip2");
#endif
#ifdef LZMA_SUPPORT
            strcat(errbuf, ", lzma");
#endif
#ifdef PPMD_SUPPORT
            strcat(errbuf, ", ppmd");
#endif
#ifdef CD_ONLY_SUPPORT
            strcat(errbuf, ", cd_only");
#endif
            zipwarn("valid compression methods are:  ", errbuf);
            sprintf(errbuf,
                    "Option -Z:  unknown compression method \"%s\"", value);
            free(value);
            ZIPERR(ZE_PARMS, errbuf);
          }
          free(value);
          break;

#if defined(MSDOS) || defined(OS2) || defined( VMS)
        case '$':   /* Include volume label */
          volume_label = 1; break;
#endif
#ifndef MACOS
        case '@':   /* read file names from stdin */
          comment_stream = NULL;
          s = 1;          /* defer -@ until have zipfile name */
          break;
#endif /* !MACOS */

        case o_atat: /* -@@ - read file names from file */
          {
            FILE *infile;
            char *path;

            if (value == NULL || strlen(value) == 0) {
              ZIPERR(ZE_PARMS, "-@@:  No file name to open to read names from");
            }

#if !defined(MACOS) && !defined(WINDLL)
            /* Treat "-@@ -" as "-@" */
            if (strcmp( value, "-") == 0)
            {
              comment_stream = NULL;
              s = 1;          /* defer -@ until have zipfile name */
              break;
            }
#endif /* ndef MACOS */

            infile = zfopen(value, "r");
            if (infile == NULL) {
              sprintf(errbuf, "-@@:  Cannot open input file:  %s\n", value);
              ZIPERR(ZE_OPEN, errbuf);
            }
#ifdef UNICODE_SUPPORT
# if 0
            /* As each arg and path is separately checked for UTF-8, checking
               entire file is not that relevant now. */
            if (is_utf8_file(infile, NULL, NULL, NULL, NULL)) {
              zipmessage("-@@ include file is UTF-8", "");
            }
            else
# endif
            if (is_utf16LE_file(infile)) {
              zipmessage("-@@:  Input file encoded as Windows UTF-16 (saved as \"Unicode\")", "");
              zipmessage("Not supported.  Save file as \"UTF-8\" instead", "");
              sprintf(errbuf, "-@@:  Unsupported Unicode format:  %s\n", value);
              ZIPERR(ZE_OPEN, errbuf);
            }
#endif

            while ((path = getnam(infile)) != NULL)
            {
              /* file argument now processed later */

              /* trimming now done by getnam() */

              if (strlen(path) > 0)
                add_name(path, 0);
              free(path);
            } /* while */
            fclose(infile);
            names_from_file = 1;
          }
          break;

        case 'X':
          if (negated)
            extra_fields = 2;
          else
            extra_fields = 0;
          break;
#ifdef OS2
        case 'E':
          /* use the .LONGNAME EA (if any) as the file's name. */
          use_longname_ea = 1;
          break;
#endif
#ifdef NTSD_EAS
        case '!':
          /* use security privilege overrides */
          use_privileges = 1;
          break;

        case o_exex:  /* (-!!) */
          /* leave out security information (ACLs) */
          no_security = 1;
          break;
#endif
#ifdef RISCOS
        case '/':
          exts2swap = value; /* override Zip$Exts */
          break;
#endif
        case o_des:
          use_descriptors = 1;
          break;

#ifdef ZIP64_SUPPORT
        case o_z64:   /* Force creation of Zip64 entries */
          if (negated) {
            force_zip64 = 0;
          } else {
            force_zip64 = 1;
          }
          break;
#endif

        case o_NON_OPTION_ARG:
          /* not an option */
          /* no more options as permuting */
          /* just dash also ends up here */

          if (recurse != 2 && kk == 0 && patterns == NULL) {
            /* have all filters so convert filterlist to patterns array
               as PROCNAME needs patterns array */
            filterlist_to_patterns();
          }

          /* "--" stops arg processing for remaining args */
          /* ignore only first -- */
          if (strcmp(value, "--") == 0 && seen_doubledash == 0) {
            /* -- */
            seen_doubledash = 1;
            if (kk == 0) {
              ZIPERR(ZE_PARMS, "can't use -- before archive name");
            }

            /* just ignore as just marks what follows as non-option arguments */

          } else if (kk == 6) {
            /* value is R pattern */

            add_filter((int)'R', value);
            free(value);
            if (first_listarg == 0) {
              first_listarg = argnum;
            }
          } else switch (kk)
          {
            case 0:
              /* first non-option arg is zipfile name */
#ifdef BACKUP_SUPPORT
              /* if doing a -BT operation, archive name is created from backup
                 path and a timestamp instead of read from command line */
              if (backup_type == 0)
              {
#endif /* BACKUP_SUPPORT */
#if (!defined(MACOS))
                if (strcmp(value, "-") == 0) {  /* output zipfile is dash */
                  /* just a dash */
# ifdef WINDLL
                  ZIPERR(ZE_PARMS, "DLL can't output to stdout");
# else
                  zipstdout();
# endif
                } else
#endif /* !MACOS */
                {
                  /* name of zipfile */
                  if ((zipfile = ziptyp(value)) == NULL) {
                    ZIPERR(ZE_MEM, "was processing arguments (3)");
                  }
                  /* read zipfile if exists */
                  /*
                  if ((r = readzipfile()) != ZE_OK) {
                    ZIPERR(r, zipfile);
                  }
                  */

                  free(value);
                }
                if (show_what_doing) {
                  char s[10];
#ifdef UNICODE_SUPPORT
                  if (is_utf8_string(zipfile, NULL, NULL, NULL, NULL))
                    strcpy(s, "UTF-8");
                  else
#endif
                    strcpy(s, "LOCAL");
                  sprintf(errbuf, "sd: Zipfile name '%s' (%s)", zipfile, s);
                  zipmessage(errbuf, "");
                }
                /* if in_path not set, use zipfile path as usual for input */
                /* in_path is used as the base path to find splits */
                if (in_path == NULL) {
                  if ((in_path = malloc(strlen(zipfile) + 1)) == NULL) {
                    ZIPERR(ZE_MEM, "was processing arguments (4)");
                  }
                  strcpy(in_path, zipfile);
                }
                /* if out_path not set, use zipfile path as usual for output */
                /* out_path is where the output archive is written */
                if (out_path == NULL) {
                  if ((out_path = malloc(strlen(zipfile) + 1)) == NULL) {
                    ZIPERR(ZE_MEM, "was processing arguments (5)");
                  }
                  strcpy(out_path, zipfile);
                }
#ifdef BACKUP_SUPPORT
              }
#endif /* BACKUP_SUPPORT */
              kk = 3;
              if (s)
              {
                /* do -@ and get names from stdin */
                /* Should be able to read names from
                   stdin and output to stdout, but
                   this was not allowed in old code.
                   This check moved to kk = 3 case to fix. */
                /* if (strcmp(zipfile, "-") == 0) {
                  ZIPERR(ZE_PARMS, "can't use - and -@ together");
                }
                */
                while ((pp = getnam(stdin)) != NULL)
                {
                  kk = 4;
                  if (recurse == 2) {
                    /* reading patterns from stdin */
                    add_filter((int)'R', pp);
                  } else {
                    /* file argument now processed later */
                    add_name(pp, 0);
                  }
                  /*
                  if ((r = PROCNAME(pp)) != ZE_OK) {
                    if (r == ZE_MISS)
                      zipwarn("name not matched: ", pp);
                    else {
                      ZIPERR(r, pp);
                    }
                  }
                  */
                  free(pp);
                }
                s = 0;
              }
              if (recurse == 2) {
                /* rest are -R patterns */
                kk = 6;
              }
#ifdef BACKUP_SUPPORT
              /* if using -BT, then value is really first input argument */
              if (backup_type == 0)
#endif
                break;

            case 3:  case 4:
              /* no recurse and -r file names */
              /* can't read filenames -@ and input - from stdin at
                 same time */
              if (s == 1 && strcmp(value, "-") == 0) {
                ZIPERR(ZE_PARMS, "can't read input (-) and filenames (-@) both from stdin");
              }
              /* add name to list for later processing */
              add_name(value, seen_doubledash);
              /*
              if ((r = PROCNAME(value)) != ZE_OK) {
                if (r == ZE_MISS)
                  zipwarn("name not matched: ", value);
                else {
                  ZIPERR(r, value);
                }
              }
              */
              free(value);  /* Added by Polo from forum */
              if (kk == 3) {
                first_listarg = argnum;
                kk = 4;
              }
              break;

            } /* switch kk */
            break;

        default:
          /* should never get here as get_option will exit if not in table,
             unless option is in table but no case for it */
          sprintf(errbuf, "no such option ID: %ld", option);
          ZIPERR(ZE_PARMS, errbuf);

    }  /* switch */
  } /* while */


  /* ========================================================== */

#ifdef SHOW_PARSED_COMMAND
  if (parsed_args) {
    parsed_args[parsed_arg_count] = NULL; /* NULL-terminate parsed_args[]. */
  }
#endif /* def SHOW_PARSED_COMMAND */


  /* do processing of command line and one-time tasks */

  if (show_what_doing) {
    zipmessage("sd: Command line read", "");
  }

  /* Check method-level suffix options. */
  if (mthd_lvl[0].suffixes == NULL)
  {
    /* We expect _some_ non-NULL default Store suffix list, even if ""
     * is compiled in.  This check must be done before setting an empty
     * list ("", ":", or ";") to NULL (below).
     */
    ZIPERR(ZE_PARMS, "missing Store suffix list");
  }

  for (i = 0; mthd_lvl[ i].method >= 0; i++)
  {
    /* Replace an empty suffix list ("", ":", or ";") with NULL. */
    if ((mthd_lvl[i].suffixes != NULL) &&
        ((mthd_lvl[i].suffixes[0] == '\0') ||
         !strcmp(mthd_lvl[i].suffixes, ";") ||
         !strcmp(mthd_lvl[i].suffixes, ":")))
    {
      mthd_lvl[i].suffixes = NULL;     /* NULL out an empty list. */
    }
  }

  /* Show method-level suffix lists. */
  if (show_suffixes)
  {
    char level_str[ 8];
    char *suffix_str;
    int any = 0;

    zfprintf( mesg, "   Method[-lvl]=Suffix_list\n");
    for (i = 0; mthd_lvl[ i].method >= 0; i++)
    {
      suffix_str = mthd_lvl[ i].suffixes;
      /* "-v": Show all methods.  Otherwise, only those with a suffix list. */
      if (verbose || (suffix_str != NULL))
      {
        /* "-lvl" string, if a non-default level was specified. */
        if ((mthd_lvl[ i].level_sufx <= 0) || (suffix_str == NULL))
          strcpy( level_str, "  ");
        else
          sprintf( level_str, "-%d", mthd_lvl[ i].level_sufx);

        /* Display an empty string, if none specified. */
        if (suffix_str == NULL)
          suffix_str = "";

        zfprintf(mesg, "     %8s%s=%s\n",
                 mthd_lvl[ i].method_str, level_str, suffix_str);
        if (suffix_str != NULL)
          any = 1;
      }
    }
    if (any == 0)
    {
      zfprintf( mesg, "   (All suffix lists empty.)\n");
    }
    zfprintf( mesg, "\n");
    ZIPERR(ZE_ABORT, "show suffixes");
  }

  /* show command line args */
  if (show_args || show_parsed_args) {
    if (show_args) {
      zfprintf(mesg, "command line:\n");
      for (i = 0; args[i]; i++) {
        sprintf(errbuf, "'%s'  ", args[i]);
        print_utf8(errbuf);
      }
      zfprintf(mesg, "\n");
    }

#ifdef SHOW_PARSED_COMMAND
    if (show_parsed_args) {
      if (parsed_args == NULL) {
        zfprintf(mesg, "\nno parsed command line\n");
      }
      else
      {
        zfprintf(mesg, "\nparsed command line:\n");
        for (i = 0; parsed_args[i]; i++) {
          sprintf(errbuf, "'%s'  ", parsed_args[i]);
          print_utf8(errbuf);
        }
        zfprintf(mesg, "\n");
      }
    }
#endif
  }

#ifdef SHOW_PARSED_COMMAND
  /* free up parsed command line */
  if (parsed_args) {
    for (i = 0; parsed_args[i]; i++) {
      free(parsed_args[i]);
    }
    free(parsed_args);
  }
#endif

  if (show_args || show_parsed_args) {
    RETURN(finish(ZE_OK));
#if 0
    ZIPERR(ZE_ABORT, "show command line");
#endif
  }


  /* show all options */
  if (show_options) {
    zprintf("available options:\n");
    zprintf(" %-2s  %-18s %-4s %-3s %-30s\n", "sh", "long", "val", "neg", "description");
    zprintf(" %-2s  %-18s %-4s %-3s %-30s\n", "--", "----", "---", "---", "-----------");
    for (i = 0; options[i].option_ID; i++) {
      zprintf(" %-2s  %-18s ", options[i].shortopt, options[i].longopt);
      switch (options[i].value_type) {
        case o_NO_VALUE:
          zprintf("%-4s ", "");
          break;
        case o_REQUIRED_VALUE:
          zprintf("%-4s ", "req");
          break;
        case o_OPTIONAL_VALUE:
          zprintf("%-4s ", "opt");
          break;
        case o_VALUE_LIST:
          zprintf("%-4s ", "list");
          break;
        case o_ONE_CHAR_VALUE:
          zprintf("%-4s ", "char");
          break;
        case o_NUMBER_VALUE:
          zprintf("%-4s ", "num");
          break;
        case o_OPT_EQ_VALUE:
          zprintf("%-4s ", "=val");
          break;
        default:
          zprintf("%-4s ", "unk");
      }
      switch (options[i].negatable) {
        case o_NEGATABLE:
          zprintf("%-3s ", "neg");
          break;
        case o_NOT_NEGATABLE:
          zprintf("%-3s ", "");
          break;
        default:
          zprintf("%-3s ", "unk");
      }
      if (options[i].name) {
        zprintf("%-30s\n", options[i].name);
      }
      else
        zprintf("\n");
    }
    RETURN(finish(ZE_OK));
  }


#ifdef WINDLL
  if (zipfile == NULL) {
    ZIPERR(ZE_PARMS, "DLL can't Zip from/to stdin/stdout");
  }
#endif

#ifdef CHANGE_DIRECTORY
  /* change dir
     Change from startup_dir to user given working_dir.  Restoring
     the startup directory is done in freeup() (if needed). */
  if (working_dir) {

# ifdef UNICODE_SUPPORT_WIN32

    int working_dir_utf8 = is_utf8_string(working_dir, NULL, NULL, NULL, NULL);

    /* save startup dir */
    if (startup_dirw)
      free(startup_dirw);
    if ((startup_dirw = _wgetcwd(NULL, 0)) == NULL) {
      sprintf(errbuf, "saving dir: %S\n  %s", startup_dirw, strerror(errno));
      free(working_dir);
      working_dir = NULL;
      ZIPERR(ZE_PARMS, errbuf);
    }

    /* change to working dir */
    if (working_dir_utf8) {
      /* UTF-8 working_dir */
      working_dirw = utf8_to_wchar_string(working_dir);
      if (working_dirw == NULL) {
        ZIPERR(ZE_PARMS, "converting working_dir to wide");
      }
      if (_wchdir(working_dirw)) {
        sprintf(errbuf, "changing to dir: %S\n  %s", working_dirw, strerror(errno));
        free(working_dirw);
        working_dirw = NULL;
        free(working_dir);
        working_dir = NULL;
        ZIPERR(ZE_PARMS, errbuf);
      }
    }

    else
    {
      /* local charset working_dir */
      if (CHDIR(working_dir)) {
        sprintf(errbuf, "changing to dir: %s\n  %s", working_dir, strerror(errno));
        free(working_dir);
        working_dir = NULL;
        ZIPERR(ZE_PARMS, errbuf);
      }
    }

    if (noisy) {
      wchar_t *current_dirw = NULL;
      
      if ((current_dirw = _wgetcwd(NULL, 0)) == NULL) {
        sprintf(errbuf, "getting current dir: %s", strerror(errno));
        ZIPERR(ZE_PARMS, errbuf);
      }
      sprintf(errbuf, "current directory set to: %S", current_dirw);
      zipmessage(errbuf, "");
      free(current_dirw);
    }

# else /* not UNICODE_SUPPORT_WIN32 */
    /* save startup dir */
    if (startup_dir)
      free(startup_dir);
    if ((startup_dir = GETCWD(NULL, 0)) == NULL) {
      sprintf(errbuf, "saving dir: %s\n  %s", startup_dir, strerror(errno));
      free(working_dir);
      working_dir = NULL;
      ZIPERR(ZE_PARMS, errbuf);
    }

    /* change to working dir */
    if (CHDIR(working_dir)) {
      sprintf(errbuf, "changing to dir: %s\n  %s", working_dir, strerror(errno));
      free(working_dir);
      working_dir = NULL;
      ZIPERR(ZE_PARMS, errbuf);
    }

    if (noisy) {
      char *current_dir = NULL;
      
      if ((current_dir = GETCWD(NULL, 0)) == NULL) {
        sprintf(errbuf, "getting current dir: %s", strerror(errno));
        ZIPERR(ZE_PARMS, errbuf);
      }
      sprintf(errbuf, "current directory set to: %s", current_dir);
      zipmessage(errbuf, "");
      free(current_dir);
    }
# endif /* def UNICODE_SUPPORT_WIN32 else */

  }
#endif /* CHANGE_DIRECTORY */

  
#ifdef BACKUP_SUPPORT

  /* ----------------------------------------- */
  /* Backup */

  /* Set up for -BT backup using control file */
  if (backup_type && fix) {
    ZIPERR(ZE_PARMS, "can't use -F or -FF with -BT");
  }
  if (have_out && (backup_type)) {
    ZIPERR(ZE_PARMS, "can't specify output file when using -BT");
  }
  if (backup_type && (backup_dir == NULL)) {
    ZIPERR(ZE_PARMS, "-BT without -BD");
  }
  if (backup_type && (backup_name == NULL)) {
    ZIPERR(ZE_PARMS, "-BT without -BN");
  }
  if ((backup_dir != NULL) && (!backup_type)) {
    ZIPERR(ZE_PARMS, "-BD without -BT");
  }
  if ((backup_name != NULL) && (!backup_type)) {
    ZIPERR(ZE_PARMS, "-BN without -BT");
  }
  if ((backup_control_dir != NULL) && (!backup_type)) {
    ZIPERR(ZE_PARMS, "-BC without -BT");
  }
  if ((backup_log_dir != NULL) && (!backup_type)) {
    ZIPERR(ZE_PARMS, "-BL without -BT");
  }

  if (backup_log_dir && (logfile_path || use_outpath_for_log)) {
    ZIPERR(ZE_PARMS, "Can't use -BL with -lf or -lF");
  }

  if (backup_type) {
    /* control dir defaults to backup dir */
    if (backup_control_dir == NULL) {
      if ((backup_control_dir = malloc(strlen(backup_dir) + 1)) == NULL) {
        ZIPERR(ZE_MEM, "backup control path (1a)");
      }
      strcpy(backup_control_dir, backup_dir);
    }
    /* build control file path */
    i = (int)(strlen(backup_control_dir) + 1 + strlen(backup_name) + 1 +
        strlen(BACKUP_CONTROL_FILE_NAME) + 1);
    if (backup_control_path)
      free(backup_control_path);
    if ((backup_control_path = malloc(i)) == NULL) {
      ZIPERR(ZE_MEM, "backup control path (1b)");
    }
    strcpy(backup_control_path, backup_control_dir);
# ifndef VMS
    strcat(backup_control_path, "/");
# endif
    strcat(backup_control_path, backup_name);
    strcat(backup_control_path, "_");
    strcat(backup_control_path, BACKUP_CONTROL_FILE_NAME);

    free(backup_control_dir);
    backup_control_dir = NULL;
  }

  if (backup_type == BACKUP_DIFF || backup_type == BACKUP_INCR) {
    /* read backup control file to get list of archives to read */
    int i;
    int j;
    char prefix[7];
    FILE *fcontrolfile = NULL;
    char *linebuf = NULL;

    /* see if we can open this path */
    if ((fcontrolfile = zfopen(backup_control_path, "r")) == NULL) {
      sprintf(errbuf, "could not open control file: %s",
              backup_control_path);
      ZIPERR(ZE_OPEN, errbuf);
    }

    /* read backup control file */
    if ((linebuf = (char *)malloc((FNMAX + 101) * sizeof(char))) == NULL) {
      ZIPERR(ZE_MEM, "backup line buf");
    }
    i = 0;
    while (fgets(linebuf, FNMAX + 10, fcontrolfile) != NULL) {
      int baselen;
      char ext[6];
      int colon;
      int prefix_len;
      int linebuf_len;
      if (strlen(linebuf) >= FNMAX + 9) {
        sprintf(errbuf, "archive path at control file line %d truncated", i + 1);
        zipwarn(errbuf, "");
      }
      baselen = (int)strlen(linebuf);
      if (linebuf[baselen - 1] == '\n') {
        linebuf[baselen - 1] = '\0';
      }
      linebuf_len = (int)strlen(linebuf);
      if (linebuf_len < 7) {
        sprintf(errbuf, "improper line in backup control file at line %d", i);
        ZIPERR(ZE_BACKUP, errbuf);
      }
      for (colon = 0; linebuf[colon]; colon++)
        if (linebuf[colon] == ':') break;

      if (colon == linebuf_len) {
        sprintf(errbuf, "improper line in backup control file at line %d", i);
        ZIPERR(ZE_BACKUP, errbuf);
      }
      strcpy(prefix, linebuf);
      prefix_len = colon + 2;
      if (prefix_len < linebuf_len) {
        sprintf(errbuf, "improper line in backup control file at line %d", i);
        ZIPERR(ZE_BACKUP, errbuf);
      }
      prefix[prefix_len] = '\0';
      for (j = 0; j < prefix_len; j++) prefix[j] = tolower(prefix[j]);

      if (strmatch(prefix, "full: ", CASE_INS, ENTIRE_STRING)) {
        /* full backup path */
        if (backup_full_path != NULL) {
          ZIPERR(ZE_BACKUP, "more than one full backup entry in control file");
        }
        if ((backup_full_path = (char *)malloc(strlen(linebuf) + 1)) == NULL) {
          ZIPERR(ZE_MEM, "backup archive path (1)");
        }
        for (j = prefix_len; linebuf[j]; j++) backup_full_path[j - prefix_len] = linebuf[j];
        backup_full_path[j - prefix_len] = '\0';
        baselen = (int)strlen(backup_full_path) - 4;
        for (j = baselen; backup_full_path[j]; j++) ext[j - baselen] = tolower(backup_full_path[j]);
        ext[4] = '\0';
        if (!strmatch(ext, ".zip", file_system_case_sensitive, ENTIRE_STRING)) {
          strcat(backup_full_path, ".zip");
        }

      } else if (strmatch(prefix, "diff: ", CASE_INS, ENTIRE_STRING) ||
                 strmatch(prefix, "incr: ", CASE_INS, ENTIRE_STRING)) {
        /* path of diff or incr archive to be read for incremental backup */
        if (backup_type == BACKUP_INCR) {
          char *path;
          if ((path = (char *)malloc(strlen(linebuf))) == NULL) {
            ZIPERR(ZE_MEM, "backup archive path (2)");
          }
          for (j = prefix_len; linebuf[j]; j++) path[j - prefix_len] = linebuf[j];
          path[j - prefix_len] = '\0';
          baselen = (int)strlen(path) - 4;
          for (j = baselen; path[j]; j++) ext[j - baselen] = tolower(path[j]);
          ext[4] = '\0';
          if (!strmatch(ext, ".zip", file_system_case_sensitive, ENTIRE_STRING)) {
            strcat(path, ".zip");
          }
          add_apath(path);
          free(path);
        }
      }
      i++;
    }
    fclose(fcontrolfile);
  }

  if ((backup_type == BACKUP_DIFF || backup_type == BACKUP_INCR) && backup_full_path == NULL) {
    zipwarn("-BT set to DIFF or INCR but no baseline full archive found", "");
    zipwarn("check control file or perform a FULL backup first to create baseline", "");
    ZIPERR(ZE_BACKUP, "no full backup archive specified or found");
  }

  if (backup_type) {
    /* build output path */
    struct tm *now;
    time_t clocktime;

    /* get current time */
    if ((backup_start_datetime = malloc(20)) == NULL) {
      ZIPERR(ZE_MEM, "backup_start_datetime");
    }
    time(&clocktime);
    now = localtime(&clocktime);
    sprintf(backup_start_datetime, "%04d%02d%02d_%02d%02d%02d",
      now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour,
      now->tm_min, now->tm_sec);

    i = (int)(strlen(backup_dir) + 1 + strlen(backup_name) + 1);
    if ((backup_output_path = malloc(i + 290)) == NULL) {
      ZIPERR(ZE_MEM, "backup control path (2)");
    }

    if (backup_type == BACKUP_FULL) {
      /* path of full archive */
      strcpy(backup_output_path, backup_dir);
# ifndef VMS
      strcat(backup_output_path, "/");
# endif
      strcat(backup_output_path, backup_name);
      strcat(backup_output_path, "_full_");
      strcat(backup_output_path, backup_start_datetime);
      strcat(backup_output_path, ".zip");
      if ((backup_full_path = malloc(strlen(backup_output_path) + 1)) == NULL) {
        ZIPERR(ZE_MEM, "backup full path");
      }
      strcpy(backup_full_path, backup_output_path);

    } else if (backup_type == BACKUP_DIFF) {
      /* path of diff archive */
      strcpy(backup_output_path, backup_full_path);
      /* chop off .zip */
      backup_output_path[strlen(backup_output_path) - 4] = '\0';
      strcat(backup_output_path, "_diff_");
      strcat(backup_output_path, backup_start_datetime);
      strcat(backup_output_path, ".zip");

    } else if (backup_type == BACKUP_INCR) {
      /* path of incr archvie */
      strcpy(backup_output_path, backup_full_path);
      /* chop off .zip */
      backup_output_path[strlen(backup_output_path) - 4] = '\0';
      strcat(backup_output_path, "_incr_");
      strcat(backup_output_path, backup_start_datetime);
      strcat(backup_output_path, ".zip");
    }

    if (backup_type == BACKUP_DIFF || backup_type == BACKUP_INCR) {
      /* make sure full backup exists */
      FILE *ffull = NULL;

      if (backup_full_path == NULL) {
        ZIPERR(ZE_BACKUP, "no full backup path in control file");
      }
      if ((ffull = zfopen(backup_full_path, "r")) == NULL) {
        zipwarn("Can't open:  ", backup_full_path);
        ZIPERR(ZE_OPEN, "full backup can't be opened");
      }
      fclose(ffull);
    }

    zipfile = ziptyp(backup_full_path);
    if (show_what_doing) {
      sprintf(errbuf, "sd: Zipfile name '%s'", zipfile);
      zipmessage(errbuf, "");
    }
    if ((in_path = malloc(strlen(zipfile) + 1)) == NULL) {
      ZIPERR(ZE_MEM, "was processing arguments (6)");
    }
    strcpy(in_path, zipfile);

    out_path = ziptyp(backup_output_path);

    zipmessage("Backup output path: ", out_path);
  }


  if (backup_log_dir) {
    if (strlen(backup_log_dir) == 0)
    {
      /* empty backup log path - default to archive path */
      int i;
      i = (int)strlen(out_path);
      /* remove .zip and replace with .log */
      if ((logfile_path = malloc(i + 1)) == NULL) {
        ZIPERR(ZE_MEM, "backup path (1)");
      }
      strcpy(logfile_path, out_path);
      strcpy(logfile_path + i - 4, ".log");
    }
    else
    {
      /* use given path for backup log */
      char *p;
      int i;

      /* find start of archive name */
      for (p = out_path + strlen(out_path); p >= out_path; p--) {
        if (*p == '/' || *p == '\\')
          break;
      }
      if (p > out_path) {
        /* found / or \ - skip it */
        p++;
      } else {
        /* use entire out_path */
      }
      i = (int)(strlen(backup_log_dir) + 1 + strlen(p) + 1);
      if ((logfile_path = malloc(i)) == NULL) {
        ZIPERR(ZE_MEM, "backup path (2)");
      }
      strcpy(logfile_path, backup_log_dir);
# ifndef VMS
      strcat(logfile_path, "/");
# endif
      strcat(logfile_path, p);
      strcpy(logfile_path + i - 4, ".log");
    }

    free(backup_log_dir);
    backup_log_dir = NULL;
  }

  /* ----------------------------------------- */

#endif /* BACKUP_SUPPORT */


  if (use_outpath_for_log) {
    if (logfile_path) {
      zipwarn("both -lf and -lF used, -lF ignored", "");
    } else {
      int out_path_len;

      if (out_path == NULL) {
        ZIPERR(ZE_PARMS, "-lF but no out path");
      }
      out_path_len = (int)strlen(out_path);
      if ((logfile_path = malloc(out_path_len + 5)) == NULL) {
        ZIPERR(ZE_MEM, "logfile path");
      }
      if (out_path_len > 4) {
        char *ext = out_path + out_path_len - 4;
        if (STRCASECMP(ext, ".zip") == 0) {
          /* remove .zip before adding .log */
          strncpy(logfile_path, out_path, out_path_len - 4);
          logfile_path[out_path_len - 4] = '\0';
        } else {
          /* just append .log */
          strcpy(logfile_path, out_path);
        }
      } else {
        strcpy(logfile_path, out_path);
      }
      strcat(logfile_path, ".log");
    }
  }


  /* open log file */
  if (logfile_path) {
    char mode[10];
    char *p;
    char *lastp;

    if (strlen(logfile_path) == 1 && logfile_path[0] == '-') {
      /* log to stdout */
      if (zip_to_stdout) {
        ZIPERR(ZE_PARMS, "cannot send both zip and log output to stdout");
      }
      if (logfile_append) {
        ZIPERR(ZE_PARMS, "cannot use -la when logging to stdout");
      }
      if (verbose) {
        ZIPERR(ZE_PARMS, "cannot use -v when logging to stdout");
      }
      /* to avoid duplicate output, turn off normal messages to stdout */
      noisy = 0;
      /* send output to stdout */
      logfile = stdout;

    } else {
      /* not stdout */
      /* if no extension add .log */
      p = logfile_path;
      /* find last / */
      lastp = NULL;
      for (p = logfile_path; (p = MBSRCHR(p, '/')) != NULL; p++) {
        lastp = p;
      }
      if (lastp == NULL)
        lastp = logfile_path;
      if (MBSRCHR(lastp, '.') == NULL) {
        /* add .log */
        if ((p = malloc(strlen(logfile_path) + 5)) == NULL) {
          ZIPERR(ZE_MEM, "logpath");
        }
        strcpy(p, logfile_path);
        strcat(p, ".log");
        free(logfile_path);
        logfile_path = p;
      }

      if (logfile_append) {
        sprintf(mode, "a");
      } else {
        sprintf(mode, "w");
      }
      if ((logfile = zfopen(logfile_path, mode)) == NULL) {
        sprintf(errbuf, "could not open logfile '%s'", logfile_path);
        ZIPERR(ZE_PARMS, errbuf);
      }
    }
    if (logfile != stdout) {
      /* At top put start time and command line */

      /* get current time */
      struct tm *now;

      time(&clocktime);
      now = localtime(&clocktime);

      zfprintf(logfile, "---------\n");
      zfprintf(logfile, "Zip log opened %s", asctime(now));
      zfprintf(logfile, "command line arguments:\n ");
      for (i = 1; args[i]; i++) {
        size_t j;
        int has_space = 0;

        for (j = 0; j < strlen(args[i]); j++) {
          if (isspace(args[i][j])) {
            has_space = 1;
            break;
          }
        }
        if (has_space) {
          zfprintf(logfile, "\"%s\" ", args[i]);
        }
        else {
          zfprintf(logfile, "%s ", args[i]);
        }
      }
      zfprintf(logfile, "\n\n");
      fflush(logfile);
    }
  } else {
    /* only set logall if logfile open */
    logall = 0;
  }


  /* Show process ID. */
#if !defined( VMS) && defined( ENABLE_USER_PROGRESS)
  if (show_pid)
  {
    fprintf( mesg, "PID = %d \n", getpid());
  }
#endif /* !defined( VMS) && defined( ENABLE_USER_PROGRESS) */


  if (show_what_doing) {
    if (in_path) {
      sprintf(errbuf, "sd: in_path  '%s'", in_path);
      zipmessage(errbuf, "");
    }
    if (out_path) {
      sprintf(errbuf, "sd: out_path '%s'", out_path);
      zipmessage(errbuf, "");
    }
  }


  /* process command line options */


#ifdef IZ_CRYPT_ANY

# if defined( IZ_CRYPT_AES_WG) || defined( IZ_CRYPT_AES_WG_NEW)
  if ((key == NULL) && (encryption_method != NO_ENCRYPTION)) {
    key_needed = 1;
  }
# endif /* defined( IZ_CRYPT_AES_WG) || defined( IZ_CRYPT_AES_WG_NEW) */

  /* ----------------- */
  if (noisy) {
    if (encryption_method == TRADITIONAL_ENCRYPTION) {
      zipmessage("Using traditional (weak ZipCrypto) zip encryption", "");
    } else if (encryption_method == AES_128_ENCRYPTION) {
      zipmessage("Using AES 128 encryption", "");
    } else if (encryption_method == AES_192_ENCRYPTION) {
      zipmessage("Using AES 192 encryption", "");
    } else if (encryption_method == AES_256_ENCRYPTION) {
      zipmessage("Using AES 256 encryption", "");
    }
  }

  /* read keyfile */
  if (keyfile) {
    /* open file and read first 128 (non-NULL) bytes */
    FILE *kf;
    char keyfilebuf[130];
    int count;
    int c;

    if ((kf = zfopen(keyfile, "rb")) == NULL) {
      sprintf(errbuf, "Could not open keyfile:  %s", keyfile);
      free(keyfile);
      ZIPERR(ZE_OPEN, errbuf);
    }
    count = 0;
    while (count < 128) {
      c = fgetc(kf);
      if (c == EOF) {
        if (ferror(kf)) {
          sprintf("error reading keyfile %s:  %s", keyfile, strerror(errno));
          ZIPERR(ZE_READ, errbuf);
        }
        break;
      }
      if (c != '\0') {
        /* Skip any NULL bytes to allow easy copying of string */
        keyfilebuf[count++] = c;
      }
    } /* while */
    fclose(kf);

    keyfile_pass = NULL;
    keyfilebuf[count] = '\0';
    if (count > 0) {
      keyfile_pass = string_dup(keyfilebuf, "read keyfile", 0);
      zipmessage("Keyfile read", "");
    }
  } /* keyfile */

  if (!show_files) {
    if (key_needed) {
      int i;

# ifdef IZ_CRYPT_AES_WG_NEW
      /* may be pulled from the new code later */
#  define MAX_PWD_LENGTH 128
# endif /* def IZ_CRYPT_AES_WG_NEW */

# if defined(IZ_CRYPT_AES_WG) || defined(IZ_CRYPT_AES_WG_NEW)
#  define MAX_PWLEN temp_pwlen
      int temp_pwlen;

      if (encryption_method <= TRADITIONAL_ENCRYPTION)
          MAX_PWLEN = IZ_PWLEN;
      else
          MAX_PWLEN = MAX_PWD_LENGTH;
# else
#  define MAX_PWLEN IZ_PWLEN
# endif

      if ((key = malloc(MAX_PWLEN+2)) == NULL) {
        ZIPERR(ZE_MEM, "was getting encryption password (1)");
      }
      r = simple_encr_passwd(ZP_PW_ENTER, key, MAX_PWLEN+1);
      if (r == -1) {
        sprintf(errbuf, "password too long - max %d", MAX_PWLEN);
        ZIPERR(ZE_CRYPT, errbuf);
      }
      if (*key == '\0') {
        ZIPERR(ZE_CRYPT, "zero length password not allowed");
      }
      if (force_ansi_key) {
        for (i = 0; key[i]; i++) {
          if (key[i] < 32 || key[i] > 126) {
            zipwarn("password must be ANSI 7-bit (unless use -pn)", "");
            ZIPERR(ZE_CRYPT, "non-ANSI character in password");
          }
        }
      }

      if ((encryption_method == AES_256_ENCRYPTION) &&
          (strlen(key) < AES_256_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars",
                          AES_256_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES256 password must be at least %d chars (longer is better)",
                  AES_256_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_CRYPT, "AES256 password too short");
        }
      }
      if ((encryption_method == AES_192_ENCRYPTION) &&
          (strlen(key) < AES_192_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars",
                          AES_192_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES192 password must be at least %d chars (longer is better)",
                  AES_192_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_CRYPT, "AES192 password too short");
        }
      }
      if ((encryption_method == AES_128_ENCRYPTION) &&
          (strlen(key) < AES_128_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars",
                          AES_128_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES128 password must be at least %d chars (longer is better)",
                  AES_128_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_CRYPT, "AES128 password too short");
        }
      }
    
      if ((e = malloc(MAX_PWLEN+2)) == NULL) {
        ZIPERR(ZE_MEM, "was verifying encryption password (1)");
      }
#ifndef ZIP_DLL_LIB
      /* Only verify password (prompt user again) if not getting it from a callback. */
      r = simple_encr_passwd(ZP_PW_VERIFY, e, MAX_PWLEN+1);
      r = strcmp(key, e);
      free((zvoid *)e);
      if (r) {
        ZIPERR(ZE_CRYPT, "password verification failed");
      }
#endif
    } /* key_needed */

    if (key) {
      passwd = string_dup(key, "passwd", 0);
    }

    /* merge password and keyfile */
    if (keyfile_pass) {
      char newkey[130];
      int keyfile_bytes_needed;

      newkey[0] = '\0';

      if (key) {
        /* copy entered password to front of newkey buffer */
        strcpy(newkey, key);
        free(key);
      }

      /* copy keyfile bytes to fill to max password length of 128 bytes */
      keyfile_bytes_needed = 128 - (int)strlen(newkey);
      if (keyfile_bytes_needed < (int)strlen(keyfile_pass)) {
        keyfile_pass[keyfile_bytes_needed] = '\0';
      }
      strcat(newkey, keyfile_pass);
      key = string_dup(newkey, "merge keyfile", 0);
    } /* merge password and keyfile */

    if (key) {
      /* if -P "" could get here */
      if (*key == '\0') {
        ZIPERR(ZE_CRYPT, "zero length password not allowed");
      }
    }

    /* Need to repeat these checks on password + keyfile merged key. */
    if (keyfile) {
      if ((encryption_method == AES_256_ENCRYPTION) &&
          (strlen(key) < AES_256_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars",
                          AES_256_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES256 password must be at least %d chars (longer is better)",
                  AES_256_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_CRYPT, "AES256 password too short");
        }
      }
      if ((encryption_method == AES_192_ENCRYPTION) &&
          (strlen(key) < AES_192_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars",
                          AES_192_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES192 password must be at least %d chars (longer is better)",
                  AES_192_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_CRYPT, "AES192 password too short");
        }
      }
      if ((encryption_method == AES_128_ENCRYPTION) &&
          (strlen(key) < AES_128_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars",
                          AES_128_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES128 password must be at least %d chars (longer is better)",
                  AES_128_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_CRYPT, "AES128 password too short");
        }
      }
    } /* keyfile */
  } /* !show_files */
  /* ----------------- */
#endif /* def IZ_CRYPT_ANY */

  /* Check path prefix */
  if (path_prefix) {
    int i;
    char last_c = '\0';
    char c;
    char *allowed_other_chars = ALLOWED_PREFIX_CHARS;

    for (i = 0; (c = path_prefix[i]); i++) {
      if (!isprint(c)) {
        if (path_prefix_mode == 1)
          ZIPERR(ZE_PARMS, "option -pa (--prefix_added): non-print char in prefix");
        else
          ZIPERR(ZE_PARMS, "option -pp (--prefix_path): non-print char in prefix");
      }
#if 0
      /* backslashes should never appear in a prefix */
#if (defined(MSDOS) || defined(OS2)) && !defined(WIN32)
      if (c == '\\') {
        c = '/';
        path_prefix[i] = c;
      }
#endif
#endif
      if (!isalnum(c) && !strchr(allowed_other_chars, c)) {
        if (path_prefix_mode == 1)
          strcpy(errbuf, "option -pa (--prefix_added), only alphanum and \"");
        else
          strcpy(errbuf, "option -pp (--prefix_path), only alphanum and \"");
        strcat(errbuf, allowed_other_chars);
        strcat(errbuf, "\" allowed");
        ZIPERR(ZE_PARMS, errbuf);
      }
      if (last_c == '.' && c == '.') {
        if (path_prefix_mode == 1)
          ZIPERR(ZE_PARMS, "option -pa (--prefix_added): \"..\" not allowed");
        else
          ZIPERR(ZE_PARMS, "option -pp (--prefix_path): \"..\" not allowed");
      }
      last_c = c;
    }
  }

  /* Check stdin name */
  if (stdin_name) {
    int i;
    char last_c = '\0';
    char c;
    char *allowed_other_chars = ALLOWED_PREFIX_CHARS;

    for (i = 0; (c = stdin_name[i]); i++) {
      if (!isprint(c)) {
        ZIPERR(ZE_PARMS, "option -SI (--name-stdin): non-print char in name");
      }
      if (!isalnum(c) && !strchr(allowed_other_chars, c)) {
        strcpy(errbuf, "option -SI (--name_stdin), only alphanum and \"");
        strcat(errbuf, allowed_other_chars);
        strcat(errbuf, "\" allowed");
        ZIPERR(ZE_PARMS, errbuf);
      }
      if (last_c == '.' && c == '.') {
        ZIPERR(ZE_PARMS, "option -SI (--name_stdin): \"..\" not allowed");
      }
      last_c = c;
    }
  }


  /* -TT and -TU */
  if (unzip_string && unzip_path) {
    ZIPERR(ZE_PARMS, "-TT and -TU can't both be specified");
  }
  /* -TT and -TV */
  if (unzip_string && unzip_verbose) {
    ZIPERR(ZE_PARMS, "-TV can't be used with -TT");
  }


  if (split_method && out_path) {
    /* if splitting, the archive name must have .zip extension */
    int plen = (int)strlen(out_path);
    char *out_path_ext;

#ifdef VMS
    /* On VMS, adjust plen (and out_path_ext) to avoid the file version. */
    plen -= strlen( vms_file_version( out_path));
#endif /* def VMS */
    out_path_ext = out_path+ plen- 4;

    if (plen < 4 ||
        out_path_ext[0] != '.' ||
        toupper(out_path_ext[1]) != 'Z' ||
        toupper(out_path_ext[2]) != 'I' ||
        toupper(out_path_ext[3]) != 'P') {
      ZIPERR(ZE_PARMS, "archive name must end in .zip for splits");
    }
  }


  if (verbose && (dot_size == 0) && (dot_count == 0)) {
    /* now default to default 10 MiB dot size */
    dot_size = 10 * 0x100000;
    /* show all dots as before if verbose set and dot_size not set (dot_count = 0) */
    /* maybe should turn off dots in default verbose mode */
    /* dot_size = -1; */
  }

  /* done getting -R filters so convert filterlist if not done */
  if (pcount && patterns == NULL) {
    filterlist_to_patterns();
  }

#if (defined(MSDOS) || defined(OS2)) && !defined(WIN32)
  if ((kk == 3 || kk == 4) && volume_label == 1) {
    /* read volume label */
    PROCNAME(NULL);
    kk = 4;
  }
#endif

  if (comadd && show_files) {
    ZIPERR(ZE_PARMS, "can't use -c (comments) with show files (-sf/-su/-sU)");
  }

  if (comadd && kk == 3) {
    /* No input files, so select archive entries to update comments for. */
    action = ARCHIVE;
  }

  if (have_out && kk == 3) {
    copy_only = 1;
    action = ARCHIVE;
  }

  if (show_files && action != ARCHIVE && kk == 3) {
    action = ARCHIVE;
  }

#ifdef CD_ONLY_SUPPORT
  if (method == CD_ONLY) {
    if (!copy_only) {
      ZIPERR(ZE_PARMS, "cd_only compression only available in copy mode");
    }
    cd_only = 1;
  }
#endif
  
  if (have_out && namecmp(in_path, out_path) == 0) {
    sprintf(errbuf, "--out path must be different than in path: %s", out_path);
    ZIPERR(ZE_PARMS, errbuf);
  }

  if (fix && diff_mode) {
    ZIPERR(ZE_PARMS, "can't use --diff (-DF) with fix (-F or -FF)");
  }

  if (action == ARCHIVE && !have_out && !show_files && !comadd) {
    ZIPERR(ZE_PARMS, "-U (--copy) requires -O (--out)");
  }

  if (fix && !have_out) {
    zipwarn("fix options -F and -FF require --out:\n",
            "                     zip -F indamagedarchive --out outfixedarchive");
    ZIPERR(ZE_PARMS, "fix options require --out");
  }

  if (fix && !copy_only) {
    ZIPERR(ZE_PARMS, "no other actions allowed when fixing archive (-F or -FF)");
  }

#ifdef BACKUP_SUPPORT
  if (!have_out && diff_mode && !backup_type) {
    ZIPERR(ZE_PARMS, "-DF (--diff) requires -O (--out)");
  }
#endif

  if (kk < 3 && diff_mode == 1) {
    ZIPERR(ZE_PARMS, "-DF (--diff) requires an input archive\n");
  }
  if (kk < 3 && diff_mode == 2) {
    ZIPERR(ZE_PARMS, "-DI (--incr) requires an input archive\n");
  }

  if (diff_mode && (action == ARCHIVE || action == DELETE)) {
    ZIPERR(ZE_PARMS, "can't use --diff (-DF) with -d or -U");
  }

  if (action != ARCHIVE && (recurse == 2 || pcount) && first_listarg == 0 &&
      !filelist && (kk < 3 || (action != UPDATE && action != FRESHEN))) {
    ZIPERR(ZE_PARMS, "nothing to select from");
  }

/*
  -------------------------------------
  end of new command line code
  -------------------------------------
*/





  /* Check option combinations */

  if (action == DELETE && (method != BEST || dispose || recurse ||
      key != NULL || comadd || zipedit)) {
    zipwarn("invalid option(s) used with -d; ignored.","");
    /* reset flags - needed? */
    method  = BEST;
    dispose = 0;
    recurse = 0;
    if (key != NULL) {
      free((zvoid *)key);
      key   = NULL;
    }
    comadd  = 0;
    zipedit = 0;
  }
  if (action == ARCHIVE &&
      ((method != BEST && method != CD_ONLY) || dispose || recurse /* || comadd || zipedit */)) {
    zipwarn("can't set method, move, or recurse with copy mode","");
    /* reset flags - needed? */
    method  = BEST;
    dispose = 0;
    recurse = 0;
    comadd  = 0;
    zipedit = 0;
  }
  if (linkput && dosify)
    {
      zipwarn("can't use -y with -k, -y ignored", "");
      linkput = 0;
    }
  if (fix == 1 && adjust)
    {
      zipwarn("can't use -F with -A, -F ignored", "");
      fix = 0;
    }
  if (fix == 2 && adjust)
    {
      zipwarn("can't use -FF with -A, -FF ignored", "");
      fix = 0;
    }

  if (split_method && (fix || adjust)) {
    ZIPERR(ZE_PARMS, "can't create split archive while fixing or adjusting\n");
  }

#if defined(EBCDIC)  && !defined(ZOS_UNIX)
  if (aflag==FT_ASCII_TXT && !translate_eol) {
    /* Translation to ASCII implies EOL translation!
     * (on z/OS, consistent EOL translation is controlled separately)
     * The default translation mode is "UNIX" mode (single LF terminators).
     */
    translate_eol = 2;
  }
#endif
#ifdef CMS_MVS
  if (aflag==FT_ASCII_TXT && bflag)
    ZIPERR(ZE_PARMS, "can't use -a with -B");
#endif
#ifdef UNIX_APPLE
  if (data_fork_only && sequester)
  {
    zipwarn("can't use -as with -df, -as ignored", "");
    sequester = 0;
  }
  if (sort_apple_double_all && sequester)
  {
    zipwarn("can't use -as with -ad, -ad ignored", "");
    sort_apple_double_all = 0;
  }
#endif

#ifdef VMS
  if (!extra_fields && vms_native) {
    zipwarn("can't use -V with -X-, -V ignored", "");
    vms_native = 0;
  }
  if (vms_native && translate_eol)
    ZIPERR(ZE_PARMS, "can't use -V with -l or -ll");
#endif

#if (!defined(MACOS))
  if (kk < 3) {               /* zip used as filter */
# ifdef WINDLL
    ZIPERR(ZE_PARMS, "DLL can't input/output to stdin/stdout");
# else
    zipstdout();
    comment_stream = NULL;

    if (s) {
      ZIPERR(ZE_PARMS, "can't use - (stdin) and -@ together");
    }

    /* Add stdin ("-") to the member list. */
    if ((r = procname("-", 0)) != ZE_OK) {
      if (r == ZE_MISS) {
        if (bad_open_is_error) {
          zipwarn("name not matched: ", "-");
          ZIPERR(ZE_OPEN, "-");
        } else {
          zipwarn("name not matched: ", "-");
        }
      } else {
        ZIPERR(r, "-");
      }
    }

    /* Specify stdout ("-") as the archive. */
    /* Unreached/unreachable message? */
    if (isatty(1))
      ziperr(ZE_PARMS, "cannot write zip file to terminal");
    if ((out_path = malloc(4)) == NULL)
      ziperr(ZE_MEM, "was processing arguments for stdout");
    strcpy( out_path, "-");
# endif

    kk = 4;
  }
#endif /* !MACOS */

  if (zipfile && !strcmp(zipfile, "-")) {
    if (show_what_doing) {
      zipmessage("sd: Zipping to stdout", "");
    }
    zip_to_stdout = 1;
  }

  if (test && zip_to_stdout) {
    test = 0;
    zipwarn("can't use -T on stdout, -T ignored", "");
  }
  if (split_method && (d || zip_to_stdout)) {
    ZIPERR(ZE_PARMS, "can't create split archive with -d or -g or on stdout");
  }
  if ((action != ADD || d) && zip_to_stdout) {
    ZIPERR(ZE_PARMS, "can't use -d, -f, -u, -U, or -g on stdout");
  }
  if ((action != ADD || d) && filesync) {
    ZIPERR(ZE_PARMS, "can't use -d, -f, -u, -U, or -g with filesync -FS");
  }
  if (kk == 3 && filesync) {
    ZIPERR(ZE_PARMS, "filesync -FS requires same input as used to create archive");
  }


  if (noisy) {
    if (fix == 1)
      zipmessage("Fix archive (-F) - assume mostly intact archive", "");
    else if (fix == 2)
      zipmessage("Fix archive (-FF) - salvage what can", "");
  }



  /* ----------------------------------------------- */


  /* If -FF we do it all here */
  if (fix == 2) {

    /* Open zip file and temporary output file */
    if (show_what_doing) {
      zipmessage("sd: Open zip file and create temp file (-FF)", "");
    }
    diag("opening zip file and creating temporary zip file");
    x = NULL;
    tempzn = 0;
    if (show_what_doing) {
      zipmessage("sd: Creating new zip file (-FF)", "");
    }
#if defined(UNIX) && !defined(NO_MKSTEMP)
    {
      int yd;
      int i;

      /* use mkstemp to avoid race condition and compiler warning */

      if (tempath != NULL)
      {
        /* if -b used to set temp file dir use that for split temp */
        if ((tempzip = malloc(strlen(tempath) + 12)) == NULL) {
          ZIPERR(ZE_MEM, "allocating temp filename (1)");
        }
        strcpy(tempzip, tempath);
        if (lastchar(tempzip) != '/')
          strcat(tempzip, "/");
      }
      else
      {
        /* create path by stripping name and appending template */
        if ((tempzip = malloc(strlen(out_path) + 12)) == NULL) {
        ZIPERR(ZE_MEM, "allocating temp filename (2)");
        }
        strcpy(tempzip, out_path);
        for (i = strlen(tempzip); i > 0; i--) {
          if (tempzip[i - 1] == '/')
            break;
        }
        tempzip[i] = '\0';
      }
      strcat(tempzip, "ziXXXXXX");

      if ((yd = mkstemp(tempzip)) == EOF) {
        ZIPERR(ZE_TEMP, tempzip);
      }
      if (show_what_doing) {
        sprintf(errbuf, "sd: Temp file (1u): %s", tempzip);
        zipmessage(errbuf, "");
      }
      if ((y = fdopen(yd, FOPW_TMP)) == NULL) {
        ZIPERR(ZE_TEMP, tempzip);
      }
    }
#else
    if ((tempzip = tempname(out_path)) == NULL) {
      ZIPERR(ZE_TEMP, out_path);
    }
    if (show_what_doing) {
      sprintf(errbuf, "sd: Temp file (1n): %s", tempzip);
      zipmessage(errbuf, "");
    }
    Trace((stderr, "zip diagnostic: tempzip: %s\n", tempzip));
    if ((y = zfopen(tempzip, FOPW_TMP)) == NULL) {
      ZIPERR(ZE_TEMP, tempzip);
    }
#endif

#if (!defined(VMS) && !defined(CMS_MVS))
    /* Use large buffer to speed up stdio: */
#if (defined(_IOFBF) || !defined(BUFSIZ))
    zipbuf = (char *)malloc(ZBSZ);
#else
    zipbuf = (char *)malloc(BUFSIZ);
#endif
    if (zipbuf == NULL) {
      ZIPERR(ZE_MEM, tempzip);
    }
# ifdef _IOFBF
    setvbuf(y, zipbuf, _IOFBF, ZBSZ);
# else
    setbuf(y, zipbuf);
# endif /* _IOBUF */
#endif /* !VMS  && !CMS_MVS */


    if ((r = readzipfile()) != ZE_OK) {
      ZIPERR(r, zipfile);
    }

    /* Write central directory and end header to temporary zip */
    if (show_what_doing) {
      zipmessage("sd: Writing central directory (-FF)", "");
    }
    diag("writing central directory");
    k = 0;                        /* keep count for end header */
    c = tempzn;                   /* get start of central */
    n = t = 0;
    for (z = zfiles; z != NULL; z = z->nxt)
    {
      if ((r = putcentral(z)) != ZE_OK) {
        ZIPERR(r, tempzip);
      }
      tempzn += 4 + CENHEAD + z->nam + z->cext + z->com;
      n += z->len;
      t += z->siz;
      k++;
    }
    if (zcount == 0)
      zipwarn("zip file empty", "");
    t = tempzn - c;               /* compute length of central */
    diag("writing end of central directory");
    if ((r = putend(k, t, c, zcomlen, zcomment)) != ZE_OK) {
      ZIPERR(r, tempzip);
    }
    if (y == current_local_file) {
      current_local_file = NULL;
    }
    if (fclose(y)) {
      ZIPERR(d ? ZE_WRITE : ZE_TEMP, tempzip);
    }
    if (in_file != NULL) {
      fclose(in_file);
      in_file = NULL;
    }

    /* Replace old zip file with new zip file, leaving only the new one */
    if (strcmp(out_path, "-") && !d)
    {
      diag("replacing old zip file with new zip file");
      if ((r = replace(out_path, tempzip)) != ZE_OK)
      {
        zipwarn("new zip file left as: ", tempzip);
        free((zvoid *)tempzip);
        tempzip = NULL;
        ZIPERR(r, "was replacing the original zip file");
      }
      free((zvoid *)tempzip);
    }
    tempzip = NULL;
    if (zip_attributes && strcmp(zipfile, "-")) {
      setfileattr(out_path, zip_attributes);
#ifdef VMS
      /* If the zip file existed previously, restore its record format: */
      if (x != NULL)
        (void)VMSmunch(out_path, RESTORE_RTYPE, NULL);
#endif
    }

    set_filetype(out_path);

    /* finish logfile (it gets closed in freeup() called by finish()) */
    if (logfile) {
        struct tm *now;
        time_t clocktime;

        zfprintf(logfile, "\nTotal %ld entries (", files_total);
        DisplayNumString(logfile, bytes_total);
        zfprintf(logfile, " bytes)");

        /* get current time */
        time(&clocktime);
        now = localtime(&clocktime);
        zfprintf(logfile, "\nDone %s", asctime(now));
        fflush(logfile);
    }

    RETURN(finish(ZE_OK));
  } /* end -FF */

  /* ----------------------------------------------- */



  zfiles = NULL;
  zfilesnext = &zfiles;                        /* first link */
  zcount = 0;

  total_cd_total_entries = 0;                  /* total CD entries for all archives */

#ifdef BACKUP_SUPPORT
  /* If --diff, read any incremental archives.  The entries from these
     archives get added to the z list and so prevents any matching
     entries from the file scan from being added to the new archive. */

  if ((diff_mode == 2 || backup_type == BACKUP_INCR) && apath_list) {
    struct filelist_struct *ap = apath_list;

    for (; ap; ) {
      if (show_what_doing) {
        sprintf(errbuf, "sd: Scanning incremental archive:  %s", ap->name);
        zipmessage(errbuf, "");
      }

      if (backup_type == BACKUP_INCR) {
        zipmessage("Scanning incr archive:  ", ap->name);
      }

      read_inc_file(ap->name);

      ap = ap->next;
    } /* for */
  }
#endif /* BACKUP_SUPPORT */



  /* Read old archive */

  /* Now read the zip file here instead of when doing args above */
  /* Only read the central directory and build zlist */

#ifdef BACKUP_SUPPORT
      if (backup_type == BACKUP_DIFF || backup_type == BACKUP_INCR) {
        zipmessage("Scanning full backup archive:  ", backup_full_path);
      }
#endif /* BACKUP_SUPPORT */


  if (show_what_doing) {
    sprintf(errbuf, "sd: Reading archive:  %s", zipfile);
    zipmessage(errbuf, "");
  }

  /* read zipfile if exists */
#ifdef BACKUP_SUPPORT
  if (backup_type == BACKUP_FULL) {
    /* skip reading archive, even if exists, as replacing */
  }
  else
#endif
  if ((r = readzipfile()) != ZE_OK) {
    ZIPERR(r, zipfile);
  }
  if (show_what_doing) {
    zipmessage("sd: Archive read or no archive", "");
  }


#ifndef UTIL
  if (d && total_disks > 1) {
    ZIPERR(ZE_SPLIT, "Can't grow a split (multi-disk) archive");
  }

  if (split_method == -1) {
    split_method = 0;
  } else if (!fix && split_method == 0 && total_disks > 1) {
    /* if input archive is multi-disk and splitting has not been
       enabled or disabled (split_method == -1), then automatically
       set split size to same as first input split */
    zoff_t size = 0;

    in_split_path = get_in_split_path(in_path, 0);

    if (filetime(in_split_path, NULL, &size, NULL) == 0) {
      zipwarn("Could not get info for input split: ", in_split_path);
      return ZE_OPEN;
    }
    split_method = 1;
    split_size = (uzoff_t) size;

    free(in_split_path);
    in_split_path = NULL;
  }

  if (noisy_splits && split_size > 0)
    zipmessage("splitsize = ", zip_fuzofft(split_size, NULL, NULL));
#endif

  /* so disk display starts at 1, will be updated when entries are read */
  current_in_disk = 0;

  /* no input zipfile and showing contents */
  if (!zipfile_exists && show_files &&
      ((kk == 3 && !names_from_file) || action == ARCHIVE)) {
    ZIPERR(ZE_OPEN, zipfile);
  }

  if (zcount == 0 && (action != ADD || d)) {
    zipwarn(zipfile, " not found or empty");
  }

  if (have_out && kk == 3) {
    /* no input paths so assume copy mode and match everything if --out */
    for (z = zfiles; z != NULL; z = z->nxt) {
#ifdef UNICODE_SUPPORT_WIN32
      char *uzname;
      uzname = wchar_to_utf8_string(z->znamew);
      z->mark = pcount ? filter(uzname, filter_match_case) : 1;
      free(uzname);
#else
      z->mark = pcount ? filter(z->zname, filter_match_case) : 1;
#endif
    }
  }

  /* Scan for new files */

#ifdef ENABLE_USER_PROGRESS
  u_p_phase = 1;
  u_p_task = "Scanning";
#endif /* def ENABLE_USER_PROGRESS */

  /* Process file arguments from command line added using add_name() */
  if (filelist) {
    int old_no_wild = no_wild;

    if (action == ARCHIVE) {
      /* find in archive */
      if (show_what_doing) {
        zipmessage("sd: Scanning archive entries", "");
      }
      for (; filelist; ) {
        char *name = filelist->name;
        if ((r = proc_archive_name(name, filter_match_case)) != ZE_OK) {
          if (r == ZE_MISS) {
            char *n = NULL;
#ifdef WIN32
            /* Win9x console always uses OEM character coding, and
               WinNT console is set to OEM charset by default, too */
            if ((n = malloc(strlen(name) + 1)) == NULL)
              ZIPERR(ZE_MEM, "name not matched error");
            INTERN_TO_OEM(name, n);
#else
            n = filelist->name;
#endif
            zipwarn("not in archive: ", n);
#ifdef WIN32
            free(n);
#endif
          }
          else {
            ZIPERR(r, name);
          }
        }
        free(filelist->name);
        filearg = filelist;
        filelist = filelist->next;
        free(filearg);
      }
    } else {
      /* try find matching files on OS first then try find entries in archive */
      if (show_what_doing) {
        zipmessage("sd: Scanning files", "");
      }
      for (; filelist; ) {
        no_wild = filelist->verbatim;
#ifdef ETWODD_SUPPORT
        if (etwodd && strcmp(filelist->name, "-") == 0) {
          ZIPERR(ZE_PARMS, "can't use -et (--etwodd) with input from stdin");
        }
#endif
        if ((r = PROCNAME(filelist->name)) != ZE_OK) {
          if (r == ZE_MISS) {
            if (bad_open_is_error) {
              zipwarn("name not matched: ", filelist->name);
              ZIPERR(ZE_OPEN, filelist->name);
            } else {
              zipwarn("name not matched: ", filelist->name);
            }
          } else {
            ZIPERR(r, filelist->name);
          }
        }
        free(filelist->name);
        filearg = filelist;
        filelist = filelist->next;
        free(filearg);
      }
    }
    no_wild = old_no_wild;
  }

#ifdef IZ_CRYPT_AES_WG
  if (encryption_method >= AES_MIN_ENCRYPTION) {
    /*
    time_t pool_init_start;
    time_t pool_init_time;
    */

    int salt_len;

    if (show_what_doing) {
      zipmessage("sd: Initializing AES_WG encryption random number pool", "");
    }

    /*
    pool_init_start = time(NULL);
    */

    /* initialize the random number pool */
    aes_rnp.entropy = entropy_fun;
    prng_init(aes_rnp.entropy, &aes_rnp);
    /* and the salt */
# if 0
    if ((zsalt = malloc(32)) == NULL) {
      ZIPERR(ZE_MEM, "Getting memory for salt");
    }
    prng_rand(zsalt, SALT_LENGTH(1), &aes_rnp);
# endif

    /* We now allow using a "fake" salt for debugging purposes (ONLY). */
    salt_len = SALT_LENGTH(encryption_method - (AES_MIN_ENCRYPTION - 1));
    if ((zsalt = malloc(salt_len)) == NULL) {
      ZIPERR(ZE_MEM, "Getting memory for salt");
    }
# ifdef FAKE_SALT
    memset(zsalt, 1, salt_len);         /* Feel free to discard. */
    zipwarn("using fake salt for debugging - ENCRYPTION COMPROMISED!!", "");
# else
    prng_rand(zsalt, salt_len, &aes_rnp);
# endif

    /*
    pool_init_time = time(NULL) - pool_init_start;
    */

    if (show_what_doing) {
      zipmessage("sd: AES_WG random number pool initialized", "");
      /*
      fprintf(mesg, "sd: AES_WG random number pool initialized in %d s\n",
       pool_init_time);
      */
    }
  }
#endif /* def IZ_CRYPT_AES_WG */

#ifdef IZ_CRYPT_AES_WG_NEW
  if (encryption_method >= AES_MIN_ENCRYPTION) {
    time_t pool_init_start;
    time_t pool_init_time;

    if (show_what_doing) {
        zipmessage("sd: Initializing AES_WG", "");
    }

    pool_init_start = time(NULL);

    /* initialise mode and set key  */
    ccm_init_and_key(key,            /* the key value                */
                     key_size,       /* and its length in bytes      */
                     &aesnew_ctx);   /* the mode context             */

    pool_init_time = time(NULL) - pool_init_start;

    if (show_what_doing) {
        sprintf(errbuf, "sd: AES initialized in %d s", pool_init_time);
        zipmessage(errbuf, "");
    }
  }
#endif

  /* recurse from current directory for -R */
  if (recurse == 2) {
#ifdef AMIGA
    if ((r = PROCNAME("")) != ZE_OK)
#else
    if ((r = PROCNAME(".")) != ZE_OK)
#endif
    {
      if (r == ZE_MISS) {
        if (bad_open_is_error) {
          zipwarn("name not matched: ", "current directory for -R");
          ZIPERR(ZE_OPEN, "-R");
        } else {
          zipwarn("name not matched: ", "current directory for -R");
        }
      } else {
        ZIPERR(r, "-R");
      }
    }
  }


  if (show_what_doing) {
    zipmessage("sd: Applying filters", "");
  }
  /* Clean up selections ("3 <= kk <= 5" now) */
  if (kk != 4 && first_listarg == 0 &&
      (action == UPDATE || action == FRESHEN || action == ARCHIVE)) {
    /* if -u or -f with no args, do all, but, when present, apply filters */
    for (z = zfiles; z != NULL; z = z->nxt) {
#ifdef UNICODE_SUPPORT_WIN32
      char *uzname = wchar_to_utf8_string(z->znamew); 
      z->mark = pcount ? filter(uzname, filter_match_case) : 1;
      free(uzname);
#else
      z->mark = pcount ? filter(z->zname, filter_match_case) : 1;
#endif
#ifdef DOS
      if (z->mark) z->dosflag = 1;      /* force DOS attribs for incl. names */
#endif
    }
  }

  /* Currently we only sort the found list for Unix.  Other ports tend to sort
   * it on their own.
   *
   * Sorting is also done when requested (-ad) to put AppleDouble files after
   * the files they go with.  This is supported on most ports.
   *
   * Now sort function fqcmpz_icfirst() is used for all ports.  This implies
   * that all ports support strcasecmp() (except Windows, which uses
   * _stricmp()).
   *
   * If your port does not support strcasecmp(), update check_dup_sort() as
   * needed.
   */
#ifdef UNIX_DIR_SCAN_SORT
  sort_found_list = 1;
#else
  sort_found_list = 0;
#endif

  /* AppleDouble sorting of Zip added "._" files (with leading __MACOSX/
     if sequester) is enabled by default on UNIX_APPLE (Mac OS X) platforms.
     If -ad (sort AppleDouble) is used, any "._" files found on the file
     system are also sorted.  If -ad- (negated -ad), AppleDouble sorting
     is disabled. */
  if (sort_apple_double) {
    sort_found_list = 1;
  }

  if (show_what_doing) {
    if (sort_found_list)
      zipmessage("sd: Checking dups, sorting found list", "");
    else
      zipmessage("sd: Checking dups", "");
    if (sort_apple_double_all)
      zipmessage("sd: AppleDouble sorting of found files enabled", "");
#ifdef UNIX_APPLE
    else if (!sort_apple_double)
      zipmessage("sd: AppleDouble sorting disabled", "");
#endif
  }

  /* remove duplicates in found list */
  if ((r = check_dup_sort(sort_found_list)) != ZE_OK) {
    if (r == ZE_PARMS) {
      ZIPERR(r, "cannot repeat names in zip file");
    }
    else {
      ZIPERR(r, "was processing list of files");
    }
  }

  if (zcount)
    free((zvoid *)zsort);

/* 2015-07-31 SMS.
 * Added HAVE_LONG_LONG to determine proper output format.
 * (Knowing the actual size of zcount might be better.)
 */
/* Using "ll" doesn't work in printf() on older Windows.
 *
 * Properly typing fcount and zcount and using that type
 * is on the ToDo list for Zip 3.1e.  For now, just need
 * to get this diagnostic print statement to work.
 *
 * Sizing fcount and zcount is impacted both by the use
 * of Zip64 or not and by limits on each platform (both
 * the availability of 64-bit integers and the size of
 * size_t).
 *
 * For now, we will make the assumption that the number
 * of entries to archive will not exceed the size of an
 * unsigned long (2 trillion).  fcount and zcount are
 * extent, which are size_t, which are unsigned int
 * (on 32-bit WIN32), which are unsigned long (32-bit).
 * When compiled as 64-bit, size_t becomes an unsigned
 * long long.  And the zip standard allows up to unsigned
 * long long entries in an archive.  However, the current
 * practical limit for the number of entries in a Zip
 * archive is probably less than 1 billion, so for now the
 * unsigned long assumption here when -sd is used seems
 * reasonable.  (The below is only used when -sd diagnostics
 * are used.)  Again, this will be cleaned up in Zip 3.1e,
 * and something like zip_fuzofft() but for size_t might
 * come out of that for displaying these values, whatever
 * size they are at the moment.  2015-08-04 EG */

  if (show_what_doing) {
    sprintf(errbuf, "sd: zcount = %ld", (ulg) zcount);
    zipmessage(errbuf, "");
  }

/* 2010-10-01 SMS.
 * Disabled the following stuff, to let the real temporary name code do
 * its job.
 */
#if 0

/*
 * XXX make some kind of mktemppath() function for each OS.
 */

#ifndef VM_CMS
/* For CMS, leave tempath NULL.  A-disk will be used as default. */
  /* If -b not specified, make temporary path the same as the zip file */
# if defined(MSDOS) || defined(__human68k__) || defined(AMIGA)
  if (tempath == NULL && ((p = MBSRCHR(zipfile, '/')) != NULL ||
#  ifdef MSDOS
                          (p = MBSRCHR(zipfile, '\\')) != NULL ||
#  endif /* def MSDOS */
                          (p = MBSRCHR(zipfile, ':')) != NULL))
  {
    if (*p == ':')
      p++;
# else /* defined(MSDOS) || defined(__human68k__) || defined(AMIGA) */
#  ifdef RISCOS
  if (tempath == NULL && (p = MBSRCHR(zipfile, '.')) != NULL)
  {
#  else /* def RISCOS */
#   ifdef QDOS
  if (tempath == NULL && (p = LastDir(zipfile)) != NULL)
  {
#   else /* def QDOS */
  if (tempath == NULL && (p = MBSRCHR(zipfile, '/')) != NULL)
  {
#   endif /* def QDOS [else] */
#  endif /* def RISCOS [else] */
# endif /* defined(MSDOS) || defined(__human68k__) || defined(AMIGA) [else] */
    if ((tempath = (char *)malloc((int)(p - zipfile) + 1)) == NULL) {
      ZIPERR(ZE_MEM, "was processing arguments (7)");
    }
    r = *p;  *p = 0;
    strcpy(tempath, zipfile);
    *p = (char)r;
  }
#endif /* ndef VM_CMS */

#endif /* 0 */


#if (defined(IZ_CHECK_TZ) && defined(USE_EF_UT_TIME))
  if (!zp_tz_is_valid) {
    zipwarn("TZ environment variable not found, cannot use UTC times!!","");
  }
#endif /* IZ_CHECK_TZ && USE_EF_UT_TIME */

  /* For each marked entry, if not deleting, check if it exists, and if
     updating or freshening, compare date with entry in old zip file.
     Unmark if it doesn't exist or is too old, else update marked count. */
  if (show_what_doing) {
    zipmessage("sd: Scanning files to update", "");
  }
#ifdef MACOS
  PrintStatProgress("Getting file information ...");
#endif
  diag("stating marked entries");
  Trace((stderr, "zip diagnostic: zfiles=%u\n", (unsigned)zfiles));
  k = 0;                        /* Initialize marked count */
  scan_started = 0;
  scan_count   = 0;
  all_current  = 1;
  for (z = zfiles; z != NULL; z = z->nxt) {
    Trace((stderr, "zip diagnostic: stat file=%s\n", z->zname));
    /* if already displayed Scanning files in newname() then continue dots */
    if (noisy && scan_last) {
      scan_count++;
      if (scan_count % 100 == 0) {
        time_t current = time(NULL);

        if (current - scan_last > scan_dot_time) {
          if (scan_started == 0) {
            scan_started = 1;
            zfprintf(mesg, " ");
            fflush(mesg);
          }
          scan_last = current;
          zfprintf(mesg, ".");
          fflush(mesg);
        }
      }
    }
#ifdef UNICODE_SUPPORT
    /* If existing entry has a uname, then Unicode is primary. */
    if (z->uname)
      z->utf8_path = 1;
#endif

    z->current = 0;
    if (!(z->mark)) {
      /* if something excluded run through the list to catch deletions */
      all_current = 0;
    }

    if (z->mark) {
#ifdef USE_EF_UT_TIME
      iztimes f_utim, z_utim;
      ulg z_tim;
#endif /* USE_EF_UT_TIME */
      /* Be aware that using zname instead of oname could cause improper
         display of name on some ports using non-ASCII character sets or
         ports that do OEM conversions. */
      Trace((stderr, "zip diagnostics: marked file=%s\n", z->zname));

      csize = z->siz;
      usize = z->len;
      if (action == DELETE) {
        /* only delete files in date range */
#ifdef USE_EF_UT_TIME
        z_tim = (get_ef_ut_ztime(z, &z_utim) & EB_UT_FL_MTIME) ?
                unix2dostime(&z_utim.mtime) : z->tim;
#else /* !USE_EF_UT_TIME */
#       define z_tim  z->tim
#endif /* ?USE_EF_UT_TIME */
        if (z_tim < before || (after && z_tim >= after)) {
          /* include in archive */
          z->mark = 0;
        } else {
          /* delete file */
          files_total++;
          /* ignore len in old archive and update to current size */
          z->len = usize;
          if (csize != (uzoff_t) -1 && csize != (uzoff_t) -2)
            bytes_total += csize;
          k++;
        }
      } else if (action == ARCHIVE) {
        /* only keep files in date range */
#ifdef USE_EF_UT_TIME
        z_tim = (get_ef_ut_ztime(z, &z_utim) & EB_UT_FL_MTIME) ?
                unix2dostime(&z_utim.mtime) : z->tim;
#else /* !USE_EF_UT_TIME */
#       define z_tim  z->tim
#endif /* ?USE_EF_UT_TIME */
        if (z_tim < before || (after && z_tim >= after)) {
          /* exclude from archive */
          z->mark = 0;
        } else {
          /* keep file */
          files_total++;
          /* ignore len in old archive and update to current size */
          z->len = usize;
          if (csize != (uzoff_t) -1 && csize != (uzoff_t) -2)
            bytes_total += csize;
          k++;
        }
      } else {
        int isdirname = 0;

        if (z->iname && (z->iname)[strlen(z->iname) - 1] == '/') {
          isdirname = 1;
        }

# ifdef UNICODE_SUPPORT_WIN32
        if (!no_win32_wide) {
          if (z->namew == NULL) {
            if (z->uname != NULL)
              z->namew = utf8_to_wchar_string(z->uname);
            else
              z->namew = local_to_wchar_string(z->name);
          }
        }
# endif

/* AD: Problem: filetime not available for MVS non-POSIX files */
/* A filetime equivalent should be created for this case. */
#ifdef USE_EF_UT_TIME
# ifdef UNICODE_SUPPORT_WIN32
        if ((!no_win32_wide) && (z->namew != NULL))
          tf = filetimew(z->namew, (ulg *)NULL, (zoff_t *)&usize, &f_utim);
        else
          tf = filetime(z->name, (ulg *)NULL, (zoff_t *)&usize, &f_utim);
# else
        tf = filetime(z->name, (ulg *)NULL, (zoff_t *)&usize, &f_utim);
# endif
#else /* !USE_EF_UT_TIME */
# ifdef UNICODE_SUPPORT_WIN32
        if ((!no_win32_wide) && (z->namew != NULL))
          tf = filetimew(z->namew, (ulg *)NULL, (zoff_t *)&usize, NULL);
        else
          tf = filetime(z->name, (ulg *)NULL, (zoff_t *)&usize, NULL);
# else
        tf = filetime(z->name, (ulg *)NULL, (zoff_t *)&usize, NULL);
# endif
#endif /* ?USE_EF_UT_TIME */
        if (tf == 0)
          /* entry that is not on OS */
          all_current = 0;
        if (tf == 0 ||
            tf < before || (after && tf >= after) ||
            ((action == UPDATE || action == FRESHEN) &&
#ifdef USE_EF_UT_TIME
             ((get_ef_ut_ztime(z, &z_utim) & EB_UT_FL_MTIME) ?
              f_utim.mtime <= ROUNDED_TIME(z_utim.mtime) : tf <= z->tim)
#else /* !USE_EF_UT_TIME */
             tf <= z->tim
#endif /* ?USE_EF_UT_TIME */
           ))
        {
          z->mark = comadd ? 2 : 0;
          z->trash = tf && tf >= before &&
                     (after ==0 || tf < after);   /* delete if -um or -fm */
          if (verbose)
            zfprintf(mesg, "zip diagnostic: %s %s\n", z->oname,
                   z->trash ? "up to date" : "missing or early");
          if (logfile)
            zfprintf(logfile, "zip diagnostic: %s %s\n", z->oname,
                   z->trash ? "up to date" : "missing or early");
        }
        else if (diff_mode && tf == z->tim &&
                 ((isdirname && (zoff_t)usize == -1) || (usize == z->len))) {
          /* if in diff mode only include if file time or size changed */
          /* usize is -1 for directories */
          z->mark = 0;
        }

        else {
          /* usize is -1 for directories and -2 for devices */
          if (tf == z->tim &&
              ((z->len == 0 && (zoff_t)usize == -1)
               || usize == z->len)) {
            /* FileSync uses the current flag */
            /* Consider an entry current if file time is the same
               and entry size is 0 and a directory on the OS
               or the entry size matches the OS size */
            z->current = 1;
          } else {
            all_current = 0;
          }
          files_total++;
          if (usize != (uzoff_t) -1 && usize != (uzoff_t) -2)
            /* ignore len in old archive and update to current size */
            z->len = usize;
          else
            z->len = 0;
          if (usize != (uzoff_t) -1 && usize != (uzoff_t) -2)
            bytes_total += usize;
          k++;
        }
      }
    }
  } /* for */

  /* Remove entries from found list that do not exist or are too old */
  if (show_what_doing) {
    sprintf(errbuf, "sd: fcount = %lu", (ulg)fcount);
    zipmessage(errbuf, "");
  }

  diag("stating new entries");
  scan_count = 0;
  scan_started = 0;
  Trace((stderr, "zip diagnostic: fcount=%u\n", (unsigned)fcount));
  for (f = found; f != NULL;) {
    Trace((stderr, "zip diagnostic: new file=%s\n", f->oname));

    if (noisy) {
      /* if updating archive and update was quick, scanning for new files
         can still take a long time */
      if (!zip_to_stdout && scan_last == 0 && scan_count % 100 == 0) {
        time_t current = time(NULL);

        if (current - scan_start > scan_delay) {
          zfprintf(mesg, "Scanning files ");
          fflush(mesg);
          mesg_line_started = 1;
          scan_last = current;
        }
      }
      /* if already displayed Scanning files in newname() or above then continue dots */
      if (scan_last) {
        scan_count++;
        if (scan_count % 100 == 0) {
          time_t current = time(NULL);

          if (current - scan_last > scan_dot_time) {
            if (scan_started == 0) {
              scan_started = 1;
              zfprintf(mesg, " ");
              fflush(mesg);
            }
            scan_last = current;
            zfprintf(mesg, ".");
            fflush(mesg);
          }
        }
      }
    }
    tf = 0;
#ifdef UNICODE_SUPPORT
    f->utf8_path = 0;
    /* if Unix port and locale UTF-8, assume paths UTF-8 */
    if (using_utf8 && (f->uname != NULL)) {
      f->utf8_path = 1;
    }
#endif
    if ((action != DELETE) && (action != FRESHEN)
#ifdef UNIX_APPLE
     && (!IS_ZFLAG_APLDBL(f->zflags))
#endif /* UNIX_APPLE */
     ) {
#ifdef UNICODE_SUPPORT_WIN32
      if ((!no_win32_wide) && (f->namew != NULL)) {
        tf = filetimew(f->namew, (ulg *)NULL, (zoff_t *)&usize, NULL);
        /* if have namew, assume got it from wide file scan or
           wide command line */
        if (f->uname) {
          f->utf8_path = 1;
        }
      }
      else
        tf = filetime(f->name, (ulg *)NULL, (zoff_t *)&usize, NULL);
#else
      tf = filetime(f->name, (ulg *)NULL, (zoff_t *)&usize, NULL);
#endif
    }

    if (action == DELETE || action == FRESHEN ||
        ((tf == 0)
#ifdef UNIX_APPLE
        /* Don't bother an AppleDouble file. */
        && (!IS_ZFLAG_APLDBL(f->zflags))
#endif /* UNIX_APPLE */
        ) ||
        tf < before || (after && tf >= after) ||
        (namecmp(f->zname, zipfile) == 0 && !zip_to_stdout)
       ) {
      Trace((stderr, "zip diagnostic: ignore file\n"));
      f = fexpel(f);
    }
    else {
      /* ??? */
      files_total++;
      f->usize = 0;
      if (usize != (uzoff_t) -1 && usize != (uzoff_t) -2) {
        bytes_total += usize;
        f->usize = usize;
      }
      f = f->nxt;
    }
  } /* for found */

  if (mesg_line_started) {
    zfprintf(mesg, "\n");
    mesg_line_started = 0;
  }
#ifdef MACOS
  PrintStatProgress("done");
#endif

#ifndef ZIP_DLL_LIB
  if (test && !unzip_string) {
    /* If we are testing the archive with unzip (unzip_string from -TT can
       use other than unzip, so we skip this check in that case), then get
       a list of needed unzip features and check if the unzip has them. */

    if (show_what_doing) {
      zipmessage("sd: Gathering test information", "");
    }
    /* get list of features believed needed to check archive */
    needed_unzip_features = get_needed_unzip_features();

    if (show_what_doing) {
      zipmessage("sd: Verifying unzip can test archive", "");
    }
    /* verify path to unzip and see if unzip has what it needs */
    if (check_unzip_version(unzip_path, needed_unzip_features) == 0) {
      ZIPERR(ZE_UNZIP, zipfile);
    }
  } /* test */
#endif

  /* Default initialization of method string.  This is set in zipup(). */
  strcpy(method_string, "");

  /* Default initialization of info string. */
  strcpy(info_string, "");

  if (show_files) {
    uzoff_t count = 0;
    uzoff_t bytes = 0;

    if (noisy) {
      fflush(mesg);
    }

    if (noisy && (show_files == 1 || show_files == 3 || show_files == 5)) {
      /* sf, su, sU */
      if (mesg_line_started) {
        zfprintf(mesg, "\n");
        mesg_line_started = 0;
      }
      if (kk == 3  && !names_from_file) {
        /* -sf alone */
        if (pcount || before || after) {
          zfprintf(mesg, "Archive contains (filtered):\n");
          strcpy(action_string, "archive contains");
        } else {
          zfprintf(mesg, "Archive contains:\n");
          strcpy(action_string, "archive contains");
        }
      } else if (action == DELETE) {
        zfprintf(mesg, "Would Delete:\n");
        strcpy(action_string, "would delete");
      } else if (action == FRESHEN) {
        zfprintf(mesg, "Would Freshen:\n");
        strcpy(action_string, "would freshen");
      } else if (action == ARCHIVE) {
        zfprintf(mesg, "Would Copy:\n");
        strcpy(action_string, "would copy");
      } else if (filesync) {
        zfprintf(mesg, "Would Add to/Freshen in/Delete from archive:\n");
        strcpy(action_string, "would sync");
      } else {
        zfprintf(mesg, "Would Add/Update:\n");
        strcpy(action_string, "would update");
      }
      fflush(mesg);
    }

    if (logfile) {
      if (logfile_line_started) {
        zfprintf(logfile, "\n");
        logfile_line_started = 0;
      }
      if (kk == 3 && !names_from_file && !filesync)
        /* -sf alone */
        zfprintf(logfile, "Archive contains:\n");
      else if (action == DELETE)
        zfprintf(logfile, "Would Delete:\n");
      else if (action == FRESHEN)
        zfprintf(logfile, "Would Freshen:\n");
      else if (action == ARCHIVE)
        zfprintf(logfile, "Would Copy:\n");
      else if (filesync)
        zfprintf(logfile, "Would Add to/Freshen in/Delete from archive:\n");
      else
        zfprintf(logfile, "Would Add/Update:\n");
      fflush(logfile);
    }

    /* zlist files */
    for (z = zfiles; z != NULL; z = z->nxt) {
      if (filesync || (!filesync && z->mark) || (kk == 3 && action != ARCHIVE)) {
        int mesg_displayed = 0;
        int log_displayed = 0;

        count++;
        if ((zoff_t)z->len > 0)
          bytes += z->len;

#ifdef ZIP_DLL_LIB
        /* zlist show files */
        if (show_files && lpZipUserFunctions->service != NULL)
        {
          char us[100];
          char cs[100];
          long perc = 0;
          char *oname;
          char *uname;

          oname = z->oname;
# ifdef UNICODE_SUPPORT
          uname = z->uname;
# else
          uname = NULL;
# endif

          WriteNumString(z->len, us);
          WriteNumString(z->siz, cs);
          perc = percent(z->len, z->siz);

          if ((*lpZipUserFunctions->service)(oname,
                                             uname,
                                             us,
                                             cs,
                                             z->len,
                                             z->siz,
                                             action_string,
                                             method_string,
                                             info_string,
                                             perc))
            ZIPERR(ZE_ABORT, "User terminated operation");
        }
#endif /* ZIP_DLL_LIB */

        if (noisy && (show_files == 1 || show_files == 3)) {
          /* sf, su */
#ifdef UNICODE_SUPPORT
          if (unicode_show && z->uname) {
            if (filesync) {
              if (z->mark && !z->current) {
                sprintf(errbuf, "  Freshen:  %s", z->uname);
                print_utf8(errbuf);
                mesg_displayed = 1;
              }
              else if (!z->mark) {
                sprintf(errbuf, "  Delete:   %s", z->uname);
                print_utf8(errbuf);
                mesg_displayed = 1;
              }
            }
            else {
              sprintf(errbuf, "  %s", z->uname);
              print_utf8(errbuf);
              mesg_displayed = 1;
            }
          } else
#endif
          {
            if (filesync) {
              if (z->mark && !z->current) {
                zfprintf(mesg, "  Freshen:  %s", z->oname);
                mesg_displayed = 1;
              }
              else if (!z->mark) {
                zfprintf(mesg, "  Delete:   %s", z->oname);
                mesg_displayed = 1;
              }
            }
            else {
              zfprintf(mesg, "  %s", z->oname);
              mesg_displayed = 1;
            }
          }
        }
        if (logfile && !(show_files == 5 || show_files == 6)) {
          /* not sU or sU- show normal name in log */
#ifdef UNICODE_SUPPORT
          if (log_utf8 && z->uname) {
            if (filesync) {
              if (z->mark && !z->current) {
                zfprintf(logfile, "  Freshen:  %s", z->uname);
                mesg_displayed = 1;
              }
              else if (!z->mark) {
                zfprintf(logfile, "  Delete:   %s", z->uname);
                mesg_displayed = 1;
              }
            }
            else {
              zfprintf(logfile, "  %s", z->uname);
              mesg_displayed = 1;
            }
          } else {
            if (filesync) {
              if (z->mark && !z->current) {
                zfprintf(logfile, "  Freshen:  %s", z->oname);
                mesg_displayed = 1;
              }
              else if (!z->mark) {
                zfprintf(logfile, "  Delete:   %s", z->oname);
                mesg_displayed = 1;
              }
            }
            else {
              zfprintf(logfile, "  %s", z->oname);
              mesg_displayed = 1;
            }
          }
          log_displayed = 1;
#else /* def UNICODE_SUPPORT */
          if (filesync) {
            if (z->mark) {
              zfprintf(logfile, "  Freshen:  %s", z->oname);
            }
            else if (!z->mark) {
              zfprintf(logfile, "  Delete:   %s\n", z->oname);
            }
          }
          else {
            zfprintf(logfile, "  %s\n", z->oname);
          }
#endif /* def UNICODE_SUPPORT [else] */
        } /* logfile && !(show_files == 5 || show_files == 6) */

#ifdef UNICODE_EXTRACT_TEST
        /* UNICODE_EXTRACT_TEST adds code that extracts a directory
           tree from the archive, handling Unicode names.  The files
           are empty (no processing of contents is done).  This is
           used to verify Unicode processing.  Now that UnZip has
           Unicode support, this code should not be needed except
           when implementing and testing new features.  */
        if (create_files) {
          int r;
          int dir = 0;
          FILE *f;

# ifdef UNICODE_SUPPORT_WIN32
          char *fn = NULL;
          wchar_t *fnw = NULL;

          if (!no_win32_wide) {
            if ((fnw = malloc((wcslen(z->znamew) + 120) * sizeof(wchar_t))) == NULL)
              ZIPERR(ZE_MEM, "sC (1)");
            wcscpy(fnw, L"testdir/");
            wcscat(fnw, z->znamew);
            if (fnw[wcslen(fnw) - 1] == '/')
              dir = 1;
            if (dir)
              r = _wmkdir(fnw);
            else
              f = _wfopen(fnw, L"w");
          } else {
            if ((fn = malloc(strlen(z->zname) + 120)) == NULL)
              ZIPERR(ZE_MEM, "sC (2)");
            strcpy(fn, "testdir/");
            strcat(fn, z->zname);
            if (fn[strlen(fn) - 1] == '/')
              dir = 1;
            if (dir)
              r = mkdir(fn);
            else
              f = fopen(fn, "w");
          }
# else /* not UNICODE_SUPPORT_WIN32 */
          char *fn = NULL;
          if ((fn = malloc(strlen(z->zname) + 120)) == NULL)
            ZIPERR(ZE_MEM, "sC (3)");
          strcpy(fn, "testdir/");
          if (z->uname)
            strcat(fn, z->uname);
          else
            strcat(fn, z->zname);

          if (fn[strlen(fn) - 1] == '/')
            dir = 1;
          if (dir)
            r = mkdir(fn, 0777);
          else
            f = fopen(fn, "w");
# endif /* def UNICODE_SUPPORT_WIN32 */
          if (dir) {
            if (r) {
              if (errno != 17) {
                zprintf(" - could not create directory testdir/%s\n", z->oname);
                zperror("    dir");
              }
            } else {
              zprintf(" - created directory testdir/%s\n", z->oname);
            }
          } else {
            if (f == NULL) {
              zprintf(" - could not open testdir/%s\n", z->oname);
              zperror("    file");
            } else {
              fclose(f);
              zprintf(" - created testdir/%s\n", z->oname);
              if (z->uname)
                zprintf("   u - created testdir/%s\n", z->uname);
            }
          }
        }
#endif /* UNICODE_EXTRACT_TEST */

#ifdef UNICODE_SUPPORT
        if (show_files == 3 || show_files == 4) {
          /* su, su- */
          /* Include escaped Unicode name (if exists) under standard name */
          if (z->ouname) {
            if (noisy && show_files == 3) {
              zfprintf(mesg, "\n     Escaped Unicode:  %s", z->ouname);
              mesg_displayed = 1;
            }
            if (logfile) {
              if (log_utf8) {
                zfprintf(logfile, "     Unicode:  %s", z->uname);
              } else {
                zfprintf(logfile, "\n     Escaped Unicode:  %s", z->ouname);
              }
              log_displayed = 1;
            }

          }
        }
        if (show_files == 5 || show_files == 6) {
          /* sU, sU- */
          /* Display only escaped Unicode name if exists or standard name */
          if (z->ouname) {
            /* Unicode name */
            if (filesync) {
              if (z->mark && !z->current) {
                zfprintf(mesg, "  Freshen:  %s", z->ouname);
                mesg_displayed = 1;
              }
              else if (!z->mark) {
                zfprintf(mesg, "  Delete:   %s", z->ouname);
                mesg_displayed = 1;
              }
            }
            else {
              if (noisy && show_files == 5) {
                zfprintf(mesg, "  %s", z->ouname);
                mesg_displayed = 1;
              }
            }
            if (logfile) {
              if (log_utf8) {
                char *u;

                if (z->uname)
                  u = z->uname;
                else
                  u = z->oname;
                if (filesync) {
                  if (z->mark && !z->current) {
                    zfprintf(logfile, "  Freshen:  %s", u);
                    log_displayed = 1;
                  }
                  else if (!z->mark) {
                    zfprintf(logfile, "  Delete:   %s", u);
                    log_displayed = 1;
                  }
                }
                else {
                  zfprintf(logfile, "  %s", u);
                  log_displayed = 1;
                }
              } /* log_utf8 */
              else {
                if (filesync) {
                  if (z->mark && !z->current) {
                    zfprintf(logfile, "  Freshen:  %s", z->ouname);
                    log_displayed = 1;
                  }
                  else if (!z->mark) {
                    zfprintf(logfile, "  Delete:   %s", z->ouname);
                    log_displayed = 1;
                  }
                }
                else {
                  zfprintf(logfile, "  %s", z->ouname);
                  log_displayed = 1;
                }
              }
            } /* logfile */
          } /* z->ouname */
          else {
            /* No Unicode name so use standard name */
            if (noisy && show_files == 5) {
              if (filesync) {
                if (z->mark && !z->current) {
                  zfprintf(mesg, "  Freshen:  %s", z->oname);
                  mesg_displayed = 1;
                }
                else if (!z->mark) {
                  zfprintf(mesg, "  Delete:   %s", z->oname);
                  mesg_displayed = 1;
                }
              }
              else {
                zfprintf(mesg, "  %s", z->oname);
                mesg_displayed = 1;
              }
            }
            if (logfile) {
              if (log_utf8 && z->uname)
                if (filesync) {
                  if (z->mark && !z->current) {
                    zfprintf(logfile, "  Freshen:  %s", z->uname);
                    log_displayed = 1;
                  }
                  else if (!z->mark) {
                    zfprintf(logfile, "  Delete:   %s", z->uname);
                    log_displayed = 1;
                  }
                }
                else {
                  zfprintf(logfile, "  %s", z->uname);
                }
              else {
                if (filesync) {
                  if (z->mark && !z->current) {
                    zfprintf(logfile, "  Freshen:  %s", z->oname);
                    log_displayed = 1;
                  }
                  else if (!z->mark) {
                    zfprintf(logfile, "  Delete:   %s", z->oname);
                    log_displayed = 1;
                  }
                }
                else {
                  zfprintf(logfile, "  %s", z->oname);
                  log_displayed = 1;
                }
              }
            }
          }
        }
#endif /* UNICODE_SUPPORT */
        if (sf_usize) {
          WriteNumString(z->len, errbuf);

          if (noisy && mesg_displayed)
            zfprintf(mesg, "  (%s)", errbuf);
          if (logfile && log_displayed)
            zfprintf(logfile, "  (%s)", errbuf);
        }
        if (noisy && mesg_displayed)
          zfprintf(mesg, "\n");
        if (logfile && log_displayed)
          zfprintf(logfile, "\n");

        if (sf_comment && z->com) {
          char *c;

          /* display file comment on new line */
          /* if multi-line, be sure to indent all lines */
          c = string_replace(z->comment, "\n", "\n    ", REPLACE_ALL, CASE_INS);
          if (noisy)
            zfprintf(mesg, "    %s\n", c);
          if (logfile)
            zfprintf(logfile, "    %s\n", c);
          free(c);
        }
      }
    } /* for zlist */

   /* found list files */
    for (f = found; f != NULL; f = f->nxt) {
      int mesg_displayed = 0;
      int log_displayed = 0;

      count++;
      if ((zoff_t)f->usize > 0)
        bytes += f->usize;
#ifdef UNICODE_SUPPORT
      if (unicode_escape_all) {
        char *escaped_unicode;
        escaped_unicode = local_to_escape_string(f->zname);
        if (noisy && (show_files == 1 || show_files == 3 || show_files == 5)) {
          /* sf, su, sU */
          zfprintf(mesg, "  %s", escaped_unicode);
          mesg_displayed = 1;
        }
        if (logfile) {
          if (log_utf8 && f->uname) {
            zfprintf(logfile, "  %s", f->uname);
            log_displayed = 1;
          } else {
            zfprintf(logfile, "  %s", escaped_unicode);
            log_displayed = 1;
          }
        }
        free(escaped_unicode);
      } /* unicode_escape_all */
      else {
#endif /* UNICODE_SUPPORT */
        if (noisy && (show_files == 1 || show_files == 3 || show_files == 5)) {
          /* sf, su, sU */
          strcpy(action_string, "would add");
#ifdef UNICODE_SUPPORT
          if (unicode_show && f->uname) {
            if (filesync) {
              sprintf(errbuf, "  Add:      %s", f->uname);
            }
            else {
              sprintf(errbuf, "  %s", f->uname);
            }
            print_utf8(errbuf);
            mesg_displayed = 1;
          }
          else
#endif /* UNICODE_SUPPORT */
            if (filesync) {
              zfprintf(mesg, "  Add:      %s", f->oname);
              mesg_displayed = 1;
            }
            else {
              zfprintf(mesg, "  %s", f->oname);
              mesg_displayed = 1;
            }
        }
        if (logfile) {
#ifdef UNICODE_SUPPORT
          if (log_utf8 && f->uname) {
            if (filesync) {
              zfprintf(logfile, "  Add:      %s", f->uname);
              log_displayed = 1;
            }
            else {
              zfprintf(logfile, "  %s", f->uname);
              log_displayed = 1;
            }
          } else {
            if (filesync) {
              zfprintf(logfile, "  Add:      %s", f->oname);
              log_displayed = 1;
            }
            else {
              zfprintf(logfile, "  %s", f->oname);
              log_displayed = 1;
            }
          }
#else /* def UNICODE_SUPPORT */
          zfprintf(logfile, "  %s", f->oname);
#endif /* def UNICODE_SUPPORT [else] */
        }
#ifdef UNICODE_SUPPORT
      } /* not unicode_escape_all */
#endif

      if (sf_usize) {
        WriteNumString(f->usize, errbuf);

        if (noisy && mesg_displayed)
          zfprintf(mesg, "  (%s)", errbuf);
        if (logfile && log_displayed)
          zfprintf(logfile, "  (%s)", errbuf);
      }

#ifdef ZIP_DLL_LIB
      /* found list show files */
      if (show_files && lpZipUserFunctions->service != NULL)
      {
        char us[100];
        char cs[100];
        long perc = 0;
        char *oname;
        char *uname;

        oname = f->oname;
# ifdef UNICODE_SUPPORT
        uname = f->uname;
# else
        uname = NULL;
# endif
        WriteNumString(f->usize, us);
        strcpy(cs, "");
        perc = 0;

        if ((*lpZipUserFunctions->service)(oname,
                                           uname,
                                           us,
                                           cs,
                                           f->usize,
                                           0,
                                           action_string,
                                           method_string,
                                           info_string,
                                           perc))
          ZIPERR(ZE_ABORT, "User terminated operation");
      }
#endif /* ZIP_DLL_LIB */

      if (noisy && mesg_displayed)
        zfprintf(mesg, "\n");
      if (logfile && log_displayed)
        zfprintf(logfile, "\n");

    } /* for found */

    if (!(filesync && show_files)) {
      char e[10];
      char b[10];

      if (count == 1)
        strcpy(e, "entry");
      else
        strcpy(e, "entries");
      
      if (bytes == 1)
        strcpy(b, "byte");
      else
        strcpy(b, "bytes");

      WriteNumString(bytes, errbuf);
      if (noisy || logfile == NULL) {
        if (bytes < 1024) {
          zfprintf(mesg, "Total %s %s (%s %s)\n",
                                              zip_fuzofft(count, NULL, NULL),
                                              e,
                                              zip_fuzofft(bytes, NULL, NULL),
                                              b);
        } else {
          zfprintf(mesg, "Total %s %s (%s %s (%s))\n",
                                              zip_fuzofft(count, NULL, NULL),
                                              e,
                                              zip_fuzofft(bytes, NULL, NULL),
                                              b,
                                              errbuf);
        }
      }
      if (logfile) {
        if (bytes < 1024) {
          zfprintf(logfile, "Total %s %s (%s %s)\n",
                                              zip_fuzofft(count, NULL, NULL),
                                              e,
                                              zip_fuzofft(bytes, NULL, NULL),
                                              b);
        } else {
          zfprintf(logfile, "Total %s %s (%s %s (%s))\n",
                                              zip_fuzofft(count, NULL, NULL),
                                              e,
                                              zip_fuzofft(bytes, NULL, NULL),
                                              b,
                                              errbuf);
        }
      }
    }
#ifdef ZIP_DLL_LIB
    if (*lpZipUserFunctions->finish != NULL) {
      char susize[100];
      char scsize[100];
      long p;
      API_FILESIZE_T abytes = (API_FILESIZE_T)bytes;

      strcpy(susize, zip_fzofft(bytes, NULL, "u"));
      strcpy(scsize, "");
      p = 0;
      (*lpZipUserFunctions->finish)(susize, scsize, abytes, 0, p);
    }
#endif /* ZIP_DLL_LIB */
    RETURN(finish(ZE_OK));
  } /* show_files */

  /* Make sure there's something left to do */
  if (k == 0 && found == NULL && !diff_mode &&
      !(zfiles == NULL && allow_empty_archive) &&
      !(zfiles != NULL &&
        (latest || fix || adjust || junk_sfx || comadd || zipedit))) {
    if (test && (zfiles != NULL || zipbeg != 0)) {
#ifndef ZIP_DLL_LIB
      check_zipfile(zipfile, argv[0], TESTING_FINAL_NAME);
#endif
      RETURN(finish(ZE_OK));
    }
    if (action == UPDATE || action == FRESHEN) {
      diag("Nothing left - update/freshen");
      RETURN(finish(zcount ? ZE_NONE : ZE_NAME));
    }
    /* If trying to delete from empty archive, error is empty archive
       rather than nothing to do. */
    else if (zfiles == NULL
             && (latest || fix || adjust || junk_sfx || action == DELETE)) {
      ZIPERR(ZE_NAME, zipfile);
    }
#ifndef ZIP_DLL_LIB
    else if (recurse && (pcount == 0) && (first_listarg > 0)) {
#if 0
      strcpy(errbuf, "maybe try:  ");
# ifdef VMS
      strcat(errbuf, "zip -r archive \"*.*\" -i pattern1 pattern2 ...");
# else /* !VMS */
#  ifdef AMIGA
      strcat(errbuf, "zip -r archive \"\" -i pattern1 pattern2 ...");
#  else
      strcat(errbuf, "zip -r archive . -i pattern1 pattern2 ...");
#  endif /* AMIGA */
# endif /* VMS */
      diag("Nothing left - error");
      ZIPERR(ZE_NONE, errbuf);
#endif
      diag("Nothing left - error");
      zipwarn("for -r, be sure input names resolve to relative paths (or use -i)", "");
      ZIPERR(ZE_NONE, zipfile);
#if 0
      ZIPERR(ZE_NONE, "for -r, be sure input names resolve to valid relative paths (or use -i)");
#endif
    }
    else {
      diag("Nothing left - zipfile");
      ZIPERR(ZE_NONE, zipfile);
    }
#endif /* !ZIP_DLL_LIB */
  }

  if (filesync && all_current && fcount == 0) {
    zipmessage("Archive is current", "");
    RETURN(finish(ZE_OK));
  }

  /* d true if appending */
  d = (d && k == 0 && (zipbeg || zfiles != NULL));

#ifdef IZ_CRYPT_ANY
  /* Initialize the crc_32_tab pointer, when encryption was requested. */
  if (key != NULL) {
    crc_32_tab = get_crc_table();
#ifdef EBCDIC
    /* convert encryption key to ASCII (ISO variant for 8-bit ASCII chars) */
    strtoasc(key, key);
#endif /* EBCDIC */
  }
#endif /* def IZ_CRYPT_ANY */

  /* Just ignore the spanning signature if a multi-disk archive */
  if (zfiles && total_disks != 1 && zipbeg == 4) {
    zipbeg = 0;
  }

  /* Not sure yet if this is the best place to free args, but seems no need for
     the args array after this.  Suggested by Polo from forum. */
  free_args(args);
  args = NULL;

  /* Before we get carried away, make sure zip file is writeable. This
   * has the undesired side effect of leaving one empty junk file on a WORM,
   * so when the zipfile does not exist already and when -b is specified,
   * the writability check is made in replace().
   */
  if (strcmp(out_path, "-"))
  {
    if (tempdir && zfiles == NULL && zipbeg == 0) {
      zip_attributes = 0;
    } else {
#ifdef BACKUP_SUPPORT
      if (backup_type) have_out = 1;
#endif
#ifdef UNICODE_SUPPORT_WIN32
      x = (have_out || (zfiles == NULL && zipbeg == 0)) ?
              zfopen(out_path, FOPW) :
              zfopen(out_path, FOPM);
#else
      x = (have_out || (zfiles == NULL && zipbeg == 0)) ? zfopen(out_path, FOPW) :
                                                          zfopen(out_path, FOPM);
#endif
      /* Note: FOPW and FOPM expand to several parameters for VMS */
      if (x == NULL) {
        ZIPERR(ZE_CREAT, out_path);
      }
      fclose(x);
      zip_attributes = getfileattr(out_path);
      if (zfiles == NULL && zipbeg == 0)
        destroy(out_path);
    }
  }
  else
    zip_attributes = 0;

  /* Throw away the garbage in front of the zip file for -J */
  if (junk_sfx) zipbeg = 0;

  /* Open zip file and temporary output file */
  if (show_what_doing) {
    zipmessage("sd: Open zip file and create temp file", "");
  }
  diag("opening zip file and creating temporary zip file");
  x = NULL;
  tempzn = 0;
  if (strcmp(out_path, "-") == 0)
  {
    zoff_t pos;

#ifdef MSDOS
    /* It is nonsense to emit the binary data stream of a zipfile to
     * the (text mode) console.  This case should already have been caught
     * in a call to zipstdout() far above.  Therefore, if the following
     * failsafe check detects a console attached to stdout, zip is stopped
     * with an "internal logic error"!  */
    if (isatty(fileno(stdout)))
      ZIPERR(ZE_LOGIC, "tried to write binary zipfile data to console!");
    /* Set stdout mode to binary for MSDOS systems */
# ifdef __HIGHC__
    setmode(stdout, _BINARY);
# else
    setmode(fileno(stdout), O_BINARY);
# endif
    y = zfdopen(fileno(stdout), FOPW_STDOUT);  /* FOPW */
#else
    y = stdout;
#endif

    /* See if output file is empty.  On Windows, if using >> we need to
       account for existing data. */
    pos = zftello(y);

    if (pos != 0) {
      /* Have data at beginning.  As we don't intentionally open in append
         mode, this usually only happens when something like
             zip - test.txt >> out.zip
         is used on Windows.  Account for existing data by skipping over it. */
      bytes_this_split = pos;
      current_local_offset = pos;
      tempzn = pos;
    }

    /* tempzip must be malloced so a later free won't barf */
    tempzip = malloc(4);
    if (tempzip == NULL) {
      ZIPERR(ZE_MEM, "allocating temp filename (4)");
    }
    strcpy(tempzip, "-");
  }
  else if (d) /* d true if just appending (-g) */
  {
    Trace((stderr, "zip diagnostic: grow zipfile: %s\n", zipfile));
    if (total_disks > 1) {
      ZIPERR(ZE_PARMS, "cannot grow split archive");
    }

    if ((y = zfopen(zipfile, FOPM)) == NULL) {
      ZIPERR(ZE_NAME, zipfile);
    }
    tempzip = zipfile;
    /*
    tempzf = y;
    */

    if (zfseeko(y, cenbeg, SEEK_SET)) {
      ZIPERR(ferror(y) ? ZE_READ : ZE_EOF, zipfile);
    }
    bytes_this_split = cenbeg;
    tempzn = cenbeg;
  }
  else
  {
    if (show_what_doing) {
      zipmessage("sd: Creating new zip file", "");
    }
    /* See if there is something at beginning of disk 1 to copy.
       If not, do nothing as zipcopy() will open files to read
       as needed. */
    if (zipbeg) {
      in_split_path = get_in_split_path(in_path, 0);

      while ((in_file = zfopen(in_split_path, FOPR_EX)) == NULL) {
        /* could not open split */

        /* Ask for directory with split.  Updates in_path */
        if (ask_for_split_read_path(0) != ZE_OK) {
          ZIPERR(ZE_ABORT, "could not open archive to read");
        }
        free(in_split_path);
        in_split_path = get_in_split_path(in_path, 1);
      }
    }
#if defined(UNIX) && !defined(NO_MKSTEMP)
    {
      int yd;
      int i;

      /* use mkstemp to avoid race condition and compiler warning */

      if (tempath != NULL)
      {
        /* if -b used to set temp file dir use that for split temp */
        if ((tempzip = malloc(strlen(tempath) + 12)) == NULL) {
          ZIPERR(ZE_MEM, "allocating temp filename (5)");
        }
        strcpy(tempzip, tempath);
        if (lastchar(tempzip) != '/')
          strcat(tempzip, "/");
      }
      else
      {
        /* create path by stripping name and appending template */
        if ((tempzip = malloc(strlen(out_path) + 12)) == NULL) {
        ZIPERR(ZE_MEM, "allocating temp filename (6)");
        }
        strcpy(tempzip, out_path);
        for (i = strlen(tempzip); i > 0; i--) {
          if (tempzip[i - 1] == '/')
            break;
        }
        tempzip[i] = '\0';
      }
      strcat(tempzip, "ziXXXXXX");

      if ((yd = mkstemp(tempzip)) == EOF) {
        ZIPERR(ZE_TEMP, tempzip);
      }
      if (show_what_doing) {
        sprintf(errbuf, "sd: Temp file (2u): %s", tempzip);
        zipmessage(errbuf, "");
      }
      if ((y = fdopen(yd, FOPW_TMP)) == NULL) {
        ZIPERR(ZE_TEMP, tempzip);
      }
    }
#else /* not (defined(UNIX) && !defined(NO_MKSTEMP)) */
    if ((tempzip = tempname(out_path)) == NULL) {
      ZIPERR(ZE_TEMP, out_path);
    }
    if (show_what_doing) {
      sprintf(errbuf, "sd: Temp file (2n): %s", tempzip);
      zipmessage(errbuf, "");
    }
    if ((y = zfopen(tempzip, FOPW_TMP)) == NULL) {
      ZIPERR(ZE_TEMP, tempzip);
    }
#endif /* defined(UNIX) && !defined(NO_MKSTEMP) */
  }

#if (!defined(VMS) && !defined(CMS_MVS))
  /* Use large buffer to speed up stdio: */
# if (defined(_IOFBF) || !defined(BUFSIZ))
  zipbuf = (char *)malloc(ZBSZ);
# else
  zipbuf = (char *)malloc(BUFSIZ);
# endif
  if (zipbuf == NULL) {
    ZIPERR(ZE_MEM, tempzip);
  }
# ifdef _IOFBF
  setvbuf(y, zipbuf, _IOFBF, ZBSZ);
# else
  setbuf(y, zipbuf);
# endif /* _IOBUF */
#endif /* !VMS  && !CMS_MVS */

  /* If not seekable set some flags 3/14/05 EG */
  output_seekable = 1;
  if (!is_seekable(y)) {
    output_seekable = 0;
    use_descriptors = 1;
  }

  /* Not needed.  Only need Zip64 when input file is larger than 2 GiB or reading
     stdin and writing stdout.  This is set in putlocal() for each file. */
#if 0
  /* If using descriptors and Zip64 enabled force Zip64 3/13/05 EG */
# ifdef ZIP64_SUPPORT
  if (use_descriptors && force_zip64 != 0) {
    force_zip64 = 1;
  }
# endif
#endif

  /* if archive exists, not streaming and not deleting or growing, copy
     any bytes at beginning */
  if (strcmp(zipfile, "-") != 0 && !d)  /* this must go *after* set[v]buf */
  {
    /* copy anything before archive */
    if (in_file && zipbeg && (r = bfcopy(zipbeg)) != ZE_OK) {
      ZIPERR(r, r == ZE_TEMP ? tempzip : zipfile);
    }
    if (in_file) {
      fclose(in_file);
      in_file = NULL;
      free(in_split_path);
    }
    tempzn = zipbeg;
    if (split_method) {
      /* add spanning signature */
      if (show_what_doing) {
        zipmessage("sd: Adding spanning/splitting signature at top of archive", "");
      }
      /* write the spanning signature at the top of the archive */
      errbuf[0] = 0x50 /*'P' except for EBCDIC*/;
      errbuf[1] = 0x4b /*'K' except for EBCDIC*/;
      errbuf[2] = 7;
      errbuf[3] = 8;
      bfwrite(errbuf, 1, 4, BFWRITE_DATA);
      /* tempzn updated below */
      tempzn += 4;
    }
  }

  o = 0;                                /* no ZE_OPEN errors yet */


  /* Process zip file, updating marked files */
#ifdef DEBUG
  if (zfiles != NULL)
    diag("going through old zip file");
#endif
  if (zfiles != NULL && show_what_doing) {
    zipmessage("sd: Going through old zip file", "");
  }

#ifdef ENABLE_USER_PROGRESS
  u_p_phase = 2;
  u_p_task = "Freshening";
#endif /* def ENABLE_USER_PROGRESS */

  w = &zfiles;
  while ((z = *w) != NULL) {
    if (z->mark == 1)
    {
      uzoff_t len;
      if ((zoff_t)z->len == -1)
        /* device */
        len = 0;
      else
        len = z->len;

      /* if not deleting, zip it up */
      if (action != ARCHIVE && action != DELETE)
      {
        struct zlist far *localz; /* local header */

#ifdef ENABLE_USER_PROGRESS
        u_p_name = z->oname;
#endif /* def ENABLE_USER_PROGRESS */

        if (action == FRESHEN) {
          strcpy(action_string, "freshen");
        } else if (filesync && z->current) {
          strcpy(action_string, "current");
        } else if (!(filesync && z->current)) {
          strcpy(action_string, "update");
        }

        if (verbose || !(filesync && z->current))
          DisplayRunningStats();
        if (noisy)
        {
          if (action == FRESHEN) {
#ifdef UNICODE_SUPPORT
            if (unicode_show && z->uname) {
              sprintf(errbuf, "freshening: %s", z->uname);
              print_utf8(errbuf);
            }
            else
#endif
            zfprintf(mesg, "freshening: %s", z->oname);

            mesg_line_started = 1;
            fflush(mesg);
          } else if (filesync && z->current) {
            if (verbose) {
              zfprintf(mesg, "      ok: %s", z->oname);
              mesg_line_started = 1;
              fflush(mesg);
            }
          } else if (!(filesync && z->current)) {
#ifdef UNICODE_SUPPORT
            if (unicode_show && z->uname) {
              sprintf(errbuf, "updating: %s", z->uname);
              print_utf8(errbuf);
            }
            else
#endif
            zfprintf(mesg, "updating: %s", z->oname);

            mesg_line_started = 1;
            fflush(mesg);
          }
        } /* noisy */
        if (logall)
        {
          if (action == FRESHEN) {
#ifdef UNICODE_SUPPORT
            if (log_utf8 && z->uname)
              zfprintf(logfile, "freshening: %s", z->uname);
            else
#endif
              zfprintf(logfile, "freshening: %s", z->oname);
            logfile_line_started = 1;
            fflush(logfile);
          } else if (filesync && z->current) {
            if (verbose) {
#ifdef UNICODE_SUPPORT
              if (log_utf8 && z->uname)
                zfprintf(logfile, " current: %s", z->uname);
              else
#endif
                zfprintf(logfile, " current: %s", z->oname);
              logfile_line_started = 1;
              fflush(logfile);
            }
          } else {
#ifdef UNICODE_SUPPORT
            if (log_utf8 && z->uname)
              zfprintf(logfile, "updating: %s", z->uname);
            else
#endif
              zfprintf(logfile, "updating: %s", z->oname);
            logfile_line_started = 1;
            fflush(logfile);
          }
        } /* logall */

        /* Get local header flags and extra fields */
        if (readlocal(&localz, z) != ZE_OK) {
          zipwarn("could not read local entry information: ", z->oname);
          z->lflg = z->flg;
          z->ext = 0;
        } else {
          z->lflg = localz->lflg;
          z->ext = localz->ext;
          z->extra = localz->extra;
          if (localz->nam) free(localz->iname);
          if (localz->nam) free(localz->name);
#ifdef UNICODE_SUPPORT
          if (localz->uname) free(localz->uname);
#endif
          free(localz);
        }

        /* zip up existing entries */

        if (!(filesync && z->current) &&
             (r = zipup(z)) != ZE_OK && r != ZE_OPEN && r != ZE_MISS)
        {
          /* zipup error */

          zipmessage_nl("", 1);
          /*
          if (noisy)
          {
            if (mesg_line_started) {
#if (!defined(MACOS) && !defined(ZIP_DLL_LIB))
              putc('\n', mesg);
              fflush(mesg);
#else
              fprintf(stdout, "\n");
              fflush(stdout);
#endif
              mesg_line_started = 0;
            }
          }
          if (logall) {
            if (logfile_line_started) {
              fprintf(logfile, "\n");
              logfile_line_started = 0;
              fflush(logfile);
            }
          }
          */
          sprintf(errbuf, "was zipping %s", z->name);
          ZIPERR(r, errbuf);
        }
        
        if (filesync && z->current)
        {
          /* if filesync if entry matches OS just copy */
          if ((r = zipcopy(z)) != ZE_OK)
          {
            sprintf(errbuf, "was copying %s", z->oname);
            ZIPERR(r, errbuf);
          }
          zipmessage_nl("", 1);
          /*
          if (noisy)
          {
            if (mesg_line_started) {
#if (!defined(MACOS) && !defined(ZIP_DLL_LIB))
              putc('\n', mesg);
              fflush(mesg);
#else
              fprintf(stdout, "\n");
              fflush(stdout);
#endif
              mesg_line_started = 0;
            }
          }
          if (logall) {
            if (logfile_line_started) {
              fprintf(logfile, "\n");
              logfile_line_started = 0;
              fflush(logfile);
            }
          }
          */
        } /* filesync && z->current */
        
        if (r == ZE_OPEN || r == ZE_MISS)
        {
          o = 1;
          zipmessage_nl("", 1);

          /*
          if (noisy)
          {
#if (!defined(MACOS) && !defined(ZIP_DLL_LIB))
            putc('\n', mesg);
            fflush(mesg);
#else
            fprintf(stdout, "\n");
#endif
            mesg_line_started = 0;
          }
          if (logall) {
            fprintf(logfile, "\n");
            logfile_line_started = 0;
            fflush(logfile);
          }
          */

          if (r == ZE_OPEN) {
            zipwarn_indent("could not open for reading: ", z->oname);
            zipwarn_indent( NULL, strerror( errno));
            if (bad_open_is_error) {
              sprintf(errbuf, "was zipping %s", z->name);
              ZIPERR(r, errbuf);
            }
            strcpy(action_string, "can't open/read");
          } else {
            zipwarn_indent("file and directory with the same name (1): ",
             z->oname);
            strcpy(action_string, "filename=dirname");
          }
          zipwarn_indent("will just copy entry over: ", z->oname);
          if ((r = zipcopy(z)) != ZE_OK)
          {
            sprintf(errbuf, "was copying %s", z->oname);
            ZIPERR(r, errbuf);
          }
          z->mark = 0;
        } /* r == ZE_OPEN || r == ZE_MISS */

#ifdef ZIP_DLL_LIB
        /* zip it up */
        if (lpZipUserFunctions->service != NULL)
        {
          char us[100];
          char cs[100];
          long perc = 0;
          char *oname;
          char *uname;


          oname = z->oname;
# ifdef UNICODE_SUPPORT
          uname = z->uname;
# else
          uname = NULL;
# endif

          WriteNumString(z->siz, cs);
          WriteNumString(z->len, us);
          if (z->siz)
            perc = percent(z->len, z->siz);

          if ((*lpZipUserFunctions->service)(oname,
                                             uname,
                                             us,
                                             cs,
                                             z->len,
                                             z->siz,
                                             action_string,
                                             method_string,
                                             info_string,
                                             perc))
            ZIPERR(ZE_ABORT, "User terminated operation");
        }
# if defined(WIN32) && defined(LARGE_FILE_SUPPORT)
        else
        {
          char *oname;
          char *uname;

          oname = z->oname;
#  ifdef UNICODE_SUPPORT
          uname = z->uname;
#  else
          uname = NULL;
#  endif
          /* no int64 support in caller */
          filesize64 = z->siz;
          low = (unsigned long)(filesize64 & 0x00000000FFFFFFFF);
          high = (unsigned long)((filesize64 >> 32) & 0x00000000FFFFFFFF);
          if (lpZipUserFunctions->service_no_int64 != NULL) {
            if ((*lpZipUserFunctions->service_no_int64)(oname,
                                                        uname,
                                                        low,
                                                        high))
                      ZIPERR(ZE_ABORT, "User terminated operation");
          }
        }
# endif /* defined(WIN32) && defined(LARGE_FILE_SUPPORT) */
#endif /* ZIP_DLL_LIB */

        files_so_far++;
        good_bytes_so_far += z->len;
        bytes_so_far += len;
        w = &z->nxt;
      } /* zip it up (action != ARCHIVE && action != DELETE) */

      else if (action == ARCHIVE && cd_only)
      {
        /* for cd_only compression archives just write central directory */
        DisplayRunningStats();
        /* just note the entry */
        strcpy(action_string, "note entry");
        if (noisy) {
#ifdef UNICODE_SUPPORT
          if (unicode_show && z->uname) {
            sprintf(errbuf, " noting: %s", z->uname);
            print_utf8(errbuf);
          }
          else
#endif
          zfprintf(mesg, " noting: %s", z->oname);

          if (display_usize) {
            zfprintf(mesg, " (");
            DisplayNumString(mesg, z->len );
            zfprintf(mesg, ")");
          }
          mesg_line_started = 1;
          fflush(mesg);
        }
        if (logall)
        {
#ifdef UNICODE_SUPPORT
          if (log_utf8 && z->uname)
            zfprintf(logfile, " noting: %s", z->uname);
          else
#endif
            zfprintf(logfile, " noting: %s", z->oname);
          if (display_usize) {
            zfprintf(logfile, " (");
            DisplayNumString(logfile, z->len );
            zfprintf(logfile, ")");
          }
          logfile_line_started = 1;
          fflush(logfile);
        }

        /* input counts */
        files_so_far++;
        good_bytes_so_far += z->siz;
        bytes_so_far += z->siz;

        w = &z->nxt;

        /* ------------------------------------------- */

#ifdef ZIP_DLL_LIB
        /* cd_only */
        if (lpZipUserFunctions->service != NULL)
        {
          char us[100];
          char cs[100];
          long perc = 0;
          char *oname;
          char *uname;

          oname = z->oname;
# ifdef UNICODE_SUPPORT
          uname = z->uname;
# else
          uname = NULL;
# endif

          WriteNumString(z->siz, us);
          WriteNumString(z->len, cs);
          if (z->siz)
            perc = percent(z->len, z->siz);

          if ((*lpZipUserFunctions->service)(oname,
                                             uname,
                                             us,
                                             cs,
                                             z->siz,
                                             z->len,
                                             action_string,
                                             method_string,
                                             info_string,
                                             perc))
            ZIPERR(ZE_ABORT, "User terminated operation");
        }
# if defined(WIN32) && defined(LARGE_FILE_SUPPORT)
        else
        {
          char *oname;
          char *uname;

          oname = z->oname;
# ifdef UNICODE_SUPPORT
          uname = z->uname;
# else
          uname = NULL;
# endif
          /* no int64 support in caller */
          if (lpZipUserFunctions->service_no_int64 != NULL) {
            filesize64 = z->siz;
            low = (unsigned long)(filesize64 & 0x00000000FFFFFFFF);
            high = (unsigned long)((filesize64 >> 32) & 0x00000000FFFFFFFF);
            if ((*lpZipUserFunctions->service_no_int64)(oname,
                                                        uname,
                                                        low,
                                                        high))
              ZIPERR(ZE_ABORT, "User terminated operation");
          }
        }
# endif /* defined(WIN32) && defined(LARGE_FILE_SUPPORT) */

/* strange but true: if I delete this and put these two endifs adjacent to
   each other, the Aztec Amiga compiler never sees the second endif!  WTF?? PK */
#endif /* ZIP_DLL_LIB */
        /* ------------------------------------------- */

      } /* action == ARCHIVE && cd_only */

      else if (action == ARCHIVE)
      {
#ifdef DEBUG
        zoff_t here = zftello(y);
#endif

        DisplayRunningStats();
        if (skip_this_disk - 1 != z->dsk)
          /* moved to another disk so start copying again */
          skip_this_disk = 0;
        if (skip_this_disk - 1 == z->dsk) {
          /* skipping this disk */
          strcpy(action_string, "skip");
          if (noisy) {
            zfprintf(mesg, " skipping: %s", z->oname);
            mesg_line_started = 1;
            fflush(mesg);
          }
          if (logall) {
#ifdef UNICODE_SUPPORT
            if (log_utf8 && z->uname)
              zfprintf(logfile, " skipping: %s", z->uname);
            else
#endif
              zfprintf(logfile, " skipping: %s", z->oname);
            logfile_line_started = 1;
            fflush(logfile);
          }
        } else {
          /* copying this entry */
          strcpy(action_string, "copy");
          if (noisy && !comadd) {
#ifdef UNICODE_SUPPORT
            if (unicode_show && z->uname) {
              sprintf(errbuf, " copying: %s", z->uname);
              print_utf8(errbuf);
            }
            else
#endif
            zfprintf(mesg, " copying: %s", z->oname);

            if (display_usize) {
              zfprintf(mesg, " (");
              DisplayNumString(mesg, z->len );
              zfprintf(mesg, ")");
            }
            mesg_line_started = 1;
            fflush(mesg);
          }
          if (logall && !comadd)
          {
#ifdef UNICODE_SUPPORT
            if (log_utf8 && z->uname)
              zfprintf(logfile, " copying: %s", z->uname);
            else
#endif
              zfprintf(logfile, " copying: %s", z->oname);
            if (display_usize) {
              zfprintf(logfile, " (");
              DisplayNumString(logfile, z->len );
              zfprintf(logfile, ")");
            }
            logfile_line_started = 1;
            fflush(logfile);
          }
        }

        if (skip_this_disk - 1 == z->dsk)
          /* skip entries on this disk */
          z->mark = 0;
        else if ((r = zipcopy(z)) != ZE_OK)
        {
          if (r == ZE_ABORT) {
            ZIPERR(r, "user requested abort");
          } else if (fix != 1) {
            /* exit */
            sprintf(errbuf, "was copying %s", z->oname);
            zipwarn("(try -F to attempt to fix)", "");
            ZIPERR(r, errbuf);
          }
          else /* if (r == ZE_FORM) */ {
#ifdef DEBUG
            zoff_t here = zftello(y);
#endif

            /* seek back in output to start of this entry so can overwrite */
            if (zfseeko(y, current_local_offset, SEEK_SET) != 0){
              ZIPERR(r, "could not seek in output file");
            }
            zipwarn("bad - skipping: ", z->oname);
#ifdef DEBUG
            here = zftello(y);
#endif
            tempzn = current_local_offset;
            bytes_this_split = current_local_offset;
          }
        }
        if (skip_this_disk || !(fix == 1 && r != ZE_OK))
        {
          if (noisy && mesg_line_started) {
            zfprintf(mesg, "\n");
            mesg_line_started = 0;
            fflush(mesg);
          }
          if (logall && logfile_line_started) {
            zfprintf(logfile, "\n");
            logfile_line_started = 0;
            fflush(logfile);
          }
        }
        /* input counts */
        files_so_far++;
        if (r != ZE_OK)
          bad_bytes_so_far += z->siz;
        else
          good_bytes_so_far += z->siz;
        bytes_so_far += z->siz;

        if (r != ZE_OK && fix == 1) {
          /* remove bad entry from list */
          v = z->nxt;                     /* delete entry from list */
          free((zvoid *)(z->iname));
          free((zvoid *)(z->zname));
          free(z->oname);
#ifdef UNICODE_SUPPORT
          if (z->uname) free(z->uname);
#endif /* def UNICODE_SUPPORT */
          if (z->ext)
            /* don't have local extra until zipcopy reads it */
            if (z->extra) free((zvoid *)(z->extra));
          if (z->cext && z->cextra != z->extra)
            free((zvoid *)(z->cextra));
          if (z->com)
            free((zvoid *)(z->comment));
          farfree((zvoid far *)z);
          *w = v;
          zcount--;
        } else {
          w = &z->nxt;
        }

        /* ------------------------------------------- */

#ifdef ZIP_DLL_LIB
        /* action ARCHIVE */
        if (lpZipUserFunctions->service != NULL)
        {
          char us[100];
          char cs[100];
          long perc = 0;
          char *oname;
          char *uname;

          oname = z->oname;
# ifdef UNICODE_SUPPORT
          uname = z->uname;
# else
          uname = NULL;
# endif

          WriteNumString(z->siz, us);
          WriteNumString(z->len, cs);
          if (z->siz)
            perc = percent(z->len, z->siz);

          if ((*lpZipUserFunctions->service)(oname,
                                             uname,
                                             us,
                                             cs,
                                             z->siz,
                                             z->len,
                                             action_string,
                                             method_string,
                                             info_string,
                                             perc))
            ZIPERR(ZE_ABORT, "User terminated operation");
        }
# if defined(WIN32) && defined(LARGE_FILE_SUPPORT)
        else
        {
          char *oname;
          char *uname;

          oname = z->oname;
#  ifdef UNICODE_SUPPORT
          uname = z->uname;
#  else
          uname = NULL;
#  endif
          /* no int64 support in caller */
          if (lpZipUserFunctions->service_no_int64 != NULL) {
            filesize64 = z->siz;
            low = (unsigned long)(filesize64 & 0x00000000FFFFFFFF);
            high = (unsigned long)((filesize64 >> 32) & 0x00000000FFFFFFFF);
            if ((*lpZipUserFunctions->service_no_int64)(oname,
                                                        uname,
                                                        low,
                                                        high))
              ZIPERR(ZE_ABORT, "User terminated operation");
          }
        }
# endif /* defined(WIN32) && defined(LARGE_FILE_SUPPORT) */

/* strange but true: if I delete this and put these two endifs adjacent to
   each other, the Aztec Amiga compiler never sees the second endif!  WTF?? PK */
#endif /* WINDLL */
        /* ------------------------------------------- */

      } /* action == ARCHIVE */

      else /* Delete */
      {
        strcpy(action_string, "delete");
        DisplayRunningStats();
        if (noisy)
        {
#ifdef UNICODE_SUPPORT
          if (unicode_show && z->uname) {
            sprintf(errbuf, "deleting: %s", z->uname);
            print_utf8(errbuf);
          }
          else
#endif
          zfprintf(mesg, "deleting: %s", z->oname);

          if (display_usize) {
            zfprintf(mesg, " (");
            DisplayNumString(mesg, z->len );
            zfprintf(mesg, ")");
          }
          fflush(mesg);
          zfprintf(mesg, "\n");
        }
        if (logall)
        {
#ifdef UNICODE_SUPPORT
          if (log_utf8 && z->uname)
            zfprintf(logfile, "deleting: %s", z->uname);
          else
#endif
            zfprintf(logfile, "deleting: %s", z->oname);
          if (display_usize) {
            zfprintf(logfile, " (");
            DisplayNumString(logfile, z->len );
            zfprintf(logfile, ")");
          }
          zfprintf(logfile, "\n");
          fflush(logfile);
        }
        files_so_far++;
        good_bytes_so_far += z->siz;
        bytes_so_far += z->siz;

        /* ------------------------------------------- */
#ifdef ZIP_DLL_LIB
        /* delete */
        if (lpZipUserFunctions->service != NULL)
        {
          char us[100];
          char cs[100];
          long perc = 0;
          char *oname;
          char *uname;


          oname = z->oname;
# ifdef UNICODE_SUPPORT
          uname = z->uname;
# else
          uname = NULL;
# endif

          WriteNumString(z->siz, us);
          WriteNumString(z->len, cs);
          if (z->siz)
            perc = percent(z->len, z->siz);

          if ((*lpZipUserFunctions->service)(oname,
                                             uname,
                                             us,
                                             cs,
                                             z->siz,
                                             z->len,
                                             action_string,
                                             method_string,
                                             info_string,
                                             perc))
            ZIPERR(ZE_ABORT, "User terminated operation");
        }
# if defined(WIN32) && defined(LARGE_FILE_SUPPORT)
        else
        {
          char *oname;
          char *uname;

          oname = z->oname;
#  ifdef UNICODE_SUPPORT
          uname = z->uname;
#  else
          uname = NULL;
#  endif
          /* no int64 support in caller */
          filesize64 = z->siz;
          low = (unsigned long)(filesize64 & 0x00000000FFFFFFFF);
          high = (unsigned long)((filesize64 >> 32) & 0x00000000FFFFFFFF);
          if (lpZipUserFunctions->service_no_int64 != NULL) {
            if ((*lpZipUserFunctions->service_no_int64)(oname,
                                                        uname,
                                                        low,
                                                        high))
              ZIPERR(ZE_ABORT, "User terminated operation");
          }
        }
# endif /* defined(WIN32) && defined(LARGE_FILE_SUPPORT) */

/* ZIP64_SUPPORT - I added comments around // comments - does that help below? EG */
/* strange but true: if I delete this and put these two endifs adjacent to
   each other, the Aztec Amiga compiler never sees the second endif!  WTF?? PK */
#endif /* ZIP_DLL_LIB */
        /* ------------------------------------------- */


        v = z->nxt;                     /* delete entry from list */
        free((zvoid *)(z->iname));
        free((zvoid *)(z->zname));
        free(z->oname);
#ifdef UNICODE_SUPPORT
        if (z->uname) free(z->uname);
#endif /* def UNICODE_SUPPORT */
        if (z->ext)
          /* don't have local extra until zipcopy reads it */
          if (z->extra) free((zvoid *)(z->extra));
        if (z->cext && z->cextra != z->extra)
          free((zvoid *)(z->cextra));
        if (z->com)
          free((zvoid *)(z->comment));
        farfree((zvoid far *)z);
        *w = v;
        zcount--;
      } /* Delete */

    } /* z->mark == 1 */
    else
    { /* z->mark != 1 */
      if (action == ARCHIVE) {
        v = z->nxt;                     /* delete entry from list */
        free((zvoid *)(z->iname));
        free((zvoid *)(z->zname));
        free(z->oname);
#ifdef UNICODE_SUPPORT
        if (z->uname) free(z->uname);
#endif /* def UNICODE_SUPPORT */
        if (z->ext)
          /* don't have local extra until zipcopy reads it */
          if (z->extra) free((zvoid *)(z->extra));
        if (z->cext && z->cextra != z->extra)
          free((zvoid *)(z->cextra));
        if (z->com)
          free((zvoid *)(z->comment));
        farfree((zvoid far *)z);
        *w = v;
        zcount--;
      }
      else
      { /* action != ARCHIVE */
        if (filesync) {
          /* Delete entries if don't match a file on OS */
          BlankRunningStats();
          strcpy(action_string, "delete");
          if (noisy)
          {
#ifdef UNICODE_SUPPORT
            if (unicode_show && z->uname) {
              sprintf(errbuf, "deleting: %s", z->uname);
              print_utf8(errbuf);
            }
            else
#endif
            zfprintf(mesg, "deleting: %s", z->oname);

            if (display_usize) {
              zfprintf(mesg, " (");
              DisplayNumString(mesg, z->len );
              zfprintf(mesg, ")");
            }
            fflush(mesg);
            zfprintf(mesg, "\n");
            mesg_line_started = 0;
          }
          if (logall)
          {
#ifdef UNICODE_SUPPORT
            if (log_utf8 && z->uname)
              zfprintf(logfile, "deleting: %s", z->uname);
            else
#endif
              zfprintf(logfile, "deleting: %s", z->oname);
            if (display_usize) {
              zfprintf(logfile, " (");
              DisplayNumString(logfile, z->len );
              zfprintf(logfile, ")");
            }
            zfprintf(logfile, "\n");

            fflush(logfile);
            logfile_line_started = 0;
          }
        }
        /* copy the original entry */
        else if (!d && !diff_mode && (r = zipcopy(z)) != ZE_OK)
        {
          sprintf(errbuf, "was copying %s", z->oname);
          ZIPERR(r, errbuf);
        }
        w = &z->nxt;
      } /* action != ARCHIVE */
    } /* z->mark != 1 */
  } /* while w in zlist */


  /* Process the edited found list, adding them to the zip file */
  if (show_what_doing) {
    zipmessage("sd: Zipping up new entries", "");
  }

#ifdef ENABLE_USER_PROGRESS
  u_p_phase = 3;
  u_p_task = "Adding";
#endif /* def ENABLE_USER_PROGRESS */

  diag("zipping up new entries, if any");
  Trace((stderr, "zip diagnostic: fcount=%u\n", (unsigned)fcount));
  for (f = found; f != NULL; f = fexpel(f))
  {
    /* process found list */

    uzoff_t len;
    /* add a new zfiles entry and set the name */
    if ((z = (struct zlist far *)farmalloc(sizeof(struct zlist))) == NULL) {
      ZIPERR(ZE_MEM, "was adding files to zip file");
    }
    z->nxt = NULL;
    z->name = f->name;
    f->name = NULL;
#ifdef UNICODE_SUPPORT
    z->uname = NULL;          /* UTF-8 name for extra field */
    z->zuname = NULL;         /* externalized UTF-8 name for matching */
    z->ouname = NULL;         /* display version of UTF-8 name with OEM */

# if 0
    /* New AppNote bit 11 allowing storing UTF-8 in path */
    if (utf8_native && f->uname) {
      if (f->iname)
        free(f->iname);
      if ((f->iname = malloc(strlen(f->uname) + 1)) == NULL)
        ZIPERR(ZE_MEM, "Unicode bit 11");
      strcpy(f->iname, f->uname);
#  ifdef WIN32
      if (f->inamew)
        free(f->inamew);
      f->inamew = utf8_to_wchar_string(f->iname);
#  endif
    }
# endif

    /* Only set z->uname if have a non-ASCII Unicode name */
    /* The Unicode path extra field is created if z->uname is not NULL,
       unless on a UTF-8 system, then instead of creating the extra field
       set bit 11 in the General Purpose Bit Flag */
    {
      int is_ascii = 0;

# ifdef WIN32
      if (!no_win32_wide)
        is_ascii = is_ascii_stringw(f->inamew);
      else
        is_ascii = is_ascii_string(f->uname);
# else
      is_ascii = is_ascii_string(f->uname);
# endif

      if (z->uname == NULL) {
        if (!is_ascii)
          z->uname = f->uname;
        else
          free(f->uname);
      } else {
        free(f->uname);
      }
    }
    f->uname = NULL;

    z->utf8_path = f->utf8_path;
#endif /* UNICODE_SUPPORT */

    z->iname = f->iname;
    f->iname = NULL;
    z->zname = f->zname;
    f->zname = NULL;
    z->oname = f->oname;
    f->oname = NULL;
#ifdef UNICODE_SUPPORT_WIN32
    z->namew = f->namew;
    f->namew = NULL;
    z->inamew = f->inamew;
    f->inamew = NULL;
    z->znamew = f->znamew;
    f->znamew = NULL;
#endif
    z->zflags = f->zflags;

    z->flg = 0;
#ifdef UNICODE_SUPPORT
    if (z->uname && utf8_native)
      z->flg |= UTF8_BIT;
#endif

    z->ext = z->cext = z->com = 0;
    z->extra = z->cextra = NULL;
    z->mark = 1;
    z->dosflag = f->dosflag;
    /* zip it up */
    DisplayRunningStats();

#ifdef ENABLE_USER_PROGRESS
    u_p_name = z->oname;
#endif /* def ENABLE_USER_PROGRESS */

    strcpy(action_string, "add");

    if (noisy)
    {
#ifdef UNICODE_SUPPORT
      if (unicode_show && z->uname) {
        sprintf(errbuf, "  adding: %s", z->uname);
        print_utf8(errbuf);
      }
      else
#endif
      zfprintf(mesg, "  adding: %s", z->oname);

      mesg_line_started = 1;
      fflush(mesg);
    }
    if (logall)
    {
#ifdef UNICODE_SUPPORT
      if (log_utf8 && z->uname)
        zfprintf(logfile, "  adding: %s", z->uname);
      else
#endif
        zfprintf(logfile, "  adding: %s", z->oname);
      logfile_line_started = 1;
      fflush(logfile);
    }
    /* initial scan */

    /* zip up found list */

    len = f->usize;
    if ((r = zipup(z)) != ZE_OK  && r != ZE_OPEN && r != ZE_MISS)
    {
      zipmessage_nl("", 1);
      /*
      if (noisy)
      {
#if (!defined(MACOS) && !defined(ZIP_DLL_LIB))
        putc('\n', mesg);
        fflush(mesg);
#else
        fprintf(stdout, "\n");
#endif
        mesg_line_started = 0;
        fflush(mesg);
      }
      if (logall) {
        fprintf(logfile, "\n");
        logfile_line_started = 0;
        fflush(logfile);
      }
      */
      sprintf(errbuf, "was zipping %s", z->oname);
      ZIPERR(r, errbuf);
    } /* zipup */
    if (r == ZE_OPEN || r == ZE_MISS)
    {
      o = 1;
      zipmessage_nl("", 1);
      /*
      if (noisy)
      {
#if (!defined(MACOS) && !defined(ZIP_DLL_LIB))
        putc('\n', mesg);
        fflush(mesg);
#else
        fprintf(stdout, "\n");
#endif
        mesg_line_started = 0;
        fflush(mesg);
      }
      if (logall) {
        fprintf(logfile, "\n");
        logfile_line_started = 0;
        fflush(logfile);
      }
      */
      if (r == ZE_OPEN) {
        zipwarn_indent("could not open for reading: ", z->oname);
        zipwarn_indent( NULL, strerror( errno));
        if (bad_open_is_error) {
          sprintf(errbuf, "was zipping %s", z->name);
          ZIPERR(r, errbuf);
        }
        strcpy(action_string, "can't open/read");
      } else {
        zipwarn_indent("file and directory with the same name (2): ",
         z->oname);
        strcpy(action_string, "filename=dirname");
      }
      files_so_far++;
      bytes_so_far += len;
      bad_files_so_far++;
      bad_bytes_so_far += len;
#ifdef ENABLE_USER_PROGRESS
      u_p_name = NULL;
#endif /* def ENABLE_USER_PROGRESS */
      free((zvoid *)(z->name));
      free((zvoid *)(z->iname));
      free((zvoid *)(z->zname));
      free(z->oname);
#ifdef UNICODE_SUPPORT
      if (z->uname)
        free(z->uname);
# ifdef UNICODE_SUPPORT_WIN32
      if (z->namew)
        free((zvoid *)(z->namew));
      if (z->inamew)
        free((zvoid *)(z->inamew));
      if (z->znamew)
        free((zvoid *)(z->znamew));
# endif
#endif /* UNICODE_SUPPORT */
      farfree((zvoid far *)z);
    }
    else
    {
      /* Not ZE_OPEN or ZE_MISS */
#ifdef ZIP_DLL_LIB
      /* process found list */
      if (lpZipUserFunctions->service != NULL)
      {
        char us[100];
        char cs[100];
        long perc = 0;
        char *oname;
        char *uname;


        oname = z->oname;
# ifdef UNICODE_SUPPORT
        uname = z->uname;
# else
        uname = NULL;
# endif
        if (uname == NULL)
          uname = z->oname;

        WriteNumString(z->siz, cs);
        WriteNumString(z->len, us);
        if (z->siz)
          perc = percent(z->len, z->siz);

        if ((*lpZipUserFunctions->service)(oname,
                                           uname,
                                           us,
                                           cs,
                                           z->len,
                                           z->siz,
                                           action_string,
                                           method_string,
                                           info_string,
                                           perc))
          ZIPERR(ZE_ABORT, "User terminated operation");
      }
# if defined(WIN32) && defined(LARGE_FILE_SUPPORT)
      else
      {
        char *oname;
        char *uname;

        oname = z->oname;
#  ifdef UNICODE_SUPPORT
        uname = z->uname;
#  else
        uname = NULL;
#  endif
        /* no int64 support in caller */
        filesize64 = z->siz;
        low = (unsigned long)(filesize64 & 0x00000000FFFFFFFF);
        high = (unsigned long)((filesize64 >> 32) & 0x00000000FFFFFFFF);
        if (lpZipUserFunctions->service_no_int64 != NULL) {
          if ((*lpZipUserFunctions->service_no_int64)(oname,
                                                      uname,
                                                      low,
                                                      high))
                    ZIPERR(ZE_ABORT, "User terminated operation");
        }
      }
# endif /* defined(WIN32) && defined(LARGE_FILE_SUPPORT) */
#endif /* ZIP_DLL_LIB */

      files_so_far++;
      /* current size of file (just before reading) */
      good_bytes_so_far += z->len;
      /* size of file on initial scan */
      bytes_so_far += len;
      *w = z;
      w = &z->nxt;
      zcount++;
    }
  } /* for found list (processing) */

  /* NULLing this here prevents check_zipfile() from using
     the password. */
#if 0
  if (key != NULL)
  {
    free((zvoid *)key);
    key = NULL;
  }
#endif

  /* final status 3/17/05 EG */

#ifdef ENABLE_USER_PROGRESS
  u_p_phase = 4;
  u_p_task = "Finishing";
#endif /* def ENABLE_USER_PROGRESS */

#ifdef WINDOWS_LONG_PATHS
  if (archive_has_long_path) {
    zipwarn("Archive contains at least one Windows long path", "");
    zipwarn("- Archive may not be readable in some utilities", "");
  }
#endif

  if (bad_files_so_far)
  {
    char tempstrg[100];

    zfprintf(mesg, "\nzip warning: Not all files were readable\n");
    zfprintf(mesg, "  files/entries read:  %lu", files_total - bad_files_so_far);
    WriteNumString(good_bytes_so_far, tempstrg);
    zfprintf(mesg, " (%s bytes)", tempstrg);
    zfprintf(mesg, "  skipped:  %lu", bad_files_so_far);
    WriteNumString(bad_bytes_so_far, tempstrg);
    zfprintf(mesg, " (%s bytes)\n", tempstrg);
    fflush(mesg);
  }
  if (logfile && bad_files_so_far)
  {
    char tempstrg[100];

    zfprintf(logfile, "\nzip warning: Not all files were readable\n");
    zfprintf(logfile, "  files/entries read:  %lu", files_total - bad_files_so_far);
    WriteNumString(good_bytes_so_far, tempstrg);
    zfprintf(logfile, " (%s bytes)", tempstrg);
    zfprintf(logfile, "  skipped:  %lu", bad_files_so_far);
    WriteNumString(bad_bytes_so_far, tempstrg);
    zfprintf(logfile, " (%s bytes)", tempstrg);
  }

  /* Get one line comment for each new entry */
  if (show_what_doing) {
    zipmessage("sd: Get comment if any", "");
  }
#if defined(AMIGA) || defined(MACOS)
  if (comadd || filenotes)
  {
    if (comadd)
#else
# ifdef STREAM_COMMENTS
  if (comadd && !include_stream_ef)
# else
  if (comadd)
# endif
  {
#endif
    {
      if (comment_stream == NULL) {
#ifndef RISCOS
        comment_stream = (FILE*)fdopen(fileno(stderr), "r");
#else
        comment_stream = stderr;
#endif
      }
      if ((e = malloc(MAXCOMLINE + 1)) == NULL) {
        ZIPERR(ZE_MEM, "was reading comment lines (1)");

      }
    }

#ifdef __human68k__
    setmode(fileno(comment_stream), O_TEXT);
#endif
#ifdef MACOS
    if (noisy) zfprintf(mesg, "\nStart commenting files ...\n");
#endif
    for (z = zfiles; z != NULL; z = z->nxt)
      if (z->mark)
#if defined(AMIGA) || defined(MACOS)
        if (filenotes && (p = GetComment(z->zname)))
        {
          if (z->comment = malloc(k = strlen(p)+1))
          {
            z->com = k;
            strcpy(z->comment, p);
          }
          else
          {
            free((zvoid *)e);
            ZIPERR(ZE_MEM, "was reading filenotes");
          }
        }
        else if (comadd)
#endif /* AMIGA || MACOS */
        {
#if defined(ZIPLIB) || defined(ZIPDLL)
          ecomment(z);
#else
          get_entry_comment(z);
#endif /* defined(ZIPLIB) || defined(ZIPDLL) */

#if 0
          if (noisy)
            zfprintf(mesg, "Enter comment for %s:\n", z->oname);
          if (fgets(e, MAXCOMLINE+1, comment_stream) != NULL)
          {
            if ((p = malloc((comment_size = strlen(e))+1)) == NULL)
            {
              free((zvoid *)e);
              ZIPERR(ZE_MEM, "was reading comment lines (2)");
            }
            strcpy(p, e);
            if (p[comment_size - 1] == '\n')
              p[--comment_size] = 0;
            z->comment = p;
            /* zip64 support 09/05/2003 R.Nausedat */
            z->com = comment_size;
          }
#endif
        }
#ifdef MACOS
    if (noisy) zfprintf(mesg, "\n...done");
#endif
#if defined(AMIGA) || defined(MACOS)
    if (comadd)
      free((zvoid *)e);
    GetComment(NULL);           /* makes it free its internal storage */
#else
    free((zvoid *)e);
#endif
  }


/* --- start Archive Comment code --- */

  /* Get (possibly multi-line) archive comment. */
  if (zipedit)
  {

#ifdef ZIP_DLL_LIB
    acomment(zcomlen);
# if 0
    if ((p = malloc(strlen(szCommentBuf)+1)) == NULL) {
      ZIPERR(ZE_MEM, "was setting comments to null (2)");
    }
    if (szCommentBuf[0] != '\0')
       lstrcpy(p, szCommentBuf);
    else
       p[0] = '\0';
    free((zvoid *)zcomment);
    zcomment = NULL;
    GlobalUnlock(hStr);
    GlobalFree(hStr);
    zcomment = p;
    zcomlen = strlen(zcomment);
# endif

# if 0
    if ((p = malloc(zcomlen+1)) == NULL) {
      ZIPERR(ZE_MEM, "was setting comments to null (2)");
    }
    if (szCommentBuf[0] != '\0')
       lstrcpy(p, szCommentBuf);
    else
       p[0] = '\0';
    free((zvoid *)zcomment);
    zcomment = NULL;
    zcomment = p;
    zcomlen = strlen(zcomment);
# endif

#else /* not ZIP_DLL_LIB */

    /* not LIB or DLL */

    /* Try to get new comment first, then replace old comment (if any). - EG */
    char *new_zcomment;
    int new_zcomlen;
    int new_len;
    int keep_current = 0;
    int new_read;
    int comment_from_tty = 0;

    if (comment_stream == NULL) {
# ifndef RISCOS
      comment_stream = (FILE*)fdopen(fileno(stderr), "r");
# else
      comment_stream = stderr;
# endif
    }
    if (noisy)
    {
      fputs("\n", mesg);
      fputs("---------------------------------------\n", mesg);
    }
    if (noisy && zcomlen)
    {
      /* Display old archive comment, if any. */
      fputs("Current zip file comment is:\n", mesg);
      fputs("----------\n", mesg);
      fwrite(zcomment, 1, zcomlen, mesg);
      if (zcomment[zcomlen-1] != '\n')
        putc('\n', mesg);
      fputs("----------\n", mesg);
    }

    comment_from_tty = isatty(fileno(comment_stream));

    if (noisy)
    {
      if (comment_from_tty) {
        if (zcomlen)
          fputs(
  "Enter new zip file comment (end with . line) or hit ENTER to keep existing:\n",
          mesg);
        else
          fputs("Enter new zip file comment (end with . line):\n", mesg);
      }
      else {
        fprintf(mesg, "Reading zip file comment from stdin:\n");
      }
      fputs("----------\n", mesg);
    }

# if (defined(AMIGA) && (defined(LATTICE)||defined(__SASC)))
    flushall();  /* tty input/output is out of sync here */
# endif
# ifdef __human68k__
    setmode(fileno(comment_stream), O_TEXT);
# endif
# ifdef MACOS
    /* 2014-04-15 SMS.
     * Apparently, on old MacOS we accept only one line.
     * The code looks sub-optimal.
     */
    if ((e = malloc(MAXCOMLINE + 1)) == NULL) {
      ZIPERR(ZE_MEM, "was reading comment lines (3)");
    }
    if (zcomment) {
      free(zcomment);
      zcomment = NULL;
    }
    zprintf("\n enter new zip file comment \n");
    if (fgets(e, MAXCOMLINE+1, comment_stream) != NULL) {
        if ((p = malloc((k = strlen(e))+1)) == NULL) {
            free((zvoid *)e);
            ZIPERR(ZE_MEM, "was reading comment lines (4)");
        }
        strcpy(p, e);
        if (p[k-1] == '\n') p[--k] = 0;
        zcomment = p;
    }
    free((zvoid *)e);
    /* if fgets() fails, zcomment is undefined */
    if (!zcomment) {
      zcomlen = 0;
    } else {
      zcomlen = strlen(zcomment);
    }
# else /* !MACOS */
    /* 2014-04-15 SMS.
     * Changed to stop adding "\r\n" within lines longer than MAXCOMLINE.
     *
     * Now:
     * Read comment text lines until ".\n" or EOF.
     * Allocate (additional) storage in increments of MAXCOMLINE+3.
     *   (MAXCOMLINE = 256)
     * Read pieces up to MAXCOMLINE+1.
     * Convert a read-terminating "\n" character to "\r\n".
     * (If too long, truncate at maximum allowed length (and complain)?)
     */
    /* Keep old comment until got new one. */
    new_zcomment = NULL;
    new_zcomlen = 0;
    while (1)
    {
      /* The first line of the comment (up to the first CR + LF) is the
         one-line description reported by some utilities. */

      new_len = new_zcomlen + MAXCOMLINE + 3;
      /* The total allowed length of the End Of Central Directory Record
         is 65535 bytes and the archive comment can be up to 65535 - 22 =
         65513 bytes of this.  We need to ensure the total comment length
         is no more than this. */
      if (new_len > MAX_COM_LEN) {
        new_len = MAX_COM_LEN;
        if (new_len == new_zcomlen) {
          break;
        }
      }
      /* Allocate (initial or more) space for the file comment. */
      if ((new_zcomment = realloc(new_zcomment, new_len)) == NULL)
      {
        ZIPERR(ZE_MEM, "was reading comment lines (5)");
#  if 0
        new_zcomlen = 0;
        break;
#  endif
      }

      /* Read up to (MAXCOMLINE + 1) characters.  Quit if none available. */
      if (fgets((new_zcomment + new_zcomlen), (MAXCOMLINE + 1), comment_stream) == NULL)
      {
        if (comment_from_tty && new_zcomlen == 0)
          keep_current = 1;
        break;
      }

      new_read = (int)strlen(new_zcomment + new_zcomlen);
      
      if (comment_from_tty) {
        /* If the first line is empty or just a newline, keep current comment */
        if (new_zcomlen == 0 &&
            (new_read == 0 || strcmp((new_zcomment), "\n") == 0))
        {
          keep_current = 1;
          break;
        }
      } /* comment_from_tty */

      /* Detect ".\n" comment terminator in new line read.  Quit, if found.
         For backward compatibility, allow this with stdin. */
      if (new_zcomlen &&
          (new_read == 0 || strcmp((new_zcomment + new_zcomlen), ".\n") == 0))
        break;

      /* Calculate the new length (old_length + newly_read). */
      new_zcomlen += new_read;

      /* Convert (bare) terminating "\n" to "\r\n". */
      if (*(new_zcomment + new_zcomlen - 1) == '\n')
      { /* Have terminating "\n". */
        if ((new_zcomlen <= 1) || (*(new_zcomment + new_zcomlen - 2) != '\r'))
        {
          /* either lone "\n" or "\n" is not already preceded by "\r",
             so insert "\r". */
          *(new_zcomment + new_zcomlen - 1) = '\r';
          new_zcomlen++;
          *(new_zcomment + new_zcomlen - 1) = '\n';
        }
      }
    } /* while (1) */

    if (keep_current)
    {
      free(new_zcomment);
    }
    else
    {
      if (zcomment)
      {
        /* Free old archive comment. */
        free(zcomment);
      }

      /* Use new comment. */
      zcomment = new_zcomment;
      zcomlen = new_zcomlen;

      /* If unsuccessful, make tidy.
       * If successful, terminate the file comment string as desired.
       */
      if (zcomlen == 0)
      {
        if (zcomment)
        {
          /* Free comment storage.  (Empty line read?) */
          free(zcomment);
          zcomment = NULL;
        }
      }
      else
      {
        /* If it's missing, add a final "\r\n".
          * (Do we really want this?  We could add one only if we've seen
          * one before, so that a normal one- or multi-line comment would
          * always end with our usual line ending, but one-line,
          * EOF-terminated input would not.  (Need to add a flag.))
          */
        /* As far as is known current utilities don't expect a terminating
            CR + LF so don't add one.  Some testing of other utilities may
            clarify this. */
        /* Seems UnZip adds a line end at the end if there isn't one. */
#  if 0
        if (*(zcomment+ zcomlen) != '\n')
        {
          *(zcomment+ (zcomlen++)) = '\r';
          *(zcomment+ zcomlen) = '\n';
        }
#  endif
        /* We could NUL-terminate the string here, but no one cares. */
        /* Do it as could be useful for debugging purposes, and it's
           used below. */
        *(zcomment + zcomlen) = '\0';
      }
    }
    if (noisy)
      if (zcomment && !comment_from_tty) {
        /* Display what we read from stdin. */
          fprintf(mesg, "%s", zcomment);
          if (zcomment[zcomlen-1] != '\n') {
            /* Add line end at end if needed. */
            putc('\n', mesg);
          }
      }
      fputs("----------\n", mesg);
#  if 0
    /* SMSd. */
    fprintf( stderr, " zcl = %d, zc: >%.*s<.\n", zcomlen, zcomlen, zcomment);
#  endif /* 0 */

# endif /* ?MACOS */

#endif /* def ZIP_DLL_LIB [else] */

    /* Check for binary. */
    if (!is_text_buf(zcomment, zcomlen)) {
      ZIPERR(ZE_NOTE, "binary not allowed in zip file comment");
    }

    /* Check for UTF-8. */
    if (is_utf8_string(zcomment, NULL, NULL, NULL, NULL)) {
      sprintf(errbuf, "new zip file comment has UTF-8");
      zipwarn(errbuf, "");
    }
  } /* zipedit */

/* --- end Archive Comment code --- */


  if (display_globaldots) {
    /* Terminate (at line end to) the global dots. */
#ifndef ZIP_DLL_LIB
    putc('\n', mesg);
#else
    zfprintf(stdout,"%c",'\n');
#endif
    mesg_line_started = 0;
  }

  /* Write central directory and end header to temporary zip */

#ifdef ENABLE_USER_PROGRESS
  u_p_phase = 5;
  u_p_task = "Done";
#endif /* def ENABLE_USER_PROGRESS */

  if (show_what_doing) {
    zipmessage("sd: Writing central directory", "");
  }
  diag("writing central directory");
  k = 0;                        /* keep count for end header */
  c = tempzn;                   /* get start of central */
  n = t = 0;
  for (z = zfiles; z != NULL; z = z->nxt)
  {
    if (z->mark || !(diff_mode || filesync)) {
      if ((r = putcentral(z)) != ZE_OK) {
        ZIPERR(r, tempzip);
      }
      tempzn += 4 + CENHEAD + z->nam + z->cext + z->com;
      n += z->len;
      t += z->siz;
      k++;
    }
  }

  if (k == 0)
    zipwarn("zip file empty", "");
  if (verbose) {
    zfprintf(mesg, "total bytes=%s, compressed=%s -> %d%% savings\n",
            zip_fzofft(n, NULL, "u"), zip_fzofft(t, NULL, "u"), percent(n, t));
    fflush(mesg);
  }
  if (logall) {
    zfprintf(logfile, "total bytes=%s, compressed=%s -> %d%% savings\n",
            zip_fzofft(n, NULL, "u"), zip_fzofft(t, NULL, "u"), percent(n, t));
    fflush(logfile);
  }

#ifdef ZIP_DLL_LIB
  if (*lpZipUserFunctions->finish != NULL) {
    char susize[100];
    char scsize[100];
    long p;
    API_FILESIZE_T api_n = (API_FILESIZE_T)n;
    API_FILESIZE_T api_t = (API_FILESIZE_T)t;

    WriteNumString(n, susize);
    WriteNumString(t, scsize);
    p = percent(n, t);
    (*lpZipUserFunctions->finish)(susize, scsize, api_n, api_t, p);
  }
#endif /* ZIP_DLL_LIB */

  if (cd_only) {
    zipwarn("cd_only mode: archive has no data - use only for diffs", "");
  }

  t = tempzn - c;               /* compute length of central */
  diag("writing end of central directory");
  if (show_what_doing) {
    zipmessage("sd: Writing end of central directory", "");
  }

  if ((r = putend(k, t, c, zcomlen, zcomment)) != ZE_OK) {
    ZIPERR(r, tempzip);
  }

  /*
  tempzf = NULL;
  */
  if (y == current_local_file) {
    current_local_file = NULL;
  }
  if (fclose(y)) {
    ZIPERR(d ? ZE_WRITE : ZE_TEMP, tempzip);
  }
  y = NULL;
  if (in_file != NULL) {
    fclose(in_file);
    in_file = NULL;
  }
  /*
  if (x != NULL)
    fclose(x);
  */

  /* Free some memory before spawning unzip */
#ifdef USE_ZLIB
  zl_deflate_free();
#else
  lm_free();
#endif
#ifdef BZIP2_SUPPORT
  bz_compress_free();
#endif


#ifndef ZIP_DLL_LIB
  /* Test new zip file before overwriting old one or removing input files */
  /* Split archives tested below after rename.  If --out used, also test
     after rename. */
  if (test && current_disk == 0 && !have_out) {
    if (show_what_doing)
      zipmessage("sd: Testing single disk archive", "");
    check_zipfile(tempzip, argv[0], TESTING_TEMP);
  }
#endif

  /* Replace old zip file with new zip file, leaving only the new one */
  if (strcmp(out_path, "-") && !d)
  {
    diag("replacing old zip file with new zip file");
    if (show_what_doing) {
      zipmessage("sd: Replacing old zip file", "");
    }
    if ((r = replace(out_path, tempzip)) != ZE_OK)
    {
      zipwarn("new zip file left as: ", tempzip);
      free((zvoid *)tempzip);
      tempzip = NULL;
      ZIPERR(r, "was replacing the original zip file");
    }
    free((zvoid *)tempzip);
  }
  tempzip = NULL;

#ifndef ZIP_DLL_LIB
  /* Test split archive after renaming .zip split */
  if (test && (current_disk != 0 || have_out)) {
    if (show_what_doing)
      zipmessage("sd: Testing archive", "");
    check_zipfile(out_path, argv[0], TESTING_FINAL_NAME);
  }
#endif

  /* This needs to be done after archive is tested. */
  if (passwd) {
    free(passwd);
    passwd = NULL;
  }
  if (keyfile) {
    free(keyfile);
    keyfile = NULL;
  }
  if (key)
  {
    free((zvoid *)key);
    key = NULL;
  }

  if (zip_attributes && strcmp(zipfile, "-")) {
    if (setfileattr(out_path, zip_attributes))
      zipwarn("Could not change attributes on output file", "");
#ifdef VMS
    /* If the zip file existed previously, restore its record format: */
    if (x != NULL)
      (void)VMSmunch(out_path, RESTORE_RTYPE, NULL);
#endif
  }
  if (strcmp(zipfile, "-")) {
    if (show_what_doing) {
      zipmessage("sd: Setting file type", "");
    }

    set_filetype(out_path);
  }


#ifdef BACKUP_SUPPORT
  /* if using the -BT backup option, output updated control/status file */
  if (backup_type) {
    struct filelist_struct *apath;
    FILE *fcontrol;
    struct tm *now;
    time_t clocktime;
    char end_datetime[20];

    if (show_what_doing) {
      zipmessage("sd: Creating -BT control file", "");
    }

    if ((fcontrol = zfopen(backup_control_path, "w")) == NULL) {
      zipwarn("Could not open for writing:  ", backup_control_path);
      ZIPERR(ZE_WRITE, "error writing to backup control file");
    }

    /* get current time */
    time(&clocktime);
    now = localtime(&clocktime);
    sprintf(end_datetime, "%04d%02d%02d_%02d%02d%02d",
      now->tm_year + 1900, now->tm_mon + 1, now->tm_mday, now->tm_hour,
      now->tm_min, now->tm_sec);

    zfprintf(fcontrol, "info: Zip Backup Control File (DO NOT EDIT)\n");
    if (backup_type == BACKUP_FULL) {
      zfprintf(fcontrol, "info: backup type:  full\n");
    }
    else if (backup_type == BACKUP_DIFF) {
      zfprintf(fcontrol, "info: backup type:  differential\n");
    }
    else if (backup_type == BACKUP_INCR) {
      zfprintf(fcontrol, "info: backup type:  incremental\n");
    }
    zfprintf(fcontrol, "info: Start date/time:  %s\n", backup_start_datetime);
    zfprintf(fcontrol, "info: End date/time:    %s\n", end_datetime);
    zfprintf(fcontrol, "path: %s\n", backup_control_path);
    zfprintf(fcontrol, "full: %s\n", backup_full_path);

    if (backup_type == BACKUP_DIFF) {
      zfprintf(fcontrol, "diff: %s\n", backup_output_path);
    }
    for (; apath_list; ) {
      if (backup_type == BACKUP_INCR) {
        zfprintf(fcontrol, "incr: %s\n", apath_list->name);
      }
      free(apath_list->name);
      apath = apath_list;
      apath_list = apath_list->next;
      free(apath);
    }

    if (backup_type == BACKUP_INCR) {
      /* next backup this will be an incremental archive to include */
      zfprintf(fcontrol, "incr: %s\n", backup_output_path);
    }

    if (backup_start_datetime) {
      free(backup_start_datetime);
      backup_start_datetime = NULL;
    }
    if (backup_dir) {
      free(backup_dir);
      backup_dir = NULL;
    }
    if (backup_name) {
      free(backup_name);
      backup_name = NULL;
    }
    if (backup_control_path) {
      free(backup_control_path);
      backup_control_path = NULL;
    }
    if (backup_full_path) {
      free(backup_full_path);
      backup_full_path = NULL;
    }
    if (backup_output_path) {
      free(backup_output_path);
      backup_output_path = NULL;
    }
  }
#endif /* BACKUP_SUPPORT */


#ifdef WIN32
  /* All looks good so, if requested, clear the DOS archive bits */
  if (clear_archive_bits) {
    if (noisy)
      zipmessage("Clearing archive bits...", "");
    for (z = zfiles; z != NULL; z = z->nxt)
    {
# ifdef UNICODE_SUPPORT_WIN32
      if (z->mark) {
        if (!no_win32_wide) {
          if (!ClearArchiveBitW(z->namew)){
            zipwarn("Could not clear archive bit for: ", z->oname);
          }
        } else {
          if (!ClearArchiveBit(z->name)){
            zipwarn("Could not clear archive bit for: ", z->oname);
          }
        }
      }
# else
      if (!ClearArchiveBit(z->name)){
        zipwarn("Could not clear archive bit for: ", z->oname);
      }
# endif
    }
  }
#endif /* WIN32 */

#ifdef IZ_CRYPT_AES_WG
  /* close random pool */
  if (encryption_method >= AES_MIN_ENCRYPTION) {
    if (show_what_doing) {
      zipmessage("sd: Closing AES_WG random pool", "");
    }
    prng_end(&aes_rnp);
    free(zsalt);
  }
#endif /* def IZ_CRYPT_AES_WG */

#ifdef IZ_CRYPT_AES_WG_NEW
  /* clean up and end operation   */
  ccm_end(&aesnew_ctx);  /* the mode context             */
#endif /* def IZ_CRYPT_AES_WG_NEW */

  /* finish logfile (it gets closed in freeup() called by finish()) */
  if (logfile) {
      struct tm *now;
      time_t clocktime;

      zfprintf(logfile, "\nTotal %ld entries (", files_total);
      if (good_bytes_so_far != bytes_total) {
        zfprintf(logfile, "planned ");
        DisplayNumString(logfile, bytes_total);
        zfprintf(logfile, " bytes, actual ");
        DisplayNumString(logfile, good_bytes_so_far);
        zfprintf(logfile, " bytes)");
      } else {
        DisplayNumString(logfile, bytes_total);
        zfprintf(logfile, " bytes)");
      }

      /* get current time */

      time(&clocktime);
      now = localtime(&clocktime);
      zfprintf(logfile, "\nDone %s", asctime(now));
  }

  /* Finish up (process -o, -m, clean up).  Exit code depends on o. */
#if (!defined(VMS) && !defined(CMS_MVS))
  free((zvoid *) zipbuf);
#endif /* !VMS && !CMS_MVS */
  RETURN(finish(o ? ZE_OPEN : ZE_OK));
}


/***************  END OF ZIP MAIN CODE  ***************/


/*
 * VMS (DEC C) initialization.
 */
#ifdef VMS
# include "decc_init.c"
#endif



/* Ctrl/T (VMS) AST or SIGUSR1 handler for user-triggered progress
 * message.
 * UNIX: arg = signal number.
 * VMS:  arg = Out-of-band character mask.
 */
#ifdef ENABLE_USER_PROGRESS

# ifndef VMS
#  include <limits.h>
#  include <time.h>
#  include <sys/times.h>
#  include <sys/utsname.h>
#  include <unistd.h>
# endif /* ndef VMS */

USER_PROGRESS_CLASS void user_progress( arg)
int arg;
{
  /* VMS Ctrl/T automatically puts out a line like:
   * ALP::_FTA24: 07:59:43 ZIP       CPU=00:00:59.08 PF=2320 IO=52406 MEM=333
   * (host::tty local_time program cpu_time page_faults I/O_ops phys_mem)
   * We do something vaguely similar on non-VMS systems.
   */
# ifndef VMS

#  ifdef CLK_TCK
#   define clk_tck CLK_TCK
#  else /* def CLK_TCK */
  long clk_tck;
#  endif /* def CLK_TCK [else] */
#  define U_P_NODENAME_LEN 32

  static int not_first = 0;                             /* First time flag. */
  static char u_p_nodename[ U_P_NODENAME_LEN+ 1];       /* "host::tty". */
  static char u_p_prog_name[] = "zip";                  /* Program name. */

  struct utsname u_p_utsname;
  struct tm u_p_loc_tm;
  struct tms u_p_tms;
  char *cp;
  char *tty_name;
  time_t u_p_time;
  float stime_f;
  float utime_f;

  /* On the first time through, get the host name and tty name, and form
   * the "host::tty" string (in u_p_nodename) for the intro line.
   */
  if (not_first == 0)
  {
    not_first = 1;
    /* Host name.  (Trim off any domain info.  (Needed on Tru64.)) */
    uname( &u_p_utsname);
    strncpy( u_p_nodename, u_p_utsname.nodename, (U_P_NODENAME_LEN- 8));
    u_p_nodename[ 24] = '\0';
    cp = strchr( u_p_nodename, '.');
    if (cp != NULL)
      *cp = '\0';

    /* Terminal name.  (Trim off any leading "/dev/"). */
    tty_name = ttyname( 0);
    if (tty_name != NULL)
    {
      cp = strstr( tty_name, "/dev/");
      if (cp != NULL)
        tty_name += 5;

      strcat( u_p_nodename, "::");
      strncat( u_p_nodename, tty_name,
       (U_P_NODENAME_LEN- strlen( u_p_nodename)));
    }
  }

  /* Local time.  (Use reentrant localtime_r().) */
  u_p_time = time( NULL);
  localtime_r( &u_p_time, &u_p_loc_tm);

  /* CPU time. */
  times( &u_p_tms);
#  ifndef CLK_TCK
  clk_tck = sysconf( _SC_CLK_TCK);
#  endif /* ndef CLK_TCK */
  utime_f = ((float)u_p_tms.tms_utime)/ clk_tck;
  stime_f = ((float)u_p_tms.tms_stime)/ clk_tck;

  /* Put out intro line. */
  zfprintf( stderr, "%s %02d:%02d:%02d %s CPU=%.2f\n",
   u_p_nodename,
   u_p_loc_tm.tm_hour, u_p_loc_tm.tm_min, u_p_loc_tm.tm_sec,
   u_p_prog_name,
   (stime_f+ utime_f));

# endif /* ndef VMS */

  if (u_p_task != NULL)
  {
    if (u_p_name == NULL)
      u_p_name = "";

    zfprintf( stderr, "   %s: %s\n", u_p_task, u_p_name);
  }

# ifndef VMS
  /* Re-establish this SIGUSR1 handler.
   * (On VMS, the Ctrl/T handler persists.) */
  signal( SIGUSR1, user_progress);
# endif /* ndef VMS */
}

#endif /* def ENABLE_USER_PROGRESS */

