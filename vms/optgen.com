$! OPTGEN.COM
$!
$!     Info-ZIP VMS procedure to extract a program version from a C
$!     header file.
$!
$!     Last revised:  2013-11-29  SMS.
$!
$!----------------------------------------------------------------------
$! Copyright (c) 2011-2013 Info-ZIP.  All rights reserved.
$!
$! See the accompanying file LICENSE, version 2009-Jan-2 or later (the
$! contents of which are also included in zip.h) for terms of use.  If,
$! for some reason, all these files are missing, the Info-ZIP license
$! may also be found at: ftp://ftp.info-zip.org/pub/infozip/license.html
$!----------------------------------------------------------------------
$!
$! Find the UnZip or Zip (P1 = product name) program version.
$!
$! Set the P2 logical name to the version string.
$! P3 and P4 may be used for qualifiers on the DEFINE command.
$!
$ file_version = ""
$ version = ""
$ file_temp_name = f$parse( -
   f$environment( "PROCEDURE"), , , "NAME", "SYNTAX_ONLY")
$ file_temp_name = file_temp_name+ "_"+ f$getjpi( 0, "PID")+ ".dat"
$!
$ p10 = f$extract( 0, 1, f$edit( p1, "TRIM, UPCASE"))
$ if (p10 .eqs. "U")
$ then
$     file_version = "unzvers.h"
$     search_string = """#"", ""define"", ""UZ_VER_STRING"", """""""""
$ else
$     if (p10 .eqs. "Z")
$     then
$         file_version = "revision.h"
$         search_string = """#"", ""define"", ""VERSION"", """""""""
$     endif
$ endif
$!
$ if (file_version .nes. "")
$ then
$     search /match = and /output = 'file_temp_name' -
       'file_version' 'search_string'
$ endif
$!
$ line = ""
$ open /read /error = open_fail file_temp 'file_temp_name'
$ read file_temp line
$ close file_temp
$ delete 'file_temp_name';*
$!
$ open_fail:
$!
$ if (line .nes. "")
$ then
$!
$     version = f$element( 1, """", line)
$     version = f$element( 0, " ", version)
$!
$! Truncate version string at first alpha.  ("3.1d13" -> "3.1".)
$!
$     ch = 0
$     loop_truncate:
$         if (f$locate( f$edit( f$extract( ch, 1, version), "UPCASE"), -
           "ABCDEFGHIJKLMNOPQRSTUVWXYZ") .lt. 26) then goto loop_truncate_end
$     ch = ch+ 1
$     goto loop_truncate
$     loop_truncate_end:
$!
$     version = f$extract( 0, ch, version)
$     if (p2 .eqs. "")
$     then
$         write sys$output version
$     else
$         define 'p3' 'p2' 'version' 'p4'
$     endif
$ endif
$!
