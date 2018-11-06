//Q4XTWNBZ JOB (EQ00,P),'AL DUNSMUIR',NOTIFY=&SYSUID,
//     MSGCLASS=T
/*JOBPARM S=XBAT
//*
//*-------------------------------------------------------------------*/
//* Encode ZIP LOADLIB using TSO XMIT                                 */
//*-------------------------------------------------------------------*/
//XMIT     EXEC PGM=IKJEFT01,DYNAMNBR=20
//DDIN     DD  DISP=SHR,DSN=PS1353.IZ.ZIP.V31.LOADLIB
//DDOUT    DD  DISP=OLD,DSN=PS1353.IZ.ZIP.V31.XMIT(ZIP)
//SYSPRINT DD  SYSOUT=*
//SYSTSPRT DD  SYSOUT=*
//SYSTSIN  DD  *
  TRANSMIT INFOZIP +
     NOCOPYLIST +
     DDNAME(DDIN) +
     NOEPILOG +
     NOLOG +
     NONOTIFY +
     OUTDDNAME(DDOUT) +
     PDS +
     NOPROLOG
//
