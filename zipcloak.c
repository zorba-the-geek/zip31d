/*
  zipcloak.c - Zip 3.1

  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-02 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
   This code was originally written in Europe and could be freely distributed
   from any country except the U.S.A. If this code was imported into the U.S.A,
   it could not be re-exported from the U.S.A to another country. (This
   restriction might seem curious but this is what US law required.)

   Now this code can be freely exported and imported.  See README.CR.
 */
#define __ZIPCLOAK_C

#ifndef UTIL
# define UTIL
#endif
#include "zip.h"
#define DEFCPYRT        /* main module: enable copyright string defines! */
#include "revision.h"
#include "crc32.h"
#include "crypt.h"
#include "ttyio.h"
#include <signal.h>
#include <errno.h>
#ifndef NO_STDLIB_H
#  include <stdlib.h>
#endif

#ifdef IZ_CRYPT_AES_WG
#  include <time.h>
#  include "aes_wg/iz_aes_wg.h"
#endif

#ifdef VMS
extern void globals_dummy( void);
#endif

#ifdef IZ_CRYPT_ANY     /* Defined in "crypt.h". */

int main OF((int argc, char **argv));

local void handler OF((int sig));
local void license OF((void));
local void help OF((void));
local void version_info OF((void));

/* Temporary zip file pointer */
local FILE *tempzf;

/* Pointer to CRC-32 table (used for decryption/encryption) */
#if (!defined(USE_ZLIB) || defined(USE_OWN_CRCTAB))
ZCONST ulg near *crc_32_tab;
# else
/* 2012-05-31 SMS.  See note in zip.c. */
/* This is for compatibility with the new ZLIB 64-bit data type. */
#  ifdef Z_U4
ZCONST z_crc_t *crc_32_tab;
#  else /* def Z_U4 */
ZCONST uLongf *crc_32_tab;
#  endif /* def Z_U4 [else] */
# endif

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

/***********************************************************************
 * Issue a message for the error, clean up files and memory, and exit.
 */
void ziperr(code, msg)
    int code;               /* error code from the ZE_ class */
    ZCONST char *msg;       /* message about how it happened */
{
    if (mesg_line_started) {
      mesg_line_started = 0;
      fprintf(mesg, "\n");
    }
    if (PERR(code)) perror("zipcloak error");
    fprintf(mesg, "zipcloak error: %s (%s)\n", ZIPERRORS(code), msg);
    if (tempzf != NULL) fclose(tempzf);
    if (tempzip != NULL) {
        destroy(tempzip);
        free((zvoid *)tempzip);
    }
    if (zipfile != NULL) free((zvoid *)zipfile);
    EXIT(code);
}

/***********************************************************************
 * Print a warning message to mesg (usually stderr) and return.
 */
void zipwarn(a, b)
ZCONST char *a, *b;     /* message strings juxtaposed in output */
{
  zipwarn_i("zipcloak warning:", 0, a, b);
}


/* zipwarn_indent(): zipwarn(), with message indented. */

void zipwarn_indent(a, b)
ZCONST char *a, *b;
{
    zipwarn_i("zipcloak warning:", 1, a, b);
}


/***********************************************************************
 * Upon getting a user interrupt, turn echo back on for tty and abort
 * cleanly using ziperr().
 */
#ifndef NO_EXCEPT_SIGNALS
local void handler(sig)
    int sig;                  /* signal number (ignored) */
{
# if (!defined(MSDOS) && !defined(__human68k__) && !defined(RISCOS))
    echon();
    putc('\n', mesg);
# endif
    ziperr(ZE_ABORT +sig-sig, "aborting");
    /* dummy usage of sig to avoid compiler warnings */
}
#endif /* ndef NO_EXCEPT_SIGNALS */


/* This is no longer used.  Instead, cryptnote and cryptAESnote
   in revision.h are now used. */
#if 0
static ZCONST char *public[] = {
"The encryption code of this program is not copyrighted and is",
"put in the public domain. It was originally written in Europe",
"and can be freely distributed in both source and object forms",
"from any country, including the USA under License Exception",
"TSU of the U.S. Export Administration Regulations (section",
"740.13(e)) of 6 June 2002.  (Prior to January 2000, re-export",
"from the US was a violation of US law.)"
};
#endif

/***********************************************************************
 * Print license information to stdout.
 */
local void license()
{
    extent i;             /* counter for copyright array */

    for (i = 0; i < sizeof(swlicense)/sizeof(char *); i++) {
        puts(swlicense[i]);
    }
    putchar('\n');
#if 0
    printf("Export notice:\n");
    for (i = 0; i < sizeof(public)/sizeof(char *); i++) {
        puts(public[i]);
    }
#endif
    /* encryption notices moved to -v, as Zip includes them there */
}


