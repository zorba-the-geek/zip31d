/*
  fileio.c - Zip 3.1

  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
 *  fileio.c by Mark Adler
 */
#define __FILEIO_C

#include "zip.h"
#include "crc32.h"
#include <errno.h>

#ifdef MACOS
# include "helpers.h"
#endif

#ifdef UNIX_APPLE
#  include "unix/macosx.h"
#endif

#ifdef VMS
# include "vms/vms.h"
#endif /* def VMS */

#include <time.h>

/* Tru64 and SunOS 4.x need <sys/time.h> to get timeval. */
#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif /* def HAVE_SYS_TIME_H */

#ifdef NO_MKTIME
time_t mktime OF((struct tm *));
#endif

#ifdef OSF
# define EXDEV 18   /* avoid a bug in the DEC OSF/1 header files. */
#else
# include <errno.h>
#endif

#ifdef NO_ERRNO
extern int errno;
#endif

/* -----------------------
   For long option support
   ----------------------- */
#include <ctype.h>


/* -----------------------
   For zfprintf
   ----------------------- */
#include <stdlib.h>
#include <stdarg.h>

#ifdef WIN32
/* for locale, codepage support */
# include <locale.h>
# include <mbctype.h>
#endif

#ifdef HAVE_ICONV
# include <iconv.h>
#endif

#if defined(VMS) || defined(TOPS20)
#  define PAD 5
#else
#  define PAD 0
#endif

#ifdef NO_RENAME
int rename OF((ZCONST char *, ZCONST char *));
#endif


/* Local functions */
local int optionerr OF((char *, ZCONST char *, int, int));
local unsigned long get_shortopt OF((char **, int, int *, int *, char **, int *, int));
local unsigned long get_longopt OF((char **, int, int *, int *, char **, int *, int));

#ifdef UNICODE_SUPPORT
local int utf8_char_bytes OF((ZCONST char *utf8));
local long ucs4_char_from_utf8 OF((ZCONST char **utf8 ));
local int utf8_from_ucs4_char OF((char *utf8buf, ulg ch));
local int utf8_to_ucs4_string OF((ZCONST char *utf8, ulg *usc4buf,
                                  int buflen));
local int ucs4_string_to_utf8 OF((ZCONST ulg *ucs4, char *utf8buf,
                                  int buflen));
#if 0
  local int utf8_chars OF((ZCONST char *utf8));
#endif
#endif /* UNICODE_SUPPORT */

/* Progress dot display. */

#ifdef PROGRESS_DOTS_PER_FLUSH
local int dots_this_group;
#endif /* def PROGRESS_DOTS_PER_FLUSH */

local void display_dot_char(chr)
  int chr;
{
#ifdef WINDLL
  zprintf("%c", chr);
#else
  /* putc(chr, mesg); */
  zfprintf(mesg, "%c", chr);

  /* If PROGRESS_DOTS_PER_FLUSH is defined, then flush only when that
   * many dots have accumulated.  If not, then flush every time.
   */
# ifdef PROGRESS_DOTS_PER_FLUSH
  if (dots_this_group >= (PROGRESS_DOTS_PER_FLUSH))
  {
    /* Reset the dots-in-this-group count. */
    dots_this_group = 0;
# endif /* def PROGRESS_DOTS_PER_FLUSH */
  fflush(mesg);
# ifdef PROGRESS_DOTS_PER_FLUSH
  }
# endif /* def PROGRESS_DOTS_PER_FLUSH */
#endif
}

void display_dot(condition, size)
  int condition;
  int size;
{
  if (dot_size > 0)
  {
    /* initial space */
    if (noisy && dot_count == -1)
    {
      display_dot_char(' ');
      dot_count++;
# ifdef PROGRESS_DOTS_PER_FLUSH
      /* Reset the dots-in-this-group count. */
      dots_this_group = 0;
# endif /* def PROGRESS_DOTS_PER_FLUSH */
    }
    dot_count++;
    if ((condition <= 1) && (dot_size <= (dot_count+ 1) * size))
    {
      dot_count = 0;
    }
  }

  /* condition == 0: (verbose || noisy)
   * condition == 1: noisy
   * (I have no idea if these different conditions actually make sense,
   * but I'm only consolidating the stuff.)
   */
  if ((((condition == 0) && verbose) || noisy) && dot_size && !dot_count)
  {
# ifdef PROGRESS_DOTS_PER_FLUSH
    /* Increment the dots-in-this-group count. */
    dots_this_group++;
# endif /* def PROGRESS_DOTS_PER_FLUSH */
    display_dot_char('.');
    mesg_line_started = 1;
  }
}


#ifndef UTIL    /* the companion #endif is a bit of ways down ... */

local int fqcmp  OF((ZCONST zvoid *, ZCONST zvoid *));
/* old iname sort */
#if 0
local int fqcmpz OF((ZCONST zvoid *, ZCONST zvoid *));
#endif
/* new iname sort */
local int fqcmpz_icfirst OF((ZCONST zvoid *, ZCONST zvoid *));


/* Local module level variables. */
local z_stat zipstatb;             /* now use z_stat globally - 7/24/04 EG */
#ifdef UNICODE_SUPPORT_WIN32
 local zw_stat zipstatbw;
#endif
#if (!defined(MACOS) && !defined(WINDLL))
local int zipstate = -1;
#else
int zipstate;
#endif
/* -1 unknown, 0 old zip file exists, 1 new zip file */


#if 0
char *getnam(n, fp)
char *n;                /* where to put name (must have >=FNMAX+1 bytes) */
#endif

/* converted to return string pointer from malloc to avoid
   size limitation - 11/8/04 EG */
/* #define GETNAM_MAX 9000 */ /* hopefully big enough for now */

/* GETNAM_MAX should probably be set to the max path length for the
   system we are running on - 9/3/2011 EG */

/* Now just set to max length allowed for paths. */
#define GETNAM_MAX MAX_PATH_SIZE

char *getnam(fp)
  FILE *fp;
  /* Read a \n or \r delimited name from stdin (or whereever fp points)
     into n, and return n.  If EOF, then return NULL.  Also, if problem
     return NULL. */
  /* Assumes names do not include '\0'. */
  /* As of Zip 3.1, we trim leading and trailing white space, unless
     name is surrounded by double quotes, in which case trimming is
     only to the quotes and the quotes are then removed. */
{
  char name[GETNAM_MAX + 1];
  int c;                /* last character read */
  char *p;              /* pointer into name area */
  char name2[GETNAM_MAX + 1];
  int name_start;
  int name_end;
  unsigned char uc;
  int i;


  while (1) {
    p = name;
    while ((c = getc(fp)) == '\n' || c == '\r')
      ;
    if (c == EOF)
      return NULL;
    do {
      if (p - name >= GETNAM_MAX) {
        name[GETNAM_MAX - 1] = '\0';
        sprintf(errbuf, "name read from file greater than max path length:  %d\n          %s",
                GETNAM_MAX, name);
        ZIPERR(ZE_PARMS, errbuf);
        return NULL;
      }
      *p++ = (char) c;
      c = getc(fp);
    } while (c != EOF && (c != '\n' && c != '\r'));
#ifdef WIN32
  /*
   * WIN32 strips off trailing spaces and periods in filenames
   * XXX what about a filename that only consists of spaces ?
   *     Answer: on WIN32, a filename must contain at least one non-space char
   */
    while (p > name) {
      if ((c = p[-1]) != ' ' && c != '.')
        break;
      --p;
    }
#endif
    *p = 0;

    /* trimming and quotes */
    for (name_start = 0; (uc = (unsigned char)name[name_start]); name_start++) {
      if (!isspace(uc)) break;
    }
    for (name_end = (int)strlen(name) - 1; name_end >= 0; name_end--) {
      uc = (unsigned char)name[name_end];
      if (!isspace(uc)) break;
    }
    if (name[name_start] == '"' && name_start < name_end && name[name_end] == '"') {
      /* trim outer quotes */
      name_start++;
      name_end--;
    }
    for (i = name_start; i <= name_end; i++) {
      name2[i - name_start] = name[i];
    }
    name2[i - name_start] = '\0';

    /* if we end up with an empty string, just loop and try again */
    if (name2[0])
      break;
  } /* while */

  /* malloc a copy */
  if ((p = malloc(strlen(name2) + 1)) == NULL) {
    return NULL;
  }
  strcpy(p, name2);
  return p;
}

struct flist far *fexpel(f)
struct flist far *f;    /* entry to delete */
/* Delete the entry *f in the doubly-linked found list.  Return pointer to
   next entry to allow stepping through list. */
{
  struct flist far *t;  /* temporary variable */

  t = f->nxt;
  *(f->lst) = t;                        /* point last to next, */
  if (t != NULL)
    t->lst = f->lst;                    /* and next to last */
  if (f->name != NULL)                  /* free memory used */
    free((zvoid *)(f->name));
  if (f->zname != NULL)
    free((zvoid *)(f->zname));
  if (f->iname != NULL)
    free((zvoid *)(f->iname));
#ifdef UNICODE_SUPPORT
  if (f->uname)
    free((zvoid *)f->uname);
# ifdef WIN32
  if (f->namew)
    free((zvoid *)f->namew);
  if (f->inamew)
    free((zvoid *)f->inamew);
  if (f->znamew)
    free((zvoid *)f->znamew);
# endif
#endif
  farfree((zvoid far *)f);
  fcount--;                             /* decrement count */
  return t;                             /* return pointer to next */
}


/* ---------------------------------------------- */
/* sort functions */

local int fqcmp(a, b)
  ZCONST zvoid *a, *b;          /* pointers to pointers to found entries */
/* Used by qsort() to compare entries in the found list by name. */
{
  return strcmp((*(struct flist far **)a)->name,
                (*(struct flist far **)b)->name);
}

/* fqcmpz() was old iname sort function, now replaced by fqcmpz_icfirst(). */
#if 0

local int fqcmpz(a, b)
  ZCONST zvoid *a, *b;          /* pointers to pointers to found entries */
/* Used by qsort() to compare entries in the found list by iname. */
{
  return strcmp((*(struct flist far **)a)->iname,
                (*(struct flist far **)b)->iname);
}

#endif

/* New iname sort function, replacing fqcmpz(). */

local int fqcmpz_icfirst(a, b)
  ZCONST zvoid *a, *b;          /* pointers to pointers to found entries */
/* Used by qsort() to compare entries in the found list by iname. */
/* Sorts ignoring case first, but when strings match uses case.
   This sorts uniquely, but similar to how Unix shell lists files. */

/* A port that has no ignore case compare can just use strcmp(). */
{
  int i;

  /* first sort ignoring case */
#ifdef WIN32
  i = _stricmp((*(struct flist far **)a)->iname,
               (*(struct flist far **)b)->iname);
#else
  i = strcasecmp((*(struct flist far **)a)->iname,
                 (*(struct flist far **)b)->iname);
#endif
  if (i) return i;
  /* if strings match ignoring case, use case */
  return strcmp((*(struct flist far **)a)->iname,
                (*(struct flist far **)b)->iname);
}

/* ---------------------------------------------- */

char *last(p, c)
  char *p;                /* sequence of path components */
  int c;                  /* path components separator character */
/* Return a pointer to the start of the last path component. For a directory
 * name terminated by the character in c, the return value is an empty string.
 */
{
  char *t;              /* temporary variable */

  if ((t = strrchr(p, c)) != NULL)
    return t + 1;
  else
#ifndef AOS_VS
    return p;
#else
/* We want to allow finding of end of path in either AOS/VS-style pathnames
 * or Unix-style pathnames.  This presents a few little problems ...
 */
  {
    if (*p == '='  ||  *p == '^')      /* like ./ and ../ respectively */
      return p + 1;
    else
      return p;
  }
#endif
}

#ifdef UNICODE_SUPPORT_WIN32
wchar_t *lastw(pw, c)
  wchar_t *pw;            /* sequence of path components */
  wchar_t c;              /* path components separator character */
/* Return a pointer to the start of the last path component. For a directory
 * name terminated by the character in c, the return value is an empty string.
 */
{
  wchar_t *tw;            /* temporary variable */

  if ((tw = wcsrchr(pw, c)) != NULL)
    return tw + 1;
  else
# ifndef AOS_VS
    return pw;
# else
/* We want to allow finding of end of path in either AOS/VS-style pathnames
 * or Unix-style pathnames.  This presents a few little problems ...
 */
  {
    if (*pw == (wchar_t)'='  ||  *pw == (wchar_t)'^')      /* like ./ and ../ respectively */
      return pw + 1;
    else
      return pw;
  }
# endif
}
#endif /* UNICODE_SUPPORT_WIN32 */


char *msname(n)
  char *n;
/* Reduce all path components to MSDOS upper case 8.3 style names. */
{
  int c;                /* current character */
  int f;                /* characters in current component */
  char *p;              /* source pointer */
  char *q;              /* destination pointer */

  p = q = n;
  f = 0;
  while ((c = (unsigned char)*POSTINCSTR(p)) != 0)
    if (c == ' ' || c == ':' || c == '"' || c == '*' || c == '+' ||
        c == ',' || c == ';' || c == '<' || c == '=' || c == '>' ||
        c == '?' || c == '[' || c == ']' || c == '|')
      continue;                         /* char is discarded */
    else if (c == '/')
    {
      *POSTINCSTR(q) = (char)c;
      f = 0;                            /* new component */
    }
#ifdef __human68k__
    else if (ismbblead(c) && *p)
    {
      if (f == 7 || f == 11)
        f++;
      else if (*p && f < 12 && f != 8)
      {
        *q++ = c;
        *q++ = *p++;
        f += 2;
      }
    }
#endif /* __human68k__ */
    else if (c == '.')
    {
      if (f == 0)
        continue;                       /* leading dots are discarded */
      else if (f < 9)
      {
        *POSTINCSTR(q) = (char)c;
        f = 9;                          /* now in file type */
      }
      else
        f = 12;                         /* now just excess characters */
    }
    else
      if (f < 12 && f != 8)
      {
        f += CLEN(p);                   /* do until end of name or type */
        *POSTINCSTR(q) = (char)(to_up(c));
      }
  *q = 0;
  return n;
}

#ifdef UNICODE_SUPPORT_WIN32
wchar_t *msnamew(nw)
  wchar_t *nw;
/* Reduce all path components to MSDOS upper case 8.3 style names. */
{
  wchar_t c;            /* current character */
  int f;                /* characters in current component */
  wchar_t *pw;          /* source pointer */
  wchar_t *qw;          /* destination pointer */

  pw = qw = nw;
  f = 0;
  while ((c = (unsigned char)*pw++) != 0)
    if (c == ' ' || c == ':' || c == '"' || c == '*' || c == '+' ||
        c == ',' || c == ';' || c == '<' || c == '=' || c == '>' ||
        c == '?' || c == '[' || c == ']' || c == '|')
      continue;                         /* char is discarded */
    else if (c == '/')
    {
      *qw++ = c;
      f = 0;                            /* new component */
    }
#ifdef __human68k__
    else if (ismbblead(c) && *pw)
    {
      if (f == 7 || f == 11)
        f++;
      else if (*pw && f < 12 && f != 8)
      {
        *qw++ = c;
        *qw++ = *pw++;
        f += 2;
      }
    }
#endif /* __human68k__ */
    else if (c == '.')
    {
      if (f == 0)
        continue;                       /* leading dots are discarded */
      else if (f < 9)
      {
        *qw++ = c;
        f = 9;                          /* now in file type */
      }
      else
        f = 12;                         /* now just excess characters */
    }
    else
      if (f < 12 && f != 8)
      {
        f++;                            /* do until end of name or type */
        *qw++ = towupper(c);
      }
  *qw = 0;
  return nw;
}
#endif /* UNICODE_SUPPORT_WIN32 */


int proc_archive_name(n, caseflag)
  char *n;              /* name to process */
  int caseflag;         /* true to force case-sensitive match */
/* Process a name or sh expression in existing archive to operate
   on (or exclude).  Return an error code in the ZE_ class. */
{
  int m;                /* matched flag */
  char *p;              /* path for recursion */
  struct zlist far *z;  /* steps through zfiles list */
#ifdef UNICODE_SUPPORT_WIN32
  int utf8 = is_utf8_string(n, NULL, NULL, NULL, NULL);
  wchar_t *pw;
#endif

  if (strcmp(n, "-") == 0) {   /* if compressing stdin */
    zipwarn("Cannot select stdin when selecting archive entries", "");
    return ZE_MISS;
  }
  else
  {
    /* Search for shell expression in zip file */
    p = ex2in(n, 0, (int *)NULL);       /* shouldn't affect matching chars */
    m = 1;
#ifdef UNICODE_SUPPORT_WIN32
    if (utf8)
      pw = utf8_to_wchar_string(p);
    else
      pw = local_to_wchar_string(p);
#endif
    for (z = zfiles; z != NULL; z = z->nxt) {
#ifdef UNICODE_SUPPORT_WIN32
      if (MATCHW(pw, z->znamew, caseflag))
      {
        char *uzname = wchar_to_utf8_string(z->znamew);
        z->mark = pcount ? filter(uzname, caseflag) : 1;
        free(uzname);
        if (verbose)
            zfprintf(mesg, "zip diagnostic: %scluding %s\n",
               z->mark ? "in" : "ex", z->oname);
        m = 0;
      }
      free(pw);
#else
      if (MATCH(p, z->iname, caseflag))
      {
        z->mark = pcount ? filter(z->zname, caseflag) : 1;
        if (verbose)
            zfprintf(mesg, "zip diagnostic: %scluding %s\n",
               z->mark ? "in" : "ex", z->oname);
        m = 0;
      }
#endif
    }
#ifdef UNICODE_SUPPORT
    /* also check escaped Unicode names */
    for (z = zfiles; z != NULL; z = z->nxt) {
      if (z->zuname) {
#ifdef UNICODE_SUPPORT_WIN32
        /* It seems something is lost in going from a listed
           name from zip -su in a console window to using that
           name in a command line.  This kluge may fix it
           and just takes zuname, converts to oem (i.e. ouname),
           then converts it back which ends up not the same as
           started with.
         */
        char *zuname = z->wuname;
#else
        char *zuname = z->zuname;
#endif
        if (MATCH(p, zuname, caseflag))
        {
          z->mark = pcount ? filter(zuname, caseflag) : 1;
          if (verbose) {
              zfprintf(mesg, "zip diagnostic: %scluding %s\n",
                 z->mark ? "in" : "ex", z->oname);
              zfprintf(mesg, "     Escaped Unicode:  %s\n",
                 z->ouname);
          }
          m = 0;
        }
      }
    }
#endif
    free((zvoid *)p);
    return m ? ZE_MISS : ZE_OK;
  }
}


int check_dup_sort(sort_found_list)
  int sort_found_list;
/* Sort the found list and remove duplicates.
   Return an error code in the ZE_ class. */
{
  struct flist far *f;          /* steps through found linked list */
  extent j, k;                  /* indices for s */
  struct flist far **s;         /* sorted table */
  struct flist far **nodup;     /* sorted table without duplicates */
  struct flist far *t;          /* temporary variable */
#ifdef UNIX_APPLE_SORT
  char *iname;
  int slash;
  int iname_len;
  int jj;
#endif

#if 0
  zprintf(" {check_dup_sort}\n");
#endif

  /* sort found list, remove duplicates */
  if (fcount)
  {
    extent fl_size = fcount * sizeof(struct flist far *);
    if ((fl_size / sizeof(struct flist far *)) != fcount ||
        (s = (struct flist far **)malloc(fl_size)) == NULL)
      return ZE_MEM;
    for (j = 0, f = found; f != NULL; f = f->nxt)
      s[j++] = f;
    /* Check names as given (f->name) */
    qsort((char *)s, fcount, sizeof(struct flist far *), fqcmp);
    for (k = j = fcount - 1; j > 0; j--)
      if (strcmp(s[j - 1]->name, s[j]->name) == 0)
        /* remove duplicate entry from list */
        fexpel(s[j]);           /* fexpel() changes fcount */
      else
        /* copy valid entry into destination position */
        s[k--] = s[j];
    s[k] = s[0];                /* First entry is always valid */
    nodup = &s[k];              /* Valid entries are at end of array s */


#ifdef UNIX_APPLE_SORT
    /* This presort code is executed if:
         Platform is Unix Apple and:
           -ad- is not used to turn off AD sorting.
             dir/file -> dir/._file
         Platform is not Unix Apple and:
           -ad is used to turn on file system AppleDouble sorting.
       Both normal "._" and sequestered "__MACOSX/._" AppleDouble paths
       are sorted the same, except that if both occur for a primary
       file, the normal AppleDouble path is preferred.
       This code preprocesses the found list, replacing normal "._" and
       sequestered "__MACOSX/._" AppleDouble paths with paths that sort
       correctly.  The original paths are saved and restored after the sort. */
    if (sort_apple_double) {
      int seq;

      for (j = 0; j < fcount; j++)
      {
        nodup[j]->saved_iname = NULL;
        seq = 0;
        if (sort_apple_double_all || IS_ZFLAG_APLDBL(nodup[j]->zflags)) {
          /* save the actual path to restore it after sort */
          nodup[j]->saved_iname = nodup[j]->iname;
          /* make copy to alter so sorts where we want */
          iname = string_dup(nodup[j]->iname, "Unix Apple sort", 0);
          iname_len = (int)strlen(iname);
          /* sequestered paths */
          if (strncmp(iname, "__MACOSX/", 9) == 0) {
            /* remove leading sequester dir */
            for (jj = 9; jj <= iname_len; jj++) {
              iname[jj - 9] = iname[jj];
            }
            iname_len = (int)strlen(iname);
            seq = 1;
          } /* __MACOSX */
          /* find last slash */
          for (slash = iname_len - 1; slash >= 0; slash--) {
            if (iname[slash] == '/') break;
          }
          if (slash < iname_len - 3 &&
              iname[slash + 1] == '.' &&
              iname[slash + 2] == '_') {
            /* looks like AppleDouble file - convert to sort format */
            /* somedir/._filename -> somedir/filename@, where @ = \01 */
            for (jj = slash + 1; jj < iname_len - 1; jj++) {
              iname[jj] = iname[jj + 2];
            }
            /* If both normal "._" and sequestered AppleDouble paths exist
               for the same main entry, normal AppleDouble paths are given
               precedence over sequestered paths as far as who gets to be
               directly below the main entry.  If Zip is creating AD paths
               internally, they should be all normal or all sequestered
               and both should not occur for the same primary file.  If
               -ad is used to sort AD paths from the file system, then
               no telling what might be read, and this order may matter. */
            if (seq)
              iname[iname_len - 2] = '\02';
            else
              iname[iname_len - 2] = '\01';
            iname[iname_len - 1] = '\0';
            nodup[j]->iname = iname;
          }
          else {
            free(iname);
            nodup[j]->saved_iname = NULL;
          }
        } /* AppleDouble */
      } /* for */
    } /* sort_apple_double */
#endif

    /* sort only valid items and check for unique internal names (f->iname) */

    /* use fqcmpz_icfirst, which sorts first ignoring case, then using case,
       which tends to sort in same order as Unix shell */
    qsort((char *)nodup, fcount, sizeof(struct flist far *), fqcmpz_icfirst);

#if 0
    /* for ports where we don't know if fqcmpz_icfirst() is supported */
    qsort((char *)nodup, fcount, sizeof(struct flist far *), fqcmpz);
#endif


#ifdef UNIX_APPLE_SORT
    /* Restore AppleDouble paths. */
    if (sort_apple_double) {
      for (j = 0; j < fcount; j++)
      {
        if (nodup[j]->saved_iname) {
          free(nodup[j]->iname);
          nodup[j]->iname = nodup[j]->saved_iname;
        }
      } /* for */
    } /* sort_apple_double */
#endif

    for (j = 1; j < fcount; j++)
    {
      if (strcmp(nodup[j - 1]->iname, nodup[j]->iname) == 0)
      {
        char tempbuf[ERRBUF_SIZE + 1];  /* was FNMAX+4081 */

        sprintf(errbuf, "  first full name: %s\n", nodup[j - 1]->name);
        sprintf(tempbuf, " second full name: %s\n", nodup[j]->name);
        strcat(errbuf, "                     ");
        strcat(errbuf, tempbuf);
#ifdef EBCDIC
        strtoebc(nodup[j]->iname, nodup[j]->iname);
#endif
        sprintf(tempbuf, "name in zip file repeated: %s", nodup[j]->iname);
        strcat(errbuf, "                     ");
        strcat(errbuf, tempbuf);
        if (pathput == 0) {
          strcat(errbuf, "\n                     this may be a result of using -j");
        }
#ifdef EBCDIC
        strtoasc(nodup[j]->iname, nodup[j]->iname);
#endif
        zipwarn(errbuf, "");
        return ZE_PARMS;
      }
    }


    /* ------------------------------------ */
    /* sort the found list */

# if 0
    printf(" { updating found list}\n");
# endif

    if (sort_found_list) {
# if 0
      /* for timing */
      uzoff_t startt, endt;
      long difft;
      long dircnt;

      startt = get_time_in_usec();
# endif

      /* update found list order
       *
       * Unix ports tend to deliver files from directory scans in a
       * seemingly random order.  On many other ports, directory
       * scans return files in alphabetical order, which is then the
       * order the files are stored in Zip archives.  This being
       * very convenient, the below adds alphabetical sorting to
       * new items being added to an archive.
       *
       * Initially the directory scans were sorted to achieve this,
       * but given a much larger number of items can be returned
       * from a scan than Zip will actually store, sorting at that
       * point is inefficient, especially since each opendir required
       * in a directory tree scan would need a new sort.
       *
       * Instead, the approach here is to use the already sorted
       * iname list to update the found list.  We just rebuild the
       * found list based on the sorted nodup[] entries.  Typically
       * this takes milliseconds for large numbers of files, where
       * sorting each directory scan could take magnitudes more
       * time. 
       */

      /* The found list is never navigated backwards.  In fact, it
         would not be easy to do as the ->lst fields in the previous
         record point to the current record, not back to the previous
         one.  There is no direct pointer back to the previous (last)
         record in the list.  ->lst fields are used by fexpel() to
         bridge over an expelled record.

         There's no reason to work with the existing links in the
         found list.  Easiest just to rebuild it from scratch in the
         order we need. */  

      found = nodup[0];
      fnxt = &found;
      found->lst = fnxt;
      fnxt = &found->nxt;

      for (j = 1; j < fcount; j++) {
        t = nodup[j];
        *fnxt = t;
        t->lst = fnxt;
        t->nxt = NULL;
        fnxt = &t->nxt;
      }

# if 0
      endt = get_time_in_usec();
      difft = (long)(endt - startt);
      zprintf(" {timedif = %ld}\n", difft);
# endif

    } /* sort_found_list */

/* ------------------------------------ */

   
    free((zvoid *)s);
  }
  return ZE_OK;
}

int filter(name, casesensitive)
  char *name;
  int casesensitive;
  /* Scan the -R, -i and -x lists for matches to the given name.
     Return TRUE if the name must be included, FALSE otherwise.
     Give precedence to -x over -i and -R.
     Note that if both R and i patterns are given then must
     have a match for both.
     This routine relies on the following global variables:
       patterns                 array of match pattern structures
       pcount                   total number of patterns
       icount                   number of -i patterns
       Rcount                   number of -R patterns
     These data are set up by the command line parsing code.

     If UNICODE_SUPPORT and name is UTF-8, the match is against the
     UTF-8 pattern and the de-escaped UTF-8 pattern.  Otherwise name
     is matched against just the local charset zname pattern as before.
   */
{
   unsigned int n;
   int slashes;
   char *p, *q;
   /* without -i patterns, every name matches the "-i select rules" */
   int imatch = (icount == 0);
   /* without -R patterns, every name matches the "-R select rules" */
   int Rmatch = (Rcount == 0);
   char *pattern;
   int match = 0;
   char *dpattern;
#ifdef UNICODE_SUPPORT
   int utf8 = is_utf8_string(name, NULL, NULL, NULL, NULL);
#endif

   if (pcount == 0) return TRUE;

   for (n = 0; n < pcount; n++) {
      if (!patterns[n].zname[0])        /* it can happen... */
         continue;
      p = name;
#ifdef UNICODE_SUPPORT
      if (utf8 && patterns[n].uzname) {
        pattern = patterns[n].uzname;
        /* de-escaped name - Unicode escapes converted back to UTF-8 */
        dpattern = patterns[n].duzname;
      }
      else {
        pattern = patterns[n].zname;
        dpattern = NULL;
      }
#else
      pattern = patterns[n].zname;
      dpattern = NULL;
#endif
      switch (patterns[n].select) {
       case 'R':
         if (Rmatch)
            /* one -R match is sufficient, skip this pattern */
            continue;
         /* With -R patterns, if the pattern has N path components (that is,
            N-1 slashes), then we test only the last N components of name.
          */
         slashes = 0;
         /* Assume pattern and dpattern have slashes in the same spots. */
         for (q = pattern; (q = MBSCHR(q, '/')) != NULL; MB_NEXTCHAR(q))
            slashes++;
         /* The name may have M path components (M-1 slashes) */
         for (q = p; (q = MBSCHR(q, '/')) != NULL; MB_NEXTCHAR(q))
            slashes--;
         /* Now, "slashes" contains the difference "N-M" between the number
            of path components in the pattern (N) and in the name (M).
          */
         if (slashes < 0)
            /* We found "M > N"
                --> skip the first (M-N) path components of the name.
             */
            for (q = p; (q = MBSCHR(q, '/')) != NULL; MB_NEXTCHAR(q))
               if (++slashes == 0) {
                  p = q + 1;    /* q points at '/', mblen("/") is 1 */
                  break;
               }
         break;
       case 'i':
         if (imatch)
            /* one -i match is sufficient, skip this pattern */
            continue;
         break;
      }

      match = MATCH(pattern, p, casesensitive);
      if (dpattern && !match)
        match = MATCH(dpattern, p, casesensitive);

      if (match) {
         switch (patterns[n].select) {
            case 'x':
               /* The -x match takes precedence over everything else */
               return FALSE;
            case 'R':
               Rmatch = TRUE;
               break;
            default:
               /* this must be a type -i match */
               imatch = TRUE;
               break;
         }
      }
   }
   return imatch && Rmatch;
}


#ifdef UNICODE_SUPPORT_WIN32
int newnamew(namew, zflags, casesensitive)
  wchar_t *namew;             /* name to add (or exclude) */
  int  zflags;                /* true for a directory */
  int  casesensitive;         /* true for case-sensitive matching */
