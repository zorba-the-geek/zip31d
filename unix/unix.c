/*
  unix/unix.c - Zip 3

  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
#include "zip.h"

#ifndef UTIL    /* the companion #endif is a bit of ways down ... */

#include <time.h>
#ifdef ENABLE_ENTRY_TIMING
# include <sys/time.h>
#endif
#if defined(MINIX) || defined(__mpexl)
#  ifdef S_IWRITE
#    undef S_IWRITE
#  endif /* S_IWRITE */
#  define S_IWRITE S_IWUSR
#endif /* MINIX */

#if (!defined(S_IWRITE) && defined(S_IWUSR))
#  define S_IWRITE S_IWUSR
#endif

#if defined(HAVE_DIRENT_H) || defined(_POSIX_VERSION)
#  include <dirent.h>
#else /* !HAVE_DIRENT_H */
#  ifdef HAVE_NDIR_H
#    include <ndir.h>
#  endif /* HAVE_NDIR_H */
#  ifdef HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif /* HAVE_SYS_NDIR_H */
#  ifdef HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif /* HAVE_SYS_DIR_H */
#  ifndef dirent
#    define dirent direct
#  endif
#endif /* HAVE_DIRENT_H || _POSIX_VERSION */

#ifdef __APPLE__
#  include "unix/macosx.h"
#endif /* def __APPLE__ */


#define PAD 0
#define PATH_END '/'

/* Library functions not in (most) header files */

#ifdef _POSIX_VERSION
#  include <utime.h>
#else
   int utime OF((char *, time_t *));
#endif

local ulg label_time = 0;
local ulg label_mode = 0;
local time_t label_utim = 0;

/* Local functions */
local char *readd OF((DIR *));

#ifdef __APPLE__

/* get_apl_dbl_info()
 * Look for data which require an AppleDouble ("._name") file.
 * Return zero if any are found.  Qualifying data:
 *    Non-zero Finder info
 *    Resource fork (size > 0)
 *    Extended attributes (non-null list, where available)
 */
static int get_apl_dbl_info( char *name)
{
  int sts;
  int ret = 1;                  /* Assume failure. */

  /* Attribute request structures for getattrlist(). */
  struct attrlist attr_list_fndr;
  struct attrlist attr_list_rsrc;

  /* Attribute buffer structures for getattrlist(). */
  attr_bufr_fndr_t attr_bufr_fndr;
  attr_bufr_rsrc_t attr_bufr_rsrc;

  /* Clear attribute list structure. */
  memset( &attr_list_fndr, 0, sizeof( attr_list_fndr));
  /* Set attribute list bits for object type and Finder info. */
  attr_list_fndr.bitmapcount = ATTR_BIT_MAP_COUNT;
  attr_list_fndr.commonattr = ATTR_CMN_OBJTYPE| ATTR_CMN_FNDRINFO;

  /* Get file type and Finder info. */
  sts = getattrlist( name,                      /* Path. */
                     &attr_list_fndr,           /* Attrib list. */
                     &attr_bufr_fndr,           /* Dest buffer. */
                     sizeof( attr_bufr_fndr),   /* Dest buffer size. */
                     0);                        /* Options. */

  /* Continue processing if it's a regular file. */
  if ((sts == 0) && (attr_bufr_fndr.obj_type == VREG))
  {
    /* Bytewise OR Finder info data to see if all zero. */
    int fior;
    int i;

    fior = 0;
    for (i = 0; i < 32; i++)
      fior |= attr_bufr_fndr.fndr_info[ i];

    if (fior != 0)
    {
      /* Non-zero Finder info. */
      ret = 0;
    }
    else
    {
      /* Check resource fork size. */
      /* Clear attribute list structure. */
      memset( &attr_list_rsrc, 0, sizeof( attr_list_rsrc));
      /* Set attribute list bits for resource fork size. */
      attr_list_rsrc.bitmapcount = ATTR_BIT_MAP_COUNT;
      attr_list_rsrc.fileattr = ATTR_FILE_RSRCLENGTH;

      sts = getattrlist( name,                      /* Path. */
                         &attr_list_rsrc,           /* Attrib list. */
                         &attr_bufr_rsrc,           /* Dest buffer. */
                         sizeof( attr_bufr_rsrc),   /* Dest buffer size. */
                         0);                        /* Options. */

      if ((sts == 0) && (attr_bufr_rsrc.size > 0))
      {
        /* Non-zero-size resource fork. */
        ret = 0;
      }
# ifdef APPLE_XATTR
      else
      {
        /* Check extended attribute list length.
         * Note: If we decide to filter extended attributes, then a more
         * complex test would be needed here.
         */
        sts = listxattr( name,                  /* Real file name. */
                         NULL,                  /* Name list buffer. */
                         0,                     /* Name list buffer size. */
                         XATTR_NOFOLLOW);       /* Options. */

        if (sts > 0)
        {
          /* Non-null list of extended attributes. */
          ret = 0;
        }
      }
# endif /* def APPLE_XATTR */
    }
  }
  return ret;
}


unsigned char *apl_dbl_hdr;             /* AppleDouble header buffer. */
int apl_dbl_hdr_alloc;                  /* Allocated size of apl_dbl_hdr. */

# ifdef APPLE_XATTR
char *apl_dbl_xattr_ignore[] =
 { "com.apple.FinderInfo", "com.apple.ResourceFork", NULL };
# endif /* def APPLE_XATTR */



