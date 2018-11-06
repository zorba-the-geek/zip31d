#==============================================================================
# unix/Makefile
# - For Zip, ZipCloak, ZipNote, ZipSplit, and Zip library   Revised: 2015-09-02
#
# Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.
#
# See the accompanying file LICENSE, version 2009-Jan-2 or later (the
# contents of which are also included in zip.h) for terms of use.  If,
# for some reason, all these files are missing, the Info-ZIP license may
# also be found at: ftp://ftp.info-zip.org/pub/infozip/license.html
#==============================================================================

# INSTRUCTIONS:
#
# "make -f unix/Makefile generic"         Make Zip on most systems using "cc".
#
# "make -f unix/Makefile generic CC=gcc"  Make Zip on most systems using "gcc".
#
# "make -f unix/Makefile list"            List all supported targets.
#
# "make -f unix/Makefile help"            Limited advice on which targets to
#                                         try if problems occur.
#
# CF are flags for the C compiler.  LF are flags for the linker.  LF2
# are more flags for the linker, if they need to be at the end of the
# line instead of at the beginning (for example, some libraries).
# LOCAL_ZIP is a "make" macro that can be used to add default C flags to
# your compile without editing this makefile (e.g., -DDEBUG_STRUC, or
# -FPi87 on a PC using Microsoft C).
#
# The "generic" target invokes the "unix/configure" script to test the
# compile and other parts of the environment to see what's supported.
# The results are used to adjust the build procedure and to enable or
# disable some program features.  unix/configure builds a $(PROD)/flags
# file with appropriate values for a number of "make" macros, which is
# used by this makefile when performing the actual builds.  A separate
# flags file for building BZIP2 may also be created.  This makefile
# recursively invokes itself to perform the actual builds.  Once the
# flags files are created, they are used for later builds.  Use the
# "clean" target to clear these files when any changes are made.
#
# While oriented towards building Unix (and Unix work-alike) ports of
# Zip, this file and/or unix/configure are also used by other ports of
# Zip.  See below.
#
# If at all possible, use the generic targets.  The other (older)
# targets are less well tested, and may need considerable help from the
# user to get optimal results (or any success at all).
#
# The "docs" and "docsrof" targets generate the Zip documentation files.
# The manual sources are provided in the man/man1 directory and should be
# usable directly by the "man" command.  "docs" creates plain text versions
# of these manuals.  Zip ships with copies of these plain text manuals in
# the docs directory, so you should not need to run "docs".  "docsrof"
# generates the same plain text manuals using "nroff" instead of "man".
# Not all systems support both "docs" and "docsrof".
#
# -----------------------------------------------------------------------------
#
# Optional Zip features are controlled by setting "make" macros as
# described below or by the results of tests done by configure (such as
# checking if the needed files are in an appropriate directory).
#
# Optional features include (see INSTALL for installation instructions):
#
# AES encryption - See the files INSTALL and aes_wg/README_AES_WG.txt
#   for more information.  To include AES_WG (WinZip/Gladman) encryption
#   support, unpack a compatible AES_WG source kit (iz_aes_wg16.zip, or
#   the latest compatible kit available on the Info-ZIP server) to
#   populate the local ./aes_wg subdirectory.  To disable AES_WG
#   encryption support (when the source kit is present), set the "make"
#   macro NO_AES_WG to 1.  Due to government restrictions, we do not
#   include this source with Zip.
#
# LZMA compression - See the files INSTALL and szip/README_LZMA.txt for
#   more information.  To disable LZMA compression support, set the
#   "make" macro NO_LZMA to 1.
#
# PPMd compression - See the files INSTALL and szip/README_LZMA.txt for
#   more information.  To disable PPMd compression support, set the
#   "make" macro NO_PPMD to 1.
#
# Bzip2 compression - See the files INSTALL and bzip2/README_BZIP2.txt
#   for more information.  To disable bzip2 compression support, set the
#   "make" macro NO_IZ_BZIP2 to 1.
#
#   To include bzip2 compression support, there are three options:
#   - Let the Zip builders build a bzip2 object library from the
#     source kit in the bzip2 subdirectory.  This is the preferred
#     method and should be used to create a "standard" build.
#   - Use an existing build of the bzip2 library by setting the "make"
#     macro IZ_BZIP2 to a directory containing the bzip2 header file and
#     object (and/or shared) library files.
#   - Use the system bzip2 library.  To do this, ensure that the local
#     ./bzip2 subdirectory exists and does not contain the files
#     Makefile, libbz2.a, or bzlib.h.  It may be safer to use one of the
#     above options first, if possible, to avoid a possible problem with
#     error handling that may not be done as expected by the system
#     library.
#
# Zlib deflation - To use zlib to handle Deflate compression instead of
#   Zip's built-in Deflate code, there are two options:
#   - Use an existing build of the zlib support by setting the "make"
#     macro IZ_ZLIB to a directory containing suitable zlib header and
#     object/shared library files.
#   - Use the system zlib support by ensuring that the local ./zlib
#     subdirectory exists, and does not contain the files libz.a or
#     bzlib.h.  Set the "make" macro IZ_ZLIB to "zlib".
#
# Zip Library - Zip now can be built into a library archive (libizzip.a).
#   See below.  izzip_example.c is an example of using the library.  See
#   the comments in that file for more information.
#
#   We currently do not support a dynamic library, except in the Windows
#   environment (zip32_dll.dll).  More information on that can be found
#   in the windll directory.
#
# -----------------------------------------------------------------------------
#
# Zip Ports:
#
# This makefile and/or the unix/configure script support a number of
# non-Unix Zip ports, where the Zip program build is under Unix (or a
# Unix work-alike), but the Zip program execution may be on another
# system or execution environment.  The unix/configure script will be
# invoked from either unix/Makefile, or from a "make" file located in
# another port directory.
#
# The port may be explicitly identified by the PORT parameter, or
# implicitly detected by compiler configuration tests.
#
# Supported ports include:
#   msdos : DOS port built using the DJGPP (or Cygwin) cross-compiler
#   os2   : OS/2 port
#   qdos  : Sinclair Q-DOS port built using a gcc cross-compiler
#   zos   : z/OS MVS port built under z/OS UNIX
#   zvm   : z/VM CMS port built under z/VM OpenExtensions
#
# Be sure to test your new Zip (and ZipCloak, ZipNote, and ZipSplit).  A
# successful build does not guarantee a working or fully functional program.


###########
# TARGETS #
###########

# List of supported systems/targets in this version:

GENERIC_TARGETS = generic  generic_pkg  docs  docsrof  flags

MISC_TARGETS = clean  clean_bzip2  clean_docs  clean_exe  dashv  help  list

SYS_TARGETS = att6300nodir coherent cray_v3 cygwin lynx minix qnx qnxnto solaris