/* Add (or exclude) the name of an existing disk file.  Return an error
   code in the ZE_ class. */
{
  wchar_t *inamew = NULL;     /* internal name */
  wchar_t *znamew = NULL;     /* external version of iname */
  wchar_t *undosmw = NULL;    /* zname version with "-j" and "-k" options disabled */
  char *oname = NULL;         /* zname converted for display */
  char *name = NULL;
  char *iname = NULL;
  char *zname = NULL;
  char *zuname = NULL;
  char *undosm = NULL;
  struct flist far *f;        /* where in found, or new found entry */
  struct zlist far *z;        /* where in zfiles (if found) */
  int dosflag;
  int is_utf8 = 0;
  char *path = NULL;

  /* Scanning files ...
   *
   * After 5 seconds output Scanning files...
   * then a dot every 2 seconds
   */
  if (noisy) {
    /* If find files then output message after delay */
    if (scan_count == 0) {
      time_t current = time(NULL);
      scan_start = current;
    }
    scan_count++;
    if (scan_count % 100 == 0) {
      time_t current = time(NULL);

      if (current - scan_start > scan_delay) {
        if (scan_last == 0) {
          zipmessage_nl("Scanning files ", 0);
          scan_last = current;
        }
        if (current - scan_last > scan_dot_time) {
          scan_last = current;
          zfprintf(mesg, ".");
          fflush(mesg);
        }
      }
    }
  }

  /* Search for name in zip file.  If there, mark it, else add to
     list of new names to do (or remove from that list). */
  if ((inamew = ex2inw(namew, IS_ZFLAG_DIR(zflags), &dosflag)) == NULL)
    return ZE_MEM;

  /* Discard directory names with zip -rj */
  if (*inamew == (wchar_t)'\0') {

 /* If extensions needs to be swapped, we will have empty directory names
    instead of the original directory. For example, zipping 'c.', 'c.main'
    should zip only 'main.c' while 'c.' will be converted to '\0' by ex2in. */

    if (pathput && !recurse) error("empty name without -j or -r");
    free((zvoid *)inamew);
    return ZE_OK;
  }

  if (dosflag || !pathput) {
    int save_dosify = dosify, save_pathput = pathput;
    dosify = 0;
    pathput = 1;
    /* zname is temporarly mis-used as "undosmode" iname pointer */
    if ((znamew = ex2inw(namew, IS_ZFLAG_DIR(zflags), NULL)) != NULL) {
      undosmw = in2exw(znamew);
      free(znamew);
    }
    dosify = save_dosify;
    pathput = save_pathput;
  }
  if ((znamew = in2exw(inamew)) == NULL)
    return ZE_MEM;

  /* Convert names from wchar_t to char */

  name = wchar_to_local_string(namew);
  iname = wchar_to_local_string(inamew);
  zname = wchar_to_local_string(znamew);

  oname = local_to_display_string(zname);

  zuname = wchar_to_utf8_string(znamew);

  if (undosmw == NULL)
    undosmw = znamew;
  undosm = wchar_to_utf8_string(undosmw);

  /* first look for UTF-8 in zlist, as paths in new archives
     should be UTF-8 */
  path = zuname;                /* UTF-8 version of zname */
  if ((z = zsearch(zuname)) == NULL) {
    /* if not found, look for local version of name, as
       an old archive might be using local paths */
    /* this should also catch escaped Unicode */
    path = zname;               /* local charset zname */
    z = zsearch(zname);
  }

  if (z != NULL) {
    if (pcount && !filter(path, casesensitive)) {
      /* Do not clear z->mark if "exclude", because, when "dosify || !pathput"
       * is in effect, two files with different filter options may hit the
       * same z entry.
       */
      if (verbose)
        zfprintf(mesg, "excluding %s\n", oname);
    } else {
      z->mark = 1;
      if ((z->name = malloc(strlen(name) + 1 + PAD)) == NULL) {
        if (undosmw != znamew)
          free(undosmw);
        if (undosm) free(undosm);
        if (inamew) free(inamew);
        if (znamew) free(znamew);
        if (name) free(name);
        if (iname) free(iname);
        if (zname) free(zname);
        if (oname) free(oname);
        if (zuname) free(zuname);
        return ZE_MEM;
      }
      strcpy(z->name, name);
      if (z->oname) free(z->oname);
      z->oname = oname;
      oname = NULL;
      z->dosflag = dosflag;

#ifdef FORCE_NEWNAME
      free((zvoid *)(z->iname));
      z->iname = iname;
      iname = NULL;
#else
      /* Better keep the old name. Useful when updating on MSDOS a zip file
       * made on Unix.
       */
#endif /* ? FORCE_NEWNAME */
    }

    if ((z->namew = (wchar_t *)malloc((wcslen(namew) + 1) * sizeof(wchar_t))) == NULL) {
      if (undosmw != znamew)
        free(undosmw);
      if (undosm) free(undosm);
      if (inamew) free(inamew);
      if (znamew) free(znamew);
      if (name) free(name);
      if (iname) free(iname);
      if (zname) free(zname);
      if (oname) free(oname);
      if (zuname) free(zuname);
      return ZE_MEM;
    }
    wcscpy(z->namew, namew);
    z->inamew = inamew;
    inamew = NULL;
    z->znamew = znamew;
    znamew = NULL;
    if (!is_ascii_stringw(z->inamew))
      z->uname = wchar_to_utf8_string(z->inamew);
    if (name == label) {
       label = z->name;
    }
    z->zflags = zflags;
  } else if (pcount == 0 || filter(undosm, casesensitive)) {

    /* Check that we are not adding the zip file to itself. This
     * catches cases like "zip -m foo ../dir/foo.zip".
     */
/* Version of stat() for CMS/MVS isn't complete enough to see if       */
/* files match.  Just let ZIP.C compare the filenames.  That's good    */
/* enough for CMS anyway since there aren't paths to worry about.      */
    zw_stat statbw;     /* need for wide stat */
    wchar_t *zipfilew = local_to_wchar_string(zipfile);

    if (zipstate == -1)
       zipstate = strcmp(zipfile, "-") != 0 &&
                   zwstat(zipfilew, &zipstatbw) == 0;
    free(zipfilew);

    if (zipstate == 1 && (statbw = zipstatbw, zwstat(namew, &statbw) == 0
      && zipstatbw.st_mode  == statbw.st_mode
      && zipstatbw.st_ino   == statbw.st_ino
      && zipstatbw.st_dev   == statbw.st_dev
      && zipstatbw.st_uid   == statbw.st_uid
      && zipstatbw.st_gid   == statbw.st_gid
      && zipstatbw.st_size  == statbw.st_size
      && zipstatbw.st_mtime == statbw.st_mtime
      && zipstatbw.st_ctime == statbw.st_ctime)) {
      /* Don't compare a_time since we are reading the file */
        if (verbose)
          zfprintf(mesg, "file matches zip file -- skipping\n");
        if (undosmw != znamew)
          free(undosmw);
        if (undosm) free(undosm);
        if (inamew) free(inamew);
        if (znamew) free(znamew);
        if (name) free(name);
        if (iname) free(iname);
        if (zname) free(zname);
        if (oname) free(oname);
        if (zuname) free(zuname);
        return ZE_OK;
    }

    /* allocate space and add to list */
    if ((f = (struct flist far *)farmalloc(sizeof(struct flist))) == NULL ||
        fcount + 1 < fcount ||
        (f->name = malloc(strlen(name) + 1 + PAD)) == NULL)
    {
      if (f != NULL)
        farfree((zvoid far *)f);
      if (undosmw != znamew)
        free(undosmw);
      if (undosm) free(undosm);
      if (inamew) free(inamew);
      if (znamew) free(znamew);
      if (name) free(name);
      if (iname) free(iname);
      if (zname) free(zname);
      if (oname) free(oname);
      if (zuname) free(zuname);
      return ZE_MEM;
    }
    if (undosmw != znamew)
      free((zvoid *)undosmw);
    strcpy(f->name, name);
    f->iname = iname;
    iname = NULL;
    f->zname = zname;
    zname = NULL;
    /* Unicode */
    if ((f->namew = (wchar_t *)malloc((wcslen(namew) + 1) * sizeof(wchar_t))) == NULL) {
      if (f != NULL)
        farfree((zvoid far *)f);
      if (undosmw != znamew)
        free(undosmw);
      if (undosm) free(undosm);
      if (inamew) free(inamew);
      if (znamew) free(znamew);
      if (name) free(name);
      if (iname) free(iname);
      if (zname) free(zname);
      if (oname) free(oname);
      if (zuname) free(zuname);
      return ZE_MEM;
    }
    wcscpy(f->namew, namew);
    f->znamew = znamew;
    znamew = NULL;
    f->uname = wchar_to_utf8_string(inamew);
    f->inamew = inamew;
    inamew = NULL;
    f->oname = oname;
    oname = NULL;
    f->dosflag = dosflag;

    f->zflags = zflags;

    *fnxt = f;
    f->lst = fnxt;
    f->nxt = NULL;
    fnxt = &f->nxt;
    fcount++;
    if (name == label) {
      label = f->name;
    }
  }
  if (undosm) free(undosm);
  if (inamew) free(inamew);
  if (znamew) free(znamew);
  if (name) free(name);
  if (iname) free(iname);
  if (zname) free(zname);
  if (oname) free(oname);
  if (zuname) free(zuname);
  return ZE_OK;
}
#endif /* UNICODE_SUPPORT_WIN32 */

#ifndef NO_PROTO
int newname(char *name, int zflags, int casesensitive)
#else
int newname(name, zflags, casesensitive)
  char *name;           /* name to add (or exclude) */
  int  zflags;          /* ZFLAG_DIR = directory, ZFLAG_APLDBL = AppleDouble */
  int  casesensitive;   /* true for case-sensitive matching */
#endif
/* Add (or exclude) the name of an existing disk file.  Return an error
   code in the ZE_ class. */
{

#ifdef UNIX_APPLE
  /* AppleDouble special name pointers.
   * name_archv is stored in the archive.
   * name_flsys is sought in the file system.
   */
  char *name_archv;     /* Arg name or AppleDouble "._" name. */
  char *name_flsys;     /* Arg name or AppleDouble "/rsrc" name. */
#else /* not UNIX_APPLE */
  /* On non-Mac-OS-X systems, use the file name argument as-is. */
# define name_archv name
# define name_flsys name
#endif /* not UNIX_APPLE */

  char *iname, *zname;  /* internal name, external version of iname */
  char *undosm;         /* zname version with "-j" and "-k" options disabled */
  char *oname;          /* zname converted for display */
  struct flist far *f;  /* where in found, or new found entry */
  struct zlist far *z;  /* where in zfiles (if found) */
  int dosflag;
  int pad_name;         /* Pad for name (may include APL_DBL_xxx). */

#ifdef UNIX_APPLE
  /* Create special names for an AppleDouble file. */
  if (IS_ZFLAG_APLDBL(zflags))
  {
    char *name_archv_p;
    char *rslash;

    /* For the in-archive name, always allow for "._".
     * If sequestering, add space for "__MACOSX/", too.
     */
    pad_name = sizeof( APL_DBL_PFX);    /* One "sizeof" for the NUL, */
    if (sequester)                      /* "strlen" for the others.  */
      pad_name += strlen( APL_DBL_PFX_SQR);

    /* Allocate storage for modified AppleDouble file names. */
    if ((name_archv = malloc( strlen( name)+ pad_name)) == NULL)
      return ZE_MEM;

    if ((name_flsys = malloc( strlen( name)+ sizeof( APL_DBL_SUFX))) == NULL)
      return ZE_MEM;

    /* Construct in-archive AppleDouble file name. */
    name_archv_p = name_archv;
    rslash = strrchr( name, '/');
    if (rslash == NULL)
    {
      /* "name" -> "._name" (or __MACOSX/._name"). */
      if (sequester)
      {
        strcpy( name_archv, APL_DBL_PFX_SQR);
        name_archv_p += strlen( APL_DBL_PFX_SQR);
      }
      strcpy( name_archv_p, APL_DBL_PFX);
      strcpy( (name_archv_p+ strlen( APL_DBL_PFX)), name);
    }
    else
    {
      /* "dir/name" -> "dir/._name" (or __MACOSX/dir/._name"). */
      if (sequester)
      {
        strcpy( name_archv, APL_DBL_PFX_SQR);
        name_archv_p += strlen( APL_DBL_PFX_SQR);
      }
      strncpy( name_archv_p, name, (rslash+ 1- name));
      strcpy( (name_archv_p+ (rslash+ 1- name)), APL_DBL_PFX);
      strcpy( (name_archv_p+ (rslash+ 1- name)+ strlen( APL_DBL_PFX)),
       &rslash[ 1]);
    }

    /* Construct in-file-system AppleDouble file name. */
    /* "name" -> "name/rsrc". */
    strcpy( name_flsys, name);
    strcat( name_flsys, APL_DBL_SUFX);
  }
  else
  {
    name_archv = name;
    name_flsys = name;
  }
#endif /* UNIX_APPLE */

  /* Scanning files ...
   *
   * After 5 seconds output Scanning files...
   * then a dot every 2 seconds
   */
  if (noisy) {
    /* If find files then output message after delay */
    if (scan_count == 0) {
      time_t current = time(NULL);
      scan_start = current;
    }
    scan_count++;
    if (scan_count % 100 == 0) {
      time_t current = time(NULL);

      if (current - scan_start > scan_delay) {
        if (scan_last == 0) {
          zipmessage_nl("Scanning files ", 0);
          scan_last = current;
        }
        if (current - scan_last > scan_dot_time) {
          scan_last = current;
          zfprintf(mesg, ".");
          fflush(mesg);
        }
      }
    }
  }

  /* Search for name in zip file.  If there, mark it, else add to
     list of new names to do (or remove from that list). */
  if ((iname = ex2in(name_archv, IS_ZFLAG_DIR(zflags), &dosflag)) == NULL)
    return ZE_MEM;

  /* Discard directory names with zip -rj */
  if (*iname == '\0') {
#ifndef AMIGA
/* A null string is a legitimate external directory name in AmigaDOS; also,
 * a command like "zip -r zipfile FOO:" produces an empty internal name.
 */
# ifndef RISCOS
 /* If extensions needs to be swapped, we will have empty directory names
    instead of the original directory. For example, zipping 'c.', 'c.main'
    should zip only 'main.c' while 'c.' will be converted to '\0' by ex2in. */

    if (pathput && !recurse) error("empty name without -j or -r");

# endif /* !RISCOS */
#endif /* !AMIGA */
    free((zvoid *)iname);

#ifdef UNIX_APPLE
    /* Free the special AppleDouble name storage. */
    if (IS_ZFLAG_APLDBL(zflags))
    {
      free( name_archv);
      free( name_flsys);
    }
#endif

    return ZE_OK;
  }
  undosm = NULL;
  if (dosflag || !pathput) {
    int save_dosify = dosify, save_pathput = pathput;
    dosify = 0;
    pathput = 1;
    /* zname is temporarly mis-used as "undosmode" iname pointer */
    if ((zname = ex2in(name_archv, IS_ZFLAG_DIR(zflags), NULL)) != NULL) {
      undosm = in2ex(zname);
      free(zname);
    }
    dosify = save_dosify;
    pathput = save_pathput;
  }
  if ((zname = in2ex(iname)) == NULL)
    return ZE_MEM;
#ifdef UNICODE_SUPPORT
  /* Convert name to display or OEM name */
  /* Was converting iname, changed to convert zname */
  oname = local_to_display_string(zname);
#else
  if ((oname = malloc(strlen(zname) + 1)) == NULL)
    return ZE_MEM;
  strcpy(oname, zname);
#endif
  if (undosm == NULL)
    undosm = zname;
  if ((z = zsearch(zname)) != NULL) {
    if (pcount && !filter(undosm, casesensitive)) {
      /* Do not clear z->mark if "exclude", because, when "dosify || !pathput"
       * is in effect, two files with different filter options may hit the
       * same z entry.
       */
      if (verbose)
        zfprintf(mesg, "excluding %s\n", oname);
      free((zvoid *)iname);
      free((zvoid *)zname);
    } else {
      z->mark = 1;
      if ((z->name = malloc(strlen(name_flsys) + 1 + PAD)) == NULL) {
        if (undosm != zname)
          free((zvoid *)undosm);
        free((zvoid *)iname);
        free((zvoid *)zname);
        return ZE_MEM;
      }
      strcpy(z->name, name_flsys);
      if (z->oname) free(z->oname);
      z->oname = oname;
      z->dosflag = dosflag;

#ifdef FORCE_NEWNAME
      free((zvoid *)(z->iname));
      z->iname = iname;
#else
      /* Better keep the old name. Useful when updating on MSDOS a zip file
       * made on Unix.
       */
      free((zvoid *)iname);
      free((zvoid *)zname);
#endif /* ? FORCE_NEWNAME */
    }
#ifdef UNICODE_SUPPORT_WIN32
    z->namew = NULL;
    z->inamew = NULL;
    z->znamew = NULL;
#endif
    if (name == label) {
      label = z->name;
    }
    if (IS_ZFLAG_FIFO(zflags))
      z->zflags |= ZFLAG_FIFO;

  } else if (pcount == 0 || filter(undosm, casesensitive)) {

    /* Check that we are not adding the zip file to itself. This
     * catches cases like "zip -m foo ../dir/foo.zip".
     */
#ifndef CMS_MVS
/* Version of stat() for CMS/MVS isn't complete enough to see if       */
/* files match.  Just let ZIP.C compare the filenames.  That's good    */
/* enough for CMS anyway since there aren't paths to worry about.      */
    z_stat statb;      /* now use structure z_stat and function zstat globally 7/24/04 EG */

    if (zipstate == -1)
        zipstate = strcmp(zipfile, "-") != 0 &&
                   zstat(zipfile, &zipstatb) == 0;

    if (zipstate == 1 && (statb = zipstatb, zstat(name, &statb) == 0
      && zipstatb.st_mode  == statb.st_mode
#ifdef VMS
      && memcmp(zipstatb.st_ino, statb.st_ino, sizeof(statb.st_ino)) == 0
      && strcmp(zipstatb.st_dev, statb.st_dev) == 0
      && zipstatb.st_uid   == statb.st_uid
#else /* !VMS */
      && zipstatb.st_ino   == statb.st_ino
      && zipstatb.st_dev   == statb.st_dev
      && zipstatb.st_uid   == statb.st_uid
      && zipstatb.st_gid   == statb.st_gid
#endif /* ?VMS */
      && zipstatb.st_size  == statb.st_size
      && zipstatb.st_mtime == statb.st_mtime
      && zipstatb.st_ctime == statb.st_ctime)) {
      /* Don't compare a_time since we are reading the file */
        if (verbose)
          zfprintf(mesg, "file matches zip file -- skipping\n");
        if (undosm != zname)
          free((zvoid *)zname);
        if (undosm != iname)
          free((zvoid *)undosm);
        free((zvoid *)iname);
        free(oname);

#ifdef UNIX_APPLE
        /* Free the special AppleDouble name storage. */
        if (IS_ZFLAG_APLDBL(zflags))
        {
          free( name_archv);
          free( name_flsys);
        }
#endif /* UNIX_APPLE */

        return ZE_OK;
    }
#endif  /* CMS_MVS */

    /* allocate space and add to list */
    pad_name = PAD;
#ifdef UNIX_APPLE
    if (IS_ZFLAG_APLDBL(zflags))
    {
       /* Add storage for AppleDouble name suffix. */
       pad_name += strlen( APL_DBL_SUFX);
    }
#endif /* UNIX_APPLE */

    if ((f = (struct flist far *)farmalloc(sizeof(struct flist))) == NULL ||
        fcount + 1 < fcount ||
        (f->name = malloc(strlen(name) + 1 + pad_name)) == NULL)
    {
      if (f != NULL)
        farfree((zvoid far *)f);
      if (undosm != zname)
        free((zvoid *)undosm);
      free((zvoid *)iname);
      free((zvoid *)zname);
      free(oname);
      return ZE_MEM;
    }
    strcpy(f->name, name_flsys);
    f->iname = iname;
    f->zname = zname;

#ifdef UNIX_APPLE
    /* Free the special AppleDouble name storage. */
    if (IS_ZFLAG_APLDBL(zflags))
    {
      free( name_archv);
      free( name_flsys);
    }
#endif /* UNIX_APPLE */

#ifdef UNICODE_SUPPORT
    /* Unicode */
    /* WIN32 should be using newnamew() */
    f->uname = local_to_utf8_string(iname);
#ifdef UNICODE_SUPPORT_WIN32
    f->namew = NULL;
    f->inamew = NULL;
    f->znamew = NULL;
    if (strcmp(f->name, "-") == 0) {
      f->namew = local_to_wchar_string(f->name);
    }
#endif /* def UNICODE_SUPPORT_WIN32 */
#endif /* def UNICODE_SUPPORT */

    f->oname = oname;
    f->dosflag = dosflag;

    f->zflags = zflags;

    *fnxt = f;
    f->lst = fnxt;
    f->nxt = NULL;
    fnxt = &f->nxt;
    fcount++;
    if (name == label) {
      label = f->name;
    }
  }
  if (undosm != zname)
    free((zvoid *)undosm);
  return ZE_OK;
}

ulg dostime(y, n, d, h, m, s)
int y;                  /* year */
int n;                  /* month */
int d;                  /* day */
int h;                  /* hour */
int m;                  /* minute */
int s;                  /* second */
/* Convert the date y/n/d and time h:m:s to a four byte DOS date and
   time (date in high two bytes, time in low two bytes allowing magnitude
   comparison). */
{
  return y < 1980 ? DOSTIME_MINIMUM /* dostime(1980, 1, 1, 0, 0, 0) */ :
        (((ulg)y - 1980) << 25) | ((ulg)n << 21) | ((ulg)d << 16) |
        ((ulg)h << 11) | ((ulg)m << 5) | ((ulg)s >> 1);
}


ulg unix2dostime(t)
time_t *t;              /* unix time to convert */
/* Return the Unix time t in DOS format, rounded up to the next two
   second boundary. */
{
  time_t t_even;
  struct tm *s;         /* result of localtime() */

  t_even = (time_t)(((unsigned long)(*t) + 1) & (~1));
                                /* Round up to even seconds. */
  s = localtime(&t_even);       /* Use local time since MSDOS does. */
  if (s == (struct tm *)NULL) {
      /* time conversion error; use current time as emergency value
         (assuming that localtime() does at least accept this value!) */
      t_even = (time_t)(((unsigned long)time(NULL) + 1) & (~1));
      s = localtime(&t_even);
  }
  return dostime(s->tm_year + 1900, s->tm_mon + 1, s->tm_mday,
                 s->tm_hour, s->tm_min, s->tm_sec);
}

int issymlnk(a)
ulg a;                  /* Attributes returned by filetime() */
/* Return true if the attributes are those of a symbolic link */
{
#ifndef QDOS
# ifdef SYMLINKS
#  ifdef __human68k__
  int *_dos_importlnenv(void);

  if (_dos_importlnenv() == NULL)
    return 0;
#  endif
#  ifdef WIN32
  /* This function should not be called for Windows.  The caller should call
     isWinSymlink() instead. */
  return 0;
#  else
  return ((a >> 16) & S_IFMT) == S_IFLNK;
#  endif
# else /* !SYMLINKS */
  return (int)a & 0;    /* avoid warning on unused parameter */
# endif /* ?SYMLINKS */
#else
  return 0;
#endif
}

#endif /* !UTIL */


#if (!defined(UTIL) && !defined(ZP_NEED_GEN_D2U_TIME))
   /* There is no need for dos2unixtime() in the ZipUtils' code. */
#  define ZP_NEED_GEN_D2U_TIME
#endif
#if ((defined(OS2) || defined(VMS)) && defined(ZP_NEED_GEN_D2U_TIME))
   /* OS/2 and VMS use a special solution to handle time-stams of files. */
#  undef ZP_NEED_GEN_D2U_TIME
#endif
#if (defined(W32_STATROOT_FIX) && !defined(ZP_NEED_GEN_D2U_TIME))
   /* The Win32 stat()-bandaid to fix stat'ing root directories needs
    * dos2unixtime() to calculate the time-stamps. */
#  define ZP_NEED_GEN_D2U_TIME
#endif

#ifdef ZP_NEED_GEN_D2U_TIME

time_t dos2unixtime(dostime)
ulg dostime;            /* DOS time to convert */
/* Return the Unix time_t value (GMT/UTC time) for the DOS format (local)
 * time dostime, where dostime is a four byte value (date in most significant
 * word, time in least significant word), see dostime() function.
 */
{
  struct tm *t;         /* argument for mktime() */
  ZCONST time_t clock = time(NULL);

  t = localtime(&clock);
  t->tm_isdst = -1;     /* let mktime() determine if DST is in effect */
  /* Convert DOS time to UNIX time_t format */
  t->tm_sec  = (((int)dostime) <<  1) & 0x3e;
  t->tm_min  = (((int)dostime) >>  5) & 0x3f;
  t->tm_hour = (((int)dostime) >> 11) & 0x1f;
  t->tm_mday = (int)(dostime >> 16) & 0x1f;
  t->tm_mon  = ((int)(dostime >> 21) & 0x0f) - 1;
  t->tm_year = ((int)(dostime >> 25) & 0x7f) + 80;

  return mktime(t);
}

#undef ZP_NEED_GEN_D2U_TIME
#endif /* ZP_NEED_GEN_D2U_TIME */


#ifndef MACOS
int destroy(f)
  char *f;             /* file to delete */
/* Delete the file *f, returning non-zero on failure. */
{
  return unlink(f);
}




int replace(d, s)
char *d, *s;            /* destination and source file names */
/* Replace file *d by file *s, removing the old *s.  Return an error code
   in the ZE_ class. This function need not preserve the file attributes,
   this will be done by setfileattr() later.

   Modified to support WIN32 Unicode.
 */
{
#ifdef UNICODE_SUPPORT_WIN32
  int s_utf8 = is_utf8_string(s, NULL, NULL, NULL, NULL);
  int d_utf8 = is_utf8_string(d, NULL, NULL, NULL, NULL);
  wchar_t *dw = NULL;
  wchar_t *sw = NULL;
  zw_stat t;
#else
  z_stat t;         /* results of stat() */
#endif

#if defined(CMS_MVS)
  /* cmsmvs.h defines FOPW_TEMP as memory(hiperspace).  Since memory is
   * lost at end of run, always do copy instead of rename.
   */
  int copy = 1;
#else
  int copy = 0;
#endif
  int d_exists;

#ifdef UNICODE_SUPPORT_WIN32
  if (d_utf8)
    dw = utf8_to_wchar_string(d);
  else
    dw = local_to_wchar_string(d);
  if (s_utf8)
    sw = utf8_to_wchar_string(s);
  else
    sw = local_to_wchar_string(s);
#endif

#if defined(VMS) || defined(CMS_MVS)
  /* stat() is broken on VMS remote files (accessed through Decnet).
   * This patch allows creation of remote zip files, but is not sufficient
   * to update them or compress remote files */
  unlink(d);
#else /* !(VMS || CMS_MVS) */
# ifdef UNICODE_SUPPORT_WIN32
  d_exists = (LSSTATW(dw, &t) == 0);
# else
  d_exists = (LSTAT(d, &t) == 0);
# endif
  if (d_exists)
  {
    /*
     * respect existing soft and hard links!
     */
    if (t.st_nlink > 1
# ifdef SYMLINKS
      /* This is a temporary kluge.  The utilities should be able to handle
         symlinks and the like, but currently big gobs of code are excluded
         from the utilities and it will take some effort to cleanly enable
         such code for the utilities.  So if ZipCloak on Windows tries to
         work with a symlink, not sure what happens.  (Need to check that.)
         Solution is to enable recognition of reparse points in utilities.
       */
#  if defined(NTSD_EAS) && !defined(UTIL)
#   ifdef UNICODE_SUPPORT_WIN32
        || isWinSymlinkw(dw)
#   else
        || isWinSymlink(d)
#   endif
#  else
        || (t.st_mode & S_IFMT) == S_IFLNK
#  endif
# endif
        )
       copy = 1;
    else
# ifdef UNICODE_SUPPORT_WIN32
      if (_wunlink(dw)) {
        free(dw);
        free(sw);
        return ZE_CREAT;                 /* Can't erase zip file--give up */
      }
# else
      if (unlink(d))
       return ZE_CREAT;                 /* Can't erase zip file--give up */
# endif
  }
#endif /* ?(VMS || CMS_MVS) */
#ifndef CMS_MVS
  if (!copy) {
# ifdef UNICODE_SUPPORT_WIN32
    if (_wrename(sw, dw))             /* Just move s on top of d */
# else
    if (rename(s, d))                 /* Just move s on top of d */
# endif
    { /* failed */
          copy = 1;                     /* failed ? */
# if !defined(VMS) && !defined(ATARI) && !defined(AZTEC_C)
#  if !defined(CMS_MVS) && !defined(RISCOS) && !defined(QDOS)
    /* For VMS, ATARI, AMIGA Aztec, VM_CMS, MVS, RISCOS,
       always assume that failure is EXDEV */
          if (errno != EXDEV
#   ifdef THEOS
           && errno != EEXIST
#   else
#    ifdef ENOTSAM
           && errno != ENOTSAM /* Used at least on Turbo C */
#    endif
#   endif
              ) return ZE_CREAT;
#  endif /* !CMS_MVS && !RISCOS */
# endif /* !VMS && !ATARI && !AZTEC_C */
      }
  }
#endif /* !CMS_MVS */

  if (copy) {
    FILE *f, *g;        /* source and destination files */
    int r;              /* temporary variable */

#ifdef RISCOS
    if (SWI_OS_FSControl_26(s,d,0xA1)!=NULL) {
#endif

    /* Use zfopen for almost all opens where fopen is used.  For
       most OS that support large files we use the 64-bit file
       environment and zfopen maps to fopen, but this allows
       tweeking ports that don't do that.  7/24/04 */
#ifdef UNICODE_SUPPORT_WIN32
    if ((f = zfopen(s, FOPR)) == NULL) {
      sprintf(errbuf," replace: can't open %s\n", s);
      if (s_utf8) {
        print_utf8(errbuf);
      }
      else
        zfprintf(mesg, "%s", errbuf);
      return ZE_TEMP;
    }
    if ((g = zfopen(d, FOPW)) == NULL)
    {
      fclose(f);
      return ZE_CREAT;
    }
#else
    if ((f = zfopen(s, FOPR)) == NULL) {
      zfprintf(mesg," replace: can't open %s\n", s);
      return ZE_TEMP;
    }
    if ((g = zfopen(d, FOPW)) == NULL)
    {
      fclose(f);
      return ZE_CREAT;
    }
#endif

    r = fcopy(f, g, (ulg)-1L);
    fclose(f);
    if (fclose(g) || r != ZE_OK)
    {
#ifdef UNICODE_SUPPORT_WIN32
      _wunlink(dw);
      free(dw);
      free(sw);
#else
      unlink(d);
#endif
      return r ? (r == ZE_TEMP ? ZE_WRITE : r) : ZE_WRITE;
    }
#ifdef UNICODE_SUPPORT_WIN32
    _wunlink(sw);
#else
    unlink(s);
#endif
#ifdef RISCOS
    }
#endif
  }
#ifdef UNICODE_SUPPORT_WIN32
  if (dw)
    free(dw);
  if (sw)
    free(sw);
#endif
  return ZE_OK;
}
#endif /* !MACOS */


int getfileattr(f)
char *f;                /* file path */
/* Return the file attributes for file f or 0 if failure */
{
#ifdef __human68k__
  struct _filbuf buf;

  return _dos_files(&buf, f, 0xff) < 0 ? 0x20 : buf.atr;
#else
  z_stat s;

  return SSTAT(f, &s) == 0 ? (int) s.st_mode : 0;
#endif
}


int setfileattr(f, a)
char *f;                /* file path */
int a;                  /* attributes returned by getfileattr() */
/* Give the file f the attributes a, return non-zero on failure */
{
#ifdef UNICODE_SUPPORT_WIN32
  int utf8 = is_utf8_string(f, NULL, NULL, NULL, NULL);
  wchar_t *fw;
  int result;

  if (utf8)
    fw = utf8_to_wchar_string(f);
  else
    fw = local_to_wchar_string(f);
  result = _wchmod(fw, a);
  free(fw);
  return result;
#else /* not UNICODE_SUPPORT_WIN32 */
# if defined(TOPS20) || defined (CMS_MVS)
  return 0;
# else
#  ifdef __human68k__
  return _dos_chmod(f, a) < 0 ? -1 : 0;
#  else
  return chmod(f, a);
#  endif
# endif
#endif /* UNICODE_SUPPORT_WIN32 */
}




/* tempname */

#ifndef VMS /* VMS-specific function is in VMS.C. */

char *tempname(zip)
  char *zip;              /* path name of zip file to generate temp name for */

