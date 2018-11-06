/*
  zipsplit.c - Zip 3.1

  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
 *  zipsplit.c by Mark Adler.
 */
#define __ZIPSPLIT_C

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

/* The below comments are getting old as floppy disks fade into the distance,
   but are still relevant in some quarters.  Nowadays CD, DVD, and other
   removable media sizes probably are the more likely output size, as well as
   2 GB to get around the FAT32 limitation.  Nonetheless, we keep the below
   floppy disk description for historical reasons.
   
   Different OS have different native capacities.  The IBM HD format (the
   most commonly used nowadays) is just one of them.  (This was set to 36000L.)
   On other systems other smaller and larger capacities are used.  The default
   split size here assumes the split will fit exactly on a standard IBM HD floppy
   with no other files and no free space (based on advertised capacity).  In
   practice, the user may want to leave some free space on the IBM HD floppy to
   avoid problems using floppys with bad sectors and the like. I suggest this be
   set to 1400000L instead. Note that single density (SD) and double density (DD)
   3.5" disks are still in use on some systems due to hardware limitations.  In
   practice, a user should always specify the split size based on their needs. */

#define DEFSIZ 1474560L /* Default split size (change in help() too) */

/* If USE_LONG_TEMPLATE set, increase output path template size to include
   more of the output path.  Some ports, like CMS, do not support this.
   To revert to previous 8.3 filename behavior, undefine this. */
#ifndef NO_USE_LONG_TEMPLATE
# ifndef USE_LONG_TEMPLATE
#  define USE_LONG_TEMPLATE
# endif
#endif

#ifdef MSDOS
#  define NL 2          /* Number of bytes written for a \n */
#else /* !MSDOS */
#  define NL 1          /* Number of bytes written for a \n */
#endif /* ?MSDOS */
#ifdef RISCOS
#  define INDEX "zipspl/idx"      /* Name of index file */
#  define TEMPL_FMT "%%0%dld"
#  define TEMPL_SIZ 13
#  define MAX_BASE 8
#  define ZPATH_SEP '.'
#else
#ifdef QDOS
#  define ZPATH_SEP '_'
#  define INDEX "zipsplit_idx"    /* Name of index file */
#  define TEMPL_FMT "%%0%dld_zip"
#  define TEMPL_SIZ 17
#  define MAX_BASE 8
#  define exit(p1) QDOSexit()
#else
#ifdef VM_CMS
#  define INDEX "zipsplit.idx"    /* Name of index file */
#  define TEMPL_FMT "%%0%dld.zip"
#  define TEMPL_SIZ 21
#  define MAX_BASE 8
#  define ZPATH_SEP '.'
#else
#  define INDEX "zipsplit.idx"    /* Name of index file */
#  define TEMPL_FMT "%%0%dld.zip"
#  ifdef USE_LONG_TEMPLATE
#    define MAX_BASE 240
#  else
#    define MAX_BASE 8
#  endif
#  define TEMPL_SIZ (MAX_BASE + 9)
#  define ZPATH_SEP '.'
#endif /* VM_CMS */
#endif /* QDOS */
#endif /* RISCOS */

#ifdef MACOS
#define ziperr(c, h)    zipspliterr(c, h)
#define zipwarn(a, b)   zipsplitwarn(a, b)
void zipsplitwarn(ZCONST char *a, ZCONST char *b);
void zipspliterr(int c, ZCONST char *h);
#endif /* MACOS */

/* Local functions */
#ifndef NO_EXCEPT_SIGNALS
local void handler OF((int));
#endif /* ndef NO_EXCEPT_SIGNALS */
local zvoid *talloc OF((extent));
local void tfree OF((zvoid *));
local void tfreeall OF((void));
local void license OF((void));
local void help OF((void));
local void version_info OF((void));
local extent simple OF((uzoff_t *, extent, uzoff_t, uzoff_t));
local int descmp OF((ZCONST zvoid *, ZCONST zvoid *));
local extent greedy OF((uzoff_t *, extent, uzoff_t, uzoff_t));
local int retry OF((void));
int main OF((int, char **));


/* Output zip files */
local char templat[TEMPL_SIZ];  /* name template for output files */
                                /* renamed from template as that's reserved now */
local int zipsmade = 0;         /* number of zip files made */
local int indexmade = 0;        /* true if index file made */
local char *path = NULL;        /* space for full name */
local int path_size;            /* size of path to malloc */
local char *name;               /* where name goes in path[] */


/* The talloc() and tree() routines extend malloc() and free() to keep
   track of all allocated memory.  Then the tfreeall() routine uses this
   information to free all allocated memory before exiting. */

#define TMAX 6          /* set intelligently by examining the code */
zvoid *talls[TMAX];     /* malloc'ed pointers to track */
int talln = 0;          /* number of entries in talls[] */


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

