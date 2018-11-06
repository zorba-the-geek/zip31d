Attribute VB_Name = "VBZipBas"

Option Explicit

'---------------------------------------------------------------
'-- Please Do Not Remove These Comments!!!
'---------------------------------------------------------------
'-- Sample VB 6 code to drive zip32_dll.dll
'-- Based on the code contributed to the Info-ZIP project
'-- by Mike Le Voi
'--
'-- See the original VB example in a separate directory for
'-- more information
'--
'-- Use this code at your own risk. Nothing implied or warranted
'-- to work on your machine :-)
'---------------------------------------------------------------
'--
'-- The Source Code Is Freely Available From Info-ZIP At:
'-- ftp://ftp.info-zip.org/pub/infozip/infozip.html
'--
'-- A Very Special Thanks To Mr. Mike Le Voi
'-- And Mr. Mike White Of The Info-ZIP
'-- For Letting Me Use And Modify His Orginal
'-- Visual Basic 5.0 Code! Thank You Mike Le Voi.
'---------------------------------------------------------------

'---------------------------------------------------------------
' This example is redesigned to work with Zip32_dll.dll compiled
' from Zip 3.1 with Zip64 enabled.  This example is not backward
' compatible with the Zip 2.x dlls, the 3.0 dll, or earlier dlls
' from earlier Zip 3.1 betas.
'
' Modified 4/24/2004, 12/4/2007, 5/22/2010, 7/20/2014,
' 9/23/2015 EG
'---------------------------------------------------------------

'---------------------------------------------------------------
' Usage notes:
'
' This code uses Zip32_dll.dll.  You DO NOT need to register the
' DLL to use it.  You also DO NOT need to reference it in your
' VB project.  You DO have to copy the DLL to your SYSTEM
' directory, your VB project directory, or place it in a directory
' on your command PATH.
'
' Note that Zip32_dll.dll is probably not thread safe so you should
' avoid using the dll in multiple threads at the same time without
' first testing for interaction.
'
' All code provided under the Info-ZIP license.  If you have any
' questions please contact Info-ZIP.
'
' Copyright (c) 2015 Info-ZIP.  All rights reserved.
'
' See the accompanying file LICENSE, version 2009-Jan-2 or later
' (the contents of which are also included in zip.h) for terms of use.
' If, for some reason, all these files are missing, the Info-ZIP license
' also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
'
' 23 September 2015 EG
'
'---------------------------------------------------------------

'-- C Style argv
'-- Holds The Zip Archive Filenames
'
' This is old but still relevant:
' Max for zFiles just over 8000 as each pointer takes up 4 bytes and
' VB only allows 32 kB of local variables and that includes function
' parameters.  - 3/19/2004 EG
'
' Can put names in strZipFileNames instead of using this array,
' which avoids this limit.  File names are separated by spaces.
' Enclose names in quotes if include spaces.
'Public Type ZIPnames
'  zFiles(1 To 100) As String
'End Type



'=====================================================================
'
' Zip 3.1 now uses a command line string instead of a separate options
' structure and a file list.  All files are put in the command string.
' File names with spaces are surrounded by double quotes.


' Below is the version of the DLL interface this program is meant to
' work with.  Even though the Zip version may increase, the DLL version
' will remain this until the DLL interface changes in a way that is not
' backward compatible.  This is checked in DisplayVersion.

Public Const Compatible_DLL_Version As String = "3.1.0"


Public CommandLine As String

Public DisplayedVersion As Boolean
  
  
'-- Call Back "String"
Public Type ZipCBChar
  ch(4096) As Byte
End Type

'-- Version Structure
Public Type VerType
  Major As Byte
  Minor As Byte
  PatchLevel As Byte
  NotUsed As Byte
End Type
Public Type ZipVerType
  structlen       As Long         ' Length Of The Structure Being Passed
  flag            As Long         ' Bit 0: is_beta  bit 1: uses_zlib
  BetaLevel       As String       ' e.g., "g BETA" or ""
  Version         As String       ' e.g., "3.1d30"
  RevDate         As String       ' e.g., "4 Sep 95" (beta) or "4 September 1995"
  RevYMD          As String       ' e.g., "1995/09/04" (same as RevDate, but YMD)
  ZLIB_version    As String       ' e.g., "1.0.5" or NULL
  encryption      As Long         ' 0 if encryption not available
  ZipVersion      As VerType      ' the version of Zip the dll is compiled from
  os2dllVersion   As VerType      ' for Windows apps ignore this
  libdllVersion   As VerType      ' backward compatible to this Zip version
  OptStrucSize    As Long         ' the expected size of the ZpOpt structure
  FeatureList     As String       ' a list of features enabled in this dll
End Type
' see the version check code below for how to use these