/* Return a temporary file name in its own malloc'ed space, using tempath. */
{
  char *t = zip;   /* malloc'ed space for name (use zip to avoid warning) */

# ifdef CMS_MVS
  if ((t = malloc(strlen(tempath) + L_tmpnam + 2)) == NULL)
    return NULL;

#  ifdef VM_CMS
  tmpnam(t);
  /* Remove filemode and replace with tempath, if any. */
  /* Otherwise A-disk is used by default */
  *(strrchr(t, ' ')+1) = '\0';
  if (tempath!=NULL)
     strcat(t, tempath);
  return t;
#  else   /* !VM_CMS */
  /* For MVS */
  tmpnam(t);
  if (tempath != NULL)
  {
    int l1 = strlen(t);
    char *dot;
    if (*t == '\'' && *(t+l1-1) == '\'' && (dot = strchr(t, '.')))
    {
      /* MVS and not OE.  tmpnam() returns quoted string of 5 qualifiers.
       * First is HLQ, rest are timestamps.  User can only replace HLQ.
       */
      int l2 = strlen(tempath);
      if (strchr(tempath, '.') || l2 < 1 || l2 > 8)
        ziperr(ZE_PARMS, "On MVS and not OE, tempath (-b) can only be HLQ");
      memmove(t+1+l2, dot, l1+1-(dot-t));  /* shift dot ready for new hlq */
      memcpy(t+1, tempath, l2);            /* insert new hlq */
    }
    else
    {
      /* MVS and probably OE.  tmpnam() returns filename based on TMPDIR,
       * no point in even attempting to change it.  User should modify TMPDIR
       * instead.
       */
      zipwarn("MVS, assumed to be OE, change TMPDIR instead of option -b: ",
              tempath);
    }
  }
  return t;
#  endif  /* !VM_CMS */

# else /* !CMS_MVS */

#  ifdef TANDEM
  char cur_subvol [FILENAME_MAX];
  char temp_subvol [FILENAME_MAX];
  char *zptr;
  char *ptr;
  char *cptr = &cur_subvol[0];
  char *tptr = &temp_subvol[0];
  short err;
  FILE *tempf;
  int attempts;

  t = (char *)malloc(NAMELEN); /* malloc here as you cannot free */
                               /* tmpnam allocated storage later */

  zptr = strrchr(zip, TANDEM_DELIMITER);

  if (zptr != NULL) {
    /* ZIP file specifies a Subvol so make temp file there so it can just
       be renamed at end */

    *tptr = *cptr = '\0';
    strcat(cptr, getenv("DEFAULTS"));

    strncat(tptr, zip, _min(FILENAME_MAX, (zptr - zip)) ); /* temp subvol */
    strncat(t, zip, _min(NAMELEN, ((zptr - zip) + 1)) );   /* temp stem   */

    err = chvol(tptr);
    ptr = t + strlen(t);  /* point to end of stem */
  }
  else
    ptr = t;

  /* If two zips are running in same subvol then we can get contention problems
     with the temporary filename.  As a work around we attempt to create
     the file here, and if it already exists we get a new temporary name */

  attempts = 0;
  do {
    attempts++;
    tmpnam(ptr);  /* Add filename */
    tempf = zfopen(ptr, FOPW_TMP);    /* Attempt to create file */
  } while (tempf == NULL && attempts < 100);

  if (attempts >= 100) {
    ziperr(ZE_TEMP, "Could not get unique temp file name");
  }

  fclose(tempf);

  if (zptr != NULL) {
    err = chvol(cptr);  /* Put ourself back to where we came in */
  }

  return t;

#  else /* !CMS_MVS && !TANDEM */
/*
 * Do something with TMPDIR, TMP, TEMP ????
 */
  if (tempath != NULL)
  {
    if ((t = malloc(strlen(tempath) + 12)) == NULL)
      return NULL;
    strcpy(t, tempath);

#   if (!defined(VMS) && !defined(TOPS20))
#    ifdef MSDOS
    {
      char c = (char)lastchar(t);
      if (c != '/' && c != ':' && c != '\\')
        strcat(t, "/");
    }
#    else

#     ifdef AMIGA
    {
      char c = (char)lastchar(t);
      if (c != '/' && c != ':')
        strcat(t, "/");
    }
#     else /* !AMIGA */
#      ifdef RISCOS
    if (lastchar(t) != '.')
      strcat(t, ".");
#      else /* !RISCOS */

#       ifdef QDOS
    if (lastchar(t) != '_')
      strcat(t, "_");
#       else
    if (lastchar(t) != '/')
      strcat(t, "/");
#       endif /* ?QDOS */
#      endif /* ?RISCOS */
#     endif  /* ?AMIGA */
#    endif /* ?MSDOS */
#   endif /* !VMS && !TOPS20 */
  }
  else
  {
    if ((t = malloc(12)) == NULL)
      return NULL;
    *t = 0;
  }
#   ifdef NO_MKTEMP
  {
    char *p = t + strlen(t);
    sprintf(p, "%08lx", (ulg)time(NULL));
    return t;
  }
#   else
  strcat(t, "ziXXXXXX"); /* must use lowercase for Linux dos file system */
#     if defined(UNIX) && !defined(NO_MKSTEMP)
  /* tempname should not be called */
  return t;
#     else
  return mktemp(t);
#     endif
#   endif /* NO_MKTEMP */
#  endif /* TANDEM */
# endif /* CMS_MVS */
}
#endif /* !VMS */

int fcopy(f, g, n)
  FILE *f, *g;            /* source and destination files */
  /* now use uzoff_t for all file sizes 5/14/05 CS */
  uzoff_t n;               /* number of bytes to copy or -1 for all */
/* Copy n bytes from file *f to file *g, or until EOF if (zoff_t)n == -1.
   Return an error code in the ZE_ class. */
{
  char *b;              /* malloc'ed buffer for copying */
  extent k;             /* result of fread() */
  uzoff_t m;            /* bytes copied so far */

  if ((b = malloc(CBSZ)) == NULL)
    return ZE_MEM;
  m = 0;
  while (n == (uzoff_t)(-1L) || m < n)
  {
    if ((k = fread(b, 1, n == (uzoff_t)(-1) ?
                   CBSZ : (n - m < CBSZ ? (extent)(n - m) : CBSZ), f)) == 0)
    {
      if (ferror(f))
      {
        free((zvoid *)b);
        return ZE_READ;
      }
      else
        break;
    }
    if (fwrite(b, 1, k, g) != k)
    {
      free((zvoid *)b);
      zfprintf(mesg," fcopy: write error\n");
      return ZE_TEMP;
    }
    m += k;
  }
  free((zvoid *)b);
  return ZE_OK;
}


/* from zipfile.c */

#ifdef THEOS
 /* Macros cause stack overflow in compiler */
 ush SH(uch* p) { return ((ush)(uch)((p)[0]) | ((ush)(uch)((p)[1]) << 8)); }
 ulg LG(uch* p) { return ((ulg)(SH(p)) | ((ulg)(SH((p)+2)) << 16)); }
#else /* !THEOS */
 /* Macros for converting integers in little-endian to machine format */
# define SH(a) ((ush)(((ush)(uch)(a)[0]) | (((ush)(uch)(a)[1]) << 8)))
# define LG(a) ((ulg)SH(a) | ((ulg)SH((a)+2) << 16))
# ifdef ZIP64_SUPPORT           /* zip64 support 08/31/2003 R.Nausedat */
#  define LLG(a) ((zoff_t)LG(a) | ((zoff_t)LG((a)+4) << 32))
# endif
#endif /* ?THEOS */

#ifdef UNICODE_SUPPORT_WIN32
/* fopen_utf8 - open file with possible utf8 path
 *
 * Only fully implemented for WIN32 currently.
 *
 * Now detects if filename is UTF-8.
 *
 * Return fopen file handle or NULL.
 */
FILE *fopen_utf8(char *filename, char *mode)
{
  int is_utf8;
  wchar_t *wfilename;
  wchar_t *wmode;
  FILE *f;

  is_utf8 = is_utf8_string(filename, NULL, NULL, NULL, NULL);

  if (is_utf8) {
    wfilename = utf8_to_wchar_string(filename);
    if (wfilename == NULL)
      return NULL;
    wmode = utf8_to_wchar_string(mode);
    if (wmode == NULL)
      return NULL;

    f = _wfopen(wfilename, wmode);
    free(wfilename);
    free(wmode);
  
    return f;
  } else {
    return fopen(filename, mode);
  }
}
#endif /* def UNICODE_SUPPORT_WIN32 */


/* always copies from global in_file to global output file y */
int bfcopy(n)
  /* now use uzoff_t for all file sizes 5/14/05 CS */
  uzoff_t n;               /* number of bytes to copy or -1 for all */
/* Copy n bytes from in_file to out_file, or until EOF if (zoff_t)n == -1.

   Normally we have the compressed size from either the central directory
   entry or the local header.

   If n != -1 and EOF, close current split and open next and continue
   copying.

   If n == -2, copy until find the extended header (data descriptor).  Only
   used for -FF when no size available.

   If fix == 1 calculate CRC of input entry and verify matches.

   If fix == 2 and this entry using data descriptor keep a sliding
   window in the buffer for looking for signature.

   Return an error code in the ZE_ class. */
{
  char *b;              /* malloc'ed buffer for copying */
  extent k;             /* result of fread() */
  uzoff_t m;            /* bytes copied so far */
  extent brd;           /* bytes to read */
  zoff_t data_start = 0;
  zoff_t des_start = 0;
  char *split_path;
  extent kk;
  int i;
  char sbuf[4];         /* buffer for sliding signature window for fix = 2 */
  int des = 0;          /* this entry has data descriptor to find */

  if ((b = malloc(CBSZ)) == NULL)
    return ZE_MEM;

  if (copy_only && !display_globaldots) {
    /* initialize dot count */
    dot_count = -1;
  }

  if (fix == 2 && n == (uzoff_t) -2) {
    data_start = zftello(in_file);
    for (kk = 0; kk < 4; kk++)
      sbuf[kk] = 0;
    des = 1;
  }

  des_good = 0;

  m = 0;
  while (des || n == (uzoff_t)(-1L) || m < n)
  {
    if (des || n == (uzoff_t)(-1))
      brd = CBSZ;
    else
      brd = (n - m < CBSZ ? (extent)(n - m) : CBSZ);

    des_start = zftello(in_file);

    k = fread(b, 1, brd, in_file);
    if ((k == 0) && !feof( in_file))
    {
      /* fread() returned no data, but not normal end-of-file. */
      if (fix == 2 && k < brd) {
        free((zvoid *)b);
        return ZE_READ;
      }
      else if (ferror(in_file))
      {
        free((zvoid *)b);
        return ZE_READ;
      }
      else {
        break;
      }
    }


    /* end at extended local header (data descriptor) signature */
    if (des) {
      des_crc = 0;
      des_csize = 0;
      des_usize = 0;

      /* If first 4 bytes in buffer are data descriptor signature then
         try to read the data descriptor.
         If not, scan for signature and break if found, let bfwrite flush
         the data and then next read should put the data descriptor at
         the beginning of the buffer.
       */

      if (
          (b[0] != 0x50 /*'P' except EBCDIC*/ ||
           b[1] != 0x4b /*'K' except EBCDIC*/ ||
           b[2] != '\07' ||
           b[3] != '\010')) {
        /* buffer is not start of data descriptor */

        for (kk = 0; kk < k; kk++) {
          /* add byte to end of sbuf */
          for (i = 0; i < 3; i++)
            sbuf[i] = sbuf[i + 1];
          sbuf[3] = b[kk];

          /* see if this is signature */
          if (
              (sbuf[0] == 0x50 /*'P' except EBCDIC*/ &&
               sbuf[1] == 0x4b /*'K' except EBCDIC*/ &&
               sbuf[2] == '\07' &&
               sbuf[3] == '\010')) {
            kk -= 3;
            if (zfseeko(in_file, bytes_this_split + kk, SEEK_SET) != 0) {
              /* seek error */
              ZIPERR(ZE_READ, "seek failed reading descriptor");
            }
            des_start = zftello(in_file);
            k = kk;
            break;
          }
        }
      }
      else

      /* signature at start of buffer */
      {
        des_good = 0;

#ifdef ZIP64_SUPPORT
        if (zip64_entry) {

          /* read Zip64 data descriptor */
          if (k < 24) {
            /* not enough bytes, so can't be data descriptor
               as data descriptors can't be split across splits
             */
          }
          else
          {
            /* read the Zip64 descriptor */

            des_crc = LG(b + 4);
            des_csize = LLG(b + 8);
            des_usize = LLG(b + 16);

            /* if this is the right data descriptor then the sizes should match */
            if ((uzoff_t)des_start - (uzoff_t)data_start != des_csize) {
              /* apparently this signature does not go with this data so skip */

              /* write out signature as data */
              k = 4;
              if (zfseeko(in_file, des_start + k, SEEK_SET) != 0) {
                /* seek error */
                ZIPERR(ZE_READ, "seek failed reading descriptor");
              }
              if (bfwrite(b, 1, k, BFWRITE_DATA) != k)
              {
                free((zvoid *)b);
                zfprintf(mesg," fcopy: write error\n");
                return ZE_TEMP;
              }
              m += k;
              continue;
            }
            else
            {
              /* apparently this is the correct data descriptor */

              /* we should check the CRC but would need to inflate
                 the data */

              /* skip descriptor as will write out later */
              des_good = 1;
              k = 24;
              data_start = zftello(in_file);
              if (zfseeko(in_file, des_start + k, SEEK_SET) != 0) {
                /* seek error */
                ZIPERR(ZE_READ, "seek failed reading descriptor");
              }
              data_start = zftello(in_file);
            }
          }

        }
        else
#endif
        {
          /* read standard data descriptor */

          if (k < 16) {
            /* not enough bytes, so can't be data descriptor
               as data descriptors can't be split across splits
             */
          }
          else
          {
            /* read the descriptor */

            des_crc = LG(b + 4);
            des_csize = LG(b + 8);
            des_usize = LG(b + 12);

            /* if this is the right data descriptor then the sizes should match */
            if ((uzoff_t)des_start - (uzoff_t)data_start != des_csize) {
              /* apparently this signature does not go with this data so skip */

              /* write out signature as data */
              k = 4;
              if (zfseeko(in_file, des_start + k, SEEK_SET) != 0) {
                /* seek error */
                ZIPERR(ZE_READ, "seek failed reading descriptor");
              }
              if (bfwrite(b, 1, k, BFWRITE_DATA) != k)
              {
                free((zvoid *)b);
                zfprintf(mesg," fcopy: write error\n");
                return ZE_TEMP;
              }
              m += k;
              continue;
            }
            else
            {
              /* apparently this is the correct data descriptor */

              /* we should check the CRC but this does not work for
                 encrypted data */

              /* skip descriptor as will write out later */
              des_good = 1;
              data_start = zftello(in_file);
              k = 16;
              if (zfseeko(in_file, des_start + k, SEEK_SET) != 0) {
                /* seek error */
                ZIPERR(ZE_READ, "seek failed reading descriptor");
              }
              data_start = zftello(in_file);
            }
          }


        }
      }
    } /* des */


    if (des_good) {
      /* skip descriptor as will write out later */
    } else {
      /* write out apparently wrong descriptor as data */
      if (bfwrite(b, 1, k, BFWRITE_DATA) != k)
      {
        free((zvoid *)b);
        zfprintf(mesg," fcopy: write error\n");
        return ZE_TEMP;
      }
      m += k;
    }

    if (copy_only && !display_globaldots) {
      if (dot_size > 0) {
        /* initial space */
        if (noisy && dot_count == -1) {
          display_dot_char( ' ');
          dot_count++;
        }
        dot_count += k;
        if (dot_size <= dot_count) dot_count = 0;
      }
      if ((verbose || noisy) && dot_size && !dot_count) {
        display_dot_char( '.');
        mesg_line_started = 1;
      }
    }

    if (des_good)
      break;

    if (des)
      continue;

    if ((des || n != (uzoff_t)(-1L)) && m < n && feof(in_file)) {
      /* open next split */
      current_in_disk++;

      if (current_in_disk >= total_disks) {
        /* done */
        break;

      } else if (current_in_disk == total_disks - 1) {
        /* last disk is archive.zip */
        if ((split_path = malloc(strlen(in_path) + 1)) == NULL) {
          zipwarn("reading archive: ", in_path);
          return ZE_MEM;
        }
        strcpy(split_path, in_path);
      } else {
        /* other disks are archive.z01, archive.z02, ... */
        split_path = get_in_split_path(in_path, current_in_disk);
      }

      fclose(in_file);

      /* open the split */
      while ((in_file = zfopen(split_path, FOPR)) == NULL) {
        int r = 0;

        /* could not open split */

        if (fix == 1 && skip_this_disk) {
          free(split_path);
          free((zvoid *)b);
          return ZE_FORM;
        }

        /* Ask for directory with split.  Updates in_path */
        r = ask_for_split_read_path(current_in_disk);
        if (r == ZE_ABORT) {
          zipwarn("could not find split: ", split_path);
          free(split_path);
          free((zvoid *)b);
          return ZE_ABORT;
        }
        if (r == ZE_EOF) {
          zipmessage_nl("", 1);
          zipwarn("user ended reading - closing archive", "");
          free(split_path);
          free((zvoid *)b);
          return ZE_EOF;
        }
        if (fix == 2 && skip_this_disk) {
          /* user asked to skip this disk */
          zipwarn("skipping split file: ", split_path);
          current_in_disk++;
        }

        if (current_in_disk == total_disks - 1) {
          /* last disk is archive.zip */
          if ((split_path = malloc(strlen(in_path) + 1)) == NULL) {
            zipwarn("reading archive: ", in_path);
            return ZE_MEM;
          }
          strcpy(split_path, in_path);
        } else {
          /* other disks are archive.z01, archive.z02, ... */
          split_path = get_in_split_path(zipfile, current_in_disk);
        }
      }
      if (fix == 2 && skip_this_disk) {
        /* user asked to skip this disk */
        free(split_path);
        free((zvoid *)b);
        return ZE_FORM;
      }
      free(split_path);
    }
  }
  free((zvoid *)b);
  return ZE_OK;
}



#ifdef NO_RENAME
int rename(from, to)
ZCONST char *from;
ZCONST char *to;
{
    unlink(to);
    if (link(from, to) == -1)
        return -1;
    if (unlink(from) == -1)
        return -1;
    return 0;
}

#endif /* NO_RENAME */


#ifdef ZMEM

/************************/
/*  Function memset()   */
/************************/

/*
 * memset - for systems without it
 *  bill davidsen - March 1990
 */

char *
memset(buf, init, len)
register char *buf;     /* buffer loc */
register int init;      /* initializer */
register unsigned int len;   /* length of the buffer */
{
    char *start;

    start = buf;
    while (len--) *(buf++) = init;
    return(start);
}


/************************/
/*  Function memcpy()   */
/************************/

char *
memcpy(dst,src,len)             /* v2.0f */
register char *dst, *src;
register unsigned int len;
{
    char *start;

    start = dst;
    while (len--)
        *dst++ = *src++;
    return(start);
}


/************************/
/*  Function memcmp()   */
/************************/

int
memcmp(b1,b2,len)                     /* jpd@usl.edu -- 11/16/90 */
register char *b1, *b2;
register unsigned int len;
{

    if (len) do {       /* examine each byte (if any) */
      if (*b1++ != *b2++)
        return (*((uch *)b1-1) - *((uch *)b2-1));  /* exit when miscompare */
    } while (--len);

    return(0);          /* no miscompares, yield 0 result */
}

#endif  /* ZMEM */


/*------------------------------------------------------------------
 * Split archives
 */


/* ask_for_split_read_path
 *
 * If the next split file is not in the current directory, ask
 * the user where it is.
 *
 * in_path is the base path for reading splits and is usually
 * the same as zipfile.  The path in in_path must be the archive
 * file ending in .zip as this is assumed by get_in_split_path().
 *
 * Updates in_path if changed.  Returns ZE_OK if OK or ZE_ABORT if
 * user cancels reading archive.
 *
 * If fix = 1 then allow skipping disk (user may not have it).
 */

#define SPLIT_MAXPATH (FNMAX + 4010)

int ask_for_split_read_path(current_disk)
  ulg current_disk;
{
  FILE *f;
  int is_readable = 0;
  int i;
  char *split_dir = NULL;
  char *archive_name = NULL;
  char *split_name = NULL;
  char *split_path = NULL;
  char buf[SPLIT_MAXPATH + 100];

  /* get split path */
  split_path = get_in_split_path(in_path, current_disk);

  /* get the directory */
  if ((split_dir = malloc(strlen(in_path) + 40)) == NULL) {
    ZIPERR(ZE_MEM, "split path");
  }
  strcpy(split_dir, in_path);

  /* remove any name at end */
  for (i = (int)strlen(split_dir) - 1; i >= 0; i--) {
    if (split_dir[i] == '/' || split_dir[i] == '\\'
          || split_dir[i] == ':') {
      split_dir[i + 1] = '\0';
      break;
    }
  }
  if (i < 0)
    split_dir[0] = '\0';

  /* get the name of the archive */
  if ((archive_name = malloc(strlen(in_path) + 1)) == NULL) {
    ZIPERR(ZE_MEM, "split path");
  }
  if (strlen(in_path) == strlen(split_dir)) {
    archive_name[0] = '\0';
  } else {
    strcpy(archive_name, in_path + strlen(split_dir));
  }

  /* get the name of the split */
  if ((split_name = malloc(strlen(split_path) + 1)) == NULL) {
    ZIPERR(ZE_MEM, "split path");
  }
  if (strlen(in_path) == strlen(split_dir)) {
    split_name[0] = '\0';
  } else {
    strcpy(split_name, split_path + strlen(split_dir));
  }
  if (i < 0) {
    strcpy(split_dir, "(current directory)");
  }

  zfprintf(mesg, "\n\nCould not find:\n");
  zfprintf(mesg, "  %s\n", split_path);
  /*
  fprintf(mesg, "Please enter the path directory (. for cur dir) where\n");
  fprintf(mesg, "  %s\n", split_name);
  fprintf(mesg, "is located\n");
  */
  for (;;) {
    if (is_readable) {
      zfprintf(mesg, "\nHit c      (change path to where this split file is)");
      zfprintf(mesg, "\n    q      (abort archive - quit)");
      zfprintf(mesg, "\n or ENTER  (continue with this split): ");
    } else {
      if (fix == 1) {
        zfprintf(mesg, "\nHit c      (change path to where this split file is)");
        zfprintf(mesg, "\n    s      (skip this split)");
        zfprintf(mesg, "\n    q      (abort archive - quit)");
        zfprintf(mesg, "\n or ENTER  (try reading this split again): ");
      } else if (fix == 2) {
        zfprintf(mesg, "\nHit c      (change path to where this split file is)");
        zfprintf(mesg, "\n    s      (skip this split)");
        zfprintf(mesg, "\n    q      (abort archive - quit)");
        zfprintf(mesg, "\n    e      (end this archive - no more splits)");
        zfprintf(mesg, "\n    z      (look for .zip split - the last split)");
        zfprintf(mesg, "\n or ENTER  (try reading this split again): ");
      } else {
        zfprintf(mesg, "\nHit c      (change path to where this split file is)");
        zfprintf(mesg, "\n    q      (abort archive - quit)");
        zfprintf(mesg, "\n or ENTER  (try reading this split again): ");
      }
    }
    fflush(mesg);
    fgets(buf, SPLIT_MAXPATH, stdin);
    /* remove any newline */
    for (i = 0; buf[i]; i++) {
      if (buf[i] == '\n') {
        buf[i] = '\0';
        break;
      }
    }
    if (toupper(buf[0]) == 'Q') {
      return ZE_ABORT;
    } else if ((fix == 1 || fix == 2) && toupper(buf[0]) == 'S') {
    /*
      fprintf(mesg, "\nSkip this split/disk?  (files in this split will not be recovered) [n/y] ");
      fflush(mesg);
      fgets(buf, SPLIT_MAXPATH, stdin);
      if (buf[0] == 'y' || buf[0] == 'Y') {
    */
      skip_this_disk = current_in_disk + 1;
      return ZE_FORM;
    } else if (toupper(buf[0]) == 'C') {
      zfprintf(mesg, "\nEnter path where this split is (ENTER = same dir, . = current dir)");
      zfprintf(mesg, "\n: ");
      fflush(mesg);
      fgets(buf, SPLIT_MAXPATH, stdin);
      is_readable = 0;
      /* remove any newline */
      for (i = 0; buf[i]; i++) {
        if (buf[i] == '\n') {
          buf[i] = '\0';
          break;
        }
      }
      if (buf[0] == '\0') {
        /* Hit ENTER so try old path again - could be removable media was changed */
        strcpy(buf, split_path);
      }
    } else if (fix == 2 && toupper(buf[0]) == 'E') {
      /* no more splits to read */
      return ZE_EOF;
    } else if (fix == 2 && toupper(buf[0]) == 'Z') {
      total_disks = current_disk + 1;
      free(split_path);
      split_path = get_in_split_path(in_path, current_disk);
      buf[0] = '\0';
      strncat(buf, split_path, SPLIT_MAXPATH);
    }
    if (strlen(buf) > 0) {
      /* changing path */

      /* check if user wants current directory */
      if (buf[0] == '.' && buf[1] == '\0') {
        buf[0] = '\0';
      }
      /* remove any name at end */
      for (i = (int)strlen(buf); i >= 0; i--) {
        if (buf[i] == '/' || buf[i] == '\\'
             || buf[i] == ':') {
          buf[i + 1] = '\0';
          break;
        }
      }
      /* update base_path to newdir/split_name - in_path is the .zip file path */
      free(in_path);
      if (i < 0) {
        /* just name so current directory */
        strcpy(buf, "(current directory)");
        if (archive_name == NULL) {
          i = 0;
        } else {
          i = (int)strlen(archive_name);
        }
        if ((in_path = malloc(strlen(archive_name) + 40)) == NULL) {
          ZIPERR(ZE_MEM, "split path");
        }
        strcpy(in_path, archive_name);
      } else {
        /* not the current directory */
        /* remove any name at end */
        for (i = (int)strlen(buf); i >= 0; i--) {
          if (buf[i] == '/') {
            buf[i + 1] = '\0';
            break;
          }
        }
        if (i < 0) {
          buf[0] = '\0';
        }
        if ((in_path = malloc(strlen(buf) + strlen(archive_name) + 40)) == NULL) {
          ZIPERR(ZE_MEM, "split path");
        }
        strcpy(in_path, buf);
        strcat(in_path, archive_name);
      }

      free(split_path);

      /* get split path */
      split_path = get_in_split_path(in_path, current_disk);

      free(split_dir);
      if ((split_dir = malloc(strlen(in_path) + 40)) == NULL) {
        ZIPERR(ZE_MEM, "split path");
      }
      strcpy(split_dir, in_path);
      /* remove any name at end */
      for (i = (int)strlen(split_dir); i >= 0; i--) {
        if (split_dir[i] == '/') {
          split_dir[i + 1] = '\0';
          break;
        }
      }

      /* try to open it */
      if ((f = fopen(split_path, "r")) == NULL) {
        zfprintf(mesg, "\nCould not find or open\n");
        zfprintf(mesg, "  %s\n", split_path);
        /*
        zfprintf(mesg, "Please enter the path (. for cur dir) where\n");
        zfprintf(mesg, "  %s\n", split_name);
        zfprintf(mesg, "is located\n");

        */
        continue;
      }
      fclose(f);
      is_readable = 1;
      zfprintf(mesg, "Found:  %s\n", split_path);
    } else {
      /* try to open it */
      if ((f = fopen(split_path, "r")) == NULL) {
        zfprintf(mesg, "\nCould not find or open\n");
        zfprintf(mesg, "  %s\n", split_path);
        /*
        zfprintf(mesg, "Please enter the path (. for cur dir) where\n");
        zfprintf(mesg, "  %s\n", split_name);
        zfprintf(mesg, "is located\n");
        */
        continue;
      }
      fclose(f);
      is_readable = 1;
      zfprintf(mesg, "\nFound:  %s\n", split_path);
      break;
    }
  }
  free(archive_name);
  free(split_dir);
  free(split_name);

  return ZE_OK;
}


/* ask_for_split_write_path
 *
 * Verify the directory for the next split.  Called
 * when -sp is used to pause between writing splits.
 *
 * Updates out_path and return 1 if OK or 0 if cancel
 */
int ask_for_split_write_path(current_disk)
  ulg current_disk;
{
  unsigned int num = (unsigned int)current_disk + 1;
  int i;
  char *split_dir = NULL;
  char *split_name = NULL;
  char buf[FNMAX + 40];

  /* get the directory */
  if ((split_dir = malloc(strlen(out_path) + 40)) == NULL) {
    ZIPERR(ZE_MEM, "split path");
  }
  strcpy(split_dir, out_path);

  /* remove any name at end */
  for (i = (int)strlen(split_dir); i >= 0; i--) {
    if (split_dir[i] == '/' || split_dir[i] == '\\'
          || split_dir[i] == ':') {
      split_dir[i + 1] = '\0';
      break;
    }
  }

  /* get the name of the split */
  if ((split_name = malloc(strlen(out_path) + 1)) == NULL) {
    ZIPERR(ZE_MEM, "split path");
  }
  if (strlen(out_path) == strlen(split_dir)) {
    split_name[0] = '\0';
  } else {
    strcpy(split_name, out_path + strlen(split_dir));
  }
  if (i < 0) {
    strcpy(split_dir, "(current directory)");
  }
  if (mesg_line_started)
    zfprintf(mesg, "\n");

  zfprintf(mesg, "\nOpening disk %d\n", num);
  zfprintf(mesg, "Hit ENTER to write to default path of\n");
  zfprintf(mesg, "  %s\n", split_dir);
  zfprintf(mesg, "or enter a new directory path (. for cur dir) and hit ENTER\n");
  for (;;) {
    zfprintf(mesg, "\nPath (or hit ENTER to continue): ");
    fflush(mesg);
    fgets(buf, FNMAX, stdin);
    /* remove any newline */
    for (i = 0; buf[i]; i++) {
      if (buf[i] == '\n') {
        buf[i] = '\0';
        break;
      }
    }
    if (strlen(buf) > 0) {
      /* changing path */

      /* current directory */
      if (buf[0] == '.' && buf[1] == '\0') {
        buf[0] = '\0';
      }
      /* remove any name at end */
      for (i = (int)strlen(buf); i >= 0; i--) {
        if (buf[i] == '/' || buf[i] == '\\'
             || buf[i] == ':') {
          buf[i + 1] = '\0';
          break;
        }
      }
      /* update out_path to newdir/split_name */
      free(out_path);
      if (i < 0) {
        /* just name so current directory */
        strcpy(buf, "(current directory)");
        if (split_name == NULL) {
          i = 0;
        } else {
          i = (int)strlen(split_name);
        }
        if ((out_path = malloc(strlen(split_name) + 40)) == NULL) {
          ZIPERR(ZE_MEM, "split path");
        }
        strcpy(out_path, split_name);
      } else {
        /* not the current directory */
        /* remove any name at end */
        for (i = (int)strlen(buf); i >= 0; i--) {
          if (buf[i] == '/') {
            buf[i + 1] = '\0';
            break;
          }
        }
        if (i < 0) {
          buf[0] = '\0';
        }
        if ((out_path = malloc(strlen(buf) + strlen(split_name) + 40)) == NULL) {
          ZIPERR(ZE_MEM, "split path");
        }
        strcpy(out_path, buf);
        strcat(out_path, split_name);
      }
      zfprintf(mesg, "Writing to:\n  %s\n", buf);
      free(split_name);
      free(split_dir);
      if ((split_dir = malloc(strlen(out_path) + 40)) == NULL) {
        ZIPERR(ZE_MEM, "split path");
      }
      strcpy(split_dir, out_path);
      /* remove any name at end */
      for (i = (int)strlen(split_dir); i >= 0; i--) {
        if (split_dir[i] == '/') {
          split_dir[i + 1] = '\0';
          break;
        }
      }

      if ((split_name = malloc(strlen(out_path) + 1)) == NULL) {
        ZIPERR(ZE_MEM, "split path");
      }
      strcpy(split_name, out_path + strlen(split_dir));
    } else {
      break;
    }
  }
  free(split_dir);
  free(split_name);

  /* for now no way out except Ctrl C */
  return 1;
}


/* split_name
 *
 * get name of split being read
 */
char *get_in_split_path(base_path, disk_number)
  char *base_path;
  ulg disk_number;
{
  char *split_path = NULL;
  int base_len = 0;
  int path_len = 0;
  ulg num = disk_number + 1;
  char ext[6];
#ifdef VMS
  int vers_len;                         /* File version length. */
  char *vers_ptr;                       /* File version string. */
#endif /* def VMS */

  /*
   * A split has extension z01, z02, ..., z99, z100, z101, ... z999
   * We currently support up to .z99999
   * WinZip will also read .100, .101, ... but AppNote 6.2.2 uses above
   * so use that.  Means on DOS can only have 100 splits.
   */

  if (num == total_disks) {
    /* last disk is base path */
    if ((split_path = malloc(strlen(base_path) + 1)) == NULL) {
      ZIPERR(ZE_MEM, "base path");
    }
    strcpy(split_path, base_path);

    return split_path;
  } else {
    if (num > 99999) {
      ZIPERR(ZE_BIG, "More than 99999 splits needed");
    }
    sprintf(ext, "z%02lu", num);
  }

  /* create path for this split - zip.c checked for .zip extension */
  base_len = (int)strlen(base_path) - 3;
  path_len = base_len + (int)strlen(ext);

#ifdef VMS
  /* On VMS, locate the file version, and adjust base_len accordingly.
     Note that path_len is correct, as-is.
  */
  vers_ptr = vms_file_version( base_path);
  vers_len = strlen( vers_ptr);
  base_len -= vers_len;
#endif /* def VMS */

  if ((split_path = malloc(path_len + 1)) == NULL) {
    ZIPERR(ZE_MEM, "split path");
  }
  /* copy base_path except for end zip */
  strcpy(split_path, base_path);
  split_path[base_len] = '\0';
  /* add extension */
  strcat(split_path, ext);

#ifdef VMS
  /* On VMS, append (preserve) the file version. */
  strcat(split_path, vers_ptr);
#endif /* def VMS */

  return split_path;
}