static ZCONST char *help_info[] = {
"",
"ZipCloak %s (%s)",
#ifdef VM_CMS
"Usage:  zipcloak [-dq] [-b fm] zipfile",
#else
"Usage:  zipcloak [-dq] [-b path] zipfile",
#endif
"  the default action is to encrypt all unencrypted entries in the zip file",
"",
"  -d  --decrypt          decrypt encrypted entries (copy if wrong password)",
#ifdef VM_CMS
"  -b  --temp-mode fm     use \"fm\" as the filemode for temporary zip file",
#else
"  -b  --temp-path path   use \"path\" for the temporary zip file",
#endif
"  -kf --keyfile fil      append (beginning of) fil to (end of) password",
"  -O  --output-file fil  write output to new zip file, \"fil\"",
"  -P  --password pswd    use \"pswd\" as password.  (NOT SECURE!  Many OS",
"                          allow seeing what others type on command line.",
"                          See manual.)",
"  -pn                    allow non-ANSI chars in password.  Default is to",
"                          restrict passwords to printable 7-bit ANSI chars",
"                          for portability.",
#ifdef IZ_CRYPT_AES_WG
"  -Y  --encryption-method em  use encryption method \"em\"",
#  ifdef IZ_CRYPT_TRAD
"                     Methods: Traditional, AES128, AES192, AES256",
#  else /* def IZ_CRYPT_TRAD */
"                     Methods: AES128, AES192, AES256",
#  endif /* def IZ_CRYPT_TRAD [else] */
#endif /* def IZ_CRYPT_AES_WG */
"  -q  --quiet            quiet operation, suppress some informational messages",
"  -h  --help             show this help",
"  -hh --more-help        display extended help for ZipCloak",
"  -v  --version          show version info",
"  -L  --license          show software license"
  };

/***********************************************************************
 * Print help (along with license info) to stdout.
 */
local void help()
{
    extent i;             /* counter for help array */

    for (i = 0; i < sizeof(help_info)/sizeof(char *); i++) {
        printf(help_info[i], VERSION, REVDATE);
        putchar('\n');
    }
}


/***********************************************************************
 * Print extended help to stdout.
 */
local void extended_help()
{
  extent i;             /* counter for help array */

  /* help array */
  static ZCONST char *text[] = {
"",
"Extended help for ZipCloak 3.1",
"",
"ZipCloak encrypts and decrypts entries in an archive.  The encryption",
"capabilities are similar to those of Zip and the decryption capabilities",
"are similar to those of UnZip.",
"",
"Use \"zipcloak -so\" to see all short and long options.",
"",
"The default action is to encrypt all unencrypted entries in the zip file.",
"",
"If -d is specified, all encrypted entries are decrypted.  If the given",
"password does not work for an entry, the entry is just copied and left",
"encrypted.",
"",
"A keyfile allows using virtually any file as (part of) the password.  If",
"  -kf keyfile",
"is used, the first 128 non-NULL bytes of file keyfile are read.  Once any",
"password is read (prompted or using -P), keyfile bytes are appended to the",
"end of the password to a total length of 128 bytes.  If the contents of",
"keyfile are text, then the user of the archive can just append the keyfile",
"bytes to the end of the password in any utility that can decrypt the",
"archive.  If keyfile has non-text, then user of the archive needs the",
"keyfile and must use an unzipper that understands keyfiles (or use",
"ZipCloak first to decrypt the entries).",
"",
"Currently ZipCloak does not read or write split archives.  This should",
"be addressed shortly.",
"",
"See the ZipCloak manual for additional information.",
""
  };

  for (i = 0; i < sizeof(text)/sizeof(char *); i++) {
    printf("%s\n", text[i]);
  }
}


local void version_info()
/* Print verbose info about program version and compile time options
   to stdout. */
{
  extent i;             /* counter in text arrays */

  /* AES_WG option string storage (with version). */

#ifdef IZ_CRYPT_AES_WG
  static char aes_wg_opt_ver[81];
#endif /* def IZ_CRYPT_AES_WG */

#ifdef IZ_CRYPT_TRAD
  static char crypt_opt_ver[81];
#endif

  /* Options info array */
  static ZCONST char *comp_opts[] = {
#ifdef ASM_CRC
    "ASM_CRC              (Assembly code used for CRC calculation)",
#endif
#ifdef ASMV
    "ASMV                 (Assembly code used for pattern matching)",
#endif

#ifdef DEBUG
    "DEBUG",
#endif

#ifdef LARGE_FILE_SUPPORT
# ifdef USING_DEFAULT_LARGE_FILE_SUPPORT
    "LARGE_FILE_SUPPORT (default settings)",
# else
    "LARGE_FILE_SUPPORT   (can read and write large files on file system)",
# endif
#endif
#ifdef ZIP64_SUPPORT
    "ZIP64_SUPPORT        (use Zip64 to store large files in archives)",
#endif

#ifdef IZ_CRYPT_TRAD
    crypt_opt_ver,
# ifdef ETWODD_SUPPORT
    "ETWODD_SUPPORT       (Encrypt Trad without data descriptor if --etwodd)",
# endif /* def ETWODD_SUPPORT */
#endif

#if IZ_CRYPT_AES_WG
    aes_wg_opt_ver,
#endif

#if IZ_CRYPT_AES_WG_NEW
    "IZ_CRYPT_AES_WG_NEW  (AES strong encr (WinZip/Gladman new) - do not use)",
#endif

#if defined(IZ_CRYPT_ANY) && defined(PASSWD_FROM_STDIN)
    "PASSWD_FROM_STDIN",
#endif /* defined(IZ_CRYPT_ANY) && defined(PASSWD_FROM_STDIN) */

    NULL
  };

#ifdef IZ_CRYPT_TRAD
  sprintf(crypt_opt_ver,
    "IZ_CRYPT_TRAD        (Traditional (weak) encryption, ver %d.%d%s)",
    CR_MAJORVER, CR_MINORVER, CR_BETA_VER);
#endif /* IZ_CRYPT_TRAD */

  /* Fill in IZ_AES_WG version. */
#if IZ_CRYPT_AES_WG
  sprintf( aes_wg_opt_ver,
    "IZ_CRYPT_AES_WG      (IZ AES encryption (WinZip/Gladman), ver %d.%d%s)",
    IZ_AES_WG_MAJORVER, IZ_AES_WG_MINORVER, IZ_AES_WG_BETA_VER);
#endif

  for (i = 0; i < sizeof(copyright)/sizeof(char *); i++)
  {
    printf(copyright[i], "zipcloak");
    putchar('\n');
  }
  putchar('\n');

  for (i = 0; i < sizeof(versinfolines)/sizeof(char *); i++)
  {
    printf(versinfolines[i], "ZipCloak", VERSION, REVDATE);
    putchar('\n');
  }

  version_local();

  puts("ZipCloak special compilation options:");
  for (i = 0; (int)i < (int)(sizeof(comp_opts)/sizeof(char *) - 1); i++)
  {
    printf("        %s\n",comp_opts[i]);
  }

#ifdef IZ_CRYPT_TRAD
  puts("\n");
  for (i = 0; i < sizeof(cryptnote)/sizeof(char *); i++) {
      puts(cryptnote[i]);
  }
#endif

#if defined(IZ_CRYPT_AES_WG) || defined(IZ_CRYPT_AES_WG_NEW)
  puts("\n");
  for (i = 0; i < sizeof(cryptAESnote)/sizeof(char *); i++) {
      puts(cryptAESnote[i]);
  }
#endif
}


