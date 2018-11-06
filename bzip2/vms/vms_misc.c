/* 2007-04-05 SMS.
   VMS-specific code for BZIP2.
*/

#include <errno.h>
#include <stat.h>
#include <stdio.h>
#include <string.h>

/* Work around /NAMES = AS_IS problems on VAX. */

#ifdef VAX
# ifndef sys$close
#  define sys$close SYS$CLOSE
# endif /* ndef sys$close */
# ifndef sys$getjpiw
#  define sys$getjpiw SYS$GETJPIW
# endif /* ndef sys$getjpiw */
# ifndef sys$open
#  define sys$open SYS$OPEN
# endif /* ndef sys$open */
# ifndef sys$parse
#  define sys$parse SYS$PARSE
# endif /* ndef sys$parse */
# ifndef sys$search
#  define sys$search SYS$SEARCH
# endif /* ndef sys$search */
#endif /* def VAX */

#include <starlet.h>
#include <jpidef.h>
#if 0
#include <fabdef.h>
#include <namdef.h>
#include <rabdef.h>
#endif /* 0 */
#include <fab.h>
#include <nam.h>
#include <rab.h>
#include <rmsdef.h>
#include <stsdef.h>
#include <xabdef.h>
#include <xabitmdef.h>

#include "vms_misc.h"


/* Define macros for use with either NAM or NAML. */

#ifdef NAML$C_MAXRSS            /* NAML is available.  Use it. */

#  define NAM_STRUCT NAML

#  define FAB_OR_NAML( fab, nam) nam
#  define FAB_OR_NAML_DNA naml$l_long_defname
#  define FAB_OR_NAML_DNS naml$l_long_defname_size
#  define FAB_OR_NAML_FNA naml$l_long_filename
#  define FAB_OR_NAML_FNS naml$l_long_filename_size

#  define CC_RMS_NAM cc$rms_naml
#  define FAB_NAM fab$l_naml
#  define NAME_DNA naml$l_long_defname
#  define NAME_DNS naml$l_long_defname_size
#  define NAME_FNA naml$l_long_filename
#  define NAME_FNS naml$l_long_filename_size
#  define NAM_DID naml$w_did
#  define NAM_DVI naml$t_dvi
#  define NAM_ESA naml$l_long_expand
#  define NAM_ESL naml$l_long_expand_size
#  define NAM_ESS naml$l_long_expand_alloc
#  define NAM_FID naml$w_fid
#  define NAM_FNB naml$l_fnb
#  define NAM_RSA naml$l_long_result
#  define NAM_RSL naml$l_long_result_size
#  define NAM_RSS naml$l_long_result_alloc
#  define NAM_MAXRSS NAML$C_MAXRSS
#  define NAM_NOP naml$b_nop
#  define NAM_M_SYNCHK NAML$M_SYNCHK
#  define NAM_B_DEV naml$l_long_dev_size
#  define NAM_L_DEV naml$l_long_dev
#  define NAM_B_DIR naml$l_long_dir_size
#  define NAM_L_DIR naml$l_long_dir
#  define NAM_B_NAME naml$l_long_name_size
#  define NAM_L_NAME naml$l_long_name
#  define NAM_B_TYPE naml$l_long_type_size
#  define NAM_L_TYPE naml$l_long_type
#  define NAM_B_VER naml$l_long_ver_size
#  define NAM_L_VER naml$l_long_ver

#else /* def NAML$C_MAXRSS */   /* NAML is not available.  Use NAM. */

#  define NAM_STRUCT NAM

#  define FAB_OR_NAML( fab, nam) fab
#  define FAB_OR_NAML_DNA fab$l_dna
#  define FAB_OR_NAML_DNS fab$b_dns
#  define FAB_OR_NAML_FNA fab$l_fna
#  define FAB_OR_NAML_FNS fab$b_fns

