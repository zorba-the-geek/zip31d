#                                               29 November 2010.  SMS.
#
#    BZIP2 1.0 for VMS - MMS (or MMK) Source Description File.
#

# This description file is included by other description files.  It is
# not intended to be used alone.  Verify proper inclusion.

.IFDEF INCL_DESCRIP_SRC
.ELSE
$$$$ THIS DESCRIPTION FILE IS NOT INTENDED TO BE USED THIS WAY.
.ENDIF


# Define MMK architecture macros when using MMS.

.IFDEF __MMK__                  # __MMK__
.ELSE                           # __MMK__
ALPHA_X_ALPHA = 1
IA64_X_IA64 = 1
VAX_X_VAX = 1
.IFDEF $(MMS$ARCH_NAME)_X_ALPHA     # $(MMS$ARCH_NAME)_X_ALPHA
__ALPHA__ = 1
.ENDIF                              # $(MMS$ARCH_NAME)_X_ALPHA
.IFDEF $(MMS$ARCH_NAME)_X_IA64      # $(MMS$ARCH_NAME)_X_IA64
__IA64__ = 1
.ENDIF                              # $(MMS$ARCH_NAME)_X_IA64
.IFDEF $(MMS$ARCH_NAME)_X_VAX       # $(MMS$ARCH_NAME)_X_VAX
__VAX__ = 1
.ENDIF                              # $(MMS$ARCH_NAME)_X_VAX
.ENDIF                          # __MMK__

# Analyze architecture-related and option macros.

.IFDEF __ALPHA__                # __ALPHA__
DESTM = ALPHA
.ELSE                           # __ALPHA__
.IFDEF __IA64__                     # __IA64__
DESTM = IA64
.ELSE                               # __IA64__
.IFDEF __VAX__                          # __VAX__
DESTM = VAX
.ELSE                                   # __VAX__
DESTM = UNK
UNK_DEST = 1
.ENDIF                                  # __VAX__
.ENDIF                              # __IA64__
.ENDIF                          # __ALPHA__

.IFDEF LARGE                    # LARGE
.IFDEF __VAX__                      # __VAX__
DESTL =
.ELSE                               # __VAX__
DESTL = L
.ENDIF                              # __VAX__
.ELSE                           # LARGE
DESTL =
.ENDIF                          # LARGE

DEST = $(DESTM)$(DESTL)

# Check for option problems.

.IFDEF __VAX__                  # __VAX__
.IFDEF LARGE                        # LARGE
LARGE_VAX = 1
.ENDIF                              # LARGE
.ENDIF                          # __VAX__

# Complain if warranted.  Otherwise, show destination directory.
# Make the destination directory, if necessary.
				
.IFDEF UNK_DEST                 # UNK_DEST
.FIRST
	@ write sys$output -
 "   Unknown system architecture."
.IFDEF __MMK__                      # __MMK__
	@ write sys$output -
 "   MMK on IA64?  Try adding ""/MACRO = __IA64__""."
.ELSE                               # __MMK__
	@ write sys$output -
 "   MMS too old?  Try adding ""/MACRO = MMS$ARCH_NAME=ALPHA"","
	@ write sys$output -
 "   or ""/MACRO = MMS$ARCH_NAME=IA64"", or ""/MACRO = MMS$ARCH_NAME=VAX"","
	@ write sys$output -
 "   as appropriate.  (Or try a newer version of MMS.)"
.ENDIF                              # __MMK__
	@ write sys$output ""
	I_WILL_DIE_NOW.  /$$$$INVALID$$$$
.ELSE                           # UNK_DEST
.IFDEF LARGE_VAX                    # LARGE_VAX
.FIRST
	@ write sys$output -
 "   Macro ""LARGE"" is invalid on VAX."
	@ write sys$output ""
	I_WILL_DIE_NOW.  /$$$$INVALID$$$$
.ELSE                               # LARGE_VAX
.FIRST
	@ write sys$output "   Destination: [.$(DEST)]"
	@ write sys$output ""
	if (f$search( "$(DEST).DIR;1") .eqs. "") then -
	 create /directory [.$(DEST)]
.ENDIF                              # LARGE_VAX
.ENDIF                          # UNK_DEST

# DBG options.

.IFDEF DBG                      # DBG
CFLAGS_DBG = /debug /nooptimize
LINKFLAGS_DBG = /debug /traceback
.ELSE                           # DBG
CFLAGS_DBG =
LINKFLAGS_DBG = /notraceback
.ENDIF                          # DBG