''-- ZPOPT Is Used To Set The Options In The ZIP32z64.DLL
' (This is not applicable to zip32_dll.dll.)
'Public Type ZpOpt
'  ExcludeBeforeDate As String ' Date in either US 12/31/98 or ISO 1998-12-31 format
'  IncludeBeforeDate As String ' Date in either US 12/31/98 or ISO 1998-12-31 format
'  szRootDir      As String ' Root Directory Pathname (Up To 256 Bytes Long)
'  szTempDir      As String ' Temp Directory Pathname (Up To 256 Bytes Long)
''  fSuffix        As Long   ' Include Suffixes (Not Yet Implemented!)
'  fUnicode       As Long   ' was Misc flags and before that "Include Suffixes"
'    ' fUnicode flags (add values together)
'    '  1 = No longer used - was Include Suffixes (Not Yet Implemented!)
'    '  2 = No UTF8 (ignore UTF8 information in existing entries)
'    '  4 = Native UTF8 (store UTF8 as native character set)
'    '  fUnicode = 2 is probably the most backward compatible
'    '  fUnicode = 0 should create entries that old and new unzips can handle
'    '  fUnicode = 4 should only be used if the unzip is known to have UTF8 support
'    '               but produces the most efficient UTF8 encoding
'  fEncrypt       As Long   ' 1 for standard Encryp, Else 0 (Other methods soon!)
'  fSystem        As Long   ' 1 To Include System/Hidden Files, Else 0
'  fVolume        As Long   ' 1 If Storing Volume Label, Else 0
'  fExtra         As Long   ' 1 If Excluding Extra Attributes, Else 0
'  fNoDirEntries  As Long   ' 1 If Ignoring Directory Entries (end with /), Else 0
'  fVerbose       As Long   ' 1 If Full Messages Wanted, Else 0
'  fQuiet         As Long   ' 1 If Minimum Messages Wanted, Else 0
'  fCRLF_LF       As Long   ' 1 If Translate CR/LF To LF, Else 0
'  fLF_CRLF       As Long   ' 1 If Translate LF To CR/LF, Else 0
'  fJunkDir       As Long   ' 1 If Junking Directory Names on entries, Else 0
'  fGrow          As Long   ' 1 If Allow Appending To Zip File, Else 0
'  fForce         As Long   ' 1 If Making Entries Using DOS File Names, Else 0
'  fMove          As Long   ' 1 If Deleting Files Added Or Updated, Else 0
'  fDeleteEntries As Long   ' 1 If Files Passed Have To Be Deleted, Else 0
'  fUpdate        As Long   ' 1 If Updating Zip File-Overwrite Only If Newer, Else 0
'  fFreshen       As Long   ' 1 If Freshing Zip File-Overwrite Only, Else 0
'  fJunkSFX       As Long   ' 1 If Junking SFX Prefix, Else 0
'  fLatestTime    As Long   ' 1 If Setting Zip File Time To Time Of Latest File In Archive, Else 0
'  fComment       As Long   ' 1 If Putting Comment In Zip File, Else 0
'  fOffsets       As Long   ' 1 If Updating Archive Offsets For SFX Files, Else 0
'  fPrivilege     As Long   ' 1 If Not Saving Privileges, Else 0
'  fEncryption    As Long   ' Read Only Property!!!
'  szSplitSize    As String ' Size of split if splitting, Else NULL (empty string)
'                           ' This string contains the size that you want to
'                           ' split the archive into. i.e. 100 for 100 bytes,
'                           ' 2K for 2 k bytes, where K is 1024, m for meg
'                           ' and g for gig.
'  szIncludeList  As String ' If used, space separated list of Include filename
'                           ' patterns where match includes file - put quotes
'                           ' around each filename pattern.
'  IncludeListCount As Long ' place filler (not for VB) - (inits to 0) DO NOT USE
'  IncludeList    As Long   ' place filler (not for VB) - (inits to 0) DO NOT USE
'  szExcludeList  As String ' If used, space separated list of Exclude filename
'                           ' patterns where match excludes file - put quotes
'                           ' around each filename pattern.
'  ExcludeListCount As Long ' place filler (not for VB) - (inits to 0) DO NOT USE
'  ExcludeList    As Long   ' place filler (not for VB) - (inits to 0) DO NOT USE
'  fRecurse       As Long   ' 1 (-r), 2 (-R) If Recursing Into Sub-Directories, Else 0
'  fRepair        As Long   ' 1 = Fix Archive, 2 = Try Harder To Fix, Else 0
'  flevel         As Byte   ' Compression Level - 0 = Stored 6 = Default 9 = Max
'  szCompMethod   As String ' compression method string (e.g. "bzip2") or NULL
'  szProgressSize As String ' bytes between progress report callbacks (in nm form,
'                           ' where n is an integer and m is a multiplier letter
'                           ' such as 10k for 10 killobytes (k, m, g, t are valid)
'  fluff(8)       As Long   ' not used, for later expansion (set to all zeroes)
'End Type


'' Used by SetZipOptions
'Public Enum ZipModeType
'    Add = 0
'    Delete = 1
'    Update = 2
'    Freshen = 3
'End Enum
'Public Enum CompressionLevelType
'    c0_NoCompression = 0
'    c1_Fast = 1
'    c2_Fast = 2
'    c3_Fast = 3
'    c4_Med = 4
'    c5_Med = 5
'    c6_Default = 6
'    c7_Extra = 7
'    c8_Extra = 8
'    c9_Max = 9
'End Enum
'Public Enum Translate_LF_Type
'    No_Line_End_Trans = 0
'    LF_To_CRLF = 1
'    CRLF_To_LF = 2
'End Enum
'Public Enum RepairType
'    NoRepair = 0
'    TryFix = 1
'    TryFixHarder = 2
'End Enum
'Public Enum VerbosenessType
'    Quiet = 0
'    Normal = 1
'    Verbose = 2
'End Enum
'Public Enum RecurseType
'    NoRecurse = 0
'    r_RecurseIntoSubdirectories = 1
'    R_RecurseUsingPatterns = 2
'End Enum
'Public Enum UnicodeType
'    Unicode_Backward_Compatible = 0
'    Unicode_No_UTF8 = 2
'    Unicode_Native_UTF8 = 4
'End Enum
'Public Enum EncryptType
'    Encrypt_No = 0
'    Encrypt_Standard = 1
'End Enum


'-- This Structure Is Used For The ZIP32_DLL.DLL Function Callbacks
'   Assumes zip32_dll.dll with Zip64 enabled.
'   The contents and order of this structure must match that in api.h.
Public Type ZIPUSERFUNCTIONS
  dll_print      As Long          ' Callback Print Function
  dll_ecomment   As Long          ' Callback Entry Comment Function
  dll_acomment   As Long          ' Callback Archive Comment Function
  dll_password   As Long          ' Callback Password Function
  dll_split      As Long          ' Callback Split Select Function (not implemented)
  ' There are 2 versions of SERVICE - VB 6 does not have 64-bit data type
  dll_service    As Long          ' See below for how to call this in 32-bit envir
  dll_service_no_int64  As Long   ' Callback Service Function (32-bit)
  dll_progress   As Long          ' Callback Progress Function
  dll_error      As Long          ' Callback Warning/Error Function
  dll_finish     As Long          ' Callback Finish Function
End Type


'-- version info
Public ZipVersion As ZipVerType

'-- Local Declarations
'Public ZOPT  As ZpOpt
Public ZUSER As ZIPUSERFUNCTIONS

'-- This Assumes zip32_dll.dll Is In Your \windows\system directory
'-- or a copy is in the program directory or in some other directory
'-- listed in PATH


' ZpZip is now the single entry point for making it happen

Private Declare Function ZpZip Lib "zip32_dll.dll" _
  (ByVal CommandLine As String, _
   ByVal CurrentDir As String, _
   ByRef Callbacks As ZIPUSERFUNCTIONS, _
   ByVal ProgressChunkSize As String) As Long '-- Real Zipping Action

' ZpVersion is how we find out about the DLL we're using and if it's
' compatible with us.

Private Declare Sub ZpVersion Lib "zip32_dll.dll" _
  (ByRef ZipVersion As ZipVerType) '-- Version of DLL


' No longer used:
'-------------------------------------------------------
'-- Public Variables For Setting The ZPOPT Structure...
'-- (WARNING!!!) You Must Set The Options That You
'-- Want The ZIP32.DLL To Do!
'-- Before Calling VBZip32!
'--
'-- NOTE: See The Above ZPOPT Structure Or The VBZip32
'--       Function, For The Meaning Of These Variables
'--       And How To Use And Set Them!!!
'-- These Parameters Must Be Set Before The Actual Call
'-- To The VBZip32 Function!
'------------------------------------------------------

'-- Public Program Variables
'Public zArgc           As Integer     ' Number Of Files To Zip Up
'Public zZipArchiveName As String      ' The Zip File Name ie: Myzip.zip
'Public zZipFileNames   As ZIPnames    ' File Names To Zip Up
'Public strZipFileNames As String      ' String of names to Zip Up
'Public zZipInfo        As String      ' Holds The Zip File Information

'-- Public Constants
'-- For Zip & UnZip Error Codes!
Public Const ZE_OK = 0              ' Success (No Error)
Public Const ZE_EOF = 2             ' Unexpected End Of Zip File Error
Public Const ZE_FORM = 3            ' Zip File Structure Error
Public Const ZE_MEM = 4             ' Out Of Memory Error
Public Const ZE_LOGIC = 5           ' Internal Logic Error
Public Const ZE_BIG = 6             ' Entry Too Large To Split Error
Public Const ZE_NOTE = 7            ' Invalid Comment Format Error
Public Const ZE_TEST = 8            ' Zip Test (-T) Failed Or Out Of Memory Error
Public Const ZE_ABORT = 9           ' User Interrupted Or Termination Error
Public Const ZE_TEMP = 10           ' Error Using A Temp File
Public Const ZE_READ = 11           ' Read Or Seek Error
Public Const ZE_NONE = 12           ' Nothing To Do Error
Public Const ZE_NAME = 13           ' Missing Or Empty Zip File Error
Public Const ZE_WRITE = 14          ' Error Writing To A File
Public Const ZE_CREAT = 15          ' Could't Open To Write Error
Public Const ZE_PARMS = 16          ' Bad Command Line Argument Error
Public Const ZE_OPEN = 18           ' Could Not Open A Specified File To Read Error
Public Const ZE_COMPERR = 19        ' Error in compilation options
Public Const ZE_ZIP64 = 20          ' Zip64 not supported
Public Const ZE_CRYPT = 21          ' encryption error
Public Const ZE_COMPRESS = 22       ' compression error
Public Const ZE_BACKUP = 23         ' backup (-BT) error


'-- These Functions Are For The zip32_dll.DLL
'--
'-- Puts A Function Pointer In A Structure
'-- For Use With Callbacks...
Public Function FnPtr(ByVal lp As Long) As Long
    
  FnPtr = lp

End Function


'-- Convert ZipCBChar to String
Function ZipCBChar_to_string(ByRef ZipCBC As ZipCBChar) As String
  Dim xx As Long
  
  For xx = 0 To 8192
    If ZipCBC.ch(xx) = 0 Then
      Exit For
    Else
      ZipCBChar_to_string = ZipCBChar_to_string + Chr(ZipCBC.ch(xx))
    End If
  Next
  
End Function


'-- Convert String to ZipCBChar
Sub String_to_ZipCBChar(ByRef ZipCBC As ZipCBChar, ByVal InString As String, _
                        ByVal BufSize As Integer)
  Dim xx As Long
  
  For xx = 0 To BufSize - 1
    ZipCBC.ch(xx) = 0
  Next

  For xx = 0 To Len(InString) - 1
    ZipCBC.ch(xx) = Asc(Mid(InString, xx + 1, 1))
  Next

  ZipCBC.ch(xx) = 0 ' Put Null Terminator For C

End Sub


'-- Make almost equivalent to long long
' (No check for overflow)
Public Function HighLowToCurrency(ByVal High As Long, _
                                  ByVal Low As Long) As Currency
  
  HighLowToCurrency = (High * &H10000 * &H10000) + Low

End Function


'-- Callback For zip32_dll.DLL - DLL Print Function
' All standard output from Zip gets routed through this callback.  If
' address is set to NULL in functions structure, no standard output is
' received.

Public Function dll_print(ByRef fname As ZipCBChar, ByVal x As Long) As Long
    
  Dim s0 As String
  Dim xx As Long
    
  '-- Always Put This In Callback Routines!
  On Error Resume Next
    
  '-- Get Zip32.DLL Message For processing
  s0 = ZipCBChar_to_string(fname)
  
  '----------------------------------------------
  '-- This Is Where The DLL Passes Back Messages
  '-- To You! You Can Change The Message Printing
  '-- Below Here!
  '----------------------------------------------
  
  ' Change newlines to CrLf
  s0 = Replace(s0, Chr(10), vbCrLf)
  
  '-- Display Zip File Information
  '-- zZipInfo = zZipInfo & s0
  PrintOut s0
    
  DoEvents
    
  dll_print = 0

End Function

    
'-- Callback For zip32_dll.DLL - DLL Service Function
' This is called once after each entry is processed to report
' file name and size of entry.
'
' As callback Service includes the latest features and can be used
' in the VB 6 32-bit environment as shown below, use of this callback
' is deprecated.
Public Function dll_service_no_int64(ByRef FileName As ZipCBChar, _
                                     ByRef UFileName As ZipCBChar, _
                                     ByVal LowSize As Long, _
                                     ByVal HighSize As Long) As Long

    Dim file_name As String
    Dim xx As Long
    Dim FS As Currency  ' for large file sizes
    
    '-- Always Put This In Callback Routines!
    On Error Resume Next
    
    FS = HighLowToCurrency(HighSize, LowSize)
    
    file_name = ZipCBChar_to_string(FileName)
    
    ' It is up to the developer to code something useful here :)
    
    dll_service_no_int64 = 0 ' Setting this to 1 will abort the zip!
    
End Function

'-- Callback For zip32_dll.DLL - DLL Service Function
' This is called once after each entry is processed to report file name,
' uncompressed size of entry, compressed size, action performed (add,
' delete, ...), compression method, additional information, and percent
' of compression.  Note use of Low and High parts to handle 64-bit size
' data.
Public Function dll_service(ByRef FileName As ZipCBChar, _
                            ByRef UFileName As ZipCBChar, _
                            ByRef USizeString As ZipCBChar, _
                            ByRef CSizeString As ZipCBChar, _
                            ByVal USizeLow As Long, _
                            ByVal USizeHigh As Long, _
                            ByVal CSizeLow As Long, _
                            ByVal CSizeHigh As Long, _
                            ByRef Action As ZipCBChar, _
                            ByRef Method As ZipCBChar, _
                            ByRef Info As ZipCBChar, _
                            ByVal Perc As Long) As Long

    Dim file_name As String
    Dim usize_string As String
    Dim csize_string As String
    Dim usize As Currency
    Dim csize As Currency
    Dim action_string As String
    Dim method_string As String
    Dim info_string As String
    Dim s As String
    
    '-- Always Put This In Callback Routines!
    On Error Resume Next
    
    file_name = ZipCBChar_to_string(FileName)
    usize_string = ZipCBChar_to_string(USizeString)
    csize_string = ZipCBChar_to_string(CSizeString)
    usize = HighLowToCurrency(USizeHigh, USizeLow)
    csize = HighLowToCurrency(CSizeHigh, CSizeLow)
    action_string = ZipCBChar_to_string(Action)
    method_string = ZipCBChar_to_string(Method)
    info_string = ZipCBChar_to_string(Info)
        
    ' It is up to the developer to code something useful here :)
    
    PrintOut "Service Callback: " & ": u " & usize_string & "  "
    s = " " & action_string & "  " & method_string & " "
    If Len(info_string) > 0 Then
      s = s & "(" & info_string & ") "
    End If
    PrintOut "c " & csize_string & s & file_name
    PrintOut "  " & Perc / 100# & "%" & vbCrLf
    
    dll_service = 0 ' Setting this to 1 will abort the zip!
    
End Function


'-- Callback For zip32_dll.DLL - DLL Finish Function
' This is called at the end of the zip operation with the
' final statistics.
Public Function dll_finish(ByRef USizeString As ZipCBChar, _
                           ByRef CSizeString As ZipCBChar, _
                           ByVal USizeLow As Long, _
                           ByVal USizeHigh As Long, _
                           ByVal CSizeLow As Long, _
                           ByVal CSizeHigh As Long, _
                           ByVal Perc As Long) As Long

    Dim usize_string As String
    Dim csize_string As String
    Dim usize As Currency
    Dim csize As Currency
    
    '-- Always Put This In Callback Routines!
    On Error Resume Next
    
    usize_string = ZipCBChar_to_string(USizeString)
    csize_string = ZipCBChar_to_string(CSizeString)
    usize = HighLowToCurrency(USizeHigh, USizeLow)
    csize = HighLowToCurrency(CSizeHigh, CSizeLow)
        
    ' It is up to the developer to code something useful here :)
    
    PrintOut "Finish Callback: " & ": u " & usize_string & "  "
    PrintOut "c " & csize_string
    PrintOut "  " & Perc / 100# & "%" & vbCrLf
    
    dll_finish = 0
    
End Function


'Dim retstring(255) As String

'-- Callback For zip32_dll.DLL - DLL Password Function
Public Function dll_password(ByVal BufSize As Long, _
                             ByRef inprompt As ZipCBChar, _
                             ByRef password As ZipCBChar) As Long

  Dim prompt     As String
  Dim xx         As Integer
  Dim szpassword As String
  
  '-- Always Put This In Callback Routines!
  On Error Resume Next
    
  '-- Return 0 if fail, 1 on success
  dll_password = 0
  
  '-- Enter or Verify
  prompt = ZipCBChar_to_string(inprompt)
  
  '-- If A Password Is Needed Have The User Enter It!
  '-- This Can Be Changed
  
  szpassword = InputBox("Please Enter The Password!", prompt)

  '-- The User Did Not Enter A Password So Exit The Function
  If szpassword = "" Then Exit Function

  String_to_ZipCBChar password, szpassword, BufSize
  
'  For xx = 0 To BufSize - 1
'    password.ch(xx) = 0
'  Next
'
'  For xx = 0 To Len(szpassword) - 1
'    password.ch(xx) = Asc(Mid(szpassword, xx + 1, 1))
'  Next
'
'  password.ch(xx) = 0 ' Put Null Terminator For C

  dll_password = 1

End Function

'-- Callback For zip32_dll.dll - DLL EComment Function
Public Function dll_ecomment(ByRef OldComment As ZipCBChar, _
                             ByRef FileName As ZipCBChar, _
                             ByRef UFileName As ZipCBChar, _
                             ByVal MaxComLen As Long, _
                             ByRef NewComLen As Long, _
                             ByRef NewComment As ZipCBChar) As Long

    Dim old_comment As String
    Dim file_name As String
    Dim new_comment As String
    Dim xx%, szcomment$
    
    '-- Always Put This In Callback Routines!
    On Error Resume Next
    
    '-- Return 0 on fail, 1 on success
    dll_ecomment = 0
    
    old_comment = ZipCBChar_to_string(OldComment)
    
    file_name = ZipCBChar_to_string(FileName)
    
    new_comment = InputBox("Enter or edit the comment for " & file_name, "Entry Comment", Default:=old_comment)
    
    If new_comment = "" Then
      ' either empty comment or Cancel button
      If MsgBox("Remove comment?" & Chr(13) & "Hit No to keep existing comment", vbYesNo) = vbNo Then
        Exit Function
      End If
    End If
    
    NewComLen = Len(new_comment)
    If NewComLen > MaxComLen Then
      NewComLen = MaxComLen
    End If
    
    String_to_ZipCBChar NewComment, new_comment, NewComLen
    
'    For xx = 0 To NewComLen - 1
'        NewComment.ch(xx) = Asc(Mid$(new_comment, xx + 1, 1))
'    Next xx
'    NewComment.ch(xx) = 0 ' Put null terminator for C

    dll_ecomment = 1
    
End Function

'-- Callback For zip32_dll.dll - DLL EComment Function
Public Function dll_acomment(ByRef OldComment As ZipCBChar, _
                             ByVal MaxComLen As Long, _
                             ByRef NewComLen As Long, _
                             ByRef NewComment As ZipCBChar) As Long
    
  Dim old_comment As String
  Dim new_comment As String
  Dim xx%, szcomment$
    
  '-- Always Put This In Callback Routines!
  On Error Resume Next
    
  ' Return 0 on fail, 1 on success
  dll_acomment = 0
    
  old_comment = ZipCBChar_to_string(OldComment)
  
  new_comment = InputBox("Enter or edit the comment for the archive", "Archive Comment", Default:=old_comment)
  
  If new_comment = "" Then
    ' either empty comment or Cancel button
    If MsgBox("Remove comment?" & Chr(13) & "Hit No to keep existing comment", vbYesNo) = vbNo Then
      Exit Function
    End If
  End If
  
  NewComLen = Len(new_comment)
  If NewComLen > MaxComLen Then
    NewComLen = MaxComLen
  End If
  
  String_to_ZipCBChar NewComment, new_comment, NewComLen
  
'  For xx = 0 To NewComLen - 1
'      NewComment.ch(xx) = Asc(Mid$(new_comment, xx + 1, 1))
'  Next xx
'  NewComment.ch(xx) = 0 ' Put null terminator for C
  
  dll_acomment = 1
  
End Function

'-- Callback For zip32_dll.dll - DLL Progress Function
' If enabled (not set to NULL), this is called before an entry
' is processed, during processing as determined by chunk size
' (ProgChunkSize in ZpZip call below), and after processing of
' the entry is done.
'
Public Function dll_progress(ByRef FileName As ZipCBChar, _
                             ByRef UFileName As ZipCBChar, _
                             ByVal PercentAllDone100 As Long, _
                             ByVal PercentEntryDone100 As Long, _
                             ByVal USize_Low As Long, _
                             ByVal USize_High As Long, _
                             ByRef USizeString As ZipCBChar, _
                             ByRef Action As ZipCBChar, _
                             ByRef Method As ZipCBChar, _
                             ByRef Info As ZipCBChar) As Long

    Dim xx As Long
    Dim file_name As String
    Dim PercentAllDone As Double
    Dim PercentEntryDone As Double
    Dim usize As Currency
    Dim usize_string As String
    Dim action_string As String
    Dim method_string As String
    Dim info_string As String
    Dim s As String
    Dim message As String
        
    '-- Always Put This In Callback Routines!
    On Error Resume Next
    
    PercentAllDone = PercentAllDone100 / 100#
    PercentEntryDone = PercentEntryDone100 / 100#
    
    file_name = ZipCBChar_to_string(FileName)
    
'    For xx = 0 To 4096
'      If FileName.ch(xx) = 0 Then
'        Exit For
'      Else
'        file_name = file_name + Chr(FileName.ch(xx))
'      End If
'    Next
    
    usize = HighLowToCurrency(USize_High, USize_Low)
    
    usize_string = ZipCBChar_to_string(USizeString)
    
'    For xx = 0 To 4096
'      If USizeString.ch(xx) = 0 Then
'        Exit For
'      Else
'        usize_string = usize_string + Chr(USizeString.ch(xx))
'      End If
'    Next
    
    action_string = ZipCBChar_to_string(Action)

'    For xx = 0 To 4096
'      If Action.ch(xx) = 0 Then
'        Exit For
'      Else
'        action_string = action_string + Chr(Action.ch(xx))
'      End If
'    Next

    method_string = ZipCBChar_to_string(Method)

    info_string = ZipCBChar_to_string(Info)

    s = action_string & " " & method_string
    If Len(info_string) > 0 Then
      s = s & " (" & info_string & ")"
    End If

    Form1.Caption = "Zip32_dll.dll Example - " & _
                    "  overall " & PercentAllDone & "% " & _
                    "  this entry " & PercentEntryDone & "% " & _
                    " - " & s & ":" & _
                    "  " & file_name & "  (" & usize_string & " bytes)"

    
    message = "Progress Callback:" & _
              "  overall " & PercentAllDone & "% " & _
              "  this entry " & PercentEntryDone & "% " & _
              " - " & s & ":" & _
              "  " & file_name & "  (" & usize_string & " bytes)" & _
              vbCrLf
    
    'PrintOut message
    
    ' It is up to the developer to code something useful here :)
    
    dll_progress = 0 ' Setting this to 1 will abort the zip!
    
End Function


'' This function can be used to set options in VB
' (This is deprecated as zip32_dll.dll uses ZpZip which takes a Zip command line.)
'Public Function SetZipOptions(ByRef ZipOpts As ZpOpt, _
'  Optional ByVal ZipMode As ZipModeType = Add, Optional ByVal RootDirToZipFrom As String = "", _
'  Optional ByVal CompressionLevel As CompressionLevelType = c6_Default, _
'  Optional ByVal RecurseSubdirectories As RecurseType = NoRecurse, _
'  Optional ByVal Verboseness As VerbosenessType = Normal, _
'  Optional ByVal i_IncludeFiles As String = "", Optional ByVal x_ExcludeFiles As String = "", _
'  Optional ByVal fUnicode As UnicodeType = Unicode_Backward_Compatible, _
'  Optional ByVal UpdateSFXOffsets As Boolean = False, Optional ByVal JunkDirNames As Boolean = False, _
'  Optional ByVal Encrypt As EncryptType = Encrypt_No, Optional ByVal Password As String = "", _
'  Optional ByVal Repair As RepairType = NoRepair, Optional ByVal NoDirEntries As Boolean = False, _
'  Optional ByVal GrowExistingArchive As Boolean = False, _
'  Optional ByVal JunkSFXPrefix As Boolean = False, Optional ByVal ForceUseOfDOSNames As Boolean = False, _
'  Optional ByVal Translate_LF As Translate_LF_Type = No_Line_End_Trans, _
'  Optional ByVal Move_DeleteAfterAddedOrUpdated As Boolean = False, _
'  Optional ByVal SetZipTimeToLatestTime As Boolean = False, _
'  Optional ByVal IncludeSystemAndHiddenFiles As Boolean = False, _
'  Optional ByVal ProgressReportChunkSize As String = "", _
'  Optional ByVal ExcludeBeforeDate As String = "", Optional ByVal IncludeBeforeDate As String = "", _
'  Optional ByVal IncludeVolumeLabel As Boolean = False, _
'  Optional ByVal ArchiveComment As Boolean = False, Optional ByVal ArchiveCommentTextString = Empty, _
'  Optional ByVal UsePrivileges As Boolean = False, _
'  Optional ByVal ExcludeExtraAttributes As Boolean = False, Optional ByVal SplitSize As String = "", _
'  Optional ByVal TempDirPath As String = "", Optional ByVal CompMethod As String = "") As Boolean
'
'  Dim SplitNum As Long
'  Dim SplitMultS As String
'  Dim SplitMult As Long
'  Dim i As Integer
'
'  ' set some defaults
'  ZipOpts.ExcludeBeforeDate = vbNullString
'  ZipOpts.IncludeBeforeDate = vbNullString
'  ZipOpts.szRootDir = vbNullString
'  ZipOpts.szTempDir = vbNullString
'  'ZipOpts.fSuffix = 0
'  ZipOpts.fEncrypt = 0
'  ZipOpts.fSystem = 0
'  ZipOpts.fVolume = 0
'  ZipOpts.fExtra = 0
'  ZipOpts.fNoDirEntries = 0
'  ZipOpts.fVerbose = 0
'  ZipOpts.fQuiet = 0
'  ZipOpts.fCRLF_LF = 0
'  ZipOpts.fLF_CRLF = 0
'  ZipOpts.fJunkDir = 0
'  ZipOpts.fGrow = 0
'  ZipOpts.fForce = 0
'  ZipOpts.fMove = 0
'  ZipOpts.fDeleteEntries = 0
'  ZipOpts.fUpdate = 0
'  ZipOpts.fFreshen = 0
'  ZipOpts.fJunkSFX = 0
'  ZipOpts.fLatestTime = 0
'  ZipOpts.fComment = 0
'  ZipOpts.fOffsets = 0
'  ZipOpts.fPrivilege = 0
'  ZipOpts.szSplitSize = vbNullString
'  ZipOpts.IncludeListCount = 0
'  ZipOpts.szIncludeList = vbNullString
'  ZipOpts.ExcludeListCount = 0
'  ZipOpts.szExcludeList = vbNullString
'  ZipOpts.fRecurse = 0
'  ZipOpts.fRepair = 0
'  ZipOpts.flevel = 0
'  ZipOpts.fUnicode = Unicode_Backward_Compatible
'  ZipOpts.szCompMethod = vbNullString
'  ZipOpts.szProgressSize = vbNullString
'  ' for future expansion
'  For i = 1 To 8
'    ZipOpts.fluff(i) = 0
'  Next
'
'  If RootDirToZipFrom <> "" Then
'    ZipOpts.szRootDir = RootDirToZipFrom
'  End If
'  ZipOpts.flevel = Asc(CompressionLevel)
'  If UpdateSFXOffsets Then ZipOpts.fOffsets = 1
'
'  If i_IncludeFiles <> "" Then
'    ZipOpts.szIncludeList = i_IncludeFiles
'  End If
'  If x_ExcludeFiles <> "" Then
'    ZipOpts.szExcludeList = x_ExcludeFiles
'  End If
'
'  If ZipMode = Add Then
'    ' default
'  ElseIf ZipMode = Delete Then
'    ZipOpts.fDeleteEntries = 1
'  ElseIf ZipMode = Update Then
'    ZipOpts.fUpdate = 1
'  Else
'    ZipOpts.fFreshen = 1
'  End If
'  ZipOpts.fRepair = Repair
'  If GrowExistingArchive Then ZipOpts.fGrow = 1
'  If Move_DeleteAfterAddedOrUpdated Then ZipOpts.fMove = 1
'
'  If Verboseness = Quiet Then
'    ZipOpts.fQuiet = 1
'  ElseIf Verboseness = Verbose Then
'    ZipOpts.fVerbose = 1
'  End If
'
'  If ArchiveComment = False And Not IsEmpty(ArchiveCommentTextString) Then
'    MsgBox "Must set ArchiveComment = True to set ArchiveCommentTextString"
'    Exit Function
'  End If
'  If IsEmpty(ArchiveCommentTextString) Then
'    ArchiveCommentText = Empty
'  Else
'    ArchiveCommentText = ArchiveCommentTextString
'  End If
'  If ArchiveComment Then ZipOpts.fComment = 1
'
'  If NoDirEntries Then ZipOpts.fNoDirEntries = 1
'  If JunkDirNames Then ZipOpts.fJunkDir = 1
'  If Encrypt Then ZipOpts.fEncrypt = 1
'  If Password <> "" Then
'    ZipOpts.fEncrypt = 1
'    EncryptionPassword = Password
'  End If
'  If JunkSFXPrefix Then ZipOpts.fJunkSFX = 1
'  If ForceUseOfDOSNames Then ZipOpts.fForce = 1
'  If Translate_LF = LF_To_CRLF Then ZipOpts.fLF_CRLF = 1
'  If Translate_LF = CRLF_To_LF Then ZipOpts.fCRLF_LF = 1
'  ZipOpts.fRecurse = RecurseSubdirectories
'  If IncludeSystemAndHiddenFiles Then ZipOpts.fSystem = 1
'
'  If SetZipTimeToLatestTime Then ZipOpts.fLatestTime = 1
'  If ExcludeBeforeDate <> "" Then
'    ZipOpts.ExcludeBeforeDate = ExcludeBeforeDate
'  End If
'  If IncludeBeforeDate <> "" Then
'    ZipOpts.IncludeBeforeDate = IncludeBeforeDate
'  End If
'
'  If TempDirPath <> "" Then
'    ZipOpts.szTempDir = TempDirPath

'  End If
'
'  If CompMethod <> "" Then
'    ZipOpts.szCompMethod = CompMethod
'  End If
'
'  If SplitSize <> "" Then
'    SplitSize = Trim(SplitSize)
'    SplitMultS = Right(SplitSize, 1)
'    SplitMultS = UCase(SplitMultS)
'    If (SplitMultS = "K") Then
'        SplitMult = 1024
'        SplitNum = Val(Left(SplitSize, Len(SplitSize) - 1))
'    ElseIf SplitMultS = "M" Then
'        SplitMult = 1024 * 1024&
'        SplitNum = Val(Left(SplitSize, Len(SplitSize) - 1))
'    ElseIf SplitMultS = "G" Then
'        SplitMult = 1024 * 1024 * 1024&
'        SplitNum = Val(Left(SplitSize, Len(SplitSize) - 1))
'    Else
'        SplitMult = 1024 * 1024&
'        SplitNum = Val(SplitSize)
'    End If
'    SplitNum = SplitNum * SplitMult
'    If SplitNum = 0 Then
'        MsgBox "SplitSize of 0 not supported"
'        Exit Function
'    ElseIf SplitNum < 64 * 1024& Then
'        MsgBox "SplitSize must be at least 64k"
'        Exit Function
'    End If
'    ZipOpts.szSplitSize = SplitSize
'  End If
'
'  If ProgressReportChunkSize <> "" Then
'    ZipOpts.szProgressSize = ProgressReportChunkSize
'  End If
'
'  If IncludeVolumeLabel Then ZipOpts.fVolume = 1
'  If UsePrivileges Then ZipOpts.fPrivilege = 1
'  If ExcludeExtraAttributes Then ZipOpts.fExtra = 1
'
'  SetZipOptions = True
'
'End Function


' Crude conversion from wide to ANSI
Function ChopNulls(ByVal str) As String
  Dim A As Integer
  Dim C As String
    
  For A = 1 To Len(str)
    If Mid(str, A, 1) = Chr(0) Then
      ChopNulls = Left(str, A - 1)
      Exit Function
    End If
  Next
  ChopNulls = str
    
End Function


Sub PrintOut(text As String)

  Form1.OutputBox.text = Form1.OutputBox.text & text
  Form1.OutputBox.SelStart = Len(Form1.OutputBox.text)
  
End Sub


Sub DisplayVersion()
  
  ' display version of DLL
  Dim Beta As Boolean
  Dim ZLIB As Boolean
  Dim Zip64 As Boolean
  Dim Flags As String
  Dim A As Integer
  Dim DLLVersion As String
  Dim DisplayVersion As String
  Dim ReturnedFeatureList As String
  
  Dim BetaLevel       As String       ' e.g., "g BETA" or ""
  Dim Version         As String       ' e.g., "3.1d30"
  Dim RevDate         As String       ' e.g., "4 Sep 95" (beta) or "4 September 1995"
  Dim RevYMD          As String       ' e.g., "1995/09/04" (same as RevDate, but YMD)
  Dim ZLIB_version    As String       ' e.g., "1.0.5" or NULL
  Dim FeatureList     As String       ' a list of features enabled in this dll

  BetaLevel = String(10, " ")
  Version = String(20, " ")
  RevDate = String(20, " ")
  RevYMD = String(20, " ")
  ZLIB_version = String(10, " ")
  FeatureList = String(1024, " ")
  
  ZipVersion.BetaLevel = BetaLevel
  ZipVersion.Version = Version
  ZipVersion.RevDate = RevDate
  ZipVersion.RevYMD = RevYMD
  ZipVersion.ZLIB_version = ZLIB_version
  ZipVersion.FeatureList = FeatureList
  
  ZipVersion.structlen = Len(ZipVersion)
  
  ' Get the Zip DLL version
  ZpVersion ZipVersion

  ' Check flag
  If ZipVersion.flag And 1 Then
    Flags = Flags & " Beta,"
    Beta = True
  Else
    Flags = Flags & " No Beta,"
  End If
  If ZipVersion.flag And 2 Then
    Flags = Flags & " ZLIB,"
    ZLIB = True
  Else
    Flags = Flags & " No ZLIB,"
  End If
  If ZipVersion.flag And 4 Then
    Flags = Flags & " Zip64, "
    Zip64 = True
  Else
    Flags = Flags & " No Zip64, "
  End If
  If ZipVersion.encryption Then
    Flags = Flags & "Encryption"
  Else
    Flags = Flags & " No encryption"
  End If
  
  DisplayVersion = ChopNulls(ZipVersion.Version) & " " & ChopNulls(ZipVersion.RevDate)
  
  DLLVersion = ZipVersion.libdllVersion.Major & "." & _
               ZipVersion.libdllVersion.Minor & "." & _
               ZipVersion.libdllVersion.PatchLevel
  
  Form1.Caption = "Zip32_dll.dll Example"
                  
  PrintOut "Using Zip32_dll.dll [" & DisplayVersion & "]" & vbCrLf
  PrintOut "Zip Version:  " & ZipVersion.ZipVersion.Major & "." & _
                                   ZipVersion.ZipVersion.Minor & "." & _
                                   ZipVersion.ZipVersion.PatchLevel & " "
  PrintOut ChopNulls(ZipVersion.BetaLevel) & vbCrLf
  PrintOut "Expect DLL Version: " & Compatible_DLL_Version
  PrintOut "    Found DLL Version:  " & DLLVersion & vbCrLf
  PrintOut "FLAGS:  " & Flags & vbCrLf
  
  ReturnedFeatureList = ChopNulls(ZipVersion.FeatureList)
  ReturnedFeatureList = Replace(ReturnedFeatureList, ";", " ")
  
  PrintOut "Feature List:  " & ReturnedFeatureList & vbCrLf

  If Not Zip64 Then
    A = MsgBox("Zip32_dll.dll not compiled with Zip64 enabled - continue?", _
               vbOKCancel, _
               "Wrong dll")
    If A = vbCancel Then
        End
    End If
  End If
  
  ' Check if this DLL is compatible with our program
  If DLLVersion <> Compatible_DLL_Version Then
    A = MsgBox("Zip32_dll.dll version is " & DLLVersion & " but program needs " & _
               Compatible_DLL_Version & " - continue?", _
               vbOKCancel, _
               "Possibly incompatible dll")
    If A = vbCancel Then
        End
    End If
  End If
  
End Sub


'-- Main zip32_dll.dll Subroutine.
'-- This Is Where It All Happens!!!
'--
'-- (WARNING!) Do Not Change This Function!!!
'--
Public Function VBZip32() As Long
    
  Dim RetCode As Long
  Dim FileNotFound As Boolean
  Dim CurrentDir As String
  Dim ProgChunkSize As String
  
  ' On Error Resume Next '-- Nothing Will Go Wrong :-)
  On Error GoTo ZipError
    
  RetCode = 0
    
  '-- Set Address Of ZIP32.DLL Callback Functions
  '-- (WARNING!) Do Not Change!!! (except as noted below)
  ' Unused callbacks should be set to NULL
  ZUSER.dll_print = FnPtr(AddressOf dll_print)
  ZUSER.dll_password = FnPtr(AddressOf dll_password)
  ZUSER.dll_ecomment = FnPtr(AddressOf dll_ecomment)
  ZUSER.dll_acomment = FnPtr(AddressOf dll_acomment)
  ZUSER.dll_service = FnPtr(AddressOf dll_service)
  'ZUSER.dll_service_no_int64 = FnPtr(AddressOf dll_service_no_int64)
  ZUSER.dll_progress = FnPtr(AddressOf dll_progress)
  ZUSER.dll_finish = FnPtr(AddressOf dll_finish)
  
  ' If you need to set destination of each split set this
  'ZUSER.ZDLLSPLIT = FnPtr(AddressOf ZDLLSplitSelect)