#  define CC_RMS_NAM cc$rms_nam
#  define FAB_NAM fab$l_nam
#  define NAME_DNA fab$l_dna
#  define NAME_DNS fab$b_dns
#  define NAME_FNA fab$l_fna
#  define NAME_FNS fab$b_fns
#  define NAM_DID nam$w_did
#  define NAM_DVI nam$t_dvi
#  define NAM_ESA nam$l_esa
#  define NAM_ESL nam$b_esl
#  define NAM_ESS nam$b_ess
#  define NAM_FID nam$w_fid
#  define NAM_FNB nam$l_fnb
#  define NAM_RSA nam$l_rsa
#  define NAM_RSL nam$b_rsl
#  define NAM_RSS nam$b_rss
#  define NAM_MAXRSS NAM$C_MAXRSS
#  define NAM_NOP nam$b_nop
#  define NAM_M_SYNCHK NAM$M_SYNCHK
#  define NAM_B_DEV nam$b_dev
#  define NAM_L_DEV nam$l_dev
#  define NAM_B_DIR nam$b_dir
#  define NAM_L_DIR nam$l_dir
#  define NAM_B_NAME nam$b_name
#  define NAM_L_NAME nam$l_name
#  define NAM_B_TYPE nam$b_type
#  define NAM_L_TYPE nam$l_type
#  define NAM_B_VER nam$b_ver
#  define NAM_L_VER nam$l_ver

#endif /* def NAML$C_MAXRSS */


/* 2005-09-29 SMS.
 *
 * vms_basename()
 *
 *    Extract the basename from a VMS file spec.
 */

char *vms_basename( char *file_spec)
{
    /* Static storage for NAM[L], and so on. */

    static struct NAM_STRUCT nam;
    static char exp_name[ NAM_MAXRSS+ 1];
    static char res_name[ NAM_MAXRSS+ 1];

    struct FAB fab;
    int status;

    /* Set up the FAB and NAM[L] blocks. */

    fab = cc$rms_fab;                   /* Initialize FAB. */
    nam = CC_RMS_NAM;                   /* Initialize NAM[L]. */

    fab.FAB_NAM = &nam;                 /* FAB -> NAM[L] */

#ifdef NAML$C_MAXRSS

    fab.fab$l_dna = (char *) -1;    /* Using NAML for default name. */
    fab.fab$l_fna = (char *) -1;    /* Using NAML for file name. */

#endif /* def NAML$C_MAXRSS */

    /* Arg name and length. */
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNA = file_spec;
    FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNS = strlen( file_spec);

    nam.NAM_ESA = exp_name;         /* Expanded name. */
    nam.NAM_ESS = NAM_MAXRSS;       /* Max length. */
    nam.NAM_RSA = res_name;         /* Resulting name. */
    nam.NAM_RSS = NAM_MAXRSS;       /* Max length. */

    nam.NAM_NOP = NAM_M_SYNCHK;     /* Syntax-only analysis. */

    /* Parse the file name. */
    status = sys$parse( &fab);      /* What could go wrong? */

    nam.NAM_L_NAME[ nam.NAM_B_NAME] = '\0';

    return nam.NAM_L_NAME;
}


/* 2010-11-29 SMS.
 *
 * vms_redot()
 *
 *    De-caret-escape a caret-escaped last dot in a file spec.
 */
void vms_redot( char *file_spec)
{
    char chr;
    char chr_l1;
    int i;

    i = strlen( file_spec)- 1;

    /* Minimum length = 2 for "^.". */
    if (i > 0)
    {
        int j = 0;              /* j < 0 -> done.  j > 0 -> "^." found. */

        /* Loop through characters, right to left, until a directory
         * delimiter or a dot is reached.
         */
        chr_l1 = file_spec[ i];
        while ((i > 0) && (j == 0))
        {
            /* Shift our attention one character to the left. */
            chr = chr_l1;
            chr_l1 = file_spec[ --i];
            switch (chr)
            {
                /* Quit when a directory delimiter is reached. */
                case '/':
                case ']':
                case '>':
                    if (chr_l1 != '^')
                        j = -1;         /* Dir. delim.  (Nothing to do.) */
                    break;
                /* Quit when the right-most dot is reached. */
                case '.':
                    if (chr_l1 != '^')
                        j = -1;         /* Plain dot.  (Nothing to do.) */
                    else
                        j = i;          /* Caret-escaped dot. */
                    break;
            }
        }

        /* If a caret-escaped dot was found, then shift the dot, and
         * everything to its right, one position to the left.
         */
        if (j > 0)
        {
            char *cp = file_spec+ j;
            do
            {
                *cp = *(cp+ 1);
                cp++;
            } while (*cp != '\0');
        }
    }
}


/* 2005-09-26 SMS.
 *
 * vms_wild()
 *
 *    Expand a wild-card file spec.  Exclude directory files.
 *       First, call with real name.
 *       Thereafter, call with NULL arg (until NULL is returned).
 */