int make_apl_dbl_header( char *name, int *hdr_size)
{

  char btrbslash;               /* Saved character had better be a slash. */
  int j;                        /* Misc. counter. */
  int sts;
  struct attrlist attr_list_fndr;
  struct attrlist attr_list_rsrc;

  attr_bufr_fndr_t attr_bufr_fndr;
  attr_bufr_rsrc_t attr_bufr_rsrc;

# ifdef APPLE_XATTR
#  define APL_DBL_OFS_RSRC_FORK (xah_val_ofs+ xav_len)
  char *xan_buf;                                /* Attribute name storage. */
  char *xan_ptr;                                /* Attribute name pointer. */
  unsigned short xa_cnt = 0;                    /* Attribute entry count. */
  int xah_val_ofs = APL_DBL_SIZE_HDR;           /* Offset to value storage. */
  int xan_len = 0;                              /* Attr name list size. */
  int xav_len = 0;                              /* Attr value storage size. */
  int xav_ofs;                                  /* Offset to value. */
# else /* def APPLE_XATTR */
#  define APL_DBL_OFS_RSRC_FORK APL_DBL_SIZE_HDR
# endif /* def APPLE_XATTR [else] */

  *hdr_size = 0;                        /* Clear the returned header size. */

# ifdef APPLE_XATTR
    /* Get AppleDouble header extended attribute storage requirement.
     * Fixed:
     *        2: Alignment (4) pad
     *        4: Extended attribute magic
     *        4: File ID
     *        4: Total size
     *        4: Values offset
     *        4: Values total size
     *       12: Reserved
     *        2: Flags
     *        2: Attribute count
     * Per Entry (align 4):
     *            4: Value offset
     *            4: Value Size
     *            2: Flags
     *            1: Attribute name size (including NUL terminator)
     *            ?: Attribute name (NUL-terminated)
     *            ?: Alignment pad [0-3]
     *        ?: Value storage
     *
     * Per-Entry size = (14+ name_size)& 0xfffffffc
     */

    /* Get extended attribute name list length. */
    sts = listxattr( name,                      /* Real file name. */
                     NULL,                      /* Name list buffer. */
                     0,                         /* Name list buffer size. */
                     XATTR_NOFOLLOW);           /* Options. */
    if (sts < 0)
    {
      return ZE_OPEN;
    }
    else
    {
      /* Allocate storage for the extended attribute name list. */
      xan_len = sts;
      if (xan_len > 0)
      {
        xan_buf = malloc( xan_len);
        if (xan_buf == NULL)
        {
          return ZE_MEM;
        }
        else
        {
          /* Get the extended attribute name list. */
          sts = listxattr( name,                  /* Real file name. */
                           xan_buf,               /* Name list buffer. */
                           xan_len,               /* Name list buffer size. */
                           XATTR_NOFOLLOW);       /* Options. */
          if (sts >= 0)
          {
            xah_val_ofs += APL_DBL_SIZE_ATTR_HDR;     /* ATTR hdr, fixed. */
            /* Accumulate extended attribute storage requirements. */
            xan_ptr = xan_buf;              /* Attribute name pointer. */
            while (xan_ptr < xan_buf+ xan_len)
            {
              sts = 0;
              for (j = 0; apl_dbl_xattr_ignore[ j] != NULL; j++)
              {
                if (strcmp( apl_dbl_xattr_ignore[ j], xan_ptr) == 0)
                {
                  sts = 1;
                  break;
                }
              }

              if (sts == 0)
              {
                xa_cnt++;                       /* Count the attributes. */
                sts = getxattr( name,           /* Real file name. */
                                xan_ptr,        /* Attribute name. */
                                NULL,           /* Attribute value. */
                                0,              /* Attribute value size. */
                                0,              /* Position. */
                                XATTR_NOFOLLOW);    /* Options. */
                if (sts > 0)
                {
                  xav_len += sts;       /* Increment value storage size. */
                }
                /* Increment value storage offset by xattr entry size.
                 * 4 [value offset] + 4 [value size] + 2 [flags] +
                 * 1 [name size (incl NUL)] + name_size =
                 * 11 + strlen( name) + 1 [NUL] = 12 + strlen( name).
                 * Then add 3, and mask off the low two bits to get
                 * the next align-4 offset.
                 */
                xah_val_ofs += (15+ strlen( xan_ptr))& 0xfffffffc;
              } /* if (sts == 0) */
              xan_ptr += strlen( xan_ptr)+ 1;   /* Next attr name. */
            } /* while */
          } /* if (sts{listxattr()} >= 0) */
        } /* if (xan_buf == NULL) [else] */
      } /* if (xan_len > 0) */
    } /* if (sts{listxattr()} < 0) [else] */
# endif /* def APPLE_XATTR */

    /* Allocate AppleDouble header buffer, if needed. */
    if (apl_dbl_hdr_alloc < APL_DBL_OFS_RSRC_FORK)
    {
      apl_dbl_hdr = realloc( apl_dbl_hdr, APL_DBL_OFS_RSRC_FORK);
      if (apl_dbl_hdr == NULL)
      {
        return ZE_MEM;
      }
      apl_dbl_hdr_alloc = APL_DBL_OFS_RSRC_FORK;
    }
    /* Set the fake-data buffer pointer to the allocated atorage. */
    file_read_fake_buf = apl_dbl_hdr;

    /* Get object type and Finder info. */
    /* Clear attribute list structure. */
    memset( &attr_list_fndr, 0, sizeof( attr_list_fndr));
    /* Set attribute list bits for object type and Finder info. */
    attr_list_fndr.bitmapcount = ATTR_BIT_MAP_COUNT;
    attr_list_fndr.commonattr = ATTR_CMN_OBJTYPE| ATTR_CMN_FNDRINFO;

    /* Get file type and Finder info. */
    sts = getattrlist( name,                            /* Path. */
                       &attr_list_fndr,                 /* Attrib list. */
                       &attr_bufr_fndr,                 /* Dest buffer. */
                       sizeof( attr_bufr_fndr),         /* Dest buffer size. */
                       FSOPT_NOFOLLOW);                 /* Options. */

    if ((sts != 0) || (attr_bufr_fndr.obj_type != VREG))
    {
      ZIPERR( ZE_OPEN, "getattrlist(fndr) failure");
    }
    else
    {
      /* Get resource fork size. */
      /* Clear attribute list structure. */
      memset( &attr_list_rsrc, 0, sizeof( attr_list_rsrc));
      /* Set attribute list bits for resource fork size. */
      attr_list_rsrc.bitmapcount = ATTR_BIT_MAP_COUNT;
      attr_list_rsrc.fileattr = ATTR_FILE_RSRCLENGTH;

      sts = getattrlist( name,                        /* Real file name. */
                         &attr_list_rsrc,             /* Attrib list. */
                         &attr_bufr_rsrc,             /* Dest buffer. */
                         sizeof( attr_bufr_rsrc),     /* Dest buffer size. */
                         FSOPT_NOFOLLOW);             /* Options. */
      if (sts != 0)
      {
        ZIPERR( ZE_OPEN, "getattrlist(rsrc) failure");
      }
      else
      {
        /* Move Finder info into AppleDouble header buffer. */
        memcpy( &apl_dbl_hdr[ APL_DBL_OFS_FNDR_INFO],
        attr_bufr_fndr.fndr_info, APL_DBL_SIZE_FNDR_INFO);
        /* Set fake I/O buffer size. */
        file_read_fake_len = APL_DBL_OFS_RSRC_FORK;
      }
    }

    /* AppleDouble header fixed/known elements. */
    HOST32_TO_BIGC( apl_dbl_hdr+ APL_DBL_OFS_MAGIC,
     APL_DBL_HDR_MAGIC);                                        /* Magic. */
    HOST32_TO_BIGC( apl_dbl_hdr+ APL_DBL_OFS_VERSION,
     APL_DBL_HDR_VERSION);                                      /* Version. */
    memcpy( apl_dbl_hdr+ APL_DBL_OFS_FILE_SYS,
     APL_DBL_HDR_FILE_SYS,                                      /* FS name. */
     (sizeof( APL_DBL_HDR_FILE_SYS)- 1));
    HOST16_TO_BIGC( apl_dbl_hdr+ APL_DBL_OFS_ENT_CNT, 2);       /* Entry cnt. */
    /* Note: Alignment is now off by 2. */

    /* AppleDouble entry 0: Finder info.  (Includes extended attributes.) */
    HOST32_TO_BIGC( apl_dbl_hdr+ APL_DBL_OFS_ENT_ID0,
     APL_DBL_HDR_EID_FI);                                       /* Ent ID 0. */
    HOST32_TO_BIGC( apl_dbl_hdr+ APL_DBL_OFS_ENT_OFS0,
     APL_DBL_OFS_FNDR_INFO);                                    /* Ent ofs 0. */
    /* Finder info size = real Finder info size + ext attr size. */
    HOST32_TO_BIGC( apl_dbl_hdr+ APL_DBL_OFS_ENT_LEN0,
     APL_DBL_OFS_RSRC_FORK- APL_DBL_OFS_FNDR_INFO);             /* Ent len 0. */

    /* AppleDouble entry 1: Resource fork. */
    HOST32_TO_BIGC( apl_dbl_hdr+ APL_DBL_OFS_ENT_ID1,
     APL_DBL_HDR_EID_RF);                                       /* Ent ID 1. */
    HOST32_TO_BIGC( apl_dbl_hdr+ APL_DBL_OFS_ENT_OFS1,
     APL_DBL_OFS_RSRC_FORK);                                    /* Ent ofs 1. */
    HOST32_TO_BIGC( apl_dbl_hdr+ APL_DBL_OFS_ENT_LEN1,          /* Ent len 1. */
     attr_bufr_rsrc.size);

# ifdef APPLE_XATTR
    if (xan_len > 0)
    {
      int ofs = APL_DBL_SIZE_HDR;   /* After end of Finder info. */
      struct stat ax_stat;          /* lstat() buffer (for file ID). */

      sts = lstat( name, &ax_stat);
      if (sts != 0)
      {
        ax_stat.st_ino = 0;         /* If lstat() fails, use ID = 0. */
      }

      /* AppleDouble extended attributes fixed/known elements. */
      /* Two-byte alignment (4) pad. */
      HOST16_TO_BIGC( apl_dbl_hdr+ ofs, 0);
      ofs += 2;

      /* AppleDouble extended attributes magic. */
      memcpy( apl_dbl_hdr+ ofs, APL_DBL_XA_MAGIC,
       (sizeof( APL_DBL_XA_MAGIC)- 1));
      ofs += sizeof( APL_DBL_XA_MAGIC)- 1;

      /* File ID.  ("For debug", so not crucial.*/
      HOST32_TO_BIGC( apl_dbl_hdr+ ofs, ax_stat.st_ino);
      ofs += 4;

      /* Total size (Finder info + extended attributes). */
      HOST32_TO_BIGC( apl_dbl_hdr+ ofs, APL_DBL_OFS_RSRC_FORK);
      ofs += 4;

      /* Offset to value storage. */
      HOST32_TO_BIGC( apl_dbl_hdr+ ofs, xah_val_ofs);
      ofs += 4;

      /* Size of value storage. */
      HOST32_TO_BIGC( apl_dbl_hdr+ ofs, xav_len);
      ofs += 4;

      /* Reserved (12 byte). */
      memset( apl_dbl_hdr+ ofs, 0, 12);
      ofs += 12;

      /* Flags. */
      HOST16_TO_BIGC( apl_dbl_hdr+ ofs, 0);
      ofs += 2;

      /* Attribute count. */
      HOST16_TO_BIGC( apl_dbl_hdr+ ofs, xa_cnt);
      ofs += 2;

      /* Loop through attributes.  Store per-entry info.
       * Get/store attribute values.
       */
      xan_ptr = xan_buf;                /* Attribute name pointer. */
      xav_ofs = xah_val_ofs;            /* Value offset. */
      while (xan_ptr < xan_buf+ xan_len)
      {
        /* Recognize attributes which we don't want. */
        sts = 0;
        for (j = 0; apl_dbl_xattr_ignore[ j] != NULL; j++)
        {
          if (strcmp( apl_dbl_xattr_ignore[ j], xan_ptr) == 0)
          {
            sts = 1;
            break;
          }
        }

        if (sts == 0)
        {
          /* Not one of the unwanted attributes.  Process it. */
          int nam_len;
          int val_len;

          /* We don't remember each value size from its earlier
           * determination, so give it the whole remaining value space,
           * which is the total allocation less the offset of the
           * currently sought value).
           */
          sts = getxattr( name,                          /* Real file name. */
                          xan_ptr,                       /* Attribute name. */
                          (apl_dbl_hdr+ xav_ofs),        /* Attribute value. */
                          (apl_dbl_hdr_alloc- xav_ofs),  /* Attr value size. */
                          0,                             /* Position. */
                          XATTR_NOFOLLOW);               /* Options. */
          if (sts >= 0)
          {
            val_len = sts;

            /* Offset to value. */
            HOST32_TO_BIGC( apl_dbl_hdr+ ofs, xav_ofs);
            xav_ofs += val_len;                 /* Increment value offset. */
            ofs += 4;

            /* Size of value. */
            HOST32_TO_BIGC( apl_dbl_hdr+ ofs, val_len);
            ofs += 4;

            /* Flags. */
            HOST16_TO_BIGC( apl_dbl_hdr+ ofs, 0);
            ofs += 2;

            /* Attribute name size (including NUL terminator). */
            nam_len = strlen( xan_ptr) + 1;

            *(apl_dbl_hdr+ ofs) = nam_len;
            ofs += 1;

            /* Attribute name (including NUL terminator). */
            memcpy( apl_dbl_hdr+ ofs, xan_ptr, nam_len);
            ofs += nam_len;

            /* Alignment (4) pad. */
            nam_len = ((ofs+ 3)& 0xfffffffc) - ofs;
            if (nam_len > 0)
            {
              memset( apl_dbl_hdr+ ofs, 0, nam_len);
              ofs += nam_len;
            }
          }
        } /* if (sts == 0) */
        xan_ptr += strlen( xan_ptr)+ 1;     /* Next attr name. */
      } /* while */
    }
# endif /* def APPLE_XATTR */

  *hdr_size = APL_DBL_OFS_RSRC_FORK;    /* Set the returned header size. */

  return ZE_OK;
}
#endif /* def __APPLE__ */


