VERSION 5.00
Begin VB.Form Form1 
   AutoRedraw      =   -1  'True
   Caption         =   "Form1"
   ClientHeight    =   7875
   ClientLeft      =   60
   ClientTop       =   1050
   ClientWidth     =   13470
   BeginProperty Font 
      Name            =   "MS Sans Serif"
      Size            =   9.75
      Charset         =   0
      Weight          =   700
      Underline       =   0   'False
      Italic          =   0   'False
      Strikethrough   =   0   'False
   EndProperty
   LinkTopic       =   "Form1"
   ScaleHeight     =   7875
   ScaleMode       =   0  'User
   ScaleWidth      =   16920.59
   Begin VB.CommandButton DoItButton 
      Caption         =   "Do it!"
      Default         =   -1  'True
      Height          =   375
      Left            =   12240
      TabIndex        =   4
      Top             =   480
      Width           =   975
   End
   Begin VB.TextBox CommandLineBox 
      BeginProperty Font 
         Name            =   "Lucida Console"
         Size            =   9.75
         Charset         =   0
         Weight          =   700
         Underline       =   0   'False
         Italic          =   0   'False
         Strikethrough   =   0   'False
      EndProperty
      Height          =   360
      Left            =   840
      TabIndex        =   1
      Top             =   480
      Width           =   11115
   End
   Begin VB.TextBox OutputBox 
      BeginProperty Font 
         Name            =   "Lucida Console"
         Size            =   9
         Charset         =   0
         Weight          =   400
         Underline       =   0   'False
         Italic          =   0   'False
         Strikethrough   =   0   'False
      EndProperty
      Height          =   6615
      Left            =   240
      Locked          =   -1  'True
      MultiLine       =   -1  'True
      ScrollBars      =   3  'Both
      TabIndex        =   0
      Text            =   "Vbzipfrm.frx":0000
      Top             =   960
      Width           =   12975
   End
   Begin VB.Label Label2 
      Caption         =   "zip"
      BeginProperty Font 
         Name            =   "Lucida Console"
         Size            =   9.75
         Charset         =   0
         Weight          =   700
         Underline       =   0   'False
         Italic          =   0   'False
         Strikethrough   =   0   'False
      EndProperty
      Height          =   375
      Left            =   360
      TabIndex        =   3
      Top             =   540
      Width           =   495
   End
   Begin VB.Label Label1 
      Caption         =   "Enter the Zip command to execute below:"
      ForeColor       =   &H00000113&
      Height          =   290
      Left            =   300
      TabIndex        =   2
      Top             =   25
      Width           =   6135
   End
End
Attribute VB_Name = "Form1"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False

Option Explicit

'---------------------------------------------------------------
'-- Sample VB 6 code to drive zip32_dll.dll
'--
'-- Based on code contributed to the Info-ZIP project by Mike Le Voi
'--
'-- See the Original VB example in the Zip 3.0 source kit for
'-- more information
'--
'-- Use this code at your own risk. Nothing implied or warranted
'-- to work on your machine :-)
'---------------------------------------------------------------
'--
'-- The Source Code Is Freely Available From Info-ZIP At:
'--   ftp://ftp.info-zip.org/pub/infozip/infozip.html
'-- and
'--   http://sourceforge.net/projects/infozip/
'--
'-- A Very Special Thanks To Mr. Mike Le Voi
'-- And Mr. Mike White Of The Info-ZIP project
'-- For Letting Me Use And Modify His Orginal
'-- Visual Basic 5.0 Code! Thank You Mike Le Voi.
'---------------------------------------------------------------
'--
'-- Contributed To The Info-ZIP Project By Raymond L. King
'-- Modified June 21, 1998
'-- By Raymond L. King
'-- Custom Software Designers
'--
'-- Contact Me At: king@ntplx.net
'-- ICQ 434355
'-- Or Visit Our Home Page At: http://www.ntplx.net/~king
'--
'---------------------------------------------------------------

'---------------------------------------------------------------
' zip32_dll.dll is the new Zip 3.1 dynamic library, replacing
' zip32.dll.  The API has greatly changed, and zip32_dll.dll is
' NOT a drop-in replacement for zip32.dll.  See the various text
' files in the windll directory for more on zip32_dll.dll,
' especially Readme_DLL_LIB.txt and WhatsNew_DLL_LIB.txt.  Also
' see the comments in VBZipBas.bas.
'
' There is also a Visual Studio 2010 VB example in this source
' kit.  Any modern VB application should start with that, not
' this VS 6 VB example.
'
' Contact Info-ZIP if problems.  This code is provided under the
' Info-ZIP license.
'
' Copyright (c) 2015 Info-ZIP.  All rights reserved.
'
' See the accompanying file LICENSE, version 2009-Jan-2 or later
' (the contents of which are also included in zip.h) for terms of use.
' If, for some reason, all these files are missing, the Info-ZIP license
' also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
'
' 4/24/2004, 12/4/2007, 5/22/2010, 7/19/2014, 9/23/2015 EG
'---------------------------------------------------------------