'  '-- Set ZIP32.DLL Callbacks - return 1 if DLL loaded 0 if not
'  retcode = ZpInit(ZUSER)
'  If retcode = 0 And FileNotFound Then
'    MsgBox "Probably could not find Zip32z64.DLL - have you copied" & Chr(10) & _
'           "it to the System directory, your program directory, " & Chr(10) & _
'           "or a directory on your command PATH?"
'    VBZip32 = retcode
'    Exit Function
'  End If
  
  If Not DisplayedVersion Then
    Form1.OutputBox.text = ""
    DisplayVersion
    PrintOut "--------------------------------------" & vbCrLf
    PrintOut vbCrLf
    DisplayedVersion = True
  End If
    
'  If strZipFileNames = "" Then
'    ' not using string of names to zip (so using array of names)
'    strZipFileNames = vbNullString
'  End If
  
  CurrentDir = "."
  ProgChunkSize = "100k"
  
  '-- Go Zip It Them Up!
  PrintOut "Zip command:  zip " & CommandLine & vbCrLf
  PrintOut "Calling Zip:" & vbCrLf
  
  PrintOut "-------------------------------------------------" & vbCrLf
  RetCode = ZpZip(CommandLine, CurrentDir, ZUSER, ProgChunkSize)
  PrintOut "-------------------------------------------------" & vbCrLf

'  retcode = ZpArchive(zArgc, zZipArchiveName, zZipFileNames, strZipFileNames, ZOPT)
  
  PrintOut "Zip returned: " & RetCode & vbCrLf
  PrintOut vbCrLf
  
  '-- Return The Function Code
  VBZip32 = RetCode

  Exit Function

ZipError:
  MsgBox "Error:  " & Err.Description
  If Err = 48 Then
    FileNotFound = True
  End If
  Resume Next

End Function

