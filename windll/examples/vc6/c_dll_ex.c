/*
  Copyright (c) 1990-2014 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-02 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/
/*
 A very simplistic example of how to load the zip dll and make a call into it.
 Note that none of the command line options are implemented in this example.

 */

#ifndef WIN32
#  define WIN32
#endif
#define API

/* Tell Microsoft Visual C++ 2005 to leave us alone and
 * let us use standard C functions the way we're supposed to.
 */
#if defined(_MSC_VER) && (_MSC_VER >= 1400)
#  ifndef _CRT_SECURE_NO_DEPRECATE
#    define _CRT_SECURE_NO_DEPRECATE
#  endif
#  ifndef _CRT_NONSTDC_NO_DEPRECATE
#    define _CRT_NONSTDC_NO_DEPRECATE
#  endif
#endif

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <direct.h>

#include "c_dll_ex.h"
#include "../../../revision.h"

#if 0
/* This is no longer true now that zprintf and zfprintf implemented. */

/* printf get redirected by a macro define in api.h !!! */
# ifdef printf
#  undef printf
# endif

# ifdef fprintf
#  undef fprintf
# endif
#endif

#if 0
# ifdef WIN32
#  include <commctrl.h>
#  include <winver.h>
# else
#  include <ver.h>
# endif
#endif

#if 0

  /* Below no longer needed. */

# ifdef WIN32
#  define ZIP_DLL_NAME "ZIP32_DLL.DLL\0"
# else
# define ZIP_DLL_NAME "ZIP16.DLL\0"
# endif

# define DLL_WARNING "Cannot find %s."\
            " The Dll must be in the application directory, the path, "\
            "the Windows directory or the Windows System directory."
# define DLL_VERSION_WARNING "%s has the wrong version number."\
            " Insure that you have the correct dll's installed, and that "\
            "an older dll is not in your path or Windows System directory."

 int hFile;              /* file handle */

 ZCL ZpZCL;
 LPZIPUSERFUNCTIONS lpZipUserFunctions;
 HANDLE hZUF = (HANDLE)NULL;
 HINSTANCE hUnzipDll;
 HANDLE hFileList;
 ZPOPT ZpOpt;
# ifdef WIN32
 DWORD dwPlatformId = 0xFFFFFFFF;
# endif
 HINSTANCE hZipDll;

 /* Forward References */
 _DLL_ZIP ZipArchive;
 _ZIP_USER_FUNCTIONS ZipInit;

 void FreeUpMemory(void);
 int WINAPI DummyPassword(LPSTR, int, LPCSTR, LPCSTR);
 int WINAPI DummyPrint(char far *, unsigned long);
 int WINAPI WINAPI DummyComment(char far *);

#endif

#if 0
# ifdef WIN32
BOOL IsNT(VOID);
# endif
#endif

#if 0
  extern char *szCommentBuf;
#endif

long (EXPENTRY MyZipEComment)(char *oldcomment,
                              char *filename,
                              char *ufilename,
                              long maxcommentsize,
                              long *newcommentsize,
                              char *newcomment);

long (EXPENTRY MyZipAComment)(char *oldcomment,
                              long maxcommentsize,
                              long *newcommentsize,
                              char *newcomment);

long (EXPENTRY MyZipPassword)(long bufsize,
                              char *prompt,
                              char *password);

int (EXPENTRY MyZipPrint)(char *buf,
                          unsigned long size);

int GetVersionInfo();
char *argv_to_commandline(int argc, char *argv[]);



