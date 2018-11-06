#                                               29 November 2010.  SMS.
#
#    BZIP2 1.0 for VMS - MMS Dependency Description File.
#
#    MMS /EXTENDED_SYNTAX description file to generate a C source
#    dependencies file.  Unsightly errors result when /EXTENDED_SYNTAX
#    is not specified.  Typical usage:
#
#    $ MMS /EXTEND /DESCRIP = [.vms]descrip_mkdeps.mms /SKIP
#
#    which discards individual source dependency files, or:
#
#    $ MMS /EXTEND /DESCRIP = [.vms]descrip_mkdeps.mms /MACRO = NOSKIP=1
#
#    which retains them.  Retaining them can save time when doing code
#    development.
#
#
# The default target is the comprehensive source dependency file,
# $(DEPS_FILE) = "DESCRIP_DEPS.MMS".
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
# This description file uses these command procedures:
#
#    [.vms]collect_deps.com
#    [.vms]mod_dep.com
#
# Note:  This dependency generation scheme assumes that the dependencies
# do not depend on host architecture type or other such variables. 
# Therefore, no "#include" directive in the C source itself should be
# conditional on such variables.
#

# Required command procedures.

COMS = [.vms]collect_deps.com [.vms]mod_dep.com

# Include the source file lists (among other data).

INCL_DESCRIP_SRC = 1
.INCLUDE [.vms]descrip_src.mms

# The ultimate product, a comprehensive dependency list.

DEPS_FILE = [.vms]descrip_deps.mms

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
	$(CC) $(CFLAGS_INCL) $(MMS$SOURCE) /NOLIST /NOOBJECT -
	 /MMS_DEPENDENCIES = (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)

# List of MMS dependency files.

# In case it's not obvious...
# To extract module name lists from object library module=object lists:
# 1.  Transform "module=[.dest]name.OBJ" into "module=[.dest] name".
# 2.  For [.vms], add [.vms] to name.
# 3.  Delete "*]" words.
#
# A similar scheme works for executable lists.

MODS_LIB_BZ2_N = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] *, $(MODS_OBJS_LIB_BZ2_N)))

MODS_LIB_BZ2_V = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] [.vms]*, $(MODS_OBJS_LIB_BZ2_V)))

MODS_LIB_BZ2_NS_N = $(FILTER-OUT *], \
 $(PATSUBST *]*.OBJ, *] *, $(MODS_OBJS_LIB_BZ2_NS_N)))

MODS_BZIP2 = $(FILTER-OUT *], \
 $(PATSUBST *]*.EXE, *] *, $(BZIP2_EXE)))

MODS_BZIPUTILS = $(FILTER-OUT *], \
 $(PATSUBST *]*.EXE, *] *, $(BZIPUTILS)))

# Complete list of C object dependency file names.

DEPS = $(FOREACH NAME, \
 $(MODS_LIB_BZ2_N) $(MODS_LIB_BZ2_V) $(MODS_LIB_BZ2_NS_N) \
 $(MODS_BZIP2) $(MODS_BZIPUTILS), \
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
#       Note that the space in P3, which prevents immediate macro
#       expansion, is removed by collect_deps.com.
#
        @[.vms]collect_deps.com "BZIP2 for VMS" "$(MMS$TARGET)" -
	 "[...]*.MMSD" "[.$ (DEST)]" $(MMSDESCRIPTION_FILE) -
	 "[." $(DEST)
        @ write sys$output -
         "Created a new dependency file: $(MMS$TARGET)"
.IF DELETE_MMSD
	@ write sys$output -
         "Deleting intermediate .MMSD files..."
	delete /log *.MMSD;*, [.vms]*.MMSD;*
.ELSE
	@ write sys$output -
         "Purging intermediate .MMSD files..."
	purge /log *.MMSD, [.vms]*.MMSD
.ENDIF

# CLEAN target.  Delete the individual C dependency files.

CLEAN :
	if (f$search( "[...]*.MMSD") .nes. "") then -
	 delete [...]*.MMSD;*

# CLEAN_ALL target.  Delete:
#    The individual C dependency files.
#    The collected source dependency file.

CLEAN_ALL :
	if (f$search( "[...]*.MMSD") .nes. "") then -
	 delete [...]*.MMSD;*
	if (f$search( "$(DEPS_FILE);*") .nes. "") then -
	 delete /log $(DEPS_FILE);*

# PURGE target.

PURGE :
	if (f$search( "*.MMSD;*") .nes. "") then -
	 purge /log *.MMSD;*
	if (f$search( "[.vms]*.MMSD;*") .nes. "") then -
	 purge /log [.vms]*.MMSD;*
	if (f$search( "$(DEPS_FILE);*") .nes. "") then -
	 purge /log $(DEPS_FILE);*

# Explicit dependencies and rules for utility variant modules.
#
# The extra dependency on the normal dependency file obviates including
# the /SKIP warning code in each rule here.

BLOCKSORT_.MMSD : BLOCKSORT.C BLOCKSORT.MMSD
	$(CC) $(CFLAGS_INCL) /define = BZ_NO_STDIO=1 $(MMS$SOURCE) -
	 /NOLIST /NOOBJECT -
	 /MMS_DEPENDENCIES = (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@[.vms]mod_dep.com $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

BZLIB_.MMSD : BZLIB.C BZLIB.MMSD
	$(CC) $(CFLAGS_INCL) /define = BZ_NO_STDIO=1 $(MMS$SOURCE) -
	 /NOLIST /NOOBJECT -
	 /MMS_DEPENDENCIES = (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@[.vms]mod_dep.com $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

COMPRESS_.MMSD : COMPRESS.C COMPRESS.MMSD
	$(CC) $(CFLAGS_INCL) /define = BZ_NO_STDIO=1 $(MMS$SOURCE) -
	 /NOLIST /NOOBJECT -
	 /MMS_DEPENDENCIES = (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@[.vms]mod_dep.com $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

CRCTABLE_.MMSD : CRCTABLE.C CRCTABLE.MMSD
	$(CC) $(CFLAGS_INCL) /define = BZ_NO_STDIO=1 $(MMS$SOURCE) -
	 /NOLIST /NOOBJECT -
	 /MMS_DEPENDENCIES = (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@[.vms]mod_dep.com $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

DECOMPRESS_.MMSD : DECOMPRESS.C DECOMPRESS.MMSD
	$(CC) $(CFLAGS_INCL) /define = BZ_NO_STDIO=1 $(MMS$SOURCE) -
	 /NOLIST /NOOBJECT -
	 /MMS_DEPENDENCIES = (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@[.vms]mod_dep.com $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

HUFFMAN_.MMSD : HUFFMAN.C HUFFMAN.MMSD
	$(CC) $(CFLAGS_INCL) /define = BZ_NO_STDIO=1 $(MMS$SOURCE) -
	 /NOLIST /NOOBJECT -
	 /MMS_DEPENDENCIES = (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@[.vms]mod_dep.com $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

RANDTABLE_.MMSD : RANDTABLE.C RANDTABLE.MMSD
	$(CC) $(CFLAGS_INCL) /define = BZ_NO_STDIO=1 $(MMS$SOURCE) -
	 /NOLIST /NOOBJECT -
	 /MMS_DEPENDENCIES = (FILE = $(MMS$TARGET), NOSYSTEM_INCLUDE_FILES)
	@[.vms]mod_dep.com $(MMS$TARGET) $(MMS$TARGET_NAME).OBJ $(MMS$TARGET)

