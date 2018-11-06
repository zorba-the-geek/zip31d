/*
  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-02 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/


/*    Stand-alone test procedure:
 *
 * set command /object = [.vms]zip_cli.obj [.vms]zip_cli.cld
 * define /user_mode vms SYS$DISK:[.vms]
 * cc /define = (TEST, VMSCLI) /include = [] /object = [.vms] [.vms]cmdline.c
 * link /executable = [] [.vms]cmdline.obj, [.vms]zip_cli.obj
 * EXEC*UTE == "$ SYS$DISK:[]'"
 * exec cmdline [ /qualifiers ...] [parameters ...]
 */


/* 2004-12-13 SMS.
 * Disabled the module name macro to accommodate old GNU C which didn't
 * obey the directive, and thus confused MMS/MMK where the object
 * library dependencies need to have the correct module name.
 */
#if 0
# define module_name VMS_ZIP_CMDLINE
# define module_ident "02-015"
#endif /* 0 */

/*
**
**  Facility:   ZIP
**
**  Module:     VMS_ZIP_CMDLINE
**
**  Author:     Hunter Goatley <goathunter@MadGoat.com>
**
**  Date:       July 30, 1993
**
**  Abstract:   Routines to handle a VMS CLI interface for Zip.  The CLI
**              command line is parsed and a new argc/argv are built and
**              returned to Zip.
**
**  Modified by:
**
**      02-015          Steven Schweda          24-MAR-2015
**              Added /[NO]ARGFILES (-AF), /BACKUP = (CONTROL,
**              DIRECTORY, LOG, NAME, TYPE) (-BC, -BD, -BL, -BN, -BT),
**              /DEFAULT_DIRECTORY (-cd), /ENCRYPT = SHORT_PASSWORD
**              (-ps), /PREFIX = ([ ALL | NEW ], PATH) (-pa, -pp),
**              /STATISTICS = TIME (-pt), /[NO]STREAM (-st),
**              /VERSION = BRIEF (-vq).
**
**      02-014          Steven Schweda          08-MAY-2012
**              Changed /BATCH = file to use (new) "-@@ file" option,
**              instead of using freopen() here to get the effect.
**
**      02-013          Steven Schweda          27-MAR-2012
**              Added /BACKUP = (TYPE = type, PATH = path).
**              Added /SHOW = SUFFIXES.
**              Changed to allow /COMPRESSION = mthd = SUFFIX = "", as
**              well as /COMPRESSION = mthd = SUFFIX = ":" (to clear the
**              suffix list).  ("" -> ":" in UNIX argument list.)
**              Moved suffix list processing into a new function,
**              get_suffix_list(), allowing /STORE_TYPES to deal with an
**              empty string (""), same as /COMPRESSION = mthd = SUFFIX.
**              Fixed problems with CHECK_BUF_ALLOC(), et al.
**              Stopped combining one-character options.
**              Made command-line variables global, and added new
**              function, append_simple_option().
**
**      02-012          Steven Schweda          13-FEB-2012
**              Added /COMPRESSION = LZMA (really) and PPMD.
**              Added /COMPRESSION = mthd = LEVEL = n and
**              Added /COMPRESSION = mthd = SUFFIX = (sufx1, ..., sufxn)
**              Added /LEVEL = (mthd1, ..., mthdn).
**              Partially de-inlined CHECK_BUFFER_ALLOCATION() (now
**              CHECK_BUF_ALLOC()) for .OBJ size reduction (20%).
**              Changed explicit run-time init init_dyndesc macro to
**              link-time/implicit DSC_D_INIT.
**
**      02-011          Steven Schweda          25-AUG-2011
**              Added NO and USIZE in /SHOW = [NO]FILES = USIZE.
**
**      02-010          Steven Schweda          22-APR-2011
**              Added /COMPRESSION = LZMA.
**      02-009          Steven Schweda          22-APR-2011
**              Added /ENCRYPT options: [NO]ANSI_PASSWORD, METHOD,
**              PASSWORD.  Sadly, old /ENCRYPT = passwd syntax is now
**              invalid.
**      02-008          Steven Schweda          21-MAY-2010
**              Changed /JUNK to /[NO]JUNK, combined its code with that
**              for /[NO]FULL_PATH, and replaced (no-op) "-p" with "-j-
**              for (/NOJUNK and) /FULL_PATH in the generated options.
**              Changed "long" types to "int".  Removed "register".
**      02-007          Steven Schweda          09-FEB-2005
**              Added /PRESERVE_CASE.
**      02-006          Onno van der Linden,
**                      Christian Spieler       07-JUL-1998 23:03
**              Support GNU CC 2.8 on Alpha AXP (vers-num unchanged).
**      02-006          Johnny Lee              25-JUN-1998 07:40
**              Fixed typo (superfluous ';') (vers-num unchanged).
**      02-006          Christian Spieler       12-SEP-1997 23:17
**              Fixed bugs in /BEFORE and /SINCE handlers (vers-num unchanged).
**      02-006          Christian Spieler       12-JUL-1997 02:05
**              Complete revision of the argv strings construction.
**              Added handling of "-P pwd", "-R", "-i@file", "-x@file" options.
**      02-005          Patrick Ellis           09-MAY-1996 22:25
**              Show UNIX style help screen when UNIX style options are used.
**      02-004          Onno van der Linden,
**                      Christian Spieler       13-APR-1996 20:05
**              Removed /ENCRYPT=VERIFY ("-ee" option).
**      02-003          Christian Spieler       11-FEB-1996 23:05
**              Added handling of /EXTRAFIELDS qualifier ("-X" option).
**      02-002          Christian Spieler       09-JAN-1996 22:25
**              Added "#include crypt.h", corrected typo.
**      02-001          Christian Spieler       04-DEC-1995 16:00
**              Fixed compilation in DEC CC's ANSI mode.
**      02-000          Christian Spieler       10-OCT-1995 17:54
**              Modified for Zip v2.1, added several new options.
**      01-000          Hunter Goatley          30-JUL-1993 07:54
**              Original version (for Zip v1.9p1).
**
*/


/* 2004-12-13 SMS.
 * Disabled the module name macro to accommodate old GNU C which didn't
 * obey the directive, and thus confused MMS/MMK where the object
 * library dependencies need to have the correct module name.
 */
#if 0
# if defined(__DECC) || defined(__GNUC__)
#  pragma module module_name module_ident
# else
#  module module_name module_ident
# endif
#endif /* 0 */

/* Accommodation for /NAMES = AS_IS with old header files. */

#define lib$establish LIB$ESTABLISH
#define lib$get_foreign LIB$GET_FOREIGN
#define lib$get_input LIB$GET_INPUT
#define lib$sig_to_ret LIB$SIG_TO_RET
#define str$concat STR$CONCAT
#define str$find_first_substring STR$FIND_FIRST_SUBSTRING
#define str$free1_dx STR$FREE1_DX

#include "zip.h"
#ifndef TEST
#include "crypt.h"      /* for VMSCLI_help() */
#include "revision.h"   /* for VMSCLI_help() */
#endif /* !TEST */

#include <ssdef.h>
#include <descrip.h>
#include <climsgdef.h>
#include <clidef.h>
#include <lib$routines.h>
#include <str$routines.h>

#ifndef CLI$_COMMA
globalvalue CLI$_COMMA;
#endif

/*
**  "Macro" to initialize a dynamic string descriptor.
*/
#define DSC_D_INIT { 0, DSC$K_DTYPE_T, DSC$K_CLASS_D, NULL }

/*
**  Memory allocation step for argv string buffer.
*/
#define ARGBSIZE_UNIT 256

#define RETURN_INSFMEM( n) \
{ \
    fprintf( stderr, " CLI Mem alloc %d failed.\n", n); \
    return SS$_INSFMEM; \
}

/*
**  Memory reallocation macro (and function) for argv string buffer.
*/
#define CHECK_BUF_ALLOC( buf, rsrvd_p, rqustd) \
{ \
    char *buf_new; \
    if ((buf_new = buffer_alloc( (buf), (rsrvd_p), (rqustd))) == NULL) \
    { \
        return (SS$_INSFMEM); \
    } \
    buf = buf_new; \
}

static char *buffer_alloc( char *buf, unsigned int *reserved_p,
                           unsigned int requested)
{
    if (requested > *reserved_p)
    {
        while (requested > *reserved_p)
        {
            *reserved_p += ARGBSIZE_UNIT;
        }
        buf = realloc( buf, *reserved_p);
    }

    return buf;
}