void show_options()
{
  int i;

  /* show all options */
  printf("available options:\n");
  printf(" %-2s  %-18s %-4s %-3s %-30s\n", "sh", "long", "val", "neg", "description");
  printf(" %-2s  %-18s %-4s %-3s %-30s\n", "--", "----", "---", "---", "-----------");
  for (i = 0; options[i].option_ID; i++) {
    printf(" %-2s  %-18s ", options[i].shortopt, options[i].longopt);
    switch (options[i].value_type) {
      case o_NO_VALUE:
        printf("%-4s ", "");
        break;
      case o_REQUIRED_VALUE:
        printf("%-4s ", "req");
        break;
      case o_OPTIONAL_VALUE:
        printf("%-4s ", "opt");
        break;
      case o_VALUE_LIST:
        printf("%-4s ", "list");
        break;
      case o_ONE_CHAR_VALUE:
        printf("%-4s ", "char");
        break;
      case o_NUMBER_VALUE:
        printf("%-4s ", "num");
        break;
      case o_OPT_EQ_VALUE:
        printf("%-4s ", "=val");
        break;
      default:
        printf("%-4s ", "unk");
    }
    switch (options[i].negatable) {
      case o_NEGATABLE:
        printf("%-3s ", "neg");
        break;
      case o_NOT_NEGATABLE:
        printf("%-3s ", "");
        break;
      default:
        printf("%-3s ", "unk");
    }
    if (options[i].name) {
      printf("%-30s\n", options[i].name);
    }
    else
      printf("\n");
  }
}


/* encr_passwd() stolen from zip.c.  Should be shared. */
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

/* This version should be sufficient for Zip.  zfn is never used
   as multiple files may be involved and using zfn is inconsistent,
   and the UnZip return codes, such as for skipping entries, are
   not applicable here. */
int simple_encr_passwd(modeflag, pwbuf, size)
  int modeflag;
  char *pwbuf;
  size_t size;
{
    char *prompt;

    prompt = (modeflag == ZP_PW_VERIFY) ?
              "Verify password: " : "Enter password: ";

    if (getp(prompt, pwbuf, (int)size) == NULL) {
      ziperr(ZE_PARMS, "stderr is not a tty");
    }
    if (strlen(pwbuf) >= (size - 1)) {
      return -1;
    }
    return 0;
}


#define o_et            0x101   /* For ETWODD.  See also zip.c. */
#define o_hh            0x102   /* -hh/--more-help. */
#define o_kf            0x103   /* -kf/--keyfile. */
#define o_pn            0x104   /* -pn/--non-ansi-password.  See also zip.c. */
#define o_ps            0x105   /* -ps/--allow-short-password. */
#define o_so            0x106   /* -so/--show-options. */


/* options for zipcloak - 3/5/2004 EG */
struct option_struct far options[] = {
  /* short longopt        value_type        negatable        ID    name */
#ifdef VM_CMS
    {"b",  "temp-mode",   o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'b',  "temp file mode"},
#else
    {"b",  "temp-path",   o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'b',  "path for temp file"},
#endif
    {"d",  "decrypt",     o_NO_VALUE,       o_NOT_NEGATABLE, 'd',  "decrypt"},
#if defined( IZ_CRYPT_TRAD) && defined( ETWODD_SUPPORT)
    {"",   "etwodd",      o_NO_VALUE,       o_NOT_NEGATABLE, o_et, "encrypt Traditional without data descriptor"},
#endif /* defined( IZ_CRYPT_TRAD) && defined( ETWODD_SUPPORT) */
    {"h",  "help",        o_NO_VALUE,       o_NOT_NEGATABLE, 'h',  "help"},
    {"hh", "more-help",   o_NO_VALUE,       o_NOT_NEGATABLE, o_hh, "extended help"},
    {"kf", "keyfile",     o_REQUIRED_VALUE, o_NOT_NEGATABLE, o_kf, "read (part of) password from keyfile"},
    {"L",  "license",     o_NO_VALUE,       o_NOT_NEGATABLE, 'L',  "license"},
    {"l",  "license",     o_NO_VALUE,       o_NOT_NEGATABLE, 'L',  "license"},
    {"O",  "output-file", o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'O',  "output to new archive"},
    {"pn", "non-ansi-password", o_NO_VALUE, o_NEGATABLE,     o_pn, "allow non-ANSI password"},
    {"ps", "short-password", o_NO_VALUE,    o_NEGATABLE,     o_ps, "allow short password"},
    {"P",  "password",    o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'P',  "encrypt entries, option value is password"},
    {"so", "show-options",o_NO_VALUE,       o_NOT_NEGATABLE, o_so, "show available options on this system"},
    {"v",  "version",     o_NO_VALUE,       o_NOT_NEGATABLE, 'v',  "version"},
    {"Y", "encryption-method",o_REQUIRED_VALUE,o_NOT_NEGATABLE,'Y',"set encryption method"},
    /* the end of the list */
    {NULL, NULL,          o_NO_VALUE,       o_NOT_NEGATABLE, 0,    NULL} /* end has option_ID = 0 */
  };