/* split_name
 *
 * get name of split being written
 */
char *get_out_split_path(base_path, disk_number)
  char *base_path;
  ulg disk_number;
{
  char *split_path = NULL;
  int base_len = 0;
  int path_len = 0;
  ulg num = disk_number + 1;
  char ext[6];
#ifdef VMS
  int vers_len;                         /* File version length. */
  char *vers_ptr;                       /* File version string. */
#endif /* def VMS */

  /*
   * A split has extension z01, z02, ..., z99, z100, z101, ... z999
   * We currently support up to .z99999
   * WinZip will also read .100, .101, ... but AppNote 6.2.2 uses above
   * so use that.  Means on DOS can only have 100 splits.
   */

  if (num > 99999) {
    ZIPERR(ZE_BIG, "More than 99999 splits needed");
  }
  sprintf(ext, "z%02lu", num);

  /* create path for this split - zip.c checked for .zip extension */
  base_len = (int)strlen(base_path) - 3;
  path_len = base_len + (int)strlen(ext);

#ifdef VMS
  /* On VMS, locate the file version, and adjust base_len accordingly.
     Note that path_len is correct, as-is.
  */
  vers_ptr = vms_file_version( base_path);
  vers_len = strlen( vers_ptr);
  base_len -= vers_len;
#endif /* def VMS */

  if ((split_path = malloc(path_len + 1)) == NULL) {
    ZIPERR(ZE_MEM, "split path");
  }
  /* copy base_path except for end zip */
  strcpy(split_path, base_path);
  split_path[base_len] = '\0';
  /* add extension */
  strcat(split_path, ext);

#ifdef VMS
  /* On VMS, append (preserve) the file version. */
  strcat(split_path, vers_ptr);
#endif /* def VMS */

  return split_path;
}

/* close_split
 *
 * close a split - assume that the paths needed for the splits are
 * available.
 */
int close_split(disk_number, tempfile, temp_name)
  ulg disk_number;
  FILE *tempfile;
  char *temp_name;
{
  char *split_path = NULL;

  split_path = get_out_split_path(out_path, disk_number);

  if (noisy_splits) {
    zipmessage("        Closing split ", split_path);
  }

  fclose(tempfile);

  rename_split(temp_name, split_path);
  set_filetype(split_path);

  return ZE_OK;
}

/* bfwrite - buffered fwrite
   Does the fwrite but also counts bytes and does splits */
size_t bfwrite(buffer, size, count, mode)
  ZCONST void *buffer;
  size_t size;
  size_t count;
  int mode;
{
  size_t bytes_written = 0;
  size_t r;
  size_t b = size * count;
  uzoff_t bytes_left_in_split = 0;
  size_t bytes_to_write = b;


  /* -------------------------------- */
  /* local header */
  if (mode == BFWRITE_LOCALHEADER) {
    /* writing local header - reset entry data count */
    bytes_this_entry = 0;
    /* save start of local header so we can rewrite later */
    current_local_file = y;
    current_local_disk = current_disk;
    current_local_offset = bytes_this_split;
  }

  if (split_size == 0)
    bytes_left_in_split = bytes_to_write;
  else
    bytes_left_in_split = split_size - bytes_this_split;

  if (bytes_to_write > bytes_left_in_split) {
    if (mode == BFWRITE_HEADER ||
        mode == BFWRITE_LOCALHEADER ||
        mode == BFWRITE_CENTRALHEADER) {
      /* if can't write entire header save for next split */
      bytes_to_write = 0;
    } else {
      /* normal data so fill the split */
      bytes_to_write = (size_t)bytes_left_in_split;
    }
  }

  /* -------------------------------- */
  /* central header */
  if (mode == BFWRITE_CENTRALHEADER) {
    /* set start disk for CD */
    if (cd_start_disk == (ulg)-1) {
      cd_start_disk = current_disk;
      cd_start_offset = bytes_this_split;
    }
    cd_entries_this_disk++;
    total_cd_entries++;
  }

  /* -------------------------------- */
  if (bytes_to_write > 0) {
    /* write out the bytes for this split */
    r = fwrite(buffer, size, bytes_to_write, y);
    bytes_written += r;
    bytes_to_write = b - r;
    bytes_this_split += r;
    if (mode == BFWRITE_DATA)
      /* if data descriptor do not include in count */
      bytes_this_entry += r;
  } else {
    bytes_to_write = b;
  }

  if (bytes_to_write > 0) {
    if (split_method) {
      /* still bytes to write so close split and open next split */
      bytes_prev_splits += bytes_this_split;

      if (split_method == 1 && ferror(y)) {
        /* if writing all splits to same place and have problem then bad */
        ZIPERR(ZE_WRITE, "Could not write split");
      }

      if (split_method == 2 && ferror(y)) {
        /* A split must be at least 64K except last .zip split */
        if (bytes_this_split < 64 * (uzoff_t)0x400) {
          ZIPERR(ZE_WRITE, "Not enough space to write split");
        }
      }

      /* close this split */
      if (split_method == 1 && current_local_disk == current_disk && !use_descriptors) {
        /* if using split_method 1 (rewrite headers) and still working on current disk
           and we are not using data descriptors (if we are, we can't rewrite the local
           headers and so putlocal() won't be called to rewrite them and that's where
           the split is closed), keep split open so can update it */
        current_local_tempname = tempzip;
      } else {
        /* we won't be updating the split, so close it */
        close_split(current_disk, y, tempzip);
        y = NULL;
        free(tempzip);
        tempzip = NULL;
      }
      cd_entries_this_disk = 0;
      bytes_this_split = 0;

      /* increment disk - disks are numbered 0, 1, 2, ... and
         splits are 01, 02, ... */
      current_disk++;

      if (split_method == 2 && split_bell) {
        /* bell when pause to ask for next split */
        putc('\007', mesg);
        fflush(mesg);
      }

      for (;;) {
        /* if method 2 pause and allow changing path */
        if (split_method == 2) {
          if (ask_for_split_write_path(current_disk) == 0) {
            ZIPERR(ZE_ABORT, "could not write split");
          }
        }

        /* open next split */
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
            strcpy(tempzip, out_path);
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
          if (show_what_doing) {
            sprintf(errbuf, "sd: Temp file (0u): %s", tempzip);
            zipmessage(errbuf, "");
          }
          if ((y = fdopen(yd, FOPW_TMP)) == NULL) {
            ZIPERR(ZE_TEMP, tempzip);
          }
        }
#else
        if ((tempzip = tempname(out_path)) == NULL) {
          ZIPERR(ZE_MEM, "allocating temp filename");
        }
        if (show_what_doing) {
          sprintf(errbuf, "sd: Temp file (0n): %s", tempzip);
          zipmessage(errbuf, "");
        }
        if ((y = zfopen(tempzip, FOPW_TMP)) == NULL) {
          ZIPERR(ZE_TEMP, tempzip);
        }
#endif

        r = fwrite((char *)buffer + bytes_written, 1, bytes_to_write, y);
        bytes_written += r;
        bytes_this_split += r;
        if (!(mode == BFWRITE_HEADER ||
              mode == BFWRITE_LOCALHEADER ||
              mode == BFWRITE_CENTRALHEADER)) {
          bytes_this_entry += r;
        }
        if (bytes_to_write > r) {
          /* buffer bigger than split */
          if (split_method == 2) {
            /* let user choose another disk */
            zipwarn("Not enough room on disk", "");
            continue;
          } else {
            ZIPERR(ZE_WRITE, "Not enough room on disk");
          }
        }
        if (mode == BFWRITE_LOCALHEADER ||
            mode == BFWRITE_HEADER ||
            mode == BFWRITE_CENTRALHEADER) {
          if (split_method == 1 && current_local_file &&
              current_local_disk != current_disk) {
            /* We're opening a new split because the next header
               did not fit on the last split.  We need to now close
               the last split and update the pointers for
               the current split. */
            close_split(current_local_disk, current_local_file,
                        current_local_tempname);
            free(current_local_tempname);
          }
          current_local_tempname = tempzip;
          current_local_file = y;
          current_local_offset = 0;
          current_local_disk = current_disk;
        }
        break;
      }
    }
    else
    {
      /* likely have more than fits but no splits */

      /* probably already have error "no space left on device" */
      /* could let flush_outbuf() handle error but bfwrite() is called for
         headers also */
      if (ferror(y))
        ziperr(ZE_WRITE, "write error on zip file");
    }
  }


  /* display dots for archive instead of for each file */
  if (display_globaldots) {
    if (dot_size > 0) {
      /* initial space */
      if (dot_count == -1) {
        display_dot_char(' ');
        /* assume a header will be written first, so avoid 0 */
        dot_count = 1;
      }
      /* skip incrementing dot count for small buffers like for headers */
      if (size * count > 1000) {
        dot_count++;
        if (dot_size <= dot_count * (zoff_t)size * (zoff_t)count) dot_count = 0;
      }
    }
    if (dot_size && !dot_count) {
      dot_count++;
      display_dot_char('.');
      mesg_line_started = 1;
    }
  }


  return bytes_written;
}



/* --- Entry Timing --- */

/* If entry timing is enabled, get_time_in_us() is used
   to determine rate in bytes / second.  */
#ifdef ENABLE_ENTRY_TIMING
# ifndef WIN32

uzoff_t get_time_in_usec()
{
  struct timeval now;

  gettimeofday(&now, NULL);
  return now.tv_sec * 1000000 + now.tv_usec;
}

# endif
#endif



/* ===================================================================== */
/* LOCALE                                                                */
/* ===================================================================== */

/* set_locale - read and set (as needed) the locale
 *
 * This code moved here so can be used by utilities.
 *
 * Returns 1 if successful.
 */
int set_locale()
{
#ifdef THEOS
  setlocale(LC_CTYPE, "I");
#else
  /* This is all done below now */
# if 0
  /* Tell base library that we support locales.  This
     will load the locale the user has selected.  Before
     setlocale() is called, a minimal "C" locale is the
     default. */
  /* This is undefined in Win32.  Will try to address it in the next beta.
     However, on Windows we use the wide paths that are Unicode, so may
     not need to worry about locale. */
  /* It looks like we're only supporting back to Windows XP now, so this
     should be OK. */
#  ifndef WIN32
  SETLOCALE(LC_CTYPE, "");
#  endif
# endif
#endif

/* --------------------------------------------------------------------- */
/* Locale detection is now done regardless of setting of UNICODE_SUPPORT.
    (setlocale was already being executed above.) */

  { /* locale detection block */
    char *loc = NULL;
    char *codeset = NULL;
    int locale_debug = 0;
    int charsetlen = 0;

#ifdef HAVE_SETLOCALE
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

     As of Zip 3.1, Windows console output uses print_utf8(), which calls
     write_consolew(), which does not need a UTF-8 console to output Unicode.
     This still displays boxes for Japanese and similar fonts, though, as that
     is a font support issue of Windows consoles.

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
     the results.

     We plan to update the locale checks in set_locale() shortly.
  */

# ifdef UNIX
    {
      if (locale_debug) {
        loc = setlocale(LC_CTYPE, NULL);
        zprintf("  Initial language locale = '%s'\n", loc);
      }

      /* New check provided by Danny Milosavljevic (SourceForge) */

      /* Tell base library that we support locales.  This
         will load the locale the user has selected.  Before
         setlocale() is called, a minimal "C" locale is the
         default. */
      setlocale(LC_CTYPE, "");

      loc = setlocale(LC_CTYPE, NULL);
      if (loc) {
        if (locale_debug) {
          zprintf("  Locale after initialization = '%s'\n", loc);
        }
        if ((localename = (char *)malloc((strlen(loc) + 1) * sizeof(char))) == NULL) {
          ZIPERR(ZE_MEM, "localename");
        }
        strcpy(localename, loc);
      }

#  ifndef NO_NL_LANGINFO
      /* get the codeset (character set encoding) currently used,
         for example "UTF-8". */
      codeset = nl_langinfo(CODESET);

      if (codeset) {
        if (locale_debug) {
          zprintf("  charsetname = codeset = '%s'\n", codeset);
        }
        charsetlen = strlen(codeset) + 1;
        if ((charsetname = (char *)malloc(charsetlen * sizeof(char))) == NULL) {
          ZIPERR(ZE_MEM, "localename");
        }
        strcpy(charsetname, codeset);
      }

#  else
      /* lacking a way to get codeset, get locale */
      {
        char *c;
        loc = setlocale(LC_CTYPE, NULL);
        /* for UTF-8, should be close to en_US.UTF-8 */
        for (c = loc; c; c++) {
          if (*c == '.') {
            /* loc is what is after '.', maybe UTF-8 */
            loc = c + 1;
            break;
          }
        }
      }

      if (locale_debug) {
        zprintf("  End part Locale = '%s'\n", loc);
      }

      charsetlen = strlen(loc) + 1;

      if ((charsetname = (char *)malloc(charsetlen * sizeof(char))) == NULL) {
        ZIPERR(ZE_MEM, "charsetname");
      }
      strcpy(charsetname, loc);

      if (locale_debug) {
        zprintf("  charsetname = '%s'\n", charsetname);
      }
#  endif

      if ((codeset && strcmp(codeset, "UTF-8") == 0)
           || (loc && strcmp(loc, "UTF-8") == 0)) {
        /* already using UTF-8 */
        using_utf8 = 1;
      }
      /* Tim K. advises not to force UTF-8 if not native */
#  if 0
      else {
        /* try setting UTF-8 */
        if (setlocale(LC_CTYPE, "en_US.UTF-8") != NULL) {
          using_utf8 = 1;
        } else {
          if (locale_debug) {
            zprintf("  Could not set Unicode UTF-8 locale\n");
          }
        }
        if (locale_debug) {
          zprintf("  Could not set Unicode UTF-8 locale\n");
        }
      }
#  endif


      /* Alternative fix for just MAEMO. */
# if 0
#  ifdef MAEMO
      loc = setlocale(LC_CTYPE, "");
#  else
      loc = setlocale(LC_CTYPE, "en_US.UTF-8");
#  endif

      if (locale_debug) {
        zprintf("langinfo %s\n", nl_langinfo(CODESET));
      }

      if (loc != NULL) {
        /* using UTF-8 character set so can set UTF-8 GPBF bit 11 */
        using_utf8 = 1;
        if (locale_debug) {
          zprintf("  Locale set to %s\n", loc);
        }
      } else {
        if (locale_debug) {
          zprintf("  Could not set Unicode UTF-8 locale\n");
        }
      }
#  endif
    }

# else /* not UNIX */

#  ifdef WIN32
    {
      char *loc = NULL;
      int codepage = 0;
      int charsetlen = 0;
      char *prefix = "WINDOWS-";

      if (locale_debug) {
        loc = setlocale(LC_CTYPE, NULL);
        zprintf("  Initial language locale = '%s'\n", loc);
      }

      /* Set the applications codepage to the current windows codepage.
       */
      setlocale(LC_CTYPE, "");

      loc = setlocale(LC_CTYPE, NULL);

      if (locale_debug) {
        zprintf("  Locale after initialization = '%s'\n", loc);
      }

      if (loc) {
        if ((localename = (char *)malloc((strlen(loc) + 1) * sizeof(char))) == NULL) {
          ZIPERR(ZE_MEM, "localename");
        }
        strcpy(localename, loc);
      }

      /* Windows does not have nl_langinfo */

      /* lacking a way to get codeset, get locale */
      {
        char *c;
        loc = setlocale(LC_CTYPE, NULL);
        /* for UTF-8, should be close to en_US.UTF-8 */
        for (c = loc; c; c++) {
          if (*c == '.') {
            /* loc is what is after '.', maybe UTF-8 */
            loc = c + 1;
            break;
          }
        }
      }
      if (locale_debug) {
        zprintf("  End part Locale = '%s'\n", loc);
      }

      charsetlen = (int)(strlen(prefix) + strlen(loc) + 1);

      if ((charsetname = (char *)malloc(charsetlen * sizeof(char))) == NULL) {
        ZIPERR(ZE_MEM, "charsetname");
      }
      strcpy(charsetname, prefix);
      strcat(charsetname, loc);

      if (locale_debug) {
        zprintf("  charsetname = '%s'\n", charsetname);
      }

      codepage = _getmbcp();

      if (locale_debug) {
        zprintf("  Codepage = '%d'\n", codepage);
      }

      /* GetLocaleInfo; */

    }

#  endif /* WIN32 */
# endif /* UNIX */

#else /* not HAVE_SETLOCALE */

/* other ports */
  SETLOCALE(LC_CTYPE, "");

#endif /* not HAVE_SETLOCALE */

    if (locale_debug) {
      zprintf("\n");
    }
  } /* locale detection block */

    return 1;

} /* set_locale() */

/* --------------------------------------------------------------------- */


/* ===================================================================== */



int get_entry_comment(z)
  struct zlist far *z;
{
  char e[MAX_COM_LEN + 1];
  char eline[MAXCOMLINE + 1];
  char *p;
  int comlen;
  int eline_len;

  if (noisy) {
    if (z->com && z->comment) {
      char *c;
      /* add leading space to indent each line of comment */
      c = string_replace(z->comment, "\n", "\n ", REPLACE_ALL, CASE_INS);
      sprintf(errbuf, "\nCurrent comment for %s:\n %s", z->oname, c);
      print_utf8(errbuf);
      free(c);
      sprintf(errbuf, "\nEnter comment for %s:\n ", z->oname);
      print_utf8(errbuf);
      sprintf(errbuf, "(ENTER=keep, TAB ENTER=remove, SPACE ENTER=multiline)\n ");
      print_utf8(errbuf);
    } else {
      sprintf(errbuf, "\nEnter comment for %s:\n ", z->oname);
      print_utf8(errbuf);
      sprintf(errbuf, "(SPACE ENTER=multiline)\n ");
      print_utf8(errbuf);
    }
  }
  if (fgets(e, MAXCOMLINE+1, comment_stream) != NULL)
  {
    if (strlen(e) > 1) {
      if (strlen(e) == 2 && e[0] == '\t') {
        /* remove the comment */
        e[0] = '\0';
      }
      if (strlen(e) == 2 && e[0] == ' ') {
        /* get multi-line comment */
        e[0] = '\0';
        sprintf(errbuf, "\nEnter multi-line comment for %s:\n", z->oname);
        print_utf8(errbuf);
        sprintf(errbuf, "(Enter line with just \".\" to end)\n ");
        print_utf8(errbuf);
        comlen = 0;
        while (fgets(eline, MAXCOMLINE+1, comment_stream) != NULL)
        {
          if (strlen(eline) == 2 && eline[0] == '.')
            break;
          eline_len = (int)strlen(eline);
          if (comlen + eline_len > MAX_COM_LEN) {
            /* limit total comment length to MAX_COM_LEN */
            eline_len = MAX_COM_LEN - comlen;
            eline[eline_len] = '\0';
          }
          strcat(e, eline);
          comlen = (int)strlen(e);
          if (comlen >= MAX_COM_LEN) {
            sprintf(errbuf, "Max comment length reached (%d)", MAX_COM_LEN);
            zipwarn(errbuf, "");
            break;
          }
          zfprintf(mesg, " ");
        }
      } /* multi-line comment */
      /* get space for new comment */
      if ((p = (char *)malloc((comment_size = strlen(e))+1)) == NULL)
      {
        ZIPERR(ZE_MEM, "was reading comment lines (s2)");
      }
      strcpy(p, e);
      if (p[comment_size - 1] == '\n')
        p[--comment_size] = 0;
      if (z->com && z->comment) {
        free(z->comment);
        z->com = 0;
      }
      z->comment = p;
      if (comment_size == 0) {
        free(z->comment);
        z->comment = NULL;
      }
      /* zip64 support 09/05/2003 R.Nausedat */
      z->com = (ush)comment_size;
    }
  }

  return z->com;
}


#if 0
/* Currently unused. */

/* trim_string() - trim leading and trailing white space from string
 *
 * Returns trimmed malloc'd copy of string.
 */
#ifndef NO_PROTO
char *trim_string(char *instring)
#else
char *trim_string(instring)
  char *instring;
#endif
{
  char *trimmed_string = NULL;
  int i;
  int j;
  int non_white_start = 0;
  int non_white_end = -1;
  int len;
  int trimmed_len = 0;
  int c;

  if (instring == NULL)
    return NULL;
  
  len = (int)strlen(instring);

  if (len > 0) {
    /* find first non-white char */
    for (i = 0; instring[i]; i++) {
      c = (unsigned char)instring[i];
      if (!isspace(c))
        break;
    }
    non_white_start = i;

    if (non_white_start != len) {
      /* find last non-white char */
      for (i = len - 1; i >= 0; i--) {
        c = (unsigned char)instring[i];
        if (!isspace(c))
          break;
      }
      non_white_end = i;
    }
  } /* len > 0 */

  trimmed_len = non_white_end - non_white_start + 1;
  if (trimmed_len <= 0)
    trimmed_len = 0;

  if ((trimmed_string = (char *)malloc(trimmed_len + 1)) == NULL) {
    sprintf(errbuf, "Could not allocate memory in trim_string()\n");
    ZIPERR(ZE_MEM, errbuf);
  }

  j = 0;
  if (trimmed_len > 0) {
    for (i = non_white_start; i <= non_white_end; i++) {
      trimmed_string[j++] = instring[i];
    }
  }
  trimmed_string[j] = '\0';

  return trimmed_string;
}
#endif /* trim_string() */



/* string_dup - duplicate a string
 *
 * in_string      = string to duplicate
 * error_message  = error message if memory can't be allocated
 * fluff          = extra bytes to allocate
 *
 * Returns a duplicate of the string, or NULL.
 */
#ifndef NO_PROTO
char *string_dup(ZCONST char *in_string, char *error_message, int fluff)
#else
char *string_dup(in_string, error_message)
  ZCONST char *in_string;
  char *error_message;
  int fluff;
#endif
{
  char *out_string;

  if (in_string == NULL)
    return NULL;

  if (fluff < 0)
    return NULL;

  if ((out_string = (char *)malloc((strlen(in_string) + fluff + 1) * sizeof(char))) == NULL) {
    sprintf(errbuf, "could not allocate memory in string_dup: %s", error_message);
    ZIPERR(ZE_MEM, errbuf);
  }

  strcpy(out_string, in_string);
  return out_string;
}


/* string_replace - replace substring with string
 *
 * Not MBCS aware!
 *
 * in_string - string to find substrings in and replace
 * find - the string to find
 * replace - the string to replace find with
 * replace_times - how many replacements to do, if 0 (or REPLACE_ALL) replace
 *   all occurrences
 * case_sens - match case sensitive (CASE_INS, CASE_SEN)
 *
 * Returns malloc'd string with replacements, or NULL.
 */
#ifndef NO_PROTO
char *string_replace(char *in_string, char *find, char *replace,
                     int replace_times, int case_sens)
#else
char *string_replace(in_string, find, replace,
                     replace_times, case_sens)
  char *in_string;
  char *find;
  char *replace;
  int replace_times;
  int case_sens;
#endif
{
  int i;
  int j;
  int in_len;
  int find_len;
  int replace_len;
  int out_len;
  int possible_times;
  int possible_increase;
  int replacements = 0;
  char *out_string;
  char *return_string;

  if (in_string == NULL)
    return NULL;
  if (find == NULL)
    return NULL;
  if (replace == NULL)
    return NULL;

  in_len = (int)strlen(in_string);
  find_len = (int)strlen(find);
  replace_len = (int)strlen(replace);

  if (find_len == 0)
    return string_dup(in_string, "string_replace", 0);

  /* calculate max length of out_string */
  possible_times = in_len/find_len + 1;
  if (possible_times < 0) {
    /* find won't fit in in_string */
    return string_dup(in_string, "string_replace", 0);
  }
  if (replace_times && replace_times < possible_times)
    possible_times = replace_times;

  possible_increase = replace_len - find_len;
  if (possible_increase < 0)
    possible_increase = 0;
  out_len = in_len + possible_times * possible_increase + 1;

  if ((out_string = (char *)malloc(out_len)) == NULL) {
    ZIPERR(ZE_MEM, "string_replace");
  }

  out_string[0] = '\0';

  for (i = 0, j = 0; i < in_len; )
  {
    if (!(replace_times && replacements >= replace_times) &&
        strmatch(in_string + i, find, case_sens, find_len)) {
      /* copy replace string to out_string */
      strcat(out_string, replace);
      i += find_len;
      j += replace_len;
      replacements++;
    }
    else {
      /* copy character to out_string */
      out_string[j++] = in_string[i++];
      out_string[j] = '\0';
    }
  }

  return_string = string_dup(out_string, "string_replace", 0);
  free(out_string);

  return return_string;
}


/* string_find - find a substring in a string
 *
 * instring    - input string
 * findstring  - substring to find in instring
 * case_sens   - if 0, search is case insensitive (CASE_INS, CASE_SEN)
 * occurrence  - search for this many matches (LAST_MATCH (-1) for last)
 *
 * Matches can overlap, i.e. if instring = "aaa" and find = "aa", 2 matches
 * will be found.
 *
 * Returns the (zero-based) index of substring, or NO_MATCH (-1) if not found.
 */
#ifndef NO_PROTO
int string_find (char *instring, char *find, int case_sens, int occurrence)
#else
int string_find (instring, find, case_sens, occurrence)
  char *instring;
  char *find;
  int case_sens;
  int occurrence;
#endif
{
  int start;
  int matches = 0;
  int instring_len;
  int find_len;
  int match_start = 0;

  if (instring == NULL || find == NULL) {
    return NO_MATCH;
  }

  instring_len = (int)strlen(instring);
  find_len = (int)strlen(find);

  for (start = 0; start < instring_len - find_len; start++) {
    if (strmatch(instring + start, find, case_sens, find_len)) {
      match_start = start;
      matches++;
    }
    if ((occurrence != LAST_MATCH) && (matches >= occurrence))
      break;
  }

  if (matches == 0)
    return NO_MATCH;
  else
    return match_start;
}



#ifdef UNICODE_SUPPORT

/* read_utf8_bom - check for three UTF-8 marker bytes at start of text file
 *                (get this if save as "UTF-8" on Windows)
 *
 * Reads and checks first three bytes.  Puts the bytes back if not
 * the byte marker.
 *
 * Returns true if marker found (is UTF-8), else false.
 */
int read_utf8_bom(FILE *infile)
{
  int b1, b2, b3;
  
  /* read first 3 bytes */
  b1 = getc(infile);
  if (b1 != 0xef) {
    ungetc(b1, infile);
    return 0;
  }
  b2 = getc(infile);
  if (b2 != 0xbb) {
    ungetc(b2, infile);
    ungetc(b1, infile);
    return 0;
  }
  b3 = getc(infile);
  if (b3 != 0xbf) {
    ungetc(b3, infile);
    ungetc(b2, infile);
    ungetc(b1, infile);
    return 0;
  }

  return 1;
}


/* is_utf16LE_file - check for Windows Unicode marker bytes at start of WIN
                     UTF-16 Unicode text file (get this if save as "Unicode")
 *
 * Reads and checks first three bytes.  Puts the bytes back if not
 * the marker.
 *
 * Returns true if marker found (is Windows Unicode), else false.
 */
int is_utf16LE_file(FILE *infile)
{
  char b[4];
  int bsize;
  int i;
  int b1, b2;
  
  rewind(infile);

  /* read first 2 bytes */
  bsize = (int)fread(b, 1, 2, infile);

  if (bsize < 2) {
    for (i = bsize - 1; i >= 0; i--)
      ungetc(b[i], infile);
    return 0;
  }

  b1 = (unsigned char)b[0];
  b2 = (unsigned char)b[1];

#if 0
  if (b1 != 0 && b2 == 0 && b3 != 0) {
    /* looks like double byte */
    for (i = bsize - 1; i >= 0; i--)
      ungetc(b[i], infile);
    return 1;
  }
#endif

  if (b1 == 0xff && b2 == 0xfe) {
    /* UTF-16LE Unicode header */
    return 1;
  }

  /* if not the BOM we are looking for, put bytes back */
  for (i = bsize - 1; i >= 0; i--)
    ungetc(b[i], infile);

  return 0;
}
#endif /* UNICODE_SUPPORT */





#ifdef UNICODE_SUPPORT

/*---------------------------------------------
 * Unicode conversion functions
 *
 * Provided by Paul Kienitz
 *
 * Some modifications to work with Zip
 *
 *---------------------------------------------
 */

/*
   NOTES APPLICABLE TO ALL STRING FUNCTIONS:

   All of the x_to_y functions take parameters for an output buffer and
   its available length, and return an int.  The value returned is the
   length of the string that the input produces, which may be larger than
   the provided buffer length.  If the returned value is less than the
   buffer length, then the contents of the buffer will be null-terminated;
   otherwise, it will not be terminated and may be invalid, possibly
   stopping in the middle of a multibyte sequence.

   In all cases you may pass NULL as the buffer and/or 0 as the length, if
   you just want to learn how much space the string is going to require.

   The functions will return -1 if the input is invalid UTF-8 or cannot be
   encoded as UTF-8.
*/

/* utility functions for managing UTF-8 and UCS-4 strings */


/* utf8_char_bytes
 *
 * Returns the number of bytes used by the first character in a UTF-8
 * string, or -1 if the UTF-8 is invalid or null.
 */
local int utf8_char_bytes(utf8)
  ZCONST char *utf8;
{
  int      t, r;
  unsigned lead;

  if (!utf8)
    return -1;          /* no input */
  lead = (unsigned char) *utf8;
  if (lead < 0x80)
    r = 1;              /* an ascii-7 character */
  else if (lead < 0xC0)
    return -1;          /* error: trailing byte without lead byte */
  else if (lead < 0xE0)
    r = 2;              /* an 11 bit character */
  else if (lead < 0xF0)
    r = 3;              /* a 16 bit character */
  else if (lead < 0xF8)
    r = 4;              /* a 21 bit character (the most currently used) */
  else if (lead < 0xFC)
    r = 5;              /* a 26 bit character (shouldn't happen) */
  else if (lead < 0xFE)
    r = 6;              /* a 31 bit character (shouldn't happen) */
  else
    return -1;          /* error: invalid lead byte */
  for (t = 1; t < r; t++)
    if ((unsigned char) utf8[t] < 0x80 || (unsigned char) utf8[t] >= 0xC0)
      return -1;        /* error: not enough valid trailing bytes */
  return r;
}


/* ucs4_char_from_utf8
 *
 * Given a reference to a pointer into a UTF-8 string, returns the next
 * UCS-4 character and advances the pointer to the next character sequence.
 * Returns ~0 and does not advance the pointer when input is ill-formed.
 *
 * Since the Unicode standard says 32-bit values won't be used (just
 * up to the current 21-bit mappings) changed this to signed to allow -1 to
 * be returned.
 */
local long ucs4_char_from_utf8(utf8)
  ZCONST char **utf8;
{
  ulg  ret;
  int  t, bytes;

  if (!utf8)
    return -1;                          /* no input */
  bytes = utf8_char_bytes(*utf8);
  if (bytes <= 0)
    return -1;                          /* invalid input */
  if (bytes == 1)
    ret = **utf8;                       /* ascii-7 */
  else
    ret = **utf8 & (0x7F >> bytes);     /* lead byte of a multibyte sequence */
  (*utf8)++;
  for (t = 1; t < bytes; t++)           /* consume trailing bytes */
    ret = (ret << 6) | (*((*utf8)++) & 0x3F);
  return (long) ret;
}


/* utf8_from_ucs4_char - Convert UCS char to UTF-8
 *
 * Returns the number of bytes put into utf8buf to represent ch, from 1 to 6,
 * or -1 if ch is too large to represent.  utf8buf must have room for 6 bytes.
 */
