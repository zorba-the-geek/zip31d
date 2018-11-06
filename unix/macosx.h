/*
  macosx.h - Zip 3.1

  Copyright (c) 2008-2013 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2007-Mar-4 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/

#ifndef __MACOSX_H
# define __MACOSX_H

# if defined( UNIX) && defined( __APPLE__)

#  include <sys/attr.h>
#  include <sys/vnode.h>
#  ifdef APPLE_XATTR
#   include <sys/xattr.h>
#  endif /* def APPLE_XATTR */

#  define APL_DBL_PFX "._"
#  define APL_DBL_PFX_SQR "__MACOSX/"

   /* Select modern ("/..namedfork/rsrc") or old ("/rsrc") suffix
    * for resource fork access.
    */
#  ifndef APPLE_NFRSRC
#   if defined( __ppc__) || defined( __ppc64__)
#    define APPLE_NFRSRC 0
#   else /* defined( __ppc__) || defined( __ppc64__) */
#    define APPLE_NFRSRC 1
#   endif /* defined( __ppc__) || defined( __ppc64__) [else] */
#  endif /* ndef APPLE_NFRSRC */
#  if APPLE_NFRSRC
#   define APL_DBL_SUFX "/..namedfork/rsrc"
#  else /* APPLE_NFRSRC */
#   define APL_DBL_SUFX "/rsrc"
#  endif /* APPLE_NFRSRC [else] */

#  define APL_DBL_HDR_MAGIC    0x00051607       /* AD header magic. */
#  define APL_DBL_HDR_VERSION  0x00020000       /* AD header version. */
#  define APL_DBL_HDR_FILE_SYS "Mac OS X        "   /* File sys nm (16 chr.) */
#  define APL_DBL_HDR_EID_FI            9       /* Entry ID: Finder info. */
#  define APL_DBL_HDR_EID_RF            2       /* Entry ID: Resource fork. */

#  define APL_DBL_OFS_MAGIC             0
#  define APL_DBL_OFS_VERSION           4
#  define APL_DBL_OFS_FILE_SYS          8
#  define APL_DBL_OFS_ENT_CNT          24
#  define APL_DBL_OFS_ENT_ID0          26
#  define APL_DBL_OFS_ENT_OFS0         30
#  define APL_DBL_OFS_ENT_LEN0         34
#  define APL_DBL_OFS_ENT_ID1          38       /* APL_DBL_OFS_ENT_ID0+ 12 */
#  define APL_DBL_OFS_ENT_OFS1         42       /* APL_DBL_OFS_ENT_OFS0+ 12 */
#  define APL_DBL_OFS_ENT_LEN1         46       /* APL_DBL_OFS_ENT_LEN0+ 12 */

#  define APL_DBL_OFS_ENT_DSCR         28
#  define APL_DBL_OFS_ENT_DSCR_OFS1    42
#  define APL_DBL_OFS_FNDR_INFO        50
#  define APL_DBL_SIZE_FNDR_INFO       32
#  define APL_DBL_SIZE_HDR              \
    (APL_DBL_OFS_FNDR_INFO+ APL_DBL_SIZE_FNDR_INFO)

#  ifdef APPLE_XATTR
#   define APL_DBL_XA_MAGIC        "ATTR"       /* AD XA magic (len = 4). */
#   define APL_DBL_SIZE_ATTR_HDR       38
#  endif /* def APPLE_XATTR */

   /* Macros to convert big-endian byte (unsigned char) array segments
    * to 16- or 32-bit entities.
    * Note that the larger entities must be naturally aligned in the
    * byte array for the simple type casts to work (on PowerPC).  This
    * should be true for the AppleDouble data where we use these macros.
    */
#  if defined( __ppc__) || defined( __ppc64__)
    /* Big-endian to Big-endian. */
#   define BIGC_TO_HOST16( i16) (*((unsigned short *)(i16)))
#   define BIGC_TO_HOST32( i32) (*((unsigned int *)(i32)))
#   define HOST16_TO_BIGC( uca, i16) *((unsigned short *)(uca)) = (i16)
#   define HOST32_TO_BIGC( uca, i32) *((unsigned int *)(uca)) = (i32)
#  else /* defined( __ppc__) || defined( __ppc64__) */
    /* Little-endian to Big-endian. */
#   define BIGC_TO_HOST16( i16) \
     (((unsigned short)*(i16)<< 8)+ (unsigned short)*(i16+ 1))
#   define BIGC_TO_HOST32( i32) \
     (((unsigned int)*(i32)<< 24) + ((unsigned int)*(i32+ 1)<< 16) +\
     ((unsigned int)*(i32+ 2)<< 8)+ ((unsigned int)*(i32+ 3)))
#   define HOST16_TO_BIGC( uca, i16) \
     *(uca) = ((i16)>> 8)& 0xff; *(uca+ 1) = (i16)& 0xff;
#   define HOST32_TO_BIGC( uca, i32) \
     *(uca) = ((i32)>> 24)& 0xff; *(uca+ 1) = ((i32)>> 16)& 0xff; \
     *(uca+ 2) = ((i32)>> 8)& 0xff; *(uca+ 3) = (i32)& 0xff;
#  endif /* defined( __ppc__) || defined( __ppc64__) [else] */

#  pragma pack(4)               /* 32-bit alignment, regardless. */

/* Finder info attribute buffer structure for getattrlist(). */
typedef struct {
  unsigned int  ret_length;
  fsobj_type_t  obj_type;
  char          fndr_info[ APL_DBL_SIZE_FNDR_INFO];
} attr_bufr_fndr_t;

/* Resource fork attribute buffer structure for getattrlist(). */
typedef struct {
  unsigned int  ret_length;
  off_t         size;
} attr_bufr_rsrc_t;

#  pragma options align=reset

/* Buffer for AppleDouble header (including Finder info). */
extern
unsigned char *apl_dbl_hdr;

extern
int apl_dbl_hdr_alloc;      /* Allocated size of apl_dbl_hdr. */

/* Pointer to fake buffer for file_read().  NULL, if no fake data. */
extern
unsigned char *file_read_fake_buf;

/* Byte count of data in file_read_fake_buf. */
extern
size_t file_read_fake_len;

# endif /* defined( unix) && defined( __APPLE__) */

#endif /* ndef __MACOSX_H */

