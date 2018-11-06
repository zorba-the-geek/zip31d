/*
  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
  crypt.h (full version) by Info-ZIP.

  The main (Traditional) encryption/decryption source code for Info-Zip
  software was originally written in Europe.  To the best of our knowledge,
  it can be freely distributed in both source and object forms from any
  country, including the USA under License Exception TSU of the U.S. Export
  Administration Regulations (section 740.13(e)) of 6 June 2002.

  NOTE on copyright history:
  Some previous versions of this source package (up to version 2.8) were
  not copyrighted and put in the public domain.  If you cannot comply
  with the Info-Zip LICENSE, you may want to look for one of those
  public domain versions.
 */

/* This file is part of the Traditional (ZipCrypto) kit, which includes
 * this file and crypt.c.  Now that Traditional Zip encryption is included
 * in the base source kits (Zip, UnZip), these files are no longer supplied
 * seperately and are now considered part of each base source kit.
 *
 * See README_CR.txt for more information on the ZCRYPT Traditional
 * encryption package.  The version of ZCRYPT is now fixed at 3.0.  This will
 * change only if the algorithms for Traditional encryption change.  Further
 * changes to crypt.h and crypt.c will be tracked as part of each base source
 * kit.
 *
 * AES_WG encryption uses files in the aes_wg directory in addition to the
 * basic crypt.c and crypt.h.  These files must be added from a separate
 * Info-ZIP AES WG kit.  This implementation of AES encryption is intended to
 * be compatible with the WinZip AES implementation, and uses AES encryption
 * code supplied by Brian Gladman.  For more information, see the file
 * aes_wg/README_AES_WG.txt in the Info-ZIP AES WG kit.  See INSTALL
 * for information on how to get that kit.
 *
 * Contact us if an encryption-free version of either Zip or UnZip is needed.
 */

#ifndef __crypt_h       /* Don't include more than once. */
# define __crypt_h

# ifdef IZ_CRYPT_ANY
#  undef IZ_CRYPT_ANY
# endif
# if defined( IZ_CRYPT_AES_WG) || defined( IZ_CRYPT_TRAD)
#  define IZ_CRYPT_ANY
# endif /* defined( IZ_CRYPT_AES_WG) || defined( IZ_CRYPT_TRAD) */

# ifdef IZ_CRYPT_ANY

#  ifdef IZ_CRYPT_AES_WG
#   include "aes_wg/fileenc.h"
#  endif /* def IZ_CRYPT_AES_WG */

/* Set this to 1 for beta, 0 for release. */
#if 0
# define CR_BETA
#endif

#  define CR_MAJORVER        3
#  define CR_MINORVER        0
#  ifdef CR_BETA
#   define CR_BETA_VER      "l BETA"
#   define CR_VERSION_DATE  "21 Mar 2013"       /* Last real code change. */
#  else
#   define CR_BETA_VER      ""
#   define CR_VERSION_DATE  "30 Aug 2015"       /* Last public release date. */
#   define CR_RELEASE
#  endif

#  ifndef __G           /* UnZip only, for now (DLL stuff). */
#   define __G
#   define __G__
#   define __GDEF
#   define __GPRO    void
#   define __GPRO__
#  endif

#  if defined(MSDOS) || defined(OS2) || defined(WIN32)
#   ifndef DOS_OS2_W32
#    define DOS_OS2_W32
#   endif
#  endif

#  if defined(DOS_OS2_W32) || defined(__human68k__)
#   ifndef DOS_H68_OS2_W32
#    define DOS_H68_OS2_W32
#   endif
#  endif

#  if defined(VM_CMS) || defined(MVS)
#   ifndef CMS_MVS
#    define CMS_MVS
#   endif
#  endif

/* To allow combining of Zip and UnZip static libraries in a single binary,
 * the Zip and UnZip versions of the crypt core functions must have
 * different names.
 */
#  ifdef ZIP
#   ifdef REALLY_SHORT_SYMS
#    define decrypt_byte   zdcrby
#   else
#    define decrypt_byte   zp_decrypt_byte
#   endif
#   define  update_keys    zp_update_keys
#   define  init_keys      zp_init_keys
#  else /* def ZIP */
#   ifdef REALLY_SHORT_SYMS
#    define decrypt_byte   dcrbyt
#   endif
#  endif /* def ZIP [else] */

#  define IZ_PWLEN 256  /* Input buffer size for reading encryption key. */
#  ifndef PWLEN         /* for compatibility with older zcrypt release. */
#   define PWLEN IZ_PWLEN
#  endif
#  define RAND_HEAD_LEN 12      /* Length of Trad. encryption random header. */

/* Encrypted data header and password check buffer sizes.
 * (One buffer accommodates both types.)
 */
#  ifdef IZ_CRYPT_AES_WG
    /* All data from extra field block. */
#   if (MAX_SALT_LENGTH + 2 > RAND_HEAD_LEN)
#    define ENCR_HEAD_LEN (MAX_SALT_LENGTH + 2)
#   endif
    /* Data required for password check. */
#   if (PWD_VER_LENGTH > RAND_HEAD_LEN)
#    define ENCR_PW_CHK_LEN PWD_VER_LENGTH
#   endif
#  endif /* def IZ_CRYPT_AES_WG */

#  ifndef ENCR_HEAD_LEN
#   define ENCR_HEAD_LEN RAND_HEAD_LEN
#  endif
#  ifndef ENCR_PW_CHK_LEN
#   define ENCR_PW_CHK_LEN RAND_HEAD_LEN
#  endif

/* The crc_32_tab array must be provided externally for the crypt calculus. */

/* Encode byte c, using temp t.  Warning: c must not have side effects. */
#  define zencode(c,t)  (t=decrypt_byte(__G), update_keys(c), t^(c))

/* Decode byte c in place. */
#  define zdecode(c)   update_keys(__G__ c ^= decrypt_byte(__G))

int  decrypt_byte OF((__GPRO));
int  update_keys OF((__GPRO__ int c));
void init_keys OF((__GPRO__ ZCONST char *passwd));

#  ifdef ZIP
void crypthead OF((ZCONST char *, ulg));
#   ifdef UTIL
int zipcloak OF((struct zlist far *, ZCONST char *));
int zipbare OF((struct zlist far *, ZCONST char *));
#   else /* def UTIL */
unsigned zfwrite OF((zvoid *, extent, extent));
extern char *key;             /* Encryption key */
extern char *passwd;          /* Password before keyfile content added */
#   endif /* def UTIL [else] */
#  endif /* def ZIP */

#  if (defined(UNZIP) && !defined(FUNZIP))
int  decrypt OF((__GPRO__ ZCONST char *passwrd));
#  endif

#  ifdef FUNZIP
extern int encrypted;
#   ifdef NEXTBYTE
#    undef NEXTBYTE
#   endif
#   define NEXTBYTE \
     (encrypted? update_keys(__G__ getc(G.in)^decrypt_byte(__G)) : getc(G.in))
#  endif /* def FUNZIP */

# else /* def IZ_CRYPT_ANY */

/* Dummy version. */

#  define zencode
#  define zdecode

#  define zfwrite(b,s,c) bfwrite(b,s,c,BFWRITE_DATA)

# endif /* def IZ_CRYPT_ANY [else] */
#endif /* ndef __crypt_h */
