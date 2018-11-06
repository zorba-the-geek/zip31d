' Example VS 2010 Visual Basic code showing how to call Zip and set up callbacks to
' handle progress tracking and error reporting.
'
' 1 June 2014  (Last updated 31 August 2015)
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
' This example is written using the newer DLL interfacing introduced around
' VS 2008.  This example probably will not work using VS 6 VB or earlier.
' See the example in the vb6 directory for a Windows XP compatible example.

Imports System.Runtime.InteropServices

Public Class ZipVBForm

  Public ButtonBackColor As Color
  Public AbortButtonText As String = "Abort!"
  Public AbortButtonTextAborting As String = "Aborting!"


  Public Sub AppendText(ByVal box As RichTextBox, ByVal text As String, _
                        Optional ByVal ForeColor As Color = Nothing, _
                        Optional ByVal BackColor As Color = Nothing)
    Dim textstart As Integer
    Dim textend As Integer

    textstart = box.TextLength
    box.AppendText(text)
    textend = box.TextLength

    ' Textbox may transform chars, so (end-start) != text.Length
    box.Select(textstart, textend - textstart)
    If ForeColor <> Nothing Then
      box.SelectionColor = ForeColor
    End If
    If BackColor <> Nothing Then
      box.SelectionBackColor = BackColor
    End If
    ' could set box.SelectionBackColor, box.SelectionFont too.
    box.SelectionLength = 0
    box.ScrollToCaret()
  End Sub

  Public Sub AppendToStatusWindow(ByVal text As String, _
                                  Optional ByVal ForeColor As Color = Nothing, _
                                  Optional ByVal BackColor As Color = Nothing)
    AppendText(StatusWindow, text, ForeColor, BackColor)
  End Sub

  Public Sub ClearStatusWindow()
    StatusWindow.Clear()
  End Sub



  <StructLayout(LayoutKind.Sequential)> Public Structure TestStruct
    Public a As Integer
    Public s As String
    <VBFixedArray(9)> Public b() As Byte
    <VBFixedString(9)> Public s10 As String
  End Structure


  Private Function Get_Zip_Version() As Boolean
    Dim v As VerType
    Dim Feature As String
    Dim sc As Integer
    Dim FeatureList As String
    '   Dim FirstOne As Boolean


    Get_Zip_Version = False

    ZVersion.StructLen = 0
    ZVersion.Flags = 0
    ZVersion.BetaLevel = "1234567890"           ' Setting this to this string allocates space for 10 characters
    ZVersion.Version = "12345678901234567890"   ' Allocate 20 characters
    ZVersion.RevDate = "12345678901234567890"   ' Allocate 20 characters
    ZVersion.RevYMD = "1234567890"              ' Allocate 10 characters
    ZVersion.ZLibVersion = "1234567890"         ' Allocate 10 characters
    ZVersion.Encryption = 0                     ' Not set
    ZVersion.ZipVersion = v                     ' Version of Zip
    ZVersion.OS2DLLVersion = v                  ' No OS2 support yet so not used
    ZVersion.LIBDLLVersion = v                  ' Zip version DLL interface compatible with
    ZVersion.OptStrucSize = 0                   ' Size of expected Options structure (if used)
    ZVersion.FeatureList = StrDup(4000, "-")    ' List of features implemented in this DLL
    ' .PadRight(4000, ".")

    On Error GoTo Version_Error

    ZpVersion(ZVersion)
    GoTo Version_Continue

Version_Error:
    If Err.Number = 53 Then
      MsgBox("DLL error:" & Chr(13) & Err.Description & Chr(13) & Chr(13) & _
             "Make sure zip32_dll.dll is in windll/examples/zip31vb directory")
    Else
      MsgBox("Error:" & Chr(13) & Err.Description & Chr(13) & "Number " & _
             Err.Number)
    End If
    Exit Function

