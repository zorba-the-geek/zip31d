/*
  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
  crypt.c (full version) by Info-ZIP.      Last revised:  [see crypt.h]

  The main encryption/decryption source code for Info-Zip software was
  originally written in Europe.  To the best of our knowledge, it can
  be freely distributed in both source and object forms from any country,
  including the USA under License Exception TSU of the U.S. Export
  Administration Regulations (section 740.13(e)) of 6 June 2002.

  NOTE on copyright history:
  Previous versions of this source package (up to version 2.8) were
  not copyrighted and put in the public domain.  If you cannot comply
  with the Info-Zip LICENSE, you may want to look for one of those
  public domain versions.
 */

/*
  This encryption code is a direct transcription of the algorithm from
  Roger Schlafly, described by Phil Katz in the file appnote.txt.  This
  file (appnote.txt) is distributed with the PKZIP program (even in the
  version without encryption capabilities).
 */

/*
  crypt.c now includes part of the AES_WG encryption implementation (in
  IZ_CRYPT_AES_WG blocks).  This code is provided under the Info-ZIP
  license.  If this code enabled, it uses the Gladman AES code (or
  equivalent), which is distributed separately.

  See crypt.h for additional information.
 */

#define ZCRYPT_INTERNAL         /* Ensure <windows.h>, where applicable. */

#include "zip.h"
#include "crypt.h"              /* Get CRYPT defined as desired. */

#ifdef IZ_CRYPT_ANY

# include "ttyio.h"

# ifndef FALSE
#  define FALSE 0
# endif

# ifdef ZIP
/*   Implementation notes on traditional zip (weak) encryption.
 *
 *   For the encoding task used in Zip (and ZipCloak), we want to initialize
 *   the crypt algorithm with some reasonably unpredictable bytes; see
 *   the crypthead() function.  The standard rand() library function is
 *   used to supply these `random' bytes, which in turn is initialized by
 *   a srand() call. The srand() function takes an "unsigned" (at least 16bit)
 *   seed value as argument to determine the starting point of the rand()
 *   pseudo-random number generator.
 *
 *   This seed number is constructed as "Seed = Seed1 .XOR. Seed2" with
 *   Seed1 supplied by the current time (= "(unsigned)time()") and Seed2
 *   as some (hopefully) nondeterministic bitmask.  On many (most) systems,
 *   we use some process-specific number, as the PID or something similar,
 *   but when nothing unpredictable is available, a fixed number may be
 *   sufficient.
 *
 *   NOTE:
 *   1.) This implementation requires the availability of the following
 *       standard UNIX C runtime library functions: time(), rand(), srand().
 *       On systems where some of them are missing, the environment that
 *       incorporates the crypt routines must supply suitable replacement
 *       functions.
 *   2.) It is a very bad idea to use a second call to time() to set the
 *       "Seed2" number! In this case, both "Seed1" and "Seed2" would be
 *       (almost) identical, resulting in a (mostly) "zero" constant seed
 *       number passed to srand().
 *
 *   The implementation environment defined in the "zip.h" header should
 *   supply a reasonable definition for ZCR_SEED2 (an unsigned number; for
 *   most implementations of rand() and srand(), only the lower 16 bits are
 *   significant!).  An example that works on many systems would be
 *         "#define ZCR_SEED2  (unsigned)getpid()".
 *   The default definition for ZCR_SEED2 supplied below should be regarded
 *   as a fallback to allow successful compilation in "beta state"
 *   environments.
 */

#  include <time.h>     /* time() function supplies first part of crypt seed */
   /* "last resort" source for second part of crypt seed pattern */
#  ifndef ZCR_SEED2
#   define ZCR_SEED2 3141592654UL       /* use PI as default pattern */
#  endif
#  ifdef GLOBAL         /* used in Amiga system headers, maybe others too */
#   undef GLOBAL
#  endif
#  define GLOBAL(g) g           /* Zip global reference. */
# else /* def ZIP */
#  define GLOBAL(g) G.g         /* UnZip global reference. */
# endif /* def ZIP [else] */


# ifdef UNZIP
   /* char *key = (char *)NULL; moved to globals.h */
#  ifndef FUNZIP
local int testp OF((__GPRO__ int hd_len, ZCONST uch *h));
local int testkey OF((__GPRO__ int hd_len, ZCONST uch *h,
                      ZCONST char *key));
#  endif /* ndef FUNZIP */
# else /* def UNZIP */          /* moved to globals.h for UnZip */
local z_uint4 keys[3];          /* keys defining the pseudo-random sequence */
# endif /* def UNZIP [else] */

# ifndef Trace
#  ifdef IZ_CRYPT_DEBUG
#   define Trace(x) fprintf x
#  else
#   define Trace(x)
#  endif
# endif

# include "crc32.h"

# ifdef IZ_CRC_BE_OPTIMIZ
local z_uint4 near crycrctab[256];
local z_uint4 near *cry_crctb_p = NULL;
local z_uint4 near *crytab_init OF((__GPRO));
#  define CRY_CRC_TAB  cry_crctb_p
#  undef CRC32
#  define CRC32(c, b, crctab) (crctab[((int)(c) ^ (b)) & 0xff] ^ ((c) >> 8))
# else /* def IZ_CRC_BE_OPTIMIZ */
#  define CRY_CRC_TAB  CRC_32_TAB
# endif /* def IZ_CRC_BE_OPTIMIZ [else] */


# ifdef IZ_CRYPT_TRAD

/***********************************************************************
 * Return the next byte in the pseudo-random sequence
 */
int decrypt_byte(__G)
    __GDEF
{
    unsigned temp;  /* POTENTIAL BUG:  temp*(temp^1) may overflow in an
                     * unpredictable manner on 16-bit systems; not a problem
                     * with any known compiler so far, though */

    temp = ((unsigned)GLOBAL(keys[2]) & 0xffff) | 2;
    return (int)(((temp * (temp ^ 1)) >> 8) & 0xff);
}


/***********************************************************************
 * Update the encryption keys with the next byte of plain text
 */
int update_keys(__G__ c)
    __GDEF
    int c;                      /* byte of plain text */
{
    GLOBAL(keys[0]) = CRC32(GLOBAL(keys[0]), c, CRY_CRC_TAB);
    GLOBAL(keys[1]) = (GLOBAL(keys[1])
                       + (GLOBAL(keys[0]) & 0xff))
                      * 134775813L + 1;
    {
      register int keyshift = (int)(GLOBAL(keys[1]) >> 24);
      GLOBAL(keys[2]) = CRC32(GLOBAL(keys[2]), keyshift, CRY_CRC_TAB);
    }
    return c;
}


