//Q4XTWNBZ JOB (EQ00,P),'AL DUNSMUIR',NOTIFY=&SYSUID,
//     MSGCLASS=T
/*JOBPARM S=XBAT
//*
//*-------------------------------------------------------------------*/
//* Delete previous ZIP source archive                                */
//*-------------------------------------------------------------------*/
//SEQDMDEL EXEC PGM=IDCAMS
//SYSPRINT DD SYSOUT=*
//SYSIN    DD *
 DELETE PS1353.IZ.ZIP.V31.SRC.ZIP
 SET MAXCC = 0
/*
//*
//*-------------------------------------------------------------------*
//* Create ZIP source archive
//*  Input:  EBCDIC C and H source, JCL files
//*  Output: ZIP file (w/ ZIPped ASCII translation)
//*-------------------------------------------------------------------*
//SEQDMZIP EXEC PGM=LEQZIP,PARM='-ajrl DD:ZIPFILE DD:C DD:H DD:JCL'
//C        DD DISP=SHR,DSN=PS1353.IZ.ZIP.V31.C
//H        DD DISP=SHR,DSN=PS1353.IZ.ZIP.V31.H
//JCL      DD DISP=SHR,DSN=PS1353.IZ.ZIP.V31.JCLLIB
//ZIPFILE  DD DISP=(NEW,KEEP),DSN=PS1353.IZ.ZIP.V31.SRC.ZIP,
//            SPACE=(10,(5,2),RLSE),AVGREC=M,
//            DCB=(RECFM=VB,LRECL=4096)
//SYSPRINT DD SYSOUT=*
