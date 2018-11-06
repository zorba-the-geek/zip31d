# DESCRIP.MMS
#
#    Zip 3.1 for VMS -- MMS (or MMK) Description File.
#
#    Last revised:  2014-11-10
#
#----------------------------------------------------------------------
# Copyright (c) 1998-2014 Info-ZIP.  All rights reserved.
#
# See the accompanying file LICENSE, version 2009-Jan-2 or later (the
# contents of which are also included in zip.h) for terms of use.  If,
# for some reason, all these files are missing, the Info-ZIP license
# may also be found at: ftp://ftp.info-zip.org/pub/infozip/license.html
#----------------------------------------------------------------------
#
# Usage:
#
#    MMS /DESCRIP = [.VMS]DESCRIP.MMS [/MACRO = (<see_below>)] [target]
#
# Note that this description file must be used from the main
# distribution directory, not from the [.VMS] subdirectory.
#
# Optional macros:
#
#    AES_WG=1       Enable/disable AES (WinZip/Gladman) encryption
#    NOAES_WG=1     support.  Specify either AES_WG=1 or NOAES_WG=1 to
#                   skip the [.aes_wg] source directory test.
#
#    CCOPTS=xxx     Compile with CC options xxx.  For example:
#                   CCOPTS=/ARCH=HOST
#
#    DBG=1          Compile /DEBUG /NOOPTIMIZE.  Link /DEBUG /TRACEBACK.
#    TRC=1          Default is /NOTRACEBACK, but TRC=1 enables link with
#                   /TRACEBACK without compiling for debug.
#
#    IM=1           Use the old "IM" scheme for storing VMS/RMS file
#                   atributes, instead of the newer "PK" scheme.
#
#    IZ_BZIP2=dev:[dir]  Direct/disable optional bzip2 support.  By
#    NOIZ_BZIP2=1        default, bzip2 support is enabled, and uses the
#                   bzip2 source kit supplied in the [.bzip2] directory.
#                   Specify NOIZ_BZIP2=1 to disable bzip2 support.
#                   Specify IZ_BZIP2 with a value ("dev:[dir]", or a
#                   suitable logical name) to use the bzip2 header file
#                   and object library found there.  The bzip2 object
#                   library (LIBBZ2_NS.OLB) is expected to be in a
#                   simple "[.dest]" directory under that one
#                   ("dev:[dir.ALPHAL]", for example), or in that
#                   directory itself.)
#
#    IZ_ZLIB=dev:[dir]  Use ZLIB compression library instead of internal
#                       Deflate compression routines.  The value of the
#                   MMS macro IZ_ZLIB ("dev:[dir]", or a suitable
#                   logical name) tells where to find "zlib.h".  The
#                   ZLIB object library (LIBZ.OLB) is expected to be in
#                   a "[.dest]" directory under that one
#                   ("dev:[dir.ALPHAL]", for example), or in that
#                   directory itself.
#
#    LARGE=1        Enable/disable large-file (>2GB) support.  Always
#    NOLARGE=1      disabled on VAX.  Enabled by default on Alpha and
#                   IA64.  On Alpha, by default, large-file support is
#                   tested, and the build will fail if that test fails.
#                   Specify NOLARGE=1 explicitly to disable support (and
#                   to skip the test on Alpha).
#
#    LIBZIP=1       Build LIBIZZIP.OLB as a callable Zip library.
#
#    LINKOPTS=xxx   Link with LINK options xxx.  For example:
#                   LINKOPTS=/NOINFO
#
#    LIST=1         Compile with /LIST /SHOW = (ALL, NOMESSAGES).
#                   Link with /MAP /CROSS_REFERENCE /FULL.
#
#    "LOCAL_ZIP=c_macro_1=value1 [, c_macro_2=value2 [...]]"
#                   Compile with these additional C macros defined.  For
#                   example:
#                   "LOCAL_ZIP=NO_EXCEPT_SIGNALS=1, NO_SYMLINKS=1"
#
#    NOLZMA=1       Disable LZMA compression support, which is enabled
#                   by default.
#
#    NOPPMD=1       Disable PPMd compression support, which is enabled
#                   by default.
#
#    NOSYSSHR=1     Link /NOSYSSHR (not using shareable images).
#    NOSYSSHR=OLDVAX  Link /NOSYSSHR on VAX for:
#                      DEC C with VMS before V7.3.
#                      VAX C without DEC C RTL (DEC C not installed).
#
#    PROD=subdir    Use [.subdir] as the destination for
#                   architecture-specific product files (.EXE, .OBJ,
#                   .OLB, and so on).  The default is a name
#                   automatically generated using rules defined in
#                   [.VMS]DESCRIP_SRC.MMS.  Note that using this option
#                   carelessly can confound the CLEAN* targets.
#
# VAX-specific optional macros:
#
#    VAXC=1         Use the VAX C compiler, assuming "CC" runs it.
#                   (That is, DEC C is not installed, or else DEC C is
#                   installed, but VAX C is the default.)
#
#    FORCE_VAXC=1   Use the VAX C compiler, assuming "CC /VAXC" runs it.
#                   (That is, DEC C is installed, and it is the
#                   default, but you want VAX C anyway, you fool.)
#
#    GNUC=1         Use the GNU C compiler.  (Seriously under-tested.)
#
#
# The default target, ALL, builds the selected product executables and
# help files.
#
# Other targets:
#
#    CLEAN      deletes architecture-specific files, but leaves any
#               individual source dependency files and the help files.
#
#    CLEAN_ALL  deletes all generated files, except the main (collected)
#               source dependency file.
#
#    CLEAN_EXE  deletes only the architecture-specific executables.
#               Handy if all you wish to do is re-link the executables.
#
#    CLEAN_OLB  deletes only the architecture-specific object libraries.
#
#    DASHV      generates a "zip -v" report.
#
#    HELP       generates HELP files.
#
#    HELP_TEXT  generates HELP output text files (.HTX).
#
#    SLASHV     generates a "zip_cli /verbose" report.
#
# Example commands:
#
# To build the large-file product (except on VAX) with all the available
# optional compression methods using the DEC/Compaq/HP C compiler (Note:
# DESCRIP.MMS is the default description file name.):
#
#    MMS /DESCRIP = [.VMS]
#
# To get small-file executables (on a non-VAX system):
#
#    MMS /DESCRIP = [.VMS] /MACRO = (NOLARGE=1)
#
# To delete the architecture-specific generated files for this system
# type:
#
#    MMS /DESCRIP = [.VMS] CLEAN
#    MMS /DESCRIP = [.VMS] /MACRO = (NOLARGE=1) CLEAN   ! Non-VAX,
#                                                       ! small-file.
#
# To build a complete product for debug with compiler listings and link
# maps:
#
#    MMS /DESCRIP = [.VMS] CLEAN
#    MMS /DESCRIP = [.VMS] /MACRO = (DBG=1, LIST=1)
#
#
#    Note that option macros like NOLARGE or PROD affect the destination
#    directory for various product files, including executables, and
#    various clean and test targets (CLEAN, DASHV, and so on) need
#    to use the proper destination directory.  Thus, if NOLARGE or PROD
#    is specified for a build, then the same macro must be specified for
#    the various clean and test targets, too.
#
#    Note that on a Unix system, LOCAL_ZIP contains compiler
#    options, such as "-g" or "-DNO_USER_PROGRESS", but on a VMS
#    system, LOCAL_ZIP contains only C macros, such as
#    "NO_USER_PROGRESS", and CCOPTS is used for any other kinds of
#    compiler options, such as "/ARCHITECTURE".  Unix compilers accept
#    multiple "-D" options, but VMS compilers consider only the last
#    /DEFINE qualifier, so the C macros must be handled differently
#    from other compiler options on VMS.  Thus, when using the generic
#    installation instructions as a guide for controlling various
#    optional features, some adjustment may be needed to adapt them to
#    a VMS build environment.
#
########################################################################

