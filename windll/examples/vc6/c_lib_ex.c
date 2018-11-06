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

#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <string.h>
#include <direct.h>
#include "c_lib_ex.h"
#include "../../../revision.h"


#if 0
# ifdef WIN32
#  include <commctrl.h>
#  include <winver.h>
# else
#  include <ver.h>
# endif
#endif


long MyZipEComment(char *oldcomment,
                   char *filename,
                   char *ufilename,
                   long maxcommentsize,
                   long *newcommentsize,
                   char *newcomment);

long MyZipAComment(char *oldcomment,
                   long maxcommentsize,
                   long *newcommentsize,
                   char *newcomment);

long MyZipPassword(long bufsize,
                   char *prompt,
                   char *password);

long MyZipPrint(char *buf,
                unsigned long size);

int GetVersionInfo();
char *argv_to_commandline(int argc, char *argv[]);



/****************************************************************************

    FUNCTION: Main(int argc, char **argv)

****************************************************************************/
int main(int argc, char **argv)
{
  int ret;
   
  ZIPUSERFUNCTIONS ZipUserFunctions, *lpZipUserFunctions;
  char *commandline = NULL;
  char *currentdir = ".";
  char *progress_size = "100m";

  if (argc < 2)
  {
    printf("usage: c_lib_ex [zip command arguments]\n");
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
  
  lpZipUserFunctions = &ZipUserFunctions;

  /* Any unused callbacks must be set to NULL.  Leave this
     block as is and add functions as needed below. */
  ZipUserFunctions.print    = NULL;     /* gets most display output */
  ZipUserFunctions.ecomment = NULL;     /* called to set entry comment */
  ZipUserFunctions.acomment = NULL;     /* called to set archive comment */
  ZipUserFunctions.password = NULL;     /* called to get password */
  ZipUserFunctions.split    = NULL;     /* NOT IMPLEMENTED - DON'T USE */
  ZipUserFunctions.service  = NULL;     /* called each entry */
  ZipUserFunctions.service_no_int64 = NULL; /* 32-bit version of service */
  ZipUserFunctions.progress = NULL;     /* provides entry progress */
  ZipUserFunctions.error    = NULL;     /* called when warning/error */
  ZipUserFunctions.finish   = NULL;     /* called when zip is finished */

  /* Set callbacks for the functions we're using. */

  /* The comment callbacks are not working for the vc6 lib. */
  ZipUserFunctions.ecomment     = MyZipEComment;
  ZipUserFunctions.acomment     = MyZipAComment;
	ZipUserFunctions.password     = MyZipPassword;
  ZipUserFunctions.print        = MyZipPrint;

  printf("--- calling ZpZip ------------------------------------\n");

  ret = ZpZip(commandline, currentdir, lpZipUserFunctions, progress_size);
  
  printf("------------------------------------------------------\n");

  printf("Zip returned:  %d\n", ret);

  return ret;

}


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
 * Maximum comment length: 32765.  (The actual maximum length is dependent on
 *                                  various factors, but this should be safe.)
 *
 * Return 0 on fail (don't change comment), 1 on success.
 *
 * Replace the contents of this function as needed by your application.
 */

/* Get and set entry comment. */
long MyZipEComment(char *oldcomment,
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
 * The caller of the callback supplies the buffer (newcomment) to write the
 * comment to.
 *
 * Maximum comment length: 32765.  (The actual maximum length is dependent on
 *                                  various factors, but this should be safe.)
 *
 * Replace the contents of this function as needed by your application.
 */
long MyZipAComment(char *oldcomment,
                   long maxcommentsize,
                   long *newcommentsize,
                   char *newcomment)
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
 * The caller provides the buffer (password) to write the password to.
 *
 * bufsize - max size of password, including terminating NULL
 * prompt  - either the "password" or "verify password" prompt text
 *
 * returns - 0 on fail, 1 on success
 *
 * Modify as needed for your application.
 */

long MyZipPassword(long bufsize,
                   char *prompt,
                   char *password)
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
 * This callback currently not used by LIB.
 */

long MyZipPrint(char *buf,
                unsigned long size)
{
#if 0
    fprintf(stderr, "\nzip output (%d bytes):  %s", size, buf);
#endif
    fprintf(stderr, "%s", buf);
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

    if ((zip_ver.BetaLevel = malloc(10)) == NULL)
    {
      fprintf(stderr, "Could not allocate BetaLevel\n");
      return 2;
    }
    if ((zip_ver.Version = malloc(20)) == NULL)
    {
      fprintf(stderr, "Could not allocate Version\n");
      return 3;
    }
    if ((zip_ver.RevDate = malloc(20)) == NULL)
    {
      fprintf(stderr, "Could not allocate RevDate\n");
      return 4;
    }
    if ((zip_ver.RevYMD = malloc(20)) == NULL)
    {
      fprintf(stderr, "Could not allocate RevYMD\n");
      return 5;
    }
    if ((zip_ver.zlib_Version = malloc(10)) == NULL)
    {
      fprintf(stderr, "Could not allocate zlib_Version\n");
      return 6;
    }
    if ((zip_ver.szFeatures = malloc(4000)) == NULL)
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

    fprintf(stderr, "--- get version of zip -----------\n");
    sprintf(zipversnum, "%d.%d.%d",
            zip_ver.zip.major, zip_ver.zip.minor, zip_ver.zip.patchlevel);
    sprintf(libdllinterfaceversnum, "%d.%d.%d",
            zip_ver.libdll_interface.major, zip_ver.libdll_interface.minor,
            zip_ver.libdll_interface.patchlevel);
    fprintf(stderr, "  Zip version:       %s\n", zip_ver.Version);
    fprintf(stderr, "  Zip version num:   %s %s\n", zipversnum,
                                                   zip_ver.BetaLevel);
    fprintf(stderr, "  Zip rev date:      %s\n", zip_ver.RevDate);
    fprintf(stderr, "  Zip rev YMD:       %s\n", zip_ver.RevYMD);
    fprintf(stderr, "  LIB/DLL interface: %s\n", libdllinterfaceversnum);
    fprintf(stderr, "  Feature list:      %s\n", features);
    fprintf(stderr, "----------------------------------\n");

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