#ifdef NO_DIR                    /* for AT&T 3B1 */
#include <sys/dir.h>
#ifndef dirent
#  define dirent direct
#endif
typedef FILE DIR;
/*
**  Apparently originally by Rich Salz.
**  Cleaned up and modified by James W. Birdsall.
*/

#define opendir(path) fopen(path, "r")

struct dirent *readdir(dirp)
DIR *dirp;
{
  static struct dirent entry;

  if (dirp == NULL)
    return NULL;
  for (;;)
    if (fread (&entry, sizeof (struct dirent), 1, dirp) == 0)
      return NULL;
    else if (entry.d_ino)
      return (&entry);
} /* end of readdir() */

#define closedir(dirp) fclose(dirp)
#endif /* NO_DIR */


local char *readd(d)
DIR *d;                 /* directory stream to read from */
/* Return a pointer to the next name in the directory stream d, or NULL if
   no more entries or an error occurs. */
{
  struct dirent *e;

  e = readdir(d);
  return e == NULL ? (char *) NULL : e->d_name;
}


int procname(n, caseflag)
char *n;                /* name to process */
int caseflag;           /* true to force case-sensitive match */
/* Process a name or sh expression to operate on (or exclude).  Return
   an error code in the ZE_ class. */
{
  char *a;              /* path and name for recursion */
  DIR *d;               /* directory stream from opendir() */
  char *e;              /* pointer to name from readd() */
  int m;                /* matched flag */
  char *p;              /* path for recursion */
  z_stat s;             /* result of stat() */
  struct zlist far *z;  /* steps through zfiles list */

  if (strcmp(n, "-") == 0) { /* if compressing stdin */
    return newname(n, 0, caseflag);
  }
  else if (LSSTAT(n, &s))
  {
    /* Not a file or directory--search for shell expression in zip file */
    p = ex2in(n, 0, (int *)NULL);       /* shouldn't affect matching chars */
    m = 1;
    for (z = zfiles; z != NULL; z = z->nxt) {
      if (MATCH(p, z->iname, caseflag))
      {
        z->mark = pcount ? filter(z->zname, caseflag) : 1;
        if (verbose)
            zfprintf(mesg, "zip diagnostic: %scluding %s\n",
               z->mark ? "in" : "ex", z->name);
        m = 0;
#if 0
        zprintf(" {in procname:  match 1 (%s)}", z->name);
#endif
      }
    }

#ifdef UNICODE_SUPPORT
    if (m) {
      /* also check de-escaped Unicode name */
      char *pu = escapes_to_utf8_string(p);
#if 0
      zprintf(" {in procname:  pu (%s)}", pu);
#endif

      for (z = zfiles; z != NULL; z = z->nxt) {
        if (z->zuname) {
          if (MATCH(pu, z->zuname, caseflag))
          {
            z->mark = pcount ? filter(z->zuname, caseflag) : 1;
            if (verbose) {
                zfprintf(mesg, "zip diagnostic: %scluding %s\n",
                   z->mark ? "in" : "ex", z->oname);
                zfprintf(mesg, "     Escaped Unicode:  %s\n",
                   z->ouname);
            }
            m = 0;
#if 0
            zprintf(" {in procname:  match 2 (%s)}", z->zuname);
#endif
          }
        }
      } /* for */
      free(pu);
    }
#endif

    free((zvoid *)p);
    return m ? ZE_MISS : ZE_OK;
  }

#if 0
  zprintf(" {in procname:  live name}");
#endif

  /* Live name.  Recurse if directory.  Use if file or symlink (or fifo?). */
  if (S_ISREG(s.st_mode) || S_ISLNK(s.st_mode))
  {
    /* Regular file or symlink.  Add or remove name of file. */
    if ((m = newname(n, 0, caseflag)) != ZE_OK)
      return m; 

#ifdef __APPLE__

    /* If saving AppleDouble files, process one for this file. */
    if (data_fork_only <= 0)
    {
      /* Check for non-null Finder info and resource fork. */
      m = get_apl_dbl_info( n);
      if (m == 0)
      {
        /* Process the AppleDouble file. */
        if ((m = newname(n, ZFLAG_APLDBL, caseflag)) != ZE_OK)
          return m;
      }
    }
#endif /* def __APPLE__ */

  } /* S_ISREG( s.st_mode) || S_ISLNK( s.st_mode) */
  else if (S_ISDIR(s.st_mode))
  {
    /* Directory.  Add trailing / to the directory name. */
    if ((p = malloc(strlen(n)+2)) == NULL)
      return ZE_MEM;
    if (strcmp(n, ".") == 0) {
      *p = '\0';  /* avoid "./" prefix and do not create zip entry */
    } else {
      strcpy(p, n);
      a = p + strlen(p);
      if (a[-1] != '/')
        strcpy(a, "/");
      if (dirnames && (m = newname(p, ZFLAG_DIR, caseflag)) != ZE_OK) {
        free((zvoid *)p);
        return m;
      }
    }
    /* recurse into directory */
    if (recurse && (d = opendir(n)) != NULL)
    {
      while ((e = readd(d)) != NULL) {
        if (strcmp(e, ".") && strcmp(e, ".."))
        {
          if ((a = malloc(strlen(p) + strlen(e) + 1)) == NULL)
          {
            closedir(d);
            free((zvoid *)p);
            return ZE_MEM;
          }
          strcat(strcpy(a, p), e);
          if ((m = procname(a, caseflag)) != ZE_OK)   /* recurse on name */
          {
            if (m == ZE_MISS)
              zipwarn("name not matched: ", a);
            else
              ziperr(m, a);
          }
          free((zvoid *)a);
        }
      }
      closedir(d);
    }
    free((zvoid *)p);
  } /* S_ISDIR( s.st_mode) [else if] */
  else if (S_ISFIFO(s.st_mode))
  {
    /* FIFO (Named Pipe) - handle as normal file by adding
     * name of FIFO.  As of Zip 3.1, a named pipe is always
     * included.  Zip will stop if FIFO is open and wait for
     * pipe to be fed and closed, but only if -FI.
     *
     * Skipping read of FIFO is now done in zipup().
     */
    if (noisy) {
      if (allow_fifo)
        zipwarn("reading FIFO (Named Pipe): ", n);
      else
        zipwarn("skipping read of FIFO (Named Pipe) - use -FI to read: ", n);
    }
    if ((m = newname(n, ZFLAG_FIFO, caseflag)) != ZE_OK)
      return m;
  } /* S_ISFIFO( s.st_mode) [else if] */
  else
  {
    zipwarn("ignoring special file: ", n);
  } /* S_IS<whatever>( s.st_mode) [else] */
  return ZE_OK;
}