# Explicit suffix list.  (Added .HTX before .HLB.)

.SUFFIXES
.SUFFIXES : .EXE .OLB .HTX .HLB .OBJ .C .CLD .MSG .HLP .RNH

# Include primary product description file.

INCL_DESCRIP_SRC = 1
.INCLUDE [.VMS]DESCRIP_SRC.MMS

# Object library names.

.IFDEF LIBZIP                   # LIBZIP
LIB_ZIP_NAME = LIBIZZIP.OLB
.ELSE                           # LIBZIP
LIB_ZIP_NAME = ZIP.OLB
.ENDIF                          # LIBZIP

LIB_ZIPCLI_NAME = ZIPCLI.OLB
LIB_ZIPUTILS_NAME = ZIPUTILS.OLB

LIB_ZIP = SYS$DISK:[.$(DEST)]$(LIB_ZIP_NAME)
LIB_ZIPCLI = SYS$DISK:[.$(DEST)]$(LIB_ZIPCLI_NAME)
LIB_ZIPUTILS = SYS$DISK:[.$(DEST)]$(LIB_ZIPUTILS_NAME)

# Help library source file names.

ZIP_HELP = ZIP.HLP ZIP_CLI.HLP

# Help output text file names.

ZIP_HELP_TEXT = ZIP.HTX ZIP_CLI.HTX

