#                                               30 November 2010.  SMS.
#
#    BZIP2 1.0 for VMS - MMS (or MMK) Description File.
#
# Usage:
#
#    MMS /DESCRIP = [.vms]descrip.mms [/MACRO = (<see_below>)] [target]
#
# Note that this description file must be used from the main
# distribution directory, not from the [.vms] subdirectory.
#
# Optional macros:
#
#    CCOPTS=xxx     Compile with CC options xxx.  For example:
#                   "CCOPTS=/ARCH=HOST" or "CCOPTS=/NAMES=AS_IS"
#
#    DBG=1          Compile with /DEBUG /NOOPTIMIZE.
#                   Link with /DEBUG /TRACEBACK.
#                   (Default is /NOTRACEBACK.)
#
#    LARGE=1        Enable large-file (>2GB) support.  Non-VAX only.
#
#    LINKOPTS=xxx   Link with LINK options xxx.  For example:
#                   "LINKOPTS=/NOINFO"
#
#    LIST=1         Compile with /LIST /SHOW = (ALL, NOMESSAGES).
#                   Link with /MAP /CROSS_REFERENCE /FULL.
#
#    "LOCAL_BZIP=c_macro_1=value1 [, c_macro_2=value2 [...]]"
#                   Compile with these additional C macros defined.
#
# The default target, ALL, builds the selected product executables.
#
# Other targets:
#
#    CLEAN       deletes architecture-specific files, but leaves any
#                individual source dependency files and the help files.
#
#    CLEAN_ALL   deletes all generated files, except the main
#                (collected) source dependency file.
#
#    CLEAN_EXE   deletes only the architecture-specific executables.
#                Handy if all you wish to do is re-link the executables.
#
#    CLEAN_OLB   deletes only the architecture-specific object
#                libraries.
#
#    CLEAN_TEST  deletes (architecture-specific) test result files.
#
#    TEST        runs some simple tests.
#
#
#
#
# Example commands:
#
# To build the conventional small-file product (Note: DESCRIP.MMS is the
# default description file name.):
#
#    MMS /DESCRIP = [.vms]
#
# To get the large-file executables (on a non-VAX system):
#
#    MMS /DESCRIP = [.vms] /MACRO = (LARGE=1)
#
# To delete the architecture-specific generated files for this system
# type:
#
#    MMS /DESCRIP = [.vms] /MACRO = (LARGE=1) CLEAN     ! Large-file.
# or
#    MMS /DESCRIP = [.vms] CLEAN                        ! Small-file.
#
# To build a complete small-file product for debug with compiler
# listings and link maps:
#
#    MMS /DESCRIP = [.vms] CLEAN
#    MMS /DESCRIP = [.vms] /MACRO = (DBG=1, LIST=1)
#
########################################################################

# Include primary product description file.

INCL_DESCRIP_SRC = 1
.INCLUDE [.vms]descrip_src.mms

# Object library names.

LIB_BZ2 = [.$(DEST)]LIBBZ2.OLB

LIB_BZ2_NS = [.$(DEST)]LIBBZ2_NS.OLB


# TARGETS.

# Default target, ALL.  Build all executables and variant object library.

ALL : $(BZIP2_EXE) $(BZIPUTILS_EXE) $(LIB_BZ2_NS) $(DECC_VER_EXE)
	@ write sys$output "Done."

# CLEAN target.  Delete the [.$(DEST)] directory and everything in it.

CLEAN :
	if (f$search( "[.$(DEST)]*.*") .nes. "") then -
	 delete /noconfirm [.$(DEST)]*.*;*
	if (f$search( "$(DEST).DIR", 1) .nes. "") then -
	 set protection = w:d $(DEST).DIR;*
	if (f$search( "$(DEST).DIR", 2) .nes. "") then -
	 delete /noconfirm $(DEST).DIR;*

# CLEAN_ALL target.  Delete:
#    The [.$(DEST)] directories and everything in them.
#    All individual C dependency files.
# Also mention:
#    Comprehensive dependency file.