/*
**  Define descriptors for all of the CLI parameters and qualifiers.
*/
$DESCRIPTOR(cli_delete,         "DELETE");              /* -d */
$DESCRIPTOR(cli_freshen,        "FRESHEN");             /* -f */
$DESCRIPTOR(cli_move,           "MOVE");                /* -m */
$DESCRIPTOR(cli_update,         "UPDATE");              /* -u */
$DESCRIPTOR(cli_exclude,        "EXCLUDE");             /* -x */
$DESCRIPTOR(cli_include,        "INCLUDE");             /* -i */
$DESCRIPTOR(cli_exlist,         "EXLIST");              /* -x@ */
$DESCRIPTOR(cli_inlist,         "INLIST");              /* -i@ */
$DESCRIPTOR(cli_adjust,         "ADJUST_OFFSETS");      /* -A */
$DESCRIPTOR(cli_append,         "APPEND");              /* -g */
$DESCRIPTOR(cli_argfiles,       "ARGFILES");            /* -AF */
#ifdef BACKUP_SUPPORT
$DESCRIPTOR(cli_backup,         "BACKUP");      /* -BC, -BD, -BL, -BN, -BT */
$DESCRIPTOR(cli_backup_control, "BACKUP.CONTROL");      /* -BC */
$DESCRIPTOR(cli_backup_dir,     "BACKUP.DIRECTORY");    /* -BD */
$DESCRIPTOR(cli_backup_log,     "BACKUP.LOG");          /* -BL */
$DESCRIPTOR(cli_backup_name,    "BACKUP.NAME");         /* -BN */
$DESCRIPTOR(cli_backup_type,    "BACKUP.TYPE");         /* -BT */
$DESCRIPTOR(cli_backup_type_diff, "BACKUP.TYPE.DIFFERENTIAL"); /* -BT diff */
$DESCRIPTOR(cli_backup_type_full, "BACKUP.TYPE.FULL");         /* -BT full */
$DESCRIPTOR(cli_backup_type_incr, "BACKUP.TYPE.INCREMENTAL");  /* -BT incr */
$DESCRIPTOR(cli_backup_type_none, "BACKUP.TYPE.NONE");         /* -BT none*/
#endif /* def BACKUP_SUPPORT */
$DESCRIPTOR(cli_batch,          "BATCH");               /* -@, -@@ */
$DESCRIPTOR(cli_before,         "BEFORE");              /* -tt */
$DESCRIPTOR(cli_comments,       "COMMENTS");            /* -c,-z */
$DESCRIPTOR(cli_comment_archive,"COMMENTS.ARCHIVE");    /* -z */
$DESCRIPTOR(cli_comment_zipfile,"COMMENTS.ZIP_FILE");   /* -z */
$DESCRIPTOR(cli_comment_files,  "COMMENTS.FILES");      /* -c */
$DESCRIPTOR(cli_compression,    "COMPRESSION");         /* -Z */
$DESCRIPTOR(cli_compression_b,  "COMPRESSION.BZIP2");           /* -Zb */
$DESCRIPTOR(cli_compression_b_l,"COMPRESSION.BZIP2.LEVEL");     /* -n */
$DESCRIPTOR(cli_compression_b_s,"COMPRESSION.BZIP2.SUFFIX");    /* -n */
$DESCRIPTOR(cli_compression_d,  "COMPRESSION.DEFLATE");         /* -Zd */
$DESCRIPTOR(cli_compression_d_l,"COMPRESSION.DEFLATE.LEVEL");   /* -n */
$DESCRIPTOR(cli_compression_d_s,"COMPRESSION.DEFLATE.SUFFIX");  /* -n */
$DESCRIPTOR(cli_compression_l,  "COMPRESSION.LZMA");            /* -Zl */
$DESCRIPTOR(cli_compression_l_l,"COMPRESSION.LZMA.LEVEL");      /* -n */
$DESCRIPTOR(cli_compression_l_s,"COMPRESSION.LZMA.SUFFIX");     /* -n */
$DESCRIPTOR(cli_compression_p,  "COMPRESSION.PPMD");            /* -Zp */
$DESCRIPTOR(cli_compression_p_l,"COMPRESSION.PPMD.LEVEL");      /* -n */
$DESCRIPTOR(cli_compression_p_s,"COMPRESSION.PPMD.SUFFIX");     /* -n */
$DESCRIPTOR(cli_compression_s,  "COMPRESSION.STORE");           /* -Zs */
$DESCRIPTOR(cli_compression_s_s,"COMPRESSION.STORE.SUFFIX");    /* -n */
$DESCRIPTOR(cli_copy_entries,   "COPY_ENTRIES");        /* -U */
#ifdef CHANGE_DIRECTORY
$DESCRIPTOR(cli_default_dir,    "DEFAULT_DIRECTORY");   /* -cd */
#endif /* def CHANGE_DIRECTORY */
$DESCRIPTOR(cli_descriptors,    "DESCRIPTORS");         /* -fd */
$DESCRIPTOR(cli_difference,     "DIFFERENCE");          /* -DF */
$DESCRIPTOR(cli_dirnames,       "DIRNAMES");            /* -D */
$DESCRIPTOR(cli_display,        "DISPLAY");             /* -d? */
$DESCRIPTOR(cli_display_bytes,  "DISPLAY.BYTES");       /* -db */
$DESCRIPTOR(cli_display_counts, "DISPLAY.COUNTS");      /* -dc */
$DESCRIPTOR(cli_display_dots,   "DISPLAY.DOTS");        /* -dd, -ds */
$DESCRIPTOR(cli_display_globaldots, "DISPLAY.GLOBALDOTS"); /* -dg */
$DESCRIPTOR(cli_display_usize,  "DISPLAY.USIZE");       /* -du */
$DESCRIPTOR(cli_display_volume, "DISPLAY.VOLUME");      /* -dv */
$DESCRIPTOR(cli_dot_version,    "DOT_VERSION");         /* -ww */
$DESCRIPTOR(cli_encrypt,        "ENCRYPT");             /* -e, -P, -pn, -Y */
$DESCRIPTOR(cli_encrypt_ansi,   "ENCRYPT.ANSI_PASSWORD"); /* -pn */
$DESCRIPTOR(cli_encrypt_mthd,   "ENCRYPT.METHOD");      /* -Y */
$DESCRIPTOR(cli_encrypt_mthd_aes128, "ENCRYPT.METHOD.AES128"); /* -Y AES128 */
$DESCRIPTOR(cli_encrypt_mthd_aes192, "ENCRYPT.METHOD.AES192"); /* -Y AES192 */
$DESCRIPTOR(cli_encrypt_mthd_aes256, "ENCRYPT.METHOD.AES256"); /* -Y AES256 */
$DESCRIPTOR(cli_encrypt_mthd_trad, "ENCRYPT.METHOD.TRAD"); /* -Y Traditional */
$DESCRIPTOR(cli_encrypt_pass,   "ENCRYPT.PASSWORD");    /* -P */
$DESCRIPTOR(cli_encrypt_short,  "ENCRYPT.SHORT_PASSWORD");     /* -ps */
$DESCRIPTOR(cli_extra_fields,   "EXTRA_FIELDS");        /* -X [/NO] */
$DESCRIPTOR(cli_extra_fields_normal, "EXTRA_FIELDS.NORMAL"); /* no -X */
$DESCRIPTOR(cli_extra_fields_keep, "EXTRA_FIELDS.KEEP_EXISTING"); /* -X- */
$DESCRIPTOR(cli_filesync,       "FILESYNC");            /* -FS */
$DESCRIPTOR(cli_fix_archive,    "FIX_ARCHIVE");         /* -F[F] */
$DESCRIPTOR(cli_fix_normal,     "FIX_ARCHIVE.NORMAL");  /* -F */
$DESCRIPTOR(cli_fix_full,       "FIX_ARCHIVE.FULL");    /* -FF */
$DESCRIPTOR(cli_full_path,      "FULL_PATH");           /* -j-, -j */
$DESCRIPTOR(cli_grow,           "GROW");                /* -g */
$DESCRIPTOR(cli_help,           "HELP");                /* -h */
$DESCRIPTOR(cli_help_normal,    "HELP.NORMAL");         /* -h */
$DESCRIPTOR(cli_help_extended,  "HELP.EXTENDED");       /* -h2 */
$DESCRIPTOR(cli_junk,           "JUNK");                /* -j, -j- */
$DESCRIPTOR(cli_keep_version,   "KEEP_VERSION");        /* -w */
$DESCRIPTOR(cli_latest,         "LATEST");              /* -o */
$DESCRIPTOR(cli_level,          "LEVEL");               /* -[0-9] */
$DESCRIPTOR(cli_level_0,        "LEVEL.0");             /* -0 */
$DESCRIPTOR(cli_level_1,        "LEVEL.1");             /* -1 */
$DESCRIPTOR(cli_level_2,        "LEVEL.2");             /* -2 */
$DESCRIPTOR(cli_level_3,        "LEVEL.3");             /* -3 */
$DESCRIPTOR(cli_level_4,        "LEVEL.4");             /* -4 */
$DESCRIPTOR(cli_level_5,        "LEVEL.5");             /* -5 */
$DESCRIPTOR(cli_level_6,        "LEVEL.6");             /* -6 */
$DESCRIPTOR(cli_level_7,        "LEVEL.7");             /* -7 */
$DESCRIPTOR(cli_level_8,        "LEVEL.8");             /* -8 */
$DESCRIPTOR(cli_level_9,        "LEVEL.9");             /* -9 */
$DESCRIPTOR(cli_license,        "LICENSE");             /* -L */
$DESCRIPTOR(cli_log_file,       "LOG_FILE");            /* -la, -lf, -li */
$DESCRIPTOR(cli_log_file_append, "LOG_FILE.APPEND");    /* -la */
$DESCRIPTOR(cli_log_file_file,  "LOG_FILE.FILE");       /* -lf */
$DESCRIPTOR(cli_log_file_info,  "LOG_FILE.INFORMATIONAL"); /* -li */
$DESCRIPTOR(cli_must_match,     "MUST_MATCH");          /* -MM */
$DESCRIPTOR(cli_output,         "OUTPUT");              /* -O */
$DESCRIPTOR(cli_patt_case,      "PATTERN_CASE");        /* -ic[-] */
$DESCRIPTOR(cli_patt_case_blind, "PATTERN_CASE.BLIND"); /* -ic */
$DESCRIPTOR(cli_patt_case_sensitive, "PATTERN_CASE.SENSITIVE"); /* -ic- */
$DESCRIPTOR(cli_pkzip,          "PKZIP");               /* -k */
$DESCRIPTOR(cli_prefix,         "PREFIX");              /* -pa, -pp */
$DESCRIPTOR(cli_prefix_all,     "PREFIX.ALL");          /* -pp */
$DESCRIPTOR(cli_prefix_new,     "PREFIX.NEW");          /* -pa */
$DESCRIPTOR(cli_prefix_path,    "PREFIX.PATH");         /* -pa, -pp */
$DESCRIPTOR(cli_pres_case,      "PRESERVE_CASE");       /* -C */
$DESCRIPTOR(cli_pres_case_no2,  "PRESERVE_CASE.NOODS2");/* -C2- */
$DESCRIPTOR(cli_pres_case_no5,  "PRESERVE_CASE.NOODS5");/* -C5- */
$DESCRIPTOR(cli_pres_case_ods2, "PRESERVE_CASE.ODS2");  /* -C2 */
$DESCRIPTOR(cli_pres_case_ods5, "PRESERVE_CASE.ODS5");  /* -C5 */
$DESCRIPTOR(cli_quiet,          "QUIET");               /* -q */
$DESCRIPTOR(cli_recurse,        "RECURSE");             /* -r,-R */
$DESCRIPTOR(cli_recurse_path,   "RECURSE.PATH");        /* -r */
$DESCRIPTOR(cli_recurse_fnames, "RECURSE.FILENAMES");   /* -R */
$DESCRIPTOR(cli_show,           "SHOW");                /* -s? */
$DESCRIPTOR(cli_show_command,   "SHOW.COMMAND");        /* -sc */
$DESCRIPTOR(cli_show_debug,     "SHOW.DEBUG");          /* -sd */
$DESCRIPTOR(cli_show_files,     "SHOW.FILES");          /* -sf */
$DESCRIPTOR(cli_show_files_usize, "SHOW.FILES.USIZE");  /* -sF=usize */
$DESCRIPTOR(cli_show_options,   "SHOW.OPTIONS");        /* -so */
$DESCRIPTOR(cli_show_suffixes,  "SHOW.SUFFIXES");       /* -ss */
$DESCRIPTOR(cli_since,          "SINCE");               /* -t */
$DESCRIPTOR(cli_split,          "SPLIT");               /* -s, -sb, -sp, -sv */
$DESCRIPTOR(cli_split_bell,     "SPLIT.BELL");          /* -sb */
$DESCRIPTOR(cli_split_pause,    "SPLIT.PAUSE");         /* -sp */
$DESCRIPTOR(cli_split_size,     "SPLIT.SIZE");          /* -s */
$DESCRIPTOR(cli_split_verbose,  "SPLIT.VERBOSE");       /* -sv */
$DESCRIPTOR(cli_statistics,     "STATISTICS");          /* -pt */
$DESCRIPTOR(cli_statistics_time, "STATISTICS.TIME");    /* -pt */
$DESCRIPTOR(cli_store_types,    "STORE_TYPES");         /* -n */
$DESCRIPTOR(cli_stream,         "STREAM");              /* -st */
$DESCRIPTOR(cli_sverbose,       "SVERBOSE");            /* -sv */
$DESCRIPTOR(cli_symlinks,       "SYMLINKS");            /* -y */
$DESCRIPTOR(cli_temp_path,      "TEMP_PATH");           /* -b */
$DESCRIPTOR(cli_test,           "TEST");                /* -T */
$DESCRIPTOR(cli_test_unzip,     "TEST.UNZIP");          /* -TT */
$DESCRIPTOR(cli_translate_eol,  "TRANSLATE_EOL");       /* -l[l] */
$DESCRIPTOR(cli_transl_eol_lf,  "TRANSLATE_EOL.LF");    /* -l */
$DESCRIPTOR(cli_transl_eol_crlf,"TRANSLATE_EOL.CRLF");  /* -ll */
$DESCRIPTOR(cli_unsfx,          "UNSFX");               /* -J */
$DESCRIPTOR(cli_verbose,        "VERBOSE");             /* -v[v[v]] */
$DESCRIPTOR(cli_verbose_normal, "VERBOSE.NORMAL");      /* -v */
$DESCRIPTOR(cli_verbose_more,   "VERBOSE.MORE");        /* -vv */
$DESCRIPTOR(cli_verbose_command,"VERBOSE.COMMAND");     /* (none) */
$DESCRIPTOR(cli_version,        "VERSION");             /* -v, --version, -vq */
$DESCRIPTOR(cli_version_brief,  "VERSION.BRIEF");       /* -vq */
$DESCRIPTOR(cli_volume_label,   "VOLUME_LABEL");        /* -$ */
$DESCRIPTOR(cli_vms,            "VMS");                 /* -V */
$DESCRIPTOR(cli_vms_all,        "VMS.ALL");             /* -VV */
$DESCRIPTOR(cli_vnames,         "VNAMES");              /* -vn */
$DESCRIPTOR(cli_wildcard,       "WILDCARD");            /* -nw */
$DESCRIPTOR(cli_wildcard_nospan,"WILDCARD.NOSPAN");     /* -W */

