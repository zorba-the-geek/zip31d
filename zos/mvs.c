/*
  Copyright (c) 1990-2014 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
 * MVS specific things
 */
#include "zip.h"
#include "mvs.h"

/* Notes on non-POSIX Partitioned Data Set (PDS) support
   - Al Dunsmuir, 2007-01-24

   The functions opendir, read and closedir in cmsmvs/mvs.c
   are analogs to the functions in unix/unix.c with the same
   names that process a POSIX directory.

   They use the special LE runtime mode of opening the PDS, and
   reading the raw PDS directory records.  The non-alias member
   names are extracted and converted into dirent nodes by the
   gen_node function.

   The dirent entries persist until the directory is fully
   processed.
 */

/* Function: gen_node

   Process the next directory record for the non-POSIX
   Partitioned Data Set (PDS) in the given DIR node,
   and generate a directory entry (dirent) node for each
   member described in the record.  Alias directory entries
   are ignored during processing.

   Return 1 on directory logical End-Of-File, 0 otherwise.
 */
static int gen_node(
                     DIR    *dirp,    /* PDS descriptor   */
                     RECORD *recptr ) /* PDS dir data rec */
{
  char           *ptr, *name, ttr[TTRLEN];
  int             skip, count = 2;
  unsigned int    info_byte;
  struct dirent  *new;

  ptr = recptr->rest;
  while (count < recptr->count) {
    if (memcmp( ptr, endmark, NAMELEN ) == 0) {
       return 1;                     /* End of members  */
    }

    name = ptr;                      /* Get member name */
    ptr += NAMELEN;

    memcpy( ttr, ptr, TTRLEN );      /* Get ttr name    */
    ptr += TTRLEN;

    info_byte = (unsigned int) (*ptr);     /* info byte */
    if ( !(info_byte & ALIAS_MASK) ) {     /* no alias  */
      new = malloc( sizeof(struct dirent) );
      new->d_next = NULL;
      if (dirp->D_list == NULL) /* First list entry? */
        dirp->D_list = new;
      else
        dirp->D_curpos->d_next = new;
      dirp->D_curpos = new;

      memcpy( new->d_name, name, NAMELEN );
      new->d_name[NAMELEN] = '\0';
      if ((name = strchr( new->d_name, ' ' )) != NULL) {
        *name = '\0';         /* skip trailing blanks */
      }
    }

    skip   = (info_byte & SKIP_MASK) * 2 + 1;
    ptr   += skip;
    count += (TTRLEN + NAMELEN + skip);
  }
  return 0;
}

/* Function: opendir

   Start processing to determine the members contained
   within the given non-POSIX Partitioned DataSet (PDS).

   Processing is comprised of:
   - Open the given PDS file.
   - Generate a PDS directory (DIR) to descibe the PDS.
   - Read the PDS directory data, and generate directory
     entry (dirent) nodes for each PDS member.
   - Close the PDS file.
   - Return the DIR describing the PDS & members.
 */
DIR *opendir(
             const char *dirname) /* _quoted_ PDS name */
{
  int      bytes, list_end = 0;
  DIR     *dirp;
  FILE    *fp;
  RECORD   rec;

  fp = fopen( dirname, "rb" );
  if (fp != NULL) {
    dirp = malloc( sizeof(DIR) );
    if (dirp != NULL) {
      dirp->D_list = dirp->D_curpos = NULL;
      strcpy( dirp->D_path, dirname );
      do {
        bytes = fread( &rec, 1, sizeof(rec), fp );
        if (bytes == sizeof(rec)) {
          list_end = gen_node( dirp, &rec );
        }
      } while (!feof(fp) && !list_end);
      fclose( fp );

      rewinddir( dirp );
      return dirp;
    }
    fclose( fp );
  }
  return NULL;
}


/* Function: readdir

   Read the given PDS's current directory entry (dirent)
   node.  Advance to the next node, and return this node.

   If already at the end of the list, returns NULL.
 */
struct dirent *readdir(
                       DIR  *dirp ) /* PDS descriptor   */
{
  struct dirent *cur;

  cur            = dirp->D_curpos;
  dirp->D_curpos = dirp->D_curpos->d_next;

  return cur;
}


/* Function: rewinddir

   Reset the given PDS's current directory entry (dirent)
   node pointer to the first node.
 */
void rewinddir(
               DIR  *dirp ) /* PDS descriptor   */
{
  dirp->D_curpos = dirp->D_list;
}


/* Function: closedir

   Complete directory processing for the given non-POSIX
   Partitioned DataSet (PDS).

   Free the associated list of directory entry nodes, and
   the DIR descibing the PDS.
 */
int closedir(
             DIR  *dirp ) /* PDS descriptor   */
{
  struct dirent *node;

  while (dirp->D_list != NULL) {
    node = dirp->D_list;
    dirp->D_list = dirp->D_list->d_next;
    free( node );
  }
  free( dirp );

  return 0;
}


/* Function: readd

   Return the next member name for the given PDS, or NULL
   if there are no more PDS members or an error occurs.
 */
local char *readd(
                  DIR  *d ) /* PDS descriptor   */
{
  struct dirent *e;

  e = readdir(d);
  if (e == NULL)
    return NULL;
  else
    return e->d_name;
}