# Message file names.

ZIP_MSG_MSG = [.VMS]ZIP_MSG.MSG
ZIP_MSG_EXE = [.$(DEST)]ZIP_MSG.EXE
ZIP_MSG_OBJ = [.$(DEST)]ZIP_MSG.OBJ

# Library link options file.

.IFDEF LIBZIP                   # LIBZIP
LIBZIP_OPT = [.$(DEST)]LIB_IZZIP.OPT
.ENDIF                          # LIBZIP

# TARGETS.

# Default target, ALL.  Build All Zip executables, utility executables,
# and help files.

ALL : $(ZIP) $(ZIP_CLI) $(ZIPUTILS) $(ZIP_HELP) $(ZIP_MSG_EXE) $(LIBZIP_OPT)
	@ write sys$output "Done."

# CLEAN* targets.  These also similarly clean a local bzip2 directory.

# CLEAN target.  Delete:
#    The [.$(DEST)] directory and everything in it.

CLEAN :
	if (f$search( "[.$(DEST)]*.*") .nes. "") then -
	 delete /noconfirm [.$(DEST)]*.*;*
	if (f$search( "$(DEST).DIR") .nes. "") then -
	 set protection = w:d $(DEST).DIR;*
	if (f$search( "$(DEST).DIR") .nes. "") then -
	 delete /noconfirm $(DEST).DIR;*
.IFDEF BUILD_BZIP2              # BUILD_BZIP2
	@ write sys$output ""
	@ write sys$output "Cleaning bzip2..."
	def_dev_dir_orig = f$environment( "default")
	set default $(IZ_BZIP2)
	$(MMS) $(MMSQUALIFIERS) /DESCR=[.vms]descrip.mms -
	 $(IZ_BZIP2_MACROS) -
	 $(MMSTARGETS)
	set default 'def_dev_dir_orig'
.ENDIF				# BUILD_BZIP2

# CLEAN_ALL target.  Delete:
#    The [.$(DEST)] directory and everything in it (CLEAN),
#    The standard [.$(DEST)] directories and everything in them,
#    All help-related derived files,
#    All individual C dependency files.
# Also mention:
#    Comprehensive dependency file.

