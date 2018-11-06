! ZIP_MSG.MSG
!
!    VMS Error Message Source File for Zip
!
!----------------------------------------------------------------------
! Copyright (c) 2007-2014 Info-ZIP.  All rights reserved.
!
! See the accompanying file LICENSE, version 2009-Jan-2 or later (the
! contents of which are also included in zip.h) for terms of use.  If,
! for some reason, all these files are missing, the Info-ZIP license
! may also be found at: ftp://ftp.info-zip.org/pub/infozip/license.html
!----------------------------------------------------------------------
!
! Because the facility code was formally assigned by HP, the .FACILITY
! directive below specifies /SYSTEM.  Because the messages are, in
! general, specific to Zip, this file is not compiled with /SHARED.
! For example:
!
!    MESSAGE /OBJECT = [.dest]ZIP_MSG.OBJ /NOSYMBOLS [.VMS]ZIP_MSG.MSG
!
!    LINK /SHAREABLE = [.dest]ZIP_MSG.EXE [.dest]ZIP_MSG.OBJ
!
!-----------------------------------------------------------------------

.TITLE  Info-ZIP Zip Error Messages
.FACILITY IZ_ZIP, 1955 /SYSTEM
.IDENT 'V3.1-000'

.BASE 0
OK          /SUCCESS  <Normal successful completion>
.BASE 4
EOF         /FATAL    <Unexpected end of zip file>
.BASE 6
FORM        /ERROR    <Zip file structure invalid>
.BASE 8
MEM         /FATAL    <Out of memory>
.BASE 10
LOGIC       /FATAL    <Internal logic error>
.BASE 12
BIG         /ERROR    <Entry too big to split, read, or write>
.BASE 14
NOTE        /ERROR    <Invalid comment format>
.BASE 16
TEST        /FATAL    <Zip file invalid, could not spawn unzip, or wrong unzip>
.BASE 18
ABORT       /ERROR    <Interrupted>
.BASE 20
TEMP        /FATAL    <Temporary file failure>
.BASE 22
READ        /FATAL    <Input file read failure>
.BASE 24
NONE        /WARNING  <Nothing to do!>
.BASE 26
NAME        /ERROR    <Missing or empty zip file>
.BASE 28
WRITE       /FATAL    <Output file write failure>
.BASE 30
CREAT       /FATAL    <Could not create output file>
.BASE 32
PARMS       /ERROR    <Invalid command arguments>
.BASE 36
OPEN        /ERROR    <File not found or no read permission>
.BASE 38
COMPILE     /FATAL    <Error in compilation options>
.BASE 40
ZIP64       /FATAL    <Attempt to read unsupported Zip64 archive>
.BASE 42
CRYPT       /ERROR    <Encryption error>
.BASE 44
COMPRESS    /ERROR    <Compression error>
.BASE 46
BACKUP      /ERROR    <Backup error>
.BASE 48
DEPTH       /ERROR    <Recursion depth limit exceeded>
.BASE 50
SPLIT       /ERROR    <Attempt to read unsupported split archive>

.END
