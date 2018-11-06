/*
  zipnote.c - Zip 3.1

  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
 *  zipnote.c by Mark Adler.
 */
#define __ZIPNOTE_C

#ifndef UTIL
# define UTIL
#endif

#include "zip.h"
#define DEFCPYRT        /* main module: enable copyright string defines! */
#include "revision.h"
#include <signal.h>

#ifdef VMS
extern void globals_dummy( void);
#endif /* def VMS */

/* Calculate size of static line buffer used in write (-w) mode. */
/* Was 2047, but increased to max size of Windows path (32K). */
#define WRBUFSIZ 32768
/* The line buffer size should be at least as large as FNMAX. */
#if FNMAX > WRBUFSIZ
#  undef WRBUFSIZ
#  define WRBUFSIZ FNMAX
#endif

/* Character to mark zip entry names in the comment file */
#define MARK '@'
#define MARKE " (comment above this line)"
#define MARKZ " (zip file comment below this line)"

/* Temporary zip file pointer */
local FILE *tempzf;


/* Local functions */
#ifndef NO_EXCEPT_SIGNALS
local void handler OF((int));
#endif /* ndef NO_EXCEPT_SIGNALS */
local void license OF((void));
local void help OF((void));
local void version_info OF((void));
local void putclean OF((char *, extent));
/* getline name conflicts with GNU getline() function */
local char *zgetline OF((char *, extent));
local int catalloc OF((char * far *, char *));
int main OF((int, char **));

/* options table */
#define o_hh 0x101
#define o_so 0x102

struct option_struct far options[] = {
  /* short longopt        value_type        negatable        ID    name */
#ifdef VM_CMS
    {"b",  "filemode",    o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'b',  "file mode"},
#else
    {"b",  "temp-dir",    o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'b',  "temp directory"},
#endif
    {"h",  "help",        o_NO_VALUE,       o_NOT_NEGATABLE, 'h',  "help"},
    {"hh", "more-help",   o_NO_VALUE,       o_NOT_NEGATABLE, o_hh, "more help"},
    {"l",  "license",     o_NO_VALUE,       o_NOT_NEGATABLE, 'L',  "license"},
    {"L",  "license",     o_NO_VALUE,       o_NOT_NEGATABLE, 'L',  "license"},
    {"q",  "quiet",       o_NO_VALUE,       o_NOT_NEGATABLE, 'q',  "quiet operation"},
    {"so", "show-options",o_NO_VALUE,       o_NOT_NEGATABLE, o_so, "show available options on this system"},
    {"v",  "version",     o_NO_VALUE,       o_NOT_NEGATABLE, 'v',  "show version"},
    {"w",  "write",       o_NO_VALUE,       o_NOT_NEGATABLE, 'w',  "write comments to zipfile"},
    /* the end of the list */
    {NULL, NULL,          o_NO_VALUE,       o_NOT_NEGATABLE, 0,    NULL} /* end has option_ID = 0 */
  };

#ifdef MACOS
#define ziperr(c, h)    zipnoteerr(c, h)
#define zipwarn(a, b)   zipnotewarn(a, b)

void zipnoteerr(int c, ZCONST char *h);
void zipnotewarn(ZCONST char *a, ZCONST char *b);
#endif

#ifdef QDOS
#define exit(p1) QDOSexit()
#endif

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

void ziperr(c, h)
int c;                  /* error code from the ZE_ class */
ZCONST char *h;         /* message about how it happened */
/* Issue a message for the error, clean up files and memory, and exit. */
{
  if (PERR(c))
    perror("zipnote error");
  fprintf(mesg, "zipnote error: %s (%s)\n", ZIPERRORS(c), h);
  if (tempzf != NULL)
    fclose(tempzf);
  if (tempzip != NULL)
  {
    destroy(tempzip);
    free((zvoid *)tempzip);
  }
  if (zipfile != NULL)
    free((zvoid *)zipfile);
  EXIT(c);
}