# What you can make ...
help:
	@echo ''
	@echo 'Make what?  You must specify which system to make Zip for.'
	@echo 'The "make" generic target choices are:'
	@echo ''
	@echo "  $(GENERIC_TARGETS)"
	@echo ''
	@echo 'Additional "make" targets (things you can do) include:'
	@echo ''
	@echo "  $(MISC_TARGETS)"
	@echo ''
	@echo 'For a list including specific targets for older systems:'
	@echo '   make -f unix/Makefile list'
	@echo ''
	@echo '"make" macros to control various optional features:'
	@echo '    "NO_AES_WG=1"     Disable AES_WG encryption.'
	@echo '    "BINS=[L][M][U]   Binaries build list:'
	@echo '                       L: object library (libizzip.a)'
	@echo '                       M: main program (Zip)'
	@echo '                       U: utility programs (ZipCloak, ZipNote,'
	@echo '                          and ZipSplit)'
	@echo '                       Default: MU.'
	@echo '    "CC=gcc"          Use "gcc" instead of "cc" for C compiler.'
	@echo '    "NO_ICONV=1"      Disable use of iconv.'
	@echo '    "IZ_BZIP2=bz2dir" Use bzip2 compression from dir "bz2dir".'
	@echo '    "NO_IZ_BZIP2=1"   Disable bzip2 compression.'
	@echo '    "IZ_ZLIB=zdir"    Use zlib from dir "zdir" for Deflate.'
	@echo '    "LIST=1"          Generate C compiler and linker listing.'
	@echo '    "LOCAL_ZIP=opts"  Add C compiler options, "opts".'
	@echo '    "NO_LZMA=1"       Disable LZMA compression.'
	@echo '    "NO_PPMD=1"       Disable PPMd compression.'
	@echo '    "PROD=subdir"     Build product files in "subdir", not ".".'
	@echo ''
	@echo 'The generic targets use unix/configure to test the target'
	@echo 'system and compiler to determine what capabilities are'
	@echo 'supported, then create the flags and flags_bz files to store'
	@echo 'these settings.  These settings are then used to build Zip.'
	@echo 'When possible, use the generic targets as they should'
	@echo 'automatically build Zip for the target system.'
	@echo ''
	@echo 'Default is to include all compression and encryption that source'
	@echo 'is available for.  (bzip2, LZMA, and PPMd now included in Zip'
	@echo 'source kit.  Need separate Info-ZIP AES WG kit to add AES'
	@echo 'encryption, available on our site.)  See above to disable'
	@echo 'these.  Macro names are case sensitive.'
	@echo ''
	@echo 'Examples:'
	@echo '   Make Zip programs (zip, zipnote, ...) using cc:'
	@echo '       make -f unix/Makefile generic'
	@echo '   Make Zip programs using gcc, but exclude AES encryption:'
	@echo '       make -f unix/Makefile generic CC=gcc NO_AES_WG=1'
	@echo '   Make the Zip static library (libizzip.a)'
	@echo '       make -f unix/Makefile generic BINS=L'
	@echo ''
	@echo 'Note that many of these optional "make" macros direct the'
	@echo '"unix/configure" script, and so may work only with the'
	@echo '"generic" target.'
	@echo ''
	@echo 'First, try "make -f unix/Makefile generic" (to use "cc")'
	@echo '(or "make -f unix/Makefile generic CC=gcc" (to use "gcc")),'
	@echo 'as "generic" should automatically set the proper flags.'
	@echo 'If you do not have a preference, start with'
	@echo '    "make -f unix/Makefile generic".'
	@echo '(On many systems cc defaults to gcc.)'
	@echo ''
	@echo 'Use the "clean" target to delete any intermediate and product'
	@echo 'files previously generated using one of the other "make"'
	@echo 'targets.  If PROD= was used, you must use it on the clean'
	@echo 'command also to clean those files.'
	@echo ''
	@echo 'If needed, use "flags" target to rebuild the system-specific'
	@echo 'flags used by Makefile.  However, you probably want to run'
	@echo '"clean" first.  As generic targets automatically make flags,'
	@echo 'you should not need to make flags directly.  For debugging'
	@echo 'purposes, the "config_flags" target can be used to generate'
	@echo 'the "flags" file only.'
	@echo ''
	@echo 'If needed, use "docs" target to make the plain text manuals.'
	@echo 'These are the files, like zip.txt, that should already be in'
	@echo 'the docs directory.  To recreate these files use the "docs"'
	@echo 'target, which uses the "man" command.  User generated plain'
	@echo 'text manuals go in $(PROD)/manout.'
	@echo ''
	@echo 'To make the plain text docs using nroff instead of man, use'
	@echo 'the "docsrof" target.  The output of "docs" and "docsrof"'
	@echo 'should be the same, but systems that have problems with "docs"'
	@echo 'may have better luck with "docsrof".  But try "docs" first.'
	@echo '"docsrof" creates plain text files with .txtr extensions,'
	@echo 'which should be changed to .txt if these are used.'
	@echo ''
	@echo 'Always do "make -f unix/Makefile clean clean_bzip2" before'
	@echo 'attempting to build Zip using a different target.'
	@echo ''
	@echo 'After Zip has been built, you can use "dashv" target to get a'
	@echo '"zip -v" (version information) report from the created'
	@echo 'executable.  This will show what capabilities were included in'
	@echo 'Zip.'
	@echo ''
	@echo 'See the file INSTALL and the comments in unix/Makefile for more'
	@echo 'on building Zip.  See docs/zip.txt or man/man1/zip.1 for more'
	@echo 'on using Zip, and the related man pages for the utilities.'
	@echo ''

# list
# - Display list of supported systems/targets.
# - The SYSTEM variable is no longer supported.  Instead, use:
#       make -f unix/Makefile <target>
list:
	@echo ''
	@echo 'Type "make -f unix/Makefile <target>", where <target> is one of'
	@echo 'the following:'
	@echo ''
	@echo 'Generic targets:'
	@echo "  $(GENERIC_TARGETS)"
	@echo ''
	@echo 'Specific targets:'
	@echo "  $(SYS_TARGETS)"
	@echo ''
	@echo 'Miscellaneous targets:'
	@echo "  $(MISC_TARGETS)"
	@echo ''
	@echo 'Use one of the generic targets if possible, as the specific'
	@echo 'targets tend to be woefully out of date.'
	@echo ''
	@echo 'For further (very useful) information, please read the comments'
	@echo 'in unix/Makefile.'
	@echo ''


#####################
# MACRO DEFINITIONS #
#####################

#----------------------------------------------------------------------------
# For most systems:
# - The generic target invokes unix/configure to build $(PROD)/flags and
#   $(PROD)/flags_bz containing system derived settings that are used by
#   this makefile.
# - Use "make" macro LOCAL_ZIP to add C compiler flags (such as
#   -DDOSWILD).
#----------------------------------------------------------------------------

# This file name as "make" parameter, for recursion.
MAKEF = '-f unix/Makefile'

# Precompiled docs (such as plain text man pages) ship in "./docs"

# Product file directory default is "."
# PROD must be set as a location in the Zip source tree
# (. = top source tree directory).  For example, PROD = "temp"
# puts the build files in zip/temp.  The parent directory must
# already exist.
PROD = .

# Build Dependencies for "make"
# - Optional object library names
#    AES (WinZip/Gladman) encryption
#    BZIP2 compression
#    LZMA compression
#    PPMd compression
#    ZLIB compression
LIB_AES_WG  =
LIB_BZ      =
LIB_LZMA    =
LIB_PPMD    =
LIB_Z       =

# Binaries build list (executables and/or object library).
# - Default: main program and utilities.
BINS = MU

# Build "make" files
#    BZIP2 compression
MK_BZ   = Makefile

