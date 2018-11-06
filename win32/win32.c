/*
  win32/win32.c - Zip 3.1

  Copyright (c) 1990-2015 Info-ZIP.  All rights reserved.

  See the accompanying file LICENSE, version 2009-Jan-2 or later
  (the contents of which are also included in zip.h) for terms of use.
  If, for some reason, all these files are missing, the Info-ZIP license
  also may be found at:  ftp://ftp.info-zip.org/pub/infozip/license.html
*/

/*
 * WIN32 specific functions for ZIP.
 *
 * The WIN32 version of ZIP heavily relies on the MSDOS and OS2 versions,
 * since we have to do similar things to switch between NTFS, HPFS and FAT.
 */


#include "../zip.h"

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <time.h>
#include <ctype.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifndef ZIP_DLL_LIB
/* for CommandLineToArgvW() */
# include <shellapi.h>
#endif

/* for LARGE_FILE_SUPPORT but may not be needed */
#include <io.h>
/* Defines _SH_DENYNO */
#include <share.h>

#ifdef __RSXNT__
#  include <alloca.h>
#  include "../win32/rsxntwin.h"
#endif
#include "../win32/win32zip.h"

#define A_RONLY    0x01
#define A_HIDDEN   0x02
#define A_SYSTEM   0x04
#define A_LABEL    0x08
#define A_DIR      0x10
#define A_ARCHIVE  0x20


#define EAID     0x0009


#if (defined(__MINGW32__) && !defined(USE_MINGW_GLOBBING))
   int _CRT_glob = 0;   /* suppress command line globbing by C RTL */
#endif

#ifndef UTIL

#ifdef NT_TZBUG_WORKAROUND
local int FSusesLocalTime(const char *path);
# ifdef UNICODE_SUPPORT
local int FSusesLocalTimeW(const wchar_t *path);
# endif
#endif
#if (defined(USE_EF_UT_TIME) || defined(NT_TZBUG_WORKAROUND))
local int FileTime2utime(FILETIME *pft, time_t *ut);
#endif
#if (defined(NT_TZBUG_WORKAROUND) && defined(W32_STAT_BANDAID))
local int VFatFileTime2utime(const FILETIME *pft, time_t *ut);
#endif


/* FAT / HPFS detection */

int IsFileSystemOldFAT(char *dir)
{
  static char lastDrive = '\0';    /* cached drive of last GetVolumeInformation call */
  static int lastDriveOldFAT = 0;  /* cached OldFAT value of last GetVolumeInformation call */
  char root[4];
  DWORD vfnsize;
  DWORD vfsflags;

    /*
     * We separate FAT and HPFS+other file systems here.
     * I consider other systems to be similar to HPFS/NTFS, i.e.
     * support for long file names and being case sensitive to some extent.
     */

    strncpy(root, dir, 3);
    if ( isalpha((uch)root[0]) && (root[1] == ':') ) {
      root[0] = to_up(dir[0]);
      root[2] = '\\';
      root[3] = 0;
    }
    else {
      root[0] = '\\';
      root[1] = 0;
    }
    if (lastDrive == root[0]) {
      return lastDriveOldFAT;
    }

    if ( !GetVolumeInformation(root, NULL, 0,
                               NULL, &vfnsize, &vfsflags,
                               NULL, 0)) {
        zfprintf(mesg, "zip diagnostic: GetVolumeInformation failed\n");
        return(FALSE);
    }

    lastDrive = root[0];
    lastDriveOldFAT = vfnsize <= 12;

    return lastDriveOldFAT;
}

#ifdef UNICODE_SUPPORT
int IsFileSystemOldFATW(wchar_t *dir)
{
  static wchar_t lastDrive = (wchar_t)'\0';    /* cached drive of last GetVolumeInformation call */
  static int lastDriveOldFAT = 0;  /* cached OldFAT value of last GetVolumeInformation call */
  wchar_t root[4];
  DWORD vfnsize;
  DWORD vfsflags;

    /*
     * We separate FAT and HPFS+other file systems here.
     * I consider other systems to be similar to HPFS/NTFS, i.e.
     * support for long file names and being case sensitive to some extent.
     */

    wcsncpy(root, dir, 3);
    if ( iswalpha(root[0]) && (root[1] == (wchar_t)':') ) {
      root[0] = towupper(dir[0]);
      root[2] = (wchar_t)'\\';
      root[3] = 0;
    }
    else {
      root[0] = (wchar_t)'\\';
      root[1] = 0;
    }
    if (lastDrive == root[0]) {
      return lastDriveOldFAT;
    }

    if ( !GetVolumeInformationW(root, NULL, 0,
                                NULL, &vfnsize, &vfsflags,
                                NULL, 0)) {
        zfprintf(mesg, "zip diagnostic: GetVolumeInformation failed\n");
        return(FALSE);
    }

    lastDrive = root[0];
    lastDriveOldFAT = vfnsize <= 12;

    return lastDriveOldFAT;
}
#endif


/* access mode bits and time stamp */

int GetFileMode(char *name)
{
DWORD dwAttr;
#if defined(__RSXNT__)  /* RSXNT/EMX C rtl uses OEM charset */
  char *ansi_name = (char *)alloca(strlen(name) + 1);

  OemToAnsi(name, ansi_name);
  name = ansi_name;
#endif

  dwAttr = GetFileAttributes(name);
  if ( dwAttr == 0xFFFFFFFF ) {
    zipwarn("reading file attributes failed: ", name);
    /*
    zfprintf(mesg, "zip diagnostic: GetFileAttributes failed");
    fflush();
    */
    return(0x20); /* the most likely, though why the error? security? */
  }
  return(
          (dwAttr&FILE_ATTRIBUTE_READONLY  ? A_RONLY   :0)
        | (dwAttr&FILE_ATTRIBUTE_HIDDEN    ? A_HIDDEN  :0)
        | (dwAttr&FILE_ATTRIBUTE_SYSTEM    ? A_SYSTEM  :0)
        | (dwAttr&FILE_ATTRIBUTE_DIRECTORY ? A_DIR     :0)
        | (dwAttr&FILE_ATTRIBUTE_ARCHIVE   ? A_ARCHIVE :0));
}

#ifdef UNICODE_SUPPORT
int GetFileModeW(wchar_t *namew)
{
DWORD dwAttr;
int failed = 0;
#if defined(__RSXNT__)  /* RSXNT/EMX C rtl uses OEM charset */
  wchar_t *ansi_namew = (wchar_t *)alloca((wcslen(namew) + 1) * sizeof(wchar_t));

  CharToAnsiW(namew, ansi_namew);
  namew = ansi_namew;
#endif

  dwAttr = GetFileAttributesW(namew);
  if ( dwAttr == 0xFFFFFFFF ) {
#ifdef WINDOWS_LONG_PATHS
    size_t namew_len = wcslen(namew);
    
    if (namew_len > MAX_PATH && include_windows_long_paths) {
      wchar_t *namew_long;
      size_t namew_long_len;
      size_t j;
      wchar_t *full_path = NULL;

      if ((full_path = _wfullpath(NULL, namew, 0)) != NULL) {
# if 0
          sprintf(errbuf, "    full_path namew: '%S'", full_path);
          zipmessage(errbuf, "");
# endif
      }
      if (full_path) {
        if ((namew_long = malloc((wcslen(full_path) + 5) * sizeof(wchar_t))) == NULL) {
          ZIPERR(ZE_MEM, "GetFileModeW");
        }
      } else {
        if ((namew_long = malloc((namew_len + 5) * sizeof(wchar_t))) == NULL) {
          ZIPERR(ZE_MEM, "GetFileModeW");
        }
      }
      wcscpy(namew_long, L"\\\\?\\");
      if (full_path) {
        wcscat(namew_long, full_path);
        free(full_path);
      } else {
        wcscat(namew_long, namew);
      }
      namew_long_len = wcslen(namew_long);
      for (j = 0; j < namew_long_len; j++) {
        if (namew_long[j] == L'/') {
          namew_long[j] = L'\\';
        }
      }

      dwAttr = GetFileAttributesW(namew_long);
      if ( dwAttr == 0xFFFFFFFF ) {
        failed = 1;
      }
      free(namew_long);
    }
    else
#endif
    {
      failed = 1;
    }
    if (failed) {
      char *name = wchar_to_local_string(namew);
      zipwarn("reading file attributes failed: ", name);
      free(name);
      return(0x20); /* the most likely, though why the error? security? */
    }
  }
  return(
          (dwAttr&FILE_ATTRIBUTE_READONLY  ? A_RONLY   :0)
        | (dwAttr&FILE_ATTRIBUTE_HIDDEN    ? A_HIDDEN  :0)
        | (dwAttr&FILE_ATTRIBUTE_SYSTEM    ? A_SYSTEM  :0)
        | (dwAttr&FILE_ATTRIBUTE_DIRECTORY ? A_DIR     :0)
        | (dwAttr&FILE_ATTRIBUTE_ARCHIVE   ? A_ARCHIVE :0));
}
#endif


int ClearArchiveBitW(wchar_t *namew)
{
DWORD dwAttr;
  dwAttr = GetFileAttributesW(namew);
  if ( dwAttr == 0xFFFFFFFF ) {
    zfprintf(mesg, "zip diagnostic: GetFileAttributes failed\n");
    return(0);
  }

  if (!SetFileAttributesW(namew, (DWORD)(dwAttr & ~FILE_ATTRIBUTE_ARCHIVE))) {
    zfprintf(mesg, "zip diagnostic: SetFileAttributes failed\n");
    perror("SetFileAttributes");
    return(0);
  }
  return(1);
}

int ClearArchiveBit(char *name)
{
DWORD dwAttr;
#if defined(__RSXNT__)  /* RSXNT/EMX C rtl uses OEM charset */
  char *ansi_name = (char *)alloca(strlen(name) + 1);

  OemToAnsi(name, ansi_name);
  name = ansi_name;
#endif

  dwAttr = GetFileAttributes(name);
  if ( dwAttr == 0xFFFFFFFF ) {
    zfprintf(mesg, "zip diagnostic: GetFileAttributes failed\n");
    return(0);
  }

  if (!SetFileAttributes(name, (DWORD)(dwAttr & ~FILE_ATTRIBUTE_ARCHIVE))) {
    zfprintf(mesg, "zip diagnostic: SetFileAttributes failed\n");
    perror("SetFileAttributes");
    return(0);
  }
  return(1);
}