#ifndef NO_EXCEPT_SIGNALS
local void handler(s)
int s;                  /* signal number (ignored) */
/* Upon getting a user interrupt, abort cleanly using ziperr(). */
{
# ifndef MSDOS
  putc('\n', mesg);
# endif /* !MSDOS */
  ziperr(ZE_ABORT, "aborting");
  s++;                                  /* keep some compilers happy */
}
#endif /* ndef NO_EXCEPT_SIGNALS */


/* Print a warning message to mesg (usually stderr) and return. */

void zipwarn(a, b)
ZCONST char *a, *b;     /* message strings juxtaposed in output */
{
  zipwarn_i("zipnote warning:", 0, a, b);
}


/* zipwarn_indent(): zipwarn(), with message indented. */

void zipwarn_indent(a, b)
ZCONST char *a, *b;
{
    zipwarn_i("zipnote warning:", 1, a, b);
}


local void license()
/* Print license information to stdout. */
{
  extent i;             /* counter for copyright array */

  for (i = 0; i < sizeof(swlicense)/sizeof(char *); i++)
    puts(swlicense[i]);
}


local void help()
/* Print help (along with license info) to stdout. */
{
  extent i;             /* counter for help array */

  /* help array */
  static ZCONST char *text[] = {
"",
"ZipNote %s (%s)",
#ifdef VM_CMS
"Usage:  zipnote [-w] [-q] [-b fm] zipfile",
#else
"Usage:  zipnote [-w] [-q] [-b path] zipfile",
#endif
"  the default action is to write the comments in zipfile to stdout",
"  -w  --write          write the zipfile comments from stdin",
#ifdef VM_CMS
"  -b  --filemode fm    use \"fm\" as the filemode for the temporary zip file",
#else
"  -b  --temp-dir path  use \"path\" for the temporary zip file directory",
#endif
"  -L  --license        show software license",
"  -q  --quiet          quieter operation, suppress some informational messages",
"  -v  --version        show version info",
"  -h  --help           show this help",
"  -hh --more-help      show extended help",
"",
"Example:",
#ifdef VMS
"     define/user sys$output foo.tmp",
"     zipnote foo.zip",
"     edit foo.tmp",
"     ... then you edit the comments, save, and exit ...",
"     define/user sys$input foo.tmp",
"     zipnote -w foo.zip",
#else
# ifdef RISCOS
"     zipnote foo/zip > foo/tmp",
"     <!Edit> foo/tmp",
"     ... then you edit the comments, save, and exit ...",
"     zipnote -w foo/zip < foo/tmp",
# else
#  ifdef VM_CMS
"     zipnote foo.zip > foo.tmp",
"     xedit foo tmp",
"     ... then you edit the comments, save, and exit ...",
"     zipnote -w foo.zip < foo.tmp",
#  else
#   ifdef WIN32
"     zipnote foo.zip > foo.tmp",
"     notepad foo.tmp",
"     ... then you edit the comments, save, and exit ...",
"     zipnote -w foo.zip < foo.tmp",
#   else
"     zipnote foo.zip > foo.tmp",
"     ed foo.tmp",
"     ... then you edit the comments, save, and exit ...",
"     zipnote -w foo.zip < foo.tmp",
#   endif /* WIN32 */
#  endif /* VM_CMS */
# endif /* RISCOS */
#endif /* VMS */
"",
"  \"@ name\" can be followed by an \"@=newname\" line to change the name.",
"  See the extended help for more on this."
  };

  for (i = 0; i < sizeof(copyright)/sizeof(char *); i++) {
    printf(copyright[i], "zipnote");
    putchar('\n');
  }
  for (i = 0; i < sizeof(text)/sizeof(char *); i++)
  {
    printf(text[i], VERSION, REVDATE);
    putchar('\n');
  }
}

