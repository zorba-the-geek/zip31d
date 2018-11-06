/* 2007-04-05 SMS.
   VMS-specific header associated with vms.c.
*/

#ifndef _VMS_H
#define _VMS_H

/* GETxxI item descriptor structure. */
typedef struct
    {
    short buf_len;
    short itm_cod;
    void *buf;
    int *ret_len;
    } xxi_item_t;

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

/* File open callback ID values. */

#  define FOPR_ID 1
#  define FOPW_ID 2

/* File open callback ID storage. */

extern int fopr_id;
extern int fopw_id;

/* File open callback ID function. */

extern int acc_cb();

/* File name version trim function. */

extern void trimFileNameVersion( char *file_name);

/* Basename extraction function. */

extern char *vms_basename( char *file_spec);

/* File spec last-dot adjustment. */

extern void vms_redot( char *file_spec);

/* Wild-card file name expansion function. */

extern char *vms_wild( char *file_spec, int *wild);


/* Private C RTL replacement functions for old C RTL. */

#   if __CRTL_VER < 70000000

#      ifndef UINT_MAX
#         define UINT_MAX 4294967295U
#      endif

#      define strdup( a) bz2_prvt_strdup( a)
       char *strdup( const char *);

#      define strcasecmp( s1, s2) bz2_prvt_strcasecmp( s1, s2)
#      define bz2_prvt_strcasecmp( s1, s2) bz2_prvt_strncasecmp( s1, s2, UINT_MAX)
#      define strncasecmp( s1, s2, n) bz2_prvt_strncasecmp( s1, s2, n)
       int strncasecmp( const char *, const char *, size_t);

#   endif /* __CRTL_VER < 70000000 */

/* Note: The __CRTL_VER condition here for fchmod() is estimated. */
#   if defined( __VAX) || (__CRTL_VER < 80300000)

#      ifndef __MODE_T
          typedef unsigned short mode_t;
#      endif /* ndef __MODE_T */

#      define fchmod( fd, mode) bz2_prvt_fchmod( fd, mode)
       int bz2_prvt_fchmod( int fd, mode_t mode);

#   endif /* defined( __VAX) || (__CRTL_VER < 80300000) */

#   if __CRTL_VER < 70300000

#      ifndef __GID_T
          typedef unsigned int gid_t;
#      endif /* ndef __GID_T */

#      ifndef __UID_T
          typedef unsigned int uid_t;
#      endif /* ndef __UID_T */

#      define fchown( fd, owner, group) bz2_prvt_fchown( fd, owner, group)
       int bz2_prvt_fchown( int fd, uid_t owner, gid_t group);

#   endif /* __CRTL_VER < 70300000 */

#endif /* ndef _VMS_H */