#if 0
/* rename a split
 * A split has a tempfile name until it is closed, then
 * here rename it as out_path the final name for the split.
 *
 * This is not used in zipsplit but is referenced by the generic split
 * writing code.  If zipsplit is made split aware (so can write splits of
 * splits, if that makes sense) then this would get used.  But if that
 * happens these utility versions should be dropped and the main ones
 * used.
 *
 * Switched over to using main version, now in fileio.c.
 *
 * zipsplit can handle input split archives.  Creating output split
 * archives still not supported.
 */
int rename_split(temp_name, out_path)
  char *temp_name;
  char *out_path;
{
  int r;
  /* Replace old zip file with new zip file, leaving only the new one */
  if ((r = replace(out_path, temp_name)) != ZE_OK)
  {
    zipwarn("new zip file left as: ", temp_name);
    free((zvoid *)tempzip);
    tempzip = NULL;
    ZIPERR(r, "was replacing split file");
  }
  if (zip_attributes) {
    setfileattr(out_path, zip_attributes);
  }
  return ZE_OK;
}
#endif

#if 0
void zipmessage_nl(a, nl)
ZCONST char *a;     /* message string to output */
int nl;             /* 1 = add nl to end */
/* If nl false, print a message to mesg without new line.
   If nl true, print and add new line.  If logfile is
   open then also write message to log file. */
{
  if (noisy) {
    fprintf(mesg, "%s", a);
    if (nl) {
      fprintf(mesg, "\n");
      mesg_line_started = 0;
    } else {
      mesg_line_started = 1;
    }
    fflush(mesg);
  }
}

void zipmessage(a, b)
ZCONST char *a, *b;     /* message strings juxtaposed in output */
/* Print a message to mesg and flush.  Also write to log file if
   open.  Write new line first if current line has output already. */
{
  if (noisy) {
    if (mesg_line_started)
      fprintf(mesg, "\n");
    fprintf(mesg, "%s%s\n", a, b);
    mesg_line_started = 0;
    fflush(mesg);
  }
}
#endif

local zvoid *talloc(s)
extent s;
/* does a malloc() and saves the pointer to free later (does not check
   for an overflow of the talls[] list) */
{
  zvoid *p;

  if ((p = (zvoid *)malloc(s)) != NULL)
    talls[talln++] = p;
  return p;
}


local void tfree(p)
zvoid *p;
/* does a free() and also removes the pointer from the talloc() list */
{
  int i;

  free(p);
  i = talln;
  while (i--)
    if (talls[i] == p)
      break;
  if (i >= 0)
  {
    while (++i < talln)
      talls[i - 1] = talls[i];
    talln--;
  }
}


local void tfreeall()
/* free everything talloc'ed and not tfree'd */
{
  while (talln)
    free(talls[--talln]);
}


void ziperr(c, h)
int c;                  /* error code from the ZE_ class */
ZCONST char *h;         /* message about how it happened */
/* Issue a message for the error, clean up files and memory, and exit. */
{
  if (PERR(c))
    perror("zipsplit error");
  fprintf(mesg, "zipsplit error: %s (%s)\n", ZIPERRORS(c), h);
  if (indexmade)
  {
    strcpy(name, INDEX);
    destroy(path);
  }
  for (; zipsmade; zipsmade--)
  {
    sprintf(name, templat, zipsmade);
    destroy(path);
  }
  tfreeall();
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
  zipwarn_i("zipsplit warning:", 0, a, b);
}


/* zipwarn_indent(): zipwarn(), with message indented. */