local void extended_help()
/* Print extended help to stdout. */
{
  extent i;             /* counter for help array */

  /* help array */
  static ZCONST char *text[] = {
"",
"Extended help for ZipNote 3.1",
"",
"  ZipNote outputs the names (paths) and comments in an archive to stdout",
"  and optionally allows these to be modified and read back in to modify",
"  names and comments in the original archive.",
"",
"  Use \"zipnote -so\" to show all short and long options.",
"",
"  See the regular help (\"zipnote -h\") for how to output, modify and",
"  read back the names and comments.  This extended help focuses on the",
"  format of the output and how to modify it.",
"",
"  For an archive (ab.zip) with two entries (a.txt and b.txt), the",
"  output of ZipNote (ab.tmp in this example) created using:",
"",
"    zipnote ab.zip > ab.tmp",
"",
"  might look like this:",
"",
"    @ a.txt",
"    This is comment a",
"    @ (comment above this line)",
"    @ b.txt",
"    This is comment b",
"    @ (comment above this line)",
"    @ (zip file comment below this line)",
"",
"",
"  (Note the blank line at the end.)",
"",
"  We want to add periods at the ends of the comment lines as well as lengthen",
"  the b comment to span two lines.  We also want to add an archive (zipfile)",
"  comment.  So we edit ab.tmp to look like this:",
"",
"    @ a.txt",
"    This is comment a.",
"    @ (comment above this line)",
"    @ b.txt",
"    This is comment b.  This comment spans",
"    two lines.",
"    @ (comment above this line)",
"    @ (zip file comment below this line)",
"    This is the zip file comment.  It can span",
"    more than one line.",
"",
"  Note that Zip currently only supports one-line entry comments.",
"",
"  ZipNote also allows changing the name (path) of entries using \"@=newname\".",
"  To show this, we rename b.txt to c.txt, updating the comment appropriately:",
"",
"    @ a.txt",
"    This is comment a.",
"    @ (comment above this line)",
"    @ b.txt",
"    @=c.txt",
"    This is comment c.",
"    @ (comment above this line)",
"    @ (zip file comment below this line)",
"    This is the zip file comment.  It can span",
"    more than one line.",
"",
"  The \"@=newname\" line must follow the \"@ name\" line it is changing.",
"",
"  Currently ZipNote only supports ANSI names and comments and, in particular,",
"  does not support UTF-8.  Do not use ZipNote on any archive that has non-ANSI",
"  names or comments.  Do not use ZipNote to add UTF-8 names or comments.",
"  UTF-8 support should be added shortly."
""
  };

  for (i = 0; i < sizeof(copyright)/sizeof(char *); i++) {
    printf(copyright[i], "zipnote");
    putchar('\n');
  }

  for (i = 0; i < sizeof(text)/sizeof(char *); i++)
  {
    printf("%s\n", text[i]);
  }
}

/*
 * XXX put this in version.c
 */

local void version_info()
/* Print verbose info about program version and compile time options
   to stdout. */
{
  extent i;             /* counter in text arrays */

  /* Options info array */
  static ZCONST char *comp_opts[] = {
#ifdef ASM_CRC
    "ASM_CRC              (Assembly code used for CRC calculation)",
#endif

#ifdef DEBUG
    "DEBUG",
#endif
    NULL
  };

  for (i = 0; i < sizeof(copyright)/sizeof(char *); i++)
  {
    printf(copyright[i], "zipnote");
    putchar('\n');
  }

  for (i = 0; i < sizeof(versinfolines)/sizeof(char *); i++)
  {
    printf(versinfolines[i], "ZipNote", VERSION, REVDATE);
    putchar('\n');
  }

  version_local();

  puts("ZipNote special compilation options:");
  for (i = 0; (int)i < (int)(sizeof(comp_opts)/sizeof(char *) - 1); i++)
  {
    printf("        %s\n",comp_opts[i]);
  }
  if (i == 0)
      puts("        [none]");
}


local void putclean(s, n)
char *s;                /* string to write to stdout */
extent n;               /* length of string */
/* Write the string s to stdout, filtering out control characters that are
   not tab or newline (mainly to remove carriage returns), and prefix MARK's
   and backslashes with a backslash.  Also, terminate with a newline if
   needed. */
{
  int c;                /* next character in string */
  int e;                /* last character written */

  e = '\n';                     /* if empty, write nothing */
  while (n--)
  {
    c = *(uch *)s++;
    if (c == MARK || c == '\\')
      putchar('\\');
    if (c >= ' ' || c == '\t' || c == '\n')
      { e=c; putchar(e); }
  }
  if (e != '\n')
    putchar('\n');
}


