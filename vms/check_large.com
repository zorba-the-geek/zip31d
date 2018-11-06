$! CHECK_LARGE.COM
$!
$!     Info-ZIP VMS procedure to verify large-file support.
$!
$!     Last revised:  2014-10-10
$!
$!----------------------------------------------------------------------
$! Copyright (c) 2014 Info-ZIP.  All rights reserved.
$!
$! See the accompanying file LICENSE, version 2009-Jan-2 or later (the
$! contents of which are also included in zip.h) for terms of use.  If,
$! for some reason, all these files are missing, the Info-ZIP license
$! may also be found at: ftp://ftp.info-zip.org/pub/infozip/license.html
$!----------------------------------------------------------------------
$!
$! Compile, link, and run a C test program to verify large-file support.
$!
$! Useful only on Alpha (IA64 always has support; VAX always lacks
$! support), so assume CC is DEC/Compaq/HP C.
$!
$! Work in the [.'P1'] destination directory.
$! Set the P2 logical name to a non-null value for success.
$! P3 and P4 may be used for qualifiers on the DEFINE command.
$!
$! For testing, define logical name IZ_CHECK_LARGE_STATUS to use that
$! (fake) test status result.
$!
$ dest = p1
$!
$ set noon
$!
$ file_temp_name = f$parse( -
   f$environment( "PROCEDURE"), , , "NAME", "SYNTAX_ONLY")
$ file_temp_name = "[.''p1']"+ file_temp_name+ "_"+ f$getjpi( 0, "PID")
$ file_temp_name_exe = file_temp_name+ ".exe"
$!
$! Create the test program file.
$!
$ create 'file_temp_name'.c
#define _LARGEFILE
#include <stdio.h>
#include <stdlib.h>
int main( int argc, char **argv)
{
    if (sizeof( off_t) >= 8)
        return EXIT_SUCCESS;
    else
        return EXIT_FAILURE;
}
$!
$! Compile the test program (quietly), and delete the source file.
$!
$ define /user_mode sys$error nl:
$ define /user_mode sys$output nl:
$ cc /nolist /object = [.'p1'] 'file_temp_name'.c
$ sts = $status
$ delete 'file_temp_name'.c;*
$ if (sts)
$ then
$!
$!    Link the test program (quietly), and delete the object file.
$!
$     define /user_mode sys$error nl:
$     define /user_mode sys$output nl:
$     link /nomap /executable = [.'p1'] 'file_temp_name'.obj
$     sts = $status
$     delete 'file_temp_name'.obj;*
$     if (sts)
$     then
$!
$!        Run the test program (quietly).
$!
$         define /user_mode sys$error nl:
$         define /user_mode sys$output nl:
$         run 'file_temp_name_exe'
$         sts = $status
$     endif
$ endif
$!
$! Delete the executable file, if it exists.
$!
$ if (f$search( file_temp_name_exe) .nes. "")
$ then
$     delete 'file_temp_name_exe';*
$ endif
$!
$ fake_test_status = f$trnlnm( "IZ_CHECK_LARGE_STATUS")
$ if (fake_test_status .nes. "")
$ then
$     sts = fake_test_status
$ endif
$!
$ if (sts)
$ then
$     if (p2 .eqs. "")
$     then
$         write sys$output "true"
$     else
$         define 'p3' 'p2' "true" 'p4'
$     endif
$ endif
$!