/* Function: procname

   Process a name or sh expression to operate on (or exclude).
   Return an error code in the ZE_ class.

   z/OS platforms supports 2 distinct classes of files:
     POSIX     - standard Unix files, in HFS or ZFS filesystems
     non-POSIX - traditional MVS file or datasets

   Files may be specified by name, or indirectly through a
   "Data Definition" (DD) name from JCL or dynamic allocation.
   The latter use the "DD:ddname" convention... and to make
   things more complex, DDs may refer to POSIX files.

   non-POSIX files may have an optional "//" prefix.  If the file
   name is quoted with single-quotes, it is a fully qualified
   name, otherwise the first qualifier corresponds to the current
   prefix (which is variable).

   Files passed via DDs and non-POSIX files require processing
   with fopen, fldata and fclose to determine the exact file
   characteristics.
*/
int procname(
             char *n,        /* name to process                    */
             int   caseflag) /* true to force case-sensitive match */
{
  char     *p;             /* path and name for recursion     */
  DIR      *d;             /* directory stream from opendir() */
  FILE     *f;             /* FILE* from fopen()              */
  fldata_t  finfo;         /* file info, from fldata()        */
  char     *fname;         /* file name, from fldata()        */
  char     *fn;            /* quoted copy of fldata.__dsname  */
  char     *t;             /* shortened name                  */
  char     *e;             /* pointer to name from readd()    */
  int       fnlen;         /* Length of fn string             */
  int       m;             /* matched flag                    */
  struct    zlist far *z;  /* steps through zfiles list       */
  int       rc;            /* return code                     */

  if (strcmp(n, "-") == 0) {
    /* A: compressing stdin                                 */
    return newname(n, 0, caseflag);
  }

  f = fopen(n, "r");          /* Open file to allow fldata   */
  if (f) {                    /* File exists, opens OK       */
    fname = NULL;             /* Indicate fname not required */
    rc = fldata(f, fname, &finfo); /* Get file info          */
    fclose(f);                /* Close the file              */

    if (rc == 0) {            /* fldata() retrieved info OK? */
      /* Make a quoted copy of the file name                 */
      if ((fn = malloc(strlen(finfo.__dsname)+2+1)) == NULL)
        return ZE_MEM;
      strcpy( fn, "'" );
      strcat( fn, finfo.__dsname );
      strcat( fn, "'" );

      /* Now determine if the file is supported              */
      if (finfo.__dsorgPS) {  /* Sequential MVS file         */
        Trace((stderr,
               "zip diagnostic: PS DSN=%s\n",
               fn));

        /* add or remove name of file */

        if ((m = newname(fn, 0, caseflag)) != ZE_OK) {
          Trace((stderr,
                 "zip diagnostic: Add failed with m=%d\n",n));
          free(fn);
          return m;
        }
      }

      else if (finfo.__dsorgPO) {  /* Partitioned MVS file   */
        Trace((stderr,
               "zip diagnostic: PO DSN=%s\n",
               fn));

        if (finfo.__dsorgPDSmem) { /* PDS member?            */
          /* Treat dsn(member) just like sequential file     */
          if ((m = newname(fn, 0, caseflag)) != ZE_OK) {
            Trace((stderr,
                   "zip diagnostic: Add failed with m=%d\n",n));
            free(fn);
            return m;
          }
        }

        else {                     /* No PDS member spec'd   */
          /* Process PDS like a *IX directory                */
          /* - Do not encode PDS name as directory, through  */
          /*   otherwise -j is mandatory                     */

          /* Allocate buffer for "'dsn(member)'"             */
          fnlen = strlen(fn);
          if ((p = malloc(strlen(fn)+8+2)) == NULL) {
            free(fn);
            return ZE_MEM;
          }

          /* recurse into directory                          */
          if (recurse) {
            /* Create "'dsn'"                                */
            strcpy(p, fn);                 /* 'dsn'          */

            /* Convert directory entries                     */
            if ((d = opendir(p))!= NULL) {
              while ((e = readd(d)) != NULL) {
                /* Create "'dsn(member)'"                    */
                strcpy(p, fn);             /* 'dsn'          */
                p[fnlen-1]= '\0';          /* Kill last "'"  */
                strcat(p, "(");            /*     (          */
                strcat(p, e);              /*      member    */
                strcat(p, ")");            /*            )   */
                strcat(p, "'");            /*             '  */

                /* Recurse on "'dsn(member)'"                */
                if ((m = procname(p, caseflag)) != ZE_OK) {
                  if (m == ZE_MISS)
                    zipwarn("name not matched: ", p);
                  else
                    ziperr(m, p);
                }
              }
              closedir(d);
            }
          }

          /* Free buffer for "'dsn(member)'"                 */
          free(p);
        }
      }

      else {                       /* Not supported MVS file */
        if (strcmp(n, fn) != 0) {  /* Need both?             */
          fprintf(mesg,
                  "Input file name %s\n", n);
        }
        fprintf(mesg,
                "File %s is not PO or PS MVS file\n",
                fn);
        ziperr(ZE_OPEN, "Unsupported file type");
      }

      free(fn);
    }

    else {                    /* fldata() failed             */
      Trace((stderr,
             "zip diagnostic: fldata failed (%d) for file=%s\n",
             rc,
             n));

    }

  }

  else {
    /* Not a file or directory--search for shell expression in zip file */
    p = ex2in(n, 0, (int *)NULL);       /* shouldn't affect matching chars */
    m = 1;
    for (z = zfiles; z != NULL; z = z->nxt) {
      if (MATCH(p, z->iname, caseflag)) {
        z->mark = pcount ? filter(z->zname, caseflag) : 1;
        if (verbose) {
          fprintf(mesg,
                  "zip diagnostic: %scluding %s\n",
                  z->mark ? "in" : "ex",
                  z->name);
        }
        m = 0;
      }
    }
    free((zvoid *)p);
    return m ? ZE_MISS : ZE_OK;
  }

  return ZE_OK;
}
