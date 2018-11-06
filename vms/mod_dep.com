$! MOD_DEP.COM
$!
$!    Info-ZIP VMS procedure to modify MMS/MMK dependencies.
$!
$!     Last revised:  2013-11-29  SMS.
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
$!    Modify a dependencies file (P1), changing the object file name to
$!    P2.
$!    P3 = output file specification.
$!
$!
$ prefix = f$edit( p3, "COLLAPSE")
$!
$! Strip any device:[directory] from P2.
$!
$ obj_name = f$parse( P2, , , "NAME", "SYNTAX_ONLY")+ -
   f$parse( P2, , , "TYPE", "SYNTAX_ONLY")
$!
$ open /read /error = end_main deps_in 'p1'
$ open /write /error = end_main deps_out 'p3'
$ on error then goto loop_main_end
$ loop_main_top:
$     read /error = loop_main_end deps_in line
$     line_reduced = f$edit( line, "COMPRESS, TRIM")
$     colon = f$locate( " : ", line_reduced)
$     line = obj_name+ f$extract( colon, 2000, line)
$     write deps_out "''line'"
$ goto loop_main_top
$!
$ loop_main_end:
$ close deps_in
$ close deps_out
$!
$ end_main:
$!