$DESCRIPTOR(cli_yyz,            "YYZ_ZIP");

$DESCRIPTOR(cli_zip64,          "ZIP64");               /* -fz */
$DESCRIPTOR(cli_zipfile,        "ZIPFILE");
$DESCRIPTOR(cli_infile,         "INFILE");
$DESCRIPTOR(zip_command,        "zip ");

static int show_VMSCLI_help;

#if !defined(zip_clitable)
#  define zip_clitable ZIP_CLITABLE
#endif
#if defined(__DECC) || defined(__GNUC__)
extern void *zip_clitable;
#else
globalref void *zip_clitable;
#endif

/* extern unsigned int LIB$GET_INPUT(void), LIB$SIG_TO_RET(void); */

#ifndef __STARLET_LOADED
#ifndef sys$bintim
#  define sys$bintim SYS$BINTIM
#endif
#ifndef sys$numtim
#  define sys$numtim SYS$NUMTIM
#endif
extern int sys$bintim ();
extern int sys$numtim ();
#endif /* !__STARLET_LOADED */
#ifndef cli$dcl_parse
#  define cli$dcl_parse CLI$DCL_PARSE
#endif
#ifndef cli$present
#  define cli$present CLI$PRESENT
#endif
#ifndef cli$get_value
#  define cli$get_value CLI$GET_VALUE
#endif
extern unsigned int cli$dcl_parse ();
extern unsigned int cli$present ();
extern unsigned int cli$get_value ();

unsigned int vms_zip_cmdline (int *, char ***);
static unsigned int get_list (struct dsc$descriptor_s *,
                              struct dsc$descriptor_d *, int,
                              char **, unsigned int *, unsigned int *);
static unsigned int get_time (struct dsc$descriptor_s *qual, char *timearg);
static unsigned int check_cli (struct dsc$descriptor_s *);
static int verbose_command = 0;


#ifdef TEST

char errbuf[FNMAX+4081]; /* Handy place to build error messages */

void ziperr( int c, ZCONST char *h)    /* Error message display function. */
{
/* int c: error code from the ZE_ class */
/* char *h: message about how it happened */

printf( "%d: %s\n", c, h);
}


int
main(int argc, char **argv)     /* Main program. */
{
    return (vms_zip_cmdline(&argc, &argv));
}

#endif /* def TEST */



/* Global storage. */
static char *the_cmd_line;              /* buffer for argv strings */
static unsigned int cmdl_size;          /* allocated size of buffer */
static unsigned int cmdl_len;           /* used size of buffer */


static int append_simple_opt( char *opt)
{
    int x;

    x = cmdl_len;
    cmdl_len += strlen( opt)+ 1;
    CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
    strcpy( &the_cmd_line[ x], opt);
}


static int get_suffix_list( struct dsc$descriptor_s *qual,
                            struct dsc$descriptor_d *w_s)
{
    int first = 1;
    int sts;
    int x;

    /* Put out "=sufx1:...:sufxn". */
    while ((sts = cli$get_value( qual, w_s))& 1)
    {
        char *wsp = w_s->dsc$a_pointer;

        /* Delimiter.  First "=", then ":". */
        x = cmdl_len;
        cmdl_len += 1;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)

        if (first == 0)
        {
            the_cmd_line[ x] = ':';
        }
        else
        {
            first = 0;
            the_cmd_line[ x] = '=';
            /* First time, for "", substitute ":". */
            if (w_s->dsc$w_length == 0)
            {
                w_s->dsc$w_length = 1;
                wsp = ":";
            }
        }

        x = cmdl_len;
        cmdl_len += w_s->dsc$w_length;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)

        strncpy( &the_cmd_line[ x], wsp, w_s->dsc$w_length);
    }

    /* NUL terminate. */
    x = cmdl_len;
    cmdl_len += 1;
    CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
    the_cmd_line[ cmdl_len] = '\0';

    return sts;
}