/***********************************************************************
 * Encrypt or decrypt all of the entries in a zip file.  See the command
 * help in help() above.
 */

int main(argc, argv)
    int argc;                   /* number of tokens in command line */
    char **argv;                /* command line tokens */
{
    int attr;                   /* attributes of zip file */
    zoff_t start_offset;        /* start of central directory */
    zoff_t entry_offset;        /* Local header offset. */
    int decrypt;                /* decryption flag */
    int temp_path;              /* 1 if next argument is path for temp files */
#if 0
    char *q;                    /* steps through option arguments */
    int r;                      /* arg counter */
#endif
    int res;                    /* result code */
    zoff_t length;              /* length of central directory */
    FILE *inzip, *outzip;       /* input and output zip files */
    struct zlist far *z;        /* steps through zfiles linked list */

    /* used by get_option */
    unsigned long option;       /* option ID returned by get_option */
    int argcnt = 0;             /* current argcnt in args */
    int argnum = 0;             /* arg number */
    int optchar = 0;            /* option state */
    char *value = NULL;         /* non-option arg, option value or NULL */
    int negated = 0;            /* 1 = option negated */
    int fna = 0;                /* current first non-opt arg */
    int optnum = 0;             /* index in table */

    char **args;                /* copy of argv that can be freed */

    char *e = NULL;             /* Password verification storage. */
    int key_needed = 1;         /* Request password, unless "-P password". */
    char *keyfile = NULL;       /* keyfile path */

#define IS_A_DIR (z->iname[ z->nam- 1] == (char)0x2f) /* ".". */

#ifdef IZ_CRYPT_AES_WG
# define REAL_PWLEN temp_pwlen
    int temp_pwlen;
#else
# define REAL_PWLEN IZ_PWLEN
#endif


    force_ansi_key = 1;         /* Only ANSI chars for passwd (32 - 126). */
    allow_short_key = 0;        /* Allow password shorter than usually required. */
    key = NULL;                 /* Password. */

#ifdef THEOS
    setlocale(LC_CTYPE, "I");
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


  /* reading and setting locale now done by common function in fileio.c */
  set_locale();

#if 0
#ifdef UNICODE_SUPPORT
# ifdef UNIX
  /* For Unix, set the locale to UTF-8.  Any UTF-8 locale is
     OK and they should all be the same.  This allows seeing,
     writing, and displaying (if the fonts are loaded) all
     characters in UTF-8. */
  {
    char *loc;

    /*
      loc = setlocale(LC_CTYPE, NULL);
      printf("  Initial language locale = '%s'\n", loc);
    */

    loc = setlocale(LC_CTYPE, "en_US.UTF-8");

    /*
      printf("langinfo %s\n", nl_langinfo(CODESET));
    */

    if (loc != NULL) {
      /* using UTF-8 character set so can set UTF-8 GPBF bit 11 */
      using_utf8 = 1;
      /*
        printf("  Locale set to %s\n", loc);
      */
    } else {
      /*
        printf("  Could not set Unicode UTF-8 locale\n");
      */
    }
  }
# endif
#endif
#endif

    /* If no args, show help */
    if (argc == 1) {
        help();
        EXIT(ZE_OK);
    }

    /* Informational messages are written to stdout. */
    mesg = stdout;

    init_upper();               /* build case map table */

    crc_32_tab = get_crc_table();
                                /* initialize crc table for crypt */

    /* Go through args */
    zipfile = tempzip = NULL;
    tempzf = NULL;

#ifndef NO_EXCEPT_SIGNALS
# ifdef SIGINT
    signal(SIGINT, handler);
# endif
# ifdef SIGTERM                  /* Some don't have SIGTERM */
    signal(SIGTERM, handler);
# endif
# ifdef SIGABRT
    signal(SIGABRT, handler);
# endif
# ifdef SIGBREAK
    signal(SIGBREAK, handler);
# endif
# ifdef SIGBUS
    signal(SIGBUS, handler);
# endif
# ifdef SIGILL
    signal(SIGILL, handler);
# endif
# ifdef SIGSEGV
    signal(SIGSEGV, handler);
# endif
#endif /* ndef NO_EXCEPT_SIGNALS */

    temp_path = decrypt = 0;
# ifdef IZ_CRYPT_TRAD
      encryption_method = TRADITIONAL_ENCRYPTION; /* Default method = Trad. */
# else
      encryption_method = AES_128_ENCRYPTION;
# endif

#if 0
    /* old command line - not updated - do not use */
    for (r = 1; r < argc; r++) {
        if (*argv[r] == '-') {
            if (!argv[r][1]) ziperr(ZE_PARMS, "zip file cannot be stdin");
            for (q = argv[r]+1; *q; q++) {
                switch (*q) {
                case 'b':   /* Specify path for temporary file */
                    if (temp_path) {
                        ziperr(ZE_PARMS, "use -b before zip file name");
                    }
                    temp_path = 1;          /* Next non-option is path */
                    break;
                case 'd':
                    decrypt = 1;  break;
                case 'h':   /* Show help */
                    help();
                    EXIT(ZE_OK);
                case 'l': case 'L':  /* Show copyright and disclaimer */
                    license();
                    EXIT(ZE_OK);
                case 'q':   /* Quiet operation, suppress info messages */
                    noisy = 0;  break;
                case 'v':   /* Show version info */
                    version_info();
                    EXIT(ZE_OK);
                default:
                    ziperr(ZE_PARMS, "unknown option");
                } /* switch */
            } /* for */

        } else if (temp_path == 0) {
            if (zipfile != NULL) {
                ziperr(ZE_PARMS, "can only specify one zip file");

            } else if ((zipfile = ziptyp(argv[r])) == NULL) {
                ziperr(ZE_MEM, "was processing arguments");
            }
        } else {
            tempath = argv[r];
            temp_path = 0;
        } /* if */
    } /* for */

#else

    /* new command line */

    zipfile = NULL;
    out_path = NULL;

    /* make copy of args that can use with insert_arg() */
    args = copy_args(argv, 0);

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
           args    - usually same as argv if no argument file support
           argcnt  - current argc for args
           value   - char* to value (free() when done with it) or NULL if no value
           negated - option was negated with trailing -
    */

    while ((option = get_option(&args, &argcnt, &argnum,
                                &optchar, &value, &negated,
                                &fna, &optnum, 0)))
    {
      switch (option)
      {
        case 'b':   /* Specify path for temporary file */
          if (temp_path) {
            ziperr(ZE_PARMS, "more than one temp_path");
          }
          temp_path = 1;
          tempath = value;
          break;
        case 'd':
          decrypt = 1;  break;
        case 'h':   /* Show help */
          help();
          EXIT(ZE_OK);
        case o_hh:  /* Show extended help */
          extended_help();
          EXIT(ZE_OK);
        case o_kf:  /* Read (end part of) password from keyfile */
          keyfile = value;
          break;
        case 'l': case 'L':  /* Show copyright and disclaimer */
          license();
          EXIT(ZE_OK);
        case 'O':   /* Output to new zip file instead of updating original zip file */
          if ((out_path = ziptyp(value)) == NULL) {
            ziperr(ZE_MEM, "was processing arguments");
          }
          free(value);
          break;
        case 'P':   /* password for encryption */
          if (key != NULL) {
            free(key);
          }
          key = value;
          key_needed = 0;
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
        case 'q':   /* Quiet operation, suppress info messages */
          noisy = 0;  break;
        case o_so:  /* Show available options */
          show_options();
          EXIT(ZE_OK);
        case 'v':   /* Show version info */
          version_info();
          EXIT(ZE_OK);

        case 'Y':   /* Encryption method */
#ifdef IZ_CRYPT_AES_WG
#  ifdef IZ_CRYPT_TRAD
          if (abbrevmatch("Traditional", value, CASE_INS, 1) ||
              abbrevmatch("ZipCrypto", value, CASE_INS, 1)) {
            encryption_method = TRADITIONAL_ENCRYPTION;
          } else
#  endif /* def IZ_CRYPT_TRAD */
          if (abbrevmatch("AES128", value, CASE_INS, 5)) {
            encryption_method = AES_128_ENCRYPTION;
          } else if (abbrevmatch("AES192",  value, CASE_INS, 5)) {
            encryption_method = AES_192_ENCRYPTION;
          } else if (abbrevmatch("AES256", value, CASE_INS, 4)) {
            encryption_method = AES_256_ENCRYPTION;
          } else if (abbrevmatch("AES1",  value, CASE_INS, 1) ||
                     abbrevmatch("AES",  value, CASE_INS, 1)) {
            zipwarn("Ambiguous encryption method abbreviation:  ", value);
            zipwarn("valid encryption methods are:  ",
#  ifdef IZ_CRYPT_TRAD
            "Traditional (ZipCrypto), AES128, AES192, and AES256");
#  else /* def IZ_CRYPT_TRAD */
            "AES128, AES192, and AES256");
#  endif /* def IZ_CRYPT_TRAD [else] */
            free(value);
            ziperr(ZE_PARMS,
             "Option -Y (--encryption-method):  unknown method");
          } else {
            zipwarn("valid encryption methods are:  ",
#  ifdef IZ_CRYPT_TRAD
            "Traditional (ZipCrypto), AES128, AES192, and AES256");
#  else /* def IZ_CRYPT_TRAD */
            "AES128, AES192, and AES256");
#  endif /* def IZ_CRYPT_TRAD [else] */

            free(value);
            ziperr(ZE_PARMS,
             "Option -Y (--encryption-method):  unknown method");
          }
          free(value);
#else
# ifdef IZ_CRYPT_TRAD
          ziperr(ZE_PARMS, "No -Y, only Traditional (ZipCrypto) encryption enabled");
# else
          ziperr(ZE_PARMS, "No -Y, no encryption methods supported");
# endif
#endif
          break;
        case o_NON_OPTION_ARG:
          /* not an option */
          /* no more options as permuting */
          /* just dash also ends up here */

          if (strcmp(value, "-") == 0) {
            ziperr(ZE_PARMS, "zip file cannot be stdin");
          } else if (zipfile != NULL) {
            ziperr(ZE_PARMS, "can only specify one zip file");
          }

          if ((zipfile = ziptyp(value)) == NULL) {
            ziperr(ZE_MEM, "was processing arguments");
          }
          free(value);
          break;

        default:
          ziperr(ZE_PARMS, "unknown option");
      }
    } /* while */

    free_args(args);