CLEAN_ALL : CLEAN
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
	if (f$search( "help_temp_*.*") .nes. "") then -
	 delete help_temp_*.*;*
	if (f$search( "[.vms]ZIP_CLI.RNH") .nes. "") then -
	 delete /noconfirm [.vms]ZIP_CLI.RNH;*
	if (f$search( "ZIP_CLI.HLP") .nes. "") then -
	 delete /noconfirm ZIP_CLI.HLP;*
	if (f$search( "ZIP.HLP") .nes. "") then -
	 delete /noconfirm ZIP.HLP;*
	if (f$search( "*.MMSD") .nes. "") then -
	 delete /noconfirm *.MMSD;*
	if (f$search( "[.vms]*.MMSD") .nes. "") then -
	 delete /noconfirm [.vms]*.MMSD;*
	@ write sys$output ""
	@ write sys$output "Note:  This procedure will not"
	@ write sys$output "   DELETE [.VMS]DESCRIP_DEPS.MMS;*"
	@ write sys$output -
 "You may choose to, but a recent version of MMS (V3.5 or newer?) is"
	@ write sys$output -
 "needed to regenerate it.  (It may also be recovered from the original"
	@ write sys$output -
 "distribution kit.)  See [.VMS]DESCRIP_MKDEPS.MMS for instructions on"
	@ write sys$output -
 "generating [.VMS]DESCRIP_DEPS.MMS."
	@ write sys$output ""
	@ write sys$output -
 "It also does not delete the error message source file:"
	@ write sys$output "   DELETE [.VMS]ZIP_MSG.MSG;*"
	@ write sys$output -
 "but it can regenerate it if needed."
	@ write sys$output ""

# CLEAN_EXE target.  Delete the executables in [.$(DEST)].

CLEAN_EXE :
	if (f$search( "[.$(DEST)]*.EXE") .nes. "") then -
	 delete /noconfirm [.$(DEST)]*.EXE;*
.IFDEF BUILD_BZIP2              # BUILD_BZIP2
	@ write sys$output ""
	@ write sys$output "Cleaning bzip2..."
	def_dev_dir_orig = f$environment( "default")
	set default $(IZ_BZIP2)
	$(MMS) $(MMSQUALIFIERS) /DESCR=[.vms]descrip.mms -
	 $(IZ_BZIP2_MACROS) -
	 $(MMSTARGETS)
	set default 'def_dev_dir_orig'
.ENDIF				# BUILD_BZIP2

# CLEAN_OLB target.  Delete the object libraries in [.$(DEST)].

CLEAN_OLB :
	if (f$search( "[.$(DEST)]*.OLB") .nes. "") then -
	 delete /noconfirm [.$(DEST)]*.OLB;*
.IFDEF BUILD_BZIP2              # BUILD_BZIP2
	@ write sys$output ""
	@ write sys$output "Cleaning bzip2..."
	def_dev_dir_orig = f$environment( "default")
	set default $(IZ_BZIP2)
	$(MMS) $(MMSQUALIFIERS) /DESCR=[.vms]descrip.mms -
	 $(IZ_BZIP2_MACROS) -
	 $(MMSTARGETS)
	set default 'def_dev_dir_orig'
.ENDIF				# BUILD_BZIP2

CLEAN_TEST :
.IFDEF BUILD_BZIP2              # BUILD_BZIP2
	@ write sys$output ""
	@ write sys$output "Cleaning bzip2..."
	def_dev_dir_orig = f$environment( "default")
	set default $(IZ_BZIP2)
	$(MMS) $(MMSQUALIFIERS) /DESCR=[.vms]descrip.mms -
	 $(IZ_BZIP2_MACROS) -
	 $(MMSTARGETS)
	set default 'def_dev_dir_orig'
.ELSE 				# BUILD_BZIP2
	@ write sys$output "No action needed."
.ENDIF				# BUILD_BZIP2

# DASHV target.  Generate a "zip -v" report.

DASHV :
	mcr [.$(DEST)]zip -v

# HELP target.  Generate the HELP library source files.

HELP : $(ZIP_HELP)
	@ write sys$output "Done."

# HELP_TEXT target.  Generate the HELP output text files.

HELP_TEXT : $(ZIP_HELP_TEXT)
	@ write sys$output "Done."

# SLASHV target.  Generate a "zip_cli /verbose" report.

SLASHV :
	mcr [.$(DEST)]zip_cli /verbose

# Object library module dependencies.

$(LIB_ZIP) : $(LIB_ZIP)($(MODS_OBJS_LIB_ZIP))
	@ write sys$output "$(MMS$TARGET) updated."

$(LIB_ZIPCLI) : $(LIB_ZIPCLI)($(MODS_OBJS_LIB_ZIPCLI))
	@ write sys$output "$(MMS$TARGET) updated."

$(LIB_ZIPUTILS) : $(LIB_ZIPUTILS)($(MODS_OBJS_LIB_ZIPUTILS))
	@ write sys$output "$(MMS$TARGET) updated."