Version_Continue:
    BetaFlag = ZVersion.Flags And 1
    ZLIBFlag = ZVersion.Flags And 2
    Zip64Flag = ZVersion.Flags And 4
    EncryptionFlag = ZVersion.Encryption And 1
    ZipVersionString = ZVersion.ZipVersion.Major & "." _
      & ZVersion.ZipVersion.Minor & "." & ZVersion.ZipVersion.PatchLevel
    WinDLLVersionString = ZVersion.LIBDLLVersion.Major & "." _
      & ZVersion.LIBDLLVersion.Minor & "." & ZVersion.LIBDLLVersion.PatchLevel
    WinDLLMajorMinorVersion = ZVersion.LIBDLLVersion.Major & "." _
      & ZVersion.LIBDLLVersion.Minor

    FeatureList = ZVersion.FeatureList
    ParsedFeatureList = ""
    If Mid(FeatureList, 1, 1) = ";" Then
      FeatureList = Mid(FeatureList, 2)
    End If
    sc = InStr(FeatureList, ";")
    'FirstOne = True
    While sc > 0
      Feature = Mid(FeatureList, 1, sc - 1)
      ' store the feature
      FeatureCount = FeatureCount + 1
      Features(FeatureCount) = Feature
      ' add to list to display
      FeatureList = Mid(FeatureList, sc + 1)
      'If FirstOne Then
      '  ParsedFeatureList = ParsedFeatureList & Feature & vbCrLf
      'Else
      ParsedFeatureList = ParsedFeatureList _
        & Chr(9) & Feature & vbCrLf
      '& "                          " & Feature & vbCrLf
      'End If
      'FirstOne = False
      sc = InStr(FeatureList, ";")
    End While

    Get_Zip_Version = True

  End Function


  Private Sub ZpVersion_Button_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ZpVersion_Button.Click

    If Get_Zip_Version() = False Then
      Exit Sub
    End If

    AppendToStatusWindow("Zip DLL version information:" & vbCrLf, ForeColor:=Color.DarkCyan)

    AppendToStatusWindow( _
               "  This is a Beta =        " & BetaFlag & vbCrLf & _
               "  ZLIB used =             " & ZLIBFlag & vbCrLf & _
               "  Zip64 enabled =         " & Zip64Flag & vbCrLf & _
               "  Encryption available =  " & EncryptionFlag & vbCrLf & _
               "  BetaLevel =             " & ZVersion.BetaLevel & vbCrLf & _
               "  Version =               " & ZVersion.Version & vbCrLf & _
               "  RevDate =               " & ZVersion.RevDate & vbCrLf & _
               "  RevYMD =                " & ZVersion.RevYMD & vbCrLf & _
               "  ZLib Version =          " & ZVersion.ZLibVersion & vbCrLf & _
               "  Zip Version =           " & ZipVersionString & vbCrLf & _
               "  LIB/DLL Interface =     " & WinDLLVersionString & vbCrLf & _
               "  Feature List =          " & vbCrLf & ParsedFeatureList & vbCrLf)

    ' This is no longer used, so no need to report it.
    '
    '               "  Option Struct Size =    " & ZVersion.OptStrucSize & vbCrLf & _

    MsgBox("ZpVersion returned:" & vbCrLf & _
           "  This is a Beta =        " & Chr(9) & BetaFlag & vbCrLf & _
           "  ZLIB used =             " & Chr(9) & ZLIBFlag & vbCrLf & _
           "  Zip64 enabled =         " & Chr(9) & Zip64Flag & vbCrLf & _
           "  Encryption available =  " & Chr(9) & EncryptionFlag & vbCrLf & _
           "  BetaLevel =             " & Chr(9) & ZVersion.BetaLevel & vbCrLf & _
           "  Version =               " & Chr(9) & ZVersion.Version & vbCrLf & _
           "  RevDate =               " & Chr(9) & ZVersion.RevDate & vbCrLf & _
           "  RevYMD =                " & Chr(9) & ZVersion.RevYMD & vbCrLf & _
           "  ZLIB Version =          " & Chr(9) & ZVersion.ZLibVersion & vbCrLf & _
           "  Zip Version =           " & Chr(9) & ZipVersionString & vbCrLf & _
           "  WinDLL Interface =      " & Chr(9) & WinDLLVersionString & vbCrLf & _
           "  Feature List =          " & vbCrLf & ParsedFeatureList, _
            MsgBoxStyle.OkOnly, "ZpVersion")

    '           "  Option Struct Size =    " & Chr(9) & ZVersion.OptStrucSize & vbCrLf & _

  End Sub




  Private Sub ZpTestComParse_Button_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ZpTestComParse_Button.Click
    Dim ParsedLine As String
    Dim argc As Integer

    ParsedLine = StrDup(4000, ".")

    argc = ZpTestComParse(ZipCommandLine.Text, ParsedLine)

    MsgBox("argc = " & argc & vbCrLf & _
           "ParsedLine = '" & Trim(ParsedLine) & "'", _
           MsgBoxStyle.OkOnly, "ZpTestComParse")
  End Sub

  Private Sub ZpTestCallback_Button_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ZpTestCallback_Button.Click

    'ZpTestCallback(Nothing)

    ZpTestCallback(AddressOf ZpVbCallback)

  End Sub

  Sub ZpVbCallback(ByVal parm As IntPtr)
    Dim sparm As String

    'Dim enc As New System.Text.ASCIIEncoding

    sparm = Marshal.PtrToStringAnsi(parm)

    '    sparm = enc.GetString(parm)

    '    sparm = TextCB(parm)

    'MsgBox("Inside VB callback function!")
    '    sparm = StrPtr(parm)
    '    sparm = AtoS(parm)
    MsgBox("From Zip DLL:  " & sparm)

  End Sub



  Private Sub ZpTestZip_Button_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ZpTestZip_Button.Click

    Dim Command_Line As String
    Dim Current_Directory As String
    Dim Callback_Functions As Callback_Functions_Structure
    Dim ZipReturn As Long

    Command_Line = ZipCommandLine.Text

    Current_Directory = RootDir_Box.Text

    Callback_Functions = New Callback_Functions_Structure

    Callback_Functions.Print_Function = Nothing
    Callback_Functions.EntryComment_Function = Nothing
    Callback_Functions.ArchiveComment_Function = Nothing
    Callback_Functions.Password_Function = Nothing
    Callback_Functions.Split_Function = Nothing
    Callback_Functions.Service_Function = Nothing
    Callback_Functions.Service_No_Int64_Function = Nothing
    Callback_Functions.ProgressReport_Function = Nothing
    Callback_Functions.ErrorReport_Function = Nothing
    Callback_Functions.FinishReport_Function = Nothing

    Callback_Functions.Print_Function = AddressOf Print_Callback
    Callback_Functions.EntryComment_Function = AddressOf EntryComment_Callback
    Callback_Functions.ArchiveComment_Function = AddressOf ArchiveComment_Callback
    Callback_Functions.Password_Function = AddressOf Password_Callback
    Callback_Functions.Service_Function = AddressOf Service_Callback
    Callback_Functions.ProgressReport_Function = AddressOf Progress_Callback
    Callback_Functions.ErrorReport_Function = AddressOf Error_Callback
    Callback_Functions.FinishReport_Function = AddressOf Finish_Callback
    
    ZipReturn = ZpZipTest(Command_Line, Current_Directory, Callback_Functions)

    AppendText(StatusWindow, "Zip returned " & ZipReturn & vbCrLf & vbCrLf, ForeColor:=Color.DarkBlue)
    'MsgBox("Zip returned = " & ZipReturn, vbOKOnly, "ZipTestZip")

    If AbortNow Then
      Clear_Abort()
    End If

  End Sub

  Private Sub ZpTestStructFunc_Button_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles ZpTestStructFunc_Button.Click

    Dim Callback_Function As New Test_Callback_Function_Structure

    Callback_Function.print = Nothing
    'Callback_Function.print = AddressOf MyCallback

    ZpTestCallbackStruct(Callback_Function)

    MsgBox("ZpTestStructFunc returned", vbOKOnly, "ZpTestStructFunc")

  End Sub

  Private Sub ZipVBForm_Load(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.Load

    RootDir_Box.Text = Application.StartupPath

    ButtonBackColor = Abort_Button.BackColor
    Abort_Button.Text = AbortButtonText

  End Sub

  Private Sub Abort_Button_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Abort_Button.Click

    Dim reply As Integer

    If Not Running Then
      MsgBox("Zip is not currently running")
      Exit Sub
    End If

    reply = MsgBox("Really abort Zip operation?", vbYesNo, "Zip Abort")

    If reply = vbYes Then
      ZipVbModule.AbortNow = True
      Abort_Button.Text = AbortButtonTextAborting
      Abort_Button.BackColor = Color.Red
    End If

    ' It's possible Zip just finished
    If Not Running Then
      Clear_Abort()
    End If
  End Sub

  Public Sub Clear_Abort()

    AbortNow = False

    ' reset button text and backcolor
    Abort_Button.Text = AbortButtonText
    Abort_Button.BackColor = ButtonBackColor

  End Sub

  Private Sub DoIt_Button_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles DoIt_Button.Click

    Dim Command_Line As String
    Dim Current_Directory As String
    Dim Callback_Functions As Callback_Functions_Structure
    Dim ZipReturn As Long
    Dim progress_chunk_size_string As String ' how many bytes before next progress report (must be set if ProgressReport not NULL)


    If Get_Zip_Version() = False Then
      Exit Sub
    End If

    If (WinDLLMajorMinorVersion <> CompatibleWinDLLMajorMinorVersion) Then
      AppendToStatusWindow("The version of the Zip DLL is not compatible with this example application" & vbCrLf & _
             "  Version of DLL:       " & WinDLLMajorMinorVersion & vbCrLf & _
             "  This example needs:   " & CompatibleWinDLLMajorMinorVersion, Color.Red)
      MsgBox("The version of the Zip DLL is not compatible with this example application" & vbCrLf & _
             "  Version of DLL:       " & Chr(9) & WinDLLMajorMinorVersion & vbCrLf & _
             "  This example needs:   " & Chr(9) & CompatibleWinDLLMajorMinorVersion, vbOKOnly, "DLL Not Compatible")
      Exit Sub
    End If

    Command_Line = ZipCommandLine.Text

    'If UnicodeZipOutput_CheckBox.Checked And Len(Command_Line) > 0 Then
    '  Command_Line = "-UN=s " + Command_Line
    'End If

    ' Display the command line
    AppendText(StatusWindow, "zip " & Command_Line & vbCrLf, ForeColor:=Color.DarkCyan)

    ' Set the root directory
    Current_Directory = RootDir_Box.Text

    ' Structure for telling Zip the addresses of used callback functions
    Callback_Functions = New Callback_Functions_Structure

    ' Set all unused callbacks to Nothing
    Callback_Functions.Print_Function = Nothing
    Callback_Functions.EntryComment_Function = Nothing
    Callback_Functions.ArchiveComment_Function = Nothing
    Callback_Functions.Password_Function = Nothing
    Callback_Functions.Split_Function = Nothing
    Callback_Functions.Service_Function = Nothing
    Callback_Functions.Service_No_Int64_Function = Nothing
    Callback_Functions.ProgressReport_Function = Nothing
    Callback_Functions.ErrorReport_Function = Nothing
    Callback_Functions.FinishReport_Function = Nothing

    ' Set callbacks we are using to the addresses of the functions
    Callback_Functions.Print_Function = AddressOf Print_Callback
    Callback_Functions.EntryComment_Function = AddressOf EntryComment_Callback
    Callback_Functions.ArchiveComment_Function = AddressOf ArchiveComment_Callback
    Callback_Functions.Password_Function = AddressOf Password_Callback
    Callback_Functions.Service_Function = AddressOf Service_Callback
    Callback_Functions.ProgressReport_Function = AddressOf Progress_Callback
    Callback_Functions.ErrorReport_Function = AddressOf Error_Callback
    Callback_Functions.FinishReport_Function = AddressOf Finish_Callback

    ' Number bytes to process before sending next progress report
    ' In form NM where N is a number and optional M multiplier {k,m,g,t}
    ' So 10k tells Zip to call the ProgressReport callback:
    '   - At the start of processing (% done = 0%)
    '   - After every 10k bytes read from input file (0 < % done < 100%)
    '   - After processing for the entry is completed (% done = 100%)
    ' This also sets how often the abort flag is checked.  Ideally this should be
    ' large enough not to seriously slow processing, but small enough to show
    ' steady progress and allow a reasonably quick abort.  Probably 100m is
    ' a reasonable value.  We set it to 10k here to allow testing with small
    ' files.
    progress_chunk_size_string = "10k"

    ' Call Zip!
    Running = True
    ZipReturn = ZpZip(Command_Line, Current_Directory, Callback_Functions, progress_chunk_size_string)
    Running = False

    ' Display the return code
    AppendText(StatusWindow, "Zip returned " & ZipReturn & vbCrLf & vbCrLf, ForeColor:=Color.DarkBlue)

    'MsgBox("Zip returned:  " & ZipReturn, vbOKOnly, "ZpZip")

    If AbortNow Then
      ' Clear any abort request
      Clear_Abort()
    End If

  End Sub

End Class