/****************************************************************************

    FUNCTION: Main(int argc, char **argv)

****************************************************************************/
int main(int argc, char **argv)
{
#if 0
  LPSTR szFileList;
  char **index, *sz;
  int retcode, i, cc;
  DWORD dwVerInfoSize;
  DWORD dwVerHnd;
  char szFullPath[PATH_MAX];
# ifdef WIN32
  char *ptr;
# else
  HFILE hfile;
  OFSTRUCT ofs;
# endif
  HANDLE  hMem;         /* handle to mem alloc'ed */
#endif

  int ret;
   
  ZIPUSERFUNCTIONS ZipUserFunctions, *lpZipUserFunctions;
#if 0
  char cmnt_buf[1024];
#endif
  char *commandline = NULL;
  char *currentdir = ".";           /* default to running in current directory */
  char *progress_size = "100k";     /* get progress callbacks every 100 KB processed */

  if (argc < 2)
  {
    printf("usage: c_dll_ex [zip command arguments]\n");
    return 99;           /* Exits if not proper number of arguments */
  }

  ret = GetVersionInfo();

  commandline = argv_to_commandline(0, argv);
  if (commandline == NULL)
  {
    printf("could not allocate commandline\n");
    return 99;
  }

  printf("commandline:  '%s'\n", commandline);
  
#if 0
  hZUF = GlobalAlloc(GPTR, (DWORD)sizeof(ZIPUSERFUNCTIONS));
  if (!hZUF)
  {
    return 0;
  }
  
  lpZipUserFunctions = (LPZIPUSERFUNCTIONS)GlobalLock(hZUF);
  if (!lpZipUserFunctions)
  {
    GlobalFree(hZUF);
    return 0;
  }
#endif

#if 0
  lpZipUserFunctions->print = DummyPrint;
  lpZipUserFunctions->password = DummyPassword;
  lpZipUserFunctions->acomment = DummyComment;
#endif

#if 0
    szCommentBuf = cmnt_buf;
#endif

  lpZipUserFunctions = &ZipUserFunctions;

  /* Any unused callbacks must be set to NULL.  Leave this
     block as is and add functions as needed below. */
  ZipUserFunctions.print = NULL;        /* gets most output to stdout */
  ZipUserFunctions.ecomment = NULL;     /* called to set entry comment */
  ZipUserFunctions.acomment = NULL;     /* called to set archive comment */
  ZipUserFunctions.password = NULL;     /* called to get password */
  ZipUserFunctions.split = NULL;        /* NOT IMPLEMENTED - DON'T USE */
  ZipUserFunctions.service = NULL;      /* called each entry */
  ZipUserFunctions.service_no_int64 = NULL; /* 32-bit version of service */
  ZipUserFunctions.progress = NULL;     /* provides entry progress */
  ZipUserFunctions.error = NULL;        /* called when warning/error */
  ZipUserFunctions.finish = NULL;       /* called when zip is finished */

  /* Set callbacks for the functions we're using. */
  ZipUserFunctions.print        = MyZipPrint;      /* See the Zip output */
  ZipUserFunctions.ecomment     = MyZipEComment;   /* Handle -c entry comment */
  ZipUserFunctions.acomment     = MyZipAComment;   /* Handle -z archive comment */
  ZipUserFunctions.password     = MyZipPassword;   /* Handle -e password */

  printf("--- calling ZpZip ------------------------------------\n");

  ret = ZpZip(commandline, currentdir, lpZipUserFunctions, progress_size);

  printf("------------------------------------------------------\n");
  printf("Zip returned:  %d\n", ret);

#if 0
  fprintf(stderr, " Calling ZpInit\n");
  ret = ZpInit( &ZipUserFunctions);
  fprintf( stderr, "  ZipInit() sts = %d (%%x%08x).\n", ret, ret);
#endif

#if 0
  fprintf(stderr, " Calling zipmain\n");
  fprintf(stderr, "-------\n");
  ret = zipmain(argc, argv);
  fprintf(stderr, "-------\n");
  fprintf(stderr, "  Zip returned = %d (%%x%08x).\n", ret, ret);

  fprintf(stderr, "\n");
  fprintf(stderr, " Getting version info\n");
  ret = GetVersionInfo();
#endif

  return ret;



#if 0

  /* Let's go find the dll */
  #ifdef WIN32
  if (SearchPath(
      NULL,               /* address of search path               */
      ZIP_DLL_NAME,       /* address of filename                  */
      NULL,               /* address of extension                 */
      PATH_MAX,           /* size, in characters, of buffer       */
      szFullPath,         /* address of buffer for found filename */
      &ptr                /* address of pointer to file component */
     ) == 0)
  #else
  hfile = OpenFile(ZIP_DLL_NAME,  &ofs, OF_SEARCH);
  if (hfile == HFILE_ERROR)
  #endif
     {
     char str[256];
     wsprintf (str, DLL_WARNING, ZIP_DLL_NAME);
     printf("%s\n", str);
     FreeUpMemory();
     return 0;
     }
  #ifndef WIN32
  else
     lstrcpy(szFullPath, ofs.szPathName);
  _lclose(hfile);
  #endif

  /* Now we'll check the zip dll version information */
  dwVerInfoSize =
      GetFileVersionInfoSize(szFullPath, &dwVerHnd);

  if (dwVerInfoSize)
     {
     BOOL  fRet, fRetName;
     char str[256];
     LPSTR   lpstrVffInfo; /* Pointer to block to hold info */
     LPSTR lszVer = NULL;
     LPSTR lszVerName = NULL;
     UINT  cchVer = 0;

     /* Get a block big enough to hold the version information */
     hMem          = GlobalAlloc(GMEM_MOVEABLE, dwVerInfoSize);
     lpstrVffInfo  = GlobalLock(hMem);

     /* Get the version information */
     GetFileVersionInfo(szFullPath, 0L, dwVerInfoSize, lpstrVffInfo);
     fRet = VerQueryValue(lpstrVffInfo,
                TEXT("\\StringFileInfo\\040904E4\\FileVersion"),
                 (LPVOID)&lszVer,
                 &cchVer);
     fRetName = VerQueryValue(lpstrVffInfo,
                 TEXT("\\StringFileInfo\\040904E4\\CompanyName"),
                (LPVOID)&lszVerName,
                &cchVer);
     if (!fRet || !fRetName ||
        (lstrcmpi(lszVer, ZIP_DLL_VERSION) != 0) ||
        (lstrcmpi(lszVerName, IZ_COMPANY_NAME) != 0))
        {
        wsprintf (str, DLL_VERSION_WARNING, ZIP_DLL_NAME);
        printf("%s\n", str);
        FreeUpMemory();
        return 0;
        }
     /* free memory */
     GlobalUnlock(hMem);
     GlobalFree(hMem);
     }
  else
     {
     char str[256];
     wsprintf (str, DLL_VERSION_WARNING, ZIP_DLL_NAME);
     printf("%s\n", str);
     FreeUpMemory();
     return 0;
     }
  /* Okay, now we know that the dll exists, and has the proper version
   * information in it. We can go ahead and load it.
   */
  hZipDll = LoadLibrary(ZIP_DLL_NAME);
  #ifndef WIN32
  if (hZipDll > HINSTANCE_ERROR)
  #else
  if (hZipDll != NULL)
  #endif
     {
     (_DLL_ZIP)ZipArchive = (_DLL_ZIP)GetProcAddress(hZipDll, "ZpArchive");
     if (!ZipArchive)
        {
        char str[256];
        wsprintf (str, "Could not get entry point to %s", ZIP_DLL_NAME);
        MessageBox((HWND)NULL, str, "Info-ZIP Example", MB_ICONSTOP | MB_OK);
        FreeUpMemory();
        return 0;
        }
     }
  else
     {
     char str[256];
     wsprintf (str, "Could not load %s", ZIP_DLL_NAME);
     printf("%s\n", str);
     FreeUpMemory();
     return 0;
     }

  (_ZIP_USER_FUNCTIONS)ZipInit = (_ZIP_USER_FUNCTIONS)GetProcAddress(hZipDll, "ZpInit");
  if (!ZipInit)
     {
     printf("Cannot get address of ZpInit in Zip dll. Terminating...");
     FreeLibrary(hZipDll);
     FreeUpMemory();
     return 0;
     }
  if (!(*ZipInit)(lpZipUserFunctions))
     {
     printf("Application functions not set up properly. Terminating...");
     FreeLibrary(hZipDll);
     FreeUpMemory();
     return 0;
     }

  /* Here is where the action starts */
  memset(&ZpOpt, 0, sizeof(ZpOpt));
  ZpOpt.ExcludeBeforeDate = NULL;    /* set to valid Zip date, or NULL */
  ZpOpt.IncludeBeforeDate = NULL;    /* set to valid Zip date, or NULL */
  ZpOpt.szRootDir = szFullPath;      /* set to root dir (will cd to), or NULL */
  ZpOpt.szTempDir = NULL;            /* set to dir for temp files, or NULL */
  ZpOpt.fUnicode = 0;                /* Unicode flag */
  ZpOpt.fEncrypt = FALSE;            /* Encrytion flag */
  ZpOpt.fSystem = FALSE;             /* true to include system/hidden files */
  ZpOpt.fVolume = FALSE;             /* true if storing volume label */
  ZpOpt.fExtra = FALSE;              /* true if including extra attributes */
  ZpOpt.fNoDirEntries = FALSE;       /* true if ignoring directory entries */
  ZpOpt.fVerbose = FALSE;            /* true if full messages wanted */
  ZpOpt.fQuiet = FALSE;              /* true if minimum messages wanted */
  ZpOpt.fCRLF_LF = FALSE;            /* true if translate CR/LF to LF */
  ZpOpt.fLF_CRLF = FALSE;            /* true if translate LF to CR/LF */
  ZpOpt.fJunkDir = FALSE;            /* true if junking directory names */
  ZpOpt.fGrow = FALSE;               /* true if allow appending to zip file */
  ZpOpt.fForce = FALSE;              /* true if making entries using DOS names */
  ZpOpt.fMove = FALSE;               /* true if deleting files added or updated */
  ZpOpt.fDeleteEntries = FALSE;      /* true if deleting files from archive */
  ZpOpt.fUpdate = FALSE;             /* true if updating zip file--overwrite only
                                          if newer */
  ZpOpt.fFreshen = FALSE;            /* true if freshening zip file--overwrite only */
  ZpOpt.fJunkSFX = FALSE;            /* true if junking sfx prefix*/
  ZpOpt.fLatestTime = FALSE;         /* true if setting zip file time to time of
                                         latest file in archive */
  ZpOpt.fComment = FALSE;            /* true if putting comment in zip file */
  ZpOpt.fOffsets = FALSE;            /* true if updating archive offsets for sfx
                                         files */
  ZpOpt.fPrivilege = 0;
  ZpOpt.fEncryption = 0;
  ZpOpt.szSplitSize = NULL;

  ZpOpt.szIncludeList = NULL;
  ZpOpt.IncludeListCount = 0;
  ZpOpt.IncludeList = NULL;
  ZpOpt.szExcludeList = NULL;
  ZpOpt.ExcludeListCount = 0;
  ZpOpt.ExcludeList = NULL;

  ZpOpt.fRecurse = 0;           /* subdir recursing mode: 1 = "-r", 2 = "-R" */
  ZpOpt.fRepair = 0;            /* archive repair mode: 1 = "-F", 2 = "-FF" */
  ZpOpt.fLevel = '6';           /* Default deflate compression level */
  ZpOpt.szCompMethod = NULL;
  for (i = 0; i < 8; i++) {
    ZpOpt.fluff[i] = 0;
  }
  getcwd(szFullPath, PATH_MAX); /* Set directory to current directory */

  ZpZCL.argc = argc - 2;        /* number of files to archive - adjust for the
                                    actual number of file names to be added */
  ZpZCL.lpszZipFN = argv[1];    /* archive to be created/updated */

  /* Copy over the appropriate portions of argv, basically stripping out argv[0]
     (name of the executable) and argv[1] (name of the archive file)
   */
  hFileList = GlobalAlloc( GPTR, 0x10000L);
  if ( hFileList )
     {
     szFileList = (char far *)GlobalLock(hFileList);
     }
  index = (char **)szFileList;
  cc = (sizeof(char *) * ZpZCL.argc);
  sz = szFileList + cc;

  for (i = 0; i < ZpZCL.argc; i++)
      {
      cc = lstrlen(argv[i+2]);
      lstrcpy(sz, argv[i+2]);
      index[i] = sz;
      sz += (cc + 1);
      }
  ZpZCL.FNV = (char **)szFileList;  /* list of files to archive */

  /* Go zip 'em up */
  retcode = ZipArchive(ZpZCL, &ZpOpt);
  if (retcode != 0)
     printf("Error in archiving\n");

  GlobalUnlock(hFileList);
  GlobalFree(hFileList);
  FreeUpMemory();
  FreeLibrary(hZipDll);
  return 1;
#endif
}