local char *zgetline(buf, size)
char *buf;
extent size;
/* Read a line of text from stdin into string buffer 'buf' of size 'size'.
   In case of buffer overflow or EOF, a NULL pointer is returned. */
{
    char *line;
    unsigned len;

    line = fgets(buf, (int)size, stdin);
    if (line != NULL && (len = (unsigned int)strlen(line)) > 0) {
        if (len == size-1 && line[len-1] != '\n') {
            /* buffer is full and record delimiter not seen -> overflow */
            line = NULL;
        } else {
            /* delete trailing record delimiter */
            if (line[len-1] == '\n') line[len-1] = '\0';
        }
    }
    return line;
}


local int catalloc(a, s)
char * far *a;          /* pointer to a pointer to a malloc'ed string */
char *s;                /* string to concatenate on a */
/* Concatentate the string s to the malloc'ed string pointed to by a.
   Preprocess s by removing backslash escape characters. */
{
  char *p;              /* temporary pointer */
  char *q;              /* temporary pointer */

  for (p = q = s; *q; *p++ = *q++)
    if (*q == '\\' && *(q+1))
      q++;
  *p = 0;
  if ((p = malloc(strlen(*a) + strlen(s) + 3)) == NULL)
    return ZE_MEM;
  strcat(strcat(strcpy(p, *a), **a ? "\r\n" : ""), s);
  free((zvoid *)*a);
  *a = p;
  return ZE_OK;
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


#ifndef USE_ZIPNOTEMAIN
int main(argc, argv)
#else
int zipnotemain(argc, argv)
#endif
int argc;               /* number of tokens in command line */
char **argv;            /* command line tokens */
/* Write the comments in the zipfile to stdout, or read them from stdin. */
{
  char abf[WRBUFSIZ+1]; /* input line buffer */
  char *a;              /* pointer to line buffer or NULL */
  zoff_t c;             /* start of central directory */
#if 0
  /* not used with get_option() */
  int k;                /* next argument type */
  char *q;              /* steps through option arguments */
#endif
  int r;                /* arg counter, temporary variable */
  zoff_t s;             /* length of central directory */
  int t;                /* attributes of zip file */
  int w;                /* true if updating zip file from stdin */
  FILE *x;              /* input file for testing if can write it */
  struct zlist far *z;  /* steps through zfiles linked list */

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

#ifdef UNICODE_SUPPORT
  int utf8;
#endif

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

  /* If no args, show help */
  if (argc == 1)
  {
    help();
    EXIT(ZE_OK);
  }

  /* Direct info messages to stderr; stdout is used for data output. */
  mesg = stderr;

  init_upper();           /* build case map table */

#ifndef NO_EXCEPT_SIGNALS
  signal(SIGINT, handler);
# ifdef SIGTERM              /* AMIGA has no SIGTERM */
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

  zipfile = tempzip = NULL;
  tempzf = NULL;

#ifdef UNICODE_SUPPORT_WIN32
  /* On WIN32, default to showing Unicode on console. */
  unicode_show = 1;
#endif

#if 0
  /* old command line processing */

  /* Go through args */
  k = w = 0;
  for (r = 1; r < argc; r++)
    if (*argv[r] == '-') {
      if (argv[r][1])
        for (q = argv[r]+1; *q; q++)
          switch (*q)
          {
            case 'b':   /* Specify path for temporary file */
              if (k)
                ziperr(ZE_PARMS, "use -b before zip file name");
              else
                k = 1;          /* Next non-option is path */
              break;
            case 'h':   /* Show help */
              help();  EXIT(ZE_OK);
            case 'l':  case 'L':  /* Show copyright and disclaimer */
              license();  EXIT(ZE_OK);
            case 'q':   /* Quiet operation, suppress info messages */
              noisy = 0;  break;
            case 'v':   /* Show version info */
              version_info();  EXIT(ZE_OK);
            case 'w':
              w = 1;  break;
            default:
              ziperr(ZE_PARMS, "unknown option");
          }
      else
        ziperr(ZE_PARMS, "zip file cannot be stdin");
    } else
      if (k == 0)
      {
        if (zipfile == NULL)
        {
          if ((zipfile = ziptyp(argv[r])) == NULL)
            ziperr(ZE_MEM, "was processing arguments");
        }
        else
          ziperr(ZE_PARMS, "can only specify one zip file");
      }
      else
      {
        tempath = argv[r];
        k = 0;
      }

#else

  w = 0;

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
      case 'b':   /* Specify output directory */
        if (tempath) {
          ziperr(ZE_PARMS, "more than one output directory specified");
        }
        tempath = value;
        break;
      case 'h':   /* Show help */
        help();
        EXIT(ZE_OK);
      case o_hh:   /* Show more help */
        extended_help();
        EXIT(ZE_OK);
      case 'l':   /* Show copyright and disclaimer */
      case 'L':
        license();
        EXIT(ZE_OK);
      case 'q':   /* Quiet operation, suppress info messages */
        noisy = 0;
        break;
      case o_so:  /* Show available options */
        show_options();
        EXIT(ZE_OK);
      case 'v':   /* Show version info */
        version_info();
        EXIT(ZE_OK);
      case 'w':   /* Write mode */
        w = 1;
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
  }

  free_args(args);
#endif

  if (zipfile == NULL)
    ziperr(ZE_PARMS, "need to specify zip file");

  if ((in_path = malloc(strlen(zipfile) + 1)) == NULL) {
    ziperr(ZE_MEM, "input");
  }
  strcpy(in_path, zipfile);

  /* Read zip file */
  if ((r = readzipfile()) != ZE_OK)
    ziperr(r, zipfile);
  if (zfiles == NULL)
    ziperr(ZE_NAME, zipfile);

  /* Put comments to stdout, if not -w */
  if (!w)
  {
    for (z = zfiles; z != NULL; z = z->nxt)
    {
      printf("%c %s\n", MARK, z->zname);
      putclean(z->comment, z->com);
      printf("%c%s\n", MARK, MARKE);
    }
    printf("%c%s\n", MARK, MARKZ);
    putclean(zcomment, zcomlen);
    EXIT(ZE_OK);
  }

  /* If updating comments, make sure zip file is writeable */
  if ((x = fopen(zipfile, "a")) == NULL)
    ziperr(ZE_CREAT, zipfile);
  fclose(x);
  t = getfileattr(zipfile);

  /* Process stdin, replacing comments */
  z = zfiles;
#ifdef UNICODE_SUPPORT_WIN32
  /* Remove a UTF-8 BOM if any */
  utf8 = read_utf8_bom(stdin);
# if 0
  if (utf8)
  {
    ziperr(ZE_NOTE, "UTF-8 input not yet supported");
  }
# endif
#endif
  while ((a = zgetline(abf, WRBUFSIZ+1)) != NULL &&
         (a[0] != MARK || strcmp(a + 1, MARKZ)))
  {                                     /* while input and not file comment */
    if (a[0] != MARK || a[1] != ' ')    /* better be "@ name" */
      ziperr(ZE_NOTE, "unexpected input");
    while (z != NULL && strcmp(a + 2, z->zname)
#ifdef UNICODE_SUPPORT
           && strcmp(a + 2, z->uname)
#endif
     )
      z = z->nxt;                       /* allow missing entries in order */
    if (z == NULL)
      ziperr(ZE_NOTE, "unknown entry name");
    if ((a = zgetline(abf, WRBUFSIZ+1)) != NULL && a[0] == MARK && a[1] == '=')
    {
      if (z->name != z->iname)
        free((zvoid *)z->iname);
      if ((z->iname = malloc(strlen(a+1))) == NULL)
        ziperr(ZE_MEM, "was changing name");
#ifdef EBCDIC
      strtoasc(z->iname, a+2);
#else
      strcpy(z->iname, a+2);
#endif

/*
 * Don't update z->nam here, we need the old value a little later.....
 * The update is handled in zipcopy().
 */
      a = zgetline(abf, WRBUFSIZ+1);
    }
    if (z->com)                         /* change zip entry comment */
      free((zvoid *)z->comment);
    z->comment = malloc(1);  *(z->comment) = 0;
    while (a != NULL && *a != MARK)
    {
      if ((r = catalloc(&(z->comment), a)) != ZE_OK)
        ziperr(r, "was building new zipentry comments");
      a = zgetline(abf, WRBUFSIZ+1);
    }
    if (is_utf8_string(z->comment, NULL, NULL, NULL, NULL)) {
      sprintf(errbuf, "new comment has UTF-8: %s", z->iname);
      zipwarn(errbuf, "");
    }
    z->com = (ush)strlen(z->comment);
    z = z->nxt;                         /* point to next entry */
  }
  if (a != NULL)                        /* change zip file comment */
  {
    zcomment = malloc(1);  *zcomment = 0;
    while ((a = zgetline(abf, WRBUFSIZ+1)) != NULL)
      if ((r = catalloc(&zcomment, a)) != ZE_OK)
        ziperr(r, "was building new zipfile comment");
    zcomlen = (ush)strlen(zcomment);
    if (!is_text_buf(zcomment, zcomlen)) {
      ziperr(ZE_NOTE, "binary not allowed in zip file comment");
    }
    if (is_utf8_string(zcomment, NULL, NULL, NULL, NULL)) {
      sprintf(errbuf, "new zip file comment has UTF-8");
      zipwarn(errbuf, "");
    }
  }

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
    if ((tempzf = y = fdopen(yd, FOPW)) == NULL) {
      ZIPERR(ZE_TEMP, tempzip);
    }
  }