char *ex2in(x, isdir, pdosflag)
char *x;                /* external file name */
int isdir;              /* input: x is a directory */
int *pdosflag;          /* output: force MSDOS file attributes? */
/* Convert the external file name to a zip file name, returning the malloc'ed
   string or NULL if not enough memory. */
{
  char *n;              /* internal file name (malloc'ed) */
  char *t = NULL;       /* shortened name */
  int dosflag;

  dosflag = dosify;     /* default for non-DOS and non-OS/2 */

  /* Find starting point in name before doing malloc */
  /* Strip "//host/share/" part of a UNC name */
  if (!strncmp(x,"//",2) && (x[2] != '\0' && x[2] != '/')) {
    n = x + 2;
    while (*n != '\0' && *n != '/')
      n++;              /* strip host name */
    if (*n != '\0') {
      n++;
      while (*n != '\0' && *n != '/')
        n++;            /* strip `share' name */
    }
    if (*n != '\0')
      t = n + 1;
  } else
      t = x;
  while (*t == '/')
    t++;                /* strip leading '/' chars to get a relative path */
  while (*t == '.' && t[1] == '/')
    t += 2;             /* strip redundant leading "./" sections */

  /* Make changes, if any, to the copied name (leave original intact) */
  if (!pathput)
    t = last(t, PATH_END);

  /* Malloc space for internal name and copy it */
  if ((n = malloc(strlen(t) + 1)) == NULL)
    return NULL;
  strcpy(n, t);

  if (dosify)
    msname(n);

#ifdef EBCDIC
  strtoasc(n, n);       /* here because msname() needs native coding */
#endif

  /* Returned malloc'ed name */
  if (pdosflag)
    *pdosflag = dosflag;

  if (isdir) return n;  /* avoid warning on unused variable */
  return n;
}

char *in2ex(n)
char *n;                /* internal file name */
/* Convert the zip file name to an external file name, returning the malloc'ed
   string or NULL if not enough memory. */
{
  char *x;              /* external file name */

  if ((x = malloc(strlen(n) + 1 + PAD)) == NULL)
    return NULL;
#ifdef EBCDIC
  strtoebc(x, n);
#else
  strcpy(x, n);
#endif
  return x;
}

/*
 * XXX use ztimbuf in both POSIX and non POSIX cases ?
 */
void stamp(f, d)
char *f;                /* name of file to change */
ulg d;                  /* dos-style time to change it to */
/* Set last updated and accessed time of file f to the DOS time d. */
{
#ifdef _POSIX_VERSION
  struct utimbuf u;     /* argument for utime()  const ?? */
#else
  time_t u[2];          /* argument for utime() */
#endif

  /* Convert DOS time to time_t format in u */
#ifdef _POSIX_VERSION
  u.actime = u.modtime = dos2unixtime(d);
  utime(f, &u);
#else
  u[0] = u[1] = dos2unixtime(d);
  utime(f, u);
#endif

}

ulg filetime(f, a, n, t)
  char *f;                /* name of file to get info on */
  ulg *a;                 /* return value: file attributes */
  zoff_t *n;              /* return value: file size */
  iztimes *t;             /* return value: access, modific. and creation times */
/* If file *f does not exist, return 0.  Else, return the file's last
   modified date and time as an MSDOS date and time.  The date and
   time is returned in a long with the date most significant to allow
   unsigned integer comparison of absolute times.  Also, if a is not
   a NULL pointer, store the file attributes there, with the high two
   bytes being the Unix attributes, and the low byte being a mapping
   of that to DOS attributes.  If n is not NULL, store the file size
   there.  If t is not NULL, the file's access, modification and creation
   times are stored there as UNIX time_t values.
   If f is "-", use standard input as the file. If f is a device, return
   a file size of -1 */
{
  z_stat s;         /* results of stat() */
  /* converted to pointer from using FNMAX - 11/8/04 EG */
  char *name;
  int len = strlen(f);

  if (f == label) {
    if (a != NULL)
      *a = label_mode;
    if (n != NULL)
      *n = -2L; /* convention for a label name */
    if (t != NULL)
      t->atime = t->mtime = t->ctime = label_utim;
    return label_time;
  }
  if ((name = malloc(len + 1)) == NULL) {
    ZIPERR(ZE_MEM, "filetime");
  }
  strcpy(name, f);

  /* not all systems allow stat'ing a file with / appended */
  if (name[len - 1] == '/')
    name[len - 1] = '\0';

  if (strcmp(f, "-") == 0) {
    /* stdin */
    /* Generally this is either a character special device
       (terminal directly in as stdin) or a FIFO (pipe). */
    if (zfstat(fileno(stdin), &s) != 0) {
      free(name);
      error("fstat(stdin)");
    }
    /* Clear character special and pipe bits and set regular file bit
       as we are storing content as regular file */
    s.st_mode = (s.st_mode & ~(S_IFIFO | S_IFCHR)) | S_IFREG;
#ifdef STDIN_PIPE_PERMS
    /* Set permissions */
    s.st_mode = (s.st_mode & ~0777) | STDIN_PIPE_PERMS;
#endif
  }
  else if (LSSTAT(name, &s) != 0) {
    /* Accept about any file kind including directories
     * (stored with trailing / with -r option)
     */
    free(name);
    return 0;
  }
  free(name);

  if (a != NULL) {
#ifdef ZOS_UNIX
    *a = ((ulg)s.st_mode << 16) | !(s.st_mode & S_IWRITE);
#else
/*
**  The following defines are copied from the unizip source and represent the
**  legacy Unix mode flags.  These fixed bit masks are no longer required
**  by XOPEN standards - the S_IS### macros being the new recommended method.
**  The approach here of setting the legacy flags by testing the macros should
**  work under any _XOPEN_SOURCE environment (and will just rebuild the same bit
**  mask), but is required if the legacy bit flags differ from legacy Unix.
*/
#define UNX_IFDIR      0040000     /* Unix directory */
#define UNX_IFREG      0100000     /* Unix regular file */
#define UNX_IFSOCK     0140000     /* Unix socket (BSD, not SysV or Amiga) */
#define UNX_IFLNK      0120000     /* Unix symbolic link (not SysV, Amiga) */
#define UNX_IFBLK      0060000     /* Unix block special       (not Amiga) */
#define UNX_IFCHR      0020000     /* Unix character special   (not Amiga) */
#define UNX_IFIFO      0010000     /* Unix fifo    (BCC, not MSC or Amiga) */
    {
    mode_t legacy_modes;

    /* Initialize with permission bits--which are not implementation-optional */
    legacy_modes = s.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO | S_ISUID | S_ISGID | S_ISVTX);
    if (S_ISDIR(s.st_mode))
      legacy_modes |= UNX_IFDIR;
    if (S_ISREG(s.st_mode))
      legacy_modes |= UNX_IFREG;
    if (S_ISLNK(s.st_mode))
      legacy_modes |= UNX_IFLNK;
    if (S_ISBLK(s.st_mode))
      legacy_modes |= UNX_IFBLK;
    if (S_ISCHR(s.st_mode))
      legacy_modes |= UNX_IFCHR;
    if (S_ISFIFO(s.st_mode))
      legacy_modes |= UNX_IFIFO;
    if (S_ISSOCK(s.st_mode))
      legacy_modes |= UNX_IFSOCK;
    *a = ((ulg)legacy_modes << 16) | !(s.st_mode & S_IWRITE);
    }
#endif
    if (S_ISDIR( s.st_mode)) {
      *a |= MSDOS_DIR_ATTR;
    }
  }
  if (n != NULL) {
    if (strcmp(f, "-") == 0)
      /* stdin relabeled above as regular, but flag no file size yet */
      *n = -1L;
    else
      *n = ((S_ISREG(s.st_mode)) ? s.st_size : -1L);
  }
  if (t != NULL) {
    t->atime = s.st_atime;
    t->mtime = s.st_mtime;
    t->ctime = t->mtime;   /* best guess, (s.st_ctime: last status change!!) */
  }
  return unix2dostime(&s.st_mtime);
}


#ifndef QLZIP /* QLZIP Unix2QDOS cross-Zip supplies an extended variant */