#endif

    if (zipfile == NULL) ziperr(ZE_PARMS, "need to specify zip file");

    /* in_path is the input zip file */
    if ((in_path = malloc(strlen(zipfile) + 1)) == NULL) {
      ziperr(ZE_MEM, "input");
    }
    strcpy(in_path, zipfile);

    /* out_path defaults to in_path */
    if (out_path == NULL) {
      if ((out_path = malloc(strlen(zipfile) + 1)) == NULL) {
        ziperr(ZE_MEM, "output");
      }
      strcpy(out_path, zipfile);
    }

    /* Initialize the AES random pool, if needed.
     * Note: Code common to zip.c: main().  Should be modularized?
     *
     * if ((zsalt = malloc(32)) == NULL) {
     * Why "32" instead of, say, MAX_SALT_LENGTH or SALT_LENGTH(mode)?
     *
     * prng_rand(zsalt, SALT_LENGTH(1), &aes_rnp);  Why "1"?
     */
#ifdef IZ_CRYPT_AES_WG
    if ((encryption_method >= AES_MIN_ENCRYPTION) &&
     (encryption_method <= AES_MAX_ENCRYPTION))
    {
        time_t pool_init_start;
        time_t pool_init_time;

        pool_init_start = time(NULL);

        /* initialize the random number pool */
        aes_rnp.entropy = entropy_fun;
        prng_init( aes_rnp.entropy, &aes_rnp);
        /* and the salt */
        if ((zsalt = malloc( MAX_SALT_LENGTH)) == NULL)
        {
            ZIPERR( ZE_MEM, "Getting memory for AES salt");
        }
        prng_rand( zsalt,
             /* Note: v-- No parentheses in SALT_LENGTH def'n. --v */
         SALT_LENGTH( (encryption_method - (AES_MIN_ENCRYPTION - 1))),
         &aes_rnp);

        pool_init_time = time(NULL) - pool_init_start;
    }