# Module ID options file.

OPT_ID = SYS$DISK:[.$(DEST)]ZIP.OPT

# Default C compile rule.

.C.OBJ :
	$(CC) $(CFLAGS) $(CDEFS_UNX) $(MMS$SOURCE)


# Normal sources in [.VMS].

[.$(DEST)]VMS.OBJ : [.VMS]VMS.C
[.$(DEST)]VMSMUNCH.OBJ : [.VMS]VMSMUNCH.C
[.$(DEST)]VMSZIP.OBJ : [.VMS]VMSZIP.C

# Command-line interface files.

[.$(DEST)]CMDLINE.OBJ : [.VMS]CMDLINE.C
	$(CC) $(CFLAGS) $(CDEFS_CLI) $(MMS$SOURCE)

[.$(DEST)]ZIPCLI.OBJ : ZIP.C
	$(CC) $(CFLAGS) $(CDEFS_CLI) $(MMS$SOURCE)

[.$(DEST)]ZIP_CLI.OBJ : [.$(DEST)]ZIP_CLI.CLD

[.$(DEST)]ZIP_CLI.CLD : [.VMS]ZIP_CLI.CLD
	@[.vms]cppcld.com "$(CC) $(CFLAGS_ARCH)" -
	 $(MMS$SOURCE) $(MMS$TARGET) "$(CDEFS)"

# Utility variant sources.

[.$(DEST)]CRC32_.OBJ : CRC32.C
	$(CC) $(CFLAGS) $(CDEFS_UTIL) $(MMS$SOURCE)

[.$(DEST)]CRYPT_.OBJ : CRYPT.C
	$(CC) $(CFLAGS) $(CDEFS_UTIL) $(MMS$SOURCE)

[.$(DEST)]FILEIO_.OBJ : FILEIO.C
	$(CC) $(CFLAGS) $(CDEFS_UTIL) $(MMS$SOURCE)

[.$(DEST)]UTIL_.OBJ : UTIL.C
	$(CC) $(CFLAGS) $(CDEFS_UTIL) $(MMS$SOURCE)

[.$(DEST)]ZIPFILE_.OBJ : ZIPFILE.C
	$(CC) $(CFLAGS) $(CDEFS_UTIL) $(MMS$SOURCE)

[.$(DEST)]VMS_.OBJ : [.VMS]VMS.C
	$(CC) $(CFLAGS) $(CDEFS_UTIL) $(MMS$SOURCE)

# Utility main sources.

[.$(DEST)]ZIPCLOAK.OBJ : ZIPCLOAK.C
	$(CC) $(CFLAGS) $(CDEFS_UTIL) $(MMS$SOURCE)

[.$(DEST)]ZIPNOTE.OBJ : ZIPNOTE.C
	$(CC) $(CFLAGS) $(CDEFS_UTIL) $(MMS$SOURCE)

[.$(DEST)]ZIPSPLIT.OBJ : ZIPSPLIT.C
	$(CC) $(CFLAGS) $(CDEFS_UTIL) $(MMS$SOURCE)

# Library variant sources.

[.$(DEST)]API_.OBJ : API.C
	$(CC) $(CFLAGS) $(CDEFS_LIBZIP) $(MMS$SOURCE)

[.$(DEST)]ZIP_.OBJ : ZIP.C
	$(CC) $(CFLAGS) $(CDEFS_LIBZIP) $(MMS$SOURCE)

# VAX C LINK options file.

.IFDEF OPT_FILE
$(OPT_FILE) :
	open /write opt_file_ln $(OPT_FILE)
	write opt_file_ln "SYS$SHARE:VAXCRTL.EXE /SHARE"
	close opt_file_ln
.ENDIF

# Module ID options files.

$(OPT_ID) :
	@[.vms]optgen.com Zip iz_zip_versn
	open /write opt_file_ln $(OPT_ID)
	write opt_file_ln "Ident = ""Zip ''f$trnlnm( "iz_zip_versn")'"""
	close opt_file_ln

# Local BZIP2 object library.