int set_new_unix_extra_field(z, s)
  struct zlist far *z;
  z_stat *s;
  /* New unix extra field.
     Currently only UIDs and GIDs are stored. */
{
  int uid_size;
  int gid_size;
  int ef_data_size;
  char *extra;
  char *cextra;
  ulg id;
  int b;

  uid_size = sizeof(s->st_uid);
  gid_size = sizeof(s->st_gid);

/* New extra field
   tag       (2 bytes)
   size      (2 bytes)
   version   (1 byte)
   uid_size  (1 byte - size in bytes)
   uid       (variable)
   gid_size  (1 byte - size in bytes)
   gid       (variable)
 */

  ef_data_size = 1 + 1 + uid_size + 1 + gid_size;

  if ((extra = (char *)malloc(z->ext + 4 + ef_data_size)) == NULL)
    return ZE_MEM;
  if ((cextra = (char *)malloc(z->ext + 4 + ef_data_size)) == NULL)
    return ZE_MEM;

  if (z->ext)
    memcpy(extra, z->extra, z->ext);
  if (z->cext)
    memcpy(cextra, z->cextra, z->cext);

  free(z->extra);
  z->extra = extra;
  free(z->cextra);
  z->cextra = cextra;

  /* 7875 */

  z->extra[z->ext + 0] = 'u';
  z->extra[z->ext + 1] = 'x';
  z->extra[z->ext + 2] = (char)ef_data_size;     /* length of data part */
  z->extra[z->ext + 3] = 0;
  z->extra[z->ext + 4] = 1;                      /* version */

  /* UID */
  z->extra[z->ext + 5] = (char)(uid_size);       /* uid size in bytes */
  b = 6;
  id = (ulg)(s->st_uid);
  z->extra[z->ext + b] = (char)(id & 0xFF);
  if (uid_size > 1) {
    b++;
    id = id >> 8;

    z->extra[z->ext + b] = (char)(id & 0xFF);
    if (uid_size > 2) {
      b++;
      id = id >> 8;
      z->extra[z->ext + b] = (char)(id & 0xFF);
      b++;
      id = id >> 8;
      z->extra[z->ext + b] = (char)(id & 0xFF);
      if (uid_size == 8) {
        b++;
        id = id >> 8;
        z->extra[z->ext + b] = (char)(id & 0xFF);
        b++;
        id = id >> 8;
        z->extra[z->ext + b] = (char)(id & 0xFF);
        b++;
        id = id >> 8;
        z->extra[z->ext + b] = (char)(id & 0xFF);
        b++;
        id = id >> 8;
        z->extra[z->ext + b] = (char)(id & 0xFF);
      }
    }
  }

  /* GID */
  b++;
  z->extra[z->ext + b] = (char)(gid_size);       /* gid size in bytes */
  b++;
  id = (ulg)(s->st_gid);
  z->extra[z->ext + b] = (char)(id & 0xFF);
  if (gid_size > 1) {
    b++;
    id = id >> 8;
    z->extra[z->ext + b] = (char)(id & 0xFF);
    if (gid_size > 2) {
      b++;
      id = id >> 8;
      z->extra[z->ext + b] = (char)(id & 0xFF);
      b++;
      id = id >> 8;
      z->extra[z->ext + b] = (char)(id & 0xFF);
      if (gid_size == 8) {
        b++;
        id = id >> 8;
        z->extra[z->ext + b] = (char)(id & 0xFF);
        b++;
        id = id >> 8;
        z->extra[z->ext + b] = (char)(id & 0xFF);
        b++;
        id = id >> 8;
        z->extra[z->ext + b] = (char)(id & 0xFF);
        b++;
        id = id >> 8;
        z->extra[z->ext + b] = (char)(id & 0xFF);
      }
    }
  }

  /* copy local extra field to central directory extra field */
  memcpy((z->cextra) + z->cext, (z->extra) + z->ext, 4 + ef_data_size);

  z->ext = z->ext + 4 + ef_data_size;
  z->cext = z->cext + 4 + ef_data_size;

  return ZE_OK;
}


int set_extra_field(z, z_utim)
  struct zlist far *z;
  iztimes *z_utim;
  /* store full data in local header but just modification time stamp info
     in central header */
{
  z_stat s;
  char *name;
  int len = strlen(z->name);

  /* For the full sized UT local field including the UID/GID fields, we
   * have to stat the file again. */

  if ((name = malloc(len + 1)) == NULL) {
    ZIPERR(ZE_MEM, "set_extra_field");
  }
  strcpy(name, z->name);
  if (name[len - 1] == '/')
    name[len - 1] = '\0';
  /* not all systems allow stat'ing a file with / appended */
  if (LSSTAT(name, &s)) {
    free(name);
    return ZE_OPEN;
  }
  free(name);

#define EB_L_UT_SIZE    (EB_HEADSIZE + EB_UT_LEN(2))
#define EB_C_UT_SIZE    (EB_HEADSIZE + EB_UT_LEN(1))

/* The flag UIDGID_NOT_16BIT should be set by the pre-compile configuration
   script when it detects st_uid or st_gid sizes differing from 16-bit.
 */
#ifndef UIDGID_NOT_16BIT
  /* The following "second-level" check for st_uid and st_gid members being
     16-bit wide is only added as a safety precaution in case the "first-level"
     check failed to define the UIDGID_NOT_16BIT symbol.
     The first-level check should have been implemented in the automatic
     compile configuration process.
   */
# ifdef UIDGID_ARE_16B
#  undef UIDGID_ARE_16B
# endif
  /* The following expression is a compile-time constant and should (hopefully)
     get optimized away by any sufficiently intelligent compiler!
   */
# define UIDGID_ARE_16B  (sizeof(s.st_uid) == 2 && sizeof(s.st_gid) == 2)

# define EB_L_UX2_SIZE   (EB_HEADSIZE + EB_UX2_MINLEN)
# define EB_C_UX2_SIZE   EB_HEADSIZE
# define EF_L_UNIX_SIZE  (EB_L_UT_SIZE + (UIDGID_ARE_16B ? EB_L_UX2_SIZE : 0))
# define EF_C_UNIX_SIZE  (EB_C_UT_SIZE + (UIDGID_ARE_16B ? EB_C_UX2_SIZE : 0))
#else
# define EF_L_UNIX_SIZE EB_L_UT_SIZE
# define EF_C_UNIX_SIZE EB_C_UT_SIZE
#endif /* !UIDGID_NOT_16BIT */

  if ((z->extra = (char *)malloc(EF_L_UNIX_SIZE)) == NULL)
    return ZE_MEM;
  if ((z->cextra = (char *)malloc(EF_C_UNIX_SIZE)) == NULL)
    return ZE_MEM;

  z->extra[0]  = 'U';
  z->extra[1]  = 'T';
  z->extra[2]  = (char)EB_UT_LEN(2);    /* length of data part of local e.f. */
  z->extra[3]  = 0;
  z->extra[4]  = EB_UT_FL_MTIME | EB_UT_FL_ATIME;    /* st_ctime != creation */
  z->extra[5]  = (char)(s.st_mtime);
  z->extra[6]  = (char)(s.st_mtime >> 8);
  z->extra[7]  = (char)(s.st_mtime >> 16);
  z->extra[8]  = (char)(s.st_mtime >> 24);
  z->extra[9]  = (char)(s.st_atime);
  z->extra[10] = (char)(s.st_atime >> 8);
  z->extra[11] = (char)(s.st_atime >> 16);
  z->extra[12] = (char)(s.st_atime >> 24);

#ifndef UIDGID_NOT_16BIT
  /* 7855 */

  /* Only store the UID and GID in the old Ux extra field if the runtime
     system provides them in 16-bit wide variables.  */
  if (UIDGID_ARE_16B) {
    z->extra[13] = 'U';
    z->extra[14] = 'x';
    z->extra[15] = (char)EB_UX2_MINLEN; /* length of data part of local e.f. */
    z->extra[16] = 0;
    z->extra[17] = (char)(s.st_uid);
    z->extra[18] = (char)(s.st_uid >> 8);
    z->extra[19] = (char)(s.st_gid);
    z->extra[20] = (char)(s.st_gid >> 8);
  }
#endif /* !UIDGID_NOT_16BIT */

  z->ext = EF_L_UNIX_SIZE;

  memcpy(z->cextra, z->extra, EB_C_UT_SIZE);
  z->cextra[EB_LEN] = (char)EB_UT_LEN(1);
#ifndef UIDGID_NOT_16BIT
  if (UIDGID_ARE_16B) {
    /* Copy header of Ux extra field from local to central */
    memcpy(z->cextra+EB_C_UT_SIZE, z->extra+EB_L_UT_SIZE, EB_C_UX2_SIZE);
    z->cextra[EB_LEN+EB_C_UT_SIZE] = 0;
  }
#endif
  z->cext = EF_C_UNIX_SIZE;

#if 0  /* UID/GID presence is now signaled by central EF_IZUNIX2 field ! */
  /* lower-middle external-attribute byte (unused until now):
   *   high bit        => (have GMT mod/acc times) >>> NO LONGER USED! <<<
   *   second-high bit => have Unix UID/GID info
   * NOTE: The high bit was NEVER used in any official Info-ZIP release,
   *       but its future use should be avoided (if possible), since it
   *       was used as "GMT mod/acc times local extra field" flags in Zip beta
   *       versions 2.0j up to 2.0v, for about 1.5 years.
   */
  z->atx |= 0x4000;
#endif /* never */

  /* new unix extra field */
  set_new_unix_extra_field(z, &s);

  return ZE_OK;
}

#endif /* !QLZIP */