#ifdef NT_TZBUG_WORKAROUND
local int FSusesLocalTime(const char *path)
{
    char  *tmp0;
    char   rootPathName[4];
    char   tmp1[MAX_PATH], tmp2[MAX_PATH];
    DWORD  volSerNo, maxCompLen, fileSysFlags;
#if defined(__RSXNT__)  /* RSXNT/EMX C rtl uses OEM charset */
    char *ansi_path = (char *)alloca(strlen(path) + 1);

    OemToAnsi(path, ansi_path);
    path = ansi_path;
#endif

    if (isalpha((uch)path[0]) && (path[1] == ':'))
        tmp0 = (char *)path;
    else
    {
        GetFullPathName(path, MAX_PATH, tmp1, &tmp0);
        tmp0 = &tmp1[0];
    }
    strncpy(rootPathName, tmp0, 3);   /* Build the root path name, */
    rootPathName[3] = '\0';           /* e.g. "A:/"                */

    GetVolumeInformation((LPCTSTR)rootPathName, (LPTSTR)tmp1, (DWORD)MAX_PATH,
                         &volSerNo, &maxCompLen, &fileSysFlags,
                         (LPTSTR)tmp2, (DWORD)MAX_PATH);

    /* Volumes in (V)FAT and (OS/2) HPFS format store file timestamps in
     * local time!
     */
    return !strncmp(strupr(tmp2), "FAT", 3) ||
           !strncmp(tmp2, "VFAT", 4) ||
           !strncmp(tmp2, "HPFS", 4);

} /* end function FSusesLocalTime() */

# ifdef UNICODE_SUPPORT
local int FSusesLocalTimeW(const wchar_t *path)
{
    wchar_t  *tmp0;
    wchar_t   rootPathName[4];
    wchar_t   tmp1[MAX_PATH], tmp2[MAX_PATH];
    DWORD  volSerNo, maxCompLen, fileSysFlags;
#if defined(__RSXNT__)  /* RSXNT/EMX C rtl uses OEM charset */
    wchar_t *ansi_path = (wchar_t *)alloca((wcslen(path) + 1) * sizeof(wchar_t));

    CharToAnsiW(path, ansi_path);
    path = ansi_path;
#endif

    if (iswalpha(path[0]) && (path[1] == (wchar_t)':'))
        tmp0 = (wchar_t *)path;
    else
    {
        GetFullPathNameW(path, MAX_PATH, tmp1, &tmp0);
        tmp0 = &tmp1[0];
    }
    wcsncpy(rootPathName, tmp0, 3);   /* Build the root path name, */
    rootPathName[3] = (wchar_t)'\0';           /* e.g. "A:/"                */

    GetVolumeInformationW(rootPathName, tmp1, (DWORD)MAX_PATH,
                         &volSerNo, &maxCompLen, &fileSysFlags,
                         tmp2, (DWORD)MAX_PATH);

    /* Volumes in (V)FAT and (OS/2) HPFS format store file timestamps in
     * local time!
     */
    return !wcsncmp(_wcsupr(tmp2), L"FAT", 3) ||
           !wcsncmp(tmp2, L"VFAT", 4) ||
           !wcsncmp(tmp2, L"HPFS", 4);

} /* end function FSusesLocalTimeW() */
# endif

#endif /* NT_TZBUG_WORKAROUND */


#if (defined(USE_EF_UT_TIME) || defined(NT_TZBUG_WORKAROUND))

#if (defined(__GNUC__) || defined(ULONG_LONG_MAX))
   typedef long long            LLONG64;
   typedef unsigned long long   ULLNG64;
#elif (defined(__WATCOMC__) && (__WATCOMC__ >= 1100))
   typedef __int64              LLONG64;
   typedef unsigned __int64     ULLNG64;
#elif (defined(_MSC_VER) && (_MSC_VER >= 1100))
   typedef __int64              LLONG64;
   typedef unsigned __int64     ULLNG64;
#elif (defined(__IBMC__) && (__IBMC__ >= 350))
   typedef __int64              LLONG64;
   typedef unsigned __int64     ULLNG64;
#else
#  define NO_INT64
#endif

#  define UNIX_TIME_ZERO_HI  0x019DB1DEUL
#  define UNIX_TIME_ZERO_LO  0xD53E8000UL
#  define NT_QUANTA_PER_UNIX 10000000L
#  define FTQUANTA_PER_UT_L  (NT_QUANTA_PER_UNIX & 0xFFFF)
#  define FTQUANTA_PER_UT_H  (NT_QUANTA_PER_UNIX >> 16)
#  define UNIX_TIME_UMAX_HI  0x0236485EUL
#  define UNIX_TIME_UMAX_LO  0xD4A5E980UL
#  define UNIX_TIME_SMIN_HI  0x0151669EUL
#  define UNIX_TIME_SMIN_LO  0xD53E8000UL
#  define UNIX_TIME_SMAX_HI  0x01E9FD1EUL
#  define UNIX_TIME_SMAX_LO  0xD4A5E980UL

