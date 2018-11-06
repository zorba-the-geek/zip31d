# DESCRIP_MKDEPS.MMS
#
#    Zip 3.1 for VMS - MMS Dependency Description File.
#
#    Last revised:  2014-10-19
#
#----------------------------------------------------------------------
# Copyright (c) 2004-2014 Info-ZIP.  All rights reserved.
#
# See the accompanying file LICENSE, version 2009-Jan-2 or later (the
# contents of which are also included in zip.h) for terms of use.  If,
# for some reason, all these files are missing, the Info-ZIP license
# may also be found at: ftp://ftp.info-zip.org/pub/infozip/license.html
#----------------------------------------------------------------------
#
#    MMS /EXTENDED_SYNTAX description file to generate a C source
#    dependencies file.  Unsightly errors result when /EXTENDED_SYNTAX
#    is not specified.  Typical usage:
#
#    $ MMS /EXTEND /DESCRIP = [.VMS]DESCRIP_MKDEPS.MMS /SKIP -
#       /MACRO = (AES_WG=1, IZ_BZIP2=iz_bzip2, IZ_ZLIB=iz_zlib, -
#       LIBZIP=1)
#
# If the IZ_AES_WG encryption source kit has not been installed, then
# the macro AES_WG should not be defined.
#
# Note that this description file must be used from the main
# distribution directory, not from the [.VMS] subdirectory.
#
# This description file uses these command procedures:
#
#    [.VMS]MOD_DEP.COM
#    [.VMS]COLLECT_DEPS.COM
#
# MMK users without MMS will be unable to generate the dependencies file
# using this description file, however there should be one supplied in
# the kit.  If this file has been deleted, users in this predicament
# will need to recover it from the original distribution kit.
#
# Note:  This dependency generation scheme assumes that the dependencies
# do not depend on host architecture type or other such variables.
# Therefore, no "#include" directive in the C source itself should be
# conditional on such variables.
#
# The default target is the comprehensive source dependency file,
# DEPS_FILE = [.VMS]DESCRIP_DEPS.MMS.
#
# Other targets:
#
#    CLEAN      deletes the individual source dependency files,
#               *.MMSD;*, but leaves the comprehensive source dependency
#               file.
#
#    CLEAN_ALL  deletes all source dependency files, including the
#               individual *.MMSD;* files and the comprehensive file,
#               DESCRIP_DEPS.MMS.*.
#

# Required command procedures.

COLLECT_DEPS = [.VMS]COLLECT_DEPS.COM
MOD_DEP = [.VMS]MOD_DEP.COM

COMS = $(COLLECT_DEPS) $(MOD_DEP)

# Include the source file lists (among other data).

INCL_DESCRIP_SRC = 1
.INCLUDE [.VMS]DESCRIP_SRC.MMS

# The ultimate product, a comprehensive dependency list.

DEPS_FILE = [.VMS]DESCRIP_DEPS.MMS

# Detect valid qualifier and/or macro options.

.IF $(FINDSTRING Skip, $(MMSQUALIFIERS)) .eq Skip
DELETE_MMSD = 1
.ELSIF NOSKIP
PURGE_MMSD = 1
.ELSE
UNK_MMSD = 1
.ENDIF

# Dependency suffixes and rules.
#
# .FIRST is assumed to be used already, so the MMS qualifier/macro check
# is included in each rule (one way or another).

.SUFFIXES_BEFORE .C .MMSD

.C.MMSD :
.IF UNK_MMSD
	@ write sys$output -
 "   /SKIP_INTERMEDIATES is expected on the MMS command line."
	@ write sys$output -
 "   For normal behavior (delete .MMSD files), specify ""/SKIP""."
	@ write sys$output -
 "   To retain the .MMSD files, specify ""/MACRO = NOSKIP=1""."
	@ exit %x00000004
.ENDIF
	$(CC) $(CFLAGS_DEP) $(CDEFS_UNX) $(MMS$SOURCE) /NOLIST /NOOBJECT -
	 /MMS_DEPENDENCIES = (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)

# List of MMS dependency files.

# In case it's not obvious...
# To extract module name lists from object library module=object lists:
# 1.  Transform "module=[.dest]name.OBJ" into "module=[.dest] name".
# 2.  For [.VMS], add [.VMS] to name.
# 3.  Delete "*]" words.
#
# A similar scheme works for executable lists.

MODS_LIB_ZIP_N = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] *, $(MODS_OBJS_LIB_ZIP_N)))

MODS_LIB_LIBZIP_N = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] *, $(MODS_OBJS_LIB_LIBZIP_N)))

MODS_LIB_ZIP_V = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] [.VMS]*, $(MODS_OBJS_LIB_ZIP_V)))

MODS_LIB_ZIP_AES = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] [.AES_WG]*, $(MODS_OBJS_LIB_ZIP_AES)))