#else
  if ((tempzf = y = fopen(tempzip = tempname(zipfile), FOPW)) == NULL)
    ziperr(ZE_TEMP, tempzip);
#endif

  /* Open input zip file again, copy preamble if any */
  if ((in_file = fopen(zipfile, FOPR)) == NULL)
    ziperr(ZE_NAME, zipfile);

  if (zipbeg && (r = bfcopy(zipbeg)) != ZE_OK)
    ziperr(r, r == ZE_TEMP ? tempzip : zipfile);
  tempzn = zipbeg;

  /* Go through local entries, copying them over as is */
  fix = 3; /* needed for zipcopy if name changed */
  for (z = zfiles; z != NULL; z = z->nxt) {
    if ((r = zipcopy(z)) != ZE_OK)
      ziperr(r, "was copying an entry");
  }
  fclose(in_file);

  /* Write central directory and end of central directory with new comments */
  if ((c = zftello(y)) == (zoff_t)-1)    /* get start of central */
    ziperr(ZE_TEMP, tempzip);
  for (z = zfiles; z != NULL; z = z->nxt)
    if ((r = putcentral(z)) != ZE_OK)
      ziperr(r, tempzip);
  if ((s = zftello(y)) == (zoff_t)-1)    /* get end of central */
    ziperr(ZE_TEMP, tempzip);
  s -= c;                       /* compute length of central */
  if ((r = putend((zoff_t)zcount, s, c, zcomlen, zcomment)) != ZE_OK)
    ziperr(r, tempzip);
  tempzf = NULL;
  if (fclose(y))
    ziperr(ZE_TEMP, tempzip);
  if ((r = replace(zipfile, tempzip)) != ZE_OK)
  {
    zipwarn("new zip file left as: ", tempzip);
    free((zvoid *)tempzip);
    tempzip = NULL;
    ziperr(r, "was replacing the original zip file");
  }
  free((zvoid *)tempzip);
  tempzip = NULL;
  setfileattr(zipfile, t);
#ifdef RISCOS
  /* Set the filetype of the zipfile to &DDC */
  setfiletype(zipfile,0xDDC);
#endif
  free((zvoid *)zipfile);
  zipfile = NULL;

  /* Done! */
  RETURN(0);
}


/*
 * VMS (DEC C) initialization.
 */
#ifdef VMS
# include "decc_init.c"
#endif