# OS-dependent files
# - Default:  Unix port.
# - Compilation and execution are both for Unix (or a Unix-like) environment.
#   The files in the Zip root and unix directories are used.
# - The vast majority of you out there will likely use this
#   configuration, and do not have to ever worry about this.
# - Where required by a specific Zip port, a generic target (in this
#   "make" file or a "make" file located in another port directory) uses
#   the PORT macro to set the correct OS dependent files via
#   unix/configure.  Or you can (very carefully) override as required.
# - In same order as variables directly below.
#    C header files
#    C object files for utilities (zipcloak, zipnote, zipsplit)
#    C object files for zip
#    Assembler object files for utilities
#    Assembler object files for zip
OSDEP_H  = unix/osdep.h
OSDEP_OCU = $(PROD)/unix.o
OSDEP_OCZ = $(PROD)/unix.o
OSDEP_OSU =
OSDEP_OSZ =

# OS-dependent file extensions
# - This default may be customized by unix/configure based on PORT
#    Executable programs
#    DLL (.so) (currently not used)
#    man page (section) suffix
PGMEXT =
DLLEXT = .so
MANSUFX =

# OS-dependent installation directories: executables and man pages
# - This default may be customized by unix/configure based on PORT
PREFIX = /usr/local
BINDIR = $(PREFIX)/bin
MANDIR = $(PREFIX)/man

MANDIR1 = $(MANDIR)/man1

# OS-dependent installation file permissions: executables and man pages
# - This default may be customized by unix/configure based on PORT
BINPERMS = 755
MANPERMS = 644

# C compiler and flags: core and bzip2
# - CC and LD must match, else "unresolved symbol: ___main" is possible.
# - To use "gcc", specify "make" macro, "CC=gcc", instead of editing here.
CC      = cc
CC_BZ   = $(CC)

CF_NOOPT = -I. -DUNIX $(LOCAL_ZIP)
CFLAGS  = -O
CF      = $(CFLAGS) $(CF_NOOPT)         # C compiler flags (core components)
CF_BZ   = $(CF)                         # C compiler flags (BZIP2 components)

# C preprocessor
CPP     = $(CC) -E

# Assembler and assembler flags
AS      = as
AF      = $(LOCAL_ZIP)

# Linker and linker flags (early and late)
# - CC and LD must match, else "unresolved symbol: ___main" is possible.
# - To use "gcc", specify "make" macro, "CC=gcc", instead of editing here.
LD      = $(CC)
LF1     =
LF2     = -s

# Other utility programs
AR      = ar cq
CHMOD   = chmod
COL     = col
LN      = ln
RANLIB  = ranlib
RM      = rm -f
SHELL   = /bin/sh
STRIP   = strip

# Default for dependency on unix/configure result is an empty symbol
# so that the static non-unix/configure targets continue to work
ACONF_DEP =


#----------------------------------------------------------------------------
#  Build configuration:  header files
#----------------------------------------------------------------------------

# OSDEP_H is port-dependent and can be updated by unix/configure

# C header files: zip
H_ZIP    = zip.h            \
           control.h        \
           tailor.h         \
           ziperr.h         \
           $(OSDEP_H)       \
           $(ACONF_DEP)

# C header files: AES (WinZip/Gladman) encryption
H_AES    = aes_wg/aes.h     \
           aes_wg/aesopt.h  \
           aes_wg/fileenc.h \
           aes_wg/hmac.h    \
           aes_wg/prng.h    \
           aes_wg/pwd2key.h \
           aes_wg/sha1.h

# C header files: LZMA compression
H_LZMA   = szip/SzVersion.h \
           szip/LzFind.h    \
           szip/LzHash.h    \
           szip/LzmaEnc.h   \
           szip/Types.h

# C header files: PPMd compression
H_PPMD   = szip/CpuArch.h   \
           szip/Ppmd.h      \
           szip/Ppmd8.h     \
           szip/Types.h


#----------------------------------------------------------------------------
#  Build configuration:  object files
#----------------------------------------------------------------------------

# File names ending in "_" are for the utilities.
# File names ending in "_l" are for the library.

# OSDEP_* are port-dependent and can be updated by unix/configure.

# object files: zip base files
O_ZIPL   = $(PROD)/crc32.o      \
           $(PROD)/crypt.o      \
           $(PROD)/deflate.o    \
           $(PROD)/globals.o    \
           $(PROD)/trees.o      \
           $(PROD)/ttyio.o      \
           $(PROD)/util.o       \
           $(PROD)/zbz2err.o    \
           $(PROD)/zipfile.o    \
           $(PROD)/zipup.o      \
           $(OSDEP_OCZ)         \
           $(OSDEP_OSZ)

# object files: zip application
O_ZIP    = $(PROD)/zip.o        \
           $(PROD)/fileio.o        \
           $(O_ZIPL)

# object files: zip library
O_ZIPN   = $(PROD)/api.o        \
           $(PROD)/zip_l.o       \
           $(PROD)/fileio_l.o

# object files: zipcloak
O_CLOAK  = $(PROD)/zipcloak.o   \
           $(PROD)/crc32.o      \
           $(PROD)/crypt_.o     \
           $(PROD)/fileio_.o    \
           $(PROD)/globals.o    \
           $(PROD)/ttyio.o      \
           $(PROD)/util_.o      \
           $(PROD)/zipfile_.o   \
           $(OSDEP_OCU:.o=_.o)  \
           $(OSDEP_OSU)

# object files: zipnote
O_NOTE   = $(PROD)/zipnote.o    \
           $(PROD)/crc32.o      \
           $(PROD)/fileio_.o    \
           $(PROD)/globals.o    \
           $(PROD)/util_.o      \
           $(PROD)/zipfile_.o   \
           $(OSDEP_OCU:.o=_.o)  \
           $(OSDEP_OSU)

# object files: zipsplit
O_SPLIT  = $(PROD)/zipsplit.o   \
           $(PROD)/crc32.o      \
           $(PROD)/fileio_.o    \
           $(PROD)/globals.o    \
           $(PROD)/util_.o      \
           $(PROD)/zipfile_.o   \
           $(OSDEP_OCU:.o=_.o)  \
           $(OSDEP_OSU)

# object files: Assembler Match (Source: match.S)
# - All OS-dependent, by unix/configure based on PORT
O_MCH_I  = $(PROD)/mch_i86.o
O_MCH_G  = $(PROD)/mch_gcc.o
O_MCH_SV = $(PROD)/mch_sysv.o

# object files: Assembler CRC (Source: msdos/crc_i86.S)
# - All OS-dependent, by unix/configure based on PORT
O_CRC_I  = $(PROD)/crc_i86.o

# object files: Assembler CRC (Source: crc_i386.S)
# - All OS-dependent, by unix/configure based on PORT
O_CRC_G  = $(PROD)/crc_gcc.o
O_CRC_SV = $(PROD)/crc_sysv.o

# object files: AES (WinZip/Gladman) encryption
O_AES    = $(PROD)/aescrypt.o   \
           $(PROD)/aeskey.o     \
           $(PROD)/aestab.o     \
           $(PROD)/fileenc.o    \
           $(PROD)/hmac.o       \
           $(PROD)/prng.o       \
           $(PROD)/pwd2key.o    \
           $(PROD)/sha1.o

# object files: LZMA compression
O_LZMA   = $(PROD)/LzFind.o     \
           $(PROD)/LzmaEnc.o

# object files: PPMd compression
O_PPMD   = $(PROD)/Ppmd8.o      \
           $(PROD)/Ppmd8Enc.o

# object files: ZLIB compression
# - N/A.  ZLIB is not built as part of Zip.


#----------------------------------------------------------------------------
#  Build configuration:  object libraries (archives)
#----------------------------------------------------------------------------