local int FileTime2utime(FILETIME *pft, time_t *ut)
{
#ifndef NO_INT64
    ULLNG64 NTtime;

    NTtime = ((ULLNG64)pft->dwLowDateTime +
              ((ULLNG64)pft->dwHighDateTime << 32));

    /* underflow and overflow handling */
#ifdef CHECK_UTIME_SIGNED_UNSIGNED
    if ((time_t)0x80000000L < (time_t)0L)
    {
        if (NTtime < ((ULLNG64)UNIX_TIME_SMIN_LO +
                      ((ULLNG64)UNIX_TIME_SMIN_HI << 32))) {
            *ut = (time_t)LONG_MIN;
            return FALSE;
        }
        if (NTtime > ((ULLNG64)UNIX_TIME_SMAX_LO +
                      ((ULLNG64)UNIX_TIME_SMAX_HI << 32))) {
            *ut = (time_t)LONG_MAX;
            return FALSE;
        }
    }
    else
#endif /* CHECK_UTIME_SIGNED_UNSIGNED */
    {
        if (NTtime < ((ULLNG64)UNIX_TIME_ZERO_LO +
                      ((ULLNG64)UNIX_TIME_ZERO_HI << 32))) {
            *ut = (time_t)0;
            return FALSE;
        }
        if (NTtime > ((ULLNG64)UNIX_TIME_UMAX_LO +
                      ((ULLNG64)UNIX_TIME_UMAX_HI << 32))) {
            *ut = (time_t)ULONG_MAX;
            return FALSE;
        }
    }

    NTtime -= ((ULLNG64)UNIX_TIME_ZERO_LO +
               ((ULLNG64)UNIX_TIME_ZERO_HI << 32));
    *ut = (time_t)(NTtime / (unsigned long)NT_QUANTA_PER_UNIX);
    return TRUE;
#else /* NO_INT64 (64-bit integer arithmetics may not be supported) */
    /* nonzero if `y' is a leap year, else zero */
#   define leap(y) (((y)%4 == 0 && (y)%100 != 0) || (y)%400 == 0)
    /* number of leap years from 1970 to `y' (not including `y' itself) */
#   define nleap(y) (((y)-1969)/4 - ((y)-1901)/100 + ((y)-1601)/400)
    /* daycount at the end of month[m-1] */
    static ZCONST ush ydays[] =
      { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 };

    time_t days;
    SYSTEMTIME w32tm;

    /* underflow and overflow handling */
#ifdef CHECK_UTIME_SIGNED_UNSIGNED
    if ((time_t)0x80000000L < (time_t)0L)
    {
        if ((pft->dwHighDateTime < UNIX_TIME_SMIN_HI) ||
            ((pft->dwHighDateTime == UNIX_TIME_SMIN_HI) &&
             (pft->dwLowDateTime < UNIX_TIME_SMIN_LO))) {
            *ut = (time_t)LONG_MIN;
            return FALSE;
        if ((pft->dwHighDateTime > UNIX_TIME_SMAX_HI) ||
            ((pft->dwHighDateTime == UNIX_TIME_SMAX_HI) &&
             (pft->dwLowDateTime > UNIX_TIME_SMAX_LO))) {
            *ut = (time_t)LONG_MAX;
            return FALSE;
        }
    }
    else
#endif /* CHECK_UTIME_SIGNED_UNSIGNED */
    {
        if ((pft->dwHighDateTime < UNIX_TIME_ZERO_HI) ||
            ((pft->dwHighDateTime == UNIX_TIME_ZERO_HI) &&
             (pft->dwLowDateTime < UNIX_TIME_ZERO_LO))) {
            *ut = (time_t)0;
            return FALSE;
        }
        if ((pft->dwHighDateTime > UNIX_TIME_UMAX_HI) ||
            ((pft->dwHighDateTime == UNIX_TIME_UMAX_HI) &&
             (pft->dwLowDateTime > UNIX_TIME_UMAX_LO))) {
            *ut = (time_t)ULONG_MAX;
            return FALSE;
        }
    }

    FileTimeToSystemTime(pft, &w32tm);

    /* set `days' to the number of days into the year */
    days = w32tm.wDay - 1 + ydays[w32tm.wMonth-1] +
           (w32tm.wMonth > 2 && leap (w32tm.wYear));

    /* now set `days' to the number of days since 1 Jan 1970 */
    days += 365 * (time_t)(w32tm.wYear - 1970) +
            (time_t)(nleap(w32tm.wYear));

    *ut = (time_t)(86400L * days + 3600L * (time_t)w32tm.wHour +
                   (time_t)(60 * w32tm.wMinute + w32tm.wSecond));
    return TRUE;
#endif /* ?NO_INT64 */
} /* end function FileTime2utime() */
#endif /* USE_EF_UT_TIME || NT_TZBUG_WORKAROUND */


#if (defined(NT_TZBUG_WORKAROUND) && defined(W32_STAT_BANDAID))

local int VFatFileTime2utime(const FILETIME *pft, time_t *ut)
{
    FILETIME lft;
    SYSTEMTIME w32tm;
    struct tm ltm;

    FileTimeToLocalFileTime(pft, &lft);
    FileTimeToSystemTime(&lft, &w32tm);
    /* underflow and overflow handling */
    /* TODO: The range checks are not accurate, the actual limits may
     *       be off by one daylight-saving-time shift (typically 1 hour),
     *       depending on the current state of "is_dst".
     */
#ifdef CHECK_UTIME_SIGNED_UNSIGNED
    if ((time_t)0x80000000L < (time_t)0L)
    {
        if ((pft->dwHighDateTime < UNIX_TIME_SMIN_HI) ||
            ((pft->dwHighDateTime == UNIX_TIME_SMIN_HI) &&
             (pft->dwLowDateTime < UNIX_TIME_SMIN_LO))) {
            *ut = (time_t)LONG_MIN;
            return FALSE;
        if ((pft->dwHighDateTime > UNIX_TIME_SMAX_HI) ||
            ((pft->dwHighDateTime == UNIX_TIME_SMAX_HI) &&
             (pft->dwLowDateTime > UNIX_TIME_SMAX_LO))) {
            *ut = (time_t)LONG_MAX;
            return FALSE;
        }
    }
    else
#endif /* CHECK_UTIME_SIGNED_UNSIGNED */
    {
        if ((pft->dwHighDateTime < UNIX_TIME_ZERO_HI) ||
            ((pft->dwHighDateTime == UNIX_TIME_ZERO_HI) &&
             (pft->dwLowDateTime < UNIX_TIME_ZERO_LO))) {
            *ut = (time_t)0;
            return FALSE;
        }
        if ((pft->dwHighDateTime > UNIX_TIME_UMAX_HI) ||
            ((pft->dwHighDateTime == UNIX_TIME_UMAX_HI) &&
             (pft->dwLowDateTime > UNIX_TIME_UMAX_LO))) {
            *ut = (time_t)ULONG_MAX;
            return FALSE;
        }
    }
    ltm.tm_year = w32tm.wYear - 1900;
    ltm.tm_mon = w32tm.wMonth - 1;
    ltm.tm_mday = w32tm.wDay;
    ltm.tm_hour = w32tm.wHour;
    ltm.tm_min = w32tm.wMinute;
    ltm.tm_sec = w32tm.wSecond;
    ltm.tm_isdst = -1;  /* let mktime determine if DST is in effect */
    *ut = mktime(&ltm);

    /* a cheap error check: mktime returns "(time_t)-1L" on conversion errors.
     * Normally, we would have to apply a consistency check because "-1"
     * could also be a valid time. But, it is quite unlikely to read back odd
     * time numbers from file systems that store time stamps in DOS format.
     * (The only known exception is creation time on VFAT partitions.)
     */
    return (*ut != (time_t)-1L);

} /* end function VFatFileTime2utime() */
#endif /* NT_TZBUG_WORKAROUND && W32_STAT_BANDAID */


#if 0           /* Currently, this is not used at all */

long GetTheFileTime(char *name, iztimes *z_ut)
{
HANDLE h;
FILETIME Modft, Accft, Creft, lft;
WORD dh, dl;
# ifdef __RSXNT__        /* RSXNT/EMX C rtl uses OEM charset */
  char *ansi_name = (char *)alloca(strlen(name) + 1);

  OemToAnsi(name, ansi_name);
  name = ansi_name;
# endif

  h = CreateFile(name, FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
                 NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  if ( h != INVALID_HANDLE_VALUE ) {
    BOOL ftOK = GetFileTime(h, &Creft, &Accft, &Modft);
    CloseHandle(h);
# ifdef USE_EF_UT_TIME
    if (ftOK && (z_ut != NULL)) {
      FileTime2utime(&Modft, &(z_ut->mtime));
      if (Accft.dwLowDateTime != 0 || Accft.dwHighDateTime != 0)
          FileTime2utime(&Accft, &(z_ut->atime));
      else
          z_ut->atime = z_ut->mtime;
      if (Creft.dwLowDateTime != 0 || Creft.dwHighDateTime != 0)
          FileTime2utime(&Creft, &(z_ut->ctime));
      else
          z_ut->ctime = z_ut->mtime;
    }
# endif
    FileTimeToLocalFileTime(&ft, &lft);
    FileTimeToDosDateTime(&lft, &dh, &dl);
    return(dh<<16) | dl;
  }
  else
    return 0L;
}

#endif /* never */


void ChangeNameForFAT(char *name)
{
  char *src, *dst, *next, *ptr, *dot, *start;
  static char invalid[] = ":;,=+\"[]<>| \t";

  if ( isalpha((uch)name[0]) && (name[1] == ':') )
    start = name + 2;
  else
    start = name;

  src = dst = start;
  if ( (*src == '/') || (*src == '\\') )
    src++, dst++;

  while ( *src )
  {
    for ( next = src; *next && (*next != '/') && (*next != '\\'); next++ );

    for ( ptr = src, dot = NULL; ptr < next; ptr++ )
      if ( *ptr == '.' )
      {
        dot = ptr; /* remember last dot */
        *ptr = '_';
      }

    if ( dot == NULL )
      for ( ptr = src; ptr < next; ptr++ )
        if ( *ptr == '_' )
          dot = ptr; /* remember last _ as if it were a dot */

    if ( dot && (dot > src) &&
         ((next - dot <= 4) ||
          ((next - src > 8) && (dot - src > 3))) )
    {
      if ( dot )
        *dot = '.';

      for ( ptr = src; (ptr < dot) && ((ptr - src) < 8); ptr++ )
        *dst++ = *ptr;

      for ( ptr = dot; (ptr < next) && ((ptr - dot) < 4); ptr++ )
        *dst++ = *ptr;
    }
    else
    {
      if ( dot && (next - src == 1) )
        *dot = '.';           /* special case: "." as a path component */

      for ( ptr = src; (ptr < next) && ((ptr - src) < 8); ptr++ )
        *dst++ = *ptr;
    }

    *dst++ = *next; /* either '/' or 0 */

    if ( *next )
    {
      src = next + 1;

      if ( *src == 0 ) /* handle trailing '/' on dirs ! */
        *dst = 0;
    }
    else
      break;
  }

  for ( src = start; *src != 0; ++src )
    if ( (strchr(invalid, *src) != NULL) || (*src == ' ') )
      *src = '_';
}

char *GetLongPathEA(char *name)
{
    return(NULL); /* volunteers ? */
}

#ifdef UNICODE_SUPPORT
wchar_t *GetLongPathEAW(wchar_t *name)
{
    return(NULL); /* volunteers ? */
}
#endif


int IsFileNameValid(x)
char *x;
{
    WIN32_FIND_DATA fd;
    HANDLE h;
#if defined(__RSXNT__)  /* RSXNT/EMX C rtl uses OEM charset */
    char *ansi_name = (char *)alloca(strlen(x) + 1);

    OemToAnsi(x, ansi_name);
    x = ansi_name;
#endif

    if ((h = FindFirstFile(x, &fd)) == INVALID_HANDLE_VALUE)
        return FALSE;
    FindClose(h);
    return TRUE;
}

char *getVolumeLabel(drive, vtime, vmode, vutim)
  int drive;    /* drive name: 'A' .. 'Z' or '\0' for current drive */
  ulg *vtime;   /* volume label creation time (DOS format) */
  ulg *vmode;   /* volume label file mode */
  time_t *vutim;/* volume label creationtime (UNIX format) */

/* If a volume label exists for the given drive, return its name and
   pretend to set its time and mode. The returned name is static data. */

/* 2013-02-13 SMS.
 * Why have arguments other than "drive"?
 * Why request "fnlen" or "flags"?
 * Why a 14-character buffer for the volume name?
 * Changed to pass the actual buffer size ("sizeof vol", now 33) instead
 * of a hard-coded one-less-than the actual buffer size ("13").
 * Microsoft says that the maximum label length is 32 characters (11 for
 * FAT).
 */
{
  char rootpath[] = "?:\\";
  static char vol[ 33];
#if 0
  DWORD fnlen, flags;
#endif /* 0 */

  *vmode = A_ARCHIVE | A_LABEL;           /* this is what msdos returns */
  *vtime = dostime(1980, 1, 1, 0, 0, 0);  /* no true date info available */
  *vutim = dos2unixtime(*vtime);
  rootpath[0] = (char)drive;
#if 0
  if (GetVolumeInformation((drive ? rootpath : NULL),
   vol, (sizeof vol), NULL, &fnlen, &flags, NULL, 0))
#else /* 0 */
  if (GetVolumeInformation((drive ? rootpath : NULL),
   vol, (sizeof vol), NULL, NULL, NULL, NULL, 0))
#endif /* 0 [else] */
#if defined(__RSXNT__)  /* RSXNT/EMX C rtl uses OEM charset */
    return (AnsiToOem(vol, vol), vol);
#else
    return vol;
#endif
  else
    return NULL;
}

#endif /* !UTIL */

int GetWinVersion(DWORD *dwMajorVers, DWORD *dwMinorVers, DWORD *dwBuild)
{
    DWORD dwVersion = 0; 
    *dwBuild = 0;

    dwVersion = GetVersion();
 
    // Get the Windows version.

    *dwMajorVers = (DWORD)(LOBYTE(LOWORD(dwVersion)));
    *dwMinorVers = (DWORD)(HIBYTE(LOWORD(dwVersion)));

    // Get the build number.

    if (dwVersion < 0x80000000) {
        *dwBuild = (DWORD)(HIWORD(dwVersion));
        return 1;
    } else {
        return 0;
    }
}

int ZipIsWinNT(void)    /* returns TRUE if real NT, FALSE if Win95 or Win32s */
{
    static DWORD g_PlatformId = 0xFFFFFFFF; /* saved platform indicator */

    if (g_PlatformId == 0xFFFFFFFF) {
        /* note: GetVersionEx() doesn't exist on WinNT 3.1 */
        if (GetVersion() < 0x80000000)
            /* Apparently true for any version of Windows. */
            g_PlatformId = TRUE;
        else
            g_PlatformId = FALSE;
    }
    return (int)g_PlatformId;
}


#ifndef UTIL
#ifdef __WATCOMC__
#  include <io.h>
#  define _get_osfhandle _os_handle
/* gaah -- Watcom's docs claim that _get_osfhandle exists, but it doesn't.  */
#endif

#ifdef HAVE_FSEEKABLE
/*
 * return TRUE if file is seekable
 */
int fseekable(fp)
FILE *fp;
{
    return GetFileType((HANDLE)_get_osfhandle(fileno(fp))) == FILE_TYPE_DISK;
}
#endif /* HAVE_FSEEKABLE */
#endif /* !UTIL */


#if 0 /* seems to be never used; try it out... */
char *StringLower(char *szArg)
{
  char *szPtr;
/*  unsigned char *szPtr; */
  for ( szPtr = szArg; *szPtr; szPtr++ )
    *szPtr = lower[*szPtr];
  return szArg;
}
#endif /* never */



#ifdef W32_STAT_BANDAID

/* All currently known variants of WIN32 operating systems (Windows 95/98,
 * WinNT 3.x, 4.0, 5.0) have a nasty bug in the OS kernel concerning
 * conversions between UTC and local time: In the time conversion functions
 * of the Win32 API, the timezone offset (including seasonal daylight saving
 * shift) between UTC and local time evaluation is erratically based on the
 * current system time. The correct evaluation must determine the offset
 * value as it {was/is/will be} for the actual time to be converted.
 *
 * The C runtime lib's stat() returns utc time-stamps so that
 * localtime(timestamp) corresponds to the (potentially false) local
 * time shown by the OS' system programs (Explorer, command shell dir, etc.)
 *
 * For the NTFS file system (and other filesystems that store time-stamps
 * as UTC values), this results in st_mtime (, st_{c|a}time) fields which
 * are not stable but vary according to the seasonal change of "daylight
 * saving time in effect / not in effect".
 *
 * To achieve timestamp consistency of UTC (UT extra field) values in
 * Zip archives, the Info-ZIP programs require work-around code for
 * proper time handling in stat() (and other time handling routines).
 */
/* stat() functions under Windows95 tend to fail for root directories.   *
 * Watcom and Borland, at least, are affected by this bug.  Watcom made  *
 * a partial fix for 11.0 but still missed some cases.  This substitute  *
 * detects the case and fills in reasonable values.  Otherwise we get    *
 * effects like failure to extract to a root dir because it's not found. */

#ifdef LARGE_FILE_SUPPORT         /* E. Gordon 9/12/03 */
 int zstat_zipwin32(const char *path, z_stat *buf)
#else
 int zstat_zipwin32(const char *path, struct stat *buf)
#endif
{
# ifdef LARGE_FILE_SUPPORT         /* E. Gordon 9/12/03 */
    if (!zstat(path, buf))
# else
    if (!stat(path, buf))
# endif
    {
#ifdef NT_TZBUG_WORKAROUND
        /* stat was successful, now redo the time-stamp fetches */
        int fs_uses_loctime = FSusesLocalTime(path);
        HANDLE h;
        FILETIME Modft, Accft, Creft;
#if defined(__RSXNT__)  /* RSXNT/EMX C rtl uses OEM charset */
        char *ansi_path = (char *)alloca(strlen(path) + 1);

        OemToAnsi(path, ansi_path);
#       define Ansi_Path  ansi_path
#else
#       define Ansi_Path  path
#endif

        Trace((stdout, "stat(%s) finds modtime %08lx\n", path, buf->st_mtime));
        h = CreateFile(Ansi_Path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
                       NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            BOOL ftOK = GetFileTime(h, &Creft, &Accft, &Modft);
            CloseHandle(h);

            if (ftOK) {
                if (!fs_uses_loctime) {
                    /*  On a filesystem that stores UTC timestamps, we refill
                     *  the time fields of the struct stat buffer by directly
                     *  using the UTC values as returned by the Win32
                     *  GetFileTime() API call.
                     */
                    FileTime2utime(&Modft, &(buf->st_mtime));
                    if (Accft.dwLowDateTime != 0 || Accft.dwHighDateTime != 0)
                        FileTime2utime(&Accft, &(buf->st_atime));
                    else
                        buf->st_atime = buf->st_mtime;
                    if (Creft.dwLowDateTime != 0 || Creft.dwHighDateTime != 0)
                        FileTime2utime(&Creft, &(buf->st_ctime));
                    else
                        buf->st_ctime = buf->st_mtime;
                    Tracev((stdout,"NTFS, recalculated modtime %08lx\n",
                            buf->st_mtime));
                } else {
                    /*  On VFAT and FAT-like filesystems, the FILETIME values
                     *  are converted back to the stable local time before
                     *  converting them to UTC unix time-stamps.
                     */
                    VFatFileTime2utime(&Modft, &(buf->st_mtime));
                    if (Accft.dwLowDateTime != 0 || Accft.dwHighDateTime != 0)
                        VFatFileTime2utime(&Accft, &(buf->st_atime));
                    else
                        buf->st_atime = buf->st_mtime;
                    if (Creft.dwLowDateTime != 0 || Creft.dwHighDateTime != 0)
                        VFatFileTime2utime(&Creft, &(buf->st_ctime));
                    else
                        buf->st_ctime = buf->st_mtime;
                    Tracev((stdout, "VFAT, recalculated modtime %08lx\n",
                            buf->st_mtime));
                }
            }
        }
#       undef Ansi_Path
#endif /* NT_TZBUG_WORKAROUND */
        return 0;
    }
#ifdef W32_STATROOT_FIX
    else
    {
        DWORD flags;
#if defined(__RSXNT__)  /* RSXNT/EMX C rtl uses OEM charset */
        char *ansi_path = (char *)alloca(strlen(path) + 1);

        OemToAnsi(path, ansi_path);
#       define Ansi_Path  ansi_path
#else
#       define Ansi_Path  path
#endif

        flags = GetFileAttributes(Ansi_Path);
        if (flags != 0xFFFFFFFF && flags & FILE_ATTRIBUTE_DIRECTORY) {
            Trace((stderr, "\nstat(\"%s\",...) failed on existing directory\n",
                   path));
#ifdef LARGE_FILE_SUPPORT         /* E. Gordon 9/12/03 */
            memset(buf, 0, sizeof(z_stat));
#else
            memset(buf, 0, sizeof(struct stat));
#endif
            buf->st_atime = buf->st_ctime = buf->st_mtime =
              dos2unixtime(DOSTIME_MINIMUM);
            /* !!!   you MUST NOT add a cast to the type of "st_mode" here;
             * !!!   different compilers do not agree on the "st_mode" size!
             * !!!   (And, some compiler may not declare the "mode_t" type
             * !!!   identifier, so you cannot use it, either.)
             */
            buf->st_mode = S_IFDIR | S_IREAD |
                           ((flags & FILE_ATTRIBUTE_READONLY) ? 0 : S_IWRITE);
            return 0;
        } /* assumes: stat() won't fail on non-dirs without good reason */
#       undef Ansi_Path
    }
#endif /* W32_STATROOT_FIX */
    return -1;
}


# ifdef UNICODE_SUPPORT

int zstat_zipwin32w(const wchar_t *pathw, zw_stat *buf)
{
    int stat_failed;

    stat_failed = zwstat(pathw, buf);
#ifdef WINDOWS_LONG_PATHS
    if (stat_failed && include_windows_long_paths) {
        if (wcslen(pathw) > MAX_PATH) {
            /* wstati64 does not handle \\?\, so open file using long path and pass handle to zfstat */
            wchar_t *full_path = NULL;
            wchar_t *pathw_long;
            size_t pathw_long_len;
            size_t i;
            HANDLE h;
            int fileID;

            if ((full_path = _wfullpath(NULL, pathw, 0)) != NULL) {
# if 0
                sprintf(errbuf, "    full_path: '%S'", full_path);
                zipmessage(errbuf, "");
# endif
                if ((pathw_long = malloc((wcslen(full_path) + 5) * sizeof(wchar_t))) == NULL) {
                      ZIPERR(ZE_MEM, "Getting long path for zwstat");
                }
            } else {
                if ((pathw_long = malloc((wcslen(pathw) + 5) * sizeof(wchar_t))) == NULL) {
                      ZIPERR(ZE_MEM, "Getting long path for zwstat");
                }
            }
            wcscpy(pathw_long, L"\\\\?\\");
            if (full_path != NULL) {
                wcscat(pathw_long, full_path);
                free(full_path);
            } else {
                wcscat(pathw_long, pathw);
            }
            pathw_long_len = wcslen(pathw_long);
            for (i = 0; i < pathw_long_len; i++) {
                if (pathw_long[i] == L'/') {
                    pathw_long[i] = L'\\';
                }
            }

            h = CreateFileW(pathw_long, FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
            if (h != INVALID_HANDLE_VALUE) {
                /* intptr_t not defined in VS 6, so use long */
                if ((fileID = _open_osfhandle((Z_INTPTR_T)h, _O_RDONLY))==-1) {
# if 0
                    zprintf(  "      _open_osfhandle Failed");
# endif
                } else {
                    stat_failed = zfstat(fileID, buf);
# if 0
                    sprintf(errbuf, "      stat_failed = %d", stat_failed);
                    zipmessage(errbuf, "");
# endif
                    CloseHandle(h);
                }
            } else {
# if 0
              zprintf("      CreateFileW open failed\n");
# endif
            }

            free(pathw_long);
        }
    }
#endif
    if (!stat_failed)
    {
#ifdef NT_TZBUG_WORKAROUND
        /* stat was successful, now redo the time-stamp fetches */
        int fs_uses_loctime = FSusesLocalTimeW(pathw);
        HANDLE h;
        FILETIME Modft, Accft, Creft;

        Trace((stdout, "stat(%s) finds modtime %08lx\n", pathw, buf->st_mtime));

        h = CreateFileW(pathw, FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
                       NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
#ifdef WINDOWS_LONG_PATHS
        if (h == INVALID_HANDLE_VALUE && wcslen(pathw) > MAX_PATH && include_windows_long_paths) {
            /* try long path */
            wchar_t *pathw_long;
            size_t pathw_long_len;
            size_t i;
            wchar_t *full_path = NULL;

            if ((full_path = _wfullpath(NULL, pathw, 0)) != NULL) {
# if 0
              sprintf(errbuf, "      CreateFileW full_path: (%d chars) '%S'", wcslen(full_path), full_path);
              zipmessage(errbuf, "");
# endif
            }
            if (full_path) {
              if ((pathw_long = malloc((wcslen(full_path) + 5) * sizeof(wchar_t))) == NULL) {
                  ZIPERR(ZE_MEM, "Getting full path for zwstat");
              }
            } else {
              if ((pathw_long = malloc((wcslen(pathw) + 5) * sizeof(wchar_t))) == NULL) {
                  ZIPERR(ZE_MEM, "Getting long path for zwstat");
              }
            }
            wcscpy(pathw_long, L"\\\\?\\");
            if (full_path) {
              wcscat(pathw_long, full_path);
              free(full_path);
            } else {
              wcscat(pathw_long, pathw);
            }
            pathw_long_len = wcslen(pathw_long);
            for (i = 0; i < pathw_long_len; i++) {
                if (pathw_long[i] == L'/') {
                    pathw_long[i] = L'\\';
                }
            }

            h = CreateFileW(pathw_long, FILE_READ_ATTRIBUTES, FILE_SHARE_READ,
                           NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        }
#endif
        if (h != INVALID_HANDLE_VALUE) {
            BOOL ftOK = GetFileTime(h, &Creft, &Accft, &Modft);
            CloseHandle(h);

            if (ftOK) {
                if (!fs_uses_loctime) {
                    /*  On a filesystem that stores UTC timestamps, we refill
                     *  the time fields of the struct stat buffer by directly
                     *  using the UTC values as returned by the Win32
                     *  GetFileTime() API call.
                     */
                    FileTime2utime(&Modft, &(buf->st_mtime));
                    if (Accft.dwLowDateTime != 0 || Accft.dwHighDateTime != 0)
                        FileTime2utime(&Accft, &(buf->st_atime));
                    else
                        buf->st_atime = buf->st_mtime;
                    if (Creft.dwLowDateTime != 0 || Creft.dwHighDateTime != 0)
                        FileTime2utime(&Creft, &(buf->st_ctime));
                    else
                        buf->st_ctime = buf->st_mtime;
                    Tracev((stdout,"NTFS, recalculated modtime %08lx\n",
                            buf->st_mtime));
                } else {
                    /*  On VFAT and FAT-like filesystems, the FILETIME values
                     *  are converted back to the stable local time before
                     *  converting them to UTC unix time-stamps.
                     */
                    VFatFileTime2utime(&Modft, &(buf->st_mtime));
                    if (Accft.dwLowDateTime != 0 || Accft.dwHighDateTime != 0)
                        VFatFileTime2utime(&Accft, &(buf->st_atime));
                    else
                        buf->st_atime = buf->st_mtime;
                    if (Creft.dwLowDateTime != 0 || Creft.dwHighDateTime != 0)
                        VFatFileTime2utime(&Creft, &(buf->st_ctime));
                    else
                        buf->st_ctime = buf->st_mtime;
                    Tracev((stdout, "VFAT, recalculated modtime %08lx\n",
                            buf->st_mtime));
                }
            }
        }
#endif /* NT_TZBUG_WORKAROUND */
        return 0;
    }
#ifdef W32_STATROOT_FIX
    else
    {
        DWORD flags;

        flags = GetFileAttributesW(pathw);
#ifdef WINDOWS_LONG_PATHS
        if (flags == 0xFFFFFFFF && wcslen(pathw) > MAX_PATH && include_windows_long_paths) {
            /* Try long path */
            wchar_t *pathw_long;
            size_t pathw_long_len;
            size_t i;

            if ((pathw_long = malloc((wcslen(pathw) + 5) * sizeof(wchar_t))) == NULL) {
                ZIPERR(ZE_MEM, "Getting long path for GetFileAttributes");
            }
            wcscpy(pathw_long, L"\\\\?\\");
            wcscat(pathw_long, pathw);
            pathw_long_len = wcslen(pathw_long);
            for (i = 0; i < pathw_long_len; i++) {
                if (pathw_long[i] == L'/') {
                    pathw_long[i] = L'\\';
                }
            }
            flags = GetFileAttributesW(pathw_long);
            free(pathw_long);
        }
#endif

        if (flags != 0xFFFFFFFF && flags & FILE_ATTRIBUTE_DIRECTORY) {
            Trace((stderr, "\nstat(\"%s\",...) failed on existing directory\n",
                   pathw));
#ifdef LARGE_FILE_SUPPORT         /* EG 9/12/03 */
            memset(buf, 0, sizeof(z_stat));
#else
            memset(buf, 0, sizeof(struct stat));
#endif
            buf->st_atime = buf->st_ctime = buf->st_mtime =
              dos2unixtime(DOSTIME_MINIMUM);
            /* !!!   you MUST NOT add a cast to the type of "st_mode" here;
             * !!!   different compilers do not agree on the "st_mode" size!
             * !!!   (And, some compiler may not declare the "mode_t" type
             * !!!   identifier, so you cannot use it, either.)
             */
            buf->st_mode = S_IFDIR | S_IREAD |
                           ((flags & FILE_ATTRIBUTE_READONLY) ? 0 : S_IWRITE);
            return 0;
        } /* assumes: stat() won't fail on non-dirs without good reason */
    }
#endif /* W32_STATROOT_FIX */
    return -1;
}

# endif


#endif /* W32_STAT_BANDAID */



#ifdef W32_USE_IZ_TIMEZONE
#include "timezone.h"
#define SECSPERMIN      60
#define MINSPERHOUR     60
#define SECSPERHOUR     (SECSPERMIN * MINSPERHOUR)
static void conv_to_rule(LPSYSTEMTIME lpw32tm, struct rule * ZCONST ptrule);

static void conv_to_rule(LPSYSTEMTIME lpw32tm, struct rule * ZCONST ptrule)
{
    if (lpw32tm->wYear != 0) {
        ptrule->r_type = JULIAN_DAY;
        ptrule->r_day = ydays[lpw32tm->wMonth - 1] + lpw32tm->wDay;
    } else {
        ptrule->r_type = MONTH_NTH_DAY_OF_WEEK;
        ptrule->r_mon = lpw32tm->wMonth;
        ptrule->r_day = lpw32tm->wDayOfWeek;
        ptrule->r_week = lpw32tm->wDay;
    }
    ptrule->r_time = (long)lpw32tm->wHour * SECSPERHOUR +
                     (long)(lpw32tm->wMinute * SECSPERMIN) +
                     (long)lpw32tm->wSecond;
}

int GetPlatformLocalTimezone(register struct state * ZCONST sp,
        void (*fill_tzstate_from_rules)(struct state * ZCONST sp_res,
                                        ZCONST struct rule * ZCONST start,
                                        ZCONST struct rule * ZCONST end))
{
    TIME_ZONE_INFORMATION tzinfo;
    DWORD res;

    /* read current timezone settings from registry if TZ envvar missing */
    res = GetTimeZoneInformation(&tzinfo);
    if (res != TIME_ZONE_ID_INVALID)
    {
        struct rule startrule, stoprule;

        conv_to_rule(&(tzinfo.StandardDate), &stoprule);
        conv_to_rule(&(tzinfo.DaylightDate), &startrule);
        sp->timecnt = 0;
        sp->ttis[0].tt_abbrind = 0;
        if ((sp->charcnt =
             WideCharToMultiByte(CP_ACP, 0, tzinfo.StandardName, -1,
                                 sp->chars, sizeof(sp->chars), NULL, NULL))
            == 0)
            sp->chars[sp->charcnt++] = '\0';
        sp->ttis[1].tt_abbrind = sp->charcnt;
        sp->charcnt +=
            WideCharToMultiByte(CP_ACP, 0, tzinfo.DaylightName, -1,
                                sp->chars + sp->charcnt,
                                sizeof(sp->chars) - sp->charcnt, NULL, NULL);
        if ((sp->charcnt - sp->ttis[1].tt_abbrind) == 0)
            sp->chars[sp->charcnt++] = '\0';
        sp->ttis[0].tt_gmtoff = - (tzinfo.Bias + tzinfo.StandardBias)
                                * MINSPERHOUR;
        sp->ttis[1].tt_gmtoff = - (tzinfo.Bias + tzinfo.DaylightBias)
                                * MINSPERHOUR;
        sp->ttis[0].tt_isdst = 0;
        sp->ttis[1].tt_isdst = 1;
        sp->typecnt = (startrule.r_mon == 0 && stoprule.r_mon == 0) ? 1 : 2;

        if (sp->typecnt > 1)
            (*fill_tzstate_from_rules)(sp, &startrule, &stoprule);
        return TRUE;
    }
    return FALSE;
}
#endif /* W32_USE_IZ_TIMEZONE */



#ifndef WINDLL
/* This replacement getch() function was originally created for Watcom C
 * and then additionally used with CYGWIN. Since UnZip 5.4, all other Win32
 * ports apply this replacement rather that their supplied getch() (or
 * alike) function.  There are problems with unabsorbed LF characters left
 * over in the keyboard buffer under Win95 (and 98) when ENTER was pressed.
 * (Under Win95, ENTER returns two(!!) characters: CR-LF.)  This problem
 * does not appear when run on a WinNT console prompt!
 */

/* Watcom 10.6's getch() does not handle Alt+<digit><digit><digit>. */
/* Note that if PASSWD_FROM_STDIN is defined, the file containing   */
/* the password must have a carriage return after the word, not a   */
/* Unix-style newline (linefeed only).  This discards linefeeds.    */

int getch_win32(void)
{
  HANDLE stin;
  DWORD rc;
  unsigned char buf[2];
  int ret = -1;
  DWORD odemode = ~(DWORD)0;

#  ifdef PASSWD_FROM_STDIN
  stin = GetStdHandle(STD_INPUT_HANDLE);
#  else
  stin = CreateFile("CONIN$", GENERIC_READ | GENERIC_WRITE,
                    FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
  if (stin == INVALID_HANDLE_VALUE)
    return -1;
#  endif
  if (GetConsoleMode(stin, &odemode))
    SetConsoleMode(stin, ENABLE_PROCESSED_INPUT);  /* raw except ^C noticed */
  if (ReadFile(stin, &buf, 1, &rc, NULL) && rc == 1)
    ret = buf[0];
  /* when the user hits return we get CR LF.  We discard the LF, not the CR,
   * because when we call this for the first time after a previous input
   * such as the one for "replace foo? [y]es, ..." the LF may still be in
   * the input stream before whatever the user types at our prompt. */
  if (ret == '\n')
    if (ReadFile(stin, &buf, 1, &rc, NULL) && rc == 1)
      ret = buf[0];
  if (odemode != ~(DWORD)0)
    SetConsoleMode(stin, odemode);
#  ifndef PASSWD_FROM_STDIN
  CloseHandle(stin);
#  endif
  return ret;
}
#endif /* !WINDLL */



/******************************/
/*  Function version_local()  */
/******************************/

void version_local()
{
    static ZCONST char CompiledWith[] = "Compiled with %s%s for %s%s%s.\n\n";
#if (defined(_MSC_VER) || defined(__WATCOMC__) || defined(__DJGPP__))
    char buf[80];
#if (defined(_MSC_VER) && (_MSC_VER > 900))
    char buf2[80];
    char buf3[80];
#endif
#endif

/* Define the compiler name and version strings */
#if defined(_MSC_VER)  /* MSC == MSVC++, including the SDK compiler */
    sprintf(buf, "Microsoft C %d.%02d ", _MSC_VER/100, _MSC_VER%100);
#  define COMPILER_NAME1        buf
#  if (_MSC_VER == 800)
#    define COMPILER_NAME2      "(Visual C++ v1.1)"
#  elif (_MSC_VER == 850)
#    define COMPILER_NAME2      "(Windows NT v3.5 SDK)"
#  elif (_MSC_VER == 900)
#    define COMPILER_NAME2      "(Visual C++ v2.x)"
#  elif (_MSC_VER > 900)
    sprintf(buf2, "(Visual C++ v%d.%d)", _MSC_VER/100 - 6, _MSC_VER%100/10);
#   ifdef CONFIG_PLATFORM
    sprintf(buf3, "%s [%s]", buf2, CONFIG_PLATFORM);
#   else
    sprintf(buf3, "%s", buf2);
#   endif
#    define COMPILER_NAME2      buf3
#  else
#    define COMPILER_NAME2      "(bad version)"
#  endif
#elif defined(__WATCOMC__)
#  if (__WATCOMC__ % 10 > 0)
/* We do this silly test because __WATCOMC__ gives two digits for the  */
/* minor version, but Watcom packaging prefers to show only one digit. */
    sprintf(buf, "Watcom C/C++ %d.%02d", __WATCOMC__ / 100,
            __WATCOMC__ % 100);
#  else
    sprintf(buf, "Watcom C/C++ %d.%d", __WATCOMC__ / 100,
            (__WATCOMC__ % 100) / 10);
#  endif /* __WATCOMC__ % 10 > 0 */
#  define COMPILER_NAME1        buf
#  define COMPILER_NAME2        ""
#elif defined(__TURBOC__)
#  ifdef __BORLANDC__
#    define COMPILER_NAME1      "Borland C++"
#    if (__BORLANDC__ == 0x0452)   /* __BCPLUSPLUS__ = 0x0320 */
#      define COMPILER_NAME2    " 4.0 or 4.02"
#    elif (__BORLANDC__ == 0x0460)   /* __BCPLUSPLUS__ = 0x0340 */
#      define COMPILER_NAME2    " 4.5"
#    elif (__BORLANDC__ == 0x0500)   /* __TURBOC__ = 0x0500 */
#      define COMPILER_NAME2    " 5.0"
#    elif (__BORLANDC__ == 0x0520)   /* __TURBOC__ = 0x0520 */
#      define COMPILER_NAME2    " 5.2 (C++ Builder 1.0)"
#    elif (__BORLANDC__ == 0x0530)   /* __BCPLUSPLUS__ = 0x0530 */
#      define COMPILER_NAME2    " 5.3 (C++ Builder 3.0)"
#    elif (__BORLANDC__ == 0x0540)   /* __BCPLUSPLUS__ = 0x0540 */
#      define COMPILER_NAME2    " 5.4 (C++ Builder 4.0)"
#    elif (__BORLANDC__ == 0x0550)   /* __BCPLUSPLUS__ = 0x0550 */
#      define COMPILER_NAME2    " 5.5 (C++ Builder 5.0)"
#    elif (__BORLANDC__ == 0x0551)   /* __BCPLUSPLUS__ = 0x0551 */
#      define COMPILER_NAME2    " 5.5.1 (C++ Builder 5.0.1)"
#    elif (__BORLANDC__ == 0x0560)   /* __BCPLUSPLUS__ = 0x0560 */
#      define COMPILER_NAME2    " 5.6 (C++ Builder 6)"
#    else
#      define COMPILER_NAME2    " later than 5.6"
#    endif
#  else /* !__BORLANDC__ */
#    define COMPILER_NAME1      "Turbo C"
#    if (__TURBOC__ >= 0x0400)     /* Kevin:  3.0 -> 0x0401 */
#      define COMPILER_NAME2    "++ 3.0 or later"
#    elif (__TURBOC__ == 0x0295)     /* [661] vfy'd by Kevin */
#      define COMPILER_NAME2    "++ 1.0"
#    endif
#  endif /* __BORLANDC__ */
#elif defined(__GNUC__)
#  ifdef __RSXNT__
#    if (defined(__DJGPP__) && !defined(__EMX__))
    sprintf(buf, "rsxnt(djgpp v%d.%02d) / gcc ",
            __DJGPP__, __DJGPP_MINOR__);
#      define COMPILER_NAME1    buf
#    elif defined(__DJGPP__)
   sprintf(buf, "rsxnt(emx+djgpp v%d.%02d) / gcc ",
            __DJGPP__, __DJGPP_MINOR__);
#      define COMPILER_NAME1    buf
#    elif (defined(__GO32__) && !defined(__EMX__))
#      define COMPILER_NAME1    "rsxnt(djgpp v1.x) / gcc "
#    elif defined(__GO32__)
#      define COMPILER_NAME1    "rsxnt(emx + djgpp v1.x) / gcc "
#    elif defined(__EMX__)
#      define COMPILER_NAME1    "rsxnt(emx)+gcc "
#    else
#      define COMPILER_NAME1    "rsxnt(unknown) / gcc "
#    endif
#  elif defined(__CYGWIN__)
#      define COMPILER_NAME1    "Cygnus win32 / gcc "
#  elif defined(__MINGW32__)
#      define COMPILER_NAME1    "mingw32 / gcc "
#  else
#      define COMPILER_NAME1    "gcc "
#  endif
#  define COMPILER_NAME2        __VERSION__
#elif defined(__LCC__)
#  define COMPILER_NAME1        "LCC-Win32"
#  define COMPILER_NAME2        ""
#else
#  define COMPILER_NAME1        "unknown compiler (SDK?)"
#  define COMPILER_NAME2        ""
#endif

/* Define the compile date string */
#if defined(__DATE__) && !defined(NO_BUILD_DATE)
#  define COMPILE_DATE " on " __DATE__
#else
#  define COMPILE_DATE ""
#endif

#ifdef _WIN64
    zprintf(CompiledWith, COMPILER_NAME1, COMPILER_NAME2,
           "\nWindows NT", " (64-bit)", COMPILE_DATE);
#else
    zprintf(CompiledWith, COMPILER_NAME1, COMPILER_NAME2,
           "\nWindows 9x / Windows NT", " (32-bit)", COMPILE_DATE);
#endif

    return;

} /* end function version_local() */



#ifdef ENABLE_ENTRY_TIMING

/* A replacement for the Unix gettimeofday() function for Windows.
   This is used for timing the rate that entries are zipped
   with usec resolution.  Apparently the timezone parameter
   is now never used (there's better ways to handle time zone),
   so NULL is always passed as the second parameter.  Could
   therefore simplify this, but it works as is.  2009 Aug 12 EG.
   Apparently free to use from
   http://www.openasthra.com/c-tidbits/gettimeofday-function-for-windows/ */

# if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#   define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
# else
#   define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
# endif

struct timezonestruct
{
  int  tz_minuteswest; /* minutes W of Greenwich */
  int  tz_dsttime;     /* type of dst correction */
};

struct timevalstruct {
        long    tv_sec;         /* seconds */
        long    tv_usec;        /* and microseconds */
};

int gettimeofday(struct timevalstruct *tv, struct timezonestruct *tz)
{
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  static int tzflag = 0;

  if (NULL != tv)
  {
    GetSystemTimeAsFileTime(&ft);

    tmpres = (__int64)ft.dwHighDateTime << 32;
    tmpres |= ft.dwLowDateTime;

    tmpres /= 10;  /*convert into microseconds*/
    /*converting file time to unix epoch*/
    tmpres -= DELTA_EPOCH_IN_MICROSECS;
    tv->tv_sec = (long)(tmpres / 1000000UL);
    tv->tv_usec = (long)(tmpres % 1000000UL);
  }

  if (NULL != tz)
  {
    if (!tzflag)
    {
      _tzset();
      tzflag++;
    }
    tz->tz_minuteswest = _timezone / 60;
    tz->tz_dsttime = _daylight;
  }

  return 0;
}

#if 0
 #define TEST
 #ifdef TEST
 int main()
 {
   struct timeval now;
   struct timezone tzone;

   gettimeofday(&now, NULL);
   gettimeofday(&now, &tzone);
 }
 #endif
#endif
/* This was used by -de and -dr to get more accurate
   rate timing.  Turns out this is only good to 10 to 15 ms
   because of how Windows does time. */
/*
uzoff_t get_time_in_usec()
{
  struct timevalstruct now;

  gettimeofday(&now, NULL);
  return now.tv_sec * 1000000 + now.tv_usec;
}
*/


/* an alternative good to around 10 to 15 ms */
uzoff_t get_time_in_usec()
{
  FILETIME ft;
  unsigned __int64 tmpres = 0;
  ULARGE_INTEGER qt;

  GetSystemTimeAsFileTime(&ft);

  tmpres = ft.dwHighDateTime;
  tmpres = tmpres << 32;
  tmpres |= ft.dwLowDateTime;


  qt.LowPart = ft.dwLowDateTime;
  qt.HighPart = ft.dwHighDateTime;

  tmpres /= 10;  /*convert into microseconds*/

  return tmpres;
}

#endif




/* --------------------------------------------------- */
/* WinZip Gladman AES encryption
 * May 2011
 */

#ifdef IZ_CRYPT_AES_WG

/* Below is more or less the entropy function provided by WinZip and
   now suggested by Gladman.  This function is for use on Win32
   systems.  Other OS probably need to modify this function or
   provide their own.
 */

/* simple entropy collection function that uses the fast timer      */
/* since we are not using the random pool for generating secret     */
/* keys we don't need to be too worried about the entropy quality   */

/* Modified in 2008 to add revised entropy generation courtesy of   */
/* WinZip Inc. This code now performs the following sequence of     */
/* entropy generation operations on sequential calls:               */
/*                                                                  */
/*      - the current 8-byte Windows performance counter value      */
/*      - an 8-byte representation of the current date/time         */
/*      - an 8-byte value built from the current process ID         */
/*        and thread ID                                             */
/*      - all subsequent calls return the then-current 8-byte       */
/*        performance counter value                                 */

int entropy_fun(unsigned char buf[], unsigned int len)
{   unsigned __int64    pentium_tsc[1];
    unsigned int        i;
    static unsigned int num = 0;

    switch(num)
    {
    /* use a value that is unlikely to repeat across system reboots         */
    case 1:
        ++num;
        GetSystemTimeAsFileTime((FILETIME *)pentium_tsc);
        break;
    /* use a value that distinguishes between different instances of this   */
    /* code that might be running on different processors at the same time  */
    case 2:
        ++num;
        {
          unsigned __int32 processtest = GetCurrentProcessId();
          unsigned __int32 threadtest  = GetCurrentThreadId();

          pentium_tsc[0] = processtest;
          pentium_tsc[0] = (pentium_tsc[0] << 32) + threadtest;
        }
        break;

    /* use a rapidly-changing value -- check QueryPerformanceFrequency()    */
    /* to ensure that QueryPerformanceCounter() will work                   */
    case 0:
        ++num;
    default:
        QueryPerformanceCounter((LARGE_INTEGER *)pentium_tsc);
        break;
    }

    for(i = 0; i < 8 && i < len; ++i) {
        buf[i] = ((unsigned char*)pentium_tsc)[i];
    }
    return i;
}

#endif /* IZ_CRYPT_AES_WG */




/* --------------------------------------------------- */
/* Large File Support
 *
 * Moved to Win32i64.c to avoid conflicts in same name functions
 * in WiZ using UnZip and Zip libraries.
 * 9/25/2003
 */


/* --------------------------------------------------- */
/* Unicode Support for Win32
 *
 */

#ifdef UNICODE_SUPPORT


/* write_console
 *
 * Write a string directly to the console.  Supports UTF-8, if
 * console supports it (codepage 65001).
 *
 * This function could change the console to CP_UTF8, convert the string
 * to UTF-8, and then output using WriteConsoleA, but dealing with the console
 * code page gets messy.  Instead, the below write_consolew() is now used for
 * Windows console writes.
 *
 * Needs at least Windows 2000.  The appropriate code to
 * ensure this is only used for Windows 2000 or later needs
 * to be added.
 *
 * Only write to console if output file is STDOUT or STDERR.
 */
unsigned long write_console(FILE *outfile, ZCONST char *instring)
{
  DWORD charswritten;
  DWORD handle = 0;

  /* Only write to console if using standard stream (stdout or stderr) */
  if (outfile == stdout) {
    handle = STD_OUTPUT_HANDLE;
  }
  else if (outfile == stderr) {
    handle = STD_ERROR_HANDLE;
  }

  if (handle && isatty(fileno(outfile))) {
    /* output file is console */
    WriteConsoleA(
              GetStdHandle(handle),            /* in  HANDLE hConsoleOutput */
              instring,                        /* in  const VOID *lpBuffer */
              (DWORD)strlen(instring),         /* in  DWORD nNumberOfCharsToWrite */
              &charswritten,                   /* out LPDWORD lpNumberOfCharsWritten */
              NULL);                           /* reserved */
    return charswritten;
  }

  return fprintf(outfile, "%s", instring);
}


/* write_consolew
 *
 * Write a wide string directly to the console.  As the write is
 * wide, the code page of the console is not important.
 *
 * Needs at least Windows 2000.  The appropriate code to
 * ensure this is only used for Windows 2000 or later needs
 * to be added.
 *
 * Only write to console if output file is STDOUT or STDERR.
 */
unsigned long write_consolew(FILE *outfile, wchar_t *instringw)
{
  long charswritten;
  DWORD handle = 0;

  /* Only write to console if using standard stream (stdout or stderr) */
  if (outfile == stdout) {
    handle = STD_OUTPUT_HANDLE;
  }
  else if (outfile == stderr) {
    handle = STD_ERROR_HANDLE;
  }

  if (handle && isatty(fileno(outfile))) {
    WriteConsoleW(
              GetStdHandle(STD_OUTPUT_HANDLE), /* in  HANDLE hConsoleOutput */
              instringw,                       /* in  const VOID *lpBuffer */
              (DWORD)wcslen(instringw),        /* in  DWORD nNumberOfCharsToWrite */
              &charswritten,                   /* out LPDWORD lpNumberOfCharsWritten */
              NULL);                           /* reserved */
    return charswritten;
  }

  return fprintf(outfile, "%S", instringw);
}


#if 0
long write_consolew2a(wchar_t *inw)
{
  int bufferSize;
  char *m;

  SetConsoleOutputCP(CP_UTF8);     // output is in UTF8
  bufferSize = WideCharToMultiByte(
                           CP_UTF8, 0, inw, -1, NULL, 0, NULL, NULL);
  if ((m = (char *)malloc(bufferSize)) == NULL)
    ZIPERR(ZE_MEM, "write_consolew2");
  WideCharToMultiByte(CP_UTF8, 0, inw, -1, m, bufferSize, NULL, NULL);
  //wprintf(L"%S", m); // <-- upper case %S in wprintf() is used for MultiByte/utf-8
                     //     lower case %s in wprintf() is used for WideChar
  //printf("wcw2A:  ");
  write_consolea(stdout, m);
  //printf("\n");
  free(m);
  return 0;
}
#endif


# ifndef ZIP_DLL_LIB
  /* get_win32_utf8_argv()
   *
   * Get the Win32 wide command line and convert each arg to UTF-8.
   *
   * By returning UTF-8, we can use the MBCS parsing functions on the results.
   * These generally ignore actual args and only look for the ANSI single-byte
   * reserved characters for paths and such, like '/'.  Since these match ANSI
   * in UTF-8 and these byte values are unique in UTF-8, the fact that the MBCS
   * character set may not be UTF-8 (and probably isn't) doesn't matter when
   * parsing args and path parts.  When args are later interpreted, the function
   * is_utf8_string() is used to determine if an arg is UTF-8 and, if it is, it
   * gets converted to the proper Windows wide string using utf8_to_wide_string(),
   * otherwise local_to_wide_string() is used and the local MBCS is converted to
   * wide.  This allows both UTF-8 and local MBCS strings to coexist seamlessly,
   * as long as the local MBCS does not look like UTF-8.
   *
   * Returns a standard argv[] with UTF-8 args.
   */
  char **get_win32_utf8_argv()
  {
    int i;
    wchar_t *commandlinew = NULL;
    wchar_t **argvw = NULL;
    int argcw;
    char **args = NULL;

    commandlinew = GetCommandLineW();

    argvw = CommandLineToArgvW(commandlinew, &argcw);
    if (argvw == NULL) {
      zipwarn("Could not get wide command line", "");
      return NULL;
    }

#if 0
    printf("\nwide args:\n");
    for( i = 0; i < argcw; i++) {
      //printf("%d: %S  ", i, argvw[i]);
      //wprintf(L"''%s''  ", argvw[i]);
      printf("%2d:  '", i);
      write_consolew2a(argvw[i]);
      printf("'  ( ");
      for (j = 0; argvw[i][j]; j++) printf(" %02x", argvw[i][j]);
      printf(" )\n");
    }
    printf("  cmd line: '");
    write_consolew2a(commandlinew);
    printf("'\n");
#endif
    
    /* Create array for UTF-8 args */
    if ((args = (char **)malloc((argcw + 1) * sizeof(char *))) == NULL) {
      ZIPERR(ZE_MEM, "get_win32_utf8_argv (1)");
    }
    for (i = 0; i < argcw; i++) {
      /* Convert wchar_t string to UTF-8 string */
      args[i] = wchar_to_utf8_string_windows(argvw[i]);
    }
    args[i] = NULL;

    /* free original argvw */
    LocalFree(argvw);

    return args;
  }
# endif


  /*------------------------------- */
  /* from win32zip.c */

wchar_t *local_to_wchar_string(ZCONST char *local_string)
{
  wchar_t  *qw;
  int       ulen;
  int       ulenw;

  if (local_string == NULL)
    return NULL;

    /* get length */
    ulenw = MultiByteToWideChar(
                CP_ACP,            /* ANSI code page */
                0,                 /* flags for character-type options */
                local_string,      /* string to convert */
                -1,                /* string length (-1 = NULL terminated) */
                NULL,              /* buffer */
                0 );               /* buffer length (0 = return length) */
    if (ulenw == 0) {
      /* failed */
      return NULL;
    }
    ulenw++;
    /* get length in bytes */
    ulen = sizeof(wchar_t) * (ulenw + 1);
    if ((qw = (wchar_t *)malloc(ulen + 1)) == NULL) {
      return NULL;
    }
    /* convert multibyte to wide */
    ulen = MultiByteToWideChar(
               CP_ACP,            /* ANSI code page */
               0,                 /* flags for character-type options */
               local_string,      /* string to convert */
               -1,                /* string length (-1 = NULL terminated) */
               qw,                /* buffer */
               ulenw);            /* buffer length (0 = return length) */
    if (ulen == 0) {
      /* failed */
      free(qw);
      return NULL;
    }

  return qw;
}


wchar_t *utf8_to_wchar_string_windows(ZCONST char *utf8_string)
{
  wchar_t  *qw;
  int       ulen;
  int       ulenw;

  if (utf8_string == NULL)
    return NULL;

    /* get length */
    ulenw = MultiByteToWideChar(
                CP_UTF8,           /* UTF-8 code page */
                0,                 /* flags for character-type options */
                utf8_string,       /* string to convert */
                -1,                /* string length (-1 = NULL terminated) */
                NULL,              /* buffer */
                0 );               /* buffer length (0 = return length) */
    if (ulenw == 0) {
      /* failed */
      return NULL;
    }
    ulenw++;
    /* get length in bytes */
    ulen = sizeof(wchar_t) * (ulenw + 1);
    if ((qw = (wchar_t *)malloc(ulen + 1)) == NULL) {
      return NULL;
    }
    /* convert multibyte to wide */
    ulen = MultiByteToWideChar(
               CP_UTF8,           /* UTF-8 code page */
               0,                 /* flags for character-type options */
               utf8_string,       /* string to convert */
               -1,                /* string length (-1 = NULL terminated) */
               qw,                /* buffer */
               ulenw);            /* buffer length (0 = return length) */
    if (ulen == 0) {
      /* failed */
      free(qw);
      return NULL;
    }

  return qw;
}



/* Convert wchar_t string to utf8 using Windows calls
   so any characters needing more than one wchar_t are
   are handled by Windows */
char *wchar_to_utf8_string_windows(wstring)
  wchar_t *wstring;
{
  char     *q;           /* return string */
  int       ulen;

  if (wstring == NULL)
    return NULL;

  /* Get buffer length */
  ulen = WideCharToMultiByte(
                  CP_UTF8,        /* UTF-8 code page */
                  0,              /* flags */
                  wstring,        /* string to convert */
                  -1,             /* input chars (-1 = NULL terminated) */
                  NULL,           /* buffer */
                  0,              /* size of buffer (0 = return needed size) */
                  NULL,           /* default char */
                  NULL);          /* used default char */
  if (ulen == 0) {
    /* failed */
    return NULL;
  }
  ulen += 2;
  if ((q = malloc(ulen + 1)) == NULL) {
    return NULL;
  }

  /* Convert the Unicode string to UTF-8 */
  if ((ulen = WideCharToMultiByte(
                  CP_UTF8,        /* UTF-8 code page */
                  0,              /* flags */
                  wstring,        /* string to convert */
                  -1,             /* input chars (-1 = NULL terminated) */
                  q,              /* buffer */
                  ulen,           /* size of buffer (0 = return needed size) */
                  NULL,           /* default char */
                  NULL)) == 0)    /* used default char */
  {
    free(q);
    return NULL;
  }

  return q;
}

  /*------------------------------- */


/* convert wide character string to multi-byte character string */
/* win32 version */
char *wide_to_local_string(wide_string)
  zwchar *wide_string;
{
  int i;
  wchar_t wc;
  int wsize = 0;
  int max_bytes = 9;
#if 0
  int bytes_char;
  int default_used;
#endif
  char buf[9];
  char *buffer = NULL;
  char *local_string = NULL;
  int ansi7_char = 0;

  if (wide_string == NULL)
    return NULL;

  for (wsize = 0; wide_string[wsize]; wsize++) ;

  if (max_bytes < MB_CUR_MAX)
    max_bytes = MB_CUR_MAX;

  if ((buffer = (char *)malloc(wsize * max_bytes + 1)) == NULL) {
    ZIPERR(ZE_MEM, "wide_to_local_string");
  }

  /* convert it */
  buffer[0] = '\0';
  for (i = 0; i < wsize; i++) {
    /* Under some vendor's C-RTL, the Wide-to-MultiByte conversion functions
     * (like wctomb() et. al.) do not use the same codepage as the other
     * string arguments I/O functions (fopen, mkdir, rmdir etc.).
     * Therefore, we have to fall back to the underlying Win32-API call to
     * achieve a consistent behaviour for all supported compiler environments.
     * Failing RTLs are for example:
     *   Borland (locale uses OEM-CP as default, but I/O functions expect ANSI
     *            names)
     *   Watcom  (only "C" locale, wctomb() always uses OEM CP)
     * (in other words: all supported environments except the Microsoft RTLs)
     */
    /* Given inconsistencies between conversion routines for non-ANSI 8-bit
     * characters, and the complications of OEM conversion, now just convert
     * all non-ANSI characters to escapes.
     */
#if 0
    if (sizeof(wchar_t) < 4 && wide_string[i] > 0xFFFF) {
      /* wchar_t probably 2 bytes */
      /* could do surrogates if state_dependent and wctomb can do */
      wc = zwchar_to_wchar_t_default_char;
    } else {
      wc = (wchar_t)wide_string[i];
    }
#endif
    wc = (wchar_t)wide_string[i];
    ansi7_char = (wc >= 0 && wc <= 0x7f);

#if 0
    bytes_char = WideCharToMultiByte(
                          CP_ACP,
                          WC_COMPOSITECHECK,
                          &wc,
                          1,
                          (LPSTR)buf,
                          sizeof(buf),
                          NULL,
                          &default_used);
#endif
    if (ansi7_char) {
      /* ASCII */
      buf[0] = (char)wc;
      buf[1] = '\0';
      strcat(buffer, buf);
    } else {
      /* use escape for wide character */
      char *e = wide_char_to_escape_string(wide_string[i]);
      strcat(buffer, e);
      free(e);
    }
#if 0
    if (default_used)
      bytes_char = -1;
    if (unicode_escape_all) {
      if (bytes_char == 1 && (uch)buf[0] <= 0x7f) {
        /* ASCII */
        strncat(buffer, buf, 1);
      } else {
        /* use escape for wide character */
        char *e = wide_char_to_escape_string(wide_string[i]);
        strcat(buffer, e);
        free(e);
      }
    } else if (bytes_char > 0) {
      /* multi-byte char */
      strncat(buffer, buf, bytes_char);
    } else {
      /* no MB for this wide */
      if (use_wide_to_mb_default) {
        /* default character */
        strcat(buffer, wide_to_mb_default_string);
      } else {
        /* use escape for wide character */
        char *e = wide_char_to_escape_string(wide_string[i]);
        strcat(buffer, e);
        free(e);
      }
    }
#endif
  }
  if ((local_string = (char *)realloc(buffer, strlen(buffer) + 1)) == NULL) {
    free(buffer);
    ZIPERR(ZE_MEM, "wide_to_local_string");
  }

  return local_string;
}

/* convert multi-byte character string to wide character string */
/* win32 version */
zwchar *local_to_wide_string(local_string)
  char *local_string;
{
  int wsize;
  wchar_t *wc_string;
  zwchar *wide_string;

  /* for now try to convert as string - fails if a bad char in string */
  wsize = MultiByteToWideChar(CP_ACP,
                              0,
                              local_string,
                              -1,
                              NULL,
                              0);
  if (wsize == (size_t)-1) {
    /* could not convert */
    return NULL;
  }

  /* convert it */
  if ((wc_string = (wchar_t *)malloc((wsize + 1) * sizeof(wchar_t))) == NULL) {
    ZIPERR(ZE_MEM, "local_to_wide_string");
  }
  wsize = MultiByteToWideChar(CP_ACP,
                              0,
                              local_string,
                              -1,
                              wc_string,
                              wsize + 1);
  wc_string[wsize] = (wchar_t) 0;

  /* in case wchar_t is not zwchar */
  if ((wide_string = (zwchar *)malloc((wsize + 1) * sizeof(zwchar))) == NULL) {
    free(wc_string);
    ZIPERR(ZE_MEM, "local_to_wide_string");
  }
  for (wsize = 0; (wide_string[wsize] = (zwchar)wc_string[wsize]); wsize++) ;
  wide_string[wsize] = (zwchar)0;
  free(wc_string);

  return wide_string;
}


/* Replacement for zwopen that handles long paths.  Normally _wsopen is
   used, but it doesn't understand long paths.  (Need to test this, but
   doc does not say it does, and long path support is generally only
   available if the Microsoft doc says it is.)  This function needs
   wide character support. */

/* Used only for reading, so flags are hardwired. */

int zwopen_read_long(filename)
  wchar_t *filename;
{
#ifdef WINDOWS_LONG_PATHS
  if (wcslen(filename) > MAX_PATH && include_windows_long_paths) {
    /* can't use int _wsopen, so use CreateFileW */
    wchar_t *pathw_long;
    size_t pathw_long_len;
    size_t i;
    HANDLE h;
    int fileID;
    wchar_t *full_path = NULL;

    if ((full_path = _wfullpath(NULL, filename, 0)) != NULL) {
# if 0
        sprintf(errbuf, "      zwopen_read_long full_path: (%d chars) '%S'", wcslen(full_path), full_path);
        zipmessage(errbuf, "");
# endif
    }
    if (full_path) {
        if ((pathw_long = malloc((wcslen(full_path) + 5) * sizeof(wchar_t))) == NULL) {
            ZIPERR(ZE_MEM, "Getting full path for zwopen_long");
        }
    } else {
        if ((pathw_long = malloc((wcslen(filename) + 5) * sizeof(wchar_t))) == NULL) {
            ZIPERR(ZE_MEM, "Getting long path for zwopen_long");
        }
    }
    wcscpy(pathw_long, L"\\\\?\\");
    if (full_path) {
        wcscat(pathw_long, full_path);
        free(full_path);
    } else {
        wcscat(pathw_long, filename);
    }
    pathw_long_len = wcslen(pathw_long);
    for (i = 0; i < pathw_long_len; i++) {
        if (pathw_long[i] == L'/') {
            pathw_long[i] = L'\\';
        }
    }
#if 0
    sprintf(errbuf, "      zwopen_read_long Trying: (%d chars) '%S'",
            pathw_long_len, pathw_long);
    zipmessage(errbuf, "");
#endif
    /* May need backup privilege or similar for this to allow reading file
     * contents.  Otherwise this fails for everything.  Given that, switched to
     * FILE_ATTRIBUTE_NORMAL which works on files the user can read without
     * privilege.  On ToDo list is to first try requesting privilege (maybe
     * if -! used) and use FILE_FLAG_BACKUP_SEMANTICS, then revert to
     * FILE_ATTRIBUTE_NORMAL if that fails or user does not permit raise in
     * privilege.
     * 2014-04-16 EG
     */
    /*
    h = CreateFileW(pathw_long, READ_CONTROL, FILE_SHARE_READ,
                    NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    */
    h = CreateFileW(pathw_long, GENERIC_READ, FILE_SHARE_READ,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    free(pathw_long);
    if (h != INVALID_HANDLE_VALUE) {
      /* get CRT file ID and return it */
      /* intptr_t not defined in VS 6, so use long */
      if ((fileID = _open_osfhandle((Z_INTPTR_T)h, O_RDONLY|O_BINARY)) == -1) {
          zprintf(  "      _open_osfhandle Failed");
      }
      return fileID;
    }
    /* if anything failed, return -1 (as _wsopen would) */
    return -1;
  }
  else
#endif
  {
    /* not a long path, so open normally */
    /* Some testing is needed, but the above may be more specific to our needs and it
     * may be worth switching to the above for all Win32 file openings.  This may be
     * especially true if we want to allow use of privileges.
     */
    return _wsopen(filename, O_RDONLY|O_BINARY, _SH_DENYNO);
  }
}

#endif /* UNICODE_SUPPORT */


/*
# if defined(UNICODE_SUPPORT) || defined(WIN32_OEM)
*/
/* convert oem to ansi character string */
char *oem_to_local_string(local_string, oem_string)
  char *local_string;
  char *oem_string;
{
  /* convert OEM to ANSI character set */
  OemToChar(oem_string, local_string);

  return local_string;
}
/*
# endif
*/


/*
#if defined(UNICODE_SUPPORT) || defined(WIN32_OEM)
*/
/* convert local to oem character string */
char *local_to_oem_string(oem_string, local_string)
  char *oem_string;
  char *local_string;
{
  /* convert to OEM display character set */
  CharToOem(local_string, oem_string);
  return oem_string;
}
/*
#endif
*/

