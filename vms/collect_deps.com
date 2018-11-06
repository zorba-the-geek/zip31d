$! COLLECT_DEPS.COM
$!
$!    Info-ZIP VMS procedure to collect MMS/MMK dependencies.
$!
$!    Last revised:  2014-10-08
$!
$!----------------------------------------------------------------------
$! Copyright (c) 2004-2014 Info-ZIP.  All rights reserved.
$!
$! See the accompanying file LICENSE, version 2009-Jan-2 or later (the
$! contents of which are also included in zip.h) for terms of use.  If,
$! for some reason, all these files are missing, the Info-ZIP license
$! may also be found at: ftp://ftp.info-zip.org/pub/infozip/license.html
$!----------------------------------------------------------------------
$!
$!    For the product named by P1,
$!    P2 = output file specification.
$!    Collect all source file dependencies specified by P3,
$!    and add P4 prefix to the object file.
$!    Convert absolute dependencies to relative from one level above P5.
$!    P6 = slash-delimited search vector for added .IFDEF conditions.
$!    P7 = slash-delimited macro vector for added .IFDEF conditions.
$!
$! MMS /EXTENDED_SYNTAX can't easily pass a macro invocation for P4, so
$! we remove any internal spaces which might have been added to prevent
$! immediate evaluation of a macro invocation.
$!
$ prefix = f$edit( p4, "COLLAPSE")
$!
$ dev_lose = f$edit( f$parse( p5, , , "DEVICE", "SYNTAX_ONLY"), "UPCASE")
$ dir_lose = f$edit( f$parse( p5, , , "DIRECTORY", "SYNTAX_ONLY"), "UPCASE")
$ suffix = ".VMS]"
$ suffix_loc = f$locate( suffix, dir_lose)
$ if (suffix_loc .lt f$length( dir_lose))
$ then
$    dev_dir_lose = dev_lose+ dir_lose- suffix
$ else
$    dev_dir_lose = dev_lose+ dir_lose- "]"
$ endif
$!
$! For portability, make the output file record format Stream_LF.
$!
$ create /fdl = sys$input 'p2'
RECORD
        Carriage_Control carriage_return
        Format stream_lf
$!
$ open /read /write /error = end_main deps_out 'p2'
$ on error then goto loop_main_end
$!
$! Include proper-inclusion-check preface.
$!
$ incl_macro = "INCL_"+ f$parse( p2, , , "NAME", "SYNTAX_ONLY")
$ write deps_out "#"
$ write deps_out "#    ''p1' -- MMS (or MMK) Source Dependency File."
$ write deps_out "#"
$ write deps_out ""
$ write deps_out -
   "# This description file is included by other description files.  It is"
$ write deps_out -
   "# not intended to be used alone.  Verify proper inclusion."
$ write deps_out ""
$ write deps_out ".IFDEF ''incl_macro'"
$ write deps_out ".ELSE"
$ write deps_out -
   "$$$$ THIS DESCRIPTION FILE IS NOT INTENDED TO BE USED THIS WAY."
$ write deps_out ".ENDIF"
$ write deps_out ""
$!
$! Actual dependencies from individual dependency files.
$!
$ loop_main_top:
$    file = f$search( p3)
$    if (file .eqs. "") then goto loop_main_end
$!
$    open /read /error = end_subs deps_in 'file'
$    cond = ""
$    loop_subs_top:
$       read /error = loop_subs_end deps_in line
$       line_reduced = f$edit( line, "COMPRESS, TRIM, UPCASE")
$       colon = f$locate( " : ", line_reduced)
$       d_d_l_loc = f$locate( dev_dir_lose, -
         f$extract( (colon+ 3), 1000, line_reduced))
$       if (d_d_l_loc .eq. 0)
$       then
$          front = f$extract( 0, (colon+ 3), line_reduced)
$          back = f$extract( (colon+ 3+ f$length( dev_dir_lose)), -
            1000, line_reduced)
$          line = front+ "["+ back
$!
$!         Add .IFDEF conditions according to P6+P7.
$!
$          elt = 0
$          cond_new = ""
$          loop_elt_top:
$             elt_str = f$element( elt, "/", p6)
$             if (elt_str .eqs. "/") then goto loop_elt_end
$             if (f$locate( elt_str, line) .lt. f$length( line))
$             then
$!               Found search target.
$                cond_new = f$element( elt, "/", p7)
$                goto loop_elt_end
$             endif
$             elt = elt+ 1
$          goto loop_elt_top
$          loop_elt_end:
$!
$          if (cond_new .nes. "")
$          then
$             if (cond_new .nes. cond)
$             then
$                if (cond .nes. "")
$                then
$                   write deps_out ".ENDIF # ''cond'"
$                   cond = ""
$                endif
$                write deps_out ".IFDEF ''cond_new'"
$                cond = cond_new
$             endif
$          else
$             if (cond .nes. "")
$             then
$                write deps_out ".ENDIF # ''cond'"
$                cond = ""
$             endif
$          endif
$!
$          write deps_out "''prefix'"+ "''line'"
$       endif
$    goto loop_subs_top
$    loop_subs_end:
$!
$    if (cond .nes. "")
$    then
$       write deps_out ".ENDIF # ''cond'"
$    endif
$!
$    close deps_in
$!
$ goto loop_main_top
$ loop_main_end:
$!
$ close deps_out
$!
$ end_main:
$!