# These names must match up with the names in configure.

# object library: AES (WinZip/Gladman) encryption
AR_AES   = $(PROD)/libiz_aes_wg.a

# object library: BZIP2 compression
AR_BZ    = bzip2/libbz2.a

# object library: LZMA compression
AR_LZMA  = $(PROD)/libiz_lzma.a

# object library: PPMd compression
AR_PPMD  = $(PROD)/libiz_ppmd.a

# object library: ZLIB compression
AR_Z     = zlib/libz.a

# -------------------------------

# object library: Callable Zip (this is the Zip static library to be created)
AR_ZIP   = $(PROD)/libizzip.a


#----------------------------------------------------------------------------
#  Build configuration:  Executable programs
#----------------------------------------------------------------------------
LIB_ZIP    = $(PROD)/libizzip.a

PGM_CLOAK  = zipcloak$(PGMEXT)
PGM_NOTE   = zipnote$(PGMEXT)
PGM_SPLIT  = zipsplit$(PGMEXT)
PGM_ZIP    = zip$(PGMEXT)

PPGM_CLOAK = $(PROD)/$(PGM_CLOAK)
PPGM_NOTE  = $(PROD)/$(PGM_NOTE)
PPGM_SPLIT = $(PROD)/$(PGM_SPLIT)
PPGM_ZIP   = $(PROD)/$(PGM_ZIP)

ZIP_PPGMS  = $(PPGM_CLOAK) \
             $(PPGM_NOTE)  \
             $(PPGM_SPLIT) \
             $(PPGM_ZIP)


#----------------------------------------------------------------------------
#  Build configuration:  Manuals
#----------------------------------------------------------------------------

MAN_CLOAK   = zipcloak.1$(MANSUFX)
MAN_NOTE    = zipnote.1$(MANSUFX)
MAN_SPLIT   = zipsplit.1$(MANSUFX)
MAN_ZIP     = zip.1$(MANSUFX)

# Plain text manuals we ship are in the ./docs directory.
# User generated plain text manuals go in the PROD/manout directory.
# Generated plain text manuals should be identical to the shipped versions.

# These are plain text files.  The "docs" target (DOC_) creates .txt files
# while the "docsrof" target (ROF_) creates .txtr files (nroff versions,
# which should be renamed .txt if these are used).

ROF_CLOAK   = $(PROD)/manout/zipcloak.txtr
ROF_NOTE    = $(PROD)/manout/zipnote.txtr
ROF_SPLIT   = $(PROD)/manout/zipsplit.txtr
ROF_ZIP     = $(PROD)/manout/zip.txtr

ZIP_ROFS    = $(ROF_CLOAK) \
              $(ROF_NOTE)  \
              $(ROF_SPLIT) \
              $(ROF_ZIP)

DOC_CLOAK   = $(PROD)/manout/zipcloak.txt
DOC_NOTE    = $(PROD)/manout/zipnote.txt
DOC_SPLIT   = $(PROD)/manout/zipsplit.txt
DOC_ZIP     = $(PROD)/manout/zip.txt

ZIP_DOCS    = $(DOC_CLOAK)  \
              $(DOC_NOTE)   \
              $(DOC_SPLIT)  \
              $(DOC_ZIP)

#----------------------------------------------------------------------------
#  Build configuration:  Installation
#----------------------------------------------------------------------------

# (Probably can change next two to "install" and "install -d" if you have it.)
INSTALL   = cp
INSTALL_PROGRAM = $(INSTALL)
INSTALL_D = mkdir -p

BIN_CLOAK = $(BINDIR)/$(PGM_CLOAK)
BIN_NOTE  = $(BINDIR)/$(PGM_NOTE)
BIN_SPLIT = $(BINDIR)/$(PGM_SPLIT)
BIN_ZIP   = $(BINDIR)/$(PGM_ZIP)

INSTALLEDBIN = $(BIN_CLOAK) \
               $(BIN_NOTE)  \
               $(BIN_SPLIT) \
               $(BIN_ZIP)

INSTALLEDMAN = $(MANDIR1)/$(MAN_CLOAK) \
               $(MANDIR1)/$(MAN_NOTE)  \
               $(MANDIR1)/$(MAN_SPLIT) \
               $(MANDIR1)/$(MAN_ZIP)

INSTALLEDMAN_GZ = $(MANDIR1)/$(MAN_CLOAK).gz \
                  $(MANDIR1)/$(MAN_NOTE).gz  \
                  $(MANDIR1)/$(MAN_SPLIT).gz \
                  $(MANDIR1)/$(MAN_ZIP).gz

# Solaris 2.x package stuff:
PKGDIR  = IZzip
VERSION = Version 3.1


###############################################
# BASIC COMPILE INSTRUCTIONS AND DEPENDENCIES #
###############################################

#----------------------------------------------------------------------------
#  Build rules: File suffixes
#  - Disable all built-in rules.
#  - Only explicit 1:1 rules are used.
#----------------------------------------------------------------------------
.SUFFIXES:

#----------------------------------------------------------------------------
#  Build rules: C compile (.c -> .o)
#  - Explicit 1:1 rules are used.
#  - Across various "make" utilities, pattern rule support is inconsistent.
#  - Sys V "make" may have problems with $< when input and output names differ.
#----------------------------------------------------------------------------

# C Compile rules: Zip

# A) Core Zip C files

$(PROD)/zip.o:       zip.c      $(H_ZIP) crc32.h crypt.h revision.h ttyio.h
	$(CC) -c $(CF) -o $@ zip.c

$(PROD)/crc32.o:     crc32.c    $(H_ZIP) crc32.h
	$(CC) -c $(CF) -o $@ crc32.c

$(PROD)/crypt.o:     crypt.c    $(H_ZIP) crc32.h crypt.h ttyio.h
	$(CC) -c $(CF) -o $@ crypt.c

$(PROD)/deflate.o:   deflate.c  $(H_ZIP)
	$(CC) -c $(CF) -o $@ deflate.c

$(PROD)/fileio.o:    fileio.c   $(H_ZIP) crc32.h
	$(CC) -c $(CF) -o $@ fileio.c

$(PROD)/globals.o:   globals.c  $(H_ZIP) $(H_CLOAK) $(H_NOTE) $(H_SPLIT)
	$(CC) -c $(CF) -o $@ globals.c

$(PROD)/trees.o:     trees.c    $(H_ZIP)
	$(CC) -c $(CF) -o $@ trees.c

$(PROD)/ttyio.o:     ttyio.c    $(H_ZIP) $(H_CLOAK) crypt.h ttyio.h
	$(CC) -c $(CF) -o $@ ttyio.c

$(PROD)/util.o:      util.c     $(H_ZIP)
	$(CC) -c $(CF) -o $@ util.c

$(PROD)/zbz2err.o:   zbz2err.c  $(H_ZIP)
	$(CC) -c $(CF) -o $@ zbz2err.c

$(PROD)/zipfile.o:   zipfile.c  $(H_ZIP) crc32.h
	$(CC) -c $(CF) -o $@ zipfile.c

$(PROD)/zipup.o:     zipup.c    $(H_ZIP) crc32.h crypt.h revision.h unix/zipup.h
	$(CC) -c $(CF) -o $@ zipup.c

# A') Callable Zip C files (Zip static library)

$(PROD)/api.o:       api.c      $(H_ZIP) api.h crc32.h crypt.h revision.h
	$(CC) -c $(CF) -DZIPLIB -o $@ api.c

