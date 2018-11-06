/*
  Copyright (c) 2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-02 or later
  (the contents of which are also included in unzip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/

/*
 * Wrapper for "bzip2/huffman.c".
 */

/* This sets BZIP2_SUPPORT if BZIP2 enabled. */
#include "../control.h"

/* If bzip2 enabled, include the file contents. */
#ifdef BZIP2_SUPPORT
# include "../bzip2/huffman.c"
#endif
