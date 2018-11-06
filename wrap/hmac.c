/*
  Copyright (c) 2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-02 or later
  (the contents of which are also included in unzip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/

/*
 * Wrapper for "aes_wg/hmac.c".
 */

/* This sets CRYPT_AES_WG if AES enabled. */
#include "../control.h"

/* If AES WG enabled, load the real file. */
#ifdef CRYPT_AES_WG
# include "../aes_wg/hmac.c"
#endif