$(PROD)/zip_l.o:      zip.c      $(H_ZIP) crc32.h crypt.h revision.h ttyio.h
	$(CC) -c $(CF) -DZIPLIB -o $@ zip.c

$(PROD)/fileio_l.o:   fileio.c   $(H_ZIP) crc32.h crypt.h revision.h ttyio.h
	$(CC) -c $(CF) -DZIPLIB -o $@ fileio.c

# B) C files for specific ports
#    - Files under msdos and os2 are used when cross compiling for those ports.
#    - Generated when referenced by OSDEP_OC*.
#    - OSDEP_OC* are customized by unix/configure, based on PORT macro.

$(PROD)/msdos.o: msdos/msdos.c $(H_ZIP)
	$(CC) -c $(CF) -o $@ msdos/msdos.c

$(PROD)/os2.o: os2/os2.c      $(H_ZIP)
	$(CC) -c $(CF) -o $@ os2/os2.c

$(PROD)/unix.o: unix/unix.c   $(H_ZIP)
	$(CC) -c $(CF) -o $@ unix/unix.c

$(PROD)/nt.o: win32/nt.c  $(H_ZIP)
	$(CC) -c $(CF) -o $@ win32/nt.c

$(PROD)/win32.o: win32/win32.c  $(H_ZIP)
	$(CC) -c $(CF) -o $@ win32/win32.c

$(PROD)/win32i64.o: win32/win32i64.c  $(H_ZIP)
	$(CC) -c $(CF) -o $@ win32/win32i64.c

$(PROD)/win32zip.o: win32/win32zip.c  $(H_ZIP)
	$(CC) -c $(CF) -o $@ win32/win32zip.c


#----------------------------------------------------------------------------

# C Compile rules: Zip utilities

# A) Core Zip utility C files

$(PROD)/zipcloak.o:  zipcloak.c $(H_ZIP) $(H_CLOAK) crc32.h crypt.h \
                     revision.h ttyio.h
	$(CC) -c $(CF) -DUTIL -o $@ zipcloak.c

$(PROD)/zipnote.o:   zipnote.c  $(H_ZIP) $(H_NOTE) revision.h
	$(CC) -c $(CF) -DUTIL -o $@ zipnote.c

$(PROD)/zipsplit.o:  zipsplit.c $(H_ZIP) $(H_SPLIT) revision.h
	$(CC) -c $(CF) -DUTIL -o $@ zipsplit.c

$(PROD)/crc32_.o:   crc32.c     $(H_ZIP) $(H_CLOAK) crc32.h
	$(CC) -c $(CF) -DUTIL -o $@ crc32.c

$(PROD)/crypt_.o:   crypt.c     $(H_ZIP) $(H_CLOAK) crc32.h crypt.h ttyio.h
	$(CC) -c $(CF) -DUTIL -o $@ crypt.c

$(PROD)/fileio_.o:  fileio.c    $(H_ZIP) $(H_CLOAK) $(H_NOTE) \
                                $(H_SPLIT) crc32.h
	$(CC) -c $(CF) -DUTIL -o $@ fileio.c

$(PROD)/util_.o:    util.c      $(H_ZIP) $(H_CLOAK) $(H_NOTE) $(H_SPLIT)
	$(CC) -c $(CF) -DUTIL -o $@ util.c

$(PROD)/zipfile_.o: zipfile.c   $(H_ZIP) $(H_CLOAK) $(H_NOTE) \
                                $(H_SPLIT) crc32.h
	$(CC) -c $(CF) -DUTIL -o $@ zipfile.c

# B) C files for specific ports
#    - Generated when referenced by OSDEP_OC.
#    - OSDEP_OC* are customized by unix/configure, based on PORT macro.

$(PROD)/msdos_.o: msdos/msdos.c $(H_ZIP) $(H_CLOAK) $(H_NOTE) $(H_SPLIT)
	$(CC) -c $(CF) -DUTIL -o $@ msdos/msdos.c

$(PROD)/os2_.o: os2/os2.c     $(H_ZIP) $(H_CLOAK) $(H_NOTE) $(H_SPLIT)
	$(CC) -c $(CF) -DUTIL -o $@ os2/os2.c

$(PROD)/unix_.o: unix/unix.c  $(H_ZIP) $(H_CLOAK) $(H_NOTE) $(H_SPLIT)
	$(CC) -c $(CF) -DUTIL -o $@ unix/unix.c

$(PROD)/win32_.o: win32/win32.c  $(H_ZIP)
	$(CC) -c $(CF) -DUTIL -o $@ win32/win32.c

$(PROD)/win32i64_.o: win32/win32i64.c  $(H_ZIP)
	$(CC) -c $(CF) -DUTIL -o $@ win32/win32i64.c


#----------------------------------------------------------------------------

# C Compile rules: AES (WinZip/Gladman) encryption

$(PROD)/aescrypt.o: aes_wg/aescrypt.c $(H_AES)
	$(CC) -c $(CF) -o $@ aes_wg/aescrypt.c

$(PROD)/aeskey.o:   aes_wg/aeskey.c   $(H_AES)
	$(CC) -c $(CF) -o $@ aes_wg/aeskey.c

$(PROD)/aestab.o:   aes_wg/aestab.c   $(H_AES)
	$(CC) -c $(CF) -o $@ aes_wg/aestab.c

$(PROD)/fileenc.o:  aes_wg/fileenc.c  $(H_AES)
	$(CC) -c $(CF) -o $@ aes_wg/fileenc.c

$(PROD)/hmac.o:     aes_wg/hmac.c     $(H_AES)
	$(CC) -c $(CF) -o $@ aes_wg/hmac.c

$(PROD)/prng.o:     aes_wg/prng.c     $(H_AES)
	$(CC) -c $(CF) -o $@ aes_wg/prng.c

$(PROD)/pwd2key.o:  aes_wg/pwd2key.c  $(H_AES)
	$(CC) -c $(CF) -o $@ aes_wg/pwd2key.c

$(PROD)/sha1.o:     aes_wg/sha1.c     $(H_AES)
	$(CC) -c $(CF) -o $@ aes_wg/sha1.c


#----------------------------------------------------------------------------

# C Compile rules: LZMA compression

$(PROD)/LzFind.o:   szip/LzFind.c     $(H_LZMA)
	$(CC) -c $(CF) -o $@ szip/LzFind.c

$(PROD)/LzmaEnc.o:  szip/LzmaEnc.c    $(H_LZMA)
	$(CC) -c $(CF) -o $@ szip/LzmaEnc.c


#----------------------------------------------------------------------------

# C Compile rules: PPMd compression

$(PROD)/Ppmd8.o:    szip/Ppmd8.c      $(H_PPMD)
	$(CC) -c $(CF) -o $@ szip/Ppmd8.c

$(PROD)/Ppmd8Enc.o: szip/Ppmd8Enc.c   $(H_PPMD)
	$(CC) -c $(CF) -o $@ szip/Ppmd8Enc.c


#----------------------------------------------------------------------------
#  Build rules: Assemble (.S -> .o)
#  - Optional assembler replacements for C routines.
#  - All are port-specific (for instance, msdos rule only used for msdos).
#    - Generated when referenced by OSDEP_OS*.
#    - OSDEP_OS* are customized by unix/configure, based on PORT macro.
#----------------------------------------------------------------------------

# Assembler rule: Match