/***********************************************************************
 * Initialize the encryption keys and the random header according to
 * the given password.
 */
void init_keys(__G__ passwd)
    __GDEF
    ZCONST char *passwd;        /* password string with which to modify keys */
{
#  ifdef IZ_CRC_BE_OPTIMIZ
    if (cry_crctb_p == NULL) {
        cry_crctb_p = crytab_init(__G);
    }
#  endif
    GLOBAL(keys[0]) = 305419896L;       /* 0x12345678. */
    GLOBAL(keys[1]) = 591751049L;       /* 0x23456789. */
    GLOBAL(keys[2]) = 878082192L;       /* 0x34567890. */
    while (*passwd != '\0') {
        update_keys(__G__ (int)*passwd);
        passwd++;
    }
}


/***********************************************************************
 * Initialize the local copy of the table of precomputed crc32 values.
 * Whereas the public crc32-table is optimized for crc32 calculations
 * on arrays of bytes, the crypt code needs the crc32 values in an
 * byte-order-independent form as 32-bit unsigned numbers. On systems
 * with Big-Endian byte order using the optimized crc32 code, this
 * requires inverting the byte-order of the values in the
 * crypt-crc32-table.
 */
#  ifdef IZ_CRC_BE_OPTIMIZ
local z_uint4 near *crytab_init(__G)
    __GDEF
{
    int i;

    for (i = 0; i < 256; i++) {
        crycrctab[i] = REV_BE(CRC_32_TAB[i]);
    }
    return crycrctab;
}
#  endif /* def IZ_CRC_BE_OPTIMIZ */

# endif /* def IZ_CRYPT_TRAD */


# ifdef ZIP

#  ifdef IZ_CRYPT_TRAD

/***********************************************************************
 * Write (Traditional) encryption header to file zfile using the
 * password passwd and the cyclic redundancy check crc.
 */
void crypthead(passwd, crc)
    ZCONST char *passwd;         /* password string */
    ulg crc;                     /* crc of file being encrypted */
{
    int n;                       /* index in random header */
    int t;                       /* temporary */
    int c;                       /* random byte */
    uch header[RAND_HEAD_LEN];   /* random header */
    static unsigned calls = 0;   /* ensure different random header each time */

    /* First generate RAND_HEAD_LEN-2 random bytes.  We encrypt the
     * output of rand() to get less predictability, because rand() is
     * often poorly implemented.
     */
    if (calls == 0)
    {
#ifndef IZ_CRYPT_SKIP_SRAND
        srand((unsigned)time(NULL) ^ ZCR_SEED2);
#endif /* ndef IZ_CRYPT_SKIP_SRAND */
        calls = 1;
    }
    init_keys(passwd);
    for (n = 0; n < RAND_HEAD_LEN-2; n++) {
        c = (rand() >> 7) & 0xff;
        header[n] = (uch)zencode(c, t);
    }
    /* Encrypt random header (last two bytes is high word of crc) */
    init_keys(passwd);
    for (n = 0; n < RAND_HEAD_LEN-2; n++) {
        header[n] = (uch)zencode(header[n], t);
    }
    header[RAND_HEAD_LEN-2] = (uch)zencode((int)(crc >> 16) & 0xff, t);
    header[RAND_HEAD_LEN-1] = (uch)zencode((int)(crc >> 24) & 0xff, t);
    bfwrite(header, 1, RAND_HEAD_LEN, BFWRITE_DATA);
}

#  endif /* def IZ_CRYPT_TRAD */


/***********************************************************************
 * zfwrite - write out buffer to file with encryption and byte counts
 *
 * If requested, encrypt the data in buf, and in any case call bfwrite()
 * with the arguments to zfwrite().  Return what bfwrite() returns.
 * bfwrite() keeps byte counts, does splits, and calls fwrite() to
 * write out the data.
 *
 * A bug has been found when encrypting large files that don't
 * compress.  See trees.c for the details and the fix.
 */
unsigned int zfwrite(buf, item_size, nb)
    zvoid *buf;                 /* data buffer */
    extent item_size;           /* size of each item in bytes */
    extent nb;                  /* number of items */
{
#ifdef IZ_CRYPT_TRAD
    int t;                      /* temporary */
#endif

    if (key != (char *)NULL)    /* key is the global password pointer */
    {
#  ifdef IZ_CRYPT_AES_WG
      if (encryption_method >= AES_MIN_ENCRYPTION)
      {
        /* assume all items are bytes */
        fcrypt_encrypt(buf, item_size * nb, &zctx);
      }
      else
      {
#  endif /* def IZ_CRYPT_AES_WG */
#  ifdef IZ_CRYPT_TRAD
        ulg size;               /* buffer size */
        char *p = (char *)buf;  /* steps through buffer */

        /* Encrypt data in buffer */
        for (size = (ulg)item_size*(ulg)nb; size != 0; p++, size--)
        {
            *p = (char)zencode(*p, t);
        }
#  else /* def IZ_CRYPT_TRAD */
        /* Do something in case the impossible happens here? */
#  endif /* def IZ_CRYPT_TRAD [else] */
#   ifdef IZ_CRYPT_AES_WG
      }
#   endif /* def IZ_CRYPT_AES_WG */
    }

    /* Write the buffer out */
    return (unsigned int)bfwrite(buf, item_size, nb, BFWRITE_DATA);
}


#  ifdef IZ_CRYPT_AES_WG

/***********************************************************************
 * Write AES_WG encryption header to file zfile.
 *
 * Size (bytes)    Content
 * Variable        Salt value
 * 2               Password verification value
 * Variable        Encrypted file data
 * 10              Authentication code
 *
 * Here, writing salt and password verification.
 */
void aes_crypthead( OFT( ZCONST uch *)salt,
                    OFT( int) salt_len,
                    OFT( ZCONST uch *)pwd_verifier)
#  ifdef NO_PROTO
    ZCONST uch *salt;
    int salt_len;
    ZCONST uch *pwd_verifier;
#  endif /* def NO_PROTO */
{
    bfwrite(salt, 1, salt_len, BFWRITE_DATA);
    bfwrite(pwd_verifier, 1, PWD_VER_LENGTH, BFWRITE_DATA);
}

