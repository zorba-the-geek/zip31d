/*
  decc_init.c

  Copyright (c) 1990-2013 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/

/* Common code for use in main programs (zip.c, zipcloak.c, zipnote.c,
 * zipsplit.c) to establish a LIB$INITIALIZE function.
 */

/* 2013-12-30 SMS.
 * Moved code to decc_init.c, for inclusion by all main programs, not
 * only Zip.
 * 2012-06-06 SMS.
 * Moved VMS LIB$INITIALIZE set-up here from vms/vms.c to let
 * ZIP_DLL_LIB determine whether to use it, without adding ZIP_DLL_LIB
 * dependencies elsewhere.
 *
 * A user-supplied LIB$INITIALIZE routine should be able to call our
 * decc_init(), if desired.  Define USE_ZIP_LIB_INITIALIZE at build time
 * (LOCAL_ZIP) to use ours even in the library.  (Or steal/adapt this
 * code for use in your application.)
 */
#if !defined( ZIP_DLL_LIB) || defined( USE_ZIP_LIB_INITIALIZE)
# ifdef __DECC
#  if !defined( __VAX) && (__CRTL_VER >= 70301000)

/* Get "decc_init()" into a valid, loaded LIB$INITIALIZE PSECT. */

#pragma nostandard

/* Establish the LIB$INITIALIZE PSECTs, with proper alignment and
   other attributes.  Note that "nopic" is significant only on VAX.
*/
#pragma extern_model save

#pragma extern_model strict_refdef "LIB$INITIALIZ" 2, nopic, nowrt
const int spare[ 8] = { 0 };

#pragma extern_model strict_refdef "LIB$INITIALIZE" 2, nopic, nowrt
void (*const x_decc_init)() = decc_init;

#pragma extern_model restore

/* Fake reference to ensure loading the LIB$INITIALIZE PSECT. */

#pragma extern_model save

int LIB$INITIALIZE( void);

#pragma extern_model strict_refdef
int dmy_lib$initialize = (int) LIB$INITIALIZE;

#pragma extern_model restore

#pragma standard

#  endif /* !defined( __VAX) && (__CRTL_VER >= 70301000) */
# endif /* def __DECC */
#endif /* !defined(ZIP_DLL_LIB) || defined( USE_ZIP_LIB_INITIALIZE) */