#if 0
  void FreeUpMemory(void)
  {
  if (hZUF)
     {
     GlobalUnlock(hZUF);
     GlobalFree(hZUF);
     }
  }

  #ifdef WIN32
  /* This simply determines if we are running on NT */
  BOOL IsNT(VOID)
  {
  if(dwPlatformId != 0xFFFFFFFF)
     return dwPlatformId;
  else
  /* note: GetVersionEx() doesn't exist on WinNT 3.1 */
     {
     if(GetVersion() < 0x80000000)
        {
        dwPlatformId = TRUE;
        }
     else
        {
        dwPlatformId = FALSE;
        }
      }
  return dwPlatformId;
  }
  #endif

#endif

#if 0
  /* Password entry routine - see password.c in the wiz directory for how
     this is actually implemented in Wiz. If you have an encrypted file,
     this will probably give you great pain. Note that none of the
     parameters are being used here, and this will give you warnings.
   */
  int WINAPI DummyPassword(LPSTR p, int n, LPCSTR m, LPCSTR name)
  {
  return 1;
  }

  /* Dummy "print" routine that simply outputs what is sent from the dll */
  int WINAPI DummyPrint(char far *buf, unsigned long size)
  {
  printf("%s", buf);
  return (unsigned int) size;
  }


  /* Dummy "comment" routine. See comment.c in the wiz directory for how
     this is actually implemented in Wiz. This will probably cause you
     great pain if you ever actually make a call into it.
   */
  int WINAPI DummyComment(char far *szBuf)
  {
  szBuf[0] = '\0';
  return TRUE;
  }