#  endif /* def IZ_CRYPT_AES_WG */

# endif /* def ZIP */


/* ef_scan_for_aes() is used by UnZip and Zip utilities (ZipCloak). */

# if defined( UNZIP) || defined( UTIL)

#  ifdef IZ_CRYPT_AES_WG

/* SH() macro lifted from zipfile.c. */

#   ifdef THEOS
  /* Macros cause stack overflow in compiler */
ush SH(uch* p) { return ((ush)(uch)((p)[0]) | ((ush)(uch)((p)[1]) << 8)); }
#   else /* !THEOS */
  /* Macros for converting integers in little-endian to machine format */
#    define SH(a) ((ush)(((ush)(uch)(a)[0]) | (((ush)(uch)(a)[1]) << 8)))
#   endif /* ?THEOS */

#   define makeword( a) SH( a)


/******************************/
/* Function ef_scan_for_aes() */
/******************************/

/* 2012-11-25 SMS.  (OUSPG report.)
 * Changed eb_len and ef_len from unsigned to signed, to catch underflow
 * of ef_len caused by corrupt/malicious data.  (32-bit is adequate.
 * Used "long" to accommodate any systems with 16-bit "int".)
 */

int ef_scan_for_aes( ef_buf, ef_len, vers, vend, mode, mthd)
    ZCONST uch *ef_buf;         /* Buffer containing extra field */
    long ef_len;                /* Total length of extra field */
    ush *vers;                  /* Return storage: AES encryption version. */
    ush *vend;                  /* Return storage: AES encryption vendor. */
    char *mode;                 /* Return storage: AES encryption mode. */
    ush *mthd;                  /* Return storage: Real compression method. */
{
    int ret = 0;
    unsigned eb_id;
    long eb_len;

/*---------------------------------------------------------------------------
    This function scans the extra field for an EF_AES_WG block
    containing the AES encryption mode (which determines key length and
    salt length) and the actual compression method used.
    The return value is 0 if EF_AES_WG was not found, 1 if a good
    EF_AES_WG was found, and negative if an error occurred.
  ---------------------------------------------------------------------------*/

    /* Exit early, if there are no extra field data. */
    if ((ef_len == 0) || (ef_buf == NULL))
        return 0;

    Trace(( stderr,
     "\nef_scan_for_aes: scanning extra field of length %ld\n",
     ef_len));

    /* Scan the extra field blocks. */
    while (ef_len >= EB_HEADSIZE)
    {
        eb_id = makeword( ef_buf+ EB_ID);       /* Extra block ID. */
        eb_len = makeword( ef_buf+ EB_LEN);     /* Extra block length. */
        if (eb_len > (ef_len - EB_HEADSIZE))
        {
            /* Discovered some extra field inconsistency! */
            Trace(( stderr,
             "ef_scan_for_aes: block length %ld > rest ef_size %ld\n",
             eb_len, (ef_len- EB_HEADSIZE)));
            ret = -1;
            break;
        }

        if (eb_id == EF_AES_WG)
        {
            /* Found an EF_AES_WG block.  Check for a valid length. */
            if (eb_len < EB_AES_HLEN)
            {
                /* Not enough data.  Return an error code. */
                ret = -2;
                break;
            }

            /* Store extra block data in the user-supplied places. */
            if (vers != NULL)
            {
                *vers = makeword( ef_buf+ EB_HEADSIZE+ EB_AES_VERS);
            }
            if (vend != NULL)
            {
                *vend = makeword( ef_buf+ EB_HEADSIZE+ EB_AES_VEND);
            }
            if (mode != NULL)
            {
                *mode = *(ef_buf+ EB_HEADSIZE+ EB_AES_MODE);
            }
            if (mthd != NULL)
            {
                *mthd = makeword( ef_buf+ EB_HEADSIZE+ EB_AES_MTHD);
            }
            ret = 1;
            break;
        }

        /* Advance to the next extra field block. */
        ef_buf += (eb_len+ EB_HEADSIZE);
        ef_len -= (eb_len+ EB_HEADSIZE);
    }

    return ret;
}

#  endif /* def IZ_CRYPT_AES_WG */

# endif /* defined( UNZIP) || defined( UTIL) */

# ifdef ZIP
#  ifdef UTIL

#   ifdef IZ_CRYPT_AES_WG

/***************************/
/* Function ef_strip_aes() */
/***************************/

/* 2012-11-25 SMS.  (OUSPG report.)
 * Changed eb_len, ef_len, and ef_len_d from unsigned to signed, to
 * catch underflow of ef_len caused by corrupt/malicious data.  (32-bit
 * is adequate.  Used "long" to accommodate any systems with 16-bit
 * "int".)  Made function static.
 */

local int ef_strip_aes( ef_buf, ef_len)
    ZCONST uch *ef_buf;         /* Buffer containing extra field */
    long ef_len;                /* Total length of extra field */
{
    int ret = -1;               /* Return value. */
    unsigned eb_id;             /* Extra block ID. */
    long eb_len;                /* Extra block length. */
    uch *eb_aes;                /* Start of AES block. */
    uch *ef_buf_d;              /* Sliding extra field pointer. */
    long ef_len_d;              /* Remaining extra field length. */

/*---------------------------------------------------------------------------
    This function strips an EF_AES_WG block from an extra field.
    The return value is -1 if EF_AES_WG was not found, and the new
    (smaller) extra field length, if an EF_AES_WG was found.
  ---------------------------------------------------------------------------*/

    /* Exit early, if there are no extra field data. */
    if ((ef_len == 0) || (ef_buf == NULL))
        return -1;

    Trace(( stderr,
     "\nef_strip_aes: scanning extra field of length %ld\n",
     ef_len));

    eb_aes = NULL;              /* Start of AES block. */
    ef_buf_d = (uch *)ef_buf;   /* Sliding extra field pointer. */
    ef_len_d = ef_len;          /* Remaining extra field length. */

    /* Scan the extra field blocks. */
    while (ef_len_d >= EB_HEADSIZE)
    {
        eb_id = makeword( ef_buf_d+ EB_ID);     /* Extra block ID. */
        eb_len = makeword( ef_buf_d+ EB_LEN);   /* Extra block length. */

        if (eb_len > (ef_len_d - EB_HEADSIZE))
        {
            /* Discovered some extra field inconsistency! */
            Trace(( stderr,
             "ef_strip_aes: block length %ld > rest ef_size %ld\n",
             eb_len, (ef_len_d- EB_HEADSIZE)));
            ret = -1;
            break;
        }

        if (eb_id == EF_AES_WG)
        {
            /* Found an EF_AES_WG block.  Save its location, and escape. */
            eb_aes = ef_buf_d;
            break;
        }

        /* Advance to the next extra field block. */
        ef_buf_d += (eb_len+ EB_HEADSIZE);
        ef_len_d -= (eb_len+ EB_HEADSIZE);
    }

    if (eb_aes != NULL)
    {
        /* Move post-AES block data up onto AES block data (if not at end).
         * Prepare to return the original EF size less the AES block size.
         * Note: memmove() is supposed to be overlap-safe.
         */
        eb_len += EB_HEADSIZE;  /* Total block size (header+data). */
        ret = ef_len- eb_len;   /* New (reduced) extra field size. */
        ef_len_d = ef_buf+ ef_len- eb_aes- eb_len;      /* Move size. */

        if (ef_len_d > 0)
        {
            /* Move the data which need to be moved. */
            memmove( eb_aes, (eb_aes+ eb_len), ef_len_d);
        }
    }

    return ret;
}