unsigned int
vms_zip_cmdline (int *argc_p, char ***argv_p)
{
/*
**  Routine:    vms_zip_cmdline
**
**  Function:
**
**      Parse the DCL command line and create a fake argv array to be
**      handed off to Zip.
**
**      NOTE: the argv[] is built as we go, so all the parameters are
**      checked in the appropriate order!!
**
**  Formal parameters:
**
**      argc_p          - Address of int to receive the new argc
**      argv_p          - Address of char ** to receive the argv address
**
**  Calling sequence:
**
**      status = vms_zip_cmdline (&argc, &argv);
**
**  Returns:
**
**      SS$_NORMAL      - Success.
**      SS$_INSFMEM     - A malloc() or realloc() failed
**      SS$_ABORT       - Bad time value
**
*/
    int flag;
    unsigned int status;
    char options[ 64];
    char *ptr;
    int x, len;

    int new_argc;
    char **new_argv;

    struct dsc$descriptor_d work_str = DSC_D_INIT;
    struct dsc$descriptor_d foreign_cmdline = DSC_D_INIT;

    /*
    **  See if the program was invoked by the CLI (SET COMMAND) or by
    **  a foreign command definition.  Check for /YYZ_ZIP, which is a
    **  valid default qualifier solely for this test.
    */
    show_VMSCLI_help = TRUE;
    status = check_cli(&cli_yyz);
    if (!(status & 1)) {
        lib$get_foreign(&foreign_cmdline);
        /*
        **  If nothing was returned or the first character is a "-", then
        **  assume it's a UNIX-style command and return.
        */
        if (foreign_cmdline.dsc$w_length == 0)
            return (SS$_NORMAL);
        if ((*(foreign_cmdline.dsc$a_pointer) == '-') ||
            ((foreign_cmdline.dsc$w_length > 1) &&
             (*(foreign_cmdline.dsc$a_pointer) == '"') &&
             (*(foreign_cmdline.dsc$a_pointer + 1) == '-'))) {
            show_VMSCLI_help = FALSE;
            return (SS$_NORMAL);
        }

        str$concat(&work_str, &zip_command, &foreign_cmdline);
        status = cli$dcl_parse(&work_str, &zip_clitable, lib$get_input,
                        lib$get_input, 0);
        if (!(status & 1)) return (status);
    }

    /*
    **  There will always be a new_argv[] because of the image name.
    */
    if ((the_cmd_line = (char *) malloc(cmdl_size = ARGBSIZE_UNIT)) == NULL)
        RETURN_INSFMEM( 1);

    strcpy(the_cmd_line, "zip");
    cmdl_len = sizeof("zip");

    /*
    **  Convert CLI options into UNIX-style options.
    */

    options[0] = '-';
    ptr = &options[1];          /* Point to temporary buffer */

    /*
    **  Copy entries.
    */
#define OPT_U_  "-U"            /* Copy entries. */

    status = cli$present( &cli_copy_entries);
    if (status & 1)
    {
        /* /COPY_ENTRIES */
        append_simple_opt( OPT_U_);
    }

    /*
    **  Delete the specified files from the zip file?
    */
#define OPT_D   "-d"            /* Delete entries. */

    status = cli$present( &cli_delete);
    if (status & 1)
    {
        /* /DELETE */
        append_simple_opt( OPT_D);
    }

    /*
    **  Freshen (only changed files).
    */
#define OPT_F   "-f"            /* Freshen entries. */

    status = cli$present( &cli_freshen);
    if (status & 1)
    {
        /* /FRESHEN */
        append_simple_opt( OPT_F);
    }

    /*
    **  Delete the files once they've been added to the zip file.
    */
#define OPT_M   "-m"            /* Move entries. */

    status = cli$present( &cli_move);
    if (status & 1)
    {
        /* /MOVE */
        append_simple_opt( OPT_M);
     }

    /*
    **  Add changed and new files.
    */
#define OPT_U   "-u"            /* Update entries. */

    status = cli$present( &cli_update);
    if (status & 1)
    {
        /* /UPDATE */
        append_simple_opt( OPT_U);
     }

    /*
    **  Adjust offsets of zip archive entries.
    */
#define OPT_A   "-A"            /* Adjust offsets. */

    status = cli$present( &cli_adjust);
    if (status & 1)
    {
        /* /ADJUST_OFFSETS */
        append_simple_opt( OPT_U);
    }


    /*
    **  Enable argument files.
    */
#define OPT_AF  "-AF"           /* Enable. */
#define OPT_AFN "-AF-"          /* Disable. */

    status = cli$present( &cli_argfiles);
    if ((status & 1) || (status == CLI$_NEGATED))
    {
        if (status == CLI$_NEGATED)
        {
            /* /NOARGFILES */
            append_simple_opt( OPT_AFN);
        }
        else
        {
            /* /ARGFILES */
            append_simple_opt( OPT_AF);
        }
    }

    /*
    **  Backup path, type.
    */
#ifdef BACKUP_SUPPORT

#define OPT_BC  "-BC"           /* Backup control directory. */
#define OPT_BD  "-BD"           /* Backup directory. */
#define OPT_BL  "-BL"           /* Backup log [=directory]. */
#define OPT_BN  "-BN"           /* Backup name. */
#define OPT_BT  "-BT"           /* Backup type. */
#define OPT_BTD "diff"          /* Backup type differential. */
#define OPT_BTF "full"          /* Backup type full. */
#define OPT_BTI "incr"          /* Backup type incremental. */
#define OPT_BTN "none"          /* Backup type none. */

    status = cli$present( &cli_backup);
    if (status & 1)
    {
        char *opt = NULL;

        if ((status = cli$present(&cli_backup_control)) & 1)
        {
            /* /BACKUP = CONTROL = dir */
            append_simple_opt( OPT_BC);

            if (cli$get_value( &cli_backup_control, &work_str) & 1)
            {
                x = cmdl_len;
                cmdl_len += work_str.dsc$w_length+ 1;
                CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                strncpy( &the_cmd_line[ x], work_str.dsc$a_pointer,
                 work_str.dsc$w_length);
                the_cmd_line[ cmdl_len- 1] = '\0';
            }
        }

        if ((status = cli$present(&cli_backup_dir)) & 1)
        {
            /* /BACKUP = DIRECTORY = dir */
            append_simple_opt( OPT_BD);

            if (cli$get_value( &cli_backup_dir, &work_str) & 1)
            {
                x = cmdl_len;
                cmdl_len += work_str.dsc$w_length+ 1;
                CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                strncpy( &the_cmd_line[ x], work_str.dsc$a_pointer,
                 work_str.dsc$w_length);
                the_cmd_line[ cmdl_len- 1] = '\0';
            }
        }

        if ((status = cli$present(&cli_backup_log)) & 1)
        {
            /* /BACKUP = LOG [= dir] */
            append_simple_opt( OPT_BL);

            if (cli$get_value( &cli_backup_log, &work_str) & 1)
            {   /* Add optional "=dir" value. */
                x = cmdl_len- 1;        /* Overwrite NUL terminator, */
                the_cmd_line[ x] = '='; /* and don't increment cmdl_len. */

                x = cmdl_len;
                cmdl_len += work_str.dsc$w_length+ 1;
                CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                strncpy( &the_cmd_line[ x], work_str.dsc$a_pointer,
                 work_str.dsc$w_length);
                the_cmd_line[ cmdl_len- 1] = '\0';
            }
        }

        if ((status = cli$present(&cli_backup_name)) & 1)
        {
            /* /BACKUP = NAME = name */
            append_simple_opt( OPT_BN);

            if (cli$get_value( &cli_backup_name, &work_str) & 1)
            {
                x = cmdl_len;
                cmdl_len += work_str.dsc$w_length+ 1;
                CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                strncpy( &the_cmd_line[ x], work_str.dsc$a_pointer,
                 work_str.dsc$w_length);
                the_cmd_line[ cmdl_len- 1] = '\0';
            }
        }

        if ((status = cli$present(&cli_backup_type)) & 1)
        {
            append_simple_opt( OPT_BT);
        }

        if ((status = cli$present(&cli_backup_type_diff)) & 1)
        {
            opt = OPT_BTD;
        }
        else if ((status = cli$present(&cli_backup_type_full)) & 1)
        {
            opt = OPT_BTF;
        }
        else if ((status = cli$present(&cli_backup_type_incr)) & 1)
        {
            opt = OPT_BTI;
        }
        else if ((status = cli$present(&cli_backup_type_none)) & 1)
        {
            opt = OPT_BTN;
        }

        if (opt != NULL)
        {
            append_simple_opt( opt);
        }
    }
#endif /* def BACKUP_SUPPORT */

    /*
    **  Add comments?
    */
#define OPT_C   "-c"            /* Comment, entry. */
#define OPT_Z   "-z"            /* Comment, archive. */

    status = cli$present( &cli_comments);
    if (status & 1)
    {
        int archive_or_zip_file = 0;

        if ((status = cli$present( &cli_comment_archive)) & 1)
            /* /COMMENTS = ARCHIVE */
            archive_or_zip_file = 1;
        if ((status = cli$present( &cli_comment_zipfile)) & 1)
            /* /COMMENTS = ZIP_FILE */
            archive_or_zip_file = 1;
        if (archive_or_zip_file != 0)
            /* /COMMENTS = ARCHIVE */
            append_simple_opt( OPT_Z);
        if ((status = cli$present(&cli_comment_files)) & 1)
            /* /COMMENTS = FILES */
            append_simple_opt( OPT_C);
    }

    /*
    **  Preserve case in file names.
    */
#define OPT_C_  "-C"            /* Preserve case all. */
#define OPT_CN  "-C-"           /* Down-case all. */
#define OPT_C2  "-C2"           /* Preserve case ODS2. */
#define OPT_C2N "-C2-"          /* Down-case ODS2. */
#define OPT_C5  "-C5"           /* Preserve case ODS5. */
#define OPT_C5N "-C5-"          /* Down-case ODS5. */

    status = cli$present( &cli_pres_case);
    if ((status & 1) || (status == CLI$_NEGATED))
    {
        /* /[NO]PRESERVE_CASE */
        char *opt;
        int ods2 = 0;
        int ods5 = 0;

        if (status == CLI$_NEGATED)
        {
            append_simple_opt( OPT_CN);
        }
        else
        {
            if (cli$present( &cli_pres_case_no2) & 1)
            {
                /* /PRESERVE_CASE = NOODS2 */
                ods2 = -1;
            }
            if (cli$present( &cli_pres_case_no5) & 1)
            {
                /* /PRESERVE_CASE = NOODS5 */
                ods5 = -1;
            }
            if (cli$present( &cli_pres_case_ods2) & 1)
            {
                /* /PRESERVE_CASE = ODS2 */
                ods2 = 1;
            }
            if (cli$present( &cli_pres_case_ods5) & 1)
            {
                /* /PRESERVE_CASE = ODS5 */
                ods5 = 1;
            }

            if (ods2 == ods5)
            {
                /* Plain "-C[-]". */
                if (ods2 < 0)
                    opt = OPT_CN;
                else
                    opt = OPT_C_;

                append_simple_opt( opt);
            }
            else
            {
                if (ods2 != 0)
                {
                    /* "-C2[-]". */
                    if (ods2 < 0)
                        opt = OPT_C2N;
                    else
                        opt = OPT_C2;

                    append_simple_opt( opt);
                }

                if (ods5 != 0)
                {
                    /* "-C5[-]". */
                    if (ods5 < 0)
                        opt = OPT_C5N;
                    else
                        opt = OPT_C5;

                    append_simple_opt( opt);
                }
            }
        }
    }

    /*
    **  Pattern case sensitivity.
    */
#define OPT_IC  "-ic"           /* Case-insensitive pattern matching. */
#define OPT_ICN "-ic-"          /* Case-sensitive pattern matching. */

    status = cli$present( &cli_patt_case);
    if (status & 1)
    {
        if (cli$present( &cli_patt_case_blind) & 1)
        {
            /* "-ic". */
            append_simple_opt( OPT_IC);
        }
        else if (cli$present( &cli_patt_case_sensitive) & 1)
        {
            /* "-ic-". */
            append_simple_opt( OPT_ICN);
        }
    }

    /*
    **  Default directory.
    */
#define OPT_CD   "-cd"          /* Set default directory. */

    status = cli$present( &cli_default_dir);
    if (status & 1)
    {
        /* /DEFAULT_DIRECTORY */
        status = cli$get_value( &cli_default_dir, &work_str);
        if (status & 1)
        {
            /* /DEFAULT_DIRECTORY = value (required argument) */
            work_str.dsc$a_pointer[work_str.dsc$w_length] = '\0';
            append_simple_opt( OPT_CD);
            append_simple_opt( work_str.dsc$a_pointer);
        }
    }

    /*
    **  Data descriptors.
    */
#define OPT_FD  "-fd"           /* Force descriptors. */

    status = cli$present( &cli_descriptors);
    if (status & 1)
    {
        /* /DESCRIPTORS */
        append_simple_opt( OPT_FD);
    }

    /*
    **  Difference archive.  Add only new or changed files.
    */
#define OPT_DF  "-DF"           /* Difference archive. */

    if ((status = cli$present( &cli_difference)) & 1)
    {
        /* /DIFFERENCE */
        append_simple_opt( OPT_DF);
    }

    /*
    **  Do not add/modify directory entries.
    */
#define OPT_D_  "-D"            /* No directory entries. */

    status = cli$present( &cli_dirnames);
    if (!(status & 1))
    {
        /* /DIRNAMES */
        append_simple_opt( OPT_D_);
    }

    /*
    **  Encrypt?
    */
#define OPT_E     "-e"          /* Encrypt. */
#define OPT_P     "-P"          /* Password. */
#define OPT_PN    "-pn"         /* Permit non-ANSI chars in password. */
#define OPT_PNN   "-pn-"        /* Permit only ANSI chars in password. */
#define OPT_PS    "-ps"         /* Permit short password. */
#define OPT_PSN   "-ps-"        /* Permit only non-short password. */
#define OPT_Y_    "-Y"          /* Method. */
#define OPT_YA128 "AES128"      /* Method AES128. */
#define OPT_YA192 "AES192"      /* Method AES192. */
#define OPT_YA256 "AES256"      /* Method AES256. */
#define OPT_YTRAD "Traditional" /* Method Traditional. */

    status = cli$present( &cli_encrypt);
    if (status & 1)
    {
        char *opt;

        /* /ENCRYPT. */
        if (cli$present( &cli_encrypt_pass) & 1)
        {
            /* /ENCRYPT = PASSWORD = "pwd". */
            append_simple_opt( OPT_P);

            if (cli$get_value( &cli_encrypt_pass, &work_str) & 1)
            {
                x = cmdl_len;
                cmdl_len += work_str.dsc$w_length+ 1;
                CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                strncpy( &the_cmd_line[ x], work_str.dsc$a_pointer,
                 work_str.dsc$w_length);
                the_cmd_line[ cmdl_len- 1] = '\0';
            }
        }
        else
        {
            /* /ENCRYPT (no password). */
            append_simple_opt( OPT_E);
        }

        status = cli$present( &cli_encrypt_ansi);
        if ((status & 1) || (status == CLI$_NEGATED))
        {
            /* /ENCRYPT = [NO]ANSI_PASSWORD */
            if (status == CLI$_NEGATED)
            {
                /* /ENCRYPT = NOANSI_PASSWORD */
                opt = OPT_PN;
            }
            else
            {
                /* /ENCRYPT = ANSI_PASSWORD */
                opt = OPT_PNN;
            }
            append_simple_opt( opt);
        }

        if (cli$present( &cli_encrypt_mthd) & 1)
        {
            /* /ENCRYPT = METHOD = mthd. */
            append_simple_opt( OPT_Y_);

            if (cli$present( &cli_encrypt_mthd_aes128) & 1)
            {   /* AES128. */
                opt = OPT_YA128;
            }
            else if (cli$present( &cli_encrypt_mthd_aes192) & 1)
            {  /* AES192. */
                opt = OPT_YA192;
            }
            else if (cli$present( &cli_encrypt_mthd_aes256) & 1)
            {   /* AES256. */
                opt = OPT_YA256;
            }
            else
            {   /* Traditional. */
                opt = OPT_YTRAD;
            }
            append_simple_opt( opt);
        }

        status = cli$present( &cli_encrypt_short);
        if ((status & 1) || (status == CLI$_NEGATED))
        {
            /* /ENCRYPT = [NO]SHORT_PASSWORD */
            if (status == CLI$_NEGATED)
            {
                /* /ENCRYPT = NOSHORT_PASSWORD */
                opt = OPT_PS;
            }
            else
            {
                /* /ENCRYPT = SHORT_PASSWORD */
                opt = OPT_PSN;
            }
            append_simple_opt( opt);
        }
    }

    /*
    **  Fix the zip archive structure.
    */
#define OPT_F_  "-F"            /* Fix archive. */
#define OPT_FF  "-FF"           /* Fix archive more. */

    status = cli$present( &cli_fix_archive);
    if (status & 1)
    {
        char *opt;

        opt = OPT_F_;
        /* /FIX_ARCHIVE = NORMAL */
        if ((status = cli$present( &cli_fix_full)) & 1)
        {
            /* /FIX_ARCHIVE = FULL */
            opt = OPT_FF;
        }
        append_simple_opt( opt);
    }

    /*
    **  Filesync.  Delete archive entry if no such file.
    */
#define OPT_FS  "-FS"           /* Filesync. */

    if ((status = cli$present( &cli_filesync)) & 1)
    {
        /* /FILESYNC */
        append_simple_opt( OPT_FS);
    }

    /*
    **  Append (allow growing of existing zip file).
    */
#define OPT_G   "-g"            /* Grow (append to) archive. */

    status = cli$present( &cli_append);
    if (status & 1)
    {
        /* /APPEND */
        append_simple_opt( OPT_G);
    }

    status = cli$present( &cli_grow);
    if (status & 1)
    {
        /* /GROW */
        append_simple_opt( OPT_G);
    }

    /*
    **  Show the help.
    */
#define OPT_H   "-h"            /* Help. */
#define OPT_H2  "-h2"           /* Help, extended. */

    status = cli$present( &cli_help);
    if (status & 1)
    {
        status = cli$present( &cli_help_normal);
        if (status & 1)
        {
            /* /HELP [= NORMAL] */
            append_simple_opt( OPT_H);
        }
        status = cli$present( &cli_help_extended);
        if (status & 1)
        {
            /* /HELP = EXTENDED */
            append_simple_opt( OPT_H2);
        }
    }

    /*
    **  Junk path names (directory specs).
    */
#define OPT_J   "-j"

    status = cli$present( &cli_junk);
    if (status & 1)
    {
        /* /JUNK */
        append_simple_opt( OPT_J);
    }

    /*
    **  Store full path or not.  (/[NO]FULL_PATH, /[NO]JUNK.)
    */
#define OPT_JN  "-j-"

    flag = 0;

    /* /[NO]FULL_PATH */
    status = cli$present( &cli_full_path);
    if (status == CLI$_PRESENT)
        flag = 1;
    else if (status == CLI$_NEGATED)
        flag = -1;

    /* /[NO]JUNK */
    status = cli$present( &cli_junk);
    if (status == CLI$_PRESENT)
        flag = -1;
    else if (status == CLI$_NEGATED)
        flag = 1;

    if (flag > 0)
    {
        /* /FULL_PATH, /NOJUNK, -j- */
        append_simple_opt( OPT_JN);
    }
    else if (flag < 0)
    {
        /* /NOFULL_PATH, /JUNK, -j */
        append_simple_opt( OPT_J);
    }

    /*
    **  Simulate zip file made by PKZIP.
    */
#define OPT_K   "-k"            /* PKZIP mode. */

    status = cli$present( &cli_pkzip);
    if (status & 1)
    {
        /* /KEEP_VERSION */
        append_simple_opt( OPT_K);
    }

    /*
    **  Translate end-of-line.
    */
#define OPT_L   "-l"            /* LF -> CR+LF. */
#define OPT_LL  "-ll"           /* CR+LF -> LF. */

    status = cli$present( &cli_translate_eol);
    if (status & 1)
    {
        /* /TRANSLATE_EOL [= LF]*/
        char *opt;

        opt = OPT_L;
        if ((status = cli$present( &cli_transl_eol_crlf)) & 1)
        {
            /* /TRANSLATE_EOL = CRLF */
            opt = OPT_LL;
        }
        append_simple_opt( opt);
    }

    /*
    **  Show the software license.
    */
#define OPT_L_  "-L"            /* Show license. */

    status = cli$present(&cli_license);
    if (status & 1)
    {
        /* /LICENSE */
        append_simple_opt( OPT_L_);
    }

    /*
    **  Set zip file time to time of latest file in it.
    */
#define OPT_O   "-o"            /* Set archive age to that of oldest member. */

    status = cli$present( &cli_latest);
    if (status & 1)
    {
        /* /LATEST */
        append_simple_opt( OPT_O);
    }

    /*
    **  Junk Zipfile prefix (SFX stub etc.).
    */
#define OPT_J_  "-J"            /* Junk SFX prolog. */

    status = cli$present( &cli_unsfx);
    if (status & 1)
    {
        /* /UNSFX */
        append_simple_opt( OPT_J_);
    }

    /*
    **  Add path prefix.
    */
#define OPT_PA  "-pa"           /* Add prefix to new (added/updated) members. */
#define OPT_PP  "-pp"           /* Add prefix to all members. */

    status = cli$present( &cli_prefix);
    if (status & 1)
    {
        char *opt = NULL;

        if ((status = cli$present( &cli_prefix_all)) & 1)
        {
            /* /PREFIX = ALL */
            opt = OPT_PP;
        }
        if ((status = cli$present( &cli_prefix_new)) & 1)
        {
            /* /PREFIX = NEW */
            opt = OPT_PA;
        }
        if (opt == NULL)
        {
            /* /PREFIX = NEW (default) */
            opt = OPT_PA;
        }
        status = cli$get_value( &cli_prefix_path, &work_str);
        if (status & 1)
        {
            append_simple_opt( opt);                    /* -pa or -pp */
            /* /PREFIX = PATH = value */
            work_str.dsc$a_pointer[work_str.dsc$w_length] = '\0';
            append_simple_opt( work_str.dsc$a_pointer); /* path prefix */
        }
    }

    /*
    **  Recurse through subdirectories.
    */
#define OPT_R   "-r"            /* Recurse. */
#define OPT_R_  "-R"            /* Recurse, PKZIP mode. */

    status = cli$present( &cli_recurse);
    if (status & 1)
    {
        if ((status = cli$present( &cli_recurse_fnames)) & 1)
        {
            /* /RECURSE [= PATH] */
            append_simple_opt( OPT_R_);
        }
        else
        {
            /* /RECURSE [= FILENAMES] */
            append_simple_opt( OPT_R);
        }
    }

    /*
    **  Statistics.
    */
#define OPT_PT  "-pt"           /* Statistics, time. */

    status = cli$present( &cli_statistics);
    if (status & 1)
    {
        if ((status = cli$present( &cli_statistics_time)) & 1)
        {
            /* /STATISTICS = TIME */
            append_simple_opt( OPT_PT);
        }
    }

    /*
    **  Test Zipfile.
    */
#define OPT_T_  "-T"            /* Test. */

    status = cli$present(&cli_test);
    if (status & 1)
    {
        /* /TEST */
        append_simple_opt( OPT_T_);
    }

    /*
    **  Be verbose.
    */
#define OPT_V   "-v"            /* Verbose (normal). */
#define OPT_VV  "-vv"           /* Verbose (more). */

    status = cli$present( &cli_verbose);
    if (status & 1)
    {
        int i;
        char *opt = NULL;

        /* /VERBOSE */
        if ((status = cli$present( &cli_verbose_command)) & 1)
        {
            /* /VERBOSE = COMMAND */
            verbose_command = 1;
        }

        /* Note that any or all of the following options may be
         * specified, and the maximum one is used.
         */
        if ((status = cli$present( &cli_verbose_normal)) & 1)
            /* /VERBOSE [ = NORMAL ] */
            opt = OPT_V;
        if ((status = cli$present( &cli_verbose_more)) & 1)
            /* /VERBOSE = MORE */
            opt = OPT_VV;

        if (opt != NULL)
        {
            append_simple_opt( opt);
        }
    }

    /*
    **  Quiet mode.
    **  (Quiet mode is processed after verbose, because a "-v" modifier
    **  resets "noisy" to 1.)
    */
#define OPT_Q   "-q"            /* Quiet. */

    status = cli$present( &cli_quiet);
    if (status & 1)
    {
        /* /QUIET */
        append_simple_opt( OPT_Q);
    }

    /*
    **  Show version.
    */
#define OPT_VQ  "-vq"           /* Version, brief. */

    status = cli$present( &cli_version);
    if (status & 1)
    {
        int i;
        char *opt = NULL;

        /* /VERSION */
        if ((status = cli$present( &cli_version_brief)) & 1)
        {
            /* /VERSION = BRIEF */
            opt = OPT_VQ;
        }
        else
        {
            /* /VERSION = NORMAL */
            opt = OPT_V;
        }
        append_simple_opt( opt);
    }

    /*
    **  Save the VMS file attributes (and all allocated blocks?).
    */
#define OPT_V_  "-V"            /* Save VMS attributes. */
#define OPT_VV_ "-VV"           /* Save VMS attributes and all blocks. */

    status = cli$present( &cli_vms);
    if (status & 1)
    {
        /* /VMS [= ALL] */
        if ((status = cli$present(&cli_vms_all)) & 1)
        {
            /* /VMS = ALL */
            append_simple_opt( OPT_VV_);
        }
        else
        {
            /* /VMS */
            append_simple_opt( OPT_V_);
        }
    }

    /*
    **  Preserve idiosyncratic VMS file names.
    */
#define OPT_VN  "-vn"           /* Preserve idiosyncratic VMS file names. */
#define OPT_VNN "-vn-"          /* Adjust idiosyncratic VMS file names. */

    status = cli$present( &cli_vnames);
    if (status == CLI$_PRESENT)
    {
        /* /VNAMES */
        append_simple_opt( OPT_VN);
    }
    else if (status == CLI$_NEGATED)
    {
        /* /NOVNAMES */
        append_simple_opt( OPT_VNN);
    }

    /*
    **  Volume label.
    */
#define OPT_DLR  "-$"           /* Record volume label. */
#define OPT_DLRN "-$-"          /* Ignore volume label. */

    status = cli$present( &cli_volume_label);
    if (status == CLI$_PRESENT)
    {
        /* /VOLUME_LABEL */
        append_simple_opt( OPT_DLR);
    }
    else if (status == CLI$_NEGATED)
    {
        /* /NOVOLUME_LABEL */
        append_simple_opt( OPT_DLRN);
    }

    /*
    **  Keep the VMS version number as part of the file name when stored.
    */
#define OPT_W   "-w"            /* Save VMS version. */

    status = cli$present( &cli_keep_version);
    if (status & 1)
    {
        /* /KEEP_VERSION */
        append_simple_opt( OPT_W);
    }

    /*
    **  Store symlinks as symlinks.
    */
#define OPT_Y   "-y"            /* Save symbolic links. */

    status = cli$present( &cli_symlinks);
    if (status & 1)
    {
        /* /SYMLINKS */
        append_simple_opt( OPT_Y);
    }

    /*
    **  Batch processing: Read list of filenames to archive from stdin
    **  or the specified file.
    */
#define OPT_AT   "-@"           /* Read file names from stdin. */
#define OPT_ATAT "-@@"          /* Read file names from specified file. */

    status = cli$present( &cli_batch);
    if (status & 1)
    {
        /* /BATCH */
        status = cli$get_value( &cli_batch, &work_str);
        if (status & 1)
        {
            /* /BATCH = value */
            work_str.dsc$a_pointer[work_str.dsc$w_length] = '\0';
            append_simple_opt( OPT_ATAT);
            append_simple_opt( work_str.dsc$a_pointer);
        }
        else
        {
            /* /BATCH (stdin) */
            append_simple_opt( OPT_AT);
        }
    }

    /*
    **  Now copy the final options string to the_cmd_line.
    */
    len = ptr - &options[0];
    if (len > 1) {
        options[len] = '\0';
        x = cmdl_len;
        cmdl_len += len + 1;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy(&the_cmd_line[x], options);
    }

    /*
    **
    **  OK.  We've done all the simple options, so check for -b (temporary
    **  file path), -n (special suffixes), -O (output atchive file),
    **  -t (exclude before time), -Z (compression method), zipfile,
    **  files to zip, and exclude list.
    **
    */

    /*
    **  Check for the compression level (-0 through -9).
    */
    status = cli$present( &cli_level);
    if (status& 1)
    {
        /* /LEVEL = value */
        int clni;

        /* Compression level CLI descriptors. */
        struct dsc$descriptor_s *cli_level_n_p[] =
         { &cli_level_0, &cli_level_1, &cli_level_2, &cli_level_3,
           &cli_level_4, &cli_level_5, &cli_level_6, &cli_level_7,
           &cli_level_8, &cli_level_9 };

#define CLI_LEVEL_NP_SIZE \
 ((sizeof cli_level_n_p)/ (sizeof cli_level_n_p[ 0]))

        /* Loop through all compression levels.
         * Note that there's currently nothing here to avoid putting out
         * multiple "-<N>=mthd" options for the same method, if the
         * victim specifies "/LEVEL = (i: mthd, j: mthd, ...)".  We'll
         * put them out in 0-9 order, Zip will use the last one it sees.
         */
        for (clni = 0; clni < CLI_LEVEL_NP_SIZE; clni++)
        {
            if (cli$present( cli_level_n_p[ clni])& 1)
            {
                /* "-0" - "-9". */
                int first = 1;

                x = cmdl_len;
                cmdl_len += 2;
                CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                the_cmd_line[ x] = '-';
                the_cmd_line[ x+ 1] = clni+ '0';

                /* /LEVEL = n = method_list */
                while (cli$get_value( cli_level_n_p[ clni], &work_str)& 1)
                {
                    /* Delimiter.  First "=", then ":". */
                    x = cmdl_len;
                    cmdl_len += 1;
                    CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                    if (first == 0)
                    {
                        the_cmd_line[ x] = ':';
                    }
                    else
                    {
                        first = 0;
                        the_cmd_line[ x] = '=';
                    }

                    /* Compression method name. */
                    x = cmdl_len;
                    cmdl_len += work_str.dsc$w_length;
                    CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                    strncpy( &the_cmd_line[ x], work_str.dsc$a_pointer,
                     work_str.dsc$w_length);
                }

                /* NUL terminate. */
                x = cmdl_len;
                cmdl_len += 1;
                CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                the_cmd_line[ x] = '\0';
            }
        }
    }

    /*
    **  Check for the temporary path (-b).
    */
    status = cli$present(&cli_temp_path);
    if (status & 1) {
        /* /TEMP_PATH = value */
        status = cli$get_value(&cli_temp_path, &work_str);
        x = cmdl_len;
        cmdl_len += work_str.dsc$w_length + 4;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy(&the_cmd_line[x], "-b");
        strncpy(&the_cmd_line[x+3], work_str.dsc$a_pointer,
                work_str.dsc$w_length);
        the_cmd_line[cmdl_len-1] = '\0';
    }

    /*
    **  Check for the output archive name (-O).
    */
    status = cli$present(&cli_output);
    if (status & 1) {
        /* /OUTPUT = value */
        status = cli$get_value(&cli_output, &work_str);
        x = cmdl_len;
        cmdl_len += work_str.dsc$w_length + 4;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy(&the_cmd_line[x], "-O");
        strncpy(&the_cmd_line[x+3], work_str.dsc$a_pointer,
                work_str.dsc$w_length);
        the_cmd_line[cmdl_len-1] = '\0';
    }

    /*
    **  Handle "-db", "-dc", "-dd", "-ds".
    */
#define OPT_DB  "-db"
#define OPT_DC  "-dc"
#define OPT_DD  "-dd"
#define OPT_DG  "-dg"
#define OPT_DS  "-ds="
#define OPT_DU  "-du"
#define OPT_DV  "-dv"

    status = cli$present( &cli_display);
    if (status & 1)
    {
        if ((status = cli$present( &cli_display_bytes)) & 1)
        {
            /* /DISPLAY = BYTES */
            append_simple_opt( OPT_DB);
        }

        if ((status = cli$present( &cli_display_counts)) & 1)
        {
            /* /DISPLAY = COUNTS */
            append_simple_opt( OPT_DC);
        }

        if ((status = cli$present( &cli_display_dots)) & 1)
        {
            /* /DISPLAY = DOTS [= value] */
            status = cli$get_value( &cli_display_dots, &work_str);

            append_simple_opt( OPT_DD);

            /* -dd[=value] now -dd -ds=value - 5/8/05 EG */
            if (work_str.dsc$w_length > 0)
            {
                x = cmdl_len;
                cmdl_len += strlen( OPT_DS);
                CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                strcpy( &the_cmd_line[ x], OPT_DS);

                x = cmdl_len;
                cmdl_len += work_str.dsc$w_length+ 1;
                CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                strncpy( &the_cmd_line[ x],
                 work_str.dsc$a_pointer, work_str.dsc$w_length);
            }
        }

        if ((status = cli$present( &cli_display_globaldots)) & 1)
        {
            /* /DISPLAY = GLOBALDOTS */
            append_simple_opt( OPT_DG);
        }

        if ((status = cli$present( &cli_display_usize)) & 1)
        {
            /* /DISPLAY = USIZE */
            append_simple_opt( OPT_DU);
        }

        if ((status = cli$present( &cli_display_volume)) & 1)
        {
            /* /DISPLAY = VOLUME */
            append_simple_opt( OPT_DV);
        }
    }

    /*
    **  Handle "-la", "-lf", "-li".
    */
#define OPT_LA  "-la"
#define OPT_LF  "-lf"
#define OPT_LI  "-li"

    status = cli$present( &cli_log_file);
    if (status & 1)
    {
        /* /LOG_FILE */
        if ((status = cli$present( &cli_log_file_append)) & 1)
        {
            /* /LOG_FILE = APPEND */
            append_simple_opt( OPT_LA);
        }

        status = cli$present( &cli_log_file_file);
        if (status & 1)
        {
            /* /LOG_FILE = FILE = file */
            status = cli$get_value(&cli_log_file_file, &work_str);
            x = cmdl_len;
            cmdl_len += strlen( OPT_LF)+ 2+ work_str.dsc$w_length;
            CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
            strcpy( &the_cmd_line[x], OPT_LF);
            strncpy( &the_cmd_line[x+strlen( OPT_LF)+ 1],
             work_str.dsc$a_pointer, work_str.dsc$w_length);
            the_cmd_line[ cmdl_len- 1] = '\0';
        }

        if ((status = cli$present( &cli_log_file_info)) & 1)
        {
            /* /LOG = INFO */
            append_simple_opt( OPT_LI);
        }
    }

    /*
    **  Handle "-s", "-sb", "-sp", "-sv".
    */
#define OPT_S   "-s"
#define OPT_SB  "-sb"
#define OPT_SP  "-sp"
#define OPT_SV  "-sv"

    status = cli$present( &cli_split);
    if (status & 1)
    {
        status = cli$present( &cli_split_bell);
        if (status & 1)
        {
            /* /SPLIT = BELL */
            append_simple_opt( OPT_SB);
        }

        status = cli$present( &cli_split_pause);
        if (status & 1)
        {
            /* /SPLIT = PAUSE */
            append_simple_opt( OPT_SP);
        }

        status = cli$present( &cli_split_size);
        if (status & 1)
        {
            /* /SPLIT = SIZE = size */
            status = cli$get_value( &cli_split_size, &work_str);

            append_simple_opt( OPT_S);

            x = cmdl_len;
            cmdl_len += work_str.dsc$w_length+ 1;
            strncpy( &the_cmd_line[ x],
             work_str.dsc$a_pointer, work_str.dsc$w_length);
        }

        status = cli$present( &cli_split_verbose);
        if (status & 1)
        {
            /* /SPLIT = VERBOSE */
            append_simple_opt( OPT_SV);
        }
    }

    /*
    **  Handle "-sc", "-sd", "-sf", "-so", "-ss".
    */
#define OPT_SC     "-sc"
#define OPT_SD     "-sd"
#define OPT_SF     "-sf"
#define OPT_SFN    "-sf-"
#define OPT_S_F_US "-sF=usize"
#define OPT_SO     "-so"
#define OPT_SS     "-ss"

    status = cli$present( &cli_show);
    if (status & 1)
    {
        /* /SHOW */
        char *opt;

        if ((status = cli$present( &cli_show_command)) & 1)
        {
            /* /SHOW = COMMAND */
            append_simple_opt( OPT_SC);
        }

        if ((status = cli$present( &cli_show_debug)) & 1)
        {
            /* /SHOW = DEBUG */
            append_simple_opt( OPT_SD);
        }

        status = cli$present( &cli_show_files);
        if ((status & 1) || (status == CLI$_NEGATED))
        {
            /* /SHOW = FILES */
            opt = OPT_SF;
            if (status == CLI$_NEGATED)
                opt = OPT_SFN;

            append_simple_opt( opt);

            if (status == CLI$_PRESENT)
            {
                /* Still /SHOW = FILES (but not /SHOW = NOFILES). */
                if ((status = cli$present( &cli_show_files_usize)) & 1)
                {
                    /* /SHOW = FILES = USIZE */
                    append_simple_opt( OPT_S_F_US);
                }
            }
        }

        if ((status = cli$present( &cli_show_options)) & 1)
        {
            /* /SHOW = OPTIONS */
            append_simple_opt( OPT_SO);
        }

        if ((status = cli$present( &cli_show_suffixes)) & 1)
        {
            /* /SHOW = SUFFIXES */
            append_simple_opt( OPT_SS);
        }
    }

    /*
    **  Enable streaming archive features.
    */
#define OPT_ST  "-st"           /* Enable. */
#define OPT_STN "-st-"          /* Disable. */

    status = cli$present( &cli_argfiles);
    if ((status & 1) || (status == CLI$_NEGATED))
    {
        if (status == CLI$_NEGATED)
        {
            /* /NOSTREAM */
            append_simple_opt( OPT_STN);
        }
        else
        {
            /* /STREAM */
            append_simple_opt( OPT_ST);
        }
    }

    /*
    **  Handle "-fz".
    */
#define OPT_FZ  "-fz"           /* Force Zip64 format. */
#define OPT_FZN "-fz-"          /* Do not force Zip64 format. */

    /* /[NO]ZIP64 */
    status = cli$present( &cli_zip64);
    if ((status & 1) || (status == CLI$_NEGATED))
    {
        char *opt;

        if (status == CLI$_NEGATED)
            /* /NOZIP64 */
            opt = OPT_FZN;
        else
            /* /ZIP64 */
            opt = OPT_FZ;

        append_simple_opt( opt);
    }

    /*
    **  Handle "-nw" and "-W".
    */
#define OPT_NW  "-nw"
#define OPT_W_  "-W"

    status = cli$present( &cli_wildcard);
    if (status & 1)
    {
        if ((status = cli$present( &cli_wildcard_nospan)) & 1)
        {
            /* /WILDCARD = NOSPAN */
            append_simple_opt( OPT_W_);
        }
    }
    else if (status == CLI$_NEGATED)
    {
        /* /NOWILDCARD */
        append_simple_opt( OPT_NW);
    }

    /*
    **  Handle "-MM".
    */
#define OPT_MM  "-MM"

    status = cli$present( &cli_must_match);
    if (status & 1)
    {
        /* /MUST_MATCH */
        append_simple_opt( OPT_MM);
    }

    /*
    **  UnZip command for archive test.
    */
#define OPT_TT_ "-TT"

    status = cli$present( &cli_test);
    if (status & 1)
    {
        /* /TEST */
        status = cli$present(&cli_test_unzip);
        if (status & 1)
        {
            /* /TEST = UNZIP = value */
            status = cli$get_value( &cli_test_unzip, &work_str);
            x = cmdl_len;
            cmdl_len += strlen( OPT_TT_)+ 2+ work_str.dsc$w_length;
            CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
            strcpy(&the_cmd_line[x], OPT_TT_);
            strncpy(&the_cmd_line[ x+ strlen( OPT_TT_)+ 1],
             work_str.dsc$a_pointer, work_str.dsc$w_length);
            the_cmd_line[ cmdl_len- 1] = '\0';
        }
    }

    /*
    **  Handle "-Z" and "-n".
    */
#define OPT_N   "-n"
#define OPT_Z_  "-Z"

    status = cli$present( &cli_compression);
    if (status & 1)
    {
        char *mthd_str;
        int first;
        int have_n;
        int mthd_len;
        int cmi;

        /* Compression method-related CLI descriptors and related info. */
        struct
        {
            char *name;                         /* Method name string. */
            struct dsc$descriptor_s *mth_p;     /* method */
            struct dsc$descriptor_s *mth_l_p;   /* method.LEVEL */
            struct dsc$descriptor_s *mth_s_p;   /* method.SUFFIX */
        } cli_cmprs_info[] =
         { { "bzip2", &cli_compression_b,
              &cli_compression_b_l, &cli_compression_b_s },
           { "deflate", &cli_compression_d,
               &cli_compression_d_l,  &cli_compression_d_s },
           { "lzma", &cli_compression_l,
               &cli_compression_l_l, &cli_compression_l_s },
           { "ppmd", &cli_compression_p,
               &cli_compression_p_l, &cli_compression_p_s },
           { "store", &cli_compression_s,
               NULL, &cli_compression_s_s } };  /* Note: No STORE = LEVEL. */

#define CLI_CMPRS_INFO_SIZE \
 ((sizeof cli_cmprs_info)/ (sizeof cli_cmprs_info[ 0]))

        /* Loop through all compression methods. */
        for (cmi = 0; cmi < CLI_CMPRS_INFO_SIZE ; cmi++)
        {
            first = 1;                  /* "=" v. ":". */
            have_n = 0;                 /* "-n" v. "-Z". */

            if (cli$present( (cli_cmprs_info[ cmi]).mth_p)& 1)
            {
                /* /COMPRESSION = mthd [= ...] */
                mthd_str = (cli_cmprs_info[ cmi]).name;
                mthd_len = strlen( mthd_str);
                if ((cli_cmprs_info[ cmi]).mth_l_p != NULL)
                {
                    if (cli$present(
                     (cli_cmprs_info[ cmi]).mth_l_p)& 1)
                    {
                        /* /COMPRESSION = mthd = LEVEL = lvl [...] */
                        if (cli$get_value(
                         (cli_cmprs_info[ cmi]).mth_l_p, &work_str)& 1)
                        {
                            /* Put out "-n". */
                            have_n = 1;
                            x = cmdl_len;
                            cmdl_len += strlen( OPT_N)+ mthd_len+ 3;
                            CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size,
                             cmdl_len)
                            strcpy( &the_cmd_line[ x], OPT_N);
                            /* Put out "mthd-lvl". */
                            strcpy( &the_cmd_line[ x+ strlen( OPT_N)+ 1],
                             mthd_str);
                            the_cmd_line[ x+ strlen( OPT_N)+ 1+ mthd_len] = '-';
                            the_cmd_line[ x+ strlen( OPT_N)+ 1+ mthd_len+ 1] =
                             *work_str.dsc$a_pointer;
                        }
                    }
                }

                if (cli$present( (cli_cmprs_info[ cmi]).mth_s_p)& 1)
                {
                    /* /COMPRESSION = mthd = ([LEVEL = lvl],
                     * SUFFIX = (sufx1, ..., sufxn))
                     */
                    if (have_n == 0)
                    {
                        /* Put out "-n". */
                        have_n = 1;
                        x = cmdl_len;
                        cmdl_len += strlen( OPT_N)+ mthd_len+ 1;
                        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                        strcpy( &the_cmd_line[ x], OPT_N);
                        strcpy( &the_cmd_line[ x+ strlen( OPT_N)+ 1], mthd_str);
                    }

                    /* Put out "=sufx1:...:sufxn". */
                    status = get_suffix_list( (cli_cmprs_info[ cmi]).mth_s_p,
                                               &work_str);
                }

                /* If there were no LEVEL or SUFFIX keywords, hence no
                 * "-n" option, then put out "-Zmthd".
                 * Note that there's currently nothing here to avoid
                 * putting out multiple "-Zmthd" options, if the victim
                 * specifies "/COMPRESSION = (mthd1, mthd2, ...)".
                 * We'll put them out alphabetically, and Zip will use
                 * the last one it sees.
                 */
                if (have_n == 0)
                {
                    /* /COMPRESSION = mthd */
                    x = cmdl_len;
                    cmdl_len += strlen( OPT_Z_)+ mthd_len+ 1;
                    CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
                    strcpy( &the_cmd_line[ x], OPT_Z_);
                    strcpy( &the_cmd_line[ x+ strlen( OPT_Z_)], mthd_str);
                }
            } /* if */
        } /* for */
    }

    /*
    **  Handle "-t [mmddyyyy][:HH:MM[:SS]]" and "-t [yyyy-mm-dd][:HH:MM[:SS]]".
    */