int deletedir(d)
char *d;                /* directory to delete */
/* Delete the directory *d if it is empty, do nothing otherwise.
   Return the result of rmdir(), delete(), or system().
   For VMS, d must be in format [x.y]z.dir;1  (not [x.y.z]).
 */
{
# ifdef NO_RMDIR
    /* code from Greg Roelofs, who horked it from Mark Edwards (unzip) */
    int r, len;
    char *s;              /* malloc'd string for system command */

    len = strlen(d);
    if ((s = malloc(len + 34)) == NULL)
      return 127;

    sprintf(s, "IFS=\" \t\n\" /bin/rmdir %s 2>/dev/null", d);
    r = system(s);
    free(s);
    return r;
# else /* !NO_RMDIR */
    return rmdir(d);
# endif /* ?NO_RMDIR */
}



/* ------------------------------------ */
/* Added 2014-09-06 */


/* whole is a pathname with wildcards, wildtail points somewhere in the  */
/* middle of it.  All wildcards to be expanded must come AFTER wildtail. */

local int wild_recurse(whole, wildtail)
  char *whole;
  char *wildtail;
{
  DIR *dir;
  char *subwild, *name, *newwhole = NULL, *glue = NULL, plug = 0, plug2;
  extent newlen;
  int amatch = 0, e = ZE_MISS;
  int wholelen;
  char *newpath;
  int pathlen;

#if 0
  zprintf(" {in wild_recurse: whole '%s' wildtail '%s'}\n", whole, wildtail);
#endif

  if (!isshexp(wildtail)) {
#if 0
    zprintf(" {no wild: %s}\n", wildtail);
#endif
    return procname(whole, 1);
  }

  wholelen = strlen(whole);

  /* back up thru path components till existing dir found */
  do
  {
    name = wildtail + strlen(wildtail) - 1;
    for (;;) {
      if (name-- <= wildtail || *name == PATH_END) {
        subwild = name + 1;
        plug2 = *subwild;
        *subwild = 0;
        break;
      }
    }
    if (glue)
      *glue = plug;
    glue = subwild;
    plug = plug2;
    dir = opendir(whole);
  } while (!dir && subwild > wildtail);
  wildtail = subwild;                 /* skip past non-wild components */

  if ((subwild = MBSCHR(wildtail + 1, PATH_END)) != NULL) {
    /* this "+ 1" dodges the     ^^^ hole left by *glue == 0 */
    *(subwild++) = 0;               /* wildtail = one component pattern */
    /* newlen = strlen(whole) + strlen(subwild) + (wholelen + 2); */
    newlen = wholelen + 1;
  } else
    /* newlen = strlen(whole) + (wholelen + 1); */
    newlen = wholelen + 1;
  if (!dir || ((newwhole = malloc(newlen)) == NULL)) {
    if (glue)
      *glue = plug;
    e = dir ? ZE_MEM : ZE_MISS;
    goto ohforgetit;
  }
  strcpy(newwhole, whole);
  newlen = strlen(newwhole);
  if (glue)
    *glue = plug;                           /* repair damage to whole */
  if (!isshexp(wildtail)) {
    e = ZE_MISS;                            /* non-wild name not found */
    goto ohforgetit;
  }

  while ((name = readd(dir)) != NULL) {

    if (strcmp(name, ".") && strcmp(name, "..") &&
        MATCH(wildtail, name, 1)) {
      pathlen = newlen + strlen(name) + 2;
      if (subwild) {
        pathlen += strlen(subwild);
      }
      if ((newpath = malloc(pathlen)) == NULL) {
        e = ZE_MEM;
        goto ohforgetit;
      }
      strcpy(newpath, newwhole);
      strcat(newpath, name);
      if (subwild) {
        name = newpath + strlen(newpath);
        *(name++) = PATH_END;
        strcpy(name, subwild);
        e = wild_recurse(newpath, name);
      } else
        e = procname(newpath, 1);
      free(newpath);

      if (e == ZE_OK)
        amatch = 1;
      else if (e != ZE_MISS)
        break;
    }
  }

ohforgetit:
  if (dir) closedir(dir);
  if (subwild) *--subwild = PATH_END;
  if (newwhole) free(newwhole);
  if (e == ZE_MISS && amatch)
    e = ZE_OK;
  return e;
}


int wild(w)
  char *w;               /* path/pattern to match */
/* If not in exclude mode, expand the pattern based on the contents of the
   file system.  Return an error code in the ZE_ class. */
{
    char *p;             /* path */
    char *q;             /* tail */
    int e;               /* result */

#if 0
    zprintf(" {in wild: %s}\n", w);
#endif
    /* special handling of stdin request */
    if (strcmp(w, "-") == 0)   /* if compressing stdin */
        return newname(w, 0, 0);

    /* Allocate and copy pattern, leaving room to add "." if needed */
    if ((p = malloc(strlen(w) + 3)) == NULL)
        return ZE_MEM;

#if 1
    /* Apparently on Unix a leading "./" is not needed to recurse. */
    /* Apparently it is if wildcards are involved. */
    p[0] = '\0';

    if (strncmp(w, "./", 2) &&
        strncmp(w, "../", 3) &&
        w[0] != '/') {
      /* if relative, add ./ if not there */
      strcat(p, "./");
    }
    strcat(p, w);
#else
    strcpy(p, w);
#endif

    /* set tail to front to start */
    q = p;

#if 0
    zprintf(" {wild calling wild_recurse with: %s}\n", p);
#endif
    /* Here we go */
    e = wild_recurse(p, q);
#if 0
    zprintf(" {wild returning: %d}\n", e);
#endif

    free((zvoid *)p);
    return e;
}


/* ------------------------------------ */


#endif /* !UTIL */


/******************************/
/*  Function version_local()  */
/******************************/

#if defined(__NetBSD__) || defined(__FreeBSD__) || defined(__386BSD__) || \
    defined(__OpenBSD__) || defined(__bsdi__)
#include <sys/param.h> /* for the BSD define */
/* if we have something newer than NET/2 we'll use uname(3) */
#if (BSD > 199103)
#include <sys/utsname.h>
#endif /* BSD > 199103 */
#endif /* __{Net,Free,Open,386}BSD__ || __bsdi__ */

void version_local()
{
#ifdef __GNUC__
   /* __GNUC__ is generated by gcc and gcc-based compilers */
   /* - Other compilers define __GNUC__ for compatibility  */
#  if (defined(NX_CURRENT_COMPILER_RELEASE) || \
       defined(__MINGW64__) || \
       defined(__MINGW32__) || \
       defined(__llvm__) || \
       defined(__PCC__) || \
       defined(__PATHCC__) || \
       defined(__INTEL_COMPILER) || \
       defined(__GNUC_PATCHLEVEL__) )
    char compiler_name[80];
#  endif
#else /* !__GNUC__ */
#  if (defined( __SUNPRO_C))
    char compiler_name[33];
#  else
#    if (defined( __HP_cc))
    char compiler_name[33];
#    else
#      if (defined( __IBMC__))
    char compiler_name[40];
#      else
#        if (defined( __DECC_VER))
    char compiler_name[33];
    int compiler_typ;
#        else
#          if ((defined(CRAY) || defined(cray)) && defined(_RELEASE))
    char compiler_name[40];
#          endif
#        endif
#      endif
#    endif
#  endif
#endif /* __GNUC__ */

#if defined( BSD) && !defined( __APPLE__)
# if (BSD > 199103)
    struct utsname u;
    char os_name[40];
# else /* (BSD > 199103) */
# if defined(__NETBSD__)
    static ZCONST char *netbsd[] = { "_ALPHA", "", "A", "B" };
    char os_name[40];
# endif /* __NETBSD__ */
# endif /* (BSD > 199103) [else] */
#else /* defined( BSD) && !defined( __APPLE__) */
#if ((defined(CRAY) || defined(cray)) && defined(_UNICOS))
    char os_name[40];
#endif /* (CRAY && defined(_UNICOS)) */
#endif /* defined( BSD) && !defined( __APPLE__) [else] */

/* Define the compiler name and version string */
#ifdef __GNUC__
   /* __GNUC__ is generated by gcc and gcc-based compilers */
   /* - Other compilers define __GNUC__ for compatibility  */
#  if defined(NX_CURRENT_COMPILER_RELEASE)
    sprintf( compiler_name, "NeXT DevKit %d.%02d (GCC " __VERSION__ ")",
             NX_CURRENT_COMPILER_RELEASE/100,
             NX_CURRENT_COMPILER_RELEASE%100 );
#    define COMPILER_NAME compiler_name
#  else
#  if defined(__MINGW64__)
    sprintf( compiler_name, "MinGW 64 GCC %d.%d.%d"),
             __GNUC__,
             __GNUC_MINOR__,
             __GNUC_PATCHLEVEL__ );