#   endif /* def IZ_CRYPT_AES_WG */


/***********************************************************************
 * Encrypt the zip entry described by z from file in_file to file y
 * using the password passwd.  Return an error code in the ZE_ class.
 *
 * bfwrite() should take care of any byte counting.
 */
int zipcloak(z, passwd)
    struct zlist far *z;        /* zip entry to encrypt */
    ZCONST char *passwd;        /* password string */
{
    int res;                    /* result code */
    struct zlist far *localz;   /* local header */
    uch buf[1024];              /* write buffer */
    zoff_t size;                /* size of input data */
#   ifdef IZ_CRYPT_AES_WG
#    define HEAD_LEN head_len   /* Variable header length. */
    int head_len;               /* Variable encryption header length. */
    int salt_len = 0;           /* AES salt length.  (Init'd to hush cmplr.) */
#   else /* def IZ_CRYPT_AES_WG */
#    define HEAD_LEN RAND_HEAD_LEN      /* Constant trad. header length. */
#   endif /* def IZ_CRYPT_AES_WG [else] */

    /* Set encrypted bit, clear extended local header bit and write local
       header to output file */

    /* assume this archive is one disk and the file is open */

    /* read the local header */
    res = readlocal(&localz, z);

    /* readlocal() has already set localz->thresh_mthd = localz->how. */

    /* Update (assumed only one) disk.  Caller is responsible for offset. */
    z->dsk = 0;

    /* Set the encryption flags, and unset any extended local header flags. */
    z->flg |= 1,  z->flg &= ~8;
    localz->lflg |= 1, localz->lflg &= ~8;

    /* Set the new encryption method. */
    localz->encrypt_method = encryption_method;
    z->encrypt_method = encryption_method;

#   ifdef IZ_CRYPT_AES_WG
    if (encryption_method == TRADITIONAL_ENCRYPTION)
    {
        HEAD_LEN = RAND_HEAD_LEN;
    }
    else
    {
        /* Determine AES encryption salt length, and header+trailer
         * length.  Header has salty stuff.  Trailer has Message
         * Authentication Code (MAC).
         */
        /*                Note: v-- No parentheses in SALT_LENGTH def'n.   --v */
        salt_len = SALT_LENGTH( (encryption_method - (AES_MIN_ENCRYPTION - 1)) );
        HEAD_LEN = salt_len + PWD_VER_LENGTH +                       /* Header. */
         MAC_LENGTH( encryption_method - (AES_MIN_ENCRYPTION - 1) ); /* Trailer. */

        /* get the salt */
        prng_rand( zsalt, salt_len, &aes_rnp);

        /* initialize encryption context for this file */
        res = fcrypt_init(
         (encryption_method- (AES_MIN_ENCRYPTION- 1)),  /* AES mode. */
         (unsigned char*) passwd,                       /* Password. */
         (unsigned int)strlen(passwd),                  /* Password length. */
         zsalt,                                         /* Salt. */
         zpwd_verifier,                                 /* Password vfy buf. */
         &zctx);                                        /* AES context. */

        if (res == PASSWORD_TOO_LONG) {
            ZIPERR(ZE_CRYPT, "Password too long");
        } else if (res == BAD_MODE) {
            ZIPERR(ZE_CRYPT, "Bad mode");
        }
    }
#   endif /* def IZ_CRYPT_AES_WG */

    /* Add size of encryption header (AES_WG: header+trailer). */
    localz->siz += HEAD_LEN;
    z->siz = localz->siz;

    /* Put out the local header. */
    if ((res = putlocal(localz, PUTLOCAL_WRITE)) != ZE_OK) return res;

    /* Write out an encryption header before the file data. */
    if (z->encrypt_method != NO_ENCRYPTION)
    {
#   ifdef IZ_CRYPT_AES_WG
        if ((z->encrypt_method >= AES_MIN_ENCRYPTION) &&
         (z->encrypt_method <= AES_MAX_ENCRYPTION))
        {
            aes_crypthead( zsalt, salt_len, zpwd_verifier );
            tempzn += HEAD_LEN;         /* Count header+trailer. */
        }
        else
        {
#   endif /* def IZ_CRYPT_AES_WG */
#   ifdef IZ_CRYPT_TRAD
            /* Initialize keys with password and write random header */
            crypthead( passwd, localz->crc);
            tempzn += HEAD_LEN;         /* Count header. */
#    endif /* IZ_CRYPT_TRAD */
#   ifdef IZ_CRYPT_AES_WG
        }
#   endif /* def IZ_CRYPT_AES_WG */
    }

    /* Read, encrypt, and write out member data. */
    for (size = z->siz- HEAD_LEN; size > 0; )
    {
        size_t bytes_to_read;
        size_t bytes_read;

        bytes_to_read = (size_t)IZ_MIN( sizeof( buf), size);
        bytes_read = fread( buf, 1, bytes_to_read, in_file);

        if (bytes_to_read == bytes_read)
        {
            zfwrite( buf, 1, bytes_read);
        }
        else
        {
            return ferror(in_file) ? ZE_READ : ZE_EOF;
        }
        size -= bytes_to_read;
    }

#   ifdef IZ_CRYPT_AES_WG
    /* For AES_WG, write out an AES_WG MAC trailer. */
    if ((z->encrypt_method >= AES_MIN_ENCRYPTION) &&
     (z->encrypt_method <= AES_MAX_ENCRYPTION))
    {
      int ret;

      ret = fcrypt_end( auth_code,      /* Returned msg auth code (MAC). */
                        &zctx);         /* AES context. */

      bfwrite(auth_code, 1, ret, BFWRITE_DATA);
      /* tempzn already got this trailer size when it got the header size. */
    }
#   endif /* def IZ_CRYPT_AES_WG */

    /* Since we seek to the start of each local header can skip
       reading any extended local header */
#if 0
    if ((flag & 8) != 0 && zfseeko(in_file, 16L, SEEK_CUR)) {
        return ferror(in_file) ? ZE_READ : ZE_EOF;
    }
    if (fflush(y) == EOF) return ZE_TEMP;
#endif /* 0 */

    /* Update number of bytes written to output file */
    tempzn += (4 + LOCHEAD) + localz->nam + localz->ext + localz->siz;

    /* Free local header */
    if (localz->ext) izc_free(localz->extra);
    if (localz->nam) izc_free(localz->iname);
    if (localz->nam) izc_free(localz->name);
#   ifdef UNICODE_SUPPORT
    if (localz->uname) izc_free(localz->uname);
#   endif
    izc_free(localz);

    return ZE_OK;
}