# Large-file options.

.IFDEF LARGE                    # LARGE
CDEFS_LARGE = , _LARGEFILE
.ELSE                           # LARGE
CDEFS_LARGE =
.ENDIF                          # LARGE

# C compiler defines.

.IFDEF LOCAL_BZIP
C_LOCAL_BZIP = , $(LOCAL_BZIP)
.ELSE
C_LOCAL_BZIP =
.ENDIF

CDEFS = VMS $(CDEFS_LARGE) $(C_LOCAL_BZIP)

# Other C compiler options.

CFLAGS_ARCH = /decc /prefix = (all)

CFLAGS_INCL = /include = ([], [.vms])

CFLAGS_NAMES = /names = as_is

# LIST options.

.IFDEF LIST                     # LIST
.IFDEF DECC                         # DECC
CFLAGS_LIST = /list = $*.LIS /show = (all, nomessages)
.ELSE                               # DECC
CFLAGS_LIST = /list = $*.LIS /show = (all)
.ENDIF                              # DECC
LINKFLAGS_LIST = /map = $*.MAP /cross_reference /full
.ELSE                           # LIST
CFLAGS_LIST =
LINKFLAGS_LIST =
.ENDIF                          # LIST

# Common CFLAGS and LINKFLAGS.

CFLAGS = \
 $(CFLAGS_ARCH) $(CFLAGS_DBG) $(CFLAGS_INCL) $(CFLAGS_NAMES) \
 $(CFLAGS_LIST) $(CCOPTS) \
 /object = $(MMS$TARGET)

CFLAGS_NS = \
 $(CFLAGS_ARCH) $(CFLAGS_DBG) $(CFLAGS_INCL) $(CFLAGS_NAMES) \
 $(CFLAGS_LIST) $(CCOPTS) \
 /object = $(MMS$TARGET)

LINKFLAGS = \
 $(LINKFLAGS_DBG) $(LINKFLAGS_LIST) $(LINKOPTS) \
 /executable = $(MMS$TARGET)

# Object library module=object lists.

#    Primary object library, [].

MODS_OBJS_LIB_BZ2_N = \
 blocksort=[.$(DEST)]BLOCKSORT.OBJ \
 bzlib=[.$(DEST)]BZLIB.OBJ \
 compress=[.$(DEST)]COMPRESS.OBJ \
 crctable=[.$(DEST)]CRCTABLE.OBJ \
 decompress=[.$(DEST)]DECOMPRESS.OBJ \
 huffman=[.$(DEST)]HUFFMAN.OBJ \
 randtable=[.$(DEST)]RANDTABLE.OBJ

#    Primary object library, [.vms].

MODS_OBJS_LIB_BZ2_V = \
 vms_misc=[.$(DEST)]VMS_MISC.OBJ

MODS_OBJS_LIB_BZ2 = $(MODS_OBJS_LIB_BZ2_N) $(MODS_OBJS_LIB_BZ2_V)

#    Variant (BZ_NO_STDIO) object library, [].

MODS_OBJS_LIB_BZ2_NS_N = \
 blocksort=[.$(DEST)]BLOCKSORT_.OBJ \
 bzlib=[.$(DEST)]BZLIB_.OBJ \
 compress=[.$(DEST)]COMPRESS_.OBJ \
 crctable=[.$(DEST)]CRCTABLE_.OBJ \
 decompress=[.$(DEST)]DECOMPRESS_.OBJ \
 huffman=[.$(DEST)]HUFFMAN_.OBJ \
 randtable=[.$(DEST)]RANDTABLE_.OBJ

MODS_OBJS_LIB_BZ2_NS = $(MODS_OBJS_LIB_BZ2_NS_N)


# Executables.

BZIP2_EXE = [.$(DEST)]BZIP2.EXE

BZIP2RECOVER_EXE = [.$(DEST)]BZIP2RECOVER.EXE

BZIPUTILS = $(BZIP2RECOVER_EXE)

# MY_MODULE_NAME source file.

MY_MODULE_NAME_H = [.vms]my_module_name.h

# DECC_VER executable and object.

DECC_VER_EXE = [.$(DEST)]DECC_VER.EXE

DECC_VER_OBJ = [.$(DEST)]DECC_VER.OBJ