char *vms_wild( char *file_spec, int *wild)
{
    /* Static storage for FAB, NAM[L], XAB, and so on. */

    static struct NAM_STRUCT nam;
    static char exp_name[ NAM_MAXRSS+ 1];
    static char res_name[ NAM_MAXRSS+ 1];
    static struct FAB fab;

    /* XAB item descriptor set. */
    static int is_directory;
    static int xab_dir_len;

    static struct
    {
        xxi_item_t xab_dir_itm;
        int term;
    } xab_itm_lst =
     { { 4, XAB$_UCHAR_DIRECTORY, &is_directory, &xab_dir_len },
       0
     };

    static struct XABITM xab_items =
     { XAB$C_ITM, XAB$C_ITMLEN,
#ifndef VAX     /* VAX has a peculiar XABITM structure declaration. */
                                0,
#endif /* ndef VAX */
                                   NULL, &xab_itm_lst, XAB$K_SENSEMODE };

    static int vms_wild_detected;

    int status;
    int unsuitable;

    if (file_spec != NULL)
    {
        vms_wild_detected = 0;          /* Clear wild-card flag. */
        if (wild != NULL)
            *wild = 0;

        /* Set up the FAB and NAM[L] blocks. */

        fab = cc$rms_fab;               /* Initialize FAB. */
        nam = CC_RMS_NAM;               /* Initialize NAM[L]. */

        fab.FAB_NAM = &nam;             /* FAB -> NAM[L] */

        /* FAB items for XAB attribute sensing. */
        fab.fab$l_xab = (void *) &xab_items;
        fab.fab$b_shr = FAB$M_SHRUPD;   /* Play well with others. */
        fab.fab$b_fac = FAB$M_GET;
        fab.fab$v_nam = 1;              /* Use sys$search() results. */

#ifdef NAML$C_MAXRSS

        fab.fab$l_dna = (char *) -1;    /* Using NAML for default name. */
        fab.fab$l_fna = (char *) -1;    /* Using NAML for file name. */

#endif /* def NAML$C_MAXRSS */

        /* Arg wild name and length. */
        FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNA = file_spec;
        FAB_OR_NAML( fab, nam).FAB_OR_NAML_FNS = strlen( file_spec);

#define DEF_DEVDIR "SYS$DISK:[]*.*;0"

        /* Default file spec and length. */
        FAB_OR_NAML( fab, nam).FAB_OR_NAML_DNA = DEF_DEVDIR;
        FAB_OR_NAML( fab, nam).FAB_OR_NAML_DNS = sizeof( DEF_DEVDIR)- 1;

        nam.NAM_ESA = exp_name;         /* Expanded name. */
        nam.NAM_ESS = NAM_MAXRSS;       /* Max length. */
        nam.NAM_RSA = res_name;         /* Resulting name. */
        nam.NAM_RSS = NAM_MAXRSS;       /* Max length. */

        /* Parse the file name. */
        status = sys$parse( &fab);

        if (status != RMS$_NORMAL)
        {
            /* Parse failed.
               Return original file spec and let someone else complain.
            */
            return file_spec;
        }
        /* Set the local wild-card flag. */
        vms_wild_detected = ((nam.NAM_FNB& NAM$M_WILDCARD) != 0);
        if (wild != NULL)
            *wild = vms_wild_detected;
    }
    else if (vms_wild_detected == 0)
    {
        /* Non-first call with no wild-card in file spec.  Done. */
        return NULL;
    }

    /* Search for the next matching file spec. */
    unsuitable = 1;
    while (unsuitable != 0)
    {
        status = sys$search( &fab);

        if (status == RMS$_NORMAL)
        {
            /* Found one.  If suitable, return resultant file spec. */
            status = sys$open( &fab);

            /* Clear internal file index.
               (Required after sys$open(), for next sys$search().)
            */
            fab.fab$w_ifi = 0;

            if (status == RMS$_NORMAL)
            {
                unsuitable = is_directory;
                status = sys$close( &fab);
            }
            else
            {
                /* Open failed.  Let someone else complain. */
                unsuitable = 0;
            }

            if (!unsuitable)
            {
                /* Suitable.  Return the resultant file spec. */
                res_name[ nam.NAM_RSL] = '\0';
                return res_name;
            }
        }
        else if (status == RMS$_NMF)
        {
            /* No more (wild-card) files.  Done. */
            return NULL;
        }
        else
        {
            /* Unexpected search failure.
               Return expanded file spec and let someone else complain.
               Could probably return the original spec instead.
            */
            exp_name[ nam.NAM_ESL] = '\0';
            return exp_name;
        }
    }
}