#endif

    /* Read zip file */
    if ((res = readzipfile()) != ZE_OK) ziperr(res, zipfile);
    if (zfiles == NULL) ziperr(ZE_NAME, zipfile);

    /* If this is a split archive, exit as zipcloak() can't read them yet. */
    if (total_disks != 1) {
      ziperr(ZE_SPLIT, "split archives not yet supported");
    }

    /* Check for something to do */
    for (z = zfiles; z != NULL; z = z->nxt) {
        if (decrypt ? z->flg & 1 : !(z->flg & 1)) break;
    }
    if (z == NULL) {
        ziperr(ZE_NONE, decrypt ? "no encrypted files"
                       : "all files encrypted already");
    }

    /* Before we get carried away, make sure zip file is writeable */
    if ((inzip = fopen(zipfile, "a")) == NULL) ziperr(ZE_CREAT, zipfile);
    fclose(inzip);
    attr = getfileattr(zipfile);

    /* Open output zip file for writing */
#if defined(UNIX) && !defined(NO_MKSTEMP)
    {
      int yd;
      int i;

      /* use mkstemp to avoid race condition and compiler warning */

      if (tempath != NULL)
      {
        /* if -b used to set temp file dir use that for split temp */
        if ((tempzip = malloc(strlen(tempath) + 12)) == NULL) {
          ZIPERR(ZE_MEM, "allocating temp filename");
        }
        strcpy(tempzip, tempath);
        if (lastchar(tempzip) != '/')
          strcat(tempzip, "/");
      }
      else
      {
        /* create path by stripping name and appending template */
        if ((tempzip = malloc(strlen(zipfile) + 12)) == NULL) {
        ZIPERR(ZE_MEM, "allocating temp filename");
        }
        strcpy(tempzip, zipfile);
        for(i = strlen(tempzip); i > 0; i--) {
          if (tempzip[i - 1] == '/')
            break;
        }
        tempzip[i] = '\0';
      }
      strcat(tempzip, "ziXXXXXX");

      if ((yd = mkstemp(tempzip)) == EOF) {
        ZIPERR(ZE_TEMP, tempzip);
      }
      if ((y = tempzf = outzip = fdopen(yd, FOPW_TMP)) == NULL) {
        ZIPERR(ZE_TEMP, tempzip);
      }
    }
#else
    if ((y = tempzf = outzip = fopen(tempzip = tempname(zipfile), FOPW)) == NULL) {
        ziperr(ZE_TEMP, tempzip);
    }
