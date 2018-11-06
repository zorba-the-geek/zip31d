/*
  windll/windll.c - Zip 3

  Copyright (c) 1990-2014 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-02 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
 *  windll.c by Mike White loosly based on Mark Adler's zip.c
 */
#include "../zip.h"
#include <windows.h>
#include <process.h>
#include <signal.h>
#include <stdarg.h>
#include <ctype.h>
#include "windll.h"

HINSTANCE hCurrentInst;
#ifdef ZIPLIB
/*  DLL Entry Point */
#ifdef __BORLANDC__
#pragma argsused
/* Borland seems to want DllEntryPoint instead of DllMain like MSVC */
#define DllMain DllEntryPoint
#endif
#ifdef WIN32
BOOL WINAPI DllMain( HINSTANCE hInstance,
                     DWORD dwReason,
                     LPVOID plvReserved)
#else
int WINAPI LibMain( HINSTANCE hInstance,
                        WORD wDataSegment,
                        WORD wHeapSize,
                        LPSTR lpszCmdLine )
#endif
{
#ifndef WIN32
/* The startup code for the DLL initializes the local heap(if there is one)
 with a call to LocalInit which locks the data segment. */

if ( wHeapSize != 0 )
   {
   UnlockData( 0 );
   }
hCurrentInst = hInstance;
return 1;   /* Indicate that the DLL was initialized successfully. */
#else
BOOL rc = TRUE;
switch( dwReason )
   {
   case DLL_PROCESS_ATTACH:
      // DLL is loaded. Do your initialization here.
      // If cannot init, set rc to FALSE.
      hCurrentInst = hInstance;
      break;

   case DLL_PROCESS_DETACH:
      // DLL is unloaded. Do your cleanup here.
      break;
   default:
      break;
   }
return rc;
#endif
}

#ifdef __BORLANDC__
#pragma argsused
#endif
int FAR PASCAL WEP ( int bSystemExit )
{
return 1;
}
#endif /* ZIPLIB */

LPSTR szCommentBuf;
HANDLE hStr;

#if 0
void comment(unsigned int comlen)
{
#if 0
unsigned int i;
#endif
long maxcomlen = 32766;
long newcomlen = 0;

#if 0
printf("WINDLL comment\n");
#endif

/* The entire record must be less than 65535, so the comment length needs to be
   kept well below that.  Changed 65534L to 32767L.  EG */
if (comlen > 32766L)
   comlen = (unsigned int) 32766L;

#if 0
hStr = GlobalAlloc( GPTR, (DWORD)32767L);
if ( !hStr )
   {
   hStr = GlobalAlloc( GPTR, (DWORD) 2);
   szCommentBuf = GlobalLock(hStr);
   szCommentBuf[0] = '\0';
   return;
   }

szCommentBuf = GlobalLock(hStr);
if (comlen && zcomment)
   {
   for (i = 0; i < comlen; i++)
       szCommentBuf[i] = zcomment[i];
   szCommentBuf[comlen] = '\0';
   }
else
   szCommentBuf[0] = '\0';
if (zcomment)
  free(zcomment);
zcomment = (char *)malloc(1);
*zcomment = 0;
if (lpZipUserFunctions->comment) {
lpZipUserFunctions->comment(szCommentBuf, comlen);
#endif

if (zcomment == NULL) {
  /* create zero length string to pass to callback */
  if ((zcomment = (char *)malloc(1)) == NULL) {
    ZIPERR(ZE_MEM, "getting comment from callback (1)");
  }
  *zcomment = 0;
  comlen = 0;
} else {
  /* make sure zcomment is NULL terminated */
  zcomment[comlen] = '\0';
}

if (lpZipUserFunctions->acomment) {
  szCommentBuf = lpZipUserFunctions->acomment(zcomment, maxcomlen, &newcomlen);
  
  if (newcomlen > maxcomlen)
    newcomlen = maxcomlen;
  szCommentBuf[newcomlen] = '\0';

  if (zcomment)
    free(zcomment);
  if ((zcomment = (char *)malloc((newcomlen + 1) * sizeof(char))) == NULL) {
    ZIPERR(ZE_MEM, "getting comment from callback (2)");
  }

  zcomlen = (ush)newcomlen;
  strcpy(zcomment, szCommentBuf);

#if 0
  for (i = 0; i < newcomlen && szCommentBuf[i]; i++)
    zcomment[i] = szCommentBuf[i];
  zcomment[i] = '\0';
#endif

}
return;
}
#endif

#define STDIO_BUF_SIZE 16384

#if 0
int __far __cdecl printf(const char *format, ...)
{
va_list argptr;
HANDLE hMemory;
LPSTR pszBuffer;
int len;

va_start(argptr, format);
hMemory = GlobalAlloc(GMEM_MOVEABLE, STDIO_BUF_SIZE);
WinAssert(hMemory);
if (!hMemory)
   {
   return 0;
   }
pszBuffer = (LPSTR)GlobalLock(hMemory);
WinAssert(pszBuffer);
len = wvsprintf(pszBuffer, format, argptr);
va_end(argptr);
WinAssert(strlen(pszBuffer) < STDIO_BUF_SIZE);
if (lpZipUserFunctions->print) {
  len = lpZipUserFunctions->print(pszBuffer, len);
}
else
{
  len = write(fileno(stdout),(char far *)(pszBuffer), len);
}
GlobalUnlock(hMemory);
GlobalFree(hMemory);
return len;
}

/* fprintf clone for code in zip.c, etc. */
int __far __cdecl fprintf(FILE *file, const char *format, ...)
{
va_list argptr;
HANDLE hMemory;
LPSTR pszBuffer;
int len;

va_start(argptr, format);
hMemory = GlobalAlloc(GMEM_MOVEABLE, STDIO_BUF_SIZE);
WinAssert(hMemory);
if (!hMemory)
   {
   return 0;
   }
pszBuffer = GlobalLock(hMemory);
WinAssert(pszBuffer);
len = wvsprintf(pszBuffer, format, argptr);
va_end(argptr);
WinAssert(strlen(pszBuffer) < STDIO_BUF_SIZE);
if ((file == stderr) || (file == stdout))
   {
     if (lpZipUserFunctions->print) {
       len = lpZipUserFunctions->print(pszBuffer, len);
     }
     else
     {
       len = write(fileno(stdout),(char far *)(pszBuffer), len);
     }
   }
else
   len = write(fileno(file),(char far *)(pszBuffer), len);
GlobalUnlock(hMemory);
GlobalFree(hMemory);
return len;
}
#endif

#if 0
void __far __cdecl perror(const char *parm1)
{
  zprintf("%s", parm1);
}
#endif