#    define COMPILER_NAME compiler_name
#  else
#  if defined(__MINGW32__)
    sprintf( compiler_name, "MinGW 32 GCC %d.%d.%d"),
             __GNUC__,
             __GNUC_MINOR__,
             __GNUC_PATCHLEVEL__ );
#    define COMPILER_NAME compiler_name
#  else
#  if defined(__llvm__)
#    if defined(__clang__)
    sprintf( compiler_name, "LLVM Clang %d.%d.%d (GCC %d.%d.%d)",
             __clang_major__,
             __clang_minor__,
             __clang_patchlevel__,
             __GNUC__,
             __GNUC_MINOR__,
             __GNUC_PATCHLEVEL__ );
#      define COMPILER_NAME compiler_name
#    else
#    if defined(__APPLE_CC__)
    sprintf( compiler_name, "LLVM Apple GCC %d.%d.%d",
             __GNUC__,
             __GNUC_MINOR__,
             __GNUC_PATCHLEVEL__ );
#      define COMPILER_NAME compiler_name
#    else
    sprintf( compiler_name, "LLVM GCC %d.%d.%d",
             __GNUC__,
             __GNUC_MINOR__,
             __GNUC_PATCHLEVEL__ );
#      define COMPILER_NAME compiler_name
#    endif /* (__APPLE_CC__) */
#    endif /* (__clang__) */
#  else
#  if defined(__PCC__)
    sprintf( compiler_name, "Portable C compiler %d.%d.%d",
             __PCC__,
             __PCC_MINOR__,
             __PCC_MINORMINOR__ );
#    define COMPILER_NAME compiler_name
#  else
#  if defined(__PATHCC__)
    sprintf( compiler_name, "EKOPath C compiler %d.%d.%d",
             __PATHCC__,
             __PATHCC_MINOR__,
             __PATHCC_PATCHLEVEL__ );
#    define COMPILER_NAME compiler_name
#  else
#  if defined(__INTEL_COMPILER)
#    if defined(__INTEL_COMPILER_BUILD_DATE)
    sprintf( compiler_name, "Intel C compiler %d.%d (%s)",
             (__INTEL_COMPILER / 100),
             (__INTEL_COMPILER % 100),
             __INTEL_COMPILER_BUILD_DATE );
#      define COMPILER_NAME compiler_name
#    else
    sprintf( compiler_name, "Intel C compiler %d.%d",
             (__INTEL_COMPILER / 100),
             (__INTEL_COMPILER % 100) );
#      define COMPILER_NAME compiler_name
#    endif /* (__INTEL_COMPILER_BUILD_DATE) */
#  else
#  if defined(__GNUC_PATCHLEVEL__)
    sprintf( compiler_name, "GCC %d.%d.%d",
             __GNUC__,
             __GNUC_MINOR__,
             __GNUC_PATCHLEVEL__ );
#    define COMPILER_NAME compiler_name
#  else
#    define COMPILER_NAME "GCC " __VERSION__
#  endif /* (__GNUC_PATCHLEVEL__) */
#  endif /* (__INTEL_COMPILER) */
#  endif /* (__PATHCC__) */
#  endif /* (__PCC__) */
#  endif /* (__llvm__) */
#  endif /* (__MINGW32__) */
#  endif /* (__MINGW64__) */
#  endif /* (NX_CURRENT_COMPILER_RELEASE) */
#else /* !__GNUC__ */
#  if defined(__SUNPRO_C)
    sprintf( compiler_name, "Sun C version %x",
             __SUNPRO_C );
#    define COMPILER_NAME compiler_name
#  else
#    if (defined( __HP_cc))
    if ((__HP_cc% 100) == 0)
    {
      sprintf( compiler_name, "HP C version A.%02d.%02d",
               (__HP_cc/ 10000),
               ((__HP_cc% 10000)/ 100) );
    }
    else
    {
      sprintf( compiler_name, "HP C version A.%02d.%02d.%02d",
               (__HP_cc/ 10000),
               ((__HP_cc% 10000)/ 100),
               (__HP_cc% 100) );
    }
#      define COMPILER_NAME compiler_name
#    else
#      if (defined( __DECC_VER))
    sprintf( compiler_name, "DEC C version %c%d.%d-%03d",
             ((compiler_typ = (__DECC_VER / 10000) % 10) == 6 ? 'T' :
              (compiler_typ == 8 ? 'S' : 'V')),
             __DECC_VER / 10000000,
             (__DECC_VER % 10000000) / 100000,
             __DECC_VER % 1000 );
#        define COMPILER_NAME compiler_name
#      else
#        if ((defined(CRAY) || defined(cray)) && defined(_RELEASE))
    sprintf( compiler_name, "Cray cc version %d",
             _RELEASE );
#          define COMPILER_NAME compiler_name
#        else
#          ifdef __IBMC__
    sprintf( compiler_name, "%s version %d.%d.%d",
#            if (defined(__TOS_LINUX__))
               "IBM XL C for Linux",
#            else
#            if (defined(__PPC__))
               "IBM XL C for AIX",
#            else
#            if (defined(__MVS__))
               "IBM z/OS XL C",
#            else
#            if (defined(__VM__))
               "IBM XL C for z/VM",
#            else
#            if (defined(__OS400__))
               "IBM ILC C for iSeries",
#            else
               "IBM C ",
#            endif /* (__OS400__) */
#            endif /* (__VM__) */
#            endif /* (__MVS__) */
#            endif /* (__PPC__) */
#            endif /* (__TOS_LINUX__) */
#           if (defined(__MVS__) || defined(__VM__))
             (((__IBMC__/ 1000)& 3)% 1000),
             ((__IBMC__/ 10)% 100),
             (__IBMC__% 10) );
#           else /* !(__MVS__ || __VM__) */
             (__IBMC__/ 100),
             ((__IBMC__/ 10)% 10),
             (__IBMC__% 10) );
#           endif /* ?(__MVS__ || __VM__) */
#            define COMPILER_NAME compiler_name
#          else /* !__IBMC__ */
#            ifdef __VERSION__
#              define COMPILER_NAME "cc " __VERSION__
#            else
#              define COMPILER_NAME "cc "
#            endif
#          endif /* ?__IBMC__ */
#        endif
#      endif
#    endif
#  endif
#endif /* ?__GNUC__ */


/* Define the name to use for the OS we're compiling on */
#if defined(sgi) || defined(__sgi)
#  define OS_NAME "Silicon Graphics IRIX"
#else
#ifdef sun
#  if defined(UNAME_P) && defined(UNAME_R) && defined(UNAME_S)
#    define OS_NAME UNAME_S" "UNAME_R" "UNAME_P
#  else
#  ifdef sparc
#    ifdef __SVR4
#      define OS_NAME "Sun SPARC/Solaris"
#    else /* may or may not be SunOS */
#      define OS_NAME "Sun SPARC"
#    endif
#  else /* def sparc */
#  if defined(sun386) || defined(i386)
#    define OS_NAME "Sun 386i"
#  else /* defined(sun386) || defined(i386) */
#  if defined(mc68020) || defined(__mc68020__)
#    define OS_NAME "Sun 3"
#  else /* mc68010 or mc68000:  Sun 2 or earlier */
#    define OS_NAME "Sun 2"
#  endif /* defined(mc68020) || defined(__mc68020__) [else] */
#  endif /* defined(sun386) || defined(i386) [else] */
#  endif /* def sparc [else] */
#  endif /* defined(UNAME_P) && defined(UNAME_R) && defined(UNAME_S) */
#else /* def sun */
#ifdef __hpux
#  if defined(UNAME_M) && defined(UNAME_R) && defined(UNAME_S)
#    define OS_NAME UNAME_S" "UNAME_R" "UNAME_M
#  else
#    define OS_NAME "HP-UX"
#  endif
#else
#ifdef __osf__
#  if defined( SIZER_V)
#    define OS_NAME "Tru64 "SIZER_V
#  else /* defined( SIZER_V) */
#    define OS_NAME "DEC OSF/1"
#  endif /* defined( SIZER_V) [else] */
#else
#ifdef _AIX
#  if defined( UNAME_R) && defined( UNAME_S) && defined( UNAME_V)
#    define OS_NAME UNAME_S" "UNAME_V"."UNAME_R
#  else /*  */
#    define OS_NAME "IBM AIX"
#  endif /* [else] */
#else
#ifdef aiws
#  define OS_NAME "IBM RT/AIX"
#else
#ifdef __MVS__
#  if defined( UNAME_R) && defined( UNAME_S) && defined( UNAME_V)
#    define OS_NAME UNAME_S" "UNAME_V"."UNAME_R
#  else
#    define OS_NAME "IBM z/OS"
#  endif
#else
#ifdef __VM__
#  if defined( UNAME_R) && defined( UNAME_S) && defined( UNAME_V)
#    define OS_NAME UNAME_S" "UNAME_V"."UNAME_R
#  else
#    define OS_NAME "IBM z/VM"
#  endif
#else
#if defined(CRAY) || defined(cray)
#  ifdef _UNICOS
    sprintf(os_name, "Cray UNICOS release %d", _UNICOS);