#endif

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
      free(keyfile);
      keyfile = NULL;

      keyfile_pass = NULL;
      if (count > 0) {
        keyfilebuf[count] = '\0';
        keyfile_pass = string_dup(keyfilebuf, "read keyfile", 0);
        zipmessage("Keyfile read", "");
      }
    } /* keyfile */

    /* Get password, if not already specified on the command line. */
    if (key_needed) {

      int i;
      int r;

#ifdef IZ_CRYPT_AES_WG
      if (encryption_method <= TRADITIONAL_ENCRYPTION)
        REAL_PWLEN = IZ_PWLEN;
      else
        REAL_PWLEN = MAX_PWD_LENGTH;
#endif /* def IZ_CRYPT_AES_WG */

      if ((key = malloc(REAL_PWLEN+2)) == NULL) {
        ziperr(ZE_MEM, "was getting encryption password (1)");
      }
      r = simple_encr_passwd(ZP_PW_ENTER, key, REAL_PWLEN+1);
      if (r == -1) {
        sprintf(errbuf, "password too long - max %d", REAL_PWLEN);
        ZIPERR(ZE_PARMS, errbuf);
      }
      if (*key == '\0') {
        ZIPERR(ZE_PARMS, "zero length password not allowed");
      }

      if (force_ansi_key) {
        for (i = 0; key[i]; i++) {
          if (key[i] < 32 || key[i] > 126) {
            zipwarn("password must be ANSI (unless use --non-ansi-password)",
             "");
            ZIPERR(ZE_PARMS, "non-ANSI character in password");
          }
        }
      }

      if ((encryption_method == AES_256_ENCRYPTION) && (strlen(key) < AES_256_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars", AES_256_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES256 password must be at least %d chars (longer is better)",
                  AES_256_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_PARMS, "AES256 password too short");
        }
      }
      if ((encryption_method == AES_192_ENCRYPTION) && (strlen(key) < AES_192_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars", AES_192_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES192 password must be at least %d chars (longer is better)",
                  AES_192_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_PARMS, "AES192 password too short");
        }
      }
      if ((encryption_method == AES_128_ENCRYPTION) && (strlen(key) < AES_128_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars", AES_128_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES128 password must be at least %d chars (longer is better)",
                  AES_128_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_PARMS, "AES128 password too short");
        }
      }

      if (!decrypt) {
        /* Only verify password if encrypting. */
        if ((e = malloc(REAL_PWLEN+2)) == NULL) {
          ZIPERR(ZE_MEM, "was verifying encryption password (1)");
        }
        r = simple_encr_passwd(ZP_PW_VERIFY, e, REAL_PWLEN+1);
        r = (strcmp(key, e));
        free((zvoid *)e);
        if (r) {
          ZIPERR(ZE_PARMS, "passwords did not match");
        }
      }
    } /* key_needed */

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
      keyfile_pass[keyfile_bytes_needed] = '\0';
      strcat(newkey, keyfile_pass);
      free(keyfile_pass);
      keyfile_pass = NULL;
      key = string_dup(newkey, "merge keyfile", 0);
    } /* merge password and keyfile */

    if (key) {
      /* -P "" could get here */
      if (*key == '\0') {
        ZIPERR(ZE_PARMS, "zero length password not allowed");
      }
    }

    if (keyfile_pass) {
      /* Need to repeat these checks on password + keyfile merged key. */
      if ((encryption_method == AES_256_ENCRYPTION) && (strlen(key) < AES_256_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars", AES_256_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES256 password must be at least %d chars (longer is better)",
                  AES_256_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_PARMS, "AES256 password too short");
        }
      }
      if ((encryption_method == AES_192_ENCRYPTION) && (strlen(key) < AES_192_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars", AES_192_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES192 password must be at least %d chars (longer is better)",
                  AES_192_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_PARMS, "AES192 password too short");
        }
      }
      if ((encryption_method == AES_128_ENCRYPTION) && (strlen(key) < AES_128_MIN_PASS)) {
        if (allow_short_key)
        {
          sprintf(errbuf, "Password shorter than minimum of %d chars", AES_128_MIN_PASS);
          zipwarn(errbuf, "");
        }
        else
        {
          sprintf(errbuf,
                  "AES128 password must be at least %d chars (longer is better)",
                  AES_128_MIN_PASS);
          zipwarn(errbuf, "");
          ZIPERR(ZE_PARMS, "AES128 password too short");
        }
      }
    } /* keyfile_pass */

/* SMSd. */
#if 0
fprintf( stderr, " key = %08x : >%s<.\n", key, key);
#endif /* 0 */

    /* Open input zip file again, copy preamble if any */
    if ((in_file = fopen(zipfile, FOPR)) == NULL) ziperr(ZE_NAME, zipfile);

    if (zipbeg && (res = bfcopy(zipbeg)) != ZE_OK)
    {
        ziperr(res, res == ZE_TEMP ? tempzip : zipfile);
    }
    tempzn = zipbeg;

#ifdef IZ_CRYPT_AES_WG
    if ((encryption_method >= AES_MIN_ENCRYPTION) &&
     (encryption_method <= AES_MAX_ENCRYPTION))
    {
        /* Set the (global) AES_WG encryption strength, which is used by
         * zipfile.c:putlocal() and putcentral().
         */
        aes_strength = encryption_method - (AES_MIN_ENCRYPTION - 1);
    }