#endif


/* -------------------------------------------------------------------- */
/*
 * MyZipEComment(): Entry comment call-back function.
 *
 * Caller provides buffer (newcomment) to write comment to.
 *
 * Return 0 on fail (don't update comment), 1 on success.
 *
 * Replace the contents of this function as needed by your application.
 */

/* Get and set entry comment. */
long (EXPENTRY MyZipEComment)(char *oldcomment,
                              char *filename,
                              char *ufilename,
                              long maxcommentsize,
                              long *newcommentsize,
                              char *newcomment)
{
  int oldcommentsize = 0;
  char *comment;
  char newcommentstring[MAX_COM_LEN];

  if (oldcomment)
    oldcommentsize = strlen(oldcomment);

  fprintf(stderr, "EComment callback:  old comment for %s (%d bytes):\n    \"%s\"\n",
                  filename, oldcommentsize, oldcomment);

  /* update entry comment here */

  fprintf(stderr, "Enter comment for %s (or hit ENTER to keep old):\n", filename);

  comment = fgets(newcommentstring, MAX_PASSWORD_LEN, stdin);
  if (comment == NULL)
    return 0;

  *newcommentsize = strlen(newcommentstring);
  if (*newcommentsize > maxcommentsize - 1)
    *newcommentsize = maxcommentsize - 1;

  if (*newcommentsize && newcommentstring[*newcommentsize - 1] == '\n') {
    newcommentstring[*newcommentsize - 1] = '\0';
    *newcommentsize = strlen(newcommentstring);
  }

  if (*newcommentsize == 0)
    return 0;

  strncpy(newcomment, newcommentstring, *newcommentsize);
  newcomment[*newcommentsize] = '\0';

  fprintf(stderr, "EComment callback:  new comment for %s (%d bytes):\n    \"%s\"\n",
                  filename, *newcommentsize, newcomment);
  return 1;
}