# 1) Assembler rule: Match, 16-bit only, MSDOS
$(O_MCH_I):  msdos/match.asm
	$(AS) $(AF) -c -o $@ msdos/match.asm $(ASEOL)

# 2) Assembler rule: Match 32bit, GNU AS
$(O_MCH_G):  match.S $(ACONF_DEP)
	$(AS) $(AF) -c -o $@ match.S

# 3) Assembler rule: Match 32bit, SysV AS
$(O_MCH_SV): match.S $(ACONF_DEP)
	$(CPP) $(AF) match.S > $(PROD)/_matchs.s
	$(AS) -c -o $@ $(PROD)/_matchs.s
	$(RM) $(PROD)/_matchs.s


#----------------------------------------------------------------------------

# Assembler rule: CRC-32

# 1) Assembler rule: CRC, 16-bit only, MSDOS
$(O_CRC_I):  msdos/crc_i86.asm
	$(AS) $(AF) -c -o $@ msdos/crc_i86.asm $(ASEOL)

# 2) Assembler rule: CRC 32bit, GNU AS
$(O_CRC_G):  crc_i386.S $(ACONF_DEP)
	$(AS) $(AF) -c -o $@ crc_i386.S

# 3) Assembler rule: CRC 32bit, SysV AS
$(O_CRC_SV): crc_i386.S $(ACONF_DEP)
	$(CPP) $(AF) crc_i386.S > $(PROD)/_crc_i386s.s
	$(AS) -c -o $@ $(PROD)/_crc_i386s.s
	$(RM) $(PROD)/_crc_i386s.s


#----------------------------------------------------------------------------
#  Build rules: Object libraries (archives) (.o -> .a)
#----------------------------------------------------------------------------

# The "@echo ''" commands here put blank lines in the Makefile output after
# each library build, allowing the linking commands to be quickly found.

# Object library rule: AES (WinZip/Gladman) encryption

$(AR_AES): $(O_AES)
	@echo 'Building/updating AES (WinZip/Gladman) object library...'
	-$(RM) $@
	$(AR) $@ $(O_AES)
	-$(RANLIB) $@
	@echo ''


# Object library rule: LZMA compression

$(AR_LZMA): $(O_LZMA)
	@echo 'Building/updating LZMA object library...'
	-$(RM) $@
	$(AR) $@ $(O_LZMA)
	-$(RANLIB) $@
	@echo ''


# Object library rule: PPMd compression

$(AR_PPMD): $(O_PPMD)
	@echo 'Building/updating PPMd object library...'
	-$(RM) $@
	$(AR) $@ $(O_PPMD)
	-$(RANLIB) $@
	@echo ''


# Object library rule: BZIP2 compression

$(AR_BZ):
	@echo 'Building/updating BZIP2 object library...'
	@if [ -f $(PROD)/flags_bz ]; then \
          ( cd bzip2/; \
            echo "Using flags_bz overrides"; \
            eval $(MAKE) -f $(MK_BZ) libbz2.a ACONF_DEP=$(PROD)/flags_bz \
             `cat ../$(PROD)/flags_bz`; \
          ); \
        else \
          ( cd bzip2/; \
            eval $(MAKE) -f $(MK_BZ) CC="$(CC_BZ)" CFLAGS="$(CF_BZ)" \
             RANLIB="$(RANLIB)" RM="rm -f" libbz2.a; \
          ); \
        fi
	@echo ''


# Object library rule: ZLIB compression

$(AR_Z):
	@echo 'Building/updating ZLIB object library...'
	( cd zlib/; \
          eval $(MAKE) -f Makefile libz.a ACONF_DEP=$(PROD)/flags \
           `cat ../$(PROD)/flags`; \
        )
	@echo ''

# -------------------------------

# Object library rule: Callable Zip (create Zip static library)

$(AR_ZIP): $(O_ZIPL) $(O_ZIPN) $(LIB_AES_WG) $(LIB_LZMA) $(LIB_PPMD) $(LIB_BZ)
	@echo 'Building/updating Callable Zip object library...'
	sh unix/mlz.sh "$(AR_ZIP)" "$(O_ZIPL) $(O_ZIPN)" \
         $(LIB_AES_WG) $(LIB_LZMA) $(LIB_PPMD) $(LIB_BZ)
	-$(RANLIB) $@
	@echo ''


#----------------------------------------------------------------------------
#  Build rules: Link programs (.o + .a -> executables)
#
#  Note: Utility programs (ZipCloak, ZipNote, ZipSplit) don't all really
#  depend on all the optional libraries, $(LIB_*), but the rules do,
#  because $(LF2) may include "-l" options for all of them, and some
#  linkers complain/fail if the corresponding libraries can't be found.
#----------------------------------------------------------------------------

# Link rule: zip

$(PPGM_ZIP): $(O_ZIP) $(LIB_AES_WG) $(LIB_BZ) $(LIB_LZMA) \
              $(LIB_PPMD) $(LIB_Z)
	$(LD) -o $@ $(LF1) $(O_ZIP) $(LIB_AES_WG) $(LIB_LZMA) \
         $(LIB_PPMD) $(LIB_Z) $(LF2)
	@echo ''


# Link rule: zipcloak

$(PPGM_CLOAK): $(O_CLOAK) $(LIB_AES_WG) $(LIB_BZ) $(LIB_LZMA) \
                $(LIB_PPMD) $(LIB_Z)
	$(LD) -o $@ $(LF1) $(O_CLOAK) $(LIB_AES_WG) $(LIB_LZMA) \
         $(LIB_PPMD) $(LIB_Z) $(LF2)
	@echo ''


# Link rule: zipnote

$(PPGM_NOTE): $(O_NOTE)    $(LIB_AES_WG) $(LIB_BZ) $(LIB_LZMA) \
               $(LIB_PPMD) $(LIB_Z)
	$(LD) -o $@ $(LF1) $(O_NOTE) $(LIB_AES_WG) $(LIB_LZMA) \
         $(LIB_PPMD) $(LIB_Z) $(LF2)
	@echo ''


# Link rule: zipsplit

$(PPGM_SPLIT): $(O_SPLIT)    $(LIB_AES_WG) $(LIB_BZ) $(LIB_LZMA) \
                $(LIB_PPMD) $(LIB_Z)
	$(LD) -o $@ $(LF1) $(O_SPLIT) $(LIB_AES_WG) $(LIB_LZMA) \
         $(LIB_PPMD) $(LIB_Z) $(LF2)
	@echo ''


#----------------------------------------------------------------------------
#  Build rules: Documentation
#  - Explicit 1:1 rules are used.
#  - Across various "make" utilities, pattern rule support is inconsistent.
#  - Sys V "make" may have problems with $< when input and output names differ.
#----------------------------------------------------------------------------

# Zip documentation output directory rules
# - Creates $(PROD)/manout directory if unix/configure does not.

$(PROD)/manout:
	if [ ! -d "$(PROD)" ]; then mkdir "$(PROD)"; fi
	if [ ! -d "$@" ]; then mkdir "$@"; fi

# Zip plain text manuals generation rules
#
# SunOS 4.x quirks:
#    "man -M" requires an absolute path, hence "`pwd`/".
#    /bin/man does not support "-x".  Specify "COL=/usr/5bin/col".
#
# The trailing "@echo ''" here are needed to fix an issue on one platform
# where the expected newline was not generated.