local int utf8_from_ucs4_char(utf8buf, ch)
  char *utf8buf;
  ulg ch;
{
  int trailing = 0;
  int leadmask = 0x80;
  int leadbits = 0x3F;
  ulg tch = ch;
  int ret;

  if (ch > 0x7FFFFFFF)
    return -1;                /* UTF-8 can represent 31 bits */
  if (ch < 0x7F)
  {
    *utf8buf++ = (char) ch;   /* ascii-7 */
    return 1;
  }
  do {
    trailing++;
    leadmask = (leadmask >> 1) | 0x80;
    leadbits >>= 1;
    tch >>= 6;
  } while (tch & ~leadbits);
  ret = trailing + 1;
  /* produce lead byte */
  *utf8buf++ = (char) (leadmask | (ch >> (6 * trailing)));
  /* produce trailing bytes */
  while (--trailing >= 0)
    *utf8buf++ = (char) (0x80 | ((ch >> (6 * trailing)) & 0x3F));
  return ret;
}


/*===================================================================*/

/* utf8_to_ucs4_string - convert UTF-8 string to UCS string
 *
 * Return UCS count.  Now returns int so can return -1.
 */
local int utf8_to_ucs4_string(utf8, ucs4buf, buflen)
  ZCONST char *utf8;
  ulg *ucs4buf;
  int buflen;
{
  int count = 0;

  for (;;)
  {
    long ch = ucs4_char_from_utf8(&utf8);
    if (ch == -1)
      return -1;
    else
    {
      if (ucs4buf && count < buflen)
        ucs4buf[count] = ch;
      if (ch == 0)
        return count;
      count++;
    }
  }
}


/* ucs4_string_to_utf8
 *
 *
 */
local int ucs4_string_to_utf8(ucs4, utf8buf, buflen)
  ZCONST ulg *ucs4;
  char *utf8buf;
  int buflen;
{
  char mb[6];
  int  count = 0;

  if (!ucs4)
    return -1;

  for (;;)
  {
    int mbl;
    int c;

    mbl = utf8_from_ucs4_char(mb, *ucs4++);
    if (mbl <= 0) {
      return -1;
    }
    /* We could optimize this a bit by passing utf8buf + count */
    /* directly to utf8_from_ucs4_char when buflen >= count + 6... */
    c = buflen - count;
    if (mbl < c)
      c = mbl;
    if (utf8buf && count < buflen)
      strncpy(utf8buf + count, mb, c);
    if (mbl == 1 && !mb[0])
      return count;           /* terminating nul */
    count += mbl;
  }
}


#if 0  /* currently unused */
/* utf8_chars
 *
 * Wrapper: counts the actual unicode characters in a UTF-8 string.
 */
local int utf8_chars(utf8)
  ZCONST char *utf8;
{
  return utf8_to_ucs4_string(utf8, NULL, 0);
}
#endif

#endif /* UNICODE_SUPPORT */



/* moved here from zip.c to allow access to by utilities */

/* rename a split
 * A split has a tempfile name until it is closed, then
 * here rename it as out_path the final name for the split.
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


/* ------------------------------------------------------------- */
/* Output functions from zip.c. */

void zipmessage_nl(a, nl)
ZCONST char *a;     /* message string to output */
int nl;             /* 1 = add nl to end */
/* If nl false, print a message to mesg without new line.
   If nl true, print and add new line.
   If logfile is open then also write message to log file. */
{
  if (noisy) {
    if (a && strlen(a)) {
#if defined(UNICODE_SUPPORT_WIN32) && !defined(ZIP_DLL_LIB)
      print_utf8(a);
#else
      zfprintf(mesg, "%s", a);
#endif
      mesg_line_started = 1;
    }
    if (nl) {
      if (mesg_line_started) {
        zfprintf(mesg, "\n");
        mesg_line_started = 0;
      }
    } else if (a && strlen(a)) {
      mesg_line_started = 1;
    }
    fflush(mesg);
  }
  if (logfile) {
    if (a && strlen(a)) {
      zfprintf(logfile, "%s", a);
      logfile_line_started = 1;
    }
    if (nl) {
      if (logfile_line_started) {
        zfprintf(logfile, "\n");
        logfile_line_started = 0;
      }
    } else if (a && strlen(a)) {
      logfile_line_started = 1;
    }
    fflush(logfile);
  }
}


void zipmessage(a, b)
ZCONST char *a, *b;     /* message strings juxtaposed in output */
/* Print a message to mesg and flush.  Also write to log file if
   open.  Write new line first if current line has output already. */
{
  /* As a and/or b may be using errbuf, provide zipmessage() with its
     own buffer. */
  char ebuf[ERRBUF_SIZE + 1];

  sprintf(ebuf, "%s%s\n", a, b);

  if (noisy) {
    if (mesg_line_started)
      zfprintf(mesg, "\n");
#if defined(UNICODE_SUPPORT_WIN32) && !defined(ZIP_DLL_LIB)
    print_utf8(ebuf);
#else
    zfprintf(mesg, "%s", ebuf);
#endif
    mesg_line_started = 0;
    fflush(mesg);
  }
  if (logfile) {
    if (logfile_line_started)
      zfprintf(logfile, "\n");
    zfprintf(logfile, "%s", ebuf);
    logfile_line_started = 0;
    fflush(logfile);
  }
}


/* Print a warning message to mesg (usually stderr) and return,
 * with or without indentation.
 */
void zipwarn_i(mesg_prefix, indent, a, b)
  char *mesg_prefix;      /* message prefix string */
  int indent;
  ZCONST char *a, *b;     /* message strings juxtaposed in output */
/* Print a warning message to mesg (usually stderr) and return. */
{
  char *prefix;
  char *warning;
  char ebuf[ERRBUF_SIZE + 1];

  if (indent)
    prefix = "      ";
  else
    prefix = "";

  if (a == NULL)
  {
    a = "";
    warning = "            ";
  }
  else
  {
    warning = mesg_prefix;
  }

  sprintf(ebuf, "%s%s %s%s\n", prefix, warning, a, b);

  if (mesg_line_started)
    zfprintf(mesg, "\n");
#if defined(UNICODE_SUPPORT_WIN32) && !defined(ZIP_DLL_LIB)
  print_utf8(ebuf);
# if 0
  zfprintf(mesg, "%s%s %s%s\n", prefix, warning, a, b);
# endif
#else
  zfprintf(mesg, "%s", ebuf);
#endif
  mesg_line_started = 0;
#ifndef WINDLL
  fflush(mesg);
#endif

  if (logfile) {
    if (logfile_line_started)
      zfprintf(logfile, "\n");
#if 0
    zfprintf(logfile, "%s%s %s%s\n", prefix, warning, a, b);
#endif
    zfprintf(logfile, "%s", ebuf);
    logfile_line_started = 0;
    fflush(logfile);
  }

#ifdef ZIP_DLL_LIB
  if (*lpZipUserFunctions->error != NULL) {
#if 0
    sprintf(buf, "%s%s %s%s\n", prefix, warning, a, b);
#endif
    (*lpZipUserFunctions->error)(ebuf);
  }
#endif /* ZIP_DLL_LIB */
}


/* For Windows with UNICODE_SUPPORT, outputs using
   write_console(), otherwise just print to mesg. */
void print_utf8(message)
  ZCONST char *message;
{
# if defined(UNICODE_SUPPORT_WIN32) && !defined(ZIP_DLL_LIB)
  int utf8;
  wchar_t *wide_message = NULL;

  if (unicode_show) {
    utf8 = is_utf8_string(message, NULL, NULL, NULL, NULL);
#if 0
    if (utf8) {
      write_console(mesg, message);
    }
    else {
      zfprintf(mesg, "%s", message);
      fflush(mesg);
    }
#else
    if (utf8)
      wide_message = utf8_to_wchar_string(message);
    else
      wide_message = local_to_wchar_string(message);
    write_consolew(mesg, wide_message);
#endif
  }
  else {
    zfprintf(mesg, "%s", message);
#  ifndef ZIP_DLL_LIB
    fflush(mesg);
#  endif
  }
# else /* not (UNICODE_SUPPORT_WIN32 && !ZIP_DLL_LIB) */
  zfprintf(mesg, "%s", message);
#  ifndef ZIP_DLL_LIB
  fflush(mesg);
#  endif
# endif
}

/* ------------------------------------------------------------- */






/* --------------------------------------------------- */
/* Unicode Support
 *
 * These functions common for all Unicode ports.
 *
 * These functions should allocate and return strings that can be
 * freed with free().
 *
 * 8/27/05 EG
 *
 * Use zwchar for wide char which is unsigned long
 * in zip.h and 32 bits.  This avoids problems with
 * different sizes of wchar_t.
 */

#if 0
/* These are here to help me sort through all this.  These
   should go away next beta. */
int is_ascii_stringw(wchar_t *wstring);
int is_ascii_string(char *mbstring);

zwchar *wchar_to_wide_string(wchar_t *wchar_string);

char *wchar_to_local_string(wchar_t *wstring);

char *wchar_to_utf8_string(wchar_t *wstring);

zwchar escape_string_to_wide(char *escape_string);

char *wide_char_to_escape_string(zwchar wide_char);
char *local_to_escape_string(char *local_string);
char *utf8_to_escape_string(char *utf8_string);
char *wide_to_escape_string(zwchar *wide_string);

char *local_to_display_string(char *local_string);

zwchar *local_to_wide_string(char *local_string);
char *wide_to_local_string(zwchar *wide_string);

char *local_to_utf8_string(char *local_string);
char *utf8_to_local_string(char *utf8_string);

char *wide_to_utf8_string(zwchar *wide_string);
zwchar *utf8_to_wide_string(char *utf8_string);
#endif



/* is_utf8_string - determine if valid UTF-8 in string
 *
 * instring - in - string to look at
 * has_bom - out - if not NULL, set to 1 if string starts with BOM, else 0
 * count - out - if not NULL, receives the total char count
 * ascii_count - out - if not NULL, receives the count of ASCII 7-bit chars found
 * utf8_count - out - if not NULL, receives the count of UTF-8 chars found
 *
 * Reads UTF-8 sequences until end of string.  Does not decode or
 * validate the actual characters.
 *
 * Skips over and ignores BOM if found.
 *
 * This is a string version of is_utf8_file().  See the notes there.
 *
 * Returns 1 if found UTF-8 and all valid, 0 otherwise.
 */
int is_utf8_string(ZCONST char *instring, int *has_bom, int *count, int *ascii_count, int *utf8_count)
{
  unsigned long char_count = 0;
  unsigned long ascii_char_count = 0;
  unsigned long utf8_char_count = 0;
  unsigned char uc;
  int bad_utf8 = 0;
  int index = 0;
  int      t, r;
  unsigned int lead;
  int is_utf8;
  int b1, b2, b3;
  int string_has_bom = 0;
  int len = (int)strlen(instring);

#if 0
  printf("len = %d\n", len);
  for (t = 0; instring[t]; t++) {
    uc = (unsigned char)instring[t];
    printf(" %2x", uc);
  }
  printf("\n");
#endif

  if (has_bom)
    *has_bom = 0;
  if (count)
    *count = 0;
  if (ascii_count)
    *ascii_count = 0;
  if (utf8_count)
    *utf8_count = 0;

  /* check for UTF-8 BOM */
  b1 = instring[index++];
  if (b1 == 0xef) {
    b2 = instring[index++];
    if (b2 == 0xbb) {
      b3 = instring[index++];
      if (b3 == 0xbf) {
        string_has_bom = 1;
      }
    }
  }

  if (has_bom)
    *has_bom = string_has_bom;

  if (!string_has_bom)
    index = 0;

  /* Run through bytes, looking for good and bad UTF-8 characters. */
  for (; instring[index];)
  {
    uc = (unsigned char)instring[index++];

    lead = uc;
    if (lead < 0x80) {
      r = 1;              /* an ascii-7 character */
      char_count++;
      ascii_char_count++;
      continue;
    }
    else if (lead < 0xC0) {
      bad_utf8 = 1;       /* error: trailing byte without lead byte */
#if 0
      printf("no lead\n");
#endif
      break;
    }
    else if (lead < 0xE0)
      r = 2;              /* an 11 bit character */
    else if (lead < 0xF0)
      r = 3;              /* a 16 bit character */
    else if (lead < 0xF8)
      r = 4;              /* a 21 bit character (the most currently used) */
    else if (lead < 0xFC)
      r = 5;              /* a 26 bit character (shouldn't happen) */
    else if (lead < 0xFE)
      r = 6;              /* a 31 bit character (shouldn't happen) */
    else {
      bad_utf8 = 1;       /* error: invalid lead byte */
      break;
    }
    for (t = 1; t < r; t++) {
      if (index == len) {
        bad_utf8 = 1;     /* incomplete UTF-8 character */
        break;
      }
      uc = (unsigned char)instring[index++];

      if (uc < 0x80 || uc >= 0xC0) {
        bad_utf8 = 1;     /* error: not enough valid trailing bytes */
#if 0
        printf("short trail\n");
#endif
        break;
      }
    } /* for */
    if (bad_utf8)
      break;
    char_count++;
    utf8_char_count++;
  } /* for */

  if (bad_utf8)
    is_utf8 = 0;
  else {
    if (utf8_char_count)
      is_utf8 = 1;
    else
      is_utf8 = 0;
  }
#if 0
  printf("\nis_utf8_string:  instring:  %s\n", instring);
  printf("is_utf8_string:  has bom %d  count %d  ascii7 %d  utf8 %d  = is UTF-8 %d\n",
         string_has_bom, char_count, ascii_char_count, utf8_char_count, is_utf8);
#endif

  if (count)
    *count = char_count;
  if (ascii_count)
    *ascii_count = ascii_char_count;
  if (utf8_count)
    *utf8_count = utf8_char_count;

  return is_utf8;
}


#ifdef UNICODE_SUPPORT

/* is_utf8_file - determine if contents valid UTF-8
 *
 * infile - in - open file pointer
 * has_bom - out - if not NULL, set to 1 if string starts with BOM, else 0
 * count - out - if not NULL, receives the total char count
 * ascii_count - out - if not NULL, receives the count of ASCII 7-bit chars found
 * utf8_count - out - if not NULL, receives the count of UTF-8 chars found
 *
 * Reads UTF-8 sequences until end of file.  Exits if a bad UTF-8 char
 * is found.  Does not decode or validate the actual characters.  Also
 * checks for UTF-8 BOM and sets file pointer after it.
 *
 * Assumes file is seekable.
 *
 * The returned counts are only up to the first bad UTF-8 character, if there
 * is one.  Otherwise, if file size less than MAX_INCLUDE_FILE_SIZE, are for
 * the entire file.
 *
 * On return, file pointer will either be at beginning or after BOM if present.
 *
 * Max size of file is MAX_INCLUDE_FILE_SIZE.  Exits when that count reached.
 *
 * This function mainly based on Paul's utf8_char_bytes().
 *
 * Returns 1 if found UTF-8 and all valid, 0 otherwise.  Note that if
 * is_utf8_file() returns 0 but utf8_count > 0, then there's UTF-8, but
 * at least one bad char.  It seems other utilities require 100% good
 * UTF-8 to open as UTF-8, so we do too.
 *
 * ------
 *
 * Note that, as Zip has further evolved, this function is no longer used.
 * As Zip on WIN32 now checks each arg and path separately using
 * is_utf8_string(), it's now not that relevant if a file has a mix of
 * ASCII 7-bit, local MBCS, and UTF-8.  However, each argument or path
 * should be just one of these.  If a string has UTF-8 and no bad UTF-8,
 * then it's handled as UTF-8, even if there's local MBCS in it.
 * Otherwise it's handled as local, which can be a mix of ASCII 7-bit and
 * MBCS.
 *
 * On non-Windows platforms, only local UTF-8 is handled.  Currently only
 * WIN32 distinguishes between UTF-8 and a local charset.
 */
int is_utf8_file(FILE *infile, int *has_bom, int *count, int *ascii_count, int *utf8_count)
{
  int has_utf8_bom = 0;
  unsigned long char_count = 0;
  unsigned long ascii_char_count = 0;
  unsigned long utf8_char_count = 0;
  int i;
  unsigned int uc;
  int bad_utf8 = 0;
  int      t, r;
  unsigned int lead;
  int is_utf8;


  if (has_bom)
    *has_bom = 0;
  if (count)
    *count = 0;
  if (ascii_count)
    *ascii_count = 0;
  if (utf8_count)
    *utf8_count = 0;

  /* make sure we're at top of file */
  rewind(infile);

  has_utf8_bom = read_utf8_bom(infile);

#if 0
  /* If we trust the BOM, we can report the file as UTF-8 without going
     further.  But we don't.  We look for ourselves to make sure that
     there is at least one UTF-8 char and no bad UTF-8 chars. */
  if (has_bom) {
    return 1;
  }
#endif

  /* Run through bytes, looking for good and bad UTF-8 characters. */
  for (;;)
  {
    i = fgetc(infile);
    if (i == EOF) {
      break;
    }
    uc = (unsigned int)i;

    char_count++;
    if (char_count >= (MAX_INCLUDE_FILE_SIZE)) {
#if 0
      printf("max count\n");
#endif
      break;
    }

    lead = uc;
    if (lead < 0x80) {
      r = 1;              /* an ascii-7 character */
      ascii_char_count++;
      continue;
    }
    else if (lead < 0xC0) {
      bad_utf8 = 1;       /* error: trailing byte without lead byte */
#if 0
      printf("no lead\n");
#endif
      break;
    }
    else if (lead < 0xE0)
      r = 2;              /* an 11 bit character */
    else if (lead < 0xF0)
      r = 3;              /* a 16 bit character */
    else if (lead < 0xF8)
      r = 4;              /* a 21 bit character (the most currently used) */
    else if (lead < 0xFC)
      r = 5;              /* a 26 bit character (shouldn't happen) */
    else if (lead < 0xFE)
      r = 6;              /* a 31 bit character (shouldn't happen) */
    else {
      bad_utf8 = 1;       /* error: invalid lead byte */
      break;
    }
    for (t = 1; t < r; t++) {
      i = fgetc(infile);
      if (i == EOF) {
        bad_utf8 = 1;     /* partial character */
        break;
      }
      uc = (unsigned int)i;

      char_count++;
      if (char_count >= (MAX_INCLUDE_FILE_SIZE)) {
        break;
      }

      if (uc < 0x80 || uc >= 0xC0) {
        bad_utf8 = 1;     /* error: not enough valid trailing bytes */
#if 0
        printf("short trail\n");
#endif
        break;
      }
    } /* for */
    if (bad_utf8)
      break;
    utf8_char_count++;
  } /* for */

  /* go back to top and reread BOM if exists, leaving file pointer just after it */
  rewind(infile);
  read_utf8_bom(infile);

  if (bad_utf8)
    is_utf8 = 0;
  else {
    if (utf8_char_count)
      is_utf8 = 1;
    else
      is_utf8 = 0;
  }

#if 0
  printf("is_utf8_file:  has bom %d  count %d  ascii7 %d  utf8 %d  = is UTF-8 %d\n",
         has_utf8_bom, char_count, ascii_char_count, utf8_char_count, is_utf8);
#endif

  if (has_bom)
    *has_bom = has_utf8_bom;
  if (count)
    *count = char_count;
  if (ascii_count)
    *ascii_count = ascii_char_count;
  if (utf8_count)
    *utf8_count = utf8_char_count;

  return is_utf8;
}


/* utf8_to_wchar_string
 *
 * Convert UTF-8 string to wchar_t string.
 *
 * Returns the allocated wchar_t string.
 */
wchar_t *utf8_to_wchar_string(ZCONST char *utf8_string)
{
#ifdef WIN32

  return utf8_to_wchar_string_windows(utf8_string);

#else /* not WIN32 */

  if (sizeof(wchar_t) == 4)
  {
    /* Using Zip's conversion functions - does not handle surrogates. */
    /* This version works if wchar_t is 4 bytes (no surrogates). */
    zwchar *wide_string;
    wchar_t *wchar_string;

    if (utf8_string == NULL)
      return NULL;

    wide_string = utf8_to_wide_string(utf8_string);
    if (wide_string == NULL)
      return NULL;
    
    wchar_string = wide_to_wchar_string(wide_string);

    free(wide_string);

    return wchar_string;
  }
  else

# ifdef HAVE_ICONV
  {
    iconv_t  cd;     /* conversion descriptor */
    size_t   iconv_result;
    int      close_result;

    size_t inbytesleft;
    size_t utf8_string_len;

    wchar_t *outbuf;
    char *coutbuf;
    size_t outbuf_size;
    size_t outbytesleft;

    char *fromcode = "UTF-8";
    char *tocode   = "WCHAR_T";

    char *inp;
    char *outp;

    utf8_string_len = strlen(utf8_string);

    /* assume wchar string will take no more than 4x input string */
    outbuf_size = (4 * utf8_string_len + 5) * sizeof(wchar_t);

    if ((outbuf = (wchar_t *)malloc(outbuf_size)) == NULL) {
      ZIPERR(ZE_MEM, "utf8_to_wchar_string");
    }

    coutbuf = (char *)outbuf;
    strcpy(coutbuf, "z\0y\0\0\0");

    inp = string_dup(utf8_string, "utf8_to_wchar_string", 0);
    outp = coutbuf;

    inbytesleft = utf8_string_len;
    outbytesleft = outbuf_size;

    cd = iconv_open(tocode, fromcode);
    if (cd == (iconv_t)-1) {
      zperror("iconv_open");
      free(outbuf);
      ZIPERR(ZE_LOGIC, "error opening iconv");
    }

    iconv_result = iconv(cd, &inp, &inbytesleft, &outp, &outbytesleft);

    close_result = iconv_close(cd);

    free(inp);

    if (iconv_result == (size_t)-1) {
      zperror("iconv");
      free(outbuf);
      return NULL;
    }

    if (close_result == -1) {
      zperror("iconv_close");
      free(outbuf);
      ZIPERR(ZE_LOGIC, "error closing iconv");
    }

    return outbuf;
  }

#  else /* not HAVE_ICONV */

  ZIPERR(ZE_COMPILE, "utf8_to_wchar_string has no appropriate implementation");

#  endif

#  if 0
  /* This version only works if the locale is UTF-8. */

  size_t len;
  wchar_t *w_string;

  if (!using_utf8) {
    ZIPERR(ZE_COMPILE, "character set not UTF-8");
  }

  /* get size needed */
  len = mbstowcs(NULL, utf8_string, MAX_PATH_SIZE);

  if ((w_string = (wchar_t *)malloc((len + 2) * sizeof(wchar_t))) == NULL) {
    ZIPERR(ZE_MEM, "utf8_to_wchar_string");
  }

  len = mbstowcs(w_string, utf8_string, MAX_PATH_SIZE);

  return w_string;
#  endif

#endif /* not WIN32 */
}


/* wchar_to_utf8_string
 *
 * Convert wchar_t string to UTF-8 string.
 *
 * Return allocated string.
 */
char *wchar_to_utf8_string(wchar_t *wchar_string)
{
#ifdef WIN32

  return wchar_to_utf8_string_windows(wchar_string);

#else /* not WIN32 */

  if (sizeof(wchar_t) == 42)
  {
    /* This works if don't use surrogate pairs (wchar_t is 4 bytes) */
  
    zwchar *wide_string = wchar_to_wide_string(wchar_string);
    char *local_string = wide_to_utf8_string(wide_string);

    free(wide_string);

    return local_string;
  }
  else

# ifdef HAVE_ICONV
  {
    iconv_t  cd;     /* conversion descriptor */
    size_t   iconv_result;
    int      close_result;

    size_t wchar_string_len;
    size_t wchar_byte_len;

    char *outbuf;
    size_t outbuf_size;

    size_t inbytesleft;
    size_t outbytesleft;

    char *fromcode = "WCHAR_T";
    char *tocode   = "UTF-8";

    char *inp;
    char *outp;

    size_t outlen;

    wchar_string_len = wcslen(wchar_string);
    wchar_byte_len = wchar_string_len * sizeof(wchar_t);

    /* assume output string will take no more than 6x input string */
    outbuf_size = (6 * wchar_string_len + 40) * sizeof(wchar_t);

    if ((outbuf = (char *)malloc(outbuf_size)) == NULL) {
      ZIPERR(ZE_MEM, "wchar_to_utf8_string");
    }

    /* This is a debugging aid. */
    strcpy(outbuf, "1234567890123456789012345678901234567890\0");

    inp = (char *)wchar_string;
    outp = (char *)outbuf;

    inbytesleft = wchar_byte_len;
    outbytesleft = outbuf_size;

    cd = iconv_open(tocode, fromcode);
    if (cd == (iconv_t)-1) {
      zperror("iconv_open");
      free(outbuf);
      ZIPERR(ZE_LOGIC, "error opening iconv");
    }

#if 0
    printf("in:  '%ls'\n", inp);
    printf("      1234567890123456789012345678901234567890\n");
#endif

#if 0
    printf("  iconv  inleft %d  outleft %d\n", inbytesleft, outbytesleft);
#endif

    iconv_result = iconv(cd, &inp, &inbytesleft, &outp, &outbytesleft);

#if 0
    printf("  iconv  inleft %d  outleft %d\n", inbytesleft, outbytesleft);
#endif

    close_result = iconv_close(cd);

    if (iconv_result == (size_t)-1) {
      zperror("iconv");
      free(outbuf);
      return NULL;
    }

    if (close_result == -1) {
      zperror("iconv_close");
      free(outbuf);
      ZIPERR(ZE_LOGIC, "error closing iconv");
    }

    outlen = outbuf_size - outbytesleft;
    outbuf[outlen] = '\0';

    return outbuf;
  }

# else /* not HAVE_ICONV */

  ZIPERR(ZE_COMPILE, "wchar_to_utf8_string has no appropriate implementation");

# endif

# if 0
  /* This only works if the locale is UTF-8. */
  size_t len;
  char *utf8_string;

  if (!using_utf8) {
    ZIPERR(ZE_COMPILE, "character set not UTF-8");
  }

  /* get size needed */
  len = wcstombs(NULL, wchar_string, MAX_PATH_SIZE);

  if ((utf8_string = (char *)malloc((len + 2) * sizeof(char))) == NULL) {
    ZIPERR(ZE_MEM, "wchar_to_utf8_string");
  }

  len = wcstombs(utf8_string, wchar_string, MAX_PATH_SIZE);

  return utf8_string;
# endif

#endif /* not WIN32 */
}


/* wchar_to_wide_string
 *
 * Convert wchar_t string to zwchar (4 byte) string.
 *
 * This version does not account for multi-word chars (surrogate pairs).
 * It should only be used when wchar_t is 4 bytes (unless someone wants
 * to add surrogate support).
 *
 * Returns allocated zwchar string.
 */
zwchar *wchar_to_wide_string(wchar_t *wchar_string)
{
  int i;
  int wchar_len;
  zwchar *wide_string;

  wchar_len = (int)wcslen(wchar_string);

  if ((wide_string = (zwchar *)malloc((wchar_len + 1) * sizeof(zwchar))) == NULL) {
    ZIPERR(ZE_MEM, "wchar_to_wide_string");
  }
  for (i = 0; i <= wchar_len; i++) {
    wide_string[i] = wchar_string[i];
  }

  return wide_string;
}


/* wide_to_wchar_string
 *
 * Convert zwchar (4 byte) string to wchar_t string.
 *
 * This version does not account for multi-word chars (surrogate pairs).
 * It should only be used when wchar_t is 4 bytes or it is known no
 * surrogate pairs are needed (unless someone wants to add surrogate support).
 *
 * Returns allocated wchar_t string.
 */
wchar_t *wide_to_wchar_string(zwchar *wide_string)
{
  int i;
  int wide_len;
  wchar_t *wchar_string;

  wide_len = zwchar_string_len(wide_string);

  if ((wchar_string = (wchar_t *)malloc((wide_len + 1) * sizeof(wchar_t))) == NULL) {
    ZIPERR(ZE_MEM, "wide_to_wchar_string");
  }
  /* this is not UTF-16 aware - should be fixed */
  for (i = 0; i <= wide_len; i++) {
    wchar_string[i] = (wchar_t)wide_string[i];
  }

  return wchar_string;
}


/* is_ascii_stringw
 *
 * Returns true if wchar_t string is all 7-bit ASCII.
 */
int is_ascii_stringw(wchar_t *wchar_string)
{
  wchar_t *pw;
  wchar_t cw;

  if (wchar_string == NULL)
    return 0;

  for (pw = wchar_string; (cw = *pw) != '\0'; pw++) {
    if (cw > 0x7F) {
      return 0;
    }
  }
  return 1;
}


/* is_ascii_string
 *
 * Returns true if string is all 7-bit ASCII.
 */
int is_ascii_string(char *mbstring)
{
  char *p;
  uch c;

  if (mbstring == NULL)
    return 0;

  for (p = mbstring; (c = (uch)*p) != '\0'; p++) {
    if (c > 0x7F) {
      return 0;
    }
  }
  return 1;
}


/* local to UTF-8
 *
 * Convert local character set string to UTF-8 string.
 *
 * This version should only be used if wchar_t is 4 bytes
 * (no surrogate pairs).
 *
 * Returns allocated UTF-8 string.
 */
char *local_to_utf8_string(char *local_string)
{
  zwchar *wide_string;
  char *utf8_string;

  if (local_string == NULL)
    return NULL;

  wide_string = local_to_wide_string(local_string);
  if (wide_string == NULL)
    return NULL;

  utf8_string = wide_to_utf8_string(wide_string);

  free(wide_string);
  return utf8_string;
}


/* wide_char_to_escape_string

   Provides a string that represents a wide char not in local char set.

   An initial try at an algorithm.  Suggestions welcome.

   If not an ASCII char, probably need 2 bytes at least.  So if
   a 2-byte wide encode it as 4 hex digits with a leading #U.
   Since the Unicode standard has been frozen, it looks like 3 bytes
   should be enough for any large Unicode character.  In these cases
   prefix the string with #L.
   So
   #U1234
   is a 2-byte wide character with bytes 0x12 and 0x34 while
   #L123456
   is a 3-byte wide with bytes 0x12, 0x34, and 0x56.
   On Windows, wide that need two wide characters as a surrogate pair
   to represent them need to be converted to a single number.
  */

 /* set this to the max bytes an escape can be */
#define MAX_ESCAPE_BYTES 8

char *wide_char_to_escape_string(zwchar wide_char)
{
  int i;
  zwchar w = wide_char;
  uch b[9];
  char e[7];
  int len;
  char *r;

  /* fill byte array with zeros */
  for (len = 0; len < sizeof(zwchar); len++) {
    b[len] = 0;
  }
  /* get bytes in right to left order */
  for (len = 0; w; len++) {
    b[len] = (char)(w % 0x100);
    w /= 0x100;
  }

  if ((r = malloc(MAX_ESCAPE_BYTES + 8)) == NULL) {
    ZIPERR(ZE_MEM, "wide_char_to_escape_string");
  }
  strcpy(r, "#");
  /* either 2 bytes or 4 bytes */
  if (len < 3) {
    len = 2;
    strcat(r, "U");
  } else {
    len = 3;
    strcat(r, "L");
  }
  for (i = len - 1; i >= 0; i--) {
    sprintf(e, "%02x", b[i]);
    strcat(r, e);
  }
  return r;
}


/* char_to_wide_string - convert char string to wide string with no translation
 *
 * This only works for ASCII 7-bit.
 */
zwchar *char_to_wide_string(char *char_string)
{
  int i;
  int len;
  zwchar *wide_string;

  len = (int)strlen(char_string) + 1;

  /* allocate space for wide string */
  if ((wide_string = (zwchar *)malloc(len * sizeof(zwchar))) == NULL) {
    ZIPERR(ZE_MEM, "char_to_wide_string");
  }

  /* just expand each char */
  for (i = 0; char_string[i]; i++) {
    wide_string[i] = (zwchar)((unsigned char)char_string[i]);
  }
  wide_string[i] = (zwchar)'\0';

  return wide_string;
}