/* -------------------------------------------------------------------- */
/*
 * MyZipAComment(): Archive comment call-back function.
 *
 * Caller provides buffer (newcomment) to write comment to.
 *
 * Return 0 if fail (don't update comment), 1 on success.
 *
 * Replace the contents of this function as needed by your application.
 */
long (EXPENTRY MyZipAComment)(char *oldcomment, long maxcommentsize,
                              long *newcommentsize, char *newcomment)
{
  int oldcommentsize = 0;
  char *comment;
  char newcommentstring[MAX_COM_LEN];

  if (oldcomment)
    oldcommentsize = strlen(oldcomment);

  fprintf(stderr, "AComment callback:  old archive comment (%d bytes):\n    \"%s\"\n",
                  oldcommentsize, oldcomment);

  /* update archive comment here */

  fprintf(stderr, "Enter zip file comment (or hit ENTER to keep old):\n");

  comment = fgets(newcommentstring, MAX_PASSWORD_LEN, stdin);
  if (comment == NULL)
    return 0;

  *newcommentsize = strlen(newcommentstring);
  if (*newcommentsize > maxcommentsize - 1)
    *newcommentsize = maxcommentsize - 1;

  if (*newcommentsize && newcommentstring[*newcommentsize - 1] == '\n') {
    newcommentstring[*newcommentsize - 1] = '\0';
    *newcommentsize = strlen(newcommentstring);
  }

  if (*newcommentsize == 0)
    return 0;

  strncpy(newcomment, newcommentstring, *newcommentsize);
  newcomment[*newcommentsize] = '\0';

  fprintf(stderr, "AComment callback:  new archive comment (%d bytes):\n    \"%s\"\n",
                  *newcommentsize, newcomment);

  return 1;
}