MODS_LIB_ZIP_LZMA = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] [.SZIP]*, $(MODS_OBJS_LIB_ZIP_LZMA)))

MODS_LIB_ZIP_PPMD = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] [.SZIP]*, $(MODS_OBJS_LIB_ZIP_PPMD)))

MODS_LIB_ZIPUTILS_N = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] *, $(MODS_OBJS_LIB_ZIPUTILS_N)))

MODS_LIB_ZIPUTILS_N_V = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] [.VMS]*, $(MODS_OBJS_LIB_ZIPUTILS_N_V)))

MODS_LIB_ZIPUTILS_U = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] *, $(MODS_OBJS_LIB_ZIPUTILS_U)))

MODS_LIB_ZIPUTILS_U_V = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] [.VMS]*, $(MODS_OBJS_LIB_ZIPUTILS_U_V)))

MODS_LIB_ZIPCLI_V = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] [.VMS]*, $(MODS_OBJS_LIB_ZIPCLI_C_V)))

MODS_ZIP = $(FILTER-OUT *], \
 $(PATSUBST *]*.EXE, *] *, $(ZIP)))

MODS_ZIPUTILS = $(FILTER-OUT *], \
 $(PATSUBST *]*.EXE, *] *, $(ZIPUTILS)))

# Complete list of C object dependency file names.
# Note that the CLI Zip main program object file is a special case.

DEPS = $(FOREACH NAME, \
 $(MODS_LIB_ZIP_N) $(MODS_LIB_LIBZIP_N) $(MODS_LIB_ZIP_V) \
 $(MODS_LIB_ZIP_AES) \
 $(MODS_LIB_ZIP_LZMA) \
 $(MODS_LIB_ZIP_PPMD) \
 $(MODS_ZIPUTILS_N) $(MODS_ZIPUTILS_N_V) \
 $(MODS_LIB_ZIPUTILS_U) $(MODS_LIB_ZIPUTILS_U_V) \
 $(MODS_LIB_ZIPCLI_V) \
 $(MODS_ZIP) ZIPCLI $(MODS_ZIPUTILS), \
 $(NAME).MMSD)

# Default target is the comprehensive dependency list.

$(DEPS_FILE) : $(DEPS) $(COMS)
.IF UNK_MMSD
	@ write sys$output -
 "   /SKIP_INTERMEDIATES is expected on the MMS command line."
	@ write sys$output -
 "   For normal behavior (delete individual .MMSD files), specify ""/SKIP""."
	@ write sys$output -
 "   To retain the individual .MMSD files, specify ""/MACRO = NOSKIP=1""."
	@ exit %x00000004
.ENDIF
#
#       Note that the space in P4, which prevents immediate macro
#       expansion, is removed by COLLECT_DEPS.COM.
#
	@$(COLLECT_DEPS) "Zip for VMS" "$(MMS$TARGET)" -
         "[...]*.MMSD" "[.$ (DEST)]" $(MMSDESCRIPTION_FILE) -
         "[.AES_WG/[.SZIP]C/[.SZIP]P/[.SZIP]S/[.SZIP]T/[.SZIP" -
         "AES_WG/LZMA_PPMD/PPMD/LZMA_PPMD/LZMA_PPMD/LZMA"
	@ write sys$output -
         "Created a new dependency file: $(MMS$TARGET)"
.IF DELETE_MMSD
	@ write sys$output -
         "Deleting intermediate .MMSD files..."
	if (f$search( "*.MMSD") .nes. "") then -
         delete /log *.MMSD;*
	if (f$search( "[.aes_wg]*.MMSD") .nes. "") then -
         delete /log [.aes_wg]*.MMSD;*
	if (f$search( "[.szip]*.MMSD") .nes. "") then -
         delete /log [.szip]*.MMSD;*
	if (f$search( "[.VMS]*.MMSD") .nes. "") then -
         delete /log [.VMS]*.MMSD;*
.ELSE
	@ write sys$output -
         "Purging intermediate .MMSD files..."
	if (f$search( "*.MMSD;-1") .nes. "") then -
         purge /log *.MMSD
	if (f$search( "[.aes_wg]*.MMSD;-1") .nes. "") then -
         purge /log [.aes_wg]*.MMSD
	if (f$search( "[.szip]*.MMSD;-1") .nes. "") then -
         purge /log [.szip]*.MMSD
	if (f$search( "[.VMS]*.MMSD;-1") .nes. "") then -
         purge /log [.VMS]*.MMSD
.ENDIF

# CLEAN target.  Delete the individual C dependency files.

CLEAN :
	if (f$search( "*.MMSD") .nes. "") then -
         delete /log *.MMSD;*
	if (f$search( "[.aes_wg]*.MMSD") .nes. "") then -
         delete /log [.aes_wg]*.MMSD;*
	if (f$search( "[.szip]*.MMSD") .nes. "") then -
         delete /log [.szip]*.MMSD;*
	if (f$search( "[.VMS]*.MMSD") .nes. "") then -
         delete /log [.VMS]*.MMSD;*

