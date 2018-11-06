# Microsoft Developer Studio Project File - Name="zip32_lib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=zip32_lib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "zip32_lib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "zip32_lib.mak" CFG="zip32_lib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "zip32_lib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "zip32_lib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "zip32_lib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "zip32_lib___Win32_Debug"
# PROP BASE Intermediate_Dir "zip32_lib___Win32_Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "zip32_lib___Win32_Debug"
# PROP Intermediate_Dir "zip32_lib___Win32_Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /GX /Z7 /Od /I "..\..\.." /I "..\..\..\WINDLL" /I "..\..\..\BZIP2" /I "..\..\..\WIN32" /I "..\..\..\ZIP" /D "USE_ZIPMAIN" /D "_DEBUG" /D "BZIP2_SUPPORT" /D "BZ_NO_STDIO" /D "LZMA_SUPPORT" /D "PPMD_SUPPORT" /D "NDEBUG" /D "_WINDOWS" /D "WIN32" /D "NO_ASM" /D "ZIPLIB" /D "WINDLL" /D "MSDOS" /FR /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MTd /W3 /GX /Z7 /Od /I "..\..\.." /I "..\..\..\WINDLL" /I "..\..\..\BZIP2" /I "..\..\..\WIN32" /I "..\..\..\ZIP" /D "WINDLL" /D "MSDOS" /D "_WINDOWS" /D "WIN32" /D "ZIPLIB" /D "USE_ZIPMAIN" /D "_DEBUG" /D "NO_ASM" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\Debug\zip32_libd.lib"
# ADD LIB32 /nologo /out:"..\Debug\zip32_libd.lib"

!ELSEIF  "$(CFG)" == "zip32_lib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "zip32_lib___Win32_Release"
# PROP BASE Intermediate_Dir "zip32_lib___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "zip32_lib___Win32_Release"
# PROP Intermediate_Dir "zip32_lib___Win32_Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /I "..\..\.." /I "..\..\..\windll" /I "..\..\..\BZIP2" /I "..\..\..\WIN32" /D "LZMA_SUPPORT" /D "PPMD_SUPPORT" /D "ZIPLIB" /D "BZIP2_SUPPORT" /D "BZ_NO_STDIO" /D "NDEBUG" /D "_WINDOWS" /D "WIN32" /D "NO_ASM" /D "WINDLL" /D "MSDOS" /FR /FD /c
# SUBTRACT BASE CPP /YX
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\..\.." /I "..\..\..\windll" /I "..\..\..\BZIP2" /I "..\..\..\WIN32" /D "WIN32" /D "WINDLL" /D "MSDOS" /D "ZIPLIB" /D "NDEBUG" /D "_WINDOWS" /D "NO_ASM" /FR /FD /c
# SUBTRACT CPP /YX
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\Release\zip32_lib.lib"

!ENDIF 

# Begin Target

# Name "zip32_lib - Win32 Debug"
# Name "zip32_lib - Win32 Release"
# Begin Source File

SOURCE=..\..\..\api.c
# End Source File
# Begin Source File

SOURCE=..\..\..\api.h
# End Source File
# Begin Source File

SOURCE=..\..\..\bzip2\blocksort.c
# End Source File
# Begin Source File

SOURCE=..\..\..\bzip2\bzlib.c
# End Source File
# Begin Source File

SOURCE=..\..\..\bzip2\bzlib.h
# End Source File
# Begin Source File

SOURCE=..\..\..\bzip2\bzlib_private.h
# End Source File
# Begin Source File

SOURCE=..\..\..\bzip2\compress.c
# End Source File
# Begin Source File

SOURCE=..\..\..\control.h
# End Source File
# Begin Source File

SOURCE=..\..\..\crc32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\bzip2\crctable.c
# End Source File
# Begin Source File

SOURCE=..\..\..\crypt.c
# End Source File
# Begin Source File

SOURCE=..\..\..\bzip2\decompress.c
# End Source File
# Begin Source File

SOURCE=..\..\..\deflate.c
# End Source File
# Begin Source File

SOURCE=..\..\..\fileio.c
# End Source File
# Begin Source File

SOURCE=..\..\..\globals.c
# End Source File
# Begin Source File

SOURCE=..\..\..\bzip2\huffman.c
# End Source File
# Begin Source File

SOURCE=..\..\..\szip\LzFind.c
# End Source File
# Begin Source File

SOURCE=..\..\..\szip\LzFind.h
# End Source File
# Begin Source File

SOURCE=..\..\..\szip\LzmaEnc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\szip\LzmaEnc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\win32\nt.c
# End Source File
# Begin Source File

SOURCE=..\..\..\szip\Ppmd.h
# End Source File
# Begin Source File

SOURCE=..\..\..\szip\Ppmd8.c
# End Source File
# Begin Source File

SOURCE=..\..\..\szip\Ppmd8.h
# End Source File
# Begin Source File

SOURCE=..\..\..\szip\Ppmd8Enc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\bzip2\randtable.c
# End Source File
# Begin Source File

SOURCE=..\..\..\trees.c
# End Source File
# Begin Source File

SOURCE=..\..\..\ttyio.c
# End Source File
# Begin Source File

SOURCE=..\..\..\util.c
# End Source File
# Begin Source File

SOURCE=..\..\..\win32\win32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\win32\win32i64.c
# End Source File
# Begin Source File

SOURCE=..\..\..\win32\win32zip.c
# End Source File
# Begin Source File

SOURCE=..\..\..\windll\windll.c
# End Source File
# Begin Source File

SOURCE=..\..\windll.h
# End Source File
# Begin Source File

SOURCE=..\..\..\windll\windll.rc
# End Source File
# Begin Source File

SOURCE=..\..\..\windll\windll32.def
# End Source File
# Begin Source File

SOURCE=..\..\..\zbz2err.c
# End Source File
# Begin Source File

SOURCE=..\..\..\zip.c
# End Source File
# Begin Source File

SOURCE=..\..\..\zipfile.c
# End Source File
# Begin Source File

SOURCE=..\..\..\windll\ziplib.def
# End Source File
# Begin Source File

SOURCE=..\..\..\zipup.c
# End Source File
# End Target
# End Project
