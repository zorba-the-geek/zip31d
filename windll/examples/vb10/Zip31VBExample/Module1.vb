' Example VS 2010 Visual Basic code showing how to call Zip and set up callbacks to
' handle progress tracking and error reporting.
'
' 28 March 2014  (Last updated 14 September 2015)
'
' Written by E. Gordon and distributed under the Info-ZIP license.
'
' Copyright (c) 2015 Info-ZIP.  All rights reserved.
'
'  See the accompanying file LICENSE, version 2009-Jan-2 or later
'  (the contents of which are also included in zip.h) for terms of use.
'  If, for some reason, all these files are missing, the Info-ZIP license
'  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
'
' If you use all or any of this code, please give credit.  Thanks!
'
' This example is written using the newer DLL interfacing introduced around VS 2008.
' This example probably will not work using VS 6 VB or earlier.  See the VB6
' example for code to use with Visual Studio 6 VB on Windows XP.

Option Explicit On

Imports System.Runtime.InteropServices


Module ZipVbModule

  ' Version of Zip DLL this example is compatible with
  Public CompatibleWinDLLMajorMinorVersion As String = "3.1"


  ' Setting this via the Abort button asks Zip to cancel the zip operation
  ' Can only abort during a Service or Progress callback
  Public AbortNow As Boolean

  Public Running As Boolean


  ' Zip Version
  Public ZVersion As ZipVerType

  ' Flags and version string (after ZpVersion is called)
  Public BetaFlag As Boolean = True
  Public ZLIBFlag As Boolean = False
  Public Zip64Flag As Boolean = False
  Public EncryptionFlag As Boolean = True
  Public ZipVersionString As String = ""
  Public WinDLLVersionString As String = ""
  Public WinDLLMajorMinorVersion As String = ""

  ' List of features in Zip (after ZpVersion is called)
  Public Features(1000) As String
  Public FeatureCount As Integer = 0
  Public ParsedFeatureList As String = ""

  ' This is used by ZipVerType
  <StructLayout(LayoutKind.Sequential)> Public Structure VerType
    Dim Major As Byte
    Dim Minor As Byte
    Dim PatchLevel As Byte
    Dim NotUsed As Byte
  End Structure

  ' Note that the below Integer members are 32-bit.  The default is 32-bit for
  ' VS 2010 Vb.  In VS 6 VB the default is 16-bit for Integer, so Long would
  ' be used.  Long in VS 2010 VB are 64-bit, so for that we need to use
  ' Integer.  So this example is only for VS 2010 or later.

  ' Structure used to receive Zip version information
  <StructLayout(LayoutKind.Sequential)> Public Structure ZipVerType
    Dim StructLen As Integer                          ' Length of this structure as returned
    Dim Flags As Integer                              ' Bit 0: Is beta, Bit 1: Uses ZLIB, Bit 2: Zip64 enabled
    Dim BetaLevel As String                           ' e.g., "g BETA" or ""
    Dim Version As String                             ' e.g., "3.1d28"
    Dim RevDate As String                             ' e.g., "4 Sep 95" (beta) or "4 September 1995"
    Dim RevYMD As String                              ' e.g., "19950904"
    Dim ZLibVersion As String                         ' e.g., "1.0.5" or NULL
    Dim Encryption As Integer                         ' 1 if any type of encryption, 0 if encryption not available
    Dim ZipVersion As VerType                         ' Version of Zip the dll is compiled from
    Dim OS2DLLVersion As VerType                      ' For Windows apps ignore this
    Dim LIBDLLVersion As VerType                      ' DLL interface backward compatible to this Zip version
    Dim OptStrucSize As Integer                       ' Expected size of the ZpOpt structure (if used)
    Dim FeatureList As String                         ' List of features enabled in this DLL
  End Structure

  Dim ZipVer As ZipVerType

  ' Structure that passes the callback function addresses to Zip.
  ' Make sure this matches the order in ZIPUSERFUNCTIONS structure in api.h.
  ' Split_Function is not yet implemented - if set, Zip will ask for a
  '  destination for each split.
  <StructLayout(LayoutKind.Sequential)> Public Structure Callback_Functions_Structure
    Dim Print_Function As PrintF_Delegate
    Dim EntryComment_Function As EntryCommentF_Delegate
    Dim ArchiveComment_Function As ArchiveCommentF_Delegate
    Dim Password_Function As PasswordF_Delegate
    Dim Split_Function As SplitF_Delegate                         ' Not implemented - This MUST be set to NULL
    Dim Service_Function As SA64F_Delegate                        ' Use this for VS 2010 and later with 64-bit Long
    Dim Service_No_Int64_Function As SA64NI64F_Delegate           ' Use this for VS 6 with 32-bit long
    Dim ProgressReport_Function As ProgressF_Delegate
    Dim ErrorReport_Function As ErrorF_Delegate
    Dim FinishReport_Function As FinishF_Delegate
  End Structure

  ' Delegate functions for each callback
  Public Delegate Function PrintF_Delegate(ByVal print_string As IntPtr, _
                                           ByVal length As UInt32) As Int32
  Public Delegate Function EntryCommentF_Delegate(ByVal old_entry_comment As String, _
                                                  ByVal file_name As String, _
                                                  ByVal utf8_file_name As String, _
                                                  ByVal max_comment_length As Int32, _
                                                  ByRef new_entry_comment_length As Int32, _
                                                  ByVal new_entry_comment As IntPtr) As Long
  Public Delegate Function ArchiveCommentF_Delegate(ByVal old_archive_comment As String, _
                                                    ByVal max_comment_length As Int32, _
                                                    ByRef new_archive_comment_length As Int32, _
                                                    ByVal new_archive_comment As IntPtr) As Long
  Public Delegate Function PasswordF_Delegate(ByVal buffer_size As Int32, _
                                              ByVal prompt_string As String, _
                                              ByVal password As IntPtr) As Long
  Public Delegate Function SplitF_Delegate(ByVal SplitPath As IntPtr, _
                                           ByVal Action As Int32) As String
  Public Delegate Function SA64F_Delegate(ByVal file_name As String, _
                                          ByVal utf8_file_name As String, _
                                          ByVal usize_string As String, _
                                          ByVal csize_string As String, _
                                          ByVal usize As Int64, _
                                          ByVal csize As Int64, _
                                          ByVal action_string As String, _
                                          ByVal method_string As String, _
                                          ByVal info_string As String, _
                                          ByVal percent As Int32) As Int32
  Public Delegate Function SA64NI64F_Delegate(ByVal file_name As IntPtr, _
                                              ByVal utf8_file_name As IntPtr, _
                                              ByVal length1 As UInt32, _
                                              ByVal length2 As UInt32) As Int32
  Public Delegate Function ProgressF_Delegate(ByVal file_name As String, _
                                              ByVal utf8_file_name As String, _
                                              ByVal perc_all_100 As UInt32, _
                                              ByVal perc_ent_100 As UInt32, _
                                              ByVal current_file_size As UInt64, _
                                              ByVal current_file_size_string As String, _
                                              ByVal action_string As String, _
                                              ByVal method_string As String,
                                              ByVal info_string As String) As Int32
  Public Delegate Function ErrorF_Delegate(ByVal error_message As String) As Int32
  Public Delegate Function FinishF_Delegate(ByVal total_bytes_string As String, _
                                            ByVal compressed_bytes_string As String, _
                                            ByVal total_bytes As Int64, _
                                            ByVal compressed_bytes As Int64, _
                                            ByVal percent_savings As Int32) As Int32

  'old
  'Public Delegate Function EntryCommentF_Delegate(ByVal comment_string As IntPtr, ByVal FileName As String, _
  '                                                ByVal max_comment_length As Int32, ByRef comment_length As Int32) As String
  'Public Delegate Function ArchiveCommentF_Delegate(ByVal comment_string As IntPtr, ByVal max_comment_length As Int32, _
  '                                                  ByRef comment_length As Int32) As String
  'Public Delegate Function PasswordF_Delegate(ByVal length As Int32, ByVal mode_string As String) As String



  ' If zip32_dll.dll is in your PATH and you want to use that version, change
  ' "..\..\..\zip32_dll.dll" to "zip32_dll.dll" below.  The below is looking
  ' for zip32_dll.dll in Zip31VBExample directory.

  ' Main Zip 3.1 DLL interface

  Public Declare Sub ZpVersion Lib "..\..\..\zip32_dll.dll" _
          (ByRef ZipVer As ZipVerType)

  Public Declare Function ZpZip Lib "..\..\..\zip32_dll.dll" _
          (ByVal CommandLine As String, ByVal CurrentDir As String, _
           ByRef Callback_Functions As Callback_Functions_Structure, _
           ByVal progress_chunk_size As String) _
          As Integer

  Public Declare Function ZpStringCopy Lib "..\..\..\zip32_dll.dll" _
          (ByVal DestString As IntPtr, ByVal SourceString As IntPtr, _
           ByVal MaxLength As Integer) As Int16


  ' Test entry points for Zip 3.1 DLL

  Public Declare Function ZpTestComParse Lib "..\..\..\zip32_dll.dll" _
          (ByVal CommandLine As String, ByVal ParsedLine As String) _
          As Integer

  Public Declare Sub ZpTestCallback Lib "..\..\..\zip32_dll.dll" _
            (ByVal CallbackAddr As ZTCDelegate)
  Public Delegate Sub ZTCDelegate(ByVal parm As IntPtr)

  Public Declare Sub ZpTestCallbackStruct Lib "..\..\..\zip32_dll.dll" _
      (ByRef Callback_Functions As Test_Callback_Function_Structure)

  Public Declare Function ZpZipTest Lib "..\..\..\zip32_dll.dll" _
          (ByVal CommandLine As String, ByVal CurrentDir As String, ByRef Callback_Functions As Callback_Functions_Structure) _
          As Integer

  <StructLayout(LayoutKind.Sequential)> Public Structure Test_Callback_Function_Structure
    Dim print As TCF_Delegate
  End Structure
  Public Delegate Sub TCF_Delegate(ByVal parm As IntPtr)


  ' ===============================================================================================
  ' Callback functions

  Function Print_Callback(ByVal PrintMessage As IntPtr, ByVal Length As Integer) As Integer

    Dim sPrintMessage As String
    Dim utf8_string As String

    sPrintMessage = Marshal.PtrToStringAnsi(PrintMessage)
    utf8_string = utf8_to_string(sPrintMessage, sPrintMessage)

    ' For our example, we display the Print message in StatusWindow
    If ZipVBForm.SeeZipOutput_CheckBox.Checked Then
      ZipVBForm.AppendToStatusWindow(utf8_string, ForeColor:=Color.Black)
    End If

    ' Let any pending events go
    Application.DoEvents()

    ' Should return the characters printed (as printf would) or -1 on error
    Print_Callback = Length

  End Function


  Function EntryComment_Callback(ByVal OldEntryComment As String, _
                                 ByVal FileName As String, _
                                 ByVal UTF8FileName As String, _
                                 ByVal MaxCommentLength As Int32, _
                                 ByRef NewEntryCommentLength As Int32, _
                                 ByVal NewEntryComment As IntPtr) As Long

    ' EntryComment_Callback called when Zip asking for comment for entry

    Dim NewComment As String
    'Dim OldComment As String
    Dim Prompt As String
    Dim CommentPtr As IntPtr


    ' Set to 0 on fail (keep old comment), 1 on success
    EntryComment_Callback = 0

    'OldComment = Marshal.PtrToStringAnsi(CurrentEntryComment)

    Prompt = "Current comment for " & FileName & ":  " & vbCrLf & _
           "------------------------------------------" & vbCrLf & _
           OldEntryComment & vbCrLf & _
           "------------------------------------------" & vbCrLf & _
           "Enter or edit comment for " & FileName & ":"

    NewComment = InputBox(Prompt, _
                          Title:="ZipVbExample - " & FileName, _
                          DefaultResponse:=OldEntryComment)
    If Len(OldEntryComment) > 0 And NewComment = "" Then
      ' either empty comment or Cancel button
      If MsgBox("Remove comment?" & Chr(13) & "Hit No to keep existing comment", vbYesNo) = vbNo Then
        Exit Function
      End If
    End If

    NewEntryCommentLength = Len(NewComment)
    If NewEntryCommentLength > MaxCommentLength Then
      NewEntryCommentLength = MaxCommentLength
      NewComment = Left(NewComment, NewEntryCommentLength)
    End If

    ' Create copy of string C can use
    CommentPtr = Marshal.StringToHGlobalAnsi(NewComment)
    ' Copy the copy to provided buffer
    ZpStringCopy(NewEntryComment, CommentPtr, MaxCommentLength)
    ' Free the copy
    Marshal.FreeHGlobal(CommentPtr)

    EntryComment_Callback = 1

  End Function


  'Function EntryComment_Callback(ByVal CurrentEntryComment As IntPtr, ByVal FileName As String, _
  '                               ByVal MaxCommentLength As Int32, ByRef CommentLength As Int32) As String
  '' EntryComment_Callback called when Zip asking for comment for entry

  'Dim NewComment As String
  'Dim OldComment As String
  '
  '  OldComment = Marshal.PtrToStringAnsi(CurrentEntryComment)

  ' MsgBox("Current comment for " & FileName & ":  " & vbCrLf & _
  '         "------------------------------------------" & vbCrLf & _
  '         OldComment & vbCrLf & _
  '         "------------------------------------------")

  '  NewComment = InputBox("Enter or edit comment for " & FileName & ":", _
  '                        Title:="ZipVbExample - " & FileName, _
  '                        DefaultResponse:=OldComment)
  '  If Len(OldComment) > 0 And NewComment = "" Then
  '' either empty comment or Cancel button
  '    If MsgBox("Remove comment?" & Chr(13) & "Hit No to keep existing comment", vbYesNo) = vbYes Then
  '      OldComment = ""
  '    End If
  '  Else
  '    OldComment = NewComment
  '  End If

  '  NewComment = OldComment
  '  CommentLength = Len(NewComment)
  '  If CommentLength > MaxCommentLength Then
  '    CommentLength = MaxCommentLength
  '    NewComment = Left(NewComment, CommentLength)
  '  End If

  '  EntryComment_Callback = NewComment

  'End Function


  Function ArchiveComment_Callback(ByVal OldArchiveComment As String, _
                                   ByVal MaxCommentLength As Int32, _
                                   ByRef NewArchiveCommentLength As Int32, _
                                   ByVal NewArchiveComment As IntPtr) As Long

    ' ArchiveComment_Callback called when Zip asking for comment for entire archive (not entries)

    '    Dim sOldArchiveComment As String
    Dim Prompt As String
    Dim CommentPtr As IntPtr
    Dim NewComment As String


    ' Set to 0 on fail (keep old commment), 1 on success
    ArchiveComment_Callback = 0

    'sOldArchiveComment = Marshal.PtrToStringAnsi(OldArchiveComment)

    Prompt = "Current Archive comment:" & vbCrLf & _
             "------------------------------------------" & vbCrLf & _
             OldArchiveComment & vbCrLf & _
             "------------------------------------------" & vbCrLf & _
             "Enter or edit new ZIP file comment:"

    NewComment = InputBox(Prompt, DefaultResponse:=OldArchiveComment)
    If Len(OldArchiveComment) > 0 And NewComment = "" Then
      ' either empty comment or Cancel button
      If MsgBox("Remove comment?" & Chr(13) & "Hit No to keep existing comment", vbYesNo) = vbNo Then
        Exit Function
      End If
    End If

    NewArchiveCommentLength = Len(NewComment)
    If NewArchiveCommentLength > MaxCommentLength Then
      NewArchiveCommentLength = MaxCommentLength
      NewComment = Left(NewArchiveComment, NewArchiveCommentLength)
    End If

    ' Create copy of string C can use
    CommentPtr = Marshal.StringToHGlobalAnsi(NewComment)
    ' Copy the copy to provided buffer
    ZpStringCopy(NewArchiveComment, CommentPtr, MaxCommentLength)
    ' Free the copy
    Marshal.FreeHGlobal(CommentPtr)

    ArchiveComment_Callback = 1

  End Function


  'Function ArchiveComment_Callback(ByVal CurrentArchiveComment As IntPtr, ByVal MaxCommentLength As Int32, _
  '                                 ByRef CommentLength As Int32) As String

  ' ArchiveComment_Callback called when Zip asking for comment for entire archive (not entries)

  'Dim NewComment As String
  'Dim sArchiveComment As String

  '  sArchiveComment = Marshal.PtrToStringAnsi(CurrentArchiveComment)

  '  MsgBox("Current Zip file comment:  " & vbCrLf & _
  '         "------------------------------------------" & vbCrLf & _
  '         sArchiveComment & vbCrLf & _
  '         "------------------------------------------")

  '  NewComment = InputBox("Enter or edit the ZIP file comment:", DefaultResponse:=sArchiveComment)
  '  If Len(sArchiveComment) > 0 And NewComment = "" Then
  '' either empty comment or Cancel button
  '    If MsgBox("Remove comment?" & Chr(13) & "Hit No to keep existing comment", vbYesNo) = vbYes Then
  '      sArchiveComment = ""
  '    End If
  '  Else
  '    sArchiveComment = NewComment
  '  End If

  '  CommentLength = Len(sArchiveComment)
  '  If CommentLength > MaxCommentLength Then
  '    CommentLength = MaxCommentLength
  '    sArchiveComment = Left(sArchiveComment, CommentLength)
  '  End If

  '  ArchiveComment_Callback = sArchiveComment

  'End Function


  Function Service_Callback(ByVal sPathString As String, _
                            ByVal sUTF8PathString As String, _
                            ByVal us As String, _
                            ByVal cs As String, _
                            ByVal u As Int64, _
                            ByVal c As Int64, _
                            ByVal sAction As String, _
                            ByVal sMethod As String, _
                            ByVal sInfo As String, _
                            ByVal perc As Int32) As Int32

    ' DLL Service Function - called as each entry processed

    Dim stats As String
    Dim unicode_filename As String
    Dim method_info As String
    Dim s As String


    'Dim sPathString As String
    '
    'sPathString = Marshal.PtrToStringAnsi(PathString)

    stats = "  (in " & us & " (" & u & "), out " & cs & " (" & c & "), " & perc & "%)"

    ' It is up to the developer to code something useful here :)
    unicode_filename = utf8_to_string(sUTF8PathString, sPathString)
    If ZipVBForm.SeeServiceOutput_Checkbox.Checked Then
      'ZipVBForm.AppendToStatusWindow(sAction & ":  " & sPathString, ForeColor:=Color.DarkRed)
      if Len(sInfo) > 0 then
        method_info = sMethod & " - " & sInfo
      Else
        method_info = sMethod
      End If
      s = sAction & " (" & method_info & "):  " & unicode_filename
      ZipVBForm.AppendToStatusWindow(s, ForeColor:=Color.DarkRed)
      ZipVBForm.AppendToStatusWindow(stats & vbCrLf, ForeColor:=Color.DarkKhaki)
      'ZipVBForm.AppendToStatusWindow("uf:  " & unicode_filename & vbCrLf, ForeColor:=Color.DarkKhaki)
    End If

    'MsgBox("Service callback:" & vbCrLf & _
    '       "Path: " & sPathString & vbCrLf & _
    '       stats, vbOKOnly, "Service_Callback")

    ' Let any pending events go
    Application.DoEvents()

    ' Setting this to 1 will abort the zip!
    Service_Callback = 0

    If AbortNow Then
      Service_Callback = 1
    End If

  End Function


  Public Function Password_Callback(ByVal BufferLength As Int32, _
                                    ByVal ModeString As String, _
                                    ByVal Password As IntPtr) As Long

    ' DLL Password Function

    Dim PasswordString As String
    Dim i As Integer
    Dim reply As Integer
    Dim PasswordPtr As IntPtr


    '-- Always Put This In Callback Routines!
    'On Error Resume Next

    ' Set to 0 if fail, 1 on success
    Password_Callback = 0

    PasswordString = ""

    ' Allow 3 attempts to enter password
    For i = 1 To 3
      PasswordString = InputBox(ModeString, ModeString)

      If Len(PasswordString) > BufferLength Then
        reply = MsgBox("Password too long (length " & Len(PasswordString) & _
                       " greater than " & BufferLength & " characters max)", vbOKCancel, "Zip Password")
        If reply = vbCancel Then
          Exit Function
        End If
      Else
        Exit For
      End If
    Next

    If i > 3 Then
      MsgBox("Too many attempts to enter password", vbOKOnly, "Zip Password")
      Exit Function
    End If

    If Len(PasswordString) = 0 Then
      Exit Function
    End If

    ' Create copy of string C can use
    PasswordPtr = Marshal.StringToHGlobalAnsi(PasswordString)
    ' Copy the copy to provided buffer
    ZpStringCopy(Password, PasswordPtr, BufferLength)
    ' Free the copy
    Marshal.FreeHGlobal(PasswordPtr)

    Password_Callback = 1

  End Function


  '  Public Function Password_Callback(ByVal BufferLength As Int16, ByVal ModeString As String) As String
  '
  '  ' DLL Password Function
  '
  '  Dim PasswordString As String
  '  Dim i As Integer
  '  Dim reply As Integer
  '
  '  '-- Always Put This In Callback Routines!
  '  'On Error Resume Next
  '
  '    Password_Callback = Nothing
  '
  '    PasswordString = ""
  '
  '  ' Allow 3 attempts to enter password
  '    For i = 1 To 3
  '      PasswordString = InputBox(ModeString, ModeString)
  '
  '      If Len(PasswordString) > BufferLength Then
  '        reply = MsgBox("Password too long (length " & Len(PasswordString) & _
  '                       " greater than " & BufferLength & " characters max)", vbOKCancel, "Zip Password")
  '        If reply = vbCancel Then
  '          Exit Function
  '        End If
  '      Else
  '        Exit For
  '      End If
  '    Next
  '
  '    If i > 3 Then
  '      MsgBox("Too many attempts to enter password", vbOKOnly, "Zip Password")
  '      Exit Function
  '    End If
  '
  '    If Len(PasswordString) = 0 Then
  '      Exit Function
  '    End If
  '
  '    Password_Callback = PasswordString
  '
  '  'PasswordString = Marshal.StringToHGlobalAnsi(sPasswordString)
  '
  '  End Function


  ' For VS 2010 and later we use the 64-bit interface
  Public Function Progress_Callback(ByVal PathString As String, _
                                    ByVal UTF8PathString As String, _
                                    ByVal PercentEntryDone100 As Int32, _
                                    ByVal PercentAllDone100 As Int32, _
                                    ByVal UncompressedFileSize As UInt64, _
                                    ByVal UncompressedFileSizeString As String, _
                                    ByVal ActionString As String, _
                                    ByVal MethodString As String, _
                                    ByVal InfoString AS String) As Int32

    ' Progress Callback Function

    Dim PercentAllDone As Double
    Dim PercentEntryDone As Double
    Dim utf8_string As String

    'Dim sPathString As String
    'sPathString = Marshal.PtrToStringAnsi(PathString)

    utf8_string = utf8_to_string(UTF8PathString, PathString)

    PercentAllDone = PercentAllDone100 / 100.0#
    PercentEntryDone = PercentEntryDone100 / 100.0#

    ZipVBForm.ActionString_Box.Text = ActionString
    'ZipVBForm.EntryPath_Box.Text = PathString
    ZipVBForm.EntryPath_Box.Text = utf8_string
    ZipVBForm.PercentEntry_Box.Text = PercentEntryDone
    ZipVBForm.PercentTotal_Box.Text = PercentAllDone
    ZipVBForm.FileSize_Box.Text = UncompressedFileSizeString

    'MsgBox("In progress callback", vbOKOnly, "Progress_Callback")

    ' Let any pending events go
    Application.DoEvents()

    ' Setting this to 1 will abort the Zip!
    Progress_Callback = 0

    If AbortNow Then
      Progress_Callback = 1
    End If

  End Function


  ' Called when a warning or error is generated by Zip
  Public Function Error_Callback(ByVal Message As String) As Int32

    ' We just append any error messages to Status Window in red
    ZipVBForm.AppendToStatusWindow(Message & vbCrLf, ForeColor:=Color.Red)

    MsgBox(Message, vbOKOnly, "Zip Warning/Error")

    ' Let any pending events go
    Application.DoEvents()

    ' Currently return value does nothing
    Error_Callback = 0

  End Function


  ' Called when the zip operation is done to provide overall stats
  Public Function Finish_Callback(ByVal TotalBytesString As String, _
                                  ByVal CompressedBytesString As String, _
                                  ByVal TotalBytes As Int64, _
                                  ByVal CompressedBytes As Int64, _
                                  ByVal PercentSavings As Int32) As Int32

    Dim msg As String

    msg = "Total bytes processed:  " & TotalBytesString & " (" & TotalBytes & "),  " & _
          "Compressed bytes:  " & CompressedBytesString & " (" & CompressedBytes & "),  " & _
          "Percent savings:  " & PercentSavings & "%"

    ZipVBForm.AppendToStatusWindow(msg & vbCrLf, ForeColor:=Color.DodgerBlue)

    'MsgBox("In Finish callback", vbOKOnly, "Finish_Callback")

    ' Let any pending events go
    Application.DoEvents()

    ' Currently return value does nothing
    Finish_Callback = 0

  End Function


  ' If utf8_string NULL, return alternate_string,
  ' otherwise convert UTF-8 byte array in utf8_string to VB string
  Function utf8_to_string(ByVal utf8_string As String, _
                          ByVal alternate_string As String) As String
    Dim utf8_byte_array As Byte()
    Dim i As Int32
    'Dim j As Int32
    Dim c As Integer

    ' utf-8 name could be NULL if name just 7-bit ASCII
    If utf8_string Is Nothing Then
      utf8_to_string = alternate_string
      Exit Function
    End If

    ' allocate space for embedded bytes in utf8_string and set all to Chr(0)
    utf8_byte_array = System.Text.Encoding.UTF8.GetBytes(StrDup(65536, Chr(0)))

    ' load the array
    For i = 0 To Len(utf8_string) - 1
      utf8_byte_array(i) = Asc(Mid(utf8_string, i + 1, 1))
    Next

    ' convert UTF-8 byte array to VB string of wide characters
    utf8_to_string = System.Text.Encoding.UTF8.GetString(utf8_byte_array)

    ' resulting string should be no longer than original
    utf8_to_string = Mid(utf8_to_string, 1, Len(utf8_string))

    ' find first Chr(0) which should be just past last converted character
    For i = 1 To Len(utf8_to_string)
      c = Asc(Mid(utf8_to_string, i, 1))
      If c = 0 Then
        Exit For
      End If
    Next

    ' chop string at actual length
    utf8_to_string = Mid(utf8_to_string, 1, i - 1)

    'For j = 1 To Len(utf8_to_string)
    'c = Asc(Mid(utf8_to_string, j, 1))
    'utf8_to_string = utf8_to_string + "(" + Str(c) + ")"
    'Next

  End Function


  Function string_to_utf8(ByVal vb_string As String) As String
    Dim utf8_byte_array As Byte()
    Dim i As Int32

    utf8_byte_array = System.Text.Encoding.UTF8.GetBytes(vb_string)

    string_to_utf8 = ""

    For i = 1 To Len(utf8_byte_array)
      string_to_utf8 = string_to_utf8 + Chr(utf8_byte_array(i))
    Next

  End Function

End Module