/* returns the wide character represented by the escape string */
zwchar escape_to_wide_char(char *escape_string)
{
  int i;
  zwchar w;
  char c;
  char u;
  int len;
  char *e = escape_string;

  if (e == NULL) {
    return 0;
  }
  if (e[0] != '#') {
    /* no leading # */
    return 0;
  }
  len = (int)strlen(e);
  /* either #U1234 or #L123456 format */
  if (len != 6 && len != 8) {
    return 0;
  }
  w = 0;
  if (e[1] == 'L') {
    if (len != 8) {
      return 0;
    }
    /* 3 bytes */
    for (i = 2; i < 8; i++) {
      c = e[i];
      u = toupper(c);
      if (u >= 'A' && u <= 'F') {
        w = w * 0x10 + (zwchar)(u + 10 - 'A');
      } else if (c >= '0' && c <= '9') {
        w = w * 0x10 + (zwchar)(c - '0');
      } else {
        return 0;
      }
    }
  } else if (e[1] == 'U') {
    /* 2 bytes */
    for (i = 2; i < 6; i++) {
      c = e[i];
      u = toupper(c);
      if (u >= 'A' && u <= 'F') {
        w = w * 0x10 + (zwchar)(u + 10 - 'A');
      } else if (c >= '0' && c <= '9') {
        w = w * 0x10 + (zwchar)(c - '0');
      } else {
        return 0;
      }
    }
  }
  return w;
}


/* escapes_to_wide_string - converts Unicode escapes in wide string to wide chars
 *
 * Returns a malloc'd string where any Unicode escapes found (#Uxxxx or #Lxxxxxx)
 * are replaced by the wide character they represent.  Bad escapes are just
 * copied as is to the output string.
 */
zwchar *escapes_to_wide_string(zwchar *escapes_string)
{
  int i;
  zwchar *e = escapes_string;
  zwchar *ep;
  zwchar *ws;
  int e_len;
  int iw;
  zwchar ec[10];
  int ec_len = 0;

  int ie;
  zwchar char_code;
  long digit;
  zwchar *wide_string;
  int ws_len;

  if (e == NULL) {
    return NULL;
  }
  ep = e;

  /* allocate string big enough */
  if ((ws = (zwchar *)malloc((zwchar_string_len(e) + 1) * sizeof(zwchar))) == NULL) {
    ZIPERR(ZE_MEM, "escapes_to_wide_string");
  }
  iw = 0;
  ws[iw] = (zwchar)'\0';

  while (*e) {
    /* look for leading # */
    if (*e != (zwchar)'#') {
      /* just copy char */
      ws[iw++] = (zwchar)*e;
      ws[iw] = (zwchar)'\0';
      e++;
      continue;
    }
    e++;

    /* look for U or L */
    if (*e != (zwchar)'U' && *e != (zwchar)'L') {
      /* just copy char */
      ws[iw++] = (zwchar)*e;
      ws[iw] = (zwchar)'\0';
      e++;
      continue;
    }

    /* see if have enough chars left */
    e_len = zwchar_string_len(e);
    if ((*e == (zwchar)'U' && e_len < 5) ||
        (*e == (zwchar)'L' && e_len < 7)) {
      /* just copy U or L */
      ws[iw++] = (zwchar)*e;
      ws[iw] = (zwchar)'\0';
      e++;
      continue;
    }

    /* get potential escape string */
    ie = 0;
    ec[ie] = '\0';
    if (*e == (zwchar)'U')
      ec_len = 4;
    else
      ec_len = 6;
    e++;
    /* copy hex digits that are Unicode character code */
    for (i = 0; i < ec_len; i++) {
      if (!((*e >= (zwchar)'0' && *e <= (zwchar)'9') ||
            (*e >= (zwchar)'a' && *e <= (zwchar)'f') ||
            (*e >= (zwchar)'A' && *e <= (zwchar)'F'))) {
        /* illegal hex digit - just copy # and skip */
        ws[iw++] = (zwchar)*ep;
        ws[iw] = L'\0';
        e++;
        continue;
      }
      ec[ie++] = (char)*e;
      e++;
    } /* for */

    /* get character code */
    char_code = 0;
    for (i = 0; i < ec_len; i++) {
      if (ec[i] >= (zwchar)'0' && ec[i] <= (zwchar)'9')
        digit = ec[i] - (zwchar)'0';
      else if (ec[i] >= (zwchar)'a' && ec[i] <= (zwchar)'f')
        digit = ec[i] - (zwchar)'a' + 10;
      else
        digit = ec[i] - (zwchar)'A' + 10;
      char_code = char_code * 16 + digit;
    }

    /* replace Unicode escape with char code */
    ws[iw++] = char_code;
  } /* while */

  ws[iw] = (zwchar)'\0';

  ws_len = zwchar_string_len(ws);

  /* allocate wide string */
  if ((wide_string = (zwchar *)malloc((ws_len + 1) * sizeof(zwchar))) == NULL) {
    ZIPERR(ZE_MEM, "escapes_to_wide_string");
  }

  /* copy result wide string */
  for (i = 0; ws[i]; i++)
    wide_string[i] = ws[i];
  wide_string[i] = (zwchar)'\0';

  free(ws);

  return wide_string;
}


/* escapes_to_wchar_string - replace Unicode escapes in wchar string with char
 *
 * UTF-16 is not handled, but Unicode escapes only use ASCII 7-bit and non-escape
 * chars are passed through and converted back as is.
 */
wchar_t *escapes_to_wchar_string(wchar_t *wchar_escaped_string){
  zwchar *escaped_wide_string;
  zwchar *wide_string;
  wchar_t *wchar_string = NULL;

  escaped_wide_string = wchar_to_wide_string(wchar_escaped_string);
  wide_string = escapes_to_wide_string(escaped_wide_string);
  free(escaped_wide_string);
  if (wide_string) {
    wchar_string = wide_to_wchar_string(wide_string);
    free(wide_string);
  }
  return wchar_string;
}


/* escapes_to_utf8_string - convert Unicode escapes in (possibly UTF-8) string to UTF-8
 *
 * Returns malloc'd string where any valid Unicode escapes (#Uxxxx or #Lxxxxxx)
 * are replaced by the represented UTF-8 bytes.
 */
char *escapes_to_utf8_string(char *escaped_string)
{
  zwchar *escaped_wide_string;
  zwchar *wide_string;
  char *utf8_string = NULL;

  escaped_wide_string = utf8_to_wide_string(escaped_string);

  wide_string = escapes_to_wide_string(escaped_wide_string);

  free(escaped_wide_string);
  if (wide_string) {
    utf8_string = wide_to_utf8_string(wide_string);
    free(wide_string);
  }

  return utf8_string;
}


/* zwchar_string_len
 *
 * Returns length of wide zwchar (UCS4) string.  Assumes
 * string is NULL terminated.
 */
int zwchar_string_len(zwchar *wide_string)
{
  int i;

  if (wide_string == NULL)
    return 0;

  for (i = 0; wide_string[i]; i++) ;

  return i;
}


char *local_to_escape_string(char *local_string)
{
  zwchar *wide_string;
  char *escape_string;

  if (local_string == NULL)
    return NULL;

  wide_string = local_to_wide_string(local_string);
  if (wide_string == NULL)
    return NULL;
  
  escape_string = wide_to_escape_string(wide_string);

  free(wide_string);

  return escape_string;
}


/* wchar_to_local_string
 *
 * Convert wchar_t string to local character set string.
 *
 * Should only be used if wchar_t is 4 bytes.
 *
 * Return allocated local string.
 */
char *wchar_to_local_string(wchar_t *wstring)
{
  zwchar *wide_string;
  char *local_string;

  if (wstring == NULL)
    return NULL;

  wide_string = wchar_to_wide_string(wstring);
  if (wide_string == NULL)
    return NULL;
  
  local_string = wide_to_local_string(wide_string);

  free(wide_string);

  return local_string;
}


#ifndef WIN32   /* The Win32 port uses a system-specific variant. */

/* wide_to_local_string - convert wide character string to multi-byte string
 *
 * This has been updated to not TRANSLIT chars, so if a character does
 * not have a counterpart in the local charset, a Unicode escape is used.
 * The results of allowing char substitution as from TRANSLIT were marginal
 * at best.
 */
char *wide_to_local_string(zwchar *wide_string)
{
# ifdef UNICODE_WCHAR

  int i;
  wchar_t wc;
  int b;
  int state_dependent;
  int wsize = 0;
  int max_bytes = MB_CUR_MAX;
  char buf[9];
  char *buffer = NULL;
  char *local_string = NULL;

  if (wide_string == NULL)
    return NULL;

  for (wsize = 0; wide_string[wsize]; wsize++) ;

  if (MAX_ESCAPE_BYTES > max_bytes)
    max_bytes = MAX_ESCAPE_BYTES;

  if ((buffer = (char *)malloc(wsize * max_bytes + 1)) == NULL) {
    ZIPERR(ZE_MEM, "wide_to_local_string");
  }

  /* convert it */
  buffer[0] = '\0';
  /* set initial state if state-dependent encoding */
  wc = (wchar_t)'a';
  b = wctomb(NULL, wc);
  if (b == 0)
    state_dependent = 0;
  else
    state_dependent = 1;
  for (i = 0; i < wsize; i++) {
    if (sizeof(wchar_t) < 4 && wide_string[i] > 0xFFFF) {
      /* wchar_t probably 2 bytes */
      /* could do surrogates if state_dependent and wctomb can do */
      wc = zwchar_to_wchar_t_default_char;
    } else {
      wc = (wchar_t)wide_string[i];
    }
    b = wctomb(buf, wc);
    if (unicode_escape_all) {
      if (b == 1 && (uch)buf[0] <= 0x7f) {
        /* ASCII */
        strncat(buffer, buf, b);
      } else {
        /* use escape for wide character */
        char *e = wide_char_to_escape_string(wide_string[i]);
        strcat(buffer, e);
        free(e);
      }
    } else if (b > 0) {
      /* multi-byte char */
      strncat(buffer, buf, b);
    } else {
      /* no MB for this wide */
      if (use_wide_to_mb_default) {
        /* default character */
        strcat(buffer, wide_to_mb_default_string);
      } else {
        /* use escape for wide character */
        char *e = wide_char_to_escape_string(wide_string[i]);
        strcat(buffer, e);
        free(e);
      }
    }
  }
  if ((local_string = (char *)malloc(strlen(buffer) + 1)) == NULL) {
    free(buffer);
    ZIPERR(ZE_MEM, "wide_to_local_string");
  }
  strcpy(local_string, buffer);
  free(buffer);

  return local_string;

# else /* not UNICODE_WCHAR */

#  ifdef HAVE_ICONV

  /* iconv per character version */

  iconv_t  cd;     /* conversion descriptor */
  size_t   iconv_result;
  int      close_result;

  size_t wide_string_len;
  size_t wide_byte_len;

  char *outbuf;
  size_t outbuf_size;

  size_t inbytesleft;
  size_t outbytesleft;

  char *fromcode = "UCS-4LE";    /* should be equivalent to zwchar */
  char *tocode   = charsetname;

  char *inp;
  char *outp; 

  int i;
  int k;
  char *escape_string;
  zwchar wc;


  wide_string_len = zwchar_string_len(wide_string);
  wide_byte_len = wide_string_len * sizeof(zwchar);

  /* assume local string will take no more than 8x input string */
  outbuf_size = (8 * wide_string_len + 40) * sizeof(char);

  if ((outbuf = (char *)malloc(outbuf_size)) == NULL) {
    ZIPERR(ZE_MEM, "wide_to_local_string");
  }

  strcpy(outbuf, "1234567890123456789012345678901234567890");

  inp = (char *)wide_string;
  outp = outbuf;

#if 0
  zprintf("in:  '%ls'\n", wide_string);
  zprintf("in:  '1234567890123456789012345678901234567890'\n");
#endif

  inbytesleft = wide_byte_len;
  outbytesleft = outbuf_size;

  cd = iconv_open(tocode, fromcode);
  if (cd == (iconv_t)-1) {
    zperror("iconv_open");
    free(outbuf);
    ZIPERR(ZE_LOGIC, "error opening iconv");
  }

#if 0
  zprintf("  iconv  inleft %d  outleft %d\n", inbytesleft, outbytesleft);
#endif

  for (i = 0; i < wide_string_len; i++) {
#if 0
    wc = wide_string[i];
    zprintf("\n  i: %d  wsl: %d  wc: %lx\n", i, wide_string_len, wc);
#endif
    inbytesleft = 4;
    iconv_result = iconv(cd, &inp, &inbytesleft, &outp, &outbytesleft);
    if (iconv_result == (size_t)-1) {
      if (errno == EILSEQ) {
        wc = wide_string[i];
        escape_string = wide_char_to_escape_string(wc);
#if 0
        zprintf("\nBad char - escape: %lx '%s'\n", wc, escape_string);
#endif
        /* escapes are no more then 8 bytes (#L123456) */
        /* write bytes to output */
        strcpy(outp, escape_string);
        /* update pointer */
        outp += strlen(escape_string);
      }
      else
      {
        zperror("iconv");
      }
    }
  }

#if 0
  zprintf("  iconv  inleft %d  outleft %d\n", inbytesleft, outbytesleft);
#endif

  close_result = iconv_close(cd);

  if (close_result == -1) {
    zperror("iconv_close");
    free(outbuf);
    ZIPERR(ZE_LOGIC, "error closing iconv");
  }

  return outbuf;

#  else /* not HAVE_ICONV */

  ZIPERR(ZE_COMPILE, "wide_to_local_string has no appropriate implementation");

#  endif /* not HAVE_ICONV */

# endif /* not UNICODE_WCHAR */

}
#endif /* not WIN32 */


/* convert wide character string to escaped string */
char *wide_to_escape_string(zwchar *wide_string)
{
  int i;
  int wsize = 0;
  char buf[9];
  char *buffer = NULL;
  char *escape_string = NULL;

  if (wide_string == NULL)
    return NULL;

  for (wsize = 0; wide_string[wsize]; wsize++) ;

  if ((buffer = (char *)malloc(wsize * MAX_ESCAPE_BYTES + 1)) == NULL) {
    ZIPERR(ZE_MEM, "wide_to_escape_string");
  }

  /* convert it */
  buffer[0] = '\0';

  for (i = 0; i < wsize; i++) {
    if (wide_string[i] <= 0x7f && isprint((char)wide_string[i])) {
      /* ASCII */
      buf[0] = (char)wide_string[i];
      buf[1] = '\0';
      strcat(buffer, buf);
    } else {
      /* use escape for wide character */
      char *e = wide_char_to_escape_string(wide_string[i]);
      strcat(buffer, e);
      free(e);
    }
  }
  if ((escape_string = (char *)malloc(strlen(buffer) + 1)) == NULL) {
    ZIPERR(ZE_MEM, "wide_to_escape_string");
  }
  strcpy(escape_string, buffer);
  free(buffer);

  return escape_string;
}

#endif /* UNICODE_SUPPORT */



/* local_to_display_string
 *
 * Convert local string to display character set string.
 * This is the string assigned to z->oname (output name)
 * that gets sent to the display (console, stdout) when
 * needed (as in progress and error messages).
 */
#ifndef NO_PROTO
char *local_to_display_string(char *local_string)
#else
char *local_to_display_string(local_string)
  char *local_string;
#endif
{
  char *temp_string;
  char *display_string;

  /* For Windows, OEM string should never be bigger than ANSI string, says
     CharToOem description.

     On UNIX, non-printable characters (0x00 - 0xFF) will be replaced by
     "^x", so more space may be needed.  Note that "^" itself is a valid
     name character, so this leaves an ambiguity, but UnZip displays
     names this way, too.  (0x00 is not possible, I hope.)

     For all other ports, just make a copy of local_string.  Those ports
     should put reasonable code here as applicable.
  
     Note that if the current environment supports Unicode display and
     Unicode display is used, this code is not called.  It is assumed
     the port Unicode display routines will display the right characters.
     For instance, if Zip on Windows is asked to display Unicode directly,
     no OEM conversions are needed and Unicode is sent to the console.

     Note that, where applicable, Unicode escapes get substituted for
     non-printable chars, such as control codes.

     Cases such as ^x still need to be worked.   
   */

#ifdef UNIX
  char *cp_dst;                 /* Character pointers used in the */
  char *cp_src;                 /*  copying/changing procedure.   */
#endif

  if ((temp_string = (char *)malloc(2 * strlen(local_string) + 1)) == NULL) {
    ZIPERR(ZE_MEM, "local_to_display_string");
  }

#ifdef WIN32
  /* convert to OEM display character set */
  local_to_oem_string(temp_string, local_string);
#else
/*
  RBW  --  2009/06/25  --  FIX ME!
  Maybe we should do this for EBCDIC, but this code is definitely ASCII
  oriented.  We need to re-think this.
  Some casting is required, or EBCDIC numerals (0xf0, 0xf1, etc.) will
  evaluate negative on z/OS (ergo, < ' ').
  Make sure '^' (0x5e ->0xb0) and '@' (0x40 ->0x7c) get translated
  properly when this code is uploaded to a mainframe to compile.
  Substituting '@' + [0-31] throws the low 32 chars into the range of
  ASCII upper case letters.  But no such luck with EBCDIC - and in
  EBCDIC there is no contiguous run of 64 (' ' = 0x40) safe characters!
  Unfortunately, I think stupid unprintable characters are allowed in
  filenames in the USS environment (but not MVS), though I've never
  seen anyone try to use them.
  For now, I think we shouldn't bother.  This doesn't affect the storing
  of filenames, just displaying them.
*/
# if defined(UNIX) && !defined(EBCDIC)
  /* Copy source string, expanding non-printable characters to "^x". */
  cp_dst = temp_string;
  cp_src = local_string;
  while (*cp_src != '\0') {
    if ((unsigned char)*cp_src < ' ') {
      *cp_dst++ = '^';
      *cp_dst++ = '@'+ *cp_src++;
    }
    else {
      *cp_dst++ = *cp_src++;
    }
  }
  *cp_dst = '\0';
# else /* not UNIX */
  strcpy(temp_string, local_string);
# endif /* UNIX */
#endif

/*  RBW  --  2009/06/22  */
/*
  I think this block got misplaced.
  It used to be below the EBCDIC block.
*/
  if ((display_string = (char *)malloc(strlen(temp_string) + 1)) == NULL) {
    ZIPERR(ZE_MEM, "local_to_display_string");
  }
  strcpy(display_string, temp_string);
  free(temp_string);

#ifdef EBCDIC
  {
    char *ebc;

    if ((ebc = malloc(strlen(display_string) + 1)) ==  NULL) {
      ZIPERR(ZE_MEM, "local_to_display_string");
    }
    strtoebc(ebc, display_string);
    free(display_string);
    display_string = ebc;
  }
#endif

  return display_string;
}


#ifdef UNICODE_SUPPORT

/* utf8_to_local_string
 *
 * The iconv code is old.  We no longer TRANSLIT the charset conversion
 * as the result is unpredictable.  All chars that are not a direct match
 * are now replaced by Unicode escapes.  This provides predictable
 * translations.  The best guess transliterations were marginally useful
 * at best anyway.  See wide_to_local_string().  Should do iconv conversion
 * one char at a time (without TRANSLIT) and check result each time instead.
 * A non-converted char gets escaped.
 */
char *utf8_to_local_string(char *utf8_string)
{
  /* Let wide_to_local_string() choose either WCHAR or ICONV conversion. */
#if defined(UNICODE_WCHAR) || defined(HAVE_ICONV)
  zwchar *wide_string;
  char *loc;
  
  if (utf8_string == NULL)
    return NULL;

  wide_string = utf8_to_wide_string(utf8_string);
  if (wide_string == NULL)
    return NULL;

  loc = wide_to_local_string(wide_string);
  if (wide_string)
    free(wide_string);
  
  return loc;

#else

# ifdef HAVE_ICONV
  iconv_t  cd;     /* conversion descriptor */
  size_t   iconv_result;
  int      close_result;

  size_t utf8_string_len;

  char *outbuf;
  size_t outbuf_size;

  size_t inbytesleft;
  size_t outbytesleft;

  char tocodename[100];

  char *fromcode = "UTF-8";
  char *tocode   = charsetname;

  char *inp;
  char *outp;

  size_t outlen;
  int i;


  /* Enable best fit if from char not in tocode. */
  /* For GNU libiconv, this tends to generate ?. */
  strcpy(tocodename, charsetname);
  strcat(tocodename, "//TRANSLIT");
  tocode = tocodename;

  utf8_string_len = strlen(utf8_string);

  /* assume local string will take no more than 8x input string */
  outbuf_size = (8 * utf8_string_len + 40) * sizeof(char);

  if ((outbuf = (char *)malloc(outbuf_size)) == NULL) {
    ZIPERR(ZE_MEM, "utf8_to_local_string");
  }

  strcpy(outbuf, "1234567890123456789012345678901234567890");

  inp = (char *)utf8_string;
  outp = outbuf;

#if 0
  printf("in:  '%s'\n", utf8_string);
  printf("in:  '1234567890123456789012345678901234567890'\n");
#endif

  inbytesleft = utf8_string_len;
  outbytesleft = outbuf_size;

  cd = iconv_open(tocode, fromcode);
  if (cd == (iconv_t)-1) {
    zperror("iconv_open");
    free(outbuf);
    ZIPERR(ZE_LOGIC, "error opening iconv");
  }

#if 0
  printf("  iconv  inleft %d  outleft %d\n", inbytesleft, outbytesleft);
#endif

  iconv_result = iconv(cd, &inp, &inbytesleft, &outp, &outbytesleft);

#if 0
  printf("  iconv  inleft %d  outleft %d\n", inbytesleft, outbytesleft);
#endif

  close_result = iconv_close(cd);

  if (iconv_result == (size_t)-1) {
    zperror("iconv");
    free(outbuf);
    return NULL;
  }

#if 1
  printf("  iconv result = %d\n", iconv_result);
#endif

  if (close_result == -1) {
    zperror("iconv_close");
    free(outbuf);
    ZIPERR(ZE_LOGIC, "error closing iconv");
  }

  /* NULL terminate result */
  outlen = outbuf_size - outbytesleft;
  outbuf[outlen] = '\0';

  /* Change over default character for missing chars to our default. */
  for (i = 0; i < outlen; i++) {
    if (outbuf[i] == '?') {
      outbuf[i] = zwchar_to_wchar_t_default_char;
    }
  }

  return outbuf;

# else /* not HAVE_ICONV */

  ZIPERR(ZE_COMPILE, "utf8_to_local_string has no appropriate implementation");

# endif /* not HAVE_ICONV */

#endif /* not UNICODE_WCHAR */

}


/* utf8_to_escape_string
 */
char *utf8_to_escape_string(char *utf8_string)
{
  zwchar *wide_string = utf8_to_wide_string(utf8_string);
  char *escape_string = wide_to_escape_string(wide_string);
  free(wide_string);
  return escape_string;
}

#ifndef WIN32   /* The Win32 port uses a system-specific variant. */

/* convert multi-byte character string to wide character string */
zwchar *local_to_wide_string(char *local_string)
{
# ifdef UNICODE_WCHAR
  int wsize;
  wchar_t *wc_string;
  zwchar *wide_string;

  /* for now try to convert as string - fails if a bad char in string */
  wsize = mbstowcs(NULL, local_string, MB_CUR_MAX );
  if (wsize == (size_t)-1) {
    /* could not convert */
    return NULL;
  }

  /* convert it */
  if ((wc_string = (wchar_t *)malloc((wsize + 1) * sizeof(wchar_t))) == NULL) {
    ZIPERR(ZE_MEM, "local_to_wide_string");
  }
  /* Fix by kellner, from forum, 12 Feb 2009 */
  wsize = mbstowcs(wc_string, local_string, wsize + 1);
  wc_string[wsize] = (wchar_t) 0;

  /* in case wchar_t is not zwchar */
  if ((wide_string = (zwchar *)malloc((wsize + 1) * sizeof(zwchar))) == NULL) {
    ZIPERR(ZE_MEM, "local_to_wide_string");
  }
  for (wsize = 0; (wide_string[wsize] = (zwchar)wc_string[wsize]); wsize++) ;
  wide_string[wsize] = (zwchar)0;
  free(wc_string);

  return wide_string;

# else /* not UNICODE_WCHAR */

#  ifdef HAVE_ICONV
  iconv_t  cd;     /* conversion descriptor */
  size_t   iconv_result;
  int      close_result;

  size_t local_string_len;

  zwchar *outbuf;
  size_t outbuf_size;

  size_t inbytesleft;
  size_t outbytesleft;

  char *fromcode = charsetname;
  char *tocode   = "UCS-4LE";    /* should be equivalent to zwchar */

  char *inp;
  char *outp;

  local_string_len = strlen(local_string);

  /* assume local string will take no more than 4x input string */
  outbuf_size = (4 * local_string_len + 40) * sizeof(zwchar);

  if ((outbuf = (zwchar *)malloc(outbuf_size)) == NULL) {
    ZIPERR(ZE_MEM, "local_to_wide_string");
  }

  inp = local_string;
  outp = (char *)outbuf;

#if 0
  printf("in:  '%s'\n", local_string);
  printf("in:  '1234567890123456789012345678901234567890'\n");
#endif

  inbytesleft = local_string_len;
  outbytesleft = outbuf_size;

  cd = iconv_open(tocode, fromcode);
  if (cd == (iconv_t)-1) {
    zperror("iconv_open");
    free(outbuf);
    ZIPERR(ZE_LOGIC, "error opening iconv");
  }

#if 0
  printf("  iconv  inleft %d  outleft %d\n", inbytesleft, outbytesleft);
#endif

  iconv_result = iconv(cd, &inp, &inbytesleft, &outp, &outbytesleft);

#if 0
  printf("  iconv  inleft %d  outleft %d\n", inbytesleft, outbytesleft);
#endif

  close_result = iconv_close(cd);

  if (iconv_result == (size_t)-1) {
    zperror("iconv");
    free(outbuf);
    return NULL;
  }

  if (close_result == -1) {
    zperror("iconv_close");
    free(outbuf);
    ZIPERR(ZE_LOGIC, "error closing iconv");
  }

  return outbuf;

#  else /* not HAVE_ICONV */

  ZIPERR(ZE_COMPILE, "local_to_wide_string has no appropriate implementation");

#  endif /* not HAVE_ICONV */

# endif /* not UNICODE_WCHAR */
}
#endif /* not WIN32 */


/* All wchar functions are only used by Windows and are
   now in win32zip.c so that the Windows functions can
   be used and multiple character wide characters can
   be handled easily.
   
   That used to be true.  Now need this to do things
   like uppercasing strings.
 */
#if 0
# ifndef WIN32
char *wchar_to_utf8_string(wchar_t *wstring)
{
  zwchar *wide_string = wchar_to_wide_string(wstring);
  char *local_string = wide_to_utf8_string(wide_string);

  free(wide_string);

  return local_string;
}
# endif
#endif


/* wide_to_utf8_string
 *
 * Convert wide string to UTF-8.
 */
char *wide_to_utf8_string(zwchar *wide_string)
{
  int mbcount;
  char *utf8_string;

#if 0
  int i;
  wchar_t *wchar_string;
  wchar_string = wide_to_wchar_string(wide_string);

  printf("wide_string ");
  for (i = 0; wide_string[i]; i++)
    printf(" %x", wide_string[i]);
  printf("\n");
#endif

  /* get size of utf8 string */
  mbcount = ucs4_string_to_utf8(wide_string, NULL, 0);
  if (mbcount == -1)
    return NULL;
  if ((utf8_string = (char *) malloc(mbcount + 1)) == NULL) {
    ZIPERR(ZE_MEM, "wide_to_utf8_string");
  }
  mbcount = ucs4_string_to_utf8(wide_string, utf8_string, mbcount + 1);
  if (mbcount == -1)
    return NULL;

  return utf8_string;
}


/* utf8_to_wide_string
 *
 * Convert UTF-8 string to wide string.
 */
zwchar *utf8_to_wide_string(ZCONST char *utf8_string)
{
  int wcount;
  zwchar *wide_string;

  wcount = utf8_to_ucs4_string(utf8_string, NULL, 0);
  if (wcount == -1)
    return NULL;
  if ((wide_string = (zwchar *) malloc((wcount + 2) * sizeof(zwchar))) == NULL) {
    ZIPERR(ZE_MEM, "utf8_to_wide_string");
  }
  wcount = utf8_to_ucs4_string(utf8_string, wide_string, wcount + 1);

  return wide_string;
}


#endif /* UNICODE_SUPPORT */




/*---------------------------------------------------------------
 * zprintf(), zfprintf(), zperror() - handle Zip output
 *
 * These allow redirecting output as well as feeding the LIB and DLL
 * callbacks.  Otherwise work just like printf() and fprintf().
 *
 * zprintf() returns what printf() would return.
 *
 * The NO_PROTO case for zprintf() and zfprintf() is broke (can't handle
 * arguments).
 */

/* zprintf */

#ifndef NO_PROTO
int zprintf(const char *format, ...)
#else
int zprintf(format)
  const char *format;
#endif
{
  int len;
  char buf[ERRBUF_SIZE + 1];
#ifndef NO_PROTO
  /* handle variable length arg list */
  va_list argptr;

  va_start(argptr, format);
  len = vsprintf(buf, format, argptr);
  va_end(argptr);
#else
  /* can't handle args, so just output the format string */
  buf[0] = '\0';
  strncat(buf, format, ERRBUF_SIZE);
#endif
  if (strlen(buf) >= ERRBUF_SIZE) {
    ZIPERR(ZE_LOGIC, "zprintf buf overflow");
  }

#ifdef ZIPLIB
  /* output when Zip is LIB */
  if (lpZipUserFunctions->print) {
    len = lpZipUserFunctions->print(buf, len);
  }
  else
  {
    len = printf("%s", buf);
  }
#else
# ifdef ZIPDLL
  /* output when Zip is DLL */
  if (lpZipUserFunctions->print) {
    len = lpZipUserFunctions->print(buf, len);
  }
  else
  {
    len = printf("%s", buf);
  }
# else
  /* normal output */
  len = printf("%s", buf);
# endif
#endif

  return len;
}


/* zfprintf */

#ifndef NO_PROTO
int zfprintf(FILE *file, const char *format, ...)
#else
int zfprint(file, format)
  FILE *file;
  const char *format;
#endif
{
  int len;
  char buf[ERRBUF_SIZE + 1];
#ifndef NO_PROTO
  va_list argptr;

  va_start(argptr, format);
  len = vsprintf(buf, format, argptr);
  va_end(argptr);
#else
  /* can't handle args, so just output the format string */
  buf[0] = '\0';
  strncat(buf, format, ERRBUF_SIZE);
#endif
  if (strlen(buf) >= ERRBUF_SIZE) {
    ZIPERR(ZE_LOGIC, "zfprintf buf overflow");
  }

#ifdef ZIPLIB
  /* output when Zip is LIB */
  if ((file == stderr) || (file == stdout))
  {
     if (lpZipUserFunctions->print) {
       len = lpZipUserFunctions->print(buf, len);
     }
     else
     {
       len = fprintf(file, "%s", buf);
     }
   }
  else
  {
    len = fprintf(file, "%s", buf);
  }
#else
# ifdef ZIPDLL
  /* output when Zip is DLL */
  if ((file == stderr) || (file == stdout))
  {
     if (lpZipUserFunctions->print) {
       len = lpZipUserFunctions->print(buf, len);
     }
     else
     {
       len = fprintf(file, "%s", buf);
     }
   }
  else
  {
    len = fprintf(file, "%s", buf);
  }
# else
  /* normal output */
  len = fprintf(file, "%s", buf);
# endif
#endif

  return len;
}


/* zperror */

#ifndef NO_PROTO
void zperror(const char *parm1)
#else
void zperror(parm1)
  const char *parm1;
#endif
{
  char *errstring = strerror(errno);

  zprintf("%s: %s", parm1, errstring);
}