/* 2005-09-26 SMS.
 *
 * trimFileNameVersion()
 *
 *    Terminate a file name to discard (effectively) its version number.
 *
 *    Note: Exotic cases like A.B.1 (instead of A.B;1) are not handled
 *    properly, but using sys$parse() to get this right causes its own
 *    problems.  (String not trimmed in place, default device and
 *    directory added, and so on.)
 */

void trimFileNameVersion( char *file_name)
{
   char *cp;

   /* Find the first (apparent) non-version-digit. */
   for (cp = file_name+ strlen( file_name)- 1;
    (*cp >= '0') && (*cp <= '9') && (cp != file_name);
    cp--);

   /* If the pre-digit character exists and is an unescaped
      semi-colon, then terminate the string at the semi-colon.
   */
   if (cp != file_name)
   {
      if ((*cp == ';') && (*(cp- 1) != '^'))
      {
         *cp = '\0';
      }
   }   
   return;
}


/* 2004-11-23 SMS.
 *
 *       get_rms_defaults().
 *
 *    Get user-specified values from (DCL) SET RMS_DEFAULT.  FAB/RAB
 *    items of particular interest are:
 *
 *       fab$w_deq         default extension quantity (blocks) (write).
 *       rab$b_mbc         multi-block count.
 *       rab$b_mbf         multi-buffer count (used with rah and wbh).
 */

extern int verbosity;

#define DIAG_FLAG (verbosity >= 2)

/* Default RMS parameter values. */

#define RMS_DEQ_DEFAULT 16384   /* About 1/4 the max (65535 blocks). */
#define RMS_MBC_DEFAULT 127     /* The max, */
#define RMS_MBF_DEFAULT 2       /* Enough to enable rah and wbh. */

/* Durable storage */

static int rms_defaults_known = 0;

/* JPI item buffers. */
static unsigned short rms_ext;
static char rms_mbc;
static unsigned char rms_mbf;

/* Active RMS item values. */
unsigned short rms_ext_active;
char rms_mbc_active;
unsigned char rms_mbf_active;

/* GETJPI item lengths. */
static int rms_ext_len;         /* Should come back 2. */
static int rms_mbc_len;         /* Should come back 1. */
static int rms_mbf_len;         /* Should come back 1. */

/* GETJPI item descriptor set. */

struct
    {
    xxi_item_t rms_ext_itm;
    xxi_item_t rms_mbc_itm;
    xxi_item_t rms_mbf_itm;
    int term;
    } jpi_itm_lst =
     { { 2, JPI$_RMS_EXTEND_SIZE, &rms_ext, &rms_ext_len },
       { 1, JPI$_RMS_DFMBC, &rms_mbc, &rms_mbc_len },
       { 1, JPI$_RMS_DFMBFSDK, &rms_mbf, &rms_mbf_len },
       0
     };

int get_rms_defaults()
{
int sts;

/* Get process RMS_DEFAULT values. */

sts = sys$getjpiw( 0, 0, 0, &jpi_itm_lst, 0, 0, 0);
if ((sts& STS$M_SEVERITY) != STS$K_SUCCESS)
    {
    /* Failed.  Don't try again. */
    rms_defaults_known = -1;
    }
else
    {
    /* Fine, but don't come back. */
    rms_defaults_known = 1;
    }

/* Limit the active values according to the RMS_DEFAULT values. */

if (rms_defaults_known > 0)
    {
    /* Set the default values. */

    rms_ext_active = RMS_DEQ_DEFAULT;
    rms_mbc_active = RMS_MBC_DEFAULT;
    rms_mbf_active = RMS_MBF_DEFAULT;

    /* Default extend quantity.  Use the user value, if set. */
    if (rms_ext > 0)
        {
        rms_ext_active = rms_ext;
        }

    /* Default multi-block count.  Use the user value, if set. */
    if (rms_mbc > 0)
        {
        rms_mbc_active = rms_mbc;
        }

    /* Default multi-buffer count.  Use the user value, if set. */
    if (rms_mbf > 0)
        {
        rms_mbf_active = rms_mbf;
        }
    }

if (DIAG_FLAG)
    {
    fprintf( stderr,
     "Get RMS defaults.  getjpi sts = %%x%08x.\n",
     sts);

    if (rms_defaults_known > 0)
        {
        fprintf( stderr,
         "               Default: deq = %6d, mbc = %3d, mbf = %3d.\n",
         rms_ext, rms_mbc, rms_mbf);
        }
    }
return sts;
}