#define OPT_T   "-t"
#define OPT_TT  "-tt"

    status = cli$present(&cli_since);
    if (status & 1) {
        /* /SINCE = value */
        char since_time[20];

        status = get_time(&cli_since, since_time);
        if (!(status & 1)) return (status);

        /*
        **  Add the option "-t yyyy-mm=dd:HH:MM:SS" to the new command line.
        */
        x = cmdl_len;
        cmdl_len += (sizeof(OPT_T) + strlen(since_time));
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy(&the_cmd_line[x], OPT_T);
        strcpy(&the_cmd_line[x + sizeof(OPT_T) - 1], since_time);
    }

    /*
    **  Handle "-tt [mmddyyyy][:HH:MM[:SS]]" and "-tt [yyyy-mm-dd][:HH:MM[:SS]]".
    */

    status = cli$present(&cli_before);
    if (status & 1) {
        /* /BEFORE = value */
        char before_time[20];

        status = get_time(&cli_before, before_time);
        if (!(status & 1)) return (status);

        /*
        **  Add the option "-tt yyyy-mm-dd:HH:MM:SS" to the new command line.
        */
        x = cmdl_len;
        cmdl_len += (sizeof(OPT_TT) + strlen(before_time));
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy(&the_cmd_line[x], OPT_TT);
        strcpy(&the_cmd_line[x + sizeof(OPT_TT)- 1], before_time);
    }

    /*
    **  Handle "-n suffix:suffix:...".  (File types to store only.)
    */