/*---------------------------------------------------------------
 *  Long option support
 *
 *  Defines function get_option() to get and process the command
 *  line options and arguments from argv[].  The caller calls
 *  get_option() in a loop to get either one option and possible
 *  value or a non-option argument each loop.
 *
 *  This version includes argument file support so should not be used
 *  directly on argv.  Generally best to use copy_args() to create a
 *  malloc'd copy of the args and pass that to get_option().
 *
 *  Supports short and long options as defined in the array options[]
 *  in zip.c, multiple short options in an argument (like -jlv), long
 *  option abbreviation (like --te for --temp-file if --te unique),
 *  short and long option values (like -b filename or --temp-file filename
 *  or --temp-file=filename), optional and required values, option negation
 *  by trailing - (like -S- to not include hidden and system files in MSDOS),
 *  value lists (like -x a b c), argument permuting (returning all options
 *  and values before any non-option arguments), and argument files (where any
 *  non-option non-value argument in form @path gets substituted with the
 *  white space separated arguments in the text file at path).
 *
 *  As of Zip 3.1, argument file support has been restored.
 *
 *  get_option() and the command line processing code below was written from
 *  scratch by E. Gordon and donated to Info-ZIP.
 *
 *  E. Gordon
 *  8/23/2003
 *  3/4/2015
 */


/* message output - char casts are needed to handle constants */
#define oWARN(message) zipwarn((char *) message, "")
#define oERR(err,message) ZIPERR(err, (char *) message)


/* Although the below provides some support for multibyte characters
   the proper thing to do may be to use wide characters and support
   Unicode.  May get to it soon.  EG
 */

/* For now stay with multi-byte characters.  Wide character support for
   Windows now added to Zip, but the wide command line arguments are converted
   to UTF-8 so that the multi-byte get_option() code can be used with them.
 */

/* multibyte character set support
   Multibyte characters use typically two or more sequential bytes
   to represent additional characters than can fit in a single byte
   character set.  The code used here is based on the ANSI mblen function. */
#ifdef MULTIBYTE_GETOPTNS
  int mb_clen(ptr)
    ZCONST char *ptr;
  {
    /* return the number of bytes that the char pointed to is.  Return 1 if
       null character or error like not start of valid multibyte character. */
    int cl;

    cl = mblen(ptr, MB_CUR_MAX);
    return (cl > 0) ? cl : 1;
  }
#endif


  /* moved to zip.h */
#if 0
#ifdef UNICODE_SUPPORT
# define MB_CLEN(ptr) (1)
# define MB_NEXTCHAR(ptr) ((ptr)++)
# ifdef MULTIBYTE_GETOPTNS
#    undef MULTIBYTE_GETOPTNS
# endif
#else
# ifdef _MBCS
#  ifndef MULTIBYTE_GETOPTNS
#    define MULTIBYTE_GETOPTNS
#  endif
# endif
/* multibyte character set support
   Multibyte characters use typically two or more sequential bytes
   to represent additional characters than can fit in a single byte
   character set.  The code used here is based on the ANSI mblen function. */
#  ifdef MULTIBYTE_GETOPTNS
  local int mb_clen OF((ZCONST char *));  /* declare proto first */
  local int mb_clen(ptr)
    ZCONST char *ptr;
  {
    /* return the number of bytes that the char pointed to is.  Return 1 if
       null character or error like not start of valid multibyte character. */
    int cl;

    cl = mblen(ptr, MB_CUR_MAX);
    return (cl > 0) ? cl : 1;
  }
#  define MB_CLEN(ptr) mb_clen(ptr)
#  define MB_NEXTCHAR(ptr) ((ptr) += MB_CLEN(ptr))
# else
#  define MB_CLEN(ptr) (1)
#  define MB_NEXTCHAR(ptr) ((ptr)++)
# endif
#endif
#endif /* 0 */


/* constants */

/* function get_args_from_arg_file() can return this in depth parameter */
#define ARG_FILE_ERR -1

/* Symbolic values for optchar. */
#define SKIP_VALUE_ARG           -1     /* Skip over a value arg */
#define SKIP_VALUE_ARG2          -2     /* 2012-03-08 SMS. */
#define THIS_ARG_DONE            -3     /* Done processing opts in arg */
#define START_VALUE_LIST         -4     /* Start a value list */
#define IN_VALUE_LIST            -5     /* Within a value list */
#define NON_OPTION_ARG           -6     /* This a non-option arg */
#define STOP_VALUE_LIST          -7     /* End of value list */
#define READ_REST_ARGS_VERBATIM  -8     /* 7/25/04 EG */


/* global veriables */

int enable_permute = 1;                     /* yes - return options first */
/* 7/25/04 EG */
int doubledash_ends_options = 1;            /* when -- what follows are not options */
#if 0
/* 8/25/2014 EG */
/* now global */
int allow_arg_files = 1;
#endif

/* buffer for error messages (this sizing is a guess but must hold 2 paths) */
#define OPTIONERR_BUF_SIZE (FNMAX * 2 + 4000)
local char Far optionerrbuf[OPTIONERR_BUF_SIZE + 1];

/* error messages */
static ZCONST char Far op_not_neg_err[] = "option %s not negatable";
static ZCONST char Far op_req_val_err[] = "option %s requires a value";
static ZCONST char Far op_no_allow_val_err[] = "option %s does not allow a value";
static ZCONST char Far sh_op_not_sup_err[] = "short option '%c' not supported";
static ZCONST char Far oco_req_val_err[] = "option %s requires one character value";
static ZCONST char Far oco_no_mbc_err[] = "option %s does not support multibyte values";
static ZCONST char Far num_req_val_err[] = "option %s requires number value";
static ZCONST char Far long_op_ambig_err[] = "long option '%s' ambiguous";
static ZCONST char Far long_op_not_sup_err[] = "long option '%s' not supported";

#if 0
static ZCONST char Far no_arg_files_err[] = "argument files not enabled\n";
#endif
static ZCONST char Far bad_arg_file_err[] = "error processing argument file";


/* below removed as only used for processing argument files */

/* get_nextarg */
/* get_args_from_string */
/* insert_args */
/* get_args_from_arg_file */

/* these are now replaced by insert_args_from_file() */


/* copy error, option name, and option description if any to buf */
local int optionerr(buf, err, optind, islong)
  char *buf;
  ZCONST char *err;
  int optind;
  int islong;
{
  char optname[100];

  if (options[optind].name && options[optind].name[0] != '\0') {
    if (islong)
      sprintf(optname, "'%s' (%s)", options[optind].longopt, options[optind].name);
    else
      sprintf(optname, "'%s' (%s)", options[optind].shortopt, options[optind].name);
  } else {
    if (islong)
      sprintf(optname, "'%s'", options[optind].longopt);
    else
      sprintf(optname, "'%s'", options[optind].shortopt);
  }
  sprintf(buf, err, optname);
  return 0;
}


/* dump_args
 *
 * Prints the values of the args in an argv[] array.  Array must be NULL terminated.
 *
 * Returns the number of args printed.
 */
#ifndef NO_PROTO
int dump_args(char *arrayname, char *args[])
#else
int dump_args(arrayname, args)
  char *arrayname;
  char *args[];
#endif
{
  int i = 0;

  printf("---------------------------\n");
  printf("dump of %s:\n", arrayname);

  if (args == NULL) {
    printf("  NULL\n");
  }
  else
  {
    for (i = 0; args[i]; i++) {
      printf("%3d : '%s'\n", i, args[i]);
    }
  }
  printf("---------------------------\n");

  return i;
}


/* copy_args
 *
 * Copy arguments in args, allocating storage with malloc.
 * Copies until a NULL argument is found or until max_args args
 * including args[0] are copied.  Set max_args to 0 to copy
 * until NULL.  Always terminates returned args[] with NULL arg.
 *
 * Any argument in the returned args can be freed with free().  Any
 * freed argument should be replaced with either another string
 * allocated with malloc or by NULL if last argument so that free_args
 * will properly work.
 */
char **copy_args(args, max_args)
  char **args;
  int max_args;
{
  int j;
  char **new_args;

  if (args == NULL) {
    return NULL;
  }

  /* Count non-NULL args.  Stop at max_args if reached first. */
  for (j = 0; args[ j] && (max_args == 0 || j < max_args); j++);
  max_args = j;

  if ((new_args = (char **) malloc((max_args + 1) * sizeof(char *))) == NULL)
  {
    /* Running out of memory should probably be fatal */
    oWARN("memory - ca.1");
    return NULL;
  }

  /* Transfer (non-NULL) original args[] to new_args[] */
  for (j = 0; j < max_args; j++)
  {
    if ((new_args[ j] = malloc( strlen( args[ j])+ 1)) == NULL)
    {
      free_args(new_args);
      /* a failed malloc should probably be fatal - propagating
         the failure elsewhere may be misleading */
      oWARN("memory - ca.2");
      return NULL;
    }
    strcpy(new_args[j], args[j]);
  }

  /* NULL_terminate new_args[] */
  new_args[max_args] = NULL;

  return new_args;
}


/* arg_count
 *
 * Return argc for args.
 */
int arg_count(args)
  char **args;
{
  int i;

  for (i = 0; args[i]; i++) ;

  return i;
}


/* free_args
 *
 * Free args created with one of these functions.
 */
int free_args(args)
  char **args;
{
  int i;

  if (args == NULL) {
    return 0;
  }

  for (i = 0; args[i]; i++) {
    free(args[i]);
  }
  free(args);
  return i;
}


/* insert_arg
 *
 * Insert the argument arg into the array args before argument at_arg.
 * Return the new count of arguments (argc).
 *
 * If free_args is true, this function frees the old args array
 * (but not the component strings).  DO NOT set free_args on original
 * argv but only on args allocated with malloc.
 */

int insert_arg(pargs, arg, at_arg, free_args)
   char ***pargs;
   ZCONST char *arg;
   int at_arg;
   int free_args;
{
   char *newarg = NULL;
   char **args;
   char **newargs = NULL;
   int argnum;
   int newargnum;
   int argcnt;
   int newargcnt;

   if (pargs == NULL) {
     return 0;
   }
   args = *pargs;

   /* count args */
   if (args == NULL) {
     argcnt = 0;
   } else {
     for (argcnt = 0; args[argcnt]; argcnt++) ;
   }
   if (arg == NULL) {
     /* done */
     return argcnt;
   }
   newargcnt = argcnt + 1;

   /* get storage for new args */
   if ((newargs = (char **) malloc((newargcnt + 1) * sizeof(char *))) == NULL) {
     oERR(ZE_MEM, "ia");
   }

   /* copy argument pointers from args to position at_arg, copy arg, then rest args */
   argnum = 0;
   newargnum = 0;
   if (args) {
     for (; args[argnum] && argnum < at_arg; argnum++) {
       newargs[newargnum++] = args[argnum];
     }
   }
   /* copy new arg */
   if ((newarg = (char *) malloc(strlen(arg) + 1)) == NULL) {
     oERR(ZE_MEM, "ia");
   }
   strcpy(newarg, arg);

   newargs[newargnum++] = newarg;
   if (args) {
     for ( ; args[argnum]; argnum++) {
       newargs[newargnum++] = args[argnum];
     }
   }
   newargs[newargnum] = NULL;

   /* free old args array but not component strings - this assumes that
      args was allocated with malloc as copy_args does.  DO NOT DO THIS
      on the original argv.
    */
   if (free_args)
     free(args);

   *pargs = newargs;

   return newargnum;
}


/* insert_args_from_file
 *
 * Insert args from file into the array args, replacing argument at_arg.
 *
 * Input args array pargs points to must be NULL terminated.  It must
 * be allocated with malloc (as with copy_args), not original argv.
 *
 * Return the new count of arguments (argc).  Return -1 if error, though
 * most errors (like can't allocate memory) call ZIPERR directly.
 *
 * This should only be used on args[] created by copy_args() where the
 * arguments can be freed.  The old array will be freed and replaced
 * with the new array.
 *
 * Reads a line, parses out whitespace-separated tokens, and inserts those
 * tokens into the command line, replacing arg at_arg.  Arguments that
 * include white space should be surrounded by double quotes (as in
 * "file name with spaces.txt").
 *
 * Args cannot span multiple lines, even if quoted.
 *
 * An argument that is just # starts a comment, meaning later tokens
 * on that line are ignored.  To include the argument #, put double quotes
 * around it.
 *
 * Tokens at the start of a line (ignoring leading white space) starting
 * with # (as in #echo) are taken as directives to Zip.  To include as
 * a file, enclose in double quotes (as in "#echo").
 *
 * 8/26/2014 EG
 */

#ifndef NO_PROTO
int insert_args_from_file(char ***pargs, char *argfilename, int at_arg,
                          int recursion_depth)
#else
int insert_args_from_file(pargs, argfilename, at_arg, recursion_depth)
  char ***pargs;
  char *argfilename;
  int at_arg;
  int recursion_depth
#endif
{
  FILE *argfile = NULL;
  char argfile_line[MAX_ARGFILE_LINE + 1];
  char *a;
  char *c;
  char *argstart;
  int argsize;
  int has_space;
  char *quote_start;
  int i;
  int argc = 0;
  int comment_line = 0;
  int line_number = 0;
  int unmatched_quote = 0;
  char *filename;
  char *afn;
  int afnlen;
  char *filename_with_extension = NULL;
  char extension[10];
  int has_the_extension = 0;
  char actual_argfile_name[MAX_ZIP_ARG_SIZE + 1];
  char *leftedge;
  char spaces[2 * MAX_ARGFILE_DEPTH + 1];
  size_t size;
  int double_dash_seen = 0;
  unsigned char uc;
  int dot;
  char *ext = NULL;

  /* existing args */
  int argnum;
  int argcnt;
  char **args;

  /* new args from argfile */
  char *newarg = NULL;
  char *newarg2;
  size_t newarglen;
  int newargnum;
  int newargcnt = 0;
  char **newargs = NULL;
  int maxargs = 0;

  /* combined old and new args */
  int totalargcnt;
  char **totalargs = NULL;


  /* We are one level deeper. */
  recursion_depth++;

  /* sanity check
   * this should have been caught when @argfile was found below
   */
  if (recursion_depth > MAX_ARGFILE_DEPTH + 1) {
    sprintf(errbuf, "arg file depth (%d) exceeds maximum: %s", recursion_depth,
                                                               argfilename);
    ZIPERR(ZE_DEPTH, errbuf);
  }

  if (pargs == NULL) {
    return 0;
  }
  args = *pargs;

  /* count args */
  if (args == NULL) {
    argcnt = 0;
  } else {
    for (argcnt = 0; args[argcnt]; argcnt++) ;
  }
  if (argfilename == NULL) {
    /* done */
    return argcnt;
  }

  afn = argfilename;
  afnlen = (int)strlen(argfilename);
  if (afn[0] == '"' && afn[afnlen - 1] == '"') {
    /* remove double quotes */
    afn[afnlen - 1] = '\0';
    afn++;
  }

  if ((filename = malloc(strlen(afn) + 5)) == NULL) {
    ZIPERR(ZE_MEM, "iaff");
  }
  strcpy(filename, afn);

  if ((filename_with_extension = malloc(strlen(afn) + 5)) == NULL) {
    ZIPERR(ZE_MEM, "iaff");
  }
  strcpy(filename_with_extension, afn);

  dot = -1;
  for (i = 0; filename[i]; i++) {
    if (filename[i] == '.') {
      dot = i;
      break;
    }
  }
  if (dot != -1)
    ext = filename + dot;

  if (ext) {
    strcpy(extension, ARGFILE_EXTENSION);
#if defined(DOS) || defined(WIN32)
    /* case insensitive */
    if (strmatch(ext, extension, CASE_INS, ENTIRE_STRING))
      has_the_extension = 1;
#else
    /* case sensitive */
    if (strmatch(ext, extension, CASE_SEN, ENTIRE_STRING))
      has_the_extension = 1;
#endif
  }

  if (!has_the_extension)
  {
    strcat(filename_with_extension, ARGFILE_EXTENSION);
  }

  /* open argument file (arg file) */

  /* try with extension first */
  argfile = zfopen(filename_with_extension, "r");
  if (argfile)
  {
    strcpy(actual_argfile_name, filename_with_extension);
  }
  else
  {
    /* some possible errors
         ENOENT (2) = no such file
         EMFILE (24) = too many files open
     */
    if (errno != ENOENT) {
      sprintf(errbuf, "could not open arg file: '%s'", filename_with_extension);
      zipwarn(errbuf, "");
      sprintf(errbuf, "  %s", filename_with_extension);
      perror(errbuf);

      free(filename);
      free(filename_with_extension);

      /* Unwind the recursion stack */
      return -1;
    }
    else {
      /* try without extension */
      argfile = zfopen(filename, "r");
    }
    if (argfile)
    {
      strcpy(actual_argfile_name, filename);
    }
    else
    {
      sprintf(errbuf, "could not open arg file: '%s'", filename);
      zipwarn(errbuf, "");
      sprintf(errbuf, "  %s", filename);
      perror(errbuf);

      free(filename);
      free(filename_with_extension);

      /* Unwind the recursion stack */
      return -1;
    }
  }

  free(filename);
  free(filename_with_extension);

  /* Read lines from argfile and parse out arguments.
   *
   * A blank line is a comment.  An arg that is just # starts a comment.
   * For example:
   *    file1.txt  file2.txt      #  the rest of the line is a comment
   * There must not be any non-spaces next to the #.
   *
   * If @argfile is found, recurses to process that file at that arg
   * location, incrementing recursion_depth. 
   */

#ifdef UNICODE_SUPPORT
# if 0
  /* check for UTF-8 */
  if (is_utf8_file(argfile, NULL, NULL, NULL, NULL)) {
    zipmessage("argfile is UTF-8", "");
  }
  else
# endif
  /* check for Windows UTF-16, which we don't support */
  if (is_utf16LE_file(argfile)) {
    zipwarn("Argfile appears to be unsupported Windows \"Unicode\" double byte:",
            actual_argfile_name);
    zipwarn("Try saving as \"UTF-8\" format instead.", "");
    fclose(argfile);
    return -1;
  }
#endif

  line_number = 0;

  while (1)
  {
    if (feof(argfile)) {
      break;
    }

    a = fgets(argfile_line, MAX_ARGFILE_LINE, argfile);
  
    if (a == NULL) {
      if (ferror(argfile)) {
        zperror(argfilename);
        if (newargs)
          free(newargs);
        fclose(argfile);
        return -1;
      }
      break;
    }

    line_number++;

    /* parse line */

    c = argfile_line;
    comment_line = FALSE;
  
    /* skip any leading whitespace */
    uc = *c;
    while (*c && isspace(uc)) {
      c++;
      uc = *c;
    }

    /* An empty line or a line with just whitespace are comment lines.
     *
     * Below, an arg that is just # starts a comment that runs to the end of the
     * line, i.e. anything after that on the line is ignored.
     */
    if (*c == '\0') {
      comment_line = TRUE;
    }

    if (!comment_line)
    {
      leftedge = c;
      argstart = c;
      while (1) {
        /* skip over double quoted string if has space */
        if (*c == '"') {
          /* starting double quote */
          has_space = 0;
          quote_start = c;
          unmatched_quote = 1;
          while (*c) {
            c++;
            uc = *c;
            if (isspace(uc))
              has_space = 1;
            if (*c == '"') {
              c++;
              unmatched_quote = 0;
              break;
            }
          }
          if (unmatched_quote) {
            sprintf(errbuf, "unmatched quote on line %d in %s",
                    line_number, argfilename);
            zipwarn(errbuf, "");
            return -1;
          }
          if (!has_space) {
            /* no need for quotes, so remove */
            /* replace end quote with space */
            c--;
            *c = ' ';
            /* replace start quote with space */
            c = quote_start;
            *c = ' ';
            c++;
            argstart++;
            /* reprocess */
            continue;
          }
        } /* double quote */
        else
        {
          /* not a double quote */

          uc = *c;    
          if (*c == '\0' || isspace(uc)) {
            /* end of line or end of arg */
        
            /* ------------------------------------------------- */
            /* process arg */

            argsize = (int)(c - argstart);

            /* get space for new arg */
            size = (argsize + 1) * sizeof(char);
            if ((newarg = (char *)malloc(size)) == NULL) {
              ZIPERR(ZE_MEM, "iaff (1)");
            }
            for (a = argstart, i = 0; a < c; a++, i++) {
              newarg[i] = *a;
            }
            newarg[i] = '\0';

            /* -- */
            if (strcmp(newarg, "--") == 0) {
              /* rest of args read verbatim */
              /* argfile comments and directives still enabled */
              double_dash_seen = 1;
#if 0
              printf(" {--} ");
#endif
            }

            /* # comment */
            if (strcmp(newarg, "#") == 0) {
              /* rest of line is comment */
              /* break without adding this arg */
#if 0
              printf(" {#} ");
#endif
              break;
            }

            /* #echo */
            if (argstart == leftedge && strmatch(newarg, "#echo", CASE_INS, ENTIRE_STRING)) {
              int clen;
              
              spaces[0] = '\0';
              for (i = 0; i < recursion_depth; i++)
                strcat(spaces, "  ");
              /* output rest of line to mesg stream */
              /* break without adding this arg */
#if 0
              printf(" {#echo} ");
#endif
              clen = (int)strlen(c);
              if (c[clen - 1] == '\n') {
                c[clen - 1] = '\0';
              }
              /* Display pieces separately as one could be UTF-8 and other not. */
              sprintf(errbuf, "%s%s :", spaces, actual_argfile_name);
              zipmessage_nl(errbuf, 0);
              sprintf(errbuf, " %s", c);
              zipmessage_nl(errbuf, 1);
              break;
            }

            /* @argfile */
            if (!double_dash_seen && newarg[0] == '@' && newarg[1] != '\0') {
              char ***piargs = NULL;
              char **iargs = NULL;
              int iargcnt;
              char *iargfilename;
              char indent[(MAX_ARGFILE_DEPTH + 1) * 2];
              int i;

#if 0
              printf(" {argfile %s} ", newarg);
#endif

              iargfilename = newarg + 1;

              if (recursion_depth >= MAX_ARGFILE_DEPTH) {
                sprintf(errbuf,
           "max arg file depth (%d) would be exceeded:  %s at line %d in %s",
                        MAX_ARGFILE_DEPTH, newarg, line_number, argfilename);
                zipwarn(errbuf, "");
                return -1;
              }

              piargs = &iargs;

              /* recurse to read arg file */
              iargcnt = insert_args_from_file(piargs, iargfilename, 0,
                                              recursion_depth);
              if (iargcnt == -1) {
                strcpy(indent, "");
                for (i = 1; i <= recursion_depth; i++) {
                  strcat(indent, "  ");
                }
                sprintf(errbuf, "error processing arg file:  %s%s", indent,
                        iargfilename);
                zipwarn(errbuf, "");
                return -1;
              }

              /* insert args into newargs[] */
              iargs = *piargs;

              /* dump_args("iargs after insert", iargs); */

              /* make sure newargs[] has space */
              if (newargcnt + iargcnt + 2 > maxargs) {
                /* add more space to newargs[] */
                maxargs += iargcnt + ADD_ARGS_DELTA;
                size = (maxargs + 1) * sizeof(char *);
                if ((newargs = (char **)realloc(newargs, size)) == NULL) {
                  ZIPERR(ZE_MEM, "iaff (2a)");
                }
              }

              for (i = 0; i < iargcnt && iargs[i]; i++) {
                newargs[newargcnt++] = iargs[i];
              }
              newargs[newargcnt] = NULL;
              free(iargs);

            } /* @argfilename */

            else

            {
              /* add this argument */

              /* make sure newargs[] has space */
              newargcnt++;
              if (newargcnt + 1 > maxargs) {
                /* add more space to newargs[] */
                maxargs += ADD_ARGS_DELTA;
                size = (maxargs + 1) * sizeof(char *);
                if ((newargs = (char **)realloc(newargs, size)) == NULL) {
                  ZIPERR(ZE_MEM, "iaff (3a)");
                }
              }

              /* copy the new arg into newargs[] */
              newarglen = strlen(newarg);
              if (newarg[0] == '\"' && newarg[newarglen - 1] == '\"') {
                /* remove quotes */
                if ((newarg2 = malloc(newarglen + 1)) == NULL) {
                  ZIPERR(ZE_MEM, "iaff (4)");
                }
                strcpy(newarg2, newarg + 1);
                newarg2[strlen(newarg2) - 1] = '\0';
                free(newarg);
                newargs[newargcnt - 1] = newarg2;
              }
              else
              {
                newargs[newargcnt - 1] = newarg;
              }
              newargs[newargcnt] = NULL;

            } /* add argument */

            /* dump_args("newargs after newarg insert", newargs); */

            /* ------------------------------------------------- */

            /* skip whitespace */
            uc = *c;
            while (*c && isspace(uc)) {
              c++;
              uc = *c;
            }
            argstart = c;

            if (*c == '\0') {
              break;
            }

          }
          else
          {
            /* skip over non-space char */
            c++;
          }
        } /* not double quote */
    
      } /* while */
    } /* !comment_line */

  } /* while(!eof(argfile)) */

  /* insert new args into args */

  /* total number of args to return */
  totalargcnt = argcnt + newargcnt + 1;

  /* get storage for combined args */
  if ((totalargs = (char **) malloc((totalargcnt + 1) * sizeof(char *))) == NULL) {
    oERR(ZE_MEM, "iaff (5)");
  }

  /* copy argument pointers from args to position at_arg, copy newargs, then
   * rest of args
   */
  argnum = 0;
  newargnum = 0;
  totalargcnt = 0;

#if 0
  dump_args("args", args);
  dump_args("newargs", newargs);
#endif

  /* front half of existing args */
  if (args) {
    for (; args[argnum] && argnum < at_arg; argnum++) {
      totalargs[totalargcnt++] = args[argnum];
    }
  }

  totalargs[totalargcnt] = NULL;
  /* dump_args("totalargs - first half args", totalargs); */

  /* new args from file */
  if (newargs) {
    for (; newargs[newargnum] && newargnum < newargcnt; newargnum++) {
      totalargs[totalargcnt++] = newargs[newargnum];
    }
  }

  totalargs[totalargcnt] = NULL;
  /* dump_args("totalargs - first half args + newargs", totalargs); */

  /* back half of existing args */
  if (args) {
    /* skip arg file arg */
    argnum++;
    for ( ; args[argnum] && argnum < argcnt; argnum++) {
      totalargs[totalargcnt++] = args[argnum];
    }
  }

  /* NULL terminate combined args */
  totalargs[totalargcnt] = NULL;

  /* dump_args("totalargs - all", totalargs); */

  /* free args and newargs, but not component strings that were moved to
   * totalargs.
   */
  free(args);
  free(newargs);

  *pargs = totalargs;

  return totalargcnt;
}



/* ------------------------------------- */




/* get_shortopt
 *
 * Get next short option from arg.  The state is stored in argnum, optchar, and
 * option_num so no static storage is used.  Returns the option_ID.
 *
 * parameters:
 *    args        - argv array of arguments
 *    argnum      - index of current arg in args
 *    optchar     - pointer to index of next char to process.  Can be 0 or
 *                  const defined at top of this file like THIS_ARG_DONE
 *    negated     - on return pointer to int set to 1 if option negated or 0 otherwise
 *    value       - on return pointer to string set to value of option if any or NULL
 *                  if none.  If value is returned then the caller should free()
 *                  it when not needed anymore.
 *    option_num  - pointer to index in options[] of returned option or
 *                  o_NO_OPTION_MATCH if none.  Do not change as used by
 *                  value lists.
 *    depth       - recursion depth (0 at top level, 1 or more in arg files)
 */