Private Sub DoItButton_Click()
  Dim RetCode As Integer
  
  Form1.Caption = "Zip32_dll.dll Example"
  
  CommandLine = CommandLineBox.text
  RetCode = VBZip32
  
End Sub

Private Sub Form_Clickx()

  Dim RetCode As Integer  ' For Return Code From zip32_dll.dll
  Dim CommandLine As String

  Cls

  '-- Set Options - Only The Common Ones Are Shown Here
  '-- These Must Be Set Before Calling The VBZip32 Function
  
  ' The updated dll now uses a command line interface instead of the
  ' old options structure.  Nearly all Zip commands are now available.
  ' Some commmon options are shown below, but see the Zip manual for
  ' the full list of what is now possible.
  
  ' There were bugs in the Zip 2.31 dll.  See the Zip 3.0 source kit
  ' for more on that.  Those bugs were fixed in the Zip 2.32 dll.
  
'  If Not SetZipOptions(ZOPT, _
'                       ZipMode:=Add, _
'                       ProgressReportChunkSize:="100k", _
'                       CompressionLevel:=c6_Default) Then
'           ' Some additional options ...
'           '            RootDirToZipFrom:="", _
'           '   strip paths and just store names:
'           '            JunkDirNames:=False, _
'           '   do not store entries for the directories themselves:
'           '            NoDirEntries:=True _
'           '   include files only if match one of these patterns:
'           '            i_IncludeFiles:="*.vbp *.frm", _
'           '   exclude files that match these patterns:
'           '            x_ExcludeFiles:="*.bas", _
'           '            Verboseness:=Verbose, _
'           '            IncludeEarlierThanDate:="2004-4-1", _
'           '            RecurseSubdirectories:=r_RecurseIntoSubdirectories, _
'           '            Encrypt:=False, _
'           '            ArchiveComment:=False
'           ' date example (format mmddyyyy or yyyy-mm-dd):
'           '           ExcludeEarlierThanDate:="2002-12-10", _
'           ' split example (can only create, can't update split archives in VB):
'           '            SplitSize:="110k", _
' Delete
' ' If Not SetZipOptions(ZOPT, _
' '                      ZipMode:=Delete) Then
'
'    ' a problem if get here - error message already displayed so just exit
'    Exit Sub
'  End If
  

  '-- Select Some Files - Wildcards Are Supported
  '-- Change The Paths Here To Your Directory
  '-- And Files!!!

  ' default to current (VB project) directory and zip up project files
 ' zZipArchiveName = "MyFirst.zip"
  
  
  ' Files to zip - use one of below
  
  '---------------
  ' Example using file name array
  
  ' Store the file paths
  ' Change Dim of zFiles at top of VBZipBas.bas if more than 100 files
  ' See note at top of VBZipBas.bas for limit on number of files
  
'  zArgc = 2           ' Number Of file paths below
'  zZipFileNames.zFiles(1) = "*.bas"
'  zZipFileNames.zFiles(2) = "*.frm"
  
  '---------------
  ' Example using file list string
  
  ' List of files to zip as string of names with space between
  ' Set zArgc = 0 as not using array
  ' Using string for file list avoids above array limit
  
'  zArgc = 0
''  ReDim FilesToZip(1)      ' dim to number of files below
''  FilesToZip(1) = "x:*.*"
'  ReDim FilesToZip(2)      ' dim to number of files below
'  FilesToZip(1) = "*.bas"
'  FilesToZip(2) = "*.frm"
 
  ' Build string of file names
  ' Best to put double quotes around each in case of spaces
'  strZipFileNames = ""
'  For i = 1 To UBound(FilesToZip)
'    strZipFileNames = strZipFileNames & """" & FilesToZip(i) & """ "
'  Next
'  '---------------
'
'  '-- Go Zip Them Up!
'  RetCode = VBZip32()

'  '-- Display The Returned Code Or Error!
'  Print "Return code:" & str(RetCode)

End Sub

Private Sub Form_Load()

  Me.Show

  'Print "Click me!"

End Sub