CLEAN_ALL :
	if (f$search( "[.ALPHA*]*.*") .nes. "") then -
	 delete /noconfirm [.ALPHA*]*.*;*
	if (f$search( "ALPHA*.DIR", 1) .nes. "") then -
	 set protection = w:d ALPHA*.DIR;*
	if (f$search( "ALPHA*.DIR", 2) .nes. "") then -
	 delete /noconfirm ALPHA*.DIR;*
	if (f$search( "[.IA64*]*.*") .nes. "") then -
	 delete /noconfirm [.IA64*]*.*;*
	if (f$search( "IA64*.DIR", 1) .nes. "") then -
	 set protection = w:d IA64*.DIR;*
	if (f$search( "IA64*.DIR", 2) .nes. "") then -
	 delete /noconfirm IA64*.DIR;*
	if (f$search( "[.VAX*]*.*") .nes. "") then -
	 delete /noconfirm [.VAX*]*.*;*
	if (f$search( "VAX*.DIR", 1) .nes. "") then -
	 set protection = w:d VAX*.DIR;*
	if (f$search( "VAX*.DIR", 2) .nes. "") then -
	 delete /noconfirm VAX*.DIR;*
	if (f$search( "*.MMSD") .nes. "") then -
	 delete /noconfirm *.MMSD;*
	if (f$search( "[.vms]*.MMSD") .nes. "") then -
	 delete /noconfirm [.vms]*.MMSD;*
	@ write sys$output ""
	@ write sys$output "Note:  This procedure will not"
	@ write sys$output "   DELETE [.vms]descrip_deps.mms;*"
	@ write sys$output -
 "You may choose to, but a recent version of MMS (V3.5 or newer?) is"
	@ write sys$output -
 "needed to regenerate it.  (It may also be recovered from the original"
	@ write sys$output -
 "distribution kit.)  See [.vms]descrip_mkdeps.mms for instructions on"
	@ write sys$output -
 "generating [.vms]descrip_deps.mms."
	@ write sys$output ""

# CLEAN_EXE target.  Delete the executables in [.$(DEST)].

CLEAN_EXE :
	if (f$search( "[.$(DEST)]*.EXE") .nes. "") then -
	 delete /noconfirm [.$(DEST)]*.EXE;*

# CLEAN_OLB target.  Delete the object libraries in [.$(DEST)].

CLEAN_OLB :
	if (f$search( "[.$(DEST)]*.OLB") .nes. "") then -
	 delete /noconfirm [.$(DEST)]*.OLB;*

# CLEAN_TEST target.  Delete the test result files in [.$(DEST)].

CLEAN_TEST :
	if (f$search( "[.$(DEST)]*.rb2") .nes. "") then -
	 delete /noconfirm [.$(DEST)]*.rb2;*
	if (f$search( "[.$(DEST)]*.tst") .nes. "") then -
	 delete /noconfirm [.$(DEST)]*.tst;*


# Object library module dependencies.

$(LIB_BZ2) : $(LIB_BZ2)($(MODS_OBJS_LIB_BZ2))
	@ write sys$output "$(MMS$TARGET) updated."

$(LIB_BZ2_NS) : $(LIB_BZ2_NS)($(MODS_OBJS_LIB_BZ2_NS))
	@ write sys$output "$(MMS$TARGET) updated."

# Default C compile rule.

.C.OBJ :
	MY_MODULE_NAME = f$parse( "$(MMS$SOURCE)", , , "NAME", "SYNTAX_ONLY")
	$(CC) $(CFLAGS) -
	 /define = ($(CDEFS), MY_MODULE_NAME='MY_MODULE_NAME') -
	 $(MY_MODULE_NAME_H)+ $(MMS$SOURCE)

# Variant object library sources.

[.$(DEST)]BLOCKSORT_.OBJ : []BLOCKSORT.C
	MY_MODULE_NAME = f$parse( "$(MMS$SOURCE)", , , "NAME", "SYNTAX_ONLY")
	$(CC) $(CFLAGS_NS) -
	 /define = ($(CDEFS), BZ_NO_STDIO=1, MY_MODULE_NAME='MY_MODULE_NAME') -
	 $(MY_MODULE_NAME_H)+ $(MMS$SOURCE)

[.$(DEST)]BZLIB_.OBJ : []BZLIB.C
	MY_MODULE_NAME = f$parse( "$(MMS$SOURCE)", , , "NAME", "SYNTAX_ONLY")
	$(CC) $(CFLAGS_NS) -
	 /define = ($(CDEFS), BZ_NO_STDIO=1, MY_MODULE_NAME='MY_MODULE_NAME') -
	 $(MY_MODULE_NAME_H)+ $(MMS$SOURCE)

[.$(DEST)]COMPRESS_.OBJ : []COMPRESS.C
	MY_MODULE_NAME = f$parse( "$(MMS$SOURCE)", , , "NAME", "SYNTAX_ONLY")
	$(CC) $(CFLAGS_NS) -
	 /define = ($(CDEFS), BZ_NO_STDIO=1, MY_MODULE_NAME='MY_MODULE_NAME') -
	 $(MY_MODULE_NAME_H)+ $(MMS$SOURCE)

[.$(DEST)]CRCTABLE_.OBJ : []CRCTABLE.C
	MY_MODULE_NAME = f$parse( "$(MMS$SOURCE)", , , "NAME", "SYNTAX_ONLY")
	$(CC) $(CFLAGS_NS) -
	 /define = ($(CDEFS), BZ_NO_STDIO=1, MY_MODULE_NAME='MY_MODULE_NAME') -
	 $(MY_MODULE_NAME_H)+ $(MMS$SOURCE)