#define OPT_STORE "store"

    status = cli$present( &cli_store_types);
    if (status & 1)
    {
        /* /STORE_TYPES = value_list */
#if 0
        /* 2012-03-23 SMS.
         * get_list() can't transform "" into ":", so use the same scheme
         * as for /COMPRESSION = mthd = SUFFIX = (sufx1, ..., sufxn).
         */
        x = cmdl_len;
        cmdl_len += 3;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy(&the_cmd_line[x], OPT_N);
        status = get_list(&cli_store_types, &foreign_cmdline, ':',
                          &the_cmd_line, &cmdl_size, &cmdl_len);
        if (!(status & 1)) return (status);
#endif /* 0 */

        x = cmdl_len;
        cmdl_len += strlen( OPT_N)+ strlen( OPT_STORE)+ 1;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy( &the_cmd_line[ x], OPT_N);
        /* Put out "mthd-lvl" ("store"). */
        strcpy( &the_cmd_line[ x+ strlen( OPT_N)+ 1], OPT_STORE);

        /* Put out "=sufx1:...:sufxn". */
        status = get_suffix_list( &cli_store_types, &work_str);
    }

    /*
    **  Handle "-X", keep or strip extra fields.
    */
#define OPT_X_  "-X"
#define OPT_XN  "-X-"

    status = cli$present(&cli_extra_fields);
    if (status & 1) {
        /* /EXTRA_FIELDS */
        if ((status = cli$present( &cli_extra_fields_keep)) & 1) {
            /* /EXTRA_FIELDS = KEEP_EXISTING */
            append_simple_opt( OPT_XN);
        }
    }
    else if (status == CLI$_NEGATED) {
        /* /NOEXTRA_FIELDS */
        append_simple_opt( OPT_X_);
    }

    /*
    **  Now get the specified zip file name.
    */
    status = cli$present(&cli_zipfile);
    /* zipfile */
    if (status & 1) {
        status = cli$get_value(&cli_zipfile, &work_str);

        x = cmdl_len;
        cmdl_len += work_str.dsc$w_length + 1;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strncpy(&the_cmd_line[x], work_str.dsc$a_pointer,
                work_str.dsc$w_length);
        the_cmd_line[cmdl_len-1] = '\0';

    }

    /*
    **  Run through the list of input files.
    */
    status = cli$present(&cli_infile);
    if (status & 1) {
        /* infile_list */
        status = get_list(&cli_infile, &foreign_cmdline, '\0',
                          &the_cmd_line, &cmdl_size, &cmdl_len);
        if (!(status & 1)) return (status);
    }

    /*
    **  List file containing exclude patterns present? ("-x@exclude.lst")
    */
