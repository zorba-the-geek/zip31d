$! HLP_LIB_NEXT.COM
$!
$!    Info-ZIP VMS procedure to find the next available HLP$LIBRARY[_*]
$!    logical name.
$!
$!    Last revised:  2013-11-29
$!
$!----------------------------------------------------------------------
$! Copyright (c) 2004-2013 Info-ZIP.  All rights reserved.
$!
$! See the accompanying file LICENSE, version 2009-Jan-2 or later (the
$! contents of which are also included in zip.h) for terms of use.  If,
$! for some reason, all these files are missing, the Info-ZIP license
$! may also be found at: ftp://ftp.info-zip.org/pub/infozip/license.html
$!----------------------------------------------------------------------
$!
$ base = "HLP$LIBRARY"
$ candidate = base
$ i = 0
$!
$ loop_top:
$    if (i .gt. 0) then candidate = base+ "_"+ f$string( i)
$    i = i+ 1
$    if (f$trnlnm( candidate) .nes. "") then goto loop_top
$!
$ write sys$output candidate
$!