void zipwarn_indent(a, b)
ZCONST char *a, *b;
{
    zipwarn_i("zipsplit warning:", 1, a, b);
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
"ZipSplit %s (%s)",
#ifdef VM_CMS
"Usage:  zipsplit [-tipqs] [-n size] [-r room] [-b fm] zipfile",
#else
"Usage:  zipsplit [-tipqs] [-n size] [-r room] [-b path] zipfile",
#endif
"-t  --total-only   report how many files it will take, but don't make them",
#ifdef RISCOS
"-i  --index-file   make index (" INDEX ") and count its size against first zip file",
#else
"-i  --index-file   make index (.idx) and count its size against first zip file",
#endif
"-n  --split size   make zip files no larger than \"size\" (default = 1440K)", /* was 36000 */
"         (for -n: 1k = 1024, 1m = 1 MiB, 1g = 1 GiB)",
"-r  --room rm      leave room for \"rm\" bytes on first disk (default = 0)",
#ifdef VM_CMS
"-b  --filemode fm  use \"fm\" as the filemode for the output zip files",
#else
"-b  --outdir path  use \"path\" as dir for the output zip files",
#endif
"-q  --quiet        quieter operation, suppress some informational messages",
"-p  --pause        pause between output zip files",
"-s  --sequential   do a sequential split even if it takes more zip files",
"-v  --version      show version info",
"-L  --license      show software license",
"-h  --help         show this help",
"-hh --more-help    show extended help"
  };

  for (i = 0; i < sizeof(copyright)/sizeof(char *); i++) {
    printf(copyright[i], "zipsplit");
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
  extent i;             /* counter for extended help array */

  /* help array */
  static ZCONST char *text[] = {
"",
"Extended help for ZipSplit 3.1",
"",
"ZipSplit splits a zipfile (archive) into smaller zipfiles, each no larger",
"than some capacity (set by -n, which defaults to 1440k).  Note that the",
"output files are complete separate archives.  In contrast, zip -s creates",
"a single archive split into parts, where all parts are needed to normally",
"read and extract from the archive.  (See the Zip documentation for more",
"on split archives.)",
"",
"ZipSplit 3.1 now supports long options similar to Zip.  Use -so to see",
"the complete list of options supported.  Note that command line processing",
"has changed, ZipSplit now using the same parser as Zip.  Though the new",
"processing should be completely compatible with old zipsplit command lines,",
"it's possible there's a non-standard zipsplit command line out there that",
"is no longer supported.  Check any use of ZipSplit to make sure proper",
"syntax is used.",
"",
"Option -n sets capacity of each bin (destination zipfile/disk).  With",
"ZipSplit 3.1, -n now can use a size suffix similar to Zip -s option, for",
"example k (1 KiB), m (1 MiB), and g (1GiB).  So -n=740m sets maximum size:",
"of bin/disk to 740 MiB (740 * 1024 * 1024).",
"",
"-r sets room left on first disk, such as for documentation.  It takes a",
"number of bytes similar to -n.  For example, -r=10k would leave 10,240",
"bytes of space on the first disk.",
"",
"-t displays how many disks a split operation would need (how many output",
"archives would be generated).  This can be used to make sure the proper",
"number of removable media are available, or to see the results of using",
"different -n, -s and -r.",
"",
"Normally destination archives are written to the current directory.  Use",
"-b destdir to write the output zip files to destdir.  destdir must exist.",
"",
"-p will pause ZipSplit after each disk (archive) is created.  This allows",
"changing media for each output zip file.",
"",
"-s instructs ZipSplit to perform \"simple\" splitting.  Entries are written",
"in sequence to the output zip files.  This can take more disks than normal",
"\"greedy\" splitting, but leaves entries in the same order as in the",
"original archive.  Without -s, ZipSplit tries to fill each disk to capacity,",
"even if entries get scattered among output disks.",
"",
"-i creates an index file showing which split each entry in original archive",
"ended up in.",
"",
"ZipSplit can read split archives (as created by zip -s).  These are handled",
"like any other archive and the input split size does not impact how ZipSplit",
"creates the output archives.  This allows a large split archive to be",
"divided into a number of separate smaller archives that may be easier to",
"work with.  In particular, older versions of UnZip can accept a set of",
"multiple archives and extract from them as a unit, where as those older",
"UnZip versions can't handle the split archive.",
"",
"Limits vary by OS and build, but with LARGE FILE SUPPORT enabled, max size",
"of an archive is around 2^62 bytes (4 EiB), max number of files in an",
"archive around 2^31 (2 billion) and max number of disks that can be written",
"around 2^31 (2 billion).  Of course performance may be a factor as these",
"limits are approached.",
"",
"For example:",
"  zipsplit original.zip -n 720m -s -i -b foo/bar",
"would split entries in original.zip into zip files no larger than 720 MiB",
"using sequential split (entries are written in the order they appear in",
"original.zip).  The output zip files are written to subdirectory foo/bar/",
"and an index file is created."
  };

  for (i = 0; i < sizeof(text)/sizeof(char *); i++)
  {
    printf("%s\n", text[i]);
  }
}


