/*
  windll/windll.h - Zip 3

  Copyright (c) 1990-2014 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
 WiZ 1.0 header file for zip dll
*/

/* Note that WiZ has not yet been updated to work with the Zip 3.1 API.  This
   is planned shortly. */

#ifndef _WINDLL_H
#define _WINDLL_H

#include "structs.h"

#ifndef MSWIN
#define MSWIN
#endif

#if 0
# ifndef USE_ZIPMAIN
#   define USE_ZIPMAIN
# endif
#endif

#ifndef NDEBUG
#  define WinAssert(exp) \
        {\
        if (!(exp))\
            {\
            char szBuffer[40];\
            sprintf(szBuffer, "File %s, Line %d",\
                    __FILE__, __LINE__) ;\
            if (IDABORT == MessageBox((HWND)NULL, szBuffer,\
                "Assertion Error",\
                MB_ABORTRETRYIGNORE|MB_ICONSTOP))\
                    FatalExit(-1);\
            }\
        }

#else
#  define WinAssert(exp)
#endif

#define cchFilesMax 4096

#if 0
extern int WINAPI ZpArchive(ZCL C, LPZPOPT Opts);
#endif
extern HWND hGetFilesDlg;
extern char szFilesToAdd[80];
extern char rgszFiles[cchFilesMax];
BOOL WINAPI CommentBoxProc(HWND hwndDlg, WORD wMessage, WPARAM wParam, LPARAM lParam);
BOOL PasswordProc(HWND, WORD, WPARAM, LPARAM);
void CenterDialog(HWND hwndParent, HWND hwndDlg);
#if 0
void comment(unsigned int);
#endif

extern LPSTR szCommentBuf;
extern HANDLE hStr;
extern HWND hWndMain;
#if 0
void __far __cdecl perror(const char *);
#endif

#endif /* _WINDLL_H */