$(DOC_ZIP):   man/man1/zip.1
	man -M ` pwd `/man zip    | $(COL) -bx | uniq | expand > $@
	@echo ''

$(DOC_CLOAK): man/man1/zipcloak.1
	man -M ` pwd `/man zipcloak | $(COL) -bx | uniq | expand > $@
	@echo ''

$(DOC_NOTE):  man/man1/zipnote.1
	man -M ` pwd `/man zipnote  | $(COL) -bx | uniq | expand > $@
	@echo ''

$(DOC_SPLIT): man/man1/zipsplit.1
	man -M ` pwd `/man zipsplit | $(COL) -bx | uniq | expand > $@
	@echo ''


# Zip alternate (ROF) documentation generation rules
# - Requires nroff, or GNU groff package.

$(ROF_ZIP):   man/man1/zip.1
	nroff -man man/man1/zip.1      | $(COL) -bx | uniq > $@

$(ROF_CLOAK): man/man1/zipcloak.1
	nroff -man man/man1/zipcloak.1 | $(COL) -bx | uniq > $@

$(ROF_NOTE):  man/man1/zipnote.1
	nroff -man man/man1/zipnote.1  | $(COL) -bx | uniq > $@

$(ROF_SPLIT): man/man1/zipsplit.1
	nroff -man man/man1/zipsplit.1 | $(COL) -bx | uniq > $@


#----------------------------------------------------------------------------
#  Build rules: Standard build targets
#----------------------------------------------------------------------------

docs:       $(PROD)/manout $(ZIP_DOCS)
docsrof:    $(PROD)/manout $(ZIP_ROFS)

# Incorrect use of BINS=all creates loop - this target is not needed.
#all:        generic_msg generic

# These, and the below, need to be specified using BIN= with generic so
# configure is called to set flags.  Do not use these as targets instead
# of generic.

zips:       $(ZIP_PPGMS)
lib:        $(LIB_ZIP)

# Binaries build list (BINS) options:
L:          $(LIB_ZIP)
LM:         $(LIB_ZIP) $(PPGM_ZIP)
LMU:        $(LIB_ZIP) $(ZIP_PPGMS)
LU:         $(LIB_ZIP) $(PPGM_CLOAK) $(PPGM_NOTE) $(PPGM_SPLIT)
M:          $(PPGM_ZIP)
MU:         $(ZIP_PPGMS)
U:          $(PPGM_CLOAK) $(PPGM_NOTE) $(PPGM_SPLIT)


#----------------------------------------------------------------------------
#  Build rules: Configuration flags
#  - Creates files containing system-dependent flag values.
#  - Generated by unix/configure based on build utility program,
#    platform and features selected.
#----------------------------------------------------------------------------

# Main flags

config_flags: $(PROD)/flags

$(PROD)/flags: unix/configure
	@echo "Running:  unix/configure"
	@sh unix/configure \
         "PROD='$(PROD)'" \
         "LIST='$(LIST)'" \
         "CC='$(CC)'" \
         "CFLAGS='$(LOCAL_ZIP)'" \
         "LFLAGS1='$(LFLAGS1)'" \
         "LFLAGS2='$(LFLAGS2)'" \
         "NO_AES_WG='$(NO_AES_WG)'" \
         "IZ_BZIP2='$(IZ_BZIP2)'" \
         "NO_IZ_BZIP2='$(NO_IZ_BZIP2)'" \
         "NO_LZMA='$(NO_LZMA)'" \
         "NO_PPMD='$(NO_PPMD)'" \
         "IZ_ZLIB='$(IZ_ZLIB)'"


# BZIP2 flags

config_flags_bz: $(PROD)/flags_bz

$(PROD)/flags_bz: $(PROD)/flags


# C macros (-Dxxxx, or edit a header file), and their effects:
#
# HAVE_DIRENT_H         - Use <dirent.h> instead of <sys/dir.h>.
# HAVE_NDIR_H           - Use <ndir.h> (unix/unix.c).
# HAVE_SYS_DIR_H        - Use <sys/dir.h>.
# HAVE_SYS_NDIR_H       - Use <sys/ndir.h>.
# NODIR                 - For 3B1, which has neither getdents() nor opendir().
# NO_BZIP2_SUPPORT      - Do not compile in bzip2 code even if available.
# NO_CONST              - Cannot handle ANSI "const" keyword.
# NO_LARGE_FILE_SUPPORT - Do not enable Large File support even if available.
# NO_PROTO              - Cannot handle ANSI function prototypes.
# NO_RMDIR              - Remove directories using a system("rmdir ...") call.
# NO_UNICODE_SUPPORT    - Do not enable Unicode support even if available.
# NO_ZIP64_SUPPORT      - Do not enable Zip64 archive support even if available.
# UTIL                  - Select routines for utilities (note, cloak, split).
# ZMEM                  - Include C language versions of memset(),
#                         memcpy(), and memcmp() (util.c).


#----------------------------------------------------------------------------
#  Build rules: Clean-up targets
#  - Used to remove old output files before performing a new build.
#  - Used when incompatible build options (or source changes) are required.
#----------------------------------------------------------------------------

clean:
	@if unix/unsafe_prod.sh "$(PROD)"; then \
          echo "Won't clean absolute (/x) or rising (../) PROD: $(PROD)"; \
        else \
          echo "rm -f \"$(PROD)\"/*.o \"$(PROD)\"/*.a $(ZIP_PPGMS)"; \
          rm -f "$(PROD)"/*.o "$(PROD)"/*.a $(ZIP_PPGMS); \
          echo "rm -f \"$(PROD)\"/flags \"$(PROD)\"/flags_bz"; \
          rm -f "$(PROD)"/flags "$(PROD)"/flags_bz; \
          echo "rm -f $(PROD)/manout"; \
          rm -rf "$(PROD)"/manout; \
        fi

#	Remove empty $(PROD) directory.
	if [ -d "$(PROD)" ]; then \
          ls -1 "${PROD}"/* > /dev/null 2>&1 || \
          rm -r "$(PROD)" ; \
        fi
	rm -rf $(PKGDIR)

clean_bzip2:
	@if [ -f bzip2/$(MK_BZ) ]; then \
          ( cd bzip2; make -f $(MK_BZ) clean ); \
        else \
          echo "Expected bzip2 \"make\" file not found: bzip2/$(MK_BZ)"; \
        fi;

clean_exe:
	rm -f $(ZIP_PPGMS)

clean_docs:
	rm -f $(ZIP_DOCS)
	rm -f $(ZIP_ROFS)


#----------------------------------------------------------------------------
#  Build rules: Installation-related
#----------------------------------------------------------------------------

install: install_bin install_man

install_gz: install_bin install_man_gz

install_bin:
	-$(INSTALL_D) $(BINDIR)
	$(INSTALL_PROGRAM) $(ZIP_PPGMS) $(BINDIR)
	$(CHMOD) $(BINPERMS) $(INSTALLEDBIN)

install_man:
	-$(INSTALL_D) $(MANDIR1)
	$(INSTALL) man/man1/zipcloak.1 $(MANDIR1)/$(MAN_CLOAK)
	$(INSTALL) man/man1/zipnote.1  $(MANDIR1)/$(MAN_NOTE)
	$(INSTALL) man/man1/zipsplit.1 $(MANDIR1)/$(MAN_SPLIT)
	$(INSTALL) man/man1/zip.1      $(MANDIR1)/$(MAN_ZIP)
	$(CHMOD) $(MANPERMS) $(INSTALLEDMAN)

install_man_gz: install_man
	gzip $(INSTALLEDMAN)
	$(CHMOD) $(MANPERMS) $(INSTALLEDMAN_GZ)

uninstall:
	$(RM) $(INSTALLEDBIN) $(INSTALLEDMAN) $(INSTALLEDMAN_GZ)


#----------------------------------------------------------------------------
#  Build rules: SVR4 Package generation Interface
#  - Originally tested under Sun Solaris 2.x (by JBush).
#  - Other SVr4s may be very similar, and could possibly use this.
#  Note:  Expects version info to be stored in VERSION macro variable.
#         See "README" under unix/Packaging.
#----------------------------------------------------------------------------

svr4package: $(BINS)
	@echo 'Creating SVR4 package for Unix ...'
	-@rm -rf $(PKGDIR) $(PKGDIR)_`uname -p`.pkg
	-@sed -e "s/.VERSION./$(VERSION)/g" \
          -e "s/.PSTAMP./$(LOGNAME)_`date | tr  ' ' '_'`/g" \
          -e "s/.ARCH./Solaris_`uname -rp | tr ' ' ','`/g" \
          ./unix/Packaging/pkginfo.in > ./unix/Packaging/pkginfo
	-@sed -e "s/.ARCH./`uname -p`/g" \
          unix/Packaging/preinstall.in > ./unix/Packaging/preinstall
	/usr/bin/pkgmk -d . -b . -r . -f unix/Packaging/prototype $(PKGDIR)
	/usr/bin/pkgtrans -o -s . $(PKGDIR)_`uname -p`.pkg $(PKGDIR)
	@echo ''
	@echo \
 "To install, copy $(PKGDIR)_`uname -p`.pkg to the target system, and"
	@echo \
 "issue the command (as root):  pkgadd -d $(PKGDIR)_`uname -p`.pkg"
	@echo ''


#----------------------------------------------------------------------------
#  Build rules: Distribution kit
#----------------------------------------------------------------------------

dist: $(DOC_ZIP)
	eval zip -r9 zip`sed -e '/VERSION/!d' -e 's/.*"\(.*\)".*/\1/' \
          -e 's/[.]//g' -e 's/ .*//g' -e q revision.h` *