/* -------------------------------------------------------------------- */
/*
 * MyZipPassword(): Encryption password call-back function.
 *
 * Caller provides buffer (password) to write password to.
 *
 * bufsize  - max size of password, including terminating NULL
 * prompt   - either the "password" or "verify password" prompt text
 * password - buffer to write null-terminated password to
 *
 * Returns 0 on fail, 1 on success.
 *
 * Modify as needed for your application.
 */

long (EXPENTRY MyZipPassword)(long bufsize, char *prompt, char *password)
{
  char *passwd;
  char newpassword[MAX_PASSWORD_LEN];
  int passwordlen;

  /* here is where you would get the password from the user */

  fprintf(stderr, "Password callback:  bufsize            = %d\n", bufsize);
  fprintf(stderr, "Password callback:  prompt             = >%s<\n", prompt);

  fprintf(stderr, "Enter the password:\n");

  passwd = fgets(newpassword, MAX_PASSWORD_LEN, stdin);
  if (passwd == NULL)
    return 0;

  passwordlen = strlen(newpassword);
  if (passwordlen > bufsize - 1)
    passwordlen = bufsize - 1;

  if (passwordlen && newpassword[passwordlen - 1] == '\n')
    newpassword[passwordlen - 1] = '\0';

  strncpy(password, newpassword, passwordlen);
  password[passwordlen] = '\0';

  fprintf(stderr, "Password callback:  returning password = \"%s\"\n", password);

  return 1;
}


/* -------------------------------------------------------------------- */
/*
 * MyZipPrint(): Message output call-back function.
 *
 * This is where all normal output from Zip goes.  If you don't set
 * this callback, then set other callbacks, such as service, error, and
 * finish, to see what's going on.
 */

int (EXPENTRY MyZipPrint)(char *buf, unsigned long size)
{
#if 0
  fprintf(stderr, "\nzip output (%d bytes):  %s", size, buf);
  fprintf(stdout, "'%s", buf);
#endif
  fprintf(stdout, "%s", buf);
  return size;
}


/* -------------------------------------------------------------------- */