#define OPT_X   "-x"
#define OPT_XAT "-x@"

    status = cli$present( &cli_exlist);
    if (status & 1)
    {
        /* /EXLIST = list */
        status = cli$get_value( &cli_exlist, &work_str);
        x = cmdl_len;
        cmdl_len += strlen( OPT_XAT)+ work_str.dsc$w_length+ 1;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy( &the_cmd_line[ x], OPT_XAT);
        strncpy( &the_cmd_line[ x+ strlen( OPT_XAT)],
         work_str.dsc$a_pointer, work_str.dsc$w_length);
        the_cmd_line[ cmdl_len- 1] = '\0';
    }

    /*
    **  Any files to exclude? ("-x file file")
    */
    status = cli$present( &cli_exclude);
    if (status & 1)
    {
        /* /EXCLUDE = list */
        x = cmdl_len;
        cmdl_len += strlen( OPT_X)+ 1;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy( &the_cmd_line[x], OPT_X);

        status = get_list( &cli_exclude, &foreign_cmdline, '\0',
                           &the_cmd_line, &cmdl_size, &cmdl_len);
        if (!(status & 1)) return (status);
    }

    /*
    **  List file containing include patterns present? ("-x@exclude.lst")
    */
#define OPT_I   "-i"
#define OPT_IAT "-i@"

    status = cli$present( &cli_inlist);
    if (status & 1)
    {
        /* /INLIST = list */
        status = cli$get_value( &cli_inlist, &work_str);
        x = cmdl_len;
        cmdl_len += strlen( OPT_IAT)+ work_str.dsc$w_length + 1;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy( &the_cmd_line[ x], OPT_IAT);
        strncpy( &the_cmd_line[ x+ strlen( OPT_IAT)],
         work_str.dsc$a_pointer, work_str.dsc$w_length);
        the_cmd_line[ cmdl_len- 1] = '\0';
    }

    /*
    **  Any files to include? ("-i file file")
    */
    status = cli$present(&cli_include);
    if (status & 1)
    {
        /* /INCLUDE = list */
        x = cmdl_len;
        cmdl_len += strlen( OPT_I)+ 1;
        CHECK_BUF_ALLOC( the_cmd_line, &cmdl_size, cmdl_len)
        strcpy( &the_cmd_line[ x], OPT_I);

        status = get_list( &cli_exclude, &foreign_cmdline, '\0',
                           &the_cmd_line, &cmdl_size, &cmdl_len);
        if (!(status & 1)) return (status);
    }


    /*
    **  We have finished collecting the strings for the argv vector,
    **  release unused space.
    */
    if ((the_cmd_line = (char *) realloc(the_cmd_line, cmdl_len)) == NULL)
        RETURN_INSFMEM( 2);

    /*
    **  Now that we've built our new UNIX-like command line, count the
    **  number of args and build an argv array.
    */
    for (new_argc = 0, x = 0; x < cmdl_len; x++)
        if (the_cmd_line[x] == '\0')
            new_argc++;

    /*
    **  Allocate memory for the new argv[].  The last element of argv[]
    **  is supposed to be NULL, so allocate enough for new_argc+1.
    */
    if ((new_argv = (char **) calloc(new_argc+1, sizeof(char *))) == NULL)
        RETURN_INSFMEM( 3);

    /*
    **  For each option, store the address in new_argv[] and convert the
    **  separating blanks to nulls so each argv[] string is terminated.
    */
    for (ptr = the_cmd_line, x = 0; x < new_argc; x++)
    {
        new_argv[ x] = ptr;
        ptr += strlen( ptr)+ 1;
    }
    new_argv[ new_argc] = NULL;

