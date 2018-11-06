/*
  Copyright (c) 2013 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-02 or later
  (the contents of which are also included in unzip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/

/*
 * Wrapper for "szip/LzFind.c".
 */

/* This sets LZMA_SUPPORT if LZMA enabled. */
#include "../control.h"

#ifdef LZMA_SUPPORT
# include "../szip/LzFind.c"
#endif