[.$(DEST)]DECOMPRESS_.OBJ : []DECOMPRESS.C
	MY_MODULE_NAME = f$parse( "$(MMS$SOURCE)", , , "NAME", "SYNTAX_ONLY")
	$(CC) $(CFLAGS_NS) -
	 /define = ($(CDEFS), BZ_NO_STDIO=1, MY_MODULE_NAME='MY_MODULE_NAME') -
	 $(MY_MODULE_NAME_H)+ $(MMS$SOURCE)

[.$(DEST)]HUFFMAN_.OBJ : []HUFFMAN.C
	MY_MODULE_NAME = f$parse( "$(MMS$SOURCE)", , , "NAME", "SYNTAX_ONLY")
	$(CC) $(CFLAGS_NS) -
	 /define = ($(CDEFS), BZ_NO_STDIO=1, MY_MODULE_NAME='MY_MODULE_NAME') -
	 $(MY_MODULE_NAME_H)+ $(MMS$SOURCE)

[.$(DEST)]RANDTABLE_.OBJ : []RANDTABLE.C
	MY_MODULE_NAME = f$parse( "$(MMS$SOURCE)", , , "NAME", "SYNTAX_ONLY")
	$(CC) $(CFLAGS_NS) -
	 /define = ($(CDEFS), BZ_NO_STDIO=1, MY_MODULE_NAME='MY_MODULE_NAME') -
	 $(MY_MODULE_NAME_H)+ $(MMS$SOURCE)

# BZIP2 executable.

$(BZIP2_EXE) : [.$(DEST)]BZIP2.OBJ $(LIB_BZ2)
	$(LINK) $(LINKFLAGS) $(MMS$SOURCE), -
	 $(LIB_BZ2) /library

# BZIPUTILS executable(s).

$(BZIP2RECOVER_EXE) : [.$(DEST)]BZIP2RECOVER.OBJ $(LIB_BZ2)
	$(LINK) $(LINKFLAGS) $(MMS$SOURCE), -
	 $(LIB_BZ2) /library

# DECC_VER executable.

$(DECC_VER_EXE) : $(DECC_VER_OBJ)
	$(LINK) $(LINKFLAGS) $(MMS$SOURCE)

$(DECC_VER_OBJ) : [.vms]DECC_VER.C

# TEST target.

TEST : $(BZIP2_EXE)
	@ type words1.
	execute = "$ SYS$DISK:[.$(DEST)]'"
	if (f$search( "[.$(DEST)]sample1.rb2") .nes. "") then -
	 delete [.$(DEST)]sample1.rb2;*
	define /user_mode sys$output [.$(DEST)]sample1.rb2
	execute bzip2 -1 -c sample1.ref
	if (f$search( "[.$(DEST)]sample2.rb2") .nes. "") then -
	 delete [.$(DEST)]sample2.rb2;*
	define /user_mode sys$output [.$(DEST)]sample2.rb2
	execute bzip2 -2 -c sample2.ref
	if (f$search( "[.$(DEST)]sample3.rb2") .nes. "") then -
	 delete [.$(DEST)]sample3.rb2;*
	define /user_mode sys$output [.$(DEST)]sample3.rb2
	execute bzip2 -3 -c sample3.ref
	if (f$search( "[.$(DEST)]sample1.tst") .nes. "") then -
	 delete [.$(DEST)]sample1.tst;*
	define /user_mode sys$output [.$(DEST)]sample1.tst
	execute bzip2 -d -c sample1.bz2
	if (f$search( "[.$(DEST)]sample2.tst") .nes. "") then -
	 delete [.$(DEST)]sample2.tst;*
	define /user_mode sys$output [.$(DEST)]sample2.tst
	execute bzip2 -d -c sample2.bz2
	if (f$search( "[.$(DEST)]sample3.tst") .nes. "") then -
	 delete [.$(DEST)]sample3.tst;*
	define /user_mode sys$output [.$(DEST)]sample3.tst
	execute bzip2 -ds -c sample3.bz2
	@ write sys$output ""
	@ write sys$output "Tests complete.  Checking results..."
	@ write sys$output ""
	backup /compare sample1.bz2 [.$(DEST)]sample1.rb2
	backup /compare sample2.bz2 [.$(DEST)]sample2.rb2
	backup /compare sample3.bz2 [.$(DEST)]sample3.rb2
	backup /compare sample1.ref [.$(DEST)]sample1.tst
	backup /compare sample2.ref [.$(DEST)]sample2.tst
	backup /compare sample3.ref [.$(DEST)]sample3.tst
	@ write sys$output ""
	@ write sys$output "Tests successful."
	@ write sys$output ""


# Include generated source dependencies.

INCL_DESCRIP_DEPS = 1
.INCLUDE [.vms]descrip_deps.mms