$(LIB_BZ2_LOCAL) :
	@ write sys$output ""
	@ write sys$output "Building bzip2..."
	def_dev_dir_orig = f$environment( "default")
	set default $(IZ_BZIP2)
	$(MMS) $(MMSQUALIFIERS) /DESCR=[.vms]descrip.mms -
	 $(IZ_BZIP2_MACROS) -
	 $(MMSTARGETS)
	set default 'def_dev_dir_orig'
	@ write sys$output ""

# Normal Zip executable.

$(ZIP) : [.$(DEST)]ZIP.OBJ $(LIB_ZIP) \
          $(LIB_BZ2_DEP) $(OPT_FILE) $(OPT_ID)
	$(LINK) $(LINKFLAGS) $(MMS$SOURCE), -
	 $(LIB_ZIP) /library,  -
	 $(LIB_BZIP2_OPTS) -
	 $(LIB_ZIP) /library,  -
	 $(LIB_ZLIB_OPTS) -
	 $(LFLAGS_ARCH) -
	 $(OPT_ID) /options -
	 $(NOSYSSHR_OPTS)


# CLI Zip executable.

$(ZIP_CLI) : [.$(DEST)]ZIPCLI.OBJ \
              $(LIB_ZIPCLI) $(LIB_ZIP) $(LIB_BZ2_DEP) \
              $(OPT_FILE) $(OPT_ID)
	$(LINK) $(LINKFLAGS) $(MMS$SOURCE), -
	 $(LIB_ZIPCLI) /library, -
	 $(LIB_ZIP) /library, -
	 $(LIB_BZIP2_OPTS) -
	 $(LIB_ZIP) /library, -
	 $(LIB_ZLIB_OPTS) -
	 $(LFLAGS_ARCH) -
	 $(OPT_ID) /options -
	 $(NOSYSSHR_OPTS)


# Utility executables.

[.$(DEST)]ZIPCLOAK.EXE : [.$(DEST)]ZIPCLOAK.OBJ \
                          $(LIB_ZIPUTILS) $(LIB_BZ2_DEP) \
                          $(OPT_FILE) $(OPT_ID)
	$(LINK) $(LINKFLAGS) $(MMS$SOURCE), -
	 $(LIB_ZIPUTILS) /library, -
	 $(LIB_ZLIB_OPTS) -
	 $(LFLAGS_ARCH) -
	 $(OPT_ID) /options -
	 $(NOSYSSHR_OPTS)

[.$(DEST)]ZIPNOTE.EXE : [.$(DEST)]ZIPNOTE.OBJ \
                        $(LIB_ZIPUTILS) $(LIB_BZ2_DEP) \
                        $(OPT_FILE) $(OPT_ID)
	$(LINK) $(LINKFLAGS) $(MMS$SOURCE), -
	 $(LIB_ZIPUTILS) /library, -
	 $(LFLAGS_ARCH) -
	 $(OPT_ID) /options -
	 $(NOSYSSHR_OPTS)

[.$(DEST)]ZIPSPLIT.EXE : [.$(DEST)]ZIPSPLIT.OBJ \
                         $(LIB_ZIPUTILS) $(LIB_BZ2_DEP) \
                         $(OPT_FILE) $(OPT_ID)
	$(LINK) $(LINKFLAGS) $(MMS$SOURCE), -
	 $(LIB_ZIPUTILS) /library, -
	 $(LFLAGS_ARCH) -
	 $(OPT_ID) /options -
	 $(NOSYSSHR_OPTS)

# Help library source files.

ZIP.HLP : [.VMS]VMS_ZIP.RNH
	runoff /output = $(MMS$TARGET) $(MMS$SOURCE)

ZIP_CLI.HLP : [.VMS]ZIP_CLI.HELP [.VMS]CVTHELP.TPU
	edit := edit
	edit /tpu /nosection /nodisplay /command = [.VMS]CVTHELP.TPU -
	 $(MMS$SOURCE)
	rename /noconfirm ZIP_CLI.RNH; [.VMS];
	purge /noconfirm /nolog /keep = 1 [.VMS]ZIP_CLI.RNH
	runoff /output = $(MMS$TARGET) [.VMS]ZIP_CLI.RNH