local unsigned long get_shortopt(args, argnum, optchar, negated, value,
                                 option_num, depth)
  char **args;
  int argnum;
  int *optchar;
  int *negated;
  char **value;
  int *option_num;
  int depth;
{
  char *shortopt;
  int clen;
  char *nextchar;
  char *s;
  char *start;
  int op;
  char *arg;
  int match = -1;


  /* get arg */
  arg = args[argnum];
  /* current char in arg */
  nextchar = arg + (*optchar);
  clen = MB_CLEN(nextchar);
  /* next char in arg */
  (*optchar) +=  clen;
  /* get first char of short option */
  shortopt = arg + (*optchar);
  /* no value */
  *value = NULL;

  if (*shortopt == '\0') {
    /* no more options in arg */
    *optchar = 0;
    *option_num = o_NO_OPTION_MATCH;
    return 0;
  }

  /* look for match in options - all valid options use ASCII single-byte
     characters, so we ignore any multi-byte characters in the option
     string - this would need to be changed if non-ASCII short options were
     to be supported - long options should support multi-byte characters
     however */
  clen = MB_CLEN(shortopt);
  for (op = 0; options[op].option_ID; op++) {
    s = options[op].shortopt;
    if (s && s[0] == shortopt[0]) {
      if (s[1] == '\0' && clen == 1) {
        /* single char match */
        match = op;
      } else {
        /* 2 wide short opt - could support more chars but should use long opts
           instead - as multi-byte character introducers would not match the
           ASCII single-byte characters in s (unless someone put them in the
           Options table, which shouldn't happen), we do not need to worry that
           shortopt[0] or shortopt[1] are part of a multi-byte character - that
           said, a two-byte character could be matched at this point */
        if (s[1] == shortopt[1]) {
          /* match 2 char short opt or 2 byte char */
          match = op;
          /* if the first character was a single byte and we have a two-character
             match, point to the second one */
          /* we do skip multi-byte characters cleanly, but it is possible to match
             the introducer of the second character as a single-byte character -
             introducers should not be used as the second character in Options for
             this reason */
          if (clen == 1) (*optchar)++;
          break;
        }
      }
    }
  }

  if (match >= 0) {
    /* match */
    clen = MB_CLEN(shortopt);
    nextchar = arg + (*optchar) + clen;
    /* check for trailing dash negating option */
    if (*nextchar == '-') {
      /* negated */
      if (options[match].negatable == o_NOT_NEGATABLE) {
        if (options[match].value_type == o_NO_VALUE) {
          optionerr(optionerrbuf, op_not_neg_err, match, 0);
          if (depth > 0) {
            /* unwind */
            oWARN(optionerrbuf);
            return o_ARG_FILE_ERR;
          } else {
            oERR(ZE_PARMS, optionerrbuf);
          }
        }
      } else {
        *negated = 1;
        /* set up to skip negating dash */
        (*optchar) += clen;
        clen = 1;
      }
    }

    /* value */
    clen = MB_CLEN(arg + (*optchar));
    /* optional value, one char value, and number value must follow option */
    if (options[match].value_type == o_ONE_CHAR_VALUE) {
      /* one char value */
      if (arg[(*optchar) + clen]) {
        /* has value */
        if (MB_CLEN(arg + (*optchar) + clen) > 1) {
          /* multibyte value not allowed for now */
          optionerr(optionerrbuf, oco_no_mbc_err, match, 0);
          if (depth > 0) {
            /* unwind */
            oWARN(optionerrbuf);
            return o_ARG_FILE_ERR;
          } else {
            oERR(ZE_PARMS, optionerrbuf);
          }
        }
        if ((*value = (char *) malloc(2)) == NULL) {
          oERR(ZE_MEM, "gso.1");
        }
        (*value)[0] = *(arg + (*optchar) + clen);
        (*value)[1] = '\0';
        *optchar += clen;
        clen = 1;
      } else {
        /* one char values require a value */
        optionerr(optionerrbuf, oco_req_val_err, match, 0);
        if (depth > 0) {
          oWARN(optionerrbuf);
          return o_ARG_FILE_ERR;
        } else {
          oERR(ZE_PARMS, optionerrbuf);
        }
      }
    } else if (options[match].value_type == o_NUMBER_VALUE) {
      /* read chars until end of number */
      start = arg + (*optchar) + clen;
      if (*start == '+' || *start == '-') {
        start++;
      }
      s = start;
      for (; isdigit(*s); MB_NEXTCHAR(s)) ;
      if (s == start) {
        /* no digits */
        optionerr(optionerrbuf, num_req_val_err, match, 0);
        if (depth > 0) {
          oWARN(optionerrbuf);
          return o_ARG_FILE_ERR;
        } else {
          oERR(ZE_PARMS, optionerrbuf);
        }
      }
      start = arg + (*optchar) + clen;
      if ((*value = (char *) malloc((int)(s - start) + 1)) == NULL) {
        oERR(ZE_MEM, "gso.2");
      }
      *optchar += (int)(s - start);
      strncpy(*value, start, (int)(s - start));
      (*value)[(int)(s - start)] = '\0';
      clen = MB_CLEN(s);
    } else if (options[match].value_type == o_OPTIONAL_VALUE) {
      /* optional value */
      /* This seemed inconsistent so now if no value attached to argument look
         to the next argument if that argument is not an option for option
         value - 11/12/04 EG */
      /* Though the parser follows definitive rules, optional values are
         probably confusing to the user and should not be used */
      /* o_OPT_EQ_VALUE is more or less now used instead of o_OPTIONAL_VALUE */
      if (arg[(*optchar) + clen]) {
        /* has value */
        /* add support for optional = - 2/6/05 EG */
        if (arg[(*optchar) + clen] == '=') {
          /* skip = */
          clen++;
        }
        if (arg[(*optchar) + clen]) {
          if ((*value = (char *)malloc(strlen(arg + (*optchar) + clen) + 1))
              == NULL) {
            oERR(ZE_MEM, "gso.3");
          }
          strcpy(*value, arg + (*optchar) + clen);
        }
        *optchar = THIS_ARG_DONE;
      }
    } else if (options[match].value_type == o_OPT_EQ_VALUE) {
      /* Optional value, but "=" required with value.  Forms are:
       * -opt=value
       * -opt= value
       * -opt = value
       * If none of these are found, the option has no value (value = NULL).
       */
      int have_eq = 0;

      if (arg[(*optchar) + clen]) {
        /* "-optXXXX".  May have attached value.  First X must be = if does. */
        if (arg[(*optchar) + clen] == '=') {
          /* So far have -opt=
             Skip '=' (but remember it). */
          clen++;
          have_eq = 1;
        }
        if (arg[(*optchar) + clen]) {
          /* "-opt=value".  Have attached value.
             This option type requires "=" for value, so if no "=" then
             no value. */
          if (have_eq) {
            /* "-opt=value".  Have attached value. */
            if ((*value = (char *)malloc(strlen(arg + (*optchar) + clen) + 1))
                == NULL) {
            oERR(ZE_MEM, "gso.4");
            }
            strcpy(*value, arg + (*optchar) + clen);
            *optchar = THIS_ARG_DONE;
          }
            /* else
             * "-optXXX".  Have more chars, but no "=" so no attached value.
             * Should be more short options.
             */
        }
        /* Found -opt= but no attached value, so look at next argument.
           Removed "&& args[argnum + 1][0] != '-'" as the = forces whatever
           the next argument is to be a value, regardless of a leading -.
           If there is no value, there should not be a trailing "=". */
        else if (have_eq && args[argnum + 1]) {
          /* "-opt= value".  Have detached value. */

          if ((*value = (char *)malloc(strlen(args[argnum + 2])+ 1)) == NULL) {

            oERR(ZE_MEM, "gso.5");
          }
          /* Set value.  Skip value arg. */
          strcpy(*value, args[argnum + 1]);
          *optchar = SKIP_VALUE_ARG;
        } else {
          /* Either no "=" or found "=" but no following argument (which we take
             as no value), so no value unless the "-opt = value" form is used */
          *optchar = THIS_ARG_DONE;
        }
      /* No value found so far, so we look for form "-opt = value". */
      } else if (args[argnum + 1] && (strcmp( args[argnum + 1], "=") == 0)) {
        /* "-opt = ".  Loose "=" token.  Look for detached value.  Removed
           "&& args[argnum + 2][0] != '-'" as = forces what follows to be a
           value. */
        if (args[argnum + 2]) {
          /* "-opt = value".  Have detached value. */
          if ((*value = (char *)malloc(strlen(args[argnum + 2])+ 1)) == NULL) {
            oERR(ZE_MEM, "gso.6");
          }
          /* Set value.  Skip "=" and value args. */
          strcpy(*value, args[argnum + 2]);
          *optchar = SKIP_VALUE_ARG2;
        } else {
          /* "-opt =", but no value.  Skip "=" arg. */
          *optchar = SKIP_VALUE_ARG;
        }
      } else if (args[argnum + 1] && args[argnum + 1][0] == '=') {
        /* "-opt =[XXXX]".  Have detached "=value".  Skip '='. */
        if ((*value = (char *)malloc(strlen(args[argnum + 1]))) == NULL)
        {
          oERR(ZE_MEM, "gso.7");
        }
        /* Using next arg (less '=') as value. */
        strcpy(*value, args[argnum + 1]+ 1);
        *optchar = SKIP_VALUE_ARG;
      } else {
        /* No "=", therefore no value. */
        *optchar = THIS_ARG_DONE;
      }
    } else if (options[match].value_type == o_REQUIRED_VALUE ||
               options[match].value_type == o_VALUE_LIST) {
      /* Required value or value list.  Forms are:
       * -optvalue
       * -opt=value
       * -opt value
       * -opt value1 value2 ... {@}    - value list
       */
      /* see if follows option */
      if (arg[(*optchar) + clen]) {
        /* has value following option as -ovalue */
        /* add support for optional = - 6/5/05 EG */
        if (arg[(*optchar) + clen] == '=') {
          /* skip = */
          clen++;
        }
          if ((*value = (char *)malloc(strlen(arg + (*optchar) + clen) + 1))
              == NULL) {
          oERR(ZE_MEM, "gso.8");
        }
        strcpy(*value, arg + (*optchar) + clen);
        *optchar = THIS_ARG_DONE;
      } else {
        /* use next arg for value */
        if (args[argnum + 1]) {
          if ((*value = (char *)malloc(strlen(args[argnum + 1]) + 1)) == NULL) {
            oERR(ZE_MEM, "gso.9");
          }
          strcpy(*value, args[argnum + 1]);
          if (options[match].value_type == o_VALUE_LIST) {
            *optchar = START_VALUE_LIST;
          } else {
            *optchar = SKIP_VALUE_ARG;
          }
        } else {
          /* no value found */
          optionerr(optionerrbuf, op_req_val_err, match, 0);
          if (depth > 0) {
            oWARN(optionerrbuf);
            return o_ARG_FILE_ERR;
          } else {
            oERR(ZE_PARMS, optionerrbuf);
          }
        }
      }
    }

    *option_num = match;
    return options[match].option_ID;
  }
  sprintf(optionerrbuf, sh_op_not_sup_err, *shortopt);
  if (depth > 0) {
    /* unwind */
    oWARN(optionerrbuf);
    return o_ARG_FILE_ERR;
  } else {
    oERR(ZE_PARMS, optionerrbuf);
  }
  return 0;
}


/* get_longopt
 *
 * Get the long option in args array at argnum.
 * Parameters same as for get_shortopt.
 */

local unsigned long get_longopt(args, argnum, optchar, negated, value,
                                option_num, depth)
  char **args;
  int argnum;
  int *optchar;
  int *negated;
  char **value;
  int *option_num;
  int depth;
{
  char *longopt;
  char *lastchr;
  char *valuestart;
  int op;
  char *arg;
  int match = -1;
  *value = NULL;

  if (args == NULL) {
    *option_num = o_NO_OPTION_MATCH;
    return 0;
  }
  if (args[argnum] == NULL) {
    *option_num = o_NO_OPTION_MATCH;
    return 0;
  }
  /* copy arg so can chop end if value */
  if ((arg = (char *)malloc(strlen(args[argnum]) + 1)) == NULL) {
    oERR(ZE_MEM, "glo.1");
  }
  strcpy(arg, args[argnum]);

  /* get option */
  longopt = arg + 2;
  /* no value */
  *value = NULL;

  /* find = */
  for (lastchr = longopt, valuestart = longopt;
       *valuestart && *valuestart != '=';
       lastchr = valuestart, MB_NEXTCHAR(valuestart)) ;
  if (*valuestart) {
    /* found =value */
    *valuestart = '\0';
    valuestart++;
  } else {
    valuestart = NULL;
  }

  if (*lastchr == '-') {
    /* option negated */
    *negated = 1;
    *lastchr = '\0';
  } else {
    *negated = 0;
  }

  /* look for long option match */
  for (op = 0; options[op].option_ID; op++) {
    if (options[op].longopt && strcmp(options[op].longopt, longopt) == 0) {
      /* exact match */
      match = op;
      break;
    }
    if (options[op].longopt && strncmp(options[op].longopt, longopt, strlen(longopt)) == 0) {
      if (match >= 0) {
        sprintf(optionerrbuf, long_op_ambig_err, longopt);
        free(arg);
        if (depth > 0) {
          /* unwind */
          oWARN(optionerrbuf);
          return o_ARG_FILE_ERR;
        } else {
          oERR(ZE_PARMS, optionerrbuf);
        }
      }
      match = op;
    }
  }

  if (match == -1) {
    sprintf(optionerrbuf, long_op_not_sup_err, longopt);
    free(arg);
    if (depth > 0) {
      oWARN(optionerrbuf);
      return o_ARG_FILE_ERR;
    } else {
      oERR(ZE_PARMS, optionerrbuf);
    }
  }

  /* one long option an arg */
  *optchar = THIS_ARG_DONE;

  /* if negated then see if allowed */
  if (*negated && options[match].negatable == o_NOT_NEGATABLE) {
    optionerr(optionerrbuf, op_not_neg_err, match, 1);
    free(arg);
    if (depth > 0) {
      /* unwind */
      oWARN(optionerrbuf);
      return o_ARG_FILE_ERR;
    } else {
      oERR(ZE_PARMS, optionerrbuf);
    }
  }
  /* get value */

  /* Optional value requires "=".
     Forms:
     -option=value
     -option =value
     -option = value
     -option= value
  */
  if (options[match].value_type == o_OPT_EQ_VALUE) {
    /* Optional value, but "=" required. */
    if (valuestart == NULL) {
      /* "--opt ="? */
      if (args[ argnum+ 1] != NULL) {
        /* Next arg exists. */
        if (*args[ argnum+ 1] == '=') {
          /* Next arg is "=" or "=val". */
          if (*(args[ ++argnum]+ 1) == '\0') {
            /* No attached value.  Use next arg, if any. */
            if (args[ argnum+ 1] != NULL) {
              valuestart = args[ ++argnum];
              *optchar = SKIP_VALUE_ARG2;
            }
          } else {
            /* "=val". */
            valuestart = args[ argnum]+ 1;
            *optchar = SKIP_VALUE_ARG;
          }
        }
      }
    } else if (*valuestart == '\0') {
      /* "--opt= val"? */
      if (args[ argnum+ 1] != NULL) {
        valuestart = args[ ++argnum];
        *optchar = SKIP_VALUE_ARG;
      }
    }
    if (valuestart) {
      /* A value was specified somehow.  Save it. */
      if ((*value = (char *)malloc(strlen(valuestart) + 1)) == NULL) {
        free(arg);
        oERR(ZE_MEM, "glo.2");
      }
      strcpy(*value, valuestart);
    }
  } else if (options[match].value_type == o_OPTIONAL_VALUE) {
    /* optional value in form option=value */
    if (valuestart) {
      /* option=value */
      if ((*value = (char *)malloc(strlen(valuestart) + 1)) == NULL) {
        free(arg);
        oERR(ZE_MEM, "glo.3");
      }
      strcpy(*value, valuestart);
    }
  } else if (options[match].value_type == o_REQUIRED_VALUE ||
             options[match].value_type == o_NUMBER_VALUE ||
             options[match].value_type == o_ONE_CHAR_VALUE ||
             options[match].value_type == o_VALUE_LIST) {
    /* handle long option one char and number value as required value */
    if (valuestart) {
      /* option=value */
      if ((*value = (char *)malloc(strlen(valuestart) + 1)) == NULL) {
        free(arg);
        oERR(ZE_MEM, "glo.4");
      }
      strcpy(*value, valuestart);
    } else {
      /* use next arg */
      if (args[argnum + 1]) {
        if ((*value = (char *)malloc(strlen(args[argnum + 1]) + 1)) == NULL) {
          free(arg);
          oERR(ZE_MEM, "glo.5");
        }
        /* using next arg as value */
        strcpy(*value, args[argnum + 1]);
        if (options[match].value_type == o_VALUE_LIST) {
          *optchar = START_VALUE_LIST;
        } else {
          *optchar = SKIP_VALUE_ARG;
        }
      } else {
        /* no value found */
        optionerr(optionerrbuf, op_req_val_err, match, 1);
        free(arg);
        if (depth > 0) {
          /* unwind */
          oWARN(optionerrbuf);
          return o_ARG_FILE_ERR;
        } else {
          oERR(ZE_PARMS, optionerrbuf);
        }
      }
    }
  } else if (options[match].value_type == o_NO_VALUE) {
    /* this option does not accept a value */
    if (valuestart) {
      /* --option=value */
      optionerr(optionerrbuf, op_no_allow_val_err, match, 1);
      free(arg);
      if (depth > 0) {
        oWARN(optionerrbuf);
        return o_ARG_FILE_ERR;
      } else {
        oERR(ZE_PARMS, optionerrbuf);
      }
    }
  }
  free(arg);

  *option_num = match;
  return options[match].option_ID;
}



/* get_option
 *
 * Main interface for user.  Use this function to get options, values and
 * non-option arguments from a command line provided in argv form.
 *
 * To use get_option() first define valid options by setting
 * the global variable options[] to an array of option_struct.  Also
 * either change defaults below or make variables global and set elsewhere.
 * Zip uses below defaults.
 *
 * Call get_option() to get an option (like -b or --temp-file) and any
 * value for that option (like filename for -b) or a non-option argument
 * (like archive name) each call.  If *value* is not NULL after calling
 * get_option() it is a returned value and the caller should either store
 * the char pointer or free() it before calling get_option() again to avoid
 * leaking memory.  If a non-option non-value argument is returned get_option()
 * returns o_NON_OPTION_ARG and value is set to the entire argument.
 * When there are no more arguments get_option() returns 0.
 *
 * The parameters argnum (after set to 0 on initial call),
 * optchar, first_nonopt_arg, option_num, and depth (after initial
 * call) are set and maintained by get_option() and should not be
 * changed.  The parameters argc, negated, and value are outputs and
 * can be used by the calling program.  get_option() returns either the
 * option_ID for the current option, a special value defined in
 * zip.h, or 0 when no more arguments.
 *
 * The value returned by get_option() is the ID value in the options
 * table.  This value can be duplicated in the table if different
 * options are really the same option.  The index into the options[]
 * table is given by option_num, though the ID should be used as
 * option numbers change when the table is changed.  The ID must
 * not be 0 for any option as this ends the table.  If get_option()
 * finds an option not in the table it calls oERR to post an
 * error and exit.  Errors also result if the option requires a
 * value that is missing, a value is present but the option does
 * not take one, and an option is negated but is not
 * negatable.  Non-option arguments return o_NON_OPTION_ARG
 * with the entire argument in value.
 *
 * For Zip, permuting is on and all options and their values are
 * returned before any non-option arguments like archive name.
 *
 * The arguments "-" alone and "--" alone return as non-option arguments.
 * Note that "-" should not be used as part of a short option
 * entry in the table but can be used in the middle of long
 * options such as in the long option "a-long-option".  Now "--" alone
 * stops option processing, returning any arguments following "--" as
 * non-option arguments instead of options.
 *
 * Argument file support is removed from this version. It may be added later.
 * Being worked.
 *
 * After each call:
 *   argc       is set to the current size of args[] but should not change
 *                with argument file support removed,
 *   argnum     is the index of the current arg,
 *   value      is either the value of the returned option or non-option
 *                argument or NULL if option with no value,
 *   negated    is set if the option was negated by a trailing dash (-)
 *   option_num is set to either the index in options[] for the option or
 *                o_NO_OPTION_MATCH if no match.
 * Negation is checked before the value is read if the option is negatable so
 * that the - is not included in the value.  If the option is not negatable
 * but takes a value then the - will start the value.  If permuting then
 * argnum and first_nonopt_arg are unreliable and should not be used.
 *
 * Command line is read from left to right.  As get_option() finds non-option
 * arguments (arguments not starting with - and that are not values to options)
 * it moves later options and values in front of the non-option arguments.
 * This permuting is turned off by setting enable_permute to 0.  Then
 * get_option() will return options and non-option arguments in the order
 * found.  Currently permuting is only done after an argument is completely
 * processed so that any value can be moved with options they go with.  All
 * state information is stored in the parameters argnum, optchar,
 * first_nonopt_arg and option_num.  You should not change these after the
 * first call to get_option().  If you need to back up to a previous arg then
 * set argnum to that arg (remembering that args may have been permuted) and
 * set optchar = 0 and first_nonopt_arg to the first non-option argument if
 * permuting.  After all arguments are returned the next call to get_option()
 * returns 0.  The caller can then call free_args(args) if appropriate.
 *
 * get_option() accepts arguments in the following forms:
 *  short options
 *       of 1 and 2 characters, e.g. a, b, cc, d, and ba, after a single
 *       leading -, as in -abccdba.  In this example if 'b' is followed by 'a'
 *       it matches short option 'ba' else it is interpreted as short option
 *       b followed by another option.  The character - is not legal as a
 *       short option or as part of a 2 character short option.
 *
 *       If a short option has a value it immediately follows the option or
 *       if that option is the end of the arg then the next arg is used as
 *       the value.  So if short option e has a value, it can be given as
 *             -evalue
 *       or
 *             -e value
 *       and now
 *             -e=value
 *       but now that = is optional a leading = is stripped for the first.
 *       This change allows optional short option values to be defaulted as
 *             -e=
 *       Either optional or required values can be specified.  Optional values
 *       now use both forms as ignoring the later got confusing.  Any
 *       non-value short options can preceed a valued short option as in
 *             -abevalue
 *       Some value types (one_char and number) allow options after the value
 *       so if oc is an option that takes a character and n takes a number
 *       then
 *             -abocVccn42evalue
 *       returns value V for oc and value 42 for n.  All values are strings
 *       so programs may have to convert the "42" to a number.  See long
 *       options below for how value lists are handled.
 *
 *       Any short option can be negated by following it with -.  Any - is
 *       handled and skipped over before any value is read unless the option
 *       is not negatable but takes a value and then - starts the value.
 *
 *       If the value for an optional value is just =, then treated as no
 *       value.
 *
 *  long options
 *       of arbitrary length are assumed if an arg starts with -- but is not
 *       exactly --.  Long options are given one per arg and can be abbreviated
 *       if the abbreviation uniquely matches one of the long options.
 *       Exact matches always match before partial matches.  If ambiguous an
 *       error is generated.
 *
 *       Values are specified either in the form
 *             --longoption=value
 *       or can be the following arg if the value is required as in
 *             --longoption value
 *       Optional values to long options must be in the first form.
 *
 *       Value lists are specified by o_VALUE_LIST and consist of an option
 *       that takes a value followed by one or more value arguments.
 *       The two forms are
 *             --option=value
 *       or
 *             -ovalue
 *       for a single value or
 *             --option value1 value2 value3 ... --option2
 *       or
 *             -o value1 value2 value3 ...
 *       for a list of values.  The list ends at the next option, the
 *       end of the command line, or at a single "@" argument.
 *       Each value is treated as if it was preceeded by the option, so
 *             --option1 val1 val2
 *       with option1 value_type set to o_VALUE_LIST is the same as
 *             --option1=val1 --option1=val2
 *
 *       Long options can be negated by following the option with - as in
 *             --longoption-
 *       Long options with values can also be negated if this makes sense for
 *       the caller as:
 *             --longoption-=value
 *       If = is not followed by anything it is treated as no value.
 *
 *  @path
 *       When an argument in the form @path is encountered, the file at path
 *       is opened and white space separated arguments read from the file
 *       and inserted into the command line at that point as if the contents
 *       of the file were directly typed at that location.  The file can
 *       have options, files to zip, or anything appropriate at that location
 *       in the command line.  Since Zip has permuting enabled, options and
 *       files will propagate to the appropriate locations in the command
 *       line.  These files containing arguments are called argument files.
 *
 *       As of Zip 3.1, argument file support is restored.
 *
 *  non-option argument
 *       is any argument not given above.  If enable_permute is 1 then
 *       these are returned after all options, otherwise all options and
 *       args are returned in order.  Returns option ID o_NON_OPTION_ARG
 *       and sets value to the argument.
 *
 *
 * Arguments to get_option:
 *  char ***pargs          - pointer to arg array in the argv form
 *  int *argc              - returns the current argc for args incl. args[0]
 *  int *argnum            - the index of the current argument (caller
 *                            should set = 0 on first call and not change
 *                            after that)
 *  int *optchar           - index of next short opt in arg or special
 *  int *first_nonopt_arg  - used by get_option to permute args
 *  int *negated           - option was negated (had trailing -)
 *  char *value            - value of option if any (free when done with it) or NULL
 *  int *option_num        - the index in options of the last option returned
 *                            (can be o_NO_OPTION_MATCH)
 *  int recursion_depth    - current depth of recursion
 *                            (always set to 0 by caller)
 *                            (always 0 with argument files support removed)
 *
 *  Caller should only read the returned option ID and the value, negated,
 *  and option_num (if required) parameters after each call.
 *
 *  Ed Gordon
 *  24 August 2003 (last updated 3 March 2015 EG)
 *
 */

unsigned long get_option(pargs, argc, argnum, optchar, value, negated,
                         first_nonopt_arg, option_num, recursion_depth)
  char ***pargs;
  int *argc;
  int *argnum;
  int *optchar;
  char **value;
  int *negated;
  int *first_nonopt_arg;
  int *option_num;
  int recursion_depth;
{
  char **args;
  unsigned long option_ID;

  int argcnt;
  int first_nonoption_arg;
  char *arg = NULL;
  int h;
  int optc;
  int argn;
  int j;
  int v;
  int read_rest_args_verbatim = 0;  /* 7/25/04 - ignore options and arg files for rest args */

  /* value is outdated.  The caller should free value before
     calling get_option again. */
  *value = NULL;

  /* if args is NULL then done */
  if (pargs == NULL) {
    *argc = 0;
    return 0;
  }
  args = *pargs;
  if (args == NULL) {
    *argc = 0;
    return 0;
  }

  /* count args */
  for (argcnt = 0; args[argcnt]; argcnt++) ;

  /* if no provided args then nothing to do */
  if (argcnt < 1 || (recursion_depth == 0 && argcnt < 2)) {
    *argc = argcnt;
    /* return 0 to note that no args are left */
    return 0;
  }

  *negated = 0;
  first_nonoption_arg = *first_nonopt_arg;
  argn = *argnum;
  optc = *optchar;

  if (optc == READ_REST_ARGS_VERBATIM) {
    read_rest_args_verbatim = 1;
  }

  if (argn == -1 || (recursion_depth == 0 && argn == 0)) {
    /* first call */
    /* if depth = 0 then args[0] is argv[0] so skip */
    *option_num = o_NO_OPTION_MATCH;
    optc = THIS_ARG_DONE;
    first_nonoption_arg = -1;
  }

  /* if option_num is set then restore last option_ID in case continuing value list */
  option_ID = 0;
  if (*option_num != o_NO_OPTION_MATCH) {
    option_ID = options[*option_num].option_ID;
  }

  /* get next option if any */
  for (;;)  {
    if (read_rest_args_verbatim) {
      /* rest of args after "--" are non-option args if doubledash_ends_options set */
      argn++;
      if (argn > argcnt || args[argn] == NULL) {
        /* done */
        option_ID = 0;
        break;
      }
      arg = args[argn];
      if ((*value = (char *)malloc(strlen(arg) + 1)) == NULL) {
        oERR(ZE_MEM, "go.1");
      }
      strcpy(*value, arg);
      *option_num = o_NO_OPTION_MATCH;
      option_ID = o_NON_OPTION_ARG;
      break;

    /* permute non-option args after option args so options are returned first */
    } else if (enable_permute) {
      if (optc == SKIP_VALUE_ARG || optc == SKIP_VALUE_ARG2 ||
          optc == THIS_ARG_DONE ||
          optc == START_VALUE_LIST || optc == IN_VALUE_LIST ||
          optc == STOP_VALUE_LIST) {
        /* moved to new arg */
        if (first_nonoption_arg > -1 && args[first_nonoption_arg]) {
          /* do the permuting - move non-options after this option */
          /* if option and value separate args or starting list skip option */
          if (optc == SKIP_VALUE_ARG2) {
            v = 2;
          } else if (optc == SKIP_VALUE_ARG || optc == START_VALUE_LIST) {
            v = 1;
          } else {
            v = 0;
          }
          for (h = first_nonoption_arg; h < argn; h++) {
            arg = args[first_nonoption_arg];
            for (j = first_nonoption_arg; j < argn + v; j++) {
              args[j] = args[j + 1];
            }
            args[j] = arg;
          }
          first_nonoption_arg += 1 + v;
        }
      }
    } else if (optc == NON_OPTION_ARG) {
      /* if not permuting then already returned arg */
      optc = THIS_ARG_DONE;
    }

    /* value lists */
    if (optc == STOP_VALUE_LIST) {
      optc = THIS_ARG_DONE;
    }

    if (optc == START_VALUE_LIST || optc == IN_VALUE_LIST) {
      if (optc == START_VALUE_LIST) {
        /* already returned first value */
        argn++;
        optc = IN_VALUE_LIST;
      }
      argn++;
      arg = args[argn];
      /* if end of args and still in list and there are non-option args then
         terminate list */
      if (arg == NULL && (optc == START_VALUE_LIST || optc == IN_VALUE_LIST)
          && first_nonoption_arg > -1) {
        /* terminate value list with @ */
        /* this is only needed for argument files */
        /* but is also good for show command line so command lines with lists
           can always be read back in */
        argcnt = insert_arg(&args, "@", first_nonoption_arg, 1);
        argn++;
        if (first_nonoption_arg > -1) {
          first_nonoption_arg++;
        }
      }

      arg = args[argn];
      if (arg && arg[0] == '@' && arg[1] == '\0') {
          /* inserted arguments terminator */
          optc = STOP_VALUE_LIST;
          continue;
      } else if (arg && arg[0] != '-') {  /* not option */
        /* - and -- are not allowed in value lists unless escaped */
        /* another value in value list */
        if ((*value = (char *)malloc(strlen(args[argn]) + 1)) == NULL) {
          oERR(ZE_MEM, "go.2");
        }
        strcpy(*value, args[argn]);
        break;

      } else {
        argn--;
        optc = THIS_ARG_DONE;
      }
    }

    /* move to next arg */
    if (optc == SKIP_VALUE_ARG2) {
      argn += 3;
      optc = 0;
    } else if (optc == SKIP_VALUE_ARG) {
      argn += 2;
      optc = 0;
    } else if (optc == THIS_ARG_DONE) {
      argn++;
      optc = 0;
    }
    if (argn > argcnt) {
      break;
    }
    if (args[argn] == NULL) {
      /* done unless permuting and non-option args */
      if (first_nonoption_arg > -1 && args[first_nonoption_arg]) {
        /* return non-option arguments at end */
        if (optc == NON_OPTION_ARG) {
          first_nonoption_arg++;
        }
        /* after first pass args are permuted but skipped over non-option args */
        /* swap so argn points to first non-option arg */
        j = argn;
        argn = first_nonoption_arg;
        first_nonoption_arg = j;
      }
      if (argn > argcnt || args[argn] == NULL) {
        /* done */
        option_ID = 0;
        break;
      }
    }

    /* after swap first_nonoption_arg points to end which is NULL */
    if (first_nonoption_arg > -1 && (args[first_nonoption_arg] == NULL)) {
      /* only non-option args left */
      if (optc == NON_OPTION_ARG) {
        argn++;
      }
      if (argn > argcnt || args[argn] == NULL) {
        /* done */
        option_ID = 0;
        break;
      }
      if ((*value = (char *)malloc(strlen(args[argn]) + 1)) == NULL) {
        oERR(ZE_MEM, "go.3");
      }
      strcpy(*value, args[argn]);
      optc = NON_OPTION_ARG;
      option_ID = o_NON_OPTION_ARG;
      break;
    }

    arg = args[argn];

    /* is it an option */
    if (arg[0] == '-') {
      /* option */
      if (arg[1] == '\0') {
        /* arg = - */
        /* treat like non-option arg */
        *option_num = o_NO_OPTION_MATCH;
        if (enable_permute) {
          /* permute args to move all non-option args to end */
          if (first_nonoption_arg < 0) {
            first_nonoption_arg = argn;
          }
          argn++;
        } else {
          /* not permute args so return non-option args when found */
          if ((*value = (char *)malloc(strlen(arg) + 1)) == NULL) {
            oERR(ZE_MEM, "go.4");
          }
          strcpy(*value, arg);
          optc = NON_OPTION_ARG;
          option_ID = o_NON_OPTION_ARG;
          break;
        }

      } else if (arg[1] == '-') {
        /* long option */
        if (arg[2] == '\0') {
          /* arg = -- */
          if (doubledash_ends_options) {
            /* Now -- stops permuting and forces the rest of
               the command line to be read verbatim - 7/25/04 EG */

            /* never permute args after -- and return as non-option args */
            if (first_nonoption_arg < 1) {
              /* -- is first non-option argument - 8/7/04 EG */
              argn--;
            } else {
              /* go back to start of non-option args - 8/7/04 EG */
              argn = first_nonoption_arg - 1;
            }

            /* disable permuting and treat remaining arguments as not
               options */
            read_rest_args_verbatim = 1;
            optc = READ_REST_ARGS_VERBATIM;

          } else {
            /* treat like non-option arg */
            *option_num = o_NO_OPTION_MATCH;
            if (enable_permute) {
              /* permute args to move all non-option args to end */
              if (first_nonoption_arg < 0) {
                first_nonoption_arg = argn;
              }
              argn++;
            } else {
              /* not permute args so return non-option args when found */
              if ((*value = (char *)malloc(strlen(arg) + 1)) == NULL) {
                oERR(ZE_MEM, "go.5");
              }
              strcpy(*value, arg);
              optc = NON_OPTION_ARG;
              option_ID = o_NON_OPTION_ARG;
              break;
            }
          }

        } else {
          option_ID = get_longopt(args, argn, &optc, negated, value, option_num, recursion_depth);

          if (option_ID == o_ARG_FILE_ERR) {
            /* unwind as only get this if recursion_depth > 0 */
            return option_ID;
          }
          break;
        }

      } else {
        /* short option */
        option_ID = get_shortopt(args, argn, &optc, negated, value, option_num, recursion_depth);

        if (option_ID == o_ARG_FILE_ERR) {
          /* unwind as only get this if recursion_depth > 0 */
          return option_ID;
        }

        if (optc == 0) {
          /* if optc = 0 then ran out of short opts this arg */
          optc = THIS_ARG_DONE;
        } else {
          break;
        }
      }

    /* argument files now allowed, so file names can't start with @ unless quoted
    */


    } else if (allow_arg_files && arg[0] == '@') {
      /* arg file */
#if 0
      oERR(ZE_PARMS, no_arg_files_err);
#endif
      char *argfilename = NULL;

#if 0
      printf(" {arg file '%s'}\n", arg);
#endif
      if (strlen(arg) > 1)
        argfilename = arg + 1;
      
      /* Any quotes around argfilename are handled by insert_args_from_file. */

      if (argfilename) {
        /*
         * Previous arg files code processed each arg file depth as it was
         * encountered.  So get_option() would be called as each arg file
         * was opened.
         *
         * insert_args_from_file() now takes care of any arg file recursion,
         * opening arg files as they are encountered.  The result is all
         * arguments found get inserted into args at this location in args
         * in the order found.  The permute code then needs to move them to
         * the appropriate spot based on if they are options or not.  This
         * means argument syntax is not checked until all arg file levels
         * are processed, though insert_args_from_file() does do some checking,
         * such as making sure double quotes match up.
         */
        /* dump_args("args before insert", args); */
        argcnt = insert_args_from_file(&args, argfilename, argn, recursion_depth);
        if (argcnt == -1) {
          sprintf(errbuf, "error processing arg file:  %s", argfilename);
          zipwarn(errbuf, "");
          oERR(ZE_PARMS, bad_arg_file_err);
        }
        /* dump_args("args after insert", args); */
      }

    } else {
      /* non-option */
      if (enable_permute) {
        /* permute args to move all non-option args to end */
        if (first_nonoption_arg < 0) {
          first_nonoption_arg = argn;
        }
        argn++;
      } else {
        /* no permute args so return non-option args when found */
        if ((*value = (char *)malloc(strlen(arg) + 1)) == NULL) {
          oERR(ZE_MEM, "go.6");
        }
        strcpy(*value, arg);
        *option_num = o_NO_OPTION_MATCH;
        optc = NON_OPTION_ARG;
        option_ID = o_NON_OPTION_ARG;
        break;
      }

    }
  }

  *pargs = args;
  *argc = argcnt;
  *first_nonopt_arg = first_nonoption_arg;
  *argnum = argn;
  *optchar = optc;

  return option_ID;
}