/***********************************************************************
 * Decrypt the zip entry described by z from file in_file to file y
 * using the password passwd.  Return an error code in the ZE_ class.
 */
int zipbare(z, passwd)
    struct zlist far *z;  /* zip entry to encrypt */
    ZCONST char *passwd;  /* password string */
{
#   ifdef ZIP10
    int c0                /* byte preceding the last input byte */
#   endif
#   ifdef IZ_CRYPT_TRAD
    int c1;               /* last input byte */
    int b;                /* bytes in buffer */
#   endif
    /* all file offset and size now zoff_t - 8/28/04 EG */
    zoff_t size;          /* size of input data */
    struct zlist far *localz; /* local header */
    uch buf[1024];        /* write buffer */
    zoff_t z_siz;
    int passwd_ok;
    int r;                /* size of encryption header */
    int res;              /* return code */

#   ifdef IZ_CRYPT_AES_WG
#    define HEAD_LEN head_len   /* Variable header length. */
    int head_len;               /* Variable encryption header length. */
    uch h[ ENCR_HEAD_LEN];      /* Encryption header. */
    uch hh[ ENCR_PW_CHK_LEN];   /* Password check buffer. */
    char aes_mode = 0;  /* AES encryption mode.  (Init'd to hush cmplr.) */
    ush aes_mthd = 0;   /* Actual compress method.  (Init'd to hush cmplr.) */
    ush how_orig;               /* Original encryption method. */
    size_t n;                   /* Bytes actually read. */
    size_t nn;                  /* Bytes requested. */
    uzoff_t nout;               /* Total bytes put out. */
#   else /* def IZ_CRYPT_AES_WG */
#    define HEAD_LEN RAND_HEAD_LEN      /* Constant trad. header length. */
#   endif /* def IZ_CRYPT_AES_WG [else] */

    ush vers = 0;               /* AES encryption version (1 or 2). */
    ush vend = 0;               /* AES encryption vendor (should be "AE" (LE: x4541)). */
    zoff_t pos_local = 0;       /* Local header position. */
    zoff_t pos = 0;             /* End data position. */
#   if defined(IZ_CRYPT_AES_WG) || defined(IZ_CRYPT_AES_WG_NEW)
    ulg crc;                    /* To calculate CRC. */
#   endif

    /* Read local header. */
    res = readlocal(&localz, z);

    /* Some work is needed to let crypt support Zip splits.  This is
       preventing split support in ZipCloak.  As ZCRYPT is now fixed
       at 3.0 (see notes in crypt.h), it may be best to let the Zip
       and UnZip versions of crypt.c part ways and make the changes
       needed in the Zip version of crypt.c.  (Changes to the Zip
       version of crypt.c are already documented in the Zip change log,
       rather than some ZCRYPT log.)  Both zipbare() and zipcloak()
       need modifications. */

    /* Update (assumed only one) disk.  Caller is responsible for offset. */
    z->dsk = 0;

    passwd_ok = 1;              /* Assume a good password. */

#   ifdef IZ_CRYPT_AES_WG
    how_orig = localz->how;     /* Save the original encryption method. */
    if (how_orig == AESENCRED)
    {
        /* Extract the AES encryption details from the local extra field. */
        ef_scan_for_aes((uch *)localz->extra, localz->ext,
                        &vers, &vend, &aes_mode, &aes_mthd);

        if ((aes_mode > 0) && (aes_mode <= 3))
        {
            /* AES header size depends on (variable) salt length. */
            HEAD_LEN = SALT_LENGTH( aes_mode)+ PWD_VER_LENGTH;
        }
        else
        {
            /* Unexpected/invalid AES mode value. */
            zipwarn("invalid AES mode", "");
            return ZE_CRYPT;
        }

        /* Read the AES header from the input file. */
        size = fread( h, 1, HEAD_LEN, in_file);
        if (size < HEAD_LEN)
        {
            return ferror(in_file) ? ZE_READ : ZE_EOF;
        }

        /* Initialize the AES decryption machine for the password check. */
        fcrypt_init( aes_mode,                          /* AES mode. */
                     (unsigned char*) passwd,           /* Password. */
                     (unsigned int)strlen(passwd),      /* Password length. */
                     h,                                 /* Salt. */
                     hh,                                /* PASSWORD_VERIFIER. */
                     &zctx);                            /* AES context. */

        /* Check the password verifier. */
        if (memcmp( (h+ HEAD_LEN- PWD_VER_LENGTH), hh, PWD_VER_LENGTH))
        {
            passwd_ok = 0;      /* Bad AES password. */
        }
    }
    else /* if (how_orig == AESENCRED) */
    {
        /* Traditional Zip decryption. */
        HEAD_LEN = RAND_HEAD_LEN;
#   else /* def IZ_CRYPT_AES_WG */
    {
#   endif /* def IZ_CRYPT_AES_WG [else] */
#   ifdef IZ_CRYPT_TRAD
        /* Traditional Zip decryption. */
        /* Update disk.  Caller is responsible for offset (z->off). */
        z->dsk = 0;

        /* Initialize keys with password */
        init_keys(passwd);

        /* Decrypt encryption header, save last two bytes */
        c1 = 0;
        for (r = RAND_HEAD_LEN; r; r--) {
#   ifdef ZIP10
            c0 = c1;
#   endif
            if ((c1 = getc(in_file)) == EOF) {
                return ferror(in_file) ? ZE_READ : ZE_EOF;
            }
            Trace((stdout, " (%02x)", c1));
            zdecode(c1);
            Trace((stdout, " %02x", c1));
        }
        Trace((stdout, "\n"));

    /* If last two bytes of header don't match crc (or file time in the
     * case of an extended local header), back up and just copy. For
     * pkzip 2.0, the check has been reduced to one byte only.
     */
#   ifdef ZIP10
        if ((ush)(c0 | (c1<<8)) !=
            (z->flg & 8 ? (ush) z->tim & 0xffff : (ush)(z->crc >> 16)))
#   else
        if ((ush)c1 != (z->flg & 8 ? (ush) z->tim >> 8 : (ush)(z->crc >> 24)))
#   endif
        {
            passwd_ok = 0;          /* Bad traditional password. */
        }
#   endif /* def IZ_CRYPT_TRAD */
    }

    if (!passwd_ok)
    {
        /* Bad password.  Copy the entry as is. */
        if ((res = zipcopy(z)) != ZE_OK) {
            ziperr(res, "was copying an entry");
        }
        return ZE_MISS;
    }

    /* Good password.  Proceed to decrypt the entry. */
    z->siz -= HEAD_LEN;         /* Subtract the encryption header length. */
    localz->siz = z->siz;       /* Local, too. */
    z_siz = z->siz;             /* Save z->siz as Use z_siz for I/O later. */

    localz->flg = z->flg &= ~9;         /* Clear the encryption and */
    z->lflg = localz->lflg &= ~9;       /* data-descriptor flags. */

    /* Use correct CRC from central.  This is zero if AE-2, in which
     * case we need to recalculate the CRC.
     */
    localz->crc = z->crc;

    /* readlocal() has already set localz->thresh_mthd = localz->how.
     * If AES, then code below revises these.
     */

#   ifdef IZ_CRYPT_AES_WG
    if (how_orig == AESENCRED)
    {
        localz->how = aes_mthd; /* Set the compression method value(s) */
        z->how = aes_mthd;      /* to the value from the AES extra block. */
        z->thresh_mthd = aes_mthd;

        /* Subtract the MAC size from the compressed size(s). */
        localz->siz -= MAC_LENGTH( aes_mode);
        z->siz = localz->siz;

        /* Strip the AES extra block out of the local extra field. */
        if (localz->extra != NULL)
        {
            r = ef_strip_aes( (uch *)localz->extra, localz->ext);
            if (r >= 0)
            {
                localz->ext = r;
                if (r == 0)
                {
                    /* Whole extra field is now gone.  free() below will
                     * see localz->ext == 0, and skip it, so do it here.
                     */
                    izc_free( localz->extra);
                }
            }
        }
        /* Strip the AES extra block out of the central extra field. */
        r = ef_strip_aes( (uch *)z->cextra, z->cext);
        if (r >= 0)
        {
            z->cext = r;
        }
    }
#   endif /* def IZ_CRYPT_AES_WG */

    pos_local = zftello(y);

    /* Put out the (modified) local extra field. */
    if ((res = putlocal(localz, PUTLOCAL_WRITE)) != ZE_OK)
        return res;

#   ifdef IZ_CRYPT_AES_WG
    if (how_orig == AESENCRED)
    {
        /* Read, AES-decrypt, and write the member data, until
         * exhausted (no more input, or no more desired output).
         * z_siz = original (encrypted) compressed size (bytes to read).
         * z->siz = new (decrypted) compressed size (bytes to write).
         */
        nout = 0;
        if (vers == AES_WG_VEND_VERS_AE2) {
            /* CRC is set to zero for AE-2, so we need to calculate it
             * to update the decrypted entry.
             */
            crc = CRCVAL_INITIAL;
        }
        for (size = z_siz; (size > 0) && (nout < z->siz);)
        {
            nn = IZ_MIN(sizeof(buf), (size_t)size);
            n = fread(buf, 1, nn, in_file);
            if (n == nn)
            {
                fcrypt_decrypt(buf, n, &zctx);
                n = IZ_MIN(n, (size_t)(z->siz - nout));
                bfwrite(buf, 1, n, BFWRITE_DATA);
                if (vers == 2) {
                    crc = crc32(crc, (uch *)buf, n);
                }
                nout += n;      /* Bytes written. */
            }
            else
            {
                return ferror(in_file) ? ZE_READ : ZE_EOF;
            }
            size -= nn;         /* Bytes left to read. */
        }
        if (vers == AES_WG_VEND_VERS_AE2) {
            /* We need to update the local header only if AE-2 to
             * replace the missing CRC.
             */
            localz->crc = z->crc = crc;
            /* Update local extra field. */
            pos = zftello(y);
            if (zfseeko(y, pos_local, SEEK_SET)) {
                zipwarn("output seek back failed", "");
                return ZE_WRITE;
            }
            if ((res = putlocal(localz, PUTLOCAL_REWRITE)) != ZE_OK)
                return res;
            if (zfseeko(y, pos, SEEK_SET)) {
                zipwarn("output seek forward failed", "");
                return ZE_WRITE;
            }
        }
    }
    else /* if (how_orig == AESENCRED) */
    {
#   endif /* def IZ_CRYPT_AES_WG */
#   ifdef IZ_CRYPT_TRAD
        /* Read, traditional-decrypt, and write the member data, until done. */
        b = 0;
        for (size = z->siz; size; size--) {
            if ((c1 = getc(in_file)) == EOF) {
                return ferror(in_file) ? ZE_READ : ZE_EOF;
            }
            zdecode(c1);
            buf[b] = c1;
            b++;
            if (b >= 1024) {
              /* write the buffer */
              bfwrite(buf, 1, b, BFWRITE_DATA);
              b = 0;
            }
        }
        if (b) {
          /* write the buffer */
          bfwrite(buf, 1, b, BFWRITE_DATA);
          b = 0;
        }
#   endif /* def IZ_CRYPT_TRAD */
#   ifdef IZ_CRYPT_AES_WG
    }
#   endif /* def IZ_CRYPT_AES_WG */

    /* Since we seek to the start of each local header can skip
         reading any extended local header */

    /* Update number of bytes written to output file */
    tempzn += (4 + LOCHEAD) + localz->nam + localz->ext + localz->siz;

    /* Free local header */
    if (localz->ext) izc_free(localz->extra);
    if (localz->nam) izc_free(localz->iname);
    if (localz->nam) izc_free(localz->name);
#   ifdef UNICODE_SUPPORT
    if (localz->uname) izc_free(localz->uname);
#   endif
    izc_free(localz);

    return ZE_OK;
}

#  endif /* def UTIL */
# endif /* def ZIP */


# if (defined(UNZIP) && !defined(FUNZIP))

/***********************************************************************
 * Get the password and set up keys for current zipfile member.
 * Return PK_ class error.
 */
int decrypt(__G__ passwrd)
    __GDEF
    ZCONST char *passwrd;
{
    ush b;
    int n;
    int r;
    uch h[ ENCR_HEAD_LEN];
#  ifdef IZ_CRYPT_AES_WG
#    define HEAD_LEN head_len   /* Variable header length. */
    int head_len;               /* Variable encryption header length. */
#  else /* def IZ_CRYPT_AES_WG */
#    define HEAD_LEN RAND_HEAD_LEN      /* Constant trad. header length. */
#  endif /* def IZ_CRYPT_AES_WG [else] */

    Trace((stdout, "\n[incnt = %d]: ", GLOBAL(incnt)));

#  ifdef IZ_CRYPT_AES_WG
    if (GLOBAL( lrec.compression_method) == AESENCRED)
    {
        if ((GLOBAL( pInfo->cmpr_mode_aes) > 0) &&
         (GLOBAL( pInfo->cmpr_mode_aes) <= 3))
        {
            /* AES header size depends on (variable) salt length. */
            HEAD_LEN = SALT_LENGTH( GLOBAL( pInfo->cmpr_mode_aes))+
             PWD_VER_LENGTH;
        }
        else
        {
            /* Unexpected/invalid AES mode value. */
            return PK_ERR;
        }
    }
    else
    {
        HEAD_LEN = RAND_HEAD_LEN;
    }
#  endif /* def IZ_CRYPT_AES_WG */

    /* get header once (turn off "encrypted" flag temporarily so we don't
     * try to decrypt the same data twice) */
    GLOBAL(pInfo->encrypted) = FALSE;
    defer_leftover_input(__G);
    for (n = 0; n < HEAD_LEN; n++)
    {
        /* 2012-11-23 SMS.  (OUSPG report.)
         * Quit early if compressed size < HEAD_LEN.  The resulting
         * error message ("unable to get password") could be improved,
         * but it's better than trying to read nonexistent data, and
         * then continuing with a negative G.csize.  (See
         * fileio.c:readbyte()).
         */
        if ((b = NEXTBYTE) == (ush)EOF)
        {
            return PK_ERR;
        }
        h[n] = (uch)b;
        Trace((stdout, " (%02x)", h[n]));
    }
    undefer_input(__G);
    GLOBAL(pInfo->encrypted) = TRUE;

    if (GLOBAL(newzip)) { /* this is first encrypted member in this zipfile */
        GLOBAL(newzip) = FALSE;
        if (passwrd != (char *)NULL) { /* user gave password on command line */
            if (!GLOBAL(key)) {
                if ((GLOBAL(key) = (char *)izc_malloc(strlen(passwrd)+1)) ==
                    (char *)NULL)
                    return PK_MEM2;
                strcpy(GLOBAL(key), passwrd);
                GLOBAL(nopwd) = TRUE;  /* inhibit password prompting! */
            }
        } else if (GLOBAL(key)) { /* get rid of previous zipfile's key */
            izc_free(GLOBAL(key));
            GLOBAL(key) = (char *)NULL;
        }
    }

    /* if have key already, test it; else allocate memory for it */
    if (GLOBAL(key)) {
        if (!testp(__G__ HEAD_LEN, h))
            return PK_COOL;   /* existing password OK (else prompt for new) */
        else if (GLOBAL(nopwd))
            return PK_WARN;   /* user indicated no more prompting */
    } else if ((GLOBAL(key) = (char *)izc_malloc(IZ_PWLEN+1)) == (char *)NULL)
        return PK_MEM2;

    /* try a few keys */
    n = 0;
    do {
        r = (*G.decr_passwd)((zvoid *)&G, &n, GLOBAL(key), IZ_PWLEN,
                             GLOBAL(zipfn), GLOBAL(filename));
        if (r == IZ_PW_ERROR) {         /* internal error in fetch of PW */
            free (GLOBAL(key));
            GLOBAL(key) = NULL;
            return PK_MEM2;
        }
        if (r != IZ_PW_ENTERED) {       /* user replied "skip" or "skip all" */
            *GLOBAL(key) = '\0';        /*   We try the NIL password, ... */
            n = 0;                      /*   and cancel fetch for this item. */
        }
        if (!testp(__G__ HEAD_LEN, h))
            return PK_COOL;
        if (r == IZ_PW_CANCELALL)       /* User replied "Skip all" */
            GLOBAL(nopwd) = TRUE;       /*   inhibit any further PW prompt! */
    } while (n > 0);

    return PK_WARN;

} /* end function decrypt() */



/***********************************************************************
 * Test the password.  Return -1 if bad, 0 if OK.
 */
local int testp(__G__ hd_len, h)
    __GDEF
    int hd_len;
    ZCONST uch *h;
{
    int r;
    char *key_translated;

    /* On systems with "obscure" native character coding (e.g., EBCDIC),
     * the first test translates the password to the "main standard"
     * character coding. */

#  ifdef STR_TO_CP1
    /* allocate buffer for translated password */
    if ((key_translated = izc_malloc(strlen(GLOBAL(key)) + 1)) == (char *)NULL)
        return -1;
    /* first try, test password translated "standard" charset */
    r = testkey(__G__ hd_len, h, STR_TO_CP1(key_translated, GLOBAL(key)));
#  else /* !STR_TO_CP1 */
    /* first try, test password as supplied on the extractor's host */
    r = testkey(__G__ hd_len, h, GLOBAL(key));
#  endif /* ?STR_TO_CP1 */

#  ifdef STR_TO_CP2
    if (r != 0) {
#   ifndef STR_TO_CP1
        /* now prepare for second (and maybe third) test with translated pwd */
        if ((key_translated = izc_malloc(strlen(GLOBAL(key)) + 1)) ==
         (char *)NULL)
            return -1;
#   endif
        /* second try, password translated to alternate ("standard") charset */
        r = testkey(__G__ hd_len, h, STR_TO_CP2(key_translated, GLOBAL(key)));
#   ifdef STR_TO_CP3
        if (r != 0)
            /* third try, password translated to another "standard" charset */
            r = testkey(__G__ hd_len, h, STR_TO_CP3(key_translated, GLOBAL(key)));
#   endif
#   ifndef STR_TO_CP1
        izc_free(key_translated);
#   endif
    }
#  endif /* STR_TO_CP2 */

#  ifdef STR_TO_CP1
    izc_free(key_translated);
    if (r != 0) {
        /* last resort, test password as supplied on the extractor's host */
        r = testkey(__G__ hd_len, h, GLOBAL(key));
    }
#  endif /* STR_TO_CP1 */

    return r;

} /* end function testp() */


local int testkey(__G__ hd_len, h, key)
    __GDEF
    int hd_len;         /* Encryption header length. */
    ZCONST uch *h;      /* Decrypted header. */
    ZCONST char *key;   /* Decryption password to test. */
{
    ush b;
#  ifdef ZIP10
    ush c;
#  endif
    int n;
    uch *p;
    uch hh[ ENCR_PW_CHK_LEN];   /* Password check buffer. */

#  ifdef IZ_CRYPT_AES_WG
    if (GLOBAL( lrec.compression_method) == AESENCRED)
    {
        fcrypt_init( GLOBAL( pInfo->cmpr_mode_aes),     /* AES mode. */
                     (unsigned char*) key,      /* Password. */
                     strlen( key),              /* Password length. */
                     h,                         /* Salt. */
                     hh,                        /* PASSWORD_VERIFIER. */
                     GLOBAL( zcx));             /* AES context. */

        /* Check password verifier. */
        if (memcmp( (h+ hd_len- PWD_VER_LENGTH), hh, PWD_VER_LENGTH))
        {
            return -1;  /* Bad AES password. */
        }
        /* Password verified, so probably (but not certainly) OK.
         * Save the count of bytes to decrypt, up to, but not including,
         * the Message Authorization Code (MAC) block.
         * Decrypt the current buffer contents (pre-MAC) before leaving.
         */
        GLOBAL( ucsize_aes) =
         GLOBAL( csize)- MAC_LENGTH( GLOBAL( pInfo->cmpr_mode_aes));
        n = (int)(IZ_MIN( GLOBAL( incnt), GLOBAL( ucsize_aes)));
        fcrypt_decrypt( GLOBAL( inptr), n, GLOBAL( zcx));
        GLOBAL( ucsize_aes) -= n;       /* Decrement bytes-to-decrypt. */
        return 0;       /* OK */
    }
    else
    {
#  endif /* def IZ_CRYPT_AES_WG */

#  ifdef IZ_CRYPT_TRAD
    /* Traditional encryption. */

    /* set keys and save the encrypted header */
    init_keys(__G__ key);
    memcpy(hh, h, hd_len);

    /* check password */
    for (n = 0; n < hd_len; n++) {
        zdecode(hh[n]);
        Trace((stdout, " %02x", hh[n]));
    }

    Trace((stdout,
      "\n  lrec.crc= %08lx  crec.crc= %08lx  pInfo->ExtLocHdr= %s\n",
      GLOBAL(lrec.crc32), GLOBAL(pInfo->crc),
      GLOBAL(pInfo->ExtLocHdr) ? "true":"false"));
    Trace((stdout, "  incnt = %d  unzip offset into zipfile = %lld\n",
      GLOBAL(incnt),
      GLOBAL(cur_zipfile_bufstart)+(GLOBAL(inptr)-GLOBAL(inbuf))));

    /* same test as in zipbare(): */

#  ifdef ZIP10 /* check two bytes */
    c = hh[RAND_HEAD_LEN-2], b = hh[RAND_HEAD_LEN-1];
    Trace((stdout,
      "  (c | (b<<8)) = %04x  (crc >> 16) = %04x  lrec.time = %04x\n",
      (ush)(c | (b<<8)), (ush)(GLOBAL(lrec.crc32) >> 16),
      ((ush)GLOBAL(lrec.last_mod_dos_datetime) & 0xffff))));
    if ((ush)(c | (b<<8)) != (GLOBAL(pInfo->ExtLocHdr) ?
                           ((ush)GLOBAL(lrec.last_mod_dos_datetime) & 0xffff) :
                           (ush)(GLOBAL(lrec.crc32) >> 16)))
        return -1;  /* bad */
#  else
    b = hh[RAND_HEAD_LEN-1];
    Trace((stdout, "  b = %02x  (crc >> 24) = %02x  (lrec.time >> 8) = %02x\n",
      b, (ush)(GLOBAL(lrec.crc32) >> 24),
      ((ush)GLOBAL(lrec.last_mod_dos_datetime) >> 8) & 0xff));
    if (b != (GLOBAL(pInfo->ExtLocHdr) ?
        ((ush)GLOBAL(lrec.last_mod_dos_datetime) >> 8) & 0xff :
        (ush)(GLOBAL(lrec.crc32) >> 24)))
        return -1;  /* bad */
#  endif
    /* password OK:  decrypt current buffer contents before leaving */
    for (n = (long)GLOBAL(incnt) > GLOBAL(csize) ?
             (int)GLOBAL(csize) : GLOBAL(incnt),
         p = GLOBAL(inptr); n--; p++)
        zdecode(*p);
    return 0;       /* OK */
#  endif /* def IZ_CRYPT_TRAD */

#  ifdef IZ_CRYPT_AES_WG
    } /* (GLOBAL( lrec.compression_method) == AESENCRED) [else] */
#  endif /* def IZ_CRYPT_AES_WG */

} /* end function testkey() */

# endif /* (defined(UNZIP) && !defined(FUNZIP)) */


#else /* def IZ_CRYPT_ANY */

/* something "externally visible" to shut up compiler/linker warnings */
int zcr_dummy;

#endif /* def IZ_CRYPT_ANY [else] */