# Help output text files.

.HLP.HTX :
	help_temp_name = "help_temp_"+ f$getjpi( 0, "PID")
	if (f$search( help_temp_name+ ".HLB") .nes. "") then -
         delete /noconfirm 'help_temp_name'.HLB;*
	library /create /help 'help_temp_name'.HLB $(MMS$SOURCE)
	help /library = sys$disk:[]'help_temp_name'.HLB -
         /output = 'help_temp_name'.OUT zip...
	delete /noconfirm 'help_temp_name'.HLB;*
	create /fdl = [.VMS]STREAM_LF.FDL $(MMS$TARGET)
	open /append help_temp $(MMS$TARGET)
	copy 'help_temp_name'.OUT help_temp
	close help_temp
	delete /noconfirm 'help_temp_name'.OUT;*

ZIP.HTX : ZIP.HLP [.VMS]STREAM_LF.FDL

ZIP_CLI.HTX : ZIP_CLI.HLP [.VMS]STREAM_LF.FDL

# Message file.

$(ZIP_MSG_EXE) : $(ZIP_MSG_OBJ)
	link /shareable = $(MMS$TARGET) $(ZIP_MSG_OBJ)

$(ZIP_MSG_OBJ) : $(ZIP_MSG_MSG)
	message /object = $(MMS$TARGET) /nosymbols $(ZIP_MSG_MSG)

$(ZIP_MSG_MSG) : ZIPERR.H [.VMS]STREAM_LF.FDL [.VMS]VMS_MSG_GEN.C
	$(CC) /include = [] /object = [.$(DEST)]VMS_MSG_GEN.OBJ -
	 [.VMS]VMS_MSG_GEN.C
	$(LINK) /executable = [.$(DEST)]VMS_MSG_GEN.EXE -
	 $(LFLAGS_ARCH) -
	 [.$(DEST)]VMS_MSG_GEN.OBJ
	create /fdl = [.VMS]STREAM_LF.FDL $(MMS$TARGET)
	define /user_mode sys$output $(MMS$TARGET)
	run [.$(DEST)]VMS_MSG_GEN.EXE
	purge $(MMS$TARGET)
	delete  /noconfirm [.$(DEST)]VMS_MSG_GEN.EXE;*, -
	 [.$(DEST)]VMS_MSG_GEN.OBJ;*

# Library link options file.

.IFDEF LIBZIP                   # LIBZIP
$(LIBZIP_OPT) : [.VMS]STREAM_LF.FDL
	def_dev_dir_orig = f$environment( "default")
	set default [.$(DEST)]
	def_dev_dir = f$environment( "default")
	set default 'def_dev_dir_orig'
	create /fdl = [.VMS]STREAM_LF.FDL $(MMS$TARGET)
	open /append opt_file_lib $(MMS$TARGET)
	write opt_file_lib "! DEFINE LIB_IZZIP ''def_dev_dir'"
	if ("$(LIB_BZIP2_OPTS)" .nes. "") then -
         write opt_file_lib "! DEFINE LIB_BZIP2 ''f$trnlnm( "lib_bzip2")'"
	if ("$(LIB_ZLIB_OPTS)" .nes. "") then -
         write opt_file_lib "! DEFINE LIB_ZLIB ''f$trnlnm( "lib_zlib")'"
	write opt_file_lib "LIB_IZZIP:$(LIB_ZIP_NAME) /library"
	if ("$(LIB_BZIP2_OPTS)" .nes. "") then -
         write opt_file_lib "$(LIB_BZIP2_OPTS)" - ","
	write opt_file_lib "LIB_IZZIP:$(LIB_ZIP_NAME) /library"
	if ("$(LIB_ZLIB_OPTS)" .nes. "") then -
         write opt_file_lib "$(LIB_ZLIB_OPTS)" - ","
	close opt_file_lib
.ENDIF                          # LIBZIP

# Include generated source dependencies.

INCL_DESCRIP_DEPS = 1
.INCLUDE [.VMS]DESCRIP_DEPS.MMS