/* Get version info. */
int GetVersionInfo()
{
  ZpVer zip_ver;

  char zipversnum[100];
  char libdllinterfaceversnum[100];
  char c;
  int i;
  int j;
  char features[4000];

  if ((zip_ver.BetaLevel = (char *)malloc(10)) == NULL)
  {
    fprintf(stderr, "Could not allocate BetaLevel\n");
    return 2;
  }
  if ((zip_ver.Version = (char *)malloc(20)) == NULL)
  {
    fprintf(stderr, "Could not allocate Version\n");
    return 3;
  }
  if ((zip_ver.RevDate = (char *)malloc(20)) == NULL)
  {
    fprintf(stderr, "Could not allocate RevDate\n");
    return 4;
  }
  if ((zip_ver.RevYMD = (char *)malloc(20)) == NULL)
  {
    fprintf(stderr, "Could not allocate RevYMD\n");
    return 5;
  }
  if ((zip_ver.zlib_Version = (char *)malloc(10)) == NULL)
  {
    fprintf(stderr, "Could not allocate zlib_Version\n");
    return 6;
  }
  if ((zip_ver.szFeatures = (char *)malloc(4000)) == NULL)
  {
    fprintf(stderr, "Could not allocate szFeatures\n");
    return 7;
  }
  ZpVersion(&zip_ver);

  features[0] = '\0';
  for (i = 0, j = 0; c = zip_ver.szFeatures[i]; i++)
  {
    if (c == ';')
    {
      features[j++] = c;
      features[j++] = ' ';
    }
    else
    {
      features[j++] = c;
    }
  }
  features[j] = '\0';

  printf("--- get version of zip -----------\n");
  sprintf(zipversnum, "%d.%d.%d",
          zip_ver.zip.major, zip_ver.zip.minor, zip_ver.zip.patchlevel);
  sprintf(libdllinterfaceversnum, "%d.%d.%d",
          zip_ver.libdll_interface.major, zip_ver.libdll_interface.minor,
          zip_ver.libdll_interface.patchlevel);
  printf("  Zip version:       %s\n", zip_ver.Version);
  printf("  Zip version num:   %s %s\n", zipversnum,
                                                  zip_ver.BetaLevel);
  printf("  Zip rev date:      %s\n", zip_ver.RevDate);
  printf("  Zip rev YMD:       %s\n", zip_ver.RevYMD);
  printf("  LIB/DLL interface: %s\n", libdllinterfaceversnum);
  printf("  Feature list:      %s\n", features);
  printf("----------------------------------\n");

  free(zip_ver.BetaLevel);
  free(zip_ver.Version);
  free(zip_ver.RevDate);
  free(zip_ver.RevYMD);
  free(zip_ver.zlib_Version);
  free(zip_ver.szFeatures);

  return 0;
}


/* argv_to_commandline
 *
 * Combine args in argv[] into a single string, excluding argv[0].
 * If argc > 0, include up to argc arguments or until NULL arg
 * found.  If argc = 0, include all args up to NULL arg.
 *
 * Return commandline string or NULL if error.
 *
 * 2014-07-12 EG
 */
char *argv_to_commandline(int argc, char *argv[])
{
  int i;
  char *commandline = NULL;
  int total_size = 0;
  int j;
  int has_space;
  char c;

  /* add up total length needed */
  for (i = 1; (!argc || i < argc) && argv[i]; i++)
  {
    /* add size of arg and 2 for double quotes */
    total_size += (strlen(argv[i]) + 2) * sizeof(char);
    if (i > 1)
    {
      /* add 1 for space between args */
      total_size += 1 * sizeof(char);
    }
  }
  /* add 1 for string terminator */
  total_size += 1 * sizeof(char);

  /* allocate space for string */
  if ((commandline = malloc(total_size)) == NULL) {
    return NULL;
  }

  /* build command line string */
  commandline[0] = '\0';
  for (i = 1; (!argc || i < argc) && argv[i]; i++)
  {
    if (i > 1)
    {
      strcat(commandline, " ");
    }
    /* only quote if has space */
    has_space = 0;
    for (j = 0; c = argv[i][j]; j++)
    {
      if (isspace(c))
        has_space = 1;
    }
    if (has_space)
      strcat(commandline, "\"");
    strcat(commandline, argv[i]);
    if (has_space)
      strcat(commandline, "\"");
  }

  return commandline;
}