local void version_info()
/* Print verbose info about program version and compile time options
   to stdout. */
{
  extent i;             /* counter in text arrays */

  /* Options info array */
  static ZCONST char *comp_opts[] = {
#ifdef DEBUG
    "DEBUG",
#endif
    NULL
  };

  for (i = 0; i < sizeof(versinfolines)/sizeof(char *); i++)
  {
    printf(versinfolines[i], "ZipSplit", VERSION, REVDATE);
    putchar('\n');
  }

  version_local();

  puts("ZipSplit special compilation options:");
  for (i = 0; (int)i < (int)(sizeof(comp_opts)/sizeof(char *) - 1); i++)
  {
    printf("        %s\n",comp_opts[i]);
  }
  if (i == 0)
      puts("        [none]");
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


local extent simple(a, n, c, d)
uzoff_t *a;     /* items to put in bins, return value: destination bins */
extent n;       /* number of items */
uzoff_t c;      /* capacity of each bin */
uzoff_t d;      /* amount to deduct from first bin */
/* Return the number of bins of capacity c that are needed to contain the
   integers in a[0..n-1] placed sequentially into the bins.  The value d
   is deducted initially from the first bin (space for index).  The entries
   in a[] are replaced by the destination bins. */
{
  uzoff_t k;    /* current bin number (was extent) */
  uzoff_t t;    /* space used in current bin */

  t = k = 0;
  while (n--)
  {
    if (*a + t > c - (k == 0 ? d : 0))
    {
      k++;
      t = 0;
    }
    t += *a;
    /* *(ulg huge *)a++ = k; */
    *a++ = k;
  }
  return (extent)k + 1;
}


local int descmp(a, b)
ZCONST zvoid *a, *b;          /* pointers to pointers to uzoff_t's (was ulg's) to compare */
/* Used by qsort() in greedy() to do a descending sort. */
{
  return **(uzoff_t **)a < **(uzoff_t **)b ? 1 :
         (**(uzoff_t **)a > **(uzoff_t **)b ? -1 : 0);
}


local extent greedy(a, n, c, d)
uzoff_t *a;     /* items to put in bins, return value: destination bins */
extent n;       /* number of items */
uzoff_t c;      /* capacity of each bin */
uzoff_t d;      /* amount to deduct from first bin */
/* Return the number of bins of capacity c that are needed to contain the
   items with sizes a[0..n-1] placed non-sequentially into the bins.  The
   value d is deducted initially from the first bin (space for index).
   The entries in a[] are replaced by the destination bins. */
{
  uzoff_t *b;   /* space left in each bin (malloc'ed for each m) */
  uzoff_t *e;   /* copy of argument a[] (malloc'ed) */
  uzoff_t i;    /* steps through items (was extent) */
  uzoff_t j;    /* steps through bins (was extent) */
  uzoff_t k;    /* best bin to put current item in (was extent) */
  uzoff_t m;    /* current number of bins (was extent) */
  uzoff_t **s;  /* pointers to e[], sorted descending (malloc'ed) */
  uzoff_t t;    /* space left in best bin (index k) */

  /* Algorithm:
     1. Copy a[] to e[] and sort pointers to e[0..n-1] (in s[]), in
        descending order.
     2. Compute total of s[] and set m to the smallest number of bins of
        capacity c that can hold the total.
     3. Allocate m bins.
     4. For each item in s[], starting with the largest, put it in the
        bin with the smallest current capacity greater than or equal to the
        item's size.  If no bin has enough room, increment m and go to step 4.
     5. Else, all items ended up in a bin--return m.
  */

  /* Copy a[] to e[], put pointers to e[] in s[], and sort s[].  Also compute
     the initial number of bins (minus 1). */
  if ((e = (uzoff_t *)malloc(n * sizeof(uzoff_t))) == NULL ||
      (s = (uzoff_t **)malloc(n * sizeof(uzoff_t *))) == NULL)
  {
    if (e != NULL)
      free((zvoid *)e);
    ziperr(ZE_MEM, "was trying a smart split");
    return 0;                           /* only to make compiler happy */
  }
  memcpy((char *)e, (char *)a, n * sizeof(uzoff_t));
  for (t = i = 0; i < (uzoff_t)n; i++)
    t += *(s[i] = e + i);
  /* avoid m = -1 */
  if (t == 0) t = 1;
  m = (zoff_t)((t + c - 1) / c) - 1;    /* pre-decrement for loop */
  qsort((char *)s, n, sizeof(uzoff_t *), descmp);

  /* Stuff bins until successful */
  do {
    /* Increment the number of bins, allocate and initialize bins */
    if ((b = (uzoff_t *)malloc((size_t)(++m * sizeof(uzoff_t)))) == NULL)
    {
      free((zvoid *)s);
      free((zvoid *)e);
      ziperr(ZE_MEM, "was trying a smart split");
    }
    b[0] = c - d;                       /* leave space in first bin */
    for (j = 1; j < m; j++)
      b[j] = c;

    /* Fill the bins greedily */
    for (i = 0; i < n; i++)
    {
      /* Find smallest bin that will hold item i (size s[i]) */
      t = c + 1;
      for (k = j = 0; j < m; j++)
        if (*s[i] <= b[j] && b[j] < t)
          t = b[k = j];

      /* If no bins big enough for *s[i], try next m */
      if (t == c + 1)
        break;

      /* Diminish that bin and save where it goes */
      b[k] -= *s[i];
      a[(int)((uzoff_t huge *)(s[i]) - (uzoff_t huge *)e)] = k;
    }

    /* Clean up */
    free((zvoid *)b);

    /* Do until all items put in a bin */
  } while (i < n);

  /* Done--clean up and return the number of bins needed */
  free((zvoid *)s);
  free((zvoid *)e);
  return (extent)m;
}


/* keep compiler happy until implement long options - 11/4/2003 EG */
/* initial filling of table - 11/6/2014 EG */

#define o_hh 0x101
#define o_so 0x102

struct option_struct far options[] = {
/* short longopt        value_type        negatable        ID    name */
#ifdef VM_CMS
  {"b",  "filemode",    o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'b',  "output directory"},
#else
  {"b",  "outdir",      o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'b',  "output directory"},
#endif
  {"h",  "help",        o_NO_VALUE,       o_NOT_NEGATABLE, 'h',  "help"},
  {"hh", "more-help",   o_NO_VALUE,       o_NOT_NEGATABLE, o_hh, "extended help"},
  {"i",  "index-file",  o_NO_VALUE,       o_NOT_NEGATABLE, 'i',  "make index"},
  {"L",  "license",     o_NO_VALUE,       o_NOT_NEGATABLE, 'L',  "license"},
  {"l",  "license",     o_NO_VALUE,       o_NOT_NEGATABLE, 'L',  "license"},
  {"n",  "split-size",  o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'n',  "split size bytes (e.g. 1440k = 1440 KiB)"},
  {"p",  "pause",       o_NO_VALUE,       o_NOT_NEGATABLE, 'p',  "pause between splits"},
  {"q",  "quiet",       o_NO_VALUE,       o_NOT_NEGATABLE, 'q',  "suppress info messages"},
  {"r",  "room-first",  o_REQUIRED_VALUE, o_NOT_NEGATABLE, 'r',  "leave bytes room on first disk"},
  {"s",  "sequential",  o_NO_VALUE,       o_NOT_NEGATABLE, 's',  "just split sequentially, even if more splits"},
  {"so", "show-options",o_NO_VALUE,       o_NOT_NEGATABLE, o_so, "show available options on this system"},
  {"t",  "total-only",  o_NO_VALUE,       o_NOT_NEGATABLE, 't',  "just report how many disks would be needed"},
  {"v",  "version",     o_NO_VALUE,       o_NOT_NEGATABLE, 'v',  "show version information"},
  /* the end of the list */
  {NULL, NULL,          o_NO_VALUE,       o_NOT_NEGATABLE, 0,    NULL} /* end has option_ID = 0 */
};


local int retry()
{
  char m[10];
  fputs("Error writing to disk--redo entire disk? ", mesg);
  fgets(m, 10, stdin);
  return *m == 'y' || *m == 'Y';
}


#ifndef USE_ZIPSPLITMAIN
int main(argc, argv)
#else
int zipsplitmain(argc, argv)
#endif

int argc;               /* number of tokens in command line */
char **argv;            /* command line tokens */
/* Split a zip file into several zip files less than a specified size.  See
   the command help in help() above. */
{
  uzoff_t *a;           /* malloc'ed list of sizes, dest bins */
  extent *b;            /* heads of bin linked lists (malloc'ed) */
  uzoff_t c;            /* bin capacity, start of central directory */
  uzoff_t c_d_ents;     /* Central directory entry counter. */
  int d;                /* if true, just report the number of disks */
  FILE *e;              /* input zip file */
  FILE *f;              /* output index and zip files */
  extent g;             /* number of bins from greedy(), entry to write */
  int h;                /* how to split--true means simple split, counter */
  zoff_t i = 0;            /* size of index file plus room to leave */
  extent j;             /* steps through zip entries, bins */
  int k;                /* next argument type */
  extent *n = NULL;     /* next item in bin list (heads in b) */
  uzoff_t *p;           /* malloc'ed list of sizes, dest bins for greedy() */
  char *q;              /* steps through option characters */
  int r;                /* temporary variable, counter */
  extent s;             /* number of bins needed */
  zoff_t t;             /* total of sizes, end of central directory */
  int u;                /* flag to wait for user on output files */
  struct zlist far **w; /* malloc'ed table for zfiles linked list */
  int x;                /* if true, make an index file */
  struct zlist far *z;  /* steps through zfiles linked list */

  int used_simple = 0;  /* 0=used greedy, 1=used simple (sequential) */

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

#ifdef AMIGA
  char tailchar;         /* temporary variable used in name generation below */
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

  /* Informational messages are written to stdout. */
  mesg = stdout;

  init_upper();           /* build case map table */

#ifndef NO_EXCEPT_SIGNALS
  /* Establish signal handler. */
  signal(SIGINT, handler);
# ifdef SIGTERM                 /* Amiga has no SIGTERM */
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


#if 0
  /* old command line processing */

  /* Go through args */
  k = h = x = d = u = 0;
  c = DEFSIZ;
  for (r = 1; r < argc; r++)
    if (*argv[r] == '-')
    {
      if (argv[r][1])
        for (q = argv[r]+1; *q; q++)
          switch (*q)
          {
            case 'b':   /* Specify path for output files */
              if (k)
                ziperr(ZE_PARMS, "options are separate and precede zip file");
              else
                k = 1;          /* Next non-option is path */
              break;
            case 'h':   /* Show help */
              help();  EXIT(ZE_OK);
            case 'i':   /* Make an index file */
              x = 1;
              break;
            case 'l': case 'L':  /* Show copyright and disclaimer */
              license();  EXIT(ZE_OK);
            case 'n':   /* Specify maximum size of resulting zip files */
              if (k)
                ziperr(ZE_PARMS, "options are separate and precede zip file");
              else
                k = 2;          /* Next non-option is size */
              break;
            case 'p':
              u = 1;
              break;
            case 'q':   /* Quiet operation, suppress info messages */
              noisy = 0;
              break;
            case 'r':
              if (k)
                ziperr(ZE_PARMS, "options are separate and precede zip file");
              else
                k = 3;          /* Next non-option is room to leave */
              break;
            case 's':
              h = 1;    /* Only try simple */
              break;
            case 't':   /* Just report number of disks */
              d = 1;
              break;
            case 'v':   /* Show version info */
              version_info();  EXIT(ZE_OK);
            default:
              ziperr(ZE_PARMS, "Use option -h for help.");
          }
      else
        ziperr(ZE_PARMS, "zip file cannot be stdin");
    }
    else
      switch (k)
      {
        case 0:
          if (zipfile == NULL)
          {
            if ((zipfile = ziptyp(argv[r])) == NULL)
              ziperr(ZE_MEM, "was processing arguments");
          }
          else
            ziperr(ZE_PARMS, "can only specify one zip file");
          break;
        case 1:
          tempath = argv[r];
          k = 0;
          break;
        case 2:
          /* Split size value.  100 is smallest allowed zip file size. */
          c = ReadNumString(argv[r]);
          if ((c == (uzoff_t)-1) || (c < 100))
            ziperr( ZE_PARMS,
             "invalid split size value.  Use option -h for help.");
          k = 0;
          break;
        default:        /* k must be 3 */
          /* Room to leave value. */
          i = ReadNumString(argv[r]);
          if (i == (uzoff_t)-1)
            ziperr( ZE_PARMS,
             "invalid room-to-leave value.  Use option -h for help.");
          k = 0;
          break;
      }

#else

  /* new command line processing */

  k = h = x = d = u = 0;
  c = DEFSIZ;
 
  zipfile = NULL;

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
      case 'i':   /* Make an index file */
        x = 1;
        break;
      case 'l':   /* Show copyright and disclaimer */
        case 'L':
        license();
        EXIT(ZE_OK);
      case 'n':   /* Split size */
        /* Split size value.  100 is smallest allowed zip file size. */
        c = ReadNumString(value);
        free(value);
        if ((c == (uzoff_t)-1) || (c < 100)) {
          ziperr( ZE_PARMS,
            "invalid split size value (100 bytes smallest allowed size)");
        }
        break;
      case 'p':   /* Pause */
        u = 1;
        break;
      case 'q':   /* Quiet operation, suppress info messages */
        noisy = 0;
        break;
      case 'r':   /* Room to leave */
        i = ReadNumString(value);
        free(value);
        if (i == (uzoff_t)-1) {
          ziperr( ZE_PARMS,
            "invalid room-to-leave value.  Use option -h for help.");
        }
        break;
      case 's':   /* Only try simple */
        h = 1;
        break;
      case o_so:  /* Show available options */
        show_options();
        EXIT(ZE_OK);
      case 't':   /* Just report number of disks */
        d = 1;
        break;
      case 'v':   /* Show version info */
        version_info();
        EXIT(ZE_OK);

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
    ziperr(ZE_MEM, "input archive path");
  }
  strcpy(in_path, zipfile);

  /* Tell readzipfile there is no out_path */
  out_path = NULL;

  /* Read zip file */
  if ((r = readzipfile()) != ZE_OK)
    ziperr(r, zipfile);
  if (zfiles == NULL)
    ziperr(ZE_NAME, zipfile);

  /* Make a list of sizes and check against capacity.  Also compute the
     size of the index file. */
  c -= ENDHEAD + 4;                     /* subtract overhead/zipfile */
  if ((a = (uzoff_t *)talloc(zcount * sizeof(uzoff_t))) == NULL ||
      (w = (struct zlist far **)talloc(zcount * sizeof(struct zlist far *))) ==
       NULL)
  {
    ziperr(ZE_MEM, "was computing split");
    return 1;
  }
  t = 0;
  for (j = 0, z = zfiles; j < zcount; j++, z = z->nxt)
  {
#if 0
    zoff_t aj = 0;
    printf("  -- entry:  (%s)  '%s' \n", zip_fzofft(z->siz, NULL, "u"), z->name);
    aj = 8 + LOCHEAD + CENHEAD + 2 * (zoff_t)z->nam + 2 * (zoff_t)z->cext + z->com + z->siz;
    printf("      aj %s\n", zip_fzofft(aj, NULL, "u"));
#endif
    w[j] = z;
    if (x)
      i += z->nam + 6 + NL;
    /* New scanzip_reg only reads central directory so use cext for ext */
    /* This is only an estimate */
    t += a[j] = 8 + LOCHEAD + CENHEAD +
           2 * (zoff_t)z->nam + 2 * (zoff_t)z->cext + z->com + z->siz;
    if (a[j] > c) {
      sprintf(errbuf, "Entry (%s) is larger than max split size of: %s",
          zip_fzofft(a[j], NULL, "u"), zip_fzofft(c, NULL, "u"));
      zipwarn(errbuf, "");
      zipwarn("use -n to set split size", "");
      ziperr(ZE_BIG, z->zname);
    }
  }

  /* Decide on split to use, report number of files */
  if (h) {
    s = simple(a, zcount, c, i);
    used_simple = 1;
  }
  else
  {
    if ((p = (uzoff_t *)talloc(zcount * sizeof(uzoff_t))) == NULL)
      ziperr(ZE_MEM, "was computing split");
    memcpy((char *)p, (char *)a, zcount * sizeof(uzoff_t));
    s = simple(a, zcount, c, i);
    g = greedy(p, zcount, c, i);
    if (s <= g) {
      tfree((zvoid *)p);
      used_simple = 1;
    }
    else
    {
      tfree((zvoid *)a);
      a = p;
      s = g;
      used_simple = 0;
    }
  }
  printf("%ld zip %s w%s be made (%s%% efficiency) using %s splitting\n",
         (ulg)s, (s == 1) ? "file" : "files", d ? "ould" : "ill",
         zip_fzofft( ((200 * ((t + c - 1)/c)) / s + 1) / 2, NULL, "d"),
         (used_simple == 1) ? "simple" : "greedy");
  if (d)
  {
    tfreeall();
    free((zvoid *)zipfile);
    zipfile = NULL;
    EXIT(ZE_OK);
  }

  /* Set up path for output files */
  /* Point "name" past the path, where the filename should go */
  path_size = (tempath == NULL ? (MAX_BASE + 5) : (int)strlen(tempath) + (MAX_BASE + 6));
  if ((path = (char *)talloc(path_size)) == NULL)
    ziperr(ZE_MEM, "was making output file names");
  if (tempath == NULL)
     name = path;
  else
  {
#ifndef VM_CMS
    /* Copy the output path to the target */
    strcpy(path, tempath);
#endif
#ifdef AMIGA
    tailchar = path[strlen(path) - 1];  /* last character */
    if (path[0] && (tailchar != '/') && (tailchar != ':'))
      strcat(path, "/");
#else
#ifdef RISCOS
    if (path[0] && path[strlen(path) - 1] != '.')
      strcat(path, ".");
#else
#ifdef QDOS
    if (path[0] && path[strlen(path) - 1] != '_')
      strcat(path, "_");
#else
#ifndef VMS
    if (path[0] && path[strlen(path) - 1] != '/')
      strcat(path, "/");
#endif /* !VMS */
#endif /* ?QDOS */
#endif /* ?RISCOS */
#endif /* ?AMIGA */
    name = path + strlen(path);
  }

  /* Make linked lists of results */
  if ((b = (extent *)talloc(s * sizeof(extent))) == NULL ||
      (n = (extent *)talloc(zcount * sizeof(extent))) == NULL)
    ziperr(ZE_MEM, "was computing split");
  for (j = 0; j < s; j++)
    b[j] = (extent)-1;
  j = zcount;
  while (j--)
  {
    g = (extent)a[j];
    n[j] = b[g];
    b[g] = j;
  }

  /* Make a name template for the zip files that is eight or less characters
     before the .zip, and that will not overwrite the original zip file. */
  for (k = 1, j = s; j >= 10; j /= 10)
    k++;
  if (k > 7)
    ziperr(ZE_PARMS, "way too many zip files must be made");
/*
 * XXX, ugly ....
 */
/* Find the final "path" separator character */
#ifdef QDOS
  q = LastDir(zipfile);
#else
#ifdef VMS
  if ((q = strrchr(zipfile, ']')) != NULL)
#else
#ifdef AMIGA
  if (((q = strrchr(zipfile, '/')) != NULL)
                       || ((q = strrchr(zipfile, ':'))) != NULL)
#else
#ifdef RISCOS
  if ((q = strrchr(zipfile, '.')) != NULL)
#else
#ifdef MVS
  if ((q = strrchr(zipfile, '.')) != NULL)
#else
  if ((q = strrchr(zipfile, '/')) != NULL)
#endif /* MVS */
#endif /* RISCOS */
#endif /* AMIGA */
#endif /* VMS */
    q++;
  else
    q = zipfile;
#endif /* QDOS */

  r = 0;
  while ((g = *q++) != '\0' && g != ZPATH_SEP && r < MAX_BASE - k)
    templat[r++] = (char)g;
  if (r == 0 || r < MAX_BASE - k)
    templat[r++] = '_';
  else if (g >= '0' && g <= '9')
    templat[r - 1] = (char)(templat[r - 1] == '_' ? '-' : '_');
  sprintf(templat + r, TEMPL_FMT, k);
#ifdef VM_CMS
  /* For CMS, add the "path" as the filemode at the end */
  if (tempath)
  {
     strcat(templat,".");
     strcat(templat,tempath);
  }
#endif

  /* Make the zip files from the linked lists of entry numbers */
  if ((e = fopen(zipfile, FOPR)) == NULL)
    ziperr(ZE_NAME, zipfile);
  free((zvoid *)zipfile);
  zipfile = NULL;
  for (j = 0; j < s; j++)
  {
    /* jump here on a disk retry */
  redobin:

    current_disk = 0;
    cd_start_disk = 0;
    cd_entries_this_disk = 0;

    /* prompt if requested */
    if (u)
    {
      char m[10];
      fprintf(mesg, "Insert disk #%ld of %ld and hit return: ",
              (ulg)j + 1, (ulg)s);
      fgets(m, 10, stdin);
    }

    /* write index file on first disk if requested */
    if (j == 0 && x)
    {
      strcpy(name, INDEX);
      printf("creating: %s\n", path);
      indexmade = 1;
      if ((f = fopen(path, "w")) == NULL)
      {
        if (u && retry()) goto redobin;
        ziperr(ZE_CREAT, path);
      }
      for (j = 0; j < zcount; j++)
        fprintf(f, "%5ld %s\n",
         (unsigned long)(a[j] + 1), w[j]->zname);

      if ((j = ferror(f)) != 0 || fclose(f))
      {
        if (j)
          fclose(f);
        if (u && retry()) goto redobin;
        ziperr(ZE_WRITE, path);
      }
    }

    /* create output zip file j */
    sprintf(name, templat, j + 1L);
    printf("creating: %s\n", path);
    zipsmade = (int)(j + 1);
    if ((y = f = fopen(path, FOPW)) == NULL)
    {
      if (u && retry()) goto redobin;
      ziperr(ZE_CREAT, path);
    }
    bytes_this_split = 0;
    tempzn = 0;

    /* write local headers and copy compressed data */
    for (g = b[j]; g != (extent)-1; g = (extent)n[g])
    {
      if (zfseeko(e, w[g]->off, SEEK_SET))
        ziperr(ferror(e) ? ZE_READ : ZE_EOF, zipfile);
      in_file = e;
      if ((r = zipcopy(w[g])) != ZE_OK)
      {
        if (r == ZE_TEMP)
        {
          if (u && retry()) goto redobin;
          ziperr(ZE_WRITE, path);
        }
        else
          ziperr(r, zipfile);
      }
    }

    /* write central headers */
    if ((c = zftello(f)) == (uzoff_t)-1)
    {
      if (u && retry()) goto redobin;
      ziperr(ZE_WRITE, path);
    }
    for (g = b[j], c_d_ents = 0; g != (extent)-1; g = n[g], c_d_ents++)
      if ((r = putcentral(w[g])) != ZE_OK)
      {
        if (u && retry()) goto redobin;
        ziperr(ZE_WRITE, path);
      }

    /* write end-of-central header */
    cd_start_offset = c;
    total_cd_entries = c_d_ents;
    if ((t = zftello(f)) == (zoff_t)-1 ||
        (r = putend( c_d_ents, t - c, c, (extent)0, (char *)NULL)) !=
        ZE_OK ||
        ferror(f) || fclose(f))
    {
      if (u && retry()) goto redobin;
      ziperr(ZE_WRITE, path);
    }
#ifdef RISCOS
    /* Set the filetype to &DDC */
    setfiletype(path,0xDDC);
#endif
  }
  fclose(e);

  /* Done! */
  if (u)
    fputs("Done.\n", mesg);
  tfreeall();

  if (tempath)
    free(tempath);

  RETURN(0);
}


/*
 * VMS (DEC C) initialization.
 */
#ifdef VMS
# include "decc_init.c"
#endif