#endif /* def IZ_CRYPT_AES_WG */

    /* Go through local entries, copying, encrypting, or decrypting */
    for (z = zfiles; z != NULL; z = z->nxt)
    {
        /* Save the current offset in the output file for later use as
         * the central directory offset to the local header.
         */
        entry_offset = zftello(y);

        if (decrypt && (z->flg & 1)) {
            printf("decrypting: %s", z->zname);
            fflush(stdout);
            if ((res = zipbare(z, key)) != ZE_OK)
            {
                if (res != ZE_MISS) ziperr(res, "was decrypting an entry");
                printf(" (wrong password--just copying)");
                fflush(stdout);
            }
            putchar('\n');

        } else if ((!decrypt) && !(z->flg & 1) && (!IS_A_DIR)) {
            printf("encrypting: %s\n", z->zname);
            fflush(stdout);

/* SMSd. */
#if 0
fprintf( stderr, " pre-zc() key = %08x : >%s<.\n", key, key);
#endif /* 0 */

            if ((res = zipcloak(z, key)) != ZE_OK)
            {
                ziperr(res, "was encrypting an entry");
            }
        } else {
            printf("   copying: %s\n", z->zname);
            fflush(stdout);
            if ((res = zipcopy(z)) != ZE_OK)
            {
                ziperr(res, "was copying an entry");
            }
        } /* if */

        /* Update the (eventual) central directory offset to local header. */
        z->off = entry_offset;

    } /* for */

    fclose(in_file);


    /* Write central directory and end of central directory */

    /* get start of central */
    if ((start_offset = zftello(outzip)) == (zoff_t)-1)
        ziperr(ZE_TEMP, tempzip);

    for (z = zfiles; z != NULL; z = z->nxt) {
        if ((res = putcentral(z)) != ZE_OK) ziperr(res, tempzip);
    }

    /* get end of central */
    if ((length = zftello(outzip)) == (zoff_t)-1)
        ziperr(ZE_TEMP, tempzip);

    length -= start_offset;               /* compute length of central */
    if ((res = putend((zoff_t)zcount, length, start_offset, zcomlen,
                      zcomment)) != ZE_OK) {
        ziperr(res, tempzip);
    }
    tempzf = NULL;
    if (fclose(outzip)) ziperr(ZE_TEMP, tempzip);
    if ((res = replace(out_path, tempzip)) != ZE_OK) {
        zipwarn("new zip file left as: ", tempzip);
        free((zvoid *)tempzip);
        tempzip = NULL;
        ziperr(res, "was replacing the original zip file");
    }
    free((zvoid *)tempzip);
    tempzip = NULL;
    setfileattr(zipfile, attr);
#ifdef RISCOS
    /* Set the filetype of the zipfile to &DDC */
    setfiletype(zipfile, 0xDDC);
#endif
    free((zvoid *)in_path);
    free((zvoid *)out_path);

    free((zvoid *)zipfile);
    zipfile = NULL;

    /* Done! */
    RETURN(0);
}

#else /* def IZ_CRYPT_ANY */

/* ZipCloak with no CRYPT support is useless. */

ZCONST char * far no_cloak_msg[] =
{
"",
"   This ZipCloak executable was built without any encryption support,",
"   making it essentially useless.",
"",
"   Building Zip with some kind of encryption enabled should yield a",
"   more useful ZipCloak executable.",
""
};


/* These dummy functions et al.  are not needed on VMS.  Elsewhere? */

/* The structure here is needed if the old command line code is used, as
   this structure needs to exist to make fileio.c happy.

   These functions are provided for the case where a zipcloak without
   encryption is being compiled.  The full versions could be used instead
   if they are visible.
 */

#ifndef VMS

struct option_struct far options[] =
{
    /* the end of the list */
    {NULL, NULL,          o_NO_VALUE,       o_NOT_NEGATABLE, 0,    NULL} /* end has option_ID = 0 */
};



#if 0
int rename_split( t, o)
  char *t;
  char *o;
{
    /* Tell picky compilers to shut up about unused variables */
    t = t;
    o = o;

    return 0;
}
#endif


int set_filetype( o)
  char *o;
{
    /* Tell picky compilers to shut up about unused variables */
    o = o;

    return 0;
}


void ziperr( c, h)
int  c;
ZCONST char *h;
{
    /* Tell picky compilers to shut up about unused variables */
    c = c; h = h;
}


#if 0
void zipmessage( a, b)
ZCONST char *a, *b;
{
    /* Tell picky compilers to shut up about unused variables */
    a = a;
    b = b;
}
#endif


#if 0
void zipmessage_nl( a, nl)
ZCONST char *a;
int nl;
{
    /* Tell picky compilers to shut up about unused variables */
    a = a;
    nl = nl;
}
#endif


void zipwarn( msg1, msg2)
ZCONST char *msg1, *msg2;
{
    /* Tell picky compilers to shut up about unused variables */
    msg1 = msg1; msg2 = msg2;
}

#endif /* ndef VMS */


int main()
{
    int i;

    printf("This is ZipCloak %s (%s), by Info-ZIP.\n", VERSION, REVDATE);

    for (i = 0; i < sizeof(no_cloak_msg)/sizeof(char *); i++)
    {
        printf("%s\n", no_cloak_msg[ i]);
    }

    EXIT(ZE_COMPILE);  /* Error in compilation options. */
}

#endif /* CRYPT [else] */


/*
 * VMS (DEC C) initialization.
 */
#ifdef VMS
# include "decc_init.c"
#endif