# CLEAN_ALL target.  Delete:
#    The individual C dependency files.
#    The collected source dependency file.

CLEAN_ALL :
	if (f$search( "*.MMSD") .nes. "") then -
         delete /log *.MMSD;*
	if (f$search( "[.aes_wg]*.MMSD") .nes. "") then -
         delete /log [.aes_wg]*.MMSD;*
	if (f$search( "[.szip]*.MMSD") .nes. "") then -
         delete /log [.szip]*.MMSD;*
	if (f$search( "[.VMS]*.MMSD") .nes. "") then -
         delete /log [.VMS]*.MMSD;*
	if (f$search( "[.VMS]DESCRIP_DEPS.MMS") .nes. "") then -
         delete /log [.VMS]DESCRIP_DEPS.MMS;*

# Explicit dependencies and rules for library and utility variant modules.
#
# The extra dependency on the normal dependency file obviates including
# the /SKIP warning code in each rule here.

CRC32_.MMSD : CRC32.C CRC32.MMSD
	$(CC) $(CFLAGS_DEP) $(CDEFS_UNX) $(CFLAGS_CLI) $(MMS$SOURCE) -
         /NOLIST /NOOBJECT /MMS_DEPENDENCIES = -
         (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@$(MOD_DEP) $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

CRYPT_.MMSD : CRYPT.C CRYPT.MMSD
	$(CC) $(CFLAGS_DEP) $(CDEFS_UNX) $(CFLAGS_CLI) $(MMS$SOURCE) -
         /NOLIST /NOOBJECT /MMS_DEPENDENCIES = -
         (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@$(MOD_DEP) $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

FILEIO_.MMSD : FILEIO.C FILEIO.MMSD
	$(CC) $(CFLAGS_DEP) $(CDEFS_UNX) $(CFLAGS_CLI) $(MMS$SOURCE) -
         /NOLIST /NOOBJECT /MMS_DEPENDENCIES = -
         (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@$(MOD_DEP) $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

UTIL_.MMSD : UTIL.C UTIL.MMSD
	$(CC) $(CFLAGS_DEP) $(CDEFS_UNX) $(CFLAGS_CLI) $(MMS$SOURCE) -
         /NOLIST /NOOBJECT /MMS_DEPENDENCIES = -
         (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@$(MOD_DEP) $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

ZIPFILE_.MMSD : ZIPFILE.C ZIPFILE.MMSD
	$(CC) $(CFLAGS_DEP) $(CDEFS_UNX) $(CFLAGS_CLI) $(MMS$SOURCE) -
         /NOLIST /NOOBJECT /MMS_DEPENDENCIES = -
         (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@$(MOD_DEP) $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

[.VMS]VMS_.MMSD : [.VMS]VMS.C [.VMS]VMS.MMSD
	$(CC) $(CFLAGS_DEP) $(CDEFS_UNX) $(CFLAGS_CLI) $(MMS$SOURCE) -
         /NOLIST /NOOBJECT /MMS_DEPENDENCIES = -
         (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@$(MOD_DEP) $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

ZIPCLI.MMSD : ZIP.C ZIP.MMSD
	$(CC) $(CFLAGS_DEP) $(CDEFS_UNX) $(CFLAGS_CLI) $(MMS$SOURCE) -
         /NOLIST /NOOBJECT /MMS_DEPENDENCIES = -
         (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@$(MOD_DEP) $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

# Zip library modules.

API_.MMSD : API.C
	$(CC) $(CFLAGS_DEP) $(CDEFS_LIBZIP) $(MMS$SOURCE) -
         /NOLIST /NOOBJECT /MMS_DEPENDENCIES = -
         (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@$(MOD_DEP) $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

ZIP_.MMSD : ZIP.C ZIP.MMSD
	$(CC) $(CFLAGS_DEP) $(CDEFS_LIBZIP) $(MMS$SOURCE) -
         /NOLIST /NOOBJECT /MMS_DEPENDENCIES = -
         (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@$(MOD_DEP) $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

# Special case.  No normal (non-CLI) version.

[.VMS]CMDLINE.MMSD : [.VMS]CMDLINE.C
.IF UNK_MMSD
	@ write sys$output -
 "   /SKIP_INTERMEDIATES is expected on the MMS command line."
	@ write sys$output -
 "   For normal behavior (delete .MMSD files), specify ""/SKIP""."
	@ write sys$output -
 "   To retain the .MMSD files, specify ""/MACRO = NOSKIP=1""."
	@ exit %x00000004
.ENDIF
	$(CC) $(CFLAGS_DEP) $(CDEFS_UNX) $(CFLAGS_CLI) $(MMS$SOURCE) -
         /NOLIST /NOOBJECT /MMS_DEPENDENCIES = -
         (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@$(MOD_DEP) $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