/* 2004-11-23 SMS.
 *
 *       acc_cb(), access callback function for DEC C fopen().
 *
 *    Set some RMS FAB/RAB items, with consideration of user-specified
 * values from (DCL) SET RMS_DEFAULT.  Items of particular interest are:
 *
 *       fab$w_deq         default extension quantity (blocks).
 *       rab$b_mbc         multi-block count.
 *       rab$b_mbf         multi-buffer count (used with rah and wbh).
 *
 *    See also the FOP* macros in VMS.H.  Currently, no notice is
 * taken of the caller-ID value, but options could be set differently
 * for read versus write access.  (I assume that specifying fab$w_deq,
 * for example, for a read-only file has no ill effects.)
 */

/* Global storage. */

int fopr_id = FOPR_ID;          /* Callback id storage, read. */
int fopw_id = FOPW_ID;          /* Callback id storage, write. */

/* acc_cb() */

int acc_cb( int *id_arg, struct FAB *fab, struct RAB *rab)
{
int sts;

/* Get process RMS_DEFAULT values, if not already done. */
if (rms_defaults_known == 0)
    {
    get_rms_defaults();
    }

/* If RMS_DEFAULT (and adjusted active) values are available, then set
 * the FAB/RAB parameters.  If RMS_DEFAULT values are not available,
 * suffer with the default parameters.
 */
if (rms_defaults_known > 0)
    {
    /* Set the FAB/RAB parameters accordingly. */
    fab-> fab$w_deq = rms_ext_active;
    rab-> rab$b_mbc = rms_mbc_active;
    rab-> rab$b_mbf = rms_mbf_active;

    /* Truncate at EOF on close, as we'll probably over-extend. */
    fab-> fab$v_tef = 1;

    /* If using multiple buffers, enable read-ahead and write-behind. */
    if (rms_mbf_active > 1)
        {
        rab-> rab$v_rah = 1;
        rab-> rab$v_wbh = 1;
        }

    /* Set the "sequential access only" flag to avoid excessive lock
       time when writing on a file system with highwater marking
       enabled.
    */
    fab-> fab$v_sqo = 1;

    if (DIAG_FLAG)
        {
        fprintf( stderr,
         "Open callback.  ID = %d, deq = %6d, mbc = %3d, mbf = %3d.\n",
         *id_arg, fab-> fab$w_deq, rab-> rab$b_mbc, rab-> rab$b_mbf);
        }
    }

/* Declare success. */
return 0;
}


/*
 * 2004-09-19 SMS.
 *
 *----------------------------------------------------------------------
 *
 *       decc_init()
 *
 *    On non-VAX systems, uses LIB$INITIALIZE to set a collection of C
 *    RTL features without using the DECC$* logical name method.
 *
 *----------------------------------------------------------------------
 */

#ifdef __CRTL_VER

#if !defined( __VAX) && (__CRTL_VER >= 70301000)

#include <unixlib.h>

/*--------------------------------------------------------------------*/

/* Global storage. */

/*    Flag to sense if decc_init() was called. */

int decc_init_done = -1;

/*--------------------------------------------------------------------*/

/* decc_init()

      Uses LIB$INITIALIZE to set a collection of C RTL features without
      requiring the user to define the corresponding logical names.
*/

/* Structure to hold a DECC$* feature name and its desired value. */

typedef struct
   {
   char *name;
   int value;
   } decc_feat_t;

/* Array of DECC$* feature names and their desired values. */

decc_feat_t decc_feat_array[] = {

   /* Preserve command-line case with SET PROCESS/PARSE_STYLE=EXTENDED */
 { "DECC$ARGV_PARSE_STYLE", 1 },

   /* Preserve case for file names on ODS5 disks. */
 { "DECC$EFS_CASE_PRESERVE", 1 },

   /* Enable multiple dots (and most characters) in ODS5 file names,
      while preserving VMS-ness of ";version". */
 { "DECC$EFS_CHARSET", 1 },

   /* List terminator. */
 { (char *)NULL, 0 } };

/* LIB$INITIALIZE initialization function. */