#----------------------------------------------------------------------------
#  Build rules: Zip validation tests
#----------------------------------------------------------------------------

dashv:
	$(PROD)/zip -v


############################
# INDIVIDUAL MACHINE RULES #
############################

#----------------------------------------------------------------------------
#  Generic targets
#  - These invoke the unix/configure script to determine configuration.
#----------------------------------------------------------------------------

# generic_msg
# - Issue warning message before generic build

generic_msg:
	@echo ''
	@echo '  Attempting "make generic" now.  If this fails for some reason, type'
	@echo '  "make help" for suggestions.  If none of the targets work for you,'
	@echo '  contact us using the information in README so we can work the'
	@echo '  problems.'
	@echo ''


# generic
# - Generate flags file (if doesn't exist), then Zip modules.

generic: $(PROD)/flags
	eval $(MAKE) $(MAKEF) $(BINS) ACONF_DEP=$(PROD)/flags \
          `cat $(PROD)/flags`

# generic_pkg
# - Generate flags file (if doesn't exist), then Zip modules, then perform
#   SVR4 package-creation

generic_pkg: generic svr4package


#----------------------------------------------------------------------------
#  Old, non-"generic" targets
#  - For those systems which do not work with the "generic" targets.
#  - Can not assume that the "make" utility groks "$(MAKE)".
#----------------------------------------------------------------------------

#----------------------------------------------------------------------
#  "Autoconfig" group, aliases for the generic targets using configure:
#----------------------------------------------------------------------

# Solaris:  generic, plus generation of installable package.
solaris: generic_pkg

#----------------------------------------------------------------------
#  "Normal" group.
#----------------------------------------------------------------------

# AT&T 6300 PLUS

# - Do not know yet how to allocate 64K bytes:
att6300nodir:
	$(MAKE) $(MAKEF) $(BINS) LF1="-Ml -s" \
          CF="-DUNIX -I. -O -Ml -DNO_RMDIR -DDYN_ALLOC -DMEDIUM_MEM \
            -DWSIZE=16384 -DNO_STDLIB_H -DNO_STDDEF_H -DNO_RENAME \
            -DNO_MKTIME -DNO_SIZE_T -DNO_VOID -DNO_PROTO -DNO_DIR \
            -DNO_CONST -DHAVE_TERMIO_H" \
          "LF2="


# Coherent

# - AS definition not needed for gcc
coherent:
	$(MAKE) $(MAKEF) $(BINS) CF="-DUNIX -I. -O -DDIRENT -DASMV" \
          AS="as -gx" OBJA=match.o


# Cray Unicos 6.1, Standard C compiler 3.0
# - All routines except trees.c may be compiled with vector3
# - Internal compiler bug in 3.0.2.3 and earlier requires vector2 for trees.c

cray_v3:
	$(MAKE) $(MAKEF) $(BINS) CC="scc" \
          CF="-DUNIX -I. -O -h vector2 -h scalar3 -DHAVE_DIRENT_H"


# Cygwin

cygwin:
	$(MAKE) $(MAKEF) generic CC="gcc" CPP="gcc -E" PGMEXE=".exe"


# LynxOS

lynx:
	$(MAKE) $(MAKEF) generic CC=gcc CPP="gcc -E" CF="$(CF) \
          -DNO_UNDERLINE -DLynx -DLYNX LF2="$LF2 -lc_p"


# MINIX 1.5.10
# - With Bruce Evans 386 patches and gcc/GNU make

minix:
	$(MAKE) $(MAKEF) $(BINS) CC=gcc CF="-DUNIX -I. -O -DDIRENT -DMINIX"
	chmem =262144 zip


# QNX
# - /bin/sh is ksh and it doesn't grok the configure script properly,
#   generating a bad flags file.  [cjh]

qnx:
	$(MAKE) $(MAKEF) $(BINS) LN=ln CC=cc CF="-DUNIX -I. -O \
          -DHAVE_DIRENT_H -DHAVE_TERMIOS_H -DNO_MKTEMP"


# QNX/Neutrino
# - No native development tools, so cross-build environement is used.
# - Use gcc and set ARCH as follows:
#      "x86"    to produce x86
#      "ppcbe"             PowerPC (big-endian)
#      "ppcle"             PowerPC (little-endian)
#      "mipsbe"            MIPS    (big-endian)
#      "mipsle"            MIPS    (little-endian)

qnxnto:
	@if [ "$(ARCH)" = "" ] ; then \
          echo "You didn't set ARCH; I'll assume you meant ARCH=x86..." ; \
          echo '' ; \
          $(MAKE) $(MAKEF) $(BINS) LN=ln CC="qcc -Vgcc_ntox86" \
          CF="-g -DUNIX -I. -O -DHAVE_DIRENT_H -DHAVE_TERMIOS_H -DNO_MKTEMP" \
          LF2=-g ; \
        else \
          echo "Making Zip for $(ARCH)..." ; \
          echo '' ; \
          $(MAKE) $(MAKEF) $(BINS) LN=ln CC="qcc -Vgcc_nto$(ARCH)" \
          CF="-g -DUNIX -I. -O -DHAVE_DIRENT_H -DHAVE_TERMIOS_H -DNO_MKTEMP" \
          LF2=-g ; \
        fi