#if defined(TEST) || defined(DEBUG)
    printf("new_argc    = %d\n", new_argc);
    for (x = 0; x < new_argc; x++)
        printf("new_argv[%d] = %s\n", x, new_argv[x]);
#endif /* TEST || DEBUG */

    /* Show the complete UNIX command line, if requested. */
    if (verbose_command != 0)
    {
        printf( "   UNIX command line args (argc = %d):\n", new_argc);
        for (x = 0; x < new_argc; x++)
            printf( "%s\n", new_argv[ x]);
        printf( "\n");
    }

    /*
    **  All finished.  Return the new argc and argv[] addresses to Zip.
    */
    *argc_p = new_argc;
    *argv_p = new_argv;

    return (SS$_NORMAL);
}



static unsigned int
get_list (struct dsc$descriptor_s *qual, struct dsc$descriptor_d *rawtail,
          int delim, char **p_str, unsigned int *p_size, unsigned int *p_end)
{
/*
**  Routine:    get_list
**
**  Function:   This routine runs through a comma-separated CLI list
**              and copies the strings to the argv buffer.  The
**              specified separation character is used to separate
**              the strings in the argv buffer.
**
**              All unquoted strings are converted to lower-case.
**
**  Formal parameters:
**
**      qual    - Address of descriptor for the qualifier name
**      rawtail - Address of descriptor for the full command line tail
**      delim   - Character to use to separate the list items
**      p_str   - Address of pointer pointing to output buffer (argv strings)
**      p_size  - Address of number containing allocated size for output string
**      p_end   - Address of number containing used length in output buf
**
*/

    unsigned int status;
    struct dsc$descriptor_d work_str = DSC_D_INIT;

    status = cli$present(qual);
    if (status & 1) {

        unsigned int len, old_len;
        int ind, sind;
        int keep_case;
        char *src, *dst; int x;

        /*
        **  Just in case the string doesn't exist yet, though it does.
        */
        if (*p_str == NULL) {
            *p_size = ARGBSIZE_UNIT;
            if ((*p_str = (char *) malloc(*p_size)) == NULL)
                RETURN_INSFMEM( 4);
            len = 0;
        } else {
            len = *p_end;
        }

        while ((status = cli$get_value(qual, &work_str)) & 1) {
            old_len = len;
            len += work_str.dsc$w_length + 1;
            CHECK_BUF_ALLOC( *p_str, p_size, len)

            /*
            **  Look for the filename in the original foreign command
            **  line to see if it was originally quoted.  If so, then
            **  don't convert it to lowercase.
            */
            keep_case = FALSE;
            str$find_first_substring(rawtail, &ind, &sind, &work_str);
            if ((ind > 1 && *(rawtail->dsc$a_pointer + ind - 2) == '"') ||
                (ind == 0))
                keep_case = TRUE;

            /*
            **  Copy the string to the buffer, converting to lowercase.
            */
            src = work_str.dsc$a_pointer;
            dst = *p_str+old_len;
            for (x = 0; x < work_str.dsc$w_length; x++) {
                if (!keep_case && ((*src >= 'A') && (*src <= 'Z')))
                    *dst++ = *src++ + 32;
                else
                    *dst++ = *src++;
            }
            if (status == CLI$_COMMA)
                (*p_str)[len-1] = (char)delim;
            else
                (*p_str)[len-1] = '\0';
        }
        *p_end = len;
    }

    return (SS$_NORMAL);

}


static unsigned int
get_time (struct dsc$descriptor_s *qual, char *timearg)
{
/*
**  Routine:    get_time
**
**  Function:   This routine reads the argument string of the qualifier
**              "qual" that should be a VMS syntax date-time string. 
**              The date-time string is converted into the standard
**              format "yyyy-mm-dd:HH:MM:SS" ("mmddyyyy" also allowed),
**              specifying an absolute date-time.  The converted string
**              is written into the 20- (formerly 9-) byte-wide buffer,
**              "timearg".
**
**  Formal parameters:
**
**      qual    - Address of descriptor for the qualifier name
**      timearg - Address of a buffer carrying the 8-char time string returned
**
*/

    unsigned int status;
    struct dsc$descriptor_d time_str = DSC_D_INIT;
    struct quadword {
        int high;
        int low;
    } bintimbuf = {0,0};
#ifdef __DECC
#pragma member_alignment save
#pragma nomember_alignment
#endif  /* __DECC */
    struct tim {
        unsigned short year;
        unsigned short month;
        unsigned short day;
        unsigned short hour;
        unsigned short minute;
        unsigned short second;
        unsigned short hundred;
    } numtimbuf;
#ifdef __DECC
#pragma member_alignment restore
#endif

    status = cli$get_value(qual, &time_str);
    /*
    **  If a date is given, convert it to 64-bit binary.
    */
    if (time_str.dsc$w_length) {
        status = sys$bintim(&time_str, &bintimbuf);
        if (!(status & 1)) return (status);
        str$free1_dx(&time_str);
    }
    /*
    **  Now call $NUMTIM to get the month, day, and year.
    */
    status = sys$numtim(&numtimbuf, (bintimbuf.low ? &bintimbuf : NULL));
    /*
    **  Write the "yyyy-mm-dd:HH:MM:SS" string to the return buffer.
    **  (With a little care, we could trim insignificant parts.)
    */
    if (!(status & 1)) {
        *timearg = '\0';
    } else {
        sprintf( timearg, "%04u-%02u-%02u:%02u:%02u:%02u",
         numtimbuf.year, numtimbuf.month, numtimbuf.day,
         numtimbuf.hour, numtimbuf.minute, numtimbuf.second);
    }
    return status;
}


static unsigned int
check_cli (struct dsc$descriptor_s *qual)
{
/*
**  Routine:    check_cli
**
**  Function:   Check to see if a CLD was used to invoke the program.
**
**  Formal parameters:
**
**      qual    - Address of descriptor for qualifier name to check.
**
*/
    lib$establish(lib$sig_to_ret);      /* Establish condition handler */
    return (cli$present(qual));         /* Just see if something was given */
}


#ifndef TEST

void VMSCLI_help(void)  /* VMSCLI version */
/* Print help (along with license info) to stdout. */
{
  extent i;             /* counter for help array */

  /* help array */
  static char *text[] = {
"Zip %s (%s). Usage: (zip :== $ dev:[dir]zip_cli.exe)",
"zip archive[.zip] [list] [/EXCL=(xlist)] /options /modifiers",
"  The default action is to add or replace archive entries from list, except",
"  those in xlist. The include file list may contain the special name \"-\" to",
"  compress standard input.  If both archive and list are omitted, Zip",
"  compresses stdin to stdout.",
"  Type zip -h for Unix-style flags.",
"  Major options include:",
"    /COPY, /DELETE, /DIFFERENCE, /FILESYNC, /FRESHEN, /GROW, /MOVE, /UPDATE,",
#ifdef BACKUP_SUPPORT
"    /BACKUP=(TYPE={DIFFERENTIAL|FULL|INCREMENTAL}, PATH=path),",
#endif /* def BACKUP_SUPPORT */
"    /ADJUST_OFFSETS, /FIX_ARCHIVE[={NORMAL|FULL}], /TEST[=UNZIP=cmd], /UNSFX,",
"  Modifiers include:",
"    /BATCH[=list_file], /BEFORE=creation_time, /COMMENTS[={ARCHIVE|FILES}],",
"    /EXCLUDE=(file_list), /EXLIST=file, /INCLUDE=(file_list), /INLIST=file,",
"    /LATEST, /OUTPUT=out_archive, /SINCE=creation_time, /TEMP_PATH=directory,",
"    /LOG_FILE=(FILE=log_file[,APPEND][,INFORMATIONAL]), /MUST_MATCH,",
"    /PATTERN_CASE={BLIND|SENSITIVE}, /NORECURSE|/RECURSE[={PATH|FILENAMES}],",
#ifdef IZ_CRYPT_ANY
"\
    /QUIET, /VERBOSE[=DEBUG], /[NO]DIRNAMES, /JUNK, /ENCRYPT[=\"pwd\"],\
",
#else /* def IZ_CRYPT_ANY */
"    /QUIET, /VERBOSE[=DEBUG], /[NO]DIRNAMES, /JUNK,",
#endif /* def IZ_CRYPT_ANY [else] */
"    /COMPRESSION=(mthd[=(SUFFIX=(sufx_list)[,LEVEL={1-9}])][,...]),",
"    /LEVEL=(0|{1-9}[=(mthd_list)][,...]), STORE_TYPES=(sufx_list),",
#ifdef BZIP2_SUPPORT
# ifdef LZMA_SUPPORT
#  ifdef PPMD_SUPPORT
"       mthd: STORE, DEFLATE, BZIP2, LZMA, PPMD",
#  else
"       mthd: STORE, DEFLATE, BZIP2, LZMA",
#  endif
# else /* def LZMA_SUPPORT */
#  ifdef PPMD_SUPPORT
"       mthd: STORE, DEFLATE, BZIP2, PPMD",
#  else
"       mthd: STORE, DEFLATE, BZIP2",
#  endif
# endif /* def LZMA_SUPPORT [else] */
#else
# ifdef LZMA_SUPPORT
#  ifdef PPMD_SUPPORT
"       mthd: STORE, DEFLATE, LZMA, PPMD",
#  else
"       mthd: STORE, DEFLATE, LZMA",
#  endif
# else /* def LZMA_SUPPORT */
#  ifdef PPMD_SUPPORT
"       mthd: STORE, DEFLATE, PPMD",
#  else
"       mthd: STORE, DEFLATE",
#  endif
# endif /* def LZMA_SUPPORT [else] */
#endif
"    /[NO]PRESERVE_CASE[=([NO]ODS{2|5}[,...])], /NOVMS|/VMS[=ALL],",
"    /[NO]PKZIP, /[NO]KEEP_VERSION, /DOT_VERSION, /TRANSLATE_EOL[={LF|CRLF}],",
"    /DISPLAY=([BYTES][,COUNTS][,DOTS=mb_per_dot][,GLOBALDOTS][,USIZE]",
"     [,VOLUME]), /DESCRIPTORS, /[NO]EXTRA_FIELDS, /[NO]ZIP64,",
#ifdef SYMLINKS
"    /SPLIT=(SIZE=ssize[,BELL][,PAUSE][,VERBOSE]), /SYMLINKS"
#else /* SYMLINKS */
"    /SPLIT=(SIZE=ssize[,BELL][,PAUSE][,VERBOSE])"
#endif /* SYMLINKS [else] */
  };

  if (!show_VMSCLI_help) {
     help();
     return;
  }

  for (i = 0; i < sizeof(copyright)/sizeof(char *); i++)
  {
    printf(copyright[i], "zip");
    putchar('\n');
  }
  for (i = 0; i < sizeof(text)/sizeof(char *); i++)
  {
    printf(text[i], VERSION, REVDATE);
    putchar('\n');
  }
} /* end function VMSCLI_help() */

#endif /* !TEST */
