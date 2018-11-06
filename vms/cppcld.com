$! CPPCLD.COM
$!
$!     Info-ZIP VMS procedure to use a C preprocessor on a .CLD file.
$!
$!     Last revised:  2013-11-29  SMS.
$!
$!----------------------------------------------------------------------
$! Copyright (c) 2012-2013 Info-ZIP.  All rights reserved.
$!
$! See the accompanying file LICENSE, version 2009-Jan-2 or later (the
$! contents of which are also included in zip.h) for terms of use.  If,
$! for some reason, all these files are missing, the Info-ZIP license
$! may also be found at: ftp://ftp.info-zip.org/pub/infozip/license.html
$!----------------------------------------------------------------------
$!
$! P1 = CC command
$! P2 = Command definition input file (".CLD") with CPP directives.
$! P3 = Command definition output file.
$! P4 = Comma-separated list of C macros.
$!
$!
$ cpp = f$edit( p1, "trim")
$ if (cpp .eqs. "")
$ then
$     cpp = "cc"
$ endif
$!
$! C preprocessing.
$!
$ 'cpp' /preprocess_only = 'p3' 'p2' /define = ('p4') /undefine = (VMS)
$!
$! Strip out the added file indentification line.
$!
$ line_nr = 0
$ open /read /error = error_r in_file 'p3'
$ create /fdl = sys$input 'p3'
RECORD
        Carriage_Control carriage_return
        Format stream_lf
$!
$ open /append /error = error_w out_file 'p3'
$ loop_top:
$     read /error = error_c in_file line
$     if ((line_nr .ne. 0) .or. (f$extract( 0, 1, line) .nes. "#"))
$     then
$         write /error = error_c out_file line
$     endif
$     line_nr = 1
$ goto loop_top
$!
$ error_c:
$ close out_file
$ error_w:
$ close in_file
$ error_r:
$!