static void decc_init( void)
{
int feat_index;
int feat_value;
int feat_value_max;
int feat_value_min;
int i;
int sts;

/* Set the global flag to indicate that LIB$INITIALIZE worked. */

decc_init_done = 1;

/* Loop through all items in the decc_feat_array[]. */

for (i = 0; decc_feat_array[i].name != NULL; i++)
   {
   /* Get the feature index. */
   feat_index = decc$feature_get_index( decc_feat_array[i].name);
   if (feat_index >= 0)
      {
      /* Valid item.  Collect its properties. */
      feat_value = decc$feature_get_value( feat_index, 1);
      feat_value_min = decc$feature_get_value( feat_index, 2);
      feat_value_max = decc$feature_get_value( feat_index, 3);

      if ((decc_feat_array[i].value >= feat_value_min) &&
          (decc_feat_array[i].value <= feat_value_max))
         {
         /* Valid value.  Set it if necessary. */
         if (feat_value != decc_feat_array[i].value)
            {
            sts = decc$feature_set_value( feat_index,
             1,
             decc_feat_array[i].value);
            }
         }
      else
         {
         /* Invalid DECC feature value. */
         fprintf( stderr,
          " INVALID DECC FEATURE VALUE, %d: %d <= %s <= %d.\n",
          feat_value, feat_value_min, decc_feat_array[i].name,
          feat_value_max);
         }
      }
   else
      {
      /* Invalid DECC feature name. */
      fprintf( stderr,
       " UNKNOWN DECC FEATURE: %s.\n", decc_feat_array[i].name);
      }
   }
}

/* Get "decc_init()" into a valid, loaded LIB$INITIALIZE PSECT. */

#pragma nostandard

/* Establish the LIB$INITIALIZE PSECT, with proper alignment and
   attributes.
*/
globaldef { "LIB$INITIALIZ" } readonly _align (LONGWORD)
   int spare[8] = { 0 };
globaldef { "LIB$INITIALIZE" } readonly _align (LONGWORD)
   void (*x_decc_init)() = decc_init;

/* Fake reference to ensure loading the LIB$INITIALIZE PSECT. */

#pragma extern_model save
int LIB$INITIALIZE( void);
#pragma extern_model strict_refdef
int dmy_lib$initialize = (int) LIB$INITIALIZE;
#pragma extern_model restore

#pragma standard

#endif /* !defined( __VAX) && (__CRTL_VER >= 70301000) */

#endif /* def __CRTL_VER */


/* Private replacement functions for old C RTL. */

/*
 * str[n]casecmp() replacement for old C RTL.
 * Assumes a prehistorically incompetent toupper().
 */
#if __CRTL_VER < 70000000

#include <ctype.h>
#include <stdlib.h>

int bz2_prvt_strncasecmp( const char *s1, const char *s2, size_t n)
{
  /* Initialization prepares for n == 0. */
  char c1 = '\0';
  char c2 = '\0';

  while (n-- > 0)
  {
    /* Set c1 and c2.  Convert lower-case characters to upper-case. */
    if (islower( c1 = *s1))
      c1 = toupper( c1);

    if (islower( c2 = *s2))
      c2 = toupper( c2);

    /* Quit at inequality or NUL. */
    if ((c1 != c2) || (c1 == '\0'))
      break;

    s1++;
    s2++;
  }
return ((unsigned int) c1- (unsigned int) c2);
}


/*
 * strdup() replacement for old C RTL.
 */
char *bz2_prvt_strdup( const char *s1)
{
    char *s2;

    s2 = malloc( strlen( s1));
    if (s2 != NULL)
    {
        strcpy( s2, s1);
    }
    return s2;
}

#endif /* __CRTL_VER < 70000000 */


#if defined( __VAX) || (__CRTL_VER < 80300000)

#include <unixio.h>

int bz2_prvt_fchmod( int fd, mode_t mode)
{
    char file_name[ NAM_MAXRSS+ 1];

    /* Get the file name associated with the (open) file descriptor. */
    if (getname(fd, file_name) == 0)
    {
        /* getname() failed.  Return failure code. */
        return 0;
    }
    else
    {
        /* getname() ok.  Return chmod() result. */
        return chmod( file_name, mode);
    }
}

#endif /* defined( __VAX) || (__CRTL_VER < 80300000) */


#if __CRTL_VER < 70300000

#include <unistd.h>

int bz2_prvt_fchown( int fd, uid_t owner, gid_t group)
{
    char file_name[ NAM_MAXRSS+ 1];

    /* Get the file name associated with the (open) file descriptor. */
    if (getname(fd, file_name) == 0)
    {
        /* getname() failed.  Return failure code. */
        return 0;
    }
    else
    {
        /* getname() ok.  Return chown() result. */
        return chown( file_name, owner, group);
    }
}

#endif /* __CRTL_VER < 70300000 */