#    define OS_NAME os_name
#  else
#    define OS_NAME "Cray UNICOS"
#  endif
#else
#if defined(uts) || defined(UTS)
#  define OS_NAME "Amdahl UTS"
#else
#ifdef NeXT
#  ifdef mc68000
#    define OS_NAME "NeXTStep/black"
#  else
#    define OS_NAME "NeXTStep for Intel"
#  endif
#else
#if defined(linux) || defined(__linux__)
#  if defined( UNAME_M) && defined( UNAME_O)
#    define OS_NAME UNAME_O" "UNAME_M
#  else
#    ifdef __ELF__
#      define OS_NAME "Linux ELF"
#    else
#      define OS_NAME "Linux a.out"
#    endif
#  endif
#else
#ifdef MINIX
#  define OS_NAME "Minix"
#else
#ifdef M_UNIX
#  define OS_NAME "SCO Unix"
#else
#ifdef M_XENIX
#  define OS_NAME "SCO Xenix"
#else
#if defined( BSD) && !defined( __APPLE__)
# if (BSD > 199103)
#    define OS_NAME os_name
    uname(&u);
    sprintf(os_name, "%s %s", u.sysname, u.release);
# else /* (BSD > 199103) */
# ifdef __NetBSD__
#   define OS_NAME os_name
#   ifdef NetBSD0_8
      sprintf(os_name, "NetBSD 0.8%s", netbsd[NetBSD0_8]);
#   else
#   ifdef NetBSD0_9
      sprintf(os_name, "NetBSD 0.9%s", netbsd[NetBSD0_9]);
#   else
#   ifdef NetBSD1_0
      sprintf(os_name, "NetBSD 1.0%s", netbsd[NetBSD1_0]);
#   endif /* NetBSD1_0 */
#   endif /* NetBSD0_9 */
#   endif /* NetBSD0_8 */
# else
# ifdef __FreeBSD__
#    define OS_NAME "FreeBSD 1.x"
# else
# ifdef __bsdi__
#    define OS_NAME "BSD/386 1.0"
# else
# ifdef __386BSD__
#    define OS_NAME "386BSD"
# else
#    define OS_NAME "Unknown BSD"
# endif /* __386BSD__ */
# endif /* __bsdi__ */
# endif /* FreeBSD */
# endif /* NetBSD */
# endif /* (BSD > 199103) [else] */
#else
#ifdef __CYGWIN__
#  define OS_NAME "Cygwin"
#else
#if defined(i686) || defined(__i686) || defined(__i686__)
#  define OS_NAME "Intel 686"
#else
#if defined(i586) || defined(__i586) || defined(__i586__)
#  define OS_NAME "Intel 586"
#else
#if defined(i486) || defined(__i486) || defined(__i486__)
#  define OS_NAME "Intel 486"
#else
#if defined(i386) || defined(__i386) || defined(__i386__)
#  define OS_NAME "Intel 386"
#else
#ifdef pyr
#  define OS_NAME "Pyramid"
#else
#if defined(ultrix) || defined(__ultrix)
#  if defined(mips) || defined(__mips)
#    define OS_NAME "DEC/MIPS"
#  else
#  if defined(vax) || defined(__vax)
#    define OS_NAME "DEC/VAX"
#  else /* __alpha? */
#    define OS_NAME "DEC/Alpha"
#  endif
#  endif
#else
#ifdef gould
#  define OS_NAME "Gould"
#else
#ifdef MTS
#  define OS_NAME "MTS"
#else
#ifdef __convexc__
#  define OS_NAME "Convex"
#else
#ifdef __QNX__
#  define OS_NAME "QNX 4"
#else
#ifdef __QNXNTO__
#  define OS_NAME "QNX Neutrino"
#else
#ifdef __APPLE__
#  if defined(UNAME_P) && defined(UNAME_R) && defined(UNAME_S)
#    define OS_NAME UNAME_S" "UNAME_R" "UNAME_P
#  else
#  ifdef __i386__
#    define OS_NAME "Mac OS X Intel"
#  else /* __i386__ */
#    ifdef __ppc__
#      define OS_NAME "Mac OS X PowerPC"
#    else /* __ppc__ */
#      ifdef __ppc64__
#        define OS_NAME "Mac OS X PowerPC64"
#      else /* __ppc64__ */
#        define OS_NAME "Mac OS X"
#      endif /* __ppc64__ */
#    endif /* __ppc__ */
#  endif /* __i386__ */
#  endif
#else
#  define OS_NAME "Unknown"
#endif /* Apple */
#endif /* QNX Neutrino */
#endif /* QNX 4 */
#endif /* Convex */
#endif /* MTS */
#endif /* Gould */
#endif /* DEC */
#endif /* Pyramid */
#endif /* 386 */
#endif /* 486 */
#endif /* 586 */
#endif /* 686 */
#endif /* Cygwin */
#endif /* defined( BSD) & !defined( __APPLE__) */
#endif /* SCO Xenix */
#endif /* SCO Unix */
#endif /* Minix */
#endif /* Linux */
#endif /* NeXT */
#endif /* Amdahl */
#endif /* Cray */
#endif /* z/VM */
#endif /* z/OS */
#endif /* RT/AIX */
#endif /* AIX */
#endif /* OSF/1 */
#endif /* HP-UX */
#endif /* Sun */
#endif /* SGI */



/* Define the compile date string */
#if defined( __DATE__) && !defined( NO_BUILD_DATE)
#  define COMPILE_DATE " on " __DATE__
#else
#  define COMPILE_DATE ""
#endif

    printf("Compiled with %s for Unix (%s)%s.\n\n",
           COMPILER_NAME, OS_NAME, COMPILE_DATE);

} /* end function version_local() */


# ifdef IZ_CRYPT_AES_WG

/* 2011-04-24 SMS.
 *
 *       entropy_fun().
 *
 *    Fill the user's buffer with stuff.
 */

#  include <time.h>

int entropy_fun( unsigned char *buf, unsigned int len)
{
    int fd;
    int len_ret;

    static int i = -1;
    char *rnd_dev_names[] = { "/dev/urandom", "/dev/random", NULL };

    if (i < 0)
    {
        fd = -1;
        while ((fd < 0) && (rnd_dev_names[ ++i] != NULL))
        {
            fd = open( rnd_dev_names[ i], O_RDONLY);
        }
    }
    else
    {
        fd = open( rnd_dev_names[ i], O_RDONLY);
    }

    len_ret = 0;
    if (fd >= 0)
    {
        len_ret = read( fd, buf, len);
        close( fd);
    }
    else
    {
        /* Good sources failed us.  Fall back to a lame source, namely
         * rand(), using time()^ getpid() as a seed.
         */
        int tbuf;

        srand( time( NULL)^ getpid());
        tbuf = rand();

        /* Move the results into the user's buffer. */
        len_ret = IZ_MIN( 4, len);
        memcpy( buf, &tbuf, len_ret);
    }

    return len_ret;
}

# endif /* def IZ_CRYPT_AES_WG */


/* 2006-03-23 SMS.
 * Emergency replacement for strerror().  (Useful on SunOS 4.*.)
 * Enable by specifying "LOCAL_ZIP=-DNEED_STRERROR=1" on the "make"
 * command line.
 */

#ifdef NEED_STRERROR

char *strerror( err)
  int err;
{
    extern char *sys_errlist[];
    extern int sys_nerr;

    static char no_msg[ 64];

    if ((err >= 0) && (err < sys_nerr))
    {
        return sys_errlist[ err];
    }
    else
    {
        sprintf( no_msg, "(no message, code = %d.)", err);
        return no_msg;
    }
}

#endif /* def NEED_STRERROR */


/* 2006-03-23 SMS.
 * Emergency replacement for memmove().  (Useful on SunOS 4.*.)
 * Enable by specifying "LOCAL_ZIP=-DNEED_MEMMOVE=1" on the "make"
 * command line.
 */

#ifdef NEED_MEMMOVE

/* memmove.c -- copy memory.
   Copy LENGTH bytes from SOURCE to DEST.  Does not null-terminate.
   In the public domain.
   By David MacKenzie <djm@gnu.ai.mit.edu>.
   Adjusted by SMS.
*/

void *memmove(dest0, source0, length)
  void *dest0;
  void const *source0;
  size_t length;
{
    char *dest = dest0;
    char const *source = source0;
    if (source < dest)
        /* Moving from low mem to hi mem; start at end.  */
        for (source += length, dest += length; length; --length)
            *--dest = *--source;
    else if (source != dest)
    {
        /* Moving from hi mem to low mem; start at beginning.  */
        for (; length; --length)
            *dest++ = *source++;
    }
    return dest0;
}

#endif /* def NEED_MEMMOVE */

